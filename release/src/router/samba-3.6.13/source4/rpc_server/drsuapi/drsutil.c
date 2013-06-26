/* 
   Unix SMB/CIFS implementation.

   useful utilities for the DRS server

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
#include "param/param.h"
#include "auth/session.h"

int drsuapi_search_with_extended_dn(struct ldb_context *ldb,
				    TALLOC_CTX *mem_ctx,
				    struct ldb_result **_res,
				    struct ldb_dn *basedn,
				    enum ldb_scope scope,
				    const char * const *attrs,
				    const char *filter)
{
	int ret;
	struct ldb_request *req;
	TALLOC_CTX *tmp_ctx;
	struct ldb_result *res;

	tmp_ctx = talloc_new(mem_ctx);

	res = talloc_zero(tmp_ctx, struct ldb_result);
	if (!res) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_build_search_req(&req, ldb, tmp_ctx,
				   basedn,
				   scope,
				   filter,
				   attrs,
				   NULL,
				   res,
				   ldb_search_default_callback,
				   NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	ret = ldb_request_add_control(req, LDB_CONTROL_EXTENDED_DN_OID, true, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_request_add_control(req, LDB_CONTROL_SHOW_RECYCLED_OID, true, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_request_add_control(req, LDB_CONTROL_REVEAL_INTERNALS, false, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_request(ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	talloc_free(req);
	*_res = talloc_steal(mem_ctx, res);
	return ret;
}

WERROR drs_security_level_check(struct dcesrv_call_state *dce_call,
				const char* call,
				enum security_user_level minimum_level,
				const struct dom_sid *domain_sid)
{
	enum security_user_level level;

	if (lpcfg_parm_bool(dce_call->conn->dce_ctx->lp_ctx, NULL,
			 "drs", "disable_sec_check", false)) {
		return WERR_OK;
	}

	level = security_session_user_level(dce_call->conn->auth_state.session_info, domain_sid);
	if (level < minimum_level) {
		if (call) {
			DEBUG(0,("%s refused for security token (level=%u)\n",
				 call, (unsigned)level));
			security_token_debug(0, 2, dce_call->conn->auth_state.session_info->security_token);
		}
		return WERR_DS_DRA_ACCESS_DENIED;
	}

	return WERR_OK;
}

void drsuapi_process_secret_attribute(struct drsuapi_DsReplicaAttribute *attr,
				      struct drsuapi_DsReplicaMetaData *meta_data)
{
	if (attr->value_ctr.num_values == 0) {
		return;
	}

	switch (attr->attid) {
	case DRSUAPI_ATTID_dBCSPwd:
	case DRSUAPI_ATTID_unicodePwd:
	case DRSUAPI_ATTID_ntPwdHistory:
	case DRSUAPI_ATTID_lmPwdHistory:
	case DRSUAPI_ATTID_supplementalCredentials:
	case DRSUAPI_ATTID_priorValue:
	case DRSUAPI_ATTID_currentValue:
	case DRSUAPI_ATTID_trustAuthOutgoing:
	case DRSUAPI_ATTID_trustAuthIncoming:
	case DRSUAPI_ATTID_initialAuthOutgoing:
	case DRSUAPI_ATTID_initialAuthIncoming:
		/*set value to null*/
		attr->value_ctr.num_values = 0;
		talloc_free(attr->value_ctr.values);
		attr->value_ctr.values = NULL;
		meta_data->originating_change_time = 0;
		return;
	default:
		return;
	}
}


/*
  check security on a DN, with logging of errors
 */
static WERROR drs_security_access_check_log(struct ldb_context *sam_ctx,
					    TALLOC_CTX *mem_ctx,
					    struct security_token *token,
					    struct ldb_dn *dn,
					    const char *ext_right)
{
	int ret;
	if (!dn) {
		DEBUG(3,("drs_security_access_check: Null dn provided, access is denied for %s\n",
			      ext_right));
		return WERR_DS_DRA_ACCESS_DENIED;
	}
	ret = dsdb_check_access_on_dn(sam_ctx,
				      mem_ctx,
				      dn,
				      token,
				      SEC_ADS_CONTROL_ACCESS,
				      ext_right);
	if (ret == LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS) {
		DEBUG(3,("%s refused for security token on %s\n",
			 ext_right, ldb_dn_get_linearized(dn)));
		security_token_debug(2, 0, token);
		return WERR_DS_DRA_ACCESS_DENIED;
	} else if (ret != LDB_SUCCESS) {
		DEBUG(1,("Failed to perform access check on %s\n", ldb_dn_get_linearized(dn)));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}
	return WERR_OK;
}


/*
  check security on a object identifier
 */
WERROR drs_security_access_check(struct ldb_context *sam_ctx,
				 TALLOC_CTX *mem_ctx,
				 struct security_token *token,
				 struct drsuapi_DsReplicaObjectIdentifier *nc,
				 const char *ext_right)
{
	struct ldb_dn *dn = drs_ObjectIdentifier_to_dn(mem_ctx, sam_ctx, nc);
	WERROR werr;
	werr = drs_security_access_check_log(sam_ctx, mem_ctx, token, dn, ext_right);
	talloc_free(dn);
	return werr;
}

/*
  check security on the NC root of a object identifier
 */
WERROR drs_security_access_check_nc_root(struct ldb_context *sam_ctx,
					 TALLOC_CTX *mem_ctx,
					 struct security_token *token,
					 struct drsuapi_DsReplicaObjectIdentifier *nc,
					 const char *ext_right)
{
	struct ldb_dn *dn, *nc_root;
	WERROR werr;
	int ret;

	dn = drs_ObjectIdentifier_to_dn(mem_ctx, sam_ctx, nc);
	W_ERROR_HAVE_NO_MEMORY(dn);
	ret = dsdb_find_nc_root(sam_ctx, dn, dn, &nc_root);
	if (ret != LDB_SUCCESS) {
		return WERR_DS_CANT_FIND_EXPECTED_NC;
	}
	werr = drs_security_access_check_log(sam_ctx, mem_ctx, token, nc_root, ext_right);
	talloc_free(dn);
	return werr;
}
