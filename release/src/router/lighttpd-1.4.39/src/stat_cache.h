#ifndef _FILE_CACHE_H_
#define _FILE_CACHE_H_

#include "base.h"

stat_cache *stat_cache_init(void);
void stat_cache_free(stat_cache *fc);

handler_t stat_cache_get_entry(server *srv, connection *con, buffer *name, stat_cache_entry **fce);
handler_t stat_cache_handle_fdevent(server *srv, void *_fce, int revent);

int stat_cache_trigger_cleanup(server *srv);
#endif
