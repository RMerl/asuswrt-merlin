#include "librpc/gen_ndr/ndr_policyagent.h"
#ifndef __SRV_POLICYAGENT__
#define __SRV_POLICYAGENT__
WERROR _policyagent_Dummy(struct pipes_struct *p, struct policyagent_Dummy *r);
void policyagent_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_policyagent_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_policyagent_shutdown(void);
#endif /* __SRV_POLICYAGENT__ */
