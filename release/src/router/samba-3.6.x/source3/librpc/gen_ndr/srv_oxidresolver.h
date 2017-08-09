#include "librpc/gen_ndr/ndr_oxidresolver.h"
#ifndef __SRV_IOXIDRESOLVER__
#define __SRV_IOXIDRESOLVER__
WERROR _ResolveOxid(struct pipes_struct *p, struct ResolveOxid *r);
WERROR _SimplePing(struct pipes_struct *p, struct SimplePing *r);
WERROR _ComplexPing(struct pipes_struct *p, struct ComplexPing *r);
WERROR _ServerAlive(struct pipes_struct *p, struct ServerAlive *r);
WERROR _ResolveOxid2(struct pipes_struct *p, struct ResolveOxid2 *r);
WERROR _ServerAlive2(struct pipes_struct *p, struct ServerAlive2 *r);
void IOXIDResolver_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IOXIDResolver_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IOXIDResolver_shutdown(void);
#endif /* __SRV_IOXIDRESOLVER__ */
