/* 
   Unix SMB/CIFS implementation.

   libndr interface

   Copyright (C) Andrew Tridgell 2003
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

NTSTATUS ndr_push_dom_sid(struct ndr_push *ndr, int ndr_flags, const struct dom_sid *r)
{
	uint32_t cntr_sub_auths_0;
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, r->sid_rev_num));
		NDR_CHECK(ndr_push_int8(ndr, NDR_SCALARS, r->num_auths));
		NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, r->id_auth, 6));
		for (cntr_sub_auths_0 = 0; cntr_sub_auths_0 < r->num_auths; cntr_sub_auths_0++) {
			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->sub_auths[cntr_sub_auths_0]));
		}
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_dom_sid(struct ndr_pull *ndr, int ndr_flags, struct dom_sid *r)
{
	uint32_t cntr_sub_auths_0;
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &r->sid_rev_num));
		NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &r->num_auths));
		if (r->num_auths > 15) {
			return ndr_pull_error(ndr, NDR_ERR_RANGE, "value out of range");
		}
		NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, r->id_auth, 6));
		for (cntr_sub_auths_0 = 0; cntr_sub_auths_0 < r->num_auths; cntr_sub_auths_0++) {
			NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->sub_auths[cntr_sub_auths_0]));
		}
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NT_STATUS_OK;
}

/*
  convert a dom_sid to a string
*/
char *dom_sid_string(TALLOC_CTX *mem_ctx, const struct dom_sid *sid)
{
	int i, ofs, maxlen;
	uint32_t ia;
	char *ret;
	
	if (!sid) {
		return talloc_strdup(mem_ctx, "(NULL SID)");
	}

	maxlen = sid->num_auths * 11 + 25;
	ret = (char *)talloc_size(mem_ctx, maxlen);
	if (!ret) return talloc_strdup(mem_ctx, "(SID ERR)");

	ia = (sid->id_auth[5]) +
		(sid->id_auth[4] << 8 ) +
		(sid->id_auth[3] << 16) +
		(sid->id_auth[2] << 24);

	ofs = snprintf(ret, maxlen, "S-%u-%lu", 
		       (unsigned int)sid->sid_rev_num, (unsigned long)ia);

	for (i = 0; i < sid->num_auths; i++) {
		ofs += snprintf(ret + ofs, maxlen - ofs, "-%lu", (unsigned long)sid->sub_auths[i]);
	}
	
	return ret;
}

/*
  parse a dom_sid2 - this is a dom_sid but with an extra copy of the num_auths field
*/
NTSTATUS ndr_pull_dom_sid2(struct ndr_pull *ndr, int ndr_flags, struct dom_sid *sid)
{
	uint32_t num_auths;
	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &num_auths));
	NDR_CHECK(ndr_pull_dom_sid(ndr, ndr_flags, sid));
	if (sid->num_auths != num_auths) {
		return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, 
				      "Bad array size %u should exceed %u", 
				      num_auths, sid->num_auths);
	}
	return NT_STATUS_OK;
}

/*
  parse a dom_sid2 - this is a dom_sid but with an extra copy of the num_auths field
*/
NTSTATUS ndr_push_dom_sid2(struct ndr_push *ndr, int ndr_flags, const struct dom_sid *sid)
{
	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}
	NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, sid->num_auths));
	return ndr_push_dom_sid(ndr, ndr_flags, sid);
}

/*
  parse a dom_sid28 - this is a dom_sid in a fixed 28 byte buffer, so we need to ensure there are only upto 5 sub_auth
*/
NTSTATUS ndr_pull_dom_sid28(struct ndr_pull *ndr, int ndr_flags, struct dom_sid *sid)
{
	NTSTATUS status;
	struct ndr_pull *subndr;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}

	subndr = talloc_zero(ndr, struct ndr_pull);
	NT_STATUS_HAVE_NO_MEMORY(subndr);
	subndr->flags		= ndr->flags;
	subndr->current_mem_ctx	= ndr->current_mem_ctx;

	subndr->data		= ndr->data + ndr->offset;
	subndr->data_size	= 28;
	subndr->offset		= 0;

	NDR_CHECK(ndr_pull_advance(ndr, 28));

	status = ndr_pull_dom_sid(subndr, ndr_flags, sid);
	if (!NT_STATUS_IS_OK(status)) {
		/* handle a w2k bug which send random data in the buffer */
		ZERO_STRUCTP(sid);
	}

	return NT_STATUS_OK;
}

/*
  push a dom_sid28 - this is a dom_sid in a 28 byte fixed buffer
*/
NTSTATUS ndr_push_dom_sid28(struct ndr_push *ndr, int ndr_flags, const struct dom_sid *sid)
{
	uint32_t old_offset;
	uint32_t padding;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NT_STATUS_OK;
	}

	if (sid->num_auths > 5) {
		return ndr_push_error(ndr, NDR_ERR_RANGE, 
				      "dom_sid28 allows only upto 5 sub auth [%u]", 
				      sid->num_auths);
	}

	old_offset = ndr->offset;
	NDR_CHECK(ndr_push_dom_sid(ndr, ndr_flags, sid));

	padding = 28 - (ndr->offset - old_offset);

	if (padding > 0) {
		NDR_CHECK(ndr_push_zero(ndr, padding));
	}

	return NT_STATUS_OK;
}

NTSTATUS ndr_push_sec_desc_buf(struct ndr_push *ndr, int ndr_flags, const struct sec_desc_buf *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, ndr_size_security_descriptor(r->sd,ndr->flags)));
		NDR_CHECK(ndr_push_unique_ptr(ndr, r->sd));
	}
	if (ndr_flags & NDR_BUFFERS) {
		if (r->sd) {
			{
				struct ndr_push *_ndr_sd;
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_sd, 4, -1));
				NDR_CHECK(ndr_push_security_descriptor(_ndr_sd, NDR_SCALARS|NDR_BUFFERS, r->sd));
				NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_sd, 4, -1));
			}
		}
	}
	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_sec_desc_buf(struct ndr_pull *ndr, int ndr_flags, struct sec_desc_buf *r)
{
	uint32_t _ptr_sd;
	TALLOC_CTX *_mem_save_sd_0;
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->sd_size));
		if (r->sd_size > 0x40000) { /* sd_size is unsigned */
			return ndr_pull_error(ndr, NDR_ERR_RANGE, "value out of range");
		}
		NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_sd));
		if (_ptr_sd) {
			NDR_PULL_ALLOC(ndr, r->sd);
		} else {
			r->sd = NULL;
		}
	}
	if (ndr_flags & NDR_BUFFERS) {
		if (r->sd) {
			_mem_save_sd_0 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, r->sd, 0);
			{
				struct ndr_pull *_ndr_sd;
				NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_sd, 4, -1));
				NDR_CHECK(ndr_pull_security_descriptor(_ndr_sd, NDR_SCALARS|NDR_BUFFERS, r->sd));
				NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_sd, 4, -1));
			}
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_sd_0, 0);
		}
	}
	return NT_STATUS_OK;
}

void ndr_print_sec_desc_buf(struct ndr_print *ndr, const char *name, const struct sec_desc_buf *r)
{
	ndr_print_struct(ndr, name, "sec_desc_buf");
	ndr->depth++;
	ndr_print_uint32(ndr, "sd_size", (ndr->flags & LIBNDR_PRINT_SET_VALUES)?ndr_size_security_descriptor(r->sd,ndr->flags):r->sd_size);
	ndr_print_ptr(ndr, "sd", r->sd);
	ndr->depth++;
	if (r->sd) {
		ndr_print_security_descriptor(ndr, "sd", r->sd);
	}
	ndr->depth--;
	ndr->depth--;
}
