/* 
   Unix SMB/CIFS implementation.
   NT transaction handling
   Copyright (C) Andrew Tridgell 2003
   Copyright (C) James J Myers 2003 <myersjj@samba.org>

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
#include "smb_server/smb_server.h"
#include "ntvfs/ntvfs.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "librpc/gen_ndr/ndr_security.h"

/*
  hold the state of a nttrans op while in progress. Needed to allow for async backend
  functions.
*/
struct nttrans_op {
	struct smb_nttrans *trans;
	NTSTATUS (*send_fn)(struct nttrans_op *);
	void *op_info;
};


/* setup a nttrans reply, given the data and params sizes */
static NTSTATUS nttrans_setup_reply(struct nttrans_op *op, 
				    struct smb_nttrans *trans,
				    uint32_t param_size, uint32_t data_size,
				    uint8_t setup_count)
{
	trans->out.setup_count = setup_count;
	if (setup_count != 0) {
		trans->out.setup = talloc_zero_array(op, uint8_t, setup_count*2);
		NT_STATUS_HAVE_NO_MEMORY(trans->out.setup);
	}
	trans->out.params = data_blob_talloc(op, NULL, param_size);
	if (param_size != 0) {
		NT_STATUS_HAVE_NO_MEMORY(trans->out.params.data);
	}
	trans->out.data = data_blob_talloc(op, NULL, data_size);
	if (data_size != 0) {
		NT_STATUS_HAVE_NO_MEMORY(trans->out.data.data);
	}
	return NT_STATUS_OK;
}

/*
  send a nttrans create reply
*/
static NTSTATUS nttrans_create_send(struct nttrans_op *op)
{
	union smb_open *io = talloc_get_type(op->op_info, union smb_open);
	uint8_t *params;
	NTSTATUS status;

	status = nttrans_setup_reply(op, op->trans, 69, 0, 0);
	NT_STATUS_NOT_OK_RETURN(status);
	params = op->trans->out.params.data;

	SSVAL(params,        0, io->ntcreatex.out.oplock_level);
	smbsrv_push_fnum(params, 2, io->ntcreatex.out.file.ntvfs);
	SIVAL(params,        4, io->ntcreatex.out.create_action);
	SIVAL(params,        8, 0); /* ea error offset */
	push_nttime(params, 12, io->ntcreatex.out.create_time);
	push_nttime(params, 20, io->ntcreatex.out.access_time);
	push_nttime(params, 28, io->ntcreatex.out.write_time);
	push_nttime(params, 36, io->ntcreatex.out.change_time);
	SIVAL(params,       44, io->ntcreatex.out.attrib);
	SBVAL(params,       48, io->ntcreatex.out.alloc_size);
	SBVAL(params,       56, io->ntcreatex.out.size);
	SSVAL(params,       64, io->ntcreatex.out.file_type);
	SSVAL(params,       66, io->ntcreatex.out.ipc_state);
	SCVAL(params,       68, io->ntcreatex.out.is_directory);

	return NT_STATUS_OK;
}

/* 
   parse NTTRANS_CREATE request
 */
static NTSTATUS nttrans_create(struct smbsrv_request *req, 
			       struct nttrans_op *op)
{
	struct smb_nttrans *trans = op->trans;
	union smb_open *io;
	uint16_t fname_len;
	uint32_t sd_length, ea_length;
	NTSTATUS status;
	uint8_t *params;
	enum ndr_err_code ndr_err;

	if (trans->in.params.length < 54) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* parse the request */
	io = talloc(op, union smb_open);
	NT_STATUS_HAVE_NO_MEMORY(io);

	io->ntcreatex.level = RAW_OPEN_NTTRANS_CREATE;

	params = trans->in.params.data;

	io->ntcreatex.in.flags            = IVAL(params,  0);
	io->ntcreatex.in.root_fid.ntvfs   = smbsrv_pull_fnum(req, params, 4);
	io->ntcreatex.in.access_mask      = IVAL(params,  8);
	io->ntcreatex.in.alloc_size       = BVAL(params, 12);
	io->ntcreatex.in.file_attr        = IVAL(params, 20);
	io->ntcreatex.in.share_access     = IVAL(params, 24);
	io->ntcreatex.in.open_disposition = IVAL(params, 28);
	io->ntcreatex.in.create_options   = IVAL(params, 32);
	sd_length                         = IVAL(params, 36);
	ea_length                         = IVAL(params, 40);
	fname_len                         = IVAL(params, 44);
	io->ntcreatex.in.impersonation    = IVAL(params, 48);
	io->ntcreatex.in.security_flags   = CVAL(params, 52);
	io->ntcreatex.in.sec_desc         = NULL;
	io->ntcreatex.in.ea_list          = NULL;
	io->ntcreatex.in.query_maximal_access = false;
	io->ntcreatex.in.private_flags    = 0;

	req_pull_string(&req->in.bufinfo, &io->ntcreatex.in.fname, 
			params + 53, 
			MIN(fname_len+1, trans->in.params.length - 53),
			STR_NO_RANGE_CHECK | STR_TERMINATE);
	if (!io->ntcreatex.in.fname) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (sd_length > trans->in.data.length ||
	    ea_length > trans->in.data.length ||
	    (sd_length+ea_length) > trans->in.data.length) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* this call has an optional security descriptor */
	if (sd_length != 0) {
		DATA_BLOB blob;
		blob.data = trans->in.data.data;
		blob.length = sd_length;
		io->ntcreatex.in.sec_desc = talloc(io, struct security_descriptor);
		if (io->ntcreatex.in.sec_desc == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		ndr_err = ndr_pull_struct_blob(&blob, io, 
					       io->ntcreatex.in.sec_desc,
					       (ndr_pull_flags_fn_t)ndr_pull_security_descriptor);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return ndr_map_error2ntstatus(ndr_err);
		}
	}

	/* and an optional ea_list */
	if (ea_length > 4) {
		DATA_BLOB blob;
		blob.data = trans->in.data.data + sd_length;
		blob.length = ea_length;
		io->ntcreatex.in.ea_list = talloc(io, struct smb_ea_list);
		if (io->ntcreatex.in.ea_list == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		status = ea_pull_list_chained(&blob, io, 
					      &io->ntcreatex.in.ea_list->num_eas,
					      &io->ntcreatex.in.ea_list->eas);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	op->send_fn = nttrans_create_send;
	op->op_info = io;

	return ntvfs_open(req->ntvfs, io);
}


/* 
   send NTTRANS_QUERY_SEC_DESC reply
 */
static NTSTATUS nttrans_query_sec_desc_send(struct nttrans_op *op)
{
	union smb_fileinfo *io = talloc_get_type(op->op_info, union smb_fileinfo);
	uint8_t *params;
	NTSTATUS status;
	enum ndr_err_code ndr_err;

	status = nttrans_setup_reply(op, op->trans, 4, 0, 0);
	NT_STATUS_NOT_OK_RETURN(status);
	params = op->trans->out.params.data;

	ndr_err = ndr_push_struct_blob(&op->trans->out.data, op, 
				       io->query_secdesc.out.sd,
				       (ndr_push_flags_fn_t)ndr_push_security_descriptor);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	SIVAL(params, 0, op->trans->out.data.length);

	return NT_STATUS_OK;
}

/* 
   parse NTTRANS_QUERY_SEC_DESC request
 */
static NTSTATUS nttrans_query_sec_desc(struct smbsrv_request *req, 
				       struct nttrans_op *op)
{
	struct smb_nttrans *trans = op->trans;
	union smb_fileinfo *io;

	if (trans->in.params.length < 8) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* parse the request */
	io = talloc(op, union smb_fileinfo);
	NT_STATUS_HAVE_NO_MEMORY(io);

	io->query_secdesc.level            = RAW_FILEINFO_SEC_DESC;
	io->query_secdesc.in.file.ntvfs    = smbsrv_pull_fnum(req, trans->in.params.data, 0);
	io->query_secdesc.in.secinfo_flags = IVAL(trans->in.params.data, 4);

	op->op_info = io;
	op->send_fn = nttrans_query_sec_desc_send;

	SMBSRV_CHECK_FILE_HANDLE_NTSTATUS(io->query_secdesc.in.file.ntvfs);
	return ntvfs_qfileinfo(req->ntvfs, io);
}


/* 
   parse NTTRANS_SET_SEC_DESC request
 */
static NTSTATUS nttrans_set_sec_desc(struct smbsrv_request *req, 
				     struct nttrans_op *op)
{
	struct smb_nttrans *trans = op->trans;
	union smb_setfileinfo *io;
	enum ndr_err_code ndr_err;

	if (trans->in.params.length < 8) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* parse the request */
	io = talloc(req, union smb_setfileinfo);
	NT_STATUS_HAVE_NO_MEMORY(io);

	io->set_secdesc.level            = RAW_SFILEINFO_SEC_DESC;
	io->set_secdesc.in.file.ntvfs    = smbsrv_pull_fnum(req, trans->in.params.data, 0);
	io->set_secdesc.in.secinfo_flags = IVAL(trans->in.params.data, 4);

	io->set_secdesc.in.sd = talloc(io, struct security_descriptor);
	NT_STATUS_HAVE_NO_MEMORY(io->set_secdesc.in.sd);

	ndr_err = ndr_pull_struct_blob(&trans->in.data, req, 
				       io->set_secdesc.in.sd,
				       (ndr_pull_flags_fn_t)ndr_pull_security_descriptor);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	SMBSRV_CHECK_FILE_HANDLE_NTSTATUS(io->set_secdesc.in.file.ntvfs);
	return ntvfs_setfileinfo(req->ntvfs, io);
}


/* parse NTTRANS_RENAME request
 */
static NTSTATUS nttrans_rename(struct smbsrv_request *req, 
			       struct nttrans_op *op)
{
	struct smb_nttrans *trans = op->trans;
	union smb_rename *io;

	if (trans->in.params.length < 5) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* parse the request */
	io = talloc(req, union smb_rename);
	NT_STATUS_HAVE_NO_MEMORY(io);

	io->nttrans.level		= RAW_RENAME_NTTRANS;
	io->nttrans.in.file.ntvfs	= smbsrv_pull_fnum(req, trans->in.params.data, 0);
	io->nttrans.in.flags		= SVAL(trans->in.params.data, 2);

	smbsrv_blob_pull_string(&req->in.bufinfo, &trans->in.params, 4,
				&io->nttrans.in.new_name,
				STR_TERMINATE);
	if (!io->nttrans.in.new_name) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	SMBSRV_CHECK_FILE_HANDLE_NTSTATUS(io->nttrans.in.file.ntvfs);
	return ntvfs_rename(req->ntvfs, io);
}

/* 
   parse NTTRANS_IOCTL send
 */
static NTSTATUS nttrans_ioctl_send(struct nttrans_op *op)
{
	union smb_ioctl *info = talloc_get_type(op->op_info, union smb_ioctl);
	NTSTATUS status;

	/* 
	 * we pass 0 as data_count here,
	 * because we reuse the DATA_BLOB from the smb_ioctl
	 * struct
	 */
	status = nttrans_setup_reply(op, op->trans, 0, 0, 1);
	NT_STATUS_NOT_OK_RETURN(status);

	op->trans->out.setup[0]		= 0;
	op->trans->out.data		= info->ntioctl.out.blob;

	return NT_STATUS_OK;
}


/* 
   parse NTTRANS_IOCTL request
 */
static NTSTATUS nttrans_ioctl(struct smbsrv_request *req, 
			      struct nttrans_op *op)
{
	struct smb_nttrans *trans = op->trans;
	union smb_ioctl *nt;

	/* should have at least 4 setup words */
	if (trans->in.setup_count != 4) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	nt = talloc(op, union smb_ioctl);
	NT_STATUS_HAVE_NO_MEMORY(nt);
	
	nt->ntioctl.level		= RAW_IOCTL_NTIOCTL;
	nt->ntioctl.in.function		= IVAL(trans->in.setup, 0);
	nt->ntioctl.in.file.ntvfs	= smbsrv_pull_fnum(req, (uint8_t *)trans->in.setup, 4);
	nt->ntioctl.in.fsctl		= CVAL(trans->in.setup, 6);
	nt->ntioctl.in.filter		= CVAL(trans->in.setup, 7);
	nt->ntioctl.in.max_data		= trans->in.max_data;
	nt->ntioctl.in.blob		= trans->in.data;

	op->op_info = nt;
	op->send_fn = nttrans_ioctl_send;

	SMBSRV_CHECK_FILE_HANDLE_NTSTATUS(nt->ntioctl.in.file.ntvfs);
	return ntvfs_ioctl(req->ntvfs, nt);
}


/* 
   send NTTRANS_NOTIFY_CHANGE reply
 */
static NTSTATUS nttrans_notify_change_send(struct nttrans_op *op)
{
	union smb_notify *info = talloc_get_type(op->op_info, union smb_notify);
	size_t size = 0;
	int i;
	NTSTATUS status;
	uint8_t *p;
#define MAX_BYTES_PER_CHAR 3
	
	/* work out how big the reply buffer could be */
	for (i=0;i<info->nttrans.out.num_changes;i++) {
		size += 12 + 3 + (1+strlen(info->nttrans.out.changes[i].name.s)) * MAX_BYTES_PER_CHAR;
	}

	status = nttrans_setup_reply(op, op->trans, size, 0, 0);
	NT_STATUS_NOT_OK_RETURN(status);
	p = op->trans->out.params.data;

	/* construct the changes buffer */
	for (i=0;i<info->nttrans.out.num_changes;i++) {
		uint32_t ofs;
		ssize_t len;

		SIVAL(p, 4, info->nttrans.out.changes[i].action);
		len = push_string(p + 12, info->nttrans.out.changes[i].name.s, 
				  op->trans->out.params.length - 
				  (p+12 - op->trans->out.params.data), STR_UNICODE);
		SIVAL(p, 8, len);

		ofs = len + 12;

		if (ofs & 3) {
			int pad = 4 - (ofs & 3);
			memset(p+ofs, 0, pad);
			ofs += pad;
		}

		if (i == info->nttrans.out.num_changes-1) {
			SIVAL(p, 0, 0);
		} else {
			SIVAL(p, 0, ofs);
		}

		p += ofs;
	}

	op->trans->out.params.length = p - op->trans->out.params.data;

	return NT_STATUS_OK;
}

/* 
   parse NTTRANS_NOTIFY_CHANGE request
 */
static NTSTATUS nttrans_notify_change(struct smbsrv_request *req, 
				      struct nttrans_op *op)
{
	struct smb_nttrans *trans = op->trans;
	union smb_notify *info;

	/* should have at least 4 setup words */
	if (trans->in.setup_count != 4) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	info = talloc(op, union smb_notify);
	NT_STATUS_HAVE_NO_MEMORY(info);

	info->nttrans.level			= RAW_NOTIFY_NTTRANS;
	info->nttrans.in.completion_filter	= IVAL(trans->in.setup, 0);
	info->nttrans.in.file.ntvfs		= smbsrv_pull_fnum(req, (uint8_t *)trans->in.setup, 4);
	info->nttrans.in.recursive		= SVAL(trans->in.setup, 6);
	info->nttrans.in.buffer_size		= trans->in.max_param;

	op->op_info = info;
	op->send_fn = nttrans_notify_change_send;

	SMBSRV_CHECK_FILE_HANDLE_NTSTATUS(info->nttrans.in.file.ntvfs);
	return ntvfs_notify(req->ntvfs, info);
}

/*
  backend for nttrans requests
*/
static NTSTATUS nttrans_backend(struct smbsrv_request *req, 
				struct nttrans_op *op)
{
	/* the nttrans command is in function */
	switch (op->trans->in.function) {
	case NT_TRANSACT_CREATE:
		return nttrans_create(req, op);
	case NT_TRANSACT_IOCTL:
		return nttrans_ioctl(req, op);
	case NT_TRANSACT_RENAME:
		return nttrans_rename(req, op);
	case NT_TRANSACT_QUERY_SECURITY_DESC:
		return nttrans_query_sec_desc(req, op);
	case NT_TRANSACT_SET_SECURITY_DESC:
		return nttrans_set_sec_desc(req, op);
	case NT_TRANSACT_NOTIFY_CHANGE:
		return nttrans_notify_change(req, op);
	}

	/* an unknown nttrans command */
	return NT_STATUS_DOS(ERRSRV, ERRerror);
}


static void reply_nttrans_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	uint32_t params_left, data_left;
	uint8_t *params, *data;
	struct smb_nttrans *trans;
	struct nttrans_op *op;

	SMBSRV_CHECK_ASYNC_STATUS(op, struct nttrans_op);

	trans = op->trans;

	/* if this function needs work to form the nttrans reply buffer, then
	   call that now */
	if (op->send_fn != NULL) {
		NTSTATUS status;
		status = op->send_fn(op);
		if (!NT_STATUS_IS_OK(status)) {
			smbsrv_send_error(req, status);
			return;
		}
	}

	smbsrv_setup_reply(req, 18 + trans->out.setup_count, 0);

	/* note that we don't check the max_setup count (matching w2k3
	   behaviour) */

	if (trans->out.params.length > trans->in.max_param) {
		smbsrv_setup_error(req, NT_STATUS_BUFFER_TOO_SMALL);
		trans->out.params.length = trans->in.max_param;
	}
	if (trans->out.data.length > trans->in.max_data) {
		smbsrv_setup_error(req, NT_STATUS_BUFFER_TOO_SMALL);
		trans->out.data.length = trans->in.max_data;
	}

	params_left = trans->out.params.length;
	data_left   = trans->out.data.length;
	params      = trans->out.params.data;
	data        = trans->out.data.data;

	/* we need to divide up the reply into chunks that fit into
	   the negotiated buffer size */
	do {
		uint32_t this_data, this_param, max_bytes;
		unsigned int align1 = 1, align2 = (params_left ? 2 : 0);
		struct smbsrv_request *this_req;

		max_bytes = req_max_data(req) - (align1 + align2);

		this_param = params_left;
		if (this_param > max_bytes) {
			this_param = max_bytes;
		}
		max_bytes -= this_param;

		this_data = data_left;
		if (this_data > max_bytes) {
			this_data = max_bytes;
		}

		/* don't destroy unless this is the last chunk */
		if (params_left - this_param != 0 || 
		    data_left - this_data != 0) {
			this_req = smbsrv_setup_secondary_request(req);
		} else {
			this_req = req;
		}

		req_grow_data(this_req, this_param + this_data + (align1 + align2));

		SSVAL(this_req->out.vwv, 0, 0); /* reserved */
		SCVAL(this_req->out.vwv, 2, 0); /* reserved */
		SIVAL(this_req->out.vwv, 3, trans->out.params.length);
		SIVAL(this_req->out.vwv, 7, trans->out.data.length);

		SIVAL(this_req->out.vwv, 11, this_param);
		SIVAL(this_req->out.vwv, 15, align1 + PTR_DIFF(this_req->out.data, this_req->out.hdr));
		SIVAL(this_req->out.vwv, 19, PTR_DIFF(params, trans->out.params.data));

		SIVAL(this_req->out.vwv, 23, this_data);
		SIVAL(this_req->out.vwv, 27, align1 + align2 + 
		      PTR_DIFF(this_req->out.data + this_param, this_req->out.hdr));
		SIVAL(this_req->out.vwv, 31, PTR_DIFF(data, trans->out.data.data));

		SCVAL(this_req->out.vwv, 35, trans->out.setup_count);
		memcpy((char *)(this_req->out.vwv) + VWV(18), trans->out.setup,
		       sizeof(uint16_t) * trans->out.setup_count);
		memset(this_req->out.data, 0, align1);
		if (this_param != 0) {
			memcpy(this_req->out.data + align1, params, this_param);
		}
		memset(this_req->out.data+this_param+align1, 0, align2);
		if (this_data != 0) {
			memcpy(this_req->out.data+this_param+align1+align2, 
			       data, this_data);
		}

		params_left -= this_param;
		data_left -= this_data;
		params += this_param;
		data += this_data;

		smbsrv_send_reply(this_req);
	} while (params_left != 0 || data_left != 0);
}

/*
  send a continue request
*/
static void reply_nttrans_continue(struct smbsrv_request *req, struct smb_nttrans *trans)
{
	struct smbsrv_request *req2;
	struct smbsrv_trans_partial *tp;
	int count;

	/* make sure they don't flood us */
	for (count=0,tp=req->smb_conn->trans_partial;tp;tp=tp->next) count++;
	if (count > 100) {
		smbsrv_send_error(req, NT_STATUS_INSUFFICIENT_RESOURCES);
		return;
	}

	tp = talloc(req, struct smbsrv_trans_partial);

	tp->req = req;
	tp->u.nttrans = trans;
	tp->command = SMBnttrans;

	DLIST_ADD(req->smb_conn->trans_partial, tp);
	talloc_set_destructor(tp, smbsrv_trans_partial_destructor);

	req2 = smbsrv_setup_secondary_request(req);

	/* send a 'please continue' reply */
	smbsrv_setup_reply(req2, 0, 0);
	smbsrv_send_reply(req2);
}


/*
  answer a reconstructed trans request
*/
static void reply_nttrans_complete(struct smbsrv_request *req, struct smb_nttrans *trans)
{
	struct nttrans_op *op;

	SMBSRV_TALLOC_IO_PTR(op, struct nttrans_op);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_nttrans_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	op->trans	= trans;
	op->op_info	= NULL;
	op->send_fn	= NULL;

	/* its a full request, give it to the backend */
	ZERO_STRUCT(trans->out);
	SMBSRV_CALL_NTVFS_BACKEND(nttrans_backend(req, op));
}


/****************************************************************************
 Reply to an SMBnttrans request
****************************************************************************/
void smbsrv_reply_nttrans(struct smbsrv_request *req)
{
	struct smb_nttrans *trans;
	uint32_t param_ofs, data_ofs;
	uint32_t param_count, data_count;
	uint32_t param_total, data_total;

	/* parse request */
	if (req->in.wct < 19) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	trans = talloc(req, struct smb_nttrans);
	if (trans == NULL) {
		smbsrv_send_error(req, NT_STATUS_NO_MEMORY);
		return;
	}

	trans->in.max_setup  = CVAL(req->in.vwv, 0);
	param_total          = IVAL(req->in.vwv, 3);
	data_total           = IVAL(req->in.vwv, 7);
	trans->in.max_param  = IVAL(req->in.vwv, 11);
	trans->in.max_data   = IVAL(req->in.vwv, 15);
	param_count          = IVAL(req->in.vwv, 19);
	param_ofs            = IVAL(req->in.vwv, 23);
	data_count           = IVAL(req->in.vwv, 27);
	data_ofs             = IVAL(req->in.vwv, 31);
	trans->in.setup_count= CVAL(req->in.vwv, 35);
	trans->in.function   = SVAL(req->in.vwv, 36);

	if (req->in.wct != 19 + trans->in.setup_count) {
		smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRerror));
		return;
	}

	/* parse out the setup words */
	trans->in.setup = talloc_array(req, uint8_t, trans->in.setup_count*2);
	if (!trans->in.setup) {
		smbsrv_send_error(req, NT_STATUS_NO_MEMORY);
		return;
	}
	memcpy(trans->in.setup, (char *)(req->in.vwv) + VWV(19),
	       sizeof(uint16_t) * trans->in.setup_count);

	if (!req_pull_blob(&req->in.bufinfo, req->in.hdr + param_ofs, param_count, &trans->in.params) ||
	    !req_pull_blob(&req->in.bufinfo, req->in.hdr + data_ofs, data_count, &trans->in.data)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	/* is it a partial request? if so, then send a 'send more' message */
	if (param_total > param_count || data_total > data_count) {
		reply_nttrans_continue(req, trans);
		return;
	}

	reply_nttrans_complete(req, trans);
}


/****************************************************************************
 Reply to an SMBnttranss request
****************************************************************************/
void smbsrv_reply_nttranss(struct smbsrv_request *req)
{
	struct smbsrv_trans_partial *tp;
	struct smb_nttrans *trans = NULL;
	uint32_t param_ofs, data_ofs;
	uint32_t param_count, data_count;
	uint32_t param_disp, data_disp;
	uint32_t param_total, data_total;
	DATA_BLOB params, data;

	/* parse request */
	if (req->in.wct != 18) {
		smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRerror));
		return;
	}

	for (tp=req->smb_conn->trans_partial;tp;tp=tp->next) {
		if (tp->command == SMBnttrans &&
		    SVAL(tp->req->in.hdr, HDR_MID) == SVAL(req->in.hdr, HDR_MID)) {
/* TODO: check the VUID, PID and TID too? */
			break;
		}
	}

	if (tp == NULL) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	trans = tp->u.nttrans;

	param_total           = IVAL(req->in.vwv, 3);
	data_total            = IVAL(req->in.vwv, 7);
	param_count           = IVAL(req->in.vwv, 11);
	param_ofs             = IVAL(req->in.vwv, 15);
	param_disp            = IVAL(req->in.vwv, 19);
	data_count            = IVAL(req->in.vwv, 23);
	data_ofs              = IVAL(req->in.vwv, 27);
	data_disp             = IVAL(req->in.vwv, 31);

	if (!req_pull_blob(&req->in.bufinfo, req->in.hdr + param_ofs, param_count, &params) ||
	    !req_pull_blob(&req->in.bufinfo, req->in.hdr + data_ofs, data_count, &data)) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	/* only allow contiguous requests */
	if ((param_count != 0 &&
	     param_disp != trans->in.params.length) ||
	    (data_count != 0 &&
	     data_disp != trans->in.data.length)) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	/* add to the existing request */
	if (param_count != 0) {
		trans->in.params.data = talloc_realloc(trans,
						       trans->in.params.data,
						       uint8_t,
						       param_disp + param_count);
		if (trans->in.params.data == NULL) {
			smbsrv_send_error(tp->req, NT_STATUS_NO_MEMORY);
			return;
		}
		trans->in.params.length = param_disp + param_count;
	}

	if (data_count != 0) {
		trans->in.data.data = talloc_realloc(trans,
						     trans->in.data.data,
						     uint8_t,
						     data_disp + data_count);
		if (trans->in.data.data == NULL) {
			smbsrv_send_error(tp->req, NT_STATUS_NO_MEMORY);
			return;
		}
		trans->in.data.length = data_disp + data_count;
	}

	memcpy(trans->in.params.data + param_disp, params.data, params.length);
	memcpy(trans->in.data.data + data_disp, data.data, data.length);

	/* the sequence number of the reply is taken from the last secondary
	   response */
	tp->req->seq_num = req->seq_num;

	/* we don't reply to Transs2 requests */
	talloc_free(req);

	if (trans->in.params.length == param_total &&
	    trans->in.data.length == data_total) {
		/* its now complete */
		req = tp->req;
		talloc_free(tp);
		reply_nttrans_complete(req, trans);
	}
	return;
}
