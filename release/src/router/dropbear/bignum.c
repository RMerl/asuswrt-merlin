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

/* Contains helper functions for mp_int handling */

#include "includes.h"
#include "dbutil.h"

/* wrapper for mp_init, failing fatally on errors (memory allocation) */
void m_mp_init(mp_int *mp) {

	if (mp_init(mp) != MP_OKAY) {
		dropbear_exit("Mem alloc error");
	}
}

/* simplified duplication of bn_mp_multi's mp_init_multi, but die fatally
 * on error */
void m_mp_init_multi(mp_int *mp, ...) 
{
	mp_int* cur_arg = mp;
	va_list args;

	va_start(args, mp);        /* init args to next argument from caller */
	while (cur_arg != NULL) {
		if (mp_init(cur_arg) != MP_OKAY) {
			dropbear_exit("Mem alloc error");
		}
		cur_arg = va_arg(args, mp_int*);
	}
	va_end(args);
}

void m_mp_alloc_init_multi(mp_int **mp, ...) 
{
	mp_int** cur_arg = mp;
	va_list args;

	va_start(args, mp);        /* init args to next argument from caller */
	while (cur_arg != NULL) {
		*cur_arg = m_malloc(sizeof(mp_int));
		if (mp_init(*cur_arg) != MP_OKAY) {
			dropbear_exit("Mem alloc error");
		}
		cur_arg = va_arg(args, mp_int**);
	}
	va_end(args);
}

void bytes_to_mp(mp_int *mp, const unsigned char* bytes, unsigned int len) {

	if (mp_read_unsigned_bin(mp, (unsigned char*)bytes, len) != MP_OKAY) {
		dropbear_exit("Mem alloc error");
	}
}

/* hash the ssh representation of the mp_int mp */
void hash_process_mp(const struct ltc_hash_descriptor *hash_desc, 
				hash_state *hs, mp_int *mp) {
	buffer * buf;

	buf = buf_new(512 + 20); /* max buffer is a 4096 bit key, 
								plus header + some leeway*/
	buf_putmpint(buf, mp);
	hash_desc->process(hs, buf->data, buf->len);
	buf_free(buf);
}
