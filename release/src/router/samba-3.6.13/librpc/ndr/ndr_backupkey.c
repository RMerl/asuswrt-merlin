/*
   Unix SMB/CIFS implementation.

   routines for top backup key protocol marshalling/unmarshalling

   Copyright (C) Matthieu Patou 2010

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
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_backupkey.h"
#include "librpc/gen_ndr/ndr_security.h"

static uint32_t backupkeyguid_to_uint(const struct GUID *guid)
{
	struct GUID tmp;
	NTSTATUS status;
	bool match;

	status = GUID_from_string(BACKUPKEY_RESTORE_GUID, &tmp);
	if (NT_STATUS_IS_OK(status)) {
		match = GUID_equal(guid, &tmp);
		if (match) {
			return BACKUPKEY_RESTORE_GUID_INTEGER;
		}
	}

	status = GUID_from_string(BACKUPKEY_RETRIEVE_BACKUP_KEY_GUID, &tmp);
	if (NT_STATUS_IS_OK(status)) {
		match = GUID_equal(guid, &tmp);
		if (match) {
			return BACKUPKEY_RETRIEVE_BACKUP_KEY_GUID_INTEGER;
		}
	}

	return BACKUPKEY_INVALID_GUID_INTEGER;
}

_PUBLIC_ void ndr_print_bkrp_BackupKey(struct ndr_print *ndr, const char *name, int flags, const struct bkrp_BackupKey *r)
{
	ndr_print_struct(ndr, name, "bkrp_BackupKey");
	if (r == NULL) { ndr_print_null(ndr); return; }
	ndr->depth++;
	if (flags & NDR_SET_VALUES) {
		ndr->flags |= LIBNDR_PRINT_SET_VALUES;
	}
	if (flags & NDR_IN) {
		union bkrp_data_in_blob inblob;
		DATA_BLOB blob;
		uint32_t level;
		enum ndr_err_code ndr_err;

		ndr_print_struct(ndr, "in", "bkrp_BackupKey");
		ndr->depth++;
		ndr_print_ptr(ndr, "guidActionAgent", r->in.guidActionAgent);
		ndr->depth++;
		ndr_print_GUID(ndr, "guidActionAgent", r->in.guidActionAgent);
		ndr->depth--;

		level = backupkeyguid_to_uint(r->in.guidActionAgent);
		blob.data = r->in.data_in;
		blob.length = r->in.data_in_len;
		ndr_err = ndr_pull_union_blob(&blob, ndr, &inblob, level,
				(ndr_pull_flags_fn_t)ndr_pull_bkrp_data_in_blob);

		ndr_print_ptr(ndr, "data_in", r->in.data_in);
		ndr->depth++;
		if (NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			ndr_print_bkrp_data_in_blob(ndr, "data_in", &inblob);
		} else {
			ndr_print_array_uint8(ndr, "data_in", r->in.data_in, r->in.data_in_len);
		}
		ndr->depth--;

		ndr_print_uint32(ndr, "data_in_len", r->in.data_in_len);
		ndr_print_uint32(ndr, "param", r->in.param);
		ndr->depth--;
	}
	if (flags & NDR_OUT) {
		ndr_print_struct(ndr, "out", "bkrp_BackupKey");
		ndr->depth++;
		ndr_print_ptr(ndr, "data_out", r->out.data_out);
		ndr->depth++;
		ndr_print_ptr(ndr, "data_out", *r->out.data_out);
		ndr->depth++;

		if (*r->out.data_out) {
			ndr_print_array_uint8(ndr, "data_out", *r->out.data_out, *r->out.data_out_len);
		}
		ndr->depth--;
		ndr->depth--;
		ndr_print_ptr(ndr, "data_out_len", r->out.data_out_len);
		ndr->depth++;
		ndr_print_uint32(ndr, "data_out_len", *r->out.data_out_len);
		ndr->depth--;
		ndr_print_WERROR(ndr, "result", r->out.result);
		ndr->depth--;
	}
	ndr->depth--;
}

/* We have manual push/pull because we didn't manage to do the alignment
 * purely in PIDL as the padding is sized so that the whole access_check_v3
 * struct size is a multiple of 8 (as specified in 2.2.2.3 of ms-bkrp.pdf)
 */
_PUBLIC_ enum ndr_err_code ndr_push_bkrp_access_check_v2(struct ndr_push *ndr, int ndr_flags, const struct bkrp_access_check_v2 *r)
{
	if (ndr_flags & NDR_SCALARS) {
		size_t ofs;
		size_t pad;
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0x00000001));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->nonce_len));
		NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, r->nonce, r->nonce_len));
		NDR_CHECK(ndr_push_dom_sid(ndr, NDR_SCALARS, &r->sid));
		/* We articially increment the offset of 20 bytes (size of hash
		 * comming after the pad) so that ndr_align can determine easily
		 * the correct pad size to make the whole struct 8 bytes aligned
		 */
		ofs = ndr->offset + 20;
		pad = ndr_align_size(ofs, 8);
		NDR_CHECK(ndr_push_zero(ndr, pad));
		NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, r->hash, 20));
		NDR_CHECK(ndr_push_trailer_align(ndr, 4));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_bkrp_access_check_v2(struct ndr_pull *ndr, int ndr_flags, struct bkrp_access_check_v2 *r)
{
	if (ndr_flags & NDR_SCALARS) {
		size_t ofs;
		size_t pad;
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->magic));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->nonce_len));
		NDR_PULL_ALLOC_N(ndr, r->nonce, r->nonce_len);
		NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, r->nonce, r->nonce_len));
		NDR_CHECK(ndr_pull_dom_sid(ndr, NDR_SCALARS, &r->sid));
		ofs = ndr->offset + 20;
		pad = ndr_align_size(ofs, 8);
		NDR_CHECK(ndr_pull_advance(ndr, pad));
		NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, r->hash, 20));
		NDR_CHECK(ndr_pull_trailer_align(ndr, 4));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}

/* We have manual push/pull because we didn't manage to do the alignment
 * purely in PIDL as the padding is sized so that the whole access_check_v3
 * struct size is a multiple of 16 (as specified in 2.2.2.4 of ms-bkrp.pdf)
 */
_PUBLIC_ enum ndr_err_code ndr_push_bkrp_access_check_v3(struct ndr_push *ndr, int ndr_flags, const struct bkrp_access_check_v3 *r)
{
	if (ndr_flags & NDR_SCALARS) {
		size_t ofs;
		size_t pad;
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0x00000001));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->nonce_len));
		NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, r->nonce, r->nonce_len));
		NDR_CHECK(ndr_push_dom_sid(ndr, NDR_SCALARS, &r->sid));
		/* We articially increment the offset of 64 bytes (size of hash
		 * comming after the pad) so that ndr_align can determine easily
		 * the correct pad size to make the whole struct 16 bytes aligned
		 */
		ofs = ndr->offset + 64;
		pad = ndr_align_size(ofs, 16);
		NDR_CHECK(ndr_push_zero(ndr, pad));
		NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, r->hash, 64));
		NDR_CHECK(ndr_push_trailer_align(ndr, 4));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_bkrp_access_check_v3(struct ndr_pull *ndr, int ndr_flags, struct bkrp_access_check_v3 *r)
{
	if (ndr_flags & NDR_SCALARS) {
		size_t ofs;
		size_t pad;
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->magic));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->nonce_len));
		NDR_PULL_ALLOC_N(ndr, r->nonce, r->nonce_len);
		NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, r->nonce, r->nonce_len));
		NDR_CHECK(ndr_pull_dom_sid(ndr, NDR_SCALARS, &r->sid));
		ofs = ndr->offset + 64;
		pad = ndr_align_size(ofs, 16);
		NDR_CHECK(ndr_pull_advance(ndr, pad));
		NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, r->hash, 64));
		NDR_CHECK(ndr_pull_trailer_align(ndr, 4));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}
