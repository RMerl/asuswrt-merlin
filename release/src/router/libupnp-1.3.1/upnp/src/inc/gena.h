///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2003 Intel Corporation 
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

#ifndef _GENA_
#define _GENA_
#include "config.h"
#include "service_table.h"
#include "miniserver.h"
#include "uri.h"
#include "upnp.h"

#include <time.h>
#include "ThreadPool.h"
#include <string.h>
#include "client_table.h"
#include "httpparser.h"
#include "sock.h"

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else 
#ifndef EXTERN_C
#define EXTERN_C 
#endif
#endif

#define XML_VERSION "<?xml version='1.0' encoding='ISO-8859-1' ?>\n"
#define XML_PROPERTYSET_HEADER \
		"<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\">\n"

#define UNABLE_MEMORY "HTTP/1.1 500 Internal Server Error\r\n\r\n"
#define UNABLE_SERVICE_UNKNOWN "HTTP/1.1 404 Not Found\r\n\r\n"
#define UNABLE_SERVICE_NOT_ACCEPT \
			"HTTP/1.1 503 Service Not Available\r\n\r\n"


#define NOT_IMPLEMENTED "HTTP/1.1 501 Not Implemented\r\n\r\n"
#define BAD_REQUEST "HTTP/1.1 400 Bad Request\r\n\r\n"
#define INVALID_NT BAD_CALLBACK
#define BAD_CALLBACK "HTTP/1.1 412 Precondition Failed\r\n\r\n" 
#define HTTP_OK_CRLF "HTTP/1.1 200 OK\r\n\r\n"
#define HTTP_OK_STR "HTTP/1.1 200 OK\r\n"
#define INVALID_SID BAD_CALLBACK
#define MISSING_SID BAD_CALLBACK
#define MAX_CONTENT_LENGTH 20
#define MAX_SECONDS 10
#define MAX_EVENTS 20
#define MAX_PORT_SIZE 10
#define GENA_E_BAD_RESPONSE UPNP_E_BAD_RESPONSE
#define GENA_E_BAD_SERVICE UPNP_E_INVALID_SERVICE
#define GENA_E_SUBSCRIPTION_UNACCEPTED UPNP_E_SUBSCRIBE_UNACCEPTED
#define GENA_E_BAD_SID UPNP_E_INVALID_SID
#define GENA_E_UNSUBSCRIBE_UNACCEPTED UPNP_E_UNSUBSCRIBE_UNACCEPTED
#define GENA_E_NOTIFY_UNACCEPTED UPNP_E_NOTIFY_UNACCEPTED
#define GENA_E_NOTIFY_UNACCEPTED_REMOVE_SUB -9
#define GENA_E_BAD_HANDLE UPNP_E_INVALID_HANDLE
#define XML_ERROR -5
#define XML_SUCCESS UPNP_E_SUCCESS
#define GENA_SUCCESS UPNP_E_SUCCESS
#define CALLBACK_SUCCESS 0
#define DEFAULT_TIMEOUT 1801



extern ithread_mutex_t GlobalClientSubscribeMutex;

//Lock the subscription
#define SubscribeLock() \
	DBGONLY(UpnpPrintf(UPNP_INFO,GENA,__FILE__,__LINE__, \
	"Trying Subscribe Lock"));  \
	ithread_mutex_lock(&GlobalClientSubscribeMutex); \
	DBGONLY(UpnpPrintf(UPNP_INFO,GENA,__FILE__,__LINE__,"Subscribe Lock");)

//Unlock the subscription
#define SubscribeUnlock() \
	DBGONLY(UpnpPrintf(UPNP_INFO,GENA,__FILE__,__LINE__, \
		"Trying Subscribe UnLock")); \
	ithread_mutex_unlock(&GlobalClientSubscribeMutex); \
	DBGONLY(UpnpPrintf(UPNP_INFO,GENA,__FILE__,__LINE__,"Subscribe UnLock");)


//Structure to send NOTIFY message to all subscribed control points
typedef struct NOTIFY_THREAD_STRUCT {
  char * headers;
  DOMString propertySet;
  char * servId;
  char * UDN;
  Upnp_SID sid;
  int eventKey;
  int *reference_count;
  UpnpDevice_Handle device_handle;
} notify_thread_struct;


/************************************************************************
* Function : genaCallback									
*																	
* Parameters:														
*	IN http_parser_t *parser: represents the parse state of the request
*	IN http_message_t* request: HTTP message containing GENA request
*	INOUT SOCKINFO *info: Structure containing information about the socket
*
* Description:														
*	This is the callback function called by the miniserver to handle 
*	incoming GENA requests. 
*
* Returns: int
*	UPNP_E_SUCCESS if successful else appropriate error
***************************************************************************/
EXTERN_C void genaCallback (IN http_parser_t *parser, 
							IN http_message_t* request, 
							IN SOCKINFO *info);

/************************************************************************
* Function : genaSubscribe
*																	
* Parameters:														
*	IN UpnpClient_Handle client_handle: 
*	IN char * PublisherURL: NULL Terminated, of the form : 
*						"http://134.134.156.80:4000/RedBulb/Event"
*	INOUT int * TimeOut: requested Duration, if -1, then "infinite".
*						in the OUT case: actual Duration granted 
*						by Service, -1 for infinite
*	OUT Upnp_SID out_sid:sid of subscription, memory passed in by caller
*
* Description:														
*	This function subscribes to a PublisherURL ( also mentioned as EventURL
*	some places). It sends SUBSCRIBE http request to service processes 
*	request. Finally adds a Subscription to 
*	the clients subscription list, if service responds with OK
*
* Returns: int
*	return UPNP_E_SUCCESS if service response is OK else 
*	returns appropriate error
***************************************************************************/
CLIENTONLY(
	EXTERN_C int genaSubscribe(UpnpClient_Handle client_handle,
				char * PublisherURL,
				int * TimeOut, 
				Upnp_SID  out_sid );)


/************************************************************************
* Function : genaUnSubscribe
*																	
* Parameters:														
*	IN UpnpClient_Handle client_handle: UPnP client handle
*	IN SID in_sid: The subscription ID
*
* Description:														
*	This function unsubscribes a SID. It first validates the SID and 
*	client_handle,copies the subscription, sends UNSUBSCRIBE http request 
*	to service processes request and finally removes the subscription
*
* Returns: int
*	return UPNP_E_SUCCESS if service response is OK else 
*	returns appropriate error
***************************************************************************/
CLIENTONLY(EXTERN_C int genaUnSubscribe(UpnpClient_Handle client_handle,
		   const Upnp_SID in_sid);)

/************************************************************************
* Function : genaUnregisterClient									
*																	
* Parameters:														
*	IN UpnpClient_Handle client_handle: Handle containing all the control
*			point related information
*
* Description:														
*	This function unsubcribes all the outstanding subscriptions and cleans
*	the subscription list. This function is called when control point 
*	unregisters.
*
* Returns: int
*	return UPNP_E_SUCCESS if successful else returns appropriate error
***************************************************************************/
CLIENTONLY(EXTERN_C int genaUnregisterClient(
			UpnpClient_Handle client_handle);)

//server
/************************************************************************
* Function : genaUnregisterDevice
*																	
* Parameters:														
*	IN UpnpDevice_Handle device_handle: Handle of the root device
*
* Description:														
*	This function cleans the service table of the device. 
*
* Returns: int
*	returns UPNP_E_SUCCESS if successful else returns GENA_E_BAD_HANDLE
****************************************************************************/
DEVICEONLY(EXTERN_C int genaUnregisterDevice(
				UpnpDevice_Handle device_handle);)


/************************************************************************
* Function : genaRenewSubscription
*																	
* Parameters:														
*	IN UpnpClient_Handle client_handle: Client handle
*	IN const Upnp_SID in_sid: subscription ID
*	INOUT int * TimeOut: requested Duration, if -1, then "infinite".
*						in the OUT case: actual Duration granted 
*						by Service, -1 for infinite
*
* Description:														
*	This function renews a SID. It first validates the SID and 
*	client_handle and copies the subscription. It sends RENEW 
*	(modified SUBSCRIBE) http request to service and processes
*	the response.
*
* Returns: int
*	return UPNP_E_SUCCESS if service response is OK else 
*	returns appropriate error
***************************************************************************/
CLIENTONLY(EXTERN_C int genaRenewSubscription(
							IN UpnpClient_Handle client_handle,
							IN const Upnp_SID in_sid,
							OUT int * TimeOut);)
/****************************************************************************
*	Function :	genaNotifyAll
*
*	Parameters :
*		IN UpnpDevice_Handle device_handle : Device handle
*		IN char *UDN :	Device udn
*		IN char *servId :	Service ID
*	    IN char **VarNames : array of varible names
*	    IN char **VarValues :	array of variable values
*		IN int var_count	 :	number of variables
*
*	Description : 	This function sends a notification to all the subscribed
*	control points
*
*	Return :	int
*
*	Note : This function is similar to the genaNotifyAllExt. The only difference
*			is it takes event variable array instead of xml document.
****************************************************************************/
DEVICEONLY(EXTERN_C int genaNotifyAll(UpnpDevice_Handle device_handle,
			   char *UDN,
			   char *servId,
			   char **VarNames,
			   char **VarValues,
		    int var_count
				      );)

/****************************************************************************
*	Function :	genaNotifyAllExt
*
*	Parameters :
*			IN UpnpDevice_Handle device_handle : Device handle
*			IN char *UDN :	Device udn
*			IN char *servId :	Service ID
*           IN IXML_Document *PropSet :	XML document Event varible property set
*
*	Description : 	This function sends a notification to all the subscribed
*	control points
*
*	Return :	int
*
*	Note : This function is similar to the genaNotifyAll. the only difference
*			is it takes the document instead of event variable array
****************************************************************************/
DEVICEONLY(EXTERN_C int genaNotifyAllExt(UpnpDevice_Handle device_handle, 
		   char *UDN, char *servId,IN IXML_Document *PropSet);)

/****************************************************************************
*	Function :	genaInitNotify
*
*	Parameters :
*		   IN UpnpDevice_Handle device_handle :	Device handle
*		   IN char *UDN :	Device udn
*		   IN char *servId :	Service ID
*		   IN char **VarNames :	Array of variable names
*		   IN char **VarValues :	Array of variable values
*		   IN int var_count :	array size
*		   IN Upnp_SID sid :	subscription ID
*
*	Description :	This function sends the intial state table dump to 
*		newly subscribed control point. 
*
*	Return :	int
*		returns GENA_E_SUCCESS if successful else returns appropriate error
* 
*	Note : No other event will be sent to this control point before the 
*			intial state table dump.
****************************************************************************/
DEVICEONLY(EXTERN_C int genaInitNotify(IN UpnpDevice_Handle device_handle,
			    IN char *UDN,
			    IN char *servId,
			    IN char **VarNames,
			    IN char **VarValues,
				IN int var_count,
				IN Upnp_SID sid);)

/****************************************************************************
*	Function :	genaInitNotifyExt
*
*	Parameters :
*		   IN UpnpDevice_Handle device_handle :	Device handle
*		   IN char *UDN :	Device udn
*		   IN char *servId :	Service ID
*		   IN IXML_Document *PropSet :	Document of the state table
*		   IN Upnp_SID sid :	subscription ID
*
*	Description :	This function is similar to the genaInitNofity. The only 
*	difference is that it takes the xml document for the state table and 
*	sends the intial state table dump to newly subscribed control point. 
*
*	Return :	int
*		returns GENA_E_SUCCESS if successful else returns appropriate error
* 
*	Note : No other event will be sent to this control point before the 
*			intial state table dump.
****************************************************************************/
DEVICEONLY(EXTERN_C  int genaInitNotifyExt(
		   IN UpnpDevice_Handle device_handle, 
		   IN char *UDN, 
		   IN char *servId,
		   IN IXML_Document *PropSet, 
		   IN Upnp_SID sid);)


/************************************************************************
* Function : error_respond									
*																	
* Parameters:														
*	IN SOCKINFO *info: Structure containing information about the socket
*	IN int error_code: error code that will be in the GENA response
*	IN http_message_t* hmsg: GENA request Packet 
*
* Description:														
*	This function send an error message to the control point in the case
*	incorrect GENA requests.
*
* Returns: int
*	UPNP_E_SUCCESS if successful else appropriate error
***************************************************************************/
void error_respond( IN SOCKINFO *info, IN int error_code,
				    IN http_message_t* hmsg );


#endif // GENA









