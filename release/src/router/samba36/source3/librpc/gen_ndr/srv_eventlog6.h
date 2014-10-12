#include "librpc/gen_ndr/ndr_eventlog6.h"
#ifndef __SRV_EVENTLOG6__
#define __SRV_EVENTLOG6__
WERROR _eventlog6_EvtRpcRegisterRemoteSubscription(struct pipes_struct *p, struct eventlog6_EvtRpcRegisterRemoteSubscription *r);
WERROR _eventlog6_EvtRpcRemoteSubscriptionNextAsync(struct pipes_struct *p, struct eventlog6_EvtRpcRemoteSubscriptionNextAsync *r);
WERROR _eventlog6_EvtRpcRemoteSubscriptionNext(struct pipes_struct *p, struct eventlog6_EvtRpcRemoteSubscriptionNext *r);
WERROR _eventlog6_EvtRpcRemoteSubscriptionWaitAsync(struct pipes_struct *p, struct eventlog6_EvtRpcRemoteSubscriptionWaitAsync *r);
WERROR _eventlog6_EvtRpcRegisterControllableOperation(struct pipes_struct *p, struct eventlog6_EvtRpcRegisterControllableOperation *r);
WERROR _eventlog6_EvtRpcRegisterLogQuery(struct pipes_struct *p, struct eventlog6_EvtRpcRegisterLogQuery *r);
WERROR _eventlog6_EvtRpcClearLog(struct pipes_struct *p, struct eventlog6_EvtRpcClearLog *r);
WERROR _eventlog6_EvtRpcExportLog(struct pipes_struct *p, struct eventlog6_EvtRpcExportLog *r);
WERROR _eventlog6_EvtRpcLocalizeExportLog(struct pipes_struct *p, struct eventlog6_EvtRpcLocalizeExportLog *r);
WERROR _eventlog6_EvtRpcMessageRender(struct pipes_struct *p, struct eventlog6_EvtRpcMessageRender *r);
WERROR _eventlog6_EvtRpcMessageRenderDefault(struct pipes_struct *p, struct eventlog6_EvtRpcMessageRenderDefault *r);
WERROR _eventlog6_EvtRpcQueryNext(struct pipes_struct *p, struct eventlog6_EvtRpcQueryNext *r);
WERROR _eventlog6_EvtRpcQuerySeek(struct pipes_struct *p, struct eventlog6_EvtRpcQuerySeek *r);
WERROR _eventlog6_EvtRpcClose(struct pipes_struct *p, struct eventlog6_EvtRpcClose *r);
WERROR _eventlog6_EvtRpcCancel(struct pipes_struct *p, struct eventlog6_EvtRpcCancel *r);
WERROR _eventlog6_EvtRpcAssertConfig(struct pipes_struct *p, struct eventlog6_EvtRpcAssertConfig *r);
WERROR _eventlog6_EvtRpcRetractConfig(struct pipes_struct *p, struct eventlog6_EvtRpcRetractConfig *r);
WERROR _eventlog6_EvtRpcOpenLogHandle(struct pipes_struct *p, struct eventlog6_EvtRpcOpenLogHandle *r);
WERROR _eventlog6_EvtRpcGetLogFileInfo(struct pipes_struct *p, struct eventlog6_EvtRpcGetLogFileInfo *r);
WERROR _eventlog6_EvtRpcGetChannelList(struct pipes_struct *p, struct eventlog6_EvtRpcGetChannelList *r);
WERROR _eventlog6_EvtRpcGetChannelConfig(struct pipes_struct *p, struct eventlog6_EvtRpcGetChannelConfig *r);
WERROR _eventlog6_EvtRpcPutChannelConfig(struct pipes_struct *p, struct eventlog6_EvtRpcPutChannelConfig *r);
WERROR _eventlog6_EvtRpcGetPublisherList(struct pipes_struct *p, struct eventlog6_EvtRpcGetPublisherList *r);
WERROR _eventlog6_EvtRpcGetPublisherListForChannel(struct pipes_struct *p, struct eventlog6_EvtRpcGetPublisherListForChannel *r);
WERROR _eventlog6_EvtRpcGetPublisherMetadata(struct pipes_struct *p, struct eventlog6_EvtRpcGetPublisherMetadata *r);
WERROR _eventlog6_EvtRpcGetPublisherResourceMetadata(struct pipes_struct *p, struct eventlog6_EvtRpcGetPublisherResourceMetadata *r);
WERROR _eventlog6_EvtRpcGetEventMetadataEnum(struct pipes_struct *p, struct eventlog6_EvtRpcGetEventMetadataEnum *r);
WERROR _eventlog6_EvtRpcGetNextEventMetadata(struct pipes_struct *p, struct eventlog6_EvtRpcGetNextEventMetadata *r);
WERROR _eventlog6_EvtRpcGetClassicLogDisplayName(struct pipes_struct *p, struct eventlog6_EvtRpcGetClassicLogDisplayName *r);
void eventlog6_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_eventlog6_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_eventlog6_shutdown(void);
#endif /* __SRV_EVENTLOG6__ */
