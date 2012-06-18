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
#include "auth/auth.h"
#include "rpc_server/drsuapi/dcesrv_drsuapi.h"
#include "libcli/security/security.h"

struct repsTo {
	uint32_t count;
	struct repsFromToBlob *r;
};

/*
  add a replication destination for a given partition GUID
 */
static WERROR uref_add_dest(struct ldb_context *sam_ctx, TALLOC_CTX *mem_ctx, 
			    struct ldb_dn *dn, struct repsFromTo1 *dest)
{
	struct repsTo reps;
	WERROR werr;

	werr = dsdb_loadreps(sam_ctx, mem_ctx, dn, "repsTo", &reps.r, &reps.count);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	reps.r = talloc_realloc(mem_ctx, reps.r, struct repsFromToBlob, reps.count+1);
	if (reps.r == NULL) {
		return WERR_DS_DRA_INTERNAL_ERROR;
	}
	ZERO_STRUCT(reps.r[reps.count]);
	reps.r[reps.count].version = 1;
	reps.r[reps.count].ctr.ctr1 = *dest;
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
			    struct ldb_dn *dn, struct GUID *dest_guid)
{
	struct repsTo reps;
	WERROR werr;
	int i;

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
		}
	}

	werr = dsdb_savereps(sam_ctx, mem_ctx, dn, "repsTo", reps.r, reps.count);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	return WERR_OK;	
}

/* 
  drsuapi_DsReplicaUpdateRefs
*/
WERROR dcesrv_drsuapi_DsReplicaUpdateRefs(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					  struct drsuapi_DsReplicaUpdateRefs *r)
{
	struct drsuapi_DsReplicaUpdateRefsRequest1 *req;
	struct ldb_context *sam_ctx;
	WERROR werr;
	struct ldb_dn *dn;

	werr = drs_security_level_check(dce_call, "DsReplicaUpdateRefs");
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	if (r->in.level != 1) {
		DEBUG(0,("DrReplicUpdateRefs - unsupported level %u\n", r->in.level));
		return WERR_DS_DRA_INVALID_PARAMETER;
	}

	req = &r->in.req.req1;
	DEBUG(4,("DsReplicaUpdateRefs for host '%s' with GUID %s options 0x%08x nc=%s\n",
		 req->dest_dsa_dns_name, GUID_string(mem_ctx, &req->dest_dsa_guid),
		 req->options,
		 drs_ObjectIdentifier_to_string(mem_ctx, req->naming_context)));

	/* TODO: We need to authenticate this operation pretty carefully */
	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, 
				system_session(mem_ctx, dce_call->conn->dce_ctx->lp_ctx));
	if (!sam_ctx) {
		return WERR_DS_DRA_INTERNAL_ERROR;		
	}

	dn = ldb_dn_new(mem_ctx, sam_ctx, req->naming_context->dn);
	if (dn == NULL) {
		talloc_free(sam_ctx);		
		return WERR_DS_INVALID_DN_SYNTAX;
	}

	if (ldb_transaction_start(sam_ctx) != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to start transaction on samdb\n"));
		talloc_free(sam_ctx);
		return WERR_DS_DRA_INTERNAL_ERROR;		
	}

	if (req->options & DRSUAPI_DS_REPLICA_UPDATE_DELETE_REFERENCE) {
		werr = uref_del_dest(sam_ctx, mem_ctx, dn, &req->dest_dsa_guid);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0,("Failed to delete repsTo for %s\n",
				 GUID_string(dce_call, &req->dest_dsa_guid)));
			goto failed;
		}
	}

	if (req->options & DRSUAPI_DS_REPLICA_UPDATE_ADD_REFERENCE) {
		struct repsFromTo1 dest;
		struct repsFromTo1OtherInfo oi;
		
		ZERO_STRUCT(dest);
		ZERO_STRUCT(oi);

		oi.dns_name = req->dest_dsa_dns_name;
		dest.other_info          = &oi;
		dest.source_dsa_obj_guid = req->dest_dsa_guid;
		dest.replica_flags       = req->options;

		werr = uref_add_dest(sam_ctx, mem_ctx, dn, &dest);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0,("Failed to delete repsTo for %s\n",
				 GUID_string(dce_call, &dest.source_dsa_obj_guid)));
			goto failed;
		}
	}

	if (ldb_transaction_commit(sam_ctx) != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to commit transaction on samdb\n"));
		return WERR_DS_DRA_INTERNAL_ERROR;		
	}

	talloc_free(sam_ctx);
	return WERR_OK;

failed:
	ldb_transaction_cancel(sam_ctx);
	talloc_free(sam_ctx);
	return werr;
}
