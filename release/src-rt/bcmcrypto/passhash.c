/*
 * passhash.c
 * Perform password to key hash algorithm as defined in WPA and 802.11i
 * specifications
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: passhash.c,v 1.18.230.2 2010-12-01 23:33:28 Exp $
 */

#include <bcmcrypto/passhash.h>
#include <bcmcrypto/sha1.h>
#include <bcmcrypto/prf.h>

#ifdef BCMDRIVER
#include <osl.h>
#else
#if defined(__GNUC__)
extern void bcopy(const void *src, void *dst, size_t len);
extern int bcmp(const void *b1, const void *b2, size_t len);
extern void bzero(void *b, size_t len);
extern size_t strlen(const char *s);
#else
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), 0, (len))
#endif
#endif	/* BCMDRIVER */

#ifdef BCMPASSHASH_TEST
#include <stdio.h>
void prhash(char *password, int passlen, unsigned char *ssid, int ssidlen, unsigned char *output);
#define	dbg(args)	printf args
#endif /* BCMPASSHASH_TEST */

/* F(P, S, c, i) = U1 xor U2 xor ... Uc
 * U1 = PRF(P, S || Int(i)
 * U2 = PRF(P, U1)
 * Uc = PRF(P, Uc-1)
 */
static void
F(char *password, int passlen, unsigned char *ssid, int ssidlength, int iterations, int count,
  unsigned char *output)
{
	unsigned char digest[36], digest1[SHA1HashSize];
	int i, j;

	/* U1 = PRF(P, S || int(i)) */
	if (ssidlength > 32)
		ssidlength = 32;
	bcopy(ssid, digest, ssidlength);
	digest[ssidlength]   = (unsigned char)((count>>24) & 0xff);
	digest[ssidlength+1] = (unsigned char)((count>>16) & 0xff);
	digest[ssidlength+2] = (unsigned char)((count>>8) & 0xff);
	digest[ssidlength+3] = (unsigned char)(count & 0xff);
	hmac_sha1(digest, ssidlength+4, (unsigned char *)password, passlen, digest1);

	/* output = U1 */
	bcopy(digest1, output, SHA1HashSize);

	for (i = 1; i < iterations; i++) {
		/* Un = PRF(P, Un-1) */
		hmac_sha1(digest1, SHA1HashSize, (unsigned char *)password, passlen, digest);
		bcopy(digest, digest1, SHA1HashSize);

		/* output = output xor Un */
		for (j = 0; j < SHA1HashSize; j++) {
			output[j] ^= digest[j];
		}
	}
}

/* passhash: perform passwork->key hash algorithm as defined in WPA and 802.11i
 * specifications.
 *	password is an ascii string of 8 to 63 characters in length
 *	ssid is up to 32 bytes
 *	ssidlen is the length of ssid in bytes
 *	output must be at lest 40 bytes long, and returns a 256 bit key
 *	returns 0 on success, non-zero on failure
 */
int
BCMROMFN(passhash)(char *password, int passlen, unsigned char *ssid, int ssidlen,
                   unsigned char *output)
{
	if ((strlen(password) < 8) || (strlen(password) > 63) || (ssidlen > 32)) return 1;

	F(password, passlen, ssid, ssidlen, 4096, 1, output);
	F(password, passlen, ssid, ssidlen, 4096, 2, &output[SHA1HashSize]);
	return 0;
}

static void
init_F(char *password, int passlen, unsigned char *ssid, int ssidlength,
       int count, unsigned char *lastdigest, unsigned char *output)
{
	unsigned char digest[36];

	/* U0 = PRF(P, S || int(i)) */
	/* output = U0 */
	if (ssidlength > 32)
		ssidlength = 32;
	bcopy(ssid, digest, ssidlength);
	digest[ssidlength]   = (unsigned char)((count>>24) & 0xff);
	digest[ssidlength+1] = (unsigned char)((count>>16) & 0xff);
	digest[ssidlength+2] = (unsigned char)((count>>8) & 0xff);
	digest[ssidlength+3] = (unsigned char)(count & 0xff);
	hmac_sha1(digest, ssidlength+4, (unsigned char *)password, passlen, output);

	/* Save U0 for next PRF() */
	bcopy(output, lastdigest, SHA1HashSize);
}

static void
do_F(char *password, int passlen, int iterations, unsigned char *lastdigest, unsigned char *output)
{
	unsigned char digest[SHA1HashSize];
	int i, j;

	for (i = 0; i < iterations; i++) {
		/* Un = PRF(P, Un-1) */
		hmac_sha1(lastdigest, SHA1HashSize, (unsigned char *)password, passlen, digest);
		/* output = output xor Un */
		for (j = 0; j < SHA1HashSize; j++)
			output[j] ^= digest[j];

		/* Save Un for next PRF() */
		bcopy(digest, lastdigest, SHA1HashSize);
	}
}

/* passhash: Perform passwork to key hash algorithm as defined in WPA and 802.11i
 * specifications. We are breaking this lengthy process into smaller pieces. Users
 * are responsible for making sure password length is between 8 and 63 inclusive.
 *
 * init_passhash: initialize passhash_t structure.
 * do_passhash: advance states in passhash_t structure and return 0 to indicate
 *              it is done and 1 to indicate more to be done.
 * get_passhash: copy passhash result to output buffer.
 */
int
init_passhash(passhash_t *ph,
              char *password, int passlen, unsigned char *ssid, int ssidlen)
{
	if (strlen(password) < 8 || strlen(password) > 63)
		return -1;

	bzero(ph, sizeof(*ph));
	ph->count = 1;
	ph->password = password;
	ph->passlen = passlen;
	ph->ssid = ssid;
	ph->ssidlen = ssidlen;

	return 0;
}

int
do_passhash(passhash_t *ph, int iterations)
{
	unsigned char *output;

	if (ph->count > 2)
		return -1;
	output = ph->output + SHA1HashSize * (ph->count - 1);
	if (ph->iters == 0) {
		init_F(ph->password, ph->passlen, ph->ssid, ph->ssidlen,
		       ph->count, ph->digest, output);
		ph->iters = 1;
		iterations --;
	}
	if (ph->iters + iterations > 4096)
		iterations = 4096 - ph->iters;
	do_F(ph->password, ph->passlen, iterations, ph->digest, output);
	ph->iters += iterations;
	if (ph->iters == 4096) {
		ph->count ++;
		ph->iters = 0;
		if (ph->count > 2)
			return 0;
	}
	return 1;
}

int
get_passhash(passhash_t *ph, unsigned char *output, int outlen)
{
	if (ph->count > 2 && outlen <= (int)sizeof(ph->output)) {
		bcopy(ph->output, output, outlen);
		return 0;
	}
	return -1;
}

#ifdef BCMPASSHASH_TEST
void
prhash(char *password, int passlen, unsigned char *ssid, int ssidlen, unsigned char *output)
{
	int k;
	printf("pass\n\t%s\nssid(hex)\n\t", password);
	for (k = 0; k < ssidlen; k++) {
		printf("%02x ", ssid[k]);
		if (!((k+1)%16)) printf("\n\t");
	}
	printf("\nhash\n\t");
	for (k = 0; k < 2 * SHA1HashSize; k++) {
		printf("%02x ", output[k]);
		if (!((k + 1) % SHA1HashSize)) printf("\n\t");
	}
	printf("\n");
}

#include "passhash_vectors.h"

int main(int argc, char **argv)
{
	unsigned char output[2*SHA1HashSize];
	int retv, k, fail = 0, fail1 = 0;
	passhash_t passhash_states;

	dbg(("%s: testing passhash()\n", *argv));

	for (k = 0; k < NUM_PASSHASH_VECTORS; k++) {
		printf("Passhash test %d:\n", k);
		bzero(output, sizeof(output));
		retv = passhash(passhash_vec[k].pass, passhash_vec[k].pl,
			passhash_vec[k].salt, passhash_vec[k].sl, output);
		prhash(passhash_vec[k].pass, passhash_vec[k].pl,
			passhash_vec[k].salt, passhash_vec[k].sl, output);

		if (retv) {
			dbg(("%s: passhash() test %d returned error\n", *argv, k));
			fail++;
		}
		if (bcmp(output, passhash_vec[k].ref, 2*SHA1HashSize) != 0) {
			dbg(("%s: passhash test %d reference mismatch\n", *argv, k));
			fail++;
		}
	}

	dbg(("%s: %s\n", *argv, fail?"FAILED":"PASSED"));

	dbg(("%s: testing init_passhash()/do_passhash()/get_passhash()\n", *argv));

	for (k = 0; k < NUM_PASSHASH_VECTORS; k++) {
		printf("Passhash test %d:\n", k);
		init_passhash(&passhash_states,
		              passhash_vec[k].pass, passhash_vec[k].pl,
		              passhash_vec[k].salt, passhash_vec[k].sl);
		while ((retv = do_passhash(&passhash_states, 100)) > 0)
			;
		get_passhash(&passhash_states, output, sizeof(output));
		prhash(passhash_vec[k].pass, passhash_vec[k].pl,
		       passhash_vec[k].salt, passhash_vec[k].sl, output);

		if (retv < 0) {
			dbg(("%s: passhash() test %d returned error\n", *argv, k));
			fail1++;
		}
		if (bcmp(output, passhash_vec[k].ref, 2*SHA1HashSize) != 0) {
			dbg(("%s: passhash test %d reference mismatch\n", *argv, k));
			fail1++;
		}
	}

	dbg(("%s: %s\n", *argv, fail1?"FAILED":"PASSED"));
	return (fail+fail1);
}
#endif	/* BCMPASSHASH_TEST */
