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

/* The format of the keyfiles is basically a raw dump of the buffer. Data types
 * are specified in the transport rfc 4253 - string is a 32-bit len then the
 * non-null-terminated string, mp_int is a 32-bit len then the bignum data.
 * The actual functions are buf_put_rsa_priv_key() and buf_put_dss_priv_key()

 * RSA:
 * string	"ssh-rsa"
 * mp_int	e
 * mp_int	n
 * mp_int	d
 * mp_int	p (newer versions only)
 * mp_int	q (newer versions only) 
 *
 * DSS:
 * string	"ssh-dss"
 * mp_int	p
 * mp_int	q
 * mp_int	g
 * mp_int	y
 * mp_int	x
 *
 */
#include "includes.h"
#include "signkey.h"
#include "buffer.h"
#include "dbutil.h"

#include "genrsa.h"
#include "gendss.h"
#include "ecdsa.h"
#include "crypto_desc.h"
#include "dbrandom.h"
#include "gensignkey.h"

static void printhelp(char * progname);


static void printpubkey(sign_key * key, int keytype);
static int printpubfile(const char* filename);

/* Print a help message */
static void printhelp(char * progname) {

	fprintf(stderr, "Usage: %s -t <type> -f <filename> [-s bits]\n"
					"-t type	Type of key to generate. One of:\n"
#ifdef DROPBEAR_RSA
					"		rsa\n"
#endif
#ifdef DROPBEAR_DSS
					"		dss\n"
#endif
#ifdef DROPBEAR_ECDSA
					"		ecdsa\n"
#endif
					"-f filename    Use filename for the secret key.\n"
					"               ~/.ssh/id_dropbear is recommended for client keys.\n"
					"-s bits	Key size in bits, should be a multiple of 8 (optional)\n"
#ifdef DROPBEAR_DSS
					"           DSS has a fixed size of 1024 bits\n"
#endif
#ifdef DROPBEAR_ECDSA
					"           ECDSA has sizes "
#ifdef DROPBEAR_ECC_256
					"256 "
#endif
#ifdef DROPBEAR_ECC_384
					"384 "
#endif
#ifdef DROPBEAR_ECC_521
					"521 "
#endif
					"\n"
#endif
					"-y		Just print the publickey and fingerprint for the\n		private key in <filename>.\n"
#ifdef DEBUG_TRACE
					"-v		verbose\n"
#endif
					,progname);
}

/* fails fatally */
static void check_signkey_bits(enum signkey_type type, int bits)
{
	switch (type) {
#ifdef DROPBEAR_RSA
		case DROPBEAR_SIGNKEY_RSA:
			if (bits < 512 || bits > 4096 || (bits % 8 != 0)) {
				dropbear_exit("Bits must satisfy 512 <= bits <= 4096, and be a"
				              " multiple of 8\n");
			}
			break;
#endif
#ifdef DROPEAR_DSS
		case DROPBEAR_SIGNKEY_DSS:
			if (bits != 1024) {
				dropbear_exit("DSS keys have a fixed size of 1024 bits\n");
				exit(EXIT_FAILURE);
			}
#endif
		default:
			(void)0; /* quiet, compiler. ecdsa handles checks itself */
	}
}

#if defined(DBMULTI_dropbearkey) || !defined(DROPBEAR_MULTI)
#if defined(DBMULTI_dropbearkey) && defined(DROPBEAR_MULTI)
int dropbearkey_main(int argc, char ** argv) {
#else
int main(int argc, char ** argv) {
#endif

	int i;
	char ** next = 0;
	char * filename = NULL;
	enum signkey_type keytype = DROPBEAR_SIGNKEY_NONE;
	char * typetext = NULL;
	char * sizetext = NULL;
	unsigned int bits = 0;
	int printpub = 0;

	crypto_init();
	seedrandom();

	/* get the commandline options */
	for (i = 1; i < argc; i++) {
		if (argv[i] == NULL) {
			continue; /* Whack */
		} 
		if (next) {
			*next = argv[i];
			next = NULL;
			continue;
		}

		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 'f':
					next = &filename;
					break;
				case 't':
					next = &typetext;
					break;
				case 's':
					next = &sizetext;
					break;
				case 'y':
					printpub = 1;
					break;
				case 'h':
					printhelp(argv[0]);
					exit(EXIT_SUCCESS);
					break;
#ifdef DEBUG_TRACE
				case 'v':
					debug_trace = 1;
					break;
#endif
				default:
					fprintf(stderr, "Unknown argument %s\n", argv[i]);
					printhelp(argv[0]);
					exit(EXIT_FAILURE);
					break;
			}
		}
	}

	if (!filename) {
		fprintf(stderr, "Must specify a key filename\n");
		printhelp(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (printpub) {
		int ret = printpubfile(filename);
		exit(ret);
	}

	/* check/parse args */
	if (!typetext) {
		fprintf(stderr, "Must specify key type\n");
		printhelp(argv[0]);
		exit(EXIT_FAILURE);
	}

#ifdef DROPBEAR_RSA
	if (strcmp(typetext, "rsa") == 0)
	{
		keytype = DROPBEAR_SIGNKEY_RSA;
	}
#endif
#ifdef DROPBEAR_DSS
	if (strcmp(typetext, "dss") == 0)
	{
		keytype = DROPBEAR_SIGNKEY_DSS;
	}
#endif
#ifdef DROPBEAR_ECDSA
	if (strcmp(typetext, "ecdsa") == 0)
	{
		keytype = DROPBEAR_SIGNKEY_ECDSA_KEYGEN;
	}
#endif

	if (keytype == DROPBEAR_SIGNKEY_NONE) {
		fprintf(stderr, "Unknown key type '%s'\n", typetext);
		printhelp(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (sizetext) {
		if (sscanf(sizetext, "%u", &bits) != 1) {
			fprintf(stderr, "Bits must be an integer\n");
			exit(EXIT_FAILURE);
		}
		
		check_signkey_bits(keytype, bits);;
	}

	fprintf(stderr, "Generating key, this may take a while...\n");
	if (signkey_generate(keytype, bits, filename, 0) == DROPBEAR_FAILURE)
	{
		dropbear_exit("Failed to generate key.\n");
	}

	printpubfile(filename);

	return EXIT_SUCCESS;
}
#endif

static int printpubfile(const char* filename) {

	buffer *buf = NULL;
	sign_key *key = NULL;
	enum signkey_type keytype;
	int ret;
	int err = DROPBEAR_FAILURE;

	buf = buf_new(MAX_PRIVKEY_SIZE);
	ret = buf_readfile(buf, filename);

	if (ret != DROPBEAR_SUCCESS) {
		fprintf(stderr, "Failed reading '%s'\n", filename);
		goto out;
	}

	key = new_sign_key();
	keytype = DROPBEAR_SIGNKEY_ANY;

	buf_setpos(buf, 0);
	ret = buf_get_priv_key(buf, key, &keytype);
	if (ret == DROPBEAR_FAILURE) {
		fprintf(stderr, "Bad key in '%s'\n", filename);
		goto out;
	}

	printpubkey(key, keytype);

	err = DROPBEAR_SUCCESS;

out:
	buf_burn(buf);
	buf_free(buf);
	buf = NULL;
	if (key) {
		sign_key_free(key);
		key = NULL;
	}
	return err;
}

static void printpubkey(sign_key * key, int keytype) {

	buffer * buf = NULL;
	unsigned char base64key[MAX_PUBKEY_SIZE*2];
	unsigned long base64len;
	int err;
	const char * typestring = NULL;
	char *fp = NULL;
	int len;
	struct passwd * pw = NULL;
	char * username = NULL;
	char hostname[100];

	buf = buf_new(MAX_PUBKEY_SIZE);
	buf_put_pub_key(buf, key, keytype);
	buf_setpos(buf, 4);

	len = buf->len - buf->pos;

	base64len = sizeof(base64key);
	err = base64_encode(buf_getptr(buf, len), len, base64key, &base64len);

	if (err != CRYPT_OK) {
		fprintf(stderr, "base64 failed");
	}

	typestring = signkey_name_from_type(keytype, NULL);

	fp = sign_key_fingerprint(buf_getptr(buf, len), len);

	/* a user@host comment is informative */
	username = "";
	pw = getpwuid(getuid());
	if (pw) {
		username = pw->pw_name;
	}

	gethostname(hostname, sizeof(hostname));
	hostname[sizeof(hostname)-1] = '\0';

	printf("Public key portion is:\n%s %s %s@%s\nFingerprint: %s\n",
			typestring, base64key, username, hostname, fp);

	m_free(fp);
	buf_free(buf);
}
