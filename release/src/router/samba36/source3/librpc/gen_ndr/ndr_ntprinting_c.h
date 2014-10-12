#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/ntprinting.h"
#ifndef _HEADER_RPC_ntprinting
#define _HEADER_RPC_ntprinting

extern const struct ndr_interface_table ndr_table_ntprinting;

struct tevent_req *dcerpc_decode_ntprinting_form_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_ntprinting_form *r);
NTSTATUS dcerpc_decode_ntprinting_form_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_ntprinting_form_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_ntprinting_form *r);
struct tevent_req *dcerpc_decode_ntprinting_form_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct ntprinting_form _form /* [in]  */);
NTSTATUS dcerpc_decode_ntprinting_form_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_ntprinting_form(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct ntprinting_form _form /* [in]  */);

struct tevent_req *dcerpc_decode_ntprinting_driver_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_ntprinting_driver *r);
NTSTATUS dcerpc_decode_ntprinting_driver_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_ntprinting_driver_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_ntprinting_driver *r);
struct tevent_req *dcerpc_decode_ntprinting_driver_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct ntprinting_driver _driver /* [in]  */);
NTSTATUS dcerpc_decode_ntprinting_driver_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_ntprinting_driver(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct ntprinting_driver _driver /* [in]  */);

struct tevent_req *dcerpc_decode_ntprinting_printer_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_ntprinting_printer *r);
NTSTATUS dcerpc_decode_ntprinting_printer_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_ntprinting_printer_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_ntprinting_printer *r);
struct tevent_req *dcerpc_decode_ntprinting_printer_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct ntprinting_printer _printer /* [in]  */);
NTSTATUS dcerpc_decode_ntprinting_printer_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_ntprinting_printer(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct ntprinting_printer _printer /* [in]  */);

#endif /* _HEADER_RPC_ntprinting */
