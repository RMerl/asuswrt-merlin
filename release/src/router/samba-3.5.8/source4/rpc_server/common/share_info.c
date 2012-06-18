/* 
   Unix SMB/CIFS implementation.

   common share info functions

   Copyright (C) Stefan (metze) Metzmacher 2004
   
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
#include "param/share.h"
#include "librpc/gen_ndr/srvsvc.h"
#include "rpc_server/dcerpc_server.h"

/* 
    Here are common server info functions used by some dcerpc server interfaces
*/

/* This hardcoded value should go into a ldb database! */
uint32_t dcesrv_common_get_share_permissions(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx, struct share_config *scfg)
{
	return 0;
}

/* This hardcoded value should go into a ldb database! */
uint32_t dcesrv_common_get_share_current_users(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx, struct share_config *scfg)
{
	return 1;
}

/* This hardcoded value should go into a ldb database! */
enum srvsvc_ShareType dcesrv_common_get_share_type(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx, struct share_config *scfg)
{
	/* for disk share	0x00000000
	 * for print share	0x00000001
	 * for IPC$ share	0x00000003 
	 *
	 * administrative shares:
	 * ADMIN$, IPC$, C$, D$, E$ ...  are type |= 0x80000000
	 * this ones are hidden in NetShareEnum, but shown in NetShareEnumAll
	 */
	enum srvsvc_ShareType share_type = 0;
	const char *sharetype;

	if (!share_bool_option(scfg, SHARE_BROWSEABLE, SHARE_BROWSEABLE_DEFAULT)) {
		share_type |= STYPE_HIDDEN;
	}

	sharetype = share_string_option(scfg, SHARE_TYPE, SHARE_TYPE_DEFAULT);
	if (sharetype && strcasecmp(sharetype, "IPC") == 0) {
		share_type |= STYPE_IPC;
		return share_type;
	}

	if (sharetype && strcasecmp(sharetype, "PRINTER") == 0) {
		share_type |= STYPE_PRINTQ;
		return share_type;
	}

	share_type |= STYPE_DISKTREE;

	return share_type;
}

/* This hardcoded value should go into a ldb database! */
const char *dcesrv_common_get_share_path(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx, struct share_config *scfg)
{
	const char *sharetype;
	char *p;
	
	sharetype = share_string_option(scfg, SHARE_TYPE, SHARE_TYPE_DEFAULT);
	
	if (sharetype && strcasecmp(sharetype, "IPC") == 0) {
		return talloc_strdup(mem_ctx, "");
	}

	p = talloc_strdup(mem_ctx, share_string_option(scfg, SHARE_PATH, ""));
	if (!p) {
		return NULL;
	}
	if (p[0] == '\0') {
		return p;
	}
	all_string_sub(p, "/", "\\", 0);
	
	return talloc_asprintf(mem_ctx, "C:%s", p);
}

/* This hardcoded value should go into a ldb database! */
uint32_t dcesrv_common_get_share_dfs_flags(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx, struct share_config *scfg)
{
	return 0;
}

/* This hardcoded value should go into a ldb database! */
struct security_descriptor *dcesrv_common_get_security_descriptor(TALLOC_CTX *mem_ctx, struct dcesrv_context *dce_ctx, struct share_config *scfg)
{
	return NULL;
}
