#include "../librpc/gen_ndr/ndr_echo.h"
#ifndef __SRV_RPCECHO__
#define __SRV_RPCECHO__
void _echo_AddOne(pipes_struct *p, struct echo_AddOne *r);
void _echo_EchoData(pipes_struct *p, struct echo_EchoData *r);
void _echo_SinkData(pipes_struct *p, struct echo_SinkData *r);
void _echo_SourceData(pipes_struct *p, struct echo_SourceData *r);
void _echo_TestCall(pipes_struct *p, struct echo_TestCall *r);
NTSTATUS _echo_TestCall2(pipes_struct *p, struct echo_TestCall2 *r);
uint32 _echo_TestSleep(pipes_struct *p, struct echo_TestSleep *r);
void _echo_TestEnum(pipes_struct *p, struct echo_TestEnum *r);
void _echo_TestSurrounding(pipes_struct *p, struct echo_TestSurrounding *r);
uint16 _echo_TestDoublePointer(pipes_struct *p, struct echo_TestDoublePointer *r);
void rpcecho_get_pipe_fns(struct api_struct **fns, int *n_fns);
NTSTATUS rpc_rpcecho_dispatch(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, const struct ndr_interface_table *table, uint32_t opnum, void *r);
void _echo_AddOne(pipes_struct *p, struct echo_AddOne *r);
void _echo_EchoData(pipes_struct *p, struct echo_EchoData *r);
void _echo_SinkData(pipes_struct *p, struct echo_SinkData *r);
void _echo_SourceData(pipes_struct *p, struct echo_SourceData *r);
void _echo_TestCall(pipes_struct *p, struct echo_TestCall *r);
NTSTATUS _echo_TestCall2(pipes_struct *p, struct echo_TestCall2 *r);
uint32 _echo_TestSleep(pipes_struct *p, struct echo_TestSleep *r);
void _echo_TestEnum(pipes_struct *p, struct echo_TestEnum *r);
void _echo_TestSurrounding(pipes_struct *p, struct echo_TestSurrounding *r);
uint16 _echo_TestDoublePointer(pipes_struct *p, struct echo_TestDoublePointer *r);
NTSTATUS rpc_rpcecho_init(void);
#endif /* __SRV_RPCECHO__ */
