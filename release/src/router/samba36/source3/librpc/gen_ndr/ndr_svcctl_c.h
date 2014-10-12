#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/svcctl.h"
#ifndef _HEADER_RPC_svcctl
#define _HEADER_RPC_svcctl

extern const struct ndr_interface_table ndr_table_svcctl;

struct tevent_req *dcerpc_svcctl_CloseServiceHandle_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_CloseServiceHandle *r);
NTSTATUS dcerpc_svcctl_CloseServiceHandle_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_CloseServiceHandle_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_CloseServiceHandle *r);
struct tevent_req *dcerpc_svcctl_CloseServiceHandle_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_svcctl_CloseServiceHandle_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_svcctl_CloseServiceHandle(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_handle /* [in,out] [ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_svcctl_ControlService_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_ControlService *r);
NTSTATUS dcerpc_svcctl_ControlService_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_ControlService_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_ControlService *r);
struct tevent_req *dcerpc_svcctl_ControlService_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_handle /* [in] [ref] */,
						     enum SERVICE_CONTROL _control /* [in]  */,
						     struct SERVICE_STATUS *_service_status /* [out] [ref] */);
NTSTATUS dcerpc_svcctl_ControlService_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_svcctl_ControlService(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_handle /* [in] [ref] */,
				      enum SERVICE_CONTROL _control /* [in]  */,
				      struct SERVICE_STATUS *_service_status /* [out] [ref] */,
				      WERROR *result);

struct tevent_req *dcerpc_svcctl_DeleteService_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_DeleteService *r);
NTSTATUS dcerpc_svcctl_DeleteService_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_DeleteService_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_DeleteService *r);
struct tevent_req *dcerpc_svcctl_DeleteService_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS dcerpc_svcctl_DeleteService_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_svcctl_DeleteService(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_handle /* [in] [ref] */,
				     WERROR *result);

struct tevent_req *dcerpc_svcctl_LockServiceDatabase_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_LockServiceDatabase *r);
NTSTATUS dcerpc_svcctl_LockServiceDatabase_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_LockServiceDatabase_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_LockServiceDatabase *r);
struct tevent_req *dcerpc_svcctl_LockServiceDatabase_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_handle /* [in] [ref] */,
							  struct policy_handle *_lock /* [out] [ref] */);
NTSTATUS dcerpc_svcctl_LockServiceDatabase_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_svcctl_LockServiceDatabase(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_handle /* [in] [ref] */,
					   struct policy_handle *_lock /* [out] [ref] */,
					   WERROR *result);

struct tevent_req *dcerpc_svcctl_QueryServiceObjectSecurity_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_QueryServiceObjectSecurity *r);
NTSTATUS dcerpc_svcctl_QueryServiceObjectSecurity_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_QueryServiceObjectSecurity_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_QueryServiceObjectSecurity *r);
struct tevent_req *dcerpc_svcctl_QueryServiceObjectSecurity_send(TALLOC_CTX *mem_ctx,
								 struct tevent_context *ev,
								 struct dcerpc_binding_handle *h,
								 struct policy_handle *_handle /* [in] [ref] */,
								 uint32_t _security_flags /* [in]  */,
								 uint8_t *_buffer /* [out] [size_is(offered),ref] */,
								 uint32_t _offered /* [in] [range(0,0x40000)] */,
								 uint32_t *_needed /* [out] [ref,range(0,0x40000)] */);
NTSTATUS dcerpc_svcctl_QueryServiceObjectSecurity_recv(struct tevent_req *req,
						       TALLOC_CTX *mem_ctx,
						       WERROR *result);
NTSTATUS dcerpc_svcctl_QueryServiceObjectSecurity(struct dcerpc_binding_handle *h,
						  TALLOC_CTX *mem_ctx,
						  struct policy_handle *_handle /* [in] [ref] */,
						  uint32_t _security_flags /* [in]  */,
						  uint8_t *_buffer /* [out] [size_is(offered),ref] */,
						  uint32_t _offered /* [in] [range(0,0x40000)] */,
						  uint32_t *_needed /* [out] [ref,range(0,0x40000)] */,
						  WERROR *result);

struct tevent_req *dcerpc_svcctl_SetServiceObjectSecurity_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_SetServiceObjectSecurity *r);
NTSTATUS dcerpc_svcctl_SetServiceObjectSecurity_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_SetServiceObjectSecurity_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_SetServiceObjectSecurity *r);
struct tevent_req *dcerpc_svcctl_SetServiceObjectSecurity_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct dcerpc_binding_handle *h,
							       struct policy_handle *_handle /* [in] [ref] */,
							       uint32_t _security_flags /* [in]  */,
							       uint8_t *_buffer /* [in] [size_is(offered),ref] */,
							       uint32_t _offered /* [in]  */);
NTSTATUS dcerpc_svcctl_SetServiceObjectSecurity_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     WERROR *result);
NTSTATUS dcerpc_svcctl_SetServiceObjectSecurity(struct dcerpc_binding_handle *h,
						TALLOC_CTX *mem_ctx,
						struct policy_handle *_handle /* [in] [ref] */,
						uint32_t _security_flags /* [in]  */,
						uint8_t *_buffer /* [in] [size_is(offered),ref] */,
						uint32_t _offered /* [in]  */,
						WERROR *result);

struct tevent_req *dcerpc_svcctl_QueryServiceStatus_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_QueryServiceStatus *r);
NTSTATUS dcerpc_svcctl_QueryServiceStatus_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_QueryServiceStatus_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_QueryServiceStatus *r);
struct tevent_req *dcerpc_svcctl_QueryServiceStatus_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_handle /* [in] [ref] */,
							 struct SERVICE_STATUS *_service_status /* [out] [ref] */);
NTSTATUS dcerpc_svcctl_QueryServiceStatus_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_svcctl_QueryServiceStatus(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_handle /* [in] [ref] */,
					  struct SERVICE_STATUS *_service_status /* [out] [ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_svcctl_UnlockServiceDatabase_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_UnlockServiceDatabase *r);
NTSTATUS dcerpc_svcctl_UnlockServiceDatabase_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_UnlockServiceDatabase_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_UnlockServiceDatabase *r);
struct tevent_req *dcerpc_svcctl_UnlockServiceDatabase_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct dcerpc_binding_handle *h,
							    struct policy_handle *_lock /* [in,out] [ref] */);
NTSTATUS dcerpc_svcctl_UnlockServiceDatabase_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS dcerpc_svcctl_UnlockServiceDatabase(struct dcerpc_binding_handle *h,
					     TALLOC_CTX *mem_ctx,
					     struct policy_handle *_lock /* [in,out] [ref] */,
					     WERROR *result);

struct tevent_req *dcerpc_svcctl_SCSetServiceBitsW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_SCSetServiceBitsW *r);
NTSTATUS dcerpc_svcctl_SCSetServiceBitsW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_SCSetServiceBitsW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_SCSetServiceBitsW *r);
struct tevent_req *dcerpc_svcctl_SCSetServiceBitsW_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_handle /* [in] [ref] */,
							uint32_t _bits /* [in]  */,
							uint32_t _bitson /* [in]  */,
							uint32_t _immediate /* [in]  */);
NTSTATUS dcerpc_svcctl_SCSetServiceBitsW_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_svcctl_SCSetServiceBitsW(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_handle /* [in] [ref] */,
					 uint32_t _bits /* [in]  */,
					 uint32_t _bitson /* [in]  */,
					 uint32_t _immediate /* [in]  */,
					 WERROR *result);

struct tevent_req *dcerpc_svcctl_ChangeServiceConfigW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_ChangeServiceConfigW *r);
NTSTATUS dcerpc_svcctl_ChangeServiceConfigW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_ChangeServiceConfigW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_ChangeServiceConfigW *r);
struct tevent_req *dcerpc_svcctl_ChangeServiceConfigW_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   struct policy_handle *_handle /* [in] [ref] */,
							   uint32_t _type /* [in]  */,
							   enum svcctl_StartType _start_type /* [in]  */,
							   enum svcctl_ErrorControl _error_control /* [in]  */,
							   const char *_binary_path /* [in] [charset(UTF16),unique] */,
							   const char *_load_order_group /* [in] [charset(UTF16),unique] */,
							   uint32_t *_tag_id /* [out] [ref] */,
							   const char *_dependencies /* [in] [charset(UTF16),unique] */,
							   const char *_service_start_name /* [in] [unique,charset(UTF16)] */,
							   const char *_password /* [in] [unique,charset(UTF16)] */,
							   const char *_display_name /* [in] [charset(UTF16),unique] */);
NTSTATUS dcerpc_svcctl_ChangeServiceConfigW_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_svcctl_ChangeServiceConfigW(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    struct policy_handle *_handle /* [in] [ref] */,
					    uint32_t _type /* [in]  */,
					    enum svcctl_StartType _start_type /* [in]  */,
					    enum svcctl_ErrorControl _error_control /* [in]  */,
					    const char *_binary_path /* [in] [charset(UTF16),unique] */,
					    const char *_load_order_group /* [in] [charset(UTF16),unique] */,
					    uint32_t *_tag_id /* [out] [ref] */,
					    const char *_dependencies /* [in] [charset(UTF16),unique] */,
					    const char *_service_start_name /* [in] [unique,charset(UTF16)] */,
					    const char *_password /* [in] [unique,charset(UTF16)] */,
					    const char *_display_name /* [in] [charset(UTF16),unique] */,
					    WERROR *result);

struct tevent_req *dcerpc_svcctl_CreateServiceW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_CreateServiceW *r);
NTSTATUS dcerpc_svcctl_CreateServiceW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_CreateServiceW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_CreateServiceW *r);
struct tevent_req *dcerpc_svcctl_CreateServiceW_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_scmanager_handle /* [in] [ref] */,
						     const char *_ServiceName /* [in] [charset(UTF16)] */,
						     const char *_DisplayName /* [in] [unique,charset(UTF16)] */,
						     uint32_t _desired_access /* [in]  */,
						     uint32_t _type /* [in]  */,
						     enum svcctl_StartType _start_type /* [in]  */,
						     enum svcctl_ErrorControl _error_control /* [in]  */,
						     const char *_binary_path /* [in] [charset(UTF16)] */,
						     const char *_LoadOrderGroupKey /* [in] [unique,charset(UTF16)] */,
						     uint32_t *_TagId /* [in,out] [unique] */,
						     uint8_t *_dependencies /* [in] [size_is(dependencies_size),unique] */,
						     uint32_t _dependencies_size /* [in]  */,
						     const char *_service_start_name /* [in] [unique,charset(UTF16)] */,
						     uint8_t *_password /* [in] [size_is(password_size),unique] */,
						     uint32_t _password_size /* [in]  */,
						     struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_svcctl_CreateServiceW_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_svcctl_CreateServiceW(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_scmanager_handle /* [in] [ref] */,
				      const char *_ServiceName /* [in] [charset(UTF16)] */,
				      const char *_DisplayName /* [in] [unique,charset(UTF16)] */,
				      uint32_t _desired_access /* [in]  */,
				      uint32_t _type /* [in]  */,
				      enum svcctl_StartType _start_type /* [in]  */,
				      enum svcctl_ErrorControl _error_control /* [in]  */,
				      const char *_binary_path /* [in] [charset(UTF16)] */,
				      const char *_LoadOrderGroupKey /* [in] [unique,charset(UTF16)] */,
				      uint32_t *_TagId /* [in,out] [unique] */,
				      uint8_t *_dependencies /* [in] [size_is(dependencies_size),unique] */,
				      uint32_t _dependencies_size /* [in]  */,
				      const char *_service_start_name /* [in] [unique,charset(UTF16)] */,
				      uint8_t *_password /* [in] [size_is(password_size),unique] */,
				      uint32_t _password_size /* [in]  */,
				      struct policy_handle *_handle /* [out] [ref] */,
				      WERROR *result);

struct tevent_req *dcerpc_svcctl_EnumDependentServicesW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_EnumDependentServicesW *r);
NTSTATUS dcerpc_svcctl_EnumDependentServicesW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_EnumDependentServicesW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_EnumDependentServicesW *r);
struct tevent_req *dcerpc_svcctl_EnumDependentServicesW_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     struct policy_handle *_service /* [in] [ref] */,
							     enum svcctl_ServiceState _state /* [in]  */,
							     uint8_t *_service_status /* [out] [size_is(offered),ref] */,
							     uint32_t _offered /* [in] [range(0,0x40000)] */,
							     uint32_t *_needed /* [out] [ref,range(0,0x40000)] */,
							     uint32_t *_services_returned /* [out] [ref,range(0,0x40000)] */);
NTSTATUS dcerpc_svcctl_EnumDependentServicesW_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_svcctl_EnumDependentServicesW(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      struct policy_handle *_service /* [in] [ref] */,
					      enum svcctl_ServiceState _state /* [in]  */,
					      uint8_t *_service_status /* [out] [size_is(offered),ref] */,
					      uint32_t _offered /* [in] [range(0,0x40000)] */,
					      uint32_t *_needed /* [out] [ref,range(0,0x40000)] */,
					      uint32_t *_services_returned /* [out] [ref,range(0,0x40000)] */,
					      WERROR *result);

struct tevent_req *dcerpc_svcctl_EnumServicesStatusW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_EnumServicesStatusW *r);
NTSTATUS dcerpc_svcctl_EnumServicesStatusW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_EnumServicesStatusW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_EnumServicesStatusW *r);
struct tevent_req *dcerpc_svcctl_EnumServicesStatusW_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_handle /* [in] [ref] */,
							  uint32_t _type /* [in]  */,
							  enum svcctl_ServiceState _state /* [in]  */,
							  uint8_t *_service /* [out] [size_is(offered),ref] */,
							  uint32_t _offered /* [in] [range(0,0x40000)] */,
							  uint32_t *_needed /* [out] [range(0,0x40000),ref] */,
							  uint32_t *_services_returned /* [out] [range(0,0x40000),ref] */,
							  uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_svcctl_EnumServicesStatusW_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_svcctl_EnumServicesStatusW(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_handle /* [in] [ref] */,
					   uint32_t _type /* [in]  */,
					   enum svcctl_ServiceState _state /* [in]  */,
					   uint8_t *_service /* [out] [size_is(offered),ref] */,
					   uint32_t _offered /* [in] [range(0,0x40000)] */,
					   uint32_t *_needed /* [out] [range(0,0x40000),ref] */,
					   uint32_t *_services_returned /* [out] [range(0,0x40000),ref] */,
					   uint32_t *_resume_handle /* [in,out] [unique] */,
					   WERROR *result);

struct tevent_req *dcerpc_svcctl_OpenSCManagerW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_OpenSCManagerW *r);
NTSTATUS dcerpc_svcctl_OpenSCManagerW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_OpenSCManagerW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_OpenSCManagerW *r);
struct tevent_req *dcerpc_svcctl_OpenSCManagerW_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     const char *_MachineName /* [in] [unique,charset(UTF16)] */,
						     const char *_DatabaseName /* [in] [charset(UTF16),unique] */,
						     uint32_t _access_mask /* [in]  */,
						     struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_svcctl_OpenSCManagerW_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_svcctl_OpenSCManagerW(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      const char *_MachineName /* [in] [unique,charset(UTF16)] */,
				      const char *_DatabaseName /* [in] [charset(UTF16),unique] */,
				      uint32_t _access_mask /* [in]  */,
				      struct policy_handle *_handle /* [out] [ref] */,
				      WERROR *result);

struct tevent_req *dcerpc_svcctl_OpenServiceW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_OpenServiceW *r);
NTSTATUS dcerpc_svcctl_OpenServiceW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_OpenServiceW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_OpenServiceW *r);
struct tevent_req *dcerpc_svcctl_OpenServiceW_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct policy_handle *_scmanager_handle /* [in] [ref] */,
						   const char *_ServiceName /* [in] [charset(UTF16)] */,
						   uint32_t _access_mask /* [in]  */,
						   struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_svcctl_OpenServiceW_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_svcctl_OpenServiceW(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *_scmanager_handle /* [in] [ref] */,
				    const char *_ServiceName /* [in] [charset(UTF16)] */,
				    uint32_t _access_mask /* [in]  */,
				    struct policy_handle *_handle /* [out] [ref] */,
				    WERROR *result);

struct tevent_req *dcerpc_svcctl_QueryServiceConfigW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_QueryServiceConfigW *r);
NTSTATUS dcerpc_svcctl_QueryServiceConfigW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_QueryServiceConfigW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_QueryServiceConfigW *r);
struct tevent_req *dcerpc_svcctl_QueryServiceConfigW_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_handle /* [in] [ref] */,
							  struct QUERY_SERVICE_CONFIG *_query /* [out] [ref] */,
							  uint32_t _offered /* [in] [range(0,8192)] */,
							  uint32_t *_needed /* [out] [ref,range(0,8192)] */);
NTSTATUS dcerpc_svcctl_QueryServiceConfigW_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_svcctl_QueryServiceConfigW(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_handle /* [in] [ref] */,
					   struct QUERY_SERVICE_CONFIG *_query /* [out] [ref] */,
					   uint32_t _offered /* [in] [range(0,8192)] */,
					   uint32_t *_needed /* [out] [ref,range(0,8192)] */,
					   WERROR *result);

struct tevent_req *dcerpc_svcctl_QueryServiceLockStatusW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_QueryServiceLockStatusW *r);
NTSTATUS dcerpc_svcctl_QueryServiceLockStatusW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_QueryServiceLockStatusW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_QueryServiceLockStatusW *r);
struct tevent_req *dcerpc_svcctl_QueryServiceLockStatusW_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      struct policy_handle *_handle /* [in] [ref] */,
							      uint32_t _offered /* [in]  */,
							      struct SERVICE_LOCK_STATUS *_lock_status /* [out] [ref] */,
							      uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_svcctl_QueryServiceLockStatusW_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    WERROR *result);
NTSTATUS dcerpc_svcctl_QueryServiceLockStatusW(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       struct policy_handle *_handle /* [in] [ref] */,
					       uint32_t _offered /* [in]  */,
					       struct SERVICE_LOCK_STATUS *_lock_status /* [out] [ref] */,
					       uint32_t *_needed /* [out] [ref] */,
					       WERROR *result);

struct tevent_req *dcerpc_svcctl_StartServiceW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_StartServiceW *r);
NTSTATUS dcerpc_svcctl_StartServiceW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_StartServiceW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_StartServiceW *r);
struct tevent_req *dcerpc_svcctl_StartServiceW_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_handle /* [in] [ref] */,
						    uint32_t _NumArgs /* [in] [range(0,SC_MAX_ARGUMENTS)] */,
						    struct svcctl_ArgumentString *_Arguments /* [in] [unique,size_is(NumArgs)] */);
NTSTATUS dcerpc_svcctl_StartServiceW_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_svcctl_StartServiceW(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_handle /* [in] [ref] */,
				     uint32_t _NumArgs /* [in] [range(0,SC_MAX_ARGUMENTS)] */,
				     struct svcctl_ArgumentString *_Arguments /* [in] [unique,size_is(NumArgs)] */,
				     WERROR *result);

struct tevent_req *dcerpc_svcctl_GetServiceDisplayNameW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_GetServiceDisplayNameW *r);
NTSTATUS dcerpc_svcctl_GetServiceDisplayNameW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_GetServiceDisplayNameW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_GetServiceDisplayNameW *r);
struct tevent_req *dcerpc_svcctl_GetServiceDisplayNameW_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     struct policy_handle *_handle /* [in] [ref] */,
							     const char *_service_name /* [in] [charset(UTF16),unique] */,
							     const char **_display_name /* [out] [ref,charset(UTF16)] */,
							     uint32_t *_display_name_length /* [in,out] [unique] */);
NTSTATUS dcerpc_svcctl_GetServiceDisplayNameW_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_svcctl_GetServiceDisplayNameW(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      struct policy_handle *_handle /* [in] [ref] */,
					      const char *_service_name /* [in] [charset(UTF16),unique] */,
					      const char **_display_name /* [out] [ref,charset(UTF16)] */,
					      uint32_t *_display_name_length /* [in,out] [unique] */,
					      WERROR *result);

struct tevent_req *dcerpc_svcctl_GetServiceKeyNameW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_GetServiceKeyNameW *r);
NTSTATUS dcerpc_svcctl_GetServiceKeyNameW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_GetServiceKeyNameW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_GetServiceKeyNameW *r);
struct tevent_req *dcerpc_svcctl_GetServiceKeyNameW_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_handle /* [in] [ref] */,
							 const char *_service_name /* [in] [unique,charset(UTF16)] */,
							 const char **_key_name /* [out] [ref,charset(UTF16)] */,
							 uint32_t *_display_name_length /* [in,out] [unique] */);
NTSTATUS dcerpc_svcctl_GetServiceKeyNameW_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_svcctl_GetServiceKeyNameW(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_handle /* [in] [ref] */,
					  const char *_service_name /* [in] [unique,charset(UTF16)] */,
					  const char **_key_name /* [out] [ref,charset(UTF16)] */,
					  uint32_t *_display_name_length /* [in,out] [unique] */,
					  WERROR *result);

struct tevent_req *dcerpc_svcctl_SCSetServiceBitsA_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_SCSetServiceBitsA *r);
NTSTATUS dcerpc_svcctl_SCSetServiceBitsA_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_SCSetServiceBitsA_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_SCSetServiceBitsA *r);
struct tevent_req *dcerpc_svcctl_SCSetServiceBitsA_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_handle /* [in] [ref] */,
							uint32_t _bits /* [in]  */,
							uint32_t _bitson /* [in]  */,
							uint32_t _immediate /* [in]  */);
NTSTATUS dcerpc_svcctl_SCSetServiceBitsA_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_svcctl_SCSetServiceBitsA(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_handle /* [in] [ref] */,
					 uint32_t _bits /* [in]  */,
					 uint32_t _bitson /* [in]  */,
					 uint32_t _immediate /* [in]  */,
					 WERROR *result);

struct tevent_req *dcerpc_svcctl_ChangeServiceConfigA_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_ChangeServiceConfigA *r);
NTSTATUS dcerpc_svcctl_ChangeServiceConfigA_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_ChangeServiceConfigA_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_ChangeServiceConfigA *r);
struct tevent_req *dcerpc_svcctl_ChangeServiceConfigA_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   struct policy_handle *_handle /* [in] [ref] */,
							   uint32_t _type /* [in]  */,
							   enum svcctl_StartType _start_type /* [in]  */,
							   enum svcctl_ErrorControl _error_control /* [in]  */,
							   const char *_binary_path /* [in] [charset(UTF16),unique] */,
							   const char *_load_order_group /* [in] [charset(UTF16),unique] */,
							   uint32_t *_tag_id /* [out] [ref] */,
							   const char *_dependencies /* [in] [charset(UTF16),unique] */,
							   const char *_service_start_name /* [in] [charset(UTF16),unique] */,
							   const char *_password /* [in] [unique,charset(UTF16)] */,
							   const char *_display_name /* [in] [charset(UTF16),unique] */);
NTSTATUS dcerpc_svcctl_ChangeServiceConfigA_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_svcctl_ChangeServiceConfigA(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    struct policy_handle *_handle /* [in] [ref] */,
					    uint32_t _type /* [in]  */,
					    enum svcctl_StartType _start_type /* [in]  */,
					    enum svcctl_ErrorControl _error_control /* [in]  */,
					    const char *_binary_path /* [in] [charset(UTF16),unique] */,
					    const char *_load_order_group /* [in] [charset(UTF16),unique] */,
					    uint32_t *_tag_id /* [out] [ref] */,
					    const char *_dependencies /* [in] [charset(UTF16),unique] */,
					    const char *_service_start_name /* [in] [charset(UTF16),unique] */,
					    const char *_password /* [in] [unique,charset(UTF16)] */,
					    const char *_display_name /* [in] [charset(UTF16),unique] */,
					    WERROR *result);

struct tevent_req *dcerpc_svcctl_CreateServiceA_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_CreateServiceA *r);
NTSTATUS dcerpc_svcctl_CreateServiceA_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_CreateServiceA_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_CreateServiceA *r);
struct tevent_req *dcerpc_svcctl_CreateServiceA_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_handle /* [in] [ref] */,
						     const char *_ServiceName /* [in] [unique,charset(UTF16)] */,
						     const char *_DisplayName /* [in] [charset(UTF16),unique] */,
						     uint32_t _desired_access /* [in]  */,
						     uint32_t _type /* [in]  */,
						     enum svcctl_StartType _start_type /* [in]  */,
						     enum svcctl_ErrorControl _error_control /* [in]  */,
						     const char *_binary_path /* [in] [charset(UTF16),unique] */,
						     const char *_LoadOrderGroupKey /* [in] [charset(UTF16),unique] */,
						     uint32_t *_TagId /* [out] [unique] */,
						     const char *_dependencies /* [in] [unique,charset(UTF16)] */,
						     const char *_service_start_name /* [in] [charset(UTF16),unique] */,
						     const char *_password /* [in] [charset(UTF16),unique] */);
NTSTATUS dcerpc_svcctl_CreateServiceA_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_svcctl_CreateServiceA(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_handle /* [in] [ref] */,
				      const char *_ServiceName /* [in] [unique,charset(UTF16)] */,
				      const char *_DisplayName /* [in] [charset(UTF16),unique] */,
				      uint32_t _desired_access /* [in]  */,
				      uint32_t _type /* [in]  */,
				      enum svcctl_StartType _start_type /* [in]  */,
				      enum svcctl_ErrorControl _error_control /* [in]  */,
				      const char *_binary_path /* [in] [charset(UTF16),unique] */,
				      const char *_LoadOrderGroupKey /* [in] [charset(UTF16),unique] */,
				      uint32_t *_TagId /* [out] [unique] */,
				      const char *_dependencies /* [in] [unique,charset(UTF16)] */,
				      const char *_service_start_name /* [in] [charset(UTF16),unique] */,
				      const char *_password /* [in] [charset(UTF16),unique] */,
				      WERROR *result);

struct tevent_req *dcerpc_svcctl_EnumDependentServicesA_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_EnumDependentServicesA *r);
NTSTATUS dcerpc_svcctl_EnumDependentServicesA_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_EnumDependentServicesA_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_EnumDependentServicesA *r);
struct tevent_req *dcerpc_svcctl_EnumDependentServicesA_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     struct policy_handle *_service /* [in] [ref] */,
							     enum svcctl_ServiceState _state /* [in]  */,
							     struct ENUM_SERVICE_STATUSA *_service_status /* [out] [unique] */,
							     uint32_t _offered /* [in]  */,
							     uint32_t *_needed /* [out] [ref] */,
							     uint32_t *_services_returned /* [out] [ref] */);
NTSTATUS dcerpc_svcctl_EnumDependentServicesA_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_svcctl_EnumDependentServicesA(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      struct policy_handle *_service /* [in] [ref] */,
					      enum svcctl_ServiceState _state /* [in]  */,
					      struct ENUM_SERVICE_STATUSA *_service_status /* [out] [unique] */,
					      uint32_t _offered /* [in]  */,
					      uint32_t *_needed /* [out] [ref] */,
					      uint32_t *_services_returned /* [out] [ref] */,
					      WERROR *result);

struct tevent_req *dcerpc_svcctl_EnumServicesStatusA_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_EnumServicesStatusA *r);
NTSTATUS dcerpc_svcctl_EnumServicesStatusA_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_EnumServicesStatusA_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_EnumServicesStatusA *r);
struct tevent_req *dcerpc_svcctl_EnumServicesStatusA_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_handle /* [in] [ref] */,
							  uint32_t _type /* [in]  */,
							  enum svcctl_ServiceState _state /* [in]  */,
							  uint32_t _offered /* [in]  */,
							  uint8_t *_service /* [out] [size_is(offered)] */,
							  uint32_t *_needed /* [out] [ref] */,
							  uint32_t *_services_returned /* [out] [ref] */,
							  uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_svcctl_EnumServicesStatusA_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_svcctl_EnumServicesStatusA(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_handle /* [in] [ref] */,
					   uint32_t _type /* [in]  */,
					   enum svcctl_ServiceState _state /* [in]  */,
					   uint32_t _offered /* [in]  */,
					   uint8_t *_service /* [out] [size_is(offered)] */,
					   uint32_t *_needed /* [out] [ref] */,
					   uint32_t *_services_returned /* [out] [ref] */,
					   uint32_t *_resume_handle /* [in,out] [unique] */,
					   WERROR *result);

struct tevent_req *dcerpc_svcctl_OpenSCManagerA_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_OpenSCManagerA *r);
NTSTATUS dcerpc_svcctl_OpenSCManagerA_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_OpenSCManagerA_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_OpenSCManagerA *r);
struct tevent_req *dcerpc_svcctl_OpenSCManagerA_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     const char *_MachineName /* [in] [charset(UTF16),unique] */,
						     const char *_DatabaseName /* [in] [charset(UTF16),unique] */,
						     uint32_t _access_mask /* [in]  */,
						     struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_svcctl_OpenSCManagerA_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_svcctl_OpenSCManagerA(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      const char *_MachineName /* [in] [charset(UTF16),unique] */,
				      const char *_DatabaseName /* [in] [charset(UTF16),unique] */,
				      uint32_t _access_mask /* [in]  */,
				      struct policy_handle *_handle /* [out] [ref] */,
				      WERROR *result);

struct tevent_req *dcerpc_svcctl_OpenServiceA_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_OpenServiceA *r);
NTSTATUS dcerpc_svcctl_OpenServiceA_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_OpenServiceA_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_OpenServiceA *r);
struct tevent_req *dcerpc_svcctl_OpenServiceA_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct policy_handle *_scmanager_handle /* [in] [ref] */,
						   const char *_ServiceName /* [in] [charset(UTF16),unique] */,
						   uint32_t _access_mask /* [in]  */);
NTSTATUS dcerpc_svcctl_OpenServiceA_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_svcctl_OpenServiceA(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *_scmanager_handle /* [in] [ref] */,
				    const char *_ServiceName /* [in] [charset(UTF16),unique] */,
				    uint32_t _access_mask /* [in]  */,
				    WERROR *result);

struct tevent_req *dcerpc_svcctl_QueryServiceConfigA_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_QueryServiceConfigA *r);
NTSTATUS dcerpc_svcctl_QueryServiceConfigA_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_QueryServiceConfigA_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_QueryServiceConfigA *r);
struct tevent_req *dcerpc_svcctl_QueryServiceConfigA_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_handle /* [in] [ref] */,
							  uint8_t *_query /* [out]  */,
							  uint32_t _offered /* [in]  */,
							  uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_svcctl_QueryServiceConfigA_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_svcctl_QueryServiceConfigA(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_handle /* [in] [ref] */,
					   uint8_t *_query /* [out]  */,
					   uint32_t _offered /* [in]  */,
					   uint32_t *_needed /* [out] [ref] */,
					   WERROR *result);

struct tevent_req *dcerpc_svcctl_QueryServiceLockStatusA_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_QueryServiceLockStatusA *r);
NTSTATUS dcerpc_svcctl_QueryServiceLockStatusA_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_QueryServiceLockStatusA_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_QueryServiceLockStatusA *r);
struct tevent_req *dcerpc_svcctl_QueryServiceLockStatusA_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      struct policy_handle *_handle /* [in] [ref] */,
							      uint32_t _offered /* [in]  */,
							      struct SERVICE_LOCK_STATUS *_lock_status /* [out] [ref] */,
							      uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_svcctl_QueryServiceLockStatusA_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    WERROR *result);
NTSTATUS dcerpc_svcctl_QueryServiceLockStatusA(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       struct policy_handle *_handle /* [in] [ref] */,
					       uint32_t _offered /* [in]  */,
					       struct SERVICE_LOCK_STATUS *_lock_status /* [out] [ref] */,
					       uint32_t *_needed /* [out] [ref] */,
					       WERROR *result);

struct tevent_req *dcerpc_svcctl_StartServiceA_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_StartServiceA *r);
NTSTATUS dcerpc_svcctl_StartServiceA_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_StartServiceA_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_StartServiceA *r);
struct tevent_req *dcerpc_svcctl_StartServiceA_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_handle /* [in] [ref] */,
						    uint32_t _NumArgs /* [in]  */,
						    const char *_Arguments /* [in] [unique,charset(UTF16)] */);
NTSTATUS dcerpc_svcctl_StartServiceA_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_svcctl_StartServiceA(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_handle /* [in] [ref] */,
				     uint32_t _NumArgs /* [in]  */,
				     const char *_Arguments /* [in] [unique,charset(UTF16)] */,
				     WERROR *result);

struct tevent_req *dcerpc_svcctl_GetServiceDisplayNameA_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_GetServiceDisplayNameA *r);
NTSTATUS dcerpc_svcctl_GetServiceDisplayNameA_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_GetServiceDisplayNameA_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_GetServiceDisplayNameA *r);
struct tevent_req *dcerpc_svcctl_GetServiceDisplayNameA_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     struct policy_handle *_handle /* [in] [ref] */,
							     const char *_service_name /* [in] [unique,charset(UTF16)] */,
							     const char **_display_name /* [out] [charset(UTF16),ref] */,
							     uint32_t *_display_name_length /* [in,out] [unique] */);
NTSTATUS dcerpc_svcctl_GetServiceDisplayNameA_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_svcctl_GetServiceDisplayNameA(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      struct policy_handle *_handle /* [in] [ref] */,
					      const char *_service_name /* [in] [unique,charset(UTF16)] */,
					      const char **_display_name /* [out] [charset(UTF16),ref] */,
					      uint32_t *_display_name_length /* [in,out] [unique] */,
					      WERROR *result);

struct tevent_req *dcerpc_svcctl_GetServiceKeyNameA_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_GetServiceKeyNameA *r);
NTSTATUS dcerpc_svcctl_GetServiceKeyNameA_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_GetServiceKeyNameA_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_GetServiceKeyNameA *r);
struct tevent_req *dcerpc_svcctl_GetServiceKeyNameA_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_handle /* [in] [ref] */,
							 const char *_service_name /* [in] [unique,charset(UTF16)] */,
							 const char **_key_name /* [out] [ref,charset(UTF16)] */,
							 uint32_t *_display_name_length /* [in,out] [unique] */);
NTSTATUS dcerpc_svcctl_GetServiceKeyNameA_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_svcctl_GetServiceKeyNameA(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_handle /* [in] [ref] */,
					  const char *_service_name /* [in] [unique,charset(UTF16)] */,
					  const char **_key_name /* [out] [ref,charset(UTF16)] */,
					  uint32_t *_display_name_length /* [in,out] [unique] */,
					  WERROR *result);

struct tevent_req *dcerpc_svcctl_ChangeServiceConfig2A_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_ChangeServiceConfig2A *r);
NTSTATUS dcerpc_svcctl_ChangeServiceConfig2A_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_ChangeServiceConfig2A_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_ChangeServiceConfig2A *r);
struct tevent_req *dcerpc_svcctl_ChangeServiceConfig2A_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct dcerpc_binding_handle *h,
							    struct policy_handle *_handle /* [in] [ref] */,
							    uint32_t _info_level /* [in]  */,
							    uint8_t *_info /* [in] [unique] */);
NTSTATUS dcerpc_svcctl_ChangeServiceConfig2A_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS dcerpc_svcctl_ChangeServiceConfig2A(struct dcerpc_binding_handle *h,
					     TALLOC_CTX *mem_ctx,
					     struct policy_handle *_handle /* [in] [ref] */,
					     uint32_t _info_level /* [in]  */,
					     uint8_t *_info /* [in] [unique] */,
					     WERROR *result);

struct tevent_req *dcerpc_svcctl_ChangeServiceConfig2W_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_ChangeServiceConfig2W *r);
NTSTATUS dcerpc_svcctl_ChangeServiceConfig2W_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_ChangeServiceConfig2W_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_ChangeServiceConfig2W *r);
struct tevent_req *dcerpc_svcctl_ChangeServiceConfig2W_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct dcerpc_binding_handle *h,
							    struct policy_handle *_handle /* [in] [ref] */,
							    uint32_t _info_level /* [in]  */,
							    uint8_t *_info /* [in] [unique] */);
NTSTATUS dcerpc_svcctl_ChangeServiceConfig2W_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS dcerpc_svcctl_ChangeServiceConfig2W(struct dcerpc_binding_handle *h,
					     TALLOC_CTX *mem_ctx,
					     struct policy_handle *_handle /* [in] [ref] */,
					     uint32_t _info_level /* [in]  */,
					     uint8_t *_info /* [in] [unique] */,
					     WERROR *result);

struct tevent_req *dcerpc_svcctl_QueryServiceConfig2A_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_QueryServiceConfig2A *r);
NTSTATUS dcerpc_svcctl_QueryServiceConfig2A_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_QueryServiceConfig2A_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_QueryServiceConfig2A *r);
struct tevent_req *dcerpc_svcctl_QueryServiceConfig2A_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   struct policy_handle *_handle /* [in] [ref] */,
							   enum svcctl_ConfigLevel _info_level /* [in]  */,
							   uint8_t *_buffer /* [out]  */,
							   uint32_t _offered /* [in]  */,
							   uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_svcctl_QueryServiceConfig2A_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_svcctl_QueryServiceConfig2A(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    struct policy_handle *_handle /* [in] [ref] */,
					    enum svcctl_ConfigLevel _info_level /* [in]  */,
					    uint8_t *_buffer /* [out]  */,
					    uint32_t _offered /* [in]  */,
					    uint32_t *_needed /* [out] [ref] */,
					    WERROR *result);

struct tevent_req *dcerpc_svcctl_QueryServiceConfig2W_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_QueryServiceConfig2W *r);
NTSTATUS dcerpc_svcctl_QueryServiceConfig2W_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_QueryServiceConfig2W_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_QueryServiceConfig2W *r);
struct tevent_req *dcerpc_svcctl_QueryServiceConfig2W_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   struct policy_handle *_handle /* [in] [ref] */,
							   enum svcctl_ConfigLevel _info_level /* [in]  */,
							   uint8_t *_buffer /* [out] [ref,size_is(offered)] */,
							   uint32_t _offered /* [in] [range(0,8192)] */,
							   uint32_t *_needed /* [out] [range(0,8192),ref] */);
NTSTATUS dcerpc_svcctl_QueryServiceConfig2W_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_svcctl_QueryServiceConfig2W(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    struct policy_handle *_handle /* [in] [ref] */,
					    enum svcctl_ConfigLevel _info_level /* [in]  */,
					    uint8_t *_buffer /* [out] [ref,size_is(offered)] */,
					    uint32_t _offered /* [in] [range(0,8192)] */,
					    uint32_t *_needed /* [out] [range(0,8192),ref] */,
					    WERROR *result);

struct tevent_req *dcerpc_svcctl_QueryServiceStatusEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct svcctl_QueryServiceStatusEx *r);
NTSTATUS dcerpc_svcctl_QueryServiceStatusEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_svcctl_QueryServiceStatusEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct svcctl_QueryServiceStatusEx *r);
struct tevent_req *dcerpc_svcctl_QueryServiceStatusEx_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   struct policy_handle *_handle /* [in] [ref] */,
							   enum svcctl_StatusLevel _info_level /* [in]  */,
							   uint8_t *_buffer /* [out] [ref,size_is(offered)] */,
							   uint32_t _offered /* [in] [range(0,8192)] */,
							   uint32_t *_needed /* [out] [range(0,8192),ref] */);
NTSTATUS dcerpc_svcctl_QueryServiceStatusEx_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_svcctl_QueryServiceStatusEx(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    struct policy_handle *_handle /* [in] [ref] */,
					    enum svcctl_StatusLevel _info_level /* [in]  */,
					    uint8_t *_buffer /* [out] [ref,size_is(offered)] */,
					    uint32_t _offered /* [in] [range(0,8192)] */,
					    uint32_t *_needed /* [out] [range(0,8192),ref] */,
					    WERROR *result);

struct tevent_req *dcerpc_EnumServicesStatusExA_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct EnumServicesStatusExA *r);
NTSTATUS dcerpc_EnumServicesStatusExA_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_EnumServicesStatusExA_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct EnumServicesStatusExA *r);
struct tevent_req *dcerpc_EnumServicesStatusExA_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_scmanager /* [in] [ref] */,
						     uint32_t _info_level /* [in]  */,
						     uint32_t _type /* [in]  */,
						     enum svcctl_ServiceState _state /* [in]  */,
						     uint8_t *_services /* [out]  */,
						     uint32_t _offered /* [in]  */,
						     uint32_t *_needed /* [out] [ref] */,
						     uint32_t *_service_returned /* [out] [ref] */,
						     uint32_t *_resume_handle /* [in,out] [unique] */,
						     const char **_group_name /* [out] [charset(UTF16),ref] */);
NTSTATUS dcerpc_EnumServicesStatusExA_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_EnumServicesStatusExA(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_scmanager /* [in] [ref] */,
				      uint32_t _info_level /* [in]  */,
				      uint32_t _type /* [in]  */,
				      enum svcctl_ServiceState _state /* [in]  */,
				      uint8_t *_services /* [out]  */,
				      uint32_t _offered /* [in]  */,
				      uint32_t *_needed /* [out] [ref] */,
				      uint32_t *_service_returned /* [out] [ref] */,
				      uint32_t *_resume_handle /* [in,out] [unique] */,
				      const char **_group_name /* [out] [charset(UTF16),ref] */,
				      WERROR *result);

struct tevent_req *dcerpc_EnumServicesStatusExW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct EnumServicesStatusExW *r);
NTSTATUS dcerpc_EnumServicesStatusExW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_EnumServicesStatusExW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct EnumServicesStatusExW *r);
struct tevent_req *dcerpc_EnumServicesStatusExW_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_scmanager /* [in] [ref] */,
						     uint32_t _info_level /* [in]  */,
						     uint32_t _type /* [in]  */,
						     enum svcctl_ServiceState _state /* [in]  */,
						     uint8_t *_services /* [out] [ref,size_is(offered)] */,
						     uint32_t _offered /* [in] [range(0,0x40000)] */,
						     uint32_t *_needed /* [out] [ref,range(0,0x40000)] */,
						     uint32_t *_service_returned /* [out] [range(0,0x40000),ref] */,
						     uint32_t *_resume_handle /* [in,out] [range(0,0x40000),unique] */,
						     const char *_group_name /* [in] [unique,charset(UTF16)] */);
NTSTATUS dcerpc_EnumServicesStatusExW_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_EnumServicesStatusExW(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_scmanager /* [in] [ref] */,
				      uint32_t _info_level /* [in]  */,
				      uint32_t _type /* [in]  */,
				      enum svcctl_ServiceState _state /* [in]  */,
				      uint8_t *_services /* [out] [ref,size_is(offered)] */,
				      uint32_t _offered /* [in] [range(0,0x40000)] */,
				      uint32_t *_needed /* [out] [ref,range(0,0x40000)] */,
				      uint32_t *_service_returned /* [out] [range(0,0x40000),ref] */,
				      uint32_t *_resume_handle /* [in,out] [range(0,0x40000),unique] */,
				      const char *_group_name /* [in] [unique,charset(UTF16)] */,
				      WERROR *result);

#endif /* _HEADER_RPC_svcctl */
