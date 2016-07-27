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
/*
 * This file incorporates work covered by the following copyright and  
 * permission notice:
 *
 * 	Copyright (c) 2000 Markus Friedl.  All rights reserved.
 * 	
 * 	Redistribution and use in source and binary forms, with or without
 * 	modification, are permitted provided that the following conditions
 * 	are met:
 * 	1. Redistributions of source code must retain the above copyright
 * 	   notice, this list of conditions and the following disclaimer.
 * 	2. Redistributions in binary form must reproduce the above copyright
 * 	   notice, this list of conditions and the following disclaimer in the
 * 	   documentation and/or other materials provided with the distribution.
 * 	
 * 	THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * 	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * 	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * 	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * 	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * 	NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * 	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * 	THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * 	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * 	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This copyright and permission notice applies to the code parsing public keys
 * options string which can also be found in OpenSSH auth2-pubkey.c file 
 * (user_key_allowed2). It has been adapted to work with buffers.
 *
 */

/* Process a pubkey auth request */

#include "includes.h"
#include "session.h"
#include "dbutil.h"
#include "buffer.h"
#include "signkey.h"
#include "auth.h"
#include "ssh.h"
#include "packet.h"
#include "algo.h"

#ifdef ENABLE_SVR_PUBKEY_AUTH

#define MIN_AUTHKEYS_LINE 10 /* "ssh-rsa AB" - short but doesn't matter */
#define MAX_AUTHKEYS_LINE 4200 /* max length of a line in authkeys */

static int checkpubkey(char* algo, unsigned int algolen,
		unsigned char* keyblob, unsigned int keybloblen);
static int checkpubkeyperms(void);
static void send_msg_userauth_pk_ok(char* algo, unsigned int algolen,
		unsigned char* keyblob, unsigned int keybloblen);
static int checkfileperm(char * filename);

/* process a pubkey auth request, sending success or failure message as
 * appropriate */
void svr_auth_pubkey() {

	unsigned char testkey; /* whether we're just checking if a key is usable */
	char* algo = NULL; /* pubkey algo */
	unsigned int algolen;
	unsigned char* keyblob = NULL;
	unsigned int keybloblen;
	unsigned int sign_payload_length;
	buffer * signbuf = NULL;
	sign_key * key = NULL;
	char* fp = NULL;
	enum signkey_type type = -1;

	TRACE(("enter pubkeyauth"))

	/* 0 indicates user just wants to check if key can be used, 1 is an
	 * actual attempt*/
	testkey = (buf_getbool(ses.payload) == 0);

	algo = buf_getstring(ses.payload, &algolen);
	keybloblen = buf_getint(ses.payload);
	keyblob = buf_getptr(ses.payload, keybloblen);

	/* check if the key is valid */
	if (checkpubkey(algo, algolen, keyblob, keybloblen) == DROPBEAR_FAILURE) {
		send_msg_userauth_failure(0, 0);
		goto out;
	}

	/* let them know that the key is ok to use */
	if (testkey) {
		send_msg_userauth_pk_ok(algo, algolen, keyblob, keybloblen);
		goto out;
	}

	/* now we can actually verify the signature */
	
	/* get the key */
	key = new_sign_key();
	type = DROPBEAR_SIGNKEY_ANY;
	if (buf_get_pub_key(ses.payload, key, &type) == DROPBEAR_FAILURE) {
		send_msg_userauth_failure(0, 1);
		goto out;
	}

	/* create the data which has been signed - this a string containing
	 * session_id, concatenated with the payload packet up to the signature */
	assert(ses.payload_beginning <= ses.payload->pos);
	sign_payload_length = ses.payload->pos - ses.payload_beginning;
	signbuf = buf_new(ses.payload->pos + 4 + ses.session_id->len);
	buf_putbufstring(signbuf, ses.session_id);

	/* The entire contents of the payload prior. */
	buf_setpos(ses.payload, ses.payload_beginning);
	buf_putbytes(signbuf, 
		buf_getptr(ses.payload, sign_payload_length),
		sign_payload_length);
	buf_incrpos(ses.payload, sign_payload_length);

	buf_setpos(signbuf, 0);

	/* ... and finally verify the signature */
	fp = sign_key_fingerprint(keyblob, keybloblen);
	if (buf_verify(ses.payload, key, signbuf) == DROPBEAR_SUCCESS) {
		dropbear_log(LOG_NOTICE,
				"Pubkey auth succeeded for '%s' with key %s from %s",
				ses.authstate.pw_name, fp, svr_ses.addrstring);
		send_msg_userauth_success();
	} else {
		dropbear_log(LOG_WARNING,
				"Pubkey auth bad signature for '%s' with key %s from %s",
				ses.authstate.pw_name, fp, svr_ses.addrstring);
		send_msg_userauth_failure(0, 1);
	}
	m_free(fp);

out:
	/* cleanup stuff */
	if (signbuf) {
		buf_free(signbuf);
	}
	if (algo) {
		m_free(algo);
	}
	if (key) {
		sign_key_free(key);
		key = NULL;
	}
	TRACE(("leave pubkeyauth"))
}

/* Reply that the key is valid for auth, this is sent when the user sends
 * a straight copy of their pubkey to test, to avoid having to perform
 * expensive signing operations with a worthless key */
static void send_msg_userauth_pk_ok(char* algo, unsigned int algolen,
		unsigned char* keyblob, unsigned int keybloblen) {

	TRACE(("enter send_msg_userauth_pk_ok"))
	CHECKCLEARTOWRITE();

	buf_putbyte(ses.writepayload, SSH_MSG_USERAUTH_PK_OK);
	buf_putstring(ses.writepayload, algo, algolen);
	buf_putstring(ses.writepayload, (const char*)keyblob, keybloblen);

	encrypt_packet();
	TRACE(("leave send_msg_userauth_pk_ok"))

}

/* Checks whether a specified publickey (and associated algorithm) is an
 * acceptable key for authentication */
/* Returns DROPBEAR_SUCCESS if key is ok for auth, DROPBEAR_FAILURE otherwise */
static int checkpubkey(char* algo, unsigned int algolen,
		unsigned char* keyblob, unsigned int keybloblen) {

	FILE * authfile = NULL;
	char * filename = NULL;
	int ret = DROPBEAR_FAILURE;
	buffer * line = NULL;
	unsigned int len, pos;
	buffer * options_buf = NULL;
	int line_num;

	TRACE(("enter checkpubkey"))

	/* check that we can use the algo */
	if (have_algo(algo, algolen, sshhostkey) == DROPBEAR_FAILURE) {
		dropbear_log(LOG_WARNING,
				"Pubkey auth attempt with unknown algo for '%s' from %s",
				ses.authstate.pw_name, svr_ses.addrstring);
		goto out;
	}

	/* check file permissions, also whether file exists */
	if (checkpubkeyperms() == DROPBEAR_FAILURE) {
		TRACE(("bad authorized_keys permissions, or file doesn't exist"))
		goto out;
	}

	/* we don't need to check pw and pw_dir for validity, since
	 * its been done in checkpubkeyperms. */
	len = strlen(ses.authstate.pw_dir);
	/* allocate max required pathname storage,
	 * = path + "/.ssh/authorized_keys" + '\0' = pathlen + 22 */
	filename = m_malloc(len + 22);
	snprintf(filename, len + 22, "%s/.ssh/authorized_keys", 
				ses.authstate.pw_dir);

	/* open the file */
	authfile = fopen(filename, "r");
	if (authfile == NULL) {
		goto out;
	}
	TRACE(("checkpubkey: opened authorized_keys OK"))

	line = buf_new(MAX_AUTHKEYS_LINE);
	line_num = 0;

	/* iterate through the lines */
	do {
		/* new line : potentially new options */
		if (options_buf) {
			buf_free(options_buf);
			options_buf = NULL;
		}

		if (buf_getline(line, authfile) == DROPBEAR_FAILURE) {
			/* EOF reached */
			TRACE(("checkpubkey: authorized_keys EOF reached"))
			break;
		}
		line_num++;

		if (line->len < MIN_AUTHKEYS_LINE) {
			TRACE(("checkpubkey: line too short"))
			continue; /* line is too short for it to be a valid key */
		}

		/* check the key type - will fail if there are options */
		TRACE(("a line!"))

		if (strncmp((const char *) buf_getptr(line, algolen), algo, algolen) != 0) {
			int is_comment = 0;
			unsigned char *options_start = NULL;
			int options_len = 0;
			int escape, quoted;
			
			/* skip over any comments or leading whitespace */
			while (line->pos < line->len) {
				const char c = buf_getbyte(line);
				if (c == ' ' || c == '\t') {
					continue;
				} else if (c == '#') {
					is_comment = 1;
					break;
				}
				buf_incrpos(line, -1);
				break;
			}
			if (is_comment) {
				/* next line */
				continue;
			}

			/* remember start of options */
			options_start = buf_getptr(line, 1);
			quoted = 0;
			escape = 0;
			options_len = 0;
			
			/* figure out where the options are */
			while (line->pos < line->len) {
				const char c = buf_getbyte(line);
				if (!quoted && (c == ' ' || c == '\t')) {
					break;
				}
				escape = (!escape && c == '\\');
				if (!escape && c == '"') {
					quoted = !quoted;
				}
				options_len++;
			}
			options_buf = buf_new(options_len);
			buf_putbytes(options_buf, options_start, options_len);

			/* compare the algorithm. +3 so we have enough bytes to read a space and some base64 characters too. */
			if (line->pos + algolen+3 > line->len) {
				continue;
			}
			if (strncmp((const char *) buf_getptr(line, algolen), algo, algolen) != 0) {
				continue;
			}
		}
		buf_incrpos(line, algolen);
		
		/* check for space (' ') character */
		if (buf_getbyte(line) != ' ') {
			TRACE(("checkpubkey: space character expected, isn't there"))
			continue;
		}

		/* truncate the line at the space after the base64 data */
		pos = line->pos;
		for (len = 0; line->pos < line->len; len++) {
			if (buf_getbyte(line) == ' ') break;
		}	
		buf_setpos(line, pos);
		buf_setlen(line, line->pos + len);

		TRACE(("checkpubkey: line pos = %d len = %d", line->pos, line->len))

		ret = cmp_base64_key(keyblob, keybloblen, (const unsigned char *) algo, algolen, line, NULL);

		if (ret == DROPBEAR_SUCCESS && options_buf) {
			ret = svr_add_pubkey_options(options_buf, line_num, filename);
		}

		if (ret == DROPBEAR_SUCCESS) {
			break;
		}

		/* We continue to the next line otherwise */
		
	} while (1);

out:
	if (authfile) {
		fclose(authfile);
	}
	if (line) {
		buf_free(line);
	}
	m_free(filename);
	if (options_buf) {
		buf_free(options_buf);
	}
	TRACE(("leave checkpubkey: ret=%d", ret))
	return ret;
}


/* Returns DROPBEAR_SUCCESS if file permissions for pubkeys are ok,
 * DROPBEAR_FAILURE otherwise.
 * Checks that the user's homedir, ~/.ssh, and
 * ~/.ssh/authorized_keys are all owned by either root or the user, and are
 * g-w, o-w */
static int checkpubkeyperms() {

	char* filename = NULL; 
	int ret = DROPBEAR_FAILURE;
	unsigned int len;

	TRACE(("enter checkpubkeyperms"))

	if (ses.authstate.pw_dir == NULL) {
		goto out;
	}

	if ((len = strlen(ses.authstate.pw_dir)) == 0) {
		goto out;
	}

	/* allocate max required pathname storage,
	 * = path + "/.ssh/authorized_keys" + '\0' = pathlen + 22 */
	filename = m_malloc(len + 22);
	strncpy(filename, ses.authstate.pw_dir, len+1);

	/* check ~ */
	if (checkfileperm(filename) != DROPBEAR_SUCCESS) {
		goto out;
	}

	/* check ~/.ssh */
	strncat(filename, "/.ssh", 5); /* strlen("/.ssh") == 5 */
	if (checkfileperm(filename) != DROPBEAR_SUCCESS) {
		goto out;
	}

	/* now check ~/.ssh/authorized_keys */
	strncat(filename, "/authorized_keys", 16);
	if (checkfileperm(filename) != DROPBEAR_SUCCESS) {
		goto out;
	}

	/* file looks ok, return success */
	ret = DROPBEAR_SUCCESS;
	
out:
	m_free(filename);

	TRACE(("leave checkpubkeyperms"))
	return ret;
}

/* Checks that a file is owned by the user or root, and isn't writable by
 * group or other */
/* returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
static int checkfileperm(char * filename) {
	struct stat filestat;
	int badperm = 0;

	TRACE(("enter checkfileperm(%s)", filename))

	if (stat(filename, &filestat) != 0) {
		TRACE(("leave checkfileperm: stat() != 0"))
		return DROPBEAR_FAILURE;
	}
	/* check ownership - user or root only*/
	if (filestat.st_uid != ses.authstate.pw_uid
			&& filestat.st_uid != 0) {
		badperm = 1;
		TRACE(("wrong ownership"))
	}
	/* check permissions - don't want group or others +w */
	if (filestat.st_mode & (S_IWGRP | S_IWOTH)) {
		badperm = 1;
		TRACE(("wrong perms"))
	}
	if (badperm) {
		if (!ses.authstate.perm_warn) {
			ses.authstate.perm_warn = 1;
			dropbear_log(LOG_INFO, "%s must be owned by user or root, and not writable by others", filename);
		}
		TRACE(("leave checkfileperm: failure perms/owner"))
		return DROPBEAR_FAILURE;
	}

	TRACE(("leave checkfileperm: success"))
	return DROPBEAR_SUCCESS;
}

#endif
