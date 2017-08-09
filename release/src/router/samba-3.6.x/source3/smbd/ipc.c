/* 
   Unix SMB/CIFS implementation.
   Inter-process communication and named pipe handling
   Copyright (C) Andrew Tridgell 1992-1998

   SMB Version handling
   Copyright (C) John H Terpstra 1995-1998

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
   This file handles the named pipe and mailslot calls
   in the SMBtrans protocol
   */

#include "includes.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "smbprofile.h"
#include "rpc_server/srv_pipe_hnd.h"

#define NERR_notsupported 50

static void api_no_reply(connection_struct *conn, struct smb_request *req);

/*******************************************************************
 copies parameters and data, as needed, into the smb buffer

 *both* the data and params sections should be aligned.  this
 is fudged in the rpc pipes by 
 at present, only the data section is.  this may be a possible
 cause of some of the ipc problems being experienced.  lkcl26dec97

 ******************************************************************/

static void copy_trans_params_and_data(char *outbuf, int align,
				char *rparam, int param_offset, int param_len,
				char *rdata, int data_offset, int data_len)
{
	char *copy_into = smb_buf(outbuf);

	if(param_len < 0)
		param_len = 0;

	if(data_len < 0)
		data_len = 0;

	DEBUG(5,("copy_trans_params_and_data: params[%d..%d] data[%d..%d] (align %d)\n",
			param_offset, param_offset + param_len,
			data_offset , data_offset  + data_len,
			align));

	*copy_into = '\0';

	copy_into += 1;

	if (param_len)
		memcpy(copy_into, &rparam[param_offset], param_len);

	copy_into += param_len;
	if (align) {
		memset(copy_into, '\0', align);
	}

	copy_into += align;

	if (data_len )
		memcpy(copy_into, &rdata[data_offset], data_len);
}

/****************************************************************************
 Send a trans reply.
 ****************************************************************************/

void send_trans_reply(connection_struct *conn,
		      struct smb_request *req,
		      char *rparam, int rparam_len,
		      char *rdata, int rdata_len,
		      bool buffer_too_large)
{
	int this_ldata,this_lparam;
	int tot_data_sent = 0;
	int tot_param_sent = 0;
	int align;

	int ldata  = rdata  ? rdata_len : 0;
	int lparam = rparam ? rparam_len : 0;
	struct smbd_server_connection *sconn = req->sconn;
	int max_send = sconn->smb1.sessions.max_send;

	if (buffer_too_large)
		DEBUG(5,("send_trans_reply: buffer %d too large\n", ldata ));

	this_lparam = MIN(lparam,max_send - 500); /* hack */
	this_ldata  = MIN(ldata,max_send - (500+this_lparam));

	align = ((this_lparam)%4);

	reply_outbuf(req, 10, 1+align+this_ldata+this_lparam);

	/*
	 * We might have SMBtranss in req which was transferred to the outbuf,
	 * fix that.
	 */
	SCVAL(req->outbuf, smb_com, SMBtrans);

	copy_trans_params_and_data((char *)req->outbuf, align,
				rparam, tot_param_sent, this_lparam,
				rdata, tot_data_sent, this_ldata);

	SSVAL(req->outbuf,smb_vwv0,lparam);
	SSVAL(req->outbuf,smb_vwv1,ldata);
	SSVAL(req->outbuf,smb_vwv3,this_lparam);
	SSVAL(req->outbuf,smb_vwv4,
	      smb_offset(smb_buf(req->outbuf)+1, req->outbuf));
	SSVAL(req->outbuf,smb_vwv5,0);
	SSVAL(req->outbuf,smb_vwv6,this_ldata);
	SSVAL(req->outbuf,smb_vwv7,
	      smb_offset(smb_buf(req->outbuf)+1+this_lparam+align,
			 req->outbuf));
	SSVAL(req->outbuf,smb_vwv8,0);
	SSVAL(req->outbuf,smb_vwv9,0);

	if (buffer_too_large) {
		error_packet_set((char *)req->outbuf, ERRDOS, ERRmoredata,
				 STATUS_BUFFER_OVERFLOW, __LINE__, __FILE__);
	}

	show_msg((char *)req->outbuf);
	if (!srv_send_smb(sconn, (char *)req->outbuf,
			  true, req->seqnum+1,
			  IS_CONN_ENCRYPTED(conn), &req->pcd)) {
		exit_server_cleanly("send_trans_reply: srv_send_smb failed.");
	}

	TALLOC_FREE(req->outbuf);

	tot_data_sent = this_ldata;
	tot_param_sent = this_lparam;

	while (tot_data_sent < ldata || tot_param_sent < lparam)
	{
		this_lparam = MIN(lparam-tot_param_sent,
				  max_send - 500); /* hack */
		this_ldata  = MIN(ldata -tot_data_sent,
				  max_send - (500+this_lparam));

		if(this_lparam < 0)
			this_lparam = 0;

		if(this_ldata < 0)
			this_ldata = 0;

		align = (this_lparam%4);

		reply_outbuf(req, 10, 1+align+this_ldata+this_lparam);

		/*
		 * We might have SMBtranss in req which was transferred to the
		 * outbuf, fix that.
		 */
		SCVAL(req->outbuf, smb_com, SMBtrans);

		copy_trans_params_and_data((char *)req->outbuf, align,
					   rparam, tot_param_sent, this_lparam,
					   rdata, tot_data_sent, this_ldata);

		SSVAL(req->outbuf,smb_vwv0,lparam);
		SSVAL(req->outbuf,smb_vwv1,ldata);

		SSVAL(req->outbuf,smb_vwv3,this_lparam);
		SSVAL(req->outbuf,smb_vwv4,
		      smb_offset(smb_buf(req->outbuf)+1,req->outbuf));
		SSVAL(req->outbuf,smb_vwv5,tot_param_sent);
		SSVAL(req->outbuf,smb_vwv6,this_ldata);
		SSVAL(req->outbuf,smb_vwv7,
		      smb_offset(smb_buf(req->outbuf)+1+this_lparam+align,
				 req->outbuf));
		SSVAL(req->outbuf,smb_vwv8,tot_data_sent);
		SSVAL(req->outbuf,smb_vwv9,0);

		if (buffer_too_large) {
			error_packet_set((char *)req->outbuf,
					 ERRDOS, ERRmoredata,
					 STATUS_BUFFER_OVERFLOW,
					 __LINE__, __FILE__);
		}

		show_msg((char *)req->outbuf);
		if (!srv_send_smb(sconn, (char *)req->outbuf,
				  true, req->seqnum+1,
				  IS_CONN_ENCRYPTED(conn), &req->pcd))
			exit_server_cleanly("send_trans_reply: srv_send_smb "
					    "failed.");

		tot_data_sent  += this_ldata;
		tot_param_sent += this_lparam;
		TALLOC_FREE(req->outbuf);
	}
}

/****************************************************************************
 Start the first part of an RPC reply which began with an SMBtrans request.
****************************************************************************/

struct dcerpc_cmd_state {
	struct fake_file_handle *handle;
	uint8_t *data;
	size_t num_data;
	size_t max_read;
};

static void api_dcerpc_cmd_write_done(struct tevent_req *subreq);
static void api_dcerpc_cmd_read_done(struct tevent_req *subreq);

static void api_dcerpc_cmd(connection_struct *conn, struct smb_request *req,
			   files_struct *fsp, uint8_t *data, size_t length,
			   size_t max_read)
{
	struct tevent_req *subreq;
	struct dcerpc_cmd_state *state;
	bool busy;

	if (!fsp_is_np(fsp)) {
		api_no_reply(conn, req);
		return;
	}

	/*
	 * Trans requests are only allowed
	 * if no other Trans or Read is active
	 */
	busy = np_read_in_progress(fsp->fake_file_handle);
	if (busy) {
		reply_nterror(req, NT_STATUS_PIPE_BUSY);
		return;
	}

	state = talloc(req, struct dcerpc_cmd_state);
	if (state == NULL) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
	req->async_priv = state;

	state->handle = fsp->fake_file_handle;

	/*
	 * This memdup severely sucks. But doing it properly essentially means
	 * to rewrite lanman.c, something which I don't really want to do now.
	 */
	state->data = (uint8_t *)talloc_memdup(state, data, length);
	if (state->data == NULL) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
	state->num_data = length;
	state->max_read = max_read;

	subreq = np_write_send(state, smbd_event_context(), state->handle,
			       state->data, length);
	if (subreq == NULL) {
		TALLOC_FREE(state);
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
	tevent_req_set_callback(subreq, api_dcerpc_cmd_write_done,
				talloc_move(conn, &req));
}

static void api_dcerpc_cmd_write_done(struct tevent_req *subreq)
{
	struct smb_request *req = tevent_req_callback_data(
		subreq, struct smb_request);
	struct dcerpc_cmd_state *state = talloc_get_type_abort(
		req->async_priv, struct dcerpc_cmd_state);
	NTSTATUS status;
	ssize_t nwritten = -1;

	status = np_write_recv(subreq, &nwritten);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status) || (nwritten != state->num_data)) {
		DEBUG(10, ("Could not write to pipe: %s (%d/%d)\n",
			   nt_errstr(status), (int)state->num_data,
			   (int)nwritten));
		reply_nterror(req, NT_STATUS_PIPE_NOT_AVAILABLE);
		goto send;
	}

	state->data = TALLOC_REALLOC_ARRAY(state, state->data, uint8_t,
					   state->max_read);
	if (state->data == NULL) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		goto send;
	}

	subreq = np_read_send(req->conn, smbd_event_context(),
			      state->handle, state->data, state->max_read);
	if (subreq == NULL) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		goto send;
	}
	tevent_req_set_callback(subreq, api_dcerpc_cmd_read_done, req);
	return;

 send:
	if (!srv_send_smb(
		    req->sconn, (char *)req->outbuf,
		    true, req->seqnum+1,
		    IS_CONN_ENCRYPTED(req->conn) || req->encrypted,
		    &req->pcd)) {
		exit_server_cleanly("api_dcerpc_cmd_write_done: "
				    "srv_send_smb failed.");
	}
	TALLOC_FREE(req);
}

static void api_dcerpc_cmd_read_done(struct tevent_req *subreq)
{
	struct smb_request *req = tevent_req_callback_data(
		subreq, struct smb_request);
	struct dcerpc_cmd_state *state = talloc_get_type_abort(
		req->async_priv, struct dcerpc_cmd_state);
	NTSTATUS status;
	ssize_t nread;
	bool is_data_outstanding;

	status = np_read_recv(subreq, &nread, &is_data_outstanding);
	TALLOC_FREE(subreq);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("Could not read from to pipe: %s\n",
			   nt_errstr(status)));
		reply_nterror(req, status);

		if (!srv_send_smb(req->sconn, (char *)req->outbuf,
				  true, req->seqnum+1,
				  IS_CONN_ENCRYPTED(req->conn)
				  ||req->encrypted, &req->pcd)) {
			exit_server_cleanly("api_dcerpc_cmd_read_done: "
					    "srv_send_smb failed.");
		}
		TALLOC_FREE(req);
		return;
	}

	send_trans_reply(req->conn, req, NULL, 0, (char *)state->data, nread,
			 is_data_outstanding);
	TALLOC_FREE(req);
}

/****************************************************************************
 WaitNamedPipeHandleState 
****************************************************************************/

static void api_WNPHS(connection_struct *conn, struct smb_request *req,
		      struct files_struct *fsp, char *param, int param_len)
{
	if (!param || param_len < 2) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	DEBUG(4,("WaitNamedPipeHandleState priority %x\n",
		 (int)SVAL(param,0)));

	send_trans_reply(conn, req, NULL, 0, NULL, 0, False);
}


/****************************************************************************
 SetNamedPipeHandleState 
****************************************************************************/

static void api_SNPHS(connection_struct *conn, struct smb_request *req,
		      struct files_struct *fsp, char *param, int param_len)
{
	if (!param || param_len < 2) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	DEBUG(4,("SetNamedPipeHandleState to code %x\n", (int)SVAL(param,0)));

	send_trans_reply(conn, req, NULL, 0, NULL, 0, False);
}


/****************************************************************************
 When no reply is generated, indicate unsupported.
 ****************************************************************************/

static void api_no_reply(connection_struct *conn, struct smb_request *req)
{
	char rparam[4];

	/* unsupported */
	SSVAL(rparam,0,NERR_notsupported);
	SSVAL(rparam,2,0); /* converter word */

	DEBUG(3,("Unsupported API fd command\n"));

	/* now send the reply */
	send_trans_reply(conn, req, rparam, 4, NULL, 0, False);

	return;
}

/****************************************************************************
 Handle remote api calls delivered to a named pipe already opened.
 ****************************************************************************/

static void api_fd_reply(connection_struct *conn, uint16 vuid,
			 struct smb_request *req,
			 uint16 *setup, uint8_t *data, char *params,
			 int suwcnt, int tdscnt, int tpscnt,
			 int mdrcnt, int mprcnt)
{
	struct files_struct *fsp;
	int pnum;
	int subcommand;

	DEBUG(5,("api_fd_reply\n"));

	/* First find out the name of this file. */
	if (suwcnt != 2) {
		DEBUG(0,("Unexpected named pipe transaction.\n"));
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	/* Get the file handle and hence the file name. */
	/* 
	 * NB. The setup array has already been transformed
	 * via SVAL and so is in host byte order.
	 */
	pnum = ((int)setup[1]) & 0xFFFF;
	subcommand = ((int)setup[0]) & 0xFFFF;

	fsp = file_fsp(req, pnum);

	if (!fsp_is_np(fsp)) {
		if (subcommand == TRANSACT_WAITNAMEDPIPEHANDLESTATE) {
			/* Win9x does this call with a unicode pipe name, not a pnum. */
			/* Just return success for now... */
			DEBUG(3,("Got TRANSACT_WAITNAMEDPIPEHANDLESTATE on text pipe name\n"));
			send_trans_reply(conn, req, NULL, 0, NULL, 0, False);
			return;
		}

		DEBUG(1,("api_fd_reply: INVALID PIPE HANDLE: %x\n", pnum));
		reply_nterror(req, NT_STATUS_INVALID_HANDLE);
		return;
	}

	if (vuid != fsp->vuid) {
		DEBUG(1, ("Got pipe request (pnum %x) using invalid VUID %d, "
			  "expected %d\n", pnum, vuid, fsp->vuid));
		reply_nterror(req, NT_STATUS_INVALID_HANDLE);
		return;
	}

	DEBUG(3,("Got API command 0x%x on pipe \"%s\" (pnum %x)\n",
		 subcommand, fsp_str_dbg(fsp), pnum));

	DEBUG(10, ("api_fd_reply: p:%p max_trans_reply: %d\n", fsp, mdrcnt));

	switch (subcommand) {
	case TRANSACT_DCERPCCMD: {
		/* dce/rpc command */
		api_dcerpc_cmd(conn, req, fsp, (uint8_t *)data, tdscnt,
			       mdrcnt);
		break;
	}
	case TRANSACT_WAITNAMEDPIPEHANDLESTATE:
		/* Wait Named Pipe Handle state */
		api_WNPHS(conn, req, fsp, params, tpscnt);
		break;
	case TRANSACT_SETNAMEDPIPEHANDLESTATE:
		/* Set Named Pipe Handle state */
		api_SNPHS(conn, req, fsp, params, tpscnt);
		break;
	default:
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}
}

/****************************************************************************
 Handle named pipe commands.
****************************************************************************/

static void named_pipe(connection_struct *conn, uint16 vuid,
		       struct smb_request *req,
		       const char *name, uint16 *setup,
		       char *data, char *params,
		       int suwcnt, int tdscnt,int tpscnt,
		       int msrcnt, int mdrcnt, int mprcnt)
{
	DEBUG(3,("named pipe command on <%s> name\n", name));

	if (strequal(name,"LANMAN")) {
		api_reply(conn, vuid, req,
			  data, params,
			  tdscnt, tpscnt,
			  mdrcnt, mprcnt);
		return;
	}

	if (strequal(name,"WKSSVC") ||
	    strequal(name,"SRVSVC") ||
	    strequal(name,"WINREG") ||
	    strequal(name,"SAMR") ||
	    strequal(name,"LSARPC")) {

		DEBUG(4,("named pipe command from Win95 (wow!)\n"));

		api_fd_reply(conn, vuid, req,
			     setup, (uint8_t *)data, params,
			     suwcnt, tdscnt, tpscnt,
			     mdrcnt, mprcnt);
		return;
	}

	if (strlen(name) < 1) {
		api_fd_reply(conn, vuid, req,
			     setup, (uint8_t *)data,
			     params, suwcnt, tdscnt,
			     tpscnt, mdrcnt, mprcnt);
		return;
	}

	if (setup)
		DEBUG(3,("unknown named pipe: setup 0x%X setup1=%d\n",
			 (int)setup[0],(int)setup[1]));

	reply_nterror(req, NT_STATUS_NOT_SUPPORTED);
	return;
}

static void handle_trans(connection_struct *conn, struct smb_request *req,
			 struct trans_state *state)
{
	char *local_machine_name;
	int name_offset = 0;

	DEBUG(3,("trans <%s> data=%u params=%u setup=%u\n",
		 state->name,(unsigned int)state->total_data,(unsigned int)state->total_param,
		 (unsigned int)state->setup_count));

	/*
	 * WinCE wierdness....
	 */

	local_machine_name = talloc_asprintf(state, "\\%s\\",
					     get_local_machine_name());

	if (local_machine_name == NULL) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}

	if (strnequal(state->name, local_machine_name,
		      strlen(local_machine_name))) {
		name_offset = strlen(local_machine_name)-1;
	}

	if (!strnequal(&state->name[name_offset], "\\PIPE",
		       strlen("\\PIPE"))) {
		reply_nterror(req, NT_STATUS_NOT_SUPPORTED);
		return;
	}

	name_offset += strlen("\\PIPE");

	/* Win9x weirdness.  When talking to a unicode server Win9x
	   only sends \PIPE instead of \PIPE\ */

	if (state->name[name_offset] == '\\')
		name_offset++;

	DEBUG(5,("calling named_pipe\n"));
	named_pipe(conn, state->vuid, req,
		   state->name+name_offset,
		   state->setup,state->data,
		   state->param,
		   state->setup_count,state->total_data,
		   state->total_param,
		   state->max_setup_return,
		   state->max_data_return,
		   state->max_param_return);

	if (state->close_on_completion) {
		close_cnum(conn,state->vuid);
		req->conn = NULL;
	}

	return;
}

/****************************************************************************
 Reply to a SMBtrans.
 ****************************************************************************/

void reply_trans(struct smb_request *req)
{
	connection_struct *conn = req->conn;
	unsigned int dsoff;
	unsigned int dscnt;
	unsigned int psoff;
	unsigned int pscnt;
	struct trans_state *state;
	NTSTATUS result;

	START_PROFILE(SMBtrans);

	if (req->wct < 14) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		END_PROFILE(SMBtrans);
		return;
	}

	dsoff = SVAL(req->vwv+12, 0);
	dscnt = SVAL(req->vwv+11, 0);
	psoff = SVAL(req->vwv+10, 0);
	pscnt = SVAL(req->vwv+9, 0);

	result = allow_new_trans(conn->pending_trans, req->mid);
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(2, ("Got invalid trans request: %s\n",
			  nt_errstr(result)));
		reply_nterror(req, result);
		END_PROFILE(SMBtrans);
		return;
	}

	if ((state = TALLOC_P(conn, struct trans_state)) == NULL) {
		DEBUG(0, ("talloc failed\n"));
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		END_PROFILE(SMBtrans);
		return;
	}

	state->cmd = SMBtrans;

	state->mid = req->mid;
	state->vuid = req->vuid;
	state->setup_count = CVAL(req->vwv+13, 0);
	state->setup = NULL;
	state->total_param = SVAL(req->vwv+0, 0);
	state->param = NULL;
	state->total_data = SVAL(req->vwv+1, 0);
	state->data = NULL;
	state->max_param_return = SVAL(req->vwv+2, 0);
	state->max_data_return = SVAL(req->vwv+3, 0);
	state->max_setup_return = CVAL(req->vwv+4, 0);
	state->close_on_completion = BITSETW(req->vwv+5, 0);
	state->one_way = BITSETW(req->vwv+5, 1);

	srvstr_pull_req_talloc(state, req, &state->name, req->buf,
			       STR_TERMINATE);

	if ((dscnt > state->total_data) || (pscnt > state->total_param) ||
			!state->name)
		goto bad_param;

	if (state->total_data)  {

		if (trans_oob(state->total_data, 0, dscnt)
		    || trans_oob(smb_len(req->inbuf), dsoff, dscnt)) {
			goto bad_param;
		}

		/* Can't use talloc here, the core routines do realloc on the
		 * params and data. Out of paranoia, 100 bytes too many. */
		state->data = (char *)SMB_MALLOC(state->total_data+100);
		if (state->data == NULL) {
			DEBUG(0,("reply_trans: data malloc fail for %u "
				 "bytes !\n", (unsigned int)state->total_data));
			TALLOC_FREE(state);
			reply_nterror(req, NT_STATUS_NO_MEMORY);
			END_PROFILE(SMBtrans);
			return;
		}
		/* null-terminate the slack space */
		memset(&state->data[state->total_data], 0, 100);

		memcpy(state->data,smb_base(req->inbuf)+dsoff,dscnt);
	}

	if (state->total_param) {

		if (trans_oob(state->total_param, 0, pscnt)
		    || trans_oob(smb_len(req->inbuf), psoff, pscnt)) {
			goto bad_param;
		}

		/* Can't use talloc here, the core routines do realloc on the
		 * params and data. Out of paranoia, 100 bytes too many */
		state->param = (char *)SMB_MALLOC(state->total_param+100);
		if (state->param == NULL) {
			DEBUG(0,("reply_trans: param malloc fail for %u "
				 "bytes !\n", (unsigned int)state->total_param));
			SAFE_FREE(state->data);
			TALLOC_FREE(state);
			reply_nterror(req, NT_STATUS_NO_MEMORY);
			END_PROFILE(SMBtrans);
			return;
		} 
		/* null-terminate the slack space */
		memset(&state->param[state->total_param], 0, 100);

		memcpy(state->param,smb_base(req->inbuf)+psoff,pscnt);
	}

	state->received_data  = dscnt;
	state->received_param = pscnt;

	if (state->setup_count) {
		unsigned int i;

		/*
		 * No overflow possible here, state->setup_count is an
		 * unsigned int, being filled by a single byte from
		 * CVAL(req->vwv+13, 0) above. The cast in the comparison
		 * below is not necessary, it's here to clarify things. The
		 * validity of req->vwv and req->wct has been checked in
		 * init_smb_request already.
		 */
		if (state->setup_count + 14 > (unsigned int)req->wct) {
			goto bad_param;
		}

		if((state->setup = TALLOC_ARRAY(
			    state, uint16, state->setup_count)) == NULL) {
			DEBUG(0,("reply_trans: setup malloc fail for %u "
				 "bytes !\n", (unsigned int)
				 (state->setup_count * sizeof(uint16))));
			SAFE_FREE(state->data);
			SAFE_FREE(state->param);
			TALLOC_FREE(state);
			reply_nterror(req, NT_STATUS_NO_MEMORY);
			END_PROFILE(SMBtrans);
			return;
		} 

		for (i=0;i<state->setup_count;i++) {
			state->setup[i] = SVAL(req->vwv + 14 + i, 0);
		}
	}

	state->received_param = pscnt;

	if ((state->received_param != state->total_param) ||
	    (state->received_data != state->total_data)) {
		DLIST_ADD(conn->pending_trans, state);

		/* We need to send an interim response then receive the rest
		   of the parameter/data bytes */
		reply_outbuf(req, 0, 0);
		show_msg((char *)req->outbuf);
		END_PROFILE(SMBtrans);
		return;
	}

	talloc_steal(talloc_tos(), state);

	handle_trans(conn, req, state);

	SAFE_FREE(state->data);
	SAFE_FREE(state->param);
	TALLOC_FREE(state);

	END_PROFILE(SMBtrans);
	return;

  bad_param:

	DEBUG(0,("reply_trans: invalid trans parameters\n"));
	SAFE_FREE(state->data);
	SAFE_FREE(state->param);
	TALLOC_FREE(state);
	END_PROFILE(SMBtrans);
	reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
	return;
}

/****************************************************************************
 Reply to a secondary SMBtrans.
 ****************************************************************************/

void reply_transs(struct smb_request *req)
{
	connection_struct *conn = req->conn;
	unsigned int pcnt,poff,dcnt,doff,pdisp,ddisp;
	struct trans_state *state;

	START_PROFILE(SMBtranss);

	show_msg((char *)req->inbuf);

	if (req->wct < 8) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		END_PROFILE(SMBtranss);
		return;
	}

	for (state = conn->pending_trans; state != NULL;
	     state = state->next) {
		if (state->mid == req->mid) {
			break;
		}
	}

	if ((state == NULL) || (state->cmd != SMBtrans)) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		END_PROFILE(SMBtranss);
		return;
	}

	/* Revise total_params and total_data in case they have changed
	 * downwards */

	if (SVAL(req->vwv+0, 0) < state->total_param)
		state->total_param = SVAL(req->vwv+0, 0);
	if (SVAL(req->vwv+1, 0) < state->total_data)
		state->total_data = SVAL(req->vwv+1, 0);

	pcnt = SVAL(req->vwv+2, 0);
	poff = SVAL(req->vwv+3, 0);
	pdisp = SVAL(req->vwv+4, 0);

	dcnt = SVAL(req->vwv+5, 0);
	doff = SVAL(req->vwv+6, 0);
	ddisp = SVAL(req->vwv+7, 0);

	state->received_param += pcnt;
	state->received_data += dcnt;

	if ((state->received_data > state->total_data) ||
	    (state->received_param > state->total_param))
		goto bad_param;

	if (pcnt) {
		if (trans_oob(state->total_param, pdisp, pcnt)
		    || trans_oob(smb_len(req->inbuf), poff, pcnt)) {
			goto bad_param;
		}
		memcpy(state->param+pdisp,smb_base(req->inbuf)+poff,pcnt);
	}

	if (dcnt) {
		if (trans_oob(state->total_data, ddisp, dcnt)
		    || trans_oob(smb_len(req->inbuf), doff, dcnt)) {
			goto bad_param;
		}
		memcpy(state->data+ddisp, smb_base(req->inbuf)+doff,dcnt);
	}

	if ((state->received_param < state->total_param) ||
	    (state->received_data < state->total_data)) {
		END_PROFILE(SMBtranss);
		return;
	}

	talloc_steal(talloc_tos(), state);

	handle_trans(conn, req, state);

	DLIST_REMOVE(conn->pending_trans, state);
	SAFE_FREE(state->data);
	SAFE_FREE(state->param);
	TALLOC_FREE(state);

	END_PROFILE(SMBtranss);
	return;

  bad_param:

	DEBUG(0,("reply_transs: invalid trans parameters\n"));
	DLIST_REMOVE(conn->pending_trans, state);
	SAFE_FREE(state->data);
	SAFE_FREE(state->param);
	TALLOC_FREE(state);
	reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
	END_PROFILE(SMBtranss);
	return;
}
