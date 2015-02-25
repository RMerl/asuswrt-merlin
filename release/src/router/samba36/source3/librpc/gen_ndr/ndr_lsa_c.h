#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/lsa.h"
#ifndef _HEADER_RPC_lsarpc
#define _HEADER_RPC_lsarpc

extern const struct ndr_interface_table ndr_table_lsarpc;

struct tevent_req *dcerpc_lsa_Close_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_Close *r);
NTSTATUS dcerpc_lsa_Close_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_Close_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_Close *r);
struct tevent_req *dcerpc_lsa_Close_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct dcerpc_binding_handle *h,
					 struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_lsa_Close_recv(struct tevent_req *req,
			       TALLOC_CTX *mem_ctx,
			       NTSTATUS *result);
NTSTATUS dcerpc_lsa_Close(struct dcerpc_binding_handle *h,
			  TALLOC_CTX *mem_ctx,
			  struct policy_handle *_handle /* [in,out] [ref] */,
			  NTSTATUS *result);

struct tevent_req *dcerpc_lsa_Delete_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_Delete *r);
NTSTATUS dcerpc_lsa_Delete_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_Delete_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_Delete *r);
struct tevent_req *dcerpc_lsa_Delete_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS dcerpc_lsa_Delete_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				NTSTATUS *result);
NTSTATUS dcerpc_lsa_Delete(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   struct policy_handle *_handle /* [in] [ref] */,
			   NTSTATUS *result);

struct tevent_req *dcerpc_lsa_EnumPrivs_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_EnumPrivs *r);
NTSTATUS dcerpc_lsa_EnumPrivs_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_EnumPrivs_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_EnumPrivs *r);
struct tevent_req *dcerpc_lsa_EnumPrivs_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     struct policy_handle *_handle /* [in] [ref] */,
					     uint32_t *_resume_handle /* [in,out] [ref] */,
					     struct lsa_PrivArray *_privs /* [out] [ref] */,
					     uint32_t _max_count /* [in]  */);
NTSTATUS dcerpc_lsa_EnumPrivs_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_lsa_EnumPrivs(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      struct policy_handle *_handle /* [in] [ref] */,
			      uint32_t *_resume_handle /* [in,out] [ref] */,
			      struct lsa_PrivArray *_privs /* [out] [ref] */,
			      uint32_t _max_count /* [in]  */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_lsa_QuerySecurity_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_QuerySecurity *r);
NTSTATUS dcerpc_lsa_QuerySecurity_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_QuerySecurity_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_QuerySecurity *r);
struct tevent_req *dcerpc_lsa_QuerySecurity_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct policy_handle *_handle /* [in] [ref] */,
						 uint32_t _sec_info /* [in]  */,
						 struct sec_desc_buf **_sdbuf /* [out] [ref] */);
NTSTATUS dcerpc_lsa_QuerySecurity_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_lsa_QuerySecurity(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *_handle /* [in] [ref] */,
				  uint32_t _sec_info /* [in]  */,
				  struct sec_desc_buf **_sdbuf /* [out] [ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_lsa_SetSecObj_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_SetSecObj *r);
NTSTATUS dcerpc_lsa_SetSecObj_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_SetSecObj_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_SetSecObj *r);
struct tevent_req *dcerpc_lsa_SetSecObj_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     struct policy_handle *_handle /* [in] [ref] */,
					     uint32_t _sec_info /* [in]  */,
					     struct sec_desc_buf *_sdbuf /* [in] [ref] */);
NTSTATUS dcerpc_lsa_SetSecObj_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_lsa_SetSecObj(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      struct policy_handle *_handle /* [in] [ref] */,
			      uint32_t _sec_info /* [in]  */,
			      struct sec_desc_buf *_sdbuf /* [in] [ref] */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_lsa_OpenPolicy_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_OpenPolicy *r);
NTSTATUS dcerpc_lsa_OpenPolicy_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_OpenPolicy_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_OpenPolicy *r);
struct tevent_req *dcerpc_lsa_OpenPolicy_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      uint16_t *_system_name /* [in] [unique] */,
					      struct lsa_ObjectAttribute *_attr /* [in] [ref] */,
					      uint32_t _access_mask /* [in]  */,
					      struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_lsa_OpenPolicy_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    NTSTATUS *result);
NTSTATUS dcerpc_lsa_OpenPolicy(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       uint16_t *_system_name /* [in] [unique] */,
			       struct lsa_ObjectAttribute *_attr /* [in] [ref] */,
			       uint32_t _access_mask /* [in]  */,
			       struct policy_handle *_handle /* [out] [ref] */,
			       NTSTATUS *result);

struct tevent_req *dcerpc_lsa_QueryInfoPolicy_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_QueryInfoPolicy *r);
NTSTATUS dcerpc_lsa_QueryInfoPolicy_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_QueryInfoPolicy_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_QueryInfoPolicy *r);
struct tevent_req *dcerpc_lsa_QueryInfoPolicy_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct policy_handle *_handle /* [in] [ref] */,
						   enum lsa_PolicyInfo _level /* [in]  */,
						   union lsa_PolicyInformation **_info /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_lsa_QueryInfoPolicy_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS dcerpc_lsa_QueryInfoPolicy(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *_handle /* [in] [ref] */,
				    enum lsa_PolicyInfo _level /* [in]  */,
				    union lsa_PolicyInformation **_info /* [out] [ref,switch_is(level)] */,
				    NTSTATUS *result);

struct tevent_req *dcerpc_lsa_SetInfoPolicy_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_SetInfoPolicy *r);
NTSTATUS dcerpc_lsa_SetInfoPolicy_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_SetInfoPolicy_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_SetInfoPolicy *r);
struct tevent_req *dcerpc_lsa_SetInfoPolicy_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct policy_handle *_handle /* [in] [ref] */,
						 enum lsa_PolicyInfo _level /* [in]  */,
						 union lsa_PolicyInformation *_info /* [in] [switch_is(level),ref] */);
NTSTATUS dcerpc_lsa_SetInfoPolicy_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_lsa_SetInfoPolicy(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *_handle /* [in] [ref] */,
				  enum lsa_PolicyInfo _level /* [in]  */,
				  union lsa_PolicyInformation *_info /* [in] [switch_is(level),ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_lsa_CreateAccount_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_CreateAccount *r);
NTSTATUS dcerpc_lsa_CreateAccount_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_CreateAccount_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_CreateAccount *r);
struct tevent_req *dcerpc_lsa_CreateAccount_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct policy_handle *_handle /* [in] [ref] */,
						 struct dom_sid2 *_sid /* [in] [ref] */,
						 uint32_t _access_mask /* [in]  */,
						 struct policy_handle *_acct_handle /* [out] [ref] */);
NTSTATUS dcerpc_lsa_CreateAccount_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_lsa_CreateAccount(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *_handle /* [in] [ref] */,
				  struct dom_sid2 *_sid /* [in] [ref] */,
				  uint32_t _access_mask /* [in]  */,
				  struct policy_handle *_acct_handle /* [out] [ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_lsa_EnumAccounts_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_EnumAccounts *r);
NTSTATUS dcerpc_lsa_EnumAccounts_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_EnumAccounts_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_EnumAccounts *r);
struct tevent_req *dcerpc_lsa_EnumAccounts_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_handle /* [in] [ref] */,
						uint32_t *_resume_handle /* [in,out] [ref] */,
						struct lsa_SidArray *_sids /* [out] [ref] */,
						uint32_t _num_entries /* [in] [range(0,8192)] */);
NTSTATUS dcerpc_lsa_EnumAccounts_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_lsa_EnumAccounts(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_handle /* [in] [ref] */,
				 uint32_t *_resume_handle /* [in,out] [ref] */,
				 struct lsa_SidArray *_sids /* [out] [ref] */,
				 uint32_t _num_entries /* [in] [range(0,8192)] */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_lsa_CreateTrustedDomain_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_CreateTrustedDomain *r);
NTSTATUS dcerpc_lsa_CreateTrustedDomain_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_CreateTrustedDomain_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_CreateTrustedDomain *r);
struct tevent_req *dcerpc_lsa_CreateTrustedDomain_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct policy_handle *_policy_handle /* [in] [ref] */,
						       struct lsa_DomainInfo *_info /* [in] [ref] */,
						       uint32_t _access_mask /* [in]  */,
						       struct policy_handle *_trustdom_handle /* [out] [ref] */);
NTSTATUS dcerpc_lsa_CreateTrustedDomain_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS dcerpc_lsa_CreateTrustedDomain(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *_policy_handle /* [in] [ref] */,
					struct lsa_DomainInfo *_info /* [in] [ref] */,
					uint32_t _access_mask /* [in]  */,
					struct policy_handle *_trustdom_handle /* [out] [ref] */,
					NTSTATUS *result);

struct tevent_req *dcerpc_lsa_EnumTrustDom_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_EnumTrustDom *r);
NTSTATUS dcerpc_lsa_EnumTrustDom_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_EnumTrustDom_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_EnumTrustDom *r);
struct tevent_req *dcerpc_lsa_EnumTrustDom_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_handle /* [in] [ref] */,
						uint32_t *_resume_handle /* [in,out] [ref] */,
						struct lsa_DomainList *_domains /* [out] [ref] */,
						uint32_t _max_size /* [in]  */);
NTSTATUS dcerpc_lsa_EnumTrustDom_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_lsa_EnumTrustDom(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_handle /* [in] [ref] */,
				 uint32_t *_resume_handle /* [in,out] [ref] */,
				 struct lsa_DomainList *_domains /* [out] [ref] */,
				 uint32_t _max_size /* [in]  */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_lsa_LookupNames_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_LookupNames *r);
NTSTATUS dcerpc_lsa_LookupNames_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_LookupNames_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_LookupNames *r);
struct tevent_req *dcerpc_lsa_LookupNames_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_handle /* [in] [ref] */,
					       uint32_t _num_names /* [in] [range(0,1000)] */,
					       struct lsa_String *_names /* [in] [size_is(num_names)] */,
					       struct lsa_RefDomainList **_domains /* [out] [ref] */,
					       struct lsa_TransSidArray *_sids /* [in,out] [ref] */,
					       enum lsa_LookupNamesLevel _level /* [in]  */,
					       uint32_t *_count /* [in,out] [ref] */);
NTSTATUS dcerpc_lsa_LookupNames_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS dcerpc_lsa_LookupNames(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_handle /* [in] [ref] */,
				uint32_t _num_names /* [in] [range(0,1000)] */,
				struct lsa_String *_names /* [in] [size_is(num_names)] */,
				struct lsa_RefDomainList **_domains /* [out] [ref] */,
				struct lsa_TransSidArray *_sids /* [in,out] [ref] */,
				enum lsa_LookupNamesLevel _level /* [in]  */,
				uint32_t *_count /* [in,out] [ref] */,
				NTSTATUS *result);

struct tevent_req *dcerpc_lsa_LookupSids_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_LookupSids *r);
NTSTATUS dcerpc_lsa_LookupSids_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_LookupSids_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_LookupSids *r);
struct tevent_req *dcerpc_lsa_LookupSids_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct policy_handle *_handle /* [in] [ref] */,
					      struct lsa_SidArray *_sids /* [in] [ref] */,
					      struct lsa_RefDomainList **_domains /* [out] [ref] */,
					      struct lsa_TransNameArray *_names /* [in,out] [ref] */,
					      enum lsa_LookupNamesLevel _level /* [in]  */,
					      uint32_t *_count /* [in,out] [ref] */);
NTSTATUS dcerpc_lsa_LookupSids_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    NTSTATUS *result);
NTSTATUS dcerpc_lsa_LookupSids(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *_handle /* [in] [ref] */,
			       struct lsa_SidArray *_sids /* [in] [ref] */,
			       struct lsa_RefDomainList **_domains /* [out] [ref] */,
			       struct lsa_TransNameArray *_names /* [in,out] [ref] */,
			       enum lsa_LookupNamesLevel _level /* [in]  */,
			       uint32_t *_count /* [in,out] [ref] */,
			       NTSTATUS *result);

struct tevent_req *dcerpc_lsa_CreateSecret_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_CreateSecret *r);
NTSTATUS dcerpc_lsa_CreateSecret_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_CreateSecret_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_CreateSecret *r);
struct tevent_req *dcerpc_lsa_CreateSecret_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_handle /* [in] [ref] */,
						struct lsa_String _name /* [in]  */,
						uint32_t _access_mask /* [in]  */,
						struct policy_handle *_sec_handle /* [out] [ref] */);
NTSTATUS dcerpc_lsa_CreateSecret_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_lsa_CreateSecret(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_handle /* [in] [ref] */,
				 struct lsa_String _name /* [in]  */,
				 uint32_t _access_mask /* [in]  */,
				 struct policy_handle *_sec_handle /* [out] [ref] */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_lsa_OpenAccount_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_OpenAccount *r);
NTSTATUS dcerpc_lsa_OpenAccount_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_OpenAccount_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_OpenAccount *r);
struct tevent_req *dcerpc_lsa_OpenAccount_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_handle /* [in] [ref] */,
					       struct dom_sid2 *_sid /* [in] [ref] */,
					       uint32_t _access_mask /* [in]  */,
					       struct policy_handle *_acct_handle /* [out] [ref] */);
NTSTATUS dcerpc_lsa_OpenAccount_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS dcerpc_lsa_OpenAccount(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_handle /* [in] [ref] */,
				struct dom_sid2 *_sid /* [in] [ref] */,
				uint32_t _access_mask /* [in]  */,
				struct policy_handle *_acct_handle /* [out] [ref] */,
				NTSTATUS *result);

struct tevent_req *dcerpc_lsa_EnumPrivsAccount_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_EnumPrivsAccount *r);
NTSTATUS dcerpc_lsa_EnumPrivsAccount_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_EnumPrivsAccount_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_EnumPrivsAccount *r);
struct tevent_req *dcerpc_lsa_EnumPrivsAccount_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_handle /* [in] [ref] */,
						    struct lsa_PrivilegeSet **_privs /* [out] [ref] */);
NTSTATUS dcerpc_lsa_EnumPrivsAccount_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);
NTSTATUS dcerpc_lsa_EnumPrivsAccount(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_handle /* [in] [ref] */,
				     struct lsa_PrivilegeSet **_privs /* [out] [ref] */,
				     NTSTATUS *result);

struct tevent_req *dcerpc_lsa_AddPrivilegesToAccount_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_AddPrivilegesToAccount *r);
NTSTATUS dcerpc_lsa_AddPrivilegesToAccount_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_AddPrivilegesToAccount_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_AddPrivilegesToAccount *r);
struct tevent_req *dcerpc_lsa_AddPrivilegesToAccount_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_handle /* [in] [ref] */,
							  struct lsa_PrivilegeSet *_privs /* [in] [ref] */);
NTSTATUS dcerpc_lsa_AddPrivilegesToAccount_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS dcerpc_lsa_AddPrivilegesToAccount(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_handle /* [in] [ref] */,
					   struct lsa_PrivilegeSet *_privs /* [in] [ref] */,
					   NTSTATUS *result);

struct tevent_req *dcerpc_lsa_RemovePrivilegesFromAccount_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_RemovePrivilegesFromAccount *r);
NTSTATUS dcerpc_lsa_RemovePrivilegesFromAccount_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_RemovePrivilegesFromAccount_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_RemovePrivilegesFromAccount *r);
struct tevent_req *dcerpc_lsa_RemovePrivilegesFromAccount_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct dcerpc_binding_handle *h,
							       struct policy_handle *_handle /* [in] [ref] */,
							       uint8_t _remove_all /* [in]  */,
							       struct lsa_PrivilegeSet *_privs /* [in] [unique] */);
NTSTATUS dcerpc_lsa_RemovePrivilegesFromAccount_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     NTSTATUS *result);
NTSTATUS dcerpc_lsa_RemovePrivilegesFromAccount(struct dcerpc_binding_handle *h,
						TALLOC_CTX *mem_ctx,
						struct policy_handle *_handle /* [in] [ref] */,
						uint8_t _remove_all /* [in]  */,
						struct lsa_PrivilegeSet *_privs /* [in] [unique] */,
						NTSTATUS *result);

struct tevent_req *dcerpc_lsa_GetSystemAccessAccount_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_GetSystemAccessAccount *r);
NTSTATUS dcerpc_lsa_GetSystemAccessAccount_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_GetSystemAccessAccount_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_GetSystemAccessAccount *r);
struct tevent_req *dcerpc_lsa_GetSystemAccessAccount_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_handle /* [in] [ref] */,
							  uint32_t *_access_mask /* [out] [ref] */);
NTSTATUS dcerpc_lsa_GetSystemAccessAccount_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS dcerpc_lsa_GetSystemAccessAccount(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_handle /* [in] [ref] */,
					   uint32_t *_access_mask /* [out] [ref] */,
					   NTSTATUS *result);

struct tevent_req *dcerpc_lsa_SetSystemAccessAccount_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_SetSystemAccessAccount *r);
NTSTATUS dcerpc_lsa_SetSystemAccessAccount_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_SetSystemAccessAccount_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_SetSystemAccessAccount *r);
struct tevent_req *dcerpc_lsa_SetSystemAccessAccount_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_handle /* [in] [ref] */,
							  uint32_t _access_mask /* [in]  */);
NTSTATUS dcerpc_lsa_SetSystemAccessAccount_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS dcerpc_lsa_SetSystemAccessAccount(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_handle /* [in] [ref] */,
					   uint32_t _access_mask /* [in]  */,
					   NTSTATUS *result);

struct tevent_req *dcerpc_lsa_OpenTrustedDomain_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_OpenTrustedDomain *r);
NTSTATUS dcerpc_lsa_OpenTrustedDomain_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_OpenTrustedDomain_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_OpenTrustedDomain *r);
struct tevent_req *dcerpc_lsa_OpenTrustedDomain_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_handle /* [in] [ref] */,
						     struct dom_sid2 *_sid /* [in] [ref] */,
						     uint32_t _access_mask /* [in]  */,
						     struct policy_handle *_trustdom_handle /* [out] [ref] */);
NTSTATUS dcerpc_lsa_OpenTrustedDomain_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   NTSTATUS *result);
NTSTATUS dcerpc_lsa_OpenTrustedDomain(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_handle /* [in] [ref] */,
				      struct dom_sid2 *_sid /* [in] [ref] */,
				      uint32_t _access_mask /* [in]  */,
				      struct policy_handle *_trustdom_handle /* [out] [ref] */,
				      NTSTATUS *result);

struct tevent_req *dcerpc_lsa_QueryTrustedDomainInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_QueryTrustedDomainInfo *r);
NTSTATUS dcerpc_lsa_QueryTrustedDomainInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_QueryTrustedDomainInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_QueryTrustedDomainInfo *r);
struct tevent_req *dcerpc_lsa_QueryTrustedDomainInfo_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_trustdom_handle /* [in] [ref] */,
							  enum lsa_TrustDomInfoEnum _level /* [in]  */,
							  union lsa_TrustedDomainInfo **_info /* [out] [switch_is(level),ref] */);
NTSTATUS dcerpc_lsa_QueryTrustedDomainInfo_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS dcerpc_lsa_QueryTrustedDomainInfo(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_trustdom_handle /* [in] [ref] */,
					   enum lsa_TrustDomInfoEnum _level /* [in]  */,
					   union lsa_TrustedDomainInfo **_info /* [out] [switch_is(level),ref] */,
					   NTSTATUS *result);

struct tevent_req *dcerpc_lsa_SetInformationTrustedDomain_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_SetInformationTrustedDomain *r);
NTSTATUS dcerpc_lsa_SetInformationTrustedDomain_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_SetInformationTrustedDomain_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_SetInformationTrustedDomain *r);
struct tevent_req *dcerpc_lsa_SetInformationTrustedDomain_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct dcerpc_binding_handle *h,
							       struct policy_handle *_trustdom_handle /* [in] [ref] */,
							       enum lsa_TrustDomInfoEnum _level /* [in]  */,
							       union lsa_TrustedDomainInfo *_info /* [in] [ref,switch_is(level)] */);
NTSTATUS dcerpc_lsa_SetInformationTrustedDomain_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     NTSTATUS *result);
NTSTATUS dcerpc_lsa_SetInformationTrustedDomain(struct dcerpc_binding_handle *h,
						TALLOC_CTX *mem_ctx,
						struct policy_handle *_trustdom_handle /* [in] [ref] */,
						enum lsa_TrustDomInfoEnum _level /* [in]  */,
						union lsa_TrustedDomainInfo *_info /* [in] [ref,switch_is(level)] */,
						NTSTATUS *result);

struct tevent_req *dcerpc_lsa_OpenSecret_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_OpenSecret *r);
NTSTATUS dcerpc_lsa_OpenSecret_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_OpenSecret_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_OpenSecret *r);
struct tevent_req *dcerpc_lsa_OpenSecret_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct policy_handle *_handle /* [in] [ref] */,
					      struct lsa_String _name /* [in]  */,
					      uint32_t _access_mask /* [in]  */,
					      struct policy_handle *_sec_handle /* [out] [ref] */);
NTSTATUS dcerpc_lsa_OpenSecret_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    NTSTATUS *result);
NTSTATUS dcerpc_lsa_OpenSecret(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *_handle /* [in] [ref] */,
			       struct lsa_String _name /* [in]  */,
			       uint32_t _access_mask /* [in]  */,
			       struct policy_handle *_sec_handle /* [out] [ref] */,
			       NTSTATUS *result);

struct tevent_req *dcerpc_lsa_SetSecret_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_SetSecret *r);
NTSTATUS dcerpc_lsa_SetSecret_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_SetSecret_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_SetSecret *r);
struct tevent_req *dcerpc_lsa_SetSecret_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     struct policy_handle *_sec_handle /* [in] [ref] */,
					     struct lsa_DATA_BUF *_new_val /* [in] [unique] */,
					     struct lsa_DATA_BUF *_old_val /* [in] [unique] */);
NTSTATUS dcerpc_lsa_SetSecret_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_lsa_SetSecret(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      struct policy_handle *_sec_handle /* [in] [ref] */,
			      struct lsa_DATA_BUF *_new_val /* [in] [unique] */,
			      struct lsa_DATA_BUF *_old_val /* [in] [unique] */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_lsa_QuerySecret_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_QuerySecret *r);
NTSTATUS dcerpc_lsa_QuerySecret_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_QuerySecret_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_QuerySecret *r);
struct tevent_req *dcerpc_lsa_QuerySecret_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_sec_handle /* [in] [ref] */,
					       struct lsa_DATA_BUF_PTR *_new_val /* [in,out] [unique] */,
					       NTTIME *_new_mtime /* [in,out] [unique] */,
					       struct lsa_DATA_BUF_PTR *_old_val /* [in,out] [unique] */,
					       NTTIME *_old_mtime /* [in,out] [unique] */);
NTSTATUS dcerpc_lsa_QuerySecret_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS dcerpc_lsa_QuerySecret(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_sec_handle /* [in] [ref] */,
				struct lsa_DATA_BUF_PTR *_new_val /* [in,out] [unique] */,
				NTTIME *_new_mtime /* [in,out] [unique] */,
				struct lsa_DATA_BUF_PTR *_old_val /* [in,out] [unique] */,
				NTTIME *_old_mtime /* [in,out] [unique] */,
				NTSTATUS *result);

struct tevent_req *dcerpc_lsa_LookupPrivValue_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_LookupPrivValue *r);
NTSTATUS dcerpc_lsa_LookupPrivValue_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_LookupPrivValue_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_LookupPrivValue *r);
struct tevent_req *dcerpc_lsa_LookupPrivValue_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct policy_handle *_handle /* [in] [ref] */,
						   struct lsa_String *_name /* [in] [ref] */,
						   struct lsa_LUID *_luid /* [out] [ref] */);
NTSTATUS dcerpc_lsa_LookupPrivValue_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS dcerpc_lsa_LookupPrivValue(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *_handle /* [in] [ref] */,
				    struct lsa_String *_name /* [in] [ref] */,
				    struct lsa_LUID *_luid /* [out] [ref] */,
				    NTSTATUS *result);

struct tevent_req *dcerpc_lsa_LookupPrivName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_LookupPrivName *r);
NTSTATUS dcerpc_lsa_LookupPrivName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_LookupPrivName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_LookupPrivName *r);
struct tevent_req *dcerpc_lsa_LookupPrivName_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct policy_handle *_handle /* [in] [ref] */,
						  struct lsa_LUID *_luid /* [in] [ref] */,
						  struct lsa_StringLarge **_name /* [out] [ref] */);
NTSTATUS dcerpc_lsa_LookupPrivName_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					NTSTATUS *result);
NTSTATUS dcerpc_lsa_LookupPrivName(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *_handle /* [in] [ref] */,
				   struct lsa_LUID *_luid /* [in] [ref] */,
				   struct lsa_StringLarge **_name /* [out] [ref] */,
				   NTSTATUS *result);

struct tevent_req *dcerpc_lsa_LookupPrivDisplayName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_LookupPrivDisplayName *r);
NTSTATUS dcerpc_lsa_LookupPrivDisplayName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_LookupPrivDisplayName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_LookupPrivDisplayName *r);
struct tevent_req *dcerpc_lsa_LookupPrivDisplayName_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_handle /* [in] [ref] */,
							 struct lsa_String *_name /* [in] [ref] */,
							 uint16_t _language_id /* [in]  */,
							 uint16_t _language_id_sys /* [in]  */,
							 struct lsa_StringLarge **_disp_name /* [out] [ref] */,
							 uint16_t *_returned_language_id /* [out] [ref] */);
NTSTATUS dcerpc_lsa_LookupPrivDisplayName_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       NTSTATUS *result);
NTSTATUS dcerpc_lsa_LookupPrivDisplayName(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_handle /* [in] [ref] */,
					  struct lsa_String *_name /* [in] [ref] */,
					  uint16_t _language_id /* [in]  */,
					  uint16_t _language_id_sys /* [in]  */,
					  struct lsa_StringLarge **_disp_name /* [out] [ref] */,
					  uint16_t *_returned_language_id /* [out] [ref] */,
					  NTSTATUS *result);

struct tevent_req *dcerpc_lsa_DeleteObject_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_DeleteObject *r);
NTSTATUS dcerpc_lsa_DeleteObject_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_DeleteObject_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_DeleteObject *r);
struct tevent_req *dcerpc_lsa_DeleteObject_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_lsa_DeleteObject_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_lsa_DeleteObject(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_handle /* [in,out] [ref] */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_lsa_EnumAccountsWithUserRight_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_EnumAccountsWithUserRight *r);
NTSTATUS dcerpc_lsa_EnumAccountsWithUserRight_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_EnumAccountsWithUserRight_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_EnumAccountsWithUserRight *r);
struct tevent_req *dcerpc_lsa_EnumAccountsWithUserRight_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     struct policy_handle *_handle /* [in] [ref] */,
							     struct lsa_String *_name /* [in] [unique] */,
							     struct lsa_SidArray *_sids /* [out] [ref] */);
NTSTATUS dcerpc_lsa_EnumAccountsWithUserRight_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   NTSTATUS *result);
NTSTATUS dcerpc_lsa_EnumAccountsWithUserRight(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      struct policy_handle *_handle /* [in] [ref] */,
					      struct lsa_String *_name /* [in] [unique] */,
					      struct lsa_SidArray *_sids /* [out] [ref] */,
					      NTSTATUS *result);

struct tevent_req *dcerpc_lsa_EnumAccountRights_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_EnumAccountRights *r);
NTSTATUS dcerpc_lsa_EnumAccountRights_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_EnumAccountRights_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_EnumAccountRights *r);
struct tevent_req *dcerpc_lsa_EnumAccountRights_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_handle /* [in] [ref] */,
						     struct dom_sid2 *_sid /* [in] [ref] */,
						     struct lsa_RightSet *_rights /* [out] [ref] */);
NTSTATUS dcerpc_lsa_EnumAccountRights_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   NTSTATUS *result);
NTSTATUS dcerpc_lsa_EnumAccountRights(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_handle /* [in] [ref] */,
				      struct dom_sid2 *_sid /* [in] [ref] */,
				      struct lsa_RightSet *_rights /* [out] [ref] */,
				      NTSTATUS *result);

struct tevent_req *dcerpc_lsa_AddAccountRights_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_AddAccountRights *r);
NTSTATUS dcerpc_lsa_AddAccountRights_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_AddAccountRights_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_AddAccountRights *r);
struct tevent_req *dcerpc_lsa_AddAccountRights_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_handle /* [in] [ref] */,
						    struct dom_sid2 *_sid /* [in] [ref] */,
						    struct lsa_RightSet *_rights /* [in] [ref] */);
NTSTATUS dcerpc_lsa_AddAccountRights_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);
NTSTATUS dcerpc_lsa_AddAccountRights(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_handle /* [in] [ref] */,
				     struct dom_sid2 *_sid /* [in] [ref] */,
				     struct lsa_RightSet *_rights /* [in] [ref] */,
				     NTSTATUS *result);

struct tevent_req *dcerpc_lsa_RemoveAccountRights_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_RemoveAccountRights *r);
NTSTATUS dcerpc_lsa_RemoveAccountRights_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_RemoveAccountRights_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_RemoveAccountRights *r);
struct tevent_req *dcerpc_lsa_RemoveAccountRights_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct policy_handle *_handle /* [in] [ref] */,
						       struct dom_sid2 *_sid /* [in] [ref] */,
						       uint8_t _remove_all /* [in]  */,
						       struct lsa_RightSet *_rights /* [in] [ref] */);
NTSTATUS dcerpc_lsa_RemoveAccountRights_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS dcerpc_lsa_RemoveAccountRights(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *_handle /* [in] [ref] */,
					struct dom_sid2 *_sid /* [in] [ref] */,
					uint8_t _remove_all /* [in]  */,
					struct lsa_RightSet *_rights /* [in] [ref] */,
					NTSTATUS *result);

struct tevent_req *dcerpc_lsa_QueryTrustedDomainInfoBySid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_QueryTrustedDomainInfoBySid *r);
NTSTATUS dcerpc_lsa_QueryTrustedDomainInfoBySid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_QueryTrustedDomainInfoBySid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_QueryTrustedDomainInfoBySid *r);
struct tevent_req *dcerpc_lsa_QueryTrustedDomainInfoBySid_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct dcerpc_binding_handle *h,
							       struct policy_handle *_handle /* [in] [ref] */,
							       struct dom_sid2 *_dom_sid /* [in] [ref] */,
							       enum lsa_TrustDomInfoEnum _level /* [in]  */,
							       union lsa_TrustedDomainInfo **_info /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_lsa_QueryTrustedDomainInfoBySid_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     NTSTATUS *result);
NTSTATUS dcerpc_lsa_QueryTrustedDomainInfoBySid(struct dcerpc_binding_handle *h,
						TALLOC_CTX *mem_ctx,
						struct policy_handle *_handle /* [in] [ref] */,
						struct dom_sid2 *_dom_sid /* [in] [ref] */,
						enum lsa_TrustDomInfoEnum _level /* [in]  */,
						union lsa_TrustedDomainInfo **_info /* [out] [ref,switch_is(level)] */,
						NTSTATUS *result);

struct tevent_req *dcerpc_lsa_SetTrustedDomainInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_SetTrustedDomainInfo *r);
NTSTATUS dcerpc_lsa_SetTrustedDomainInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_SetTrustedDomainInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_SetTrustedDomainInfo *r);
struct tevent_req *dcerpc_lsa_SetTrustedDomainInfo_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_handle /* [in] [ref] */,
							struct dom_sid2 *_dom_sid /* [in] [ref] */,
							enum lsa_TrustDomInfoEnum _level /* [in]  */,
							union lsa_TrustedDomainInfo *_info /* [in] [ref,switch_is(level)] */);
NTSTATUS dcerpc_lsa_SetTrustedDomainInfo_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS dcerpc_lsa_SetTrustedDomainInfo(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_handle /* [in] [ref] */,
					 struct dom_sid2 *_dom_sid /* [in] [ref] */,
					 enum lsa_TrustDomInfoEnum _level /* [in]  */,
					 union lsa_TrustedDomainInfo *_info /* [in] [ref,switch_is(level)] */,
					 NTSTATUS *result);

struct tevent_req *dcerpc_lsa_DeleteTrustedDomain_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_DeleteTrustedDomain *r);
NTSTATUS dcerpc_lsa_DeleteTrustedDomain_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_DeleteTrustedDomain_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_DeleteTrustedDomain *r);
struct tevent_req *dcerpc_lsa_DeleteTrustedDomain_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct policy_handle *_handle /* [in] [ref] */,
						       struct dom_sid2 *_dom_sid /* [in] [ref] */);
NTSTATUS dcerpc_lsa_DeleteTrustedDomain_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS dcerpc_lsa_DeleteTrustedDomain(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *_handle /* [in] [ref] */,
					struct dom_sid2 *_dom_sid /* [in] [ref] */,
					NTSTATUS *result);

struct tevent_req *dcerpc_lsa_StorePrivateData_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_StorePrivateData *r);
NTSTATUS dcerpc_lsa_StorePrivateData_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_StorePrivateData_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_StorePrivateData *r);
struct tevent_req *dcerpc_lsa_StorePrivateData_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_handle /* [in] [ref] */,
						    struct lsa_String *_name /* [in] [ref] */,
						    struct lsa_DATA_BUF *_val /* [in] [unique] */);
NTSTATUS dcerpc_lsa_StorePrivateData_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);
NTSTATUS dcerpc_lsa_StorePrivateData(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_handle /* [in] [ref] */,
				     struct lsa_String *_name /* [in] [ref] */,
				     struct lsa_DATA_BUF *_val /* [in] [unique] */,
				     NTSTATUS *result);

struct tevent_req *dcerpc_lsa_RetrievePrivateData_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_RetrievePrivateData *r);
NTSTATUS dcerpc_lsa_RetrievePrivateData_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_RetrievePrivateData_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_RetrievePrivateData *r);
struct tevent_req *dcerpc_lsa_RetrievePrivateData_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct policy_handle *_handle /* [in] [ref] */,
						       struct lsa_String *_name /* [in] [ref] */,
						       struct lsa_DATA_BUF **_val /* [in,out] [ref] */);
NTSTATUS dcerpc_lsa_RetrievePrivateData_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS dcerpc_lsa_RetrievePrivateData(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *_handle /* [in] [ref] */,
					struct lsa_String *_name /* [in] [ref] */,
					struct lsa_DATA_BUF **_val /* [in,out] [ref] */,
					NTSTATUS *result);

struct tevent_req *dcerpc_lsa_OpenPolicy2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_OpenPolicy2 *r);
NTSTATUS dcerpc_lsa_OpenPolicy2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_OpenPolicy2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_OpenPolicy2 *r);
struct tevent_req *dcerpc_lsa_OpenPolicy2_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       const char *_system_name /* [in] [charset(UTF16),unique] */,
					       struct lsa_ObjectAttribute *_attr /* [in] [ref] */,
					       uint32_t _access_mask /* [in]  */,
					       struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_lsa_OpenPolicy2_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS dcerpc_lsa_OpenPolicy2(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				const char *_system_name /* [in] [charset(UTF16),unique] */,
				struct lsa_ObjectAttribute *_attr /* [in] [ref] */,
				uint32_t _access_mask /* [in]  */,
				struct policy_handle *_handle /* [out] [ref] */,
				NTSTATUS *result);

struct tevent_req *dcerpc_lsa_GetUserName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_GetUserName *r);
NTSTATUS dcerpc_lsa_GetUserName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_GetUserName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_GetUserName *r);
struct tevent_req *dcerpc_lsa_GetUserName_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       const char *_system_name /* [in] [charset(UTF16),unique] */,
					       struct lsa_String **_account_name /* [in,out] [ref] */,
					       struct lsa_String **_authority_name /* [in,out] [unique] */);
NTSTATUS dcerpc_lsa_GetUserName_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS dcerpc_lsa_GetUserName(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				const char *_system_name /* [in] [charset(UTF16),unique] */,
				struct lsa_String **_account_name /* [in,out] [ref] */,
				struct lsa_String **_authority_name /* [in,out] [unique] */,
				NTSTATUS *result);

struct tevent_req *dcerpc_lsa_QueryInfoPolicy2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_QueryInfoPolicy2 *r);
NTSTATUS dcerpc_lsa_QueryInfoPolicy2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_QueryInfoPolicy2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_QueryInfoPolicy2 *r);
struct tevent_req *dcerpc_lsa_QueryInfoPolicy2_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_handle /* [in] [ref] */,
						    enum lsa_PolicyInfo _level /* [in]  */,
						    union lsa_PolicyInformation **_info /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_lsa_QueryInfoPolicy2_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);
NTSTATUS dcerpc_lsa_QueryInfoPolicy2(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_handle /* [in] [ref] */,
				     enum lsa_PolicyInfo _level /* [in]  */,
				     union lsa_PolicyInformation **_info /* [out] [ref,switch_is(level)] */,
				     NTSTATUS *result);

struct tevent_req *dcerpc_lsa_SetInfoPolicy2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_SetInfoPolicy2 *r);
NTSTATUS dcerpc_lsa_SetInfoPolicy2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_SetInfoPolicy2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_SetInfoPolicy2 *r);
struct tevent_req *dcerpc_lsa_SetInfoPolicy2_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct policy_handle *_handle /* [in] [ref] */,
						  enum lsa_PolicyInfo _level /* [in]  */,
						  union lsa_PolicyInformation *_info /* [in] [switch_is(level),ref] */);
NTSTATUS dcerpc_lsa_SetInfoPolicy2_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					NTSTATUS *result);
NTSTATUS dcerpc_lsa_SetInfoPolicy2(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *_handle /* [in] [ref] */,
				   enum lsa_PolicyInfo _level /* [in]  */,
				   union lsa_PolicyInformation *_info /* [in] [switch_is(level),ref] */,
				   NTSTATUS *result);

struct tevent_req *dcerpc_lsa_QueryTrustedDomainInfoByName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_QueryTrustedDomainInfoByName *r);
NTSTATUS dcerpc_lsa_QueryTrustedDomainInfoByName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_QueryTrustedDomainInfoByName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_QueryTrustedDomainInfoByName *r);
struct tevent_req *dcerpc_lsa_QueryTrustedDomainInfoByName_send(TALLOC_CTX *mem_ctx,
								struct tevent_context *ev,
								struct dcerpc_binding_handle *h,
								struct policy_handle *_handle /* [in] [ref] */,
								struct lsa_String *_trusted_domain /* [in] [ref] */,
								enum lsa_TrustDomInfoEnum _level /* [in]  */,
								union lsa_TrustedDomainInfo **_info /* [out] [switch_is(level),ref] */);
NTSTATUS dcerpc_lsa_QueryTrustedDomainInfoByName_recv(struct tevent_req *req,
						      TALLOC_CTX *mem_ctx,
						      NTSTATUS *result);
NTSTATUS dcerpc_lsa_QueryTrustedDomainInfoByName(struct dcerpc_binding_handle *h,
						 TALLOC_CTX *mem_ctx,
						 struct policy_handle *_handle /* [in] [ref] */,
						 struct lsa_String *_trusted_domain /* [in] [ref] */,
						 enum lsa_TrustDomInfoEnum _level /* [in]  */,
						 union lsa_TrustedDomainInfo **_info /* [out] [switch_is(level),ref] */,
						 NTSTATUS *result);

struct tevent_req *dcerpc_lsa_SetTrustedDomainInfoByName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_SetTrustedDomainInfoByName *r);
NTSTATUS dcerpc_lsa_SetTrustedDomainInfoByName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_SetTrustedDomainInfoByName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_SetTrustedDomainInfoByName *r);
struct tevent_req *dcerpc_lsa_SetTrustedDomainInfoByName_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      struct policy_handle *_handle /* [in] [ref] */,
							      struct lsa_String *_trusted_domain /* [in] [ref] */,
							      enum lsa_TrustDomInfoEnum _level /* [in]  */,
							      union lsa_TrustedDomainInfo *_info /* [in] [ref,switch_is(level)] */);
NTSTATUS dcerpc_lsa_SetTrustedDomainInfoByName_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    NTSTATUS *result);
NTSTATUS dcerpc_lsa_SetTrustedDomainInfoByName(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       struct policy_handle *_handle /* [in] [ref] */,
					       struct lsa_String *_trusted_domain /* [in] [ref] */,
					       enum lsa_TrustDomInfoEnum _level /* [in]  */,
					       union lsa_TrustedDomainInfo *_info /* [in] [ref,switch_is(level)] */,
					       NTSTATUS *result);

struct tevent_req *dcerpc_lsa_EnumTrustedDomainsEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_EnumTrustedDomainsEx *r);
NTSTATUS dcerpc_lsa_EnumTrustedDomainsEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_EnumTrustedDomainsEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_EnumTrustedDomainsEx *r);
struct tevent_req *dcerpc_lsa_EnumTrustedDomainsEx_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_handle /* [in] [ref] */,
							uint32_t *_resume_handle /* [in,out] [ref] */,
							struct lsa_DomainListEx *_domains /* [out] [ref] */,
							uint32_t _max_size /* [in]  */);
NTSTATUS dcerpc_lsa_EnumTrustedDomainsEx_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS dcerpc_lsa_EnumTrustedDomainsEx(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_handle /* [in] [ref] */,
					 uint32_t *_resume_handle /* [in,out] [ref] */,
					 struct lsa_DomainListEx *_domains /* [out] [ref] */,
					 uint32_t _max_size /* [in]  */,
					 NTSTATUS *result);

struct tevent_req *dcerpc_lsa_CreateTrustedDomainEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_CreateTrustedDomainEx *r);
NTSTATUS dcerpc_lsa_CreateTrustedDomainEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_CreateTrustedDomainEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_CreateTrustedDomainEx *r);
struct tevent_req *dcerpc_lsa_CreateTrustedDomainEx_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_policy_handle /* [in] [ref] */,
							 struct lsa_TrustDomainInfoInfoEx *_info /* [in] [ref] */,
							 struct lsa_TrustDomainInfoAuthInfoInternal *_auth_info /* [in] [ref] */,
							 uint32_t _access_mask /* [in]  */,
							 struct policy_handle *_trustdom_handle /* [out] [ref] */);
NTSTATUS dcerpc_lsa_CreateTrustedDomainEx_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       NTSTATUS *result);
NTSTATUS dcerpc_lsa_CreateTrustedDomainEx(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_policy_handle /* [in] [ref] */,
					  struct lsa_TrustDomainInfoInfoEx *_info /* [in] [ref] */,
					  struct lsa_TrustDomainInfoAuthInfoInternal *_auth_info /* [in] [ref] */,
					  uint32_t _access_mask /* [in]  */,
					  struct policy_handle *_trustdom_handle /* [out] [ref] */,
					  NTSTATUS *result);

struct tevent_req *dcerpc_lsa_CloseTrustedDomainEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_CloseTrustedDomainEx *r);
NTSTATUS dcerpc_lsa_CloseTrustedDomainEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_CloseTrustedDomainEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_CloseTrustedDomainEx *r);
struct tevent_req *dcerpc_lsa_CloseTrustedDomainEx_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_lsa_CloseTrustedDomainEx_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS dcerpc_lsa_CloseTrustedDomainEx(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_handle /* [in,out] [ref] */,
					 NTSTATUS *result);

struct tevent_req *dcerpc_lsa_QueryDomainInformationPolicy_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_QueryDomainInformationPolicy *r);
NTSTATUS dcerpc_lsa_QueryDomainInformationPolicy_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_QueryDomainInformationPolicy_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_QueryDomainInformationPolicy *r);
struct tevent_req *dcerpc_lsa_QueryDomainInformationPolicy_send(TALLOC_CTX *mem_ctx,
								struct tevent_context *ev,
								struct dcerpc_binding_handle *h,
								struct policy_handle *_handle /* [in] [ref] */,
								uint16_t _level /* [in]  */,
								union lsa_DomainInformationPolicy **_info /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_lsa_QueryDomainInformationPolicy_recv(struct tevent_req *req,
						      TALLOC_CTX *mem_ctx,
						      NTSTATUS *result);
NTSTATUS dcerpc_lsa_QueryDomainInformationPolicy(struct dcerpc_binding_handle *h,
						 TALLOC_CTX *mem_ctx,
						 struct policy_handle *_handle /* [in] [ref] */,
						 uint16_t _level /* [in]  */,
						 union lsa_DomainInformationPolicy **_info /* [out] [ref,switch_is(level)] */,
						 NTSTATUS *result);

struct tevent_req *dcerpc_lsa_SetDomainInformationPolicy_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_SetDomainInformationPolicy *r);
NTSTATUS dcerpc_lsa_SetDomainInformationPolicy_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_SetDomainInformationPolicy_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_SetDomainInformationPolicy *r);
struct tevent_req *dcerpc_lsa_SetDomainInformationPolicy_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      struct policy_handle *_handle /* [in] [ref] */,
							      uint16_t _level /* [in]  */,
							      union lsa_DomainInformationPolicy *_info /* [in] [unique,switch_is(level)] */);
NTSTATUS dcerpc_lsa_SetDomainInformationPolicy_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    NTSTATUS *result);
NTSTATUS dcerpc_lsa_SetDomainInformationPolicy(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       struct policy_handle *_handle /* [in] [ref] */,
					       uint16_t _level /* [in]  */,
					       union lsa_DomainInformationPolicy *_info /* [in] [unique,switch_is(level)] */,
					       NTSTATUS *result);

struct tevent_req *dcerpc_lsa_OpenTrustedDomainByName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_OpenTrustedDomainByName *r);
NTSTATUS dcerpc_lsa_OpenTrustedDomainByName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_OpenTrustedDomainByName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_OpenTrustedDomainByName *r);
struct tevent_req *dcerpc_lsa_OpenTrustedDomainByName_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   struct policy_handle *_handle /* [in] [ref] */,
							   struct lsa_String _name /* [in]  */,
							   uint32_t _access_mask /* [in]  */,
							   struct policy_handle *_trustdom_handle /* [out] [ref] */);
NTSTATUS dcerpc_lsa_OpenTrustedDomainByName_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 NTSTATUS *result);
NTSTATUS dcerpc_lsa_OpenTrustedDomainByName(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    struct policy_handle *_handle /* [in] [ref] */,
					    struct lsa_String _name /* [in]  */,
					    uint32_t _access_mask /* [in]  */,
					    struct policy_handle *_trustdom_handle /* [out] [ref] */,
					    NTSTATUS *result);

struct tevent_req *dcerpc_lsa_LookupSids2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_LookupSids2 *r);
NTSTATUS dcerpc_lsa_LookupSids2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_LookupSids2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_LookupSids2 *r);
struct tevent_req *dcerpc_lsa_LookupSids2_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_handle /* [in] [ref] */,
					       struct lsa_SidArray *_sids /* [in] [ref] */,
					       struct lsa_RefDomainList **_domains /* [out] [ref] */,
					       struct lsa_TransNameArray2 *_names /* [in,out] [ref] */,
					       enum lsa_LookupNamesLevel _level /* [in]  */,
					       uint32_t *_count /* [in,out] [ref] */,
					       enum lsa_LookupOptions _lookup_options /* [in]  */,
					       enum lsa_ClientRevision _client_revision /* [in]  */);
NTSTATUS dcerpc_lsa_LookupSids2_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS dcerpc_lsa_LookupSids2(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_handle /* [in] [ref] */,
				struct lsa_SidArray *_sids /* [in] [ref] */,
				struct lsa_RefDomainList **_domains /* [out] [ref] */,
				struct lsa_TransNameArray2 *_names /* [in,out] [ref] */,
				enum lsa_LookupNamesLevel _level /* [in]  */,
				uint32_t *_count /* [in,out] [ref] */,
				enum lsa_LookupOptions _lookup_options /* [in]  */,
				enum lsa_ClientRevision _client_revision /* [in]  */,
				NTSTATUS *result);

struct tevent_req *dcerpc_lsa_LookupNames2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_LookupNames2 *r);
NTSTATUS dcerpc_lsa_LookupNames2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_LookupNames2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_LookupNames2 *r);
struct tevent_req *dcerpc_lsa_LookupNames2_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_handle /* [in] [ref] */,
						uint32_t _num_names /* [in] [range(0,1000)] */,
						struct lsa_String *_names /* [in] [size_is(num_names)] */,
						struct lsa_RefDomainList **_domains /* [out] [ref] */,
						struct lsa_TransSidArray2 *_sids /* [in,out] [ref] */,
						enum lsa_LookupNamesLevel _level /* [in]  */,
						uint32_t *_count /* [in,out] [ref] */,
						enum lsa_LookupOptions _lookup_options /* [in]  */,
						enum lsa_ClientRevision _client_revision /* [in]  */);
NTSTATUS dcerpc_lsa_LookupNames2_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_lsa_LookupNames2(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_handle /* [in] [ref] */,
				 uint32_t _num_names /* [in] [range(0,1000)] */,
				 struct lsa_String *_names /* [in] [size_is(num_names)] */,
				 struct lsa_RefDomainList **_domains /* [out] [ref] */,
				 struct lsa_TransSidArray2 *_sids /* [in,out] [ref] */,
				 enum lsa_LookupNamesLevel _level /* [in]  */,
				 uint32_t *_count /* [in,out] [ref] */,
				 enum lsa_LookupOptions _lookup_options /* [in]  */,
				 enum lsa_ClientRevision _client_revision /* [in]  */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_lsa_CreateTrustedDomainEx2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_CreateTrustedDomainEx2 *r);
NTSTATUS dcerpc_lsa_CreateTrustedDomainEx2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_CreateTrustedDomainEx2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_CreateTrustedDomainEx2 *r);
struct tevent_req *dcerpc_lsa_CreateTrustedDomainEx2_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_policy_handle /* [in] [ref] */,
							  struct lsa_TrustDomainInfoInfoEx *_info /* [in] [ref] */,
							  struct lsa_TrustDomainInfoAuthInfoInternal *_auth_info /* [in] [ref] */,
							  uint32_t _access_mask /* [in]  */,
							  struct policy_handle *_trustdom_handle /* [out] [ref] */);
NTSTATUS dcerpc_lsa_CreateTrustedDomainEx2_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS dcerpc_lsa_CreateTrustedDomainEx2(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_policy_handle /* [in] [ref] */,
					   struct lsa_TrustDomainInfoInfoEx *_info /* [in] [ref] */,
					   struct lsa_TrustDomainInfoAuthInfoInternal *_auth_info /* [in] [ref] */,
					   uint32_t _access_mask /* [in]  */,
					   struct policy_handle *_trustdom_handle /* [out] [ref] */,
					   NTSTATUS *result);

struct tevent_req *dcerpc_lsa_LookupNames3_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_LookupNames3 *r);
NTSTATUS dcerpc_lsa_LookupNames3_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_LookupNames3_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_LookupNames3 *r);
struct tevent_req *dcerpc_lsa_LookupNames3_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_handle /* [in] [ref] */,
						uint32_t _num_names /* [in] [range(0,1000)] */,
						struct lsa_String *_names /* [in] [size_is(num_names)] */,
						struct lsa_RefDomainList **_domains /* [out] [ref] */,
						struct lsa_TransSidArray3 *_sids /* [in,out] [ref] */,
						enum lsa_LookupNamesLevel _level /* [in]  */,
						uint32_t *_count /* [in,out] [ref] */,
						enum lsa_LookupOptions _lookup_options /* [in]  */,
						enum lsa_ClientRevision _client_revision /* [in]  */);
NTSTATUS dcerpc_lsa_LookupNames3_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_lsa_LookupNames3(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_handle /* [in] [ref] */,
				 uint32_t _num_names /* [in] [range(0,1000)] */,
				 struct lsa_String *_names /* [in] [size_is(num_names)] */,
				 struct lsa_RefDomainList **_domains /* [out] [ref] */,
				 struct lsa_TransSidArray3 *_sids /* [in,out] [ref] */,
				 enum lsa_LookupNamesLevel _level /* [in]  */,
				 uint32_t *_count /* [in,out] [ref] */,
				 enum lsa_LookupOptions _lookup_options /* [in]  */,
				 enum lsa_ClientRevision _client_revision /* [in]  */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_lsa_lsaRQueryForestTrustInformation_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_lsaRQueryForestTrustInformation *r);
NTSTATUS dcerpc_lsa_lsaRQueryForestTrustInformation_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_lsaRQueryForestTrustInformation_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_lsaRQueryForestTrustInformation *r);
struct tevent_req *dcerpc_lsa_lsaRQueryForestTrustInformation_send(TALLOC_CTX *mem_ctx,
								   struct tevent_context *ev,
								   struct dcerpc_binding_handle *h,
								   struct policy_handle *_handle /* [in] [ref] */,
								   struct lsa_String *_trusted_domain_name /* [in] [ref] */,
								   uint16_t _unknown /* [in]  */,
								   struct lsa_ForestTrustInformation **_forest_trust_info /* [out] [ref] */);
NTSTATUS dcerpc_lsa_lsaRQueryForestTrustInformation_recv(struct tevent_req *req,
							 TALLOC_CTX *mem_ctx,
							 NTSTATUS *result);
NTSTATUS dcerpc_lsa_lsaRQueryForestTrustInformation(struct dcerpc_binding_handle *h,
						    TALLOC_CTX *mem_ctx,
						    struct policy_handle *_handle /* [in] [ref] */,
						    struct lsa_String *_trusted_domain_name /* [in] [ref] */,
						    uint16_t _unknown /* [in]  */,
						    struct lsa_ForestTrustInformation **_forest_trust_info /* [out] [ref] */,
						    NTSTATUS *result);

struct tevent_req *dcerpc_lsa_lsaRSetForestTrustInformation_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_lsaRSetForestTrustInformation *r);
NTSTATUS dcerpc_lsa_lsaRSetForestTrustInformation_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_lsaRSetForestTrustInformation_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_lsaRSetForestTrustInformation *r);
struct tevent_req *dcerpc_lsa_lsaRSetForestTrustInformation_send(TALLOC_CTX *mem_ctx,
								 struct tevent_context *ev,
								 struct dcerpc_binding_handle *h,
								 struct policy_handle *_handle /* [in] [ref] */,
								 struct lsa_StringLarge *_trusted_domain_name /* [in] [ref] */,
								 uint16_t _highest_record_type /* [in]  */,
								 struct lsa_ForestTrustInformation *_forest_trust_info /* [in] [ref] */,
								 uint8_t _check_only /* [in]  */,
								 struct lsa_ForestTrustCollisionInfo **_collision_info /* [out] [ref] */);
NTSTATUS dcerpc_lsa_lsaRSetForestTrustInformation_recv(struct tevent_req *req,
						       TALLOC_CTX *mem_ctx,
						       NTSTATUS *result);
NTSTATUS dcerpc_lsa_lsaRSetForestTrustInformation(struct dcerpc_binding_handle *h,
						  TALLOC_CTX *mem_ctx,
						  struct policy_handle *_handle /* [in] [ref] */,
						  struct lsa_StringLarge *_trusted_domain_name /* [in] [ref] */,
						  uint16_t _highest_record_type /* [in]  */,
						  struct lsa_ForestTrustInformation *_forest_trust_info /* [in] [ref] */,
						  uint8_t _check_only /* [in]  */,
						  struct lsa_ForestTrustCollisionInfo **_collision_info /* [out] [ref] */,
						  NTSTATUS *result);

struct tevent_req *dcerpc_lsa_LookupSids3_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_LookupSids3 *r);
NTSTATUS dcerpc_lsa_LookupSids3_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_LookupSids3_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_LookupSids3 *r);
struct tevent_req *dcerpc_lsa_LookupSids3_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct lsa_SidArray *_sids /* [in] [ref] */,
					       struct lsa_RefDomainList **_domains /* [out] [ref] */,
					       struct lsa_TransNameArray2 *_names /* [in,out] [ref] */,
					       enum lsa_LookupNamesLevel _level /* [in]  */,
					       uint32_t *_count /* [in,out] [ref] */,
					       enum lsa_LookupOptions _lookup_options /* [in]  */,
					       enum lsa_ClientRevision _client_revision /* [in]  */);
NTSTATUS dcerpc_lsa_LookupSids3_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS dcerpc_lsa_LookupSids3(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct lsa_SidArray *_sids /* [in] [ref] */,
				struct lsa_RefDomainList **_domains /* [out] [ref] */,
				struct lsa_TransNameArray2 *_names /* [in,out] [ref] */,
				enum lsa_LookupNamesLevel _level /* [in]  */,
				uint32_t *_count /* [in,out] [ref] */,
				enum lsa_LookupOptions _lookup_options /* [in]  */,
				enum lsa_ClientRevision _client_revision /* [in]  */,
				NTSTATUS *result);

struct tevent_req *dcerpc_lsa_LookupNames4_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct lsa_LookupNames4 *r);
NTSTATUS dcerpc_lsa_LookupNames4_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_lsa_LookupNames4_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct lsa_LookupNames4 *r);
struct tevent_req *dcerpc_lsa_LookupNames4_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						uint32_t _num_names /* [in] [range(0,1000)] */,
						struct lsa_String *_names /* [in] [size_is(num_names)] */,
						struct lsa_RefDomainList **_domains /* [out] [ref] */,
						struct lsa_TransSidArray3 *_sids /* [in,out] [ref] */,
						enum lsa_LookupNamesLevel _level /* [in]  */,
						uint32_t *_count /* [in,out] [ref] */,
						enum lsa_LookupOptions _lookup_options /* [in]  */,
						enum lsa_ClientRevision _client_revision /* [in]  */);
NTSTATUS dcerpc_lsa_LookupNames4_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_lsa_LookupNames4(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 uint32_t _num_names /* [in] [range(0,1000)] */,
				 struct lsa_String *_names /* [in] [size_is(num_names)] */,
				 struct lsa_RefDomainList **_domains /* [out] [ref] */,
				 struct lsa_TransSidArray3 *_sids /* [in,out] [ref] */,
				 enum lsa_LookupNamesLevel _level /* [in]  */,
				 uint32_t *_count /* [in,out] [ref] */,
				 enum lsa_LookupOptions _lookup_options /* [in]  */,
				 enum lsa_ClientRevision _client_revision /* [in]  */,
				 NTSTATUS *result);

#endif /* _HEADER_RPC_lsarpc */
