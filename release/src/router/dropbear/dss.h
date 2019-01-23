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

#ifndef DROPBEAR_DSS_H_
#define DROPBEAR_DSS_H_

#include "includes.h"
#include "buffer.h"

#ifdef DROPBEAR_DSS 

typedef struct {

	mp_int* p;
	mp_int* q;
	mp_int* g;
	mp_int* y;
	/* x is the private part */
	mp_int* x;

} dropbear_dss_key;

void buf_put_dss_sign(buffer* buf, dropbear_dss_key *key, buffer *data_buf);
#ifdef DROPBEAR_SIGNKEY_VERIFY
int buf_dss_verify(buffer* buf, dropbear_dss_key *key, buffer *data_buf);
#endif
int buf_get_dss_pub_key(buffer* buf, dropbear_dss_key *key);
int buf_get_dss_priv_key(buffer* buf, dropbear_dss_key *key);
void buf_put_dss_pub_key(buffer* buf, dropbear_dss_key *key);
void buf_put_dss_priv_key(buffer* buf, dropbear_dss_key *key);
void dss_key_free(dropbear_dss_key *key);

#endif /* DROPBEAR_DSS */

#endif /* DROPBEAR_DSS_H_ */
