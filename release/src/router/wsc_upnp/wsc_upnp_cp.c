/*
	wsc UPnP Contorl Point routines


*/
#include <stdio.h>
#include <string.h>
#include "sample_util.h"
#include "wsc_common.h"
#include "wsc_msg.h"
#include "wsc_upnp.h"
#include "wsc_ioctl.h"
#include "upnp.h"
#include "ixmlparser.h"
/*
   Mutex for protecting the global Wsc device list in a multi-threaded, asynchronous 
   environment. All functions should lock this mutex before reading or writing the Wsc 
   device list. 
 */
pthread_mutex_t wscDevListMutex;

static UpnpClient_Handle WscCPHandle = -1;

/* Global arrays for storing variable names and counts for Wsc services */
static char *WscVarName[WSC_STATE_VAR_COUNT] = {"WLANEvent", "APStatus", "STAStatus"};

/* Timeout to request during subscriptions */
#define DEF_SUBSCRIPTION_TIMEOUT	1801

/* The first node in the global device list, or NULL if empty */
struct upnpDeviceNode *WscDeviceList = NULL;



int WscUPnPCPDeviceHandler(
	IN Upnp_EventType EventType,
	IN void *Event,
	IN void *Cookie);

	
/********************************************************************************
 * WscUPnPCPDumpList
 *
 * Description: 
 *       Print the universal device names for each device in the Wsc device list
 *
 * Parameters:
 *   None
 *
 ********************************************************************************/
int WscUPnPCPDumpList(void)
{
	struct upnpDeviceNode *nodePtr;
	struct upnpDevice *dev;
	struct upnpService *service;
	int i = 0;

	pthread_mutex_lock( &wscDevListMutex );

	printf("WscUPnPCPDumpList:\n");
	nodePtr = WscDeviceList;
	while (nodePtr) 
	{
		dev = &nodePtr->device;
		service = &dev->services;
		
		printf("UPnPDevice No.%3d:\n", ++i);
		printf("\tDevice:\n");
		printf("\t\tFriendlyName=%s!\n", dev->FriendlyName);
		printf("\t\tDescDocURL=%s!\n", dev->DescDocURL);
		printf("\t\tUDN=%s!\n", dev->UDN);
		printf("\t\tPresURL=%s!\n", dev->PresURL);
		printf("\t\tIP Address=0x%x!\n", dev->ipAddr);
		printf("\t\tAdvrTimeOut=%d!\n", dev->AdvrTimeOut);
		printf("\tService:\n");
		printf("\t\tservIdStr=%s!\n", service->ServiceId);
		printf("\t\tservTypeStr=%s!\n", service->ServiceType);
		printf("\t\tscpdURL=%s!\n", service->SCPDURL);
		printf("\t\teventURL=%s!\n", service->EventURL);
		printf("\t\tcontrolURL=%s!\n", service->ControlURL);
		printf("\t\tsubscribeID=%s!\n", service->SID);
		
		nodePtr = nodePtr->next;
	}
    printf("\n");
	pthread_mutex_unlock(&wscDevListMutex);

	return WSC_SYS_SUCCESS;
}


/********************************************************************************
 * WscUPnPCPDeleteNode
 *
 * Description: 
 *       Delete a device node from the Wsc device list.  Note that this function is 
 *       NOT thread safe, and should be called from another function that has already 
 *       locked the Wsc device list.
 *
 * Parameters:
 *   node -- The device node
 *
 * Return:
 *    WSC_SYS_SUCCESS - if success
 *    WSC_SYS_ERROR   - if failure
 ********************************************************************************/
int WscUPnPCPDeleteNode(
	IN struct upnpDeviceNode *node)
{
	int rc, var;

	if (NULL == node) 
	{
		DBGPRINTF(RT_DBG_ERROR, "ERROR: WscUPnPCPDeleteNode: Node is empty\n");
		return WSC_SYS_ERROR;
	}

	/* If we have a valid control SID, then unsubscribe */
	if (strcmp(node->device.services.SID, "") != 0) 
	{
		rc = UpnpUnSubscribe(WscCPHandle, node->device.services.SID);
		if (rc == UPNP_E_SUCCESS) 
			DBGPRINTF(RT_DBG_INFO, "Unsubscribed from WscService EventURL with SID=%s\n", node->device.services.SID);
		else
			DBGPRINTF(RT_DBG_ERROR, "Error unsubscribing to WscService EventURL -- %d\n", rc);
	}
			
	for (var = 0; var < WSC_STATE_VAR_COUNT; var++) 
	{
		if (node->device.services.StateVarVal[var])
			free(node->device.services.StateVarVal[var]);
	}

	//Notify New Device Added
//	WscCPDevUpdate(NULL, NULL, node->device.UDN, DEVICE_REMOVED);
	free(node);
	node = NULL;

	return WSC_SYS_SUCCESS;
}


/********************************************************************************
 * WscUPnPCPRemoveDevice
 *
 * Description: 
 *       Remove a device from the wsc device list.
 *
 * Parameters:
 *   UDN -- The Unique Device Name for the device to remove
 *
 * Return:
 * 		Always success
 ********************************************************************************/
int WscUPnPCPRemoveDevice(IN char *UDN)
{
	struct upnpDeviceNode *devNodePtr, *prev;

	pthread_mutex_lock(&wscDevListMutex);

	devNodePtr = WscDeviceList;
	if (!devNodePtr)
	{
		DBGPRINTF(RT_DBG_INFO, "WARNING: WscUPnPCPRemoveDevice: Device list empty\n");
	} else {
		if (strcmp(devNodePtr->device.UDN, UDN) == 0)
		{
			WscDeviceList = devNodePtr->next;
			WscUPnPCPDeleteNode(devNodePtr);
		} else {
			prev = devNodePtr;
			devNodePtr = devNodePtr->next;

			while (devNodePtr)
			{
				if (strcmp(devNodePtr->device.UDN, UDN) == 0)
				{
					prev->next = devNodePtr->next;
					WscUPnPCPDeleteNode(devNodePtr);
					break;
				}

				prev = devNodePtr;
				devNodePtr = devNodePtr->next;
			}
		}
	}

	pthread_mutex_unlock(&wscDevListMutex);

	return WSC_SYS_SUCCESS;
}


/********************************************************************************
 * WscUPnPCPRemoveAll
 *
 * Description: 
 * 		Remove all devices from the wsc device list.
 *
 * Parameters:
 *   	void
 *
 * Return:
 * 		Always success
 ********************************************************************************/
int WscUPnPCPRemoveAll(void)
{
	struct upnpDeviceNode *devNodePtr, *next;

	pthread_mutex_lock(&wscDevListMutex);

	devNodePtr = WscDeviceList;
	WscDeviceList = NULL;

	while(devNodePtr)
	{
		next = devNodePtr->next;
		WscUPnPCPDeleteNode(devNodePtr);
		devNodePtr = next;
	}

	pthread_mutex_unlock(&wscDevListMutex);

	return WSC_SYS_SUCCESS;
}


int WscUPnPCPCheckService(
	IN IXML_Document * DescDoc)
{
	// TODO:Check if the SCPD description XML file has correct format
	return 0;
}


/********************************************************************************
 * WscUPnPCPAddDevice
 *
 * Description: 
 *       Add a new Wsc device into the wsc device list or just update its advertisement 
 *       expiration timeout if it alreay exists in the list.
 *
 * Parameters:
 *   	deviceType -- The device type want to add to the Device List
 *   	d_event -- The "Upnp_Discovery" data
 *
 * Return:
 *	 	WSC_SYS_SUCCESS - if success
 *   	other value     - if failure
 ********************************************************************************/
int WscUPnPCPAddDevice(
	IN char *deviceType,
	IN struct Upnp_Discovery *d_event,
	OUT unsigned int *outIPAddr)
{
	IXML_Document *DescDoc = NULL, *scpdDescDoc = NULL;
	char *deviceTypeStr = NULL, *friendlyName = NULL, *baseURL = NULL, *relURL = NULL, *UDN = NULL;
	char presURL[200] = {0};
	char *serviceId = NULL, *SCPDURL = NULL, *eventURL = NULL, *controlURL = NULL;
	Upnp_SID eventSID;
	struct upnpDeviceNode *newDeviceNode, *devNodePtr;
	int ret = WSC_SYS_ERROR;
	int found = 0;
	int TimeOut = DEF_SUBSCRIPTION_TIMEOUT;
	unsigned int ipAddr = 0;
	unsigned short port = 0;

	pthread_mutex_lock(&wscDevListMutex);

	if(d_event->DestAddr != NULL)
	{
		ipAddr = (unsigned int)d_event->DestAddr->sin_addr.s_addr;
		port = (unsigned short)d_event->DestAddr->sin_port;
		*outIPAddr = ipAddr;
	}
	
	printf("%s():outIPAddr=0x%x!\n", __FUNCTION__, *outIPAddr);
	if( !(strcmp(&HostDescURL[0], &d_event->Location[0])))
	{
//		DBGPRINTF(RT_DBG_INFO, "%s():Adver from LocalDev, ignore it!\n", __FUNCTION__);
		goto done;
	}
		
	// Check if this device is already in the list
	devNodePtr = WscDeviceList;
	while (devNodePtr)
	{
		if((strcmp(devNodePtr->device.UDN, d_event->DeviceId) == 0) 
			&& (strcmp(devNodePtr->device.DescDocURL, d_event->Location) == 0))
		{
			found = 1;
			break;
		}
		devNodePtr = devNodePtr->next;
	}

	if (found)
	{
		// Update the advertisement timeout field
//		DBGPRINTF(RT_DBG_INFO, "Old device, just update the expires!\n");
		devNodePtr->device.AdvrTimeOut = d_event->Expires;
		devNodePtr->device.timeCount = 0;
	} else {
		DBGPRINTF(RT_DBG_INFO, "Adver from a new device!\n");
		if((ret = UpnpDownloadXmlDoc(d_event->Location, &DescDoc)) != UPNP_E_SUCCESS){
			DBGPRINTF(RT_DBG_ERROR, "Get device description failed from %s -- err=%d\n", d_event->Location, ret);
			goto done;
		}

		/* Read key elements from description document */
		UDN = SampleUtil_GetFirstDocumentItem(DescDoc, "UDN");
		deviceTypeStr = SampleUtil_GetFirstDocumentItem(DescDoc, "deviceType");
		friendlyName = SampleUtil_GetFirstDocumentItem(DescDoc, "friendlyName");
		baseURL = SampleUtil_GetFirstDocumentItem(DescDoc, "URLBase");
		relURL = SampleUtil_GetFirstDocumentItem(DescDoc, "presentationURL");

		if((UDN == NULL) || (deviceTypeStr == NULL) || (friendlyName == NULL))
		{
			DBGPRINTF(RT_DBG_ERROR, "%s(): Get UDN failed!\n", __FUNCTION__);
			goto done;
		}
		UpnpResolveURL((baseURL ? baseURL : d_event->Location), relURL, presURL);
	
		if (SampleUtil_FindAndParseService(DescDoc, d_event->Location, WscServiceTypeStr,
											&serviceId, &SCPDURL, &eventURL,&controlURL))
		{
			if (SCPDURL != NULL)
			{	
				if ((ret = UpnpDownloadXmlDoc(SCPDURL, &scpdDescDoc)) != UPNP_E_SUCCESS)
				{
					DBGPRINTF(RT_DBG_ERROR, "Get service description failed from %s -- err=%d\n", SCPDURL, ret);
				} else {
					WscUPnPCPCheckService(scpdDescDoc);
					free(scpdDescDoc);
				}
			}

			if ((ret = UpnpSubscribe(WscCPHandle, eventURL, &TimeOut, eventSID)) != UPNP_E_SUCCESS)
			{
				DBGPRINTF(RT_DBG_ERROR, "Error Subscribing to EventURL(%s) -- %d\n", eventURL, ret);
				goto done;
			}
		
			/* Create a new device node */
			newDeviceNode = (struct upnpDeviceNode *)malloc(sizeof(struct upnpDeviceNode));
			if (newDeviceNode == NULL) {
				DBGPRINTF(RT_DBG_ERROR, "%s: create new wsc Device Node failed!\n", __FUNCTION__);
				goto done;
			}
			memset(newDeviceNode, 0, sizeof(newDeviceNode));
			newDeviceNode->device.services.StateVarVal[WSC_EVENT_WLANEVENT] = NULL;
			newDeviceNode->device.services.StateVarVal[WSC_EVENT_APSTATUS] = NULL;
			newDeviceNode->device.services.StateVarVal[WSC_EVENT_STASTATUS] = NULL;

			strncpy(newDeviceNode->device.UDN, UDN, NAME_SIZE-1);
			strncpy(newDeviceNode->device.DescDocURL, d_event->Location, NAME_SIZE-1);
			strncpy(newDeviceNode->device.FriendlyName, friendlyName, NAME_SIZE-1);
			strncpy(newDeviceNode->device.PresURL, presURL, NAME_SIZE-1);
			newDeviceNode->device.AdvrTimeOut = d_event->Expires;
			newDeviceNode->device.timeCount = 0;
			newDeviceNode->device.ipAddr = ipAddr;
			strncpy(newDeviceNode->device.services.ServiceType, WscServiceTypeStr, NAME_SIZE-1);
			if (serviceId)
				strncpy(newDeviceNode->device.services.ServiceId, serviceId, NAME_SIZE-1);
			if (SCPDURL)
				strncpy(newDeviceNode->device.services.SCPDURL, SCPDURL, NAME_SIZE-1);
			if (eventURL)
				strncpy(newDeviceNode->device.services.EventURL, eventURL, NAME_SIZE-1);
			if (controlURL)
				strncpy(newDeviceNode->device.services.ControlURL, controlURL, NAME_SIZE-1);
			if (eventSID)
				strncpy(newDeviceNode->device.services.SID, eventSID, NAME_SIZE-1);

			newDeviceNode->next = NULL;
#if 0
			for (var = 0; var < WSC_STATE_VAR_COUNT; var++)
			{
				deviceNode->device.services.serviceList.stateVarVals[var] = (char *)malloc(WSC_STATE_VAR_MAX_STR_LEN);
				strcpy(deviceNode->device.services.serviceList.stateVarVals[var], "");
			}
#endif
			// Insert the new device node in the list
			if(WscDeviceList)
			{
				newDeviceNode->next = WscDeviceList->next;
				WscDeviceList->next = newDeviceNode;
			} else {
				WscDeviceList = newDeviceNode;
			}

			ret = WSC_SYS_SUCCESS;

			// TODO:Notify New Device Added
			//WscCPDevUpdate(NULL, NULL, deviceNode->device.UDN, DEVICE_ADDED);
		} else {
			DBGPRINTF(RT_DBG_ERROR, "Error: Could not find Service: %s\n", WscServiceTypeStr);
		}
	}

done:
	pthread_mutex_unlock(&wscDevListMutex);

	if (DescDoc)
		ixmlDocument_free(DescDoc);
			
	if (UDN)
		free(UDN);
	if (deviceTypeStr)
		free(deviceTypeStr);
    if (friendlyName)
		free(friendlyName);
	if (baseURL)
		free(baseURL);
	if (relURL)
		free(relURL);

	if (serviceId)
		free(serviceId);
	if (controlURL)
		free(controlURL);
	if (eventURL)
		free(eventURL);

	return ret;
}


/********************************************************************************
 * WscStateVarUpdate
 *
 * Description: 
 *       Update a Wsc state table.  Called when an event is received.  
 *
 * Parameters:
 *   devNode -- The Wsc Device which need to update the State Variables.
 *   ChangedStateVars -- DOM document representing the XML received with the event
 *
 * Note: 
 *       This function is NOT thread save.  It must be called from another function 
 *       that has locked the global device list.
 ********************************************************************************/
void WscStateVarUpdate(
	IN struct upnpDeviceNode *devNode,
	IN IXML_Document *stateDoc)
{
	IXML_NodeList *properties, *stateVars;
	IXML_Element *propItem, *varItem;
	int propLen;
	int i, j;
	char *stateVal = NULL;


	/* Find all of the e:property tags in the document */
	properties = ixmlDocument_getElementsByTagName(stateDoc, "e:property");
	if (properties != NULL)
	{
		propLen = ixmlNodeList_length(properties);
		for (i = 0; i < propLen; i++)
		{ 	/* Loop through each property change found */
			propItem = (IXML_Element *)ixmlNodeList_item(properties, i);
			
			/*
				For each stateVar name in the state table, check if this
				is a corresponding property change
			*/
			for (j = 0; j < WSC_STATE_VAR_MAXVARS; j++)
			{
				stateVars = ixmlElement_getElementsByTagName(propItem, WscVarName[j]);
				/* If found matched item, extract the value, and update the stateVar table */
				if (stateVars)
				{
					if (ixmlNodeList_length(stateVars))
					{
						varItem = (IXML_Element *)ixmlNodeList_item(stateVars, 0);
						stateVal = SampleUtil_GetElementValue(varItem);
						if (stateVal)
						{
							if (devNode->device.services.StateVarVal[j] != NULL)
							{
								//DBGPRINTF(RT_DBG_INFO, "Free the OLD statVarVal!\n"); 
								// We didn't need do this, because the libupnp will free it.
								//free(&devNode->device.services.StateVarVal[j]);
							}
							devNode->device.services.StateVarVal[j] = stateVal;
						}
					}

					ixmlNodeList_free(stateVars);
					stateVars = NULL;
				}
			}
		}
		
		ixmlNodeList_free(properties);
		
	}
}


void WscUPnPCPHandleGetVar(
	IN char *controlURL,
	IN char *varName,
	IN DOMString varValue)
{
	struct upnpDeviceNode *tmpdevnode;

	pthread_mutex_lock(&wscDevListMutex);

	tmpdevnode = WscDeviceList;
	while (tmpdevnode)
	{
		if(strcmp(tmpdevnode->device.services.ControlURL, controlURL) == 0)
		{
//			SampleUtil_StateUpdate(varName, varValue, tmpdevnode->device.UDN, GET_VAR_COMPLETE);
			break;
		}
		tmpdevnode = tmpdevnode->next;
	}

	pthread_mutex_unlock(&wscDevListMutex);
}


/********************************************************************************
 * WscUPnPCPGetDevice
 *
 * Description: 
 *       Given a list number, returns the pointer to the device
 *       node at that position in the global device list.  Note
 *       that this function is not thread safe.  It must be called 
 *       from a function that has locked the global device list.
 *
 * Parameters:
 *   devIPAddr -- The IP address of the device.
 *   devnode -- The output device node pointer.
 *
 ********************************************************************************/
int WscUPnPCPGetDevice(
	IN uint32 devIPAddr,
	IN struct upnpDeviceNode **devnode)
{
	struct upnpDeviceNode *devPtr = NULL;

	devPtr = WscDeviceList;

	while (devPtr)
	{
		if (devPtr->device.ipAddr == devIPAddr)
			break;
		devPtr = devPtr->next;
	}

	if (!devPtr)
	{
		DBGPRINTF(RT_DBG_ERROR, "Didn't find the UPnP Device with IP Address -- 0x%x\n", devIPAddr);
		return WSC_SYS_ERROR;
    }

	*devnode = devPtr;
	
	return WSC_SYS_SUCCESS;
}


/********************************************************************************
 * WscUPnPCPSendAction
 *
 * Description: 
 *       Send an Action request to the specified service of a device.
 *
 * Parameters:
 *   devnum -- The number of the device (order in the list,
 *             starting with 1)
 *   actionname -- The name of the action.
 *   param_name -- An array of parameter names
 *   param_val -- The corresponding parameter values
 *   param_count -- The number of parameters
 *
 ********************************************************************************/
int WscUPnPCPSendAction(
	IN uint32 ipAddr,
	IN char *actionname,
	IN char **param_name,
	IN char **param_val,
	IN int param_count)
{
	struct upnpDeviceNode *devnode;
	IXML_Document *actionNode = NULL;
	int rc = WSC_SYS_SUCCESS;
	int param;

	pthread_mutex_lock(&wscDevListMutex);

    rc = WscUPnPCPGetDevice(ipAddr, &devnode);
	if (rc == WSC_SYS_SUCCESS)
	{
		if (param_count == 0) 
		{
			actionNode = UpnpMakeAction(actionname, WscServiceTypeStr, 0, NULL);
		} else {
			for (param = 0; param < param_count; param++)
			{
				rc = UpnpAddToAction(&actionNode, actionname, WscServiceTypeStr, 
										param_name[param], param_val[param]);
				if (rc != UPNP_E_SUCCESS ) 
				{
					DBGPRINTF(RT_DBG_ERROR, "ERROR: WscUPnPCPSendAction: Trying to add action param, rc=%d!\n", rc);
					//return -1; // TBD - BAD! leaves mutex locked
				}
			}
		}
		DBGPRINTF(RT_DBG_INFO, "ControlURL=%s!\n", devnode->device.services.ControlURL);
		rc = UpnpSendActionAsync( WscCPHandle, devnode->device.services.ControlURL, 
									WscServiceTypeStr, NULL, actionNode, 
									WscUPnPCPDeviceHandler, NULL);

        if (rc != UPNP_E_SUCCESS)
		{
			DBGPRINTF(RT_DBG_ERROR, "Error in UpnpSendActionAsync -- %d\n", rc);
			rc = WSC_SYS_ERROR;
		}
	}
	else 
	{
		DBGPRINTF(RT_DBG_ERROR, "WscUPnPCPGetDevice failed!\n");
	}
	
	pthread_mutex_unlock(&wscDevListMutex);

	if (actionNode)
		ixmlDocument_free(actionNode);

	return rc;
}


char *wscCPPutWLANResponseParam[]={"NewMessage", "NewWLANEventType", "NewWLANEventMAC"};
char *wscCPPutWLANResponseStr[3]={NULL, "2", "00:80:c8:34:c9:56"};
int wscCPPutWLANResponse(char *msg, int msgLen)
{
	unsigned char *encodeStr = NULL;
	RTMP_WSC_MSG_HDR *rtmpHdr = NULL;
	int len;

	rtmpHdr = (RTMP_WSC_MSG_HDR *)msg;
	DBGPRINTF(RT_DBG_INFO, "(%s):Ready to parse the K2U msg(TotalLen=%d, headerLen=%d)!\n", __FUNCTION__, msgLen, sizeof(RTMP_WSC_MSG_HDR));
	DBGPRINTF(RT_DBG_INFO, "\tMsgType=%d!\n", rtmpHdr->msgType);
	DBGPRINTF(RT_DBG_INFO, "\tMsgSubType=%d!\n", rtmpHdr->msgSubType);
	DBGPRINTF(RT_DBG_INFO, "\tipAddress=0x%x!\n", rtmpHdr->ipAddr);
	DBGPRINTF(RT_DBG_INFO, "\tMsgLen=%d!\n", rtmpHdr->msgLen);
	
	if(rtmpHdr->msgType == WSC_OPCODE_UPNP_CTRL || rtmpHdr->msgType == WSC_OPCODE_UPNP_MGMT)
	{
		DBGPRINTF(RT_DBG_INFO, "Receive a UPnP Ctrl/Mgmt Msg!\n");
		return 0;
	}
	wsc_hexdump("wscCPPutWLANResponse-K2UMsg", msg, msgLen);

	len = ILibBase64Encode((unsigned char *)(msg + sizeof(RTMP_WSC_MSG_HDR)), rtmpHdr->msgLen, &encodeStr);
	if (len >0 && encodeStr){
		wscCPPutWLANResponseStr[0] = (char *)encodeStr;
		WscUPnPCPSendAction(rtmpHdr->ipAddr, "PutWLANResponse", wscCPPutWLANResponseParam, wscCPPutWLANResponseStr, 3);
		free(encodeStr);
	}

	return 0;
}


int wscCPPutMessage(char *msg, int msgLen)
{
	unsigned char *encodeStr = NULL;
	RTMP_WSC_MSG_HDR *rtmpHdr = NULL;
	int len;
	char *wscCPStateVarParam="NewInMessage";
	
	DBGPRINTF(RT_DBG_INFO, "(%s):Ready to parse the K2U msg(TotalLen=%d, headerLen=%d)!\n", __FUNCTION__, msgLen, sizeof(RTMP_WSC_MSG_HDR));
	rtmpHdr = (RTMP_WSC_MSG_HDR *)msg;
	DBGPRINTF(RT_DBG_INFO, "\tMsgType=%d!\n", rtmpHdr->msgType);
	DBGPRINTF(RT_DBG_INFO, "\tMsgSubType=%d!\n", rtmpHdr->msgSubType);
	DBGPRINTF(RT_DBG_INFO, "\tipAddress=0x%x!\n", rtmpHdr->ipAddr);
	DBGPRINTF(RT_DBG_INFO, "\tMsgLen=%d!\n", rtmpHdr->msgLen);
	
	if(rtmpHdr->msgType == WSC_OPCODE_UPNP_CTRL || rtmpHdr->msgType == WSC_OPCODE_UPNP_MGMT)
	{
		DBGPRINTF(RT_DBG_INFO, "Receive a UPnP Ctrl/Mgmt Msg!\n");
		return 0;
	}
	wsc_hexdump("wscCPPutMessage-K2UMsg", msg, msgLen);

	len = ILibBase64Encode((unsigned char *)(msg + sizeof(RTMP_WSC_MSG_HDR)), rtmpHdr->msgLen, &encodeStr);
	if (len >0)
	{
		WscUPnPCPSendAction(rtmpHdr->ipAddr, "PutMessage", &wscCPStateVarParam, &encodeStr, 1);
		free(encodeStr);
	}
	return 0;
}



/********************************************************************************
 * WscUPnPCPSendAction
 *
 * Description: 
 *       Send an Action request to the specified service of a device.
 *
 * Parameters:
 *   devnum -- The number of the device (order in the list,
 *             starting with 1)
 *   actionname -- The name of the action.
 *   param_name -- An array of parameter names
 *   param_val -- The corresponding parameter values
 *   param_count -- The number of parameters
 *
 ********************************************************************************/
#define CP_RESP_SUPPORT_LIST_NUM	2

static char *stateVarNames[CP_RESP_SUPPORT_LIST_NUM]={"NewDeviceInfo", "NewOutMessage"};
static char *actionResNames[CP_RESP_SUPPORT_LIST_NUM]= {"u:GetDeviceInfoResponse", "u:PutMessageResponse"};

int WscUPnPCPHandleActionResponse(
	IN struct Upnp_Action_Complete *a_event)
{
	IXML_NodeList *nodeList = NULL, *msgNodeList = NULL;
	IXML_Node *element, *child = NULL;
	char *varName = NULL;
	struct upnpDeviceNode *nodePtr;
	
	char *inStr = NULL, *pWscU2KMsg = NULL;
	unsigned char *decodeStr = NULL;
	int i, decodeLen = 0, wscU2KMsgLen;
	
	unsigned int UPnPDevIP = 0;
			
	DBGPRINTF(RT_DBG_INFO, "ErrCode = %d, CtrlUrl=%s!\n", a_event->ErrCode, a_event->CtrlUrl);
	
	if(a_event->ActionResult == NULL || a_event->ErrCode != 0)
		return 0;

	// Check if this device is already in the list
	pthread_mutex_lock(&wscDevListMutex);
	nodePtr = WscDeviceList;
	while (nodePtr)
	{
		if(strcmp(nodePtr->device.services.ControlURL, a_event->CtrlUrl) == 0)
		{
			UPnPDevIP = nodePtr->device.ipAddr;
			nodePtr->device.timeCount = nodePtr->device.AdvrTimeOut;
			break;
		}
		nodePtr = nodePtr->next;
    }
	pthread_mutex_unlock(&wscDevListMutex);
	if (UPnPDevIP == 0)
		goto done;
	DBGPRINTF(RT_DBG_INFO, "Find the ActionResponse Device IP=%x\n", UPnPDevIP);

	
	/*
		We just support following ActionResponse from remote device.
	*/
	for (i=0; i < CP_RESP_SUPPORT_LIST_NUM; i++)
	{
		DBGPRINTF(RT_DBG_INFO, "check actionResNames[%d]=%s!\n", i, actionResNames[i]);
		nodeList = ixmlDocument_getElementsByTagName(a_event->ActionResult, actionResNames[i]);
		if(nodeList){
			varName = stateVarNames[i];
			break;
		}
	}

	if(nodeList == NULL)
	{
		DBGPRINTF(RT_DBG_INFO, "UnSupportted ActResponse!\n");
		goto done;
	}
	
	if ((element = ixmlNodeList_item(nodeList, 0)))
	{
		//First check if we have supportted State Variable name!
		ixmlNode_getElementsByTagName(element, varName, &msgNodeList);
		if(msgNodeList != NULL)
		{
			DBGPRINTF(RT_DBG_INFO, "find stateVarName=%s!\n", varName);
			while((child = ixmlNode_getFirstChild(element))!= NULL)
			{	// Find the Response text content!
				if (ixmlNode_getNodeType(child) == eTEXT_NODE)
				{
					inStr = strdup(ixmlNode_getNodeValue(child));
					break;
				}
				element = child;
			}
			ixmlNodeList_free(msgNodeList);
		}
	}
	
	// Here depends on the ActionRequest and ActionResponse, dispatch to correct handler!
	if(inStr!= NULL)
	{
		DBGPRINTF(RT_DBG_INFO, "Receive a %s Message!\n", actionResNames[i]);
		DBGPRINTF(RT_DBG_INFO, "\tinStr=%s!\n", inStr);
		decodeLen = ILibBase64Decode((unsigned char *)inStr, strlen(inStr), &decodeStr);
		if((decodeLen > 0) && (ioctl_sock >= 0))
		{
			RTMP_WSC_U2KMSG_HDR *msgHdr;
			WscEnvelope *msgEnvelope;
			int msgQIdx = -1;
			 
			/* Prepare the msg buffers */
			wscU2KMsgLen = wscU2KMsgCreate(&pWscU2KMsg, (char *)decodeStr, decodeLen, EAP_FRAME_TYPE_WSC);
			if (wscU2KMsgLen == 0)
				goto done;

			/* Prepare the msg envelope */
			if ((msgEnvelope = wscEnvelopeCreate()) == NULL)
				goto done;
			msgEnvelope->callBack = wscCPPutMessage;
			
			/* Lock the msgQ and check if we can get a valid mailbox to insert our request! */
			if (wscMsgQInsert(msgEnvelope, &msgQIdx) != WSC_SYS_SUCCESS)
				goto done;

			// Fill the session ID to the U2KMsg buffer header.
			msgHdr = (RTMP_WSC_U2KMSG_HDR *)pWscU2KMsg;
			msgHdr->envID = msgEnvelope->envID;

			// copy the Addr1 & Addr2
			memcpy(msgHdr->Addr1, HostMacAddr, MAC_ADDR_LEN);
			memcpy(msgHdr->Addr2, &UPnPDevIP, sizeof(unsigned int));

			// Now send the msg to kernel space.
			DBGPRINTF(RT_DBG_INFO, "(%s):Ready to send pWscU2KMsg(len=%d) to kernel by ioctl(%d)!\n", __FUNCTION__, wscU2KMsgLen, ioctl_sock);
			wsc_hexdump("U2KMsg", pWscU2KMsg, wscU2KMsgLen);
	
			if(wsc_set_oid(RT_OID_WSC_EAPMSG, pWscU2KMsg, wscU2KMsgLen) != 0)
				wscEnvelopeRemove(msgEnvelope, msgQIdx);
		}
	}


done:
	if (nodeList)
		ixmlNodeList_free(nodeList);
	if (inStr)
		free(inStr);
	if (pWscU2KMsg)
		free(pWscU2KMsg);
	if (decodeStr)
		free(decodeStr);
			
	//wsc_printEvent(UPNP_CONTROL_ACTION_COMPLETE, (void *)a_event);
	return 0;
}


/********************************************************************************
 * WscUPnPCPHandleEvent
 *
 * Description: 
 *       Handle a UPnP event that was received.  Process the event and update
 *       the appropriate service state table.
 *
 * Parameters:
 *   sid -- The subscription id for the event
 *   eventkey -- The eventkey number for the event
 *   changes -- The DOM document representing the changes
 *
 * Return:
 *   None 
 ********************************************************************************/
void WscUPnPCPHandleEvent(
	IN Upnp_SID sid,
	IN int evntkey,
	IN IXML_Document * changes)
{
	struct upnpDeviceNode *devNode;
	char *inStr = NULL, *pWscU2KMsg = NULL;
	unsigned char *decodeStr = NULL;
	int decodeLen, wscU2KMsgLen;
	unsigned int UPnPDevIP = 0;
	
	pthread_mutex_lock(&wscDevListMutex);

	devNode = WscDeviceList;
	while (devNode) 
	{
		if(strcmp(devNode->device.services.SID, sid) == 0) 
		{
			devNode->device.timeCount = devNode->device.AdvrTimeOut;
			UPnPDevIP = devNode->device.ipAddr;
			
			DBGPRINTF(RT_DBG_INFO, "Received WscService Event: %d for SID %s\n", evntkey, sid);
			WscStateVarUpdate(devNode, changes);

			inStr = devNode->device.services.StateVarVal[WSC_EVENT_WLANEVENT];
			if (inStr)
			{
#define WSC_EVENT_WLANEVENT_MSG_LEN_MIN		18	// The first byte is the msg type, the next 17 bytes is the MAC address in xx:xx format

				DBGPRINTF(RT_DBG_INFO, "\tWLANEvent=%s!\n", devNode->device.services.StateVarVal[WSC_EVENT_WLANEVENT]);
				decodeLen = ILibBase64Decode((unsigned char *)inStr, strlen(inStr), &decodeStr);

				if((decodeLen > WSC_EVENT_WLANEVENT_MSG_LEN_MIN) && (ioctl_sock >= 0))
				{
					RTMP_WSC_U2KMSG_HDR *msgHdr;
					WscEnvelope *msgEnvelope;
					int msgQIdx = -1;
			 
					
			 		decodeLen -= WSC_EVENT_WLANEVENT_MSG_LEN_MIN;
					
					/* Prepare the msg buffers */
					wscU2KMsgLen = wscU2KMsgCreate(&pWscU2KMsg, (char *)(decodeStr + WSC_EVENT_WLANEVENT_MSG_LEN_MIN), 
													decodeLen, EAP_FRAME_TYPE_WSC);
					if (wscU2KMsgLen == 0)
						goto done;

					/* Prepare the msg envelope */
					if ((msgEnvelope = wscEnvelopeCreate()) == NULL)
						goto done;
					msgEnvelope->callBack = wscCPPutMessage;
			
					/* Lock the msgQ and check if we can get a valid mailbox to insert our request! */
					if (wscMsgQInsert(msgEnvelope, &msgQIdx) != WSC_SYS_SUCCESS)
						goto done;

					// Fill the session ID to the U2KMsg buffer header.
					msgHdr = (RTMP_WSC_U2KMSG_HDR *)pWscU2KMsg;
					msgHdr->envID = msgEnvelope->envID;

					// copy the Addr1 & Addr2
					memcpy(msgHdr->Addr1, HostMacAddr, MAC_ADDR_LEN);
					memcpy(msgHdr->Addr2, &UPnPDevIP, sizeof(unsigned int));

					// Now send the msg to kernel space.
					DBGPRINTF(RT_DBG_INFO, "(%s):Ready to send pWscU2KMsg(len=%d) to kernel by ioctl(%d)!\n", __FUNCTION__, wscU2KMsgLen, ioctl_sock);
					wsc_hexdump("U2KMsg", pWscU2KMsg, wscU2KMsgLen);
	
					if(wsc_set_oid(RT_OID_WSC_EAPMSG, pWscU2KMsg, wscU2KMsgLen) != 0)
						wscEnvelopeRemove(msgEnvelope, msgQIdx);
				}
			}
			if (devNode->device.services.StateVarVal[WSC_EVENT_APSTATUS] != NULL)
				DBGPRINTF(RT_DBG_INFO, "\tAPStatus=%s!\n", devNode->device.services.StateVarVal[WSC_EVENT_APSTATUS]);
			if (devNode->device.services.StateVarVal[WSC_EVENT_STASTATUS] != NULL)
				DBGPRINTF(RT_DBG_INFO, "\tSTAStatus=%s!\n", devNode->device.services.StateVarVal[WSC_EVENT_STASTATUS]);
			break;
		}
		devNode = devNode->next;
	}
	
done:
	pthread_mutex_unlock(&wscDevListMutex);
}


/********************************************************************************
 * WscUPnPCPHandleSubscribeUpdate
 *
 * Description: 
 *       Handle a UPnP subscription update that was received.  Find the 
 *       service the update belongs to, and update its subscription
 *       timeout.
 *
 * Parameters:
 *   eventURL -- The event URL for the subscription
 *   sid -- The subscription id for the subscription
 *   timeout  -- The new timeout for the subscription
 *
 * Return:
 *    None
 ********************************************************************************/
void WscUPnPCPHandleSubscribeUpdate(
	IN char *eventURL,
	IN Upnp_SID sid,
	IN int timeout)
{
	struct upnpDeviceNode *tmpdevnode;

	pthread_mutex_lock(&wscDevListMutex);

	tmpdevnode = WscDeviceList;
	while (tmpdevnode) 
	{
		if( strcmp(tmpdevnode->device.services.EventURL, eventURL) == 0) 
		{
			DBGPRINTF(RT_DBG_INFO, "Received WscService Event Renewal for eventURL %s\n", eventURL);
			strcpy(tmpdevnode->device.services.SID, sid);
			break;
		}

		tmpdevnode = tmpdevnode->next;
	}

	pthread_mutex_unlock(&wscDevListMutex);
}



/********************************************************************************
 * WscUPnPCPDeviceHandler
 *
 * Description: 
 *       The callback handler registered with the SDK while registering the Control Point.
 *       This callback funtion detects the type of callback, and passes the request on to 
 *       the appropriate function.
 *
 * Parameters:
 *   EventType -- The type of callback event
 *   Event -- Data structure containing event data
 *   Cookie -- Optional data specified during callback registration
 *
 * Return:
 *    Always zero 
 ********************************************************************************/
int
WscUPnPCPDeviceHandler(
	IN Upnp_EventType EventType,
	IN void *Event,
	IN void *Cookie)
{
    //wsc_PrintEvent( EventType, Event );

	switch (EventType) 
	{
		/*
			SSDP Stuff 
		*/
		case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		case UPNP_DISCOVERY_SEARCH_RESULT:
			{
				struct Upnp_Discovery *d_event = (struct Upnp_Discovery *)Event;
				unsigned int ipAddr = 0;
				if (d_event->ErrCode != UPNP_E_SUCCESS)
				{
					DBGPRINTF(RT_DBG_ERROR, "Error in Discovery Adv Callback -- %d\n", d_event->ErrCode);
				}
				else 
				{
					if (strcmp(d_event->DeviceType, WscDeviceTypeStr) == 0)
					{	//We just need to take care about the WscDeviceTypeStr
						DBGPRINTF(RT_DBG_INFO, "Receive a Advertisement from a WFADevice(URL=%s)\n", d_event->Location);
						if (WscUPnPCPAddDevice(WscDeviceTypeStr, d_event, &ipAddr) == WSC_SYS_SUCCESS)
						{
							WscUPnPCPDumpList(); // Printing the Wsc Device List for debug
							WscUPnPCPSendAction(ipAddr,"GetDeviceInfo", NULL, NULL, 0);
						}
					}
				}
				break;
			}

		case UPNP_DISCOVERY_SEARCH_TIMEOUT:
			// Nothing to do here..
			break;

		case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
			{
				struct Upnp_Discovery *d_event = (struct Upnp_Discovery *)Event;

				if (d_event->ErrCode != UPNP_E_SUCCESS)
				{
					DBGPRINTF(RT_DBG_ERROR, "Error in Discovery ByeBye Callback -- %d\n", d_event->ErrCode);
				}
				else
				{
					DBGPRINTF(RT_DBG_INFO, "Received ByeBye for Device: %s\n", d_event->DeviceId);
	                WscUPnPCPRemoveDevice(d_event->DeviceId);
					//Dump it for debug
					WscUPnPCPDumpList();
				}

				break;
			}

		/*
			SOAP Stuff 
		*/
		case UPNP_CONTROL_ACTION_COMPLETE:
			{
				struct Upnp_Action_Complete *a_event = (struct Upnp_Action_Complete *)Event;

				if (a_event->ErrCode != UPNP_E_SUCCESS)
					DBGPRINTF(RT_DBG_ERROR, "Error in  Action Complete Callback -- %d\n", a_event->ErrCode);
				else 
				{
					DBGPRINTF(RT_DBG_INFO, "Get event:UPNP_CONTROL_ACTION_COMPLETE!\n");
					WscUPnPCPHandleActionResponse(a_event);
				}
				break;
			}

		case UPNP_CONTROL_GET_VAR_COMPLETE:
			{
				struct Upnp_State_Var_Complete *sv_event = (struct Upnp_State_Var_Complete *)Event;

				if (sv_event->ErrCode != UPNP_E_SUCCESS)
				{
					DBGPRINTF(RT_DBG_ERROR, "Error in Get Var Complete Callback -- %d\n", sv_event->ErrCode);
                } else {
					WscUPnPCPHandleGetVar(sv_event->CtrlUrl, sv_event->StateVarName, sv_event->CurrentVal);
				}

				break;
			}

		/*
			GENA Stuff 
		*/
		case UPNP_EVENT_RECEIVED:
			{
				struct Upnp_Event *e_event = (struct Upnp_Event *)Event;
				
				DBGPRINTF(RT_DBG_INFO, "Get event:UPNP_EVENT_RECEIVED!\n");
				
				WscUPnPCPHandleEvent(e_event->Sid, e_event->EventKey, e_event->ChangedVariables);
				break;
			}

		case UPNP_EVENT_SUBSCRIBE_COMPLETE:
		case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
		case UPNP_EVENT_RENEWAL_COMPLETE:
			{
				struct Upnp_Event_Subscribe *es_event = (struct Upnp_Event_Subscribe *)Event;

				if (es_event->ErrCode != UPNP_E_SUCCESS) 
				{
					DBGPRINTF(RT_DBG_ERROR, "Error in Event Subscribe Callback -- %d\n", es_event->ErrCode);
				} else {
					WscUPnPCPHandleSubscribeUpdate(es_event->PublisherUrl, es_event->Sid, es_event->TimeOut);
                }

				break;
			}

		case UPNP_EVENT_AUTORENEWAL_FAILED:
		case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
			{
				int TimeOut = DEF_SUBSCRIPTION_TIMEOUT;
				Upnp_SID newSID;
				int ret;

				struct Upnp_Event_Subscribe *es_event = (struct Upnp_Event_Subscribe *)Event;

				ret = UpnpSubscribe(WscCPHandle, es_event->PublisherUrl, &TimeOut, newSID);

				if (ret == UPNP_E_SUCCESS) 
				{
					DBGPRINTF(RT_DBG_INFO, "Subscribed to EventURL with SID=%s\n", newSID);
					WscUPnPCPHandleSubscribeUpdate(es_event->PublisherUrl, newSID, TimeOut);
				} else {
					DBGPRINTF(RT_DBG_ERROR, "Error Subscribing to EventURL -- %d\n", ret);
				}
				break;
			}

			/*
				Ignore these cases, since this is not a device 
			*/
		case UPNP_EVENT_SUBSCRIPTION_REQUEST:
		case UPNP_CONTROL_GET_VAR_REQUEST:
		case UPNP_CONTROL_ACTION_REQUEST:
			break;
	}

	return 0;
}


/********************************************************************************
 * WscUPnPCPGetVar
 *
 * Description: 
 *       Send a GetVar request to the specified service of a device.
 *
 * Parameters:
 *   ipAddr -- The IP address of the device.
 *   varname -- The name of the variable to request.
 *
 * Return:
 *    UPNP_E_SUCCESS - if success
 *    WSC_SYS_ERROR  - if failure
 ********************************************************************************/
int
WscUPnPCPGetVar(
	IN uint32 ipAddr,
	IN char *varname)
{
	struct upnpDeviceNode *devnode;
	int rc;

	pthread_mutex_lock(&wscDevListMutex);

	rc = WscUPnPCPGetDevice(ipAddr, &devnode);

	if (rc == WSC_SYS_SUCCESS)
	{
		rc = UpnpGetServiceVarStatusAsync( WscCPHandle, devnode->device.services.ControlURL,
                                           varname, WscUPnPCPDeviceHandler, NULL);
		if( rc != UPNP_E_SUCCESS )
		{
			DBGPRINTF(RT_DBG_ERROR, "Error in UpnpGetServiceVarStatusAsync -- %d\n", rc);
			rc = WSC_SYS_ERROR;
		}
	}

	pthread_mutex_unlock(&wscDevListMutex);

	return rc;
}


#if 0
int WscUPnPCPGetVarSampleCode(
	IN int devnum)
{
    return WscUPnPCPGetVar(devnum, "Power");
}
#endif


/********************************************************************************
 * WscUPnPCPVerifyTimeouts
 *
 * Description: 
 *       Checks the advertisement each device in the global device list. 
 *		 If an advertisement expires, the device is removed from the list.
 *		 If an advertisement is about to expire, a search request is sent 
 *		 for that device.  
 *
 * Parameters:
 *    incr -- The increment to subtract from the timeouts each time the
 *            function is called.
 *
 * Return:
 *    None
 ********************************************************************************/
void WscUPnPCPVerifyTimeouts(int incr)
{
	struct upnpDeviceNode *prevdevnode, *curdevnode;
	int ret, timeLeft;

	pthread_mutex_lock(&wscDevListMutex);

	prevdevnode = NULL;
	curdevnode = WscDeviceList;

	while (curdevnode)
	{
		curdevnode->device.timeCount += incr;
		timeLeft = curdevnode->device.AdvrTimeOut - curdevnode->device.timeCount;
		
		if (timeLeft <= 0)
		{
			/* This advertisement has expired, so we should remove the device from the list */
			if (WscDeviceList == curdevnode)
				WscDeviceList = curdevnode->next;
			else
				prevdevnode->next = curdevnode->next;
			DBGPRINTF(RT_DBG_INFO, "%s(), delete the node(ipAddr=0x%08x)!\n", __FUNCTION__, curdevnode->device.ipAddr);
			WscUPnPCPDeleteNode(curdevnode);
			curdevnode = prevdevnode ? prevdevnode->next : WscDeviceList;
		} else {
			if (timeLeft < 3 * incr)
			{
				/*
					This advertisement is about to expire, so send out a search request for this 
					device UDN to try to renew
				*/
				ret = UpnpSearchAsync(WscCPHandle, incr, curdevnode->device.UDN, NULL);
				if (ret != UPNP_E_SUCCESS)
					DBGPRINTF(RT_DBG_ERROR, "Err sending SearchReq for Device UDN: %s -- err = %d\n", curdevnode->device.UDN, ret);
			}

			prevdevnode = curdevnode;
			curdevnode = curdevnode->next;
		}

	}
	pthread_mutex_unlock(&wscDevListMutex);

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
 * Return:
 *    UPNP_E_SUCCESS - if success
 *    WSC_SYS_ERROR  - if failure
 ********************************************************************************/
void *WscUPnPCPHouseKeep(void *args)
{
	int incr = 30;              // how often to verify the timeouts, in seconds

	while(1)
	{
		sleep(incr);
		WscUPnPCPVerifyTimeouts(incr);
	}
}


/********************************************************************************
 * WscUPnPCPRefresh
 *
 * Description: 
 *       Clear the current wsc device list and issue new search
 *	 requests to build it up again from scratch.
 *
 * Parameters:
 *   None
 *
 * Return:
 *    Always success
 ********************************************************************************/
int WscUPnPCPRefresh(void)
{
	int rc;

	WscUPnPCPRemoveAll();

	/*
		Search for all devices of type Wsc UPnP Device version 1, 
		waiting for up to 5 seconds for the response 
	*/
	rc = UpnpSearchAsync(WscCPHandle, 5, WscDeviceTypeStr, NULL);
	if (rc != UPNP_E_SUCCESS)
	{
		DBGPRINTF(RT_DBG_ERROR, "Error sending search request%d\n", rc);
		return WSC_SYS_ERROR;
	}

	return WSC_SYS_SUCCESS;
}


/******************************************************************************
 * WscUPnPCPStop
 *
 * Description: 
 *      Stop the UPnP Control Point, and remove all remote Device node.
 *
 * Parameters:
 *		void
 *   
 * Return:
 *    Always success
 *****************************************************************************/
int WscUPnPCPStop(void)
{
	WscUPnPCPRemoveAll();
    UpnpUnRegisterClient(WscCPHandle);

	pthread_mutex_destroy(&wscDevListMutex);
    return WSC_SYS_SUCCESS;
}


/******************************************************************************
 * WscUPnPCPStart
 *
 * Description: 
 *      Registers the UPnP Control Point, and sends out M-SEARCH.
 *
 * Parameters:
 *
 *   char *ipAddr 		 - ip address to initialize the Control Point Service.
 *   unsigned short port - port number to initialize the control Point Service.
 *   
 * Return:
 *    success - WSC_SYS_SUCCESS
 *    failed  - WSC_SYS_ERROR
 *****************************************************************************/
int WscUPnPCPStart(
	IN char *ipAddr,
	IN unsigned short port)
{
	pthread_t timer_thread;
	int rc;

	pthread_mutex_init(&wscDevListMutex, NULL);

	DBGPRINTF(RT_DBG_INFO, "Registering Control Point...\n");
	rc = UpnpRegisterClient(WscUPnPCPDeviceHandler, &WscCPHandle, &WscCPHandle);
	if (rc != UPNP_E_SUCCESS)
	{
		DBGPRINTF(RT_DBG_ERROR, "Error registering CP: %d\n", rc);
		pthread_mutex_destroy(&wscDevListMutex);
		
		return WSC_SYS_ERROR;
	}
	DBGPRINTF(RT_DBG_INFO, "Control Point Registered\n");

	WscUPnPCPRefresh();

	// start a timer thread
	rc = pthread_create(&timer_thread, NULL, WscUPnPCPHouseKeep, NULL);
	if(rc == 0)
		pthread_detach(timer_thread);
	
	return WSC_SYS_SUCCESS;

}
