#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/wbint.h"
#ifndef _HEADER_RPC_wbint
#define _HEADER_RPC_wbint

extern const struct ndr_interface_table ndr_table_wbint;

struct tevent_req *dcerpc_wbint_Ping_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_Ping *r);
NTSTATUS dcerpc_wbint_Ping_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_Ping_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_Ping *r);
struct tevent_req *dcerpc_wbint_Ping_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  uint32_t _in_data /* [in]  */,
					  uint32_t *_out_data /* [out] [ref] */);
NTSTATUS dcerpc_wbint_Ping_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_Ping(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   uint32_t _in_data /* [in]  */,
			   uint32_t *_out_data /* [out] [ref] */);

struct tevent_req *dcerpc_wbint_LookupSid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_LookupSid *r);
NTSTATUS dcerpc_wbint_LookupSid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_LookupSid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_LookupSid *r);
struct tevent_req *dcerpc_wbint_LookupSid_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct dom_sid *_sid /* [in] [ref] */,
					       enum lsa_SidType *_type /* [out] [ref] */,
					       const char **_domain /* [out] [ref,charset(UTF8)] */,
					       const char **_name /* [out] [ref,charset(UTF8)] */);
NTSTATUS dcerpc_wbint_LookupSid_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS dcerpc_wbint_LookupSid(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct dom_sid *_sid /* [in] [ref] */,
				enum lsa_SidType *_type /* [out] [ref] */,
				const char **_domain /* [out] [ref,charset(UTF8)] */,
				const char **_name /* [out] [ref,charset(UTF8)] */,
				NTSTATUS *result);

struct tevent_req *dcerpc_wbint_LookupSids_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_LookupSids *r);
NTSTATUS dcerpc_wbint_LookupSids_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_LookupSids_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_LookupSids *r);
struct tevent_req *dcerpc_wbint_LookupSids_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct lsa_SidArray *_sids /* [in] [ref] */,
						struct lsa_RefDomainList *_domains /* [out] [ref] */,
						struct lsa_TransNameArray *_names /* [out] [ref] */);
NTSTATUS dcerpc_wbint_LookupSids_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_wbint_LookupSids(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct lsa_SidArray *_sids /* [in] [ref] */,
				 struct lsa_RefDomainList *_domains /* [out] [ref] */,
				 struct lsa_TransNameArray *_names /* [out] [ref] */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_wbint_LookupName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_LookupName *r);
NTSTATUS dcerpc_wbint_LookupName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_LookupName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_LookupName *r);
struct tevent_req *dcerpc_wbint_LookupName_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char *_domain /* [in] [ref,charset(UTF8)] */,
						const char *_name /* [in] [ref,charset(UTF8)] */,
						uint32_t _flags /* [in]  */,
						enum lsa_SidType *_type /* [out] [ref] */,
						struct dom_sid *_sid /* [out] [ref] */);
NTSTATUS dcerpc_wbint_LookupName_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_wbint_LookupName(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char *_domain /* [in] [ref,charset(UTF8)] */,
				 const char *_name /* [in] [ref,charset(UTF8)] */,
				 uint32_t _flags /* [in]  */,
				 enum lsa_SidType *_type /* [out] [ref] */,
				 struct dom_sid *_sid /* [out] [ref] */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_wbint_Sid2Uid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_Sid2Uid *r);
NTSTATUS dcerpc_wbint_Sid2Uid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_Sid2Uid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_Sid2Uid *r);
struct tevent_req *dcerpc_wbint_Sid2Uid_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     const char *_dom_name /* [in] [unique,charset(UTF8)] */,
					     struct dom_sid *_sid /* [in] [ref] */,
					     uint64_t *_uid /* [out] [ref] */);
NTSTATUS dcerpc_wbint_Sid2Uid_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_wbint_Sid2Uid(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      const char *_dom_name /* [in] [unique,charset(UTF8)] */,
			      struct dom_sid *_sid /* [in] [ref] */,
			      uint64_t *_uid /* [out] [ref] */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_wbint_Sid2Gid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_Sid2Gid *r);
NTSTATUS dcerpc_wbint_Sid2Gid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_Sid2Gid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_Sid2Gid *r);
struct tevent_req *dcerpc_wbint_Sid2Gid_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     const char *_dom_name /* [in] [unique,charset(UTF8)] */,
					     struct dom_sid *_sid /* [in] [ref] */,
					     uint64_t *_gid /* [out] [ref] */);
NTSTATUS dcerpc_wbint_Sid2Gid_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_wbint_Sid2Gid(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      const char *_dom_name /* [in] [unique,charset(UTF8)] */,
			      struct dom_sid *_sid /* [in] [ref] */,
			      uint64_t *_gid /* [out] [ref] */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_wbint_Sids2UnixIDs_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_Sids2UnixIDs *r);
NTSTATUS dcerpc_wbint_Sids2UnixIDs_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_Sids2UnixIDs_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_Sids2UnixIDs *r);
struct tevent_req *dcerpc_wbint_Sids2UnixIDs_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct lsa_RefDomainList *_domains /* [in] [ref] */,
						  struct wbint_TransIDArray *_ids /* [in,out] [ref] */);
NTSTATUS dcerpc_wbint_Sids2UnixIDs_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					NTSTATUS *result);
NTSTATUS dcerpc_wbint_Sids2UnixIDs(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct lsa_RefDomainList *_domains /* [in] [ref] */,
				   struct wbint_TransIDArray *_ids /* [in,out] [ref] */,
				   NTSTATUS *result);

struct tevent_req *dcerpc_wbint_Uid2Sid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_Uid2Sid *r);
NTSTATUS dcerpc_wbint_Uid2Sid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_Uid2Sid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_Uid2Sid *r);
struct tevent_req *dcerpc_wbint_Uid2Sid_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     const char *_dom_name /* [in] [unique,charset(UTF8)] */,
					     uint64_t _uid /* [in]  */,
					     struct dom_sid *_sid /* [out] [ref] */);
NTSTATUS dcerpc_wbint_Uid2Sid_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_wbint_Uid2Sid(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      const char *_dom_name /* [in] [unique,charset(UTF8)] */,
			      uint64_t _uid /* [in]  */,
			      struct dom_sid *_sid /* [out] [ref] */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_wbint_Gid2Sid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_Gid2Sid *r);
NTSTATUS dcerpc_wbint_Gid2Sid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_Gid2Sid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_Gid2Sid *r);
struct tevent_req *dcerpc_wbint_Gid2Sid_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     const char *_dom_name /* [in] [unique,charset(UTF8)] */,
					     uint64_t _gid /* [in]  */,
					     struct dom_sid *_sid /* [out] [ref] */);
NTSTATUS dcerpc_wbint_Gid2Sid_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_wbint_Gid2Sid(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      const char *_dom_name /* [in] [unique,charset(UTF8)] */,
			      uint64_t _gid /* [in]  */,
			      struct dom_sid *_sid /* [out] [ref] */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_wbint_AllocateUid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_AllocateUid *r);
NTSTATUS dcerpc_wbint_AllocateUid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_AllocateUid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_AllocateUid *r);
struct tevent_req *dcerpc_wbint_AllocateUid_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 uint64_t *_uid /* [out] [ref] */);
NTSTATUS dcerpc_wbint_AllocateUid_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_wbint_AllocateUid(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  uint64_t *_uid /* [out] [ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_wbint_AllocateGid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_AllocateGid *r);
NTSTATUS dcerpc_wbint_AllocateGid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_AllocateGid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_AllocateGid *r);
struct tevent_req *dcerpc_wbint_AllocateGid_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 uint64_t *_gid /* [out] [ref] */);
NTSTATUS dcerpc_wbint_AllocateGid_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_wbint_AllocateGid(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  uint64_t *_gid /* [out] [ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_wbint_QueryUser_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_QueryUser *r);
NTSTATUS dcerpc_wbint_QueryUser_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_QueryUser_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_QueryUser *r);
struct tevent_req *dcerpc_wbint_QueryUser_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct dom_sid *_sid /* [in] [ref] */,
					       struct wbint_userinfo *_info /* [out] [ref] */);
NTSTATUS dcerpc_wbint_QueryUser_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     NTSTATUS *result);
NTSTATUS dcerpc_wbint_QueryUser(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct dom_sid *_sid /* [in] [ref] */,
				struct wbint_userinfo *_info /* [out] [ref] */,
				NTSTATUS *result);

struct tevent_req *dcerpc_wbint_LookupUserAliases_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_LookupUserAliases *r);
NTSTATUS dcerpc_wbint_LookupUserAliases_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_LookupUserAliases_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_LookupUserAliases *r);
struct tevent_req *dcerpc_wbint_LookupUserAliases_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct wbint_SidArray *_sids /* [in] [ref] */,
						       struct wbint_RidArray *_rids /* [out] [ref] */);
NTSTATUS dcerpc_wbint_LookupUserAliases_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS dcerpc_wbint_LookupUserAliases(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct wbint_SidArray *_sids /* [in] [ref] */,
					struct wbint_RidArray *_rids /* [out] [ref] */,
					NTSTATUS *result);

struct tevent_req *dcerpc_wbint_LookupUserGroups_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_LookupUserGroups *r);
NTSTATUS dcerpc_wbint_LookupUserGroups_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_LookupUserGroups_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_LookupUserGroups *r);
struct tevent_req *dcerpc_wbint_LookupUserGroups_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct dom_sid *_sid /* [in] [ref] */,
						      struct wbint_SidArray *_sids /* [out] [ref] */);
NTSTATUS dcerpc_wbint_LookupUserGroups_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_wbint_LookupUserGroups(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct dom_sid *_sid /* [in] [ref] */,
				       struct wbint_SidArray *_sids /* [out] [ref] */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_wbint_QuerySequenceNumber_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_QuerySequenceNumber *r);
NTSTATUS dcerpc_wbint_QuerySequenceNumber_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_QuerySequenceNumber_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_QuerySequenceNumber *r);
struct tevent_req *dcerpc_wbint_QuerySequenceNumber_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 uint32_t *_sequence /* [out] [ref] */);
NTSTATUS dcerpc_wbint_QuerySequenceNumber_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       NTSTATUS *result);
NTSTATUS dcerpc_wbint_QuerySequenceNumber(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  uint32_t *_sequence /* [out] [ref] */,
					  NTSTATUS *result);

struct tevent_req *dcerpc_wbint_LookupGroupMembers_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_LookupGroupMembers *r);
NTSTATUS dcerpc_wbint_LookupGroupMembers_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_LookupGroupMembers_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_LookupGroupMembers *r);
struct tevent_req *dcerpc_wbint_LookupGroupMembers_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct dom_sid *_sid /* [in] [ref] */,
							enum lsa_SidType _type /* [in]  */,
							struct wbint_Principals *_members /* [out] [ref] */);
NTSTATUS dcerpc_wbint_LookupGroupMembers_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS dcerpc_wbint_LookupGroupMembers(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct dom_sid *_sid /* [in] [ref] */,
					 enum lsa_SidType _type /* [in]  */,
					 struct wbint_Principals *_members /* [out] [ref] */,
					 NTSTATUS *result);

struct tevent_req *dcerpc_wbint_QueryUserList_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_QueryUserList *r);
NTSTATUS dcerpc_wbint_QueryUserList_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_QueryUserList_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_QueryUserList *r);
struct tevent_req *dcerpc_wbint_QueryUserList_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct wbint_userinfos *_users /* [out] [ref] */);
NTSTATUS dcerpc_wbint_QueryUserList_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS dcerpc_wbint_QueryUserList(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct wbint_userinfos *_users /* [out] [ref] */,
				    NTSTATUS *result);

struct tevent_req *dcerpc_wbint_QueryGroupList_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_QueryGroupList *r);
NTSTATUS dcerpc_wbint_QueryGroupList_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_QueryGroupList_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_QueryGroupList *r);
struct tevent_req *dcerpc_wbint_QueryGroupList_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct wbint_Principals *_groups /* [out] [ref] */);
NTSTATUS dcerpc_wbint_QueryGroupList_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);
NTSTATUS dcerpc_wbint_QueryGroupList(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct wbint_Principals *_groups /* [out] [ref] */,
				     NTSTATUS *result);

struct tevent_req *dcerpc_wbint_DsGetDcName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_DsGetDcName *r);
NTSTATUS dcerpc_wbint_DsGetDcName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_DsGetDcName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_DsGetDcName *r);
struct tevent_req *dcerpc_wbint_DsGetDcName_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char *_domain_name /* [in] [ref,charset(UTF8)] */,
						 struct GUID *_domain_guid /* [in] [unique] */,
						 const char *_site_name /* [in] [unique,charset(UTF8)] */,
						 uint32_t _flags /* [in]  */,
						 struct netr_DsRGetDCNameInfo **_dc_info /* [out] [ref] */);
NTSTATUS dcerpc_wbint_DsGetDcName_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_wbint_DsGetDcName(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char *_domain_name /* [in] [ref,charset(UTF8)] */,
				  struct GUID *_domain_guid /* [in] [unique] */,
				  const char *_site_name /* [in] [unique,charset(UTF8)] */,
				  uint32_t _flags /* [in]  */,
				  struct netr_DsRGetDCNameInfo **_dc_info /* [out] [ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_wbint_LookupRids_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_LookupRids *r);
NTSTATUS dcerpc_wbint_LookupRids_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_LookupRids_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_LookupRids *r);
struct tevent_req *dcerpc_wbint_LookupRids_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct dom_sid *_domain_sid /* [in] [ref] */,
						struct wbint_RidArray *_rids /* [in] [ref] */,
						const char **_domain_name /* [out] [ref,charset(UTF8)] */,
						struct wbint_Principals *_names /* [out] [ref] */);
NTSTATUS dcerpc_wbint_LookupRids_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_wbint_LookupRids(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct dom_sid *_domain_sid /* [in] [ref] */,
				 struct wbint_RidArray *_rids /* [in] [ref] */,
				 const char **_domain_name /* [out] [ref,charset(UTF8)] */,
				 struct wbint_Principals *_names /* [out] [ref] */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_wbint_CheckMachineAccount_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_CheckMachineAccount *r);
NTSTATUS dcerpc_wbint_CheckMachineAccount_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_CheckMachineAccount_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_CheckMachineAccount *r);
struct tevent_req *dcerpc_wbint_CheckMachineAccount_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wbint_CheckMachineAccount_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       NTSTATUS *result);
NTSTATUS dcerpc_wbint_CheckMachineAccount(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);

struct tevent_req *dcerpc_wbint_ChangeMachineAccount_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_ChangeMachineAccount *r);
NTSTATUS dcerpc_wbint_ChangeMachineAccount_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_ChangeMachineAccount_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_ChangeMachineAccount *r);
struct tevent_req *dcerpc_wbint_ChangeMachineAccount_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wbint_ChangeMachineAccount_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS dcerpc_wbint_ChangeMachineAccount(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   NTSTATUS *result);

struct tevent_req *dcerpc_wbint_PingDc_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wbint_PingDc *r);
NTSTATUS dcerpc_wbint_PingDc_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wbint_PingDc_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wbint_PingDc *r);
struct tevent_req *dcerpc_wbint_PingDc_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wbint_PingDc_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  NTSTATUS *result);
NTSTATUS dcerpc_wbint_PingDc(struct dcerpc_binding_handle *h,
			     TALLOC_CTX *mem_ctx,
			     NTSTATUS *result);

#endif /* _HEADER_RPC_wbint */
