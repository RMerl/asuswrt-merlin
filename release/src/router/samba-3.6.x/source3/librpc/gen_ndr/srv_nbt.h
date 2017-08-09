#include "librpc/gen_ndr/ndr_nbt.h"
#ifndef __SRV_NBT__
#define __SRV_NBT__
void _decode_nbt_netlogon_packet(struct pipes_struct *p, struct decode_nbt_netlogon_packet *r);
void nbt_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_nbt_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_nbt_shutdown(void);
#endif /* __SRV_NBT__ */
