#include "librpc/gen_ndr/ndr_wkssvc.h"
#ifndef __SRV_WKSSVC__
#define __SRV_WKSSVC__
WERROR _wkssvc_NetWkstaGetInfo(struct pipes_struct *p, struct wkssvc_NetWkstaGetInfo *r);
WERROR _wkssvc_NetWkstaSetInfo(struct pipes_struct *p, struct wkssvc_NetWkstaSetInfo *r);
WERROR _wkssvc_NetWkstaEnumUsers(struct pipes_struct *p, struct wkssvc_NetWkstaEnumUsers *r);
WERROR _wkssvc_NetrWkstaUserGetInfo(struct pipes_struct *p, struct wkssvc_NetrWkstaUserGetInfo *r);
WERROR _wkssvc_NetrWkstaUserSetInfo(struct pipes_struct *p, struct wkssvc_NetrWkstaUserSetInfo *r);
WERROR _wkssvc_NetWkstaTransportEnum(struct pipes_struct *p, struct wkssvc_NetWkstaTransportEnum *r);
WERROR _wkssvc_NetrWkstaTransportAdd(struct pipes_struct *p, struct wkssvc_NetrWkstaTransportAdd *r);
WERROR _wkssvc_NetrWkstaTransportDel(struct pipes_struct *p, struct wkssvc_NetrWkstaTransportDel *r);
WERROR _wkssvc_NetrUseAdd(struct pipes_struct *p, struct wkssvc_NetrUseAdd *r);
WERROR _wkssvc_NetrUseGetInfo(struct pipes_struct *p, struct wkssvc_NetrUseGetInfo *r);
WERROR _wkssvc_NetrUseDel(struct pipes_struct *p, struct wkssvc_NetrUseDel *r);
WERROR _wkssvc_NetrUseEnum(struct pipes_struct *p, struct wkssvc_NetrUseEnum *r);
WERROR _wkssvc_NetrMessageBufferSend(struct pipes_struct *p, struct wkssvc_NetrMessageBufferSend *r);
WERROR _wkssvc_NetrWorkstationStatisticsGet(struct pipes_struct *p, struct wkssvc_NetrWorkstationStatisticsGet *r);
WERROR _wkssvc_NetrLogonDomainNameAdd(struct pipes_struct *p, struct wkssvc_NetrLogonDomainNameAdd *r);
WERROR _wkssvc_NetrLogonDomainNameDel(struct pipes_struct *p, struct wkssvc_NetrLogonDomainNameDel *r);
WERROR _wkssvc_NetrJoinDomain(struct pipes_struct *p, struct wkssvc_NetrJoinDomain *r);
WERROR _wkssvc_NetrUnjoinDomain(struct pipes_struct *p, struct wkssvc_NetrUnjoinDomain *r);
WERROR _wkssvc_NetrRenameMachineInDomain(struct pipes_struct *p, struct wkssvc_NetrRenameMachineInDomain *r);
WERROR _wkssvc_NetrValidateName(struct pipes_struct *p, struct wkssvc_NetrValidateName *r);
WERROR _wkssvc_NetrGetJoinInformation(struct pipes_struct *p, struct wkssvc_NetrGetJoinInformation *r);
WERROR _wkssvc_NetrGetJoinableOus(struct pipes_struct *p, struct wkssvc_NetrGetJoinableOus *r);
WERROR _wkssvc_NetrJoinDomain2(struct pipes_struct *p, struct wkssvc_NetrJoinDomain2 *r);
WERROR _wkssvc_NetrUnjoinDomain2(struct pipes_struct *p, struct wkssvc_NetrUnjoinDomain2 *r);
WERROR _wkssvc_NetrRenameMachineInDomain2(struct pipes_struct *p, struct wkssvc_NetrRenameMachineInDomain2 *r);
WERROR _wkssvc_NetrValidateName2(struct pipes_struct *p, struct wkssvc_NetrValidateName2 *r);
WERROR _wkssvc_NetrGetJoinableOus2(struct pipes_struct *p, struct wkssvc_NetrGetJoinableOus2 *r);
WERROR _wkssvc_NetrAddAlternateComputerName(struct pipes_struct *p, struct wkssvc_NetrAddAlternateComputerName *r);
WERROR _wkssvc_NetrRemoveAlternateComputerName(struct pipes_struct *p, struct wkssvc_NetrRemoveAlternateComputerName *r);
WERROR _wkssvc_NetrSetPrimaryComputername(struct pipes_struct *p, struct wkssvc_NetrSetPrimaryComputername *r);
WERROR _wkssvc_NetrEnumerateComputerNames(struct pipes_struct *p, struct wkssvc_NetrEnumerateComputerNames *r);
void wkssvc_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_wkssvc_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_wkssvc_shutdown(void);
#endif /* __SRV_WKSSVC__ */
