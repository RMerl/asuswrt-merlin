#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/rot.h"
#ifndef _HEADER_RPC_rot
#define _HEADER_RPC_rot

extern const struct ndr_interface_table ndr_table_rot;

struct tevent_req *dcerpc_rot_add_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rot_add *r);
NTSTATUS dcerpc_rot_add_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rot_add_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rot_add *r);
struct tevent_req *dcerpc_rot_add_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       struct dcerpc_binding_handle *h,
				       uint32_t _flags /* [in]  */,
				       struct MInterfacePointer *_unk /* [in] [ref] */,
				       struct MInterfacePointer *_moniker /* [in] [ref] */,
				       uint32_t *_rotid /* [out] [ref] */);
NTSTATUS dcerpc_rot_add_recv(struct tevent_req *req,
			     TALLOC_CTX *mem_ctx,
			     WERROR *result);
NTSTATUS dcerpc_rot_add(struct dcerpc_binding_handle *h,
			TALLOC_CTX *mem_ctx,
			uint32_t _flags /* [in]  */,
			struct MInterfacePointer *_unk /* [in] [ref] */,
			struct MInterfacePointer *_moniker /* [in] [ref] */,
			uint32_t *_rotid /* [out] [ref] */,
			WERROR *result);

struct tevent_req *dcerpc_rot_remove_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rot_remove *r);
NTSTATUS dcerpc_rot_remove_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rot_remove_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rot_remove *r);
struct tevent_req *dcerpc_rot_remove_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  uint32_t _rotid /* [in]  */);
NTSTATUS dcerpc_rot_remove_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS dcerpc_rot_remove(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   uint32_t _rotid /* [in]  */,
			   WERROR *result);

struct tevent_req *dcerpc_rot_is_listed_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rot_is_listed *r);
NTSTATUS dcerpc_rot_is_listed_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rot_is_listed_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rot_is_listed *r);
struct tevent_req *dcerpc_rot_is_listed_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     struct MInterfacePointer *_moniker /* [in] [ref] */);
NTSTATUS dcerpc_rot_is_listed_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   WERROR *result);
NTSTATUS dcerpc_rot_is_listed(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      struct MInterfacePointer *_moniker /* [in] [ref] */,
			      WERROR *result);

struct tevent_req *dcerpc_rot_get_interface_pointer_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rot_get_interface_pointer *r);
NTSTATUS dcerpc_rot_get_interface_pointer_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rot_get_interface_pointer_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rot_get_interface_pointer *r);
struct tevent_req *dcerpc_rot_get_interface_pointer_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct MInterfacePointer *_moniker /* [in] [ref] */,
							 struct MInterfacePointer *_ip /* [out] [ref] */);
NTSTATUS dcerpc_rot_get_interface_pointer_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_rot_get_interface_pointer(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct MInterfacePointer *_moniker /* [in] [ref] */,
					  struct MInterfacePointer *_ip /* [out] [ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_rot_set_modification_time_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rot_set_modification_time *r);
NTSTATUS dcerpc_rot_set_modification_time_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rot_set_modification_time_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rot_set_modification_time *r);
struct tevent_req *dcerpc_rot_set_modification_time_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 uint32_t _rotid /* [in]  */,
							 NTTIME *_t /* [in] [ref] */);
NTSTATUS dcerpc_rot_set_modification_time_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_rot_set_modification_time(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  uint32_t _rotid /* [in]  */,
					  NTTIME *_t /* [in] [ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_rot_get_modification_time_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rot_get_modification_time *r);
NTSTATUS dcerpc_rot_get_modification_time_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rot_get_modification_time_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rot_get_modification_time *r);
struct tevent_req *dcerpc_rot_get_modification_time_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct MInterfacePointer *_moniker /* [in] [ref] */,
							 NTTIME *_t /* [out] [ref] */);
NTSTATUS dcerpc_rot_get_modification_time_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_rot_get_modification_time(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct MInterfacePointer *_moniker /* [in] [ref] */,
					  NTTIME *_t /* [out] [ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_rot_enum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rot_enum *r);
NTSTATUS dcerpc_rot_enum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rot_enum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rot_enum *r);
struct tevent_req *dcerpc_rot_enum_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct dcerpc_binding_handle *h,
					struct MInterfacePointer *_EnumMoniker /* [out] [ref] */);
NTSTATUS dcerpc_rot_enum_recv(struct tevent_req *req,
			      TALLOC_CTX *mem_ctx,
			      WERROR *result);
NTSTATUS dcerpc_rot_enum(struct dcerpc_binding_handle *h,
			 TALLOC_CTX *mem_ctx,
			 struct MInterfacePointer *_EnumMoniker /* [out] [ref] */,
			 WERROR *result);

#endif /* _HEADER_RPC_rot */
