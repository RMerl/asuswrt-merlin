/* Copyright (c) 2008-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/** \file memarea.c
 * \brief Implementation for memarea_t, an allocator for allocating lots of
 * small objects that will be freed all at once.
 */

#include "orconfig.h"
#include <stdlib.h>
#include "memarea.h"
#include "util.h"
#include "compat.h"
#include "torlog.h"

/** If true, we try to detect any attempts to write beyond the length of a
 * memarea. */
#define USE_SENTINELS

/** All returned pointers should be aligned to the nearest multiple of this
 * value. */
#define MEMAREA_ALIGN SIZEOF_VOID_P

#if MEMAREA_ALIGN == 4
#define MEMAREA_ALIGN_MASK 3lu
#elif MEMAREA_ALIGN == 8
#define MEMAREA_ALIGN_MASK 7lu
#else
#error "void* is neither 4 nor 8 bytes long. I don't know how to align stuff."
#endif

#if defined(__GNUC__) && defined(FLEXIBLE_ARRAY_MEMBER)
#define USE_ALIGNED_ATTRIBUTE
#define U_MEM mem
#else
#define U_MEM u.mem
#endif

#ifdef USE_SENTINELS
/** Magic value that we stick at the end of a memarea so we can make sure
 * there are no run-off-the-end bugs. */
#define SENTINEL_VAL 0x90806622u
/** How many bytes per area do we devote to the sentinel? */
#define SENTINEL_LEN sizeof(uint32_t)
/** Given a mem_area_chunk_t with SENTINEL_LEN extra bytes allocated at the
 * end, set those bytes. */
#define SET_SENTINEL(chunk)                                     \
  STMT_BEGIN                                                    \
  set_uint32( &(chunk)->U_MEM[chunk->mem_size], SENTINEL_VAL ); \
  STMT_END
/** Assert that the sentinel on a memarea is set correctly. */
#define CHECK_SENTINEL(chunk)                                           \
  STMT_BEGIN                                                            \
  uint32_t sent_val = get_uint32(&(chunk)->U_MEM[chunk->mem_size]);     \
  tor_assert(sent_val == SENTINEL_VAL);                                 \
  STMT_END
#else
#define SENTINEL_LEN 0
#define SET_SENTINEL(chunk) STMT_NIL
#define CHECK_SENTINEL(chunk) STMT_NIL
#endif

/** Increment <b>ptr</b> until it is aligned to MEMAREA_ALIGN. */
static INLINE void *
realign_pointer(void *ptr)
{
  uintptr_t x = (uintptr_t)ptr;
  x = (x+MEMAREA_ALIGN_MASK) & ~MEMAREA_ALIGN_MASK;
  /* Reinstate this if bug 930 ever reappears
  tor_assert(((void*)x) >= ptr);
  */
  return (void*)x;
}

/** Implements part of a memarea.  New memory is carved off from chunk->mem in
 * increasing order until a request is too big, at which point a new chunk is
 * allocated. */
typedef struct memarea_chunk_t {
  /** Next chunk in this area. Only kept around so we can free it. */
  struct memarea_chunk_t *next_chunk;
  size_t mem_size; /**< How much RAM is available in mem, total? */
  char *next_mem; /**< Next position in mem to allocate data at.  If it's
                   * greater than or equal to mem+mem_size, this chunk is
                   * full. */
#ifdef USE_ALIGNED_ATTRIBUTE
  char mem[FLEXIBLE_ARRAY_MEMBER] __attribute__((aligned(MEMAREA_ALIGN)));
#else
  union {
    char mem[1]; /**< Memory space in this chunk.  */
    void *void_for_alignment_; /**< Dummy; used to make sure mem is aligned. */
  } u;
#endif
} memarea_chunk_t;

/** How many bytes are needed for overhead before we get to the memory part
 * of a chunk? */
#define CHUNK_HEADER_SIZE STRUCT_OFFSET(memarea_chunk_t, U_MEM)

/** What's the smallest that we'll allocate a chunk? */
#define CHUNK_SIZE 4096

/** A memarea_t is an allocation region for a set of small memory requests
 * that will all be freed at once. */
struct memarea_t {
  memarea_chunk_t *first; /**< Top of the chunk stack: never NULL. */
};

/** How many chunks will we put into the freelist before freeing them? */
#define MAX_FREELIST_LEN 4
/** The number of memarea chunks currently in our freelist. */
static int freelist_len=0;
/** A linked list of unused memory area chunks.  Used to prevent us from
 * spinning in malloc/free loops. */
static memarea_chunk_t *freelist = NULL;

/** Helper: allocate a new memarea chunk of around <b>chunk_size</b> bytes. */
static memarea_chunk_t *
alloc_chunk(size_t sz, int freelist_ok)
{
  tor_assert(sz < SIZE_T_CEILING);
  if (freelist && freelist_ok) {
    memarea_chunk_t *res = freelist;
    freelist = res->next_chunk;
    res->next_chunk = NULL;
    --freelist_len;
    CHECK_SENTINEL(res);
    return res;
  } else {
    size_t chunk_size = freelist_ok ? CHUNK_SIZE : sz;
    memarea_chunk_t *res;
    chunk_size += SENTINEL_LEN;
    res = tor_malloc(chunk_size);
    res->next_chunk = NULL;
    res->mem_size = chunk_size - CHUNK_HEADER_SIZE - SENTINEL_LEN;
    res->next_mem = res->U_MEM;
    tor_assert(res->next_mem+res->mem_size+SENTINEL_LEN ==
               ((char*)res)+chunk_size);
    tor_assert(realign_pointer(res->next_mem) == res->next_mem);
    SET_SENTINEL(res);
    return res;
  }
}

/** Release <b>chunk</b> from a memarea, either by adding it to the freelist
 * or by freeing it if the freelist is already too big. */
static void
chunk_free_unchecked(memarea_chunk_t *chunk)
{
  CHECK_SENTINEL(chunk);
  if (freelist_len < MAX_FREELIST_LEN) {
    ++freelist_len;
    chunk->next_chunk = freelist;
    freelist = chunk;
    chunk->next_mem = chunk->U_MEM;
  } else {
    tor_free(chunk);
  }
}

/** Allocate and return new memarea. */
memarea_t *
memarea_new(void)
{
  memarea_t *head = tor_malloc(sizeof(memarea_t));
  head->first = alloc_chunk(CHUNK_SIZE, 1);
  return head;
}

/** Free <b>area</b>, invalidating all pointers returned from memarea_alloc()
 * and friends for this area */
void
memarea_drop_all(memarea_t *area)
{
  memarea_chunk_t *chunk, *next;
  for (chunk = area->first; chunk; chunk = next) {
    next = chunk->next_chunk;
    chunk_free_unchecked(chunk);
  }
  area->first = NULL; /*fail fast on */
  tor_free(area);
}

/** Forget about having allocated anything in <b>area</b>, and free some of
 * the backing storage associated with it, as appropriate. Invalidates all
 * pointers returned from memarea_alloc() for this area. */
void
memarea_clear(memarea_t *area)
{
  memarea_chunk_t *chunk, *next;
  if (area->first->next_chunk) {
    for (chunk = area->first->next_chunk; chunk; chunk = next) {
      next = chunk->next_chunk;
      chunk_free_unchecked(chunk);
    }
    area->first->next_chunk = NULL;
  }
  area->first->next_mem = area->first->U_MEM;
}

/** Remove all unused memarea chunks from the internal freelist. */
void
memarea_clear_freelist(void)
{
  memarea_chunk_t *chunk, *next;
  freelist_len = 0;
  for (chunk = freelist; chunk; chunk = next) {
    next = chunk->next_chunk;
    tor_free(chunk);
  }
  freelist = NULL;
}

/** Return true iff <b>p</b> is in a range that has been returned by an
 * allocation from <b>area</b>. */
int
memarea_owns_ptr(const memarea_t *area, const void *p)
{
  memarea_chunk_t *chunk;
  const char *ptr = p;
  for (chunk = area->first; chunk; chunk = chunk->next_chunk) {
    if (ptr >= chunk->U_MEM && ptr < chunk->next_mem)
      return 1;
  }
  return 0;
}

/** Return a pointer to a chunk of memory in <b>area</b> of at least <b>sz</b>
 * bytes.  <b>sz</b> should be significantly smaller than the area's chunk
 * size, though we can deal if it isn't. */
void *
memarea_alloc(memarea_t *area, size_t sz)
{
  memarea_chunk_t *chunk = area->first;
  char *result;
  tor_assert(chunk);
  CHECK_SENTINEL(chunk);
  tor_assert(sz < SIZE_T_CEILING);
  if (sz == 0)
    sz = 1;
  if (chunk->next_mem+sz > chunk->U_MEM+chunk->mem_size) {
    if (sz+CHUNK_HEADER_SIZE >= CHUNK_SIZE) {
      /* This allocation is too big.  Stick it in a special chunk, and put
       * that chunk second in the list. */
      memarea_chunk_t *new_chunk = alloc_chunk(sz+CHUNK_HEADER_SIZE, 0);
      new_chunk->next_chunk = chunk->next_chunk;
      chunk->next_chunk = new_chunk;
      chunk = new_chunk;
    } else {
      memarea_chunk_t *new_chunk = alloc_chunk(CHUNK_SIZE, 1);
      new_chunk->next_chunk = chunk;
      area->first = chunk = new_chunk;
    }
    tor_assert(chunk->mem_size >= sz);
  }
  result = chunk->next_mem;
  chunk->next_mem = chunk->next_mem + sz;
  /* Reinstate these if bug 930 ever comes back
  tor_assert(chunk->next_mem >= chunk->U_MEM);
  tor_assert(chunk->next_mem <= chunk->U_MEM+chunk->mem_size);
  */
  chunk->next_mem = realign_pointer(chunk->next_mem);
  return result;
}

/** As memarea_alloc(), but clears the memory it returns. */
void *
memarea_alloc_zero(memarea_t *area, size_t sz)
{
  void *result = memarea_alloc(area, sz);
  memset(result, 0, sz);
  return result;
}

/** As memdup, but returns the memory from <b>area</b>. */
void *
memarea_memdup(memarea_t *area, const void *s, size_t n)
{
  char *result = memarea_alloc(area, n);
  memcpy(result, s, n);
  return result;
}

/** As strdup, but returns the memory from <b>area</b>. */
char *
memarea_strdup(memarea_t *area, const char *s)
{
  return memarea_memdup(area, s, strlen(s)+1);
}

/** As strndup, but returns the memory from <b>area</b>. */
char *
memarea_strndup(memarea_t *area, const char *s, size_t n)
{
  size_t ln = 0;
  char *result;
  tor_assert(n < SIZE_T_CEILING);
  for (ln = 0; ln < n && s[ln]; ++ln)
    ;
  result = memarea_alloc(area, ln+1);
  memcpy(result, s, ln);
  result[ln]='\0';
  return result;
}

/** Set <b>allocated_out</b> to the number of bytes allocated in <b>area</b>,
 * and <b>used_out</b> to the number of bytes currently used. */
void
memarea_get_stats(memarea_t *area, size_t *allocated_out, size_t *used_out)
{
  size_t a = 0, u = 0;
  memarea_chunk_t *chunk;
  for (chunk = area->first; chunk; chunk = chunk->next_chunk) {
    CHECK_SENTINEL(chunk);
    a += CHUNK_HEADER_SIZE + chunk->mem_size;
    tor_assert(chunk->next_mem >= chunk->U_MEM);
    u += CHUNK_HEADER_SIZE + (chunk->next_mem - chunk->U_MEM);
  }
  *allocated_out = a;
  *used_out = u;
}

/** Assert that <b>area</b> is okay. */
void
memarea_assert_ok(memarea_t *area)
{
  memarea_chunk_t *chunk;
  tor_assert(area->first);

  for (chunk = area->first; chunk; chunk = chunk->next_chunk) {
    CHECK_SENTINEL(chunk);
    tor_assert(chunk->next_mem >= chunk->U_MEM);
    tor_assert(chunk->next_mem <=
          (char*) realign_pointer(chunk->U_MEM+chunk->mem_size));
  }
}

