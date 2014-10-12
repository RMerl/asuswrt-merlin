#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/dns.h"
#ifndef _HEADER_RPC_dns
#define _HEADER_RPC_dns

extern const struct ndr_interface_table ndr_table_dns;

struct tevent_req *dcerpc_decode_dns_name_packet_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_dns_name_packet *r);
NTSTATUS dcerpc_decode_dns_name_packet_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_dns_name_packet_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_dns_name_packet *r);
struct tevent_req *dcerpc_decode_dns_name_packet_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct dns_name_packet _packet /* [in]  */);
NTSTATUS dcerpc_decode_dns_name_packet_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_dns_name_packet(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct dns_name_packet _packet /* [in]  */);

#endif /* _HEADER_RPC_dns */
