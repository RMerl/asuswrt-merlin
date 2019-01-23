#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/krb5pac.h"
#ifndef _HEADER_RPC_krb5pac
#define _HEADER_RPC_krb5pac

extern const struct ndr_interface_table ndr_table_krb5pac;

struct tevent_req *dcerpc_decode_pac_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_pac *r);
NTSTATUS dcerpc_decode_pac_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_pac_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_pac *r);
struct tevent_req *dcerpc_decode_pac_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  struct PAC_DATA _pac /* [in]  */);
NTSTATUS dcerpc_decode_pac_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_pac(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   struct PAC_DATA _pac /* [in]  */);

struct tevent_req *dcerpc_decode_pac_raw_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_pac_raw *r);
NTSTATUS dcerpc_decode_pac_raw_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_pac_raw_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_pac_raw *r);
struct tevent_req *dcerpc_decode_pac_raw_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct PAC_DATA_RAW _pac /* [in]  */);
NTSTATUS dcerpc_decode_pac_raw_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_pac_raw(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct PAC_DATA_RAW _pac /* [in]  */);

struct tevent_req *dcerpc_decode_login_info_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_login_info *r);
NTSTATUS dcerpc_decode_login_info_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_login_info_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_login_info *r);
struct tevent_req *dcerpc_decode_login_info_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct PAC_LOGON_INFO _logon_info /* [in]  */);
NTSTATUS dcerpc_decode_login_info_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_login_info(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct PAC_LOGON_INFO _logon_info /* [in]  */);

struct tevent_req *dcerpc_decode_login_info_ctr_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_login_info_ctr *r);
NTSTATUS dcerpc_decode_login_info_ctr_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_login_info_ctr_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_login_info_ctr *r);
struct tevent_req *dcerpc_decode_login_info_ctr_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct PAC_LOGON_INFO_CTR _logon_info_ctr /* [in]  */);
NTSTATUS dcerpc_decode_login_info_ctr_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_login_info_ctr(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct PAC_LOGON_INFO_CTR _logon_info_ctr /* [in]  */);

struct tevent_req *dcerpc_decode_pac_validate_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_pac_validate *r);
NTSTATUS dcerpc_decode_pac_validate_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_pac_validate_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_pac_validate *r);
struct tevent_req *dcerpc_decode_pac_validate_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct PAC_Validate _pac_validate /* [in]  */);
NTSTATUS dcerpc_decode_pac_validate_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_pac_validate(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct PAC_Validate _pac_validate /* [in]  */);

#endif /* _HEADER_RPC_krb5pac */
