/* ctr.c

   Cipher counter mode.

   Copyright (C) 2005 Niels Möller

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
#include <stdlib.h>
#include <string.h>

#include "ctr.h"

#include "macros.h"
#include "memxor.h"
#include "nettle-internal.h"

#define NBLOCKS 4

void
ctr_crypt(const void *ctx, nettle_cipher_func *f,
	  size_t block_size, uint8_t *ctr,
	  size_t length, uint8_t *dst,
	  const uint8_t *src)
{
  if (src != dst)
    {
      if (length == block_size)
	{
	  f(ctx, block_size, dst, ctr);
	  INCREMENT(block_size, ctr);
	  memxor(dst, src, block_size);
	}
      else
	{
	  size_t left;
	  uint8_t *p;	  

	  for (p = dst, left = length;
	       left >= block_size;
	       left -= block_size, p += block_size)
	    {
	      memcpy (p, ctr, block_size);
	      INCREMENT(block_size, ctr);
	    }

	  f(ctx, length - left, dst, dst);
	  memxor(dst, src, length - left);

	  if (left)
	    {
	      TMP_DECL(buffer, uint8_t, NETTLE_MAX_CIPHER_BLOCK_SIZE);
	      TMP_ALLOC(buffer, block_size);

	      f(ctx, block_size, buffer, ctr);
	      INCREMENT(block_size, ctr);
	      memxor3(dst + length - left, src + length - left, buffer, left);
	    }
	}
    }
  else
    {
      if (length > block_size)
	{
	  TMP_DECL(buffer, uint8_t, NBLOCKS * NETTLE_MAX_CIPHER_BLOCK_SIZE);
	  size_t chunk = NBLOCKS * block_size;

	  TMP_ALLOC(buffer, chunk);

	  for (; length >= chunk;
	       length -= chunk, src += chunk, dst += chunk)
	    {
	      unsigned n;
	      uint8_t *p;	  
	      for (n = 0, p = buffer; n < NBLOCKS; n++, p += block_size)
		{
		  memcpy (p, ctr, block_size);
		  INCREMENT(block_size, ctr);
		}
	      f(ctx, chunk, buffer, buffer);
	      memxor(dst, buffer, chunk);
	    }

	  if (length > 0)
	    {
	      /* Final, possibly partial, blocks */
	      for (chunk = 0; chunk < length; chunk += block_size)
		{
		  memcpy (buffer + chunk, ctr, block_size);
		  INCREMENT(block_size, ctr);
		}
	      f(ctx, chunk, buffer, buffer);
	      memxor3(dst, src, buffer, length);
	    }
	}
      else if (length > 0)
      	{
	  TMP_DECL(buffer, uint8_t, NETTLE_MAX_CIPHER_BLOCK_SIZE);
	  TMP_ALLOC(buffer, block_size);

	  f(ctx, block_size, buffer, ctr);
	  INCREMENT(block_size, ctr);
	  memxor3(dst, src, buffer, length);
	}
    }
}
