#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/initshutdown.h"
#ifndef _HEADER_RPC_initshutdown
#define _HEADER_RPC_initshutdown

extern const struct ndr_interface_table ndr_table_initshutdown;

struct tevent_req *dcerpc_initshutdown_Init_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct initshutdown_Init *r);
NTSTATUS dcerpc_initshutdown_Init_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_initshutdown_Init_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct initshutdown_Init *r);
struct tevent_req *dcerpc_initshutdown_Init_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 uint16_t *_hostname /* [in] [unique] */,
						 struct lsa_StringLarge *_message /* [in] [unique] */,
						 uint32_t _timeout /* [in]  */,
						 uint8_t _force_apps /* [in]  */,
						 uint8_t _do_reboot /* [in]  */);
NTSTATUS dcerpc_initshutdown_Init_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_initshutdown_Init(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  uint16_t *_hostname /* [in] [unique] */,
				  struct lsa_StringLarge *_message /* [in] [unique] */,
				  uint32_t _timeout /* [in]  */,
				  uint8_t _force_apps /* [in]  */,
				  uint8_t _do_reboot /* [in]  */,
				  WERROR *result);

struct tevent_req *dcerpc_initshutdown_Abort_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct initshutdown_Abort *r);
NTSTATUS dcerpc_initshutdown_Abort_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_initshutdown_Abort_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct initshutdown_Abort *r);
struct tevent_req *dcerpc_initshutdown_Abort_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  uint16_t *_server /* [in] [unique] */);
NTSTATUS dcerpc_initshutdown_Abort_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_initshutdown_Abort(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   uint16_t *_server /* [in] [unique] */,
				   WERROR *result);

struct tevent_req *dcerpc_initshutdown_InitEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct initshutdown_InitEx *r);
NTSTATUS dcerpc_initshutdown_InitEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_initshutdown_InitEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct initshutdown_InitEx *r);
struct tevent_req *dcerpc_initshutdown_InitEx_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   uint16_t *_hostname /* [in] [unique] */,
						   struct lsa_StringLarge *_message /* [in] [unique] */,
						   uint32_t _timeout /* [in]  */,
						   uint8_t _force_apps /* [in]  */,
						   uint8_t _do_reboot /* [in]  */,
						   uint32_t _reason /* [in]  */);
NTSTATUS dcerpc_initshutdown_InitEx_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_initshutdown_InitEx(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    uint16_t *_hostname /* [in] [unique] */,
				    struct lsa_StringLarge *_message /* [in] [unique] */,
				    uint32_t _timeout /* [in]  */,
				    uint8_t _force_apps /* [in]  */,
				    uint8_t _do_reboot /* [in]  */,
				    uint32_t _reason /* [in]  */,
				    WERROR *result);

#endif /* _HEADER_RPC_initshutdown */
