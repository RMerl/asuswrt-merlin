#include "librpc/gen_ndr/ndr_initshutdown.h"
#ifndef __SRV_INITSHUTDOWN__
#define __SRV_INITSHUTDOWN__
WERROR _initshutdown_Init(struct pipes_struct *p, struct initshutdown_Init *r);
WERROR _initshutdown_Abort(struct pipes_struct *p, struct initshutdown_Abort *r);
WERROR _initshutdown_InitEx(struct pipes_struct *p, struct initshutdown_InitEx *r);
void initshutdown_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_initshutdown_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_initshutdown_shutdown(void);
#endif /* __SRV_INITSHUTDOWN__ */
