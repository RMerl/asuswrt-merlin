#include "../librpc/gen_ndr/ndr_drsuapi.h"
#ifndef __CLI_DRSUAPI__
#define __CLI_DRSUAPI__
struct tevent_req *rpccli_drsuapi_DsBind_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli,
					      struct GUID *_bind_guid /* [in] [unique] */,
					      struct drsuapi_DsBindInfoCtr *_bind_info /* [in,out] [unique] */,
					      struct policy_handle *_bind_handle /* [out] [ref] */);
NTSTATUS rpccli_drsuapi_DsBind_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS rpccli_drsuapi_DsBind(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       struct GUID *bind_guid /* [in] [unique] */,
			       struct drsuapi_DsBindInfoCtr *bind_info /* [in,out] [unique] */,
			       struct policy_handle *bind_handle /* [out] [ref] */,
			       WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsUnbind_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						struct policy_handle *_bind_handle /* [in,out] [ref] */);
NTSTATUS rpccli_drsuapi_DsUnbind_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS rpccli_drsuapi_DsUnbind(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *bind_handle /* [in,out] [ref] */,
				 WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsReplicaSync_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli,
						     struct policy_handle *_bind_handle /* [in] [ref] */,
						     int32_t _level /* [in]  */,
						     union drsuapi_DsReplicaSyncRequest _req /* [in] [switch_is(level)] */);
NTSTATUS rpccli_drsuapi_DsReplicaSync_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS rpccli_drsuapi_DsReplicaSync(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *bind_handle /* [in] [ref] */,
				      int32_t level /* [in]  */,
				      union drsuapi_DsReplicaSyncRequest req /* [in] [switch_is(level)] */,
				      WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsGetNCChanges_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct rpc_pipe_client *cli,
						      struct policy_handle *_bind_handle /* [in] [ref] */,
						      int32_t _level /* [in]  */,
						      union drsuapi_DsGetNCChangesRequest *_req /* [in] [ref,switch_is(level)] */,
						      int32_t *_level_out /* [out] [ref] */,
						      union drsuapi_DsGetNCChangesCtr *_ctr /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS rpccli_drsuapi_DsGetNCChanges_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS rpccli_drsuapi_DsGetNCChanges(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *bind_handle /* [in] [ref] */,
				       int32_t level /* [in]  */,
				       union drsuapi_DsGetNCChangesRequest *req /* [in] [ref,switch_is(level)] */,
				       int32_t *level_out /* [out] [ref] */,
				       union drsuapi_DsGetNCChangesCtr *ctr /* [out] [ref,switch_is(*level_out)] */,
				       WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsReplicaUpdateRefs_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct rpc_pipe_client *cli,
							   struct policy_handle *_bind_handle /* [in] [ref] */,
							   int32_t _level /* [in]  */,
							   union drsuapi_DsReplicaUpdateRefsRequest _req /* [in] [switch_is(level)] */);
NTSTATUS rpccli_drsuapi_DsReplicaUpdateRefs_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS rpccli_drsuapi_DsReplicaUpdateRefs(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    struct policy_handle *bind_handle /* [in] [ref] */,
					    int32_t level /* [in]  */,
					    union drsuapi_DsReplicaUpdateRefsRequest req /* [in] [switch_is(level)] */,
					    WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsReplicaAdd_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct policy_handle *_bind_handle /* [in] [ref] */,
						    int32_t _level /* [in]  */,
						    union drsuapi_DsReplicaAddRequest _req /* [in] [switch_is(level)] */);
NTSTATUS rpccli_drsuapi_DsReplicaAdd_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_drsuapi_DsReplicaAdd(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *bind_handle /* [in] [ref] */,
				     int32_t level /* [in]  */,
				     union drsuapi_DsReplicaAddRequest req /* [in] [switch_is(level)] */,
				     WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsReplicaDel_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct policy_handle *_bind_handle /* [in] [ref] */,
						    int32_t _level /* [in]  */,
						    union drsuapi_DsReplicaDelRequest _req /* [in] [switch_is(level)] */);
NTSTATUS rpccli_drsuapi_DsReplicaDel_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_drsuapi_DsReplicaDel(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *bind_handle /* [in] [ref] */,
				     int32_t level /* [in]  */,
				     union drsuapi_DsReplicaDelRequest req /* [in] [switch_is(level)] */,
				     WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsReplicaMod_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct policy_handle *_bind_handle /* [in] [ref] */,
						    int32_t _level /* [in]  */,
						    union drsuapi_DsReplicaModRequest _req /* [in] [switch_is(level)] */);
NTSTATUS rpccli_drsuapi_DsReplicaMod_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_drsuapi_DsReplicaMod(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *bind_handle /* [in] [ref] */,
				     int32_t level /* [in]  */,
				     union drsuapi_DsReplicaModRequest req /* [in] [switch_is(level)] */,
				     WERROR *werror);
struct tevent_req *rpccli_DRSUAPI_VERIFY_NAMES_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli);
NTSTATUS rpccli_DRSUAPI_VERIFY_NAMES_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_DRSUAPI_VERIFY_NAMES(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsGetMemberships_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							struct policy_handle *_bind_handle /* [in] [ref] */,
							int32_t _level /* [in]  */,
							union drsuapi_DsGetMembershipsRequest *_req /* [in] [ref,switch_is(level)] */,
							int32_t *_level_out /* [out] [ref] */,
							union drsuapi_DsGetMembershipsCtr *_ctr /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS rpccli_drsuapi_DsGetMemberships_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_drsuapi_DsGetMemberships(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *bind_handle /* [in] [ref] */,
					 int32_t level /* [in]  */,
					 union drsuapi_DsGetMembershipsRequest *req /* [in] [ref,switch_is(level)] */,
					 int32_t *level_out /* [out] [ref] */,
					 union drsuapi_DsGetMembershipsCtr *ctr /* [out] [ref,switch_is(*level_out)] */,
					 WERROR *werror);
struct tevent_req *rpccli_DRSUAPI_INTER_DOMAIN_MOVE_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli);
NTSTATUS rpccli_DRSUAPI_INTER_DOMAIN_MOVE_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS rpccli_DRSUAPI_INTER_DOMAIN_MOVE(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsGetNT4ChangeLog_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli,
							 struct policy_handle *_bind_handle /* [in] [ref] */,
							 uint32_t _level /* [in]  */,
							 union drsuapi_DsGetNT4ChangeLogRequest *_req /* [in] [ref,switch_is(level)] */,
							 uint32_t *_level_out /* [out] [ref] */,
							 union drsuapi_DsGetNT4ChangeLogInfo *_info /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS rpccli_drsuapi_DsGetNT4ChangeLog_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS rpccli_drsuapi_DsGetNT4ChangeLog(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *bind_handle /* [in] [ref] */,
					  uint32_t level /* [in]  */,
					  union drsuapi_DsGetNT4ChangeLogRequest *req /* [in] [ref,switch_is(level)] */,
					  uint32_t *level_out /* [out] [ref] */,
					  union drsuapi_DsGetNT4ChangeLogInfo *info /* [out] [ref,switch_is(*level_out)] */,
					  WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsCrackNames_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct policy_handle *_bind_handle /* [in] [ref] */,
						    int32_t _level /* [in]  */,
						    union drsuapi_DsNameRequest *_req /* [in] [ref,switch_is(level)] */,
						    int32_t *_level_out /* [out] [ref] */,
						    union drsuapi_DsNameCtr *_ctr /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS rpccli_drsuapi_DsCrackNames_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_drsuapi_DsCrackNames(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *bind_handle /* [in] [ref] */,
				     int32_t level /* [in]  */,
				     union drsuapi_DsNameRequest *req /* [in] [ref,switch_is(level)] */,
				     int32_t *level_out /* [out] [ref] */,
				     union drsuapi_DsNameCtr *ctr /* [out] [ref,switch_is(*level_out)] */,
				     WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsWriteAccountSpn_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli,
							 struct policy_handle *_bind_handle /* [in] [ref] */,
							 int32_t _level /* [in]  */,
							 union drsuapi_DsWriteAccountSpnRequest *_req /* [in] [ref,switch_is(level)] */,
							 int32_t *_level_out /* [out] [ref] */,
							 union drsuapi_DsWriteAccountSpnResult *_res /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS rpccli_drsuapi_DsWriteAccountSpn_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS rpccli_drsuapi_DsWriteAccountSpn(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *bind_handle /* [in] [ref] */,
					  int32_t level /* [in]  */,
					  union drsuapi_DsWriteAccountSpnRequest *req /* [in] [ref,switch_is(level)] */,
					  int32_t *level_out /* [out] [ref] */,
					  union drsuapi_DsWriteAccountSpnResult *res /* [out] [ref,switch_is(*level_out)] */,
					  WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsRemoveDSServer_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							struct policy_handle *_bind_handle /* [in] [ref] */,
							int32_t _level /* [in]  */,
							union drsuapi_DsRemoveDSServerRequest *_req /* [in] [ref,switch_is(level)] */,
							int32_t *_level_out /* [out] [ref] */,
							union drsuapi_DsRemoveDSServerResult *_res /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS rpccli_drsuapi_DsRemoveDSServer_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_drsuapi_DsRemoveDSServer(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *bind_handle /* [in] [ref] */,
					 int32_t level /* [in]  */,
					 union drsuapi_DsRemoveDSServerRequest *req /* [in] [ref,switch_is(level)] */,
					 int32_t *level_out /* [out] [ref] */,
					 union drsuapi_DsRemoveDSServerResult *res /* [out] [ref,switch_is(*level_out)] */,
					 WERROR *werror);
struct tevent_req *rpccli_DRSUAPI_REMOVE_DS_DOMAIN_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli);
NTSTATUS rpccli_DRSUAPI_REMOVE_DS_DOMAIN_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_DRSUAPI_REMOVE_DS_DOMAIN(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsGetDomainControllerInfo_send(TALLOC_CTX *mem_ctx,
								 struct tevent_context *ev,
								 struct rpc_pipe_client *cli,
								 struct policy_handle *_bind_handle /* [in] [ref] */,
								 int32_t _level /* [in]  */,
								 union drsuapi_DsGetDCInfoRequest *_req /* [in] [ref,switch_is(level)] */,
								 int32_t *_level_out /* [out] [ref] */,
								 union drsuapi_DsGetDCInfoCtr *_ctr /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS rpccli_drsuapi_DsGetDomainControllerInfo_recv(struct tevent_req *req,
						       TALLOC_CTX *mem_ctx,
						       WERROR *result);
NTSTATUS rpccli_drsuapi_DsGetDomainControllerInfo(struct rpc_pipe_client *cli,
						  TALLOC_CTX *mem_ctx,
						  struct policy_handle *bind_handle /* [in] [ref] */,
						  int32_t level /* [in]  */,
						  union drsuapi_DsGetDCInfoRequest *req /* [in] [ref,switch_is(level)] */,
						  int32_t *level_out /* [out] [ref] */,
						  union drsuapi_DsGetDCInfoCtr *ctr /* [out] [ref,switch_is(*level_out)] */,
						  WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsAddEntry_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli,
						  struct policy_handle *_bind_handle /* [in] [ref] */,
						  int32_t _level /* [in]  */,
						  union drsuapi_DsAddEntryRequest *_req /* [in] [ref,switch_is(level)] */,
						  int32_t *_level_out /* [out] [ref] */,
						  union drsuapi_DsAddEntryCtr *_ctr /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS rpccli_drsuapi_DsAddEntry_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS rpccli_drsuapi_DsAddEntry(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *bind_handle /* [in] [ref] */,
				   int32_t level /* [in]  */,
				   union drsuapi_DsAddEntryRequest *req /* [in] [ref,switch_is(level)] */,
				   int32_t *level_out /* [out] [ref] */,
				   union drsuapi_DsAddEntryCtr *ctr /* [out] [ref,switch_is(*level_out)] */,
				   WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsExecuteKCC_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct policy_handle *_bind_handle /* [in] [ref] */,
						    uint32_t _level /* [in]  */,
						    union drsuapi_DsExecuteKCCRequest *_req /* [in] [ref,switch_is(level)] */);
NTSTATUS rpccli_drsuapi_DsExecuteKCC_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_drsuapi_DsExecuteKCC(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *bind_handle /* [in] [ref] */,
				     uint32_t level /* [in]  */,
				     union drsuapi_DsExecuteKCCRequest *req /* [in] [ref,switch_is(level)] */,
				     WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsReplicaGetInfo_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							struct policy_handle *_bind_handle /* [in] [ref] */,
							enum drsuapi_DsReplicaGetInfoLevel _level /* [in]  */,
							union drsuapi_DsReplicaGetInfoRequest *_req /* [in] [ref,switch_is(level)] */,
							enum drsuapi_DsReplicaInfoType *_info_type /* [out] [ref] */,
							union drsuapi_DsReplicaInfo *_info /* [out] [ref,switch_is(*info_type)] */);
NTSTATUS rpccli_drsuapi_DsReplicaGetInfo_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_drsuapi_DsReplicaGetInfo(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *bind_handle /* [in] [ref] */,
					 enum drsuapi_DsReplicaGetInfoLevel level /* [in]  */,
					 union drsuapi_DsReplicaGetInfoRequest *req /* [in] [ref,switch_is(level)] */,
					 enum drsuapi_DsReplicaInfoType *info_type /* [out] [ref] */,
					 union drsuapi_DsReplicaInfo *info /* [out] [ref,switch_is(*info_type)] */,
					 WERROR *werror);
struct tevent_req *rpccli_DRSUAPI_ADD_SID_HISTORY_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct rpc_pipe_client *cli);
NTSTATUS rpccli_DRSUAPI_ADD_SID_HISTORY_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS rpccli_DRSUAPI_ADD_SID_HISTORY(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					WERROR *werror);
struct tevent_req *rpccli_drsuapi_DsGetMemberships2_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli,
							 struct policy_handle *_bind_handle /* [in] [ref] */,
							 int32_t _level /* [in]  */,
							 union drsuapi_DsGetMemberships2Request *_req /* [in] [ref,switch_is(level)] */,
							 int32_t *_level_out /* [out] [ref] */,
							 union drsuapi_DsGetMemberships2Ctr *_ctr /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS rpccli_drsuapi_DsGetMemberships2_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS rpccli_drsuapi_DsGetMemberships2(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *bind_handle /* [in] [ref] */,
					  int32_t level /* [in]  */,
					  union drsuapi_DsGetMemberships2Request *req /* [in] [ref,switch_is(level)] */,
					  int32_t *level_out /* [out] [ref] */,
					  union drsuapi_DsGetMemberships2Ctr *ctr /* [out] [ref,switch_is(*level_out)] */,
					  WERROR *werror);
struct tevent_req *rpccli_DRSUAPI_REPLICA_VERIFY_OBJECTS_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct rpc_pipe_client *cli);
NTSTATUS rpccli_DRSUAPI_REPLICA_VERIFY_OBJECTS_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    WERROR *result);
NTSTATUS rpccli_DRSUAPI_REPLICA_VERIFY_OBJECTS(struct rpc_pipe_client *cli,
					       TALLOC_CTX *mem_ctx,
					       WERROR *werror);
struct tevent_req *rpccli_DRSUAPI_GET_OBJECT_EXISTENCE_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct rpc_pipe_client *cli);
NTSTATUS rpccli_DRSUAPI_GET_OBJECT_EXISTENCE_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS rpccli_DRSUAPI_GET_OBJECT_EXISTENCE(struct rpc_pipe_client *cli,
					     TALLOC_CTX *mem_ctx,
					     WERROR *werror);
struct tevent_req *rpccli_drsuapi_QuerySitesByCost_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							struct policy_handle *_bind_handle /* [in] [ref] */,
							int32_t _level /* [in]  */,
							union drsuapi_QuerySitesByCostRequest *_req /* [in] [ref,switch_is(level)] */,
							int32_t *_level_out /* [out] [ref] */,
							union drsuapi_QuerySitesByCostCtr *_ctr /* [out] [ref,switch_is(*level_out)] */);
NTSTATUS rpccli_drsuapi_QuerySitesByCost_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_drsuapi_QuerySitesByCost(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *bind_handle /* [in] [ref] */,
					 int32_t level /* [in]  */,
					 union drsuapi_QuerySitesByCostRequest *req /* [in] [ref,switch_is(level)] */,
					 int32_t *level_out /* [out] [ref] */,
					 union drsuapi_QuerySitesByCostCtr *ctr /* [out] [ref,switch_is(*level_out)] */,
					 WERROR *werror);
#endif /* __CLI_DRSUAPI__ */
