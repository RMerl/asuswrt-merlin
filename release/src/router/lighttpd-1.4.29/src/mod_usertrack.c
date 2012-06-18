#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_OPENSSL
# include <openssl/md5.h>
#else
# include "md5.h"

typedef li_MD5_CTX MD5_CTX;
#define MD5_Init li_MD5_Init
#define MD5_Update li_MD5_Update
#define MD5_Final li_MD5_Final

#endif

/* plugin config for all request/connections */

typedef struct {
	buffer *cookie_name;
	buffer *cookie_domain;
	unsigned int cookie_max_age;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

/* init the plugin data */
INIT_FUNC(mod_usertrack_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_usertrack_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			buffer_free(s->cookie_name);
			buffer_free(s->cookie_domain);

			free(s);
		}
		free(p->config_storage);
	}

	free(p);

	return HANDLER_GO_ON;
}

/* handle plugin config and check values */

SETDEFAULTS_FUNC(mod_usertrack_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "usertrack.cookie-name",       NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ "usertrack.cookie-max-age",    NULL, T_CONFIG_INT, T_CONFIG_SCOPE_CONNECTION },          /* 1 */
		{ "usertrack.cookie-domain",     NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 2 */

		{ "usertrack.cookiename",        NULL, T_CONFIG_DEPRECATED, T_CONFIG_SCOPE_CONNECTION },
		{ NULL,                          NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->cookie_name    = buffer_init();
		s->cookie_domain  = buffer_init();
		s->cookie_max_age = 0;

		cv[0].destination = s->cookie_name;
		cv[1].destination = &(s->cookie_max_age);
		cv[2].destination = s->cookie_domain;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv)) {
			return HANDLER_ERROR;
		}

		if (buffer_is_empty(s->cookie_name)) {
			buffer_copy_string_len(s->cookie_name, CONST_STR_LEN("TRACKID"));
		} else {
			size_t j;
			for (j = 0; j < s->cookie_name->used - 1; j++) {
				char c = s->cookie_name->ptr[j] | 32;
				if (c < 'a' || c > 'z') {
					log_error_write(srv, __FILE__, __LINE__, "sb",
							"invalid character in usertrack.cookie-name:",
							s->cookie_name);

					return HANDLER_ERROR;
				}
			}
		}

		if (!buffer_is_empty(s->cookie_domain)) {
			size_t j;
			for (j = 0; j < s->cookie_domain->used - 1; j++) {
				char c = s->cookie_domain->ptr[j];
				if (c <= 32 || c >= 127 || c == '"' || c == '\\') {
					log_error_write(srv, __FILE__, __LINE__, "sb",
							"invalid character in usertrack.cookie-domain:",
							s->cookie_domain);

					return HANDLER_ERROR;
				}
			}
		}
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_usertrack_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(cookie_name);
	PATCH(cookie_domain);
	PATCH(cookie_max_age);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("usertrack.cookie-name"))) {
				PATCH(cookie_name);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("usertrack.cookie-max-age"))) {
				PATCH(cookie_max_age);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("usertrack.cookie-domain"))) {
				PATCH(cookie_domain);
			}
		}
	}

	return 0;
}
#undef PATCH

URIHANDLER_FUNC(mod_usertrack_uri_handler) {
	plugin_data *p = p_d;
	data_string *ds;
	unsigned char h[16];
	MD5_CTX Md5Ctx;
	char hh[32];

	if (con->uri.path->used == 0) return HANDLER_GO_ON;

	mod_usertrack_patch_connection(srv, con, p);

	if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Cookie"))) {
		char *g;
		/* we have a cookie, does it contain a valid name ? */

		/* parse the cookie
		 *
		 * check for cookiename + (WS | '=')
		 *
		 */

		if (NULL != (g = strstr(ds->value->ptr, p->conf.cookie_name->ptr))) {
			char *nc;

			/* skip WS */
			for (nc = g + p->conf.cookie_name->used-1; *nc == ' ' || *nc == '\t'; nc++);

			if (*nc == '=') {
				/* ok, found the key of our own cookie */

				if (strlen(nc) > 32) {
					/* i'm lazy */
					return HANDLER_GO_ON;
				}
			}
		}
	}

	/* set a cookie */
	if (NULL == (ds = (data_string *)array_get_unused_element(con->response.headers, TYPE_STRING))) {
		ds = data_response_init();
	}
	buffer_copy_string_len(ds->key, CONST_STR_LEN("Set-Cookie"));
	buffer_copy_string_buffer(ds->value, p->conf.cookie_name);
	buffer_append_string_len(ds->value, CONST_STR_LEN("="));


	/* taken from mod_auth.c */

	/* generate shared-secret */
	MD5_Init(&Md5Ctx);
	MD5_Update(&Md5Ctx, (unsigned char *)con->uri.path->ptr, con->uri.path->used - 1);
	MD5_Update(&Md5Ctx, (unsigned char *)"+", 1);

	/* we assume sizeof(time_t) == 4 here, but if not it ain't a problem at all */
	LI_ltostr(hh, srv->cur_ts);
	MD5_Update(&Md5Ctx, (unsigned char *)hh, strlen(hh));
	MD5_Update(&Md5Ctx, (unsigned char *)srv->entropy, sizeof(srv->entropy));
	LI_ltostr(hh, rand());
	MD5_Update(&Md5Ctx, (unsigned char *)hh, strlen(hh));

	MD5_Final(h, &Md5Ctx);

	buffer_append_string_encoded(ds->value, (char *)h, 16, ENCODING_HEX);
	buffer_append_string_len(ds->value, CONST_STR_LEN("; Path=/"));
	buffer_append_string_len(ds->value, CONST_STR_LEN("; Version=1"));

	if (!buffer_is_empty(p->conf.cookie_domain)) {
		buffer_append_string_len(ds->value, CONST_STR_LEN("; Domain="));
		buffer_append_string_encoded(ds->value, CONST_BUF_LEN(p->conf.cookie_domain), ENCODING_REL_URI);
	}

	if (p->conf.cookie_max_age) {
		buffer_append_string_len(ds->value, CONST_STR_LEN("; max-age="));
		buffer_append_long(ds->value, p->conf.cookie_max_age);
	}

	array_insert_unique(con->response.headers, (data_unset *)ds);

	return HANDLER_GO_ON;
}

/* this function is called at dlopen() time and inits the callbacks */

int mod_usertrack_plugin_init(plugin *p);
int mod_usertrack_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("usertrack");

	p->init        = mod_usertrack_init;
	p->handle_uri_clean  = mod_usertrack_uri_handler;
	p->set_defaults  = mod_usertrack_set_defaults;
	p->cleanup     = mod_usertrack_free;

	p->data        = NULL;

	return 0;
}
