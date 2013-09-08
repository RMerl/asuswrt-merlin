/*
 * Wrap functions for WPS to adapt to OpenSSL crypto library
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_openssl.c 241182 2011-02-17 21:50:03Z $
 */

#ifdef EXTERNAL_OPENSSL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wps_openssl.h>
#include <sha.h>
#include <aes.h>


/*
 * Pad the length at last block,
 * Modify from OpenSSL AES_cbc_encrypt() in aes_cbc.c
 */
static int
AES_cbc_encrypt_pad(const unsigned char *in, unsigned char *out,
	const unsigned long length, const AES_KEY *key,
	unsigned char *ivec, const int enc) {

	unsigned long n;
	unsigned long len = length;
	unsigned long out_len = length;
	unsigned char tmp[AES_BLOCK_SIZE];
	const unsigned char *iv = ivec;

	if (!in || !out || !key || !ivec)
		return -1;
	if (enc != AES_ENCRYPT && enc != AES_DECRYPT)
		return -1;

	if (AES_ENCRYPT == enc) {
		while (len >= AES_BLOCK_SIZE) {
			for (n = 0; n < AES_BLOCK_SIZE; ++n)
				out[n] = in[n] ^ iv[n];
			AES_encrypt(out, out, key);
			iv = out;
			len -= AES_BLOCK_SIZE;
			in += AES_BLOCK_SIZE;
			out += AES_BLOCK_SIZE;
		}

		if (len) {
			for (n = 0; n < len; ++n)
				out[n] = in[n] ^ iv[n];
		}

		/* Pad length */
		for (n = len; n < AES_BLOCK_SIZE; n++)
			out[n] = (AES_BLOCK_SIZE - len) ^  iv[n];
		AES_encrypt(out, out, key);
		iv = out;

		/* Calculate out length */
		out_len = out_len - len + AES_BLOCK_SIZE;
	} else if (in != out) {
		while (len >= AES_BLOCK_SIZE) {
			AES_decrypt(in, out, key);
			for (n = 0; n < AES_BLOCK_SIZE; ++n)
				out[n] ^= iv[n];
			iv = in;
			len -= AES_BLOCK_SIZE;
			in  += AES_BLOCK_SIZE;
			out += AES_BLOCK_SIZE;
		}

		if (len) {
			AES_decrypt(in, tmp, key);
			for (n = 0; n < len; ++n)
				out[n] = tmp[n] ^ iv[n];
			iv = in;
		} else {
			/* Calculate out length */
			out_len -= *(out - 1);
		}
	} else {
		unsigned char tmp_ivec[AES_BLOCK_SIZE];

		/* Copy ivec to tmp_ivec */
		memcpy(tmp_ivec, ivec, AES_BLOCK_SIZE);

		while (len >= AES_BLOCK_SIZE) {
			memcpy(tmp, in, AES_BLOCK_SIZE);
			AES_decrypt(in, out, key);
			for (n = 0; n < AES_BLOCK_SIZE; ++n)
				out[n] ^= tmp_ivec[n];
			memcpy(tmp_ivec, tmp, AES_BLOCK_SIZE);
			len -= AES_BLOCK_SIZE;
			in += AES_BLOCK_SIZE;
			out += AES_BLOCK_SIZE;
		}
		if (len) {
			memcpy(tmp, in, AES_BLOCK_SIZE);
			AES_decrypt(tmp, out, key);
			for (n = 0; n < len; ++n)
				out[n] ^= tmp_ivec[n];
			for (n = len; n < AES_BLOCK_SIZE; ++n)
				out[n] = tmp[n];
		} else {
			/* Calculate out length */
			out_len -= *(out - 1);
		}
	}

	/* Return encrypt or decrypt text length */
	return out_len;
}

void
RAND_linux_init()
{
	return;
}

void
hmac_sha256(const void *key, int key_len, const unsigned char *text, size_t text_len,
	unsigned char *digest, unsigned int *digest_len)

{
	SHA256_CTX ctx;

	int i;
	unsigned char sha_key[SHA256_CBLOCK];
	unsigned char k_ipad[SHA256_CBLOCK];    /* inner padding -
						 * key XORd with ipad
						 */
	unsigned char k_opad[SHA256_CBLOCK];    /* outer padding -
						 * key XORd with opad
						 */
	/* set the key  */
	/* block size smaller than key size : hash down */
	if (SHA256_CBLOCK < key_len) {
		SHA256_Init(&ctx);
		SHA256_Update(&ctx, key, key_len);
		SHA256_Final(sha_key, &ctx);
		key = sha_key;
		key_len = SHA256_DIGEST_LENGTH;
	}

	/*
	 * the HMAC_SHA256 transform looks like:
	 *
	 * SHA256(K XOR opad, SHA256(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */
	/* compute inner and outer pads from key */
	memset(k_ipad, 0, sizeof(k_ipad));
	memset(k_opad, 0, sizeof(k_opad));
	memcpy(k_ipad, key, key_len);
	memcpy(k_opad, key, key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	/*
	 * perform inner SHA256
	 */
	SHA256_Init(&ctx);                   /* init context for 1st pass */
	SHA256_Update(&ctx, k_ipad, SHA256_CBLOCK);     /* start with inner pad */
	SHA256_Update(&ctx, text, text_len); /* then text of datagram */
	SHA256_Final(digest, &ctx);          /* finish up 1st pass */

	/*
	 * perform outer SHA256
	 */
	SHA256_Init(&ctx);                   /* init context for 2nd pass */
	SHA256_Update(&ctx, k_opad, SHA256_CBLOCK); /* start with outer pad */
	SHA256_Update(&ctx, digest, SHA256_DIGEST_LENGTH); /* then results of 1st hash */
	SHA256_Final(digest, &ctx);          /* finish up 2nd pass */

	if (digest_len)
		*digest_len = SHA256_DIGEST_LENGTH;
}

int
rijndaelKeySetupEnc(uint32 rk[], const uint8 cipherKey[], int keyBits)
{
	int i, ret;
	AES_KEY key;

	memset((char *)&key, 0, sizeof(key));

	ret = AES_set_encrypt_key(cipherKey, keyBits, &key);
	if (ret < 0) {
		printf("rijndaelKeySetupEnc() : AES_set_encrypt_key failed %d\n", ret);
		return ret; /* zero rounds ? */
	}

	for (i = 0; i < (4 * (key.rounds + 1)); i++)
		rk[i] = key.rd_key[i];

	return key.rounds;
}

int
rijndaelKeySetupDec(uint32 rk[], const uint8 cipherKey[], int keyBits)
{
	int i, ret;
	AES_KEY key;

	memset((char *)&key, 0, sizeof(key));

	ret = AES_set_decrypt_key(cipherKey, keyBits, &key);
	if (ret < 0) {
		printf("rijndaelKeySetupDec() :  AES_set_decrypt_key failed %d\n", ret);
		return ret; /* zero rounds ? */
	}

	for (i = 0; i < (4 * (key.rounds + 1)); i++)
		rk[i] = key.rd_key[i];

	return key.rounds;
}

int
aes_cbc_encrypt_pad(uint32 *rk, const size_t key_len, const uint8 *nonce,
	const size_t data_len, const uint8 *ptxt, uint8 *ctxt, uint8 padd_type)
{
	int i, encrypt_len;
	AES_KEY key;

	 /* Compiler reference to avoid unused variable warning */
	(void)(padd_type);

	memset((char *)&key, 0, sizeof(key));

	/* Prepare key structure */
	key.rounds = ((key_len * 8) / 32) + 6;
	for (i = 0; i < (4 * (key.rounds + 1)); i++)
		key.rd_key[i] = rk[i];

	encrypt_len = AES_cbc_encrypt_pad(ptxt, ctxt, data_len, &key, (unsigned char*)nonce,
		AES_ENCRYPT);

	return encrypt_len;
}

int
aes_cbc_decrypt_pad(uint32 *rk, const size_t key_len, const uint8 *nonce,
	const size_t data_len, const uint8 *ctxt, uint8 *ptxt, uint8 padd_type)
{
	int i, plaintext_len;
	AES_KEY key;

	 /* Compiler reference to avoid unused variable warning */
	(void)(padd_type);

	memset((char *)&key, 0, sizeof(key));

	/* Prepare key structure */
	key.rounds = ((key_len * 8) / 32) + 6;
	for (i = 0; i < (4 * (key.rounds + 1)); i++)
		key.rd_key[i] = rk[i];

	plaintext_len = AES_cbc_encrypt_pad(ctxt, ptxt, data_len, &key, (unsigned char*)nonce,
		AES_DECRYPT);

	return plaintext_len;
}
#endif /* EXTERNAL_OPENSSL */
