#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/eventlog6.h"
#ifndef _HEADER_RPC_eventlog6
#define _HEADER_RPC_eventlog6

extern const struct ndr_interface_table ndr_table_eventlog6;

struct tevent_req *dcerpc_eventlog6_EvtRpcRegisterRemoteSubscription_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcRegisterRemoteSubscription *r);
NTSTATUS dcerpc_eventlog6_EvtRpcRegisterRemoteSubscription_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcRegisterRemoteSubscription_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcRegisterRemoteSubscription *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcRegisterRemoteSubscription_send(TALLOC_CTX *mem_ctx,
									  struct tevent_context *ev,
									  struct dcerpc_binding_handle *h,
									  const char *_channelPath /* [in] [unique,range(0,MAX_RPC_CHANNEL_NAME_LENGTH),charset(UTF16)] */,
									  const char *_query /* [in] [charset(UTF16),range(1,MAX_RPC_QUERY_LENGTH),ref] */,
									  const char *_bookmarkXml /* [in] [unique,range(0,MAX_RPC_BOOKMARK_LENGTH),charset(UTF16)] */,
									  uint32_t _flags /* [in]  */,
									  struct policy_handle *_handle /* [out] [ref] */,
									  struct policy_handle *_control /* [out] [ref] */,
									  uint32_t *_queryChannelInfoSize /* [out] [ref] */,
									  struct eventlog6_EvtRpcQueryChannelInfo **_queryChannelInfo /* [out] [size_is(,*queryChannelInfoSize),range(0,MAX_RPC_QUERY_CHANNEL_SIZE),ref] */,
									  struct eventlog6_RpcInfo *_error /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcRegisterRemoteSubscription_recv(struct tevent_req *req,
								TALLOC_CTX *mem_ctx,
								WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcRegisterRemoteSubscription(struct dcerpc_binding_handle *h,
							   TALLOC_CTX *mem_ctx,
							   const char *_channelPath /* [in] [unique,range(0,MAX_RPC_CHANNEL_NAME_LENGTH),charset(UTF16)] */,
							   const char *_query /* [in] [charset(UTF16),range(1,MAX_RPC_QUERY_LENGTH),ref] */,
							   const char *_bookmarkXml /* [in] [unique,range(0,MAX_RPC_BOOKMARK_LENGTH),charset(UTF16)] */,
							   uint32_t _flags /* [in]  */,
							   struct policy_handle *_handle /* [out] [ref] */,
							   struct policy_handle *_control /* [out] [ref] */,
							   uint32_t *_queryChannelInfoSize /* [out] [ref] */,
							   struct eventlog6_EvtRpcQueryChannelInfo **_queryChannelInfo /* [out] [size_is(,*queryChannelInfoSize),range(0,MAX_RPC_QUERY_CHANNEL_SIZE),ref] */,
							   struct eventlog6_RpcInfo *_error /* [out] [ref] */,
							   WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcRemoteSubscriptionNextAsync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcRemoteSubscriptionNextAsync *r);
NTSTATUS dcerpc_eventlog6_EvtRpcRemoteSubscriptionNextAsync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcRemoteSubscriptionNextAsync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcRemoteSubscriptionNextAsync *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcRemoteSubscriptionNextAsync_send(TALLOC_CTX *mem_ctx,
									   struct tevent_context *ev,
									   struct dcerpc_binding_handle *h,
									   struct policy_handle *_handle /* [in] [ref] */,
									   uint32_t _numRequestedRecords /* [in]  */,
									   uint32_t _flags /* [in]  */,
									   uint32_t *_numActualRecords /* [out] [ref] */,
									   uint32_t **_eventDataIndices /* [out] [size_is(,*numActualRecords),range(0,MAX_RPC_RECORD_COUNT),ref] */,
									   uint32_t **_eventDataSizes /* [out] [size_is(,*numActualRecords),range(0,MAX_RPC_RECORD_COUNT),ref] */,
									   uint32_t *_resultBufferSize /* [out] [ref] */,
									   uint8_t **_resultBuffer /* [out] [size_is(,*resultBufferSize),ref,range(0,MAX_RPC_BATCH_SIZE)] */);
NTSTATUS dcerpc_eventlog6_EvtRpcRemoteSubscriptionNextAsync_recv(struct tevent_req *req,
								 TALLOC_CTX *mem_ctx,
								 WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcRemoteSubscriptionNextAsync(struct dcerpc_binding_handle *h,
							    TALLOC_CTX *mem_ctx,
							    struct policy_handle *_handle /* [in] [ref] */,
							    uint32_t _numRequestedRecords /* [in]  */,
							    uint32_t _flags /* [in]  */,
							    uint32_t *_numActualRecords /* [out] [ref] */,
							    uint32_t **_eventDataIndices /* [out] [size_is(,*numActualRecords),range(0,MAX_RPC_RECORD_COUNT),ref] */,
							    uint32_t **_eventDataSizes /* [out] [size_is(,*numActualRecords),range(0,MAX_RPC_RECORD_COUNT),ref] */,
							    uint32_t *_resultBufferSize /* [out] [ref] */,
							    uint8_t **_resultBuffer /* [out] [size_is(,*resultBufferSize),ref,range(0,MAX_RPC_BATCH_SIZE)] */,
							    WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcRemoteSubscriptionNext_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcRemoteSubscriptionNext *r);
NTSTATUS dcerpc_eventlog6_EvtRpcRemoteSubscriptionNext_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcRemoteSubscriptionNext_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcRemoteSubscriptionNext *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcRemoteSubscriptionNext_send(TALLOC_CTX *mem_ctx,
								      struct tevent_context *ev,
								      struct dcerpc_binding_handle *h,
								      struct policy_handle *_handle /* [in] [ref] */,
								      uint32_t _numRequestedRecords /* [in]  */,
								      uint32_t _timeOut /* [in]  */,
								      uint32_t _flags /* [in]  */,
								      uint32_t *_numActualRecords /* [out] [ref] */,
								      uint32_t **_eventDataIndices /* [out] [ref,range(0,MAX_RPC_RECORD_COUNT),size_is(,*numActualRecords)] */,
								      uint32_t **_eventDataSizes /* [out] [range(0,MAX_RPC_RECORD_COUNT),ref,size_is(,*numActualRecords)] */,
								      uint32_t *_resultBufferSize /* [out] [ref] */,
								      uint8_t **_resultBuffer /* [out] [size_is(,*resultBufferSize),range(0,MAX_RPC_BATCH_SIZE),ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcRemoteSubscriptionNext_recv(struct tevent_req *req,
							    TALLOC_CTX *mem_ctx,
							    WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcRemoteSubscriptionNext(struct dcerpc_binding_handle *h,
						       TALLOC_CTX *mem_ctx,
						       struct policy_handle *_handle /* [in] [ref] */,
						       uint32_t _numRequestedRecords /* [in]  */,
						       uint32_t _timeOut /* [in]  */,
						       uint32_t _flags /* [in]  */,
						       uint32_t *_numActualRecords /* [out] [ref] */,
						       uint32_t **_eventDataIndices /* [out] [ref,range(0,MAX_RPC_RECORD_COUNT),size_is(,*numActualRecords)] */,
						       uint32_t **_eventDataSizes /* [out] [range(0,MAX_RPC_RECORD_COUNT),ref,size_is(,*numActualRecords)] */,
						       uint32_t *_resultBufferSize /* [out] [ref] */,
						       uint8_t **_resultBuffer /* [out] [size_is(,*resultBufferSize),range(0,MAX_RPC_BATCH_SIZE),ref] */,
						       WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcRemoteSubscriptionWaitAsync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcRemoteSubscriptionWaitAsync *r);
NTSTATUS dcerpc_eventlog6_EvtRpcRemoteSubscriptionWaitAsync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcRemoteSubscriptionWaitAsync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcRemoteSubscriptionWaitAsync *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcRemoteSubscriptionWaitAsync_send(TALLOC_CTX *mem_ctx,
									   struct tevent_context *ev,
									   struct dcerpc_binding_handle *h,
									   struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcRemoteSubscriptionWaitAsync_recv(struct tevent_req *req,
								 TALLOC_CTX *mem_ctx,
								 WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcRemoteSubscriptionWaitAsync(struct dcerpc_binding_handle *h,
							    TALLOC_CTX *mem_ctx,
							    struct policy_handle *_handle /* [in] [ref] */,
							    WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcRegisterControllableOperation_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcRegisterControllableOperation *r);
NTSTATUS dcerpc_eventlog6_EvtRpcRegisterControllableOperation_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcRegisterControllableOperation_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcRegisterControllableOperation *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcRegisterControllableOperation_send(TALLOC_CTX *mem_ctx,
									     struct tevent_context *ev,
									     struct dcerpc_binding_handle *h,
									     struct policy_handle *_handle /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcRegisterControllableOperation_recv(struct tevent_req *req,
								   TALLOC_CTX *mem_ctx,
								   WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcRegisterControllableOperation(struct dcerpc_binding_handle *h,
							      TALLOC_CTX *mem_ctx,
							      struct policy_handle *_handle /* [out] [ref] */,
							      WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcRegisterLogQuery_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcRegisterLogQuery *r);
NTSTATUS dcerpc_eventlog6_EvtRpcRegisterLogQuery_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcRegisterLogQuery_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcRegisterLogQuery *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcRegisterLogQuery_send(TALLOC_CTX *mem_ctx,
								struct tevent_context *ev,
								struct dcerpc_binding_handle *h,
								const char *_path /* [in] [charset(UTF16),range(0,MAX_RPC_CHANNEL_PATH_LENGTH),unique] */,
								const char *_query /* [in] [charset(UTF16),range(1,MAX_RPC_QUERY_LENGTH),ref] */,
								uint32_t _flags /* [in]  */,
								struct policy_handle *_handle /* [out] [ref] */,
								struct policy_handle *_opControl /* [out] [ref] */,
								uint32_t *_queryChannelInfoSize /* [out] [ref] */,
								struct eventlog6_EvtRpcQueryChannelInfo **_queryChannelInfo /* [out] [size_is(,*queryChannelInfoSize),range(0,MAX_RPC_QUERY_CHANNEL_SIZE),ref] */,
								struct eventlog6_RpcInfo *_error /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcRegisterLogQuery_recv(struct tevent_req *req,
						      TALLOC_CTX *mem_ctx,
						      WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcRegisterLogQuery(struct dcerpc_binding_handle *h,
						 TALLOC_CTX *mem_ctx,
						 const char *_path /* [in] [charset(UTF16),range(0,MAX_RPC_CHANNEL_PATH_LENGTH),unique] */,
						 const char *_query /* [in] [charset(UTF16),range(1,MAX_RPC_QUERY_LENGTH),ref] */,
						 uint32_t _flags /* [in]  */,
						 struct policy_handle *_handle /* [out] [ref] */,
						 struct policy_handle *_opControl /* [out] [ref] */,
						 uint32_t *_queryChannelInfoSize /* [out] [ref] */,
						 struct eventlog6_EvtRpcQueryChannelInfo **_queryChannelInfo /* [out] [size_is(,*queryChannelInfoSize),range(0,MAX_RPC_QUERY_CHANNEL_SIZE),ref] */,
						 struct eventlog6_RpcInfo *_error /* [out] [ref] */,
						 WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcClearLog_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcClearLog *r);
NTSTATUS dcerpc_eventlog6_EvtRpcClearLog_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcClearLog_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcClearLog *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcClearLog_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct policy_handle *_control /* [in] [ref] */,
							const char *_channelPath /* [in] [charset(UTF16),ref,range(0,MAX_RPC_CHANNEL_NAME_LENGTH)] */,
							const char *_backupPath /* [in] [unique,range(0,MAX_RPC_FILE_PATH_LENGTH),charset(UTF16)] */,
							uint32_t _flags /* [in]  */,
							struct eventlog6_RpcInfo *_error /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcClearLog_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcClearLog(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct policy_handle *_control /* [in] [ref] */,
					 const char *_channelPath /* [in] [charset(UTF16),ref,range(0,MAX_RPC_CHANNEL_NAME_LENGTH)] */,
					 const char *_backupPath /* [in] [unique,range(0,MAX_RPC_FILE_PATH_LENGTH),charset(UTF16)] */,
					 uint32_t _flags /* [in]  */,
					 struct eventlog6_RpcInfo *_error /* [out] [ref] */,
					 WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcExportLog_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcExportLog *r);
NTSTATUS dcerpc_eventlog6_EvtRpcExportLog_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcExportLog_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcExportLog *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcExportLog_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_control /* [in] [ref] */,
							 const char *_channelPath /* [in] [unique,range(0,MAX_RPC_CHANNEL_NAME_LENGTH),charset(UTF16)] */,
							 const char *_query /* [in] [charset(UTF16),range(1,MAX_RPC_QUERY_LENGTH),ref] */,
							 const char *_backupPath /* [in] [ref,range(1,MAX_RPC_FILE_PATH_LENGTH),charset(UTF16)] */,
							 uint32_t _flags /* [in]  */,
							 struct eventlog6_RpcInfo *_error /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcExportLog_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcExportLog(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_control /* [in] [ref] */,
					  const char *_channelPath /* [in] [unique,range(0,MAX_RPC_CHANNEL_NAME_LENGTH),charset(UTF16)] */,
					  const char *_query /* [in] [charset(UTF16),range(1,MAX_RPC_QUERY_LENGTH),ref] */,
					  const char *_backupPath /* [in] [ref,range(1,MAX_RPC_FILE_PATH_LENGTH),charset(UTF16)] */,
					  uint32_t _flags /* [in]  */,
					  struct eventlog6_RpcInfo *_error /* [out] [ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcLocalizeExportLog_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcLocalizeExportLog *r);
NTSTATUS dcerpc_eventlog6_EvtRpcLocalizeExportLog_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcLocalizeExportLog_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcLocalizeExportLog *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcLocalizeExportLog_send(TALLOC_CTX *mem_ctx,
								 struct tevent_context *ev,
								 struct dcerpc_binding_handle *h,
								 struct policy_handle *_control /* [in] [ref] */,
								 const char *_logFilePath /* [in] [charset(UTF16),ref,range(1,MAX_RPC_FILE_PATH_LENGTH)] */,
								 uint32_t _locale /* [in]  */,
								 uint32_t _flags /* [in]  */,
								 struct eventlog6_RpcInfo *_error /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcLocalizeExportLog_recv(struct tevent_req *req,
						       TALLOC_CTX *mem_ctx,
						       WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcLocalizeExportLog(struct dcerpc_binding_handle *h,
						  TALLOC_CTX *mem_ctx,
						  struct policy_handle *_control /* [in] [ref] */,
						  const char *_logFilePath /* [in] [charset(UTF16),ref,range(1,MAX_RPC_FILE_PATH_LENGTH)] */,
						  uint32_t _locale /* [in]  */,
						  uint32_t _flags /* [in]  */,
						  struct eventlog6_RpcInfo *_error /* [out] [ref] */,
						  WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcMessageRender_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcMessageRender *r);
NTSTATUS dcerpc_eventlog6_EvtRpcMessageRender_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcMessageRender_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcMessageRender *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcMessageRender_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     struct policy_handle *_pubCfgObj /* [in] [ref] */,
							     uint32_t _sizeEventId /* [in] [range(1,MAX_RPC_EVENT_ID_SIZE)] */,
							     uint8_t *_eventId /* [in] [ref,size_is(sizeEventId)] */,
							     uint32_t _messageId /* [in]  */,
							     struct eventlog6_EvtRpcVariantList *_values /* [in] [ref] */,
							     uint32_t _flags /* [in]  */,
							     uint32_t _maxSizeString /* [in]  */,
							     uint32_t *_actualSizeString /* [out] [ref] */,
							     uint32_t *_neededSizeString /* [out] [ref] */,
							     uint8_t **_string /* [out] [size_is(,*actualSizeString),ref,range(0,MAX_RPC_RENDERED_STRING_SIZE)] */,
							     struct eventlog6_RpcInfo *_error /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcMessageRender_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcMessageRender(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      struct policy_handle *_pubCfgObj /* [in] [ref] */,
					      uint32_t _sizeEventId /* [in] [range(1,MAX_RPC_EVENT_ID_SIZE)] */,
					      uint8_t *_eventId /* [in] [ref,size_is(sizeEventId)] */,
					      uint32_t _messageId /* [in]  */,
					      struct eventlog6_EvtRpcVariantList *_values /* [in] [ref] */,
					      uint32_t _flags /* [in]  */,
					      uint32_t _maxSizeString /* [in]  */,
					      uint32_t *_actualSizeString /* [out] [ref] */,
					      uint32_t *_neededSizeString /* [out] [ref] */,
					      uint8_t **_string /* [out] [size_is(,*actualSizeString),ref,range(0,MAX_RPC_RENDERED_STRING_SIZE)] */,
					      struct eventlog6_RpcInfo *_error /* [out] [ref] */,
					      WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcMessageRenderDefault_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcMessageRenderDefault *r);
NTSTATUS dcerpc_eventlog6_EvtRpcMessageRenderDefault_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcMessageRenderDefault_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcMessageRenderDefault *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcMessageRenderDefault_send(TALLOC_CTX *mem_ctx,
								    struct tevent_context *ev,
								    struct dcerpc_binding_handle *h,
								    uint32_t _sizeEventId /* [in] [range(1,MAX_RPC_EVENT_ID_SIZE)] */,
								    uint8_t *_eventId /* [in] [ref,size_is(sizeEventId)] */,
								    uint32_t _messageId /* [in]  */,
								    struct eventlog6_EvtRpcVariantList *_values /* [in] [ref] */,
								    uint32_t _flags /* [in]  */,
								    uint32_t _maxSizeString /* [in]  */,
								    uint32_t *_actualSizeString /* [out] [ref] */,
								    uint32_t *_neededSizeString /* [out] [ref] */,
								    uint8_t **_string /* [out] [ref,range(0,MAX_RPC_RENDERED_STRING_SIZE),size_is(,*actualSizeString)] */,
								    struct eventlog6_RpcInfo *_error /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcMessageRenderDefault_recv(struct tevent_req *req,
							  TALLOC_CTX *mem_ctx,
							  WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcMessageRenderDefault(struct dcerpc_binding_handle *h,
						     TALLOC_CTX *mem_ctx,
						     uint32_t _sizeEventId /* [in] [range(1,MAX_RPC_EVENT_ID_SIZE)] */,
						     uint8_t *_eventId /* [in] [ref,size_is(sizeEventId)] */,
						     uint32_t _messageId /* [in]  */,
						     struct eventlog6_EvtRpcVariantList *_values /* [in] [ref] */,
						     uint32_t _flags /* [in]  */,
						     uint32_t _maxSizeString /* [in]  */,
						     uint32_t *_actualSizeString /* [out] [ref] */,
						     uint32_t *_neededSizeString /* [out] [ref] */,
						     uint8_t **_string /* [out] [ref,range(0,MAX_RPC_RENDERED_STRING_SIZE),size_is(,*actualSizeString)] */,
						     struct eventlog6_RpcInfo *_error /* [out] [ref] */,
						     WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcQueryNext_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcQueryNext *r);
NTSTATUS dcerpc_eventlog6_EvtRpcQueryNext_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcQueryNext_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcQueryNext *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcQueryNext_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_logQuery /* [in] [ref] */,
							 uint32_t _numRequestedRecords /* [in]  */,
							 uint32_t _timeOutEnd /* [in]  */,
							 uint32_t _flags /* [in]  */,
							 uint32_t *_numActualRecords /* [out] [ref] */,
							 uint32_t **_eventDataIndices /* [out] [ref,range(0,MAX_RPC_RECORD_COUNT),size_is(,*numActualRecords)] */,
							 uint32_t **_eventDataSizes /* [out] [range(0,MAX_RPC_RECORD_COUNT),ref,size_is(,*numActualRecords)] */,
							 uint32_t *_resultBufferSize /* [out] [ref] */,
							 uint8_t **_resultBuffer /* [out] [size_is(,*resultBufferSize),ref,range(0,MAX_RPC_BATCH_SIZE)] */);
NTSTATUS dcerpc_eventlog6_EvtRpcQueryNext_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcQueryNext(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_logQuery /* [in] [ref] */,
					  uint32_t _numRequestedRecords /* [in]  */,
					  uint32_t _timeOutEnd /* [in]  */,
					  uint32_t _flags /* [in]  */,
					  uint32_t *_numActualRecords /* [out] [ref] */,
					  uint32_t **_eventDataIndices /* [out] [ref,range(0,MAX_RPC_RECORD_COUNT),size_is(,*numActualRecords)] */,
					  uint32_t **_eventDataSizes /* [out] [range(0,MAX_RPC_RECORD_COUNT),ref,size_is(,*numActualRecords)] */,
					  uint32_t *_resultBufferSize /* [out] [ref] */,
					  uint8_t **_resultBuffer /* [out] [size_is(,*resultBufferSize),ref,range(0,MAX_RPC_BATCH_SIZE)] */,
					  WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcQuerySeek_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcQuerySeek *r);
NTSTATUS dcerpc_eventlog6_EvtRpcQuerySeek_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcQuerySeek_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcQuerySeek *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcQuerySeek_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct policy_handle *_logQuery /* [in] [ref] */,
							 uint64_t _pos /* [in]  */,
							 const char *_bookmarkXml /* [in] [charset(UTF16),range(0,MAX_RPC_BOOKMARK_LENGTH),unique] */,
							 uint32_t _timeOut /* [in]  */,
							 uint32_t _flags /* [in]  */,
							 struct eventlog6_RpcInfo *_error /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcQuerySeek_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcQuerySeek(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct policy_handle *_logQuery /* [in] [ref] */,
					  uint64_t _pos /* [in]  */,
					  const char *_bookmarkXml /* [in] [charset(UTF16),range(0,MAX_RPC_BOOKMARK_LENGTH),unique] */,
					  uint32_t _timeOut /* [in]  */,
					  uint32_t _flags /* [in]  */,
					  struct eventlog6_RpcInfo *_error /* [out] [ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcClose_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcClose *r);
NTSTATUS dcerpc_eventlog6_EvtRpcClose_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcClose_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcClose *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcClose_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct policy_handle **_handle /* [in,out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcClose_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcClose(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct policy_handle **_handle /* [in,out] [ref] */,
				      WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcCancel_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcCancel *r);
NTSTATUS dcerpc_eventlog6_EvtRpcCancel_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcCancel_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcCancel *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcCancel_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct policy_handle *_handle /* [in] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcCancel_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcCancel(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct policy_handle *_handle /* [in] [ref] */,
				       WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcAssertConfig_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcAssertConfig *r);
NTSTATUS dcerpc_eventlog6_EvtRpcAssertConfig_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcAssertConfig_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcAssertConfig *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcAssertConfig_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct dcerpc_binding_handle *h,
							    const char *_path /* [in] [range(1,MAX_RPC_CHANNEL_NAME_LENGTH),ref,charset(UTF16)] */,
							    uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_eventlog6_EvtRpcAssertConfig_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcAssertConfig(struct dcerpc_binding_handle *h,
					     TALLOC_CTX *mem_ctx,
					     const char *_path /* [in] [range(1,MAX_RPC_CHANNEL_NAME_LENGTH),ref,charset(UTF16)] */,
					     uint32_t _flags /* [in]  */,
					     WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcRetractConfig_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcRetractConfig *r);
NTSTATUS dcerpc_eventlog6_EvtRpcRetractConfig_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcRetractConfig_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcRetractConfig *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcRetractConfig_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     const char *_path /* [in] [ref,range(1,MAX_RPC_CHANNEL_NAME_LENGTH),charset(UTF16)] */,
							     uint32_t _flags /* [in]  */);
NTSTATUS dcerpc_eventlog6_EvtRpcRetractConfig_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcRetractConfig(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      const char *_path /* [in] [ref,range(1,MAX_RPC_CHANNEL_NAME_LENGTH),charset(UTF16)] */,
					      uint32_t _flags /* [in]  */,
					      WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcOpenLogHandle_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcOpenLogHandle *r);
NTSTATUS dcerpc_eventlog6_EvtRpcOpenLogHandle_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcOpenLogHandle_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcOpenLogHandle *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcOpenLogHandle_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     const char *_channel /* [in] [charset(UTF16),range(1,MAX_RPC_CHANNEL_NAME_LENGTH),ref] */,
							     uint32_t _flags /* [in]  */,
							     struct policy_handle *_handle /* [out] [ref] */,
							     struct eventlog6_RpcInfo *_error /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcOpenLogHandle_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcOpenLogHandle(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      const char *_channel /* [in] [charset(UTF16),range(1,MAX_RPC_CHANNEL_NAME_LENGTH),ref] */,
					      uint32_t _flags /* [in]  */,
					      struct policy_handle *_handle /* [out] [ref] */,
					      struct eventlog6_RpcInfo *_error /* [out] [ref] */,
					      WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcGetLogFileInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcGetLogFileInfo *r);
NTSTATUS dcerpc_eventlog6_EvtRpcGetLogFileInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcGetLogFileInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcGetLogFileInfo *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcGetLogFileInfo_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      struct policy_handle *_logHandle /* [in] [ref] */,
							      uint32_t _propertyId /* [in]  */,
							      uint32_t _propertyValueBufferSize /* [in] [range(0,MAX_RPC_PROPERTY_BUFFER_SIZE)] */,
							      uint8_t *_propertyValueBuffer /* [out] [ref,size_is(propertyValueBufferSize)] */,
							      uint32_t *_propertyValueBufferLength /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcGetLogFileInfo_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcGetLogFileInfo(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       struct policy_handle *_logHandle /* [in] [ref] */,
					       uint32_t _propertyId /* [in]  */,
					       uint32_t _propertyValueBufferSize /* [in] [range(0,MAX_RPC_PROPERTY_BUFFER_SIZE)] */,
					       uint8_t *_propertyValueBuffer /* [out] [ref,size_is(propertyValueBufferSize)] */,
					       uint32_t *_propertyValueBufferLength /* [out] [ref] */,
					       WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcGetChannelList_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcGetChannelList *r);
NTSTATUS dcerpc_eventlog6_EvtRpcGetChannelList_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcGetChannelList_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcGetChannelList *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcGetChannelList_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      uint32_t _flags /* [in]  */,
							      uint32_t *_numChannelPaths /* [out] [ref] */,
							      const char ***_channelPaths /* [out] [size_is(,*numChannelPaths),ref,range(0,MAX_RPC_CHANNEL_COUNT),charset(UTF16)] */);
NTSTATUS dcerpc_eventlog6_EvtRpcGetChannelList_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx,
						    WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcGetChannelList(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       uint32_t _flags /* [in]  */,
					       uint32_t *_numChannelPaths /* [out] [ref] */,
					       const char ***_channelPaths /* [out] [size_is(,*numChannelPaths),ref,range(0,MAX_RPC_CHANNEL_COUNT),charset(UTF16)] */,
					       WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcGetChannelConfig_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcGetChannelConfig *r);
NTSTATUS dcerpc_eventlog6_EvtRpcGetChannelConfig_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcGetChannelConfig_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcGetChannelConfig *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcGetChannelConfig_send(TALLOC_CTX *mem_ctx,
								struct tevent_context *ev,
								struct dcerpc_binding_handle *h,
								const char *_channelPath /* [in] [charset(UTF16),ref,range(1,MAX_RPC_CHANNEL_NAME_LENGTH)] */,
								uint32_t _flags /* [in]  */,
								struct eventlog6_EvtRpcVariantList *_props /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcGetChannelConfig_recv(struct tevent_req *req,
						      TALLOC_CTX *mem_ctx,
						      WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcGetChannelConfig(struct dcerpc_binding_handle *h,
						 TALLOC_CTX *mem_ctx,
						 const char *_channelPath /* [in] [charset(UTF16),ref,range(1,MAX_RPC_CHANNEL_NAME_LENGTH)] */,
						 uint32_t _flags /* [in]  */,
						 struct eventlog6_EvtRpcVariantList *_props /* [out] [ref] */,
						 WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcPutChannelConfig_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcPutChannelConfig *r);
NTSTATUS dcerpc_eventlog6_EvtRpcPutChannelConfig_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcPutChannelConfig_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcPutChannelConfig *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcPutChannelConfig_send(TALLOC_CTX *mem_ctx,
								struct tevent_context *ev,
								struct dcerpc_binding_handle *h,
								const char *_channelPath /* [in] [ref,range(1,MAX_RPC_CHANNEL_NAME_LENGTH),charset(UTF16)] */,
								uint32_t _flags /* [in]  */,
								struct eventlog6_EvtRpcVariantList *_props /* [in] [ref] */,
								struct eventlog6_RpcInfo *_error /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcPutChannelConfig_recv(struct tevent_req *req,
						      TALLOC_CTX *mem_ctx,
						      WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcPutChannelConfig(struct dcerpc_binding_handle *h,
						 TALLOC_CTX *mem_ctx,
						 const char *_channelPath /* [in] [ref,range(1,MAX_RPC_CHANNEL_NAME_LENGTH),charset(UTF16)] */,
						 uint32_t _flags /* [in]  */,
						 struct eventlog6_EvtRpcVariantList *_props /* [in] [ref] */,
						 struct eventlog6_RpcInfo *_error /* [out] [ref] */,
						 WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcGetPublisherList_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcGetPublisherList *r);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherList_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherList_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcGetPublisherList *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcGetPublisherList_send(TALLOC_CTX *mem_ctx,
								struct tevent_context *ev,
								struct dcerpc_binding_handle *h,
								uint32_t _flags /* [in]  */,
								uint32_t *_numPublisherIds /* [out] [ref] */,
								const char ***_publisherIds /* [out] [charset(UTF16),size_is(,*numPublisherIds),range(0,MAX_RPC_PUBLISHER_COUNT),ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherList_recv(struct tevent_req *req,
						      TALLOC_CTX *mem_ctx,
						      WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherList(struct dcerpc_binding_handle *h,
						 TALLOC_CTX *mem_ctx,
						 uint32_t _flags /* [in]  */,
						 uint32_t *_numPublisherIds /* [out] [ref] */,
						 const char ***_publisherIds /* [out] [charset(UTF16),size_is(,*numPublisherIds),range(0,MAX_RPC_PUBLISHER_COUNT),ref] */,
						 WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcGetPublisherListForChannel_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcGetPublisherListForChannel *r);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherListForChannel_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherListForChannel_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcGetPublisherListForChannel *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcGetPublisherListForChannel_send(TALLOC_CTX *mem_ctx,
									  struct tevent_context *ev,
									  struct dcerpc_binding_handle *h,
									  uint16_t *_channelName /* [in] [ref] */,
									  uint32_t _flags /* [in]  */,
									  uint32_t *_numPublisherIds /* [out] [ref] */,
									  const char ***_publisherIds /* [out] [charset(UTF16),range(0,MAX_RPC_PUBLISHER_COUNT),ref,size_is(,*numPublisherIds)] */);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherListForChannel_recv(struct tevent_req *req,
								TALLOC_CTX *mem_ctx,
								WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherListForChannel(struct dcerpc_binding_handle *h,
							   TALLOC_CTX *mem_ctx,
							   uint16_t *_channelName /* [in] [ref] */,
							   uint32_t _flags /* [in]  */,
							   uint32_t *_numPublisherIds /* [out] [ref] */,
							   const char ***_publisherIds /* [out] [charset(UTF16),range(0,MAX_RPC_PUBLISHER_COUNT),ref,size_is(,*numPublisherIds)] */,
							   WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcGetPublisherMetadata_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcGetPublisherMetadata *r);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherMetadata_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherMetadata_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcGetPublisherMetadata *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcGetPublisherMetadata_send(TALLOC_CTX *mem_ctx,
								    struct tevent_context *ev,
								    struct dcerpc_binding_handle *h,
								    const char *_publisherId /* [in] [range(0,MAX_RPC_PUBLISHER_ID_LENGTH),unique,charset(UTF16)] */,
								    const char *_logFilePath /* [in] [unique,range(0,MAX_RPC_FILE_PATH_LENGTH),charset(UTF16)] */,
								    uint32_t _locale /* [in]  */,
								    uint32_t _flags /* [in]  */,
								    struct eventlog6_EvtRpcVariantList *_pubMetadataProps /* [out] [ref] */,
								    struct policy_handle *_pubMetadata /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherMetadata_recv(struct tevent_req *req,
							  TALLOC_CTX *mem_ctx,
							  WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherMetadata(struct dcerpc_binding_handle *h,
						     TALLOC_CTX *mem_ctx,
						     const char *_publisherId /* [in] [range(0,MAX_RPC_PUBLISHER_ID_LENGTH),unique,charset(UTF16)] */,
						     const char *_logFilePath /* [in] [unique,range(0,MAX_RPC_FILE_PATH_LENGTH),charset(UTF16)] */,
						     uint32_t _locale /* [in]  */,
						     uint32_t _flags /* [in]  */,
						     struct eventlog6_EvtRpcVariantList *_pubMetadataProps /* [out] [ref] */,
						     struct policy_handle *_pubMetadata /* [out] [ref] */,
						     WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcGetPublisherResourceMetadata_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcGetPublisherResourceMetadata *r);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherResourceMetadata_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherResourceMetadata_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcGetPublisherResourceMetadata *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcGetPublisherResourceMetadata_send(TALLOC_CTX *mem_ctx,
									    struct tevent_context *ev,
									    struct dcerpc_binding_handle *h,
									    struct policy_handle *_handle /* [in] [ref] */,
									    uint32_t _propertyId /* [in]  */,
									    uint32_t _flags /* [in]  */,
									    struct eventlog6_EvtRpcVariantList *_pubMetadataProps /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherResourceMetadata_recv(struct tevent_req *req,
								  TALLOC_CTX *mem_ctx,
								  WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcGetPublisherResourceMetadata(struct dcerpc_binding_handle *h,
							     TALLOC_CTX *mem_ctx,
							     struct policy_handle *_handle /* [in] [ref] */,
							     uint32_t _propertyId /* [in]  */,
							     uint32_t _flags /* [in]  */,
							     struct eventlog6_EvtRpcVariantList *_pubMetadataProps /* [out] [ref] */,
							     WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcGetEventMetadataEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcGetEventMetadataEnum *r);
NTSTATUS dcerpc_eventlog6_EvtRpcGetEventMetadataEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcGetEventMetadataEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcGetEventMetadataEnum *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcGetEventMetadataEnum_send(TALLOC_CTX *mem_ctx,
								    struct tevent_context *ev,
								    struct dcerpc_binding_handle *h,
								    struct policy_handle *_pubMetadata /* [in] [ref] */,
								    uint32_t _flags /* [in]  */,
								    const char *_reservedForFilter /* [in] [charset(UTF16),unique,range(0,MAX_RPC_FILTER_LENGTH)] */,
								    struct policy_handle *_eventMetaDataEnum /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcGetEventMetadataEnum_recv(struct tevent_req *req,
							  TALLOC_CTX *mem_ctx,
							  WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcGetEventMetadataEnum(struct dcerpc_binding_handle *h,
						     TALLOC_CTX *mem_ctx,
						     struct policy_handle *_pubMetadata /* [in] [ref] */,
						     uint32_t _flags /* [in]  */,
						     const char *_reservedForFilter /* [in] [charset(UTF16),unique,range(0,MAX_RPC_FILTER_LENGTH)] */,
						     struct policy_handle *_eventMetaDataEnum /* [out] [ref] */,
						     WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcGetNextEventMetadata_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcGetNextEventMetadata *r);
NTSTATUS dcerpc_eventlog6_EvtRpcGetNextEventMetadata_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcGetNextEventMetadata_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcGetNextEventMetadata *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcGetNextEventMetadata_send(TALLOC_CTX *mem_ctx,
								    struct tevent_context *ev,
								    struct dcerpc_binding_handle *h,
								    struct policy_handle *_eventMetaDataEnum /* [in] [ref] */,
								    uint32_t _flags /* [in]  */,
								    uint32_t _numRequested /* [in]  */,
								    uint32_t *_numReturned /* [out] [ref] */,
								    struct eventlog6_EvtRpcVariantList **_eventMetadataInstances /* [out] [ref,range(0,MAX_RPC_EVENT_METADATA_COUNT),size_is(,*numReturned)] */);
NTSTATUS dcerpc_eventlog6_EvtRpcGetNextEventMetadata_recv(struct tevent_req *req,
							  TALLOC_CTX *mem_ctx,
							  WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcGetNextEventMetadata(struct dcerpc_binding_handle *h,
						     TALLOC_CTX *mem_ctx,
						     struct policy_handle *_eventMetaDataEnum /* [in] [ref] */,
						     uint32_t _flags /* [in]  */,
						     uint32_t _numRequested /* [in]  */,
						     uint32_t *_numReturned /* [out] [ref] */,
						     struct eventlog6_EvtRpcVariantList **_eventMetadataInstances /* [out] [ref,range(0,MAX_RPC_EVENT_METADATA_COUNT),size_is(,*numReturned)] */,
						     WERROR *result);

struct tevent_req *dcerpc_eventlog6_EvtRpcGetClassicLogDisplayName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct eventlog6_EvtRpcGetClassicLogDisplayName *r);
NTSTATUS dcerpc_eventlog6_EvtRpcGetClassicLogDisplayName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_eventlog6_EvtRpcGetClassicLogDisplayName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct eventlog6_EvtRpcGetClassicLogDisplayName *r);
struct tevent_req *dcerpc_eventlog6_EvtRpcGetClassicLogDisplayName_send(TALLOC_CTX *mem_ctx,
									struct tevent_context *ev,
									struct dcerpc_binding_handle *h,
									const char *_logName /* [in] [charset(UTF16),range(1,MAX_RPC_CHANNEL_NAME_LENGTH),ref] */,
									uint32_t _locale /* [in]  */,
									uint32_t _flags /* [in]  */,
									uint16_t **_displayName /* [out] [ref] */);
NTSTATUS dcerpc_eventlog6_EvtRpcGetClassicLogDisplayName_recv(struct tevent_req *req,
							      TALLOC_CTX *mem_ctx,
							      WERROR *result);
NTSTATUS dcerpc_eventlog6_EvtRpcGetClassicLogDisplayName(struct dcerpc_binding_handle *h,
							 TALLOC_CTX *mem_ctx,
							 const char *_logName /* [in] [charset(UTF16),range(1,MAX_RPC_CHANNEL_NAME_LENGTH),ref] */,
							 uint32_t _locale /* [in]  */,
							 uint32_t _flags /* [in]  */,
							 uint16_t **_displayName /* [out] [ref] */,
							 WERROR *result);

#endif /* _HEADER_RPC_eventlog6 */
