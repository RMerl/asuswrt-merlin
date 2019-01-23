#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/preg.h"
#ifndef _HEADER_RPC_preg
#define _HEADER_RPC_preg

extern const struct ndr_interface_table ndr_table_preg;

struct tevent_req *dcerpc_decode_preg_file_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_preg_file *r);
NTSTATUS dcerpc_decode_preg_file_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_preg_file_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_preg_file *r);
struct tevent_req *dcerpc_decode_preg_file_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct preg_file _file /* [in]  */);
NTSTATUS dcerpc_decode_preg_file_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_preg_file(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct preg_file _file /* [in]  */);

#endif /* _HEADER_RPC_preg */
