/* twofish-meta.c

   Copyright (C) 2002, 2014 Niels Möller

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

#include "twofish.h"

#define TWOFISH(bits) {					\
  "twofish" #bits,					\
  sizeof(struct twofish_ctx),				\
  TWOFISH_BLOCK_SIZE,					\
  TWOFISH ## bits ## _KEY_SIZE,				\
  (nettle_set_key_func *) twofish ## bits ## _set_key,	\
  (nettle_set_key_func *) twofish ## bits ## _set_key,	\
  (nettle_cipher_func *) twofish_encrypt,		\
  (nettle_cipher_func *) twofish_decrypt			\
}

const struct nettle_cipher nettle_twofish128
= TWOFISH(128);
const struct nettle_cipher nettle_twofish192
= TWOFISH(192);
const struct nettle_cipher nettle_twofish256
= TWOFISH(256);
