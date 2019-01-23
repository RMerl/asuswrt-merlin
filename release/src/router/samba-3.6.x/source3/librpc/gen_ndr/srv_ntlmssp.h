#include "librpc/gen_ndr/ndr_ntlmssp.h"
#ifndef __SRV_NTLMSSP__
#define __SRV_NTLMSSP__
void _decode_NEGOTIATE_MESSAGE(struct pipes_struct *p, struct decode_NEGOTIATE_MESSAGE *r);
void _decode_CHALLENGE_MESSAGE(struct pipes_struct *p, struct decode_CHALLENGE_MESSAGE *r);
void _decode_AUTHENTICATE_MESSAGE(struct pipes_struct *p, struct decode_AUTHENTICATE_MESSAGE *r);
void _decode_NTLMv2_CLIENT_CHALLENGE(struct pipes_struct *p, struct decode_NTLMv2_CLIENT_CHALLENGE *r);
void _decode_NTLMv2_RESPONSE(struct pipes_struct *p, struct decode_NTLMv2_RESPONSE *r);
void ntlmssp_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_ntlmssp_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_ntlmssp_shutdown(void);
#endif /* __SRV_NTLMSSP__ */
