/*
 * Cryptographic API.
 *
 * Null algorithms, aka Much Ado About Nothing.
 *
 * These are needed for IPsec, and may be useful in general for
 * testing & debugging.
 *
 * The null cipher is compliant with RFC2410.
 *
 * Copyright (c) 2002 James Morris <jmorris@intercode.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <crypto/internal/hash.h>
#include <crypto/internal/skcipher.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/string.h>

#define NULL_KEY_SIZE		0
#define NULL_BLOCK_SIZE		1
#define NULL_DIGEST_SIZE	0
#define NULL_IV_SIZE		0

static int null_compress(struct crypto_tfm *tfm, const u8 *src,
			 unsigned int slen, u8 *dst, unsigned int *dlen)
{
	if (slen > *dlen)
		return -EINVAL;
	memcpy(dst, src, slen);
	*dlen = slen;
	return 0;
}

static int null_init(struct shash_desc *desc)
{
	return 0;
}

static int null_update(struct shash_desc *desc, const u8 *data,
		       unsigned int len)
{
	return 0;
}

static int null_final(struct shash_desc *desc, u8 *out)
{
	return 0;
}

static int null_digest(struct shash_desc *desc, const u8 *data,
		       unsigned int len, u8 *out)
{
	return 0;
}

static int null_hash_setkey(struct crypto_shash *tfm, const u8 *key,
			    unsigned int keylen)
{ return 0; }

static int null_setkey(struct crypto_tfm *tfm, const u8 *key,
		       unsigned int keylen)
{ return 0; }

static void null_crypt(struct crypto_tfm *tfm, u8 *dst, const u8 *src)
{
	memcpy(dst, src, NULL_BLOCK_SIZE);
}

static int skcipher_null_crypt(struct blkcipher_desc *desc,
			       struct scatterlist *dst,
			       struct scatterlist *src, unsigned int nbytes)
{
	struct blkcipher_walk walk;
	int err;

	blkcipher_walk_init(&walk, dst, src, nbytes);
	err = blkcipher_walk_virt(desc, &walk);

	while (walk.nbytes) {
		if (walk.src.virt.addr != walk.dst.virt.addr)
			memcpy(walk.dst.virt.addr, walk.src.virt.addr,
			       walk.nbytes);
		err = blkcipher_walk_done(desc, &walk, 0);
	}

	return err;
}

static struct crypto_alg compress_null = {
	.cra_name		=	"compress_null",
	.cra_flags		=	CRYPTO_ALG_TYPE_COMPRESS,
	.cra_blocksize		=	NULL_BLOCK_SIZE,
	.cra_ctxsize		=	0,
	.cra_module		=	THIS_MODULE,
	.cra_list		=       LIST_HEAD_INIT(compress_null.cra_list),
	.cra_u			=	{ .compress = {
	.coa_compress 		=	null_compress,
	.coa_decompress		=	null_compress } }
};

static struct shash_alg digest_null = {
	.digestsize		=	NULL_DIGEST_SIZE,
	.setkey   		=	null_hash_setkey,
	.init   		=	null_init,
	.update 		=	null_update,
	.finup 			=	null_digest,
	.digest 		=	null_digest,
	.final  		=	null_final,
	.base			=	{
		.cra_name		=	"digest_null",
		.cra_flags		=	CRYPTO_ALG_TYPE_SHASH,
		.cra_blocksize		=	NULL_BLOCK_SIZE,
		.cra_module		=	THIS_MODULE,
	}
};

static struct crypto_alg cipher_null = {
	.cra_name		=	"cipher_null",
	.cra_flags		=	CRYPTO_ALG_TYPE_CIPHER,
	.cra_blocksize		=	NULL_BLOCK_SIZE,
	.cra_ctxsize		=	0,
	.cra_module		=	THIS_MODULE,
	.cra_list		=	LIST_HEAD_INIT(cipher_null.cra_list),
	.cra_u			=	{ .cipher = {
	.cia_min_keysize	=	NULL_KEY_SIZE,
	.cia_max_keysize	=	NULL_KEY_SIZE,
	.cia_setkey		= 	null_setkey,
	.cia_encrypt		=	null_crypt,
	.cia_decrypt		=	null_crypt } }
};

static struct crypto_alg skcipher_null = {
	.cra_name		=	"ecb(cipher_null)",
	.cra_driver_name	=	"ecb-cipher_null",
	.cra_priority		=	100,
	.cra_flags		=	CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_blocksize		=	NULL_BLOCK_SIZE,
	.cra_type		=	&crypto_blkcipher_type,
	.cra_ctxsize		=	0,
	.cra_module		=	THIS_MODULE,
	.cra_list		=	LIST_HEAD_INIT(skcipher_null.cra_list),
	.cra_u			=	{ .blkcipher = {
	.min_keysize		=	NULL_KEY_SIZE,
	.max_keysize		=	NULL_KEY_SIZE,
	.ivsize			=	NULL_IV_SIZE,
	.setkey			= 	null_setkey,
	.encrypt		=	skcipher_null_crypt,
	.decrypt		=	skcipher_null_crypt } }
};

MODULE_ALIAS("compress_null");
MODULE_ALIAS("digest_null");
MODULE_ALIAS("cipher_null");

static int __init crypto_null_mod_init(void)
{
	int ret = 0;

	ret = crypto_register_alg(&cipher_null);
	if (ret < 0)
		goto out;

	ret = crypto_register_alg(&skcipher_null);
	if (ret < 0)
		goto out_unregister_cipher;

	ret = crypto_register_shash(&digest_null);
	if (ret < 0)
		goto out_unregister_skcipher;

	ret = crypto_register_alg(&compress_null);
	if (ret < 0)
		goto out_unregister_digest;

out:
	return ret;

out_unregister_digest:
	crypto_unregister_shash(&digest_null);
out_unregister_skcipher:
	crypto_unregister_alg(&skcipher_null);
out_unregister_cipher:
	crypto_unregister_alg(&cipher_null);
	goto out;
}

static void __exit crypto_null_mod_fini(void)
{
	crypto_unregister_alg(&compress_null);
	crypto_unregister_shash(&digest_null);
	crypto_unregister_alg(&skcipher_null);
	crypto_unregister_alg(&cipher_null);
}

module_init(crypto_null_mod_init);
module_exit(crypto_null_mod_fini);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Null Cryptographic Algorithms");
