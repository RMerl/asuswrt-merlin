/*
 * Copyright (C) 2012 John Crispin <blogic@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "../uloop.h"
#include "../list.h"

struct lua_uloop_timeout {
	struct uloop_timeout t;
	int r;
};

struct lua_uloop_process {
	struct uloop_process p;
	int r;
};

static lua_State *state;

static void ul_timer_cb(struct uloop_timeout *t)
{
	struct lua_uloop_timeout *tout = container_of(t, struct lua_uloop_timeout, t);

	lua_getglobal(state, "__uloop_cb");
	lua_rawgeti(state, -1, tout->r);
	lua_call(state, 0, 0);
}

static int ul_timer_set(lua_State *L)
{
	struct lua_uloop_timeout *tout;
	double set;

	if (!lua_isnumber(L, -1)) {
		lua_pushstring(L, "invalid arg list");
		lua_error(L);

		return 0;
	}

	set = lua_tointeger(L, -1);
	tout = lua_touserdata(L, 1);
	uloop_timeout_set(&tout->t, set);

	return 1;
}

static int ul_timer_free(lua_State *L)
{
	struct lua_uloop_timeout *tout = lua_touserdata(L, 1);

	uloop_timeout_cancel(&tout->t);
	lua_getglobal(state, "__uloop_cb");
	luaL_unref(L, -1, tout->r);

	return 1;
}

static const luaL_Reg timer_m[] = {
	{ "set", ul_timer_set },
	{ "cancel", ul_timer_free },
	{ NULL, NULL }
};

static int ul_timer(lua_State *L)
{
	struct lua_uloop_timeout *tout;
	int set = 0;
	int ref;

	if (lua_isnumber(L, -1)) {
		set = lua_tointeger(L, -1);
		lua_pop(L, 1);
	}

	if (!lua_isfunction(L, -1)) {
		lua_pushstring(L, "invalid arg list");
		lua_error(L);

		return 0;
	}

	lua_getglobal(L, "__uloop_cb");
	lua_pushvalue(L, -2);
	ref = luaL_ref(L, -2);

	tout = lua_newuserdata(L, sizeof(*tout));
	lua_createtable(L, 0, 2);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, ul_timer_free);
	lua_setfield(L, -2, "__gc");
	lua_pushvalue(L, -1);
	lua_setmetatable(L, -3);
	lua_pushvalue(L, -2);
	luaI_openlib(L, NULL, timer_m, 1);
	lua_pushvalue(L, -2);

	memset(tout, 0, sizeof(*tout));

	tout->r = ref;
	tout->t.cb = ul_timer_cb;
	if (set)
		uloop_timeout_set(&tout->t, set);

	return 1;
}

static void ul_process_cb(struct uloop_process *p, int ret)
{
	struct lua_uloop_process *proc = container_of(p, struct lua_uloop_process, p);

	lua_getglobal(state, "__uloop_cb");
	lua_rawgeti(state, -1, proc->r);
	luaL_unref(state, -2, proc->r);
	lua_pushinteger(state, ret >> 8);
	lua_call(state, 1, 0);
}

static int ul_process(lua_State *L)
{
	struct lua_uloop_process *proc;
	pid_t pid;
	int ref;

	if (!lua_isfunction(L, -1) || !lua_istable(L, -2) ||
			!lua_istable(L, -3) || !lua_isstring(L, -4)) {
		lua_pushstring(L, "invalid arg list");
		lua_error(L);

		return 0;
	}

	pid = fork();

	if (pid == -1) {
		lua_pushstring(L, "failed to fork");
		lua_error(L);

		return 0;
	}

	if (pid == 0) {
		/* child */
		int argn = lua_objlen(L, -3);
		int envn = lua_objlen(L, -2);
		char** argp = malloc(sizeof(char*) * (argn + 2));
		char** envp = malloc(sizeof(char*) * envn + 1);
		int i = 1;

		argp[0] = (char*) lua_tostring(L, -4);
		for (i = 1; i <= argn; i++) {
			lua_rawgeti(L, -3, i);
			argp[i] = (char*) lua_tostring(L, -1);
			lua_pop(L, 1);
		}
		argp[i] = NULL;

		for (i = 1; i <= envn; i++) {
			lua_rawgeti(L, -2, i);
			envp[i - 1] = (char*) lua_tostring(L, -1);
			lua_pop(L, 1);
		}
		envp[i - 1] = NULL;

		execve(*argp, argp, envp);
		exit(-1);
	}

	lua_getglobal(L, "__uloop_cb");
	lua_pushvalue(L, -2);
	ref = luaL_ref(L, -2);

	proc = lua_newuserdata(L, sizeof(*proc));
	memset(proc, 0, sizeof(*proc));

	proc->r = ref;
	proc->p.pid = pid;
	proc->p.cb = ul_process_cb;
	uloop_process_add(&proc->p);

	return 1;
}

static int ul_init(lua_State *L)
{
	uloop_init();
	lua_pushboolean(L, 1);

	return 1;
}

static int ul_run(lua_State *L)
{
	uloop_run();
	lua_pushboolean(L, 1);

	return 1;
}

static luaL_reg uloop_func[] = {
	{"init", ul_init},
	{"run", ul_run},
	{"timer", ul_timer},
	{"process", ul_process},
	{NULL, NULL},
};

/* avoid warnings about missing declarations */
int luaopen_uloop(lua_State *L);
int luaclose_uloop(lua_State *L);

int luaopen_uloop(lua_State *L)
{
	state = L;

	lua_createtable(L, 1, 0);
	lua_setglobal(L, "__uloop_cb");

	luaL_openlib(L, "uloop", uloop_func, 0);
	lua_pushstring(L, "_VERSION");
	lua_pushstring(L, "1.0");
	lua_rawset(L, -3);

	return 1;
}

int luaclose_uloop(lua_State *L)
{
	lua_pushstring(L, "Called");

	return 1;
}
