/* arcfour-crypt.c

   The arcfour/rc4 stream cipher.

   Copyright (C) 2001, 2004 Niels Möller

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

#include <assert.h>

#include "arcfour.h"

void
arcfour_crypt(struct arcfour_ctx *ctx,
	      size_t length, uint8_t *dst,
	      const uint8_t *src)
{
  register uint8_t i, j;
  register int si, sj;

  i = ctx->i; j = ctx->j;
  while(length--)
    {
      i++; i &= 0xff;
      si = ctx->S[i];
      j += si; j &= 0xff;
      sj = ctx->S[i] = ctx->S[j];
      ctx->S[j] = si;
      *dst++ = *src++ ^ ctx->S[ (si + sj) & 0xff ];
    }
  ctx->i = i; ctx->j = j;
}
