/* 
* HandleControlPacket.c
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

/************************************************************
*	This file contains the routines to deal with 
*      sending and receiving of control packets.
************************************************************/


#include<headers.h>

/**
When a control packet is received, analyze the  
"status" and call appropriate response function.
Enqueue the control packet for Application.			
@return None
*/
VOID handle_rx_control_packet(PMINI_ADAPTER Adapter, 	/**<Pointer to the Adapter structure*/
								struct sk_buff *skb)				/**<Pointer to the socket buffer*/
{
	PPER_TARANG_DATA	pTarang = NULL;
	BOOLEAN HighPriorityMessage = FALSE;
	struct sk_buff * newPacket = NULL;
	CHAR cntrl_msg_mask_bit = 0;
	BOOLEAN drop_pkt_flag = TRUE ;
	USHORT usStatus = *(PUSHORT)(skb->data);
	BCM_DEBUG_PRINT( Adapter,DBG_TYPE_OTHERS, CP_CTRL_PKT, DBG_LVL_ALL, "=====>");
	/* Get the Leader field */
	
	switch(usStatus) 
	{
		case CM_RESPONSES:               // 0xA0
			BCM_DEBUG_PRINT( Adapter,DBG_TYPE_OTHERS, CP_CTRL_PKT, DBG_LVL_ALL, "MAC Version Seems to be Non Multi-Classifier, rejected by Driver");
			HighPriorityMessage = TRUE ;
			break;
		case CM_CONTROL_NEWDSX_MULTICLASSIFIER_RESP:
			HighPriorityMessage = TRUE ;
			if(Adapter->LinkStatus==LINKUP_DONE)
			{
				CmControlResponseMessage(Adapter,(skb->data +sizeof(USHORT)));
			}
			break;
		case LINK_CONTROL_RESP:          //0xA2
		case STATUS_RSP:          //0xA1
			BCM_DEBUG_PRINT( Adapter,DBG_TYPE_OTHERS, CP_CTRL_PKT, DBG_LVL_ALL,"LINK_CONTROL_RESP");
			HighPriorityMessage = TRUE ;
			LinkControlResponseMessage(Adapter,(skb->data + sizeof(USHORT)));
			break;
		case STATS_POINTER_RESP:       //0xA6
			HighPriorityMessage = TRUE ;
			StatisticsResponse(Adapter, (skb->data + sizeof(USHORT)));
			break;
		case IDLE_MODE_STATUS:			//0xA3
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, CP_CTRL_PKT, DBG_LVL_ALL,"IDLE_MODE_STATUS Type Message Got from F/W");
			InterfaceIdleModeRespond(Adapter, (PUINT)(skb->data + 
						sizeof(USHORT)));
			HighPriorityMessage = TRUE ;
			break;
						   
		case AUTH_SS_HOST_MSG:
			HighPriorityMessage = TRUE ;
			break;

	 	default:
			BCM_DEBUG_PRINT( Adapter,DBG_TYPE_OTHERS, CP_CTRL_PKT, DBG_LVL_ALL,"Got Default Response");
			/* Let the Application Deal with This Packet */
			break;
	}

	//Queue The Control Packet to The Application Queues
	down(&Adapter->RxAppControlQueuelock);

	for (pTarang = Adapter->pTarangs; pTarang; pTarang = pTarang->next)
    {
       	if(Adapter->device_removed)
		{
			break;
		}
		
		drop_pkt_flag = TRUE ;
		/*
			There are cntrl msg from A0 to AC. It has been mapped to 0 to C bit in the cntrl mask.
			Also, by default AD to BF has been masked to the rest of the bits... which wil be ON by default.
			if mask bit is enable to particular pkt status, send it out to app else stop it.
		*/	
		cntrl_msg_mask_bit = (usStatus & 0x1F);
		//printk("\ninew  msg  mask bit which is disable in mask:%X", cntrl_msg_mask_bit);	
		if(pTarang->RxCntrlMsgBitMask & (1<<cntrl_msg_mask_bit))
				drop_pkt_flag = FALSE;

		if ((drop_pkt_flag == TRUE)  || (pTarang->AppCtrlQueueLen > MAX_APP_QUEUE_LEN) ||
					((pTarang->AppCtrlQueueLen > MAX_APP_QUEUE_LEN/2) && (HighPriorityMessage == FALSE)))
		{
			/*
				Assumption:- 
				1. every tarang manages it own dropped pkt statitistics 
				2. Total packet dropped per tarang will be equal to the sum of all types of dropped 
					pkt by that tarang only.
			
			*/
			switch(*(PUSHORT)skb->data)
			{
				case CM_RESPONSES:
					pTarang->stDroppedAppCntrlMsgs.cm_responses++;
					break;
				case CM_CONTROL_NEWDSX_MULTICLASSIFIER_RESP:
				 pTarang->stDroppedAppCntrlMsgs.cm_control_newdsx_multiclassifier_resp++;
					 break;
				case LINK_CONTROL_RESP: 
					pTarang->stDroppedAppCntrlMsgs.link_control_resp++;
					break;
				case STATUS_RSP:
					pTarang->stDroppedAppCntrlMsgs.status_rsp++;
					break;
				case STATS_POINTER_RESP: 
					pTarang->stDroppedAppCntrlMsgs.stats_pointer_resp++;
					break;
				case IDLE_MODE_STATUS:	
					pTarang->stDroppedAppCntrlMsgs.idle_mode_status++ ;
					break;
				case AUTH_SS_HOST_MSG:
					pTarang->stDroppedAppCntrlMsgs.auth_ss_host_msg++ ;
					break;
			default:
					pTarang->stDroppedAppCntrlMsgs.low_priority_message++ ;
					break;
			}
								
			continue;
		}
		
        newPacket = skb_clone(skb, GFP_KERNEL);
        if (!newPacket)
           break; 
        ENQUEUEPACKET(pTarang->RxAppControlHead,pTarang->RxAppControlTail,
				newPacket);
        pTarang->AppCtrlQueueLen++;
    }
	up(&Adapter->RxAppControlQueuelock);
    wake_up(&Adapter->process_read_wait_queue);
    bcm_kfree_skb(skb);
	BCM_DEBUG_PRINT( Adapter,DBG_TYPE_OTHERS, CP_CTRL_PKT, DBG_LVL_ALL, "After wake_up_interruptible");
}

/**
@ingroup ctrl_pkt_functions
Thread to handle control pkt reception
*/
int control_packet_handler  (PMINI_ADAPTER Adapter  /**< pointer to adapter object*/
						)
{
	struct sk_buff *ctrl_packet= NULL;
	unsigned long flags = 0;
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, CP_CTRL_PKT, DBG_LVL_ALL, "Entering to make thread wait on control packet event!");
	while(1)
	{
		wait_event_interruptible_timeout(Adapter->process_rx_cntrlpkt,
								 atomic_read(&Adapter->cntrlpktCnt) || 
								 Adapter->bWakeUpDevice || 
								 kthread_should_stop(),
								 msecs_to_jiffies(10)
								);

		if(kthread_should_stop())
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, CP_CTRL_PKT, DBG_LVL_ALL, "Exiting \n");
            Adapter->control_packet_handler = NULL;
			return 0;
		}
#ifndef BCM_SHM_INTERFACE
       shiftDetect(Adapter);
#endif

		if(TRUE == Adapter->bWakeUpDevice)
		{	
			Adapter->bWakeUpDevice = FALSE;
			if((FALSE == Adapter->bTriedToWakeUpFromlowPowerMode) && 
				((TRUE == Adapter->IdleMode)|| (TRUE == Adapter->bShutStatus)))
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, CP_CTRL_PKT, DBG_LVL_ALL, "Calling InterfaceAbortIdlemode\n");
	//			Adapter->bTriedToWakeUpFromlowPowerMode = TRUE;
				InterfaceIdleModeWakeup (Adapter);
			}
			continue;
		}
		
		while(atomic_read(&Adapter->cntrlpktCnt))
		{
			spin_lock_irqsave(&Adapter->control_queue_lock, flags);
			ctrl_packet = Adapter->RxControlHead;
			if(ctrl_packet)
			{
				DEQUEUEPACKET(Adapter->RxControlHead,Adapter->RxControlTail);
//				Adapter->RxControlHead=ctrl_packet->next;
				((PLINUX_DEP_DATA)Adapter->pvOsDepData)->netstats.rx_packets++;
				((PLINUX_DEP_DATA)Adapter->pvOsDepData)->netstats.rx_bytes+=
				((PLEADER)ctrl_packet->data)->PLength;
			}
			#if 0  //Idle mode debug profiling...
			if(*(PUSHORT)ctrl_packet->data == IDLE_MODE_STATUS)
			{
				puiBuffer = (PUINT)(ctrl_packet->data +sizeof(USHORT));
				if((ntohl(*puiBuffer) == GO_TO_IDLE_MODE_PAYLOAD))
				{
					memset(&tv, 0, sizeof(tv));
					do_gettimeofday(&tv);
					if((ntohl(*(puiBuffer+1)) == 0))
					{
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, CP_CTRL_PKT, DBG_LVL_ALL, "IdleMode Wake-up Msg from f/w at time :%ld ms", tv.tv_sec *1000 + tv.tv_usec /1000);
					}
					else
					{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, CP_CTRL_PKT, DBG_LVL_ALL, "IdleMode req Msg from f/w at time :%ld ms", tv.tv_sec *1000 + tv.tv_usec /1000);
					}
				}
				else if((ntohl(*puiBuffer) == IDLE_MODE_SF_UPDATE_MSG))
				{	
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, CP_CTRL_PKT, DBG_LVL_ALL, "GOT IDLE_MODE_SF_UPDATE MSG at time :%ld ms", tv.tv_sec *1000 + tv.tv_usec /1000);
				}
			}	
			#endif
	
			spin_unlock_irqrestore (&Adapter->control_queue_lock, flags);
		 	handle_rx_control_packet(Adapter, ctrl_packet);
			atomic_dec(&Adapter->cntrlpktCnt);
		}
		
	}
	return STATUS_SUCCESS;
}

INT flushAllAppQ()
{
	PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	PPER_TARANG_DATA	pTarang = NULL;
	struct sk_buff *PacketToDrop = NULL;
	for(pTarang = Adapter->pTarangs; pTarang; pTarang = pTarang->next)
	{
		while(pTarang->RxAppControlHead != NULL)
		{
			PacketToDrop=pTarang->RxAppControlHead;
			DEQUEUEPACKET(pTarang->RxAppControlHead,pTarang->RxAppControlTail);
			bcm_kfree_skb(PacketToDrop);
		}
		pTarang->AppCtrlQueueLen = 0;
		//dropped contrl packet statistics also should be reset.
		memset((PVOID)&pTarang->stDroppedAppCntrlMsgs, 0, sizeof(S_MIBS_DROPPED_APP_CNTRL_MESSAGES));

	}
	return STATUS_SUCCESS ;
}


