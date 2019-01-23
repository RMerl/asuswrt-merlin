/* Stable-sorting of an array using mergesort.
   Copyright (C) 2009-2017 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2009.

   This program is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* This file implements stable sorting of an array, using the mergesort
   algorithm.
   Worst-case running time for an array of length N is O(N log N).
   Unlike the mpsort module, the algorithm here attempts to minimize not
   only the number of comparisons, but also the number of copying operations.

   Before including this file, you need to define
     ELEMENT      The type of every array element.
     COMPARE      A two-argument macro that takes two 'const ELEMENT *'
                  pointers and returns a negative, zero, or positive 'int'
                  value if the element pointed to by the first argument is,
                  respectively, less, equal, or greater than the element
                  pointed to by the second argument.
     STATIC       The storage class of the functions being defined.
   Before including this file, you also need to include:
     #include <stddef.h>
 */

/* Merge the sorted arrays src1[0..n1-1] and src2[0..n2-1] into
   dst[0..n1+n2-1].  In case of ambiguity, put the elements of src1
   before the elements of src2.
   n1 and n2 must be > 0.
   The arrays src1 and src2 must not overlap the dst array, except that
   src1 may be dst[n2..n1+n2-1], or src2 may be dst[n1..n1+n2-1].  */
static void
merge (const ELEMENT *src1, size_t n1,
       const ELEMENT *src2, size_t n2,
       ELEMENT *dst)
{
  for (;;) /* while (n1 > 0 && n2 > 0) */
    {
      if (COMPARE (src1, src2) <= 0)
        {
          *dst++ = *src1++;
          n1--;
          if (n1 == 0)
            break;
        }
      else
        {
          *dst++ = *src2++;
          n2--;
          if (n2 == 0)
            break;
        }
    }
  /* Here n1 == 0 || n2 == 0 but also n1 > 0 || n2 > 0.  */
  if (n1 > 0)
    {
      if (dst != src1)
        do
          {
            *dst++ = *src1++;
            n1--;
          }
        while (n1 > 0);
    }
  else /* n2 > 0 */
    {
      if (dst != src2)
        do
          {
            *dst++ = *src2++;
            n2--;
          }
        while (n2 > 0);
    }
}

/* Sort src[0..n-1] into dst[0..n-1], using tmp[0..n/2-1] as temporary
   (scratch) storage.
   The arrays src, dst, tmp must not overlap.  */
STATIC void
merge_sort_fromto (const ELEMENT *src, ELEMENT *dst, size_t n, ELEMENT *tmp)
{
  switch (n)
    {
    case 0:
      return;
    case 1:
      /* Nothing to do.  */
      dst[0] = src[0];
      return;
    case 2:
      /* Trivial case.  */
      if (COMPARE (&src[0], &src[1]) <= 0)
        {
          /* src[0] <= src[1] */
          dst[0] = src[0];
          dst[1] = src[1];
        }
      else
        {
          dst[0] = src[1];
          dst[1] = src[0];
        }
      break;
    case 3:
      /* Simple case.  */
      if (COMPARE (&src[0], &src[1]) <= 0)
        {
          if (COMPARE (&src[1], &src[2]) <= 0)
            {
              /* src[0] <= src[1] <= src[2] */
              dst[0] = src[0];
              dst[1] = src[1];
              dst[2] = src[2];
            }
          else if (COMPARE (&src[0], &src[2]) <= 0)
            {
              /* src[0] <= src[2] < src[1] */
              dst[0] = src[0];
              dst[1] = src[2];
              dst[2] = src[1];
            }
          else
            {
              /* src[2] < src[0] <= src[1] */
              dst[0] = src[2];
              dst[1] = src[0];
              dst[2] = src[1];
            }
        }
      else
        {
          if (COMPARE (&src[0], &src[2]) <= 0)
            {
              /* src[1] < src[0] <= src[2] */
              dst[0] = src[1];
              dst[1] = src[0];
              dst[2] = src[2];
            }
          else if (COMPARE (&src[1], &src[2]) <= 0)
            {
              /* src[1] <= src[2] < src[0] */
              dst[0] = src[1];
              dst[1] = src[2];
              dst[2] = src[0];
            }
          else
            {
              /* src[2] < src[1] < src[0] */
              dst[0] = src[2];
              dst[1] = src[1];
              dst[2] = src[0];
            }
        }
      break;
    default:
      {
        size_t n1 = n / 2;
        size_t n2 = (n + 1) / 2;
        /* Note: n1 + n2 = n, n1 <= n2.  */
        /* Sort src[n1..n-1] into dst[n1..n-1], scratching tmp[0..n2/2-1].  */
        merge_sort_fromto (src + n1, dst + n1, n2, tmp);
        /* Sort src[0..n1-1] into tmp[0..n1-1], scratching dst[0..n1-1].  */
        merge_sort_fromto (src, tmp, n1, dst);
        /* Merge the two half results.  */
        merge (tmp, n1, dst + n1, n2, dst);
      }
      break;
    }
}

/* Sort src[0..n-1], using tmp[0..n-1] as temporary (scratch) storage.
   The arrays src, tmp must not overlap. */
STATIC void
merge_sort_inplace (ELEMENT *src, size_t n, ELEMENT *tmp)
{
  switch (n)
    {
    case 0:
    case 1:
      /* Nothing to do.  */
      return;
    case 2:
      /* Trivial case.  */
      if (COMPARE (&src[0], &src[1]) <= 0)
        {
          /* src[0] <= src[1] */
        }
      else
        {
          ELEMENT t = src[0];
          src[0] = src[1];
          src[1] = t;
        }
      break;
    case 3:
      /* Simple case.  */
      if (COMPARE (&src[0], &src[1]) <= 0)
        {
          if (COMPARE (&src[1], &src[2]) <= 0)
            {
              /* src[0] <= src[1] <= src[2] */
            }
          else if (COMPARE (&src[0], &src[2]) <= 0)
            {
              /* src[0] <= src[2] < src[1] */
              ELEMENT t = src[1];
              src[1] = src[2];
              src[2] = t;
            }
          else
            {
              /* src[2] < src[0] <= src[1] */
              ELEMENT t = src[0];
              src[0] = src[2];
              src[2] = src[1];
              src[1] = t;
            }
        }
      else
        {
          if (COMPARE (&src[0], &src[2]) <= 0)
            {
              /* src[1] < src[0] <= src[2] */
              ELEMENT t = src[0];
              src[0] = src[1];
              src[1] = t;
            }
          else if (COMPARE (&src[1], &src[2]) <= 0)
            {
              /* src[1] <= src[2] < src[0] */
              ELEMENT t = src[0];
              src[0] = src[1];
              src[1] = src[2];
              src[2] = t;
            }
          else
            {
              /* src[2] < src[1] < src[0] */
              ELEMENT t = src[0];
              src[0] = src[2];
              src[2] = t;
            }
        }
      break;
    default:
      {
        size_t n1 = n / 2;
        size_t n2 = (n + 1) / 2;
        /* Note: n1 + n2 = n, n1 <= n2.  */
        /* Sort src[n1..n-1], scratching tmp[0..n2-1].  */
        merge_sort_inplace (src + n1, n2, tmp);
        /* Sort src[0..n1-1] into tmp[0..n1-1], scratching tmp[n1..2*n1-1].  */
        merge_sort_fromto (src, tmp, n1, tmp + n1);
        /* Merge the two half results.  */
        merge (tmp, n1, src + n1, n2, src);
      }
      break;
    }
}

#undef ELEMENT
#undef COMPARE
#undef STATIC
