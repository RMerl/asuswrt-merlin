#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <dlfcn.h>

#if __APPLE__
#include <mach/mach_time.h>
#endif

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "timeout.h"
#include "bench.h"

#if LUA_VERSION_NUM < 502
static int lua_absindex(lua_State *L, int idx) {
	return (idx > 0 || idx <= LUA_REGISTRYINDEX)? idx : lua_gettop(L) + idx + 1;
} /* lua_absindex() */

static void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup) {
	int i, t = lua_absindex(L, -1 - nup);

	for (; l->name; l++) {
		for (i = 0; i < nup; i++)
			lua_pushvalue(L, -nup);
		lua_pushcclosure(L, l->func, nup);
		lua_setfield(L, t, l->name);
	}

	lua_pop(L, nup);
} /* luaL_setfuncs() */

#define luaL_newlibtable(L, l) \
	lua_createtable(L, 0, (sizeof (l) / sizeof *(l)) - 1)

#define luaL_newlib(L, l) \
	(luaL_newlibtable((L), (l)), luaL_setfuncs((L), (l), 0))
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b))? (a) : (b))
#endif


struct bench {
	const char *path;
	void *solib;
	size_t count;
	timeout_t timeout_max;
	int verbose;

	void *state;
	struct timeout *timeout;
	struct benchops ops;
	timeout_t curtime;
}; /* struct bench */


#if __APPLE__
static mach_timebase_info_data_t timebase;
#endif


static int long long monotime(void) {
#if __APPLE__
	unsigned long long abt;

	abt = mach_absolute_time();
	abt = abt * timebase.numer / timebase.denom;

	return abt / 1000LL;
#else
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return (ts.tv_sec * 1000000L) + (ts.tv_nsec / 1000L);
#endif
} /* monotime() */


static int bench_clock(lua_State *L) {
	lua_pushnumber(L, (double)monotime() / 1000000L);

	return 1;
} /* bench_clock() */


static int bench_new(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	size_t count = luaL_optinteger(L, 2, 1000000);
	timeout_t timeout_max = luaL_optinteger(L, 3, 300 * 1000000L);
	int verbose = (lua_isnone(L, 4))? 0 : lua_toboolean(L, 4);
	struct bench *B;
	struct benchops *ops;

	B = lua_newuserdata(L, sizeof *B);
	memset(B, 0, sizeof *B);

	luaL_getmetatable(L, "BENCH*");
	lua_setmetatable(L, -2);

	B->count = count;
	B->timeout_max = timeout_max;
	B->verbose = verbose;

	if (!(B->timeout = calloc(count, sizeof *B->timeout)))
		return luaL_error(L, "%s", strerror(errno));

	if (!(B->solib = dlopen(path, RTLD_NOW|RTLD_LOCAL)))
		return luaL_error(L, "%s: %s", path, dlerror());

	if (!(ops = dlsym(B->solib, "benchops")))
		return luaL_error(L, "%s: %s", path, dlerror());

	B->ops = *ops;
	B->state = B->ops.init(B->timeout, B->count, B->verbose);

	return 1;
} /* bench_new() */


static int bench_add(lua_State *L) {
	struct bench *B = lua_touserdata(L, 1);
	unsigned i;
	timeout_t t;

	i = (lua_isnoneornil(L, 2))? random() % B->count : (unsigned)luaL_checkinteger(L, 2);
	t = (lua_isnoneornil(L, 3))? random() % B->timeout_max : (unsigned)luaL_checkinteger(L, 3);

	B->ops.add(B->state, &B->timeout[i], t);

	return 0;
} /* bench_add() */


static int bench_del(lua_State *L) {
	struct bench *B = lua_touserdata(L, 1);
	size_t i = luaL_optinteger(L, 2, random() % B->count);
	size_t j = luaL_optinteger(L, 3, i);

	while (i <= j && i < B->count) {
		B->ops.del(B->state, &B->timeout[i]);
		++i;
	}

	return 0;
} /* bench_del() */


static int bench_fill(lua_State *L) {
	struct bench *B = lua_touserdata(L, 1);
	size_t count = luaL_optinteger(L, 2, B->count);
	long timeout_inc = luaL_optinteger(L, 3, -1), timeout_max = 0, timeout;
	size_t i;

	if (timeout_inc < 0) {
		for (i = 0; i < count; i++) {
			timeout = random() % B->timeout_max;
			B->ops.add(B->state, &B->timeout[i], timeout);
			timeout_max = MAX(timeout, timeout_max);
		}
	} else {
		for (i = 0; i < count; i++) {
			timeout = timeout_inc + i;
			B->ops.add(B->state, &B->timeout[i], timeout_inc + i);
			timeout_max = MAX(timeout, timeout_max);
		}
	}

	lua_pushinteger(L, (lua_Integer)count);
	lua_pushinteger(L, (lua_Integer)timeout_max);

	return 2;
} /* bench_fill() */


static int bench_expire(lua_State *L) {
	struct bench *B = lua_touserdata(L, 1);
	unsigned count = luaL_optinteger(L, 2, B->count);
	unsigned step = luaL_optinteger(L, 3, 300000);
	size_t i = 0;

	while (i < count && !B->ops.empty(B->state)) {
		B->curtime += step;
		B->ops.update(B->state, B->curtime);

		while (B->ops.get(B->state))
			i++;
	}

	lua_pushinteger(L, (lua_Integer)i);

	return 1;
} /* bench_expire() */


static int bench_empty(lua_State *L) {
	struct bench *B = lua_touserdata(L, 1);

	lua_pushboolean(L, B->ops.empty(B->state));

	return 1;
} /* bench_empty() */


static int bench__next(lua_State *L) {
	struct bench *B = lua_touserdata(L, lua_upvalueindex(1));
	struct timeouts_it *it = lua_touserdata(L, lua_upvalueindex(2));
	struct timeout *to;

	if (!B->ops.next || !(to = B->ops.next(B->state, it)))
		return 0;

	lua_pushinteger(L, luaL_optinteger(L, 2, 0) + 1);

	lua_newtable(L);
	lua_pushinteger(L, to->expires);
	lua_setfield(L, -2, "expires");

	return 2;
} /* bench__next() */

static int bench__pairs(lua_State *L) {
	struct timeouts_it *it;

	lua_settop(L, 1);

	it = lua_newuserdata(L, sizeof *it);
	TIMEOUTS_IT_INIT(it, TIMEOUTS_ALL);

	lua_pushcclosure(L, &bench__next, 2);
	lua_pushvalue(L, 1);
	lua_pushinteger(L, 0);

	return 3;
} /* bench__pairs() */


static int bench__gc(lua_State *L) {
	struct bench *B = lua_touserdata(L, 1);

	if (B->state) {
		B->ops.destroy(B->state);
		B->state = NULL;
	}

	return 0;
} /* bench__gc() */


static const luaL_Reg bench_methods[] = {
	{ "add",    &bench_add },
	{ "del",    &bench_del },
	{ "fill",   &bench_fill },
	{ "expire", &bench_expire },
	{ "empty",  &bench_empty },
	{ "close",  &bench__gc },
	{ NULL,     NULL }
};

static const luaL_Reg bench_metatable[] = {
	{ "__pairs", &bench__pairs },
	{ "__gc",    &bench__gc },
	{ NULL,      NULL }
};

static const luaL_Reg bench_globals[] = {
	{ "new",   &bench_new },
	{ "clock", &bench_clock },
	{ NULL,  NULL }
};

int luaopen_bench(lua_State *L) {
#if __APPLE__
	mach_timebase_info(&timebase);
#endif

	if (luaL_newmetatable(L, "BENCH*")) {
		luaL_setfuncs(L, bench_metatable, 0);
		luaL_newlib(L, bench_methods);
		lua_setfield(L, -2, "__index");
	}

	luaL_newlib(L, bench_globals);

	return 1;
} /* luaopen_bench() */

