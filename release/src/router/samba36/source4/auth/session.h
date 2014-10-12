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

#include "librpc/gen_ndr/security.h"
#include "librpc/gen_ndr/netlogon.h"
#include "librpc/gen_ndr/auth.h"

struct tevent_context;
struct ldb_context;
struct ldb_dn;
/* Create a security token for a session SYSTEM (the most
 * trusted/prvilaged account), including the local machine account as
 * the off-host credentials */
struct auth_session_info *system_session(struct loadparm_context *lp_ctx) ;

NTSTATUS auth_anonymous_user_info_dc(TALLOC_CTX *mem_ctx,
					     const char *netbios_name,
					     struct auth_user_info_dc **interim_info);
NTSTATUS auth_generate_session_info(TALLOC_CTX *mem_ctx,
				    struct loadparm_context *lp_ctx, /* Optional, if you don't want privilages */
				    struct ldb_context *sam_ctx, /* Optional, if you don't want local groups */
				    struct auth_user_info_dc *interim_info,
				    uint32_t session_info_flags,
				    struct auth_session_info **session_info);
NTSTATUS auth_anonymous_session_info(TALLOC_CTX *parent_ctx, 
				     struct loadparm_context *lp_ctx,
				     struct auth_session_info **session_info);
struct auth_session_info *auth_session_info_from_transport(TALLOC_CTX *mem_ctx,
							   struct auth_session_info_transport *session_info_transport,
							   struct loadparm_context *lp_ctx,
							   const char **reason);
NTSTATUS auth_session_info_transport_from_session(TALLOC_CTX *mem_ctx,
						  struct auth_session_info *session_info,
						  struct tevent_context *event_ctx,
						  struct loadparm_context *lp_ctx,
						  struct auth_session_info_transport **transport_out);

/* Produce a session_info for an arbitary DN or principal in the local
 * DB, assuming the local DB holds all the groups
 *
 * Supply either a principal or a DN
 */
NTSTATUS authsam_get_session_info_principal(TALLOC_CTX *mem_ctx,
					    struct loadparm_context *lp_ctx,
					    struct ldb_context *sam_ctx,
					    const char *principal,
					    struct ldb_dn *user_dn,
					    uint32_t session_info_flags,
					    struct auth_session_info **session_info);

struct auth_session_info *anonymous_session(TALLOC_CTX *mem_ctx, 
					    struct loadparm_context *lp_ctx);

struct auth_session_info *admin_session(TALLOC_CTX *mem_ctx,
					struct loadparm_context *lp_ctx,
					struct dom_sid *domain_sid);


#endif /* _SAMBA_AUTH_SESSION_H */
