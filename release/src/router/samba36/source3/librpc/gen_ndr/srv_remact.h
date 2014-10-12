#include "librpc/gen_ndr/ndr_remact.h"
#ifndef __SRV_IREMOTEACTIVATION__
#define __SRV_IREMOTEACTIVATION__
WERROR _RemoteActivation(struct pipes_struct *p, struct RemoteActivation *r);
void IRemoteActivation_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IRemoteActivation_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IRemoteActivation_shutdown(void);
#endif /* __SRV_IREMOTEACTIVATION__ */
