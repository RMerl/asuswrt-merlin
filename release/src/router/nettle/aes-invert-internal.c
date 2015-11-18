/* aes-invert-internal.c

   Inverse key setup for the aes/rijndael block cipher.

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

/* NOTE: We don't include rotated versions of the table. */
static const uint32_t mtable[0x100] =
{
  0x00000000,0x0b0d090e,0x161a121c,0x1d171b12,
  0x2c342438,0x27392d36,0x3a2e3624,0x31233f2a,
  0x58684870,0x5365417e,0x4e725a6c,0x457f5362,
  0x745c6c48,0x7f516546,0x62467e54,0x694b775a,
  0xb0d090e0,0xbbdd99ee,0xa6ca82fc,0xadc78bf2,
  0x9ce4b4d8,0x97e9bdd6,0x8afea6c4,0x81f3afca,
  0xe8b8d890,0xe3b5d19e,0xfea2ca8c,0xf5afc382,
  0xc48cfca8,0xcf81f5a6,0xd296eeb4,0xd99be7ba,
  0x7bbb3bdb,0x70b632d5,0x6da129c7,0x66ac20c9,
  0x578f1fe3,0x5c8216ed,0x41950dff,0x4a9804f1,
  0x23d373ab,0x28de7aa5,0x35c961b7,0x3ec468b9,
  0x0fe75793,0x04ea5e9d,0x19fd458f,0x12f04c81,
  0xcb6bab3b,0xc066a235,0xdd71b927,0xd67cb029,
  0xe75f8f03,0xec52860d,0xf1459d1f,0xfa489411,
  0x9303e34b,0x980eea45,0x8519f157,0x8e14f859,
  0xbf37c773,0xb43ace7d,0xa92dd56f,0xa220dc61,
  0xf66d76ad,0xfd607fa3,0xe07764b1,0xeb7a6dbf,
  0xda595295,0xd1545b9b,0xcc434089,0xc74e4987,
  0xae053edd,0xa50837d3,0xb81f2cc1,0xb31225cf,
  0x82311ae5,0x893c13eb,0x942b08f9,0x9f2601f7,
  0x46bde64d,0x4db0ef43,0x50a7f451,0x5baafd5f,
  0x6a89c275,0x6184cb7b,0x7c93d069,0x779ed967,
  0x1ed5ae3d,0x15d8a733,0x08cfbc21,0x03c2b52f,
  0x32e18a05,0x39ec830b,0x24fb9819,0x2ff69117,
  0x8dd64d76,0x86db4478,0x9bcc5f6a,0x90c15664,
  0xa1e2694e,0xaaef6040,0xb7f87b52,0xbcf5725c,
  0xd5be0506,0xdeb30c08,0xc3a4171a,0xc8a91e14,
  0xf98a213e,0xf2872830,0xef903322,0xe49d3a2c,
  0x3d06dd96,0x360bd498,0x2b1ccf8a,0x2011c684,
  0x1132f9ae,0x1a3ff0a0,0x0728ebb2,0x0c25e2bc,
  0x656e95e6,0x6e639ce8,0x737487fa,0x78798ef4,
  0x495ab1de,0x4257b8d0,0x5f40a3c2,0x544daacc,
  0xf7daec41,0xfcd7e54f,0xe1c0fe5d,0xeacdf753,
  0xdbeec879,0xd0e3c177,0xcdf4da65,0xc6f9d36b,
  0xafb2a431,0xa4bfad3f,0xb9a8b62d,0xb2a5bf23,
  0x83868009,0x888b8907,0x959c9215,0x9e919b1b,
  0x470a7ca1,0x4c0775af,0x51106ebd,0x5a1d67b3,
  0x6b3e5899,0x60335197,0x7d244a85,0x7629438b,
  0x1f6234d1,0x146f3ddf,0x097826cd,0x02752fc3,
  0x335610e9,0x385b19e7,0x254c02f5,0x2e410bfb,
  0x8c61d79a,0x876cde94,0x9a7bc586,0x9176cc88,
  0xa055f3a2,0xab58faac,0xb64fe1be,0xbd42e8b0,
  0xd4099fea,0xdf0496e4,0xc2138df6,0xc91e84f8,
  0xf83dbbd2,0xf330b2dc,0xee27a9ce,0xe52aa0c0,
  0x3cb1477a,0x37bc4e74,0x2aab5566,0x21a65c68,
  0x10856342,0x1b886a4c,0x069f715e,0x0d927850,
  0x64d90f0a,0x6fd40604,0x72c31d16,0x79ce1418,
  0x48ed2b32,0x43e0223c,0x5ef7392e,0x55fa3020,
  0x01b79aec,0x0aba93e2,0x17ad88f0,0x1ca081fe,
  0x2d83bed4,0x268eb7da,0x3b99acc8,0x3094a5c6,
  0x59dfd29c,0x52d2db92,0x4fc5c080,0x44c8c98e,
  0x75ebf6a4,0x7ee6ffaa,0x63f1e4b8,0x68fcedb6,
  0xb1670a0c,0xba6a0302,0xa77d1810,0xac70111e,
  0x9d532e34,0x965e273a,0x8b493c28,0x80443526,
  0xe90f427c,0xe2024b72,0xff155060,0xf418596e,
  0xc53b6644,0xce366f4a,0xd3217458,0xd82c7d56,
  0x7a0ca137,0x7101a839,0x6c16b32b,0x671bba25,
  0x5638850f,0x5d358c01,0x40229713,0x4b2f9e1d,
  0x2264e947,0x2969e049,0x347efb5b,0x3f73f255,
  0x0e50cd7f,0x055dc471,0x184adf63,0x1347d66d,
  0xcadc31d7,0xc1d138d9,0xdcc623cb,0xd7cb2ac5,
  0xe6e815ef,0xede51ce1,0xf0f207f3,0xfbff0efd,
  0x92b479a7,0x99b970a9,0x84ae6bbb,0x8fa362b5,
  0xbe805d9f,0xb58d5491,0xa89a4f83,0xa397468d,
};

#define MIX_COLUMN(T, key) do { \
    uint32_t _k, _nk, _t;	\
    _k = (key);			\
    _nk = T[_k & 0xff];		\
    _k >>= 8;			\
    _t = T[_k & 0xff];		\
    _nk ^= ROTL32(8, _t);	\
    _k >>= 8;			\
    _t = T[_k & 0xff];		\
    _nk ^= ROTL32(16, _t);	\
    _k >>= 8;			\
    _t = T[_k & 0xff];		\
    _nk ^= ROTL32(24, _t);	\
    (key) = _nk;		\
  } while(0)
  

#define SWAP(a, b) \
do { uint32_t t_swap = (a); (a) = (b); (b) = t_swap; } while(0)

void
_aes_invert(unsigned rounds, uint32_t *dst, const uint32_t *src)
{
  unsigned i;

  /* Reverse the order of subkeys, in groups of 4. */
  /* FIXME: Instead of reordering the subkeys, change the access order
     of aes_decrypt, since it's a separate function anyway? */
  if (src == dst)
    {
      unsigned j, k;

      for (i = 0, j = rounds * 4;
	   i < j;
	   i += 4, j -= 4)
	for (k = 0; k<4; k++)
	  SWAP(dst[i+k], dst[j+k]);
    }
  else
    {
      unsigned k;

      for (i = 0; i <= rounds * 4; i += 4)
	for (k = 0; k < 4; k++)
	  dst[i+k] = src[rounds * 4 - i + k];
    }

  /* Transform all subkeys but the first and last. */
  for (i = 4; i < 4 * rounds; i++)
    MIX_COLUMN (mtable, dst[i]);
}
