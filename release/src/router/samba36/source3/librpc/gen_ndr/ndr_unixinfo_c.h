#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/unixinfo.h"
#ifndef _HEADER_RPC_unixinfo
#define _HEADER_RPC_unixinfo

extern const struct ndr_interface_table ndr_table_unixinfo;

struct tevent_req *dcerpc_unixinfo_SidToUid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct unixinfo_SidToUid *r);
NTSTATUS dcerpc_unixinfo_SidToUid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_unixinfo_SidToUid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct unixinfo_SidToUid *r);
struct tevent_req *dcerpc_unixinfo_SidToUid_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct dom_sid _sid /* [in]  */,
						 uint64_t *_uid /* [out] [ref] */);
NTSTATUS dcerpc_unixinfo_SidToUid_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_unixinfo_SidToUid(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct dom_sid _sid /* [in]  */,
				  uint64_t *_uid /* [out] [ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_unixinfo_UidToSid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct unixinfo_UidToSid *r);
NTSTATUS dcerpc_unixinfo_UidToSid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_unixinfo_UidToSid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct unixinfo_UidToSid *r);
struct tevent_req *dcerpc_unixinfo_UidToSid_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 uint64_t _uid /* [in]  */,
						 struct dom_sid *_sid /* [out] [ref] */);
NTSTATUS dcerpc_unixinfo_UidToSid_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_unixinfo_UidToSid(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  uint64_t _uid /* [in]  */,
				  struct dom_sid *_sid /* [out] [ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_unixinfo_SidToGid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct unixinfo_SidToGid *r);
NTSTATUS dcerpc_unixinfo_SidToGid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_unixinfo_SidToGid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct unixinfo_SidToGid *r);
struct tevent_req *dcerpc_unixinfo_SidToGid_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct dom_sid _sid /* [in]  */,
						 uint64_t *_gid /* [out] [ref] */);
NTSTATUS dcerpc_unixinfo_SidToGid_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_unixinfo_SidToGid(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct dom_sid _sid /* [in]  */,
				  uint64_t *_gid /* [out] [ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_unixinfo_GidToSid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct unixinfo_GidToSid *r);
NTSTATUS dcerpc_unixinfo_GidToSid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_unixinfo_GidToSid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct unixinfo_GidToSid *r);
struct tevent_req *dcerpc_unixinfo_GidToSid_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 uint64_t _gid /* [in]  */,
						 struct dom_sid *_sid /* [out] [ref] */);
NTSTATUS dcerpc_unixinfo_GidToSid_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_unixinfo_GidToSid(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  uint64_t _gid /* [in]  */,
				  struct dom_sid *_sid /* [out] [ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_unixinfo_GetPWUid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct unixinfo_GetPWUid *r);
NTSTATUS dcerpc_unixinfo_GetPWUid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_unixinfo_GetPWUid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct unixinfo_GetPWUid *r);
struct tevent_req *dcerpc_unixinfo_GetPWUid_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 uint32_t *_count /* [in,out] [ref,range(0,1023)] */,
						 uint64_t *_uids /* [in] [size_is(*count)] */,
						 struct unixinfo_GetPWUidInfo *_infos /* [out] [size_is(*count)] */);
NTSTATUS dcerpc_unixinfo_GetPWUid_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_unixinfo_GetPWUid(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  uint32_t *_count /* [in,out] [ref,range(0,1023)] */,
				  uint64_t *_uids /* [in] [size_is(*count)] */,
				  struct unixinfo_GetPWUidInfo *_infos /* [out] [size_is(*count)] */,
				  NTSTATUS *result);

#endif /* _HEADER_RPC_unixinfo */
