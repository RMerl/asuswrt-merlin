#include "librpc/gen_ndr/ndr_dnsp.h"
#ifndef __SRV_DNSP__
#define __SRV_DNSP__
void _decode_DnssrvRpcRecord(struct pipes_struct *p, struct decode_DnssrvRpcRecord *r);
void dnsp_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_dnsp_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_dnsp_shutdown(void);
#endif /* __SRV_DNSP__ */
