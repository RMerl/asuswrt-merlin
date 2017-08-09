#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/samr.h"
#ifndef _HEADER_RPC_samr
#define _HEADER_RPC_samr

extern const struct ndr_interface_table ndr_table_samr;

struct tevent_req *dcerpc_samr_Connect_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_Connect *r);
NTSTATUS dcerpc_samr_Connect_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_Connect_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_Connect *r);
struct tevent_req *dcerpc_samr_Connect_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct dcerpc_binding_handle *h,
					    uint16_t *_system_name /* [in] [unique] */,
					    uint32_t _access_mask /* [in]  */,
					    struct policy_handle *_connect_handle /* [out] [ref] */);
NTSTATUS dcerpc_samr_Connect_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  NTSTATUS *result);
NTSTATUS dcerpc_samr_Connect(struct dcerpc_binding_handle *h,
			     TALLOC_CTX *mem_ctx,
			     uint16_t *_system_name /* [in] [unique] */,
			     uint32_t _access_mask /* [in]  */,
			     struct policy_handle *_connect_handle /* [out] [ref] */,
			     NTSTATUS *result);

struct tevent_req *dcerpc_samr_Close_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_Close *r);
NTSTATUS dcerpc_samr_Close_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_Close_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_Close *r);
struct tevent_req *dcerpc_samr_Close_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_samr_Close_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				NTSTATUS *result);
NTSTATUS dcerpc_samr_Close(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   struct policy_handle *_handle /* [in,out] [ref] */,
			   NTSTATUS *result);

struct tevent_req *dcerpc_samr_SetSecurity_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_SetSecurity *r);
NTSTATUS dcerpc_samr_SetSecurity_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_SetSecurity_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_SetSecurity *r);
struct tevent_req *dcerpc_samr_SetSecurity_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_handle /* [in] [ref] */,
						uint32_t _sec_info /* [in]  */,
						struct sec_desc_buf *_sdbuf /* [in] [ref] */);
NTSTATUS dcerpc_samr_SetSecurity_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_samr_SetSecurity(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_handle /* [in] [ref] */,
				 uint32_t _sec_info /* [in]  */,
				 struct sec_desc_buf *_sdbuf /* [in] [ref] */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_samr_QuerySecurity_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_QuerySecurity *r);
NTSTATUS dcerpc_samr_QuerySecurity_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_QuerySecurity_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_QuerySecurity *r);
struct tevent_req *dcerpc_samr_QuerySecurity_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct policy_handle *_handle /* [in] [ref] */,
						  uint32_t _sec_info /* [in]  */,
						  struct sec_desc_buf **_sdbuf /* [out] [ref] */);
NTSTATUS dcerpc_samr_QuerySecurity_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					NTSTATUS *result);
NTSTATUS dcerpc_samr_QuerySecurity(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *_handle /* [in] [ref] */,
				   uint32_t _sec_info /* [in]  */,
				   struct sec_desc_buf **_sdbuf /* [out] [ref] */,
				   NTSTATUS *result);

struct tevent_req *dcerpc_samr_Shutdown_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_Shutdown *r);
NTSTATUS dcerpc_samr_Shutdown_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_Shutdown_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_Shutdown *r);
struct tevent_req *dcerpc_samr_Shutdown_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     struct policy_handle *_connect_handle /* [in] [ref] */);
NTSTATUS dcerpc_samr_Shutdown_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_samr_Shutdown(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      struct policy_handle *_connect_handle /* [in] [ref] */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_samr_LookupDomain_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_LookupDomain *r);
NTSTATUS dcerpc_samr_LookupDomain_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_LookupDomain_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_LookupDomain *r);
struct tevent_req *dcerpc_samr_LookupDomain_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct policy_handle *_connect_handle /* [in] [ref] */,
						 struct lsa_String *_domain_name /* [in] [ref] */,
						 struct dom_sid2 **_sid /* [out] [ref] */);
NTSTATUS dcerpc_samr_LookupDomain_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_samr_LookupDomain(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *_connect_handle /* [in] [ref] */,
				  struct lsa_String *_domain_name /* [in] [ref] */,
				  struct dom_sid2 **_sid /* [out] [ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_samr_EnumDomains_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_EnumDomains *r);
NTSTATUS dcerpc_samr_EnumDomains_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_EnumDomains_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_EnumDomains *r);
struct tevent_req *dcerpc_samr_EnumDomains_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_connect_handle /* [in] [ref] */,
						uint32_t *_resume_handle /* [in,out] [ref] */,
						struct samr_SamArray **_sam /* [out] [ref] */,
						uint32_t _buf_size /* [in]  */,
						uint32_t *_num_entries /* [out] [ref] */);
NTSTATUS dcerpc_samr_EnumDomains_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_samr_EnumDomains(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_connect_handle /* [in] [ref] */,
				 uint32_t *_resume_handle /* [in,out] [ref] */,
				 struct samr_SamArray **_sam /* [out] [ref] */,
				 uint32_t _buf_size /* [in]  */,
				 uint32_t *_num_entries /* [out] [ref] */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_samr_OpenDomain_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_OpenDomain *r);
NTSTATUS dcerpc_samr_OpenDomain_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_OpenDomain_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_OpenDomain *r);
struct tevent_req *dcerpc_samr_OpenDomain_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_connect_handle /* [in] [ref] */,
					       uint32_t _access_mask /* [in]  */,
					       struct dom_sid2 *_sid /* [in] [ref] */,
					       struct policy_handle *_domain_handle /* [out] [ref] */);
NTSTATUS dcerpc_samr_OpenDomain_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS dcerpc_samr_OpenDomain(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_connect_handle /* [in] [ref] */,
				uint32_t _access_mask /* [in]  */,
				struct dom_sid2 *_sid /* [in] [ref] */,
				struct policy_handle *_domain_handle /* [out] [ref] */,
				NTSTATUS *result);

struct tevent_req *dcerpc_samr_QueryDomainInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_QueryDomainInfo *r);
NTSTATUS dcerpc_samr_QueryDomainInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_QueryDomainInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_QueryDomainInfo *r);
struct tevent_req *dcerpc_samr_QueryDomainInfo_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_domain_handle /* [in] [ref] */,
						    enum samr_DomainInfoClass _level /* [in]  */,
						    union samr_DomainInfo **_info /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_samr_QueryDomainInfo_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);
NTSTATUS dcerpc_samr_QueryDomainInfo(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_domain_handle /* [in] [ref] */,
				     enum samr_DomainInfoClass _level /* [in]  */,
				     union samr_DomainInfo **_info /* [out] [ref,switch_is(level)] */,
				     NTSTATUS *result);

struct tevent_req *dcerpc_samr_SetDomainInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_SetDomainInfo *r);
NTSTATUS dcerpc_samr_SetDomainInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_SetDomainInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_SetDomainInfo *r);
struct tevent_req *dcerpc_samr_SetDomainInfo_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct policy_handle *_domain_handle /* [in] [ref] */,
						  enum samr_DomainInfoClass _level /* [in]  */,
						  union samr_DomainInfo *_info /* [in] [switch_is(level),ref] */);
NTSTATUS dcerpc_samr_SetDomainInfo_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					NTSTATUS *result);
NTSTATUS dcerpc_samr_SetDomainInfo(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *_domain_handle /* [in] [ref] */,
				   enum samr_DomainInfoClass _level /* [in]  */,
				   union samr_DomainInfo *_info /* [in] [switch_is(level),ref] */,
				   NTSTATUS *result);

struct tevent_req *dcerpc_samr_CreateDomainGroup_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_CreateDomainGroup *r);
NTSTATUS dcerpc_samr_CreateDomainGroup_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_CreateDomainGroup_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_CreateDomainGroup *r);
struct tevent_req *dcerpc_samr_CreateDomainGroup_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_domain_handle /* [in] [ref] */,
						      struct lsa_String *_name /* [in] [ref] */,
						      uint32_t _access_mask /* [in]  */,
						      struct policy_handle *_group_handle /* [out] [ref] */,
						      uint32_t *_rid /* [out] [ref] */);
NTSTATUS dcerpc_samr_CreateDomainGroup_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_samr_CreateDomainGroup(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_domain_handle /* [in] [ref] */,
				       struct lsa_String *_name /* [in] [ref] */,
				       uint32_t _access_mask /* [in]  */,
				       struct policy_handle *_group_handle /* [out] [ref] */,
				       uint32_t *_rid /* [out] [ref] */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_samr_EnumDomainGroups_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_EnumDomainGroups *r);
NTSTATUS dcerpc_samr_EnumDomainGroups_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_EnumDomainGroups_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_EnumDomainGroups *r);
struct tevent_req *dcerpc_samr_EnumDomainGroups_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_domain_handle /* [in] [ref] */,
						     uint32_t *_resume_handle /* [in,out] [ref] */,
						     struct samr_SamArray **_sam /* [out] [ref] */,
						     uint32_t _max_size /* [in]  */,
						     uint32_t *_num_entries /* [out] [ref] */);
NTSTATUS dcerpc_samr_EnumDomainGroups_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   NTSTATUS *result);
NTSTATUS dcerpc_samr_EnumDomainGroups(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_domain_handle /* [in] [ref] */,
				      uint32_t *_resume_handle /* [in,out] [ref] */,
				      struct samr_SamArray **_sam /* [out] [ref] */,
				      uint32_t _max_size /* [in]  */,
				      uint32_t *_num_entries /* [out] [ref] */,
				      NTSTATUS *result);

struct tevent_req *dcerpc_samr_CreateUser_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_CreateUser *r);
NTSTATUS dcerpc_samr_CreateUser_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_CreateUser_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_CreateUser *r);
struct tevent_req *dcerpc_samr_CreateUser_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_domain_handle /* [in] [ref] */,
					       struct lsa_String *_account_name /* [in] [ref] */,
					       uint32_t _access_mask /* [in]  */,
					       struct policy_handle *_user_handle /* [out] [ref] */,
					       uint32_t *_rid /* [out] [ref] */);
NTSTATUS dcerpc_samr_CreateUser_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS dcerpc_samr_CreateUser(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_domain_handle /* [in] [ref] */,
				struct lsa_String *_account_name /* [in] [ref] */,
				uint32_t _access_mask /* [in]  */,
				struct policy_handle *_user_handle /* [out] [ref] */,
				uint32_t *_rid /* [out] [ref] */,
				NTSTATUS *result);

struct tevent_req *dcerpc_samr_EnumDomainUsers_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_EnumDomainUsers *r);
NTSTATUS dcerpc_samr_EnumDomainUsers_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_EnumDomainUsers_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_EnumDomainUsers *r);
struct tevent_req *dcerpc_samr_EnumDomainUsers_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_domain_handle /* [in] [ref] */,
						    uint32_t *_resume_handle /* [in,out] [ref] */,
						    uint32_t _acct_flags /* [in]  */,
						    struct samr_SamArray **_sam /* [out] [ref] */,
						    uint32_t _max_size /* [in]  */,
						    uint32_t *_num_entries /* [out] [ref] */);
NTSTATUS dcerpc_samr_EnumDomainUsers_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);
NTSTATUS dcerpc_samr_EnumDomainUsers(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_domain_handle /* [in] [ref] */,
				     uint32_t *_resume_handle /* [in,out] [ref] */,
				     uint32_t _acct_flags /* [in]  */,
				     struct samr_SamArray **_sam /* [out] [ref] */,
				     uint32_t _max_size /* [in]  */,
				     uint32_t *_num_entries /* [out] [ref] */,
				     NTSTATUS *result);

struct tevent_req *dcerpc_samr_CreateDomAlias_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_CreateDomAlias *r);
NTSTATUS dcerpc_samr_CreateDomAlias_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_CreateDomAlias_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_CreateDomAlias *r);
struct tevent_req *dcerpc_samr_CreateDomAlias_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct policy_handle *_domain_handle /* [in] [ref] */,
						   struct lsa_String *_alias_name /* [in] [ref] */,
						   uint32_t _access_mask /* [in]  */,
						   struct policy_handle *_alias_handle /* [out] [ref] */,
						   uint32_t *_rid /* [out] [ref] */);
NTSTATUS dcerpc_samr_CreateDomAlias_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS dcerpc_samr_CreateDomAlias(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *_domain_handle /* [in] [ref] */,
				    struct lsa_String *_alias_name /* [in] [ref] */,
				    uint32_t _access_mask /* [in]  */,
				    struct policy_handle *_alias_handle /* [out] [ref] */,
				    uint32_t *_rid /* [out] [ref] */,
				    NTSTATUS *result);

struct tevent_req *dcerpc_samr_EnumDomainAliases_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_EnumDomainAliases *r);
NTSTATUS dcerpc_samr_EnumDomainAliases_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_EnumDomainAliases_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_EnumDomainAliases *r);
struct tevent_req *dcerpc_samr_EnumDomainAliases_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_domain_handle /* [in] [ref] */,
						      uint32_t *_resume_handle /* [in,out] [ref] */,
						      struct samr_SamArray **_sam /* [out] [ref] */,
						      uint32_t _max_size /* [in]  */,
						      uint32_t *_num_entries /* [out] [ref] */);
NTSTATUS dcerpc_samr_EnumDomainAliases_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_samr_EnumDomainAliases(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_domain_handle /* [in] [ref] */,
				       uint32_t *_resume_handle /* [in,out] [ref] */,
				       struct samr_SamArray **_sam /* [out] [ref] */,
				       uint32_t _max_size /* [in]  */,
				       uint32_t *_num_entries /* [out] [ref] */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_samr_GetAliasMembership_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_GetAliasMembership *r);
NTSTATUS dcerpc_samr_GetAliasMembership_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_GetAliasMembership_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_GetAliasMembership *r);
struct tevent_req *dcerpc_samr_GetAliasMembership_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct policy_handle *_domain_handle /* [in] [ref] */,
						       struct lsa_SidArray *_sids /* [in] [ref] */,
						       struct samr_Ids *_rids /* [out] [ref] */);
NTSTATUS dcerpc_samr_GetAliasMembership_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS dcerpc_samr_GetAliasMembership(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *_domain_handle /* [in] [ref] */,
					struct lsa_SidArray *_sids /* [in] [ref] */,
					struct samr_Ids *_rids /* [out] [ref] */,
					NTSTATUS *result);

struct tevent_req *dcerpc_samr_LookupNames_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_LookupNames *r);
NTSTATUS dcerpc_samr_LookupNames_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_LookupNames_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_LookupNames *r);
struct tevent_req *dcerpc_samr_LookupNames_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_domain_handle /* [in] [ref] */,
						uint32_t _num_names /* [in] [range(0,1000)] */,
						struct lsa_String *_names /* [in] [length_is(num_names),size_is(1000)] */,
						struct samr_Ids *_rids /* [out] [ref] */,
						struct samr_Ids *_types /* [out] [ref] */);
NTSTATUS dcerpc_samr_LookupNames_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_samr_LookupNames(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_domain_handle /* [in] [ref] */,
				 uint32_t _num_names /* [in] [range(0,1000)] */,
				 struct lsa_String *_names /* [in] [length_is(num_names),size_is(1000)] */,
				 struct samr_Ids *_rids /* [out] [ref] */,
				 struct samr_Ids *_types /* [out] [ref] */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_samr_LookupRids_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_LookupRids *r);
NTSTATUS dcerpc_samr_LookupRids_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_LookupRids_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_LookupRids *r);
struct tevent_req *dcerpc_samr_LookupRids_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_domain_handle /* [in] [ref] */,
					       uint32_t _num_rids /* [in] [range(0,1000)] */,
					       uint32_t *_rids /* [in] [length_is(num_rids),size_is(1000)] */,
					       struct lsa_Strings *_names /* [out] [ref] */,
					       struct samr_Ids *_types /* [out] [ref] */);
NTSTATUS dcerpc_samr_LookupRids_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS dcerpc_samr_LookupRids(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_domain_handle /* [in] [ref] */,
				uint32_t _num_rids /* [in] [range(0,1000)] */,
				uint32_t *_rids /* [in] [length_is(num_rids),size_is(1000)] */,
				struct lsa_Strings *_names /* [out] [ref] */,
				struct samr_Ids *_types /* [out] [ref] */,
				NTSTATUS *result);

struct tevent_req *dcerpc_samr_OpenGroup_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_OpenGroup *r);
NTSTATUS dcerpc_samr_OpenGroup_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_OpenGroup_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_OpenGroup *r);
struct tevent_req *dcerpc_samr_OpenGroup_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct policy_handle *_domain_handle /* [in] [ref] */,
					      uint32_t _access_mask /* [in]  */,
					      uint32_t _rid /* [in]  */,
					      struct policy_handle *_group_handle /* [out] [ref] */);
NTSTATUS dcerpc_samr_OpenGroup_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    NTSTATUS *result);
NTSTATUS dcerpc_samr_OpenGroup(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *_domain_handle /* [in] [ref] */,
			       uint32_t _access_mask /* [in]  */,
			       uint32_t _rid /* [in]  */,
			       struct policy_handle *_group_handle /* [out] [ref] */,
			       NTSTATUS *result);

struct tevent_req *dcerpc_samr_QueryGroupInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_QueryGroupInfo *r);
NTSTATUS dcerpc_samr_QueryGroupInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_QueryGroupInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_QueryGroupInfo *r);
struct tevent_req *dcerpc_samr_QueryGroupInfo_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct policy_handle *_group_handle /* [in] [ref] */,
						   enum samr_GroupInfoEnum _level /* [in]  */,
						   union samr_GroupInfo **_info /* [out] [switch_is(level),ref] */);
NTSTATUS dcerpc_samr_QueryGroupInfo_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS dcerpc_samr_QueryGroupInfo(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *_group_handle /* [in] [ref] */,
				    enum samr_GroupInfoEnum _level /* [in]  */,
				    union samr_GroupInfo **_info /* [out] [switch_is(level),ref] */,
				    NTSTATUS *result);

struct tevent_req *dcerpc_samr_SetGroupInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_SetGroupInfo *r);
NTSTATUS dcerpc_samr_SetGroupInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_SetGroupInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_SetGroupInfo *r);
struct tevent_req *dcerpc_samr_SetGroupInfo_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct policy_handle *_group_handle /* [in] [ref] */,
						 enum samr_GroupInfoEnum _level /* [in]  */,
						 union samr_GroupInfo *_info /* [in] [switch_is(level),ref] */);
NTSTATUS dcerpc_samr_SetGroupInfo_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_samr_SetGroupInfo(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *_group_handle /* [in] [ref] */,
				  enum samr_GroupInfoEnum _level /* [in]  */,
				  union samr_GroupInfo *_info /* [in] [switch_is(level),ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_samr_AddGroupMember_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_AddGroupMember *r);
NTSTATUS dcerpc_samr_AddGroupMember_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_AddGroupMember_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_AddGroupMember *r);
struct tevent_req *dcerpc_samr_AddGroupMember_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct policy_handle *_group_handle /* [in] [ref] */,
						   uint32_t _rid /* [in]  */,
						   uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_samr_AddGroupMember_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS dcerpc_samr_AddGroupMember(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *_group_handle /* [in] [ref] */,
				    uint32_t _rid /* [in]  */,
				    uint32_t _flags /* [in]  */,
				    NTSTATUS *result);

struct tevent_req *dcerpc_samr_DeleteDomainGroup_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_DeleteDomainGroup *r);
NTSTATUS dcerpc_samr_DeleteDomainGroup_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_DeleteDomainGroup_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_DeleteDomainGroup *r);
struct tevent_req *dcerpc_samr_DeleteDomainGroup_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_group_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_samr_DeleteDomainGroup_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_samr_DeleteDomainGroup(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_group_handle /* [in,out] [ref] */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_samr_DeleteGroupMember_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_DeleteGroupMember *r);
NTSTATUS dcerpc_samr_DeleteGroupMember_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_DeleteGroupMember_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_DeleteGroupMember *r);
struct tevent_req *dcerpc_samr_DeleteGroupMember_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_group_handle /* [in] [ref] */,
						      uint32_t _rid /* [in]  */);
NTSTATUS dcerpc_samr_DeleteGroupMember_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_samr_DeleteGroupMember(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_group_handle /* [in] [ref] */,
				       uint32_t _rid /* [in]  */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_samr_QueryGroupMember_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_QueryGroupMember *r);
NTSTATUS dcerpc_samr_QueryGroupMember_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_QueryGroupMember_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_QueryGroupMember *r);
struct tevent_req *dcerpc_samr_QueryGroupMember_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_group_handle /* [in] [ref] */,
						     struct samr_RidAttrArray **_rids /* [out] [ref] */);
NTSTATUS dcerpc_samr_QueryGroupMember_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   NTSTATUS *result);
NTSTATUS dcerpc_samr_QueryGroupMember(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_group_handle /* [in] [ref] */,
				      struct samr_RidAttrArray **_rids /* [out] [ref] */,
				      NTSTATUS *result);

struct tevent_req *dcerpc_samr_SetMemberAttributesOfGroup_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_SetMemberAttributesOfGroup *r);
NTSTATUS dcerpc_samr_SetMemberAttributesOfGroup_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_SetMemberAttributesOfGroup_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_SetMemberAttributesOfGroup *r);
struct tevent_req *dcerpc_samr_SetMemberAttributesOfGroup_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct dcerpc_binding_handle *h,
							       struct policy_handle *_group_handle /* [in] [ref] */,
							       uint32_t _unknown1 /* [in]  */,
							       uint32_t _unknown2 /* [in]  */);
NTSTATUS dcerpc_samr_SetMemberAttributesOfGroup_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     NTSTATUS *result);
NTSTATUS dcerpc_samr_SetMemberAttributesOfGroup(struct dcerpc_binding_handle *h,
						TALLOC_CTX *mem_ctx,
						struct policy_handle *_group_handle /* [in] [ref] */,
						uint32_t _unknown1 /* [in]  */,
						uint32_t _unknown2 /* [in]  */,
						NTSTATUS *result);

struct tevent_req *dcerpc_samr_OpenAlias_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_OpenAlias *r);
NTSTATUS dcerpc_samr_OpenAlias_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_OpenAlias_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_OpenAlias *r);
struct tevent_req *dcerpc_samr_OpenAlias_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct policy_handle *_domain_handle /* [in] [ref] */,
					      uint32_t _access_mask /* [in]  */,
					      uint32_t _rid /* [in]  */,
					      struct policy_handle *_alias_handle /* [out] [ref] */);
NTSTATUS dcerpc_samr_OpenAlias_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    NTSTATUS *result);
NTSTATUS dcerpc_samr_OpenAlias(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *_domain_handle /* [in] [ref] */,
			       uint32_t _access_mask /* [in]  */,
			       uint32_t _rid /* [in]  */,
			       struct policy_handle *_alias_handle /* [out] [ref] */,
			       NTSTATUS *result);

struct tevent_req *dcerpc_samr_QueryAliasInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_QueryAliasInfo *r);
NTSTATUS dcerpc_samr_QueryAliasInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_QueryAliasInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_QueryAliasInfo *r);
struct tevent_req *dcerpc_samr_QueryAliasInfo_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct policy_handle *_alias_handle /* [in] [ref] */,
						   enum samr_AliasInfoEnum _level /* [in]  */,
						   union samr_AliasInfo **_info /* [out] [switch_is(level),ref] */);
NTSTATUS dcerpc_samr_QueryAliasInfo_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS dcerpc_samr_QueryAliasInfo(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *_alias_handle /* [in] [ref] */,
				    enum samr_AliasInfoEnum _level /* [in]  */,
				    union samr_AliasInfo **_info /* [out] [switch_is(level),ref] */,
				    NTSTATUS *result);

struct tevent_req *dcerpc_samr_SetAliasInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_SetAliasInfo *r);
NTSTATUS dcerpc_samr_SetAliasInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_SetAliasInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_SetAliasInfo *r);
struct tevent_req *dcerpc_samr_SetAliasInfo_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct policy_handle *_alias_handle /* [in] [ref] */,
						 enum samr_AliasInfoEnum _level /* [in]  */,
						 union samr_AliasInfo *_info /* [in] [switch_is(level),ref] */);
NTSTATUS dcerpc_samr_SetAliasInfo_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_samr_SetAliasInfo(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *_alias_handle /* [in] [ref] */,
				  enum samr_AliasInfoEnum _level /* [in]  */,
				  union samr_AliasInfo *_info /* [in] [switch_is(level),ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_samr_DeleteDomAlias_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_DeleteDomAlias *r);
NTSTATUS dcerpc_samr_DeleteDomAlias_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_DeleteDomAlias_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_DeleteDomAlias *r);
struct tevent_req *dcerpc_samr_DeleteDomAlias_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct policy_handle *_alias_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_samr_DeleteDomAlias_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS dcerpc_samr_DeleteDomAlias(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *_alias_handle /* [in,out] [ref] */,
				    NTSTATUS *result);

struct tevent_req *dcerpc_samr_AddAliasMember_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_AddAliasMember *r);
NTSTATUS dcerpc_samr_AddAliasMember_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_AddAliasMember_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_AddAliasMember *r);
struct tevent_req *dcerpc_samr_AddAliasMember_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct policy_handle *_alias_handle /* [in] [ref] */,
						   struct dom_sid2 *_sid /* [in] [ref] */);
NTSTATUS dcerpc_samr_AddAliasMember_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS dcerpc_samr_AddAliasMember(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *_alias_handle /* [in] [ref] */,
				    struct dom_sid2 *_sid /* [in] [ref] */,
				    NTSTATUS *result);

struct tevent_req *dcerpc_samr_DeleteAliasMember_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_DeleteAliasMember *r);
NTSTATUS dcerpc_samr_DeleteAliasMember_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_DeleteAliasMember_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_DeleteAliasMember *r);
struct tevent_req *dcerpc_samr_DeleteAliasMember_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_alias_handle /* [in] [ref] */,
						      struct dom_sid2 *_sid /* [in] [ref] */);
NTSTATUS dcerpc_samr_DeleteAliasMember_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_samr_DeleteAliasMember(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_alias_handle /* [in] [ref] */,
				       struct dom_sid2 *_sid /* [in] [ref] */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_samr_GetMembersInAlias_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_GetMembersInAlias *r);
NTSTATUS dcerpc_samr_GetMembersInAlias_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_GetMembersInAlias_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_GetMembersInAlias *r);
struct tevent_req *dcerpc_samr_GetMembersInAlias_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_alias_handle /* [in] [ref] */,
						      struct lsa_SidArray *_sids /* [out] [ref] */);
NTSTATUS dcerpc_samr_GetMembersInAlias_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_samr_GetMembersInAlias(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_alias_handle /* [in] [ref] */,
				       struct lsa_SidArray *_sids /* [out] [ref] */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_samr_OpenUser_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_OpenUser *r);
NTSTATUS dcerpc_samr_OpenUser_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_OpenUser_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_OpenUser *r);
struct tevent_req *dcerpc_samr_OpenUser_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     struct policy_handle *_domain_handle /* [in] [ref] */,
					     uint32_t _access_mask /* [in]  */,
					     uint32_t _rid /* [in]  */,
					     struct policy_handle *_user_handle /* [out] [ref] */);
NTSTATUS dcerpc_samr_OpenUser_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_samr_OpenUser(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      struct policy_handle *_domain_handle /* [in] [ref] */,
			      uint32_t _access_mask /* [in]  */,
			      uint32_t _rid /* [in]  */,
			      struct policy_handle *_user_handle /* [out] [ref] */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_samr_DeleteUser_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_DeleteUser *r);
NTSTATUS dcerpc_samr_DeleteUser_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_DeleteUser_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_DeleteUser *r);
struct tevent_req *dcerpc_samr_DeleteUser_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_user_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_samr_DeleteUser_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS dcerpc_samr_DeleteUser(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_user_handle /* [in,out] [ref] */,
				NTSTATUS *result);

struct tevent_req *dcerpc_samr_QueryUserInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_QueryUserInfo *r);
NTSTATUS dcerpc_samr_QueryUserInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_QueryUserInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_QueryUserInfo *r);
struct tevent_req *dcerpc_samr_QueryUserInfo_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct policy_handle *_user_handle /* [in] [ref] */,
						  enum samr_UserInfoLevel _level /* [in]  */,
						  union samr_UserInfo **_info /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_samr_QueryUserInfo_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					NTSTATUS *result);
NTSTATUS dcerpc_samr_QueryUserInfo(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *_user_handle /* [in] [ref] */,
				   enum samr_UserInfoLevel _level /* [in]  */,
				   union samr_UserInfo **_info /* [out] [ref,switch_is(level)] */,
				   NTSTATUS *result);

struct tevent_req *dcerpc_samr_SetUserInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_SetUserInfo *r);
NTSTATUS dcerpc_samr_SetUserInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_SetUserInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_SetUserInfo *r);
struct tevent_req *dcerpc_samr_SetUserInfo_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_user_handle /* [in] [ref] */,
						enum samr_UserInfoLevel _level /* [in]  */,
						union samr_UserInfo *_info /* [in] [ref,switch_is(level)] */);
NTSTATUS dcerpc_samr_SetUserInfo_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_samr_SetUserInfo(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_user_handle /* [in] [ref] */,
				 enum samr_UserInfoLevel _level /* [in]  */,
				 union samr_UserInfo *_info /* [in] [ref,switch_is(level)] */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_samr_ChangePasswordUser_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_ChangePasswordUser *r);
NTSTATUS dcerpc_samr_ChangePasswordUser_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_ChangePasswordUser_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_ChangePasswordUser *r);
struct tevent_req *dcerpc_samr_ChangePasswordUser_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct policy_handle *_user_handle /* [in] [ref] */,
						       uint8_t _lm_present /* [in]  */,
						       struct samr_Password *_old_lm_crypted /* [in] [unique] */,
						       struct samr_Password *_new_lm_crypted /* [in] [unique] */,
						       uint8_t _nt_present /* [in]  */,
						       struct samr_Password *_old_nt_crypted /* [in] [unique] */,
						       struct samr_Password *_new_nt_crypted /* [in] [unique] */,
						       uint8_t _cross1_present /* [in]  */,
						       struct samr_Password *_nt_cross /* [in] [unique] */,
						       uint8_t _cross2_present /* [in]  */,
						       struct samr_Password *_lm_cross /* [in] [unique] */);
NTSTATUS dcerpc_samr_ChangePasswordUser_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS dcerpc_samr_ChangePasswordUser(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *_user_handle /* [in] [ref] */,
					uint8_t _lm_present /* [in]  */,
					struct samr_Password *_old_lm_crypted /* [in] [unique] */,
					struct samr_Password *_new_lm_crypted /* [in] [unique] */,
					uint8_t _nt_present /* [in]  */,
					struct samr_Password *_old_nt_crypted /* [in] [unique] */,
					struct samr_Password *_new_nt_crypted /* [in] [unique] */,
					uint8_t _cross1_present /* [in]  */,
					struct samr_Password *_nt_cross /* [in] [unique] */,
					uint8_t _cross2_present /* [in]  */,
					struct samr_Password *_lm_cross /* [in] [unique] */,
					NTSTATUS *result);

struct tevent_req *dcerpc_samr_GetGroupsForUser_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_GetGroupsForUser *r);
NTSTATUS dcerpc_samr_GetGroupsForUser_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_GetGroupsForUser_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_GetGroupsForUser *r);
struct tevent_req *dcerpc_samr_GetGroupsForUser_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_user_handle /* [in] [ref] */,
						     struct samr_RidWithAttributeArray **_rids /* [out] [ref] */);
NTSTATUS dcerpc_samr_GetGroupsForUser_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   NTSTATUS *result);
NTSTATUS dcerpc_samr_GetGroupsForUser(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_user_handle /* [in] [ref] */,
				      struct samr_RidWithAttributeArray **_rids /* [out] [ref] */,
				      NTSTATUS *result);

struct tevent_req *dcerpc_samr_QueryDisplayInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_QueryDisplayInfo *r);
NTSTATUS dcerpc_samr_QueryDisplayInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_QueryDisplayInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_QueryDisplayInfo *r);
struct tevent_req *dcerpc_samr_QueryDisplayInfo_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_domain_handle /* [in] [ref] */,
						     uint16_t _level /* [in]  */,
						     uint32_t _start_idx /* [in]  */,
						     uint32_t _max_entries /* [in]  */,
						     uint32_t _buf_size /* [in]  */,
						     uint32_t *_total_size /* [out] [ref] */,
						     uint32_t *_returned_size /* [out] [ref] */,
						     union samr_DispInfo *_info /* [out] [switch_is(level),ref] */);
NTSTATUS dcerpc_samr_QueryDisplayInfo_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   NTSTATUS *result);
NTSTATUS dcerpc_samr_QueryDisplayInfo(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_domain_handle /* [in] [ref] */,
				      uint16_t _level /* [in]  */,
				      uint32_t _start_idx /* [in]  */,
				      uint32_t _max_entries /* [in]  */,
				      uint32_t _buf_size /* [in]  */,
				      uint32_t *_total_size /* [out] [ref] */,
				      uint32_t *_returned_size /* [out] [ref] */,
				      union samr_DispInfo *_info /* [out] [switch_is(level),ref] */,
				      NTSTATUS *result);

struct tevent_req *dcerpc_samr_GetDisplayEnumerationIndex_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_GetDisplayEnumerationIndex *r);
NTSTATUS dcerpc_samr_GetDisplayEnumerationIndex_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_GetDisplayEnumerationIndex_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_GetDisplayEnumerationIndex *r);
struct tevent_req *dcerpc_samr_GetDisplayEnumerationIndex_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct dcerpc_binding_handle *h,
							       struct policy_handle *_domain_handle /* [in] [ref] */,
							       uint16_t _level /* [in]  */,
							       struct lsa_String *_name /* [in] [ref] */,
							       uint32_t *_idx /* [out] [ref] */);
NTSTATUS dcerpc_samr_GetDisplayEnumerationIndex_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     NTSTATUS *result);
NTSTATUS dcerpc_samr_GetDisplayEnumerationIndex(struct dcerpc_binding_handle *h,
						TALLOC_CTX *mem_ctx,
						struct policy_handle *_domain_handle /* [in] [ref] */,
						uint16_t _level /* [in]  */,
						struct lsa_String *_name /* [in] [ref] */,
						uint32_t *_idx /* [out] [ref] */,
						NTSTATUS *result);

struct tevent_req *dcerpc_samr_TestPrivateFunctionsDomain_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_TestPrivateFunctionsDomain *r);
NTSTATUS dcerpc_samr_TestPrivateFunctionsDomain_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_TestPrivateFunctionsDomain_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_TestPrivateFunctionsDomain *r);
struct tevent_req *dcerpc_samr_TestPrivateFunctionsDomain_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct dcerpc_binding_handle *h,
							       struct policy_handle *_domain_handle /* [in] [ref] */);
NTSTATUS dcerpc_samr_TestPrivateFunctionsDomain_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     NTSTATUS *result);
NTSTATUS dcerpc_samr_TestPrivateFunctionsDomain(struct dcerpc_binding_handle *h,
						TALLOC_CTX *mem_ctx,
						struct policy_handle *_domain_handle /* [in] [ref] */,
						NTSTATUS *result);

struct tevent_req *dcerpc_samr_TestPrivateFunctionsUser_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_TestPrivateFunctionsUser *r);
NTSTATUS dcerpc_samr_TestPrivateFunctionsUser_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_TestPrivateFunctionsUser_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_TestPrivateFunctionsUser *r);
struct tevent_req *dcerpc_samr_TestPrivateFunctionsUser_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     struct policy_handle *_user_handle /* [in] [ref] */);
NTSTATUS dcerpc_samr_TestPrivateFunctionsUser_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   NTSTATUS *result);
NTSTATUS dcerpc_samr_TestPrivateFunctionsUser(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      struct policy_handle *_user_handle /* [in] [ref] */,
					      NTSTATUS *result);

struct tevent_req *dcerpc_samr_GetUserPwInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_GetUserPwInfo *r);
NTSTATUS dcerpc_samr_GetUserPwInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_GetUserPwInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_GetUserPwInfo *r);
struct tevent_req *dcerpc_samr_GetUserPwInfo_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct policy_handle *_user_handle /* [in] [ref] */,
						  struct samr_PwInfo *_info /* [out] [ref] */);
NTSTATUS dcerpc_samr_GetUserPwInfo_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					NTSTATUS *result);
NTSTATUS dcerpc_samr_GetUserPwInfo(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *_user_handle /* [in] [ref] */,
				   struct samr_PwInfo *_info /* [out] [ref] */,
				   NTSTATUS *result);

struct tevent_req *dcerpc_samr_RemoveMemberFromForeignDomain_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_RemoveMemberFromForeignDomain *r);
NTSTATUS dcerpc_samr_RemoveMemberFromForeignDomain_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_RemoveMemberFromForeignDomain_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_RemoveMemberFromForeignDomain *r);
struct tevent_req *dcerpc_samr_RemoveMemberFromForeignDomain_send(TALLOC_CTX *mem_ctx,
								  struct tevent_context *ev,
								  struct dcerpc_binding_handle *h,
								  struct policy_handle *_domain_handle /* [in] [ref] */,
								  struct dom_sid2 *_sid /* [in] [ref] */);
NTSTATUS dcerpc_samr_RemoveMemberFromForeignDomain_recv(struct tevent_req *req,
							TALLOC_CTX *mem_ctx,
							NTSTATUS *result);
NTSTATUS dcerpc_samr_RemoveMemberFromForeignDomain(struct dcerpc_binding_handle *h,
						   TALLOC_CTX *mem_ctx,
						   struct policy_handle *_domain_handle /* [in] [ref] */,
						   struct dom_sid2 *_sid /* [in] [ref] */,
						   NTSTATUS *result);

struct tevent_req *dcerpc_samr_QueryDomainInfo2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_QueryDomainInfo2 *r);
NTSTATUS dcerpc_samr_QueryDomainInfo2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_QueryDomainInfo2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_QueryDomainInfo2 *r);
struct tevent_req *dcerpc_samr_QueryDomainInfo2_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_domain_handle /* [in] [ref] */,
						     enum samr_DomainInfoClass _level /* [in]  */,
						     union samr_DomainInfo **_info /* [out] [switch_is(level),ref] */);
NTSTATUS dcerpc_samr_QueryDomainInfo2_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   NTSTATUS *result);
NTSTATUS dcerpc_samr_QueryDomainInfo2(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_domain_handle /* [in] [ref] */,
				      enum samr_DomainInfoClass _level /* [in]  */,
				      union samr_DomainInfo **_info /* [out] [switch_is(level),ref] */,
				      NTSTATUS *result);

struct tevent_req *dcerpc_samr_QueryUserInfo2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_QueryUserInfo2 *r);
NTSTATUS dcerpc_samr_QueryUserInfo2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_QueryUserInfo2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_QueryUserInfo2 *r);
struct tevent_req *dcerpc_samr_QueryUserInfo2_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct policy_handle *_user_handle /* [in] [ref] */,
						   enum samr_UserInfoLevel _level /* [in]  */,
						   union samr_UserInfo **_info /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_samr_QueryUserInfo2_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS dcerpc_samr_QueryUserInfo2(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *_user_handle /* [in] [ref] */,
				    enum samr_UserInfoLevel _level /* [in]  */,
				    union samr_UserInfo **_info /* [out] [ref,switch_is(level)] */,
				    NTSTATUS *result);

struct tevent_req *dcerpc_samr_QueryDisplayInfo2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_QueryDisplayInfo2 *r);
NTSTATUS dcerpc_samr_QueryDisplayInfo2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_QueryDisplayInfo2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_QueryDisplayInfo2 *r);
struct tevent_req *dcerpc_samr_QueryDisplayInfo2_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_domain_handle /* [in] [ref] */,
						      uint16_t _level /* [in]  */,
						      uint32_t _start_idx /* [in]  */,
						      uint32_t _max_entries /* [in]  */,
						      uint32_t _buf_size /* [in]  */,
						      uint32_t *_total_size /* [out] [ref] */,
						      uint32_t *_returned_size /* [out] [ref] */,
						      union samr_DispInfo *_info /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_samr_QueryDisplayInfo2_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_samr_QueryDisplayInfo2(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_domain_handle /* [in] [ref] */,
				       uint16_t _level /* [in]  */,
				       uint32_t _start_idx /* [in]  */,
				       uint32_t _max_entries /* [in]  */,
				       uint32_t _buf_size /* [in]  */,
				       uint32_t *_total_size /* [out] [ref] */,
				       uint32_t *_returned_size /* [out] [ref] */,
				       union samr_DispInfo *_info /* [out] [ref,switch_is(level)] */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_samr_GetDisplayEnumerationIndex2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_GetDisplayEnumerationIndex2 *r);
NTSTATUS dcerpc_samr_GetDisplayEnumerationIndex2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_GetDisplayEnumerationIndex2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_GetDisplayEnumerationIndex2 *r);
struct tevent_req *dcerpc_samr_GetDisplayEnumerationIndex2_send(TALLOC_CTX *mem_ctx,
								struct tevent_context *ev,
								struct dcerpc_binding_handle *h,
								struct policy_handle *_domain_handle /* [in] [ref] */,
								uint16_t _level /* [in]  */,
								struct lsa_String *_name /* [in] [ref] */,
								uint32_t *_idx /* [out] [ref] */);
NTSTATUS dcerpc_samr_GetDisplayEnumerationIndex2_recv(struct tevent_req *req,
						      TALLOC_CTX *mem_ctx,
						      NTSTATUS *result);
NTSTATUS dcerpc_samr_GetDisplayEnumerationIndex2(struct dcerpc_binding_handle *h,
						 TALLOC_CTX *mem_ctx,
						 struct policy_handle *_domain_handle /* [in] [ref] */,
						 uint16_t _level /* [in]  */,
						 struct lsa_String *_name /* [in] [ref] */,
						 uint32_t *_idx /* [out] [ref] */,
						 NTSTATUS *result);

struct tevent_req *dcerpc_samr_CreateUser2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_CreateUser2 *r);
NTSTATUS dcerpc_samr_CreateUser2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_CreateUser2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_CreateUser2 *r);
struct tevent_req *dcerpc_samr_CreateUser2_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_domain_handle /* [in] [ref] */,
						struct lsa_String *_account_name /* [in] [ref] */,
						uint32_t _acct_flags /* [in]  */,
						uint32_t _access_mask /* [in]  */,
						struct policy_handle *_user_handle /* [out] [ref] */,
						uint32_t *_access_granted /* [out] [ref] */,
						uint32_t *_rid /* [out] [ref] */);
NTSTATUS dcerpc_samr_CreateUser2_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_samr_CreateUser2(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_domain_handle /* [in] [ref] */,
				 struct lsa_String *_account_name /* [in] [ref] */,
				 uint32_t _acct_flags /* [in]  */,
				 uint32_t _access_mask /* [in]  */,
				 struct policy_handle *_user_handle /* [out] [ref] */,
				 uint32_t *_access_granted /* [out] [ref] */,
				 uint32_t *_rid /* [out] [ref] */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_samr_QueryDisplayInfo3_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_QueryDisplayInfo3 *r);
NTSTATUS dcerpc_samr_QueryDisplayInfo3_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_QueryDisplayInfo3_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_QueryDisplayInfo3 *r);
struct tevent_req *dcerpc_samr_QueryDisplayInfo3_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_domain_handle /* [in] [ref] */,
						      uint16_t _level /* [in]  */,
						      uint32_t _start_idx /* [in]  */,
						      uint32_t _max_entries /* [in]  */,
						      uint32_t _buf_size /* [in]  */,
						      uint32_t *_total_size /* [out] [ref] */,
						      uint32_t *_returned_size /* [out] [ref] */,
						      union samr_DispInfo *_info /* [out] [switch_is(level),ref] */);
NTSTATUS dcerpc_samr_QueryDisplayInfo3_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_samr_QueryDisplayInfo3(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_domain_handle /* [in] [ref] */,
				       uint16_t _level /* [in]  */,
				       uint32_t _start_idx /* [in]  */,
				       uint32_t _max_entries /* [in]  */,
				       uint32_t _buf_size /* [in]  */,
				       uint32_t *_total_size /* [out] [ref] */,
				       uint32_t *_returned_size /* [out] [ref] */,
				       union samr_DispInfo *_info /* [out] [switch_is(level),ref] */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_samr_AddMultipleMembersToAlias_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_AddMultipleMembersToAlias *r);
NTSTATUS dcerpc_samr_AddMultipleMembersToAlias_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_AddMultipleMembersToAlias_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_AddMultipleMembersToAlias *r);
struct tevent_req *dcerpc_samr_AddMultipleMembersToAlias_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      struct policy_handle *_alias_handle /* [in] [ref] */,
							      struct lsa_SidArray *_sids /* [in] [ref] */);
NTSTATUS dcerpc_samr_AddMultipleMembersToAlias_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    NTSTATUS *result);
NTSTATUS dcerpc_samr_AddMultipleMembersToAlias(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       struct policy_handle *_alias_handle /* [in] [ref] */,
					       struct lsa_SidArray *_sids /* [in] [ref] */,
					       NTSTATUS *result);

struct tevent_req *dcerpc_samr_RemoveMultipleMembersFromAlias_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_RemoveMultipleMembersFromAlias *r);
NTSTATUS dcerpc_samr_RemoveMultipleMembersFromAlias_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_RemoveMultipleMembersFromAlias_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_RemoveMultipleMembersFromAlias *r);
struct tevent_req *dcerpc_samr_RemoveMultipleMembersFromAlias_send(TALLOC_CTX *mem_ctx,
								   struct tevent_context *ev,
								   struct dcerpc_binding_handle *h,
								   struct policy_handle *_alias_handle /* [in] [ref] */,
								   struct lsa_SidArray *_sids /* [in] [ref] */);
NTSTATUS dcerpc_samr_RemoveMultipleMembersFromAlias_recv(struct tevent_req *req,
							 TALLOC_CTX *mem_ctx,
							 NTSTATUS *result);
NTSTATUS dcerpc_samr_RemoveMultipleMembersFromAlias(struct dcerpc_binding_handle *h,
						    TALLOC_CTX *mem_ctx,
						    struct policy_handle *_alias_handle /* [in] [ref] */,
						    struct lsa_SidArray *_sids /* [in] [ref] */,
						    NTSTATUS *result);

struct tevent_req *dcerpc_samr_OemChangePasswordUser2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_OemChangePasswordUser2 *r);
NTSTATUS dcerpc_samr_OemChangePasswordUser2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_OemChangePasswordUser2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_OemChangePasswordUser2 *r);
struct tevent_req *dcerpc_samr_OemChangePasswordUser2_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   struct lsa_AsciiString *_server /* [in] [unique] */,
							   struct lsa_AsciiString *_account /* [in] [ref] */,
							   struct samr_CryptPassword *_password /* [in] [unique] */,
							   struct samr_Password *_hash /* [in] [unique] */);
NTSTATUS dcerpc_samr_OemChangePasswordUser2_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 NTSTATUS *result);
NTSTATUS dcerpc_samr_OemChangePasswordUser2(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    struct lsa_AsciiString *_server /* [in] [unique] */,
					    struct lsa_AsciiString *_account /* [in] [ref] */,
					    struct samr_CryptPassword *_password /* [in] [unique] */,
					    struct samr_Password *_hash /* [in] [unique] */,
					    NTSTATUS *result);

struct tevent_req *dcerpc_samr_ChangePasswordUser2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_ChangePasswordUser2 *r);
NTSTATUS dcerpc_samr_ChangePasswordUser2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_ChangePasswordUser2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_ChangePasswordUser2 *r);
struct tevent_req *dcerpc_samr_ChangePasswordUser2_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct lsa_String *_server /* [in] [unique] */,
							struct lsa_String *_account /* [in] [ref] */,
							struct samr_CryptPassword *_nt_password /* [in] [unique] */,
							struct samr_Password *_nt_verifier /* [in] [unique] */,
							uint8_t _lm_change /* [in]  */,
							struct samr_CryptPassword *_lm_password /* [in] [unique] */,
							struct samr_Password *_lm_verifier /* [in] [unique] */);
NTSTATUS dcerpc_samr_ChangePasswordUser2_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS dcerpc_samr_ChangePasswordUser2(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct lsa_String *_server /* [in] [unique] */,
					 struct lsa_String *_account /* [in] [ref] */,
					 struct samr_CryptPassword *_nt_password /* [in] [unique] */,
					 struct samr_Password *_nt_verifier /* [in] [unique] */,
					 uint8_t _lm_change /* [in]  */,
					 struct samr_CryptPassword *_lm_password /* [in] [unique] */,
					 struct samr_Password *_lm_verifier /* [in] [unique] */,
					 NTSTATUS *result);

struct tevent_req *dcerpc_samr_GetDomPwInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_GetDomPwInfo *r);
NTSTATUS dcerpc_samr_GetDomPwInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_GetDomPwInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_GetDomPwInfo *r);
struct tevent_req *dcerpc_samr_GetDomPwInfo_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct lsa_String *_domain_name /* [in] [unique] */,
						 struct samr_PwInfo *_info /* [out] [ref] */);
NTSTATUS dcerpc_samr_GetDomPwInfo_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_samr_GetDomPwInfo(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct lsa_String *_domain_name /* [in] [unique] */,
				  struct samr_PwInfo *_info /* [out] [ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_samr_Connect2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_Connect2 *r);
NTSTATUS dcerpc_samr_Connect2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_Connect2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_Connect2 *r);
struct tevent_req *dcerpc_samr_Connect2_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     const char *_system_name /* [in] [charset(UTF16),unique] */,
					     uint32_t _access_mask /* [in]  */,
					     struct policy_handle *_connect_handle /* [out] [ref] */);
NTSTATUS dcerpc_samr_Connect2_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_samr_Connect2(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      const char *_system_name /* [in] [charset(UTF16),unique] */,
			      uint32_t _access_mask /* [in]  */,
			      struct policy_handle *_connect_handle /* [out] [ref] */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_samr_SetUserInfo2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_SetUserInfo2 *r);
NTSTATUS dcerpc_samr_SetUserInfo2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_SetUserInfo2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_SetUserInfo2 *r);
struct tevent_req *dcerpc_samr_SetUserInfo2_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct policy_handle *_user_handle /* [in] [ref] */,
						 enum samr_UserInfoLevel _level /* [in]  */,
						 union samr_UserInfo *_info /* [in] [ref,switch_is(level)] */);
NTSTATUS dcerpc_samr_SetUserInfo2_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_samr_SetUserInfo2(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *_user_handle /* [in] [ref] */,
				  enum samr_UserInfoLevel _level /* [in]  */,
				  union samr_UserInfo *_info /* [in] [ref,switch_is(level)] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_samr_SetBootKeyInformation_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_SetBootKeyInformation *r);
NTSTATUS dcerpc_samr_SetBootKeyInformation_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_SetBootKeyInformation_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_SetBootKeyInformation *r);
struct tevent_req *dcerpc_samr_SetBootKeyInformation_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_connect_handle /* [in] [ref] */,
							  uint32_t _unknown1 /* [in]  */,
							  uint32_t _unknown2 /* [in]  */,
							  uint32_t _unknown3 /* [in]  */);
NTSTATUS dcerpc_samr_SetBootKeyInformation_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS dcerpc_samr_SetBootKeyInformation(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_connect_handle /* [in] [ref] */,
					   uint32_t _unknown1 /* [in]  */,
					   uint32_t _unknown2 /* [in]  */,
					   uint32_t _unknown3 /* [in]  */,
					   NTSTATUS *result);

struct tevent_req *dcerpc_samr_GetBootKeyInformation_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_GetBootKeyInformation *r);
NTSTATUS dcerpc_samr_GetBootKeyInformation_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_GetBootKeyInformation_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_GetBootKeyInformation *r);
struct tevent_req *dcerpc_samr_GetBootKeyInformation_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_domain_handle /* [in] [ref] */,
							  uint32_t *_unknown /* [out] [ref] */);
NTSTATUS dcerpc_samr_GetBootKeyInformation_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS dcerpc_samr_GetBootKeyInformation(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_domain_handle /* [in] [ref] */,
					   uint32_t *_unknown /* [out] [ref] */,
					   NTSTATUS *result);

struct tevent_req *dcerpc_samr_Connect3_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_Connect3 *r);
NTSTATUS dcerpc_samr_Connect3_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_Connect3_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_Connect3 *r);
struct tevent_req *dcerpc_samr_Connect3_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     const char *_system_name /* [in] [charset(UTF16),unique] */,
					     uint32_t _unknown /* [in]  */,
					     uint32_t _access_mask /* [in]  */,
					     struct policy_handle *_connect_handle /* [out] [ref] */);
NTSTATUS dcerpc_samr_Connect3_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_samr_Connect3(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      const char *_system_name /* [in] [charset(UTF16),unique] */,
			      uint32_t _unknown /* [in]  */,
			      uint32_t _access_mask /* [in]  */,
			      struct policy_handle *_connect_handle /* [out] [ref] */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_samr_Connect4_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_Connect4 *r);
NTSTATUS dcerpc_samr_Connect4_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_Connect4_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_Connect4 *r);
struct tevent_req *dcerpc_samr_Connect4_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     const char *_system_name /* [in] [charset(UTF16),unique] */,
					     enum samr_ConnectVersion _client_version /* [in]  */,
					     uint32_t _access_mask /* [in]  */,
					     struct policy_handle *_connect_handle /* [out] [ref] */);
NTSTATUS dcerpc_samr_Connect4_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_samr_Connect4(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      const char *_system_name /* [in] [charset(UTF16),unique] */,
			      enum samr_ConnectVersion _client_version /* [in]  */,
			      uint32_t _access_mask /* [in]  */,
			      struct policy_handle *_connect_handle /* [out] [ref] */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_samr_ChangePasswordUser3_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_ChangePasswordUser3 *r);
NTSTATUS dcerpc_samr_ChangePasswordUser3_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_ChangePasswordUser3_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_ChangePasswordUser3 *r);
struct tevent_req *dcerpc_samr_ChangePasswordUser3_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct lsa_String *_server /* [in] [unique] */,
							struct lsa_String *_account /* [in] [ref] */,
							struct samr_CryptPassword *_nt_password /* [in] [unique] */,
							struct samr_Password *_nt_verifier /* [in] [unique] */,
							uint8_t _lm_change /* [in]  */,
							struct samr_CryptPassword *_lm_password /* [in] [unique] */,
							struct samr_Password *_lm_verifier /* [in] [unique] */,
							struct samr_CryptPassword *_password3 /* [in] [unique] */,
							struct samr_DomInfo1 **_dominfo /* [out] [ref] */,
							struct userPwdChangeFailureInformation **_reject /* [out] [ref] */);
NTSTATUS dcerpc_samr_ChangePasswordUser3_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS dcerpc_samr_ChangePasswordUser3(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct lsa_String *_server /* [in] [unique] */,
					 struct lsa_String *_account /* [in] [ref] */,
					 struct samr_CryptPassword *_nt_password /* [in] [unique] */,
					 struct samr_Password *_nt_verifier /* [in] [unique] */,
					 uint8_t _lm_change /* [in]  */,
					 struct samr_CryptPassword *_lm_password /* [in] [unique] */,
					 struct samr_Password *_lm_verifier /* [in] [unique] */,
					 struct samr_CryptPassword *_password3 /* [in] [unique] */,
					 struct samr_DomInfo1 **_dominfo /* [out] [ref] */,
					 struct userPwdChangeFailureInformation **_reject /* [out] [ref] */,
					 NTSTATUS *result);

struct tevent_req *dcerpc_samr_Connect5_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_Connect5 *r);
NTSTATUS dcerpc_samr_Connect5_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_Connect5_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_Connect5 *r);
struct tevent_req *dcerpc_samr_Connect5_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     const char *_system_name /* [in] [charset(UTF16),unique] */,
					     uint32_t _access_mask /* [in]  */,
					     uint32_t _level_in /* [in]  */,
					     union samr_ConnectInfo *_info_in /* [in] [ref,switch_is(level_in)] */,
					     uint32_t *_level_out /* [out] [ref] */,
					     union samr_ConnectInfo *_info_out /* [out] [ref,switch_is(*level_out)] */,
					     struct policy_handle *_connect_handle /* [out] [ref] */);
NTSTATUS dcerpc_samr_Connect5_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_samr_Connect5(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      const char *_system_name /* [in] [charset(UTF16),unique] */,
			      uint32_t _access_mask /* [in]  */,
			      uint32_t _level_in /* [in]  */,
			      union samr_ConnectInfo *_info_in /* [in] [ref,switch_is(level_in)] */,
			      uint32_t *_level_out /* [out] [ref] */,
			      union samr_ConnectInfo *_info_out /* [out] [ref,switch_is(*level_out)] */,
			      struct policy_handle *_connect_handle /* [out] [ref] */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_samr_RidToSid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_RidToSid *r);
NTSTATUS dcerpc_samr_RidToSid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_RidToSid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_RidToSid *r);
struct tevent_req *dcerpc_samr_RidToSid_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     struct policy_handle *_domain_handle /* [in] [ref] */,
					     uint32_t _rid /* [in]  */,
					     struct dom_sid2 **_sid /* [out] [ref] */);
NTSTATUS dcerpc_samr_RidToSid_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_samr_RidToSid(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      struct policy_handle *_domain_handle /* [in] [ref] */,
			      uint32_t _rid /* [in]  */,
			      struct dom_sid2 **_sid /* [out] [ref] */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_samr_SetDsrmPassword_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_SetDsrmPassword *r);
NTSTATUS dcerpc_samr_SetDsrmPassword_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_SetDsrmPassword_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_SetDsrmPassword *r);
struct tevent_req *dcerpc_samr_SetDsrmPassword_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct lsa_String *_name /* [in] [unique] */,
						    uint32_t _unknown /* [in]  */,
						    struct samr_Password *_hash /* [in] [unique] */);
NTSTATUS dcerpc_samr_SetDsrmPassword_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);
NTSTATUS dcerpc_samr_SetDsrmPassword(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct lsa_String *_name /* [in] [unique] */,
				     uint32_t _unknown /* [in]  */,
				     struct samr_Password *_hash /* [in] [unique] */,
				     NTSTATUS *result);

struct tevent_req *dcerpc_samr_ValidatePassword_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct samr_ValidatePassword *r);
NTSTATUS dcerpc_samr_ValidatePassword_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_samr_ValidatePassword_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct samr_ValidatePassword *r);
struct tevent_req *dcerpc_samr_ValidatePassword_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     enum samr_ValidatePasswordLevel _level /* [in]  */,
						     union samr_ValidatePasswordReq *_req /* [in] [switch_is(level),ref] */,
						     union samr_ValidatePasswordRep **_rep /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_samr_ValidatePassword_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   NTSTATUS *result);
NTSTATUS dcerpc_samr_ValidatePassword(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      enum samr_ValidatePasswordLevel _level /* [in]  */,
				      union samr_ValidatePasswordReq *_req /* [in] [switch_is(level),ref] */,
				      union samr_ValidatePasswordRep **_rep /* [out] [ref,switch_is(level)] */,
				      NTSTATUS *result);

#endif /* _HEADER_RPC_samr */
