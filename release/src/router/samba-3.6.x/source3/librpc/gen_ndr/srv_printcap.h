#include "librpc/gen_ndr/ndr_printcap.h"
#ifndef __SRV_PRINTCAP__
#define __SRV_PRINTCAP__
void printcap_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_printcap_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_printcap_shutdown(void);
#endif /* __SRV_PRINTCAP__ */
