#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/echo.h"
#ifndef _HEADER_RPC_rpcecho
#define _HEADER_RPC_rpcecho

extern const struct ndr_interface_table ndr_table_rpcecho;

struct tevent_req *dcerpc_echo_AddOne_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct echo_AddOne *r);
NTSTATUS dcerpc_echo_AddOne_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_AddOne_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct echo_AddOne *r);
struct tevent_req *dcerpc_echo_AddOne_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   uint32_t _in_data /* [in]  */,
					   uint32_t *_out_data /* [out] [ref] */);
NTSTATUS dcerpc_echo_AddOne_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_AddOne(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    uint32_t _in_data /* [in]  */,
			    uint32_t *_out_data /* [out] [ref] */);

struct tevent_req *dcerpc_echo_EchoData_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct echo_EchoData *r);
NTSTATUS dcerpc_echo_EchoData_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_EchoData_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct echo_EchoData *r);
struct tevent_req *dcerpc_echo_EchoData_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     uint32_t _len /* [in]  */,
					     uint8_t *_in_data /* [in] [size_is(len)] */,
					     uint8_t *_out_data /* [out] [size_is(len)] */);
NTSTATUS dcerpc_echo_EchoData_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_EchoData(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      uint32_t _len /* [in]  */,
			      uint8_t *_in_data /* [in] [size_is(len)] */,
			      uint8_t *_out_data /* [out] [size_is(len)] */);

struct tevent_req *dcerpc_echo_SinkData_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct echo_SinkData *r);
NTSTATUS dcerpc_echo_SinkData_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_SinkData_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct echo_SinkData *r);
struct tevent_req *dcerpc_echo_SinkData_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     uint32_t _len /* [in]  */,
					     uint8_t *_data /* [in] [size_is(len)] */);
NTSTATUS dcerpc_echo_SinkData_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_SinkData(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      uint32_t _len /* [in]  */,
			      uint8_t *_data /* [in] [size_is(len)] */);

struct tevent_req *dcerpc_echo_SourceData_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct echo_SourceData *r);
NTSTATUS dcerpc_echo_SourceData_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_SourceData_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct echo_SourceData *r);
struct tevent_req *dcerpc_echo_SourceData_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       uint32_t _len /* [in]  */,
					       uint8_t *_data /* [out] [size_is(len)] */);
NTSTATUS dcerpc_echo_SourceData_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_SourceData(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				uint32_t _len /* [in]  */,
				uint8_t *_data /* [out] [size_is(len)] */);

struct tevent_req *dcerpc_echo_TestCall_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct echo_TestCall *r);
NTSTATUS dcerpc_echo_TestCall_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_TestCall_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct echo_TestCall *r);
struct tevent_req *dcerpc_echo_TestCall_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     const char *_s1 /* [in] [ref,charset(UTF16)] */,
					     const char **_s2 /* [out] [charset(UTF16),ref] */);
NTSTATUS dcerpc_echo_TestCall_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_TestCall(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      const char *_s1 /* [in] [ref,charset(UTF16)] */,
			      const char **_s2 /* [out] [charset(UTF16),ref] */);

struct tevent_req *dcerpc_echo_TestCall2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct echo_TestCall2 *r);
NTSTATUS dcerpc_echo_TestCall2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_TestCall2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct echo_TestCall2 *r);
struct tevent_req *dcerpc_echo_TestCall2_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      uint16_t _level /* [in]  */,
					      union echo_Info *_info /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_echo_TestCall2_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    NTSTATUS *result);
NTSTATUS dcerpc_echo_TestCall2(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       uint16_t _level /* [in]  */,
			       union echo_Info *_info /* [out] [ref,switch_is(level)] */,
			       NTSTATUS *result);

struct tevent_req *dcerpc_echo_TestSleep_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct echo_TestSleep *r);
NTSTATUS dcerpc_echo_TestSleep_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_TestSleep_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct echo_TestSleep *r);
struct tevent_req *dcerpc_echo_TestSleep_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      uint32_t _seconds /* [in]  */);
NTSTATUS dcerpc_echo_TestSleep_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    uint32_t *result);
NTSTATUS dcerpc_echo_TestSleep(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       uint32_t _seconds /* [in]  */,
			       uint32_t *result);

struct tevent_req *dcerpc_echo_TestEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct echo_TestEnum *r);
NTSTATUS dcerpc_echo_TestEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_TestEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct echo_TestEnum *r);
struct tevent_req *dcerpc_echo_TestEnum_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     enum echo_Enum1 *_foo1 /* [in,out] [ref] */,
					     struct echo_Enum2 *_foo2 /* [in,out] [ref] */,
					     union echo_Enum3 *_foo3 /* [in,out] [ref,switch_is(*foo1)] */);
NTSTATUS dcerpc_echo_TestEnum_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_TestEnum(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      enum echo_Enum1 *_foo1 /* [in,out] [ref] */,
			      struct echo_Enum2 *_foo2 /* [in,out] [ref] */,
			      union echo_Enum3 *_foo3 /* [in,out] [ref,switch_is(*foo1)] */);

struct tevent_req *dcerpc_echo_TestSurrounding_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct echo_TestSurrounding *r);
NTSTATUS dcerpc_echo_TestSurrounding_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_TestSurrounding_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct echo_TestSurrounding *r);
struct tevent_req *dcerpc_echo_TestSurrounding_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct echo_Surrounding *_data /* [in,out] [ref] */);
NTSTATUS dcerpc_echo_TestSurrounding_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_TestSurrounding(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct echo_Surrounding *_data /* [in,out] [ref] */);

struct tevent_req *dcerpc_echo_TestDoublePointer_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct echo_TestDoublePointer *r);
NTSTATUS dcerpc_echo_TestDoublePointer_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_echo_TestDoublePointer_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct echo_TestDoublePointer *r);
struct tevent_req *dcerpc_echo_TestDoublePointer_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      uint16_t ***_data /* [in] [ref] */);
NTSTATUS dcerpc_echo_TestDoublePointer_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    uint16_t *result);
NTSTATUS dcerpc_echo_TestDoublePointer(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       uint16_t ***_data /* [in] [ref] */,
				       uint16_t *result);

#endif /* _HEADER_RPC_rpcecho */
