#include "../librpc/gen_ndr/ndr_initshutdown.h"
#ifndef __SRV_INITSHUTDOWN__
#define __SRV_INITSHUTDOWN__
WERROR _initshutdown_Init(pipes_struct *p, struct initshutdown_Init *r);
WERROR _initshutdown_Abort(pipes_struct *p, struct initshutdown_Abort *r);
WERROR _initshutdown_InitEx(pipes_struct *p, struct initshutdown_InitEx *r);
void initshutdown_get_pipe_fns(struct api_struct **fns, int *n_fns);
NTSTATUS rpc_initshutdown_dispatch(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, const struct ndr_interface_table *table, uint32_t opnum, void *r);
WERROR _initshutdown_Init(pipes_struct *p, struct initshutdown_Init *r);
WERROR _initshutdown_Abort(pipes_struct *p, struct initshutdown_Abort *r);
WERROR _initshutdown_InitEx(pipes_struct *p, struct initshutdown_InitEx *r);
NTSTATUS rpc_initshutdown_init(void);
#endif /* __SRV_INITSHUTDOWN__ */
