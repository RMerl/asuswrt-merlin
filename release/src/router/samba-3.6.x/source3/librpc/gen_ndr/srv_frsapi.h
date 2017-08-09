#include "librpc/gen_ndr/ndr_frsapi.h"
#ifndef __SRV_FRSAPI__
#define __SRV_FRSAPI__
void _FRSAPI_VERIFY_PROMOTION(struct pipes_struct *p, struct FRSAPI_VERIFY_PROMOTION *r);
void _FRSAPI_PROMOTION_STATUS(struct pipes_struct *p, struct FRSAPI_PROMOTION_STATUS *r);
void _FRSAPI_START_DEMOTION(struct pipes_struct *p, struct FRSAPI_START_DEMOTION *r);
void _FRSAPI_COMMIT_DEMOTION(struct pipes_struct *p, struct FRSAPI_COMMIT_DEMOTION *r);
WERROR _frsapi_SetDsPollingIntervalW(struct pipes_struct *p, struct frsapi_SetDsPollingIntervalW *r);
WERROR _frsapi_GetDsPollingIntervalW(struct pipes_struct *p, struct frsapi_GetDsPollingIntervalW *r);
void _FRSAPI_VERIFY_PROMOTION_W(struct pipes_struct *p, struct FRSAPI_VERIFY_PROMOTION_W *r);
WERROR _frsapi_InfoW(struct pipes_struct *p, struct frsapi_InfoW *r);
WERROR _frsapi_IsPathReplicated(struct pipes_struct *p, struct frsapi_IsPathReplicated *r);
WERROR _frsapi_WriterCommand(struct pipes_struct *p, struct frsapi_WriterCommand *r);
WERROR _frsapi_ForceReplication(struct pipes_struct *p, struct frsapi_ForceReplication *r);
void frsapi_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_frsapi_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_frsapi_shutdown(void);
#endif /* __SRV_FRSAPI__ */
