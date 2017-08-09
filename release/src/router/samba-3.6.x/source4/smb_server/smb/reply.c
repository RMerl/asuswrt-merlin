/* 
   Unix SMB/CIFS implementation.
   Main SMB reply routines
   Copyright (C) Andrew Tridgell 1992-2003
   Copyright (C) James J Myers 2003 <myersjj@samba.org>
   Copyright (C) Stefan Metzmacher 2006

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
   This file handles most of the reply_ calls that the server
   makes to handle specific SMB commands
*/

#include "includes.h"
#include "smb_server/smb_server.h"
#include "ntvfs/ntvfs.h"
#include "librpc/gen_ndr/ndr_nbt.h"


/****************************************************************************
 Reply to a simple request (async send)
****************************************************************************/
static void reply_simple_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;

	SMBSRV_CHECK_ASYNC_STATUS_SIMPLE;

	smbsrv_setup_reply(req, 0, 0);
	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to a tcon (async reply)
****************************************************************************/
static void reply_tcon_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_tcon *con;

	SMBSRV_CHECK_ASYNC_STATUS(con, union smb_tcon);

	/* construct reply */
	smbsrv_setup_reply(req, 2, 0);

	SSVAL(req->out.vwv, VWV(0), con->tcon.out.max_xmit);
	SSVAL(req->out.vwv, VWV(1), con->tcon.out.tid);
	SSVAL(req->out.hdr, HDR_TID, req->tcon->tid);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a tcon.
****************************************************************************/
void smbsrv_reply_tcon(struct smbsrv_request *req)
{
	union smb_tcon *con;
	NTSTATUS status;
	uint8_t *p;
	
	/* parse request */
	SMBSRV_CHECK_WCT(req, 0);

	SMBSRV_TALLOC_IO_PTR(con, union smb_tcon);

	con->tcon.level = RAW_TCON_TCON;

	p = req->in.data;	
	p += req_pull_ascii4(&req->in.bufinfo, &con->tcon.in.service, p, STR_TERMINATE);
	p += req_pull_ascii4(&req->in.bufinfo, &con->tcon.in.password, p, STR_TERMINATE);
	p += req_pull_ascii4(&req->in.bufinfo, &con->tcon.in.dev, p, STR_TERMINATE);

	if (!con->tcon.in.service || !con->tcon.in.password || !con->tcon.in.dev) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	/* Instantiate backend */
	status = smbsrv_tcon_backend(req, con);
	if (!NT_STATUS_IS_OK(status)) {
		smbsrv_send_error(req, status);
		return;
	}

	SMBSRV_SETUP_NTVFS_REQUEST(reply_tcon_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	/* Invoke NTVFS connection hook */
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_connect(req->ntvfs, con));
}


/****************************************************************************
 Reply to a tcon and X (async reply)
****************************************************************************/
static void reply_tcon_and_X_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_tcon *con;

	SMBSRV_CHECK_ASYNC_STATUS(con, union smb_tcon);

	/* construct reply - two variants */
	if (req->smb_conn->negotiate.protocol < PROTOCOL_NT1) {
		smbsrv_setup_reply(req, 2, 0);

		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);

		req_push_str(req, NULL, con->tconx.out.dev_type, -1, STR_TERMINATE|STR_ASCII);
	} else {
		smbsrv_setup_reply(req, 3, 0);

		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv, VWV(2), con->tconx.out.options);

		req_push_str(req, NULL, con->tconx.out.dev_type, -1, STR_TERMINATE|STR_ASCII);
		req_push_str(req, NULL, con->tconx.out.fs_type, -1, STR_TERMINATE);
	}

	/* set the incoming and outgoing tid to the just created one */
	SSVAL(req->in.hdr, HDR_TID, con->tconx.out.tid);
	SSVAL(req->out.hdr,HDR_TID, con->tconx.out.tid);

	smbsrv_chain_reply(req);
}

/****************************************************************************
 Reply to a tcon and X.
****************************************************************************/
void smbsrv_reply_tcon_and_X(struct smbsrv_request *req)
{
	NTSTATUS status;
	union smb_tcon *con;
	uint8_t *p;
	uint16_t passlen;

	SMBSRV_TALLOC_IO_PTR(con, union smb_tcon);

	con->tconx.level = RAW_TCON_TCONX;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 4);

	con->tconx.in.flags  = SVAL(req->in.vwv, VWV(2));
	passlen              = SVAL(req->in.vwv, VWV(3));

	p = req->in.data;

	if (!req_pull_blob(&req->in.bufinfo, p, passlen, &con->tconx.in.password)) {
		smbsrv_send_error(req, NT_STATUS_ILL_FORMED_PASSWORD);
		return;
	}
	p += passlen;

	p += req_pull_string(&req->in.bufinfo, &con->tconx.in.path, p, -1, STR_TERMINATE);
	p += req_pull_string(&req->in.bufinfo, &con->tconx.in.device, p, -1, STR_ASCII);

	if (!con->tconx.in.path || !con->tconx.in.device) {
		smbsrv_send_error(req, NT_STATUS_BAD_DEVICE_TYPE);
		return;
	}

	/* Instantiate backend */
	status = smbsrv_tcon_backend(req, con);
	if (!NT_STATUS_IS_OK(status)) {
		smbsrv_send_error(req, status);
		return;
	}

	SMBSRV_SETUP_NTVFS_REQUEST(reply_tcon_and_X_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	/* Invoke NTVFS connection hook */
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_connect(req->ntvfs, con));
}


/****************************************************************************
 Reply to an unknown request
****************************************************************************/
void smbsrv_reply_unknown(struct smbsrv_request *req)
{
	int type;

	type = CVAL(req->in.hdr, HDR_COM);
  
	DEBUG(0,("unknown command type %d (0x%X)\n", type, type));

	smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRunknownsmb));
}


/****************************************************************************
 Reply to an ioctl (async reply)
****************************************************************************/
static void reply_ioctl_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_ioctl *io;

	SMBSRV_CHECK_ASYNC_STATUS(io, union smb_ioctl);

	/* the +1 is for nicer alignment */
	smbsrv_setup_reply(req, 8, io->ioctl.out.blob.length+1);
	SSVAL(req->out.vwv, VWV(1), io->ioctl.out.blob.length);
	SSVAL(req->out.vwv, VWV(5), io->ioctl.out.blob.length);
	SSVAL(req->out.vwv, VWV(6), PTR_DIFF(req->out.data, req->out.hdr) + 1);

	memcpy(req->out.data+1, io->ioctl.out.blob.data, io->ioctl.out.blob.length);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to an ioctl.
****************************************************************************/
void smbsrv_reply_ioctl(struct smbsrv_request *req)
{
	union smb_ioctl *io;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 3);
	SMBSRV_TALLOC_IO_PTR(io, union smb_ioctl);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_ioctl_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->ioctl.level		= RAW_IOCTL_IOCTL;
	io->ioctl.in.file.ntvfs	= smbsrv_pull_fnum(req, req->in.vwv, VWV(0));
	io->ioctl.in.request	= IVAL(req->in.vwv, VWV(1));

	SMBSRV_CHECK_FILE_HANDLE_ERROR(io->ioctl.in.file.ntvfs,
				       NT_STATUS_DOS(ERRSRV, ERRerror));
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_ioctl(req->ntvfs, io));
}


/****************************************************************************
 Reply to a chkpth.
****************************************************************************/
void smbsrv_reply_chkpth(struct smbsrv_request *req)
{
	union smb_chkpath *io;

	SMBSRV_TALLOC_IO_PTR(io, union smb_chkpath);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	req_pull_ascii4(&req->in.bufinfo, &io->chkpath.in.path, req->in.data, STR_TERMINATE);

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_chkpath(req->ntvfs, io));
}

/****************************************************************************
 Reply to a getatr (async reply)
****************************************************************************/
static void reply_getatr_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_fileinfo *st;

	SMBSRV_CHECK_ASYNC_STATUS(st, union smb_fileinfo);

	/* construct reply */
	smbsrv_setup_reply(req, 10, 0);

	SSVAL(req->out.vwv,         VWV(0), st->getattr.out.attrib);
	srv_push_dos_date3(req->smb_conn, req->out.vwv, VWV(1), st->getattr.out.write_time);
	SIVAL(req->out.vwv,         VWV(3), st->getattr.out.size);

	SMBSRV_VWV_RESERVED(5, 5);

	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to a getatr.
****************************************************************************/
void smbsrv_reply_getatr(struct smbsrv_request *req)
{
	union smb_fileinfo *st;

	SMBSRV_TALLOC_IO_PTR(st, union smb_fileinfo);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_getatr_send, NTVFS_ASYNC_STATE_MAY_ASYNC);
	
	st->getattr.level = RAW_FILEINFO_GETATTR;

	/* parse request */
	req_pull_ascii4(&req->in.bufinfo, &st->getattr.in.file.path, req->in.data, STR_TERMINATE);
	if (!st->getattr.in.file.path) {
		smbsrv_send_error(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return;
	}

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_qpathinfo(req->ntvfs, st));
}


/****************************************************************************
 Reply to a setatr.
****************************************************************************/
void smbsrv_reply_setatr(struct smbsrv_request *req)
{
	union smb_setfileinfo *st;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 8);
	SMBSRV_TALLOC_IO_PTR(st, union smb_setfileinfo);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	st->setattr.level = RAW_SFILEINFO_SETATTR;
	st->setattr.in.attrib     = SVAL(req->in.vwv, VWV(0));
	st->setattr.in.write_time = srv_pull_dos_date3(req->smb_conn, req->in.vwv + VWV(1));
	
	req_pull_ascii4(&req->in.bufinfo, &st->setattr.in.file.path, req->in.data, STR_TERMINATE);

	if (!st->setattr.in.file.path) {
		smbsrv_send_error(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return;
	}
	
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_setpathinfo(req->ntvfs, st));
}


/****************************************************************************
 Reply to a dskattr (async reply)
****************************************************************************/
static void reply_dskattr_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_fsinfo *fs;

	SMBSRV_CHECK_ASYNC_STATUS(fs, union smb_fsinfo);

	/* construct reply */
	smbsrv_setup_reply(req, 5, 0);

	SSVAL(req->out.vwv, VWV(0), fs->dskattr.out.units_total);
	SSVAL(req->out.vwv, VWV(1), fs->dskattr.out.blocks_per_unit);
	SSVAL(req->out.vwv, VWV(2), fs->dskattr.out.block_size);
	SSVAL(req->out.vwv, VWV(3), fs->dskattr.out.units_free);

	SMBSRV_VWV_RESERVED(4, 1);

	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to a dskattr.
****************************************************************************/
void smbsrv_reply_dskattr(struct smbsrv_request *req)
{
	union smb_fsinfo *fs;

	SMBSRV_TALLOC_IO_PTR(fs, union smb_fsinfo);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_dskattr_send, NTVFS_ASYNC_STATE_MAY_ASYNC);
	
	fs->dskattr.level = RAW_QFS_DSKATTR;

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_fsinfo(req->ntvfs, fs));
}


/****************************************************************************
 Reply to an open (async reply)
****************************************************************************/
static void reply_open_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_open *oi;

	SMBSRV_CHECK_ASYNC_STATUS(oi, union smb_open);

	/* construct reply */
	smbsrv_setup_reply(req, 7, 0);

	smbsrv_push_fnum(req->out.vwv, VWV(0), oi->openold.out.file.ntvfs);
	SSVAL(req->out.vwv, VWV(1), oi->openold.out.attrib);
	srv_push_dos_date3(req->smb_conn, req->out.vwv, VWV(2), oi->openold.out.write_time);
	SIVAL(req->out.vwv, VWV(4), oi->openold.out.size);
	SSVAL(req->out.vwv, VWV(6), oi->openold.out.rmode);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to an open.
****************************************************************************/
void smbsrv_reply_open(struct smbsrv_request *req)
{
	union smb_open *oi;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 2);
	SMBSRV_TALLOC_IO_PTR(oi, union smb_open);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_open_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	oi->openold.level = RAW_OPEN_OPEN;
	oi->openold.in.open_mode = SVAL(req->in.vwv, VWV(0));
	oi->openold.in.search_attrs = SVAL(req->in.vwv, VWV(1));

	req_pull_ascii4(&req->in.bufinfo, &oi->openold.in.fname, req->in.data, STR_TERMINATE);

	if (!oi->openold.in.fname) {
		smbsrv_send_error(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return;
	}

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_open(req->ntvfs, oi));
}


/****************************************************************************
 Reply to an open and X (async reply)
****************************************************************************/
static void reply_open_and_X_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_open *oi;

	SMBSRV_CHECK_ASYNC_STATUS(oi, union smb_open);

	/* build the reply */
	if (oi->openx.in.flags & OPENX_FLAGS_EXTENDED_RETURN) {
		smbsrv_setup_reply(req, 19, 0);
	} else {
		smbsrv_setup_reply(req, 15, 0);
	}

	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);
	smbsrv_push_fnum(req->out.vwv, VWV(2), oi->openx.out.file.ntvfs);
	SSVAL(req->out.vwv, VWV(3), oi->openx.out.attrib);
	srv_push_dos_date3(req->smb_conn, req->out.vwv, VWV(4), oi->openx.out.write_time);
	SIVAL(req->out.vwv, VWV(6), oi->openx.out.size);
	SSVAL(req->out.vwv, VWV(8), oi->openx.out.access);
	SSVAL(req->out.vwv, VWV(9), oi->openx.out.ftype);
	SSVAL(req->out.vwv, VWV(10),oi->openx.out.devstate);
	SSVAL(req->out.vwv, VWV(11),oi->openx.out.action);
	SIVAL(req->out.vwv, VWV(12),oi->openx.out.unique_fid);
	SSVAL(req->out.vwv, VWV(14),0); /* reserved */
	if (oi->openx.in.flags & OPENX_FLAGS_EXTENDED_RETURN) {
		SIVAL(req->out.vwv, VWV(15),oi->openx.out.access_mask);
		SMBSRV_VWV_RESERVED(17, 2);
	}

	req->chained_fnum = SVAL(req->out.vwv, VWV(2));

	smbsrv_chain_reply(req);
}


/****************************************************************************
 Reply to an open and X.
****************************************************************************/
void smbsrv_reply_open_and_X(struct smbsrv_request *req)
{
	union smb_open *oi;

	/* parse the request */
	SMBSRV_CHECK_WCT(req, 15);
	SMBSRV_TALLOC_IO_PTR(oi, union smb_open);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_open_and_X_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	oi->openx.level = RAW_OPEN_OPENX;
	oi->openx.in.flags        = SVAL(req->in.vwv, VWV(2));
	oi->openx.in.open_mode    = SVAL(req->in.vwv, VWV(3));
	oi->openx.in.search_attrs = SVAL(req->in.vwv, VWV(4));
	oi->openx.in.file_attrs   = SVAL(req->in.vwv, VWV(5));
	oi->openx.in.write_time   = srv_pull_dos_date3(req->smb_conn, req->in.vwv + VWV(6));
	oi->openx.in.open_func    = SVAL(req->in.vwv, VWV(8));
	oi->openx.in.size         = IVAL(req->in.vwv, VWV(9));
	oi->openx.in.timeout      = IVAL(req->in.vwv, VWV(11));

	req_pull_ascii4(&req->in.bufinfo, &oi->openx.in.fname, req->in.data, STR_TERMINATE);

	if (!oi->openx.in.fname) {
		smbsrv_send_error(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return;
	}

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_open(req->ntvfs, oi));
}


/****************************************************************************
 Reply to a mknew or a create.
****************************************************************************/
static void reply_mknew_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_open *oi;

	SMBSRV_CHECK_ASYNC_STATUS(oi, union smb_open);

	/* build the reply */
	smbsrv_setup_reply(req, 1, 0);

	smbsrv_push_fnum(req->out.vwv, VWV(0), oi->mknew.out.file.ntvfs);

	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to a mknew or a create.
****************************************************************************/
void smbsrv_reply_mknew(struct smbsrv_request *req)
{
	union smb_open *oi;

	/* parse the request */
	SMBSRV_CHECK_WCT(req, 3);
	SMBSRV_TALLOC_IO_PTR(oi, union smb_open);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_mknew_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	if (CVAL(req->in.hdr, HDR_COM) == SMBmknew) {
		oi->mknew.level = RAW_OPEN_MKNEW;
	} else {
		oi->mknew.level = RAW_OPEN_CREATE;
	}
	oi->mknew.in.attrib  = SVAL(req->in.vwv, VWV(0));
	oi->mknew.in.write_time  = srv_pull_dos_date3(req->smb_conn, req->in.vwv + VWV(1));

	req_pull_ascii4(&req->in.bufinfo, &oi->mknew.in.fname, req->in.data, STR_TERMINATE);

	if (!oi->mknew.in.fname) {
		smbsrv_send_error(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return;
	}

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_open(req->ntvfs, oi));
}

/****************************************************************************
 Reply to a create temporary file (async reply)
****************************************************************************/
static void reply_ctemp_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_open *oi;

	SMBSRV_CHECK_ASYNC_STATUS(oi, union smb_open);

	/* build the reply */
	smbsrv_setup_reply(req, 1, 0);

	smbsrv_push_fnum(req->out.vwv, VWV(0), oi->ctemp.out.file.ntvfs);

	/* the returned filename is relative to the directory */
	req_push_str(req, NULL, oi->ctemp.out.name, -1, STR_TERMINATE | STR_ASCII);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a create temporary file.
****************************************************************************/
void smbsrv_reply_ctemp(struct smbsrv_request *req)
{
	union smb_open *oi;

	/* parse the request */
	SMBSRV_CHECK_WCT(req, 3);
	SMBSRV_TALLOC_IO_PTR(oi, union smb_open);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_ctemp_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	oi->ctemp.level = RAW_OPEN_CTEMP;
	oi->ctemp.in.attrib = SVAL(req->in.vwv, VWV(0));
	oi->ctemp.in.write_time = srv_pull_dos_date3(req->smb_conn, req->in.vwv + VWV(1));

	/* the filename is actually a directory name, the server provides a filename
	   in that directory */
	req_pull_ascii4(&req->in.bufinfo, &oi->ctemp.in.directory, req->in.data, STR_TERMINATE);

	if (!oi->ctemp.in.directory) {
		smbsrv_send_error(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return;
	}

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_open(req->ntvfs, oi));
}


/****************************************************************************
 Reply to a unlink
****************************************************************************/
void smbsrv_reply_unlink(struct smbsrv_request *req)
{
	union smb_unlink *unl;

	/* parse the request */
	SMBSRV_CHECK_WCT(req, 1);
	SMBSRV_TALLOC_IO_PTR(unl, union smb_unlink);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);
	
	unl->unlink.in.attrib = SVAL(req->in.vwv, VWV(0));

	req_pull_ascii4(&req->in.bufinfo, &unl->unlink.in.pattern, req->in.data, STR_TERMINATE);
	
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_unlink(req->ntvfs, unl));
}


/****************************************************************************
 Reply to a readbraw (core+ protocol).
 this is a strange packet because it doesn't use a standard SMB header in the reply,
 only the 4 byte NBT header
 This command must be replied to synchronously
****************************************************************************/
void smbsrv_reply_readbraw(struct smbsrv_request *req)
{
	NTSTATUS status;
	union smb_read io;

	io.readbraw.level = RAW_READ_READBRAW;

	/* there are two variants, one with 10 and one with 8 command words */
	if (req->in.wct < 8) {
		goto failed;
	}

	io.readbraw.in.file.ntvfs = smbsrv_pull_fnum(req, req->in.vwv, VWV(0));
	io.readbraw.in.offset  = IVAL(req->in.vwv, VWV(1));
	io.readbraw.in.maxcnt  = SVAL(req->in.vwv, VWV(3));
	io.readbraw.in.mincnt  = SVAL(req->in.vwv, VWV(4));
	io.readbraw.in.timeout = IVAL(req->in.vwv, VWV(5));

	if (!io.readbraw.in.file.ntvfs) {
		goto failed;
	}

	/* the 64 bit variant */
	if (req->in.wct == 10) {
		uint32_t offset_high = IVAL(req->in.vwv, VWV(8));
		io.readbraw.in.offset |= (((off_t)offset_high) << 32);
	}

	/* before calling the backend we setup the raw buffer. This
	 * saves a copy later */
	req->out.size = io.readbraw.in.maxcnt + NBT_HDR_SIZE;
	req->out.buffer = talloc_size(req, req->out.size);
	if (req->out.buffer == NULL) {
		goto failed;
	}
	SIVAL(req->out.buffer, 0, 0); /* init NBT header */

	/* tell the backend where to put the data */
	io.readbraw.out.data = req->out.buffer + NBT_HDR_SIZE;

	/* prepare the ntvfs request */
	req->ntvfs = ntvfs_request_create(req->tcon->ntvfs, req,
					  req->session->session_info,
					  SVAL(req->in.hdr,HDR_PID),
					  req->request_time,
					  req, NULL, 0);
	if (!req->ntvfs) {
		goto failed;
	}

	/* call the backend */
	status = ntvfs_read(req->ntvfs, &io);
	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	req->out.size = io.readbraw.out.nread + NBT_HDR_SIZE;

	smbsrv_send_reply_nosign(req);
	return;

failed:
	/* any failure in readbraw is equivalent to reading zero bytes */
	req->out.size = 4;
	req->out.buffer = talloc_size(req, req->out.size);
	SIVAL(req->out.buffer, 0, 0); /* init NBT header */

	smbsrv_send_reply_nosign(req);
}


/****************************************************************************
 Reply to a lockread (async reply)
****************************************************************************/
static void reply_lockread_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_read *io;

	SMBSRV_CHECK_ASYNC_STATUS(io, union smb_read);

	/* trim packet */
	io->lockread.out.nread = MIN(io->lockread.out.nread,
		req_max_data(req) - 3);
	req_grow_data(req, 3 + io->lockread.out.nread);

	/* construct reply */
	SSVAL(req->out.vwv, VWV(0), io->lockread.out.nread);
	SMBSRV_VWV_RESERVED(1, 4);

	SCVAL(req->out.data, 0, SMB_DATA_BLOCK);
	SSVAL(req->out.data, 1, io->lockread.out.nread);

	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to a lockread (core+ protocol).
 note that the lock is a write lock, not a read lock!
****************************************************************************/
void smbsrv_reply_lockread(struct smbsrv_request *req)
{
	union smb_read *io;
	
	/* parse request */
	SMBSRV_CHECK_WCT(req, 5);
	SMBSRV_TALLOC_IO_PTR(io, union smb_read);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_lockread_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->lockread.level = RAW_READ_LOCKREAD;
	io->lockread.in.file.ntvfs= smbsrv_pull_fnum(req, req->in.vwv, VWV(0));
	io->lockread.in.count     = SVAL(req->in.vwv, VWV(1));
	io->lockread.in.offset    = IVAL(req->in.vwv, VWV(2));
	io->lockread.in.remaining = SVAL(req->in.vwv, VWV(4));

	/* setup the reply packet assuming the maximum possible read */
	smbsrv_setup_reply(req, 5, 3 + io->lockread.in.count);

	/* tell the backend where to put the data */
	io->lockread.out.data = req->out.data + 3;

	SMBSRV_CHECK_FILE_HANDLE(io->lockread.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_read(req->ntvfs, io));
}



/****************************************************************************
 Reply to a read (async reply)
****************************************************************************/
static void reply_read_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_read *io;

	SMBSRV_CHECK_ASYNC_STATUS(io, union smb_read);

	/* trim packet */
	io->read.out.nread = MIN(io->read.out.nread,
		req_max_data(req) - 3);
	req_grow_data(req, 3 + io->read.out.nread);

	/* construct reply */
	SSVAL(req->out.vwv, VWV(0), io->read.out.nread);
	SMBSRV_VWV_RESERVED(1, 4);

	SCVAL(req->out.data, 0, SMB_DATA_BLOCK);
	SSVAL(req->out.data, 1, io->read.out.nread);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a read.
****************************************************************************/
void smbsrv_reply_read(struct smbsrv_request *req)
{
	union smb_read *io;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 5);
	SMBSRV_TALLOC_IO_PTR(io, union smb_read);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_read_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->read.level = RAW_READ_READ;
	io->read.in.file.ntvfs    = smbsrv_pull_fnum(req, req->in.vwv, VWV(0));
	io->read.in.count         = SVAL(req->in.vwv, VWV(1));
	io->read.in.offset        = IVAL(req->in.vwv, VWV(2));
	io->read.in.remaining     = SVAL(req->in.vwv, VWV(4));

	/* setup the reply packet assuming the maximum possible read */
	smbsrv_setup_reply(req, 5, 3 + io->read.in.count);

	/* tell the backend where to put the data */
	io->read.out.data = req->out.data + 3;

	SMBSRV_CHECK_FILE_HANDLE(io->read.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_read(req->ntvfs, io));
}

/****************************************************************************
 Reply to a read and X (async reply)
****************************************************************************/
static void reply_read_and_X_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_read *io;

	SMBSRV_CHECK_ASYNC_STATUS_ERR(io, union smb_read);

	/* readx reply packets can be over-sized */
	req->control_flags |= SMBSRV_REQ_CONTROL_LARGE;
	if (io->readx.in.maxcnt != 0xFFFF &&
	    io->readx.in.mincnt != 0xFFFF) {
		req_grow_data(req, 1 + io->readx.out.nread);
		SCVAL(req->out.data, 0, 0); /* padding */
	} else {
		req_grow_data(req, io->readx.out.nread);
	}

	/* construct reply */
	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);
	SSVAL(req->out.vwv, VWV(2), io->readx.out.remaining);
	SSVAL(req->out.vwv, VWV(3), io->readx.out.compaction_mode);
	SMBSRV_VWV_RESERVED(4, 1);
	SSVAL(req->out.vwv, VWV(5), io->readx.out.nread);
	SSVAL(req->out.vwv, VWV(6), PTR_DIFF(io->readx.out.data, req->out.hdr));
	SSVAL(req->out.vwv, VWV(7), (io->readx.out.nread>>16));
	SMBSRV_VWV_RESERVED(8, 4);

	if (!NT_STATUS_IS_OK(req->ntvfs->async_states->status)) {
		smbsrv_setup_error(req, req->ntvfs->async_states->status);
	}

	smbsrv_chain_reply(req);
}

/****************************************************************************
 Reply to a read and X.
****************************************************************************/
void smbsrv_reply_read_and_X(struct smbsrv_request *req)
{
	union smb_read *io;

	/* parse request */
	if (req->in.wct != 12) {
		SMBSRV_CHECK_WCT(req, 10);
	}

	SMBSRV_TALLOC_IO_PTR(io, union smb_read);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_read_and_X_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->readx.level = RAW_READ_READX;
	io->readx.in.file.ntvfs    = smbsrv_pull_fnum(req, req->in.vwv, VWV(2));
	io->readx.in.offset        = IVAL(req->in.vwv, VWV(3));
	io->readx.in.maxcnt        = SVAL(req->in.vwv, VWV(5));
	io->readx.in.mincnt        = SVAL(req->in.vwv, VWV(6));
	io->readx.in.remaining     = SVAL(req->in.vwv, VWV(9));
	if (req->flags2 & FLAGS2_READ_PERMIT_EXECUTE) {
		io->readx.in.read_for_execute = true;
	} else {
		io->readx.in.read_for_execute = false;
	}

	if (req->smb_conn->negotiate.client_caps & CAP_LARGE_READX) {
		uint32_t high_part = IVAL(req->in.vwv, VWV(7));
		if (high_part == 1) {
			io->readx.in.maxcnt |= high_part << 16;
		}
	}
	
	/* the 64 bit variant */
	if (req->in.wct == 12) {
		uint32_t offset_high = IVAL(req->in.vwv, VWV(10));
		io->readx.in.offset |= (((uint64_t)offset_high) << 32);
	}

	/* setup the reply packet assuming the maximum possible read */
	smbsrv_setup_reply(req, 12, 1 + io->readx.in.maxcnt);

	/* tell the backend where to put the data. Notice the pad byte. */
	if (io->readx.in.maxcnt != 0xFFFF &&
	    io->readx.in.mincnt != 0xFFFF) {
		io->readx.out.data = req->out.data + 1;
	} else {
		io->readx.out.data = req->out.data;
	}

	SMBSRV_CHECK_FILE_HANDLE(io->readx.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_read(req->ntvfs, io));
}


/****************************************************************************
 Reply to a writebraw (core+ or LANMAN1.0 protocol).
****************************************************************************/
void smbsrv_reply_writebraw(struct smbsrv_request *req)
{
	smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRuseSTD));
}


/****************************************************************************
 Reply to a writeunlock (async reply)
****************************************************************************/
static void reply_writeunlock_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_write *io;

	SMBSRV_CHECK_ASYNC_STATUS(io, union smb_write);

	/* construct reply */
	smbsrv_setup_reply(req, 1, 0);

	SSVAL(req->out.vwv, VWV(0), io->writeunlock.out.nwritten);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a writeunlock (core+).
****************************************************************************/
void smbsrv_reply_writeunlock(struct smbsrv_request *req)
{
	union smb_write *io;

	SMBSRV_CHECK_WCT(req, 5);
	SMBSRV_TALLOC_IO_PTR(io, union smb_write);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_writeunlock_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->writeunlock.level = RAW_WRITE_WRITEUNLOCK;
	io->writeunlock.in.file.ntvfs  = smbsrv_pull_fnum(req, req->in.vwv, VWV(0));
	io->writeunlock.in.count       = SVAL(req->in.vwv, VWV(1));
	io->writeunlock.in.offset      = IVAL(req->in.vwv, VWV(2));
	io->writeunlock.in.remaining   = SVAL(req->in.vwv, VWV(4));
	io->writeunlock.in.data        = req->in.data + 3;

	/* make sure they gave us the data they promised */
	if (io->writeunlock.in.count+3 > req->in.data_size) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	/* make sure the data block is big enough */
	if (SVAL(req->in.data, 1) < io->writeunlock.in.count) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	SMBSRV_CHECK_FILE_HANDLE(io->writeunlock.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_write(req->ntvfs, io));
}



/****************************************************************************
 Reply to a write (async reply)
****************************************************************************/
static void reply_write_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_write *io;
	
	SMBSRV_CHECK_ASYNC_STATUS(io, union smb_write);

	/* construct reply */
	smbsrv_setup_reply(req, 1, 0);

	SSVAL(req->out.vwv, VWV(0), io->write.out.nwritten);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a write
****************************************************************************/
void smbsrv_reply_write(struct smbsrv_request *req)
{
	union smb_write *io;

	SMBSRV_CHECK_WCT(req, 5);
	SMBSRV_TALLOC_IO_PTR(io, union smb_write);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_write_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->write.level = RAW_WRITE_WRITE;
	io->write.in.file.ntvfs  = smbsrv_pull_fnum(req, req->in.vwv, VWV(0));
	io->write.in.count       = SVAL(req->in.vwv, VWV(1));
	io->write.in.offset      = IVAL(req->in.vwv, VWV(2));
	io->write.in.remaining   = SVAL(req->in.vwv, VWV(4));
	io->write.in.data        = req->in.data + 3;

	/* make sure they gave us the data they promised */
	if (req_data_oob(&req->in.bufinfo, io->write.in.data, io->write.in.count)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	/* make sure the data block is big enough */
	if (SVAL(req->in.data, 1) < io->write.in.count) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	SMBSRV_CHECK_FILE_HANDLE(io->write.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_write(req->ntvfs, io));
}


/****************************************************************************
 Reply to a write and X (async reply)
****************************************************************************/
static void reply_write_and_X_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_write *io;

	SMBSRV_CHECK_ASYNC_STATUS(io, union smb_write);

	/* construct reply */
	smbsrv_setup_reply(req, 6, 0);

	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);
	SSVAL(req->out.vwv, VWV(2), io->writex.out.nwritten & 0xFFFF);
	SSVAL(req->out.vwv, VWV(3), io->writex.out.remaining);
	SSVAL(req->out.vwv, VWV(4), io->writex.out.nwritten >> 16);
	SMBSRV_VWV_RESERVED(5, 1);

	smbsrv_chain_reply(req);
}

/****************************************************************************
 Reply to a write and X.
****************************************************************************/
void smbsrv_reply_write_and_X(struct smbsrv_request *req)
{
	union smb_write *io;
	
	if (req->in.wct != 14) {
		SMBSRV_CHECK_WCT(req, 12);
	}

	SMBSRV_TALLOC_IO_PTR(io, union smb_write);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_write_and_X_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->writex.level = RAW_WRITE_WRITEX;
	io->writex.in.file.ntvfs= smbsrv_pull_fnum(req, req->in.vwv, VWV(2));
	io->writex.in.offset    = IVAL(req->in.vwv, VWV(3));
	io->writex.in.wmode     = SVAL(req->in.vwv, VWV(7));
	io->writex.in.remaining = SVAL(req->in.vwv, VWV(8));
	io->writex.in.count     = SVAL(req->in.vwv, VWV(10));
	io->writex.in.data      = req->in.hdr + SVAL(req->in.vwv, VWV(11));

	if (req->in.wct == 14) {
		uint32_t offset_high = IVAL(req->in.vwv, VWV(12));
		uint16_t count_high = SVAL(req->in.vwv, VWV(9));
		io->writex.in.offset |= (((uint64_t)offset_high) << 32);
		io->writex.in.count |= ((uint32_t)count_high) << 16;
	}

	/* make sure the data is in bounds */
	if (req_data_oob(&req->in.bufinfo, io->writex.in.data, io->writex.in.count)) {
		smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRerror));
		return;
	}

	SMBSRV_CHECK_FILE_HANDLE(io->writex.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_write(req->ntvfs, io));
}


/****************************************************************************
 Reply to a lseek (async reply)
****************************************************************************/
static void reply_lseek_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_seek *io;

	SMBSRV_CHECK_ASYNC_STATUS(io, union smb_seek);

	/* construct reply */
	smbsrv_setup_reply(req, 2, 0);

	SIVALS(req->out.vwv, VWV(0), io->lseek.out.offset);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a lseek.
****************************************************************************/
void smbsrv_reply_lseek(struct smbsrv_request *req)
{
	union smb_seek *io;

	SMBSRV_CHECK_WCT(req, 4);
	SMBSRV_TALLOC_IO_PTR(io, union smb_seek);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_lseek_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->lseek.in.file.ntvfs	= smbsrv_pull_fnum(req, req->in.vwv,  VWV(0));
	io->lseek.in.mode	= SVAL(req->in.vwv,  VWV(1));
	io->lseek.in.offset	= IVALS(req->in.vwv, VWV(2));

	SMBSRV_CHECK_FILE_HANDLE(io->lseek.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_seek(req->ntvfs, io));
}

/****************************************************************************
 Reply to a flush.
****************************************************************************/
void smbsrv_reply_flush(struct smbsrv_request *req)
{
	union smb_flush *io;
	uint16_t fnum;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 1);
	SMBSRV_TALLOC_IO_PTR(io, union smb_flush);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	fnum = SVAL(req->in.vwv,  VWV(0));
	if (fnum == 0xFFFF) {
		io->flush_all.level	= RAW_FLUSH_ALL;
	} else {
		io->flush.level		= RAW_FLUSH_FLUSH;
		io->flush.in.file.ntvfs = smbsrv_pull_fnum(req, req->in.vwv,  VWV(0));
		SMBSRV_CHECK_FILE_HANDLE(io->flush.in.file.ntvfs);
	}

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_flush(req->ntvfs, io));
}

/****************************************************************************
 Reply to a close 

 Note that this has to deal with closing a directory opened by NT SMB's.
****************************************************************************/
void smbsrv_reply_close(struct smbsrv_request *req)
{
	union smb_close *io;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 3);
	SMBSRV_TALLOC_IO_PTR(io, union smb_close);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->close.level = RAW_CLOSE_CLOSE;
	io->close.in.file.ntvfs = smbsrv_pull_fnum(req, req->in.vwv,  VWV(0));
	io->close.in.write_time = srv_pull_dos_date3(req->smb_conn, req->in.vwv + VWV(1));

	SMBSRV_CHECK_FILE_HANDLE(io->close.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_close(req->ntvfs, io));
}


/****************************************************************************
 Reply to a writeclose (async reply)
****************************************************************************/
static void reply_writeclose_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_write *io;

	SMBSRV_CHECK_ASYNC_STATUS(io, union smb_write);

	/* construct reply */
	smbsrv_setup_reply(req, 1, 0);

	SSVAL(req->out.vwv, VWV(0), io->write.out.nwritten);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a writeclose (Core+ protocol).
****************************************************************************/
void smbsrv_reply_writeclose(struct smbsrv_request *req)
{
	union smb_write *io;

	/* this one is pretty weird - the wct can be 6 or 12 */
	if (req->in.wct != 12) {
		SMBSRV_CHECK_WCT(req, 6);
	}

	SMBSRV_TALLOC_IO_PTR(io, union smb_write);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_writeclose_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->writeclose.level		= RAW_WRITE_WRITECLOSE;
	io->writeclose.in.file.ntvfs	= smbsrv_pull_fnum(req, req->in.vwv, VWV(0));
	io->writeclose.in.count		= SVAL(req->in.vwv, VWV(1));
	io->writeclose.in.offset	= IVAL(req->in.vwv, VWV(2));
	io->writeclose.in.mtime		= srv_pull_dos_date3(req->smb_conn, req->in.vwv + VWV(4));
	io->writeclose.in.data		= req->in.data + 1;

	/* make sure they gave us the data they promised */
	if (req_data_oob(&req->in.bufinfo, io->writeclose.in.data, io->writeclose.in.count)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	SMBSRV_CHECK_FILE_HANDLE(io->writeclose.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_write(req->ntvfs, io));
}

/****************************************************************************
 Reply to a lock.
****************************************************************************/
void smbsrv_reply_lock(struct smbsrv_request *req)
{
	union smb_lock *lck;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 5);
	SMBSRV_TALLOC_IO_PTR(lck, union smb_lock);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	lck->lock.level		= RAW_LOCK_LOCK;
	lck->lock.in.file.ntvfs	= smbsrv_pull_fnum(req, req->in.vwv, VWV(0));
	lck->lock.in.count	= IVAL(req->in.vwv, VWV(1));
	lck->lock.in.offset	= IVAL(req->in.vwv, VWV(3));

	SMBSRV_CHECK_FILE_HANDLE(lck->lock.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_lock(req->ntvfs, lck));
}


/****************************************************************************
 Reply to a unlock.
****************************************************************************/
void smbsrv_reply_unlock(struct smbsrv_request *req)
{
	union smb_lock *lck;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 5);
	SMBSRV_TALLOC_IO_PTR(lck, union smb_lock);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	lck->unlock.level		= RAW_LOCK_UNLOCK;
	lck->unlock.in.file.ntvfs	= smbsrv_pull_fnum(req, req->in.vwv, VWV(0));
	lck->unlock.in.count 		= IVAL(req->in.vwv, VWV(1));
	lck->unlock.in.offset		= IVAL(req->in.vwv, VWV(3));

	SMBSRV_CHECK_FILE_HANDLE(lck->unlock.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_lock(req->ntvfs, lck));
}


/****************************************************************************
 Reply to a tdis.
****************************************************************************/
void smbsrv_reply_tdis(struct smbsrv_request *req)
{
	struct smbsrv_handle *h, *nh;

	SMBSRV_CHECK_WCT(req, 0);

	/*
	 * TODO: cancel all pending requests on this tcon
	 */

	/*
	 * close all handles on this tcon
	 */
	for (h=req->tcon->handles.list; h; h=nh) {
		nh = h->next;
		talloc_free(h);
	}

	/* finally destroy the tcon */
	talloc_free(req->tcon);
	req->tcon = NULL;

	smbsrv_setup_reply(req, 0, 0);
	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to a echo. This is one of the few calls that is handled directly (the
 backends don't see it at all)
****************************************************************************/
void smbsrv_reply_echo(struct smbsrv_request *req)
{
	uint16_t count;
	int i;

	SMBSRV_CHECK_WCT(req, 1);

	count = SVAL(req->in.vwv, VWV(0));

	smbsrv_setup_reply(req, 1, req->in.data_size);

	memcpy(req->out.data, req->in.data, req->in.data_size);

	for (i=1; i <= count;i++) {
		struct smbsrv_request *this_req;
		
		if (i != count) {
			this_req = smbsrv_setup_secondary_request(req);
		} else {
			this_req = req;
		}

		SSVAL(this_req->out.vwv, VWV(0), i);
		smbsrv_send_reply(this_req);
	}
}



/****************************************************************************
 Reply to a printopen (async reply)
****************************************************************************/
static void reply_printopen_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_open *oi;

	SMBSRV_CHECK_ASYNC_STATUS(oi, union smb_open);

	/* construct reply */
	smbsrv_setup_reply(req, 1, 0);

	smbsrv_push_fnum(req->out.vwv, VWV(0), oi->openold.out.file.ntvfs);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a printopen.
****************************************************************************/
void smbsrv_reply_printopen(struct smbsrv_request *req)
{
	union smb_open *oi;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 2);
	SMBSRV_TALLOC_IO_PTR(oi, union smb_open);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_printopen_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	oi->splopen.level = RAW_OPEN_SPLOPEN;
	oi->splopen.in.setup_length = SVAL(req->in.vwv, VWV(0));
	oi->splopen.in.mode         = SVAL(req->in.vwv, VWV(1));

	req_pull_ascii4(&req->in.bufinfo, &oi->splopen.in.ident, req->in.data, STR_TERMINATE);

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_open(req->ntvfs, oi));
}

/****************************************************************************
 Reply to a printclose.
****************************************************************************/
void smbsrv_reply_printclose(struct smbsrv_request *req)
{
	union smb_close *io;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 3);
	SMBSRV_TALLOC_IO_PTR(io, union smb_close);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->splclose.level		= RAW_CLOSE_SPLCLOSE;
	io->splclose.in.file.ntvfs	= smbsrv_pull_fnum(req, req->in.vwv,  VWV(0));

	SMBSRV_CHECK_FILE_HANDLE(io->splclose.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_close(req->ntvfs, io));
}

/****************************************************************************
 Reply to a printqueue.
****************************************************************************/
static void reply_printqueue_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_lpq *lpq;
	int i, maxcount;
	const unsigned int el_size = 28;

	SMBSRV_CHECK_ASYNC_STATUS(lpq,union smb_lpq);

	/* construct reply */
	smbsrv_setup_reply(req, 2, 0);

	/* truncate the returned list to fit in the negotiated buffer size */
	maxcount = (req_max_data(req) - 3) / el_size;
	if (maxcount < lpq->retq.out.count) {
		lpq->retq.out.count = maxcount;
	}

	/* setup enough space in the reply */
	req_grow_data(req, 3 + el_size*lpq->retq.out.count);
	
	/* and fill it in */
	SSVAL(req->out.vwv, VWV(0), lpq->retq.out.count);
	SSVAL(req->out.vwv, VWV(1), lpq->retq.out.restart_idx);

	SCVAL(req->out.data, 0, SMB_DATA_BLOCK);
	SSVAL(req->out.data, 1, el_size*lpq->retq.out.count);

	req->out.ptr = req->out.data + 3;

	for (i=0;i<lpq->retq.out.count;i++) {
		srv_push_dos_date2(req->smb_conn, req->out.ptr, 0 , lpq->retq.out.queue[i].time);
		SCVAL(req->out.ptr,  4, lpq->retq.out.queue[i].status);
		SSVAL(req->out.ptr,  5, lpq->retq.out.queue[i].job);
		SIVAL(req->out.ptr,  7, lpq->retq.out.queue[i].size);
		SCVAL(req->out.ptr, 11, 0); /* reserved */
		req_push_str(req, req->out.ptr+12, lpq->retq.out.queue[i].user, 16, STR_ASCII);
		req->out.ptr += el_size;
	}

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a printqueue.
****************************************************************************/
void smbsrv_reply_printqueue(struct smbsrv_request *req)
{
	union smb_lpq *lpq;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 2);
	SMBSRV_TALLOC_IO_PTR(lpq, union smb_lpq);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_printqueue_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	lpq->retq.level = RAW_LPQ_RETQ;
	lpq->retq.in.maxcount = SVAL(req->in.vwv,  VWV(0));
	lpq->retq.in.startidx = SVAL(req->in.vwv,  VWV(1));

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_lpq(req->ntvfs, lpq));
}


/****************************************************************************
 Reply to a printwrite.
****************************************************************************/
void smbsrv_reply_printwrite(struct smbsrv_request *req)
{
	union smb_write *io;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 1);
	SMBSRV_TALLOC_IO_PTR(io, union smb_write);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	if (req->in.data_size < 3) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	io->splwrite.level		= RAW_WRITE_SPLWRITE;
	io->splwrite.in.file.ntvfs	= smbsrv_pull_fnum(req, req->in.vwv, VWV(0));
	io->splwrite.in.count		= SVAL(req->in.data, 1);
	io->splwrite.in.data		= req->in.data + 3;

	/* make sure they gave us the data they promised */
	if (req_data_oob(&req->in.bufinfo, io->splwrite.in.data, io->splwrite.in.count)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	SMBSRV_CHECK_FILE_HANDLE(io->splwrite.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_write(req->ntvfs, io));
}


/****************************************************************************
 Reply to a mkdir.
****************************************************************************/
void smbsrv_reply_mkdir(struct smbsrv_request *req)
{
	union smb_mkdir *io;

	/* parse the request */
	SMBSRV_CHECK_WCT(req, 0);
	SMBSRV_TALLOC_IO_PTR(io, union smb_mkdir);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->generic.level = RAW_MKDIR_MKDIR;
	req_pull_ascii4(&req->in.bufinfo, &io->mkdir.in.path, req->in.data, STR_TERMINATE);

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_mkdir(req->ntvfs, io));
}


/****************************************************************************
 Reply to a rmdir.
****************************************************************************/
void smbsrv_reply_rmdir(struct smbsrv_request *req)
{
	struct smb_rmdir *io;
 
	/* parse the request */
	SMBSRV_CHECK_WCT(req, 0);
	SMBSRV_TALLOC_IO_PTR(io, struct smb_rmdir);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	req_pull_ascii4(&req->in.bufinfo, &io->in.path, req->in.data, STR_TERMINATE);

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_rmdir(req->ntvfs, io));
}


/****************************************************************************
 Reply to a mv.
****************************************************************************/
void smbsrv_reply_mv(struct smbsrv_request *req)
{
	union smb_rename *io;
	uint8_t *p;
 
	/* parse the request */
	SMBSRV_CHECK_WCT(req, 1);
	SMBSRV_TALLOC_IO_PTR(io, union smb_rename);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->generic.level = RAW_RENAME_RENAME;
	io->rename.in.attrib = SVAL(req->in.vwv, VWV(0));

	p = req->in.data;
	p += req_pull_ascii4(&req->in.bufinfo, &io->rename.in.pattern1, p, STR_TERMINATE);
	p += req_pull_ascii4(&req->in.bufinfo, &io->rename.in.pattern2, p, STR_TERMINATE);

	if (!io->rename.in.pattern1 || !io->rename.in.pattern2) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_rename(req->ntvfs, io));
}


/****************************************************************************
 Reply to an NT rename.
****************************************************************************/
void smbsrv_reply_ntrename(struct smbsrv_request *req)
{
	union smb_rename *io;
	uint8_t *p;
 
	/* parse the request */
	SMBSRV_CHECK_WCT(req, 4);
	SMBSRV_TALLOC_IO_PTR(io, union smb_rename);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->generic.level = RAW_RENAME_NTRENAME;
	io->ntrename.in.attrib  = SVAL(req->in.vwv, VWV(0));
	io->ntrename.in.flags   = SVAL(req->in.vwv, VWV(1));
	io->ntrename.in.cluster_size = IVAL(req->in.vwv, VWV(2));

	p = req->in.data;
	p += req_pull_ascii4(&req->in.bufinfo, &io->ntrename.in.old_name, p, STR_TERMINATE);
	p += req_pull_ascii4(&req->in.bufinfo, &io->ntrename.in.new_name, p, STR_TERMINATE);

	if (!io->ntrename.in.old_name || !io->ntrename.in.new_name) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_rename(req->ntvfs, io));
}

/****************************************************************************
 Reply to a file copy (async reply)
****************************************************************************/
static void reply_copy_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	struct smb_copy *cp;

	SMBSRV_CHECK_ASYNC_STATUS(cp, struct smb_copy);

	/* build the reply */
	smbsrv_setup_reply(req, 1, 0);

	SSVAL(req->out.vwv, VWV(0), cp->out.count);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a file copy.
****************************************************************************/
void smbsrv_reply_copy(struct smbsrv_request *req)
{
	struct smb_copy *cp;
	uint8_t *p;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 3);
	SMBSRV_TALLOC_IO_PTR(cp, struct smb_copy);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_copy_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	cp->in.tid2  = SVAL(req->in.vwv, VWV(0));
	cp->in.ofun  = SVAL(req->in.vwv, VWV(1));
	cp->in.flags = SVAL(req->in.vwv, VWV(2));

	p = req->in.data;
	p += req_pull_ascii4(&req->in.bufinfo, &cp->in.path1, p, STR_TERMINATE);
	p += req_pull_ascii4(&req->in.bufinfo, &cp->in.path2, p, STR_TERMINATE);

	if (!cp->in.path1 || !cp->in.path2) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_copy(req->ntvfs, cp));
}

/****************************************************************************
 Reply to a lockingX request (async send)
****************************************************************************/
static void reply_lockingX_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_lock *lck;

	SMBSRV_CHECK_ASYNC_STATUS(lck, union smb_lock);

	/* if it was an oplock break ack then we only send a reply if
	   there was an error */
	if (lck->lockx.in.ulock_cnt + lck->lockx.in.lock_cnt == 0) {
		talloc_free(req);
		return;
	}

	/* construct reply */
	smbsrv_setup_reply(req, 2, 0);
	
	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);

	smbsrv_chain_reply(req);
}


/****************************************************************************
 Reply to a lockingX request.
****************************************************************************/
void smbsrv_reply_lockingX(struct smbsrv_request *req)
{
	union smb_lock *lck;
	unsigned int total_locks, i;
	unsigned int lck_size;
	uint8_t *p;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 8);
	SMBSRV_TALLOC_IO_PTR(lck, union smb_lock);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_lockingX_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	lck->lockx.level = RAW_LOCK_LOCKX;
	lck->lockx.in.file.ntvfs= smbsrv_pull_fnum(req, req->in.vwv, VWV(2));
	lck->lockx.in.mode      = SVAL(req->in.vwv, VWV(3));
	lck->lockx.in.timeout   = IVAL(req->in.vwv, VWV(4));
	lck->lockx.in.ulock_cnt = SVAL(req->in.vwv, VWV(6));
	lck->lockx.in.lock_cnt  = SVAL(req->in.vwv, VWV(7));

	total_locks = lck->lockx.in.ulock_cnt + lck->lockx.in.lock_cnt;

	/* there are two variants, one with 64 bit offsets and counts */
	if (lck->lockx.in.mode & LOCKING_ANDX_LARGE_FILES) {
		lck_size = 20;
	} else {
		lck_size = 10;		
	}

	/* make sure we got the promised data */
	if (req_data_oob(&req->in.bufinfo, req->in.data, total_locks * lck_size)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	/* allocate the locks array */
	if (total_locks) {
		lck->lockx.in.locks = talloc_array(req, struct smb_lock_entry, 
						   total_locks);
		if (lck->lockx.in.locks == NULL) {
			smbsrv_send_error(req, NT_STATUS_NO_MEMORY);
			return;
		}
	}

	p = req->in.data;

	/* construct the locks array */
	for (i=0;i<total_locks;i++) {
		uint32_t ofs_high=0, count_high=0;

		lck->lockx.in.locks[i].pid = SVAL(p, 0);

		if (lck->lockx.in.mode & LOCKING_ANDX_LARGE_FILES) {
			ofs_high   = IVAL(p, 4);
			lck->lockx.in.locks[i].offset = IVAL(p, 8);
			count_high = IVAL(p, 12);
			lck->lockx.in.locks[i].count  = IVAL(p, 16);
		} else {
			lck->lockx.in.locks[i].offset = IVAL(p, 2);
			lck->lockx.in.locks[i].count  = IVAL(p, 6);
		}
		if (ofs_high != 0 || count_high != 0) {
			lck->lockx.in.locks[i].count  |= ((uint64_t)count_high) << 32;
			lck->lockx.in.locks[i].offset |= ((uint64_t)ofs_high) << 32;
		}
		p += lck_size;
	}

	SMBSRV_CHECK_FILE_HANDLE(lck->lockx.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_lock(req->ntvfs, lck));
}

/****************************************************************************
 Reply to a SMBreadbmpx (read block multiplex) request.
****************************************************************************/
void smbsrv_reply_readbmpx(struct smbsrv_request *req)
{
	/* tell the client to not use a multiplexed read - its too broken to use */
	smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRuseSTD));
}


/****************************************************************************
 Reply to a SMBsetattrE.
****************************************************************************/
void smbsrv_reply_setattrE(struct smbsrv_request *req)
{
	union smb_setfileinfo *info;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 7);
	SMBSRV_TALLOC_IO_PTR(info, union smb_setfileinfo);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	info->setattre.level = RAW_SFILEINFO_SETATTRE;
	info->setattre.in.file.ntvfs  = smbsrv_pull_fnum(req, req->in.vwv,    VWV(0));
	info->setattre.in.create_time = srv_pull_dos_date2(req->smb_conn, req->in.vwv + VWV(1));
	info->setattre.in.access_time = srv_pull_dos_date2(req->smb_conn, req->in.vwv + VWV(3));
	info->setattre.in.write_time  = srv_pull_dos_date2(req->smb_conn, req->in.vwv + VWV(5));

	SMBSRV_CHECK_FILE_HANDLE(info->setattre.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_setfileinfo(req->ntvfs, info));
}


/****************************************************************************
 Reply to a SMBwritebmpx (write block multiplex primary) request.
****************************************************************************/
void smbsrv_reply_writebmpx(struct smbsrv_request *req)
{
	smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRuseSTD));
}


/****************************************************************************
 Reply to a SMBwritebs (write block multiplex secondary) request.
****************************************************************************/
void smbsrv_reply_writebs(struct smbsrv_request *req)
{
	smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRuseSTD));
}



/****************************************************************************
 Reply to a SMBgetattrE (async reply)
****************************************************************************/
static void reply_getattrE_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_fileinfo *info;

	SMBSRV_CHECK_ASYNC_STATUS(info, union smb_fileinfo);

	/* setup reply */
	smbsrv_setup_reply(req, 11, 0);

	srv_push_dos_date2(req->smb_conn, req->out.vwv, VWV(0), info->getattre.out.create_time);
	srv_push_dos_date2(req->smb_conn, req->out.vwv, VWV(2), info->getattre.out.access_time);
	srv_push_dos_date2(req->smb_conn, req->out.vwv, VWV(4), info->getattre.out.write_time);
	SIVAL(req->out.vwv,         VWV(6), info->getattre.out.size);
	SIVAL(req->out.vwv,         VWV(8), info->getattre.out.alloc_size);
	SSVAL(req->out.vwv,        VWV(10), info->getattre.out.attrib);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a SMBgetattrE.
****************************************************************************/
void smbsrv_reply_getattrE(struct smbsrv_request *req)
{
	union smb_fileinfo *info;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 1);
	SMBSRV_TALLOC_IO_PTR(info, union smb_fileinfo);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_getattrE_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	info->getattr.level		= RAW_FILEINFO_GETATTRE;
	info->getattr.in.file.ntvfs	= smbsrv_pull_fnum(req, req->in.vwv, VWV(0));

	SMBSRV_CHECK_FILE_HANDLE(info->getattr.in.file.ntvfs);
	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_qfileinfo(req->ntvfs, info));
}

void smbsrv_reply_sesssetup_send(struct smbsrv_request *req,
				 union smb_sesssetup *io,
				 NTSTATUS status)
{
	switch (io->old.level) {
	case RAW_SESSSETUP_OLD:
		if (!NT_STATUS_IS_OK(status)) {
			smbsrv_send_error(req, status);
			return;
		}

		/* construct reply */
		smbsrv_setup_reply(req, 3, 0);

		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv, VWV(2), io->old.out.action);

		SSVAL(req->out.hdr, HDR_UID, io->old.out.vuid);

		smbsrv_chain_reply(req);
		return;

	case RAW_SESSSETUP_NT1:
		if (!NT_STATUS_IS_OK(status)) {
			smbsrv_send_error(req, status);
			return;
		}

		/* construct reply */
		smbsrv_setup_reply(req, 3, 0);

		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv, VWV(2), io->nt1.out.action);

		SSVAL(req->out.hdr, HDR_UID, io->nt1.out.vuid);

		req_push_str(req, NULL, io->nt1.out.os, -1, STR_TERMINATE);
		req_push_str(req, NULL, io->nt1.out.lanman, -1, STR_TERMINATE);
		req_push_str(req, NULL, io->nt1.out.domain, -1, STR_TERMINATE);

		smbsrv_chain_reply(req);
		return;

	case RAW_SESSSETUP_SPNEGO:
		if (!NT_STATUS_IS_OK(status) && 
		    !NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			smbsrv_send_error(req, status);
			return;
		}

		/* construct reply */
		smbsrv_setup_reply(req, 4, io->spnego.out.secblob.length);

		if (NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			smbsrv_setup_error(req, status);
		}

		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv, VWV(2), io->spnego.out.action);
		SSVAL(req->out.vwv, VWV(3), io->spnego.out.secblob.length);

		SSVAL(req->out.hdr, HDR_UID, io->spnego.out.vuid);

		memcpy(req->out.data, io->spnego.out.secblob.data, io->spnego.out.secblob.length);
		req_push_str(req, NULL, io->spnego.out.os,        -1, STR_TERMINATE);
		req_push_str(req, NULL, io->spnego.out.lanman,    -1, STR_TERMINATE);
		req_push_str(req, NULL, io->spnego.out.workgroup, -1, STR_TERMINATE);

		smbsrv_chain_reply(req);
		return;

	case RAW_SESSSETUP_SMB2:
		break;
	}

	smbsrv_send_error(req, NT_STATUS_INTERNAL_ERROR);
}

/****************************************************************************
reply to an old style session setup command
****************************************************************************/
static void reply_sesssetup_old(struct smbsrv_request *req)
{
	uint8_t *p;
	uint16_t passlen;
	union smb_sesssetup *io;

	SMBSRV_TALLOC_IO_PTR(io, union smb_sesssetup);

	io->old.level = RAW_SESSSETUP_OLD;

	/* parse request */
	io->old.in.bufsize = SVAL(req->in.vwv, VWV(2));
	io->old.in.mpx_max = SVAL(req->in.vwv, VWV(3));
	io->old.in.vc_num  = SVAL(req->in.vwv, VWV(4));
	io->old.in.sesskey = IVAL(req->in.vwv, VWV(5));
	passlen            = SVAL(req->in.vwv, VWV(7));

	/* check the request isn't malformed */
	if (req_data_oob(&req->in.bufinfo, req->in.data, passlen)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}
	
	p = req->in.data;
	if (!req_pull_blob(&req->in.bufinfo, p, passlen, &io->old.in.password)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}
	p += passlen;
	
	p += req_pull_string(&req->in.bufinfo, &io->old.in.user,   p, -1, STR_TERMINATE);
	p += req_pull_string(&req->in.bufinfo, &io->old.in.domain, p, -1, STR_TERMINATE);
	p += req_pull_string(&req->in.bufinfo, &io->old.in.os,     p, -1, STR_TERMINATE);
	p += req_pull_string(&req->in.bufinfo, &io->old.in.lanman, p, -1, STR_TERMINATE);

	/* call the generic handler */
	smbsrv_sesssetup_backend(req, io);
}

/****************************************************************************
reply to an NT1 style session setup command
****************************************************************************/
static void reply_sesssetup_nt1(struct smbsrv_request *req)
{
	uint8_t *p;
	uint16_t passlen1, passlen2;
	union smb_sesssetup *io;

	SMBSRV_TALLOC_IO_PTR(io, union smb_sesssetup);

	io->nt1.level = RAW_SESSSETUP_NT1;

	/* parse request */
	io->nt1.in.bufsize      = SVAL(req->in.vwv, VWV(2));
	io->nt1.in.mpx_max      = SVAL(req->in.vwv, VWV(3));
	io->nt1.in.vc_num       = SVAL(req->in.vwv, VWV(4));
	io->nt1.in.sesskey      = IVAL(req->in.vwv, VWV(5));
	passlen1                = SVAL(req->in.vwv, VWV(7));
	passlen2                = SVAL(req->in.vwv, VWV(8));
	io->nt1.in.capabilities = IVAL(req->in.vwv, VWV(11));

	/* check the request isn't malformed */
	if (req_data_oob(&req->in.bufinfo, req->in.data, passlen1) ||
	    req_data_oob(&req->in.bufinfo, req->in.data + passlen1, passlen2)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}
	
	p = req->in.data;
	if (!req_pull_blob(&req->in.bufinfo, p, passlen1, &io->nt1.in.password1)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}
	p += passlen1;
	if (!req_pull_blob(&req->in.bufinfo, p, passlen2, &io->nt1.in.password2)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}
	p += passlen2;
	
	p += req_pull_string(&req->in.bufinfo, &io->nt1.in.user,   p, -1, STR_TERMINATE);
	p += req_pull_string(&req->in.bufinfo, &io->nt1.in.domain, p, -1, STR_TERMINATE);
	p += req_pull_string(&req->in.bufinfo, &io->nt1.in.os,     p, -1, STR_TERMINATE);
	p += req_pull_string(&req->in.bufinfo, &io->nt1.in.lanman, p, -1, STR_TERMINATE);

	/* call the generic handler */
	smbsrv_sesssetup_backend(req, io);
}


/****************************************************************************
reply to an SPNEGO style session setup command
****************************************************************************/
static void reply_sesssetup_spnego(struct smbsrv_request *req)
{
	uint8_t *p;
	uint16_t blob_len;
	union smb_sesssetup *io;

	SMBSRV_TALLOC_IO_PTR(io, union smb_sesssetup);

	io->spnego.level = RAW_SESSSETUP_SPNEGO;

	/* parse request */
	io->spnego.in.bufsize      = SVAL(req->in.vwv, VWV(2));
	io->spnego.in.mpx_max      = SVAL(req->in.vwv, VWV(3));
	io->spnego.in.vc_num       = SVAL(req->in.vwv, VWV(4));
	io->spnego.in.sesskey      = IVAL(req->in.vwv, VWV(5));
	blob_len                   = SVAL(req->in.vwv, VWV(7));
	io->spnego.in.capabilities = IVAL(req->in.vwv, VWV(10));

	p = req->in.data;
	if (!req_pull_blob(&req->in.bufinfo, p, blob_len, &io->spnego.in.secblob)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}
	p += blob_len;
	
	p += req_pull_string(&req->in.bufinfo, &io->spnego.in.os,        p, -1, STR_TERMINATE);
	p += req_pull_string(&req->in.bufinfo, &io->spnego.in.lanman,    p, -1, STR_TERMINATE);
	p += req_pull_string(&req->in.bufinfo, &io->spnego.in.workgroup, p, -1, STR_TERMINATE);

	/* call the generic handler */
	smbsrv_sesssetup_backend(req, io);
}


/****************************************************************************
reply to a session setup command
****************************************************************************/
void smbsrv_reply_sesssetup(struct smbsrv_request *req)
{
	switch (req->in.wct) {
	case 10:
		/* a pre-NT1 call */
		reply_sesssetup_old(req);
		return;
	case 13:
		/* a NT1 call */
		reply_sesssetup_nt1(req);
		return;
	case 12:
		/* a SPNEGO call */
		reply_sesssetup_spnego(req);
		return;
	}

	/* unsupported variant */
	smbsrv_send_error(req, NT_STATUS_FOOBAR);
}

/****************************************************************************
 Reply to a exit. This closes all files open by a smbpid
****************************************************************************/
void smbsrv_reply_exit(struct smbsrv_request *req)
{
	struct smbsrv_handle_session_item *i, *ni;
	struct smbsrv_handle *h;
	struct smbsrv_tcon *tcon;
	uint16_t smbpid;

	SMBSRV_CHECK_WCT(req, 0);

	smbpid = SVAL(req->in.hdr,HDR_PID);

	/* first destroy all handles, which have the same PID as the request */
	for (i=req->session->handles; i; i=ni) {
		ni = i->next;
		h = i->handle;
		if (h->smbpid != smbpid) continue;

		talloc_free(h);
	}

	/*
	 * then let the ntvfs backends proxy the call if they want to,
	 * but we didn't check the return value of the backends,
	 * as for the SMB client the call succeed
	 */
	for (tcon=req->smb_conn->smb_tcons.list;tcon;tcon=tcon->next) {
		req->tcon = tcon;
		SMBSRV_SETUP_NTVFS_REQUEST(NULL,0);
		ntvfs_exit(req->ntvfs);
		talloc_free(req->ntvfs);
		req->ntvfs = NULL;
		req->tcon = NULL;
	}

	smbsrv_setup_reply(req, 0, 0);
	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a SMBulogoffX.
****************************************************************************/
void smbsrv_reply_ulogoffX(struct smbsrv_request *req)
{
	struct smbsrv_handle_session_item *i, *ni;
	struct smbsrv_handle *h;
	struct smbsrv_tcon *tcon;

	SMBSRV_CHECK_WCT(req, 2);

	/*
	 * TODO: cancel all pending requests
	 */
	

	/* destroy all handles */
	for (i=req->session->handles; i; i=ni) {
		ni = i->next;
		h = i->handle;
		talloc_free(h);
	}

	/*
	 * then let the ntvfs backends proxy the call if they want to,
	 * but we didn't check the return value of the backends,
	 * as for the SMB client the call succeed
	 */
	for (tcon=req->smb_conn->smb_tcons.list;tcon;tcon=tcon->next) {
		req->tcon = tcon;
		SMBSRV_SETUP_NTVFS_REQUEST(NULL,0);
		ntvfs_logoff(req->ntvfs);
		talloc_free(req->ntvfs);
		req->ntvfs = NULL;
		req->tcon = NULL;
	}

	talloc_free(req->session);
	req->session = NULL; /* it is now invalid, don't use on 
				any chained packets */

	smbsrv_setup_reply(req, 2, 0);

	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);	

	smbsrv_chain_reply(req);
}

/****************************************************************************
 Reply to an SMBfindclose request
****************************************************************************/
void smbsrv_reply_findclose(struct smbsrv_request *req)
{
	union smb_search_close *io;

	/* parse request */
	SMBSRV_CHECK_WCT(req, 1);
	SMBSRV_TALLOC_IO_PTR(io, union smb_search_close);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_simple_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->findclose.level	= RAW_FINDCLOSE_FINDCLOSE;
	io->findclose.in.handle	= SVAL(req->in.vwv, VWV(0));

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_search_close(req->ntvfs, io));
}

/****************************************************************************
 Reply to an SMBfindnclose request
****************************************************************************/
void smbsrv_reply_findnclose(struct smbsrv_request *req)
{
	smbsrv_send_error(req, NT_STATUS_FOOBAR);
}


/****************************************************************************
 Reply to an SMBntcreateX request (async send)
****************************************************************************/
static void reply_ntcreate_and_X_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_open *io;

	SMBSRV_CHECK_ASYNC_STATUS(io, union smb_open);

	/* construct reply */
	smbsrv_setup_reply(req, 34, 0);

	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);	
	SCVAL(req->out.vwv, VWV(2), io->ntcreatex.out.oplock_level);

	/* the rest of the parameters are not aligned! */
	smbsrv_push_fnum(req->out.vwv, 5, io->ntcreatex.out.file.ntvfs);
	SIVAL(req->out.vwv,        7, io->ntcreatex.out.create_action);
	push_nttime(req->out.vwv, 11, io->ntcreatex.out.create_time);
	push_nttime(req->out.vwv, 19, io->ntcreatex.out.access_time);
	push_nttime(req->out.vwv, 27, io->ntcreatex.out.write_time);
	push_nttime(req->out.vwv, 35, io->ntcreatex.out.change_time);
	SIVAL(req->out.vwv,       43, io->ntcreatex.out.attrib);
	SBVAL(req->out.vwv,       47, io->ntcreatex.out.alloc_size);
	SBVAL(req->out.vwv,       55, io->ntcreatex.out.size);
	SSVAL(req->out.vwv,       63, io->ntcreatex.out.file_type);
	SSVAL(req->out.vwv,       65, io->ntcreatex.out.ipc_state);
	SCVAL(req->out.vwv,       67, io->ntcreatex.out.is_directory);

	req->chained_fnum = SVAL(req->out.vwv, 5);

	smbsrv_chain_reply(req);
}

/****************************************************************************
 Reply to an SMBntcreateX request
****************************************************************************/
void smbsrv_reply_ntcreate_and_X(struct smbsrv_request *req)
{
	union smb_open *io;
	uint16_t fname_len;

	/* parse the request */
	SMBSRV_CHECK_WCT(req, 24);
	SMBSRV_TALLOC_IO_PTR(io, union smb_open);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_ntcreate_and_X_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->ntcreatex.level = RAW_OPEN_NTCREATEX;

	/* notice that the word parameters are not word aligned, so we don't use VWV() */
	fname_len =                         SVAL(req->in.vwv, 5);
	io->ntcreatex.in.flags =            IVAL(req->in.vwv, 7);
	io->ntcreatex.in.root_fid.ntvfs =   smbsrv_pull_fnum(req, req->in.vwv, 11);
	io->ntcreatex.in.access_mask =      IVAL(req->in.vwv, 15);
	io->ntcreatex.in.alloc_size =       BVAL(req->in.vwv, 19);
	io->ntcreatex.in.file_attr =        IVAL(req->in.vwv, 27);
	io->ntcreatex.in.share_access =     IVAL(req->in.vwv, 31);
	io->ntcreatex.in.open_disposition = IVAL(req->in.vwv, 35);
	io->ntcreatex.in.create_options =   IVAL(req->in.vwv, 39);
	io->ntcreatex.in.impersonation =    IVAL(req->in.vwv, 43);
	io->ntcreatex.in.security_flags =   CVAL(req->in.vwv, 47);
	io->ntcreatex.in.ea_list          = NULL;
	io->ntcreatex.in.sec_desc         = NULL;
	io->ntcreatex.in.query_maximal_access = false;
	io->ntcreatex.in.private_flags    = 0;

	/* we need a neater way to handle this alignment */
	if ((req->flags2 & FLAGS2_UNICODE_STRINGS) && 
	    ucs2_align(req->in.buffer, req->in.data, STR_TERMINATE|STR_UNICODE)) {
		fname_len++;
	}

	req_pull_string(&req->in.bufinfo, &io->ntcreatex.in.fname, req->in.data, fname_len, STR_TERMINATE);
	if (!io->ntcreatex.in.fname) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_open(req->ntvfs, io));
}


/****************************************************************************
 Reply to an SMBntcancel request
****************************************************************************/
void smbsrv_reply_ntcancel(struct smbsrv_request *req)
{
	struct smbsrv_request *r;
	uint16_t tid = SVAL(req->in.hdr,HDR_TID);
	uint16_t uid = SVAL(req->in.hdr,HDR_UID);
	uint16_t mid = SVAL(req->in.hdr,HDR_MID);
	uint16_t pid = SVAL(req->in.hdr,HDR_PID);

	for (r = req->smb_conn->requests; r; r = r->next) {
		if (tid != SVAL(r->in.hdr,HDR_TID)) continue;
		if (uid != SVAL(r->in.hdr,HDR_UID)) continue;
		if (mid != SVAL(r->in.hdr,HDR_MID)) continue;
		if (pid != SVAL(r->in.hdr,HDR_PID)) continue;

		SMBSRV_CHECK(ntvfs_cancel(r->ntvfs));

		/* NOTE: this request does not generate a reply */
		talloc_free(req);
		return;
	}

	/* TODO: workout the correct error code,
	 *       until we know how the smb signing works
	 *       for ntcancel replies, don't send an error
	 */
	/*smbsrv_send_error(req, NT_STATUS_FOOBAR);*/
	talloc_free(req);
}

/*
  parse the called/calling names from session request
*/
static NTSTATUS parse_session_request(struct smbsrv_request *req)
{
	DATA_BLOB blob;
	NTSTATUS status;
	
	blob.data = req->in.buffer + 4;
	blob.length = ascii_len_n((const char *)blob.data, req->in.size - PTR_DIFF(blob.data, req->in.buffer));
	if (blob.length == 0) return NT_STATUS_BAD_NETWORK_NAME;

	req->smb_conn->negotiate.called_name  = talloc(req->smb_conn, struct nbt_name);
	req->smb_conn->negotiate.calling_name = talloc(req->smb_conn, struct nbt_name);
	if (req->smb_conn->negotiate.called_name == NULL ||
	    req->smb_conn->negotiate.calling_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = nbt_name_from_blob(req->smb_conn, &blob,
				    req->smb_conn->negotiate.called_name);
	NT_STATUS_NOT_OK_RETURN(status);

	blob.data += blob.length;
	blob.length = ascii_len_n((const char *)blob.data, req->in.size - PTR_DIFF(blob.data, req->in.buffer));
	if (blob.length == 0) return NT_STATUS_BAD_NETWORK_NAME;

	status = nbt_name_from_blob(req->smb_conn, &blob,
				    req->smb_conn->negotiate.calling_name);
	NT_STATUS_NOT_OK_RETURN(status);

	req->smb_conn->negotiate.done_nbt_session = true;

	return NT_STATUS_OK;
}	



/****************************************************************************
 Reply to a special message - a SMB packet with non zero NBT message type
****************************************************************************/
void smbsrv_reply_special(struct smbsrv_request *req)
{
	uint8_t msg_type;
	uint8_t *buf = talloc_zero_array(req, uint8_t, 4);

	msg_type = CVAL(req->in.buffer,0);

	SIVAL(buf, 0, 0);
	
	switch (msg_type) {
	case 0x81: /* session request */
		if (req->smb_conn->negotiate.done_nbt_session) {
			DEBUG(0,("Warning: ignoring secondary session request\n"));
			return;
		}
		
		SCVAL(buf,0,0x82);
		SCVAL(buf,3,0);

		/* we don't check the status - samba always accepts session
		   requests for any name */
		parse_session_request(req);

		req->out.buffer = buf;
		req->out.size = 4;
		smbsrv_send_reply_nosign(req);
		return;
		
	case 0x89: /* session keepalive request 
		      (some old clients produce this?) */
		SCVAL(buf, 0, SMBkeepalive);
		SCVAL(buf, 3, 0);
		req->out.buffer = buf;
		req->out.size = 4;
		smbsrv_send_reply_nosign(req);
		return;
		
	case SMBkeepalive: 
		/* session keepalive - swallow it */
		talloc_free(req);
		return;
	}

	DEBUG(0,("Unexpected NBT session packet (%d)\n", msg_type));
	talloc_free(req);
}
