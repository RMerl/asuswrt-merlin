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

static NTSTATUS smbd_smb2_close(struct smbd_smb2_request *req,
				struct files_struct *fsp,
				uint16_t in_flags,
				DATA_BLOB *outbody);

NTSTATUS smbd_smb2_request_process_close(struct smbd_smb2_request *req)
{
	const uint8_t *inbody;
	int i = req->current_idx;
	uint8_t *outhdr;
	DATA_BLOB outbody;
	uint16_t in_flags;
	uint64_t in_file_id_persistent;
	uint64_t in_file_id_volatile;
	struct files_struct *in_fsp;
	NTSTATUS status;

	status = smbd_smb2_request_verify_sizes(req, 0x18);
	if (!NT_STATUS_IS_OK(status)) {
		return smbd_smb2_request_error(req, status);
	}
	inbody = (const uint8_t *)req->in.vector[i+1].iov_base;

	outbody = data_blob_talloc(req->out.vector, NULL, 0x3C);
	if (outbody.data == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_NO_MEMORY);
	}

	in_flags		= SVAL(inbody, 0x02);
	in_file_id_persistent	= BVAL(inbody, 0x08);
	in_file_id_volatile	= BVAL(inbody, 0x10);

	in_fsp = file_fsp_smb2(req, in_file_id_persistent, in_file_id_volatile);
	if (in_fsp == NULL) {
		return smbd_smb2_request_error(req, NT_STATUS_FILE_CLOSED);
	}

	status = smbd_smb2_close(req,
				in_fsp,
				in_flags,
				&outbody);
	if (!NT_STATUS_IS_OK(status)) {
		return smbd_smb2_request_error(req, status);
	}

	outhdr = (uint8_t *)req->out.vector[i].iov_base;
	return smbd_smb2_request_done(req, outbody, NULL);
}

static NTSTATUS smbd_smb2_close(struct smbd_smb2_request *req,
				struct files_struct *fsp,
				uint16_t in_flags,
				DATA_BLOB *outbody)
{
	NTSTATUS status;
	struct smb_request *smbreq;
	connection_struct *conn = req->tcon->compat_conn;
	struct smb_filename *smb_fname = NULL;
	struct timespec mdate_ts, adate_ts, cdate_ts, create_date_ts;
	uint64_t allocation_size = 0;
	uint64_t file_size = 0;
	uint32_t dos_attrs = 0;
	uint16_t out_flags = 0;
	bool posix_open = false;

	ZERO_STRUCT(create_date_ts);
	ZERO_STRUCT(adate_ts);
	ZERO_STRUCT(mdate_ts);
	ZERO_STRUCT(cdate_ts);

	DEBUG(10,("smbd_smb2_close: %s - fnum[%d]\n",
		  fsp_str_dbg(fsp), fsp->fnum));

	smbreq = smbd_smb2_fake_smb_request(req);
	if (smbreq == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	posix_open = fsp->posix_open;
	status = copy_smb_filename(talloc_tos(),
				fsp->fsp_name,
				&smb_fname);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = close_file(smbreq, fsp, NORMAL_CLOSE);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5,("smbd_smb2_close: close_file[%s]: %s\n",
			 fsp_str_dbg(fsp), nt_errstr(status)));
		return status;
	}

	if (in_flags & SMB2_CLOSE_FLAGS_FULL_INFORMATION) {
		int ret;
		if (posix_open) {
			ret = SMB_VFS_LSTAT(conn, smb_fname);
		} else {
			ret = SMB_VFS_STAT(conn, smb_fname);
		}
		if (ret == 0) {
			out_flags = SMB2_CLOSE_FLAGS_FULL_INFORMATION;
			dos_attrs = dos_mode(conn, smb_fname);
			mdate_ts = smb_fname->st.st_ex_mtime;
			adate_ts = smb_fname->st.st_ex_atime;
			create_date_ts = get_create_timespec(conn, NULL, smb_fname);
			cdate_ts = get_change_timespec(conn, NULL, smb_fname);

			if (lp_dos_filetime_resolution(SNUM(conn))) {
				dos_filetime_timespec(&create_date_ts);
				dos_filetime_timespec(&mdate_ts);
				dos_filetime_timespec(&adate_ts);
				dos_filetime_timespec(&cdate_ts);
			}
			if (!(dos_attrs & FILE_ATTRIBUTE_DIRECTORY)) {
				file_size = get_file_size_stat(&smb_fname->st);
			}

			allocation_size = SMB_VFS_GET_ALLOC_SIZE(conn, NULL, &smb_fname->st);
		}
	}

	SSVAL(outbody->data, 0x00, 0x3C);	/* struct size */
	SSVAL(outbody->data, 0x02, out_flags);	/* flags */
	SIVAL(outbody->data, 0x04, 0);		/* reserved */
	put_long_date_timespec(conn->ts_res,
		(char *)&outbody->data[0x8],create_date_ts); /* creation time */
	put_long_date_timespec(conn->ts_res,
		(char *)&outbody->data[0x10],adate_ts); /* last access time */
	put_long_date_timespec(conn->ts_res,
		(char *)&outbody->data[0x18],mdate_ts); /* last write time */
	put_long_date_timespec(conn->ts_res,
		(char *)&outbody->data[0x20],cdate_ts); /* change time */
	SBVAL(outbody->data, 0x28, allocation_size);/* allocation size */
	SBVAL(outbody->data, 0x30, file_size);	/* end of file */
	SIVAL(outbody->data, 0x38, dos_attrs);	/* file attributes */

	return NT_STATUS_OK;
}
