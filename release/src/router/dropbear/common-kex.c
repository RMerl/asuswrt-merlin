/*
 * Dropbear SSH
 * 
 * Copyright (c) 2002-2004 Matt Johnston
 * Portions Copyright (c) 2004 by Mihnea Stoenescu
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
#include "dbutil.h"
#include "algo.h"
#include "buffer.h"
#include "session.h"
#include "kex.h"
#include "ssh.h"
#include "packet.h"
#include "bignum.h"
#include "dbrandom.h"
#include "runopts.h"
#include "ecc.h"
#include "crypto_desc.h"

/* diffie-hellman-group1-sha1 value for p */
const unsigned char dh_p_1[DH_P_1_LEN] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2,
    0x21, 0x68, 0xC2, 0x34, 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
	0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 0x02, 0x0B, 0xBE, 0xA6,
	0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
	0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D,
	0xF2, 0x5F, 0x14, 0x37, 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
	0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 0xF4, 0x4C, 0x42, 0xE9,
	0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
	0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11,
	0x7C, 0x4B, 0x1F, 0xE6, 0x49, 0x28, 0x66, 0x51, 0xEC, 0xE6, 0x53, 0x81,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* diffie-hellman-group14-sha1 value for p */
const unsigned char dh_p_14[DH_P_14_LEN] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2, 
    0x21, 0x68, 0xC2, 0x34, 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1, 
	0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 0x02, 0x0B, 0xBE, 0xA6,
	0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
	0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D,
	0xF2, 0x5F, 0x14, 0x37, 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
	0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 0xF4, 0x4C, 0x42, 0xE9,
	0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
	0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11,
	0x7C, 0x4B, 0x1F, 0xE6, 0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
	0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05, 0x98, 0xDA, 0x48, 0x36,
	0x1C, 0x55, 0xD3, 0x9A, 0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
	0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96, 0x1C, 0x62, 0xF3, 0x56,
	0x20, 0x85, 0x52, 0xBB, 0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
	0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04, 0xF1, 0x74, 0x6C, 0x08,
	0xCA, 0x18, 0x21, 0x7C, 0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
	0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03, 0x9B, 0x27, 0x83, 0xA2,
	0xEC, 0x07, 0xA2, 0x8F, 0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9,
	0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18, 0x39, 0x95, 0x49, 0x7C,
	0xEA, 0x95, 0x6A, 0xE5, 0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
	0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAC, 0xAA, 0x68, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF};

/* Same for group1 and group14 */
static const int DH_G_VAL = 2;

static void kexinitialise();
static void gen_new_keys();
#ifndef DISABLE_ZLIB
static void gen_new_zstream_recv();
static void gen_new_zstream_trans();
#endif
static void read_kex_algos();
/* helper function for gen_new_keys */
static void hashkeys(unsigned char *out, unsigned int outlen, 
		const hash_state * hs, const unsigned char X);
static void finish_kexhashbuf(void);


/* Send our list of algorithms we can use */
void send_msg_kexinit() {

	CHECKCLEARTOWRITE();
	buf_putbyte(ses.writepayload, SSH_MSG_KEXINIT);

	/* cookie */
	genrandom(buf_getwriteptr(ses.writepayload, 16), 16);
	buf_incrwritepos(ses.writepayload, 16);

	/* kex algos */
	buf_put_algolist(ses.writepayload, sshkex);

	/* server_host_key_algorithms */
	buf_put_algolist(ses.writepayload, sshhostkey);

	/* encryption_algorithms_client_to_server */
	buf_put_algolist(ses.writepayload, sshciphers);

	/* encryption_algorithms_server_to_client */
	buf_put_algolist(ses.writepayload, sshciphers);

	/* mac_algorithms_client_to_server */
	buf_put_algolist(ses.writepayload, sshhashes);

	/* mac_algorithms_server_to_client */
	buf_put_algolist(ses.writepayload, sshhashes);


	/* compression_algorithms_client_to_server */
	buf_put_algolist(ses.writepayload, ses.compress_algos);

	/* compression_algorithms_server_to_client */
	buf_put_algolist(ses.writepayload, ses.compress_algos);

	/* languages_client_to_server */
	buf_putstring(ses.writepayload, "", 0);

	/* languages_server_to_client */
	buf_putstring(ses.writepayload, "", 0);

	/* first_kex_packet_follows */
	buf_putbyte(ses.writepayload, (ses.send_kex_first_guess != NULL));

	/* reserved unit32 */
	buf_putint(ses.writepayload, 0);

	/* set up transmitted kex packet buffer for hashing. 
	 * This is freed after the end of the kex */
	ses.transkexinit = buf_newcopy(ses.writepayload);

	encrypt_packet();
	ses.dataallowed = 0; /* don't send other packets during kex */

	ses.kexstate.sentkexinit = 1;

	ses.newkeys = (struct key_context*)m_malloc(sizeof(struct key_context));

	if (ses.send_kex_first_guess) {
		ses.newkeys->algo_kex = sshkex[0].data;
		ses.newkeys->algo_hostkey = sshhostkey[0].val;
		ses.send_kex_first_guess();
	}

	TRACE(("DATAALLOWED=0"))
	TRACE(("-> KEXINIT"))

}

static void switch_keys() {
	TRACE2(("enter switch_keys"))
	if (!(ses.kexstate.sentkexinit && ses.kexstate.recvkexinit)) {
		dropbear_exit("Unexpected newkeys message");
	}

	if (!ses.keys) {
		ses.keys = m_malloc(sizeof(*ses.newkeys));
	}
	if (ses.kexstate.recvnewkeys && ses.newkeys->recv.valid) {
		TRACE(("switch_keys recv"))
#ifndef DISABLE_ZLIB
		gen_new_zstream_recv();
#endif
		ses.keys->recv = ses.newkeys->recv;
		m_burn(&ses.newkeys->recv, sizeof(ses.newkeys->recv));
		ses.newkeys->recv.valid = 0;
	}
	if (ses.kexstate.sentnewkeys && ses.newkeys->trans.valid) {
		TRACE(("switch_keys trans"))
#ifndef DISABLE_ZLIB
		gen_new_zstream_trans();
#endif
		ses.keys->trans = ses.newkeys->trans;
		m_burn(&ses.newkeys->trans, sizeof(ses.newkeys->trans));
		ses.newkeys->trans.valid = 0;
	}
	if (ses.kexstate.sentnewkeys && ses.kexstate.recvnewkeys)
	{
		TRACE(("switch_keys done"))
		ses.keys->algo_kex = ses.newkeys->algo_kex;
		ses.keys->algo_hostkey = ses.newkeys->algo_hostkey;
		ses.keys->allow_compress = 0;
		m_free(ses.newkeys);
		ses.newkeys = NULL;
		kexinitialise();
	}
	TRACE2(("leave switch_keys"))
}

/* Bring new keys into use after a key exchange, and let the client know*/
void send_msg_newkeys() {

	TRACE(("enter send_msg_newkeys"))

	/* generate the kexinit request */
	CHECKCLEARTOWRITE();
	buf_putbyte(ses.writepayload, SSH_MSG_NEWKEYS);
	encrypt_packet();

	
	/* set up our state */
	ses.kexstate.sentnewkeys = 1;
	ses.kexstate.donefirstkex = 1;
	ses.dataallowed = 1; /* we can send other packets again now */
	gen_new_keys();
	switch_keys();

	TRACE(("leave send_msg_newkeys"))
}

/* Bring the new keys into use after a key exchange */
void recv_msg_newkeys() {

	TRACE(("enter recv_msg_newkeys"))

	ses.kexstate.recvnewkeys = 1;
	switch_keys();
	
	TRACE(("leave recv_msg_newkeys"))
}


/* Set up the kex for the first time */
void kexfirstinitialise() {
	ses.kexstate.donefirstkex = 0;

#ifdef DISABLE_ZLIB
	ses.compress_algos = ssh_nocompress;
#else
	switch (opts.compress_mode)
	{
		case DROPBEAR_COMPRESS_DELAYED:
			ses.compress_algos = ssh_delaycompress;
			break;

		case DROPBEAR_COMPRESS_ON:
			ses.compress_algos = ssh_compress;
			break;

		case DROPBEAR_COMPRESS_OFF:
			ses.compress_algos = ssh_nocompress;
			break;
	}
#endif
	kexinitialise();
}

/* Reset the kex state, ready for a new negotiation */
static void kexinitialise() {

	TRACE(("kexinitialise()"))

	/* sent/recv'd MSG_KEXINIT */
	ses.kexstate.sentkexinit = 0;
	ses.kexstate.recvkexinit = 0;

	/* sent/recv'd MSG_NEWKEYS */
	ses.kexstate.recvnewkeys = 0;
	ses.kexstate.sentnewkeys = 0;

	/* first_packet_follows */
	ses.kexstate.them_firstfollows = 0;

	ses.kexstate.datatrans = 0;
	ses.kexstate.datarecv = 0;

	ses.kexstate.our_first_follows_matches = 0;

	ses.kexstate.lastkextime = monotonic_now();

}

/* Helper function for gen_new_keys, creates a hash. It makes a copy of the
 * already initialised hash_state hs, which should already have processed
 * the dh_K and hash, since these are common. X is the letter 'A', 'B' etc.
 * out must have at least min(SHA1_HASH_SIZE, outlen) bytes allocated.
 *
 * See Section 7.2 of rfc4253 (ssh transport) for details */
static void hashkeys(unsigned char *out, unsigned int outlen, 
		const hash_state * hs, const unsigned char X) {

	const struct ltc_hash_descriptor *hash_desc = ses.newkeys->algo_kex->hash_desc;
	hash_state hs2;
	unsigned int offset;
	unsigned char tmpout[MAX_HASH_SIZE];

	memcpy(&hs2, hs, sizeof(hash_state));
	hash_desc->process(&hs2, &X, 1);
	hash_desc->process(&hs2, ses.session_id->data, ses.session_id->len);
	hash_desc->done(&hs2, tmpout);
	memcpy(out, tmpout, MIN(hash_desc->hashsize, outlen));
	for (offset = hash_desc->hashsize; 
			offset < outlen; 
			offset += hash_desc->hashsize)
	{
		/* need to extend */
		memcpy(&hs2, hs, sizeof(hash_state));
		hash_desc->process(&hs2, out, offset);
		hash_desc->done(&hs2, tmpout);
		memcpy(&out[offset], tmpout, MIN(outlen - offset, hash_desc->hashsize));
	}
	m_burn(&hs2, sizeof(hash_state));
}

/* Generate the actual encryption/integrity keys, using the results of the
 * key exchange, as specified in section 7.2 of the transport rfc 4253.
 * This occurs after the DH key-exchange.
 *
 * ses.newkeys is the new set of keys which are generated, these are only
 * taken into use after both sides have sent a newkeys message */

static void gen_new_keys() {

	unsigned char C2S_IV[MAX_IV_LEN];
	unsigned char C2S_key[MAX_KEY_LEN];
	unsigned char S2C_IV[MAX_IV_LEN];
	unsigned char S2C_key[MAX_KEY_LEN];
	/* unsigned char key[MAX_KEY_LEN]; */
	unsigned char *trans_IV, *trans_key, *recv_IV, *recv_key;

	hash_state hs;
	const struct ltc_hash_descriptor *hash_desc = ses.newkeys->algo_kex->hash_desc;
	char mactransletter, macrecvletter; /* Client or server specific */

	TRACE(("enter gen_new_keys"))
	/* the dh_K and hash are the start of all hashes, we make use of that */

	hash_desc->init(&hs);
	hash_process_mp(hash_desc, &hs, ses.dh_K);
	mp_clear(ses.dh_K);
	m_free(ses.dh_K);
	hash_desc->process(&hs, ses.hash->data, ses.hash->len);
	buf_burn(ses.hash);
	buf_free(ses.hash);
	ses.hash = NULL;

	if (IS_DROPBEAR_CLIENT) {
	    trans_IV	= C2S_IV;
	    recv_IV		= S2C_IV;
	    trans_key	= C2S_key;
	    recv_key	= S2C_key;
		mactransletter = 'E';
		macrecvletter = 'F';
	} else {
	    trans_IV	= S2C_IV;
	    recv_IV		= C2S_IV;
	    trans_key	= S2C_key;
	    recv_key	= C2S_key;
		mactransletter = 'F';
		macrecvletter = 'E';
	}

	hashkeys(C2S_IV, sizeof(C2S_IV), &hs, 'A');
	hashkeys(S2C_IV, sizeof(S2C_IV), &hs, 'B');
	hashkeys(C2S_key, sizeof(C2S_key), &hs, 'C');
	hashkeys(S2C_key, sizeof(S2C_key), &hs, 'D');

	if (ses.newkeys->recv.algo_crypt->cipherdesc != NULL) {
		int recv_cipher = find_cipher(ses.newkeys->recv.algo_crypt->cipherdesc->name);
		if (recv_cipher < 0)
			dropbear_exit("Crypto error");
		if (ses.newkeys->recv.crypt_mode->start(recv_cipher, 
				recv_IV, recv_key, 
				ses.newkeys->recv.algo_crypt->keysize, 0, 
				&ses.newkeys->recv.cipher_state) != CRYPT_OK) {
			dropbear_exit("Crypto error");
		}
	}

	if (ses.newkeys->trans.algo_crypt->cipherdesc != NULL) {
		int trans_cipher = find_cipher(ses.newkeys->trans.algo_crypt->cipherdesc->name);
		if (trans_cipher < 0)
			dropbear_exit("Crypto error");
		if (ses.newkeys->trans.crypt_mode->start(trans_cipher, 
				trans_IV, trans_key, 
				ses.newkeys->trans.algo_crypt->keysize, 0, 
				&ses.newkeys->trans.cipher_state) != CRYPT_OK) {
			dropbear_exit("Crypto error");
		}
	}

	if (ses.newkeys->trans.algo_mac->hash_desc != NULL) {
		hashkeys(ses.newkeys->trans.mackey, 
				ses.newkeys->trans.algo_mac->keysize, &hs, mactransletter);
		ses.newkeys->trans.hash_index = find_hash(ses.newkeys->trans.algo_mac->hash_desc->name);
	}

	if (ses.newkeys->recv.algo_mac->hash_desc != NULL) {
		hashkeys(ses.newkeys->recv.mackey, 
				ses.newkeys->recv.algo_mac->keysize, &hs, macrecvletter);
		ses.newkeys->recv.hash_index = find_hash(ses.newkeys->recv.algo_mac->hash_desc->name);
	}

	/* Ready to switch over */
	ses.newkeys->trans.valid = 1;
	ses.newkeys->recv.valid = 1;

	m_burn(C2S_IV, sizeof(C2S_IV));
	m_burn(C2S_key, sizeof(C2S_key));
	m_burn(S2C_IV, sizeof(S2C_IV));
	m_burn(S2C_key, sizeof(S2C_key));
	m_burn(&hs, sizeof(hash_state));

	TRACE(("leave gen_new_keys"))
}

#ifndef DISABLE_ZLIB

int is_compress_trans() {
	return ses.keys->trans.algo_comp == DROPBEAR_COMP_ZLIB
		|| (ses.authstate.authdone
			&& ses.keys->trans.algo_comp == DROPBEAR_COMP_ZLIB_DELAY);
}

int is_compress_recv() {
	return ses.keys->recv.algo_comp == DROPBEAR_COMP_ZLIB
		|| (ses.authstate.authdone
			&& ses.keys->recv.algo_comp == DROPBEAR_COMP_ZLIB_DELAY);
}

/* Set up new zlib compression streams, close the old ones. Only
 * called from gen_new_keys() */
static void gen_new_zstream_recv() {

	/* create new zstreams */
	if (ses.newkeys->recv.algo_comp == DROPBEAR_COMP_ZLIB
			|| ses.newkeys->recv.algo_comp == DROPBEAR_COMP_ZLIB_DELAY) {
		ses.newkeys->recv.zstream = (z_streamp)m_malloc(sizeof(z_stream));
		ses.newkeys->recv.zstream->zalloc = Z_NULL;
		ses.newkeys->recv.zstream->zfree = Z_NULL;
		
		if (inflateInit(ses.newkeys->recv.zstream) != Z_OK) {
			dropbear_exit("zlib error");
		}
	} else {
		ses.newkeys->recv.zstream = NULL;
	}
	/* clean up old keys */
	if (ses.keys->recv.zstream != NULL) {
		if (inflateEnd(ses.keys->recv.zstream) == Z_STREAM_ERROR) {
			/* Z_DATA_ERROR is ok, just means that stream isn't ended */
			dropbear_exit("Crypto error");
		}
		m_free(ses.keys->recv.zstream);
	}
}

static void gen_new_zstream_trans() {

	if (ses.newkeys->trans.algo_comp == DROPBEAR_COMP_ZLIB
			|| ses.newkeys->trans.algo_comp == DROPBEAR_COMP_ZLIB_DELAY) {
		ses.newkeys->trans.zstream = (z_streamp)m_malloc(sizeof(z_stream));
		ses.newkeys->trans.zstream->zalloc = Z_NULL;
		ses.newkeys->trans.zstream->zfree = Z_NULL;
	
		if (deflateInit2(ses.newkeys->trans.zstream, Z_DEFAULT_COMPRESSION,
					Z_DEFLATED, DROPBEAR_ZLIB_WINDOW_BITS, 
					DROPBEAR_ZLIB_MEM_LEVEL, Z_DEFAULT_STRATEGY)
				!= Z_OK) {
			dropbear_exit("zlib error");
		}
	} else {
		ses.newkeys->trans.zstream = NULL;
	}

	if (ses.keys->trans.zstream != NULL) {
		if (deflateEnd(ses.keys->trans.zstream) == Z_STREAM_ERROR) {
			/* Z_DATA_ERROR is ok, just means that stream isn't ended */
			dropbear_exit("Crypto error");
		}
		m_free(ses.keys->trans.zstream);
	}
}
#endif /* DISABLE_ZLIB */


/* Executed upon receiving a kexinit message from the client to initiate
 * key exchange. If we haven't already done so, we send the list of our
 * preferred algorithms. The client's requested algorithms are processed,
 * and we calculate the first portion of the key-exchange-hash for used
 * later in the key exchange. No response is sent, as the client should
 * initiate the diffie-hellman key exchange */
void recv_msg_kexinit() {
	
	unsigned int kexhashbuf_len = 0;
	unsigned int remote_ident_len = 0;
	unsigned int local_ident_len = 0;

	TRACE(("<- KEXINIT"))
	TRACE(("enter recv_msg_kexinit"))
	
	if (!ses.kexstate.sentkexinit) {
		/* we need to send a kex packet */
		send_msg_kexinit();
		TRACE(("continue recv_msg_kexinit: sent kexinit"))
	}

	/* start the kex hash */
	local_ident_len = strlen(LOCAL_IDENT);
	remote_ident_len = strlen(ses.remoteident);

	kexhashbuf_len = local_ident_len + remote_ident_len
		+ ses.transkexinit->len + ses.payload->len
		+ KEXHASHBUF_MAX_INTS;

	ses.kexhashbuf = buf_new(kexhashbuf_len);

	if (IS_DROPBEAR_CLIENT) {

		/* read the peer's choice of algos */
		read_kex_algos();

		/* V_C, the client's version string (CR and NL excluded) */
	    buf_putstring(ses.kexhashbuf, LOCAL_IDENT, local_ident_len);
		/* V_S, the server's version string (CR and NL excluded) */
	    buf_putstring(ses.kexhashbuf, ses.remoteident, remote_ident_len);

		/* I_C, the payload of the client's SSH_MSG_KEXINIT */
	    buf_putstring(ses.kexhashbuf,
			(const char*)ses.transkexinit->data, ses.transkexinit->len);
		/* I_S, the payload of the server's SSH_MSG_KEXINIT */
	    buf_setpos(ses.payload, ses.payload_beginning);
	    buf_putstring(ses.kexhashbuf, 
	    	(const char*)buf_getptr(ses.payload, ses.payload->len-ses.payload->pos),
	    	ses.payload->len-ses.payload->pos);
		ses.requirenext = SSH_MSG_KEXDH_REPLY;
	} else {
		/* SERVER */

		/* read the peer's choice of algos */
		read_kex_algos();
		/* V_C, the client's version string (CR and NL excluded) */
	    buf_putstring(ses.kexhashbuf, ses.remoteident, remote_ident_len);
		/* V_S, the server's version string (CR and NL excluded) */
	    buf_putstring(ses.kexhashbuf, LOCAL_IDENT, local_ident_len);

		/* I_C, the payload of the client's SSH_MSG_KEXINIT */
	    buf_setpos(ses.payload, ses.payload_beginning);
	    buf_putstring(ses.kexhashbuf, 
	    	(const char*)buf_getptr(ses.payload, ses.payload->len-ses.payload->pos),
	    	ses.payload->len-ses.payload->pos);

		/* I_S, the payload of the server's SSH_MSG_KEXINIT */
	    buf_putstring(ses.kexhashbuf,
			(const char*)ses.transkexinit->data, ses.transkexinit->len);

		ses.requirenext = SSH_MSG_KEXDH_INIT;
	}

	buf_free(ses.transkexinit);
	ses.transkexinit = NULL;
	/* the rest of ses.kexhashbuf will be done after DH exchange */

	ses.kexstate.recvkexinit = 1;

	TRACE(("leave recv_msg_kexinit"))
}

static void load_dh_p(mp_int * dh_p)
{
	bytes_to_mp(dh_p, ses.newkeys->algo_kex->dh_p_bytes, 
		ses.newkeys->algo_kex->dh_p_len);
}

/* Initialises and generate one side of the diffie-hellman key exchange values.
 * See the transport rfc 4253 section 8 for details */
/* dh_pub and dh_priv MUST be already initialised */
struct kex_dh_param *gen_kexdh_param() {
	struct kex_dh_param *param = NULL;

	DEF_MP_INT(dh_p);
	DEF_MP_INT(dh_q);
	DEF_MP_INT(dh_g);

	TRACE(("enter gen_kexdh_vals"))

	param = m_malloc(sizeof(*param));
	m_mp_init_multi(&param->pub, &param->priv, &dh_g, &dh_p, &dh_q, NULL);

	/* read the prime and generator*/
	load_dh_p(&dh_p);
	
	if (mp_set_int(&dh_g, DH_G_VAL) != MP_OKAY) {
		dropbear_exit("Diffie-Hellman error");
	}

	/* calculate q = (p-1)/2 */
	/* dh_priv is just a temp var here */
	if (mp_sub_d(&dh_p, 1, &param->priv) != MP_OKAY) { 
		dropbear_exit("Diffie-Hellman error");
	}
	if (mp_div_2(&param->priv, &dh_q) != MP_OKAY) {
		dropbear_exit("Diffie-Hellman error");
	}

	/* Generate a private portion 0 < dh_priv < dh_q */
	gen_random_mpint(&dh_q, &param->priv);

	/* f = g^y mod p */
	if (mp_exptmod(&dh_g, &param->priv, &dh_p, &param->pub) != MP_OKAY) {
		dropbear_exit("Diffie-Hellman error");
	}
	mp_clear_multi(&dh_g, &dh_p, &dh_q, NULL);
	return param;
}

void free_kexdh_param(struct kex_dh_param *param)
{
	mp_clear_multi(&param->pub, &param->priv, NULL);
	m_free(param);
}

/* This function is fairly common between client/server, with some substitution
 * of dh_e/dh_f etc. Hence these arguments:
 * dh_pub_us is 'e' for the client, 'f' for the server. dh_pub_them is 
 * vice-versa. dh_priv is the x/y value corresponding to dh_pub_us */
void kexdh_comb_key(struct kex_dh_param *param, mp_int *dh_pub_them,
		sign_key *hostkey) {

	DEF_MP_INT(dh_p);
	DEF_MP_INT(dh_p_min1);
	mp_int *dh_e = NULL, *dh_f = NULL;

	m_mp_init_multi(&dh_p, &dh_p_min1, NULL);
	load_dh_p(&dh_p);

	if (mp_sub_d(&dh_p, 1, &dh_p_min1) != MP_OKAY) { 
		dropbear_exit("Diffie-Hellman error");
	}

	/* Check that dh_pub_them (dh_e or dh_f) is in the range [2, p-2] */
	if (mp_cmp(dh_pub_them, &dh_p_min1) != MP_LT 
			|| mp_cmp_d(dh_pub_them, 1) != MP_GT) {
		dropbear_exit("Diffie-Hellman error");
	}
	
	/* K = e^y mod p = f^x mod p */
	m_mp_alloc_init_multi(&ses.dh_K, NULL);
	if (mp_exptmod(dh_pub_them, &param->priv, &dh_p, ses.dh_K) != MP_OKAY) {
		dropbear_exit("Diffie-Hellman error");
	}

	/* clear no longer needed vars */
	mp_clear_multi(&dh_p, &dh_p_min1, NULL);

	/* From here on, the code needs to work with the _same_ vars on each side,
	 * not vice-versaing for client/server */
	if (IS_DROPBEAR_CLIENT) {
		dh_e = &param->pub;
		dh_f = dh_pub_them;
	} else {
		dh_e = dh_pub_them;
		dh_f = &param->pub;
	} 

	/* Create the remainder of the hash buffer, to generate the exchange hash */
	/* K_S, the host key */
	buf_put_pub_key(ses.kexhashbuf, hostkey, ses.newkeys->algo_hostkey);
	/* e, exchange value sent by the client */
	buf_putmpint(ses.kexhashbuf, dh_e);
	/* f, exchange value sent by the server */
	buf_putmpint(ses.kexhashbuf, dh_f);
	/* K, the shared secret */
	buf_putmpint(ses.kexhashbuf, ses.dh_K);

	/* calculate the hash H to sign */
	finish_kexhashbuf();
}

#ifdef DROPBEAR_ECDH
struct kex_ecdh_param *gen_kexecdh_param() {
	struct kex_ecdh_param *param = m_malloc(sizeof(*param));
	if (ecc_make_key_ex(NULL, dropbear_ltc_prng, 
		&param->key, ses.newkeys->algo_kex->ecc_curve->dp) != CRYPT_OK) {
		dropbear_exit("ECC error");
	}
	return param;
}

void free_kexecdh_param(struct kex_ecdh_param *param) {
	ecc_free(&param->key);
	m_free(param);

}
void kexecdh_comb_key(struct kex_ecdh_param *param, buffer *pub_them,
		sign_key *hostkey) {
	const struct dropbear_kex *algo_kex = ses.newkeys->algo_kex;
	/* public keys from client and server */
	ecc_key *Q_C, *Q_S, *Q_them;

	Q_them = buf_get_ecc_raw_pubkey(pub_them, algo_kex->ecc_curve);
	if (Q_them == NULL) {
		dropbear_exit("ECC error");
	}

	ses.dh_K = dropbear_ecc_shared_secret(Q_them, &param->key);

	/* Create the remainder of the hash buffer, to generate the exchange hash
	   See RFC5656 section 4 page 7 */
	if (IS_DROPBEAR_CLIENT) {
		Q_C = &param->key;
		Q_S = Q_them;
	} else {
		Q_C = Q_them;
		Q_S = &param->key;
	} 

	/* K_S, the host key */
	buf_put_pub_key(ses.kexhashbuf, hostkey, ses.newkeys->algo_hostkey);
	/* Q_C, client's ephemeral public key octet string */
	buf_put_ecc_raw_pubkey_string(ses.kexhashbuf, Q_C);
	/* Q_S, server's ephemeral public key octet string */
	buf_put_ecc_raw_pubkey_string(ses.kexhashbuf, Q_S);
	/* K, the shared secret */
	buf_putmpint(ses.kexhashbuf, ses.dh_K);

	/* calculate the hash H to sign */
	finish_kexhashbuf();
}
#endif /* DROPBEAR_ECDH */

#ifdef DROPBEAR_CURVE25519
struct kex_curve25519_param *gen_kexcurve25519_param () {
	/* Per http://cr.yp.to/ecdh.html */
	struct kex_curve25519_param *param = m_malloc(sizeof(*param));
	const unsigned char basepoint[32] = {9};

	genrandom(param->priv, CURVE25519_LEN);
	param->priv[0] &= 248;
	param->priv[31] &= 127;
	param->priv[31] |= 64;

	curve25519_donna(param->pub, param->priv, basepoint);

	return param;
}

void free_kexcurve25519_param(struct kex_curve25519_param *param)
{
	m_burn(param->priv, CURVE25519_LEN);
	m_free(param);
}

void kexcurve25519_comb_key(struct kex_curve25519_param *param, buffer *buf_pub_them,
	sign_key *hostkey) {
	unsigned char out[CURVE25519_LEN];
	const unsigned char* Q_C = NULL;
	const unsigned char* Q_S = NULL;
	char zeroes[CURVE25519_LEN] = {0};

	if (buf_pub_them->len != CURVE25519_LEN)
	{
		dropbear_exit("Bad curve25519");
	}

	curve25519_donna(out, param->priv, buf_pub_them->data);

	if (constant_time_memcmp(zeroes, out, CURVE25519_LEN) == 0) {
		dropbear_exit("Bad curve25519");
	}

	m_mp_alloc_init_multi(&ses.dh_K, NULL);
	bytes_to_mp(ses.dh_K, out, CURVE25519_LEN);
	m_burn(out, sizeof(out));

	/* Create the remainder of the hash buffer, to generate the exchange hash.
	   See RFC5656 section 4 page 7 */
	if (IS_DROPBEAR_CLIENT) {
		Q_C = param->pub;
		Q_S = buf_pub_them->data;
	} else {
		Q_S = param->pub;
		Q_C = buf_pub_them->data;
	}

	/* K_S, the host key */
	buf_put_pub_key(ses.kexhashbuf, hostkey, ses.newkeys->algo_hostkey);
	/* Q_C, client's ephemeral public key octet string */
	buf_putstring(ses.kexhashbuf, (const char*)Q_C, CURVE25519_LEN);
	/* Q_S, server's ephemeral public key octet string */
	buf_putstring(ses.kexhashbuf, (const char*)Q_S, CURVE25519_LEN);
	/* K, the shared secret */
	buf_putmpint(ses.kexhashbuf, ses.dh_K);

	/* calculate the hash H to sign */
	finish_kexhashbuf();
}
#endif /* DROPBEAR_CURVE25519 */



static void finish_kexhashbuf(void) {
	hash_state hs;
	const struct ltc_hash_descriptor *hash_desc = ses.newkeys->algo_kex->hash_desc;

	hash_desc->init(&hs);
	buf_setpos(ses.kexhashbuf, 0);
	hash_desc->process(&hs, buf_getptr(ses.kexhashbuf, ses.kexhashbuf->len),
			ses.kexhashbuf->len);
	ses.hash = buf_new(hash_desc->hashsize);
	hash_desc->done(&hs, buf_getwriteptr(ses.hash, hash_desc->hashsize));
	buf_setlen(ses.hash, hash_desc->hashsize);

#if defined(DEBUG_KEXHASH) && defined(DEBUG_TRACE)
	if (!debug_trace) {
		printhex("kexhashbuf", ses.kexhashbuf->data, ses.kexhashbuf->len);
		printhex("kexhash", ses.hash->data, ses.hash->len);
	}
#endif

	buf_burn(ses.kexhashbuf);
	buf_free(ses.kexhashbuf);
	m_burn(&hs, sizeof(hash_state));
	ses.kexhashbuf = NULL;
	
	/* first time around, we set the session_id to H */
	if (ses.session_id == NULL) {
		/* create the session_id, this never needs freeing */
		ses.session_id = buf_newcopy(ses.hash);
	}
}

/* read the other side's algo list. buf_match_algo is a callback to match
 * algos for the client or server. */
static void read_kex_algos() {

	/* for asymmetry */
	algo_type * c2s_hash_algo = NULL;
	algo_type * s2c_hash_algo = NULL;
	algo_type * c2s_cipher_algo = NULL;
	algo_type * s2c_cipher_algo = NULL;
	algo_type * c2s_comp_algo = NULL;
	algo_type * s2c_comp_algo = NULL;
	/* the generic one */
	algo_type * algo = NULL;

	/* which algo couldn't match */
	char * erralgo = NULL;

	int goodguess = 0;
	int allgood = 1; /* we AND this with each goodguess and see if its still
						true after */

#ifdef USE_KEXGUESS2
	enum kexguess2_used kexguess2 = KEXGUESS2_LOOK;
#else
	enum kexguess2_used kexguess2 = KEXGUESS2_NO;
#endif

	buf_incrpos(ses.payload, 16); /* start after the cookie */

	memset(ses.newkeys, 0x0, sizeof(*ses.newkeys));

	/* kex_algorithms */
	algo = buf_match_algo(ses.payload, sshkex, &kexguess2, &goodguess);
	allgood &= goodguess;
	if (algo == NULL || algo->val == KEXGUESS2_ALGO_ID) {
		erralgo = "kex";
		goto error;
	}
	TRACE(("kexguess2 %d", kexguess2))
	TRACE(("kex algo %s", algo->name))
	ses.newkeys->algo_kex = algo->data;

	/* server_host_key_algorithms */
	algo = buf_match_algo(ses.payload, sshhostkey, &kexguess2, &goodguess);
	allgood &= goodguess;
	if (algo == NULL) {
		erralgo = "hostkey";
		goto error;
	}
	TRACE(("hostkey algo %s", algo->name))
	ses.newkeys->algo_hostkey = algo->val;

	/* encryption_algorithms_client_to_server */
	c2s_cipher_algo = buf_match_algo(ses.payload, sshciphers, NULL, NULL);
	if (c2s_cipher_algo == NULL) {
		erralgo = "enc c->s";
		goto error;
	}
	TRACE(("enc c2s is  %s", c2s_cipher_algo->name))

	/* encryption_algorithms_server_to_client */
	s2c_cipher_algo = buf_match_algo(ses.payload, sshciphers, NULL, NULL);
	if (s2c_cipher_algo == NULL) {
		erralgo = "enc s->c";
		goto error;
	}
	TRACE(("enc s2c is  %s", s2c_cipher_algo->name))

	/* mac_algorithms_client_to_server */
	c2s_hash_algo = buf_match_algo(ses.payload, sshhashes, NULL, NULL);
	if (c2s_hash_algo == NULL) {
		erralgo = "mac c->s";
		goto error;
	}
	TRACE(("hash c2s is  %s", c2s_hash_algo->name))

	/* mac_algorithms_server_to_client */
	s2c_hash_algo = buf_match_algo(ses.payload, sshhashes, NULL, NULL);
	if (s2c_hash_algo == NULL) {
		erralgo = "mac s->c";
		goto error;
	}
	TRACE(("hash s2c is  %s", s2c_hash_algo->name))

	/* compression_algorithms_client_to_server */
	c2s_comp_algo = buf_match_algo(ses.payload, ses.compress_algos, NULL, NULL);
	if (c2s_comp_algo == NULL) {
		erralgo = "comp c->s";
		goto error;
	}
	TRACE(("hash c2s is  %s", c2s_comp_algo->name))

	/* compression_algorithms_server_to_client */
	s2c_comp_algo = buf_match_algo(ses.payload, ses.compress_algos, NULL, NULL);
	if (s2c_comp_algo == NULL) {
		erralgo = "comp s->c";
		goto error;
	}
	TRACE(("hash s2c is  %s", s2c_comp_algo->name))

	/* languages_client_to_server */
	buf_eatstring(ses.payload);

	/* languages_server_to_client */
	buf_eatstring(ses.payload);

	/* their first_kex_packet_follows */
	if (buf_getbool(ses.payload)) {
		TRACE(("them kex firstfollows. allgood %d", allgood))
		ses.kexstate.them_firstfollows = 1;
		/* if the guess wasn't good, we ignore the packet sent */
		if (!allgood) {
			ses.ignorenext = 1;
		}
	}

	/* Handle the asymmetry */
	if (IS_DROPBEAR_CLIENT) {
		ses.newkeys->recv.algo_crypt = 
			(struct dropbear_cipher*)s2c_cipher_algo->data;
		ses.newkeys->trans.algo_crypt = 
			(struct dropbear_cipher*)c2s_cipher_algo->data;
		ses.newkeys->recv.crypt_mode = 
			(struct dropbear_cipher_mode*)s2c_cipher_algo->mode;
		ses.newkeys->trans.crypt_mode =
			(struct dropbear_cipher_mode*)c2s_cipher_algo->mode;
		ses.newkeys->recv.algo_mac = 
			(struct dropbear_hash*)s2c_hash_algo->data;
		ses.newkeys->trans.algo_mac = 
			(struct dropbear_hash*)c2s_hash_algo->data;
		ses.newkeys->recv.algo_comp = s2c_comp_algo->val;
		ses.newkeys->trans.algo_comp = c2s_comp_algo->val;
	} else {
		/* SERVER */
		ses.newkeys->recv.algo_crypt = 
			(struct dropbear_cipher*)c2s_cipher_algo->data;
		ses.newkeys->trans.algo_crypt = 
			(struct dropbear_cipher*)s2c_cipher_algo->data;
		ses.newkeys->recv.crypt_mode =
			(struct dropbear_cipher_mode*)c2s_cipher_algo->mode;
		ses.newkeys->trans.crypt_mode =
			(struct dropbear_cipher_mode*)s2c_cipher_algo->mode;
		ses.newkeys->recv.algo_mac = 
			(struct dropbear_hash*)c2s_hash_algo->data;
		ses.newkeys->trans.algo_mac = 
			(struct dropbear_hash*)s2c_hash_algo->data;
		ses.newkeys->recv.algo_comp = c2s_comp_algo->val;
		ses.newkeys->trans.algo_comp = s2c_comp_algo->val;
	}

	/* reserved for future extensions */
	buf_getint(ses.payload);

	if (ses.send_kex_first_guess && allgood) {
		TRACE(("our_first_follows_matches 1"))
		ses.kexstate.our_first_follows_matches = 1;
	}
	return;

error:
	dropbear_exit("No matching algo %s", erralgo);
}
