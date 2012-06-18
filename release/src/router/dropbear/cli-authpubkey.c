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

#include "includes.h"
#include "buffer.h"
#include "dbutil.h"
#include "session.h"
#include "ssh.h"
#include "runopts.h"
#include "auth.h"

#ifdef ENABLE_CLI_PUBKEY_AUTH
static void send_msg_userauth_pubkey(sign_key *key, int type, int realsign);

/* Called when we receive a SSH_MSG_USERAUTH_FAILURE for a pubkey request.
 * We use it to remove the key we tried from the list */
void cli_pubkeyfail() {

	struct SignKeyList *keyitem;
	struct SignKeyList **previtem;

	TRACE(("enter cli_pubkeyfail"))
	previtem = &cli_opts.privkeys;

	/* Find the key we failed with, and remove it */
	for (keyitem = cli_opts.privkeys; keyitem != NULL; keyitem = keyitem->next) {
		if (keyitem == cli_ses.lastprivkey) {
			*previtem = keyitem->next;
		}
		previtem = &keyitem;
	}

	sign_key_free(cli_ses.lastprivkey->key); /* It won't be used again */
	m_free(cli_ses.lastprivkey);

	TRACE(("leave cli_pubkeyfail"))
}

void recv_msg_userauth_pk_ok() {

	struct SignKeyList *keyitem = NULL;
	buffer* keybuf = NULL;
	char* algotype = NULL;
	unsigned int algolen;
	int keytype;
	unsigned int remotelen;

	TRACE(("enter recv_msg_userauth_pk_ok"))

	algotype = buf_getstring(ses.payload, &algolen);
	keytype = signkey_type_from_name(algotype, algolen);
	TRACE(("recv_msg_userauth_pk_ok: type %d", keytype))
	m_free(algotype);

	keybuf = buf_new(MAX_PUBKEY_SIZE);

	remotelen = buf_getint(ses.payload);

	/* Iterate through our keys, find which one it was that matched, and
	 * send a real request with that key */
	for (keyitem = cli_opts.privkeys; keyitem != NULL; keyitem = keyitem->next) {

		if (keyitem->type != keytype) {
			/* Types differed */
			TRACE(("types differed"))
			continue;
		}

		/* Now we compare the contents of the key */
		keybuf->pos = keybuf->len = 0;
		buf_put_pub_key(keybuf, keyitem->key, keytype);
		buf_setpos(keybuf, 0);
		buf_incrpos(keybuf, 4); /* first int is the length of the remainder (ie
								   remotelen) which has already been taken from
								   the remote buffer */


		if (keybuf->len-4 != remotelen) {
			TRACE(("lengths differed: localh %d remote %d", keybuf->len, remotelen))
			/* Lengths differed */
			continue;
		}
		if (memcmp(buf_getptr(keybuf, remotelen),
					buf_getptr(ses.payload, remotelen), remotelen) != 0) {
			/* Data didn't match this key */
			TRACE(("data differed"))
			continue;
		}

		/* Success */
		break;
	}
	buf_free(keybuf);

	if (keyitem != NULL) {
		TRACE(("matching key"))
		/* XXX TODO: if it's an encrypted key, here we ask for their
		 * password */
		send_msg_userauth_pubkey(keyitem->key, keytype, 1);
	} else {
		TRACE(("That was whacky. We got told that a key was valid, but it didn't match our list. Sounds like dodgy code on Dropbear's part"))
	}
	
	TRACE(("leave recv_msg_userauth_pk_ok"))
}

/* TODO: make it take an agent reference to use as well */
static void send_msg_userauth_pubkey(sign_key *key, int type, int realsign) {

	const char *algoname = NULL;
	int algolen;
	buffer* sigbuf = NULL;

	TRACE(("enter send_msg_userauth_pubkey"))
	CHECKCLEARTOWRITE();

	buf_putbyte(ses.writepayload, SSH_MSG_USERAUTH_REQUEST);

	buf_putstring(ses.writepayload, cli_opts.username,
			strlen(cli_opts.username));

	buf_putstring(ses.writepayload, SSH_SERVICE_CONNECTION, 
			SSH_SERVICE_CONNECTION_LEN);

	buf_putstring(ses.writepayload, AUTH_METHOD_PUBKEY, 
			AUTH_METHOD_PUBKEY_LEN);

	buf_putbyte(ses.writepayload, realsign);

	algoname = signkey_name_from_type(type, &algolen);

	buf_putstring(ses.writepayload, algoname, algolen);
	buf_put_pub_key(ses.writepayload, key, type);

	if (realsign) {
		TRACE(("realsign"))
		/* We put the signature as well - this contains string(session id), then
		 * the contents of the write payload to this point */
		sigbuf = buf_new(4 + SHA1_HASH_SIZE + ses.writepayload->len);
		buf_putstring(sigbuf, ses.session_id, SHA1_HASH_SIZE);
		buf_putbytes(sigbuf, ses.writepayload->data, ses.writepayload->len);
		buf_put_sign(ses.writepayload, key, type, sigbuf->data, sigbuf->len);
		buf_free(sigbuf); /* Nothing confidential in the buffer */
	}

	encrypt_packet();
	TRACE(("leave send_msg_userauth_pubkey"))
}

int cli_auth_pubkey() {

	TRACE(("enter cli_auth_pubkey"))

	if (cli_opts.privkeys != NULL) {
		/* Send a trial request */
		send_msg_userauth_pubkey(cli_opts.privkeys->key,
				cli_opts.privkeys->type, 0);
		cli_ses.lastprivkey = cli_opts.privkeys;
		TRACE(("leave cli_auth_pubkey-success"))
		return 1;
	} else {
		TRACE(("leave cli_auth_pubkey-failure"))
		return 0;
	}
}
#endif /* Pubkey auth */
