/* nettle-meta-ciphers.c

   Copyright (C) 2011 Daniel Kahn Gillmor

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

#include <stddef.h>
#include "nettle-meta.h"

const struct nettle_cipher * const nettle_ciphers[] = {
  &nettle_aes128,
  &nettle_aes192,
  &nettle_aes256,
  &nettle_camellia128,
  &nettle_camellia192,
  &nettle_camellia256,
  &nettle_cast128,
  &nettle_serpent128,
  &nettle_serpent192,
  &nettle_serpent256,
  &nettle_twofish128,
  &nettle_twofish192,
  &nettle_twofish256,
  &nettle_arctwo40,
  &nettle_arctwo64,
  &nettle_arctwo128,
  &nettle_arctwo_gutmann128,
  NULL
};
