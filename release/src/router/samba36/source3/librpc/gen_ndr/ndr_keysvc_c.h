#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/keysvc.h"
#ifndef _HEADER_RPC_keysvc
#define _HEADER_RPC_keysvc

extern const struct ndr_interface_table ndr_table_keysvc;

struct tevent_req *dcerpc_keysvc_Unknown0_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct keysvc_Unknown0 *r);
NTSTATUS dcerpc_keysvc_Unknown0_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_keysvc_Unknown0_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct keysvc_Unknown0 *r);
struct tevent_req *dcerpc_keysvc_Unknown0_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_keysvc_Unknown0_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_keysvc_Unknown0(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				WERROR *result);

#endif /* _HEADER_RPC_keysvc */
