#include "librpc/gen_ndr/ndr_security.h"
#ifndef __SRV_SECURITY__
#define __SRV_SECURITY__
void security_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_security_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_security_shutdown(void);
#endif /* __SRV_SECURITY__ */
