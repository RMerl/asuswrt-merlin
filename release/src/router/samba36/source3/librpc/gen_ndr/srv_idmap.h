#include "librpc/gen_ndr/ndr_idmap.h"
#ifndef __SRV_IDMAP__
#define __SRV_IDMAP__
void idmap_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_idmap_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_idmap_shutdown(void);
#endif /* __SRV_IDMAP__ */
