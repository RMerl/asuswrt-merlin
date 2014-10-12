#include "librpc/gen_ndr/ndr_winreg.h"
#ifndef __SRV_WINREG__
#define __SRV_WINREG__
WERROR _winreg_OpenHKCR(struct pipes_struct *p, struct winreg_OpenHKCR *r);
WERROR _winreg_OpenHKCU(struct pipes_struct *p, struct winreg_OpenHKCU *r);
WERROR _winreg_OpenHKLM(struct pipes_struct *p, struct winreg_OpenHKLM *r);
WERROR _winreg_OpenHKPD(struct pipes_struct *p, struct winreg_OpenHKPD *r);
WERROR _winreg_OpenHKU(struct pipes_struct *p, struct winreg_OpenHKU *r);
WERROR _winreg_CloseKey(struct pipes_struct *p, struct winreg_CloseKey *r);
WERROR _winreg_CreateKey(struct pipes_struct *p, struct winreg_CreateKey *r);
WERROR _winreg_DeleteKey(struct pipes_struct *p, struct winreg_DeleteKey *r);
WERROR _winreg_DeleteValue(struct pipes_struct *p, struct winreg_DeleteValue *r);
WERROR _winreg_EnumKey(struct pipes_struct *p, struct winreg_EnumKey *r);
WERROR _winreg_EnumValue(struct pipes_struct *p, struct winreg_EnumValue *r);
WERROR _winreg_FlushKey(struct pipes_struct *p, struct winreg_FlushKey *r);
WERROR _winreg_GetKeySecurity(struct pipes_struct *p, struct winreg_GetKeySecurity *r);
WERROR _winreg_LoadKey(struct pipes_struct *p, struct winreg_LoadKey *r);
WERROR _winreg_NotifyChangeKeyValue(struct pipes_struct *p, struct winreg_NotifyChangeKeyValue *r);
WERROR _winreg_OpenKey(struct pipes_struct *p, struct winreg_OpenKey *r);
WERROR _winreg_QueryInfoKey(struct pipes_struct *p, struct winreg_QueryInfoKey *r);
WERROR _winreg_QueryValue(struct pipes_struct *p, struct winreg_QueryValue *r);
WERROR _winreg_ReplaceKey(struct pipes_struct *p, struct winreg_ReplaceKey *r);
WERROR _winreg_RestoreKey(struct pipes_struct *p, struct winreg_RestoreKey *r);
WERROR _winreg_SaveKey(struct pipes_struct *p, struct winreg_SaveKey *r);
WERROR _winreg_SetKeySecurity(struct pipes_struct *p, struct winreg_SetKeySecurity *r);
WERROR _winreg_SetValue(struct pipes_struct *p, struct winreg_SetValue *r);
WERROR _winreg_UnLoadKey(struct pipes_struct *p, struct winreg_UnLoadKey *r);
WERROR _winreg_InitiateSystemShutdown(struct pipes_struct *p, struct winreg_InitiateSystemShutdown *r);
WERROR _winreg_AbortSystemShutdown(struct pipes_struct *p, struct winreg_AbortSystemShutdown *r);
WERROR _winreg_GetVersion(struct pipes_struct *p, struct winreg_GetVersion *r);
WERROR _winreg_OpenHKCC(struct pipes_struct *p, struct winreg_OpenHKCC *r);
WERROR _winreg_OpenHKDD(struct pipes_struct *p, struct winreg_OpenHKDD *r);
WERROR _winreg_QueryMultipleValues(struct pipes_struct *p, struct winreg_QueryMultipleValues *r);
WERROR _winreg_InitiateSystemShutdownEx(struct pipes_struct *p, struct winreg_InitiateSystemShutdownEx *r);
WERROR _winreg_SaveKeyEx(struct pipes_struct *p, struct winreg_SaveKeyEx *r);
WERROR _winreg_OpenHKPT(struct pipes_struct *p, struct winreg_OpenHKPT *r);
WERROR _winreg_OpenHKPN(struct pipes_struct *p, struct winreg_OpenHKPN *r);
WERROR _winreg_QueryMultipleValues2(struct pipes_struct *p, struct winreg_QueryMultipleValues2 *r);
WERROR _winreg_DeleteKeyEx(struct pipes_struct *p, struct winreg_DeleteKeyEx *r);
void winreg_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_winreg_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_winreg_shutdown(void);
#endif /* __SRV_WINREG__ */
