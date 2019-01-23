/* 
   Unix SMB/CIFS implementation.
   SMB client session context management functions

   Copyright (C) Andrew Tridgell 1994-2005
   Copyright (C) James Myers 2003 <myersjj@samba.org>
   
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
#include "system/filesys.h"

#define SETUP_REQUEST_SESSION(cmd, wct, buflen) do { \
	req = smbcli_request_setup_session(session, cmd, wct, buflen); \
	if (!req) return NULL; \
} while (0)


/****************************************************************************
 Initialize the session context
****************************************************************************/
struct smbcli_session *smbcli_session_init(struct smbcli_transport *transport, 
					   TALLOC_CTX *parent_ctx, bool primary,
					   struct smbcli_session_options options)
{
	struct smbcli_session *session;
	uint16_t flags2;
	uint32_t capabilities;

	session = talloc_zero(parent_ctx, struct smbcli_session);
	if (!session) {
		return NULL;
	}

	if (primary) {
		session->transport = talloc_steal(session, transport);
	} else {
		session->transport = talloc_reference(session, transport);
	}
	session->pid = (uint16_t)getpid();
	session->vuid = UID_FIELD_INVALID;
	session->options = options;
	
	capabilities = transport->negotiate.capabilities;

	flags2 = FLAGS2_LONG_PATH_COMPONENTS | FLAGS2_EXTENDED_ATTRIBUTES;

	if (capabilities & CAP_UNICODE) {
		flags2 |= FLAGS2_UNICODE_STRINGS;
	}
	if (capabilities & CAP_STATUS32) {
		flags2 |= FLAGS2_32_BIT_ERROR_CODES;
	}
	if (capabilities & CAP_EXTENDED_SECURITY) {
		flags2 |= FLAGS2_EXTENDED_SECURITY;
	}
	if (session->transport->negotiate.sign_info.doing_signing) {
		flags2 |= FLAGS2_SMB_SECURITY_SIGNATURES;
	}

	session->flags2 = flags2;

	return session;
}

/****************************************************************************
 Perform a session setup (async send)
****************************************************************************/
struct smbcli_request *smb_raw_sesssetup_send(struct smbcli_session *session, 
					      union smb_sesssetup *parms) 
{
	struct smbcli_request *req = NULL;

	switch (parms->old.level) {
	case RAW_SESSSETUP_OLD:
		SETUP_REQUEST_SESSION(SMBsesssetupX, 10, 0);
		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv,VWV(2),parms->old.in.bufsize);
		SSVAL(req->out.vwv,VWV(3),parms->old.in.mpx_max);
		SSVAL(req->out.vwv,VWV(4),parms->old.in.vc_num);
		SIVAL(req->out.vwv,VWV(5),parms->old.in.sesskey);
		SSVAL(req->out.vwv,VWV(7),parms->old.in.password.length);
		SIVAL(req->out.vwv,VWV(8), 0); /* reserved */
		smbcli_req_append_blob(req, &parms->old.in.password);
		smbcli_req_append_string(req, parms->old.in.user, STR_TERMINATE);
		smbcli_req_append_string(req, parms->old.in.domain, STR_TERMINATE|STR_UPPER);
		smbcli_req_append_string(req, parms->old.in.os, STR_TERMINATE);
		smbcli_req_append_string(req, parms->old.in.lanman, STR_TERMINATE);
		break;

	case RAW_SESSSETUP_NT1:
		SETUP_REQUEST_SESSION(SMBsesssetupX, 13, 0);
		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv, VWV(2), parms->nt1.in.bufsize);
		SSVAL(req->out.vwv, VWV(3), parms->nt1.in.mpx_max);
		SSVAL(req->out.vwv, VWV(4), parms->nt1.in.vc_num);
		SIVAL(req->out.vwv, VWV(5), parms->nt1.in.sesskey);
		SSVAL(req->out.vwv, VWV(7), parms->nt1.in.password1.length);
		SSVAL(req->out.vwv, VWV(8), parms->nt1.in.password2.length);
		SIVAL(req->out.vwv, VWV(9), 0); /* reserved */
		SIVAL(req->out.vwv, VWV(11), parms->nt1.in.capabilities);
		smbcli_req_append_blob(req, &parms->nt1.in.password1);
		smbcli_req_append_blob(req, &parms->nt1.in.password2);
		smbcli_req_append_string(req, parms->nt1.in.user, STR_TERMINATE);
		smbcli_req_append_string(req, parms->nt1.in.domain, STR_TERMINATE|STR_UPPER);
		smbcli_req_append_string(req, parms->nt1.in.os, STR_TERMINATE);
		smbcli_req_append_string(req, parms->nt1.in.lanman, STR_TERMINATE);
		break;

	case RAW_SESSSETUP_SPNEGO:
		SETUP_REQUEST_SESSION(SMBsesssetupX, 12, 0);
		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);
		SSVAL(req->out.vwv, VWV(2), parms->spnego.in.bufsize);
		SSVAL(req->out.vwv, VWV(3), parms->spnego.in.mpx_max);
		SSVAL(req->out.vwv, VWV(4), parms->spnego.in.vc_num);
		SIVAL(req->out.vwv, VWV(5), parms->spnego.in.sesskey);
		SSVAL(req->out.vwv, VWV(7), parms->spnego.in.secblob.length);
		SIVAL(req->out.vwv, VWV(8), 0); /* reserved */
		SIVAL(req->out.vwv, VWV(10), parms->spnego.in.capabilities);
		smbcli_req_append_blob(req, &parms->spnego.in.secblob);
		smbcli_req_append_string(req, parms->spnego.in.os, STR_TERMINATE);
		smbcli_req_append_string(req, parms->spnego.in.lanman, STR_TERMINATE);
		smbcli_req_append_string(req, parms->spnego.in.workgroup, STR_TERMINATE);
		break;

	case RAW_SESSSETUP_SMB2:
		return NULL;
	}

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}


/****************************************************************************
 Perform a session setup (async recv)
****************************************************************************/
NTSTATUS smb_raw_sesssetup_recv(struct smbcli_request *req, 
				TALLOC_CTX *mem_ctx, 
				union smb_sesssetup *parms) 
{
	uint16_t len;
	uint8_t *p;

	if (!smbcli_request_receive(req)) {
		return smbcli_request_destroy(req);
	}
	
	if (!NT_STATUS_IS_OK(req->status) &&
	    !NT_STATUS_EQUAL(req->status,NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		return smbcli_request_destroy(req);
	}

	switch (parms->old.level) {
	case RAW_SESSSETUP_OLD:
		SMBCLI_CHECK_WCT(req, 3);
		ZERO_STRUCT(parms->old.out);
		parms->old.out.vuid = SVAL(req->in.hdr, HDR_UID);
		parms->old.out.action = SVAL(req->in.vwv, VWV(2));
		p = req->in.data;
		if (p) {
			p += smbcli_req_pull_string(&req->in.bufinfo, mem_ctx, &parms->old.out.os, p, -1, STR_TERMINATE);
			p += smbcli_req_pull_string(&req->in.bufinfo, mem_ctx, &parms->old.out.lanman, p, -1, STR_TERMINATE);
			p += smbcli_req_pull_string(&req->in.bufinfo, mem_ctx, &parms->old.out.domain, p, -1, STR_TERMINATE);
		}
		break;

	case RAW_SESSSETUP_NT1:
		SMBCLI_CHECK_WCT(req, 3);
		ZERO_STRUCT(parms->nt1.out);
		parms->nt1.out.vuid   = SVAL(req->in.hdr, HDR_UID);
		parms->nt1.out.action = SVAL(req->in.vwv, VWV(2));
		p = req->in.data;
		if (p) {
			p += smbcli_req_pull_string(&req->in.bufinfo, mem_ctx, &parms->nt1.out.os, p, -1, STR_TERMINATE);
			p += smbcli_req_pull_string(&req->in.bufinfo, mem_ctx, &parms->nt1.out.lanman, p, -1, STR_TERMINATE);
			if (p < (req->in.data + req->in.data_size)) {
				p += smbcli_req_pull_string(&req->in.bufinfo, mem_ctx, &parms->nt1.out.domain, p, -1, STR_TERMINATE);
			}
		}
		break;

	case RAW_SESSSETUP_SPNEGO:
		SMBCLI_CHECK_WCT(req, 4);
		ZERO_STRUCT(parms->spnego.out);
		parms->spnego.out.vuid   = SVAL(req->in.hdr, HDR_UID);
		parms->spnego.out.action = SVAL(req->in.vwv, VWV(2));
		len                      = SVAL(req->in.vwv, VWV(3));
		p = req->in.data;
		if (!p) {
			break;
		}

		parms->spnego.out.secblob = smbcli_req_pull_blob(&req->in.bufinfo, mem_ctx, p, len);
		p += parms->spnego.out.secblob.length;
		p += smbcli_req_pull_string(&req->in.bufinfo, mem_ctx, &parms->spnego.out.os, p, -1, STR_TERMINATE);
		p += smbcli_req_pull_string(&req->in.bufinfo, mem_ctx, &parms->spnego.out.lanman, p, -1, STR_TERMINATE);
		p += smbcli_req_pull_string(&req->in.bufinfo, mem_ctx, &parms->spnego.out.workgroup, p, -1, STR_TERMINATE);
		break;

	case RAW_SESSSETUP_SMB2:
		req->status = NT_STATUS_INTERNAL_ERROR;
		break;
	}

failed:
	return smbcli_request_destroy(req);
}


/*
 Perform a session setup (sync interface)
*/
NTSTATUS smb_raw_sesssetup(struct smbcli_session *session, 
			   TALLOC_CTX *mem_ctx, union smb_sesssetup *parms) 
{
	struct smbcli_request *req = smb_raw_sesssetup_send(session, parms);
	return smb_raw_sesssetup_recv(req, mem_ctx, parms);
}


/****************************************************************************
 Send a ulogoff (async send)
*****************************************************************************/
struct smbcli_request *smb_raw_ulogoff_send(struct smbcli_session *session)
{
	struct smbcli_request *req;

	SETUP_REQUEST_SESSION(SMBulogoffX, 2, 0);

	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}

/****************************************************************************
 Send a ulogoff (sync interface)
*****************************************************************************/
NTSTATUS smb_raw_ulogoff(struct smbcli_session *session)
{
	struct smbcli_request *req = smb_raw_ulogoff_send(session);
	return smbcli_request_simple_recv(req);
}


/****************************************************************************
 Send a exit (async send)
*****************************************************************************/
struct smbcli_request *smb_raw_exit_send(struct smbcli_session *session)
{
	struct smbcli_request *req;

	SETUP_REQUEST_SESSION(SMBexit, 0, 0);

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}

/****************************************************************************
 Send a exit (sync interface)
*****************************************************************************/
_PUBLIC_ NTSTATUS smb_raw_exit(struct smbcli_session *session)
{
	struct smbcli_request *req = smb_raw_exit_send(session);
	return smbcli_request_simple_recv(req);
}
