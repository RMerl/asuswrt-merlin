/*
   Unix SMB/CIFS implementation.

   endpoint server for the eventlog6 pipe

   Copyright (C) Anatoliy Atanasov 2010

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "rpc_server/dcerpc_server.h"
#include "librpc/gen_ndr/ndr_eventlog6.h"
#include "rpc_server/common/common.h"


/*
  eventlog6_EvtRpcRegisterRemoteSubscription
*/
static WERROR dcesrv_eventlog6_EvtRpcRegisterRemoteSubscription(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcRegisterRemoteSubscription *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcRemoteSubscriptionNextAsync
*/
static WERROR dcesrv_eventlog6_EvtRpcRemoteSubscriptionNextAsync(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcRemoteSubscriptionNextAsync *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcRemoteSubscriptionNext
*/
static WERROR dcesrv_eventlog6_EvtRpcRemoteSubscriptionNext(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcRemoteSubscriptionNext *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcRemoteSubscriptionWaitAsync
*/
static WERROR dcesrv_eventlog6_EvtRpcRemoteSubscriptionWaitAsync(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcRemoteSubscriptionWaitAsync *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcRegisterControllableOperation
*/
static WERROR dcesrv_eventlog6_EvtRpcRegisterControllableOperation(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcRegisterControllableOperation *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcRegisterLogQuery
*/
static WERROR dcesrv_eventlog6_EvtRpcRegisterLogQuery(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcRegisterLogQuery *r)
{
	struct dcesrv_handle *handle;

	handle = dcesrv_handle_new(dce_call->context, 0);
	W_ERROR_HAVE_NO_MEMORY(handle);

	r->out.handle = &handle->wire_handle;

	handle = dcesrv_handle_new(dce_call->context, 0);
	W_ERROR_HAVE_NO_MEMORY(handle);

	r->out.opControl = &handle->wire_handle;

	return WERR_OK;
}


/*
  eventlog6_EvtRpcClearLog
*/
static WERROR dcesrv_eventlog6_EvtRpcClearLog(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcClearLog *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcExportLog
*/
static WERROR dcesrv_eventlog6_EvtRpcExportLog(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcExportLog *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcLocalizeExportLog
*/
static WERROR dcesrv_eventlog6_EvtRpcLocalizeExportLog(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcLocalizeExportLog *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcMessageRender
*/
static WERROR dcesrv_eventlog6_EvtRpcMessageRender(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcMessageRender *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcMessageRenderDefault
*/
static WERROR dcesrv_eventlog6_EvtRpcMessageRenderDefault(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcMessageRenderDefault *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcQueryNext
*/
static WERROR dcesrv_eventlog6_EvtRpcQueryNext(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcQueryNext *r)
{
	return WERR_OK;
}


/*
  eventlog6_EvtRpcQuerySeek
*/
static WERROR dcesrv_eventlog6_EvtRpcQuerySeek(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcQuerySeek *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcClose
*/
static WERROR dcesrv_eventlog6_EvtRpcClose(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcClose *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcCancel
*/
static WERROR dcesrv_eventlog6_EvtRpcCancel(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcCancel *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcAssertConfig
*/
static WERROR dcesrv_eventlog6_EvtRpcAssertConfig(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcAssertConfig *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcRetractConfig
*/
static WERROR dcesrv_eventlog6_EvtRpcRetractConfig(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcRetractConfig *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcOpenLogHandle
*/
static WERROR dcesrv_eventlog6_EvtRpcOpenLogHandle(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcOpenLogHandle *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcGetLogFileInfo
*/
static WERROR dcesrv_eventlog6_EvtRpcGetLogFileInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcGetLogFileInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcGetChannelList
*/
static WERROR dcesrv_eventlog6_EvtRpcGetChannelList(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcGetChannelList *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcGetChannelConfig
*/
static WERROR dcesrv_eventlog6_EvtRpcGetChannelConfig(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcGetChannelConfig *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcPutChannelConfig
*/
static WERROR dcesrv_eventlog6_EvtRpcPutChannelConfig(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcPutChannelConfig *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcGetPublisherList
*/
static WERROR dcesrv_eventlog6_EvtRpcGetPublisherList(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcGetPublisherList *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcGetPublisherListForChannel
*/
static WERROR dcesrv_eventlog6_EvtRpcGetPublisherListForChannel(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcGetPublisherListForChannel *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcGetPublisherMetadata
*/
static WERROR dcesrv_eventlog6_EvtRpcGetPublisherMetadata(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcGetPublisherMetadata *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcGetPublisherResourceMetadata
*/
static WERROR dcesrv_eventlog6_EvtRpcGetPublisherResourceMetadata(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcGetPublisherResourceMetadata *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcGetEventMetadataEnum
*/
static WERROR dcesrv_eventlog6_EvtRpcGetEventMetadataEnum(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcGetEventMetadataEnum *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcGetNextEventMetadata
*/
static WERROR dcesrv_eventlog6_EvtRpcGetNextEventMetadata(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcGetNextEventMetadata *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  eventlog6_EvtRpcGetClassicLogDisplayName
*/
static WERROR dcesrv_eventlog6_EvtRpcGetClassicLogDisplayName(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct eventlog6_EvtRpcGetClassicLogDisplayName *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_eventlog6_s.c"
