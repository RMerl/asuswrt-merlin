#include "../librpc/gen_ndr/ndr_spoolss.h"
#ifndef __CLI_SPOOLSS__
#define __CLI_SPOOLSS__
struct tevent_req *rpccli_spoolss_EnumPrinters_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    uint32_t _flags /* [in]  */,
						    const char *_server /* [in] [unique,charset(UTF16)] */,
						    uint32_t _level /* [in]  */,
						    DATA_BLOB *_buffer /* [in] [unique] */,
						    uint32_t _offered /* [in]  */,
						    uint32_t *_count /* [out] [ref] */,
						    union spoolss_PrinterInfo **_info /* [out] [ref,switch_is(level),size_is(,*count)] */,
						    uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_EnumPrinters_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_spoolss_EnumPrinters(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     uint32_t flags /* [in]  */,
				     const char *server /* [in] [unique,charset(UTF16)] */,
				     uint32_t level /* [in]  */,
				     DATA_BLOB *buffer /* [in] [unique] */,
				     uint32_t offered /* [in]  */,
				     uint32_t *count /* [out] [ref] */,
				     union spoolss_PrinterInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
				     uint32_t *needed /* [out] [ref] */,
				     WERROR *werror);
struct tevent_req *rpccli_spoolss_OpenPrinter_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct rpc_pipe_client *cli,
						   const char *_printername /* [in] [unique,charset(UTF16)] */,
						   const char *_datatype /* [in] [unique,charset(UTF16)] */,
						   struct spoolss_DevmodeContainer _devmode_ctr /* [in]  */,
						   uint32_t _access_mask /* [in]  */,
						   struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS rpccli_spoolss_OpenPrinter_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS rpccli_spoolss_OpenPrinter(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    const char *printername /* [in] [unique,charset(UTF16)] */,
				    const char *datatype /* [in] [unique,charset(UTF16)] */,
				    struct spoolss_DevmodeContainer devmode_ctr /* [in]  */,
				    uint32_t access_mask /* [in]  */,
				    struct policy_handle *handle /* [out] [ref] */,
				    WERROR *werror);
struct tevent_req *rpccli_spoolss_SetJob_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli,
					      struct policy_handle *_handle /* [in] [ref] */,
					      uint32_t _job_id /* [in]  */,
					      struct spoolss_JobInfoContainer *_ctr /* [in] [unique] */,
					      enum spoolss_JobControl _command /* [in]  */);
NTSTATUS rpccli_spoolss_SetJob_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS rpccli_spoolss_SetJob(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *handle /* [in] [ref] */,
			       uint32_t job_id /* [in]  */,
			       struct spoolss_JobInfoContainer *ctr /* [in] [unique] */,
			       enum spoolss_JobControl command /* [in]  */,
			       WERROR *werror);
struct tevent_req *rpccli_spoolss_GetJob_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli,
					      struct policy_handle *_handle /* [in] [ref] */,
					      uint32_t _job_id /* [in]  */,
					      uint32_t _level /* [in]  */,
					      DATA_BLOB *_buffer /* [in] [unique] */,
					      uint32_t _offered /* [in]  */,
					      union spoolss_JobInfo *_info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
					      uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_GetJob_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS rpccli_spoolss_GetJob(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *handle /* [in] [ref] */,
			       uint32_t job_id /* [in]  */,
			       uint32_t level /* [in]  */,
			       DATA_BLOB *buffer /* [in] [unique] */,
			       uint32_t offered /* [in]  */,
			       union spoolss_JobInfo *info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
			       uint32_t *needed /* [out] [ref] */,
			       WERROR *werror);
struct tevent_req *rpccli_spoolss_EnumJobs_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct rpc_pipe_client *cli,
						struct policy_handle *_handle /* [in] [ref] */,
						uint32_t _firstjob /* [in]  */,
						uint32_t _numjobs /* [in]  */,
						uint32_t _level /* [in]  */,
						DATA_BLOB *_buffer /* [in] [unique] */,
						uint32_t _offered /* [in]  */,
						uint32_t *_count /* [out] [ref] */,
						union spoolss_JobInfo **_info /* [out] [ref,switch_is(level),size_is(,*count)] */,
						uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_EnumJobs_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS rpccli_spoolss_EnumJobs(struct rpc_pipe_client *cli,
				 TALLOC_CTX *mem_ctx,
				 struct policy_handle *handle /* [in] [ref] */,
				 uint32_t firstjob /* [in]  */,
				 uint32_t numjobs /* [in]  */,
				 uint32_t level /* [in]  */,
				 DATA_BLOB *buffer /* [in] [unique] */,
				 uint32_t offered /* [in]  */,
				 uint32_t *count /* [out] [ref] */,
				 union spoolss_JobInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
				 uint32_t *needed /* [out] [ref] */,
				 WERROR *werror);
struct tevent_req *rpccli_spoolss_AddPrinter_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli,
						  const char *_server /* [in] [unique,charset(UTF16)] */,
						  struct spoolss_SetPrinterInfoCtr *_info_ctr /* [in] [ref] */,
						  struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */,
						  struct sec_desc_buf *_secdesc_ctr /* [in] [ref] */,
						  struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS rpccli_spoolss_AddPrinter_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS rpccli_spoolss_AddPrinter(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   const char *server /* [in] [unique,charset(UTF16)] */,
				   struct spoolss_SetPrinterInfoCtr *info_ctr /* [in] [ref] */,
				   struct spoolss_DevmodeContainer *devmode_ctr /* [in] [ref] */,
				   struct sec_desc_buf *secdesc_ctr /* [in] [ref] */,
				   struct policy_handle *handle /* [out] [ref] */,
				   WERROR *werror);
struct tevent_req *rpccli_spoolss_DeletePrinter_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli,
						     struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS rpccli_spoolss_DeletePrinter_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS rpccli_spoolss_DeletePrinter(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *handle /* [in] [ref] */,
				      WERROR *werror);
struct tevent_req *rpccli_spoolss_SetPrinter_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli,
						  struct policy_handle *_handle /* [in] [ref] */,
						  struct spoolss_SetPrinterInfoCtr *_info_ctr /* [in] [ref] */,
						  struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */,
						  struct sec_desc_buf *_secdesc_ctr /* [in] [ref] */,
						  enum spoolss_PrinterControl _command /* [in]  */);
NTSTATUS rpccli_spoolss_SetPrinter_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS rpccli_spoolss_SetPrinter(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *handle /* [in] [ref] */,
				   struct spoolss_SetPrinterInfoCtr *info_ctr /* [in] [ref] */,
				   struct spoolss_DevmodeContainer *devmode_ctr /* [in] [ref] */,
				   struct sec_desc_buf *secdesc_ctr /* [in] [ref] */,
				   enum spoolss_PrinterControl command /* [in]  */,
				   WERROR *werror);
struct tevent_req *rpccli_spoolss_GetPrinter_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli,
						  struct policy_handle *_handle /* [in] [ref] */,
						  uint32_t _level /* [in]  */,
						  DATA_BLOB *_buffer /* [in] [unique] */,
						  uint32_t _offered /* [in]  */,
						  union spoolss_PrinterInfo *_info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
						  uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_GetPrinter_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS rpccli_spoolss_GetPrinter(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *handle /* [in] [ref] */,
				   uint32_t level /* [in]  */,
				   DATA_BLOB *buffer /* [in] [unique] */,
				   uint32_t offered /* [in]  */,
				   union spoolss_PrinterInfo *info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
				   uint32_t *needed /* [out] [ref] */,
				   WERROR *werror);
struct tevent_req *rpccli_spoolss_AddPrinterDriver_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							const char *_servername /* [in] [unique,charset(UTF16)] */,
							struct spoolss_AddDriverInfoCtr *_info_ctr /* [in] [ref] */);
NTSTATUS rpccli_spoolss_AddPrinterDriver_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_spoolss_AddPrinterDriver(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 const char *servername /* [in] [unique,charset(UTF16)] */,
					 struct spoolss_AddDriverInfoCtr *info_ctr /* [in] [ref] */,
					 WERROR *werror);
struct tevent_req *rpccli_spoolss_EnumPrinterDrivers_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct rpc_pipe_client *cli,
							  const char *_server /* [in] [unique,charset(UTF16)] */,
							  const char *_environment /* [in] [unique,charset(UTF16)] */,
							  uint32_t _level /* [in]  */,
							  DATA_BLOB *_buffer /* [in] [unique] */,
							  uint32_t _offered /* [in]  */,
							  uint32_t *_count /* [out] [ref] */,
							  union spoolss_DriverInfo **_info /* [out] [ref,switch_is(level),size_is(,*count)] */,
							  uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_EnumPrinterDrivers_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS rpccli_spoolss_EnumPrinterDrivers(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   const char *server /* [in] [unique,charset(UTF16)] */,
					   const char *environment /* [in] [unique,charset(UTF16)] */,
					   uint32_t level /* [in]  */,
					   DATA_BLOB *buffer /* [in] [unique] */,
					   uint32_t offered /* [in]  */,
					   uint32_t *count /* [out] [ref] */,
					   union spoolss_DriverInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
					   uint32_t *needed /* [out] [ref] */,
					   WERROR *werror);
struct tevent_req *rpccli_spoolss_GetPrinterDriver_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							struct policy_handle *_handle /* [in] [ref] */,
							const char *_architecture /* [in] [unique,charset(UTF16)] */,
							uint32_t _level /* [in]  */,
							DATA_BLOB *_buffer /* [in] [unique] */,
							uint32_t _offered /* [in]  */,
							union spoolss_DriverInfo *_info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
							uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_GetPrinterDriver_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_spoolss_GetPrinterDriver(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *handle /* [in] [ref] */,
					 const char *architecture /* [in] [unique,charset(UTF16)] */,
					 uint32_t level /* [in]  */,
					 DATA_BLOB *buffer /* [in] [unique] */,
					 uint32_t offered /* [in]  */,
					 union spoolss_DriverInfo *info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
					 uint32_t *needed /* [out] [ref] */,
					 WERROR *werror);
struct tevent_req *rpccli_spoolss_GetPrinterDriverDirectory_send(TALLOC_CTX *mem_ctx,
								 struct tevent_context *ev,
								 struct rpc_pipe_client *cli,
								 const char *_server /* [in] [unique,charset(UTF16)] */,
								 const char *_environment /* [in] [unique,charset(UTF16)] */,
								 uint32_t _level /* [in]  */,
								 DATA_BLOB *_buffer /* [in] [unique] */,
								 uint32_t _offered /* [in]  */,
								 union spoolss_DriverDirectoryInfo *_info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
								 uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_GetPrinterDriverDirectory_recv(struct tevent_req *req,
						       TALLOC_CTX *mem_ctx,
						       WERROR *result);
NTSTATUS rpccli_spoolss_GetPrinterDriverDirectory(struct rpc_pipe_client *cli,
						  TALLOC_CTX *mem_ctx,
						  const char *server /* [in] [unique,charset(UTF16)] */,
						  const char *environment /* [in] [unique,charset(UTF16)] */,
						  uint32_t level /* [in]  */,
						  DATA_BLOB *buffer /* [in] [unique] */,
						  uint32_t offered /* [in]  */,
						  union spoolss_DriverDirectoryInfo *info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
						  uint32_t *needed /* [out] [ref] */,
						  WERROR *werror);
struct tevent_req *rpccli_spoolss_DeletePrinterDriver_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct rpc_pipe_client *cli,
							   const char *_server /* [in] [unique,charset(UTF16)] */,
							   const char *_architecture /* [in] [charset(UTF16)] */,
							   const char *_driver /* [in] [charset(UTF16)] */);
NTSTATUS rpccli_spoolss_DeletePrinterDriver_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS rpccli_spoolss_DeletePrinterDriver(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    const char *server /* [in] [unique,charset(UTF16)] */,
					    const char *architecture /* [in] [charset(UTF16)] */,
					    const char *driver /* [in] [charset(UTF16)] */,
					    WERROR *werror);
struct tevent_req *rpccli_spoolss_AddPrintProcessor_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli,
							 const char *_server /* [in] [unique,charset(UTF16)] */,
							 const char *_architecture /* [in] [charset(UTF16)] */,
							 const char *_path_name /* [in] [charset(UTF16)] */,
							 const char *_print_processor_name /* [in] [charset(UTF16)] */);
NTSTATUS rpccli_spoolss_AddPrintProcessor_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS rpccli_spoolss_AddPrintProcessor(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  const char *server /* [in] [unique,charset(UTF16)] */,
					  const char *architecture /* [in] [charset(UTF16)] */,
					  const char *path_name /* [in] [charset(UTF16)] */,
					  const char *print_processor_name /* [in] [charset(UTF16)] */,
					  WERROR *werror);
struct tevent_req *rpccli_spoolss_EnumPrintProcessors_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct rpc_pipe_client *cli,
							   const char *_servername /* [in] [unique,charset(UTF16)] */,
							   const char *_environment /* [in] [unique,charset(UTF16)] */,
							   uint32_t _level /* [in]  */,
							   DATA_BLOB *_buffer /* [in] [unique] */,
							   uint32_t _offered /* [in]  */,
							   uint32_t *_count /* [out] [ref] */,
							   union spoolss_PrintProcessorInfo **_info /* [out] [ref,switch_is(level),size_is(,*count)] */,
							   uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_EnumPrintProcessors_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS rpccli_spoolss_EnumPrintProcessors(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    const char *servername /* [in] [unique,charset(UTF16)] */,
					    const char *environment /* [in] [unique,charset(UTF16)] */,
					    uint32_t level /* [in]  */,
					    DATA_BLOB *buffer /* [in] [unique] */,
					    uint32_t offered /* [in]  */,
					    uint32_t *count /* [out] [ref] */,
					    union spoolss_PrintProcessorInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
					    uint32_t *needed /* [out] [ref] */,
					    WERROR *werror);
struct tevent_req *rpccli_spoolss_GetPrintProcessorDirectory_send(TALLOC_CTX *mem_ctx,
								  struct tevent_context *ev,
								  struct rpc_pipe_client *cli,
								  const char *_server /* [in] [unique,charset(UTF16)] */,
								  const char *_environment /* [in] [unique,charset(UTF16)] */,
								  uint32_t _level /* [in]  */,
								  DATA_BLOB *_buffer /* [in] [unique] */,
								  uint32_t _offered /* [in]  */,
								  union spoolss_PrintProcessorDirectoryInfo *_info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
								  uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_GetPrintProcessorDirectory_recv(struct tevent_req *req,
							TALLOC_CTX *mem_ctx,
							WERROR *result);
NTSTATUS rpccli_spoolss_GetPrintProcessorDirectory(struct rpc_pipe_client *cli,
						   TALLOC_CTX *mem_ctx,
						   const char *server /* [in] [unique,charset(UTF16)] */,
						   const char *environment /* [in] [unique,charset(UTF16)] */,
						   uint32_t level /* [in]  */,
						   DATA_BLOB *buffer /* [in] [unique] */,
						   uint32_t offered /* [in]  */,
						   union spoolss_PrintProcessorDirectoryInfo *info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
						   uint32_t *needed /* [out] [ref] */,
						   WERROR *werror);
struct tevent_req *rpccli_spoolss_StartDocPrinter_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct rpc_pipe_client *cli,
						       struct policy_handle *_handle /* [in] [ref] */,
						       uint32_t _level /* [in]  */,
						       union spoolss_DocumentInfo _info /* [in] [switch_is(level)] */,
						       uint32_t *_job_id /* [out] [ref] */);
NTSTATUS rpccli_spoolss_StartDocPrinter_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS rpccli_spoolss_StartDocPrinter(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *handle /* [in] [ref] */,
					uint32_t level /* [in]  */,
					union spoolss_DocumentInfo info /* [in] [switch_is(level)] */,
					uint32_t *job_id /* [out] [ref] */,
					WERROR *werror);
struct tevent_req *rpccli_spoolss_StartPagePrinter_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS rpccli_spoolss_StartPagePrinter_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_spoolss_StartPagePrinter(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *handle /* [in] [ref] */,
					 WERROR *werror);
struct tevent_req *rpccli_spoolss_WritePrinter_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct policy_handle *_handle /* [in] [ref] */,
						    DATA_BLOB _data /* [in]  */,
						    uint32_t __data_size /* [in] [value(r->in.data.length)] */,
						    uint32_t *_num_written /* [out] [ref] */);
NTSTATUS rpccli_spoolss_WritePrinter_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_spoolss_WritePrinter(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle /* [in] [ref] */,
				     DATA_BLOB data /* [in]  */,
				     uint32_t _data_size /* [in] [value(r->in.data.length)] */,
				     uint32_t *num_written /* [out] [ref] */,
				     WERROR *werror);
struct tevent_req *rpccli_spoolss_EndPagePrinter_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct rpc_pipe_client *cli,
						      struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS rpccli_spoolss_EndPagePrinter_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS rpccli_spoolss_EndPagePrinter(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *handle /* [in] [ref] */,
				       WERROR *werror);
struct tevent_req *rpccli_spoolss_AbortPrinter_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS rpccli_spoolss_AbortPrinter_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_spoolss_AbortPrinter(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle /* [in] [ref] */,
				     WERROR *werror);
struct tevent_req *rpccli_spoolss_ReadPrinter_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct rpc_pipe_client *cli,
						   struct policy_handle *_handle /* [in] [ref] */,
						   uint8_t *_data /* [out] [ref,size_is(data_size)] */,
						   uint32_t _data_size /* [in]  */,
						   uint32_t *__data_size /* [out] [ref] */);
NTSTATUS rpccli_spoolss_ReadPrinter_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS rpccli_spoolss_ReadPrinter(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *handle /* [in] [ref] */,
				    uint8_t *data /* [out] [ref,size_is(data_size)] */,
				    uint32_t data_size /* [in]  */,
				    uint32_t *_data_size /* [out] [ref] */,
				    WERROR *werror);
struct tevent_req *rpccli_spoolss_EndDocPrinter_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli,
						     struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS rpccli_spoolss_EndDocPrinter_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS rpccli_spoolss_EndDocPrinter(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *handle /* [in] [ref] */,
				      WERROR *werror);
struct tevent_req *rpccli_spoolss_AddJob_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli,
					      struct policy_handle *_handle /* [in] [ref] */,
					      uint32_t _level /* [in]  */,
					      uint8_t *_buffer /* [in,out] [unique,size_is(offered)] */,
					      uint32_t _offered /* [in]  */,
					      uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_AddJob_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS rpccli_spoolss_AddJob(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *handle /* [in] [ref] */,
			       uint32_t level /* [in]  */,
			       uint8_t *buffer /* [in,out] [unique,size_is(offered)] */,
			       uint32_t offered /* [in]  */,
			       uint32_t *needed /* [out] [ref] */,
			       WERROR *werror);
struct tevent_req *rpccli_spoolss_ScheduleJob_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct rpc_pipe_client *cli,
						   struct policy_handle *_handle /* [in] [ref] */,
						   uint32_t _jobid /* [in]  */);
NTSTATUS rpccli_spoolss_ScheduleJob_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS rpccli_spoolss_ScheduleJob(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    struct policy_handle *handle /* [in] [ref] */,
				    uint32_t jobid /* [in]  */,
				    WERROR *werror);
struct tevent_req *rpccli_spoolss_GetPrinterData_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct rpc_pipe_client *cli,
						      struct policy_handle *_handle /* [in] [ref] */,
						      const char *_value_name /* [in] [charset(UTF16)] */,
						      enum winreg_Type *_type /* [out] [ref] */,
						      uint8_t *_data /* [out] [ref,size_is(offered)] */,
						      uint32_t _offered /* [in]  */,
						      uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_GetPrinterData_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS rpccli_spoolss_GetPrinterData(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *handle /* [in] [ref] */,
				       const char *value_name /* [in] [charset(UTF16)] */,
				       enum winreg_Type *type /* [out] [ref] */,
				       uint8_t *data /* [out] [ref,size_is(offered)] */,
				       uint32_t offered /* [in]  */,
				       uint32_t *needed /* [out] [ref] */,
				       WERROR *werror);
struct tevent_req *rpccli_spoolss_SetPrinterData_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct rpc_pipe_client *cli,
						      struct policy_handle *_handle /* [in] [ref] */,
						      const char *_value_name /* [in] [charset(UTF16)] */,
						      enum winreg_Type _type /* [in]  */,
						      uint8_t *_data /* [in] [ref,size_is(offered)] */,
						      uint32_t _offered /* [in]  */);
NTSTATUS rpccli_spoolss_SetPrinterData_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS rpccli_spoolss_SetPrinterData(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *handle /* [in] [ref] */,
				       const char *value_name /* [in] [charset(UTF16)] */,
				       enum winreg_Type type /* [in]  */,
				       uint8_t *data /* [in] [ref,size_is(offered)] */,
				       uint32_t offered /* [in]  */,
				       WERROR *werror);
struct tevent_req *rpccli_spoolss_WaitForPrinterChange_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_WaitForPrinterChange_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS rpccli_spoolss_WaitForPrinterChange(struct rpc_pipe_client *cli,
					     TALLOC_CTX *mem_ctx,
					     WERROR *werror);
struct tevent_req *rpccli_spoolss_ClosePrinter_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS rpccli_spoolss_ClosePrinter_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_spoolss_ClosePrinter(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle /* [in,out] [ref] */,
				     WERROR *werror);
struct tevent_req *rpccli_spoolss_AddForm_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       struct policy_handle *_handle /* [in] [ref] */,
					       uint32_t _level /* [in]  */,
					       union spoolss_AddFormInfo _info /* [in] [switch_is(level)] */);
NTSTATUS rpccli_spoolss_AddForm_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS rpccli_spoolss_AddForm(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *handle /* [in] [ref] */,
				uint32_t level /* [in]  */,
				union spoolss_AddFormInfo info /* [in] [switch_is(level)] */,
				WERROR *werror);
struct tevent_req *rpccli_spoolss_DeleteForm_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli,
						  struct policy_handle *_handle /* [in] [ref] */,
						  const char *_form_name /* [in] [charset(UTF16)] */);
NTSTATUS rpccli_spoolss_DeleteForm_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS rpccli_spoolss_DeleteForm(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   struct policy_handle *handle /* [in] [ref] */,
				   const char *form_name /* [in] [charset(UTF16)] */,
				   WERROR *werror);
struct tevent_req *rpccli_spoolss_GetForm_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       struct policy_handle *_handle /* [in] [ref] */,
					       const char *_form_name /* [in] [charset(UTF16)] */,
					       uint32_t _level /* [in]  */,
					       DATA_BLOB *_buffer /* [in] [unique] */,
					       uint32_t _offered /* [in]  */,
					       union spoolss_FormInfo *_info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
					       uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_GetForm_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS rpccli_spoolss_GetForm(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *handle /* [in] [ref] */,
				const char *form_name /* [in] [charset(UTF16)] */,
				uint32_t level /* [in]  */,
				DATA_BLOB *buffer /* [in] [unique] */,
				uint32_t offered /* [in]  */,
				union spoolss_FormInfo *info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
				uint32_t *needed /* [out] [ref] */,
				WERROR *werror);
struct tevent_req *rpccli_spoolss_SetForm_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       struct policy_handle *_handle /* [in] [ref] */,
					       const char *_form_name /* [in] [charset(UTF16)] */,
					       uint32_t _level /* [in]  */,
					       union spoolss_AddFormInfo _info /* [in] [switch_is(level)] */);
NTSTATUS rpccli_spoolss_SetForm_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS rpccli_spoolss_SetForm(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *handle /* [in] [ref] */,
				const char *form_name /* [in] [charset(UTF16)] */,
				uint32_t level /* [in]  */,
				union spoolss_AddFormInfo info /* [in] [switch_is(level)] */,
				WERROR *werror);
struct tevent_req *rpccli_spoolss_EnumForms_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli,
						 struct policy_handle *_handle /* [in] [ref] */,
						 uint32_t _level /* [in]  */,
						 DATA_BLOB *_buffer /* [in] [unique] */,
						 uint32_t _offered /* [in]  */,
						 uint32_t *_count /* [out] [ref] */,
						 union spoolss_FormInfo **_info /* [out] [ref,switch_is(level),size_is(,*count)] */,
						 uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_EnumForms_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS rpccli_spoolss_EnumForms(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *handle /* [in] [ref] */,
				  uint32_t level /* [in]  */,
				  DATA_BLOB *buffer /* [in] [unique] */,
				  uint32_t offered /* [in]  */,
				  uint32_t *count /* [out] [ref] */,
				  union spoolss_FormInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
				  uint32_t *needed /* [out] [ref] */,
				  WERROR *werror);
struct tevent_req *rpccli_spoolss_EnumPorts_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli,
						 const char *_servername /* [in] [unique,charset(UTF16)] */,
						 uint32_t _level /* [in]  */,
						 DATA_BLOB *_buffer /* [in] [unique] */,
						 uint32_t _offered /* [in]  */,
						 uint32_t *_count /* [out] [ref] */,
						 union spoolss_PortInfo **_info /* [out] [ref,switch_is(level),size_is(,*count)] */,
						 uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_EnumPorts_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS rpccli_spoolss_EnumPorts(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  const char *servername /* [in] [unique,charset(UTF16)] */,
				  uint32_t level /* [in]  */,
				  DATA_BLOB *buffer /* [in] [unique] */,
				  uint32_t offered /* [in]  */,
				  uint32_t *count /* [out] [ref] */,
				  union spoolss_PortInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
				  uint32_t *needed /* [out] [ref] */,
				  WERROR *werror);
struct tevent_req *rpccli_spoolss_EnumMonitors_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    const char *_servername /* [in] [unique,charset(UTF16)] */,
						    uint32_t _level /* [in]  */,
						    DATA_BLOB *_buffer /* [in] [unique] */,
						    uint32_t _offered /* [in]  */,
						    uint32_t *_count /* [out] [ref] */,
						    union spoolss_MonitorInfo **_info /* [out] [ref,switch_is(level),size_is(,*count)] */,
						    uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_EnumMonitors_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_spoolss_EnumMonitors(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     const char *servername /* [in] [unique,charset(UTF16)] */,
				     uint32_t level /* [in]  */,
				     DATA_BLOB *buffer /* [in] [unique] */,
				     uint32_t offered /* [in]  */,
				     uint32_t *count /* [out] [ref] */,
				     union spoolss_MonitorInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
				     uint32_t *needed /* [out] [ref] */,
				     WERROR *werror);
struct tevent_req *rpccli_spoolss_AddPort_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       const char *_server_name /* [in] [unique,charset(UTF16)] */,
					       uint32_t _unknown /* [in]  */,
					       const char *_monitor_name /* [in] [charset(UTF16)] */);
NTSTATUS rpccli_spoolss_AddPort_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS rpccli_spoolss_AddPort(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				const char *server_name /* [in] [unique,charset(UTF16)] */,
				uint32_t unknown /* [in]  */,
				const char *monitor_name /* [in] [charset(UTF16)] */,
				WERROR *werror);
struct tevent_req *rpccli_spoolss_ConfigurePort_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_ConfigurePort_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS rpccli_spoolss_ConfigurePort(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      WERROR *werror);
struct tevent_req *rpccli_spoolss_DeletePort_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_DeletePort_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS rpccli_spoolss_DeletePort(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   WERROR *werror);
struct tevent_req *rpccli_spoolss_CreatePrinterIC_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct rpc_pipe_client *cli,
						       struct policy_handle *_handle /* [in] [ref] */,
						       struct policy_handle *_gdi_handle /* [out] [ref] */,
						       struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */);
NTSTATUS rpccli_spoolss_CreatePrinterIC_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS rpccli_spoolss_CreatePrinterIC(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *handle /* [in] [ref] */,
					struct policy_handle *gdi_handle /* [out] [ref] */,
					struct spoolss_DevmodeContainer *devmode_ctr /* [in] [ref] */,
					WERROR *werror);
struct tevent_req *rpccli_spoolss_PlayGDIScriptOnPrinterIC_send(TALLOC_CTX *mem_ctx,
								struct tevent_context *ev,
								struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_PlayGDIScriptOnPrinterIC_recv(struct tevent_req *req,
						      TALLOC_CTX *mem_ctx,
						      WERROR *result);
NTSTATUS rpccli_spoolss_PlayGDIScriptOnPrinterIC(struct rpc_pipe_client *cli,
						 TALLOC_CTX *mem_ctx,
						 WERROR *werror);
struct tevent_req *rpccli_spoolss_DeletePrinterIC_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct rpc_pipe_client *cli,
						       struct policy_handle *_gdi_handle /* [in,out] [ref] */);
NTSTATUS rpccli_spoolss_DeletePrinterIC_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS rpccli_spoolss_DeletePrinterIC(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *gdi_handle /* [in,out] [ref] */,
					WERROR *werror);
struct tevent_req *rpccli_spoolss_AddPrinterConnection_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_AddPrinterConnection_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS rpccli_spoolss_AddPrinterConnection(struct rpc_pipe_client *cli,
					     TALLOC_CTX *mem_ctx,
					     WERROR *werror);
struct tevent_req *rpccli_spoolss_DeletePrinterConnection_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_DeletePrinterConnection_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx,
						     WERROR *result);
NTSTATUS rpccli_spoolss_DeletePrinterConnection(struct rpc_pipe_client *cli,
						TALLOC_CTX *mem_ctx,
						WERROR *werror);
struct tevent_req *rpccli_spoolss_PrinterMessageBox_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_PrinterMessageBox_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS rpccli_spoolss_PrinterMessageBox(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  WERROR *werror);
struct tevent_req *rpccli_spoolss_AddMonitor_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_AddMonitor_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS rpccli_spoolss_AddMonitor(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   WERROR *werror);
struct tevent_req *rpccli_spoolss_DeleteMonitor_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_DeleteMonitor_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS rpccli_spoolss_DeleteMonitor(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      WERROR *werror);
struct tevent_req *rpccli_spoolss_DeletePrintProcessor_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_DeletePrintProcessor_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS rpccli_spoolss_DeletePrintProcessor(struct rpc_pipe_client *cli,
					     TALLOC_CTX *mem_ctx,
					     WERROR *werror);
struct tevent_req *rpccli_spoolss_AddPrintProvidor_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_AddPrintProvidor_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_spoolss_AddPrintProvidor(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 WERROR *werror);
struct tevent_req *rpccli_spoolss_DeletePrintProvidor_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_DeletePrintProvidor_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS rpccli_spoolss_DeletePrintProvidor(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    WERROR *werror);
struct tevent_req *rpccli_spoolss_EnumPrintProcDataTypes_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct rpc_pipe_client *cli,
							      const char *_servername /* [in] [unique,charset(UTF16)] */,
							      const char *_print_processor_name /* [in] [unique,charset(UTF16)] */,
							      uint32_t _level /* [in]  */,
							      DATA_BLOB *_buffer /* [in] [unique] */,
							      uint32_t _offered /* [in]  */,
							      uint32_t *_count /* [out] [ref] */,
							      union spoolss_PrintProcDataTypesInfo **_info /* [out] [ref,switch_is(level),size_is(,*count)] */,
							      uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_EnumPrintProcDataTypes_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    WERROR *result);
NTSTATUS rpccli_spoolss_EnumPrintProcDataTypes(struct rpc_pipe_client *cli,
					       TALLOC_CTX *mem_ctx,
					       const char *servername /* [in] [unique,charset(UTF16)] */,
					       const char *print_processor_name /* [in] [unique,charset(UTF16)] */,
					       uint32_t level /* [in]  */,
					       DATA_BLOB *buffer /* [in] [unique] */,
					       uint32_t offered /* [in]  */,
					       uint32_t *count /* [out] [ref] */,
					       union spoolss_PrintProcDataTypesInfo **info /* [out] [ref,switch_is(level),size_is(,*count)] */,
					       uint32_t *needed /* [out] [ref] */,
					       WERROR *werror);
struct tevent_req *rpccli_spoolss_ResetPrinter_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct policy_handle *_handle /* [in] [ref] */,
						    const char *_data_type /* [in] [unique,charset(UTF16)] */,
						    struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */);
NTSTATUS rpccli_spoolss_ResetPrinter_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_spoolss_ResetPrinter(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct policy_handle *handle /* [in] [ref] */,
				     const char *data_type /* [in] [unique,charset(UTF16)] */,
				     struct spoolss_DevmodeContainer *devmode_ctr /* [in] [ref] */,
				     WERROR *werror);
struct tevent_req *rpccli_spoolss_GetPrinterDriver2_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli,
							 struct policy_handle *_handle /* [in] [ref] */,
							 const char *_architecture /* [in] [unique,charset(UTF16)] */,
							 uint32_t _level /* [in]  */,
							 DATA_BLOB *_buffer /* [in] [unique] */,
							 uint32_t _offered /* [in]  */,
							 uint32_t _client_major_version /* [in]  */,
							 uint32_t _client_minor_version /* [in]  */,
							 union spoolss_DriverInfo *_info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
							 uint32_t *_needed /* [out] [ref] */,
							 uint32_t *_server_major_version /* [out] [ref] */,
							 uint32_t *_server_minor_version /* [out] [ref] */);
NTSTATUS rpccli_spoolss_GetPrinterDriver2_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS rpccli_spoolss_GetPrinterDriver2(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *handle /* [in] [ref] */,
					  const char *architecture /* [in] [unique,charset(UTF16)] */,
					  uint32_t level /* [in]  */,
					  DATA_BLOB *buffer /* [in] [unique] */,
					  uint32_t offered /* [in]  */,
					  uint32_t client_major_version /* [in]  */,
					  uint32_t client_minor_version /* [in]  */,
					  union spoolss_DriverInfo *info /* [out] [unique,subcontext_size(offered),subcontext(4),switch_is(level)] */,
					  uint32_t *needed /* [out] [ref] */,
					  uint32_t *server_major_version /* [out] [ref] */,
					  uint32_t *server_minor_version /* [out] [ref] */,
					  WERROR *werror);
struct tevent_req *rpccli_spoolss_FindFirstPrinterChangeNotification_send(TALLOC_CTX *mem_ctx,
									  struct tevent_context *ev,
									  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_FindFirstPrinterChangeNotification_recv(struct tevent_req *req,
								TALLOC_CTX *mem_ctx,
								WERROR *result);
NTSTATUS rpccli_spoolss_FindFirstPrinterChangeNotification(struct rpc_pipe_client *cli,
							   TALLOC_CTX *mem_ctx,
							   WERROR *werror);
struct tevent_req *rpccli_spoolss_FindNextPrinterChangeNotification_send(TALLOC_CTX *mem_ctx,
									 struct tevent_context *ev,
									 struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_FindNextPrinterChangeNotification_recv(struct tevent_req *req,
							       TALLOC_CTX *mem_ctx,
							       WERROR *result);
NTSTATUS rpccli_spoolss_FindNextPrinterChangeNotification(struct rpc_pipe_client *cli,
							  TALLOC_CTX *mem_ctx,
							  WERROR *werror);
struct tevent_req *rpccli_spoolss_FindClosePrinterNotify_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct rpc_pipe_client *cli,
							      struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS rpccli_spoolss_FindClosePrinterNotify_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    WERROR *result);
NTSTATUS rpccli_spoolss_FindClosePrinterNotify(struct rpc_pipe_client *cli,
					       TALLOC_CTX *mem_ctx,
					       struct policy_handle *handle /* [in] [ref] */,
					       WERROR *werror);
struct tevent_req *rpccli_spoolss_RouterFindFirstPrinterChangeNotificationOld_send(TALLOC_CTX *mem_ctx,
										   struct tevent_context *ev,
										   struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_RouterFindFirstPrinterChangeNotificationOld_recv(struct tevent_req *req,
									 TALLOC_CTX *mem_ctx,
									 WERROR *result);
NTSTATUS rpccli_spoolss_RouterFindFirstPrinterChangeNotificationOld(struct rpc_pipe_client *cli,
								    TALLOC_CTX *mem_ctx,
								    WERROR *werror);
struct tevent_req *rpccli_spoolss_ReplyOpenPrinter_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							const char *_server_name /* [in] [charset(UTF16)] */,
							uint32_t _printer_local /* [in]  */,
							enum winreg_Type _type /* [in]  */,
							uint32_t _bufsize /* [in] [range(0,512)] */,
							uint8_t *_buffer /* [in] [unique,size_is(bufsize)] */,
							struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS rpccli_spoolss_ReplyOpenPrinter_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_spoolss_ReplyOpenPrinter(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 const char *server_name /* [in] [charset(UTF16)] */,
					 uint32_t printer_local /* [in]  */,
					 enum winreg_Type type /* [in]  */,
					 uint32_t bufsize /* [in] [range(0,512)] */,
					 uint8_t *buffer /* [in] [unique,size_is(bufsize)] */,
					 struct policy_handle *handle /* [out] [ref] */,
					 WERROR *werror);
struct tevent_req *rpccli_spoolss_RouterReplyPrinter_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct rpc_pipe_client *cli,
							  struct policy_handle *_handle /* [in] [ref] */,
							  uint32_t _flags /* [in]  */,
							  uint32_t _bufsize /* [in] [range(0,512)] */,
							  uint8_t *_buffer /* [in] [unique,size_is(bufsize)] */);
NTSTATUS rpccli_spoolss_RouterReplyPrinter_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS rpccli_spoolss_RouterReplyPrinter(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *handle /* [in] [ref] */,
					   uint32_t flags /* [in]  */,
					   uint32_t bufsize /* [in] [range(0,512)] */,
					   uint8_t *buffer /* [in] [unique,size_is(bufsize)] */,
					   WERROR *werror);
struct tevent_req *rpccli_spoolss_ReplyClosePrinter_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli,
							 struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS rpccli_spoolss_ReplyClosePrinter_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS rpccli_spoolss_ReplyClosePrinter(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *handle /* [in,out] [ref] */,
					  WERROR *werror);
struct tevent_req *rpccli_spoolss_AddPortEx_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_AddPortEx_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS rpccli_spoolss_AddPortEx(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  WERROR *werror);
struct tevent_req *rpccli_spoolss_RouterFindFirstPrinterChangeNotification_send(TALLOC_CTX *mem_ctx,
										struct tevent_context *ev,
										struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_RouterFindFirstPrinterChangeNotification_recv(struct tevent_req *req,
								      TALLOC_CTX *mem_ctx,
								      WERROR *result);
NTSTATUS rpccli_spoolss_RouterFindFirstPrinterChangeNotification(struct rpc_pipe_client *cli,
								 TALLOC_CTX *mem_ctx,
								 WERROR *werror);
struct tevent_req *rpccli_spoolss_SpoolerInit_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_SpoolerInit_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS rpccli_spoolss_SpoolerInit(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    WERROR *werror);
struct tevent_req *rpccli_spoolss_ResetPrinterEx_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_ResetPrinterEx_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS rpccli_spoolss_ResetPrinterEx(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       WERROR *werror);
struct tevent_req *rpccli_spoolss_RemoteFindFirstPrinterChangeNotifyEx_send(TALLOC_CTX *mem_ctx,
									    struct tevent_context *ev,
									    struct rpc_pipe_client *cli,
									    struct policy_handle *_handle /* [in] [ref] */,
									    uint32_t _flags /* [in]  */,
									    uint32_t _options /* [in]  */,
									    const char *_local_machine /* [in] [unique,charset(UTF16)] */,
									    uint32_t _printer_local /* [in]  */,
									    struct spoolss_NotifyOption *_notify_options /* [in] [unique] */);
NTSTATUS rpccli_spoolss_RemoteFindFirstPrinterChangeNotifyEx_recv(struct tevent_req *req,
								  TALLOC_CTX *mem_ctx,
								  WERROR *result);
NTSTATUS rpccli_spoolss_RemoteFindFirstPrinterChangeNotifyEx(struct rpc_pipe_client *cli,
							     TALLOC_CTX *mem_ctx,
							     struct policy_handle *handle /* [in] [ref] */,
							     uint32_t flags /* [in]  */,
							     uint32_t options /* [in]  */,
							     const char *local_machine /* [in] [unique,charset(UTF16)] */,
							     uint32_t printer_local /* [in]  */,
							     struct spoolss_NotifyOption *notify_options /* [in] [unique] */,
							     WERROR *werror);
struct tevent_req *rpccli_spoolss_RouterReplyPrinterEx_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct rpc_pipe_client *cli,
							    struct policy_handle *_handle /* [in] [ref] */,
							    uint32_t _color /* [in]  */,
							    uint32_t _flags /* [in]  */,
							    uint32_t *_reply_result /* [out] [ref] */,
							    uint32_t _reply_type /* [in]  */,
							    union spoolss_ReplyPrinterInfo _info /* [in] [switch_is(reply_type)] */);
NTSTATUS rpccli_spoolss_RouterReplyPrinterEx_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS rpccli_spoolss_RouterReplyPrinterEx(struct rpc_pipe_client *cli,
					     TALLOC_CTX *mem_ctx,
					     struct policy_handle *handle /* [in] [ref] */,
					     uint32_t color /* [in]  */,
					     uint32_t flags /* [in]  */,
					     uint32_t *reply_result /* [out] [ref] */,
					     uint32_t reply_type /* [in]  */,
					     union spoolss_ReplyPrinterInfo info /* [in] [switch_is(reply_type)] */,
					     WERROR *werror);
struct tevent_req *rpccli_spoolss_RouterRefreshPrinterChangeNotify_send(TALLOC_CTX *mem_ctx,
									struct tevent_context *ev,
									struct rpc_pipe_client *cli,
									struct policy_handle *_handle /* [in] [ref] */,
									uint32_t _change_low /* [in]  */,
									struct spoolss_NotifyOption *_options /* [in] [unique] */,
									struct spoolss_NotifyInfo **_info /* [out] [ref] */);
NTSTATUS rpccli_spoolss_RouterRefreshPrinterChangeNotify_recv(struct tevent_req *req,
							      TALLOC_CTX *mem_ctx,
							      WERROR *result);
NTSTATUS rpccli_spoolss_RouterRefreshPrinterChangeNotify(struct rpc_pipe_client *cli,
							 TALLOC_CTX *mem_ctx,
							 struct policy_handle *handle /* [in] [ref] */,
							 uint32_t change_low /* [in]  */,
							 struct spoolss_NotifyOption *options /* [in] [unique] */,
							 struct spoolss_NotifyInfo **info /* [out] [ref] */,
							 WERROR *werror);
struct tevent_req *rpccli_spoolss_44_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_44_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_44(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_OpenPrinterEx_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct rpc_pipe_client *cli,
						     const char *_printername /* [in] [unique,charset(UTF16)] */,
						     const char *_datatype /* [in] [unique,charset(UTF16)] */,
						     struct spoolss_DevmodeContainer _devmode_ctr /* [in]  */,
						     uint32_t _access_mask /* [in]  */,
						     uint32_t _level /* [in]  */,
						     union spoolss_UserLevel _userlevel /* [in] [switch_is(level)] */,
						     struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS rpccli_spoolss_OpenPrinterEx_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS rpccli_spoolss_OpenPrinterEx(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      const char *printername /* [in] [unique,charset(UTF16)] */,
				      const char *datatype /* [in] [unique,charset(UTF16)] */,
				      struct spoolss_DevmodeContainer devmode_ctr /* [in]  */,
				      uint32_t access_mask /* [in]  */,
				      uint32_t level /* [in]  */,
				      union spoolss_UserLevel userlevel /* [in] [switch_is(level)] */,
				      struct policy_handle *handle /* [out] [ref] */,
				      WERROR *werror);
struct tevent_req *rpccli_spoolss_AddPrinterEx_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    const char *_server /* [in] [unique,charset(UTF16)] */,
						    struct spoolss_SetPrinterInfoCtr *_info_ctr /* [in] [ref] */,
						    struct spoolss_DevmodeContainer *_devmode_ctr /* [in] [ref] */,
						    struct sec_desc_buf *_secdesc_ctr /* [in] [ref] */,
						    struct spoolss_UserLevelCtr *_userlevel_ctr /* [in] [ref] */,
						    struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS rpccli_spoolss_AddPrinterEx_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS rpccli_spoolss_AddPrinterEx(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     const char *server /* [in] [unique,charset(UTF16)] */,
				     struct spoolss_SetPrinterInfoCtr *info_ctr /* [in] [ref] */,
				     struct spoolss_DevmodeContainer *devmode_ctr /* [in] [ref] */,
				     struct sec_desc_buf *secdesc_ctr /* [in] [ref] */,
				     struct spoolss_UserLevelCtr *userlevel_ctr /* [in] [ref] */,
				     struct policy_handle *handle /* [out] [ref] */,
				     WERROR *werror);
struct tevent_req *rpccli_spoolss_47_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_47_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_47(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_EnumPrinterData_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct rpc_pipe_client *cli,
						       struct policy_handle *_handle /* [in] [ref] */,
						       uint32_t _enum_index /* [in]  */,
						       const char *_value_name /* [out] [charset(UTF16),size_is(value_offered/2)] */,
						       uint32_t _value_offered /* [in]  */,
						       uint32_t *_value_needed /* [out] [ref] */,
						       enum winreg_Type *_type /* [out] [ref] */,
						       uint8_t *_data /* [out] [ref,flag(LIBNDR_PRINT_ARRAY_HEX),size_is(data_offered)] */,
						       uint32_t _data_offered /* [in]  */,
						       uint32_t *_data_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_EnumPrinterData_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS rpccli_spoolss_EnumPrinterData(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *handle /* [in] [ref] */,
					uint32_t enum_index /* [in]  */,
					const char *value_name /* [out] [charset(UTF16),size_is(value_offered/2)] */,
					uint32_t value_offered /* [in]  */,
					uint32_t *value_needed /* [out] [ref] */,
					enum winreg_Type *type /* [out] [ref] */,
					uint8_t *data /* [out] [ref,flag(LIBNDR_PRINT_ARRAY_HEX),size_is(data_offered)] */,
					uint32_t data_offered /* [in]  */,
					uint32_t *data_needed /* [out] [ref] */,
					WERROR *werror);
struct tevent_req *rpccli_spoolss_DeletePrinterData_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli,
							 struct policy_handle *_handle /* [in] [ref] */,
							 const char *_value_name /* [in] [charset(UTF16)] */);
NTSTATUS rpccli_spoolss_DeletePrinterData_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS rpccli_spoolss_DeletePrinterData(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *handle /* [in] [ref] */,
					  const char *value_name /* [in] [charset(UTF16)] */,
					  WERROR *werror);
struct tevent_req *rpccli_spoolss_4a_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_4a_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_4a(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_4b_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_4b_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_4b(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_4c_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_4c_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_4c(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_SetPrinterDataEx_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							struct policy_handle *_handle /* [in] [ref] */,
							const char *_key_name /* [in] [charset(UTF16)] */,
							const char *_value_name /* [in] [charset(UTF16)] */,
							enum winreg_Type _type /* [in]  */,
							uint8_t *_data /* [in] [ref,size_is(offered)] */,
							uint32_t _offered /* [in]  */);
NTSTATUS rpccli_spoolss_SetPrinterDataEx_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_spoolss_SetPrinterDataEx(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *handle /* [in] [ref] */,
					 const char *key_name /* [in] [charset(UTF16)] */,
					 const char *value_name /* [in] [charset(UTF16)] */,
					 enum winreg_Type type /* [in]  */,
					 uint8_t *data /* [in] [ref,size_is(offered)] */,
					 uint32_t offered /* [in]  */,
					 WERROR *werror);
struct tevent_req *rpccli_spoolss_GetPrinterDataEx_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							struct policy_handle *_handle /* [in] [ref] */,
							const char *_key_name /* [in] [charset(UTF16)] */,
							const char *_value_name /* [in] [charset(UTF16)] */,
							enum winreg_Type *_type /* [out] [ref] */,
							uint8_t *_data /* [out] [ref,size_is(offered)] */,
							uint32_t _offered /* [in]  */,
							uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_GetPrinterDataEx_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_spoolss_GetPrinterDataEx(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *handle /* [in] [ref] */,
					 const char *key_name /* [in] [charset(UTF16)] */,
					 const char *value_name /* [in] [charset(UTF16)] */,
					 enum winreg_Type *type /* [out] [ref] */,
					 uint8_t *data /* [out] [ref,size_is(offered)] */,
					 uint32_t offered /* [in]  */,
					 uint32_t *needed /* [out] [ref] */,
					 WERROR *werror);
struct tevent_req *rpccli_spoolss_EnumPrinterDataEx_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct rpc_pipe_client *cli,
							 struct policy_handle *_handle /* [in] [ref] */,
							 const char *_key_name /* [in] [charset(UTF16)] */,
							 uint32_t _offered /* [in]  */,
							 uint32_t *_count /* [out] [ref] */,
							 struct spoolss_PrinterEnumValues **_info /* [out] [ref,size_is(,*count)] */,
							 uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_EnumPrinterDataEx_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS rpccli_spoolss_EnumPrinterDataEx(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *handle /* [in] [ref] */,
					  const char *key_name /* [in] [charset(UTF16)] */,
					  uint32_t offered /* [in]  */,
					  uint32_t *count /* [out] [ref] */,
					  struct spoolss_PrinterEnumValues **info /* [out] [ref,size_is(,*count)] */,
					  uint32_t *needed /* [out] [ref] */,
					  WERROR *werror);
struct tevent_req *rpccli_spoolss_EnumPrinterKey_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct rpc_pipe_client *cli,
						      struct policy_handle *_handle /* [in] [ref] */,
						      const char *_key_name /* [in] [charset(UTF16)] */,
						      uint32_t *__ndr_size /* [out] [ref] */,
						      union spoolss_KeyNames *_key_buffer /* [out] [subcontext_size(*_ndr_size*2),ref,subcontext(0),switch_is(*_ndr_size)] */,
						      uint32_t _offered /* [in]  */,
						      uint32_t *_needed /* [out] [ref] */);
NTSTATUS rpccli_spoolss_EnumPrinterKey_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS rpccli_spoolss_EnumPrinterKey(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *handle /* [in] [ref] */,
				       const char *key_name /* [in] [charset(UTF16)] */,
				       uint32_t *_ndr_size /* [out] [ref] */,
				       union spoolss_KeyNames *key_buffer /* [out] [subcontext_size(*_ndr_size*2),ref,subcontext(0),switch_is(*_ndr_size)] */,
				       uint32_t offered /* [in]  */,
				       uint32_t *needed /* [out] [ref] */,
				       WERROR *werror);
struct tevent_req *rpccli_spoolss_DeletePrinterDataEx_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct rpc_pipe_client *cli,
							   struct policy_handle *_handle /* [in] [ref] */,
							   const char *_key_name /* [in] [charset(UTF16)] */,
							   const char *_value_name /* [in] [charset(UTF16)] */);
NTSTATUS rpccli_spoolss_DeletePrinterDataEx_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS rpccli_spoolss_DeletePrinterDataEx(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    struct policy_handle *handle /* [in] [ref] */,
					    const char *key_name /* [in] [charset(UTF16)] */,
					    const char *value_name /* [in] [charset(UTF16)] */,
					    WERROR *werror);
struct tevent_req *rpccli_spoolss_DeletePrinterKey_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct rpc_pipe_client *cli,
							struct policy_handle *_handle /* [in] [ref] */,
							const char *_key_name /* [in] [charset(UTF16)] */);
NTSTATUS rpccli_spoolss_DeletePrinterKey_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS rpccli_spoolss_DeletePrinterKey(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *handle /* [in] [ref] */,
					 const char *key_name /* [in] [charset(UTF16)] */,
					 WERROR *werror);
struct tevent_req *rpccli_spoolss_53_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_53_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_53(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_DeletePrinterDriverEx_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct rpc_pipe_client *cli,
							     const char *_server /* [in] [unique,charset(UTF16)] */,
							     const char *_architecture /* [in] [charset(UTF16)] */,
							     const char *_driver /* [in] [charset(UTF16)] */,
							     uint32_t _delete_flags /* [in]  */,
							     uint32_t _version /* [in]  */);
NTSTATUS rpccli_spoolss_DeletePrinterDriverEx_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS rpccli_spoolss_DeletePrinterDriverEx(struct rpc_pipe_client *cli,
					      TALLOC_CTX *mem_ctx,
					      const char *server /* [in] [unique,charset(UTF16)] */,
					      const char *architecture /* [in] [charset(UTF16)] */,
					      const char *driver /* [in] [charset(UTF16)] */,
					      uint32_t delete_flags /* [in]  */,
					      uint32_t version /* [in]  */,
					      WERROR *werror);
struct tevent_req *rpccli_spoolss_55_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_55_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_55(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_56_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_56_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_56(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_57_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_57_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_57(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_XcvData_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       struct policy_handle *_handle /* [in] [ref] */,
					       const char *_function_name /* [in] [charset(UTF16)] */,
					       DATA_BLOB _in_data /* [in]  */,
					       uint32_t __in_data_length /* [in] [value(r->in.in_data.length)] */,
					       uint8_t *_out_data /* [out] [ref,size_is(out_data_size)] */,
					       uint32_t _out_data_size /* [in]  */,
					       uint32_t *_needed /* [out] [ref] */,
					       uint32_t *_status_code /* [in,out] [ref] */);
NTSTATUS rpccli_spoolss_XcvData_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS rpccli_spoolss_XcvData(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *handle /* [in] [ref] */,
				const char *function_name /* [in] [charset(UTF16)] */,
				DATA_BLOB in_data /* [in]  */,
				uint32_t _in_data_length /* [in] [value(r->in.in_data.length)] */,
				uint8_t *out_data /* [out] [ref,size_is(out_data_size)] */,
				uint32_t out_data_size /* [in]  */,
				uint32_t *needed /* [out] [ref] */,
				uint32_t *status_code /* [in,out] [ref] */,
				WERROR *werror);
struct tevent_req *rpccli_spoolss_AddPrinterDriverEx_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct rpc_pipe_client *cli,
							  const char *_servername /* [in] [unique,charset(UTF16)] */,
							  struct spoolss_AddDriverInfoCtr *_info_ctr /* [in] [ref] */,
							  uint32_t _flags /* [in]  */);
NTSTATUS rpccli_spoolss_AddPrinterDriverEx_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS rpccli_spoolss_AddPrinterDriverEx(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   const char *servername /* [in] [unique,charset(UTF16)] */,
					   struct spoolss_AddDriverInfoCtr *info_ctr /* [in] [ref] */,
					   uint32_t flags /* [in]  */,
					   WERROR *werror);
struct tevent_req *rpccli_spoolss_5a_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_5a_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_5a(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_5b_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_5b_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_5b(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_5c_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_5c_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_5c(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_5d_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_5d_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_5d(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_5e_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_5e_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_5e(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_5f_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_5f_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_5f(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_60_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_60_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_60(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_61_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_61_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_61(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_62_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_62_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_62(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_63_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_63_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_63(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_64_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_64_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_64(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_65_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_65_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_65(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_GetCorePrinterDrivers_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct rpc_pipe_client *cli,
							     const char *_servername /* [in] [unique,charset(UTF16)] */,
							     const char *_architecture /* [in] [ref,charset(UTF16)] */,
							     uint32_t _core_driver_size /* [in]  */,
							     const char *_core_driver_dependencies /* [in] [ref,charset(UTF16),size_is(core_driver_size)] */,
							     uint32_t _core_printer_driver_count /* [in]  */,
							     struct spoolss_CorePrinterDriver *_core_printer_drivers /* [out] [ref,size_is(core_printer_driver_count)] */);
NTSTATUS rpccli_spoolss_GetCorePrinterDrivers_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS rpccli_spoolss_GetCorePrinterDrivers(struct rpc_pipe_client *cli,
					      TALLOC_CTX *mem_ctx,
					      const char *servername /* [in] [unique,charset(UTF16)] */,
					      const char *architecture /* [in] [ref,charset(UTF16)] */,
					      uint32_t core_driver_size /* [in]  */,
					      const char *core_driver_dependencies /* [in] [ref,charset(UTF16),size_is(core_driver_size)] */,
					      uint32_t core_printer_driver_count /* [in]  */,
					      struct spoolss_CorePrinterDriver *core_printer_drivers /* [out] [ref,size_is(core_printer_driver_count)] */,
					      WERROR *werror);
struct tevent_req *rpccli_spoolss_67_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_67_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_67(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_GetPrinterDriverPackagePath_send(TALLOC_CTX *mem_ctx,
								   struct tevent_context *ev,
								   struct rpc_pipe_client *cli,
								   const char *_servername /* [in] [unique,charset(UTF16)] */,
								   const char *_architecture /* [in] [ref,charset(UTF16)] */,
								   const char *_language /* [in] [unique,charset(UTF16)] */,
								   const char *_package_id /* [in] [ref,charset(UTF16)] */,
								   const char *_driver_package_cab /* [in,out] [unique,charset(UTF16),size_is(driver_package_cab_size)] */,
								   uint32_t _driver_package_cab_size /* [in]  */,
								   uint32_t *_required /* [out] [ref] */);
NTSTATUS rpccli_spoolss_GetPrinterDriverPackagePath_recv(struct tevent_req *req,
							 TALLOC_CTX *mem_ctx,
							 WERROR *result);
NTSTATUS rpccli_spoolss_GetPrinterDriverPackagePath(struct rpc_pipe_client *cli,
						    TALLOC_CTX *mem_ctx,
						    const char *servername /* [in] [unique,charset(UTF16)] */,
						    const char *architecture /* [in] [ref,charset(UTF16)] */,
						    const char *language /* [in] [unique,charset(UTF16)] */,
						    const char *package_id /* [in] [ref,charset(UTF16)] */,
						    const char *driver_package_cab /* [in,out] [unique,charset(UTF16),size_is(driver_package_cab_size)] */,
						    uint32_t driver_package_cab_size /* [in]  */,
						    uint32_t *required /* [out] [ref] */,
						    WERROR *werror);
struct tevent_req *rpccli_spoolss_69_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_69_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_69(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_6a_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_6a_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_6a(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_6b_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_6b_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_6b(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_6c_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_6c_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_6c(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
struct tevent_req *rpccli_spoolss_6d_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct rpc_pipe_client *cli);
NTSTATUS rpccli_spoolss_6d_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS rpccli_spoolss_6d(struct rpc_pipe_client *cli,
			   TALLOC_CTX *mem_ctx,
			   WERROR *werror);
#endif /* __CLI_SPOOLSS__ */
