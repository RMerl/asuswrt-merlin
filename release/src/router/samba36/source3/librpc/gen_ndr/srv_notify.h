#include "librpc/gen_ndr/ndr_notify.h"
#ifndef __SRV_NOTIFY__
#define __SRV_NOTIFY__
void notify_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_notify_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_notify_shutdown(void);
#endif /* __SRV_NOTIFY__ */
