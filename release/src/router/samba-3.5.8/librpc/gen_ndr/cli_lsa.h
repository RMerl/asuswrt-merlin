#include "../librpc/gen_ndr/ndr_lsa.h"
#ifndef __CLI_LSARPC__
#define __CLI_LSARPC__
struct tevent_req *rpccli_lsa_Close_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct rpc_pipe_client *cli,
					 struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS rpccli_lsa_Close_recv(struct tevent_req *req,
			       TALLOC_CTX *mem_ctx,
			       NTSTATUS *result);
NTSTATUS rpccli_lsa_Close(struct rpc_pipe_client *cli,
			  TALLOC_CTX *mem_ctx,
			  struct policy_handle *handle /* [in,out] [ref] */);
struct tevent_req *rpccli_lsa_Delete_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli,
					  struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS rpccli_lsa_Delete_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				NTSTATUS *result);
NTSTATUS rpccli_lsa_Delete(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   struct policy_handle *handle /* [in] [ref] */);
struct tevent_req *rpccli_lsa_EnumPrivs_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli,
					     struct policy_handle *_handle /* [in] [ref] */,
					     uint32_t *_resume_handle /* [in,out] [ref] */,
					     struct lsa_PrivArray *_privs /* [out] [ref] */,
					     uint32_t _max_count /* [in]  */);
NTSTATUS rpccli_lsa_EnumPrivs_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS rpccli_lsa_EnumPrivs(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      struct policy_handle *handle /* [in] [ref] */,
			      uint32_t *resume_handle /* [in,out] [ref] */,
			      struct lsa_PrivArray *privs /* [out] [ref] */,
			      uint32_t max_count /* [in]  */);
struct tevent_req *rpccli_lsa_QuerySecurity_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli,
						 struct policy_handle *_handle /* [in] [ref] */,
						 uint32_t _sec_info /* [in]  */,
						 struct sec_desc_buf **_sdbuf /* [out] [ref] */);
NTSTATUS rpccli_lsa_QuerySecurity_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS rpccli_lsa_QuerySecurity(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *handle /* [in] [ref] */,
				  uint32_t sec_info /* [in]  */,
				  struct sec_desc_buf **sdbuf /* [out] [ref] */);
struct tevent_req *rpccli_lsa_SetSecObj_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli,
					     struct policy_handle *_handle /* [in] [ref] */,
					     uint32_t _sec_info /* [in]  */,
					     struct sec_desc_buf *_sdbuf /* [in] [ref] */);
NTSTATUS rpccli_lsa_SetSecObj_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS rpccli_lsa_SetSecObj(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      struct policy_handle *handle /* [in] [ref] */,
			      uint32_t sec_info /* [in]  */,
			      struct sec_desc_buf *sdbuf /* [in] [ref] */);
struct tevent_req *rpccli_lsa_ChangePassword_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_ChangePassword_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					NTSTATUS *result);
NTSTATUS rpccli_lsa_ChangePassword(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_OpenPolicy_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli,
					      uint16_t *_system_name /* [in] [unique] */,
					      struct lsa_ObjectAttribute *_attr /* [in] [ref] */,
					      uint32_t _access_mask /* [in]  */,
					      struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS rpccli_lsa_OpenPolicy_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    NTSTATUS *result);
NTSTATUS rpccli_lsa_OpenPolicy(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       uint16_t *system_name /* [in] [unique] */,
			       struct lsa_ObjectAttribute *attr /* [in] [ref] */,
			       uint32_t access_mask /* [in]  */,
			       struct policy_handle *handle /* [out] [ref] */);
struct tevent_req *rpccli_lsa_QueryInfoPolicy_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct rpc_pipe_client *cli,
						   struct policy_handle *_handle /* [in] [ref] */,
						   enum lsa_PolicyInfo _level /* [in]  */,
						   union lsa_PolicyInformation **_info /* [out] [ref,switch_is(level)] */);
NTSTATUS rpccli_lsa_QueryInfoPolicy_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS rpccli_lsa_QueryInfoPolicy(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *handle /* [in] [ref] */,
				    enum lsa_PolicyInfo level /* [in]  */,
				    union lsa_PolicyInformation **info /* [out] [ref,switch_is(level)] */);
struct tevent_req *rpccli_lsa_SetInfoPolicy_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli,
						 struct policy_handle *_handle /* [in] [ref] */,
						 enum lsa_PolicyInfo _level /* [in]  */,
						 union lsa_PolicyInformation *_info /* [in] [ref,switch_is(level)] */);
NTSTATUS rpccli_lsa_SetInfoPolicy_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS rpccli_lsa_SetInfoPolicy(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *handle /* [in] [ref] */,
				  enum lsa_PolicyInfo level /* [in]  */,
				  union lsa_PolicyInformation *info /* [in] [ref,switch_is(level)] */);
struct tevent_req *rpccli_lsa_ClearAuditLog_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_ClearAuditLog_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS rpccli_lsa_ClearAuditLog(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_CreateAccount_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli,
						 struct policy_handle *_handle /* [in] [ref] */,
						 struct dom_sid2 *_sid /* [in] [ref] */,
						 uint32_t _access_mask /* [in]  */,
						 struct policy_handle *_acct_handle /* [out] [ref] */);
NTSTATUS rpccli_lsa_CreateAccount_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS rpccli_lsa_CreateAccount(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *handle /* [in] [ref] */,
				  struct dom_sid2 *sid /* [in] [ref] */,
				  uint32_t access_mask /* [in]  */,
				  struct policy_handle *acct_handle /* [out] [ref] */);
struct tevent_req *rpccli_lsa_EnumAccounts_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						struct policy_handle *_handle /* [in] [ref] */,
						uint32_t *_resume_handle /* [in,out] [ref] */,
						struct lsa_SidArray *_sids /* [out] [ref] */,
						uint32_t _num_entries /* [in] [range(0,8192)] */);
NTSTATUS rpccli_lsa_EnumAccounts_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS rpccli_lsa_EnumAccounts(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *handle /* [in] [ref] */,
				 uint32_t *resume_handle /* [in,out] [ref] */,
				 struct lsa_SidArray *sids /* [out] [ref] */,
				 uint32_t num_entries /* [in] [range(0,8192)] */);
struct tevent_req *rpccli_lsa_CreateTrustedDomain_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct rpc_pipe_client *cli,
						       struct policy_handle *_policy_handle /* [in] [ref] */,
						       struct lsa_DomainInfo *_info /* [in] [ref] */,
						       uint32_t _access_mask /* [in]  */,
						       struct policy_handle *_trustdom_handle /* [out] [ref] */);
NTSTATUS rpccli_lsa_CreateTrustedDomain_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS rpccli_lsa_CreateTrustedDomain(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *policy_handle /* [in] [ref] */,
					struct lsa_DomainInfo *info /* [in] [ref] */,
					uint32_t access_mask /* [in]  */,
					struct policy_handle *trustdom_handle /* [out] [ref] */);
struct tevent_req *rpccli_lsa_EnumTrustDom_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						struct policy_handle *_handle /* [in] [ref] */,
						uint32_t *_resume_handle /* [in,out] [ref] */,
						struct lsa_DomainList *_domains /* [out] [ref] */,
						uint32_t _max_size /* [in]  */);
NTSTATUS rpccli_lsa_EnumTrustDom_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS rpccli_lsa_EnumTrustDom(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *handle /* [in] [ref] */,
				 uint32_t *resume_handle /* [in,out] [ref] */,
				 struct lsa_DomainList *domains /* [out] [ref] */,
				 uint32_t max_size /* [in]  */);
struct tevent_req *rpccli_lsa_LookupNames_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       struct policy_handle *_handle /* [in] [ref] */,
					       uint32_t _num_names /* [in] [range(0,1000)] */,
					       struct lsa_String *_names /* [in] [size_is(num_names)] */,
					       struct lsa_RefDomainList **_domains /* [out] [ref] */,
					       struct lsa_TransSidArray *_sids /* [in,out] [ref] */,
					       enum lsa_LookupNamesLevel _level /* [in]  */,
					       uint32_t *_count /* [in,out] [ref] */);
NTSTATUS rpccli_lsa_LookupNames_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS rpccli_lsa_LookupNames(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *handle /* [in] [ref] */,
				uint32_t num_names /* [in] [range(0,1000)] */,
				struct lsa_String *names /* [in] [size_is(num_names)] */,
				struct lsa_RefDomainList **domains /* [out] [ref] */,
				struct lsa_TransSidArray *sids /* [in,out] [ref] */,
				enum lsa_LookupNamesLevel level /* [in]  */,
				uint32_t *count /* [in,out] [ref] */);
struct tevent_req *rpccli_lsa_LookupSids_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli,
					      struct policy_handle *_handle /* [in] [ref] */,
					      struct lsa_SidArray *_sids /* [in] [ref] */,
					      struct lsa_RefDomainList **_domains /* [out] [ref] */,
					      struct lsa_TransNameArray *_names /* [in,out] [ref] */,
					      enum lsa_LookupNamesLevel _level /* [in]  */,
					      uint32_t *_count /* [in,out] [ref] */);
NTSTATUS rpccli_lsa_LookupSids_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    NTSTATUS *result);
NTSTATUS rpccli_lsa_LookupSids(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *handle /* [in] [ref] */,
			       struct lsa_SidArray *sids /* [in] [ref] */,
			       struct lsa_RefDomainList **domains /* [out] [ref] */,
			       struct lsa_TransNameArray *names /* [in,out] [ref] */,
			       enum lsa_LookupNamesLevel level /* [in]  */,
			       uint32_t *count /* [in,out] [ref] */);
struct tevent_req *rpccli_lsa_CreateSecret_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						struct policy_handle *_handle /* [in] [ref] */,
						struct lsa_String _name /* [in]  */,
						uint32_t _access_mask /* [in]  */,
						struct policy_handle *_sec_handle /* [out] [ref] */);
NTSTATUS rpccli_lsa_CreateSecret_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS rpccli_lsa_CreateSecret(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *handle /* [in] [ref] */,
				 struct lsa_String name /* [in]  */,
				 uint32_t access_mask /* [in]  */,
				 struct policy_handle *sec_handle /* [out] [ref] */);
struct tevent_req *rpccli_lsa_OpenAccount_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       struct policy_handle *_handle /* [in] [ref] */,
					       struct dom_sid2 *_sid /* [in] [ref] */,
					       uint32_t _access_mask /* [in]  */,
					       struct policy_handle *_acct_handle /* [out] [ref] */);
NTSTATUS rpccli_lsa_OpenAccount_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS rpccli_lsa_OpenAccount(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *handle /* [in] [ref] */,
				struct dom_sid2 *sid /* [in] [ref] */,
				uint32_t access_mask /* [in]  */,
				struct policy_handle *acct_handle /* [out] [ref] */);
struct tevent_req *rpccli_lsa_EnumPrivsAccount_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct policy_handle *_handle /* [in] [ref] */,
						    struct lsa_PrivilegeSet **_privs /* [out] [ref] */);
NTSTATUS rpccli_lsa_EnumPrivsAccount_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);
NTSTATUS rpccli_lsa_EnumPrivsAccount(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle /* [in] [ref] */,
				     struct lsa_PrivilegeSet **privs /* [out] [ref] */);
struct tevent_req *rpccli_lsa_AddPrivilegesToAccount_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct rpc_pipe_client *cli,
							  struct policy_handle *_handle /* [in] [ref] */,
							  struct lsa_PrivilegeSet *_privs /* [in] [ref] */);
NTSTATUS rpccli_lsa_AddPrivilegesToAccount_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS rpccli_lsa_AddPrivilegesToAccount(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *handle /* [in] [ref] */,
					   struct lsa_PrivilegeSet *privs /* [in] [ref] */);
struct tevent_req *rpccli_lsa_RemovePrivilegesFromAccount_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct rpc_pipe_client *cli,
							       struct policy_handle *_handle /* [in] [ref] */,
							       uint8_t _remove_all /* [in]  */,
							       struct lsa_PrivilegeSet *_privs /* [in] [unique] */);
NTSTATUS rpccli_lsa_RemovePrivilegesFromAccount_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     NTSTATUS *result);
NTSTATUS rpccli_lsa_RemovePrivilegesFromAccount(struct rpc_pipe_client *cli,
						TALLOC_CTX *mem_ctx,
						struct policy_handle *handle /* [in] [ref] */,
						uint8_t remove_all /* [in]  */,
						struct lsa_PrivilegeSet *privs /* [in] [unique] */);
struct tevent_req *rpccli_lsa_GetQuotasForAccount_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_GetQuotasForAccount_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS rpccli_lsa_GetQuotasForAccount(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_SetQuotasForAccount_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_SetQuotasForAccount_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS rpccli_lsa_SetQuotasForAccount(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_GetSystemAccessAccount_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct rpc_pipe_client *cli,
							  struct policy_handle *_handle /* [in] [ref] */,
							  uint32_t *_access_mask /* [out] [ref] */);
NTSTATUS rpccli_lsa_GetSystemAccessAccount_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS rpccli_lsa_GetSystemAccessAccount(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *handle /* [in] [ref] */,
					   uint32_t *access_mask /* [out] [ref] */);
struct tevent_req *rpccli_lsa_SetSystemAccessAccount_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct rpc_pipe_client *cli,
							  struct policy_handle *_handle /* [in] [ref] */,
							  uint32_t _access_mask /* [in]  */);
NTSTATUS rpccli_lsa_SetSystemAccessAccount_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS rpccli_lsa_SetSystemAccessAccount(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *handle /* [in] [ref] */,
					   uint32_t access_mask /* [in]  */);
struct tevent_req *rpccli_lsa_OpenTrustedDomain_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli,
						     struct policy_handle *_handle /* [in] [ref] */,
						     struct dom_sid2 *_sid /* [in] [ref] */,
						     uint32_t _access_mask /* [in]  */,
						     struct policy_handle *_trustdom_handle /* [out] [ref] */);
NTSTATUS rpccli_lsa_OpenTrustedDomain_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   NTSTATUS *result);
NTSTATUS rpccli_lsa_OpenTrustedDomain(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *handle /* [in] [ref] */,
				      struct dom_sid2 *sid /* [in] [ref] */,
				      uint32_t access_mask /* [in]  */,
				      struct policy_handle *trustdom_handle /* [out] [ref] */);
struct tevent_req *rpccli_lsa_QueryTrustedDomainInfo_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct rpc_pipe_client *cli,
							  struct policy_handle *_trustdom_handle /* [in] [ref] */,
							  enum lsa_TrustDomInfoEnum _level /* [in]  */,
							  union lsa_TrustedDomainInfo **_info /* [out] [ref,switch_is(level)] */);
NTSTATUS rpccli_lsa_QueryTrustedDomainInfo_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS rpccli_lsa_QueryTrustedDomainInfo(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *trustdom_handle /* [in] [ref] */,
					   enum lsa_TrustDomInfoEnum level /* [in]  */,
					   union lsa_TrustedDomainInfo **info /* [out] [ref,switch_is(level)] */);
struct tevent_req *rpccli_lsa_SetInformationTrustedDomain_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct rpc_pipe_client *cli,
							       struct policy_handle *_trustdom_handle /* [in] [ref] */,
							       enum lsa_TrustDomInfoEnum _level /* [in]  */,
							       union lsa_TrustedDomainInfo *_info /* [in] [ref,switch_is(level)] */);
NTSTATUS rpccli_lsa_SetInformationTrustedDomain_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     NTSTATUS *result);
NTSTATUS rpccli_lsa_SetInformationTrustedDomain(struct rpc_pipe_client *cli,
						TALLOC_CTX *mem_ctx,
						struct policy_handle *trustdom_handle /* [in] [ref] */,
						enum lsa_TrustDomInfoEnum level /* [in]  */,
						union lsa_TrustedDomainInfo *info /* [in] [ref,switch_is(level)] */);
struct tevent_req *rpccli_lsa_OpenSecret_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli,
					      struct policy_handle *_handle /* [in] [ref] */,
					      struct lsa_String _name /* [in]  */,
					      uint32_t _access_mask /* [in]  */,
					      struct policy_handle *_sec_handle /* [out] [ref] */);
NTSTATUS rpccli_lsa_OpenSecret_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    NTSTATUS *result);
NTSTATUS rpccli_lsa_OpenSecret(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *handle /* [in] [ref] */,
			       struct lsa_String name /* [in]  */,
			       uint32_t access_mask /* [in]  */,
			       struct policy_handle *sec_handle /* [out] [ref] */);
struct tevent_req *rpccli_lsa_SetSecret_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli,
					     struct policy_handle *_sec_handle /* [in] [ref] */,
					     struct lsa_DATA_BUF *_new_val /* [in] [unique] */,
					     struct lsa_DATA_BUF *_old_val /* [in] [unique] */);
NTSTATUS rpccli_lsa_SetSecret_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS rpccli_lsa_SetSecret(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      struct policy_handle *sec_handle /* [in] [ref] */,
			      struct lsa_DATA_BUF *new_val /* [in] [unique] */,
			      struct lsa_DATA_BUF *old_val /* [in] [unique] */);
struct tevent_req *rpccli_lsa_QuerySecret_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       struct policy_handle *_sec_handle /* [in] [ref] */,
					       struct lsa_DATA_BUF_PTR *_new_val /* [in,out] [unique] */,
					       NTTIME *_new_mtime /* [in,out] [unique] */,
					       struct lsa_DATA_BUF_PTR *_old_val /* [in,out] [unique] */,
					       NTTIME *_old_mtime /* [in,out] [unique] */);
NTSTATUS rpccli_lsa_QuerySecret_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS rpccli_lsa_QuerySecret(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *sec_handle /* [in] [ref] */,
				struct lsa_DATA_BUF_PTR *new_val /* [in,out] [unique] */,
				NTTIME *new_mtime /* [in,out] [unique] */,
				struct lsa_DATA_BUF_PTR *old_val /* [in,out] [unique] */,
				NTTIME *old_mtime /* [in,out] [unique] */);
struct tevent_req *rpccli_lsa_LookupPrivValue_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct rpc_pipe_client *cli,
						   struct policy_handle *_handle /* [in] [ref] */,
						   struct lsa_String *_name /* [in] [ref] */,
						   struct lsa_LUID *_luid /* [out] [ref] */);
NTSTATUS rpccli_lsa_LookupPrivValue_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS rpccli_lsa_LookupPrivValue(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *handle /* [in] [ref] */,
				    struct lsa_String *name /* [in] [ref] */,
				    struct lsa_LUID *luid /* [out] [ref] */);
struct tevent_req *rpccli_lsa_LookupPrivName_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli,
						  struct policy_handle *_handle /* [in] [ref] */,
						  struct lsa_LUID *_luid /* [in] [ref] */,
						  struct lsa_StringLarge **_name /* [out] [ref] */);
NTSTATUS rpccli_lsa_LookupPrivName_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					NTSTATUS *result);
NTSTATUS rpccli_lsa_LookupPrivName(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *handle /* [in] [ref] */,
				   struct lsa_LUID *luid /* [in] [ref] */,
				   struct lsa_StringLarge **name /* [out] [ref] */);
struct tevent_req *rpccli_lsa_LookupPrivDisplayName_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli,
							 struct policy_handle *_handle /* [in] [ref] */,
							 struct lsa_String *_name /* [in] [ref] */,
							 uint16_t _language_id /* [in]  */,
							 uint16_t _language_id_sys /* [in]  */,
							 struct lsa_StringLarge **_disp_name /* [out] [ref] */,
							 uint16_t *_returned_language_id /* [out] [ref] */);
NTSTATUS rpccli_lsa_LookupPrivDisplayName_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       NTSTATUS *result);
NTSTATUS rpccli_lsa_LookupPrivDisplayName(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *handle /* [in] [ref] */,
					  struct lsa_String *name /* [in] [ref] */,
					  uint16_t language_id /* [in]  */,
					  uint16_t language_id_sys /* [in]  */,
					  struct lsa_StringLarge **disp_name /* [out] [ref] */,
					  uint16_t *returned_language_id /* [out] [ref] */);
struct tevent_req *rpccli_lsa_DeleteObject_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS rpccli_lsa_DeleteObject_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS rpccli_lsa_DeleteObject(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *handle /* [in,out] [ref] */);
struct tevent_req *rpccli_lsa_EnumAccountsWithUserRight_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct rpc_pipe_client *cli,
							     struct policy_handle *_handle /* [in] [ref] */,
							     struct lsa_String *_name /* [in] [unique] */,
							     struct lsa_SidArray *_sids /* [out] [ref] */);
NTSTATUS rpccli_lsa_EnumAccountsWithUserRight_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   NTSTATUS *result);
NTSTATUS rpccli_lsa_EnumAccountsWithUserRight(struct rpc_pipe_client *cli,
					      TALLOC_CTX *mem_ctx,
					      struct policy_handle *handle /* [in] [ref] */,
					      struct lsa_String *name /* [in] [unique] */,
					      struct lsa_SidArray *sids /* [out] [ref] */);
struct tevent_req *rpccli_lsa_EnumAccountRights_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli,
						     struct policy_handle *_handle /* [in] [ref] */,
						     struct dom_sid2 *_sid /* [in] [ref] */,
						     struct lsa_RightSet *_rights /* [out] [ref] */);
NTSTATUS rpccli_lsa_EnumAccountRights_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   NTSTATUS *result);
NTSTATUS rpccli_lsa_EnumAccountRights(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *handle /* [in] [ref] */,
				      struct dom_sid2 *sid /* [in] [ref] */,
				      struct lsa_RightSet *rights /* [out] [ref] */);
struct tevent_req *rpccli_lsa_AddAccountRights_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct policy_handle *_handle /* [in] [ref] */,
						    struct dom_sid2 *_sid /* [in] [ref] */,
						    struct lsa_RightSet *_rights /* [in] [ref] */);
NTSTATUS rpccli_lsa_AddAccountRights_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);
NTSTATUS rpccli_lsa_AddAccountRights(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle /* [in] [ref] */,
				     struct dom_sid2 *sid /* [in] [ref] */,
				     struct lsa_RightSet *rights /* [in] [ref] */);
struct tevent_req *rpccli_lsa_RemoveAccountRights_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct rpc_pipe_client *cli,
						       struct policy_handle *_handle /* [in] [ref] */,
						       struct dom_sid2 *_sid /* [in] [ref] */,
						       uint8_t _remove_all /* [in]  */,
						       struct lsa_RightSet *_rights /* [in] [ref] */);
NTSTATUS rpccli_lsa_RemoveAccountRights_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS rpccli_lsa_RemoveAccountRights(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *handle /* [in] [ref] */,
					struct dom_sid2 *sid /* [in] [ref] */,
					uint8_t remove_all /* [in]  */,
					struct lsa_RightSet *rights /* [in] [ref] */);
struct tevent_req *rpccli_lsa_QueryTrustedDomainInfoBySid_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct rpc_pipe_client *cli,
							       struct policy_handle *_handle /* [in] [ref] */,
							       struct dom_sid2 *_dom_sid /* [in] [ref] */,
							       enum lsa_TrustDomInfoEnum _level /* [in]  */,
							       union lsa_TrustedDomainInfo **_info /* [out] [ref,switch_is(level)] */);
NTSTATUS rpccli_lsa_QueryTrustedDomainInfoBySid_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     NTSTATUS *result);
NTSTATUS rpccli_lsa_QueryTrustedDomainInfoBySid(struct rpc_pipe_client *cli,
						TALLOC_CTX *mem_ctx,
						struct policy_handle *handle /* [in] [ref] */,
						struct dom_sid2 *dom_sid /* [in] [ref] */,
						enum lsa_TrustDomInfoEnum level /* [in]  */,
						union lsa_TrustedDomainInfo **info /* [out] [ref,switch_is(level)] */);
struct tevent_req *rpccli_lsa_SetTrustedDomainInfo_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							struct policy_handle *_handle /* [in] [ref] */,
							struct dom_sid2 *_dom_sid /* [in] [ref] */,
							enum lsa_TrustDomInfoEnum _level /* [in]  */,
							union lsa_TrustedDomainInfo *_info /* [in] [ref,switch_is(level)] */);
NTSTATUS rpccli_lsa_SetTrustedDomainInfo_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS rpccli_lsa_SetTrustedDomainInfo(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *handle /* [in] [ref] */,
					 struct dom_sid2 *dom_sid /* [in] [ref] */,
					 enum lsa_TrustDomInfoEnum level /* [in]  */,
					 union lsa_TrustedDomainInfo *info /* [in] [ref,switch_is(level)] */);
struct tevent_req *rpccli_lsa_DeleteTrustedDomain_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct rpc_pipe_client *cli,
						       struct policy_handle *_handle /* [in] [ref] */,
						       struct dom_sid2 *_dom_sid /* [in] [ref] */);
NTSTATUS rpccli_lsa_DeleteTrustedDomain_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS rpccli_lsa_DeleteTrustedDomain(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *handle /* [in] [ref] */,
					struct dom_sid2 *dom_sid /* [in] [ref] */);
struct tevent_req *rpccli_lsa_StorePrivateData_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct policy_handle *_handle /* [in] [ref] */,
						    struct lsa_String *_name /* [in] [ref] */,
						    struct lsa_DATA_BUF *_val /* [in] [unique] */);
NTSTATUS rpccli_lsa_StorePrivateData_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);
NTSTATUS rpccli_lsa_StorePrivateData(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle /* [in] [ref] */,
				     struct lsa_String *name /* [in] [ref] */,
				     struct lsa_DATA_BUF *val /* [in] [unique] */);
struct tevent_req *rpccli_lsa_RetrievePrivateData_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct rpc_pipe_client *cli,
						       struct policy_handle *_handle /* [in] [ref] */,
						       struct lsa_String *_name /* [in] [ref] */,
						       struct lsa_DATA_BUF **_val /* [in,out] [ref] */);
NTSTATUS rpccli_lsa_RetrievePrivateData_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS rpccli_lsa_RetrievePrivateData(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *handle /* [in] [ref] */,
					struct lsa_String *name /* [in] [ref] */,
					struct lsa_DATA_BUF **val /* [in,out] [ref] */);
struct tevent_req *rpccli_lsa_OpenPolicy2_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       const char *_system_name /* [in] [unique,charset(UTF16)] */,
					       struct lsa_ObjectAttribute *_attr /* [in] [ref] */,
					       uint32_t _access_mask /* [in]  */,
					       struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS rpccli_lsa_OpenPolicy2_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS rpccli_lsa_OpenPolicy2(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				const char *system_name /* [in] [unique,charset(UTF16)] */,
				struct lsa_ObjectAttribute *attr /* [in] [ref] */,
				uint32_t access_mask /* [in]  */,
				struct policy_handle *handle /* [out] [ref] */);
struct tevent_req *rpccli_lsa_GetUserName_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       const char *_system_name /* [in] [unique,charset(UTF16)] */,
					       struct lsa_String **_account_name /* [in,out] [ref] */,
					       struct lsa_String **_authority_name /* [in,out] [unique] */);
NTSTATUS rpccli_lsa_GetUserName_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS rpccli_lsa_GetUserName(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				const char *system_name /* [in] [unique,charset(UTF16)] */,
				struct lsa_String **account_name /* [in,out] [ref] */,
				struct lsa_String **authority_name /* [in,out] [unique] */);
struct tevent_req *rpccli_lsa_QueryInfoPolicy2_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct policy_handle *_handle /* [in] [ref] */,
						    enum lsa_PolicyInfo _level /* [in]  */,
						    union lsa_PolicyInformation **_info /* [out] [ref,switch_is(level)] */);
NTSTATUS rpccli_lsa_QueryInfoPolicy2_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);
NTSTATUS rpccli_lsa_QueryInfoPolicy2(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle /* [in] [ref] */,
				     enum lsa_PolicyInfo level /* [in]  */,
				     union lsa_PolicyInformation **info /* [out] [ref,switch_is(level)] */);
struct tevent_req *rpccli_lsa_SetInfoPolicy2_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli,
						  struct policy_handle *_handle /* [in] [ref] */,
						  enum lsa_PolicyInfo _level /* [in]  */,
						  union lsa_PolicyInformation *_info /* [in] [ref,switch_is(level)] */);
NTSTATUS rpccli_lsa_SetInfoPolicy2_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					NTSTATUS *result);
NTSTATUS rpccli_lsa_SetInfoPolicy2(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *handle /* [in] [ref] */,
				   enum lsa_PolicyInfo level /* [in]  */,
				   union lsa_PolicyInformation *info /* [in] [ref,switch_is(level)] */);
struct tevent_req *rpccli_lsa_QueryTrustedDomainInfoByName_send(TALLOC_CTX *mem_ctx,
								struct tevent_context *ev,
								struct rpc_pipe_client *cli,
								struct policy_handle *_handle /* [in] [ref] */,
								struct lsa_String *_trusted_domain /* [in] [ref] */,
								enum lsa_TrustDomInfoEnum _level /* [in]  */,
								union lsa_TrustedDomainInfo **_info /* [out] [ref,switch_is(level)] */);
NTSTATUS rpccli_lsa_QueryTrustedDomainInfoByName_recv(struct tevent_req *req,
						      TALLOC_CTX *mem_ctx,
						      NTSTATUS *result);
NTSTATUS rpccli_lsa_QueryTrustedDomainInfoByName(struct rpc_pipe_client *cli,
						 TALLOC_CTX *mem_ctx,
						 struct policy_handle *handle /* [in] [ref] */,
						 struct lsa_String *trusted_domain /* [in] [ref] */,
						 enum lsa_TrustDomInfoEnum level /* [in]  */,
						 union lsa_TrustedDomainInfo **info /* [out] [ref,switch_is(level)] */);
struct tevent_req *rpccli_lsa_SetTrustedDomainInfoByName_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct rpc_pipe_client *cli,
							      struct policy_handle *_handle /* [in] [ref] */,
							      struct lsa_String _trusted_domain /* [in]  */,
							      enum lsa_TrustDomInfoEnum _level /* [in]  */,
							      union lsa_TrustedDomainInfo *_info /* [in] [unique,switch_is(level)] */);
NTSTATUS rpccli_lsa_SetTrustedDomainInfoByName_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    NTSTATUS *result);
NTSTATUS rpccli_lsa_SetTrustedDomainInfoByName(struct rpc_pipe_client *cli,
					       TALLOC_CTX *mem_ctx,
					       struct policy_handle *handle /* [in] [ref] */,
					       struct lsa_String trusted_domain /* [in]  */,
					       enum lsa_TrustDomInfoEnum level /* [in]  */,
					       union lsa_TrustedDomainInfo *info /* [in] [unique,switch_is(level)] */);
struct tevent_req *rpccli_lsa_EnumTrustedDomainsEx_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							struct policy_handle *_handle /* [in] [ref] */,
							uint32_t *_resume_handle /* [in,out] [ref] */,
							struct lsa_DomainListEx *_domains /* [out] [ref] */,
							uint32_t _max_size /* [in]  */);
NTSTATUS rpccli_lsa_EnumTrustedDomainsEx_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS rpccli_lsa_EnumTrustedDomainsEx(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *handle /* [in] [ref] */,
					 uint32_t *resume_handle /* [in,out] [ref] */,
					 struct lsa_DomainListEx *domains /* [out] [ref] */,
					 uint32_t max_size /* [in]  */);
struct tevent_req *rpccli_lsa_CreateTrustedDomainEx_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli,
							 struct policy_handle *_policy_handle /* [in] [ref] */,
							 struct lsa_TrustDomainInfoInfoEx *_info /* [in] [ref] */,
							 struct lsa_TrustDomainInfoAuthInfoInternal *_auth_info /* [in] [ref] */,
							 uint32_t _access_mask /* [in]  */,
							 struct policy_handle *_trustdom_handle /* [out] [ref] */);
NTSTATUS rpccli_lsa_CreateTrustedDomainEx_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       NTSTATUS *result);
NTSTATUS rpccli_lsa_CreateTrustedDomainEx(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *policy_handle /* [in] [ref] */,
					  struct lsa_TrustDomainInfoInfoEx *info /* [in] [ref] */,
					  struct lsa_TrustDomainInfoAuthInfoInternal *auth_info /* [in] [ref] */,
					  uint32_t access_mask /* [in]  */,
					  struct policy_handle *trustdom_handle /* [out] [ref] */);
struct tevent_req *rpccli_lsa_CloseTrustedDomainEx_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS rpccli_lsa_CloseTrustedDomainEx_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS rpccli_lsa_CloseTrustedDomainEx(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *handle /* [in,out] [ref] */);
struct tevent_req *rpccli_lsa_QueryDomainInformationPolicy_send(TALLOC_CTX *mem_ctx,
								struct tevent_context *ev,
								struct rpc_pipe_client *cli,
								struct policy_handle *_handle /* [in] [ref] */,
								uint16_t _level /* [in]  */,
								union lsa_DomainInformationPolicy **_info /* [out] [ref,switch_is(level)] */);
NTSTATUS rpccli_lsa_QueryDomainInformationPolicy_recv(struct tevent_req *req,
						      TALLOC_CTX *mem_ctx,
						      NTSTATUS *result);
NTSTATUS rpccli_lsa_QueryDomainInformationPolicy(struct rpc_pipe_client *cli,
						 TALLOC_CTX *mem_ctx,
						 struct policy_handle *handle /* [in] [ref] */,
						 uint16_t level /* [in]  */,
						 union lsa_DomainInformationPolicy **info /* [out] [ref,switch_is(level)] */);
struct tevent_req *rpccli_lsa_SetDomainInformationPolicy_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct rpc_pipe_client *cli,
							      struct policy_handle *_handle /* [in] [ref] */,
							      uint16_t _level /* [in]  */,
							      union lsa_DomainInformationPolicy *_info /* [in] [unique,switch_is(level)] */);
NTSTATUS rpccli_lsa_SetDomainInformationPolicy_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    NTSTATUS *result);
NTSTATUS rpccli_lsa_SetDomainInformationPolicy(struct rpc_pipe_client *cli,
					       TALLOC_CTX *mem_ctx,
					       struct policy_handle *handle /* [in] [ref] */,
					       uint16_t level /* [in]  */,
					       union lsa_DomainInformationPolicy *info /* [in] [unique,switch_is(level)] */);
struct tevent_req *rpccli_lsa_OpenTrustedDomainByName_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct rpc_pipe_client *cli,
							   struct policy_handle *_handle /* [in] [ref] */,
							   struct lsa_String _name /* [in]  */,
							   uint32_t _access_mask /* [in]  */,
							   struct policy_handle *_trustdom_handle /* [out] [ref] */);
NTSTATUS rpccli_lsa_OpenTrustedDomainByName_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 NTSTATUS *result);
NTSTATUS rpccli_lsa_OpenTrustedDomainByName(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    struct policy_handle *handle /* [in] [ref] */,
					    struct lsa_String name /* [in]  */,
					    uint32_t access_mask /* [in]  */,
					    struct policy_handle *trustdom_handle /* [out] [ref] */);
struct tevent_req *rpccli_lsa_TestCall_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_TestCall_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  NTSTATUS *result);
NTSTATUS rpccli_lsa_TestCall(struct rpc_pipe_client *cli,
			     TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_LookupSids2_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       struct policy_handle *_handle /* [in] [ref] */,
					       struct lsa_SidArray *_sids /* [in] [ref] */,
					       struct lsa_RefDomainList **_domains /* [out] [ref] */,
					       struct lsa_TransNameArray2 *_names /* [in,out] [ref] */,
					       enum lsa_LookupNamesLevel _level /* [in]  */,
					       uint32_t *_count /* [in,out] [ref] */,
					       enum lsa_LookupOptions _lookup_options /* [in]  */,
					       enum lsa_ClientRevision _client_revision /* [in]  */);
NTSTATUS rpccli_lsa_LookupSids2_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS rpccli_lsa_LookupSids2(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *handle /* [in] [ref] */,
				struct lsa_SidArray *sids /* [in] [ref] */,
				struct lsa_RefDomainList **domains /* [out] [ref] */,
				struct lsa_TransNameArray2 *names /* [in,out] [ref] */,
				enum lsa_LookupNamesLevel level /* [in]  */,
				uint32_t *count /* [in,out] [ref] */,
				enum lsa_LookupOptions lookup_options /* [in]  */,
				enum lsa_ClientRevision client_revision /* [in]  */);
struct tevent_req *rpccli_lsa_LookupNames2_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						struct policy_handle *_handle /* [in] [ref] */,
						uint32_t _num_names /* [in] [range(0,1000)] */,
						struct lsa_String *_names /* [in] [size_is(num_names)] */,
						struct lsa_RefDomainList **_domains /* [out] [ref] */,
						struct lsa_TransSidArray2 *_sids /* [in,out] [ref] */,
						enum lsa_LookupNamesLevel _level /* [in]  */,
						uint32_t *_count /* [in,out] [ref] */,
						enum lsa_LookupOptions _lookup_options /* [in]  */,
						enum lsa_ClientRevision _client_revision /* [in]  */);
NTSTATUS rpccli_lsa_LookupNames2_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS rpccli_lsa_LookupNames2(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *handle /* [in] [ref] */,
				 uint32_t num_names /* [in] [range(0,1000)] */,
				 struct lsa_String *names /* [in] [size_is(num_names)] */,
				 struct lsa_RefDomainList **domains /* [out] [ref] */,
				 struct lsa_TransSidArray2 *sids /* [in,out] [ref] */,
				 enum lsa_LookupNamesLevel level /* [in]  */,
				 uint32_t *count /* [in,out] [ref] */,
				 enum lsa_LookupOptions lookup_options /* [in]  */,
				 enum lsa_ClientRevision client_revision /* [in]  */);
struct tevent_req *rpccli_lsa_CreateTrustedDomainEx2_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct rpc_pipe_client *cli,
							  struct policy_handle *_policy_handle /* [in] [ref] */,
							  struct lsa_TrustDomainInfoInfoEx *_info /* [in] [ref] */,
							  struct lsa_TrustDomainInfoAuthInfoInternal *_auth_info /* [in] [ref] */,
							  uint32_t _access_mask /* [in]  */,
							  struct policy_handle *_trustdom_handle /* [out] [ref] */);
NTSTATUS rpccli_lsa_CreateTrustedDomainEx2_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS rpccli_lsa_CreateTrustedDomainEx2(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *policy_handle /* [in] [ref] */,
					   struct lsa_TrustDomainInfoInfoEx *info /* [in] [ref] */,
					   struct lsa_TrustDomainInfoAuthInfoInternal *auth_info /* [in] [ref] */,
					   uint32_t access_mask /* [in]  */,
					   struct policy_handle *trustdom_handle /* [out] [ref] */);
struct tevent_req *rpccli_lsa_CREDRWRITE_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_CREDRWRITE_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    NTSTATUS *result);
NTSTATUS rpccli_lsa_CREDRWRITE(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_CREDRREAD_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_CREDRREAD_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS rpccli_lsa_CREDRREAD(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_CREDRENUMERATE_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_CREDRENUMERATE_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					NTSTATUS *result);
NTSTATUS rpccli_lsa_CREDRENUMERATE(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_CREDRWRITEDOMAINCREDENTIALS_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_CREDRWRITEDOMAINCREDENTIALS_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     NTSTATUS *result);
NTSTATUS rpccli_lsa_CREDRWRITEDOMAINCREDENTIALS(struct rpc_pipe_client *cli,
						TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_CREDRREADDOMAINCREDENTIALS_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_CREDRREADDOMAINCREDENTIALS_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    NTSTATUS *result);
NTSTATUS rpccli_lsa_CREDRREADDOMAINCREDENTIALS(struct rpc_pipe_client *cli,
					       TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_CREDRDELETE_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_CREDRDELETE_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS rpccli_lsa_CREDRDELETE(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_CREDRGETTARGETINFO_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_CREDRGETTARGETINFO_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS rpccli_lsa_CREDRGETTARGETINFO(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_CREDRPROFILELOADED_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_CREDRPROFILELOADED_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS rpccli_lsa_CREDRPROFILELOADED(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_LookupNames3_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						struct policy_handle *_handle /* [in] [ref] */,
						uint32_t _num_names /* [in] [range(0,1000)] */,
						struct lsa_String *_names /* [in] [size_is(num_names)] */,
						struct lsa_RefDomainList **_domains /* [out] [ref] */,
						struct lsa_TransSidArray3 *_sids /* [in,out] [ref] */,
						enum lsa_LookupNamesLevel _level /* [in]  */,
						uint32_t *_count /* [in,out] [ref] */,
						enum lsa_LookupOptions _lookup_options /* [in]  */,
						enum lsa_ClientRevision _client_revision /* [in]  */);
NTSTATUS rpccli_lsa_LookupNames3_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS rpccli_lsa_LookupNames3(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *handle /* [in] [ref] */,
				 uint32_t num_names /* [in] [range(0,1000)] */,
				 struct lsa_String *names /* [in] [size_is(num_names)] */,
				 struct lsa_RefDomainList **domains /* [out] [ref] */,
				 struct lsa_TransSidArray3 *sids /* [in,out] [ref] */,
				 enum lsa_LookupNamesLevel level /* [in]  */,
				 uint32_t *count /* [in,out] [ref] */,
				 enum lsa_LookupOptions lookup_options /* [in]  */,
				 enum lsa_ClientRevision client_revision /* [in]  */);
struct tevent_req *rpccli_lsa_CREDRGETSESSIONTYPES_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_CREDRGETSESSIONTYPES_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS rpccli_lsa_CREDRGETSESSIONTYPES(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_LSARREGISTERAUDITEVENT_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_LSARREGISTERAUDITEVENT_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS rpccli_lsa_LSARREGISTERAUDITEVENT(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_LSARGENAUDITEVENT_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_LSARGENAUDITEVENT_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   NTSTATUS *result);
NTSTATUS rpccli_lsa_LSARGENAUDITEVENT(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_LSARUNREGISTERAUDITEVENT_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_LSARUNREGISTERAUDITEVENT_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  NTSTATUS *result);
NTSTATUS rpccli_lsa_LSARUNREGISTERAUDITEVENT(struct rpc_pipe_client *cli,
					     TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_lsaRQueryForestTrustInformation_send(TALLOC_CTX *mem_ctx,
								   struct tevent_context *ev,
								   struct rpc_pipe_client *cli,
								   struct policy_handle *_handle /* [in] [ref] */,
								   struct lsa_String *_trusted_domain_name /* [in] [ref] */,
								   uint16_t _unknown /* [in]  */,
								   struct lsa_ForestTrustInformation **_forest_trust_info /* [out] [ref] */);
NTSTATUS rpccli_lsa_lsaRQueryForestTrustInformation_recv(struct tevent_req *req,
							 TALLOC_CTX *mem_ctx,
							 NTSTATUS *result);
NTSTATUS rpccli_lsa_lsaRQueryForestTrustInformation(struct rpc_pipe_client *cli,
						    TALLOC_CTX *mem_ctx,
						    struct policy_handle *handle /* [in] [ref] */,
						    struct lsa_String *trusted_domain_name /* [in] [ref] */,
						    uint16_t unknown /* [in]  */,
						    struct lsa_ForestTrustInformation **forest_trust_info /* [out] [ref] */);
struct tevent_req *rpccli_lsa_LSARSETFORESTTRUSTINFORMATION_send(TALLOC_CTX *mem_ctx,
								 struct tevent_context *ev,
								 struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_LSARSETFORESTTRUSTINFORMATION_recv(struct tevent_req *req,
						       TALLOC_CTX *mem_ctx,
						       NTSTATUS *result);
NTSTATUS rpccli_lsa_LSARSETFORESTTRUSTINFORMATION(struct rpc_pipe_client *cli,
						  TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_CREDRRENAME_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_CREDRRENAME_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS rpccli_lsa_CREDRRENAME(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_LookupSids3_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       struct lsa_SidArray *_sids /* [in] [ref] */,
					       struct lsa_RefDomainList **_domains /* [out] [ref] */,
					       struct lsa_TransNameArray2 *_names /* [in,out] [ref] */,
					       enum lsa_LookupNamesLevel _level /* [in]  */,
					       uint32_t *_count /* [in,out] [ref] */,
					       enum lsa_LookupOptions _lookup_options /* [in]  */,
					       enum lsa_ClientRevision _client_revision /* [in]  */);
NTSTATUS rpccli_lsa_LookupSids3_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS rpccli_lsa_LookupSids3(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct lsa_SidArray *sids /* [in] [ref] */,
				struct lsa_RefDomainList **domains /* [out] [ref] */,
				struct lsa_TransNameArray2 *names /* [in,out] [ref] */,
				enum lsa_LookupNamesLevel level /* [in]  */,
				uint32_t *count /* [in,out] [ref] */,
				enum lsa_LookupOptions lookup_options /* [in]  */,
				enum lsa_ClientRevision client_revision /* [in]  */);
struct tevent_req *rpccli_lsa_LookupNames4_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						uint32_t _num_names /* [in] [range(0,1000)] */,
						struct lsa_String *_names /* [in] [size_is(num_names)] */,
						struct lsa_RefDomainList **_domains /* [out] [ref] */,
						struct lsa_TransSidArray3 *_sids /* [in,out] [ref] */,
						enum lsa_LookupNamesLevel _level /* [in]  */,
						uint32_t *_count /* [in,out] [ref] */,
						enum lsa_LookupOptions _lookup_options /* [in]  */,
						enum lsa_ClientRevision _client_revision /* [in]  */);
NTSTATUS rpccli_lsa_LookupNames4_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS rpccli_lsa_LookupNames4(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 uint32_t num_names /* [in] [range(0,1000)] */,
				 struct lsa_String *names /* [in] [size_is(num_names)] */,
				 struct lsa_RefDomainList **domains /* [out] [ref] */,
				 struct lsa_TransSidArray3 *sids /* [in,out] [ref] */,
				 enum lsa_LookupNamesLevel level /* [in]  */,
				 uint32_t *count /* [in,out] [ref] */,
				 enum lsa_LookupOptions lookup_options /* [in]  */,
				 enum lsa_ClientRevision client_revision /* [in]  */);
struct tevent_req *rpccli_lsa_LSAROPENPOLICYSCE_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_LSAROPENPOLICYSCE_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   NTSTATUS *result);
NTSTATUS rpccli_lsa_LSAROPENPOLICYSCE(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_LSARADTREGISTERSECURITYEVENTSOURCE_send(TALLOC_CTX *mem_ctx,
								      struct tevent_context *ev,
								      struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_LSARADTREGISTERSECURITYEVENTSOURCE_recv(struct tevent_req *req,
							    TALLOC_CTX *mem_ctx,
							    NTSTATUS *result);
NTSTATUS rpccli_lsa_LSARADTREGISTERSECURITYEVENTSOURCE(struct rpc_pipe_client *cli,
						       TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_LSARADTUNREGISTERSECURITYEVENTSOURCE_send(TALLOC_CTX *mem_ctx,
									struct tevent_context *ev,
									struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_LSARADTUNREGISTERSECURITYEVENTSOURCE_recv(struct tevent_req *req,
							      TALLOC_CTX *mem_ctx,
							      NTSTATUS *result);
NTSTATUS rpccli_lsa_LSARADTUNREGISTERSECURITYEVENTSOURCE(struct rpc_pipe_client *cli,
							 TALLOC_CTX *mem_ctx);
struct tevent_req *rpccli_lsa_LSARADTREPORTSECURITYEVENT_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct rpc_pipe_client *cli);
NTSTATUS rpccli_lsa_LSARADTREPORTSECURITYEVENT_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    NTSTATUS *result);
NTSTATUS rpccli_lsa_LSARADTREPORTSECURITYEVENT(struct rpc_pipe_client *cli,
					       TALLOC_CTX *mem_ctx);
#endif /* __CLI_LSARPC__ */
