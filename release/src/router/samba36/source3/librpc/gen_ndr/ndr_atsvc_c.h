#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/atsvc.h"
#ifndef _HEADER_RPC_atsvc
#define _HEADER_RPC_atsvc

extern const struct ndr_interface_table ndr_table_atsvc;

struct tevent_req *dcerpc_atsvc_JobAdd_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct atsvc_JobAdd *r);
NTSTATUS dcerpc_atsvc_JobAdd_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_atsvc_JobAdd_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct atsvc_JobAdd *r);
struct tevent_req *dcerpc_atsvc_JobAdd_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct dcerpc_binding_handle *h,
					    const char *_servername /* [in] [charset(UTF16),unique] */,
					    struct atsvc_JobInfo *_job_info /* [in] [ref] */,
					    uint32_t *_job_id /* [out] [ref] */);
NTSTATUS dcerpc_atsvc_JobAdd_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  NTSTATUS *result);
NTSTATUS dcerpc_atsvc_JobAdd(struct dcerpc_binding_handle *h,
			     TALLOC_CTX *mem_ctx,
			     const char *_servername /* [in] [charset(UTF16),unique] */,
			     struct atsvc_JobInfo *_job_info /* [in] [ref] */,
			     uint32_t *_job_id /* [out] [ref] */,
			     NTSTATUS *result);

struct tevent_req *dcerpc_atsvc_JobDel_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct atsvc_JobDel *r);
NTSTATUS dcerpc_atsvc_JobDel_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_atsvc_JobDel_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct atsvc_JobDel *r);
struct tevent_req *dcerpc_atsvc_JobDel_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct dcerpc_binding_handle *h,
					    const char *_servername /* [in] [charset(UTF16),unique] */,
					    uint32_t _min_job_id /* [in]  */,
					    uint32_t _max_job_id /* [in]  */);
NTSTATUS dcerpc_atsvc_JobDel_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  NTSTATUS *result);
NTSTATUS dcerpc_atsvc_JobDel(struct dcerpc_binding_handle *h,
			     TALLOC_CTX *mem_ctx,
			     const char *_servername /* [in] [charset(UTF16),unique] */,
			     uint32_t _min_job_id /* [in]  */,
			     uint32_t _max_job_id /* [in]  */,
			     NTSTATUS *result);

struct tevent_req *dcerpc_atsvc_JobEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct atsvc_JobEnum *r);
NTSTATUS dcerpc_atsvc_JobEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_atsvc_JobEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct atsvc_JobEnum *r);
struct tevent_req *dcerpc_atsvc_JobEnum_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     const char *_servername /* [in] [charset(UTF16),unique] */,
					     struct atsvc_enum_ctr *_ctr /* [in,out] [ref] */,
					     uint32_t _preferred_max_len /* [in]  */,
					     uint32_t *_total_entries /* [out] [ref] */,
					     uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_atsvc_JobEnum_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   NTSTATUS *result);
NTSTATUS dcerpc_atsvc_JobEnum(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      const char *_servername /* [in] [charset(UTF16),unique] */,
			      struct atsvc_enum_ctr *_ctr /* [in,out] [ref] */,
			      uint32_t _preferred_max_len /* [in]  */,
			      uint32_t *_total_entries /* [out] [ref] */,
			      uint32_t *_resume_handle /* [in,out] [unique] */,
			      NTSTATUS *result);

struct tevent_req *dcerpc_atsvc_JobGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct atsvc_JobGetInfo *r);
NTSTATUS dcerpc_atsvc_JobGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_atsvc_JobGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct atsvc_JobGetInfo *r);
struct tevent_req *dcerpc_atsvc_JobGetInfo_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						const char *_servername /* [in] [charset(UTF16),unique] */,
						uint32_t _job_id /* [in]  */,
						struct atsvc_JobInfo **_job_info /* [out] [ref] */);
NTSTATUS dcerpc_atsvc_JobGetInfo_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      NTSTATUS *result);
NTSTATUS dcerpc_atsvc_JobGetInfo(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 const char *_servername /* [in] [charset(UTF16),unique] */,
				 uint32_t _job_id /* [in]  */,
				 struct atsvc_JobInfo **_job_info /* [out] [ref] */,
				 NTSTATUS *result);

#endif /* _HEADER_RPC_atsvc */
