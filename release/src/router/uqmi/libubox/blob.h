/*
 * blob - library for generating/parsing tagged binary data
 *
 * Copyright (C) 2010 Felix Fietkau <nbd@openwrt.org>
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

#ifndef _BLOB_H__
#define _BLOB_H__

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "utils.h"

#define BLOB_COOKIE		0x01234567

enum {
	BLOB_ATTR_UNSPEC,
	BLOB_ATTR_NESTED,
	BLOB_ATTR_BINARY,
	BLOB_ATTR_STRING,
	BLOB_ATTR_INT8,
	BLOB_ATTR_INT16,
	BLOB_ATTR_INT32,
	BLOB_ATTR_INT64,
	BLOB_ATTR_LAST
};

#define BLOB_ATTR_ID_MASK  0xff000000
#define BLOB_ATTR_ID_SHIFT 24
#define BLOB_ATTR_LEN_MASK 0x00ffffff
#define BLOB_ATTR_ALIGN    4

struct blob_attr {
	uint32_t id_len;
	char data[];
} __packed;

struct blob_attr_info {
	unsigned int type;
	unsigned int minlen;
	unsigned int maxlen;
	bool (*validate)(const struct blob_attr_info *, struct blob_attr *);
};

struct blob_buf {
	struct blob_attr *head;
	bool (*grow)(struct blob_buf *buf, int minlen);
	int buflen;
	void *buf;
};

/*
 * blob_data: returns the data pointer for an attribute
 */
static inline void *
blob_data(const struct blob_attr *attr)
{
	return (void *) attr->data;
}

/*
 * blob_id: returns the id of an attribute
 */
static inline unsigned int
blob_id(const struct blob_attr *attr)
{
	int id = (be32_to_cpu(attr->id_len) & BLOB_ATTR_ID_MASK) >> BLOB_ATTR_ID_SHIFT;
	return id;
}

/*
 * blob_len: returns the length of the attribute's payload
 */
static inline unsigned int
blob_len(const struct blob_attr *attr)
{
	return (be32_to_cpu(attr->id_len) & BLOB_ATTR_LEN_MASK) - sizeof(struct blob_attr);
}

/*
 * blob_raw_len: returns the complete length of an attribute (including the header)
 */
static inline unsigned int
blob_raw_len(const struct blob_attr *attr)
{
	return blob_len(attr) + sizeof(struct blob_attr);
}

/*
 * blob_pad_len: returns the padded length of an attribute (including the header)
 */
static inline unsigned int
blob_pad_len(const struct blob_attr *attr)
{
	int len = blob_raw_len(attr);
	len = (len + BLOB_ATTR_ALIGN - 1) & ~(BLOB_ATTR_ALIGN - 1);
	return len;
}

static inline uint8_t
blob_get_u8(const struct blob_attr *attr)
{
	return *((uint8_t *) attr->data);
}

static inline uint16_t
blob_get_u16(const struct blob_attr *attr)
{
	uint16_t *tmp = (uint16_t*)attr->data;
	return be16_to_cpu(*tmp);
}

static inline uint32_t
blob_get_u32(const struct blob_attr *attr)
{
	uint32_t *tmp = (uint32_t*)attr->data;
	return be32_to_cpu(*tmp);
}

static inline uint64_t
blob_get_u64(const struct blob_attr *attr)
{
	uint32_t *ptr = blob_data(attr);
	uint64_t tmp = ((uint64_t) be32_to_cpu(ptr[0])) << 32;
	tmp |= be32_to_cpu(ptr[1]);
	return tmp;
}

static inline int8_t
blob_get_int8(const struct blob_attr *attr)
{
	return blob_get_u8(attr);
}

static inline int16_t
blob_get_int16(const struct blob_attr *attr)
{
	return blob_get_u16(attr);
}

static inline int32_t
blob_get_int32(const struct blob_attr *attr)
{
	return blob_get_u32(attr);
}

static inline int64_t
blob_get_int64(const struct blob_attr *attr)
{
	return blob_get_u64(attr);
}

static inline const char *
blob_get_string(const struct blob_attr *attr)
{
	return attr->data;
}

static inline struct blob_attr *
blob_next(const struct blob_attr *attr)
{
	return (struct blob_attr *) ((char *) attr + blob_pad_len(attr));
}

extern void blob_fill_pad(struct blob_attr *attr);
extern void blob_set_raw_len(struct blob_attr *attr, unsigned int len);
extern bool blob_attr_equal(const struct blob_attr *a1, const struct blob_attr *a2);
extern int blob_buf_init(struct blob_buf *buf, int id);
extern void blob_buf_free(struct blob_buf *buf);
extern void blob_buf_grow(struct blob_buf *buf, int required);
extern struct blob_attr *blob_new(struct blob_buf *buf, int id, int payload);
extern void *blob_nest_start(struct blob_buf *buf, int id);
extern void blob_nest_end(struct blob_buf *buf, void *cookie);
extern struct blob_attr *blob_put(struct blob_buf *buf, int id, const void *ptr, int len);
extern bool blob_check_type(const void *ptr, int len, int type);
extern int blob_parse(struct blob_attr *attr, struct blob_attr **data, const struct blob_attr_info *info, int max);
extern struct blob_attr *blob_memdup(struct blob_attr *attr);
extern struct blob_attr *blob_put_raw(struct blob_buf *buf, const void *ptr, int len);

static inline struct blob_attr *
blob_put_string(struct blob_buf *buf, int id, const char *str)
{
	return blob_put(buf, id, str, strlen(str) + 1);
}

static inline struct blob_attr *
blob_put_u8(struct blob_buf *buf, int id, uint8_t val)
{
	return blob_put(buf, id, &val, sizeof(val));
}

static inline struct blob_attr *
blob_put_u16(struct blob_buf *buf, int id, uint16_t val)
{
	val = cpu_to_be16(val);
	return blob_put(buf, id, &val, sizeof(val));
}

static inline struct blob_attr *
blob_put_u32(struct blob_buf *buf, int id, uint32_t val)
{
	val = cpu_to_be32(val);
	return blob_put(buf, id, &val, sizeof(val));
}

static inline struct blob_attr *
blob_put_u64(struct blob_buf *buf, int id, uint64_t val)
{
	val = cpu_to_be64(val);
	return blob_put(buf, id, &val, sizeof(val));
}

#define blob_put_int8	blob_put_u8
#define blob_put_int16	blob_put_u16
#define blob_put_int32	blob_put_u32
#define blob_put_int64	blob_put_u64

#define __blob_for_each_attr(pos, attr, rem) \
	for (pos = (void *) attr; \
	     rem > 0 && (blob_pad_len(pos) <= rem) && \
	     (blob_pad_len(pos) >= sizeof(struct blob_attr)); \
	     rem -= blob_pad_len(pos), pos = blob_next(pos))


#define blob_for_each_attr(pos, attr, rem) \
	for (rem = attr ? blob_len(attr) : 0, \
	     pos = attr ? blob_data(attr) : 0; \
	     rem > 0 && (blob_pad_len(pos) <= rem) && \
	     (blob_pad_len(pos) >= sizeof(struct blob_attr)); \
	     rem -= blob_pad_len(pos), pos = blob_next(pos))


#endif
