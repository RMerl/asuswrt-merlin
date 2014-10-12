/* 
   Unix SMB/CIFS implementation.
   error packet handling
   Copyright (C) Andrew Tridgell 1992-1998
   
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

/* From lib/error.c */
extern struct unix_error_map unix_dos_nt_errmap[];

bool use_nt_status(void)
{
	return lp_nt_status_support() && (global_client_caps & CAP_STATUS32);
}

/****************************************************************************
 Create an error packet. Normally called using the ERROR() macro.

 Setting eclass and ecode to zero and status to a valid NT error will
 reply with an NT error if the client supports CAP_STATUS32, otherwise
 it maps to and returns a DOS error if the client doesn't support CAP_STATUS32.
 This is the normal mode of calling this function via reply_nterror(req, status).

 Setting eclass and ecode to non-zero and status to NT_STATUS_OK (0) will map
 from a DOS error to an NT error and reply with an NT error if the client
 supports CAP_STATUS32, otherwise it replies with the given DOS error.
 This mode is currently not used in the server.

 Setting both eclass, ecode and status to non-zero values allows a non-default
 mapping from NT error codes to DOS error codes, and will return one or the
 other depending on the client supporting CAP_STATUS32 or not. This is the
 path taken by calling reply_botherror(req, eclass, ecode, status);

 Setting status to NT_STATUS_DOS(eclass, ecode) forces DOS errors even if the
 client supports CAP_STATUS32. This is the path taken to force a DOS error
 reply by calling reply_force_doserror(req, eclass, ecode).

 Setting status only and eclass to -1 forces NT errors even if the client
 doesn't support CAP_STATUS32. This mode is currently never used in the
 server.
****************************************************************************/

void error_packet_set(char *outbuf, uint8 eclass, uint32 ecode, NTSTATUS ntstatus, int line, const char *file)
{
	bool force_nt_status = False;
	bool force_dos_status = False;

	if (eclass == (uint8)-1) {
		force_nt_status = True;
	} else if (NT_STATUS_IS_DOS(ntstatus)) {
		force_dos_status = True;
	}

	if (force_nt_status || (!force_dos_status && lp_nt_status_support() && (global_client_caps & CAP_STATUS32))) {
		/* We're returning an NT error. */
		if (NT_STATUS_V(ntstatus) == 0 && eclass) {
			ntstatus = dos_to_ntstatus(eclass, ecode);
		}
		SIVAL(outbuf,smb_rcls,NT_STATUS_V(ntstatus));
		SSVAL(outbuf,smb_flg2, SVAL(outbuf,smb_flg2)|FLAGS2_32_BIT_ERROR_CODES);
		DEBUG(3,("error packet at %s(%d) cmd=%d (%s) %s\n",
			 file, line,
			 (int)CVAL(outbuf,smb_com),
			 smb_fn_name(CVAL(outbuf,smb_com)),
			 nt_errstr(ntstatus)));
	} else {
		/* We're returning a DOS error only. */
		if (NT_STATUS_IS_DOS(ntstatus)) {
			eclass = NT_STATUS_DOS_CLASS(ntstatus);
			ecode = NT_STATUS_DOS_CODE(ntstatus);
		} else 	if (eclass == 0 && NT_STATUS_V(ntstatus)) {
			ntstatus_to_dos(ntstatus, &eclass, &ecode);
		}

		SSVAL(outbuf,smb_flg2, SVAL(outbuf,smb_flg2)&~FLAGS2_32_BIT_ERROR_CODES);
		SSVAL(outbuf,smb_rcls,eclass);
		SSVAL(outbuf,smb_err,ecode);  

		DEBUG(3,("error packet at %s(%d) cmd=%d (%s) eclass=%d ecode=%d\n",
			  file, line,
			  (int)CVAL(outbuf,smb_com),
			  smb_fn_name(CVAL(outbuf,smb_com)),
			  eclass,
			  ecode));
	}
}

int error_packet(char *outbuf, uint8 eclass, uint32 ecode, NTSTATUS ntstatus, int line, const char *file)
{
	int outsize = srv_set_message(outbuf,0,0,True);
	error_packet_set(outbuf, eclass, ecode, ntstatus, line, file);
	return outsize;
}

void reply_nt_error(struct smb_request *req, NTSTATUS ntstatus,
		    int line, const char *file)
{
	TALLOC_FREE(req->outbuf);
	reply_outbuf(req, 0, 0);
	error_packet_set((char *)req->outbuf, 0, 0, ntstatus, line, file);
}

/****************************************************************************
 Forces a DOS error on the wire.
****************************************************************************/

void reply_force_dos_error(struct smb_request *req, uint8 eclass, uint32 ecode,
		    int line, const char *file)
{
	TALLOC_FREE(req->outbuf);
	reply_outbuf(req, 0, 0);
	error_packet_set((char *)req->outbuf,
			eclass, ecode,
			NT_STATUS_DOS(eclass, ecode),
			line,
			file);
}

void reply_both_error(struct smb_request *req, uint8 eclass, uint32 ecode,
		      NTSTATUS status, int line, const char *file)
{
	TALLOC_FREE(req->outbuf);
	reply_outbuf(req, 0, 0);
	error_packet_set((char *)req->outbuf, eclass, ecode, status,
			 line, file);
}

void reply_openerror(struct smb_request *req, NTSTATUS status)
{
	if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_COLLISION)) {
		/*
		 * We hit an existing file, and if we're returning DOS
		 * error codes OBJECT_NAME_COLLISION would map to
		 * ERRDOS/183, we need to return ERRDOS/80, see bug
		 * 4852.
		 */
		reply_botherror(req, NT_STATUS_OBJECT_NAME_COLLISION,
			ERRDOS, ERRfilexists);
	} else if (NT_STATUS_EQUAL(status, NT_STATUS_TOO_MANY_OPENED_FILES)) {
		/* EMFILE always seems to be returned as a DOS error.
		 * See bug 6837. NOTE this forces a DOS error on the wire
		 * even though it's calling reply_nterror(). */
		reply_force_doserror(req, ERRDOS, ERRnofids);
	} else {
		reply_nterror(req, status);
	}
}
