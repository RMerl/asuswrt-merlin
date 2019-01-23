#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/dfs.h"
#ifndef _HEADER_RPC_netdfs
#define _HEADER_RPC_netdfs

extern const struct ndr_interface_table ndr_table_netdfs;

struct tevent_req *dcerpc_dfs_GetManagerVersion_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_GetManagerVersion *r);
NTSTATUS dcerpc_dfs_GetManagerVersion_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_GetManagerVersion_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_GetManagerVersion *r);
struct tevent_req *dcerpc_dfs_GetManagerVersion_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     enum dfs_ManagerVersion *_version /* [out] [ref] */);
NTSTATUS dcerpc_dfs_GetManagerVersion_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_GetManagerVersion(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      enum dfs_ManagerVersion *_version /* [out] [ref] */);

struct tevent_req *dcerpc_dfs_Add_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_Add *r);
NTSTATUS dcerpc_dfs_Add_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_Add_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_Add *r);
struct tevent_req *dcerpc_dfs_Add_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       struct dcerpc_binding_handle *h,
				       const char *_path /* [in] [ref,charset(UTF16)] */,
				       const char *_server /* [in] [ref,charset(UTF16)] */,
				       const char *_share /* [in] [unique,charset(UTF16)] */,
				       const char *_comment /* [in] [charset(UTF16),unique] */,
				       uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_dfs_Add_recv(struct tevent_req *req,
			     TALLOC_CTX *mem_ctx,
			     WERROR *result);
NTSTATUS dcerpc_dfs_Add(struct dcerpc_binding_handle *h,
			TALLOC_CTX *mem_ctx,
			const char *_path /* [in] [ref,charset(UTF16)] */,
			const char *_server /* [in] [ref,charset(UTF16)] */,
			const char *_share /* [in] [unique,charset(UTF16)] */,
			const char *_comment /* [in] [charset(UTF16),unique] */,
			uint32_t _flags /* [in]  */,
			WERROR *result);

struct tevent_req *dcerpc_dfs_Remove_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_Remove *r);
NTSTATUS dcerpc_dfs_Remove_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_Remove_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_Remove *r);
struct tevent_req *dcerpc_dfs_Remove_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  const char *_dfs_entry_path /* [in] [charset(UTF16),ref] */,
					  const char *_servername /* [in] [charset(UTF16),unique] */,
					  const char *_sharename /* [in] [charset(UTF16),unique] */);
NTSTATUS dcerpc_dfs_Remove_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS dcerpc_dfs_Remove(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   const char *_dfs_entry_path /* [in] [charset(UTF16),ref] */,
			   const char *_servername /* [in] [charset(UTF16),unique] */,
			   const char *_sharename /* [in] [charset(UTF16),unique] */,
			   WERROR *result);

struct tevent_req *dcerpc_dfs_SetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_SetInfo *r);
NTSTATUS dcerpc_dfs_SetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_SetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_SetInfo *r);
struct tevent_req *dcerpc_dfs_SetInfo_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   const char *_dfs_entry_path /* [in] [charset(UTF16)] */,
					   const char *_servername /* [in] [charset(UTF16),unique] */,
					   const char *_sharename /* [in] [charset(UTF16),unique] */,
					   uint32_t _level /* [in]  */,
					   union dfs_Info *_info /* [in] [switch_is(level),ref] */);
NTSTATUS dcerpc_dfs_SetInfo_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 WERROR *result);
NTSTATUS dcerpc_dfs_SetInfo(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    const char *_dfs_entry_path /* [in] [charset(UTF16)] */,
			    const char *_servername /* [in] [charset(UTF16),unique] */,
			    const char *_sharename /* [in] [charset(UTF16),unique] */,
			    uint32_t _level /* [in]  */,
			    union dfs_Info *_info /* [in] [switch_is(level),ref] */,
			    WERROR *result);

struct tevent_req *dcerpc_dfs_GetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_GetInfo *r);
NTSTATUS dcerpc_dfs_GetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_GetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_GetInfo *r);
struct tevent_req *dcerpc_dfs_GetInfo_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   const char *_dfs_entry_path /* [in] [charset(UTF16)] */,
					   const char *_servername /* [in] [unique,charset(UTF16)] */,
					   const char *_sharename /* [in] [charset(UTF16),unique] */,
					   uint32_t _level /* [in]  */,
					   union dfs_Info *_info /* [out] [switch_is(level),ref] */);
NTSTATUS dcerpc_dfs_GetInfo_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 WERROR *result);
NTSTATUS dcerpc_dfs_GetInfo(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    const char *_dfs_entry_path /* [in] [charset(UTF16)] */,
			    const char *_servername /* [in] [unique,charset(UTF16)] */,
			    const char *_sharename /* [in] [charset(UTF16),unique] */,
			    uint32_t _level /* [in]  */,
			    union dfs_Info *_info /* [out] [switch_is(level),ref] */,
			    WERROR *result);

struct tevent_req *dcerpc_dfs_Enum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_Enum *r);
NTSTATUS dcerpc_dfs_Enum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_Enum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_Enum *r);
struct tevent_req *dcerpc_dfs_Enum_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct dcerpc_binding_handle *h,
					uint32_t _level /* [in]  */,
					uint32_t _bufsize /* [in]  */,
					struct dfs_EnumStruct *_info /* [in,out] [unique] */,
					uint32_t *_total /* [in,out] [unique] */);
NTSTATUS dcerpc_dfs_Enum_recv(struct tevent_req *req,
			      TALLOC_CTX *mem_ctx,
			      WERROR *result);
NTSTATUS dcerpc_dfs_Enum(struct dcerpc_binding_handle *h,
			 TALLOC_CTX *mem_ctx,
			 uint32_t _level /* [in]  */,
			 uint32_t _bufsize /* [in]  */,
			 struct dfs_EnumStruct *_info /* [in,out] [unique] */,
			 uint32_t *_total /* [in,out] [unique] */,
			 WERROR *result);

struct tevent_req *dcerpc_dfs_AddFtRoot_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_AddFtRoot *r);
NTSTATUS dcerpc_dfs_AddFtRoot_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_AddFtRoot_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_AddFtRoot *r);
struct tevent_req *dcerpc_dfs_AddFtRoot_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     const char *_servername /* [in] [charset(UTF16)] */,
					     const char *_dns_servername /* [in] [charset(UTF16)] */,
					     const char *_dfsname /* [in] [charset(UTF16)] */,
					     const char *_rootshare /* [in] [charset(UTF16)] */,
					     const char *_comment /* [in] [charset(UTF16)] */,
					     const char *_dfs_config_dn /* [in] [charset(UTF16)] */,
					     uint8_t _unknown1 /* [in]  */,
					     uint32_t _flags /* [in]  */,
					     struct dfs_UnknownStruct **_unknown2 /* [in,out] [unique] */);
NTSTATUS dcerpc_dfs_AddFtRoot_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   WERROR *result);
NTSTATUS dcerpc_dfs_AddFtRoot(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      const char *_servername /* [in] [charset(UTF16)] */,
			      const char *_dns_servername /* [in] [charset(UTF16)] */,
			      const char *_dfsname /* [in] [charset(UTF16)] */,
			      const char *_rootshare /* [in] [charset(UTF16)] */,
			      const char *_comment /* [in] [charset(UTF16)] */,
			      const char *_dfs_config_dn /* [in] [charset(UTF16)] */,
			      uint8_t _unknown1 /* [in]  */,
			      uint32_t _flags /* [in]  */,
			      struct dfs_UnknownStruct **_unknown2 /* [in,out] [unique] */,
			      WERROR *result);

struct tevent_req *dcerpc_dfs_RemoveFtRoot_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_RemoveFtRoot *r);
NTSTATUS dcerpc_dfs_RemoveFtRoot_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_RemoveFtRoot_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_RemoveFtRoot *r);
struct tevent_req *dcerpc_dfs_RemoveFtRoot_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char *_servername /* [in] [charset(UTF16)] */,
						const char *_dns_servername /* [in] [charset(UTF16)] */,
						const char *_dfsname /* [in] [charset(UTF16)] */,
						const char *_rootshare /* [in] [charset(UTF16)] */,
						uint32_t _flags /* [in]  */,
						struct dfs_UnknownStruct **_unknown /* [in,out] [unique] */);
NTSTATUS dcerpc_dfs_RemoveFtRoot_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_dfs_RemoveFtRoot(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char *_servername /* [in] [charset(UTF16)] */,
				 const char *_dns_servername /* [in] [charset(UTF16)] */,
				 const char *_dfsname /* [in] [charset(UTF16)] */,
				 const char *_rootshare /* [in] [charset(UTF16)] */,
				 uint32_t _flags /* [in]  */,
				 struct dfs_UnknownStruct **_unknown /* [in,out] [unique] */,
				 WERROR *result);

struct tevent_req *dcerpc_dfs_AddStdRoot_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_AddStdRoot *r);
NTSTATUS dcerpc_dfs_AddStdRoot_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_AddStdRoot_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_AddStdRoot *r);
struct tevent_req *dcerpc_dfs_AddStdRoot_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      const char *_servername /* [in] [charset(UTF16)] */,
					      const char *_rootshare /* [in] [charset(UTF16)] */,
					      const char *_comment /* [in] [charset(UTF16)] */,
					      uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_dfs_AddStdRoot_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_dfs_AddStdRoot(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       const char *_servername /* [in] [charset(UTF16)] */,
			       const char *_rootshare /* [in] [charset(UTF16)] */,
			       const char *_comment /* [in] [charset(UTF16)] */,
			       uint32_t _flags /* [in]  */,
			       WERROR *result);

struct tevent_req *dcerpc_dfs_RemoveStdRoot_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_RemoveStdRoot *r);
NTSTATUS dcerpc_dfs_RemoveStdRoot_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_RemoveStdRoot_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_RemoveStdRoot *r);
struct tevent_req *dcerpc_dfs_RemoveStdRoot_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char *_servername /* [in] [charset(UTF16)] */,
						 const char *_rootshare /* [in] [charset(UTF16)] */,
						 uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_dfs_RemoveStdRoot_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_dfs_RemoveStdRoot(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char *_servername /* [in] [charset(UTF16)] */,
				  const char *_rootshare /* [in] [charset(UTF16)] */,
				  uint32_t _flags /* [in]  */,
				  WERROR *result);

struct tevent_req *dcerpc_dfs_ManagerInitialize_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_ManagerInitialize *r);
NTSTATUS dcerpc_dfs_ManagerInitialize_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_ManagerInitialize_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_ManagerInitialize *r);
struct tevent_req *dcerpc_dfs_ManagerInitialize_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     const char *_servername /* [in] [ref,charset(UTF16)] */,
						     uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_dfs_ManagerInitialize_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_dfs_ManagerInitialize(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      const char *_servername /* [in] [ref,charset(UTF16)] */,
				      uint32_t _flags /* [in]  */,
				      WERROR *result);

struct tevent_req *dcerpc_dfs_AddStdRootForced_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_AddStdRootForced *r);
NTSTATUS dcerpc_dfs_AddStdRootForced_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_AddStdRootForced_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_AddStdRootForced *r);
struct tevent_req *dcerpc_dfs_AddStdRootForced_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    const char *_servername /* [in] [charset(UTF16)] */,
						    const char *_rootshare /* [in] [charset(UTF16)] */,
						    const char *_comment /* [in] [charset(UTF16)] */,
						    const char *_store /* [in] [charset(UTF16)] */);
NTSTATUS dcerpc_dfs_AddStdRootForced_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_dfs_AddStdRootForced(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char *_servername /* [in] [charset(UTF16)] */,
				     const char *_rootshare /* [in] [charset(UTF16)] */,
				     const char *_comment /* [in] [charset(UTF16)] */,
				     const char *_store /* [in] [charset(UTF16)] */,
				     WERROR *result);

struct tevent_req *dcerpc_dfs_GetDcAddress_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_GetDcAddress *r);
NTSTATUS dcerpc_dfs_GetDcAddress_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_GetDcAddress_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_GetDcAddress *r);
struct tevent_req *dcerpc_dfs_GetDcAddress_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char *_servername /* [in] [charset(UTF16)] */,
						const char **_server_fullname /* [in,out] [ref,charset(UTF16)] */,
						uint8_t *_is_root /* [in,out] [ref] */,
						uint32_t *_ttl /* [in,out] [ref] */);
NTSTATUS dcerpc_dfs_GetDcAddress_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_dfs_GetDcAddress(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char *_servername /* [in] [charset(UTF16)] */,
				 const char **_server_fullname /* [in,out] [ref,charset(UTF16)] */,
				 uint8_t *_is_root /* [in,out] [ref] */,
				 uint32_t *_ttl /* [in,out] [ref] */,
				 WERROR *result);

struct tevent_req *dcerpc_dfs_SetDcAddress_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_SetDcAddress *r);
NTSTATUS dcerpc_dfs_SetDcAddress_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_SetDcAddress_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_SetDcAddress *r);
struct tevent_req *dcerpc_dfs_SetDcAddress_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char *_servername /* [in] [charset(UTF16)] */,
						const char *_server_fullname /* [in] [charset(UTF16)] */,
						uint32_t _flags /* [in]  */,
						uint32_t _ttl /* [in]  */);
NTSTATUS dcerpc_dfs_SetDcAddress_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_dfs_SetDcAddress(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char *_servername /* [in] [charset(UTF16)] */,
				 const char *_server_fullname /* [in] [charset(UTF16)] */,
				 uint32_t _flags /* [in]  */,
				 uint32_t _ttl /* [in]  */,
				 WERROR *result);

struct tevent_req *dcerpc_dfs_FlushFtTable_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_FlushFtTable *r);
NTSTATUS dcerpc_dfs_FlushFtTable_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_FlushFtTable_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_FlushFtTable *r);
struct tevent_req *dcerpc_dfs_FlushFtTable_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char *_servername /* [in] [charset(UTF16)] */,
						const char *_rootshare /* [in] [charset(UTF16)] */);
NTSTATUS dcerpc_dfs_FlushFtTable_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_dfs_FlushFtTable(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char *_servername /* [in] [charset(UTF16)] */,
				 const char *_rootshare /* [in] [charset(UTF16)] */,
				 WERROR *result);

struct tevent_req *dcerpc_dfs_EnumEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct dfs_EnumEx *r);
NTSTATUS dcerpc_dfs_EnumEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_dfs_EnumEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct dfs_EnumEx *r);
struct tevent_req *dcerpc_dfs_EnumEx_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  const char *_dfs_name /* [in] [charset(UTF16)] */,
					  uint32_t _level /* [in]  */,
					  uint32_t _bufsize /* [in]  */,
					  struct dfs_EnumStruct *_info /* [in,out] [unique] */,
					  uint32_t *_total /* [in,out] [unique] */);
NTSTATUS dcerpc_dfs_EnumEx_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS dcerpc_dfs_EnumEx(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   const char *_dfs_name /* [in] [charset(UTF16)] */,
			   uint32_t _level /* [in]  */,
			   uint32_t _bufsize /* [in]  */,
			   struct dfs_EnumStruct *_info /* [in,out] [unique] */,
			   uint32_t *_total /* [in,out] [unique] */,
			   WERROR *result);

#endif /* _HEADER_RPC_netdfs */
