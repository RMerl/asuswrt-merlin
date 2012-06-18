/*
 *	md2.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	MD2 hash implementation
 */
/*
 *	Copyright (c) PeerSec Networks, 2002-2009. All Rights Reserved.
 *	The latest version of this code is available at http://www.matrixssl.org
 *
 *	This software is open source; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This General Public License does NOT permit incorporating this software 
 *	into proprietary programs.  If you are unable to comply with the GPL, a 
 *	commercial license for this software may be purchased from PeerSec Networks
 *	at http://www.peersec.com
 *	
 *	This program is distributed in WITHOUT ANY WARRANTY; without even the 
 *	implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *	See the GNU General Public License for more details.
 *	
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *	http://www.gnu.org/copyleft/gpl.html
 */
/******************************************************************************/

#include "../cryptoLayer.h"

#ifdef USE_MD2

static const unsigned char PI_SUBST[256] = {
  41, 46, 67, 201, 162, 216, 124, 1, 61, 54, 84, 161, 236, 240, 6,
  19, 98, 167, 5, 243, 192, 199, 115, 140, 152, 147, 43, 217, 188,
  76, 130, 202, 30, 155, 87, 60, 253, 212, 224, 22, 103, 66, 111, 24,
  138, 23, 229, 18, 190, 78, 196, 214, 218, 158, 222, 73, 160, 251,
  245, 142, 187, 47, 238, 122, 169, 104, 121, 145, 21, 178, 7, 63,
  148, 194, 16, 137, 11, 34, 95, 33, 128, 127, 93, 154, 90, 144, 50,
  39, 53, 62, 204, 231, 191, 247, 151, 3, 255, 25, 48, 179, 72, 165,
  181, 209, 215, 94, 146, 42, 172, 86, 170, 198, 79, 184, 56, 210,
  150, 164, 125, 182, 118, 252, 107, 226, 156, 116, 4, 241, 69, 157,
  112, 89, 100, 113, 135, 32, 134, 91, 207, 101, 230, 45, 168, 2, 27,
  96, 37, 173, 174, 176, 185, 246, 28, 70, 97, 105, 52, 64, 126, 15,
  85, 71, 163, 35, 221, 81, 175, 58, 195, 92, 249, 206, 186, 197,
  234, 38, 44, 83, 13, 110, 133, 40, 132, 9, 211, 223, 205, 244, 65,
  129, 77, 82, 106, 220, 55, 200, 108, 193, 171, 250, 36, 225, 123,
  8, 12, 189, 177, 74, 120, 136, 149, 139, 227, 99, 232, 109, 233,
  203, 213, 254, 59, 0, 29, 57, 242, 239, 183, 14, 102, 88, 208, 228,
  166, 119, 114, 248, 235, 117, 75, 10, 49, 68, 80, 180, 143, 237,
  31, 26, 219, 153, 141, 51, 159, 17, 131, 20
};

/* adds 16 bytes to the checksum */
static void md2_update_chksum(hash_state *md)
{
   int32 j;
   unsigned char L;
   L = md->md2.chksum[15];
   for (j = 0; j < 16; j++) {

/* caution, the RFC says its "C[j] = S[M[i*16+j] xor L]" but the reference source code [and test vectors] say 
   otherwise.
*/
       L = (md->md2.chksum[j] ^= PI_SUBST[(int32)(md->md2.buf[j] ^ L)] & 255);
   }
}

static void md2_compress(hash_state *md)
{
   int32 j, k;
   unsigned char t;
   
   /* copy block */
   for (j = 0; j < 16; j++) {
       md->md2.X[16+j] = md->md2.buf[j];
       md->md2.X[32+j] = md->md2.X[j] ^ md->md2.X[16+j];
   }

   t = (unsigned char)0;

   /* do 18 rounds */
   for (j = 0; j < 18; j++) {
       for (k = 0; k < 48; k++) {
           t = (md->md2.X[k] ^= PI_SUBST[(int32)(t & 255)]);
       }
       t = (t + (unsigned char)j) & 255;
   }
}

void matrixMd2Init(hash_state *md)
{
   sslAssert(md != NULL);

   /* MD2 uses a zero'ed state... */
   psZeromem(md->md2.X, sizeof(md->md2.X));
   psZeromem(md->md2.chksum, sizeof(md->md2.chksum));
   psZeromem(md->md2.buf, sizeof(md->md2.buf));
   md->md2.curlen = 0;
}

int32 matrixMd2Update(hash_state *md, const unsigned char *buf, unsigned long len)
{
    unsigned long n;
    sslAssert(md != NULL);
    sslAssert(buf != NULL);
    if (md-> md2 .curlen > sizeof(md-> md2 .buf)) {                            
       return CRYPT_INVALID_ARG;                                                           
    }                                                                                       
    while (len > 0) {
        n = MIN(len, (16 - md->md2.curlen));
        memcpy(md->md2.buf + md->md2.curlen, buf, (size_t)n);
        md->md2.curlen += n;
        buf            += n;
        len            -= n;

        /* is 16 bytes full? */
        if (md->md2.curlen == 16) {
            md2_compress(md);
            md2_update_chksum(md);
            md->md2.curlen = 0;
        }
    }
    return CRYPT_OK;
}

int32 matrixMd2Final(hash_state * md, unsigned char *hash)
{
    unsigned long i, k;

    sslAssert(md != NULL);
    sslAssert(hash != NULL);

    if (md->md2.curlen >= sizeof(md->md2.buf)) {
       return CRYPT_INVALID_ARG;
    }


    /* pad the message */
    k = 16 - md->md2.curlen;
    for (i = md->md2.curlen; i < 16; i++) {
        md->md2.buf[i] = (unsigned char)k;
    }

    /* hash and update */
    md2_compress(md);
    md2_update_chksum(md);

    /* hash checksum */
    memcpy(md->md2.buf, md->md2.chksum, 16);
    md2_compress(md);

    /* output is lower 16 bytes of X */
    memcpy(hash, md->md2.X, 16);

#ifdef CLEAN_STACK
    psZeromem(md, sizeof(hash_state));
#endif
    return CRYPT_OK;
}

#ifdef PEERSEC_TEST

int32 matrixMd2Test(void)
{ 
   static const struct {
        char *msg;
        unsigned char md[16];
   } tests[] = {
      { "",
        {0x83,0x50,0xe5,0xa3,0xe2,0x4c,0x15,0x3d,
         0xf2,0x27,0x5c,0x9f,0x80,0x69,0x27,0x73
        }
      },
      { "a",
        {0x32,0xec,0x01,0xec,0x4a,0x6d,0xac,0x72,
         0xc0,0xab,0x96,0xfb,0x34,0xc0,0xb5,0xd1
        }
      },
      { "message digest",
        {0xab,0x4f,0x49,0x6b,0xfb,0x2a,0x53,0x0b,
         0x21,0x9f,0xf3,0x30,0x31,0xfe,0x06,0xb0
        }
      },
      { "abcdefghijklmnopqrstuvwxyz",
        {0x4e,0x8d,0xdf,0xf3,0x65,0x02,0x92,0xab,
         0x5a,0x41,0x08,0xc3,0xaa,0x47,0x94,0x0b
        }
      },
      { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
        {0xda,0x33,0xde,0xf2,0xa4,0x2d,0xf1,0x39,
         0x75,0x35,0x28,0x46,0xc3,0x03,0x38,0xcd
        }
      },
      { "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
        {0xd5,0x97,0x6f,0x79,0xd8,0x3d,0x3a,0x0d,
         0xc9,0x80,0x6c,0x3c,0x66,0xf3,0xef,0xd8
        }
      }
   };
   int32 i;
   hash_state md;
   unsigned char buf[16];

   for (i = 0; i < (int32)(sizeof(tests) / sizeof(tests[0])); i++) {
       matrixMd2Init(&md);
       matrixMd2Update(&md, (unsigned char*)tests[i].msg, (unsigned long)strlen(tests[i].msg));
       matrixMd2Final(&md, buf);
       if (memcmp(buf, tests[i].md, 16) != 0) {
          return CRYPT_FAIL_TESTVECTOR;
       }
   }
   return CRYPT_OK;
}

#endif /* PEERSEC_TEST */

#endif /* USE_MD2 */
