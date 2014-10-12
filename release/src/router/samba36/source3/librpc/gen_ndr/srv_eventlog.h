#include "librpc/gen_ndr/ndr_eventlog.h"
#ifndef __SRV_EVENTLOG__
#define __SRV_EVENTLOG__
NTSTATUS _eventlog_ClearEventLogW(struct pipes_struct *p, struct eventlog_ClearEventLogW *r);
NTSTATUS _eventlog_BackupEventLogW(struct pipes_struct *p, struct eventlog_BackupEventLogW *r);
NTSTATUS _eventlog_CloseEventLog(struct pipes_struct *p, struct eventlog_CloseEventLog *r);
NTSTATUS _eventlog_DeregisterEventSource(struct pipes_struct *p, struct eventlog_DeregisterEventSource *r);
NTSTATUS _eventlog_GetNumRecords(struct pipes_struct *p, struct eventlog_GetNumRecords *r);
NTSTATUS _eventlog_GetOldestRecord(struct pipes_struct *p, struct eventlog_GetOldestRecord *r);
NTSTATUS _eventlog_ChangeNotify(struct pipes_struct *p, struct eventlog_ChangeNotify *r);
NTSTATUS _eventlog_OpenEventLogW(struct pipes_struct *p, struct eventlog_OpenEventLogW *r);
NTSTATUS _eventlog_RegisterEventSourceW(struct pipes_struct *p, struct eventlog_RegisterEventSourceW *r);
NTSTATUS _eventlog_OpenBackupEventLogW(struct pipes_struct *p, struct eventlog_OpenBackupEventLogW *r);
NTSTATUS _eventlog_ReadEventLogW(struct pipes_struct *p, struct eventlog_ReadEventLogW *r);
NTSTATUS _eventlog_ReportEventW(struct pipes_struct *p, struct eventlog_ReportEventW *r);
NTSTATUS _eventlog_ClearEventLogA(struct pipes_struct *p, struct eventlog_ClearEventLogA *r);
NTSTATUS _eventlog_BackupEventLogA(struct pipes_struct *p, struct eventlog_BackupEventLogA *r);
NTSTATUS _eventlog_OpenEventLogA(struct pipes_struct *p, struct eventlog_OpenEventLogA *r);
NTSTATUS _eventlog_RegisterEventSourceA(struct pipes_struct *p, struct eventlog_RegisterEventSourceA *r);
NTSTATUS _eventlog_OpenBackupEventLogA(struct pipes_struct *p, struct eventlog_OpenBackupEventLogA *r);
NTSTATUS _eventlog_ReadEventLogA(struct pipes_struct *p, struct eventlog_ReadEventLogA *r);
NTSTATUS _eventlog_ReportEventA(struct pipes_struct *p, struct eventlog_ReportEventA *r);
NTSTATUS _eventlog_RegisterClusterSvc(struct pipes_struct *p, struct eventlog_RegisterClusterSvc *r);
NTSTATUS _eventlog_DeregisterClusterSvc(struct pipes_struct *p, struct eventlog_DeregisterClusterSvc *r);
NTSTATUS _eventlog_WriteClusterEvents(struct pipes_struct *p, struct eventlog_WriteClusterEvents *r);
NTSTATUS _eventlog_GetLogInformation(struct pipes_struct *p, struct eventlog_GetLogInformation *r);
NTSTATUS _eventlog_FlushEventLog(struct pipes_struct *p, struct eventlog_FlushEventLog *r);
NTSTATUS _eventlog_ReportEventAndSourceW(struct pipes_struct *p, struct eventlog_ReportEventAndSourceW *r);
void eventlog_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_eventlog_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_eventlog_shutdown(void);
#endif /* __SRV_EVENTLOG__ */
