/*
   Unix SMB/CIFS implementation.

   helper routines for FRSRPC marshalling

   Copyright (C) Stefan (metze) Metzmacher 2009

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
#include "librpc/gen_ndr/ndr_frsrpc.h"

enum ndr_err_code ndr_push_frsrpc_CommPktChunkCtr(struct ndr_push *ndr,
					int ndr_flags,
					const struct frsrpc_CommPktChunkCtr *r)
{
	uint32_t cntr_chunks_0;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_push_align(ndr, 2));
			for (cntr_chunks_0 = 0; cntr_chunks_0 < r->num_chunks; cntr_chunks_0++) {
				NDR_CHECK(ndr_push_frsrpc_CommPktChunk(ndr, NDR_SCALARS, &r->chunks[cntr_chunks_0]));
			}
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

#define _TMP_PULL_REALLOC_N(ndr, s, t, n) do { \
	_NDR_PULL_FIX_CURRENT_MEM_CTX(ndr);\
	(s) = talloc_realloc(ndr->current_mem_ctx, (s), t, n); \
	if (!(s)) { \
		return ndr_pull_error(ndr, NDR_ERR_ALLOC, \
				      "Alloc %u * %s failed: %s\n", \
				      (unsigned)n, # s, __location__); \
	} \
} while (0)

enum ndr_err_code ndr_pull_frsrpc_CommPktChunkCtr(struct ndr_pull *ndr,
					int ndr_flags,
					struct frsrpc_CommPktChunkCtr *r)
{
	uint32_t cntr_chunks_0;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			uint32_t remaining = ndr->data_size - ndr->offset;
			r->num_chunks = 0;
			r->chunks = NULL;
			for (cntr_chunks_0 = 0; remaining > 0; cntr_chunks_0++) {
				r->num_chunks += 1;
				_TMP_PULL_REALLOC_N(ndr, r->chunks,
						    struct frsrpc_CommPktChunk,
						    r->num_chunks);
				NDR_CHECK(ndr_pull_frsrpc_CommPktChunk(ndr,
						NDR_SCALARS,
						&r->chunks[cntr_chunks_0]));
				remaining = ndr->data_size - ndr->offset;
			}
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

size_t ndr_size_frsrpc_CommPktChunkCtr(const struct frsrpc_CommPktChunkCtr *r,
				       int flags)
{
	flags |= LIBNDR_FLAG_NOALIGN;
	return ndr_size_struct(r, flags,
			(ndr_push_flags_fn_t)ndr_push_frsrpc_CommPktChunkCtr);
}
