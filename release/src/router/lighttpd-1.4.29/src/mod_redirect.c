#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"
#include "response.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	pcre_keyvalue_buffer *redirect;
	data_config *context; /* to which apply me */
} plugin_config;

typedef struct {
	PLUGIN_DATA;
	buffer *match_buf;
	buffer *location;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

INIT_FUNC(mod_redirect_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->match_buf = buffer_init();
	p->location = buffer_init();

	return p;
}

FREE_FUNC(mod_redirect_free) {
	plugin_data *p = p_d;

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			pcre_keyvalue_buffer_free(s->redirect);

			free(s);
		}
		free(p->config_storage);
	}


	buffer_free(p->match_buf);
	buffer_free(p->location);

	free(p);

	return HANDLER_GO_ON;
}

SETDEFAULTS_FUNC(mod_redirect_set_defaults) {
	plugin_data *p = p_d;
	data_unset *du;
	size_t i = 0;

	config_values_t cv[] = {
		{ "url.redirect",               NULL, T_CONFIG_LOCAL, T_CONFIG_SCOPE_CONNECTION }, /* 0 */
		{ NULL,                         NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	/* 0 */
	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;
		size_t j;
		array *ca;
		data_array *da = (data_array *)du;

		s = calloc(1, sizeof(plugin_config));
		s->redirect   = pcre_keyvalue_buffer_init();

		cv[0].destination = s->redirect;

		p->config_storage[i] = s;
		ca = ((data_config *)srv->config_context->data[i])->value;

		if (0 != config_insert_values_global(srv, ca, cv)) {
			return HANDLER_ERROR;
		}

		if (NULL == (du = array_get_element(ca, "url.redirect"))) {
			/* no url.redirect defined */
			continue;
		}

		if (du->type != TYPE_ARRAY) {
			log_error_write(srv, __FILE__, __LINE__, "sss",
					"unexpected type for key: ", "url.redirect", "array of strings");

			return HANDLER_ERROR;
		}

		da = (data_array *)du;

		for (j = 0; j < da->value->used; j++) {
			if (da->value->data[j]->type != TYPE_STRING) {
				log_error_write(srv, __FILE__, __LINE__, "sssbs",
						"unexpected type for key: ",
						"url.redirect",
						"[", da->value->data[j]->key, "](string)");

				return HANDLER_ERROR;
			}

			if (0 != pcre_keyvalue_buffer_append(srv, s->redirect,
							     ((data_string *)(da->value->data[j]))->key->ptr,
							     ((data_string *)(da->value->data[j]))->value->ptr)) {

				log_error_write(srv, __FILE__, __LINE__, "sb",
						"pcre-compile failed for", da->value->data[j]->key);
			}
		}
	}

	return HANDLER_GO_ON;
}
#ifdef HAVE_PCRE_H
static int mod_redirect_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	p->conf.redirect = s->redirect;
	p->conf.context = NULL;

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (0 == strcmp(du->key->ptr, "url.redirect")) {
				p->conf.redirect = s->redirect;
				p->conf.context = dc;
			}
		}
	}

	return 0;
}
#endif
static handler_t mod_redirect_uri_handler(server *srv, connection *con, void *p_data) {
#ifdef HAVE_PCRE_H
	plugin_data *p = p_data;
	size_t i;

	/*
	 * REWRITE URL
	 *
	 * e.g. redirect /base/ to /index.php?section=base
	 *
	 */

	mod_redirect_patch_connection(srv, con, p);

	buffer_copy_string_buffer(p->match_buf, con->request.uri);

	for (i = 0; i < p->conf.redirect->used; i++) {
		pcre *match;
		pcre_extra *extra;
		const char *pattern;
		size_t pattern_len;
		int n;
		pcre_keyvalue *kv = p->conf.redirect->kv[i];
# define N 10
		int ovec[N * 3];

		match       = kv->key;
		extra       = kv->key_extra;
		pattern     = kv->value->ptr;
		pattern_len = kv->value->used - 1;

		if ((n = pcre_exec(match, extra, p->match_buf->ptr, p->match_buf->used - 1, 0, 0, ovec, 3 * N)) < 0) {
			if (n != PCRE_ERROR_NOMATCH) {
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"execution error while matching: ", n);
				return HANDLER_ERROR;
			}
		} else {
			const char **list;
			size_t start;
			size_t k;

			/* it matched */
			pcre_get_substring_list(p->match_buf->ptr, ovec, n, &list);

			/* search for $[0-9] */

			buffer_reset(p->location);

			start = 0;
			for (k = 0; k + 1 < pattern_len; k++) {
				if (pattern[k] == '$' || pattern[k] == '%') {
					/* got one */

					size_t num = pattern[k + 1] - '0';

					buffer_append_string_len(p->location, pattern + start, k - start);

					if (!isdigit((unsigned char)pattern[k + 1])) {
						/* enable escape: "%%" => "%", "%a" => "%a", "$$" => "$" */
						buffer_append_string_len(p->location, pattern+k, pattern[k] == pattern[k+1] ? 1 : 2);
					} else if (pattern[k] == '$') {
						/* n is always > 0 */
						if (num < (size_t)n) {
							buffer_append_string(p->location, list[num]);
						}
					} else if (p->conf.context == NULL) {
						/* we have no context, we are global */
						log_error_write(srv, __FILE__, __LINE__, "sb",
								"used a rewrite containing a %[0-9]+ in the global scope, ignored:",
								kv->value);
					} else {
						config_append_cond_match_buffer(con, p->conf.context, p->location, num);
					}

					k++;
					start = k + 1;
				}
			}

			buffer_append_string_len(p->location, pattern + start, pattern_len - start);

			pcre_free(list);

			response_header_insert(srv, con, CONST_STR_LEN("Location"), CONST_BUF_LEN(p->location));

			con->http_status = 301;
			con->mode = DIRECT;
			con->file_finished = 1;

			return HANDLER_FINISHED;
		}
	}
#undef N

#else
	UNUSED(srv);
	UNUSED(con);
	UNUSED(p_data);
#endif

	return HANDLER_GO_ON;
}


int mod_redirect_plugin_init(plugin *p);
int mod_redirect_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("redirect");

	p->init        = mod_redirect_init;
	p->handle_uri_clean  = mod_redirect_uri_handler;
	p->set_defaults  = mod_redirect_set_defaults;
	p->cleanup     = mod_redirect_free;

	p->data        = NULL;

	return 0;
}
