/* 
   Unix SMB/CIFS implementation.

   implement the DRSUpdateRefs call

   Copyright (C) Andrew Tridgell 2009
   
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
#include "dsdb/samdb/samdb.h"
#include "libcli/security/security.h"
#include "libcli/security/session.h"
#include "rpc_server/drsuapi/dcesrv_drsuapi.h"
#include "auth/session.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"

struct repsTo {
	uint32_t count;
	struct repsFromToBlob *r;
};

/*
  add a replication destination for a given partition GUID
 */
static WERROR uref_add_dest(struct ldb_context *sam_ctx, TALLOC_CTX *mem_ctx, 
			    struct ldb_dn *dn, struct repsFromTo1 *dest, 
			    uint32_t options)
{
	struct repsTo reps;
	WERROR werr;
	unsigned int i;

	werr = dsdb_loadreps(sam_ctx, mem_ctx, dn, "repsTo", &reps.r, &reps.count);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	for (i=0; i<reps.count; i++) {
		if (GUID_compare(&dest->source_dsa_obj_guid, 
				 &reps.r[i].ctr.ctr1.source_dsa_obj_guid) == 0) {
			if (options & DRSUAPI_DRS_GETCHG_CHECK) {
				return WERR_OK;
			} else {
				return WERR_DS_DRA_REF_ALREADY_EXISTS;
			}
		}
	}

	reps.r = talloc_realloc(mem_ctx, reps.r, struct repsFromToBlob, reps.count+1);
	if (reps.r == NULL) {
		return WERR_DS_DRA_INTERNAL_ERROR;
	}
	ZERO_STRUCT(reps.r[reps.count]);
	reps.r[reps.count].version = 1;
	reps.r[reps.count].ctr.ctr1 = *dest;
	/* add the GCSPN flag if the client asked for it */
	reps.r[reps.count].ctr.ctr1.replica_flags |= (options & DRSUAPI_DRS_REF_GCSPN);
	reps.count++;

	werr = dsdb_savereps(sam_ctx, mem_ctx, dn, "repsTo", reps.r, reps.count);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	return WERR_OK;	
}

/*
  delete a replication destination for a given partition GUID
 */
static WERROR uref_del_dest(struct ldb_context *sam_ctx, TALLOC_CTX *mem_ctx, 
			    struct ldb_dn *dn, struct GUID *dest_guid, 
			    uint32_t options)
{
	struct repsTo reps;
	WERROR werr;
	unsigned int i;
	bool found = false;

	werr = dsdb_loadreps(sam_ctx, mem_ctx, dn, "repsTo", &reps.r, &reps.count);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	for (i=0; i<reps.count; i++) {
		if (GUID_compare(dest_guid, &reps.r[i].ctr.ctr1.source_dsa_obj_guid) == 0) {
			if (i+1 < reps.count) {
				memmove(&reps.r[i], &reps.r[i+1], sizeof(reps.r[i])*(reps.count-(i+1)));
			}
			reps.count--;
			found = true;
		}
	}

	werr = dsdb_savereps(sam_ctx, mem_ctx, dn, "repsTo", reps.r, reps.count);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	if (!found &&
	    !(options & DRSUAPI_DRS_GETCHG_CHECK) &&
	    !(options & DRSUAPI_DRS_ADD_REF)) {
		return WERR_DS_DRA_REF_NOT_FOUND;
	}

	return WERR_OK;	
}

/* 
  drsuapi_DsReplicaUpdateRefs - a non RPC version callable from getncchanges
*/
WERROR drsuapi_UpdateRefs(struct drsuapi_bind_state *b_state, TALLOC_CTX *mem_ctx,
			  struct drsuapi_DsReplicaUpdateRefsRequest1 *req)
{
	WERROR werr;
	struct ldb_dn *dn;
	struct ldb_context *sam_ctx = b_state->sam_ctx_system?b_state->sam_ctx_system:b_state->sam_ctx;

	DEBUG(4,("DsReplicaUpdateRefs for host '%s' with GUID %s options 0x%08x nc=%s\n",
		 req->dest_dsa_dns_name, GUID_string(mem_ctx, &req->dest_dsa_guid),
		 req->options,
		 drs_ObjectIdentifier_to_string(mem_ctx, req->naming_context)));

	dn = ldb_dn_new(mem_ctx, sam_ctx, req->naming_context->dn);
	if (dn == NULL) {
		return WERR_DS_INVALID_DN_SYNTAX;
	}

	if (ldb_transaction_start(sam_ctx) != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to start transaction on samdb: %s\n",
			 ldb_errstring(sam_ctx)));
		return WERR_DS_DRA_INTERNAL_ERROR;		
	}

	if (req->options & DRSUAPI_DRS_DEL_REF) {
		werr = uref_del_dest(sam_ctx, mem_ctx, dn, &req->dest_dsa_guid, req->options);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0,("Failed to delete repsTo for %s: %s\n",
				 GUID_string(mem_ctx, &req->dest_dsa_guid),
				 win_errstr(werr)));
			goto failed;
		}
	}

	if (req->options & DRSUAPI_DRS_ADD_REF) {
		struct repsFromTo1 dest;
		struct repsFromTo1OtherInfo oi;
		
		ZERO_STRUCT(dest);
		ZERO_STRUCT(oi);

		oi.dns_name = req->dest_dsa_dns_name;
		dest.other_info          = &oi;
		dest.source_dsa_obj_guid = req->dest_dsa_guid;
		dest.replica_flags       = req->options;

		werr = uref_add_dest(sam_ctx, mem_ctx, dn, &dest, req->options);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0,("Failed to add repsTo for %s: %s\n",
				 GUID_string(mem_ctx, &dest.source_dsa_obj_guid),
				 win_errstr(werr)));
			goto failed;
		}
	}

	if (ldb_transaction_commit(sam_ctx) != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to commit transaction on samdb: %s\n",
			 ldb_errstring(sam_ctx)));
		return WERR_DS_DRA_INTERNAL_ERROR;		
	}

	return WERR_OK;

failed:
	ldb_transaction_cancel(sam_ctx);
	return werr;
}

/* 
  drsuapi_DsReplicaUpdateRefs
*/
WERROR dcesrv_drsuapi_DsReplicaUpdateRefs(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					  struct drsuapi_DsReplicaUpdateRefs *r)
{
	struct dcesrv_handle *h;
	struct drsuapi_bind_state *b_state;
	struct drsuapi_DsReplicaUpdateRefsRequest1 *req;
	WERROR werr;
	int ret;
	enum security_user_level security_level;

	DCESRV_PULL_HANDLE_WERR(h, r->in.bind_handle, DRSUAPI_BIND_HANDLE);
	b_state = h->data;

	if (r->in.level != 1) {
		DEBUG(0,("DrReplicUpdateRefs - unsupported level %u\n", r->in.level));
		return WERR_DS_DRA_INVALID_PARAMETER;
	}
	req = &r->in.req.req1;
	werr = drs_security_access_check(b_state->sam_ctx,
					 mem_ctx,
					 dce_call->conn->auth_state.session_info->security_token,
					 req->naming_context,
					 GUID_DRS_MANAGE_TOPOLOGY);

	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	security_level = security_session_user_level(dce_call->conn->auth_state.session_info, NULL);
	if (security_level < SECURITY_ADMINISTRATOR) {
		/* check that they are using an DSA objectGUID that they own */
		ret = dsdb_validate_dsa_guid(b_state->sam_ctx,
		                             &req->dest_dsa_guid,
		                             &dce_call->conn->auth_state.session_info->security_token->sids[PRIMARY_USER_SID_INDEX]);
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ ": Refusing DsReplicaUpdateRefs for sid %s with GUID %s\n",
				 dom_sid_string(mem_ctx,
						&dce_call->conn->auth_state.session_info->security_token->sids[PRIMARY_USER_SID_INDEX]),
				 GUID_string(mem_ctx, &req->dest_dsa_guid)));
			return WERR_DS_DRA_ACCESS_DENIED;
		}
	}

	werr = drsuapi_UpdateRefs(b_state, mem_ctx, req);

#if 0
	NDR_PRINT_FUNCTION_DEBUG(drsuapi_DsReplicaUpdateRefs, NDR_BOTH, r);
#endif

	return werr;
}
