/* chacha-set-key.c

   Copyright (C) 2014 Niels Möller

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
#include <string.h>

#include "chacha.h"

#include "macros.h"

void
chacha_set_key(struct chacha_ctx *ctx, const uint8_t *key)
{
  static const uint32_t sigma[4] = {
    /* "expand 32-byte k" */
    0x61707865, 0x3320646e, 0x79622d32, 0x6b206574
  };
  ctx->state[4] = LE_READ_UINT32(key + 0);
  ctx->state[5] = LE_READ_UINT32(key + 4);
  ctx->state[6] = LE_READ_UINT32(key + 8);
  ctx->state[7] = LE_READ_UINT32(key + 12);

  ctx->state[8]  = LE_READ_UINT32(key + 16);
  ctx->state[9]  = LE_READ_UINT32(key + 20);
  ctx->state[10] = LE_READ_UINT32(key + 24);
  ctx->state[11] = LE_READ_UINT32(key + 28);

  memcpy (ctx->state, sigma, sizeof(sigma));
}
