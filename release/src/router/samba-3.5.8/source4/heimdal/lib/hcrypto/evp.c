/*
 * Copyright (c) 2006 - 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define HC_DEPRECATED
#define HC_DEPRECATED_CRYPTO

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <evp.h>

#include <krb5-types.h>

#include "camellia.h"
#include <des.h>
#include <sha.h>
#include <rc2.h>
#include <rc4.h>
#include <md2.h>
#include <md4.h>
#include <md5.h>

/**
 * @page page_evp EVP - generic crypto interface
 *
 * See the library functions here: @ref hcrypto_evp
 *
 * @section evp_cipher EVP Cipher
 *
 * The use of EVP_CipherInit_ex() and EVP_Cipher() is pretty easy to
 * understand forward, then EVP_CipherUpdate() and
 * EVP_CipherFinal_ex() really needs an example to explain @ref
 * example_evp_cipher.c .
 *
 * @example example_evp_cipher.c
 *
 * This is an example how to use EVP_CipherInit_ex(),
 * EVP_CipherUpdate() and EVP_CipherFinal_ex().
 */

struct hc_EVP_MD_CTX {
    const EVP_MD *md;
    ENGINE *engine;
    void *ptr;
};


/**
 * Return the output size of the message digest function.
 *
 * @param md the evp message
 *
 * @return size output size of the message digest function.
 *
 * @ingroup hcrypto_evp
 */

size_t
EVP_MD_size(const EVP_MD *md)
{
    return md->hash_size;
}

/**
 * Return the blocksize of the message digest function.
 *
 * @param md the evp message
 *
 * @return size size of the message digest block size
 *
 * @ingroup hcrypto_evp
 */

size_t
EVP_MD_block_size(const EVP_MD *md)
{
    return md->block_size;
}

/**
 * Allocate a messsage digest context object. Free with
 * EVP_MD_CTX_destroy().
 *
 * @return a newly allocated message digest context object.
 *
 * @ingroup hcrypto_evp
 */

EVP_MD_CTX *
EVP_MD_CTX_create(void)
{
    return calloc(1, sizeof(EVP_MD_CTX));
}

/**
 * Initiate a messsage digest context object. Deallocate with
 * EVP_MD_CTX_cleanup(). Please use EVP_MD_CTX_create() instead.
 *
 * @param ctx variable to initiate.
 *
 * @ingroup hcrypto_evp
 */

void HC_DEPRECATED
EVP_MD_CTX_init(EVP_MD_CTX *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

/**
 * Free a messsage digest context object.
 *
 * @param ctx context to free.
 *
 * @ingroup hcrypto_evp
 */

void
EVP_MD_CTX_destroy(EVP_MD_CTX *ctx)
{
    EVP_MD_CTX_cleanup(ctx);
    free(ctx);
}

/**
 * Free the resources used by the EVP_MD context.
 *
 * @param ctx the context to free the resources from.
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_evp
 */

int HC_DEPRECATED
EVP_MD_CTX_cleanup(EVP_MD_CTX *ctx)
{
    if (ctx->md && ctx->md->cleanup)
	(ctx->md->cleanup)(ctx);
    ctx->md = NULL;
    ctx->engine = NULL;
    free(ctx->ptr);
    memset(ctx, 0, sizeof(*ctx));
    return 1;
}

/**
 * Get the EVP_MD use for a specified context.
 *
 * @param ctx the EVP_MD context to get the EVP_MD for.
 *
 * @return the EVP_MD used for the context.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_MD_CTX_md(EVP_MD_CTX *ctx)
{
    return ctx->md;
}

/**
 * Return the output size of the message digest function.
 *
 * @param ctx the evp message digest context
 *
 * @return size output size of the message digest function.
 *
 * @ingroup hcrypto_evp
 */

size_t
EVP_MD_CTX_size(EVP_MD_CTX *ctx)
{
    return EVP_MD_size(ctx->md);
}

/**
 * Return the blocksize of the message digest function.
 *
 * @param ctx the evp message digest context
 *
 * @return size size of the message digest block size
 *
 * @ingroup hcrypto_evp
 */

size_t
EVP_MD_CTX_block_size(EVP_MD_CTX *ctx)
{
    return EVP_MD_block_size(ctx->md);
}

/**
 * Init a EVP_MD_CTX for use a specific message digest and engine.
 *
 * @param ctx the message digest context to init.
 * @param md the message digest to use.
 * @param engine the engine to use, NULL to use the default engine.
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_evp
 */

int
EVP_DigestInit_ex(EVP_MD_CTX *ctx, const EVP_MD *md, ENGINE *engine)
{
    if (ctx->md != md || ctx->engine != engine) {
	EVP_MD_CTX_cleanup(ctx);
	ctx->md = md;
	ctx->engine = engine;

	ctx->ptr = calloc(1, md->ctx_size);
	if (ctx->ptr == NULL)
	    return 0;
    }
    (ctx->md->init)(ctx->ptr);
    return 1;
}

/**
 * Update the digest with some data.
 *
 * @param ctx the context to update
 * @param data the data to update the context with
 * @param size length of data
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_evp
 */

int
EVP_DigestUpdate(EVP_MD_CTX *ctx, const void *data, size_t size)
{
    (ctx->md->update)(ctx->ptr, data, size);
    return 1;
}

/**
 * Complete the message digest.
 *
 * @param ctx the context to complete.
 * @param hash the output of the message digest function. At least
 * EVP_MD_size().
 * @param size the output size of hash.
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_evp
 */

int
EVP_DigestFinal_ex(EVP_MD_CTX *ctx, void *hash, unsigned int *size)
{
    (ctx->md->final)(hash, ctx->ptr);
    if (size)
	*size = ctx->md->hash_size;
    return 1;
}

/**
 * Do the whole EVP_MD_CTX_create(), EVP_DigestInit_ex(),
 * EVP_DigestUpdate(), EVP_DigestFinal_ex(), EVP_MD_CTX_destroy()
 * dance in one call.
 *
 * @param data the data to update the context with
 * @param dsize length of data
 * @param hash output data of at least EVP_MD_size() length.
 * @param hsize output length of hash.
 * @param md message digest to use
 * @param engine engine to use, NULL for default engine.
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_evp
 */

int
EVP_Digest(const void *data, size_t dsize, void *hash, unsigned int *hsize,
	   const EVP_MD *md, ENGINE *engine)
{
    EVP_MD_CTX *ctx;
    int ret;

    ctx = EVP_MD_CTX_create();
    if (ctx == NULL)
	return 0;
    ret = EVP_DigestInit_ex(ctx, md, engine);
    if (ret != 1) {
	EVP_MD_CTX_destroy(ctx);
	return ret;
    }
    ret = EVP_DigestUpdate(ctx, data, dsize);
    if (ret != 1) {
	EVP_MD_CTX_destroy(ctx);
	return ret;
    }
    ret = EVP_DigestFinal_ex(ctx, hash, hsize);
    EVP_MD_CTX_destroy(ctx);
    return ret;
}

/**
 * The message digest SHA256
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_sha256(void)
{
    static const struct hc_evp_md sha256 = {
	32,
	64,
	sizeof(SHA256_CTX),
	(hc_evp_md_init)SHA256_Init,
	(hc_evp_md_update)SHA256_Update,
	(hc_evp_md_final)SHA256_Final,
	NULL
    };
    return &sha256;
}

static const struct hc_evp_md sha1 = {
    20,
    64,
    sizeof(SHA_CTX),
    (hc_evp_md_init)SHA1_Init,
    (hc_evp_md_update)SHA1_Update,
    (hc_evp_md_final)SHA1_Final,
    NULL
};

/**
 * The message digest SHA1
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_sha1(void)
{
    return &sha1;
}

/**
 * The message digest SHA1
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_sha(void)
{
    return &sha1;
}

/**
 * The message digest MD5
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_md5(void)
{
    static const struct hc_evp_md md5 = {
	16,
	64,
	sizeof(MD5_CTX),
	(hc_evp_md_init)MD5_Init,
	(hc_evp_md_update)MD5_Update,
	(hc_evp_md_final)MD5_Final,
	NULL
    };
    return &md5;
}

/**
 * The message digest MD4
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_md4(void)
{
    static const struct hc_evp_md md4 = {
	16,
	64,
	sizeof(MD4_CTX),
	(hc_evp_md_init)MD4_Init,
	(hc_evp_md_update)MD4_Update,
	(hc_evp_md_final)MD4_Final,
	NULL
    };
    return &md4;
}

/**
 * The message digest MD2
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_md2(void)
{
    static const struct hc_evp_md md2 = {
	16,
	16,
	sizeof(MD2_CTX),
	(hc_evp_md_init)MD2_Init,
	(hc_evp_md_update)MD2_Update,
	(hc_evp_md_final)MD2_Final,
	NULL
    };
    return &md2;
}

/*
 *
 */

static void
null_Init (void *m)
{
}
static void
null_Update (void *m, const void * data, size_t size)
{
}
static void
null_Final(void *res, void *m)
{
}

/**
 * The null message digest
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_md_null(void)
{
    static const struct hc_evp_md null = {
	0,
	0,
	0,
	(hc_evp_md_init)null_Init,
	(hc_evp_md_update)null_Update,
	(hc_evp_md_final)null_Final,
	NULL
    };
    return &null;
}

/**
 * Return the block size of the cipher.
 *
 * @param c cipher to get the block size from.
 *
 * @return the block size of the cipher.
 *
 * @ingroup hcrypto_evp
 */

size_t
EVP_CIPHER_block_size(const EVP_CIPHER *c)
{
    return c->block_size;
}

/**
 * Return the key size of the cipher.
 *
 * @param c cipher to get the key size from.
 *
 * @return the key size of the cipher.
 *
 * @ingroup hcrypto_evp
 */

size_t
EVP_CIPHER_key_length(const EVP_CIPHER *c)
{
    return c->key_len;
}

/**
 * Return the IV size of the cipher.
 *
 * @param c cipher to get the IV size from.
 *
 * @return the IV size of the cipher.
 *
 * @ingroup hcrypto_evp
 */

size_t
EVP_CIPHER_iv_length(const EVP_CIPHER *c)
{
    return c->iv_len;
}

/**
 * Initiate a EVP_CIPHER_CTX context. Clean up with
 * EVP_CIPHER_CTX_cleanup().
 *
 * @param c the cipher initiate.
 *
 * @ingroup hcrypto_evp
 */

void
EVP_CIPHER_CTX_init(EVP_CIPHER_CTX *c)
{
    memset(c, 0, sizeof(*c));
}

/**
 * Clean up the EVP_CIPHER_CTX context.
 *
 * @param c the cipher to clean up.
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_evp
 */

int
EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *c)
{
    if (c->cipher && c->cipher->cleanup)
	c->cipher->cleanup(c);
    if (c->cipher_data) {
	free(c->cipher_data);
	c->cipher_data = NULL;
    }
    return 1;
}

#if 0
int
EVP_CIPHER_CTX_set_key_length(EVP_CIPHER_CTX *c, int length)
{
    return 0;
}

int
EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *c, int pad)
{
    return 0;
}
#endif

/**
 * Return the EVP_CIPHER for a EVP_CIPHER_CTX context.
 *
 * @param ctx the context to get the cipher type from.
 *
 * @return the EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_CIPHER_CTX_cipher(EVP_CIPHER_CTX *ctx)
{
    return ctx->cipher;
}

/**
 * Return the block size of the cipher context.
 *
 * @param ctx cipher context to get the block size from.
 *
 * @return the block size of the cipher context.
 *
 * @ingroup hcrypto_evp
 */

size_t
EVP_CIPHER_CTX_block_size(const EVP_CIPHER_CTX *ctx)
{
    return EVP_CIPHER_block_size(ctx->cipher);
}

/**
 * Return the key size of the cipher context.
 *
 * @param ctx cipher context to get the key size from.
 *
 * @return the key size of the cipher context.
 *
 * @ingroup hcrypto_evp
 */

size_t
EVP_CIPHER_CTX_key_length(const EVP_CIPHER_CTX *ctx)
{
    return EVP_CIPHER_key_length(ctx->cipher);
}

/**
 * Return the IV size of the cipher context.
 *
 * @param ctx cipher context to get the IV size from.
 *
 * @return the IV size of the cipher context.
 *
 * @ingroup hcrypto_evp
 */

size_t
EVP_CIPHER_CTX_iv_length(const EVP_CIPHER_CTX *ctx)
{
    return EVP_CIPHER_iv_length(ctx->cipher);
}

/**
 * Get the flags for an EVP_CIPHER_CTX context.
 *
 * @param ctx the EVP_CIPHER_CTX to get the flags from
 *
 * @return the flags for an EVP_CIPHER_CTX.
 *
 * @ingroup hcrypto_evp
 */

unsigned long
EVP_CIPHER_CTX_flags(const EVP_CIPHER_CTX *ctx)
{
    return ctx->cipher->flags;
}

/**
 * Get the mode for an EVP_CIPHER_CTX context.
 *
 * @param ctx the EVP_CIPHER_CTX to get the mode from
 *
 * @return the mode for an EVP_CIPHER_CTX.
 *
 * @ingroup hcrypto_evp
 */

int
EVP_CIPHER_CTX_mode(const EVP_CIPHER_CTX *ctx)
{
    return EVP_CIPHER_CTX_flags(ctx) & EVP_CIPH_MODE;
}

/**
 * Get the app data for an EVP_CIPHER_CTX context.
 *
 * @param ctx the EVP_CIPHER_CTX to get the app data from
 *
 * @return the app data for an EVP_CIPHER_CTX.
 *
 * @ingroup hcrypto_evp
 */

void *
EVP_CIPHER_CTX_get_app_data(EVP_CIPHER_CTX *ctx)
{
    return ctx->app_data;
}

/**
 * Set the app data for an EVP_CIPHER_CTX context.
 *
 * @param ctx the EVP_CIPHER_CTX to set the app data for
 * @param data the app data to set for an EVP_CIPHER_CTX.
 *
 * @ingroup hcrypto_evp
 */

void
EVP_CIPHER_CTX_set_app_data(EVP_CIPHER_CTX *ctx, void *data)
{
    ctx->app_data = data;
}

/**
 * Initiate the EVP_CIPHER_CTX context to encrypt or decrypt data.
 * Clean up with EVP_CIPHER_CTX_cleanup().
 *
 * @param ctx context to initiate
 * @param c cipher to use.
 * @param engine crypto engine to use, NULL to select default.
 * @param key the crypto key to use, NULL will use the previous value.
 * @param iv the IV to use, NULL will use the previous value.
 * @param encp non zero will encrypt, -1 use the previous value.
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_evp
 */

int
EVP_CipherInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *c, ENGINE *engine,
		  const void *key, const void *iv, int encp)
{
    ctx->buf_len = 0;

    if (encp == -1)
	encp = ctx->encrypt;
    else
	ctx->encrypt = (encp ? 1 : 0);

    if (c && (c != ctx->cipher)) {
	EVP_CIPHER_CTX_cleanup(ctx);
	ctx->cipher = c;
	ctx->key_len = c->key_len;

	ctx->cipher_data = malloc(c->ctx_size);
	if (ctx->cipher_data == NULL && c->ctx_size != 0)
	    return 0;

	/* assume block size is a multiple of 2 */
	ctx->block_mask = EVP_CIPHER_block_size(c) - 1;

    } else if (ctx->cipher == NULL) {
	/* reuse of cipher, but not any cipher ever set! */
	return 0;
    }

    switch (EVP_CIPHER_CTX_flags(ctx)) {
    case EVP_CIPH_CBC_MODE:

	assert(EVP_CIPHER_CTX_iv_length(ctx) <= sizeof(ctx->iv));

	if (iv)
	    memcpy(ctx->oiv, iv, EVP_CIPHER_CTX_iv_length(ctx));
	memcpy(ctx->iv, ctx->oiv, EVP_CIPHER_CTX_iv_length(ctx));
	break;
    default:
	return 0;
    }

    if (key || (ctx->cipher->flags & EVP_CIPH_ALWAYS_CALL_INIT))
	ctx->cipher->init(ctx, key, iv, encp);

    return 1;
}

/**
 * Encipher/decipher partial data
 *
 * @param ctx the cipher context.
 * @param out output data from the operation.
 * @param outlen output length
 * @param in input data to the operation.
 * @param inlen length of data.
 *
 * The output buffer length should at least be EVP_CIPHER_block_size()
 * byte longer then the input length.
 *
 * See @ref evp_cipher for an example how to use this function.
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_evp
 */

int
EVP_CipherUpdate(EVP_CIPHER_CTX *ctx, void *out, int *outlen,
		 void *in, size_t inlen)
{
    int ret, left, blocksize;

    *outlen = 0;

    /**
     * If there in no spare bytes in the left from last Update and the
     * input length is on the block boundery, the EVP_CipherUpdate()
     * function can take a shortcut (and preformance gain) and
     * directly encrypt the data, otherwise we hav to fix it up and
     * store extra it the EVP_CIPHER_CTX.
     */
    if (ctx->buf_len == 0 && (inlen & ctx->block_mask) == 0) {
	ret = (*ctx->cipher->do_cipher)(ctx, out, in, inlen);
	if (ret == 1)
	    *outlen = inlen;
	else
	    *outlen = 0;
	return ret;
    }


    blocksize = EVP_CIPHER_CTX_block_size(ctx);
    left = blocksize - ctx->buf_len;
    assert(left > 0);

    if (ctx->buf_len) {

	/* if total buffer is smaller then input, store locally */
	if (inlen < left) {
	    memcpy(ctx->buf + ctx->buf_len, in, inlen);
	    ctx->buf_len += inlen;
	    return 1;
	}
	
	/* fill in local buffer and encrypt */
	memcpy(ctx->buf + ctx->buf_len, in, left);
	ret = (*ctx->cipher->do_cipher)(ctx, out, ctx->buf, blocksize);
	memset(ctx->buf, 0, blocksize);
	if (ret != 1)
	    return ret;

	*outlen += blocksize;
	inlen -= left;
	in = ((unsigned char *)in) + left;
	out = ((unsigned char *)out) + blocksize;
	ctx->buf_len = 0;
    }

    if (inlen) {
	ctx->buf_len = (inlen & ctx->block_mask);
	inlen &= ~ctx->block_mask;
	
	ret = (*ctx->cipher->do_cipher)(ctx, out, in, inlen);
	if (ret != 1)
	    return ret;

	*outlen += inlen;

	in = ((unsigned char *)in) + inlen;
	memcpy(ctx->buf, in, ctx->buf_len);
    }

    return 1;
}

/**
 * Encipher/decipher final data
 *
 * @param ctx the cipher context.
 * @param out output data from the operation.
 * @param outlen output length
 *
 * The input length needs to be at least EVP_CIPHER_block_size() bytes
 * long.
 *
 * See @ref evp_cipher for an example how to use this function.
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_evp
 */

int
EVP_CipherFinal_ex(EVP_CIPHER_CTX *ctx, void *out, int *outlen)
{
    *outlen = 0;

    if (ctx->buf_len) {
	int ret, left, blocksize;

	blocksize = EVP_CIPHER_CTX_block_size(ctx);

	left = blocksize - ctx->buf_len;
	assert(left > 0);

	/* zero fill local buffer */
	memset(ctx->buf + ctx->buf_len, 0, left);
	ret = (*ctx->cipher->do_cipher)(ctx, out, ctx->buf, blocksize);
	memset(ctx->buf, 0, blocksize);
	if (ret != 1)
	    return ret;

	*outlen += blocksize;
    }

    return 1;
}

/**
 * Encipher/decipher data
 *
 * @param ctx the cipher context.
 * @param out out data from the operation.
 * @param in in data to the operation.
 * @param size length of data.
 *
 * @return 1 on success.
 */

int
EVP_Cipher(EVP_CIPHER_CTX *ctx, void *out, const void *in,size_t size)
{
    return ctx->cipher->do_cipher(ctx, out, in, size);
}

/*
 *
 */

static int
enc_null_init(EVP_CIPHER_CTX *ctx,
		  const unsigned char * key,
		  const unsigned char * iv,
		  int encp)
{
    return 1;
}

static int
enc_null_do_cipher(EVP_CIPHER_CTX *ctx,
	      unsigned char *out,
	      const unsigned char *in,
	      unsigned int size)
{
    memmove(out, in, size);
    return 1;
}

static int
enc_null_cleanup(EVP_CIPHER_CTX *ctx)
{
    return 1;
}

/**
 * The NULL cipher type, does no encryption/decryption.
 *
 * @return the null EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_enc_null(void)
{
    static const EVP_CIPHER enc_null = {
	0,
	0,
	0,
	0,
	EVP_CIPH_CBC_MODE,
	enc_null_init,
	enc_null_do_cipher,
	enc_null_cleanup,
	0,
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &enc_null;
}

/*
 *
 */

struct rc2_cbc {
    unsigned int maximum_effective_key;
    RC2_KEY key;
};

static int
rc2_init(EVP_CIPHER_CTX *ctx,
	 const unsigned char * key,
	 const unsigned char * iv,
	 int encp)
{
    struct rc2_cbc *k = ctx->cipher_data;
    k->maximum_effective_key = EVP_CIPHER_CTX_key_length(ctx) * 8;
    RC2_set_key(&k->key,
		EVP_CIPHER_CTX_key_length(ctx),
		key,
		k->maximum_effective_key);
    return 1;
}

static int
rc2_do_cipher(EVP_CIPHER_CTX *ctx,
	      unsigned char *out,
	      const unsigned char *in,
	      unsigned int size)
{
    struct rc2_cbc *k = ctx->cipher_data;
    RC2_cbc_encrypt(in, out, size, &k->key, ctx->iv, ctx->encrypt);
    return 1;
}

static int
rc2_cleanup(EVP_CIPHER_CTX *ctx)
{
    memset(ctx->cipher_data, 0, sizeof(struct rc2_cbc));
    return 1;
}

/**
 * The RC2 cipher type
 *
 * @return the RC2 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_rc2_cbc(void)
{
    static const EVP_CIPHER rc2_cbc = {
	0,
	RC2_BLOCK_SIZE,
	RC2_KEY_LENGTH,
	RC2_BLOCK_SIZE,
	EVP_CIPH_CBC_MODE,
	rc2_init,
	rc2_do_cipher,
	rc2_cleanup,
	sizeof(struct rc2_cbc),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &rc2_cbc;
}

/**
 * The RC2-40 cipher type
 *
 * @return the RC2-40 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_rc2_40_cbc(void)
{
    static const EVP_CIPHER rc2_40_cbc = {
	0,
	RC2_BLOCK_SIZE,
	5,
	RC2_BLOCK_SIZE,
	EVP_CIPH_CBC_MODE,
	rc2_init,
	rc2_do_cipher,
	rc2_cleanup,
	sizeof(struct rc2_cbc),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &rc2_40_cbc;
}

/**
 * The RC2-64 cipher type
 *
 * @return the RC2-64 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_rc2_64_cbc(void)
{
    static const EVP_CIPHER rc2_64_cbc = {
	0,
	RC2_BLOCK_SIZE,
	8,
	RC2_BLOCK_SIZE,
	EVP_CIPH_CBC_MODE,
	rc2_init,
	rc2_do_cipher,
	rc2_cleanup,
	sizeof(struct rc2_cbc),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &rc2_64_cbc;
}

/**
 * The RC4 cipher type
 *
 * @return the RC4 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_rc4(void)
{
    printf("evp rc4\n");
    abort();
    return NULL;
}

/**
 * The RC4-40 cipher type
 *
 * @return the RC4-40 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_rc4_40(void)
{
    printf("evp rc4_40\n");
    abort();
    return NULL;
}

/*
 *
 */

static int
des_cbc_init(EVP_CIPHER_CTX *ctx,
	     const unsigned char * key,
	     const unsigned char * iv,
	     int encp)
{
    DES_key_schedule *k = ctx->cipher_data;
    DES_cblock deskey;
    memcpy(&deskey, key, sizeof(deskey));
    DES_set_key_unchecked(&deskey, k);
    return 1;
}

static int
des_cbc_do_cipher(EVP_CIPHER_CTX *ctx,
		  unsigned char *out,
		  const unsigned char *in,
		  unsigned int size)
{
    DES_key_schedule *k = ctx->cipher_data;
    DES_cbc_encrypt(in, out, size,
		    k, (DES_cblock *)ctx->iv, ctx->encrypt);
    return 1;
}

static int
des_cbc_cleanup(EVP_CIPHER_CTX *ctx)
{
    memset(ctx->cipher_data, 0, sizeof(struct DES_key_schedule));
    return 1;
}

/**
 * The DES cipher type
 *
 * @return the DES-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_des_cbc(void)
{
    static const EVP_CIPHER des_ede3_cbc = {
	0,
	8,
	8,
	8,
	EVP_CIPH_CBC_MODE,
	des_cbc_init,
	des_cbc_do_cipher,
	des_cbc_cleanup,
	sizeof(DES_key_schedule),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &des_ede3_cbc;
}

/*
 *
 */

struct des_ede3_cbc {
    DES_key_schedule ks[3];
};

static int
des_ede3_cbc_init(EVP_CIPHER_CTX *ctx,
		  const unsigned char * key,
		  const unsigned char * iv,
		  int encp)
{
    struct des_ede3_cbc *k = ctx->cipher_data;
    DES_cblock deskey;

    memcpy(&deskey, key, sizeof(deskey));
    DES_set_odd_parity(&deskey);
    DES_set_key_unchecked(&deskey, &k->ks[0]);

    memcpy(&deskey, key + 8, sizeof(deskey));
    DES_set_odd_parity(&deskey);
    DES_set_key_unchecked(&deskey, &k->ks[1]);

    memcpy(&deskey, key + 16, sizeof(deskey));
    DES_set_odd_parity(&deskey);
    DES_set_key_unchecked(&deskey, &k->ks[2]);

    return 1;
}

static int
des_ede3_cbc_do_cipher(EVP_CIPHER_CTX *ctx,
		       unsigned char *out,
		       const unsigned char *in,
		       unsigned int size)
{
    struct des_ede3_cbc *k = ctx->cipher_data;
    DES_ede3_cbc_encrypt(in, out, size,
			 &k->ks[0], &k->ks[1], &k->ks[2],
			 (DES_cblock *)ctx->iv, ctx->encrypt);
    return 1;
}

static int
des_ede3_cbc_cleanup(EVP_CIPHER_CTX *ctx)
{
    memset(ctx->cipher_data, 0, sizeof(struct des_ede3_cbc));
    return 1;
}

/**
 * The tripple DES cipher type
 *
 * @return the DES-EDE3-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_des_ede3_cbc(void)
{
    static const EVP_CIPHER des_ede3_cbc = {
	0,
	8,
	24,
	8,
	EVP_CIPH_CBC_MODE,
	des_ede3_cbc_init,
	des_ede3_cbc_do_cipher,
	des_ede3_cbc_cleanup,
	sizeof(struct des_ede3_cbc),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &des_ede3_cbc;
}

/**
 * The AES-128 cipher type
 *
 * @return the AES-128 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_aes_128_cbc(void)
{
    return EVP_hcrypto_aes_128_cbc();
}

/**
 * The AES-192 cipher type
 *
 * @return the AES-192 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_aes_192_cbc(void)
{
    return EVP_hcrypto_aes_192_cbc();
}

/**
 * The AES-256 cipher type
 *
 * @return the AES-256 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_aes_256_cbc(void)
{
    return EVP_hcrypto_aes_256_cbc();
}

static int
camellia_init(EVP_CIPHER_CTX *ctx,
	 const unsigned char * key,
	 const unsigned char * iv,
	 int encp)
{
    CAMELLIA_KEY *k = ctx->cipher_data;
    k->bits = ctx->cipher->key_len * 8;
    CAMELLIA_set_key(key, ctx->cipher->key_len * 8, k);
    return 1;
}

static int
camellia_do_cipher(EVP_CIPHER_CTX *ctx,
	      unsigned char *out,
	      const unsigned char *in,
	      unsigned int size)
{
    CAMELLIA_KEY *k = ctx->cipher_data;
    CAMELLIA_cbc_encrypt(in, out, size, k, ctx->iv, ctx->encrypt);
    return 1;
}

static int
camellia_cleanup(EVP_CIPHER_CTX *ctx)
{
    memset(ctx->cipher_data, 0, sizeof(CAMELLIA_KEY));
    return 1;
}

/**
 * The Camellia-128 cipher type
 *
 * @return the Camellia-128 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_camellia_128_cbc(void)
{
    static const EVP_CIPHER cipher = {
	0,
	16,
	16,
	16,
	EVP_CIPH_CBC_MODE,
	camellia_init,
	camellia_do_cipher,
	camellia_cleanup,
	sizeof(CAMELLIA_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &cipher;
}

/**
 * The Camellia-198 cipher type
 *
 * @return the Camellia-198 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_camellia_192_cbc(void)
{
    static const EVP_CIPHER cipher = {
	0,
	16,
	24,
	16,
	EVP_CIPH_CBC_MODE,
	camellia_init,
	camellia_do_cipher,
	camellia_cleanup,
	sizeof(CAMELLIA_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &cipher;
}

/**
 * The Camellia-256 cipher type
 *
 * @return the Camellia-256 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_camellia_256_cbc(void)
{
    static const EVP_CIPHER cipher = {
	0,
	16,
	32,
	16,
	EVP_CIPH_CBC_MODE,
	camellia_init,
	camellia_do_cipher,
	camellia_cleanup,
	sizeof(CAMELLIA_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &cipher;
}

/*
 *
 */

static const struct cipher_name {
    const char *name;
    const EVP_CIPHER *(*func)(void);
} cipher_name[] = {
    { "des-ede3-cbc", EVP_des_ede3_cbc },
    { "aes-128-cbc", EVP_aes_128_cbc },
    { "aes-192-cbc", EVP_aes_192_cbc },
    { "aes-256-cbc", EVP_aes_256_cbc },
    { "camellia-128-cbc", EVP_camellia_128_cbc },
    { "camellia-192-cbc", EVP_camellia_192_cbc },
    { "camellia-256-cbc", EVP_camellia_256_cbc }
};

/**
 * Get the cipher type using their name.
 *
 * @param name the name of the cipher.
 *
 * @return the selected EVP_CIPHER pointer or NULL if not found.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_get_cipherbyname(const char *name)
{
    int i;
    for (i = 0; i < sizeof(cipher_name)/sizeof(cipher_name[0]); i++) {
	if (strcasecmp(cipher_name[i].name, name) == 0)
	    return (*cipher_name[i].func)();
    }
    return NULL;
}


/*
 *
 */

#ifndef min
#define min(a,b) (((a)>(b))?(b):(a))
#endif

/**
 * Provides a legancy string to key function, used in PEM files.
 *
 * New protocols should use new string to key functions like NIST
 * SP56-800A or PKCS#5 v2.0 (see PKCS5_PBKDF2_HMAC_SHA1()).
 *
 * @param type type of cipher to use
 * @param md message digest to use
 * @param salt salt salt string, should be an binary 8 byte buffer.
 * @param data the password/input key string.
 * @param datalen length of data parameter.
 * @param count iteration counter.
 * @param keydata output keydata, needs to of the size EVP_CIPHER_key_length().
 * @param ivdata output ivdata, needs to of the size EVP_CIPHER_block_size().
 *
 * @return the size of derived key.
 *
 * @ingroup hcrypto_evp
 */

int
EVP_BytesToKey(const EVP_CIPHER *type,
	       const EVP_MD *md,
	       const void *salt,
	       const void *data, size_t datalen,
	       unsigned int count,
	       void *keydata,
	       void *ivdata)
{
    int ivlen, keylen, first = 0;
    unsigned int mds = 0, i;
    unsigned char *key = keydata;
    unsigned char *iv = ivdata;
    unsigned char *buf;
    EVP_MD_CTX c;

    keylen = EVP_CIPHER_key_length(type);
    ivlen = EVP_CIPHER_iv_length(type);

    if (data == NULL)
	return keylen;

    buf = malloc(EVP_MD_size(md));
    if (buf == NULL)
	return -1;

    EVP_MD_CTX_init(&c);

    first = 1;
    while (1) {
	EVP_DigestInit_ex(&c, md, NULL);
	if (!first)
	    EVP_DigestUpdate(&c, buf, mds);
	first = 0;
	EVP_DigestUpdate(&c,data,datalen);

#define PKCS5_SALT_LEN 8

	if (salt)
	    EVP_DigestUpdate(&c, salt, PKCS5_SALT_LEN);

	EVP_DigestFinal_ex(&c, buf, &mds);
	assert(mds == EVP_MD_size(md));

	for (i = 1; i < count; i++) {
	    EVP_DigestInit_ex(&c, md, NULL);
	    EVP_DigestUpdate(&c, buf, mds);
	    EVP_DigestFinal_ex(&c, buf, &mds);
	    assert(mds == EVP_MD_size(md));
	}

	i = 0;
	if (keylen) {
	    size_t sz = min(keylen, mds);
	    if (key) {
		memcpy(key, buf, sz);
		key += sz;
	    }
	    keylen -= sz;
	    i += sz;
	}
	if (ivlen && mds > i) {
	    size_t sz = min(ivlen, (mds - i));
	    if (iv) {
		memcpy(iv, &buf[i], sz);
		iv += sz;
	    }
	    ivlen -= sz;
	}
	if (keylen == 0 && ivlen == 0)
	    break;
    }

    EVP_MD_CTX_cleanup(&c);
    free(buf);

    return EVP_CIPHER_key_length(type);
}

/**
 * Generate a random key for the specificed EVP_CIPHER.
 *
 * @param ctx EVP_CIPHER_CTX type to build the key for.
 * @param key return key, must be at least EVP_CIPHER_key_length() byte long.
 *
 * @return 1 for success, 0 for failure.
 *
 * @ingroup hcrypto_core
 */

int
EVP_CIPHER_CTX_rand_key(EVP_CIPHER_CTX *ctx, void *key)
{
    if (ctx->cipher->flags & EVP_CIPH_RAND_KEY)
	return EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_RAND_KEY, 0, key);
    if (RAND_bytes(key, ctx->key_len) != 1)
	return 0;
    return 1;
}

/**
 * Perform a operation on a ctx
 *
 * @param ctx context to perform operation on.
 * @param type type of operation.
 * @param arg argument to operation.
 * @param data addition data to operation.

 * @return 1 for success, 0 for failure.
 *
 * @ingroup hcrypto_core
 */

int
EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *data)
{
    if (ctx->cipher == NULL || ctx->cipher->ctrl == NULL)
	return 0;
    return (*ctx->cipher->ctrl)(ctx, type, arg, data);
}

/**
 * Add all algorithms to the crypto core.
 *
 * @ingroup hcrypto_core
 */

void
OpenSSL_add_all_algorithms(void)
{
    return;
}

/**
 * Add all algorithms to the crypto core using configuration file.
 *
 * @ingroup hcrypto_core
 */

void
OpenSSL_add_all_algorithms_conf(void)
{
    return;
}

/**
 * Add all algorithms to the crypto core, but don't use the
 * configuration file.
 *
 * @ingroup hcrypto_core
 */

void
OpenSSL_add_all_algorithms_noconf(void)
{
    return;
}
