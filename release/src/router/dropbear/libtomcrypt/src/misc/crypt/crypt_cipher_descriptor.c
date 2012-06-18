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
  @file crypt_cipher_descriptor.c
  Stores the cipher descriptor table, Tom St Denis
*/

struct ltc_cipher_descriptor cipher_descriptor[TAB_SIZE] = {
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
 };

LTC_MUTEX_GLOBAL(ltc_cipher_mutex)


/* $Source: /cvs/libtom/libtomcrypt/src/misc/crypt/crypt_cipher_descriptor.c,v $ */
/* $Revision: 1.12 $ */
/* $Date: 2006/11/08 23:01:06 $ */
