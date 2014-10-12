#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/srvsvc.h"
#ifndef _HEADER_RPC_srvsvc
#define _HEADER_RPC_srvsvc

extern const struct ndr_interface_table ndr_table_srvsvc;

struct tevent_req *dcerpc_srvsvc_NetCharDevEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetCharDevEnum *r);
NTSTATUS dcerpc_srvsvc_NetCharDevEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetCharDevEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetCharDevEnum *r);
struct tevent_req *dcerpc_srvsvc_NetCharDevEnum_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     const char *_server_unc /* [in] [charset(UTF16),unique] */,
						     struct srvsvc_NetCharDevInfoCtr *_info_ctr /* [in,out] [ref] */,
						     uint32_t _max_buffer /* [in]  */,
						     uint32_t *_totalentries /* [out] [ref] */,
						     uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetCharDevEnum_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_srvsvc_NetCharDevEnum(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      const char *_server_unc /* [in] [charset(UTF16),unique] */,
				      struct srvsvc_NetCharDevInfoCtr *_info_ctr /* [in,out] [ref] */,
				      uint32_t _max_buffer /* [in]  */,
				      uint32_t *_totalentries /* [out] [ref] */,
				      uint32_t *_resume_handle /* [in,out] [unique] */,
				      WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetCharDevGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetCharDevGetInfo *r);
NTSTATUS dcerpc_srvsvc_NetCharDevGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetCharDevGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetCharDevGetInfo *r);
struct tevent_req *dcerpc_srvsvc_NetCharDevGetInfo_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							const char *_server_unc /* [in] [unique,charset(UTF16)] */,
							const char *_device_name /* [in] [charset(UTF16)] */,
							uint32_t _level /* [in]  */,
							union srvsvc_NetCharDevInfo *_info /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_srvsvc_NetCharDevGetInfo_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_srvsvc_NetCharDevGetInfo(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 const char *_server_unc /* [in] [unique,charset(UTF16)] */,
					 const char *_device_name /* [in] [charset(UTF16)] */,
					 uint32_t _level /* [in]  */,
					 union srvsvc_NetCharDevInfo *_info /* [out] [ref,switch_is(level)] */,
					 WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetCharDevControl_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetCharDevControl *r);
NTSTATUS dcerpc_srvsvc_NetCharDevControl_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetCharDevControl_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetCharDevControl *r);
struct tevent_req *dcerpc_srvsvc_NetCharDevControl_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							const char *_server_unc /* [in] [charset(UTF16),unique] */,
							const char *_device_name /* [in] [charset(UTF16)] */,
							uint32_t _opcode /* [in]  */);
NTSTATUS dcerpc_srvsvc_NetCharDevControl_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_srvsvc_NetCharDevControl(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 const char *_server_unc /* [in] [charset(UTF16),unique] */,
					 const char *_device_name /* [in] [charset(UTF16)] */,
					 uint32_t _opcode /* [in]  */,
					 WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetCharDevQEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetCharDevQEnum *r);
NTSTATUS dcerpc_srvsvc_NetCharDevQEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetCharDevQEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetCharDevQEnum *r);
struct tevent_req *dcerpc_srvsvc_NetCharDevQEnum_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      const char *_server_unc /* [in] [charset(UTF16),unique] */,
						      const char *_user /* [in] [unique,charset(UTF16)] */,
						      struct srvsvc_NetCharDevQInfoCtr *_info_ctr /* [in,out] [ref] */,
						      uint32_t _max_buffer /* [in]  */,
						      uint32_t *_totalentries /* [out] [ref] */,
						      uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetCharDevQEnum_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_srvsvc_NetCharDevQEnum(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       const char *_server_unc /* [in] [charset(UTF16),unique] */,
				       const char *_user /* [in] [unique,charset(UTF16)] */,
				       struct srvsvc_NetCharDevQInfoCtr *_info_ctr /* [in,out] [ref] */,
				       uint32_t _max_buffer /* [in]  */,
				       uint32_t *_totalentries /* [out] [ref] */,
				       uint32_t *_resume_handle /* [in,out] [unique] */,
				       WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetCharDevQGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetCharDevQGetInfo *r);
NTSTATUS dcerpc_srvsvc_NetCharDevQGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetCharDevQGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetCharDevQGetInfo *r);
struct tevent_req *dcerpc_srvsvc_NetCharDevQGetInfo_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 const char *_server_unc /* [in] [unique,charset(UTF16)] */,
							 const char *_queue_name /* [in] [charset(UTF16)] */,
							 const char *_user /* [in] [charset(UTF16)] */,
							 uint32_t _level /* [in]  */,
							 union srvsvc_NetCharDevQInfo *_info /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_srvsvc_NetCharDevQGetInfo_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_srvsvc_NetCharDevQGetInfo(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  const char *_server_unc /* [in] [unique,charset(UTF16)] */,
					  const char *_queue_name /* [in] [charset(UTF16)] */,
					  const char *_user /* [in] [charset(UTF16)] */,
					  uint32_t _level /* [in]  */,
					  union srvsvc_NetCharDevQInfo *_info /* [out] [ref,switch_is(level)] */,
					  WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetCharDevQSetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetCharDevQSetInfo *r);
NTSTATUS dcerpc_srvsvc_NetCharDevQSetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetCharDevQSetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetCharDevQSetInfo *r);
struct tevent_req *dcerpc_srvsvc_NetCharDevQSetInfo_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 const char *_server_unc /* [in] [charset(UTF16),unique] */,
							 const char *_queue_name /* [in] [charset(UTF16)] */,
							 uint32_t _level /* [in]  */,
							 union srvsvc_NetCharDevQInfo _info /* [in] [switch_is(level)] */,
							 uint32_t *_parm_error /* [in,out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetCharDevQSetInfo_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_srvsvc_NetCharDevQSetInfo(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  const char *_server_unc /* [in] [charset(UTF16),unique] */,
					  const char *_queue_name /* [in] [charset(UTF16)] */,
					  uint32_t _level /* [in]  */,
					  union srvsvc_NetCharDevQInfo _info /* [in] [switch_is(level)] */,
					  uint32_t *_parm_error /* [in,out] [unique] */,
					  WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetCharDevQPurge_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetCharDevQPurge *r);
NTSTATUS dcerpc_srvsvc_NetCharDevQPurge_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetCharDevQPurge_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetCharDevQPurge *r);
struct tevent_req *dcerpc_srvsvc_NetCharDevQPurge_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char *_server_unc /* [in] [charset(UTF16),unique] */,
						       const char *_queue_name /* [in] [charset(UTF16)] */);
NTSTATUS dcerpc_srvsvc_NetCharDevQPurge_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS dcerpc_srvsvc_NetCharDevQPurge(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char *_server_unc /* [in] [charset(UTF16),unique] */,
					const char *_queue_name /* [in] [charset(UTF16)] */,
					WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetCharDevQPurgeSelf_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetCharDevQPurgeSelf *r);
NTSTATUS dcerpc_srvsvc_NetCharDevQPurgeSelf_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetCharDevQPurgeSelf_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetCharDevQPurgeSelf *r);
struct tevent_req *dcerpc_srvsvc_NetCharDevQPurgeSelf_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   const char *_server_unc /* [in] [unique,charset(UTF16)] */,
							   const char *_queue_name /* [in] [charset(UTF16)] */,
							   const char *_computer_name /* [in] [charset(UTF16)] */);
NTSTATUS dcerpc_srvsvc_NetCharDevQPurgeSelf_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_srvsvc_NetCharDevQPurgeSelf(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    const char *_server_unc /* [in] [unique,charset(UTF16)] */,
					    const char *_queue_name /* [in] [charset(UTF16)] */,
					    const char *_computer_name /* [in] [charset(UTF16)] */,
					    WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetConnEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetConnEnum *r);
NTSTATUS dcerpc_srvsvc_NetConnEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetConnEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetConnEnum *r);
struct tevent_req *dcerpc_srvsvc_NetConnEnum_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						  const char *_path /* [in] [unique,charset(UTF16)] */,
						  struct srvsvc_NetConnInfoCtr *_info_ctr /* [in,out] [ref] */,
						  uint32_t _max_buffer /* [in]  */,
						  uint32_t *_totalentries /* [out] [ref] */,
						  uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetConnEnum_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_srvsvc_NetConnEnum(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_server_unc /* [in] [unique,charset(UTF16)] */,
				   const char *_path /* [in] [unique,charset(UTF16)] */,
				   struct srvsvc_NetConnInfoCtr *_info_ctr /* [in,out] [ref] */,
				   uint32_t _max_buffer /* [in]  */,
				   uint32_t *_totalentries /* [out] [ref] */,
				   uint32_t *_resume_handle /* [in,out] [unique] */,
				   WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetFileEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetFileEnum *r);
NTSTATUS dcerpc_srvsvc_NetFileEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetFileEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetFileEnum *r);
struct tevent_req *dcerpc_srvsvc_NetFileEnum_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						  const char *_path /* [in] [charset(UTF16),unique] */,
						  const char *_user /* [in] [charset(UTF16),unique] */,
						  struct srvsvc_NetFileInfoCtr *_info_ctr /* [in,out] [ref] */,
						  uint32_t _max_buffer /* [in]  */,
						  uint32_t *_totalentries /* [out] [ref] */,
						  uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetFileEnum_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_srvsvc_NetFileEnum(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_server_unc /* [in] [unique,charset(UTF16)] */,
				   const char *_path /* [in] [charset(UTF16),unique] */,
				   const char *_user /* [in] [charset(UTF16),unique] */,
				   struct srvsvc_NetFileInfoCtr *_info_ctr /* [in,out] [ref] */,
				   uint32_t _max_buffer /* [in]  */,
				   uint32_t *_totalentries /* [out] [ref] */,
				   uint32_t *_resume_handle /* [in,out] [unique] */,
				   WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetFileGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetFileGetInfo *r);
NTSTATUS dcerpc_srvsvc_NetFileGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetFileGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetFileGetInfo *r);
struct tevent_req *dcerpc_srvsvc_NetFileGetInfo_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     const char *_server_unc /* [in] [charset(UTF16),unique] */,
						     uint32_t _fid /* [in]  */,
						     uint32_t _level /* [in]  */,
						     union srvsvc_NetFileInfo *_info /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_srvsvc_NetFileGetInfo_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_srvsvc_NetFileGetInfo(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      const char *_server_unc /* [in] [charset(UTF16),unique] */,
				      uint32_t _fid /* [in]  */,
				      uint32_t _level /* [in]  */,
				      union srvsvc_NetFileInfo *_info /* [out] [ref,switch_is(level)] */,
				      WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetFileClose_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetFileClose *r);
NTSTATUS dcerpc_srvsvc_NetFileClose_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetFileClose_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetFileClose *r);
struct tevent_req *dcerpc_srvsvc_NetFileClose_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   const char *_server_unc /* [in] [charset(UTF16),unique] */,
						   uint32_t _fid /* [in]  */);
NTSTATUS dcerpc_srvsvc_NetFileClose_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_srvsvc_NetFileClose(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    const char *_server_unc /* [in] [charset(UTF16),unique] */,
				    uint32_t _fid /* [in]  */,
				    WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetSessEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetSessEnum *r);
NTSTATUS dcerpc_srvsvc_NetSessEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetSessEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetSessEnum *r);
struct tevent_req *dcerpc_srvsvc_NetSessEnum_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						  const char *_client /* [in] [charset(UTF16),unique] */,
						  const char *_user /* [in] [charset(UTF16),unique] */,
						  struct srvsvc_NetSessInfoCtr *_info_ctr /* [in,out] [ref] */,
						  uint32_t _max_buffer /* [in]  */,
						  uint32_t *_totalentries /* [out] [ref] */,
						  uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetSessEnum_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_srvsvc_NetSessEnum(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_server_unc /* [in] [unique,charset(UTF16)] */,
				   const char *_client /* [in] [charset(UTF16),unique] */,
				   const char *_user /* [in] [charset(UTF16),unique] */,
				   struct srvsvc_NetSessInfoCtr *_info_ctr /* [in,out] [ref] */,
				   uint32_t _max_buffer /* [in]  */,
				   uint32_t *_totalentries /* [out] [ref] */,
				   uint32_t *_resume_handle /* [in,out] [unique] */,
				   WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetSessDel_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetSessDel *r);
NTSTATUS dcerpc_srvsvc_NetSessDel_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetSessDel_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetSessDel *r);
struct tevent_req *dcerpc_srvsvc_NetSessDel_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						 const char *_client /* [in] [unique,charset(UTF16)] */,
						 const char *_user /* [in] [charset(UTF16),unique] */);
NTSTATUS dcerpc_srvsvc_NetSessDel_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_srvsvc_NetSessDel(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char *_server_unc /* [in] [unique,charset(UTF16)] */,
				  const char *_client /* [in] [unique,charset(UTF16)] */,
				  const char *_user /* [in] [charset(UTF16),unique] */,
				  WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetShareAdd_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetShareAdd *r);
NTSTATUS dcerpc_srvsvc_NetShareAdd_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetShareAdd_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetShareAdd *r);
struct tevent_req *dcerpc_srvsvc_NetShareAdd_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_server_unc /* [in] [charset(UTF16),unique] */,
						  uint32_t _level /* [in]  */,
						  union srvsvc_NetShareInfo *_info /* [in] [switch_is(level),ref] */,
						  uint32_t *_parm_error /* [in,out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetShareAdd_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_srvsvc_NetShareAdd(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_server_unc /* [in] [charset(UTF16),unique] */,
				   uint32_t _level /* [in]  */,
				   union srvsvc_NetShareInfo *_info /* [in] [switch_is(level),ref] */,
				   uint32_t *_parm_error /* [in,out] [unique] */,
				   WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetShareEnumAll_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetShareEnumAll *r);
NTSTATUS dcerpc_srvsvc_NetShareEnumAll_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetShareEnumAll_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetShareEnumAll *r);
struct tevent_req *dcerpc_srvsvc_NetShareEnumAll_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						      struct srvsvc_NetShareInfoCtr *_info_ctr /* [in,out] [ref] */,
						      uint32_t _max_buffer /* [in]  */,
						      uint32_t *_totalentries /* [out] [ref] */,
						      uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetShareEnumAll_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_srvsvc_NetShareEnumAll(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       const char *_server_unc /* [in] [unique,charset(UTF16)] */,
				       struct srvsvc_NetShareInfoCtr *_info_ctr /* [in,out] [ref] */,
				       uint32_t _max_buffer /* [in]  */,
				       uint32_t *_totalentries /* [out] [ref] */,
				       uint32_t *_resume_handle /* [in,out] [unique] */,
				       WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetShareGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetShareGetInfo *r);
NTSTATUS dcerpc_srvsvc_NetShareGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetShareGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetShareGetInfo *r);
struct tevent_req *dcerpc_srvsvc_NetShareGetInfo_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						      const char *_share_name /* [in] [charset(UTF16)] */,
						      uint32_t _level /* [in]  */,
						      union srvsvc_NetShareInfo *_info /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_srvsvc_NetShareGetInfo_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_srvsvc_NetShareGetInfo(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       const char *_server_unc /* [in] [unique,charset(UTF16)] */,
				       const char *_share_name /* [in] [charset(UTF16)] */,
				       uint32_t _level /* [in]  */,
				       union srvsvc_NetShareInfo *_info /* [out] [ref,switch_is(level)] */,
				       WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetShareSetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetShareSetInfo *r);
NTSTATUS dcerpc_srvsvc_NetShareSetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetShareSetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetShareSetInfo *r);
struct tevent_req *dcerpc_srvsvc_NetShareSetInfo_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      const char *_server_unc /* [in] [charset(UTF16),unique] */,
						      const char *_share_name /* [in] [charset(UTF16)] */,
						      uint32_t _level /* [in]  */,
						      union srvsvc_NetShareInfo *_info /* [in] [switch_is(level),ref] */,
						      uint32_t *_parm_error /* [in,out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetShareSetInfo_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_srvsvc_NetShareSetInfo(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       const char *_server_unc /* [in] [charset(UTF16),unique] */,
				       const char *_share_name /* [in] [charset(UTF16)] */,
				       uint32_t _level /* [in]  */,
				       union srvsvc_NetShareInfo *_info /* [in] [switch_is(level),ref] */,
				       uint32_t *_parm_error /* [in,out] [unique] */,
				       WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetShareDel_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetShareDel *r);
NTSTATUS dcerpc_srvsvc_NetShareDel_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetShareDel_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetShareDel *r);
struct tevent_req *dcerpc_srvsvc_NetShareDel_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						  const char *_share_name /* [in] [charset(UTF16)] */,
						  uint32_t _reserved /* [in]  */);
NTSTATUS dcerpc_srvsvc_NetShareDel_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_srvsvc_NetShareDel(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_server_unc /* [in] [unique,charset(UTF16)] */,
				   const char *_share_name /* [in] [charset(UTF16)] */,
				   uint32_t _reserved /* [in]  */,
				   WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetShareDelSticky_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetShareDelSticky *r);
NTSTATUS dcerpc_srvsvc_NetShareDelSticky_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetShareDelSticky_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetShareDelSticky *r);
struct tevent_req *dcerpc_srvsvc_NetShareDelSticky_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							const char *_server_unc /* [in] [unique,charset(UTF16)] */,
							const char *_share_name /* [in] [charset(UTF16)] */,
							uint32_t _reserved /* [in]  */);
NTSTATUS dcerpc_srvsvc_NetShareDelSticky_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_srvsvc_NetShareDelSticky(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 const char *_server_unc /* [in] [unique,charset(UTF16)] */,
					 const char *_share_name /* [in] [charset(UTF16)] */,
					 uint32_t _reserved /* [in]  */,
					 WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetShareCheck_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetShareCheck *r);
NTSTATUS dcerpc_srvsvc_NetShareCheck_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetShareCheck_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetShareCheck *r);
struct tevent_req *dcerpc_srvsvc_NetShareCheck_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    const char *_server_unc /* [in] [charset(UTF16),unique] */,
						    const char *_device_name /* [in] [charset(UTF16)] */,
						    enum srvsvc_ShareType *_type /* [out] [ref] */);
NTSTATUS dcerpc_srvsvc_NetShareCheck_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_srvsvc_NetShareCheck(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char *_server_unc /* [in] [charset(UTF16),unique] */,
				     const char *_device_name /* [in] [charset(UTF16)] */,
				     enum srvsvc_ShareType *_type /* [out] [ref] */,
				     WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetSrvGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetSrvGetInfo *r);
NTSTATUS dcerpc_srvsvc_NetSrvGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetSrvGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetSrvGetInfo *r);
struct tevent_req *dcerpc_srvsvc_NetSrvGetInfo_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    const char *_server_unc /* [in] [charset(UTF16),unique] */,
						    uint32_t _level /* [in]  */,
						    union srvsvc_NetSrvInfo *_info /* [out] [switch_is(level),ref] */);
NTSTATUS dcerpc_srvsvc_NetSrvGetInfo_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_srvsvc_NetSrvGetInfo(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char *_server_unc /* [in] [charset(UTF16),unique] */,
				     uint32_t _level /* [in]  */,
				     union srvsvc_NetSrvInfo *_info /* [out] [switch_is(level),ref] */,
				     WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetSrvSetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetSrvSetInfo *r);
NTSTATUS dcerpc_srvsvc_NetSrvSetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetSrvSetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetSrvSetInfo *r);
struct tevent_req *dcerpc_srvsvc_NetSrvSetInfo_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						    uint32_t _level /* [in]  */,
						    union srvsvc_NetSrvInfo *_info /* [in] [ref,switch_is(level)] */,
						    uint32_t *_parm_error /* [in,out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetSrvSetInfo_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_srvsvc_NetSrvSetInfo(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char *_server_unc /* [in] [unique,charset(UTF16)] */,
				     uint32_t _level /* [in]  */,
				     union srvsvc_NetSrvInfo *_info /* [in] [ref,switch_is(level)] */,
				     uint32_t *_parm_error /* [in,out] [unique] */,
				     WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetDiskEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetDiskEnum *r);
NTSTATUS dcerpc_srvsvc_NetDiskEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetDiskEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetDiskEnum *r);
struct tevent_req *dcerpc_srvsvc_NetDiskEnum_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_server_unc /* [in] [charset(UTF16),unique] */,
						  uint32_t _level /* [in]  */,
						  struct srvsvc_NetDiskInfo *_info /* [in,out] [ref] */,
						  uint32_t _maxlen /* [in]  */,
						  uint32_t *_totalentries /* [out] [ref] */,
						  uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetDiskEnum_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_srvsvc_NetDiskEnum(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_server_unc /* [in] [charset(UTF16),unique] */,
				   uint32_t _level /* [in]  */,
				   struct srvsvc_NetDiskInfo *_info /* [in,out] [ref] */,
				   uint32_t _maxlen /* [in]  */,
				   uint32_t *_totalentries /* [out] [ref] */,
				   uint32_t *_resume_handle /* [in,out] [unique] */,
				   WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetServerStatisticsGet_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetServerStatisticsGet *r);
NTSTATUS dcerpc_srvsvc_NetServerStatisticsGet_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetServerStatisticsGet_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetServerStatisticsGet *r);
struct tevent_req *dcerpc_srvsvc_NetServerStatisticsGet_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     const char *_server_unc /* [in] [unique,charset(UTF16)] */,
							     const char *_service /* [in] [charset(UTF16),unique] */,
							     uint32_t _level /* [in]  */,
							     uint32_t _options /* [in]  */,
							     struct srvsvc_Statistics **_stats /* [out] [ref] */);
NTSTATUS dcerpc_srvsvc_NetServerStatisticsGet_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_srvsvc_NetServerStatisticsGet(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      const char *_server_unc /* [in] [unique,charset(UTF16)] */,
					      const char *_service /* [in] [charset(UTF16),unique] */,
					      uint32_t _level /* [in]  */,
					      uint32_t _options /* [in]  */,
					      struct srvsvc_Statistics **_stats /* [out] [ref] */,
					      WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetTransportAdd_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetTransportAdd *r);
NTSTATUS dcerpc_srvsvc_NetTransportAdd_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetTransportAdd_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetTransportAdd *r);
struct tevent_req *dcerpc_srvsvc_NetTransportAdd_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						      uint32_t _level /* [in]  */,
						      union srvsvc_NetTransportInfo _info /* [in] [switch_is(level)] */);
NTSTATUS dcerpc_srvsvc_NetTransportAdd_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_srvsvc_NetTransportAdd(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       const char *_server_unc /* [in] [unique,charset(UTF16)] */,
				       uint32_t _level /* [in]  */,
				       union srvsvc_NetTransportInfo _info /* [in] [switch_is(level)] */,
				       WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetTransportEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetTransportEnum *r);
NTSTATUS dcerpc_srvsvc_NetTransportEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetTransportEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetTransportEnum *r);
struct tevent_req *dcerpc_srvsvc_NetTransportEnum_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char *_server_unc /* [in] [charset(UTF16),unique] */,
						       struct srvsvc_NetTransportInfoCtr *_transports /* [in,out] [ref] */,
						       uint32_t _max_buffer /* [in]  */,
						       uint32_t *_totalentries /* [out] [ref] */,
						       uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetTransportEnum_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS dcerpc_srvsvc_NetTransportEnum(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char *_server_unc /* [in] [charset(UTF16),unique] */,
					struct srvsvc_NetTransportInfoCtr *_transports /* [in,out] [ref] */,
					uint32_t _max_buffer /* [in]  */,
					uint32_t *_totalentries /* [out] [ref] */,
					uint32_t *_resume_handle /* [in,out] [unique] */,
					WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetTransportDel_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetTransportDel *r);
NTSTATUS dcerpc_srvsvc_NetTransportDel_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetTransportDel_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetTransportDel *r);
struct tevent_req *dcerpc_srvsvc_NetTransportDel_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      const char *_server_unc /* [in] [charset(UTF16),unique] */,
						      uint32_t _level /* [in]  */,
						      struct srvsvc_NetTransportInfo0 *_info0 /* [in] [ref] */);
NTSTATUS dcerpc_srvsvc_NetTransportDel_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_srvsvc_NetTransportDel(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       const char *_server_unc /* [in] [charset(UTF16),unique] */,
				       uint32_t _level /* [in]  */,
				       struct srvsvc_NetTransportInfo0 *_info0 /* [in] [ref] */,
				       WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetRemoteTOD_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetRemoteTOD *r);
NTSTATUS dcerpc_srvsvc_NetRemoteTOD_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetRemoteTOD_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetRemoteTOD *r);
struct tevent_req *dcerpc_srvsvc_NetRemoteTOD_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   const char *_server_unc /* [in] [charset(UTF16),unique] */,
						   struct srvsvc_NetRemoteTODInfo **_info /* [out] [ref] */);
NTSTATUS dcerpc_srvsvc_NetRemoteTOD_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_srvsvc_NetRemoteTOD(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    const char *_server_unc /* [in] [charset(UTF16),unique] */,
				    struct srvsvc_NetRemoteTODInfo **_info /* [out] [ref] */,
				    WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetSetServiceBits_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetSetServiceBits *r);
NTSTATUS dcerpc_srvsvc_NetSetServiceBits_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetSetServiceBits_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetSetServiceBits *r);
struct tevent_req *dcerpc_srvsvc_NetSetServiceBits_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							const char *_server_unc /* [in] [charset(UTF16),unique] */,
							const char *_transport /* [in] [unique,charset(UTF16)] */,
							uint32_t _servicebits /* [in]  */,
							uint32_t _updateimmediately /* [in]  */);
NTSTATUS dcerpc_srvsvc_NetSetServiceBits_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_srvsvc_NetSetServiceBits(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 const char *_server_unc /* [in] [charset(UTF16),unique] */,
					 const char *_transport /* [in] [unique,charset(UTF16)] */,
					 uint32_t _servicebits /* [in]  */,
					 uint32_t _updateimmediately /* [in]  */,
					 WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetPathType_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetPathType *r);
NTSTATUS dcerpc_srvsvc_NetPathType_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetPathType_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetPathType *r);
struct tevent_req *dcerpc_srvsvc_NetPathType_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						  const char *_path /* [in] [charset(UTF16)] */,
						  uint32_t _pathflags /* [in]  */,
						  uint32_t *_pathtype /* [out] [ref] */);
NTSTATUS dcerpc_srvsvc_NetPathType_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_srvsvc_NetPathType(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_server_unc /* [in] [unique,charset(UTF16)] */,
				   const char *_path /* [in] [charset(UTF16)] */,
				   uint32_t _pathflags /* [in]  */,
				   uint32_t *_pathtype /* [out] [ref] */,
				   WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetPathCanonicalize_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetPathCanonicalize *r);
NTSTATUS dcerpc_srvsvc_NetPathCanonicalize_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetPathCanonicalize_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetPathCanonicalize *r);
struct tevent_req *dcerpc_srvsvc_NetPathCanonicalize_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  const char *_server_unc /* [in] [unique,charset(UTF16)] */,
							  const char *_path /* [in] [charset(UTF16)] */,
							  uint8_t *_can_path /* [out] [size_is(maxbuf)] */,
							  uint32_t _maxbuf /* [in]  */,
							  const char *_prefix /* [in] [charset(UTF16)] */,
							  uint32_t *_pathtype /* [in,out] [ref] */,
							  uint32_t _pathflags /* [in]  */);
NTSTATUS dcerpc_srvsvc_NetPathCanonicalize_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_srvsvc_NetPathCanonicalize(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   const char *_server_unc /* [in] [unique,charset(UTF16)] */,
					   const char *_path /* [in] [charset(UTF16)] */,
					   uint8_t *_can_path /* [out] [size_is(maxbuf)] */,
					   uint32_t _maxbuf /* [in]  */,
					   const char *_prefix /* [in] [charset(UTF16)] */,
					   uint32_t *_pathtype /* [in,out] [ref] */,
					   uint32_t _pathflags /* [in]  */,
					   WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetPathCompare_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetPathCompare *r);
NTSTATUS dcerpc_srvsvc_NetPathCompare_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetPathCompare_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetPathCompare *r);
struct tevent_req *dcerpc_srvsvc_NetPathCompare_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     const char *_server_unc /* [in] [charset(UTF16),unique] */,
						     const char *_path1 /* [in] [charset(UTF16)] */,
						     const char *_path2 /* [in] [charset(UTF16)] */,
						     uint32_t _pathtype /* [in]  */,
						     uint32_t _pathflags /* [in]  */);
NTSTATUS dcerpc_srvsvc_NetPathCompare_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_srvsvc_NetPathCompare(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      const char *_server_unc /* [in] [charset(UTF16),unique] */,
				      const char *_path1 /* [in] [charset(UTF16)] */,
				      const char *_path2 /* [in] [charset(UTF16)] */,
				      uint32_t _pathtype /* [in]  */,
				      uint32_t _pathflags /* [in]  */,
				      WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetNameValidate_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetNameValidate *r);
NTSTATUS dcerpc_srvsvc_NetNameValidate_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetNameValidate_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetNameValidate *r);
struct tevent_req *dcerpc_srvsvc_NetNameValidate_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						      const char *_name /* [in] [charset(UTF16)] */,
						      uint32_t _name_type /* [in]  */,
						      uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_srvsvc_NetNameValidate_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_srvsvc_NetNameValidate(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       const char *_server_unc /* [in] [unique,charset(UTF16)] */,
				       const char *_name /* [in] [charset(UTF16)] */,
				       uint32_t _name_type /* [in]  */,
				       uint32_t _flags /* [in]  */,
				       WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetPRNameCompare_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetPRNameCompare *r);
NTSTATUS dcerpc_srvsvc_NetPRNameCompare_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetPRNameCompare_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetPRNameCompare *r);
struct tevent_req *dcerpc_srvsvc_NetPRNameCompare_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						       const char *_name1 /* [in] [charset(UTF16)] */,
						       const char *_name2 /* [in] [charset(UTF16)] */,
						       uint32_t _name_type /* [in]  */,
						       uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_srvsvc_NetPRNameCompare_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS dcerpc_srvsvc_NetPRNameCompare(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char *_server_unc /* [in] [unique,charset(UTF16)] */,
					const char *_name1 /* [in] [charset(UTF16)] */,
					const char *_name2 /* [in] [charset(UTF16)] */,
					uint32_t _name_type /* [in]  */,
					uint32_t _flags /* [in]  */,
					WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetShareEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetShareEnum *r);
NTSTATUS dcerpc_srvsvc_NetShareEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetShareEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetShareEnum *r);
struct tevent_req *dcerpc_srvsvc_NetShareEnum_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   const char *_server_unc /* [in] [charset(UTF16),unique] */,
						   struct srvsvc_NetShareInfoCtr *_info_ctr /* [in,out] [ref] */,
						   uint32_t _max_buffer /* [in]  */,
						   uint32_t *_totalentries /* [out] [ref] */,
						   uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetShareEnum_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_srvsvc_NetShareEnum(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    const char *_server_unc /* [in] [charset(UTF16),unique] */,
				    struct srvsvc_NetShareInfoCtr *_info_ctr /* [in,out] [ref] */,
				    uint32_t _max_buffer /* [in]  */,
				    uint32_t *_totalentries /* [out] [ref] */,
				    uint32_t *_resume_handle /* [in,out] [unique] */,
				    WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetShareDelStart_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetShareDelStart *r);
NTSTATUS dcerpc_srvsvc_NetShareDelStart_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetShareDelStart_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetShareDelStart *r);
struct tevent_req *dcerpc_srvsvc_NetShareDelStart_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char *_server_unc /* [in] [unique,charset(UTF16)] */,
						       const char *_share /* [in] [charset(UTF16)] */,
						       uint32_t _reserved /* [in]  */,
						       struct policy_handle *_hnd /* [out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetShareDelStart_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS dcerpc_srvsvc_NetShareDelStart(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char *_server_unc /* [in] [unique,charset(UTF16)] */,
					const char *_share /* [in] [charset(UTF16)] */,
					uint32_t _reserved /* [in]  */,
					struct policy_handle *_hnd /* [out] [unique] */,
					WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetShareDelCommit_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetShareDelCommit *r);
NTSTATUS dcerpc_srvsvc_NetShareDelCommit_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetShareDelCommit_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetShareDelCommit *r);
struct tevent_req *dcerpc_srvsvc_NetShareDelCommit_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_hnd /* [in,out] [unique] */);
NTSTATUS dcerpc_srvsvc_NetShareDelCommit_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_srvsvc_NetShareDelCommit(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_hnd /* [in,out] [unique] */,
					 WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetGetFileSecurity_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetGetFileSecurity *r);
NTSTATUS dcerpc_srvsvc_NetGetFileSecurity_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetGetFileSecurity_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetGetFileSecurity *r);
struct tevent_req *dcerpc_srvsvc_NetGetFileSecurity_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 const char *_server_unc /* [in] [unique,charset(UTF16)] */,
							 const char *_share /* [in] [unique,charset(UTF16)] */,
							 const char *_file /* [in] [charset(UTF16)] */,
							 uint32_t _securityinformation /* [in]  */,
							 struct sec_desc_buf **_sd_buf /* [out] [ref] */);
NTSTATUS dcerpc_srvsvc_NetGetFileSecurity_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_srvsvc_NetGetFileSecurity(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  const char *_server_unc /* [in] [unique,charset(UTF16)] */,
					  const char *_share /* [in] [unique,charset(UTF16)] */,
					  const char *_file /* [in] [charset(UTF16)] */,
					  uint32_t _securityinformation /* [in]  */,
					  struct sec_desc_buf **_sd_buf /* [out] [ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetSetFileSecurity_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetSetFileSecurity *r);
NTSTATUS dcerpc_srvsvc_NetSetFileSecurity_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetSetFileSecurity_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetSetFileSecurity *r);
struct tevent_req *dcerpc_srvsvc_NetSetFileSecurity_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 const char *_server_unc /* [in] [unique,charset(UTF16)] */,
							 const char *_share /* [in] [charset(UTF16),unique] */,
							 const char *_file /* [in] [charset(UTF16)] */,
							 uint32_t _securityinformation /* [in]  */,
							 struct sec_desc_buf *_sd_buf /* [in] [ref] */);
NTSTATUS dcerpc_srvsvc_NetSetFileSecurity_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_srvsvc_NetSetFileSecurity(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  const char *_server_unc /* [in] [unique,charset(UTF16)] */,
					  const char *_share /* [in] [charset(UTF16),unique] */,
					  const char *_file /* [in] [charset(UTF16)] */,
					  uint32_t _securityinformation /* [in]  */,
					  struct sec_desc_buf *_sd_buf /* [in] [ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetServerTransportAddEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetServerTransportAddEx *r);
NTSTATUS dcerpc_srvsvc_NetServerTransportAddEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetServerTransportAddEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetServerTransportAddEx *r);
struct tevent_req *dcerpc_srvsvc_NetServerTransportAddEx_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      const char *_server_unc /* [in] [charset(UTF16),unique] */,
							      uint32_t _level /* [in]  */,
							      union srvsvc_NetTransportInfo _info /* [in] [switch_is(level)] */);
NTSTATUS dcerpc_srvsvc_NetServerTransportAddEx_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    WERROR *result);
NTSTATUS dcerpc_srvsvc_NetServerTransportAddEx(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       const char *_server_unc /* [in] [charset(UTF16),unique] */,
					       uint32_t _level /* [in]  */,
					       union srvsvc_NetTransportInfo _info /* [in] [switch_is(level)] */,
					       WERROR *result);

struct tevent_req *dcerpc_srvsvc_NetServerSetServiceBitsEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct srvsvc_NetServerSetServiceBitsEx *r);
NTSTATUS dcerpc_srvsvc_NetServerSetServiceBitsEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_srvsvc_NetServerSetServiceBitsEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct srvsvc_NetServerSetServiceBitsEx *r);
struct tevent_req *dcerpc_srvsvc_NetServerSetServiceBitsEx_send(TALLOC_CTX *mem_ctx,
								struct tevent_context *ev,
								struct dcerpc_binding_handle *h,
								const char *_server_unc /* [in] [charset(UTF16),unique] */,
								const char *_emulated_server_unc /* [in] [charset(UTF16),unique] */,
								const char *_transport /* [in] [charset(UTF16),unique] */,
								uint32_t _servicebitsofinterest /* [in]  */,
								uint32_t _servicebits /* [in]  */,
								uint32_t _updateimmediately /* [in]  */);
NTSTATUS dcerpc_srvsvc_NetServerSetServiceBitsEx_recv(struct tevent_req *req,
						      TALLOC_CTX *mem_ctx,
						      WERROR *result);
NTSTATUS dcerpc_srvsvc_NetServerSetServiceBitsEx(struct dcerpc_binding_handle *h,
						 TALLOC_CTX *mem_ctx,
						 const char *_server_unc /* [in] [charset(UTF16),unique] */,
						 const char *_emulated_server_unc /* [in] [charset(UTF16),unique] */,
						 const char *_transport /* [in] [charset(UTF16),unique] */,
						 uint32_t _servicebitsofinterest /* [in]  */,
						 uint32_t _servicebits /* [in]  */,
						 uint32_t _updateimmediately /* [in]  */,
						 WERROR *result);

#endif /* _HEADER_RPC_srvsvc */
