/*
 *************************************************************************
 * Ralink Tech Inc.
 * 5F., No.36, Taiyuan St., Jhubei City,
 * Hsinchu County 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2007, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************
 */

#include <linux/firmware.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include "rt_config.h"

unsigned long RTDebugLevel = RT_DEBUG_ERROR;

/* for wireless system event message */
char const *pWirelessSysEventText[IW_SYS_EVENT_TYPE_NUM] = {
	/* system status event */
	"had associated successfully",	/* IW_ASSOC_EVENT_FLAG */
	"had disassociated",	/* IW_DISASSOC_EVENT_FLAG */
	"had deauthenticated",	/* IW_DEAUTH_EVENT_FLAG */
	"had been aged-out and disassociated",	/* IW_AGEOUT_EVENT_FLAG */
	"occurred CounterMeasures attack",	/* IW_COUNTER_MEASURES_EVENT_FLAG */
	"occurred replay counter different in Key Handshaking",	/* IW_REPLAY_COUNTER_DIFF_EVENT_FLAG */
	"occurred RSNIE different in Key Handshaking",	/* IW_RSNIE_DIFF_EVENT_FLAG */
	"occurred MIC different in Key Handshaking",	/* IW_MIC_DIFF_EVENT_FLAG */
	"occurred ICV error in RX",	/* IW_ICV_ERROR_EVENT_FLAG */
	"occurred MIC error in RX",	/* IW_MIC_ERROR_EVENT_FLAG */
	"Group Key Handshaking timeout",	/* IW_GROUP_HS_TIMEOUT_EVENT_FLAG */
	"Pairwise Key Handshaking timeout",	/* IW_PAIRWISE_HS_TIMEOUT_EVENT_FLAG */
	"RSN IE sanity check failure",	/* IW_RSNIE_SANITY_FAIL_EVENT_FLAG */
	"set key done in WPA/WPAPSK",	/* IW_SET_KEY_DONE_WPA1_EVENT_FLAG */
	"set key done in WPA2/WPA2PSK",	/* IW_SET_KEY_DONE_WPA2_EVENT_FLAG */
	"connects with our wireless client",	/* IW_STA_LINKUP_EVENT_FLAG */
	"disconnects with our wireless client",	/* IW_STA_LINKDOWN_EVENT_FLAG */
	"scan completed"	/* IW_SCAN_COMPLETED_EVENT_FLAG */
	    "scan terminate! Busy! Enqueue fail!"	/* IW_SCAN_ENQUEUE_FAIL_EVENT_FLAG */
};

/* for wireless IDS_spoof_attack event message */
char const *pWirelessSpoofEventText[IW_SPOOF_EVENT_TYPE_NUM] = {
	"detected conflict SSID",	/* IW_CONFLICT_SSID_EVENT_FLAG */
	"detected spoofed association response",	/* IW_SPOOF_ASSOC_RESP_EVENT_FLAG */
	"detected spoofed reassociation responses",	/* IW_SPOOF_REASSOC_RESP_EVENT_FLAG */
	"detected spoofed probe response",	/* IW_SPOOF_PROBE_RESP_EVENT_FLAG */
	"detected spoofed beacon",	/* IW_SPOOF_BEACON_EVENT_FLAG */
	"detected spoofed disassociation",	/* IW_SPOOF_DISASSOC_EVENT_FLAG */
	"detected spoofed authentication",	/* IW_SPOOF_AUTH_EVENT_FLAG */
	"detected spoofed deauthentication",	/* IW_SPOOF_DEAUTH_EVENT_FLAG */
	"detected spoofed unknown management frame",	/* IW_SPOOF_UNKNOWN_MGMT_EVENT_FLAG */
	"detected replay attack"	/* IW_REPLAY_ATTACK_EVENT_FLAG */
};

/* for wireless IDS_flooding_attack event message */
char const *pWirelessFloodEventText[IW_FLOOD_EVENT_TYPE_NUM] = {
	"detected authentication flooding",	/* IW_FLOOD_AUTH_EVENT_FLAG */
	"detected association request flooding",	/* IW_FLOOD_ASSOC_REQ_EVENT_FLAG */
	"detected reassociation request flooding",	/* IW_FLOOD_REASSOC_REQ_EVENT_FLAG */
	"detected probe request flooding",	/* IW_FLOOD_PROBE_REQ_EVENT_FLAG */
	"detected disassociation flooding",	/* IW_FLOOD_DISASSOC_EVENT_FLAG */
	"detected deauthentication flooding",	/* IW_FLOOD_DEAUTH_EVENT_FLAG */
	"detected 802.1x eap-request flooding"	/* IW_FLOOD_EAP_REQ_EVENT_FLAG */
};

/* timeout -- ms */
void RTMP_SetPeriodicTimer(struct timer_list *pTimer,
			   IN unsigned long timeout)
{
	timeout = ((timeout * OS_HZ) / 1000);
	pTimer->expires = jiffies + timeout;
	add_timer(pTimer);
}

/* convert NdisMInitializeTimer --> RTMP_OS_Init_Timer */
void RTMP_OS_Init_Timer(struct rt_rtmp_adapter *pAd,
			struct timer_list *pTimer,
			IN TIMER_FUNCTION function, void *data)
{
	init_timer(pTimer);
	pTimer->data = (unsigned long)data;
	pTimer->function = function;
}

void RTMP_OS_Add_Timer(struct timer_list *pTimer,
		       IN unsigned long timeout)
{
	if (timer_pending(pTimer))
		return;

	timeout = ((timeout * OS_HZ) / 1000);
	pTimer->expires = jiffies + timeout;
	add_timer(pTimer);
}

void RTMP_OS_Mod_Timer(struct timer_list *pTimer,
		       IN unsigned long timeout)
{
	timeout = ((timeout * OS_HZ) / 1000);
	mod_timer(pTimer, jiffies + timeout);
}

void RTMP_OS_Del_Timer(struct timer_list *pTimer,
		       OUT BOOLEAN * pCancelled)
{
	if (timer_pending(pTimer)) {
		*pCancelled = del_timer_sync(pTimer);
	} else {
		*pCancelled = TRUE;
	}

}

void RTMP_OS_Release_Packet(struct rt_rtmp_adapter *pAd, struct rt_queue_entry *pEntry)
{
	/*RTMPFreeNdisPacket(pAd, (struct sk_buff *)pEntry); */
}

/* Unify all delay routine by using udelay */
void RTMPusecDelay(unsigned long usec)
{
	unsigned long i;

	for (i = 0; i < (usec / 50); i++)
		udelay(50);

	if (usec % 50)
		udelay(usec % 50);
}

void RTMP_GetCurrentSystemTime(LARGE_INTEGER *time)
{
	time->u.LowPart = jiffies;
}

/* pAd MUST allow to be NULL */
int os_alloc_mem(struct rt_rtmp_adapter *pAd, u8 ** mem, unsigned long size)
{
	*mem = kmalloc(size, GFP_ATOMIC);
	if (*mem)
		return NDIS_STATUS_SUCCESS;
	else
		return NDIS_STATUS_FAILURE;
}

/* pAd MUST allow to be NULL */
int os_free_mem(struct rt_rtmp_adapter *pAd, void *mem)
{

	ASSERT(mem);
	kfree(mem);
	return NDIS_STATUS_SUCCESS;
}

void *RtmpOSNetPktAlloc(struct rt_rtmp_adapter *pAd, IN int size)
{
	struct sk_buff *skb;
	/* Add 2 more bytes for ip header alignment */
	skb = dev_alloc_skb(size + 2);

	return (void *)skb;
}

void *RTMP_AllocateFragPacketBuffer(struct rt_rtmp_adapter *pAd,
					   unsigned long Length)
{
	struct sk_buff *pkt;

	pkt = dev_alloc_skb(Length);

	if (pkt == NULL) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("can't allocate frag rx %ld size packet\n", Length));
	}

	if (pkt) {
		RTMP_SET_PACKET_SOURCE(OSPKT_TO_RTPKT(pkt), PKTSRC_NDIS);
	}

	return (void *)pkt;
}

void *RTMP_AllocateTxPacketBuffer(struct rt_rtmp_adapter *pAd,
					 unsigned long Length,
					 IN BOOLEAN Cached,
					 void **VirtualAddress)
{
	struct sk_buff *pkt;

	pkt = dev_alloc_skb(Length);

	if (pkt == NULL) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("can't allocate tx %ld size packet\n", Length));
	}

	if (pkt) {
		RTMP_SET_PACKET_SOURCE(OSPKT_TO_RTPKT(pkt), PKTSRC_NDIS);
		*VirtualAddress = (void *)pkt->data;
	} else {
		*VirtualAddress = (void *)NULL;
	}

	return (void *)pkt;
}

void build_tx_packet(struct rt_rtmp_adapter *pAd,
		     void *pPacket,
		     u8 *pFrame, unsigned long FrameLen)
{

	struct sk_buff *pTxPkt;

	ASSERT(pPacket);
	pTxPkt = RTPKT_TO_OSPKT(pPacket);

	NdisMoveMemory(skb_put(pTxPkt, FrameLen), pFrame, FrameLen);
}

void RTMPFreeAdapter(struct rt_rtmp_adapter *pAd)
{
	struct os_cookie *os_cookie;
	int index;

	os_cookie = (struct os_cookie *)pAd->OS_Cookie;

	if (pAd->BeaconBuf)
		kfree(pAd->BeaconBuf);

	NdisFreeSpinLock(&pAd->MgmtRingLock);

#ifdef RTMP_MAC_PCI
	NdisFreeSpinLock(&pAd->RxRingLock);
#ifdef RT3090
	NdisFreeSpinLock(&pAd->McuCmdLock);
#endif /* RT3090 // */
#endif /* RTMP_MAC_PCI // */

	for (index = 0; index < NUM_OF_TX_RING; index++) {
		NdisFreeSpinLock(&pAd->TxSwQueueLock[index]);
		NdisFreeSpinLock(&pAd->DeQueueLock[index]);
		pAd->DeQueueRunning[index] = FALSE;
	}

	NdisFreeSpinLock(&pAd->irq_lock);

	release_firmware(pAd->firmware);

	vfree(pAd);		/* pci_free_consistent(os_cookie->pci_dev,sizeof(struct rt_rtmp_adapter),pAd,os_cookie->pAd_pa); */
	if (os_cookie)
		kfree(os_cookie);
}

BOOLEAN OS_Need_Clone_Packet(void)
{
	return FALSE;
}

/*
	========================================================================

	Routine Description:
		clone an input NDIS PACKET to another one. The new internally created NDIS PACKET
		must have only one NDIS BUFFER
		return - byte copied. 0 means can't create NDIS PACKET
		NOTE: internally created char should be destroyed by RTMPFreeNdisPacket

	Arguments:
		pAd 	Pointer to our adapter
		pInsAMSDUHdr	EWC A-MSDU format has extra 14-bytes header. if TRUE, insert this 14-byte hdr in front of MSDU.
		*pSrcTotalLen			return total packet length. This lenght is calculated with 802.3 format packet.

	Return Value:
		NDIS_STATUS_SUCCESS
		NDIS_STATUS_FAILURE

	Note:

	========================================================================
*/
int RTMPCloneNdisPacket(struct rt_rtmp_adapter *pAd,
				IN BOOLEAN pInsAMSDUHdr,
				void *pInPacket,
				void **ppOutPacket)
{

	struct sk_buff *pkt;

	ASSERT(pInPacket);
	ASSERT(ppOutPacket);

	/* 1. Allocate a packet */
	pkt = dev_alloc_skb(2048);

	if (pkt == NULL) {
		return NDIS_STATUS_FAILURE;
	}

	skb_put(pkt, GET_OS_PKT_LEN(pInPacket));
	NdisMoveMemory(pkt->data, GET_OS_PKT_DATAPTR(pInPacket),
		       GET_OS_PKT_LEN(pInPacket));
	*ppOutPacket = OSPKT_TO_RTPKT(pkt);

	RTMP_SET_PACKET_SOURCE(OSPKT_TO_RTPKT(pkt), PKTSRC_NDIS);

	printk("###Clone###\n");

	return NDIS_STATUS_SUCCESS;
}

/* the allocated NDIS PACKET must be freed via RTMPFreeNdisPacket() */
int RTMPAllocateNdisPacket(struct rt_rtmp_adapter *pAd,
				   void **ppPacket,
				   u8 *pHeader,
				   u32 HeaderLen,
				   u8 *pData, u32 DataLen)
{
	void *pPacket;
	ASSERT(pData);
	ASSERT(DataLen);

	/* 1. Allocate a packet */
	pPacket =
	    (void **) dev_alloc_skb(HeaderLen + DataLen +
					   RTMP_PKT_TAIL_PADDING);
	if (pPacket == NULL) {
		*ppPacket = NULL;
#ifdef DEBUG
		printk("RTMPAllocateNdisPacket Fail\n");
#endif
		return NDIS_STATUS_FAILURE;
	}
	/* 2. clone the frame content */
	if (HeaderLen > 0)
		NdisMoveMemory(GET_OS_PKT_DATAPTR(pPacket), pHeader, HeaderLen);
	if (DataLen > 0)
		NdisMoveMemory(GET_OS_PKT_DATAPTR(pPacket) + HeaderLen, pData,
			       DataLen);

	/* 3. update length of packet */
	skb_put(GET_OS_PKT_TYPE(pPacket), HeaderLen + DataLen);

	RTMP_SET_PACKET_SOURCE(pPacket, PKTSRC_NDIS);
/*      printk("%s : pPacket = %p, len = %d\n", __func__, pPacket, GET_OS_PKT_LEN(pPacket)); */
	*ppPacket = pPacket;
	return NDIS_STATUS_SUCCESS;
}

/*
  ========================================================================
  Description:
	This routine frees a miniport internally allocated char and its
	corresponding NDIS_BUFFER and allocated memory.
  ========================================================================
*/
void RTMPFreeNdisPacket(struct rt_rtmp_adapter *pAd, void *pPacket)
{
	dev_kfree_skb_any(RTPKT_TO_OSPKT(pPacket));
}

/* IRQL = DISPATCH_LEVEL */
/* NOTE: we do have an assumption here, that Byte0 and Byte1 always reasid at the same */
/*                       scatter gather buffer */
int Sniff2BytesFromNdisBuffer(char *pFirstBuffer,
				      u8 DesiredOffset,
				      u8 *pByte0, u8 *pByte1)
{
	*pByte0 = *(u8 *)(pFirstBuffer + DesiredOffset);
	*pByte1 = *(u8 *)(pFirstBuffer + DesiredOffset + 1);

	return NDIS_STATUS_SUCCESS;
}

void RTMP_QueryPacketInfo(void *pPacket,
			  struct rt_packet_info *pPacketInfo,
			  u8 **pSrcBufVA, u32 * pSrcBufLen)
{
	pPacketInfo->BufferCount = 1;
	pPacketInfo->pFirstBuffer = (char *)GET_OS_PKT_DATAPTR(pPacket);
	pPacketInfo->PhysicalBufferCount = 1;
	pPacketInfo->TotalPacketLength = GET_OS_PKT_LEN(pPacket);

	*pSrcBufVA = GET_OS_PKT_DATAPTR(pPacket);
	*pSrcBufLen = GET_OS_PKT_LEN(pPacket);
}

void RTMP_QueryNextPacketInfo(void **ppPacket,
			      struct rt_packet_info *pPacketInfo,
			      u8 **pSrcBufVA, u32 * pSrcBufLen)
{
	void *pPacket = NULL;

	if (*ppPacket)
		pPacket = GET_OS_PKT_NEXT(*ppPacket);

	if (pPacket) {
		pPacketInfo->BufferCount = 1;
		pPacketInfo->pFirstBuffer =
		    (char *)GET_OS_PKT_DATAPTR(pPacket);
		pPacketInfo->PhysicalBufferCount = 1;
		pPacketInfo->TotalPacketLength = GET_OS_PKT_LEN(pPacket);

		*pSrcBufVA = GET_OS_PKT_DATAPTR(pPacket);
		*pSrcBufLen = GET_OS_PKT_LEN(pPacket);
		*ppPacket = GET_OS_PKT_NEXT(pPacket);
	} else {
		pPacketInfo->BufferCount = 0;
		pPacketInfo->pFirstBuffer = NULL;
		pPacketInfo->PhysicalBufferCount = 0;
		pPacketInfo->TotalPacketLength = 0;

		*pSrcBufVA = NULL;
		*pSrcBufLen = 0;
		*ppPacket = NULL;
	}
}

void *DuplicatePacket(struct rt_rtmp_adapter *pAd,
			     void *pPacket, u8 FromWhichBSSID)
{
	struct sk_buff *skb;
	void *pRetPacket = NULL;
	u16 DataSize;
	u8 *pData;

	DataSize = (u16)GET_OS_PKT_LEN(pPacket);
	pData = (u8 *)GET_OS_PKT_DATAPTR(pPacket);

	skb = skb_clone(RTPKT_TO_OSPKT(pPacket), MEM_ALLOC_FLAG);
	if (skb) {
		skb->dev = get_netdev_from_bssid(pAd, FromWhichBSSID);
		pRetPacket = OSPKT_TO_RTPKT(skb);
	}

	return pRetPacket;

}

void *duplicate_pkt(struct rt_rtmp_adapter *pAd,
			   u8 *pHeader802_3,
			   u32 HdrLen,
			   u8 *pData,
			   unsigned long DataSize, u8 FromWhichBSSID)
{
	struct sk_buff *skb;
	void *pPacket = NULL;

	skb = __dev_alloc_skb(HdrLen + DataSize + 2, MEM_ALLOC_FLAG);
	if (skb != NULL) {
		skb_reserve(skb, 2);
		NdisMoveMemory(skb_tail_pointer(skb), pHeader802_3, HdrLen);
		skb_put(skb, HdrLen);
		NdisMoveMemory(skb_tail_pointer(skb), pData, DataSize);
		skb_put(skb, DataSize);
		skb->dev = get_netdev_from_bssid(pAd, FromWhichBSSID);
		pPacket = OSPKT_TO_RTPKT(skb);
	}

	return pPacket;
}

#define TKIP_TX_MIC_SIZE		8
void *duplicate_pkt_with_TKIP_MIC(struct rt_rtmp_adapter *pAd,
					 void *pPacket)
{
	struct sk_buff *skb, *newskb;

	skb = RTPKT_TO_OSPKT(pPacket);
	if (skb_tailroom(skb) < TKIP_TX_MIC_SIZE) {
		/* alloc a new skb and copy the packet */
		newskb =
		    skb_copy_expand(skb, skb_headroom(skb), TKIP_TX_MIC_SIZE,
				    GFP_ATOMIC);
		dev_kfree_skb_any(skb);
		if (newskb == NULL) {
			DBGPRINT(RT_DEBUG_ERROR,
				 ("Extend Tx.MIC for packet failed!, dropping packet!\n"));
			return NULL;
		}
		skb = newskb;
	}

	return OSPKT_TO_RTPKT(skb);
}

void *ClonePacket(struct rt_rtmp_adapter *pAd,
			 void *pPacket,
			 u8 *pData, unsigned long DataSize)
{
	struct sk_buff *pRxPkt;
	struct sk_buff *pClonedPkt;

	ASSERT(pPacket);
	pRxPkt = RTPKT_TO_OSPKT(pPacket);

	/* clone the packet */
	pClonedPkt = skb_clone(pRxPkt, MEM_ALLOC_FLAG);

	if (pClonedPkt) {
		/* set the correct dataptr and data len */
		pClonedPkt->dev = pRxPkt->dev;
		pClonedPkt->data = pData;
		pClonedPkt->len = DataSize;
		skb_set_tail_pointer(pClonedPkt, DataSize)
		ASSERT(DataSize < 1530);
	}
	return pClonedPkt;
}

/* */
/* change OS packet DataPtr and DataLen */
/* */
void update_os_packet_info(struct rt_rtmp_adapter *pAd,
			   struct rt_rx_blk *pRxBlk, u8 FromWhichBSSID)
{
	struct sk_buff *pOSPkt;

	ASSERT(pRxBlk->pRxPacket);
	pOSPkt = RTPKT_TO_OSPKT(pRxBlk->pRxPacket);

	pOSPkt->dev = get_netdev_from_bssid(pAd, FromWhichBSSID);
	pOSPkt->data = pRxBlk->pData;
	pOSPkt->len = pRxBlk->DataSize;
	skb_set_tail_pointer(pOSPkt, pOSPkt->len);
}

void wlan_802_11_to_802_3_packet(struct rt_rtmp_adapter *pAd,
				 struct rt_rx_blk *pRxBlk,
				 u8 *pHeader802_3,
				 u8 FromWhichBSSID)
{
	struct sk_buff *pOSPkt;

	ASSERT(pRxBlk->pRxPacket);
	ASSERT(pHeader802_3);

	pOSPkt = RTPKT_TO_OSPKT(pRxBlk->pRxPacket);

	pOSPkt->dev = get_netdev_from_bssid(pAd, FromWhichBSSID);
	pOSPkt->data = pRxBlk->pData;
	pOSPkt->len = pRxBlk->DataSize;
	skb_set_tail_pointer(pOSPkt, pOSPkt->len);

	/* */
	/* copy 802.3 header */
	/* */
	/* */

	NdisMoveMemory(skb_push(pOSPkt, LENGTH_802_3), pHeader802_3,
		       LENGTH_802_3);
}

void announce_802_3_packet(struct rt_rtmp_adapter *pAd, void *pPacket)
{

	struct sk_buff *pRxPkt;

	ASSERT(pPacket);

	pRxPkt = RTPKT_TO_OSPKT(pPacket);

	/* Push up the protocol stack */
	pRxPkt->protocol = eth_type_trans(pRxPkt, pRxPkt->dev);

	netif_rx(pRxPkt);
}

struct rt_rtmp_sg_list *
rt_get_sg_list_from_packet(void *pPacket, struct rt_rtmp_sg_list *sg)
{
	sg->NumberOfElements = 1;
	sg->Elements[0].Address = GET_OS_PKT_DATAPTR(pPacket);
	sg->Elements[0].Length = GET_OS_PKT_LEN(pPacket);
	return sg;
}

void hex_dump(char *str, unsigned char *pSrcBufVA, unsigned int SrcBufLen)
{
	unsigned char *pt;
	int x;

	if (RTDebugLevel < RT_DEBUG_TRACE)
		return;

	pt = pSrcBufVA;
	printk("%s: %p, len = %d\n", str, pSrcBufVA, SrcBufLen);
	for (x = 0; x < SrcBufLen; x++) {
		if (x % 16 == 0)
			printk("0x%04x : ", x);
		printk("%02x ", ((unsigned char)pt[x]));
		if (x % 16 == 15)
			printk("\n");
	}
	printk("\n");
}

/*
	========================================================================

	Routine Description:
		Send log message through wireless event

		Support standard iw_event with IWEVCUSTOM. It is used below.

		iwreq_data.data.flags is used to store event_flag that is defined by user.
		iwreq_data.data.length is the length of the event log.

		The format of the event log is composed of the entry's MAC address and
		the desired log message (refer to pWirelessEventText).

			ex: 11:22:33:44:55:66 has associated successfully

		p.s. The requirement of Wireless Extension is v15 or newer.

	========================================================================
*/
void RTMPSendWirelessEvent(struct rt_rtmp_adapter *pAd,
			   u16 Event_flag,
			   u8 *pAddr, u8 BssIdx, char Rssi)
{

	/*union         iwreq_data      wrqu; */
	char *pBuf = NULL, *pBufPtr = NULL;
	u16 event, type, BufLen;
	u8 event_table_len = 0;

	type = Event_flag & 0xFF00;
	event = Event_flag & 0x00FF;

	switch (type) {
	case IW_SYS_EVENT_FLAG_START:
		event_table_len = IW_SYS_EVENT_TYPE_NUM;
		break;

	case IW_SPOOF_EVENT_FLAG_START:
		event_table_len = IW_SPOOF_EVENT_TYPE_NUM;
		break;

	case IW_FLOOD_EVENT_FLAG_START:
		event_table_len = IW_FLOOD_EVENT_TYPE_NUM;
		break;
	}

	if (event_table_len == 0) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("%s : The type(%0x02x) is not valid.\n", __func__,
			  type));
		return;
	}

	if (event >= event_table_len) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("%s : The event(%0x02x) is not valid.\n", __func__,
			  event));
		return;
	}
	/*Allocate memory and copy the msg. */
	pBuf = kmalloc(IW_CUSTOM_MAX_LEN, GFP_ATOMIC);
	if (pBuf != NULL) {
		/*Prepare the payload */
		memset(pBuf, 0, IW_CUSTOM_MAX_LEN);

		pBufPtr = pBuf;

		if (pAddr)
			pBufPtr +=
			    sprintf(pBufPtr,
				    "(RT2860) STA(%02x:%02x:%02x:%02x:%02x:%02x) ",
				    PRINT_MAC(pAddr));
		else if (BssIdx < MAX_MBSSID_NUM)
			pBufPtr +=
			    sprintf(pBufPtr, "(RT2860) BSS(wlan%d) ", BssIdx);
		else
			pBufPtr += sprintf(pBufPtr, "(RT2860) ");

		if (type == IW_SYS_EVENT_FLAG_START)
			pBufPtr +=
			    sprintf(pBufPtr, "%s",
				    pWirelessSysEventText[event]);
		else if (type == IW_SPOOF_EVENT_FLAG_START)
			pBufPtr +=
			    sprintf(pBufPtr, "%s (RSSI=%d)",
				    pWirelessSpoofEventText[event], Rssi);
		else if (type == IW_FLOOD_EVENT_FLAG_START)
			pBufPtr +=
			    sprintf(pBufPtr, "%s",
				    pWirelessFloodEventText[event]);
		else
			pBufPtr += sprintf(pBufPtr, "%s", "unknown event");

		pBufPtr[pBufPtr - pBuf] = '\0';
		BufLen = pBufPtr - pBuf;

		RtmpOSWrielessEventSend(pAd, IWEVCUSTOM, Event_flag, NULL,
					(u8 *)pBuf, BufLen);
		/*DBGPRINT(RT_DEBUG_TRACE, ("%s : %s\n", __func__, pBuf)); */

		kfree(pBuf);
	} else
		DBGPRINT(RT_DEBUG_ERROR,
			 ("%s : Can't allocate memory for wireless event.\n",
			  __func__));
}

void send_monitor_packets(struct rt_rtmp_adapter *pAd, struct rt_rx_blk *pRxBlk)
{
	struct sk_buff *pOSPkt;
	struct rt_wlan_ng_prism2_header *ph;
	int rate_index = 0;
	u16 header_len = 0;
	u8 temp_header[40] = { 0 };

	u_int32_t ralinkrate[256] = { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108, 109, 110, 111, 112, 13, 26, 39, 52, 78, 104, 117, 130, 26, 52, 78, 104, 156, 208, 234, 260, 27, 54, 81, 108, 162, 216, 243, 270,	/* Last 38 */
		54, 108, 162, 216, 324, 432, 486, 540, 14, 29, 43, 57, 87, 115,
		    130, 144, 29, 59, 87, 115, 173, 230, 260, 288, 30, 60, 90,
		    120, 180, 240, 270, 300, 60, 120, 180, 240, 360, 480, 540,
		    600, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
		11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
		    27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
		    42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56,
		    57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
		    72, 73, 74, 75, 76, 77, 78, 79, 80
	};

	ASSERT(pRxBlk->pRxPacket);
	if (pRxBlk->DataSize < 10) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("%s : Size is too small! (%d)\n", __func__,
			  pRxBlk->DataSize));
		goto err_free_sk_buff;
	}

	if (pRxBlk->DataSize + sizeof(struct rt_wlan_ng_prism2_header) >
	    RX_BUFFER_AGGRESIZE) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("%s : Size is too large! (%zu)\n", __func__,
			  pRxBlk->DataSize + sizeof(struct rt_wlan_ng_prism2_header)));
		goto err_free_sk_buff;
	}

	pOSPkt = RTPKT_TO_OSPKT(pRxBlk->pRxPacket);
	pOSPkt->dev = get_netdev_from_bssid(pAd, BSS0);
	if (pRxBlk->pHeader->FC.Type == BTYPE_DATA) {
		pRxBlk->DataSize -= LENGTH_802_11;
		if ((pRxBlk->pHeader->FC.ToDs == 1) &&
		    (pRxBlk->pHeader->FC.FrDs == 1))
			header_len = LENGTH_802_11_WITH_ADDR4;
		else
			header_len = LENGTH_802_11;

		/* QOS */
		if (pRxBlk->pHeader->FC.SubType & 0x08) {
			header_len += 2;
			/* Data skip QOS contorl field */
			pRxBlk->DataSize -= 2;
		}
		/* Order bit: A-Ralink or HTC+ */
		if (pRxBlk->pHeader->FC.Order) {
			header_len += 4;
			/* Data skip HTC contorl field */
			pRxBlk->DataSize -= 4;
		}
		/* Copy Header */
		if (header_len <= 40)
			NdisMoveMemory(temp_header, pRxBlk->pData, header_len);

		/* skip HW padding */
		if (pRxBlk->RxD.L2PAD)
			pRxBlk->pData += (header_len + 2);
		else
			pRxBlk->pData += header_len;
	}			/*end if */

	if (pRxBlk->DataSize < pOSPkt->len) {
		skb_trim(pOSPkt, pRxBlk->DataSize);
	} else {
		skb_put(pOSPkt, (pRxBlk->DataSize - pOSPkt->len));
	}			/*end if */

	if ((pRxBlk->pData - pOSPkt->data) > 0) {
		skb_put(pOSPkt, (pRxBlk->pData - pOSPkt->data));
		skb_pull(pOSPkt, (pRxBlk->pData - pOSPkt->data));
	}			/*end if */

	if (skb_headroom(pOSPkt) < (sizeof(struct rt_wlan_ng_prism2_header) + header_len)) {
		if (pskb_expand_head
		    (pOSPkt, (sizeof(struct rt_wlan_ng_prism2_header) + header_len), 0,
		     GFP_ATOMIC)) {
			DBGPRINT(RT_DEBUG_ERROR,
				 ("%s : Reallocate header size of sk_buff fail!\n",
				  __func__));
			goto err_free_sk_buff;
		}		/*end if */
	}			/*end if */

	if (header_len > 0)
		NdisMoveMemory(skb_push(pOSPkt, header_len), temp_header,
			       header_len);

	ph = (struct rt_wlan_ng_prism2_header *)skb_push(pOSPkt,
						sizeof(struct rt_wlan_ng_prism2_header));
	NdisZeroMemory(ph, sizeof(struct rt_wlan_ng_prism2_header));

	ph->msgcode = DIDmsg_lnxind_wlansniffrm;
	ph->msglen = sizeof(struct rt_wlan_ng_prism2_header);
	strcpy((char *)ph->devname, (char *)pAd->net_dev->name);

	ph->hosttime.did = DIDmsg_lnxind_wlansniffrm_hosttime;
	ph->hosttime.status = 0;
	ph->hosttime.len = 4;
	ph->hosttime.data = jiffies;

	ph->mactime.did = DIDmsg_lnxind_wlansniffrm_mactime;
	ph->mactime.status = 0;
	ph->mactime.len = 0;
	ph->mactime.data = 0;

	ph->istx.did = DIDmsg_lnxind_wlansniffrm_istx;
	ph->istx.status = 0;
	ph->istx.len = 0;
	ph->istx.data = 0;

	ph->channel.did = DIDmsg_lnxind_wlansniffrm_channel;
	ph->channel.status = 0;
	ph->channel.len = 4;

	ph->channel.data = (u_int32_t) pAd->CommonCfg.Channel;

	ph->rssi.did = DIDmsg_lnxind_wlansniffrm_rssi;
	ph->rssi.status = 0;
	ph->rssi.len = 4;
	ph->rssi.data =
	    (u_int32_t) RTMPMaxRssi(pAd,
				    ConvertToRssi(pAd, pRxBlk->pRxWI->RSSI0,
						  RSSI_0), ConvertToRssi(pAd,
									 pRxBlk->
									 pRxWI->
									 RSSI1,
									 RSSI_1),
				    ConvertToRssi(pAd, pRxBlk->pRxWI->RSSI2,
						  RSSI_2));;

	ph->signal.did = DIDmsg_lnxind_wlansniffrm_signal;
	ph->signal.status = 0;
	ph->signal.len = 4;
	ph->signal.data = 0;	/*rssi + noise; */

	ph->noise.did = DIDmsg_lnxind_wlansniffrm_noise;
	ph->noise.status = 0;
	ph->noise.len = 4;
	ph->noise.data = 0;

	if (pRxBlk->pRxWI->PHYMODE >= MODE_HTMIX) {
		rate_index =
		    16 + ((u8)pRxBlk->pRxWI->BW * 16) +
		    ((u8)pRxBlk->pRxWI->ShortGI * 32) +
		    ((u8)pRxBlk->pRxWI->MCS);
	} else if (pRxBlk->pRxWI->PHYMODE == MODE_OFDM)
		rate_index = (u8)(pRxBlk->pRxWI->MCS) + 4;
	else
		rate_index = (u8)(pRxBlk->pRxWI->MCS);
	if (rate_index < 0)
		rate_index = 0;
	if (rate_index > 255)
		rate_index = 255;

	ph->rate.did = DIDmsg_lnxind_wlansniffrm_rate;
	ph->rate.status = 0;
	ph->rate.len = 4;
	ph->rate.data = ralinkrate[rate_index];

	ph->frmlen.did = DIDmsg_lnxind_wlansniffrm_frmlen;
	ph->frmlen.status = 0;
	ph->frmlen.len = 4;
	ph->frmlen.data = (u_int32_t) pRxBlk->DataSize;

	pOSPkt->pkt_type = PACKET_OTHERHOST;
	pOSPkt->protocol = eth_type_trans(pOSPkt, pOSPkt->dev);
	pOSPkt->ip_summed = CHECKSUM_NONE;
	netif_rx(pOSPkt);

	return;

err_free_sk_buff:
	RELEASE_NDIS_PACKET(pAd, pRxBlk->pRxPacket, NDIS_STATUS_FAILURE);
	return;

}

/*******************************************************************************

	Device IRQ related functions.

 *******************************************************************************/
int RtmpOSIRQRequest(struct net_device *pNetDev)
{
#ifdef RTMP_PCI_SUPPORT
	struct net_device *net_dev = pNetDev;
	struct rt_rtmp_adapter *pAd = NULL;
	int retval = 0;

	GET_PAD_FROM_NET_DEV(pAd, pNetDev);

	ASSERT(pAd);

	if (pAd->infType == RTMP_DEV_INF_PCI) {
		struct os_cookie *_pObj = (struct os_cookie *)(pAd->OS_Cookie);
		RTMP_MSI_ENABLE(pAd);
		retval =
		    request_irq(_pObj->pci_dev->irq, rt2860_interrupt, SA_SHIRQ,
				(net_dev)->name, (net_dev));
		if (retval != 0)
			printk("RT2860: request_irq  ERROR(%d)\n", retval);
	}

	return retval;
#else
	return 0;
#endif
}

int RtmpOSIRQRelease(struct net_device *pNetDev)
{
	struct net_device *net_dev = pNetDev;
	struct rt_rtmp_adapter *pAd = NULL;

	GET_PAD_FROM_NET_DEV(pAd, net_dev);

	ASSERT(pAd);

#ifdef RTMP_PCI_SUPPORT
	if (pAd->infType == RTMP_DEV_INF_PCI) {
		struct os_cookie *pObj = (struct os_cookie *)(pAd->OS_Cookie);
		synchronize_irq(pObj->pci_dev->irq);
		free_irq(pObj->pci_dev->irq, (net_dev));
		RTMP_MSI_DISABLE(pAd);
	}
#endif /* RTMP_PCI_SUPPORT // */

	return 0;
}

/*******************************************************************************

	File open/close related functions.

 *******************************************************************************/
struct file *RtmpOSFileOpen(char *pPath, int flag, int mode)
{
	struct file *filePtr;

	filePtr = filp_open(pPath, flag, 0);
	if (IS_ERR(filePtr)) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("%s(): Error %ld opening %s\n", __func__,
			  -PTR_ERR(filePtr), pPath));
	}

	return (struct file *)filePtr;
}

int RtmpOSFileClose(struct file *osfd)
{
	filp_close(osfd, NULL);
	return 0;
}

void RtmpOSFileSeek(struct file *osfd, int offset)
{
	osfd->f_pos = offset;
}

int RtmpOSFileRead(struct file *osfd, char *pDataPtr, int readLen)
{
	/* The object must have a read method */
	if (osfd->f_op && osfd->f_op->read) {
		return osfd->f_op->read(osfd, pDataPtr, readLen, &osfd->f_pos);
	} else {
		DBGPRINT(RT_DEBUG_ERROR, ("no file read method\n"));
		return -1;
	}
}

int RtmpOSFileWrite(struct file *osfd, char *pDataPtr, int writeLen)
{
	return osfd->f_op->write(osfd, pDataPtr, (size_t) writeLen,
				 &osfd->f_pos);
}

/*******************************************************************************

	Task create/management/kill related functions.

 *******************************************************************************/
int RtmpOSTaskKill(struct rt_rtmp_os_task *pTask)
{
	struct rt_rtmp_adapter *pAd;
	int ret = NDIS_STATUS_FAILURE;

	pAd = pTask->priv;

#ifdef KTHREAD_SUPPORT
	if (pTask->kthread_task) {
		kthread_stop(pTask->kthread_task);
		ret = NDIS_STATUS_SUCCESS;
	}
#else
	CHECK_PID_LEGALITY(pTask->taskPID) {
		printk("Terminate the task(%s) with pid(%d)!\n",
		       pTask->taskName, GET_PID_NUMBER(pTask->taskPID));
		mb();
		pTask->task_killed = 1;
		mb();
		ret = KILL_THREAD_PID(pTask->taskPID, SIGTERM, 1);
		if (ret) {
			printk(KERN_WARNING
			       "kill task(%s) with pid(%d) failed(retVal=%d)!\n",
			       pTask->taskName, GET_PID_NUMBER(pTask->taskPID),
			       ret);
		} else {
			wait_for_completion(&pTask->taskComplete);
			pTask->taskPID = THREAD_PID_INIT_VALUE;
			pTask->task_killed = 0;
			ret = NDIS_STATUS_SUCCESS;
		}
	}
#endif

	return ret;

}

int RtmpOSTaskNotifyToExit(struct rt_rtmp_os_task *pTask)
{

#ifndef KTHREAD_SUPPORT
	complete_and_exit(&pTask->taskComplete, 0);
#endif

	return 0;
}

void RtmpOSTaskCustomize(struct rt_rtmp_os_task *pTask)
{

#ifndef KTHREAD_SUPPORT

	daemonize((char *)&pTask->taskName[0] /*"%s",pAd->net_dev->name */);

	allow_signal(SIGTERM);
	allow_signal(SIGKILL);
	current->flags |= PF_NOFREEZE;

	/* signal that we've started the thread */
	complete(&pTask->taskComplete);

#endif
}

int RtmpOSTaskAttach(struct rt_rtmp_os_task *pTask,
			     IN int (*fn) (void *), IN void *arg)
{
	int status = NDIS_STATUS_SUCCESS;

#ifdef KTHREAD_SUPPORT
	pTask->task_killed = 0;
	pTask->kthread_task = NULL;
	pTask->kthread_task = kthread_run(fn, arg, pTask->taskName);
	if (IS_ERR(pTask->kthread_task))
		status = NDIS_STATUS_FAILURE;
#else
	pid_number = kernel_thread(fn, arg, RTMP_OS_MGMT_TASK_FLAGS);
	if (pid_number < 0) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("Attach task(%s) failed!\n", pTask->taskName));
		status = NDIS_STATUS_FAILURE;
	} else {
		pTask->taskPID = GET_PID(pid_number);

		/* Wait for the thread to start */
		wait_for_completion(&pTask->taskComplete);
		status = NDIS_STATUS_SUCCESS;
	}
#endif
	return status;
}

int RtmpOSTaskInit(struct rt_rtmp_os_task *pTask,
			   char *pTaskName, void * pPriv)
{
	int len;

	ASSERT(pTask);

#ifndef KTHREAD_SUPPORT
	NdisZeroMemory((u8 *)(pTask), sizeof(struct rt_rtmp_os_task));
#endif

	len = strlen(pTaskName);
	len =
	    len >
	    (RTMP_OS_TASK_NAME_LEN - 1) ? (RTMP_OS_TASK_NAME_LEN - 1) : len;
	NdisMoveMemory(&pTask->taskName[0], pTaskName, len);
	pTask->priv = pPriv;

#ifndef KTHREAD_SUPPORT
	RTMP_SEM_EVENT_INIT_LOCKED(&(pTask->taskSema));
	pTask->taskPID = THREAD_PID_INIT_VALUE;

	init_completion(&pTask->taskComplete);
#endif

	return NDIS_STATUS_SUCCESS;
}

void RTMP_IndicateMediaState(struct rt_rtmp_adapter *pAd)
{
	if (pAd->CommonCfg.bWirelessEvent) {
		if (pAd->IndicateMediaState == NdisMediaStateConnected) {
			RTMPSendWirelessEvent(pAd, IW_STA_LINKUP_EVENT_FLAG,
					      pAd->MacTab.Content[BSSID_WCID].
					      Addr, BSS0, 0);
		} else {
			RTMPSendWirelessEvent(pAd, IW_STA_LINKDOWN_EVENT_FLAG,
					      pAd->MacTab.Content[BSSID_WCID].
					      Addr, BSS0, 0);
		}
	}
}

int RtmpOSWrielessEventSend(struct rt_rtmp_adapter *pAd,
			    u32 eventType,
			    int flags,
			    u8 *pSrcMac,
			    u8 *pData, u32 dataLen)
{
	union iwreq_data wrqu;

	memset(&wrqu, 0, sizeof(wrqu));

	if (flags > -1)
		wrqu.data.flags = flags;

	if (pSrcMac)
		memcpy(wrqu.ap_addr.sa_data, pSrcMac, MAC_ADDR_LEN);

	if ((pData != NULL) && (dataLen > 0))
		wrqu.data.length = dataLen;

	wireless_send_event(pAd->net_dev, eventType, &wrqu, (char *)pData);
	return 0;
}

int RtmpOSNetDevAddrSet(struct net_device *pNetDev, u8 *pMacAddr)
{
	struct net_device *net_dev;
	struct rt_rtmp_adapter *pAd;

	net_dev = pNetDev;
	GET_PAD_FROM_NET_DEV(pAd, net_dev);

	{
		NdisZeroMemory(pAd->StaCfg.dev_name, 16);
		NdisMoveMemory(pAd->StaCfg.dev_name, net_dev->name,
			       strlen(net_dev->name));
	}

	NdisMoveMemory(net_dev->dev_addr, pMacAddr, 6);

	return 0;
}

/*
  *	Assign the network dev name for created Ralink WiFi interface.
  */
static int RtmpOSNetDevRequestName(struct rt_rtmp_adapter *pAd,
				   struct net_device *dev,
				   char *pPrefixStr, int devIdx)
{
	struct net_device *existNetDev;
	char suffixName[IFNAMSIZ];
	char desiredName[IFNAMSIZ];
	int ifNameIdx, prefixLen, slotNameLen;
	int Status;

	prefixLen = strlen(pPrefixStr);
	ASSERT((prefixLen < IFNAMSIZ));

	for (ifNameIdx = devIdx; ifNameIdx < 32; ifNameIdx++) {
		memset(suffixName, 0, IFNAMSIZ);
		memset(desiredName, 0, IFNAMSIZ);
		strncpy(&desiredName[0], pPrefixStr, prefixLen);

		sprintf(suffixName, "%d", ifNameIdx);

		slotNameLen = strlen(suffixName);
		ASSERT(((slotNameLen + prefixLen) < IFNAMSIZ));
		strcat(desiredName, suffixName);

		existNetDev = RtmpOSNetDevGetByName(dev, &desiredName[0]);
		if (existNetDev == NULL)
			break;
		else
			RtmpOSNetDeviceRefPut(existNetDev);
	}

	if (ifNameIdx < 32) {
		strcpy(&dev->name[0], &desiredName[0]);
		Status = NDIS_STATUS_SUCCESS;
	} else {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("Cannot request DevName with preifx(%s) and in range(0~32) as suffix from OS!\n",
			  pPrefixStr));
		Status = NDIS_STATUS_FAILURE;
	}

	return Status;
}

void RtmpOSNetDevClose(struct net_device *pNetDev)
{
	dev_close(pNetDev);
}

void RtmpOSNetDevFree(struct net_device *pNetDev)
{
	ASSERT(pNetDev);

	free_netdev(pNetDev);
}

int RtmpOSNetDevAlloc(struct net_device **new_dev_p, u32 privDataSize)
{
	/* assign it as null first. */
	*new_dev_p = NULL;

	DBGPRINT(RT_DEBUG_TRACE,
		 ("Allocate a net device with private data size=%d!\n",
		  privDataSize));
	*new_dev_p = alloc_etherdev(privDataSize);
	if (*new_dev_p)
		return NDIS_STATUS_SUCCESS;
	else
		return NDIS_STATUS_FAILURE;
}

struct net_device *RtmpOSNetDevGetByName(struct net_device *pNetDev, char *pDevName)
{
	struct net_device *pTargetNetDev = NULL;

	pTargetNetDev = dev_get_by_name(dev_net(pNetDev), pDevName);

	return pTargetNetDev;
}

void RtmpOSNetDeviceRefPut(struct net_device *pNetDev)
{
	/*
	   every time dev_get_by_name is called, and it has returned a valid struct
	   net_device*, dev_put should be called afterwards, because otherwise the
	   machine hangs when the device is unregistered (since dev->refcnt > 1).
	 */
	if (pNetDev)
		dev_put(pNetDev);
}

int RtmpOSNetDevDestory(struct rt_rtmp_adapter *pAd, struct net_device *pNetDev)
{

	/* TODO: Need to fix this */
	printk("WARNING: This function(%s) not implement yet!\n", __func__);
	return 0;
}

void RtmpOSNetDevDetach(struct net_device *pNetDev)
{
	unregister_netdev(pNetDev);
}

int RtmpOSNetDevAttach(struct net_device *pNetDev,
		       struct rt_rtmp_os_netdev_op_hook *pDevOpHook)
{
	int ret, rtnl_locked = FALSE;

	DBGPRINT(RT_DEBUG_TRACE, ("RtmpOSNetDevAttach()--->\n"));
	/* If we need hook some callback function to the net device structrue, now do it. */
	if (pDevOpHook) {
		struct rt_rtmp_adapter *pAd = NULL;

		GET_PAD_FROM_NET_DEV(pAd, pNetDev);

		pNetDev->netdev_ops = pDevOpHook->netdev_ops;

		/* OS specific flags, here we used to indicate if we are virtual interface */
		pNetDev->priv_flags = pDevOpHook->priv_flags;

		if (pAd->OpMode == OPMODE_STA) {
			pNetDev->wireless_handlers = &rt28xx_iw_handler_def;
		}

		/* copy the net device mac address to the net_device structure. */
		NdisMoveMemory(pNetDev->dev_addr, &pDevOpHook->devAddr[0],
			       MAC_ADDR_LEN);

		rtnl_locked = pDevOpHook->needProtcted;
	}

	if (rtnl_locked)
		ret = register_netdevice(pNetDev);
	else
		ret = register_netdev(pNetDev);

	DBGPRINT(RT_DEBUG_TRACE, ("<---RtmpOSNetDevAttach(), ret=%d\n", ret));
	if (ret == 0)
		return NDIS_STATUS_SUCCESS;
	else
		return NDIS_STATUS_FAILURE;
}

struct net_device *RtmpOSNetDevCreate(struct rt_rtmp_adapter *pAd,
			    int devType,
			    int devNum,
			    int privMemSize, char *pNamePrefix)
{
	struct net_device *pNetDev = NULL;
	int status;

	/* allocate a new network device */
	status = RtmpOSNetDevAlloc(&pNetDev, 0 /*privMemSize */);
	if (status != NDIS_STATUS_SUCCESS) {
		/* allocation fail, exit */
		DBGPRINT(RT_DEBUG_ERROR,
			 ("Allocate network device fail (%s)...\n",
			  pNamePrefix));
		return NULL;
	}

	/* find a available interface name, max 32 interfaces */
	status = RtmpOSNetDevRequestName(pAd, pNetDev, pNamePrefix, devNum);
	if (status != NDIS_STATUS_SUCCESS) {
		/* error! no any available ra name can be used! */
		DBGPRINT(RT_DEBUG_ERROR,
			 ("Assign interface name (%s with suffix 0~32) failed...\n",
			  pNamePrefix));
		RtmpOSNetDevFree(pNetDev);

		return NULL;
	} else {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("The name of the new %s interface is %s...\n",
			  pNamePrefix, pNetDev->name));
	}

	return pNetDev;
}
