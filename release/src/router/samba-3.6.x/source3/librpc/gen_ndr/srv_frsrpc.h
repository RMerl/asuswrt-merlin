#include "librpc/gen_ndr/ndr_frsrpc.h"
#ifndef __SRV_FRSRPC__
#define __SRV_FRSRPC__
WERROR _frsrpc_FrsSendCommPkt(struct pipes_struct *p, struct frsrpc_FrsSendCommPkt *r);
WERROR _frsrpc_FrsVerifyPromotionParent(struct pipes_struct *p, struct frsrpc_FrsVerifyPromotionParent *r);
WERROR _frsrpc_FrsStartPromotionParent(struct pipes_struct *p, struct frsrpc_FrsStartPromotionParent *r);
WERROR _frsrpc_FrsNOP(struct pipes_struct *p, struct frsrpc_FrsNOP *r);
void _FRSRPC_BACKUP_COMPLETE(struct pipes_struct *p, struct FRSRPC_BACKUP_COMPLETE *r);
void _FRSRPC_BACKUP_COMPLETE_5(struct pipes_struct *p, struct FRSRPC_BACKUP_COMPLETE_5 *r);
void _FRSRPC_BACKUP_COMPLETE_6(struct pipes_struct *p, struct FRSRPC_BACKUP_COMPLETE_6 *r);
void _FRSRPC_BACKUP_COMPLETE_7(struct pipes_struct *p, struct FRSRPC_BACKUP_COMPLETE_7 *r);
void _FRSRPC_BACKUP_COMPLETE_8(struct pipes_struct *p, struct FRSRPC_BACKUP_COMPLETE_8 *r);
void _FRSRPC_BACKUP_COMPLETE_9(struct pipes_struct *p, struct FRSRPC_BACKUP_COMPLETE_9 *r);
void _FRSRPC_VERIFY_PROMOTION_PARENT_EX(struct pipes_struct *p, struct FRSRPC_VERIFY_PROMOTION_PARENT_EX *r);
void frsrpc_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_frsrpc_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_frsrpc_shutdown(void);
#endif /* __SRV_FRSRPC__ */
