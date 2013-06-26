/* 
   Unix SMB/CIFS implementation.

   CLDAP server structures

   Copyright (C) Andrew Tridgell	2005
   
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

#include "libcli/cldap/cldap.h"
#include "libcli/ldap/libcli_ldap.h"

/*
  top level context structure for the cldap server
*/
struct cldapd_server {
	struct task_server *task;
	struct ldb_context *samctx;
};

struct ldap_SearchRequest;

/* used by netlogon DCE/RPC server */
NTSTATUS fill_netlogon_samlogon_response(struct ldb_context *sam_ctx,
                                         TALLOC_CTX *mem_ctx,
                                         const char *domain,
                                         const char *netbios_domain,
                                         struct dom_sid *domain_sid,
                                         const char *domain_guid,
                                         const char *user,
                                         uint32_t acct_control,
                                         const char *src_address,
                                         uint32_t version,
                                         struct loadparm_context *lp_ctx,
                                         struct netlogon_samlogon_response *netlogon,
					 bool fill_on_blank_request);

#include "cldap_server/proto.h"
