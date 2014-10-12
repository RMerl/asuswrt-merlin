#include "librpc/gen_ndr/ndr_backupkey.h"
#ifndef __SRV_BACKUPKEY__
#define __SRV_BACKUPKEY__
WERROR _bkrp_BackupKey(struct pipes_struct *p, struct bkrp_BackupKey *r);
void backupkey_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_backupkey_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_backupkey_shutdown(void);
#endif /* __SRV_BACKUPKEY__ */
