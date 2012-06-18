/*
 * Function: hmac_md5
 * From rfc2104.txt
 *
 * $Id: hmac.c,v 1.16 2009-04-13 16:25:16 Exp $
 */

/*
 *   Copyright (C) The Internet Society (2001).  All Rights Reserved.
 *
 *   This document and translations of it may be copied and furnished to
 *   others, and derivative works that comment on or otherwise explain it
 *   or assist in its implementation may be prepared, copied, published
 *   and distributed, in whole or in part, without restriction of any
 *   kind, provided that the above copyright notice and this paragraph are
 *   included on all such copies and derivative works.  However, this
 *   document itself may not be modified in any way, such as by removing
 *   the copyright notice or references to the Internet Society or other
 *   Internet organizations, except as needed for the purpose of
 *   developing Internet standards in which case the procedures for
 *   copyrights defined in the Internet Standards process must be
 *   followed, or as required to translate it into languages other than
 *   English.
 *
 *   The limited permissions granted above are perpetual and will not be
 *   revoked by the Internet Society or its successors or assigns.
 *
 *   This document and the information contained herein is provided on an
 *   "AS IS" basis and THE INTERNET SOCIETY AND THE INTERNET ENGINEERING
 *   TASK FORCE DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING
 *   BUT NOT LIMITED TO ANY WARRANTY THAT THE USE OF THE INFORMATION
 *   HEREIN WILL NOT INFRINGE ANY RIGHTS OR ANY IMPLIED WARRANTIES OF
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: hmac.c,v 1.16 2009-04-13 16:25:16 Exp $
 */


#include <bcmcrypto/md5.h>

#ifdef BCMDRIVER
#if !defined(BCMSUP_PSK)
#error "BCMSUP_PSK or BCMCCX must be defined to compile hmac.c for driver!"
#endif
#include <osl.h>
#else
#if defined(__GNUC__)
extern void bcopy(const void *src, void *dst, int len);
extern void bzero(void *b, int len);
#else
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bzero(b, len)		memset((b), 0, (len))
#endif
#endif	/* BCMDRIVER */


extern void BCMROMFN(hmac_md5)(unsigned char *text, int text_len, unsigned char *key,
                               int key_len, unsigned char *digest);

/* text		pointer to data stream */
/* text_len	length of data stream */
/* key		pointer to authentication key */
/* key_len	length of authentication key */
/* digest	caller digest to be filled in */
void
BCMROMFN(hmac_md5)(unsigned char *text, int text_len, unsigned char *key,
                   int key_len, unsigned char *digest)
{
	MD5_CTX context;
	unsigned char k_ipad[65];    /* inner padding -
				      * key XORd with ipad
				      */
	unsigned char k_opad[65];    /* outer padding -
				      * key XORd with opad
				      */
	unsigned char tk[16];
	int i;
	/* if key is longer than 64 bytes reset it to key=MD5(key) */
	if (key_len > 64) {

		MD5_CTX      tctx;

		MD5Init(&tctx);
		MD5Update(&tctx, key, key_len);
		MD5Final(tk, &tctx);

		key = tk;
		key_len = 16;
	}

	/*
	 * the HMAC_MD5 transform looks like:
	 *
	 * MD5(K XOR opad, MD5(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times

	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */

	/* start out by storing key in pads */
	bzero(k_ipad, sizeof(k_ipad));
	bzero(k_opad, sizeof(k_opad));
	bcopy(key, k_ipad, key_len);
	bcopy(key, k_opad, key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}
	/*
	 * perform inner MD5
	 */
	MD5Init(&context);                   /* init context for 1st pass */
	MD5Update(&context, k_ipad, 64);     /* start with inner pad */
	MD5Update(&context, text, text_len); /* then text of datagram */
	MD5Final(digest, &context);          /* finish up 1st pass */
	/*
	 * perform outer MD5
	 */
	MD5Init(&context);                   /* init context for 2nd pass */
	MD5Update(&context, k_opad, 64);     /* start with outer pad */
	MD5Update(&context, digest, 16);     /* then results of 1st hash */
	MD5Final(digest, &context);          /* finish up 2nd pass */
}
