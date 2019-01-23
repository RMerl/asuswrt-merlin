/*
   Unix SMB/CIFS implementation.

   helper routines for XATTR marshalling

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
#include "librpc/gen_ndr/ndr_xattr.h"

static char *ndr_compat_xattr_attrib_hex(TALLOC_CTX *mem_ctx,
					const struct xattr_DOSATTRIB *r)
{
	char *attrib_hex = NULL;

	switch (r->version) {
	case 0xFFFF:
		attrib_hex = talloc_asprintf(mem_ctx, "0x%x",
					r->info.compatinfoFFFF.attrib);
		break;
	case 1:
		attrib_hex = talloc_asprintf(mem_ctx, "0x%x",
					r->info.info1.attrib);
		break;
	case 2:
		attrib_hex = talloc_asprintf(mem_ctx, "0x%x",
					r->info.oldinfo2.attrib);
		break;
	case 3:
		if (!(r->info.info3.valid_flags & XATTR_DOSINFO_ATTRIB)) {
			attrib_hex = talloc_strdup(mem_ctx, "");
			break;
		}
		attrib_hex = talloc_asprintf(mem_ctx, "0x%x",
					r->info.info3.attrib);
		break;
	default:
		attrib_hex = talloc_strdup(mem_ctx, "");
		break;
	}

	return attrib_hex;
}

_PUBLIC_ enum ndr_err_code ndr_push_xattr_DOSATTRIB(struct ndr_push *ndr,
						int ndr_flags,
						const struct xattr_DOSATTRIB *r)
{
	if (ndr_flags & NDR_SCALARS) {
		char *attrib_hex = NULL;

		attrib_hex = ndr_compat_xattr_attrib_hex(ndr, r);
		NDR_ERR_HAVE_NO_MEMORY(attrib_hex);

		NDR_CHECK(ndr_push_align(ndr, 4));
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, attrib_hex));
			ndr->flags = _flags_save_string;
		}
		if (r->version == 0xFFFF) {
			return NDR_ERR_SUCCESS;
		}
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r->version));
		NDR_CHECK(ndr_push_set_switch_value(ndr, &r->info, r->version));
		NDR_CHECK(ndr_push_xattr_DosInfo(ndr, NDR_SCALARS, &r->info));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_xattr_DOSATTRIB(struct ndr_pull *ndr, int ndr_flags, struct xattr_DOSATTRIB *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_align(ndr, 4));
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_pull_string(ndr, NDR_SCALARS, &r->attrib_hex));
			ndr->flags = _flags_save_string;
		}
		if (ndr->offset >= ndr->data_size) {
			unsigned int dosattr;
			int ret;

			if (r->attrib_hex[0] != '0') {

			}
			if (r->attrib_hex[1] != 'x') {

			}
			ret = sscanf(r->attrib_hex, "%x", &dosattr);
			if (ret != 1) {
			}
			r->version = 0xFFFF;
			r->info.compatinfoFFFF.attrib = dosattr;
			return NDR_ERR_SUCCESS;
		}
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->version));
		if (r->version == 0xFFFF) {
			return ndr_pull_error(ndr, NDR_ERR_BAD_SWITCH,
					"ndr_pull_xattr_DOSATTRIB: "
					"invalid level 0x%02X",
					r->version);
		}
		NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->info, r->version));
		NDR_CHECK(ndr_pull_xattr_DosInfo(ndr, NDR_SCALARS, &r->info));
	}
	if (ndr_flags & NDR_BUFFERS) {
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ void ndr_print_xattr_DOSATTRIB(struct ndr_print *ndr, const char *name, const struct xattr_DOSATTRIB *r)
{
	char *attrib_hex;

	ndr_print_struct(ndr, name, "xattr_DOSATTRIB");
	ndr->depth++;

	if (ndr->flags & LIBNDR_PRINT_SET_VALUES) {
		attrib_hex = ndr_compat_xattr_attrib_hex(ndr, r);
	} else {
		attrib_hex = talloc_strdup(ndr, r->attrib_hex);
	}
	ndr_print_string(ndr, "attrib_hex", attrib_hex);

	ndr_print_uint16(ndr, "version", r->version);
	ndr_print_set_switch_value(ndr, &r->info, r->version);
	ndr_print_xattr_DosInfo(ndr, "info", &r->info);
	ndr->depth--;
}
