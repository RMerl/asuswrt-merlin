#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include "inet_ntop_cache.h"
#include "configfile.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>

/**
 * mod_extforward.c for lighttpd, by comman.kang <at> gmail <dot> com
 *                  extended, modified by Lionel Elie Mamane (LEM), lionel <at> mamane <dot> lu
 *                  support chained proxies by glen@delfi.ee, #1528
 *
 * Config example:
 *
 *       Trust proxy 10.0.0.232 and 10.0.0.232
 *       extforward.forwarder = ( "10.0.0.232" => "trust",
 *                                "10.0.0.233" => "trust" )
 *
 *       Trust all proxies  (NOT RECOMMENDED!)
 *       extforward.forwarder = ( "all" => "trust")
 *
 *       Note that "all" has precedence over specific entries,
 *       so "all except" setups will not work.
 *
 *       In case you have chained proxies, you can add all their IP's to the
 *       config. However "all" has effect only on connecting IP, as the
 *       X-Forwarded-For header can not be trusted.
 *
 * Note: The effect of this module is variable on $HTTP["remotip"] directives and
 *       other module's remote ip dependent actions.
 *  Things done by modules before we change the remoteip or after we reset it will match on the proxy's IP.
 *  Things done in between these two moments will match on the real client's IP.
 *  The moment things are done by a module depends on in which hook it does things and within the same hook
 *  on whether they are before/after us in the module loading order
 *  (order in the server.modules directive in the config file).
 *
 * Tested behaviours:
 *
 *  mod_access: Will match on the real client.
 *
 *  mod_accesslog:
 *   In order to see the "real" ip address in access log ,
 *   you'll have to load mod_extforward after mod_accesslog.
 *   like this:
 *
 *    server.modules  = (
 *       .....
 *       mod_accesslog,
 *       mod_extforward
 *    )
 *
 * Known issues:
 *      seems causing segfault with mod_ssl and $HTTP{"socket"} directives
 *      LEM 2006.05.26: Fixed segfault $SERVER["socket"] directive. Untested with SSL.
 *
 * ChangeLog:
 *     2005.12.19   Initial Version
 *     2005.12.19   fixed conflict with conditional directives
 *     2006.05.26   LEM: IPv6 support
 *     2006.05.26   LEM: Fix a segfault with $SERVER["socket"] directive.
 *     2006.05.26   LEM: Run at uri_raw time, as we don't need to see the URI
 *                       In this manner, we run before mod_access and $HTTP["remoteip"] directives work!
 *     2006.05.26   LEM: Clean config_cond cache of tests whose result we probably change.
 */


/* plugin config for all request/connections */

typedef struct {
	array *forwarder;
	array *headers;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;


/* context , used for restore remote ip */

typedef struct {
	sock_addr saved_remote_addr;
	buffer *saved_remote_addr_buf;
} handler_ctx;


static handler_ctx * handler_ctx_init(sock_addr oldaddr, buffer *oldaddr_buf) {
	handler_ctx * hctx;
	hctx = calloc(1, sizeof(*hctx));
	hctx->saved_remote_addr = oldaddr;
	hctx->saved_remote_addr_buf = oldaddr_buf;
	return hctx;
}

static void handler_ctx_free(handler_ctx *hctx) {
	free(hctx);
}

/* init the plugin data */
INIT_FUNC(mod_extforward_init) {
	plugin_data *p;
	p = calloc(1, sizeof(*p));
	return p;
}

/* destroy the plugin data */
FREE_FUNC(mod_extforward_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;

		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (!s) continue;

			array_free(s->forwarder);
			array_free(s->headers);

			free(s);
		}
		free(p->config_storage);
	}


	free(p);

	return HANDLER_GO_ON;
}

/* handle plugin config and check values */

SETDEFAULTS_FUNC(mod_extforward_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "extforward.forwarder",       NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ "extforward.headers",         NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 1 */
		{ NULL,                         NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->forwarder    = array_init();
		s->headers      = array_init();

		cv[0].destination = s->forwarder;
		cv[1].destination = s->headers;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_extforward_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(forwarder);
	PATCH(headers);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("extforward.forwarder"))) {
				PATCH(forwarder);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("extforward.headers"))) {
				PATCH(headers);
			}
		}
	}

	return 0;
}
#undef PATCH


static void put_string_into_array_len(array *ary, const char *str, int len)
{
	data_string *tempdata;
	if (len == 0)
		return;
	tempdata = data_string_init();
	buffer_copy_string_len(tempdata->value,str,len);
	array_insert_unique(ary,(data_unset *)tempdata);
}
/*
   extract a forward array from the environment
*/
static array *extract_forward_array(buffer *pbuffer)
{
	array *result = array_init();
	if (pbuffer->used > 0) {
		char *base, *curr;
		/* state variable, 0 means not in string, 1 means in string */
		int in_str = 0;
		for (base = pbuffer->ptr, curr = pbuffer->ptr; *curr; curr++) {
			if (in_str) {
				if ((*curr > '9' || *curr < '0') && *curr != '.' && *curr != ':') {
					/* found an separator , insert value into result array */
					put_string_into_array_len(result, base, curr - base);
					/* change state to not in string */
					in_str = 0;
				}
			} else {
				if (*curr >= '0' && *curr <= '9') {
					/* found leading char of an IP address, move base pointer and change state */
					base = curr;
					in_str = 1;
				}
			}
		}
		/* if breaking out while in str, we got to the end of string, so add it */
		if (in_str) {
			put_string_into_array_len(result, base, curr - base);
		}
	}
	return result;
}

#define IP_TRUSTED 1
#define IP_UNTRUSTED 0
/*
 * check whether ip is trusted, return 1 for trusted , 0 for untrusted
 */
static int is_proxy_trusted(const char *ipstr, plugin_data *p)
{
	data_string* allds = (data_string *)array_get_element(p->conf.forwarder, "all");

	if (allds) {
		if (strcasecmp(allds->value->ptr, "trust") == 0) {
			return IP_TRUSTED;
		} else {
			return IP_UNTRUSTED;
		}
	}

	return (data_string *)array_get_element(p->conf.forwarder, ipstr) ? IP_TRUSTED : IP_UNTRUSTED;
}

/*
 * Return char *ip of last address of proxy that is not trusted.
 * Do not accept "all" keyword here.
 */
static const char *last_not_in_array(array *a, plugin_data *p)
{
	array *forwarder = p->conf.forwarder;
	int i;

	for (i = a->used - 1; i >= 0; i--) {
		data_string *ds = (data_string *)a->data[i];
		const char *ip = ds->value->ptr;

		if (!array_get_element(forwarder, ip)) {
			return ip;
		}
	}
	return NULL;
}

static struct addrinfo *ipstr_to_sockaddr(server *srv, const char *host) {
	struct addrinfo hints, *res0;
	int result;

	memset(&hints, 0, sizeof(hints));
#ifndef AI_NUMERICSERV
	/**
	  * quoting $ man getaddrinfo
	  *
	  * NOTES
	  *        AI_ADDRCONFIG, AI_ALL, and AI_V4MAPPED are available since glibc 2.3.3.
	  *        AI_NUMERICSERV is available since glibc 2.3.4.
	  */
#define AI_NUMERICSERV 0
#endif
	hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

	errno = 0;
	result = getaddrinfo(host, NULL, &hints, &res0);

	if (result != 0) {
		log_error_write(srv, __FILE__, __LINE__, "SSSs(S)",
			"could not resolve hostname ", host, " because ", gai_strerror(result), strerror(errno));

		return NULL;
	} else if (res0 == 0) {
		log_error_write(srv, __FILE__, __LINE__, "SSS",
			"Problem in resolving hostname ", host, ": succeeded, but no information returned");
	}

	return res0;
}



static void clean_cond_cache(server *srv, connection *con) {
	config_cond_cache_reset_item(srv, con, COMP_HTTP_REMOTE_IP);
}

URIHANDLER_FUNC(mod_extforward_uri_handler) {
	plugin_data *p = p_d;
	data_string *forwarded = NULL;
#ifdef HAVE_IPV6
	char b2[INET6_ADDRSTRLEN + 1];
	struct addrinfo *addrlist = NULL;
#endif
	const char *dst_addr_str = NULL;
	array *forward_array = NULL;
	const char *real_remote_addr = NULL;
#ifdef HAVE_IPV6
#endif

	if (!con->request.headers) return HANDLER_GO_ON;

	mod_extforward_patch_connection(srv, con, p);

	if (con->conf.log_request_handling) {
		log_error_write(srv, __FILE__, __LINE__, "s",
			"-- mod_extforward_uri_handler called");
	}

	if (p->conf.headers->used) {
		data_string *ds;
		size_t k;

		for(k = 0; k < p->conf.headers->used; k++) {
			ds = (data_string *) p->conf.headers->data[k];
			if (NULL != (forwarded = (data_string*) array_get_element(con->request.headers, ds->value->ptr))) break;
		}
	} else {
		forwarded = (data_string *) array_get_element(con->request.headers,"X-Forwarded-For");
		if (NULL == forwarded) forwarded = (data_string *) array_get_element(con->request.headers,  "Forwarded-For");
	}

	if (NULL == forwarded) {
		if (con->conf.log_request_handling) {
			log_error_write(srv, __FILE__, __LINE__, "s", "no forward header found, skipping");
		}

		return HANDLER_GO_ON;
	}

#ifdef HAVE_IPV6
	dst_addr_str = inet_ntop(con->dst_addr.plain.sa_family,
		con->dst_addr.plain.sa_family == AF_INET6 ?
			(struct sockaddr *)&(con->dst_addr.ipv6.sin6_addr) :
			(struct sockaddr *)&(con->dst_addr.ipv4.sin_addr),
		b2, (sizeof b2) - 1);
#else
	dst_addr_str = inet_ntoa(con->dst_addr.ipv4.sin_addr);
#endif

	/* if the remote ip itself is not trusted, then do nothing */
	if (IP_UNTRUSTED == is_proxy_trusted(dst_addr_str, p)) {
		if (con->conf.log_request_handling) {
			log_error_write(srv, __FILE__, __LINE__, "s",
					"remote address is NOT a trusted proxy, skipping");
		}

		return HANDLER_GO_ON;
	}

	/* build forward_array from forwarded data_string */
	forward_array = extract_forward_array(forwarded->value);
	real_remote_addr = last_not_in_array(forward_array, p);

	if (real_remote_addr != NULL) { /* parsed */
		sock_addr sock;
		struct addrinfo *addrs_left;
		server_socket *srv_sock = con->srv_socket;
		data_string *forwarded_proto = (data_string *)array_get_element(con->request.headers, "X-Forwarded-Proto");

		if (forwarded_proto && !strcmp(forwarded_proto->value->ptr, "https")) {
			srv_sock->is_proxy_ssl = 1;
		} else {
			srv_sock->is_proxy_ssl = 0;
		}

		if (con->conf.log_request_handling) {
 			log_error_write(srv, __FILE__, __LINE__, "ss", "using address:", real_remote_addr);
		}
#ifdef HAVE_IPV6
		addrlist = ipstr_to_sockaddr(srv, real_remote_addr);
		sock.plain.sa_family = AF_UNSPEC;
		for (addrs_left = addrlist; addrs_left != NULL; addrs_left = addrs_left -> ai_next) {
			sock.plain.sa_family = addrs_left->ai_family;
			if (sock.plain.sa_family == AF_INET) {
				sock.ipv4.sin_addr = ((struct sockaddr_in*)addrs_left->ai_addr)->sin_addr;
				break;
			} else if (sock.plain.sa_family == AF_INET6) {
				sock.ipv6.sin6_addr = ((struct sockaddr_in6*)addrs_left->ai_addr)->sin6_addr;
				break;
			}
		}
#else
		UNUSED(addrs_left);
		sock.ipv4.sin_addr.s_addr = inet_addr(real_remote_addr);
		sock.plain.sa_family = (sock.ipv4.sin_addr.s_addr == 0xFFFFFFFF) ? AF_UNSPEC : AF_INET;
#endif
		if (sock.plain.sa_family != AF_UNSPEC) {
			/* we found the remote address, modify current connection and save the old address */
			if (con->plugin_ctx[p->id]) {
				log_error_write(srv, __FILE__, __LINE__, "s", 
						"patching an already patched connection!");
				handler_ctx_free(con->plugin_ctx[p->id]);
				con->plugin_ctx[p->id] = NULL;
			}
			/* save old address */
			con->plugin_ctx[p->id] = handler_ctx_init(con->dst_addr, con->dst_addr_buf);
			/* patch connection address */
			con->dst_addr = sock;
			con->dst_addr_buf = buffer_init();
			buffer_copy_string(con->dst_addr_buf, real_remote_addr);
		
			if (con->conf.log_request_handling) {
				log_error_write(srv, __FILE__, __LINE__, "ss",
						"patching con->dst_addr_buf for the accesslog:", real_remote_addr);
			}
			/* Now, clean the conf_cond cache, because we may have changed the results of tests */
			clean_cond_cache(srv, con);
		}
#ifdef HAVE_IPV6
		if (addrlist != NULL ) freeaddrinfo(addrlist);
#endif
	}
	array_free(forward_array);

	/* not found */
	return HANDLER_GO_ON;
}

CONNECTION_FUNC(mod_extforward_restore) {
	plugin_data *p = p_d;
	handler_ctx *hctx = con->plugin_ctx[p->id];

	if (!hctx) return HANDLER_GO_ON;
	
	con->dst_addr = hctx->saved_remote_addr;
	buffer_free(con->dst_addr_buf);

	con->dst_addr_buf = hctx->saved_remote_addr_buf;
	
	handler_ctx_free(hctx);

	con->plugin_ctx[p->id] = NULL;

	/* Now, clean the conf_cond cache, because we may have changed the results of tests */
	clean_cond_cache(srv, con);

	return HANDLER_GO_ON;
}


/* this function is called at dlopen() time and inits the callbacks */

int mod_extforward_plugin_init(plugin *p);
int mod_extforward_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("extforward");

	p->init        = mod_extforward_init;
	p->handle_uri_raw = mod_extforward_uri_handler;
	p->handle_request_done = mod_extforward_restore;
	p->connection_reset = mod_extforward_restore;
	p->set_defaults  = mod_extforward_set_defaults;
	p->cleanup     = mod_extforward_free;

	p->data        = NULL;

	return 0;
}

