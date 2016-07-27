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

#ifndef DROPBEAR_RSA_H_
#define DROPBEAR_RSA_H_

#include "includes.h"
#include "buffer.h"

#ifdef DROPBEAR_RSA 

#define RSA_SIGNATURE_SIZE (4+7+4+40)

typedef struct {

	mp_int* n;
	mp_int* e;
	/* d, p, and q are private parts */
	mp_int* d;
	mp_int* p;
	mp_int* q;

} dropbear_rsa_key;

void buf_put_rsa_sign(buffer* buf, dropbear_rsa_key *key, buffer *data_buf);
#ifdef DROPBEAR_SIGNKEY_VERIFY
int buf_rsa_verify(buffer * buf, dropbear_rsa_key *key, buffer *data_buf);
#endif
int buf_get_rsa_pub_key(buffer* buf, dropbear_rsa_key *key);
int buf_get_rsa_priv_key(buffer* buf, dropbear_rsa_key *key);
void buf_put_rsa_pub_key(buffer* buf, dropbear_rsa_key *key);
void buf_put_rsa_priv_key(buffer* buf, dropbear_rsa_key *key);
void rsa_key_free(dropbear_rsa_key *key);

#endif /* DROPBEAR_RSA */

#endif /* DROPBEAR_RSA_H_ */
