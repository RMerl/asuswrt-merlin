/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtomcrypt.com
 */
#include "tomcrypt.h"

/**
   @file zeromem.c
   Zero a block of memory, Tom St Denis
*/

/**
   Zero a block of memory
   @param out    The destination of the area to zero
   @param outlen The length of the area to zero (octets)
*/
void zeromem(void *out, size_t outlen)
{
   unsigned char *mem = out;
   LTC_ARGCHKVD(out != NULL);
   while (outlen-- > 0) {
      *mem++ = 0;
   }
}

/* $Source: /cvs/libtom/libtomcrypt/src/misc/zeromem.c,v $ */
/* $Revision: 1.6 $ */
/* $Date: 2006/06/09 01:38:13 $ */
