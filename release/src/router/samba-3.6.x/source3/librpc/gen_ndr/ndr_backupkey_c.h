#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/backupkey.h"
#ifndef _HEADER_RPC_backupkey
#define _HEADER_RPC_backupkey

extern const struct ndr_interface_table ndr_table_backupkey;

struct tevent_req *dcerpc_bkrp_BackupKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct bkrp_BackupKey *r);
NTSTATUS dcerpc_bkrp_BackupKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_bkrp_BackupKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct bkrp_BackupKey *r);
struct tevent_req *dcerpc_bkrp_BackupKey_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct GUID *_guidActionAgent /* [in] [ref] */,
					      uint8_t *_data_in /* [in] [size_is(data_in_len),ref] */,
					      uint32_t _data_in_len /* [in]  */,
					      uint8_t **_data_out /* [out] [ref,size_is(,*data_out_len)] */,
					      uint32_t *_data_out_len /* [out] [ref] */,
					      uint32_t _param /* [in]  */);
NTSTATUS dcerpc_bkrp_BackupKey_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_bkrp_BackupKey(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct GUID *_guidActionAgent /* [in] [ref] */,
			       uint8_t *_data_in /* [in] [size_is(data_in_len),ref] */,
			       uint32_t _data_in_len /* [in]  */,
			       uint8_t **_data_out /* [out] [ref,size_is(,*data_out_len)] */,
			       uint32_t *_data_out_len /* [out] [ref] */,
			       uint32_t _param /* [in]  */,
			       WERROR *result);

#endif /* _HEADER_RPC_backupkey */
