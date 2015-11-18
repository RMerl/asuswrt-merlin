/* serpent-meta.c

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

#include "serpent.h"

#define SERPENT(bits) {					\
  "serpent" #bits,					\
  sizeof(struct serpent_ctx),				\
  SERPENT_BLOCK_SIZE,					\
  SERPENT ## bits ##_KEY_SIZE,				\
  (nettle_set_key_func *) serpent ## bits ## _set_key,	\
  (nettle_set_key_func *) serpent ## bits ## _set_key,	\
  (nettle_cipher_func *) serpent_encrypt,		\
  (nettle_cipher_func *) serpent_decrypt			\
}

const struct nettle_cipher nettle_serpent128
= SERPENT(128);
const struct nettle_cipher nettle_serpent192
= SERPENT(192);
const struct nettle_cipher nettle_serpent256
= SERPENT(256);
