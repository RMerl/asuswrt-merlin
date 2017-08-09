#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/netlogon.h"
#ifndef _HEADER_RPC_netlogon
#define _HEADER_RPC_netlogon

extern const struct ndr_interface_table ndr_table_netlogon;

struct tevent_req *dcerpc_netr_LogonUasLogon_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_LogonUasLogon *r);
NTSTATUS dcerpc_netr_LogonUasLogon_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_LogonUasLogon_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_LogonUasLogon *r);
struct tevent_req *dcerpc_netr_LogonUasLogon_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_server_name /* [in] [charset(UTF16),unique] */,
						  const char *_account_name /* [in] [charset(UTF16),ref] */,
						  const char *_workstation /* [in] [charset(UTF16),ref] */,
						  struct netr_UasInfo **_info /* [out] [ref] */);
NTSTATUS dcerpc_netr_LogonUasLogon_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_netr_LogonUasLogon(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_server_name /* [in] [charset(UTF16),unique] */,
				   const char *_account_name /* [in] [charset(UTF16),ref] */,
				   const char *_workstation /* [in] [charset(UTF16),ref] */,
				   struct netr_UasInfo **_info /* [out] [ref] */,
				   WERROR *result);

struct tevent_req *dcerpc_netr_LogonUasLogoff_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_LogonUasLogoff *r);
NTSTATUS dcerpc_netr_LogonUasLogoff_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_LogonUasLogoff_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_LogonUasLogoff *r);
struct tevent_req *dcerpc_netr_LogonUasLogoff_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   const char *_server_name /* [in] [unique,charset(UTF16)] */,
						   const char *_account_name /* [in] [ref,charset(UTF16)] */,
						   const char *_workstation /* [in] [charset(UTF16),ref] */,
						   struct netr_UasLogoffInfo *_info /* [out] [ref] */);
NTSTATUS dcerpc_netr_LogonUasLogoff_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_netr_LogonUasLogoff(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    const char *_server_name /* [in] [unique,charset(UTF16)] */,
				    const char *_account_name /* [in] [ref,charset(UTF16)] */,
				    const char *_workstation /* [in] [charset(UTF16),ref] */,
				    struct netr_UasLogoffInfo *_info /* [out] [ref] */,
				    WERROR *result);

struct tevent_req *dcerpc_netr_LogonSamLogon_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_LogonSamLogon *r);
NTSTATUS dcerpc_netr_LogonSamLogon_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_LogonSamLogon_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_LogonSamLogon *r);
struct tevent_req *dcerpc_netr_LogonSamLogon_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_server_name /* [in] [charset(UTF16),unique] */,
						  const char *_computer_name /* [in] [charset(UTF16),unique] */,
						  struct netr_Authenticator *_credential /* [in] [unique] */,
						  struct netr_Authenticator *_return_authenticator /* [in,out] [unique] */,
						  enum netr_LogonInfoClass _logon_level /* [in]  */,
						  union netr_LogonLevel *_logon /* [in] [switch_is(logon_level),ref] */,
						  uint16_t _validation_level /* [in]  */,
						  union netr_Validation *_validation /* [out] [ref,switch_is(validation_level)] */,
						  uint8_t *_authoritative /* [out] [ref] */);
NTSTATUS dcerpc_netr_LogonSamLogon_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					NTSTATUS *result);
NTSTATUS dcerpc_netr_LogonSamLogon(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_server_name /* [in] [charset(UTF16),unique] */,
				   const char *_computer_name /* [in] [charset(UTF16),unique] */,
				   struct netr_Authenticator *_credential /* [in] [unique] */,
				   struct netr_Authenticator *_return_authenticator /* [in,out] [unique] */,
				   enum netr_LogonInfoClass _logon_level /* [in]  */,
				   union netr_LogonLevel *_logon /* [in] [switch_is(logon_level),ref] */,
				   uint16_t _validation_level /* [in]  */,
				   union netr_Validation *_validation /* [out] [ref,switch_is(validation_level)] */,
				   uint8_t *_authoritative /* [out] [ref] */,
				   NTSTATUS *result);

struct tevent_req *dcerpc_netr_LogonSamLogoff_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_LogonSamLogoff *r);
NTSTATUS dcerpc_netr_LogonSamLogoff_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_LogonSamLogoff_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_LogonSamLogoff *r);
struct tevent_req *dcerpc_netr_LogonSamLogoff_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   const char *_server_name /* [in] [charset(UTF16),unique] */,
						   const char *_computer_name /* [in] [charset(UTF16),unique] */,
						   struct netr_Authenticator *_credential /* [in] [unique] */,
						   struct netr_Authenticator *_return_authenticator /* [in,out] [unique] */,
						   enum netr_LogonInfoClass _logon_level /* [in]  */,
						   union netr_LogonLevel _logon /* [in] [switch_is(logon_level)] */);
NTSTATUS dcerpc_netr_LogonSamLogoff_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS dcerpc_netr_LogonSamLogoff(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    const char *_server_name /* [in] [charset(UTF16),unique] */,
				    const char *_computer_name /* [in] [charset(UTF16),unique] */,
				    struct netr_Authenticator *_credential /* [in] [unique] */,
				    struct netr_Authenticator *_return_authenticator /* [in,out] [unique] */,
				    enum netr_LogonInfoClass _logon_level /* [in]  */,
				    union netr_LogonLevel _logon /* [in] [switch_is(logon_level)] */,
				    NTSTATUS *result);

struct tevent_req *dcerpc_netr_ServerReqChallenge_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_ServerReqChallenge *r);
NTSTATUS dcerpc_netr_ServerReqChallenge_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_ServerReqChallenge_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_ServerReqChallenge *r);
struct tevent_req *dcerpc_netr_ServerReqChallenge_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char *_server_name /* [in] [charset(UTF16),unique] */,
						       const char *_computer_name /* [in] [ref,charset(UTF16)] */,
						       struct netr_Credential *_credentials /* [in] [ref] */,
						       struct netr_Credential *_return_credentials /* [out] [ref] */);
NTSTATUS dcerpc_netr_ServerReqChallenge_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS dcerpc_netr_ServerReqChallenge(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char *_server_name /* [in] [charset(UTF16),unique] */,
					const char *_computer_name /* [in] [ref,charset(UTF16)] */,
					struct netr_Credential *_credentials /* [in] [ref] */,
					struct netr_Credential *_return_credentials /* [out] [ref] */,
					NTSTATUS *result);

struct tevent_req *dcerpc_netr_ServerAuthenticate_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_ServerAuthenticate *r);
NTSTATUS dcerpc_netr_ServerAuthenticate_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_ServerAuthenticate_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_ServerAuthenticate *r);
struct tevent_req *dcerpc_netr_ServerAuthenticate_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char *_server_name /* [in] [unique,charset(UTF16)] */,
						       const char *_account_name /* [in] [ref,charset(UTF16)] */,
						       enum netr_SchannelType _secure_channel_type /* [in]  */,
						       const char *_computer_name /* [in] [charset(UTF16),ref] */,
						       struct netr_Credential *_credentials /* [in] [ref] */,
						       struct netr_Credential *_return_credentials /* [out] [ref] */);
NTSTATUS dcerpc_netr_ServerAuthenticate_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS dcerpc_netr_ServerAuthenticate(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char *_server_name /* [in] [unique,charset(UTF16)] */,
					const char *_account_name /* [in] [ref,charset(UTF16)] */,
					enum netr_SchannelType _secure_channel_type /* [in]  */,
					const char *_computer_name /* [in] [charset(UTF16),ref] */,
					struct netr_Credential *_credentials /* [in] [ref] */,
					struct netr_Credential *_return_credentials /* [out] [ref] */,
					NTSTATUS *result);

struct tevent_req *dcerpc_netr_ServerPasswordSet_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_ServerPasswordSet *r);
NTSTATUS dcerpc_netr_ServerPasswordSet_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_ServerPasswordSet_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_ServerPasswordSet *r);
struct tevent_req *dcerpc_netr_ServerPasswordSet_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      const char *_server_name /* [in] [charset(UTF16),unique] */,
						      const char *_account_name /* [in] [charset(UTF16),ref] */,
						      enum netr_SchannelType _secure_channel_type /* [in]  */,
						      const char *_computer_name /* [in] [ref,charset(UTF16)] */,
						      struct netr_Authenticator *_credential /* [in] [ref] */,
						      struct netr_Authenticator *_return_authenticator /* [out] [ref] */,
						      struct samr_Password *_new_password /* [in] [ref] */);
NTSTATUS dcerpc_netr_ServerPasswordSet_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_netr_ServerPasswordSet(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       const char *_server_name /* [in] [charset(UTF16),unique] */,
				       const char *_account_name /* [in] [charset(UTF16),ref] */,
				       enum netr_SchannelType _secure_channel_type /* [in]  */,
				       const char *_computer_name /* [in] [ref,charset(UTF16)] */,
				       struct netr_Authenticator *_credential /* [in] [ref] */,
				       struct netr_Authenticator *_return_authenticator /* [out] [ref] */,
				       struct samr_Password *_new_password /* [in] [ref] */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_netr_DatabaseDeltas_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DatabaseDeltas *r);
NTSTATUS dcerpc_netr_DatabaseDeltas_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DatabaseDeltas_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DatabaseDeltas *r);
struct tevent_req *dcerpc_netr_DatabaseDeltas_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   const char *_logon_server /* [in] [charset(UTF16),ref] */,
						   const char *_computername /* [in] [ref,charset(UTF16)] */,
						   struct netr_Authenticator *_credential /* [in] [ref] */,
						   struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
						   enum netr_SamDatabaseID _database_id /* [in]  */,
						   uint64_t *_sequence_num /* [in,out] [ref] */,
						   struct netr_DELTA_ENUM_ARRAY **_delta_enum_array /* [out] [ref] */,
						   uint32_t _preferredmaximumlength /* [in]  */);
NTSTATUS dcerpc_netr_DatabaseDeltas_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 NTSTATUS *result);
NTSTATUS dcerpc_netr_DatabaseDeltas(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    const char *_logon_server /* [in] [charset(UTF16),ref] */,
				    const char *_computername /* [in] [ref,charset(UTF16)] */,
				    struct netr_Authenticator *_credential /* [in] [ref] */,
				    struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
				    enum netr_SamDatabaseID _database_id /* [in]  */,
				    uint64_t *_sequence_num /* [in,out] [ref] */,
				    struct netr_DELTA_ENUM_ARRAY **_delta_enum_array /* [out] [ref] */,
				    uint32_t _preferredmaximumlength /* [in]  */,
				    NTSTATUS *result);

struct tevent_req *dcerpc_netr_DatabaseSync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DatabaseSync *r);
NTSTATUS dcerpc_netr_DatabaseSync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DatabaseSync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DatabaseSync *r);
struct tevent_req *dcerpc_netr_DatabaseSync_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char *_logon_server /* [in] [ref,charset(UTF16)] */,
						 const char *_computername /* [in] [ref,charset(UTF16)] */,
						 struct netr_Authenticator *_credential /* [in] [ref] */,
						 struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
						 enum netr_SamDatabaseID _database_id /* [in]  */,
						 uint32_t *_sync_context /* [in,out] [ref] */,
						 struct netr_DELTA_ENUM_ARRAY **_delta_enum_array /* [out] [ref] */,
						 uint32_t _preferredmaximumlength /* [in]  */);
NTSTATUS dcerpc_netr_DatabaseSync_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_netr_DatabaseSync(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char *_logon_server /* [in] [ref,charset(UTF16)] */,
				  const char *_computername /* [in] [ref,charset(UTF16)] */,
				  struct netr_Authenticator *_credential /* [in] [ref] */,
				  struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
				  enum netr_SamDatabaseID _database_id /* [in]  */,
				  uint32_t *_sync_context /* [in,out] [ref] */,
				  struct netr_DELTA_ENUM_ARRAY **_delta_enum_array /* [out] [ref] */,
				  uint32_t _preferredmaximumlength /* [in]  */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_netr_AccountDeltas_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_AccountDeltas *r);
NTSTATUS dcerpc_netr_AccountDeltas_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_AccountDeltas_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_AccountDeltas *r);
struct tevent_req *dcerpc_netr_AccountDeltas_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_logon_server /* [in] [charset(UTF16),unique] */,
						  const char *_computername /* [in] [charset(UTF16),ref] */,
						  struct netr_Authenticator _credential /* [in]  */,
						  struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
						  struct netr_UAS_INFO_0 _uas /* [in]  */,
						  uint32_t _count /* [in]  */,
						  uint32_t _level /* [in]  */,
						  uint32_t _buffersize /* [in]  */,
						  struct netr_AccountBuffer *_buffer /* [out] [subcontext(4),ref] */,
						  uint32_t *_count_returned /* [out] [ref] */,
						  uint32_t *_total_entries /* [out] [ref] */,
						  struct netr_UAS_INFO_0 *_recordid /* [out] [ref] */);
NTSTATUS dcerpc_netr_AccountDeltas_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					NTSTATUS *result);
NTSTATUS dcerpc_netr_AccountDeltas(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_logon_server /* [in] [charset(UTF16),unique] */,
				   const char *_computername /* [in] [charset(UTF16),ref] */,
				   struct netr_Authenticator _credential /* [in]  */,
				   struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
				   struct netr_UAS_INFO_0 _uas /* [in]  */,
				   uint32_t _count /* [in]  */,
				   uint32_t _level /* [in]  */,
				   uint32_t _buffersize /* [in]  */,
				   struct netr_AccountBuffer *_buffer /* [out] [subcontext(4),ref] */,
				   uint32_t *_count_returned /* [out] [ref] */,
				   uint32_t *_total_entries /* [out] [ref] */,
				   struct netr_UAS_INFO_0 *_recordid /* [out] [ref] */,
				   NTSTATUS *result);

struct tevent_req *dcerpc_netr_AccountSync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_AccountSync *r);
NTSTATUS dcerpc_netr_AccountSync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_AccountSync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_AccountSync *r);
struct tevent_req *dcerpc_netr_AccountSync_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char *_logon_server /* [in] [charset(UTF16),unique] */,
						const char *_computername /* [in] [ref,charset(UTF16)] */,
						struct netr_Authenticator _credential /* [in]  */,
						struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
						uint32_t _reference /* [in]  */,
						uint32_t _level /* [in]  */,
						uint32_t _buffersize /* [in]  */,
						struct netr_AccountBuffer *_buffer /* [out] [ref,subcontext(4)] */,
						uint32_t *_count_returned /* [out] [ref] */,
						uint32_t *_total_entries /* [out] [ref] */,
						uint32_t *_next_reference /* [out] [ref] */,
						struct netr_UAS_INFO_0 *_recordid /* [in,out] [ref] */);
NTSTATUS dcerpc_netr_AccountSync_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_netr_AccountSync(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char *_logon_server /* [in] [charset(UTF16),unique] */,
				 const char *_computername /* [in] [ref,charset(UTF16)] */,
				 struct netr_Authenticator _credential /* [in]  */,
				 struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
				 uint32_t _reference /* [in]  */,
				 uint32_t _level /* [in]  */,
				 uint32_t _buffersize /* [in]  */,
				 struct netr_AccountBuffer *_buffer /* [out] [ref,subcontext(4)] */,
				 uint32_t *_count_returned /* [out] [ref] */,
				 uint32_t *_total_entries /* [out] [ref] */,
				 uint32_t *_next_reference /* [out] [ref] */,
				 struct netr_UAS_INFO_0 *_recordid /* [in,out] [ref] */,
				 NTSTATUS *result);

struct tevent_req *dcerpc_netr_GetDcName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_GetDcName *r);
NTSTATUS dcerpc_netr_GetDcName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_GetDcName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_GetDcName *r);
struct tevent_req *dcerpc_netr_GetDcName_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      const char *_logon_server /* [in] [ref,charset(UTF16)] */,
					      const char *_domainname /* [in] [unique,charset(UTF16)] */,
					      const char **_dcname /* [out] [ref,charset(UTF16)] */);
NTSTATUS dcerpc_netr_GetDcName_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_netr_GetDcName(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       const char *_logon_server /* [in] [ref,charset(UTF16)] */,
			       const char *_domainname /* [in] [unique,charset(UTF16)] */,
			       const char **_dcname /* [out] [ref,charset(UTF16)] */,
			       WERROR *result);

struct tevent_req *dcerpc_netr_LogonControl_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_LogonControl *r);
NTSTATUS dcerpc_netr_LogonControl_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_LogonControl_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_LogonControl *r);
struct tevent_req *dcerpc_netr_LogonControl_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char *_logon_server /* [in] [charset(UTF16),unique] */,
						 enum netr_LogonControlCode _function_code /* [in]  */,
						 uint32_t _level /* [in]  */,
						 union netr_CONTROL_QUERY_INFORMATION *_query /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_netr_LogonControl_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_netr_LogonControl(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char *_logon_server /* [in] [charset(UTF16),unique] */,
				  enum netr_LogonControlCode _function_code /* [in]  */,
				  uint32_t _level /* [in]  */,
				  union netr_CONTROL_QUERY_INFORMATION *_query /* [out] [ref,switch_is(level)] */,
				  WERROR *result);

struct tevent_req *dcerpc_netr_GetAnyDCName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_GetAnyDCName *r);
NTSTATUS dcerpc_netr_GetAnyDCName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_GetAnyDCName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_GetAnyDCName *r);
struct tevent_req *dcerpc_netr_GetAnyDCName_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char *_logon_server /* [in] [charset(UTF16),unique] */,
						 const char *_domainname /* [in] [charset(UTF16),unique] */,
						 const char **_dcname /* [out] [ref,charset(UTF16)] */);
NTSTATUS dcerpc_netr_GetAnyDCName_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_netr_GetAnyDCName(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char *_logon_server /* [in] [charset(UTF16),unique] */,
				  const char *_domainname /* [in] [charset(UTF16),unique] */,
				  const char **_dcname /* [out] [ref,charset(UTF16)] */,
				  WERROR *result);

struct tevent_req *dcerpc_netr_LogonControl2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_LogonControl2 *r);
NTSTATUS dcerpc_netr_LogonControl2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_LogonControl2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_LogonControl2 *r);
struct tevent_req *dcerpc_netr_LogonControl2_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_logon_server /* [in] [unique,charset(UTF16)] */,
						  enum netr_LogonControlCode _function_code /* [in]  */,
						  uint32_t _level /* [in]  */,
						  union netr_CONTROL_DATA_INFORMATION *_data /* [in] [switch_is(function_code),ref] */,
						  union netr_CONTROL_QUERY_INFORMATION *_query /* [out] [switch_is(level),ref] */);
NTSTATUS dcerpc_netr_LogonControl2_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_netr_LogonControl2(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_logon_server /* [in] [unique,charset(UTF16)] */,
				   enum netr_LogonControlCode _function_code /* [in]  */,
				   uint32_t _level /* [in]  */,
				   union netr_CONTROL_DATA_INFORMATION *_data /* [in] [switch_is(function_code),ref] */,
				   union netr_CONTROL_QUERY_INFORMATION *_query /* [out] [switch_is(level),ref] */,
				   WERROR *result);

struct tevent_req *dcerpc_netr_ServerAuthenticate2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_ServerAuthenticate2 *r);
NTSTATUS dcerpc_netr_ServerAuthenticate2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_ServerAuthenticate2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_ServerAuthenticate2 *r);
struct tevent_req *dcerpc_netr_ServerAuthenticate2_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							const char *_server_name /* [in] [unique,charset(UTF16)] */,
							const char *_account_name /* [in] [ref,charset(UTF16)] */,
							enum netr_SchannelType _secure_channel_type /* [in]  */,
							const char *_computer_name /* [in] [ref,charset(UTF16)] */,
							struct netr_Credential *_credentials /* [in] [ref] */,
							struct netr_Credential *_return_credentials /* [out] [ref] */,
							uint32_t *_negotiate_flags /* [in,out] [ref] */);
NTSTATUS dcerpc_netr_ServerAuthenticate2_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS dcerpc_netr_ServerAuthenticate2(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 const char *_server_name /* [in] [unique,charset(UTF16)] */,
					 const char *_account_name /* [in] [ref,charset(UTF16)] */,
					 enum netr_SchannelType _secure_channel_type /* [in]  */,
					 const char *_computer_name /* [in] [ref,charset(UTF16)] */,
					 struct netr_Credential *_credentials /* [in] [ref] */,
					 struct netr_Credential *_return_credentials /* [out] [ref] */,
					 uint32_t *_negotiate_flags /* [in,out] [ref] */,
					 NTSTATUS *result);

struct tevent_req *dcerpc_netr_DatabaseSync2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DatabaseSync2 *r);
NTSTATUS dcerpc_netr_DatabaseSync2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DatabaseSync2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DatabaseSync2 *r);
struct tevent_req *dcerpc_netr_DatabaseSync2_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_logon_server /* [in] [ref,charset(UTF16)] */,
						  const char *_computername /* [in] [charset(UTF16),ref] */,
						  struct netr_Authenticator *_credential /* [in] [ref] */,
						  struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
						  enum netr_SamDatabaseID _database_id /* [in]  */,
						  enum SyncStateEnum _restart_state /* [in]  */,
						  uint32_t *_sync_context /* [in,out] [ref] */,
						  struct netr_DELTA_ENUM_ARRAY **_delta_enum_array /* [out] [ref] */,
						  uint32_t _preferredmaximumlength /* [in]  */);
NTSTATUS dcerpc_netr_DatabaseSync2_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					NTSTATUS *result);
NTSTATUS dcerpc_netr_DatabaseSync2(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_logon_server /* [in] [ref,charset(UTF16)] */,
				   const char *_computername /* [in] [charset(UTF16),ref] */,
				   struct netr_Authenticator *_credential /* [in] [ref] */,
				   struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
				   enum netr_SamDatabaseID _database_id /* [in]  */,
				   enum SyncStateEnum _restart_state /* [in]  */,
				   uint32_t *_sync_context /* [in,out] [ref] */,
				   struct netr_DELTA_ENUM_ARRAY **_delta_enum_array /* [out] [ref] */,
				   uint32_t _preferredmaximumlength /* [in]  */,
				   NTSTATUS *result);

struct tevent_req *dcerpc_netr_DatabaseRedo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DatabaseRedo *r);
NTSTATUS dcerpc_netr_DatabaseRedo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DatabaseRedo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DatabaseRedo *r);
struct tevent_req *dcerpc_netr_DatabaseRedo_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char *_logon_server /* [in] [charset(UTF16),ref] */,
						 const char *_computername /* [in] [ref,charset(UTF16)] */,
						 struct netr_Authenticator *_credential /* [in] [ref] */,
						 struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
						 struct netr_ChangeLogEntry _change_log_entry /* [in] [subcontext(4)] */,
						 uint32_t _change_log_entry_size /* [in] [value(ndr_size_netr_ChangeLogEntry(&change_log_entry,ndr->flags))] */,
						 struct netr_DELTA_ENUM_ARRAY **_delta_enum_array /* [out] [ref] */);
NTSTATUS dcerpc_netr_DatabaseRedo_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       NTSTATUS *result);
NTSTATUS dcerpc_netr_DatabaseRedo(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char *_logon_server /* [in] [charset(UTF16),ref] */,
				  const char *_computername /* [in] [ref,charset(UTF16)] */,
				  struct netr_Authenticator *_credential /* [in] [ref] */,
				  struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
				  struct netr_ChangeLogEntry _change_log_entry /* [in] [subcontext(4)] */,
				  uint32_t _change_log_entry_size /* [in] [value(ndr_size_netr_ChangeLogEntry(&change_log_entry,ndr->flags))] */,
				  struct netr_DELTA_ENUM_ARRAY **_delta_enum_array /* [out] [ref] */,
				  NTSTATUS *result);

struct tevent_req *dcerpc_netr_LogonControl2Ex_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_LogonControl2Ex *r);
NTSTATUS dcerpc_netr_LogonControl2Ex_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_LogonControl2Ex_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_LogonControl2Ex *r);
struct tevent_req *dcerpc_netr_LogonControl2Ex_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    const char *_logon_server /* [in] [unique,charset(UTF16)] */,
						    enum netr_LogonControlCode _function_code /* [in]  */,
						    uint32_t _level /* [in]  */,
						    union netr_CONTROL_DATA_INFORMATION *_data /* [in] [switch_is(function_code),ref] */,
						    union netr_CONTROL_QUERY_INFORMATION *_query /* [out] [switch_is(level),ref] */);
NTSTATUS dcerpc_netr_LogonControl2Ex_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_netr_LogonControl2Ex(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char *_logon_server /* [in] [unique,charset(UTF16)] */,
				     enum netr_LogonControlCode _function_code /* [in]  */,
				     uint32_t _level /* [in]  */,
				     union netr_CONTROL_DATA_INFORMATION *_data /* [in] [switch_is(function_code),ref] */,
				     union netr_CONTROL_QUERY_INFORMATION *_query /* [out] [switch_is(level),ref] */,
				     WERROR *result);

struct tevent_req *dcerpc_netr_NetrEnumerateTrustedDomains_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_NetrEnumerateTrustedDomains *r);
NTSTATUS dcerpc_netr_NetrEnumerateTrustedDomains_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_NetrEnumerateTrustedDomains_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_NetrEnumerateTrustedDomains *r);
struct tevent_req *dcerpc_netr_NetrEnumerateTrustedDomains_send(TALLOC_CTX *mem_ctx,
								struct tevent_context *ev,
								struct dcerpc_binding_handle *h,
								const char *_server_name /* [in] [charset(UTF16),unique] */,
								struct netr_Blob *_trusted_domains_blob /* [out] [ref] */);
NTSTATUS dcerpc_netr_NetrEnumerateTrustedDomains_recv(struct tevent_req *req,
						      TALLOC_CTX *mem_ctx,
						      NTSTATUS *result);
NTSTATUS dcerpc_netr_NetrEnumerateTrustedDomains(struct dcerpc_binding_handle *h,
						 TALLOC_CTX *mem_ctx,
						 const char *_server_name /* [in] [charset(UTF16),unique] */,
						 struct netr_Blob *_trusted_domains_blob /* [out] [ref] */,
						 NTSTATUS *result);

struct tevent_req *dcerpc_netr_DsRGetDCName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DsRGetDCName *r);
NTSTATUS dcerpc_netr_DsRGetDCName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DsRGetDCName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DsRGetDCName *r);
struct tevent_req *dcerpc_netr_DsRGetDCName_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						 const char *_domain_name /* [in] [unique,charset(UTF16)] */,
						 struct GUID *_domain_guid /* [in] [unique] */,
						 struct GUID *_site_guid /* [in] [unique] */,
						 uint32_t _flags /* [in]  */,
						 struct netr_DsRGetDCNameInfo **_info /* [out] [ref] */);
NTSTATUS dcerpc_netr_DsRGetDCName_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_netr_DsRGetDCName(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char *_server_unc /* [in] [unique,charset(UTF16)] */,
				  const char *_domain_name /* [in] [unique,charset(UTF16)] */,
				  struct GUID *_domain_guid /* [in] [unique] */,
				  struct GUID *_site_guid /* [in] [unique] */,
				  uint32_t _flags /* [in]  */,
				  struct netr_DsRGetDCNameInfo **_info /* [out] [ref] */,
				  WERROR *result);

struct tevent_req *dcerpc_netr_LogonGetCapabilities_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_LogonGetCapabilities *r);
NTSTATUS dcerpc_netr_LogonGetCapabilities_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_LogonGetCapabilities_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_LogonGetCapabilities *r);
struct tevent_req *dcerpc_netr_LogonGetCapabilities_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 const char *_server_name /* [in] [ref,charset(UTF16)] */,
							 const char *_computer_name /* [in] [charset(UTF16),unique] */,
							 struct netr_Authenticator *_credential /* [in] [ref] */,
							 struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
							 uint32_t _query_level /* [in]  */,
							 union netr_Capabilities *_capabilities /* [out] [switch_is(query_level),ref] */);
NTSTATUS dcerpc_netr_LogonGetCapabilities_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       NTSTATUS *result);
NTSTATUS dcerpc_netr_LogonGetCapabilities(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  const char *_server_name /* [in] [ref,charset(UTF16)] */,
					  const char *_computer_name /* [in] [charset(UTF16),unique] */,
					  struct netr_Authenticator *_credential /* [in] [ref] */,
					  struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
					  uint32_t _query_level /* [in]  */,
					  union netr_Capabilities *_capabilities /* [out] [switch_is(query_level),ref] */,
					  NTSTATUS *result);

struct tevent_req *dcerpc_netr_LogonGetTrustRid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_LogonGetTrustRid *r);
NTSTATUS dcerpc_netr_LogonGetTrustRid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_LogonGetTrustRid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_LogonGetTrustRid *r);
struct tevent_req *dcerpc_netr_LogonGetTrustRid_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     const char *_server_name /* [in] [charset(UTF16),unique] */,
						     const char *_domain_name /* [in] [unique,charset(UTF16)] */,
						     uint32_t *_rid /* [out] [ref] */);
NTSTATUS dcerpc_netr_LogonGetTrustRid_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_netr_LogonGetTrustRid(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      const char *_server_name /* [in] [charset(UTF16),unique] */,
				      const char *_domain_name /* [in] [unique,charset(UTF16)] */,
				      uint32_t *_rid /* [out] [ref] */,
				      WERROR *result);

struct tevent_req *dcerpc_netr_ServerAuthenticate3_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_ServerAuthenticate3 *r);
NTSTATUS dcerpc_netr_ServerAuthenticate3_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_ServerAuthenticate3_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_ServerAuthenticate3 *r);
struct tevent_req *dcerpc_netr_ServerAuthenticate3_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							const char *_server_name /* [in] [charset(UTF16),unique] */,
							const char *_account_name /* [in] [charset(UTF16),ref] */,
							enum netr_SchannelType _secure_channel_type /* [in]  */,
							const char *_computer_name /* [in] [ref,charset(UTF16)] */,
							struct netr_Credential *_credentials /* [in] [ref] */,
							struct netr_Credential *_return_credentials /* [out] [ref] */,
							uint32_t *_negotiate_flags /* [in,out] [ref] */,
							uint32_t *_rid /* [out] [ref] */);
NTSTATUS dcerpc_netr_ServerAuthenticate3_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS dcerpc_netr_ServerAuthenticate3(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 const char *_server_name /* [in] [charset(UTF16),unique] */,
					 const char *_account_name /* [in] [charset(UTF16),ref] */,
					 enum netr_SchannelType _secure_channel_type /* [in]  */,
					 const char *_computer_name /* [in] [ref,charset(UTF16)] */,
					 struct netr_Credential *_credentials /* [in] [ref] */,
					 struct netr_Credential *_return_credentials /* [out] [ref] */,
					 uint32_t *_negotiate_flags /* [in,out] [ref] */,
					 uint32_t *_rid /* [out] [ref] */,
					 NTSTATUS *result);

struct tevent_req *dcerpc_netr_DsRGetDCNameEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DsRGetDCNameEx *r);
NTSTATUS dcerpc_netr_DsRGetDCNameEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DsRGetDCNameEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DsRGetDCNameEx *r);
struct tevent_req *dcerpc_netr_DsRGetDCNameEx_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						   const char *_domain_name /* [in] [charset(UTF16),unique] */,
						   struct GUID *_domain_guid /* [in] [unique] */,
						   const char *_site_name /* [in] [unique,charset(UTF16)] */,
						   uint32_t _flags /* [in]  */,
						   struct netr_DsRGetDCNameInfo **_info /* [out] [ref] */);
NTSTATUS dcerpc_netr_DsRGetDCNameEx_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_netr_DsRGetDCNameEx(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    const char *_server_unc /* [in] [unique,charset(UTF16)] */,
				    const char *_domain_name /* [in] [charset(UTF16),unique] */,
				    struct GUID *_domain_guid /* [in] [unique] */,
				    const char *_site_name /* [in] [unique,charset(UTF16)] */,
				    uint32_t _flags /* [in]  */,
				    struct netr_DsRGetDCNameInfo **_info /* [out] [ref] */,
				    WERROR *result);

struct tevent_req *dcerpc_netr_DsRGetSiteName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DsRGetSiteName *r);
NTSTATUS dcerpc_netr_DsRGetSiteName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DsRGetSiteName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DsRGetSiteName *r);
struct tevent_req *dcerpc_netr_DsRGetSiteName_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   const char *_computer_name /* [in] [unique,charset(UTF16)] */,
						   const char **_site /* [out] [charset(UTF16),ref] */);
NTSTATUS dcerpc_netr_DsRGetSiteName_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_netr_DsRGetSiteName(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    const char *_computer_name /* [in] [unique,charset(UTF16)] */,
				    const char **_site /* [out] [charset(UTF16),ref] */,
				    WERROR *result);

struct tevent_req *dcerpc_netr_LogonGetDomainInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_LogonGetDomainInfo *r);
NTSTATUS dcerpc_netr_LogonGetDomainInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_LogonGetDomainInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_LogonGetDomainInfo *r);
struct tevent_req *dcerpc_netr_LogonGetDomainInfo_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char *_server_name /* [in] [ref,charset(UTF16)] */,
						       const char *_computer_name /* [in] [unique,charset(UTF16)] */,
						       struct netr_Authenticator *_credential /* [in] [ref] */,
						       struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
						       uint32_t _level /* [in]  */,
						       union netr_WorkstationInfo *_query /* [in] [switch_is(level),ref] */,
						       union netr_DomainInfo *_info /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_netr_LogonGetDomainInfo_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS dcerpc_netr_LogonGetDomainInfo(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char *_server_name /* [in] [ref,charset(UTF16)] */,
					const char *_computer_name /* [in] [unique,charset(UTF16)] */,
					struct netr_Authenticator *_credential /* [in] [ref] */,
					struct netr_Authenticator *_return_authenticator /* [in,out] [ref] */,
					uint32_t _level /* [in]  */,
					union netr_WorkstationInfo *_query /* [in] [switch_is(level),ref] */,
					union netr_DomainInfo *_info /* [out] [ref,switch_is(level)] */,
					NTSTATUS *result);

struct tevent_req *dcerpc_netr_ServerPasswordSet2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_ServerPasswordSet2 *r);
NTSTATUS dcerpc_netr_ServerPasswordSet2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_ServerPasswordSet2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_ServerPasswordSet2 *r);
struct tevent_req *dcerpc_netr_ServerPasswordSet2_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char *_server_name /* [in] [charset(UTF16),unique] */,
						       const char *_account_name /* [in] [charset(UTF16),ref] */,
						       enum netr_SchannelType _secure_channel_type /* [in]  */,
						       const char *_computer_name /* [in] [charset(UTF16),ref] */,
						       struct netr_Authenticator *_credential /* [in] [ref] */,
						       struct netr_Authenticator *_return_authenticator /* [out] [ref] */,
						       struct netr_CryptPassword *_new_password /* [in] [ref] */);
NTSTATUS dcerpc_netr_ServerPasswordSet2_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS dcerpc_netr_ServerPasswordSet2(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char *_server_name /* [in] [charset(UTF16),unique] */,
					const char *_account_name /* [in] [charset(UTF16),ref] */,
					enum netr_SchannelType _secure_channel_type /* [in]  */,
					const char *_computer_name /* [in] [charset(UTF16),ref] */,
					struct netr_Authenticator *_credential /* [in] [ref] */,
					struct netr_Authenticator *_return_authenticator /* [out] [ref] */,
					struct netr_CryptPassword *_new_password /* [in] [ref] */,
					NTSTATUS *result);

struct tevent_req *dcerpc_netr_ServerPasswordGet_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_ServerPasswordGet *r);
NTSTATUS dcerpc_netr_ServerPasswordGet_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_ServerPasswordGet_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_ServerPasswordGet *r);
struct tevent_req *dcerpc_netr_ServerPasswordGet_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      const char *_server_name /* [in] [charset(UTF16),unique] */,
						      const char *_account_name /* [in] [charset(UTF16),ref] */,
						      enum netr_SchannelType _secure_channel_type /* [in]  */,
						      const char *_computer_name /* [in] [charset(UTF16),ref] */,
						      struct netr_Authenticator *_credential /* [in] [ref] */,
						      struct netr_Authenticator *_return_authenticator /* [out] [ref] */,
						      struct samr_Password *_password /* [out] [ref] */);
NTSTATUS dcerpc_netr_ServerPasswordGet_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_netr_ServerPasswordGet(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       const char *_server_name /* [in] [charset(UTF16),unique] */,
				       const char *_account_name /* [in] [charset(UTF16),ref] */,
				       enum netr_SchannelType _secure_channel_type /* [in]  */,
				       const char *_computer_name /* [in] [charset(UTF16),ref] */,
				       struct netr_Authenticator *_credential /* [in] [ref] */,
				       struct netr_Authenticator *_return_authenticator /* [out] [ref] */,
				       struct samr_Password *_password /* [out] [ref] */,
				       WERROR *result);

struct tevent_req *dcerpc_netr_DsRAddressToSitenamesW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DsRAddressToSitenamesW *r);
NTSTATUS dcerpc_netr_DsRAddressToSitenamesW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DsRAddressToSitenamesW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DsRAddressToSitenamesW *r);
struct tevent_req *dcerpc_netr_DsRAddressToSitenamesW_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   const char *_server_name /* [in] [unique,charset(UTF16)] */,
							   uint32_t _count /* [in] [range(0,32000)] */,
							   struct netr_DsRAddress *_addresses /* [in] [size_is(count),ref] */,
							   struct netr_DsRAddressToSitenamesWCtr **_ctr /* [out] [ref] */);
NTSTATUS dcerpc_netr_DsRAddressToSitenamesW_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_netr_DsRAddressToSitenamesW(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    const char *_server_name /* [in] [unique,charset(UTF16)] */,
					    uint32_t _count /* [in] [range(0,32000)] */,
					    struct netr_DsRAddress *_addresses /* [in] [size_is(count),ref] */,
					    struct netr_DsRAddressToSitenamesWCtr **_ctr /* [out] [ref] */,
					    WERROR *result);

struct tevent_req *dcerpc_netr_DsRGetDCNameEx2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DsRGetDCNameEx2 *r);
NTSTATUS dcerpc_netr_DsRGetDCNameEx2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DsRGetDCNameEx2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DsRGetDCNameEx2 *r);
struct tevent_req *dcerpc_netr_DsRGetDCNameEx2_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						    const char *_client_account /* [in] [charset(UTF16),unique] */,
						    uint32_t _mask /* [in]  */,
						    const char *_domain_name /* [in] [unique,charset(UTF16)] */,
						    struct GUID *_domain_guid /* [in] [unique] */,
						    const char *_site_name /* [in] [charset(UTF16),unique] */,
						    uint32_t _flags /* [in]  */,
						    struct netr_DsRGetDCNameInfo **_info /* [out] [ref] */);
NTSTATUS dcerpc_netr_DsRGetDCNameEx2_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_netr_DsRGetDCNameEx2(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char *_server_unc /* [in] [unique,charset(UTF16)] */,
				     const char *_client_account /* [in] [charset(UTF16),unique] */,
				     uint32_t _mask /* [in]  */,
				     const char *_domain_name /* [in] [unique,charset(UTF16)] */,
				     struct GUID *_domain_guid /* [in] [unique] */,
				     const char *_site_name /* [in] [charset(UTF16),unique] */,
				     uint32_t _flags /* [in]  */,
				     struct netr_DsRGetDCNameInfo **_info /* [out] [ref] */,
				     WERROR *result);

struct tevent_req *dcerpc_netr_NetrEnumerateTrustedDomainsEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_NetrEnumerateTrustedDomainsEx *r);
NTSTATUS dcerpc_netr_NetrEnumerateTrustedDomainsEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_NetrEnumerateTrustedDomainsEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_NetrEnumerateTrustedDomainsEx *r);
struct tevent_req *dcerpc_netr_NetrEnumerateTrustedDomainsEx_send(TALLOC_CTX *mem_ctx,
								  struct tevent_context *ev,
								  struct dcerpc_binding_handle *h,
								  const char *_server_name /* [in] [unique,charset(UTF16)] */,
								  struct netr_DomainTrustList *_dom_trust_list /* [out] [ref] */);
NTSTATUS dcerpc_netr_NetrEnumerateTrustedDomainsEx_recv(struct tevent_req *req,
							TALLOC_CTX *mem_ctx,
							WERROR *result);
NTSTATUS dcerpc_netr_NetrEnumerateTrustedDomainsEx(struct dcerpc_binding_handle *h,
						   TALLOC_CTX *mem_ctx,
						   const char *_server_name /* [in] [unique,charset(UTF16)] */,
						   struct netr_DomainTrustList *_dom_trust_list /* [out] [ref] */,
						   WERROR *result);

struct tevent_req *dcerpc_netr_DsRAddressToSitenamesExW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DsRAddressToSitenamesExW *r);
NTSTATUS dcerpc_netr_DsRAddressToSitenamesExW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DsRAddressToSitenamesExW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DsRAddressToSitenamesExW *r);
struct tevent_req *dcerpc_netr_DsRAddressToSitenamesExW_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     const char *_server_name /* [in] [charset(UTF16),unique] */,
							     uint32_t _count /* [in] [range(0,32000)] */,
							     struct netr_DsRAddress *_addresses /* [in] [size_is(count),ref] */,
							     struct netr_DsRAddressToSitenamesExWCtr **_ctr /* [out] [ref] */);
NTSTATUS dcerpc_netr_DsRAddressToSitenamesExW_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_netr_DsRAddressToSitenamesExW(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      const char *_server_name /* [in] [charset(UTF16),unique] */,
					      uint32_t _count /* [in] [range(0,32000)] */,
					      struct netr_DsRAddress *_addresses /* [in] [size_is(count),ref] */,
					      struct netr_DsRAddressToSitenamesExWCtr **_ctr /* [out] [ref] */,
					      WERROR *result);

struct tevent_req *dcerpc_netr_DsrGetDcSiteCoverageW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DsrGetDcSiteCoverageW *r);
NTSTATUS dcerpc_netr_DsrGetDcSiteCoverageW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DsrGetDcSiteCoverageW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DsrGetDcSiteCoverageW *r);
struct tevent_req *dcerpc_netr_DsrGetDcSiteCoverageW_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  const char *_server_name /* [in] [unique,charset(UTF16)] */,
							  struct DcSitesCtr **_ctr /* [out] [ref] */);
NTSTATUS dcerpc_netr_DsrGetDcSiteCoverageW_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_netr_DsrGetDcSiteCoverageW(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   const char *_server_name /* [in] [unique,charset(UTF16)] */,
					   struct DcSitesCtr **_ctr /* [out] [ref] */,
					   WERROR *result);

struct tevent_req *dcerpc_netr_LogonSamLogonEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_LogonSamLogonEx *r);
NTSTATUS dcerpc_netr_LogonSamLogonEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_LogonSamLogonEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_LogonSamLogonEx *r);
struct tevent_req *dcerpc_netr_LogonSamLogonEx_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    const char *_server_name /* [in] [charset(UTF16),unique] */,
						    const char *_computer_name /* [in] [unique,charset(UTF16)] */,
						    enum netr_LogonInfoClass _logon_level /* [in]  */,
						    union netr_LogonLevel *_logon /* [in] [ref,switch_is(logon_level)] */,
						    uint16_t _validation_level /* [in]  */,
						    union netr_Validation *_validation /* [out] [switch_is(validation_level),ref] */,
						    uint8_t *_authoritative /* [out] [ref] */,
						    uint32_t *_flags /* [in,out] [ref] */);
NTSTATUS dcerpc_netr_LogonSamLogonEx_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  NTSTATUS *result);
NTSTATUS dcerpc_netr_LogonSamLogonEx(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char *_server_name /* [in] [charset(UTF16),unique] */,
				     const char *_computer_name /* [in] [unique,charset(UTF16)] */,
				     enum netr_LogonInfoClass _logon_level /* [in]  */,
				     union netr_LogonLevel *_logon /* [in] [ref,switch_is(logon_level)] */,
				     uint16_t _validation_level /* [in]  */,
				     union netr_Validation *_validation /* [out] [switch_is(validation_level),ref] */,
				     uint8_t *_authoritative /* [out] [ref] */,
				     uint32_t *_flags /* [in,out] [ref] */,
				     NTSTATUS *result);

struct tevent_req *dcerpc_netr_DsrEnumerateDomainTrusts_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DsrEnumerateDomainTrusts *r);
NTSTATUS dcerpc_netr_DsrEnumerateDomainTrusts_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DsrEnumerateDomainTrusts_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DsrEnumerateDomainTrusts *r);
struct tevent_req *dcerpc_netr_DsrEnumerateDomainTrusts_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     const char *_server_name /* [in] [charset(UTF16),unique] */,
							     uint32_t _trust_flags /* [in]  */,
							     struct netr_DomainTrustList *_trusts /* [out] [ref] */);
NTSTATUS dcerpc_netr_DsrEnumerateDomainTrusts_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_netr_DsrEnumerateDomainTrusts(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      const char *_server_name /* [in] [charset(UTF16),unique] */,
					      uint32_t _trust_flags /* [in]  */,
					      struct netr_DomainTrustList *_trusts /* [out] [ref] */,
					      WERROR *result);

struct tevent_req *dcerpc_netr_DsrDeregisterDNSHostRecords_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DsrDeregisterDNSHostRecords *r);
NTSTATUS dcerpc_netr_DsrDeregisterDNSHostRecords_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DsrDeregisterDNSHostRecords_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DsrDeregisterDNSHostRecords *r);
struct tevent_req *dcerpc_netr_DsrDeregisterDNSHostRecords_send(TALLOC_CTX *mem_ctx,
								struct tevent_context *ev,
								struct dcerpc_binding_handle *h,
								const char *_server_name /* [in] [charset(UTF16),unique] */,
								const char *_domain /* [in] [charset(UTF16),unique] */,
								struct GUID *_domain_guid /* [in] [unique] */,
								struct GUID *_dsa_guid /* [in] [unique] */,
								const char *_dns_host /* [in] [charset(UTF16),ref] */);
NTSTATUS dcerpc_netr_DsrDeregisterDNSHostRecords_recv(struct tevent_req *req,
						      TALLOC_CTX *mem_ctx,
						      WERROR *result);
NTSTATUS dcerpc_netr_DsrDeregisterDNSHostRecords(struct dcerpc_binding_handle *h,
						 TALLOC_CTX *mem_ctx,
						 const char *_server_name /* [in] [charset(UTF16),unique] */,
						 const char *_domain /* [in] [charset(UTF16),unique] */,
						 struct GUID *_domain_guid /* [in] [unique] */,
						 struct GUID *_dsa_guid /* [in] [unique] */,
						 const char *_dns_host /* [in] [charset(UTF16),ref] */,
						 WERROR *result);

struct tevent_req *dcerpc_netr_ServerTrustPasswordsGet_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_ServerTrustPasswordsGet *r);
NTSTATUS dcerpc_netr_ServerTrustPasswordsGet_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_ServerTrustPasswordsGet_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_ServerTrustPasswordsGet *r);
struct tevent_req *dcerpc_netr_ServerTrustPasswordsGet_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct dcerpc_binding_handle *h,
							    const char *_server_name /* [in] [unique,charset(UTF16)] */,
							    const char *_account_name /* [in] [ref,charset(UTF16)] */,
							    enum netr_SchannelType _secure_channel_type /* [in]  */,
							    const char *_computer_name /* [in] [charset(UTF16),ref] */,
							    struct netr_Authenticator *_credential /* [in] [ref] */,
							    struct netr_Authenticator *_return_authenticator /* [out] [ref] */,
							    struct samr_Password *_password /* [out] [ref] */,
							    struct samr_Password *_password2 /* [out] [ref] */);
NTSTATUS dcerpc_netr_ServerTrustPasswordsGet_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  NTSTATUS *result);
NTSTATUS dcerpc_netr_ServerTrustPasswordsGet(struct dcerpc_binding_handle *h,
					     TALLOC_CTX *mem_ctx,
					     const char *_server_name /* [in] [unique,charset(UTF16)] */,
					     const char *_account_name /* [in] [ref,charset(UTF16)] */,
					     enum netr_SchannelType _secure_channel_type /* [in]  */,
					     const char *_computer_name /* [in] [charset(UTF16),ref] */,
					     struct netr_Authenticator *_credential /* [in] [ref] */,
					     struct netr_Authenticator *_return_authenticator /* [out] [ref] */,
					     struct samr_Password *_password /* [out] [ref] */,
					     struct samr_Password *_password2 /* [out] [ref] */,
					     NTSTATUS *result);

struct tevent_req *dcerpc_netr_DsRGetForestTrustInformation_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DsRGetForestTrustInformation *r);
NTSTATUS dcerpc_netr_DsRGetForestTrustInformation_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DsRGetForestTrustInformation_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DsRGetForestTrustInformation *r);
struct tevent_req *dcerpc_netr_DsRGetForestTrustInformation_send(TALLOC_CTX *mem_ctx,
								 struct tevent_context *ev,
								 struct dcerpc_binding_handle *h,
								 const char *_server_name /* [in] [unique,charset(UTF16)] */,
								 const char *_trusted_domain_name /* [in] [charset(UTF16),unique] */,
								 uint32_t _flags /* [in]  */,
								 struct lsa_ForestTrustInformation **_forest_trust_info /* [out] [ref] */);
NTSTATUS dcerpc_netr_DsRGetForestTrustInformation_recv(struct tevent_req *req,
						       TALLOC_CTX *mem_ctx,
						       WERROR *result);
NTSTATUS dcerpc_netr_DsRGetForestTrustInformation(struct dcerpc_binding_handle *h,
						  TALLOC_CTX *mem_ctx,
						  const char *_server_name /* [in] [unique,charset(UTF16)] */,
						  const char *_trusted_domain_name /* [in] [charset(UTF16),unique] */,
						  uint32_t _flags /* [in]  */,
						  struct lsa_ForestTrustInformation **_forest_trust_info /* [out] [ref] */,
						  WERROR *result);

struct tevent_req *dcerpc_netr_GetForestTrustInformation_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_GetForestTrustInformation *r);
NTSTATUS dcerpc_netr_GetForestTrustInformation_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_GetForestTrustInformation_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_GetForestTrustInformation *r);
struct tevent_req *dcerpc_netr_GetForestTrustInformation_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      const char *_server_name /* [in] [unique,charset(UTF16)] */,
							      const char *_computer_name /* [in] [charset(UTF16),ref] */,
							      struct netr_Authenticator *_credential /* [in] [ref] */,
							      struct netr_Authenticator *_return_authenticator /* [out] [ref] */,
							      uint32_t _flags /* [in]  */,
							      struct lsa_ForestTrustInformation **_forest_trust_info /* [out] [ref] */);
NTSTATUS dcerpc_netr_GetForestTrustInformation_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    NTSTATUS *result);
NTSTATUS dcerpc_netr_GetForestTrustInformation(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       const char *_server_name /* [in] [unique,charset(UTF16)] */,
					       const char *_computer_name /* [in] [charset(UTF16),ref] */,
					       struct netr_Authenticator *_credential /* [in] [ref] */,
					       struct netr_Authenticator *_return_authenticator /* [out] [ref] */,
					       uint32_t _flags /* [in]  */,
					       struct lsa_ForestTrustInformation **_forest_trust_info /* [out] [ref] */,
					       NTSTATUS *result);

struct tevent_req *dcerpc_netr_LogonSamLogonWithFlags_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_LogonSamLogonWithFlags *r);
NTSTATUS dcerpc_netr_LogonSamLogonWithFlags_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_LogonSamLogonWithFlags_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_LogonSamLogonWithFlags *r);
struct tevent_req *dcerpc_netr_LogonSamLogonWithFlags_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   const char *_server_name /* [in] [unique,charset(UTF16)] */,
							   const char *_computer_name /* [in] [unique,charset(UTF16)] */,
							   struct netr_Authenticator *_credential /* [in] [unique] */,
							   struct netr_Authenticator *_return_authenticator /* [in,out] [unique] */,
							   enum netr_LogonInfoClass _logon_level /* [in]  */,
							   union netr_LogonLevel *_logon /* [in] [switch_is(logon_level),ref] */,
							   uint16_t _validation_level /* [in]  */,
							   union netr_Validation *_validation /* [out] [switch_is(validation_level),ref] */,
							   uint8_t *_authoritative /* [out] [ref] */,
							   uint32_t *_flags /* [in,out] [ref] */);
NTSTATUS dcerpc_netr_LogonSamLogonWithFlags_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 NTSTATUS *result);
NTSTATUS dcerpc_netr_LogonSamLogonWithFlags(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    const char *_server_name /* [in] [unique,charset(UTF16)] */,
					    const char *_computer_name /* [in] [unique,charset(UTF16)] */,
					    struct netr_Authenticator *_credential /* [in] [unique] */,
					    struct netr_Authenticator *_return_authenticator /* [in,out] [unique] */,
					    enum netr_LogonInfoClass _logon_level /* [in]  */,
					    union netr_LogonLevel *_logon /* [in] [switch_is(logon_level),ref] */,
					    uint16_t _validation_level /* [in]  */,
					    union netr_Validation *_validation /* [out] [switch_is(validation_level),ref] */,
					    uint8_t *_authoritative /* [out] [ref] */,
					    uint32_t *_flags /* [in,out] [ref] */,
					    NTSTATUS *result);

struct tevent_req *dcerpc_netr_ServerGetTrustInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_ServerGetTrustInfo *r);
NTSTATUS dcerpc_netr_ServerGetTrustInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_ServerGetTrustInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_ServerGetTrustInfo *r);
struct tevent_req *dcerpc_netr_ServerGetTrustInfo_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char *_server_name /* [in] [unique,charset(UTF16)] */,
						       const char *_account_name /* [in] [charset(UTF16),ref] */,
						       enum netr_SchannelType _secure_channel_type /* [in]  */,
						       const char *_computer_name /* [in] [charset(UTF16),ref] */,
						       struct netr_Authenticator *_credential /* [in] [ref] */,
						       struct netr_Authenticator *_return_authenticator /* [out] [ref] */,
						       struct samr_Password *_new_owf_password /* [out] [ref] */,
						       struct samr_Password *_old_owf_password /* [out] [ref] */,
						       struct netr_TrustInfo **_trust_info /* [out] [ref] */);
NTSTATUS dcerpc_netr_ServerGetTrustInfo_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS dcerpc_netr_ServerGetTrustInfo(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char *_server_name /* [in] [unique,charset(UTF16)] */,
					const char *_account_name /* [in] [charset(UTF16),ref] */,
					enum netr_SchannelType _secure_channel_type /* [in]  */,
					const char *_computer_name /* [in] [charset(UTF16),ref] */,
					struct netr_Authenticator *_credential /* [in] [ref] */,
					struct netr_Authenticator *_return_authenticator /* [out] [ref] */,
					struct samr_Password *_new_owf_password /* [out] [ref] */,
					struct samr_Password *_old_owf_password /* [out] [ref] */,
					struct netr_TrustInfo **_trust_info /* [out] [ref] */,
					NTSTATUS *result);

struct tevent_req *dcerpc_netr_Unused47_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_Unused47 *r);
NTSTATUS dcerpc_netr_Unused47_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_Unused47_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_Unused47 *r);
struct tevent_req *dcerpc_netr_Unused47_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_netr_Unused47_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_netr_Unused47(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      NTSTATUS *result);

struct tevent_req *dcerpc_netr_DsrUpdateReadOnlyServerDnsRecords_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct netr_DsrUpdateReadOnlyServerDnsRecords *r);
NTSTATUS dcerpc_netr_DsrUpdateReadOnlyServerDnsRecords_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_netr_DsrUpdateReadOnlyServerDnsRecords_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct netr_DsrUpdateReadOnlyServerDnsRecords *r);
struct tevent_req *dcerpc_netr_DsrUpdateReadOnlyServerDnsRecords_send(TALLOC_CTX *mem_ctx,
								      struct tevent_context *ev,
								      struct dcerpc_binding_handle *h,
								      const char *_server_name /* [in] [charset(UTF16),unique] */,
								      const char *_computer_name /* [in] [ref,charset(UTF16)] */,
								      struct netr_Authenticator *_credential /* [in] [ref] */,
								      struct netr_Authenticator *_return_authenticator /* [out] [ref] */,
								      const char *_site_name /* [in] [unique,charset(UTF16)] */,
								      uint32_t _dns_ttl /* [in]  */,
								      struct NL_DNS_NAME_INFO_ARRAY *_dns_names /* [in,out] [ref] */);
NTSTATUS dcerpc_netr_DsrUpdateReadOnlyServerDnsRecords_recv(struct tevent_req *req,
							    TALLOC_CTX *mem_ctx,
							    NTSTATUS *result);
NTSTATUS dcerpc_netr_DsrUpdateReadOnlyServerDnsRecords(struct dcerpc_binding_handle *h,
						       TALLOC_CTX *mem_ctx,
						       const char *_server_name /* [in] [charset(UTF16),unique] */,
						       const char *_computer_name /* [in] [ref,charset(UTF16)] */,
						       struct netr_Authenticator *_credential /* [in] [ref] */,
						       struct netr_Authenticator *_return_authenticator /* [out] [ref] */,
						       const char *_site_name /* [in] [unique,charset(UTF16)] */,
						       uint32_t _dns_ttl /* [in]  */,
						       struct NL_DNS_NAME_INFO_ARRAY *_dns_names /* [in,out] [ref] */,
						       NTSTATUS *result);

#endif /* _HEADER_RPC_netlogon */
