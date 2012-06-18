#include "librpc/gen_ndr/ndr_wbint.h"
#ifndef __CLI_WBINT__
#define __CLI_WBINT__
struct tevent_req *rpccli_wbint_Ping_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli,
					  uint32_t _in_data /* [in]  */,
					  uint32_t *_out_data /* [out] [ref] */);
NTSTATUS rpccli_wbint_Ping_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_wbint_Ping(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   uint32_t in_data /* [in]  */,
			   uint32_t *out_data /* [out] [ref] */);
struct tevent_req *rpccli_wbint_LookupSid_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       struct dom_sid *_sid /* [in] [ref] */,
					       enum lsa_SidType *_type /* [out] [ref] */,
					       const char **_domain /* [out] [ref,charset(UTF8)] */,
					       const char **_name /* [out] [ref,charset(UTF8)] */);
NTSTATUS rpccli_wbint_LookupSid_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS rpccli_wbint_LookupSid(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct dom_sid *sid /* [in] [ref] */,
				enum lsa_SidType *type /* [out] [ref] */,
				const char **domain /* [out] [ref,charset(UTF8)] */,
				const char **name /* [out] [ref,charset(UTF8)] */);
struct tevent_req *rpccli_wbint_LookupName_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						const char *_domain /* [in] [ref,charset(UTF8)] */,
						const char *_name /* [in] [ref,charset(UTF8)] */,
						uint32_t _flags /* [in]  */,
						enum lsa_SidType *_type /* [out] [ref] */,
						struct dom_sid *_sid /* [out] [ref] */);
NTSTATUS rpccli_wbint_LookupName_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS rpccli_wbint_LookupName(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 const char *domain /* [in] [ref,charset(UTF8)] */,
				 const char *name /* [in] [ref,charset(UTF8)] */,
				 uint32_t flags /* [in]  */,
				 enum lsa_SidType *type /* [out] [ref] */,
				 struct dom_sid *sid /* [out] [ref] */);
struct tevent_req *rpccli_wbint_Sid2Uid_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli,
					     const char *_dom_name /* [in] [unique,charset(UTF8)] */,
					     struct dom_sid *_sid /* [in] [ref] */,
					     uint64_t *_uid /* [out] [ref] */);
NTSTATUS rpccli_wbint_Sid2Uid_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS rpccli_wbint_Sid2Uid(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      const char *dom_name /* [in] [unique,charset(UTF8)] */,
			      struct dom_sid *sid /* [in] [ref] */,
			      uint64_t *uid /* [out] [ref] */);
struct tevent_req *rpccli_wbint_Sid2Gid_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli,
					     const char *_dom_name /* [in] [unique,charset(UTF8)] */,
					     struct dom_sid *_sid /* [in] [ref] */,
					     uint64_t *_gid /* [out] [ref] */);
NTSTATUS rpccli_wbint_Sid2Gid_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS rpccli_wbint_Sid2Gid(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      const char *dom_name /* [in] [unique,charset(UTF8)] */,
			      struct dom_sid *sid /* [in] [ref] */,
			      uint64_t *gid /* [out] [ref] */);
struct tevent_req *rpccli_wbint_Uid2Sid_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli,
					     const char *_dom_name /* [in] [unique,charset(UTF8)] */,
					     uint64_t _uid /* [in]  */,
					     struct dom_sid *_sid /* [out] [ref] */);
NTSTATUS rpccli_wbint_Uid2Sid_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS rpccli_wbint_Uid2Sid(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      const char *dom_name /* [in] [unique,charset(UTF8)] */,
			      uint64_t uid /* [in]  */,
			      struct dom_sid *sid /* [out] [ref] */);
struct tevent_req *rpccli_wbint_Gid2Sid_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli,
					     const char *_dom_name /* [in] [unique,charset(UTF8)] */,
					     uint64_t _gid /* [in]  */,
					     struct dom_sid *_sid /* [out] [ref] */);
NTSTATUS rpccli_wbint_Gid2Sid_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS rpccli_wbint_Gid2Sid(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      const char *dom_name /* [in] [unique,charset(UTF8)] */,
			      uint64_t gid /* [in]  */,
			      struct dom_sid *sid /* [out] [ref] */);
struct tevent_req *rpccli_wbint_AllocateUid_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli,
						 uint64_t *_uid /* [out] [ref] */);
NTSTATUS rpccli_wbint_AllocateUid_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS rpccli_wbint_AllocateUid(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  uint64_t *uid /* [out] [ref] */);
struct tevent_req *rpccli_wbint_AllocateGid_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli,
						 uint64_t *_gid /* [out] [ref] */);
NTSTATUS rpccli_wbint_AllocateGid_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS rpccli_wbint_AllocateGid(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  uint64_t *gid /* [out] [ref] */);
struct tevent_req *rpccli_wbint_QueryUser_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       struct dom_sid *_sid /* [in] [ref] */,
					       struct wbint_userinfo *_info /* [out] [ref] */);
NTSTATUS rpccli_wbint_QueryUser_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS rpccli_wbint_QueryUser(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct dom_sid *sid /* [in] [ref] */,
				struct wbint_userinfo *info /* [out] [ref] */);
struct tevent_req *rpccli_wbint_LookupUserAliases_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct rpc_pipe_client *cli,
						       struct wbint_SidArray *_sids /* [in] [ref] */,
						       struct wbint_RidArray *_rids /* [out] [ref] */);
NTSTATUS rpccli_wbint_LookupUserAliases_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS rpccli_wbint_LookupUserAliases(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					struct wbint_SidArray *sids /* [in] [ref] */,
					struct wbint_RidArray *rids /* [out] [ref] */);
struct tevent_req *rpccli_wbint_LookupUserGroups_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct rpc_pipe_client *cli,
						      struct dom_sid *_sid /* [in] [ref] */,
						      struct wbint_SidArray *_sids /* [out] [ref] */);
NTSTATUS rpccli_wbint_LookupUserGroups_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS rpccli_wbint_LookupUserGroups(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       struct dom_sid *sid /* [in] [ref] */,
				       struct wbint_SidArray *sids /* [out] [ref] */);
struct tevent_req *rpccli_wbint_QuerySequenceNumber_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli,
							 uint32_t *_sequence /* [out] [ref] */);
NTSTATUS rpccli_wbint_QuerySequenceNumber_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       NTSTATUS *result);
NTSTATUS rpccli_wbint_QuerySequenceNumber(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  uint32_t *sequence /* [out] [ref] */);
struct tevent_req *rpccli_wbint_LookupGroupMembers_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							struct dom_sid *_sid /* [in] [ref] */,
							enum lsa_SidType _type /* [in]  */,
							struct wbint_Principals *_members /* [out] [ref] */);
NTSTATUS rpccli_wbint_LookupGroupMembers_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS rpccli_wbint_LookupGroupMembers(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct dom_sid *sid /* [in] [ref] */,
					 enum lsa_SidType type /* [in]  */,
					 struct wbint_Principals *members /* [out] [ref] */);
struct tevent_req *rpccli_wbint_QueryUserList_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct rpc_pipe_client *cli,
						   struct wbint_userinfos *_users /* [out] [ref] */);
NTSTATUS rpccli_wbint_QueryUserList_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS rpccli_wbint_QueryUserList(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    struct wbint_userinfos *users /* [out] [ref] */);
struct tevent_req *rpccli_wbint_QueryGroupList_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct wbint_Principals *_groups /* [out] [ref] */);
NTSTATUS rpccli_wbint_QueryGroupList_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);
NTSTATUS rpccli_wbint_QueryGroupList(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct wbint_Principals *groups /* [out] [ref] */);
struct tevent_req *rpccli_wbint_DsGetDcName_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli,
						 const char *_domain_name /* [in] [ref,charset(UTF8)] */,
						 struct GUID *_domain_guid /* [in] [unique] */,
						 const char *_site_name /* [in] [unique,charset(UTF8)] */,
						 uint32_t _flags /* [in]  */,
						 struct netr_DsRGetDCNameInfo **_dc_info /* [out] [ref] */);
NTSTATUS rpccli_wbint_DsGetDcName_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS rpccli_wbint_DsGetDcName(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  const char *domain_name /* [in] [ref,charset(UTF8)] */,
				  struct GUID *domain_guid /* [in] [unique] */,
				  const char *site_name /* [in] [unique,charset(UTF8)] */,
				  uint32_t flags /* [in]  */,
				  struct netr_DsRGetDCNameInfo **dc_info /* [out] [ref] */);
struct tevent_req *rpccli_wbint_LookupRids_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						struct wbint_RidArray *_rids /* [in] [ref] */,
						const char **_domain_name /* [out] [ref,charset(UTF8)] */,
						struct wbint_Principals *_names /* [out] [ref] */);
NTSTATUS rpccli_wbint_LookupRids_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS rpccli_wbint_LookupRids(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct wbint_RidArray *rids /* [in] [ref] */,
				 const char **domain_name /* [out] [ref,charset(UTF8)] */,
				 struct wbint_Principals *names /* [out] [ref] */);
struct tevent_req *rpccli_wbint_CheckMachineAccount_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli);
NTSTATUS rpccli_wbint_CheckMachineAccount_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       NTSTATUS *result);
NTSTATUS rpccli_wbint_CheckMachineAccount(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_wbint_ChangeMachineAccount_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct rpc_pipe_client *cli);
NTSTATUS rpccli_wbint_ChangeMachineAccount_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS rpccli_wbint_ChangeMachineAccount(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_wbint_PingDc_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct rpc_pipe_client *cli);
NTSTATUS rpccli_wbint_PingDc_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  NTSTATUS *result);
NTSTATUS rpccli_wbint_PingDc(struct rpc_pipe_client *cli,
			     TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_wbint_SetMapping_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						struct dom_sid *_sid /* [in] [ref] */,
						enum wbint_IdType _type /* [in]  */,
						uint64_t _id /* [in]  */);
NTSTATUS rpccli_wbint_SetMapping_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS rpccli_wbint_SetMapping(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct dom_sid *sid /* [in] [ref] */,
				 enum wbint_IdType type /* [in]  */,
				 uint64_t id /* [in]  */);
struct tevent_req *rpccli_wbint_RemoveMapping_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct rpc_pipe_client *cli,
						   struct dom_sid *_sid /* [in] [ref] */,
						   enum wbint_IdType _type /* [in]  */,
						   uint64_t _id /* [in]  */);
NTSTATUS rpccli_wbint_RemoveMapping_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS rpccli_wbint_RemoveMapping(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    struct dom_sid *sid /* [in] [ref] */,
				    enum wbint_IdType type /* [in]  */,
				    uint64_t id /* [in]  */);
struct tevent_req *rpccli_wbint_SetHWM_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct rpc_pipe_client *cli,
					    enum wbint_IdType _type /* [in]  */,
					    uint64_t _id /* [in]  */);
NTSTATUS rpccli_wbint_SetHWM_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  NTSTATUS *result);
NTSTATUS rpccli_wbint_SetHWM(struct rpc_pipe_client *cli,
			     TALLOC_CTX *mem_ctx,
			     enum wbint_IdType type /* [in]  */,
			     uint64_t id /* [in]  */);
#endif /* __CLI_WBINT__ */
