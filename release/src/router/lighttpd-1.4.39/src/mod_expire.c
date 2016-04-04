#include "base.h"
#include "log.h"
#include "buffer.h"
#include "response.h"

#include "plugin.h"
#include "stat_cache.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * this is a expire module for a lighttpd
 *
 * set 'Expires:' HTTP Headers on demand
 */



/* plugin config for all request/connections */

typedef struct {
	array *expire_url;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	buffer *expire_tstmp;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

/* init the plugin data */
INIT_FUNC(mod_expire_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->expire_tstmp = buffer_init();

	buffer_string_prepare_copy(p->expire_tstmp, 255);

	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_expire_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	buffer_free(p->expire_tstmp);

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (NULL == s) continue;

			array_free(s->expire_url);
			free(s);
		}
		free(p->config_storage);
	}

	free(p);

	return HANDLER_GO_ON;
}

static int mod_expire_get_offset(server *srv, plugin_data *p, buffer *expire, time_t *offset) {
	char *ts;
	int type = -1;
	time_t retts = 0;

	UNUSED(p);

	/*
	 * parse
	 *
	 * '(access|now|modification) [plus] {<num> <type>}*'
	 *
	 * e.g. 'access 1 years'
	 */

	if (buffer_string_is_empty(expire)) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"empty:");
		return -1;
	}

	ts = expire->ptr;

	if (0 == strncmp(ts, "access ", 7)) {
		type  = 0;
		ts   += 7;
	} else if (0 == strncmp(ts, "now ", 4)) {
		type  = 0;
		ts   += 4;
	} else if (0 == strncmp(ts, "modification ", 13)) {
		type  = 1;
		ts   += 13;
	} else {
		/* invalid type-prefix */
		log_error_write(srv, __FILE__, __LINE__, "ss",
				"invalid <base>:", ts);
		return -1;
	}

	if (0 == strncmp(ts, "plus ", 5)) {
		/* skip the optional plus */
		ts   += 5;
	}

	/* the rest is just <number> (years|months|weeks|days|hours|minutes|seconds) */
	while (1) {
		char *space, *err;
		int num;

		if (NULL == (space = strchr(ts, ' '))) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
					"missing space after <num>:", ts);
			return -1;
		}

		num = strtol(ts, &err, 10);
		if (*err != ' ') {
			log_error_write(srv, __FILE__, __LINE__, "ss",
					"missing <type> after <num>:", ts);
			return -1;
		}

		ts = space + 1;

		if (NULL != (space = strchr(ts, ' '))) {
			int slen;
			/* */

			slen = space - ts;

			if (slen == 5 &&
			    0 == strncmp(ts, "years", slen)) {
				num *= 60 * 60 * 24 * 30 * 12;
			} else if (slen == 6 &&
				   0 == strncmp(ts, "months", slen)) {
				num *= 60 * 60 * 24 * 30;
			} else if (slen == 5 &&
				   0 == strncmp(ts, "weeks", slen)) {
				num *= 60 * 60 * 24 * 7;
			} else if (slen == 4 &&
				   0 == strncmp(ts, "days", slen)) {
				num *= 60 * 60 * 24;
			} else if (slen == 5 &&
				   0 == strncmp(ts, "hours", slen)) {
				num *= 60 * 60;
			} else if (slen == 7 &&
				   0 == strncmp(ts, "minutes", slen)) {
				num *= 60;
			} else if (slen == 7 &&
				   0 == strncmp(ts, "seconds", slen)) {
				num *= 1;
			} else {
				log_error_write(srv, __FILE__, __LINE__, "ss",
						"unknown type:", ts);
				return -1;
			}

			retts += num;

			ts = space + 1;
		} else {
			if (0 == strcmp(ts, "years")) {
				num *= 60 * 60 * 24 * 30 * 12;
			} else if (0 == strcmp(ts, "months")) {
				num *= 60 * 60 * 24 * 30;
			} else if (0 == strcmp(ts, "weeks")) {
				num *= 60 * 60 * 24 * 7;
			} else if (0 == strcmp(ts, "days")) {
				num *= 60 * 60 * 24;
			} else if (0 == strcmp(ts, "hours")) {
				num *= 60 * 60;
			} else if (0 == strcmp(ts, "minutes")) {
				num *= 60;
			} else if (0 == strcmp(ts, "seconds")) {
				num *= 1;
			} else {
				log_error_write(srv, __FILE__, __LINE__, "ss",
						"unknown type:", ts);
				return -1;
			}

			retts += num;

			break;
		}
	}

	if (offset != NULL) *offset = retts;

	return type;
}


/* handle plugin config and check values */

SETDEFAULTS_FUNC(mod_expire_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0, k;

	config_values_t cv[] = {
		{ "expire.url",                 NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ NULL,                         NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(plugin_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->expire_url    = array_init();

		cv[0].destination = s->expire_url;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}

		for (k = 0; k < s->expire_url->used; k++) {
			data_string *ds = (data_string *)s->expire_url->data[k];

			/* parse lines */
			if (-1 == mod_expire_get_offset(srv, p, ds->value, NULL)) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
						"parsing expire.url failed:", ds->value);
				return HANDLER_ERROR;
			}
		}
	}


	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_expire_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(expire_url);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("expire.url"))) {
				PATCH(expire_url);
			}
		}
	}

	return 0;
}
#undef PATCH

URIHANDLER_FUNC(mod_expire_path_handler) {
	plugin_data *p = p_d;
	int s_len;
	size_t k;

	if (buffer_is_empty(con->uri.path)) return HANDLER_GO_ON;

	mod_expire_patch_connection(srv, con, p);

	s_len = buffer_string_length(con->uri.path);

	for (k = 0; k < p->conf.expire_url->used; k++) {
		data_string *ds = (data_string *)p->conf.expire_url->data[k];
		int ct_len = buffer_string_length(ds->key);

		if (ct_len > s_len) continue;
		if (buffer_is_empty(ds->key)) continue;

		if (0 == strncmp(con->uri.path->ptr, ds->key->ptr, ct_len)) {
			time_t ts, expires;
			stat_cache_entry *sce = NULL;

			/* if stat fails => sce == NULL, ignore return value */
			(void) stat_cache_get_entry(srv, con, con->physical.path, &sce);

			switch(mod_expire_get_offset(srv, p, ds->value, &ts)) {
			case 0:
				/* access */
				expires = (ts + srv->cur_ts);
				break;
			case 1:
				/* modification */

				/* can't set modification based expire header if
				 * mtime is not available
				 */
				if (NULL == sce) return HANDLER_GO_ON;

				expires = (ts + sce->st.st_mtime);
				break;
			default:
				/* -1 is handled at parse-time */
				return HANDLER_ERROR;
			}

			/* expires should be at least srv->cur_ts */
			if (expires < srv->cur_ts) expires = srv->cur_ts;

			buffer_string_prepare_copy(p->expire_tstmp, 255);
			buffer_append_strftime(p->expire_tstmp, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&(expires)));

			/* HTTP/1.0 */
			response_header_overwrite(srv, con, CONST_STR_LEN("Expires"), CONST_BUF_LEN(p->expire_tstmp));

			/* HTTP/1.1 */
			buffer_copy_string_len(p->expire_tstmp, CONST_STR_LEN("max-age="));
			buffer_append_int(p->expire_tstmp, expires - srv->cur_ts); /* as expires >= srv->cur_ts the difference is >= 0 */

			response_header_append(srv, con, CONST_STR_LEN("Cache-Control"), CONST_BUF_LEN(p->expire_tstmp));

			return HANDLER_GO_ON;
		}
	}

	/* not found */
	return HANDLER_GO_ON;
}

/* this function is called at dlopen() time and inits the callbacks */

int mod_expire_plugin_init(plugin *p);
int mod_expire_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("expire");

	p->init        = mod_expire_init;
	p->handle_subrequest_start = mod_expire_path_handler;
	p->set_defaults  = mod_expire_set_defaults;
	p->cleanup     = mod_expire_free;

	p->data        = NULL;

	return 0;
}
