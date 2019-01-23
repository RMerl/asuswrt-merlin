#include <assert.h>
#include <string.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#if LUA_VERSION_NUM != 503
#error only Lua 5.3 supported
#endif

#define TIMEOUT_PUBLIC static
#include "timeout.h"
#include "timeout.c"

#define TIMEOUT_METANAME "struct timeout"
#define TIMEOUTS_METANAME "struct timeouts*"

static struct timeout *
to_checkudata(lua_State *L, int index)
{
	return luaL_checkudata(L, index, TIMEOUT_METANAME);
}

static struct timeouts *
tos_checkudata(lua_State *L, int index)
{
	return *(struct timeouts **)luaL_checkudata(L, index, TIMEOUTS_METANAME);
}

static void
tos_bind(lua_State *L, int tos_index, int to_index)
{
	lua_getuservalue(L, tos_index);
	lua_pushlightuserdata(L, to_checkudata(L, to_index));
	lua_pushvalue(L, to_index);
	lua_rawset(L, -3);
	lua_pop(L, 1);
}

static void
tos_unbind(lua_State *L, int tos_index, int to_index)
{
	lua_getuservalue(L, tos_index);
	lua_pushlightuserdata(L, to_checkudata(L, to_index));
	lua_pushnil(L);
	lua_rawset(L, -3);
	lua_pop(L, 1);
}

static int
to__index(lua_State *L)
{
	struct timeout *to = to_checkudata(L, 1);

	if (lua_type(L, 2 == LUA_TSTRING)) {
		const char *key = lua_tostring(L, 2);

		if (!strcmp(key, "flags")) {
			lua_pushinteger(L, to->flags);

			return 1;
		} else if (!strcmp(key, "expires")) {
			lua_pushinteger(L, to->expires);

			return 1;
		}
	}

	if (LUA_TNIL != lua_getuservalue(L, 1)) {
		lua_pushvalue(L, 2);
		if (LUA_TNIL != lua_rawget(L, -2))
			return 1;
	}

	lua_pushvalue(L, 2);
	if (LUA_TNIL != lua_rawget(L, lua_upvalueindex(1)))
		return 1;

	return 0;
}

static int
to__newindex(lua_State *L)
{
	if (LUA_TNIL == lua_getuservalue(L, 1)) {
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setuservalue(L, 1);
	}

	lua_pushvalue(L, 2);
	lua_pushvalue(L, 3);
	lua_rawset(L, -3);

	return 0;
}

static int
to__gc(lua_State *L)
{
	struct timeout *to = to_checkudata(L, 1);

	/*
	 * NB: On script exit it's possible for a timeout to still be
	 * associated with a timeouts object, particularly when the timeouts
	 * object was created first.
	 */
	timeout_del(to);

	return 0;
}

static int
to_new(lua_State *L)
{
	int flags = luaL_optinteger(L, 1, 0);
	struct timeout *to;

	to = lua_newuserdata(L, sizeof *to);
	timeout_init(to, flags);
	luaL_setmetatable(L, TIMEOUT_METANAME);

	return 1;
}

static const luaL_Reg to_methods[] = {
	{ NULL,  NULL },
};

static const luaL_Reg to_metatable[] = {
	{ "__index",    &to__index },
	{ "__newindex", &to__newindex },
	{ "__gc",       &to__gc },
	{ NULL,         NULL },
};

static const luaL_Reg to_globals[] = {
	{ "new", &to_new },
	{ NULL,  NULL },
};

static void
to_newmetatable(lua_State *L)
{
	if (luaL_newmetatable(L, TIMEOUT_METANAME)) {
		/*
		 * fill metamethod table, capturing the methods table as an
		 * upvalue for use by __index metamethod
		 */
		luaL_newlib(L, to_methods);
		luaL_setfuncs(L, to_metatable, 1);
	}
}

int
luaopen_timeout(lua_State *L)
{
	to_newmetatable(L);

	luaL_newlib(L, to_globals);
	lua_pushinteger(L, TIMEOUT_INT);
	lua_setfield(L, -2, "INT");
	lua_pushinteger(L, TIMEOUT_ABS);
	lua_setfield(L, -2, "ABS");

	return 1;
}

static int
tos_update(lua_State *L)
{
	struct timeouts *T = tos_checkudata(L, 1);
	lua_Number n = luaL_checknumber(L, 2);

	timeouts_update(T, timeouts_f2i(T, n));

	lua_pushvalue(L, 1);

	return 1;
}

static int
tos_step(lua_State *L)
{
	struct timeouts *T = tos_checkudata(L, 1);
	lua_Number n = luaL_checknumber(L, 2);

	timeouts_step(T, timeouts_f2i(T, n));

	lua_pushvalue(L, 1);

	return 1;
}

static int
tos_timeout(lua_State *L)
{
	struct timeouts *T = tos_checkudata(L, 1);

	lua_pushnumber(L, timeouts_i2f(T, timeouts_timeout(T)));

	return 1;
}

static int
tos_add(lua_State *L)
{
	struct timeouts *T = tos_checkudata(L, 1);
	struct timeout *to = to_checkudata(L, 2);
	lua_Number timeout = luaL_checknumber(L, 3);

	tos_bind(L, 1, 2);
	timeouts_addf(T, to, timeout);

	return lua_pushvalue(L, 1), 1;
}

static int
tos_del(lua_State *L)
{
	struct timeouts *T = tos_checkudata(L, 1);
	struct timeout *to = to_checkudata(L, 2);

	timeouts_del(T, to);
	tos_unbind(L, 1, 2);

	return lua_pushvalue(L, 1), 1;
}

static int
tos_get(lua_State *L)
{
	struct timeouts *T = tos_checkudata(L, 1);
	struct timeout *to;

	if (!(to = timeouts_get(T)))
		return 0;

	lua_getuservalue(L, 1);
	lua_rawgetp(L, -1, to);

	if (!timeout_pending(to))
		tos_unbind(L, 1, lua_absindex(L, -1));

	return 1;
}

static int
tos_pending(lua_State *L)
{
	struct timeouts *T = tos_checkudata(L, 1);

	lua_pushboolean(L, timeouts_pending(T));

	return 1;
}

static int
tos_expired(lua_State *L)
{
	struct timeouts *T = tos_checkudata(L, 1);

	lua_pushboolean(L, timeouts_expired(T));

	return 1;
}

static int
tos_check(lua_State *L)
{
	struct timeouts *T = tos_checkudata(L, 1);

	lua_pushboolean(L, timeouts_check(T, NULL));

	return 1;
}

static int
tos__next(lua_State *L)
{
	struct timeouts *T = tos_checkudata(L, lua_upvalueindex(1));
	struct timeouts_it *it = lua_touserdata(L, lua_upvalueindex(2));
	struct timeout *to;

	if (!(to = timeouts_next(T, it)))
		return 0;

	lua_getuservalue(L, lua_upvalueindex(1));
	lua_rawgetp(L, -1, to);

	return 1;
}

static int
tos_timeouts(lua_State *L)
{
	int flags = luaL_checkinteger(L, 2);
	struct timeouts_it *it;

	tos_checkudata(L, 1);
	lua_pushvalue(L, 1);
	it = lua_newuserdata(L, sizeof *it);
	TIMEOUTS_IT_INIT(it, flags);
	lua_pushcclosure(L, &tos__next, 2);

	return 1;
}

static int
tos__gc(lua_State *L)
{
	struct timeouts **tos = luaL_checkudata(L, 1, TIMEOUTS_METANAME);
	struct timeout *to;

	TIMEOUTS_FOREACH(to, *tos, TIMEOUTS_ALL) {
		timeouts_del(*tos, to);
	}

	timeouts_close(*tos);
	*tos = NULL;

	return 0;
}

static int
tos_new(lua_State *L)
{
	timeout_t hz = luaL_optinteger(L, 1, 0);
	struct timeouts **T;
	int error;
	
	T = lua_newuserdata(L, sizeof *T);
	luaL_setmetatable(L, TIMEOUTS_METANAME);

	lua_newtable(L);
	lua_setuservalue(L, -2);

	if (!(*T = timeouts_open(hz, &error)))
		return luaL_error(L, "%s", strerror(error));

	return 1;
}

static const luaL_Reg tos_methods[] = {
	{ "update",   &tos_update },
	{ "step",     &tos_step },
	{ "timeout",  &tos_timeout },
	{ "add",      &tos_add },
	{ "del",      &tos_del },
	{ "get",      &tos_get },
	{ "pending",  &tos_pending },
	{ "expired",  &tos_expired },
	{ "check",    &tos_check },
	{ "timeouts", &tos_timeouts },
	{ NULL,       NULL },
};

static const luaL_Reg tos_metatable[] = {
	{ "__gc", &tos__gc },
	{ NULL,   NULL },
};

static const luaL_Reg tos_globals[] = {
	{ "new", &tos_new },
	{ NULL,  NULL },
};

static void
tos_newmetatable(lua_State *L)
{
	if (luaL_newmetatable(L, TIMEOUTS_METANAME)) {
		luaL_setfuncs(L, tos_metatable, 0);
		luaL_newlib(L, tos_methods);
		lua_setfield(L, -2, "__index");
	}
}

int
luaopen_timeouts(lua_State *L)
{
	to_newmetatable(L);
	tos_newmetatable(L);

	luaL_newlib(L, tos_globals);
	lua_pushinteger(L, TIMEOUTS_PENDING);
	lua_setfield(L, -2, "PENDING");
	lua_pushinteger(L, TIMEOUTS_EXPIRED);
	lua_setfield(L, -2, "EXPIRED");
	lua_pushinteger(L, TIMEOUTS_ALL);
	lua_setfield(L, -2, "ALL");
	lua_pushinteger(L, TIMEOUTS_CLEAR);
	lua_setfield(L, -2, "CLEAR");

	return 1;
}
