/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002-2004 Matt Johnston
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
#include "session.h"
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
#include "signkey.h"
#include "ecc.h"


static void checkhostkey(unsigned char* keyblob, unsigned int keybloblen);
#define MAX_KNOWNHOSTS_LINE 4500

void send_msg_kexdh_init() {
	TRACE(("send_msg_kexdh_init()"))	

	CHECKCLEARTOWRITE();
	buf_putbyte(ses.writepayload, SSH_MSG_KEXDH_INIT);
	switch (ses.newkeys->algo_kex->mode) {
		case DROPBEAR_KEX_NORMAL_DH:
			if (ses.newkeys->algo_kex != cli_ses.param_kex_algo
				|| !cli_ses.dh_param) {
				if (cli_ses.dh_param) {
					free_kexdh_param(cli_ses.dh_param);
				}
				cli_ses.dh_param = gen_kexdh_param();
			}
			buf_putmpint(ses.writepayload, &cli_ses.dh_param->pub);
			break;
		case DROPBEAR_KEX_ECDH:
#ifdef DROPBEAR_ECDH
			if (ses.newkeys->algo_kex != cli_ses.param_kex_algo
				|| !cli_ses.ecdh_param) {
				if (cli_ses.ecdh_param) {
					free_kexecdh_param(cli_ses.ecdh_param);
				}
				cli_ses.ecdh_param = gen_kexecdh_param();
			}
			buf_put_ecc_raw_pubkey_string(ses.writepayload, &cli_ses.ecdh_param->key);
#endif
			break;
#ifdef DROPBEAR_CURVE25519
		case DROPBEAR_KEX_CURVE25519:
			if (ses.newkeys->algo_kex != cli_ses.param_kex_algo
				|| !cli_ses.curve25519_param) {
				if (cli_ses.curve25519_param) {
					free_kexcurve25519_param(cli_ses.curve25519_param);
				}
				cli_ses.curve25519_param = gen_kexcurve25519_param();
			}
			buf_putstring(ses.writepayload, (const char*)cli_ses.curve25519_param->pub, CURVE25519_LEN);
#endif
			break;
	}

	cli_ses.param_kex_algo = ses.newkeys->algo_kex;
	encrypt_packet();
}

/* Handle a diffie-hellman key exchange reply. */
void recv_msg_kexdh_reply() {

	sign_key *hostkey = NULL;
	unsigned int type, keybloblen;
	unsigned char* keyblob = NULL;

	TRACE(("enter recv_msg_kexdh_reply"))

	if (cli_ses.kex_state != KEXDH_INIT_SENT) {
		dropbear_exit("Received out-of-order kexdhreply");
	}
	type = ses.newkeys->algo_hostkey;
	TRACE(("type is %d", type))

	hostkey = new_sign_key();
	keybloblen = buf_getint(ses.payload);

	keyblob = buf_getptr(ses.payload, keybloblen);
	if (!ses.kexstate.donefirstkex) {
		/* Only makes sense the first time */
		checkhostkey(keyblob, keybloblen);
	}

	if (buf_get_pub_key(ses.payload, hostkey, &type) != DROPBEAR_SUCCESS) {
		TRACE(("failed getting pubkey"))
		dropbear_exit("Bad KEX packet");
	}

	switch (ses.newkeys->algo_kex->mode) {
		case DROPBEAR_KEX_NORMAL_DH:
			{
			DEF_MP_INT(dh_f);
			m_mp_init(&dh_f);
			if (buf_getmpint(ses.payload, &dh_f) != DROPBEAR_SUCCESS) {
				TRACE(("failed getting mpint"))
				dropbear_exit("Bad KEX packet");
			}

			kexdh_comb_key(cli_ses.dh_param, &dh_f, hostkey);
			mp_clear(&dh_f);
			}
			break;
		case DROPBEAR_KEX_ECDH:
#ifdef DROPBEAR_ECDH
			{
			buffer *ecdh_qs = buf_getstringbuf(ses.payload);
			kexecdh_comb_key(cli_ses.ecdh_param, ecdh_qs, hostkey);
			buf_free(ecdh_qs);
			}
#endif
			break;
#ifdef DROPBEAR_CURVE25519
		case DROPBEAR_KEX_CURVE25519:
			{
			buffer *ecdh_qs = buf_getstringbuf(ses.payload);
			kexcurve25519_comb_key(cli_ses.curve25519_param, ecdh_qs, hostkey);
			buf_free(ecdh_qs);
			}
#endif
			break;
	}

	if (cli_ses.dh_param) {
		free_kexdh_param(cli_ses.dh_param);
		cli_ses.dh_param = NULL;
	}
#ifdef DROPBEAR_ECDH
	if (cli_ses.ecdh_param) {
		free_kexecdh_param(cli_ses.ecdh_param);
		cli_ses.ecdh_param = NULL;
	}
#endif
#ifdef DROPBEAR_CURVE25519
	if (cli_ses.curve25519_param) {
		free_kexcurve25519_param(cli_ses.curve25519_param);
		cli_ses.curve25519_param = NULL;
	}
#endif

	cli_ses.param_kex_algo = NULL;
	if (buf_verify(ses.payload, hostkey, ses.hash) != DROPBEAR_SUCCESS) {
		dropbear_exit("Bad hostkey signature");
	}

	sign_key_free(hostkey);
	hostkey = NULL;

	send_msg_newkeys();
	ses.requirenext = SSH_MSG_NEWKEYS;
	TRACE(("leave recv_msg_kexdh_init"))
}

static void ask_to_confirm(unsigned char* keyblob, unsigned int keybloblen,
	const char* algoname) {

	char* fp = NULL;
	FILE *tty = NULL;
	char response = 'z';

	fp = sign_key_fingerprint(keyblob, keybloblen);
	if (cli_opts.always_accept_key) {
		fprintf(stderr, "\nHost '%s' key accepted unconditionally.\n(%s fingerprint %s)\n",
				cli_opts.remotehost,
				algoname,
				fp);
		m_free(fp);
		return;
	}
	fprintf(stderr, "\nHost '%s' is not in the trusted hosts file.\n(%s fingerprint %s)\nDo you want to continue connecting? (y/n) ", 
			cli_opts.remotehost, 
			algoname,
			fp);
	m_free(fp);

	tty = fopen(_PATH_TTY, "r");
	if (tty) {
		response = getc(tty);
		fclose(tty);
	} else {
		response = getc(stdin);
	}

	if (response == 'y') {
		return;
	}

	dropbear_exit("Didn't validate host key");
}

static FILE* open_known_hosts_file(int * readonly)
{
	FILE * hostsfile = NULL;
	char * filename = NULL;
	char * homedir = NULL;
	
	homedir = getenv("HOME");

	if (!homedir) {
		struct passwd * pw = NULL;
		pw = getpwuid(getuid());
		if (pw) {
			homedir = pw->pw_dir;
		}
	}

	if (homedir) {
		unsigned int len;
		len = strlen(homedir);
		filename = m_malloc(len + 18); /* "/.ssh/known_hosts" and null-terminator*/

		snprintf(filename, len+18, "%s/.ssh", homedir);
		/* Check that ~/.ssh exists - easiest way is just to mkdir */
		if (mkdir(filename, S_IRWXU) != 0) {
			if (errno != EEXIST) {
				dropbear_log(LOG_INFO, "Warning: failed creating %s/.ssh: %s",
						homedir, strerror(errno));
				TRACE(("mkdir didn't work: %s", strerror(errno)))
				goto out;
			}
		}

		snprintf(filename, len+18, "%s/.ssh/known_hosts", homedir);
		hostsfile = fopen(filename, "a+");
		
		if (hostsfile != NULL) {
			*readonly = 0;
			fseek(hostsfile, 0, SEEK_SET);
		} else {
			/* We mightn't have been able to open it if it was read-only */
			if (errno == EACCES || errno == EROFS) {
					TRACE(("trying readonly: %s", strerror(errno)))
					*readonly = 1;
					hostsfile = fopen(filename, "r");
			}
		}
	}

	if (hostsfile == NULL) {
		TRACE(("hostsfile didn't open: %s", strerror(errno)))
		dropbear_log(LOG_WARNING, "Failed to open %s/.ssh/known_hosts",
				homedir);
		goto out;
	}	

out:
	m_free(filename);
	return hostsfile;
}

static void checkhostkey(unsigned char* keyblob, unsigned int keybloblen) {

	FILE *hostsfile = NULL;
	int readonly = 0;
	unsigned int hostlen, algolen;
	unsigned long len;
	const char *algoname = NULL;
	char * fingerprint = NULL;
	buffer * line = NULL;
	int ret;

	if (cli_opts.no_hostkey_check) {
		fprintf(stderr, "Caution, skipping hostkey check for %s\n", cli_opts.remotehost);
		return;
	}

	algoname = signkey_name_from_type(ses.newkeys->algo_hostkey, &algolen);

	hostsfile = open_known_hosts_file(&readonly);
	if (!hostsfile)	{
		ask_to_confirm(keyblob, keybloblen, algoname);
		/* ask_to_confirm will exit upon failure */
		return;
	}
	
	line = buf_new(MAX_KNOWNHOSTS_LINE);
	hostlen = strlen(cli_opts.remotehost);

	do {
		if (buf_getline(line, hostsfile) == DROPBEAR_FAILURE) {
			TRACE(("failed reading line: prob EOF"))
			break;
		}

		/* The line is too short to be sensible */
		/* "30" is 'enough to hold ssh-dss plus the spaces, ie so we don't
		 * buf_getfoo() past the end and die horribly - the base64 parsing
		 * code is what tiptoes up to the end nicely */
		if (line->len < (hostlen+30) ) {
			TRACE(("line is too short to be sensible"))
			continue;
		}

		/* Compare hostnames */
		if (strncmp(cli_opts.remotehost, (const char *) buf_getptr(line, hostlen),
					hostlen) != 0) {
			continue;
		}

		buf_incrpos(line, hostlen);
		if (buf_getbyte(line) != ' ') {
			/* there wasn't a space after the hostname, something dodgy */
			TRACE(("missing space afte matching hostname"))
			continue;
		}

		if (strncmp((const char *) buf_getptr(line, algolen), algoname, algolen) != 0) {
			TRACE(("algo doesn't match"))
			continue;
		}

		buf_incrpos(line, algolen);
		if (buf_getbyte(line) != ' ') {
			TRACE(("missing space after algo"))
			continue;
		}

		/* Now we're at the interesting hostkey */
		ret = cmp_base64_key(keyblob, keybloblen, (const unsigned char *) algoname, algolen,
						line, &fingerprint);

		if (ret == DROPBEAR_SUCCESS) {
			/* Good matching key */
			TRACE(("good matching key"))
			goto out;
		}

		/* The keys didn't match. eep. Note that we're "leaking"
		   the fingerprint strings here, but we're exiting anyway */
		dropbear_exit("\n\n%s host key mismatch for %s !\n"
					"Fingerprint is %s\n"
					"Expected %s\n"
					"If you know that the host key is correct you can\nremove the bad entry from ~/.ssh/known_hosts", 
					algoname,
					cli_opts.remotehost,
					sign_key_fingerprint(keyblob, keybloblen),
					fingerprint ? fingerprint : "UNKNOWN");
	} while (1); /* keep going 'til something happens */

	/* Key doesn't exist yet */
	ask_to_confirm(keyblob, keybloblen, algoname);

	/* If we get here, they said yes */

	if (readonly) {
		TRACE(("readonly"))
		goto out;
	}

	if (!cli_opts.always_accept_key) {
		/* put the new entry in the file */
		fseek(hostsfile, 0, SEEK_END); /* In case it wasn't opened append */
		buf_setpos(line, 0);
		buf_setlen(line, 0);
		buf_putbytes(line, (const unsigned char *) cli_opts.remotehost, hostlen);
		buf_putbyte(line, ' ');
		buf_putbytes(line, (const unsigned char *) algoname, algolen);
		buf_putbyte(line, ' ');
		len = line->size - line->pos;
		/* The only failure with base64 is buffer_overflow, but buf_getwriteptr
		 * will die horribly in the case anyway */
		base64_encode(keyblob, keybloblen, buf_getwriteptr(line, len), &len);
		buf_incrwritepos(line, len);
		buf_putbyte(line, '\n');
		buf_setpos(line, 0);
		fwrite(buf_getptr(line, line->len), line->len, 1, hostsfile);
		/* We ignore errors, since there's not much we can do about them */
	}

out:
	if (hostsfile != NULL) {
		fclose(hostsfile);
	}
	if (line != NULL) {
		buf_free(line);
	}
	m_free(fingerprint);
}
