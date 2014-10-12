#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/frstrans.h"
#ifndef _HEADER_RPC_frstrans
#define _HEADER_RPC_frstrans

extern const struct ndr_interface_table ndr_table_frstrans;

struct tevent_req *dcerpc_frstrans_CheckConnectivity_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frstrans_CheckConnectivity *r);
NTSTATUS dcerpc_frstrans_CheckConnectivity_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frstrans_CheckConnectivity_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frstrans_CheckConnectivity *r);
struct tevent_req *dcerpc_frstrans_CheckConnectivity_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct GUID _replica_set_guid /* [in]  */,
							  struct GUID _connection_guid /* [in]  */);
NTSTATUS dcerpc_frstrans_CheckConnectivity_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_frstrans_CheckConnectivity(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct GUID _replica_set_guid /* [in]  */,
					   struct GUID _connection_guid /* [in]  */,
					   WERROR *result);

struct tevent_req *dcerpc_frstrans_EstablishConnection_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frstrans_EstablishConnection *r);
NTSTATUS dcerpc_frstrans_EstablishConnection_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frstrans_EstablishConnection_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frstrans_EstablishConnection *r);
struct tevent_req *dcerpc_frstrans_EstablishConnection_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct dcerpc_binding_handle *h,
							    struct GUID _replica_set_guid /* [in]  */,
							    struct GUID _connection_guid /* [in]  */,
							    enum frstrans_ProtocolVersion _downstream_protocol_version /* [in]  */,
							    uint32_t _downstream_flags /* [in]  */,
							    enum frstrans_ProtocolVersion *_upstream_protocol_version /* [out] [ref] */,
							    uint32_t *_upstream_flags /* [out] [ref] */);
NTSTATUS dcerpc_frstrans_EstablishConnection_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS dcerpc_frstrans_EstablishConnection(struct dcerpc_binding_handle *h,
					     TALLOC_CTX *mem_ctx,
					     struct GUID _replica_set_guid /* [in]  */,
					     struct GUID _connection_guid /* [in]  */,
					     enum frstrans_ProtocolVersion _downstream_protocol_version /* [in]  */,
					     uint32_t _downstream_flags /* [in]  */,
					     enum frstrans_ProtocolVersion *_upstream_protocol_version /* [out] [ref] */,
					     uint32_t *_upstream_flags /* [out] [ref] */,
					     WERROR *result);

struct tevent_req *dcerpc_frstrans_EstablishSession_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frstrans_EstablishSession *r);
NTSTATUS dcerpc_frstrans_EstablishSession_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frstrans_EstablishSession_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frstrans_EstablishSession *r);
struct tevent_req *dcerpc_frstrans_EstablishSession_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct GUID _connection_guid /* [in]  */,
							 struct GUID _content_set_guid /* [in]  */);
NTSTATUS dcerpc_frstrans_EstablishSession_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_frstrans_EstablishSession(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct GUID _connection_guid /* [in]  */,
					  struct GUID _content_set_guid /* [in]  */,
					  WERROR *result);

struct tevent_req *dcerpc_frstrans_RequestUpdates_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frstrans_RequestUpdates *r);
NTSTATUS dcerpc_frstrans_RequestUpdates_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frstrans_RequestUpdates_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frstrans_RequestUpdates *r);
struct tevent_req *dcerpc_frstrans_RequestUpdates_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct GUID _connection_guid /* [in]  */,
						       struct GUID _content_set_guid /* [in]  */,
						       uint32_t _credits_available /* [in] [range(0,256)] */,
						       uint32_t _hash_requested /* [in] [range(0,1)] */,
						       enum frstrans_UpdateRequestType _update_request_type /* [in] [range(0,2)] */,
						       uint32_t _version_vector_diff_count /* [in]  */,
						       struct frstrans_VersionVector *_version_vector_diff /* [in] [ref,size_is(version_vector_diff_count)] */,
						       struct frstrans_Update *_frs_update /* [out] [length_is(*update_count),ref,size_is(credits_available)] */,
						       uint32_t *_update_count /* [out] [ref] */,
						       enum frstrans_UpdateStatus *_update_status /* [out] [ref] */,
						       struct GUID *_gvsn_db_guid /* [out] [ref] */,
						       uint64_t *_gvsn_version /* [out] [ref] */);
NTSTATUS dcerpc_frstrans_RequestUpdates_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS dcerpc_frstrans_RequestUpdates(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct GUID _connection_guid /* [in]  */,
					struct GUID _content_set_guid /* [in]  */,
					uint32_t _credits_available /* [in] [range(0,256)] */,
					uint32_t _hash_requested /* [in] [range(0,1)] */,
					enum frstrans_UpdateRequestType _update_request_type /* [in] [range(0,2)] */,
					uint32_t _version_vector_diff_count /* [in]  */,
					struct frstrans_VersionVector *_version_vector_diff /* [in] [ref,size_is(version_vector_diff_count)] */,
					struct frstrans_Update *_frs_update /* [out] [length_is(*update_count),ref,size_is(credits_available)] */,
					uint32_t *_update_count /* [out] [ref] */,
					enum frstrans_UpdateStatus *_update_status /* [out] [ref] */,
					struct GUID *_gvsn_db_guid /* [out] [ref] */,
					uint64_t *_gvsn_version /* [out] [ref] */,
					WERROR *result);

struct tevent_req *dcerpc_frstrans_RequestVersionVector_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frstrans_RequestVersionVector *r);
NTSTATUS dcerpc_frstrans_RequestVersionVector_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frstrans_RequestVersionVector_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frstrans_RequestVersionVector *r);
struct tevent_req *dcerpc_frstrans_RequestVersionVector_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     uint32_t _sequence_number /* [in]  */,
							     struct GUID _connection_guid /* [in]  */,
							     struct GUID _content_set_guid /* [in]  */,
							     enum frstrans_VersionRequestType _request_type /* [in] [range(0,2)] */,
							     enum frstrans_VersionChangeType _change_type /* [in] [range(0,2)] */,
							     uint64_t _vv_generation /* [in]  */);
NTSTATUS dcerpc_frstrans_RequestVersionVector_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_frstrans_RequestVersionVector(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      uint32_t _sequence_number /* [in]  */,
					      struct GUID _connection_guid /* [in]  */,
					      struct GUID _content_set_guid /* [in]  */,
					      enum frstrans_VersionRequestType _request_type /* [in] [range(0,2)] */,
					      enum frstrans_VersionChangeType _change_type /* [in] [range(0,2)] */,
					      uint64_t _vv_generation /* [in]  */,
					      WERROR *result);

struct tevent_req *dcerpc_frstrans_AsyncPoll_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frstrans_AsyncPoll *r);
NTSTATUS dcerpc_frstrans_AsyncPoll_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frstrans_AsyncPoll_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frstrans_AsyncPoll *r);
struct tevent_req *dcerpc_frstrans_AsyncPoll_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct GUID _connection_guid /* [in]  */,
						  struct frstrans_AsyncResponseContext *_response /* [out] [ref] */);
NTSTATUS dcerpc_frstrans_AsyncPoll_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_frstrans_AsyncPoll(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct GUID _connection_guid /* [in]  */,
				   struct frstrans_AsyncResponseContext *_response /* [out] [ref] */,
				   WERROR *result);

struct tevent_req *dcerpc_frstrans_InitializeFileTransferAsync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frstrans_InitializeFileTransferAsync *r);
NTSTATUS dcerpc_frstrans_InitializeFileTransferAsync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frstrans_InitializeFileTransferAsync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frstrans_InitializeFileTransferAsync *r);
struct tevent_req *dcerpc_frstrans_InitializeFileTransferAsync_send(TALLOC_CTX *mem_ctx,
								    struct tevent_context *ev,
								    struct dcerpc_binding_handle *h,
								    struct GUID _connection_guid /* [in]  */,
								    struct frstrans_Update *_frs_update /* [in,out] [ref] */,
								    uint32_t _rdc_desired /* [in] [range(0,1)] */,
								    enum frstrans_RequestedStagingPolicy *_staging_policy /* [in,out] [ref] */,
								    struct policy_handle *_server_context /* [out] [ref] */,
								    struct frstrans_RdcFileInfo **_rdc_file_info /* [out] [ref] */,
								    uint8_t *_data_buffer /* [out] [length_is(*size_read),ref,size_is(buffer_size)] */,
								    uint32_t _buffer_size /* [in] [range(0,262144)] */,
								    uint32_t *_size_read /* [out] [ref] */,
								    uint32_t *_is_end_of_file /* [out] [ref] */);
NTSTATUS dcerpc_frstrans_InitializeFileTransferAsync_recv(struct tevent_req *req,
							  TALLOC_CTX *mem_ctx,
							  WERROR *result);
NTSTATUS dcerpc_frstrans_InitializeFileTransferAsync(struct dcerpc_binding_handle *h,
						     TALLOC_CTX *mem_ctx,
						     struct GUID _connection_guid /* [in]  */,
						     struct frstrans_Update *_frs_update /* [in,out] [ref] */,
						     uint32_t _rdc_desired /* [in] [range(0,1)] */,
						     enum frstrans_RequestedStagingPolicy *_staging_policy /* [in,out] [ref] */,
						     struct policy_handle *_server_context /* [out] [ref] */,
						     struct frstrans_RdcFileInfo **_rdc_file_info /* [out] [ref] */,
						     uint8_t *_data_buffer /* [out] [length_is(*size_read),ref,size_is(buffer_size)] */,
						     uint32_t _buffer_size /* [in] [range(0,262144)] */,
						     uint32_t *_size_read /* [out] [ref] */,
						     uint32_t *_is_end_of_file /* [out] [ref] */,
						     WERROR *result);

/*
 * The following function is skipped because
 * it uses pipes:
 *
 * dcerpc_frstrans_RawGetFileDataAsync_r_send()
 * dcerpc_frstrans_RawGetFileDataAsync_r_recv()
 * dcerpc_frstrans_RawGetFileDataAsync_r()
 *
 * dcerpc_frstrans_RawGetFileDataAsync_send()
 * dcerpc_frstrans_RawGetFileDataAsync_recv()
 * dcerpc_frstrans_RawGetFileDataAsync()
 */

/*
 * The following function is skipped because
 * it uses pipes:
 *
 * dcerpc_frstrans_RdcGetFileDataAsync_r_send()
 * dcerpc_frstrans_RdcGetFileDataAsync_r_recv()
 * dcerpc_frstrans_RdcGetFileDataAsync_r()
 *
 * dcerpc_frstrans_RdcGetFileDataAsync_send()
 * dcerpc_frstrans_RdcGetFileDataAsync_recv()
 * dcerpc_frstrans_RdcGetFileDataAsync()
 */

#endif /* _HEADER_RPC_frstrans */
