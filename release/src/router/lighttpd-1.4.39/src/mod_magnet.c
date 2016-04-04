#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include "mod_magnet_cache.h"
#include "response.h"
#include "stat_cache.h"
#include "status_counter.h"
#include "etag.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>

#ifdef HAVE_LUA_H
#include <lua.h>
#include <lauxlib.h>

#define MAGNET_CONFIG_RAW_URL       "magnet.attract-raw-url-to"
#define MAGNET_CONFIG_PHYSICAL_PATH "magnet.attract-physical-path-to"
#define MAGNET_RESTART_REQUEST      99

/* plugin config for all request/connections */

static jmp_buf exceptionjmp;

typedef struct {
	array *url_raw;
	array *physical_path;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	script_cache *cache;

	buffer *encode_buf;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

/* init the plugin data */
INIT_FUNC(mod_magnet_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->cache = script_cache_init();
	p->encode_buf = buffer_init();

	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_magnet_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;

		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (NULL == s) continue;

			array_free(s->url_raw);
			array_free(s->physical_path);

			free(s);
		}
		free(p->config_storage);
	}

	script_cache_free(p->cache);
	buffer_free(p->encode_buf);

	free(p);

	return HANDLER_GO_ON;
}

/* handle plugin config and check values */

SETDEFAULTS_FUNC(mod_magnet_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ MAGNET_CONFIG_RAW_URL,       NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ MAGNET_CONFIG_PHYSICAL_PATH, NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 1 */
		{ NULL,                           NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(plugin_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->url_raw  = array_init();
		s->physical_path = array_init();

		cv[0].destination = s->url_raw;
		cv[1].destination = s->physical_path;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_magnet_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(url_raw);
	PATCH(physical_path);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN(MAGNET_CONFIG_RAW_URL))) {
				PATCH(url_raw);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN(MAGNET_CONFIG_PHYSICAL_PATH))) {
				PATCH(physical_path);
			}
		}
	}

	return 0;
}
#undef PATCH

/* See http://lua-users.org/wiki/GeneralizedPairsAndIpairs for implementation details. */

/* Override the default pairs() function to allow us to use a __pairs metakey */
static int magnet_pairs(lua_State *L) {
	luaL_checkany(L, 1);

	if (luaL_getmetafield(L, 1, "__pairs")) {
		lua_insert(L, 1);
		lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
		return lua_gettop(L);
	} else {
		lua_pushvalue(L, lua_upvalueindex(1));
		lua_insert(L, 1);
		lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
		return lua_gettop(L);
	}
}

/* Define a function that will iterate over an array* (in upval 1) using current position (upval 2) */
static int magnet_array_next(lua_State *L) {
	data_unset *du;
	data_string *ds;
	data_integer *di;

	size_t pos = lua_tointeger(L, lua_upvalueindex(1));
	array *a = lua_touserdata(L, lua_upvalueindex(2));

	lua_settop(L, 0);

	if (pos >= a->used) return 0;
	if (NULL != (du = a->data[pos])) {
		lua_pushlstring(L, CONST_BUF_LEN(du->key));
		switch (du->type) {
			case TYPE_STRING:
				ds = (data_string *)du;
				if (!buffer_is_empty(ds->value)) {
					lua_pushlstring(L, CONST_BUF_LEN(ds->value));
				} else {
					lua_pushnil(L);
				}
				break;
			case TYPE_COUNT:
			case TYPE_INTEGER:
				di = (data_integer *)du;
				lua_pushinteger(L, di->value);
				break;
			default:
				lua_pushnil(L);
				break;
		}

		/* Update our positional upval to reflect our new current position */
		pos++;
		lua_pushinteger(L, pos);
		lua_replace(L, lua_upvalueindex(1));

		/* Returning 2 items on the stack (key, value) */
		return 2;
	}
	return 0;
}

/* Create the closure necessary to iterate over the array *a with the above function */
static int magnet_array_pairs(lua_State *L, array *a) {
	lua_pushinteger(L, 0); /* Push our current pos (the start) into upval 1 */
	lua_pushlightuserdata(L, a); /* Push our array *a into upval 2 */
	lua_pushcclosure(L, magnet_array_next, 2); /* Push our new closure with 2 upvals */
	return 1;
}

static int magnet_print(lua_State *L) {
	const char *s = luaL_checkstring(L, 1);
	server *srv;

	lua_pushstring(L, "lighty.srv");
	lua_gettable(L, LUA_REGISTRYINDEX);
	srv = lua_touserdata(L, -1);
	lua_pop(L, 1);

	log_error_write(srv, __FILE__, __LINE__, "ss",
			"(lua-print)", s);

	return 0;
}

static int magnet_stat(lua_State *L) {
	const char *s = luaL_checkstring(L, 1);
	server *srv;
	connection *con;
	buffer *sb;
	stat_cache_entry *sce = NULL;
	handler_t res;

	lua_pushstring(L, "lighty.srv");
	lua_gettable(L, LUA_REGISTRYINDEX);
	srv = lua_touserdata(L, -1);
	lua_pop(L, 1);

	lua_pushstring(L, "lighty.con");
	lua_gettable(L, LUA_REGISTRYINDEX);
	con = lua_touserdata(L, -1);
	lua_pop(L, 1);

	sb = buffer_init_string(s);
	res = stat_cache_get_entry(srv, con, sb, &sce);
	buffer_free(sb);

	if (HANDLER_GO_ON != res) {
		lua_pushnil(L);
		return 1;
	}

	lua_newtable(L);

	lua_pushboolean(L, S_ISREG(sce->st.st_mode));
	lua_setfield(L, -2, "is_file");
	
	lua_pushboolean(L, S_ISDIR(sce->st.st_mode));
	lua_setfield(L, -2, "is_dir");

	lua_pushboolean(L, S_ISCHR(sce->st.st_mode));
	lua_setfield(L, -2, "is_char");

	lua_pushboolean(L, S_ISBLK(sce->st.st_mode));
	lua_setfield(L, -2, "is_block");

	lua_pushboolean(L, S_ISSOCK(sce->st.st_mode));
	lua_setfield(L, -2, "is_socket");

	lua_pushboolean(L, S_ISLNK(sce->st.st_mode));
	lua_setfield(L, -2, "is_link");

	lua_pushboolean(L, S_ISFIFO(sce->st.st_mode));
	lua_setfield(L, -2, "is_fifo");

	lua_pushinteger(L, sce->st.st_mtime);
	lua_setfield(L, -2, "st_mtime");

	lua_pushinteger(L, sce->st.st_ctime);
	lua_setfield(L, -2, "st_ctime");

	lua_pushinteger(L, sce->st.st_atime);
	lua_setfield(L, -2, "st_atime");

	lua_pushinteger(L, sce->st.st_uid);
	lua_setfield(L, -2, "st_uid");

	lua_pushinteger(L, sce->st.st_gid);
	lua_setfield(L, -2, "st_gid");

	lua_pushinteger(L, sce->st.st_size);
	lua_setfield(L, -2, "st_size");

	lua_pushinteger(L, sce->st.st_ino);
	lua_setfield(L, -2, "st_ino");


	if (!buffer_string_is_empty(sce->etag)) {
		/* we have to mutate the etag */
		buffer *b = buffer_init();
		etag_mutate(b, sce->etag);

		lua_pushlstring(L, CONST_BUF_LEN(b));
		buffer_free(b);
	} else {
		lua_pushnil(L);
	}
	lua_setfield(L, -2, "etag");

	if (!buffer_string_is_empty(sce->content_type)) {
		lua_pushlstring(L, CONST_BUF_LEN(sce->content_type));
	} else {
		lua_pushnil(L);
	}
	lua_setfield(L, -2, "content-type");

	return 1;
}


static int magnet_atpanic(lua_State *L) {
	const char *s = luaL_checkstring(L, 1);
	server *srv;

	lua_pushstring(L, "lighty.srv");
	lua_gettable(L, LUA_REGISTRYINDEX);
	srv = lua_touserdata(L, -1);
	lua_pop(L, 1);

	log_error_write(srv, __FILE__, __LINE__, "ss",
			"(lua-atpanic)", s);

	longjmp(exceptionjmp, 1);
}

static int magnet_reqhdr_get(lua_State *L) {
	connection *con;
	data_string *ds;

	const char *key = luaL_checkstring(L, 2);

	lua_pushstring(L, "lighty.con");
	lua_gettable(L, LUA_REGISTRYINDEX);
	con = lua_touserdata(L, -1);
	lua_pop(L, 1);

	if (NULL != (ds = (data_string *)array_get_element(con->request.headers, key))) {
		if (!buffer_is_empty(ds->value)) {
			lua_pushlstring(L, CONST_BUF_LEN(ds->value));
		} else {
			lua_pushnil(L);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int magnet_reqhdr_pairs(lua_State *L) {
	connection *con;

	lua_pushstring(L, "lighty.con");
	lua_gettable(L, LUA_REGISTRYINDEX);
	con = lua_touserdata(L, -1);
	lua_pop(L, 1);

	return magnet_array_pairs(L, con->request.headers);
}

static int magnet_status_get(lua_State *L) {
	data_integer *di;
	server *srv;
	size_t key_len = 0;

	const char *key = luaL_checklstring(L, 2, &key_len);

	lua_pushstring(L, "lighty.srv");
	lua_gettable(L, LUA_REGISTRYINDEX);
	srv = lua_touserdata(L, -1);
	lua_pop(L, 1);

	di = status_counter_get_counter(srv, key, key_len);

	lua_pushnumber(L, (double)di->value);

	return 1;
}

static int magnet_status_set(lua_State *L) {
	size_t key_len = 0;
	server *srv;

	const char *key = luaL_checklstring(L, 2, &key_len);
	int counter = luaL_checkint(L, 3);

	lua_pushstring(L, "lighty.srv");
	lua_gettable(L, LUA_REGISTRYINDEX);
	srv = lua_touserdata(L, -1);
	lua_pop(L, 1);

	status_counter_set(srv, key, key_len, counter);

	return 0;
}

static int magnet_status_pairs(lua_State *L) {
	server *srv;

	lua_pushstring(L, "lighty.srv");
	lua_gettable(L, LUA_REGISTRYINDEX);
	srv = lua_touserdata(L, -1);
	lua_pop(L, 1);

	return magnet_array_pairs(L, srv->status);
}

typedef struct {
	const char *name;
	enum {
		MAGNET_ENV_UNSET,

		MAGNET_ENV_PHYICAL_PATH,
		MAGNET_ENV_PHYICAL_REL_PATH,
		MAGNET_ENV_PHYICAL_DOC_ROOT,
		MAGNET_ENV_PHYICAL_BASEDIR,

		MAGNET_ENV_URI_PATH,
		MAGNET_ENV_URI_PATH_RAW,
		MAGNET_ENV_URI_SCHEME,
		MAGNET_ENV_URI_AUTHORITY,
		MAGNET_ENV_URI_QUERY,

		MAGNET_ENV_REQUEST_METHOD,
		MAGNET_ENV_REQUEST_URI,
		MAGNET_ENV_REQUEST_ORIG_URI,
		MAGNET_ENV_REQUEST_PATH_INFO,
		MAGNET_ENV_REQUEST_REMOTE_IP,
		MAGNET_ENV_REQUEST_PROTOCOL
	} type;
} magnet_env_t;

static const magnet_env_t magnet_env[] = {
	{ "physical.path", MAGNET_ENV_PHYICAL_PATH },
	{ "physical.rel-path", MAGNET_ENV_PHYICAL_REL_PATH },
	{ "physical.doc-root", MAGNET_ENV_PHYICAL_DOC_ROOT },
	{ "physical.basedir", MAGNET_ENV_PHYICAL_BASEDIR },

	{ "uri.path", MAGNET_ENV_URI_PATH },
	{ "uri.path-raw", MAGNET_ENV_URI_PATH_RAW },
	{ "uri.scheme", MAGNET_ENV_URI_SCHEME },
	{ "uri.authority", MAGNET_ENV_URI_AUTHORITY },
	{ "uri.query", MAGNET_ENV_URI_QUERY },

	{ "request.method", MAGNET_ENV_REQUEST_METHOD },
	{ "request.uri", MAGNET_ENV_REQUEST_URI },
	{ "request.orig-uri", MAGNET_ENV_REQUEST_ORIG_URI },
	{ "request.path-info", MAGNET_ENV_REQUEST_PATH_INFO },
	{ "request.remote-ip", MAGNET_ENV_REQUEST_REMOTE_IP },
	{ "request.protocol", MAGNET_ENV_REQUEST_PROTOCOL },

	{ NULL, MAGNET_ENV_UNSET }
};

static buffer *magnet_env_get_buffer_by_id(server *srv, connection *con, int id) {
	buffer *dest = NULL;

	UNUSED(srv);

	/**
	 * map all internal variables to lua
	 *
	 */

	switch (id) {
	case MAGNET_ENV_PHYICAL_PATH: dest = con->physical.path; break;
	case MAGNET_ENV_PHYICAL_REL_PATH: dest = con->physical.rel_path; break;
	case MAGNET_ENV_PHYICAL_DOC_ROOT: dest = con->physical.doc_root; break;
	case MAGNET_ENV_PHYICAL_BASEDIR: dest = con->physical.basedir; break;

	case MAGNET_ENV_URI_PATH: dest = con->uri.path; break;
	case MAGNET_ENV_URI_PATH_RAW: dest = con->uri.path_raw; break;
	case MAGNET_ENV_URI_SCHEME: dest = con->uri.scheme; break;
	case MAGNET_ENV_URI_AUTHORITY: dest = con->uri.authority; break;
	case MAGNET_ENV_URI_QUERY: dest = con->uri.query; break;

	case MAGNET_ENV_REQUEST_METHOD:
		buffer_copy_string(srv->tmp_buf, get_http_method_name(con->request.http_method));
		dest = srv->tmp_buf;
		break;
	case MAGNET_ENV_REQUEST_URI:      dest = con->request.uri; break;
	case MAGNET_ENV_REQUEST_ORIG_URI: dest = con->request.orig_uri; break;
	case MAGNET_ENV_REQUEST_PATH_INFO: dest = con->request.pathinfo; break;
	case MAGNET_ENV_REQUEST_REMOTE_IP: dest = con->dst_addr_buf; break;
	case MAGNET_ENV_REQUEST_PROTOCOL:
		buffer_copy_string(srv->tmp_buf, get_http_version_name(con->request.http_version));
		dest = srv->tmp_buf;
		break;

	case MAGNET_ENV_UNSET: break;
	}

	return dest;
}

static buffer *magnet_env_get_buffer(server *srv, connection *con, const char *key) {
	size_t i;

	for (i = 0; magnet_env[i].name; i++) {
		if (0 == strcmp(key, magnet_env[i].name)) break;
	}

	return magnet_env_get_buffer_by_id(srv, con, magnet_env[i].type);
}

static int magnet_env_get(lua_State *L) {
	server *srv;
	connection *con;

	const char *key = luaL_checkstring(L, 2);
	buffer *dest = NULL;

	lua_pushstring(L, "lighty.srv");
	lua_gettable(L, LUA_REGISTRYINDEX);
	srv = lua_touserdata(L, -1);
	lua_pop(L, 1);

	lua_pushstring(L, "lighty.con");
	lua_gettable(L, LUA_REGISTRYINDEX);
	con = lua_touserdata(L, -1);
	lua_pop(L, 1);

	dest = magnet_env_get_buffer(srv, con, key);

	if (!buffer_is_empty(dest)) {
		lua_pushlstring(L, CONST_BUF_LEN(dest));
	} else {
		lua_pushnil(L);
	}

	return 1;
}

static int magnet_env_set(lua_State *L) {
	server *srv;
	connection *con;

	const char *key = luaL_checkstring(L, 2);
	const char *val = luaL_checkstring(L, 3);
	buffer *dest = NULL;

	lua_pushstring(L, "lighty.srv");
	lua_gettable(L, LUA_REGISTRYINDEX);
	srv = lua_touserdata(L, -1);
	lua_pop(L, 1);

	lua_pushstring(L, "lighty.con");
	lua_gettable(L, LUA_REGISTRYINDEX);
	con = lua_touserdata(L, -1);
	lua_pop(L, 1);

	if (NULL != (dest = magnet_env_get_buffer(srv, con, key))) {
		buffer_copy_string(dest, val);
	} else {
		/* couldn't save */

		return luaL_error(L, "couldn't store '%s' in lighty.env[]", key);
	}

	return 0;
}

static int magnet_env_next(lua_State *L) {
	server *srv;
	connection *con;
	int pos = lua_tointeger(L, lua_upvalueindex(1));

	buffer *dest;

	lua_pushstring(L, "lighty.srv");
	lua_gettable(L, LUA_REGISTRYINDEX);
	srv = lua_touserdata(L, -1);
	lua_pop(L, 1);

	lua_pushstring(L, "lighty.con");
	lua_gettable(L, LUA_REGISTRYINDEX);
	con = lua_touserdata(L, -1);
	lua_pop(L, 1);

	lua_settop(L, 0);

	if (NULL == magnet_env[pos].name) return 0; /* end of list */

	lua_pushstring(L, magnet_env[pos].name);

	dest = magnet_env_get_buffer_by_id(srv, con, magnet_env[pos].type);
	if (!buffer_is_empty(dest)) {
		lua_pushlstring(L, CONST_BUF_LEN(dest));
	} else {
		lua_pushnil(L);
	}

	/* Update our positional upval to reflect our new current position */
	pos++;
	lua_pushinteger(L, pos);
	lua_replace(L, lua_upvalueindex(1));

	/* Returning 2 items on the stack (key, value) */
	return 2;
}

static int magnet_env_pairs(lua_State *L) {
	lua_pushinteger(L, 0); /* Push our current pos (the start) into upval 1 */
	lua_pushcclosure(L, magnet_env_next, 1); /* Push our new closure with 1 upvals */
	return 1;
}

static int magnet_cgi_get(lua_State *L) {
	connection *con;
	data_string *ds;

	const char *key = luaL_checkstring(L, 2);

	lua_pushstring(L, "lighty.con");
	lua_gettable(L, LUA_REGISTRYINDEX);
	con = lua_touserdata(L, -1);
	lua_pop(L, 1);

	ds = (data_string *)array_get_element(con->environment, key);
	if (NULL != ds && !buffer_is_empty(ds->value))
		lua_pushlstring(L, CONST_BUF_LEN(ds->value));
	else
		lua_pushnil(L);

	return 1;
}

static int magnet_cgi_set(lua_State *L) {
	connection *con;

	const char *key = luaL_checkstring(L, 2);
	const char *val = luaL_checkstring(L, 3);

	lua_pushstring(L, "lighty.con");
	lua_gettable(L, LUA_REGISTRYINDEX);
	con = lua_touserdata(L, -1);
	lua_pop(L, 1);

	array_set_key_value(con->environment, key, strlen(key), val, strlen(val));

	return 0;
}

static int magnet_cgi_pairs(lua_State *L) {
	connection *con;

	lua_pushstring(L, "lighty.con");
	lua_gettable(L, LUA_REGISTRYINDEX);
	con = lua_touserdata(L, -1);
	lua_pop(L, 1);

	return magnet_array_pairs(L, con->environment);
}


static int magnet_copy_response_header(server *srv, connection *con, plugin_data *p, lua_State *L) {
	UNUSED(p);
	/**
	 * get the environment of the function
	 */

	lua_getfenv(L, -1); /* -1 is the function */

	/* lighty.header */

	lua_getfield(L, -1, "lighty"); /* lighty.* from the env  */
	force_assert(lua_istable(L, -1));

	lua_getfield(L, -1, "header"); /* lighty.header */
	if (lua_istable(L, -1)) {
		/* header is found, and is a table */

		lua_pushnil(L);
		while (lua_next(L, -2) != 0) {
			if (lua_isstring(L, -1) && lua_isstring(L, -2)) {
				const char *key, *val;
				size_t key_len, val_len;

				key = lua_tolstring(L, -2, &key_len);
				val = lua_tolstring(L, -1, &val_len);

				response_header_overwrite(srv, con, key, key_len, val, val_len);
			}

			lua_pop(L, 1);
		}
	}

	lua_pop(L, 1); /* pop the header-table */
	lua_pop(L, 1); /* pop the lighty-env */
	lua_pop(L, 1); /* pop the function env */

	return 0;
}

/**
 * walk through the content array
 *
 * content = { "<pre>", { file = "/content" } , "</pre>" }
 *
 * header["Content-Type"] = "text/html"
 *
 * return 200
 */
static int magnet_attach_content(server *srv, connection *con, plugin_data *p, lua_State *L) {
	UNUSED(p);
	/**
	 * get the environment of the function
	 */

	force_assert(lua_isfunction(L, -1));
	lua_getfenv(L, -1); /* -1 is the function */

	lua_getfield(L, -1, "lighty"); /* lighty.* from the env  */
	force_assert(lua_istable(L, -1));

	lua_getfield(L, -1, "content"); /* lighty.content */
	if (lua_istable(L, -1)) {
		int i;
		/* header is found, and is a table */

		for (i = 1; ; i++) {
			lua_rawgeti(L, -1, i);

			/* -1 is the value and should be the value ... aka a table */
			if (lua_isstring(L, -1)) {
				size_t s_len = 0;
				const char *s = lua_tolstring(L, -1, &s_len);

				chunkqueue_append_mem(con->write_queue, s, s_len);
			} else if (lua_istable(L, -1)) {
				lua_getfield(L, -1, "filename");
				lua_getfield(L, -2, "length");
				lua_getfield(L, -3, "offset");

				if (lua_isstring(L, -3)) { /* filename has to be a string */
					buffer *fn;
					stat_cache_entry *sce;
					const char *fn_str;
					handler_t res;

					fn_str = lua_tostring(L, -3);
					fn = buffer_init_string(fn_str);

					res = stat_cache_get_entry(srv, con, fn, &sce);

					if (HANDLER_GO_ON == res) {
						off_t off = 0;
						off_t len = 0;

						if (lua_isnumber(L, -1)) {
							off = lua_tonumber(L, -1);
						}

						if (lua_isnumber(L, -2)) {
							len = lua_tonumber(L, -2);
						} else {
							len = sce->st.st_size;
						}

						if (off < 0) {
							buffer_free(fn);
							return luaL_error(L, "offset for '%s' is negative", fn_str);
						}

						if (len < off) {
							buffer_free(fn);
							return luaL_error(L, "offset > length for '%s'", fn_str);
						}

						chunkqueue_append_file(con->write_queue, fn, off, len - off);
					}

					buffer_free(fn);
				} else {
					lua_pop(L, 3 + 2); /* correct the stack */

					return luaL_error(L, "content[%d] is a table and requires the field \"filename\"", i);
				}

				lua_pop(L, 3);
			} else if (lua_isnil(L, -1)) {
				/* oops, end of list */

				lua_pop(L, 1);

				break;
			} else {
				lua_pop(L, 4);

				return luaL_error(L, "content[%d] is neither a string nor a table: ", i);
			}

			lua_pop(L, 1); /* pop the content[...] table */
		}
	} else {
		return luaL_error(L, "lighty.content has to be a table");
	}
	lua_pop(L, 1); /* pop the header-table */
	lua_pop(L, 1); /* pop the lighty-table */
	lua_pop(L, 1); /* php the function env */

	return 0;
}

static int traceback (lua_State *L) {
	if (!lua_isstring(L, 1))  /* 'message' not a string? */
		return 1;  /* keep it intact */
	lua_getfield(L, LUA_GLOBALSINDEX, "debug");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return 1;
	}
	lua_getfield(L, -1, "traceback");
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 2);
		return 1;
	}
	lua_pushvalue(L, 1);  /* pass error message */
	lua_pushinteger(L, 2);  /* skip this function and traceback */
	lua_call(L, 2, 1);  /* call debug.traceback */
	return 1;
}

static int push_traceback(lua_State *L, int narg) {
	int base = lua_gettop(L) - narg;  /* function index */
	lua_pushcfunction(L, traceback);
	lua_insert(L, base);
	return base;
}

static handler_t magnet_attract(server *srv, connection *con, plugin_data *p, buffer *name) {
	lua_State *L;
	int lua_return_value = -1;
	int errfunc;
	/* get the script-context */


	L = script_cache_get_script(srv, con, p->cache, name);

	if (lua_isstring(L, -1)) {
		log_error_write(srv, __FILE__, __LINE__,
				"sbss",
				"loading script",
				name,
				"failed:",
				lua_tostring(L, -1));

		lua_pop(L, 1);

		force_assert(lua_gettop(L) == 0); /* only the function should be on the stack */

		con->http_status = 500;
		con->mode = DIRECT;

		return HANDLER_FINISHED;
	}

	lua_pushstring(L, "lighty.srv");
	lua_pushlightuserdata(L, srv);
	lua_settable(L, LUA_REGISTRYINDEX); /* registery[<id>] = srv */

	lua_pushstring(L, "lighty.con");
	lua_pushlightuserdata(L, con);
	lua_settable(L, LUA_REGISTRYINDEX); /* registery[<id>] = con */

	lua_atpanic(L, magnet_atpanic);

	/**
	 * we want to create empty environment for our script
	 *
	 * setmetatable({}, {__index = _G})
	 *
	 * if a function, symbol is not defined in our env, __index will lookup
	 * in the global env.
	 *
	 * all variables created in the script-env will be thrown
	 * away at the end of the script run.
	 */
	lua_newtable(L); /* my empty environment aka {}              (sp += 1) */

	/* we have to overwrite the print function */
	lua_pushcfunction(L, magnet_print);                       /* (sp += 1) */
	lua_setfield(L, -2, "print"); /* -1 is the env we want to set(sp -= 1) */

	/**
	 * lighty.request[] has the HTTP-request headers
	 * lighty.content[] is a table of string/file
	 * lighty.header[] is a array to set response headers
	 */

	lua_newtable(L); /* lighty.*                                 (sp += 1) */

	lua_newtable(L); /*  {}                                      (sp += 1) */
	lua_newtable(L); /* the meta-table for the request-table     (sp += 1) */
	lua_pushcfunction(L, magnet_reqhdr_get);                  /* (sp += 1) */
	lua_setfield(L, -2, "__index");                           /* (sp -= 1) */
	lua_pushcfunction(L, magnet_reqhdr_pairs);                /* (sp += 1) */
	lua_setfield(L, -2, "__pairs");                           /* (sp -= 1) */
	lua_setmetatable(L, -2); /* tie the metatable to request     (sp -= 1) */
	lua_setfield(L, -2, "request"); /* content = {}              (sp -= 1) */

	lua_newtable(L); /*  {}                                      (sp += 1) */
	lua_newtable(L); /* the meta-table for the request-table     (sp += 1) */
	lua_pushcfunction(L, magnet_env_get);                     /* (sp += 1) */
	lua_setfield(L, -2, "__index");                           /* (sp -= 1) */
	lua_pushcfunction(L, magnet_env_set);                     /* (sp += 1) */
	lua_setfield(L, -2, "__newindex");                        /* (sp -= 1) */
	lua_pushcfunction(L, magnet_env_pairs);                   /* (sp += 1) */
	lua_setfield(L, -2, "__pairs");                           /* (sp -= 1) */
	lua_setmetatable(L, -2); /* tie the metatable to request     (sp -= 1) */
	lua_setfield(L, -2, "env"); /* content = {}                  (sp -= 1) */

	lua_newtable(L); /*  {}                                      (sp += 1) */
	lua_newtable(L); /* the meta-table for the request-table     (sp += 1) */
	lua_pushcfunction(L, magnet_cgi_get);                     /* (sp += 1) */
	lua_setfield(L, -2, "__index");                           /* (sp -= 1) */
	lua_pushcfunction(L, magnet_cgi_set);                     /* (sp += 1) */
	lua_setfield(L, -2, "__newindex");                        /* (sp -= 1) */
	lua_pushcfunction(L, magnet_cgi_pairs);                   /* (sp += 1) */
	lua_setfield(L, -2, "__pairs");                           /* (sp -= 1) */
	lua_setmetatable(L, -2); /* tie the metatable to req_env     (sp -= 1) */
	lua_setfield(L, -2, "req_env"); /* content = {}              (sp -= 1) */

	lua_newtable(L); /*  {}                                      (sp += 1) */
	lua_newtable(L); /* the meta-table for the request-table     (sp += 1) */
	lua_pushcfunction(L, magnet_status_get);                  /* (sp += 1) */
	lua_setfield(L, -2, "__index");                           /* (sp -= 1) */
	lua_pushcfunction(L, magnet_status_set);                  /* (sp += 1) */
	lua_setfield(L, -2, "__newindex");                        /* (sp -= 1) */
	lua_pushcfunction(L, magnet_status_pairs);                /* (sp += 1) */
	lua_setfield(L, -2, "__pairs");                           /* (sp -= 1) */
	lua_setmetatable(L, -2); /* tie the metatable to request     (sp -= 1) */
	lua_setfield(L, -2, "status"); /* content = {}               (sp -= 1) */

	/* add empty 'content' and 'header' tables */
	lua_newtable(L); /*  {}                                      (sp += 1) */
	lua_setfield(L, -2, "content"); /* content = {}              (sp -= 1) */

	lua_newtable(L); /*  {}                                      (sp += 1) */
	lua_setfield(L, -2, "header"); /* header = {}                (sp -= 1) */

	lua_pushinteger(L, MAGNET_RESTART_REQUEST);
	lua_setfield(L, -2, "RESTART_REQUEST");

	lua_pushcfunction(L, magnet_stat);                        /* (sp += 1) */
	lua_setfield(L, -2, "stat"); /* -1 is the env we want to set (sp -= 1) */

	lua_setfield(L, -2, "lighty"); /* lighty.*                   (sp -= 1) */

	/* override the default pairs() function to our __pairs capable version */
	lua_getglobal(L, "pairs"); /* push original pairs()          (sp += 1) */
	lua_pushcclosure(L, magnet_pairs, 1);
	lua_setfield(L, -2, "pairs");                             /* (sp -= 1) */

	lua_newtable(L); /* the meta-table for the new env           (sp += 1) */
	lua_pushvalue(L, LUA_GLOBALSINDEX);                       /* (sp += 1) */
	lua_setfield(L, -2, "__index"); /* { __index = _G }          (sp -= 1) */
	lua_setmetatable(L, -2); /* setmetatable({}, {__index = _G}) (sp -= 1) */


	lua_setfenv(L, -2); /* on the stack should be a modified env (sp -= 1) */

	errfunc = push_traceback(L, 0);
	if (lua_pcall(L, 0, 1, errfunc)) {
		lua_remove(L, errfunc);
		log_error_write(srv, __FILE__, __LINE__,
			"ss",
			"lua_pcall():",
			lua_tostring(L, -1));
		lua_pop(L, 1); /* remove the error-msg and the function copy from the stack */

		force_assert(lua_gettop(L) == 1); /* only the function should be on the stack */

		con->http_status = 500;
		con->mode = DIRECT;

		return HANDLER_FINISHED;
	}
	lua_remove(L, errfunc);

	/* we should have the function-copy and the return value on the stack */
	force_assert(lua_gettop(L) == 2);

	if (lua_isnumber(L, -1)) {
		/* if the ret-value is a number, take it */
		lua_return_value = (int)lua_tonumber(L, -1);
	}
	lua_pop(L, 1); /* pop the ret-value */

	magnet_copy_response_header(srv, con, p, L);

	if (lua_return_value > 99) {
		con->http_status = lua_return_value;
		con->file_finished = 1;

		/* try { ...*/
		if (0 == setjmp(exceptionjmp)) {
			magnet_attach_content(srv, con, p, L);
			if (!chunkqueue_is_empty(con->write_queue)) {
				con->mode = p->id;
			}
		} else {
			/* } catch () { */
			con->http_status = 500;
			con->mode = DIRECT;
		}

		force_assert(lua_gettop(L) == 1); /* only the function should be on the stack */

		/* we are finished */
		return HANDLER_FINISHED;
	} else if (MAGNET_RESTART_REQUEST == lua_return_value) {
		force_assert(lua_gettop(L) == 1); /* only the function should be on the stack */

		return HANDLER_COMEBACK;
	} else {
		force_assert(lua_gettop(L) == 1); /* only the function should be on the stack */

		return HANDLER_GO_ON;
	}
}

static handler_t magnet_attract_array(server *srv, connection *con, plugin_data *p, array *files) {
	size_t i;

	/* no filename set */
	if (files->used == 0) return HANDLER_GO_ON;

	/**
	 * execute all files and jump out on the first !HANDLER_GO_ON
	 */
	for (i = 0; i < files->used; i++) {
		data_string *ds = (data_string *)files->data[i];
		handler_t ret;

		if (buffer_string_is_empty(ds->value)) continue;

		ret = magnet_attract(srv, con, p, ds->value);

		if (ret != HANDLER_GO_ON) return ret;
	}

	return HANDLER_GO_ON;
}

URIHANDLER_FUNC(mod_magnet_uri_handler) {
	plugin_data *p = p_d;

	mod_magnet_patch_connection(srv, con, p);

	return magnet_attract_array(srv, con, p, p->conf.url_raw);
}

URIHANDLER_FUNC(mod_magnet_physical) {
	plugin_data *p = p_d;

	mod_magnet_patch_connection(srv, con, p);

	return magnet_attract_array(srv, con, p, p->conf.physical_path);
}


/* this function is called at dlopen() time and inits the callbacks */

int mod_magnet_plugin_init(plugin *p);
int mod_magnet_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("magnet");

	p->init        = mod_magnet_init;
	p->handle_uri_clean  = mod_magnet_uri_handler;
	p->handle_physical   = mod_magnet_physical;
	p->set_defaults  = mod_magnet_set_defaults;
	p->cleanup     = mod_magnet_free;

	p->data        = NULL;

	return 0;
}

#else

#pragma message("lua is required, but was not found")

int mod_magnet_plugin_init(plugin *p);
int mod_magnet_plugin_init(plugin *p) {
	UNUSED(p);
	return -1;
}
#endif
