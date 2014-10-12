/* 
   Unix SMB/CIFS implementation.
   TDR definitions
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

#ifndef __TDR_H__
#define __TDR_H__

#include <talloc.h>
#include "../lib/util/charset/charset.h"

#define TDR_BIG_ENDIAN			0x01
#define TDR_ALIGN2			0x02
#define TDR_ALIGN4			0x04
#define TDR_ALIGN8			0x08
#define TDR_REMAINING			0x10

struct tdr_pull {
	DATA_BLOB data;
	uint32_t offset;
	int flags;
};

struct tdr_push {
	DATA_BLOB data;
	int flags;
};

struct tdr_print {
	int level;
	void (*print)(struct tdr_print *, const char *, ...);
	int flags;
};

#define TDR_CHECK(call) do { NTSTATUS _status; \
                             _status = call; \
                             if (!NT_STATUS_IS_OK(_status)) \
                                return _status; \
                        } while (0)

#define TDR_ALLOC(ctx, s, n) do { \
			       (s) = talloc_array_ptrtype(ctx, (s), n); \
                           if ((n) && !(s)) return NT_STATUS_NO_MEMORY; \
                           } while (0)

typedef NTSTATUS (*tdr_push_fn_t) (struct tdr_push *, const void *);
typedef NTSTATUS (*tdr_pull_fn_t) (struct tdr_pull *, TALLOC_CTX *, void *);

NTSTATUS tdr_push_expand(struct tdr_push *tdr, uint32_t size);
NTSTATUS tdr_pull_uint8(struct tdr_pull *tdr, TALLOC_CTX *ctx, uint8_t *v);
NTSTATUS tdr_push_uint8(struct tdr_push *tdr, const uint8_t *v);
NTSTATUS tdr_print_uint8(struct tdr_print *tdr, const char *name, uint8_t *v);
NTSTATUS tdr_pull_uint16(struct tdr_pull *tdr, TALLOC_CTX *ctx, uint16_t *v);
NTSTATUS tdr_pull_uint1632(struct tdr_pull *tdr, TALLOC_CTX *ctx, uint16_t *v);
NTSTATUS tdr_push_uint16(struct tdr_push *tdr, const uint16_t *v);
NTSTATUS tdr_push_uint1632(struct tdr_push *tdr, const uint16_t *v);
NTSTATUS tdr_print_uint16(struct tdr_print *tdr, const char *name, uint16_t *v);
NTSTATUS tdr_pull_uint32(struct tdr_pull *tdr, TALLOC_CTX *ctx, uint32_t *v);
NTSTATUS tdr_push_uint32(struct tdr_push *tdr, const uint32_t *v);
NTSTATUS tdr_print_uint32(struct tdr_print *tdr, const char *name, uint32_t *v);
NTSTATUS tdr_pull_charset(struct tdr_pull *tdr, TALLOC_CTX *ctx, const char **v, uint32_t length, uint32_t el_size, charset_t chset);
NTSTATUS tdr_push_charset(struct tdr_push *tdr, const char **v, uint32_t length, uint32_t el_size, charset_t chset);
NTSTATUS tdr_print_charset(struct tdr_print *tdr, const char *name, const char **v, uint32_t length, uint32_t el_size, charset_t chset);

NTSTATUS tdr_pull_hyper(struct tdr_pull *tdr, TALLOC_CTX *ctx, uint64_t *v);
NTSTATUS tdr_push_hyper(struct tdr_push *tdr, uint64_t *v);

NTSTATUS tdr_push_NTTIME(struct tdr_push *tdr, NTTIME *t);
NTSTATUS tdr_pull_NTTIME(struct tdr_pull *tdr, TALLOC_CTX *ctx, NTTIME *t);
NTSTATUS tdr_print_NTTIME(struct tdr_print *tdr, const char *name, NTTIME *t);

NTSTATUS tdr_push_time_t(struct tdr_push *tdr, time_t *t);
NTSTATUS tdr_pull_time_t(struct tdr_pull *tdr, TALLOC_CTX *ctx, time_t *t);
NTSTATUS tdr_print_time_t(struct tdr_print *tdr, const char *name, time_t *t);

NTSTATUS tdr_print_DATA_BLOB(struct tdr_print *tdr, const char *name, DATA_BLOB *r);
NTSTATUS tdr_push_DATA_BLOB(struct tdr_push *tdr, DATA_BLOB *blob);
NTSTATUS tdr_pull_DATA_BLOB(struct tdr_pull *tdr, TALLOC_CTX *ctx, DATA_BLOB *blob);

struct tdr_push *tdr_push_init(TALLOC_CTX *mem_ctx);
struct tdr_pull *tdr_pull_init(TALLOC_CTX *mem_ctx);

NTSTATUS tdr_push_to_fd(int fd, tdr_push_fn_t push_fn, const void *p);
void tdr_print_debug_helper(struct tdr_print *tdr, const char *format, ...) PRINTF_ATTRIBUTE(2,3);

#endif /* __TDR_H__ */
