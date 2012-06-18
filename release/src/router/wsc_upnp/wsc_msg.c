/*
	wsc_msg.c used to handle wsc msg system.
*/
#include "wsc_msg.h"
#include "wsc_upnp.h"
#include "wsc_common.h"


/* Used for Passive Queue system */
#define WSCPASSIVEMSGQ_MAX_ENTRY	32

static uint32 gPassiveMsgID=0;
static pthread_mutex_t gPassiveMsgQLock;
static WscU2KMsgQ gPassiveMsgQ[WSCPASSIVEMSGQ_MAX_ENTRY];

/* Used for Active Queue system */
#define WSCACTIVEMSGQ_MAX_ENTRY	32

static pthread_mutex_t gActiveMsgQLock;
static WscU2KMsgQ gActiveMsgQ[WSCACTIVEMSGQ_MAX_ENTRY];

/* Used for Active Message Handler callback fucntion list */
#define WSCACTIVEMSGHANDLE_LSIT_MAX_ENTRY    3

static WscMsgHandle gActiveMsgHandleList[WSCACTIVEMSGHANDLE_LSIT_MAX_ENTRY]={
	{WSC_OPCODE_UPNP_DATA, WscEventDataMsgRecv},
	{WSC_OPCODE_UPNP_MGMT, WscEventMgmtMsgRecv},
	{WSC_OPCODE_UPNP_CTRL, WscEventCtrlMsgRecv},
};

	
int wscEnvelopeFree(IN WscEnvelope *envelope)
{
	if(envelope)
	{
		pthread_cond_destroy(&envelope->condition);
		pthread_mutex_destroy(&envelope->mutex);
		if(envelope->pMsgPtr)
			free(envelope->pMsgPtr);
		free(envelope);
	}

	return 0;
}

int wscEnvelopeRemove(
	IN WscEnvelope *envelope,
	OUT int qIdx)
{
	if(!envelope)
		return WSC_SYS_ERROR;
	
	pthread_mutex_lock(&gPassiveMsgQLock);
	wscEnvelopeFree(envelope);
	if((qIdx >= 0) && (qIdx < WSCPASSIVEMSGQ_MAX_ENTRY))
	{
		gPassiveMsgQ[qIdx].pEnvelope = NULL;
		gPassiveMsgQ[qIdx].used = 0;
	}
	pthread_mutex_unlock(&gPassiveMsgQLock);
	
	return WSC_SYS_SUCCESS;;
}


/******************************************************************************
 * wscEnvelopeCreate
 *
 * Description: 
 *       Allocate the memory space for a new envelope and init it.
 *
 * Parameters:
 *    void
 
 * Return Value:
 *    The pointer of cretaed envelope.
 *    	NULL = failure.
 *    	others = True. 
 *****************************************************************************/
WscEnvelope *wscEnvelopeCreate(void)
{
	WscEnvelope *envelope = NULL;
	
	if((envelope = malloc(sizeof(WscEnvelope))) == NULL)
	{
		DBGPRINTF(RT_DBG_ERROR, "Allocate memory to create a new envelope failed!\n");
		return NULL;
	}
	
	memset(envelope, 0 ,sizeof(WscEnvelope));
	if(pthread_mutex_init(&envelope->mutex, NULL) != 0)
	{
		DBGPRINTF(RT_DBG_ERROR, "pthread_mutex_init() failed in wscEnvelopeCreate()!\n");
		goto failed;
	}

	if((pthread_cond_init(&envelope->condition, NULL)) != 0)
	{
		DBGPRINTF(RT_DBG_ERROR, "pthread_cond_init() failed in wscEnvelopeCreate()!\n");
		pthread_mutex_destroy(&envelope->mutex);
		goto failed;
	} 

	envelope->callBack = NULL;
	envelope->timerCount = DEF_WSC_ENVELOPE_TIME_OUT;
	return envelope;
	

failed:
	free(envelope);
	
	return NULL;
}


/******************************************************************************
 * wscEnvelopeCreate
 *
 * Description: 
 *       Insert a envelope to the message Queue(gPassiveMsgQ) and assign a 
 *       eventID to this envelope.
 *
 * Parameters:
 *    WscEnvelope *envelope - The envelope want to insert.
 *    int *pQIdx            - The msg Q index of the envelope inserted in.
 *  
 * Return Value:
 *    Indicate if the envelope insetion success or failure.
 *    	WSC_SYS_SUCCESS = failure.
 *    	WSC_SYS_ERROR   = True. 
 *****************************************************************************/
int wscMsgQInsert(
	IN WscEnvelope *envelope,
	OUT int *pQIdx)
{
	int idx;

	if(!envelope)
		return WSC_SYS_ERROR;
	
	pthread_mutex_lock(&gPassiveMsgQLock);
	for(idx = 0; idx < WSCPASSIVEMSGQ_MAX_ENTRY; idx++)
	{
		if(gPassiveMsgQ[idx].used == 0)
		{
			envelope->envID = ((++gPassiveMsgID) == 0 ? (++gPassiveMsgID) : gPassiveMsgID); //ID 0 is reserved for msg directly send by kernel.
			gPassiveMsgQ[idx].pEnvelope = envelope;
			gPassiveMsgQ[idx].used = 1;
			
			break;
		}
	}
	pthread_mutex_unlock(&gPassiveMsgQLock);
	
	if(idx < WSCPASSIVEMSGQ_MAX_ENTRY)
	{
		*pQIdx = idx;
		DBGPRINTF(RT_DBG_INFO, "Insert envelope to msgQ success, msgQIdx = %d!envID=0x%x!\n", idx, envelope->envID);
		return WSC_SYS_SUCCESS;
	}
	
	return WSC_SYS_ERROR;
}


/******************************************************************************
 * wscMsgQRemove
 *
 * Description: 
 *    	Remove the envelope in the message Queue. 
 * Note: 
 *  	This function just remove the envelope from the Q but not free it. You 
 * 		should free it yourself. 
 *
 * Parameters:
 *    int qType - The Message Queue type want to handle.
 *    int qIdx  - The msg Q index want to be removed.
 *  
 * Return Value:
 *    Indicate if the envelope delete success or failure.
 *    	WSC_SYS_SUCCESS = failure.
 *    	WSC_SYS_ERROR   = True. 
 *****************************************************************************/
int wscMsgQRemove(
	IN int qType, 
	IN int qIdx)
{
	// The ActiveQ and PassiveQ have the same maximum capacity of the envelope.
	if (qIdx < 0 || qIdx >= WSCPASSIVEMSGQ_MAX_ENTRY)
		return WSC_SYS_ERROR;

	if(qType == Q_TYPE_ACTIVE)
	{	// Handle the Active Q.
		if(pthread_mutex_lock(&gActiveMsgQLock) == 0)
		{
			gActiveMsgQ[qIdx].pEnvelope = NULL;
			gActiveMsgQ[qIdx].used = 0;
			pthread_mutex_unlock(&gActiveMsgQLock);
		}
	} 
	else if(qType ==Q_TYPE_PASSIVE)
	{	// Handle the Passive Q.
		if(pthread_mutex_lock(&gPassiveMsgQLock) == 0)
		{
			gPassiveMsgQ[qIdx].pEnvelope = NULL;
			gPassiveMsgQ[qIdx].used = 0;
			pthread_mutex_unlock(&gPassiveMsgQLock);
		}
	}
	else
	{
		DBGPRINTF(RT_DBG_ERROR, "%s():Wrong MsgQ Type -- type=%d!\n", __FUNCTION__, qType);
	}
	
	return WSC_SYS_ERROR;
	
}

int registerActiveMsgTypeHandle(
	IN int MsgType, 
	IN msgHandleCallback handle)
{

	return -1;
}

int wscActiveMsgHandler(IN WscEnvelope* pEnvelope)
{
	int i;
	RTMP_WSC_MSG_HDR *pHdr = NULL;
	
	pHdr = (RTMP_WSC_MSG_HDR *)pEnvelope->pMsgPtr;

	DBGPRINTF(RT_DBG_INFO, "%s(): the ActiveMsgType=%d, SubType=%d!\n", __FUNCTION__, pHdr->msgType, pHdr->msgSubType);
	for(i=0; i< WSCACTIVEMSGHANDLE_LSIT_MAX_ENTRY; i++)
	{
		if((gActiveMsgHandleList[i].msgType == pHdr->msgType) && (gActiveMsgHandleList[i].handle != NULL))
		{
			DBGPRINTF(RT_DBG_INFO, "find ActiveMsgHandler!\n");
			return (gActiveMsgHandleList[i].handle)((char *)pEnvelope->pMsgPtr, pEnvelope->msgLen);
		}
	}

	return 0;
}


int wscMsgStateUpdate(char *pBuf, int len)
{

	/*
		TODO:  If we want to maintain our UPnP engine status. We need to add the code here. 
	*/
#if 0
	RTMP_WSC_MSG_HDR *pHdr;
	
	pHdr = (RTMP_WSC_MSG_HDR *)pBuf;

	if(pHdr == NULL)
		return 0;

	if(pHdr->msgType != WSC_OPCODE_UPNP_DATA)
		return 0;
#endif
	return 0;
	
}


/*
	NETLINK tunnel msg format send to WSCUPnP handler in user space:
	1. Signature of following string(Not include the quote, 8 bytes)
			"RAWSCMSG"
	2. eID: eventID (4 bytes)
			the ID of this message(4 bytes)
	3. aID: ackID (4 bytes)
			means that which event ID this mesage was response to.
	4. TL:  Message Total Length (4 bytes) 
			Total length of this message.
	5. F:   Flag (2 bytes)
			used to notify some specific character of this msg segment.
				Bit 1: fragment
					set as 1 if netlink layer have more segment of this Msg need to send.
				Bit 2~15: reserve, should set as 0 now.
	5. SL:  Segment Length(2 bytes)
			msg actual length in this segment, The SL may not equal the "TL" field if "F" ==1
	6. "WSC_MSG" info:

          8        4     4    4   2  2     variable length(MAXIMA=232)
	+------------+----+----+----+--+--+------------------------------+
	|  Signature |eID |aID | TL |F |SL|            WSC_MSG           |

*/
int WscRTMPMsgHandler(char *pBuf, int len)
{
	char *pos, *pBufPtr;
	int dataLen = 0, qIdx, workQIdx = -1;
	RTMP_WSC_NLMSG_HDR *pSegHdr;
	WscEnvelope *pEnvPtr = NULL;
	int qType;
	
	pSegHdr = (RTMP_WSC_NLMSG_HDR *)pBuf;

#ifdef MULTIPLE_CARD_SUPPORT
	if (memcmp(&pSegHdr->devAddr[0],  &HostMacAddr[0], MAC_ADDR_LEN) != 0)
	{
		DBGPRINTF(RT_DBG_INFO, "pSegHdr->devAddr=%02x:%02x:%02x:%02x:%02x:%02x, not the device we served(%02x:%02x:%02x:%02x:%02x:%02x)!!\n", 
								pSegHdr->devAddr[0], pSegHdr->devAddr[1], pSegHdr->devAddr[2], pSegHdr->devAddr[3], pSegHdr->devAddr[4], pSegHdr->devAddr[5],
								HostMacAddr[0], HostMacAddr[1], HostMacAddr[2], HostMacAddr[3], HostMacAddr[4], HostMacAddr[5]);
		return 0;
	}
#endif // MULTIPLE_CARD_SUPPORT //

	DBGPRINTF(RT_DBG_INFO, "pSegHdr->ackID=0x%x!\n", pSegHdr->ackID);
	if(pSegHdr->ackID == 0)
	{
		int emptyIdx = -1;
		
		/* The message should insert into the ActiveMsgQ */
		pthread_mutex_lock(&gActiveMsgQLock);
		for(qIdx = 0; qIdx < WSCACTIVEMSGQ_MAX_ENTRY; qIdx++)
		{	// Search for mailBox to check if we have a matched eventID.
			if((gActiveMsgQ[qIdx].used == 1) && ((pEnvPtr = gActiveMsgQ[qIdx].pEnvelope) != NULL))
			{
				if(pEnvPtr->envID == pSegHdr->envID)
				{
					workQIdx = qIdx;
					pEnvPtr->timerCount = DEF_WSC_ENVELOPE_TIME_OUT;
					break;
				}
			}
			else if(emptyIdx < 0 && gActiveMsgQ[qIdx].used == 0){
				emptyIdx = qIdx; //record the first un-used qIdx.
			}
		}
		
		if((qIdx == WSCACTIVEMSGQ_MAX_ENTRY) && (emptyIdx >= 0))
		{
			pEnvPtr = wscEnvelopeCreate();
			if(pEnvPtr != NULL)
			{	
				pEnvPtr->envID = pSegHdr->envID;
				gActiveMsgQ[emptyIdx].used = 1;
				gActiveMsgQ[emptyIdx].pEnvelope = pEnvPtr;
				workQIdx = emptyIdx;
			}
		} 
		pthread_mutex_unlock(&gActiveMsgQLock);
		
		if(pEnvPtr == NULL)
			return -1;
		
		qType = Q_TYPE_ACTIVE;

		//For debug
		if (pEnvPtr->pMsgPtr == NULL) {
			DBGPRINTF(RT_DBG_INFO, "Receive a new wireless event(WscActiveMsg):\n");
			DBGPRINTF(RT_DBG_INFO, "\tWscActiveMsgHdr->msgLen=%d\n", pSegHdr->msgLen);
			DBGPRINTF(RT_DBG_INFO, "\tWscActiveMsgHdr->envID=0x%x\n", pSegHdr->envID);
			DBGPRINTF(RT_DBG_INFO, "\tWscActiveMsgHdr->ackID=0x%x\n", pSegHdr->ackID);
			DBGPRINTF(RT_DBG_INFO, "\tWscActiveMsgHdr->flags=0x%x\n", pSegHdr->flags);
			DBGPRINTF(RT_DBG_INFO, "\tWscActiveMsgHdr->segLen=%d\n", pSegHdr->segLen);	
		}
	}
	else
	{
		/* The message should insert into the PassiveMsgQ */
		pthread_mutex_lock(&gPassiveMsgQLock);
		for(qIdx = 0; qIdx < WSCPASSIVEMSGQ_MAX_ENTRY; qIdx++)
		{	// Search for mailBox to check if we have a matched ackID.
			if(gPassiveMsgQ[qIdx].used == 1 && ((pEnvPtr = gPassiveMsgQ[qIdx].pEnvelope) != NULL))
			{
				if(pEnvPtr->envID == pSegHdr->ackID)
				{
					pEnvPtr->timerCount = DEF_WSC_ENVELOPE_TIME_OUT;
					break;
				}
			}
		}
		pthread_mutex_unlock(&gPassiveMsgQLock);
		
		if(qIdx == WSCPASSIVEMSGQ_MAX_ENTRY)	//Queue is full, drop it directly?
			return -1;

		qType = Q_TYPE_PASSIVE;

		//For debug
		if (pEnvPtr->pMsgPtr == NULL) {
			DBGPRINTF(RT_DBG_INFO, "Receive a new wireless event(WscPassiveMsg):\n");
			DBGPRINTF(RT_DBG_INFO, "\tWscPassiveMsgHdr->msgLen=%d\n", pSegHdr->msgLen);
			DBGPRINTF(RT_DBG_INFO, "\tWscPassiveMsgHdr->envID=0x%x\n", pSegHdr->envID);
			DBGPRINTF(RT_DBG_INFO, "\tWscPassiveMsgHdr->ackID=0x%x\n", pSegHdr->ackID);
			DBGPRINTF(RT_DBG_INFO, "\tWscPassiveMsgHdr->flags=0x%x\n", pSegHdr->flags);
			DBGPRINTF(RT_DBG_INFO, "\tWscPassiveMsgHdr->segLen=%d\n", pSegHdr->segLen);	
		}
	}					

	//Now handle the wsc_msg payload.
	pos = ((char *)pSegHdr + sizeof(RTMP_WSC_NLMSG_HDR));
	dataLen = pSegHdr->segLen;
	if (pos)
	{
		/* TODO: Need to handle it to Upnp Module */
		//wsc_hexdump("WSCENROLMSG", pos, dataLen);
		if (pEnvPtr->pMsgPtr == NULL)
		{	if ((pEnvPtr->pMsgPtr = malloc(pSegHdr->msgLen)) != NULL)
			{
				pEnvPtr->msgLen = pSegHdr->msgLen;
				pEnvPtr->actLen = 0;
			} else {
				DBGPRINTF(RT_DBG_ERROR, "%s():allocate memory for pEnvPtr->pMsgPtr failed!\n", __FUNCTION__);
				pEnvPtr->flag = WSC_ENVELOPE_MEM_FAILED;
			}
		} 

		if((pEnvPtr->flag & WSC_ENVELOPE_MEM_FAILED) != WSC_ENVELOPE_MEM_FAILED)
		{
			pBufPtr = &((char *)pEnvPtr->pMsgPtr)[pEnvPtr->actLen];
			memcpy(pBufPtr, pos, dataLen);
			pEnvPtr->actLen += dataLen;

			if (pSegHdr->flags == 0)
				pEnvPtr->flag = WSC_ENVELOPE_SUCCESS;
		}
	}

	// Trigger the event handler to process the received msgs.
	if(pEnvPtr->flag != WSC_ENVELOPE_NONE)
	{
		// Check and update the WSC status depends on the messages.
		if(pEnvPtr->flag == WSC_ENVELOPE_SUCCESS)
			wscMsgStateUpdate(pEnvPtr->pMsgPtr, pEnvPtr->msgLen);

		// Deliver the message to corresponding handler.
		if(qType == Q_TYPE_PASSIVE)
		{
			pthread_mutex_lock(&gPassiveMsgQLock);
			gPassiveMsgQ[qIdx].used = 0;
			gPassiveMsgQ[qIdx].pEnvelope = NULL;
			pthread_mutex_unlock(&gPassiveMsgQLock);
			
			//wsc_hexdump("NLMsg", pEnvPtr->pMsgPtr, pEnvPtr->msgLen);
			if(pEnvPtr->callBack != NULL)
			{
				pEnvPtr->callBack(pEnvPtr->pMsgPtr, pEnvPtr->msgLen);
				wscEnvelopeFree(pEnvPtr);
			} 
			else 
			{
				pthread_mutex_lock(&pEnvPtr->mutex);
				pthread_cond_signal(&pEnvPtr->condition);
				pthread_mutex_unlock(&pEnvPtr->mutex);
			}
		}
		else 
		{
			pthread_mutex_lock(&gActiveMsgQLock);
			gActiveMsgQ[workQIdx].pEnvelope = NULL;
			gActiveMsgQ[workQIdx].used = 0;
			pthread_mutex_unlock(&gActiveMsgQLock);
			
			//Call handler
			wscActiveMsgHandler(pEnvPtr);

			//After handle this message, remove it from ActiveMsgQ.
			wscEnvelopeFree(pEnvPtr);

		}
	}

	return 0;	
}


/********************************************************************************
 * WscMsgQVerifyTimeouts
 *
 * Description: 
 *       Checks if the envelope in  MsgQ(active and passive) was expired. 
 *		 If the envelope expires, this handle is removed from the list and trigger the 
 *			corresponding handler handle it.
 *
 * Parameters:
 *    incr -- The increment to subtract from the timeouts each time the
 *            function is called.
 *
 ********************************************************************************/
void WscMsgQVerifyTimeouts(int incr)
{
	int idx;
	WscEnvelope *envelope;

	//First check the passive queue!
	pthread_mutex_lock(&gPassiveMsgQLock);
	for(idx = 0; idx < WSCPASSIVEMSGQ_MAX_ENTRY; idx++)
	{
		if(gPassiveMsgQ[idx].used == 1 && ((envelope = gPassiveMsgQ[idx].pEnvelope) != NULL))
		{
			envelope->timerCount -= incr;
			if(envelope->timerCount <= 0)
			{
				DBGPRINTF(RT_DBG_INFO, "WscMsgQVerifyTimeouts(): PassiveQIdx(%d) timeout happened, eventID=0x%x!\n", 
						idx, envelope->envID);
				if(envelope->callBack != NULL)
					wscEnvelopeFree(envelope);
				else 
				{
					envelope->flag = WSC_ENVELOPE_TIME_OUT;
					pthread_mutex_lock(&envelope->mutex);
					pthread_cond_signal(&envelope->condition);
					pthread_mutex_unlock(&envelope->mutex);
				}
				gPassiveMsgQ[idx].used = 0;
				gPassiveMsgQ[idx].pEnvelope = NULL;
			}
		}
	}
	pthread_mutex_unlock(&gPassiveMsgQLock);


	// Check the Active Queue.
	pthread_mutex_lock(&gActiveMsgQLock);
	for(idx = 0; idx < WSCACTIVEMSGQ_MAX_ENTRY; idx++)
	{
		if((gActiveMsgQ[idx].used == 1) && ((envelope = gActiveMsgQ[idx].pEnvelope) != NULL))
		{
			envelope->timerCount -= incr;
			if(envelope->timerCount <= 0)
			{
				DBGPRINTF(RT_DBG_INFO, "WscMsgQVerifyTimeouts(): ActiveQIdx(%d) timeout happened!\n", idx);
				wscEnvelopeFree(envelope);
				gActiveMsgQ[idx].used = 0;
				gActiveMsgQ[idx].pEnvelope = NULL;
			}
		}
	}
	pthread_mutex_unlock(&gActiveMsgQLock);

}

static inline int WscMsgQRemoveAll(void)
{
	// Just set all queueed envelope as expired!
	WscMsgQVerifyTimeouts(DEF_WSC_ENVELOPE_TIME_OUT);
	return 0;
}
/********************************************************************************
 * WscUPnPCPHouseKeep
 *
 * Description: 
 *       Function that runs in its own thread and monitors advertisement
 *       and subscription timeouts for devices in the global device list.
 *
 * Parameters:
 *    None
 *
 ********************************************************************************/
void *WscMsgQHouseKeep(void *args)
{
	int incr = 2;              // how often to verify the timeouts, in seconds

	while(!stopThread)
	{
		sleep(incr);
		WscMsgQVerifyTimeouts(incr);
	}

	return NULL;
}

int wscMsgSubSystemStop()
{
	WscMsgQRemoveAll();
	pthread_mutex_destroy(&gPassiveMsgQLock);
	pthread_mutex_destroy(&gActiveMsgQLock);
	return 0;	
}


int wscMsgSubSystemInit(void)
{
	time_t nowTime;
	pthread_t msgQ_thread;
	int ret;
	/* 
		Initialize the mutex lock used to protect the U2K msg id. 
	*/

	// Initialize the Passive message Queue.
	if(pthread_mutex_init(&gPassiveMsgQLock, NULL) != 0)
	{
		goto Failed;
	} else {
		pthread_mutex_lock(&gPassiveMsgQLock);
		if ((time(&nowTime)) != ((time_t)-1))
		{
			gPassiveMsgID = (uint32)nowTime;
			memset(gPassiveMsgQ, 0, sizeof(WscU2KMsgQ)* WSCPASSIVEMSGQ_MAX_ENTRY);
			stopThread = 0;
			DBGPRINTF(RT_DBG_INFO, "gPassiveMsgQ Init success! gPassiveMsgID=0x%x!\n", gPassiveMsgID);
		}
		pthread_mutex_unlock(&gPassiveMsgQLock);
	}

	// Initialize the Active message Queue system.
	if(pthread_mutex_init(&gActiveMsgQLock, NULL) != 0) 
	{
		goto FailedActiveQ;
	} else {
		pthread_mutex_lock(&gActiveMsgQLock);
		memset(gActiveMsgQ, 0, sizeof(WscU2KMsgQ)* WSCACTIVEMSGQ_MAX_ENTRY);
		DBGPRINTF(RT_DBG_INFO, "gActiveMsgQ Init success!\n");
		pthread_mutex_unlock(&gActiveMsgQLock);
	}

	// start the MsgQ timer thread
	ret = pthread_create(&msgQ_thread, NULL, WscMsgQHouseKeep, NULL);
	if(ret == 0)
		pthread_detach(msgQ_thread);
	else
		goto FailedTimerHandle;
	
	return WSC_SYS_SUCCESS;


FailedTimerHandle:
	pthread_mutex_destroy(&gActiveMsgQLock);
FailedActiveQ:
	pthread_mutex_destroy(&gPassiveMsgQLock);
Failed:
	return WSC_SYS_ERROR; 

}

