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

#include "includes.h"
#include "runopts.h"
#include "signkey.h"
#include "buffer.h"
#include "dbutil.h"
#include "auth.h"
#include "algo.h"
#include "dbrandom.h"

runopts opts; /* GLOBAL */

/* returns success or failure, and the keytype in *type. If we want
 * to restrict the type, type can contain a type to return */
int readhostkey(const char * filename, sign_key * hostkey, 
	enum signkey_type *type) {

	int ret = DROPBEAR_FAILURE;
	buffer *buf;

	buf = buf_new(MAX_PRIVKEY_SIZE);

	if (buf_readfile(buf, filename) == DROPBEAR_FAILURE) {
		goto out;
	}
	buf_setpos(buf, 0);

	addrandom(buf_getptr(buf, buf->len), buf->len);

	if (buf_get_priv_key(buf, hostkey, type) == DROPBEAR_FAILURE) {
		goto out;
	}

	ret = DROPBEAR_SUCCESS;
out:

	buf_burn(buf);
	buf_free(buf);
	return ret;
}

#ifdef ENABLE_USER_ALGO_LIST
void
parse_ciphers_macs()
{
	if (opts.cipher_list)
	{
		if (strcmp(opts.cipher_list, "help") == 0)
		{
			char *ciphers = algolist_string(sshciphers);
			dropbear_log(LOG_INFO, "Available ciphers:\n%s\n", ciphers);
			m_free(ciphers);
			dropbear_exit(".");
		}

		if (strcmp(opts.cipher_list, "none") == 0)
		{
			/* Encryption is required during authentication */
			opts.cipher_list = "none,aes128-ctr";
		}

		if (check_user_algos(opts.cipher_list, sshciphers, "cipher") == 0)
		{
			dropbear_exit("No valid ciphers specified for '-c'");
		}
	}

	if (opts.mac_list)
	{
		if (strcmp(opts.mac_list, "help") == 0)
		{
			char *macs = algolist_string(sshhashes);
			dropbear_log(LOG_INFO, "Available MACs:\n%s\n", macs);
			m_free(macs);
			dropbear_exit(".");
		}

		if (check_user_algos(opts.mac_list, sshhashes, "MAC") == 0)
		{
			dropbear_exit("No valid MACs specified for '-m'");
		}
	}
}
#endif

void print_version() {
	fprintf(stderr, "Dropbear v%s\n", DROPBEAR_VERSION);
}


