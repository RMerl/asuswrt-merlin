#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/browser.h"
#ifndef _HEADER_RPC_browser
#define _HEADER_RPC_browser

extern const struct ndr_interface_table ndr_table_browser;

struct tevent_req *dcerpc_BrowserrQueryOtherDomains_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct BrowserrQueryOtherDomains *r);
NTSTATUS dcerpc_BrowserrQueryOtherDomains_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_BrowserrQueryOtherDomains_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct BrowserrQueryOtherDomains *r);
struct tevent_req *dcerpc_BrowserrQueryOtherDomains_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 const char *_server_unc /* [in] [unique,charset(UTF16)] */,
							 struct BrowserrSrvInfo *_info /* [in,out] [ref] */,
							 uint32_t *_total_entries /* [out] [ref] */);
NTSTATUS dcerpc_BrowserrQueryOtherDomains_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_BrowserrQueryOtherDomains(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  const char *_server_unc /* [in] [unique,charset(UTF16)] */,
					  struct BrowserrSrvInfo *_info /* [in,out] [ref] */,
					  uint32_t *_total_entries /* [out] [ref] */,
					  WERROR *result);

#endif /* _HEADER_RPC_browser */
