/*
   Unix SMB/CIFS implementation.

   routines for marshalling/unmarshalling special schannel structures

   Copyright (C) Guenther Deschner 2009

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
#include "../librpc/gen_ndr/ndr_schannel.h"
#include "../librpc/ndr/ndr_schannel.h"
#include "../libcli/nbt/libnbt.h"

_PUBLIC_ void ndr_print_NL_AUTH_MESSAGE_BUFFER(struct ndr_print *ndr, const char *name, const union NL_AUTH_MESSAGE_BUFFER *r)
{
	int level;
	level = ndr_print_get_switch_value(ndr, r);
	switch (level) {
		case NL_FLAG_OEM_NETBIOS_DOMAIN_NAME:
			ndr_print_string(ndr, name, r->a);
		break;

		case NL_FLAG_OEM_NETBIOS_COMPUTER_NAME:
			ndr_print_string(ndr, name, r->a);
		break;

		case NL_FLAG_UTF8_DNS_DOMAIN_NAME:
			ndr_print_nbt_string(ndr, name, r->u);
		break;

		case NL_FLAG_UTF8_DNS_HOST_NAME:
			ndr_print_nbt_string(ndr, name, r->u);
		break;

		case NL_FLAG_UTF8_NETBIOS_COMPUTER_NAME:
			ndr_print_nbt_string(ndr, name, r->u);
		break;

		default:
		break;

	}
}

_PUBLIC_ void ndr_print_NL_AUTH_MESSAGE_BUFFER_REPLY(struct ndr_print *ndr, const char *name, const union NL_AUTH_MESSAGE_BUFFER_REPLY *r)
{
	int level;
	level = ndr_print_get_switch_value(ndr, r);
	switch (level) {
		case NL_NEGOTIATE_RESPONSE:
			ndr_print_uint32(ndr, name, r->dummy);
		break;

		default:
		break;

	}
}

void dump_NL_AUTH_SIGNATURE(TALLOC_CTX *mem_ctx,
			    const DATA_BLOB *blob)
{
	enum ndr_err_code ndr_err;
	uint16_t signature_algorithm;

	if (blob->length < 2) {
		return;
	}

	signature_algorithm = SVAL(blob->data, 0);

	switch (signature_algorithm) {
	case NL_SIGN_HMAC_MD5: {
		struct NL_AUTH_SIGNATURE r;
		ndr_err = ndr_pull_struct_blob(blob, mem_ctx, &r,
		       (ndr_pull_flags_fn_t)ndr_pull_NL_AUTH_SIGNATURE);
		if (NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			NDR_PRINT_DEBUG(NL_AUTH_SIGNATURE, &r);
		}
		break;
	}
	case NL_SIGN_HMAC_SHA256: {
		struct NL_AUTH_SHA2_SIGNATURE r;
		ndr_err = ndr_pull_struct_blob(blob, mem_ctx, &r,
		       (ndr_pull_flags_fn_t)ndr_pull_NL_AUTH_SHA2_SIGNATURE);
		if (NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			NDR_PRINT_DEBUG(NL_AUTH_SHA2_SIGNATURE, &r);
		}
		break;
	}
	default:
		break;
	}
}
