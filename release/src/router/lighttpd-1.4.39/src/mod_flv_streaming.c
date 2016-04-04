#include "base.h"
#include "log.h"
#include "buffer.h"
#include "response.h"
#include "http_chunk.h"
#include "stat_cache.h"

#include "plugin.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* plugin config for all request/connections */

typedef struct {
	array *extensions;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	buffer *query_str;
	array *get_params;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

/* init the plugin data */
INIT_FUNC(mod_flv_streaming_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->query_str = buffer_init();
	p->get_params = array_init();

	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_flv_streaming_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;

		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (NULL == s) continue;

			array_free(s->extensions);

			free(s);
		}
		free(p->config_storage);
	}

	buffer_free(p->query_str);
	array_free(p->get_params);

	free(p);

	return HANDLER_GO_ON;
}

/* handle plugin config and check values */

SETDEFAULTS_FUNC(mod_flv_streaming_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "flv-streaming.extensions",   NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ NULL,                         NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(plugin_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->extensions     = array_init();

		cv[0].destination = s->extensions;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_flv_streaming_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(extensions);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("flv-streaming.extensions"))) {
				PATCH(extensions);
			}
		}
	}

	return 0;
}
#undef PATCH

static int split_get_params(array *get_params, buffer *qrystr) {
	size_t is_key = 1;
	size_t i, len;
	char *key = NULL, *val = NULL;

	key = qrystr->ptr;

	/* we need the \0 */
	len = buffer_string_length(qrystr);
	for (i = 0; i <= len; i++) {
		switch(qrystr->ptr[i]) {
		case '=':
			if (is_key) {
				val = qrystr->ptr + i + 1;

				qrystr->ptr[i] = '\0';

				is_key = 0;
			}

			break;
		case '&':
		case '\0': /* fin symbol */
			if (!is_key) {
				data_string *ds;
				/* we need at least a = since the last & */

				/* terminate the value */
				qrystr->ptr[i] = '\0';

				if (NULL == (ds = (data_string *)array_get_unused_element(get_params, TYPE_STRING))) {
					ds = data_string_init();
				}
				buffer_copy_string_len(ds->key, key, strlen(key));
				buffer_copy_string_len(ds->value, val, strlen(val));

				array_insert_unique(get_params, (data_unset *)ds);
			}

			key = qrystr->ptr + i + 1;
			val = NULL;
			is_key = 1;
			break;
		}
	}

	return 0;
}

URIHANDLER_FUNC(mod_flv_streaming_path_handler) {
	plugin_data *p = p_d;
	int s_len;
	size_t k;

	UNUSED(srv);

	if (con->mode != DIRECT) return HANDLER_GO_ON;

	if (buffer_string_is_empty(con->physical.path)) return HANDLER_GO_ON;

	mod_flv_streaming_patch_connection(srv, con, p);

	s_len = buffer_string_length(con->physical.path);

	for (k = 0; k < p->conf.extensions->used; k++) {
		data_string *ds = (data_string *)p->conf.extensions->data[k];
		int ct_len = buffer_string_length(ds->value);

		if (ct_len > s_len) continue;
		if (buffer_is_empty(ds->value)) continue;

		if (0 == strncmp(con->physical.path->ptr + s_len - ct_len, ds->value->ptr, ct_len)) {
			data_string *get_param;
			stat_cache_entry *sce = NULL;
			int start;
			char *err = NULL;
			/* if there is a start=[0-9]+ in the header use it as start,
			 * otherwise send the full file */

			array_reset(p->get_params);
			buffer_copy_buffer(p->query_str, con->uri.query);
			split_get_params(p->get_params, p->query_str);

			if (NULL == (get_param = (data_string *)array_get_element(p->get_params, "start"))) {
				return HANDLER_GO_ON;
			}

			/* too short */
			if (buffer_string_is_empty(get_param->value)) return HANDLER_GO_ON;

			/* check if it is a number */
			start = strtol(get_param->value->ptr, &err, 10);
			if (*err != '\0') {
				return HANDLER_GO_ON;
			}

			if (start <= 0) return HANDLER_GO_ON;

			/* check if start is > filesize */
			if (HANDLER_GO_ON != stat_cache_get_entry(srv, con, con->physical.path, &sce)) {
				return HANDLER_GO_ON;
			}

			if (start > sce->st.st_size) {
				return HANDLER_GO_ON;
			}

			/* we are safe now, let's build a flv header */
			http_chunk_append_mem(srv, con, CONST_STR_LEN("FLV\x1\x1\0\0\0\x9\0\0\0\x9"));
			http_chunk_append_file(srv, con, con->physical.path, start, sce->st.st_size - start);
			http_chunk_close(srv, con);

			response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("video/x-flv"));

			con->file_finished = 1;

			return HANDLER_FINISHED;
		}
	}

	/* not found */
	return HANDLER_GO_ON;
}

/* this function is called at dlopen() time and inits the callbacks */

int mod_flv_streaming_plugin_init(plugin *p);
int mod_flv_streaming_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("flv_streaming");

	p->init        = mod_flv_streaming_init;
	p->handle_physical = mod_flv_streaming_path_handler;
	p->set_defaults  = mod_flv_streaming_set_defaults;
	p->cleanup     = mod_flv_streaming_free;

	p->data        = NULL;

	return 0;
}
