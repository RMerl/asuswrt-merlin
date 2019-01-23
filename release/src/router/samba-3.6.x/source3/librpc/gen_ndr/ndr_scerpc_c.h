#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/scerpc.h"
#ifndef _HEADER_RPC_scerpc
#define _HEADER_RPC_scerpc

extern const struct ndr_interface_table ndr_table_scerpc;

struct tevent_req *dcerpc_scerpc_Unknown0_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct scerpc_Unknown0 *r);
NTSTATUS dcerpc_scerpc_Unknown0_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_scerpc_Unknown0_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct scerpc_Unknown0 *r);
struct tevent_req *dcerpc_scerpc_Unknown0_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_scerpc_Unknown0_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_scerpc_Unknown0(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				WERROR *result);

#endif /* _HEADER_RPC_scerpc */
