#include "librpc/gen_ndr/ndr_perfcount.h"
#ifndef __SRV_PERFCOUNT__
#define __SRV_PERFCOUNT__
void perfcount_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_perfcount_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_perfcount_shutdown(void);
#endif /* __SRV_PERFCOUNT__ */
