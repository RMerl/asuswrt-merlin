/*
 * gcm.c
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: gcm.c 349805 2012-08-09 19:01:57Z $
 */

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

#include <bcmendian.h>
#include <bcmutils.h>
#include <bcmcrypto/gcm.h>

#define GCM_MAX_PLEN_BITS 0x0000007fffffff00ULL

#undef HOWMANY
#undef CEIL
#undef FLOOR

#define HOWMANY(x, y)  (((x) + ((y) - 1)) / (y))
/* Note: CEIL in bcmutils.h is defined differently */
#define CEIL(x, y) HOWMANY(x, y) * (y)
#define FLOOR(x, y) ((x) / (y)) * (y)

/* R - for polynomial chosen for GF(128) 11100001 || 0^128 */
#define GCM_R0 0xe1

#if GCM_BLOCK_SZ == 16
#define XOR_GCM_BLOCK xor_128bit_block
#else
static INLINE void
xor_gcm_block(const gcm_block_t s1, const gcm_block_t s2, gcm_block_t d)
{
	int i;
	for (i = 0; i < GCM_BLOCK_SZ; ++i)
		d[i] = s1[i] ^ s2[i];
}
#define XOR_GCM_BLOCK xor_gcm_block
#endif

static INLINE void
gcm_rshift(gcm_block_t b)
{
	uint8 oc = 0;
	int i;
	for (i = 0; i < GCM_BLOCK_SZ; ++i) {
		uint8 nc = b[i] & 0x1 ? 0x80 : 0;
		b[i] = (b[i] >> 1) | oc;
		oc = nc;
	}
}

/* serialize 64 bit int for GCM - MSB first */
static INLINE void
gcm_serialize64(uint64 x, uint8* p)
{
	*p++ = x >> 56 & 0xff;
	*p++ = x >> 48 & 0xff;
	*p++ = x >> 40 & 0xff;
	*p++ = x >> 32 & 0xff;
	*p++ = x >> 24 & 0xff;
	*p++ = x >> 16 & 0xff;
	*p++ = x >> 8 & 0xff;
	*p++ = x & 0xff;
}

/* Given two blocks, gcm multiply. Note that input(s) and output may overlap */
void
gcm_mul_block(const gcm_block_t x, const gcm_block_t y, gcm_block_t out)
{
	gcm_block_t v, out2;
	int i, j;

	memset(out2, 0, GCM_BLOCK_SZ);
	memcpy(v, y, GCM_BLOCK_SZ);
	for (i = 0; i < GCM_BLOCK_SZ; ++i) {
		const uint8 bx = x[i];
		for (j = 0; j < 8; ++j) {
			uint8 bv;
			if (bx & (0x1 << (~j&7)))
				XOR_GCM_BLOCK(out2, v, out2);

			bv = v[15] & 0x01;
			gcm_rshift(v);
			if (bv)
				v[0] ^= GCM_R0;
		}
	}
	memcpy(out, out2, GCM_BLOCK_SZ);
}

#if !GCM_TABLE_SZ
/* default implementation */
#define GCM_MUL(ctx, y, out) gcm_mul_block(ctx->hk, y, out)
#else
#define GCM_MUL(ctx, y, out) gcm_mul_with_table(&ctx->hk_table, y, out)
#endif /* !GCM_TABLE_SZ */

/* Given a hash key 'ctx->hk' , input with length in bytes a multiple of
 * gcm block size, return the hash value in 'gh'. Caller can pass
 * the last computed value for incremental hash.
 */
static void
gcm_hash(const gcm_ctx_t *ctx, const uint8* in, const size_t len,
	gcm_block_t gh)
{
	int i;
	size_t nb;

	ASSERT(!(len % GCM_BLOCK_SZ));

	nb = len / GCM_BLOCK_SZ;
	for (i = 0; i < nb; ++i) {
		XOR_GCM_BLOCK(gh, in + i * GCM_BLOCK_SZ, gh);
		GCM_MUL(ctx, gh, gh);
	}
}

/* gcm inc32 operation */
static void
gcm_inc32(gcm_block_t b)
{
	uint8* bp = b+15;
	int i;
	for (i = 0; i < 4; ++i) {
		(*bp)++;
		if (*bp--)
			break;
	}
}

/* encrypt using gcm counter, and increment the counter */
static void
gcm_ctr_encr_incr(gcm_ctx_t* ctx, const gcm_block_t in, gcm_block_t out)
{
	gcm_block_t ecb;
	ctx->encr_cb(ctx->encr_cb_ctx, ctx->counter, ecb);
	XOR_GCM_BLOCK(in, ecb, out);
	gcm_inc32(ctx->counter);
}

void
gcm_init(gcm_ctx_t *ctx, gcm_op_type_t op_type,
	gcm_encr_fn_t encr, void *encr_ctx,
	const uint8 *nonce, size_t nonce_len,
	const uint8 *aad, size_t aad_len)
{
	uint8 *hk = ctx->hk;
	uint8 *j0 = ctx->ej0;
	uint8 *ctr = ctx->counter;
	uint8 *mac = ctx->mac;
	size_t v;
	uint8 *tmp;
	size_t tmp_len;
	gcm_block_t btmp;

	ASSERT(op_type == GCM_ENCRYPT || op_type == GCM_DECRYPT ||
		op_type == GCM_MAC);

	memset(ctx, 0, sizeof(*ctx));
	ctx->op_type = op_type;
	ctx->encr_cb = encr;
	ctx->encr_cb_ctx = encr_ctx;
	ctx->aad_len = aad_len;

	/* create hash key */
	encr(encr_ctx, hk, hk);

#if GCM_TABLE_SZ
	/* init per-key table - must be done after hk is initialized */
	gcm_init_table(&ctx->hk_table, ctx->hk);
#endif

	/* compute j0 */
	if (nonce_len == 12) {
		memcpy(j0, nonce, 12);
		memset(j0+12, 0, 3);
		j0[15] = 1;
	}
	else {
		size_t s;
		uint8 tmp2[64];

		s = 16 * HOWMANY(nonce_len, 16) - nonce_len;
		tmp_len = nonce_len + s + 16;
		ASSERT(tmp_len <= sizeof(tmp2));

		tmp = tmp2;
		memcpy(tmp, nonce, nonce_len); tmp += nonce_len;
		memset(tmp, 0, s+8); tmp += s+8;
		gcm_serialize64((uint64)nonce_len << 3, tmp); tmp += 8;
		gcm_hash(ctx, tmp2, tmp_len, j0);
	}

	/* compute initial counter and encrypted j0 */
	memcpy(ctr, j0, GCM_BLOCK_SZ);
	gcm_inc32(ctr);
	encr(encr_ctx, j0, j0);

	/* compute hash over aad */
	v = CEIL(aad_len, GCM_BLOCK_SZ) - aad_len;
	tmp_len = FLOOR(aad_len, GCM_BLOCK_SZ);
	gcm_hash(ctx, aad, tmp_len, mac);
	if (v) {
		memcpy(btmp, aad + tmp_len, GCM_BLOCK_SZ - v);
		memset(btmp + GCM_BLOCK_SZ - v, 0, v);
		gcm_hash(ctx, btmp, GCM_BLOCK_SZ, mac);
	}
}

void
gcm_update(gcm_ctx_t *ctx, uint8 *data, size_t data_len)
{
	size_t off;
	gcm_block_t btmp;

	ASSERT(data_len % GCM_BLOCK_SZ == 0);

	/* process the data; if encryption/decryption is required do it in-place,
	 * otherwise, use temporary block. note that hash is computed over
	 * cipher text
	 */
	for (off = 0; off < data_len; off += GCM_BLOCK_SZ) {
		switch (ctx->op_type) {
		case GCM_ENCRYPT: /* plain txt -> cipher text, mac */
			gcm_ctr_encr_incr(ctx, data + off, data + off);
			gcm_hash(ctx, data + off, GCM_BLOCK_SZ, ctx->mac);
			break;
		case GCM_DECRYPT:  /* cipher text -> plain text, mac */
			gcm_hash(ctx, data + off, GCM_BLOCK_SZ, ctx->mac);
			gcm_ctr_encr_incr(ctx, data + off, data + off);
			break;
		case GCM_MAC:  /* plain text -> mac */
			gcm_ctr_encr_incr(ctx, data + off, btmp);
			gcm_hash(ctx, btmp, GCM_BLOCK_SZ, ctx->mac);
			break;
		default:
			ASSERT(0);
		}
	}

	ctx->data_len += data_len;
}

void
gcm_final(gcm_ctx_t *ctx, uint8 *data, size_t data_len,
	uint8 *mac, size_t mac_len)
{
	size_t len;
	gcm_block_t btmp;

	ASSERT(mac != 0 && mac_len != 0 &&
		mac_len <= GCM_BLOCK_SZ &&
		(mac_len > 12 || (mac_len % 4) == 0)); /* see 5.2.1 SP-800-38D */

	if (!data)
		data_len = 0;

	if (!data_len)
		goto finalize;

	/* process full blocks */
	len = FLOOR(data_len, GCM_BLOCK_SZ);
	if (len)
		gcm_update(ctx, data, len);
	data += len;
	data_len -= len;

	if (!data_len)
		goto finalize;

	/* process partial block */
	memcpy(btmp, data, data_len);
	switch (ctx->op_type) {
	case GCM_ENCRYPT:	/* plain txt -> cipher text, mac */
	case GCM_MAC:		/* plain text -> mac */
		/* don't care about trailing bytes */
		gcm_ctr_encr_incr(ctx, btmp, btmp);
		/* but must be zero for hash computation */
		memset(btmp + data_len, 0, GCM_BLOCK_SZ - data_len);
		gcm_hash(ctx, btmp, GCM_BLOCK_SZ, ctx->mac);
		if (ctx->op_type == GCM_ENCRYPT)
			memcpy(data, btmp, data_len);
		break;
	case GCM_DECRYPT: /* cipher text -> plain text, mac */
		memset(btmp + data_len, 0, GCM_BLOCK_SZ - data_len);
		gcm_hash(ctx, btmp, GCM_BLOCK_SZ, ctx->mac);
		gcm_ctr_encr_incr(ctx, btmp, btmp);
		memcpy(data, btmp, data_len);
		break;
	default:
		ASSERT(0);
	}

	ctx->data_len += data_len;

finalize:
	gcm_serialize64((uint64)ctx->aad_len << 3, btmp);
	gcm_serialize64((uint64)ctx->data_len << 3, btmp + 8);
	gcm_hash(ctx, btmp, GCM_BLOCK_SZ, ctx->mac);
	XOR_GCM_BLOCK(ctx->mac, ctx->ej0, ctx->mac);

	ASSERT((uint64)ctx->data_len << 3 <= GCM_MAX_PLEN_BITS); /* see spec */
	memcpy(mac, ctx->mac, mac_len);
}
