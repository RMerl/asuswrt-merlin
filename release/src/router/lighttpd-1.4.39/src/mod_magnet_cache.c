#include "mod_magnet_cache.h"
#include "stat_cache.h"

#include <stdlib.h>
#include <time.h>
#include <assert.h>

#ifdef HAVE_LUA_H
#include <lualib.h>
#include <lauxlib.h>

static script *script_init() {
	script *sc;

	sc = calloc(1, sizeof(*sc));
	sc->name = buffer_init();
	sc->etag = buffer_init();

	return sc;
}

static void script_free(script *sc) {
	if (!sc) return;

	lua_pop(sc->L, 1); /* the function copy */

	buffer_free(sc->name);
	buffer_free(sc->etag);

	lua_close(sc->L);

	free(sc);
}

script_cache *script_cache_init() {
	script_cache *p;

	p = calloc(1, sizeof(*p));

	return p;
}

void script_cache_free(script_cache *p) {
	size_t i;

	if (!p) return;

	for (i = 0; i < p->used; i++) {
		script_free(p->ptr[i]);
	}

	free(p->ptr);

	free(p);
}

lua_State *script_cache_get_script(server *srv, connection *con, script_cache *cache, buffer *name) {
	size_t i;
	script *sc = NULL;
	stat_cache_entry *sce;

	for (i = 0; i < cache->used; i++) {
		sc = cache->ptr[i];

		if (buffer_is_equal(name, sc->name)) {
			sc->last_used = time(NULL);

			/* oops, the script failed last time */

			if (lua_gettop(sc->L) == 0) break;

			if (HANDLER_ERROR == stat_cache_get_entry(srv, con, sc->name, &sce)) {
				lua_pop(sc->L, 1); /* pop the old function */
				break;
			}

			if (!buffer_is_equal(sce->etag, sc->etag)) {
				/* the etag is outdated, reload the function */
				lua_pop(sc->L, 1);
				break;
			}

			force_assert(lua_isfunction(sc->L, -1));
			lua_pushvalue(sc->L, -1); /* copy the function-reference */

			return sc->L;
		}

		sc = NULL;
	}

	/* if the script was script already loaded but either got changed or
	 * failed to load last time */
	if (sc == NULL) {
		sc = script_init();

		if (cache->size == 0) {
			cache->size = 16;
			cache->ptr = malloc(cache->size * sizeof(*(cache->ptr)));
		} else if (cache->used == cache->size) {
			cache->size += 16;
			cache->ptr = realloc(cache->ptr, cache->size * sizeof(*(cache->ptr)));
		}

		cache->ptr[cache->used++] = sc;

		buffer_copy_buffer(sc->name, name);

		sc->L = luaL_newstate();
		luaL_openlibs(sc->L);
	}

	sc->last_used = time(NULL);

	if (0 != luaL_loadfile(sc->L, name->ptr)) {
		/* oops, an error, return it */

		return sc->L;
	}

	if (HANDLER_GO_ON == stat_cache_get_entry(srv, con, sc->name, &sce)) {
		buffer_copy_buffer(sc->etag, sce->etag);
	}

	/**
	 * pcall() needs the function on the stack
	 *
	 * as pcall() will pop the script from the stack when done, we have to
	 * duplicate it here
	 */
	force_assert(lua_isfunction(sc->L, -1));
	lua_pushvalue(sc->L, -1); /* copy the function-reference */

	return sc->L;
}

#endif
