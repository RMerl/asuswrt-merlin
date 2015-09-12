/*
 * gcm.h
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: gcm.h 349803 2012-08-09 18:49:41Z $
 */

#ifndef _GCM_H_
#define _GCM_H_

/* GCM is an authenticated encryption mechanism standardized by
 * NIST -  http://csrc.nist.gov/publications/nistpubs/800-38D/SP-800-38D.pdf
 * based on submission http://siswg.net/docs/gcm_spec.pdf
 */

#include <typedefs.h>
#ifdef BCMDRIVER
#include <osl.h>
#else
#include <stddef.h>  /* For size_t */
#endif

#define GCM_BLOCK_SZ 16
typedef uint8 gcm_block_t[GCM_BLOCK_SZ];

#ifndef GCM_TABLE_SZ
#define GCM_TABLE_SZ 0
#endif

#if GCM_TABLE_SZ
#include <bcmcrypto/gcm_tables.h>
#endif

/* GCM works with block ciphers (e.g. AES) with block length of 128 bits, but
 * is independent of the cipher. We abstract the encryption functionlity
 * note: block cipher decryption function not needed  and that in and out
 * may overlap
 */
typedef void (*gcm_encr_fn_t)(void *encr_ctx, gcm_block_t in, gcm_block_t out);

enum gcm_op_type {
	GCM_ENCRYPT = 1,    /* plain txt -> cipher text, mac */
	GCM_DECRYPT,		/* cipher text -> plain text, mac */
	GCM_MAC				/* plain text -> mac */
};

typedef enum gcm_op_type gcm_op_type_t;

struct gcm_ctx {
	gcm_op_type_t op_type; 	/* selected operation */
	gcm_encr_fn_t encr_cb;	/* encryption callback */
	void *encr_cb_ctx;		/* encryption callback context */
	gcm_block_t ej0;		/* encrypted j0 - see spec */
	gcm_block_t counter;	/* gcm counter block - initial = encr(j0+1) */
	gcm_block_t hk;			/* hash key */
	gcm_block_t mac;		/* incremental mac */
	size_t		aad_len;	/* length of aad */
	size_t		data_len;	/* processed data length */

#if GCM_TABLE_SZ
	/* Support for per-key table-based optimization. */
	gcm_table_t hk_table;
#endif
};

typedef struct gcm_ctx gcm_ctx_t;

void gcm_init(gcm_ctx_t *gcm_ctx, gcm_op_type_t op_type,
	gcm_encr_fn_t encr, void *encr_ctx,
	const uint8 *nonce, size_t nonce_len,
	const uint8 *aad, size_t aad_len);

/* update gcm with data. depending on the operation, data is modified
 * in place except GCM_VERIFY where the data is not changed.
 * data_len MUST be a multiple of GCM_BLOCK_SZ
 */
void gcm_update(gcm_ctx_t *gcm_ctx, uint8 *data, size_t data_len);


/* finalize gcm - data length may be 0, and need not be a multiple of
 * GCM_BLOCK_SZ
 */
void gcm_final(gcm_ctx_t *gcm_ctx, uint8 *data, size_t data_len,
	uint8 *mac, size_t mac_len);

/* gcm multiply operation - non-table version */
void gcm_mul_block(const gcm_block_t x, const gcm_block_t y, gcm_block_t out);

#endif /* _GCM_H_ */
