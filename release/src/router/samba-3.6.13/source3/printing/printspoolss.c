/*
   Unix SMB/CIFS implementation.
   Printing routines that bridge to spoolss
   Copyright (C) Simo Sorce 2010

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
#include "printing.h"
#include "rpc_client/rpc_client.h"
#include "../librpc/gen_ndr/ndr_spoolss_c.h"
#include "rpc_server/rpc_ncacn_np.h"
#include "smbd/globals.h"
#include "../libcli/security/security.h"

void print_spool_terminate(struct connection_struct *conn,
			   struct print_file_data *print_file);

/***************************************************************************
 * Open a Document over spoolss
 ***************************************************************************/

#define DOCNAME_DEFAULT "Remote Downlevel Document"
#ifndef PRINT_SPOOL_PREFIX
#define PRINT_SPOOL_PREFIX "smbprn."
#endif

NTSTATUS print_spool_open(files_struct *fsp,
			  const char *fname,
			  uint16_t current_vuid)
{
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx;
	struct print_file_data *pf;
	struct dcerpc_binding_handle *b = NULL;
	struct spoolss_DevmodeContainer devmode_ctr;
	union spoolss_DocumentInfo info;
	int fd = -1;
	WERROR werr;

	tmp_ctx = talloc_new(fsp);
	if (!tmp_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	pf = talloc_zero(fsp, struct print_file_data);
	if (!pf) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}
	pf->svcname = talloc_strdup(pf, lp_servicename(SNUM(fsp->conn)));

	/* the document name is derived from the file name.
	 * "Remote Downlevel Document" is added in front to
	 * mimic what windows does in this case */
	pf->docname = talloc_strdup(pf, DOCNAME_DEFAULT);
	if (!pf->docname) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}
	if (fname) {
		const char *p = strrchr(fname, '/');
		if (!p) {
			p = fname;
		}
		pf->docname = talloc_asprintf_append(pf->docname, " %s", p);
		if (!pf->docname) {
			status = NT_STATUS_NO_MEMORY;
			goto done;
		}
	}

	/*
	 * Ok, now we have to open an actual file.
	 * Here is the reason:
	 * We want to write the spool job to this file in
	 * smbd for scalability reason (and also because
	 * apparently window printer drivers can seek when
	 * spooling to a file).
	 * So we first create a file, and then we pass it
	 * to spoolss in output_file so it can monitor and
	 * take over once we call EndDocPrinter().
	 * Of course we will not start writing until
	 * StartDocPrinter() actually gives the ok.
	 * smbd spooler files do not include a print jobid
	 * path component, as the jobid is only known after
	 * calling StartDocPrinter().
	 */

	pf->filename = talloc_asprintf(pf, "%s/%sXXXXXX",
					lp_pathname(SNUM(fsp->conn)),
					PRINT_SPOOL_PREFIX);
	if (!pf->filename) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}
	errno = 0;
	fd = mkstemp(pf->filename);
	if (fd == -1) {
		if (errno == EACCES) {
			/* Common setup error, force a report. */
			DEBUG(0, ("Insufficient permissions "
				  "to open spool file %s.\n",
				  pf->filename));
		} else {
			/* Normal case, report at level 3 and above. */
			DEBUG(3, ("can't open spool file %s,\n",
				  pf->filename));
			DEBUGADD(3, ("errno = %d (%s).\n",
				     errno, strerror(errno)));
		}
		status = map_nt_error_from_unix(errno);
		goto done;
	}

	/* now open a document over spoolss so that it does
	 * all printer verification, and eventually assigns
	 * a job id */

	status = rpc_pipe_open_interface(fsp->conn,
					 &ndr_table_spoolss.syntax_id,
					 fsp->conn->session_info,
					 &fsp->conn->sconn->client_id,
					 fsp->conn->sconn->msg_ctx,
					 &fsp->conn->spoolss_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	b = fsp->conn->spoolss_pipe->binding_handle;

	ZERO_STRUCT(devmode_ctr);

	status = dcerpc_spoolss_OpenPrinter(b, pf, pf->svcname,
					    "RAW", devmode_ctr,
					    PRINTER_ACCESS_USE,
					    &pf->handle, &werr);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!W_ERROR_IS_OK(werr)) {
		status = werror_to_ntstatus(werr);
		goto done;
	}

	info.info1 = talloc(tmp_ctx, struct spoolss_DocumentInfo1);
	if (!info.info1) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}
	info.info1->document_name = pf->docname;
	info.info1->output_file = pf->filename;
	info.info1->datatype = "RAW";

	status = dcerpc_spoolss_StartDocPrinter(b, tmp_ctx, &pf->handle,
						1, info, &pf->jobid, &werr);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!W_ERROR_IS_OK(werr)) {
		status = werror_to_ntstatus(werr);
		goto done;
	}

	/* Convert to RAP id. */
	pf->rap_jobid = pjobid_to_rap(pf->svcname, pf->jobid);
	if (pf->rap_jobid == 0) {
		/* No errno around here */
		status = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	/* setup a full fsp */
	status = create_synthetic_smb_fname(fsp, pf->filename, NULL,
					    NULL, &fsp->fsp_name);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (sys_fstat(fd, &fsp->fsp_name->st, false) != 0) {
		status = map_nt_error_from_unix(errno);
		goto done;
	}

	fsp->file_id = vfs_file_id_from_sbuf(fsp->conn, &fsp->fsp_name->st);
	fsp->mode = fsp->fsp_name->st.st_ex_mode;
	fsp->fh->fd = fd;

	fsp->vuid = current_vuid;
	fsp->can_lock = false;
	fsp->can_read = false;
	fsp->access_mask = FILE_GENERIC_WRITE;
	fsp->can_write = true;
	fsp->modified = false;
	fsp->oplock_type = NO_OPLOCK;
	fsp->sent_oplock_break = NO_BREAK_SENT;
	fsp->is_directory = false;

	fsp->print_file = pf;

	status = NT_STATUS_OK;
done:
	if (!NT_STATUS_IS_OK(status)) {
		if (fd != -1) {
			close(fd);
			if (fsp->print_file) {
				unlink(fsp->print_file->filename);
			}
		}
		/* We need to delete the job from spoolss too */
		if (pf->jobid) {
			print_spool_terminate(fsp->conn, pf);
		}
	}
	talloc_free(tmp_ctx);
	return status;
}

int print_spool_write(files_struct *fsp,
		      const char *data, uint32_t size,
		      SMB_OFF_T offset, uint32_t *written)
{
	SMB_STRUCT_STAT st;
	ssize_t n;
	int ret;

	*written = 0;

	/* first of all stat file to find out if it is still there.
	 * spoolss may have deleted it to signal someone has killed
	 * the job through it's interface */

	if (sys_fstat(fsp->fh->fd, &st, false) != 0) {
		ret = errno;
		DEBUG(3, ("printfile_offset: sys_fstat failed on %s (%s)\n",
			  fsp_str_dbg(fsp), strerror(ret)));
		return ret;
	}

	/* check if the file is unlinked, this will signal spoolss has
	 * killed it, just return an error and close the file */
	if (st.st_ex_nlink == 0) {
		close(fsp->fh->fd);
		return EBADF;
	}

	/* When print files go beyond 4GB, the 32-bit offset sent in
	 * old SMBwrite calls is relative to the current 4GB chunk
	 * we're writing to.
	 *    Discovered by Sebastian Kloska <oncaphillis@snafu.de>.
	 */
	if (offset < 0xffffffff00000000LL) {
		offset = (st.st_ex_size & 0xffffffff00000000LL) + offset;
	}

	n = write_data_at_offset(fsp->fh->fd, data, size, offset);
	if (n == -1) {
		ret = errno;
		print_spool_terminate(fsp->conn, fsp->print_file);
	} else {
		*written = n;
		ret = 0;
	}

	return ret;
}

void print_spool_end(files_struct *fsp, enum file_close_type close_type)
{
	NTSTATUS status;
	WERROR werr;
	struct dcerpc_binding_handle *b = NULL;

	b = fsp->conn->spoolss_pipe->binding_handle;

	switch (close_type) {
	case NORMAL_CLOSE:
	case SHUTDOWN_CLOSE:
		/* this also automatically calls spoolss_EndDocPrinter */
		status = dcerpc_spoolss_ClosePrinter(b, fsp->print_file,
						&fsp->print_file->handle,
						&werr);
		if (!NT_STATUS_IS_OK(status) ||
		    !NT_STATUS_IS_OK(status = werror_to_ntstatus(werr))) {
			DEBUG(3, ("Failed to close printer %s [%s]\n",
			      fsp->print_file->svcname, nt_errstr(status)));
		}
		break;
	case ERROR_CLOSE:
		print_spool_terminate(fsp->conn, fsp->print_file);
		break;
	}
}


void print_spool_terminate(struct connection_struct *conn,
			   struct print_file_data *print_file)
{
	NTSTATUS status;
	WERROR werr;
	struct dcerpc_binding_handle *b = NULL;

	rap_jobid_delete(print_file->svcname, print_file->jobid);

	status = rpc_pipe_open_interface(conn,
					 &ndr_table_spoolss.syntax_id,
					 conn->session_info,
					 &conn->sconn->client_id,
					 conn->sconn->msg_ctx,
					 &conn->spoolss_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("print_spool_terminate: "
			  "Failed to get spoolss pipe [%s]\n",
			  nt_errstr(status)));
		return;
	}
	b = conn->spoolss_pipe->binding_handle;

	status = dcerpc_spoolss_SetJob(b, print_file,
					&print_file->handle,
					print_file->jobid,
					NULL, SPOOLSS_JOB_CONTROL_DELETE,
					&werr);
	if (!NT_STATUS_IS_OK(status) ||
	    !NT_STATUS_IS_OK(status = werror_to_ntstatus(werr))) {
		DEBUG(3, ("Failed to delete job %d [%s]\n",
			  print_file->jobid, nt_errstr(status)));
		return;
	}
	status = dcerpc_spoolss_ClosePrinter(b, print_file,
					     &print_file->handle,
					     &werr);
	if (!NT_STATUS_IS_OK(status) ||
	    !NT_STATUS_IS_OK(status = werror_to_ntstatus(werr))) {
		DEBUG(3, ("Failed to close printer %s [%s]\n",
			  print_file->svcname, nt_errstr(status)));
		return;
	}
}
