/* Vector API for GDB.
   Copyright (C) 2004, 2005, 2006, 2007 Free Software Foundation, Inc.
   Contributed by Nathan Sidwell <nathan@codesourcery.com>

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "vec.h"

struct vec_prefix
{
  unsigned num;
  unsigned alloc;
  void *vec[1];
};

/* Calculate the new ALLOC value, making sure that abs(RESERVE) slots
   are free.  If RESERVE < 0 grow exactly, otherwise grow
   exponentially.  */

static inline unsigned
calculate_allocation (const struct vec_prefix *pfx, int reserve)
{
  unsigned alloc = 0;
  unsigned num = 0;

  if (pfx)
    {
      alloc = pfx->alloc;
      num = pfx->num;
    }
  else if (!reserve)
    /* If there's no prefix, and we've not requested anything, then we
       will create a NULL vector.  */
    return 0;

  /* We must have run out of room.  */
  gdb_assert (alloc - num < (unsigned)(reserve < 0 ? -reserve : reserve));

  if (reserve < 0)
    /* Exact size.  */
    alloc = num + -reserve;
  else
    {
      /* Exponential growth. */
      if (!alloc)
	alloc = 4;
      else if (alloc < 16)
	/* Double when small.  */
	alloc = alloc * 2;
      else
	/* Grow slower when large.  */
	alloc = (alloc * 3 / 2);

      /* If this is still too small, set it to the right size. */
      if (alloc < num + reserve)
	alloc = num + reserve;
    }
  return alloc;
}

/* Ensure there are at least abs(RESERVE) free slots in VEC.  If
   RESERVE < 0 grow exactly, else grow exponentially.  As a special
   case, if VEC is NULL, and RESERVE is 0, no vector will be created. */

void *
vec_p_reserve (void *vec, int reserve)
{
  return vec_o_reserve (vec, reserve,
			offsetof (struct vec_prefix, vec), sizeof (void *));
}

/* As vec_p_reserve, but for object vectors.  The vector's trailing
   array is at VEC_OFFSET offset and consists of ELT_SIZE sized
   elements.  */

void *
vec_o_reserve (void *vec, int reserve, size_t vec_offset, size_t elt_size)
{
  struct vec_prefix *pfx = vec;
  unsigned alloc = calculate_allocation (pfx, reserve);

  if (!alloc)
    return NULL;

  vec = xrealloc (vec, vec_offset + alloc * elt_size);
  ((struct vec_prefix *)vec)->alloc = alloc;
  if (!pfx)
    ((struct vec_prefix *)vec)->num = 0;

  return vec;
}

#if 0
/* Example uses.  */
DEF_VEC_I (int);
typedef struct X
{
  int i;
} obj_t;
typedef obj_t *ptr_t;

DEF_VEC_P (ptr_t);
DEF_VEC_O (obj_t);
#endif
