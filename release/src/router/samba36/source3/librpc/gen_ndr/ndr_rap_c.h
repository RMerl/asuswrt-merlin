#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/rap.h"
#ifndef _HEADER_RPC_rap
#define _HEADER_RPC_rap

struct tevent_req *dcerpc_rap_NetShareEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetShareEnum *r);
NTSTATUS dcerpc_rap_NetShareEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetShareEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetShareEnum *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetShareEnum_send()
 * dcerpc_rap_NetShareEnum_recv()
 * dcerpc_rap_NetShareEnum()
 */

struct tevent_req *dcerpc_rap_NetShareAdd_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetShareAdd *r);
NTSTATUS dcerpc_rap_NetShareAdd_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetShareAdd_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetShareAdd *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetShareAdd_send()
 * dcerpc_rap_NetShareAdd_recv()
 * dcerpc_rap_NetShareAdd()
 */

struct tevent_req *dcerpc_rap_NetServerEnum2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetServerEnum2 *r);
NTSTATUS dcerpc_rap_NetServerEnum2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetServerEnum2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetServerEnum2 *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetServerEnum2_send()
 * dcerpc_rap_NetServerEnum2_recv()
 * dcerpc_rap_NetServerEnum2()
 */

struct tevent_req *dcerpc_rap_WserverGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_WserverGetInfo *r);
NTSTATUS dcerpc_rap_WserverGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_WserverGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_WserverGetInfo *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_WserverGetInfo_send()
 * dcerpc_rap_WserverGetInfo_recv()
 * dcerpc_rap_WserverGetInfo()
 */

struct tevent_req *dcerpc_rap_NetPrintQEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetPrintQEnum *r);
NTSTATUS dcerpc_rap_NetPrintQEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetPrintQEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetPrintQEnum *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetPrintQEnum_send()
 * dcerpc_rap_NetPrintQEnum_recv()
 * dcerpc_rap_NetPrintQEnum()
 */

struct tevent_req *dcerpc_rap_NetPrintQGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetPrintQGetInfo *r);
NTSTATUS dcerpc_rap_NetPrintQGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetPrintQGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetPrintQGetInfo *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetPrintQGetInfo_send()
 * dcerpc_rap_NetPrintQGetInfo_recv()
 * dcerpc_rap_NetPrintQGetInfo()
 */

struct tevent_req *dcerpc_rap_NetPrintJobPause_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetPrintJobPause *r);
NTSTATUS dcerpc_rap_NetPrintJobPause_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetPrintJobPause_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetPrintJobPause *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetPrintJobPause_send()
 * dcerpc_rap_NetPrintJobPause_recv()
 * dcerpc_rap_NetPrintJobPause()
 */

struct tevent_req *dcerpc_rap_NetPrintJobContinue_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetPrintJobContinue *r);
NTSTATUS dcerpc_rap_NetPrintJobContinue_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetPrintJobContinue_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetPrintJobContinue *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetPrintJobContinue_send()
 * dcerpc_rap_NetPrintJobContinue_recv()
 * dcerpc_rap_NetPrintJobContinue()
 */

struct tevent_req *dcerpc_rap_NetPrintJobDelete_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetPrintJobDelete *r);
NTSTATUS dcerpc_rap_NetPrintJobDelete_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetPrintJobDelete_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetPrintJobDelete *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetPrintJobDelete_send()
 * dcerpc_rap_NetPrintJobDelete_recv()
 * dcerpc_rap_NetPrintJobDelete()
 */

struct tevent_req *dcerpc_rap_NetPrintQueuePause_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetPrintQueuePause *r);
NTSTATUS dcerpc_rap_NetPrintQueuePause_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetPrintQueuePause_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetPrintQueuePause *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetPrintQueuePause_send()
 * dcerpc_rap_NetPrintQueuePause_recv()
 * dcerpc_rap_NetPrintQueuePause()
 */

struct tevent_req *dcerpc_rap_NetPrintQueueResume_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetPrintQueueResume *r);
NTSTATUS dcerpc_rap_NetPrintQueueResume_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetPrintQueueResume_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetPrintQueueResume *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetPrintQueueResume_send()
 * dcerpc_rap_NetPrintQueueResume_recv()
 * dcerpc_rap_NetPrintQueueResume()
 */

struct tevent_req *dcerpc_rap_NetPrintQueuePurge_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetPrintQueuePurge *r);
NTSTATUS dcerpc_rap_NetPrintQueuePurge_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetPrintQueuePurge_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetPrintQueuePurge *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetPrintQueuePurge_send()
 * dcerpc_rap_NetPrintQueuePurge_recv()
 * dcerpc_rap_NetPrintQueuePurge()
 */

struct tevent_req *dcerpc_rap_NetPrintJobEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetPrintJobEnum *r);
NTSTATUS dcerpc_rap_NetPrintJobEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetPrintJobEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetPrintJobEnum *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetPrintJobEnum_send()
 * dcerpc_rap_NetPrintJobEnum_recv()
 * dcerpc_rap_NetPrintJobEnum()
 */

struct tevent_req *dcerpc_rap_NetPrintJobGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetPrintJobGetInfo *r);
NTSTATUS dcerpc_rap_NetPrintJobGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetPrintJobGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetPrintJobGetInfo *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetPrintJobGetInfo_send()
 * dcerpc_rap_NetPrintJobGetInfo_recv()
 * dcerpc_rap_NetPrintJobGetInfo()
 */

struct tevent_req *dcerpc_rap_NetPrintJobSetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetPrintJobSetInfo *r);
NTSTATUS dcerpc_rap_NetPrintJobSetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetPrintJobSetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetPrintJobSetInfo *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetPrintJobSetInfo_send()
 * dcerpc_rap_NetPrintJobSetInfo_recv()
 * dcerpc_rap_NetPrintJobSetInfo()
 */

struct tevent_req *dcerpc_rap_NetPrintDestEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetPrintDestEnum *r);
NTSTATUS dcerpc_rap_NetPrintDestEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetPrintDestEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetPrintDestEnum *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetPrintDestEnum_send()
 * dcerpc_rap_NetPrintDestEnum_recv()
 * dcerpc_rap_NetPrintDestEnum()
 */

struct tevent_req *dcerpc_rap_NetPrintDestGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetPrintDestGetInfo *r);
NTSTATUS dcerpc_rap_NetPrintDestGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetPrintDestGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetPrintDestGetInfo *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetPrintDestGetInfo_send()
 * dcerpc_rap_NetPrintDestGetInfo_recv()
 * dcerpc_rap_NetPrintDestGetInfo()
 */

struct tevent_req *dcerpc_rap_NetUserPasswordSet2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetUserPasswordSet2 *r);
NTSTATUS dcerpc_rap_NetUserPasswordSet2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetUserPasswordSet2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetUserPasswordSet2 *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetUserPasswordSet2_send()
 * dcerpc_rap_NetUserPasswordSet2_recv()
 * dcerpc_rap_NetUserPasswordSet2()
 */

struct tevent_req *dcerpc_rap_NetOEMChangePassword_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetOEMChangePassword *r);
NTSTATUS dcerpc_rap_NetOEMChangePassword_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetOEMChangePassword_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetOEMChangePassword *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetOEMChangePassword_send()
 * dcerpc_rap_NetOEMChangePassword_recv()
 * dcerpc_rap_NetOEMChangePassword()
 */

struct tevent_req *dcerpc_rap_NetUserGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetUserGetInfo *r);
NTSTATUS dcerpc_rap_NetUserGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetUserGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetUserGetInfo *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetUserGetInfo_send()
 * dcerpc_rap_NetUserGetInfo_recv()
 * dcerpc_rap_NetUserGetInfo()
 */

struct tevent_req *dcerpc_rap_NetSessionEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetSessionEnum *r);
NTSTATUS dcerpc_rap_NetSessionEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetSessionEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetSessionEnum *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetSessionEnum_send()
 * dcerpc_rap_NetSessionEnum_recv()
 * dcerpc_rap_NetSessionEnum()
 */

struct tevent_req *dcerpc_rap_NetSessionGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetSessionGetInfo *r);
NTSTATUS dcerpc_rap_NetSessionGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetSessionGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetSessionGetInfo *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetSessionGetInfo_send()
 * dcerpc_rap_NetSessionGetInfo_recv()
 * dcerpc_rap_NetSessionGetInfo()
 */

struct tevent_req *dcerpc_rap_NetUserAdd_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetUserAdd *r);
NTSTATUS dcerpc_rap_NetUserAdd_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetUserAdd_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetUserAdd *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetUserAdd_send()
 * dcerpc_rap_NetUserAdd_recv()
 * dcerpc_rap_NetUserAdd()
 */

struct tevent_req *dcerpc_rap_NetUserDelete_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetUserDelete *r);
NTSTATUS dcerpc_rap_NetUserDelete_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetUserDelete_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetUserDelete *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetUserDelete_send()
 * dcerpc_rap_NetUserDelete_recv()
 * dcerpc_rap_NetUserDelete()
 */

struct tevent_req *dcerpc_rap_NetRemoteTOD_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct rap_NetRemoteTOD *r);
NTSTATUS dcerpc_rap_NetRemoteTOD_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_rap_NetRemoteTOD_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct rap_NetRemoteTOD *r);
/*
 * The following functions are skipped because
 * an [out] argument status is not a pointer or array:
 *
 * dcerpc_rap_NetRemoteTOD_send()
 * dcerpc_rap_NetRemoteTOD_recv()
 * dcerpc_rap_NetRemoteTOD()
 */

#endif /* _HEADER_RPC_rap */
