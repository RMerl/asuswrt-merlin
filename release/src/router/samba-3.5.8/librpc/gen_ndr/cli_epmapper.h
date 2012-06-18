#include "../librpc/gen_ndr/ndr_epmapper.h"
#ifndef __CLI_EPMAPPER__
#define __CLI_EPMAPPER__
struct tevent_req *rpccli_epm_Insert_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli,
					  uint32_t _num_ents /* [in]  */,
					  struct epm_entry_t *_entries /* [in] [size_is(num_ents)] */,
					  uint32_t _replace /* [in]  */);
NTSTATUS rpccli_epm_Insert_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				uint32 *result);
NTSTATUS rpccli_epm_Insert(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   uint32_t num_ents /* [in]  */,
			   struct epm_entry_t *entries /* [in] [size_is(num_ents)] */,
			   uint32_t replace /* [in]  */);
struct tevent_req *rpccli_epm_Delete_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli,
					  uint32_t _num_ents /* [in]  */,
					  struct epm_entry_t *_entries /* [in] [size_is(num_ents)] */);
NTSTATUS rpccli_epm_Delete_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				uint32 *result);
NTSTATUS rpccli_epm_Delete(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   uint32_t num_ents /* [in]  */,
			   struct epm_entry_t *entries /* [in] [size_is(num_ents)] */);
struct tevent_req *rpccli_epm_Lookup_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli,
					  uint32_t _inquiry_type /* [in]  */,
					  struct GUID *_object /* [in] [ptr] */,
					  struct rpc_if_id_t *_interface_id /* [in] [ptr] */,
					  uint32_t _vers_option /* [in]  */,
					  struct policy_handle *_entry_handle /* [in,out] [ref] */,
					  uint32_t _max_ents /* [in]  */,
					  uint32_t *_num_ents /* [out] [ref] */,
					  struct epm_entry_t *_entries /* [out] [length_is(*num_ents),size_is(max_ents)] */);
NTSTATUS rpccli_epm_Lookup_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				uint32 *result);
NTSTATUS rpccli_epm_Lookup(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   uint32_t inquiry_type /* [in]  */,
			   struct GUID *object /* [in] [ptr] */,
			   struct rpc_if_id_t *interface_id /* [in] [ptr] */,
			   uint32_t vers_option /* [in]  */,
			   struct policy_handle *entry_handle /* [in,out] [ref] */,
			   uint32_t max_ents /* [in]  */,
			   uint32_t *num_ents /* [out] [ref] */,
			   struct epm_entry_t *entries /* [out] [length_is(*num_ents),size_is(max_ents)] */);
struct tevent_req *rpccli_epm_Map_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       struct rpc_pipe_client *cli,
				       struct GUID *_object /* [in] [ptr] */,
				       struct epm_twr_t *_map_tower /* [in] [ptr] */,
				       struct policy_handle *_entry_handle /* [in,out] [ref] */,
				       uint32_t _max_towers /* [in]  */,
				       uint32_t *_num_towers /* [out] [ref] */,
				       struct epm_twr_p_t *_towers /* [out] [length_is(*num_towers),size_is(max_towers)] */);
NTSTATUS rpccli_epm_Map_recv(struct tevent_req *req,
			     TALLOC_CTX *mem_ctx,
			     uint32 *result);
NTSTATUS rpccli_epm_Map(struct rpc_pipe_client *cli,
			TALLOC_CTX *mem_ctx,
			struct GUID *object /* [in] [ptr] */,
			struct epm_twr_t *map_tower /* [in] [ptr] */,
			struct policy_handle *entry_handle /* [in,out] [ref] */,
			uint32_t max_towers /* [in]  */,
			uint32_t *num_towers /* [out] [ref] */,
			struct epm_twr_p_t *towers /* [out] [length_is(*num_towers),size_is(max_towers)] */);
struct tevent_req *rpccli_epm_LookupHandleFree_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct policy_handle *_entry_handle /* [in,out] [ref] */);
NTSTATUS rpccli_epm_LookupHandleFree_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  uint32 *result);
NTSTATUS rpccli_epm_LookupHandleFree(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *entry_handle /* [in,out] [ref] */);
struct tevent_req *rpccli_epm_InqObject_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli,
					     struct GUID *_epm_object /* [in] [ref] */);
NTSTATUS rpccli_epm_InqObject_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   uint32 *result);
NTSTATUS rpccli_epm_InqObject(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      struct GUID *epm_object /* [in] [ref] */);
struct tevent_req *rpccli_epm_MgmtDelete_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli,
					      uint32_t _object_speced /* [in]  */,
					      struct GUID *_object /* [in] [ptr] */,
					      struct epm_twr_t *_tower /* [in] [ptr] */);
NTSTATUS rpccli_epm_MgmtDelete_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    uint32 *result);
NTSTATUS rpccli_epm_MgmtDelete(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       uint32_t object_speced /* [in]  */,
			       struct GUID *object /* [in] [ptr] */,
			       struct epm_twr_t *tower /* [in] [ptr] */);
struct tevent_req *rpccli_epm_MapAuth_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct rpc_pipe_client *cli);
NTSTATUS rpccli_epm_MapAuth_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 uint32 *result);
NTSTATUS rpccli_epm_MapAuth(struct rpc_pipe_client *cli,
			    TALLOC_CTX *mem_ctx);
#endif /* __CLI_EPMAPPER__ */
