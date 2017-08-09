/* 
   Unix SMB/CIFS implementation.

   a pass-thru NTVFS module to record a NBENCH load file

   Copyright (C) Andrew Tridgell 2004

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
  "passthru" in this module refers to the next level of NTVFS being used
*/

#include "includes.h"
#include "ntvfs/ntvfs.h"
#include "system/filesys.h"

/* this is stored in ntvfs_private */
struct nbench_private {
	int log_fd;
};

/*
  log one request to the nbench log
*/
static void nbench_log(struct ntvfs_request *req,
		       const char *format, ...) PRINTF_ATTRIBUTE(2, 3);

static void nbench_log(struct ntvfs_request *req,
		       const char *format, ...)
{
	struct nbench_private *nprivates = req->async_states->ntvfs->private_data;
	va_list ap;
	char *s = NULL;

	va_start(ap, format);
	vasprintf(&s, format, ap);
	va_end(ap);

	write(nprivates->log_fd, s, strlen(s));
	free(s);
}

static char *nbench_ntvfs_handle_string(struct ntvfs_request *req, struct ntvfs_handle *h)
{
	DATA_BLOB key;
	uint16_t fnum = 0;

	key = ntvfs_handle_get_wire_key(h, req);

	switch (key.length) {
	case 2: /* SMB fnum */
		fnum = SVAL(key.data, 0);
		break;
	default:
		DEBUG(0,("%s: invalid wire handle size: %u\n",
			__FUNCTION__, (unsigned)key.length));
		break;
	}

	return talloc_asprintf(req, "%u", fnum);
}

/*
  this pass through macro operates on request contexts, and disables
  async calls. 

  async calls are a pain for the nbench module as it makes pulling the
  status code and any result parameters much harder.
*/
#define PASS_THRU_REQ_PRE_ASYNC(ntvfs, req, op, par1) do { \
	status = ntvfs_async_state_push(ntvfs, req, par1, nbench_##op##_send); \
	if (!NT_STATUS_IS_OK(status)) { \
		return status; \
	} \
} while (0)

#define PASS_THRU_REQ_POST_ASYNC(req) do { \
	req->async_states->status = status; \
	if (!(req->async_states->state & NTVFS_ASYNC_STATE_ASYNC)) { \
		req->async_states->send_fn(req); \
	} \
} while (0)

#define PASS_THRU_REQ(ntvfs, req, op, par1, args) do { \
	PASS_THRU_REQ_PRE_ASYNC(ntvfs, req, op, par1); \
	status = ntvfs_next_##op args; \
	PASS_THRU_REQ_POST_ASYNC(req); \
} while (0)

#define PASS_THRU_REP_POST(req) do { \
	ntvfs_async_state_pop(req); \
	if (req->async_states->state & NTVFS_ASYNC_STATE_ASYNC) { \
		req->async_states->send_fn(req); \
	} \
} while (0)

/*
  connect to a share - used when a tree_connect operation comes in.
*/
static NTSTATUS nbench_connect(struct ntvfs_module_context *ntvfs,
			       struct ntvfs_request *req,
			       union smb_tcon* con)
{
	struct nbench_private *nprivates;
	NTSTATUS status;
	char *logname = NULL;

	nprivates = talloc(ntvfs, struct nbench_private);
	if (!nprivates) {
		return NT_STATUS_NO_MEMORY;
	}

	logname = talloc_asprintf(req, "/tmp/nbenchlog%d.%u", ntvfs->depth,
				  getpid());
	NT_STATUS_HAVE_NO_MEMORY(logname);
	nprivates->log_fd = open(logname, O_WRONLY|O_CREAT|O_APPEND, 0644);
	talloc_free(logname);

	if (nprivates->log_fd == -1) {
		DEBUG(0,("Failed to open nbench log\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	ntvfs->private_data = nprivates;

	status = ntvfs_next_connect(ntvfs, req, con);

	return status;
}

/*
  disconnect from a share
*/
static NTSTATUS nbench_disconnect(struct ntvfs_module_context *ntvfs)
{
	struct nbench_private *nprivates = ntvfs->private_data;
	NTSTATUS status;

	close(nprivates->log_fd);

	status = ntvfs_next_disconnect(ntvfs);

	return status;
}

/*
  delete a file - the dirtype specifies the file types to include in the search. 
  The name can contain CIFS wildcards, but rarely does (except with OS/2 clients)
*/
static void nbench_unlink_send(struct ntvfs_request *req)
{
	union smb_unlink *unl = req->async_states->private_data;

	nbench_log(req, "Unlink \"%s\" 0x%x %s\n", 
		   unl->unlink.in.pattern, unl->unlink.in.attrib, 
		   get_nt_error_c_code(req->async_states->status));

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_unlink(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req,
			      union smb_unlink *unl)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, unlink, unl, (ntvfs, req, unl));

	return status;
}

/*
  ioctl interface
*/
static void nbench_ioctl_send(struct ntvfs_request *req)
{
	nbench_log(req, "Ioctl - NOT HANDLED\n");

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_ioctl(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, union smb_ioctl *io)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, ioctl, io, (ntvfs, req, io));

	return status;
}

/*
  check if a directory exists
*/
static void nbench_chkpath_send(struct ntvfs_request *req)
{
	union smb_chkpath *cp = req->async_states->private_data;

	nbench_log(req, "Chkpath \"%s\" %s\n", 
		   cp->chkpath.in.path, 
		   get_nt_error_c_code(req->async_states->status));

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_chkpath(struct ntvfs_module_context *ntvfs,
			       struct ntvfs_request *req,
			       union smb_chkpath *cp)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, chkpath, cp, (ntvfs, req, cp));

	return status;
}

/*
  return info on a pathname
*/
static void nbench_qpathinfo_send(struct ntvfs_request *req)
{
	union smb_fileinfo *info = req->async_states->private_data;

	nbench_log(req, "QUERY_PATH_INFORMATION \"%s\" %d %s\n", 
		   info->generic.in.file.path, 
		   info->generic.level,
		   get_nt_error_c_code(req->async_states->status));

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_qpathinfo(struct ntvfs_module_context *ntvfs,
				 struct ntvfs_request *req, union smb_fileinfo *info)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, qpathinfo, info, (ntvfs, req, info));

	return status;
}

/*
  query info on a open file
*/
static void nbench_qfileinfo_send(struct ntvfs_request *req)
{
	union smb_fileinfo *info = req->async_states->private_data;

	nbench_log(req, "QUERY_FILE_INFORMATION %s %d %s\n", 
		   nbench_ntvfs_handle_string(req, info->generic.in.file.ntvfs),
		   info->generic.level,
		   get_nt_error_c_code(req->async_states->status));

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_qfileinfo(struct ntvfs_module_context *ntvfs,
				 struct ntvfs_request *req, union smb_fileinfo *info)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, qfileinfo, info, (ntvfs, req, info));

	return status;
}

/*
  set info on a pathname
*/
static void nbench_setpathinfo_send(struct ntvfs_request *req)
{
	union smb_setfileinfo *st = req->async_states->private_data;

	nbench_log(req, "SET_PATH_INFORMATION \"%s\" %d %s\n", 
		   st->generic.in.file.path, 
		   st->generic.level,
		   get_nt_error_c_code(req->async_states->status));

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_setpathinfo(struct ntvfs_module_context *ntvfs,
				   struct ntvfs_request *req, union smb_setfileinfo *st)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, setpathinfo, st, (ntvfs, req, st));

	return status;
}

/*
  open a file
*/
static void nbench_open_send(struct ntvfs_request *req)
{
	union smb_open *io = req->async_states->private_data;

	switch (io->generic.level) {
	case RAW_OPEN_NTCREATEX:
		if (!NT_STATUS_IS_OK(req->async_states->status)) {
			ZERO_STRUCT(io->ntcreatex.out);
		}
		nbench_log(req, "NTCreateX \"%s\" 0x%x 0x%x %s %s\n", 
			   io->ntcreatex.in.fname, 
			   io->ntcreatex.in.create_options, 
			   io->ntcreatex.in.open_disposition, 
			   nbench_ntvfs_handle_string(req, io->ntcreatex.out.file.ntvfs),
			   get_nt_error_c_code(req->async_states->status));
		break;

	default:
		nbench_log(req, "Open-%d - NOT HANDLED\n",
			   io->generic.level);
		break;
	}

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_open(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req, union smb_open *io)
{
	NTSTATUS status;

#undef open /* AIX defines open to be open64 */
	PASS_THRU_REQ(ntvfs, req, open, io, (ntvfs, req, io));

	return status;
}

/*
  create a directory
*/
static void nbench_mkdir_send(struct ntvfs_request *req)
{
	nbench_log(req, "Mkdir - NOT HANDLED\n");

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_mkdir(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, union smb_mkdir *md)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, mkdir, md, (ntvfs, req, md));

	return status;
}

/*
  remove a directory
*/
static void nbench_rmdir_send(struct ntvfs_request *req)
{
	struct smb_rmdir *rd = req->async_states->private_data;

	nbench_log(req, "Rmdir \"%s\" %s\n", 
		   rd->in.path, 
		   get_nt_error_c_code(req->async_states->status));

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_rmdir(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, struct smb_rmdir *rd)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, rmdir, rd, (ntvfs, req, rd));

	return status;
}

/*
  rename a set of files
*/
static void nbench_rename_send(struct ntvfs_request *req)
{
	union smb_rename *ren = req->async_states->private_data;

	switch (ren->generic.level) {
	case RAW_RENAME_RENAME:
		nbench_log(req, "Rename \"%s\" \"%s\" %s\n", 
			   ren->rename.in.pattern1, 
			   ren->rename.in.pattern2, 
			   get_nt_error_c_code(req->async_states->status));
		break;

	default:
		nbench_log(req, "Rename-%d - NOT HANDLED\n",
			   ren->generic.level);
		break;
	}

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_rename(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req, union smb_rename *ren)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, rename, ren, (ntvfs, req, ren));

	return status;
}

/*
  copy a set of files
*/
static void nbench_copy_send(struct ntvfs_request *req)
{
	nbench_log(req, "Copy - NOT HANDLED\n");

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_copy(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req, struct smb_copy *cp)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, copy, cp, (ntvfs, req, cp));

	return status;
}

/*
  read from a file
*/
static void nbench_read_send(struct ntvfs_request *req)
{
	union smb_read *rd = req->async_states->private_data;
	
	switch (rd->generic.level) {
	case RAW_READ_READX:
		if (!NT_STATUS_IS_OK(req->async_states->status)) {
			ZERO_STRUCT(rd->readx.out);
		}
		nbench_log(req, "ReadX %s %d %d %d %s\n", 
			   nbench_ntvfs_handle_string(req, rd->readx.in.file.ntvfs),
			   (int)rd->readx.in.offset,
			   rd->readx.in.maxcnt,
			   rd->readx.out.nread,
			   get_nt_error_c_code(req->async_states->status));
		break;
	default:
		nbench_log(req, "Read-%d - NOT HANDLED\n",
			   rd->generic.level);
		break;
	}

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_read(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req, union smb_read *rd)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, read, rd, (ntvfs, req, rd));

	return status;
}

/*
  write to a file
*/
static void nbench_write_send(struct ntvfs_request *req)
{
	union smb_write *wr = req->async_states->private_data;

	switch (wr->generic.level) {
	case RAW_WRITE_WRITEX:
		if (!NT_STATUS_IS_OK(req->async_states->status)) {
			ZERO_STRUCT(wr->writex.out);
		}
		nbench_log(req, "WriteX %s %d %d %d %s\n", 
			   nbench_ntvfs_handle_string(req, wr->writex.in.file.ntvfs),
			   (int)wr->writex.in.offset,
			   wr->writex.in.count,
			   wr->writex.out.nwritten,
			   get_nt_error_c_code(req->async_states->status));
		break;

	case RAW_WRITE_WRITE:
		if (!NT_STATUS_IS_OK(req->async_states->status)) {
			ZERO_STRUCT(wr->write.out);
		}
		nbench_log(req, "Write %s %d %d %d %s\n", 
			   nbench_ntvfs_handle_string(req, wr->write.in.file.ntvfs),
			   wr->write.in.offset,
			   wr->write.in.count,
			   wr->write.out.nwritten,
			   get_nt_error_c_code(req->async_states->status));
		break;

	default:
		nbench_log(req, "Write-%d - NOT HANDLED\n",
			   wr->generic.level);
		break;
	}

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_write(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, union smb_write *wr)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, write, wr, (ntvfs, req, wr));

	return status;
}

/*
  seek in a file
*/
static void nbench_seek_send(struct ntvfs_request *req)
{
	nbench_log(req, "Seek - NOT HANDLED\n");

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_seek(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req,
			    union smb_seek *io)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, seek, io, (ntvfs, req, io));

	return status;
}

/*
  flush a file
*/
static void nbench_flush_send(struct ntvfs_request *req)
{
	union smb_flush *io = req->async_states->private_data;

	switch (io->generic.level) {
	case RAW_FLUSH_FLUSH:
		nbench_log(req, "Flush %s %s\n",
			   nbench_ntvfs_handle_string(req, io->flush.in.file.ntvfs),
			   get_nt_error_c_code(req->async_states->status));
		break;
	case RAW_FLUSH_ALL:
		nbench_log(req, "Flush %d %s\n",
			   0xFFFF,
			   get_nt_error_c_code(req->async_states->status));
		break;
	default:
		nbench_log(req, "Flush-%d - NOT HANDLED\n",
			   io->generic.level);
		break;
	}

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_flush(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req,
			     union smb_flush *io)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, flush, io, (ntvfs, req, io));

	return status;
}

/*
  close a file
*/
static void nbench_close_send(struct ntvfs_request *req)
{
	union smb_close *io = req->async_states->private_data;

	switch (io->generic.level) {
	case RAW_CLOSE_CLOSE:
		nbench_log(req, "Close %s %s\n",
			   nbench_ntvfs_handle_string(req, io->close.in.file.ntvfs),
			   get_nt_error_c_code(req->async_states->status));
		break;

	default:
		nbench_log(req, "Close-%d - NOT HANDLED\n",
			   io->generic.level);
		break;
	}		

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_close(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, union smb_close *io)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, close, io, (ntvfs, req, io));

	return status;
}

/*
  exit - closing files
*/
static void nbench_exit_send(struct ntvfs_request *req)
{
	nbench_log(req, "Exit - NOT HANDLED\n");

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_exit(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, exit, NULL, (ntvfs, req));

	return status;
}

/*
  logoff - closing files
*/
static void nbench_logoff_send(struct ntvfs_request *req)
{
	nbench_log(req, "Logoff - NOT HANDLED\n");

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_logoff(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, logoff, NULL, (ntvfs, req));

	return status;
}

/*
  async_setup - send fn
*/
static void nbench_async_setup_send(struct ntvfs_request *req)
{
	PASS_THRU_REP_POST(req);
}

/*
  async setup
*/
static NTSTATUS nbench_async_setup(struct ntvfs_module_context *ntvfs,
				   struct ntvfs_request *req,
				   void *private_data)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, async_setup, NULL, (ntvfs, req, private_data));

	return status;
}


static void nbench_cancel_send(struct ntvfs_request *req)
{
	PASS_THRU_REP_POST(req);
}

/*
  cancel an existing async request
*/
static NTSTATUS nbench_cancel(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, cancel, NULL, (ntvfs, req));

	return status;
}

/*
  lock a byte range
*/
static void nbench_lock_send(struct ntvfs_request *req)
{
	union smb_lock *lck = req->async_states->private_data;

	if (lck->generic.level == RAW_LOCK_LOCKX &&
	    lck->lockx.in.lock_cnt == 1 &&
	    lck->lockx.in.ulock_cnt == 0) {
		nbench_log(req, "LockX %s %d %d %s\n", 
			   nbench_ntvfs_handle_string(req, lck->lockx.in.file.ntvfs),
			   (int)lck->lockx.in.locks[0].offset,
			   (int)lck->lockx.in.locks[0].count,
			   get_nt_error_c_code(req->async_states->status));
	} else if (lck->generic.level == RAW_LOCK_LOCKX &&
		   lck->lockx.in.ulock_cnt == 1) {
		nbench_log(req, "UnlockX %s %d %d %s\n", 
			   nbench_ntvfs_handle_string(req, lck->lockx.in.file.ntvfs),
			   (int)lck->lockx.in.locks[0].offset,
			   (int)lck->lockx.in.locks[0].count,
			   get_nt_error_c_code(req->async_states->status));
	} else {
		nbench_log(req, "Lock-%d - NOT HANDLED\n", lck->generic.level);
	}

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_lock(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req, union smb_lock *lck)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, lock, lck, (ntvfs, req, lck));

	return status;
}

/*
  set info on a open file
*/
static void nbench_setfileinfo_send(struct ntvfs_request *req)
{
	union smb_setfileinfo *info = req->async_states->private_data;

	nbench_log(req, "SET_FILE_INFORMATION %s %d %s\n", 
		   nbench_ntvfs_handle_string(req, info->generic.in.file.ntvfs),
		   info->generic.level,
		   get_nt_error_c_code(req->async_states->status));

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_setfileinfo(struct ntvfs_module_context *ntvfs,
				   struct ntvfs_request *req, 
				   union smb_setfileinfo *info)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, setfileinfo, info, (ntvfs, req, info));

	return status;
}

/*
  return filesystem space info
*/
static void nbench_fsinfo_send(struct ntvfs_request *req)
{
	union smb_fsinfo *fs = req->async_states->private_data;

	nbench_log(req, "QUERY_FS_INFORMATION %d %s\n", 
		   fs->generic.level, 
		   get_nt_error_c_code(req->async_states->status));

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_fsinfo(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req, union smb_fsinfo *fs)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, fsinfo, fs, (ntvfs, req, fs));

	return status;
}

/*
  return print queue info
*/
static void nbench_lpq_send(struct ntvfs_request *req)
{
	union smb_lpq *lpq = req->async_states->private_data;

	nbench_log(req, "Lpq-%d - NOT HANDLED\n", lpq->generic.level);

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_lpq(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req, union smb_lpq *lpq)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, lpq, lpq, (ntvfs, req, lpq));

	return status;
}

/* 
   list files in a directory matching a wildcard pattern
*/
static void nbench_search_first_send(struct ntvfs_request *req)
{
	union smb_search_first *io = req->async_states->private_data;
	
	switch (io->generic.level) {
	case RAW_SEARCH_TRANS2:
		if (NT_STATUS_IS_ERR(req->async_states->status)) {
			ZERO_STRUCT(io->t2ffirst.out);
		}
		nbench_log(req, "FIND_FIRST \"%s\" %d %d %d %s\n", 
			   io->t2ffirst.in.pattern,
			   io->t2ffirst.data_level,
			   io->t2ffirst.in.max_count,
			   io->t2ffirst.out.count,
			   get_nt_error_c_code(req->async_states->status));
		break;
		
	default:
		nbench_log(req, "Search-%d - NOT HANDLED\n", io->generic.level);
		break;
	}

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_search_first(struct ntvfs_module_context *ntvfs,
				    struct ntvfs_request *req, union smb_search_first *io, 
				    void *search_private, 
				    bool (*callback)(void *, const union smb_search_data *))
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, search_first, io, (ntvfs, req, io, search_private, callback));

	return status;
}

/* continue a search */
static void nbench_search_next_send(struct ntvfs_request *req)
{
	union smb_search_next *io = req->async_states->private_data;

	nbench_log(req, "Searchnext-%d - NOT HANDLED\n", io->generic.level);

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_search_next(struct ntvfs_module_context *ntvfs,
				   struct ntvfs_request *req, union smb_search_next *io, 
				   void *search_private, 
				   bool (*callback)(void *, const union smb_search_data *))
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, search_next, io, (ntvfs, req, io, search_private, callback));

	return status;
}

/* close a search */
static void nbench_search_close_send(struct ntvfs_request *req)
{
	union smb_search_close *io = req->async_states->private_data;

	nbench_log(req, "Searchclose-%d - NOT HANDLED\n", io->generic.level);

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_search_close(struct ntvfs_module_context *ntvfs,
				    struct ntvfs_request *req, union smb_search_close *io)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, search_close, io, (ntvfs, req, io));

	return status;
}

/* SMBtrans - not used on file shares */
static void nbench_trans_send(struct ntvfs_request *req)
{
	nbench_log(req, "Trans - NOT HANDLED\n");

	PASS_THRU_REP_POST(req);
}

static NTSTATUS nbench_trans(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, struct smb_trans2 *trans2)
{
	NTSTATUS status;

	PASS_THRU_REQ(ntvfs, req, trans, trans2, (ntvfs, req, trans2));

	return status;
}

/*
  initialise the nbench backend, registering ourselves with the ntvfs subsystem
 */
NTSTATUS ntvfs_nbench_init(void)
{
	NTSTATUS ret;
	struct ntvfs_ops ops;
	NTVFS_CURRENT_CRITICAL_SIZES(vers);

	ZERO_STRUCT(ops);

	/* fill in the name and type */
	ops.name = "nbench";
	ops.type = NTVFS_DISK;
	
	/* fill in all the operations */
	ops.connect = nbench_connect;
	ops.disconnect = nbench_disconnect;
	ops.unlink = nbench_unlink;
	ops.chkpath = nbench_chkpath;
	ops.qpathinfo = nbench_qpathinfo;
	ops.setpathinfo = nbench_setpathinfo;
	ops.open = nbench_open;
	ops.mkdir = nbench_mkdir;
	ops.rmdir = nbench_rmdir;
	ops.rename = nbench_rename;
	ops.copy = nbench_copy;
	ops.ioctl = nbench_ioctl;
	ops.read = nbench_read;
	ops.write = nbench_write;
	ops.seek = nbench_seek;
	ops.flush = nbench_flush;	
	ops.close = nbench_close;
	ops.exit = nbench_exit;
	ops.lock = nbench_lock;
	ops.setfileinfo = nbench_setfileinfo;
	ops.qfileinfo = nbench_qfileinfo;
	ops.fsinfo = nbench_fsinfo;
	ops.lpq = nbench_lpq;
	ops.search_first = nbench_search_first;
	ops.search_next = nbench_search_next;
	ops.search_close = nbench_search_close;
	ops.trans = nbench_trans;
	ops.logoff = nbench_logoff;
	ops.async_setup = nbench_async_setup;
	ops.cancel = nbench_cancel;

	/* we don't register a trans2 handler as we want to be able to
	   log individual trans2 requests */
	ops.trans2 = NULL;

	/* register ourselves with the NTVFS subsystem. */
	ret = ntvfs_register(&ops, &vers);

	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register nbench backend!\n"));
	}
	
	return ret;
}
