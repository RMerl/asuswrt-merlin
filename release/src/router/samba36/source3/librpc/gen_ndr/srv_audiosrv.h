#include "librpc/gen_ndr/ndr_audiosrv.h"
#ifndef __SRV_AUDIOSRV__
#define __SRV_AUDIOSRV__
void _audiosrv_CreatezoneFactoriesList(struct pipes_struct *p, struct audiosrv_CreatezoneFactoriesList *r);
void _audiosrv_CreateGfxFactoriesList(struct pipes_struct *p, struct audiosrv_CreateGfxFactoriesList *r);
void _audiosrv_CreateGfxList(struct pipes_struct *p, struct audiosrv_CreateGfxList *r);
void _audiosrv_RemoveGfx(struct pipes_struct *p, struct audiosrv_RemoveGfx *r);
void _audiosrv_AddGfx(struct pipes_struct *p, struct audiosrv_AddGfx *r);
void _audiosrv_ModifyGfx(struct pipes_struct *p, struct audiosrv_ModifyGfx *r);
void _audiosrv_OpenGfx(struct pipes_struct *p, struct audiosrv_OpenGfx *r);
void _audiosrv_Logon(struct pipes_struct *p, struct audiosrv_Logon *r);
void _audiosrv_Logoff(struct pipes_struct *p, struct audiosrv_Logoff *r);
void _audiosrv_RegisterSessionNotificationEvent(struct pipes_struct *p, struct audiosrv_RegisterSessionNotificationEvent *r);
void _audiosrv_UnregisterSessionNotificationEvent(struct pipes_struct *p, struct audiosrv_UnregisterSessionNotificationEvent *r);
void _audiosrv_SessionConnectState(struct pipes_struct *p, struct audiosrv_SessionConnectState *r);
void _audiosrv_DriverOpenDrvRegKey(struct pipes_struct *p, struct audiosrv_DriverOpenDrvRegKey *r);
void _audiosrv_AdvisePreferredDeviceChange(struct pipes_struct *p, struct audiosrv_AdvisePreferredDeviceChange *r);
void _audiosrv_GetPnpInfo(struct pipes_struct *p, struct audiosrv_GetPnpInfo *r);
void audiosrv_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_audiosrv_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_audiosrv_shutdown(void);
#endif /* __SRV_AUDIOSRV__ */
