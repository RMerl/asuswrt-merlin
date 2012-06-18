/* 
   Unix SMB/CIFS implementation.
   printing backend routines for smbd - using files_struct rather
   than only snum
   Copyright (C) Andrew Tridgell 1992-2000
   
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

/***************************************************************************
open a print file and setup a fsp for it. This is a wrapper around
print_job_start().
***************************************************************************/

NTSTATUS print_fsp_open(struct smb_request *req, connection_struct *conn,
			const char *fname,
			uint16_t current_vuid, files_struct *fsp)
{
	int jobid;
	fstring name;
	NTSTATUS status;

	fstrcpy( name, "Remote Downlevel Document");
	if (fname) {
		const char *p = strrchr(fname, '/');
		fstrcat(name, " ");
		if (!p) {
			p = fname;
		}
		fstrcat(name, p);
	}

	jobid = print_job_start(conn->server_info, SNUM(conn), name, NULL);
	if (jobid == -1) {
		status = map_nt_error_from_unix(errno);
		return status;
	}

	/* Convert to RAP id. */
	fsp->rap_print_jobid = pjobid_to_rap(lp_const_servicename(SNUM(conn)), jobid);
	if (fsp->rap_print_jobid == 0) {
		/* We need to delete the entry in the tdb. */
		pjob_delete(lp_const_servicename(SNUM(conn)), jobid);
		return NT_STATUS_ACCESS_DENIED;	/* No errno around here */
	}

	status = create_synthetic_smb_fname(fsp,
	    print_job_fname(lp_const_servicename(SNUM(conn)), jobid), NULL,
	    NULL, &fsp->fsp_name);
	if (!NT_STATUS_IS_OK(status)) {
		pjob_delete(lp_const_servicename(SNUM(conn)), jobid);
		return status;
	}
	/* setup a full fsp */
	fsp->fh->fd = print_job_fd(lp_const_servicename(SNUM(conn)),jobid);
	GetTimeOfDay(&fsp->open_time);
	fsp->vuid = current_vuid;
	fsp->fh->pos = -1;
	fsp->can_lock = True;
	fsp->can_read = False;
	fsp->access_mask = FILE_GENERIC_WRITE;
	fsp->can_write = True;
	fsp->print_file = True;
	fsp->modified = False;
	fsp->oplock_type = NO_OPLOCK;
	fsp->sent_oplock_break = NO_BREAK_SENT;
	fsp->is_directory = False;
	fsp->wcp = NULL;
	SMB_VFS_FSTAT(fsp, &fsp->fsp_name->st);
	fsp->mode = fsp->fsp_name->st.st_ex_mode;
	fsp->file_id = vfs_file_id_from_sbuf(conn, &fsp->fsp_name->st);

	return NT_STATUS_OK;
}

/****************************************************************************
 Print a file - called on closing the file.
****************************************************************************/

void print_fsp_end(files_struct *fsp, enum file_close_type close_type)
{
	uint32 jobid;

	if (fsp->fh->private_options & FILE_DELETE_ON_CLOSE) {
		/*
		 * Truncate the job. print_job_end will take
		 * care of deleting it for us. JRA.
		 */
		sys_ftruncate(fsp->fh->fd, 0);
	}

	if (fsp->fsp_name) {
		TALLOC_FREE(fsp->fsp_name);
	}

	if (!rap_to_pjobid(fsp->rap_print_jobid, NULL, &jobid)) {
		DEBUG(3,("print_fsp_end: Unable to convert RAP jobid %u to print jobid.\n",
			(unsigned int)fsp->rap_print_jobid ));
		return;
	}

	print_job_end(SNUM(fsp->conn),jobid, close_type);
}
