/* base64-meta.c

   Copyright (C) 2002 Dan Egnor, Niels Möller

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

#include "base64.h"

/* Same as the macros with the same name */
static nettle_armor_length_func base64_encode_length;
static size_t
base64_encode_length(size_t length)
{
  return BASE64_ENCODE_LENGTH(length);
}

static nettle_armor_length_func base64_decode_length;
static size_t
base64_decode_length(size_t length)
{
  return BASE64_DECODE_LENGTH(length);
}

const struct nettle_armor nettle_base64
= _NETTLE_ARMOR(base64, BASE64);
