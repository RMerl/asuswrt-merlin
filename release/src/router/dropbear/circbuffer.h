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

#ifndef _CIRCBUFFER_H_
#define _CIRCBUFFER_H_
struct circbuf {

	unsigned int size;
	unsigned int readpos;
	unsigned int writepos;
	unsigned int used;
	unsigned char* data;
};

typedef struct circbuf circbuffer;

circbuffer * cbuf_new(unsigned int size);
void cbuf_free(circbuffer * cbuf);

unsigned int cbuf_getused(circbuffer * cbuf); /* how much data stored */
unsigned int cbuf_getavail(circbuffer * cbuf); /* how much we can write */
unsigned int cbuf_readlen(circbuffer *cbuf); /* max linear read len */
unsigned int cbuf_writelen(circbuffer *cbuf); /* max linear write len */

unsigned char* cbuf_readptr(circbuffer *cbuf, unsigned int len);
unsigned char* cbuf_writeptr(circbuffer *cbuf, unsigned int len);
void cbuf_incrwrite(circbuffer *cbuf, unsigned int len);
void cbuf_incrread(circbuffer *cbuf, unsigned int len);
#endif
