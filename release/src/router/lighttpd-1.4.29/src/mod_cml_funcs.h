#ifndef _MOD_CML_FUNCS_H_
#define _MOD_CML_FUNCS_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_LUA_H
#include <lua.h>

int f_crypto_md5(lua_State *L);
int f_file_mtime(lua_State *L);
int f_file_isreg(lua_State *L);
int f_file_isdir(lua_State *L);
int f_dir_files(lua_State *L);

int f_memcache_exists(lua_State *L);
int f_memcache_get_string(lua_State *L);
int f_memcache_get_long(lua_State *L);
#endif
#endif
