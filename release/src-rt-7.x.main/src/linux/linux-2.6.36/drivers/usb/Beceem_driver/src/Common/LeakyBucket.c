/* 
* LeakyBucket.c
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
*	This file contains the routines related to Leaky Bucket Algorithm. 
***********************************************************************/
#include<headers.h>

/*********************************************************************
* Function    - UpdateTokenCount()
* 
* Description - This function calculates the token count for each 
*				channel and updates the same in Adapter strucuture.
* 
* Parameters  - Adapter: Pointer to the Adapter structure.
*
* Returns     - None
**********************************************************************/

VOID UpdateTokenCount(register PMINI_ADAPTER Adapter)
{
	ULONG 	liElapsedTime;
	INT 	i = 0;
	ULONG 	currjiffies = jiffies;	

	
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TOKEN_COUNTS, DBG_LVL_ALL, "=====>\n");
	if(NULL == Adapter)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TOKEN_COUNTS, DBG_LVL_ALL, "Adapter found NULL!\n");
		return;
	}

	for(i = 0; i < NO_OF_QUEUES; i++)
	{
		if(TRUE == Adapter->PackInfo[i].bValid && 
			(1 == Adapter->PackInfo[i].ucDirection))
		{
			liElapsedTime = currjiffies - Adapter->localjiffies;
			if(0!=liElapsedTime)
			{
				Adapter->PackInfo[i].uiCurrentTokenCount += (ULONG)
					(((Adapter->PackInfo[i].uiMaxAllowedRate) * liElapsedTime)/HZ);

				if((Adapter->PackInfo[i].uiCurrentTokenCount) >= Adapter->PackInfo[i].uiMaxBucketSize)
				{
					Adapter->PackInfo[i].uiCurrentTokenCount = Adapter->PackInfo[i].uiMaxBucketSize;
				}
			}
		}
	}
	Adapter->localjiffies = currjiffies;
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TOKEN_COUNTS, DBG_LVL_ALL, "<=====\n");
	return;

}


/*********************************************************************
* Function    - IsPacketAllowedForFlow()
* 
* Description - This function checks whether the given packet from the
*				specified queue can be allowed for transmission by 
*				checking the token count.
* 
* Parameters  - Adapter	      :	Pointer to the Adpater structure.
* 			  - iQIndex	      :	The queue Identifier.
* 			  - ulPacketLength:	Number of bytes to be transmitted.
*
* Returns     - The number of bytes allowed for transmission.
* 
***********************************************************************/
static __inline ULONG GetSFTokenCount(PMINI_ADAPTER Adapter, PacketInfo *psSF)
{
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TOKEN_COUNTS, DBG_LVL_ALL, "IsPacketAllowedForFlow ===>");
	/* Validate the parameters */
	if(NULL == Adapter || (psSF < Adapter->PackInfo && 
		(UINT)psSF > (UINT) &Adapter->PackInfo[HiPriority]))
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TOKEN_COUNTS, DBG_LVL_ALL, "IPAFF: Got wrong Parameters:Adapter: %p, QIndex: %d\n", Adapter, (psSF-Adapter->PackInfo));
		return 0;
	}

	if(FALSE != psSF->bValid && psSF->ucDirection)
	{
		if(0 != psSF->uiCurrentTokenCount)
		{
				return psSF->uiCurrentTokenCount;
		}
		else
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TOKEN_COUNTS, DBG_LVL_ALL, "Not enough tokens in queue %d Available %u\n", 
				psSF-Adapter->PackInfo, psSF->uiCurrentTokenCount);
			psSF->uiPendedLast = 1;
		}
	}
	else
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TOKEN_COUNTS, DBG_LVL_ALL, "IPAFF: Queue %d not valid\n", psSF-Adapter->PackInfo);
	}
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TOKEN_COUNTS, DBG_LVL_ALL, "IsPacketAllowedForFlow <===");
	return 0;
}

static __inline void RemovePacketFromQueue(PacketInfo *pPackInfo , struct sk_buff *Packet)
{
	struct sk_buff *psQueueCurrent=NULL, *psLastQueueNode=NULL;
	psQueueCurrent = pPackInfo->FirstTxQueue;
	while(psQueueCurrent)
	{
		if((UINT)Packet == (UINT)psQueueCurrent)
		{
			if((UINT)psQueueCurrent == (UINT)pPackInfo->FirstTxQueue)
			{
				pPackInfo->FirstTxQueue=psQueueCurrent->next;
				if((UINT)psQueueCurrent==(UINT)pPackInfo->LastTxQueue)
					pPackInfo->LastTxQueue=NULL;
			}
			else
			{
				psLastQueueNode->next=psQueueCurrent->next;
			}
			break;
		}
		psLastQueueNode = psQueueCurrent;
		psQueueCurrent=psQueueCurrent->next;
	}
}
/**
@ingroup tx_functions
This function despatches packet from the specified queue.
@return Zero(success) or Negative value(failure)
*/
static __inline INT SendPacketFromQueue(PMINI_ADAPTER Adapter,/**<Logical Adapter*/
								PacketInfo *psSF,		/**<Queue identifier*/
								struct sk_buff*  Packet)	/**<Pointer to the packet to be sent*/
{
	INT  	Status=STATUS_FAILURE;
	UINT uiIndex =0,PktLen = 0;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, SEND_QUEUE, DBG_LVL_ALL, "=====>");	
	if(!Adapter || !Packet || !psSF)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, SEND_QUEUE, DBG_LVL_ALL, "Got NULL Adapter or Packet");
		return -EINVAL;
	}

	if(psSF->liDrainCalculated==0)
	{
		psSF->liDrainCalculated = jiffies;
	}
	///send the packet to the fifo..
	PktLen = Packet->len;
	Status = SetupNextSend(Adapter, Packet, psSF->usVCID_Value);
	if(Status == 0)
	{
		for(uiIndex = 0 ; uiIndex < MIBS_MAX_HIST_ENTRIES ; uiIndex++)
		{	if((PktLen <= MIBS_PKTSIZEHIST_RANGE*(uiIndex+1)) && (PktLen > MIBS_PKTSIZEHIST_RANGE*(uiIndex)))
				Adapter->aTxPktSizeHist[uiIndex]++;
		}
	}
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, SEND_QUEUE, DBG_LVL_ALL, "<=====");	
	return Status;
}
	
/************************************************************************
* Function    - CheckAndSendPacketFromIndex()
* 
* Description - This function dequeues the data/control packet from the
*				specified queue for transmission.
* 
* Parameters  - Adapter : Pointer to the driver control structure.
* 			  - iQIndex : The queue Identifier.
*
* Returns     - None.
* 
****************************************************************************/
static __inline VOID CheckAndSendPacketFromIndex
(PMINI_ADAPTER Adapter, PacketInfo *psSF)
{
	struct sk_buff	*QueuePacket=NULL;
	char 			*pControlPacket = NULL;
	INT				Status=0;
	int				iPacketLen=0;


	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "%d ====>", (psSF-Adapter->PackInfo));
	if(((UINT)psSF != (UINT)&Adapter->PackInfo[HiPriority]) && Adapter->LinkUpStatus )//Get data packet
  	{
		if(!psSF->ucDirection )
			return;

		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "UpdateTokenCount ");	
		if(Adapter->IdleMode || Adapter->bPreparingForLowPowerMode)
		{
			//BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0,"Device is in Idle Mode..Hence blocking Data Packets..\n");
			return;
		}
		// Check for Free Descriptors
		if(atomic_read(&Adapter->CurrNumFreeTxDesc) <= MINIMUM_PENDING_DESCRIPTORS)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, " No Free Tx Descriptor(%d) is available for Data pkt..",atomic_read(&Adapter->CurrNumFreeTxDesc));
			return ;
		}
		
#if 0
		PruneQueue(Adapter,(psSF-Adapter->PackInfo));
#endif
		spin_lock_bh(&psSF->SFQueueLock);
		QueuePacket=psSF->FirstTxQueue;	
		
		if(QueuePacket)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "Dequeuing Data Packet");

			if(psSF->bEthCSSupport)
				iPacketLen = QueuePacket->len;
			else
				iPacketLen = QueuePacket->len-ETH_HLEN;
			
			iPacketLen<<=3;	
			if(iPacketLen <= GetSFTokenCount(Adapter, psSF))
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "Allowed bytes %d", 
					(iPacketLen >> 3));
				
				DEQUEUEPACKET(psSF->FirstTxQueue,psSF->LastTxQueue);
				psSF->uiCurrentBytesOnHost -= (QueuePacket->len);
				psSF->uiCurrentPacketsOnHost--;
				atomic_dec(&Adapter->TotalPacketCount);
				spin_unlock_bh(&psSF->SFQueueLock);

			   	Status = SendPacketFromQueue(Adapter, psSF, QueuePacket);
				psSF->uiPendedLast = FALSE;
			}
			else
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "For Queue: %d\n", psSF-Adapter->PackInfo);
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "\nAvailable Tokens = %d required = %d\n", 
					psSF->uiCurrentTokenCount, iPacketLen);
				//this part indicates that becuase of non-availability of the tokens 
				//pkt has not been send out hence setting the pending flag indicating the host to send it out 
				//first next iteration  .
				psSF->uiPendedLast = TRUE;
				spin_unlock_bh(&psSF->SFQueueLock);
			}
		}
		else
		{
			spin_unlock_bh(&psSF->SFQueueLock);
		}
	}
	else if((UINT)psSF == (UINT)&Adapter->PackInfo[HiPriority])
	{
		spin_lock_bh(&psSF->SFQueueLock);
		if((atomic_read(&Adapter->CurrNumFreeTxDesc) > 0 ) && 
			(atomic_read(&Adapter->index_rd_txcntrlpkt) !=
			 atomic_read(&Adapter->index_wr_txcntrlpkt))
			)
		{
			pControlPacket = Adapter->txctlpacket
			[(atomic_read(&Adapter->index_rd_txcntrlpkt)%MAX_CNTRL_PKTS)];
			if(pControlPacket) 
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "Sending Control packet");
				Status = SendControlPacket(Adapter, pControlPacket);
				if(STATUS_SUCCESS==Status)
				{

					psSF->NumOfPacketsSent++;
					psSF->uiSentBytes+=((PLEADER)pControlPacket)->PLength;
					psSF->uiSentPackets++;
					atomic_dec(&Adapter->TotalPacketCount);
					psSF->uiCurrentBytesOnHost -= ((PLEADER)pControlPacket)->PLength;
					psSF->uiCurrentPacketsOnHost--;
					atomic_inc(&Adapter->index_rd_txcntrlpkt);

				}
				else
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "SendControlPacket Failed\n");
			}
			else
			{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, " Control Pkt is not available, Indexing is wrong....");
			}
	   	}

		spin_unlock_bh(&psSF->SFQueueLock);
	}

	if(Status != STATUS_SUCCESS)	//Tx of data packet to device Failed
	{
		if(Adapter->bcm_jiffies == 0)
			Adapter->bcm_jiffies = jiffies;
	}
	else
	{
		Adapter->bcm_jiffies = 0;
	}
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "<=====");
}


/*******************************************************************
* Function    - transmit_packets()
* 
* Description - This function transmits the packets from different
*				queues, if free descriptors are available on target.
* 
* Parameters  - Adapter:  Pointer to the Adapter structure.
*
* Returns     - None.
********************************************************************/
VOID transmit_packets(PMINI_ADAPTER Adapter)
{
	UINT 	uiPrevTotalCount = 0;
	int iIndex = 0;
	
	BOOLEAN exit_flag = TRUE ;
	
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "=====>");

	if(NULL == Adapter)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX,TX_PACKETS, DBG_LVL_ALL, "Got NULL Adapter");
		return;
	}
	if(Adapter->device_removed == TRUE)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "Device removed");
		return;
	}

    BCM_DEBUG_PRINT (Adapter, DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "\nUpdateTokenCount ====>\n");

	UpdateTokenCount(Adapter);

    BCM_DEBUG_PRINT (Adapter, DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "\nPruneQueueAllSF ====>\n");

	PruneQueueAllSF(Adapter);

	uiPrevTotalCount = atomic_read(&Adapter->TotalPacketCount);

	for(iIndex=HiPriority;iIndex>=0;iIndex--)
	{
		if(	!uiPrevTotalCount || (TRUE == Adapter->device_removed))
				break;

		if(Adapter->PackInfo[iIndex].bValid && 
			Adapter->PackInfo[iIndex].uiPendedLast &&
			Adapter->PackInfo[iIndex].uiCurrentBytesOnHost && 
			((iIndex==HiPriority) || (atomic_read(&Adapter->PackInfo[iIndex].uiPerSFTxResourceCount)>0))
			)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "Calling CheckAndSendPacketFromIndex..");
			CheckAndSendPacketFromIndex(Adapter, &Adapter->PackInfo[iIndex]);
			uiPrevTotalCount--;
		}
	}

	while(uiPrevTotalCount > 0 && !Adapter->device_removed)
	{
		exit_flag = TRUE ;
			//second iteration to parse non-pending queues 
		for(iIndex=HiPriority;iIndex>=0;iIndex--)
		{
			if( !uiPrevTotalCount || (TRUE == Adapter->device_removed))
					break;
			
			if(Adapter->PackInfo[iIndex].bValid &&
				Adapter->PackInfo[iIndex].uiCurrentBytesOnHost && 
				!Adapter->PackInfo[iIndex].uiPendedLast &&
                ((iIndex==HiPriority) || (atomic_read(&Adapter->PackInfo[iIndex].uiPerSFTxResourceCount)>0))
				)
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "Calling CheckAndSendPacketFromIndex..");
				CheckAndSendPacketFromIndex(Adapter, &Adapter->PackInfo[iIndex]);
				uiPrevTotalCount--;
				exit_flag = FALSE;
			}
		}

		if(Adapter->IdleMode || Adapter->bPreparingForLowPowerMode)
		{   
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "In Idle Mode\n");
			break;
		}	
		if(exit_flag == TRUE )
		    break ;	
	}/* end of inner while loop */
	if(Adapter->bcm_jiffies == 0 && 
		atomic_read(&Adapter->TotalPacketCount) != 0 &&
	   	uiPrevTotalCount == atomic_read(&Adapter->TotalPacketCount))
	{
		Adapter->bcm_jiffies = jiffies;	
	}
	update_per_cid_rx  (Adapter);
	Adapter->txtransmit_running = 0;
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, TX_PACKETS, DBG_LVL_ALL, "<======");
}
