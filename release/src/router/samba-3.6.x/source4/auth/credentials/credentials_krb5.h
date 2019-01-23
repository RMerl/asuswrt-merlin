/* 
   samba -- Unix SMB/CIFS implementation.

   Client credentials structure

   Copyright (C) Jelmer Vernooij 2004-2006
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005

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

#ifndef __CREDENTIALS_KRB5_H__
#define __CREDENTIALS_KRB5_H__

#include <gssapi/gssapi.h>
#include <gssapi/gssapi_krb5.h>
#include <krb5.h>

struct gssapi_creds_container {
	gss_cred_id_t creds;
};

/* Manually prototyped here to avoid needing gss headers in most callers */
int cli_credentials_set_client_gss_creds(struct cli_credentials *cred, 
					 struct loadparm_context *lp_ctx,
					 gss_cred_id_t gssapi_cred,
					 enum credentials_obtained obtained,
					 const char **error_string);

/* Manually prototyped here to avoid needing krb5 headers in most callers */
krb5_error_code principal_from_credentials(TALLOC_CTX *parent_ctx, 
					   struct cli_credentials *credentials, 
					   struct smb_krb5_context *smb_krb5_context,
					   krb5_principal *princ,
					   enum credentials_obtained *obtained,
					   const char **error_string);
krb5_error_code impersonate_principal_from_credentials(TALLOC_CTX *parent_ctx,
						       struct cli_credentials *credentials,
						       struct smb_krb5_context *smb_krb5_context,
						       krb5_principal *princ,
						       const char **error_string);
	
void cli_credentials_invalidate_client_gss_creds(struct cli_credentials *cred, 
						 enum credentials_obtained obtained);

#endif /* __CREDENTIALS_KRB5_H__ */
