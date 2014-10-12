#include "librpc/gen_ndr/ndr_server_id.h"
#ifndef __SRV_SERVER_ID__
#define __SRV_SERVER_ID__
void server_id_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_server_id_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_server_id_shutdown(void);
#endif /* __SRV_SERVER_ID__ */
