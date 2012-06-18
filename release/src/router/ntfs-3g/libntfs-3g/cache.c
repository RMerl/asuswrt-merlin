/**
 * cache.c : deal with LRU caches
 *
 * Copyright (c) 2008-2009 Jean-Pierre Andre
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "types.h"
#include "security.h"
#include "cache.h"
#include "misc.h"
#include "logging.h"

/*
 *		General functions to deal with LRU caches
 *
 *	The cached data have to be organized in a structure in which
 *	the first fields must follow a mandatory pattern and further
 *	fields may contain any fixed size data. They are stored in an
 *	LRU list.
 *
 *	A compare function must be provided for finding a wanted entry
 *	in the cache. Another function may be provided for invalidating
 *	an entry to facilitate multiple invalidation.
 *
 *	These functions never return error codes. When there is a
 *	shortage of memory, data is simply not cached.
 *	When there is a hashing bug, hashing is dropped, and sequential
 *	searches are used.
 */

/*
 *		Enter a new hash index, after a new record has been inserted
 *
 *	Do not call when a record has been modified (with no key change)
 */

static void inserthashindex(struct CACHE_HEADER *cache,
			struct CACHED_GENERIC *current)
{
	int h;
	struct HASH_ENTRY *link;
	struct HASH_ENTRY *first;

	if (cache->dohash) {
		h = cache->dohash(current);
		if ((h >= 0) && (h < cache->max_hash)) {
			/* get a free link and insert at top of hash list */
			link = cache->free_hash;
			if (link) {
				cache->free_hash = link->next;
				first = cache->first_hash[h];
				if (first)
					link->next = first;
				else
					link->next = NULL;
				link->entry = current;
				cache->first_hash[h] = link;
			} else {
				ntfs_log_error("No more hash entries,"
						" cache %s hashing dropped\n",
						cache->name);
				cache->dohash = (cache_hash)NULL;
			}
		} else {
			ntfs_log_error("Illegal hash value,"
						" cache %s hashing dropped\n",
						cache->name);
			cache->dohash = (cache_hash)NULL;
		}
	}
}

/*
 *		Drop a hash index when a record is about to be deleted
 */

static void drophashindex(struct CACHE_HEADER *cache,
			const struct CACHED_GENERIC *current, int hash)
{
	struct HASH_ENTRY *link;
	struct HASH_ENTRY *previous;

	if (cache->dohash) {
		if ((hash >= 0) && (hash < cache->max_hash)) {
			/* find the link and unlink */
			link = cache->first_hash[hash];
			previous = (struct HASH_ENTRY*)NULL;
			while (link && (link->entry != current)) {
				previous = link;
				link = link->next;
			}
			if (link) {
				if (previous)
					previous->next = link->next;
				else
					cache->first_hash[hash] = link->next;
				link->next = cache->free_hash;
				cache->free_hash = link;
			} else {
				ntfs_log_error("Bad hash list,"
						" cache %s hashing dropped\n",
						cache->name);
				cache->dohash = (cache_hash)NULL;
			}
		} else {
			ntfs_log_error("Illegal hash value,"
					" cache %s hashing dropped\n",
					cache->name);
			cache->dohash = (cache_hash)NULL;
		}
	}
}

/*
 *		Fetch an entry from cache
 *
 *	returns the cache entry, or NULL if not available
 *	The returned entry may be modified, but not freed
 */

struct CACHED_GENERIC *ntfs_fetch_cache(struct CACHE_HEADER *cache,
		const struct CACHED_GENERIC *wanted, cache_compare compare)
{
	struct CACHED_GENERIC *current;
	struct CACHED_GENERIC *previous;
	struct HASH_ENTRY *link;
	int h;

	current = (struct CACHED_GENERIC*)NULL;
	if (cache) {
		if (cache->dohash) {
			/*
			 * When possible, use the hash table to
			 * locate the entry if present
			 */
			h = cache->dohash(wanted);
		        link = cache->first_hash[h];
			while (link && compare(link->entry, wanted))
				link = link->next;
			if (link)
				current = link->entry;
		}
		if (!cache->dohash) {
			/*
			 * Search sequentially in LRU list if no hash table
			 * or if hashing has just failed
			 */
			current = cache->most_recent_entry;
			while (current
				   && compare(current, wanted)) {
				current = current->next;
				}
		}
		if (current) {
			previous = current->previous;
			cache->hits++;
			if (previous) {
			/*
			 * found and not at head of list, unlink from current
			 * position and relink as head of list
			 */
				previous->next = current->next;
				if (current->next)
					current->next->previous
						= current->previous;
				else
					cache->oldest_entry
						= current->previous;
				current->next = cache->most_recent_entry;
				current->previous
						= (struct CACHED_GENERIC*)NULL;
				cache->most_recent_entry->previous = current;
				cache->most_recent_entry = current;
			}
		}
		cache->reads++;
	}
	return (current);
}

/*
 *		Enter an inode number into cache
 *	returns the cache entry or NULL if not possible
 */

struct CACHED_GENERIC *ntfs_enter_cache(struct CACHE_HEADER *cache,
			const struct CACHED_GENERIC *item,
			cache_compare compare)
{
	struct CACHED_GENERIC *current;
	struct CACHED_GENERIC *before;
	struct HASH_ENTRY *link;
	int h;

	current = (struct CACHED_GENERIC*)NULL;
	if (cache) {
		if (cache->dohash) {
			/*
			 * When possible, use the hash table to
			 * find out whether the entry if present
			 */
			h = cache->dohash(item);
		        link = cache->first_hash[h];
			while (link && compare(link->entry, item))
				link = link->next;
			if (link) {
				current = link->entry;
			}
		}
		if (!cache->dohash) {
			/*
			 * Search sequentially in LRU list to locate the end,
			 * and find out whether the entry is already in list
			 * As we normally go to the end, no statistics is
			 * kept.
		 	 */
			current = cache->most_recent_entry;
			while (current
			   && compare(current, item)) {
				current = current->next;
				}
		}

		if (!current) {
			/*
			 * Not in list, get a free entry or reuse the
			 * last entry, and relink as head of list
			 * Note : we assume at least three entries, so
			 * before, previous and first are different when
			 * an entry is reused.
			 */

			if (cache->free_entry) {
				current = cache->free_entry;
				cache->free_entry = cache->free_entry->next;
				if (item->varsize) {
					current->variable = ntfs_malloc(
						item->varsize);
				} else
					current->variable = (void*)NULL;
				current->varsize = item->varsize;
				if (!cache->oldest_entry)
					cache->oldest_entry = current;
			} else {
				/* reusing the oldest entry */
				current = cache->oldest_entry;
				before = current->previous;
				before->next = (struct CACHED_GENERIC*)NULL;
				if (cache->dohash)
					drophashindex(cache,current,
						cache->dohash(current));
				if (cache->dofree)
					cache->dofree(current);
				cache->oldest_entry = current->previous;
				if (item->varsize) {
					if (current->varsize)
						current->variable = realloc(
							current->variable,
							item->varsize);
					else
						current->variable = ntfs_malloc(
							item->varsize);
				} else {
					if (current->varsize)
						free(current->variable);
					current->variable = (void*)NULL;
				}
				current->varsize = item->varsize;
			}
			current->next = cache->most_recent_entry;
			current->previous = (struct CACHED_GENERIC*)NULL;
			if (cache->most_recent_entry)
				cache->most_recent_entry->previous = current;
			cache->most_recent_entry = current;
			memcpy(current->fixed, item->fixed, cache->fixed_size);
			if (item->varsize) {
				if (current->variable) {
					memcpy(current->variable,
						item->variable, item->varsize);
				} else {
					/*
					 * no more memory for variable part
					 * recycle entry in free list
					 * not an error, just uncacheable
					 */
					cache->most_recent_entry = current->next;
					current->next = cache->free_entry;
					cache->free_entry = current;
					current = (struct CACHED_GENERIC*)NULL;
				}
			} else {
				current->variable = (void*)NULL;
				current->varsize = 0;
			}
			if (cache->dohash && current)
				inserthashindex(cache,current);
		}
		cache->writes++;
	}
	return (current);
}

/*
 *		Invalidate a cache entry
 *	The entry is moved to the free entry list
 *	A specific function may be called for entry deletion
 */

static void do_invalidate(struct CACHE_HEADER *cache,
		struct CACHED_GENERIC *current, int flags)
{
	struct CACHED_GENERIC *previous;

	previous = current->previous;
	if ((flags & CACHE_FREE) && cache->dofree)
		cache->dofree(current);
	/*
	 * Relink into free list
	 */
	if (current->next)
		current->next->previous = current->previous;
	else
		cache->oldest_entry = current->previous;
	if (previous)
		previous->next = current->next;
	else
		cache->most_recent_entry = current->next;
	current->next = cache->free_entry;
	cache->free_entry = current;
	if (current->variable)
		free(current->variable);
	current->varsize = 0;
   }


/*
 *		Invalidate entries in cache
 *
 *	Several entries may have to be invalidated (at least for inodes
 *	associated to directories which have been renamed), a different
 *	compare function may be provided to select entries to invalidate
 *
 *	Returns the number of deleted entries, this can be used by
 *	the caller to signal a cache corruption if the entry was
 *	supposed to be found.
 */

int ntfs_invalidate_cache(struct CACHE_HEADER *cache,
		const struct CACHED_GENERIC *item, cache_compare compare,
		int flags)
{
	struct CACHED_GENERIC *current;
	struct CACHED_GENERIC *previous;
	struct CACHED_GENERIC *next;
	struct HASH_ENTRY *link;
	int count;
	int h;

	current = (struct CACHED_GENERIC*)NULL;
	count = 0;
	if (cache) {
		if (!(flags & CACHE_NOHASH) && cache->dohash) {
			/*
			 * When possible, use the hash table to
			 * find out whether the entry if present
			 */
			h = cache->dohash(item);
		        link = cache->first_hash[h];
			while (link) {
				if (compare(link->entry, item))
					link = link->next;
				else {
					current = link->entry;
					link = link->next;
					if (current) {
						drophashindex(cache,current,h);
						do_invalidate(cache,
							current,flags);
						count++;
					}
				}
			}
		}
		if ((flags & CACHE_NOHASH) || !cache->dohash) {
				/*
				 * Search sequentially in LRU list
				 */
			current = cache->most_recent_entry;
			previous = (struct CACHED_GENERIC*)NULL;
			while (current) {
				if (!compare(current, item)) {
					next = current->next;
					if (cache->dohash)
						drophashindex(cache,current,
						    cache->dohash(current));
					do_invalidate(cache,current,flags);
					current = next;
					count++;
				} else {
					previous = current;
					current = current->next;
				}
			}
		}
	}
	return (count);
}

int ntfs_remove_cache(struct CACHE_HEADER *cache,
		struct CACHED_GENERIC *item, int flags)
{
	int count;

	count = 0;
	if (cache) {
		if (cache->dohash)
			drophashindex(cache,item,cache->dohash(item));
		do_invalidate(cache,item,flags);
		count++;
	}
	return (count);
}

/*
 *		Free memory allocated to a cache
 */

static void ntfs_free_cache(struct CACHE_HEADER *cache)
{
	struct CACHED_GENERIC *entry;

	if (cache) {
		for (entry=cache->most_recent_entry; entry; entry=entry->next) {
			if (cache->dofree)
				cache->dofree(entry);
			if (entry->variable)
				free(entry->variable);
		}
		free(cache);
	}
}

/*
 *		Create a cache
 *
 *	Returns the cache header, or NULL if the cache could not be created
 */

static struct CACHE_HEADER *ntfs_create_cache(const char *name,
			cache_free dofree, cache_hash dohash,
			int full_item_size,
			int item_count, int max_hash)
{
	struct CACHE_HEADER *cache;
	struct CACHED_GENERIC *pc;
	struct CACHED_GENERIC *qc;
	struct HASH_ENTRY *ph;
	struct HASH_ENTRY *qh;
	struct HASH_ENTRY **px;
	size_t size;
	int i;

	size = sizeof(struct CACHE_HEADER) + item_count*full_item_size;
	if (max_hash)
		size += item_count*sizeof(struct HASH_ENTRY)
			 + max_hash*sizeof(struct HASH_ENTRY*);
	cache = (struct CACHE_HEADER*)ntfs_malloc(size);
	if (cache) {
				/* header */
		cache->name = name;
		cache->dofree = dofree;
		if (dohash && max_hash) {
			cache->dohash = dohash;
			cache->max_hash = max_hash;
		} else {
			cache->dohash = (cache_hash)NULL;
			cache->max_hash = 0;
		}
		cache->fixed_size = full_item_size - sizeof(struct CACHED_GENERIC);
		cache->reads = 0;
		cache->writes = 0;
		cache->hits = 0;
		/* chain the data entries, and mark an invalid entry */
		cache->most_recent_entry = (struct CACHED_GENERIC*)NULL;
		cache->oldest_entry = (struct CACHED_GENERIC*)NULL;
		cache->free_entry = &cache->entry[0];
		pc = &cache->entry[0];
		for (i=0; i<(item_count - 1); i++) {
			qc = (struct CACHED_GENERIC*)((char*)pc
							 + full_item_size);
			pc->next = qc;
			pc->variable = (void*)NULL;
			pc->varsize = 0;
			pc = qc;
		}
			/* special for the last entry */
		pc->next =  (struct CACHED_GENERIC*)NULL;
		pc->variable = (void*)NULL;
		pc->varsize = 0;

		if (max_hash) {
				/* chain the hash entries */
			ph = (struct HASH_ENTRY*)(((char*)pc) + full_item_size);
			cache->free_hash = ph;
			for (i=0; i<(item_count - 1); i++) {
				qh = &ph[1];
				ph->next = qh;
				ph = qh;
			}
				/* special for the last entry */
			if (item_count) {
				ph->next =  (struct HASH_ENTRY*)NULL;
			}
				/* create and initialize the hash indexes */
			px = (struct HASH_ENTRY**)&ph[1];
			cache->first_hash = px;
			for (i=0; i<max_hash; i++)
				px[i] = (struct HASH_ENTRY*)NULL;
		} else {
			cache->free_hash = (struct HASH_ENTRY*)NULL;
			cache->first_hash = (struct HASH_ENTRY**)NULL;
		}
	}
	return (cache);
}

/*
 *		Create all LRU caches
 *
 *	No error return, if creation is not possible, cacheing will
 *	just be not available
 */

void ntfs_create_lru_caches(ntfs_volume *vol)
{
#if CACHE_INODE_SIZE
		 /* inode cache */
	vol->xinode_cache = ntfs_create_cache("inode",(cache_free)NULL,
		ntfs_dir_inode_hash, sizeof(struct CACHED_INODE),
		CACHE_INODE_SIZE, 2*CACHE_INODE_SIZE);
#endif
#if CACHE_NIDATA_SIZE
		 /* idata cache */
	vol->nidata_cache = ntfs_create_cache("nidata",
		ntfs_inode_nidata_free, ntfs_inode_nidata_hash,
		sizeof(struct CACHED_NIDATA),
		CACHE_NIDATA_SIZE, 2*CACHE_NIDATA_SIZE);
#endif
#if CACHE_LOOKUP_SIZE
		 /* lookup cache */
	vol->lookup_cache = ntfs_create_cache("lookup",
		(cache_free)NULL, ntfs_dir_lookup_hash,
		sizeof(struct CACHED_LOOKUP),
		CACHE_LOOKUP_SIZE, 2*CACHE_LOOKUP_SIZE);
#endif
	vol->securid_cache = ntfs_create_cache("securid",(cache_free)NULL,
		(cache_hash)NULL,sizeof(struct CACHED_SECURID), CACHE_SECURID_SIZE, 0);
#if CACHE_LEGACY_SIZE
	vol->legacy_cache = ntfs_create_cache("legacy",(cache_free)NULL,
		(cache_hash)NULL, sizeof(struct CACHED_PERMISSIONS_LEGACY), CACHE_LEGACY_SIZE, 0);
#endif
}

/*
 *		Free all LRU caches
 */

void ntfs_free_lru_caches(ntfs_volume *vol)
{
#if CACHE_INODE_SIZE
	ntfs_free_cache(vol->xinode_cache);
#endif
#if CACHE_NIDATA_SIZE
	ntfs_free_cache(vol->nidata_cache);
#endif
#if CACHE_LOOKUP_SIZE
	ntfs_free_cache(vol->lookup_cache);
#endif
	ntfs_free_cache(vol->securid_cache);
#if CACHE_LEGACY_SIZE
	ntfs_free_cache(vol->legacy_cache);
#endif
}
