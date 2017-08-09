/* 
   Unix SMB/CIFS implementation.

   SMB2 client find calls

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
  send a find request
*/
struct smb2_request *smb2_find_send(struct smb2_tree *tree, struct smb2_find *io)
{
	struct smb2_request *req;
	NTSTATUS status;

	req = smb2_request_init_tree(tree, SMB2_OP_FIND, 0x20, true, 0);
	if (req == NULL) return NULL;

	SCVAL(req->out.body, 0x02, io->in.level);
	SCVAL(req->out.body, 0x03, io->in.continue_flags);
	SIVAL(req->out.body, 0x04, io->in.file_index);
	smb2_push_handle(req->out.body+0x08, &io->in.file.handle);

	status = smb2_push_o16s16_string(&req->out, 0x18, io->in.pattern);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(req);
		return NULL;
	}

	SIVAL(req->out.body, 0x1C, io->in.max_response_size);

	smb2_transport_send(req);

	return req;
}


/*
  recv a find reply
*/
NTSTATUS smb2_find_recv(struct smb2_request *req, TALLOC_CTX *mem_ctx,
			   struct smb2_find *io)
{
	NTSTATUS status;

	if (!smb2_request_receive(req) || 
	    smb2_request_is_error(req)) {
		return smb2_request_destroy(req);
	}

	SMB2_CHECK_PACKET_RECV(req, 0x08, true);

	status = smb2_pull_o16s32_blob(&req->in, mem_ctx, 
				       req->in.body+0x02, &io->out.blob);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return smb2_request_destroy(req);
}

/*
  sync find request
*/
NTSTATUS smb2_find(struct smb2_tree *tree, TALLOC_CTX *mem_ctx,
		   struct smb2_find *io)
{
	struct smb2_request *req = smb2_find_send(tree, io);
	return smb2_find_recv(req, mem_ctx, io);
}


/*
  a varient of smb2_find_recv that parses the resulting blob into
  smb_search_data structures
*/
NTSTATUS smb2_find_level_recv(struct smb2_request *req, TALLOC_CTX *mem_ctx,
			      uint8_t level, unsigned int *count,
			      union smb_search_data **io)
{
	struct smb2_find f;
	NTSTATUS status;
	DATA_BLOB b;
	enum smb_search_data_level smb_level;
	unsigned int next_ofs=0;

	switch (level) {
	case SMB2_FIND_DIRECTORY_INFO:
		smb_level = RAW_SEARCH_DATA_DIRECTORY_INFO;
		break;
	case SMB2_FIND_FULL_DIRECTORY_INFO:
		smb_level = RAW_SEARCH_DATA_FULL_DIRECTORY_INFO;
		break;
	case SMB2_FIND_BOTH_DIRECTORY_INFO:
		smb_level = RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO;
		break;
	case SMB2_FIND_NAME_INFO:
		smb_level = RAW_SEARCH_DATA_NAME_INFO;
		break;
	case SMB2_FIND_ID_FULL_DIRECTORY_INFO:
		smb_level = RAW_SEARCH_DATA_ID_FULL_DIRECTORY_INFO;
		break;
	case SMB2_FIND_ID_BOTH_DIRECTORY_INFO:
		smb_level = RAW_SEARCH_DATA_ID_BOTH_DIRECTORY_INFO;
		break;
	default:
		return NT_STATUS_INVALID_INFO_CLASS;
	}

	status = smb2_find_recv(req, mem_ctx, &f);
	NT_STATUS_NOT_OK_RETURN(status);
	
	b = f.out.blob;
	*io = NULL;
	*count = 0;

	do {
		union smb_search_data *io2;

		io2 = talloc_realloc(mem_ctx, *io, union smb_search_data, (*count)+1);
		if (io2 == NULL) {
			data_blob_free(&f.out.blob);
			talloc_free(*io);
			return NT_STATUS_NO_MEMORY;
		}
		*io = io2;

		status = smb_raw_search_common(*io, smb_level, &b, (*io) + (*count), 
					       &next_ofs, STR_UNICODE);

		if (NT_STATUS_IS_OK(status) &&
		    next_ofs >= b.length) {
			data_blob_free(&f.out.blob);
			talloc_free(*io);
			return NT_STATUS_INFO_LENGTH_MISMATCH;			
		}

		(*count)++;

		b = data_blob_const(b.data+next_ofs, b.length - next_ofs);
	} while (NT_STATUS_IS_OK(status) && next_ofs != 0);

	data_blob_free(&f.out.blob);
	
	return NT_STATUS_OK;
}

/*
  a varient of smb2_find that parses the resulting blob into
  smb_search_data structures
*/
NTSTATUS smb2_find_level(struct smb2_tree *tree, TALLOC_CTX *mem_ctx,
			 struct smb2_find *f, 
			 unsigned int *count, union smb_search_data **io)
{
	struct smb2_request *req;

	req = smb2_find_send(tree, f);
	return smb2_find_level_recv(req, mem_ctx, f->in.level, count, io);
}
