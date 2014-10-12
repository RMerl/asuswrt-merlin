#include "librpc/gen_ndr/ndr_dsbackup.h"
#ifndef __SRV_AD_BACKUP__
#define __SRV_AD_BACKUP__
void _HrRBackupPrepare(struct pipes_struct *p, struct HrRBackupPrepare *r);
void _HrRBackupEnd(struct pipes_struct *p, struct HrRBackupEnd *r);
void _HrRBackupGetAttachmentInformation(struct pipes_struct *p, struct HrRBackupGetAttachmentInformation *r);
void _HrRBackupOpenFile(struct pipes_struct *p, struct HrRBackupOpenFile *r);
void _HrRBackupRead(struct pipes_struct *p, struct HrRBackupRead *r);
void _HrRBackupClose(struct pipes_struct *p, struct HrRBackupClose *r);
void _HrRBackupGetBackupLogs(struct pipes_struct *p, struct HrRBackupGetBackupLogs *r);
void _HrRBackupTruncateLogs(struct pipes_struct *p, struct HrRBackupTruncateLogs *r);
void _HrRBackupPing(struct pipes_struct *p, struct HrRBackupPing *r);
void ad_backup_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_ad_backup_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_ad_backup_shutdown(void);
#endif /* __SRV_AD_BACKUP__ */
#ifndef __SRV_AD_RESTORE__
#define __SRV_AD_RESTORE__
void _HrRIsNTDSOnline(struct pipes_struct *p, struct HrRIsNTDSOnline *r);
void _HrRRestorePrepare(struct pipes_struct *p, struct HrRRestorePrepare *r);
void _HrRRestoreRegister(struct pipes_struct *p, struct HrRRestoreRegister *r);
void _HrRRestoreRegisterComplete(struct pipes_struct *p, struct HrRRestoreRegisterComplete *r);
void _HrRRestoreGetDatabaseLocations(struct pipes_struct *p, struct HrRRestoreGetDatabaseLocations *r);
void _HrRRestoreEnd(struct pipes_struct *p, struct HrRRestoreEnd *r);
void _HrRRestoreSetCurrentLogNumber(struct pipes_struct *p, struct HrRRestoreSetCurrentLogNumber *r);
void _HrRRestoreCheckLogsForBackup(struct pipes_struct *p, struct HrRRestoreCheckLogsForBackup *r);
void ad_restore_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_ad_restore_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_ad_restore_shutdown(void);
#endif /* __SRV_AD_RESTORE__ */
