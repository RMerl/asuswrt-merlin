#include "base.h"
#include "log.h"
#include "buffer.h"
#include "stat_cache.h"

#include "plugin.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct {
	buffer *server_root;
	buffer *default_host;
	buffer *document_root;

	buffer *docroot_cache_key;
	buffer *docroot_cache_value;
	buffer *docroot_cache_servername;

	unsigned short debug;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	buffer *doc_root;

	plugin_config **config_storage;
	plugin_config conf;
} plugin_data;

INIT_FUNC(mod_simple_vhost_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->doc_root = buffer_init();

	return p;
}

FREE_FUNC(mod_simple_vhost_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			buffer_free(s->document_root);
			buffer_free(s->default_host);
			buffer_free(s->server_root);

			buffer_free(s->docroot_cache_key);
			buffer_free(s->docroot_cache_value);
			buffer_free(s->docroot_cache_servername);

			free(s);
		}

		free(p->config_storage);
	}

	buffer_free(p->doc_root);

	free(p);

	return HANDLER_GO_ON;
}

SETDEFAULTS_FUNC(mod_simple_vhost_set_defaults) {
	plugin_data *p = p_d;
	size_t i;

	config_values_t cv[] = {
		{ "simple-vhost.server-root",       NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },
		{ "simple-vhost.default-host",      NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },
		{ "simple-vhost.document-root",     NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },
		{ "simple-vhost.debug",             NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },
		{ NULL,                             NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(plugin_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));

		s->server_root = buffer_init();
		s->default_host = buffer_init();
		s->document_root = buffer_init();

		s->docroot_cache_key = buffer_init();
		s->docroot_cache_value = buffer_init();
		s->docroot_cache_servername = buffer_init();

		s->debug = 0;

		cv[0].destination = s->server_root;
		cv[1].destination = s->default_host;
		cv[2].destination = s->document_root;
		cv[3].destination = &(s->debug);


		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

static int build_doc_root(server *srv, connection *con, plugin_data *p, buffer *out, buffer *host) {
	stat_cache_entry *sce = NULL;
	force_assert(!buffer_string_is_empty(p->conf.server_root));

	buffer_string_prepare_copy(out, 127);
	buffer_copy_buffer(out, p->conf.server_root);

	if (!buffer_string_is_empty(host)) {
		/* a hostname has to start with a alpha-numerical character
		 * and must not contain a slash "/"
		 */
		char *dp;

		buffer_append_slash(out);

		if (NULL == (dp = strchr(host->ptr, ':'))) {
			buffer_append_string_buffer(out, host);
		} else {
			buffer_append_string_len(out, host->ptr, dp - host->ptr);
		}
	}
	buffer_append_slash(out);

	if (buffer_string_length(p->conf.document_root) > 1 && p->conf.document_root->ptr[0] == '/') {
		buffer_append_string_len(out, p->conf.document_root->ptr + 1, buffer_string_length(p->conf.document_root) - 1);
	} else {
		buffer_append_string_buffer(out, p->conf.document_root);
		buffer_append_slash(out);
	}

	if (HANDLER_ERROR == stat_cache_get_entry(srv, con, out, &sce)) {
		if (p->conf.debug) {
			log_error_write(srv, __FILE__, __LINE__, "sb",
					strerror(errno), out);
		}
		return -1;
	} else if (!S_ISDIR(sce->st.st_mode)) {
		return -1;
	}

	return 0;
}


#define PATCH(x) \
	p->conf.x = s->x;
static int mod_simple_vhost_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(server_root);
	PATCH(default_host);
	PATCH(document_root);

	PATCH(docroot_cache_key);
	PATCH(docroot_cache_value);
	PATCH(docroot_cache_servername);

	PATCH(debug);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("simple-vhost.server-root"))) {
				PATCH(server_root);
				PATCH(docroot_cache_key);
				PATCH(docroot_cache_value);
				PATCH(docroot_cache_servername);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("simple-vhost.default-host"))) {
				PATCH(default_host);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("simple-vhost.document-root"))) {
				PATCH(document_root);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("simple-vhost.debug"))) {
				PATCH(debug);
			}
		}
	}

	return 0;
}
#undef PATCH

static handler_t mod_simple_vhost_docroot(server *srv, connection *con, void *p_data) {
	plugin_data *p = p_data;

	/*
	 * cache the last successfull translation from hostname (authority) to docroot
	 * - this saves us a stat() call
	 *
	 */

	mod_simple_vhost_patch_connection(srv, con, p);

	/* build_doc_root() requires a server_root; skip module if simple-vhost.server-root is not set
	 * or set to an empty string (especially don't cache any results!)
	 */
	if (buffer_string_is_empty(p->conf.server_root)) return HANDLER_GO_ON;

	if (!buffer_string_is_empty(p->conf.docroot_cache_key) &&
	    !buffer_string_is_empty(con->uri.authority) &&
	    buffer_is_equal(p->conf.docroot_cache_key, con->uri.authority)) {
		/* cache hit */
		buffer_copy_buffer(con->server_name,       p->conf.docroot_cache_servername);
		buffer_copy_buffer(con->physical.doc_root, p->conf.docroot_cache_value);
	} else {
		/* build document-root */
		if (buffer_string_is_empty(con->uri.authority) ||
		    build_doc_root(srv, con, p, p->doc_root, con->uri.authority)) {
			/* not found, fallback the default-host */
			if (0 == build_doc_root(srv, con, p,
					   p->doc_root,
					   p->conf.default_host)) {
				/* default host worked */
				buffer_copy_buffer(con->server_name, p->conf.default_host);
				buffer_copy_buffer(con->physical.doc_root, p->doc_root);
				/* do not cache default host */
			}
			return HANDLER_GO_ON;
		}

		/* found host */
		buffer_copy_buffer(con->server_name, con->uri.authority);
		buffer_copy_buffer(con->physical.doc_root, p->doc_root);

		/* copy to cache */
		buffer_copy_buffer(p->conf.docroot_cache_key,        con->uri.authority);
		buffer_copy_buffer(p->conf.docroot_cache_value,      p->doc_root);
		buffer_copy_buffer(p->conf.docroot_cache_servername, con->server_name);
	}

	return HANDLER_GO_ON;
}


int mod_simple_vhost_plugin_init(plugin *p);
int mod_simple_vhost_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("simple_vhost");

	p->init        = mod_simple_vhost_init;
	p->set_defaults = mod_simple_vhost_set_defaults;
	p->handle_docroot  = mod_simple_vhost_docroot;
	p->cleanup     = mod_simple_vhost_free;

	p->data        = NULL;

	return 0;
}
