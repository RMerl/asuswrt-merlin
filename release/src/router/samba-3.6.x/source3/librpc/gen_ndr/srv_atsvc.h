#include "librpc/gen_ndr/ndr_atsvc.h"
#ifndef __SRV_ATSVC__
#define __SRV_ATSVC__
NTSTATUS _atsvc_JobAdd(struct pipes_struct *p, struct atsvc_JobAdd *r);
NTSTATUS _atsvc_JobDel(struct pipes_struct *p, struct atsvc_JobDel *r);
NTSTATUS _atsvc_JobEnum(struct pipes_struct *p, struct atsvc_JobEnum *r);
NTSTATUS _atsvc_JobGetInfo(struct pipes_struct *p, struct atsvc_JobGetInfo *r);
void atsvc_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_atsvc_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_atsvc_shutdown(void);
#endif /* __SRV_ATSVC__ */
