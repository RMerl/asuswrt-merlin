/* 
   Unix SMB/CIFS implementation.

   endpoint server for the lsarpc pipe

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005
   
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
#include "rpc_server/dcerpc_server.h"
#include "rpc_server/common/common.h"
#include "auth/auth.h"
#include "dsdb/samdb/samdb.h"
#include "libcli/ldap/ldap_ndr.h"
#include "lib/ldb/include/ldb_errors.h"
#include "libcli/security/security.h"
#include "libcli/auth/libcli_auth.h"
#include "param/secrets.h"
#include "../lib/util/util_ldb.h"
#include "librpc/gen_ndr/ndr_dssetup.h"
#include "param/param.h"

/*
  state associated with a lsa_OpenPolicy() operation
*/
struct lsa_policy_state {
	struct dcesrv_handle *handle;
	struct ldb_context *sam_ldb;
	uint32_t access_mask;
	struct ldb_dn *domain_dn;
	struct ldb_dn *forest_dn;
	struct ldb_dn *builtin_dn;
	struct ldb_dn *system_dn;
	const char *domain_name;
	const char *domain_dns;
	const char *forest_dns;
	struct dom_sid *domain_sid;
	struct GUID domain_guid;
	struct dom_sid *builtin_sid;
	struct dom_sid *nt_authority_sid;
	struct dom_sid *creator_owner_domain_sid;
	struct dom_sid *world_domain_sid;
	int mixed_domain;
};

enum lsa_handle {
	LSA_HANDLE_POLICY,
	LSA_HANDLE_ACCOUNT,
	LSA_HANDLE_SECRET,
	LSA_HANDLE_TRUSTED_DOMAIN
};

#include "rpc_server/lsa/proto.h"

