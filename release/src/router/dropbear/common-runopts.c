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

runopts opts; /* GLOBAL */

/* returns success or failure, and the keytype in *type. If we want
 * to restrict the type, type can contain a type to return */
int readhostkey(const char * filename, sign_key * hostkey, int *type) {

	int ret = DROPBEAR_FAILURE;
	buffer *buf;

	buf = buf_new(MAX_PRIVKEY_SIZE);

	if (buf_readfile(buf, filename) == DROPBEAR_FAILURE) {
		goto out;
	}
	buf_setpos(buf, 0);
	if (buf_get_priv_key(buf, hostkey, type) == DROPBEAR_FAILURE) {
		goto out;
	}

	ret = DROPBEAR_SUCCESS;
out:

	buf_burn(buf);
	buf_free(buf);
	return ret;
}
