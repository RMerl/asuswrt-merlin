#include "librpc/gen_ndr/ndr_messaging.h"
#ifndef __SRV_MESSAGING__
#define __SRV_MESSAGING__
void messaging_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_messaging_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_messaging_shutdown(void);
#endif /* __SRV_MESSAGING__ */
