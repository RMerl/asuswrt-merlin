#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/winreg.h"
#ifndef _HEADER_RPC_winreg
#define _HEADER_RPC_winreg

extern const struct ndr_interface_table ndr_table_winreg;

struct tevent_req *dcerpc_winreg_OpenHKCR_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_OpenHKCR *r);
NTSTATUS dcerpc_winreg_OpenHKCR_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_OpenHKCR_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_OpenHKCR *r);
struct tevent_req *dcerpc_winreg_OpenHKCR_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       uint16_t *_system_name /* [in] [unique] */,
					       uint32_t _access_mask /* [in]  */,
					       struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_winreg_OpenHKCR_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_winreg_OpenHKCR(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				uint16_t *_system_name /* [in] [unique] */,
				uint32_t _access_mask /* [in]  */,
				struct policy_handle *_handle /* [out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_winreg_OpenHKCU_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_OpenHKCU *r);
NTSTATUS dcerpc_winreg_OpenHKCU_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_OpenHKCU_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_OpenHKCU *r);
struct tevent_req *dcerpc_winreg_OpenHKCU_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       uint16_t *_system_name /* [in] [unique] */,
					       uint32_t _access_mask /* [in]  */,
					       struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_winreg_OpenHKCU_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_winreg_OpenHKCU(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				uint16_t *_system_name /* [in] [unique] */,
				uint32_t _access_mask /* [in]  */,
				struct policy_handle *_handle /* [out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_winreg_OpenHKLM_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_OpenHKLM *r);
NTSTATUS dcerpc_winreg_OpenHKLM_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_OpenHKLM_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_OpenHKLM *r);
struct tevent_req *dcerpc_winreg_OpenHKLM_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       uint16_t *_system_name /* [in] [unique] */,
					       uint32_t _access_mask /* [in]  */,
					       struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_winreg_OpenHKLM_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_winreg_OpenHKLM(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				uint16_t *_system_name /* [in] [unique] */,
				uint32_t _access_mask /* [in]  */,
				struct policy_handle *_handle /* [out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_winreg_OpenHKPD_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_OpenHKPD *r);
NTSTATUS dcerpc_winreg_OpenHKPD_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_OpenHKPD_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_OpenHKPD *r);
struct tevent_req *dcerpc_winreg_OpenHKPD_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       uint16_t *_system_name /* [in] [unique] */,
					       uint32_t _access_mask /* [in]  */,
					       struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_winreg_OpenHKPD_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_winreg_OpenHKPD(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				uint16_t *_system_name /* [in] [unique] */,
				uint32_t _access_mask /* [in]  */,
				struct policy_handle *_handle /* [out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_winreg_OpenHKU_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_OpenHKU *r);
NTSTATUS dcerpc_winreg_OpenHKU_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_OpenHKU_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_OpenHKU *r);
struct tevent_req *dcerpc_winreg_OpenHKU_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      uint16_t *_system_name /* [in] [unique] */,
					      uint32_t _access_mask /* [in]  */,
					      struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_winreg_OpenHKU_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_winreg_OpenHKU(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       uint16_t *_system_name /* [in] [unique] */,
			       uint32_t _access_mask /* [in]  */,
			       struct policy_handle *_handle /* [out] [ref] */,
			       WERROR *result);

struct tevent_req *dcerpc_winreg_CloseKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_CloseKey *r);
NTSTATUS dcerpc_winreg_CloseKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_CloseKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_CloseKey *r);
struct tevent_req *dcerpc_winreg_CloseKey_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_winreg_CloseKey_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_winreg_CloseKey(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_handle /* [in,out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_winreg_CreateKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_CreateKey *r);
NTSTATUS dcerpc_winreg_CreateKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_CreateKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_CreateKey *r);
struct tevent_req *dcerpc_winreg_CreateKey_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_handle /* [in] [ref] */,
						struct winreg_String _name /* [in]  */,
						struct winreg_String _keyclass /* [in]  */,
						uint32_t _options /* [in]  */,
						uint32_t _access_mask /* [in]  */,
						struct winreg_SecBuf *_secdesc /* [in] [unique] */,
						struct policy_handle *_new_handle /* [out] [ref] */,
						enum winreg_CreateAction *_action_taken /* [in,out] [unique] */);
NTSTATUS dcerpc_winreg_CreateKey_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_winreg_CreateKey(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_handle /* [in] [ref] */,
				 struct winreg_String _name /* [in]  */,
				 struct winreg_String _keyclass /* [in]  */,
				 uint32_t _options /* [in]  */,
				 uint32_t _access_mask /* [in]  */,
				 struct winreg_SecBuf *_secdesc /* [in] [unique] */,
				 struct policy_handle *_new_handle /* [out] [ref] */,
				 enum winreg_CreateAction *_action_taken /* [in,out] [unique] */,
				 WERROR *result);

struct tevent_req *dcerpc_winreg_DeleteKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_DeleteKey *r);
NTSTATUS dcerpc_winreg_DeleteKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_DeleteKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_DeleteKey *r);
struct tevent_req *dcerpc_winreg_DeleteKey_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_handle /* [in] [ref] */,
						struct winreg_String _key /* [in]  */);
NTSTATUS dcerpc_winreg_DeleteKey_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_winreg_DeleteKey(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_handle /* [in] [ref] */,
				 struct winreg_String _key /* [in]  */,
				 WERROR *result);

struct tevent_req *dcerpc_winreg_DeleteValue_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_DeleteValue *r);
NTSTATUS dcerpc_winreg_DeleteValue_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_DeleteValue_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_DeleteValue *r);
struct tevent_req *dcerpc_winreg_DeleteValue_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct policy_handle *_handle /* [in] [ref] */,
						  struct winreg_String _value /* [in]  */);
NTSTATUS dcerpc_winreg_DeleteValue_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_winreg_DeleteValue(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *_handle /* [in] [ref] */,
				   struct winreg_String _value /* [in]  */,
				   WERROR *result);

struct tevent_req *dcerpc_winreg_EnumKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_EnumKey *r);
NTSTATUS dcerpc_winreg_EnumKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_EnumKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_EnumKey *r);
struct tevent_req *dcerpc_winreg_EnumKey_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct policy_handle *_handle /* [in] [ref] */,
					      uint32_t _enum_index /* [in]  */,
					      struct winreg_StringBuf *_name /* [in,out] [ref] */,
					      struct winreg_StringBuf *_keyclass /* [in,out] [unique] */,
					      NTTIME *_last_changed_time /* [in,out] [unique] */);
NTSTATUS dcerpc_winreg_EnumKey_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_winreg_EnumKey(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *_handle /* [in] [ref] */,
			       uint32_t _enum_index /* [in]  */,
			       struct winreg_StringBuf *_name /* [in,out] [ref] */,
			       struct winreg_StringBuf *_keyclass /* [in,out] [unique] */,
			       NTTIME *_last_changed_time /* [in,out] [unique] */,
			       WERROR *result);

struct tevent_req *dcerpc_winreg_EnumValue_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_EnumValue *r);
NTSTATUS dcerpc_winreg_EnumValue_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_EnumValue_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_EnumValue *r);
struct tevent_req *dcerpc_winreg_EnumValue_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_handle /* [in] [ref] */,
						uint32_t _enum_index /* [in]  */,
						struct winreg_ValNameBuf *_name /* [in,out] [ref] */,
						enum winreg_Type *_type /* [in,out] [unique] */,
						uint8_t *_value /* [in,out] [size_is(size?*size:0),unique,length_is(length?*length:0),range(0,0x4000000)] */,
						uint32_t *_size /* [in,out] [unique] */,
						uint32_t *_length /* [in,out] [unique] */);
NTSTATUS dcerpc_winreg_EnumValue_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_winreg_EnumValue(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_handle /* [in] [ref] */,
				 uint32_t _enum_index /* [in]  */,
				 struct winreg_ValNameBuf *_name /* [in,out] [ref] */,
				 enum winreg_Type *_type /* [in,out] [unique] */,
				 uint8_t *_value /* [in,out] [size_is(size?*size:0),unique,length_is(length?*length:0),range(0,0x4000000)] */,
				 uint32_t *_size /* [in,out] [unique] */,
				 uint32_t *_length /* [in,out] [unique] */,
				 WERROR *result);

struct tevent_req *dcerpc_winreg_FlushKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_FlushKey *r);
NTSTATUS dcerpc_winreg_FlushKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_FlushKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_FlushKey *r);
struct tevent_req *dcerpc_winreg_FlushKey_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS dcerpc_winreg_FlushKey_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_winreg_FlushKey(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_handle /* [in] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_winreg_GetKeySecurity_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_GetKeySecurity *r);
NTSTATUS dcerpc_winreg_GetKeySecurity_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_GetKeySecurity_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_GetKeySecurity *r);
struct tevent_req *dcerpc_winreg_GetKeySecurity_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_handle /* [in] [ref] */,
						     uint32_t _sec_info /* [in]  */,
						     struct KeySecurityData *_sd /* [in,out] [ref] */);
NTSTATUS dcerpc_winreg_GetKeySecurity_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_winreg_GetKeySecurity(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_handle /* [in] [ref] */,
				      uint32_t _sec_info /* [in]  */,
				      struct KeySecurityData *_sd /* [in,out] [ref] */,
				      WERROR *result);

struct tevent_req *dcerpc_winreg_LoadKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_LoadKey *r);
NTSTATUS dcerpc_winreg_LoadKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_LoadKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_LoadKey *r);
struct tevent_req *dcerpc_winreg_LoadKey_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct policy_handle *_handle /* [in] [ref] */,
					      struct winreg_String *_keyname /* [in] [unique] */,
					      struct winreg_String *_filename /* [in] [unique] */);
NTSTATUS dcerpc_winreg_LoadKey_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_winreg_LoadKey(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *_handle /* [in] [ref] */,
			       struct winreg_String *_keyname /* [in] [unique] */,
			       struct winreg_String *_filename /* [in] [unique] */,
			       WERROR *result);

struct tevent_req *dcerpc_winreg_NotifyChangeKeyValue_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_NotifyChangeKeyValue *r);
NTSTATUS dcerpc_winreg_NotifyChangeKeyValue_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_NotifyChangeKeyValue_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_NotifyChangeKeyValue *r);
struct tevent_req *dcerpc_winreg_NotifyChangeKeyValue_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   struct policy_handle *_handle /* [in] [ref] */,
							   uint8_t _watch_subtree /* [in]  */,
							   uint32_t _notify_filter /* [in]  */,
							   uint32_t _unknown /* [in]  */,
							   struct winreg_String _string1 /* [in]  */,
							   struct winreg_String _string2 /* [in]  */,
							   uint32_t _unknown2 /* [in]  */);
NTSTATUS dcerpc_winreg_NotifyChangeKeyValue_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_winreg_NotifyChangeKeyValue(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    struct policy_handle *_handle /* [in] [ref] */,
					    uint8_t _watch_subtree /* [in]  */,
					    uint32_t _notify_filter /* [in]  */,
					    uint32_t _unknown /* [in]  */,
					    struct winreg_String _string1 /* [in]  */,
					    struct winreg_String _string2 /* [in]  */,
					    uint32_t _unknown2 /* [in]  */,
					    WERROR *result);

struct tevent_req *dcerpc_winreg_OpenKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_OpenKey *r);
NTSTATUS dcerpc_winreg_OpenKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_OpenKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_OpenKey *r);
struct tevent_req *dcerpc_winreg_OpenKey_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct policy_handle *_parent_handle /* [in] [ref] */,
					      struct winreg_String _keyname /* [in]  */,
					      uint32_t _options /* [in]  */,
					      uint32_t _access_mask /* [in]  */,
					      struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_winreg_OpenKey_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_winreg_OpenKey(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *_parent_handle /* [in] [ref] */,
			       struct winreg_String _keyname /* [in]  */,
			       uint32_t _options /* [in]  */,
			       uint32_t _access_mask /* [in]  */,
			       struct policy_handle *_handle /* [out] [ref] */,
			       WERROR *result);

struct tevent_req *dcerpc_winreg_QueryInfoKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_QueryInfoKey *r);
NTSTATUS dcerpc_winreg_QueryInfoKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_QueryInfoKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_QueryInfoKey *r);
struct tevent_req *dcerpc_winreg_QueryInfoKey_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct policy_handle *_handle /* [in] [ref] */,
						   struct winreg_String *_classname /* [in,out] [ref] */,
						   uint32_t *_num_subkeys /* [out] [ref] */,
						   uint32_t *_max_subkeylen /* [out] [ref] */,
						   uint32_t *_max_classlen /* [out] [ref] */,
						   uint32_t *_num_values /* [out] [ref] */,
						   uint32_t *_max_valnamelen /* [out] [ref] */,
						   uint32_t *_max_valbufsize /* [out] [ref] */,
						   uint32_t *_secdescsize /* [out] [ref] */,
						   NTTIME *_last_changed_time /* [out] [ref] */);
NTSTATUS dcerpc_winreg_QueryInfoKey_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_winreg_QueryInfoKey(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *_handle /* [in] [ref] */,
				    struct winreg_String *_classname /* [in,out] [ref] */,
				    uint32_t *_num_subkeys /* [out] [ref] */,
				    uint32_t *_max_subkeylen /* [out] [ref] */,
				    uint32_t *_max_classlen /* [out] [ref] */,
				    uint32_t *_num_values /* [out] [ref] */,
				    uint32_t *_max_valnamelen /* [out] [ref] */,
				    uint32_t *_max_valbufsize /* [out] [ref] */,
				    uint32_t *_secdescsize /* [out] [ref] */,
				    NTTIME *_last_changed_time /* [out] [ref] */,
				    WERROR *result);

struct tevent_req *dcerpc_winreg_QueryValue_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_QueryValue *r);
NTSTATUS dcerpc_winreg_QueryValue_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_QueryValue_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_QueryValue *r);
struct tevent_req *dcerpc_winreg_QueryValue_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct policy_handle *_handle /* [in] [ref] */,
						 struct winreg_String *_value_name /* [in] [ref] */,
						 enum winreg_Type *_type /* [in,out] [unique] */,
						 uint8_t *_data /* [in,out] [size_is(data_size?*data_size:0),unique,length_is(data_length?*data_length:0),range(0,0x4000000)] */,
						 uint32_t *_data_size /* [in,out] [unique] */,
						 uint32_t *_data_length /* [in,out] [unique] */);
NTSTATUS dcerpc_winreg_QueryValue_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_winreg_QueryValue(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *_handle /* [in] [ref] */,
				  struct winreg_String *_value_name /* [in] [ref] */,
				  enum winreg_Type *_type /* [in,out] [unique] */,
				  uint8_t *_data /* [in,out] [size_is(data_size?*data_size:0),unique,length_is(data_length?*data_length:0),range(0,0x4000000)] */,
				  uint32_t *_data_size /* [in,out] [unique] */,
				  uint32_t *_data_length /* [in,out] [unique] */,
				  WERROR *result);

struct tevent_req *dcerpc_winreg_ReplaceKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_ReplaceKey *r);
NTSTATUS dcerpc_winreg_ReplaceKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_ReplaceKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_ReplaceKey *r);
struct tevent_req *dcerpc_winreg_ReplaceKey_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct policy_handle *_handle /* [in] [ref] */,
						 struct winreg_String *_subkey /* [in] [ref] */,
						 struct winreg_String *_new_file /* [in] [ref] */,
						 struct winreg_String *_old_file /* [in] [ref] */);
NTSTATUS dcerpc_winreg_ReplaceKey_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_winreg_ReplaceKey(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *_handle /* [in] [ref] */,
				  struct winreg_String *_subkey /* [in] [ref] */,
				  struct winreg_String *_new_file /* [in] [ref] */,
				  struct winreg_String *_old_file /* [in] [ref] */,
				  WERROR *result);

struct tevent_req *dcerpc_winreg_RestoreKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_RestoreKey *r);
NTSTATUS dcerpc_winreg_RestoreKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_RestoreKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_RestoreKey *r);
struct tevent_req *dcerpc_winreg_RestoreKey_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct policy_handle *_handle /* [in] [ref] */,
						 struct winreg_String *_filename /* [in] [ref] */,
						 uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_winreg_RestoreKey_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_winreg_RestoreKey(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *_handle /* [in] [ref] */,
				  struct winreg_String *_filename /* [in] [ref] */,
				  uint32_t _flags /* [in]  */,
				  WERROR *result);

struct tevent_req *dcerpc_winreg_SaveKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_SaveKey *r);
NTSTATUS dcerpc_winreg_SaveKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_SaveKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_SaveKey *r);
struct tevent_req *dcerpc_winreg_SaveKey_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct policy_handle *_handle /* [in] [ref] */,
					      struct winreg_String *_filename /* [in] [ref] */,
					      struct KeySecurityAttribute *_sec_attrib /* [in] [unique] */);
NTSTATUS dcerpc_winreg_SaveKey_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_winreg_SaveKey(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *_handle /* [in] [ref] */,
			       struct winreg_String *_filename /* [in] [ref] */,
			       struct KeySecurityAttribute *_sec_attrib /* [in] [unique] */,
			       WERROR *result);

struct tevent_req *dcerpc_winreg_SetKeySecurity_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_SetKeySecurity *r);
NTSTATUS dcerpc_winreg_SetKeySecurity_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_SetKeySecurity_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_SetKeySecurity *r);
struct tevent_req *dcerpc_winreg_SetKeySecurity_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_handle /* [in] [ref] */,
						     uint32_t _sec_info /* [in]  */,
						     struct KeySecurityData *_sd /* [in] [ref] */);
NTSTATUS dcerpc_winreg_SetKeySecurity_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_winreg_SetKeySecurity(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_handle /* [in] [ref] */,
				      uint32_t _sec_info /* [in]  */,
				      struct KeySecurityData *_sd /* [in] [ref] */,
				      WERROR *result);

struct tevent_req *dcerpc_winreg_SetValue_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_SetValue *r);
NTSTATUS dcerpc_winreg_SetValue_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_SetValue_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_SetValue *r);
struct tevent_req *dcerpc_winreg_SetValue_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_handle /* [in] [ref] */,
					       struct winreg_String _name /* [in]  */,
					       enum winreg_Type _type /* [in]  */,
					       uint8_t *_data /* [in] [ref,size_is(size)] */,
					       uint32_t _size /* [in]  */);
NTSTATUS dcerpc_winreg_SetValue_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_winreg_SetValue(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_handle /* [in] [ref] */,
				struct winreg_String _name /* [in]  */,
				enum winreg_Type _type /* [in]  */,
				uint8_t *_data /* [in] [ref,size_is(size)] */,
				uint32_t _size /* [in]  */,
				WERROR *result);

struct tevent_req *dcerpc_winreg_UnLoadKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_UnLoadKey *r);
NTSTATUS dcerpc_winreg_UnLoadKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_UnLoadKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_UnLoadKey *r);
struct tevent_req *dcerpc_winreg_UnLoadKey_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_handle /* [in] [ref] */,
						struct winreg_String *_subkey /* [in] [ref] */);
NTSTATUS dcerpc_winreg_UnLoadKey_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_winreg_UnLoadKey(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_handle /* [in] [ref] */,
				 struct winreg_String *_subkey /* [in] [ref] */,
				 WERROR *result);

struct tevent_req *dcerpc_winreg_InitiateSystemShutdown_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_InitiateSystemShutdown *r);
NTSTATUS dcerpc_winreg_InitiateSystemShutdown_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_InitiateSystemShutdown_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_InitiateSystemShutdown *r);
struct tevent_req *dcerpc_winreg_InitiateSystemShutdown_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     uint16_t *_hostname /* [in] [unique] */,
							     struct lsa_StringLarge *_message /* [in] [unique] */,
							     uint32_t _timeout /* [in]  */,
							     uint8_t _force_apps /* [in]  */,
							     uint8_t _do_reboot /* [in]  */);
NTSTATUS dcerpc_winreg_InitiateSystemShutdown_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_winreg_InitiateSystemShutdown(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      uint16_t *_hostname /* [in] [unique] */,
					      struct lsa_StringLarge *_message /* [in] [unique] */,
					      uint32_t _timeout /* [in]  */,
					      uint8_t _force_apps /* [in]  */,
					      uint8_t _do_reboot /* [in]  */,
					      WERROR *result);

struct tevent_req *dcerpc_winreg_AbortSystemShutdown_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_AbortSystemShutdown *r);
NTSTATUS dcerpc_winreg_AbortSystemShutdown_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_AbortSystemShutdown_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_AbortSystemShutdown *r);
struct tevent_req *dcerpc_winreg_AbortSystemShutdown_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  uint16_t *_server /* [in] [unique] */);
NTSTATUS dcerpc_winreg_AbortSystemShutdown_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_winreg_AbortSystemShutdown(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   uint16_t *_server /* [in] [unique] */,
					   WERROR *result);

struct tevent_req *dcerpc_winreg_GetVersion_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_GetVersion *r);
NTSTATUS dcerpc_winreg_GetVersion_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_GetVersion_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_GetVersion *r);
struct tevent_req *dcerpc_winreg_GetVersion_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct policy_handle *_handle /* [in] [ref] */,
						 uint32_t *_version /* [out] [ref] */);
NTSTATUS dcerpc_winreg_GetVersion_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_winreg_GetVersion(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *_handle /* [in] [ref] */,
				  uint32_t *_version /* [out] [ref] */,
				  WERROR *result);

struct tevent_req *dcerpc_winreg_OpenHKCC_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_OpenHKCC *r);
NTSTATUS dcerpc_winreg_OpenHKCC_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_OpenHKCC_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_OpenHKCC *r);
struct tevent_req *dcerpc_winreg_OpenHKCC_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       uint16_t *_system_name /* [in] [unique] */,
					       uint32_t _access_mask /* [in]  */,
					       struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_winreg_OpenHKCC_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_winreg_OpenHKCC(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				uint16_t *_system_name /* [in] [unique] */,
				uint32_t _access_mask /* [in]  */,
				struct policy_handle *_handle /* [out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_winreg_OpenHKDD_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_OpenHKDD *r);
NTSTATUS dcerpc_winreg_OpenHKDD_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_OpenHKDD_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_OpenHKDD *r);
struct tevent_req *dcerpc_winreg_OpenHKDD_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       uint16_t *_system_name /* [in] [unique] */,
					       uint32_t _access_mask /* [in]  */,
					       struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_winreg_OpenHKDD_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_winreg_OpenHKDD(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				uint16_t *_system_name /* [in] [unique] */,
				uint32_t _access_mask /* [in]  */,
				struct policy_handle *_handle /* [out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_winreg_QueryMultipleValues_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_QueryMultipleValues *r);
NTSTATUS dcerpc_winreg_QueryMultipleValues_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_QueryMultipleValues_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_QueryMultipleValues *r);
struct tevent_req *dcerpc_winreg_QueryMultipleValues_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_key_handle /* [in] [ref] */,
							  struct QueryMultipleValue *_values_in /* [in] [ref,length_is(num_values),size_is(num_values)] */,
							  struct QueryMultipleValue *_values_out /* [out] [size_is(num_values),ref,length_is(num_values)] */,
							  uint32_t _num_values /* [in]  */,
							  uint8_t *_buffer /* [in,out] [length_is(*buffer_size),unique,size_is(*buffer_size)] */,
							  uint32_t *_buffer_size /* [in,out] [ref] */);
NTSTATUS dcerpc_winreg_QueryMultipleValues_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_winreg_QueryMultipleValues(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_key_handle /* [in] [ref] */,
					   struct QueryMultipleValue *_values_in /* [in] [ref,length_is(num_values),size_is(num_values)] */,
					   struct QueryMultipleValue *_values_out /* [out] [size_is(num_values),ref,length_is(num_values)] */,
					   uint32_t _num_values /* [in]  */,
					   uint8_t *_buffer /* [in,out] [length_is(*buffer_size),unique,size_is(*buffer_size)] */,
					   uint32_t *_buffer_size /* [in,out] [ref] */,
					   WERROR *result);

struct tevent_req *dcerpc_winreg_InitiateSystemShutdownEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_InitiateSystemShutdownEx *r);
NTSTATUS dcerpc_winreg_InitiateSystemShutdownEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_InitiateSystemShutdownEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_InitiateSystemShutdownEx *r);
struct tevent_req *dcerpc_winreg_InitiateSystemShutdownEx_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct dcerpc_binding_handle *h,
							       uint16_t *_hostname /* [in] [unique] */,
							       struct lsa_StringLarge *_message /* [in] [unique] */,
							       uint32_t _timeout /* [in]  */,
							       uint8_t _force_apps /* [in]  */,
							       uint8_t _do_reboot /* [in]  */,
							       uint32_t _reason /* [in]  */);
NTSTATUS dcerpc_winreg_InitiateSystemShutdownEx_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     WERROR *result);
NTSTATUS dcerpc_winreg_InitiateSystemShutdownEx(struct dcerpc_binding_handle *h,
						TALLOC_CTX *mem_ctx,
						uint16_t *_hostname /* [in] [unique] */,
						struct lsa_StringLarge *_message /* [in] [unique] */,
						uint32_t _timeout /* [in]  */,
						uint8_t _force_apps /* [in]  */,
						uint8_t _do_reboot /* [in]  */,
						uint32_t _reason /* [in]  */,
						WERROR *result);

struct tevent_req *dcerpc_winreg_SaveKeyEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_SaveKeyEx *r);
NTSTATUS dcerpc_winreg_SaveKeyEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_SaveKeyEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_SaveKeyEx *r);
struct tevent_req *dcerpc_winreg_SaveKeyEx_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_handle /* [in] [ref] */,
						struct winreg_String *_filename /* [in] [ref] */,
						struct KeySecurityAttribute *_sec_attrib /* [in] [unique] */,
						uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_winreg_SaveKeyEx_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_winreg_SaveKeyEx(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_handle /* [in] [ref] */,
				 struct winreg_String *_filename /* [in] [ref] */,
				 struct KeySecurityAttribute *_sec_attrib /* [in] [unique] */,
				 uint32_t _flags /* [in]  */,
				 WERROR *result);

struct tevent_req *dcerpc_winreg_OpenHKPT_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_OpenHKPT *r);
NTSTATUS dcerpc_winreg_OpenHKPT_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_OpenHKPT_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_OpenHKPT *r);
struct tevent_req *dcerpc_winreg_OpenHKPT_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       uint16_t *_system_name /* [in] [unique] */,
					       uint32_t _access_mask /* [in]  */,
					       struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_winreg_OpenHKPT_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_winreg_OpenHKPT(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				uint16_t *_system_name /* [in] [unique] */,
				uint32_t _access_mask /* [in]  */,
				struct policy_handle *_handle /* [out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_winreg_OpenHKPN_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_OpenHKPN *r);
NTSTATUS dcerpc_winreg_OpenHKPN_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_OpenHKPN_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_OpenHKPN *r);
struct tevent_req *dcerpc_winreg_OpenHKPN_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       uint16_t *_system_name /* [in] [unique] */,
					       uint32_t _access_mask /* [in]  */,
					       struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_winreg_OpenHKPN_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_winreg_OpenHKPN(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				uint16_t *_system_name /* [in] [unique] */,
				uint32_t _access_mask /* [in]  */,
				struct policy_handle *_handle /* [out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_winreg_QueryMultipleValues2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_QueryMultipleValues2 *r);
NTSTATUS dcerpc_winreg_QueryMultipleValues2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_QueryMultipleValues2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_QueryMultipleValues2 *r);
struct tevent_req *dcerpc_winreg_QueryMultipleValues2_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   struct policy_handle *_key_handle /* [in] [ref] */,
							   struct QueryMultipleValue *_values_in /* [in] [size_is(num_values),ref,length_is(num_values)] */,
							   struct QueryMultipleValue *_values_out /* [out] [ref,length_is(num_values),size_is(num_values)] */,
							   uint32_t _num_values /* [in]  */,
							   uint8_t *_buffer /* [in,out] [size_is(*offered),unique,length_is(*offered)] */,
							   uint32_t *_offered /* [in] [ref] */,
							   uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_winreg_QueryMultipleValues2_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_winreg_QueryMultipleValues2(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    struct policy_handle *_key_handle /* [in] [ref] */,
					    struct QueryMultipleValue *_values_in /* [in] [size_is(num_values),ref,length_is(num_values)] */,
					    struct QueryMultipleValue *_values_out /* [out] [ref,length_is(num_values),size_is(num_values)] */,
					    uint32_t _num_values /* [in]  */,
					    uint8_t *_buffer /* [in,out] [size_is(*offered),unique,length_is(*offered)] */,
					    uint32_t *_offered /* [in] [ref] */,
					    uint32_t *_needed /* [out] [ref] */,
					    WERROR *result);

struct tevent_req *dcerpc_winreg_DeleteKeyEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct winreg_DeleteKeyEx *r);
NTSTATUS dcerpc_winreg_DeleteKeyEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_winreg_DeleteKeyEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct winreg_DeleteKeyEx *r);
struct tevent_req *dcerpc_winreg_DeleteKeyEx_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct policy_handle *_handle /* [in] [ref] */,
						  struct winreg_String *_key /* [in] [ref] */,
						  uint32_t _access_mask /* [in]  */,
						  uint32_t _reserved /* [in]  */);
NTSTATUS dcerpc_winreg_DeleteKeyEx_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_winreg_DeleteKeyEx(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *_handle /* [in] [ref] */,
				   struct winreg_String *_key /* [in] [ref] */,
				   uint32_t _access_mask /* [in]  */,
				   uint32_t _reserved /* [in]  */,
				   WERROR *result);

#endif /* _HEADER_RPC_winreg */
