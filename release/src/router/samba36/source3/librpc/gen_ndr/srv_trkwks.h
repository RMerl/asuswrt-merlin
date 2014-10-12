#include "librpc/gen_ndr/ndr_trkwks.h"
#ifndef __SRV_TRKWKS__
#define __SRV_TRKWKS__
WERROR _trkwks_Unknown0(struct pipes_struct *p, struct trkwks_Unknown0 *r);
void trkwks_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_trkwks_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_trkwks_shutdown(void);
#endif /* __SRV_TRKWKS__ */
