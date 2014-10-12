#include "librpc/gen_ndr/ndr_preg.h"
#ifndef __SRV_PREG__
#define __SRV_PREG__
void _decode_preg_file(struct pipes_struct *p, struct decode_preg_file *r);
void preg_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_preg_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_preg_shutdown(void);
#endif /* __SRV_PREG__ */
