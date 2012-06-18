#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include "response.h"

#include <stdlib.h>
#include <string.h>

/* plugin config for all request/connections */

typedef struct {
	int handled; /* make sure that we only apply the headers once */
} handler_ctx;

typedef struct {
	array *request_header;
	array *response_header;

	array *environment;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

static handler_ctx * handler_ctx_init() {
	handler_ctx * hctx;

	hctx = calloc(1, sizeof(*hctx));

	hctx->handled = 0;

	return hctx;
}

static void handler_ctx_free(handler_ctx *hctx) {
	free(hctx);
}


/* init the plugin data */
INIT_FUNC(mod_setenv_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_setenv_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			array_free(s->request_header);
			array_free(s->response_header);
			array_free(s->environment);

			free(s);
		}
		free(p->config_storage);
	}

	free(p);

	return HANDLER_GO_ON;
}

/* handle plugin config and check values */

SETDEFAULTS_FUNC(mod_setenv_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "setenv.add-request-header",  NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ "setenv.add-response-header", NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 1 */
		{ "setenv.add-environment",     NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 2 */
		{ NULL,                         NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->request_header   = array_init();
		s->response_header  = array_init();
		s->environment      = array_init();

		cv[0].destination = s->request_header;
		cv[1].destination = s->response_header;
		cv[2].destination = s->environment;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_setenv_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(request_header);
	PATCH(response_header);
	PATCH(environment);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("setenv.add-request-header"))) {
				PATCH(request_header);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("setenv.add-response-header"))) {
				PATCH(response_header);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("setenv.add-environment"))) {
				PATCH(environment);
			}
		}
	}

	return 0;
}
#undef PATCH

URIHANDLER_FUNC(mod_setenv_uri_handler) {
	plugin_data *p = p_d;
	size_t k;
	handler_ctx *hctx;

	if (con->plugin_ctx[p->id]) {
		hctx = con->plugin_ctx[p->id];
	} else {
		hctx = handler_ctx_init();

		con->plugin_ctx[p->id] = hctx;
	}

	if (hctx->handled) {
		return HANDLER_GO_ON;
	}

	hctx->handled = 1;

	mod_setenv_patch_connection(srv, con, p);

	for (k = 0; k < p->conf.request_header->used; k++) {
		data_string *ds = (data_string *)p->conf.request_header->data[k];
		data_string *ds_dst;

		if (NULL == (ds_dst = (data_string *)array_get_unused_element(con->request.headers, TYPE_STRING))) {
			ds_dst = data_string_init();
		}

		buffer_copy_string_buffer(ds_dst->key, ds->key);
		buffer_copy_string_buffer(ds_dst->value, ds->value);

		array_insert_unique(con->request.headers, (data_unset *)ds_dst);
	}

	for (k = 0; k < p->conf.environment->used; k++) {
		data_string *ds = (data_string *)p->conf.environment->data[k];
		data_string *ds_dst;

		if (NULL == (ds_dst = (data_string *)array_get_unused_element(con->environment, TYPE_STRING))) {
			ds_dst = data_string_init();
		}

		buffer_copy_string_buffer(ds_dst->key, ds->key);
		buffer_copy_string_buffer(ds_dst->value, ds->value);

		array_insert_unique(con->environment, (data_unset *)ds_dst);
	}

	for (k = 0; k < p->conf.response_header->used; k++) {
		data_string *ds = (data_string *)p->conf.response_header->data[k];

		response_header_insert(srv, con, CONST_BUF_LEN(ds->key), CONST_BUF_LEN(ds->value));
	}

	/* not found */
	return HANDLER_GO_ON;
}

CONNECTION_FUNC(mod_setenv_reset) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (con->plugin_ctx[p->id]) {
		handler_ctx_free(con->plugin_ctx[p->id]);
		con->plugin_ctx[p->id] = NULL;
	}

	return HANDLER_GO_ON;
}

/* this function is called at dlopen() time and inits the callbacks */

int mod_setenv_plugin_init(plugin *p);
int mod_setenv_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("setenv");

	p->init        = mod_setenv_init;
	p->handle_uri_clean  = mod_setenv_uri_handler;
	p->set_defaults  = mod_setenv_set_defaults;
	p->cleanup     = mod_setenv_free;

	p->connection_reset  = mod_setenv_reset;

	p->data        = NULL;

	return 0;
}
