/*
 * Accelerated GHASH implementation with Intel PCLMULQDQ-NI
 * instructions. This file contains glue code.
 *
 * Copyright (c) 2009 Intel Corp.
 *   Author: Huang Ying <ying.huang@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/crypto.h>
#include <crypto/algapi.h>
#include <crypto/cryptd.h>
#include <crypto/gf128mul.h>
#include <crypto/internal/hash.h>
#include <asm/i387.h>

#define GHASH_BLOCK_SIZE	16
#define GHASH_DIGEST_SIZE	16

void clmul_ghash_mul(char *dst, const be128 *shash);

void clmul_ghash_update(char *dst, const char *src, unsigned int srclen,
			const be128 *shash);

void clmul_ghash_setkey(be128 *shash, const u8 *key);

struct ghash_async_ctx {
	struct cryptd_ahash *cryptd_tfm;
};

struct ghash_ctx {
	be128 shash;
};

struct ghash_desc_ctx {
	u8 buffer[GHASH_BLOCK_SIZE];
	u32 bytes;
};

static int ghash_init(struct shash_desc *desc)
{
	struct ghash_desc_ctx *dctx = shash_desc_ctx(desc);

	memset(dctx, 0, sizeof(*dctx));

	return 0;
}

static int ghash_setkey(struct crypto_shash *tfm,
			const u8 *key, unsigned int keylen)
{
	struct ghash_ctx *ctx = crypto_shash_ctx(tfm);

	if (keylen != GHASH_BLOCK_SIZE) {
		crypto_shash_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}

	clmul_ghash_setkey(&ctx->shash, key);

	return 0;
}

static int ghash_update(struct shash_desc *desc,
			 const u8 *src, unsigned int srclen)
{
	struct ghash_desc_ctx *dctx = shash_desc_ctx(desc);
	struct ghash_ctx *ctx = crypto_shash_ctx(desc->tfm);
	u8 *dst = dctx->buffer;

	kernel_fpu_begin();
	if (dctx->bytes) {
		int n = min(srclen, dctx->bytes);
		u8 *pos = dst + (GHASH_BLOCK_SIZE - dctx->bytes);

		dctx->bytes -= n;
		srclen -= n;

		while (n--)
			*pos++ ^= *src++;

		if (!dctx->bytes)
			clmul_ghash_mul(dst, &ctx->shash);
	}

	clmul_ghash_update(dst, src, srclen, &ctx->shash);
	kernel_fpu_end();

	if (srclen & 0xf) {
		src += srclen - (srclen & 0xf);
		srclen &= 0xf;
		dctx->bytes = GHASH_BLOCK_SIZE - srclen;
		while (srclen--)
			*dst++ ^= *src++;
	}

	return 0;
}

static void ghash_flush(struct ghash_ctx *ctx, struct ghash_desc_ctx *dctx)
{
	u8 *dst = dctx->buffer;

	if (dctx->bytes) {
		u8 *tmp = dst + (GHASH_BLOCK_SIZE - dctx->bytes);

		while (dctx->bytes--)
			*tmp++ ^= 0;

		kernel_fpu_begin();
		clmul_ghash_mul(dst, &ctx->shash);
		kernel_fpu_end();
	}

	dctx->bytes = 0;
}

static int ghash_final(struct shash_desc *desc, u8 *dst)
{
	struct ghash_desc_ctx *dctx = shash_desc_ctx(desc);
	struct ghash_ctx *ctx = crypto_shash_ctx(desc->tfm);
	u8 *buf = dctx->buffer;

	ghash_flush(ctx, dctx);
	memcpy(dst, buf, GHASH_BLOCK_SIZE);

	return 0;
}

static struct shash_alg ghash_alg = {
	.digestsize	= GHASH_DIGEST_SIZE,
	.init		= ghash_init,
	.update		= ghash_update,
	.final		= ghash_final,
	.setkey		= ghash_setkey,
	.descsize	= sizeof(struct ghash_desc_ctx),
	.base		= {
		.cra_name		= "__ghash",
		.cra_driver_name	= "__ghash-pclmulqdqni",
		.cra_priority		= 0,
		.cra_flags		= CRYPTO_ALG_TYPE_SHASH,
		.cra_blocksize		= GHASH_BLOCK_SIZE,
		.cra_ctxsize		= sizeof(struct ghash_ctx),
		.cra_module		= THIS_MODULE,
		.cra_list		= LIST_HEAD_INIT(ghash_alg.base.cra_list),
	},
};

static int ghash_async_init(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct ghash_async_ctx *ctx = crypto_ahash_ctx(tfm);
	struct ahash_request *cryptd_req = ahash_request_ctx(req);
	struct cryptd_ahash *cryptd_tfm = ctx->cryptd_tfm;

	if (!irq_fpu_usable()) {
		memcpy(cryptd_req, req, sizeof(*req));
		ahash_request_set_tfm(cryptd_req, &cryptd_tfm->base);
		return crypto_ahash_init(cryptd_req);
	} else {
		struct shash_desc *desc = cryptd_shash_desc(cryptd_req);
		struct crypto_shash *child = cryptd_ahash_child(cryptd_tfm);

		desc->tfm = child;
		desc->flags = req->base.flags;
		return crypto_shash_init(desc);
	}
}

static int ghash_async_update(struct ahash_request *req)
{
	struct ahash_request *cryptd_req = ahash_request_ctx(req);

	if (!irq_fpu_usable()) {
		struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
		struct ghash_async_ctx *ctx = crypto_ahash_ctx(tfm);
		struct cryptd_ahash *cryptd_tfm = ctx->cryptd_tfm;

		memcpy(cryptd_req, req, sizeof(*req));
		ahash_request_set_tfm(cryptd_req, &cryptd_tfm->base);
		return crypto_ahash_update(cryptd_req);
	} else {
		struct shash_desc *desc = cryptd_shash_desc(cryptd_req);
		return shash_ahash_update(req, desc);
	}
}

static int ghash_async_final(struct ahash_request *req)
{
	struct ahash_request *cryptd_req = ahash_request_ctx(req);

	if (!irq_fpu_usable()) {
		struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
		struct ghash_async_ctx *ctx = crypto_ahash_ctx(tfm);
		struct cryptd_ahash *cryptd_tfm = ctx->cryptd_tfm;

		memcpy(cryptd_req, req, sizeof(*req));
		ahash_request_set_tfm(cryptd_req, &cryptd_tfm->base);
		return crypto_ahash_final(cryptd_req);
	} else {
		struct shash_desc *desc = cryptd_shash_desc(cryptd_req);
		return crypto_shash_final(desc, req->result);
	}
}

static int ghash_async_digest(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct ghash_async_ctx *ctx = crypto_ahash_ctx(tfm);
	struct ahash_request *cryptd_req = ahash_request_ctx(req);
	struct cryptd_ahash *cryptd_tfm = ctx->cryptd_tfm;

	if (!irq_fpu_usable()) {
		memcpy(cryptd_req, req, sizeof(*req));
		ahash_request_set_tfm(cryptd_req, &cryptd_tfm->base);
		return crypto_ahash_digest(cryptd_req);
	} else {
		struct shash_desc *desc = cryptd_shash_desc(cryptd_req);
		struct crypto_shash *child = cryptd_ahash_child(cryptd_tfm);

		desc->tfm = child;
		desc->flags = req->base.flags;
		return shash_ahash_digest(req, desc);
	}
}

static int ghash_async_setkey(struct crypto_ahash *tfm, const u8 *key,
			      unsigned int keylen)
{
	struct ghash_async_ctx *ctx = crypto_ahash_ctx(tfm);
	struct crypto_ahash *child = &ctx->cryptd_tfm->base;
	int err;

	crypto_ahash_clear_flags(child, CRYPTO_TFM_REQ_MASK);
	crypto_ahash_set_flags(child, crypto_ahash_get_flags(tfm)
			       & CRYPTO_TFM_REQ_MASK);
	err = crypto_ahash_setkey(child, key, keylen);
	crypto_ahash_set_flags(tfm, crypto_ahash_get_flags(child)
			       & CRYPTO_TFM_RES_MASK);

	return 0;
}

static int ghash_async_init_tfm(struct crypto_tfm *tfm)
{
	struct cryptd_ahash *cryptd_tfm;
	struct ghash_async_ctx *ctx = crypto_tfm_ctx(tfm);

	cryptd_tfm = cryptd_alloc_ahash("__ghash-pclmulqdqni", 0, 0);
	if (IS_ERR(cryptd_tfm))
		return PTR_ERR(cryptd_tfm);
	ctx->cryptd_tfm = cryptd_tfm;
	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct ahash_request) +
				 crypto_ahash_reqsize(&cryptd_tfm->base));

	return 0;
}

static void ghash_async_exit_tfm(struct crypto_tfm *tfm)
{
	struct ghash_async_ctx *ctx = crypto_tfm_ctx(tfm);

	cryptd_free_ahash(ctx->cryptd_tfm);
}

static struct ahash_alg ghash_async_alg = {
	.init		= ghash_async_init,
	.update		= ghash_async_update,
	.final		= ghash_async_final,
	.setkey		= ghash_async_setkey,
	.digest		= ghash_async_digest,
	.halg = {
		.digestsize	= GHASH_DIGEST_SIZE,
		.base = {
			.cra_name		= "ghash",
			.cra_driver_name	= "ghash-clmulni",
			.cra_priority		= 400,
			.cra_flags		= CRYPTO_ALG_TYPE_AHASH | CRYPTO_ALG_ASYNC,
			.cra_blocksize		= GHASH_BLOCK_SIZE,
			.cra_type		= &crypto_ahash_type,
			.cra_module		= THIS_MODULE,
			.cra_list		= LIST_HEAD_INIT(ghash_async_alg.halg.base.cra_list),
			.cra_init		= ghash_async_init_tfm,
			.cra_exit		= ghash_async_exit_tfm,
		},
	},
};

static int __init ghash_pclmulqdqni_mod_init(void)
{
	int err;

	if (!cpu_has_pclmulqdq) {
		printk(KERN_INFO "Intel PCLMULQDQ-NI instructions are not"
		       " detected.\n");
		return -ENODEV;
	}

	err = crypto_register_shash(&ghash_alg);
	if (err)
		goto err_out;
	err = crypto_register_ahash(&ghash_async_alg);
	if (err)
		goto err_shash;

	return 0;

err_shash:
	crypto_unregister_shash(&ghash_alg);
err_out:
	return err;
}

static void __exit ghash_pclmulqdqni_mod_exit(void)
{
	crypto_unregister_ahash(&ghash_async_alg);
	crypto_unregister_shash(&ghash_alg);
}

module_init(ghash_pclmulqdqni_mod_init);
module_exit(ghash_pclmulqdqni_mod_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GHASH Message Digest Algorithm, "
		   "acclerated by PCLMULQDQ-NI");
MODULE_ALIAS("ghash");
