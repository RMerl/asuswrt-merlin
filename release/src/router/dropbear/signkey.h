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

#ifndef _SIGNKEY_H_
#define _SIGNKEY_H_

#include "buffer.h"
#include "dss.h"
#include "rsa.h"


/* Sources for signing keys */
typedef enum {
	SIGNKEY_SOURCE_RAW_FILE,
	SIGNKEY_SOURCE_AGENT,
	SIGNKEY_SOURCE_INVALID,	
} signkey_source;

struct SIGN_key {

	int type; /* The type of key (dss or rsa) */
	signkey_source source;
	char *filename;
	/* the buffer? for encrypted keys, so we can later get
	 * the private key portion */

#ifdef DROPBEAR_DSS
	dropbear_dss_key * dsskey;
#endif
#ifdef DROPBEAR_RSA
	dropbear_rsa_key * rsakey;
#endif
};

typedef struct SIGN_key sign_key;

sign_key * new_sign_key();
const char* signkey_name_from_type(int type, int *namelen);
int signkey_type_from_name(const char* name, int namelen);
int buf_get_pub_key(buffer *buf, sign_key *key, int *type);
int buf_get_priv_key(buffer* buf, sign_key *key, int *type);
void buf_put_pub_key(buffer* buf, sign_key *key, int type);
void buf_put_priv_key(buffer* buf, sign_key *key, int type);
void sign_key_free(sign_key *key);
void buf_put_sign(buffer* buf, sign_key *key, int type, 
		const unsigned char *data, unsigned int len);
#ifdef DROPBEAR_SIGNKEY_VERIFY
int buf_verify(buffer * buf, sign_key *key, const unsigned char *data,
		unsigned int len);
char * sign_key_fingerprint(unsigned char* keyblob, unsigned int keybloblen);
#endif
int cmp_base64_key(const unsigned char* keyblob, unsigned int keybloblen, 
					const unsigned char* algoname, unsigned int algolen, 
					buffer * line, char ** fingerprint);

#endif /* _SIGNKEY_H_ */
