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

#ifndef DROPBEAR_KEX_H_
#define DROPBEAR_KEX_H_

#include "includes.h"
#include "algo.h"
#include "signkey.h"

void send_msg_kexinit();
void recv_msg_kexinit();
void send_msg_newkeys();
void recv_msg_newkeys();
void kexfirstinitialise();

struct kex_dh_param *gen_kexdh_param();
void free_kexdh_param(struct kex_dh_param *param);
void kexdh_comb_key(struct kex_dh_param *param, mp_int *dh_pub_them,
		sign_key *hostkey);

#ifdef DROPBEAR_ECDH
struct kex_ecdh_param *gen_kexecdh_param();
void free_kexecdh_param(struct kex_ecdh_param *param);
void kexecdh_comb_key(struct kex_ecdh_param *param, buffer *pub_them,
		sign_key *hostkey);
#endif

#ifdef DROPBEAR_CURVE25519
struct kex_curve25519_param *gen_kexcurve25519_param();
void free_kexcurve25519_param(struct kex_curve25519_param *param);
void kexcurve25519_comb_key(struct kex_curve25519_param *param, buffer *pub_them,
		sign_key *hostkey);
#endif

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
	unsigned them_firstfollows : 1; /* true when first_kex_packet_follows is set */
	unsigned sentnewkeys : 1; /* set once we've send MSG_NEWKEYS (will be cleared once we have also received */
	unsigned recvnewkeys : 1; /* set once we've received MSG_NEWKEYS (cleared once we have also sent */

	unsigned donefirstkex : 1; /* Set to 1 after the first kex has completed,
								  ie the transport layer has been set up */

	unsigned our_first_follows_matches : 1;

	time_t lastkextime; /* time of the last kex */
	unsigned int datatrans; /* data transmitted since last kex */
	unsigned int datarecv; /* data received since last kex */

};

#define DH_P_1_LEN 128
extern const unsigned char dh_p_1[DH_P_1_LEN];
#define DH_P_14_LEN 256
extern const unsigned char dh_p_14[DH_P_14_LEN];

struct kex_dh_param {
	mp_int pub; /* e */
	mp_int priv; /* x */
};

#ifdef DROPBEAR_ECDH
struct kex_ecdh_param {
	ecc_key key;
};
#endif

#ifdef DROPBEAR_CURVE25519
#define CURVE25519_LEN 32
struct kex_curve25519_param {
	unsigned char priv[CURVE25519_LEN];
	unsigned char pub[CURVE25519_LEN];
};

/* No header file for curve25519_donna */
int curve25519_donna(unsigned char *out, const unsigned char *secret, const unsigned char *other);
#endif


#define MAX_KEXHASHBUF 2000

#endif /* DROPBEAR_KEX_H_ */
