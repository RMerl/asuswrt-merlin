/*
 * Microsoft Point-to-Point Encryption Protocol (MPPE)
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: mppe.c 241182 2011-02-17 21:50:03Z $
 */

#include <typedefs.h>
#include <string.h>
#include <bcmcrypto/md5.h>

#include "mppe.h"

#define bcopy(src, dst, len) memcpy((dst), (src), (len))
#define bzero(b, len) memset((b), '\0', (len))

/* Encrypt or decrypt a MPPE message in place */
void
mppe_crypt(unsigned char salt[2],		/* 2 bytes Salt */
	   unsigned char *text,	int text_len,	/* Multiple of 16 bytes String */
	   unsigned char *key, int key_len,	/* Shared secret */
	   unsigned char vector[16],		/* 16 bytes Request Authenticator vector */
	   int encrypt)				/* Encrypt if 1 */
{
	unsigned char b[16], c[16], *p;
	MD5_CTX md5;
	int i;

	/* Initial cipher block is Request Authenticator vector */
	bcopy(vector, c, 16);
	for (p = text; &p[15] < &text[text_len]; p += 16) {
		MD5Init(&md5);
		/* Add shared secret */
		MD5Update(&md5, key, key_len);
		/* Add last cipher block */
		MD5Update(&md5, c, 16);
		/* Add salt */
		if (p == text)
			MD5Update(&md5, salt, 2);
		MD5Final(b, &md5);
		if (encrypt) {
			for (i = 0; i < 16; i++) {
				p[i] ^= b[i];
				c[i] = p[i];
			}
		} else {
			for (i = 0; i < 16; i++) {
				c[i] = p[i];
				p[i] ^= b[i];
			}
		}
	}
}
