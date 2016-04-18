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

/* The following definitions come from lib/bitmap.c  */

struct bitmap {
	uint32_t *b;
	unsigned int n;
};

struct bitmap *bitmap_talloc(TALLOC_CTX *mem_ctx, int n);
int bitmap_copy(struct bitmap * const dst, const struct bitmap * const src);
bool bitmap_set(struct bitmap *bm, unsigned i);
bool bitmap_clear(struct bitmap *bm, unsigned i);
bool bitmap_query(struct bitmap *bm, unsigned i);
int bitmap_find(struct bitmap *bm, unsigned ofs);
