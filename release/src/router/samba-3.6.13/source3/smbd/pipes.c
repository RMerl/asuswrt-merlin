/* 
   Unix SMB/CIFS implementation.
   Pipe SMB reply routines
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Luke Kenneth Casson Leighton 1996-1998
   Copyright (C) Paul Ashton  1997-1998.
   Copyright (C) Jeremy Allison 2005.
   
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
   This file handles reply_ calls on named pipes that the server
   makes to handle specific protocols
*/


#include "includes.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "libcli/security/security.h"
#include "rpc_server/srv_pipe_hnd.h"

#define	PIPE		"\\PIPE\\"
#define	PIPELEN		strlen(PIPE)

#define MAX_PIPE_NAME_LEN	24

NTSTATUS open_np_file(struct smb_request *smb_req, const char *name,
		      struct files_struct **pfsp)
{
	struct connection_struct *conn = smb_req->conn;
	struct files_struct *fsp;
	struct smb_filename *smb_fname = NULL;
	NTSTATUS status;

	status = file_new(smb_req, conn, &fsp);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("file_new failed: %s\n", nt_errstr(status)));
		return status;
	}

	fsp->conn = conn;
	fsp->fh->fd = -1;
	fsp->vuid = smb_req->vuid;
	fsp->can_lock = false;
	fsp->access_mask = FILE_READ_DATA | FILE_WRITE_DATA;

	status = create_synthetic_smb_fname(talloc_tos(), name, NULL, NULL,
					    &smb_fname);
		if (!NT_STATUS_IS_OK(status)) {
		file_free(smb_req, fsp);
		return status;
	}
	status = fsp_set_smb_fname(fsp, smb_fname);
	TALLOC_FREE(smb_fname);
	if (!NT_STATUS_IS_OK(status)) {
		file_free(smb_req, fsp);
		return status;
	}

	status = np_open(fsp, name,
			 conn->sconn->local_address,
			 conn->sconn->remote_address,
			 &conn->sconn->client_id,
			 conn->session_info,
			 conn->sconn->msg_ctx,
			 &fsp->fake_file_handle);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("np_open(%s) returned %s\n", name,
			   nt_errstr(status)));
		file_free(smb_req, fsp);
		return status;
	}

	*pfsp = fsp;

	return NT_STATUS_OK;
}

/****************************************************************************
 Reply to an open and X on a named pipe.
 This code is basically stolen from reply_open_and_X with some
 wrinkles to handle pipes.
****************************************************************************/

void reply_open_pipe_and_X(connection_struct *conn, struct smb_request *req)
{
	const char *fname = NULL;
	char *pipe_name = NULL;
	files_struct *fsp;
	TALLOC_CTX *ctx = talloc_tos();
	NTSTATUS status;

	/* XXXX we need to handle passed times, sattr and flags */
	srvstr_pull_req_talloc(ctx, req, &pipe_name, req->buf, STR_TERMINATE);
	if (!pipe_name) {
		reply_botherror(req, NT_STATUS_OBJECT_NAME_NOT_FOUND,
				ERRDOS, ERRbadpipe);
		return;
	}

	/* If the name doesn't start \PIPE\ then this is directed */
	/* at a mailslot or something we really, really don't understand, */
	/* not just something we really don't understand. */
	if ( strncmp(pipe_name,PIPE,PIPELEN) != 0 ) {
		reply_nterror(req, NT_STATUS_ACCESS_DENIED);
		return;
	}

	DEBUG(4,("Opening pipe %s.\n", pipe_name));

	/* Strip \PIPE\ off the name. */
	fname = pipe_name + PIPELEN;

#if 0
	/*
	 * Hack for NT printers... JRA.
	 */
	if(should_fail_next_srvsvc_open(fname)) {
		reply_nterror(req, NT_STATUS_ACCESS_DENIED);
		return;
	}
#endif

	status = open_np_file(req, fname, &fsp);
	if (!NT_STATUS_IS_OK(status)) {
		if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			reply_botherror(req, NT_STATUS_OBJECT_NAME_NOT_FOUND,
					ERRDOS, ERRbadpipe);
			return;
		}
		reply_nterror(req, status);
		return;
	}

	/* Prepare the reply */
	reply_outbuf(req, 15, 0);

	/* Mark the opened file as an existing named pipe in message mode. */
	SSVAL(req->outbuf,smb_vwv9,2);
	SSVAL(req->outbuf,smb_vwv10,0xc700);

	SSVAL(req->outbuf, smb_vwv2, fsp->fnum);
	SSVAL(req->outbuf, smb_vwv3, 0);	/* fmode */
	srv_put_dos_date3((char *)req->outbuf, smb_vwv4, 0);	/* mtime */
	SIVAL(req->outbuf, smb_vwv6, 0);	/* size */
	SSVAL(req->outbuf, smb_vwv8, 0);	/* rmode */
	SSVAL(req->outbuf, smb_vwv11, 0x0001);

	chain_reply(req);
	return;
}

/****************************************************************************
 Reply to a write on a pipe.
****************************************************************************/

struct pipe_write_state {
	size_t numtowrite;
};

static void pipe_write_done(struct tevent_req *subreq);

void reply_pipe_write(struct smb_request *req)
{
	files_struct *fsp = file_fsp(req, SVAL(req->vwv+0, 0));
	const uint8_t *data;
	struct pipe_write_state *state;
	struct tevent_req *subreq;

	if (!fsp_is_np(fsp)) {
		reply_nterror(req, NT_STATUS_INVALID_HANDLE);
		return;
	}

	if (fsp->vuid != req->vuid) {
		reply_nterror(req, NT_STATUS_INVALID_HANDLE);
		return;
	}

	state = talloc(req, struct pipe_write_state);
	if (state == NULL) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
	req->async_priv = state;

	state->numtowrite = SVAL(req->vwv+1, 0);

	data = req->buf + 3;

	DEBUG(6, ("reply_pipe_write: %x name: %s len: %d\n", (int)fsp->fnum,
		  fsp_str_dbg(fsp), (int)state->numtowrite));

	subreq = np_write_send(state, smbd_event_context(),
			       fsp->fake_file_handle, data, state->numtowrite);
	if (subreq == NULL) {
		TALLOC_FREE(state);
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
	tevent_req_set_callback(subreq, pipe_write_done,
				talloc_move(req->conn, &req));
}

static void pipe_write_done(struct tevent_req *subreq)
{
	struct smb_request *req = tevent_req_callback_data(
		subreq, struct smb_request);
	struct pipe_write_state *state = talloc_get_type_abort(
		req->async_priv, struct pipe_write_state);
	NTSTATUS status;
	ssize_t nwritten = -1;

	status = np_write_recv(subreq, &nwritten);
	TALLOC_FREE(subreq);
	if (nwritten < 0) {
		reply_nterror(req, status);
		goto send;
	}

	/* Looks bogus to me now. Needs to be removed ? JRA. */
	if ((nwritten == 0 && state->numtowrite != 0)) {
		reply_nterror(req, NT_STATUS_ACCESS_DENIED);
		goto send;
	}

	reply_outbuf(req, 1, 0);

	SSVAL(req->outbuf,smb_vwv0,nwritten);

	DEBUG(3,("write-IPC nwritten=%d\n", (int)nwritten));

 send:
	if (!srv_send_smb(req->sconn, (char *)req->outbuf,
			  true, req->seqnum+1,
			  IS_CONN_ENCRYPTED(req->conn)||req->encrypted,
			  &req->pcd)) {
		exit_server_cleanly("construct_reply: srv_send_smb failed.");
	}
	TALLOC_FREE(req);
}

/****************************************************************************
 Reply to a write and X.

 This code is basically stolen from reply_write_and_X with some
 wrinkles to handle pipes.
****************************************************************************/

struct pipe_write_andx_state {
	bool pipe_start_message_raw;
	size_t numtowrite;
};

static void pipe_write_andx_done(struct tevent_req *subreq);

void reply_pipe_write_and_X(struct smb_request *req)
{
	files_struct *fsp = file_fsp(req, SVAL(req->vwv+2, 0));
	int smb_doff = SVAL(req->vwv+11, 0);
	uint8_t *data;
	struct pipe_write_andx_state *state;
	struct tevent_req *subreq;

	if (!fsp_is_np(fsp)) {
		reply_nterror(req, NT_STATUS_INVALID_HANDLE);
		return;
	}

	if (fsp->vuid != req->vuid) {
		reply_nterror(req, NT_STATUS_INVALID_HANDLE);
		return;
	}

	state = talloc(req, struct pipe_write_andx_state);
	if (state == NULL) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
	req->async_priv = state;

	state->numtowrite = SVAL(req->vwv+10, 0);
	state->pipe_start_message_raw =
		((SVAL(req->vwv+7, 0) & (PIPE_START_MESSAGE|PIPE_RAW_MODE))
		 == (PIPE_START_MESSAGE|PIPE_RAW_MODE));

	DEBUG(6, ("reply_pipe_write_and_X: %x name: %s len: %d\n",
		  (int)fsp->fnum, fsp_str_dbg(fsp), (int)state->numtowrite));

	data = (uint8_t *)smb_base(req->inbuf) + smb_doff;

	if (state->pipe_start_message_raw) {
		/*
		 * For the start of a message in named pipe byte mode,
		 * the first two bytes are a length-of-pdu field. Ignore
		 * them (we don't trust the client). JRA.
		 */
		if (state->numtowrite < 2) {
			DEBUG(0,("reply_pipe_write_and_X: start of message "
				 "set and not enough data sent.(%u)\n",
				 (unsigned int)state->numtowrite ));
			reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
			return;
		}

		data += 2;
		state->numtowrite -= 2;
	}

	subreq = np_write_send(state, smbd_event_context(),
			       fsp->fake_file_handle, data, state->numtowrite);
	if (subreq == NULL) {
		TALLOC_FREE(state);
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
	tevent_req_set_callback(subreq, pipe_write_andx_done,
				talloc_move(req->conn, &req));
}

static void pipe_write_andx_done(struct tevent_req *subreq)
{
	struct smb_request *req = tevent_req_callback_data(
		subreq, struct smb_request);
	struct pipe_write_andx_state *state = talloc_get_type_abort(
		req->async_priv, struct pipe_write_andx_state);
	NTSTATUS status;
	ssize_t nwritten = -1;

	status = np_write_recv(subreq, &nwritten);
	TALLOC_FREE(subreq);

	if (!NT_STATUS_IS_OK(status)) {
		reply_nterror(req, status);
		goto done;
	}

	/* Looks bogus to me now. Is this error message correct ? JRA. */
	if (nwritten != state->numtowrite) {
		reply_nterror(req, NT_STATUS_ACCESS_DENIED);
		goto done;
	}

	reply_outbuf(req, 6, 0);

	nwritten = (state->pipe_start_message_raw ? nwritten + 2 : nwritten);
	SSVAL(req->outbuf,smb_vwv2,nwritten);

	DEBUG(3,("writeX-IPC nwritten=%d\n", (int)nwritten));

 done:
	chain_reply(req);
	/*
	 * We must free here as the ownership of req was
	 * moved to the connection struct in reply_pipe_write_and_X().
	 */
	TALLOC_FREE(req);
}

/****************************************************************************
 Reply to a read and X.
 This code is basically stolen from reply_read_and_X with some
 wrinkles to handle pipes.
****************************************************************************/

struct pipe_read_andx_state {
	uint8_t *outbuf;
	int smb_mincnt;
	int smb_maxcnt;
};

static void pipe_read_andx_done(struct tevent_req *subreq);

void reply_pipe_read_and_X(struct smb_request *req)
{
	files_struct *fsp = file_fsp(req, SVAL(req->vwv+0, 0));
	uint8_t *data;
	struct pipe_read_andx_state *state;
	struct tevent_req *subreq;

	/* we don't use the offset given to use for pipe reads. This
           is deliberate, instead we always return the next lump of
           data on the pipe */
#if 0
	uint32 smb_offs = IVAL(req->vwv+3, 0);
#endif

	if (!fsp_is_np(fsp)) {
		reply_nterror(req, NT_STATUS_INVALID_HANDLE);
		return;
	}

	if (fsp->vuid != req->vuid) {
		reply_nterror(req, NT_STATUS_INVALID_HANDLE);
		return;
	}

	state = talloc(req, struct pipe_read_andx_state);
	if (state == NULL) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
	req->async_priv = state;

	state->smb_maxcnt = SVAL(req->vwv+5, 0);
	state->smb_mincnt = SVAL(req->vwv+6, 0);

	reply_outbuf(req, 12, state->smb_maxcnt);
	data = (uint8_t *)smb_buf(req->outbuf);

	/*
	 * We have to tell the upper layers that we're async.
	 */
	state->outbuf = req->outbuf;
	req->outbuf = NULL;

	subreq = np_read_send(state, smbd_event_context(),
			      fsp->fake_file_handle, data,
			      state->smb_maxcnt);
	if (subreq == NULL) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
	tevent_req_set_callback(subreq, pipe_read_andx_done,
				talloc_move(req->conn, &req));
}

static void pipe_read_andx_done(struct tevent_req *subreq)
{
	struct smb_request *req = tevent_req_callback_data(
		subreq, struct smb_request);
	struct pipe_read_andx_state *state = talloc_get_type_abort(
		req->async_priv, struct pipe_read_andx_state);
	NTSTATUS status;
	ssize_t nread;
	bool is_data_outstanding;

	status = np_read_recv(subreq, &nread, &is_data_outstanding);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		reply_nterror(req, status);
		goto done;
	}

	req->outbuf = state->outbuf;
	state->outbuf = NULL;

	srv_set_message((char *)req->outbuf, 12, nread, False);

#if 0
	/*
	 * we should return STATUS_BUFFER_OVERFLOW if there's
	 * out standing data.
	 *
	 * But we can't enable it yet, as it has bad interactions
	 * with fixup_chain_error_packet() in chain_reply().
	 */
	if (is_data_outstanding) {
		error_packet_set((char *)req->outbuf, ERRDOS, ERRmoredata,
				 STATUS_BUFFER_OVERFLOW, __LINE__, __FILE__);
	}
#endif

	SSVAL(req->outbuf,smb_vwv5,nread);
	SSVAL(req->outbuf,smb_vwv6,
	      req_wct_ofs(req)
	      + 1 		/* the wct field */
	      + 12 * sizeof(uint16_t) /* vwv */
	      + 2);		/* the buflen field */
	SSVAL(req->outbuf,smb_vwv11,state->smb_maxcnt);
  
	DEBUG(3,("readX-IPC min=%d max=%d nread=%d\n",
		 state->smb_mincnt, state->smb_maxcnt, (int)nread));

 done:
	chain_reply(req);
	/*
	 * We must free here as the ownership of req was
	 * moved to the connection struct in reply_pipe_read_and_X().
	 */
	TALLOC_FREE(req);
}
