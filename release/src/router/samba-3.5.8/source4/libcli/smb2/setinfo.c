/* 
   Unix SMB/CIFS implementation.

   SMB2 client setinfo calls

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
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"

/*
  send a setinfo request
*/
struct smb2_request *smb2_setinfo_send(struct smb2_tree *tree, struct smb2_setinfo *io)
{
	NTSTATUS status;
	struct smb2_request *req;

	req = smb2_request_init_tree(tree, SMB2_OP_SETINFO, 0x20, true, io->in.blob.length);
	if (req == NULL) return NULL;

	SSVAL(req->out.body, 0x02, io->in.level);

	status = smb2_push_s32o32_blob(&req->out, 0x04, io->in.blob);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(req);
		return NULL;
	}

	SIVAL(req->out.body, 0x0C, io->in.flags);
	smb2_push_handle(req->out.body+0x10, &io->in.file.handle);

	smb2_transport_send(req);

	return req;
}


/*
  recv a setinfo reply
*/
NTSTATUS smb2_setinfo_recv(struct smb2_request *req)
{
	if (!smb2_request_receive(req) || 
	    !smb2_request_is_ok(req)) {
		return smb2_request_destroy(req);
	}

	SMB2_CHECK_PACKET_RECV(req, 0x02, false);

	return smb2_request_destroy(req);
}

/*
  sync setinfo request
*/
NTSTATUS smb2_setinfo(struct smb2_tree *tree, struct smb2_setinfo *io)
{
	struct smb2_request *req = smb2_setinfo_send(tree, io);
	return smb2_setinfo_recv(req);
}

/*
  level specific file setinfo call - async send
*/
struct smb2_request *smb2_setinfo_file_send(struct smb2_tree *tree, union smb_setfileinfo *io)
{
	struct smb2_setinfo b;
	uint16_t smb2_level = smb2_getinfo_map_level(io->generic.level, SMB2_GETINFO_FILE);
	struct smb2_request *req;
	
	if (smb2_level == 0) {
		return NULL;
	}

	ZERO_STRUCT(b);
	b.in.level             = smb2_level;
	b.in.file.handle       = io->generic.in.file.handle;

	/* change levels so the parsers know it is SMB2 */
	if (io->generic.level == RAW_SFILEINFO_RENAME_INFORMATION) {
		io->generic.level = RAW_SFILEINFO_RENAME_INFORMATION_SMB2;
	}

	if (!smb_raw_setfileinfo_passthru(tree, io->generic.level, io, &b.in.blob)) {
		return NULL;
	}

	if (io->generic.level == RAW_SFILEINFO_SEC_DESC) {
		b.in.flags = io->set_secdesc.in.secinfo_flags;
	}

	req = smb2_setinfo_send(tree, &b);
	data_blob_free(&b.in.blob);
	return req;
}

/*
  level specific file setinfo call - sync
*/
NTSTATUS smb2_setinfo_file(struct smb2_tree *tree, union smb_setfileinfo *io)
{
	struct smb2_request *req = smb2_setinfo_file_send(tree, io);
	return smb2_setinfo_recv(req);
}
