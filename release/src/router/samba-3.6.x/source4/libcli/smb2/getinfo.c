/* 
   Unix SMB/CIFS implementation.

   SMB2 client getinfo calls

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
  send a getinfo request
*/
struct smb2_request *smb2_getinfo_send(struct smb2_tree *tree, struct smb2_getinfo *io)
{
	struct smb2_request *req;
	NTSTATUS status;

	req = smb2_request_init_tree(tree, SMB2_OP_GETINFO, 0x28, true, 
				     io->in.blob.length);
	if (req == NULL) return NULL;

	SCVAL(req->out.body, 0x02, io->in.info_type);
	SCVAL(req->out.body, 0x03, io->in.info_class);
	SIVAL(req->out.body, 0x04, io->in.output_buffer_length);
	SIVAL(req->out.body, 0x0C, io->in.reserved);
	SIVAL(req->out.body, 0x08, io->in.input_buffer_length);
	SIVAL(req->out.body, 0x10, io->in.additional_information);
	SIVAL(req->out.body, 0x14, io->in.getinfo_flags);
	smb2_push_handle(req->out.body+0x18, &io->in.file.handle);

	/* this blob is used for quota queries */
	status = smb2_push_o32s32_blob(&req->out, 0x08, io->in.blob);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(req);
		return NULL;
	}
	smb2_transport_send(req);

	return req;
}


/*
  recv a getinfo reply
*/
NTSTATUS smb2_getinfo_recv(struct smb2_request *req, TALLOC_CTX *mem_ctx,
			   struct smb2_getinfo *io)
{
	NTSTATUS status;

	if (!smb2_request_receive(req) || 
	    smb2_request_is_error(req)) {
		return smb2_request_destroy(req);
	}

	SMB2_CHECK_PACKET_RECV(req, 0x08, true);

	status = smb2_pull_o16s16_blob(&req->in, mem_ctx, req->in.body+0x02, &io->out.blob);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return smb2_request_destroy(req);
}

/*
  sync getinfo request
*/
NTSTATUS smb2_getinfo(struct smb2_tree *tree, TALLOC_CTX *mem_ctx,
		      struct smb2_getinfo *io)
{
	struct smb2_request *req = smb2_getinfo_send(tree, io);
	return smb2_getinfo_recv(req, mem_ctx, io);
}


/*
  map a generic info level to a SMB2 info level
*/
uint16_t smb2_getinfo_map_level(uint16_t level, uint8_t info_class)
{
	if (info_class == SMB2_GETINFO_FILE &&
	    level == RAW_FILEINFO_SEC_DESC) {
		return SMB2_GETINFO_SECURITY;
	}
	if ((level & 0xFF) == info_class) {
		return level;
	} else if (level > 1000) {
		return ((level-1000)<<8) | info_class;
	}
	DEBUG(0,("Unable to map SMB2 info level 0x%04x of class %d\n",
		 level, info_class));
	return 0;	
}

/*
  level specific getinfo call - async send
*/
struct smb2_request *smb2_getinfo_file_send(struct smb2_tree *tree, union smb_fileinfo *io)
{
	struct smb2_getinfo b;
	uint16_t smb2_level = smb2_getinfo_map_level(io->generic.level, SMB2_GETINFO_FILE);
	
	if (smb2_level == 0) {
		return NULL;
	}

	ZERO_STRUCT(b);
	b.in.info_type            = smb2_level & 0xFF;
	b.in.info_class           = smb2_level >> 8;
	b.in.output_buffer_length = 0x10000;
	b.in.input_buffer_length  = 0;
	b.in.file.handle          = io->generic.in.file.handle;

	if (io->generic.level == RAW_FILEINFO_SEC_DESC) {
		b.in.additional_information = io->query_secdesc.in.secinfo_flags;
	}
	if (io->generic.level == RAW_FILEINFO_SMB2_ALL_EAS) {
		b.in.getinfo_flags = io->all_eas.in.continue_flags;
	}

	return smb2_getinfo_send(tree, &b);
}

/*
  recv a getinfo reply and parse the level info
*/
NTSTATUS smb2_getinfo_file_recv(struct smb2_request *req, TALLOC_CTX *mem_ctx,
				union smb_fileinfo *io)
{
	struct smb2_getinfo b;
	NTSTATUS status;

	status = smb2_getinfo_recv(req, mem_ctx, &b);
	NT_STATUS_NOT_OK_RETURN(status);

	status = smb_raw_fileinfo_passthru_parse(&b.out.blob, mem_ctx, io->generic.level, io);
	data_blob_free(&b.out.blob);

	return status;
}

/*
  level specific getinfo call
*/
NTSTATUS smb2_getinfo_file(struct smb2_tree *tree, TALLOC_CTX *mem_ctx, 
			   union smb_fileinfo *io)
{
	struct smb2_request *req = smb2_getinfo_file_send(tree, io);
	return smb2_getinfo_file_recv(req, mem_ctx, io);
}


/*
  level specific getinfo call - async send
*/
struct smb2_request *smb2_getinfo_fs_send(struct smb2_tree *tree, union smb_fsinfo *io)
{
	struct smb2_getinfo b;
	uint16_t smb2_level = smb2_getinfo_map_level(io->generic.level, SMB2_GETINFO_FS);
	
	if (smb2_level == 0) {
		return NULL;
	}
	
	ZERO_STRUCT(b);
	b.in.output_buffer_length = 0x10000;
	b.in.file.handle          = io->generic.handle;
	b.in.info_type            = smb2_level & 0xFF;
	b.in.info_class           = smb2_level >> 8;

	return smb2_getinfo_send(tree, &b);
}

/*
  recv a getinfo reply and parse the level info
*/
NTSTATUS smb2_getinfo_fs_recv(struct smb2_request *req, TALLOC_CTX *mem_ctx,
				union smb_fsinfo *io)
{
	struct smb2_getinfo b;
	NTSTATUS status;

	status = smb2_getinfo_recv(req, mem_ctx, &b);
	NT_STATUS_NOT_OK_RETURN(status);

	status = smb_raw_fsinfo_passthru_parse(b.out.blob, mem_ctx, io->generic.level, io);
	data_blob_free(&b.out.blob);

	return status;
}

/*
  level specific getinfo call
*/
NTSTATUS smb2_getinfo_fs(struct smb2_tree *tree, TALLOC_CTX *mem_ctx, 
			   union smb_fsinfo *io)
{
	struct smb2_request *req = smb2_getinfo_fs_send(tree, io);
	return smb2_getinfo_fs_recv(req, mem_ctx, io);
}

