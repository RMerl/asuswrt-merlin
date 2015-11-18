/* umac-set-key.c

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

#include <string.h>

#include "umac.h"

#include "macros.h"

static void
umac_kdf (struct aes128_ctx *aes, unsigned index, unsigned length, uint8_t *dst)
{
  uint8_t block[AES_BLOCK_SIZE];
  uint64_t count;
  WRITE_UINT64 (block, (uint64_t) index);
  for (count = 1; length >= AES_BLOCK_SIZE;
       length -= AES_BLOCK_SIZE, dst += AES_BLOCK_SIZE, count++)
    {
      WRITE_UINT64 (block + 8, count);
      aes128_encrypt (aes, AES_BLOCK_SIZE, dst, block);
    }
  if (length > 0)
    {
      WRITE_UINT64 (block + 8, count);
      aes128_encrypt (aes, AES_BLOCK_SIZE, block, block);
      memcpy (dst, block, length);
    }
}

#if WORDS_BIGENDIAN
#define BE_SWAP32(x) x
#define BE_SWAP32_N(n, x)
#else
#define BE_SWAP32(x)				\
  ((ROTL32(8,  x) & 0x00FF00FFUL) |		\
   (ROTL32(24, x) & 0xFF00FF00UL))
#define BE_SWAP32_N(n, x) do {			\
  unsigned be_i;				\
  for (be_i = 0; be_i < n; be_i++)		\
    {						\
      uint32_t be_x = (x)[be_i];		\
      (x)[be_i] = BE_SWAP32 (be_x);		\
    }						\
  } while (0)
#endif

void
_umac_set_key (uint32_t *l1_key, uint32_t *l2_key,
	       uint64_t *l3_key1, uint32_t *l3_key2,
	       struct aes128_ctx *aes, const uint8_t *key, unsigned n)
{
  unsigned size;
  uint8_t buffer[UMAC_KEY_SIZE];

  aes128_set_encrypt_key (aes, key);

  size = UMAC_BLOCK_SIZE / 4 + 4*(n-1);
  umac_kdf (aes, 1, size * sizeof(uint32_t), (uint8_t *) l1_key);
  BE_SWAP32_N (size, l1_key);

  size = 6*n;
  umac_kdf (aes, 2, size * sizeof(uint32_t), (uint8_t *) l2_key);
  _umac_l2_init (size, l2_key);

  size = 8*n;
  umac_kdf (aes, 3, size * sizeof(uint64_t), (uint8_t *) l3_key1);
  _umac_l3_init (size, l3_key1);

  /* No need to byteswap these subkeys. */
  umac_kdf (aes, 4, n * sizeof(uint32_t), (uint8_t *) l3_key2);

  umac_kdf (aes, 0, UMAC_KEY_SIZE, buffer);
  aes128_set_encrypt_key (aes, buffer);
}
