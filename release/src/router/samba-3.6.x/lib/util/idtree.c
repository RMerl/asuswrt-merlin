/* 
   Unix SMB/CIFS implementation.

   very efficient functions to manage mapping a id (such as a fnum) to
   a pointer. This is used for fnum and search id allocation.

   Copyright (C) Andrew Tridgell 2004

   This code is derived from lib/idr.c in the 2.6 Linux kernel, which was 
   written by Jim Houston jim.houston@ccur.com, and is
   Copyright (C) 2002 by Concurrent Computer Corporation
    
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
  see the section marked "public interface" below for documentation
*/

/**
 * @file
 */

#include "includes.h"

#define IDR_BITS 5
#define IDR_FULL 0xfffffffful
#if 0 /* unused */
#define TOP_LEVEL_FULL (IDR_FULL >> 30)
#endif
#define IDR_SIZE (1 << IDR_BITS)
#define IDR_MASK ((1 << IDR_BITS)-1)
#define MAX_ID_SHIFT (sizeof(int)*8 - 1)
#define MAX_ID_BIT (1U << MAX_ID_SHIFT)
#define MAX_ID_MASK (MAX_ID_BIT - 1)
#define MAX_LEVEL (MAX_ID_SHIFT + IDR_BITS - 1) / IDR_BITS
#define IDR_FREE_MAX MAX_LEVEL + MAX_LEVEL

#define set_bit(bit, v) (v) |= (1<<(bit))
#define clear_bit(bit, v) (v) &= ~(1<<(bit))
#define test_bit(bit, v) ((v) & (1<<(bit)))
				   
struct idr_layer {
	uint32_t		 bitmap;
	struct idr_layer	*ary[IDR_SIZE];
	int			 count;
};

struct idr_context {
	struct idr_layer *top;
	struct idr_layer *id_free;
	int		  layers;
	int		  id_free_cnt;
};

static struct idr_layer *alloc_layer(struct idr_context *idp)
{
	struct idr_layer *p;

	if (!(p = idp->id_free))
		return NULL;
	idp->id_free = p->ary[0];
	idp->id_free_cnt--;
	p->ary[0] = NULL;
	return p;
}

static int find_next_bit(uint32_t bm, int maxid, int n)
{
	while (n<maxid && !test_bit(n, bm)) n++;
	return n;
}

static void free_layer(struct idr_context *idp, struct idr_layer *p)
{
	p->ary[0] = idp->id_free;
	idp->id_free = p;
	idp->id_free_cnt++;
}

static int idr_pre_get(struct idr_context *idp)
{
	while (idp->id_free_cnt < IDR_FREE_MAX) {
		struct idr_layer *pn = talloc_zero(idp, struct idr_layer);
		if(pn == NULL)
			return (0);
		free_layer(idp, pn);
	}
	return 1;
}

static int sub_alloc(struct idr_context *idp, void *ptr, int *starting_id)
{
	int n, m, sh;
	struct idr_layer *p, *pn;
	struct idr_layer *pa[MAX_LEVEL+1];
	unsigned int l, id, oid;
	uint32_t bm;

	memset(pa, 0, sizeof(pa));

	id = *starting_id;
restart:
	p = idp->top;
	l = idp->layers;
	pa[l--] = NULL;
	while (1) {
		/*
		 * We run around this while until we reach the leaf node...
		 */
		n = (id >> (IDR_BITS*l)) & IDR_MASK;
		bm = ~p->bitmap;
		m = find_next_bit(bm, IDR_SIZE, n);
		if (m == IDR_SIZE) {
			/* no space available go back to previous layer. */
			l++;
			oid = id;
			id = (id | ((1 << (IDR_BITS*l))-1)) + 1;

			/* if already at the top layer, we need to grow */
			if (!(p = pa[l])) {
				*starting_id = id;
				return -2;
			}

			/* If we need to go up one layer, continue the
			 * loop; otherwise, restart from the top.
			 */
			sh = IDR_BITS * (l + 1);
			if (oid >> sh == id >> sh)
			continue;
			else
				goto restart;
		}
		if (m != n) {
			sh = IDR_BITS*l;
			id = ((id >> sh) ^ n ^ m) << sh;
		}
		if ((id >= MAX_ID_BIT) || (id < 0))
			return -1;
		if (l == 0)
			break;
		/*
		 * Create the layer below if it is missing.
		 */
		if (!p->ary[m]) {
			if (!(pn = alloc_layer(idp)))
				return -1;
			p->ary[m] = pn;
			p->count++;
		}
		pa[l--] = p;
		p = p->ary[m];
	}
	/*
	 * We have reached the leaf node, plant the
	 * users pointer and return the raw id.
	 */
	p->ary[m] = (struct idr_layer *)ptr;
	set_bit(m, p->bitmap);
	p->count++;
	/*
	 * If this layer is full mark the bit in the layer above
	 * to show that this part of the radix tree is full.
	 * This may complete the layer above and require walking
	 * up the radix tree.
	 */
	n = id;
	while (p->bitmap == IDR_FULL) {
		if (!(p = pa[++l]))
			break;
		n = n >> IDR_BITS;
		set_bit((n & IDR_MASK), p->bitmap);
	}
	return(id);
}

static int idr_get_new_above_int(struct idr_context *idp, void *ptr, int starting_id)
{
	struct idr_layer *p, *pn;
	int layers, v, id;

	idr_pre_get(idp);
	
	id = starting_id;
build_up:
	p = idp->top;
	layers = idp->layers;
	if (!p) {
		if (!(p = alloc_layer(idp)))
			return -1;
		layers = 1;
	}
	/*
	 * Add a new layer to the top of the tree if the requested
	 * id is larger than the currently allocated space.
	 */
	while ((layers < MAX_LEVEL) && (id >= (1 << (layers*IDR_BITS)))) {
		layers++;
		if (!p->count)
			continue;
		if (!(pn = alloc_layer(idp))) {
			/*
			 * The allocation failed.  If we built part of
			 * the structure tear it down.
			 */
			for (pn = p; p && p != idp->top; pn = p) {
				p = p->ary[0];
				pn->ary[0] = NULL;
				pn->bitmap = pn->count = 0;
				free_layer(idp, pn);
			}
			return -1;
		}
		pn->ary[0] = p;
		pn->count = 1;
		if (p->bitmap == IDR_FULL)
			set_bit(0, pn->bitmap);
		p = pn;
	}
	idp->top = p;
	idp->layers = layers;
	v = sub_alloc(idp, ptr, &id);
	if (v == -2)
		goto build_up;
	return(v);
}

static int sub_remove(struct idr_context *idp, int shift, int id)
{
	struct idr_layer *p = idp->top;
	struct idr_layer **pa[1+MAX_LEVEL];
	struct idr_layer ***paa = &pa[0];
	int n;

	*paa = NULL;
	*++paa = &idp->top;

	while ((shift > 0) && p) {
		n = (id >> shift) & IDR_MASK;
		clear_bit(n, p->bitmap);
		*++paa = &p->ary[n];
		p = p->ary[n];
		shift -= IDR_BITS;
	}
	n = id & IDR_MASK;
	if (p != NULL && test_bit(n, p->bitmap)) {
		clear_bit(n, p->bitmap);
		p->ary[n] = NULL;
		while(*paa && ! --((**paa)->count)){
			free_layer(idp, **paa);
			**paa-- = NULL;
		}
		if ( ! *paa )
			idp->layers = 0;
		return 0;
	}
	return -1;
}

static void *_idr_find(struct idr_context *idp, int id)
{
	int n;
	struct idr_layer *p;

	n = idp->layers * IDR_BITS;
	p = idp->top;
	/*
	 * This tests to see if bits outside the current tree are
	 * present.  If so, tain't one of ours!
	 */
	if (n + IDR_BITS < 31 &&
	    ((id & ~(~0 << MAX_ID_SHIFT)) >> (n + IDR_BITS))) {
		return NULL;
	}

	/* Mask off upper bits we don't use for the search. */
	id &= MAX_ID_MASK;

	while (n >= IDR_BITS && p) {
		n -= IDR_BITS;
		p = p->ary[(id >> n) & IDR_MASK];
	}
	return((void *)p);
}

static int _idr_remove(struct idr_context *idp, int id)
{
	struct idr_layer *p;

	/* Mask off upper bits we don't use for the search. */
	id &= MAX_ID_MASK;

	if (sub_remove(idp, (idp->layers - 1) * IDR_BITS, id) == -1) {
		return -1;
	}

	if ( idp->top && idp->top->count == 1 && 
	     (idp->layers > 1) &&
	     idp->top->ary[0]) {
		/* We can drop a layer */
		p = idp->top->ary[0];
		idp->top->bitmap = idp->top->count = 0;
		free_layer(idp, idp->top);
		idp->top = p;
		--idp->layers;
	}
	while (idp->id_free_cnt >= IDR_FREE_MAX) {
		p = alloc_layer(idp);
		talloc_free(p);
	}
	return 0;
}

/************************************************************************
  this is the public interface
**************************************************************************/

/**
  initialise a idr tree. The context return value must be passed to
  all subsequent idr calls. To destroy the idr tree use talloc_free()
  on this context
 */
_PUBLIC_ struct idr_context *idr_init(TALLOC_CTX *mem_ctx)
{
	return talloc_zero(mem_ctx, struct idr_context);
}

/**
  allocate the next available id, and assign 'ptr' into its slot.
  you can retrieve later this pointer using idr_find()
*/
_PUBLIC_ int idr_get_new(struct idr_context *idp, void *ptr, int limit)
{
	int ret = idr_get_new_above_int(idp, ptr, 0);
	if (ret > limit) {
		idr_remove(idp, ret);
		return -1;
	}
	return ret;
}

/**
   allocate a new id, giving the first available value greater than or
   equal to the given starting id
*/
_PUBLIC_ int idr_get_new_above(struct idr_context *idp, void *ptr, int starting_id, int limit)
{
	int ret = idr_get_new_above_int(idp, ptr, starting_id);
	if (ret > limit) {
		idr_remove(idp, ret);
		return -1;
	}
	return ret;
}

/**
  allocate a new id randomly in the given range
*/
_PUBLIC_ int idr_get_new_random(struct idr_context *idp, void *ptr, int limit)
{
	int id;

	/* first try a random starting point in the whole range, and if that fails,
	   then start randomly in the bottom half of the range. This can only
	   fail if the range is over half full, and finally fallback to any
	   free id */
	id = idr_get_new_above(idp, ptr, 1+(generate_random() % limit), limit);
	if (id == -1) {
		id = idr_get_new_above(idp, ptr, 1+(generate_random()%(limit/2)), limit);
	}
	if (id == -1) {
		id = idr_get_new_above(idp, ptr, 1, limit);
	}

	return id;
}

/**
  find a pointer value previously set with idr_get_new given an id
*/
_PUBLIC_ void *idr_find(struct idr_context *idp, int id)
{
	return _idr_find(idp, id);
}

/**
  remove an id from the idr tree
*/
_PUBLIC_ int idr_remove(struct idr_context *idp, int id)
{
	int ret;
	ret = _idr_remove((struct idr_context *)idp, id);
	if (ret != 0) {
		DEBUG(0,("WARNING: attempt to remove unset id %d in idtree\n", id));
	}
	return ret;
}
