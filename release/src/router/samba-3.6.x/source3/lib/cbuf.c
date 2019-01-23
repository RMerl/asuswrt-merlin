/*
 * Samba Unix/Linux SMB client library
 *
 * Copyright (C) Gregor Beck 2010
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file   cbuf.c
 * @author Gregor Beck <gb@sernet.de>
 * @date   Aug 2010
 *
 * @brief  A talloced character buffer.
 *
 */


#include "includes.h"
#include "cbuf.h"


struct cbuf {
	char*  buf;
	size_t pos;
	size_t size;
};


cbuf* cbuf_clear(cbuf* b)
{
	cbuf_setpos(b, 0);
	return b;
}

cbuf* cbuf_new(const void* ctx)
{
	cbuf* s = talloc(ctx, cbuf);
	if (s == NULL)
		return NULL;
	s->size = 32;
	s->buf  = (char *)talloc_size(s, s->size);
	if (s->size && (s->buf == NULL)) {
		talloc_free(s);
		return NULL;
	}
	return cbuf_clear(s);
}

cbuf* cbuf_copy(const cbuf* b)
{
	cbuf* s = talloc(talloc_parent(b), cbuf);
	if (s == NULL) {
		return NULL;
	}

	s->buf = (char *)talloc_memdup(s, b->buf, b->size); /* only up to pos? */

	/* XXX shallow did not work, because realloc */
	/* fails with multiple references */
	/* s->buf = talloc_reference(s, b->buf); */

	if (s->buf == NULL) {
		cbuf_delete(s);
		return NULL;
	}
	s->size = b->size;
	s->pos  = b->pos;
	return s;
}

void cbuf_delete(cbuf* b)
{
	talloc_free(b);
}

#define SWAP(A,B,T) do {			\
		T tmp = A; A = B; B = tmp;	\
	} while(0)


void cbuf_swap(cbuf* b1, cbuf* b2)
{
	if (b1 == b2) {
		return;
	}
	talloc_reparent(b1, b2, b1->buf);
	talloc_reparent(b2, b1, b2->buf);
	SWAP(b1->buf,  b2->buf, char*);
	SWAP(b1->pos,  b2->pos, size_t);
	SWAP(b1->size, b2->size, size_t);
}



cbuf* cbuf_takeover(cbuf* b1, cbuf* b2)
{
	talloc_reparent(b2, b1, b2->buf);
	b1->buf = b2->buf;
	b1->pos = b2->pos;
	b1->size = b2->size;
	cbuf_delete(b2);
	return b1;
}

cbuf* cbuf_swapptr(cbuf* b, char** ptr, size_t len)
{
	void* p = talloc_parent(*ptr);
	SWAP(b->buf, *ptr, char*);
	talloc_steal(b, b->buf);
	talloc_steal(p, *ptr);
	b->size = talloc_get_size(b->buf);
	b->pos  = (len == -1) ? strlen(b->buf) : len;

	assert(b->pos <= b->size);
	return b;
}

cbuf* cbuf_resize(cbuf* b, size_t size)
{
	char* save_buf = b->buf;
	b->buf = talloc_realloc(b, b->buf, char, size);
	if (b->buf == NULL) {
		talloc_free(save_buf);
		b->size = 0;
	} else {
		b->size = size;
	}
	b->pos  = MIN(b->pos, b->size);
	return b->buf ? b : NULL;
}

char* cbuf_reserve(cbuf* b, size_t len)
{
	if(b->size < b->pos + len)
		cbuf_resize(b, MAX(2*b->size, b->pos + len));
	return b->buf ? b->buf + b->pos : NULL;
}

int cbuf_puts(cbuf* b, const char* str, size_t len)
{
	char* dst;

	if (b == NULL)
		return 0;

	if (len == -1) {
		len=strlen(str);
	}

	dst = cbuf_reserve(b, len+1);
	if (dst == NULL)
		return -1;

	memcpy(dst, str, len);
	dst[len] = '\0'; /* just to ease debugging */

	b->pos += len;
	assert(b->pos < b->size);

	return len;
}

int cbuf_putc(cbuf* b, char c) {
	char* dst;

	if (b == NULL)
		return 0;

	dst = cbuf_reserve(b, 2);
	if (dst == NULL) {
		return -1;
	}

	dst[0] = c;
	dst[1] = '\0'; /* just to ease debugging */

	b->pos++;
	assert(b->pos < b->size);

	return 1;
}

int cbuf_putdw(cbuf* b, uint32_t u) {
	char* dst;
	static const size_t LEN = sizeof(uint32_t);

	if (b == NULL)
		return 0;

	dst = cbuf_reserve(b, LEN);
	if (dst == NULL) {
		return -1;
	}

	SIVAL(dst, 0, u);

	b->pos += LEN;
	assert(b->pos <= b->size); /* no NULL termination*/

	return LEN;
}

size_t cbuf_getpos(const cbuf* b) {
	assert(b->pos <= b->size);
	return b->pos;
}

void cbuf_setpos(cbuf* b, size_t pos) {
	assert(pos <= b->size);
	b->pos = pos;
	if (pos < b->size)
		b->buf[pos] = '\0'; /* just to ease debugging */
}

char* cbuf_gets(cbuf* b, size_t idx) {
	assert(idx <= b->pos);

	if (cbuf_reserve(b, 1) == NULL)
		return NULL;

	b->buf[b->pos] = '\0';
	return b->buf + idx;
}

int cbuf_printf(cbuf* b, const char* fmt, ...)
{
	va_list args, args2;
	int len;
	char* dst = b->buf + b->pos;
	const int avail = b->size - b->pos;
	assert(avail >= 0);

	va_start(args, fmt);
	va_copy(args2, args);

	len = vsnprintf(dst, avail, fmt, args);

	if (len >= avail) {
		dst = cbuf_reserve(b, len+1);
		len = (dst != NULL) ? vsnprintf(dst, len+1, fmt, args2) : -1;
	}

	if (len > 0) {
		b->pos += len;
	}

	va_end(args);
	va_end(args2);
	assert(b->pos <= b->size);

	return len;
}

int cbuf_print_quoted_string(cbuf* ost, const char* s)
{
	int n = 1;
	cbuf_putc(ost,'"');

	while(true) {
		switch (*s) {
		case '\0':
			cbuf_putc(ost, '"');
			return n+1;

		case '"':
		case '\\':
			cbuf_putc(ost, '\\');
		        n++;
			/* no break */
		default:
			cbuf_putc(ost, *s);
			n++;
		}
		s++;
	}
}


int cbuf_print_quoted(cbuf* ost, const char* s, size_t len)
{
	int n = 1;
	int ret;
	cbuf_reserve(ost, len+2);

	cbuf_putc(ost,'"');

	while(len--) {
		switch (*s) {
		case '"':
		case '\\':
			ret = cbuf_printf(ost, "\\%c", *s);
			break;
		default:
			if (isprint(*s) && ((*s == ' ') || !isspace(*s))) {
				ret = cbuf_putc(ost, *s);
			} else {
				ret = cbuf_printf(ost, "\\%02x", *s);
			}
		}
		s++;
		if (ret == -1) {
			return -1;
		}
		n += ret;
	}
	ret = cbuf_putc(ost,'"');

	return (ret == -1) ? -1 : (n + ret);
}
