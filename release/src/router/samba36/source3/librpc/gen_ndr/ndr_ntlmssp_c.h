#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/ntlmssp.h"
#ifndef _HEADER_RPC_ntlmssp
#define _HEADER_RPC_ntlmssp

extern const struct ndr_interface_table ndr_table_ntlmssp;

struct tevent_req *dcerpc_decode_NEGOTIATE_MESSAGE_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_NEGOTIATE_MESSAGE *r);
NTSTATUS dcerpc_decode_NEGOTIATE_MESSAGE_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_NEGOTIATE_MESSAGE_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_NEGOTIATE_MESSAGE *r);
struct tevent_req *dcerpc_decode_NEGOTIATE_MESSAGE_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct NEGOTIATE_MESSAGE _negotiate /* [in]  */);
NTSTATUS dcerpc_decode_NEGOTIATE_MESSAGE_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_NEGOTIATE_MESSAGE(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct NEGOTIATE_MESSAGE _negotiate /* [in]  */);

struct tevent_req *dcerpc_decode_CHALLENGE_MESSAGE_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_CHALLENGE_MESSAGE *r);
NTSTATUS dcerpc_decode_CHALLENGE_MESSAGE_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_CHALLENGE_MESSAGE_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_CHALLENGE_MESSAGE *r);
struct tevent_req *dcerpc_decode_CHALLENGE_MESSAGE_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct CHALLENGE_MESSAGE _challenge /* [in]  */);
NTSTATUS dcerpc_decode_CHALLENGE_MESSAGE_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_CHALLENGE_MESSAGE(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct CHALLENGE_MESSAGE _challenge /* [in]  */);

struct tevent_req *dcerpc_decode_AUTHENTICATE_MESSAGE_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_AUTHENTICATE_MESSAGE *r);
NTSTATUS dcerpc_decode_AUTHENTICATE_MESSAGE_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_AUTHENTICATE_MESSAGE_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_AUTHENTICATE_MESSAGE *r);
struct tevent_req *dcerpc_decode_AUTHENTICATE_MESSAGE_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   struct AUTHENTICATE_MESSAGE _authenticate /* [in]  */);
NTSTATUS dcerpc_decode_AUTHENTICATE_MESSAGE_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_AUTHENTICATE_MESSAGE(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    struct AUTHENTICATE_MESSAGE _authenticate /* [in]  */);

struct tevent_req *dcerpc_decode_NTLMv2_CLIENT_CHALLENGE_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_NTLMv2_CLIENT_CHALLENGE *r);
NTSTATUS dcerpc_decode_NTLMv2_CLIENT_CHALLENGE_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_NTLMv2_CLIENT_CHALLENGE_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_NTLMv2_CLIENT_CHALLENGE *r);
struct tevent_req *dcerpc_decode_NTLMv2_CLIENT_CHALLENGE_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      struct NTLMv2_CLIENT_CHALLENGE _challenge /* [in]  */);
NTSTATUS dcerpc_decode_NTLMv2_CLIENT_CHALLENGE_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_NTLMv2_CLIENT_CHALLENGE(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       struct NTLMv2_CLIENT_CHALLENGE _challenge /* [in]  */);

struct tevent_req *dcerpc_decode_NTLMv2_RESPONSE_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_NTLMv2_RESPONSE *r);
NTSTATUS dcerpc_decode_NTLMv2_RESPONSE_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_NTLMv2_RESPONSE_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_NTLMv2_RESPONSE *r);
struct tevent_req *dcerpc_decode_NTLMv2_RESPONSE_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct NTLMv2_RESPONSE _response /* [in]  */);
NTSTATUS dcerpc_decode_NTLMv2_RESPONSE_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_NTLMv2_RESPONSE(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct NTLMv2_RESPONSE _response /* [in]  */);

#endif /* _HEADER_RPC_ntlmssp */
