#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/dbgidl.h"
#ifndef _HEADER_RPC_dbgidl
#define _HEADER_RPC_dbgidl

extern const struct ndr_interface_table ndr_table_dbgidl;

struct tevent_req *dcerpc_dummy_dbgidl_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dummy_dbgidl *r);
NTSTATUS dcerpc_dummy_dbgidl_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dummy_dbgidl_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dummy_dbgidl *r);
struct tevent_req *dcerpc_dummy_dbgidl_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_dummy_dbgidl_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dummy_dbgidl(struct dcerpc_binding_handle *h,
			     TALLOC_CTX *mem_ctx);

#endif /* _HEADER_RPC_dbgidl */
