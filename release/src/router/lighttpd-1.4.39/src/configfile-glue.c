#include "base.h"
#include "buffer.h"
#include "array.h"
#include "log.h"
#include "plugin.h"

#include "configfile.h"

#include <string.h>
#include <stdlib.h>

/**
 * like all glue code this file contains functions which
 * are the external interface of lighttpd. The functions
 * are used by the server itself and the plugins.
 *
 * The main-goal is to have a small library in the end
 * which is linked against both and which will define
 * the interface itself in the end.
 *
 */


/* handle global options */

/* parse config array */
int config_insert_values_internal(server *srv, array *ca, const config_values_t cv[], config_scope_type_t scope) {
	size_t i;
	data_unset *du;

	for (i = 0; cv[i].key; i++) {

		if (NULL == (du = array_get_element(ca, cv[i].key))) {
			/* no found */

			continue;
		}

		if ((T_CONFIG_SCOPE_SERVER == cv[i].scope)
		    && (T_CONFIG_SCOPE_SERVER != scope)) {
			/* server scope options should only be set in server scope, not in conditionals */
			log_error_write(srv, __FILE__, __LINE__, "ss",
				"DEPRECATED: don't set server options in conditionals, variable:",
				cv[i].key);
		}
		
		switch (cv[i].type) {
		case T_CONFIG_ARRAY:
			if (du->type == TYPE_ARRAY) {
				size_t j;
				data_array *da = (data_array *)du;

				for (j = 0; j < da->value->used; j++) {
					if (da->value->data[j]->type == TYPE_STRING) {
						data_string *ds = data_string_init();

						buffer_copy_buffer(ds->value, ((data_string *)(da->value->data[j]))->value);
						if (!da->is_index_key) {
							/* the id's were generated automaticly, as we copy now we might have to renumber them
							 * this is used to prepend server.modules by mod_indexfile as it has to be loaded
							 * before mod_fastcgi and friends */
							buffer_copy_buffer(ds->key, ((data_string *)(da->value->data[j]))->key);
						}

						array_insert_unique(cv[i].destination, (data_unset *)ds);
					} else {
						log_error_write(srv, __FILE__, __LINE__, "sssbsd",
								"the value of an array can only be a string, variable:",
								cv[i].key, "[", da->value->data[j]->key, "], type:", da->value->data[j]->type);

						return -1;
					}
				}
			} else {
				log_error_write(srv, __FILE__, __LINE__, "ss", cv[i].key, "should have been a array of strings like ... = ( \"...\" )");

				return -1;
			}
			break;
		case T_CONFIG_STRING:
			if (du->type == TYPE_STRING) {
				data_string *ds = (data_string *)du;
				buffer_copy_buffer(cv[i].destination, ds->value);
			} else {
				log_error_write(srv, __FILE__, __LINE__, "ssss", cv[i].key, "should have been a string like ... = \"...\"");

				return -1;
			}
			break;
		case T_CONFIG_SHORT:
			switch(du->type) {
			case TYPE_INTEGER: {
				data_integer *di = (data_integer *)du;

				*((unsigned short *)(cv[i].destination)) = di->value;
				break;
			}
			case TYPE_STRING: {
				data_string *ds = (data_string *)du;

				/* If the value came from an environment variable, then it is a
				 * data_string, although it may contain a number in ASCII
				 * decimal format.  We try to interpret the string as a decimal
				 * short before giving up, in order to support setting numeric
				 * values with environment variables (eg, port number).
				 */
				if (ds->value->ptr && *ds->value->ptr) {
					char *e;
					long l = strtol(ds->value->ptr, &e, 10);
					if (e != ds->value->ptr && !*e && l >=0 && l <= 65535) {
						*((unsigned short *)(cv[i].destination)) = l;
						break;
					}
				}

				log_error_write(srv, __FILE__, __LINE__, "ssb", "got a string but expected a short:", cv[i].key, ds->value);

				return -1;
			}
			default:
				log_error_write(srv, __FILE__, __LINE__, "ssds", "unexpected type for key:", cv[i].key, du->type, "expected a short integer, range 0 ... 65535");
				return -1;
			}
			break;
		case T_CONFIG_INT:
			switch(du->type) {
			case TYPE_INTEGER: {
				data_integer *di = (data_integer *)du;

				*((unsigned int *)(cv[i].destination)) = di->value;
				break;
			}
			case TYPE_STRING: {
				data_string *ds = (data_string *)du;

				if (ds->value->ptr && *ds->value->ptr) {
					char *e;
					long l = strtol(ds->value->ptr, &e, 10);
					if (e != ds->value->ptr && !*e && l >= 0) {
						*((unsigned int *)(cv[i].destination)) = l;
						break;
					}
				}

				log_error_write(srv, __FILE__, __LINE__, "ssb", "got a string but expected an integer:", cv[i].key, ds->value);

				return -1;
			}
			default:
				log_error_write(srv, __FILE__, __LINE__, "ssds", "unexpected type for key:", cv[i].key, du->type, "expected an integer, range 0 ... 4294967295");
				return -1;
			}
			break;
		case T_CONFIG_BOOLEAN:
			if (du->type == TYPE_STRING) {
				data_string *ds = (data_string *)du;

				if (buffer_is_equal_string(ds->value, CONST_STR_LEN("enable"))) {
					*((unsigned short *)(cv[i].destination)) = 1;
				} else if (buffer_is_equal_string(ds->value, CONST_STR_LEN("disable"))) {
					*((unsigned short *)(cv[i].destination)) = 0;
				} else {
					log_error_write(srv, __FILE__, __LINE__, "ssbs", "ERROR: unexpected value for key:", cv[i].key, ds->value, "(enable|disable)");

					return -1;
				}
			} else {
				log_error_write(srv, __FILE__, __LINE__, "ssss", "ERROR: unexpected type for key:", cv[i].key, "(string)", "\"(enable|disable)\"");

				return -1;
			}
			break;
		case T_CONFIG_LOCAL:
		case T_CONFIG_UNSET:
			break;
		case T_CONFIG_UNSUPPORTED:
			log_error_write(srv, __FILE__, __LINE__, "ssss", "ERROR: found unsupported key:", cv[i].key, "-", (char *)(cv[i].destination));

			srv->config_unsupported = 1;

			break;
		case T_CONFIG_DEPRECATED:
			log_error_write(srv, __FILE__, __LINE__, "ssss", "ERROR: found deprecated key:", cv[i].key, "-", (char *)(cv[i].destination));

			srv->config_deprecated = 1;

			break;
		}
	}

	return 0;
}

int config_insert_values_global(server *srv, array *ca, const config_values_t cv[], config_scope_type_t scope) {
	size_t i;
	data_unset *du;

	for (i = 0; cv[i].key; i++) {
		data_string *touched;

		if (NULL == (du = array_get_element(ca, cv[i].key))) {
			/* no found */

			continue;
		}

		/* touched */
		touched = data_string_init();
		
		buffer_copy_string_len(touched->value, CONST_STR_LEN(""));
		buffer_copy_buffer(touched->key, du->key);

		array_insert_unique(srv->config_touched, (data_unset *)touched);
	}
	
	return config_insert_values_internal(srv, ca, cv, scope);
}

static unsigned short sock_addr_get_port(sock_addr *addr) {
#ifdef HAVE_IPV6
	return ntohs(addr->plain.sa_family ? addr->ipv6.sin6_port : addr->ipv4.sin_port);
#else
	return ntohs(addr->ipv4.sin_port);
#endif
}

static cond_result_t config_check_cond_cached(server *srv, connection *con, data_config *dc);

static cond_result_t config_check_cond_nocache(server *srv, connection *con, data_config *dc) {
	buffer *l;
	server_socket *srv_sock = con->srv_socket;

	/* check parent first */
	if (dc->parent && dc->parent->context_ndx) {
		/**
		 * a nested conditional 
		 *
		 * if the parent is not decided yet or false, we can't be true either 
		 */
		if (con->conf.log_condition_handling) {
			log_error_write(srv, __FILE__, __LINE__,  "sb", "go parent", dc->parent->key);
		}

		switch (config_check_cond_cached(srv, con, dc->parent)) {
		case COND_RESULT_FALSE:
			return COND_RESULT_FALSE;
		case COND_RESULT_UNSET:
			return COND_RESULT_UNSET;
		default:
			break;
		}
	}

	if (dc->prev) {
		/**
		 * a else branch
		 *
		 * we can only be executed, if all of our previous brothers 
		 * are false
		 */
		if (con->conf.log_condition_handling) {
			log_error_write(srv, __FILE__, __LINE__,  "sb", "go prev", dc->prev->key);
		}

		/* make sure prev is checked first */
		config_check_cond_cached(srv, con, dc->prev);

		/* one of prev set me to FALSE */
		switch (con->cond_cache[dc->context_ndx].result) {
		case COND_RESULT_FALSE:
			return con->cond_cache[dc->context_ndx].result;
		default:
			break;
		}
	}

	if (!con->conditional_is_valid[dc->comp]) {
		if (con->conf.log_condition_handling) {
			log_error_write(srv, __FILE__, __LINE__,  "dss", 
				dc->comp,
				dc->key->ptr,
				con->conditional_is_valid[dc->comp] ? "yeah" : "nej");
		}

		return COND_RESULT_UNSET;
	}

	/* pass the rules */

	switch (dc->comp) {
	case COMP_HTTP_HOST: {
		char *ck_colon = NULL, *val_colon = NULL;

		if (!buffer_string_is_empty(con->uri.authority)) {

			/*
			 * append server-port to the HTTP_POST if necessary
			 */

			l = con->uri.authority;

			switch(dc->cond) {
			case CONFIG_COND_NE:
			case CONFIG_COND_EQ:
				ck_colon = strchr(dc->string->ptr, ':');
				val_colon = strchr(l->ptr, ':');

				if (NULL != ck_colon && NULL == val_colon) {
					/* condition "host:port" but client send "host" */
					buffer_copy_buffer(srv->cond_check_buf, l);
					buffer_append_string_len(srv->cond_check_buf, CONST_STR_LEN(":"));
					buffer_append_int(srv->cond_check_buf, sock_addr_get_port(&(srv_sock->addr)));
					l = srv->cond_check_buf;
				} else if (NULL != val_colon && NULL == ck_colon) {
					/* condition "host" but client send "host:port" */
					buffer_copy_string_len(srv->cond_check_buf, l->ptr, val_colon - l->ptr);
					l = srv->cond_check_buf;
				}
				break;
			default:
				break;
			}
#if defined USE_OPENSSL && ! defined OPENSSL_NO_TLSEXT
		} else if (!buffer_string_is_empty(con->tlsext_server_name)) {
			l = con->tlsext_server_name;
#endif
		} else {
			l = srv->empty_string;
		}
		break;
	}
	case COMP_HTTP_REMOTE_IP: {
		char *nm_slash;
		/* handle remoteip limitations
		 *
		 * "10.0.0.1" is provided for all comparisions
		 *
		 * only for == and != we support
		 *
		 * "10.0.0.1/24"
		 */

		if ((dc->cond == CONFIG_COND_EQ ||
		     dc->cond == CONFIG_COND_NE) &&
		    (con->dst_addr.plain.sa_family == AF_INET) &&
		    (NULL != (nm_slash = strchr(dc->string->ptr, '/')))) {
			int nm_bits;
			long nm;
			char *err;
			struct in_addr val_inp;

			if (*(nm_slash+1) == '\0') {
				log_error_write(srv, __FILE__, __LINE__, "sb", "ERROR: no number after / ", dc->string);

				return COND_RESULT_FALSE;
			}

			nm_bits = strtol(nm_slash + 1, &err, 10);

			if (*err) {
				log_error_write(srv, __FILE__, __LINE__, "sbs", "ERROR: non-digit found in netmask:", dc->string, err);

				return COND_RESULT_FALSE;
			}

			if (nm_bits > 32 || nm_bits < 0) {
				log_error_write(srv, __FILE__, __LINE__, "sbs", "ERROR: invalid netmask:", dc->string, err);

				return COND_RESULT_FALSE;
			}

			/* take IP convert to the native */
			buffer_copy_string_len(srv->cond_check_buf, dc->string->ptr, nm_slash - dc->string->ptr);
#ifdef __WIN32
			if (INADDR_NONE == (val_inp.s_addr = inet_addr(srv->cond_check_buf->ptr))) {
				log_error_write(srv, __FILE__, __LINE__, "sb", "ERROR: ip addr is invalid:", srv->cond_check_buf);

				return COND_RESULT_FALSE;
			}

#else
			if (0 == inet_aton(srv->cond_check_buf->ptr, &val_inp)) {
				log_error_write(srv, __FILE__, __LINE__, "sb", "ERROR: ip addr is invalid:", srv->cond_check_buf);

				return COND_RESULT_FALSE;
			}
#endif

			/* build netmask */
			nm = nm_bits ? htonl(~((1 << (32 - nm_bits)) - 1)) : 0;

			if ((val_inp.s_addr & nm) == (con->dst_addr.ipv4.sin_addr.s_addr & nm)) {
				return (dc->cond == CONFIG_COND_EQ) ? COND_RESULT_TRUE : COND_RESULT_FALSE;
			} else {
				return (dc->cond == CONFIG_COND_EQ) ? COND_RESULT_FALSE : COND_RESULT_TRUE;
			}
		} else {
			l = con->dst_addr_buf;
		}
		break;
	}
	case COMP_HTTP_SCHEME:
		l = con->uri.scheme;
		break;

	case COMP_HTTP_URL:
		l = con->uri.path;
		break;

	case COMP_HTTP_QUERY_STRING:
		l = con->uri.query;
		break;

	case COMP_SERVER_SOCKET:
		l = srv_sock->srv_token;
		break;

	case COMP_HTTP_REFERER: {
		data_string *ds;

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Referer"))) {
			l = ds->value;
		} else {
			l = srv->empty_string;
		}
		break;
	}
	case COMP_HTTP_COOKIE: {
		data_string *ds;
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Cookie"))) {
			l = ds->value;
		} else {
			l = srv->empty_string;
		}
		break;
	}
	case COMP_HTTP_USER_AGENT: {
		data_string *ds;
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "User-Agent"))) {
			l = ds->value;
		} else {
			l = srv->empty_string;
		}
		break;
	}
	case COMP_HTTP_REQUEST_METHOD: {
		const char *method = get_http_method_name(con->request.http_method);

		/* we only have the request method as const char but we need a buffer for comparing */

		buffer_copy_string(srv->tmp_buf, method);

		l = srv->tmp_buf;

		break;
	}
	case COMP_HTTP_LANGUAGE: {
		data_string *ds;
		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "Accept-Language"))) {
			l = ds->value;
		} else {
			l = srv->empty_string;
		}
		break;
	}
	default:
		return COND_RESULT_FALSE;
	}

	if (NULL == l) {
		if (con->conf.log_condition_handling) {
			log_error_write(srv, __FILE__, __LINE__,  "bsbs", dc->comp_key,
					"(", l, ") compare to NULL");
		}
		return COND_RESULT_FALSE;
	}

	if (con->conf.log_condition_handling) {
		log_error_write(srv, __FILE__, __LINE__,  "bsbsb", dc->comp_key,
				"(", l, ") compare to ", dc->string);
	}
	switch(dc->cond) {
	case CONFIG_COND_NE:
	case CONFIG_COND_EQ:
		if (buffer_is_equal(l, dc->string)) {
			return (dc->cond == CONFIG_COND_EQ) ? COND_RESULT_TRUE : COND_RESULT_FALSE;
		} else {
			return (dc->cond == CONFIG_COND_EQ) ? COND_RESULT_FALSE : COND_RESULT_TRUE;
		}
		break;
#ifdef HAVE_PCRE_H
	case CONFIG_COND_NOMATCH:
	case CONFIG_COND_MATCH: {
		cond_cache_t *cache = &con->cond_cache[dc->context_ndx];
		int n;

#ifndef elementsof
#define elementsof(x) (sizeof(x) / sizeof(x[0]))
#endif
		n = pcre_exec(dc->regex, dc->regex_study, CONST_BUF_LEN(l), 0, 0,
				cache->matches, elementsof(cache->matches));

		cache->patterncount = n;
		if (n > 0) {
			cache->comp_value = l;
			cache->comp_type  = dc->comp;
			return (dc->cond == CONFIG_COND_MATCH) ? COND_RESULT_TRUE : COND_RESULT_FALSE;
		} else {
			/* cache is already cleared */
			return (dc->cond == CONFIG_COND_MATCH) ? COND_RESULT_FALSE : COND_RESULT_TRUE;
		}
		break;
	}
#endif
	default:
		/* no way */
		break;
	}

	return COND_RESULT_FALSE;
}

static cond_result_t config_check_cond_cached(server *srv, connection *con, data_config *dc) {
	cond_cache_t *caches = con->cond_cache;

	if (COND_RESULT_UNSET == caches[dc->context_ndx].result) {
		if (COND_RESULT_TRUE == (caches[dc->context_ndx].result = config_check_cond_nocache(srv, con, dc))) {
			if (dc->next) {
				data_config *c;
				if (con->conf.log_condition_handling) {
					log_error_write(srv, __FILE__, __LINE__, "s",
							"setting remains of chaining to false");
				}
				for (c = dc->next; c; c = c->next) {
					caches[c->context_ndx].result = COND_RESULT_FALSE;
				}
			}
		}
		caches[dc->context_ndx].comp_type = dc->comp;

		if (con->conf.log_condition_handling) {
			log_error_write(srv, __FILE__, __LINE__, "dss", dc->context_ndx,
					"(uncached) result:",
					caches[dc->context_ndx].result == COND_RESULT_UNSET ? "unknown" :
						(caches[dc->context_ndx].result == COND_RESULT_TRUE ? "true" : "false"));
		}
	} else {
		if (con->conf.log_condition_handling) {
			log_error_write(srv, __FILE__, __LINE__, "dss", dc->context_ndx,
					"(cached) result:",
					caches[dc->context_ndx].result == COND_RESULT_UNSET ? "unknown" : 
						(caches[dc->context_ndx].result == COND_RESULT_TRUE ? "true" : "false"));
		}
	}
	return caches[dc->context_ndx].result;
}

/**
 * reset the config-cache for a named item
 *
 * if the item is COND_LAST_ELEMENT we reset all items
 */
void config_cond_cache_reset_item(server *srv, connection *con, comp_key_t item) {
	size_t i;

	for (i = 0; i < srv->config_context->used; i++) {
		if (item == COMP_LAST_ELEMENT || 
		    con->cond_cache[i].comp_type == item) {
			con->cond_cache[i].result = COND_RESULT_UNSET;
			con->cond_cache[i].patterncount = 0;
			con->cond_cache[i].comp_value = NULL;
		}
	}
}

/**
 * reset the config cache to its initial state at connection start
 */
void config_cond_cache_reset(server *srv, connection *con) {
	size_t i;

	config_cond_cache_reset_all_items(srv, con);

	for (i = 0; i < COMP_LAST_ELEMENT; i++) {
		con->conditional_is_valid[i] = 0;
	}
}

int config_check_cond(server *srv, connection *con, data_config *dc) {
	if (con->conf.log_condition_handling) {
		log_error_write(srv, __FILE__, __LINE__,  "s",  "=== start of condition block ===");
	}
	return (config_check_cond_cached(srv, con, dc) == COND_RESULT_TRUE);
}

int config_append_cond_match_buffer(connection *con, data_config *dc, buffer *buf, int n)
{
	cond_cache_t *cache = &con->cond_cache[dc->context_ndx];
	if (n >= cache->patterncount) {
		return 0;
	}

	n <<= 1; /* n *= 2 */
	buffer_append_string_len(buf,
			cache->comp_value->ptr + cache->matches[n],
			cache->matches[n + 1] - cache->matches[n]);
	return 1;
}

