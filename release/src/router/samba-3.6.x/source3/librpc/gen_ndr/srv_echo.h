#include "librpc/gen_ndr/ndr_echo.h"
#ifndef __SRV_RPCECHO__
#define __SRV_RPCECHO__
void _echo_AddOne(struct pipes_struct *p, struct echo_AddOne *r);
void _echo_EchoData(struct pipes_struct *p, struct echo_EchoData *r);
void _echo_SinkData(struct pipes_struct *p, struct echo_SinkData *r);
void _echo_SourceData(struct pipes_struct *p, struct echo_SourceData *r);
void _echo_TestCall(struct pipes_struct *p, struct echo_TestCall *r);
NTSTATUS _echo_TestCall2(struct pipes_struct *p, struct echo_TestCall2 *r);
uint32 _echo_TestSleep(struct pipes_struct *p, struct echo_TestSleep *r);
void _echo_TestEnum(struct pipes_struct *p, struct echo_TestEnum *r);
void _echo_TestSurrounding(struct pipes_struct *p, struct echo_TestSurrounding *r);
uint16 _echo_TestDoublePointer(struct pipes_struct *p, struct echo_TestDoublePointer *r);
void rpcecho_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_rpcecho_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_rpcecho_shutdown(void);
#endif /* __SRV_RPCECHO__ */
