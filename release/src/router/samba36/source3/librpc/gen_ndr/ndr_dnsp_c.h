#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/dnsp.h"
#ifndef _HEADER_RPC_dnsp
#define _HEADER_RPC_dnsp

extern const struct ndr_interface_table ndr_table_dnsp;

struct tevent_req *dcerpc_decode_DnssrvRpcRecord_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_DnssrvRpcRecord *r);
NTSTATUS dcerpc_decode_DnssrvRpcRecord_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_DnssrvRpcRecord_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_DnssrvRpcRecord *r);
struct tevent_req *dcerpc_decode_DnssrvRpcRecord_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct dnsp_DnssrvRpcRecord _blob /* [in]  */);
NTSTATUS dcerpc_decode_DnssrvRpcRecord_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_DnssrvRpcRecord(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct dnsp_DnssrvRpcRecord _blob /* [in]  */);

#endif /* _HEADER_RPC_dnsp */
