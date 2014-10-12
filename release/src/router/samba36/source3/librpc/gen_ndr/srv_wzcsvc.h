#include "librpc/gen_ndr/ndr_wzcsvc.h"
#ifndef __SRV_WZCSVC__
#define __SRV_WZCSVC__
void _wzcsvc_EnumInterfaces(struct pipes_struct *p, struct wzcsvc_EnumInterfaces *r);
void _wzcsvc_QueryInterface(struct pipes_struct *p, struct wzcsvc_QueryInterface *r);
void _wzcsvc_SetInterface(struct pipes_struct *p, struct wzcsvc_SetInterface *r);
void _wzcsvc_RefreshInterface(struct pipes_struct *p, struct wzcsvc_RefreshInterface *r);
void _wzcsvc_QueryContext(struct pipes_struct *p, struct wzcsvc_QueryContext *r);
void _wzcsvc_SetContext(struct pipes_struct *p, struct wzcsvc_SetContext *r);
void _wzcsvc_EapolUIResponse(struct pipes_struct *p, struct wzcsvc_EapolUIResponse *r);
void _wzcsvc_EapolGetCustomAuthData(struct pipes_struct *p, struct wzcsvc_EapolGetCustomAuthData *r);
void _wzcsvc_EapolSetCustomAuthData(struct pipes_struct *p, struct wzcsvc_EapolSetCustomAuthData *r);
void _wzcsvc_EapolGetInterfaceParams(struct pipes_struct *p, struct wzcsvc_EapolGetInterfaceParams *r);
void _wzcsvc_EapolSetInterfaceParams(struct pipes_struct *p, struct wzcsvc_EapolSetInterfaceParams *r);
void _wzcsvc_EapolReAuthenticateInterface(struct pipes_struct *p, struct wzcsvc_EapolReAuthenticateInterface *r);
void _wzcsvc_EapolQueryInterfaceState(struct pipes_struct *p, struct wzcsvc_EapolQueryInterfaceState *r);
void _wzcsvc_OpenWZCDbLogSession(struct pipes_struct *p, struct wzcsvc_OpenWZCDbLogSession *r);
void _wzcsvc_CloseWZCDbLogSession(struct pipes_struct *p, struct wzcsvc_CloseWZCDbLogSession *r);
void _wzcsvc_EnumWZCDbLogRecords(struct pipes_struct *p, struct wzcsvc_EnumWZCDbLogRecords *r);
void _wzcsvc_FlushWZCdbLog(struct pipes_struct *p, struct wzcsvc_FlushWZCdbLog *r);
void _wzcsvc_GetWZCDbLogRecord(struct pipes_struct *p, struct wzcsvc_GetWZCDbLogRecord *r);
void wzcsvc_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_wzcsvc_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_wzcsvc_shutdown(void);
#endif /* __SRV_WZCSVC__ */
