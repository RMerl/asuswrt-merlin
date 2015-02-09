/* 
* Qos.c
*
*Copyright (C) 2010 Beceem Communications, Inc.
*
*This program is free software: you can redistribute it and/or modify 
*it under the terms of the GNU General Public License version 2 as
*published by the Free Software Foundation. 
*
*This program is distributed in the hope that it will be useful,but 
*WITHOUT ANY WARRANTY; without even the implied warranty of
*MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
*See the GNU General Public License for more details.
*
*You should have received a copy of the GNU General Public License
*along with this program. If not, write to the Free Software Foundation, Inc.,
*51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/


/**********************************************************************
*	This file contains the routines related to Quality of Service. 
***********************************************************************/

#include <headers.h>

BOOLEAN MatchSrcIpAddress(S_CLASSIFIER_RULE *pstClassifierRule,ULONG ulSrcIP);
BOOLEAN MatchTos(S_CLASSIFIER_RULE *pstClassifierRule,UCHAR ucTypeOfService);
BOOLEAN MatchSrcPort(S_CLASSIFIER_RULE *pstClassifierRule,USHORT ushSrcPort);
BOOLEAN MatchDestPort(S_CLASSIFIER_RULE *pstClassifierRule,USHORT ushDestPort);
BOOLEAN MatchProtocol(S_CLASSIFIER_RULE *pstClassifierRule,UCHAR ucProtocol);
BOOLEAN MatchDestIpAddress(S_CLASSIFIER_RULE *pstClassifierRule,ULONG ulDestIP);
USHORT ClassifyPacket(PMINI_ADAPTER Adapter,struct sk_buff* skb);
void EThCSGetPktInfo(PMINI_ADAPTER Adapter,PVOID pvEthPayload,PS_ETHCS_PKT_INFO pstEthCsPktInfo);
BOOLEAN EThCSClassifyPkt(PMINI_ADAPTER Adapter,struct sk_buff* skb,PS_ETHCS_PKT_INFO pstEthCsPktInfo,S_CLASSIFIER_RULE *pstClassifierRule, B_UINT8 EthCSCupport);

/*******************************************************************
* Function    - MatchSrcIpAddress()
* 
* Description - Checks whether the Source IP address from the packet
*				matches with that of Queue.
* 
* Parameters  - pstClassifierRule: Pointer to the packet info structure.
* 			  - ulSrcIP	    : Source IP address from the packet.
*
* Returns     - TRUE(If address matches) else FAIL .
*********************************************************************/
BOOLEAN MatchSrcIpAddress(S_CLASSIFIER_RULE *pstClassifierRule,ULONG ulSrcIP)
{
    UCHAR 	ucLoopIndex=0;

    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	
    ulSrcIP=ntohl(ulSrcIP);
    if(0 == pstClassifierRule->ucIPSourceAddressLength)
       	return TRUE;
    for(ucLoopIndex=0; ucLoopIndex < (pstClassifierRule->ucIPSourceAddressLength);ucLoopIndex++)
    {
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Src Ip Address Mask:0x%x PacketIp:0x%x and Classification:0x%x", (UINT)pstClassifierRule->stSrcIpAddress.ulIpv4Mask[ucLoopIndex], (UINT)ulSrcIP, (UINT)pstClassifierRule->stSrcIpAddress.ulIpv6Addr[ucLoopIndex]);
		if((pstClassifierRule->stSrcIpAddress.ulIpv4Mask[ucLoopIndex] & ulSrcIP)== 
				(pstClassifierRule->stSrcIpAddress.ulIpv4Addr[ucLoopIndex] & pstClassifierRule->stSrcIpAddress.ulIpv4Mask[ucLoopIndex] ))
       	{
       		return TRUE;
       	}
    }
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Src Ip Address Not Matched");
   	return FALSE;
}


/*******************************************************************
* Function    - MatchDestIpAddress() 
*
* Description - Checks whether the Destination IP address from the packet
*				matches with that of Queue.
* 
* Parameters  - pstClassifierRule: Pointer to the packet info structure.
* 			  - ulDestIP    : Destination IP address from the packet.
*
* Returns     - TRUE(If address matches) else FAIL .
*********************************************************************/
BOOLEAN MatchDestIpAddress(S_CLASSIFIER_RULE *pstClassifierRule,ULONG ulDestIP)
{
	UCHAR 	ucLoopIndex=0;
    PMINI_ADAPTER	Adapter = GET_BCM_ADAPTER(gblpnetdev);
    	
	ulDestIP=ntohl(ulDestIP);
    if(0 == pstClassifierRule->ucIPDestinationAddressLength)
       	return TRUE;
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Destination Ip Address 0x%x 0x%x 0x%x  ", (UINT)ulDestIP, (UINT)pstClassifierRule->stDestIpAddress.ulIpv4Mask[ucLoopIndex], (UINT)pstClassifierRule->stDestIpAddress.ulIpv4Addr[ucLoopIndex]);

    for(ucLoopIndex=0;ucLoopIndex<(pstClassifierRule->ucIPDestinationAddressLength);ucLoopIndex++)
    {
		if((pstClassifierRule->stDestIpAddress.ulIpv4Mask[ucLoopIndex] & ulDestIP)== 
					(pstClassifierRule->stDestIpAddress.ulIpv4Addr[ucLoopIndex] & pstClassifierRule->stDestIpAddress.ulIpv4Mask[ucLoopIndex]))
       	{
       		return TRUE;
       	}
    }
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Destination Ip Address Not Matched");
    return FALSE;
}


/************************************************************************
* Function    - MatchTos()
* 
* Description - Checks the TOS from the packet matches with that of queue.
* 
* Parameters  - pstClassifierRule   : Pointer to the packet info structure.
* 			  - ucTypeOfService: TOS from the packet.
*
* Returns     - TRUE(If address matches) else FAIL.
**************************************************************************/
BOOLEAN MatchTos(S_CLASSIFIER_RULE *pstClassifierRule,UCHAR ucTypeOfService)
{
    
	PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    if( 3 != pstClassifierRule->ucIPTypeOfServiceLength )
       	return TRUE;

    if(((pstClassifierRule->ucTosMask & ucTypeOfService)<=pstClassifierRule->ucTosHigh) && ((pstClassifierRule->ucTosMask & ucTypeOfService)>=pstClassifierRule->ucTosLow))
    {
       	return TRUE;
    }
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Type Of Service Not Matched");
    return FALSE;
}


/***************************************************************************
* Function    - MatchProtocol()
* 
* Description - Checks the protocol from the packet matches with that of queue.
* 
* Parameters  - pstClassifierRule: Pointer to the packet info structure.
* 			  - ucProtocol	: Protocol from the packet.
*
* Returns     - TRUE(If address matches) else FAIL.
****************************************************************************/
BOOLEAN MatchProtocol(S_CLASSIFIER_RULE *pstClassifierRule,UCHAR ucProtocol)
{
   	UCHAR 	ucLoopIndex=0;
	PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	if(0 == pstClassifierRule->ucProtocolLength)
      	return TRUE;
	for(ucLoopIndex=0;ucLoopIndex<pstClassifierRule->ucProtocolLength;ucLoopIndex++)
    {
       	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Protocol:0x%X Classification Protocol:0x%X",ucProtocol,pstClassifierRule->ucProtocol[ucLoopIndex]);
       	if(pstClassifierRule->ucProtocol[ucLoopIndex]==ucProtocol)
       	{
       		return TRUE;
       	}
    }
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Protocol Not Matched");
   	return FALSE;
}


/***********************************************************************
* Function    - MatchSrcPort()
* 
* Description - Checks, Source port from the packet matches with that of queue.
* 
* Parameters  - pstClassifierRule: Pointer to the packet info structure.
* 			  - ushSrcPort	: Source port from the packet.
*
* Returns     - TRUE(If address matches) else FAIL.
***************************************************************************/
BOOLEAN MatchSrcPort(S_CLASSIFIER_RULE *pstClassifierRule,USHORT ushSrcPort)
{
    	UCHAR 	ucLoopIndex=0;
		
		PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
		
    	
    	if(0 == pstClassifierRule->ucSrcPortRangeLength)
        	return TRUE;
    	for(ucLoopIndex=0;ucLoopIndex<pstClassifierRule->ucSrcPortRangeLength;ucLoopIndex++)
    	{
        	if(ushSrcPort <= pstClassifierRule->usSrcPortRangeHi[ucLoopIndex] &&
		    ushSrcPort >= pstClassifierRule->usSrcPortRangeLo[ucLoopIndex])
	    	{
		    	return TRUE;
	    	}
    	}
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Src Port: %x Not Matched ",ushSrcPort);
    	return FALSE;
}


/***********************************************************************
* Function    - MatchDestPort()
* 
* Description - Checks, Destination port from packet matches with that of queue.
* 
* Parameters  - pstClassifierRule: Pointer to the packet info structure.
* 			  - ushDestPort	: Destination port from the packet.
*
* Returns     - TRUE(If address matches) else FAIL.
***************************************************************************/
BOOLEAN MatchDestPort(S_CLASSIFIER_RULE *pstClassifierRule,USHORT ushDestPort)
{
    	UCHAR 	ucLoopIndex=0;
		PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
        
    	if(0 == pstClassifierRule->ucDestPortRangeLength)
        	return TRUE;
    
    	for(ucLoopIndex=0;ucLoopIndex<pstClassifierRule->ucDestPortRangeLength;ucLoopIndex++)
    	{
        	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Matching Port:0x%X   0x%X  0x%X",ushDestPort,pstClassifierRule->usDestPortRangeLo[ucLoopIndex],pstClassifierRule->usDestPortRangeHi[ucLoopIndex]);

 		if(ushDestPort <= pstClassifierRule->usDestPortRangeHi[ucLoopIndex] &&
		    ushDestPort >= pstClassifierRule->usDestPortRangeLo[ucLoopIndex])
	    	{
		    return TRUE;
	    	}
    	}
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Dest Port: %x Not Matched",ushDestPort);
    	return FALSE;
}
/**
@ingroup tx_functions
Compares IPV4 Ip address and port number
@return Queue Index.
*/
USHORT	IpVersion4(PMINI_ADAPTER Adapter, /**< Pointer to the driver control structure */
					struct iphdr *iphd, /**<Pointer to the IP Hdr of the packet*/
					S_CLASSIFIER_RULE *pstClassifierRule )
{
	//IPHeaderFormat 		*pIpHeader=NULL;
	xporthdr     		*xprt_hdr=NULL;
	BOOLEAN	bClassificationSucceed=FALSE;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "========>");

	xprt_hdr=(xporthdr *)((PUCHAR)iphd + sizeof(struct iphdr));

	do {
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Trying to see Direction = %d %d", 
			pstClassifierRule->ucDirection, 
			pstClassifierRule->usVCID_Value);

		//Checking classifier validity
		if(!pstClassifierRule->bUsed || pstClassifierRule->ucDirection == DOWNLINK_DIR)
		{	
			bClassificationSucceed = FALSE;
			break;
		}

		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "is IPv6 check!");
		if(pstClassifierRule->bIpv6Protocol)
			break;

		//**************Checking IP header parameter**************************//  
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Trying to match Source IP Address");
		if(FALSE == (bClassificationSucceed =  
			MatchSrcIpAddress(pstClassifierRule, iphd->saddr)))
			break;
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Source IP Address Matched");

		if(FALSE == (bClassificationSucceed = 
			MatchDestIpAddress(pstClassifierRule, iphd->daddr)))
			break;
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Destination IP Address Matched");

		if(FALSE == (bClassificationSucceed = 
			MatchTos(pstClassifierRule, iphd->tos)))
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "TOS Match failed\n");
			break;
		}
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "TOS Matched");

		if(FALSE == (bClassificationSucceed = 
			MatchProtocol(pstClassifierRule,iphd->protocol)))
			break;
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Protocol Matched");

		//if protocol is not TCP or UDP then no need of comparing source port and destination port
		if(iphd->protocol!=TCP && iphd->protocol!=UDP)
			break;
#if 0
		//check if memory is available of src and Dest port
		if(ETH_AND_IP_HEADER_LEN + L4_SRC_PORT_LEN + L4_DEST_PORT_LEN > Packet->len)
		{
			//This is not an erroneous condition and pkt will be checked for next classification.
			bClassificationSucceed = FALSE;
			break;
		}
#endif
		//******************Checking Transport Layer Header field if present *****************//
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Source Port %04x", 
			(iphd->protocol==UDP)?xprt_hdr->uhdr.source:xprt_hdr->thdr.source);

		if(FALSE == (bClassificationSucceed = 
			MatchSrcPort(pstClassifierRule, 
				ntohs((iphd->protocol == UDP)?
				xprt_hdr->uhdr.source:xprt_hdr->thdr.source))))
			break;
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Src Port Matched");

		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Destination Port %04x", 
			(iphd->protocol==UDP)?xprt_hdr->uhdr.dest:
			xprt_hdr->thdr.dest);
		if(FALSE == (bClassificationSucceed = 
			MatchDestPort(pstClassifierRule,
			ntohs((iphd->protocol == UDP)?
			xprt_hdr->uhdr.dest:xprt_hdr->thdr.dest))))
			break;
	} while(0);

	if(TRUE==bClassificationSucceed)
	{
		INT iMatchedSFQueueIndex = 0;
		iMatchedSFQueueIndex = SearchSfid(Adapter,pstClassifierRule->ulSFID);
		if(iMatchedSFQueueIndex >= NO_OF_QUEUES)
		{
			bClassificationSucceed = FALSE;
		}
		else
		{
			if(FALSE == Adapter->PackInfo[iMatchedSFQueueIndex].bActive) 
			{					
				bClassificationSucceed = FALSE;
			}
		}
	}

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "IpVersion4 <==========");

	return bClassificationSucceed;
}
/**
@ingroup tx_functions
@return  Queue Index based on priority.
*/
USHORT GetPacketQueueIndex(PMINI_ADAPTER Adapter, /**<Pointer to the driver control structure */
								struct sk_buff* Packet /**< Pointer to the Packet to be sent*/
								)
{
	USHORT			usIndex=-1;
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, QUEUE_INDEX, DBG_LVL_ALL, "=====>");	
	
	if(NULL==Adapter || NULL==Packet)
	{	
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, QUEUE_INDEX, DBG_LVL_ALL, "Got NULL Values<======");
		return -1;
	}

	usIndex = ClassifyPacket(Adapter,Packet);

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, QUEUE_INDEX, DBG_LVL_ALL, "Got Queue Index %x",usIndex);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, QUEUE_INDEX, DBG_LVL_ALL, "GetPacketQueueIndex <==============");
	return usIndex;
}

VOID PruneQueueAllSF(PMINI_ADAPTER Adapter)
{
	UINT iIndex = 0;
	
	for(iIndex = 0; iIndex < HiPriority; iIndex++)
	{
		if(!Adapter->PackInfo[iIndex].bValid)
			continue;

		PruneQueue(Adapter, iIndex);
	}
}


/**
@ingroup tx_functions
This function checks if the max queue size for a queue
is less than number of bytes in the queue. If so - 
drops packets from the Head till the number of bytes is
less than or equal to max queue size for the queue.
*/
VOID PruneQueue(PMINI_ADAPTER Adapter,/**<Pointer to the driver control structure*/
					INT iIndex/**<Queue Index*/
					)
{
	struct sk_buff* PacketToDrop=NULL;
	struct net_device_stats*  netstats=NULL;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, PRUNE_QUEUE, DBG_LVL_ALL, "=====> Index %d",iIndex);	

   	if(iIndex == HiPriority)
       	return;

	if(!Adapter || (iIndex < 0) || (iIndex > HiPriority))
		return;

	/* To Store the netdevice statistic */
	netstats = &((PLINUX_DEP_DATA)Adapter->pvOsDepData)->netstats;

	spin_lock_bh(&Adapter->PackInfo[iIndex].SFQueueLock);	

	while(1)
//	while((UINT)Adapter->PackInfo[iIndex].uiCurrentPacketsOnHost > 
//		SF_MAX_ALLOWED_PACKETS_TO_BACKUP)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, PRUNE_QUEUE, DBG_LVL_ALL, "uiCurrentBytesOnHost:%x uiMaxBucketSize :%x",
		Adapter->PackInfo[iIndex].uiCurrentBytesOnHost,
		Adapter->PackInfo[iIndex].uiMaxBucketSize);

		PacketToDrop = Adapter->PackInfo[iIndex].FirstTxQueue;

		if(PacketToDrop == NULL)
			break;	
		if((Adapter->PackInfo[iIndex].uiCurrentPacketsOnHost < SF_MAX_ALLOWED_PACKETS_TO_BACKUP) && 
			((1000*(jiffies - *((B_UINT32 *)(PacketToDrop->cb)+SKB_CB_LATENCY_OFFSET))/HZ) <= Adapter->PackInfo[iIndex].uiMaxLatency))
			break;
		
		if(PacketToDrop)
		{
			if(netstats)
				netstats->tx_dropped++;
			atomic_inc(&Adapter->TxDroppedPacketCount);
			DEQUEUEPACKET(Adapter->PackInfo[iIndex].FirstTxQueue,
						Adapter->PackInfo[iIndex].LastTxQueue);
			/// update current bytes and packets count
			Adapter->PackInfo[iIndex].uiCurrentBytesOnHost -= 
				PacketToDrop->len;
			Adapter->PackInfo[iIndex].uiCurrentPacketsOnHost--;
			/// update dropped bytes and packets counts 
			Adapter->PackInfo[iIndex].uiDroppedCountBytes += PacketToDrop->len;
			Adapter->PackInfo[iIndex].uiDroppedCountPackets++;
			bcm_kfree_skb(PacketToDrop);
			
		}

		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, PRUNE_QUEUE, DBG_LVL_ALL, "Dropped Bytes:%x Dropped Packets:%x",
			Adapter->PackInfo[iIndex].uiDroppedCountBytes,
			Adapter->PackInfo[iIndex].uiDroppedCountPackets);

		atomic_dec(&Adapter->TotalPacketCount);
		Adapter->bcm_jiffies = jiffies;
	}

	spin_unlock_bh(&Adapter->PackInfo[iIndex].SFQueueLock); 
	
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, PRUNE_QUEUE, DBG_LVL_ALL, "TotalPacketCount:%x",
		atomic_read(&Adapter->TotalPacketCount));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, PRUNE_QUEUE, DBG_LVL_ALL, "<=====");	
}

VOID flush_all_queues(PMINI_ADAPTER Adapter)
{
	INT		iQIndex;
	UINT	uiTotalPacketLength;
	struct sk_buff*				PacketToDrop=NULL;
	struct net_device_stats*  	netstats=NULL;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, DBG_LVL_ALL, "=====>");	
	/* To Store the netdevice statistic */
	netstats = &((PLINUX_DEP_DATA)Adapter->pvOsDepData)->netstats;

//	down(&Adapter->data_packet_queue_lock);
	for(iQIndex=LowPriority; iQIndex<HiPriority; iQIndex++)
	{
		spin_lock_bh(&Adapter->PackInfo[iQIndex].SFQueueLock);	
		while(Adapter->PackInfo[iQIndex].FirstTxQueue)
		{
			PacketToDrop = Adapter->PackInfo[iQIndex].FirstTxQueue;
			if(PacketToDrop)
			{
				uiTotalPacketLength = PacketToDrop->len;
				netstats->tx_dropped++;
				atomic_inc(&Adapter->TxDroppedPacketCount);
			}
			else
				uiTotalPacketLength = 0;

			DEQUEUEPACKET(Adapter->PackInfo[iQIndex].FirstTxQueue,
						Adapter->PackInfo[iQIndex].LastTxQueue);

			/* Free the skb */	
			bcm_kfree_skb(PacketToDrop);
		
			/// update current bytes and packets count
			Adapter->PackInfo[iQIndex].uiCurrentBytesOnHost -= uiTotalPacketLength;
			Adapter->PackInfo[iQIndex].uiCurrentPacketsOnHost--;

			/// update dropped bytes and packets counts 
			Adapter->PackInfo[iQIndex].uiDroppedCountBytes += uiTotalPacketLength;
			Adapter->PackInfo[iQIndex].uiDroppedCountPackets++;

			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, DBG_LVL_ALL, "Dropped Bytes:%x Dropped Packets:%x",
					Adapter->PackInfo[iQIndex].uiDroppedCountBytes,
					Adapter->PackInfo[iQIndex].uiDroppedCountPackets);
			atomic_dec(&Adapter->TotalPacketCount);
		}
		spin_unlock_bh(&Adapter->PackInfo[iQIndex].SFQueueLock);	
	}
//	up(&Adapter->data_packet_queue_lock);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, DBG_LVL_ALL, "<=====");	
}

USHORT ClassifyPacket(PMINI_ADAPTER Adapter,struct sk_buff* skb)
{
	INT			uiLoopIndex=0;
	S_CLASSIFIER_RULE *pstClassifierRule = NULL;
	S_ETHCS_PKT_INFO stEthCsPktInfo;
	PVOID pvEThPayload = NULL;
	struct iphdr 		*pIpHeader = NULL;
	INT	  uiSfIndex=0;
	USHORT	usIndex=Adapter->usBestEffortQueueIndex;
	BOOLEAN	bFragmentedPkt=FALSE,bClassificationSucceed=FALSE;
	USHORT	usCurrFragment =0;

	PTCP_HEADER pTcpHeader;
	UCHAR IpHeaderLength;
	UCHAR TcpHeaderLength;

	pvEThPayload = skb->data;
	*((UINT32*) (skb->cb) +SKB_CB_TCPACK_OFFSET ) = 0;
	EThCSGetPktInfo(Adapter,pvEThPayload,&stEthCsPktInfo);

	switch(stEthCsPktInfo.eNwpktEthFrameType)
	{
		case eEth802LLCFrame:
		{
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "ClassifyPacket : 802LLCFrame\n");
            pIpHeader = pvEThPayload + sizeof(ETH_CS_802_LLC_FRAME);
			break;
		}
	
		case eEth802LLCSNAPFrame:
		{
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "ClassifyPacket : 802LLC SNAP Frame\n");
			pIpHeader = pvEThPayload + sizeof(ETH_CS_802_LLC_SNAP_FRAME);
			break;
		}
		case eEth802QVLANFrame:
		{
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "ClassifyPacket : 802.1Q VLANFrame\n");
			pIpHeader = pvEThPayload + sizeof(ETH_CS_802_Q_FRAME);
			break;
		}
		case eEthOtherFrame:
		{
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "ClassifyPacket : ETH Other Frame\n");
			pIpHeader = pvEThPayload + sizeof(ETH_CS_ETH2_FRAME);
			break;
		}
		default:
		{
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "ClassifyPacket : Unrecognized ETH Frame\n");
			pIpHeader = pvEThPayload + sizeof(ETH_CS_ETH2_FRAME);
			break;
		}
	}

	if(stEthCsPktInfo.eNwpktIPFrameType == eIPv4Packet) 
	{
		usCurrFragment = (ntohs(pIpHeader->frag_off) & IP_OFFSET);
		if((ntohs(pIpHeader->frag_off) & IP_MF) || usCurrFragment) 
			bFragmentedPkt = TRUE;

		if(bFragmentedPkt)
		{
				//Fragmented  Packet. Get Frag Classifier Entry.
			pstClassifierRule = GetFragIPClsEntry(Adapter,pIpHeader->id, pIpHeader->saddr);
			if(pstClassifierRule)
			{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,"It is next Fragmented pkt");
					bClassificationSucceed=TRUE;
			}		
			if(!(ntohs(pIpHeader->frag_off) & IP_MF))
			{
				//Fragmented Last packet . Remove Frag Classifier Entry
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,"This is the last fragmented Pkt");
				DelFragIPClsEntry(Adapter,pIpHeader->id, pIpHeader->saddr);
			}
		}
	}

	for(uiLoopIndex = MAX_CLASSIFIERS - 1; uiLoopIndex >= 0; uiLoopIndex--)
	{
		if (Adapter->device_removed)
		{
			bClassificationSucceed = FALSE;
			break;
		}
	
		if(bClassificationSucceed)
			break;
		//Iterate through all classifiers which are already in order of priority
		//to classify the packet until match found
		do
		{
			if(FALSE==Adapter->astClassifierTable[uiLoopIndex].bUsed)
			{
				bClassificationSucceed=FALSE;
				break;
			}
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "Adapter->PackInfo[%d].bvalid=True\n",uiLoopIndex);

			if(0 == Adapter->astClassifierTable[uiLoopIndex].ucDirection)
			{
				bClassificationSucceed=FALSE;//cannot be processed for classification.
				break;						// it is a down link connection
			}

			pstClassifierRule = &Adapter->astClassifierTable[uiLoopIndex];

			uiSfIndex = SearchSfid(Adapter,pstClassifierRule->ulSFID);
			if(uiSfIndex > NO_OF_QUEUES)
			{
				BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "Queue Not Valid. SearchSfid for this classifier Failed\n");
				break;
			}

			if(Adapter->PackInfo[uiSfIndex].bEthCSSupport)
			{

				if(eEthUnsupportedFrame==stEthCsPktInfo.eNwpktEthFrameType)
				{
					BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  " ClassifyPacket : Packet Not a Valid Supported Ethernet Frame \n");
					bClassificationSucceed = FALSE;
					break;
				}
					

			
				BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "Performing ETH CS Classification on Classifier Rule ID : %x Service Flow ID : %lx\n",pstClassifierRule->uiClassifierRuleIndex,Adapter->PackInfo[uiSfIndex].ulSFID);
				bClassificationSucceed = EThCSClassifyPkt(Adapter,skb,&stEthCsPktInfo,pstClassifierRule, Adapter->PackInfo[uiSfIndex].bEthCSSupport);
				
				if(!bClassificationSucceed)
				{
					BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "ClassifyPacket : Ethernet CS Classification Failed\n");
					break;
				}
			}
			
			else // No ETH Supported on this SF
			{
				if(eEthOtherFrame != stEthCsPktInfo.eNwpktEthFrameType)
				{
					BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  " ClassifyPacket : Packet Not a 802.3 Ethernet Frame... hence not allowed over non-ETH CS SF \n");
					bClassificationSucceed = FALSE;
					break;
				}
			}
			
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "Proceeding to IP CS Clasification");
			
			if(Adapter->PackInfo[uiSfIndex].bIPCSSupport)
			{
				
				if(stEthCsPktInfo.eNwpktIPFrameType == eNonIPPacket)
				{
					BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  " ClassifyPacket : Packet is Not an IP Packet \n");
					bClassificationSucceed = FALSE;
					break;
				}
				BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "Dump IP Header : \n");
				DumpFullPacket((PUCHAR)pIpHeader,20);

				if(stEthCsPktInfo.eNwpktIPFrameType == eIPv4Packet) 
					bClassificationSucceed = IpVersion4(Adapter,pIpHeader,pstClassifierRule);
				else if(stEthCsPktInfo.eNwpktIPFrameType == eIPv6Packet)
					bClassificationSucceed = IpVersion6(Adapter,pIpHeader,pstClassifierRule);
			}

		}while(0);
	}

	if(bClassificationSucceed == TRUE)
	{			
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "CF id : %d, SF ID is =%lu",pstClassifierRule->uiClassifierRuleIndex, pstClassifierRule->ulSFID);	

		//Store The matched Classifier in SKB
		*((UINT32*)(skb->cb)+SKB_CB_CLASSIFICATION_OFFSET) = pstClassifierRule->uiClassifierRuleIndex;	
		if((TCP == pIpHeader->protocol ) && !bFragmentedPkt && (ETH_AND_IP_HEADER_LEN + TCP_HEADER_LEN <= skb->len) )
		{   
			 IpHeaderLength   = pIpHeader->ihl;
			 pTcpHeader = (PTCP_HEADER)(((PUCHAR)pIpHeader)+(IpHeaderLength*4));
			 TcpHeaderLength  = GET_TCP_HEADER_LEN(pTcpHeader->HeaderLength);
			
			if((pTcpHeader->ucFlags & TCP_ACK) &&
			   (ntohs(pIpHeader->tot_len) == (IpHeaderLength*4)+(TcpHeaderLength*4)))
			{
    			*((UINT32*) (skb->cb) +SKB_CB_TCPACK_OFFSET ) = TCP_ACK;
			}
		}
		        
		usIndex = SearchSfid(Adapter, pstClassifierRule->ulSFID);
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "index is	=%d", usIndex); 

		//If this is the first fragment of a Fragmented pkt, add this CF. Only This CF should be used for all other fragment of this Pkt.
		if(bFragmentedPkt && (usCurrFragment == 0))
		{
			//First Fragment of Fragmented Packet. Create Frag CLS Entry
			S_FRAGMENTED_PACKET_INFO stFragPktInfo;
			stFragPktInfo.bUsed = TRUE;
			stFragPktInfo.ulSrcIpAddress = pIpHeader->saddr;
			stFragPktInfo.usIpIdentification = pIpHeader->id;
			stFragPktInfo.pstMatchedClassifierEntry = pstClassifierRule;
			stFragPktInfo.bOutOfOrderFragment = FALSE;
			AddFragIPClsEntry(Adapter,&stFragPktInfo);
		}

		
	}	
	
	if(bClassificationSucceed)
		return usIndex;
	else
		return INVALID_QUEUE_INDEX;
}

BOOLEAN EthCSMatchSrcMACAddress(S_CLASSIFIER_RULE *pstClassifierRule,PUCHAR Mac)
{
	UINT i=0;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	if(pstClassifierRule->ucEthCSSrcMACLen==0)
		return TRUE;
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "%s \n",__FUNCTION__);
	for(i=0;i<MAC_ADDRESS_SIZE;i++)
	{
		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "SRC MAC[%x] = %x ClassifierRuleSrcMAC = %x Mask : %x\n",i,Mac[i],pstClassifierRule->au8EThCSSrcMAC[i],pstClassifierRule->au8EThCSSrcMACMask[i]);
		if((pstClassifierRule->au8EThCSSrcMAC[i] & pstClassifierRule->au8EThCSSrcMACMask[i])!=
			(Mac[i] & pstClassifierRule->au8EThCSSrcMACMask[i]))
			return FALSE;
	}
	return TRUE;
}

BOOLEAN EthCSMatchDestMACAddress(S_CLASSIFIER_RULE *pstClassifierRule,PUCHAR Mac)
{
	UINT i=0;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	if(pstClassifierRule->ucEthCSDestMACLen==0)
		return TRUE;
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "%s \n",__FUNCTION__);
	for(i=0;i<MAC_ADDRESS_SIZE;i++)
	{
		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "SRC MAC[%x] = %x ClassifierRuleSrcMAC = %x Mask : %x\n",i,Mac[i],pstClassifierRule->au8EThCSDestMAC[i],pstClassifierRule->au8EThCSDestMACMask[i]);
		if((pstClassifierRule->au8EThCSDestMAC[i] & pstClassifierRule->au8EThCSDestMACMask[i])!=
			(Mac[i] & pstClassifierRule->au8EThCSDestMACMask[i]))
			return FALSE;
	}
	return TRUE;
}

BOOLEAN EthCSMatchEThTypeSAP(S_CLASSIFIER_RULE *pstClassifierRule,struct sk_buff* skb,PS_ETHCS_PKT_INFO pstEthCsPktInfo)
{
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	if((pstClassifierRule->ucEtherTypeLen==0)||
		(pstClassifierRule->au8EthCSEtherType[0] == 0))
		return TRUE;

	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "%s SrcEtherType:%x CLS EtherType[0]:%x\n",__FUNCTION__,pstEthCsPktInfo->usEtherType,pstClassifierRule->au8EthCSEtherType[0]);
	if(pstClassifierRule->au8EthCSEtherType[0] == 1)
	{
		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "%s  CLS EtherType[1]:%x EtherType[2]:%x\n",__FUNCTION__,pstClassifierRule->au8EthCSEtherType[1],pstClassifierRule->au8EthCSEtherType[2]);

		if(memcmp(&pstEthCsPktInfo->usEtherType,&pstClassifierRule->au8EthCSEtherType[1],2)==0)
			return TRUE;
		else
			return FALSE;
	}

	if(pstClassifierRule->au8EthCSEtherType[0] == 2)
	{
		if(eEth802LLCFrame != pstEthCsPktInfo->eNwpktEthFrameType)
			return FALSE;

		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "%s  EthCS DSAP:%x EtherType[2]:%x\n",__FUNCTION__,pstEthCsPktInfo->ucDSAP,pstClassifierRule->au8EthCSEtherType[2]);
		if(pstEthCsPktInfo->ucDSAP == pstClassifierRule->au8EthCSEtherType[2])
			return TRUE;
		else
			return FALSE;
		
	}

	return FALSE;

}

BOOLEAN EthCSMatchVLANRules(S_CLASSIFIER_RULE *pstClassifierRule,struct sk_buff* skb,PS_ETHCS_PKT_INFO pstEthCsPktInfo)
{
	BOOLEAN bClassificationSucceed = FALSE;
	USHORT usVLANID;
	B_UINT8 uPriority = 0;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);

	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "%s  CLS UserPrio:%x CLS VLANID:%x\n",__FUNCTION__,ntohs(*((USHORT *)pstClassifierRule->usUserPriority)),pstClassifierRule->usVLANID);

	/* In case FW didn't recieve the TLV, the priority field should be ignored */
	if(pstClassifierRule->usValidityBitMap & (1<<PKT_CLASSIFICATION_USER_PRIORITY_VALID))
	{
		if(pstEthCsPktInfo->eNwpktEthFrameType!=eEth802QVLANFrame)
				return FALSE;

		uPriority = (ntohs(*(USHORT *)(skb->data + sizeof(ETH_HEADER_STRUC))) & 0xF000) >> 13;
		
		if((uPriority >= pstClassifierRule->usUserPriority[0]) && (uPriority <= pstClassifierRule->usUserPriority[1]))
				bClassificationSucceed = TRUE;
		
		if(!bClassificationSucceed)
			return FALSE;
	}

	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "ETH CS 802.1 D  User Priority Rule Matched\n");

	bClassificationSucceed = FALSE;
	
	if(pstClassifierRule->usValidityBitMap & (1<<PKT_CLASSIFICATION_VLANID_VALID))
	{
		if(pstEthCsPktInfo->eNwpktEthFrameType!=eEth802QVLANFrame)
				return FALSE;

		usVLANID = ntohs(*(USHORT *)(skb->data + sizeof(ETH_HEADER_STRUC))) & 0xFFF;

		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "%s  Pkt VLANID %x Priority: %d\n",__FUNCTION__,usVLANID, uPriority);

		if(usVLANID == ((pstClassifierRule->usVLANID & 0xFFF0) >> 4))
			bClassificationSucceed = TRUE;

		if(!bClassificationSucceed)
			return FALSE;
	}
	
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "ETH CS 802.1 Q VLAN ID Rule Matched\n");
		
	return TRUE;
}


BOOLEAN EThCSClassifyPkt(PMINI_ADAPTER Adapter,struct sk_buff* skb,PS_ETHCS_PKT_INFO pstEthCsPktInfo,S_CLASSIFIER_RULE *pstClassifierRule, B_UINT8 EthCSCupport)
{
	BOOLEAN bClassificationSucceed = FALSE;
	bClassificationSucceed = EthCSMatchSrcMACAddress(pstClassifierRule,((ETH_HEADER_STRUC *)(skb->data))->au8SourceAddress);
	if(!bClassificationSucceed)
		return FALSE;
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "ETH CS SrcMAC Matched\n");

	bClassificationSucceed = EthCSMatchDestMACAddress(pstClassifierRule,((ETH_HEADER_STRUC*)(skb->data))->au8DestinationAddress);
	if(!bClassificationSucceed)
		return FALSE;
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "ETH CS DestMAC Matched\n");

	//classify on ETHType/802.2SAP TLV
	bClassificationSucceed = EthCSMatchEThTypeSAP(pstClassifierRule,skb,pstEthCsPktInfo);
	if(!bClassificationSucceed)
		return FALSE;

	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "ETH CS EthType/802.2SAP Matched\n");

	//classify on 802.1VLAN Header Parameters

	bClassificationSucceed = EthCSMatchVLANRules(pstClassifierRule,skb,pstEthCsPktInfo);
	if(!bClassificationSucceed)
		return FALSE;
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "ETH CS 802.1 VLAN Rules Matched\n");
	
	return bClassificationSucceed;
}

void EThCSGetPktInfo(PMINI_ADAPTER Adapter,PVOID pvEthPayload,PS_ETHCS_PKT_INFO pstEthCsPktInfo)
{
	USHORT u16Etype = ntohs(((ETH_HEADER_STRUC*)pvEthPayload)->u16Etype);
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "EthCSGetPktInfo : Eth Hdr Type : %X\n",u16Etype);
	if(u16Etype > 0x5dc)
	{
		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "EthCSGetPktInfo : ETH2 Frame \n");
		//ETH2 Frame
		if(u16Etype == ETHERNET_FRAMETYPE_802QVLAN)
		{
			//802.1Q VLAN Header
			pstEthCsPktInfo->eNwpktEthFrameType = eEth802QVLANFrame;
			u16Etype = ((ETH_CS_802_Q_FRAME*)pvEthPayload)->EthType;
			//((ETH_CS_802_Q_FRAME*)pvEthPayload)->UserPriority 
		}
		else
		{
			pstEthCsPktInfo->eNwpktEthFrameType = eEthOtherFrame;
			u16Etype = ntohs(u16Etype);
		}
	
	}
	else
	{
		//802.2 LLC 
		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "802.2 LLC Frame \n");
		pstEthCsPktInfo->eNwpktEthFrameType = eEth802LLCFrame;
		pstEthCsPktInfo->ucDSAP = ((ETH_CS_802_LLC_FRAME*)pvEthPayload)->DSAP;
		if(pstEthCsPktInfo->ucDSAP == 0xAA && ((ETH_CS_802_LLC_FRAME*)pvEthPayload)->SSAP == 0xAA)
		{
			//SNAP Frame
			pstEthCsPktInfo->eNwpktEthFrameType = eEth802LLCSNAPFrame;
			u16Etype = ((ETH_CS_802_LLC_SNAP_FRAME*)pvEthPayload)->usEtherType;
		}
	}
	if(u16Etype == ETHERNET_FRAMETYPE_IPV4)
		pstEthCsPktInfo->eNwpktIPFrameType = eIPv4Packet;
	else if(u16Etype == ETHERNET_FRAMETYPE_IPV6)
		pstEthCsPktInfo->eNwpktIPFrameType = eIPv6Packet;
	else
		pstEthCsPktInfo->eNwpktIPFrameType = eNonIPPacket;

	pstEthCsPktInfo->usEtherType = ((ETH_HEADER_STRUC*)pvEthPayload)->u16Etype;
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "EthCsPktInfo->eNwpktIPFrameType : %x\n",pstEthCsPktInfo->eNwpktIPFrameType);
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "EthCsPktInfo->eNwpktEthFrameType : %x\n",pstEthCsPktInfo->eNwpktEthFrameType);
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "EthCsPktInfo->usEtherType : %x\n",pstEthCsPktInfo->usEtherType);
}



