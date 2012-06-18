/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 **********************************************************************
 ** md5.c                                                            **
 ** RSA Data Security, Inc. MD5 Message Digest Algorithm             **
 ** Created: 2/17/90 RLR                                             **
 ** Revised: 1/91 SRD,AJ,BSK,JT Reference C Version                  **
 **********************************************************************
 */

/*
 **********************************************************************
 ** Copyright (C) 1990, RSA Data Security, Inc. All rights reserved. **
 **                                                                  **
 ** License to copy and use this software is granted provided that   **
 ** it is identified as the "RSA Data Security, Inc. MD5 Message     **
 ** Digest Algorithm" in all material mentioning or referencing this **
 ** software or this function.                                       **
 **                                                                  **
 ** License is also granted to make and use derivative works         **
 ** provided that such works are identified as "derived from the RSA **
 ** Data Security, Inc. MD5 Message Digest Algorithm" in all         **
 ** material mentioning or referencing the derived work.             **
 **                                                                  **
 ** RSA Data Security, Inc. makes no representations concerning      **
 ** either the merchantability of this software or the suitability   **
 ** of this software for any particular purpose.  It is provided "as **
 ** is" without express or implied warranty of any kind.             **
 **                                                                  **
 ** These notices must be retained in any copies of any part of this **
 ** documentation and/or software.                                   **
 **********************************************************************
 */

/* -- include the following line if the md5.h header file is separate -- */
#include "md5.h"

/* forward declaration */
static void Transform ();

static unsigned char PADDING[64] = {
  0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* F, G and H are basic MD5 functions: selection, majority, parity */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z))) 

/* ROTATE_LEFT rotates x left n bits */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4 */
/* Rotation is separate from addition to prevent recomputation */
#define FF(a, b, c, d, x, s, ac) \
  {(a) += F ((b), (c), (d)) + (x) + (UINT4)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) \
  {(a) += G ((b), (c), (d)) + (x) + (UINT4)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) \
  {(a) += H ((b), (c), (d)) + (x) + (UINT4)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) \
  {(a) += I ((b), (c), (d)) + (x) + (UINT4)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }

void MD5Init (mdContext)
MD5_CONTEXT *mdContext;
{
  mdContext->i[0] = mdContext->i[1] = (UINT4)0;

  /* Load magic initialization constants.
   */
  mdContext->buf[0] = (UINT4)0x67452301;
  mdContext->buf[1] = (UINT4)0xefcdab89;
  mdContext->buf[2] = (UINT4)0x98badcfe;
  mdContext->buf[3] = (UINT4)0x10325476;
}

void MD5Update (mdContext, inBuf, inLen)
MD5_CONTEXT *mdContext;
unsigned char *inBuf;
unsigned int inLen;
{
  UINT4 in[16];
  int mdi;
  unsigned int i, ii;

  /* compute number of bytes mod 64 */
  mdi = (int)((mdContext->i[0] >> 3) & 0x3F);

  /* update number of bits */
  if ((mdContext->i[0] + ((UINT4)inLen << 3)) < mdContext->i[0])
    mdContext->i[1]++;
  mdContext->i[0] += ((UINT4)inLen << 3);
  mdContext->i[1] += ((UINT4)inLen >> 29);

  while (inLen--) {
    /* add new character to buffer, increment mdi */
    mdContext->in[mdi++] = *inBuf++;

    /* transform if necessary */
    if (mdi == 0x40) {
      for (i = 0, ii = 0; i < 16; i++, ii += 4)
        in[i] = (((UINT4)mdContext->in[ii+3]) << 24) |
                (((UINT4)mdContext->in[ii+2]) << 16) |
                (((UINT4)mdContext->in[ii+1]) << 8) |
                ((UINT4)mdContext->in[ii]);
      Transform (mdContext->buf, in);
      mdi = 0;
    }
  }
}

void MD5Final (mdContext)
MD5_CONTEXT *mdContext;
{
  UINT4 in[16];
  int mdi;
  unsigned int i, ii;
  unsigned int padLen;

  /* save number of bits */
  in[14] = mdContext->i[0];
  in[15] = mdContext->i[1];

  /* compute number of bytes mod 64 */
  mdi = (int)((mdContext->i[0] >> 3) & 0x3F);

  /* pad out to 56 mod 64 */
  padLen = (mdi < 56) ? (56 - mdi) : (120 - mdi);
  MD5Update (mdContext, PADDING, padLen);

  /* append length in bits and transform */
  for (i = 0, ii = 0; i < 14; i++, ii += 4)
    in[i] = (((UINT4)mdContext->in[ii+3]) << 24) |
            (((UINT4)mdContext->in[ii+2]) << 16) |
            (((UINT4)mdContext->in[ii+1]) << 8) |
            ((UINT4)mdContext->in[ii]);
  Transform (mdContext->buf, in);

  /* store buffer in digest */
  for (i = 0, ii = 0; i < 4; i++, ii += 4) {
    mdContext->digest[ii] = (unsigned char)(mdContext->buf[i] & 0xFF);
    mdContext->digest[ii+1] =
      (unsigned char)((mdContext->buf[i] >> 8) & 0xFF);
    mdContext->digest[ii+2] =
      (unsigned char)((mdContext->buf[i] >> 16) & 0xFF);
    mdContext->digest[ii+3] =
      (unsigned char)((mdContext->buf[i] >> 24) & 0xFF);
  }
}

/* Basic MD5 step. Transform buf based on in.
 */
static void Transform (buf, in)
UINT4 *buf;
UINT4 *in;
{
  UINT4 a = buf[0], b = buf[1], c = buf[2], d = buf[3];

#define D606105819   606105819
#define D1200080426 1200080426
#define D1770035416 1770035416
#define D1804603682 1804603682
#define D1236535329 1236535329
#define D643717713   643717713
#define D38016083     38016083
#define D568446438   568446438
#define D1163531501 1163531501
#define D1735328473 1735328473
#define D1839030562 1839030562
#define D1272893353 1272893353
#define D681279174   681279174
#define D76029189     76029189
#define D530742520   530742520
#define D1126891415 1126891415
#define D1700485571 1700485571
#define D1873313359 1873313359
#define D1309151649 1309151649
#define D718787259   718787259
#define D2240044497 0x85845dd1
#define D2272392833 0x8771f681
#define D2304563134 0x895cd7be
#define D2336552879 0x8b44f7af
#define D2368359562 0x8d2a4c8a
#define D2399980690 0x8f0ccc92
#define D2734768916 0xa3014314
#define D2763975236 0xa4beea44
#define D2792965006 0xa679438e
#define D2821735955 0xa8304613
#define D2850285829 0xa9e3e905
#define D2878612391 0xab9423a7
#define D3174756917 0xbd3af235
#define D3200236656 0xbebfbc70
#define D3225465664 0xc040b340
#define D3250441966 0xc1bdceee
#define D3275163606 0xc33707d6
#define D3299628645 0xc4ac5665
#define D3572445317 0xd4ef3085
#define D3593408605 0xd62f105d
#define D3614090360 0xd76aa478
#define D3634488961 0xd8a1e681
#define D3654602809 0xd9d4d039
#define D3873151461 0xe6db99e5
#define D3889429448 0xe7d3fbc8
#define D3905402710 0xe8c7b756
#define D3921069994 0xe9b6c7aa
#define D3936430074 0xeaa127fa
#define D3951481745 0xeb86d391
#define D4096336452 0xf4292244
#define D4107603335 0xf4d50d87
#define D4118548399 0xf57c0faf
#define D4129170786 0xf61e2562
#define D4139469664 0xf6bb4b60
#define D4149444226 0xf7537e82
#define D4237533241 0xfc93a039
#define D4243563512 0xfcefa3f8
#define D4249261313 0xfd469501
#define D4254626195 0xfd987193
#define D4259657740 0xfde5380c
#define D4264355552 0xfe2ce6e0
#define D4293915773 0xffeff47d
#define D4294588738 0xfffa3942
#define D4294925233 0xffff5bb1

  /* Round 1 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
  FF ( a, b, c, d, in[ 0], S11, D3614090360); /* 1 */
  FF ( d, a, b, c, in[ 1], S12, D3905402710); /* 2 */
  FF ( c, d, a, b, in[ 2], S13,  D606105819); /* 3 */
  FF ( b, c, d, a, in[ 3], S14, D3250441966); /* 4 */
  FF ( a, b, c, d, in[ 4], S11, D4118548399); /* 5 */
  FF ( d, a, b, c, in[ 5], S12, D1200080426); /* 6 */
  FF ( c, d, a, b, in[ 6], S13, D2821735955); /* 7 */
  FF ( b, c, d, a, in[ 7], S14, D4249261313); /* 8 */
  FF ( a, b, c, d, in[ 8], S11, D1770035416); /* 9 */
  FF ( d, a, b, c, in[ 9], S12, D2336552879); /* 10 */
  FF ( c, d, a, b, in[10], S13, D4294925233); /* 11 */
  FF ( b, c, d, a, in[11], S14, D2304563134); /* 12 */
  FF ( a, b, c, d, in[12], S11, D1804603682); /* 13 */
  FF ( d, a, b, c, in[13], S12, D4254626195); /* 14 */
  FF ( c, d, a, b, in[14], S13, D2792965006); /* 15 */
  FF ( b, c, d, a, in[15], S14, D1236535329); /* 16 */

  /* Round 2 */
#define S21 5
#define S22 9
#define S23 14
#define S24 20
  GG ( a, b, c, d, in[ 1], S21, D4129170786); /* 17 */
  GG ( d, a, b, c, in[ 6], S22, D3225465664); /* 18 */
  GG ( c, d, a, b, in[11], S23,  D643717713); /* 19 */
  GG ( b, c, d, a, in[ 0], S24, D3921069994); /* 20 */
  GG ( a, b, c, d, in[ 5], S21, D3593408605); /* 21 */
  GG ( d, a, b, c, in[10], S22,   D38016083); /* 22 */
  GG ( c, d, a, b, in[15], S23, D3634488961); /* 23 */
  GG ( b, c, d, a, in[ 4], S24, D3889429448); /* 24 */
  GG ( a, b, c, d, in[ 9], S21,  D568446438); /* 25 */
  GG ( d, a, b, c, in[14], S22, D3275163606); /* 26 */
  GG ( c, d, a, b, in[ 3], S23, D4107603335); /* 27 */
  GG ( b, c, d, a, in[ 8], S24, D1163531501); /* 28 */
  GG ( a, b, c, d, in[13], S21, D2850285829); /* 29 */
  GG ( d, a, b, c, in[ 2], S22, D4243563512); /* 30 */
  GG ( c, d, a, b, in[ 7], S23, D1735328473); /* 31 */
  GG ( b, c, d, a, in[12], S24, D2368359562); /* 32 */

  /* Round 3 */
#define S31 4
#define S32 11
#define S33 16
#define S34 23
  HH ( a, b, c, d, in[ 5], S31, D4294588738); /* 33 */
  HH ( d, a, b, c, in[ 8], S32, D2272392833); /* 34 */
  HH ( c, d, a, b, in[11], S33, D1839030562); /* 35 */
  HH ( b, c, d, a, in[14], S34, D4259657740); /* 36 */
  HH ( a, b, c, d, in[ 1], S31, D2763975236); /* 37 */
  HH ( d, a, b, c, in[ 4], S32, D1272893353); /* 38 */
  HH ( c, d, a, b, in[ 7], S33, D4139469664); /* 39 */
  HH ( b, c, d, a, in[10], S34, D3200236656); /* 40 */
  HH ( a, b, c, d, in[13], S31,  D681279174); /* 41 */
  HH ( d, a, b, c, in[ 0], S32, D3936430074); /* 42 */
  HH ( c, d, a, b, in[ 3], S33, D3572445317); /* 43 */
  HH ( b, c, d, a, in[ 6], S34,   D76029189); /* 44 */
  HH ( a, b, c, d, in[ 9], S31, D3654602809); /* 45 */
  HH ( d, a, b, c, in[12], S32, D3873151461); /* 46 */
  HH ( c, d, a, b, in[15], S33,  D530742520); /* 47 */
  HH ( b, c, d, a, in[ 2], S34, D3299628645); /* 48 */

  /* Round 4 */
#define S41 6
#define S42 10
#define S43 15
#define S44 21
  II ( a, b, c, d, in[ 0], S41, D4096336452); /* 49 */
  II ( d, a, b, c, in[ 7], S42, D1126891415); /* 50 */
  II ( c, d, a, b, in[14], S43, D2878612391); /* 51 */
  II ( b, c, d, a, in[ 5], S44, D4237533241); /* 52 */
  II ( a, b, c, d, in[12], S41, D1700485571); /* 53 */
  II ( d, a, b, c, in[ 3], S42, D2399980690); /* 54 */
  II ( c, d, a, b, in[10], S43, D4293915773); /* 55 */
  II ( b, c, d, a, in[ 1], S44, D2240044497); /* 56 */
  II ( a, b, c, d, in[ 8], S41, D1873313359); /* 57 */
  II ( d, a, b, c, in[15], S42, D4264355552); /* 58 */
  II ( c, d, a, b, in[ 6], S43, D2734768916); /* 59 */
  II ( b, c, d, a, in[13], S44, D1309151649); /* 60 */
  II ( a, b, c, d, in[ 4], S41, D4149444226); /* 61 */
  II ( d, a, b, c, in[11], S42, D3174756917); /* 62 */
  II ( c, d, a, b, in[ 2], S43,  D718787259); /* 63 */
  II ( b, c, d, a, in[ 9], S44, D3951481745); /* 64 */

  buf[0] += a;
  buf[1] += b;
  buf[2] += c;
  buf[3] += d;
}
