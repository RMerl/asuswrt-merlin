/* 
   Samba Unix SMB/CIFS implementation.

   Samba trivial allocation library - new interface

   NOTE: Please read talloc_guide.txt for full documentation

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Stefan Metzmacher 2006
   
     ** NOTE! The following LGPL license applies to the talloc
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

/*
  inspired by http://swapped.cc/halloc/
*/

#include "replace.h"
#include "talloc.h"

#ifdef TALLOC_BUILD_VERSION_MAJOR
#if (TALLOC_VERSION_MAJOR != TALLOC_BUILD_VERSION_MAJOR)
#error "TALLOC_VERSION_MAJOR != TALLOC_BUILD_VERSION_MAJOR"
#endif
#endif

#ifdef TALLOC_BUILD_VERSION_MINOR
#if (TALLOC_VERSION_MINOR != TALLOC_BUILD_VERSION_MINOR)
#error "TALLOC_VERSION_MINOR != TALLOC_BUILD_VERSION_MINOR"
#endif
#endif

/* Special macros that are no-ops except when run under Valgrind on
 * x86.  They've moved a little bit from valgrind 1.0.4 to 1.9.4 */
#ifdef HAVE_VALGRIND_MEMCHECK_H
        /* memcheck.h includes valgrind.h */
#include <valgrind/memcheck.h>
#elif defined(HAVE_VALGRIND_H)
#include <valgrind.h>
#endif

/* use this to force every realloc to change the pointer, to stress test
   code that might not cope */
#define ALWAYS_REALLOC 0


#define MAX_TALLOC_SIZE 0x10000000
#define TALLOC_MAGIC_BASE 0xe814ec70
#define TALLOC_MAGIC ( \
	TALLOC_MAGIC_BASE + \
	(TALLOC_VERSION_MAJOR << 12) + \
	(TALLOC_VERSION_MINOR << 4) \
)

#define TALLOC_FLAG_FREE 0x01
#define TALLOC_FLAG_LOOP 0x02
#define TALLOC_FLAG_POOL 0x04		/* This is a talloc pool */
#define TALLOC_FLAG_POOLMEM 0x08	/* This is allocated in a pool */
#define TALLOC_MAGIC_REFERENCE ((const char *)1)

/* by default we abort when given a bad pointer (such as when talloc_free() is called 
   on a pointer that came from malloc() */
#ifndef TALLOC_ABORT
#define TALLOC_ABORT(reason) abort()
#endif

#ifndef discard_const_p
#if defined(__intptr_t_defined) || defined(HAVE_INTPTR_T)
# define discard_const_p(type, ptr) ((type *)((intptr_t)(ptr)))
#else
# define discard_const_p(type, ptr) ((type *)(ptr))
#endif
#endif

/* these macros gain us a few percent of speed on gcc */
#if (__GNUC__ >= 3)
/* the strange !! is to ensure that __builtin_expect() takes either 0 or 1
   as its first argument */
#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif
#else
#ifndef likely
#define likely(x) (x)
#endif
#ifndef unlikely
#define unlikely(x) (x)
#endif
#endif

/* this null_context is only used if talloc_enable_leak_report() or
   talloc_enable_leak_report_full() is called, otherwise it remains
   NULL
*/
static void *null_context;
static void *autofree_context;

/* used to enable fill of memory on free, which can be useful for
 * catching use after free errors when valgrind is too slow
 */
static struct {
	bool initialised;
	bool enabled;
	uint8_t fill_value;
} talloc_fill;

#define TALLOC_FILL_ENV "TALLOC_FREE_FILL"

/*
 * do not wipe the header, to allow the
 * double-free logic to still work
 */
#define TC_INVALIDATE_FULL_FILL_CHUNK(_tc) do { \
	if (unlikely(talloc_fill.enabled)) { \
		size_t _flen = (_tc)->size; \
		char *_fptr = (char *)TC_PTR_FROM_CHUNK(_tc); \
		memset(_fptr, talloc_fill.fill_value, _flen); \
	} \
} while (0)

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_NOACCESS)
/* Mark the whole chunk as not accessable */
#define TC_INVALIDATE_FULL_VALGRIND_CHUNK(_tc) do { \
	size_t _flen = TC_HDR_SIZE + (_tc)->size; \
	char *_fptr = (char *)(_tc); \
	VALGRIND_MAKE_MEM_NOACCESS(_fptr, _flen); \
} while(0)
#else
#define TC_INVALIDATE_FULL_VALGRIND_CHUNK(_tc) do { } while (0)
#endif

#define TC_INVALIDATE_FULL_CHUNK(_tc) do { \
	TC_INVALIDATE_FULL_FILL_CHUNK(_tc); \
	TC_INVALIDATE_FULL_VALGRIND_CHUNK(_tc); \
} while (0)

#define TC_INVALIDATE_SHRINK_FILL_CHUNK(_tc, _new_size) do { \
	if (unlikely(talloc_fill.enabled)) { \
		size_t _flen = (_tc)->size - (_new_size); \
		char *_fptr = (char *)TC_PTR_FROM_CHUNK(_tc); \
		_fptr += (_new_size); \
		memset(_fptr, talloc_fill.fill_value, _flen); \
	} \
} while (0)

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_NOACCESS)
/* Mark the unused bytes not accessable */
#define TC_INVALIDATE_SHRINK_VALGRIND_CHUNK(_tc, _new_size) do { \
	size_t _flen = (_tc)->size - (_new_size); \
	char *_fptr = (char *)TC_PTR_FROM_CHUNK(_tc); \
	_fptr += (_new_size); \
	VALGRIND_MAKE_MEM_NOACCESS(_fptr, _flen); \
} while (0)
#else
#define TC_INVALIDATE_SHRINK_VALGRIND_CHUNK(_tc, _new_size) do { } while (0)
#endif

#define TC_INVALIDATE_SHRINK_CHUNK(_tc, _new_size) do { \
	TC_INVALIDATE_SHRINK_FILL_CHUNK(_tc, _new_size); \
	TC_INVALIDATE_SHRINK_VALGRIND_CHUNK(_tc, _new_size); \
} while (0)

#define TC_UNDEFINE_SHRINK_FILL_CHUNK(_tc, _new_size) do { \
	if (unlikely(talloc_fill.enabled)) { \
		size_t _flen = (_tc)->size - (_new_size); \
		char *_fptr = (char *)TC_PTR_FROM_CHUNK(_tc); \
		_fptr += (_new_size); \
		memset(_fptr, talloc_fill.fill_value, _flen); \
	} \
} while (0)

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_UNDEFINED)
/* Mark the unused bytes as undefined */
#define TC_UNDEFINE_SHRINK_VALGRIND_CHUNK(_tc, _new_size) do { \
	size_t _flen = (_tc)->size - (_new_size); \
	char *_fptr = (char *)TC_PTR_FROM_CHUNK(_tc); \
	_fptr += (_new_size); \
	VALGRIND_MAKE_MEM_UNDEFINED(_fptr, _flen); \
} while (0)
#else
#define TC_UNDEFINE_SHRINK_VALGRIND_CHUNK(_tc, _new_size) do { } while (0)
#endif

#define TC_UNDEFINE_SHRINK_CHUNK(_tc, _new_size) do { \
	TC_UNDEFINE_SHRINK_FILL_CHUNK(_tc, _new_size); \
	TC_UNDEFINE_SHRINK_VALGRIND_CHUNK(_tc, _new_size); \
} while (0)

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_UNDEFINED)
/* Mark the new bytes as undefined */
#define TC_UNDEFINE_GROW_VALGRIND_CHUNK(_tc, _new_size) do { \
	size_t _old_used = TC_HDR_SIZE + (_tc)->size; \
	size_t _new_used = TC_HDR_SIZE + (_new_size); \
	size_t _flen = _new_used - _old_used; \
	char *_fptr = _old_used + (char *)(_tc); \
	VALGRIND_MAKE_MEM_UNDEFINED(_fptr, _flen); \
} while (0)
#else
#define TC_UNDEFINE_GROW_VALGRIND_CHUNK(_tc, _new_size) do { } while (0)
#endif

#define TC_UNDEFINE_GROW_CHUNK(_tc, _new_size) do { \
	TC_UNDEFINE_GROW_VALGRIND_CHUNK(_tc, _new_size); \
} while (0)

struct talloc_reference_handle {
	struct talloc_reference_handle *next, *prev;
	void *ptr;
	const char *location;
};

typedef int (*talloc_destructor_t)(void *);

struct talloc_chunk {
	struct talloc_chunk *next, *prev;
	struct talloc_chunk *parent, *child;
	struct talloc_reference_handle *refs;
	talloc_destructor_t destructor;
	const char *name;
	size_t size;
	unsigned flags;

	/*
	 * "pool" has dual use:
	 *
	 * For the talloc pool itself (i.e. TALLOC_FLAG_POOL is set), "pool"
	 * marks the end of the currently allocated area.
	 *
	 * For members of the pool (i.e. TALLOC_FLAG_POOLMEM is set), "pool"
	 * is a pointer to the struct talloc_chunk of the pool that it was
	 * allocated from. This way children can quickly find the pool to chew
	 * from.
	 */
	void *pool;
};

/* 16 byte alignment seems to keep everyone happy */
#define TC_ALIGN16(s) (((s)+15)&~15)
#define TC_HDR_SIZE TC_ALIGN16(sizeof(struct talloc_chunk))
#define TC_PTR_FROM_CHUNK(tc) ((void *)(TC_HDR_SIZE + (char*)tc))

_PUBLIC_ int talloc_version_major(void)
{
	return TALLOC_VERSION_MAJOR;
}

_PUBLIC_ int talloc_version_minor(void)
{
	return TALLOC_VERSION_MINOR;
}

static void (*talloc_log_fn)(const char *message);

_PUBLIC_ void talloc_set_log_fn(void (*log_fn)(const char *message))
{
	talloc_log_fn = log_fn;
}

static void talloc_log(const char *fmt, ...) PRINTF_ATTRIBUTE(1,2);
static void talloc_log(const char *fmt, ...)
{
	va_list ap;
	char *message;

	if (!talloc_log_fn) {
		return;
	}

	va_start(ap, fmt);
	message = talloc_vasprintf(NULL, fmt, ap);
	va_end(ap);

	talloc_log_fn(message);
	talloc_free(message);
}

static void talloc_log_stderr(const char *message)
{
	fprintf(stderr, "%s", message);
}

_PUBLIC_ void talloc_set_log_stderr(void)
{
	talloc_set_log_fn(talloc_log_stderr);
}

static void (*talloc_abort_fn)(const char *reason);

_PUBLIC_ void talloc_set_abort_fn(void (*abort_fn)(const char *reason))
{
	talloc_abort_fn = abort_fn;
}

static void talloc_abort(const char *reason)
{
	talloc_log("%s\n", reason);

	if (!talloc_abort_fn) {
		TALLOC_ABORT(reason);
	}

	talloc_abort_fn(reason);
}

static void talloc_abort_magic(unsigned magic)
{
	unsigned striped = magic - TALLOC_MAGIC_BASE;
	unsigned major = (striped & 0xFFFFF000) >> 12;
	unsigned minor = (striped & 0x00000FF0) >> 4;
	talloc_log("Bad talloc magic[0x%08X/%u/%u] expected[0x%08X/%u/%u]\n",
		   magic, major, minor,
		   TALLOC_MAGIC, TALLOC_VERSION_MAJOR, TALLOC_VERSION_MINOR);
	talloc_abort("Bad talloc magic value - wrong talloc version used/mixed");
}

static void talloc_abort_access_after_free(void)
{
	talloc_abort("Bad talloc magic value - access after free");
}

static void talloc_abort_unknown_value(void)
{
	talloc_abort("Bad talloc magic value - unknown value");
}

/* panic if we get a bad magic value */
static inline struct talloc_chunk *talloc_chunk_from_ptr(const void *ptr)
{
	const char *pp = (const char *)ptr;
	struct talloc_chunk *tc = discard_const_p(struct talloc_chunk, pp - TC_HDR_SIZE);
	if (unlikely((tc->flags & (TALLOC_FLAG_FREE | ~0xF)) != TALLOC_MAGIC)) { 
		if ((tc->flags & (~0xFFF)) == TALLOC_MAGIC_BASE) {
			talloc_abort_magic(tc->flags & (~0xF));
			return NULL;
		}

		if (tc->flags & TALLOC_FLAG_FREE) {
			talloc_log("talloc: access after free error - first free may be at %s\n", tc->name);
			talloc_abort_access_after_free();
			return NULL;
		} else {
			talloc_abort_unknown_value();
			return NULL;
		}
	}
	return tc;
}

/* hook into the front of the list */
#define _TLIST_ADD(list, p) \
do { \
        if (!(list)) { \
		(list) = (p); \
		(p)->next = (p)->prev = NULL; \
	} else { \
		(list)->prev = (p); \
		(p)->next = (list); \
		(p)->prev = NULL; \
		(list) = (p); \
	}\
} while (0)

/* remove an element from a list - element doesn't have to be in list. */
#define _TLIST_REMOVE(list, p) \
do { \
	if ((p) == (list)) { \
		(list) = (p)->next; \
		if (list) (list)->prev = NULL; \
	} else { \
		if ((p)->prev) (p)->prev->next = (p)->next; \
		if ((p)->next) (p)->next->prev = (p)->prev; \
	} \
	if ((p) && ((p) != (list))) (p)->next = (p)->prev = NULL; \
} while (0)


/*
  return the parent chunk of a pointer
*/
static inline struct talloc_chunk *talloc_parent_chunk(const void *ptr)
{
	struct talloc_chunk *tc;

	if (unlikely(ptr == NULL)) {
		return NULL;
	}

	tc = talloc_chunk_from_ptr(ptr);
	while (tc->prev) tc=tc->prev;

	return tc->parent;
}

_PUBLIC_ void *talloc_parent(const void *ptr)
{
	struct talloc_chunk *tc = talloc_parent_chunk(ptr);
	return tc? TC_PTR_FROM_CHUNK(tc) : NULL;
}

/*
  find parents name
*/
_PUBLIC_ const char *talloc_parent_name(const void *ptr)
{
	struct talloc_chunk *tc = talloc_parent_chunk(ptr);
	return tc? tc->name : NULL;
}

/*
  A pool carries an in-pool object count count in the first 16 bytes.
  bytes. This is done to support talloc_steal() to a parent outside of the
  pool. The count includes the pool itself, so a talloc_free() on a pool will
  only destroy the pool if the count has dropped to zero. A talloc_free() of a
  pool member will reduce the count, and eventually also call free(3) on the
  pool memory.

  The object count is not put into "struct talloc_chunk" because it is only
  relevant for talloc pools and the alignment to 16 bytes would increase the
  memory footprint of each talloc chunk by those 16 bytes.
*/

#define TALLOC_POOL_HDR_SIZE 16

#define TC_POOL_SPACE_LEFT(_pool_tc) \
	PTR_DIFF(TC_HDR_SIZE + (_pool_tc)->size + (char *)(_pool_tc), \
		 (_pool_tc)->pool)

#define TC_POOL_FIRST_CHUNK(_pool_tc) \
	((void *)(TC_HDR_SIZE + TALLOC_POOL_HDR_SIZE + (char *)(_pool_tc)))

#define TC_POOLMEM_CHUNK_SIZE(_tc) \
	TC_ALIGN16(TC_HDR_SIZE + (_tc)->size)

#define TC_POOLMEM_NEXT_CHUNK(_tc) \
	((void *)(TC_POOLMEM_CHUNK_SIZE(tc) + (char*)(_tc)))

/* Mark the whole remaining pool as not accessable */
#define TC_INVALIDATE_FILL_POOL(_pool_tc) do { \
	if (unlikely(talloc_fill.enabled)) { \
		size_t _flen = TC_POOL_SPACE_LEFT(_pool_tc); \
		char *_fptr = (char *)(_pool_tc)->pool; \
		memset(_fptr, talloc_fill.fill_value, _flen); \
	} \
} while(0)

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_NOACCESS)
/* Mark the whole remaining pool as not accessable */
#define TC_INVALIDATE_VALGRIND_POOL(_pool_tc) do { \
	size_t _flen = TC_POOL_SPACE_LEFT(_pool_tc); \
	char *_fptr = (char *)(_pool_tc)->pool; \
	VALGRIND_MAKE_MEM_NOACCESS(_fptr, _flen); \
} while(0)
#else
#define TC_INVALIDATE_VALGRIND_POOL(_pool_tc) do { } while (0)
#endif

#define TC_INVALIDATE_POOL(_pool_tc) do { \
	TC_INVALIDATE_FILL_POOL(_pool_tc); \
	TC_INVALIDATE_VALGRIND_POOL(_pool_tc); \
} while (0)

static unsigned int *talloc_pool_objectcount(struct talloc_chunk *tc)
{
	return (unsigned int *)((char *)tc + TC_HDR_SIZE);
}

/*
  Allocate from a pool
*/

static struct talloc_chunk *talloc_alloc_pool(struct talloc_chunk *parent,
					      size_t size)
{
	struct talloc_chunk *pool_ctx = NULL;
	size_t space_left;
	struct talloc_chunk *result;
	size_t chunk_size;

	if (parent == NULL) {
		return NULL;
	}

	if (parent->flags & TALLOC_FLAG_POOL) {
		pool_ctx = parent;
	}
	else if (parent->flags & TALLOC_FLAG_POOLMEM) {
		pool_ctx = (struct talloc_chunk *)parent->pool;
	}

	if (pool_ctx == NULL) {
		return NULL;
	}

	space_left = TC_POOL_SPACE_LEFT(pool_ctx);

	/*
	 * Align size to 16 bytes
	 */
	chunk_size = TC_ALIGN16(size);

	if (space_left < chunk_size) {
		return NULL;
	}

	result = (struct talloc_chunk *)pool_ctx->pool;

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_UNDEFINED)
	VALGRIND_MAKE_MEM_UNDEFINED(result, size);
#endif

	pool_ctx->pool = (void *)((char *)result + chunk_size);

	result->flags = TALLOC_MAGIC | TALLOC_FLAG_POOLMEM;
	result->pool = pool_ctx;

	*talloc_pool_objectcount(pool_ctx) += 1;

	return result;
}

/* 
   Allocate a bit of memory as a child of an existing pointer
*/
static inline void *__talloc(const void *context, size_t size)
{
	struct talloc_chunk *tc = NULL;

	if (unlikely(context == NULL)) {
		context = null_context;
	}

	if (unlikely(size >= MAX_TALLOC_SIZE)) {
		return NULL;
	}

	if (context != NULL) {
		tc = talloc_alloc_pool(talloc_chunk_from_ptr(context),
				       TC_HDR_SIZE+size);
	}

	if (tc == NULL) {
		tc = (struct talloc_chunk *)malloc(TC_HDR_SIZE+size);
		if (unlikely(tc == NULL)) return NULL;
		tc->flags = TALLOC_MAGIC;
		tc->pool  = NULL;
	}

	tc->size = size;
	tc->destructor = NULL;
	tc->child = NULL;
	tc->name = NULL;
	tc->refs = NULL;

	if (likely(context)) {
		struct talloc_chunk *parent = talloc_chunk_from_ptr(context);

		if (parent->child) {
			parent->child->parent = NULL;
			tc->next = parent->child;
			tc->next->prev = tc;
		} else {
			tc->next = NULL;
		}
		tc->parent = parent;
		tc->prev = NULL;
		parent->child = tc;
	} else {
		tc->next = tc->prev = tc->parent = NULL;
	}

	return TC_PTR_FROM_CHUNK(tc);
}

/*
 * Create a talloc pool
 */

_PUBLIC_ void *talloc_pool(const void *context, size_t size)
{
	void *result = __talloc(context, size + TALLOC_POOL_HDR_SIZE);
	struct talloc_chunk *tc;

	if (unlikely(result == NULL)) {
		return NULL;
	}

	tc = talloc_chunk_from_ptr(result);

	tc->flags |= TALLOC_FLAG_POOL;
	tc->pool = TC_POOL_FIRST_CHUNK(tc);

	*talloc_pool_objectcount(tc) = 1;

	TC_INVALIDATE_POOL(tc);

	return result;
}

/*
  setup a destructor to be called on free of a pointer
  the destructor should return 0 on success, or -1 on failure.
  if the destructor fails then the free is failed, and the memory can
  be continued to be used
*/
_PUBLIC_ void _talloc_set_destructor(const void *ptr, int (*destructor)(void *))
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	tc->destructor = destructor;
}

/*
  increase the reference count on a piece of memory. 
*/
_PUBLIC_ int talloc_increase_ref_count(const void *ptr)
{
	if (unlikely(!talloc_reference(null_context, ptr))) {
		return -1;
	}
	return 0;
}

/*
  helper for talloc_reference()

  this is referenced by a function pointer and should not be inline
*/
static int talloc_reference_destructor(struct talloc_reference_handle *handle)
{
	struct talloc_chunk *ptr_tc = talloc_chunk_from_ptr(handle->ptr);
	_TLIST_REMOVE(ptr_tc->refs, handle);
	return 0;
}

/*
   more efficient way to add a name to a pointer - the name must point to a 
   true string constant
*/
static inline void _talloc_set_name_const(const void *ptr, const char *name)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	tc->name = name;
}

/*
  internal talloc_named_const()
*/
static inline void *_talloc_named_const(const void *context, size_t size, const char *name)
{
	void *ptr;

	ptr = __talloc(context, size);
	if (unlikely(ptr == NULL)) {
		return NULL;
	}

	_talloc_set_name_const(ptr, name);

	return ptr;
}

/*
  make a secondary reference to a pointer, hanging off the given context.
  the pointer remains valid until both the original caller and this given
  context are freed.
  
  the major use for this is when two different structures need to reference the 
  same underlying data, and you want to be able to free the two instances separately,
  and in either order
*/
_PUBLIC_ void *_talloc_reference_loc(const void *context, const void *ptr, const char *location)
{
	struct talloc_chunk *tc;
	struct talloc_reference_handle *handle;
	if (unlikely(ptr == NULL)) return NULL;

	tc = talloc_chunk_from_ptr(ptr);
	handle = (struct talloc_reference_handle *)_talloc_named_const(context,
						   sizeof(struct talloc_reference_handle),
						   TALLOC_MAGIC_REFERENCE);
	if (unlikely(handle == NULL)) return NULL;

	/* note that we hang the destructor off the handle, not the
	   main context as that allows the caller to still setup their
	   own destructor on the context if they want to */
	talloc_set_destructor(handle, talloc_reference_destructor);
	handle->ptr = discard_const_p(void, ptr);
	handle->location = location;
	_TLIST_ADD(tc->refs, handle);
	return handle->ptr;
}

static void *_talloc_steal_internal(const void *new_ctx, const void *ptr);

static inline void _talloc_free_poolmem(struct talloc_chunk *tc,
					const char *location)
{
	struct talloc_chunk *pool;
	void *next_tc;
	unsigned int *pool_object_count;

	pool = (struct talloc_chunk *)tc->pool;
	next_tc = TC_POOLMEM_NEXT_CHUNK(tc);

	tc->flags |= TALLOC_FLAG_FREE;

	/* we mark the freed memory with where we called the free
	 * from. This means on a double free error we can report where
	 * the first free came from
	 */
	tc->name = location;

	TC_INVALIDATE_FULL_CHUNK(tc);

	pool_object_count = talloc_pool_objectcount(pool);

	if (unlikely(*pool_object_count == 0)) {
		talloc_abort("Pool object count zero!");
		return;
	}

	*pool_object_count -= 1;

	if (unlikely(*pool_object_count == 1 && !(pool->flags & TALLOC_FLAG_FREE))) {
		/*
		 * if there is just one object left in the pool
		 * and pool->flags does not have TALLOC_FLAG_FREE,
		 * it means this is the pool itself and
		 * the rest is available for new objects
		 * again.
		 */
		pool->pool = TC_POOL_FIRST_CHUNK(pool);
		TC_INVALIDATE_POOL(pool);
	} else if (unlikely(*pool_object_count == 0)) {
		/*
		 * we mark the freed memory with where we called the free
		 * from. This means on a double free error we can report where
		 * the first free came from
		 */
		pool->name = location;

		TC_INVALIDATE_FULL_CHUNK(pool);
		free(pool);
	} else if (pool->pool == next_tc) {
		/*
		 * if pool->pool still points to end of
		 * 'tc' (which is stored in the 'next_tc' variable),
		 * we can reclaim the memory of 'tc'.
		 */
		pool->pool = tc;
	}
}

static inline void _talloc_free_children_internal(struct talloc_chunk *tc,
						  void *ptr,
						  const char *location);

/* 
   internal talloc_free call
*/
static inline int _talloc_free_internal(void *ptr, const char *location)
{
	struct talloc_chunk *tc;

	if (unlikely(ptr == NULL)) {
		return -1;
	}

	/* possibly initialised the talloc fill value */
	if (unlikely(!talloc_fill.initialised)) {
		const char *fill = getenv(TALLOC_FILL_ENV);
		if (fill != NULL) {
			talloc_fill.enabled = true;
			talloc_fill.fill_value = strtoul(fill, NULL, 0);
		}
		talloc_fill.initialised = true;
	}

	tc = talloc_chunk_from_ptr(ptr);

	if (unlikely(tc->refs)) {
		int is_child;
		/* check if this is a reference from a child or
		 * grandchild back to it's parent or grandparent
		 *
		 * in that case we need to remove the reference and
		 * call another instance of talloc_free() on the current
		 * pointer.
		 */
		is_child = talloc_is_parent(tc->refs, ptr);
		_talloc_free_internal(tc->refs, location);
		if (is_child) {
			return _talloc_free_internal(ptr, location);
		}
		return -1;
	}

	if (unlikely(tc->flags & TALLOC_FLAG_LOOP)) {
		/* we have a free loop - stop looping */
		return 0;
	}

	if (unlikely(tc->destructor)) {
		talloc_destructor_t d = tc->destructor;
		if (d == (talloc_destructor_t)-1) {
			return -1;
		}
		tc->destructor = (talloc_destructor_t)-1;
		if (d(ptr) == -1) {
			tc->destructor = d;
			return -1;
		}
		tc->destructor = NULL;
	}

	if (tc->parent) {
		_TLIST_REMOVE(tc->parent->child, tc);
		if (tc->parent->child) {
			tc->parent->child->parent = tc->parent;
		}
	} else {
		if (tc->prev) tc->prev->next = tc->next;
		if (tc->next) tc->next->prev = tc->prev;
		tc->prev = tc->next = NULL;
	}

	tc->flags |= TALLOC_FLAG_LOOP;

	_talloc_free_children_internal(tc, ptr, location);

	tc->flags |= TALLOC_FLAG_FREE;

	/* we mark the freed memory with where we called the free
	 * from. This means on a double free error we can report where
	 * the first free came from 
	 */	 
	tc->name = location;

	if (tc->flags & TALLOC_FLAG_POOL) {
		unsigned int *pool_object_count;

		pool_object_count = talloc_pool_objectcount(tc);

		if (unlikely(*pool_object_count == 0)) {
			talloc_abort("Pool object count zero!");
			return 0;
		}

		*pool_object_count -= 1;

		if (unlikely(*pool_object_count == 0)) {
			TC_INVALIDATE_FULL_CHUNK(tc);
			free(tc);
		}
	} else if (tc->flags & TALLOC_FLAG_POOLMEM) {
		_talloc_free_poolmem(tc, location);
	} else {
		TC_INVALIDATE_FULL_CHUNK(tc);
		free(tc);
	}
	return 0;
}

/* 
   move a lump of memory from one talloc context to another return the
   ptr on success, or NULL if it could not be transferred.
   passing NULL as ptr will always return NULL with no side effects.
*/
static void *_talloc_steal_internal(const void *new_ctx, const void *ptr)
{
	struct talloc_chunk *tc, *new_tc;

	if (unlikely(!ptr)) {
		return NULL;
	}

	if (unlikely(new_ctx == NULL)) {
		new_ctx = null_context;
	}

	tc = talloc_chunk_from_ptr(ptr);

	if (unlikely(new_ctx == NULL)) {
		if (tc->parent) {
			_TLIST_REMOVE(tc->parent->child, tc);
			if (tc->parent->child) {
				tc->parent->child->parent = tc->parent;
			}
		} else {
			if (tc->prev) tc->prev->next = tc->next;
			if (tc->next) tc->next->prev = tc->prev;
		}
		
		tc->parent = tc->next = tc->prev = NULL;
		return discard_const_p(void, ptr);
	}

	new_tc = talloc_chunk_from_ptr(new_ctx);

	if (unlikely(tc == new_tc || tc->parent == new_tc)) {
		return discard_const_p(void, ptr);
	}

	if (tc->parent) {
		_TLIST_REMOVE(tc->parent->child, tc);
		if (tc->parent->child) {
			tc->parent->child->parent = tc->parent;
		}
	} else {
		if (tc->prev) tc->prev->next = tc->next;
		if (tc->next) tc->next->prev = tc->prev;
		tc->prev = tc->next = NULL;
	}

	tc->parent = new_tc;
	if (new_tc->child) new_tc->child->parent = NULL;
	_TLIST_ADD(new_tc->child, tc);

	return discard_const_p(void, ptr);
}

/* 
   move a lump of memory from one talloc context to another return the
   ptr on success, or NULL if it could not be transferred.
   passing NULL as ptr will always return NULL with no side effects.
*/
_PUBLIC_ void *_talloc_steal_loc(const void *new_ctx, const void *ptr, const char *location)
{
	struct talloc_chunk *tc;

	if (unlikely(ptr == NULL)) {
		return NULL;
	}
	
	tc = talloc_chunk_from_ptr(ptr);
	
	if (unlikely(tc->refs != NULL) && talloc_parent(ptr) != new_ctx) {
		struct talloc_reference_handle *h;

		talloc_log("WARNING: talloc_steal with references at %s\n",
			   location);

		for (h=tc->refs; h; h=h->next) {
			talloc_log("\treference at %s\n",
				   h->location);
		}
	}

#if 0
	/* this test is probably too expensive to have on in the
	   normal build, but it useful for debugging */
	if (talloc_is_parent(new_ctx, ptr)) {
		talloc_log("WARNING: stealing into talloc child at %s\n", location);
	}
#endif
	
	return _talloc_steal_internal(new_ctx, ptr);
}

/* 
   this is like a talloc_steal(), but you must supply the old
   parent. This resolves the ambiguity in a talloc_steal() which is
   called on a context that has more than one parent (via references)

   The old parent can be either a reference or a parent
*/
_PUBLIC_ void *talloc_reparent(const void *old_parent, const void *new_parent, const void *ptr)
{
	struct talloc_chunk *tc;
	struct talloc_reference_handle *h;

	if (unlikely(ptr == NULL)) {
		return NULL;
	}

	if (old_parent == talloc_parent(ptr)) {
		return _talloc_steal_internal(new_parent, ptr);
	}

	tc = talloc_chunk_from_ptr(ptr);
	for (h=tc->refs;h;h=h->next) {
		if (talloc_parent(h) == old_parent) {
			if (_talloc_steal_internal(new_parent, h) != h) {
				return NULL;
			}
			return discard_const_p(void, ptr);
		}
	}	

	/* it wasn't a parent */
	return NULL;
}

/*
  remove a secondary reference to a pointer. This undo's what
  talloc_reference() has done. The context and pointer arguments
  must match those given to a talloc_reference()
*/
static inline int talloc_unreference(const void *context, const void *ptr)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	struct talloc_reference_handle *h;

	if (unlikely(context == NULL)) {
		context = null_context;
	}

	for (h=tc->refs;h;h=h->next) {
		struct talloc_chunk *p = talloc_parent_chunk(h);
		if (p == NULL) {
			if (context == NULL) break;
		} else if (TC_PTR_FROM_CHUNK(p) == context) {
			break;
		}
	}
	if (h == NULL) {
		return -1;
	}

	return _talloc_free_internal(h, __location__);
}

/*
  remove a specific parent context from a pointer. This is a more
  controlled varient of talloc_free()
*/
_PUBLIC_ int talloc_unlink(const void *context, void *ptr)
{
	struct talloc_chunk *tc_p, *new_p;
	void *new_parent;

	if (ptr == NULL) {
		return -1;
	}

	if (context == NULL) {
		context = null_context;
	}

	if (talloc_unreference(context, ptr) == 0) {
		return 0;
	}

	if (context == NULL) {
		if (talloc_parent_chunk(ptr) != NULL) {
			return -1;
		}
	} else {
		if (talloc_chunk_from_ptr(context) != talloc_parent_chunk(ptr)) {
			return -1;
		}
	}
	
	tc_p = talloc_chunk_from_ptr(ptr);

	if (tc_p->refs == NULL) {
		return _talloc_free_internal(ptr, __location__);
	}

	new_p = talloc_parent_chunk(tc_p->refs);
	if (new_p) {
		new_parent = TC_PTR_FROM_CHUNK(new_p);
	} else {
		new_parent = NULL;
	}

	if (talloc_unreference(new_parent, ptr) != 0) {
		return -1;
	}

	_talloc_steal_internal(new_parent, ptr);

	return 0;
}

/*
  add a name to an existing pointer - va_list version
*/
static inline const char *talloc_set_name_v(const void *ptr, const char *fmt, va_list ap) PRINTF_ATTRIBUTE(2,0);

static inline const char *talloc_set_name_v(const void *ptr, const char *fmt, va_list ap)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	tc->name = talloc_vasprintf(ptr, fmt, ap);
	if (likely(tc->name)) {
		_talloc_set_name_const(tc->name, ".name");
	}
	return tc->name;
}

/*
  add a name to an existing pointer
*/
_PUBLIC_ const char *talloc_set_name(const void *ptr, const char *fmt, ...)
{
	const char *name;
	va_list ap;
	va_start(ap, fmt);
	name = talloc_set_name_v(ptr, fmt, ap);
	va_end(ap);
	return name;
}


/*
  create a named talloc pointer. Any talloc pointer can be named, and
  talloc_named() operates just like talloc() except that it allows you
  to name the pointer.
*/
_PUBLIC_ void *talloc_named(const void *context, size_t size, const char *fmt, ...)
{
	va_list ap;
	void *ptr;
	const char *name;

	ptr = __talloc(context, size);
	if (unlikely(ptr == NULL)) return NULL;

	va_start(ap, fmt);
	name = talloc_set_name_v(ptr, fmt, ap);
	va_end(ap);

	if (unlikely(name == NULL)) {
		_talloc_free_internal(ptr, __location__);
		return NULL;
	}

	return ptr;
}

/*
  return the name of a talloc ptr, or "UNNAMED"
*/
_PUBLIC_ const char *talloc_get_name(const void *ptr)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	if (unlikely(tc->name == TALLOC_MAGIC_REFERENCE)) {
		return ".reference";
	}
	if (likely(tc->name)) {
		return tc->name;
	}
	return "UNNAMED";
}


/*
  check if a pointer has the given name. If it does, return the pointer,
  otherwise return NULL
*/
_PUBLIC_ void *talloc_check_name(const void *ptr, const char *name)
{
	const char *pname;
	if (unlikely(ptr == NULL)) return NULL;
	pname = talloc_get_name(ptr);
	if (likely(pname == name || strcmp(pname, name) == 0)) {
		return discard_const_p(void, ptr);
	}
	return NULL;
}

static void talloc_abort_type_missmatch(const char *location,
					const char *name,
					const char *expected)
{
	const char *reason;

	reason = talloc_asprintf(NULL,
				 "%s: Type mismatch: name[%s] expected[%s]",
				 location,
				 name?name:"NULL",
				 expected);
	if (!reason) {
		reason = "Type mismatch";
	}

	talloc_abort(reason);
}

_PUBLIC_ void *_talloc_get_type_abort(const void *ptr, const char *name, const char *location)
{
	const char *pname;

	if (unlikely(ptr == NULL)) {
		talloc_abort_type_missmatch(location, NULL, name);
		return NULL;
	}

	pname = talloc_get_name(ptr);
	if (likely(pname == name || strcmp(pname, name) == 0)) {
		return discard_const_p(void, ptr);
	}

	talloc_abort_type_missmatch(location, pname, name);
	return NULL;
}

/*
  this is for compatibility with older versions of talloc
*/
_PUBLIC_ void *talloc_init(const char *fmt, ...)
{
	va_list ap;
	void *ptr;
	const char *name;

	ptr = __talloc(NULL, 0);
	if (unlikely(ptr == NULL)) return NULL;

	va_start(ap, fmt);
	name = talloc_set_name_v(ptr, fmt, ap);
	va_end(ap);

	if (unlikely(name == NULL)) {
		_talloc_free_internal(ptr, __location__);
		return NULL;
	}

	return ptr;
}

static inline void _talloc_free_children_internal(struct talloc_chunk *tc,
						  void *ptr,
						  const char *location)
{
	while (tc->child) {
		/* we need to work out who will own an abandoned child
		   if it cannot be freed. In priority order, the first
		   choice is owner of any remaining reference to this
		   pointer, the second choice is our parent, and the
		   final choice is the null context. */
		void *child = TC_PTR_FROM_CHUNK(tc->child);
		const void *new_parent = null_context;
		struct talloc_chunk *old_parent = NULL;
		if (unlikely(tc->child->refs)) {
			struct talloc_chunk *p = talloc_parent_chunk(tc->child->refs);
			if (p) new_parent = TC_PTR_FROM_CHUNK(p);
		}
		if (unlikely(_talloc_free_internal(child, location) == -1)) {
			if (new_parent == null_context) {
				struct talloc_chunk *p = talloc_parent_chunk(ptr);
				if (p) new_parent = TC_PTR_FROM_CHUNK(p);
			}
			_talloc_steal_internal(new_parent, child);
		}
	}
}

/*
  this is a replacement for the Samba3 talloc_destroy_pool functionality. It
  should probably not be used in new code. It's in here to keep the talloc
  code consistent across Samba 3 and 4.
*/
_PUBLIC_ void talloc_free_children(void *ptr)
{
	struct talloc_chunk *tc_name = NULL;
	struct talloc_chunk *tc;

	if (unlikely(ptr == NULL)) {
		return;
	}

	tc = talloc_chunk_from_ptr(ptr);

	/* we do not want to free the context name if it is a child .. */
	if (likely(tc->child)) {
		for (tc_name = tc->child; tc_name; tc_name = tc_name->next) {
			if (tc->name == TC_PTR_FROM_CHUNK(tc_name)) break;
		}
		if (tc_name) {
			_TLIST_REMOVE(tc->child, tc_name);
			if (tc->child) {
				tc->child->parent = tc;
			}
		}
	}

	_talloc_free_children_internal(tc, ptr, __location__);

	/* .. so we put it back after all other children have been freed */
	if (tc_name) {
		if (tc->child) {
			tc->child->parent = NULL;
		}
		tc_name->parent = tc;
		_TLIST_ADD(tc->child, tc_name);
	}
}

/* 
   Allocate a bit of memory as a child of an existing pointer
*/
_PUBLIC_ void *_talloc(const void *context, size_t size)
{
	return __talloc(context, size);
}

/*
  externally callable talloc_set_name_const()
*/
_PUBLIC_ void talloc_set_name_const(const void *ptr, const char *name)
{
	_talloc_set_name_const(ptr, name);
}

/*
  create a named talloc pointer. Any talloc pointer can be named, and
  talloc_named() operates just like talloc() except that it allows you
  to name the pointer.
*/
_PUBLIC_ void *talloc_named_const(const void *context, size_t size, const char *name)
{
	return _talloc_named_const(context, size, name);
}

/* 
   free a talloc pointer. This also frees all child pointers of this 
   pointer recursively

   return 0 if the memory is actually freed, otherwise -1. The memory
   will not be freed if the ref_count is > 1 or the destructor (if
   any) returns non-zero
*/
_PUBLIC_ int _talloc_free(void *ptr, const char *location)
{
	struct talloc_chunk *tc;

	if (unlikely(ptr == NULL)) {
		return -1;
	}
	
	tc = talloc_chunk_from_ptr(ptr);
	
	if (unlikely(tc->refs != NULL)) {
		struct talloc_reference_handle *h;

		if (talloc_parent(ptr) == null_context && tc->refs->next == NULL) {
			/* in this case we do know which parent should
			   get this pointer, as there is really only
			   one parent */
			return talloc_unlink(null_context, ptr);
		}

		talloc_log("ERROR: talloc_free with references at %s\n",
			   location);

		for (h=tc->refs; h; h=h->next) {
			talloc_log("\treference at %s\n",
				   h->location);
		}
		return -1;
	}
	
	return _talloc_free_internal(ptr, location);
}



/*
  A talloc version of realloc. The context argument is only used if
  ptr is NULL
*/
_PUBLIC_ void *_talloc_realloc(const void *context, void *ptr, size_t size, const char *name)
{
	struct talloc_chunk *tc;
	void *new_ptr;
	bool malloced = false;
	struct talloc_chunk *pool_tc = NULL;

	/* size zero is equivalent to free() */
	if (unlikely(size == 0)) {
		talloc_unlink(context, ptr);
		return NULL;
	}

	if (unlikely(size >= MAX_TALLOC_SIZE)) {
		return NULL;
	}

	/* realloc(NULL) is equivalent to malloc() */
	if (ptr == NULL) {
		return _talloc_named_const(context, size, name);
	}

	tc = talloc_chunk_from_ptr(ptr);

	/* don't allow realloc on referenced pointers */
	if (unlikely(tc->refs)) {
		return NULL;
	}

	/* don't let anybody try to realloc a talloc_pool */
	if (unlikely(tc->flags & TALLOC_FLAG_POOL)) {
		return NULL;
	}

	/* don't let anybody try to realloc a talloc_pool */
	if (unlikely(tc->flags & TALLOC_FLAG_POOLMEM)) {
		pool_tc = (struct talloc_chunk *)tc->pool;
	}

#if (ALWAYS_REALLOC == 0)
	/* don't shrink if we have less than 1k to gain */
	if (size < tc->size) {
		if (pool_tc) {
			void *next_tc = TC_POOLMEM_NEXT_CHUNK(tc);
			TC_INVALIDATE_SHRINK_CHUNK(tc, size);
			tc->size = size;
			if (next_tc == pool_tc->pool) {
				pool_tc->pool = TC_POOLMEM_NEXT_CHUNK(tc);
			}
			return ptr;
		} else if ((tc->size - size) < 1024) {
			/*
			 * if we call TC_INVALIDATE_SHRINK_CHUNK() here
			 * we would need to call TC_UNDEFINE_GROW_CHUNK()
			 * after each realloc call, which slows down
			 * testing a lot :-(.
			 *
			 * That is why we only mark memory as undefined here.
			 */
			TC_UNDEFINE_SHRINK_CHUNK(tc, size);

			/* do not shrink if we have less than 1k to gain */
			tc->size = size;
			return ptr;
		}
	} else if (tc->size == size) {
		/*
		 * do not change the pointer if it is exactly
		 * the same size.
		 */
		return ptr;
	}
#endif

	/* by resetting magic we catch users of the old memory */
	tc->flags |= TALLOC_FLAG_FREE;

#if ALWAYS_REALLOC
	if (pool_tc) {
		new_ptr = talloc_alloc_pool(tc, size + TC_HDR_SIZE);
		*talloc_pool_objectcount(pool_tc) -= 1;

		if (new_ptr == NULL) {
			new_ptr = malloc(TC_HDR_SIZE+size);
			malloced = true;
		}

		if (new_ptr) {
			memcpy(new_ptr, tc, MIN(tc->size,size) + TC_HDR_SIZE);
			TC_INVALIDATE_FULL_CHUNK(tc);
		}
	} else {
		new_ptr = malloc(size + TC_HDR_SIZE);
		if (new_ptr) {
			memcpy(new_ptr, tc, MIN(tc->size, size) + TC_HDR_SIZE);
			free(tc);
		}
	}
#else
	if (pool_tc) {
		void *next_tc = TC_POOLMEM_NEXT_CHUNK(tc);
		size_t old_chunk_size = TC_POOLMEM_CHUNK_SIZE(tc);
		size_t new_chunk_size = TC_ALIGN16(TC_HDR_SIZE + size);
		size_t space_needed;
		size_t space_left;
		unsigned int chunk_count = *talloc_pool_objectcount(pool_tc);

		if (!(pool_tc->flags & TALLOC_FLAG_FREE)) {
			chunk_count -= 1;
		}

		if (chunk_count == 1) {
			/*
			 * optimize for the case where 'tc' is the only
			 * chunk in the pool.
			 */
			space_needed = new_chunk_size;
			space_left = pool_tc->size - TALLOC_POOL_HDR_SIZE;

			if (space_left >= space_needed) {
				size_t old_used = TC_HDR_SIZE + tc->size;
				size_t new_used = TC_HDR_SIZE + size;
				pool_tc->pool = TC_POOL_FIRST_CHUNK(pool_tc);
#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_UNDEFINED)
				/*
				 * we need to prepare the memmove into
				 * the unaccessable area.
				 */
				{
					size_t diff = PTR_DIFF(tc, pool_tc->pool);
					size_t flen = MIN(diff, old_used);
					char *fptr = (char *)pool_tc->pool;
					VALGRIND_MAKE_MEM_UNDEFINED(fptr, flen);
				}
#endif
				memmove(pool_tc->pool, tc, old_used);
				new_ptr = pool_tc->pool;

				tc = (struct talloc_chunk *)new_ptr;
				TC_UNDEFINE_GROW_CHUNK(tc, size);

				/*
				 * first we do not align the pool pointer
				 * because we want to invalidate the padding
				 * too.
				 */
				pool_tc->pool = new_used + (char *)new_ptr;
				TC_INVALIDATE_POOL(pool_tc);

				/* now the aligned pointer */
				pool_tc->pool = new_chunk_size + (char *)new_ptr;
				goto got_new_ptr;
			}

			next_tc = NULL;
		}

		if (new_chunk_size == old_chunk_size) {
			TC_UNDEFINE_GROW_CHUNK(tc, size);
			tc->flags &= ~TALLOC_FLAG_FREE;
			tc->size = size;
			return ptr;
		}

		if (next_tc == pool_tc->pool) {
			/*
			 * optimize for the case where 'tc' is the last
			 * chunk in the pool.
			 */
			space_needed = new_chunk_size - old_chunk_size;
			space_left = TC_POOL_SPACE_LEFT(pool_tc);

			if (space_left >= space_needed) {
				TC_UNDEFINE_GROW_CHUNK(tc, size);
				tc->flags &= ~TALLOC_FLAG_FREE;
				tc->size = size;
				pool_tc->pool = TC_POOLMEM_NEXT_CHUNK(tc);
				return ptr;
			}
		}

		new_ptr = talloc_alloc_pool(tc, size + TC_HDR_SIZE);

		if (new_ptr == NULL) {
			new_ptr = malloc(TC_HDR_SIZE+size);
			malloced = true;
		}

		if (new_ptr) {
			memcpy(new_ptr, tc, MIN(tc->size,size) + TC_HDR_SIZE);

			_talloc_free_poolmem(tc, __location__ "_talloc_realloc");
		}
	}
	else {
		new_ptr = realloc(tc, size + TC_HDR_SIZE);
	}
got_new_ptr:
#endif
	if (unlikely(!new_ptr)) {	
		tc->flags &= ~TALLOC_FLAG_FREE; 
		return NULL; 
	}

	tc = (struct talloc_chunk *)new_ptr;
	tc->flags &= ~TALLOC_FLAG_FREE;
	if (malloced) {
		tc->flags &= ~TALLOC_FLAG_POOLMEM;
	}
	if (tc->parent) {
		tc->parent->child = tc;
	}
	if (tc->child) {
		tc->child->parent = tc;
	}

	if (tc->prev) {
		tc->prev->next = tc;
	}
	if (tc->next) {
		tc->next->prev = tc;
	}

	tc->size = size;
	_talloc_set_name_const(TC_PTR_FROM_CHUNK(tc), name);

	return TC_PTR_FROM_CHUNK(tc);
}

/*
  a wrapper around talloc_steal() for situations where you are moving a pointer
  between two structures, and want the old pointer to be set to NULL
*/
_PUBLIC_ void *_talloc_move(const void *new_ctx, const void *_pptr)
{
	const void **pptr = discard_const_p(const void *,_pptr);
	void *ret = talloc_steal(new_ctx, discard_const_p(void, *pptr));
	(*pptr) = NULL;
	return ret;
}

/*
  return the total size of a talloc pool (subtree)
*/
_PUBLIC_ size_t talloc_total_size(const void *ptr)
{
	size_t total = 0;
	struct talloc_chunk *c, *tc;

	if (ptr == NULL) {
		ptr = null_context;
	}
	if (ptr == NULL) {
		return 0;
	}

	tc = talloc_chunk_from_ptr(ptr);

	if (tc->flags & TALLOC_FLAG_LOOP) {
		return 0;
	}

	tc->flags |= TALLOC_FLAG_LOOP;

	if (likely(tc->name != TALLOC_MAGIC_REFERENCE)) {
		total = tc->size;
	}
	for (c=tc->child;c;c=c->next) {
		total += talloc_total_size(TC_PTR_FROM_CHUNK(c));
	}

	tc->flags &= ~TALLOC_FLAG_LOOP;

	return total;
}

/*
  return the total number of blocks in a talloc pool (subtree)
*/
_PUBLIC_ size_t talloc_total_blocks(const void *ptr)
{
	size_t total = 0;
	struct talloc_chunk *c, *tc;

	if (ptr == NULL) {
		ptr = null_context;
	}
	if (ptr == NULL) {
		return 0;
	}

	tc = talloc_chunk_from_ptr(ptr);

	if (tc->flags & TALLOC_FLAG_LOOP) {
		return 0;
	}

	tc->flags |= TALLOC_FLAG_LOOP;

	total++;
	for (c=tc->child;c;c=c->next) {
		total += talloc_total_blocks(TC_PTR_FROM_CHUNK(c));
	}

	tc->flags &= ~TALLOC_FLAG_LOOP;

	return total;
}

/*
  return the number of external references to a pointer
*/
_PUBLIC_ size_t talloc_reference_count(const void *ptr)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	struct talloc_reference_handle *h;
	size_t ret = 0;

	for (h=tc->refs;h;h=h->next) {
		ret++;
	}
	return ret;
}

/*
  report on memory usage by all children of a pointer, giving a full tree view
*/
_PUBLIC_ void talloc_report_depth_cb(const void *ptr, int depth, int max_depth,
			    void (*callback)(const void *ptr,
			  		     int depth, int max_depth,
					     int is_ref,
					     void *private_data),
			    void *private_data)
{
	struct talloc_chunk *c, *tc;

	if (ptr == NULL) {
		ptr = null_context;
	}
	if (ptr == NULL) return;

	tc = talloc_chunk_from_ptr(ptr);

	if (tc->flags & TALLOC_FLAG_LOOP) {
		return;
	}

	callback(ptr, depth, max_depth, 0, private_data);

	if (max_depth >= 0 && depth >= max_depth) {
		return;
	}

	tc->flags |= TALLOC_FLAG_LOOP;
	for (c=tc->child;c;c=c->next) {
		if (c->name == TALLOC_MAGIC_REFERENCE) {
			struct talloc_reference_handle *h = (struct talloc_reference_handle *)TC_PTR_FROM_CHUNK(c);
			callback(h->ptr, depth + 1, max_depth, 1, private_data);
		} else {
			talloc_report_depth_cb(TC_PTR_FROM_CHUNK(c), depth + 1, max_depth, callback, private_data);
		}
	}
	tc->flags &= ~TALLOC_FLAG_LOOP;
}

static void talloc_report_depth_FILE_helper(const void *ptr, int depth, int max_depth, int is_ref, void *_f)
{
	const char *name = talloc_get_name(ptr);
	FILE *f = (FILE *)_f;

	if (is_ref) {
		fprintf(f, "%*sreference to: %s\n", depth*4, "", name);
		return;
	}

	if (depth == 0) {
		fprintf(f,"%stalloc report on '%s' (total %6lu bytes in %3lu blocks)\n", 
			(max_depth < 0 ? "full " :""), name,
			(unsigned long)talloc_total_size(ptr),
			(unsigned long)talloc_total_blocks(ptr));
		return;
	}

	fprintf(f, "%*s%-30s contains %6lu bytes in %3lu blocks (ref %d) %p\n", 
		depth*4, "",
		name,
		(unsigned long)talloc_total_size(ptr),
		(unsigned long)talloc_total_blocks(ptr),
		(int)talloc_reference_count(ptr), ptr);

#if 0
	fprintf(f, "content: ");
	if (talloc_total_size(ptr)) {
		int tot = talloc_total_size(ptr);
		int i;

		for (i = 0; i < tot; i++) {
			if ((((char *)ptr)[i] > 31) && (((char *)ptr)[i] < 126)) {
				fprintf(f, "%c", ((char *)ptr)[i]);
			} else {
				fprintf(f, "~%02x", ((char *)ptr)[i]);
			}
		}
	}
	fprintf(f, "\n");
#endif
}

/*
  report on memory usage by all children of a pointer, giving a full tree view
*/
_PUBLIC_ void talloc_report_depth_file(const void *ptr, int depth, int max_depth, FILE *f)
{
	if (f) {
		talloc_report_depth_cb(ptr, depth, max_depth, talloc_report_depth_FILE_helper, f);
		fflush(f);
	}
}

/*
  report on memory usage by all children of a pointer, giving a full tree view
*/
_PUBLIC_ void talloc_report_full(const void *ptr, FILE *f)
{
	talloc_report_depth_file(ptr, 0, -1, f);
}

/*
  report on memory usage by all children of a pointer
*/
_PUBLIC_ void talloc_report(const void *ptr, FILE *f)
{
	talloc_report_depth_file(ptr, 0, 1, f);
}

/*
  report on any memory hanging off the null context
*/
static void talloc_report_null(void)
{
	if (talloc_total_size(null_context) != 0) {
		talloc_report(null_context, stderr);
	}
}

/*
  report on any memory hanging off the null context
*/
static void talloc_report_null_full(void)
{
	if (talloc_total_size(null_context) != 0) {
		talloc_report_full(null_context, stderr);
	}
}

/*
  enable tracking of the NULL context
*/
_PUBLIC_ void talloc_enable_null_tracking(void)
{
	if (null_context == NULL) {
		null_context = _talloc_named_const(NULL, 0, "null_context");
		if (autofree_context != NULL) {
			talloc_reparent(NULL, null_context, autofree_context);
		}
	}
}

/*
  enable tracking of the NULL context, not moving the autofree context
  into the NULL context. This is needed for the talloc testsuite
*/
_PUBLIC_ void talloc_enable_null_tracking_no_autofree(void)
{
	if (null_context == NULL) {
		null_context = _talloc_named_const(NULL, 0, "null_context");
	}
}

/*
  disable tracking of the NULL context
*/
_PUBLIC_ void talloc_disable_null_tracking(void)
{
	if (null_context != NULL) {
		/* we have to move any children onto the real NULL
		   context */
		struct talloc_chunk *tc, *tc2;
		tc = talloc_chunk_from_ptr(null_context);
		for (tc2 = tc->child; tc2; tc2=tc2->next) {
			if (tc2->parent == tc) tc2->parent = NULL;
			if (tc2->prev == tc) tc2->prev = NULL;
		}
		for (tc2 = tc->next; tc2; tc2=tc2->next) {
			if (tc2->parent == tc) tc2->parent = NULL;
			if (tc2->prev == tc) tc2->prev = NULL;
		}
		tc->child = NULL;
		tc->next = NULL;
	}
	talloc_free(null_context);
	null_context = NULL;
}

/*
  enable leak reporting on exit
*/
_PUBLIC_ void talloc_enable_leak_report(void)
{
	talloc_enable_null_tracking();
	atexit(talloc_report_null);
}

/*
  enable full leak reporting on exit
*/
_PUBLIC_ void talloc_enable_leak_report_full(void)
{
	talloc_enable_null_tracking();
	atexit(talloc_report_null_full);
}

/* 
   talloc and zero memory. 
*/
_PUBLIC_ void *_talloc_zero(const void *ctx, size_t size, const char *name)
{
	void *p = _talloc_named_const(ctx, size, name);

	if (p) {
		memset(p, '\0', size);
	}

	return p;
}

/*
  memdup with a talloc. 
*/
_PUBLIC_ void *_talloc_memdup(const void *t, const void *p, size_t size, const char *name)
{
	void *newp = _talloc_named_const(t, size, name);

	if (likely(newp)) {
		memcpy(newp, p, size);
	}

	return newp;
}

static inline char *__talloc_strlendup(const void *t, const char *p, size_t len)
{
	char *ret;

	ret = (char *)__talloc(t, len + 1);
	if (unlikely(!ret)) return NULL;

	memcpy(ret, p, len);
	ret[len] = 0;

	_talloc_set_name_const(ret, ret);
	return ret;
}

/*
  strdup with a talloc
*/
_PUBLIC_ char *talloc_strdup(const void *t, const char *p)
{
	if (unlikely(!p)) return NULL;
	return __talloc_strlendup(t, p, strlen(p));
}

/*
  strndup with a talloc
*/
_PUBLIC_ char *talloc_strndup(const void *t, const char *p, size_t n)
{
	if (unlikely(!p)) return NULL;
	return __talloc_strlendup(t, p, strnlen(p, n));
}

static inline char *__talloc_strlendup_append(char *s, size_t slen,
					      const char *a, size_t alen)
{
	char *ret;

	ret = talloc_realloc(NULL, s, char, slen + alen + 1);
	if (unlikely(!ret)) return NULL;

	/* append the string and the trailing \0 */
	memcpy(&ret[slen], a, alen);
	ret[slen+alen] = 0;

	_talloc_set_name_const(ret, ret);
	return ret;
}

/*
 * Appends at the end of the string.
 */
_PUBLIC_ char *talloc_strdup_append(char *s, const char *a)
{
	if (unlikely(!s)) {
		return talloc_strdup(NULL, a);
	}

	if (unlikely(!a)) {
		return s;
	}

	return __talloc_strlendup_append(s, strlen(s), a, strlen(a));
}

/*
 * Appends at the end of the talloc'ed buffer,
 * not the end of the string.
 */
_PUBLIC_ char *talloc_strdup_append_buffer(char *s, const char *a)
{
	size_t slen;

	if (unlikely(!s)) {
		return talloc_strdup(NULL, a);
	}

	if (unlikely(!a)) {
		return s;
	}

	slen = talloc_get_size(s);
	if (likely(slen > 0)) {
		slen--;
	}

	return __talloc_strlendup_append(s, slen, a, strlen(a));
}

/*
 * Appends at the end of the string.
 */
_PUBLIC_ char *talloc_strndup_append(char *s, const char *a, size_t n)
{
	if (unlikely(!s)) {
		return talloc_strdup(NULL, a);
	}

	if (unlikely(!a)) {
		return s;
	}

	return __talloc_strlendup_append(s, strlen(s), a, strnlen(a, n));
}

/*
 * Appends at the end of the talloc'ed buffer,
 * not the end of the string.
 */
_PUBLIC_ char *talloc_strndup_append_buffer(char *s, const char *a, size_t n)
{
	size_t slen;

	if (unlikely(!s)) {
		return talloc_strdup(NULL, a);
	}

	if (unlikely(!a)) {
		return s;
	}

	slen = talloc_get_size(s);
	if (likely(slen > 0)) {
		slen--;
	}

	return __talloc_strlendup_append(s, slen, a, strnlen(a, n));
}

#ifndef HAVE_VA_COPY
#ifdef HAVE___VA_COPY
#define va_copy(dest, src) __va_copy(dest, src)
#else
#define va_copy(dest, src) (dest) = (src)
#endif
#endif

_PUBLIC_ char *talloc_vasprintf(const void *t, const char *fmt, va_list ap)
{
	int len;
	char *ret;
	va_list ap2;
	char c;

	/* this call looks strange, but it makes it work on older solaris boxes */
	va_copy(ap2, ap);
	len = vsnprintf(&c, 1, fmt, ap2);
	va_end(ap2);
	if (unlikely(len < 0)) {
		return NULL;
	}

	ret = (char *)__talloc(t, len+1);
	if (unlikely(!ret)) return NULL;

	va_copy(ap2, ap);
	vsnprintf(ret, len+1, fmt, ap2);
	va_end(ap2);

	_talloc_set_name_const(ret, ret);
	return ret;
}


/*
  Perform string formatting, and return a pointer to newly allocated
  memory holding the result, inside a memory pool.
 */
_PUBLIC_ char *talloc_asprintf(const void *t, const char *fmt, ...)
{
	va_list ap;
	char *ret;

	va_start(ap, fmt);
	ret = talloc_vasprintf(t, fmt, ap);
	va_end(ap);
	return ret;
}

static inline char *__talloc_vaslenprintf_append(char *s, size_t slen,
						 const char *fmt, va_list ap)
						 PRINTF_ATTRIBUTE(3,0);

static inline char *__talloc_vaslenprintf_append(char *s, size_t slen,
						 const char *fmt, va_list ap)
{
	ssize_t alen;
	va_list ap2;
	char c;

	va_copy(ap2, ap);
	alen = vsnprintf(&c, 1, fmt, ap2);
	va_end(ap2);

	if (alen <= 0) {
		/* Either the vsnprintf failed or the format resulted in
		 * no characters being formatted. In the former case, we
		 * ought to return NULL, in the latter we ought to return
		 * the original string. Most current callers of this
		 * function expect it to never return NULL.
		 */
		return s;
	}

	s = talloc_realloc(NULL, s, char, slen + alen + 1);
	if (!s) return NULL;

	va_copy(ap2, ap);
	vsnprintf(s + slen, alen + 1, fmt, ap2);
	va_end(ap2);

	_talloc_set_name_const(s, s);
	return s;
}

/**
 * Realloc @p s to append the formatted result of @p fmt and @p ap,
 * and return @p s, which may have moved.  Good for gradually
 * accumulating output into a string buffer. Appends at the end
 * of the string.
 **/
_PUBLIC_ char *talloc_vasprintf_append(char *s, const char *fmt, va_list ap)
{
	if (unlikely(!s)) {
		return talloc_vasprintf(NULL, fmt, ap);
	}

	return __talloc_vaslenprintf_append(s, strlen(s), fmt, ap);
}

/**
 * Realloc @p s to append the formatted result of @p fmt and @p ap,
 * and return @p s, which may have moved. Always appends at the
 * end of the talloc'ed buffer, not the end of the string.
 **/
_PUBLIC_ char *talloc_vasprintf_append_buffer(char *s, const char *fmt, va_list ap)
{
	size_t slen;

	if (unlikely(!s)) {
		return talloc_vasprintf(NULL, fmt, ap);
	}

	slen = talloc_get_size(s);
	if (likely(slen > 0)) {
		slen--;
	}

	return __talloc_vaslenprintf_append(s, slen, fmt, ap);
}

/*
  Realloc @p s to append the formatted result of @p fmt and return @p
  s, which may have moved.  Good for gradually accumulating output
  into a string buffer.
 */
_PUBLIC_ char *talloc_asprintf_append(char *s, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	s = talloc_vasprintf_append(s, fmt, ap);
	va_end(ap);
	return s;
}

/*
  Realloc @p s to append the formatted result of @p fmt and return @p
  s, which may have moved.  Good for gradually accumulating output
  into a buffer.
 */
_PUBLIC_ char *talloc_asprintf_append_buffer(char *s, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	s = talloc_vasprintf_append_buffer(s, fmt, ap);
	va_end(ap);
	return s;
}

/*
  alloc an array, checking for integer overflow in the array size
*/
_PUBLIC_ void *_talloc_array(const void *ctx, size_t el_size, unsigned count, const char *name)
{
	if (count >= MAX_TALLOC_SIZE/el_size) {
		return NULL;
	}
	return _talloc_named_const(ctx, el_size * count, name);
}

/*
  alloc an zero array, checking for integer overflow in the array size
*/
_PUBLIC_ void *_talloc_zero_array(const void *ctx, size_t el_size, unsigned count, const char *name)
{
	if (count >= MAX_TALLOC_SIZE/el_size) {
		return NULL;
	}
	return _talloc_zero(ctx, el_size * count, name);
}

/*
  realloc an array, checking for integer overflow in the array size
*/
_PUBLIC_ void *_talloc_realloc_array(const void *ctx, void *ptr, size_t el_size, unsigned count, const char *name)
{
	if (count >= MAX_TALLOC_SIZE/el_size) {
		return NULL;
	}
	return _talloc_realloc(ctx, ptr, el_size * count, name);
}

/*
  a function version of talloc_realloc(), so it can be passed as a function pointer
  to libraries that want a realloc function (a realloc function encapsulates
  all the basic capabilities of an allocation library, which is why this is useful)
*/
_PUBLIC_ void *talloc_realloc_fn(const void *context, void *ptr, size_t size)
{
	return _talloc_realloc(context, ptr, size, NULL);
}


static int talloc_autofree_destructor(void *ptr)
{
	autofree_context = NULL;
	return 0;
}

static void talloc_autofree(void)
{
	talloc_free(autofree_context);
}

/*
  return a context which will be auto-freed on exit
  this is useful for reducing the noise in leak reports
*/
_PUBLIC_ void *talloc_autofree_context(void)
{
	if (autofree_context == NULL) {
		autofree_context = _talloc_named_const(NULL, 0, "autofree_context");
		talloc_set_destructor(autofree_context, talloc_autofree_destructor);
		atexit(talloc_autofree);
	}
	return autofree_context;
}

_PUBLIC_ size_t talloc_get_size(const void *context)
{
	struct talloc_chunk *tc;

	if (context == NULL) {
		context = null_context;
	}
	if (context == NULL) {
		return 0;
	}

	tc = talloc_chunk_from_ptr(context);

	return tc->size;
}

/*
  find a parent of this context that has the given name, if any
*/
_PUBLIC_ void *talloc_find_parent_byname(const void *context, const char *name)
{
	struct talloc_chunk *tc;

	if (context == NULL) {
		return NULL;
	}

	tc = talloc_chunk_from_ptr(context);
	while (tc) {
		if (tc->name && strcmp(tc->name, name) == 0) {
			return TC_PTR_FROM_CHUNK(tc);
		}
		while (tc && tc->prev) tc = tc->prev;
		if (tc) {
			tc = tc->parent;
		}
	}
	return NULL;
}

/*
  show the parentage of a context
*/
_PUBLIC_ void talloc_show_parents(const void *context, FILE *file)
{
	struct talloc_chunk *tc;

	if (context == NULL) {
		fprintf(file, "talloc no parents for NULL\n");
		return;
	}

	tc = talloc_chunk_from_ptr(context);
	fprintf(file, "talloc parents of '%s'\n", talloc_get_name(context));
	while (tc) {
		fprintf(file, "\t'%s'\n", talloc_get_name(TC_PTR_FROM_CHUNK(tc)));
		while (tc && tc->prev) tc = tc->prev;
		if (tc) {
			tc = tc->parent;
		}
	}
	fflush(file);
}

/*
  return 1 if ptr is a parent of context
*/
static int _talloc_is_parent(const void *context, const void *ptr, int depth)
{
	struct talloc_chunk *tc;

	if (context == NULL) {
		return 0;
	}

	tc = talloc_chunk_from_ptr(context);
	while (tc && depth > 0) {
		if (TC_PTR_FROM_CHUNK(tc) == ptr) return 1;
		while (tc && tc->prev) tc = tc->prev;
		if (tc) {
			tc = tc->parent;
			depth--;
		}
	}
	return 0;
}

/*
  return 1 if ptr is a parent of context
*/
_PUBLIC_ int talloc_is_parent(const void *context, const void *ptr)
{
	return _talloc_is_parent(context, ptr, TALLOC_MAX_DEPTH);
}
