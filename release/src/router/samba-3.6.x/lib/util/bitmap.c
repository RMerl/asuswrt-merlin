/*
   Unix SMB/CIFS implementation.
   simple bitmap functions
   Copyright (C) Andrew Tridgell 1992-1998

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

#include "includes.h"
#include "lib/util/bitmap.h"

/* these functions provide a simple way to allocate integers from a
   pool without repetition */

/****************************************************************************
talloc a bitmap
****************************************************************************/
struct bitmap *bitmap_talloc(TALLOC_CTX *mem_ctx, int n)
{
	struct bitmap *bm;

	bm = talloc_zero(mem_ctx, struct bitmap);

	if (!bm) return NULL;

	bm->n = n;
	bm->b = talloc_zero_array(bm, uint32_t, (n+31)/32);
	if (!bm->b) {
		TALLOC_FREE(bm);
		return NULL;
	}
	return bm;
}

/****************************************************************************
copy as much of the source bitmap as will fit in the destination bitmap.
****************************************************************************/

int bitmap_copy(struct bitmap * const dst, const struct bitmap * const src)
{
        int count = MIN(dst->n, src->n);

        SMB_ASSERT(dst->b != src->b);
	memcpy(dst->b, src->b, sizeof(uint32_t)*((count+31)/32));

        return count;
}

/****************************************************************************
set a bit in a bitmap
****************************************************************************/
bool bitmap_set(struct bitmap *bm, unsigned i)
{
	if (i >= bm->n) {
		DEBUG(0,("Setting invalid bitmap entry %d (of %d)\n",
		      i, bm->n));
		return false;
	}
	bm->b[i/32] |= (1<<(i%32));
	return true;
}

/****************************************************************************
clear a bit in a bitmap
****************************************************************************/
bool bitmap_clear(struct bitmap *bm, unsigned i)
{
	if (i >= bm->n) {
		DEBUG(0,("clearing invalid bitmap entry %d (of %d)\n",
		      i, bm->n));
		return false;
	}
	bm->b[i/32] &= ~(1<<(i%32));
	return true;
}

/****************************************************************************
query a bit in a bitmap
****************************************************************************/
bool bitmap_query(struct bitmap *bm, unsigned i)
{
	if (i >= bm->n) return false;
	if (bm->b[i/32] & (1<<(i%32))) {
		return true;
	}
	return false;
}

/****************************************************************************
find a zero bit in a bitmap starting at the specified offset, with
wraparound
****************************************************************************/
int bitmap_find(struct bitmap *bm, unsigned ofs)
{
	unsigned int i, j;

	if (ofs > bm->n) ofs = 0;

	i = ofs;
	while (i < bm->n) {
		if (~(bm->b[i/32])) {
			j = i;
			do {
				if (!bitmap_query(bm, j)) return j;
				j++;
			} while (j & 31 && j < bm->n);
		}
		i += 32;
		i &= ~31;
	}

	i = 0;
	while (i < ofs) {
		if (~(bm->b[i/32])) {
			j = i;
			do {
				if (!bitmap_query(bm, j)) return j;
				j++;
			} while (j & 31 && j < bm->n);
		}
		i += 32;
		i &= ~31;
	}

	return -1;
}
