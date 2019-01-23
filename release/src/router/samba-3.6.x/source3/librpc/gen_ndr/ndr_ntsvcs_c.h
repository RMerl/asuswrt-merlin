#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/ntsvcs.h"
#ifndef _HEADER_RPC_ntsvcs
#define _HEADER_RPC_ntsvcs

extern const struct ndr_interface_table ndr_table_ntsvcs;

struct tevent_req *dcerpc_PNP_GetVersion_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct PNP_GetVersion *r);
NTSTATUS dcerpc_PNP_GetVersion_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_PNP_GetVersion_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct PNP_GetVersion *r);
struct tevent_req *dcerpc_PNP_GetVersion_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      uint16_t *_version /* [out] [ref] */);
NTSTATUS dcerpc_PNP_GetVersion_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_PNP_GetVersion(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       uint16_t *_version /* [out] [ref] */,
			       WERROR *result);

struct tevent_req *dcerpc_PNP_ValidateDeviceInstance_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct PNP_ValidateDeviceInstance *r);
NTSTATUS dcerpc_PNP_ValidateDeviceInstance_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_PNP_ValidateDeviceInstance_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct PNP_ValidateDeviceInstance *r);
struct tevent_req *dcerpc_PNP_ValidateDeviceInstance_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  const char *_devicepath /* [in] [charset(UTF16),ref] */,
							  uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_PNP_ValidateDeviceInstance_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_PNP_ValidateDeviceInstance(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   const char *_devicepath /* [in] [charset(UTF16),ref] */,
					   uint32_t _flags /* [in]  */,
					   WERROR *result);

struct tevent_req *dcerpc_PNP_GetDeviceList_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct PNP_GetDeviceList *r);
NTSTATUS dcerpc_PNP_GetDeviceList_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_PNP_GetDeviceList_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct PNP_GetDeviceList *r);
struct tevent_req *dcerpc_PNP_GetDeviceList_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char *_filter /* [in] [charset(UTF16),unique] */,
						 uint16_t *_buffer /* [out] [size_is(*length),length_is(*length),ref] */,
						 uint32_t *_length /* [in,out] [ref] */,
						 uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_PNP_GetDeviceList_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_PNP_GetDeviceList(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char *_filter /* [in] [charset(UTF16),unique] */,
				  uint16_t *_buffer /* [out] [size_is(*length),length_is(*length),ref] */,
				  uint32_t *_length /* [in,out] [ref] */,
				  uint32_t _flags /* [in]  */,
				  WERROR *result);

struct tevent_req *dcerpc_PNP_GetDeviceListSize_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct PNP_GetDeviceListSize *r);
NTSTATUS dcerpc_PNP_GetDeviceListSize_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_PNP_GetDeviceListSize_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct PNP_GetDeviceListSize *r);
struct tevent_req *dcerpc_PNP_GetDeviceListSize_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     const char *_devicename /* [in] [charset(UTF16),unique] */,
						     uint32_t *_size /* [out] [ref] */,
						     uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_PNP_GetDeviceListSize_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_PNP_GetDeviceListSize(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      const char *_devicename /* [in] [charset(UTF16),unique] */,
				      uint32_t *_size /* [out] [ref] */,
				      uint32_t _flags /* [in]  */,
				      WERROR *result);

struct tevent_req *dcerpc_PNP_GetDeviceRegProp_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct PNP_GetDeviceRegProp *r);
NTSTATUS dcerpc_PNP_GetDeviceRegProp_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_PNP_GetDeviceRegProp_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct PNP_GetDeviceRegProp *r);
struct tevent_req *dcerpc_PNP_GetDeviceRegProp_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    const char *_devicepath /* [in] [charset(UTF16),ref] */,
						    uint32_t _property /* [in]  */,
						    enum winreg_Type *_reg_data_type /* [in,out] [ref] */,
						    uint8_t *_buffer /* [out] [ref,size_is(*buffer_size),length_is(*buffer_size)] */,
						    uint32_t *_buffer_size /* [in,out] [ref] */,
						    uint32_t *_needed /* [in,out] [ref] */,
						    uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_PNP_GetDeviceRegProp_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_PNP_GetDeviceRegProp(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char *_devicepath /* [in] [charset(UTF16),ref] */,
				     uint32_t _property /* [in]  */,
				     enum winreg_Type *_reg_data_type /* [in,out] [ref] */,
				     uint8_t *_buffer /* [out] [ref,size_is(*buffer_size),length_is(*buffer_size)] */,
				     uint32_t *_buffer_size /* [in,out] [ref] */,
				     uint32_t *_needed /* [in,out] [ref] */,
				     uint32_t _flags /* [in]  */,
				     WERROR *result);

struct tevent_req *dcerpc_PNP_HwProfFlags_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct PNP_HwProfFlags *r);
NTSTATUS dcerpc_PNP_HwProfFlags_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_PNP_HwProfFlags_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct PNP_HwProfFlags *r);
struct tevent_req *dcerpc_PNP_HwProfFlags_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       uint32_t _action /* [in]  */,
					       const char *_devicepath /* [in] [ref,charset(UTF16)] */,
					       uint32_t _config /* [in]  */,
					       uint32_t *_profile_flags /* [in,out] [ref] */,
					       uint16_t *_veto_type /* [in,out] [unique] */,
					       const char *_unknown5 /* [in] [unique,charset(UTF16)] */,
					       const char **_unknown5a /* [out] [unique,charset(UTF16)] */,
					       uint32_t _name_length /* [in]  */,
					       uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_PNP_HwProfFlags_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_PNP_HwProfFlags(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				uint32_t _action /* [in]  */,
				const char *_devicepath /* [in] [ref,charset(UTF16)] */,
				uint32_t _config /* [in]  */,
				uint32_t *_profile_flags /* [in,out] [ref] */,
				uint16_t *_veto_type /* [in,out] [unique] */,
				const char *_unknown5 /* [in] [unique,charset(UTF16)] */,
				const char **_unknown5a /* [out] [unique,charset(UTF16)] */,
				uint32_t _name_length /* [in]  */,
				uint32_t _flags /* [in]  */,
				WERROR *result);

struct tevent_req *dcerpc_PNP_GetHwProfInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct PNP_GetHwProfInfo *r);
NTSTATUS dcerpc_PNP_GetHwProfInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_PNP_GetHwProfInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct PNP_GetHwProfInfo *r);
struct tevent_req *dcerpc_PNP_GetHwProfInfo_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 uint32_t _idx /* [in]  */,
						 struct PNP_HwProfInfo *_info /* [in,out] [ref] */,
						 uint32_t _size /* [in]  */,
						 uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_PNP_GetHwProfInfo_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_PNP_GetHwProfInfo(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  uint32_t _idx /* [in]  */,
				  struct PNP_HwProfInfo *_info /* [in,out] [ref] */,
				  uint32_t _size /* [in]  */,
				  uint32_t _flags /* [in]  */,
				  WERROR *result);

#endif /* _HEADER_RPC_ntsvcs */
