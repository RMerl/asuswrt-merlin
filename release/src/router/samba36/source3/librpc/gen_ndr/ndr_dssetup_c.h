#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/dssetup.h"
#ifndef _HEADER_RPC_dssetup
#define _HEADER_RPC_dssetup

extern const struct ndr_interface_table ndr_table_dssetup;

struct tevent_req *dcerpc_dssetup_DsRoleGetPrimaryDomainInformation_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dssetup_DsRoleGetPrimaryDomainInformation *r);
NTSTATUS dcerpc_dssetup_DsRoleGetPrimaryDomainInformation_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dssetup_DsRoleGetPrimaryDomainInformation_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dssetup_DsRoleGetPrimaryDomainInformation *r);
struct tevent_req *dcerpc_dssetup_DsRoleGetPrimaryDomainInformation_send(TALLOC_CTX *mem_ctx,
									 struct tevent_context *ev,
									 struct dcerpc_binding_handle *h,
									 enum dssetup_DsRoleInfoLevel _level /* [in]  */,
									 union dssetup_DsRoleInfo *_info /* [out] [switch_is(level),unique] */);
NTSTATUS dcerpc_dssetup_DsRoleGetPrimaryDomainInformation_recv(struct tevent_req *req,
							       TALLOC_CTX *mem_ctx,
							       WERROR *result);
NTSTATUS dcerpc_dssetup_DsRoleGetPrimaryDomainInformation(struct dcerpc_binding_handle *h,
							  TALLOC_CTX *mem_ctx,
							  enum dssetup_DsRoleInfoLevel _level /* [in]  */,
							  union dssetup_DsRoleInfo *_info /* [out] [switch_is(level),unique] */,
							  WERROR *result);

#endif /* _HEADER_RPC_dssetup */
