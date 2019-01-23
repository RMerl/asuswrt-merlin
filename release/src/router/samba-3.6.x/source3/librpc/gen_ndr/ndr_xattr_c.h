#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/xattr.h"
#ifndef _HEADER_RPC_xattr
#define _HEADER_RPC_xattr

extern const struct ndr_interface_table ndr_table_xattr;

struct tevent_req *dcerpc_xattr_parse_DOSATTRIB_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct xattr_parse_DOSATTRIB *r);
NTSTATUS dcerpc_xattr_parse_DOSATTRIB_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_xattr_parse_DOSATTRIB_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct xattr_parse_DOSATTRIB *r);
struct tevent_req *dcerpc_xattr_parse_DOSATTRIB_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct xattr_DOSATTRIB _x /* [in]  */);
NTSTATUS dcerpc_xattr_parse_DOSATTRIB_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_xattr_parse_DOSATTRIB(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct xattr_DOSATTRIB _x /* [in]  */);

#endif /* _HEADER_RPC_xattr */
