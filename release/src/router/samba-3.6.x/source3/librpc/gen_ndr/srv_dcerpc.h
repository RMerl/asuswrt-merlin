#include "librpc/gen_ndr/ndr_dcerpc.h"
#ifndef __SRV_DCERPC__
#define __SRV_DCERPC__
void dcerpc_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_dcerpc_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_dcerpc_shutdown(void);
#endif /* __SRV_DCERPC__ */
