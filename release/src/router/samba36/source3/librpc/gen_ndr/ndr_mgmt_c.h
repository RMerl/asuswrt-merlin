#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/mgmt.h"
#ifndef _HEADER_RPC_mgmt
#define _HEADER_RPC_mgmt

extern const struct ndr_interface_table ndr_table_mgmt;

struct tevent_req *dcerpc_mgmt_inq_if_ids_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct mgmt_inq_if_ids *r);
NTSTATUS dcerpc_mgmt_inq_if_ids_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_mgmt_inq_if_ids_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct mgmt_inq_if_ids *r);
struct tevent_req *dcerpc_mgmt_inq_if_ids_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct rpc_if_id_vector_t **_if_id_vector /* [out] [ref] */);
NTSTATUS dcerpc_mgmt_inq_if_ids_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_mgmt_inq_if_ids(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct rpc_if_id_vector_t **_if_id_vector /* [out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_mgmt_inq_stats_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct mgmt_inq_stats *r);
NTSTATUS dcerpc_mgmt_inq_stats_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_mgmt_inq_stats_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct mgmt_inq_stats *r);
struct tevent_req *dcerpc_mgmt_inq_stats_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      uint32_t _max_count /* [in]  */,
					      uint32_t _unknown /* [in]  */,
					      struct mgmt_statistics *_statistics /* [out] [ref] */);
NTSTATUS dcerpc_mgmt_inq_stats_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_mgmt_inq_stats(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       uint32_t _max_count /* [in]  */,
			       uint32_t _unknown /* [in]  */,
			       struct mgmt_statistics *_statistics /* [out] [ref] */,
			       WERROR *result);

struct tevent_req *dcerpc_mgmt_is_server_listening_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct mgmt_is_server_listening *r);
NTSTATUS dcerpc_mgmt_is_server_listening_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_mgmt_is_server_listening_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct mgmt_is_server_listening *r);
struct tevent_req *dcerpc_mgmt_is_server_listening_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							uint32_t *_status /* [out] [ref] */);
NTSTATUS dcerpc_mgmt_is_server_listening_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      uint32_t *result);
NTSTATUS dcerpc_mgmt_is_server_listening(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 uint32_t *_status /* [out] [ref] */,
					 uint32_t *result);

struct tevent_req *dcerpc_mgmt_stop_server_listening_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct mgmt_stop_server_listening *r);
NTSTATUS dcerpc_mgmt_stop_server_listening_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_mgmt_stop_server_listening_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct mgmt_stop_server_listening *r);
struct tevent_req *dcerpc_mgmt_stop_server_listening_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_mgmt_stop_server_listening_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_mgmt_stop_server_listening(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);

struct tevent_req *dcerpc_mgmt_inq_princ_name_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct mgmt_inq_princ_name *r);
NTSTATUS dcerpc_mgmt_inq_princ_name_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_mgmt_inq_princ_name_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct mgmt_inq_princ_name *r);
struct tevent_req *dcerpc_mgmt_inq_princ_name_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   uint32_t _authn_proto /* [in]  */,
						   uint32_t _princ_name_size /* [in]  */,
						   const char *_princ_name /* [out] [charset(DOS),size_is(princ_name_size)] */);
NTSTATUS dcerpc_mgmt_inq_princ_name_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_mgmt_inq_princ_name(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    uint32_t _authn_proto /* [in]  */,
				    uint32_t _princ_name_size /* [in]  */,
				    const char *_princ_name /* [out] [charset(DOS),size_is(princ_name_size)] */,
				    WERROR *result);

#endif /* _HEADER_RPC_mgmt */
