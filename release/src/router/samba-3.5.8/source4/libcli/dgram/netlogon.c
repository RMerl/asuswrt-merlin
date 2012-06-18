/* 
   Unix SMB/CIFS implementation.

   handling for netlogon dgram requests

   Copyright (C) Andrew Tridgell 2005
   
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
#include "libcli/dgram/libdgram.h"
#include "lib/socket/socket.h"
#include "libcli/resolve/resolve.h"
#include "librpc/gen_ndr/ndr_nbt.h"

/* 
   send a netlogon mailslot request 
*/
NTSTATUS dgram_mailslot_netlogon_send(struct nbt_dgram_socket *dgmsock,
				      struct nbt_name *dest_name,
				      struct socket_address *dest,
				      const char *mailslot,
				      struct nbt_name *src_name,
				      struct nbt_netlogon_packet *request)
{
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	TALLOC_CTX *tmp_ctx = talloc_new(dgmsock);

	ndr_err = ndr_push_struct_blob(&blob, tmp_ctx, 
				       dgmsock->iconv_convenience,
				       request,
				      (ndr_push_flags_fn_t)ndr_push_nbt_netlogon_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(tmp_ctx);
		return ndr_map_error2ntstatus(ndr_err);
	}


	status = dgram_mailslot_send(dgmsock, DGRAM_DIRECT_UNIQUE, 
				     mailslot,
				     dest_name, dest, 
				     src_name, &blob);
	talloc_free(tmp_ctx);
	return status;
}


/* 
   send a netlogon mailslot reply
*/
NTSTATUS dgram_mailslot_netlogon_reply(struct nbt_dgram_socket *dgmsock,
				       struct nbt_dgram_packet *request,
				       const char *my_netbios_name,
				       const char *mailslot_name,
				       struct nbt_netlogon_response *reply)
{
	NTSTATUS status;
	DATA_BLOB blob;
	TALLOC_CTX *tmp_ctx = talloc_new(dgmsock);
	struct nbt_name myname;
	struct socket_address *dest;

	status = push_nbt_netlogon_response(&blob, tmp_ctx, dgmsock->iconv_convenience,
					    reply);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	make_nbt_name_client(&myname, my_netbios_name);

	dest = socket_address_from_strings(tmp_ctx, dgmsock->sock->backend_name, 
					   request->src_addr, request->src_port);
	if (!dest) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	status = dgram_mailslot_send(dgmsock, DGRAM_DIRECT_UNIQUE, 
				     mailslot_name,
				     &request->data.msg.source_name,
				     dest,
				     &myname, &blob);
	talloc_free(tmp_ctx);
	return status;
}


/*
  parse a netlogon response. The packet must be a valid mailslot packet
*/
NTSTATUS dgram_mailslot_netlogon_parse_request(struct dgram_mailslot_handler *dgmslot,
					       TALLOC_CTX *mem_ctx,
					       struct nbt_dgram_packet *dgram,
					       struct nbt_netlogon_packet *netlogon)
{
	DATA_BLOB data = dgram_mailslot_data(dgram);
	enum ndr_err_code ndr_err;

	ndr_err = ndr_pull_struct_blob(&data, mem_ctx, dgmslot->dgmsock->iconv_convenience, netlogon,
				      (ndr_pull_flags_fn_t)ndr_pull_nbt_netlogon_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("Failed to parse netlogon packet of length %d: %s\n",
			 (int)data.length, nt_errstr(status)));
		if (DEBUGLVL(10)) {
			file_save("netlogon.dat", data.data, data.length);
		}
		return status;
	}
	return NT_STATUS_OK;
}

/*
  parse a netlogon response. The packet must be a valid mailslot packet
*/
NTSTATUS dgram_mailslot_netlogon_parse_response(struct dgram_mailslot_handler *dgmslot,
				       TALLOC_CTX *mem_ctx,
				       struct nbt_dgram_packet *dgram,
				       struct nbt_netlogon_response *netlogon)
{
	NTSTATUS status;
	DATA_BLOB data = dgram_mailslot_data(dgram);
	
	status = pull_nbt_netlogon_response(&data, mem_ctx, dgmslot->dgmsock->iconv_convenience, netlogon);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	
	return NT_STATUS_OK;
}

