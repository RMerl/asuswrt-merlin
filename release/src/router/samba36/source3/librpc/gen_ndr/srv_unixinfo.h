#include "librpc/gen_ndr/ndr_unixinfo.h"
#ifndef __SRV_UNIXINFO__
#define __SRV_UNIXINFO__
NTSTATUS _unixinfo_SidToUid(struct pipes_struct *p, struct unixinfo_SidToUid *r);
NTSTATUS _unixinfo_UidToSid(struct pipes_struct *p, struct unixinfo_UidToSid *r);
NTSTATUS _unixinfo_SidToGid(struct pipes_struct *p, struct unixinfo_SidToGid *r);
NTSTATUS _unixinfo_GidToSid(struct pipes_struct *p, struct unixinfo_GidToSid *r);
NTSTATUS _unixinfo_GetPWUid(struct pipes_struct *p, struct unixinfo_GetPWUid *r);
void unixinfo_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_unixinfo_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_unixinfo_shutdown(void);
#endif /* __SRV_UNIXINFO__ */
