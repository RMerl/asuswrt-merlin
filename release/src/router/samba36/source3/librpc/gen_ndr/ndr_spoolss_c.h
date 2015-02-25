#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/spoolss.h"
#ifndef _HEADER_RPC_spoolss
#define _HEADER_RPC_spoolss

extern const struct ndr_interface_table ndr_table_spoolss;

struct tevent_req *dcerpc_spoolss_EnumPrinters_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_EnumPrinters *r);
NTSTATUS dcerpc_spoolss_EnumPrinters_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_EnumPrinters_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_EnumPrinters *r);
struct tevent_req *dcerpc_spoolss_EnumPrinters_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    uint32_t _flags /* [in]  */,
						    const char *_server /* [in] [unique,charset(UTF16)] */,
						    uint32_t _level /* [in]  */,
						    DATA_BLOB *_buffer /* [in] [unique] */,
						    uint32_t _offered /* [in]  */,
						    uint32_t *_count /* [out] [ref] */,
						    union spoolss_PrinterInfo **_info /* [out] [switch_is(level),size_is(,*count),ref] */,
						    uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_EnumPrinters_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_spoolss_EnumPrinters(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     uint32_t _flags /* [in]  */,
				     const char *_server /* [in] [unique,charset(UTF16)] */,
				     uint32_t _level /* [in]  */,
				     DATA_BLOB *_buffer /* [in] [unique] */,
				     uint32_t _offered /* [in]  */,
				     uint32_t *_count /* [out] [ref] */,
				     union spoolss_PrinterInfo **_info /* [out] [switch_is(level),size_is(,*count),ref] */,
				     uint32_t *_needed /* [out] [ref] */,
				     WERROR *result);

struct tevent_req *dcerpc_spoolss_OpenPrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_OpenPrinter *r);
NTSTATUS dcerpc_spoolss_OpenPrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_OpenPrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_OpenPrinter *r);
struct tevent_req *dcerpc_spoolss_OpenPrinter_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   const char *_printername /* [in] [charset(UTF16),unique] */,
						   const char *_datatype /* [in] [unique,charset(UTF16)] */,
						   struct spoolss_DevmodeContainer _devmode_ctr /* [in]  */,
						   uint32_t _access_mask /* [in]  */,
						   struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_OpenPrinter_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_spoolss_OpenPrinter(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    const char *_printername /* [in] [charset(UTF16),unique] */,
				    const char *_datatype /* [in] [unique,charset(UTF16)] */,
				    struct spoolss_DevmodeContainer _devmode_ctr /* [in]  */,
				    uint32_t _access_mask /* [in]  */,
				    struct policy_handle *_handle /* [out] [ref] */,
				    WERROR *result);

struct tevent_req *dcerpc_spoolss_SetJob_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_SetJob *r);
NTSTATUS dcerpc_spoolss_SetJob_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_SetJob_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_SetJob *r);
struct tevent_req *dcerpc_spoolss_SetJob_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct policy_handle *_handle /* [in] [ref] */,
					      uint32_t _job_id /* [in]  */,
					      struct spoolss_JobInfoContainer *_ctr /* [in] [unique] */,
					      enum spoolss_JobControl _command /* [in]  */);
NTSTATUS dcerpc_spoolss_SetJob_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_spoolss_SetJob(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *_handle /* [in] [ref] */,
			       uint32_t _job_id /* [in]  */,
			       struct spoolss_JobInfoContainer *_ctr /* [in] [unique] */,
			       enum spoolss_JobControl _command /* [in]  */,
			       WERROR *result);

struct tevent_req *dcerpc_spoolss_GetJob_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_GetJob *r);
NTSTATUS dcerpc_spoolss_GetJob_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_GetJob_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_GetJob *r);
struct tevent_req *dcerpc_spoolss_GetJob_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct policy_handle *_handle /* [in] [ref] */,
					      uint32_t _job_id /* [in]  */,
					      uint32_t _level /* [in]  */,
					      DATA_BLOB *_buffer /* [in] [unique] */,
					      uint32_t _offered /* [in]  */,
					      union spoolss_JobInfo *_info /* [out] [subcontext_size(offered),switch_is(level),subcontext(4),unique] */,
					      uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_GetJob_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_spoolss_GetJob(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *_handle /* [in] [ref] */,
			       uint32_t _job_id /* [in]  */,
			       uint32_t _level /* [in]  */,
			       DATA_BLOB *_buffer /* [in] [unique] */,
			       uint32_t _offered /* [in]  */,
			       union spoolss_JobInfo *_info /* [out] [subcontext_size(offered),switch_is(level),subcontext(4),unique] */,
			       uint32_t *_needed /* [out] [ref] */,
			       WERROR *result);

struct tevent_req *dcerpc_spoolss_EnumJobs_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_EnumJobs *r);
NTSTATUS dcerpc_spoolss_EnumJobs_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_EnumJobs_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_EnumJobs *r);
struct tevent_req *dcerpc_spoolss_EnumJobs_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct policy_handle *_handle /* [in] [ref] */,
						uint32_t _firstjob /* [in]  */,
						uint32_t _numjobs /* [in]  */,
						uint32_t _level /* [in]  */,
						DATA_BLOB *_buffer /* [in] [unique] */,
						uint32_t _offered /* [in]  */,
						uint32_t *_count /* [out] [ref] */,
						union spoolss_JobInfo **_info /* [out] [switch_is(level),size_is(,*count),ref] */,
						uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_EnumJobs_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_spoolss_EnumJobs(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *_handle /* [in] [ref] */,
				 uint32_t _firstjob /* [in]  */,
				 uint32_t _numjobs /* [in]  */,
				 uint32_t _level /* [in]  */,
				 DATA_BLOB *_buffer /* [in] [unique] */,
				 uint32_t _offered /* [in]  */,
				 uint32_t *_count /* [out] [ref] */,
				 union spoolss_JobInfo **_info /* [out] [switch_is(level),size_is(,*count),ref] */,
				 uint32_t *_needed /* [out] [ref] */,
				 WERROR *result);

struct tevent_req *dcerpc_spoolss_AddPrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_AddPrinter *r);
NTSTATUS dcerpc_spoolss_AddPrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_AddPrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_AddPrinter *r);
struct tevent_req *dcerpc_spoolss_AddPrinter_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_server /* [in] [unique,charset(UTF16)] */,
						  struct spoolss_SetPrinterInfoCtr *_info_ctr /* [in] [ref] */,
						  struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */,
						  struct sec_desc_buf *_secdesc_ctr /* [in] [ref] */,
						  struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_AddPrinter_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_spoolss_AddPrinter(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_server /* [in] [unique,charset(UTF16)] */,
				   struct spoolss_SetPrinterInfoCtr *_info_ctr /* [in] [ref] */,
				   struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */,
				   struct sec_desc_buf *_secdesc_ctr /* [in] [ref] */,
				   struct policy_handle *_handle /* [out] [ref] */,
				   WERROR *result);

struct tevent_req *dcerpc_spoolss_DeletePrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_DeletePrinter *r);
NTSTATUS dcerpc_spoolss_DeletePrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_DeletePrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_DeletePrinter *r);
struct tevent_req *dcerpc_spoolss_DeletePrinter_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS dcerpc_spoolss_DeletePrinter_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_spoolss_DeletePrinter(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_handle /* [in] [ref] */,
				      WERROR *result);

struct tevent_req *dcerpc_spoolss_SetPrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_SetPrinter *r);
NTSTATUS dcerpc_spoolss_SetPrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_SetPrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_SetPrinter *r);
struct tevent_req *dcerpc_spoolss_SetPrinter_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct policy_handle *_handle /* [in] [ref] */,
						  struct spoolss_SetPrinterInfoCtr *_info_ctr /* [in] [ref] */,
						  struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */,
						  struct sec_desc_buf *_secdesc_ctr /* [in] [ref] */,
						  enum spoolss_PrinterControl _command /* [in]  */);
NTSTATUS dcerpc_spoolss_SetPrinter_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_spoolss_SetPrinter(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *_handle /* [in] [ref] */,
				   struct spoolss_SetPrinterInfoCtr *_info_ctr /* [in] [ref] */,
				   struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */,
				   struct sec_desc_buf *_secdesc_ctr /* [in] [ref] */,
				   enum spoolss_PrinterControl _command /* [in]  */,
				   WERROR *result);

struct tevent_req *dcerpc_spoolss_GetPrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_GetPrinter *r);
NTSTATUS dcerpc_spoolss_GetPrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_GetPrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_GetPrinter *r);
struct tevent_req *dcerpc_spoolss_GetPrinter_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct policy_handle *_handle /* [in] [ref] */,
						  uint32_t _level /* [in]  */,
						  DATA_BLOB *_buffer /* [in] [unique] */,
						  uint32_t _offered /* [in]  */,
						  union spoolss_PrinterInfo *_info /* [out] [unique,subcontext_size(offered),switch_is(level),subcontext(4)] */,
						  uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_GetPrinter_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_spoolss_GetPrinter(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *_handle /* [in] [ref] */,
				   uint32_t _level /* [in]  */,
				   DATA_BLOB *_buffer /* [in] [unique] */,
				   uint32_t _offered /* [in]  */,
				   union spoolss_PrinterInfo *_info /* [out] [unique,subcontext_size(offered),switch_is(level),subcontext(4)] */,
				   uint32_t *_needed /* [out] [ref] */,
				   WERROR *result);

struct tevent_req *dcerpc_spoolss_AddPrinterDriver_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_AddPrinterDriver *r);
NTSTATUS dcerpc_spoolss_AddPrinterDriver_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_AddPrinterDriver_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_AddPrinterDriver *r);
struct tevent_req *dcerpc_spoolss_AddPrinterDriver_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							const char *_servername /* [in] [unique,charset(UTF16)] */,
							struct spoolss_AddDriverInfoCtr *_info_ctr /* [in] [ref] */);
NTSTATUS dcerpc_spoolss_AddPrinterDriver_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_spoolss_AddPrinterDriver(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 const char *_servername /* [in] [unique,charset(UTF16)] */,
					 struct spoolss_AddDriverInfoCtr *_info_ctr /* [in] [ref] */,
					 WERROR *result);

struct tevent_req *dcerpc_spoolss_EnumPrinterDrivers_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_EnumPrinterDrivers *r);
NTSTATUS dcerpc_spoolss_EnumPrinterDrivers_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_EnumPrinterDrivers_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_EnumPrinterDrivers *r);
struct tevent_req *dcerpc_spoolss_EnumPrinterDrivers_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  const char *_server /* [in] [charset(UTF16),unique] */,
							  const char *_environment /* [in] [unique,charset(UTF16)] */,
							  uint32_t _level /* [in]  */,
							  DATA_BLOB *_buffer /* [in] [unique] */,
							  uint32_t _offered /* [in]  */,
							  uint32_t *_count /* [out] [ref] */,
							  union spoolss_DriverInfo **_info /* [out] [switch_is(level),ref,size_is(,*count)] */,
							  uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_EnumPrinterDrivers_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_spoolss_EnumPrinterDrivers(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   const char *_server /* [in] [charset(UTF16),unique] */,
					   const char *_environment /* [in] [unique,charset(UTF16)] */,
					   uint32_t _level /* [in]  */,
					   DATA_BLOB *_buffer /* [in] [unique] */,
					   uint32_t _offered /* [in]  */,
					   uint32_t *_count /* [out] [ref] */,
					   union spoolss_DriverInfo **_info /* [out] [switch_is(level),ref,size_is(,*count)] */,
					   uint32_t *_needed /* [out] [ref] */,
					   WERROR *result);

struct tevent_req *dcerpc_spoolss_GetPrinterDriver_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_GetPrinterDriver *r);
NTSTATUS dcerpc_spoolss_GetPrinterDriver_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_GetPrinterDriver_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_GetPrinterDriver *r);
struct tevent_req *dcerpc_spoolss_GetPrinterDriver_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_handle /* [in] [ref] */,
							const char *_architecture /* [in] [unique,charset(UTF16)] */,
							uint32_t _level /* [in]  */,
							DATA_BLOB *_buffer /* [in] [unique] */,
							uint32_t _offered /* [in]  */,
							union spoolss_DriverInfo *_info /* [out] [subcontext_size(offered),switch_is(level),subcontext(4),unique] */,
							uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_GetPrinterDriver_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_spoolss_GetPrinterDriver(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_handle /* [in] [ref] */,
					 const char *_architecture /* [in] [unique,charset(UTF16)] */,
					 uint32_t _level /* [in]  */,
					 DATA_BLOB *_buffer /* [in] [unique] */,
					 uint32_t _offered /* [in]  */,
					 union spoolss_DriverInfo *_info /* [out] [subcontext_size(offered),switch_is(level),subcontext(4),unique] */,
					 uint32_t *_needed /* [out] [ref] */,
					 WERROR *result);

struct tevent_req *dcerpc_spoolss_GetPrinterDriverDirectory_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_GetPrinterDriverDirectory *r);
NTSTATUS dcerpc_spoolss_GetPrinterDriverDirectory_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_GetPrinterDriverDirectory_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_GetPrinterDriverDirectory *r);
struct tevent_req *dcerpc_spoolss_GetPrinterDriverDirectory_send(TALLOC_CTX *mem_ctx,
								 struct tevent_context *ev,
								 struct dcerpc_binding_handle *h,
								 const char *_server /* [in] [charset(UTF16),unique] */,
								 const char *_environment /* [in] [charset(UTF16),unique] */,
								 uint32_t _level /* [in]  */,
								 DATA_BLOB *_buffer /* [in] [unique] */,
								 uint32_t _offered /* [in]  */,
								 union spoolss_DriverDirectoryInfo *_info /* [out] [unique,subcontext(4),switch_is(level),subcontext_size(offered)] */,
								 uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_GetPrinterDriverDirectory_recv(struct tevent_req *req,
						       TALLOC_CTX *mem_ctx,
						       WERROR *result);
NTSTATUS dcerpc_spoolss_GetPrinterDriverDirectory(struct dcerpc_binding_handle *h,
						  TALLOC_CTX *mem_ctx,
						  const char *_server /* [in] [charset(UTF16),unique] */,
						  const char *_environment /* [in] [charset(UTF16),unique] */,
						  uint32_t _level /* [in]  */,
						  DATA_BLOB *_buffer /* [in] [unique] */,
						  uint32_t _offered /* [in]  */,
						  union spoolss_DriverDirectoryInfo *_info /* [out] [unique,subcontext(4),switch_is(level),subcontext_size(offered)] */,
						  uint32_t *_needed /* [out] [ref] */,
						  WERROR *result);

struct tevent_req *dcerpc_spoolss_DeletePrinterDriver_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_DeletePrinterDriver *r);
NTSTATUS dcerpc_spoolss_DeletePrinterDriver_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_DeletePrinterDriver_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_DeletePrinterDriver *r);
struct tevent_req *dcerpc_spoolss_DeletePrinterDriver_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   const char *_server /* [in] [charset(UTF16),unique] */,
							   const char *_architecture /* [in] [charset(UTF16)] */,
							   const char *_driver /* [in] [charset(UTF16)] */);
NTSTATUS dcerpc_spoolss_DeletePrinterDriver_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_spoolss_DeletePrinterDriver(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    const char *_server /* [in] [charset(UTF16),unique] */,
					    const char *_architecture /* [in] [charset(UTF16)] */,
					    const char *_driver /* [in] [charset(UTF16)] */,
					    WERROR *result);

struct tevent_req *dcerpc_spoolss_AddPrintProcessor_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_AddPrintProcessor *r);
NTSTATUS dcerpc_spoolss_AddPrintProcessor_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_AddPrintProcessor_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_AddPrintProcessor *r);
struct tevent_req *dcerpc_spoolss_AddPrintProcessor_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 const char *_server /* [in] [charset(UTF16),unique] */,
							 const char *_architecture /* [in] [charset(UTF16)] */,
							 const char *_path_name /* [in] [charset(UTF16)] */,
							 const char *_print_processor_name /* [in] [charset(UTF16)] */);
NTSTATUS dcerpc_spoolss_AddPrintProcessor_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_spoolss_AddPrintProcessor(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  const char *_server /* [in] [charset(UTF16),unique] */,
					  const char *_architecture /* [in] [charset(UTF16)] */,
					  const char *_path_name /* [in] [charset(UTF16)] */,
					  const char *_print_processor_name /* [in] [charset(UTF16)] */,
					  WERROR *result);

struct tevent_req *dcerpc_spoolss_EnumPrintProcessors_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_EnumPrintProcessors *r);
NTSTATUS dcerpc_spoolss_EnumPrintProcessors_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_EnumPrintProcessors_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_EnumPrintProcessors *r);
struct tevent_req *dcerpc_spoolss_EnumPrintProcessors_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   const char *_servername /* [in] [unique,charset(UTF16)] */,
							   const char *_environment /* [in] [unique,charset(UTF16)] */,
							   uint32_t _level /* [in]  */,
							   DATA_BLOB *_buffer /* [in] [unique] */,
							   uint32_t _offered /* [in]  */,
							   uint32_t *_count /* [out] [ref] */,
							   union spoolss_PrintProcessorInfo **_info /* [out] [switch_is(level),size_is(,*count),ref] */,
							   uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_EnumPrintProcessors_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_spoolss_EnumPrintProcessors(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    const char *_servername /* [in] [unique,charset(UTF16)] */,
					    const char *_environment /* [in] [unique,charset(UTF16)] */,
					    uint32_t _level /* [in]  */,
					    DATA_BLOB *_buffer /* [in] [unique] */,
					    uint32_t _offered /* [in]  */,
					    uint32_t *_count /* [out] [ref] */,
					    union spoolss_PrintProcessorInfo **_info /* [out] [switch_is(level),size_is(,*count),ref] */,
					    uint32_t *_needed /* [out] [ref] */,
					    WERROR *result);

struct tevent_req *dcerpc_spoolss_GetPrintProcessorDirectory_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_GetPrintProcessorDirectory *r);
NTSTATUS dcerpc_spoolss_GetPrintProcessorDirectory_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_GetPrintProcessorDirectory_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_GetPrintProcessorDirectory *r);
struct tevent_req *dcerpc_spoolss_GetPrintProcessorDirectory_send(TALLOC_CTX *mem_ctx,
								  struct tevent_context *ev,
								  struct dcerpc_binding_handle *h,
								  const char *_server /* [in] [unique,charset(UTF16)] */,
								  const char *_environment /* [in] [unique,charset(UTF16)] */,
								  uint32_t _level /* [in]  */,
								  DATA_BLOB *_buffer /* [in] [unique] */,
								  uint32_t _offered /* [in]  */,
								  union spoolss_PrintProcessorDirectoryInfo *_info /* [out] [subcontext(4),switch_is(level),subcontext_size(offered),unique] */,
								  uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_GetPrintProcessorDirectory_recv(struct tevent_req *req,
							TALLOC_CTX *mem_ctx,
							WERROR *result);
NTSTATUS dcerpc_spoolss_GetPrintProcessorDirectory(struct dcerpc_binding_handle *h,
						   TALLOC_CTX *mem_ctx,
						   const char *_server /* [in] [unique,charset(UTF16)] */,
						   const char *_environment /* [in] [unique,charset(UTF16)] */,
						   uint32_t _level /* [in]  */,
						   DATA_BLOB *_buffer /* [in] [unique] */,
						   uint32_t _offered /* [in]  */,
						   union spoolss_PrintProcessorDirectoryInfo *_info /* [out] [subcontext(4),switch_is(level),subcontext_size(offered),unique] */,
						   uint32_t *_needed /* [out] [ref] */,
						   WERROR *result);

struct tevent_req *dcerpc_spoolss_StartDocPrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_StartDocPrinter *r);
NTSTATUS dcerpc_spoolss_StartDocPrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_StartDocPrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_StartDocPrinter *r);
struct tevent_req *dcerpc_spoolss_StartDocPrinter_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct policy_handle *_handle /* [in] [ref] */,
						       uint32_t _level /* [in]  */,
						       union spoolss_DocumentInfo _info /* [in] [switch_is(level)] */,
						       uint32_t *_job_id /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_StartDocPrinter_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS dcerpc_spoolss_StartDocPrinter(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *_handle /* [in] [ref] */,
					uint32_t _level /* [in]  */,
					union spoolss_DocumentInfo _info /* [in] [switch_is(level)] */,
					uint32_t *_job_id /* [out] [ref] */,
					WERROR *result);

struct tevent_req *dcerpc_spoolss_StartPagePrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_StartPagePrinter *r);
NTSTATUS dcerpc_spoolss_StartPagePrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_StartPagePrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_StartPagePrinter *r);
struct tevent_req *dcerpc_spoolss_StartPagePrinter_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS dcerpc_spoolss_StartPagePrinter_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_spoolss_StartPagePrinter(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_handle /* [in] [ref] */,
					 WERROR *result);

struct tevent_req *dcerpc_spoolss_WritePrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_WritePrinter *r);
NTSTATUS dcerpc_spoolss_WritePrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_WritePrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_WritePrinter *r);
struct tevent_req *dcerpc_spoolss_WritePrinter_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_handle /* [in] [ref] */,
						    DATA_BLOB _data /* [in]  */,
						    uint32_t __data_size /* [in] [value(r->in.data.length)] */,
						    uint32_t *_num_written /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_WritePrinter_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_spoolss_WritePrinter(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_handle /* [in] [ref] */,
				     DATA_BLOB _data /* [in]  */,
				     uint32_t __data_size /* [in] [value(r->in.data.length)] */,
				     uint32_t *_num_written /* [out] [ref] */,
				     WERROR *result);

struct tevent_req *dcerpc_spoolss_EndPagePrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_EndPagePrinter *r);
NTSTATUS dcerpc_spoolss_EndPagePrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_EndPagePrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_EndPagePrinter *r);
struct tevent_req *dcerpc_spoolss_EndPagePrinter_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS dcerpc_spoolss_EndPagePrinter_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_spoolss_EndPagePrinter(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_handle /* [in] [ref] */,
				       WERROR *result);

struct tevent_req *dcerpc_spoolss_AbortPrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_AbortPrinter *r);
NTSTATUS dcerpc_spoolss_AbortPrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_AbortPrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_AbortPrinter *r);
struct tevent_req *dcerpc_spoolss_AbortPrinter_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS dcerpc_spoolss_AbortPrinter_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_spoolss_AbortPrinter(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_handle /* [in] [ref] */,
				     WERROR *result);

struct tevent_req *dcerpc_spoolss_ReadPrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_ReadPrinter *r);
NTSTATUS dcerpc_spoolss_ReadPrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_ReadPrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_ReadPrinter *r);
struct tevent_req *dcerpc_spoolss_ReadPrinter_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct policy_handle *_handle /* [in] [ref] */,
						   uint8_t *_data /* [out] [ref,size_is(data_size)] */,
						   uint32_t _data_size /* [in]  */,
						   uint32_t *__data_size /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_ReadPrinter_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_spoolss_ReadPrinter(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *_handle /* [in] [ref] */,
				    uint8_t *_data /* [out] [ref,size_is(data_size)] */,
				    uint32_t _data_size /* [in]  */,
				    uint32_t *__data_size /* [out] [ref] */,
				    WERROR *result);

struct tevent_req *dcerpc_spoolss_EndDocPrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_EndDocPrinter *r);
NTSTATUS dcerpc_spoolss_EndDocPrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_EndDocPrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_EndDocPrinter *r);
struct tevent_req *dcerpc_spoolss_EndDocPrinter_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS dcerpc_spoolss_EndDocPrinter_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_spoolss_EndDocPrinter(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_handle /* [in] [ref] */,
				      WERROR *result);

struct tevent_req *dcerpc_spoolss_AddJob_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_AddJob *r);
NTSTATUS dcerpc_spoolss_AddJob_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_AddJob_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_AddJob *r);
struct tevent_req *dcerpc_spoolss_AddJob_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct policy_handle *_handle /* [in] [ref] */,
					      uint32_t _level /* [in]  */,
					      uint8_t *_buffer /* [in,out] [size_is(offered),unique] */,
					      uint32_t _offered /* [in]  */,
					      uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_AddJob_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_spoolss_AddJob(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *_handle /* [in] [ref] */,
			       uint32_t _level /* [in]  */,
			       uint8_t *_buffer /* [in,out] [size_is(offered),unique] */,
			       uint32_t _offered /* [in]  */,
			       uint32_t *_needed /* [out] [ref] */,
			       WERROR *result);

struct tevent_req *dcerpc_spoolss_ScheduleJob_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_ScheduleJob *r);
NTSTATUS dcerpc_spoolss_ScheduleJob_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_ScheduleJob_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_ScheduleJob *r);
struct tevent_req *dcerpc_spoolss_ScheduleJob_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct policy_handle *_handle /* [in] [ref] */,
						   uint32_t _jobid /* [in]  */);
NTSTATUS dcerpc_spoolss_ScheduleJob_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_spoolss_ScheduleJob(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *_handle /* [in] [ref] */,
				    uint32_t _jobid /* [in]  */,
				    WERROR *result);

struct tevent_req *dcerpc_spoolss_GetPrinterData_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_GetPrinterData *r);
NTSTATUS dcerpc_spoolss_GetPrinterData_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_GetPrinterData_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_GetPrinterData *r);
struct tevent_req *dcerpc_spoolss_GetPrinterData_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_handle /* [in] [ref] */,
						      const char *_value_name /* [in] [charset(UTF16)] */,
						      enum winreg_Type *_type /* [out] [ref] */,
						      uint8_t *_data /* [out] [size_is(offered),ref] */,
						      uint32_t _offered /* [in]  */,
						      uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_GetPrinterData_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_spoolss_GetPrinterData(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_handle /* [in] [ref] */,
				       const char *_value_name /* [in] [charset(UTF16)] */,
				       enum winreg_Type *_type /* [out] [ref] */,
				       uint8_t *_data /* [out] [size_is(offered),ref] */,
				       uint32_t _offered /* [in]  */,
				       uint32_t *_needed /* [out] [ref] */,
				       WERROR *result);

struct tevent_req *dcerpc_spoolss_SetPrinterData_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_SetPrinterData *r);
NTSTATUS dcerpc_spoolss_SetPrinterData_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_SetPrinterData_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_SetPrinterData *r);
struct tevent_req *dcerpc_spoolss_SetPrinterData_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_handle /* [in] [ref] */,
						      const char *_value_name /* [in] [charset(UTF16)] */,
						      enum winreg_Type _type /* [in]  */,
						      uint8_t *_data /* [in] [size_is(offered),ref] */,
						      uint32_t _offered /* [in]  */);
NTSTATUS dcerpc_spoolss_SetPrinterData_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_spoolss_SetPrinterData(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_handle /* [in] [ref] */,
				       const char *_value_name /* [in] [charset(UTF16)] */,
				       enum winreg_Type _type /* [in]  */,
				       uint8_t *_data /* [in] [size_is(offered),ref] */,
				       uint32_t _offered /* [in]  */,
				       WERROR *result);

struct tevent_req *dcerpc_spoolss_ClosePrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_ClosePrinter *r);
NTSTATUS dcerpc_spoolss_ClosePrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_ClosePrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_ClosePrinter *r);
struct tevent_req *dcerpc_spoolss_ClosePrinter_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_spoolss_ClosePrinter_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_spoolss_ClosePrinter(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_handle /* [in,out] [ref] */,
				     WERROR *result);

struct tevent_req *dcerpc_spoolss_AddForm_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_AddForm *r);
NTSTATUS dcerpc_spoolss_AddForm_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_AddForm_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_AddForm *r);
struct tevent_req *dcerpc_spoolss_AddForm_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_handle /* [in] [ref] */,
					       uint32_t _level /* [in]  */,
					       union spoolss_AddFormInfo _info /* [in] [switch_is(level)] */);
NTSTATUS dcerpc_spoolss_AddForm_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_spoolss_AddForm(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_handle /* [in] [ref] */,
				uint32_t _level /* [in]  */,
				union spoolss_AddFormInfo _info /* [in] [switch_is(level)] */,
				WERROR *result);

struct tevent_req *dcerpc_spoolss_DeleteForm_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_DeleteForm *r);
NTSTATUS dcerpc_spoolss_DeleteForm_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_DeleteForm_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_DeleteForm *r);
struct tevent_req *dcerpc_spoolss_DeleteForm_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct policy_handle *_handle /* [in] [ref] */,
						  const char *_form_name /* [in] [charset(UTF16)] */);
NTSTATUS dcerpc_spoolss_DeleteForm_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_spoolss_DeleteForm(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *_handle /* [in] [ref] */,
				   const char *_form_name /* [in] [charset(UTF16)] */,
				   WERROR *result);

struct tevent_req *dcerpc_spoolss_GetForm_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_GetForm *r);
NTSTATUS dcerpc_spoolss_GetForm_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_GetForm_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_GetForm *r);
struct tevent_req *dcerpc_spoolss_GetForm_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_handle /* [in] [ref] */,
					       const char *_form_name /* [in] [charset(UTF16)] */,
					       uint32_t _level /* [in]  */,
					       DATA_BLOB *_buffer /* [in] [unique] */,
					       uint32_t _offered /* [in]  */,
					       union spoolss_FormInfo *_info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
					       uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_GetForm_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_spoolss_GetForm(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_handle /* [in] [ref] */,
				const char *_form_name /* [in] [charset(UTF16)] */,
				uint32_t _level /* [in]  */,
				DATA_BLOB *_buffer /* [in] [unique] */,
				uint32_t _offered /* [in]  */,
				union spoolss_FormInfo *_info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
				uint32_t *_needed /* [out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_spoolss_SetForm_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_SetForm *r);
NTSTATUS dcerpc_spoolss_SetForm_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_SetForm_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_SetForm *r);
struct tevent_req *dcerpc_spoolss_SetForm_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_handle /* [in] [ref] */,
					       const char *_form_name /* [in] [charset(UTF16)] */,
					       uint32_t _level /* [in]  */,
					       union spoolss_AddFormInfo _info /* [in] [switch_is(level)] */);
NTSTATUS dcerpc_spoolss_SetForm_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_spoolss_SetForm(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_handle /* [in] [ref] */,
				const char *_form_name /* [in] [charset(UTF16)] */,
				uint32_t _level /* [in]  */,
				union spoolss_AddFormInfo _info /* [in] [switch_is(level)] */,
				WERROR *result);

struct tevent_req *dcerpc_spoolss_EnumForms_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_EnumForms *r);
NTSTATUS dcerpc_spoolss_EnumForms_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_EnumForms_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_EnumForms *r);
struct tevent_req *dcerpc_spoolss_EnumForms_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct policy_handle *_handle /* [in] [ref] */,
						 uint32_t _level /* [in]  */,
						 DATA_BLOB *_buffer /* [in] [unique] */,
						 uint32_t _offered /* [in]  */,
						 uint32_t *_count /* [out] [ref] */,
						 union spoolss_FormInfo **_info /* [out] [ref,size_is(,*count),switch_is(level)] */,
						 uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_EnumForms_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_spoolss_EnumForms(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *_handle /* [in] [ref] */,
				  uint32_t _level /* [in]  */,
				  DATA_BLOB *_buffer /* [in] [unique] */,
				  uint32_t _offered /* [in]  */,
				  uint32_t *_count /* [out] [ref] */,
				  union spoolss_FormInfo **_info /* [out] [ref,size_is(,*count),switch_is(level)] */,
				  uint32_t *_needed /* [out] [ref] */,
				  WERROR *result);

struct tevent_req *dcerpc_spoolss_EnumPorts_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_EnumPorts *r);
NTSTATUS dcerpc_spoolss_EnumPorts_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_EnumPorts_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_EnumPorts *r);
struct tevent_req *dcerpc_spoolss_EnumPorts_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char *_servername /* [in] [unique,charset(UTF16)] */,
						 uint32_t _level /* [in]  */,
						 DATA_BLOB *_buffer /* [in] [unique] */,
						 uint32_t _offered /* [in]  */,
						 uint32_t *_count /* [out] [ref] */,
						 union spoolss_PortInfo **_info /* [out] [size_is(,*count),ref,switch_is(level)] */,
						 uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_EnumPorts_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_spoolss_EnumPorts(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char *_servername /* [in] [unique,charset(UTF16)] */,
				  uint32_t _level /* [in]  */,
				  DATA_BLOB *_buffer /* [in] [unique] */,
				  uint32_t _offered /* [in]  */,
				  uint32_t *_count /* [out] [ref] */,
				  union spoolss_PortInfo **_info /* [out] [size_is(,*count),ref,switch_is(level)] */,
				  uint32_t *_needed /* [out] [ref] */,
				  WERROR *result);

struct tevent_req *dcerpc_spoolss_EnumMonitors_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_EnumMonitors *r);
NTSTATUS dcerpc_spoolss_EnumMonitors_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_EnumMonitors_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_EnumMonitors *r);
struct tevent_req *dcerpc_spoolss_EnumMonitors_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    const char *_servername /* [in] [unique,charset(UTF16)] */,
						    uint32_t _level /* [in]  */,
						    DATA_BLOB *_buffer /* [in] [unique] */,
						    uint32_t _offered /* [in]  */,
						    uint32_t *_count /* [out] [ref] */,
						    union spoolss_MonitorInfo **_info /* [out] [size_is(,*count),ref,switch_is(level)] */,
						    uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_EnumMonitors_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_spoolss_EnumMonitors(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char *_servername /* [in] [unique,charset(UTF16)] */,
				     uint32_t _level /* [in]  */,
				     DATA_BLOB *_buffer /* [in] [unique] */,
				     uint32_t _offered /* [in]  */,
				     uint32_t *_count /* [out] [ref] */,
				     union spoolss_MonitorInfo **_info /* [out] [size_is(,*count),ref,switch_is(level)] */,
				     uint32_t *_needed /* [out] [ref] */,
				     WERROR *result);

struct tevent_req *dcerpc_spoolss_AddPort_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_AddPort *r);
NTSTATUS dcerpc_spoolss_AddPort_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_AddPort_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_AddPort *r);
struct tevent_req *dcerpc_spoolss_AddPort_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       const char *_server_name /* [in] [unique,charset(UTF16)] */,
					       uint32_t _unknown /* [in]  */,
					       const char *_monitor_name /* [in] [charset(UTF16)] */);
NTSTATUS dcerpc_spoolss_AddPort_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_spoolss_AddPort(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				const char *_server_name /* [in] [unique,charset(UTF16)] */,
				uint32_t _unknown /* [in]  */,
				const char *_monitor_name /* [in] [charset(UTF16)] */,
				WERROR *result);

struct tevent_req *dcerpc_spoolss_DeletePort_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_DeletePort *r);
NTSTATUS dcerpc_spoolss_DeletePort_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_DeletePort_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_DeletePort *r);
struct tevent_req *dcerpc_spoolss_DeletePort_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_server_name /* [in] [unique,charset(UTF16)] */,
						  uint32_t _ptr /* [in]  */,
						  const char *_port_name /* [in] [charset(UTF16),ref] */);
NTSTATUS dcerpc_spoolss_DeletePort_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_spoolss_DeletePort(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_server_name /* [in] [unique,charset(UTF16)] */,
				   uint32_t _ptr /* [in]  */,
				   const char *_port_name /* [in] [charset(UTF16),ref] */,
				   WERROR *result);

struct tevent_req *dcerpc_spoolss_CreatePrinterIC_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_CreatePrinterIC *r);
NTSTATUS dcerpc_spoolss_CreatePrinterIC_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_CreatePrinterIC_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_CreatePrinterIC *r);
struct tevent_req *dcerpc_spoolss_CreatePrinterIC_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct policy_handle *_handle /* [in] [ref] */,
						       struct policy_handle *_gdi_handle /* [out] [ref] */,
						       struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */);
NTSTATUS dcerpc_spoolss_CreatePrinterIC_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS dcerpc_spoolss_CreatePrinterIC(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *_handle /* [in] [ref] */,
					struct policy_handle *_gdi_handle /* [out] [ref] */,
					struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */,
					WERROR *result);

struct tevent_req *dcerpc_spoolss_DeletePrinterIC_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_DeletePrinterIC *r);
NTSTATUS dcerpc_spoolss_DeletePrinterIC_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_DeletePrinterIC_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_DeletePrinterIC *r);
struct tevent_req *dcerpc_spoolss_DeletePrinterIC_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct policy_handle *_gdi_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_spoolss_DeletePrinterIC_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS dcerpc_spoolss_DeletePrinterIC(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *_gdi_handle /* [in,out] [ref] */,
					WERROR *result);

struct tevent_req *dcerpc_spoolss_EnumPrintProcDataTypes_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_EnumPrintProcDataTypes *r);
NTSTATUS dcerpc_spoolss_EnumPrintProcDataTypes_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_EnumPrintProcDataTypes_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_EnumPrintProcDataTypes *r);
struct tevent_req *dcerpc_spoolss_EnumPrintProcDataTypes_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      const char *_servername /* [in] [unique,charset(UTF16)] */,
							      const char *_print_processor_name /* [in] [charset(UTF16),unique] */,
							      uint32_t _level /* [in]  */,
							      DATA_BLOB *_buffer /* [in] [unique] */,
							      uint32_t _offered /* [in]  */,
							      uint32_t *_count /* [out] [ref] */,
							      union spoolss_PrintProcDataTypesInfo **_info /* [out] [switch_is(level),ref,size_is(,*count)] */,
							      uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_EnumPrintProcDataTypes_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    WERROR *result);
NTSTATUS dcerpc_spoolss_EnumPrintProcDataTypes(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       const char *_servername /* [in] [unique,charset(UTF16)] */,
					       const char *_print_processor_name /* [in] [charset(UTF16),unique] */,
					       uint32_t _level /* [in]  */,
					       DATA_BLOB *_buffer /* [in] [unique] */,
					       uint32_t _offered /* [in]  */,
					       uint32_t *_count /* [out] [ref] */,
					       union spoolss_PrintProcDataTypesInfo **_info /* [out] [switch_is(level),ref,size_is(,*count)] */,
					       uint32_t *_needed /* [out] [ref] */,
					       WERROR *result);

struct tevent_req *dcerpc_spoolss_ResetPrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_ResetPrinter *r);
NTSTATUS dcerpc_spoolss_ResetPrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_ResetPrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_ResetPrinter *r);
struct tevent_req *dcerpc_spoolss_ResetPrinter_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct policy_handle *_handle /* [in] [ref] */,
						    const char *_data_type /* [in] [charset(UTF16),unique] */,
						    struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */);
NTSTATUS dcerpc_spoolss_ResetPrinter_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_spoolss_ResetPrinter(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *_handle /* [in] [ref] */,
				     const char *_data_type /* [in] [charset(UTF16),unique] */,
				     struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */,
				     WERROR *result);

struct tevent_req *dcerpc_spoolss_GetPrinterDriver2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_GetPrinterDriver2 *r);
NTSTATUS dcerpc_spoolss_GetPrinterDriver2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_GetPrinterDriver2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_GetPrinterDriver2 *r);
struct tevent_req *dcerpc_spoolss_GetPrinterDriver2_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_handle /* [in] [ref] */,
							 const char *_architecture /* [in] [charset(UTF16),unique] */,
							 uint32_t _level /* [in]  */,
							 DATA_BLOB *_buffer /* [in] [unique] */,
							 uint32_t _offered /* [in]  */,
							 uint32_t _client_major_version /* [in]  */,
							 uint32_t _client_minor_version /* [in]  */,
							 union spoolss_DriverInfo *_info /* [out] [switch_is(level),subcontext(4),subcontext_size(offered),unique] */,
							 uint32_t *_needed /* [out] [ref] */,
							 uint32_t *_server_major_version /* [out] [ref] */,
							 uint32_t *_server_minor_version /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_GetPrinterDriver2_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_spoolss_GetPrinterDriver2(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_handle /* [in] [ref] */,
					  const char *_architecture /* [in] [charset(UTF16),unique] */,
					  uint32_t _level /* [in]  */,
					  DATA_BLOB *_buffer /* [in] [unique] */,
					  uint32_t _offered /* [in]  */,
					  uint32_t _client_major_version /* [in]  */,
					  uint32_t _client_minor_version /* [in]  */,
					  union spoolss_DriverInfo *_info /* [out] [switch_is(level),subcontext(4),subcontext_size(offered),unique] */,
					  uint32_t *_needed /* [out] [ref] */,
					  uint32_t *_server_major_version /* [out] [ref] */,
					  uint32_t *_server_minor_version /* [out] [ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_spoolss_FindClosePrinterNotify_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_FindClosePrinterNotify *r);
NTSTATUS dcerpc_spoolss_FindClosePrinterNotify_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_FindClosePrinterNotify_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_FindClosePrinterNotify *r);
struct tevent_req *dcerpc_spoolss_FindClosePrinterNotify_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS dcerpc_spoolss_FindClosePrinterNotify_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    WERROR *result);
NTSTATUS dcerpc_spoolss_FindClosePrinterNotify(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       struct policy_handle *_handle /* [in] [ref] */,
					       WERROR *result);

struct tevent_req *dcerpc_spoolss_ReplyOpenPrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_ReplyOpenPrinter *r);
NTSTATUS dcerpc_spoolss_ReplyOpenPrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_ReplyOpenPrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_ReplyOpenPrinter *r);
struct tevent_req *dcerpc_spoolss_ReplyOpenPrinter_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							const char *_server_name /* [in] [charset(UTF16)] */,
							uint32_t _printer_local /* [in]  */,
							enum winreg_Type _type /* [in]  */,
							uint32_t _bufsize /* [in] [range(0,512)] */,
							uint8_t *_buffer /* [in] [unique,size_is(bufsize)] */,
							struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_ReplyOpenPrinter_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_spoolss_ReplyOpenPrinter(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 const char *_server_name /* [in] [charset(UTF16)] */,
					 uint32_t _printer_local /* [in]  */,
					 enum winreg_Type _type /* [in]  */,
					 uint32_t _bufsize /* [in] [range(0,512)] */,
					 uint8_t *_buffer /* [in] [unique,size_is(bufsize)] */,
					 struct policy_handle *_handle /* [out] [ref] */,
					 WERROR *result);

struct tevent_req *dcerpc_spoolss_RouterReplyPrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_RouterReplyPrinter *r);
NTSTATUS dcerpc_spoolss_RouterReplyPrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_RouterReplyPrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_RouterReplyPrinter *r);
struct tevent_req *dcerpc_spoolss_RouterReplyPrinter_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_handle /* [in] [ref] */,
							  uint32_t _flags /* [in]  */,
							  uint32_t _bufsize /* [in] [range(0,512)] */,
							  uint8_t *_buffer /* [in] [unique,size_is(bufsize)] */);
NTSTATUS dcerpc_spoolss_RouterReplyPrinter_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_spoolss_RouterReplyPrinter(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_handle /* [in] [ref] */,
					   uint32_t _flags /* [in]  */,
					   uint32_t _bufsize /* [in] [range(0,512)] */,
					   uint8_t *_buffer /* [in] [unique,size_is(bufsize)] */,
					   WERROR *result);

struct tevent_req *dcerpc_spoolss_ReplyClosePrinter_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_ReplyClosePrinter *r);
NTSTATUS dcerpc_spoolss_ReplyClosePrinter_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_ReplyClosePrinter_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_ReplyClosePrinter *r);
struct tevent_req *dcerpc_spoolss_ReplyClosePrinter_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_spoolss_ReplyClosePrinter_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_spoolss_ReplyClosePrinter(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_handle /* [in,out] [ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_spoolss_AddPortEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_AddPortEx *r);
NTSTATUS dcerpc_spoolss_AddPortEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_AddPortEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_AddPortEx *r);
struct tevent_req *dcerpc_spoolss_AddPortEx_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char *_servername /* [in] [charset(UTF16),unique] */,
						 struct spoolss_SetPortInfoContainer *_port_ctr /* [in] [ref] */,
						 struct spoolss_PortVarContainer *_port_var_ctr /* [in] [ref] */,
						 const char *_monitor_name /* [in] [charset(UTF16),unique] */);
NTSTATUS dcerpc_spoolss_AddPortEx_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_spoolss_AddPortEx(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char *_servername /* [in] [charset(UTF16),unique] */,
				  struct spoolss_SetPortInfoContainer *_port_ctr /* [in] [ref] */,
				  struct spoolss_PortVarContainer *_port_var_ctr /* [in] [ref] */,
				  const char *_monitor_name /* [in] [charset(UTF16),unique] */,
				  WERROR *result);

struct tevent_req *dcerpc_spoolss_RemoteFindFirstPrinterChangeNotifyEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_RemoteFindFirstPrinterChangeNotifyEx *r);
NTSTATUS dcerpc_spoolss_RemoteFindFirstPrinterChangeNotifyEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_RemoteFindFirstPrinterChangeNotifyEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_RemoteFindFirstPrinterChangeNotifyEx *r);
struct tevent_req *dcerpc_spoolss_RemoteFindFirstPrinterChangeNotifyEx_send(TALLOC_CTX *mem_ctx,
									    struct tevent_context *ev,
									    struct dcerpc_binding_handle *h,
									    struct policy_handle *_handle /* [in] [ref] */,
									    uint32_t _flags /* [in]  */,
									    uint32_t _options /* [in]  */,
									    const char *_local_machine /* [in] [charset(UTF16),unique] */,
									    uint32_t _printer_local /* [in]  */,
									    struct spoolss_NotifyOption *_notify_options /* [in] [unique] */);
NTSTATUS dcerpc_spoolss_RemoteFindFirstPrinterChangeNotifyEx_recv(struct tevent_req *req,
								  TALLOC_CTX *mem_ctx,
								  WERROR *result);
NTSTATUS dcerpc_spoolss_RemoteFindFirstPrinterChangeNotifyEx(struct dcerpc_binding_handle *h,
							     TALLOC_CTX *mem_ctx,
							     struct policy_handle *_handle /* [in] [ref] */,
							     uint32_t _flags /* [in]  */,
							     uint32_t _options /* [in]  */,
							     const char *_local_machine /* [in] [charset(UTF16),unique] */,
							     uint32_t _printer_local /* [in]  */,
							     struct spoolss_NotifyOption *_notify_options /* [in] [unique] */,
							     WERROR *result);

struct tevent_req *dcerpc_spoolss_RouterReplyPrinterEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_RouterReplyPrinterEx *r);
NTSTATUS dcerpc_spoolss_RouterReplyPrinterEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_RouterReplyPrinterEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_RouterReplyPrinterEx *r);
struct tevent_req *dcerpc_spoolss_RouterReplyPrinterEx_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct dcerpc_binding_handle *h,
							    struct policy_handle *_handle /* [in] [ref] */,
							    uint32_t _color /* [in]  */,
							    uint32_t _flags /* [in]  */,
							    uint32_t *_reply_result /* [out] [ref] */,
							    uint32_t _reply_type /* [in]  */,
							    union spoolss_ReplyPrinterInfo _info /* [in] [switch_is(reply_type)] */);
NTSTATUS dcerpc_spoolss_RouterReplyPrinterEx_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS dcerpc_spoolss_RouterReplyPrinterEx(struct dcerpc_binding_handle *h,
					     TALLOC_CTX *mem_ctx,
					     struct policy_handle *_handle /* [in] [ref] */,
					     uint32_t _color /* [in]  */,
					     uint32_t _flags /* [in]  */,
					     uint32_t *_reply_result /* [out] [ref] */,
					     uint32_t _reply_type /* [in]  */,
					     union spoolss_ReplyPrinterInfo _info /* [in] [switch_is(reply_type)] */,
					     WERROR *result);

struct tevent_req *dcerpc_spoolss_RouterRefreshPrinterChangeNotify_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_RouterRefreshPrinterChangeNotify *r);
NTSTATUS dcerpc_spoolss_RouterRefreshPrinterChangeNotify_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_RouterRefreshPrinterChangeNotify_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_RouterRefreshPrinterChangeNotify *r);
struct tevent_req *dcerpc_spoolss_RouterRefreshPrinterChangeNotify_send(TALLOC_CTX *mem_ctx,
									struct tevent_context *ev,
									struct dcerpc_binding_handle *h,
									struct policy_handle *_handle /* [in] [ref] */,
									uint32_t _change_low /* [in]  */,
									struct spoolss_NotifyOption *_options /* [in] [unique] */,
									struct spoolss_NotifyInfo **_info /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_RouterRefreshPrinterChangeNotify_recv(struct tevent_req *req,
							      TALLOC_CTX *mem_ctx,
							      WERROR *result);
NTSTATUS dcerpc_spoolss_RouterRefreshPrinterChangeNotify(struct dcerpc_binding_handle *h,
							 TALLOC_CTX *mem_ctx,
							 struct policy_handle *_handle /* [in] [ref] */,
							 uint32_t _change_low /* [in]  */,
							 struct spoolss_NotifyOption *_options /* [in] [unique] */,
							 struct spoolss_NotifyInfo **_info /* [out] [ref] */,
							 WERROR *result);

struct tevent_req *dcerpc_spoolss_OpenPrinterEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_OpenPrinterEx *r);
NTSTATUS dcerpc_spoolss_OpenPrinterEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_OpenPrinterEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_OpenPrinterEx *r);
struct tevent_req *dcerpc_spoolss_OpenPrinterEx_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     const char *_printername /* [in] [charset(UTF16),unique] */,
						     const char *_datatype /* [in] [charset(UTF16),unique] */,
						     struct spoolss_DevmodeContainer _devmode_ctr /* [in]  */,
						     uint32_t _access_mask /* [in]  */,
						     uint32_t _level /* [in]  */,
						     union spoolss_UserLevel _userlevel /* [in] [switch_is(level)] */,
						     struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_OpenPrinterEx_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_spoolss_OpenPrinterEx(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      const char *_printername /* [in] [charset(UTF16),unique] */,
				      const char *_datatype /* [in] [charset(UTF16),unique] */,
				      struct spoolss_DevmodeContainer _devmode_ctr /* [in]  */,
				      uint32_t _access_mask /* [in]  */,
				      uint32_t _level /* [in]  */,
				      union spoolss_UserLevel _userlevel /* [in] [switch_is(level)] */,
				      struct policy_handle *_handle /* [out] [ref] */,
				      WERROR *result);

struct tevent_req *dcerpc_spoolss_AddPrinterEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_AddPrinterEx *r);
NTSTATUS dcerpc_spoolss_AddPrinterEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_AddPrinterEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_AddPrinterEx *r);
struct tevent_req *dcerpc_spoolss_AddPrinterEx_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    const char *_server /* [in] [charset(UTF16),unique] */,
						    struct spoolss_SetPrinterInfoCtr *_info_ctr /* [in] [ref] */,
						    struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */,
						    struct sec_desc_buf *_secdesc_ctr /* [in] [ref] */,
						    struct spoolss_UserLevelCtr *_userlevel_ctr /* [in] [ref] */,
						    struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_AddPrinterEx_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_spoolss_AddPrinterEx(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char *_server /* [in] [charset(UTF16),unique] */,
				     struct spoolss_SetPrinterInfoCtr *_info_ctr /* [in] [ref] */,
				     struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */,
				     struct sec_desc_buf *_secdesc_ctr /* [in] [ref] */,
				     struct spoolss_UserLevelCtr *_userlevel_ctr /* [in] [ref] */,
				     struct policy_handle *_handle /* [out] [ref] */,
				     WERROR *result);

struct tevent_req *dcerpc_spoolss_SetPort_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_SetPort *r);
NTSTATUS dcerpc_spoolss_SetPort_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_SetPort_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_SetPort *r);
struct tevent_req *dcerpc_spoolss_SetPort_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       const char *_servername /* [in] [charset(UTF16),unique] */,
					       const char *_port_name /* [in] [unique,charset(UTF16)] */,
					       struct spoolss_SetPortInfoContainer *_port_ctr /* [in] [ref] */);
NTSTATUS dcerpc_spoolss_SetPort_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_spoolss_SetPort(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				const char *_servername /* [in] [charset(UTF16),unique] */,
				const char *_port_name /* [in] [unique,charset(UTF16)] */,
				struct spoolss_SetPortInfoContainer *_port_ctr /* [in] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_spoolss_EnumPrinterData_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_EnumPrinterData *r);
NTSTATUS dcerpc_spoolss_EnumPrinterData_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_EnumPrinterData_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_EnumPrinterData *r);
struct tevent_req *dcerpc_spoolss_EnumPrinterData_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct policy_handle *_handle /* [in] [ref] */,
						       uint32_t _enum_index /* [in]  */,
						       const char *_value_name /* [out] [size_is(value_offered/2),charset(UTF16)] */,
						       uint32_t _value_offered /* [in]  */,
						       uint32_t *_value_needed /* [out] [ref] */,
						       enum winreg_Type *_type /* [out] [ref] */,
						       uint8_t *_data /* [out] [ref,flag(LIBNDR_PRINT_ARRAY_HEX),size_is(data_offered)] */,
						       uint32_t _data_offered /* [in]  */,
						       uint32_t *_data_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_EnumPrinterData_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS dcerpc_spoolss_EnumPrinterData(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *_handle /* [in] [ref] */,
					uint32_t _enum_index /* [in]  */,
					const char *_value_name /* [out] [size_is(value_offered/2),charset(UTF16)] */,
					uint32_t _value_offered /* [in]  */,
					uint32_t *_value_needed /* [out] [ref] */,
					enum winreg_Type *_type /* [out] [ref] */,
					uint8_t *_data /* [out] [ref,flag(LIBNDR_PRINT_ARRAY_HEX),size_is(data_offered)] */,
					uint32_t _data_offered /* [in]  */,
					uint32_t *_data_needed /* [out] [ref] */,
					WERROR *result);

struct tevent_req *dcerpc_spoolss_DeletePrinterData_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_DeletePrinterData *r);
NTSTATUS dcerpc_spoolss_DeletePrinterData_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_DeletePrinterData_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_DeletePrinterData *r);
struct tevent_req *dcerpc_spoolss_DeletePrinterData_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_handle /* [in] [ref] */,
							 const char *_value_name /* [in] [charset(UTF16)] */);
NTSTATUS dcerpc_spoolss_DeletePrinterData_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_spoolss_DeletePrinterData(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_handle /* [in] [ref] */,
					  const char *_value_name /* [in] [charset(UTF16)] */,
					  WERROR *result);

struct tevent_req *dcerpc_spoolss_SetPrinterDataEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_SetPrinterDataEx *r);
NTSTATUS dcerpc_spoolss_SetPrinterDataEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_SetPrinterDataEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_SetPrinterDataEx *r);
struct tevent_req *dcerpc_spoolss_SetPrinterDataEx_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_handle /* [in] [ref] */,
							const char *_key_name /* [in] [charset(UTF16)] */,
							const char *_value_name /* [in] [charset(UTF16)] */,
							enum winreg_Type _type /* [in]  */,
							uint8_t *_data /* [in] [size_is(offered),ref] */,
							uint32_t _offered /* [in]  */);
NTSTATUS dcerpc_spoolss_SetPrinterDataEx_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_spoolss_SetPrinterDataEx(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_handle /* [in] [ref] */,
					 const char *_key_name /* [in] [charset(UTF16)] */,
					 const char *_value_name /* [in] [charset(UTF16)] */,
					 enum winreg_Type _type /* [in]  */,
					 uint8_t *_data /* [in] [size_is(offered),ref] */,
					 uint32_t _offered /* [in]  */,
					 WERROR *result);

struct tevent_req *dcerpc_spoolss_GetPrinterDataEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_GetPrinterDataEx *r);
NTSTATUS dcerpc_spoolss_GetPrinterDataEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_GetPrinterDataEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_GetPrinterDataEx *r);
struct tevent_req *dcerpc_spoolss_GetPrinterDataEx_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_handle /* [in] [ref] */,
							const char *_key_name /* [in] [charset(UTF16)] */,
							const char *_value_name /* [in] [charset(UTF16)] */,
							enum winreg_Type *_type /* [out] [ref] */,
							uint8_t *_data /* [out] [size_is(offered),ref] */,
							uint32_t _offered /* [in]  */,
							uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_GetPrinterDataEx_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_spoolss_GetPrinterDataEx(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_handle /* [in] [ref] */,
					 const char *_key_name /* [in] [charset(UTF16)] */,
					 const char *_value_name /* [in] [charset(UTF16)] */,
					 enum winreg_Type *_type /* [out] [ref] */,
					 uint8_t *_data /* [out] [size_is(offered),ref] */,
					 uint32_t _offered /* [in]  */,
					 uint32_t *_needed /* [out] [ref] */,
					 WERROR *result);

struct tevent_req *dcerpc_spoolss_EnumPrinterDataEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_EnumPrinterDataEx *r);
NTSTATUS dcerpc_spoolss_EnumPrinterDataEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_EnumPrinterDataEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_EnumPrinterDataEx *r);
struct tevent_req *dcerpc_spoolss_EnumPrinterDataEx_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_handle /* [in] [ref] */,
							 const char *_key_name /* [in] [charset(UTF16)] */,
							 uint32_t _offered /* [in]  */,
							 uint32_t *_count /* [out] [ref] */,
							 struct spoolss_PrinterEnumValues **_info /* [out] [ref,size_is(,*count)] */,
							 uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_EnumPrinterDataEx_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_spoolss_EnumPrinterDataEx(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_handle /* [in] [ref] */,
					  const char *_key_name /* [in] [charset(UTF16)] */,
					  uint32_t _offered /* [in]  */,
					  uint32_t *_count /* [out] [ref] */,
					  struct spoolss_PrinterEnumValues **_info /* [out] [ref,size_is(,*count)] */,
					  uint32_t *_needed /* [out] [ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_spoolss_EnumPrinterKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_EnumPrinterKey *r);
NTSTATUS dcerpc_spoolss_EnumPrinterKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_EnumPrinterKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_EnumPrinterKey *r);
struct tevent_req *dcerpc_spoolss_EnumPrinterKey_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_handle /* [in] [ref] */,
						      const char *_key_name /* [in] [charset(UTF16)] */,
						      uint32_t *__ndr_size /* [out] [ref] */,
						      union spoolss_KeyNames *_key_buffer /* [out] [ref,subcontext_size(*_ndr_size*2),switch_is(*_ndr_size),subcontext(0)] */,
						      uint32_t _offered /* [in]  */,
						      uint32_t *_needed /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_EnumPrinterKey_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_spoolss_EnumPrinterKey(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_handle /* [in] [ref] */,
				       const char *_key_name /* [in] [charset(UTF16)] */,
				       uint32_t *__ndr_size /* [out] [ref] */,
				       union spoolss_KeyNames *_key_buffer /* [out] [ref,subcontext_size(*_ndr_size*2),switch_is(*_ndr_size),subcontext(0)] */,
				       uint32_t _offered /* [in]  */,
				       uint32_t *_needed /* [out] [ref] */,
				       WERROR *result);

struct tevent_req *dcerpc_spoolss_DeletePrinterDataEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_DeletePrinterDataEx *r);
NTSTATUS dcerpc_spoolss_DeletePrinterDataEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_DeletePrinterDataEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_DeletePrinterDataEx *r);
struct tevent_req *dcerpc_spoolss_DeletePrinterDataEx_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   struct policy_handle *_handle /* [in] [ref] */,
							   const char *_key_name /* [in] [charset(UTF16)] */,
							   const char *_value_name /* [in] [charset(UTF16)] */);
NTSTATUS dcerpc_spoolss_DeletePrinterDataEx_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_spoolss_DeletePrinterDataEx(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    struct policy_handle *_handle /* [in] [ref] */,
					    const char *_key_name /* [in] [charset(UTF16)] */,
					    const char *_value_name /* [in] [charset(UTF16)] */,
					    WERROR *result);

struct tevent_req *dcerpc_spoolss_DeletePrinterKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_DeletePrinterKey *r);
NTSTATUS dcerpc_spoolss_DeletePrinterKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_DeletePrinterKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_DeletePrinterKey *r);
struct tevent_req *dcerpc_spoolss_DeletePrinterKey_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_handle /* [in] [ref] */,
							const char *_key_name /* [in] [charset(UTF16)] */);
NTSTATUS dcerpc_spoolss_DeletePrinterKey_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_spoolss_DeletePrinterKey(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_handle /* [in] [ref] */,
					 const char *_key_name /* [in] [charset(UTF16)] */,
					 WERROR *result);

struct tevent_req *dcerpc_spoolss_DeletePrinterDriverEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_DeletePrinterDriverEx *r);
NTSTATUS dcerpc_spoolss_DeletePrinterDriverEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_DeletePrinterDriverEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_DeletePrinterDriverEx *r);
struct tevent_req *dcerpc_spoolss_DeletePrinterDriverEx_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     const char *_server /* [in] [unique,charset(UTF16)] */,
							     const char *_architecture /* [in] [charset(UTF16)] */,
							     const char *_driver /* [in] [charset(UTF16)] */,
							     uint32_t _delete_flags /* [in]  */,
							     uint32_t _version /* [in]  */);
NTSTATUS dcerpc_spoolss_DeletePrinterDriverEx_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_spoolss_DeletePrinterDriverEx(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      const char *_server /* [in] [unique,charset(UTF16)] */,
					      const char *_architecture /* [in] [charset(UTF16)] */,
					      const char *_driver /* [in] [charset(UTF16)] */,
					      uint32_t _delete_flags /* [in]  */,
					      uint32_t _version /* [in]  */,
					      WERROR *result);

struct tevent_req *dcerpc_spoolss_AddPerMachineConnection_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_AddPerMachineConnection *r);
NTSTATUS dcerpc_spoolss_AddPerMachineConnection_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_AddPerMachineConnection_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_AddPerMachineConnection *r);
struct tevent_req *dcerpc_spoolss_AddPerMachineConnection_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct dcerpc_binding_handle *h,
							       const char *_server /* [in] [unique,charset(UTF16)] */,
							       const char *_printername /* [in] [charset(UTF16),ref] */,
							       const char *_printserver /* [in] [charset(UTF16),ref] */,
							       const char *_provider /* [in] [charset(UTF16),ref] */);
NTSTATUS dcerpc_spoolss_AddPerMachineConnection_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     WERROR *result);
NTSTATUS dcerpc_spoolss_AddPerMachineConnection(struct dcerpc_binding_handle *h,
						TALLOC_CTX *mem_ctx,
						const char *_server /* [in] [unique,charset(UTF16)] */,
						const char *_printername /* [in] [charset(UTF16),ref] */,
						const char *_printserver /* [in] [charset(UTF16),ref] */,
						const char *_provider /* [in] [charset(UTF16),ref] */,
						WERROR *result);

struct tevent_req *dcerpc_spoolss_DeletePerMachineConnection_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_DeletePerMachineConnection *r);
NTSTATUS dcerpc_spoolss_DeletePerMachineConnection_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_DeletePerMachineConnection_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_DeletePerMachineConnection *r);
struct tevent_req *dcerpc_spoolss_DeletePerMachineConnection_send(TALLOC_CTX *mem_ctx,
								  struct tevent_context *ev,
								  struct dcerpc_binding_handle *h,
								  const char *_server /* [in] [unique,charset(UTF16)] */,
								  const char *_printername /* [in] [charset(UTF16),ref] */);
NTSTATUS dcerpc_spoolss_DeletePerMachineConnection_recv(struct tevent_req *req,
							TALLOC_CTX *mem_ctx,
							WERROR *result);
NTSTATUS dcerpc_spoolss_DeletePerMachineConnection(struct dcerpc_binding_handle *h,
						   TALLOC_CTX *mem_ctx,
						   const char *_server /* [in] [unique,charset(UTF16)] */,
						   const char *_printername /* [in] [charset(UTF16),ref] */,
						   WERROR *result);

struct tevent_req *dcerpc_spoolss_XcvData_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_XcvData *r);
NTSTATUS dcerpc_spoolss_XcvData_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_XcvData_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_XcvData *r);
struct tevent_req *dcerpc_spoolss_XcvData_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct policy_handle *_handle /* [in] [ref] */,
					       const char *_function_name /* [in] [charset(UTF16)] */,
					       DATA_BLOB _in_data /* [in]  */,
					       uint32_t __in_data_length /* [in] [value(r->in.in_data.length)] */,
					       uint8_t *_out_data /* [out] [ref,size_is(out_data_size)] */,
					       uint32_t _out_data_size /* [in]  */,
					       uint32_t *_needed /* [out] [ref] */,
					       uint32_t *_status_code /* [in,out] [ref] */);
NTSTATUS dcerpc_spoolss_XcvData_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_spoolss_XcvData(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *_handle /* [in] [ref] */,
				const char *_function_name /* [in] [charset(UTF16)] */,
				DATA_BLOB _in_data /* [in]  */,
				uint32_t __in_data_length /* [in] [value(r->in.in_data.length)] */,
				uint8_t *_out_data /* [out] [ref,size_is(out_data_size)] */,
				uint32_t _out_data_size /* [in]  */,
				uint32_t *_needed /* [out] [ref] */,
				uint32_t *_status_code /* [in,out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_spoolss_AddPrinterDriverEx_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_AddPrinterDriverEx *r);
NTSTATUS dcerpc_spoolss_AddPrinterDriverEx_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_AddPrinterDriverEx_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_AddPrinterDriverEx *r);
struct tevent_req *dcerpc_spoolss_AddPrinterDriverEx_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  const char *_servername /* [in] [charset(UTF16),unique] */,
							  struct spoolss_AddDriverInfoCtr *_info_ctr /* [in] [ref] */,
							  uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_spoolss_AddPrinterDriverEx_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_spoolss_AddPrinterDriverEx(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   const char *_servername /* [in] [charset(UTF16),unique] */,
					   struct spoolss_AddDriverInfoCtr *_info_ctr /* [in] [ref] */,
					   uint32_t _flags /* [in]  */,
					   WERROR *result);

struct tevent_req *dcerpc_spoolss_GetCorePrinterDrivers_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_GetCorePrinterDrivers *r);
NTSTATUS dcerpc_spoolss_GetCorePrinterDrivers_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_GetCorePrinterDrivers_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_GetCorePrinterDrivers *r);
struct tevent_req *dcerpc_spoolss_GetCorePrinterDrivers_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     const char *_servername /* [in] [unique,charset(UTF16)] */,
							     const char *_architecture /* [in] [ref,charset(UTF16)] */,
							     uint32_t _core_driver_size /* [in]  */,
							     const char *_core_driver_dependencies /* [in] [charset(UTF16),ref,size_is(core_driver_size)] */,
							     uint32_t _core_printer_driver_count /* [in]  */,
							     struct spoolss_CorePrinterDriver *_core_printer_drivers /* [out] [ref,size_is(core_printer_driver_count)] */);
NTSTATUS dcerpc_spoolss_GetCorePrinterDrivers_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_spoolss_GetCorePrinterDrivers(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      const char *_servername /* [in] [unique,charset(UTF16)] */,
					      const char *_architecture /* [in] [ref,charset(UTF16)] */,
					      uint32_t _core_driver_size /* [in]  */,
					      const char *_core_driver_dependencies /* [in] [charset(UTF16),ref,size_is(core_driver_size)] */,
					      uint32_t _core_printer_driver_count /* [in]  */,
					      struct spoolss_CorePrinterDriver *_core_printer_drivers /* [out] [ref,size_is(core_printer_driver_count)] */,
					      WERROR *result);

struct tevent_req *dcerpc_spoolss_GetPrinterDriverPackagePath_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct spoolss_GetPrinterDriverPackagePath *r);
NTSTATUS dcerpc_spoolss_GetPrinterDriverPackagePath_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_spoolss_GetPrinterDriverPackagePath_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct spoolss_GetPrinterDriverPackagePath *r);
struct tevent_req *dcerpc_spoolss_GetPrinterDriverPackagePath_send(TALLOC_CTX *mem_ctx,
								   struct tevent_context *ev,
								   struct dcerpc_binding_handle *h,
								   const char *_servername /* [in] [charset(UTF16),unique] */,
								   const char *_architecture /* [in] [ref,charset(UTF16)] */,
								   const char *_language /* [in] [unique,charset(UTF16)] */,
								   const char *_package_id /* [in] [ref,charset(UTF16)] */,
								   const char *_driver_package_cab /* [in,out] [unique,size_is(driver_package_cab_size),charset(UTF16)] */,
								   uint32_t _driver_package_cab_size /* [in]  */,
								   uint32_t *_required /* [out] [ref] */);
NTSTATUS dcerpc_spoolss_GetPrinterDriverPackagePath_recv(struct tevent_req *req,
							 TALLOC_CTX *mem_ctx,
							 WERROR *result);
NTSTATUS dcerpc_spoolss_GetPrinterDriverPackagePath(struct dcerpc_binding_handle *h,
						    TALLOC_CTX *mem_ctx,
						    const char *_servername /* [in] [charset(UTF16),unique] */,
						    const char *_architecture /* [in] [ref,charset(UTF16)] */,
						    const char *_language /* [in] [unique,charset(UTF16)] */,
						    const char *_package_id /* [in] [ref,charset(UTF16)] */,
						    const char *_driver_package_cab /* [in,out] [unique,size_is(driver_package_cab_size),charset(UTF16)] */,
						    uint32_t _driver_package_cab_size /* [in]  */,
						    uint32_t *_required /* [out] [ref] */,
						    WERROR *result);

#endif /* _HEADER_RPC_spoolss */
