/*
 * Scatterlist Cryptographic API.
 *
 * Copyright (c) 2002 James Morris <jmorris@intercode.com.au>
 * Copyright (c) 2002 David S. Miller (davem@redhat.com)
 *
 * Portions derived from Cryptoapi, by Alexander Kjeldaas <astor@fast.no>
 * and Nettle, by Niels M鰈ler.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */
#include "kmap_types.h"

#include <linux/init.h>
#include <linux/module.h>
//#include <linux/crypto.h>
#include "rtl_crypto.h"
#include <linux/errno.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include "internal.h"

LIST_HEAD(crypto_alg_list);
DECLARE_RWSEM(crypto_alg_sem);

static inline int crypto_alg_get(struct crypto_alg *alg)
{
	return try_inc_mod_count(alg->cra_module);
}

static inline void crypto_alg_put(struct crypto_alg *alg)
{
	if (alg->cra_module)
		__MOD_DEC_USE_COUNT(alg->cra_module);
}

struct crypto_alg *crypto_alg_lookup(const char *name)
{
	struct crypto_alg *q, *alg = NULL;

	if (!name)
		return NULL;

	down_read(&crypto_alg_sem);

	list_for_each_entry(q, &crypto_alg_list, cra_list) {
		if (!(strcmp(q->cra_name, name))) {
			if (crypto_alg_get(q))
				alg = q;
			break;
		}
	}

	up_read(&crypto_alg_sem);
	return alg;
}

static int crypto_init_flags(struct crypto_tfm *tfm, u32 flags)
{
	tfm->crt_flags = 0;

	switch (crypto_tfm_alg_type(tfm)) {
	case CRYPTO_ALG_TYPE_CIPHER:
		return crypto_init_cipher_flags(tfm, flags);

	case CRYPTO_ALG_TYPE_DIGEST:
		return crypto_init_digest_flags(tfm, flags);

	case CRYPTO_ALG_TYPE_COMPRESS:
		return crypto_init_compress_flags(tfm, flags);

	default:
		break;
	}

	BUG();
	return -EINVAL;
}

static int crypto_init_ops(struct crypto_tfm *tfm)
{
	switch (crypto_tfm_alg_type(tfm)) {
	case CRYPTO_ALG_TYPE_CIPHER:
		return crypto_init_cipher_ops(tfm);

	case CRYPTO_ALG_TYPE_DIGEST:
		return crypto_init_digest_ops(tfm);

	case CRYPTO_ALG_TYPE_COMPRESS:
		return crypto_init_compress_ops(tfm);

	default:
		break;
	}

	BUG();
	return -EINVAL;
}

static void crypto_exit_ops(struct crypto_tfm *tfm)
{
	switch (crypto_tfm_alg_type(tfm)) {
	case CRYPTO_ALG_TYPE_CIPHER:
		crypto_exit_cipher_ops(tfm);
		break;

	case CRYPTO_ALG_TYPE_DIGEST:
		crypto_exit_digest_ops(tfm);
		break;

	case CRYPTO_ALG_TYPE_COMPRESS:
		crypto_exit_compress_ops(tfm);
		break;

	default:
		BUG();

	}
}

struct crypto_tfm *crypto_alloc_tfm(const char *name, u32 flags)
{
	struct crypto_tfm *tfm = NULL;
	struct crypto_alg *alg;

	alg = crypto_alg_mod_lookup(name);
	if (alg == NULL)
		goto out;

	tfm = kzalloc(sizeof(*tfm) + alg->cra_ctxsize, GFP_KERNEL);
	if (tfm == NULL)
		goto out_put;

	tfm->__crt_alg = alg;

	if (crypto_init_flags(tfm, flags))
		goto out_free_tfm;

	if (crypto_init_ops(tfm)) {
		crypto_exit_ops(tfm);
		goto out_free_tfm;
	}

	goto out;

out_free_tfm:
	kfree(tfm);
	tfm = NULL;
out_put:
	crypto_alg_put(alg);
out:
	return tfm;
}

void crypto_free_tfm(struct crypto_tfm *tfm)
{
	struct crypto_alg *alg = tfm->__crt_alg;
	int size = sizeof(*tfm) + alg->cra_ctxsize;

	crypto_exit_ops(tfm);
	crypto_alg_put(alg);
	memset(tfm, 0, size);
	kfree(tfm);
}

int crypto_register_alg(struct crypto_alg *alg)
{
	int ret = 0;
	struct crypto_alg *q;

	down_write(&crypto_alg_sem);

	list_for_each_entry(q, &crypto_alg_list, cra_list) {
		if (!(strcmp(q->cra_name, alg->cra_name))) {
			ret = -EEXIST;
			goto out;
		}
	}

	list_add_tail(&alg->cra_list, &crypto_alg_list);
out:
	up_write(&crypto_alg_sem);
	return ret;
}

int crypto_unregister_alg(struct crypto_alg *alg)
{
	int ret = -ENOENT;
	struct crypto_alg *q;

	BUG_ON(!alg->cra_module);

	down_write(&crypto_alg_sem);
	list_for_each_entry(q, &crypto_alg_list, cra_list) {
		if (alg == q) {
			list_del(&alg->cra_list);
			ret = 0;
			goto out;
		}
	}
out:
	up_write(&crypto_alg_sem);
	return ret;
}

int crypto_alg_available(const char *name, u32 flags)
{
	int ret = 0;
	struct crypto_alg *alg = crypto_alg_mod_lookup(name);

	if (alg) {
		crypto_alg_put(alg);
		ret = 1;
	}

	return ret;
}

static int __init init_crypto(void)
{
	printk(KERN_INFO "Initializing Cryptographic API\n");
	crypto_init_proc();
	return 0;
}

__initcall(init_crypto);

/*
EXPORT_SYMBOL_GPL(crypto_register_alg);
EXPORT_SYMBOL_GPL(crypto_unregister_alg);
EXPORT_SYMBOL_GPL(crypto_alloc_tfm);
EXPORT_SYMBOL_GPL(crypto_free_tfm);
EXPORT_SYMBOL_GPL(crypto_alg_available);
*/

EXPORT_SYMBOL_NOVERS(crypto_register_alg);
EXPORT_SYMBOL_NOVERS(crypto_unregister_alg);
EXPORT_SYMBOL_NOVERS(crypto_alloc_tfm);
EXPORT_SYMBOL_NOVERS(crypto_free_tfm);
EXPORT_SYMBOL_NOVERS(crypto_alg_available);
