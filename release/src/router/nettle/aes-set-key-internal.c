/* aes-set-key-internal.c
 
   Key setup for the aes/rijndael block cipher.

   Copyright (C) 2000, 2001, 2002 Rafael R. Sevilla, Niels Möller
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

/* Originally written by Rafael R. Sevilla <dido@pacific.net.ph> */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "aes-internal.h"
#include "macros.h"

void
_aes_set_key(unsigned nr, unsigned nk,
	     uint32_t *subkeys, const uint8_t *key)
{
  static const uint8_t rcon[10] = {
    0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36,
  };
  const uint8_t *rp;
  unsigned lastkey, i;
  uint32_t t;

  lastkey = (AES_BLOCK_SIZE/4) * (nr + 1);
  
  for (i=0, rp = rcon; i<nk; i++)
    subkeys[i] = LE_READ_UINT32(key + i*4);

  for (i=nk; i<lastkey; i++)
    {
      t = subkeys[i-1];
      if (i % nk == 0)
	t = SUBBYTE(ROTL32(24, t), aes_sbox) ^ *rp++;

      else if (nk > 6 && (i%nk) == 4)
	t = SUBBYTE(t, aes_sbox);

      subkeys[i] = subkeys[i-nk] ^ t;
    }  
}
