/*
   Unix SMB/CIFS implementation.

   CLDAP server structures

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2008

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
#include "../libcli/netlogon.h"

#undef DEBUG
#define DEBUG(x, y)
#undef DEBUGLVL
#define DEBUGLVL(x) false
#undef DEBUGLEVEL
#define DEBUGLEVEL 0

NTSTATUS push_netlogon_samlogon_response(DATA_BLOB *data, TALLOC_CTX *mem_ctx,
					 struct smb_iconv_convenience *iconv_convenience,
					 struct netlogon_samlogon_response *response)
{
	enum ndr_err_code ndr_err;
	if (response->ntver == NETLOGON_NT_VERSION_1) {
		ndr_err = ndr_push_struct_blob(data, mem_ctx,
					       iconv_convenience,
					       &response->data.nt4,
					       (ndr_push_flags_fn_t)ndr_push_NETLOGON_SAM_LOGON_RESPONSE_NT40);
	} else if (response->ntver & NETLOGON_NT_VERSION_5EX) {
		ndr_err = ndr_push_struct_blob(data, mem_ctx,
					       iconv_convenience,
					       &response->data.nt5_ex,
					       (ndr_push_flags_fn_t)ndr_push_NETLOGON_SAM_LOGON_RESPONSE_EX_with_flags);
	} else if (response->ntver & NETLOGON_NT_VERSION_5) {
		ndr_err = ndr_push_struct_blob(data, mem_ctx,
					       iconv_convenience,
					       &response->data.nt5,
					       (ndr_push_flags_fn_t)ndr_push_NETLOGON_SAM_LOGON_RESPONSE);
	} else {
		DEBUG(0, ("Asked to push unknown netlogon response type 0x%02x\n", response->ntver));
		return NT_STATUS_INVALID_PARAMETER;
	}
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(2,("failed to push netlogon response of type 0x%02x\n",
			 response->ntver));
		return ndr_map_error2ntstatus(ndr_err);
	}
	return NT_STATUS_OK;
}

NTSTATUS pull_netlogon_samlogon_response(DATA_BLOB *data, TALLOC_CTX *mem_ctx,
					 struct smb_iconv_convenience *iconv_convenience,
					 struct netlogon_samlogon_response *response)
{
	uint32_t ntver;
	enum ndr_err_code ndr_err;

	if (data->length < 8) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	/* lmnttoken */
	if (SVAL(data->data, data->length - 4) != 0xffff) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}
	/* lm20token */
	if (SVAL(data->data, data->length - 2) != 0xffff) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	ntver = IVAL(data->data, data->length - 8);

	if (ntver == NETLOGON_NT_VERSION_1) {
		ndr_err = ndr_pull_struct_blob_all(data, mem_ctx,
						   iconv_convenience,
						   &response->data.nt4,
						   (ndr_pull_flags_fn_t)ndr_pull_NETLOGON_SAM_LOGON_RESPONSE_NT40);
		response->ntver = NETLOGON_NT_VERSION_1;
		if (NDR_ERR_CODE_IS_SUCCESS(ndr_err) && DEBUGLEVEL >= 10) {
			NDR_PRINT_DEBUG(NETLOGON_SAM_LOGON_RESPONSE_NT40,
					&response->data.nt4);
		}

	} else if (ntver & NETLOGON_NT_VERSION_5EX) {
		struct ndr_pull *ndr;
		ndr = ndr_pull_init_blob(data, mem_ctx, iconv_convenience);
		if (!ndr) {
			return NT_STATUS_NO_MEMORY;
		}
		ndr_err = ndr_pull_NETLOGON_SAM_LOGON_RESPONSE_EX_with_flags(
			ndr, NDR_SCALARS|NDR_BUFFERS, &response->data.nt5_ex,
			ntver);
		if (ndr->offset < ndr->data_size) {
			ndr_err = ndr_pull_error(ndr, NDR_ERR_UNREAD_BYTES,
						 "not all bytes consumed ofs[%u] size[%u]",
						 ndr->offset, ndr->data_size);
		}
		response->ntver = NETLOGON_NT_VERSION_5EX;
		if (NDR_ERR_CODE_IS_SUCCESS(ndr_err) && DEBUGLEVEL >= 10) {
			NDR_PRINT_DEBUG(NETLOGON_SAM_LOGON_RESPONSE_EX,
					&response->data.nt5_ex);
		}

	} else if (ntver & NETLOGON_NT_VERSION_5) {
		ndr_err = ndr_pull_struct_blob_all(data, mem_ctx,
						   iconv_convenience,
						   &response->data.nt5,
						   (ndr_pull_flags_fn_t)ndr_pull_NETLOGON_SAM_LOGON_RESPONSE);
		response->ntver = NETLOGON_NT_VERSION_5;
		if (NDR_ERR_CODE_IS_SUCCESS(ndr_err) && DEBUGLEVEL >= 10) {
			NDR_PRINT_DEBUG(NETLOGON_SAM_LOGON_RESPONSE,
					&response->data.nt5);
		}
	} else {
		DEBUG(2,("failed to parse netlogon response of type 0x%02x - unknown response type\n",
			 ntver));
		dump_data(10, data->data, data->length);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(2,("failed to parse netlogon response of type 0x%02x\n",
			 ntver));
		dump_data(10, data->data, data->length);
		return ndr_map_error2ntstatus(ndr_err);
	}

	return NT_STATUS_OK;
}

void map_netlogon_samlogon_response(struct netlogon_samlogon_response *response)
{
	struct NETLOGON_SAM_LOGON_RESPONSE_EX response_5_ex;
	switch (response->ntver) {
	case NETLOGON_NT_VERSION_5EX:
		break;
	case NETLOGON_NT_VERSION_5:
		ZERO_STRUCT(response_5_ex);
		response_5_ex.command = response->data.nt5.command;
		response_5_ex.pdc_name = response->data.nt5.pdc_name;
		response_5_ex.user_name = response->data.nt5.user_name;
		response_5_ex.domain = response->data.nt5.domain_name;
		response_5_ex.domain_uuid = response->data.nt5.domain_uuid;
		response_5_ex.forest = response->data.nt5.forest;
		response_5_ex.dns_domain = response->data.nt5.dns_domain;
		response_5_ex.pdc_dns_name = response->data.nt5.pdc_dns_name;
		response_5_ex.sockaddr.pdc_ip = response->data.nt5.pdc_ip;
		response_5_ex.server_type = response->data.nt5.server_type;
		response_5_ex.nt_version = response->data.nt5.nt_version;
		response_5_ex.lmnt_token = response->data.nt5.lmnt_token;
		response_5_ex.lm20_token = response->data.nt5.lm20_token;
		response->ntver = NETLOGON_NT_VERSION_5EX;
		response->data.nt5_ex = response_5_ex;
		break;

	case NETLOGON_NT_VERSION_1:
		ZERO_STRUCT(response_5_ex);
		response_5_ex.command = response->data.nt4.command;
		response_5_ex.pdc_name = response->data.nt4.server;
		response_5_ex.user_name = response->data.nt4.user_name;
		response_5_ex.domain = response->data.nt4.domain;
		response_5_ex.nt_version = response->data.nt4.nt_version;
		response_5_ex.lmnt_token = response->data.nt4.lmnt_token;
		response_5_ex.lm20_token = response->data.nt4.lm20_token;
		response->ntver = NETLOGON_NT_VERSION_5EX;
		response->data.nt5_ex = response_5_ex;
		break;
	}
	return;
}

NTSTATUS push_nbt_netlogon_response(DATA_BLOB *data, TALLOC_CTX *mem_ctx,
				    struct smb_iconv_convenience *iconv_convenience,
				    struct nbt_netlogon_response *response)
{
	NTSTATUS status = NT_STATUS_INVALID_NETWORK_RESPONSE;
	enum ndr_err_code ndr_err;
	switch (response->response_type) {
	case NETLOGON_GET_PDC:
		ndr_err = ndr_push_struct_blob(data, mem_ctx,
					       iconv_convenience,
					       &response->data.get_pdc,
					       (ndr_push_flags_fn_t)ndr_push_nbt_netlogon_response_from_pdc);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			status = ndr_map_error2ntstatus(ndr_err);
			DEBUG(0,("Failed to parse netlogon packet of length %d: %s\n",
				 (int)data->length, nt_errstr(status)));
			if (DEBUGLVL(10)) {
				file_save("netlogon.dat", data->data, data->length);
			}
			return status;
		}
		status = NT_STATUS_OK;
		break;
	case NETLOGON_SAMLOGON:
		status = push_netlogon_samlogon_response(
			data, mem_ctx, iconv_convenience,
			&response->data.samlogon);
		break;
	}
	return status;
}


NTSTATUS pull_nbt_netlogon_response(DATA_BLOB *data, TALLOC_CTX *mem_ctx,
					 struct smb_iconv_convenience *iconv_convenience,
					 struct nbt_netlogon_response *response)
{
	NTSTATUS status = NT_STATUS_INVALID_NETWORK_RESPONSE;
	enum netlogon_command command;
	enum ndr_err_code ndr_err;
	if (data->length < 4) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	command = SVAL(data->data, 0);

	switch (command) {
	case NETLOGON_RESPONSE_FROM_PDC:
		ndr_err = ndr_pull_struct_blob_all(data, mem_ctx,
						   iconv_convenience,
						   &response->data.get_pdc,
						   (ndr_pull_flags_fn_t)ndr_pull_nbt_netlogon_response_from_pdc);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			status = ndr_map_error2ntstatus(ndr_err);
			DEBUG(0,("Failed to parse netlogon packet of length %d: %s\n",
				 (int)data->length, nt_errstr(status)));
			if (DEBUGLVL(10)) {
				file_save("netlogon.dat", data->data, data->length);
			}
			return status;
		}
		status = NT_STATUS_OK;
		response->response_type = NETLOGON_GET_PDC;
		break;
	case LOGON_SAM_LOGON_RESPONSE:
	case LOGON_SAM_LOGON_PAUSE_RESPONSE:
	case LOGON_SAM_LOGON_USER_UNKNOWN:
	case LOGON_SAM_LOGON_RESPONSE_EX:
	case LOGON_SAM_LOGON_PAUSE_RESPONSE_EX:
	case LOGON_SAM_LOGON_USER_UNKNOWN_EX:
		status = pull_netlogon_samlogon_response(
			data, mem_ctx, iconv_convenience,
			&response->data.samlogon);
		response->response_type = NETLOGON_SAMLOGON;
		break;

	/* These levels are queries, not responses */
	case LOGON_PRIMARY_QUERY:
	case NETLOGON_ANNOUNCE_UAS:
	case LOGON_SAM_LOGON_REQUEST:
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	return status;

}
