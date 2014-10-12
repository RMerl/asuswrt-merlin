/* 
   Unix SMB2 implementation.
   
   Copyright (C) Stefan Metzmacher	2005
   
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
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include "smb_server/smb_server.h"
#include "smb_server/smb2/smb2_server.h"
#include "smbd/service_stream.h"
#include "ntvfs/ntvfs.h"

/*
  send an oplock break request to a client
*/
static NTSTATUS smb2srv_send_oplock_break(void *p, struct ntvfs_handle *h, uint8_t level)
{
	struct smbsrv_handle *handle = talloc_get_type(h->frontend_data.private_data,
						       struct smbsrv_handle);
	struct smb2srv_request *req;
	NTSTATUS status;

	/* setup a dummy request structure */
	req = smb2srv_init_request(handle->tcon->smb_conn);
	NT_STATUS_HAVE_NO_MEMORY(req);

	req->in.buffer		= talloc_array(req, uint8_t, 
					       NBT_HDR_SIZE + SMB2_MIN_SIZE);
	NT_STATUS_HAVE_NO_MEMORY(req->in.buffer);
	req->in.size		= NBT_HDR_SIZE + SMB2_MIN_SIZE;
	req->in.allocated	= req->in.size;

	req->in.hdr		= req->in.buffer+ NBT_HDR_SIZE;
	req->in.body		= req->in.hdr	+ SMB2_HDR_BODY;
	req->in.body_size	= req->in.size	- (SMB2_HDR_BODY+NBT_HDR_SIZE);
	req->in.dynamic 	= NULL;

	req->seqnum		= UINT64_MAX;

	smb2srv_setup_bufinfo(req);

	SIVAL(req->in.hdr, 0,				SMB2_MAGIC);
	SSVAL(req->in.hdr, SMB2_HDR_LENGTH,		SMB2_HDR_BODY);
	SSVAL(req->in.hdr, SMB2_HDR_EPOCH,		0);
	SIVAL(req->in.hdr, SMB2_HDR_STATUS,		0);
	SSVAL(req->in.hdr, SMB2_HDR_OPCODE,		SMB2_OP_BREAK);
	SSVAL(req->in.hdr, SMB2_HDR_CREDIT,		0);
	SIVAL(req->in.hdr, SMB2_HDR_FLAGS,		0);
	SIVAL(req->in.hdr, SMB2_HDR_NEXT_COMMAND,	0);
	SBVAL(req->in.hdr, SMB2_HDR_MESSAGE_ID,		0);
	SIVAL(req->in.hdr, SMB2_HDR_PID,		0);
	SIVAL(req->in.hdr, SMB2_HDR_TID,		0);
	SBVAL(req->in.hdr, SMB2_HDR_SESSION_ID,		0);
	memset(req->in.hdr+SMB2_HDR_SIGNATURE, 0, 16);

	SSVAL(req->in.body, 0, 2);

	status = smb2srv_setup_reply(req, 0x18, false, 0);
	NT_STATUS_NOT_OK_RETURN(status);

	SSVAL(req->out.hdr, SMB2_HDR_CREDIT,	0x0000);

	SSVAL(req->out.body, 0x02, 0x0001);
	SIVAL(req->out.body, 0x04, 0x00000000);
	smb2srv_push_handle(req->out.body, 0x08, h);

	smb2srv_send_reply(req);

	return NT_STATUS_OK;
}

struct ntvfs_handle *smb2srv_pull_handle(struct smb2srv_request *req, const uint8_t *base, unsigned int offset)
{
	struct smbsrv_tcon *tcon;
	struct smbsrv_handle *handle;
	uint32_t hid;
	uint32_t tid;
	uint64_t uid;

	/*
	 * if there're chained requests used the cached handle
	 *
	 * TODO: check if this also correct when the given handle
	 *       isn't all 0xFF.
	 */
	if (req->chained_file_handle) {
		base = req->chained_file_handle;
		offset = 0;
	}

	hid = IVAL(base, offset);
	tid = IVAL(base, offset + 4);
	uid = BVAL(base, offset + 8);

	/* if it's the wildcard handle, don't waste time to search it... */
	if (hid == UINT32_MAX && tid == UINT32_MAX && uid == UINT64_MAX) {
		return NULL;
	}

	/*
	 * if the (v)uid part doesn't match the given session the handle isn't
	 * valid
	 */
	if (uid != req->session->vuid) {
		return NULL;
	}

	/*
	 * the handle can belong to a different tcon
	 * as that TID in the SMB2 header says, but
	 * the request should succeed nevertheless!
	 *
	 * because of this we put the 32 bit TID into the
	 * 128 bit handle, so that we can extract the tcon from the
	 * handle
	 */
	tcon = req->tcon;
	if (tid != req->tcon->tid) {
		tcon = smbsrv_smb2_tcon_find(req->session, tid, req->request_time);
		if (!tcon) {
			return NULL;
		}
	}

	handle = smbsrv_smb2_handle_find(tcon, hid, req->request_time);
	if (!handle) {
		return NULL;
	}

	/*
	 * as the smb2srv_tcon is a child object of the smb2srv_session
	 * the handle belongs to the correct session!
	 *
	 * Note: no check is needed here for SMB2
	 */

	/*
	 * as the handle may have overwritten the tcon
	 * we need to set it on the request so that the
	 * correct ntvfs context will be used for the ntvfs_*() request
	 *
	 * TODO: check if that's correct for chained requests as well!
	 */
	req->tcon = tcon;
	return handle->ntvfs;
}

void smb2srv_push_handle(uint8_t *base, unsigned int offset, struct ntvfs_handle *ntvfs)
{
	struct smbsrv_handle *handle = talloc_get_type(ntvfs->frontend_data.private_data,
				       struct smbsrv_handle);

	/* 
	 * the handle is 128 bit on the wire
	 */
	SIVAL(base, offset,	handle->hid);
	SIVAL(base, offset + 4,	handle->tcon->tid);
	SBVAL(base, offset + 8,	handle->session->vuid);
}

static NTSTATUS smb2srv_handle_create_new(void *private_data, struct ntvfs_request *ntvfs, struct ntvfs_handle **_h)
{
	struct smb2srv_request *req = talloc_get_type(ntvfs->frontend_data.private_data,
				      struct smb2srv_request);
	struct smbsrv_handle *handle;
	struct ntvfs_handle *h;

	handle = smbsrv_handle_new(req->session, req->tcon, req, req->request_time);
	if (!handle) return NT_STATUS_INSUFFICIENT_RESOURCES;

	h = talloc_zero(handle, struct ntvfs_handle);
	if (!h) goto nomem;

	/* 
	 * note: we don't set handle->ntvfs yet,
	 *       this will be done by smbsrv_handle_make_valid()
	 *       this makes sure the handle is invalid for clients
	 *       until the ntvfs subsystem has made it valid
	 */
	h->ctx		= ntvfs->ctx;
	h->session_info	= ntvfs->session_info;
	h->smbpid	= ntvfs->smbpid;

	h->frontend_data.private_data = handle;

	*_h = h;
	return NT_STATUS_OK;
nomem:
	talloc_free(handle);
	return NT_STATUS_NO_MEMORY;
}

static NTSTATUS smb2srv_handle_make_valid(void *private_data, struct ntvfs_handle *h)
{
	struct smbsrv_tcon *tcon = talloc_get_type(private_data, struct smbsrv_tcon);
	struct smbsrv_handle *handle = talloc_get_type(h->frontend_data.private_data,
						       struct smbsrv_handle);
	/* this tells the frontend that the handle is valid */
	handle->ntvfs = h;
	/* this moves the smbsrv_request to the smbsrv_tcon memory context */
	talloc_steal(tcon, handle);
	return NT_STATUS_OK;
}

static void smb2srv_handle_destroy(void *private_data, struct ntvfs_handle *h)
{
	struct smbsrv_handle *handle = talloc_get_type(h->frontend_data.private_data,
						       struct smbsrv_handle);
	talloc_free(handle);
}

static struct ntvfs_handle *smb2srv_handle_search_by_wire_key(void *private_data, struct ntvfs_request *ntvfs, const DATA_BLOB *key)
{
	return NULL;
}

static DATA_BLOB smb2srv_handle_get_wire_key(void *private_data, struct ntvfs_handle *handle, TALLOC_CTX *mem_ctx)
{
	return data_blob(NULL, 0);
}

static NTSTATUS smb2srv_tcon_backend(struct smb2srv_request *req, union smb_tcon *io)
{
	struct smbsrv_tcon *tcon;
	NTSTATUS status;
	enum ntvfs_type type;
	const char *service = io->smb2.in.path;
	struct share_config *scfg;
	const char *sharetype;
	uint64_t ntvfs_caps = 0;

	if (strncmp(service, "\\\\", 2) == 0) {
		const char *p = strchr(service+2, '\\');
		if (p) {
			service = p + 1;
		}
	}

	status = share_get_config(req, req->smb_conn->share_context, service, &scfg);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("smb2srv_tcon_backend: couldn't find service %s\n", service));
		return NT_STATUS_BAD_NETWORK_NAME;
	}

	if (!socket_check_access(req->smb_conn->connection->socket, 
				 scfg->name, 
				 share_string_list_option(req, scfg, SHARE_HOSTS_ALLOW), 
				 share_string_list_option(req, scfg, SHARE_HOSTS_DENY))) {
		return NT_STATUS_ACCESS_DENIED;
	}

	/* work out what sort of connection this is */
	sharetype = share_string_option(scfg, SHARE_TYPE, "DISK");
	if (sharetype && strcmp(sharetype, "IPC") == 0) {
		type = NTVFS_IPC;
	} else if (sharetype && strcmp(sharetype, "PRINTER") == 0) {
		type = NTVFS_PRINT;
	} else {
		type = NTVFS_DISK;
	}

	tcon = smbsrv_smb2_tcon_new(req->session, scfg->name);
	if (!tcon) {
		DEBUG(0,("smb2srv_tcon_backend: Couldn't find free connection.\n"));
		return NT_STATUS_INSUFFICIENT_RESOURCES;
	}
	req->tcon = tcon;

	ntvfs_caps = NTVFS_CLIENT_CAP_LEVEL_II_OPLOCKS;

	/* init ntvfs function pointers */
	status = ntvfs_init_connection(tcon, scfg, type,
				       req->smb_conn->negotiate.protocol,
				       ntvfs_caps,
				       req->smb_conn->connection->event.ctx,
				       req->smb_conn->connection->msg_ctx,
				       req->smb_conn->lp_ctx,
				       req->smb_conn->connection->server_id,
				       &tcon->ntvfs);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("smb2srv_tcon_backend: ntvfs_init_connection failed for service %s\n", 
			  scfg->name));
		goto failed;
	}

	status = ntvfs_set_oplock_handler(tcon->ntvfs, smb2srv_send_oplock_break, tcon);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("smb2srv_tcon_backend: NTVFS failed to set the oplock handler!\n"));
		goto failed;
	}

	status = ntvfs_set_addresses(tcon->ntvfs,
				     req->smb_conn->connection->local_address,
				     req->smb_conn->connection->remote_address);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("smb2srv_tcon_backend: NTVFS failed to set the address!\n"));
		goto failed;
	}

	status = ntvfs_set_handle_callbacks(tcon->ntvfs,
					    smb2srv_handle_create_new,
					    smb2srv_handle_make_valid,
					    smb2srv_handle_destroy,
					    smb2srv_handle_search_by_wire_key,
					    smb2srv_handle_get_wire_key,
					    tcon);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("smb2srv_tcon_backend: NTVFS failed to set the handle callbacks!\n"));
		goto failed;
	}

	req->ntvfs = ntvfs_request_create(req->tcon->ntvfs, req,
					  req->session->session_info,
					  SVAL(req->in.hdr, SMB2_HDR_PID),
					  req->request_time,
					  req, NULL, 0);
	if (!req->ntvfs) {
		status = NT_STATUS_NO_MEMORY;
		goto failed;
	}

	io->smb2.out.share_type	  = (unsigned)type; /* 1 - DISK, 2 - Print, 3 - IPC */
	io->smb2.out.reserved	  = 0;
	io->smb2.out.flags	  = 0x00000000;
	io->smb2.out.capabilities = 0;
	io->smb2.out.access_mask  = SEC_RIGHTS_FILE_ALL;

	io->smb2.out.tid	= tcon->tid;

	/* Invoke NTVFS connection hook */
	status = ntvfs_connect(req->ntvfs, io);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("smb2srv_tcon_backend: NTVFS ntvfs_connect() failed: %s!\n", nt_errstr(status)));
		goto failed;
	}

	return NT_STATUS_OK;

failed:
	req->tcon = NULL;
	talloc_free(tcon);
	return status;
}

static void smb2srv_tcon_send(struct smb2srv_request *req, union smb_tcon *io)
{
	uint16_t credit;

	if (!NT_STATUS_IS_OK(req->status)) {
		smb2srv_send_error(req, req->status);
		return;
	}
	if (io->smb2.out.share_type == NTVFS_IPC) {
		/* if it's an IPC share vista returns 0x0005 */
		credit = 0x0005;
	} else {
		credit = 0x0001;
	}

	SMB2SRV_CHECK(smb2srv_setup_reply(req, 0x10, false, 0));

	SIVAL(req->out.hdr,	SMB2_HDR_TID,	io->smb2.out.tid);
	SSVAL(req->out.hdr,	SMB2_HDR_CREDIT,credit);

	SCVAL(req->out.body,	0x02,		io->smb2.out.share_type);
	SCVAL(req->out.body,	0x03,		io->smb2.out.reserved);
	SIVAL(req->out.body,	0x04,		io->smb2.out.flags);
	SIVAL(req->out.body,	0x08,		io->smb2.out.capabilities);
	SIVAL(req->out.body,	0x0C,		io->smb2.out.access_mask);

	smb2srv_send_reply(req);
}

void smb2srv_tcon_recv(struct smb2srv_request *req)
{
	union smb_tcon *io;

	SMB2SRV_CHECK_BODY_SIZE(req, 0x08, true);
	SMB2SRV_TALLOC_IO_PTR(io, union smb_tcon);

	io->smb2.level		= RAW_TCON_SMB2;
	io->smb2.in.reserved	= SVAL(req->in.body, 0x02);
	SMB2SRV_CHECK(smb2_pull_o16s16_string(&req->in, io, req->in.body+0x04, &io->smb2.in.path));

	/* the VFS backend does not yet handle NULL paths */
	if (io->smb2.in.path == NULL) {
		io->smb2.in.path = "";
	}

	req->status = smb2srv_tcon_backend(req, io);

	if (req->control_flags & SMB2SRV_REQ_CTRL_FLAG_NOT_REPLY) {
		talloc_free(req);
		return;
	}
	smb2srv_tcon_send(req, io);
}

static NTSTATUS smb2srv_tdis_backend(struct smb2srv_request *req)
{
	/* TODO: call ntvfs backends to close file of this tcon */
	talloc_free(req->tcon);
	req->tcon = NULL;
	return NT_STATUS_OK;
}

static void smb2srv_tdis_send(struct smb2srv_request *req)
{
	NTSTATUS status;

	if (NT_STATUS_IS_ERR(req->status)) {
		smb2srv_send_error(req, req->status);
		return;
	}

	status = smb2srv_setup_reply(req, 0x04, false, 0);
	if (!NT_STATUS_IS_OK(status)) {
		smbsrv_terminate_connection(req->smb_conn, nt_errstr(status));
		talloc_free(req);
		return;
	}

	SSVAL(req->out.body, 0x02, 0);

	smb2srv_send_reply(req);
}

void smb2srv_tdis_recv(struct smb2srv_request *req)
{
	uint16_t _pad;

	SMB2SRV_CHECK_BODY_SIZE(req, 0x04, false);

	_pad	= SVAL(req->in.body, 0x02);

	req->status = smb2srv_tdis_backend(req);

	if (req->control_flags & SMB2SRV_REQ_CTRL_FLAG_NOT_REPLY) {
		talloc_free(req);
		return;
	}
	smb2srv_tdis_send(req);
}
