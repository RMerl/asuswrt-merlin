#include "librpc/gen_ndr/ndr_keysvc.h"
#ifndef __SRV_KEYSVC__
#define __SRV_KEYSVC__
WERROR _keysvc_Unknown0(struct pipes_struct *p, struct keysvc_Unknown0 *r);
void keysvc_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_keysvc_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_keysvc_shutdown(void);
#endif /* __SRV_KEYSVC__ */
