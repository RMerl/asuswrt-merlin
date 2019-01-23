#include "librpc/gen_ndr/ndr_dnsserver.h"
#ifndef __SRV_DNSSERVER__
#define __SRV_DNSSERVER__
void _dnsserver_foo(struct pipes_struct *p, struct dnsserver_foo *r);
void dnsserver_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_dnsserver_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_dnsserver_shutdown(void);
#endif /* __SRV_DNSSERVER__ */
