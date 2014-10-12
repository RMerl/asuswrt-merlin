#include "librpc/gen_ndr/ndr_frstrans.h"
#ifndef __SRV_FRSTRANS__
#define __SRV_FRSTRANS__
WERROR _frstrans_CheckConnectivity(struct pipes_struct *p, struct frstrans_CheckConnectivity *r);
WERROR _frstrans_EstablishConnection(struct pipes_struct *p, struct frstrans_EstablishConnection *r);
WERROR _frstrans_EstablishSession(struct pipes_struct *p, struct frstrans_EstablishSession *r);
WERROR _frstrans_RequestUpdates(struct pipes_struct *p, struct frstrans_RequestUpdates *r);
WERROR _frstrans_RequestVersionVector(struct pipes_struct *p, struct frstrans_RequestVersionVector *r);
WERROR _frstrans_AsyncPoll(struct pipes_struct *p, struct frstrans_AsyncPoll *r);
void _FRSTRANS_REQUEST_RECORDS(struct pipes_struct *p, struct FRSTRANS_REQUEST_RECORDS *r);
void _FRSTRANS_UPDATE_CANCEL(struct pipes_struct *p, struct FRSTRANS_UPDATE_CANCEL *r);
void _FRSTRANS_RAW_GET_FILE_DATA(struct pipes_struct *p, struct FRSTRANS_RAW_GET_FILE_DATA *r);
void _FRSTRANS_RDC_GET_SIGNATURES(struct pipes_struct *p, struct FRSTRANS_RDC_GET_SIGNATURES *r);
void _FRSTRANS_RDC_PUSH_SOURCE_NEEDS(struct pipes_struct *p, struct FRSTRANS_RDC_PUSH_SOURCE_NEEDS *r);
void _FRSTRANS_RDC_GET_FILE_DATA(struct pipes_struct *p, struct FRSTRANS_RDC_GET_FILE_DATA *r);
void _FRSTRANS_RDC_CLOSE(struct pipes_struct *p, struct FRSTRANS_RDC_CLOSE *r);
WERROR _frstrans_InitializeFileTransferAsync(struct pipes_struct *p, struct frstrans_InitializeFileTransferAsync *r);
void _FRSTRANS_OPNUM_0E_NOT_USED_ON_THE_WIRE(struct pipes_struct *p, struct FRSTRANS_OPNUM_0E_NOT_USED_ON_THE_WIRE *r);
WERROR _frstrans_RawGetFileDataAsync(struct pipes_struct *p, struct frstrans_RawGetFileDataAsync *r);
WERROR _frstrans_RdcGetFileDataAsync(struct pipes_struct *p, struct frstrans_RdcGetFileDataAsync *r);
void frstrans_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_frstrans_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_frstrans_shutdown(void);
#endif /* __SRV_FRSTRANS__ */
