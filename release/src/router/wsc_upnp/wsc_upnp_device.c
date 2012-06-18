///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2003 Ralink Corporation 
// All rights reserved. 
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met: 
//
// * Redistributions of source code must retain the above copyright notice, 
// this list of conditions and the following disclaimer. 
// * Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution. 
// * Neither name of Intel Corporation nor the names of its contributors 
// may be used to endorse or promote products derived from this software 
// without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include "wsc_msg.h"
#include "wsc_common.h"
#include "wsc_ioctl.h"
#include "wsc_upnp.h"
#include "sample_util.h"
#include "ThreadPool.h"
#include <service_table.h>
#include <upnpapi.h>
#include "ssdplib.h"

#define UUID_STR_LEN            36	
#define WSC_UPNP_UUID_STR_LEN	(5 + UUID_STR_LEN + 1)	// The UUID string get from the driver by ioctl and the strlen is 36 plus 1 '\0', 
						        // and we need extra 5 bytes for prefix string "uuid:"

//extern char HostDescURL[WSC_UPNP_DESC_URL_LEN];
//extern unsigned char HostMacAddr[MAC_ADDR_LEN];
//extern unsigned int HostIPAddr;
//extern int WscUPnPOpMode;

typedef struct _UPnPDevDescBuf{
	char *pBuf;
	int bufSize;
}UPnPDevDescBuf;

UPnPDevDescBuf WFADeviceDescBuf = {
	.pBuf = NULL, 
	.bufSize = 0
};


/*
   Mutex for protected access in a multi-threaded, asynchronous environment. 
*/
pthread_mutex_t WscDevMutex;		// mutex used when access the state table data of "wscLocalDevice".
pthread_mutex_t wscCPListMutex;		// Mutex used when access the Control-Point List "".
struct upnpCPNode *WscCPList = NULL;

static struct upnpDevice wscLocalDevice;	// The data structure used the save the local Wsc UPnP Device.
static UpnpDevice_Handle wscDevHandle = -1;	// Device handle of "wscLocalDevice" supplied by UPnP SDK.

/* The amount of time (in seconds) before advertisements will expire */
int defAdvrExpires = 100;


/* 
	Structure for storing Wsc Device Service identifiers and state table 
*/
#define WSC_ACTION_COUNTS		13
#define WSC_ACTION_MAXCOUNT		WSC_ACTION_COUNTS

typedef int (*upnp_action) (IXML_Document *request, uint32 ipAddr, IXML_Document **out, char **errorString);

struct WscDevActionList {
  char *actionNames[WSC_ACTION_COUNTS];
  upnp_action actionFuncs[WSC_ACTION_COUNTS];
};

struct WscDevActionList wscDevActTable;


static char *wscStateVarName[] = {"WLANEvent", "APStatus", "STAStatus"};
static char wscStateVarCont[WSC_STATE_VAR_COUNT][WSC_STATE_VAR_MAX_STR_LEN];

static char *wscStateVarDefVal[] = {"", "0", "0"};

char wscAckMsg[]= {0x10, 0x4a, 0x00, 0x01, 0x10, 0x10, 0x22, 0x00, 0x01, 0x0d, 0x10, 0x1a, 0x00, 0x10, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x39, 
				   0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
				   0x00, 0x00};


typedef enum _WSC_EVENT_STATUS_CODE{
	WSC_EVENT_STATUS_CONFIG_CHANGED = 0x00000001,
	WSC_EVENT_STATUS_AUTH_THRESHOLD_REACHED = 0x00000010,
}WSC_EVENT_STATUS_CODE;

typedef enum _WSC_EVENT_WLANEVENTTYPE_CODE{
	WSC_EVENT_WLANEVENTTYPE_PROBE = 1,
	WSC_EVENT_WLANEVENTTYPE_EAP = 2,
}WSC_EVENT_WLANEVENTTYPE_CODE;


int senderID = 1;
char CfgMacStr[18]={0};


/******************************************************************************
 * CreateDeviceDescXML
 *
 * Description: 
 *       Called before the UpnpRegisterRootDevice2(). We read the device xml
 *       template file and insert the device specific UUID to create our new
 *       device xml description file for lateer to send advertisement.
 *
 * Parameters:
 *   char *tmpelateFile - template file name including the absolute path
 *   char *uuidStr      - UUID string wanna used to create new UPnP device description file.
 *
 * Return:
 *    TRUE  - Success
 *    FALSE - Failure
 *****************************************************************************/
static int 
CreateDeviceDescXML(
	IN char *tmpelateFile,
	IN char *uuidStr)
{
	int descSize, tmpLen = 256, i;
	char *pPos, *pTarget, lineStr[256]={0};
	struct stat fstat;
	FILE *fp;
	
	memset(&fstat, 0 , sizeof(fstat));
	if (stat(tmpelateFile, &fstat) < 0)
	{
		fprintf(stderr, "stat the description template(%s) failed!(errmsg=%s)\n", tmpelateFile, strerror(errno));
		return FALSE;
	}

	descSize = fstat.st_size;
	descSize += WSC_UPNP_UUID_STR_LEN;	// 42 bytes used to reserve the memory space for "uuid:36_BYTES_UUID_STRING"
	if ((fp = fopen(tmpelateFile, "r")) == NULL)
	{
		fprintf(stderr, "fopen the description template failed!(errmsg=%s)\n", strerror(errno));
		return FALSE;
	}

	if ((WFADeviceDescBuf.pBuf = (char *)malloc(descSize)) == NULL) 
	{
		fprintf(stderr, "alloc memory for description template buffer failed(errmsg=%s)!\n", strerror(errno));
		fclose(fp);
		return FALSE;
	}
	DBGPRINTF(RT_DBG_INFO, "alloc memory size=%d!\n", descSize);
	
	memset(WFADeviceDescBuf.pBuf, 0, descSize);
	WFADeviceDescBuf.bufSize = descSize;

	pPos = WFADeviceDescBuf.pBuf;
	while (!feof(fp))
	{
		if (fgets(lineStr, tmpLen, fp))
		{
			if ((pTarget = strstr(lineStr, "<UDN>")) !=  NULL)
			{
				memset(lineStr, 0, 256);
				strcat(lineStr, "<UDN>");
				strcat(lineStr, uuidStr);
				strcat(lineStr, "</UDN>\n");
			}
			strcpy(pPos, lineStr);
			pPos += strlen(lineStr);
		}
	}
	fclose(fp);
	
#if 1
	//Dump the all description files!
	for (i=0; i<descSize; i++ )
	{
		DBGPRINTF(RT_DBG_INFO, "%c", WFADeviceDescBuf.pBuf[i]);
	}
	DBGPRINTF(RT_DBG_INFO, "\n");
#endif

	return TRUE;

}


/******************************************************************************
 * WscDevGetAPSettings
 *
 * Description: 
 *       Wsc Service action callback used to get the AP's settings.
 *
 * Parameters:
 *    
 *    IXML_Document * in -  action request document
 *    IXML_Document **out - action result document
 *    char **errorString - errorString (in case action was unsuccessful)
 *  
 * Return:
 *    UPNP_E_SUCCESS 		- if success
 *    others 				- if failure
 *****************************************************************************/
static int
WscDevGetAPSettings(
	IN IXML_Document * in,
	IN uint32 ipAddr,
    OUT IXML_Document ** out,
    OUT char **errorString )
{
	// TODO: Need to complete it, currently do nothing and return directly.
	
	//create a response	
	if(UpnpAddToActionResponse(out, "GetAPSettings", WscServiceTypeStr, NULL, NULL) != UPNP_E_SUCCESS)
	{
		(*out) = NULL;
		(*errorString) = "Internal Error";
		
		return UPNP_E_INTERNAL_ERROR;
	}
	
	return UPNP_E_SUCCESS;

}


/******************************************************************************
 * WscDevGetSTASettings
 *
 * Description: 
 *      Wsc Service action callback used to get the STA's settings.
 *
 * Parameters:
 *   
 *    IXML_Document * in -  action request document
 *    IXML_Document **out - action result document
 *    char **errorString - errorString (in case action was unsuccessful)
 *  
 * Return:
 *    UPNP_E_SUCCESS 		- if success
 *    others 				- if failure
 *****************************************************************************/
static int
WscDevGetSTASettings(
	IN IXML_Document * in,
	IN uint32 ipAddr,
	OUT IXML_Document ** out,
	OUT char **errorString)
{
	// TODO: Need to complete it, currently do nothing and return directly.
	
	//create a response	
	if(UpnpAddToActionResponse(out, "GetSTASettings", WscServiceTypeStr, NULL, NULL) != UPNP_E_SUCCESS)
	{
		(*out) = NULL;
		(*errorString) = "Internal Error";
		
		return UPNP_E_INTERNAL_ERROR;
	}
	
	return UPNP_E_SUCCESS;

}

/******************************************************************************
 * WscDevSetAPSettings
 *
 * Description: 
 *       Wsc Service action callback used to set the AP's settings.
 *
 * Parameters:
 *   
 *    IXML_Document * in -  action request document
 *    IXML_Document **out - action result document
 *    char **errorString - errorString (in case action was unsuccessful)
 *  
 * Return:
 *    UPNP_E_SUCCESS 		- if success
 *    others 				- if failure
 *****************************************************************************/
static int
WscDevSetAPSettings(
	IN IXML_Document * in,
	IN uint32 ipAddr,
	OUT IXML_Document ** out,
	OUT char **errorString)
{
	// TODO: Need to complete it, currently do nothing and return directly.
	
	//create a response	
	if(UpnpAddToActionResponse(out, "SetAPSettings", WscServiceTypeStr, NULL, NULL) != UPNP_E_SUCCESS)
	{
		(*out) = NULL;
		(*errorString) = "Internal Error";
		
		return UPNP_E_INTERNAL_ERROR;
	}
	
	return UPNP_E_SUCCESS;

}


/******************************************************************************
 * WscDevDelAPSettings
 *
 * Description: 
 *       Wsc Service action callback used to delete the AP's settings.
 *
 * Parameters:
 *   
 *    IXML_Document * in -  action request document
 *    IXML_Document **out - action result document
 *    char **errorString - errorString (in case action was unsuccessful)
 *  
 * Return:
 *    UPNP_E_SUCCESS 		- if success
 *    others 				- if failure
 *****************************************************************************/
static int
WscDevDelAPSettings(
	IN IXML_Document * in,
	IN uint32 ipAddr,
	OUT IXML_Document ** out,
	OUT char **errorString)
{
	// TODO: Need to complete it, currently do nothing and return directly.
	
	//create a response	
	if(UpnpAddToActionResponse(out, "DelAPSettings", WscServiceTypeStr, NULL, NULL) != UPNP_E_SUCCESS)
	{
		(*out) = NULL;
		(*errorString) = "Internal Error";
		
		return UPNP_E_INTERNAL_ERROR;
	}
	
	return UPNP_E_SUCCESS;

}


/******************************************************************************
 * WscDevSetSTASettings
 *
 * Description: 
 *       Wsc Service action callback used to set the STA's settings.
 *
 * Parameters:
 *  
 *    IXML_Document * in -  action request document
 *    IXML_Document **out - action result document
 *    char **errorString - errorString (in case action was unsuccessful)
 *  
 * Return:
 *    UPNP_E_SUCCESS 		- if success
 *    others 				- if failure
 *****************************************************************************/
static int
WscDevSetSTASettings(
	IN IXML_Document * in,
	IN uint32 ipAddr,
	OUT IXML_Document ** out,
	OUT char **errorString)
{

	// TODO: Need to complete it, currently do nothing and return directly.
	
	//create a response	
	if(UpnpAddToActionResponse(out, "SetSTASettings", WscServiceTypeStr, NULL, NULL) != UPNP_E_SUCCESS)
	{
		(*out) = NULL;
		(*errorString) = "Internal Error";
		
		return UPNP_E_INTERNAL_ERROR;
	}
	
	return UPNP_E_SUCCESS;

}


/******************************************************************************
 * WscDevRebootAP
 *
 * Description: 
 *       Wsc Service action callback used to reboot the AP.
 *
 * Parameters:
 *   
 *    IXML_Document * in -  action request document
 *    IXML_Document **out - action result document
 *    char **errorString - errorString (in case action was unsuccessful)
 *  
 * Return:
 *    UPNP_E_SUCCESS 		- if success
 *    others 				- if failure
 *****************************************************************************/
static int
WscDevRebootAP(
	IN IXML_Document * in,
	IN uint32 ipAddr,
	OUT IXML_Document ** out,
	OUT char **errorString)
{
	// TODO: Need to complete it, currently do nothing and return directly.
	
	//create a response	
	if(UpnpAddToActionResponse(out, "RebootAP", WscServiceTypeStr, NULL, NULL) != UPNP_E_SUCCESS)
	{
		(*out) = NULL;
		(*errorString) = "Internal Error";
		
		return UPNP_E_INTERNAL_ERROR;
	}
	
	return UPNP_E_SUCCESS;

}


/******************************************************************************
 * WscDevResetAP
 *
 * Description: 
 *       Wsc Service action callback used to reset the AP device to factory default config.
 *
 * Parameters:
 *   
 *    IXML_Document * in -  action request document
 *    IXML_Document **out - action result document
 *    char **errorString - errorString (in case action was unsuccessful)
 *  
 * Return:
 *    UPNP_E_SUCCESS 		- if success
 *    others 				- if failure
 *****************************************************************************/
static int 
WscDevResetAP(
	IN IXML_Document * in,
	IN uint32 ipAddr,
	OUT IXML_Document ** out,
	OUT char **errorString)
{
	// TODO: Need to complete it, currently do nothing and return directly.
	
	//create a response	
	if(UpnpAddToActionResponse(out, "ResetAP", WscServiceTypeStr, NULL, NULL) != UPNP_E_SUCCESS)
	{
		(*out) = NULL;
		(*errorString) = "Internal Error";
		
		return UPNP_E_INTERNAL_ERROR;
	}
	
	return UPNP_E_SUCCESS;
}


/******************************************************************************
 * WscDevRebootSTA
 *
 * Description: 
 *       Wsc Service action callback used to reboot the STA.
 *
 * Parameters:
 *   
 *    IXML_Document * in -  action request document
 *    IXML_Document **out - action result document
 *    char **errorString - errorString (in case action was unsuccessful)
 *  
 * Return:
 *    UPNP_E_SUCCESS 		- if success
 *    others 				- if failure
 *****************************************************************************/
static int
WscDevRebootSTA(
	IN IXML_Document * in,
	IN uint32 ipAddr,
	OUT IXML_Document ** out,
	OUT char **errorString)
{
	// TODO: Need to complete it, currently do nothing and return directly.
	
	//create a response	
	if(UpnpAddToActionResponse(out, "RebootSTA", WscServiceTypeStr, NULL, NULL) != UPNP_E_SUCCESS)
	{
		(*out) = NULL;
		(*errorString) = "Internal Error";
		
		return UPNP_E_INTERNAL_ERROR;
	}
	
	return UPNP_E_SUCCESS;
}


/******************************************************************************
 * WscDevResetSTA
 *
 * Description: 
 *       Wsc Service action callback used to reset the STA to factory default value.
 *
 * Parameters:
 *   
 *    IXML_Document * in -  action request document
 *    IXML_Document **out - action result document
 *    char **errorString - errorString (in case action was unsuccessful)
 *  
 * Return:
 *    UPNP_E_SUCCESS 		- if success
 *    others 				- if failure
 *****************************************************************************/
static int
WscDevResetSTA(
	IN IXML_Document * in,
	IN uint32 ipAddr,
	OUT IXML_Document ** out,
	OUT char **errorString)
{
	
	// TODO: Need to complete it, currently do nothing and return directly.

	//create a response	
	if(UpnpAddToActionResponse(out, "ResetSTA", WscServiceTypeStr, NULL, NULL) != UPNP_E_SUCCESS)
	{
		( *out ) = NULL;
		( *errorString ) = "Internal Error";
		
		return UPNP_E_INTERNAL_ERROR;
	}
	return UPNP_E_SUCCESS;

}


/******************************************************************************
 * wscU2KMsgCreate
 *
 * Description: 
 *       Allocate the memory and copy the content to the buffer.
 *
 * Parameters:
 *    char **dstPtr - pointer used for refer the allocated U2Kmsg.
 *    char *srcPtr  - the message need to copy into the U2KMsg.
 *    int 	msgLen  - length of the message "srcPtr".
 *    int	EAPType - the EAP message type. 
 *    				 			1=Identity, 0xFE=reserved, used by WSC
 * Return Value:
 *    Total length of created U2KMsg
 *    	zero 	- if failure
 *    	others  - if success 
 *****************************************************************************/
int 
wscU2KMsgCreate(
	INOUT char **dstPtr,
	IN char *srcPtr,
	IN int msgLen,
	IN int EAPType)
{
	int totalLen;
	char *pPos = NULL, *pMsgPtr = NULL;
	RTMP_WSC_U2KMSG_HDR *pU2KMsgHdr; 
	IEEE8021X_FRAME *p1xFrameHdr;
	EAP_FRAME	*pEAPFrameHdr;
		
	/* Allocate the msg buffer and fill the content */
	totalLen = sizeof(RTMP_WSC_U2KMSG_HDR) + msgLen;
	if ((pMsgPtr = malloc(totalLen)) != NULL)
	{
		memset(pMsgPtr , 0, totalLen);
		pU2KMsgHdr = (RTMP_WSC_U2KMSG_HDR *)pMsgPtr;
		
		// create the IEEE8021X_FRAME header
		p1xFrameHdr = &pU2KMsgHdr->IEEE8021XHdr;
		p1xFrameHdr->Version = IEEE8021X_FRAME_VERSION;
		p1xFrameHdr->Length = htons(sizeof(EAP_FRAME) + msgLen);
		p1xFrameHdr->Type = IEEE8021X_FRAME_TYPE_EAP;

		// create the EAP header
		pEAPFrameHdr = &pU2KMsgHdr->EAPHdr;
		pEAPFrameHdr->Code = EAP_FRAME_CODE_RESPONSE;
		pEAPFrameHdr->Id = 1;  // The Id field is useless here.
		pEAPFrameHdr->Type = EAPType;
		pEAPFrameHdr->Length = htons(sizeof(EAP_FRAME) + msgLen);

		//copy the msg payload
		pPos = (char *)(pMsgPtr + sizeof(RTMP_WSC_U2KMSG_HDR));
		memcpy(pPos, srcPtr, msgLen);
		*dstPtr = pMsgPtr;
		DBGPRINTF(RT_DBG_INFO, "create U2KMsg success!MsgLen = %d, headerLen=%d! totalLen=%d!\n", msgLen, sizeof(RTMP_WSC_U2KMSG_HDR), totalLen);
	} else {
		DBGPRINTF(RT_DBG_INFO, "malloc allocation(size=%d) failed in wscU2KMsgCreate()!\n", totalLen);
		totalLen = 0;
	}

	return totalLen;
}


/******************************************************************************
 * WscDevSetSelectedRegistrar
 *
 * Description: 
 *       This action callback used to receive a SetSelectedRegistar message send by
 *       contorl Point(Registrar). 
 *
 * Parameters:
 *   
 *    IXML_Document * in -  action request document
 *    IXML_Document **out - action result document
 *    char **errorString - errorString (in case action was unsuccessful)
 *  
 * Return:
 *    UPNP_E_SUCCESS 		- if success
 *    others 				- if failure
 *****************************************************************************/
static int WscDevSetSelectedRegistrar(
	IN IXML_Document * in,
	IN uint32 ipAddr,
	OUT IXML_Document ** out,
	OUT char **errorString)
{
	char *inStr = NULL;
	unsigned char *decodeStr = NULL;
	int decodeLen, retVal = -1;

	(*out) = NULL;
	(*errorString) = NULL;
	
	if (!(inStr = SampleUtil_GetFirstDocumentItem(in, "NewMessage")))
	{
		(*errorString) = "Invalid SetSelectedRegistrar mesg parameter";
		
		return UPNP_E_INVALID_PARAM;
	}
		 	
	decodeLen = ILibBase64Decode((unsigned char *)inStr, strlen(inStr), &decodeStr);
	if (decodeLen == 0 || decodeStr == NULL)
		goto done;
	
	wsc_hexdump("WscDevSetSelectedRegistrar", (char *)decodeStr, decodeLen);

	/* Now send ioctl to wireless driver to set the ProbeReponse bit field. */
	if (ioctl_sock >= 0)
		retVal = wsc_set_oid(RT_OID_WSC_SET_SELECTED_REGISTRAR, (char *)decodeStr, decodeLen);

	if (retVal != 0)
		goto done;
	
	/*
		Send UPnP repsone to remote UPnP device controller 
	*/
	retVal = UpnpAddToActionResponse(out, "SetSelectedRegistrar", WscServiceTypeStr, NULL, NULL);
	if (retVal != UPNP_E_SUCCESS)
		retVal = UPNP_E_INTERNAL_ERROR;

done:
	if (inStr)
		free(inStr);
	if (decodeStr)
		free(decodeStr);

	if (retVal != UPNP_E_SUCCESS)
		(*errorString) = "Internal Error";
	
	return UPNP_E_SUCCESS;
	
}


/******************************************************************************
 * WscDevPutWLANResponse
 *
 * Description: 
 *      When Device in Proxy Mode, the Registrar will use this Action response 
 *      the M2/M2D, M4, M6, and M8 messages.
 *
 * Parameters:
 *   
 *    IXML_Document * in -  action request document
 *    IXML_Document **out - action result document
 *    char **errorString - errorString (in case action was unsuccessful)
 *  
 * Return:
 *    UPNP_E_SUCCESS 		- if success
 *    UPNP_E_INTERNAL_ERROR - if failure
 *****************************************************************************/
static int
WscDevPutWLANResponse(
	IN IXML_Document * in,
	IN uint32 ipAddr,
	OUT IXML_Document ** out,
	OUT char **errorString)
{
	char *inStr = NULL;
	unsigned char *decodeStr = NULL;
	int decodeLen, retVal = UPNP_E_INTERNAL_ERROR;
	
	char *pWscU2KMsg = NULL;
	RTMP_WSC_U2KMSG_HDR *msgHdr = NULL;
	int wscU2KMsgLen;

	(*out) = NULL;
	(*errorString) = NULL;

	inStr = SampleUtil_GetFirstDocumentItem(in, "NewMessage");
	if (inStr == NULL)
	{
		(*errorString) = "Invalid PutWLANResponse msg";
		return UPNP_E_INVALID_PARAM;
	}
		 	
	decodeLen = ILibBase64Decode((unsigned char *)inStr, strlen(inStr), &decodeStr);
	if (decodeLen == 0 || decodeStr == NULL)
		goto done;
	
	/* Prepare the msg buffers */
	wscU2KMsgLen = wscU2KMsgCreate(&pWscU2KMsg, (char *)decodeStr, decodeLen, EAP_FRAME_TYPE_WSC);
	if (wscU2KMsgLen == 0)
		goto done;

	// Fill the sessionID, Addr1, and Adde2 to the U2KMsg buffer header.
	msgHdr = (RTMP_WSC_U2KMSG_HDR *)pWscU2KMsg;
	msgHdr->envID = 0;
	memcpy(msgHdr->Addr1, HostMacAddr, MAC_ADDR_LEN);
	memcpy(msgHdr->Addr2, &ipAddr, sizeof(unsigned int));
	
	DBGPRINTF(RT_DBG_INFO, "(%s):Ready to send pWscU2KMsg(len=%d) to kernel by ioctl(%d)!\n", __FUNCTION__, 
				wscU2KMsgLen, ioctl_sock);
	wsc_hexdump("U2KMsg", pWscU2KMsg, wscU2KMsgLen);
	
	// Now send the msg to kernel space.
	if (ioctl_sock >= 0)
		retVal = wsc_set_oid(RT_OID_WSC_EAPMSG, (char *)pWscU2KMsg, wscU2KMsgLen);

	// Waiting for the response from the kernel space.
	DBGPRINTF(RT_DBG_INFO, "ioctl retval=%d! senderID=%d!\n", retVal, senderID);
	if(retVal == 0)
		retVal = UpnpAddToActionResponse(out, "PutWLANResponse", WscServiceTypeStr, NULL, NULL);

done:

	if (inStr)
		free(inStr);
	if (decodeStr)
		free(decodeStr);
	if (pWscU2KMsg)
		free(pWscU2KMsg);
	
	if (retVal == UPNP_E_SUCCESS)
		return UPNP_E_SUCCESS;
	else {
		(*errorString) = "Internal Error";
		return UPNP_E_INTERNAL_ERROR;
	}
}


/******************************************************************************
 * WscDevPutMessage
 *
 * Description: 
 *       This action used by Registrar send WSC_MSG(M2,M4,M6, M8) to Enrollee.
 *
 * Parameters:
 *    
 *    IXML_Document * in - document of action request
 *    IXML_Document **out - action result
 *    char **errorString - errorString (in case action was unsuccessful)
 *
 * Return:
 *    UPNP_E_SUCCESS 		- if success
 *    UPNP_E_INTERNAL_ERROR - if failure
 *****************************************************************************/
static int 
WscDevPutMessage(
	IN IXML_Document * in,
	IN uint32 ipAddr,
	OUT IXML_Document ** out,
	OUT char **errorString)
{
	char *inStr = NULL;
	unsigned char *decodeStr = NULL;
	int decodeLen, retVal = UPNP_E_INTERNAL_ERROR;
	
	char *pWscU2KMsg = NULL;
	RTMP_WSC_U2KMSG_HDR *msgHdr = NULL;
	WscEnvelope *msgEnvelope = NULL;
	int wscU2KMsgLen;
	int msgQIdx = -1;

	
	(*out) = NULL;
	(*errorString) = NULL;

	inStr = SampleUtil_GetFirstDocumentItem(in, "NewInMessage");
	if (inStr == NULL)
	{
		(*errorString) = "Invalid PutMessage mesg";
        return UPNP_E_INVALID_PARAM;
	}

	decodeLen = ILibBase64Decode((unsigned char *)inStr, strlen(inStr), &decodeStr);
	if (decodeLen == 0 || decodeStr == NULL)
		goto done;
	
	/* Prepare the msg buffers */
	wscU2KMsgLen = wscU2KMsgCreate(&pWscU2KMsg, (char *)decodeStr, decodeLen, EAP_FRAME_TYPE_WSC);
	if (wscU2KMsgLen == 0)
		goto done;

	/* Prepare the msg envelope */
	if ((msgEnvelope = wscEnvelopeCreate()) == NULL)
		goto done;
	pthread_mutex_lock(&msgEnvelope->mutex);
	
	/* Lock the msgQ and check if we can get a valid mailbox to insert our request! */
	if(wscMsgQInsert(msgEnvelope, &msgQIdx)!= WSC_SYS_SUCCESS)
	{
		pthread_mutex_unlock(&msgEnvelope->mutex);
		goto done;
	}
	
	// Fill the sessionID, Addr1, and Adde2 to the U2KMsg buffer header.
	msgHdr = (RTMP_WSC_U2KMSG_HDR *)pWscU2KMsg;
	msgHdr->envID = msgEnvelope->envID;
	memcpy(msgHdr->Addr1, HostMacAddr, MAC_ADDR_LEN);
	memcpy(msgHdr->Addr2, &ipAddr, sizeof(unsigned int));
	
	DBGPRINTF(RT_DBG_INFO, "(%s):Ready to send pWscU2KMsg(len=%d) to kernel by ioctl(%d)!\n", __FUNCTION__, 
				wscU2KMsgLen, ioctl_sock);
	wsc_hexdump("U2KMsg", pWscU2KMsg, wscU2KMsgLen);
	
	// Now send the msg to kernel space.
	if (ioctl_sock >= 0)
		retVal = wsc_set_oid(RT_OID_WSC_EAPMSG, (char *)pWscU2KMsg, wscU2KMsgLen);

	if (retVal == 0){
		// Waiting for the response from the kernel space.
		DBGPRINTF(RT_DBG_INFO, "%s():ioctl to kernel success, waiting for condition!\n", __FUNCTION__);
		pthread_cond_wait(&msgEnvelope->condition, &msgEnvelope->mutex);
	} else {
		DBGPRINTF(RT_DBG_INFO, "%s():ioctl to kernel failed, retVal=%d, goto done!\n", __FUNCTION__, retVal);
		wscMsgQRemove(Q_TYPE_PASSIVE, msgQIdx);
		pthread_mutex_unlock(&msgEnvelope->mutex);
		goto done;
	}

	DBGPRINTF(RT_DBG_INFO, "Got msg from netlink! msgQIdx=%d, envID=0x%x!\n", msgQIdx, msgEnvelope->envID);
	if ((msgEnvelope->flag != WSC_ENVELOPE_SUCCESS) || 
		(msgEnvelope->pMsgPtr == NULL) || 
		(msgEnvelope->msgLen == 0))
	{
		pthread_mutex_unlock(&msgEnvelope->mutex);
		goto done;
	}

	// Response the msg from state machine back to the remote UPnP control device.
	if (msgEnvelope->pMsgPtr)
	{
		unsigned char *encodeStr = NULL;
		RTMP_WSC_MSG_HDR *rtmpHdr = NULL;		
		int len;

		DBGPRINTF(RT_DBG_INFO, "(%s):Ready to parse the K2U msg(len=%d)!\n", __FUNCTION__, msgEnvelope->msgLen);
		wsc_hexdump("WscDevPutMessage-K2UMsg", msgEnvelope->pMsgPtr, msgEnvelope->msgLen);
		
		rtmpHdr = (RTMP_WSC_MSG_HDR *)(msgEnvelope->pMsgPtr);
	
		if(rtmpHdr->msgType == WSC_OPCODE_UPNP_CTRL || rtmpHdr->msgType == WSC_OPCODE_UPNP_MGMT){
			DBGPRINTF(RT_DBG_INFO, "Receive a UPnP Ctrl/Mgmt Msg!\n");
			pthread_mutex_unlock(&msgEnvelope->mutex);
			goto done;
		}
		
		if(rtmpHdr->msgSubType == WSC_UPNP_DATA_SUB_ACK)
		{
			retVal = UpnpAddToActionResponse(out, "PutMessage", WscServiceTypeStr, NULL, NULL);
		}
		else 
		{
			len = ILibBase64Encode((unsigned char *)(msgEnvelope->pMsgPtr + sizeof(RTMP_WSC_MSG_HDR)), 
									rtmpHdr->msgLen, &encodeStr);
			if (len >0)
				retVal = UpnpAddToActionResponse(out, "PutMessage", WscServiceTypeStr, 
													"NewOutMessage", (char *)encodeStr);
			if (encodeStr != NULL)
				free(encodeStr);
		}
	} 
	pthread_mutex_unlock(&msgEnvelope->mutex);

done:
	if (inStr)
		free(inStr);
	if (decodeStr)
		free(decodeStr);
	
	if (pWscU2KMsg)
		free(pWscU2KMsg);
	
	wscEnvelopeFree(msgEnvelope);
	
	if (retVal == UPNP_E_SUCCESS)
		return UPNP_E_SUCCESS;
	else {
		(*errorString) = "Internal Error";
		return UPNP_E_INTERNAL_ERROR;
	}
	
}


/******************************************************************************
 * WscDevGetDeviceInfo
 *
 * Description: 
 *       This action callback used to send AP's M1 message to the Control Point(Registrar).
 *
 * Parameters:
 *
 *    IXML_Document * in  - document of action request
 *    uint32 ipAddr       - ipAddr,
 *    IXML_Document **out - action result
 *    char **errorString  - errorString (in case action was unsuccessful)
 *
 * Return:
 *    UPNP_E_SUCCESS 		- if success
 *    UPNP_E_INTERNAL_ERROR - if failure
 *****************************************************************************/
static int
WscDevGetDeviceInfo(
	IN IXML_Document * in,
	IN uint32 ipAddr,
	OUT IXML_Document ** out,
	OUT char **errorString)
{
	char UPnPIdentity[] = {"WFA-SimpleConfig-Registrar"};
	
	int retVal= UPNP_E_INTERNAL_ERROR;
	char *pWscU2KMsg = NULL;
	WscEnvelope *msgEnvelope = NULL;
	RTMP_WSC_U2KMSG_HDR *msgHdr = NULL;
	int wscU2KMsgLen;
	int msgQIdx = -1;
	
	
	DBGPRINTF(RT_DBG_INFO, "Receive a GetDeviceInfo msg from Remote Upnp Control Point!\n");
	
	( *out ) = NULL;
	( *errorString ) = NULL;

	/* Prepare the msg buffers */
	wscU2KMsgLen = wscU2KMsgCreate(&pWscU2KMsg, UPnPIdentity, strlen(UPnPIdentity), EAP_FRAME_TYPE_IDENTITY);
	if (wscU2KMsgLen == 0)
		goto done;

	/* Prepare the msg envelope */
	if ((msgEnvelope = wscEnvelopeCreate()) == NULL)
		goto done;
	
	pthread_mutex_lock(&msgEnvelope->mutex);
	
	/* Lock the msgQ and check if we can get a valid mailbox to insert our request! */
	if (wscMsgQInsert(msgEnvelope, &msgQIdx) != WSC_SYS_SUCCESS)
	{
		pthread_mutex_unlock(&msgEnvelope->mutex);
		goto done;
	}
	
	// Fill the sessionID, Addr1, and Adde2 to the U2KMsg buffer header.
	msgHdr = (RTMP_WSC_U2KMSG_HDR *)pWscU2KMsg;
	msgHdr->envID = msgEnvelope->envID;
	memcpy(msgHdr->Addr1, HostMacAddr, MAC_ADDR_LEN);
	memcpy(msgHdr->Addr2, &ipAddr, sizeof(int));
	
	// Now send the msg to kernel space.
	DBGPRINTF(RT_DBG_INFO, "(%s):Ready to send pWscU2KMsg(len=%d) to kernel by ioctl(%d)!\n", __FUNCTION__, 
				wscU2KMsgLen, ioctl_sock);
	wsc_hexdump("U2KMsg", pWscU2KMsg, wscU2KMsgLen);

	if (ioctl_sock >= 0)
		retVal = wsc_set_oid(RT_OID_WSC_EAPMSG, pWscU2KMsg, wscU2KMsgLen);
	else
		retVal = UPNP_E_INTERNAL_ERROR;
	
	if (retVal == 0){
		// Waiting for the response from the kernel space.
		DBGPRINTF(RT_DBG_INFO, "%s():ioctl to kernel success, waiting for condition!\n", __FUNCTION__);
		pthread_cond_wait(&msgEnvelope->condition, &msgEnvelope->mutex);
	} else {
		DBGPRINTF(RT_DBG_ERROR, "%s():ioctl to kernel failed, retVal=%d, goto done!\n", __FUNCTION__, retVal);
		pthread_mutex_unlock(&msgEnvelope->mutex);
		wscMsgQRemove(Q_TYPE_PASSIVE, msgQIdx);
		goto done;
	}

	DBGPRINTF(RT_DBG_INFO, "(%s):Got msg from netlink! msgQIdx=%d, envID=0x%x, flag=%d!\n", 
				__FUNCTION__, msgQIdx, msgEnvelope->envID, msgEnvelope->flag);
	if ((msgEnvelope->flag != WSC_ENVELOPE_SUCCESS) || 
		(msgEnvelope->pMsgPtr == NULL) || 
		(msgEnvelope->msgLen == 0))
	{
		pthread_mutex_unlock(&msgEnvelope->mutex);
		goto done;
	}

	// Response the msg from state machine back to the remote UPnP control device.
	if (msgEnvelope->pMsgPtr)
	{
		unsigned char *encodeStr = NULL;
		RTMP_WSC_MSG_HDR *rtmpHdr = NULL;		
		int len;

		rtmpHdr = (RTMP_WSC_MSG_HDR *)(msgEnvelope->pMsgPtr);
		DBGPRINTF(RT_DBG_INFO, "(%s):Ready to parse the K2U msg(TotalLen=%d, headerLen=%d)!\n", __FUNCTION__, 
						msgEnvelope->msgLen, sizeof(RTMP_WSC_MSG_HDR));
		DBGPRINTF(RT_DBG_INFO, "\tMsgType=%d!\n", rtmpHdr->msgType);
		DBGPRINTF(RT_DBG_INFO, "\tMsgSubType=%d!\n", rtmpHdr->msgSubType);
		DBGPRINTF(RT_DBG_INFO, "\tipAddress=0x%x!\n", rtmpHdr->ipAddr);
		DBGPRINTF(RT_DBG_INFO, "\tMsgLen=%d!\n", rtmpHdr->msgLen);
		
		if(rtmpHdr->msgType == WSC_OPCODE_UPNP_CTRL || rtmpHdr->msgType == WSC_OPCODE_UPNP_MGMT)
		{
			DBGPRINTF(RT_DBG_INFO, "Receive a UPnP Ctrl/Mgmt Msg!\n");
			pthread_mutex_unlock(&msgEnvelope->mutex);
			goto done;
		}
		wsc_hexdump("WscDevGetDeviceInfo-K2UMsg", msgEnvelope->pMsgPtr, msgEnvelope->msgLen);
		
		if(rtmpHdr->msgSubType == WSC_UPNP_DATA_SUB_ACK)
		{
			retVal = UpnpAddToActionResponse(out, "GetDeviceInfo", WscServiceTypeStr, NULL, NULL);
		}
		else
		{
			len = ILibBase64Encode((unsigned char *)(msgEnvelope->pMsgPtr + sizeof(RTMP_WSC_MSG_HDR)), 
									rtmpHdr->msgLen, &encodeStr);

			if (len >0)
				retVal = UpnpAddToActionResponse(out, "GetDeviceInfo", WscServiceTypeStr, 
												"NewDeviceInfo", (char *)encodeStr);
			
			if (encodeStr != NULL)
				free(encodeStr);
		}
	} 
	pthread_mutex_unlock(&msgEnvelope->mutex);
		
done:
	if (pWscU2KMsg)
		free(pWscU2KMsg);
	
	wscEnvelopeFree(msgEnvelope);
	
	if (retVal == UPNP_E_SUCCESS)
		return UPNP_E_SUCCESS;
	else {
		(*errorString) = "Internal Error";
		return UPNP_E_INTERNAL_ERROR;
	}
}


/******************************************************************************
 * WscDevHandleActionReq
 *
 * Description: 
 *       Called during an action request callback.  If the request is for the
 *       Wsc device and match the ServiceID, then perform the action and respond.
 *
 * Parameters:
 *   ca_event -- The "Upnp_Action_Request" structure
 *
 * Return:
 *    
 *****************************************************************************/
static int
WscDevHandleActionReq(IN struct Upnp_Action_Request *ca_event)
{
	int action_found = 0;
	int i = 0;
	int retCode = 0;
	char *errorString = NULL;


	/* Defaults if action not found */
	ca_event->ErrCode = 0;
	ca_event->ActionResult = NULL;

	if ((strcmp(ca_event->DevUDN, wscLocalDevice.UDN) != 0) ||
		(strcmp(ca_event->ServiceID, wscLocalDevice.services.ServiceId) != 0))
	{
		/* Request for action in the Wsc Device Control Service  */
		ca_event->ErrCode = UPNP_E_INVALID_SERVICE;
		return ca_event->ErrCode;
	}

{// For debug
	DBGPRINTF(RT_DBG_INFO, "Get a new Action Request:\n");
	DBGPRINTF(RT_DBG_INFO, "ca_event->ActionName=%s!\n", ca_event->ActionName);
	DBGPRINTF(RT_DBG_INFO, "ca_event->CtrlIPtIPAddr=%s!\n", inet_ntoa(ca_event->CtrlPtIPAddr));
	DBGPRINTF(RT_DBG_INFO, "ca_event->Socket=%d!\n", ca_event->Socket);
	DBGPRINTF(RT_DBG_INFO, "ca_event->ServiceID=%s!\n", ca_event->ServiceID);
	DBGPRINTF(RT_DBG_INFO, "ca_envet->DevUDN=%s!\n", ca_event->DevUDN);
	DBGPRINTF(RT_DBG_INFO, "----\n");
	
}
	/* 
		Find and call appropriate procedure based on action name. Each action name has an 
		associated procedure stored in the service table. These are set at initialization.
	*/
	for (i = 0; ((i < WSC_ACTION_COUNTS) &&(wscDevActTable.actionNames[i] != NULL)); i++)
	{
		if (strcmp(ca_event->ActionName, wscDevActTable.actionNames[i]) == 0)
		{
			retCode = wscDevActTable.actionFuncs[i](ca_event->ActionRequest, ca_event->CtrlPtIPAddr.s_addr,
														&ca_event->ActionResult, &errorString);
			action_found = 1;
			break;
		}
	}

	if (!action_found)
	{
		ca_event->ActionResult = NULL;
		strcpy(ca_event->ErrStr, "Invalid Action");
		ca_event->ErrCode = 401;
	} else {
		if (retCode == UPNP_E_SUCCESS)
		{
			ca_event->ErrCode = UPNP_E_SUCCESS;
		} else {
			//copy the error string 
			strcpy(ca_event->ErrStr, errorString);
			switch (retCode)
			{
				case UPNP_E_INVALID_PARAM:
						ca_event->ErrCode = 402;
						break;
				case UPNP_E_INTERNAL_ERROR:
				default:
						ca_event->ErrCode = 501;
						break;
			}
        }
    }

    return (ca_event->ErrCode);
}


/******************************************************************************
 * WscDevCPNodeInsert
 *
 * Description: 
 *       Depends on the UDN String, service identifier and Subscription ID, find 
 *		 the IP address of the remote Control Point. And insert the Control Point
 *       into the "WscCPList".  
 *
 * NOTE: 
 *		 Since this function blocks on the mutex WscDevMutex, to avoid a hang this 
 *		 function should not be called within any other function that currently has 
 *		 this mutex locked.
 *
 *		 Besides, this function use the internal data structure of libupnp-1.3.1 
 *		 SDK, so if you wanna change the libupnp to other versions, please make sure
 *		 the data structure refered in this function was not changed!
 *
 * Parameters:
 *   UpnpDevice_Handle  - UpnpDevice handle of WscLocalDevice assigned by UPnP SDK.
 *   char *UDN			- The UUID string of the Subscription requested.
 *   char *servId 		- The service Identifier of the Subscription requested.
 *   char *sid 			- The Subscription ID of the ControlPoint.
 * 
 * Return:
 *   TRUE  - If success
 *   FALSE - If failure
 *****************************************************************************/
static int 
WscDevCPNodeInsert(
	IN UpnpDevice_Handle device_handle,
	IN char *UDN,
	IN char *servId,
	IN char *sid)
{
	struct Handle_Info *handle_info;
	service_info *service;
	subscription *sub;
	uri_type *uriInfo = NULL;
	int found = 0;
	unsigned int ipAddr;
	
	struct upnpCPNode *CPNode, *newCPNode;
	
	HandleLock();
	if (GetHandleInfo(device_handle, &handle_info) != HND_DEVICE)
		goto Fail;

    if ((service = FindServiceId(&handle_info->ServiceTable, servId, UDN)) == NULL)
		goto Fail;
	
    DBGPRINTF(RT_DBG_INFO, "found service in WscUPnPDevGetIPBySID: UDN=%s, ServID= %s\n", UDN, servId);
	if (((sub = GetSubscriptionSID(sid, service)) == NULL) || (sub->active))
		goto Fail;
	
	if ((uriInfo = sub->DeliveryURLs.parsedURLs))
	{
		ipAddr = uriInfo->hostport.IPv4address.sin_addr.s_addr;
		if(ipAddr == HostIPAddr)
		{
			DBGPRINTF(RT_DBG_INFO, "Control Point from the same host with our Device! Ignore it(ipAddr=0x%08x)\n", ipAddr);
			goto Fail;
		}

		DBGPRINTF(RT_DBG_INFO, "get the ipAddr=0x%x! timeOut=%lld!\n", ipAddr, (long long int)sub->expireTime);
		DBGPRINTF(RT_DBG_INFO, "URLs=%s!\n", sub->DeliveryURLs.URLs);
		
		pthread_mutex_lock(&wscCPListMutex);
		CPNode = WscCPList;
		while(CPNode)
		{
			if( (strcmp(CPNode->device.SID, sid) == 0) ||
				(CPNode->device.ipAddr == ipAddr))
			{
				found = 1;
				break;
			}
			CPNode = CPNode->next;
		}

		if (found)
		{
			strncpy(CPNode->device.SID, sid, NAME_SIZE);
			CPNode->device.ipAddr = ipAddr;
			CPNode->device.SubTimeOut = sub->expireTime;
			strncpy(CPNode->device.SubURL, sub->DeliveryURLs.URLs, NAME_SIZE);
		} else {
			// It's a new subscriber, insert it.
			if ((newCPNode = malloc(sizeof(struct upnpCPNode))) != NULL)
			{
				memset(newCPNode, 0, sizeof(struct upnpCPNode));
				newCPNode->device.ipAddr = ipAddr;
				strncpy(newCPNode->device.SID, sid, NAME_SIZE);
				newCPNode->device.SubTimeOut = sub->expireTime;
				strncpy(newCPNode->device.SubURL, sub->DeliveryURLs.URLs, NAME_SIZE);
				newCPNode->next = NULL;

				if(WscCPList)
				{
					newCPNode->next = WscCPList->next;
					WscCPList->next = newCPNode;
				} else {
					WscCPList = newCPNode;
				}
			} else {
				pthread_mutex_unlock(&wscCPListMutex);
				goto Fail;
			}
		}
		pthread_mutex_unlock(&wscCPListMutex);

		HandleUnlock();
		DBGPRINTF(RT_DBG_INFO, "Insert ControlPoint success!\n");
		
		return TRUE;
	}

Fail:
	
	HandleUnlock();	
	DBGPRINTF(RT_DBG_ERROR, "Insert ControlPoint failed!\n");	
	return FALSE;
	
}



/******************************************************************************
 * WscDevCPNodeSearch
 *
 * Description: 
 *   Search for specific Control Point Node by ip address in WscCPList
 *
 * Parameters:
 *   unsigned int ipAddr - ip address of the contorl point we want to seach.
 *   char *sid           - used to copy the SID string
 *
 * Return:
 *   1 - if found
 *   0 - if not found
 *****************************************************************************/
static int 
WscDevCPNodeSearch(
	IN unsigned int ipAddr,
	OUT char **strSID)
{
	struct upnpCPNode *CPNode;
	int found = 0;
	
	pthread_mutex_lock(&wscCPListMutex);
	
	CPNode = WscCPList;
	while (CPNode)
	{
		if(CPNode->device.ipAddr == ipAddr)
		{
			*strSID = strdup(CPNode->device.SID);
			found = 1;
			break;
		}
		CPNode = CPNode->next;
	}

	pthread_mutex_unlock(&wscCPListMutex);

	return found;
	
}


/******************************************************************************
 * WscDevCPListRemoveAll
 *
 * Description: 
 *   Remove all Control Point Node in WscCPList
 *
 * Parameters:
 *   None
 *
 * Return:
 *   TRUE
 *****************************************************************************/
static int WscDevCPListRemoveAll(void)
{
	struct upnpCPNode *CPNode;

	pthread_mutex_lock(&wscCPListMutex);
	while((CPNode = WscCPList))
	{
		WscCPList = CPNode->next;
		free(CPNode);
	}
	pthread_mutex_unlock(&wscCPListMutex);
	
	return TRUE;
}

/******************************************************************************
 * WscDevCPNodeRemove
 *
 * Description: 
 *       Remove the ControlPoint Node in WscCPList depends on the subscription ID.
 *
 * Parameters:
 *   char *SID - The subscription ID of the ControlPoint will be deleted
 * 
 * Return:
 *   TRUE  - If success
 *   FALSE - If failure
 *****************************************************************************/
int WscDevCPNodeRemove(IN char *SID)
{
	struct upnpCPNode *CPNode, *prevNode = NULL;
	
	pthread_mutex_lock(&wscCPListMutex);
	CPNode = prevNode = WscCPList;

	if(strcmp(WscCPList->device.SID, SID) == 0)
	{
		WscCPList = WscCPList->next;
		free(CPNode);
	} else {
		while((CPNode = CPNode->next))
		{
			if(strcmp(CPNode->device.SID, SID) == 0)
			{
				prevNode->next = CPNode->next;
				free(CPNode);
				break;
			}
			prevNode = CPNode;
		}
	}
	pthread_mutex_unlock(&wscCPListMutex);

	return TRUE;
}


/******************************************************************************
 * dumpDevCPNodeList
 *
 * Description: 
 *       Dump the WscCPList. Used for debug.
 *
 * Parameters:
 *   	void
 *
 *****************************************************************************/
void dumpDevCPNodeList(void)
{
	struct upnpCPNode *CPNode;
	int i=0;
	
	printf("Dump The UPnP Subscribed ControlPoint:\n");

	pthread_mutex_lock(&wscCPListMutex);
	CPNode = WscCPList;	
	while(CPNode)
	{
		i++;
		printf("ControlPoint Node[%d]:\n", i);
		printf("\t ->sid=%s\n", CPNode->device.SID);
		printf("\t ->SubURL=%s\n", CPNode->device.SubURL);
		printf("\t ->ipAddr=0x%x!\n", CPNode->device.ipAddr);
		printf("\t ->SubTimeOut=%d\n", CPNode->device.SubTimeOut);
		CPNode = CPNode->next;
	}
	pthread_mutex_unlock(&wscCPListMutex);
	
	printf("\n-----DumpFinished!\n");

}


/******************************************************************************
 * WscDevStateVarUpdate
 *
 * Description: 
 *       Update the Wsc Device Service State variable, and notify all subscribed 
 *       Control Points of the updated state.  Note that since this function
 *       blocks on the mutex WscDevMutex, to avoid a hang this function should 
 *       not be called within any other function that currently has this mutex 
 *       locked.
 *
 * Parameters:
 *   variable -- The variable number (WSC_EVENT_WLANEVENT, WSC_EVENT_APSTATUS,
 *                   WSC_EVENT_STASTATUS)
 *   value -- The string representation of the new value
 *
 *****************************************************************************/
static int
WscDevStateVarUpdate(
	IN unsigned int variable,
	IN char *value,
	IN char *SubsId)
{
	IXML_Document *PropSet= NULL;

	if((variable >= WSC_STATE_VAR_MAXVARS) || (strlen(value) >= WSC_STATE_VAR_MAX_STR_LEN))
		return (0);

	pthread_mutex_lock(&WscDevMutex);

	strcpy(wscLocalDevice.services.StateVarVal[variable], value);
	
	//Using utility api
	PropSet = UpnpCreatePropertySet(3,
									wscStateVarName[WSC_EVENT_WLANEVENT],
									wscLocalDevice.services.StateVarVal[WSC_EVENT_WLANEVENT],
									wscStateVarName[WSC_EVENT_APSTATUS],
									wscLocalDevice.services.StateVarVal[WSC_EVENT_APSTATUS],
									wscStateVarName[WSC_EVENT_STASTATUS],
									wscLocalDevice.services.StateVarVal[WSC_EVENT_STASTATUS]);

	if (PropSet)
	{
		if (SubsId == NULL)
		{
			UpnpNotifyExt(wscDevHandle, wscLocalDevice.UDN, 
							wscLocalDevice.services.ServiceId, PropSet);
		}
		else 
		{
			struct Handle_Info *SInfo = NULL;
			
			if (GetHandleInfo(wscDevHandle, &SInfo) != HND_DEVICE)
			{
				HandleUnlock();
				return UPNP_E_INVALID_HANDLE;
			}
		    HandleUnlock();
    		genaInitNotifyExt(wscDevHandle, wscLocalDevice.UDN, 
								wscLocalDevice.services.ServiceId, PropSet, SubsId);
		}
		//Free created property set
		ixmlDocument_free(PropSet);
	}
#if 0
    UpnpNotify( wscDevHandle,
                wscLocalDevice.UDN,
                wscLocalDevice.services.ServiceId,
                (const char **)&wscStateVarName[variable],
                (const char **)&wscLocalDevice.services.StateVarVal[variable], 1);
#endif

    pthread_mutex_unlock(&WscDevMutex);

    return (1);

}

/*
	The format of UPnP WLANEvent message send to Remote Registrar.
	  WLANEvent:
		=>This variable represents the concatenation of the WLANEventType, WLANEventMac and the 
		  802.11 WSC message received from the Enrollee & forwarded by the proxy over UPnP.
		=>the format of this variable
			->represented as base64(WLANEventType || WLANEventMAC || Enrollee's message).
	  WLANEventType:
		=>This variable represents the type of WLANEvent frame that was received by the proxy on 
		  the 802.11 network.
		=>the options for this variable
			->1: 802.11 WCN-NET Probe Frame
			->2: 802.11 WCN-NET 802.1X/EAP Frame
	  WLANEventMAC:
		=>This variable represents the MAC address of the WLAN Enrollee that generated the 802.11 
	 	  frame that was received by the proxy.
	 	=>Depends on the WFAWLANConfig:1  Service Template Version 1.01, the format is
 			->"xx:xx:xx:xx:xx:xx", case-independent, 17 char
*/	
int WscEventCtrlMsgRecv(
	IN char *pBuf,
	IN int  bufLen)
{

	DBGPRINTF(RT_DBG_INFO, "Receive a Control Message!\n");
	return 0;

}


//receive a WSC message and send it to remote UPnP Contorl Point
int WscEventDataMsgRecv(
	IN char *pBuf,
	IN int  bufLen)
{
	RTMP_WSC_MSG_HDR *pHdr = NULL;
	unsigned char *encodeStr = NULL, *pWscMsg = NULL, *pUPnPMsg = NULL;
	int encodeLen = 0, UPnPMsgLen = 0;
	int retVal;
	uint32 wscLen;
	char *strSID = NULL;
	unsigned char includeMAC = 0;
	char curMacStr[18];
	
	if ((WscUPnPOpMode & UPNP_OPMODE_DEV) == 0)
		return -1;
		
	pHdr = (RTMP_WSC_MSG_HDR *)pBuf;
		
#if 0
	//  Find the SID.
	if(WscDevCPNodeSearch(pHdr->ipAddr, &strSID) == 0)
	{
		DBGPRINTF(RT_DBG_INFO, "%s(): Didn't find the SID by ip(0x%x)!\n", __FUNCTION__, pHdr->ipAddr);
	} else {
		DBGPRINTF(RT_DBG_INFO, "%s(): The SID(%s) by ip(0x%x)!\n", __FUNCTION__, pHdr->ipAddr, strSID);
	}
#endif	
	DBGPRINTF(RT_DBG_INFO, "%s:Receive a Data event, msgSubType=%d!\n", __FUNCTION__, pHdr->msgSubType);

	memset(curMacStr, 0 , sizeof(curMacStr));
	if ((pHdr->msgSubType & WSC_UPNP_DATA_SUB_INCLUDE_MAC) == WSC_UPNP_DATA_SUB_INCLUDE_MAC)
	{
		if (pHdr->msgLen < 6){
			DBGPRINTF(RT_DBG_ERROR, "pHdr->msgSubType didn't have enoguh length!\n", pHdr->msgLen);
			return -1;
		}
		includeMAC = 1;
		pHdr->msgSubType &= 0x00ff;
		pWscMsg = (unsigned char *)(pBuf + sizeof(RTMP_WSC_MSG_HDR));
		snprintf(curMacStr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", pWscMsg[0], pWscMsg[1], pWscMsg[2], pWscMsg[3],pWscMsg[4],pWscMsg[5]);	
	}
	else
	{
		memcpy(&curMacStr[0], CfgMacStr, 17);
	}

	
	if (pHdr->msgSubType == WSC_UPNP_DATA_SUB_NORMAL || 
		pHdr->msgSubType == WSC_UPNP_DATA_SUB_TO_ALL ||
		pHdr->msgSubType == WSC_UPNP_DATA_SUB_TO_ONE)
	{

		DBGPRINTF(RT_DBG_INFO, "(%s):Ready to parse the K2U msg(len=%d)!\n", __FUNCTION__, bufLen);
		wsc_hexdump("WscEventDataMsgRecv-K2UMsg", pBuf, bufLen);

		pWscMsg = (unsigned char *)(pBuf + sizeof(RTMP_WSC_MSG_HDR));
		//Get the WscData Length
		wscLen = pHdr->msgLen;
		
		if (includeMAC)
		{
			wscLen -= MAC_ADDR_LEN;
			pWscMsg += MAC_ADDR_LEN;
		}
		
		DBGPRINTF(RT_DBG_INFO, "(%s): pWscMsg Len=%d!\n", __FUNCTION__, wscLen);
			
			
		UPnPMsgLen = wscLen + 18;
		pUPnPMsg = malloc(UPnPMsgLen);
		if(pUPnPMsg)
		{
			memset(pUPnPMsg, 0, UPnPMsgLen);
			pUPnPMsg[0] = WSC_EVENT_WLANEVENTTYPE_EAP;
			memcpy(&pUPnPMsg[1], &curMacStr[0], 17);

			//Copy the WscMsg to pUPnPMsg buffer
			memcpy(&pUPnPMsg[18], pWscMsg, wscLen);
			wsc_hexdump("UPnP WLANEVENT Msg", (char *)pUPnPMsg, UPnPMsgLen);

			//encode the message use base64
			encodeLen = ILibBase64Encode(pUPnPMsg, UPnPMsgLen, &encodeStr);
			
			//Send event out
			if (encodeLen > 0){
				DBGPRINTF(RT_DBG_INFO, "EAP->Msg=%s!\n", encodeStr);
				retVal = WscDevStateVarUpdate(WSC_EVENT_WLANEVENT, (char *)encodeStr, strSID);
			}
			if (encodeStr != NULL)
				free(encodeStr);
			free(pUPnPMsg);
		}
	}
	
	if (strSID)
		free(strSID);
	
	return 0;
	
}


/*
	Format of iwcustom msg WSC RegistrarSelected message:
	1. The Registrar ID which send M2 to the Enrollee(4 bytes):
								
			  4
		+-----------+
		|RegistrarID|
*/
static int WscEventMgmt_RegSelect(	
	IN char *pBuf,
	IN int  bufLen)
{
	char *pWscMsg = NULL;
	int registrarID = 0;
		
	if ((WscUPnPOpMode & UPNP_OPMODE_DEV) == 0)
		return -1;
	
	DBGPRINTF(RT_DBG_INFO, "(%s):Ready to parse the K2U msg(len=%d)!\n", __FUNCTION__, bufLen);
	wsc_hexdump("WscEventMgmt_RegSelect-K2UMsg", pBuf, bufLen);

	pWscMsg = (char *)(pBuf + sizeof(RTMP_WSC_MSG_HDR));

	registrarID = *((int *)pWscMsg);
	DBGPRINTF(RT_DBG_INFO, "The registrarID=%d!\n", registrarID);
	
	return 0;

}

/*
	Format of iwcustom msg WSC clientJoin message:
	1. SSID which station want to probe(32 bytes):
			<SSID string>
		*If the length if SSID string is small than 32 bytes, fill 0x0 for remaining bytes.
	2. Station MAC address(6 bytes):
	3. Status:
		Value 1 means change APStatus as 1. 
		Value 2 means change STAStatus as 1.
		Value 3 means trigger msg.
								
			32         6       1 
		+----------+--------+------+
		|SSIDString| SrcMAC |Status|
*/
static int WscEventMgmt_ConfigReq(
	IN char *pBuf,
	IN int  bufLen)
{
	unsigned char *encodeStr = NULL, *pWscMsg = NULL, *pUPnPMsg = NULL;
	int encodeLen = 0, UPnPMsgLen = 0;
	int retVal;
	unsigned char Status;
	char triggerMac[18];
		
	
	DBGPRINTF(RT_DBG_INFO, "(%s):Ready to parse the K2U msg(len=%d)!\n", __FUNCTION__, bufLen);
	wsc_hexdump("WscEventMgmt_ConfigReq-K2UMsg", pBuf, bufLen);

	pWscMsg = (unsigned char *)(pBuf + sizeof(RTMP_WSC_MSG_HDR));

	memset(&triggerMac[0], 0, 18);
	memcpy(&triggerMac[0], pWscMsg, 18);
	//Skip the SSID field
	pWscMsg += 32;
			
#if 0
	// Get the SrcMAC and copy to the WLANEVENTMAC, in format "xx:xx:xx:xx:xx:xx", case-independent, 17 char.
	memset(CfgMacStr, 0 , sizeof(CfgMacStr));
	snprintf(CfgMacStr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", pWscMsg[0], pWscMsg[1], pWscMsg[2],
															pWscMsg[3],pWscMsg[4],pWscMsg[5]);
#endif
	//Skip the SrcMAC field
	pWscMsg += 6;

	//Change APStatus and STAStatus
	Status = *(pWscMsg);
	DBGPRINTF(RT_DBG_INFO, "(%s): Status =%d!\n", __FUNCTION__, Status);

	if ((WscUPnPOpMode & UPNP_OPMODE_DEV) && (strlen(triggerMac) == 0))
	{
	strcpy(wscLocalDevice.services.StateVarVal[WSC_EVENT_APSTATUS], "0");
	strcpy(wscLocalDevice.services.StateVarVal[WSC_EVENT_STASTATUS], "0");

	if(Status == 3)
	{
		/* This "ConfigReq" is the trigger msg, we should send BYEBYE to all external registrar for 
			reset the cache status of those Vista devices. Then re-send Notify out.
		*/
    	retVal = AdvertiseAndReply(-1, wscDevHandle, 0, (struct sockaddr_in *)NULL,(char *)NULL, 
								(char *)NULL, (char *)NULL, defAdvrExpires);
	    retVal = AdvertiseAndReply(1, wscDevHandle, 0, (struct sockaddr_in *)NULL,(char *)NULL, 
								(char *)NULL, (char *)NULL, defAdvrExpires);
	}

	//Prepare the message.
	UPnPMsgLen = 18 + sizeof(wscAckMsg);
	pUPnPMsg = malloc(UPnPMsgLen);
	
	if(pUPnPMsg)
	{
		memset(pUPnPMsg, 0, UPnPMsgLen);
		pUPnPMsg[0] = WSC_EVENT_WLANEVENTTYPE_EAP;
		memcpy(&pUPnPMsg[1], CfgMacStr, 17);

		//jump to the WscProbeReqData and copy to pUPnPMsg buffer
		pWscMsg++;	
		memcpy(&pUPnPMsg[18], wscAckMsg, sizeof(wscAckMsg));
		wsc_hexdump("UPnP WLANEVENT Msg", (char *)pUPnPMsg, UPnPMsgLen);
				
		//encode the message use base64
		encodeLen = ILibBase64Encode(pUPnPMsg, UPnPMsgLen, &encodeStr);
		if(encodeLen > 0)
			DBGPRINTF(RT_DBG_INFO, "ConfigReq->Msg=%s!\n", encodeStr);
		strcpy(wscLocalDevice.services.StateVarVal[WSC_EVENT_WLANEVENT], (const char *)encodeStr);
		
		//Send event out
		if (encodeLen > 0)
			retVal = WscDevStateVarUpdate(WSC_EVENT_WLANEVENT, (char *)encodeStr, NULL);

		if (encodeStr != NULL)
			free(encodeStr);
		free(pUPnPMsg);
	}
	}

	if ((WscUPnPOpMode & UPNP_OPMODE_CP) && (strlen(triggerMac) > 0))
	{
		
	}

	DBGPRINTF(RT_DBG_INFO, "<-----End(%s)\n", __FUNCTION__);
	return 0;
}

/*
	Format of iwcustom msg WSC ProbeReq message:
	1. SSID which station want to probe(32 bytes):
			<SSID string>
		*If the length if SSID string is small than 32 bytes, fill 0x0 for remaining bytes.
	2. Station MAC address(6 bytes):
	3. element ID(OID)(1 byte):
			val=0xDD
	4. Length of "WscProbeReqData" in the probeReq packet(1 byte):
	5. "WscProbeReqData" info in ProbeReq:
								
			32        6      1          1          variable length
		+----------+--------+---+-----------------+----------------------+
		|SSIDString| SrcMAC |eID|LenOfWscProbeData|    WscProbeReqData   |

*/
static int WscEventMgmt_ProbreReq(
	IN char *pBuf,
	IN int  bufLen)
{
	unsigned char *encodeStr = NULL, *pWscMsg = NULL, *pUPnPMsg = NULL;
	char srcMacStr[18];
	int encodeLen = 0, UPnPMsgLen = 0;
	int retVal;
	unsigned char wscLen;
		
	if ((WscUPnPOpMode & UPNP_OPMODE_DEV) == 0)
		return -1;
	
	DBGPRINTF(RT_DBG_INFO, "(%s):Ready to parse the K2U msg(len=%d)!\n", __FUNCTION__, bufLen);
	wsc_hexdump("WscMgmtEvent_ProbreReq-K2UMsg", pBuf, bufLen);

	pWscMsg = (unsigned char *)(pBuf + sizeof(RTMP_WSC_MSG_HDR));
	//Skip the SSID field
	pWscMsg += 32;
			
	/* Get the SrcMAC and copy to the WLANEVENTMAC, 
		depends on the WFAWLANConfig:1  Service Template Version 1.01, 
		the MAC format is "xx:xx:xx:xx:xx:xx", case-independent, 17 char.
	*/
	memset(srcMacStr, 0 , sizeof(srcMacStr));
	snprintf(srcMacStr, 17, "%02x:%02x:%02x:%02x:%02x:%02x", pWscMsg[0], pWscMsg[1], pWscMsg[2],pWscMsg[3],pWscMsg[4],pWscMsg[5]);
	DBGPRINTF(RT_DBG_INFO, "ProbeReq->Mac=%s!\n", srcMacStr);

	//Skip the SrcMAC field & eID field
	pWscMsg += (6 + 1);

	//Get the WscProbeData Length
	wscLen = *(pWscMsg);
	DBGPRINTF(RT_DBG_INFO, "(%s): pWscHdr Len=%d!\n", __FUNCTION__, wscLen);
		
	UPnPMsgLen = wscLen + 18;
	pUPnPMsg = malloc(UPnPMsgLen);
	
	if(pUPnPMsg)
	{
		memset(pUPnPMsg, 0, UPnPMsgLen);
		pUPnPMsg[0] = WSC_EVENT_WLANEVENTTYPE_PROBE;
		memcpy(&pUPnPMsg[1], srcMacStr, 17);

		//jump to the WscProbeReqData and copy to pUPnPMsg buffer
		pWscMsg++;	
		memcpy(&pUPnPMsg[18], pWscMsg, wscLen);
		wsc_hexdump("UPnP WLANEVENT Msg", (char *)pUPnPMsg, UPnPMsgLen);
				
		//encode the message use base64
		encodeLen = ILibBase64Encode(pUPnPMsg, UPnPMsgLen, &encodeStr);
			
		//Send event out
		if (encodeLen > 0){
			DBGPRINTF(RT_DBG_INFO, "ProbeReq->Msg=%s!\n", encodeStr);
			retVal = WscDevStateVarUpdate(WSC_EVENT_WLANEVENT, (char *)encodeStr, NULL);
		}
		if (encodeStr != NULL)
			free(encodeStr);
		free(pUPnPMsg);
	}
	
	return 0;
	
}


int WscEventMgmtMsgRecv(
	IN char *pBuf,
	IN int  bufLen)
{
	RTMP_WSC_MSG_HDR *pHdr = NULL;	
	
	pHdr = (RTMP_WSC_MSG_HDR *)pBuf;
	if (!pHdr)
		return -1;

	if (pHdr->msgType != WSC_OPCODE_UPNP_MGMT)
		return -1;

	DBGPRINTF(RT_DBG_INFO, "%s:Receive a MGMT event, msgSubType=%d\n", __FUNCTION__, pHdr->msgSubType);
	switch(pHdr->msgSubType)
	{
		case WSC_UPNP_MGMT_SUB_PROBE_REQ:
			WscEventMgmt_ProbreReq(pBuf, bufLen);
			break;
		case WSC_UPNP_MGMT_SUB_CONFIG_REQ:
			WscEventMgmt_ConfigReq(pBuf, bufLen);
			break;
		case WSC_UPNP_MGMT_SUB_REG_SELECT:
			WscEventMgmt_RegSelect(pBuf, bufLen);
			break;
		default:
			DBGPRINTF(RT_DBG_ERROR, "Un-Supported WSC Mgmt event type(%d)!\n", pHdr->msgSubType);
			break;
	}

	return 0;
}


/******************************************************************************
 * WscDevHandleSubscriptionReq
 *
 * Description: 
 *       Called during a subscription request callback.  If the subscription 
 *       request is for the Wsc device and match the serviceId, then accept it.
 *
 * Parameters:
 *   sr_event -- The "Upnp_Subscription_Request" structure
 *
 * Return:
 *   TRUE 
 *****************************************************************************/
int
WscDevHandleSubscriptionReq(IN struct Upnp_Subscription_Request *sr_event)
{
	// IXML_Document *PropSet=NULL;

	//lock state mutex
	pthread_mutex_lock( &WscDevMutex );

	if ((strcmp(sr_event->UDN, wscLocalDevice.UDN) == 0) &&
		(strcmp(sr_event->ServiceId, wscLocalDevice.services.ServiceId) == 0))
	{

	/*
		PropSet = NULL;

		for (j=0; j< WSC_STATE_VAR_MAXVARS; j++)
		{
			//add each variable to the property set for initial state dump
			UpnpAddToPropertySet(&PropSet, wscStateVarName[j], wscLocalDevice.services.StateVarVal[j]);
		}

		//dump initial state 
		UpnpAcceptSubscriptionExt(wscDevHandle, sr_event->UDN, sr_event->ServiceId, PropSet,sr_event->Sid);
		//free document
		ixmlDocument_free(PropSet);
	*/

		/*  Insert this Control Point into WscCPList*/
		WscDevCPNodeInsert(wscDevHandle, sr_event->UDN, sr_event->ServiceId, sr_event->Sid);
		dumpDevCPNodeList();

		/* Send the WSC event to request */
		UpnpAcceptSubscription(wscDevHandle, sr_event->UDN, sr_event->ServiceId,
								(const char **)wscStateVarName,
								(const char **)wscLocalDevice.services.StateVarVal,
								WSC_STATE_VAR_MAXVARS, sr_event->Sid);

	}

	pthread_mutex_unlock(&WscDevMutex);

	return TRUE;
}


/******************************************************************************
 * WscDevServiceHandler
 *
 * Description: 
 *       The callback handler registered with the SDK while registering
 *       root device.  Dispatches the request to the appropriate procedure
 *       based on the value of EventType. The four requests handled by the 
 *       device are: 
 *             1) Event Subscription requests.  
 *             2) Get Variable requests. 
 *             3) Action requests.
 *
 * Parameters:
 *
 *   EventType - The type of callback event
 *   Event     - Data structure containing event data
 *   Cookie    - Optional data specified during callback registration
 *
 * Return:
 *   Zero 
 *****************************************************************************/
static int WscDevServiceHandler(
	IN Upnp_EventType EventType,
	IN void *Event,
	IN void *Cookie)
{

	switch (EventType) 
	{
		case UPNP_EVENT_SUBSCRIPTION_REQUEST:
			WscDevHandleSubscriptionReq((struct Upnp_Subscription_Request *)Event);
			break;
		case UPNP_CONTROL_GET_VAR_REQUEST:
			// TODO: Shall we support this Request??
			//WscDevHandleGetVarRequest((struct Upnp_State_Var_Request * )Event);
			break;
		case UPNP_CONTROL_ACTION_REQUEST:
			WscDevHandleActionReq((struct Upnp_Action_Request *)Event);
			break;

		/* Ignore floowing cases, since this is not a UPnP Control Point */
		case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		case UPNP_DISCOVERY_SEARCH_RESULT:
		case UPNP_DISCOVERY_SEARCH_TIMEOUT:
		case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
		case UPNP_CONTROL_ACTION_COMPLETE:
		case UPNP_CONTROL_GET_VAR_COMPLETE:
		case UPNP_EVENT_RECEIVED:
		case UPNP_EVENT_RENEWAL_COMPLETE:
		case UPNP_EVENT_SUBSCRIBE_COMPLETE:
		case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
			break;

		default:
			DBGPRINTF(RT_DBG_ERROR, "Error in WscDevServiceHandler: unknown event type %d\n", EventType);
	}

	return (0);
}


/******************************************************************************
 * WscLocalDevServTbInit
 *
 * Description: 
 *     Initializes the service table for the Wsc UPnP service.
 *     Note that knowledge of the service description is assumed. 
 *
 * Paramters:
 *     None
 *
 * Return:
 *      always TRUE.
 *****************************************************************************/
static int WscLocalDevServTbInit(void)
{
	unsigned int i = 0;

	pthread_mutex_lock(&WscDevMutex);
	for (i = 0; i < WSC_STATE_VAR_MAXVARS; i++)
	{
		wscLocalDevice.services.StateVarVal[i] = wscStateVarCont[i];
		strncpy(wscLocalDevice.services.StateVarVal[i], wscStateVarDefVal[i], WSC_STATE_VAR_MAX_STR_LEN-1);
	}
	pthread_mutex_unlock(&WscDevMutex);

	wscDevActTable.actionNames[0] = "GetDeviceInfo";
	wscDevActTable.actionFuncs[0] = WscDevGetDeviceInfo;
	wscDevActTable.actionNames[1] = "PutMessage";
	wscDevActTable.actionFuncs[1] = WscDevPutMessage;
	wscDevActTable.actionNames[2] = "GetAPSettings";
	wscDevActTable.actionFuncs[2] = WscDevGetAPSettings;
	wscDevActTable.actionNames[3] = "SetAPSettings";
	wscDevActTable.actionFuncs[3] = WscDevSetAPSettings;
	wscDevActTable.actionNames[4] = "DelAPSettings";
	wscDevActTable.actionFuncs[4] = WscDevDelAPSettings;
	wscDevActTable.actionNames[5] = "GetSTASettings";
	wscDevActTable.actionFuncs[5] = WscDevGetSTASettings;
	wscDevActTable.actionNames[6] = "SetSTASettings";
	wscDevActTable.actionFuncs[6] = WscDevSetSTASettings;
	wscDevActTable.actionNames[7] = "PutWLANResponse";
	wscDevActTable.actionFuncs[7] = WscDevPutWLANResponse;
	wscDevActTable.actionNames[8] = "RebootAP";
	wscDevActTable.actionFuncs[8] = WscDevRebootAP;
	wscDevActTable.actionNames[9] = "ResetAP";
	wscDevActTable.actionFuncs[9] = WscDevResetAP;
	wscDevActTable.actionNames[10] = "RebootSTA";
	wscDevActTable.actionFuncs[10] = WscDevRebootSTA;
	wscDevActTable.actionNames[10] = "ResetSTA";
	wscDevActTable.actionFuncs[10] = WscDevResetSTA;
	wscDevActTable.actionNames[11] = "SetSelectedRegistrar";
	wscDevActTable.actionFuncs[11] = WscDevSetSelectedRegistrar;
	wscDevActTable.actionNames[12] = NULL;
	wscDevActTable.actionFuncs[12] = NULL;
		
	return TRUE;
}


/******************************************************************************
 * WscLocalDeviceInit
 *
 * Description: 
 *       Initialize the device state table for wscLocalDevice , pulling identifier info
 *       from the description Document.  Note that knowledge of the service description 
 *       is assumed.  State table variables and default values are currently hardcoded 
 *       in this file rather than being read from service description documents.
 *
 * Parameters:
 *   DescDocURL -- The description document URL
 *
 * Return:
 *    UPNP_E_SUCCESS - if success
 *    Non zero value - if failed
 *****************************************************************************/
static int WscLocalDeviceInit(IN char *DescDocURL)
{
	IXML_Document *DescDoc = NULL;
	int ret = UPNP_E_SUCCESS;
	char *udnStr = NULL, *friendlyName = NULL, *baseURL = NULL, *presURL = NULL, *base = NULL;
	char *servId = NULL, *eventURL = NULL, *ctrlURL = NULL, *scpdURL = NULL;

	memset(&wscLocalDevice, 0, sizeof(wscLocalDevice));
	
	/* Download description document */
	if (UpnpDownloadXmlDoc(DescDocURL, &DescDoc) != UPNP_E_SUCCESS) 
	{
		DBGPRINTF(RT_DBG_ERROR, "WscLocalDeviceInit -- Error Parsing %s\n", DescDocURL);
		ret = UPNP_E_INVALID_DESC;
		goto error_handler;
	}

	/* Get Wsc Device Info */
	udnStr = SampleUtil_GetFirstDocumentItem(DescDoc, "UDN");
	friendlyName = SampleUtil_GetFirstDocumentItem(DescDoc, "friendlyName");
	baseURL = SampleUtil_GetFirstDocumentItem(DescDoc, "URLBase");
	presURL = SampleUtil_GetFirstDocumentItem(DescDoc, "presentationURL");

	base = (baseURL == NULL) ? DescDocURL : baseURL;
	UpnpResolveURL(base, presURL, wscLocalDevice.PresURL);

	strncpy(wscLocalDevice.DescDocURL, DescDocURL, NAME_SIZE-1);
	if(udnStr)
		strncpy(wscLocalDevice.UDN, udnStr, NAME_SIZE-1);
	if(friendlyName)
		strncpy(wscLocalDevice.FriendlyName, friendlyName, NAME_SIZE-1);
	if(presURL)
		strncpy(wscLocalDevice.PresURL, presURL, NAME_SIZE-1);

	//This's the local device, we didn't care about the expires
	wscLocalDevice.AdvrTimeOut = -1;


	/* Get the Wsc UPnP Service identifiers */
	if (SampleUtil_FindAndParseService(DescDoc, DescDocURL, WscServiceTypeStr,
										&servId, &scpdURL, &eventURL, &ctrlURL))
	{
		strcpy(wscLocalDevice.services.ServiceId, WscServiceIDStr);
		strcpy(wscLocalDevice.services.ServiceType, WscServiceTypeStr);
	
		if(scpdURL)
			strncpy(wscLocalDevice.services.SCPDURL, scpdURL, NAME_SIZE-1);
		if(eventURL)
			strncpy(wscLocalDevice.services.EventURL, eventURL, NAME_SIZE-1);
		if(ctrlURL)
			strncpy(wscLocalDevice.services.ControlURL, ctrlURL, NAME_SIZE-1);

		//Wsc Device currently just define One Service. So set the next as NULL.
		wscLocalDevice.services.next = NULL;
		
		//Init the Wsc Device service table
		WscLocalDevServTbInit();
	}
	else 
	{
		DBGPRINTF(RT_DBG_ERROR, "WscDeviceStateTableInit -- Error: Could not find Service: %s\n", WscServiceTypeStr);
		ret = UPNP_E_INVALID_DESC;
	}

	//For debug	
	if(ret == UPNP_E_SUCCESS)
	{
		DBGPRINTF(RT_DBG_INFO, "WscLocalDevice:\n");
		DBGPRINTF(RT_DBG_INFO, "\tDevice:\n");
		DBGPRINTF(RT_DBG_INFO, "\t\tFriendlyName=%s!\n", wscLocalDevice.FriendlyName);
		DBGPRINTF(RT_DBG_INFO, "\t\tDescDocURL=%s!\n", wscLocalDevice.DescDocURL);
		DBGPRINTF(RT_DBG_INFO, "\t\tUDN=%s!\n", wscLocalDevice.UDN);
		DBGPRINTF(RT_DBG_INFO, "\t\tPresURL=%s!\n", wscLocalDevice.PresURL);
		DBGPRINTF(RT_DBG_INFO, "\tService:\n");
		DBGPRINTF(RT_DBG_INFO, "\t\tservIdStr=%s!\n", wscLocalDevice.services.ServiceId);
		DBGPRINTF(RT_DBG_INFO, "\t\tservTypeStr=%s!\n", wscLocalDevice.services.ServiceType);
		DBGPRINTF(RT_DBG_INFO, "\t\tscpdURL=%s!\n", wscLocalDevice.services.SCPDURL);
		DBGPRINTF(RT_DBG_INFO, "\t\teventURL=%s!\n", wscLocalDevice.services.EventURL);
		DBGPRINTF(RT_DBG_INFO, "\t\tcontrolURL=%s!\n", wscLocalDevice.services.ControlURL);
	}


error_handler:
	//clean up
	if (udnStr)
        free(udnStr);
	if (friendlyName)
        free(friendlyName);
	if (baseURL)
		free(baseURL);
	if (presURL)
		free(presURL);

	if (servId)
		free(servId);
	if (scpdURL)
		free(scpdURL);
	if (eventURL)
		free(eventURL);
    if (ctrlURL)
        free(ctrlURL);
	if (DescDoc)
		ixmlDocument_free(DescDoc);
	return (ret);
}


/******************************************************************************
 * WscUPnPDevStop
 *
 * Description: 
 *     Stops the device. Uninitializes the sdk. 
 *
 * Parameters:
 *     None
 * Return:
 *     TRUE 
 *****************************************************************************/
int WscUPnPDevStop(void)
{
	UpnpUnRegisterRootDevice(wscDevHandle);
	WscDevCPListRemoveAll();
	
	if (WFADeviceDescBuf.pBuf)
	{
		free(WFADeviceDescBuf.pBuf);
		WFADeviceDescBuf.bufSize = 0;
	}
	
	pthread_mutex_destroy(&wscCPListMutex);
	pthread_mutex_destroy(&WscDevMutex);
	
	return UPNP_E_SUCCESS;
}

/******************************************************************************
 * WscUPnPDevStart
 *
 * Description: 
 *      Registers the UPnP device, and sends out advertisements.
 *
 * Parameters:
 *
 *   char *ipAddr 		 - ip address to initialize the Device Service.
 *   unsigned short port - port number to initialize the Device Service.
 *   char *descDoc  	 - name of description document.
 *                   		If NULL, use default description file name. 
 *   char *webRootDir 	 - path of web directory.
 *                   		If NULL, use default PATH.
 * Return:
 *    success - WSC_SYS_SUCCESS
 *    failed  - WSC_SYS_ERROR
 *****************************************************************************/
int WscUPnPDevStart(
	IN char *ipAddr,
	IN unsigned short port,
	IN char *descDoc,
	IN char *webRootDir)
{
	int ret, strLen = 0;
	char descDocUrl[WSC_UPNP_DESC_URL_LEN] = {0};
	char udnStr[WSC_UPNP_UUID_STR_LEN]={0};

	
	pthread_mutex_init(&WscDevMutex, NULL);
	pthread_mutex_init(&wscCPListMutex, NULL);

	if (descDoc == NULL)
		descDoc = DEFAULT_DESC_FILE_NAME;

	if (webRootDir == NULL)
		webRootDir = DEFAULT_WEB_ROOT_DIR;

	DBGPRINTF(RT_DBG_INFO, "Specifying the webserver root directory -- %s\n", webRootDir);
	if ((ret = UpnpSetWebServerRootDir(webRootDir)) != UPNP_E_SUCCESS) 
	{
		DBGPRINTF(RT_DBG_ERROR, "Error specifying webserver root directory -- %s: %d\n", webRootDir, ret);
		goto failed;
	}

	memset(CfgMacStr, 0 , sizeof(CfgMacStr));
	snprintf(CfgMacStr, 18, "%02x:%02x:%02x:%02x:%02x:%02x", HostMacAddr[0], HostMacAddr[1], 
				HostMacAddr[2],HostMacAddr[3],HostMacAddr[4],HostMacAddr[5]);
	
	memset(&HostDescURL[0], 0, WSC_UPNP_DESC_URL_LEN);
#ifdef USE_XML_TEMPLATE
	memset(udnStr, 0 , WSC_UPNP_UUID_STR_LEN);
	ret = wsc_get_oid(RT_OID_WSC_UUID, &udnStr[5], &strLen);
	if(ret == 0)
	{
		memcpy(&udnStr[0], "uuid:", 5);
		DBGPRINTF(RT_DBG_INFO, "UUID Str=%s!\n", udnStr);
	} else {
		DBGPRINTF(RT_DBG_ERROR,  "Get UUID string failed -- ret=%d\n", ret);
		goto failed;
	}
	
	snprintf(descDocUrl, WSC_UPNP_DESC_URL_LEN, "%s/%s", webRootDir, descDoc);
	if (CreateDeviceDescXML(descDocUrl, &udnStr[0]) == FALSE)
	{
		fprintf(stderr, "Create Device Description xml buffer failed!\n");
		goto failed;
	}

	memset(descDocUrl, 0, WSC_UPNP_DESC_URL_LEN);
	snprintf(descDocUrl, WSC_UPNP_DESC_URL_LEN, "http://%s:%d/description.xml", ipAddr, port);
	strcpy(&HostDescURL[0], &descDocUrl[0]);
	DBGPRINTF(RT_DBG_INFO, "Registering the RootDevice\n\t with descDocUrl: %s\n", descDocUrl);
	ret = UpnpRegisterRootDevice2(UPNPREG_BUF_DESC, WFADeviceDescBuf.pBuf, 
									WFADeviceDescBuf.bufSize, TRUE, 
									WscDevServiceHandler, 
									&wscDevHandle, &wscDevHandle);
#else
	snprintf(descDocUrl, WSC_UPNP_DESC_URL_LEN, "http://%s:%d/%s", ipAddr, port, descDoc);
	strcpy(&HostDescURL[0], &descDocUrl[0]);
	ret = UpnpRegisterRootDevice(descDocUrl, WscDevServiceHandler,
								&wscDevHandle, &wscDevHandle);
#endif
    if(ret!= UPNP_E_SUCCESS)
	{
		DBGPRINTF(RT_DBG_ERROR, "Error registering the rootdevice: %d\n", ret);
		if (WFADeviceDescBuf.pBuf)
		{
			free(WFADeviceDescBuf.pBuf);
			WFADeviceDescBuf.bufSize = 0;
		}
		goto failed;
	}
	else 
	{
		DBGPRINTF(RT_DBG_INFO, "RootDevice Registered\n");
		
		WscLocalDeviceInit(descDocUrl);
		DBGPRINTF(RT_DBG_INFO, "WscLocalDeviceInit Initialized\n");
		
		//Init the ControlPoint List.
		pthread_mutex_lock(&wscCPListMutex);
		WscCPList = NULL;
		pthread_mutex_unlock(&wscCPListMutex);
		
		if ((ret = UpnpSendAdvertisement(wscDevHandle, defAdvrExpires)) != UPNP_E_SUCCESS)
		{
			DBGPRINTF(RT_DBG_ERROR, "Error sending advertisements : %d\n", ret);
			WscUPnPDevStop();
			return WSC_SYS_ERROR;
		}
		DBGPRINTF(RT_DBG_INFO, "Advertisement Sent\n");
	}
	
	return UPNP_E_SUCCESS;

failed:
	pthread_mutex_destroy(&wscCPListMutex);
	pthread_mutex_destroy(&WscDevMutex);
	
	return WSC_SYS_ERROR;
}

