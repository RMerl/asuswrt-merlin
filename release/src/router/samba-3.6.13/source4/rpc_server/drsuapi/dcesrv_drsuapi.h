/* 
   Unix SMB/CIFS implementation.

   endpoint server for the drsuapi pipe

   Copyright (C) Stefan Metzmacher 2004
   
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

/*
  this type allows us to distinguish handle types
*/
enum drsuapi_handle {
	DRSUAPI_BIND_HANDLE,
};

/*
  state asscoiated with a drsuapi_DsBind*() operation
*/
struct drsuapi_bind_state {
	struct ldb_context *sam_ctx;
	struct ldb_context *sam_ctx_system;
	struct GUID remote_bind_guid;
	struct drsuapi_DsBindInfo28 remote_info28;
	struct drsuapi_DsBindInfo28 local_info28;
	struct drsuapi_getncchanges_state *getncchanges_state;
};


/* prototypes of internal functions */
WERROR drsuapi_UpdateRefs(struct drsuapi_bind_state *b_state, TALLOC_CTX *mem_ctx,
			  struct drsuapi_DsReplicaUpdateRefsRequest1 *req);
WERROR dcesrv_drsuapi_DsReplicaUpdateRefs(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					  struct drsuapi_DsReplicaUpdateRefs *r);
WERROR dcesrv_drsuapi_DsGetNCChanges(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				     struct drsuapi_DsGetNCChanges *r);
WERROR dcesrv_drsuapi_DsAddEntry(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				 struct drsuapi_DsAddEntry *r);
WERROR dcesrv_drsuapi_DsWriteAccountSpn(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					struct drsuapi_DsWriteAccountSpn *r);

char *drs_ObjectIdentifier_to_string(TALLOC_CTX *mem_ctx,
				     struct drsuapi_DsReplicaObjectIdentifier *nc);

int drsuapi_search_with_extended_dn(struct ldb_context *ldb,
				    TALLOC_CTX *mem_ctx,
				    struct ldb_result **_res,
				    struct ldb_dn *basedn,
				    enum ldb_scope scope,
				    const char * const *attrs,
				    const char *filter);

WERROR drs_security_level_check(struct dcesrv_call_state *dce_call,
				const char* call, enum security_user_level minimum_level,
				const struct dom_sid *domain_sid);

void drsuapi_process_secret_attribute(struct drsuapi_DsReplicaAttribute *attr,
				      struct drsuapi_DsReplicaMetaData *meta_data);

WERROR drs_security_access_check(struct ldb_context *sam_ctx,
				 TALLOC_CTX *mem_ctx,
				 struct security_token *token,
				 struct drsuapi_DsReplicaObjectIdentifier *nc,
				 const char *ext_right);

WERROR drs_security_access_check_nc_root(struct ldb_context *sam_ctx,
					 TALLOC_CTX *mem_ctx,
					 struct security_token *token,
					 struct drsuapi_DsReplicaObjectIdentifier *nc,
					 const char *ext_right);
