#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/eventlog.h"
#ifndef _HEADER_RPC_eventlog
#define _HEADER_RPC_eventlog

extern const struct ndr_interface_table ndr_table_eventlog;

struct tevent_req *dcerpc_eventlog_ClearEventLogW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog_ClearEventLogW *r);
NTSTATUS dcerpc_eventlog_ClearEventLogW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog_ClearEventLogW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog_ClearEventLogW *r);
struct tevent_req *dcerpc_eventlog_ClearEventLogW_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct policy_handle *_handle /* [in] [ref] */,
						       struct lsa_String *_backupfile /* [in] [unique] */);
NTSTATUS dcerpc_eventlog_ClearEventLogW_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     NTSTATUS *result);
NTSTATUS dcerpc_eventlog_ClearEventLogW(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct policy_handle *_handle /* [in] [ref] */,
					struct lsa_String *_backupfile /* [in] [unique] */,
					NTSTATUS *result);

struct tevent_req *dcerpc_eventlog_BackupEventLogW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog_BackupEventLogW *r);
NTSTATUS dcerpc_eventlog_BackupEventLogW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog_BackupEventLogW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog_BackupEventLogW *r);
struct tevent_req *dcerpc_eventlog_BackupEventLogW_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_handle /* [in] [ref] */,
							struct lsa_String *_backup_filename /* [in] [ref] */);
NTSTATUS dcerpc_eventlog_BackupEventLogW_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS dcerpc_eventlog_BackupEventLogW(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_handle /* [in] [ref] */,
					 struct lsa_String *_backup_filename /* [in] [ref] */,
					 NTSTATUS *result);

struct tevent_req *dcerpc_eventlog_CloseEventLog_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog_CloseEventLog *r);
NTSTATUS dcerpc_eventlog_CloseEventLog_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog_CloseEventLog_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog_CloseEventLog *r);
struct tevent_req *dcerpc_eventlog_CloseEventLog_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_eventlog_CloseEventLog_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_eventlog_CloseEventLog(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_handle /* [in,out] [ref] */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_eventlog_DeregisterEventSource_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog_DeregisterEventSource *r);
NTSTATUS dcerpc_eventlog_DeregisterEventSource_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog_DeregisterEventSource_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog_DeregisterEventSource *r);
struct tevent_req *dcerpc_eventlog_DeregisterEventSource_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      struct policy_handle *_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_eventlog_DeregisterEventSource_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    NTSTATUS *result);
NTSTATUS dcerpc_eventlog_DeregisterEventSource(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       struct policy_handle *_handle /* [in,out] [ref] */,
					       NTSTATUS *result);

struct tevent_req *dcerpc_eventlog_GetNumRecords_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog_GetNumRecords *r);
NTSTATUS dcerpc_eventlog_GetNumRecords_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog_GetNumRecords_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog_GetNumRecords *r);
struct tevent_req *dcerpc_eventlog_GetNumRecords_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_handle /* [in] [ref] */,
						      uint32_t *_number /* [out] [ref] */);
NTSTATUS dcerpc_eventlog_GetNumRecords_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_eventlog_GetNumRecords(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_handle /* [in] [ref] */,
				       uint32_t *_number /* [out] [ref] */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_eventlog_GetOldestRecord_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog_GetOldestRecord *r);
NTSTATUS dcerpc_eventlog_GetOldestRecord_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog_GetOldestRecord_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog_GetOldestRecord *r);
struct tevent_req *dcerpc_eventlog_GetOldestRecord_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_handle /* [in] [ref] */,
							uint32_t *_oldest_entry /* [out] [ref] */);
NTSTATUS dcerpc_eventlog_GetOldestRecord_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      NTSTATUS *result);
NTSTATUS dcerpc_eventlog_GetOldestRecord(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_handle /* [in] [ref] */,
					 uint32_t *_oldest_entry /* [out] [ref] */,
					 NTSTATUS *result);

struct tevent_req *dcerpc_eventlog_OpenEventLogW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog_OpenEventLogW *r);
NTSTATUS dcerpc_eventlog_OpenEventLogW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog_OpenEventLogW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog_OpenEventLogW *r);
struct tevent_req *dcerpc_eventlog_OpenEventLogW_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct eventlog_OpenUnknown0 *_unknown0 /* [in] [unique] */,
						      struct lsa_String *_logname /* [in] [ref] */,
						      struct lsa_String *_servername /* [in] [ref] */,
						      uint32_t _major_version /* [in]  */,
						      uint32_t _minor_version /* [in]  */,
						      struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_eventlog_OpenEventLogW_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_eventlog_OpenEventLogW(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct eventlog_OpenUnknown0 *_unknown0 /* [in] [unique] */,
				       struct lsa_String *_logname /* [in] [ref] */,
				       struct lsa_String *_servername /* [in] [ref] */,
				       uint32_t _major_version /* [in]  */,
				       uint32_t _minor_version /* [in]  */,
				       struct policy_handle *_handle /* [out] [ref] */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_eventlog_RegisterEventSourceW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog_RegisterEventSourceW *r);
NTSTATUS dcerpc_eventlog_RegisterEventSourceW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog_RegisterEventSourceW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog_RegisterEventSourceW *r);
struct tevent_req *dcerpc_eventlog_RegisterEventSourceW_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     struct eventlog_OpenUnknown0 *_unknown0 /* [in] [unique] */,
							     struct lsa_String *_module_name /* [in] [ref] */,
							     struct lsa_String *_reg_module_name /* [in] [ref] */,
							     uint32_t _major_version /* [in]  */,
							     uint32_t _minor_version /* [in]  */,
							     struct policy_handle *_log_handle /* [out] [ref] */);
NTSTATUS dcerpc_eventlog_RegisterEventSourceW_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   NTSTATUS *result);
NTSTATUS dcerpc_eventlog_RegisterEventSourceW(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      struct eventlog_OpenUnknown0 *_unknown0 /* [in] [unique] */,
					      struct lsa_String *_module_name /* [in] [ref] */,
					      struct lsa_String *_reg_module_name /* [in] [ref] */,
					      uint32_t _major_version /* [in]  */,
					      uint32_t _minor_version /* [in]  */,
					      struct policy_handle *_log_handle /* [out] [ref] */,
					      NTSTATUS *result);

struct tevent_req *dcerpc_eventlog_OpenBackupEventLogW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog_OpenBackupEventLogW *r);
NTSTATUS dcerpc_eventlog_OpenBackupEventLogW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog_OpenBackupEventLogW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog_OpenBackupEventLogW *r);
struct tevent_req *dcerpc_eventlog_OpenBackupEventLogW_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct dcerpc_binding_handle *h,
							    struct eventlog_OpenUnknown0 *_unknown0 /* [in] [unique] */,
							    struct lsa_String *_backup_logname /* [in] [ref] */,
							    uint32_t _major_version /* [in]  */,
							    uint32_t _minor_version /* [in]  */,
							    struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_eventlog_OpenBackupEventLogW_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  NTSTATUS *result);
NTSTATUS dcerpc_eventlog_OpenBackupEventLogW(struct dcerpc_binding_handle *h,
					     TALLOC_CTX *mem_ctx,
					     struct eventlog_OpenUnknown0 *_unknown0 /* [in] [unique] */,
					     struct lsa_String *_backup_logname /* [in] [ref] */,
					     uint32_t _major_version /* [in]  */,
					     uint32_t _minor_version /* [in]  */,
					     struct policy_handle *_handle /* [out] [ref] */,
					     NTSTATUS *result);

struct tevent_req *dcerpc_eventlog_ReadEventLogW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog_ReadEventLogW *r);
NTSTATUS dcerpc_eventlog_ReadEventLogW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog_ReadEventLogW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog_ReadEventLogW *r);
struct tevent_req *dcerpc_eventlog_ReadEventLogW_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_handle /* [in] [ref] */,
						      uint32_t _flags /* [in]  */,
						      uint32_t _offset /* [in]  */,
						      uint32_t _number_of_bytes /* [in] [range(0,0x7FFFF)] */,
						      uint8_t *_data /* [out] [size_is(number_of_bytes),ref] */,
						      uint32_t *_sent_size /* [out] [ref] */,
						      uint32_t *_real_size /* [out] [ref] */);
NTSTATUS dcerpc_eventlog_ReadEventLogW_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_eventlog_ReadEventLogW(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_handle /* [in] [ref] */,
				       uint32_t _flags /* [in]  */,
				       uint32_t _offset /* [in]  */,
				       uint32_t _number_of_bytes /* [in] [range(0,0x7FFFF)] */,
				       uint8_t *_data /* [out] [size_is(number_of_bytes),ref] */,
				       uint32_t *_sent_size /* [out] [ref] */,
				       uint32_t *_real_size /* [out] [ref] */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_eventlog_ReportEventW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog_ReportEventW *r);
NTSTATUS dcerpc_eventlog_ReportEventW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog_ReportEventW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog_ReportEventW *r);
struct tevent_req *dcerpc_eventlog_ReportEventW_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle *_handle /* [in] [ref] */,
						     time_t _timestamp /* [in]  */,
						     enum eventlogEventTypes _event_type /* [in]  */,
						     uint16_t _event_category /* [in]  */,
						     uint32_t _event_id /* [in]  */,
						     uint16_t _num_of_strings /* [in] [range(0,256)] */,
						     uint32_t _data_size /* [in] [range(0,0x3FFFF)] */,
						     struct lsa_String *_servername /* [in] [ref] */,
						     struct dom_sid *_user_sid /* [in] [unique] */,
						     struct lsa_String **_strings /* [in] [size_is(num_of_strings),unique] */,
						     uint8_t *_data /* [in] [size_is(data_size),unique] */,
						     uint16_t _flags /* [in]  */,
						     uint32_t *_record_number /* [in,out] [unique] */,
						     time_t *_time_written /* [in,out] [unique] */);
NTSTATUS dcerpc_eventlog_ReportEventW_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   NTSTATUS *result);
NTSTATUS dcerpc_eventlog_ReportEventW(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle *_handle /* [in] [ref] */,
				      time_t _timestamp /* [in]  */,
				      enum eventlogEventTypes _event_type /* [in]  */,
				      uint16_t _event_category /* [in]  */,
				      uint32_t _event_id /* [in]  */,
				      uint16_t _num_of_strings /* [in] [range(0,256)] */,
				      uint32_t _data_size /* [in] [range(0,0x3FFFF)] */,
				      struct lsa_String *_servername /* [in] [ref] */,
				      struct dom_sid *_user_sid /* [in] [unique] */,
				      struct lsa_String **_strings /* [in] [size_is(num_of_strings),unique] */,
				      uint8_t *_data /* [in] [size_is(data_size),unique] */,
				      uint16_t _flags /* [in]  */,
				      uint32_t *_record_number /* [in,out] [unique] */,
				      time_t *_time_written /* [in,out] [unique] */,
				      NTSTATUS *result);

struct tevent_req *dcerpc_eventlog_GetLogInformation_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog_GetLogInformation *r);
NTSTATUS dcerpc_eventlog_GetLogInformation_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog_GetLogInformation_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog_GetLogInformation *r);
struct tevent_req *dcerpc_eventlog_GetLogInformation_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct policy_handle *_handle /* [in] [ref] */,
							  uint32_t _level /* [in]  */,
							  uint8_t *_buffer /* [out] [ref,size_is(buf_size)] */,
							  uint32_t _buf_size /* [in] [range(0,1024)] */,
							  uint32_t *_bytes_needed /* [out] [ref] */);
NTSTATUS dcerpc_eventlog_GetLogInformation_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						NTSTATUS *result);
NTSTATUS dcerpc_eventlog_GetLogInformation(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct policy_handle *_handle /* [in] [ref] */,
					   uint32_t _level /* [in]  */,
					   uint8_t *_buffer /* [out] [ref,size_is(buf_size)] */,
					   uint32_t _buf_size /* [in] [range(0,1024)] */,
					   uint32_t *_bytes_needed /* [out] [ref] */,
					   NTSTATUS *result);

struct tevent_req *dcerpc_eventlog_FlushEventLog_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog_FlushEventLog *r);
NTSTATUS dcerpc_eventlog_FlushEventLog_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog_FlushEventLog_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog_FlushEventLog *r);
struct tevent_req *dcerpc_eventlog_FlushEventLog_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS dcerpc_eventlog_FlushEventLog_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    NTSTATUS *result);
NTSTATUS dcerpc_eventlog_FlushEventLog(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_handle /* [in] [ref] */,
				       NTSTATUS *result);

struct tevent_req *dcerpc_eventlog_ReportEventAndSourceW_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog_ReportEventAndSourceW *r);
NTSTATUS dcerpc_eventlog_ReportEventAndSourceW_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog_ReportEventAndSourceW_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog_ReportEventAndSourceW *r);
struct tevent_req *dcerpc_eventlog_ReportEventAndSourceW_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      struct policy_handle *_handle /* [in] [ref] */,
							      time_t _timestamp /* [in]  */,
							      enum eventlogEventTypes _event_type /* [in]  */,
							      uint16_t _event_category /* [in]  */,
							      uint32_t _event_id /* [in]  */,
							      struct lsa_String *_sourcename /* [in] [ref] */,
							      uint16_t _num_of_strings /* [in] [range(0,256)] */,
							      uint32_t _data_size /* [in] [range(0,0x3FFFF)] */,
							      struct lsa_String *_servername /* [in] [ref] */,
							      struct dom_sid *_user_sid /* [in] [unique] */,
							      struct lsa_String **_strings /* [in] [size_is(num_of_strings),unique] */,
							      uint8_t *_data /* [in] [size_is(data_size),unique] */,
							      uint16_t _flags /* [in]  */,
							      uint32_t *_record_number /* [in,out] [unique] */,
							      time_t *_time_written /* [in,out] [unique] */);
NTSTATUS dcerpc_eventlog_ReportEventAndSourceW_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    NTSTATUS *result);
NTSTATUS dcerpc_eventlog_ReportEventAndSourceW(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       struct policy_handle *_handle /* [in] [ref] */,
					       time_t _timestamp /* [in]  */,
					       enum eventlogEventTypes _event_type /* [in]  */,
					       uint16_t _event_category /* [in]  */,
					       uint32_t _event_id /* [in]  */,
					       struct lsa_String *_sourcename /* [in] [ref] */,
					       uint16_t _num_of_strings /* [in] [range(0,256)] */,
					       uint32_t _data_size /* [in] [range(0,0x3FFFF)] */,
					       struct lsa_String *_servername /* [in] [ref] */,
					       struct dom_sid *_user_sid /* [in] [unique] */,
					       struct lsa_String **_strings /* [in] [size_is(num_of_strings),unique] */,
					       uint8_t *_data /* [in] [size_is(data_size),unique] */,
					       uint16_t _flags /* [in]  */,
					       uint32_t *_record_number /* [in,out] [unique] */,
					       time_t *_time_written /* [in,out] [unique] */,
					       NTSTATUS *result);

#endif /* _HEADER_RPC_eventlog */
