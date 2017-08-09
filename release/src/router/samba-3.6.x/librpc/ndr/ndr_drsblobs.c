/* 
   Unix SMB/CIFS implementation.

   Manually parsed structures found in the DRS protocol

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2008
   Copyright (C) Guenther Deschner <gd@samba.org> 2010
   
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
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "../lib/util/asn1.h"

_PUBLIC_ enum ndr_err_code ndr_push_AuthenticationInformationArray(struct ndr_push *ndr, int ndr_flags, const struct AuthenticationInformationArray *r)
{
	uint32_t cntr_array_0;
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		for (cntr_array_0 = 0; cntr_array_0 < r->count; cntr_array_0++) {
			NDR_CHECK(ndr_push_AuthenticationInformation(ndr, NDR_SCALARS, &r->array[cntr_array_0]));
		}
		NDR_CHECK(ndr_push_trailer_align(ndr, 4));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_AuthenticationInformationArray(struct ndr_pull *ndr, int ndr_flags, struct AuthenticationInformationArray *r)
{
	if (ndr_flags & NDR_SCALARS) {
		r->count = 0;
		NDR_PULL_ALLOC_N(ndr, r->array, r->count);
		/* entry is at least 16 bytes large */
		while (ndr->offset + 16 <= ndr->data_size) {
			r->array = talloc_realloc(ndr, r->array, struct AuthenticationInformation, r->count + 1);
			NDR_ERR_HAVE_NO_MEMORY(r->array);
			NDR_CHECK(ndr_pull_AuthenticationInformation(ndr, NDR_SCALARS, &r->array[r->count]));
			r->count++;
		}
		NDR_CHECK(ndr_pull_trailer_align(ndr, 4));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_push_trustAuthInOutBlob(struct ndr_push *ndr, int ndr_flags, const struct trustAuthInOutBlob *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->count));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, (r->count > 0)?12:0));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, (r->count > 0)?12 + ndr_size_AuthenticationInformationArray(&r->current, ndr_flags):0));
		{
			struct ndr_push *_ndr_current;
			NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_current, 0, ((r->count > 0)?12 + ndr_size_AuthenticationInformationArray(&r->current, ndr_flags):0) - ((r->count > 0)?12:0)));
			NDR_CHECK(ndr_push_AuthenticationInformationArray(_ndr_current, NDR_SCALARS, &r->current));
			NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_current, 0, ((r->count > 0)?12 + ndr_size_AuthenticationInformationArray(&r->current, ndr_flags):0) - ((r->count > 0)?12:0)));
		}
		{
			uint32_t _flags_save_AuthenticationInformationArray = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_REMAINING);
			{
				struct ndr_push *_ndr_previous;
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_previous, 0, -1));
				NDR_CHECK(ndr_push_AuthenticationInformationArray(_ndr_previous, NDR_SCALARS, &r->previous));
				NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_previous, 0, -1));
			}
			ndr->flags = _flags_save_AuthenticationInformationArray;
		}
		NDR_CHECK(ndr_push_trailer_align(ndr, 4));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}


_PUBLIC_ enum ndr_err_code ndr_pull_trustDomainPasswords(struct ndr_pull *ndr, int ndr_flags, struct trustDomainPasswords *r)
{
	if (ndr_flags & NDR_SCALARS) {
		uint32_t offset;
		NDR_PULL_ALIGN(ndr, 4);
		NDR_PULL_NEED_BYTES(ndr, 8);
		
		offset = ndr->offset;
		ndr->offset = ndr->data_size - 8;

		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->outgoing_size));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->incoming_size));

		ndr->offset = offset;
		NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, r->confounder, 512));
		{
			struct ndr_pull *_ndr_outgoing;
			NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_outgoing, 0, r->outgoing_size));
			NDR_CHECK(ndr_pull_trustAuthInOutBlob(_ndr_outgoing, NDR_SCALARS|NDR_BUFFERS, &r->outgoing));
			NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_outgoing, 0, r->outgoing_size));
		}
		{
			struct ndr_pull *_ndr_incoming;
			NDR_CHECK(ndr_pull_subcontext_start(ndr, &_ndr_incoming, 0, r->incoming_size));
			NDR_CHECK(ndr_pull_trustAuthInOutBlob(_ndr_incoming, NDR_SCALARS|NDR_BUFFERS, &r->incoming));
			NDR_CHECK(ndr_pull_subcontext_end(ndr, _ndr_incoming, 0, r->incoming_size));
		}
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->outgoing_size));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->incoming_size));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ void ndr_print_drsuapi_MSPrefixMap_Entry(struct ndr_print *ndr, const char *name, const struct drsuapi_MSPrefixMap_Entry *r)
{
	ndr_print_struct(ndr, name, "drsuapi_MSPrefixMap_Entry");
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		ndr->depth++;
		ndr_print_uint16(ndr, "entryID", r->entryID);
		ndr->print(ndr, "%-25s: length=%u", "oid", r->length);
		if (r->binary_oid) {
			char *partial_oid = NULL;
			DATA_BLOB oid_blob = data_blob_const(r->binary_oid, r->length);
			char *hex_str = data_blob_hex_string_upper(ndr, &oid_blob);
			ber_read_partial_OID_String(ndr, oid_blob, &partial_oid);
			ndr->depth++;
			ndr->print(ndr, "%-25s: 0x%s (%s)", "binary_oid", hex_str, partial_oid);
			ndr->depth--;
			talloc_free(hex_str);
			talloc_free(partial_oid);
		}
		ndr->depth--;
		ndr->flags = _flags_save_STRUCT;
	}
}
