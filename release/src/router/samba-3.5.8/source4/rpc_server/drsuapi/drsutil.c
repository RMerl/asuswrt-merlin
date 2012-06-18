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
#include "libcli/security/dom_sid.h"
#include "rpc_server/drsuapi/dcesrv_drsuapi.h"
#include "libcli/security/security.h"
#include "param/param.h"

/*
  format a drsuapi_DsReplicaObjectIdentifier naming context as a string
 */
char *drs_ObjectIdentifier_to_string(TALLOC_CTX *mem_ctx,
				     struct drsuapi_DsReplicaObjectIdentifier *nc)
{
	char *guid, *sid, *ret;
	guid = GUID_string(mem_ctx, &nc->guid);
	sid  = dom_sid_string(mem_ctx, &nc->sid);
	ret = talloc_asprintf(mem_ctx, "<GUID=%s>;<SID=%s>;%s",
			      guid, sid, nc->dn);
	talloc_free(guid);
	talloc_free(sid);
	return ret;
}

int drsuapi_search_with_extended_dn(struct ldb_context *ldb,
				    TALLOC_CTX *mem_ctx,
				    struct ldb_result **_res,
				    struct ldb_dn *basedn,
				    enum ldb_scope scope,
				    const char * const *attrs,
				    const char *sort_attrib,
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

	ret = ldb_request_add_control(req, LDB_CONTROL_SHOW_DELETED_OID, true, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (sort_attrib) {
		struct ldb_server_sort_control **sort_control;
		sort_control = talloc_array(req, struct ldb_server_sort_control *, 2);
		if (sort_control == NULL) {
			talloc_free(tmp_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		sort_control[0] = talloc(req, struct ldb_server_sort_control);
		sort_control[0]->attributeName = sort_attrib;
		sort_control[0]->orderingRule = NULL;
		sort_control[0]->reverse = 1;
		sort_control[1] = NULL;

		ret = ldb_request_add_control(req, LDB_CONTROL_SERVER_SORT_OID, true, sort_control);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}


	ret = ldb_request(ldb, req);
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	}

	talloc_free(req);
	*_res = talloc_steal(mem_ctx, res);
	return ret;
}

WERROR drs_security_level_check(struct dcesrv_call_state *dce_call, const char* call)
{
	if (lp_parm_bool(dce_call->conn->dce_ctx->lp_ctx, NULL, 
			 "drs", "disable_sec_check", false)) {
		return WERR_OK;
	}

	if (security_session_user_level(dce_call->conn->auth_state.session_info) <
		SECURITY_DOMAIN_CONTROLLER) {
		DEBUG(0,("DsReplicaGetInfo refused for security token\n"));
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
	case DRSUAPI_ATTRIBUTE_dBCSPwd:
	case DRSUAPI_ATTRIBUTE_unicodePwd:
	case DRSUAPI_ATTRIBUTE_ntPwdHistory:
	case DRSUAPI_ATTRIBUTE_lmPwdHistory:
	case DRSUAPI_ATTRIBUTE_supplementalCredentials:
	case DRSUAPI_ATTRIBUTE_priorValue:
	case DRSUAPI_ATTRIBUTE_currentValue:
	case DRSUAPI_ATTRIBUTE_trustAuthOutgoing:
	case DRSUAPI_ATTRIBUTE_trustAuthIncoming:
	case DRSUAPI_ATTRIBUTE_initialAuthOutgoing:
	case DRSUAPI_ATTRIBUTE_initialAuthIncoming:
		/*set value to null*/
		attr->value_ctr.num_values = 0;
		talloc_free(attr->value_ctr.values);
		attr->value_ctr.values = NULL;
		meta_data->originating_change_time = 0;
		return;
	default:
		return;
	}
	return;
}
