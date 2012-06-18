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
	struct smb_iconv_convenience *iconv_convenience;
};

struct tdr_push {
	DATA_BLOB data;
	int flags;
	struct smb_iconv_convenience *iconv_convenience;
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

#include "../lib/tdr/tdr_proto.h"

#endif /* __TDR_H__ */
