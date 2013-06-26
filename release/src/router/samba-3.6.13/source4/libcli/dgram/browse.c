/* 
   Unix SMB/CIFS implementation.

   handling for browsing dgram requests

   Copyright (C) Jelmer Vernooij 2005
        Heavily based on ntlogon.c
   
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
#include "param/param.h"

NTSTATUS dgram_mailslot_browse_send(struct nbt_dgram_socket *dgmsock,
				    struct nbt_name *dest_name,
				    struct socket_address *dest,
				    struct nbt_name *src_name,
				    struct nbt_browse_packet *request)
{
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	TALLOC_CTX *tmp_ctx = talloc_new(dgmsock);

	ndr_err = ndr_push_struct_blob(&blob, tmp_ctx, request,
				      (ndr_push_flags_fn_t)ndr_push_nbt_browse_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(tmp_ctx);
		return ndr_map_error2ntstatus(ndr_err);
	}

	status = dgram_mailslot_send(dgmsock, DGRAM_DIRECT_UNIQUE, 
				     NBT_MAILSLOT_BROWSE,
				     dest_name, dest, 
				     src_name, &blob);
	talloc_free(tmp_ctx);
	return status;
}

NTSTATUS dgram_mailslot_browse_reply(struct nbt_dgram_socket *dgmsock,
				     struct nbt_dgram_packet *request,
				     const char *mailslot_name,
				     const char *my_netbios_name,
				     struct nbt_browse_packet *reply)
{
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	TALLOC_CTX *tmp_ctx = talloc_new(dgmsock);
	struct nbt_name myname;
	struct socket_address *dest;

	ndr_err = ndr_push_struct_blob(&blob, tmp_ctx, reply,
				      (ndr_push_flags_fn_t)ndr_push_nbt_browse_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(tmp_ctx);
		return ndr_map_error2ntstatus(ndr_err);
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

NTSTATUS dgram_mailslot_browse_parse(struct dgram_mailslot_handler *dgmslot,
				     TALLOC_CTX *mem_ctx,
				     struct nbt_dgram_packet *dgram,
				     struct nbt_browse_packet *pkt)
{
	DATA_BLOB data = dgram_mailslot_data(dgram);
	enum ndr_err_code ndr_err;

	ndr_err = ndr_pull_struct_blob(&data, mem_ctx, pkt,
				      (ndr_pull_flags_fn_t)ndr_pull_nbt_browse_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("Failed to parse browse packet of length %d: %s\n",
			 (int)data.length, nt_errstr(status)));
		if (DEBUGLVL(10)) {
			file_save("browse.dat", data.data, data.length);
		}
		return status;
	}
	return NT_STATUS_OK;
}
