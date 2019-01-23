/*
 * Dropbear SSH
 * 
 * Copyright (c) 2002-2004 Matt Johnston
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
#include "circbuffer.h"

#define MAX_CBUF_SIZE 100000000

circbuffer * cbuf_new(unsigned int size) {

	circbuffer *cbuf = NULL;

	if (size > MAX_CBUF_SIZE) {
		dropbear_exit("Bad cbuf size");
	}

	cbuf = (circbuffer*)m_malloc(sizeof(circbuffer));
	/* data is malloced on first write */
	cbuf->data = NULL;
	cbuf->used = 0;
	cbuf->readpos = 0;
	cbuf->writepos = 0;
	cbuf->size = size;

	return cbuf;
}

void cbuf_free(circbuffer * cbuf) {

	if (cbuf->data) {
		m_burn(cbuf->data, cbuf->size);
		m_free(cbuf->data);
	}
	m_free(cbuf);
}

unsigned int cbuf_getused(circbuffer * cbuf) {

	return cbuf->used;

}

unsigned int cbuf_getavail(circbuffer * cbuf) {

	return cbuf->size - cbuf->used;

}

unsigned int cbuf_writelen(circbuffer *cbuf) {

	dropbear_assert(cbuf->used <= cbuf->size);
	dropbear_assert(((2*cbuf->size)+cbuf->writepos-cbuf->readpos)%cbuf->size == cbuf->used%cbuf->size);
	dropbear_assert(((2*cbuf->size)+cbuf->readpos-cbuf->writepos)%cbuf->size == (cbuf->size-cbuf->used)%cbuf->size);

	if (cbuf->used == cbuf->size) {
		TRACE(("cbuf_writelen: full buffer"))
		return 0; /* full */
	}
	
	if (cbuf->writepos < cbuf->readpos) {
		return cbuf->readpos - cbuf->writepos;
	}

	return cbuf->size - cbuf->writepos;
}

void cbuf_readptrs(circbuffer *cbuf, 
	unsigned char **p1, unsigned int *len1, 
	unsigned char **p2, unsigned int *len2) {
	*p1 = &cbuf->data[cbuf->readpos];
	*len1 = MIN(cbuf->used, cbuf->size - cbuf->readpos);

	if (*len1 < cbuf->used) {
		*p2 = cbuf->data;
		*len2 = cbuf->used - *len1;
	} else {
		*p2 = NULL;
		*len2 = 0;
	}
}

unsigned char* cbuf_writeptr(circbuffer *cbuf, unsigned int len) {

	if (len > cbuf_writelen(cbuf)) {
		dropbear_exit("Bad cbuf write");
	}

	if (!cbuf->data) {
		/* lazy allocation */
		cbuf->data = (unsigned char*)m_malloc(cbuf->size);
	}

	return &cbuf->data[cbuf->writepos];
}

void cbuf_incrwrite(circbuffer *cbuf, unsigned int len) {
	if (len > cbuf_writelen(cbuf)) {
		dropbear_exit("Bad cbuf write");
	}

	cbuf->used += len;
	dropbear_assert(cbuf->used <= cbuf->size);
	cbuf->writepos = (cbuf->writepos + len) % cbuf->size;
}


void cbuf_incrread(circbuffer *cbuf, unsigned int len) {
	dropbear_assert(cbuf->used >= len);
	cbuf->used -= len;
	cbuf->readpos = (cbuf->readpos + len) % cbuf->size;
}
