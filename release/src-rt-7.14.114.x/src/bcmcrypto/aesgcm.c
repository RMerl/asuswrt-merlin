/*
 * aesgcm.c
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: aesgcm.c 349805 2012-08-09 19:01:57Z $
 */


#include <bcmcrypto/aesgcm.h>
#include <typedefs.h>

#ifdef BCMDRIVER
#include <osl.h>
#else
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define ASSERT assert
#endif  /* BCMDRIVER */

#ifdef FLOOR
#undef FLOOR
#endif
#define FLOOR(x, y) ((x) / (y)) * (y)

static void
aes_gcm_encr_cb(void *ctx_in, gcm_block_t in, gcm_block_t out)
{
	aes_gcm_ctx_t *ctx = (aes_gcm_ctx_t*)ctx_in;
	rijndaelEncrypt(ctx->rkey, ctx->nrounds, in, out);
}

void
aes_gcm_init(aes_gcm_ctx_t *ctx, gcm_op_type_t op_type,
	const uint8 *key, size_t key_len,
	const uint8 *nonce, size_t nonce_len,
	const uint8 *aad, size_t aad_len)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->nrounds = (int)AES_ROUNDS(key_len);
	rijndaelKeySetupEnc(ctx->rkey, key, (int)AES_KEY_BITLEN(key_len));
	gcm_init(&ctx->gcm, op_type, aes_gcm_encr_cb, (void*)ctx,
		nonce, nonce_len, aad, aad_len);
}

void
aes_gcm_update(aes_gcm_ctx_t *ctx, uint8 *data, size_t data_len)
{
	gcm_update(&ctx->gcm, data, data_len);
}

void
aes_gcm_final(aes_gcm_ctx_t *ctx, uint8 *data, size_t data_len,
	uint8 *mac, size_t mac_len)
{
	gcm_final(&ctx->gcm, data, data_len, mac, mac_len);
}

static void
aes_gcm_op(gcm_op_type_t op_type, const uint8 *key, size_t key_len,
    const uint8 *nonce, size_t nonce_len, const uint8 *aad, size_t aad_len,
    uint8 *data, size_t data_len, uint8 *mac, size_t mac_len)
{
	aes_gcm_ctx_t ctx;
	size_t len;

	if (!data)
		data_len = 0;

	aes_gcm_init(&ctx, op_type, key, key_len, nonce, nonce_len,
		aad, aad_len);

	len = FLOOR(data_len, GCM_BLOCK_SZ);
	if (len)
		aes_gcm_update(&ctx, data, len);
	data += len;
	data_len -= len;

	aes_gcm_final(&ctx, data, data_len, mac, mac_len);
}

void
aes_gcm_encrypt(const uint8 *key, size_t key_len,
    const uint8 *nonce, size_t nonce_len, const uint8 *aad, size_t aad_len,
    uint8 *data, size_t data_len, uint8 *mac, size_t mac_len)
{
	aes_gcm_op(GCM_ENCRYPT, key, key_len, nonce, nonce_len, aad, aad_len,
		data, data_len, mac, mac_len);
}

void
aes_gcm_mac(const uint8 *key, size_t key_len,
    const uint8 *nonce, size_t nonce_len, const uint8 *aad, size_t aad_len,
    uint8 *data, size_t data_len, uint8 *mac, size_t mac_len)
{
	aes_gcm_op(GCM_MAC, key, key_len, nonce, nonce_len, aad, aad_len,
		data, data_len, mac, mac_len);
}

int
aes_gcm_decrypt(const uint8 *key, size_t key_len,
    const uint8 *nonce, size_t nonce_len, const uint8 *aad, size_t aad_len,
    uint8 *data, size_t data_len, const uint8 *expected_mac, size_t mac_len)
{
	gcm_block_t computed_mac;
	ASSERT(mac_len <= GCM_BLOCK_SZ);
	aes_gcm_op(GCM_DECRYPT, key, key_len, nonce, nonce_len, aad, aad_len,
		data, data_len, computed_mac, mac_len);
	return !memcmp(expected_mac, computed_mac, mac_len);
}

int
aes_gcm_verify(const uint8 *key, size_t key_len,
    const uint8 *nonce, size_t nonce_len, const uint8 *aad, size_t aad_len,
    /* const */ uint8 *data, size_t data_len,
	const uint8 *expected_mac, size_t mac_len)
{
	gcm_block_t computed_mac;
	ASSERT(mac_len <= GCM_BLOCK_SZ);
	aes_gcm_op(GCM_MAC, key, key_len, nonce, nonce_len, aad, aad_len,
		data, data_len, computed_mac, mac_len);
	return !memcmp(expected_mac, computed_mac, mac_len);
}
