#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/nbt.h"
#ifndef _HEADER_RPC_nbt
#define _HEADER_RPC_nbt

extern const struct ndr_interface_table ndr_table_nbt;

struct tevent_req *dcerpc_decode_nbt_netlogon_packet_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_nbt_netlogon_packet *r);
NTSTATUS dcerpc_decode_nbt_netlogon_packet_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_nbt_netlogon_packet_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_nbt_netlogon_packet *r);
struct tevent_req *dcerpc_decode_nbt_netlogon_packet_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct nbt_netlogon_packet _packet /* [in]  */);
NTSTATUS dcerpc_decode_nbt_netlogon_packet_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_nbt_netlogon_packet(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct nbt_netlogon_packet _packet /* [in]  */);

#endif /* _HEADER_RPC_nbt */
