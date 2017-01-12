/* ==========================================================================
 * llrb.h - Iterative Left-leaning Red-Black Tree.
 * --------------------------------------------------------------------------
 * Copyright (c) 2011, 2013  William Ahern <william@25thandClement.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 * --------------------------------------------------------------------------
 * CREDITS:
 * 	o Algorithm courtesy of Robert Sedgewick, "Left-leaning Red-Black
 * 	  Trees" (September 2008); and Robert Sedgewick and Kevin Wayne,
 * 	  Algorithms (4th ed. 2011).
 *
 * 	  Sedgewick touts the simplicity of the recursive implementation,
 * 	  but at least for the 2-3 tree variant the iterative approach is
 * 	  almost line-for-line identical. The magic of C pointers helps;
 * 	  it'd be uglier with Java.
 *
 * 	  A couple of missing NULL checks were added to Sedgewick's deletion
 * 	  example, and insert was optimized to short-circuit rotations when
 * 	  walking up the tree.
 * 
 * 	o Code implemented in the fashion of Niels Provos' excellent *BSD
 * 	  sys/tree.h pre-processor library.
 *
 * 	  Regarding relative performance, I've refrained from sharing my own
 * 	  benchmarks. Differences in run-time speed were too correlated to
 * 	  compiler options and other external factors.
 *
 * 	  Provos' delete implementation doesn't need to start at the root of
 * 	  the tree. However, RB_REMOVE must be passed the actual node to be
 * 	  removed. LLRB_REMOVE merely requires a key, much like
 * 	  RB_FIND/LLRB_FIND.
 * ==========================================================================
 */
#ifndef LLRB_H
#define LLRB_H

#define LLRB_VENDOR "william@25thandClement.com"
#define LLRB_VERSION 0x20130925

#ifndef LLRB_STATIC
#ifdef __GNUC__
#define LLRB_STATIC __attribute__((__unused__)) static
#else
#define LLRB_STATIC static
#endif
#endif

#define LLRB_HEAD(name, type) \
struct name { struct type *rbh_root; }

#define LLRB_INITIALIZER(root) { 0 }

#define LLRB_INIT(root) do { (root)->rbh_root = 0; } while (0)

#define LLRB_BLACK 0
#define LLRB_RED   1

#define LLRB_ENTRY(type) \
struct { struct type *rbe_left, *rbe_right, *rbe_parent; _Bool rbe_color; }

#define LLRB_LEFT(elm, field) (elm)->field.rbe_left
#define LLRB_RIGHT(elm, field) (elm)->field.rbe_right
#define LLRB_PARENT(elm, field) (elm)->field.rbe_parent
#define LLRB_EDGE(head, elm, field) (((elm) == LLRB_ROOT(head))? &LLRB_ROOT(head) : ((elm) == LLRB_LEFT(LLRB_PARENT((elm), field), field))? &LLRB_LEFT(LLRB_PARENT((elm), field), field) : &LLRB_RIGHT(LLRB_PARENT((elm), field), field))
#define LLRB_COLOR(elm, field) (elm)->field.rbe_color
#define LLRB_ROOT(head) (head)->rbh_root
#define LLRB_EMPTY(head) ((head)->rbh_root == 0)
#define LLRB_ISRED(elm, field) ((elm) && LLRB_COLOR((elm), field) == LLRB_RED)

#define LLRB_PROTOTYPE(name, type, field, cmp) \
	LLRB_PROTOTYPE_INTERNAL(name, type, field, cmp,)
#define LLRB_PROTOTYPE_STATIC(name, type, field, cmp) \
	LLRB_PROTOTYPE_INTERNAL(name, type, field, cmp, LLRB_STATIC)
#define LLRB_PROTOTYPE_INTERNAL(name, type, field, cmp, attr) \
attr struct type *name##_LLRB_INSERT(struct name *, struct type *); \
attr struct type *name##_LLRB_DELETE(struct name *, struct type *); \
attr struct type *name##_LLRB_FIND(struct name *, struct type *); \
attr struct type *name##_LLRB_MIN(struct type *); \
attr struct type *name##_LLRB_MAX(struct type *); \
attr struct type *name##_LLRB_NEXT(struct type *);

#define LLRB_GENERATE(name, type, field, cmp) \
	LLRB_GENERATE_INTERNAL(name, type, field, cmp,)
#define LLRB_GENERATE_STATIC(name, type, field, cmp) \
	LLRB_GENERATE_INTERNAL(name, type, field, cmp, LLRB_STATIC)
#define LLRB_GENERATE_INTERNAL(name, type, field, cmp, attr) \
static inline void name##_LLRB_ROTL(struct type **pivot) { \
	struct type *a = *pivot; \
	struct type *b = LLRB_RIGHT(a, field); \
	if ((LLRB_RIGHT(a, field) = LLRB_LEFT(b, field))) \
		LLRB_PARENT(LLRB_RIGHT(a, field), field) = a; \
	LLRB_LEFT(b, field) = a; \
	LLRB_COLOR(b, field) = LLRB_COLOR(a, field); \
	LLRB_COLOR(a, field) = LLRB_RED; \
	LLRB_PARENT(b, field) = LLRB_PARENT(a, field); \
	LLRB_PARENT(a, field) = b; \
	*pivot = b; \
} \
static inline void name##_LLRB_ROTR(struct type **pivot) { \
	struct type *b = *pivot; \
	struct type *a = LLRB_LEFT(b, field); \
	if ((LLRB_LEFT(b, field) = LLRB_RIGHT(a, field))) \
		LLRB_PARENT(LLRB_LEFT(b, field), field) = b; \
	LLRB_RIGHT(a, field) = b; \
	LLRB_COLOR(a, field) = LLRB_COLOR(b, field); \
	LLRB_COLOR(b, field) = LLRB_RED; \
	LLRB_PARENT(a, field) = LLRB_PARENT(b, field); \
	LLRB_PARENT(b, field) = a; \
	*pivot = a; \
} \
static inline void name##_LLRB_FLIP(struct type *root) { \
	LLRB_COLOR(root, field) = !LLRB_COLOR(root, field); \
	LLRB_COLOR(LLRB_LEFT(root, field), field) = !LLRB_COLOR(LLRB_LEFT(root, field), field); \
	LLRB_COLOR(LLRB_RIGHT(root, field), field) = !LLRB_COLOR(LLRB_RIGHT(root, field), field); \
} \
static inline void name##_LLRB_FIXUP(struct type **root) { \
	if (LLRB_ISRED(LLRB_RIGHT(*root, field), field) && !LLRB_ISRED(LLRB_LEFT(*root, field), field)) \
		name##_LLRB_ROTL(root); \
	if (LLRB_ISRED(LLRB_LEFT(*root, field), field) && LLRB_ISRED(LLRB_LEFT(LLRB_LEFT(*root, field), field), field)) \
		name##_LLRB_ROTR(root); \
	if (LLRB_ISRED(LLRB_LEFT(*root, field), field) && LLRB_ISRED(LLRB_RIGHT(*root, field), field)) \
		name##_LLRB_FLIP(*root); \
} \
attr struct type *name##_LLRB_INSERT(struct name *head, struct type *elm) { \
	struct type **root = &LLRB_ROOT(head); \
	struct type *parent = 0; \
	while (*root) { \
		int comp = (cmp)((elm), (*root)); \
		parent = *root; \
		if (comp < 0) \
			root = &LLRB_LEFT(*root, field); \
		else if (comp > 0) \
			root = &LLRB_RIGHT(*root, field); \
		else \
			return *root; \
	} \
	LLRB_LEFT((elm), field) = 0; \
	LLRB_RIGHT((elm), field) = 0; \
	LLRB_COLOR((elm), field) = LLRB_RED; \
	LLRB_PARENT((elm), field) = parent; \
	*root = (elm); \
	while (parent && (LLRB_ISRED(LLRB_LEFT(parent, field), field) || LLRB_ISRED(LLRB_RIGHT(parent, field), field))) { \
		root = LLRB_EDGE(head, parent, field); \
		parent = LLRB_PARENT(parent, field); \
		name##_LLRB_FIXUP(root); \
	} \
	LLRB_COLOR(LLRB_ROOT(head), field) = LLRB_BLACK; \
	return 0; \
} \
static inline void name##_LLRB_MOVL(struct type **pivot) { \
	name##_LLRB_FLIP(*pivot); \
	if (LLRB_ISRED(LLRB_LEFT(LLRB_RIGHT(*pivot, field), field), field)) { \
		name##_LLRB_ROTR(&LLRB_RIGHT(*pivot, field)); \
		name##_LLRB_ROTL(pivot); \
		name##_LLRB_FLIP(*pivot); \
	} \
} \
static inline void name##_LLRB_MOVR(struct type **pivot) { \
	name##_LLRB_FLIP(*pivot); \
	if (LLRB_ISRED(LLRB_LEFT(LLRB_LEFT(*pivot, field), field), field)) { \
		name##_LLRB_ROTR(pivot); \
		name##_LLRB_FLIP(*pivot); \
	} \
} \
static inline struct type *name##_DELETEMIN(struct name *head, struct type **root) { \
	struct type **pivot = root, *deleted, *parent; \
	while (LLRB_LEFT(*pivot, field)) { \
		if (!LLRB_ISRED(LLRB_LEFT(*pivot, field), field) && !LLRB_ISRED(LLRB_LEFT(LLRB_LEFT(*pivot, field), field), field)) \
			name##_LLRB_MOVL(pivot); \
		pivot = &LLRB_LEFT(*pivot, field); \
	} \
	deleted = *pivot; \
	parent = LLRB_PARENT(*pivot, field); \
	*pivot = 0; \
	while (root != pivot) { \
		pivot = LLRB_EDGE(head, parent, field); \
		parent = LLRB_PARENT(parent, field); \
		name##_LLRB_FIXUP(pivot); \
	} \
	return deleted; \
} \
attr struct type *name##_LLRB_DELETE(struct name *head, struct type *elm) { \
	struct type **root = &LLRB_ROOT(head), *parent = 0, *deleted = 0; \
	int comp; \
	while (*root) { \
		parent = LLRB_PARENT(*root, field); \
		comp = (cmp)(elm, *root); \
		if (comp < 0) { \
			if (LLRB_LEFT(*root, field) && !LLRB_ISRED(LLRB_LEFT(*root, field), field) && !LLRB_ISRED(LLRB_LEFT(LLRB_LEFT(*root, field), field), field)) \
				name##_LLRB_MOVL(root); \
			root = &LLRB_LEFT(*root, field); \
		} else { \
			if (LLRB_ISRED(LLRB_LEFT(*root, field), field)) { \
				name##_LLRB_ROTR(root); \
				comp = (cmp)(elm, *root); \
			} \
			if (!comp && !LLRB_RIGHT(*root, field)) { \
				deleted = *root; \
				*root = 0; \
				break; \
			} \
			if (LLRB_RIGHT(*root, field) && !LLRB_ISRED(LLRB_RIGHT(*root, field), field) && !LLRB_ISRED(LLRB_LEFT(LLRB_RIGHT(*root, field), field), field)) { \
				name##_LLRB_MOVR(root); \
				comp = (cmp)(elm, *root); \
			} \
			if (!comp) { \
				struct type *orphan = name##_DELETEMIN(head, &LLRB_RIGHT(*root, field)); \
				LLRB_COLOR(orphan, field) = LLRB_COLOR(*root, field); \
				LLRB_PARENT(orphan, field) = LLRB_PARENT(*root, field); \
				if ((LLRB_RIGHT(orphan, field) = LLRB_RIGHT(*root, field))) \
					LLRB_PARENT(LLRB_RIGHT(orphan, field), field) = orphan; \
				if ((LLRB_LEFT(orphan, field) = LLRB_LEFT(*root, field))) \
					LLRB_PARENT(LLRB_LEFT(orphan, field), field) = orphan; \
				deleted = *root; \
				*root = orphan; \
				parent = *root; \
				break; \
			} else \
				root = &LLRB_RIGHT(*root, field); \
		} \
	} \
	while (parent) { \
		root = LLRB_EDGE(head, parent, field); \
		parent = LLRB_PARENT(parent, field); \
		name##_LLRB_FIXUP(root); \
	} \
	if (LLRB_ROOT(head)) \
		LLRB_COLOR(LLRB_ROOT(head), field) = LLRB_BLACK; \
	return deleted; \
} \
attr struct type *name##_LLRB_FIND(struct name *head, struct type *key) { \
	struct type *elm = LLRB_ROOT(head); \
	while (elm) { \
		int comp = (cmp)(key, elm); \
		if (comp < 0) \
			elm = LLRB_LEFT(elm, field); \
		else if (comp > 0) \
			elm = LLRB_RIGHT(elm, field); \
		else \
			return elm; \
	} \
	return 0; \
} \
attr struct type *name##_LLRB_MIN(struct type *elm) { \
	while (elm && LLRB_LEFT(elm, field)) \
		elm = LLRB_LEFT(elm, field); \
	return elm; \
} \
attr struct type *name##_LLRB_MAX(struct type *elm) { \
	while (elm && LLRB_RIGHT(elm, field)) \
		elm = LLRB_RIGHT(elm, field); \
	return elm; \
} \
attr struct type *name##_LLRB_NEXT(struct type *elm) { \
	if (LLRB_RIGHT(elm, field)) { \
		return name##_LLRB_MIN(LLRB_RIGHT(elm, field)); \
	} else if (LLRB_PARENT(elm, field)) { \
		if (elm == LLRB_LEFT(LLRB_PARENT(elm, field), field)) \
			return LLRB_PARENT(elm, field); \
		while (LLRB_PARENT(elm, field) && elm == LLRB_RIGHT(LLRB_PARENT(elm, field), field)) \
			elm = LLRB_PARENT(elm, field); \
		return LLRB_PARENT(elm, field); \
	} else return 0; \
}

#define LLRB_INSERT(name, head, elm) name##_LLRB_INSERT((head), (elm))
#define LLRB_DELETE(name, head, elm) name##_LLRB_DELETE((head), (elm))
#define LLRB_REMOVE(name, head, elm) name##_LLRB_DELETE((head), (elm))
#define LLRB_FIND(name, head, elm) name##_LLRB_FIND((head), (elm))
#define LLRB_MIN(name, head) name##_LLRB_MIN(LLRB_ROOT((head)))
#define LLRB_MAX(name, head) name##_LLRB_MAX(LLRB_ROOT((head)))
#define LLRB_NEXT(name, head, elm) name##_LLRB_NEXT((elm))

#define LLRB_FOREACH(elm, name, head) \
for ((elm) = LLRB_MIN(name, head); (elm); (elm) = name##_LLRB_NEXT((elm)))

#endif /* LLRB_H */


#include <stdlib.h>

#include "timeout.h"
#include "bench.h"


struct rbtimeout {
	timeout_t expires;

	int pending;

	LLRB_ENTRY(rbtimeout) rbe;
};

struct rbtimeouts {
	timeout_t curtime;
	LLRB_HEAD(tree, rbtimeout) tree;
};


static int timeoutcmp(struct rbtimeout *a, struct rbtimeout *b) {
	if (a->expires < b->expires) {
		return -1;
	} else if (a->expires > b->expires) {
		return 1;
	} else if (a < b) {
		return -1;
	} else if (a > b) {
		return 1;
	} else {
		return 0;
	}
} /* timeoutcmp() */

LLRB_GENERATE_STATIC(tree, rbtimeout, rbe, timeoutcmp)

static void *init(struct timeout *timeout, size_t count, int verbose) {
	struct rbtimeouts *T;
	size_t i;

	T = malloc(sizeof *T);
	T->curtime = 0;
	LLRB_INIT(&T->tree);

	for (i = 0; i < count; i++) {
		struct rbtimeout *to = (void *)&timeout[i];
		to->expires = 0;
		to->pending = 0;
	}

	return T;
} /* init() */


static void add(void *ctx, struct timeout *_to, timeout_t expires) {
	struct rbtimeouts *T = ctx;
	struct rbtimeout *to = (void *)_to;

	if (to->pending)
		LLRB_REMOVE(tree, &T->tree, to);

	to->expires = T->curtime + expires;
	LLRB_INSERT(tree, &T->tree, to);
	to->pending = 1;
} /* add() */


static void del(void *ctx, struct timeout *_to) {
	struct rbtimeouts *T = ctx;
	struct rbtimeout *to = (void *)_to;

	LLRB_REMOVE(tree, &T->tree, to);
	to->pending = 0;
	to->expires = 0;
} /* del() */


static struct timeout *get(void *ctx) {
	struct rbtimeouts *T = ctx;
	struct rbtimeout *to;

	if ((to = LLRB_MIN(tree, &T->tree)) && to->expires <= T->curtime) {
		LLRB_REMOVE(tree, &T->tree, to);
		to->pending = 0;
		to->expires = 0;

		return (void *)to;
	}

	return NULL;
} /* get() */


static void update(void *ctx, timeout_t ts) {
	struct rbtimeouts *T = ctx;
	T->curtime = ts;
} /* update() */


static void check(void *ctx) {
	return;
} /* check() */


static int empty(void *ctx) {
	struct rbtimeouts *T = ctx;

	return LLRB_EMPTY(&T->tree);
} /* empty() */


static void destroy(void *ctx) {
	free(ctx);
	return;
} /* destroy() */


const struct benchops benchops = {
	.init    = &init,
	.add     = &add,
	.del     = &del,
	.get     = &get,
	.update  = &update,
	.check   = &check,
	.empty   = &empty,
	.destroy = &destroy,
};

