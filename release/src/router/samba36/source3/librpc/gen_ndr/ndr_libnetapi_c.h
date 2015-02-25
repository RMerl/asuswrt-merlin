#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/libnetapi.h"
#ifndef _HEADER_RPC_libnetapi
#define _HEADER_RPC_libnetapi

struct tevent_req *dcerpc_NetJoinDomain_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetJoinDomain *r);
NTSTATUS dcerpc_NetJoinDomain_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetJoinDomain_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetJoinDomain *r);
struct tevent_req *dcerpc_NetJoinDomain_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     const char * _server /* [in] [unique] */,
					     const char * _domain /* [in] [ref] */,
					     const char * _account_ou /* [in] [unique] */,
					     const char * _account /* [in] [unique] */,
					     const char * _password /* [in] [unique] */,
					     uint32_t _join_flags /* [in]  */);
NTSTATUS dcerpc_NetJoinDomain_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetJoinDomain(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      const char * _server /* [in] [unique] */,
			      const char * _domain /* [in] [ref] */,
			      const char * _account_ou /* [in] [unique] */,
			      const char * _account /* [in] [unique] */,
			      const char * _password /* [in] [unique] */,
			      uint32_t _join_flags /* [in]  */,
			      enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetUnjoinDomain_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetUnjoinDomain *r);
NTSTATUS dcerpc_NetUnjoinDomain_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetUnjoinDomain_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetUnjoinDomain *r);
struct tevent_req *dcerpc_NetUnjoinDomain_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       const char * _server_name /* [in] [unique] */,
					       const char * _account /* [in] [unique] */,
					       const char * _password /* [in] [unique] */,
					       uint32_t _unjoin_flags /* [in]  */);
NTSTATUS dcerpc_NetUnjoinDomain_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetUnjoinDomain(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				const char * _server_name /* [in] [unique] */,
				const char * _account /* [in] [unique] */,
				const char * _password /* [in] [unique] */,
				uint32_t _unjoin_flags /* [in]  */,
				enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetGetJoinInformation_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetGetJoinInformation *r);
NTSTATUS dcerpc_NetGetJoinInformation_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetGetJoinInformation_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetGetJoinInformation *r);
struct tevent_req *dcerpc_NetGetJoinInformation_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     const char * _server_name /* [in] [unique] */,
						     const char * *_name_buffer /* [out] [ref] */,
						     uint16_t *_name_type /* [out] [ref] */);
NTSTATUS dcerpc_NetGetJoinInformation_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetGetJoinInformation(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      const char * _server_name /* [in] [unique] */,
				      const char * *_name_buffer /* [out] [ref] */,
				      uint16_t *_name_type /* [out] [ref] */,
				      enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetGetJoinableOUs_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetGetJoinableOUs *r);
NTSTATUS dcerpc_NetGetJoinableOUs_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetGetJoinableOUs_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetGetJoinableOUs *r);
struct tevent_req *dcerpc_NetGetJoinableOUs_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char * _server_name /* [in] [unique] */,
						 const char * _domain /* [in] [ref] */,
						 const char * _account /* [in] [unique] */,
						 const char * _password /* [in] [unique] */,
						 uint32_t *_ou_count /* [out] [ref] */,
						 const char * **_ous /* [out] [ref] */);
NTSTATUS dcerpc_NetGetJoinableOUs_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetGetJoinableOUs(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char * _server_name /* [in] [unique] */,
				  const char * _domain /* [in] [ref] */,
				  const char * _account /* [in] [unique] */,
				  const char * _password /* [in] [unique] */,
				  uint32_t *_ou_count /* [out] [ref] */,
				  const char * **_ous /* [out] [ref] */,
				  enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetRenameMachineInDomain_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetRenameMachineInDomain *r);
NTSTATUS dcerpc_NetRenameMachineInDomain_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetRenameMachineInDomain_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetRenameMachineInDomain *r);
struct tevent_req *dcerpc_NetRenameMachineInDomain_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							const char * _server_name /* [in]  */,
							const char * _new_machine_name /* [in]  */,
							const char * _account /* [in]  */,
							const char * _password /* [in]  */,
							uint32_t _rename_options /* [in]  */);
NTSTATUS dcerpc_NetRenameMachineInDomain_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetRenameMachineInDomain(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 const char * _server_name /* [in]  */,
					 const char * _new_machine_name /* [in]  */,
					 const char * _account /* [in]  */,
					 const char * _password /* [in]  */,
					 uint32_t _rename_options /* [in]  */,
					 enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetServerGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetServerGetInfo *r);
NTSTATUS dcerpc_NetServerGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetServerGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetServerGetInfo *r);
struct tevent_req *dcerpc_NetServerGetInfo_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char * _server_name /* [in] [unique] */,
						uint32_t _level /* [in]  */,
						uint8_t **_buffer /* [out] [ref] */);
NTSTATUS dcerpc_NetServerGetInfo_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetServerGetInfo(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char * _server_name /* [in] [unique] */,
				 uint32_t _level /* [in]  */,
				 uint8_t **_buffer /* [out] [ref] */,
				 enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetServerSetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetServerSetInfo *r);
NTSTATUS dcerpc_NetServerSetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetServerSetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetServerSetInfo *r);
struct tevent_req *dcerpc_NetServerSetInfo_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char * _server_name /* [in] [unique] */,
						uint32_t _level /* [in]  */,
						uint8_t *_buffer /* [in] [ref] */,
						uint32_t *_parm_error /* [out] [ref] */);
NTSTATUS dcerpc_NetServerSetInfo_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetServerSetInfo(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char * _server_name /* [in] [unique] */,
				 uint32_t _level /* [in]  */,
				 uint8_t *_buffer /* [in] [ref] */,
				 uint32_t *_parm_error /* [out] [ref] */,
				 enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetGetDCName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetGetDCName *r);
NTSTATUS dcerpc_NetGetDCName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetGetDCName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetGetDCName *r);
struct tevent_req *dcerpc_NetGetDCName_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct dcerpc_binding_handle *h,
					    const char * _server_name /* [in] [unique] */,
					    const char * _domain_name /* [in] [unique] */,
					    uint8_t **_buffer /* [out] [ref] */);
NTSTATUS dcerpc_NetGetDCName_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetGetDCName(struct dcerpc_binding_handle *h,
			     TALLOC_CTX *mem_ctx,
			     const char * _server_name /* [in] [unique] */,
			     const char * _domain_name /* [in] [unique] */,
			     uint8_t **_buffer /* [out] [ref] */,
			     enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetGetAnyDCName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetGetAnyDCName *r);
NTSTATUS dcerpc_NetGetAnyDCName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetGetAnyDCName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetGetAnyDCName *r);
struct tevent_req *dcerpc_NetGetAnyDCName_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       const char * _server_name /* [in] [unique] */,
					       const char * _domain_name /* [in] [unique] */,
					       uint8_t **_buffer /* [out] [ref] */);
NTSTATUS dcerpc_NetGetAnyDCName_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetGetAnyDCName(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				const char * _server_name /* [in] [unique] */,
				const char * _domain_name /* [in] [unique] */,
				uint8_t **_buffer /* [out] [ref] */,
				enum NET_API_STATUS *result);

struct tevent_req *dcerpc_DsGetDcName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct DsGetDcName *r);
NTSTATUS dcerpc_DsGetDcName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_DsGetDcName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct DsGetDcName *r);
struct tevent_req *dcerpc_DsGetDcName_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   const char * _server_name /* [in] [unique] */,
					   const char * _domain_name /* [in] [ref] */,
					   struct GUID *_domain_guid /* [in] [unique] */,
					   const char * _site_name /* [in] [unique] */,
					   uint32_t _flags /* [in]  */,
					   struct DOMAIN_CONTROLLER_INFO **_dc_info /* [out] [ref] */);
NTSTATUS dcerpc_DsGetDcName_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 enum NET_API_STATUS *result);
NTSTATUS dcerpc_DsGetDcName(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    const char * _server_name /* [in] [unique] */,
			    const char * _domain_name /* [in] [ref] */,
			    struct GUID *_domain_guid /* [in] [unique] */,
			    const char * _site_name /* [in] [unique] */,
			    uint32_t _flags /* [in]  */,
			    struct DOMAIN_CONTROLLER_INFO **_dc_info /* [out] [ref] */,
			    enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetUserAdd_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetUserAdd *r);
NTSTATUS dcerpc_NetUserAdd_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetUserAdd_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetUserAdd *r);
struct tevent_req *dcerpc_NetUserAdd_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  const char * _server_name /* [in] [unique] */,
					  uint32_t _level /* [in]  */,
					  uint8_t *_buffer /* [in] [ref] */,
					  uint32_t *_parm_error /* [out] [ref] */);
NTSTATUS dcerpc_NetUserAdd_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetUserAdd(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   const char * _server_name /* [in] [unique] */,
			   uint32_t _level /* [in]  */,
			   uint8_t *_buffer /* [in] [ref] */,
			   uint32_t *_parm_error /* [out] [ref] */,
			   enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetUserDel_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetUserDel *r);
NTSTATUS dcerpc_NetUserDel_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetUserDel_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetUserDel *r);
struct tevent_req *dcerpc_NetUserDel_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  const char * _server_name /* [in] [unique] */,
					  const char * _user_name /* [in] [ref] */);
NTSTATUS dcerpc_NetUserDel_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetUserDel(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   const char * _server_name /* [in] [unique] */,
			   const char * _user_name /* [in] [ref] */,
			   enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetUserEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetUserEnum *r);
NTSTATUS dcerpc_NetUserEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetUserEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetUserEnum *r);
struct tevent_req *dcerpc_NetUserEnum_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   const char * _server_name /* [in] [unique] */,
					   uint32_t _level /* [in]  */,
					   uint32_t _filter /* [in]  */,
					   uint8_t **_buffer /* [out] [ref] */,
					   uint32_t _prefmaxlen /* [in]  */,
					   uint32_t *_entries_read /* [out] [ref] */,
					   uint32_t *_total_entries /* [out] [ref] */,
					   uint32_t *_resume_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_NetUserEnum_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetUserEnum(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    const char * _server_name /* [in] [unique] */,
			    uint32_t _level /* [in]  */,
			    uint32_t _filter /* [in]  */,
			    uint8_t **_buffer /* [out] [ref] */,
			    uint32_t _prefmaxlen /* [in]  */,
			    uint32_t *_entries_read /* [out] [ref] */,
			    uint32_t *_total_entries /* [out] [ref] */,
			    uint32_t *_resume_handle /* [in,out] [ref] */,
			    enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetUserChangePassword_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetUserChangePassword *r);
NTSTATUS dcerpc_NetUserChangePassword_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetUserChangePassword_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetUserChangePassword *r);
struct tevent_req *dcerpc_NetUserChangePassword_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     const char * _domain_name /* [in]  */,
						     const char * _user_name /* [in]  */,
						     const char * _old_password /* [in]  */,
						     const char * _new_password /* [in]  */);
NTSTATUS dcerpc_NetUserChangePassword_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetUserChangePassword(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      const char * _domain_name /* [in]  */,
				      const char * _user_name /* [in]  */,
				      const char * _old_password /* [in]  */,
				      const char * _new_password /* [in]  */,
				      enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetUserGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetUserGetInfo *r);
NTSTATUS dcerpc_NetUserGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetUserGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetUserGetInfo *r);
struct tevent_req *dcerpc_NetUserGetInfo_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      const char * _server_name /* [in]  */,
					      const char * _user_name /* [in]  */,
					      uint32_t _level /* [in]  */,
					      uint8_t **_buffer /* [out] [ref] */);
NTSTATUS dcerpc_NetUserGetInfo_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetUserGetInfo(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       const char * _server_name /* [in]  */,
			       const char * _user_name /* [in]  */,
			       uint32_t _level /* [in]  */,
			       uint8_t **_buffer /* [out] [ref] */,
			       enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetUserSetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetUserSetInfo *r);
NTSTATUS dcerpc_NetUserSetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetUserSetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetUserSetInfo *r);
struct tevent_req *dcerpc_NetUserSetInfo_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      const char * _server_name /* [in]  */,
					      const char * _user_name /* [in]  */,
					      uint32_t _level /* [in]  */,
					      uint8_t *_buffer /* [in] [ref] */,
					      uint32_t *_parm_err /* [out] [ref] */);
NTSTATUS dcerpc_NetUserSetInfo_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetUserSetInfo(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       const char * _server_name /* [in]  */,
			       const char * _user_name /* [in]  */,
			       uint32_t _level /* [in]  */,
			       uint8_t *_buffer /* [in] [ref] */,
			       uint32_t *_parm_err /* [out] [ref] */,
			       enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetUserGetGroups_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetUserGetGroups *r);
NTSTATUS dcerpc_NetUserGetGroups_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetUserGetGroups_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetUserGetGroups *r);
struct tevent_req *dcerpc_NetUserGetGroups_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char * _server_name /* [in]  */,
						const char * _user_name /* [in]  */,
						uint32_t _level /* [in]  */,
						uint8_t **_buffer /* [out] [ref] */,
						uint32_t _prefmaxlen /* [in]  */,
						uint32_t *_entries_read /* [out] [ref] */,
						uint32_t *_total_entries /* [out] [ref] */);
NTSTATUS dcerpc_NetUserGetGroups_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetUserGetGroups(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char * _server_name /* [in]  */,
				 const char * _user_name /* [in]  */,
				 uint32_t _level /* [in]  */,
				 uint8_t **_buffer /* [out] [ref] */,
				 uint32_t _prefmaxlen /* [in]  */,
				 uint32_t *_entries_read /* [out] [ref] */,
				 uint32_t *_total_entries /* [out] [ref] */,
				 enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetUserSetGroups_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetUserSetGroups *r);
NTSTATUS dcerpc_NetUserSetGroups_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetUserSetGroups_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetUserSetGroups *r);
struct tevent_req *dcerpc_NetUserSetGroups_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char * _server_name /* [in]  */,
						const char * _user_name /* [in]  */,
						uint32_t _level /* [in]  */,
						uint8_t *_buffer /* [in] [ref] */,
						uint32_t _num_entries /* [in]  */);
NTSTATUS dcerpc_NetUserSetGroups_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetUserSetGroups(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char * _server_name /* [in]  */,
				 const char * _user_name /* [in]  */,
				 uint32_t _level /* [in]  */,
				 uint8_t *_buffer /* [in] [ref] */,
				 uint32_t _num_entries /* [in]  */,
				 enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetUserGetLocalGroups_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetUserGetLocalGroups *r);
NTSTATUS dcerpc_NetUserGetLocalGroups_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetUserGetLocalGroups_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetUserGetLocalGroups *r);
struct tevent_req *dcerpc_NetUserGetLocalGroups_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     const char * _server_name /* [in]  */,
						     const char * _user_name /* [in]  */,
						     uint32_t _level /* [in]  */,
						     uint32_t _flags /* [in]  */,
						     uint8_t **_buffer /* [out] [ref] */,
						     uint32_t _prefmaxlen /* [in]  */,
						     uint32_t *_entries_read /* [out] [ref] */,
						     uint32_t *_total_entries /* [out] [ref] */);
NTSTATUS dcerpc_NetUserGetLocalGroups_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetUserGetLocalGroups(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      const char * _server_name /* [in]  */,
				      const char * _user_name /* [in]  */,
				      uint32_t _level /* [in]  */,
				      uint32_t _flags /* [in]  */,
				      uint8_t **_buffer /* [out] [ref] */,
				      uint32_t _prefmaxlen /* [in]  */,
				      uint32_t *_entries_read /* [out] [ref] */,
				      uint32_t *_total_entries /* [out] [ref] */,
				      enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetUserModalsGet_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetUserModalsGet *r);
NTSTATUS dcerpc_NetUserModalsGet_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetUserModalsGet_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetUserModalsGet *r);
struct tevent_req *dcerpc_NetUserModalsGet_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char * _server_name /* [in]  */,
						uint32_t _level /* [in]  */,
						uint8_t **_buffer /* [out] [ref] */);
NTSTATUS dcerpc_NetUserModalsGet_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetUserModalsGet(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char * _server_name /* [in]  */,
				 uint32_t _level /* [in]  */,
				 uint8_t **_buffer /* [out] [ref] */,
				 enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetUserModalsSet_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetUserModalsSet *r);
NTSTATUS dcerpc_NetUserModalsSet_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetUserModalsSet_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetUserModalsSet *r);
struct tevent_req *dcerpc_NetUserModalsSet_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char * _server_name /* [in]  */,
						uint32_t _level /* [in]  */,
						uint8_t *_buffer /* [in] [ref] */,
						uint32_t *_parm_err /* [out] [ref] */);
NTSTATUS dcerpc_NetUserModalsSet_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetUserModalsSet(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char * _server_name /* [in]  */,
				 uint32_t _level /* [in]  */,
				 uint8_t *_buffer /* [in] [ref] */,
				 uint32_t *_parm_err /* [out] [ref] */,
				 enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetQueryDisplayInformation_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetQueryDisplayInformation *r);
NTSTATUS dcerpc_NetQueryDisplayInformation_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetQueryDisplayInformation_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetQueryDisplayInformation *r);
struct tevent_req *dcerpc_NetQueryDisplayInformation_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  const char * _server_name /* [in] [unique] */,
							  uint32_t _level /* [in]  */,
							  uint32_t _idx /* [in]  */,
							  uint32_t _entries_requested /* [in]  */,
							  uint32_t _prefmaxlen /* [in]  */,
							  uint32_t *_entries_read /* [out] [ref] */,
							  void **_buffer /* [out] [ref,noprint] */);
NTSTATUS dcerpc_NetQueryDisplayInformation_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetQueryDisplayInformation(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   const char * _server_name /* [in] [unique] */,
					   uint32_t _level /* [in]  */,
					   uint32_t _idx /* [in]  */,
					   uint32_t _entries_requested /* [in]  */,
					   uint32_t _prefmaxlen /* [in]  */,
					   uint32_t *_entries_read /* [out] [ref] */,
					   void **_buffer /* [out] [ref,noprint] */,
					   enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetGroupAdd_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetGroupAdd *r);
NTSTATUS dcerpc_NetGroupAdd_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetGroupAdd_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetGroupAdd *r);
struct tevent_req *dcerpc_NetGroupAdd_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   const char * _server_name /* [in]  */,
					   uint32_t _level /* [in]  */,
					   uint8_t *_buffer /* [in] [ref] */,
					   uint32_t *_parm_err /* [out] [ref] */);
NTSTATUS dcerpc_NetGroupAdd_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetGroupAdd(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    const char * _server_name /* [in]  */,
			    uint32_t _level /* [in]  */,
			    uint8_t *_buffer /* [in] [ref] */,
			    uint32_t *_parm_err /* [out] [ref] */,
			    enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetGroupDel_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetGroupDel *r);
NTSTATUS dcerpc_NetGroupDel_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetGroupDel_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetGroupDel *r);
struct tevent_req *dcerpc_NetGroupDel_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   const char * _server_name /* [in]  */,
					   const char * _group_name /* [in]  */);
NTSTATUS dcerpc_NetGroupDel_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetGroupDel(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    const char * _server_name /* [in]  */,
			    const char * _group_name /* [in]  */,
			    enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetGroupEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetGroupEnum *r);
NTSTATUS dcerpc_NetGroupEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetGroupEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetGroupEnum *r);
struct tevent_req *dcerpc_NetGroupEnum_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct dcerpc_binding_handle *h,
					    const char * _server_name /* [in]  */,
					    uint32_t _level /* [in]  */,
					    uint8_t **_buffer /* [out] [ref] */,
					    uint32_t _prefmaxlen /* [in]  */,
					    uint32_t *_entries_read /* [out] [ref] */,
					    uint32_t *_total_entries /* [out] [ref] */,
					    uint32_t *_resume_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_NetGroupEnum_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetGroupEnum(struct dcerpc_binding_handle *h,
			     TALLOC_CTX *mem_ctx,
			     const char * _server_name /* [in]  */,
			     uint32_t _level /* [in]  */,
			     uint8_t **_buffer /* [out] [ref] */,
			     uint32_t _prefmaxlen /* [in]  */,
			     uint32_t *_entries_read /* [out] [ref] */,
			     uint32_t *_total_entries /* [out] [ref] */,
			     uint32_t *_resume_handle /* [in,out] [ref] */,
			     enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetGroupSetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetGroupSetInfo *r);
NTSTATUS dcerpc_NetGroupSetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetGroupSetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetGroupSetInfo *r);
struct tevent_req *dcerpc_NetGroupSetInfo_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       const char * _server_name /* [in]  */,
					       const char * _group_name /* [in]  */,
					       uint32_t _level /* [in]  */,
					       uint8_t *_buffer /* [in] [ref] */,
					       uint32_t *_parm_err /* [out] [ref] */);
NTSTATUS dcerpc_NetGroupSetInfo_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetGroupSetInfo(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				const char * _server_name /* [in]  */,
				const char * _group_name /* [in]  */,
				uint32_t _level /* [in]  */,
				uint8_t *_buffer /* [in] [ref] */,
				uint32_t *_parm_err /* [out] [ref] */,
				enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetGroupGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetGroupGetInfo *r);
NTSTATUS dcerpc_NetGroupGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetGroupGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetGroupGetInfo *r);
struct tevent_req *dcerpc_NetGroupGetInfo_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       const char * _server_name /* [in]  */,
					       const char * _group_name /* [in]  */,
					       uint32_t _level /* [in]  */,
					       uint8_t **_buffer /* [out] [ref] */);
NTSTATUS dcerpc_NetGroupGetInfo_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetGroupGetInfo(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				const char * _server_name /* [in]  */,
				const char * _group_name /* [in]  */,
				uint32_t _level /* [in]  */,
				uint8_t **_buffer /* [out] [ref] */,
				enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetGroupAddUser_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetGroupAddUser *r);
NTSTATUS dcerpc_NetGroupAddUser_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetGroupAddUser_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetGroupAddUser *r);
struct tevent_req *dcerpc_NetGroupAddUser_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       const char * _server_name /* [in]  */,
					       const char * _group_name /* [in]  */,
					       const char * _user_name /* [in]  */);
NTSTATUS dcerpc_NetGroupAddUser_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetGroupAddUser(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				const char * _server_name /* [in]  */,
				const char * _group_name /* [in]  */,
				const char * _user_name /* [in]  */,
				enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetGroupDelUser_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetGroupDelUser *r);
NTSTATUS dcerpc_NetGroupDelUser_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetGroupDelUser_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetGroupDelUser *r);
struct tevent_req *dcerpc_NetGroupDelUser_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       const char * _server_name /* [in]  */,
					       const char * _group_name /* [in]  */,
					       const char * _user_name /* [in]  */);
NTSTATUS dcerpc_NetGroupDelUser_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetGroupDelUser(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				const char * _server_name /* [in]  */,
				const char * _group_name /* [in]  */,
				const char * _user_name /* [in]  */,
				enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetGroupGetUsers_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetGroupGetUsers *r);
NTSTATUS dcerpc_NetGroupGetUsers_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetGroupGetUsers_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetGroupGetUsers *r);
struct tevent_req *dcerpc_NetGroupGetUsers_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char * _server_name /* [in]  */,
						const char * _group_name /* [in]  */,
						uint32_t _level /* [in]  */,
						uint8_t **_buffer /* [out] [ref] */,
						uint32_t _prefmaxlen /* [in]  */,
						uint32_t *_entries_read /* [out] [ref] */,
						uint32_t *_total_entries /* [out] [ref] */,
						uint32_t *_resume_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_NetGroupGetUsers_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetGroupGetUsers(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char * _server_name /* [in]  */,
				 const char * _group_name /* [in]  */,
				 uint32_t _level /* [in]  */,
				 uint8_t **_buffer /* [out] [ref] */,
				 uint32_t _prefmaxlen /* [in]  */,
				 uint32_t *_entries_read /* [out] [ref] */,
				 uint32_t *_total_entries /* [out] [ref] */,
				 uint32_t *_resume_handle /* [in,out] [ref] */,
				 enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetGroupSetUsers_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetGroupSetUsers *r);
NTSTATUS dcerpc_NetGroupSetUsers_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetGroupSetUsers_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetGroupSetUsers *r);
struct tevent_req *dcerpc_NetGroupSetUsers_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char * _server_name /* [in]  */,
						const char * _group_name /* [in]  */,
						uint32_t _level /* [in]  */,
						uint8_t *_buffer /* [in] [ref] */,
						uint32_t _num_entries /* [in]  */);
NTSTATUS dcerpc_NetGroupSetUsers_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetGroupSetUsers(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char * _server_name /* [in]  */,
				 const char * _group_name /* [in]  */,
				 uint32_t _level /* [in]  */,
				 uint8_t *_buffer /* [in] [ref] */,
				 uint32_t _num_entries /* [in]  */,
				 enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetLocalGroupAdd_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetLocalGroupAdd *r);
NTSTATUS dcerpc_NetLocalGroupAdd_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetLocalGroupAdd_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetLocalGroupAdd *r);
struct tevent_req *dcerpc_NetLocalGroupAdd_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char * _server_name /* [in]  */,
						uint32_t _level /* [in]  */,
						uint8_t *_buffer /* [in] [ref] */,
						uint32_t *_parm_err /* [out] [ref] */);
NTSTATUS dcerpc_NetLocalGroupAdd_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetLocalGroupAdd(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char * _server_name /* [in]  */,
				 uint32_t _level /* [in]  */,
				 uint8_t *_buffer /* [in] [ref] */,
				 uint32_t *_parm_err /* [out] [ref] */,
				 enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetLocalGroupDel_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetLocalGroupDel *r);
NTSTATUS dcerpc_NetLocalGroupDel_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetLocalGroupDel_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetLocalGroupDel *r);
struct tevent_req *dcerpc_NetLocalGroupDel_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char * _server_name /* [in]  */,
						const char * _group_name /* [in]  */);
NTSTATUS dcerpc_NetLocalGroupDel_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetLocalGroupDel(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char * _server_name /* [in]  */,
				 const char * _group_name /* [in]  */,
				 enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetLocalGroupGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetLocalGroupGetInfo *r);
NTSTATUS dcerpc_NetLocalGroupGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetLocalGroupGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetLocalGroupGetInfo *r);
struct tevent_req *dcerpc_NetLocalGroupGetInfo_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    const char * _server_name /* [in]  */,
						    const char * _group_name /* [in]  */,
						    uint32_t _level /* [in]  */,
						    uint8_t **_buffer /* [out] [ref] */);
NTSTATUS dcerpc_NetLocalGroupGetInfo_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetLocalGroupGetInfo(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char * _server_name /* [in]  */,
				     const char * _group_name /* [in]  */,
				     uint32_t _level /* [in]  */,
				     uint8_t **_buffer /* [out] [ref] */,
				     enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetLocalGroupSetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetLocalGroupSetInfo *r);
NTSTATUS dcerpc_NetLocalGroupSetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetLocalGroupSetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetLocalGroupSetInfo *r);
struct tevent_req *dcerpc_NetLocalGroupSetInfo_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    const char * _server_name /* [in]  */,
						    const char * _group_name /* [in]  */,
						    uint32_t _level /* [in]  */,
						    uint8_t *_buffer /* [in] [ref] */,
						    uint32_t *_parm_err /* [out] [ref] */);
NTSTATUS dcerpc_NetLocalGroupSetInfo_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetLocalGroupSetInfo(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char * _server_name /* [in]  */,
				     const char * _group_name /* [in]  */,
				     uint32_t _level /* [in]  */,
				     uint8_t *_buffer /* [in] [ref] */,
				     uint32_t *_parm_err /* [out] [ref] */,
				     enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetLocalGroupEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetLocalGroupEnum *r);
NTSTATUS dcerpc_NetLocalGroupEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetLocalGroupEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetLocalGroupEnum *r);
struct tevent_req *dcerpc_NetLocalGroupEnum_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char * _server_name /* [in]  */,
						 uint32_t _level /* [in]  */,
						 uint8_t **_buffer /* [out] [ref] */,
						 uint32_t _prefmaxlen /* [in]  */,
						 uint32_t *_entries_read /* [out] [ref] */,
						 uint32_t *_total_entries /* [out] [ref] */,
						 uint32_t *_resume_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_NetLocalGroupEnum_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetLocalGroupEnum(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char * _server_name /* [in]  */,
				  uint32_t _level /* [in]  */,
				  uint8_t **_buffer /* [out] [ref] */,
				  uint32_t _prefmaxlen /* [in]  */,
				  uint32_t *_entries_read /* [out] [ref] */,
				  uint32_t *_total_entries /* [out] [ref] */,
				  uint32_t *_resume_handle /* [in,out] [ref] */,
				  enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetLocalGroupAddMembers_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetLocalGroupAddMembers *r);
NTSTATUS dcerpc_NetLocalGroupAddMembers_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetLocalGroupAddMembers_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetLocalGroupAddMembers *r);
struct tevent_req *dcerpc_NetLocalGroupAddMembers_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char * _server_name /* [in]  */,
						       const char * _group_name /* [in]  */,
						       uint32_t _level /* [in]  */,
						       uint8_t *_buffer /* [in] [ref] */,
						       uint32_t _total_entries /* [in]  */);
NTSTATUS dcerpc_NetLocalGroupAddMembers_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetLocalGroupAddMembers(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char * _server_name /* [in]  */,
					const char * _group_name /* [in]  */,
					uint32_t _level /* [in]  */,
					uint8_t *_buffer /* [in] [ref] */,
					uint32_t _total_entries /* [in]  */,
					enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetLocalGroupDelMembers_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetLocalGroupDelMembers *r);
NTSTATUS dcerpc_NetLocalGroupDelMembers_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetLocalGroupDelMembers_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetLocalGroupDelMembers *r);
struct tevent_req *dcerpc_NetLocalGroupDelMembers_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char * _server_name /* [in]  */,
						       const char * _group_name /* [in]  */,
						       uint32_t _level /* [in]  */,
						       uint8_t *_buffer /* [in] [ref] */,
						       uint32_t _total_entries /* [in]  */);
NTSTATUS dcerpc_NetLocalGroupDelMembers_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetLocalGroupDelMembers(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char * _server_name /* [in]  */,
					const char * _group_name /* [in]  */,
					uint32_t _level /* [in]  */,
					uint8_t *_buffer /* [in] [ref] */,
					uint32_t _total_entries /* [in]  */,
					enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetLocalGroupGetMembers_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetLocalGroupGetMembers *r);
NTSTATUS dcerpc_NetLocalGroupGetMembers_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetLocalGroupGetMembers_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetLocalGroupGetMembers *r);
struct tevent_req *dcerpc_NetLocalGroupGetMembers_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char * _server_name /* [in]  */,
						       const char * _local_group_name /* [in]  */,
						       uint32_t _level /* [in]  */,
						       uint8_t **_buffer /* [out] [ref] */,
						       uint32_t _prefmaxlen /* [in]  */,
						       uint32_t *_entries_read /* [out] [ref] */,
						       uint32_t *_total_entries /* [out] [ref] */,
						       uint32_t *_resume_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_NetLocalGroupGetMembers_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetLocalGroupGetMembers(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char * _server_name /* [in]  */,
					const char * _local_group_name /* [in]  */,
					uint32_t _level /* [in]  */,
					uint8_t **_buffer /* [out] [ref] */,
					uint32_t _prefmaxlen /* [in]  */,
					uint32_t *_entries_read /* [out] [ref] */,
					uint32_t *_total_entries /* [out] [ref] */,
					uint32_t *_resume_handle /* [in,out] [ref] */,
					enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetLocalGroupSetMembers_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetLocalGroupSetMembers *r);
NTSTATUS dcerpc_NetLocalGroupSetMembers_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetLocalGroupSetMembers_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetLocalGroupSetMembers *r);
struct tevent_req *dcerpc_NetLocalGroupSetMembers_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char * _server_name /* [in]  */,
						       const char * _group_name /* [in]  */,
						       uint32_t _level /* [in]  */,
						       uint8_t *_buffer /* [in] [ref] */,
						       uint32_t _total_entries /* [in]  */);
NTSTATUS dcerpc_NetLocalGroupSetMembers_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetLocalGroupSetMembers(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char * _server_name /* [in]  */,
					const char * _group_name /* [in]  */,
					uint32_t _level /* [in]  */,
					uint8_t *_buffer /* [in] [ref] */,
					uint32_t _total_entries /* [in]  */,
					enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetRemoteTOD_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetRemoteTOD *r);
NTSTATUS dcerpc_NetRemoteTOD_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetRemoteTOD_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetRemoteTOD *r);
struct tevent_req *dcerpc_NetRemoteTOD_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct dcerpc_binding_handle *h,
					    const char * _server_name /* [in]  */,
					    uint8_t **_buffer /* [out] [ref] */);
NTSTATUS dcerpc_NetRemoteTOD_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetRemoteTOD(struct dcerpc_binding_handle *h,
			     TALLOC_CTX *mem_ctx,
			     const char * _server_name /* [in]  */,
			     uint8_t **_buffer /* [out] [ref] */,
			     enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetShareAdd_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetShareAdd *r);
NTSTATUS dcerpc_NetShareAdd_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetShareAdd_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetShareAdd *r);
struct tevent_req *dcerpc_NetShareAdd_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   const char * _server_name /* [in]  */,
					   uint32_t _level /* [in]  */,
					   uint8_t *_buffer /* [in] [ref] */,
					   uint32_t *_parm_err /* [out] [ref] */);
NTSTATUS dcerpc_NetShareAdd_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetShareAdd(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    const char * _server_name /* [in]  */,
			    uint32_t _level /* [in]  */,
			    uint8_t *_buffer /* [in] [ref] */,
			    uint32_t *_parm_err /* [out] [ref] */,
			    enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetShareDel_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetShareDel *r);
NTSTATUS dcerpc_NetShareDel_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetShareDel_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetShareDel *r);
struct tevent_req *dcerpc_NetShareDel_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   const char * _server_name /* [in]  */,
					   const char * _net_name /* [in]  */,
					   uint32_t _reserved /* [in]  */);
NTSTATUS dcerpc_NetShareDel_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetShareDel(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    const char * _server_name /* [in]  */,
			    const char * _net_name /* [in]  */,
			    uint32_t _reserved /* [in]  */,
			    enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetShareEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetShareEnum *r);
NTSTATUS dcerpc_NetShareEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetShareEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetShareEnum *r);
struct tevent_req *dcerpc_NetShareEnum_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct dcerpc_binding_handle *h,
					    const char * _server_name /* [in]  */,
					    uint32_t _level /* [in]  */,
					    uint8_t **_buffer /* [out] [ref] */,
					    uint32_t _prefmaxlen /* [in]  */,
					    uint32_t *_entries_read /* [out] [ref] */,
					    uint32_t *_total_entries /* [out] [ref] */,
					    uint32_t *_resume_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_NetShareEnum_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetShareEnum(struct dcerpc_binding_handle *h,
			     TALLOC_CTX *mem_ctx,
			     const char * _server_name /* [in]  */,
			     uint32_t _level /* [in]  */,
			     uint8_t **_buffer /* [out] [ref] */,
			     uint32_t _prefmaxlen /* [in]  */,
			     uint32_t *_entries_read /* [out] [ref] */,
			     uint32_t *_total_entries /* [out] [ref] */,
			     uint32_t *_resume_handle /* [in,out] [ref] */,
			     enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetShareGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetShareGetInfo *r);
NTSTATUS dcerpc_NetShareGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetShareGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetShareGetInfo *r);
struct tevent_req *dcerpc_NetShareGetInfo_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       const char * _server_name /* [in]  */,
					       const char * _net_name /* [in]  */,
					       uint32_t _level /* [in]  */,
					       uint8_t **_buffer /* [out] [ref] */);
NTSTATUS dcerpc_NetShareGetInfo_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetShareGetInfo(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				const char * _server_name /* [in]  */,
				const char * _net_name /* [in]  */,
				uint32_t _level /* [in]  */,
				uint8_t **_buffer /* [out] [ref] */,
				enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetShareSetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetShareSetInfo *r);
NTSTATUS dcerpc_NetShareSetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetShareSetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetShareSetInfo *r);
struct tevent_req *dcerpc_NetShareSetInfo_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       const char * _server_name /* [in]  */,
					       const char * _net_name /* [in]  */,
					       uint32_t _level /* [in]  */,
					       uint8_t *_buffer /* [in] [ref] */,
					       uint32_t *_parm_err /* [out] [ref] */);
NTSTATUS dcerpc_NetShareSetInfo_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetShareSetInfo(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				const char * _server_name /* [in]  */,
				const char * _net_name /* [in]  */,
				uint32_t _level /* [in]  */,
				uint8_t *_buffer /* [in] [ref] */,
				uint32_t *_parm_err /* [out] [ref] */,
				enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetFileClose_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetFileClose *r);
NTSTATUS dcerpc_NetFileClose_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetFileClose_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetFileClose *r);
struct tevent_req *dcerpc_NetFileClose_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct dcerpc_binding_handle *h,
					    const char * _server_name /* [in]  */,
					    uint32_t _fileid /* [in]  */);
NTSTATUS dcerpc_NetFileClose_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetFileClose(struct dcerpc_binding_handle *h,
			     TALLOC_CTX *mem_ctx,
			     const char * _server_name /* [in]  */,
			     uint32_t _fileid /* [in]  */,
			     enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetFileGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetFileGetInfo *r);
NTSTATUS dcerpc_NetFileGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetFileGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetFileGetInfo *r);
struct tevent_req *dcerpc_NetFileGetInfo_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      const char * _server_name /* [in]  */,
					      uint32_t _fileid /* [in]  */,
					      uint32_t _level /* [in]  */,
					      uint8_t **_buffer /* [out] [ref] */);
NTSTATUS dcerpc_NetFileGetInfo_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetFileGetInfo(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       const char * _server_name /* [in]  */,
			       uint32_t _fileid /* [in]  */,
			       uint32_t _level /* [in]  */,
			       uint8_t **_buffer /* [out] [ref] */,
			       enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetFileEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetFileEnum *r);
NTSTATUS dcerpc_NetFileEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetFileEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetFileEnum *r);
struct tevent_req *dcerpc_NetFileEnum_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   const char * _server_name /* [in]  */,
					   const char * _base_path /* [in]  */,
					   const char * _user_name /* [in]  */,
					   uint32_t _level /* [in]  */,
					   uint8_t **_buffer /* [out] [ref] */,
					   uint32_t _prefmaxlen /* [in]  */,
					   uint32_t *_entries_read /* [out] [ref] */,
					   uint32_t *_total_entries /* [out] [ref] */,
					   uint32_t *_resume_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_NetFileEnum_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetFileEnum(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    const char * _server_name /* [in]  */,
			    const char * _base_path /* [in]  */,
			    const char * _user_name /* [in]  */,
			    uint32_t _level /* [in]  */,
			    uint8_t **_buffer /* [out] [ref] */,
			    uint32_t _prefmaxlen /* [in]  */,
			    uint32_t *_entries_read /* [out] [ref] */,
			    uint32_t *_total_entries /* [out] [ref] */,
			    uint32_t *_resume_handle /* [in,out] [ref] */,
			    enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetShutdownInit_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetShutdownInit *r);
NTSTATUS dcerpc_NetShutdownInit_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetShutdownInit_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetShutdownInit *r);
struct tevent_req *dcerpc_NetShutdownInit_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       const char * _server_name /* [in]  */,
					       const char * _message /* [in]  */,
					       uint32_t _timeout /* [in]  */,
					       uint8_t _force_apps /* [in]  */,
					       uint8_t _do_reboot /* [in]  */);
NTSTATUS dcerpc_NetShutdownInit_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetShutdownInit(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				const char * _server_name /* [in]  */,
				const char * _message /* [in]  */,
				uint32_t _timeout /* [in]  */,
				uint8_t _force_apps /* [in]  */,
				uint8_t _do_reboot /* [in]  */,
				enum NET_API_STATUS *result);

struct tevent_req *dcerpc_NetShutdownAbort_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NetShutdownAbort *r);
NTSTATUS dcerpc_NetShutdownAbort_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NetShutdownAbort_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NetShutdownAbort *r);
struct tevent_req *dcerpc_NetShutdownAbort_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char * _server_name /* [in]  */);
NTSTATUS dcerpc_NetShutdownAbort_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      enum NET_API_STATUS *result);
NTSTATUS dcerpc_NetShutdownAbort(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char * _server_name /* [in]  */,
				 enum NET_API_STATUS *result);

struct tevent_req *dcerpc_I_NetLogonControl_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct I_NetLogonControl *r);
NTSTATUS dcerpc_I_NetLogonControl_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_I_NetLogonControl_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct I_NetLogonControl *r);
struct tevent_req *dcerpc_I_NetLogonControl_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char * _server_name /* [in]  */,
						 uint32_t _function_code /* [in]  */,
						 uint32_t _query_level /* [in]  */,
						 uint8_t **_buffer /* [out] [ref] */);
NTSTATUS dcerpc_I_NetLogonControl_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       enum NET_API_STATUS *result);
NTSTATUS dcerpc_I_NetLogonControl(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char * _server_name /* [in]  */,
				  uint32_t _function_code /* [in]  */,
				  uint32_t _query_level /* [in]  */,
				  uint8_t **_buffer /* [out] [ref] */,
				  enum NET_API_STATUS *result);

struct tevent_req *dcerpc_I_NetLogonControl2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct I_NetLogonControl2 *r);
NTSTATUS dcerpc_I_NetLogonControl2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_I_NetLogonControl2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct I_NetLogonControl2 *r);
struct tevent_req *dcerpc_I_NetLogonControl2_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char * _server_name /* [in]  */,
						  uint32_t _function_code /* [in]  */,
						  uint32_t _query_level /* [in]  */,
						  uint8_t *_data /* [in] [unique] */,
						  uint8_t **_buffer /* [out] [ref] */);
NTSTATUS dcerpc_I_NetLogonControl2_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					enum NET_API_STATUS *result);
NTSTATUS dcerpc_I_NetLogonControl2(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char * _server_name /* [in]  */,
				   uint32_t _function_code /* [in]  */,
				   uint32_t _query_level /* [in]  */,
				   uint8_t *_data /* [in] [unique] */,
				   uint8_t **_buffer /* [out] [ref] */,
				   enum NET_API_STATUS *result);

#endif /* _HEADER_RPC_libnetapi */
