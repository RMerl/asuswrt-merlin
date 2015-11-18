/* umac-nh.c

   Copyright (C) 2013 Niels Möller

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

#include "umac.h"
#include "macros.h"

/* For fat builds */
#if HAVE_NATIVE_umac_nh
uint64_t
_nettle_umac_nh_c (const uint32_t *key, unsigned length, const uint8_t *msg);
#define _nettle_umac_nh _nettle_umac_nh_c
#endif

uint64_t
_umac_nh (const uint32_t *key, unsigned length, const uint8_t *msg)
{
  uint64_t y;

  assert (length > 0);
  assert (length <= 1024);
  assert (length % 32 == 0);
  for (y = 0; length > 0; length -= 32, msg += 32, key += 8)
    {
      uint32_t a, b;
      a = LE_READ_UINT32 (msg)      + key[0];
      b = LE_READ_UINT32 (msg + 16) + key[4];
      y += (uint64_t) a * b;
      a = LE_READ_UINT32 (msg +  4) + key[1];
      b = LE_READ_UINT32 (msg + 20) + key[5];
      y += (uint64_t) a * b;
      a = LE_READ_UINT32 (msg +  8) + key[2];
      b = LE_READ_UINT32 (msg + 24) + key[6];
      y += (uint64_t) a * b;
      a = LE_READ_UINT32 (msg + 12) + key[3];
      b = LE_READ_UINT32 (msg + 28) + key[7];
      y += (uint64_t) a * b;
    }

  return y;
}
