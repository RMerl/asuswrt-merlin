#include "librpc/gen_ndr/ndr_ntprinting.h"
#ifndef __SRV_NTPRINTING__
#define __SRV_NTPRINTING__
void _decode_ntprinting_form(struct pipes_struct *p, struct decode_ntprinting_form *r);
void _decode_ntprinting_driver(struct pipes_struct *p, struct decode_ntprinting_driver *r);
void _decode_ntprinting_printer(struct pipes_struct *p, struct decode_ntprinting_printer *r);
void ntprinting_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_ntprinting_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_ntprinting_shutdown(void);
#endif /* __SRV_NTPRINTING__ */
