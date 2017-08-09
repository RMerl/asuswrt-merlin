#include "librpc/gen_ndr/ndr_xattr.h"
#ifndef __SRV_XATTR__
#define __SRV_XATTR__
void _xattr_parse_DOSATTRIB(struct pipes_struct *p, struct xattr_parse_DOSATTRIB *r);
void xattr_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_xattr_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_xattr_shutdown(void);
#endif /* __SRV_XATTR__ */
