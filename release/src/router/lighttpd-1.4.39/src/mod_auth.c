#include "plugin.h"
#include "http_auth.h"
#include "log.h"
#include "response.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

handler_t auth_ldap_init(server *srv, mod_auth_plugin_config *s);


/**
 * the basic and digest auth framework
 *
 * - config handling
 * - protocol handling
 *
 * http_auth.c
 * http_auth_digest.c
 *
 * do the real work
 */

INIT_FUNC(mod_auth_init) {
	mod_auth_plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->tmp_buf = buffer_init();

	p->auth_user = buffer_init();
#ifdef USE_LDAP
	p->ldap_filter = buffer_init();
#endif

	return p;
}

FREE_FUNC(mod_auth_free) {
	mod_auth_plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	buffer_free(p->tmp_buf);
	buffer_free(p->auth_user);
#ifdef USE_LDAP
	buffer_free(p->ldap_filter);
#endif

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			mod_auth_plugin_config *s = p->config_storage[i];

			if (NULL == s) continue;

			array_free(s->auth_require);
			buffer_free(s->auth_plain_groupfile);
			buffer_free(s->auth_plain_userfile);
			buffer_free(s->auth_htdigest_userfile);
			buffer_free(s->auth_htpasswd_userfile);
			buffer_free(s->auth_backend_conf);

			buffer_free(s->auth_ldap_hostname);
			buffer_free(s->auth_ldap_basedn);
			buffer_free(s->auth_ldap_binddn);
			buffer_free(s->auth_ldap_bindpw);
			buffer_free(s->auth_ldap_filter);
			buffer_free(s->auth_ldap_cafile);

#ifdef USE_LDAP
			buffer_free(s->ldap_filter_pre);
			buffer_free(s->ldap_filter_post);

			if (s->ldap) ldap_unbind_s(s->ldap);
#endif

			free(s);
		}
		free(p->config_storage);
	}

	free(p);

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_auth_patch_connection(server *srv, connection *con, mod_auth_plugin_data *p) {
	size_t i, j;
	mod_auth_plugin_config *s = p->config_storage[0];

	PATCH(auth_backend);
	PATCH(auth_plain_groupfile);
	PATCH(auth_plain_userfile);
	PATCH(auth_htdigest_userfile);
	PATCH(auth_htpasswd_userfile);
	PATCH(auth_require);
	PATCH(auth_debug);
	PATCH(auth_ldap_hostname);
	PATCH(auth_ldap_basedn);
	PATCH(auth_ldap_binddn);
	PATCH(auth_ldap_bindpw);
	PATCH(auth_ldap_filter);
	PATCH(auth_ldap_cafile);
	PATCH(auth_ldap_starttls);
	PATCH(auth_ldap_allow_empty_pw);
#ifdef USE_LDAP
	p->anon_conf = s;
	PATCH(ldap_filter_pre);
	PATCH(ldap_filter_post);
#endif

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.backend"))) {
				PATCH(auth_backend);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.backend.plain.groupfile"))) {
				PATCH(auth_plain_groupfile);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.backend.plain.userfile"))) {
				PATCH(auth_plain_userfile);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.backend.htdigest.userfile"))) {
				PATCH(auth_htdigest_userfile);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.backend.htpasswd.userfile"))) {
				PATCH(auth_htpasswd_userfile);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.require"))) {
				PATCH(auth_require);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.debug"))) {
				PATCH(auth_debug);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.backend.ldap.hostname"))) {
				PATCH(auth_ldap_hostname);
#ifdef USE_LDAP
				p->anon_conf = s;
#endif
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.backend.ldap.base-dn"))) {
				PATCH(auth_ldap_basedn);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.backend.ldap.filter"))) {
				PATCH(auth_ldap_filter);
#ifdef USE_LDAP
				PATCH(ldap_filter_pre);
				PATCH(ldap_filter_post);
#endif
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.backend.ldap.ca-file"))) {
				PATCH(auth_ldap_cafile);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.backend.ldap.starttls"))) {
				PATCH(auth_ldap_starttls);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.backend.ldap.bind-dn"))) {
				PATCH(auth_ldap_binddn);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.backend.ldap.bind-pw"))) {
				PATCH(auth_ldap_bindpw);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("auth.backend.ldap.allow-empty-pw"))) {
				PATCH(auth_ldap_allow_empty_pw);
			}
		}
	}

	return 0;
}
#undef PATCH

static handler_t mod_auth_uri_handler(server *srv, connection *con, void *p_d) {
	size_t k;
	int auth_required = 0, auth_satisfied = 0;
	char *http_authorization = NULL;
	const char *auth_type = NULL;
	data_string *ds;
	mod_auth_plugin_data *p = p_d;
	array *req;
	data_string *req_method;

	/* select the right config */
	mod_auth_patch_connection(srv, con, p);

	if (p->conf.auth_require == NULL) return HANDLER_GO_ON;

	/*
	 * AUTH
	 *
	 */

	/* do we have to ask for auth ? */

	auth_required = 0;
	auth_satisfied = 0;

	/* search auth-directives for path */
	for (k = 0; k < p->conf.auth_require->used; k++) {
		buffer *require = p->conf.auth_require->data[k]->key;

		if (buffer_is_empty(require)) continue;
		if (buffer_string_length(con->uri.path) < buffer_string_length(require)) continue;

		/* if we have a case-insensitive FS we have to lower-case the URI here too */

		if (con->conf.force_lowercase_filenames) {
			if (0 == strncasecmp(con->uri.path->ptr, require->ptr, buffer_string_length(require))) {
				auth_required = 1;
				break;
			}
		} else {
			if (0 == strncmp(con->uri.path->ptr, require->ptr, buffer_string_length(require))) {
				auth_required = 1;
				break;
			}
		}
	}

	/* nothing to do for us */
	if (auth_required == 0) return HANDLER_GO_ON;

	req = ((data_array *)(p->conf.auth_require->data[k]))->value;
	req_method = (data_string *)array_get_element(req, "method");

	if (0 == strcmp(req_method->value->ptr, "extern")) {
		/* require REMOTE_USER to be already set */
		if (NULL == (ds = (data_string *)array_get_element(con->environment, "REMOTE_USER"))) {
			con->http_status = 401;
			con->mode = DIRECT;
			return HANDLER_FINISHED;
		} else if (http_auth_match_rules(srv, req, ds->value->ptr, NULL, NULL)) {
			log_error_write(srv, __FILE__, __LINE__, "s", "rules didn't match");
			con->http_status = 401;
			con->mode = DIRECT;
			return HANDLER_FINISHED;
		} else {
			return HANDLER_GO_ON;
		}
	}

	/* try to get Authorization-header */

	if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Authorization")) && !buffer_is_empty(ds->value)) {
		char *auth_realm;

		http_authorization = ds->value->ptr;

		/* parse auth-header */
		if (NULL != (auth_realm = strchr(http_authorization, ' '))) {
			int auth_type_len = auth_realm - http_authorization;

			if ((auth_type_len == 5) &&
			    (0 == strncasecmp(http_authorization, "Basic", auth_type_len))) {
				auth_type = "Basic";

				if (0 == strcmp(req_method->value->ptr, "basic")) {
					auth_satisfied = http_auth_basic_check(srv, con, p, req, auth_realm+1);
				}
			} else if ((auth_type_len == 6) &&
				   (0 == strncasecmp(http_authorization, "Digest", auth_type_len))) {
				auth_type = "Digest";
				if (0 == strcmp(req_method->value->ptr, "digest")) {
					if (-1 == (auth_satisfied = http_auth_digest_check(srv, con, p, req, auth_realm+1))) {
						con->http_status = 400;
						con->mode = DIRECT;

						/* a field was missing */

						return HANDLER_FINISHED;
					}
				}
			} else {
				log_error_write(srv, __FILE__, __LINE__, "ss",
						"unknown authentication type:",
						http_authorization);
			}
		}
	}

	if (!auth_satisfied) {
		data_string *method, *realm;
		method = (data_string *)array_get_element(req, "method");
		realm = (data_string *)array_get_element(req, "realm");

		con->http_status = 401;
		con->mode = DIRECT;

		if (0 == strcmp(method->value->ptr, "basic")) {
			buffer_copy_string_len(p->tmp_buf, CONST_STR_LEN("Basic realm=\""));
			buffer_append_string_buffer(p->tmp_buf, realm->value);
			buffer_append_string_len(p->tmp_buf, CONST_STR_LEN("\""));

			response_header_insert(srv, con, CONST_STR_LEN("WWW-Authenticate"), CONST_BUF_LEN(p->tmp_buf));
		} else if (0 == strcmp(method->value->ptr, "digest")) {
			char hh[33];
			http_auth_digest_generate_nonce(srv, p, srv->tmp_buf, hh);

			buffer_copy_string_len(p->tmp_buf, CONST_STR_LEN("Digest realm=\""));
			buffer_append_string_buffer(p->tmp_buf, realm->value);
			buffer_append_string_len(p->tmp_buf, CONST_STR_LEN("\", nonce=\""));
			buffer_append_string(p->tmp_buf, hh);
			buffer_append_string_len(p->tmp_buf, CONST_STR_LEN("\", qop=\"auth\""));

			response_header_insert(srv, con, CONST_STR_LEN("WWW-Authenticate"), CONST_BUF_LEN(p->tmp_buf));
		} else {
			/* evil */
		}
		return HANDLER_FINISHED;
	} else {
		/* the REMOTE_USER header */

		if (NULL == (ds = (data_string *)array_get_element(con->environment, "REMOTE_USER"))) {
			if (NULL == (ds = (data_string *)array_get_unused_element(con->environment, TYPE_STRING))) {
				ds = data_string_init();
			}
			buffer_copy_string(ds->key, "REMOTE_USER");
			array_insert_unique(con->environment, (data_unset *)ds);
		}
		buffer_copy_buffer(ds->value, p->auth_user);

		/* AUTH_TYPE environment */

		if (NULL == (ds = (data_string *)array_get_element(con->environment, "AUTH_TYPE"))) {
			if (NULL == (ds = (data_string *)array_get_unused_element(con->environment, TYPE_STRING))) {
				ds = data_string_init();
			}
			buffer_copy_string(ds->key, "AUTH_TYPE");
			array_insert_unique(con->environment, (data_unset *)ds);
		}
		buffer_copy_string(ds->value, auth_type);
	}

	return HANDLER_GO_ON;
}

SETDEFAULTS_FUNC(mod_auth_set_defaults) {
	mod_auth_plugin_data *p = p_d;
	size_t i;

	config_values_t cv[] = {
		{ "auth.backend",                   NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 0 */
		{ "auth.backend.plain.groupfile",   NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 1 */
		{ "auth.backend.plain.userfile",    NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 2 */
		{ "auth.require",                   NULL, T_CONFIG_LOCAL, T_CONFIG_SCOPE_CONNECTION },  /* 3 */
		{ "auth.backend.ldap.hostname",     NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 4 */
		{ "auth.backend.ldap.base-dn",      NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 5 */
		{ "auth.backend.ldap.filter",       NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 6 */
		{ "auth.backend.ldap.ca-file",      NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 7 */
		{ "auth.backend.ldap.starttls",     NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 8 */
 		{ "auth.backend.ldap.bind-dn",      NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 9 */
 		{ "auth.backend.ldap.bind-pw",      NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 10 */
		{ "auth.backend.ldap.allow-empty-pw",     NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 11 */
		{ "auth.backend.htdigest.userfile", NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 12 */
		{ "auth.backend.htpasswd.userfile", NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION }, /* 13 */
		{ "auth.debug",                     NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },  /* 14 */
		{ NULL,                             NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	p->config_storage = calloc(1, srv->config_context->used * sizeof(mod_auth_plugin_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		mod_auth_plugin_config *s;
		size_t n;
		data_array *da;

		s = calloc(1, sizeof(mod_auth_plugin_config));
		s->auth_plain_groupfile = buffer_init();
		s->auth_plain_userfile = buffer_init();
		s->auth_htdigest_userfile = buffer_init();
		s->auth_htpasswd_userfile = buffer_init();
		s->auth_backend_conf = buffer_init();

		s->auth_ldap_hostname = buffer_init();
		s->auth_ldap_basedn = buffer_init();
		s->auth_ldap_binddn = buffer_init();
		s->auth_ldap_bindpw = buffer_init();
		s->auth_ldap_filter = buffer_init();
		s->auth_ldap_cafile = buffer_init();
		s->auth_ldap_starttls = 0;
		s->auth_debug = 0;

		s->auth_require = array_init();

#ifdef USE_LDAP
		s->ldap_filter_pre = buffer_init();
		s->ldap_filter_post = buffer_init();
		s->ldap = NULL;
#endif

		cv[0].destination = s->auth_backend_conf;
		cv[1].destination = s->auth_plain_groupfile;
		cv[2].destination = s->auth_plain_userfile;
		cv[3].destination = s->auth_require;
		cv[4].destination = s->auth_ldap_hostname;
		cv[5].destination = s->auth_ldap_basedn;
		cv[6].destination = s->auth_ldap_filter;
		cv[7].destination = s->auth_ldap_cafile;
		cv[8].destination = &(s->auth_ldap_starttls);
		cv[9].destination = s->auth_ldap_binddn;
		cv[10].destination = s->auth_ldap_bindpw;
		cv[11].destination = &(s->auth_ldap_allow_empty_pw);
		cv[12].destination = s->auth_htdigest_userfile;
		cv[13].destination = s->auth_htpasswd_userfile;
		cv[14].destination = &(s->auth_debug);

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}

		if (!buffer_string_is_empty(s->auth_backend_conf)) {
			if (0 == strcmp(s->auth_backend_conf->ptr, "htpasswd")) {
				s->auth_backend = AUTH_BACKEND_HTPASSWD;
			} else if (0 == strcmp(s->auth_backend_conf->ptr, "htdigest")) {
				s->auth_backend = AUTH_BACKEND_HTDIGEST;
			} else if (0 == strcmp(s->auth_backend_conf->ptr, "plain")) {
				s->auth_backend = AUTH_BACKEND_PLAIN;
			} else if (0 == strcmp(s->auth_backend_conf->ptr, "ldap")) {
				s->auth_backend = AUTH_BACKEND_LDAP;
			} else {
				log_error_write(srv, __FILE__, __LINE__, "sb", "auth.backend not supported:", s->auth_backend_conf);

				return HANDLER_ERROR;
			}
		}

#ifdef USE_LDAP
		if (!buffer_string_is_empty(s->auth_ldap_filter)) {
			char *dollar;

			/* parse filter */

			if (NULL == (dollar = strchr(s->auth_ldap_filter->ptr, '$'))) {
				log_error_write(srv, __FILE__, __LINE__, "s", "ldap: auth.backend.ldap.filter is missing a replace-operator '$'");

				return HANDLER_ERROR;
			}

			buffer_copy_string_len(s->ldap_filter_pre, s->auth_ldap_filter->ptr, dollar - s->auth_ldap_filter->ptr);
			buffer_copy_string(s->ldap_filter_post, dollar+1);
		}
#endif

		/* no auth.require for this section */
		if (NULL == (da = (data_array *)array_get_element(config->value, "auth.require"))) continue;

		if (da->type != TYPE_ARRAY) continue;

		for (n = 0; n < da->value->used; n++) {
			size_t m;
			data_array *da_file = (data_array *)da->value->data[n];
			const char *method, *realm, *require;

			if (da->value->data[n]->type != TYPE_ARRAY) {
				log_error_write(srv, __FILE__, __LINE__, "ss",
						"auth.require should contain an array as in:",
						"auth.require = ( \"...\" => ( ..., ...) )");

				return HANDLER_ERROR;
			}

			method = realm = require = NULL;

			for (m = 0; m < da_file->value->used; m++) {
				if (da_file->value->data[m]->type == TYPE_STRING) {
					if (0 == strcmp(da_file->value->data[m]->key->ptr, "method")) {
						method = ((data_string *)(da_file->value->data[m]))->value->ptr;
					} else if (0 == strcmp(da_file->value->data[m]->key->ptr, "realm")) {
						realm = ((data_string *)(da_file->value->data[m]))->value->ptr;
					} else if (0 == strcmp(da_file->value->data[m]->key->ptr, "require")) {
						require = ((data_string *)(da_file->value->data[m]))->value->ptr;
					} else {
						log_error_write(srv, __FILE__, __LINE__, "ssbs",
							"the field is unknown in:",
							"auth.require = ( \"...\" => ( ..., -> \"",
							da_file->value->data[m]->key,
							"\" <- => \"...\" ) )");

						return HANDLER_ERROR;
					}
				} else {
					log_error_write(srv, __FILE__, __LINE__, "ssbs",
						"a string was expected for:",
						"auth.require = ( \"...\" => ( ..., -> \"",
						da_file->value->data[m]->key,
						"\" <- => \"...\" ) )");

					return HANDLER_ERROR;
				}
			}

			if (method == NULL) {
				log_error_write(srv, __FILE__, __LINE__, "ss",
						"the method field is missing in:",
						"auth.require = ( \"...\" => ( ..., \"method\" => \"...\" ) )");
				return HANDLER_ERROR;
			} else {
				if (0 != strcmp(method, "basic") &&
				    0 != strcmp(method, "digest") &&
				    0 != strcmp(method, "extern")) {
					log_error_write(srv, __FILE__, __LINE__, "ss",
							"method has to be either \"basic\", \"digest\" or \"extern\" in",
							"auth.require = ( \"...\" => ( ..., \"method\" => \"...\") )");
					return HANDLER_ERROR;
				}
			}

			if (realm == NULL) {
				log_error_write(srv, __FILE__, __LINE__, "ss",
						"the realm field is missing in:",
						"auth.require = ( \"...\" => ( ..., \"realm\" => \"...\" ) )");
				return HANDLER_ERROR;
			}

			if (require == NULL) {
				log_error_write(srv, __FILE__, __LINE__, "ss",
						"the require field is missing in:",
						"auth.require = ( \"...\" => ( ..., \"require\" => \"...\" ) )");
				return HANDLER_ERROR;
			}

			if (method && realm && require) {
				data_string *ds;
				data_array *a;

				a = data_array_init();
				buffer_copy_buffer(a->key, da_file->key);

				ds = data_string_init();

				buffer_copy_string_len(ds->key, CONST_STR_LEN("method"));
				buffer_copy_string(ds->value, method);

				array_insert_unique(a->value, (data_unset *)ds);

				ds = data_string_init();

				buffer_copy_string_len(ds->key, CONST_STR_LEN("realm"));
				buffer_copy_string(ds->value, realm);

				array_insert_unique(a->value, (data_unset *)ds);

				ds = data_string_init();

				buffer_copy_string_len(ds->key, CONST_STR_LEN("require"));
				buffer_copy_string(ds->value, require);

				array_insert_unique(a->value, (data_unset *)ds);

				array_insert_unique(s->auth_require, (data_unset *)a);
			}
		}

		switch(s->auth_backend) {
		case AUTH_BACKEND_LDAP: {
			handler_t ret = auth_ldap_init(srv, s);
			if (ret == HANDLER_ERROR)
				return (ret);
			break;
		}
		default:
			break;
		}
	}

	return HANDLER_GO_ON;
}

handler_t auth_ldap_init(server *srv, mod_auth_plugin_config *s) {
#ifdef USE_LDAP
	int ret;
#if 0
	if (s->auth_ldap_basedn->used == 0) {
		log_error_write(srv, __FILE__, __LINE__, "s", "ldap: auth.backend.ldap.base-dn has to be set");

		return HANDLER_ERROR;
	}
#endif

	if (!buffer_string_is_empty(s->auth_ldap_hostname)) {
		/* free old context */
		if (NULL != s->ldap) ldap_unbind_s(s->ldap);

		if (NULL == (s->ldap = ldap_init(s->auth_ldap_hostname->ptr, LDAP_PORT))) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "ldap ...", strerror(errno));

			return HANDLER_ERROR;
		}

		ret = LDAP_VERSION3;
		if (LDAP_OPT_SUCCESS != (ret = ldap_set_option(s->ldap, LDAP_OPT_PROTOCOL_VERSION, &ret))) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "ldap:", ldap_err2string(ret));

			return HANDLER_ERROR;
		}

		if (s->auth_ldap_starttls) {
			/* if no CA file is given, it is ok, as we will use encryption
				* if the server requires a CAfile it will tell us */
			if (!buffer_string_is_empty(s->auth_ldap_cafile)) {
				if (LDAP_OPT_SUCCESS != (ret = ldap_set_option(NULL, LDAP_OPT_X_TLS_CACERTFILE,
								s->auth_ldap_cafile->ptr))) {
					log_error_write(srv, __FILE__, __LINE__, "ss",
							"Loading CA certificate failed:", ldap_err2string(ret));

					return HANDLER_ERROR;
				}
			}

			if (LDAP_OPT_SUCCESS != (ret = ldap_start_tls_s(s->ldap, NULL,  NULL))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "ldap startTLS failed:", ldap_err2string(ret));

				return HANDLER_ERROR;
			}
		}


		/* 1. */
		if (!buffer_string_is_empty(s->auth_ldap_binddn)) {
			if (LDAP_SUCCESS != (ret = ldap_simple_bind_s(s->ldap, s->auth_ldap_binddn->ptr, s->auth_ldap_bindpw->ptr))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "ldap:", ldap_err2string(ret));

				return HANDLER_ERROR;
			}
		} else {
			if (LDAP_SUCCESS != (ret = ldap_simple_bind_s(s->ldap, NULL, NULL))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "ldap:", ldap_err2string(ret));

				return HANDLER_ERROR;
			}
		}
	}
	return HANDLER_GO_ON;
#else
	UNUSED(s);
	log_error_write(srv, __FILE__, __LINE__, "s", "no ldap support available");
	return HANDLER_ERROR;
#endif
}
#ifndef APP_IPKG
int mod_auth_plugin_init(plugin *p);
int mod_auth_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("auth");
	p->init        = mod_auth_init;
	p->set_defaults = mod_auth_set_defaults;
	p->handle_uri_clean = mod_auth_uri_handler;
	p->cleanup     = mod_auth_free;

	p->data        = NULL;

	return 0;
}
#else
int aicloud_mod_auth_plugin_init(plugin *p);
int aicloud_mod_auth_plugin_init(plugin *p) {
    p->version     = LIGHTTPD_VERSION_ID;
    p->name        = buffer_init_string("auth");
    p->init        = mod_auth_init;
    p->set_defaults = mod_auth_set_defaults;
    p->handle_uri_clean = mod_auth_uri_handler;
    p->cleanup     = mod_auth_free;

    p->data        = NULL;

    return 0;
}
#endif
