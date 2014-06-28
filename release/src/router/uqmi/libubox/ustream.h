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

#ifndef __USTREAM_H
#define __USTREAM_H

#include <stdarg.h>
#include "uloop.h"

struct ustream;
struct ustream_buf;

enum read_blocked_reason {
	READ_BLOCKED_USER = (1 << 0),
	READ_BLOCKED_FULL = (1 << 1),
};

struct ustream_buf_list {
	struct ustream_buf *head;
	struct ustream_buf *data_tail;
	struct ustream_buf *tail;

	int (*alloc)(struct ustream *s, struct ustream_buf_list *l);

	int data_bytes;

	int min_buffers;
	int max_buffers;
	int buffer_len;

	int buffers;
};

struct ustream {
	struct ustream_buf_list r, w;
	struct uloop_timeout state_change;
	struct ustream *next;

	/*
	 * notify_read: (optional)
	 * called by the ustream core to notify that new data is available
	 * for reading.
	 * must not free the ustream from this callback
	 */
	void (*notify_read)(struct ustream *s, int bytes_new);

	/*
	 * notify_write: (optional)
	 * called by the ustream core to notify that some buffered data has
	 * been written to the stream.
	 * must not free the ustream from this callback
	 */
	void (*notify_write)(struct ustream *s, int bytes);

	/*
	 * notify_state: (optional)
	 * called by the ustream implementation to notify that the read
	 * side of the stream is closed (eof is set) or there was a write
	 * error (write_error is set).
	 * will be called again after the write buffer has been emptied when
	 * the read side has hit EOF.
	 */
	void (*notify_state)(struct ustream *s);

	/*
	 * write:
	 * must be defined by ustream implementation, accepts new write data.
	 * 'more' is used to indicate that a subsequent call will provide more
	 * data (useful for aggregating writes)
	 * returns the number of bytes accepted, or -1 if no more writes can
	 * be accepted (link error)
	 */
	int (*write)(struct ustream *s, const char *buf, int len, bool more);

	/*
	 * free: (optional)
	 * defined by ustream implementation, tears down the ustream and frees data
	 */
	void (*free)(struct ustream *s);

	/*
	 * set_read_blocked: (optional)
	 * defined by ustream implementation, called when the read_blocked flag
	 * changes
	 */
	void (*set_read_blocked)(struct ustream *s);

	/*
	 * poll: (optional)
	 * defined by the upstream implementation, called to request polling for
	 * available data.
	 * returns true if data was fetched.
	 */
	bool (*poll)(struct ustream *s);

	/*
	 * ustream user should set this if the input stream is expected
	 * to contain string data. the core will keep all data 0-terminated.
	 */
	bool string_data;
	bool write_error;
	bool eof, eof_write_done;

	enum read_blocked_reason read_blocked;
};

struct ustream_fd {
	struct ustream stream;
	struct uloop_fd fd;
};

struct ustream_buf {
	struct ustream_buf *next;

	char *data;
	char *tail;
	char *end;

	char head[];
};

/* ustream_fd_init: create a file descriptor ustream (uses uloop) */
void ustream_fd_init(struct ustream_fd *s, int fd);

/* ustream_free: free all buffers and data associated with a ustream */
void ustream_free(struct ustream *s);

/* ustream_consume: remove data from the head of the read buffer */
void ustream_consume(struct ustream *s, int len);

/* ustream_write: add data to the write buffer */
int ustream_write(struct ustream *s, const char *buf, int len, bool more);
int ustream_printf(struct ustream *s, const char *format, ...);
int ustream_vprintf(struct ustream *s, const char *format, va_list arg);

/* ustream_get_read_buf: get a pointer to the next read buffer data */
char *ustream_get_read_buf(struct ustream *s, int *buflen);

/*
 * ustream_set_read_blocked: set read blocked state
 *
 * if set, the ustream will no longer fetch pending data.
 */
void ustream_set_read_blocked(struct ustream *s, bool set);

static inline bool ustream_read_blocked(struct ustream *s)
{
	return !!(s->read_blocked & READ_BLOCKED_USER);
}

static inline int ustream_pending_data(struct ustream *s, bool write)
{
	struct ustream_buf_list *b = write ? &s->w : &s->r;
	return b->data_bytes;
}

static inline bool ustream_read_buf_full(struct ustream *s)
{
	struct ustream_buf *buf = s->r.data_tail;
	return buf && buf->data == buf->head && buf->tail == buf->end &&
	       s->r.buffers == s->r.max_buffers;
}

/*** --- functions only used by ustream implementations --- ***/

/* ustream_init_defaults: fill default callbacks and options */
void ustream_init_defaults(struct ustream *s);

/*
 * ustream_reserve: allocate rx buffer space
 *
 * len: hint for how much space is needed (not guaranteed to be met)
 * maxlen: pointer to where the actual buffer size is going to be stored
 */
char *ustream_reserve(struct ustream *s, int len, int *maxlen);

/* ustream_fill_read: mark rx buffer space as filled */
void ustream_fill_read(struct ustream *s, int len);

/*
 * ustream_write_pending: attempt to write more data from write buffers
 * returns true if all write buffers have been emptied.
 */
bool ustream_write_pending(struct ustream *s);

static inline void ustream_state_change(struct ustream *s)
{
	uloop_timeout_set(&s->state_change, 0);
}

static inline bool ustream_poll(struct ustream *s)
{
	if (!s->poll)
		return false;

	return s->poll(s);
}

#endif
