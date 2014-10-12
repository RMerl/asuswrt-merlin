/*
 * Unix SMB/CIFS implementation.
 * collected prototypes header
 *
 * frozen from "make proto" in May 2008
 *
 * Copyright (C) Michael Adam 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _NTLM_AUTH_PROTO_H_
#define _NTLM_AUTH_PROTO_H_


/* The following definitions come from utils/ntlm_auth.c  */

const char *get_winbind_domain(void);
const char *get_winbind_netbios_name(void);
DATA_BLOB get_challenge(void) ;
NTSTATUS contact_winbind_auth_crap(const char *username,
				   const char *domain,
				   const char *workstation,
				   const DATA_BLOB *challenge,
				   const DATA_BLOB *lm_response,
				   const DATA_BLOB *nt_response,
				   uint32 flags,
				   uint8 lm_key[8],
				   uint8 user_session_key[16],
				   char **error_string,
				   char **unix_name);

/* The following definitions come from utils/ntlm_auth_diagnostics.c  */

bool diagnose_ntlm_auth(void);
int get_pam_winbind_config(void);

#endif /*  _NTLM_AUTH_PROTO_H_  */
