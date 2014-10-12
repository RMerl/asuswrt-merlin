#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/dnsserver.h"
#ifndef _HEADER_RPC_dnsserver
#define _HEADER_RPC_dnsserver

extern const struct ndr_interface_table ndr_table_dnsserver;

struct tevent_req *dcerpc_dnsserver_foo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dnsserver_foo *r);
NTSTATUS dcerpc_dnsserver_foo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dnsserver_foo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dnsserver_foo *r);
struct tevent_req *dcerpc_dnsserver_foo_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_dnsserver_foo_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dnsserver_foo(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx);

#endif /* _HEADER_RPC_dnsserver */
