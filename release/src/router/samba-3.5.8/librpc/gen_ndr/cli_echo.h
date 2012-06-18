#include "../librpc/gen_ndr/ndr_echo.h"
#ifndef __CLI_RPCECHO__
#define __CLI_RPCECHO__
struct tevent_req *rpccli_echo_AddOne_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct rpc_pipe_client *cli,
					   uint32_t _in_data /* [in]  */,
					   uint32_t *_out_data /* [out] [ref] */);
NTSTATUS rpccli_echo_AddOne_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_echo_AddOne(struct rpc_pipe_client *cli,
			    TALLOC_CTX *mem_ctx,
			    uint32_t in_data /* [in]  */,
			    uint32_t *out_data /* [out] [ref] */);
struct tevent_req *rpccli_echo_EchoData_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli,
					     uint32_t _len /* [in]  */,
					     uint8_t *_in_data /* [in] [size_is(len)] */,
					     uint8_t *_out_data /* [out] [size_is(len)] */);
NTSTATUS rpccli_echo_EchoData_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_echo_EchoData(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      uint32_t len /* [in]  */,
			      uint8_t *in_data /* [in] [size_is(len)] */,
			      uint8_t *out_data /* [out] [size_is(len)] */);
struct tevent_req *rpccli_echo_SinkData_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli,
					     uint32_t _len /* [in]  */,
					     uint8_t *_data /* [in] [size_is(len)] */);
NTSTATUS rpccli_echo_SinkData_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_echo_SinkData(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      uint32_t len /* [in]  */,
			      uint8_t *data /* [in] [size_is(len)] */);
struct tevent_req *rpccli_echo_SourceData_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct rpc_pipe_client *cli,
					       uint32_t _len /* [in]  */,
					       uint8_t *_data /* [out] [size_is(len)] */);
NTSTATUS rpccli_echo_SourceData_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_echo_SourceData(struct rpc_pipe_client *cli,
				TALLOC_CTX *mem_ctx,
				uint32_t len /* [in]  */,
				uint8_t *data /* [out] [size_is(len)] */);
struct tevent_req *rpccli_echo_TestCall_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli,
					     const char *_s1 /* [in] [ref,charset(UTF16)] */,
					     const char **_s2 /* [out] [ref,charset(UTF16)] */);
NTSTATUS rpccli_echo_TestCall_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_echo_TestCall(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      const char *s1 /* [in] [ref,charset(UTF16)] */,
			      const char **s2 /* [out] [ref,charset(UTF16)] */);
struct tevent_req *rpccli_echo_TestCall2_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli,
					      uint16_t _level /* [in]  */,
					      union echo_Info *_info /* [out] [ref,switch_is(level)] */);
NTSTATUS rpccli_echo_TestCall2_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    NTSTATUS *result);
NTSTATUS rpccli_echo_TestCall2(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       uint16_t level /* [in]  */,
			       union echo_Info *info /* [out] [ref,switch_is(level)] */);
struct tevent_req *rpccli_echo_TestSleep_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct rpc_pipe_client *cli,
					      uint32_t _seconds /* [in]  */);
NTSTATUS rpccli_echo_TestSleep_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    uint32 *result);
NTSTATUS rpccli_echo_TestSleep(struct rpc_pipe_client *cli,
			       TALLOC_CTX *mem_ctx,
			       uint32_t seconds /* [in]  */);
struct tevent_req *rpccli_echo_TestEnum_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct rpc_pipe_client *cli,
					     enum echo_Enum1 *_foo1 /* [in,out] [ref] */,
					     struct echo_Enum2 *_foo2 /* [in,out] [ref] */,
					     union echo_Enum3 *_foo3 /* [in,out] [ref,switch_is(*foo1)] */);
NTSTATUS rpccli_echo_TestEnum_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_echo_TestEnum(struct rpc_pipe_client *cli,
			      TALLOC_CTX *mem_ctx,
			      enum echo_Enum1 *foo1 /* [in,out] [ref] */,
			      struct echo_Enum2 *foo2 /* [in,out] [ref] */,
			      union echo_Enum3 *foo3 /* [in,out] [ref,switch_is(*foo1)] */);
struct tevent_req *rpccli_echo_TestSurrounding_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct rpc_pipe_client *cli,
						    struct echo_Surrounding *_data /* [in,out] [ref] */);
NTSTATUS rpccli_echo_TestSurrounding_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_echo_TestSurrounding(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx,
				     struct echo_Surrounding *data /* [in,out] [ref] */);
struct tevent_req *rpccli_echo_TestDoublePointer_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct rpc_pipe_client *cli,
						      uint16_t ***_data /* [in] [ref] */);
NTSTATUS rpccli_echo_TestDoublePointer_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    uint16 *result);
NTSTATUS rpccli_echo_TestDoublePointer(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       uint16_t ***data /* [in] [ref] */);
#endif /* __CLI_RPCECHO__ */
