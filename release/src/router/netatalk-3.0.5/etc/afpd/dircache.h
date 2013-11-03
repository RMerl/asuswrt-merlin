/*
   Copyright (c) 2010 Frank Lahm <franklahm@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 */

#ifndef DIRCACHE_H 
#define DIRCACHE_H

#include <sys/types.h>

#include <atalk/volume.h>
#include <atalk/directory.h>

/* Maximum size of the dircache hashtable */
#define MAX_POSSIBLE_DIRCACHE_SIZE 131072
#define DIRCACHE_FREE_QUANTUM 256

/* flags for dircache_remove */
#define DIRCACHE      (1 << 0)
#define DIDNAME_INDEX (1 << 1)
#define QUEUE_INDEX   (1 << 2)
#define DIRCACHE_ALL  (DIRCACHE|DIDNAME_INDEX|QUEUE_INDEX)

extern int        dircache_init(int reqsize);
extern int        dircache_add(const struct vol *, struct dir *);
extern void       dircache_remove(const struct vol *, struct dir *, int flag);
extern struct dir *dircache_search_by_did(const struct vol *vol, cnid_t did);
extern struct dir *dircache_search_by_name(const struct vol *, const struct dir *dir, char *name, int len);
extern void       dircache_dump(void);
extern void       log_dircache_stat(void);
#endif /* DIRCACHE_H */
