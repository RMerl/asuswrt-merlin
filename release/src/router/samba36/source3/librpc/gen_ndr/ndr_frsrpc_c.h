#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/frsrpc.h"
#ifndef _HEADER_RPC_frsrpc
#define _HEADER_RPC_frsrpc

extern const struct ndr_interface_table ndr_table_frsrpc;

struct tevent_req *dcerpc_frsrpc_FrsSendCommPkt_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frsrpc_FrsSendCommPkt *r);
NTSTATUS dcerpc_frsrpc_FrsSendCommPkt_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frsrpc_FrsSendCommPkt_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frsrpc_FrsSendCommPkt *r);
struct tevent_req *dcerpc_frsrpc_FrsSendCommPkt_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct frsrpc_FrsSendCommPktReq _req /* [in]  */);
NTSTATUS dcerpc_frsrpc_FrsSendCommPkt_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_frsrpc_FrsSendCommPkt(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct frsrpc_FrsSendCommPktReq _req /* [in]  */,
				      WERROR *result);

struct tevent_req *dcerpc_frsrpc_FrsVerifyPromotionParent_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frsrpc_FrsVerifyPromotionParent *r);
NTSTATUS dcerpc_frsrpc_FrsVerifyPromotionParent_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frsrpc_FrsVerifyPromotionParent_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frsrpc_FrsVerifyPromotionParent *r);
struct tevent_req *dcerpc_frsrpc_FrsVerifyPromotionParent_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct dcerpc_binding_handle *h,
							       const char *_parent_account /* [in] [unique,charset(UTF16)] */,
							       const char *_parent_password /* [in] [unique,charset(UTF16)] */,
							       const char *_replica_set_name /* [in] [unique,charset(UTF16)] */,
							       const char *_replica_set_type /* [in] [unique,charset(UTF16)] */,
							       enum frsrpc_PartnerAuthLevel _partner_auth_level /* [in]  */,
							       uint32_t ___ndr_guid_size /* [in]  */);
NTSTATUS dcerpc_frsrpc_FrsVerifyPromotionParent_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     WERROR *result);
NTSTATUS dcerpc_frsrpc_FrsVerifyPromotionParent(struct dcerpc_binding_handle *h,
						TALLOC_CTX *mem_ctx,
						const char *_parent_account /* [in] [unique,charset(UTF16)] */,
						const char *_parent_password /* [in] [unique,charset(UTF16)] */,
						const char *_replica_set_name /* [in] [unique,charset(UTF16)] */,
						const char *_replica_set_type /* [in] [unique,charset(UTF16)] */,
						enum frsrpc_PartnerAuthLevel _partner_auth_level /* [in]  */,
						uint32_t ___ndr_guid_size /* [in]  */,
						WERROR *result);

struct tevent_req *dcerpc_frsrpc_FrsStartPromotionParent_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frsrpc_FrsStartPromotionParent *r);
NTSTATUS dcerpc_frsrpc_FrsStartPromotionParent_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frsrpc_FrsStartPromotionParent_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frsrpc_FrsStartPromotionParent *r);
struct tevent_req *dcerpc_frsrpc_FrsStartPromotionParent_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      const char *_parent_account /* [in] [charset(UTF16),unique] */,
							      const char *_parent_password /* [in] [unique,charset(UTF16)] */,
							      const char *_replica_set_name /* [in] [unique,charset(UTF16)] */,
							      const char *_replica_set_type /* [in] [charset(UTF16),unique] */,
							      const char *_connection_name /* [in] [unique,charset(UTF16)] */,
							      const char *_partner_name /* [in] [unique,charset(UTF16)] */,
							      const char *_partner_princ_name /* [in] [charset(UTF16),unique] */,
							      enum frsrpc_PartnerAuthLevel _partner_auth_level /* [in]  */,
							      uint32_t ___ndr_guid_size /* [in] [value(16),range(16,16)] */,
							      struct GUID *_connection_guid /* [in] [subcontext_size(16),subcontext(4),unique] */,
							      struct GUID *_partner_guid /* [in] [subcontext(4),subcontext_size(16),unique] */,
							      struct GUID *_parent_guid /* [in,out] [unique,subcontext(4),subcontext_size(16)] */);
NTSTATUS dcerpc_frsrpc_FrsStartPromotionParent_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    WERROR *result);
NTSTATUS dcerpc_frsrpc_FrsStartPromotionParent(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       const char *_parent_account /* [in] [charset(UTF16),unique] */,
					       const char *_parent_password /* [in] [unique,charset(UTF16)] */,
					       const char *_replica_set_name /* [in] [unique,charset(UTF16)] */,
					       const char *_replica_set_type /* [in] [charset(UTF16),unique] */,
					       const char *_connection_name /* [in] [unique,charset(UTF16)] */,
					       const char *_partner_name /* [in] [unique,charset(UTF16)] */,
					       const char *_partner_princ_name /* [in] [charset(UTF16),unique] */,
					       enum frsrpc_PartnerAuthLevel _partner_auth_level /* [in]  */,
					       uint32_t ___ndr_guid_size /* [in] [value(16),range(16,16)] */,
					       struct GUID *_connection_guid /* [in] [subcontext_size(16),subcontext(4),unique] */,
					       struct GUID *_partner_guid /* [in] [subcontext(4),subcontext_size(16),unique] */,
					       struct GUID *_parent_guid /* [in,out] [unique,subcontext(4),subcontext_size(16)] */,
					       WERROR *result);

struct tevent_req *dcerpc_frsrpc_FrsNOP_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct frsrpc_FrsNOP *r);
NTSTATUS dcerpc_frsrpc_FrsNOP_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_frsrpc_FrsNOP_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct frsrpc_FrsNOP *r);
struct tevent_req *dcerpc_frsrpc_FrsNOP_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_frsrpc_FrsNOP_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   WERROR *result);
NTSTATUS dcerpc_frsrpc_FrsNOP(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      WERROR *result);

#endif /* _HEADER_RPC_frsrpc */
