/* getdelim.c --- Implementation of replacement getdelim function.
   Copyright (C) 1994, 1996-1998, 2001, 2003, 2005-2017 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.  */

/* Ported from glibc by Simon Josefsson. */

/* Don't use __attribute__ __nonnull__ in this compilation unit.  Otherwise gcc
   optimizes away the lineptr == NULL || n == NULL || fp == NULL tests below.  */
#define _GL_ARG_NONNULL(params)

#include <config.h>

#include <stdio.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#ifndef SSIZE_MAX
# define SSIZE_MAX ((ssize_t) (SIZE_MAX / 2))
#endif

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
# define getc_maybe_unlocked(fp)        getc(fp)
#elif !HAVE_FLOCKFILE || !HAVE_FUNLOCKFILE || !HAVE_DECL_GETC_UNLOCKED
# undef flockfile
# undef funlockfile
# define flockfile(x) ((void) 0)
# define funlockfile(x) ((void) 0)
# define getc_maybe_unlocked(fp)        getc(fp)
#else
# define getc_maybe_unlocked(fp)        getc_unlocked(fp)
#endif

static void
alloc_failed (void)
{
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
  /* Avoid errno problem without using the realloc module; see:
     http://lists.gnu.org/archive/html/bug-gnulib/2016-08/msg00025.html  */
  errno = ENOMEM;
#endif
}

/* Read up to (and including) a DELIMITER from FP into *LINEPTR (and
   NUL-terminate it).  *LINEPTR is a pointer returned from malloc (or
   NULL), pointing to *N characters of space.  It is realloc'ed as
   necessary.  Returns the number of characters read (not including
   the null terminator), or -1 on error or EOF.  */

ssize_t
getdelim (char **lineptr, size_t *n, int delimiter, FILE *fp)
{
  ssize_t result;
  size_t cur_len = 0;

  if (lineptr == NULL || n == NULL || fp == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  flockfile (fp);

  if (*lineptr == NULL || *n == 0)
    {
      char *new_lineptr;
      *n = 120;
      new_lineptr = (char *) realloc (*lineptr, *n);
      if (new_lineptr == NULL)
        {
          alloc_failed ();
          result = -1;
          goto unlock_return;
        }
      *lineptr = new_lineptr;
    }

  for (;;)
    {
      int i;

      i = getc_maybe_unlocked (fp);
      if (i == EOF)
        {
          result = -1;
          break;
        }

      /* Make enough space for len+1 (for final NUL) bytes.  */
      if (cur_len + 1 >= *n)
        {
          size_t needed_max =
            SSIZE_MAX < SIZE_MAX ? (size_t) SSIZE_MAX + 1 : SIZE_MAX;
          size_t needed = 2 * *n + 1;   /* Be generous. */
          char *new_lineptr;

          if (needed_max < needed)
            needed = needed_max;
          if (cur_len + 1 >= needed)
            {
              result = -1;
              errno = EOVERFLOW;
              goto unlock_return;
            }

          new_lineptr = (char *) realloc (*lineptr, needed);
          if (new_lineptr == NULL)
            {
              alloc_failed ();
              result = -1;
              goto unlock_return;
            }

          *lineptr = new_lineptr;
          *n = needed;
        }

      (*lineptr)[cur_len] = i;
      cur_len++;

      if (i == delimiter)
        break;
    }
  (*lineptr)[cur_len] = '\0';
  result = cur_len ? cur_len : result;

 unlock_return:
  funlockfile (fp); /* doesn't set errno */

  return result;
}
