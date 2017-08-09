#include "librpc/gen_ndr/ndr_w32time.h"
#ifndef __SRV_W32TIME__
#define __SRV_W32TIME__
WERROR _w32time_SyncTime(struct pipes_struct *p, struct w32time_SyncTime *r);
WERROR _w32time_GetNetLogonServiceBits(struct pipes_struct *p, struct w32time_GetNetLogonServiceBits *r);
WERROR _w32time_QueryProviderStatus(struct pipes_struct *p, struct w32time_QueryProviderStatus *r);
void w32time_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_w32time_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_w32time_shutdown(void);
#endif /* __SRV_W32TIME__ */
