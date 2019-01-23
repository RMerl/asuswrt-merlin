/* 
   Unix SMB/CIFS implementation.
   default IPC$ NTVFS backend

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Stefan (metze) Metzmacher 2004-2005

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
  this implements the IPC$ backend, called by the NTVFS subsystem to
  handle requests on IPC$ shares
*/


#include "includes.h"
#include "../lib/util/dlinklist.h"
#include "ntvfs/ntvfs.h"
#include "../librpc/gen_ndr/rap.h"
#include "ntvfs/ipc/proto.h"
#include "libcli/raw/ioctl.h"
#include "param/param.h"
#include "../lib/tsocket/tsocket.h"
#include "../libcli/named_pipe_auth/npa_tstream.h"
#include "auth/auth.h"
#include "auth/auth_sam_reply.h"
#include "lib/socket/socket.h"
#include "auth/credentials/credentials.h"
#include "auth/credentials/credentials_krb5.h"
#include <gssapi/gssapi.h>
#include "system/locale.h"

/* this is the private structure used to keep the state of an open
   ipc$ connection. It needs to keep information about all open
   pipes */
struct ipc_private {
	struct ntvfs_module_context *ntvfs;

	/* a list of open pipes */
	struct pipe_state {
		struct pipe_state *next, *prev;
		struct ipc_private *ipriv;
		const char *pipe_name;
		struct ntvfs_handle *handle;
		struct tstream_context *npipe;
		uint16_t file_type;
		uint16_t device_state;
		uint64_t allocation_size;
		struct tevent_queue *write_queue;
		struct tevent_queue *read_queue;
	} *pipe_list;
};


/*
  find a open pipe give a file handle
*/
static struct pipe_state *pipe_state_find(struct ipc_private *ipriv, struct ntvfs_handle *handle)
{
	struct pipe_state *s;
	void *p;

	p = ntvfs_handle_get_backend_data(handle, ipriv->ntvfs);
	if (!p) return NULL;

	s = talloc_get_type(p, struct pipe_state);
	if (!s) return NULL;

	return s;
}

/*
  find a open pipe give a wire fnum
*/
static struct pipe_state *pipe_state_find_key(struct ipc_private *ipriv, struct ntvfs_request *req, const DATA_BLOB *key)
{
	struct ntvfs_handle *h;

	h = ntvfs_handle_search_by_wire_key(ipriv->ntvfs, req, key);
	if (!h) return NULL;

	return pipe_state_find(ipriv, h);
}


/*
  connect to a share - always works 
*/
static NTSTATUS ipc_connect(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req,
			    union smb_tcon* tcon)
{
	struct ipc_private *ipriv;
	const char *sharename;

	switch (tcon->generic.level) {
	case RAW_TCON_TCON:
		sharename = tcon->tcon.in.service;
		break;
	case RAW_TCON_TCONX:
		sharename = tcon->tconx.in.path;
		break;
	case RAW_TCON_SMB2:
		sharename = tcon->smb2.in.path;
		break;
	default:
		return NT_STATUS_INVALID_LEVEL;
	}

	if (strncmp(sharename, "\\\\", 2) == 0) {
		char *p = strchr(sharename+2, '\\');
		if (p) {
			sharename = p + 1;
		}
	}

	ntvfs->ctx->fs_type = talloc_strdup(ntvfs->ctx, "IPC");
	NT_STATUS_HAVE_NO_MEMORY(ntvfs->ctx->fs_type);

	ntvfs->ctx->dev_type = talloc_strdup(ntvfs->ctx, "IPC");
	NT_STATUS_HAVE_NO_MEMORY(ntvfs->ctx->dev_type);

	if (tcon->generic.level == RAW_TCON_TCONX) {
		tcon->tconx.out.fs_type = ntvfs->ctx->fs_type;
		tcon->tconx.out.dev_type = ntvfs->ctx->dev_type;
	}

	/* prepare the private state for this connection */
	ipriv = talloc(ntvfs, struct ipc_private);
	NT_STATUS_HAVE_NO_MEMORY(ipriv);

	ntvfs->private_data = ipriv;

	ipriv->ntvfs = ntvfs;
	ipriv->pipe_list = NULL;

	return NT_STATUS_OK;
}

/*
  disconnect from a share
*/
static NTSTATUS ipc_disconnect(struct ntvfs_module_context *ntvfs)
{
	return NT_STATUS_OK;
}

/*
  delete a file
*/
static NTSTATUS ipc_unlink(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req,
			   union smb_unlink *unl)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  check if a directory exists
*/
static NTSTATUS ipc_chkpath(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req,
			    union smb_chkpath *cp)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  return info on a pathname
*/
static NTSTATUS ipc_qpathinfo(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req, union smb_fileinfo *info)
{
	switch (info->generic.level) {
	case  RAW_FILEINFO_GENERIC:
		return NT_STATUS_INVALID_DEVICE_REQUEST;
	case RAW_FILEINFO_GETATTR:
		return NT_STATUS_ACCESS_DENIED;
	default:
		return ntvfs_map_qpathinfo(ntvfs, req, info);
	}
}

/*
  set info on a pathname
*/
static NTSTATUS ipc_setpathinfo(struct ntvfs_module_context *ntvfs,
				struct ntvfs_request *req, union smb_setfileinfo *st)
{
	return NT_STATUS_ACCESS_DENIED;
}


/*
  destroy a open pipe structure
*/
static int ipc_fd_destructor(struct pipe_state *p)
{
	DLIST_REMOVE(p->ipriv->pipe_list, p);
	ntvfs_handle_remove_backend_data(p->handle, p->ipriv->ntvfs);
	return 0;
}

struct ipc_open_state {
	struct ipc_private *ipriv;
	struct pipe_state *p;
	struct ntvfs_request *req;
	union smb_open *oi;
	struct auth_session_info_transport *session_info_transport;
};

static void ipc_open_done(struct tevent_req *subreq);

/*
  check the pipename is valid
 */
static NTSTATUS validate_pipename(const char *name)
{
	while (*name) {
		if (!isalnum(*name) && *name != '_') {
			return NT_STATUS_INVALID_PARAMETER;
		}
		name++;
	}
	return NT_STATUS_OK;
}

/*
  open a file - used for MSRPC pipes
*/
static NTSTATUS ipc_open(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req, union smb_open *oi)
{
	NTSTATUS status;
	struct pipe_state *p;
	struct ipc_private *ipriv = talloc_get_type_abort(ntvfs->private_data,
				    struct ipc_private);
	struct ntvfs_handle *h;
	struct ipc_open_state *state;
	struct tevent_req *subreq;
	const char *fname;
	const char *directory;
	const struct tsocket_address *client_addr;
	const struct tsocket_address *server_addr;

	switch (oi->generic.level) {
	case RAW_OPEN_NTCREATEX:
	case RAW_OPEN_NTTRANS_CREATE:
		fname = oi->ntcreatex.in.fname;
		break;
	case RAW_OPEN_OPENX:
		fname = oi->openx.in.fname;
		break;
	case RAW_OPEN_SMB2:
		fname = oi->smb2.in.fname;
		break;
	default:
		return NT_STATUS_NOT_SUPPORTED;
	}

	directory = talloc_asprintf(req, "%s/np",
				    lpcfg_ncalrpc_dir(ipriv->ntvfs->ctx->lp_ctx));
	NT_STATUS_HAVE_NO_MEMORY(directory);

	state = talloc(req, struct ipc_open_state);
	NT_STATUS_HAVE_NO_MEMORY(state);

	status = ntvfs_handle_new(ntvfs, req, &h);
	NT_STATUS_NOT_OK_RETURN(status);

	p = talloc(h, struct pipe_state);
	NT_STATUS_HAVE_NO_MEMORY(p);

	while (fname[0] == '\\') fname++;

	/* check for valid characters in name */
	fname = strlower_talloc(p, fname);

	status = validate_pipename(fname);
	NT_STATUS_NOT_OK_RETURN(status);

	p->pipe_name = talloc_asprintf(p, "\\pipe\\%s", fname);
	NT_STATUS_HAVE_NO_MEMORY(p->pipe_name);

	p->handle = h;
	p->ipriv = ipriv;

	p->write_queue = tevent_queue_create(p, "ipc_write_queue");
	NT_STATUS_HAVE_NO_MEMORY(p->write_queue);

	p->read_queue = tevent_queue_create(p, "ipc_read_queue");
	NT_STATUS_HAVE_NO_MEMORY(p->read_queue);

	state->ipriv = ipriv;
	state->p = p;
	state->req = req;
	state->oi = oi;

	status = auth_session_info_transport_from_session(state,
							  req->session_info,
							  ipriv->ntvfs->ctx->event_ctx,
							  ipriv->ntvfs->ctx->lp_ctx,
							  &state->session_info_transport);

	NT_STATUS_NOT_OK_RETURN(status);

	client_addr = ntvfs_get_local_address(ipriv->ntvfs);
	server_addr = ntvfs_get_remote_address(ipriv->ntvfs);

	subreq = tstream_npa_connect_send(p,
					  ipriv->ntvfs->ctx->event_ctx,
					  directory,
					  fname,
					  client_addr,
					  NULL,
					  server_addr,
					  NULL,
					  state->session_info_transport);
	NT_STATUS_HAVE_NO_MEMORY(subreq);
	tevent_req_set_callback(subreq, ipc_open_done, state);

	req->async_states->state |= NTVFS_ASYNC_STATE_ASYNC;
	return NT_STATUS_OK;
}

static void ipc_open_done(struct tevent_req *subreq)
{
	struct ipc_open_state *state = tevent_req_callback_data(subreq,
				       struct ipc_open_state);
	struct ipc_private *ipriv = state->ipriv;
	struct pipe_state *p = state->p;
	struct ntvfs_request *req = state->req;
	union smb_open *oi = state->oi;
	int ret;
	int sys_errno;
	NTSTATUS status;

	ret = tstream_npa_connect_recv(subreq, &sys_errno,
				       p, &p->npipe,
				       &p->file_type,
				       &p->device_state,
				       &p->allocation_size);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		status = map_nt_error_from_unix(sys_errno);
		goto reply;
	}

	DLIST_ADD(ipriv->pipe_list, p);
	talloc_set_destructor(p, ipc_fd_destructor);

	status = ntvfs_handle_set_backend_data(p->handle, ipriv->ntvfs, p);
	if (!NT_STATUS_IS_OK(status)) {
		goto reply;
	}

	switch (oi->generic.level) {
	case RAW_OPEN_NTCREATEX:
		ZERO_STRUCT(oi->ntcreatex.out);
		oi->ntcreatex.out.file.ntvfs	= p->handle;
		oi->ntcreatex.out.oplock_level	= 0;
		oi->ntcreatex.out.create_action	= NTCREATEX_ACTION_EXISTED;
		oi->ntcreatex.out.create_time	= 0;
		oi->ntcreatex.out.access_time	= 0;
		oi->ntcreatex.out.write_time	= 0;
		oi->ntcreatex.out.change_time	= 0;
		oi->ntcreatex.out.attrib	= FILE_ATTRIBUTE_NORMAL;
		oi->ntcreatex.out.alloc_size	= p->allocation_size;
		oi->ntcreatex.out.size		= 0;
		oi->ntcreatex.out.file_type	= p->file_type;
		oi->ntcreatex.out.ipc_state	= p->device_state;
		oi->ntcreatex.out.is_directory	= 0;
		break;
	case RAW_OPEN_OPENX:
		ZERO_STRUCT(oi->openx.out);
		oi->openx.out.file.ntvfs	= p->handle;
		oi->openx.out.attrib		= FILE_ATTRIBUTE_NORMAL;
		oi->openx.out.write_time	= 0;
		oi->openx.out.size		= 0;
		oi->openx.out.access		= 0;
		oi->openx.out.ftype		= p->file_type;
		oi->openx.out.devstate		= p->device_state;
		oi->openx.out.action		= 0;
		oi->openx.out.unique_fid	= 0;
		oi->openx.out.access_mask	= 0;
		oi->openx.out.unknown		= 0;
		break;
	case RAW_OPEN_SMB2:
		ZERO_STRUCT(oi->smb2.out);
		oi->smb2.out.file.ntvfs		= p->handle;
		oi->smb2.out.oplock_level	= oi->smb2.in.oplock_level;
		oi->smb2.out.create_action	= NTCREATEX_ACTION_EXISTED;
		oi->smb2.out.create_time	= 0;
		oi->smb2.out.access_time	= 0;
		oi->smb2.out.write_time		= 0;
		oi->smb2.out.change_time	= 0;
		oi->smb2.out.alloc_size		= p->allocation_size;
		oi->smb2.out.size		= 0;
		oi->smb2.out.file_attr		= FILE_ATTRIBUTE_NORMAL;
		oi->smb2.out.reserved2		= 0;
		break;
	default:
		break;
	}

reply:
	req->async_states->status = status;
	req->async_states->send_fn(req);
}

/*
  create a directory
*/
static NTSTATUS ipc_mkdir(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, union smb_mkdir *md)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  remove a directory
*/
static NTSTATUS ipc_rmdir(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, struct smb_rmdir *rd)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  rename a set of files
*/
static NTSTATUS ipc_rename(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req, union smb_rename *ren)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  copy a set of files
*/
static NTSTATUS ipc_copy(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req, struct smb_copy *cp)
{
	return NT_STATUS_ACCESS_DENIED;
}

struct ipc_readv_next_vector_state {
	uint8_t *buf;
	size_t len;
	off_t ofs;
	size_t remaining;
};

static void ipc_readv_next_vector_init(struct ipc_readv_next_vector_state *s,
				       uint8_t *buf, size_t len)
{
	ZERO_STRUCTP(s);

	s->buf = buf;
	s->len = MIN(len, UINT16_MAX);
}

static int ipc_readv_next_vector(struct tstream_context *stream,
				 void *private_data,
				 TALLOC_CTX *mem_ctx,
				 struct iovec **_vector,
				 size_t *count)
{
	struct ipc_readv_next_vector_state *state =
		(struct ipc_readv_next_vector_state *)private_data;
	struct iovec *vector;
	ssize_t pending;
	size_t wanted;

	if (state->ofs == state->len) {
		*_vector = NULL;
		*count = 0;
		return 0;
	}

	pending = tstream_pending_bytes(stream);
	if (pending == -1) {
		return -1;
	}

	if (pending == 0 && state->ofs != 0) {
		/* return a short read */
		*_vector = NULL;
		*count = 0;
		return 0;
	}

	if (pending == 0) {
		/* we want at least one byte and recheck again */
		wanted = 1;
	} else {
		size_t missing = state->len - state->ofs;
		if (pending > missing) {
			/* there's more available */
			state->remaining = pending - missing;
			wanted = missing;
		} else {
			/* read what we can get and recheck in the next cycle */
			wanted = pending;
		}
	}

	vector = talloc_array(mem_ctx, struct iovec, 1);
	if (!vector) {
		return -1;
	}

	vector[0].iov_base = (char *) (state->buf + state->ofs);
	vector[0].iov_len = wanted;

	state->ofs += wanted;

	*_vector = vector;
	*count = 1;
	return 0;
}

struct ipc_read_state {
	struct ipc_private *ipriv;
	struct pipe_state *p;
	struct ntvfs_request *req;
	union smb_read *rd;
	struct ipc_readv_next_vector_state next_vector;
};

static void ipc_read_done(struct tevent_req *subreq);

/*
  read from a file
*/
static NTSTATUS ipc_read(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req, union smb_read *rd)
{
	struct ipc_private *ipriv = talloc_get_type_abort(ntvfs->private_data,
				    struct ipc_private);
	struct pipe_state *p;
	struct ipc_read_state *state;
	struct tevent_req *subreq;

	if (rd->generic.level != RAW_READ_GENERIC) {
		return ntvfs_map_read(ntvfs, req, rd);
	}

	p = pipe_state_find(ipriv, rd->readx.in.file.ntvfs);
	if (!p) {
		return NT_STATUS_INVALID_HANDLE;
	}

	state = talloc(req, struct ipc_read_state);
	NT_STATUS_HAVE_NO_MEMORY(state);

	state->ipriv = ipriv;
	state->p = p;
	state->req = req;
	state->rd = rd;

	/* rd->readx.out.data is already allocated */
	ipc_readv_next_vector_init(&state->next_vector,
				   rd->readx.out.data,
				   rd->readx.in.maxcnt);

	subreq = tstream_readv_pdu_queue_send(req,
					      ipriv->ntvfs->ctx->event_ctx,
					      p->npipe,
					      p->read_queue,
					      ipc_readv_next_vector,
					      &state->next_vector);
	NT_STATUS_HAVE_NO_MEMORY(subreq);
	tevent_req_set_callback(subreq, ipc_read_done, state);

	req->async_states->state |= NTVFS_ASYNC_STATE_ASYNC;
	return NT_STATUS_OK;
}

static void ipc_read_done(struct tevent_req *subreq)
{
	struct ipc_read_state *state =
		tevent_req_callback_data(subreq,
		struct ipc_read_state);
	struct ntvfs_request *req = state->req;
	union smb_read *rd = state->rd;
	int ret;
	int sys_errno;
	NTSTATUS status;

	ret = tstream_readv_pdu_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		status = map_nt_error_from_unix(sys_errno);
		goto reply;
	}

	status = NT_STATUS_OK;
	if (state->next_vector.remaining > 0) {
		status = STATUS_BUFFER_OVERFLOW;
	}

	rd->readx.out.remaining = state->next_vector.remaining;
	rd->readx.out.compaction_mode = 0;
	rd->readx.out.nread = ret;

reply:
	req->async_states->status = status;
	req->async_states->send_fn(req);
}

struct ipc_write_state {
	struct ipc_private *ipriv;
	struct pipe_state *p;
	struct ntvfs_request *req;
	union smb_write *wr;
	struct iovec iov;
};

static void ipc_write_done(struct tevent_req *subreq);

/*
  write to a file
*/
static NTSTATUS ipc_write(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, union smb_write *wr)
{
	struct ipc_private *ipriv = talloc_get_type_abort(ntvfs->private_data,
				    struct ipc_private);
	struct pipe_state *p;
	struct tevent_req *subreq;
	struct ipc_write_state *state;

	if (wr->generic.level != RAW_WRITE_GENERIC) {
		return ntvfs_map_write(ntvfs, req, wr);
	}

	p = pipe_state_find(ipriv, wr->writex.in.file.ntvfs);
	if (!p) {
		return NT_STATUS_INVALID_HANDLE;
	}

	state = talloc(req, struct ipc_write_state);
	NT_STATUS_HAVE_NO_MEMORY(state);

	state->ipriv = ipriv;
	state->p = p;
	state->req = req;
	state->wr = wr;
	state->iov.iov_base = discard_const_p(void, wr->writex.in.data);
	state->iov.iov_len = wr->writex.in.count;

	subreq = tstream_writev_queue_send(state,
					   ipriv->ntvfs->ctx->event_ctx,
					   p->npipe,
					   p->write_queue,
					   &state->iov, 1);
	NT_STATUS_HAVE_NO_MEMORY(subreq);
	tevent_req_set_callback(subreq, ipc_write_done, state);

	req->async_states->state |= NTVFS_ASYNC_STATE_ASYNC;
	return NT_STATUS_OK;
}

static void ipc_write_done(struct tevent_req *subreq)
{
	struct ipc_write_state *state =
		tevent_req_callback_data(subreq,
		struct ipc_write_state);
	struct ntvfs_request *req = state->req;
	union smb_write *wr = state->wr;
	int ret;
	int sys_errno;
	NTSTATUS status;

	ret = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		status = map_nt_error_from_unix(sys_errno);
		goto reply;
	}

	status = NT_STATUS_OK;

	wr->writex.out.nwritten = ret;
	wr->writex.out.remaining = 0;

reply:
	req->async_states->status = status;
	req->async_states->send_fn(req);
}

/*
  seek in a file
*/
static NTSTATUS ipc_seek(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req,
			 union smb_seek *io)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  flush a file
*/
static NTSTATUS ipc_flush(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req,
			  union smb_flush *io)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  close a file
*/
static NTSTATUS ipc_close(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, union smb_close *io)
{
	struct ipc_private *ipriv = talloc_get_type_abort(ntvfs->private_data,
				    struct ipc_private);
	struct pipe_state *p;

	if (io->generic.level != RAW_CLOSE_CLOSE) {
		return ntvfs_map_close(ntvfs, req, io);
	}

	p = pipe_state_find(ipriv, io->close.in.file.ntvfs);
	if (!p) {
		return NT_STATUS_INVALID_HANDLE;
	}

	talloc_free(p);

	return NT_STATUS_OK;
}

/*
  exit - closing files
*/
static NTSTATUS ipc_exit(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req)
{
	struct ipc_private *ipriv = talloc_get_type_abort(ntvfs->private_data,
				    struct ipc_private);
	struct pipe_state *p, *next;
	
	for (p=ipriv->pipe_list; p; p=next) {
		next = p->next;
		if (p->handle->session_info == req->session_info &&
		    p->handle->smbpid == req->smbpid) {
			talloc_free(p);
		}
	}

	return NT_STATUS_OK;
}

/*
  logoff - closing files open by the user
*/
static NTSTATUS ipc_logoff(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req)
{
	struct ipc_private *ipriv = talloc_get_type_abort(ntvfs->private_data,
				    struct ipc_private);
	struct pipe_state *p, *next;
	
	for (p=ipriv->pipe_list; p; p=next) {
		next = p->next;
		if (p->handle->session_info == req->session_info) {
			talloc_free(p);
		}
	}

	return NT_STATUS_OK;
}

/*
  setup for an async call
*/
static NTSTATUS ipc_async_setup(struct ntvfs_module_context *ntvfs,
				struct ntvfs_request *req,
				void *private_data)
{
	return NT_STATUS_OK;
}

/*
  cancel an async call
*/
static NTSTATUS ipc_cancel(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req)
{
	return NT_STATUS_UNSUCCESSFUL;
}

/*
  lock a byte range
*/
static NTSTATUS ipc_lock(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req, union smb_lock *lck)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  set info on a open file
*/
static NTSTATUS ipc_setfileinfo(struct ntvfs_module_context *ntvfs,
				struct ntvfs_request *req, union smb_setfileinfo *info)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  query info on a open file
*/
static NTSTATUS ipc_qfileinfo(struct ntvfs_module_context *ntvfs,
			      struct ntvfs_request *req, union smb_fileinfo *info)
{
	struct ipc_private *ipriv = talloc_get_type_abort(ntvfs->private_data,
				    struct ipc_private);
	struct pipe_state *p = pipe_state_find(ipriv, info->generic.in.file.ntvfs);
	if (!p) {
		return NT_STATUS_INVALID_HANDLE;
	}
	switch (info->generic.level) {
	case RAW_FILEINFO_GENERIC: 
	{
		ZERO_STRUCT(info->generic.out);
		info->generic.out.attrib = FILE_ATTRIBUTE_NORMAL;
		info->generic.out.fname.s = strrchr(p->pipe_name, '\\');
		info->generic.out.alloc_size = 4096;
		info->generic.out.nlink = 1;
		/* What the heck?  Match Win2k3: IPC$ pipes are delete pending */
		info->generic.out.delete_pending = 1;
		return NT_STATUS_OK;
	}
	case RAW_FILEINFO_ALT_NAME_INFO:
	case RAW_FILEINFO_ALT_NAME_INFORMATION:
	case RAW_FILEINFO_STREAM_INFO:
	case RAW_FILEINFO_STREAM_INFORMATION:
	case RAW_FILEINFO_COMPRESSION_INFO:
	case RAW_FILEINFO_COMPRESSION_INFORMATION:
	case RAW_FILEINFO_NETWORK_OPEN_INFORMATION:
	case RAW_FILEINFO_ATTRIBUTE_TAG_INFORMATION:
		return NT_STATUS_INVALID_PARAMETER;
	case  RAW_FILEINFO_ALL_EAS:
		return NT_STATUS_ACCESS_DENIED;
	default:
		return ntvfs_map_qfileinfo(ntvfs, req, info);
	}
}


/*
  return filesystem info
*/
static NTSTATUS ipc_fsinfo(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req, union smb_fsinfo *fs)
{
	return NT_STATUS_ACCESS_DENIED;
}

/*
  return print queue info
*/
static NTSTATUS ipc_lpq(struct ntvfs_module_context *ntvfs,
			struct ntvfs_request *req, union smb_lpq *lpq)
{
	return NT_STATUS_ACCESS_DENIED;
}

/* 
   list files in a directory matching a wildcard pattern
*/
static NTSTATUS ipc_search_first(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, union smb_search_first *io,
			  void *search_private, 
			  bool (*callback)(void *, const union smb_search_data *))
{
	return NT_STATUS_ACCESS_DENIED;
}

/* 
   continue listing files in a directory 
*/
static NTSTATUS ipc_search_next(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req, union smb_search_next *io,
			 void *search_private, 
			 bool (*callback)(void *, const union smb_search_data *))
{
	return NT_STATUS_ACCESS_DENIED;
}

/* 
   end listing files in a directory 
*/
static NTSTATUS ipc_search_close(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, union smb_search_close *io)
{
	return NT_STATUS_ACCESS_DENIED;
}

struct ipc_trans_state {
	struct ipc_private *ipriv;
	struct pipe_state *p;
	struct ntvfs_request *req;
	struct smb_trans2 *trans;
	struct iovec writev_iov;
	struct ipc_readv_next_vector_state next_vector;
};

static void ipc_trans_writev_done(struct tevent_req *subreq);
static void ipc_trans_readv_done(struct tevent_req *subreq);

/* SMBtrans - handle a DCERPC command */
static NTSTATUS ipc_dcerpc_cmd(struct ntvfs_module_context *ntvfs,
			       struct ntvfs_request *req, struct smb_trans2 *trans)
{
	struct ipc_private *ipriv = talloc_get_type_abort(ntvfs->private_data,
				    struct ipc_private);
	struct pipe_state *p;
	DATA_BLOB fnum_key;
	uint16_t fnum;
	struct ipc_trans_state *state;
	struct tevent_req *subreq;

	/*
	 * the fnum is in setup[1], a 16 bit value
	 * the setup[*] values are already in host byteorder
	 * but ntvfs_handle_search_by_wire_key() expects
	 * network byteorder
	 */
	SSVAL(&fnum, 0, trans->in.setup[1]);
	fnum_key = data_blob_const(&fnum, 2);

	p = pipe_state_find_key(ipriv, req, &fnum_key);
	if (!p) {
		return NT_STATUS_INVALID_HANDLE;
	}

	/*
	 * Trans requests are only allowed
	 * if no other Trans or Read is active
	 */
	if (tevent_queue_length(p->read_queue) > 0) {
		return NT_STATUS_PIPE_BUSY;
	}

	state = talloc(req, struct ipc_trans_state);
	NT_STATUS_HAVE_NO_MEMORY(state);

	trans->out.setup_count = 0;
	trans->out.setup = NULL;
	trans->out.params = data_blob(NULL, 0);
	trans->out.data = data_blob_talloc(req, NULL, trans->in.max_data);
	NT_STATUS_HAVE_NO_MEMORY(trans->out.data.data);

	state->ipriv = ipriv;
	state->p = p;
	state->req = req;
	state->trans = trans;
	state->writev_iov.iov_base = (char *) trans->in.data.data;
	state->writev_iov.iov_len = trans->in.data.length;

	ipc_readv_next_vector_init(&state->next_vector,
				   trans->out.data.data,
				   trans->out.data.length);

	subreq = tstream_writev_queue_send(state,
					   ipriv->ntvfs->ctx->event_ctx,
					   p->npipe,
					   p->write_queue,
					   &state->writev_iov, 1);
	NT_STATUS_HAVE_NO_MEMORY(subreq);
	tevent_req_set_callback(subreq, ipc_trans_writev_done, state);

	req->async_states->state |= NTVFS_ASYNC_STATE_ASYNC;
	return NT_STATUS_OK;
}

static void ipc_trans_writev_done(struct tevent_req *subreq)
{
	struct ipc_trans_state *state =
		tevent_req_callback_data(subreq,
		struct ipc_trans_state);
	struct ipc_private *ipriv = state->ipriv;
	struct pipe_state *p = state->p;
	struct ntvfs_request *req = state->req;
	int ret;
	int sys_errno;
	NTSTATUS status;

	ret = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == 0) {
		status = NT_STATUS_PIPE_DISCONNECTED;
		goto reply;
	} else if (ret == -1) {
		status = map_nt_error_from_unix(sys_errno);
		goto reply;
	}

	subreq = tstream_readv_pdu_queue_send(state,
					      ipriv->ntvfs->ctx->event_ctx,
					      p->npipe,
					      p->read_queue,
					      ipc_readv_next_vector,
					      &state->next_vector);
	if (!subreq) {
		status = NT_STATUS_NO_MEMORY;
		goto reply;
	}
	tevent_req_set_callback(subreq, ipc_trans_readv_done, state);
	return;

reply:
	req->async_states->status = status;
	req->async_states->send_fn(req);
}

static void ipc_trans_readv_done(struct tevent_req *subreq)
{
	struct ipc_trans_state *state =
		tevent_req_callback_data(subreq,
		struct ipc_trans_state);
	struct ntvfs_request *req = state->req;
	struct smb_trans2 *trans = state->trans;
	int ret;
	int sys_errno;
	NTSTATUS status;

	ret = tstream_readv_pdu_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		status = map_nt_error_from_unix(sys_errno);
		goto reply;
	}

	status = NT_STATUS_OK;
	if (state->next_vector.remaining > 0) {
		status = STATUS_BUFFER_OVERFLOW;
	}

	trans->out.data.length = ret;

reply:
	req->async_states->status = status;
	req->async_states->send_fn(req);
}

/* SMBtrans - set named pipe state */
static NTSTATUS ipc_set_nm_pipe_state(struct ntvfs_module_context *ntvfs,
				      struct ntvfs_request *req, struct smb_trans2 *trans)
{
	struct ipc_private *ipriv = talloc_get_type_abort(ntvfs->private_data,
				    struct ipc_private);
	struct pipe_state *p;
	DATA_BLOB fnum_key;

	/* the fnum is in setup[1] */
	fnum_key = data_blob_const(&trans->in.setup[1], sizeof(trans->in.setup[1]));

	p = pipe_state_find_key(ipriv, req, &fnum_key);
	if (!p) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (trans->in.params.length != 2) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/*
	 * TODO: pass this to the tstream_npa logic
	 */
	p->device_state = SVAL(trans->in.params.data, 0);

	trans->out.setup_count = 0;
	trans->out.setup = NULL;
	trans->out.params = data_blob(NULL, 0);
	trans->out.data = data_blob(NULL, 0);

	return NT_STATUS_OK;
}


/* SMBtrans - used to provide access to SMB pipes */
static NTSTATUS ipc_trans(struct ntvfs_module_context *ntvfs,
				struct ntvfs_request *req, struct smb_trans2 *trans)
{
	NTSTATUS status;

	if (strequal(trans->in.trans_name, "\\PIPE\\LANMAN"))
		return ipc_rap_call(req, ntvfs->ctx->event_ctx, ntvfs->ctx->lp_ctx, trans);

       	if (trans->in.setup_count != 2) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	switch (trans->in.setup[0]) {
	case TRANSACT_SETNAMEDPIPEHANDLESTATE:
		status = ipc_set_nm_pipe_state(ntvfs, req, trans);
		break;
	case TRANSACT_DCERPCCMD:
		status = ipc_dcerpc_cmd(ntvfs, req, trans);
		break;
	default:
		status = NT_STATUS_INVALID_PARAMETER;
		break;
	}

	return status;
}

struct ipc_ioctl_state {
	struct ipc_private *ipriv;
	struct pipe_state *p;
	struct ntvfs_request *req;
	union smb_ioctl *io;
	struct iovec writev_iov;
	struct ipc_readv_next_vector_state next_vector;
};

static void ipc_ioctl_writev_done(struct tevent_req *subreq);
static void ipc_ioctl_readv_done(struct tevent_req *subreq);

static NTSTATUS ipc_ioctl_smb2(struct ntvfs_module_context *ntvfs,
			       struct ntvfs_request *req, union smb_ioctl *io)
{
	struct ipc_private *ipriv = talloc_get_type_abort(ntvfs->private_data,
				    struct ipc_private);
	struct pipe_state *p;
	struct ipc_ioctl_state *state;
	struct tevent_req *subreq;

	switch (io->smb2.in.function) {
	case FSCTL_NAMED_PIPE_READ_WRITE:
		break;

	default:
		return NT_STATUS_FS_DRIVER_REQUIRED;
	}

	p = pipe_state_find(ipriv, io->smb2.in.file.ntvfs);
	if (!p) {
		return NT_STATUS_INVALID_HANDLE;
	}

	/*
	 * Trans requests are only allowed
	 * if no other Trans or Read is active
	 */
	if (tevent_queue_length(p->read_queue) > 0) {
		return NT_STATUS_PIPE_BUSY;
	}

	state = talloc(req, struct ipc_ioctl_state);
	NT_STATUS_HAVE_NO_MEMORY(state);

	io->smb2.out._pad	= 0;
	io->smb2.out.function	= io->smb2.in.function;
	io->smb2.out.unknown2	= 0;
	io->smb2.out.unknown3	= 0;
	io->smb2.out.in		= io->smb2.in.out;
	io->smb2.out.out = data_blob_talloc(req, NULL, io->smb2.in.max_response_size);
	NT_STATUS_HAVE_NO_MEMORY(io->smb2.out.out.data);

	state->ipriv = ipriv;
	state->p = p;
	state->req = req;
	state->io = io;
	state->writev_iov.iov_base = (char *) io->smb2.in.out.data;
	state->writev_iov.iov_len = io->smb2.in.out.length;

	ipc_readv_next_vector_init(&state->next_vector,
				   io->smb2.out.out.data,
				   io->smb2.out.out.length);

	subreq = tstream_writev_queue_send(state,
					   ipriv->ntvfs->ctx->event_ctx,
					   p->npipe,
					   p->write_queue,
					   &state->writev_iov, 1);
	NT_STATUS_HAVE_NO_MEMORY(subreq);
	tevent_req_set_callback(subreq, ipc_ioctl_writev_done, state);

	req->async_states->state |= NTVFS_ASYNC_STATE_ASYNC;
	return NT_STATUS_OK;
}

static void ipc_ioctl_writev_done(struct tevent_req *subreq)
{
	struct ipc_ioctl_state *state =
		tevent_req_callback_data(subreq,
		struct ipc_ioctl_state);
	struct ipc_private *ipriv = state->ipriv;
	struct pipe_state *p = state->p;
	struct ntvfs_request *req = state->req;
	int ret;
	int sys_errno;
	NTSTATUS status;

	ret = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		status = map_nt_error_from_unix(sys_errno);
		goto reply;
	}

	subreq = tstream_readv_pdu_queue_send(state,
					      ipriv->ntvfs->ctx->event_ctx,
					      p->npipe,
					      p->read_queue,
					      ipc_readv_next_vector,
					      &state->next_vector);
	if (!subreq) {
		status = NT_STATUS_NO_MEMORY;
		goto reply;
	}
	tevent_req_set_callback(subreq, ipc_ioctl_readv_done, state);
	return;

reply:
	req->async_states->status = status;
	req->async_states->send_fn(req);
}

static void ipc_ioctl_readv_done(struct tevent_req *subreq)
{
	struct ipc_ioctl_state *state =
		tevent_req_callback_data(subreq,
		struct ipc_ioctl_state);
	struct ntvfs_request *req = state->req;
	union smb_ioctl *io = state->io;
	int ret;
	int sys_errno;
	NTSTATUS status;

	ret = tstream_readv_pdu_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (ret == -1) {
		status = map_nt_error_from_unix(sys_errno);
		goto reply;
	}

	status = NT_STATUS_OK;
	if (state->next_vector.remaining > 0) {
		status = STATUS_BUFFER_OVERFLOW;
	}

	io->smb2.out.out.length = ret;

reply:
	req->async_states->status = status;
	req->async_states->send_fn(req);
}

/*
  ioctl interface
*/
static NTSTATUS ipc_ioctl(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, union smb_ioctl *io)
{
	switch (io->generic.level) {
	case RAW_IOCTL_SMB2:
		return ipc_ioctl_smb2(ntvfs, req, io);

	case RAW_IOCTL_SMB2_NO_HANDLE:
		return NT_STATUS_FS_DRIVER_REQUIRED;

	default:
		return NT_STATUS_ACCESS_DENIED;
	}
}


/*
  initialialise the IPC backend, registering ourselves with the ntvfs subsystem
 */
NTSTATUS ntvfs_ipc_init(void)
{
	NTSTATUS ret;
	struct ntvfs_ops ops;
	NTVFS_CURRENT_CRITICAL_SIZES(vers);

	ZERO_STRUCT(ops);
	
	/* fill in the name and type */
	ops.name = "default";
	ops.type = NTVFS_IPC;

	/* fill in all the operations */
	ops.connect = ipc_connect;
	ops.disconnect = ipc_disconnect;
	ops.unlink = ipc_unlink;
	ops.chkpath = ipc_chkpath;
	ops.qpathinfo = ipc_qpathinfo;
	ops.setpathinfo = ipc_setpathinfo;
	ops.open = ipc_open;
	ops.mkdir = ipc_mkdir;
	ops.rmdir = ipc_rmdir;
	ops.rename = ipc_rename;
	ops.copy = ipc_copy;
	ops.ioctl = ipc_ioctl;
	ops.read = ipc_read;
	ops.write = ipc_write;
	ops.seek = ipc_seek;
	ops.flush = ipc_flush;	
	ops.close = ipc_close;
	ops.exit = ipc_exit;
	ops.lock = ipc_lock;
	ops.setfileinfo = ipc_setfileinfo;
	ops.qfileinfo = ipc_qfileinfo;
	ops.fsinfo = ipc_fsinfo;
	ops.lpq = ipc_lpq;
	ops.search_first = ipc_search_first;
	ops.search_next = ipc_search_next;
	ops.search_close = ipc_search_close;
	ops.trans = ipc_trans;
	ops.logoff = ipc_logoff;
	ops.async_setup = ipc_async_setup;
	ops.cancel = ipc_cancel;

	/* register ourselves with the NTVFS subsystem. */
	ret = ntvfs_register(&ops, &vers);

	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register IPC backend!\n"));
		return ret;
	}

	return ret;
}
