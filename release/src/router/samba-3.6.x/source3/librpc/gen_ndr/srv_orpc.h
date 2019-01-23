#include "librpc/gen_ndr/ndr_orpc.h"
#ifndef __SRV_OBJECTRPCBASETYPES__
#define __SRV_OBJECTRPCBASETYPES__
void ObjectRpcBaseTypes_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_ObjectRpcBaseTypes_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_ObjectRpcBaseTypes_shutdown(void);
#endif /* __SRV_OBJECTRPCBASETYPES__ */
