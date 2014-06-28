/*
 * Copyright (C) 2010-2012 Felix Fietkau <nbd@openwrt.org>
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
#ifndef __BLOBMSG_H
#define __BLOBMSG_H

#include <stdarg.h>
#include "blob.h"

#define BLOBMSG_ALIGN	2
#define BLOBMSG_PADDING(len) (((len) + (1 << BLOBMSG_ALIGN) - 1) & ~((1 << BLOBMSG_ALIGN) - 1))

enum blobmsg_type {
	BLOBMSG_TYPE_UNSPEC,
	BLOBMSG_TYPE_ARRAY,
	BLOBMSG_TYPE_TABLE,
	BLOBMSG_TYPE_STRING,
	BLOBMSG_TYPE_INT64,
	BLOBMSG_TYPE_INT32,
	BLOBMSG_TYPE_INT16,
	BLOBMSG_TYPE_INT8,
	__BLOBMSG_TYPE_LAST,
	BLOBMSG_TYPE_LAST = __BLOBMSG_TYPE_LAST - 1,
	BLOBMSG_TYPE_BOOL = BLOBMSG_TYPE_INT8,
};

struct blobmsg_hdr {
	uint16_t namelen;
	uint8_t name[];
} __packed;

struct blobmsg_policy {
	const char *name;
	enum blobmsg_type type;
};

static inline int blobmsg_hdrlen(int namelen)
{
	return BLOBMSG_PADDING(sizeof(struct blobmsg_hdr) + namelen + 1);
}

static inline const char *blobmsg_name(const struct blob_attr *attr)
{
	struct blobmsg_hdr *hdr = (struct blobmsg_hdr *) blob_data(attr);
	return (const char *) hdr->name;
}

static inline int blobmsg_type(const struct blob_attr *attr)
{
	return blob_id(attr);
}

static inline void *blobmsg_data(const struct blob_attr *attr)
{
	struct blobmsg_hdr *hdr = (struct blobmsg_hdr *) blob_data(attr);
	return (char *) hdr + blobmsg_hdrlen(be16_to_cpu(hdr->namelen));
}

static inline int blobmsg_data_len(const struct blob_attr *attr)
{
	uint8_t *start, *end;

	start = (uint8_t *) blob_data(attr);
	end = (uint8_t *) blobmsg_data(attr);

	return blob_len(attr) - (end - start);
}

static inline int blobmsg_len(const struct blob_attr *attr)
{
	return blobmsg_data_len(attr);
}

bool blobmsg_check_attr(const struct blob_attr *attr, bool name);
bool blobmsg_check_attr_list(const struct blob_attr *attr, int type);
int blobmsg_parse(const struct blobmsg_policy *policy, int policy_len,
                  struct blob_attr **tb, void *data, int len);
int blobmsg_parse_array(const struct blobmsg_policy *policy, int policy_len,
			struct blob_attr **tb, void *data, int len);

int blobmsg_add_field(struct blob_buf *buf, int type, const char *name,
                      const void *data, int len);

static inline int
blobmsg_add_u8(struct blob_buf *buf, const char *name, uint8_t val)
{
	return blobmsg_add_field(buf, BLOBMSG_TYPE_INT8, name, &val, 1);
}

static inline int
blobmsg_add_u16(struct blob_buf *buf, const char *name, uint16_t val)
{
	val = cpu_to_be16(val);
	return blobmsg_add_field(buf, BLOBMSG_TYPE_INT16, name, &val, 2);
}

static inline int
blobmsg_add_u32(struct blob_buf *buf, const char *name, uint32_t val)
{
	val = cpu_to_be32(val);
	return blobmsg_add_field(buf, BLOBMSG_TYPE_INT32, name, &val, 4);
}

static inline int
blobmsg_add_u64(struct blob_buf *buf, const char *name, uint64_t val)
{
	val = cpu_to_be64(val);
	return blobmsg_add_field(buf, BLOBMSG_TYPE_INT64, name, &val, 8);
}

static inline int
blobmsg_add_string(struct blob_buf *buf, const char *name, const char *string)
{
	return blobmsg_add_field(buf, BLOBMSG_TYPE_STRING, name, string, strlen(string) + 1);
}

static inline int
blobmsg_add_blob(struct blob_buf *buf, struct blob_attr *attr)
{
	return blobmsg_add_field(buf, blobmsg_type(attr), blobmsg_name(attr),
				 blobmsg_data(attr), blobmsg_data_len(attr));
}

void *blobmsg_open_nested(struct blob_buf *buf, const char *name, bool array);

static inline void *
blobmsg_open_array(struct blob_buf *buf, const char *name)
{
	return blobmsg_open_nested(buf, name, true);
}

static inline void *
blobmsg_open_table(struct blob_buf *buf, const char *name)
{
	return blobmsg_open_nested(buf, name, false);
}

static inline void
blobmsg_close_array(struct blob_buf *buf, void *cookie)
{
	blob_nest_end(buf, cookie);
}

static inline void
blobmsg_close_table(struct blob_buf *buf, void *cookie)
{
	blob_nest_end(buf, cookie);
}

static inline int blobmsg_buf_init(struct blob_buf *buf)
{
	return blob_buf_init(buf, BLOBMSG_TYPE_TABLE);
}

static inline uint8_t blobmsg_get_u8(struct blob_attr *attr)
{
	return *(uint8_t *) blobmsg_data(attr);
}

static inline bool blobmsg_get_bool(struct blob_attr *attr)
{
	return *(uint8_t *) blobmsg_data(attr);
}

static inline uint16_t blobmsg_get_u16(struct blob_attr *attr)
{
	return be16_to_cpu(*(uint16_t *) blobmsg_data(attr));
}

static inline uint32_t blobmsg_get_u32(struct blob_attr *attr)
{
	return be32_to_cpu(*(uint32_t *) blobmsg_data(attr));
}

static inline uint64_t blobmsg_get_u64(struct blob_attr *attr)
{
	uint32_t *ptr = blobmsg_data(attr);
	uint64_t tmp = ((uint64_t) be32_to_cpu(ptr[0])) << 32;
	tmp |= be32_to_cpu(ptr[1]);
	return tmp;
}

static inline char *blobmsg_get_string(struct blob_attr *attr)
{
	return blobmsg_data(attr);
}

void *blobmsg_alloc_string_buffer(struct blob_buf *buf, const char *name, int maxlen);
void *blobmsg_realloc_string_buffer(struct blob_buf *buf, int maxlen);
void blobmsg_add_string_buffer(struct blob_buf *buf);

void blobmsg_vprintf(struct blob_buf *buf, const char *name, const char *format, va_list arg);
void blobmsg_printf(struct blob_buf *buf, const char *name, const char *format, ...)
     __attribute__((format(printf, 3, 4)));


/* blobmsg to json formatting */

#define blobmsg_for_each_attr(pos, attr, rem) \
	for (rem = attr ? blobmsg_data_len(attr) : 0, \
	     pos = attr ? blobmsg_data(attr) : 0; \
	     rem > 0 && (blob_pad_len(pos) <= rem) && \
	     (blob_pad_len(pos) >= sizeof(struct blob_attr)); \
	     rem -= blob_pad_len(pos), pos = blob_next(pos))

#endif
