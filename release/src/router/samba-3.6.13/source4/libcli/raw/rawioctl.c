/* 
   Unix SMB/CIFS implementation.
   client file operations
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

#include "includes.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"

#define SETUP_REQUEST(cmd, wct, buflen) do { \
	req = smbcli_request_setup(tree, cmd, wct, buflen); \
	if (!req) return NULL; \
} while (0)

/* 
   send a raw smb ioctl - async send
*/
static struct smbcli_request *smb_raw_smbioctl_send(struct smbcli_tree *tree, 
						 union smb_ioctl *parms)
{
	struct smbcli_request *req; 

	SETUP_REQUEST(SMBioctl, 3, 0);

	SSVAL(req->out.vwv, VWV(0), parms->ioctl.in.file.fnum);
	SIVAL(req->out.vwv, VWV(1), parms->ioctl.in.request);

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}

/* 
   send a raw smb ioctl - async recv
*/
static NTSTATUS smb_raw_smbioctl_recv(struct smbcli_request *req, 
				      TALLOC_CTX *mem_ctx, 
				      union smb_ioctl *parms)
{
	if (!smbcli_request_receive(req) ||
	    smbcli_request_is_error(req)) {
		return smbcli_request_destroy(req);
	}

	parms->ioctl.out.blob = smbcli_req_pull_blob(&req->in.bufinfo, mem_ctx, req->in.data, -1);
	return smbcli_request_destroy(req);
}



/****************************************************************************
NT ioctl (async send)
****************************************************************************/
static struct smbcli_request *smb_raw_ntioctl_send(struct smbcli_tree *tree, 
						union smb_ioctl *parms)
{
	struct smb_nttrans nt;
	uint8_t setup[8];

	nt.in.max_setup = 4;
	nt.in.max_param = 0;
	nt.in.max_data = parms->ntioctl.in.max_data;
	nt.in.setup_count = 4;
	nt.in.setup = setup;
	SIVAL(setup, 0, parms->ntioctl.in.function);
	SSVAL(setup, 4, parms->ntioctl.in.file.fnum);
	SCVAL(setup, 6, parms->ntioctl.in.fsctl);
	SCVAL(setup, 7, parms->ntioctl.in.filter);
	nt.in.function = NT_TRANSACT_IOCTL;
	nt.in.params = data_blob(NULL, 0);
	nt.in.data = parms->ntioctl.in.blob;

	return smb_raw_nttrans_send(tree, &nt);
}

/****************************************************************************
NT ioctl (async recv)
****************************************************************************/
static NTSTATUS smb_raw_ntioctl_recv(struct smbcli_request *req, 
				     TALLOC_CTX *mem_ctx,
				     union smb_ioctl *parms)
{
	NTSTATUS status;
	struct smb_nttrans nt;
	TALLOC_CTX *tmp_mem;

	tmp_mem = talloc_new(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(tmp_mem);

	status = smb_raw_nttrans_recv(req, tmp_mem, &nt);
	if (!NT_STATUS_IS_OK(status)) goto fail;

	parms->ntioctl.out.blob = nt.out.data;
	talloc_steal(mem_ctx, parms->ntioctl.out.blob.data);

fail:
	talloc_free(tmp_mem);
	return status;
}


/* 
   send a raw ioctl - async send
*/
struct smbcli_request *smb_raw_ioctl_send(struct smbcli_tree *tree, union smb_ioctl *parms)
{
	struct smbcli_request *req = NULL;
	
	switch (parms->generic.level) {
	case RAW_IOCTL_IOCTL:
		req = smb_raw_smbioctl_send(tree, parms);
		break;
		
	case RAW_IOCTL_NTIOCTL:
		req = smb_raw_ntioctl_send(tree, parms);
		break;

	case RAW_IOCTL_SMB2:
	case RAW_IOCTL_SMB2_NO_HANDLE:
		return NULL;
	}

	return req;
}

/* 
   recv a raw ioctl - async recv
*/
NTSTATUS smb_raw_ioctl_recv(struct smbcli_request *req,
			    TALLOC_CTX *mem_ctx, union smb_ioctl *parms)
{
	switch (parms->generic.level) {
	case RAW_IOCTL_IOCTL:
		return smb_raw_smbioctl_recv(req, mem_ctx, parms);
		
	case RAW_IOCTL_NTIOCTL:
		return smb_raw_ntioctl_recv(req, mem_ctx, parms);

	case RAW_IOCTL_SMB2:
	case RAW_IOCTL_SMB2_NO_HANDLE:
		break;
	}
	return NT_STATUS_INVALID_LEVEL;
}

/* 
   send a raw ioctl - sync interface
*/
NTSTATUS smb_raw_ioctl(struct smbcli_tree *tree, 
		TALLOC_CTX *mem_ctx, union smb_ioctl *parms)
{
	struct smbcli_request *req;
	req = smb_raw_ioctl_send(tree, parms);
	return smb_raw_ioctl_recv(req, mem_ctx, parms);
}
