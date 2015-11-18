/* sha512-224-meta.c

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

#include "nettle-meta.h"

#include "sha2.h"

const struct nettle_hash nettle_sha512_224 =
  {
    "sha512-224", sizeof(struct sha512_ctx),
    SHA512_224_DIGEST_SIZE,
    SHA512_224_BLOCK_SIZE,
    (nettle_hash_init_func *) sha512_224_init,
    (nettle_hash_update_func *) sha512_224_update,
    (nettle_hash_digest_func *) sha512_224_digest
  };

