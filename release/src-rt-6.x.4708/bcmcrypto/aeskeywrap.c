/*
 * aeskeywrap.c
 * Perform RFC3394 AES-based key wrap and unwrap functions.
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: aeskeywrap.c 241182 2011-02-17 21:50:03Z $
 */

#include <typedefs.h>

#ifdef BCMDRIVER
#include <osl.h>
#else
#include <string.h>
#endif	/* BCMDRIVER */

#include <bcmcrypto/aes.h>
#include <bcmcrypto/aeskeywrap.h>
#include <bcmcrypto/rijndael-alg-fst.h>

#ifdef BCMAESKEYWRAP_TEST
#include <stdio.h>

#define	dbg(args)	printf args

void
pinter(const char *label, const uint8 *A, const size_t il, const uint8 *R)
{
	unsigned int k;
	printf("%s", label);
	for (k = 0; k < AKW_BLOCK_LEN; k++)
		printf("%02X", A[k]);
	printf(" ");
	for (k = 0; k < il; k++) {
		printf("%02X", R[k]);
		if (!((k+1)%AKW_BLOCK_LEN))
			printf(" ");
		}
	printf("\n");
}

void
pres(const char *label, const size_t len, const uint8 *data)
{
	unsigned int k;
	printf("%lu %s", (unsigned long)len, label);
	for (k = 0; k < len; k++) {
		printf("%02x", data[k]);
		if (!((k + 1) % AKW_BLOCK_LEN))
			printf(" ");
	}
	printf("\n");
}
#else
#define	dbg(args)
#define pinter(label, A, il, R)
#endif /* BCMAESKEYWRAP_TEST */

static const uint8 aeskeywrapIV[] = { 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6 };

/* aes_wrap: perform AES-based keywrap function defined in RFC3394
 *	return 0 on success, 1 on error
 *	input is il bytes
 *	output is (il+8) bytes
 */
int
BCMROMFN(aes_wrap)(size_t kl, uint8 *key, size_t il, uint8 *input, uint8 *output)
{
	uint32 rk[4*(AES_MAXROUNDS+1)];
	uint8 A[AES_BLOCK_SZ];
	uint8 R[AKW_MAX_WRAP_LEN];
	uint8 B[AES_BLOCK_SZ];
	int n = (int)(il/AKW_BLOCK_LEN), i, j, k;

	/* validate kl (must be valid AES key length)  */
	if ((kl != 16) && (kl != 24) && (kl != 32)) {
		dbg(("aes_wrap: invlaid key length %lu\n", (unsigned long)kl));
		return (1);
	}
	if (il > AKW_MAX_WRAP_LEN) {
		dbg(("aes_wrap: input length %lu too large\n", (unsigned long)il));
		return (1);
	}
	if (il % AKW_BLOCK_LEN) {
		dbg(("aes_wrap: input length %lu must be a multiple of block length\n",
		     (unsigned long)il));
		return (1);
	}

	dbg(("   Input:\n"));
	dbg(("   KEK:        "));
	for (k = 0; k < (int)kl; k++)
		dbg(("%02X", key[k]));
	dbg(("\n   Key Data:   "));
	for (k = 0; k < (int)il; k++)
		dbg(("%02X", input[k]));
	dbg(("\n\n   Wrap: \n"));

	rijndaelKeySetupEnc(rk, key, (int)AES_KEY_BITLEN(kl));

	/* Set A = IV */
	memcpy(A, aeskeywrapIV, AKW_BLOCK_LEN);
	/* For i = 1 to n */
	/*	R[i] = P[i] */
	memcpy(R, input, il);

	/* For j = 0 to 5 */
	for (j = 0; j < 6; j++) {
		/* For i = 1 to n */
		for (i = 0; i < n; i++) {
			dbg(("\n   %d\n", (n*j)+i+1));
			pinter("   In   ", A, il, R);
			/* B = AES(K, A | R[i]) */
			memcpy(&A[AKW_BLOCK_LEN], &R[i*AKW_BLOCK_LEN], AKW_BLOCK_LEN);
			aes_block_encrypt((int)AES_ROUNDS(kl), rk, A, B);

			/* R[i] = LSB(64, B) */
			memcpy(&R[i*AKW_BLOCK_LEN], &B[AKW_BLOCK_LEN], AKW_BLOCK_LEN);

			/* A = MSB(64, B) ^ t where t = (n*j)+i */
			memcpy(&A[0], &B[0], AKW_BLOCK_LEN);
			pinter("   Enc  ", A, il, R);
			A[AKW_BLOCK_LEN-1] ^= ((n*j)+i+1);
			pinter("   XorT ", A, il, R);
		}
	}
	/* Set C[0] = A */
	memcpy(output, A, AKW_BLOCK_LEN);
	/* For i = 1 to n */
	/* 	C[i] = R[i] */
	memcpy(&output[AKW_BLOCK_LEN], R, il);

	return (0);
}

/* aes_unwrap: perform AES-based key unwrap function defined in RFC3394,
 *	return 0 on success, 1 on error
 *	input is il bytes
 *	output is (il-8) bytes
 */
int
BCMROMFN(aes_unwrap)(size_t kl, uint8 *key, size_t il, uint8 *input, uint8 *output)
{
	uint32 rk[4*(AES_MAXROUNDS+1)];
	uint8 A[AES_BLOCK_SZ];
	uint8 R[AKW_MAX_WRAP_LEN + AKW_BLOCK_LEN];
	uint8 B[AES_BLOCK_SZ];
	size_t ol = il - AKW_BLOCK_LEN;
	int n = (int)(ol/AKW_BLOCK_LEN), i, j, k;

	/* validate kl (must be valid AES key length)  */
	if ((kl != 16) && (kl != 24) && (kl != 32)) {
		dbg(("aes_wrap: invlaid key length %lu\n", (unsigned long)kl));
		return (1);
	}
	if (il > (AKW_MAX_WRAP_LEN + AKW_BLOCK_LEN)) {
		dbg(("aes_unwrap: input length %lu too large\n", (unsigned long)il));
		return (1);
	}
	if (il % AKW_BLOCK_LEN) {
		dbg(("aes_unwrap: input length %lu must be a multiple of block length\n",
		     (unsigned long)il));
		return (1);
	}

	dbg(("   Input:\n"));
	dbg(("   KEK:        "));
	for (k = 0; k < (int)kl; k++)
		dbg(("%02X", key[k]));
	dbg(("\n   Data:       "));
	for (k = 0; k < (int)il; k++)
		dbg(("%02X", input[k]));
	dbg(("\n\n   Unwrap: \n"));

	rijndaelKeySetupDec(rk, key, (int)AES_KEY_BITLEN(kl));

	/* Set A = C[0] */
	memcpy(A, input, AKW_BLOCK_LEN);

	/* For i = 1 to n */
	/*	R[i] = C[i] */
	memcpy(R, &input[AKW_BLOCK_LEN], ol);

	/* For j = 5 to 0 */
	for (j = 5; j >= 0; j--) {
		/* For i = n to 1 */
		for (i = n - 1; i >= 0; i--) {
			dbg(("\n   %d\n", (n*j)+i+1));
			pinter("   In   ", A, ol, R);

			/* B = AES - 1 (K, (A ^ t) | R[i]) where t = n * j + i */
			A[AKW_BLOCK_LEN - 1] ^= ((n*j)+i+1);
			pinter("   XorT ", A, ol, R);

			memcpy(&A[AKW_BLOCK_LEN], &R[i*AKW_BLOCK_LEN], AKW_BLOCK_LEN);
			aes_block_decrypt((int)AES_ROUNDS(kl), rk, A, B);

			/* A = MSB(64, B) */
			memcpy(&A[0], &B[0], AKW_BLOCK_LEN);

			/* R[i] = LSB(64, B) */
			memcpy(&R[i*AKW_BLOCK_LEN], &B[AKW_BLOCK_LEN], AKW_BLOCK_LEN);
			pinter("   Dec  ", A, ol, R);
		}
	}
	if (!memcmp(A, aeskeywrapIV, AKW_BLOCK_LEN)) {
		/* For i = 1 to n */
		/*	P[i] = R[i] */
		memcpy(&output[0], R, ol);
		return 0;
	} else {
		dbg(("aes_unwrap: IV mismatch in unwrapped data\n"));
		return 1;
	}
}

#ifdef BCMAESKEYWRAP_TEST
#include "aeskeywrap_vectors.h"
#define NUM_VECTORS  (sizeof(akw_vec)/sizeof(akw_vec[0]))
#define NUM_WRAP_FAIL_VECTORS  \
	(sizeof(akw_wrap_fail_vec)/sizeof(akw_wrap_fail_vec[0]))
#define NUM_UNWRAP_FAIL_VECTORS  \
	(sizeof(akw_unwrap_fail_vec)/sizeof(akw_unwrap_fail_vec[0]))

int
main(int argc, char **argv)
{
	uint8 output[AKW_MAX_WRAP_LEN+AKW_BLOCK_LEN];
	uint8 input2[AKW_MAX_WRAP_LEN];
	int retv, k, fail = 0;

	for (k = 0; k < NUM_VECTORS; k++) {
		retv = aes_wrap(akw_vec[k].kl, akw_vec[k].key, akw_vec[k].il,
		                akw_vec[k].input, output);
		pres("\n   AES Wrap: ", akw_vec[k].il+AKW_BLOCK_LEN, output);

		if (retv) {
			dbg(("%s: aes_wrap failed\n", *argv));
			fail++;
		}
		if (memcmp(output, akw_vec[k].ref, akw_vec[k].il+AKW_BLOCK_LEN) != 0) {
			dbg(("%s: aes_wrap failed\n", *argv));
			fail++;
		}

		retv = aes_unwrap(akw_vec[k].kl, akw_vec[k].key, akw_vec[k].il + AKW_BLOCK_LEN,
		                  output, input2);
		pres("\n   AES Unwrap: ", akw_vec[k].il, input2);

		if (retv) {
			dbg(("%s: aes_unwrap failed\n", *argv));
			fail++;
		}
		if (memcmp(akw_vec[k].input, input2, akw_vec[k].il) != 0) {
			dbg(("%s: aes_unwrap failed\n", *argv));
			fail++;
		}
	}

	for (k = 0; k < NUM_WRAP_FAIL_VECTORS; k++) {
		if (!aes_wrap(akw_wrap_fail_vec[k].kl, akw_wrap_fail_vec[k].key,
		              akw_wrap_fail_vec[k].il, akw_wrap_fail_vec[k].input, output)) {
			dbg(("%s: aes_wrap didn't detect failure case\n", *argv));
			fail++;
		}
	}

	for (k = 0; k < NUM_UNWRAP_FAIL_VECTORS; k++) {
		if (!aes_unwrap(akw_unwrap_fail_vec[k].kl, akw_unwrap_fail_vec[k].key,
		                akw_unwrap_fail_vec[k].il, akw_unwrap_fail_vec[k].input, input2)) {
			dbg(("%s: aes_unwrap didn't detect failure case\n", *argv));
			fail++;
		}
	}

	dbg(("%s: %s\n", *argv, fail?"FAILED":"PASSED"));
	return (fail);
}
#endif /* BCMAESKEYWRAP_TEST */
