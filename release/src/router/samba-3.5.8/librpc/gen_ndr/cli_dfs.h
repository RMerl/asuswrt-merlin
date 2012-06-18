#include "../librpc/gen_ndr/ndr_dfs.h"
#ifndef __CLI_NETDFS__
#define __CLI_NETDFS__
struct tevent_req *rpccli_dfs_GetManagerVersion_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli,
						     enum dfs_ManagerVersion *_version /* [out] [ref] */);
NTSTATUS rpccli_dfs_GetManagerVersion_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_dfs_GetManagerVersion(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      enum dfs_ManagerVersion *version /* [out] [ref] */);
struct tevent_req *rpccli_dfs_Add_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       struct rpc_pipe_client *cli,
				       const char *_path /* [in] [ref,charset(UTF16)] */,
				       const char *_server /* [in] [ref,charset(UTF16)] */,
				       const char *_share /* [in] [unique,charset(UTF16)] */,
				       const char *_comment /* [in] [unique,charset(UTF16)] */,
				       uint32_t _flags /* [in]  */);
NTSTATUS rpccli_dfs_Add_recv(struct tevent_req *req,
			     TALLOC_CTX *mem_ctx,
			     WERROR *result);
NTSTATUS rpccli_dfs_Add(struct rpc_pipe_client *cli,
			TALLOC_CTX *mem_ctx,
			const char *path /* [in] [ref,charset(UTF16)] */,
			const char *server /* [in] [ref,charset(UTF16)] */,
			const char *share /* [in] [unique,charset(UTF16)] */,
			const char *comment /* [in] [unique,charset(UTF16)] */,
			uint32_t flags /* [in]  */,
			WERROR *werror);
struct tevent_req *rpccli_dfs_Remove_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli,
					  const char *_dfs_entry_path /* [in] [ref,charset(UTF16)] */,
					  const char *_servername /* [in] [unique,charset(UTF16)] */,
					  const char *_sharename /* [in] [unique,charset(UTF16)] */);
NTSTATUS rpccli_dfs_Remove_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_dfs_Remove(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   const char *dfs_entry_path /* [in] [ref,charset(UTF16)] */,
			   const char *servername /* [in] [unique,charset(UTF16)] */,
			   const char *sharename /* [in] [unique,charset(UTF16)] */,
			   WERROR *werror);
struct tevent_req *rpccli_dfs_SetInfo_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct rpc_pipe_client *cli,
					   const char *_dfs_entry_path /* [in] [charset(UTF16)] */,
					   const char *_servername /* [in] [unique,charset(UTF16)] */,
					   const char *_sharename /* [in] [unique,charset(UTF16)] */,
					   uint32_t _level /* [in]  */,
					   union dfs_Info *_info /* [in] [ref,switch_is(level)] */);
NTSTATUS rpccli_dfs_SetInfo_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 WERROR *result);
NTSTATUS rpccli_dfs_SetInfo(struct rpc_pipe_client *cli,
			    TALLOC_CTX *mem_ctx,
			    const char *dfs_entry_path /* [in] [charset(UTF16)] */,
			    const char *servername /* [in] [unique,charset(UTF16)] */,
			    const char *sharename /* [in] [unique,charset(UTF16)] */,
			    uint32_t level /* [in]  */,
			    union dfs_Info *info /* [in] [ref,switch_is(level)] */,
			    WERROR *werror);
struct tevent_req *rpccli_dfs_GetInfo_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct rpc_pipe_client *cli,
					   const char *_dfs_entry_path /* [in] [charset(UTF16)] */,
					   const char *_servername /* [in] [unique,charset(UTF16)] */,
					   const char *_sharename /* [in] [unique,charset(UTF16)] */,
					   uint32_t _level /* [in]  */,
					   union dfs_Info *_info /* [out] [ref,switch_is(level)] */);
NTSTATUS rpccli_dfs_GetInfo_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 WERROR *result);
NTSTATUS rpccli_dfs_GetInfo(struct rpc_pipe_client *cli,
			    TALLOC_CTX *mem_ctx,
			    const char *dfs_entry_path /* [in] [charset(UTF16)] */,
			    const char *servername /* [in] [unique,charset(UTF16)] */,
			    const char *sharename /* [in] [unique,charset(UTF16)] */,
			    uint32_t level /* [in]  */,
			    union dfs_Info *info /* [out] [ref,switch_is(level)] */,
			    WERROR *werror);
struct tevent_req *rpccli_dfs_Enum_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct rpc_pipe_client *cli,
					uint32_t _level /* [in]  */,
					uint32_t _bufsize /* [in]  */,
					struct dfs_EnumStruct *_info /* [in,out] [unique] */,
					uint32_t *_total /* [in,out] [unique] */);
NTSTATUS rpccli_dfs_Enum_recv(struct tevent_req *req,
			      TALLOC_CTX *mem_ctx,
			      WERROR *result);
NTSTATUS rpccli_dfs_Enum(struct rpc_pipe_client *cli,
			 TALLOC_CTX *mem_ctx,
			 uint32_t level /* [in]  */,
			 uint32_t bufsize /* [in]  */,
			 struct dfs_EnumStruct *info /* [in,out] [unique] */,
			 uint32_t *total /* [in,out] [unique] */,
			 WERROR *werror);
struct tevent_req *rpccli_dfs_Rename_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_dfs_Rename_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_dfs_Rename(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_dfs_Move_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct rpc_pipe_client *cli);
NTSTATUS rpccli_dfs_Move_recv(struct tevent_req *req,
			      TALLOC_CTX *mem_ctx,
			      WERROR *result);
NTSTATUS rpccli_dfs_Move(struct rpc_pipe_client *cli,
			 TALLOC_CTX *mem_ctx,
			 WERROR *werror);
struct tevent_req *rpccli_dfs_ManagerGetConfigInfo_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli);
NTSTATUS rpccli_dfs_ManagerGetConfigInfo_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_dfs_ManagerGetConfigInfo(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 WERROR *werror);
struct tevent_req *rpccli_dfs_ManagerSendSiteInfo_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct rpc_pipe_client *cli);
NTSTATUS rpccli_dfs_ManagerSendSiteInfo_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS rpccli_dfs_ManagerSendSiteInfo(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					WERROR *werror);
struct tevent_req *rpccli_dfs_AddFtRoot_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli,
					     const char *_servername /* [in] [charset(UTF16)] */,
					     const char *_dns_servername /* [in] [charset(UTF16)] */,
					     const char *_dfsname /* [in] [charset(UTF16)] */,
					     const char *_rootshare /* [in] [charset(UTF16)] */,
					     const char *_comment /* [in] [charset(UTF16)] */,
					     const char *_dfs_config_dn /* [in] [charset(UTF16)] */,
					     uint8_t _unknown1 /* [in]  */,
					     uint32_t _flags /* [in]  */,
					     struct dfs_UnknownStruct **_unknown2 /* [in,out] [unique] */);
NTSTATUS rpccli_dfs_AddFtRoot_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   WERROR *result);
NTSTATUS rpccli_dfs_AddFtRoot(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      const char *servername /* [in] [charset(UTF16)] */,
			      const char *dns_servername /* [in] [charset(UTF16)] */,
			      const char *dfsname /* [in] [charset(UTF16)] */,
			      const char *rootshare /* [in] [charset(UTF16)] */,
			      const char *comment /* [in] [charset(UTF16)] */,
			      const char *dfs_config_dn /* [in] [charset(UTF16)] */,
			      uint8_t unknown1 /* [in]  */,
			      uint32_t flags /* [in]  */,
			      struct dfs_UnknownStruct **unknown2 /* [in,out] [unique] */,
			      WERROR *werror);
struct tevent_req *rpccli_dfs_RemoveFtRoot_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						const char *_servername /* [in] [charset(UTF16)] */,
						const char *_dns_servername /* [in] [charset(UTF16)] */,
						const char *_dfsname /* [in] [charset(UTF16)] */,
						const char *_rootshare /* [in] [charset(UTF16)] */,
						uint32_t _flags /* [in]  */,
						struct dfs_UnknownStruct **_unknown /* [in,out] [unique] */);
NTSTATUS rpccli_dfs_RemoveFtRoot_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS rpccli_dfs_RemoveFtRoot(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 const char *servername /* [in] [charset(UTF16)] */,
				 const char *dns_servername /* [in] [charset(UTF16)] */,
				 const char *dfsname /* [in] [charset(UTF16)] */,
				 const char *rootshare /* [in] [charset(UTF16)] */,
				 uint32_t flags /* [in]  */,
				 struct dfs_UnknownStruct **unknown /* [in,out] [unique] */,
				 WERROR *werror);
struct tevent_req *rpccli_dfs_AddStdRoot_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli,
					      const char *_servername /* [in] [charset(UTF16)] */,
					      const char *_rootshare /* [in] [charset(UTF16)] */,
					      const char *_comment /* [in] [charset(UTF16)] */,
					      uint32_t _flags /* [in]  */);
NTSTATUS rpccli_dfs_AddStdRoot_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS rpccli_dfs_AddStdRoot(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       const char *servername /* [in] [charset(UTF16)] */,
			       const char *rootshare /* [in] [charset(UTF16)] */,
			       const char *comment /* [in] [charset(UTF16)] */,
			       uint32_t flags /* [in]  */,
			       WERROR *werror);
struct tevent_req *rpccli_dfs_RemoveStdRoot_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli,
						 const char *_servername /* [in] [charset(UTF16)] */,
						 const char *_rootshare /* [in] [charset(UTF16)] */,
						 uint32_t _flags /* [in]  */);
NTSTATUS rpccli_dfs_RemoveStdRoot_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS rpccli_dfs_RemoveStdRoot(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  const char *servername /* [in] [charset(UTF16)] */,
				  const char *rootshare /* [in] [charset(UTF16)] */,
				  uint32_t flags /* [in]  */,
				  WERROR *werror);
struct tevent_req *rpccli_dfs_ManagerInitialize_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli,
						     const char *_servername /* [in] [ref,charset(UTF16)] */,
						     uint32_t _flags /* [in]  */);
NTSTATUS rpccli_dfs_ManagerInitialize_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS rpccli_dfs_ManagerInitialize(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      const char *servername /* [in] [ref,charset(UTF16)] */,
				      uint32_t flags /* [in]  */,
				      WERROR *werror);
struct tevent_req *rpccli_dfs_AddStdRootForced_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    const char *_servername /* [in] [charset(UTF16)] */,
						    const char *_rootshare /* [in] [charset(UTF16)] */,
						    const char *_comment /* [in] [charset(UTF16)] */,
						    const char *_store /* [in] [charset(UTF16)] */);
NTSTATUS rpccli_dfs_AddStdRootForced_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_dfs_AddStdRootForced(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     const char *servername /* [in] [charset(UTF16)] */,
				     const char *rootshare /* [in] [charset(UTF16)] */,
				     const char *comment /* [in] [charset(UTF16)] */,
				     const char *store /* [in] [charset(UTF16)] */,
				     WERROR *werror);
struct tevent_req *rpccli_dfs_GetDcAddress_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						const char *_servername /* [in] [charset(UTF16)] */,
						const char **_server_fullname /* [in,out] [ref,charset(UTF16)] */,
						uint8_t *_is_root /* [in,out] [ref] */,
						uint32_t *_ttl /* [in,out] [ref] */);
NTSTATUS rpccli_dfs_GetDcAddress_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS rpccli_dfs_GetDcAddress(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 const char *servername /* [in] [charset(UTF16)] */,
				 const char **server_fullname /* [in,out] [ref,charset(UTF16)] */,
				 uint8_t *is_root /* [in,out] [ref] */,
				 uint32_t *ttl /* [in,out] [ref] */,
				 WERROR *werror);
struct tevent_req *rpccli_dfs_SetDcAddress_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						const char *_servername /* [in] [charset(UTF16)] */,
						const char *_server_fullname /* [in] [charset(UTF16)] */,
						uint32_t _flags /* [in]  */,
						uint32_t _ttl /* [in]  */);
NTSTATUS rpccli_dfs_SetDcAddress_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS rpccli_dfs_SetDcAddress(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 const char *servername /* [in] [charset(UTF16)] */,
				 const char *server_fullname /* [in] [charset(UTF16)] */,
				 uint32_t flags /* [in]  */,
				 uint32_t ttl /* [in]  */,
				 WERROR *werror);
struct tevent_req *rpccli_dfs_FlushFtTable_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						const char *_servername /* [in] [charset(UTF16)] */,
						const char *_rootshare /* [in] [charset(UTF16)] */);
NTSTATUS rpccli_dfs_FlushFtTable_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS rpccli_dfs_FlushFtTable(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 const char *servername /* [in] [charset(UTF16)] */,
				 const char *rootshare /* [in] [charset(UTF16)] */,
				 WERROR *werror);
struct tevent_req *rpccli_dfs_Add2_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct rpc_pipe_client *cli);
NTSTATUS rpccli_dfs_Add2_recv(struct tevent_req *req,
			      TALLOC_CTX *mem_ctx,
			      WERROR *result);
NTSTATUS rpccli_dfs_Add2(struct rpc_pipe_client *cli,
			 TALLOC_CTX *mem_ctx,
			 WERROR *werror);
struct tevent_req *rpccli_dfs_Remove2_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct rpc_pipe_client *cli);
NTSTATUS rpccli_dfs_Remove2_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 WERROR *result);
NTSTATUS rpccli_dfs_Remove2(struct rpc_pipe_client *cli,
			    TALLOC_CTX *mem_ctx,
			    WERROR *werror);
struct tevent_req *rpccli_dfs_EnumEx_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli,
					  const char *_dfs_name /* [in] [charset(UTF16)] */,
					  uint32_t _level /* [in]  */,
					  uint32_t _bufsize /* [in]  */,
					  struct dfs_EnumStruct *_info /* [in,out] [unique] */,
					  uint32_t *_total /* [in,out] [unique] */);
NTSTATUS rpccli_dfs_EnumEx_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_dfs_EnumEx(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   const char *dfs_name /* [in] [charset(UTF16)] */,
			   uint32_t level /* [in]  */,
			   uint32_t bufsize /* [in]  */,
			   struct dfs_EnumStruct *info /* [in,out] [unique] */,
			   uint32_t *total /* [in,out] [unique] */,
			   WERROR *werror);
struct tevent_req *rpccli_dfs_SetInfo2_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct rpc_pipe_client *cli);
NTSTATUS rpccli_dfs_SetInfo2_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  WERROR *result);
NTSTATUS rpccli_dfs_SetInfo2(struct rpc_pipe_client *cli,
			     TALLOC_CTX *mem_ctx,
			     WERROR *werror);
#endif /* __CLI_NETDFS__ */
