/*
   Unix SMB/CIFS implementation.
   SMB Transport encryption code.
   Copyright (C) Jeremy Allison 2007.

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

#ifndef _HEADER_SMB_CRYPT_H
#define _HEADER_SMB_CRYPT_H

#if HAVE_GSSAPI_GSSAPI_H
#include <gssapi/gssapi.h>
#elif HAVE_GSSAPI_GSSAPI_GENERIC_H
#include <gssapi/gssapi_generic.h>
#elif HAVE_GSSAPI_H
#include <gssapi.h>
#endif

#if HAVE_COM_ERR_H
#include <com_err.h>
#endif

/* Transport encryption state. */
enum smb_trans_enc_type {
		SMB_TRANS_ENC_NTLM
#if defined(HAVE_GSSAPI) && defined(HAVE_KRB5)
		, SMB_TRANS_ENC_GSS
#endif
};

#if defined(HAVE_GSSAPI) && defined(HAVE_KRB5)
struct smb_tran_enc_state_gss {
        gss_ctx_id_t gss_ctx;
        gss_cred_id_t creds;
};
#endif

struct smb_trans_enc_state {
        enum smb_trans_enc_type smb_enc_type;
        uint16 enc_ctx_num;
        bool enc_on;
        union {
                struct ntlmssp_state *ntlmssp_state;
#if defined(HAVE_GSSAPI) && defined(HAVE_KRB5)
                struct smb_tran_enc_state_gss *gss_state;
#endif
        } s;
};

#endif /* _HEADER_SMB_CRYPT_H */
