#include "librpc/gen_ndr/ndr_dfs.h"
#ifndef __SRV_NETDFS__
#define __SRV_NETDFS__
void _dfs_GetManagerVersion(struct pipes_struct *p, struct dfs_GetManagerVersion *r);
WERROR _dfs_Add(struct pipes_struct *p, struct dfs_Add *r);
WERROR _dfs_Remove(struct pipes_struct *p, struct dfs_Remove *r);
WERROR _dfs_SetInfo(struct pipes_struct *p, struct dfs_SetInfo *r);
WERROR _dfs_GetInfo(struct pipes_struct *p, struct dfs_GetInfo *r);
WERROR _dfs_Enum(struct pipes_struct *p, struct dfs_Enum *r);
WERROR _dfs_Rename(struct pipes_struct *p, struct dfs_Rename *r);
WERROR _dfs_Move(struct pipes_struct *p, struct dfs_Move *r);
WERROR _dfs_ManagerGetConfigInfo(struct pipes_struct *p, struct dfs_ManagerGetConfigInfo *r);
WERROR _dfs_ManagerSendSiteInfo(struct pipes_struct *p, struct dfs_ManagerSendSiteInfo *r);
WERROR _dfs_AddFtRoot(struct pipes_struct *p, struct dfs_AddFtRoot *r);
WERROR _dfs_RemoveFtRoot(struct pipes_struct *p, struct dfs_RemoveFtRoot *r);
WERROR _dfs_AddStdRoot(struct pipes_struct *p, struct dfs_AddStdRoot *r);
WERROR _dfs_RemoveStdRoot(struct pipes_struct *p, struct dfs_RemoveStdRoot *r);
WERROR _dfs_ManagerInitialize(struct pipes_struct *p, struct dfs_ManagerInitialize *r);
WERROR _dfs_AddStdRootForced(struct pipes_struct *p, struct dfs_AddStdRootForced *r);
WERROR _dfs_GetDcAddress(struct pipes_struct *p, struct dfs_GetDcAddress *r);
WERROR _dfs_SetDcAddress(struct pipes_struct *p, struct dfs_SetDcAddress *r);
WERROR _dfs_FlushFtTable(struct pipes_struct *p, struct dfs_FlushFtTable *r);
WERROR _dfs_Add2(struct pipes_struct *p, struct dfs_Add2 *r);
WERROR _dfs_Remove2(struct pipes_struct *p, struct dfs_Remove2 *r);
WERROR _dfs_EnumEx(struct pipes_struct *p, struct dfs_EnumEx *r);
WERROR _dfs_SetInfo2(struct pipes_struct *p, struct dfs_SetInfo2 *r);
void netdfs_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_netdfs_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_netdfs_shutdown(void);
#endif /* __SRV_NETDFS__ */
