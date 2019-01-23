#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/drsuapi.h"
#ifndef _HEADER_RPC_drsuapi
#define _HEADER_RPC_drsuapi

extern const struct ndr_interface_table ndr_table_drsuapi;

struct tevent_req *dcerpc_drsuapi_DsBind_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsBind *r);
NTSTATUS dcerpc_drsuapi_DsBind_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsBind_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsBind *r);
struct tevent_req *dcerpc_drsuapi_DsBind_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct GUID *_bind_guid /* [in] [unique] */,
					      struct drsuapi_DsBindInfoCtr *_bind_info /* [in,out] [unique] */,
					      struct policy_handle *_bind_handle /* [out] [ref] */);
NTSTATUS dcerpc_drsuapi_DsBind_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_drsuapi_DsBind(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct GUID *_bind_guid /* [in] [unique] */,
			       struct drsuapi_DsBindInfoCtr *_bind_info /* [in,out] [unique] */,
			       struct policy_handle *_bind_handle /* [out] [ref] */,
			       WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsUnbind_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsUnbind *r);
NTSTATUS dcerpc_drsuapi_DsUnbind_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsUnbind_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsUnbind *r);
struct tevent_req *dcerpc_drsuapi_DsUnbind_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_bind_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_drsuapi_DsUnbind_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_drsuapi_DsUnbind(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_bind_handle /* [in,out] [ref] */,
				 WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsReplicaSync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsReplicaSync *r);
NTSTATUS dcerpc_drsuapi_DsReplicaSync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsReplicaSync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsReplicaSync *r);
struct tevent_req *dcerpc_drsuapi_DsReplicaSync_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_bind_handle /* [in] [ref] */,
						     uint32_t _level /* [in]  */,
						     union drsuapi_DsReplicaSyncRequest *_req /* [in] [ref,switch_is(level)] */);
NTSTATUS dcerpc_drsuapi_DsReplicaSync_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_drsuapi_DsReplicaSync(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_bind_handle /* [in] [ref] */,
				      uint32_t _level /* [in]  */,
				      union drsuapi_DsReplicaSyncRequest *_req /* [in] [ref,switch_is(level)] */,
				      WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsGetNCChanges_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsGetNCChanges *r);
NTSTATUS dcerpc_drsuapi_DsGetNCChanges_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsGetNCChanges_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsGetNCChanges *r);
struct tevent_req *dcerpc_drsuapi_DsGetNCChanges_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_bind_handle /* [in] [ref] */,
						      uint32_t _level /* [in]  */,
						      union drsuapi_DsGetNCChangesRequest *_req /* [in] [switch_is(level),ref] */,
						      uint32_t *_level_out /* [out] [ref] */,
						      union drsuapi_DsGetNCChangesCtr *_ctr /* [out] [switch_is(*level_out),ref] */);
NTSTATUS dcerpc_drsuapi_DsGetNCChanges_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_drsuapi_DsGetNCChanges(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_bind_handle /* [in] [ref] */,
				       uint32_t _level /* [in]  */,
				       union drsuapi_DsGetNCChangesRequest *_req /* [in] [switch_is(level),ref] */,
				       uint32_t *_level_out /* [out] [ref] */,
				       union drsuapi_DsGetNCChangesCtr *_ctr /* [out] [switch_is(*level_out),ref] */,
				       WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsReplicaUpdateRefs_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsReplicaUpdateRefs *r);
NTSTATUS dcerpc_drsuapi_DsReplicaUpdateRefs_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsReplicaUpdateRefs_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsReplicaUpdateRefs *r);
struct tevent_req *dcerpc_drsuapi_DsReplicaUpdateRefs_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   struct policy_handle *_bind_handle /* [in] [ref] */,
							   uint32_t _level /* [in]  */,
							   union drsuapi_DsReplicaUpdateRefsRequest _req /* [in] [switch_is(level)] */);
NTSTATUS dcerpc_drsuapi_DsReplicaUpdateRefs_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_drsuapi_DsReplicaUpdateRefs(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    struct policy_handle *_bind_handle /* [in] [ref] */,
					    uint32_t _level /* [in]  */,
					    union drsuapi_DsReplicaUpdateRefsRequest _req /* [in] [switch_is(level)] */,
					    WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsReplicaAdd_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsReplicaAdd *r);
NTSTATUS dcerpc_drsuapi_DsReplicaAdd_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsReplicaAdd_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsReplicaAdd *r);
struct tevent_req *dcerpc_drsuapi_DsReplicaAdd_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_bind_handle /* [in] [ref] */,
						    uint32_t _level /* [in]  */,
						    union drsuapi_DsReplicaAddRequest _req /* [in] [switch_is(level)] */);
NTSTATUS dcerpc_drsuapi_DsReplicaAdd_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_drsuapi_DsReplicaAdd(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_bind_handle /* [in] [ref] */,
				     uint32_t _level /* [in]  */,
				     union drsuapi_DsReplicaAddRequest _req /* [in] [switch_is(level)] */,
				     WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsReplicaDel_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsReplicaDel *r);
NTSTATUS dcerpc_drsuapi_DsReplicaDel_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsReplicaDel_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsReplicaDel *r);
struct tevent_req *dcerpc_drsuapi_DsReplicaDel_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_bind_handle /* [in] [ref] */,
						    uint32_t _level /* [in]  */,
						    union drsuapi_DsReplicaDelRequest _req /* [in] [switch_is(level)] */);
NTSTATUS dcerpc_drsuapi_DsReplicaDel_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_drsuapi_DsReplicaDel(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_bind_handle /* [in] [ref] */,
				     uint32_t _level /* [in]  */,
				     union drsuapi_DsReplicaDelRequest _req /* [in] [switch_is(level)] */,
				     WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsReplicaMod_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsReplicaMod *r);
NTSTATUS dcerpc_drsuapi_DsReplicaMod_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsReplicaMod_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsReplicaMod *r);
struct tevent_req *dcerpc_drsuapi_DsReplicaMod_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_bind_handle /* [in] [ref] */,
						    uint32_t _level /* [in]  */,
						    union drsuapi_DsReplicaModRequest _req /* [in] [switch_is(level)] */);
NTSTATUS dcerpc_drsuapi_DsReplicaMod_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_drsuapi_DsReplicaMod(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_bind_handle /* [in] [ref] */,
				     uint32_t _level /* [in]  */,
				     union drsuapi_DsReplicaModRequest _req /* [in] [switch_is(level)] */,
				     WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsGetMemberships_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsGetMemberships *r);
NTSTATUS dcerpc_drsuapi_DsGetMemberships_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsGetMemberships_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsGetMemberships *r);
struct tevent_req *dcerpc_drsuapi_DsGetMemberships_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_bind_handle /* [in] [ref] */,
							uint32_t _level /* [in]  */,
							union drsuapi_DsGetMembershipsRequest *_req /* [in] [ref,switch_is(level)] */,
							uint32_t *_level_out /* [out] [ref] */,
							union drsuapi_DsGetMembershipsCtr *_ctr /* [out] [switch_is(*level_out),ref] */);
NTSTATUS dcerpc_drsuapi_DsGetMemberships_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_drsuapi_DsGetMemberships(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_bind_handle /* [in] [ref] */,
					 uint32_t _level /* [in]  */,
					 union drsuapi_DsGetMembershipsRequest *_req /* [in] [ref,switch_is(level)] */,
					 uint32_t *_level_out /* [out] [ref] */,
					 union drsuapi_DsGetMembershipsCtr *_ctr /* [out] [switch_is(*level_out),ref] */,
					 WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsGetNT4ChangeLog_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsGetNT4ChangeLog *r);
NTSTATUS dcerpc_drsuapi_DsGetNT4ChangeLog_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsGetNT4ChangeLog_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsGetNT4ChangeLog *r);
struct tevent_req *dcerpc_drsuapi_DsGetNT4ChangeLog_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_bind_handle /* [in] [ref] */,
							 uint32_t _level /* [in]  */,
							 union drsuapi_DsGetNT4ChangeLogRequest *_req /* [in] [switch_is(level),ref] */,
							 uint32_t *_level_out /* [out] [ref] */,
							 union drsuapi_DsGetNT4ChangeLogInfo *_info /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS dcerpc_drsuapi_DsGetNT4ChangeLog_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_drsuapi_DsGetNT4ChangeLog(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_bind_handle /* [in] [ref] */,
					  uint32_t _level /* [in]  */,
					  union drsuapi_DsGetNT4ChangeLogRequest *_req /* [in] [switch_is(level),ref] */,
					  uint32_t *_level_out /* [out] [ref] */,
					  union drsuapi_DsGetNT4ChangeLogInfo *_info /* [out] [ref,switch_is(*level_out)] */,
					  WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsCrackNames_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsCrackNames *r);
NTSTATUS dcerpc_drsuapi_DsCrackNames_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsCrackNames_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsCrackNames *r);
struct tevent_req *dcerpc_drsuapi_DsCrackNames_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_bind_handle /* [in] [ref] */,
						    uint32_t _level /* [in]  */,
						    union drsuapi_DsNameRequest *_req /* [in] [ref,switch_is(level)] */,
						    uint32_t *_level_out /* [out] [ref] */,
						    union drsuapi_DsNameCtr *_ctr /* [out] [switch_is(*level_out),ref] */);
NTSTATUS dcerpc_drsuapi_DsCrackNames_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_drsuapi_DsCrackNames(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_bind_handle /* [in] [ref] */,
				     uint32_t _level /* [in]  */,
				     union drsuapi_DsNameRequest *_req /* [in] [ref,switch_is(level)] */,
				     uint32_t *_level_out /* [out] [ref] */,
				     union drsuapi_DsNameCtr *_ctr /* [out] [switch_is(*level_out),ref] */,
				     WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsWriteAccountSpn_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsWriteAccountSpn *r);
NTSTATUS dcerpc_drsuapi_DsWriteAccountSpn_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsWriteAccountSpn_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsWriteAccountSpn *r);
struct tevent_req *dcerpc_drsuapi_DsWriteAccountSpn_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_bind_handle /* [in] [ref] */,
							 uint32_t _level /* [in]  */,
							 union drsuapi_DsWriteAccountSpnRequest *_req /* [in] [ref,switch_is(level)] */,
							 uint32_t *_level_out /* [out] [ref] */,
							 union drsuapi_DsWriteAccountSpnResult *_res /* [out] [switch_is(*level_out),ref] */);
NTSTATUS dcerpc_drsuapi_DsWriteAccountSpn_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_drsuapi_DsWriteAccountSpn(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_bind_handle /* [in] [ref] */,
					  uint32_t _level /* [in]  */,
					  union drsuapi_DsWriteAccountSpnRequest *_req /* [in] [ref,switch_is(level)] */,
					  uint32_t *_level_out /* [out] [ref] */,
					  union drsuapi_DsWriteAccountSpnResult *_res /* [out] [switch_is(*level_out),ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsRemoveDSServer_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsRemoveDSServer *r);
NTSTATUS dcerpc_drsuapi_DsRemoveDSServer_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsRemoveDSServer_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsRemoveDSServer *r);
struct tevent_req *dcerpc_drsuapi_DsRemoveDSServer_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_bind_handle /* [in] [ref] */,
							uint32_t _level /* [in]  */,
							union drsuapi_DsRemoveDSServerRequest *_req /* [in] [ref,switch_is(level)] */,
							uint32_t *_level_out /* [out] [ref] */,
							union drsuapi_DsRemoveDSServerResult *_res /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS dcerpc_drsuapi_DsRemoveDSServer_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_drsuapi_DsRemoveDSServer(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_bind_handle /* [in] [ref] */,
					 uint32_t _level /* [in]  */,
					 union drsuapi_DsRemoveDSServerRequest *_req /* [in] [ref,switch_is(level)] */,
					 uint32_t *_level_out /* [out] [ref] */,
					 union drsuapi_DsRemoveDSServerResult *_res /* [out] [ref,switch_is(*level_out)] */,
					 WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsGetDomainControllerInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsGetDomainControllerInfo *r);
NTSTATUS dcerpc_drsuapi_DsGetDomainControllerInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsGetDomainControllerInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsGetDomainControllerInfo *r);
struct tevent_req *dcerpc_drsuapi_DsGetDomainControllerInfo_send(TALLOC_CTX *mem_ctx,
								 struct tevent_context *ev,
								 struct dcerpc_binding_handle *h,
								 struct policy_handle *_bind_handle /* [in] [ref] */,
								 int32_t _level /* [in]  */,
								 union drsuapi_DsGetDCInfoRequest *_req /* [in] [switch_is(level),ref] */,
								 int32_t *_level_out /* [out] [ref] */,
								 union drsuapi_DsGetDCInfoCtr *_ctr /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS dcerpc_drsuapi_DsGetDomainControllerInfo_recv(struct tevent_req *req,
						       TALLOC_CTX *mem_ctx,
						       WERROR *result);
NTSTATUS dcerpc_drsuapi_DsGetDomainControllerInfo(struct dcerpc_binding_handle *h,
						  TALLOC_CTX *mem_ctx,
						  struct policy_handle *_bind_handle /* [in] [ref] */,
						  int32_t _level /* [in]  */,
						  union drsuapi_DsGetDCInfoRequest *_req /* [in] [switch_is(level),ref] */,
						  int32_t *_level_out /* [out] [ref] */,
						  union drsuapi_DsGetDCInfoCtr *_ctr /* [out] [ref,switch_is(*level_out)] */,
						  WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsAddEntry_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsAddEntry *r);
NTSTATUS dcerpc_drsuapi_DsAddEntry_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsAddEntry_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsAddEntry *r);
struct tevent_req *dcerpc_drsuapi_DsAddEntry_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct policy_handle *_bind_handle /* [in] [ref] */,
						  uint32_t _level /* [in]  */,
						  union drsuapi_DsAddEntryRequest *_req /* [in] [switch_is(level),ref] */,
						  uint32_t *_level_out /* [out] [ref] */,
						  union drsuapi_DsAddEntryCtr *_ctr /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS dcerpc_drsuapi_DsAddEntry_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_drsuapi_DsAddEntry(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *_bind_handle /* [in] [ref] */,
				   uint32_t _level /* [in]  */,
				   union drsuapi_DsAddEntryRequest *_req /* [in] [switch_is(level),ref] */,
				   uint32_t *_level_out /* [out] [ref] */,
				   union drsuapi_DsAddEntryCtr *_ctr /* [out] [ref,switch_is(*level_out)] */,
				   WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsExecuteKCC_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsExecuteKCC *r);
NTSTATUS dcerpc_drsuapi_DsExecuteKCC_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsExecuteKCC_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsExecuteKCC *r);
struct tevent_req *dcerpc_drsuapi_DsExecuteKCC_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_bind_handle /* [in] [ref] */,
						    uint32_t _level /* [in]  */,
						    union drsuapi_DsExecuteKCCRequest *_req /* [in] [ref,switch_is(level)] */);
NTSTATUS dcerpc_drsuapi_DsExecuteKCC_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_drsuapi_DsExecuteKCC(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_bind_handle /* [in] [ref] */,
				     uint32_t _level /* [in]  */,
				     union drsuapi_DsExecuteKCCRequest *_req /* [in] [ref,switch_is(level)] */,
				     WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsReplicaGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsReplicaGetInfo *r);
NTSTATUS dcerpc_drsuapi_DsReplicaGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsReplicaGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsReplicaGetInfo *r);
struct tevent_req *dcerpc_drsuapi_DsReplicaGetInfo_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_bind_handle /* [in] [ref] */,
							enum drsuapi_DsReplicaGetInfoLevel _level /* [in]  */,
							union drsuapi_DsReplicaGetInfoRequest *_req /* [in] [switch_is(level),ref] */,
							enum drsuapi_DsReplicaInfoType *_info_type /* [out] [ref] */,
							union drsuapi_DsReplicaInfo *_info /* [out] [switch_is(*info_type),ref] */);
NTSTATUS dcerpc_drsuapi_DsReplicaGetInfo_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_drsuapi_DsReplicaGetInfo(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_bind_handle /* [in] [ref] */,
					 enum drsuapi_DsReplicaGetInfoLevel _level /* [in]  */,
					 union drsuapi_DsReplicaGetInfoRequest *_req /* [in] [switch_is(level),ref] */,
					 enum drsuapi_DsReplicaInfoType *_info_type /* [out] [ref] */,
					 union drsuapi_DsReplicaInfo *_info /* [out] [switch_is(*info_type),ref] */,
					 WERROR *result);

struct tevent_req *dcerpc_drsuapi_DsGetMemberships2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_DsGetMemberships2 *r);
NTSTATUS dcerpc_drsuapi_DsGetMemberships2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_DsGetMemberships2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_DsGetMemberships2 *r);
struct tevent_req *dcerpc_drsuapi_DsGetMemberships2_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_bind_handle /* [in] [ref] */,
							 uint32_t _level /* [in]  */,
							 union drsuapi_DsGetMemberships2Request *_req /* [in] [ref,switch_is(level)] */,
							 uint32_t *_level_out /* [out] [ref] */,
							 union drsuapi_DsGetMemberships2Ctr *_ctr /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS dcerpc_drsuapi_DsGetMemberships2_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_drsuapi_DsGetMemberships2(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_bind_handle /* [in] [ref] */,
					  uint32_t _level /* [in]  */,
					  union drsuapi_DsGetMemberships2Request *_req /* [in] [ref,switch_is(level)] */,
					  uint32_t *_level_out /* [out] [ref] */,
					  union drsuapi_DsGetMemberships2Ctr *_ctr /* [out] [ref,switch_is(*level_out)] */,
					  WERROR *result);

struct tevent_req *dcerpc_drsuapi_QuerySitesByCost_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct drsuapi_QuerySitesByCost *r);
NTSTATUS dcerpc_drsuapi_QuerySitesByCost_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_drsuapi_QuerySitesByCost_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct drsuapi_QuerySitesByCost *r);
struct tevent_req *dcerpc_drsuapi_QuerySitesByCost_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_bind_handle /* [in] [ref] */,
							uint32_t _level /* [in]  */,
							union drsuapi_QuerySitesByCostRequest *_req /* [in] [ref,switch_is(level)] */,
							uint32_t *_level_out /* [out] [ref] */,
							union drsuapi_QuerySitesByCostCtr *_ctr /* [out] [switch_is(*level_out),ref] */);
NTSTATUS dcerpc_drsuapi_QuerySitesByCost_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_drsuapi_QuerySitesByCost(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_bind_handle /* [in] [ref] */,
					 uint32_t _level /* [in]  */,
					 union drsuapi_QuerySitesByCostRequest *_req /* [in] [ref,switch_is(level)] */,
					 uint32_t *_level_out /* [out] [ref] */,
					 union drsuapi_QuerySitesByCostCtr *_ctr /* [out] [switch_is(*level_out),ref] */,
					 WERROR *result);

#endif /* _HEADER_RPC_drsuapi */
