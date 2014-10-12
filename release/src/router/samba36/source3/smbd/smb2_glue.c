/*
   Unix SMB/CIFS implementation.
   Core SMB2 server

   Copyright (C) Stefan Metzmacher 2009

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
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "../libcli/smb/smb_common.h"

struct smb_request *smbd_smb2_fake_smb_request(struct smbd_smb2_request *req)
{
	struct smb_request *smbreq;
	const uint8_t *inhdr;
	int i = req->current_idx;

	inhdr = (const uint8_t *)req->in.vector[i+0].iov_base;

	smbreq = talloc_zero(req, struct smb_request);
	if (smbreq == NULL) {
		return NULL;
	}

	smbreq->vuid = req->session->compat_vuser->vuid;
	smbreq->tid = req->tcon->compat_conn->cnum;
	smbreq->conn = req->tcon->compat_conn;
	smbreq->sconn = req->sconn;
	smbreq->smbpid = (uint16_t)IVAL(inhdr, SMB2_HDR_PID);
	smbreq->flags2 = FLAGS2_UNICODE_STRINGS |
			 FLAGS2_32_BIT_ERROR_CODES |
			 FLAGS2_LONG_PATH_COMPONENTS |
			 FLAGS2_IS_LONG_NAME;
	if (IVAL(inhdr, SMB2_HDR_FLAGS) & SMB2_HDR_FLAG_DFS) {
		smbreq->flags2 |= FLAGS2_DFS_PATHNAMES;
	}
	smbreq->mid = BVAL(inhdr, SMB2_HDR_MESSAGE_ID);
	smbreq->chain_fsp = req->compat_chain_fsp;
	smbreq->smb2req = req;
	req->smb1req = smbreq;

	return smbreq;
}

/*********************************************************
 Called from file_free() to remove any chained fsp pointers.
*********************************************************/

void remove_smb2_chained_fsp(files_struct *fsp)
{
	struct smbd_server_connection *sconn = fsp->conn->sconn;
	struct smbd_smb2_request *smb2req;

	for (smb2req = sconn->smb2.requests; smb2req; smb2req = smb2req->next) {
		if (smb2req->compat_chain_fsp == fsp) {
			smb2req->compat_chain_fsp = NULL;
		}
		if (smb2req->smb1req && smb2req->smb1req->chain_fsp == fsp) {
			smb2req->smb1req->chain_fsp = NULL;
		}
	}
}
