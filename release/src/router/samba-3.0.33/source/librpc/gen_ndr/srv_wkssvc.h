#include "librpc/gen_ndr/ndr_wkssvc.h"
#ifndef __SRV_WKSSVC__
#define __SRV_WKSSVC__
WERROR _wkssvc_NetWkstaGetInfo(pipes_struct *p, struct wkssvc_NetWkstaGetInfo *r);
WERROR _wkssvc_NetWkstaSetInfo(pipes_struct *p, struct wkssvc_NetWkstaSetInfo *r);
WERROR _wkssvc_NetWkstaEnumUsers(pipes_struct *p, struct wkssvc_NetWkstaEnumUsers *r);
WERROR _WKSSVC_NETRWKSTAUSERGETINFO(pipes_struct *p, struct WKSSVC_NETRWKSTAUSERGETINFO *r);
WERROR _WKSSVC_NETRWKSTAUSERSETINFO(pipes_struct *p, struct WKSSVC_NETRWKSTAUSERSETINFO *r);
WERROR _wkssvc_NetWkstaTransportEnum(pipes_struct *p, struct wkssvc_NetWkstaTransportEnum *r);
WERROR _WKSSVC_NETRWKSTATRANSPORTADD(pipes_struct *p, struct WKSSVC_NETRWKSTATRANSPORTADD *r);
WERROR _WKSSVC_NETRWKSTATRANSPORTDEL(pipes_struct *p, struct WKSSVC_NETRWKSTATRANSPORTDEL *r);
WERROR _WKSSVC_NETRUSEADD(pipes_struct *p, struct WKSSVC_NETRUSEADD *r);
WERROR _WKSSVC_NETRUSEGETINFO(pipes_struct *p, struct WKSSVC_NETRUSEGETINFO *r);
WERROR _WKSSVC_NETRUSEDEL(pipes_struct *p, struct WKSSVC_NETRUSEDEL *r);
WERROR _WKSSVC_NETRUSEENUM(pipes_struct *p, struct WKSSVC_NETRUSEENUM *r);
WERROR _WKSSVC_NETRMESSAGEBUFFERSEND(pipes_struct *p, struct WKSSVC_NETRMESSAGEBUFFERSEND *r);
WERROR _WKSSVC_NETRWORKSTATIONSTATISTICSGET(pipes_struct *p, struct WKSSVC_NETRWORKSTATIONSTATISTICSGET *r);
WERROR _WKSSVC_NETRLOGONDOMAINNAMEADD(pipes_struct *p, struct WKSSVC_NETRLOGONDOMAINNAMEADD *r);
WERROR _WKSSVC_NETRLOGONDOMAINNAMEDEL(pipes_struct *p, struct WKSSVC_NETRLOGONDOMAINNAMEDEL *r);
WERROR _WKSSVC_NETRJOINDOMAIN(pipes_struct *p, struct WKSSVC_NETRJOINDOMAIN *r);
WERROR _WKSSVC_NETRUNJOINDOMAIN(pipes_struct *p, struct WKSSVC_NETRUNJOINDOMAIN *r);
WERROR _WKSSVC_NETRRENAMEMACHINEINDOMAIN(pipes_struct *p, struct WKSSVC_NETRRENAMEMACHINEINDOMAIN *r);
WERROR _WKSSVC_NETRVALIDATENAME(pipes_struct *p, struct WKSSVC_NETRVALIDATENAME *r);
WERROR _WKSSVC_NETRGETJOININFORMATION(pipes_struct *p, struct WKSSVC_NETRGETJOININFORMATION *r);
WERROR _WKSSVC_NETRGETJOINABLEOUS(pipes_struct *p, struct WKSSVC_NETRGETJOINABLEOUS *r);
WERROR _wkssvc_NetrJoinDomain2(pipes_struct *p, struct wkssvc_NetrJoinDomain2 *r);
WERROR _wkssvc_NetrUnjoinDomain2(pipes_struct *p, struct wkssvc_NetrUnjoinDomain2 *r);
WERROR _wkssvc_NetrRenameMachineInDomain2(pipes_struct *p, struct wkssvc_NetrRenameMachineInDomain2 *r);
WERROR _WKSSVC_NETRVALIDATENAME2(pipes_struct *p, struct WKSSVC_NETRVALIDATENAME2 *r);
WERROR _WKSSVC_NETRGETJOINABLEOUS2(pipes_struct *p, struct WKSSVC_NETRGETJOINABLEOUS2 *r);
WERROR _wkssvc_NetrAddAlternateComputerName(pipes_struct *p, struct wkssvc_NetrAddAlternateComputerName *r);
WERROR _wkssvc_NetrRemoveAlternateComputerName(pipes_struct *p, struct wkssvc_NetrRemoveAlternateComputerName *r);
WERROR _WKSSVC_NETRSETPRIMARYCOMPUTERNAME(pipes_struct *p, struct WKSSVC_NETRSETPRIMARYCOMPUTERNAME *r);
WERROR _WKSSVC_NETRENUMERATECOMPUTERNAMES(pipes_struct *p, struct WKSSVC_NETRENUMERATECOMPUTERNAMES *r);
void wkssvc_get_pipe_fns(struct api_struct **fns, int *n_fns);
NTSTATUS rpc_wkssvc_init(void);
#endif /* __SRV_WKSSVC__ */
