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

#ifndef _BUFFER_H_

#define _BUFFER_H_

#include "includes.h"

struct buf {

	unsigned char * data;
	unsigned int len; /* the used size */
	unsigned int pos;
	unsigned int size; /* the memory size */

};

typedef struct buf buffer;

buffer * buf_new(unsigned int size);
void buf_resize(buffer *buf, unsigned int newsize);
void buf_free(buffer* buf);
void buf_burn(buffer* buf);
buffer* buf_newcopy(buffer* buf);
void buf_setlen(buffer* buf, unsigned int len);
void buf_incrlen(buffer* buf, unsigned int incr);
void buf_setpos(buffer* buf, unsigned int pos);
void buf_incrpos(buffer* buf, int incr); /* -ve is ok, to go backwards */
void buf_incrwritepos(buffer* buf, unsigned int incr);
unsigned char buf_getbyte(buffer* buf);
unsigned char buf_getbool(buffer* buf);
void buf_putbyte(buffer* buf, unsigned char val);
unsigned char* buf_getptr(buffer* buf, unsigned int len);
unsigned char* buf_getwriteptr(buffer* buf, unsigned int len);
unsigned char* buf_getstring(buffer* buf, unsigned int *retlen);
buffer * buf_getstringbuf(buffer *buf);
void buf_eatstring(buffer *buf);
void buf_putint(buffer* buf, unsigned int val);
void buf_putstring(buffer* buf, const unsigned char* str, unsigned int len);
void buf_putbytes(buffer *buf, const unsigned char *bytes, unsigned int len);
void buf_putmpint(buffer* buf, mp_int * mp);
int buf_getmpint(buffer* buf, mp_int* mp);
unsigned int buf_getint(buffer* buf);

#endif /* _BUFFER_H_ */
