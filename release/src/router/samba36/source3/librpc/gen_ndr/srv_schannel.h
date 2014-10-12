#include "librpc/gen_ndr/ndr_schannel.h"
#ifndef __SRV_SCHANNEL__
#define __SRV_SCHANNEL__
void schannel_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_schannel_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_schannel_shutdown(void);
#endif /* __SRV_SCHANNEL__ */
