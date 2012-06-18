/*
 * pppcrypt.c - PPP/DES linkage for MS-CHAP and EAP SRP-SHA1
 *
 * Extracted from chap_ms.c by James Carlson.
 *
 * Copyright (c) 1995 Eric Rosenquist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <errno.h>
#include "pppd.h"
#include "pppcrypt.h"

static u_char
Get7Bits(input, startBit)
u_char *input;
int startBit;
{
	unsigned int word;

	word  = (unsigned)input[startBit / 8] << 8;
	word |= (unsigned)input[startBit / 8 + 1];

	word >>= 15 - (startBit % 8 + 7);

	return word & 0xFE;
}

static void
MakeKey(key, des_key)
u_char *key;		/* IN  56 bit DES key missing parity bits */
u_char *des_key;	/* OUT 64 bit DES key with parity bits added */
{
	des_key[0] = Get7Bits(key,  0);
	des_key[1] = Get7Bits(key,  7);
	des_key[2] = Get7Bits(key, 14);
	des_key[3] = Get7Bits(key, 21);
	des_key[4] = Get7Bits(key, 28);
	des_key[5] = Get7Bits(key, 35);
	des_key[6] = Get7Bits(key, 42);
	des_key[7] = Get7Bits(key, 49);

#ifndef USE_CRYPT
	des_set_odd_parity((des_cblock *)des_key);
#endif
}

#ifdef USE_CRYPT
/*
 * in == 8-byte string (expanded version of the 56-bit key)
 * out == 64-byte string where each byte is either 1 or 0
 * Note that the low-order "bit" is always ignored by by setkey()
 */
static void
Expand(in, out)
u_char *in;
u_char *out;
{
        int j, c;
        int i;

        for (i = 0; i < 64; in++){
		c = *in;
                for (j = 7; j >= 0; j--)
                        *out++ = (c >> j) & 01;
                i += 8;
        }
}

/* The inverse of Expand
 */
static void
Collapse(in, out)
u_char *in;
u_char *out;
{
        int j;
        int i;
	unsigned int c;

	for (i = 0; i < 64; i += 8, out++) {
	    c = 0;
	    for (j = 7; j >= 0; j--, in++)
		c |= *in << j;
	    *out = c & 0xff;
	}
}

bool
DesSetkey(key)
u_char *key;
{
	u_char des_key[8];
	u_char crypt_key[66];

	MakeKey(key, des_key);
	Expand(des_key, crypt_key);
	errno = 0;
	setkey((const char *)crypt_key);
	if (errno != 0)
		return (0);
	return (1);
}

bool
DesEncrypt(clear, cipher)
u_char *clear;	/* IN  8 octets */
u_char *cipher;	/* OUT 8 octets */
{
	u_char des_input[66];

	Expand(clear, des_input);
	errno = 0;
	encrypt((char *)des_input, 0);
	if (errno != 0)
		return (0);
	Collapse(des_input, cipher);
	return (1);
}

bool
DesDecrypt(cipher, clear)
u_char *cipher;	/* IN  8 octets */
u_char *clear;	/* OUT 8 octets */
{
	u_char des_input[66];

	Expand(cipher, des_input);
	errno = 0;
	encrypt((char *)des_input, 1);
	if (errno != 0)
		return (0);
	Collapse(des_input, clear);
	return (1);
}

#else /* USE_CRYPT */
static des_key_schedule	key_schedule;

bool
DesSetkey(key)
u_char *key;
{
	des_cblock des_key;
	MakeKey(key, des_key);
	des_set_key(&des_key, key_schedule);
	return (1);
}

bool
DesEncrypt(clear, key, cipher)
u_char *clear;	/* IN  8 octets */
u_char *cipher;	/* OUT 8 octets */
{
	des_ecb_encrypt((des_cblock *)clear, (des_cblock *)cipher,
	    key_schedule, 1);
	return (1);
}

bool
DesDecrypt(cipher, clear)
u_char *cipher;	/* IN  8 octets */
u_char *clear;	/* OUT 8 octets */
{
	des_ecb_encrypt((des_cblock *)cipher, (des_cblock *)clear,
	    key_schedule, 0);
	return (1);
}

#endif /* USE_CRYPT */
