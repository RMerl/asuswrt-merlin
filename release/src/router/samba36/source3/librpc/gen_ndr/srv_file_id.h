#include "librpc/gen_ndr/ndr_file_id.h"
#ifndef __SRV_FILE_ID__
#define __SRV_FILE_ID__
void file_id_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_file_id_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_file_id_shutdown(void);
#endif /* __SRV_FILE_ID__ */
