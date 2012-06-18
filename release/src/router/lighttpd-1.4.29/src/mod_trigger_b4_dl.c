#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"
#include "response.h"
#include "inet_ntop_cache.h"

#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#if defined(HAVE_GDBM_H)
# include <gdbm.h>
#endif

#if defined(HAVE_PCRE_H)
# include <pcre.h>
#endif

#if defined(HAVE_MEMCACHE_H)
# include <memcache.h>
#endif

/**
 * this is a trigger_b4_dl for a lighttpd plugin
 *
 */

/* plugin config for all request/connections */

typedef struct {
	buffer *db_filename;

	buffer *trigger_url;
	buffer *download_url;
	buffer *deny_url;

	array  *mc_hosts;
	buffer *mc_namespace;
#if defined(HAVE_PCRE_H)
	pcre *trigger_regex;
	pcre *download_regex;
#endif
#if defined(HAVE_GDBM_H)
	GDBM_FILE db;
#endif

#if defined(HAVE_MEMCACHE_H)
	struct memcache *mc;
#endif

	unsigned short trigger_timeout;
	unsigned short debug;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	buffer *tmp_buf;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

/* init the plugin data */
INIT_FUNC(mod_trigger_b4_dl_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->tmp_buf = buffer_init();

	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_trigger_b4_dl_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (!s) continue;

			buffer_free(s->db_filename);
			buffer_free(s->download_url);
			buffer_free(s->trigger_url);
			buffer_free(s->deny_url);

			buffer_free(s->mc_namespace);
			array_free(s->mc_hosts);

#if defined(HAVE_PCRE_H)
			if (s->trigger_regex) pcre_free(s->trigger_regex);
			if (s->download_regex) pcre_free(s->download_regex);
#endif
#if defined(HAVE_GDBM_H)
			if (s->db) gdbm_close(s->db);
#endif
#if defined(HAVE_MEMCACHE_H)
			if (s->mc) mc_free(s->mc);
#endif

			free(s);
		}
		free(p->config_storage);
	}

	buffer_free(p->tmp_buf);

	free(p);

	return HANDLER_GO_ON;
}

/* handle plugin config and check values */

SETDEFAULTS_FUNC(mod_trigger_b4_dl_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;


	config_values_t cv[] = {
		{ "trigger-before-download.gdbm-filename",   NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ "trigger-before-download.trigger-url",     NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 1 */
		{ "trigger-before-download.download-url",    NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 2 */
		{ "trigger-before-download.deny-url",        NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 3 */
		{ "trigger-before-download.trigger-timeout", NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },        /* 4 */
		{ "trigger-before-download.memcache-hosts",  NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },        /* 5 */
		{ "trigger-before-download.memcache-namespace", NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },    /* 6 */
		{ "trigger-before-download.debug",           NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION },      /* 7 */
		{ NULL,                        NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;
#if defined(HAVE_PCRE_H)
		const char *errptr;
		int erroff;
#endif

		s = calloc(1, sizeof(plugin_config));
		s->db_filename    = buffer_init();
		s->download_url   = buffer_init();
		s->trigger_url    = buffer_init();
		s->deny_url       = buffer_init();
		s->mc_hosts       = array_init();
		s->mc_namespace   = buffer_init();

		cv[0].destination = s->db_filename;
		cv[1].destination = s->trigger_url;
		cv[2].destination = s->download_url;
		cv[3].destination = s->deny_url;
		cv[4].destination = &(s->trigger_timeout);
		cv[5].destination = s->mc_hosts;
		cv[6].destination = s->mc_namespace;
		cv[7].destination = &(s->debug);

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv)) {
			return HANDLER_ERROR;
		}
#if defined(HAVE_GDBM_H)
		if (!buffer_is_empty(s->db_filename)) {
			if (NULL == (s->db = gdbm_open(s->db_filename->ptr, 4096, GDBM_WRCREAT | GDBM_NOLOCK, S_IRUSR | S_IWUSR, 0))) {
				log_error_write(srv, __FILE__, __LINE__, "s",
						"gdbm-open failed");
				return HANDLER_ERROR;
			}
#ifdef FD_CLOEXEC
			fcntl(gdbm_fdesc(s->db), F_SETFD, FD_CLOEXEC);
#endif
		}
#endif
#if defined(HAVE_PCRE_H)
		if (!buffer_is_empty(s->download_url)) {
			if (NULL == (s->download_regex = pcre_compile(s->download_url->ptr,
								      0, &errptr, &erroff, NULL))) {

				log_error_write(srv, __FILE__, __LINE__, "sbss",
						"compiling regex for download-url failed:",
						s->download_url, "pos:", erroff);
				return HANDLER_ERROR;
			}
		}

		if (!buffer_is_empty(s->trigger_url)) {
			if (NULL == (s->trigger_regex = pcre_compile(s->trigger_url->ptr,
								     0, &errptr, &erroff, NULL))) {

				log_error_write(srv, __FILE__, __LINE__, "sbss",
						"compiling regex for trigger-url failed:",
						s->trigger_url, "pos:", erroff);

				return HANDLER_ERROR;
			}
		}
#endif

		if (s->mc_hosts->used) {
#if defined(HAVE_MEMCACHE_H)
			size_t k;
			s->mc = mc_new();

			for (k = 0; k < s->mc_hosts->used; k++) {
				data_string *ds = (data_string *)s->mc_hosts->data[k];

				if (0 != mc_server_add4(s->mc, ds->value->ptr)) {
					log_error_write(srv, __FILE__, __LINE__, "sb",
							"connection to host failed:",
							ds->value);

					return HANDLER_ERROR;
				}
			}
#else
			log_error_write(srv, __FILE__, __LINE__, "s",
					"memcache support is not compiled in but trigger-before-download.memcache-hosts is set, aborting");
			return HANDLER_ERROR;
#endif
		}


#if (!defined(HAVE_GDBM_H) && !defined(HAVE_MEMCACHE_H)) || !defined(HAVE_PCRE_H)
		log_error_write(srv, __FILE__, __LINE__, "s",
				"(either gdbm or libmemcache) and pcre are require, but were not found, aborting");
		return HANDLER_ERROR;
#endif
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_trigger_b4_dl_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

#if defined(HAVE_GDBM)
	PATCH(db);
#endif
#if defined(HAVE_PCRE_H)
	PATCH(download_regex);
	PATCH(trigger_regex);
#endif
	PATCH(trigger_timeout);
	PATCH(deny_url);
	PATCH(mc_namespace);
	PATCH(debug);
#if defined(HAVE_MEMCACHE_H)
	PATCH(mc);
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

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("trigger-before-download.download-url"))) {
#if defined(HAVE_PCRE_H)
				PATCH(download_regex);
#endif
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("trigger-before-download.trigger-url"))) {
# if defined(HAVE_PCRE_H)
				PATCH(trigger_regex);
# endif
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("trigger-before-download.gdbm-filename"))) {
#if defined(HAVE_GDBM_H)
				PATCH(db);
#endif
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("trigger-before-download.trigger-timeout"))) {
				PATCH(trigger_timeout);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("trigger-before-download.debug"))) {
				PATCH(debug);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("trigger-before-download.deny-url"))) {
				PATCH(deny_url);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("trigger-before-download.memcache-namespace"))) {
				PATCH(mc_namespace);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("trigger-before-download.memcache-hosts"))) {
#if defined(HAVE_MEMCACHE_H)
				PATCH(mc);
#endif
			}
		}
	}

	return 0;
}
#undef PATCH

URIHANDLER_FUNC(mod_trigger_b4_dl_uri_handler) {
	plugin_data *p = p_d;
	const char *remote_ip;
	data_string *ds;

#if defined(HAVE_PCRE_H)
	int n;
# define N 10
	int ovec[N * 3];

	if (con->mode != DIRECT) return HANDLER_GO_ON;

	if (con->uri.path->used == 0) return HANDLER_GO_ON;

	mod_trigger_b4_dl_patch_connection(srv, con, p);

	if (!p->conf.trigger_regex || !p->conf.download_regex) return HANDLER_GO_ON;

# if !defined(HAVE_GDBM_H) && !defined(HAVE_MEMCACHE_H)
	return HANDLER_GO_ON;
# elif defined(HAVE_GDBM_H) && defined(HAVE_MEMCACHE_H)
	if (!p->conf.db && !p->conf.mc) return HANDLER_GO_ON;
	if (p->conf.db && p->conf.mc) {
		/* can't decide which one */

		return HANDLER_GO_ON;
	}
# elif defined(HAVE_GDBM_H)
	if (!p->conf.db) return HANDLER_GO_ON;
# else
	if (!p->conf.mc) return HANDLER_GO_ON;
# endif

	if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "X-Forwarded-For"))) {
		/* X-Forwarded-For contains the ip behind the proxy */

		remote_ip = ds->value->ptr;

		/* memcache can't handle spaces */
	} else {
		remote_ip = inet_ntop_cache_get_ip(srv, &(con->dst_addr));
	}

	if (p->conf.debug) {
		log_error_write(srv, __FILE__, __LINE__, "ss", "(debug) remote-ip:", remote_ip);
	}

	/* check if URL is a trigger -> insert IP into DB */
	if ((n = pcre_exec(p->conf.trigger_regex, NULL, con->uri.path->ptr, con->uri.path->used - 1, 0, 0, ovec, 3 * N)) < 0) {
		if (n != PCRE_ERROR_NOMATCH) {
			log_error_write(srv, __FILE__, __LINE__, "sd",
					"execution error while matching:", n);

			return HANDLER_ERROR;
		}
	} else {
# if defined(HAVE_GDBM_H)
		if (p->conf.db) {
			/* the trigger matched */
			datum key, val;

			key.dptr = (char *)remote_ip;
			key.dsize = strlen(remote_ip);

			val.dptr = (char *)&(srv->cur_ts);
			val.dsize = sizeof(srv->cur_ts);

			if (0 != gdbm_store(p->conf.db, key, val, GDBM_REPLACE)) {
				log_error_write(srv, __FILE__, __LINE__, "s",
						"insert failed");
			}
		}
# endif
# if defined(HAVE_MEMCACHE_H)
		if (p->conf.mc) {
			size_t i;
			buffer_copy_string_buffer(p->tmp_buf, p->conf.mc_namespace);
			buffer_append_string(p->tmp_buf, remote_ip);

			for (i = 0; i < p->tmp_buf->used - 1; i++) {
				if (p->tmp_buf->ptr[i] == ' ') p->tmp_buf->ptr[i] = '-';
			}

			if (p->conf.debug) {
				log_error_write(srv, __FILE__, __LINE__, "sb", "(debug) triggered IP:", p->tmp_buf);
			}

			if (0 != mc_set(p->conf.mc,
					CONST_BUF_LEN(p->tmp_buf),
					(char *)&(srv->cur_ts), sizeof(srv->cur_ts),
					p->conf.trigger_timeout, 0)) {
				log_error_write(srv, __FILE__, __LINE__, "s",
						"insert failed");
			}
		}
# endif
	}

	/* check if URL is a download -> check IP in DB, update timestamp */
	if ((n = pcre_exec(p->conf.download_regex, NULL, con->uri.path->ptr, con->uri.path->used - 1, 0, 0, ovec, 3 * N)) < 0) {
		if (n != PCRE_ERROR_NOMATCH) {
			log_error_write(srv, __FILE__, __LINE__, "sd",
					"execution error while matching: ", n);
			return HANDLER_ERROR;
		}
	} else {
		/* the download uri matched */
# if defined(HAVE_GDBM_H)
		if (p->conf.db) {
			datum key, val;
			time_t last_hit;

			key.dptr = (char *)remote_ip;
			key.dsize = strlen(remote_ip);

			val = gdbm_fetch(p->conf.db, key);

			if (val.dptr == NULL) {
				/* not found, redirect */

				response_header_insert(srv, con, CONST_STR_LEN("Location"), CONST_BUF_LEN(p->conf.deny_url));
				con->http_status = 307;
				con->file_finished = 1;

				return HANDLER_FINISHED;
			}

			last_hit = *(time_t *)(val.dptr);

			free(val.dptr);

			if (srv->cur_ts - last_hit > p->conf.trigger_timeout) {
				/* found, but timeout, redirect */

				response_header_insert(srv, con, CONST_STR_LEN("Location"), CONST_BUF_LEN(p->conf.deny_url));
				con->http_status = 307;
				con->file_finished = 1;

				if (p->conf.db) {
					if (0 != gdbm_delete(p->conf.db, key)) {
						log_error_write(srv, __FILE__, __LINE__, "s",
								"delete failed");
					}
				}

				return HANDLER_FINISHED;
			}

			val.dptr = (char *)&(srv->cur_ts);
			val.dsize = sizeof(srv->cur_ts);

			if (0 != gdbm_store(p->conf.db, key, val, GDBM_REPLACE)) {
				log_error_write(srv, __FILE__, __LINE__, "s",
						"insert failed");
			}
		}
# endif

# if defined(HAVE_MEMCACHE_H)
		if (p->conf.mc) {
			void *r;
			size_t i;

			buffer_copy_string_buffer(p->tmp_buf, p->conf.mc_namespace);
			buffer_append_string(p->tmp_buf, remote_ip);

			for (i = 0; i < p->tmp_buf->used - 1; i++) {
				if (p->tmp_buf->ptr[i] == ' ') p->tmp_buf->ptr[i] = '-';
			}

			if (p->conf.debug) {
				log_error_write(srv, __FILE__, __LINE__, "sb", "(debug) checking IP:", p->tmp_buf);
			}

			/**
			 *
			 * memcached is do expiration for us, as long as we can fetch it every thing is ok
			 * and the timestamp is updated
			 *
			 */
			if (NULL == (r = mc_aget(p->conf.mc,
						 CONST_BUF_LEN(p->tmp_buf)
						 ))) {

				response_header_insert(srv, con, CONST_STR_LEN("Location"), CONST_BUF_LEN(p->conf.deny_url));

				con->http_status = 307;
				con->file_finished = 1;

				return HANDLER_FINISHED;
			}

			free(r);

			/* set a new timeout */
			if (0 != mc_set(p->conf.mc,
					CONST_BUF_LEN(p->tmp_buf),
					(char *)&(srv->cur_ts), sizeof(srv->cur_ts),
					p->conf.trigger_timeout, 0)) {
				log_error_write(srv, __FILE__, __LINE__, "s",
						"insert failed");
			}
		}
# endif
	}

#else
	UNUSED(srv);
	UNUSED(con);
	UNUSED(p_d);
#endif

	return HANDLER_GO_ON;
}

#if defined(HAVE_GDBM_H)
TRIGGER_FUNC(mod_trigger_b4_dl_handle_trigger) {
	plugin_data *p = p_d;
	size_t i;

	/* check DB each minute */
	if (srv->cur_ts % 60 != 0) return HANDLER_GO_ON;

	/* cleanup */
	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s = p->config_storage[i];
		datum key, val, okey;

		if (!s->db) continue;

		okey.dptr = NULL;

		/* according to the manual this loop + delete does delete all entries on its way
		 *
		 * we don't care as the next round will remove them. We don't have to perfect here.
		 */
		for (key = gdbm_firstkey(s->db); key.dptr; key = gdbm_nextkey(s->db, okey)) {
			time_t last_hit;
			if (okey.dptr) {
				free(okey.dptr);
				okey.dptr = NULL;
			}

			val = gdbm_fetch(s->db, key);

			last_hit = *(time_t *)(val.dptr);

			free(val.dptr);

			if (srv->cur_ts - last_hit > s->trigger_timeout) {
				gdbm_delete(s->db, key);
			}

			okey = key;
		}
		if (okey.dptr) free(okey.dptr);

		/* reorg once a day */
		if ((srv->cur_ts % (60 * 60 * 24) != 0)) gdbm_reorganize(s->db);
	}
	return HANDLER_GO_ON;
}
#endif

/* this function is called at dlopen() time and inits the callbacks */

int mod_trigger_b4_dl_plugin_init(plugin *p);
int mod_trigger_b4_dl_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("trigger_b4_dl");

	p->init        = mod_trigger_b4_dl_init;
	p->handle_uri_clean  = mod_trigger_b4_dl_uri_handler;
	p->set_defaults  = mod_trigger_b4_dl_set_defaults;
#if defined(HAVE_GDBM_H)
	p->handle_trigger  = mod_trigger_b4_dl_handle_trigger;
#endif
	p->cleanup     = mod_trigger_b4_dl_free;

	p->data        = NULL;

	return 0;
}
