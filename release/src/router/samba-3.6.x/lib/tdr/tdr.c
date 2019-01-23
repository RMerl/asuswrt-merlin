/* 
   Unix SMB/CIFS implementation.

   TDR (Trivial Data Representation) helper functions
     Based loosely on ndr.c by Andrew Tridgell.

   Copyright (C) Jelmer Vernooij 2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/filesys.h"
#include "system/network.h"
#include "lib/tdr/tdr.h"

#define TDR_BASE_MARSHALL_SIZE 1024

#define TDR_PUSH_NEED_BYTES(tdr, n) TDR_CHECK(tdr_push_expand(tdr, tdr->data.length+(n)))

#define TDR_PULL_NEED_BYTES(tdr, n) do { \
	if ((n) > tdr->data.length || tdr->offset + (n) > tdr->data.length) { \
		return NT_STATUS_BUFFER_TOO_SMALL; \
	} \
} while(0)

#define TDR_BE(tdr) ((tdr)->flags & TDR_BIG_ENDIAN)

#define TDR_CVAL(tdr, ofs) CVAL(tdr->data.data,ofs)
#define TDR_SVAL(tdr, ofs) (TDR_BE(tdr)?RSVAL(tdr->data.data,ofs):SVAL(tdr->data.data,ofs))
#define TDR_IVAL(tdr, ofs) (TDR_BE(tdr)?RIVAL(tdr->data.data,ofs):IVAL(tdr->data.data,ofs))
#define TDR_SCVAL(tdr, ofs, v) SCVAL(tdr->data.data,ofs,v)
#define TDR_SSVAL(tdr, ofs, v) do { if (TDR_BE(tdr))  { RSSVAL(tdr->data.data,ofs,v); } else SSVAL(tdr->data.data,ofs,v); } while (0)
#define TDR_SIVAL(tdr, ofs, v) do { if (TDR_BE(tdr))  { RSIVAL(tdr->data.data,ofs,v); } else SIVAL(tdr->data.data,ofs,v); } while (0)

/**
  expand the available space in the buffer to 'size'
*/
NTSTATUS tdr_push_expand(struct tdr_push *tdr, uint32_t size)
{
	if (talloc_get_size(tdr->data.data) >= size) {
		return NT_STATUS_OK;
	}

	tdr->data.data = talloc_realloc(tdr, tdr->data.data, uint8_t, tdr->data.length + TDR_BASE_MARSHALL_SIZE);

	if (tdr->data.data == NULL)
		return NT_STATUS_NO_MEMORY;

	return NT_STATUS_OK;
}


NTSTATUS tdr_pull_uint8(struct tdr_pull *tdr, TALLOC_CTX *ctx, uint8_t *v)
{
	TDR_PULL_NEED_BYTES(tdr, 1);
	*v = TDR_CVAL(tdr, tdr->offset);
	tdr->offset += 1;
	return NT_STATUS_OK;
}

NTSTATUS tdr_push_uint8(struct tdr_push *tdr, const uint8_t *v)
{
	TDR_PUSH_NEED_BYTES(tdr, 1);
	TDR_SCVAL(tdr, tdr->data.length, *v);
	tdr->data.length += 1;
	return NT_STATUS_OK;
}

NTSTATUS tdr_print_uint8(struct tdr_print *tdr, const char *name, uint8_t *v)
{
	tdr->print(tdr, "%-25s: 0x%02x (%u)", name, *v, *v);
	return NT_STATUS_OK;
}

NTSTATUS tdr_pull_uint16(struct tdr_pull *tdr, TALLOC_CTX *ctx, uint16_t *v)
{
	TDR_PULL_NEED_BYTES(tdr, 2);
	*v = TDR_SVAL(tdr, tdr->offset);
	tdr->offset += 2;
	return NT_STATUS_OK;
}

NTSTATUS tdr_pull_uint1632(struct tdr_pull *tdr, TALLOC_CTX *ctx, uint16_t *v)
{
	return tdr_pull_uint16(tdr, ctx, v);
}

NTSTATUS tdr_push_uint16(struct tdr_push *tdr, const uint16_t *v)
{
	TDR_PUSH_NEED_BYTES(tdr, 2);
	TDR_SSVAL(tdr, tdr->data.length, *v);
	tdr->data.length += 2;
	return NT_STATUS_OK;
}

NTSTATUS tdr_push_uint1632(struct tdr_push *tdr, const uint16_t *v)
{
	return tdr_push_uint16(tdr, v);
}

NTSTATUS tdr_print_uint16(struct tdr_print *tdr, const char *name, uint16_t *v)
{
	tdr->print(tdr, "%-25s: 0x%02x (%u)", name, *v, *v);
	return NT_STATUS_OK;
}

NTSTATUS tdr_pull_uint32(struct tdr_pull *tdr, TALLOC_CTX *ctx, uint32_t *v)
{
	TDR_PULL_NEED_BYTES(tdr, 4);
	*v = TDR_IVAL(tdr, tdr->offset);
	tdr->offset += 4;
	return NT_STATUS_OK;
}

NTSTATUS tdr_push_uint32(struct tdr_push *tdr, const uint32_t *v)
{
	TDR_PUSH_NEED_BYTES(tdr, 4);
	TDR_SIVAL(tdr, tdr->data.length, *v);
	tdr->data.length += 4;
	return NT_STATUS_OK;
}

NTSTATUS tdr_print_uint32(struct tdr_print *tdr, const char *name, uint32_t *v)
{
	tdr->print(tdr, "%-25s: 0x%02x (%u)", name, *v, *v);
	return NT_STATUS_OK;
}

NTSTATUS tdr_pull_charset(struct tdr_pull *tdr, TALLOC_CTX *ctx, const char **v, uint32_t length, uint32_t el_size, charset_t chset)
{
	size_t ret;

	if (length == -1) {
		switch (chset) {
			case CH_DOS:
				length = ascii_len_n((const char*)tdr->data.data+tdr->offset, tdr->data.length-tdr->offset);
				break;
			case CH_UTF16:
				length = utf16_len_n(tdr->data.data+tdr->offset, tdr->data.length-tdr->offset);
				break;

			default:
				return NT_STATUS_INVALID_PARAMETER;
		}
	}

	if (length == 0) {
		*v = talloc_strdup(ctx, "");
		return NT_STATUS_OK;
	}

	TDR_PULL_NEED_BYTES(tdr, el_size*length);
	
	if (!convert_string_talloc(ctx, chset, CH_UNIX, tdr->data.data+tdr->offset, el_size*length, discard_const_p(void *, v), &ret, false)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	tdr->offset += length * el_size;

	return NT_STATUS_OK;
}

NTSTATUS tdr_push_charset(struct tdr_push *tdr, const char **v, uint32_t length, uint32_t el_size, charset_t chset)
{
	size_t ret, required;

	if (length == -1) {
		length = strlen(*v) + 1; /* Extra element for null character */
	}

	required = el_size * length;
	TDR_PUSH_NEED_BYTES(tdr, required);

	ret = convert_string(CH_UNIX, chset, *v, strlen(*v), tdr->data.data+tdr->data.length, required, false);
	if (ret == -1) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Make sure the remaining part of the string is filled with zeroes */
	if (ret < required) {
		memset(tdr->data.data+tdr->data.length+ret, 0, required-ret);
	}
	
	tdr->data.length += required;
						 
	return NT_STATUS_OK;
}

NTSTATUS tdr_print_charset(struct tdr_print *tdr, const char *name, const char **v, uint32_t length, uint32_t el_size, charset_t chset)
{
	tdr->print(tdr, "%-25s: %s", name, *v);
	return NT_STATUS_OK;
}

/**
  parse a hyper
*/
NTSTATUS tdr_pull_hyper(struct tdr_pull *tdr, TALLOC_CTX *ctx, uint64_t *v)
{
	TDR_PULL_NEED_BYTES(tdr, 8);
	*v = TDR_IVAL(tdr, tdr->offset);
	*v |= (uint64_t)(TDR_IVAL(tdr, tdr->offset+4)) << 32;
	tdr->offset += 8;
	return NT_STATUS_OK;
}

/**
  push a hyper
*/
NTSTATUS tdr_push_hyper(struct tdr_push *tdr, uint64_t *v)
{
	TDR_PUSH_NEED_BYTES(tdr, 8);
	TDR_SIVAL(tdr, tdr->data.length, ((*v) & 0xFFFFFFFF));
	TDR_SIVAL(tdr, tdr->data.length+4, ((*v)>>32));
	tdr->data.length += 8;
	return NT_STATUS_OK;
}

/**
  push a NTTIME
*/
NTSTATUS tdr_push_NTTIME(struct tdr_push *tdr, NTTIME *t)
{
	TDR_CHECK(tdr_push_hyper(tdr, t));
	return NT_STATUS_OK;
}

/**
  pull a NTTIME
*/
NTSTATUS tdr_pull_NTTIME(struct tdr_pull *tdr, TALLOC_CTX *ctx, NTTIME *t)
{
	TDR_CHECK(tdr_pull_hyper(tdr, ctx, t));
	return NT_STATUS_OK;
}

/**
  push a time_t
*/
NTSTATUS tdr_push_time_t(struct tdr_push *tdr, time_t *t)
{
	return tdr_push_uint32(tdr, (uint32_t *)t);
}

/**
  pull a time_t
*/
NTSTATUS tdr_pull_time_t(struct tdr_pull *tdr, TALLOC_CTX *ctx, time_t *t)
{
	uint32_t tt;
	TDR_CHECK(tdr_pull_uint32(tdr, ctx, &tt));
	*t = tt;
	return NT_STATUS_OK;
}

NTSTATUS tdr_print_time_t(struct tdr_print *tdr, const char *name, time_t *t)
{
	if (*t == (time_t)-1 || *t == 0) {
		tdr->print(tdr, "%-25s: (time_t)%d", name, (int)*t);
	} else {
		tdr->print(tdr, "%-25s: %s", name, timestring(tdr, *t));
	}

	return NT_STATUS_OK;
}

NTSTATUS tdr_print_NTTIME(struct tdr_print *tdr, const char *name, NTTIME *t)
{
	tdr->print(tdr, "%-25s: %s", name, nt_time_string(tdr, *t));

	return NT_STATUS_OK;
}

NTSTATUS tdr_print_DATA_BLOB(struct tdr_print *tdr, const char *name, DATA_BLOB *r)
{
	tdr->print(tdr, "%-25s: DATA_BLOB length=%u", name, r->length);
	if (r->length) {
		dump_data(10, r->data, r->length);
	}

	return NT_STATUS_OK;
}

#define TDR_ALIGN(l,n) (((l) & ((n)-1)) == 0?0:((n)-((l)&((n)-1))))

/*
  push a DATA_BLOB onto the wire. 
*/
NTSTATUS tdr_push_DATA_BLOB(struct tdr_push *tdr, DATA_BLOB *blob)
{
	if (tdr->flags & TDR_ALIGN2) {
		blob->length = TDR_ALIGN(tdr->data.length, 2);
	} else if (tdr->flags & TDR_ALIGN4) {
		blob->length = TDR_ALIGN(tdr->data.length, 4);
	} else if (tdr->flags & TDR_ALIGN8) {
		blob->length = TDR_ALIGN(tdr->data.length, 8);
	}

	TDR_PUSH_NEED_BYTES(tdr, blob->length);
	
	memcpy(tdr->data.data+tdr->data.length, blob->data, blob->length);
	return NT_STATUS_OK;
}

/*
  pull a DATA_BLOB from the wire. 
*/
NTSTATUS tdr_pull_DATA_BLOB(struct tdr_pull *tdr, TALLOC_CTX *ctx, DATA_BLOB *blob)
{
	uint32_t length;

	if (tdr->flags & TDR_ALIGN2) {
		length = TDR_ALIGN(tdr->offset, 2);
	} else if (tdr->flags & TDR_ALIGN4) {
		length = TDR_ALIGN(tdr->offset, 4);
	} else if (tdr->flags & TDR_ALIGN8) {
		length = TDR_ALIGN(tdr->offset, 8);
	} else if (tdr->flags & TDR_REMAINING) {
		length = tdr->data.length - tdr->offset;
	} else {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (tdr->data.length - tdr->offset < length) {
		length = tdr->data.length - tdr->offset;
	}

	TDR_PULL_NEED_BYTES(tdr, length);

	*blob = data_blob_talloc(tdr, tdr->data.data+tdr->offset, length);
	tdr->offset += length;
	return NT_STATUS_OK;
}

struct tdr_push *tdr_push_init(TALLOC_CTX *mem_ctx)
{
	struct tdr_push *push = talloc_zero(mem_ctx, struct tdr_push);

	if (push == NULL)
		return NULL;

	return push;
}

struct tdr_pull *tdr_pull_init(TALLOC_CTX *mem_ctx)
{
	struct tdr_pull *pull = talloc_zero(mem_ctx, struct tdr_pull);

	if (pull == NULL)
		return NULL;

	return pull;
}

NTSTATUS tdr_push_to_fd(int fd, tdr_push_fn_t push_fn, const void *p)
{
	struct tdr_push *push = tdr_push_init(NULL);

	if (push == NULL)
		return NT_STATUS_NO_MEMORY;

	if (NT_STATUS_IS_ERR(push_fn(push, p))) {
		DEBUG(1, ("Error pushing data\n"));
		talloc_free(push);
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (write(fd, push->data.data, push->data.length) < push->data.length) {
		DEBUG(1, ("Error writing all data\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	talloc_free(push);

	return NT_STATUS_OK;
}

void tdr_print_debug_helper(struct tdr_print *tdr, const char *format, ...)
{
	va_list ap;
	char *s = NULL;
	int i;

	va_start(ap, format);
	vasprintf(&s, format, ap);
	va_end(ap);

	for (i=0;i<tdr->level;i++) { DEBUG(0,("    ")); }

	DEBUG(0,("%s\n", s));
	free(s);
}
