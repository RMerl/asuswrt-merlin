/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
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

#ifndef DROPBEAR_ALGO_H_

#define DROPBEAR_ALGO_H_

#include "includes.h"
#include "buffer.h"

#define DROPBEAR_MODE_UNUSED 0
#define DROPBEAR_MODE_CBC 1
#define DROPBEAR_MODE_CTR 2

struct Algo_Type {

	const char *name; /* identifying name */
	char val; /* a value for this cipher, or -1 for invalid */
	const void *data; /* algorithm specific data */
	char usable; /* whether we can use this algorithm */
	const void *mode; /* the mode, currently only used for ciphers,
						 points to a 'struct dropbear_cipher_mode' */
};

typedef struct Algo_Type algo_type;

/* lists mapping ssh types of algorithms to internal values */
extern algo_type sshkex[];
extern algo_type sshhostkey[];
extern algo_type sshciphers[];
extern algo_type sshhashes[];
extern algo_type ssh_compress[];
extern algo_type ssh_delaycompress[];
extern algo_type ssh_nocompress[];

extern const struct dropbear_cipher dropbear_nocipher;
extern const struct dropbear_cipher_mode dropbear_mode_none;
extern const struct dropbear_hash dropbear_nohash;

struct dropbear_cipher {
	const struct ltc_cipher_descriptor *cipherdesc;
	const unsigned long keysize;
	const unsigned char blocksize;
};

struct dropbear_cipher_mode {
	int (*start)(int cipher, const unsigned char *IV, 
			const unsigned char *key, 
			int keylen, int num_rounds, void *cipher_state);
	int (*encrypt)(const unsigned char *pt, unsigned char *ct, 
			unsigned long len, void *cipher_state);
	int (*decrypt)(const unsigned char *ct, unsigned char *pt, 
			unsigned long len, void *cipher_state);
};

struct dropbear_hash {
	const struct ltc_hash_descriptor *hash_desc;
	const unsigned long keysize;
	/* hashsize may be truncated from the size returned by hash_desc,
	   eg sha1-96 */
	const unsigned char hashsize;
};

enum dropbear_kex_mode {
	DROPBEAR_KEX_NORMAL_DH,
	DROPBEAR_KEX_ECDH,
	DROPBEAR_KEX_CURVE25519,
};

struct dropbear_kex {
	enum dropbear_kex_mode mode;
	
	/* "normal" DH KEX */
	const unsigned char *dh_p_bytes;
	const int dh_p_len;

	/* elliptic curve DH KEX */
#ifdef DROPBEAR_ECDH
	const struct dropbear_ecc_curve *ecc_curve;
#else
	const void* dummy;
#endif

	/* both */
	const struct ltc_hash_descriptor *hash_desc;
};

int have_algo(char* algo, size_t algolen, algo_type algos[]);
void buf_put_algolist(buffer * buf, algo_type localalgos[]);

enum kexguess2_used {
	KEXGUESS2_LOOK,
	KEXGUESS2_NO,
	KEXGUESS2_YES,
};

#define KEXGUESS2_ALGO_NAME "kexguess2@matt.ucc.asn.au"
#define KEXGUESS2_ALGO_ID 99


algo_type * buf_match_algo(buffer* buf, algo_type localalgos[],
		enum kexguess2_used *kexguess2, int *goodguess);

#ifdef ENABLE_USER_ALGO_LIST
int check_user_algos(const char* user_algo_list, algo_type * algos, 
		const char *algo_desc);
char * algolist_string(algo_type algos[]);
#endif

enum {
	DROPBEAR_COMP_NONE,
	DROPBEAR_COMP_ZLIB,
	DROPBEAR_COMP_ZLIB_DELAY,
};

#endif /* DROPBEAR_ALGO_H_ */
