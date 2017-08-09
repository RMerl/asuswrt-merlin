/*
   Unix SMB/CIFS implementation.
   In-memory cache
   Copyright (C) Volker Lendecke 2007-2008

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __MEMCACHE_H__
#define __MEMCACHE_H__

#include "includes.h"

struct memcache;

/*
 * A memcache can store different subkeys with overlapping keys, the
 * memcache_number becomes part of the key. Feel free to add caches of your
 * own here.
 *
 * If you add talloc type caches, also note this in the switch statement in
 * memcache_is_talloc().
 */

enum memcache_number {
	STAT_CACHE,
	UID_SID_CACHE,
	SID_UID_CACHE,
	GID_SID_CACHE,
	SID_GID_CACHE,
	GETWD_CACHE,
	GETPWNAM_CACHE,		/* talloc */
	MANGLE_HASH2_CACHE,
	PDB_GETPWSID_CACHE,	/* talloc */
	SINGLETON_CACHE_TALLOC,	/* talloc */
	SINGLETON_CACHE
};

/*
 * Create a memcache structure. max_size is in bytes, if you set it 0 it will
 * not forget anything.
 */

struct memcache *memcache_init(TALLOC_CTX *mem_ctx, size_t max_size);

/*
 * If you set this global memcache, use it as the default cache when NULL is
 * passed to the memcache functions below. This is a workaround for many
 * situations where passing the cache everywhere would be a big hassle.
 */

void memcache_set_global(struct memcache *cache);

/*
 * Add a data blob to the cache
 */

void memcache_add(struct memcache *cache, enum memcache_number n,
		  DATA_BLOB key, DATA_BLOB value);

/*
 * Add a talloc object to the cache. The difference to memcache_add() is that
 * when the objects is to be discared, talloc_free is called for it. Also
 * talloc_move() ownership of the object to the cache.
 *
 * Please note that the current implementation has a fixed relationship
 * between what cache subtypes store talloc objects and which ones store plain
 * blobs. We can fix this, but for now we don't have a mixed use of blobs vs
 * talloc objects in the cache types.
 */

void memcache_add_talloc(struct memcache *cache, enum memcache_number n,
			 DATA_BLOB key, void *ptr);

/*
 * Delete an object from the cache
 */

void memcache_delete(struct memcache *cache, enum memcache_number n,
		     DATA_BLOB key);

/*
 * Look up an object from the cache. Memory still belongs to the cache, so
 * make a copy of it if needed.
 */

bool memcache_lookup(struct memcache *cache, enum memcache_number n,
		     DATA_BLOB key, DATA_BLOB *value);

/*
 * Look up an object from the cache. Memory still belongs to the cache, so
 * make a copy of it if needed.
 */

void *memcache_lookup_talloc(struct memcache *cache, enum memcache_number n,
			     DATA_BLOB key);

/*
 * Flush a complete cache subset.
 */

void memcache_flush(struct memcache *cache, enum memcache_number n);

#endif
