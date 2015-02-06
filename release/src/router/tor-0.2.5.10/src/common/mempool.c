/* Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */
#if 1
/* Tor dependencies */
#include "orconfig.h"
#endif

#include <stdlib.h>
#include <string.h>
#include "torint.h"
#include "crypto.h"
#define MEMPOOL_PRIVATE
#include "mempool.h"

/* OVERVIEW:
 *
 *     This is an implementation of memory pools for Tor cells.  It may be
 *     useful for you too.
 *
 *     Generally, a memory pool is an allocation strategy optimized for large
 *     numbers of identically-sized objects.  Rather than the elaborate arena
 *     and coalescing strategies you need to get good performance for a
 *     general-purpose malloc(), pools use a series of large memory "chunks",
 *     each of which is carved into a bunch of smaller "items" or
 *     "allocations".
 *
 *     To get decent performance, you need to:
 *        - Minimize the number of times you hit the underlying allocator.
 *        - Try to keep accesses as local in memory as possible.
 *        - Try to keep the common case fast.
 *
 *     Our implementation uses three lists of chunks per pool.  Each chunk can
 *     be either "full" (no more room for items); "empty" (no items); or
 *     "used" (not full, not empty).  There are independent doubly-linked
 *     lists for each state.
 *
 * CREDIT:
 *
 *     I wrote this after looking at 3 or 4 other pooling allocators, but
 *     without copying.  The strategy this most resembles (which is funny,
 *     since that's the one I looked at longest ago) is the pool allocator
 *     underlying Python's obmalloc code.  Major differences from obmalloc's
 *     pools are:
 *       - We don't even try to be threadsafe.
 *       - We only handle objects of one size.
 *       - Our list of empty chunks is doubly-linked, not singly-linked.
 *         (This could change pretty easily; it's only doubly-linked for
 *         consistency.)
 *       - We keep a list of full chunks (so we can have a "nuke everything"
 *         function).  Obmalloc's pools leave full chunks to float unanchored.
 *
 * LIMITATIONS:
 *   - Not even slightly threadsafe.
 *   - Likes to have lots of items per chunks.
 *   - One pointer overhead per allocated thing.  (The alternative is
 *     something like glib's use of an RB-tree to keep track of what
 *     chunk any given piece of memory is in.)
 *   - Only aligns allocated things to void* level: redefine ALIGNMENT_TYPE
 *     if you need doubles.
 *   - Could probably be optimized a bit; the representation contains
 *     a bit more info than it really needs to have.
 */

#if 1
/* Tor dependencies */
#include "util.h"
#include "compat.h"
#include "torlog.h"
#define ALLOC(x) tor_malloc(x)
#define FREE(x) tor_free(x)
#define ASSERT(x) tor_assert(x)
#undef ALLOC_CAN_RETURN_NULL
#define TOR
/* End Tor dependencies */
#else
/* If you're not building this as part of Tor, you'll want to define the
 * following macros.  For now, these should do as defaults.
 */
#include <assert.h>
#define PREDICT_UNLIKELY(x) (x)
#define PREDICT_LIKELY(x) (x)
#define ALLOC(x) malloc(x)
#define FREE(x) free(x)
#define STRUCT_OFFSET(tp, member)                       \
  ((off_t) (((char*)&((tp*)0)->member)-(char*)0))
#define ASSERT(x) assert(x)
#define ALLOC_CAN_RETURN_NULL
#endif

/* Tuning parameters */
/** Largest type that we need to ensure returned memory items are aligned to.
 * Change this to "double" if we need to be safe for structs with doubles. */
#define ALIGNMENT_TYPE void *
/** Increment that we need to align allocated. */
#define ALIGNMENT sizeof(ALIGNMENT_TYPE)
/** Largest memory chunk that we should allocate. */
#define MAX_CHUNK (8*(1L<<20))
/** Smallest memory chunk size that we should allocate. */
#define MIN_CHUNK 4096

typedef struct mp_allocated_t mp_allocated_t;
typedef struct mp_chunk_t mp_chunk_t;

/** Holds a single allocated item, allocated as part of a chunk. */
struct mp_allocated_t {
  /** The chunk that this item is allocated in.  This adds overhead to each
   * allocated item, thus making this implementation inappropriate for
   * very small items. */
  mp_chunk_t *in_chunk;
  union {
    /** If this item is free, the next item on the free list. */
    mp_allocated_t *next_free;
    /** If this item is not free, the actual memory contents of this item.
     * (Not actual size.) */
    char mem[1];
    /** An extra element to the union to insure correct alignment. */
    ALIGNMENT_TYPE dummy_;
  } u;
};

/** 'Magic' value used to detect memory corruption. */
#define MP_CHUNK_MAGIC 0x09870123

/** A chunk of memory.  Chunks come from malloc; we use them  */
struct mp_chunk_t {
  unsigned long magic; /**< Must be MP_CHUNK_MAGIC if this chunk is valid. */
  mp_chunk_t *next; /**< The next free, used, or full chunk in sequence. */
  mp_chunk_t *prev; /**< The previous free, used, or full chunk in sequence. */
  mp_pool_t *pool; /**< The pool that this chunk is part of. */
  /** First free item in the freelist for this chunk.  Note that this may be
   * NULL even if this chunk is not at capacity: if so, the free memory at
   * next_mem has not yet been carved into items.
   */
  mp_allocated_t *first_free;
  int n_allocated; /**< Number of currently allocated items in this chunk. */
  int capacity; /**< Number of items that can be fit into this chunk. */
  size_t mem_size; /**< Number of usable bytes in mem. */
  char *next_mem; /**< Pointer into part of <b>mem</b> not yet carved up. */
  char mem[FLEXIBLE_ARRAY_MEMBER]; /**< Storage for this chunk. */
};

/** Number of extra bytes needed beyond mem_size to allocate a chunk. */
#define CHUNK_OVERHEAD STRUCT_OFFSET(mp_chunk_t, mem[0])

/** Given a pointer to a mp_allocated_t, return a pointer to the memory
 * item it holds. */
#define A2M(a) (&(a)->u.mem)
/** Given a pointer to a memory_item_t, return a pointer to its enclosing
 * mp_allocated_t. */
#define M2A(p) ( ((char*)p) - STRUCT_OFFSET(mp_allocated_t, u.mem) )

#ifdef ALLOC_CAN_RETURN_NULL
/** If our ALLOC() macro can return NULL, check whether <b>x</b> is NULL,
 * and if so, return NULL. */
#define CHECK_ALLOC(x)                           \
  if (PREDICT_UNLIKELY(!x)) { return NULL; }
#else
/** If our ALLOC() macro can't return NULL, do nothing. */
#define CHECK_ALLOC(x)
#endif

/** Helper: Allocate and return a new memory chunk for <b>pool</b>.  Does not
 * link the chunk into any list. */
static mp_chunk_t *
mp_chunk_new(mp_pool_t *pool)
{
  size_t sz = pool->new_chunk_capacity * pool->item_alloc_size;
  mp_chunk_t *chunk = ALLOC(CHUNK_OVERHEAD + sz);

#ifdef MEMPOOL_STATS
  ++pool->total_chunks_allocated;
#endif
  CHECK_ALLOC(chunk);
  memset(chunk, 0, sizeof(mp_chunk_t)); /* Doesn't clear the whole thing. */
  chunk->magic = MP_CHUNK_MAGIC;
  chunk->capacity = pool->new_chunk_capacity;
  chunk->mem_size = sz;
  chunk->next_mem = chunk->mem;
  chunk->pool = pool;
  return chunk;
}

/** Take a <b>chunk</b> that has just been allocated or removed from
 * <b>pool</b>'s empty chunk list, and add it to the head of the used chunk
 * list. */
static INLINE void
add_newly_used_chunk_to_used_list(mp_pool_t *pool, mp_chunk_t *chunk)
{
  chunk->next = pool->used_chunks;
  if (chunk->next)
    chunk->next->prev = chunk;
  pool->used_chunks = chunk;
  ASSERT(!chunk->prev);
}

/** Return a newly allocated item from <b>pool</b>. */
void *
mp_pool_get(mp_pool_t *pool)
{
  mp_chunk_t *chunk;
  mp_allocated_t *allocated;

  if (PREDICT_LIKELY(pool->used_chunks != NULL)) {
    /* Common case: there is some chunk that is neither full nor empty.  Use
     * that one. (We can't use the full ones, obviously, and we should fill
     * up the used ones before we start on any empty ones. */
    chunk = pool->used_chunks;

  } else if (pool->empty_chunks) {
    /* We have no used chunks, but we have an empty chunk that we haven't
     * freed yet: use that.  (We pull from the front of the list, which should
     * get us the most recently emptied chunk.) */
    chunk = pool->empty_chunks;

    /* Remove the chunk from the empty list. */
    pool->empty_chunks = chunk->next;
    if (chunk->next)
      chunk->next->prev = NULL;

    /* Put the chunk on the 'used' list*/
    add_newly_used_chunk_to_used_list(pool, chunk);

    ASSERT(!chunk->prev);
    --pool->n_empty_chunks;
    if (pool->n_empty_chunks < pool->min_empty_chunks)
      pool->min_empty_chunks = pool->n_empty_chunks;
  } else {
    /* We have no used or empty chunks: allocate a new chunk. */
    chunk = mp_chunk_new(pool);
    CHECK_ALLOC(chunk);

    /* Add the new chunk to the used list. */
    add_newly_used_chunk_to_used_list(pool, chunk);
  }

  ASSERT(chunk->n_allocated < chunk->capacity);

  if (chunk->first_free) {
    /* If there's anything on the chunk's freelist, unlink it and use it. */
    allocated = chunk->first_free;
    chunk->first_free = allocated->u.next_free;
    allocated->u.next_free = NULL; /* For debugging; not really needed. */
    ASSERT(allocated->in_chunk == chunk);
  } else {
    /* Otherwise, the chunk had better have some free space left on it. */
    ASSERT(chunk->next_mem + pool->item_alloc_size <=
           chunk->mem + chunk->mem_size);

    /* Good, it did.  Let's carve off a bit of that free space, and use
     * that. */
    allocated = (void*)chunk->next_mem;
    chunk->next_mem += pool->item_alloc_size;
    allocated->in_chunk = chunk;
    allocated->u.next_free = NULL; /* For debugging; not really needed. */
  }

  ++chunk->n_allocated;
#ifdef MEMPOOL_STATS
  ++pool->total_items_allocated;
#endif

  if (PREDICT_UNLIKELY(chunk->n_allocated == chunk->capacity)) {
    /* This chunk just became full. */
    ASSERT(chunk == pool->used_chunks);
    ASSERT(chunk->prev == NULL);

    /* Take it off the used list. */
    pool->used_chunks = chunk->next;
    if (chunk->next)
      chunk->next->prev = NULL;

    /* Put it on the full list. */
    chunk->next = pool->full_chunks;
    if (chunk->next)
      chunk->next->prev = chunk;
    pool->full_chunks = chunk;
  }
  /* And return the memory portion of the mp_allocated_t. */
  return A2M(allocated);
}

/** Return an allocated memory item to its memory pool. */
void
mp_pool_release(void *item)
{
  mp_allocated_t *allocated = (void*) M2A(item);
  mp_chunk_t *chunk = allocated->in_chunk;

  ASSERT(chunk);
  ASSERT(chunk->magic == MP_CHUNK_MAGIC);
  ASSERT(chunk->n_allocated > 0);

  allocated->u.next_free = chunk->first_free;
  chunk->first_free = allocated;

  if (PREDICT_UNLIKELY(chunk->n_allocated == chunk->capacity)) {
    /* This chunk was full and is about to be used. */
    mp_pool_t *pool = chunk->pool;
    /* unlink from the full list  */
    if (chunk->prev)
      chunk->prev->next = chunk->next;
    if (chunk->next)
      chunk->next->prev = chunk->prev;
    if (chunk == pool->full_chunks)
      pool->full_chunks = chunk->next;

    /* link to the used list. */
    chunk->next = pool->used_chunks;
    chunk->prev = NULL;
    if (chunk->next)
      chunk->next->prev = chunk;
    pool->used_chunks = chunk;
  } else if (PREDICT_UNLIKELY(chunk->n_allocated == 1)) {
    /* This was used and is about to be empty. */
    mp_pool_t *pool = chunk->pool;

    /* Unlink from the used list */
    if (chunk->prev)
      chunk->prev->next = chunk->next;
    if (chunk->next)
      chunk->next->prev = chunk->prev;
    if (chunk == pool->used_chunks)
      pool->used_chunks = chunk->next;

    /* Link to the empty list */
    chunk->next = pool->empty_chunks;
    chunk->prev = NULL;
    if (chunk->next)
      chunk->next->prev = chunk;
    pool->empty_chunks = chunk;

    /* Reset the guts of this chunk to defragment it, in case it gets
     * used again. */
    chunk->first_free = NULL;
    chunk->next_mem = chunk->mem;

    ++pool->n_empty_chunks;
  }
  --chunk->n_allocated;
}

/** Allocate a new memory pool to hold items of size <b>item_size</b>. We'll
 * try to fit about <b>chunk_capacity</b> bytes in each chunk. */
mp_pool_t *
mp_pool_new(size_t item_size, size_t chunk_capacity)
{
  mp_pool_t *pool;
  size_t alloc_size, new_chunk_cap;

  tor_assert(item_size < SIZE_T_CEILING);
  tor_assert(chunk_capacity < SIZE_T_CEILING);
  tor_assert(SIZE_T_CEILING / item_size > chunk_capacity);

  pool = ALLOC(sizeof(mp_pool_t));
  CHECK_ALLOC(pool);
  memset(pool, 0, sizeof(mp_pool_t));

  /* First, we figure out how much space to allow per item.  We'll want to
   * use make sure we have enough for the overhead plus the item size. */
  alloc_size = (size_t)(STRUCT_OFFSET(mp_allocated_t, u.mem) + item_size);
  /* If the item_size is less than sizeof(next_free), we need to make
   * the allocation bigger. */
  if (alloc_size < sizeof(mp_allocated_t))
    alloc_size = sizeof(mp_allocated_t);

  /* If we're not an even multiple of ALIGNMENT, round up. */
  if (alloc_size % ALIGNMENT) {
    alloc_size = alloc_size + ALIGNMENT - (alloc_size % ALIGNMENT);
  }
  if (alloc_size < ALIGNMENT)
    alloc_size = ALIGNMENT;
  ASSERT((alloc_size % ALIGNMENT) == 0);

  /* Now we figure out how many items fit in each chunk.  We need to fit at
   * least 2 items per chunk. No chunk can be more than MAX_CHUNK bytes long,
   * or less than MIN_CHUNK. */
  if (chunk_capacity > MAX_CHUNK)
    chunk_capacity = MAX_CHUNK;
  /* Try to be around a power of 2 in size, since that's what allocators like
   * handing out. 512K-1 byte is a lot better than 512K+1 byte. */
  chunk_capacity = (size_t) round_to_power_of_2(chunk_capacity);
  while (chunk_capacity < alloc_size * 2 + CHUNK_OVERHEAD)
    chunk_capacity *= 2;
  if (chunk_capacity < MIN_CHUNK)
    chunk_capacity = MIN_CHUNK;

  new_chunk_cap = (chunk_capacity-CHUNK_OVERHEAD) / alloc_size;
  tor_assert(new_chunk_cap < INT_MAX);
  pool->new_chunk_capacity = (int)new_chunk_cap;

  pool->item_alloc_size = alloc_size;

  log_debug(LD_MM, "Capacity is %lu, item size is %lu, alloc size is %lu",
            (unsigned long)pool->new_chunk_capacity,
            (unsigned long)pool->item_alloc_size,
            (unsigned long)(pool->new_chunk_capacity*pool->item_alloc_size));

  return pool;
}

/** Helper function for qsort: used to sort pointers to mp_chunk_t into
 * descending order of fullness. */
static int
mp_pool_sort_used_chunks_helper(const void *_a, const void *_b)
{
  mp_chunk_t *a = *(mp_chunk_t**)_a;
  mp_chunk_t *b = *(mp_chunk_t**)_b;
  return b->n_allocated - a->n_allocated;
}

/** Sort the used chunks in <b>pool</b> into descending order of fullness,
 * so that we preferentially fill up mostly full chunks before we make
 * nearly empty chunks less nearly empty. */
static void
mp_pool_sort_used_chunks(mp_pool_t *pool)
{
  int i, n=0, inverted=0;
  mp_chunk_t **chunks, *chunk;
  for (chunk = pool->used_chunks; chunk; chunk = chunk->next) {
    ++n;
    if (chunk->next && chunk->next->n_allocated > chunk->n_allocated)
      ++inverted;
  }
  if (!inverted)
    return;
  //printf("Sort %d/%d\n",inverted,n);
  chunks = ALLOC(sizeof(mp_chunk_t *)*n);
#ifdef ALLOC_CAN_RETURN_NULL
  if (PREDICT_UNLIKELY(!chunks)) return;
#endif
  for (i=0,chunk = pool->used_chunks; chunk; chunk = chunk->next)
    chunks[i++] = chunk;
  qsort(chunks, n, sizeof(mp_chunk_t *), mp_pool_sort_used_chunks_helper);
  pool->used_chunks = chunks[0];
  chunks[0]->prev = NULL;
  for (i=1;i<n;++i) {
    chunks[i-1]->next = chunks[i];
    chunks[i]->prev = chunks[i-1];
  }
  chunks[n-1]->next = NULL;
  FREE(chunks);
  mp_pool_assert_ok(pool);
}

/** If there are more than <b>n</b> empty chunks in <b>pool</b>, free the
 * excess ones that have been empty for the longest. If
 * <b>keep_recently_used</b> is true, do not free chunks unless they have been
 * empty since the last call to this function.
 **/
void
mp_pool_clean(mp_pool_t *pool, int n_to_keep, int keep_recently_used)
{
  mp_chunk_t *chunk, **first_to_free;

  mp_pool_sort_used_chunks(pool);
  ASSERT(n_to_keep >= 0);

  if (keep_recently_used) {
    int n_recently_used = pool->n_empty_chunks - pool->min_empty_chunks;
    if (n_to_keep < n_recently_used)
      n_to_keep = n_recently_used;
  }

  ASSERT(n_to_keep >= 0);

  first_to_free = &pool->empty_chunks;
  while (*first_to_free && n_to_keep > 0) {
    first_to_free = &(*first_to_free)->next;
    --n_to_keep;
  }
  if (!*first_to_free) {
    pool->min_empty_chunks = pool->n_empty_chunks;
    return;
  }

  chunk = *first_to_free;
  while (chunk) {
    mp_chunk_t *next = chunk->next;
    chunk->magic = 0xdeadbeef;
    FREE(chunk);
#ifdef MEMPOOL_STATS
    ++pool->total_chunks_freed;
#endif
    --pool->n_empty_chunks;
    chunk = next;
  }

  pool->min_empty_chunks = pool->n_empty_chunks;
  *first_to_free = NULL;
}

/** Helper: Given a list of chunks, free all the chunks in the list. */
static void
destroy_chunks(mp_chunk_t *chunk)
{
  mp_chunk_t *next;
  while (chunk) {
    chunk->magic = 0xd3adb33f;
    next = chunk->next;
    FREE(chunk);
    chunk = next;
  }
}

/** Free all space held in <b>pool</b>  This makes all pointers returned from
 * mp_pool_get(<b>pool</b>) invalid. */
void
mp_pool_destroy(mp_pool_t *pool)
{
  destroy_chunks(pool->empty_chunks);
  destroy_chunks(pool->used_chunks);
  destroy_chunks(pool->full_chunks);
  memwipe(pool, 0xe0, sizeof(mp_pool_t));
  FREE(pool);
}

/** Helper: make sure that a given chunk list is not corrupt. */
static int
assert_chunks_ok(mp_pool_t *pool, mp_chunk_t *chunk, int empty, int full)
{
  mp_allocated_t *allocated;
  int n = 0;
  if (chunk)
    ASSERT(chunk->prev == NULL);

  while (chunk) {
    n++;
    ASSERT(chunk->magic == MP_CHUNK_MAGIC);
    ASSERT(chunk->pool == pool);
    for (allocated = chunk->first_free; allocated;
         allocated = allocated->u.next_free) {
      ASSERT(allocated->in_chunk == chunk);
    }
    if (empty)
      ASSERT(chunk->n_allocated == 0);
    else if (full)
      ASSERT(chunk->n_allocated == chunk->capacity);
    else
      ASSERT(chunk->n_allocated > 0 && chunk->n_allocated < chunk->capacity);

    ASSERT(chunk->capacity == pool->new_chunk_capacity);

    ASSERT(chunk->mem_size ==
           pool->new_chunk_capacity * pool->item_alloc_size);

    ASSERT(chunk->next_mem >= chunk->mem &&
           chunk->next_mem <= chunk->mem + chunk->mem_size);

    if (chunk->next)
      ASSERT(chunk->next->prev == chunk);

    chunk = chunk->next;
  }
  return n;
}

/** Fail with an assertion if <b>pool</b> is not internally consistent. */
void
mp_pool_assert_ok(mp_pool_t *pool)
{
  int n_empty;

  n_empty = assert_chunks_ok(pool, pool->empty_chunks, 1, 0);
  assert_chunks_ok(pool, pool->full_chunks, 0, 1);
  assert_chunks_ok(pool, pool->used_chunks, 0, 0);

  ASSERT(pool->n_empty_chunks == n_empty);
}

#ifdef TOR
/** Dump information about <b>pool</b>'s memory usage to the Tor log at level
 * <b>severity</b>. */
/*FFFF uses Tor logging functions. */
void
mp_pool_log_status(mp_pool_t *pool, int severity)
{
  uint64_t bytes_used = 0;
  uint64_t bytes_allocated = 0;
  uint64_t bu = 0, ba = 0;
  mp_chunk_t *chunk;
  int n_full = 0, n_used = 0;

  ASSERT(pool);

  for (chunk = pool->empty_chunks; chunk; chunk = chunk->next) {
    bytes_allocated += chunk->mem_size;
  }
  log_fn(severity, LD_MM, U64_FORMAT" bytes in %d empty chunks",
         U64_PRINTF_ARG(bytes_allocated), pool->n_empty_chunks);
  for (chunk = pool->used_chunks; chunk; chunk = chunk->next) {
    ++n_used;
    bu += chunk->n_allocated * pool->item_alloc_size;
    ba += chunk->mem_size;
    log_fn(severity, LD_MM, "   used chunk: %d items allocated",
           chunk->n_allocated);
  }
  log_fn(severity, LD_MM, U64_FORMAT"/"U64_FORMAT
         " bytes in %d partially full chunks",
         U64_PRINTF_ARG(bu), U64_PRINTF_ARG(ba), n_used);
  bytes_used += bu;
  bytes_allocated += ba;
  bu = ba = 0;
  for (chunk = pool->full_chunks; chunk; chunk = chunk->next) {
    ++n_full;
    bu += chunk->n_allocated * pool->item_alloc_size;
    ba += chunk->mem_size;
  }
  log_fn(severity, LD_MM, U64_FORMAT"/"U64_FORMAT
         " bytes in %d full chunks",
         U64_PRINTF_ARG(bu), U64_PRINTF_ARG(ba), n_full);
  bytes_used += bu;
  bytes_allocated += ba;

  log_fn(severity, LD_MM, "Total: "U64_FORMAT"/"U64_FORMAT" bytes allocated "
         "for cell pools are full.",
         U64_PRINTF_ARG(bytes_used), U64_PRINTF_ARG(bytes_allocated));

#ifdef MEMPOOL_STATS
  log_fn(severity, LD_MM, U64_FORMAT" cell allocations ever; "
         U64_FORMAT" chunk allocations ever; "
         U64_FORMAT" chunk frees ever.",
         U64_PRINTF_ARG(pool->total_items_allocated),
         U64_PRINTF_ARG(pool->total_chunks_allocated),
         U64_PRINTF_ARG(pool->total_chunks_freed));
#endif
}
#endif

