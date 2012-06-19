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

#ifndef _KEX_H_
#define _KEX_H_

#include "includes.h"
#include "algo.h"

void send_msg_kexinit();
void recv_msg_kexinit();
void send_msg_newkeys();
void recv_msg_newkeys();
void kexfirstinitialise();
void gen_kexdh_vals(mp_int *dh_pub, mp_int *dh_priv);
void kexdh_comb_key(mp_int *dh_pub_us, mp_int *dh_priv, mp_int *dh_pub_them,
		sign_key *hostkey);

#ifndef DISABLE_ZLIB
int is_compress_trans();
int is_compress_recv();
#endif

void recv_msg_kexdh_init(); /* server */

void send_msg_kexdh_init(); /* client */
void recv_msg_kexdh_reply(); /* client */

struct KEXState {

	unsigned sentkexinit : 1; /*set when we've sent/recv kexinit packet */
	unsigned recvkexinit : 1;
	unsigned firstfollows : 1; /* true when first_kex_packet_follows is set */
	unsigned sentnewkeys : 1; /* set once we've send MSG_NEWKEYS (will be cleared once we have also received */
	unsigned recvnewkeys : 1; /* set once we've received MSG_NEWKEYS (cleared once we have also sent */

	unsigned donefirstkex : 1; /* Set to 1 after the first kex has completed,
								  ie the transport layer has been set up */

	time_t lastkextime; /* time of the last kex */
	unsigned int datatrans; /* data transmitted since last kex */
	unsigned int datarecv; /* data received since last kex */

};

#define MAX_KEXHASHBUF 2000

#endif /* _KEX_H_ */
