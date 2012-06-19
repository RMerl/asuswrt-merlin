/*
 * Dropbear - a SSH2 server
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


static void send_msg_kexdh_reply(mp_int *dh_e);

/* Handle a diffie-hellman key exchange initialisation. This involves
 * calculating a session key reply value, and corresponding hash. These
 * are carried out by send_msg_kexdh_reply(). recv_msg_kexdh_init() calls
 * that function, then brings the new keys into use */
void recv_msg_kexdh_init() {

	DEF_MP_INT(dh_e);

	TRACE(("enter recv_msg_kexdh_init"))
	if (!ses.kexstate.recvkexinit) {
		dropbear_exit("Premature kexdh_init message received");
	}

	m_mp_init(&dh_e);
	if (buf_getmpint(ses.payload, &dh_e) != DROPBEAR_SUCCESS) {
		dropbear_exit("Failed to get kex value");
	}

	send_msg_kexdh_reply(&dh_e);

	mp_clear(&dh_e);

	send_msg_newkeys();
	ses.requirenext = SSH_MSG_NEWKEYS;
	TRACE(("leave recv_msg_kexdh_init"))
}
	
/* Generate our side of the diffie-hellman key exchange value (dh_f), and
 * calculate the session key using the diffie-hellman algorithm. Following
 * that, the session hash is calculated, and signed with RSA or DSS. The
 * result is sent to the client. 
 *
 * See the transport rfc 4253 section 8 for details */
static void send_msg_kexdh_reply(mp_int *dh_e) {

	DEF_MP_INT(dh_y);
	DEF_MP_INT(dh_f);

	TRACE(("enter send_msg_kexdh_reply"))
	m_mp_init_multi(&dh_y, &dh_f, NULL);
	
	gen_kexdh_vals(&dh_f, &dh_y);

	kexdh_comb_key(&dh_f, &dh_y, dh_e, svr_opts.hostkey);
	mp_clear(&dh_y);

	/* we can start creating the kexdh_reply packet */
	CHECKCLEARTOWRITE();
	buf_putbyte(ses.writepayload, SSH_MSG_KEXDH_REPLY);
	buf_put_pub_key(ses.writepayload, svr_opts.hostkey,
			ses.newkeys->algo_hostkey);

	/* put f */
	buf_putmpint(ses.writepayload, &dh_f);
	mp_clear(&dh_f);

	/* calc the signature */
	buf_put_sign(ses.writepayload, svr_opts.hostkey, 
			ses.newkeys->algo_hostkey, ses.hash, SHA1_HASH_SIZE);

	/* the SSH_MSG_KEXDH_REPLY is done */
	encrypt_packet();

	TRACE(("leave send_msg_kexdh_reply"))
}

