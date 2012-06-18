#include "../librpc/gen_ndr/ndr_ntsvcs.h"
#ifndef __CLI_NTSVCS__
#define __CLI_NTSVCS__
struct tevent_req *rpccli_PNP_Disconnect_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_Disconnect_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS rpccli_PNP_Disconnect(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       WERROR *werror);
struct tevent_req *rpccli_PNP_Connect_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_Connect_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 WERROR *result);
NTSTATUS rpccli_PNP_Connect(struct rpc_pipe_client *cli,
			    TALLOC_CTX *mem_ctx,
			    WERROR *werror);
struct tevent_req *rpccli_PNP_GetVersion_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli,
					      uint16_t *_version /* [out] [ref] */);
NTSTATUS rpccli_PNP_GetVersion_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS rpccli_PNP_GetVersion(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       uint16_t *version /* [out] [ref] */,
			       WERROR *werror);
struct tevent_req *rpccli_PNP_GetGlobalState_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetGlobalState_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS rpccli_PNP_GetGlobalState(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   WERROR *werror);
struct tevent_req *rpccli_PNP_InitDetection_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_InitDetection_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS rpccli_PNP_InitDetection(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  WERROR *werror);
struct tevent_req *rpccli_PNP_ReportLogOn_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_ReportLogOn_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS rpccli_PNP_ReportLogOn(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				WERROR *werror);
struct tevent_req *rpccli_PNP_ValidateDeviceInstance_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct rpc_pipe_client *cli,
							  const char *_devicepath /* [in] [ref,charset(UTF16)] */,
							  uint32_t _flags /* [in]  */);
NTSTATUS rpccli_PNP_ValidateDeviceInstance_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS rpccli_PNP_ValidateDeviceInstance(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   const char *devicepath /* [in] [ref,charset(UTF16)] */,
					   uint32_t flags /* [in]  */,
					   WERROR *werror);
struct tevent_req *rpccli_PNP_GetRootDeviceInstance_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetRootDeviceInstance_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS rpccli_PNP_GetRootDeviceInstance(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  WERROR *werror);
struct tevent_req *rpccli_PNP_GetRelatedDeviceInstance_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetRelatedDeviceInstance_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS rpccli_PNP_GetRelatedDeviceInstance(struct rpc_pipe_client *cli,
					     TALLOC_CTX *mem_ctx,
					     WERROR *werror);
struct tevent_req *rpccli_PNP_EnumerateSubKeys_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_EnumerateSubKeys_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_PNP_EnumerateSubKeys(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     WERROR *werror);
struct tevent_req *rpccli_PNP_GetDeviceList_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli,
						 const char *_filter /* [in] [unique,charset(UTF16)] */,
						 uint16_t *_buffer /* [out] [ref,length_is(*length),size_is(*length)] */,
						 uint32_t *_length /* [in,out] [ref] */,
						 uint32_t _flags /* [in]  */);
NTSTATUS rpccli_PNP_GetDeviceList_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS rpccli_PNP_GetDeviceList(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  const char *filter /* [in] [unique,charset(UTF16)] */,
				  uint16_t *buffer /* [out] [ref,length_is(*length),size_is(*length)] */,
				  uint32_t *length /* [in,out] [ref] */,
				  uint32_t flags /* [in]  */,
				  WERROR *werror);
struct tevent_req *rpccli_PNP_GetDeviceListSize_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli,
						     const char *_devicename /* [in] [unique,charset(UTF16)] */,
						     uint32_t *_size /* [out] [ref] */,
						     uint32_t _flags /* [in]  */);
NTSTATUS rpccli_PNP_GetDeviceListSize_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS rpccli_PNP_GetDeviceListSize(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      const char *devicename /* [in] [unique,charset(UTF16)] */,
				      uint32_t *size /* [out] [ref] */,
				      uint32_t flags /* [in]  */,
				      WERROR *werror);
struct tevent_req *rpccli_PNP_GetDepth_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetDepth_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  WERROR *result);
NTSTATUS rpccli_PNP_GetDepth(struct rpc_pipe_client *cli,
			     TALLOC_CTX *mem_ctx,
			     WERROR *werror);
struct tevent_req *rpccli_PNP_GetDeviceRegProp_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    const char *_devicepath /* [in] [ref,charset(UTF16)] */,
						    uint32_t _property /* [in]  */,
						    enum winreg_Type *_reg_data_type /* [in,out] [ref] */,
						    uint8_t *_buffer /* [out] [ref,length_is(*buffer_size),size_is(*buffer_size)] */,
						    uint32_t *_buffer_size /* [in,out] [ref] */,
						    uint32_t *_needed /* [in,out] [ref] */,
						    uint32_t _flags /* [in]  */);
NTSTATUS rpccli_PNP_GetDeviceRegProp_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_PNP_GetDeviceRegProp(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     const char *devicepath /* [in] [ref,charset(UTF16)] */,
				     uint32_t property /* [in]  */,
				     enum winreg_Type *reg_data_type /* [in,out] [ref] */,
				     uint8_t *buffer /* [out] [ref,length_is(*buffer_size),size_is(*buffer_size)] */,
				     uint32_t *buffer_size /* [in,out] [ref] */,
				     uint32_t *needed /* [in,out] [ref] */,
				     uint32_t flags /* [in]  */,
				     WERROR *werror);
struct tevent_req *rpccli_PNP_SetDeviceRegProp_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_SetDeviceRegProp_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_PNP_SetDeviceRegProp(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     WERROR *werror);
struct tevent_req *rpccli_PNP_GetClassInstance_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetClassInstance_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_PNP_GetClassInstance(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     WERROR *werror);
struct tevent_req *rpccli_PNP_CreateKey_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_CreateKey_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   WERROR *result);
NTSTATUS rpccli_PNP_CreateKey(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      WERROR *werror);
struct tevent_req *rpccli_PNP_DeleteRegistryKey_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_DeleteRegistryKey_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS rpccli_PNP_DeleteRegistryKey(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      WERROR *werror);
struct tevent_req *rpccli_PNP_GetClassCount_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetClassCount_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS rpccli_PNP_GetClassCount(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  WERROR *werror);
struct tevent_req *rpccli_PNP_GetClassName_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetClassName_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS rpccli_PNP_GetClassName(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 WERROR *werror);
struct tevent_req *rpccli_PNP_DeleteClassKey_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_DeleteClassKey_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS rpccli_PNP_DeleteClassKey(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   WERROR *werror);
struct tevent_req *rpccli_PNP_GetInterfaceDeviceAlias_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetInterfaceDeviceAlias_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS rpccli_PNP_GetInterfaceDeviceAlias(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    WERROR *werror);
struct tevent_req *rpccli_PNP_GetInterfaceDeviceList_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetInterfaceDeviceList_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS rpccli_PNP_GetInterfaceDeviceList(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   WERROR *werror);
struct tevent_req *rpccli_PNP_GetInterfaceDeviceListSize_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetInterfaceDeviceListSize_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    WERROR *result);
NTSTATUS rpccli_PNP_GetInterfaceDeviceListSize(struct rpc_pipe_client *cli,
					       TALLOC_CTX *mem_ctx,
					       WERROR *werror);
struct tevent_req *rpccli_PNP_RegisterDeviceClassAssociation_send(TALLOC_CTX *mem_ctx,
								  struct tevent_context *ev,
								  struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_RegisterDeviceClassAssociation_recv(struct tevent_req *req,
							TALLOC_CTX *mem_ctx,
							WERROR *result);
NTSTATUS rpccli_PNP_RegisterDeviceClassAssociation(struct rpc_pipe_client *cli,
						   TALLOC_CTX *mem_ctx,
						   WERROR *werror);
struct tevent_req *rpccli_PNP_UnregisterDeviceClassAssociation_send(TALLOC_CTX *mem_ctx,
								    struct tevent_context *ev,
								    struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_UnregisterDeviceClassAssociation_recv(struct tevent_req *req,
							  TALLOC_CTX *mem_ctx,
							  WERROR *result);
NTSTATUS rpccli_PNP_UnregisterDeviceClassAssociation(struct rpc_pipe_client *cli,
						     TALLOC_CTX *mem_ctx,
						     WERROR *werror);
struct tevent_req *rpccli_PNP_GetClassRegProp_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetClassRegProp_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS rpccli_PNP_GetClassRegProp(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    WERROR *werror);
struct tevent_req *rpccli_PNP_SetClassRegProp_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_SetClassRegProp_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS rpccli_PNP_SetClassRegProp(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    WERROR *werror);
struct tevent_req *rpccli_PNP_CreateDevInst_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_CreateDevInst_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS rpccli_PNP_CreateDevInst(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  WERROR *werror);
struct tevent_req *rpccli_PNP_DeviceInstanceAction_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_DeviceInstanceAction_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_PNP_DeviceInstanceAction(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 WERROR *werror);
struct tevent_req *rpccli_PNP_GetDeviceStatus_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetDeviceStatus_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS rpccli_PNP_GetDeviceStatus(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    WERROR *werror);
struct tevent_req *rpccli_PNP_SetDeviceProblem_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_SetDeviceProblem_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_PNP_SetDeviceProblem(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     WERROR *werror);
struct tevent_req *rpccli_PNP_DisableDevInst_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_DisableDevInst_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS rpccli_PNP_DisableDevInst(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   WERROR *werror);
struct tevent_req *rpccli_PNP_UninstallDevInst_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_UninstallDevInst_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_PNP_UninstallDevInst(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     WERROR *werror);
struct tevent_req *rpccli_PNP_AddID_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_AddID_recv(struct tevent_req *req,
			       TALLOC_CTX *mem_ctx,
			       WERROR *result);
NTSTATUS rpccli_PNP_AddID(struct rpc_pipe_client *cli,
			  TALLOC_CTX *mem_ctx,
			  WERROR *werror);
struct tevent_req *rpccli_PNP_RegisterDriver_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_RegisterDriver_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS rpccli_PNP_RegisterDriver(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   WERROR *werror);
struct tevent_req *rpccli_PNP_QueryRemove_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_QueryRemove_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS rpccli_PNP_QueryRemove(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				WERROR *werror);
struct tevent_req *rpccli_PNP_RequestDeviceEject_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_RequestDeviceEject_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS rpccli_PNP_RequestDeviceEject(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       WERROR *werror);
struct tevent_req *rpccli_PNP_IsDockStationPresent_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_IsDockStationPresent_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_PNP_IsDockStationPresent(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 WERROR *werror);
struct tevent_req *rpccli_PNP_RequestEjectPC_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_RequestEjectPC_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS rpccli_PNP_RequestEjectPC(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   WERROR *werror);
struct tevent_req *rpccli_PNP_HwProfFlags_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       uint32_t _action /* [in]  */,
					       const char *_devicepath /* [in] [ref,charset(UTF16)] */,
					       uint32_t _config /* [in]  */,
					       uint32_t *_profile_flags /* [in,out] [ref] */,
					       uint16_t *_veto_type /* [in,out] [unique] */,
					       const char *_unknown5 /* [in] [unique,charset(UTF16)] */,
					       const char **_unknown5a /* [out] [unique,charset(UTF16)] */,
					       uint32_t _name_length /* [in]  */,
					       uint32_t _flags /* [in]  */);
NTSTATUS rpccli_PNP_HwProfFlags_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS rpccli_PNP_HwProfFlags(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				uint32_t action /* [in]  */,
				const char *devicepath /* [in] [ref,charset(UTF16)] */,
				uint32_t config /* [in]  */,
				uint32_t *profile_flags /* [in,out] [ref] */,
				uint16_t *veto_type /* [in,out] [unique] */,
				const char *unknown5 /* [in] [unique,charset(UTF16)] */,
				const char **unknown5a /* [out] [unique,charset(UTF16)] */,
				uint32_t name_length /* [in]  */,
				uint32_t flags /* [in]  */,
				WERROR *werror);
struct tevent_req *rpccli_PNP_GetHwProfInfo_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli,
						 uint32_t _idx /* [in]  */,
						 struct PNP_HwProfInfo *_info /* [in,out] [ref] */,
						 uint32_t _size /* [in]  */,
						 uint32_t _flags /* [in]  */);
NTSTATUS rpccli_PNP_GetHwProfInfo_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS rpccli_PNP_GetHwProfInfo(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  uint32_t idx /* [in]  */,
				  struct PNP_HwProfInfo *info /* [in,out] [ref] */,
				  uint32_t size /* [in]  */,
				  uint32_t flags /* [in]  */,
				  WERROR *werror);
struct tevent_req *rpccli_PNP_AddEmptyLogConf_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_AddEmptyLogConf_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS rpccli_PNP_AddEmptyLogConf(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    WERROR *werror);
struct tevent_req *rpccli_PNP_FreeLogConf_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_FreeLogConf_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS rpccli_PNP_FreeLogConf(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				WERROR *werror);
struct tevent_req *rpccli_PNP_GetFirstLogConf_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetFirstLogConf_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS rpccli_PNP_GetFirstLogConf(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    WERROR *werror);
struct tevent_req *rpccli_PNP_GetNextLogConf_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetNextLogConf_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS rpccli_PNP_GetNextLogConf(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   WERROR *werror);
struct tevent_req *rpccli_PNP_GetLogConfPriority_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetLogConfPriority_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS rpccli_PNP_GetLogConfPriority(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       WERROR *werror);
struct tevent_req *rpccli_PNP_AddResDes_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_AddResDes_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   WERROR *result);
NTSTATUS rpccli_PNP_AddResDes(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      WERROR *werror);
struct tevent_req *rpccli_PNP_FreeResDes_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_FreeResDes_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS rpccli_PNP_FreeResDes(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       WERROR *werror);
struct tevent_req *rpccli_PNP_GetNextResDes_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetNextResDes_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS rpccli_PNP_GetNextResDes(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  WERROR *werror);
struct tevent_req *rpccli_PNP_GetResDesData_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetResDesData_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS rpccli_PNP_GetResDesData(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  WERROR *werror);
struct tevent_req *rpccli_PNP_GetResDesDataSize_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetResDesDataSize_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS rpccli_PNP_GetResDesDataSize(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      WERROR *werror);
struct tevent_req *rpccli_PNP_ModifyResDes_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_ModifyResDes_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS rpccli_PNP_ModifyResDes(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 WERROR *werror);
struct tevent_req *rpccli_PNP_DetectResourceLimit_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_DetectResourceLimit_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS rpccli_PNP_DetectResourceLimit(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					WERROR *werror);
struct tevent_req *rpccli_PNP_QueryResConfList_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_QueryResConfList_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_PNP_QueryResConfList(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     WERROR *werror);
struct tevent_req *rpccli_PNP_SetHwProf_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_SetHwProf_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   WERROR *result);
NTSTATUS rpccli_PNP_SetHwProf(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      WERROR *werror);
struct tevent_req *rpccli_PNP_QueryArbitratorFreeData_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_QueryArbitratorFreeData_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS rpccli_PNP_QueryArbitratorFreeData(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    WERROR *werror);
struct tevent_req *rpccli_PNP_QueryArbitratorFreeSize_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_QueryArbitratorFreeSize_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS rpccli_PNP_QueryArbitratorFreeSize(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    WERROR *werror);
struct tevent_req *rpccli_PNP_RunDetection_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_RunDetection_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS rpccli_PNP_RunDetection(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 WERROR *werror);
struct tevent_req *rpccli_PNP_RegisterNotification_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_RegisterNotification_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_PNP_RegisterNotification(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 WERROR *werror);
struct tevent_req *rpccli_PNP_UnregisterNotification_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_UnregisterNotification_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS rpccli_PNP_UnregisterNotification(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   WERROR *werror);
struct tevent_req *rpccli_PNP_GetCustomDevProp_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetCustomDevProp_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_PNP_GetCustomDevProp(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     WERROR *werror);
struct tevent_req *rpccli_PNP_GetVersionInternal_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetVersionInternal_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS rpccli_PNP_GetVersionInternal(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       WERROR *werror);
struct tevent_req *rpccli_PNP_GetBlockedDriverInfo_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetBlockedDriverInfo_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_PNP_GetBlockedDriverInfo(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 WERROR *werror);
struct tevent_req *rpccli_PNP_GetServerSideDeviceInstallFlags_send(TALLOC_CTX *mem_ctx,
								   struct tevent_context *ev,
								   struct rpc_pipe_client *cli);
NTSTATUS rpccli_PNP_GetServerSideDeviceInstallFlags_recv(struct tevent_req *req,
							 TALLOC_CTX *mem_ctx,
							 WERROR *result);
NTSTATUS rpccli_PNP_GetServerSideDeviceInstallFlags(struct rpc_pipe_client *cli,
						    TALLOC_CTX *mem_ctx,
						    WERROR *werror);
#endif /* __CLI_NTSVCS__ */
