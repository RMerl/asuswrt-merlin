/* des3.c

   Triple DES cipher. Three key encrypt-decrypt-encrypt.

   Copyright (C) 2001, 2010 Niels Möller

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

#include "des.h"

/* It's possible to make some more general pipe construction, like the
 * lsh/src/cascade.c, but as in practice it's never used for anything
 * like triple DES, it's not worth the effort. */

/* Returns 1 for good keys and 0 for weak keys. */
int
des3_set_key(struct des3_ctx *ctx, const uint8_t *key)
{
  unsigned i;
  int is_good = 1;
  
  for (i = 0; i<3; i++, key += DES_KEY_SIZE)
    if (!des_set_key(&ctx->des[i], key))
      is_good = 0;

  return is_good;
}

void
des3_encrypt(const struct des3_ctx *ctx,
	     size_t length, uint8_t *dst,
	     const uint8_t *src)
{
  des_encrypt(&ctx->des[0],
	      length, dst, src);
  des_decrypt(&ctx->des[1],
	      length, dst, dst);
  des_encrypt(&ctx->des[2],
	      length, dst, dst);
}

void
des3_decrypt(const struct des3_ctx *ctx,
	     size_t length, uint8_t *dst,
	     const uint8_t *src)
{
  des_decrypt(&ctx->des[2],
	      length, dst, src);
  des_encrypt(&ctx->des[1],
	      length, dst, dst);
  des_decrypt(&ctx->des[0],
	      length, dst, dst);
} 
