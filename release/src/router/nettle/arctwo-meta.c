/* arctwo-meta.c

   Copyright (C) 2004 Simon Josefsson
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

#include "arctwo.h"

#define ARCTWO(bits) {					\
  "arctwo" #bits, sizeof (struct arctwo_ctx),		\
  ARCTWO_BLOCK_SIZE, bits/8,				\
  (nettle_set_key_func *) arctwo ## bits ## _set_key,	\
  (nettle_set_key_func *) arctwo ## bits ## _set_key,	\
  (nettle_cipher_func *) arctwo_encrypt,			\
  (nettle_cipher_func *) arctwo_decrypt,			\
}
const struct nettle_cipher nettle_arctwo40
= ARCTWO(40);
const struct nettle_cipher nettle_arctwo64
= ARCTWO(64);
const struct nettle_cipher nettle_arctwo128
= ARCTWO(128);

/* Gutmann variant. */
const struct nettle_cipher nettle_arctwo_gutmann128 =
  {
    "arctwo_gutmann128", sizeof (struct arctwo_ctx),
    ARCTWO_BLOCK_SIZE, 16,
    (nettle_set_key_func *) arctwo128_set_key_gutmann,
    (nettle_set_key_func *) arctwo128_set_key_gutmann,
    (nettle_cipher_func *) arctwo_encrypt,
    (nettle_cipher_func *) arctwo_decrypt,
  };
