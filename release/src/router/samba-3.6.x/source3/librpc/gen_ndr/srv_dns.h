#include "librpc/gen_ndr/ndr_dns.h"
#ifndef __SRV_DNS__
#define __SRV_DNS__
void _decode_dns_name_packet(struct pipes_struct *p, struct decode_dns_name_packet *r);
void dns_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_dns_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_dns_shutdown(void);
#endif /* __SRV_DNS__ */
