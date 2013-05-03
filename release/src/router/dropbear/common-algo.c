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
#include "session.h"
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
#ifdef DROPBEAR_NONE_CIPHER
	{"none", 0, (void*)&dropbear_nocipher, 1, &dropbear_mode_none},
#endif
	{NULL, 0, NULL, 0, NULL}
};

algo_type sshhashes[] = {
#ifdef DROPBEAR_SHA2_256_HMAC
	{"hmac-sha2-256", 0, &dropbear_sha2_256, 1, NULL},
#endif
#ifdef DROPBEAR_SHA2_512_HMAC
	{"hmac-sha2-512", 0, &dropbear_sha2_512, 1, NULL},
#endif
#ifdef DROPBEAR_SHA1_96_HMAC
	{"hmac-sha1-96", 0, &dropbear_sha1_96, 1, NULL},
#endif
#ifdef DROPBEAR_SHA1_HMAC
	{"hmac-sha1", 0, &dropbear_sha1, 1, NULL},
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
#ifdef USE_KEXGUESS2
	{KEXGUESS2_ALGO_NAME, KEXGUESS2_ALGO_ID, NULL, 1, NULL},
#endif
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
#ifdef DROPBEAR_SHA2_256_HMAC
		&sha256_desc,
#endif
#ifdef DROPBEAR_SHA2_512_HMAC
		&sha512_desc,
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

/* match the first algorithm in the comma-separated list in buf which is
 * also in localalgos[], or return NULL on failure.
 * (*goodguess) is set to 1 if the preferred client/server algos match,
 * 0 otherwise. This is used for checking if the kexalgo/hostkeyalgos are
 * guessed correctly */
algo_type * buf_match_algo(buffer* buf, algo_type localalgos[],
		enum kexguess2_used *kexguess2, int *goodguess)
{

	unsigned char * algolist = NULL;
	const unsigned char *remotenames[MAX_PROPOSED_ALGO], *localnames[MAX_PROPOSED_ALGO];
	unsigned int len;
	unsigned int remotecount, localcount, clicount, servcount, i, j;
	algo_type * ret = NULL;
	const unsigned char **clinames, **servnames;

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
				// unusable algos are NULL
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

#endif // DROPBEAR_NONE_CIPHER

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
	ret_list = m_strdup(buf_getptr(b, b->len - b->pos));
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

static void
try_add_algo(const char *algo_name, algo_type *algos, 
		const char *algo_desc, algo_type * new_algos, int *num_ret)
{
	algo_type *match_algo = check_algo(algo_name, algos);
	if (!match_algo)
	{
		dropbear_log(LOG_WARNING, "This Dropbear program does not support '%s' %s algorithm", algo_name, algo_desc);
		return;
	}

	new_algos[*num_ret] = *match_algo;
	(*num_ret)++;
}

/* Checks a user provided comma-separated algorithm list for available
 * options. Any that are not acceptable are removed in-place. Returns the
 * number of valid algorithms. */
int
check_user_algos(const char* user_algo_list, algo_type * algos, 
		const char *algo_desc)
{
	algo_type new_algos[MAX_PROPOSED_ALGO];
	/* this has two passes. first we sweep through the given list of
	 * algorithms and mark them as usable=2 in the algo_type[] array... */
	int num_ret = 0;
	char *work_list = m_strdup(user_algo_list);
	char *last_name = work_list;
	char *c;
	for (c = work_list; *c; c++)
	{
		if (*c == ',')
		{
			*c = '\0';
			try_add_algo(last_name, algos, algo_desc, new_algos, &num_ret);
			c++;
			last_name = c;
		}
	}
	try_add_algo(last_name, algos, algo_desc, new_algos, &num_ret);
	m_free(work_list);

	new_algos[num_ret].name = NULL;

	/* Copy one more as a blank delimiter */
	memcpy(algos, new_algos, sizeof(*new_algos) * (num_ret+1));
	return num_ret;
}
#endif // ENABLE_USER_ALGO_LIST
