/*
 * aesgcm.h
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 */

#ifndef _AESGCM_H_
#define _AESGCM_H_

#include <bcmcrypto/gcm.h>
#include <bcmcrypto/aes.h>

struct aes_gcm_ctx {
	int       nrounds;	/* number of rounds */
	uint32    rkey[(AES_MAXROUNDS + 1) << 2];	/* round key */
	gcm_ctx_t gcm;								/* gcm context */
};

typedef struct aes_gcm_ctx aes_gcm_ctx_t;

void aes_gcm_init(aes_gcm_ctx_t *ctx, gcm_op_type_t op_type,
	const uint8 *key, size_t key_len,
	const uint8 *nonce, size_t nonce_len,
	const uint8 *aad, size_t aad_len);

/* see gcm_update. data_len must be a multiple of AES_BLOCK_SZ */
void aes_gcm_update(aes_gcm_ctx_t *ctx, uint8 *data, size_t data_len);

/* finalize aes gcm - data_len need not be a multiple of AES_BLOCK_SZ */
void aes_gcm_final(aes_gcm_ctx_t *ctx, uint8 *data, size_t data_len,
	uint8 *mac, size_t mac_len);

/* convenience functions */

/* plain text to cipher text, mac. data is modified in place */
void aes_gcm_encrypt(const uint8 *key, size_t key_len,
    const uint8 *nonce, size_t nonce_len, const uint8 *aad, size_t aad_len,
	uint8 *data, size_t data_len, uint8 *mac, size_t mac_len);

/* plain text to mac. data is not modified */
void aes_gcm_mac(const uint8 *key, size_t key_len,
    const uint8 *nonce, size_t nonce_len, const uint8 *aad, size_t aad_len,
	/* const */ uint8 *data, size_t data_len,
	uint8 *mac, size_t mac_len);

/* cipher text to plain text. data in modified in place. expected mac is
 * verified and returns non-zero on success
 */
int aes_gcm_decrypt(const uint8 *key, size_t key_len,
    const uint8 *nonce, size_t nonce_len, const uint8 *aad, size_t aad_len,
	uint8 *data, size_t data_len, const uint8 *expected_mac, size_t mac_len);

/* using plain text, verifies expected mac - return non-zero on success */
int aes_gcm_verify(const uint8 *key, size_t key_len,
    const uint8 *nonce, size_t nonce_len, const uint8 *aad, size_t aad_len,
	/* const */ uint8 *data, size_t data_len,
	const uint8 *expected_mac, size_t mac_len);

#endif /* _AESGCM_H_ */
