/*
 * ustream - library for stream buffer management
 *
 * Copyright (C) 2012 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

#include "ustream.h"

static void ustream_init_buf(struct ustream_buf *buf, int len)
{
	if (!len)
		abort();

	memset(buf, 0, sizeof(*buf));
	buf->data = buf->tail = buf->head;
	buf->end = buf->head + len;
	*buf->head = 0;
}

static void ustream_add_buf(struct ustream_buf_list *l, struct ustream_buf *buf)
{
	l->buffers++;
	if (!l->tail)
		l->head = buf;
	else
		l->tail->next = buf;

	buf->next = NULL;
	l->tail = buf;
	if (!l->data_tail)
		l->data_tail = l->head;
}

static bool ustream_can_alloc(struct ustream_buf_list *l)
{
	if (l->max_buffers <= 0)
		return true;

	return (l->buffers < l->max_buffers);
}

static int ustream_alloc_default(struct ustream *s, struct ustream_buf_list *l)
{
	struct ustream_buf *buf;

	if (!ustream_can_alloc(l))
		return -1;

	buf = malloc(sizeof(*buf) + l->buffer_len + s->string_data);
	ustream_init_buf(buf, l->buffer_len);
	ustream_add_buf(l, buf);

	return 0;
}

static void ustream_free_buffers(struct ustream_buf_list *l)
{
	struct ustream_buf *buf = l->head;

	while (buf) {
		struct ustream_buf *next = buf->next;

		free(buf);
		buf = next;
	}
	l->head = NULL;
	l->tail = NULL;
	l->data_tail = NULL;
}

void ustream_free(struct ustream *s)
{
	if (s->free)
		s->free(s);

	uloop_timeout_cancel(&s->state_change);
	ustream_free_buffers(&s->r);
	ustream_free_buffers(&s->w);
}

static void ustream_state_change_cb(struct uloop_timeout *t)
{
	struct ustream *s = container_of(t, struct ustream, state_change);

	if (s->write_error)
		ustream_free_buffers(&s->w);
	if (s->notify_state)
		s->notify_state(s);
}

void ustream_init_defaults(struct ustream *s)
{
#define DEFAULT_SET(_f, _default)	\
	do {				\
		if (!_f)		\
			_f = _default;	\
	} while(0)

	DEFAULT_SET(s->r.alloc, ustream_alloc_default);
	DEFAULT_SET(s->w.alloc, ustream_alloc_default);

	DEFAULT_SET(s->r.min_buffers, 1);
	DEFAULT_SET(s->r.max_buffers, 1);
	DEFAULT_SET(s->r.buffer_len, 4096);

	DEFAULT_SET(s->w.min_buffers, 2);
	DEFAULT_SET(s->w.max_buffers, -1);
	DEFAULT_SET(s->w.buffer_len, 256);

#undef DEFAULT_SET

	s->state_change.cb = ustream_state_change_cb;
	s->write_error = false;
	s->eof = false;
	s->eof_write_done = false;
	s->read_blocked = 0;

	s->r.buffers = 0;
	s->r.data_bytes = 0;

	s->w.buffers = 0;
	s->w.data_bytes = 0;
}

static bool ustream_should_move(struct ustream_buf_list *l, struct ustream_buf *buf, int len)
{
	int maxlen;
	int offset;

	if (buf->data == buf->head)
		return false;

	maxlen = buf->end - buf->head;
	offset = buf->data - buf->head;

	if (offset > maxlen / 2)
		return true;

	if (buf->tail - buf->data < 32 && offset > maxlen / 4)
		return true;

	if (buf != l->tail || ustream_can_alloc(l))
		return false;

	return (buf->end - buf->tail < len);
}

static void ustream_free_buf(struct ustream_buf_list *l, struct ustream_buf *buf)
{
	if (buf == l->head)
		l->head = buf->next;

	if (buf == l->data_tail)
		l->data_tail = buf->next;

	if (buf == l->tail)
		l->tail = NULL;

	if (--l->buffers >= l->min_buffers) {
		free(buf);
		return;
	}

	/* recycle */
	ustream_init_buf(buf, buf->end - buf->head);
	ustream_add_buf(l, buf);
}

static void __ustream_set_read_blocked(struct ustream *s, unsigned char val)
{
	bool changed = !!s->read_blocked != !!val;

	s->read_blocked = val;
	if (changed)
		s->set_read_blocked(s);
}

void ustream_set_read_blocked(struct ustream *s, bool set)
{
	unsigned char val = s->read_blocked & ~READ_BLOCKED_USER;

	if (set)
		val |= READ_BLOCKED_USER;

	__ustream_set_read_blocked(s, val);
}

void ustream_consume(struct ustream *s, int len)
{
	struct ustream_buf *buf = s->r.head;

	if (!len)
		return;

	s->r.data_bytes -= len;
	if (s->r.data_bytes < 0)
		abort();

	do {
		struct ustream_buf *next = buf->next;
		int buf_len = buf->tail - buf->data;

		if (len < buf_len) {
			buf->data += len;
			break;
		}

		len -= buf_len;
		ustream_free_buf(&s->r, buf);
		buf = next;
	} while(len);

	__ustream_set_read_blocked(s, s->read_blocked & ~READ_BLOCKED_FULL);
}

static void ustream_fixup_string(struct ustream *s, struct ustream_buf *buf)
{
	if (!s->string_data)
		return;

	*buf->tail = 0;
}

static bool ustream_prepare_buf(struct ustream *s, struct ustream_buf_list *l, int len)
{
	struct ustream_buf *buf;

	buf = l->data_tail;
	if (buf) {
		if (ustream_should_move(l, buf, len)) {
			int len = buf->tail - buf->data;

			memmove(buf->head, buf->data, len);
			buf->data = buf->head;
			buf->tail = buf->data + len;

			if (l == &s->r)
				ustream_fixup_string(s, buf);
		}
		if (buf->tail != buf->end)
			return true;
	}

	if (buf && buf->next) {
		l->data_tail = buf->next;
		return true;
	}

	if (!ustream_can_alloc(l))
		return false;

	if (l->alloc(s, l) < 0)
		return false;

	l->data_tail = l->tail;
	return true;
}

char *ustream_reserve(struct ustream *s, int len, int *maxlen)
{
	struct ustream_buf *buf = s->r.head;

	if (!ustream_prepare_buf(s, &s->r, len)) {
		__ustream_set_read_blocked(s, s->read_blocked | READ_BLOCKED_FULL);
		*maxlen = 0;
		return NULL;
	}

	buf = s->r.data_tail;
	*maxlen = buf->end - buf->tail;
	return buf->tail;
}

void ustream_fill_read(struct ustream *s, int len)
{
	struct ustream_buf *buf = s->r.data_tail;
	int n = len;
	int maxlen;

	s->r.data_bytes += len;
	do {
		if (!buf)
			abort();

		maxlen = buf->end - buf->tail;
		if (len < maxlen)
			maxlen = len;

		len -= maxlen;
		buf->tail += maxlen;
		ustream_fixup_string(s, buf);

		s->r.data_tail = buf;
		buf = buf->next;
	} while (len);

	if (s->notify_read)
		s->notify_read(s, n);
}

char *ustream_get_read_buf(struct ustream *s, int *buflen)
{
	char *data = NULL;
	int len = 0;

	if (s->r.head) {
		len = s->r.head->tail - s->r.head->data;
		if (len > 0)
			data = s->r.head->data;
	}

	if (buflen)
		*buflen = len;

	return data;
}

static void ustream_write_error(struct ustream *s)
{
	if (!s->write_error)
		ustream_state_change(s);
	s->write_error = true;
}

bool ustream_write_pending(struct ustream *s)
{
	struct ustream_buf *buf = s->w.head;
	int wr = 0, len;

	if (s->write_error)
		return false;

	while (buf && s->w.data_bytes) {
		struct ustream_buf *next = buf->next;
		int maxlen = buf->tail - buf->data;

		len = s->write(s, buf->data, maxlen, !!buf->next);
		if (len < 0) {
			ustream_write_error(s);
			break;
		}

		if (len == 0)
			break;

		wr += len;
		s->w.data_bytes -= len;
		if (len < maxlen) {
			buf->data += len;
			break;
		}

		ustream_free_buf(&s->w, buf);
		buf = next;
	}

	if (s->notify_write)
		s->notify_write(s, wr);

	if (s->eof && wr && !s->w.data_bytes)
		ustream_state_change(s);

	return !s->w.data_bytes;
}

static int ustream_write_buffered(struct ustream *s, const char *data, int len, int wr)
{
	struct ustream_buf_list *l = &s->w;
	struct ustream_buf *buf;
	int maxlen;

	while (len) {
		if (!ustream_prepare_buf(s, &s->w, len))
			break;

		buf = l->data_tail;

		maxlen = buf->end - buf->tail;
		if (maxlen > len)
			maxlen = len;

		memcpy(buf->tail, data, maxlen);
		buf->tail += maxlen;
		data += maxlen;
		len -= maxlen;
		wr += maxlen;
		l->data_bytes += maxlen;
	}

	return wr;
}

int ustream_write(struct ustream *s, const char *data, int len, bool more)
{
	struct ustream_buf_list *l = &s->w;
	int wr = 0;

	if (s->write_error)
		return 0;

	if (!l->data_bytes) {
		wr = s->write(s, data, len, more);
		if (wr == len)
			return wr;

		if (wr < 0) {
			ustream_write_error(s);
			return wr;
		}

		data += wr;
		len -= wr;
	}

	return ustream_write_buffered(s, data, len, wr);
}

#define MAX_STACK_BUFLEN	256

int ustream_vprintf(struct ustream *s, const char *format, va_list arg)
{
	struct ustream_buf_list *l = &s->w;
	char *buf;
	va_list arg2;
	int wr, maxlen, buflen;

	if (s->write_error)
		return 0;

	if (!l->data_bytes) {
		buf = alloca(MAX_STACK_BUFLEN);
		va_copy(arg2, arg);
		maxlen = vsnprintf(buf, MAX_STACK_BUFLEN, format, arg2);
		va_end(arg2);
		if (maxlen < MAX_STACK_BUFLEN) {
			wr = s->write(s, buf, maxlen, false);
			if (wr < 0) {
				ustream_write_error(s);
				return wr;
			}
			if (wr == maxlen)
				return wr;

			buf += wr;
			maxlen -= wr;
			return ustream_write_buffered(s, buf, maxlen, wr);
		} else {
			buf = malloc(maxlen + 1);
			wr = vsnprintf(buf, maxlen + 1, format, arg);
			wr = ustream_write(s, buf, wr, false);
			free(buf);
			return wr;
		}
	}

	if (!ustream_prepare_buf(s, l, 1))
		return 0;

	buf = l->data_tail->tail;
	buflen = l->data_tail->end - buf;

	va_copy(arg2, arg);
	maxlen = vsnprintf(buf, buflen, format, arg2);
	va_end(arg2);

	wr = maxlen;
	if (wr >= buflen)
		wr = buflen - 1;

	l->data_tail->tail += wr;
	l->data_bytes += wr;
	if (maxlen < buflen)
		return wr;

	buf = malloc(maxlen + 1);
	maxlen = vsnprintf(buf, maxlen + 1, format, arg);
	wr = ustream_write_buffered(s, buf + wr, maxlen - wr, wr);
	free(buf);

	return wr;
}

int ustream_printf(struct ustream *s, const char *format, ...)
{
	va_list arg;
	int ret;

	if (s->write_error)
		return 0;

	va_start(arg, format);
	ret = ustream_vprintf(s, format, arg);
	va_end(arg);

	return ret;
}
