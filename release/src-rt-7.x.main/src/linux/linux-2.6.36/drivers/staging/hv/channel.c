/*
 * Copyright (c) 2009, Microsoft Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 * Authors:
 *   Haiyang Zhang <haiyangz@microsoft.com>
 *   Hank Janssen  <hjanssen@microsoft.com>
 */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/module.h>
#include "osd.h"
#include "logging.h"
#include "vmbus_private.h"

/* Internal routines */
static int VmbusChannelCreateGpadlHeader(
	void *Kbuffer,	/* must be phys and virt contiguous */
	u32 Size,	/* page-size multiple */
	struct vmbus_channel_msginfo **msgInfo,
	u32 *MessageCount);
static void DumpVmbusChannel(struct vmbus_channel *channel);
static void VmbusChannelSetEvent(struct vmbus_channel *channel);



/*
 * VmbusChannelSetEvent - Trigger an event notification on the specified
 * channel.
 */
static void VmbusChannelSetEvent(struct vmbus_channel *Channel)
{
	struct hv_monitor_page *monitorPage;

	if (Channel->OfferMsg.MonitorAllocated) {
		/* Each u32 represents 32 channels */
		set_bit(Channel->OfferMsg.ChildRelId & 31,
			(unsigned long *) gVmbusConnection.SendInterruptPage +
			(Channel->OfferMsg.ChildRelId >> 5));

		monitorPage = gVmbusConnection.MonitorPages;
		monitorPage++; /* Get the child to parent monitor page */

		set_bit(Channel->MonitorBit,
			(unsigned long *)&monitorPage->TriggerGroup
					[Channel->MonitorGroup].Pending);

	} else {
		VmbusSetEvent(Channel->OfferMsg.ChildRelId);
	}
}

/*
 * VmbusChannelGetDebugInfo -Retrieve various channel debug info
 */
void VmbusChannelGetDebugInfo(struct vmbus_channel *Channel,
			      struct vmbus_channel_debug_info *DebugInfo)
{
	struct hv_monitor_page *monitorPage;
	u8 monitorGroup = (u8)Channel->OfferMsg.MonitorId / 32;
	u8 monitorOffset = (u8)Channel->OfferMsg.MonitorId % 32;
	/* u32 monitorBit	= 1 << monitorOffset; */

	DebugInfo->RelId = Channel->OfferMsg.ChildRelId;
	DebugInfo->State = Channel->State;
	memcpy(&DebugInfo->InterfaceType,
	       &Channel->OfferMsg.Offer.InterfaceType, sizeof(struct hv_guid));
	memcpy(&DebugInfo->InterfaceInstance,
	       &Channel->OfferMsg.Offer.InterfaceInstance,
	       sizeof(struct hv_guid));

	monitorPage = (struct hv_monitor_page *)gVmbusConnection.MonitorPages;

	DebugInfo->MonitorId = Channel->OfferMsg.MonitorId;

	DebugInfo->ServerMonitorPending =
			monitorPage->TriggerGroup[monitorGroup].Pending;
	DebugInfo->ServerMonitorLatency =
			monitorPage->Latency[monitorGroup][monitorOffset];
	DebugInfo->ServerMonitorConnectionId =
			monitorPage->Parameter[monitorGroup]
					      [monitorOffset].ConnectionId.u.Id;

	monitorPage++;

	DebugInfo->ClientMonitorPending =
			monitorPage->TriggerGroup[monitorGroup].Pending;
	DebugInfo->ClientMonitorLatency =
			monitorPage->Latency[monitorGroup][monitorOffset];
	DebugInfo->ClientMonitorConnectionId =
			monitorPage->Parameter[monitorGroup]
					      [monitorOffset].ConnectionId.u.Id;

	RingBufferGetDebugInfo(&Channel->Inbound, &DebugInfo->Inbound);
	RingBufferGetDebugInfo(&Channel->Outbound, &DebugInfo->Outbound);
}

/*
 * VmbusChannelOpen - Open the specified channel.
 */
int VmbusChannelOpen(struct vmbus_channel *NewChannel, u32 SendRingBufferSize,
		     u32 RecvRingBufferSize, void *UserData, u32 UserDataLen,
		     void (*OnChannelCallback)(void *context), void *Context)
{
	struct vmbus_channel_open_channel *openMsg;
	struct vmbus_channel_msginfo *openInfo = NULL;
	void *in, *out;
	unsigned long flags;
	int ret, err = 0;

	/* Aligned to page size */
	/* ASSERT(!(SendRingBufferSize & (PAGE_SIZE - 1))); */
	/* ASSERT(!(RecvRingBufferSize & (PAGE_SIZE - 1))); */

	NewChannel->OnChannelCallback = OnChannelCallback;
	NewChannel->ChannelCallbackContext = Context;

	/* Allocate the ring buffer */
	out = osd_PageAlloc((SendRingBufferSize + RecvRingBufferSize)
			     >> PAGE_SHIFT);
	if (!out)
		return -ENOMEM;

	/* ASSERT(((unsigned long)out & (PAGE_SIZE-1)) == 0); */

	in = (void *)((unsigned long)out + SendRingBufferSize);

	NewChannel->RingBufferPages = out;
	NewChannel->RingBufferPageCount = (SendRingBufferSize +
					   RecvRingBufferSize) >> PAGE_SHIFT;

	ret = RingBufferInit(&NewChannel->Outbound, out, SendRingBufferSize);
	if (ret != 0) {
		err = ret;
		goto errorout;
	}

	ret = RingBufferInit(&NewChannel->Inbound, in, RecvRingBufferSize);
	if (ret != 0) {
		err = ret;
		goto errorout;
	}


	/* Establish the gpadl for the ring buffer */
	DPRINT_DBG(VMBUS, "Establishing ring buffer's gpadl for channel %p...",
		   NewChannel);

	NewChannel->RingBufferGpadlHandle = 0;

	ret = VmbusChannelEstablishGpadl(NewChannel,
					 NewChannel->Outbound.RingBuffer,
					 SendRingBufferSize +
					 RecvRingBufferSize,
					 &NewChannel->RingBufferGpadlHandle);

	if (ret != 0) {
		err = ret;
		goto errorout;
	}

	DPRINT_DBG(VMBUS, "channel %p <relid %d gpadl 0x%x send ring %p "
		   "size %d recv ring %p size %d, downstreamoffset %d>",
		   NewChannel, NewChannel->OfferMsg.ChildRelId,
		   NewChannel->RingBufferGpadlHandle,
		   NewChannel->Outbound.RingBuffer,
		   NewChannel->Outbound.RingSize,
		   NewChannel->Inbound.RingBuffer,
		   NewChannel->Inbound.RingSize,
		   SendRingBufferSize);

	/* Create and init the channel open message */
	openInfo = kmalloc(sizeof(*openInfo) +
			   sizeof(struct vmbus_channel_open_channel),
			   GFP_KERNEL);
	if (!openInfo) {
		err = -ENOMEM;
		goto errorout;
	}

	openInfo->WaitEvent = osd_WaitEventCreate();
	if (!openInfo->WaitEvent) {
		err = -ENOMEM;
		goto errorout;
	}

	openMsg = (struct vmbus_channel_open_channel *)openInfo->Msg;
	openMsg->Header.MessageType = ChannelMessageOpenChannel;
	openMsg->OpenId = NewChannel->OfferMsg.ChildRelId;
	openMsg->ChildRelId = NewChannel->OfferMsg.ChildRelId;
	openMsg->RingBufferGpadlHandle = NewChannel->RingBufferGpadlHandle;
	openMsg->DownstreamRingBufferPageOffset = SendRingBufferSize >>
						  PAGE_SHIFT;
	openMsg->ServerContextAreaGpadlHandle = 0; /* TODO */

	if (UserDataLen > MAX_USER_DEFINED_BYTES) {
		err = -EINVAL;
		goto errorout;
	}

	if (UserDataLen)
		memcpy(openMsg->UserData, UserData, UserDataLen);

	spin_lock_irqsave(&gVmbusConnection.channelmsg_lock, flags);
	list_add_tail(&openInfo->MsgListEntry,
		      &gVmbusConnection.ChannelMsgList);
	spin_unlock_irqrestore(&gVmbusConnection.channelmsg_lock, flags);

	DPRINT_DBG(VMBUS, "Sending channel open msg...");

	ret = VmbusPostMessage(openMsg,
			       sizeof(struct vmbus_channel_open_channel));
	if (ret != 0) {
		DPRINT_ERR(VMBUS, "unable to open channel - %d", ret);
		goto Cleanup;
	}

	osd_WaitEventWait(openInfo->WaitEvent);

	if (openInfo->Response.OpenResult.Status == 0)
		DPRINT_INFO(VMBUS, "channel <%p> open success!!", NewChannel);
	else
		DPRINT_INFO(VMBUS, "channel <%p> open failed - %d!!",
			    NewChannel, openInfo->Response.OpenResult.Status);

Cleanup:
	spin_lock_irqsave(&gVmbusConnection.channelmsg_lock, flags);
	list_del(&openInfo->MsgListEntry);
	spin_unlock_irqrestore(&gVmbusConnection.channelmsg_lock, flags);

	kfree(openInfo->WaitEvent);
	kfree(openInfo);
	return 0;

errorout:
	RingBufferCleanup(&NewChannel->Outbound);
	RingBufferCleanup(&NewChannel->Inbound);
	osd_PageFree(out, (SendRingBufferSize + RecvRingBufferSize)
		     >> PAGE_SHIFT);
	kfree(openInfo);
	return err;
}

/*
 * DumpGpadlBody - Dump the gpadl body message to the console for
 * debugging purposes.
 */
static void DumpGpadlBody(struct vmbus_channel_gpadl_body *Gpadl, u32 Len)
{
	int i;
	int pfnCount;

	pfnCount = (Len - sizeof(struct vmbus_channel_gpadl_body)) /
		   sizeof(u64);
	DPRINT_DBG(VMBUS, "gpadl body - len %d pfn count %d", Len, pfnCount);

	for (i = 0; i < pfnCount; i++)
		DPRINT_DBG(VMBUS, "gpadl body  - %d) pfn %llu",
			   i, Gpadl->Pfn[i]);
}

/*
 * DumpGpadlHeader - Dump the gpadl header message to the console for
 * debugging purposes.
 */
static void DumpGpadlHeader(struct vmbus_channel_gpadl_header *Gpadl)
{
	int i, j;
	int pageCount;

	DPRINT_DBG(VMBUS,
		   "gpadl header - relid %d, range count %d, range buflen %d",
		   Gpadl->ChildRelId, Gpadl->RangeCount, Gpadl->RangeBufLen);
	for (i = 0; i < Gpadl->RangeCount; i++) {
		pageCount = Gpadl->Range[i].ByteCount >> PAGE_SHIFT;
		pageCount = (pageCount > 26) ? 26 : pageCount;

		DPRINT_DBG(VMBUS, "gpadl range %d - len %d offset %d "
			   "page count %d", i, Gpadl->Range[i].ByteCount,
			   Gpadl->Range[i].ByteOffset, pageCount);

		for (j = 0; j < pageCount; j++)
			DPRINT_DBG(VMBUS, "%d) pfn %llu", j,
				   Gpadl->Range[i].PfnArray[j]);
	}
}

/*
 * VmbusChannelCreateGpadlHeader - Creates a gpadl for the specified buffer
 */
static int VmbusChannelCreateGpadlHeader(void *Kbuffer, u32 Size,
					 struct vmbus_channel_msginfo **MsgInfo,
					 u32 *MessageCount)
{
	int i;
	int pageCount;
	unsigned long long pfn;
	struct vmbus_channel_gpadl_header *gpaHeader;
	struct vmbus_channel_gpadl_body *gpadlBody;
	struct vmbus_channel_msginfo *msgHeader;
	struct vmbus_channel_msginfo *msgBody = NULL;
	u32 msgSize;

	int pfnSum, pfnCount, pfnLeft, pfnCurr, pfnSize;

	/* ASSERT((kbuffer & (PAGE_SIZE-1)) == 0); */
	/* ASSERT((Size & (PAGE_SIZE-1)) == 0); */

	pageCount = Size >> PAGE_SHIFT;
	pfn = virt_to_phys(Kbuffer) >> PAGE_SHIFT;

	/* do we need a gpadl body msg */
	pfnSize = MAX_SIZE_CHANNEL_MESSAGE -
		  sizeof(struct vmbus_channel_gpadl_header) -
		  sizeof(struct gpa_range);
	pfnCount = pfnSize / sizeof(u64);

	if (pageCount > pfnCount) {
		/* we need a gpadl body */
		/* fill in the header */
		msgSize = sizeof(struct vmbus_channel_msginfo) +
			  sizeof(struct vmbus_channel_gpadl_header) +
			  sizeof(struct gpa_range) + pfnCount * sizeof(u64);
		msgHeader =  kzalloc(msgSize, GFP_KERNEL);
		if (!msgHeader)
			goto nomem;

		INIT_LIST_HEAD(&msgHeader->SubMsgList);
		msgHeader->MessageSize = msgSize;

		gpaHeader = (struct vmbus_channel_gpadl_header *)msgHeader->Msg;
		gpaHeader->RangeCount = 1;
		gpaHeader->RangeBufLen = sizeof(struct gpa_range) +
					 pageCount * sizeof(u64);
		gpaHeader->Range[0].ByteOffset = 0;
		gpaHeader->Range[0].ByteCount = Size;
		for (i = 0; i < pfnCount; i++)
			gpaHeader->Range[0].PfnArray[i] = pfn+i;
		*MsgInfo = msgHeader;
		*MessageCount = 1;

		pfnSum = pfnCount;
		pfnLeft = pageCount - pfnCount;

		/* how many pfns can we fit */
		pfnSize = MAX_SIZE_CHANNEL_MESSAGE -
			  sizeof(struct vmbus_channel_gpadl_body);
		pfnCount = pfnSize / sizeof(u64);

		/* fill in the body */
		while (pfnLeft) {
			if (pfnLeft > pfnCount)
				pfnCurr = pfnCount;
			else
				pfnCurr = pfnLeft;

			msgSize = sizeof(struct vmbus_channel_msginfo) +
				  sizeof(struct vmbus_channel_gpadl_body) +
				  pfnCurr * sizeof(u64);
			msgBody = kzalloc(msgSize, GFP_KERNEL);
			if (!msgBody)
				goto nomem;
			msgBody->MessageSize = msgSize;
			(*MessageCount)++;
			gpadlBody =
				(struct vmbus_channel_gpadl_body *)msgBody->Msg;

			/* gpadlBody->Gpadl = kbuffer; */
			for (i = 0; i < pfnCurr; i++)
				gpadlBody->Pfn[i] = pfn + pfnSum + i;

			/* add to msg header */
			list_add_tail(&msgBody->MsgListEntry,
				      &msgHeader->SubMsgList);
			pfnSum += pfnCurr;
			pfnLeft -= pfnCurr;
		}
	} else {
		/* everything fits in a header */
		msgSize = sizeof(struct vmbus_channel_msginfo) +
			  sizeof(struct vmbus_channel_gpadl_header) +
			  sizeof(struct gpa_range) + pageCount * sizeof(u64);
		msgHeader = kzalloc(msgSize, GFP_KERNEL);
		if (msgHeader == NULL)
			goto nomem;
		msgHeader->MessageSize = msgSize;

		gpaHeader = (struct vmbus_channel_gpadl_header *)msgHeader->Msg;
		gpaHeader->RangeCount = 1;
		gpaHeader->RangeBufLen = sizeof(struct gpa_range) +
					 pageCount * sizeof(u64);
		gpaHeader->Range[0].ByteOffset = 0;
		gpaHeader->Range[0].ByteCount = Size;
		for (i = 0; i < pageCount; i++)
			gpaHeader->Range[0].PfnArray[i] = pfn+i;

		*MsgInfo = msgHeader;
		*MessageCount = 1;
	}

	return 0;
nomem:
	kfree(msgHeader);
	kfree(msgBody);
	return -ENOMEM;
}

/*
 * VmbusChannelEstablishGpadl - Estabish a GPADL for the specified buffer
 *
 * @Channel: a channel
 * @Kbuffer: from kmalloc
 * @Size: page-size multiple
 * @GpadlHandle: some funky thing
 */
int VmbusChannelEstablishGpadl(struct vmbus_channel *Channel, void *Kbuffer,
			       u32 Size, u32 *GpadlHandle)
{
	struct vmbus_channel_gpadl_header *gpadlMsg;
	struct vmbus_channel_gpadl_body *gpadlBody;
	/* struct vmbus_channel_gpadl_created *gpadlCreated; */
	struct vmbus_channel_msginfo *msgInfo = NULL;
	struct vmbus_channel_msginfo *subMsgInfo;
	u32 msgCount;
	struct list_head *curr;
	u32 nextGpadlHandle;
	unsigned long flags;
	int ret = 0;

	nextGpadlHandle = atomic_read(&gVmbusConnection.NextGpadlHandle);
	atomic_inc(&gVmbusConnection.NextGpadlHandle);

	ret = VmbusChannelCreateGpadlHeader(Kbuffer, Size, &msgInfo, &msgCount);
	if (ret)
		return ret;

	msgInfo->WaitEvent = osd_WaitEventCreate();
	if (!msgInfo->WaitEvent) {
		ret = -ENOMEM;
		goto Cleanup;
	}

	gpadlMsg = (struct vmbus_channel_gpadl_header *)msgInfo->Msg;
	gpadlMsg->Header.MessageType = ChannelMessageGpadlHeader;
	gpadlMsg->ChildRelId = Channel->OfferMsg.ChildRelId;
	gpadlMsg->Gpadl = nextGpadlHandle;

	DumpGpadlHeader(gpadlMsg);

	spin_lock_irqsave(&gVmbusConnection.channelmsg_lock, flags);
	list_add_tail(&msgInfo->MsgListEntry,
		      &gVmbusConnection.ChannelMsgList);

	spin_unlock_irqrestore(&gVmbusConnection.channelmsg_lock, flags);
	DPRINT_DBG(VMBUS, "buffer %p, size %d msg cnt %d",
		   Kbuffer, Size, msgCount);

	DPRINT_DBG(VMBUS, "Sending GPADL Header - len %zd",
		   msgInfo->MessageSize - sizeof(*msgInfo));

	ret = VmbusPostMessage(gpadlMsg, msgInfo->MessageSize -
			       sizeof(*msgInfo));
	if (ret != 0) {
		DPRINT_ERR(VMBUS, "Unable to open channel - %d", ret);
		goto Cleanup;
	}

	if (msgCount > 1) {
		list_for_each(curr, &msgInfo->SubMsgList) {

			subMsgInfo = (struct vmbus_channel_msginfo *)curr;
			gpadlBody =
			     (struct vmbus_channel_gpadl_body *)subMsgInfo->Msg;

			gpadlBody->Header.MessageType = ChannelMessageGpadlBody;
			gpadlBody->Gpadl = nextGpadlHandle;

			DPRINT_DBG(VMBUS, "Sending GPADL Body - len %zd",
				   subMsgInfo->MessageSize -
				   sizeof(*subMsgInfo));

			DumpGpadlBody(gpadlBody, subMsgInfo->MessageSize -
				      sizeof(*subMsgInfo));
			ret = VmbusPostMessage(gpadlBody,
					       subMsgInfo->MessageSize -
					       sizeof(*subMsgInfo));
			if (ret != 0)
				goto Cleanup;

		}
	}
	osd_WaitEventWait(msgInfo->WaitEvent);

	/* At this point, we received the gpadl created msg */
	DPRINT_DBG(VMBUS, "Received GPADL created "
		   "(relid %d, status %d handle %x)",
		   Channel->OfferMsg.ChildRelId,
		   msgInfo->Response.GpadlCreated.CreationStatus,
		   gpadlMsg->Gpadl);

	*GpadlHandle = gpadlMsg->Gpadl;

Cleanup:
	spin_lock_irqsave(&gVmbusConnection.channelmsg_lock, flags);
	list_del(&msgInfo->MsgListEntry);
	spin_unlock_irqrestore(&gVmbusConnection.channelmsg_lock, flags);

	kfree(msgInfo->WaitEvent);
	kfree(msgInfo);
	return ret;
}

/*
 * VmbusChannelTeardownGpadl -Teardown the specified GPADL handle
 */
int VmbusChannelTeardownGpadl(struct vmbus_channel *Channel, u32 GpadlHandle)
{
	struct vmbus_channel_gpadl_teardown *msg;
	struct vmbus_channel_msginfo *info;
	unsigned long flags;
	int ret;

	/* ASSERT(GpadlHandle != 0); */

	info = kmalloc(sizeof(*info) +
		       sizeof(struct vmbus_channel_gpadl_teardown), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->WaitEvent = osd_WaitEventCreate();
	if (!info->WaitEvent) {
		kfree(info);
		return -ENOMEM;
	}

	msg = (struct vmbus_channel_gpadl_teardown *)info->Msg;

	msg->Header.MessageType = ChannelMessageGpadlTeardown;
	msg->ChildRelId = Channel->OfferMsg.ChildRelId;
	msg->Gpadl = GpadlHandle;

	spin_lock_irqsave(&gVmbusConnection.channelmsg_lock, flags);
	list_add_tail(&info->MsgListEntry,
		      &gVmbusConnection.ChannelMsgList);
	spin_unlock_irqrestore(&gVmbusConnection.channelmsg_lock, flags);

	ret = VmbusPostMessage(msg,
			       sizeof(struct vmbus_channel_gpadl_teardown));
	if (ret != 0) {
		/* TODO: */
		/* something... */
	}

	osd_WaitEventWait(info->WaitEvent);

	/* Received a torndown response */
	spin_lock_irqsave(&gVmbusConnection.channelmsg_lock, flags);
	list_del(&info->MsgListEntry);
	spin_unlock_irqrestore(&gVmbusConnection.channelmsg_lock, flags);

	kfree(info->WaitEvent);
	kfree(info);
	return ret;
}

/*
 * VmbusChannelClose - Close the specified channel
 */
void VmbusChannelClose(struct vmbus_channel *Channel)
{
	struct vmbus_channel_close_channel *msg;
	struct vmbus_channel_msginfo *info;
	unsigned long flags;
	int ret;

	/* Stop callback and cancel the timer asap */
	Channel->OnChannelCallback = NULL;
	del_timer_sync(&Channel->poll_timer);

	/* Send a closing message */
	info = kmalloc(sizeof(*info) +
		       sizeof(struct vmbus_channel_close_channel), GFP_KERNEL);
	if (!info)
		return;

	/* info->waitEvent = osd_WaitEventCreate(); */

	msg = (struct vmbus_channel_close_channel *)info->Msg;
	msg->Header.MessageType = ChannelMessageCloseChannel;
	msg->ChildRelId = Channel->OfferMsg.ChildRelId;

	ret = VmbusPostMessage(msg, sizeof(struct vmbus_channel_close_channel));
	if (ret != 0) {
		/* TODO: */
		/* something... */
	}

	/* Tear down the gpadl for the channel's ring buffer */
	if (Channel->RingBufferGpadlHandle)
		VmbusChannelTeardownGpadl(Channel,
					  Channel->RingBufferGpadlHandle);

	/* TODO: Send a msg to release the childRelId */

	/* Cleanup the ring buffers for this channel */
	RingBufferCleanup(&Channel->Outbound);
	RingBufferCleanup(&Channel->Inbound);

	osd_PageFree(Channel->RingBufferPages, Channel->RingBufferPageCount);

	kfree(info);

	/*
	 * If we are closing the channel during an error path in
	 * opening the channel, don't free the channel since the
	 * caller will free the channel
	 */

	if (Channel->State == CHANNEL_OPEN_STATE) {
		spin_lock_irqsave(&gVmbusConnection.channel_lock, flags);
		list_del(&Channel->ListEntry);
		spin_unlock_irqrestore(&gVmbusConnection.channel_lock, flags);

		FreeVmbusChannel(Channel);
	}
}

/**
 * VmbusChannelSendPacket() - Send the specified buffer on the given channel
 * @Channel: Pointer to vmbus_channel structure.
 * @Buffer: Pointer to the buffer you want to receive the data into.
 * @BufferLen: Maximum size of what the the buffer will hold
 * @RequestId: Identifier of the request
 * @vmbus_packet_type: Type of packet that is being send e.g. negotiate, time
 * packet etc.
 *
 * Sends data in @Buffer directly to hyper-v via the vmbus
 * This will send the data unparsed to hyper-v.
 *
 * Mainly used by Hyper-V drivers.
 */
int VmbusChannelSendPacket(struct vmbus_channel *Channel, const void *Buffer,
			   u32 BufferLen, u64 RequestId,
			   enum vmbus_packet_type Type, u32 Flags)
{
	struct vmpacket_descriptor desc;
	u32 packetLen = sizeof(struct vmpacket_descriptor) + BufferLen;
	u32 packetLenAligned = ALIGN_UP(packetLen, sizeof(u64));
	struct scatterlist bufferList[3];
	u64 alignedData = 0;
	int ret;

	DPRINT_DBG(VMBUS, "channel %p buffer %p len %d",
		   Channel, Buffer, BufferLen);

	DumpVmbusChannel(Channel);

	/* ASSERT((packetLenAligned - packetLen) < sizeof(u64)); */

	/* Setup the descriptor */
	desc.Type = Type; /* VmbusPacketTypeDataInBand; */
	desc.Flags = Flags; /* VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED; */
	/* in 8-bytes granularity */
	desc.DataOffset8 = sizeof(struct vmpacket_descriptor) >> 3;
	desc.Length8 = (u16)(packetLenAligned >> 3);
	desc.TransactionId = RequestId;

	sg_init_table(bufferList, 3);
	sg_set_buf(&bufferList[0], &desc, sizeof(struct vmpacket_descriptor));
	sg_set_buf(&bufferList[1], Buffer, BufferLen);
	sg_set_buf(&bufferList[2], &alignedData, packetLenAligned - packetLen);

	ret = RingBufferWrite(&Channel->Outbound, bufferList, 3);

	/* TODO: We should determine if this is optional */
	if (ret == 0 && !GetRingBufferInterruptMask(&Channel->Outbound))
		VmbusChannelSetEvent(Channel);

	return ret;
}
EXPORT_SYMBOL(VmbusChannelSendPacket);

/*
 * VmbusChannelSendPacketPageBuffer - Send a range of single-page buffer
 * packets using a GPADL Direct packet type.
 */
int VmbusChannelSendPacketPageBuffer(struct vmbus_channel *Channel,
				     struct hv_page_buffer PageBuffers[],
				     u32 PageCount, void *Buffer, u32 BufferLen,
				     u64 RequestId)
{
	int ret;
	int i;
	struct VMBUS_CHANNEL_PACKET_PAGE_BUFFER desc;
	u32 descSize;
	u32 packetLen;
	u32 packetLenAligned;
	struct scatterlist bufferList[3];
	u64 alignedData = 0;

	if (PageCount > MAX_PAGE_BUFFER_COUNT)
		return -EINVAL;

	DumpVmbusChannel(Channel);

	/*
	 * Adjust the size down since VMBUS_CHANNEL_PACKET_PAGE_BUFFER is the
	 * largest size we support
	 */
	descSize = sizeof(struct VMBUS_CHANNEL_PACKET_PAGE_BUFFER) -
			  ((MAX_PAGE_BUFFER_COUNT - PageCount) *
			  sizeof(struct hv_page_buffer));
	packetLen = descSize + BufferLen;
	packetLenAligned = ALIGN_UP(packetLen, sizeof(u64));

	/* ASSERT((packetLenAligned - packetLen) < sizeof(u64)); */

	/* Setup the descriptor */
	desc.Type = VmbusPacketTypeDataUsingGpaDirect;
	desc.Flags = VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED;
	desc.DataOffset8 = descSize >> 3; /* in 8-bytes grandularity */
	desc.Length8 = (u16)(packetLenAligned >> 3);
	desc.TransactionId = RequestId;
	desc.RangeCount = PageCount;

	for (i = 0; i < PageCount; i++) {
		desc.Range[i].Length = PageBuffers[i].Length;
		desc.Range[i].Offset = PageBuffers[i].Offset;
		desc.Range[i].Pfn	 = PageBuffers[i].Pfn;
	}

	sg_init_table(bufferList, 3);
	sg_set_buf(&bufferList[0], &desc, descSize);
	sg_set_buf(&bufferList[1], Buffer, BufferLen);
	sg_set_buf(&bufferList[2], &alignedData, packetLenAligned - packetLen);

	ret = RingBufferWrite(&Channel->Outbound, bufferList, 3);

	/* TODO: We should determine if this is optional */
	if (ret == 0 && !GetRingBufferInterruptMask(&Channel->Outbound))
		VmbusChannelSetEvent(Channel);

	return ret;
}

/*
 * VmbusChannelSendPacketMultiPageBuffer - Send a multi-page buffer packet
 * using a GPADL Direct packet type.
 */
int VmbusChannelSendPacketMultiPageBuffer(struct vmbus_channel *Channel,
				struct hv_multipage_buffer *MultiPageBuffer,
				void *Buffer, u32 BufferLen, u64 RequestId)
{
	int ret;
	struct VMBUS_CHANNEL_PACKET_MULITPAGE_BUFFER desc;
	u32 descSize;
	u32 packetLen;
	u32 packetLenAligned;
	struct scatterlist bufferList[3];
	u64 alignedData = 0;
	u32 PfnCount = NUM_PAGES_SPANNED(MultiPageBuffer->Offset,
					 MultiPageBuffer->Length);

	DumpVmbusChannel(Channel);

	DPRINT_DBG(VMBUS, "data buffer - offset %u len %u pfn count %u",
		   MultiPageBuffer->Offset, MultiPageBuffer->Length, PfnCount);

	if ((PfnCount < 0) || (PfnCount > MAX_MULTIPAGE_BUFFER_COUNT))
		return -EINVAL;

	/*
	 * Adjust the size down since VMBUS_CHANNEL_PACKET_MULITPAGE_BUFFER is
	 * the largest size we support
	 */
	descSize = sizeof(struct VMBUS_CHANNEL_PACKET_MULITPAGE_BUFFER) -
			  ((MAX_MULTIPAGE_BUFFER_COUNT - PfnCount) *
			  sizeof(u64));
	packetLen = descSize + BufferLen;
	packetLenAligned = ALIGN_UP(packetLen, sizeof(u64));

	/* ASSERT((packetLenAligned - packetLen) < sizeof(u64)); */

	/* Setup the descriptor */
	desc.Type = VmbusPacketTypeDataUsingGpaDirect;
	desc.Flags = VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED;
	desc.DataOffset8 = descSize >> 3; /* in 8-bytes grandularity */
	desc.Length8 = (u16)(packetLenAligned >> 3);
	desc.TransactionId = RequestId;
	desc.RangeCount = 1;

	desc.Range.Length = MultiPageBuffer->Length;
	desc.Range.Offset = MultiPageBuffer->Offset;

	memcpy(desc.Range.PfnArray, MultiPageBuffer->PfnArray,
	       PfnCount * sizeof(u64));

	sg_init_table(bufferList, 3);
	sg_set_buf(&bufferList[0], &desc, descSize);
	sg_set_buf(&bufferList[1], Buffer, BufferLen);
	sg_set_buf(&bufferList[2], &alignedData, packetLenAligned - packetLen);

	ret = RingBufferWrite(&Channel->Outbound, bufferList, 3);

	/* TODO: We should determine if this is optional */
	if (ret == 0 && !GetRingBufferInterruptMask(&Channel->Outbound))
		VmbusChannelSetEvent(Channel);

	return ret;
}


/**
 * VmbusChannelRecvPacket() - Retrieve the user packet on the specified channel
 * @Channel: Pointer to vmbus_channel structure.
 * @Buffer: Pointer to the buffer you want to receive the data into.
 * @BufferLen: Maximum size of what the the buffer will hold
 * @BufferActualLen: The actual size of the data after it was received
 * @RequestId: Identifier of the request
 *
 * Receives directly from the hyper-v vmbus and puts the data it received
 * into Buffer. This will receive the data unparsed from hyper-v.
 *
 * Mainly used by Hyper-V drivers.
 */
int VmbusChannelRecvPacket(struct vmbus_channel *Channel, void *Buffer,
			   u32 BufferLen, u32 *BufferActualLen, u64 *RequestId)
{
	struct vmpacket_descriptor desc;
	u32 packetLen;
	u32 userLen;
	int ret;
	unsigned long flags;

	*BufferActualLen = 0;
	*RequestId = 0;

	spin_lock_irqsave(&Channel->inbound_lock, flags);

	ret = RingBufferPeek(&Channel->Inbound, &desc,
			     sizeof(struct vmpacket_descriptor));
	if (ret != 0) {
		spin_unlock_irqrestore(&Channel->inbound_lock, flags);

		/* DPRINT_DBG(VMBUS, "nothing to read!!"); */
		return 0;
	}

	/* VmbusChannelClearEvent(Channel); */

	packetLen = desc.Length8 << 3;
	userLen = packetLen - (desc.DataOffset8 << 3);
	/* ASSERT(userLen > 0); */

	DPRINT_DBG(VMBUS, "packet received on channel %p relid %d <type %d "
		   "flag %d tid %llx pktlen %d datalen %d> ",
		   Channel, Channel->OfferMsg.ChildRelId, desc.Type,
		   desc.Flags, desc.TransactionId, packetLen, userLen);

	*BufferActualLen = userLen;

	if (userLen > BufferLen) {
		spin_unlock_irqrestore(&Channel->inbound_lock, flags);

		DPRINT_ERR(VMBUS, "buffer too small - got %d needs %d",
			   BufferLen, userLen);
		return -1;
	}

	*RequestId = desc.TransactionId;

	/* Copy over the packet to the user buffer */
	ret = RingBufferRead(&Channel->Inbound, Buffer, userLen,
			     (desc.DataOffset8 << 3));

	spin_unlock_irqrestore(&Channel->inbound_lock, flags);

	return 0;
}
EXPORT_SYMBOL(VmbusChannelRecvPacket);

/*
 * VmbusChannelRecvPacketRaw - Retrieve the raw packet on the specified channel
 */
int VmbusChannelRecvPacketRaw(struct vmbus_channel *Channel, void *Buffer,
			      u32 BufferLen, u32 *BufferActualLen,
			      u64 *RequestId)
{
	struct vmpacket_descriptor desc;
	u32 packetLen;
	u32 userLen;
	int ret;
	unsigned long flags;

	*BufferActualLen = 0;
	*RequestId = 0;

	spin_lock_irqsave(&Channel->inbound_lock, flags);

	ret = RingBufferPeek(&Channel->Inbound, &desc,
			     sizeof(struct vmpacket_descriptor));
	if (ret != 0) {
		spin_unlock_irqrestore(&Channel->inbound_lock, flags);

		/* DPRINT_DBG(VMBUS, "nothing to read!!"); */
		return 0;
	}

	/* VmbusChannelClearEvent(Channel); */

	packetLen = desc.Length8 << 3;
	userLen = packetLen - (desc.DataOffset8 << 3);

	DPRINT_DBG(VMBUS, "packet received on channel %p relid %d <type %d "
		   "flag %d tid %llx pktlen %d datalen %d> ",
		   Channel, Channel->OfferMsg.ChildRelId, desc.Type,
		   desc.Flags, desc.TransactionId, packetLen, userLen);

	*BufferActualLen = packetLen;

	if (packetLen > BufferLen) {
		spin_unlock_irqrestore(&Channel->inbound_lock, flags);

		DPRINT_ERR(VMBUS, "buffer too small - needed %d bytes but "
			   "got space for only %d bytes", packetLen, BufferLen);
		return -2;
	}

	*RequestId = desc.TransactionId;

	/* Copy over the entire packet to the user buffer */
	ret = RingBufferRead(&Channel->Inbound, Buffer, packetLen, 0);

	spin_unlock_irqrestore(&Channel->inbound_lock, flags);
	return 0;
}

/*
 * VmbusChannelOnChannelEvent - Channel event callback
 */
void VmbusChannelOnChannelEvent(struct vmbus_channel *Channel)
{
	DumpVmbusChannel(Channel);
	/* ASSERT(Channel->OnChannelCallback); */

	Channel->OnChannelCallback(Channel->ChannelCallbackContext);

	mod_timer(&Channel->poll_timer, jiffies + usecs_to_jiffies(100));
}

/*
 * VmbusChannelOnTimer - Timer event callback
 */
void VmbusChannelOnTimer(unsigned long data)
{
	struct vmbus_channel *channel = (struct vmbus_channel *)data;

	if (channel->OnChannelCallback)
		channel->OnChannelCallback(channel->ChannelCallbackContext);
}

/*
 * DumpVmbusChannel - Dump vmbus channel info to the console
 */
static void DumpVmbusChannel(struct vmbus_channel *Channel)
{
	DPRINT_DBG(VMBUS, "Channel (%d)", Channel->OfferMsg.ChildRelId);
	DumpRingInfo(&Channel->Outbound, "Outbound ");
	DumpRingInfo(&Channel->Inbound, "Inbound ");
}
