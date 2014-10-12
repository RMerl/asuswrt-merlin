#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/epmapper.h"
#ifndef _HEADER_RPC_epmapper
#define _HEADER_RPC_epmapper

extern const struct ndr_interface_table ndr_table_epmapper;

struct tevent_req *dcerpc_epm_Insert_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct epm_Insert *r);
NTSTATUS dcerpc_epm_Insert_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_epm_Insert_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct epm_Insert *r);
struct tevent_req *dcerpc_epm_Insert_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  uint32_t _num_ents /* [in]  */,
					  struct epm_entry_t *_entries /* [in] [size_is(num_ents)] */,
					  uint32_t _replace /* [in]  */);
NTSTATUS dcerpc_epm_Insert_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				uint32_t *result);
NTSTATUS dcerpc_epm_Insert(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   uint32_t _num_ents /* [in]  */,
			   struct epm_entry_t *_entries /* [in] [size_is(num_ents)] */,
			   uint32_t _replace /* [in]  */,
			   uint32_t *result);

struct tevent_req *dcerpc_epm_Delete_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct epm_Delete *r);
NTSTATUS dcerpc_epm_Delete_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_epm_Delete_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct epm_Delete *r);
struct tevent_req *dcerpc_epm_Delete_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  uint32_t _num_ents /* [in]  */,
					  struct epm_entry_t *_entries /* [in] [size_is(num_ents)] */);
NTSTATUS dcerpc_epm_Delete_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				uint32_t *result);
NTSTATUS dcerpc_epm_Delete(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   uint32_t _num_ents /* [in]  */,
			   struct epm_entry_t *_entries /* [in] [size_is(num_ents)] */,
			   uint32_t *result);

struct tevent_req *dcerpc_epm_Lookup_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct epm_Lookup *r);
NTSTATUS dcerpc_epm_Lookup_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_epm_Lookup_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct epm_Lookup *r);
struct tevent_req *dcerpc_epm_Lookup_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  enum epm_InquiryType _inquiry_type /* [in]  */,
					  struct GUID *_object /* [in] [ptr] */,
					  struct rpc_if_id_t *_interface_id /* [in] [ptr] */,
					  enum epm_VersionOption _vers_option /* [in]  */,
					  struct policy_handle *_entry_handle /* [in,out] [ref] */,
					  uint32_t _max_ents /* [in]  */,
					  uint32_t *_num_ents /* [out] [ref] */,
					  struct epm_entry_t *_entries /* [out] [size_is(max_ents),length_is(*num_ents)] */);
NTSTATUS dcerpc_epm_Lookup_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				uint32_t *result);
NTSTATUS dcerpc_epm_Lookup(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   enum epm_InquiryType _inquiry_type /* [in]  */,
			   struct GUID *_object /* [in] [ptr] */,
			   struct rpc_if_id_t *_interface_id /* [in] [ptr] */,
			   enum epm_VersionOption _vers_option /* [in]  */,
			   struct policy_handle *_entry_handle /* [in,out] [ref] */,
			   uint32_t _max_ents /* [in]  */,
			   uint32_t *_num_ents /* [out] [ref] */,
			   struct epm_entry_t *_entries /* [out] [size_is(max_ents),length_is(*num_ents)] */,
			   uint32_t *result);

struct tevent_req *dcerpc_epm_Map_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct epm_Map *r);
NTSTATUS dcerpc_epm_Map_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_epm_Map_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct epm_Map *r);
struct tevent_req *dcerpc_epm_Map_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       struct dcerpc_binding_handle *h,
				       struct GUID *_object /* [in] [ptr] */,
				       struct epm_twr_t *_map_tower /* [in] [ptr] */,
				       struct policy_handle *_entry_handle /* [in,out] [ref] */,
				       uint32_t _max_towers /* [in]  */,
				       uint32_t *_num_towers /* [out] [ref] */,
				       struct epm_twr_p_t *_towers /* [out] [size_is(max_towers),length_is(*num_towers)] */);
NTSTATUS dcerpc_epm_Map_recv(struct tevent_req *req,
			     TALLOC_CTX *mem_ctx,
			     uint32_t *result);
NTSTATUS dcerpc_epm_Map(struct dcerpc_binding_handle *h,
			TALLOC_CTX *mem_ctx,
			struct GUID *_object /* [in] [ptr] */,
			struct epm_twr_t *_map_tower /* [in] [ptr] */,
			struct policy_handle *_entry_handle /* [in,out] [ref] */,
			uint32_t _max_towers /* [in]  */,
			uint32_t *_num_towers /* [out] [ref] */,
			struct epm_twr_p_t *_towers /* [out] [size_is(max_towers),length_is(*num_towers)] */,
			uint32_t *result);

struct tevent_req *dcerpc_epm_LookupHandleFree_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct epm_LookupHandleFree *r);
NTSTATUS dcerpc_epm_LookupHandleFree_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_epm_LookupHandleFree_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct epm_LookupHandleFree *r);
struct tevent_req *dcerpc_epm_LookupHandleFree_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_entry_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_epm_LookupHandleFree_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  uint32_t *result);
NTSTATUS dcerpc_epm_LookupHandleFree(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_entry_handle /* [in,out] [ref] */,
				     uint32_t *result);

struct tevent_req *dcerpc_epm_InqObject_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct epm_InqObject *r);
NTSTATUS dcerpc_epm_InqObject_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_epm_InqObject_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct epm_InqObject *r);
struct tevent_req *dcerpc_epm_InqObject_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     struct GUID *_epm_object /* [in] [ref] */);
NTSTATUS dcerpc_epm_InqObject_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   uint32_t *result);
NTSTATUS dcerpc_epm_InqObject(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      struct GUID *_epm_object /* [in] [ref] */,
			      uint32_t *result);

struct tevent_req *dcerpc_epm_MgmtDelete_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct epm_MgmtDelete *r);
NTSTATUS dcerpc_epm_MgmtDelete_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_epm_MgmtDelete_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct epm_MgmtDelete *r);
struct tevent_req *dcerpc_epm_MgmtDelete_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      uint32_t _object_speced /* [in]  */,
					      struct GUID *_object /* [in] [ptr] */,
					      struct epm_twr_t *_tower /* [in] [ptr] */);
NTSTATUS dcerpc_epm_MgmtDelete_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    uint32_t *result);
NTSTATUS dcerpc_epm_MgmtDelete(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       uint32_t _object_speced /* [in]  */,
			       struct GUID *_object /* [in] [ptr] */,
			       struct epm_twr_t *_tower /* [in] [ptr] */,
			       uint32_t *result);

#endif /* _HEADER_RPC_epmapper */
