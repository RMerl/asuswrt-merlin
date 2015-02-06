/* Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file mempool.h
 * \brief Headers for mempool.c
 **/

#ifndef TOR_MEMPOOL_H
#define TOR_MEMPOOL_H

/** A memory pool is a context in which a large number of fixed-sized
* objects can be allocated efficiently.  See mempool.c for implementation
* details. */
typedef struct mp_pool_t mp_pool_t;

void *mp_pool_get(mp_pool_t *pool);
void mp_pool_release(void *item);
mp_pool_t *mp_pool_new(size_t item_size, size_t chunk_capacity);
void mp_pool_clean(mp_pool_t *pool, int n_to_keep, int keep_recently_used);
void mp_pool_destroy(mp_pool_t *pool);
void mp_pool_assert_ok(mp_pool_t *pool);
void mp_pool_log_status(mp_pool_t *pool, int severity);

#define MP_POOL_ITEM_OVERHEAD (sizeof(void*))

#define MEMPOOL_STATS

#ifdef MEMPOOL_PRIVATE
/* These declarations are only used by mempool.c and test.c */

struct mp_pool_t {
  /** Doubly-linked list of chunks in which no items have been allocated.
   * The front of the list is the most recently emptied chunk. */
  struct mp_chunk_t *empty_chunks;
  /** Doubly-linked list of chunks in which some items have been allocated,
   * but which are not yet full. The front of the list is the chunk that has
   * most recently been modified. */
  struct mp_chunk_t *used_chunks;
  /** Doubly-linked list of chunks in which no more items can be allocated.
   * The front of the list is the chunk that has most recently become full. */
  struct mp_chunk_t *full_chunks;
  /** Length of <b>empty_chunks</b>. */
  int n_empty_chunks;
  /** Lowest value of <b>empty_chunks</b> since last call to
   * mp_pool_clean(-1). */
  int min_empty_chunks;
  /** Size of each chunk (in items). */
  int new_chunk_capacity;
  /** Size to allocate for each item, including overhead and alignment
   * padding. */
  size_t item_alloc_size;
#ifdef MEMPOOL_STATS
  /** Total number of items allocated ever. */
  uint64_t total_items_allocated;
  /** Total number of chunks allocated ever. */
  uint64_t total_chunks_allocated;
  /** Total number of chunks freed ever. */
  uint64_t total_chunks_freed;
#endif
};
#endif

#endif

