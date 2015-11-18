/* base16-meta.c

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

#include "nettle-meta.h"

#include "base16.h"

/* Same as the macros with the same name */
static nettle_armor_length_func base16_encode_length;
static size_t
base16_encode_length(size_t length)
{
  return BASE16_ENCODE_LENGTH(length);
}

static nettle_armor_length_func base16_decode_length;
static size_t
base16_decode_length(size_t length)
{
  return BASE16_DECODE_LENGTH(length);
}

static nettle_armor_init_func base16_encode_init;
static void
base16_encode_init(void *ctx UNUSED)
{ }

static nettle_armor_encode_update_func base16_encode_update_wrapper;
static size_t
base16_encode_update_wrapper(void *ctx UNUSED, uint8_t *dst,
			     size_t length, const uint8_t *src)
{
  base16_encode_update(dst, length, src);
  return BASE16_ENCODE_LENGTH(length);
}

#undef base16_encode_update
#define base16_encode_update base16_encode_update_wrapper

static nettle_armor_encode_final_func base16_encode_final;
static size_t
base16_encode_final(void *ctx UNUSED, uint8_t *dst UNUSED)
{
  return 0;
}


#define BASE16_ENCODE_FINAL_LENGTH 0

const struct nettle_armor nettle_base16
= _NETTLE_ARMOR_0(base16, BASE16);
