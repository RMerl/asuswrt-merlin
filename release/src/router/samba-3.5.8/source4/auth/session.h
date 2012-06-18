/* 
   Unix SMB/CIFS implementation.
   Process and provide the logged on user's authorization token
   Copyright (C) Andrew Bartlett   2001
   Copyright (C) Stefan Metzmacher 2005
   
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

#ifndef _SAMBA_AUTH_SESSION_H
#define _SAMBA_AUTH_SESSION_H

struct auth_session_info {
	struct security_token *security_token;
	struct auth_serversupplied_info *server_info;
	DATA_BLOB session_key;
	struct cli_credentials *credentials;
};

#include "librpc/gen_ndr/netlogon.h"

struct tevent_context;

/* Create a security token for a session SYSTEM (the most
 * trusted/prvilaged account), including the local machine account as
 * the off-host credentials */
struct auth_session_info *system_session(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx) ;

/*
 * Create a system session, but with anonymous credentials (so we do
 * not need to open secrets.ldb) 
 */
struct auth_session_info *system_session_anon(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx);


NTSTATUS auth_anonymous_server_info(TALLOC_CTX *mem_ctx, 
				    const char *netbios_name,
				    struct auth_serversupplied_info **_server_info) ;
NTSTATUS auth_generate_session_info(TALLOC_CTX *mem_ctx, 
				    struct tevent_context *event_ctx,
				    struct loadparm_context *lp_ctx,
				    struct auth_serversupplied_info *server_info, 
				    struct auth_session_info **_session_info) ;

NTSTATUS auth_anonymous_session_info(TALLOC_CTX *parent_ctx, 
				     struct tevent_context *ev_ctx,
				     struct loadparm_context *lp_ctx,
				     struct auth_session_info **_session_info);

struct auth_session_info *anonymous_session(TALLOC_CTX *mem_ctx, 
					    struct tevent_context *event_ctx,
					    struct loadparm_context *lp_ctx);

struct auth_session_info *admin_session(TALLOC_CTX *mem_ctx,
					struct loadparm_context *lp_ctx,
					struct dom_sid *domain_sid);


#endif /* _SAMBA_AUTH_SESSION_H */
