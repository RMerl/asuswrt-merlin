/*
 * Dropbear SSH
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * Copyright (c) 2004 by Mihnea Stoenescu
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "algo.h"
#include "dbutil.h"

/* This file (algo.c) organises the ciphers which can be used, and is used to
 * decide which ciphers/hashes/compression/signing to use during key exchange*/

static int void_cipher(const unsigned char* in, unsigned char* out,
		unsigned long len, void *cipher_state) {
	if (in != out) {
		memmove(out, in, len);
	}
	return CRYPT_OK;
}

static int void_start(int cipher, const unsigned char *IV, 
			const unsigned char *key, 
			int keylen, int num_rounds, void *cipher_state) {
	return CRYPT_OK;
}

/* Mappings for ciphers, parameters are
   {&cipher_desc, keysize, blocksize} */
/* NOTE: if keysize > 2*SHA1_HASH_SIZE, code such as hashkeys()
   needs revisiting */

#ifdef DROPBEAR_AES256
static const struct dropbear_cipher dropbear_aes256 = 
	{&aes_desc, 32, 16};
#endif
#ifdef DROPBEAR_AES128
static const struct dropbear_cipher dropbear_aes128 = 
	{&aes_desc, 16, 16};
#endif
#ifdef DROPBEAR_BLOWFISH
static const struct dropbear_cipher dropbear_blowfish = 
	{&blowfish_desc, 16, 8};
#endif
#ifdef DROPBEAR_TWOFISH256
static const struct dropbear_cipher dropbear_twofish256 = 
	{&twofish_desc, 32, 16};
#endif
#ifdef DROPBEAR_TWOFISH128
static const struct dropbear_cipher dropbear_twofish128 = 
	{&twofish_desc, 16, 16};
#endif
#ifdef DROPBEAR_3DES
static const struct dropbear_cipher dropbear_3des = 
	{&des3_desc, 24, 8};
#endif

/* used to indicate no encryption, as defined in rfc2410 */
const struct dropbear_cipher dropbear_nocipher =
	{NULL, 16, 8}; 

/* A few void* s are required to silence warnings
 * about the symmetric_CBC vs symmetric_CTR cipher_state pointer */
const struct dropbear_cipher_mode dropbear_mode_cbc =
	{(void*)cbc_start, (void*)cbc_encrypt, (void*)cbc_decrypt};
const struct dropbear_cipher_mode dropbear_mode_none =
	{void_start, void_cipher, void_cipher};
#ifdef DROPBEAR_ENABLE_CTR_MODE
/* a wrapper to make ctr_start and cbc_start look the same */
static int dropbear_big_endian_ctr_start(int cipher, 
		const unsigned char *IV, 
		const unsigned char *key, int keylen, 
		int num_rounds, symmetric_CTR *ctr) {
	return ctr_start(cipher, IV, key, keylen, num_rounds, CTR_COUNTER_BIG_ENDIAN, ctr);
}
const struct dropbear_cipher_mode dropbear_mode_ctr =
	{(void*)dropbear_big_endian_ctr_start, (void*)ctr_encrypt, (void*)ctr_decrypt};
#endif

/* Mapping of ssh hashes to libtomcrypt hashes, including keysize etc.
   {&hash_desc, keysize, hashsize} */

#ifdef DROPBEAR_SHA1_HMAC
static const struct dropbear_hash dropbear_sha1 = 
	{&sha1_desc, 20, 20};
#endif
#ifdef DROPBEAR_SHA1_96_HMAC
static const struct dropbear_hash dropbear_sha1_96 = 
	{&sha1_desc, 20, 12};
#endif
#ifdef DROPBEAR_MD5_HMAC
static const struct dropbear_hash dropbear_md5 = 
	{&md5_desc, 16, 16};
#endif

const struct dropbear_hash dropbear_nohash =
	{NULL, 16, 0}; /* used initially */
	

/* The following map ssh names to internal values.
 * The ordering here is important for the client - the first mode
 * that is also supported by the server will get used. */

algo_type sshciphers[] = {
#ifdef DROPBEAR_ENABLE_CTR_MODE
#ifdef DROPBEAR_AES128
	{"aes128-ctr", 0, &dropbear_aes128, 1, &dropbear_mode_ctr},
#endif
#ifdef DROPBEAR_3DES
	{"3des-ctr", 0, &dropbear_3des, 1, &dropbear_mode_ctr},
#endif
#ifdef DROPBEAR_AES256
	{"aes256-ctr", 0, &dropbear_aes256, 1, &dropbear_mode_ctr},
#endif
#endif /* DROPBEAR_ENABLE_CTR_MODE */

/* CBC modes are always enabled */
#ifdef DROPBEAR_AES128
	{"aes128-cbc", 0, &dropbear_aes128, 1, &dropbear_mode_cbc},
#endif
#ifdef DROPBEAR_3DES
	{"3des-cbc", 0, &dropbear_3des, 1, &dropbear_mode_cbc},
#endif
#ifdef DROPBEAR_AES256
	{"aes256-cbc", 0, &dropbear_aes256, 1, &dropbear_mode_cbc},
#endif
#ifdef DROPBEAR_TWOFISH256
	{"twofish256-cbc", 0, &dropbear_twofish256, 1, &dropbear_mode_cbc},
	{"twofish-cbc", 0, &dropbear_twofish256, 1, &dropbear_mode_cbc},
#endif
#ifdef DROPBEAR_TWOFISH128
	{"twofish128-cbc", 0, &dropbear_twofish128, 1, &dropbear_mode_cbc},
#endif
#ifdef DROPBEAR_BLOWFISH
	{"blowfish-cbc", 0, &dropbear_blowfish, 1, &dropbear_mode_cbc},
#endif
	{NULL, 0, NULL, 0, NULL}
};

algo_type sshhashes[] = {
#ifdef DROPBEAR_SHA1_96_HMAC
	{"hmac-sha1-96", 0, &dropbear_sha1_96, 1, NULL},
#endif
#ifdef DROPBEAR_SHA1_HMAC
	{"hmac-sha1", 0, &dropbear_sha1, 1, NULL},
#endif
#ifdef DROPBEAR_MD5_HMAC
	{"hmac-md5", 0, &dropbear_md5, 1, NULL},
#endif
	{NULL, 0, NULL, 0, NULL}
};

#ifndef DISABLE_ZLIB
algo_type ssh_compress[] = {
	{"zlib", DROPBEAR_COMP_ZLIB, NULL, 1, NULL},
	{"zlib@openssh.com", DROPBEAR_COMP_ZLIB_DELAY, NULL, 1, NULL},
	{"none", DROPBEAR_COMP_NONE, NULL, 1, NULL},
	{NULL, 0, NULL, 0, NULL}
};
#endif

algo_type ssh_nocompress[] = {
	{"none", DROPBEAR_COMP_NONE, NULL, 1, NULL},
	{NULL, 0, NULL, 0, NULL}
};

algo_type sshhostkey[] = {
#ifdef DROPBEAR_RSA
	{"ssh-rsa", DROPBEAR_SIGNKEY_RSA, NULL, 1, NULL},
#endif
#ifdef DROPBEAR_DSS
	{"ssh-dss", DROPBEAR_SIGNKEY_DSS, NULL, 1, NULL},
#endif
	{NULL, 0, NULL, 0, NULL}
};

algo_type sshkex[] = {
	{"diffie-hellman-group1-sha1", DROPBEAR_KEX_DH_GROUP1, NULL, 1, NULL},
	{"diffie-hellman-group14-sha1", DROPBEAR_KEX_DH_GROUP14, NULL, 1, NULL},
	{NULL, 0, NULL, 0, NULL}
};


/* Register the compiled in ciphers.
 * This should be run before using any of the ciphers/hashes */
void crypto_init() {

	const struct ltc_cipher_descriptor *regciphers[] = {
#ifdef DROPBEAR_AES
		&aes_desc,
#endif
#ifdef DROPBEAR_BLOWFISH
		&blowfish_desc,
#endif
#ifdef DROPBEAR_TWOFISH
		&twofish_desc,
#endif
#ifdef DROPBEAR_3DES
		&des3_desc,
#endif
		NULL
	};

	const struct ltc_hash_descriptor *reghashes[] = {
		/* we need sha1 for hostkey stuff regardless */
		&sha1_desc,
#ifdef DROPBEAR_MD5_HMAC
		&md5_desc,
#endif
		NULL
	};	
	int i;
	
	for (i = 0; regciphers[i] != NULL; i++) {
		if (register_cipher(regciphers[i]) == -1) {
			dropbear_exit("Error registering crypto");
		}
	}

	for (i = 0; reghashes[i] != NULL; i++) {
		if (register_hash(reghashes[i]) == -1) {
			dropbear_exit("Error registering crypto");
		}
	}
}

/* algolen specifies the length of algo, algos is our local list to match
 * against.
 * Returns DROPBEAR_SUCCESS if we have a match for algo, DROPBEAR_FAILURE
 * otherwise */
int have_algo(char* algo, size_t algolen, algo_type algos[]) {

	int i;

	for (i = 0; algos[i].name != NULL; i++) {
		if (strlen(algos[i].name) == algolen
				&& (strncmp(algos[i].name, algo, algolen) == 0)) {
			return DROPBEAR_SUCCESS;
		}
	}

	return DROPBEAR_FAILURE;
}



/* Output a comma separated list of algorithms to a buffer */
void buf_put_algolist(buffer * buf, algo_type localalgos[]) {

	unsigned int i, len;
	unsigned int donefirst = 0;
	buffer *algolist = NULL;

	algolist = buf_new(160);
	for (i = 0; localalgos[i].name != NULL; i++) {
		if (localalgos[i].usable) {
			if (donefirst)
				buf_putbyte(algolist, ',');
			donefirst = 1;
			len = strlen(localalgos[i].name);
			buf_putbytes(algolist, localalgos[i].name, len);
		}
	}
	buf_putstring(buf, algolist->data, algolist->len);
	buf_free(algolist);
}
