/* 
   Unix SMB/CIFS implementation.
   client file read/write routines
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) James Myers 2003
   
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
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"

#define SETUP_REQUEST(cmd, wct, buflen) do { \
	req = smbcli_request_setup(tree, cmd, wct, buflen); \
	if (!req) return NULL; \
} while (0)

/****************************************************************************
 low level read operation (async send)
****************************************************************************/
_PUBLIC_ struct smbcli_request *smb_raw_read_send(struct smbcli_tree *tree, union smb_read *parms)
{
	bool bigoffset = false;
	struct smbcli_request *req = NULL; 

	switch (parms->generic.level) {
	case RAW_READ_READBRAW:
		if (tree->session->transport->negotiate.capabilities & CAP_LARGE_FILES) {
			bigoffset = true;
		}
		SETUP_REQUEST(SMBreadbraw, bigoffset? 10:8, 0);
		SSVAL(req->out.vwv, VWV(0), parms->readbraw.in.file.fnum);
		SIVAL(req->out.vwv, VWV(1), parms->readbraw.in.offset);
		SSVAL(req->out.vwv, VWV(3), parms->readbraw.in.maxcnt);
		SSVAL(req->out.vwv, VWV(4), parms->readbraw.in.mincnt);
		SIVAL(req->out.vwv, VWV(5), parms->readbraw.in.timeout);
		SSVAL(req->out.vwv, VWV(7), 0); /* reserved */
		if (bigoffset) {
			SIVAL(req->out.vwv, VWV(8),parms->readbraw.in.offset>>32);
		}
		break;

	case RAW_READ_LOCKREAD:
		SETUP_REQUEST(SMBlockread, 5, 0);
		SSVAL(req->out.vwv, VWV(0), parms->lockread.in.file.fnum);
		SSVAL(req->out.vwv, VWV(1), parms->lockread.in.count);
		SIVAL(req->out.vwv, VWV(2), parms->lockread.in.offset);
		SSVAL(req->out.vwv, VWV(4), parms->lockread.in.remaining);
		break;

	case RAW_READ_READ:
		SETUP_REQUEST(SMBread, 5, 0);
		SSVAL(req->out.vwv, VWV(0), parms->read.in.file.fnum);
		SSVAL(req->out.vwv, VWV(1), parms->read.in.count);
		SIVAL(req->out.vwv, VWV(2), parms->read.in.offset);
		SSVAL(req->out.vwv, VWV(4), parms->read.in.remaining);
		break;

	case RAW_READ_READX:
		if (tree->session->transport->negotiate.capabilities & CAP_LARGE_FILES) {
			bigoffset = true;
		}
		SETUP_REQUEST(SMBreadX, bigoffset ? 12 : 10, 0);
		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv, VWV(2), parms->readx.in.file.fnum);
		SIVAL(req->out.vwv, VWV(3), parms->readx.in.offset);
		SSVAL(req->out.vwv, VWV(5), parms->readx.in.maxcnt & 0xFFFF);
		SSVAL(req->out.vwv, VWV(6), parms->readx.in.mincnt);
		SIVAL(req->out.vwv, VWV(7), parms->readx.in.maxcnt >> 16);
		SSVAL(req->out.vwv, VWV(9), parms->readx.in.remaining);
		/*
		 * TODO: give an error when the offset is 64 bit
		 *       and the server doesn't support it
		 */
		if (bigoffset) {
			SIVAL(req->out.vwv, VWV(10),parms->readx.in.offset>>32);
		}
		if (parms->readx.in.read_for_execute) {
			uint16_t flags2 = SVAL(req->out.hdr, HDR_FLG2);
			flags2 |= FLAGS2_READ_PERMIT_EXECUTE;
			SSVAL(req->out.hdr, HDR_FLG2, flags2);
		}
		break;

	case RAW_READ_SMB2:
		return NULL;
	}

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	/* the transport layer needs to know that a readbraw is pending
	   and handle receives a little differently */
	if (parms->generic.level == RAW_READ_READBRAW) {
		tree->session->transport->readbraw_pending = 1;
	}

	return req;
}

/****************************************************************************
 low level read operation (async recv)
****************************************************************************/
_PUBLIC_ NTSTATUS smb_raw_read_recv(struct smbcli_request *req, union smb_read *parms)
{
	if (!smbcli_request_receive(req) ||
	    smbcli_request_is_error(req)) {
		goto failed;
	}

	switch (parms->generic.level) {
	case RAW_READ_READBRAW:
		parms->readbraw.out.nread = req->in.size - NBT_HDR_SIZE;
		if (parms->readbraw.out.nread > 
		    MAX(parms->readx.in.mincnt, parms->readx.in.maxcnt)) {
			req->status = NT_STATUS_BUFFER_TOO_SMALL;
			goto failed;
		}
		memcpy(parms->readbraw.out.data, req->in.buffer + NBT_HDR_SIZE, parms->readbraw.out.nread);
		break;
		
	case RAW_READ_LOCKREAD:
		SMBCLI_CHECK_WCT(req, 5);
		parms->lockread.out.nread = SVAL(req->in.vwv, VWV(0));
		if (parms->lockread.out.nread > parms->lockread.in.count ||
		    !smbcli_raw_pull_data(&req->in.bufinfo, req->in.data+3, 
				       parms->lockread.out.nread, parms->lockread.out.data)) {
			req->status = NT_STATUS_BUFFER_TOO_SMALL;
		}
		break;

	case RAW_READ_READ:
		/* there are 4 reserved words in the reply */
		SMBCLI_CHECK_WCT(req, 5);
		parms->read.out.nread = SVAL(req->in.vwv, VWV(0));
		if (parms->read.out.nread > parms->read.in.count ||
		    !smbcli_raw_pull_data(&req->in.bufinfo, req->in.data+3, 
				       parms->read.out.nread, parms->read.out.data)) {
			req->status = NT_STATUS_BUFFER_TOO_SMALL;
		}
		break;

	case RAW_READ_READX:
		/* there are 5 reserved words in the reply */
		SMBCLI_CHECK_WCT(req, 12);
		parms->readx.out.remaining       = SVAL(req->in.vwv, VWV(2));
		parms->readx.out.compaction_mode = SVAL(req->in.vwv, VWV(3));
		parms->readx.out.nread = SVAL(req->in.vwv, VWV(5));

		/* handle oversize replies for non-chained readx replies with
		   CAP_LARGE_READX. The snia spec has must to answer for. */
		if ((req->tree->session->transport->negotiate.capabilities & CAP_LARGE_READX)
		    && CVAL(req->in.vwv, VWV(0)) == SMB_CHAIN_NONE &&
		    req->in.size >= 0x10000) {
			parms->readx.out.nread += (SVAL(req->in.vwv, VWV(7)) << 16);
			if (req->in.hdr + SVAL(req->in.vwv, VWV(6)) +
			    parms->readx.out.nread <= 
			    req->in.buffer + req->in.size) {
				req->in.data_size += (SVAL(req->in.vwv, VWV(7)) << 16);

				/* update the bufinfo with the new size */
				smb_setup_bufinfo(req);
			}
		}

		if (parms->readx.out.nread > MAX(parms->readx.in.mincnt, parms->readx.in.maxcnt) ||
		    !smbcli_raw_pull_data(&req->in.bufinfo, req->in.hdr + SVAL(req->in.vwv, VWV(6)), 
				       parms->readx.out.nread, 
				       parms->readx.out.data)) {
			req->status = NT_STATUS_BUFFER_TOO_SMALL;
		}
		break;

	case RAW_READ_SMB2:
		req->status = NT_STATUS_INTERNAL_ERROR;
		break;
	}

failed:
	return smbcli_request_destroy(req);
}

/****************************************************************************
 low level read operation (sync interface)
****************************************************************************/
_PUBLIC_ NTSTATUS smb_raw_read(struct smbcli_tree *tree, union smb_read *parms)
{
	struct smbcli_request *req = smb_raw_read_send(tree, parms);
	return smb_raw_read_recv(req, parms);
}


/****************************************************************************
 raw write interface (async send)
****************************************************************************/
_PUBLIC_ struct smbcli_request *smb_raw_write_send(struct smbcli_tree *tree, union smb_write *parms)
{
	bool bigoffset = false;
	struct smbcli_request *req = NULL; 

	switch (parms->generic.level) {
	case RAW_WRITE_WRITEUNLOCK:
		SETUP_REQUEST(SMBwriteunlock, 5, 3 + parms->writeunlock.in.count);
		SSVAL(req->out.vwv, VWV(0), parms->writeunlock.in.file.fnum);
		SSVAL(req->out.vwv, VWV(1), parms->writeunlock.in.count);
		SIVAL(req->out.vwv, VWV(2), parms->writeunlock.in.offset);
		SSVAL(req->out.vwv, VWV(4), parms->writeunlock.in.remaining);
		SCVAL(req->out.data, 0, SMB_DATA_BLOCK);
		SSVAL(req->out.data, 1, parms->writeunlock.in.count);
		if (parms->writeunlock.in.count > 0) {
			memcpy(req->out.data+3, parms->writeunlock.in.data, 
			       parms->writeunlock.in.count);
		}
		break;

	case RAW_WRITE_WRITE:
		SETUP_REQUEST(SMBwrite, 5,  3 + parms->write.in.count);
		SSVAL(req->out.vwv, VWV(0), parms->write.in.file.fnum);
		SSVAL(req->out.vwv, VWV(1), parms->write.in.count);
		SIVAL(req->out.vwv, VWV(2), parms->write.in.offset);
		SSVAL(req->out.vwv, VWV(4), parms->write.in.remaining);
		SCVAL(req->out.data, 0, SMB_DATA_BLOCK);
		SSVAL(req->out.data, 1, parms->write.in.count);
		if (parms->write.in.count > 0) {
			memcpy(req->out.data+3, parms->write.in.data, parms->write.in.count);
		}
		break;

	case RAW_WRITE_WRITECLOSE:
		SETUP_REQUEST(SMBwriteclose, 6, 1 + parms->writeclose.in.count);
		SSVAL(req->out.vwv, VWV(0), parms->writeclose.in.file.fnum);
		SSVAL(req->out.vwv, VWV(1), parms->writeclose.in.count);
		SIVAL(req->out.vwv, VWV(2), parms->writeclose.in.offset);
		raw_push_dos_date3(tree->session->transport,
				  req->out.vwv, VWV(4), parms->writeclose.in.mtime);
		SCVAL(req->out.data, 0, 0);
		if (parms->writeclose.in.count > 0) {
			memcpy(req->out.data+1, parms->writeclose.in.data, 
			       parms->writeclose.in.count);
		}
		break;

	case RAW_WRITE_WRITEX:
		if (tree->session->transport->negotiate.capabilities & CAP_LARGE_FILES) {
			bigoffset = true;
		}
		SETUP_REQUEST(SMBwriteX, bigoffset ? 14 : 12, parms->writex.in.count);
		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv, VWV(2), parms->writex.in.file.fnum);
		SIVAL(req->out.vwv, VWV(3), parms->writex.in.offset);
		SIVAL(req->out.vwv, VWV(5), 0); /* reserved */
		SSVAL(req->out.vwv, VWV(7), parms->writex.in.wmode);
		SSVAL(req->out.vwv, VWV(8), parms->writex.in.remaining);
		SSVAL(req->out.vwv, VWV(9), parms->writex.in.count>>16);
		SSVAL(req->out.vwv, VWV(10), parms->writex.in.count);
		SSVAL(req->out.vwv, VWV(11), PTR_DIFF(req->out.data, req->out.hdr));
		if (bigoffset) {
	      		SIVAL(req->out.vwv,VWV(12),parms->writex.in.offset>>32);
		}
	      	if (parms->writex.in.count > 0) {
			memcpy(req->out.data, parms->writex.in.data, parms->writex.in.count);
		}
		break;

	case RAW_WRITE_SPLWRITE:
		SETUP_REQUEST(SMBsplwr, 1, parms->splwrite.in.count);
		SSVAL(req->out.vwv, VWV(0), parms->splwrite.in.file.fnum);
		if (parms->splwrite.in.count > 0) {
			memcpy(req->out.data, parms->splwrite.in.data, parms->splwrite.in.count);
		}
		break;

	case RAW_WRITE_SMB2:
		return NULL;
	}

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}


/****************************************************************************
 raw write interface (async recv)
****************************************************************************/
NTSTATUS smb_raw_write_recv(struct smbcli_request *req, union smb_write *parms)
{
	if (!smbcli_request_receive(req) ||
	    !NT_STATUS_IS_OK(req->status)) {
		goto failed;
	}

	switch (parms->generic.level) {
	case RAW_WRITE_WRITEUNLOCK:
		SMBCLI_CHECK_WCT(req, 1);		
		parms->writeunlock.out.nwritten = SVAL(req->in.vwv, VWV(0));
		break;
	case RAW_WRITE_WRITE:
		SMBCLI_CHECK_WCT(req, 1);
		parms->write.out.nwritten = SVAL(req->in.vwv, VWV(0));
		break;
	case RAW_WRITE_WRITECLOSE:
		SMBCLI_CHECK_WCT(req, 1);
		parms->writeclose.out.nwritten = SVAL(req->in.vwv, VWV(0));
		break;
	case RAW_WRITE_WRITEX:
		SMBCLI_CHECK_WCT(req, 6);
		parms->writex.out.nwritten  = SVAL(req->in.vwv, VWV(2));
		parms->writex.out.nwritten += (CVAL(req->in.vwv, VWV(4)) << 16);
		parms->writex.out.remaining = SVAL(req->in.vwv, VWV(3));
		break;
	case RAW_WRITE_SPLWRITE:
		break;
	case RAW_WRITE_SMB2:
		req->status = NT_STATUS_INTERNAL_ERROR;
		break;
	}

failed:
	return smbcli_request_destroy(req);
}

/****************************************************************************
 raw write interface (sync interface)
****************************************************************************/
_PUBLIC_ NTSTATUS smb_raw_write(struct smbcli_tree *tree, union smb_write *parms)
{
	struct smbcli_request *req = smb_raw_write_send(tree, parms);
	return smb_raw_write_recv(req, parms);
}
