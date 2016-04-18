/*
   Unix SMB/CIFS implementation.

   Manually parsed structures found in the DCERPC protocol

   Copyright (C) Stefan Metzmacher 2014
   Copyright (C) Gregor Beck 2014

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
#include "librpc/gen_ndr/ndr_dcerpc.h"

#include "librpc/gen_ndr/ndr_misc.h"
#include "lib/util/bitmap.h"

const uint8_t DCERPC_SEC_VT_MAGIC[] = {0x8a,0xe3,0x13,0x71,0x02,0xf4,0x36,0x71};

_PUBLIC_ enum ndr_err_code ndr_push_dcerpc_sec_vt_count(struct ndr_push *ndr, int ndr_flags, const struct dcerpc_sec_vt_count *r)
{
	NDR_PUSH_CHECK_FLAGS(ndr, ndr_flags);
	/* nothing */
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_dcerpc_sec_vt_count(struct ndr_pull *ndr, int ndr_flags, struct dcerpc_sec_vt_count *r)
{
	uint32_t _saved_ofs = ndr->offset;

	NDR_PULL_CHECK_FLAGS(ndr, ndr_flags);

	if (!(ndr_flags & NDR_SCALARS)) {
		return NDR_ERR_SUCCESS;
	}

	r->count = 0;

	while (true) {
		uint16_t command;
		uint16_t length;

		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &command));
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &length));
		NDR_CHECK(ndr_pull_advance(ndr, length));

		r->count += 1;

		if (command & DCERPC_SEC_VT_COMMAND_END) {
			break;
		}
	}

	ndr->offset = _saved_ofs;
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pop_dcerpc_sec_verification_trailer(
	struct ndr_pull *ndr, TALLOC_CTX *mem_ctx,
	struct dcerpc_sec_verification_trailer **_r)
{
	enum ndr_err_code ndr_err;
	uint32_t ofs;
	uint32_t min_ofs = 0;
	struct dcerpc_sec_verification_trailer *r;
	DATA_BLOB sub_blob = data_blob_null;
	struct ndr_pull *sub_ndr = NULL;
	uint32_t remaining;

	*_r = NULL;

	r = talloc_zero(mem_ctx, struct dcerpc_sec_verification_trailer);
	if (r == NULL) {
		return NDR_ERR_ALLOC;
	}

	if (ndr->data_size < sizeof(DCERPC_SEC_VT_MAGIC)) {
		/*
		 * we return with r->count = 0
		 */
		*_r = r;
		return NDR_ERR_SUCCESS;
	}

	ofs = ndr->data_size - sizeof(DCERPC_SEC_VT_MAGIC);
	/* the magic is 4 byte aligned */
	ofs &= ~3;

	if (ofs > DCERPC_SEC_VT_MAX_SIZE) {
		/*
		 * We just scan the last 1024 bytes.
		 */
		min_ofs = ofs - DCERPC_SEC_VT_MAX_SIZE;
	} else {
		min_ofs = 0;
	}

	while (true) {
		int ret;

		ret = memcmp(&ndr->data[ofs],
			     DCERPC_SEC_VT_MAGIC,
			     sizeof(DCERPC_SEC_VT_MAGIC));
		if (ret == 0) {
			sub_blob = data_blob_const(&ndr->data[ofs],
						   ndr->data_size - ofs);
			break;
		}

		if (ofs <= min_ofs) {
			break;
		}

		ofs -= 4;
	}

	if (sub_blob.length == 0) {
		/*
		 * we return with r->count = 0
		 */
		*_r = r;
		return NDR_ERR_SUCCESS;
	}

	sub_ndr = ndr_pull_init_blob(&sub_blob, r);
	if (sub_ndr == NULL) {
		TALLOC_FREE(r);
		return NDR_ERR_ALLOC;
	}

	ndr_err = ndr_pull_dcerpc_sec_verification_trailer(sub_ndr,
							   NDR_SCALARS | NDR_BUFFERS,
							   r);
	if (ndr_err == NDR_ERR_ALLOC) {
		TALLOC_FREE(r);
		return NDR_ERR_ALLOC;
	}

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		goto ignore_error;
	}

	remaining = sub_ndr->data_size - sub_ndr->offset;
	if (remaining > 16) {
		/*
		 * we expect not more than 16 byte of additional
		 * padding after the verification trailer.
		 */
		goto ignore_error;
	}

	/*
	 * We assume that we got a real verification trailer.
	 *
	 * We remove it from the available stub data.
	 */
	ndr->data_size = ofs;

	TALLOC_FREE(sub_ndr);

	*_r = r;
	return NDR_ERR_SUCCESS;

ignore_error:
	TALLOC_FREE(sub_ndr);
	/*
	 * just ignore the error, it's likely
	 * that the magic we found belongs to
	 * the stub data.
	 *
	 * we return with r->count = 0
	 */
	ZERO_STRUCTP(r);
	*_r = r;
	return NDR_ERR_SUCCESS;
}
