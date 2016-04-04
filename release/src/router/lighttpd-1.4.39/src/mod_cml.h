#ifndef _MOD_CACHE_H_
#define _MOD_CACHE_H_

#include "buffer.h"
#include "server.h"
#include "response.h"

#include "stream.h"
#include "plugin.h"

#if defined(HAVE_MEMCACHE_H)
#include <memcache.h>
#endif

#define plugin_data mod_cache_plugin_data

typedef struct {
	buffer *ext;

	array  *mc_hosts;
	buffer *mc_namespace;
#if defined(HAVE_MEMCACHE_H)
	struct memcache *mc;
#endif
	buffer *power_magnet;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	buffer *basedir;
	buffer *baseurl;

	buffer *trigger_handler;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

int cache_parse_lua(server *srv, connection *con, plugin_data *p, buffer *fn);

#endif
