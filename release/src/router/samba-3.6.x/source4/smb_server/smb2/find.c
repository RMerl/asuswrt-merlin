/* 
   Unix SMB/CIFS implementation.
   SMB2 Find
   Copyright (C) Andrew Tridgell 2003
   Copyright (c) Stefan Metzmacher 2006

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
/*
   This file handles the parsing of transact2 requests
*/

#include "includes.h"
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include "smb_server/smb_server.h"
#include "smb_server/smb2/smb2_server.h"
#include "ntvfs/ntvfs.h"


/* a structure to encapsulate the state information about an in-progress ffirst/fnext operation */
struct smb2srv_find_state {
	struct smb2srv_request *req;
	struct smb2_find *info;
	union smb_search_first *ff;
	union smb_search_next *fn;
	uint32_t last_entry_offset;
};

/* callback function for SMB2 Find */
static bool smb2srv_find_callback(void *private_data, const union smb_search_data *file)
{
	struct smb2srv_find_state *state = talloc_get_type(private_data, struct smb2srv_find_state);
	struct smb2_find *info = state->info;
	uint32_t old_length;
	NTSTATUS status;

	old_length = info->out.blob.length;

	status = smbsrv_push_passthru_search(state, &info->out.blob, info->data_level, file, STR_UNICODE);
	if (!NT_STATUS_IS_OK(status) ||
	    info->out.blob.length > info->in.max_response_size) {
		/* restore the old length and tell the backend to stop */
		smbsrv_blob_grow_data(state, &info->out.blob, old_length);
		return false;
	}

	state->last_entry_offset = old_length;

	return true;
}

static void smb2srv_find_send(struct ntvfs_request *ntvfs)
{
	struct smb2srv_request *req;
	struct smb2srv_find_state *state;

	SMB2SRV_CHECK_ASYNC_STATUS(state, struct smb2srv_find_state);
	SMB2SRV_CHECK(smb2srv_setup_reply(req, 0x08, true, state->info->out.blob.length));

	if (state->info->out.blob.length > 0) {
		SIVAL(state->info->out.blob.data + state->last_entry_offset, 0, 0);
	}

	SMB2SRV_CHECK(smb2_push_o16s32_blob(&req->out, 0x02, state->info->out.blob));

	smb2srv_send_reply(req);
}

static NTSTATUS smb2srv_find_backend(struct smb2srv_find_state *state)
{
	struct smb2_find *info = state->info;

	switch (info->in.level) {
	case SMB2_FIND_DIRECTORY_INFO:
		info->data_level = RAW_SEARCH_DATA_DIRECTORY_INFO;
		break;

	case SMB2_FIND_FULL_DIRECTORY_INFO:
		info->data_level = RAW_SEARCH_DATA_FULL_DIRECTORY_INFO;
		break;

	case SMB2_FIND_BOTH_DIRECTORY_INFO:
		info->data_level = RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO;
		break;

	case SMB2_FIND_NAME_INFO:
		info->data_level = RAW_SEARCH_DATA_NAME_INFO;
		break;

	case SMB2_FIND_ID_BOTH_DIRECTORY_INFO:
		info->data_level = RAW_SEARCH_DATA_ID_BOTH_DIRECTORY_INFO;
		break;

	case SMB2_FIND_ID_FULL_DIRECTORY_INFO:
		info->data_level = RAW_SEARCH_DATA_ID_FULL_DIRECTORY_INFO;
		break;

	default:
		return NT_STATUS_FOOBAR;
	}

	if (info->in.continue_flags & SMB2_CONTINUE_FLAG_REOPEN) {
		state->ff = talloc(state, union smb_search_first);
		NT_STATUS_HAVE_NO_MEMORY(state->ff);

		state->ff->smb2 = *info;
		state->info = &state->ff->smb2;
		ZERO_STRUCT(state->ff->smb2.out);

		return ntvfs_search_first(state->req->ntvfs, state->ff, state, smb2srv_find_callback);
	} else {
		state->fn = talloc(state, union smb_search_next);
		NT_STATUS_HAVE_NO_MEMORY(state->fn);

		state->fn->smb2 = *info;
		state->info = &state->fn->smb2;
		ZERO_STRUCT(state->fn->smb2.out);

		return ntvfs_search_next(state->req->ntvfs, state->fn, state, smb2srv_find_callback);
	}
}

void smb2srv_find_recv(struct smb2srv_request *req)
{
	struct smb2srv_find_state *state;
	struct smb2_find *info;

	SMB2SRV_CHECK_BODY_SIZE(req, 0x20, true);
	SMB2SRV_TALLOC_IO_PTR(info, struct smb2_find);
	/* this overwrites req->io_ptr !*/
	SMB2SRV_TALLOC_IO_PTR(state, struct smb2srv_find_state);
	state->req		= req;
	state->info		= info;
	state->ff		= NULL;
	state->fn		= NULL;
	state->last_entry_offset= 0;
	SMB2SRV_SETUP_NTVFS_REQUEST(smb2srv_find_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	info->level			= RAW_SEARCH_SMB2;
	info->data_level		= RAW_SEARCH_DATA_GENERIC;/* will be overwritten later */
	info->in.level			= CVAL(req->in.body, 0x02);
	info->in.continue_flags		= CVAL(req->in.body, 0x03);
	info->in.file_index		= IVAL(req->in.body, 0x04);
	info->in.file.ntvfs		= smb2srv_pull_handle(req, req->in.body, 0x08);
	SMB2SRV_CHECK(smb2_pull_o16s16_string(&req->in, info, req->in.body+0x18, &info->in.pattern));
	info->in.max_response_size	= IVAL(req->in.body, 0x1C);

	/* the VFS backend does not yet handle NULL patterns */
	if (info->in.pattern == NULL) {
		info->in.pattern = "";
	}

	SMB2SRV_CHECK_FILE_HANDLE(info->in.file.ntvfs);
	SMB2SRV_CALL_NTVFS_BACKEND(smb2srv_find_backend(state));
}
