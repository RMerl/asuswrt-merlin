#include "librpc/gen_ndr/ndr_dfsblobs.h"
#ifndef __SRV_DFSBLOBS__
#define __SRV_DFSBLOBS__
void _dfs_GetDFSReferral(struct pipes_struct *p, struct dfs_GetDFSReferral *r);
void dfsblobs_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_dfsblobs_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_dfsblobs_shutdown(void);
#endif /* __SRV_DFSBLOBS__ */
