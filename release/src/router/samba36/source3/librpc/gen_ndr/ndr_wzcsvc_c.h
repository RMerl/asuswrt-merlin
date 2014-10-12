#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/wzcsvc.h"
#ifndef _HEADER_RPC_wzcsvc
#define _HEADER_RPC_wzcsvc

extern const struct ndr_interface_table ndr_table_wzcsvc;

struct tevent_req *dcerpc_wzcsvc_EnumInterfaces_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_EnumInterfaces *r);
NTSTATUS dcerpc_wzcsvc_EnumInterfaces_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EnumInterfaces_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_EnumInterfaces *r);
struct tevent_req *dcerpc_wzcsvc_EnumInterfaces_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_EnumInterfaces_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EnumInterfaces(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_QueryInterface_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_QueryInterface *r);
NTSTATUS dcerpc_wzcsvc_QueryInterface_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_QueryInterface_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_QueryInterface *r);
struct tevent_req *dcerpc_wzcsvc_QueryInterface_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_QueryInterface_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_QueryInterface(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_SetInterface_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_SetInterface *r);
NTSTATUS dcerpc_wzcsvc_SetInterface_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_SetInterface_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_SetInterface *r);
struct tevent_req *dcerpc_wzcsvc_SetInterface_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_SetInterface_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_SetInterface(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_RefreshInterface_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_RefreshInterface *r);
NTSTATUS dcerpc_wzcsvc_RefreshInterface_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_RefreshInterface_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_RefreshInterface *r);
struct tevent_req *dcerpc_wzcsvc_RefreshInterface_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_RefreshInterface_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_RefreshInterface(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_QueryContext_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_QueryContext *r);
NTSTATUS dcerpc_wzcsvc_QueryContext_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_QueryContext_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_QueryContext *r);
struct tevent_req *dcerpc_wzcsvc_QueryContext_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_QueryContext_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_QueryContext(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_SetContext_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_SetContext *r);
NTSTATUS dcerpc_wzcsvc_SetContext_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_SetContext_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_SetContext *r);
struct tevent_req *dcerpc_wzcsvc_SetContext_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_SetContext_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_SetContext(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_EapolUIResponse_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_EapolUIResponse *r);
NTSTATUS dcerpc_wzcsvc_EapolUIResponse_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EapolUIResponse_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_EapolUIResponse *r);
struct tevent_req *dcerpc_wzcsvc_EapolUIResponse_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_EapolUIResponse_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EapolUIResponse(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_EapolGetCustomAuthData_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_EapolGetCustomAuthData *r);
NTSTATUS dcerpc_wzcsvc_EapolGetCustomAuthData_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EapolGetCustomAuthData_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_EapolGetCustomAuthData *r);
struct tevent_req *dcerpc_wzcsvc_EapolGetCustomAuthData_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_EapolGetCustomAuthData_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EapolGetCustomAuthData(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_EapolSetCustomAuthData_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_EapolSetCustomAuthData *r);
NTSTATUS dcerpc_wzcsvc_EapolSetCustomAuthData_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EapolSetCustomAuthData_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_EapolSetCustomAuthData *r);
struct tevent_req *dcerpc_wzcsvc_EapolSetCustomAuthData_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_EapolSetCustomAuthData_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EapolSetCustomAuthData(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_EapolGetInterfaceParams_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_EapolGetInterfaceParams *r);
NTSTATUS dcerpc_wzcsvc_EapolGetInterfaceParams_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EapolGetInterfaceParams_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_EapolGetInterfaceParams *r);
struct tevent_req *dcerpc_wzcsvc_EapolGetInterfaceParams_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_EapolGetInterfaceParams_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EapolGetInterfaceParams(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_EapolSetInterfaceParams_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_EapolSetInterfaceParams *r);
NTSTATUS dcerpc_wzcsvc_EapolSetInterfaceParams_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EapolSetInterfaceParams_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_EapolSetInterfaceParams *r);
struct tevent_req *dcerpc_wzcsvc_EapolSetInterfaceParams_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_EapolSetInterfaceParams_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EapolSetInterfaceParams(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_EapolReAuthenticateInterface_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_EapolReAuthenticateInterface *r);
NTSTATUS dcerpc_wzcsvc_EapolReAuthenticateInterface_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EapolReAuthenticateInterface_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_EapolReAuthenticateInterface *r);
struct tevent_req *dcerpc_wzcsvc_EapolReAuthenticateInterface_send(TALLOC_CTX *mem_ctx,
								   struct tevent_context *ev,
								   struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_EapolReAuthenticateInterface_recv(struct tevent_req *req,
							 TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EapolReAuthenticateInterface(struct dcerpc_binding_handle *h,
						    TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_EapolQueryInterfaceState_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_EapolQueryInterfaceState *r);
NTSTATUS dcerpc_wzcsvc_EapolQueryInterfaceState_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EapolQueryInterfaceState_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_EapolQueryInterfaceState *r);
struct tevent_req *dcerpc_wzcsvc_EapolQueryInterfaceState_send(TALLOC_CTX *mem_ctx,
							       struct tevent_context *ev,
							       struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_EapolQueryInterfaceState_recv(struct tevent_req *req,
						     TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EapolQueryInterfaceState(struct dcerpc_binding_handle *h,
						TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_OpenWZCDbLogSession_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_OpenWZCDbLogSession *r);
NTSTATUS dcerpc_wzcsvc_OpenWZCDbLogSession_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_OpenWZCDbLogSession_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_OpenWZCDbLogSession *r);
struct tevent_req *dcerpc_wzcsvc_OpenWZCDbLogSession_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_OpenWZCDbLogSession_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_OpenWZCDbLogSession(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_CloseWZCDbLogSession_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_CloseWZCDbLogSession *r);
NTSTATUS dcerpc_wzcsvc_CloseWZCDbLogSession_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_CloseWZCDbLogSession_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_CloseWZCDbLogSession *r);
struct tevent_req *dcerpc_wzcsvc_CloseWZCDbLogSession_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_CloseWZCDbLogSession_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_CloseWZCDbLogSession(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_EnumWZCDbLogRecords_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_EnumWZCDbLogRecords *r);
NTSTATUS dcerpc_wzcsvc_EnumWZCDbLogRecords_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EnumWZCDbLogRecords_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_EnumWZCDbLogRecords *r);
struct tevent_req *dcerpc_wzcsvc_EnumWZCDbLogRecords_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_EnumWZCDbLogRecords_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_EnumWZCDbLogRecords(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_FlushWZCdbLog_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_FlushWZCdbLog *r);
NTSTATUS dcerpc_wzcsvc_FlushWZCdbLog_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_FlushWZCdbLog_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_FlushWZCdbLog *r);
struct tevent_req *dcerpc_wzcsvc_FlushWZCdbLog_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_FlushWZCdbLog_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_FlushWZCdbLog(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_wzcsvc_GetWZCDbLogRecord_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wzcsvc_GetWZCDbLogRecord *r);
NTSTATUS dcerpc_wzcsvc_GetWZCDbLogRecord_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_GetWZCDbLogRecord_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wzcsvc_GetWZCDbLogRecord *r);
struct tevent_req *dcerpc_wzcsvc_GetWZCDbLogRecord_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_wzcsvc_GetWZCDbLogRecord_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wzcsvc_GetWZCDbLogRecord(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx);

#endif /* _HEADER_RPC_wzcsvc */
