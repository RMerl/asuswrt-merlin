#include "../librpc/gen_ndr/ndr_dssetup.h"
#ifndef __CLI_DSSETUP__
#define __CLI_DSSETUP__
struct tevent_req *rpccli_dssetup_DsRoleGetPrimaryDomainInformation_send(TALLOC_CTX *mem_ctx,
									 struct tevent_context *ev,
									 struct rpc_pipe_client *cli,
									 enum dssetup_DsRoleInfoLevel _level /* [in]  */,
									 union dssetup_DsRoleInfo *_info /* [out] [unique,switch_is(level)] */);
NTSTATUS rpccli_dssetup_DsRoleGetPrimaryDomainInformation_recv(struct tevent_req *req,
							       TALLOC_CTX *mem_ctx,
							       WERROR *result);
NTSTATUS rpccli_dssetup_DsRoleGetPrimaryDomainInformation(struct rpc_pipe_client *cli,
							  TALLOC_CTX *mem_ctx,
							  enum dssetup_DsRoleInfoLevel level /* [in]  */,
							  union dssetup_DsRoleInfo *info /* [out] [unique,switch_is(level)] */,
							  WERROR *werror);
struct tevent_req *rpccli_dssetup_DsRoleDnsNameToFlatName_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct rpc_pipe_client *cli);
NTSTATUS rpccli_dssetup_DsRoleDnsNameToFlatName_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     WERROR *result);
NTSTATUS rpccli_dssetup_DsRoleDnsNameToFlatName(struct rpc_pipe_client *cli,
						TALLOC_CTX *mem_ctx,
						WERROR *werror);
struct tevent_req *rpccli_dssetup_DsRoleDcAsDc_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli);
NTSTATUS rpccli_dssetup_DsRoleDcAsDc_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_dssetup_DsRoleDcAsDc(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     WERROR *werror);
struct tevent_req *rpccli_dssetup_DsRoleDcAsReplica_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli);
NTSTATUS rpccli_dssetup_DsRoleDcAsReplica_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS rpccli_dssetup_DsRoleDcAsReplica(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  WERROR *werror);
struct tevent_req *rpccli_dssetup_DsRoleDemoteDc_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct rpc_pipe_client *cli);
NTSTATUS rpccli_dssetup_DsRoleDemoteDc_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS rpccli_dssetup_DsRoleDemoteDc(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       WERROR *werror);
struct tevent_req *rpccli_dssetup_DsRoleGetDcOperationProgress_send(TALLOC_CTX *mem_ctx,
								    struct tevent_context *ev,
								    struct rpc_pipe_client *cli);
NTSTATUS rpccli_dssetup_DsRoleGetDcOperationProgress_recv(struct tevent_req *req,
							  TALLOC_CTX *mem_ctx,
							  WERROR *result);
NTSTATUS rpccli_dssetup_DsRoleGetDcOperationProgress(struct rpc_pipe_client *cli,
						     TALLOC_CTX *mem_ctx,
						     WERROR *werror);
struct tevent_req *rpccli_dssetup_DsRoleGetDcOperationResults_send(TALLOC_CTX *mem_ctx,
								   struct tevent_context *ev,
								   struct rpc_pipe_client *cli);
NTSTATUS rpccli_dssetup_DsRoleGetDcOperationResults_recv(struct tevent_req *req,
							 TALLOC_CTX *mem_ctx,
							 WERROR *result);
NTSTATUS rpccli_dssetup_DsRoleGetDcOperationResults(struct rpc_pipe_client *cli,
						    TALLOC_CTX *mem_ctx,
						    WERROR *werror);
struct tevent_req *rpccli_dssetup_DsRoleCancel_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli);
NTSTATUS rpccli_dssetup_DsRoleCancel_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_dssetup_DsRoleCancel(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     WERROR *werror);
struct tevent_req *rpccli_dssetup_DsRoleServerSaveStateForUpgrade_send(TALLOC_CTX *mem_ctx,
								       struct tevent_context *ev,
								       struct rpc_pipe_client *cli);
NTSTATUS rpccli_dssetup_DsRoleServerSaveStateForUpgrade_recv(struct tevent_req *req,
							     TALLOC_CTX *mem_ctx,
							     WERROR *result);
NTSTATUS rpccli_dssetup_DsRoleServerSaveStateForUpgrade(struct rpc_pipe_client *cli,
							TALLOC_CTX *mem_ctx,
							WERROR *werror);
struct tevent_req *rpccli_dssetup_DsRoleUpgradeDownlevelServer_send(TALLOC_CTX *mem_ctx,
								    struct tevent_context *ev,
								    struct rpc_pipe_client *cli);
NTSTATUS rpccli_dssetup_DsRoleUpgradeDownlevelServer_recv(struct tevent_req *req,
							  TALLOC_CTX *mem_ctx,
							  WERROR *result);
NTSTATUS rpccli_dssetup_DsRoleUpgradeDownlevelServer(struct rpc_pipe_client *cli,
						     TALLOC_CTX *mem_ctx,
						     WERROR *werror);
struct tevent_req *rpccli_dssetup_DsRoleAbortDownlevelServerUpgrade_send(TALLOC_CTX *mem_ctx,
									 struct tevent_context *ev,
									 struct rpc_pipe_client *cli);
NTSTATUS rpccli_dssetup_DsRoleAbortDownlevelServerUpgrade_recv(struct tevent_req *req,
							       TALLOC_CTX *mem_ctx,
							       WERROR *result);
NTSTATUS rpccli_dssetup_DsRoleAbortDownlevelServerUpgrade(struct rpc_pipe_client *cli,
							  TALLOC_CTX *mem_ctx,
							  WERROR *werror);
#endif /* __CLI_DSSETUP__ */
