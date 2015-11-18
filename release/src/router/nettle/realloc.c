/* realloc.c

   Copyright (C) 2002 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#include "realloc.h"

/* NOTE: Calling libc realloc with size == 0 is not required to
   totally free the object, it is allowed to return a valid
   pointer. */
void *
nettle_realloc(void *ctx UNUSED, void *p, size_t length)
{
  if (length > 0)
    return realloc(p, length);

  free(p);
  return NULL;
}

void *
nettle_xrealloc(void *ctx UNUSED, void *p, size_t length)
{
  if (length > 0)
    {
      void *n = realloc(p, length);
      if (!n)
	{
	  fprintf(stderr, "Virtual memory exhausted.\n");
	  abort();
	}
      return n;
    }
  free(p);
  return NULL;
}
