/*
   Unix SMB/CIFS implementation.
   SMB NT transaction handling
   Copyright (C) Jeremy Allison			1994-2007
   Copyright (C) Stefan (metze) Metzmacher	2003

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
#include "system/filesys.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "fake_file.h"
#include "../libcli/security/security.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "passdb/lookup_sid.h"
#include "auth.h"
#include "ntioctl.h"
#include "smbprofile.h"
#include "libsmb/libsmb.h"

extern const struct generic_mapping file_generic_mapping;

static char *nttrans_realloc(char **ptr, size_t size)
{
	if (ptr==NULL) {
		smb_panic("nttrans_realloc() called with NULL ptr");
	}

	*ptr = (char *)SMB_REALLOC(*ptr, size);
	if(*ptr == NULL) {
		return NULL;
	}
	memset(*ptr,'\0',size);
	return *ptr;
}

/****************************************************************************
 Send the required number of replies back.
 We assume all fields other than the data fields are
 set correctly for the type of call.
 HACK ! Always assumes smb_setup field is zero.
****************************************************************************/

void send_nt_replies(connection_struct *conn,
			struct smb_request *req, NTSTATUS nt_error,
		     char *params, int paramsize,
		     char *pdata, int datasize)
{
	int data_to_send = datasize;
	int params_to_send = paramsize;
	int useable_space;
	char *pp = params;
	char *pd = pdata;
	int params_sent_thistime, data_sent_thistime, total_sent_thistime;
	int alignment_offset = 1;
	int data_alignment_offset = 0;
	struct smbd_server_connection *sconn = req->sconn;
	int max_send = sconn->smb1.sessions.max_send;

	/*
	 * If there genuinely are no parameters or data to send just send
	 * the empty packet.
	 */

	if(params_to_send == 0 && data_to_send == 0) {
		reply_outbuf(req, 18, 0);
		if (NT_STATUS_V(nt_error)) {
			error_packet_set((char *)req->outbuf,
					 0, 0, nt_error,
					 __LINE__,__FILE__);
		}
		show_msg((char *)req->outbuf);
		if (!srv_send_smb(sconn,
				(char *)req->outbuf,
				true, req->seqnum+1,
				IS_CONN_ENCRYPTED(conn),
				&req->pcd)) {
			exit_server_cleanly("send_nt_replies: srv_send_smb failed.");
		}
		TALLOC_FREE(req->outbuf);
		return;
	}

	/*
	 * When sending params and data ensure that both are nicely aligned.
	 * Only do this alignment when there is also data to send - else
	 * can cause NT redirector problems.
	 */

	if (((params_to_send % 4) != 0) && (data_to_send != 0)) {
		data_alignment_offset = 4 - (params_to_send % 4);
	}

	/*
	 * Space is bufsize minus Netbios over TCP header minus SMB header.
	 * The alignment_offset is to align the param bytes on a four byte
	 * boundary (2 bytes for data len, one byte pad).
	 * NT needs this to work correctly.
	 */

	useable_space = max_send - (smb_size
				    + 2 * 18 /* wct */
				    + alignment_offset
				    + data_alignment_offset);

	if (useable_space < 0) {
		char *msg = talloc_asprintf(
			talloc_tos(),
			"send_nt_replies failed sanity useable_space = %d!!!",
			useable_space);
		DEBUG(0, ("%s\n", msg));
		exit_server_cleanly(msg);
	}

	while (params_to_send || data_to_send) {

		/*
		 * Calculate whether we will totally or partially fill this packet.
		 */

		total_sent_thistime = params_to_send + data_to_send;

		/*
		 * We can never send more than useable_space.
		 */

		total_sent_thistime = MIN(total_sent_thistime, useable_space);

		reply_outbuf(req, 18,
			     total_sent_thistime + alignment_offset
			     + data_alignment_offset);

		/*
		 * Set total params and data to be sent.
		 */

		SIVAL(req->outbuf,smb_ntr_TotalParameterCount,paramsize);
		SIVAL(req->outbuf,smb_ntr_TotalDataCount,datasize);

		/*
		 * Calculate how many parameters and data we can fit into
		 * this packet. Parameters get precedence.
		 */

		params_sent_thistime = MIN(params_to_send,useable_space);
		data_sent_thistime = useable_space - params_sent_thistime;
		data_sent_thistime = MIN(data_sent_thistime,data_to_send);

		SIVAL(req->outbuf, smb_ntr_ParameterCount,
		      params_sent_thistime);

		if(params_sent_thistime == 0) {
			SIVAL(req->outbuf,smb_ntr_ParameterOffset,0);
			SIVAL(req->outbuf,smb_ntr_ParameterDisplacement,0);
		} else {
			/*
			 * smb_ntr_ParameterOffset is the offset from the start of the SMB header to the
			 * parameter bytes, however the first 4 bytes of outbuf are
			 * the Netbios over TCP header. Thus use smb_base() to subtract
			 * them from the calculation.
			 */

			SIVAL(req->outbuf,smb_ntr_ParameterOffset,
			      ((smb_buf(req->outbuf)+alignment_offset)
			       - smb_base(req->outbuf)));
			/*
			 * Absolute displacement of param bytes sent in this packet.
			 */

			SIVAL(req->outbuf, smb_ntr_ParameterDisplacement,
			      pp - params);
		}

		/*
		 * Deal with the data portion.
		 */

		SIVAL(req->outbuf, smb_ntr_DataCount, data_sent_thistime);

		if(data_sent_thistime == 0) {
			SIVAL(req->outbuf,smb_ntr_DataOffset,0);
			SIVAL(req->outbuf,smb_ntr_DataDisplacement, 0);
		} else {
			/*
			 * The offset of the data bytes is the offset of the
			 * parameter bytes plus the number of parameters being sent this time.
			 */

			SIVAL(req->outbuf, smb_ntr_DataOffset,
			      ((smb_buf(req->outbuf)+alignment_offset) -
			       smb_base(req->outbuf))
			      + params_sent_thistime + data_alignment_offset);
			SIVAL(req->outbuf,smb_ntr_DataDisplacement, pd - pdata);
		}

		/*
		 * Copy the param bytes into the packet.
		 */

		if(params_sent_thistime) {
			if (alignment_offset != 0) {
				memset(smb_buf(req->outbuf), 0,
				       alignment_offset);
			}
			memcpy((smb_buf(req->outbuf)+alignment_offset), pp,
			       params_sent_thistime);
		}

		/*
		 * Copy in the data bytes
		 */

		if(data_sent_thistime) {
			if (data_alignment_offset != 0) {
				memset((smb_buf(req->outbuf)+alignment_offset+
					params_sent_thistime), 0,
				       data_alignment_offset);
			}
			memcpy(smb_buf(req->outbuf)+alignment_offset
			       +params_sent_thistime+data_alignment_offset,
			       pd,data_sent_thistime);
		}

		DEBUG(9,("nt_rep: params_sent_thistime = %d, data_sent_thistime = %d, useable_space = %d\n",
			params_sent_thistime, data_sent_thistime, useable_space));
		DEBUG(9,("nt_rep: params_to_send = %d, data_to_send = %d, paramsize = %d, datasize = %d\n",
			params_to_send, data_to_send, paramsize, datasize));

		if (NT_STATUS_V(nt_error)) {
			error_packet_set((char *)req->outbuf,
					 0, 0, nt_error,
					 __LINE__,__FILE__);
		}

		/* Send the packet */
		show_msg((char *)req->outbuf);
		if (!srv_send_smb(sconn,
				(char *)req->outbuf,
				true, req->seqnum+1,
				IS_CONN_ENCRYPTED(conn),
				&req->pcd)) {
			exit_server_cleanly("send_nt_replies: srv_send_smb failed.");
		}

		TALLOC_FREE(req->outbuf);

		pp += params_sent_thistime;
		pd += data_sent_thistime;

		params_to_send -= params_sent_thistime;
		data_to_send -= data_sent_thistime;

		/*
		 * Sanity check
		 */

		if(params_to_send < 0 || data_to_send < 0) {
			DEBUG(0,("send_nt_replies failed sanity check pts = %d, dts = %d\n!!!",
				params_to_send, data_to_send));
			exit_server_cleanly("send_nt_replies: internal error");
		}
	}
}

/****************************************************************************
 Reply to an NT create and X call on a pipe
****************************************************************************/

static void nt_open_pipe(char *fname, connection_struct *conn,
			 struct smb_request *req, int *ppnum)
{
	files_struct *fsp;
	NTSTATUS status;

	DEBUG(4,("nt_open_pipe: Opening pipe %s.\n", fname));

	/* Strip \\ off the name. */
	fname++;

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

	*ppnum = fsp->fnum;
	return;
}

/****************************************************************************
 Reply to an NT create and X call for pipes.
****************************************************************************/

static void do_ntcreate_pipe_open(connection_struct *conn,
				  struct smb_request *req)
{
	char *fname = NULL;
	int pnum = -1;
	char *p = NULL;
	uint32 flags = IVAL(req->vwv+3, 1);
	TALLOC_CTX *ctx = talloc_tos();

	srvstr_pull_req_talloc(ctx, req, &fname, req->buf, STR_TERMINATE);

	if (!fname) {
		reply_botherror(req, NT_STATUS_OBJECT_NAME_NOT_FOUND,
				ERRDOS, ERRbadpipe);
		return;
	}
	nt_open_pipe(fname, conn, req, &pnum);

	if (req->outbuf) {
		/* error reply */
		return;
	}

	/*
	 * Deal with pipe return.
	 */

	if (flags & EXTENDED_RESPONSE_REQUIRED) {
		/* This is very strange. We
 		 * return 50 words, but only set
 		 * the wcnt to 42 ? It's definately
 		 * what happens on the wire....
 		 */
		reply_outbuf(req, 50, 0);
		SCVAL(req->outbuf,smb_wct,42);
	} else {
		reply_outbuf(req, 34, 0);
	}

	p = (char *)req->outbuf + smb_vwv2;
	p++;
	SSVAL(p,0,pnum);
	p += 2;
	SIVAL(p,0,FILE_WAS_OPENED);
	p += 4;
	p += 32;
	SIVAL(p,0,FILE_ATTRIBUTE_NORMAL); /* File Attributes. */
	p += 20;
	/* File type. */
	SSVAL(p,0,FILE_TYPE_MESSAGE_MODE_PIPE);
	/* Device state. */
	SSVAL(p,2, 0x5FF); /* ? */
	p += 4;

	if (flags & EXTENDED_RESPONSE_REQUIRED) {
		p += 25;
		SIVAL(p,0,FILE_GENERIC_ALL);
		/*
		 * For pipes W2K3 seems to return
 		 * 0x12019B next.
 		 * This is ((FILE_GENERIC_READ|FILE_GENERIC_WRITE) & ~FILE_APPEND_DATA)
 		 */
		SIVAL(p,4,(FILE_GENERIC_READ|FILE_GENERIC_WRITE)&~FILE_APPEND_DATA);
	}

	DEBUG(5,("do_ntcreate_pipe_open: open pipe = %s\n", fname));

	chain_reply(req);
}

struct case_semantics_state {
	connection_struct *conn;
	bool case_sensitive;
	bool case_preserve;
	bool short_case_preserve;
};

/****************************************************************************
 Restore case semantics.
****************************************************************************/

static int restore_case_semantics(struct case_semantics_state *state)
{
	state->conn->case_sensitive = state->case_sensitive;
	state->conn->case_preserve = state->case_preserve;
	state->conn->short_case_preserve = state->short_case_preserve;
	return 0;
}

/****************************************************************************
 Save case semantics.
****************************************************************************/

static struct case_semantics_state *set_posix_case_semantics(TALLOC_CTX *mem_ctx,
						connection_struct *conn)
{
	struct case_semantics_state *result;

	if (!(result = talloc(mem_ctx, struct case_semantics_state))) {
		return NULL;
	}

	result->conn = conn;
	result->case_sensitive = conn->case_sensitive;
	result->case_preserve = conn->case_preserve;
	result->short_case_preserve = conn->short_case_preserve;

	/* Set to POSIX. */
	conn->case_sensitive = True;
	conn->case_preserve = True;
	conn->short_case_preserve = True;

	talloc_set_destructor(result, restore_case_semantics);

	return result;
}

/****************************************************************************
 Reply to an NT create and X call.
****************************************************************************/

void reply_ntcreate_and_X(struct smb_request *req)
{
	connection_struct *conn = req->conn;
	struct smb_filename *smb_fname = NULL;
	char *fname = NULL;
	uint32 flags;
	uint32 access_mask;
	uint32 file_attributes;
	uint32 share_access;
	uint32 create_disposition;
	uint32 create_options;
	uint16 root_dir_fid;
	uint64_t allocation_size;
	/* Breakout the oplock request bits so we can set the
	   reply bits separately. */
	uint32 fattr=0;
	SMB_OFF_T file_len = 0;
	int info = 0;
	files_struct *fsp = NULL;
	char *p = NULL;
	struct timespec create_timespec;
	struct timespec c_timespec;
	struct timespec a_timespec;
	struct timespec m_timespec;
	struct timespec write_time_ts;
	NTSTATUS status;
	int oplock_request;
	uint8_t oplock_granted = NO_OPLOCK_RETURN;
	struct case_semantics_state *case_state = NULL;
	TALLOC_CTX *ctx = talloc_tos();

	START_PROFILE(SMBntcreateX);

	if (req->wct < 24) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		goto out;
	}

	flags = IVAL(req->vwv+3, 1);
	access_mask = IVAL(req->vwv+7, 1);
	file_attributes = IVAL(req->vwv+13, 1);
	share_access = IVAL(req->vwv+15, 1);
	create_disposition = IVAL(req->vwv+17, 1);
	create_options = IVAL(req->vwv+19, 1);
	root_dir_fid = (uint16)IVAL(req->vwv+5, 1);

	allocation_size = BVAL(req->vwv+9, 1);

	srvstr_get_path_req(ctx, req, &fname, (const char *)req->buf,
			    STR_TERMINATE, &status);

	if (!NT_STATUS_IS_OK(status)) {
		reply_nterror(req, status);
		goto out;
	}

	DEBUG(10,("reply_ntcreate_and_X: flags = 0x%x, access_mask = 0x%x "
		  "file_attributes = 0x%x, share_access = 0x%x, "
		  "create_disposition = 0x%x create_options = 0x%x "
		  "root_dir_fid = 0x%x, fname = %s\n",
			(unsigned int)flags,
			(unsigned int)access_mask,
			(unsigned int)file_attributes,
			(unsigned int)share_access,
			(unsigned int)create_disposition,
			(unsigned int)create_options,
			(unsigned int)root_dir_fid,
			fname));

	/*
	 * we need to remove ignored bits when they come directly from the client
	 * because we reuse some of them for internal stuff
	 */
	create_options &= ~NTCREATEX_OPTIONS_MUST_IGNORE_MASK;

	/*
	 * If it's an IPC, use the pipe handler.
	 */

	if (IS_IPC(conn)) {
		if (lp_nt_pipe_support()) {
			do_ntcreate_pipe_open(conn, req);
			goto out;
		}
		reply_nterror(req, NT_STATUS_ACCESS_DENIED);
		goto out;
	}

	oplock_request = (flags & REQUEST_OPLOCK) ? EXCLUSIVE_OPLOCK : 0;
	if (oplock_request) {
		oplock_request |= (flags & REQUEST_BATCH_OPLOCK)
			? BATCH_OPLOCK : 0;
	}

	if (file_attributes & FILE_FLAG_POSIX_SEMANTICS) {
		case_state = set_posix_case_semantics(ctx, conn);
		if (!case_state) {
			reply_nterror(req, NT_STATUS_NO_MEMORY);
			goto out;
		}
	}

	status = filename_convert(ctx,
				conn,
				req->flags2 & FLAGS2_DFS_PATHNAMES,
				fname,
				(create_disposition == FILE_CREATE)
					? UCF_CREATING_FILE : 0,
				NULL,
				&smb_fname);

	TALLOC_FREE(case_state);

	if (!NT_STATUS_IS_OK(status)) {
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			reply_botherror(req,
				NT_STATUS_PATH_NOT_COVERED,
				ERRSRV, ERRbadpath);
			goto out;
		}
		reply_nterror(req, status);
		goto out;
	}

	/*
	 * Bug #6898 - clients using Windows opens should
	 * never be able to set this attribute into the
	 * VFS.
	 */
	file_attributes &= ~FILE_FLAG_POSIX_SEMANTICS;

	status = SMB_VFS_CREATE_FILE(
		conn,					/* conn */
		req,					/* req */
		root_dir_fid,				/* root_dir_fid */
		smb_fname,				/* fname */
		access_mask,				/* access_mask */
		share_access,				/* share_access */
		create_disposition,			/* create_disposition*/
		create_options,				/* create_options */
		file_attributes,			/* file_attributes */
		oplock_request,				/* oplock_request */
		allocation_size,			/* allocation_size */
		0,					/* private_flags */
		NULL,					/* sd */
		NULL,					/* ea_list */
		&fsp,					/* result */
		&info);					/* pinfo */

	if (!NT_STATUS_IS_OK(status)) {
		if (open_was_deferred(req->mid)) {
			/* We have re-scheduled this call, no error. */
			goto out;
		}
		reply_openerror(req, status);
		goto out;
	}

	/* Ensure we're pointing at the correct stat struct. */
	TALLOC_FREE(smb_fname);
	smb_fname = fsp->fsp_name;

	/*
	 * If the caller set the extended oplock request bit
	 * and we granted one (by whatever means) - set the
	 * correct bit for extended oplock reply.
	 */

	if (oplock_request &&
	    (lp_fake_oplocks(SNUM(conn))
	     || EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type))) {

		/*
		 * Exclusive oplock granted
		 */

		if (flags & REQUEST_BATCH_OPLOCK) {
			oplock_granted = BATCH_OPLOCK_RETURN;
		} else {
			oplock_granted = EXCLUSIVE_OPLOCK_RETURN;
		}
	} else if (fsp->oplock_type == LEVEL_II_OPLOCK) {
		oplock_granted = LEVEL_II_OPLOCK_RETURN;
	} else {
		oplock_granted = NO_OPLOCK_RETURN;
	}

	file_len = smb_fname->st.st_ex_size;

	if (flags & EXTENDED_RESPONSE_REQUIRED) {
		/* This is very strange. We
 		 * return 50 words, but only set
 		 * the wcnt to 42 ? It's definately
 		 * what happens on the wire....
 		 */
		reply_outbuf(req, 50, 0);
		SCVAL(req->outbuf,smb_wct,42);
	} else {
		reply_outbuf(req, 34, 0);
	}

	p = (char *)req->outbuf + smb_vwv2;

	SCVAL(p, 0, oplock_granted);

	p++;
	SSVAL(p,0,fsp->fnum);
	p += 2;
	if ((create_disposition == FILE_SUPERSEDE)
	    && (info == FILE_WAS_OVERWRITTEN)) {
		SIVAL(p,0,FILE_WAS_SUPERSEDED);
	} else {
		SIVAL(p,0,info);
	}
	p += 4;

	fattr = dos_mode(conn, smb_fname);
	if (fattr == 0) {
		fattr = FILE_ATTRIBUTE_NORMAL;
	}

	/* Deal with other possible opens having a modified
	   write time. JRA. */
	ZERO_STRUCT(write_time_ts);
	get_file_infos(fsp->file_id, 0, NULL, &write_time_ts);
	if (!null_timespec(write_time_ts)) {
		update_stat_ex_mtime(&smb_fname->st, write_time_ts);
	}

	/* Create time. */
	create_timespec = get_create_timespec(conn, fsp, smb_fname);
	a_timespec = smb_fname->st.st_ex_atime;
	m_timespec = smb_fname->st.st_ex_mtime;
	c_timespec = get_change_timespec(conn, fsp, smb_fname);

	if (lp_dos_filetime_resolution(SNUM(conn))) {
		dos_filetime_timespec(&create_timespec);
		dos_filetime_timespec(&a_timespec);
		dos_filetime_timespec(&m_timespec);
		dos_filetime_timespec(&c_timespec);
	}

	put_long_date_timespec(conn->ts_res, p, create_timespec); /* create time. */
	p += 8;
	put_long_date_timespec(conn->ts_res, p, a_timespec); /* access time */
	p += 8;
	put_long_date_timespec(conn->ts_res, p, m_timespec); /* write time */
	p += 8;
	put_long_date_timespec(conn->ts_res, p, c_timespec); /* change time */
	p += 8;
	SIVAL(p,0,fattr); /* File Attributes. */
	p += 4;
	SOFF_T(p, 0, SMB_VFS_GET_ALLOC_SIZE(conn,fsp,&smb_fname->st));
	p += 8;
	SOFF_T(p,0,file_len);
	p += 8;
	if (flags & EXTENDED_RESPONSE_REQUIRED) {
		uint16_t file_status = (NO_EAS|NO_SUBSTREAMS|NO_REPARSETAG);
		size_t num_names = 0;
		unsigned int num_streams = 0;
		struct stream_struct *streams = NULL;

		/* Do we have any EA's ? */
		status = get_ea_names_from_file(ctx, conn, fsp,
				smb_fname->base_name, NULL, &num_names);
		if (NT_STATUS_IS_OK(status) && num_names) {
			file_status &= ~NO_EAS;
		}
		status = vfs_streaminfo(conn, NULL, smb_fname->base_name, ctx,
			&num_streams, &streams);
		/* There is always one stream, ::$DATA. */
		if (NT_STATUS_IS_OK(status) && num_streams > 1) {
			file_status &= ~NO_SUBSTREAMS;
		}
		TALLOC_FREE(streams);
		SSVAL(p,2,file_status);
	}
	p += 4;
	SCVAL(p,0,fsp->is_directory ? 1 : 0);

	if (flags & EXTENDED_RESPONSE_REQUIRED) {
		uint32 perms = 0;
		p += 25;
		if (fsp->is_directory ||
		    can_write_to_file(conn, smb_fname)) {
			perms = FILE_GENERIC_ALL;
		} else {
			perms = FILE_GENERIC_READ|FILE_EXECUTE;
		}
		SIVAL(p,0,perms);
	}

	DEBUG(5,("reply_ntcreate_and_X: fnum = %d, open name = %s\n",
		fsp->fnum, smb_fname_str_dbg(smb_fname)));

	chain_reply(req);
 out:
	END_PROFILE(SMBntcreateX);
	return;
}

/****************************************************************************
 Reply to a NT_TRANSACT_CREATE call to open a pipe.
****************************************************************************/

static void do_nt_transact_create_pipe(connection_struct *conn,
				       struct smb_request *req,
				       uint16 **ppsetup, uint32 setup_count,
				       char **ppparams, uint32 parameter_count,
				       char **ppdata, uint32 data_count)
{
	char *fname = NULL;
	char *params = *ppparams;
	int pnum = -1;
	char *p = NULL;
	NTSTATUS status;
	size_t param_len;
	uint32 flags;
	TALLOC_CTX *ctx = talloc_tos();

	/*
	 * Ensure minimum number of parameters sent.
	 */

	if(parameter_count < 54) {
		DEBUG(0,("do_nt_transact_create_pipe - insufficient parameters (%u)\n", (unsigned int)parameter_count));
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	flags = IVAL(params,0);

	srvstr_get_path(ctx, params, req->flags2, &fname, params+53,
			parameter_count-53, STR_TERMINATE,
			&status);
	if (!NT_STATUS_IS_OK(status)) {
		reply_nterror(req, status);
		return;
	}

	nt_open_pipe(fname, conn, req, &pnum);

	if (req->outbuf) {
		/* Error return */
		return;
	}

	/* Realloc the size of parameters and data we will return */
	if (flags & EXTENDED_RESPONSE_REQUIRED) {
		/* Extended response is 32 more byyes. */
		param_len = 101;
	} else {
		param_len = 69;
	}
	params = nttrans_realloc(ppparams, param_len);
	if(params == NULL) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}

	p = params;
	SCVAL(p,0,NO_OPLOCK_RETURN);

	p += 2;
	SSVAL(p,0,pnum);
	p += 2;
	SIVAL(p,0,FILE_WAS_OPENED);
	p += 8;

	p += 32;
	SIVAL(p,0,FILE_ATTRIBUTE_NORMAL); /* File Attributes. */
	p += 20;
	/* File type. */
	SSVAL(p,0,FILE_TYPE_MESSAGE_MODE_PIPE);
	/* Device state. */
	SSVAL(p,2, 0x5FF); /* ? */
	p += 4;

	if (flags & EXTENDED_RESPONSE_REQUIRED) {
		p += 25;
		SIVAL(p,0,FILE_GENERIC_ALL);
		/*
		 * For pipes W2K3 seems to return
 		 * 0x12019B next.
 		 * This is ((FILE_GENERIC_READ|FILE_GENERIC_WRITE) & ~FILE_APPEND_DATA)
 		 */
		SIVAL(p,4,(FILE_GENERIC_READ|FILE_GENERIC_WRITE)&~FILE_APPEND_DATA);
	}

	DEBUG(5,("do_nt_transact_create_pipe: open name = %s\n", fname));

	/* Send the required number of replies */
	send_nt_replies(conn, req, NT_STATUS_OK, params, param_len, *ppdata, 0);

	return;
}

/*********************************************************************
 Windows seems to do canonicalization of inheritance bits. Do the
 same.
*********************************************************************/

static void canonicalize_inheritance_bits(struct security_descriptor *psd)
{
	bool set_auto_inherited = false;

	/*
	 * We need to filter out the
	 * SEC_DESC_DACL_AUTO_INHERITED|SEC_DESC_DACL_AUTO_INHERIT_REQ
	 * bits. If both are set we store SEC_DESC_DACL_AUTO_INHERITED
	 * as this alters whether SEC_ACE_FLAG_INHERITED_ACE is set
	 * when an ACE is inherited. Otherwise we zero these bits out.
	 * See:
	 *
	 * http://social.msdn.microsoft.com/Forums/eu/os_fileservices/thread/11f77b68-731e-407d-b1b3-064750716531
	 *
	 * for details.
	 */

	if ((psd->type & (SEC_DESC_DACL_AUTO_INHERITED|SEC_DESC_DACL_AUTO_INHERIT_REQ))
			== (SEC_DESC_DACL_AUTO_INHERITED|SEC_DESC_DACL_AUTO_INHERIT_REQ)) {
		set_auto_inherited = true;
	}

	psd->type &= ~(SEC_DESC_DACL_AUTO_INHERITED|SEC_DESC_DACL_AUTO_INHERIT_REQ);
	if (set_auto_inherited) {
		psd->type |= SEC_DESC_DACL_AUTO_INHERITED;
	}
}

/****************************************************************************
 Internal fn to set security descriptors.
****************************************************************************/

NTSTATUS set_sd(files_struct *fsp, struct security_descriptor *psd,
		       uint32_t security_info_sent)
{
	NTSTATUS status;

	if (!CAN_WRITE(fsp->conn)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (!lp_nt_acl_support(SNUM(fsp->conn))) {
		return NT_STATUS_OK;
	}

	if (S_ISLNK(fsp->fsp_name->st.st_ex_mode)) {
		DEBUG(10, ("ACL set on symlink %s denied.\n",
			fsp_str_dbg(fsp)));
		return NT_STATUS_ACCESS_DENIED;
	}

	if (psd->owner_sid == NULL) {
		security_info_sent &= ~SECINFO_OWNER;
	}
	if (psd->group_sid == NULL) {
		security_info_sent &= ~SECINFO_GROUP;
	}

	/* Ensure we have at least one thing set. */
	if ((security_info_sent & (SECINFO_OWNER|SECINFO_GROUP|SECINFO_DACL|SECINFO_SACL)) == 0) {
		/* Just like W2K3 */
		return NT_STATUS_OK;
	}

	/* Ensure we have the rights to do this. */
	if (security_info_sent & SECINFO_OWNER) {
		if (!(fsp->access_mask & SEC_STD_WRITE_OWNER)) {
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	if (security_info_sent & SECINFO_GROUP) {
		if (!(fsp->access_mask & SEC_STD_WRITE_OWNER)) {
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	if (security_info_sent & SECINFO_DACL) {
		if (!(fsp->access_mask & SEC_STD_WRITE_DAC)) {
			return NT_STATUS_ACCESS_DENIED;
		}
		/* Convert all the generic bits. */
		if (psd->dacl) {
			security_acl_map_generic(psd->dacl, &file_generic_mapping);
		}
	}

	if (security_info_sent & SECINFO_SACL) {
		if (!(fsp->access_mask & SEC_FLAG_SYSTEM_SECURITY)) {
			return NT_STATUS_ACCESS_DENIED;
		}
		/* Convert all the generic bits. */
		if (psd->sacl) {
			security_acl_map_generic(psd->sacl, &file_generic_mapping);
		}
	}

	canonicalize_inheritance_bits(psd);

	if (DEBUGLEVEL >= 10) {
		DEBUG(10,("set_sd for file %s\n", fsp_str_dbg(fsp)));
		NDR_PRINT_DEBUG(security_descriptor, psd);
	}

	status = SMB_VFS_FSET_NT_ACL(fsp, security_info_sent, psd);

	TALLOC_FREE(psd);

	return status;
}

/****************************************************************************
 Internal fn to set security descriptors from a data blob.
****************************************************************************/

NTSTATUS set_sd_blob(files_struct *fsp, uint8_t *data, uint32_t sd_len,
		       uint32_t security_info_sent)
{
	struct security_descriptor *psd = NULL;
	NTSTATUS status;

	if (sd_len == 0) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = unmarshall_sec_desc(talloc_tos(), data, sd_len, &psd);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return set_sd(fsp, psd, security_info_sent);
}

/****************************************************************************
 Read a list of EA names and data from an incoming data buffer. Create an ea_list with them.
****************************************************************************/

struct ea_list *read_nttrans_ea_list(TALLOC_CTX *ctx, const char *pdata, size_t data_size)
{
	struct ea_list *ea_list_head = NULL;
	size_t offset = 0;

	if (data_size < 4) {
		return NULL;
	}

	while (offset + 4 <= data_size) {
		size_t next_offset = IVAL(pdata,offset);
		struct ea_list *eal = read_ea_list_entry(ctx, pdata + offset + 4, data_size - offset - 4, NULL);

		if (!eal) {
			return NULL;
		}

		DLIST_ADD_END(ea_list_head, eal, struct ea_list *);
		if (next_offset == 0) {
			break;
		}

		/* Integer wrap protection for the increment. */
		if (offset + next_offset < offset) {
			break;
		}

		offset += next_offset;

		/* Integer wrap protection for while loop. */
		if (offset + 4 < offset) {
			break;
		}

	}

	return ea_list_head;
}

/****************************************************************************
 Reply to a NT_TRANSACT_CREATE call (needs to process SD's).
****************************************************************************/

static void call_nt_transact_create(connection_struct *conn,
				    struct smb_request *req,
				    uint16 **ppsetup, uint32 setup_count,
				    char **ppparams, uint32 parameter_count,
				    char **ppdata, uint32 data_count,
				    uint32 max_data_count)
{
	struct smb_filename *smb_fname = NULL;
	char *fname = NULL;
	char *params = *ppparams;
	char *data = *ppdata;
	/* Breakout the oplock request bits so we can set the reply bits separately. */
	uint32 fattr=0;
	SMB_OFF_T file_len = 0;
	int info = 0;
	files_struct *fsp = NULL;
	char *p = NULL;
	uint32 flags;
	uint32 access_mask;
	uint32 file_attributes;
	uint32 share_access;
	uint32 create_disposition;
	uint32 create_options;
	uint32 sd_len;
	struct security_descriptor *sd = NULL;
	uint32 ea_len;
	uint16 root_dir_fid;
	struct timespec create_timespec;
	struct timespec c_timespec;
	struct timespec a_timespec;
	struct timespec m_timespec;
	struct timespec write_time_ts;
	struct ea_list *ea_list = NULL;
	NTSTATUS status;
	size_t param_len;
	uint64_t allocation_size;
	int oplock_request;
	uint8_t oplock_granted;
	struct case_semantics_state *case_state = NULL;
	TALLOC_CTX *ctx = talloc_tos();

	DEBUG(5,("call_nt_transact_create\n"));

	/*
	 * If it's an IPC, use the pipe handler.
	 */

	if (IS_IPC(conn)) {
		if (lp_nt_pipe_support()) {
			do_nt_transact_create_pipe(
				conn, req,
				ppsetup, setup_count,
				ppparams, parameter_count,
				ppdata, data_count);
			goto out;
		}
		reply_nterror(req, NT_STATUS_ACCESS_DENIED);
		goto out;
	}

	/*
	 * Ensure minimum number of parameters sent.
	 */

	if(parameter_count < 54) {
		DEBUG(0,("call_nt_transact_create - insufficient parameters (%u)\n", (unsigned int)parameter_count));
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		goto out;
	}

	flags = IVAL(params,0);
	access_mask = IVAL(params,8);
	file_attributes = IVAL(params,20);
	share_access = IVAL(params,24);
	create_disposition = IVAL(params,28);
	create_options = IVAL(params,32);
	sd_len = IVAL(params,36);
	ea_len = IVAL(params,40);
	root_dir_fid = (uint16)IVAL(params,4);
	allocation_size = BVAL(params,12);

	/*
	 * we need to remove ignored bits when they come directly from the client
	 * because we reuse some of them for internal stuff
	 */
	create_options &= ~NTCREATEX_OPTIONS_MUST_IGNORE_MASK;

	/* Ensure the data_len is correct for the sd and ea values given. */
	if ((ea_len + sd_len > data_count)
	    || (ea_len > data_count) || (sd_len > data_count)
	    || (ea_len + sd_len < ea_len) || (ea_len + sd_len < sd_len)) {
		DEBUG(10, ("call_nt_transact_create - ea_len = %u, sd_len = "
			   "%u, data_count = %u\n", (unsigned int)ea_len,
			   (unsigned int)sd_len, (unsigned int)data_count));
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		goto out;
	}

	if (sd_len) {
		DEBUG(10, ("call_nt_transact_create - sd_len = %d\n",
			   sd_len));

		status = unmarshall_sec_desc(ctx, (uint8_t *)data, sd_len,
					     &sd);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10, ("call_nt_transact_create: "
				   "unmarshall_sec_desc failed: %s\n",
				   nt_errstr(status)));
			reply_nterror(req, status);
			goto out;
		}
	}

	if (ea_len) {
		if (!lp_ea_support(SNUM(conn))) {
			DEBUG(10, ("call_nt_transact_create - ea_len = %u but "
				   "EA's not supported.\n",
				   (unsigned int)ea_len));
			reply_nterror(req, NT_STATUS_EAS_NOT_SUPPORTED);
			goto out;
		}

		if (ea_len < 10) {
			DEBUG(10,("call_nt_transact_create - ea_len = %u - "
				  "too small (should be more than 10)\n",
				  (unsigned int)ea_len ));
			reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
			goto out;
		}

		/* We have already checked that ea_len <= data_count here. */
		ea_list = read_nttrans_ea_list(talloc_tos(), data + sd_len,
					       ea_len);
		if (ea_list == NULL) {
			reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
			goto out;
		}
	}

	srvstr_get_path(ctx, params, req->flags2, &fname,
			params+53, parameter_count-53,
			STR_TERMINATE, &status);
	if (!NT_STATUS_IS_OK(status)) {
		reply_nterror(req, status);
		goto out;
	}

	if (file_attributes & FILE_FLAG_POSIX_SEMANTICS) {
		case_state = set_posix_case_semantics(ctx, conn);
		if (!case_state) {
			reply_nterror(req, NT_STATUS_NO_MEMORY);
			goto out;
		}
	}

	status = filename_convert(ctx,
				conn,
				req->flags2 & FLAGS2_DFS_PATHNAMES,
				fname,
				(create_disposition == FILE_CREATE)
					? UCF_CREATING_FILE : 0,
				NULL,
				&smb_fname);

	TALLOC_FREE(case_state);

	if (!NT_STATUS_IS_OK(status)) {
		if (NT_STATUS_EQUAL(status,NT_STATUS_PATH_NOT_COVERED)) {
			reply_botherror(req,
				NT_STATUS_PATH_NOT_COVERED,
				ERRSRV, ERRbadpath);
			goto out;
		}
		reply_nterror(req, status);
		goto out;
	}

	oplock_request = (flags & REQUEST_OPLOCK) ? EXCLUSIVE_OPLOCK : 0;
	if (oplock_request) {
		oplock_request |= (flags & REQUEST_BATCH_OPLOCK)
			? BATCH_OPLOCK : 0;
	}

	/*
	 * Bug #6898 - clients using Windows opens should
	 * never be able to set this attribute into the
	 * VFS.
	 */
	file_attributes &= ~FILE_FLAG_POSIX_SEMANTICS;

	status = SMB_VFS_CREATE_FILE(
		conn,					/* conn */
		req,					/* req */
		root_dir_fid,				/* root_dir_fid */
		smb_fname,				/* fname */
		access_mask,				/* access_mask */
		share_access,				/* share_access */
		create_disposition,			/* create_disposition*/
		create_options,				/* create_options */
		file_attributes,			/* file_attributes */
		oplock_request,				/* oplock_request */
		allocation_size,			/* allocation_size */
		0,					/* private_flags */
		sd,					/* sd */
		ea_list,				/* ea_list */
		&fsp,					/* result */
		&info);					/* pinfo */

	if(!NT_STATUS_IS_OK(status)) {
		if (open_was_deferred(req->mid)) {
			/* We have re-scheduled this call, no error. */
			return;
		}
		reply_openerror(req, status);
		goto out;
	}

	/* Ensure we're pointing at the correct stat struct. */
	TALLOC_FREE(smb_fname);
	smb_fname = fsp->fsp_name;

	/*
	 * If the caller set the extended oplock request bit
	 * and we granted one (by whatever means) - set the
	 * correct bit for extended oplock reply.
	 */

	if (oplock_request &&
	    (lp_fake_oplocks(SNUM(conn))
	     || EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type))) {

		/*
		 * Exclusive oplock granted
		 */

		if (flags & REQUEST_BATCH_OPLOCK) {
			oplock_granted = BATCH_OPLOCK_RETURN;
		} else {
			oplock_granted = EXCLUSIVE_OPLOCK_RETURN;
		}
	} else if (fsp->oplock_type == LEVEL_II_OPLOCK) {
		oplock_granted = LEVEL_II_OPLOCK_RETURN;
	} else {
		oplock_granted = NO_OPLOCK_RETURN;
	}

	file_len = smb_fname->st.st_ex_size;

	/* Realloc the size of parameters and data we will return */
	if (flags & EXTENDED_RESPONSE_REQUIRED) {
		/* Extended response is 32 more byyes. */
		param_len = 101;
	} else {
		param_len = 69;
	}
	params = nttrans_realloc(ppparams, param_len);
	if(params == NULL) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		goto out;
	}

	p = params;
	SCVAL(p, 0, oplock_granted);

	p += 2;
	SSVAL(p,0,fsp->fnum);
	p += 2;
	if ((create_disposition == FILE_SUPERSEDE)
	    && (info == FILE_WAS_OVERWRITTEN)) {
		SIVAL(p,0,FILE_WAS_SUPERSEDED);
	} else {
		SIVAL(p,0,info);
	}
	p += 8;

	fattr = dos_mode(conn, smb_fname);
	if (fattr == 0) {
		fattr = FILE_ATTRIBUTE_NORMAL;
	}

	/* Deal with other possible opens having a modified
	   write time. JRA. */
	ZERO_STRUCT(write_time_ts);
	get_file_infos(fsp->file_id, 0, NULL, &write_time_ts);
	if (!null_timespec(write_time_ts)) {
		update_stat_ex_mtime(&smb_fname->st, write_time_ts);
	}

	/* Create time. */
	create_timespec = get_create_timespec(conn, fsp, smb_fname);
	a_timespec = smb_fname->st.st_ex_atime;
	m_timespec = smb_fname->st.st_ex_mtime;
	c_timespec = get_change_timespec(conn, fsp, smb_fname);

	if (lp_dos_filetime_resolution(SNUM(conn))) {
		dos_filetime_timespec(&create_timespec);
		dos_filetime_timespec(&a_timespec);
		dos_filetime_timespec(&m_timespec);
		dos_filetime_timespec(&c_timespec);
	}

	put_long_date_timespec(conn->ts_res, p, create_timespec); /* create time. */
	p += 8;
	put_long_date_timespec(conn->ts_res, p, a_timespec); /* access time */
	p += 8;
	put_long_date_timespec(conn->ts_res, p, m_timespec); /* write time */
	p += 8;
	put_long_date_timespec(conn->ts_res, p, c_timespec); /* change time */
	p += 8;
	SIVAL(p,0,fattr); /* File Attributes. */
	p += 4;
	SOFF_T(p, 0, SMB_VFS_GET_ALLOC_SIZE(conn, fsp, &smb_fname->st));
	p += 8;
	SOFF_T(p,0,file_len);
	p += 8;
	if (flags & EXTENDED_RESPONSE_REQUIRED) {
		uint16_t file_status = (NO_EAS|NO_SUBSTREAMS|NO_REPARSETAG);
		size_t num_names = 0;
		unsigned int num_streams = 0;
		struct stream_struct *streams = NULL;

		/* Do we have any EA's ? */
		status = get_ea_names_from_file(ctx, conn, fsp,
				smb_fname->base_name, NULL, &num_names);
		if (NT_STATUS_IS_OK(status) && num_names) {
			file_status &= ~NO_EAS;
		}
		status = vfs_streaminfo(conn, NULL, smb_fname->base_name, ctx,
			&num_streams, &streams);
		/* There is always one stream, ::$DATA. */
		if (NT_STATUS_IS_OK(status) && num_streams > 1) {
			file_status &= ~NO_SUBSTREAMS;
		}
		TALLOC_FREE(streams);
		SSVAL(p,2,file_status);
	}
	p += 4;
	SCVAL(p,0,fsp->is_directory ? 1 : 0);

	if (flags & EXTENDED_RESPONSE_REQUIRED) {
		uint32 perms = 0;
		p += 25;
		if (fsp->is_directory ||
		    can_write_to_file(conn, smb_fname)) {
			perms = FILE_GENERIC_ALL;
		} else {
			perms = FILE_GENERIC_READ|FILE_EXECUTE;
		}
		SIVAL(p,0,perms);
	}

	DEBUG(5,("call_nt_transact_create: open name = %s\n",
		 smb_fname_str_dbg(smb_fname)));

	/* Send the required number of replies */
	send_nt_replies(conn, req, NT_STATUS_OK, params, param_len, *ppdata, 0);
 out:
	return;
}

/****************************************************************************
 Reply to a NT CANCEL request.
 conn POINTER CAN BE NULL HERE !
****************************************************************************/

void reply_ntcancel(struct smb_request *req)
{
	/*
	 * Go through and cancel any pending change notifies.
	 */

	START_PROFILE(SMBntcancel);
	srv_cancel_sign_response(req->sconn);
	remove_pending_change_notify_requests_by_mid(req->sconn, req->mid);
	remove_pending_lock_requests_by_mid_smb1(req->sconn, req->mid);

	DEBUG(3,("reply_ntcancel: cancel called on mid = %llu.\n",
		(unsigned long long)req->mid));

	END_PROFILE(SMBntcancel);
	return;
}

/****************************************************************************
 Copy a file.
****************************************************************************/

static NTSTATUS copy_internals(TALLOC_CTX *ctx,
				connection_struct *conn,
				struct smb_request *req,
				struct smb_filename *smb_fname_src,
				struct smb_filename *smb_fname_dst,
				uint32 attrs)
{
	files_struct *fsp1,*fsp2;
	uint32 fattr;
	int info;
	SMB_OFF_T ret=-1;
	NTSTATUS status = NT_STATUS_OK;
	char *parent;

	if (!CAN_WRITE(conn)) {
		status = NT_STATUS_MEDIA_WRITE_PROTECTED;
		goto out;
	}

        /* Source must already exist. */
	if (!VALID_STAT(smb_fname_src->st)) {
		status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
		goto out;
	}

	/* Ensure attributes match. */
	fattr = dos_mode(conn, smb_fname_src);
	if ((fattr & ~attrs) & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) {
		status = NT_STATUS_NO_SUCH_FILE;
		goto out;
	}

	/* Disallow if dst file already exists. */
	if (VALID_STAT(smb_fname_dst->st)) {
		status = NT_STATUS_OBJECT_NAME_COLLISION;
		goto out;
	}

	/* No links from a directory. */
	if (S_ISDIR(smb_fname_src->st.st_ex_mode)) {
		status = NT_STATUS_FILE_IS_A_DIRECTORY;
		goto out;
	}

	DEBUG(10,("copy_internals: doing file copy %s to %s\n",
		  smb_fname_str_dbg(smb_fname_src),
		  smb_fname_str_dbg(smb_fname_dst)));

        status = SMB_VFS_CREATE_FILE(
		conn,					/* conn */
		req,					/* req */
		0,					/* root_dir_fid */
		smb_fname_src,				/* fname */
		FILE_READ_DATA|FILE_READ_ATTRIBUTES|
			FILE_READ_EA,			/* access_mask */
		(FILE_SHARE_READ | FILE_SHARE_WRITE |	/* share_access */
		    FILE_SHARE_DELETE),
		FILE_OPEN,				/* create_disposition*/
		0,					/* create_options */
		FILE_ATTRIBUTE_NORMAL,			/* file_attributes */
		NO_OPLOCK,				/* oplock_request */
		0,					/* allocation_size */
		0,					/* private_flags */
		NULL,					/* sd */
		NULL,					/* ea_list */
		&fsp1,					/* result */
		&info);					/* pinfo */

	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

        status = SMB_VFS_CREATE_FILE(
		conn,					/* conn */
		req,					/* req */
		0,					/* root_dir_fid */
		smb_fname_dst,				/* fname */
		FILE_WRITE_DATA|FILE_WRITE_ATTRIBUTES|
			FILE_WRITE_EA,			/* access_mask */
		(FILE_SHARE_READ | FILE_SHARE_WRITE |	/* share_access */
		    FILE_SHARE_DELETE),
		FILE_CREATE,				/* create_disposition*/
		0,					/* create_options */
		fattr,					/* file_attributes */
		NO_OPLOCK,				/* oplock_request */
		0,					/* allocation_size */
		0,					/* private_flags */
		NULL,					/* sd */
		NULL,					/* ea_list */
		&fsp2,					/* result */
		&info);					/* pinfo */

	if (!NT_STATUS_IS_OK(status)) {
		close_file(NULL, fsp1, ERROR_CLOSE);
		goto out;
	}

	if (smb_fname_src->st.st_ex_size) {
		ret = vfs_transfer_file(fsp1, fsp2, smb_fname_src->st.st_ex_size);
	}

	/*
	 * As we are opening fsp1 read-only we only expect
	 * an error on close on fsp2 if we are out of space.
	 * Thus we don't look at the error return from the
	 * close of fsp1.
	 */
	close_file(NULL, fsp1, NORMAL_CLOSE);

	/* Ensure the modtime is set correctly on the destination file. */
	set_close_write_time(fsp2, smb_fname_src->st.st_ex_mtime);

	status = close_file(NULL, fsp2, NORMAL_CLOSE);

	/* Grrr. We have to do this as open_file_ntcreate adds FILE_ATTRIBUTE_ARCHIVE when it
	   creates the file. This isn't the correct thing to do in the copy
	   case. JRA */
	if (!parent_dirname(talloc_tos(), smb_fname_dst->base_name, &parent,
			    NULL)) {
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}
	file_set_dosmode(conn, smb_fname_dst, fattr, parent, false);
	TALLOC_FREE(parent);

	if (ret < (SMB_OFF_T)smb_fname_src->st.st_ex_size) {
		status = NT_STATUS_DISK_FULL;
		goto out;
	}
 out:
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3,("copy_internals: Error %s copy file %s to %s\n",
			nt_errstr(status), smb_fname_str_dbg(smb_fname_src),
			smb_fname_str_dbg(smb_fname_dst)));
	}

	return status;
}

/****************************************************************************
 Reply to a NT rename request.
****************************************************************************/

void reply_ntrename(struct smb_request *req)
{
	connection_struct *conn = req->conn;
	struct smb_filename *smb_fname_old = NULL;
	struct smb_filename *smb_fname_new = NULL;
	char *oldname = NULL;
	char *newname = NULL;
	const char *p;
	NTSTATUS status;
	bool src_has_wcard = False;
	bool dest_has_wcard = False;
	uint32 attrs;
	uint32_t ucf_flags_src = 0;
	uint32_t ucf_flags_dst = 0;
	uint16 rename_type;
	TALLOC_CTX *ctx = talloc_tos();
	bool stream_rename = false;

	START_PROFILE(SMBntrename);

	if (req->wct < 4) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		goto out;
	}

	attrs = SVAL(req->vwv+0, 0);
	rename_type = SVAL(req->vwv+1, 0);

	p = (const char *)req->buf + 1;
	p += srvstr_get_path_req_wcard(ctx, req, &oldname, p, STR_TERMINATE,
				       &status, &src_has_wcard);
	if (!NT_STATUS_IS_OK(status)) {
		reply_nterror(req, status);
		goto out;
	}

	if (ms_has_wild(oldname)) {
		reply_nterror(req, NT_STATUS_OBJECT_PATH_SYNTAX_BAD);
		goto out;
	}

	p++;
	p += srvstr_get_path_req_wcard(ctx, req, &newname, p, STR_TERMINATE,
				       &status, &dest_has_wcard);
	if (!NT_STATUS_IS_OK(status)) {
		reply_nterror(req, status);
		goto out;
	}

	if (!lp_posix_pathnames()) {
		/* The newname must begin with a ':' if the
		   oldname contains a ':'. */
		if (strchr_m(oldname, ':')) {
			if (newname[0] != ':') {
				reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
				goto out;
			}
			stream_rename = true;
		}
	}

	/*
	 * If this is a rename operation, allow wildcards and save the
	 * destination's last component.
	 */
	if (rename_type == RENAME_FLAG_RENAME) {
		ucf_flags_src = UCF_COND_ALLOW_WCARD_LCOMP;
		ucf_flags_dst = UCF_COND_ALLOW_WCARD_LCOMP | UCF_SAVE_LCOMP;
	}

	/* rename_internals() calls unix_convert(), so don't call it here. */
	status = filename_convert(ctx, conn,
				  req->flags2 & FLAGS2_DFS_PATHNAMES,
				  oldname,
				  ucf_flags_src,
				  NULL,
				  &smb_fname_old);
	if (!NT_STATUS_IS_OK(status)) {
		if (NT_STATUS_EQUAL(status,
				    NT_STATUS_PATH_NOT_COVERED)) {
			reply_botherror(req,
					NT_STATUS_PATH_NOT_COVERED,
					ERRSRV, ERRbadpath);
			goto out;
		}
		reply_nterror(req, status);
		goto out;
	}

	status = filename_convert(ctx, conn,
				  req->flags2 & FLAGS2_DFS_PATHNAMES,
				  newname,
				  ucf_flags_dst,
				  &dest_has_wcard,
				  &smb_fname_new);
	if (!NT_STATUS_IS_OK(status)) {
		if (NT_STATUS_EQUAL(status,
				    NT_STATUS_PATH_NOT_COVERED)) {
			reply_botherror(req,
				        NT_STATUS_PATH_NOT_COVERED,
					ERRSRV, ERRbadpath);
			goto out;
		}
		reply_nterror(req, status);
		goto out;
	}

	if (stream_rename) {
		/* smb_fname_new must be the same as smb_fname_old. */
		TALLOC_FREE(smb_fname_new->base_name);
		smb_fname_new->base_name = talloc_strdup(smb_fname_new,
						smb_fname_old->base_name);
		if (!smb_fname_new->base_name) {
			reply_nterror(req, NT_STATUS_NO_MEMORY);
			goto out;
		}
	}

	DEBUG(3,("reply_ntrename: %s -> %s\n",
		 smb_fname_str_dbg(smb_fname_old),
		 smb_fname_str_dbg(smb_fname_new)));

	switch(rename_type) {
		case RENAME_FLAG_RENAME:
			status = rename_internals(ctx, conn, req,
						  smb_fname_old, smb_fname_new,
						  attrs, False, src_has_wcard,
						  dest_has_wcard,
						  DELETE_ACCESS);
			break;
		case RENAME_FLAG_HARD_LINK:
			if (src_has_wcard || dest_has_wcard) {
				/* No wildcards. */
				status = NT_STATUS_OBJECT_PATH_SYNTAX_BAD;
			} else {
				status = hardlink_internals(ctx, conn,
							    req,
							    false,
							    smb_fname_old,
							    smb_fname_new);
			}
			break;
		case RENAME_FLAG_COPY:
			if (src_has_wcard || dest_has_wcard) {
				/* No wildcards. */
				status = NT_STATUS_OBJECT_PATH_SYNTAX_BAD;
			} else {
				status = copy_internals(ctx, conn, req,
							smb_fname_old,
							smb_fname_new,
							attrs);
			}
			break;
		case RENAME_FLAG_MOVE_CLUSTER_INFORMATION:
			status = NT_STATUS_INVALID_PARAMETER;
			break;
		default:
			status = NT_STATUS_ACCESS_DENIED; /* Default error. */
			break;
	}

	if (!NT_STATUS_IS_OK(status)) {
		if (open_was_deferred(req->mid)) {
			/* We have re-scheduled this call. */
			goto out;
		}

		reply_nterror(req, status);
		goto out;
	}

	reply_outbuf(req, 0, 0);
 out:
	END_PROFILE(SMBntrename);
	return;
}

/****************************************************************************
 Reply to a notify change - queue the request and
 don't allow a directory to be opened.
****************************************************************************/

static void smbd_smb1_notify_reply(struct smb_request *req,
				   NTSTATUS error_code,
				   uint8_t *buf, size_t len)
{
	send_nt_replies(req->conn, req, error_code, (char *)buf, len, NULL, 0);
}

static void call_nt_transact_notify_change(connection_struct *conn,
					   struct smb_request *req,
					   uint16 **ppsetup,
					   uint32 setup_count,
					   char **ppparams,
					   uint32 parameter_count,
					   char **ppdata, uint32 data_count,
					   uint32 max_data_count,
					   uint32 max_param_count)
{
	uint16 *setup = *ppsetup;
	files_struct *fsp;
	uint32 filter;
	NTSTATUS status;
	bool recursive;

	if(setup_count < 6) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	fsp = file_fsp(req, SVAL(setup,4));
	filter = IVAL(setup, 0);
	recursive = (SVAL(setup, 6) != 0) ? True : False;

	DEBUG(3,("call_nt_transact_notify_change\n"));

	if(!fsp) {
		reply_nterror(req, NT_STATUS_INVALID_HANDLE);
		return;
	}

	{
		char *filter_string;

		if (!(filter_string = notify_filter_string(NULL, filter))) {
			reply_nterror(req,NT_STATUS_NO_MEMORY);
			return;
		}

		DEBUG(3,("call_nt_transact_notify_change: notify change "
			 "called on %s, filter = %s, recursive = %d\n",
			 fsp_str_dbg(fsp), filter_string, recursive));

		TALLOC_FREE(filter_string);
	}

	if((!fsp->is_directory) || (conn != fsp->conn)) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	if (fsp->notify == NULL) {

		status = change_notify_create(fsp, filter, recursive);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10, ("change_notify_create returned %s\n",
				   nt_errstr(status)));
			reply_nterror(req, status);
			return;
		}
	}

	if (fsp->notify->num_changes != 0) {

		/*
		 * We've got changes pending, respond immediately
		 */

		/*
		 * TODO: write a torture test to check the filtering behaviour
		 * here.
		 */

		change_notify_reply(req,
				    NT_STATUS_OK,
				    max_param_count,
				    fsp->notify,
				    smbd_smb1_notify_reply);

		/*
		 * change_notify_reply() above has independently sent its
		 * results
		 */
		return;
	}

	/*
	 * No changes pending, queue the request
	 */

	status = change_notify_add_request(req,
			max_param_count,
			filter,
			recursive, fsp,
			smbd_smb1_notify_reply);
	if (!NT_STATUS_IS_OK(status)) {
		reply_nterror(req, status);
	}
	return;
}

/****************************************************************************
 Reply to an NT transact rename command.
****************************************************************************/

static void call_nt_transact_rename(connection_struct *conn,
				    struct smb_request *req,
				    uint16 **ppsetup, uint32 setup_count,
				    char **ppparams, uint32 parameter_count,
				    char **ppdata, uint32 data_count,
				    uint32 max_data_count)
{
	char *params = *ppparams;
	char *new_name = NULL;
	files_struct *fsp = NULL;
	bool dest_has_wcard = False;
	NTSTATUS status;
	TALLOC_CTX *ctx = talloc_tos();

        if(parameter_count < 5) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	fsp = file_fsp(req, SVAL(params, 0));
	if (!check_fsp(conn, req, fsp)) {
		return;
	}
	srvstr_get_path_wcard(ctx, params, req->flags2, &new_name, params+4,
			      parameter_count - 4,
			      STR_TERMINATE, &status, &dest_has_wcard);
	if (!NT_STATUS_IS_OK(status)) {
		reply_nterror(req, status);
		return;
	}

	/*
	 * W2K3 ignores this request as the RAW-RENAME test
	 * demonstrates, so we do.
	 */
	send_nt_replies(conn, req, NT_STATUS_OK, NULL, 0, NULL, 0);

	DEBUG(3,("nt transact rename from = %s, to = %s ignored!\n",
		 fsp_str_dbg(fsp), new_name));

	return;
}

/******************************************************************************
 Fake up a completely empty SD.
*******************************************************************************/

static NTSTATUS get_null_nt_acl(TALLOC_CTX *mem_ctx, struct security_descriptor **ppsd)
{
	size_t sd_size;

	*ppsd = make_standard_sec_desc( mem_ctx, &global_sid_World, &global_sid_World, NULL, &sd_size);
	if(!*ppsd) {
		DEBUG(0,("get_null_nt_acl: Unable to malloc space for security descriptor.\n"));
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Reply to query a security descriptor.
 Callable from SMB2 and SMB2.
 If it returns NT_STATUS_BUFFER_TOO_SMALL, pdata_size is initialized with
 the required size.
****************************************************************************/

NTSTATUS smbd_do_query_security_desc(connection_struct *conn,
					TALLOC_CTX *mem_ctx,
					files_struct *fsp,
					uint32_t security_info_wanted,
					uint32_t max_data_count,
					uint8_t **ppmarshalled_sd,
					size_t *psd_size)
{
	NTSTATUS status;
	struct security_descriptor *psd = NULL;

	/*
	 * Get the permissions to return.
	 */

	if ((security_info_wanted & SECINFO_SACL) &&
			!(fsp->access_mask & SEC_FLAG_SYSTEM_SECURITY)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if ((security_info_wanted & (SECINFO_DACL|SECINFO_OWNER|SECINFO_GROUP)) &&
			!(fsp->access_mask & SEC_STD_READ_CONTROL)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (S_ISLNK(fsp->fsp_name->st.st_ex_mode)) {
		DEBUG(10, ("ACL get on symlink %s denied.\n",
			fsp_str_dbg(fsp)));
		return NT_STATUS_ACCESS_DENIED;
	}

	if (security_info_wanted & (SECINFO_DACL|SECINFO_OWNER|
			SECINFO_GROUP|SECINFO_SACL)) {
		/* Don't return SECINFO_LABEL if anything else was
		   requested. See bug #8458. */
		security_info_wanted &= ~SECINFO_LABEL;
	}

	if (!lp_nt_acl_support(SNUM(conn))) {
		status = get_null_nt_acl(mem_ctx, &psd);
	} else if (security_info_wanted & SECINFO_LABEL) {
		/* Like W2K3 return a null object. */
		status = get_null_nt_acl(mem_ctx, &psd);
	} else {
		status = SMB_VFS_FGET_NT_ACL(
			fsp, security_info_wanted, &psd);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!(security_info_wanted & SECINFO_OWNER)) {
		psd->owner_sid = NULL;
	}
	if (!(security_info_wanted & SECINFO_GROUP)) {
		psd->group_sid = NULL;
	}
	if (!(security_info_wanted & SECINFO_DACL)) {
		psd->type &= ~SEC_DESC_DACL_PRESENT;
		psd->dacl = NULL;
	}
	if (!(security_info_wanted & SECINFO_SACL)) {
		psd->type &= ~SEC_DESC_SACL_PRESENT;
		psd->sacl = NULL;
	}

	/* If the SACL/DACL is NULL, but was requested, we mark that it is
	 * present in the reply to match Windows behavior */
	if (psd->sacl == NULL &&
	    security_info_wanted & SECINFO_SACL)
		psd->type |= SEC_DESC_SACL_PRESENT;
	if (psd->dacl == NULL &&
	    security_info_wanted & SECINFO_DACL)
		psd->type |= SEC_DESC_DACL_PRESENT;

	if (security_info_wanted & SECINFO_LABEL) {
		/* Like W2K3 return a null object. */
		psd->owner_sid = NULL;
		psd->group_sid = NULL;
		psd->dacl = NULL;
		psd->sacl = NULL;
		psd->type &= ~(SEC_DESC_DACL_PRESENT|SEC_DESC_SACL_PRESENT);
	}

	*psd_size = ndr_size_security_descriptor(psd, 0);

	DEBUG(3,("smbd_do_query_security_desc: sd_size = %lu.\n",
		(unsigned long)*psd_size));

	if (DEBUGLEVEL >= 10) {
		DEBUG(10,("smbd_do_query_security_desc for file %s\n",
			  fsp_str_dbg(fsp)));
		NDR_PRINT_DEBUG(security_descriptor, psd);
	}

	if (max_data_count < *psd_size) {
		TALLOC_FREE(psd);
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	status = marshall_sec_desc(mem_ctx, psd,
				   ppmarshalled_sd, psd_size);

	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(psd);
		return status;
	}

	TALLOC_FREE(psd);
	return NT_STATUS_OK;
}

/****************************************************************************
 SMB1 reply to query a security descriptor.
****************************************************************************/

static void call_nt_transact_query_security_desc(connection_struct *conn,
						 struct smb_request *req,
						 uint16 **ppsetup,
						 uint32 setup_count,
						 char **ppparams,
						 uint32 parameter_count,
						 char **ppdata,
						 uint32 data_count,
						 uint32 max_data_count)
{
	char *params = *ppparams;
	char *data = *ppdata;
	size_t sd_size = 0;
	uint32 security_info_wanted;
	files_struct *fsp = NULL;
	NTSTATUS status;
	uint8_t *marshalled_sd = NULL;

        if(parameter_count < 8) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	fsp = file_fsp(req, SVAL(params,0));
	if(!fsp) {
		reply_nterror(req, NT_STATUS_INVALID_HANDLE);
		return;
	}

	security_info_wanted = IVAL(params,4);

	DEBUG(3,("call_nt_transact_query_security_desc: file = %s, "
		 "info_wanted = 0x%x\n", fsp_str_dbg(fsp),
		 (unsigned int)security_info_wanted));

	params = nttrans_realloc(ppparams, 4);
	if(params == NULL) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}

	/*
	 * Get the permissions to return.
	 */

	status = smbd_do_query_security_desc(conn,
					talloc_tos(),
					fsp,
					security_info_wanted,
					max_data_count,
					&marshalled_sd,
					&sd_size);

	if (NT_STATUS_EQUAL(status, NT_STATUS_BUFFER_TOO_SMALL)) {
		SIVAL(params,0,(uint32_t)sd_size);
		send_nt_replies(conn, req, NT_STATUS_BUFFER_TOO_SMALL,
			params, 4, NULL, 0);
		return;
        }

	if (!NT_STATUS_IS_OK(status)) {
		reply_nterror(req, status);
		return;
	}

	SMB_ASSERT(sd_size > 0);

	SIVAL(params,0,(uint32_t)sd_size);

	if (max_data_count < sd_size) {
		send_nt_replies(conn, req, NT_STATUS_BUFFER_TOO_SMALL,
				params, 4, NULL, 0);
		return;
	}

	/*
	 * Allocate the data we will return.
	 */

	data = nttrans_realloc(ppdata, sd_size);
	if(data == NULL) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}

	memcpy(data, marshalled_sd, sd_size);

	send_nt_replies(conn, req, NT_STATUS_OK, params, 4, data, (int)sd_size);

	return;
}

/****************************************************************************
 Reply to set a security descriptor. Map to UNIX perms or POSIX ACLs.
****************************************************************************/

static void call_nt_transact_set_security_desc(connection_struct *conn,
					       struct smb_request *req,
					       uint16 **ppsetup,
					       uint32 setup_count,
					       char **ppparams,
					       uint32 parameter_count,
					       char **ppdata,
					       uint32 data_count,
					       uint32 max_data_count)
{
	char *params= *ppparams;
	char *data = *ppdata;
	files_struct *fsp = NULL;
	uint32 security_info_sent = 0;
	NTSTATUS status;

	if(parameter_count < 8) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	if((fsp = file_fsp(req, SVAL(params,0))) == NULL) {
		reply_nterror(req, NT_STATUS_INVALID_HANDLE);
		return;
	}

	if (!CAN_WRITE(fsp->conn)) {
		reply_nterror(req, NT_STATUS_ACCESS_DENIED);
		return;
	}

	if(!lp_nt_acl_support(SNUM(conn))) {
		goto done;
	}

	security_info_sent = IVAL(params,4);

	DEBUG(3,("call_nt_transact_set_security_desc: file = %s, sent 0x%x\n",
		 fsp_str_dbg(fsp), (unsigned int)security_info_sent));

	if (data_count == 0) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	status = set_sd_blob(fsp, (uint8 *)data, data_count, security_info_sent);

	if (!NT_STATUS_IS_OK(status)) {
		reply_nterror(req, status);
		return;
	}

  done:
	send_nt_replies(conn, req, NT_STATUS_OK, NULL, 0, NULL, 0);
	return;
}

/*
 * Implement the default fsctl operation.
 */

static bool vfswrap_logged_ioctl_message = false;

/*
 * In 3.6 we do not have a SMB_VFS_FSCTL() function
 * it's just faked to make it more look like
 * master (4.0)
 */
NTSTATUS smb_fsctl(struct files_struct *fsp,
		       TALLOC_CTX *ctx,
		       uint32_t function,
		       uint16_t req_flags,  /* Needed for UNICODE ... */
		       const uint8_t *_in_data,
		       uint32_t in_len,
		       uint8_t **_out_data,
		       uint32_t max_out_len,
		       uint32_t *out_len)
{
	const char *in_data = (const char *)_in_data;
	char **out_data = (char **)_out_data;

	switch (function) {
	case FSCTL_SET_SPARSE:
	{
		bool set_sparse = true;
		NTSTATUS status;

		if (in_len >= 1 && in_data[0] == 0) {
			set_sparse = false;
		}

		status = file_set_sparse(fsp->conn, fsp, set_sparse);

		DEBUG(NT_STATUS_IS_OK(status) ? 10 : 9,
		      ("FSCTL_SET_SPARSE: fname[%s] set[%u] - %s\n",
		       smb_fname_str_dbg(fsp->fsp_name), set_sparse,
		       nt_errstr(status)));

		return status;
	}

	case FSCTL_CREATE_OR_GET_OBJECT_ID:
	{
		unsigned char objid[16];
		char *return_data = NULL;

		/* This should return the object-id on this file.
		 * I think I'll make this be the inode+dev. JRA.
		 */

		DEBUG(10,("FSCTL_CREATE_OR_GET_OBJECT_ID: called on FID[0x%04X]\n",fsp->fnum));

		*out_len = (max_out_len >= 64) ? 64 : max_out_len;
		/* Hmmm, will this cause problems if less data asked for? */
		return_data = talloc_array(ctx, char, 64);
		if (return_data == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		/* For backwards compatibility only store the dev/inode. */
		push_file_id_16(return_data, &fsp->file_id);
		memcpy(return_data+16,create_volume_objectid(fsp->conn,objid),16);
		push_file_id_16(return_data+32, &fsp->file_id);
		*out_data = return_data;
		return NT_STATUS_OK;
	}

	case FSCTL_GET_REPARSE_POINT:
	{
		/* Fail it with STATUS_NOT_A_REPARSE_POINT */
		DEBUG(10, ("FSCTL_GET_REPARSE_POINT: called on FID[0x%04X] Status: NOT_IMPLEMENTED\n", fsp->fnum));
		return NT_STATUS_NOT_A_REPARSE_POINT;
	}

	case FSCTL_SET_REPARSE_POINT:
	{
		/* Fail it with STATUS_NOT_A_REPARSE_POINT */
		DEBUG(10, ("FSCTL_SET_REPARSE_POINT: called on FID[0x%04X] Status: NOT_IMPLEMENTED\n", fsp->fnum));
		return NT_STATUS_NOT_A_REPARSE_POINT;
	}

	case FSCTL_GET_SHADOW_COPY_DATA:
	{
		/*
		 * This is called to retrieve the number of Shadow Copies (a.k.a. snapshots)
		 * and return their volume names.  If max_data_count is 16, then it is just
		 * asking for the number of volumes and length of the combined names.
		 *
		 * pdata is the data allocated by our caller, but that uses
		 * total_data_count (which is 0 in our case) rather than max_data_count.
		 * Allocate the correct amount and return the pointer to let
		 * it be deallocated when we return.
		 */
		struct shadow_copy_data *shadow_data = NULL;
		bool labels = False;
		uint32 labels_data_count = 0;
		uint32 i;
		char *cur_pdata = NULL;

		if (max_out_len < 16) {
			DEBUG(0,("FSCTL_GET_SHADOW_COPY_DATA: max_data_count(%u) < 16 is invalid!\n",
				max_out_len));
			return NT_STATUS_INVALID_PARAMETER;
		}

		if (max_out_len > 16) {
			labels = True;
		}

		shadow_data = talloc_zero(ctx, struct shadow_copy_data);
		if (shadow_data == NULL) {
			DEBUG(0,("TALLOC_ZERO() failed!\n"));
			return NT_STATUS_NO_MEMORY;
		}

		/*
		 * Call the VFS routine to actually do the work.
		 */
		if (SMB_VFS_GET_SHADOW_COPY_DATA(fsp, shadow_data, labels)!=0) {
			TALLOC_FREE(shadow_data);
			if (errno == ENOSYS) {
				DEBUG(5,("FSCTL_GET_SHADOW_COPY_DATA: connectpath %s, not supported.\n", 
					fsp->conn->connectpath));
				return NT_STATUS_NOT_SUPPORTED;
			} else {
				DEBUG(0,("FSCTL_GET_SHADOW_COPY_DATA: connectpath %s, failed.\n", 
					fsp->conn->connectpath));
				return NT_STATUS_UNSUCCESSFUL;
			}
		}

		labels_data_count = (shadow_data->num_volumes * 2 *
					sizeof(SHADOW_COPY_LABEL)) + 2;

		if (!labels) {
			*out_len = 16;
		} else {
			*out_len = 12 + labels_data_count;
		}

		if (max_out_len < *out_len) {
			DEBUG(0,("FSCTL_GET_SHADOW_COPY_DATA: max_data_count(%u) too small (%u) bytes needed!\n",
				max_out_len, *out_len));
			TALLOC_FREE(shadow_data);
			return NT_STATUS_BUFFER_TOO_SMALL;
		}

		cur_pdata = talloc_zero_array(ctx, char, *out_len);
		if (cur_pdata == NULL) {
			TALLOC_FREE(shadow_data);
			return NT_STATUS_NO_MEMORY;
		}

		*out_data = cur_pdata;

		/* num_volumes 4 bytes */
		SIVAL(cur_pdata, 0, shadow_data->num_volumes);

		if (labels) {
			/* num_labels 4 bytes */
			SIVAL(cur_pdata, 4, shadow_data->num_volumes);
		}

		/* needed_data_count 4 bytes */
		SIVAL(cur_pdata, 8, labels_data_count);

		cur_pdata += 12;

		DEBUG(10,("FSCTL_GET_SHADOW_COPY_DATA: %u volumes for path[%s].\n",
			  shadow_data->num_volumes, fsp_str_dbg(fsp)));
		if (labels && shadow_data->labels) {
			for (i=0; i<shadow_data->num_volumes; i++) {
				srvstr_push(cur_pdata, req_flags,
					    cur_pdata, shadow_data->labels[i],
					    2 * sizeof(SHADOW_COPY_LABEL),
					    STR_UNICODE|STR_TERMINATE);
				cur_pdata += 2 * sizeof(SHADOW_COPY_LABEL);
				DEBUGADD(10,("Label[%u]: '%s'\n",i,shadow_data->labels[i]));
			}
		}

		TALLOC_FREE(shadow_data);

		return NT_STATUS_OK;
	}

	case FSCTL_FIND_FILES_BY_SID:
	{
		/* pretend this succeeded -
		 *
		 * we have to send back a list with all files owned by this SID
		 *
		 * but I have to check that --metze
		 */
		struct dom_sid sid;
		uid_t uid;
		size_t sid_len;

		DEBUG(10,("FSCTL_FIND_FILES_BY_SID: called on FID[0x%04X]\n", fsp->fnum));

		if (in_len < 8) {
			/* NT_STATUS_BUFFER_TOO_SMALL maybe? */
			return NT_STATUS_INVALID_PARAMETER;
		}

		sid_len = MIN(in_len - 4,SID_MAX_SIZE);

		/* unknown 4 bytes: this is not the length of the sid :-(  */
		/*unknown = IVAL(pdata,0);*/

		if (!sid_parse(in_data + 4, sid_len, &sid)) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		DEBUGADD(10, ("for SID: %s\n", sid_string_dbg(&sid)));

		if (!sid_to_uid(&sid, &uid)) {
			DEBUG(0,("sid_to_uid: failed, sid[%s] sid_len[%lu]\n",
				 sid_string_dbg(&sid),
				 (unsigned long)sid_len));
			uid = (-1);
		}

		/* we can take a look at the find source :-)
		 *
		 * find ./ -uid $uid  -name '*'   is what we need here
		 *
		 *
		 * and send 4bytes len and then NULL terminated unicode strings
		 * for each file
		 *
		 * but I don't know how to deal with the paged results
		 * (maybe we can hang the result anywhere in the fsp struct)
		 *
		 * but I don't know how to deal with the paged results
		 * (maybe we can hang the result anywhere in the fsp struct)
		 *
		 * we don't send all files at once
		 * and at the next we should *not* start from the beginning,
		 * so we have to cache the result
		 *
		 * --metze
		 */

		/* this works for now... */
		return NT_STATUS_OK;
	}

	case FSCTL_QUERY_ALLOCATED_RANGES:
	{
		/* FIXME: This is just a dummy reply, telling that all of the
		 * file is allocated. MKS cp needs that.
		 * Adding the real allocated ranges via FIEMAP on Linux
		 * and SEEK_DATA/SEEK_HOLE on Solaris is needed to make
		 * this FSCTL correct for sparse files.
		 */
		NTSTATUS status;
		uint64_t offset, length;
		char *out_data_tmp = NULL;

		if (in_len != 16) {
			DEBUG(0,("FSCTL_QUERY_ALLOCATED_RANGES: data_count(%u) != 16 is invalid!\n",
				in_len));
			return NT_STATUS_INVALID_PARAMETER;
		}

		if (max_out_len < 16) {
			DEBUG(0,("FSCTL_QUERY_ALLOCATED_RANGES: max_out_len (%u) < 16 is invalid!\n",
				max_out_len));
			return NT_STATUS_INVALID_PARAMETER;
		}

		offset = BVAL(in_data,0);
		length = BVAL(in_data,8);

		if (offset + length < offset) {
			/* No 64-bit integer wrap. */
			return NT_STATUS_INVALID_PARAMETER;
		}

		/* Shouldn't this be SMB_VFS_STAT ... ? */
		status = vfs_stat_fsp(fsp);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		*out_len = 16;
		out_data_tmp = talloc_array(ctx, char, *out_len);
		if (out_data_tmp == NULL) {
			DEBUG(10, ("unable to allocate memory for response\n"));
			return NT_STATUS_NO_MEMORY;
		}

		if (offset > fsp->fsp_name->st.st_ex_size ||
				fsp->fsp_name->st.st_ex_size == 0 ||
				length == 0) {
			memset(out_data_tmp, 0, *out_len);
		} else {
			uint64_t end = offset + length;
			end = MIN(end, fsp->fsp_name->st.st_ex_size);
			SBVAL(out_data_tmp, 0, 0);
			SBVAL(out_data_tmp, 8, end);
		}

		*out_data = out_data_tmp;

		return NT_STATUS_OK;
	}

	case FSCTL_IS_VOLUME_DIRTY:
	{
		DEBUG(10,("FSCTL_IS_VOLUME_DIRTY: called on FID[0x%04X] "
			  "(but not implemented)\n", fsp->fnum));
		/*
		 * http://msdn.microsoft.com/en-us/library/cc232128%28PROT.10%29.aspx
		 * says we have to respond with NT_STATUS_INVALID_PARAMETER
		 */
		return NT_STATUS_INVALID_PARAMETER;
	}

	default:
		/*
		 * Only print once ... unfortunately there could be lots of
		 * different FSCTLs that are called.
		 */
		if (!vfswrap_logged_ioctl_message) {
			vfswrap_logged_ioctl_message = true;
			DEBUG(2, ("%s (0x%x): Currently not implemented.\n",
			__FUNCTION__, function));
		}
	}

	return NT_STATUS_NOT_SUPPORTED;
}

/****************************************************************************
 Reply to NT IOCTL
****************************************************************************/

static void call_nt_transact_ioctl(connection_struct *conn,
				   struct smb_request *req,
				   uint16 **ppsetup, uint32 setup_count,
				   char **ppparams, uint32 parameter_count,
				   char **ppdata, uint32 data_count,
				   uint32 max_data_count)
{
	NTSTATUS status;
	uint32 function;
	uint16 fidnum;
	files_struct *fsp;
	uint8 isFSctl;
	uint8 compfilter;
	char *out_data = NULL;
	uint32 out_data_len = 0;
	char *pdata = *ppdata;
	TALLOC_CTX *ctx = talloc_tos();

	if (setup_count != 8) {
		DEBUG(3,("call_nt_transact_ioctl: invalid setup count %d\n", setup_count));
		reply_nterror(req, NT_STATUS_NOT_SUPPORTED);
		return;
	}

	function = IVAL(*ppsetup, 0);
	fidnum = SVAL(*ppsetup, 4);
	isFSctl = CVAL(*ppsetup, 6);
	compfilter = CVAL(*ppsetup, 7);

	DEBUG(10, ("call_nt_transact_ioctl: function[0x%08X] FID[0x%04X] isFSctl[0x%02X] compfilter[0x%02X]\n",
		 function, fidnum, isFSctl, compfilter));

	fsp=file_fsp(req, fidnum);

	/*
	 * We don't really implement IOCTLs, especially on files.
	 */
	if (!isFSctl) {
		DEBUG(10, ("isFSctl: 0x%02X indicates IOCTL, not FSCTL!\n",
			isFSctl));
		reply_nterror(req, NT_STATUS_NOT_SUPPORTED);
		return;
	}

	/* Has to be for an open file! */
	if (!check_fsp_open(conn, req, fsp)) {
		return;
	}

	/*
	 * out_data might be allocated by the VFS module, but talloc should be
	 * used, and should be cleaned up when the request ends.
	 */
	status = smb_fsctl(fsp,
			       ctx,
			       function,
			       req->flags2,
			       (uint8_t *)pdata,
			       data_count,
			       (uint8_t **)&out_data,
			       max_data_count,
			       &out_data_len);
	if (!NT_STATUS_IS_OK(status)) {
		reply_nterror(req, status);
	} else {
		send_nt_replies(conn, req, NT_STATUS_OK, NULL, 0, out_data, out_data_len);
	}
}


#ifdef HAVE_SYS_QUOTAS
/****************************************************************************
 Reply to get user quota
****************************************************************************/

static void call_nt_transact_get_user_quota(connection_struct *conn,
					    struct smb_request *req,
					    uint16 **ppsetup,
					    uint32 setup_count,
					    char **ppparams,
					    uint32 parameter_count,
					    char **ppdata,
					    uint32 data_count,
					    uint32 max_data_count)
{
	NTSTATUS nt_status = NT_STATUS_OK;
	char *params = *ppparams;
	char *pdata = *ppdata;
	char *entry;
	int data_len=0,param_len=0;
	int qt_len=0;
	int entry_len = 0;
	files_struct *fsp = NULL;
	uint16 level = 0;
	size_t sid_len;
	struct dom_sid sid;
	bool start_enum = True;
	SMB_NTQUOTA_STRUCT qt;
	SMB_NTQUOTA_LIST *tmp_list;
	SMB_NTQUOTA_HANDLE *qt_handle = NULL;

	ZERO_STRUCT(qt);

	/* access check */
	if (get_current_uid(conn) != 0) {
		DEBUG(1,("get_user_quota: access_denied service [%s] user "
			 "[%s]\n", lp_servicename(SNUM(conn)),
			 conn->session_info->unix_name));
		reply_nterror(req, NT_STATUS_ACCESS_DENIED);
		return;
	}

	/*
	 * Ensure minimum number of parameters sent.
	 */

	if (parameter_count < 4) {
		DEBUG(0,("TRANSACT_GET_USER_QUOTA: requires %d >= 4 bytes parameters\n",parameter_count));
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	/* maybe we can check the quota_fnum */
	fsp = file_fsp(req, SVAL(params,0));
	if (!check_fsp_ntquota_handle(conn, req, fsp)) {
		DEBUG(3,("TRANSACT_GET_USER_QUOTA: no valid QUOTA HANDLE\n"));
		reply_nterror(req, NT_STATUS_INVALID_HANDLE);
		return;
	}

	/* the NULL pointer checking for fsp->fake_file_handle->pd
	 * is done by CHECK_NTQUOTA_HANDLE_OK()
	 */
	qt_handle = (SMB_NTQUOTA_HANDLE *)fsp->fake_file_handle->private_data;

	level = SVAL(params,2);

	/* unknown 12 bytes leading in params */

	switch (level) {
		case TRANSACT_GET_USER_QUOTA_LIST_CONTINUE:
			/* seems that we should continue with the enum here --metze */

			if (qt_handle->quota_list!=NULL &&
			    qt_handle->tmp_list==NULL) {

				/* free the list */
				free_ntquota_list(&(qt_handle->quota_list));

				/* Realloc the size of parameters and data we will return */
				param_len = 4;
				params = nttrans_realloc(ppparams, param_len);
				if(params == NULL) {
					reply_nterror(req, NT_STATUS_NO_MEMORY);
					return;
				}

				data_len = 0;
				SIVAL(params,0,data_len);

				break;
			}

			start_enum = False;

		case TRANSACT_GET_USER_QUOTA_LIST_START:

			if (qt_handle->quota_list==NULL &&
				qt_handle->tmp_list==NULL) {
				start_enum = True;
			}

			if (start_enum && vfs_get_user_ntquota_list(fsp,&(qt_handle->quota_list))!=0) {
				reply_nterror(req, NT_STATUS_INTERNAL_ERROR);
				return;
			}

			/* Realloc the size of parameters and data we will return */
			param_len = 4;
			params = nttrans_realloc(ppparams, param_len);
			if(params == NULL) {
				reply_nterror(req, NT_STATUS_NO_MEMORY);
				return;
			}

			/* we should not trust the value in max_data_count*/
			max_data_count = MIN(max_data_count,2048);

			pdata = nttrans_realloc(ppdata, max_data_count);/* should be max data count from client*/
			if(pdata == NULL) {
				reply_nterror(req, NT_STATUS_NO_MEMORY);
				return;
			}

			entry = pdata;

			/* set params Size of returned Quota Data 4 bytes*/
			/* but set it later when we know it */

			/* for each entry push the data */

			if (start_enum) {
				qt_handle->tmp_list = qt_handle->quota_list;
			}

			tmp_list = qt_handle->tmp_list;

			for (;((tmp_list!=NULL)&&((qt_len +40+SID_MAX_SIZE)<max_data_count));
				tmp_list=tmp_list->next,entry+=entry_len,qt_len+=entry_len) {

				sid_len = ndr_size_dom_sid(
					&tmp_list->quotas->sid, 0);
				entry_len = 40 + sid_len;

				/* nextoffset entry 4 bytes */
				SIVAL(entry,0,entry_len);

				/* then the len of the SID 4 bytes */
				SIVAL(entry,4,sid_len);

				/* unknown data 8 bytes uint64_t */
				SBIG_UINT(entry,8,(uint64_t)0); /* this is not 0 in windows...-metze*/

				/* the used disk space 8 bytes uint64_t */
				SBIG_UINT(entry,16,tmp_list->quotas->usedspace);

				/* the soft quotas 8 bytes uint64_t */
				SBIG_UINT(entry,24,tmp_list->quotas->softlim);

				/* the hard quotas 8 bytes uint64_t */
				SBIG_UINT(entry,32,tmp_list->quotas->hardlim);

				/* and now the SID */
				sid_linearize(entry+40, sid_len, &tmp_list->quotas->sid);
			}

			qt_handle->tmp_list = tmp_list;

			/* overwrite the offset of the last entry */
			SIVAL(entry-entry_len,0,0);

			data_len = 4+qt_len;
			/* overwrite the params quota_data_len */
			SIVAL(params,0,data_len);

			break;

		case TRANSACT_GET_USER_QUOTA_FOR_SID:

			/* unknown 4 bytes IVAL(pdata,0) */

			if (data_count < 8) {
				DEBUG(0,("TRANSACT_GET_USER_QUOTA_FOR_SID: requires %d >= %d bytes data\n",data_count,8));
				reply_nterror(req, NT_STATUS_INVALID_LEVEL);
				return;
			}

			sid_len = IVAL(pdata,4);
			/* Ensure this is less than 1mb. */
			if (sid_len > (1024*1024)) {
				reply_nterror(req, NT_STATUS_NO_MEMORY);
				return;
			}

			if (data_count < 8+sid_len) {
				DEBUG(0,("TRANSACT_GET_USER_QUOTA_FOR_SID: requires %d >= %lu bytes data\n",data_count,(unsigned long)(8+sid_len)));
				reply_nterror(req, NT_STATUS_INVALID_LEVEL);
				return;
			}

			data_len = 4+40+sid_len;

			if (max_data_count < data_len) {
				DEBUG(0,("TRANSACT_GET_USER_QUOTA_FOR_SID: max_data_count(%d) < data_len(%d)\n",
					max_data_count, data_len));
				param_len = 4;
				SIVAL(params,0,data_len);
				data_len = 0;
				nt_status = NT_STATUS_BUFFER_TOO_SMALL;
				break;
			}

			if (!sid_parse(pdata+8,sid_len,&sid)) {
				reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
				return;
			}

			if (vfs_get_ntquota(fsp, SMB_USER_QUOTA_TYPE, &sid, &qt)!=0) {
				ZERO_STRUCT(qt);
				/*
				 * we have to return zero's in all fields
				 * instead of returning an error here
				 * --metze
				 */
			}

			/* Realloc the size of parameters and data we will return */
			param_len = 4;
			params = nttrans_realloc(ppparams, param_len);
			if(params == NULL) {
				reply_nterror(req, NT_STATUS_NO_MEMORY);
				return;
			}

			pdata = nttrans_realloc(ppdata, data_len);
			if(pdata == NULL) {
				reply_nterror(req, NT_STATUS_NO_MEMORY);
				return;
			}

			entry = pdata;

			/* set params Size of returned Quota Data 4 bytes*/
			SIVAL(params,0,data_len);

			/* nextoffset entry 4 bytes */
			SIVAL(entry,0,0);

			/* then the len of the SID 4 bytes */
			SIVAL(entry,4,sid_len);

			/* unknown data 8 bytes uint64_t */
			SBIG_UINT(entry,8,(uint64_t)0); /* this is not 0 in windows...-mezte*/

			/* the used disk space 8 bytes uint64_t */
			SBIG_UINT(entry,16,qt.usedspace);

			/* the soft quotas 8 bytes uint64_t */
			SBIG_UINT(entry,24,qt.softlim);

			/* the hard quotas 8 bytes uint64_t */
			SBIG_UINT(entry,32,qt.hardlim);

			/* and now the SID */
			sid_linearize(entry+40, sid_len, &sid);

			break;

		default:
			DEBUG(0,("do_nt_transact_get_user_quota: fnum %d unknown level 0x%04hX\n",fsp->fnum,level));
			reply_nterror(req, NT_STATUS_INVALID_LEVEL);
			return;
			break;
	}

	send_nt_replies(conn, req, nt_status, params, param_len,
			pdata, data_len);
}

/****************************************************************************
 Reply to set user quota
****************************************************************************/

static void call_nt_transact_set_user_quota(connection_struct *conn,
					    struct smb_request *req,
					    uint16 **ppsetup,
					    uint32 setup_count,
					    char **ppparams,
					    uint32 parameter_count,
					    char **ppdata,
					    uint32 data_count,
					    uint32 max_data_count)
{
	char *params = *ppparams;
	char *pdata = *ppdata;
	int data_len=0,param_len=0;
	SMB_NTQUOTA_STRUCT qt;
	size_t sid_len;
	struct dom_sid sid;
	files_struct *fsp = NULL;

	ZERO_STRUCT(qt);

	/* access check */
	if (get_current_uid(conn) != 0) {
		DEBUG(1,("set_user_quota: access_denied service [%s] user "
			 "[%s]\n", lp_servicename(SNUM(conn)),
			 conn->session_info->unix_name));
		reply_nterror(req, NT_STATUS_ACCESS_DENIED);
		return;
	}

	/*
	 * Ensure minimum number of parameters sent.
	 */

	if (parameter_count < 2) {
		DEBUG(0,("TRANSACT_SET_USER_QUOTA: requires %d >= 2 bytes parameters\n",parameter_count));
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	/* maybe we can check the quota_fnum */
	fsp = file_fsp(req, SVAL(params,0));
	if (!check_fsp_ntquota_handle(conn, req, fsp)) {
		DEBUG(3,("TRANSACT_GET_USER_QUOTA: no valid QUOTA HANDLE\n"));
		reply_nterror(req, NT_STATUS_INVALID_HANDLE);
		return;
	}

	if (data_count < 40) {
		DEBUG(0,("TRANSACT_SET_USER_QUOTA: requires %d >= %d bytes data\n",data_count,40));
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	/* offset to next quota record.
	 * 4 bytes IVAL(pdata,0)
	 * unused here...
	 */

	/* sid len */
	sid_len = IVAL(pdata,4);

	if (data_count < 40+sid_len || (40+sid_len < sid_len)) {
		DEBUG(0,("TRANSACT_SET_USER_QUOTA: requires %d >= %lu bytes data\n",data_count,(unsigned long)40+sid_len));
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	/* unknown 8 bytes in pdata
	 * maybe its the change time in NTTIME
	 */

	/* the used space 8 bytes (uint64_t)*/
	qt.usedspace = BVAL(pdata,16);

	/* the soft quotas 8 bytes (uint64_t)*/
	qt.softlim = BVAL(pdata,24);

	/* the hard quotas 8 bytes (uint64_t)*/
	qt.hardlim = BVAL(pdata,32);

	if (!sid_parse(pdata+40,sid_len,&sid)) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	DEBUGADD(8,("SID: %s\n", sid_string_dbg(&sid)));

	/* 44 unknown bytes left... */

	if (vfs_set_ntquota(fsp, SMB_USER_QUOTA_TYPE, &sid, &qt)!=0) {
		reply_nterror(req, NT_STATUS_INTERNAL_ERROR);
		return;
	}

	send_nt_replies(conn, req, NT_STATUS_OK, params, param_len,
			pdata, data_len);
}
#endif /* HAVE_SYS_QUOTAS */

static void handle_nttrans(connection_struct *conn,
			   struct trans_state *state,
			   struct smb_request *req)
{
	if (get_Protocol() >= PROTOCOL_NT1) {
		req->flags2 |= 0x40; /* IS_LONG_NAME */
		SSVAL(req->inbuf,smb_flg2,req->flags2);
	}


	SMB_PERFCOUNT_SET_SUBOP(&req->pcd, state->call);

	/* Now we must call the relevant NT_TRANS function */
	switch(state->call) {
		case NT_TRANSACT_CREATE:
		{
			START_PROFILE(NT_transact_create);
			call_nt_transact_create(
				conn, req,
				&state->setup, state->setup_count,
				&state->param, state->total_param,
				&state->data, state->total_data,
				state->max_data_return);
			END_PROFILE(NT_transact_create);
			break;
		}

		case NT_TRANSACT_IOCTL:
		{
			START_PROFILE(NT_transact_ioctl);
			call_nt_transact_ioctl(
				conn, req,
				&state->setup, state->setup_count,
				&state->param, state->total_param,
				&state->data, state->total_data,
				state->max_data_return);
			END_PROFILE(NT_transact_ioctl);
			break;
		}

		case NT_TRANSACT_SET_SECURITY_DESC:
		{
			START_PROFILE(NT_transact_set_security_desc);
			call_nt_transact_set_security_desc(
				conn, req,
				&state->setup, state->setup_count,
				&state->param, state->total_param,
				&state->data, state->total_data,
				state->max_data_return);
			END_PROFILE(NT_transact_set_security_desc);
			break;
		}

		case NT_TRANSACT_NOTIFY_CHANGE:
		{
			START_PROFILE(NT_transact_notify_change);
			call_nt_transact_notify_change(
				conn, req,
				&state->setup, state->setup_count,
				&state->param, state->total_param,
				&state->data, state->total_data,
				state->max_data_return,
				state->max_param_return);
			END_PROFILE(NT_transact_notify_change);
			break;
		}

		case NT_TRANSACT_RENAME:
		{
			START_PROFILE(NT_transact_rename);
			call_nt_transact_rename(
				conn, req,
				&state->setup, state->setup_count,
				&state->param, state->total_param,
				&state->data, state->total_data,
				state->max_data_return);
			END_PROFILE(NT_transact_rename);
			break;
		}

		case NT_TRANSACT_QUERY_SECURITY_DESC:
		{
			START_PROFILE(NT_transact_query_security_desc);
			call_nt_transact_query_security_desc(
				conn, req,
				&state->setup, state->setup_count,
				&state->param, state->total_param,
				&state->data, state->total_data,
				state->max_data_return);
			END_PROFILE(NT_transact_query_security_desc);
			break;
		}

#ifdef HAVE_SYS_QUOTAS
		case NT_TRANSACT_GET_USER_QUOTA:
		{
			START_PROFILE(NT_transact_get_user_quota);
			call_nt_transact_get_user_quota(
				conn, req,
				&state->setup, state->setup_count,
				&state->param, state->total_param,
				&state->data, state->total_data,
				state->max_data_return);
			END_PROFILE(NT_transact_get_user_quota);
			break;
		}

		case NT_TRANSACT_SET_USER_QUOTA:
		{
			START_PROFILE(NT_transact_set_user_quota);
			call_nt_transact_set_user_quota(
				conn, req,
				&state->setup, state->setup_count,
				&state->param, state->total_param,
				&state->data, state->total_data,
				state->max_data_return);
			END_PROFILE(NT_transact_set_user_quota);
			break;
		}
#endif /* HAVE_SYS_QUOTAS */

		default:
			/* Error in request */
			DEBUG(0,("handle_nttrans: Unknown request %d in "
				 "nttrans call\n", state->call));
			reply_nterror(req, NT_STATUS_INVALID_LEVEL);
			return;
	}
	return;
}

/****************************************************************************
 Reply to a SMBNTtrans.
****************************************************************************/

void reply_nttrans(struct smb_request *req)
{
	connection_struct *conn = req->conn;
	uint32_t pscnt;
	uint32_t psoff;
	uint32_t dscnt;
	uint32_t dsoff;
	uint16 function_code;
	NTSTATUS result;
	struct trans_state *state;

	START_PROFILE(SMBnttrans);

	if (req->wct < 19) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		END_PROFILE(SMBnttrans);
		return;
	}

	pscnt = IVAL(req->vwv+9, 1);
	psoff = IVAL(req->vwv+11, 1);
	dscnt = IVAL(req->vwv+13, 1);
	dsoff = IVAL(req->vwv+15, 1);
	function_code = SVAL(req->vwv+18, 0);

	if (IS_IPC(conn) && (function_code != NT_TRANSACT_CREATE)) {
		reply_nterror(req, NT_STATUS_ACCESS_DENIED);
		END_PROFILE(SMBnttrans);
		return;
	}

	result = allow_new_trans(conn->pending_trans, req->mid);
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(2, ("Got invalid nttrans request: %s\n", nt_errstr(result)));
		reply_nterror(req, result);
		END_PROFILE(SMBnttrans);
		return;
	}

	if ((state = TALLOC_P(conn, struct trans_state)) == NULL) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		END_PROFILE(SMBnttrans);
		return;
	}

	state->cmd = SMBnttrans;

	state->mid = req->mid;
	state->vuid = req->vuid;
	state->total_data = IVAL(req->vwv+3, 1);
	state->data = NULL;
	state->total_param = IVAL(req->vwv+1, 1);
	state->param = NULL;
	state->max_data_return = IVAL(req->vwv+7, 1);
	state->max_param_return = IVAL(req->vwv+5, 1);

	/* setup count is in *words* */
	state->setup_count = 2*CVAL(req->vwv+17, 1);
	state->setup = NULL;
	state->call = function_code;

	DEBUG(10, ("num_setup=%u, "
		   "param_total=%u, this_param=%u, max_param=%u, "
		   "data_total=%u, this_data=%u, max_data=%u, "
		   "param_offset=%u, data_offset=%u\n",
		   (unsigned)state->setup_count,
		   (unsigned)state->total_param, (unsigned)pscnt,
		   (unsigned)state->max_param_return,
		   (unsigned)state->total_data, (unsigned)dscnt,
		   (unsigned)state->max_data_return,
		   (unsigned)psoff, (unsigned)dsoff));

	/*
	 * All nttrans messages we handle have smb_wct == 19 +
	 * state->setup_count.  Ensure this is so as a sanity check.
	 */

	if(req->wct != 19 + (state->setup_count/2)) {
		DEBUG(2,("Invalid smb_wct %d in nttrans call (should be %d)\n",
			 req->wct, 19 + (state->setup_count/2)));
		goto bad_param;
	}

	/* Don't allow more than 128mb for each value. */
	if ((state->total_data > (1024*1024*128)) ||
	    (state->total_param > (1024*1024*128))) {
		reply_nterror(req, NT_STATUS_NO_MEMORY);
		END_PROFILE(SMBnttrans);
		return;
	}

	if ((dscnt > state->total_data) || (pscnt > state->total_param))
		goto bad_param;

	if (state->total_data)  {

		if (trans_oob(state->total_data, 0, dscnt)
		    || trans_oob(smb_len(req->inbuf), dsoff, dscnt)) {
			goto bad_param;
		}

		/* Can't use talloc here, the core routines do realloc on the
		 * params and data. */
		if ((state->data = (char *)SMB_MALLOC(state->total_data)) == NULL) {
			DEBUG(0,("reply_nttrans: data malloc fail for %u "
				 "bytes !\n", (unsigned int)state->total_data));
			TALLOC_FREE(state);
			reply_nterror(req, NT_STATUS_NO_MEMORY);
			END_PROFILE(SMBnttrans);
			return;
		}

		memcpy(state->data,smb_base(req->inbuf)+dsoff,dscnt);
	}

	if (state->total_param) {

		if (trans_oob(state->total_param, 0, pscnt)
		    || trans_oob(smb_len(req->inbuf), psoff, pscnt)) {
			goto bad_param;
		}

		/* Can't use talloc here, the core routines do realloc on the
		 * params and data. */
		if ((state->param = (char *)SMB_MALLOC(state->total_param)) == NULL) {
			DEBUG(0,("reply_nttrans: param malloc fail for %u "
				 "bytes !\n", (unsigned int)state->total_param));
			SAFE_FREE(state->data);
			TALLOC_FREE(state);
			reply_nterror(req, NT_STATUS_NO_MEMORY);
			END_PROFILE(SMBnttrans);
			return;
		}

		memcpy(state->param,smb_base(req->inbuf)+psoff,pscnt);
	}

	state->received_data  = dscnt;
	state->received_param = pscnt;

	if(state->setup_count > 0) {
		DEBUG(10,("reply_nttrans: state->setup_count = %d\n",
			  state->setup_count));

		/*
		 * No overflow possible here, state->setup_count is an
		 * unsigned int, being filled by a single byte from
		 * CVAL(req->vwv+13, 0) above. The cast in the comparison
		 * below is not necessary, it's here to clarify things. The
		 * validity of req->vwv and req->wct has been checked in
		 * init_smb_request already.
		 */
		if ((state->setup_count/2) + 19 > (unsigned int)req->wct) {
			goto bad_param;
		}

		state->setup = (uint16 *)TALLOC(state, state->setup_count);
		if (state->setup == NULL) {
			DEBUG(0,("reply_nttrans : Out of memory\n"));
			SAFE_FREE(state->data);
			SAFE_FREE(state->param);
			TALLOC_FREE(state);
			reply_nterror(req, NT_STATUS_NO_MEMORY);
			END_PROFILE(SMBnttrans);
			return;
		}

		memcpy(state->setup, req->vwv+19, state->setup_count);
		dump_data(10, (uint8 *)state->setup, state->setup_count);
	}

	if ((state->received_data == state->total_data) &&
	    (state->received_param == state->total_param)) {
		handle_nttrans(conn, state, req);
		SAFE_FREE(state->param);
		SAFE_FREE(state->data);
		TALLOC_FREE(state);
		END_PROFILE(SMBnttrans);
		return;
	}

	DLIST_ADD(conn->pending_trans, state);

	/* We need to send an interim response then receive the rest
	   of the parameter/data bytes */
	reply_outbuf(req, 0, 0);
	show_msg((char *)req->outbuf);
	END_PROFILE(SMBnttrans);
	return;

  bad_param:

	DEBUG(0,("reply_nttrans: invalid trans parameters\n"));
	SAFE_FREE(state->data);
	SAFE_FREE(state->param);
	TALLOC_FREE(state);
	reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
	END_PROFILE(SMBnttrans);
	return;
}

/****************************************************************************
 Reply to a SMBnttranss
 ****************************************************************************/

void reply_nttranss(struct smb_request *req)
{
	connection_struct *conn = req->conn;
	uint32_t pcnt,poff,dcnt,doff,pdisp,ddisp;
	struct trans_state *state;

	START_PROFILE(SMBnttranss);

	show_msg((char *)req->inbuf);

	/* Windows clients expect all replies to
	   an NT transact secondary (SMBnttranss 0xA1)
	   to have a command code of NT transact
	   (SMBnttrans 0xA0). See bug #8989 for details. */
	req->cmd = SMBnttrans;

	if (req->wct < 18) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		END_PROFILE(SMBnttranss);
		return;
	}

	for (state = conn->pending_trans; state != NULL;
	     state = state->next) {
		if (state->mid == req->mid) {
			break;
		}
	}

	if ((state == NULL) || (state->cmd != SMBnttrans)) {
		reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
		END_PROFILE(SMBnttranss);
		return;
	}

	/* Revise state->total_param and state->total_data in case they have
	   changed downwards */
	if (IVAL(req->vwv+1, 1) < state->total_param) {
		state->total_param = IVAL(req->vwv+1, 1);
	}
	if (IVAL(req->vwv+3, 1) < state->total_data) {
		state->total_data = IVAL(req->vwv+3, 1);
	}

	pcnt = IVAL(req->vwv+5, 1);
	poff = IVAL(req->vwv+7, 1);
	pdisp = IVAL(req->vwv+9, 1);

	dcnt = IVAL(req->vwv+11, 1);
	doff = IVAL(req->vwv+13, 1);
	ddisp = IVAL(req->vwv+15, 1);

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
		memcpy(state->param+pdisp, smb_base(req->inbuf)+poff,pcnt);
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
		END_PROFILE(SMBnttranss);
		return;
	}

	handle_nttrans(conn, state, req);

	DLIST_REMOVE(conn->pending_trans, state);
	SAFE_FREE(state->data);
	SAFE_FREE(state->param);
	TALLOC_FREE(state);
	END_PROFILE(SMBnttranss);
	return;

  bad_param:

	DEBUG(0,("reply_nttranss: invalid trans parameters\n"));
	DLIST_REMOVE(conn->pending_trans, state);
	SAFE_FREE(state->data);
	SAFE_FREE(state->param);
	TALLOC_FREE(state);
	reply_nterror(req, NT_STATUS_INVALID_PARAMETER);
	END_PROFILE(SMBnttranss);
	return;
}
