#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/dfsblobs.h"
#ifndef _HEADER_RPC_dfsblobs
#define _HEADER_RPC_dfsblobs

extern const struct ndr_interface_table ndr_table_dfsblobs;

struct tevent_req *dcerpc_dfs_GetDFSReferral_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_GetDFSReferral *r);
NTSTATUS dcerpc_dfs_GetDFSReferral_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_GetDFSReferral_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_GetDFSReferral *r);
struct tevent_req *dcerpc_dfs_GetDFSReferral_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct dfs_GetDFSReferral_in _req /* [in]  */,
						  struct dfs_referral_resp *_resp /* [out] [ref] */);
NTSTATUS dcerpc_dfs_GetDFSReferral_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_GetDFSReferral(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct dfs_GetDFSReferral_in _req /* [in]  */,
				   struct dfs_referral_resp *_resp /* [out] [ref] */);

#endif /* _HEADER_RPC_dfsblobs */
