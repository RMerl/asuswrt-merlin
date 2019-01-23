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

#include "includes.h"
#include "algo.h"
#include "session.h"
#include "dbutil.h"
#include "dh_groups.h"
#include "ltc_prng.h"
#include "ecc.h"

/* This file (algo.c) organises the ciphers which can be used, and is used to
 * decide which ciphers/hashes/compression/signing to use during key exchange*/

static int void_cipher(const unsigned char* in, unsigned char* out,
		unsigned long len, void* UNUSED(cipher_state)) {
	if (in != out) {
		memmove(out, in, len);
	}
	return CRYPT_OK;
}

static int void_start(int UNUSED(cipher), const unsigned char* UNUSED(IV), 
			const unsigned char* UNUSED(key), 
			int UNUSED(keylen), int UNUSED(num_rounds), void* UNUSED(cipher_state)) {
	return CRYPT_OK;
}

/* Mappings for ciphers, parameters are
   {&cipher_desc, keysize, blocksize} */

/* Remember to add new ciphers/hashes to regciphers/reghashes too */

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
#ifdef DROPBEAR_ENABLE_CBC_MODE
const struct dropbear_cipher_mode dropbear_mode_cbc =
	{(void*)cbc_start, (void*)cbc_encrypt, (void*)cbc_decrypt};
#endif /* DROPBEAR_ENABLE_CBC_MODE */

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
#endif /* DROPBEAR_ENABLE_CTR_MODE */

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
#ifdef DROPBEAR_SHA2_256_HMAC
static const struct dropbear_hash dropbear_sha2_256 = 
	{&sha256_desc, 32, 32};
#endif
#ifdef DROPBEAR_SHA2_512_HMAC
static const struct dropbear_hash dropbear_sha2_512 =
	{&sha512_desc, 64, 64};
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
#ifdef DROPBEAR_AES256
	{"aes256-ctr", 0, &dropbear_aes256, 1, &dropbear_mode_ctr},
#endif
#ifdef DROPBEAR_TWOFISH_CTR
/* twofish ctr is conditional as it hasn't been tested for interoperability, see options.h */
#ifdef DROPBEAR_TWOFISH256
	{"twofish256-ctr", 0, &dropbear_twofish256, 1, &dropbear_mode_ctr},
#endif
#ifdef DROPBEAR_TWOFISH128
	{"twofish128-ctr", 0, &dropbear_twofish128, 1, &dropbear_mode_ctr},
#endif
#endif /* DROPBEAR_TWOFISH_CTR */
#endif /* DROPBEAR_ENABLE_CTR_MODE */

#ifdef DROPBEAR_ENABLE_CBC_MODE
#ifdef DROPBEAR_AES128
	{"aes128-cbc", 0, &dropbear_aes128, 1, &dropbear_mode_cbc},
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
#ifdef DROPBEAR_3DES
	{"3des-ctr", 0, &dropbear_3des, 1, &dropbear_mode_ctr},
#endif
#ifdef DROPBEAR_3DES
	{"3des-cbc", 0, &dropbear_3des, 1, &dropbear_mode_cbc},
#endif
#ifdef DROPBEAR_BLOWFISH
	{"blowfish-cbc", 0, &dropbear_blowfish, 1, &dropbear_mode_cbc},
#endif
#endif /* DROPBEAR_ENABLE_CBC_MODE */
#ifdef DROPBEAR_NONE_CIPHER
	{"none", 0, (void*)&dropbear_nocipher, 1, &dropbear_mode_none},
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
#ifdef DROPBEAR_SHA2_256_HMAC
	{"hmac-sha2-256", 0, &dropbear_sha2_256, 1, NULL},
#endif
#ifdef DROPBEAR_SHA2_512_HMAC
	{"hmac-sha2-512", 0, &dropbear_sha2_512, 1, NULL},
#endif
#ifdef DROPBEAR_MD5_HMAC
	{"hmac-md5", 0, (void*)&dropbear_md5, 1, NULL},
#endif
#ifdef DROPBEAR_NONE_INTEGRITY
	{"none", 0, (void*)&dropbear_nohash, 1, NULL},
#endif
	{NULL, 0, NULL, 0, NULL}
};

#ifndef DISABLE_ZLIB
algo_type ssh_compress[] = {
	{"zlib@openssh.com", DROPBEAR_COMP_ZLIB_DELAY, NULL, 1, NULL},
	{"zlib", DROPBEAR_COMP_ZLIB, NULL, 1, NULL},
	{"none", DROPBEAR_COMP_NONE, NULL, 1, NULL},
	{NULL, 0, NULL, 0, NULL}
};

algo_type ssh_delaycompress[] = {
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
#ifdef DROPBEAR_ECDSA
#ifdef DROPBEAR_ECC_256
	{"ecdsa-sha2-nistp256", DROPBEAR_SIGNKEY_ECDSA_NISTP256, NULL, 1, NULL},
#endif
#ifdef DROPBEAR_ECC_384
	{"ecdsa-sha2-nistp384", DROPBEAR_SIGNKEY_ECDSA_NISTP384, NULL, 1, NULL},
#endif
#ifdef DROPBEAR_ECC_521
	{"ecdsa-sha2-nistp521", DROPBEAR_SIGNKEY_ECDSA_NISTP521, NULL, 1, NULL},
#endif
#endif
#ifdef DROPBEAR_RSA
	{"ssh-rsa", DROPBEAR_SIGNKEY_RSA, NULL, 1, NULL},
#endif
#ifdef DROPBEAR_DSS
	{"ssh-dss", DROPBEAR_SIGNKEY_DSS, NULL, 1, NULL},
#endif
	{NULL, 0, NULL, 0, NULL}
};

#if DROPBEAR_DH_GROUP1
static const struct dropbear_kex kex_dh_group1 = {DROPBEAR_KEX_NORMAL_DH, dh_p_1, DH_P_1_LEN, NULL, &sha1_desc };
#endif
#if DROPBEAR_DH_GROUP14
static const struct dropbear_kex kex_dh_group14_sha1 = {DROPBEAR_KEX_NORMAL_DH, dh_p_14, DH_P_14_LEN, NULL, &sha1_desc };
#if DROPBEAR_DH_GROUP14_256
static const struct dropbear_kex kex_dh_group14_sha256 = {DROPBEAR_KEX_NORMAL_DH, dh_p_14, DH_P_14_LEN, NULL, &sha256_desc };
#endif
#endif
#if DROPBEAR_DH_GROUP16
static const struct dropbear_kex kex_dh_group16_sha512 = {DROPBEAR_KEX_NORMAL_DH, dh_p_16, DH_P_16_LEN, NULL, &sha512_desc };
#endif

/* These can't be const since dropbear_ecc_fill_dp() fills out
 ecc_curve at runtime */
#ifdef DROPBEAR_ECDH
#ifdef DROPBEAR_ECC_256
static const struct dropbear_kex kex_ecdh_nistp256 = {DROPBEAR_KEX_ECDH, NULL, 0, &ecc_curve_nistp256, &sha256_desc };
#endif
#ifdef DROPBEAR_ECC_384
static const struct dropbear_kex kex_ecdh_nistp384 = {DROPBEAR_KEX_ECDH, NULL, 0, &ecc_curve_nistp384, &sha384_desc };
#endif
#ifdef DROPBEAR_ECC_521
static const struct dropbear_kex kex_ecdh_nistp521 = {DROPBEAR_KEX_ECDH, NULL, 0, &ecc_curve_nistp521, &sha512_desc };
#endif
#endif /* DROPBEAR_ECDH */

#ifdef DROPBEAR_CURVE25519
/* Referred to directly */
static const struct dropbear_kex kex_curve25519 = {DROPBEAR_KEX_CURVE25519, NULL, 0, NULL, &sha256_desc };
#endif

algo_type sshkex[] = {
#ifdef DROPBEAR_CURVE25519
	{"curve25519-sha256@libssh.org", 0, &kex_curve25519, 1, NULL},
#endif
#ifdef DROPBEAR_ECDH
#ifdef DROPBEAR_ECC_521
	{"ecdh-sha2-nistp521", 0, &kex_ecdh_nistp521, 1, NULL},
#endif
#ifdef DROPBEAR_ECC_384
	{"ecdh-sha2-nistp384", 0, &kex_ecdh_nistp384, 1, NULL},
#endif
#ifdef DROPBEAR_ECC_256
	{"ecdh-sha2-nistp256", 0, &kex_ecdh_nistp256, 1, NULL},
#endif
#endif
#if DROPBEAR_DH_GROUP14
#if DROPBEAR_DH_GROUP14_256
	{"diffie-hellman-group14-sha256", 0, &kex_dh_group14_sha256, 1, NULL},
#endif
	{"diffie-hellman-group14-sha1", 0, &kex_dh_group14_sha1, 1, NULL},
#endif
#if DROPBEAR_DH_GROUP1
	{"diffie-hellman-group1-sha1", 0, &kex_dh_group1, 1, NULL},
#endif
#if DROPBEAR_DH_GROUP16
	{"diffie-hellman-group16-sha512", 0, &kex_dh_group16_sha512, 1, NULL},
#endif
#ifdef USE_KEXGUESS2
	{KEXGUESS2_ALGO_NAME, KEXGUESS2_ALGO_ID, NULL, 1, NULL},
#endif
	{NULL, 0, NULL, 0, NULL}
};

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

	algolist = buf_new(300);
	for (i = 0; localalgos[i].name != NULL; i++) {
		if (localalgos[i].usable) {
			if (donefirst)
				buf_putbyte(algolist, ',');
			donefirst = 1;
			len = strlen(localalgos[i].name);
			buf_putbytes(algolist, (const unsigned char *) localalgos[i].name, len);
		}
	}
	buf_putstring(buf, (const char*)algolist->data, algolist->len);
	buf_free(algolist);
}

/* match the first algorithm in the comma-separated list in buf which is
 * also in localalgos[], or return NULL on failure.
 * (*goodguess) is set to 1 if the preferred client/server algos match,
 * 0 otherwise. This is used for checking if the kexalgo/hostkeyalgos are
 * guessed correctly */
algo_type * buf_match_algo(buffer* buf, algo_type localalgos[],
		enum kexguess2_used *kexguess2, int *goodguess)
{

	char * algolist = NULL;
	const char *remotenames[MAX_PROPOSED_ALGO], *localnames[MAX_PROPOSED_ALGO];
	unsigned int len;
	unsigned int remotecount, localcount, clicount, servcount, i, j;
	algo_type * ret = NULL;
	const char **clinames, **servnames;

	if (goodguess) {
		*goodguess = 0;
	}

	/* get the comma-separated list from the buffer ie "algo1,algo2,algo3" */
	algolist = buf_getstring(buf, &len);
	TRACE(("buf_match_algo: %s", algolist))
	if (len > MAX_PROPOSED_ALGO*(MAX_NAME_LEN+1)) {
		goto out;
	}

	/* remotenames will contain a list of the strings parsed out */
	/* We will have at least one string (even if it's just "") */
	remotenames[0] = algolist;
	remotecount = 1;
	for (i = 0; i < len; i++) {
		if (algolist[i] == '\0') {
			/* someone is trying something strange */
			goto out;
		}
		if (algolist[i] == ',') {
			algolist[i] = '\0';
			remotenames[remotecount] = &algolist[i+1];
			remotecount++;
		}
		if (remotecount >= MAX_PROPOSED_ALGO) {
			break;
		}
	}
	if (kexguess2 && *kexguess2 == KEXGUESS2_LOOK) {
		for (i = 0; i < remotecount; i++)
		{
			if (strcmp(remotenames[i], KEXGUESS2_ALGO_NAME) == 0) {
				*kexguess2 = KEXGUESS2_YES;
				break;
			}
		}
		if (*kexguess2 == KEXGUESS2_LOOK) {
			*kexguess2 = KEXGUESS2_NO;
		}
	}

	for (i = 0; localalgos[i].name != NULL; i++) {
		if (localalgos[i].usable) {
			localnames[i] = localalgos[i].name;
		} else {
			localnames[i] = NULL;
		}
	}
	localcount = i;

	if (IS_DROPBEAR_SERVER) {
		clinames = remotenames;
		clicount = remotecount;
		servnames = localnames;
		servcount = localcount;
	} else {
		clinames = localnames;
		clicount = localcount;
		servnames = remotenames;
		servcount = remotecount;
	}

	/* iterate and find the first match */
	for (i = 0; i < clicount; i++) {
		for (j = 0; j < servcount; j++) {
			if (!(servnames[j] && clinames[i])) {
				/* unusable algos are NULL */
				continue;
			}
			if (strcmp(servnames[j], clinames[i]) == 0) {
				/* set if it was a good guess */
				if (goodguess && kexguess2) {
					if (*kexguess2 == KEXGUESS2_YES) {
						if (i == 0) {
							*goodguess = 1;
						}

					} else {
						if (i == 0 && j == 0) {
							*goodguess = 1;
						}
					}
				}
				/* set the algo to return */
				if (IS_DROPBEAR_SERVER) {
					ret = &localalgos[j];
				} else {
					ret = &localalgos[i];
				}
				goto out;
			}
		}
	}

out:
	m_free(algolist);
	return ret;
}

#ifdef DROPBEAR_NONE_CIPHER

void
set_algo_usable(algo_type algos[], const char * algo_name, int usable)
{
	algo_type *a;
	for (a = algos; a->name != NULL; a++)
	{
		if (strcmp(a->name, algo_name) == 0)
		{
			a->usable = usable;
			return;
		}
	}
}

int
get_algo_usable(algo_type algos[], const char * algo_name)
{
	algo_type *a;
	for (a = algos; a->name != NULL; a++)
	{
		if (strcmp(a->name, algo_name) == 0)
		{
			return a->usable;
		}
	}
	return 0;
}

#endif /* DROPBEAR_NONE_CIPHER */

#ifdef ENABLE_USER_ALGO_LIST

char *
algolist_string(algo_type algos[])
{
	char *ret_list;
	buffer *b = buf_new(200);
	buf_put_algolist(b, algos);
	buf_setpos(b, b->len);
	buf_putbyte(b, '\0');
	buf_setpos(b, 4);
	ret_list = m_strdup((const char *) buf_getptr(b, b->len - b->pos));
	buf_free(b);
	return ret_list;
}

static algo_type*
check_algo(const char* algo_name, algo_type *algos)
{
	algo_type *a;
	for (a = algos; a->name != NULL; a++)
	{
		if (strcmp(a->name, algo_name) == 0)
		{
			return a;
		}
	}

	return NULL;
}

/* Checks a user provided comma-separated algorithm list for available
 * options. Any that are not acceptable are removed in-place. Returns the
 * number of valid algorithms. */
int
check_user_algos(const char* user_algo_list, algo_type * algos, 
		const char *algo_desc)
{
	algo_type new_algos[MAX_PROPOSED_ALGO+1];
	char *work_list = m_strdup(user_algo_list);
	char *start = work_list;
	char *c;
	int n;
	/* So we can iterate and look for null terminator */
	memset(new_algos, 0x0, sizeof(new_algos));
	for (c = work_list, n = 0; ; c++)
	{
		char oc = *c;
		if (n >= MAX_PROPOSED_ALGO) {
			dropbear_exit("Too many algorithms '%s'", user_algo_list);
		}
		if (*c == ',' || *c == '\0') {
			algo_type *match_algo = NULL;
			*c = '\0';
			match_algo = check_algo(start, algos);
			if (match_algo) {
				if (check_algo(start, new_algos)) {
					TRACE(("Skip repeated algorithm '%s'", start))
				} else {
					new_algos[n] = *match_algo;
					n++;
				}
			} else {
				dropbear_log(LOG_WARNING, "This Dropbear program does not support '%s' %s algorithm", start, algo_desc);
			}
			c++;
			start = c;
		}
		if (oc == '\0') {
			break;
		}
	}
	m_free(work_list);
	/* n+1 to include a null terminator */
	memcpy(algos, new_algos, sizeof(*new_algos) * (n+1));
	return n;
}
#endif /* ENABLE_USER_ALGO_LIST */
