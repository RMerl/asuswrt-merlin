#include "mod_cml.h"
#include "mod_cml_funcs.h"
#include "log.h"
#include "stream.h"

#include "stat_cache.h"

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#define HASHLEN 16
typedef unsigned char HASH[HASHLEN];
#define HASHHEXLEN 32
typedef char HASHHEX[HASHHEXLEN+1];
#ifdef USE_OPENSSL
#define IN const
#else
#define IN
#endif
#define OUT

#ifdef HAVE_LUA_H

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

typedef struct {
	stream st;
	int done;
} readme;

static const char * load_file(lua_State *L, void *data, size_t *size) {
	readme *rm = data;

	UNUSED(L);

	if (rm->done) return 0;

	*size = rm->st.size;
	rm->done = 1;
	return rm->st.start;
}

static int lua_to_c_get_string(lua_State *L, const char *varname, buffer *b) {
	int curelem;

	lua_pushstring(L, varname);

	curelem = lua_gettop(L);
	lua_gettable(L, LUA_GLOBALSINDEX);

	/* it should be a table */
	if (!lua_isstring(L, curelem)) {
		lua_settop(L, curelem - 1);

		return -1;
	}

	buffer_copy_string(b, lua_tostring(L, curelem));

	lua_pop(L, 1);

	force_assert(curelem - 1 == lua_gettop(L));

	return 0;
}

static int lua_to_c_is_table(lua_State *L, const char *varname) {
	int curelem;

	lua_pushstring(L, varname);

	curelem = lua_gettop(L);
	lua_gettable(L, LUA_GLOBALSINDEX);

	/* it should be a table */
	if (!lua_istable(L, curelem)) {
		lua_settop(L, curelem - 1);

		return 0;
	}

	lua_settop(L, curelem - 1);

	force_assert(curelem - 1 == lua_gettop(L));

	return 1;
}

static int c_to_lua_push(lua_State *L, int tbl, const char *key, size_t key_len, const char *val, size_t val_len) {
	lua_pushlstring(L, key, key_len);
	lua_pushlstring(L, val, val_len);
	lua_settable(L, tbl);

	return 0;
}


static int cache_export_get_params(lua_State *L, int tbl, buffer *qrystr) {
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
				/* we need at least a = since the last & */

				/* terminate the value */
				qrystr->ptr[i] = '\0';

				c_to_lua_push(L, tbl,
					key, strlen(key),
					val, strlen(val));
			}

			key = qrystr->ptr + i + 1;
			val = NULL;
			is_key = 1;
			break;
		}
	}

	return 0;
}
#if 0
int cache_export_cookie_params(server *srv, connection *con, plugin_data *p) {
	data_unset *d;

	UNUSED(srv);

	if (NULL != (d = array_get_element(con->request.headers, "Cookie"))) {
		data_string *ds = (data_string *)d;
		size_t key = 0, value = 0;
		size_t is_key = 1, is_sid = 0;
		size_t i;

		/* found COOKIE */
		if (!DATA_IS_STRING(d)) return -1;
		if (ds->value->used == 0) return -1;

		if (ds->value->ptr[0] == '\0' ||
		    ds->value->ptr[0] == '=' ||
		    ds->value->ptr[0] == ';') return -1;

		buffer_reset(p->session_id);
		for (i = 0; i < ds->value->used; i++) {
			switch(ds->value->ptr[i]) {
			case '=':
				if (is_key) {
					if (0 == strncmp(ds->value->ptr + key, "PHPSESSID", i - key)) {
						/* found PHP-session-id-key */
						is_sid = 1;
					}
					value = i + 1;

					is_key = 0;
				}

				break;
			case ';':
				if (is_sid) {
					buffer_copy_string_len(p->session_id, ds->value->ptr + value, i - value);
				}

				is_sid = 0;
				key = i + 1;
				value = 0;
				is_key = 1;
				break;
			case ' ':
				if (is_key == 1 && key == i) key = i + 1;
				if (is_key == 0 && value == i) value = i + 1;
				break;
			case '\0':
				if (is_sid) {
					buffer_copy_string_len(p->session_id, ds->value->ptr + value, i - value);
				}
				/* fin */
				break;
			}
		}
	}

	return 0;
}
#endif

int cache_parse_lua(server *srv, connection *con, plugin_data *p, buffer *fn) {
	lua_State *L;
	readme rm;
	int ret = -1;
	buffer *b;
	int header_tbl = 0;

	rm.done = 0;
	if (-1 == stream_open(&rm.st, fn)) {
		log_error_write(srv, __FILE__, __LINE__, "sbss",
				"opening lua cml file ", fn, "failed:", strerror(errno));
		return -1;
	}

	b = buffer_init();
	/* push the lua file to the interpreter and see what happends */
	L = luaL_newstate();
	luaL_openlibs(L);

	/* register functions */
	lua_register(L, "md5", f_crypto_md5);
	lua_register(L, "file_mtime", f_file_mtime);
	lua_register(L, "file_isreg", f_file_isreg);
	lua_register(L, "file_isdir", f_file_isreg);
	lua_register(L, "dir_files", f_dir_files);

#ifdef HAVE_MEMCACHE_H
	lua_pushliteral(L, "memcache_get_long");
	lua_pushlightuserdata(L, p->conf.mc);
	lua_pushcclosure(L, f_memcache_get_long, 1);
	lua_settable(L, LUA_GLOBALSINDEX);

	lua_pushliteral(L, "memcache_get_string");
	lua_pushlightuserdata(L, p->conf.mc);
	lua_pushcclosure(L, f_memcache_get_string, 1);
	lua_settable(L, LUA_GLOBALSINDEX);

	lua_pushliteral(L, "memcache_exists");
	lua_pushlightuserdata(L, p->conf.mc);
	lua_pushcclosure(L, f_memcache_exists, 1);
	lua_settable(L, LUA_GLOBALSINDEX);
#endif
	/* register CGI environment */
	lua_pushliteral(L, "request");
	lua_newtable(L);
	lua_settable(L, LUA_GLOBALSINDEX);

	lua_pushliteral(L, "request");
	header_tbl = lua_gettop(L);
	lua_gettable(L, LUA_GLOBALSINDEX);

	c_to_lua_push(L, header_tbl, CONST_STR_LEN("REQUEST_URI"), CONST_BUF_LEN(con->request.orig_uri));
	c_to_lua_push(L, header_tbl, CONST_STR_LEN("SCRIPT_NAME"), CONST_BUF_LEN(con->uri.path));
	c_to_lua_push(L, header_tbl, CONST_STR_LEN("SCRIPT_FILENAME"), CONST_BUF_LEN(con->physical.path));
	c_to_lua_push(L, header_tbl, CONST_STR_LEN("DOCUMENT_ROOT"), CONST_BUF_LEN(con->physical.doc_root));
	if (!buffer_string_is_empty(con->request.pathinfo)) {
		c_to_lua_push(L, header_tbl, CONST_STR_LEN("PATH_INFO"), CONST_BUF_LEN(con->request.pathinfo));
	}

	c_to_lua_push(L, header_tbl, CONST_STR_LEN("CWD"), CONST_BUF_LEN(p->basedir));
	c_to_lua_push(L, header_tbl, CONST_STR_LEN("BASEURL"), CONST_BUF_LEN(p->baseurl));

	/* register GET parameter */
	lua_pushliteral(L, "get");
	lua_newtable(L);
	lua_settable(L, LUA_GLOBALSINDEX);

	lua_pushliteral(L, "get");
	header_tbl = lua_gettop(L);
	lua_gettable(L, LUA_GLOBALSINDEX);

	buffer_copy_buffer(b, con->uri.query);
	cache_export_get_params(L, header_tbl, b);
	buffer_reset(b);

	/* 2 default constants */
	lua_pushliteral(L, "CACHE_HIT");
	lua_pushnumber(L, 0);
	lua_settable(L, LUA_GLOBALSINDEX);

	lua_pushliteral(L, "CACHE_MISS");
	lua_pushnumber(L, 1);
	lua_settable(L, LUA_GLOBALSINDEX);

	/* load lua program */
	if (lua_load(L, load_file, &rm, fn->ptr) || lua_pcall(L,0,1,0)) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				lua_tostring(L,-1));

		goto error;
	}

	/* get return value */
	ret = (int)lua_tonumber(L, -1);
	lua_pop(L, 1);

	/* fetch the data from lua */
	lua_to_c_get_string(L, "trigger_handler", p->trigger_handler);

	if (0 == lua_to_c_get_string(L, "output_contenttype", b)) {
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_BUF_LEN(b));
	}

	if (ret == 0) {
		/* up to now it is a cache-hit, check if all files exist */

		int curelem;
		time_t mtime = 0;

		if (!lua_to_c_is_table(L, "output_include")) {
			log_error_write(srv, __FILE__, __LINE__, "s",
				"output_include is missing or not a table");
			ret = -1;

			goto error;
		}

		lua_pushstring(L, "output_include");

		curelem = lua_gettop(L);
		lua_gettable(L, LUA_GLOBALSINDEX);

		/* HOW-TO build a etag ?
		 * as we don't just have one file we have to take the stat()
		 * from all base files, merge them and build the etag from
		 * it later.
		 *
		 * The mtime of the content is the mtime of the freshest base file
		 *
		 * */

		lua_pushnil(L);  /* first key */
		while (lua_next(L, curelem) != 0) {
			stat_cache_entry *sce = NULL;
			/* key' is at index -2 and value' at index -1 */

			if (lua_isstring(L, -1)) {
				const char *s = lua_tostring(L, -1);

				/* the file is relative, make it absolute */
				if (s[0] != '/') {
					buffer_copy_buffer(b, p->basedir);
					buffer_append_string(b, lua_tostring(L, -1));
				} else {
					buffer_copy_string(b, lua_tostring(L, -1));
				}

				if (HANDLER_ERROR == stat_cache_get_entry(srv, con, b, &sce)) {
					/* stat failed */

					switch(errno) {
					case ENOENT:
						/* a file is missing, call the handler to generate it */
						if (!buffer_string_is_empty(p->trigger_handler)) {
							ret = 1; /* cache-miss */

							log_error_write(srv, __FILE__, __LINE__, "s",
									"a file is missing, calling handler");

							break;
						} else {
							/* handler not set -> 500 */
							ret = -1;

							log_error_write(srv, __FILE__, __LINE__, "s",
									"a file missing and no handler set");

							break;
						}
						break;
					default:
						break;
					}
				} else {
					chunkqueue_append_file(con->write_queue, b, 0, sce->st.st_size);
					if (sce->st.st_mtime > mtime) mtime = sce->st.st_mtime;
				}
			} else {
				/* not a string */
				ret = -1;
				log_error_write(srv, __FILE__, __LINE__, "s",
						"not a string");
				break;
			}

			lua_pop(L, 1);  /* removes value'; keeps key' for next iteration */
		}

		lua_settop(L, curelem - 1);

		if (ret == 0) {
			data_string *ds;
			char timebuf[sizeof("Sat, 23 Jul 2005 21:20:01 GMT")];

			con->file_finished = 1;

			ds = (data_string *)array_get_element(con->response.headers, "Last-Modified");
			if (0 == mtime) mtime = time(NULL); /* default last-modified to now */

			/* no Last-Modified specified */
			if (NULL == ds) {

				strftime(timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&mtime));

				response_header_overwrite(srv, con, CONST_STR_LEN("Last-Modified"), timebuf, sizeof(timebuf) - 1);
				ds = (data_string *)array_get_element(con->response.headers, "Last-Modified");
				force_assert(NULL != ds);
			}

			if (HANDLER_FINISHED == http_response_handle_cachable(srv, con, ds->value)) {
				/* ok, the client already has our content,
				 * no need to send it again */

				chunkqueue_reset(con->write_queue);
				ret = 0; /* cache-hit */
			}
		} else {
			chunkqueue_reset(con->write_queue);
		}
	}

	if (ret == 1 && !buffer_string_is_empty(p->trigger_handler)) {
		/* cache-miss */
		buffer_copy_buffer(con->uri.path, p->baseurl);
		buffer_append_string_buffer(con->uri.path, p->trigger_handler);

		buffer_copy_buffer(con->physical.path, p->basedir);
		buffer_append_string_buffer(con->physical.path, p->trigger_handler);

		chunkqueue_reset(con->write_queue);
	}

error:
	lua_close(L);

	stream_close(&rm.st);
	buffer_free(b);

	return ret /* cache-error */;
}
#else
int cache_parse_lua(server *srv, connection *con, plugin_data *p, buffer *fn) {
	UNUSED(srv);
	UNUSED(con);
	UNUSED(p);
	UNUSED(fn);
	/* error */
	return -1;
}
#endif
