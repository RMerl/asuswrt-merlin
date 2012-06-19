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
#include "random.h"
#include "runopts.h"

/* diffie-hellman-group1-sha1 value for p */
#define DH_P_1_LEN 128
static const unsigned char dh_p_1[DH_P_1_LEN] = {
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
#define DH_P_14_LEN 256
static const unsigned char dh_p_14[DH_P_14_LEN] = {
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
void gen_new_keys();
#ifndef DISABLE_ZLIB
static void gen_new_zstreams();
#endif
static void read_kex_algos();
/* helper function for gen_new_keys */
static void hashkeys(unsigned char *out, int outlen, 
		const hash_state * hs, unsigned const char X);


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

	/* first_kex_packet_follows - unimplemented for now */
	buf_putbyte(ses.writepayload, 0x00);

	/* reserved unit32 */
	buf_putint(ses.writepayload, 0);

	/* set up transmitted kex packet buffer for hashing. 
	 * This is freed after the end of the kex */
	ses.transkexinit = buf_newcopy(ses.writepayload);

	encrypt_packet();
	ses.dataallowed = 0; /* don't send other packets during kex */

	TRACE(("DATAALLOWED=0"))
	TRACE(("-> KEXINIT"))
	ses.kexstate.sentkexinit = 1;
}

/* *** NOTE regarding (send|recv)_msg_newkeys *** 
 * Changed by mihnea from the original kex.c to set dataallowed after a 
 * completed key exchange, no matter the order in which it was performed.
 * This enables client mode without affecting server functionality.
 */

/* Bring new keys into use after a key exchange, and let the client know*/
void send_msg_newkeys() {

	TRACE(("enter send_msg_newkeys"))

	/* generate the kexinit request */
	CHECKCLEARTOWRITE();
	buf_putbyte(ses.writepayload, SSH_MSG_NEWKEYS);
	encrypt_packet();
	

	/* set up our state */
	if (ses.kexstate.recvnewkeys) {
		TRACE(("while RECVNEWKEYS=1"))
		gen_new_keys();
		kexinitialise(); /* we've finished with this kex */
		TRACE((" -> DATAALLOWED=1"))
		ses.dataallowed = 1; /* we can send other packets again now */
		ses.kexstate.donefirstkex = 1;
	} else {
		ses.kexstate.sentnewkeys = 1;
		TRACE(("SENTNEWKEYS=1"))
	}

	TRACE(("-> MSG_NEWKEYS"))
	TRACE(("leave send_msg_newkeys"))
}

/* Bring the new keys into use after a key exchange */
void recv_msg_newkeys() {

	TRACE(("<- MSG_NEWKEYS"))
	TRACE(("enter recv_msg_newkeys"))

	/* simply check if we've sent SSH_MSG_NEWKEYS, and if so,
	 * switch to the new keys */
	if (ses.kexstate.sentnewkeys) {
		TRACE(("while SENTNEWKEYS=1"))
		gen_new_keys();
		kexinitialise(); /* we've finished with this kex */
	    TRACE((" -> DATAALLOWED=1"))
	    ses.dataallowed = 1; /* we can send other packets again now */
		ses.kexstate.donefirstkex = 1;
	} else {
		TRACE(("RECVNEWKEYS=1"))
		ses.kexstate.recvnewkeys = 1;
	}
	
	TRACE(("leave recv_msg_newkeys"))
}


/* Set up the kex for the first time */
void kexfirstinitialise() {
	ses.kexstate.donefirstkex = 0;

#ifndef DISABLE_ZLIB
	if (opts.enable_compress) {
		ses.compress_algos = ssh_compress;
	} else
#endif
	{
		ses.compress_algos = ssh_nocompress;
	}
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
	ses.kexstate.firstfollows = 0;

	ses.kexstate.datatrans = 0;
	ses.kexstate.datarecv = 0;

	ses.kexstate.lastkextime = time(NULL);

}

/* Helper function for gen_new_keys, creates a hash. It makes a copy of the
 * already initialised hash_state hs, which should already have processed
 * the dh_K and hash, since these are common. X is the letter 'A', 'B' etc.
 * out must have at least min(SHA1_HASH_SIZE, outlen) bytes allocated.
 * The output will only be expanded once, as we are assured that
 * outlen <= 2*SHA1_HASH_SIZE for all known hashes.
 *
 * See Section 7.2 of rfc4253 (ssh transport) for details */
static void hashkeys(unsigned char *out, int outlen, 
		const hash_state * hs, const unsigned char X) {

	hash_state hs2;
	unsigned char k2[SHA1_HASH_SIZE]; /* used to extending */

	memcpy(&hs2, hs, sizeof(hash_state));
	sha1_process(&hs2, &X, 1);
	sha1_process(&hs2, ses.session_id, SHA1_HASH_SIZE);
	sha1_done(&hs2, out);
	if (SHA1_HASH_SIZE < outlen) {
		/* need to extend */
		memcpy(&hs2, hs, sizeof(hash_state));
		sha1_process(&hs2, out, SHA1_HASH_SIZE);
		sha1_done(&hs2, k2);
		memcpy(&out[SHA1_HASH_SIZE], k2, outlen - SHA1_HASH_SIZE);
	}
}

/* Generate the actual encryption/integrity keys, using the results of the
 * key exchange, as specified in section 7.2 of the transport rfc 4253.
 * This occurs after the DH key-exchange.
 *
 * ses.newkeys is the new set of keys which are generated, these are only
 * taken into use after both sides have sent a newkeys message */

/* Originally from kex.c, generalized for cli/svr mode --mihnea */
void gen_new_keys() {

	unsigned char C2S_IV[MAX_IV_LEN];
	unsigned char C2S_key[MAX_KEY_LEN];
	unsigned char S2C_IV[MAX_IV_LEN];
	unsigned char S2C_key[MAX_KEY_LEN];
	/* unsigned char key[MAX_KEY_LEN]; */
	unsigned char *trans_IV, *trans_key, *recv_IV, *recv_key;

	hash_state hs;
	unsigned int C2S_keysize, S2C_keysize;
	char mactransletter, macrecvletter; /* Client or server specific */
	int recv_cipher = 0, trans_cipher = 0;

	TRACE(("enter gen_new_keys"))
	/* the dh_K and hash are the start of all hashes, we make use of that */

	sha1_init(&hs);
	sha1_process_mp(&hs, ses.dh_K);
	mp_clear(ses.dh_K);
	m_free(ses.dh_K);
	sha1_process(&hs, ses.hash, SHA1_HASH_SIZE);
	m_burn(ses.hash, SHA1_HASH_SIZE);

	if (IS_DROPBEAR_CLIENT) {
	    trans_IV	= C2S_IV;
	    recv_IV		= S2C_IV;
	    trans_key	= C2S_key;
	    recv_key	= S2C_key;
	    C2S_keysize = ses.newkeys->trans.algo_crypt->keysize;
	    S2C_keysize = ses.newkeys->recv.algo_crypt->keysize;
		mactransletter = 'E';
		macrecvletter = 'F';
	} else {
	    trans_IV	= S2C_IV;
	    recv_IV		= C2S_IV;
	    trans_key	= S2C_key;
	    recv_key	= C2S_key;
	    C2S_keysize = ses.newkeys->recv.algo_crypt->keysize;
	    S2C_keysize = ses.newkeys->trans.algo_crypt->keysize;
		mactransletter = 'F';
		macrecvletter = 'E';
	}

	hashkeys(C2S_IV, SHA1_HASH_SIZE, &hs, 'A');
	hashkeys(S2C_IV, SHA1_HASH_SIZE, &hs, 'B');
	hashkeys(C2S_key, C2S_keysize, &hs, 'C');
	hashkeys(S2C_key, S2C_keysize, &hs, 'D');

	recv_cipher = find_cipher(ses.newkeys->recv.algo_crypt->cipherdesc->name);
	if (recv_cipher < 0)
	    dropbear_exit("Crypto error");
	if (ses.newkeys->recv.crypt_mode->start(recv_cipher, 
			recv_IV, recv_key, 
			ses.newkeys->recv.algo_crypt->keysize, 0, 
			&ses.newkeys->recv.cipher_state) != CRYPT_OK) {
		dropbear_exit("Crypto error");
	}

	trans_cipher = find_cipher(ses.newkeys->trans.algo_crypt->cipherdesc->name);
	if (trans_cipher < 0)
	    dropbear_exit("Crypto error");
	if (ses.newkeys->trans.crypt_mode->start(trans_cipher, 
			trans_IV, trans_key, 
			ses.newkeys->trans.algo_crypt->keysize, 0, 
			&ses.newkeys->trans.cipher_state) != CRYPT_OK) {
		dropbear_exit("Crypto error");
	}
	
	/* MAC keys */
	hashkeys(ses.newkeys->trans.mackey, 
			ses.newkeys->trans.algo_mac->keysize, &hs, mactransletter);
	hashkeys(ses.newkeys->recv.mackey, 
			ses.newkeys->recv.algo_mac->keysize, &hs, macrecvletter);
	ses.newkeys->trans.hash_index = find_hash(ses.newkeys->trans.algo_mac->hashdesc->name),
	ses.newkeys->recv.hash_index = find_hash(ses.newkeys->recv.algo_mac->hashdesc->name),

#ifndef DISABLE_ZLIB
	gen_new_zstreams();
#endif
	
	/* Switch over to the new keys */
	m_burn(ses.keys, sizeof(struct key_context));
	m_free(ses.keys);
	ses.keys = ses.newkeys;
	ses.newkeys = NULL;

	m_burn(C2S_IV, sizeof(C2S_IV));
	m_burn(C2S_key, sizeof(C2S_key));
	m_burn(S2C_IV, sizeof(S2C_IV));
	m_burn(S2C_key, sizeof(S2C_key));

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
static void gen_new_zstreams() {

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

	/* clean up old keys */
	if (ses.keys->recv.zstream != NULL) {
		if (inflateEnd(ses.keys->recv.zstream) == Z_STREAM_ERROR) {
			/* Z_DATA_ERROR is ok, just means that stream isn't ended */
			dropbear_exit("Crypto error");
		}
		m_free(ses.keys->recv.zstream);
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

/* Originally from kex.c, generalized for cli/svr mode --mihnea  */
/* Belongs in common_kex.c where it should be moved after review */
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
	remote_ident_len = strlen((char*)ses.remoteident);

	kexhashbuf_len = local_ident_len + remote_ident_len
		+ ses.transkexinit->len + ses.payload->len
		+ KEXHASHBUF_MAX_INTS;

	ses.kexhashbuf = buf_new(kexhashbuf_len);

	if (IS_DROPBEAR_CLIENT) {

		/* read the peer's choice of algos */
		read_kex_algos();

		/* V_C, the client's version string (CR and NL excluded) */
	    buf_putstring(ses.kexhashbuf,
			(unsigned char*)LOCAL_IDENT, local_ident_len);
		/* V_S, the server's version string (CR and NL excluded) */
	    buf_putstring(ses.kexhashbuf, ses.remoteident, remote_ident_len);

		/* I_C, the payload of the client's SSH_MSG_KEXINIT */
	    buf_putstring(ses.kexhashbuf,
			ses.transkexinit->data, ses.transkexinit->len);
		/* I_S, the payload of the server's SSH_MSG_KEXINIT */
	    buf_setpos(ses.payload, 0);
	    buf_putstring(ses.kexhashbuf, ses.payload->data, ses.payload->len);

	} else {
		/* SERVER */

		/* read the peer's choice of algos */
		read_kex_algos();
		/* V_C, the client's version string (CR and NL excluded) */
	    buf_putstring(ses.kexhashbuf, ses.remoteident, remote_ident_len);
		/* V_S, the server's version string (CR and NL excluded) */
	    buf_putstring(ses.kexhashbuf, 
				(unsigned char*)LOCAL_IDENT, local_ident_len);

		/* I_C, the payload of the client's SSH_MSG_KEXINIT */
	    buf_setpos(ses.payload, 0);
	    buf_putstring(ses.kexhashbuf, ses.payload->data, ses.payload->len);

		/* I_S, the payload of the server's SSH_MSG_KEXINIT */
	    buf_putstring(ses.kexhashbuf,
			ses.transkexinit->data, ses.transkexinit->len);

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
	switch (ses.newkeys->algo_kex) {
		case DROPBEAR_KEX_DH_GROUP1:
			bytes_to_mp(dh_p, dh_p_1, DH_P_1_LEN);
			break;
		case DROPBEAR_KEX_DH_GROUP14:
			bytes_to_mp(dh_p, dh_p_14, DH_P_14_LEN);
			break;
	}
}

/* Initialises and generate one side of the diffie-hellman key exchange values.
 * See the transport rfc 4253 section 8 for details */
/* dh_pub and dh_priv MUST be already initialised */
void gen_kexdh_vals(mp_int *dh_pub, mp_int *dh_priv) {

	DEF_MP_INT(dh_p);
	DEF_MP_INT(dh_q);
	DEF_MP_INT(dh_g);

	TRACE(("enter send_msg_kexdh_reply"))
	
	m_mp_init_multi(&dh_g, &dh_p, &dh_q, NULL);

	/* read the prime and generator*/
	load_dh_p(&dh_p);
	
	if (mp_set_int(&dh_g, DH_G_VAL) != MP_OKAY) {
		dropbear_exit("Diffie-Hellman error");
	}

	/* calculate q = (p-1)/2 */
	/* dh_priv is just a temp var here */
	if (mp_sub_d(&dh_p, 1, dh_priv) != MP_OKAY) { 
		dropbear_exit("Diffie-Hellman error");
	}
	if (mp_div_2(dh_priv, &dh_q) != MP_OKAY) {
		dropbear_exit("Diffie-Hellman error");
	}

	/* Generate a private portion 0 < dh_priv < dh_q */
	gen_random_mpint(&dh_q, dh_priv);

	/* f = g^y mod p */
	if (mp_exptmod(&dh_g, dh_priv, &dh_p, dh_pub) != MP_OKAY) {
		dropbear_exit("Diffie-Hellman error");
	}
	mp_clear_multi(&dh_g, &dh_p, &dh_q, NULL);
}

/* This function is fairly common between client/server, with some substitution
 * of dh_e/dh_f etc. Hence these arguments:
 * dh_pub_us is 'e' for the client, 'f' for the server. dh_pub_them is 
 * vice-versa. dh_priv is the x/y value corresponding to dh_pub_us */
void kexdh_comb_key(mp_int *dh_pub_us, mp_int *dh_priv, mp_int *dh_pub_them,
		sign_key *hostkey) {

	mp_int dh_p;
	mp_int *dh_e = NULL, *dh_f = NULL;
	hash_state hs;

	/* read the prime and generator*/
	m_mp_init(&dh_p);
	load_dh_p(&dh_p);

	/* Check that dh_pub_them (dh_e or dh_f) is in the range [1, p-1] */
	if (mp_cmp(dh_pub_them, &dh_p) != MP_LT 
			|| mp_cmp_d(dh_pub_them, 0) != MP_GT) {
		dropbear_exit("Diffie-Hellman error");
	}
	
	/* K = e^y mod p = f^x mod p */
	ses.dh_K = (mp_int*)m_malloc(sizeof(mp_int));
	m_mp_init(ses.dh_K);
	if (mp_exptmod(dh_pub_them, dh_priv, &dh_p, ses.dh_K) != MP_OKAY) {
		dropbear_exit("Diffie-Hellman error");
	}

	/* clear no longer needed vars */
	mp_clear_multi(&dh_p, NULL);

	/* From here on, the code needs to work with the _same_ vars on each side,
	 * not vice-versaing for client/server */
	if (IS_DROPBEAR_CLIENT) {
		dh_e = dh_pub_us;
		dh_f = dh_pub_them;
	} else {
		dh_e = dh_pub_them;
		dh_f = dh_pub_us;
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
	sha1_init(&hs);
	buf_setpos(ses.kexhashbuf, 0);
	sha1_process(&hs, buf_getptr(ses.kexhashbuf, ses.kexhashbuf->len),
			ses.kexhashbuf->len);
	sha1_done(&hs, ses.hash);

	buf_burn(ses.kexhashbuf);
	buf_free(ses.kexhashbuf);
	ses.kexhashbuf = NULL;
	
	/* first time around, we set the session_id to H */
	if (ses.session_id == NULL) {
		/* create the session_id, this never needs freeing */
		ses.session_id = (unsigned char*)m_malloc(SHA1_HASH_SIZE);
		memcpy(ses.session_id, ses.hash, SHA1_HASH_SIZE);
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

	buf_incrpos(ses.payload, 16); /* start after the cookie */

	ses.newkeys = (struct key_context*)m_malloc(sizeof(struct key_context));

	/* kex_algorithms */
	algo = ses.buf_match_algo(ses.payload, sshkex, &goodguess);
	allgood &= goodguess;
	if (algo == NULL) {
		erralgo = "kex";
		goto error;
	}
	TRACE(("kex algo %s", algo->name))
	ses.newkeys->algo_kex = algo->val;

	/* server_host_key_algorithms */
	algo = ses.buf_match_algo(ses.payload, sshhostkey, &goodguess);
	allgood &= goodguess;
	if (algo == NULL) {
		erralgo = "hostkey";
		goto error;
	}
	TRACE(("hostkey algo %s", algo->name))
	ses.newkeys->algo_hostkey = algo->val;

	/* encryption_algorithms_client_to_server */
	c2s_cipher_algo = ses.buf_match_algo(ses.payload, sshciphers, &goodguess);
	if (c2s_cipher_algo == NULL) {
		erralgo = "enc c->s";
		goto error;
	}
	TRACE(("enc c2s is  %s", c2s_cipher_algo->name))

	/* encryption_algorithms_server_to_client */
	s2c_cipher_algo = ses.buf_match_algo(ses.payload, sshciphers, &goodguess);
	if (s2c_cipher_algo == NULL) {
		erralgo = "enc s->c";
		goto error;
	}
	TRACE(("enc s2c is  %s", s2c_cipher_algo->name))

	/* mac_algorithms_client_to_server */
	c2s_hash_algo = ses.buf_match_algo(ses.payload, sshhashes, &goodguess);
	if (c2s_hash_algo == NULL) {
		erralgo = "mac c->s";
		goto error;
	}
	TRACE(("hash c2s is  %s", c2s_hash_algo->name))

	/* mac_algorithms_server_to_client */
	s2c_hash_algo = ses.buf_match_algo(ses.payload, sshhashes, &goodguess);
	if (s2c_hash_algo == NULL) {
		erralgo = "mac s->c";
		goto error;
	}
	TRACE(("hash s2c is  %s", s2c_hash_algo->name))

	/* compression_algorithms_client_to_server */
	c2s_comp_algo = ses.buf_match_algo(ses.payload, ses.compress_algos, &goodguess);
	if (c2s_comp_algo == NULL) {
		erralgo = "comp c->s";
		goto error;
	}
	TRACE(("hash c2s is  %s", c2s_comp_algo->name))

	/* compression_algorithms_server_to_client */
	s2c_comp_algo = ses.buf_match_algo(ses.payload, ses.compress_algos, &goodguess);
	if (s2c_comp_algo == NULL) {
		erralgo = "comp s->c";
		goto error;
	}
	TRACE(("hash s2c is  %s", s2c_comp_algo->name))

	/* languages_client_to_server */
	buf_eatstring(ses.payload);

	/* languages_server_to_client */
	buf_eatstring(ses.payload);

	/* first_kex_packet_follows */
	if (buf_getbool(ses.payload)) {
		ses.kexstate.firstfollows = 1;
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
	return;

error:
	dropbear_exit("No matching algo %s", erralgo);
}
