/*
 * rc4.c
 * RC4 stream cipher
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: rc4.c 241182 2011-02-17 21:50:03Z $
 */

#include <typedefs.h>
#include <bcmcrypto/rc4.h>

void
BCMROMFN(prepare_key)(uint8 *key_data, int key_data_len, rc4_ks_t *ks)
{
	unsigned int counter, index1 = 0, index2 = 0;
	uint8 key_byte, temp;
	uint8 *key_state = ks->state;

	for (counter = 0; counter < RC4_STATE_NBYTES; counter++) {
		key_state[counter] = (uint8) counter;
	}

	for (counter = 0; counter < RC4_STATE_NBYTES; counter++) {
		key_byte = key_data[index1];
		index2 = (key_byte + key_state[counter] + index2) % RC4_STATE_NBYTES;
		temp = key_state[counter];
		key_state[counter] = key_state[index2];
		key_state[index2] = temp;
		index1 = (index1 + 1) % key_data_len;
	}

	ks->x = 0;
	ks->y = 0;
}

/* encrypt or decrypt using RC4 */
void
BCMROMFN(rc4)(uint8 *buf, int data_len, rc4_ks_t *ks)
{
	uint8 tmp;
	uint8 xor_ind, x = ks->x, y = ks->y, *key_state = ks->state;
	int i;

	for (i = 0; i < data_len; i++) {
		y += key_state[++x]; /* mod RC4_STATE_NBYTES */
		tmp = key_state[x];
		key_state[x] = key_state[y];
		key_state[y] = tmp;
		xor_ind = key_state[x] + key_state[y];
		buf[i] ^= key_state[xor_ind];
	}

	ks->x = x;
	ks->y = y;
}

#ifdef BCMRC4_TEST
#include <stdio.h>
#include <string.h>

#include "rc4_vectors.h"

#define NUM_VECTORS  (sizeof(rc4_vec) / sizeof(rc4_vec[0]))

int
main(int argc, char **argv)
{
	int k, fail = 0;
	uint8 data[RC4_STATE_NBYTES];
	rc4_ks_t ks;
	for (k = 0; k < NUM_VECTORS; k++) {
		memset(data, 0, RC4_STATE_NBYTES);
		memcpy(data, rc4_vec[k].input, rc4_vec[k].il);

		prepare_key(rc4_vec[k].key, rc4_vec[k].kl, &ks);
		rc4(data, rc4_vec[k].il, &ks);
		if (memcmp(data, rc4_vec[k].ref, rc4_vec[k].il) != 0) {
			printf("%s: rc4 encrypt failed\n", *argv);
			fail++;
		} else {
			printf("%s: rc4 encrypt %d passed\n", *argv, k);
		}

		memset(data, 0, RC4_STATE_NBYTES);
		memcpy(data, rc4_vec[k].ref, rc4_vec[k].il);

		prepare_key(rc4_vec[k].key, rc4_vec[k].kl, &ks);
		rc4(data, rc4_vec[k].il, &ks);
		if (memcmp(data, rc4_vec[k].input, rc4_vec[k].il) != 0) {
			printf("%s: rc4 decrypt failed\n", *argv);
			fail++;
		} else {
			printf("%s: rc4 decrypt %d passed\n", *argv, k);
		}
	}

	printf("%s: %s\n", *argv, fail?"FAILED":"PASSED");
	return (fail);
}
#endif /* BCMRC4_TEST */
