#include "librpc/gen_ndr/ndr_rap.h"
#ifndef __SRV_RAP__
#define __SRV_RAP__
void _rap_NetShareEnum(struct pipes_struct *p, struct rap_NetShareEnum *r);
void _rap_NetShareAdd(struct pipes_struct *p, struct rap_NetShareAdd *r);
void _rap_NetServerEnum2(struct pipes_struct *p, struct rap_NetServerEnum2 *r);
void _rap_WserverGetInfo(struct pipes_struct *p, struct rap_WserverGetInfo *r);
void _rap_NetPrintQEnum(struct pipes_struct *p, struct rap_NetPrintQEnum *r);
void _rap_NetPrintQGetInfo(struct pipes_struct *p, struct rap_NetPrintQGetInfo *r);
void _rap_NetPrintJobPause(struct pipes_struct *p, struct rap_NetPrintJobPause *r);
void _rap_NetPrintJobContinue(struct pipes_struct *p, struct rap_NetPrintJobContinue *r);
void _rap_NetPrintJobDelete(struct pipes_struct *p, struct rap_NetPrintJobDelete *r);
void _rap_NetPrintQueuePause(struct pipes_struct *p, struct rap_NetPrintQueuePause *r);
void _rap_NetPrintQueueResume(struct pipes_struct *p, struct rap_NetPrintQueueResume *r);
void _rap_NetPrintQueuePurge(struct pipes_struct *p, struct rap_NetPrintQueuePurge *r);
void _rap_NetPrintJobEnum(struct pipes_struct *p, struct rap_NetPrintJobEnum *r);
void _rap_NetPrintJobGetInfo(struct pipes_struct *p, struct rap_NetPrintJobGetInfo *r);
void _rap_NetPrintJobSetInfo(struct pipes_struct *p, struct rap_NetPrintJobSetInfo *r);
void _rap_NetPrintDestEnum(struct pipes_struct *p, struct rap_NetPrintDestEnum *r);
void _rap_NetPrintDestGetInfo(struct pipes_struct *p, struct rap_NetPrintDestGetInfo *r);
void _rap_NetUserPasswordSet2(struct pipes_struct *p, struct rap_NetUserPasswordSet2 *r);
void _rap_NetOEMChangePassword(struct pipes_struct *p, struct rap_NetOEMChangePassword *r);
void _rap_NetUserGetInfo(struct pipes_struct *p, struct rap_NetUserGetInfo *r);
void _rap_NetSessionEnum(struct pipes_struct *p, struct rap_NetSessionEnum *r);
void _rap_NetSessionGetInfo(struct pipes_struct *p, struct rap_NetSessionGetInfo *r);
void _rap_NetUserAdd(struct pipes_struct *p, struct rap_NetUserAdd *r);
void _rap_NetUserDelete(struct pipes_struct *p, struct rap_NetUserDelete *r);
void _rap_NetRemoteTOD(struct pipes_struct *p, struct rap_NetRemoteTOD *r);
void rap_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_rap_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_rap_shutdown(void);
#endif /* __SRV_RAP__ */
