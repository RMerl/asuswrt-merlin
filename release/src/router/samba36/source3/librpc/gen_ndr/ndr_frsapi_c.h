#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/frsapi.h"
#ifndef _HEADER_RPC_frsapi
#define _HEADER_RPC_frsapi

extern const struct ndr_interface_table ndr_table_frsapi;

struct tevent_req *dcerpc_frsapi_SetDsPollingIntervalW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frsapi_SetDsPollingIntervalW *r);
NTSTATUS dcerpc_frsapi_SetDsPollingIntervalW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frsapi_SetDsPollingIntervalW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frsapi_SetDsPollingIntervalW *r);
struct tevent_req *dcerpc_frsapi_SetDsPollingIntervalW_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct dcerpc_binding_handle *h,
							    uint32_t _CurrentInterval /* [in]  */,
							    uint32_t _DsPollingLongInterval /* [in]  */,
							    uint32_t _DsPollingShortInterval /* [in]  */);
NTSTATUS dcerpc_frsapi_SetDsPollingIntervalW_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS dcerpc_frsapi_SetDsPollingIntervalW(struct dcerpc_binding_handle *h,
					     TALLOC_CTX *mem_ctx,
					     uint32_t _CurrentInterval /* [in]  */,
					     uint32_t _DsPollingLongInterval /* [in]  */,
					     uint32_t _DsPollingShortInterval /* [in]  */,
					     WERROR *result);

struct tevent_req *dcerpc_frsapi_GetDsPollingIntervalW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frsapi_GetDsPollingIntervalW *r);
NTSTATUS dcerpc_frsapi_GetDsPollingIntervalW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frsapi_GetDsPollingIntervalW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frsapi_GetDsPollingIntervalW *r);
struct tevent_req *dcerpc_frsapi_GetDsPollingIntervalW_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct dcerpc_binding_handle *h,
							    uint32_t *_CurrentInterval /* [out] [ref] */,
							    uint32_t *_DsPollingLongInterval /* [out] [ref] */,
							    uint32_t *_DsPollingShortInterval /* [out] [ref] */);
NTSTATUS dcerpc_frsapi_GetDsPollingIntervalW_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS dcerpc_frsapi_GetDsPollingIntervalW(struct dcerpc_binding_handle *h,
					     TALLOC_CTX *mem_ctx,
					     uint32_t *_CurrentInterval /* [out] [ref] */,
					     uint32_t *_DsPollingLongInterval /* [out] [ref] */,
					     uint32_t *_DsPollingShortInterval /* [out] [ref] */,
					     WERROR *result);

struct tevent_req *dcerpc_frsapi_InfoW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frsapi_InfoW *r);
NTSTATUS dcerpc_frsapi_InfoW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frsapi_InfoW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frsapi_InfoW *r);
struct tevent_req *dcerpc_frsapi_InfoW_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct dcerpc_binding_handle *h,
					    uint32_t _length /* [in] [range(0,0x10000)] */,
					    struct frsapi_Info *_info /* [in,out] [unique] */);
NTSTATUS dcerpc_frsapi_InfoW_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  WERROR *result);
NTSTATUS dcerpc_frsapi_InfoW(struct dcerpc_binding_handle *h,
			     TALLOC_CTX *mem_ctx,
			     uint32_t _length /* [in] [range(0,0x10000)] */,
			     struct frsapi_Info *_info /* [in,out] [unique] */,
			     WERROR *result);

struct tevent_req *dcerpc_frsapi_IsPathReplicated_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frsapi_IsPathReplicated *r);
NTSTATUS dcerpc_frsapi_IsPathReplicated_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frsapi_IsPathReplicated_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frsapi_IsPathReplicated *r);
struct tevent_req *dcerpc_frsapi_IsPathReplicated_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char *_path /* [in] [unique,charset(UTF16)] */,
						       enum frsapi_ReplicaSetType _replica_set_type /* [in]  */,
						       uint32_t *_replicated /* [out] [ref] */,
						       uint32_t *_primary /* [out] [ref] */,
						       uint32_t *_root /* [out] [ref] */,
						       struct GUID *_replica_set_guid /* [out] [ref] */);
NTSTATUS dcerpc_frsapi_IsPathReplicated_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS dcerpc_frsapi_IsPathReplicated(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char *_path /* [in] [unique,charset(UTF16)] */,
					enum frsapi_ReplicaSetType _replica_set_type /* [in]  */,
					uint32_t *_replicated /* [out] [ref] */,
					uint32_t *_primary /* [out] [ref] */,
					uint32_t *_root /* [out] [ref] */,
					struct GUID *_replica_set_guid /* [out] [ref] */,
					WERROR *result);

struct tevent_req *dcerpc_frsapi_WriterCommand_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frsapi_WriterCommand *r);
NTSTATUS dcerpc_frsapi_WriterCommand_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frsapi_WriterCommand_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frsapi_WriterCommand *r);
struct tevent_req *dcerpc_frsapi_WriterCommand_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    enum frsapi_WriterCommandsValues _command /* [in]  */);
NTSTATUS dcerpc_frsapi_WriterCommand_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_frsapi_WriterCommand(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     enum frsapi_WriterCommandsValues _command /* [in]  */,
				     WERROR *result);

struct tevent_req *dcerpc_frsapi_ForceReplication_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frsapi_ForceReplication *r);
NTSTATUS dcerpc_frsapi_ForceReplication_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frsapi_ForceReplication_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frsapi_ForceReplication *r);
struct tevent_req *dcerpc_frsapi_ForceReplication_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct GUID *_replica_set_guid /* [in] [unique] */,
						       struct GUID *_connection_guid /* [in] [unique] */,
						       const char *_replica_set_name /* [in] [unique,charset(UTF16)] */,
						       const char *_partner_dns_name /* [in] [charset(UTF16),unique] */);
NTSTATUS dcerpc_frsapi_ForceReplication_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS dcerpc_frsapi_ForceReplication(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct GUID *_replica_set_guid /* [in] [unique] */,
					struct GUID *_connection_guid /* [in] [unique] */,
					const char *_replica_set_name /* [in] [unique,charset(UTF16)] */,
					const char *_partner_dns_name /* [in] [charset(UTF16),unique] */,
					WERROR *result);

#endif /* _HEADER_RPC_frsapi */
