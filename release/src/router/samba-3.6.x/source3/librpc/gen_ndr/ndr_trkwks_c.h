#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/trkwks.h"
#ifndef _HEADER_RPC_trkwks
#define _HEADER_RPC_trkwks

extern const struct ndr_interface_table ndr_table_trkwks;

struct tevent_req *dcerpc_trkwks_Unknown0_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct trkwks_Unknown0 *r);
NTSTATUS dcerpc_trkwks_Unknown0_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_trkwks_Unknown0_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct trkwks_Unknown0 *r);
struct tevent_req *dcerpc_trkwks_Unknown0_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_trkwks_Unknown0_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_trkwks_Unknown0(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				WERROR *result);

#endif /* _HEADER_RPC_trkwks */
