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

#include "config.h"
#if EXCLUDE_GENA == 0
#ifdef INCLUDE_DEVICE_APIS

#include "gena.h"
#include "sysdep.h"
#include "uuid.h"
#include "upnpapi.h"
#include "parsetools.h"
#include "statcodes.h"
#include "httpparser.h"
#include "httpreadwrite.h"
#include "ssdplib.h"

#include "unixutil.h"

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
int
genaUnregisterDevice( IN UpnpDevice_Handle device_handle )
{
    struct Handle_Info *handle_info;

    HandleLock(  );
    if( GetHandleInfo( device_handle, &handle_info ) != HND_DEVICE ) {

        DBGONLY( UpnpPrintf( UPNP_CRITICAL, GENA, __FILE__, __LINE__,
                             "genaUnregisterDevice : BAD Handle : %d\n",
                             device_handle ) );

        HandleUnlock(  );
        return GENA_E_BAD_HANDLE;
    }

    freeServiceTable( &handle_info->ServiceTable );
    HandleUnlock(  );

    return UPNP_E_SUCCESS;
}

/************************************************************************
* Function : GeneratePropertySet
*																	
* Parameters:														
*	IN char **names : Array of variable names (go in the event notify)
*	IN char ** values : Array of variable values (go in the event notify)
*   IN int count : number of variables
*   OUT DOMString *out: PropertySet node in the string format
*
* Description:														
*	This function to generate XML propery Set for notifications				
*
* Returns: int
*	returns UPNP_E_SUCCESS if successful else returns GENA_E_BAD_HANDLE
*
* Note: XML_VERSION comment is NOT sent due to interop issues with other 
*		UPnP vendors
****************************************************************************/
static int
GeneratePropertySet( IN char **names,
                     IN char **values,
                     IN int count,
                     OUT DOMString * out )
{
    char *buffer;
    int counter = 0;
    int size = 0;
    int temp_counter = 0;

    //size+=strlen(XML_VERSION);  the XML_VERSION is not interopeable with 
    //other vendors
    size += strlen( XML_PROPERTYSET_HEADER );
    size += strlen( "</e:propertyset>\n\n" );

    for( temp_counter = 0, counter = 0; counter < count; counter++ ) {
        size += strlen( "<e:property>\n</e:property>\n" );
        size +=
            ( 2 * strlen( names[counter] ) + strlen( values[counter] ) +
              ( strlen( "<></>\n" ) ) );

    }
    buffer = ( char * )malloc( size + 1 );

    if( buffer == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }
    memset( buffer, 0, size + 1 );

    //strcpy(buffer,XML_VERSION);  the XML_VERSION is not interopeable with 
    //other vendors
    strcpy( buffer, XML_PROPERTYSET_HEADER );

    for( counter = 0; counter < count; counter++ ) {
        strcat( buffer, "<e:property>\n" );
        sprintf( &buffer[strlen( buffer )],
                 "<%s>%s</%s>\n</e:property>\n", names[counter],
                 values[counter], names[counter] );
    }
    strcat( buffer, "</e:propertyset>\n\n" );
    ( *out ) = ixmlCloneDOMString( buffer );
    free( buffer );
    return XML_SUCCESS;
}

/************************************************************************
* Function : free_notify_struct
*																	
* Parameters:														
*	IN notify_thread_struct * input : Notify structure
*
* Description:														
*	This function frees memory used in notify_threads if the reference 
*	count is 0 otherwise decrements the refrence count
*
* Returns: VOID
*	
****************************************************************************/
static void
free_notify_struct( IN notify_thread_struct * input )
{
    ( *input->reference_count )--;
    if( ( *input->reference_count ) == 0 ) {
        free( input->headers );
        ixmlFreeDOMString( input->propertySet );
        free( input->servId );
        free( input->UDN );
        free( input->reference_count );
    }
    free( input );
}

/****************************************************************************
*	Function :	notify_send_and_recv
*
*	Parameters :
*		IN uri_type* destination_url : subscription callback URL 
*										(URL of the control point)
*		IN membuffer* mid_msg :	Common HTTP headers 
*		IN char* propertySet :	The evented XML 
*		OUT http_parser_t* response : The response from the control point.
*
*	Description :	This function sends the notify message and returns a 
*					reply.
*
*	Return : int
*		on success: returns UPNP_E_SUCCESS; else returns a UPNP error
*
*	Note : called by genaNotify
****************************************************************************/
static XINLINE int
notify_send_and_recv( IN uri_type * destination_url,
                      IN membuffer * mid_msg,
                      IN char *propertySet,
                      OUT http_parser_t * response )
{
    uri_type url;
    int conn_fd;
    membuffer start_msg;
    int ret_code;
    int err_code;
    int timeout;
    SOCKINFO info;

    // connect
    DBGONLY( UpnpPrintf( UPNP_ALL, GENA, __FILE__, __LINE__,
                         "gena notify to: %.*s\n",
                         destination_url->hostport.text.size,
                         destination_url->hostport.text.buff ); )

        conn_fd = http_Connect( destination_url, &url );
    if( conn_fd < 0 ) {
        return conn_fd;         // return UPNP error
    }

    if( ( ret_code = sock_init( &info, conn_fd ) ) != 0 ) {
        sock_destroy( &info, SD_BOTH );
        return ret_code;
    }
    // make start line and HOST header
    membuffer_init( &start_msg );
    if( http_MakeMessage( &start_msg, 1, 1,
                          "q" "s",
                          HTTPMETHOD_NOTIFY, &url, mid_msg->buf ) != 0 ) {
        membuffer_destroy( &start_msg );
        sock_destroy( &info, SD_BOTH );
        return UPNP_E_OUTOF_MEMORY;
    }

    timeout = HTTP_DEFAULT_TIMEOUT;

    // send msg (note +1 for propertyset; null-terminator is also sent)
    if( ( ret_code = http_SendMessage( &info, &timeout,
                                       "bb",
                                       start_msg.buf, start_msg.length,
                                       propertySet,
                                       strlen( propertySet ) + 1 ) ) !=
        0 ) {
        membuffer_destroy( &start_msg );
        sock_destroy( &info, SD_BOTH );
        return ret_code;
    }

    if( ( ret_code = http_RecvMessage( &info, response,
                                       HTTPMETHOD_NOTIFY, &timeout,
                                       &err_code ) ) != 0 ) {
        membuffer_destroy( &start_msg );
        sock_destroy( &info, SD_BOTH );
        httpmsg_destroy( &response->msg );
        return ret_code;
    }

    sock_destroy( &info, SD_BOTH ); //should shutdown completely
    //when closing socket
    //  sock_destroy( &info,SD_RECEIVE);
    membuffer_destroy( &start_msg );

    return UPNP_E_SUCCESS;
}

/****************************************************************************
*	Function :	genaNotify
*
*	Parameters :
*		IN char *headers :	(null terminated) (includes all headers 
*							(including \r\n) except SID and SEQ)
*		IN char *propertySet :	The evented XML 
*		IN subscription* sub :	subscription to be Notified, 
*								Assumes this is valid for life of function)
*
*	Description :	Function to Notify a particular subscription of a 
*					particular event. In general the service should NOT be 
*					blocked around this call. (this may cause deadlock 
*					with a client) NOTIFY http request is sent and the 
*					reply is processed.
*
*	Return :	int
*		GENA_SUCCESS  if the event was delivered else returns appropriate 
*		error
*
*	Note :
****************************************************************************/
int
genaNotify( IN char *headers,
            IN char *propertySet,
            IN subscription * sub )
{
    int i;
    membuffer mid_msg;
    membuffer endmsg;
    uri_type *url;
    http_parser_t response;
    int return_code = -1;

    membuffer_init( &mid_msg );

    // make 'end' msg (the part that won't vary with the destination)
    endmsg.size_inc = 30;
    if( http_MakeMessage( &mid_msg, 1, 1,
                          "s" "ssc" "sdcc",
                          headers,
                          "SID: ", sub->sid,
                          "SEQ: ", sub->ToSendEventKey ) != 0 ) {
        membuffer_destroy( &mid_msg );
        return UPNP_E_OUTOF_MEMORY;
    }
    // send a notify to each url until one goes thru
    for( i = 0; i < sub->DeliveryURLs.size; i++ ) {
        url = &sub->DeliveryURLs.parsedURLs[i];

        if( ( return_code = notify_send_and_recv( url,
                                                  &mid_msg, propertySet,
                                                  &response ) ) ==
            UPNP_E_SUCCESS ) {
            break;
        }
    }

    membuffer_destroy( &mid_msg );

    if( return_code == UPNP_E_SUCCESS ) {
        if( response.msg.status_code == HTTP_OK ) {
            return_code = GENA_SUCCESS;
        } else {
            if( response.msg.status_code == HTTP_PRECONDITION_FAILED ) {
                //Invalid SID gets removed
                return_code = GENA_E_NOTIFY_UNACCEPTED_REMOVE_SUB;
            } else {
                return_code = GENA_E_NOTIFY_UNACCEPTED;
            }
        }
        httpmsg_destroy( &response.msg );
    }

    return return_code;
}

/****************************************************************************
*	Function :	genaNotifyThread
*
*	Parameters :
*			IN void * input : notify thread structure containing all the 
*								headers and property set info
*
*	Description :	Thread job to Notify a control point. It validates the
*		subscription and copies the subscription. Also make sure that 
*		events are sent in order. 
*
*	Return : void
*
*	Note : calls the genaNotify to do the actual work
****************************************************************************/
static void
genaNotifyThread( IN void *input )
{

    subscription *sub;
    service_info *service;
    subscription sub_copy;
    notify_thread_struct *in = ( notify_thread_struct * ) input;
    int return_code;
    struct Handle_Info *handle_info;
    ThreadPoolJob job;

    HandleLock(  );
    //validate context

    if( GetHandleInfo( in->device_handle, &handle_info ) != HND_DEVICE ) {
        free_notify_struct( in );
        HandleUnlock(  );
        return;
    }

    if( ( ( service = FindServiceId( &handle_info->ServiceTable,
                                     in->servId, in->UDN ) ) == NULL )
        || ( !service->active )
        || ( ( sub = GetSubscriptionSID( in->sid, service ) ) == NULL )
        || ( ( copy_subscription( sub, &sub_copy ) != HTTP_SUCCESS ) ) ) {
        free_notify_struct( in );
        HandleUnlock(  );
        return;
    }
    //If the event is out of order push it back to the job queue
    if( in->eventKey != sub->ToSendEventKey ) {

        TPJobInit( &job, ( start_routine ) genaNotifyThread, input );
        TPJobSetFreeFunction( &job, ( free_function ) free_notify_struct );
        TPJobSetPriority( &job, MED_PRIORITY );
	/* Sleep a little before creating another thread otherwise if there is
	 * a lot of notifications to send, the device will take 100% of the CPU
	 * to create threads and push them back to the job queue. */
	imillisleep(1);
        ThreadPoolAdd( &gSendThreadPool, &job, NULL );

        freeSubscription( &sub_copy );
        HandleUnlock(  );
        return;
    }

    HandleUnlock(  );

    //send the notify
    return_code = genaNotify( in->headers, in->propertySet, &sub_copy );

    freeSubscription( &sub_copy );

    HandleLock(  );

    if( GetHandleInfo( in->device_handle, &handle_info ) != HND_DEVICE ) {
        free_notify_struct( in );
        HandleUnlock(  );
        return;
    }
    //validate context
    if( ( ( service = FindServiceId( &handle_info->ServiceTable,
                                     in->servId, in->UDN ) ) == NULL )
        || ( !service->active )
        || ( ( sub = GetSubscriptionSID( in->sid, service ) ) == NULL ) ) {
        free_notify_struct( in );
        HandleUnlock(  );
        return;
    }

    sub->ToSendEventKey++;

    if( sub->ToSendEventKey < 0 )   //wrap to 1 for overflow
        sub->ToSendEventKey = 1;

    if( return_code == GENA_E_NOTIFY_UNACCEPTED_REMOVE_SUB ) {
        RemoveSubscriptionSID( in->sid, service );
    }

    free_notify_struct( in );
    HandleUnlock(  );
}

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
int
genaInitNotify( IN UpnpDevice_Handle device_handle,
                IN char *UDN,
                IN char *servId,
                IN char **VarNames,
                IN char **VarValues,
                IN int var_count,
                IN Upnp_SID sid )
{
    char *UDN_copy = NULL;
    char *servId_copy = NULL;
    char *propertySet = NULL;
    char *headers = NULL;
    subscription *sub = NULL;
    service_info *service = NULL;
    int return_code = GENA_SUCCESS;
    int headers_size;
    int *reference_count = NULL;
    struct Handle_Info *handle_info;
    ThreadPoolJob job;

    notify_thread_struct *thread_struct = NULL;

    DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                         "GENA BEGIN INITIAL NOTIFY " ) );

    reference_count = ( int * )malloc( sizeof( int ) );

    if( reference_count == NULL )
        return UPNP_E_OUTOF_MEMORY;

    ( *reference_count ) = 0;

    UDN_copy = ( char * )malloc( strlen( UDN ) + 1 );

    if( UDN_copy == NULL ) {
        free( reference_count );
        return UPNP_E_OUTOF_MEMORY;
    }
    servId_copy = ( char * )malloc( strlen( servId ) + 1 );

    if( servId_copy == NULL ) {
        free( UDN_copy );
        free( reference_count );
        return UPNP_E_OUTOF_MEMORY;
    }

    strcpy( UDN_copy, UDN );
    strcpy( servId_copy, servId );

    HandleLock(  );

    if( GetHandleInfo( device_handle, &handle_info ) != HND_DEVICE ) {
        free( UDN_copy );
        free( reference_count );
        free( servId_copy );
        HandleUnlock(  );
        return GENA_E_BAD_HANDLE;
    }

    if( ( service = FindServiceId( &handle_info->ServiceTable,
                                   servId, UDN ) ) == NULL ) {
        free( UDN_copy );
        free( reference_count );
        free( servId_copy );
        HandleUnlock(  );
        return GENA_E_BAD_SERVICE;
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                         "FOUND SERVICE IN INIT NOTFY: UDN %s, ServID: %s ",
                         UDN, servId ) );

    if( ( ( sub = GetSubscriptionSID( sid, service ) ) == NULL ) ||
        ( sub->active ) ) {
        free( UDN_copy );
        free( reference_count );
        free( servId_copy );
        HandleUnlock(  );
        return GENA_E_BAD_SID;
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                         "FOUND SUBSCRIPTION IN INIT NOTIFY: SID %s ",
                         sid ) );

    sub->active = 1;

    if( ( return_code = GeneratePropertySet( VarNames, VarValues,
                                             var_count,
                                             &propertySet ) ) !=
        XML_SUCCESS ) {
        free( UDN_copy );
        free( reference_count );
        free( servId_copy );
        HandleUnlock(  );
        return return_code;
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                         "GENERATED PROPERY SET IN INIT NOTIFY: \n'%s'\n",
                         propertySet ) );

    headers_size = strlen( "CONTENT-TYPE text/xml\r\n" ) +
        strlen( "CONTENT-LENGTH: \r\n" ) + MAX_CONTENT_LENGTH +
        strlen( "NT: upnp:event\r\n" ) +
        strlen( "NTS: upnp:propchange\r\n" ) + 1;

    headers = ( char * )malloc( headers_size );

    if( headers == NULL ) {
        ixmlFreeDOMString( propertySet );
        free( UDN_copy );
        free( servId_copy );
        free( reference_count );
        HandleUnlock(  );
        return UPNP_E_OUTOF_MEMORY;
    }

    sprintf( headers, "CONTENT-TYPE: text/xml\r\nCONTENT-LENGTH: "
             "%d\r\nNT: upnp:event\r\nNTS: upnp:propchange\r\n",
             strlen( propertySet ) + 1 );

    //schedule thread for initial notification

    thread_struct =
        ( notify_thread_struct * )
        malloc( sizeof( notify_thread_struct ) );

    if( thread_struct == NULL ) {
        return_code = UPNP_E_OUTOF_MEMORY;
    } else {
        ( *reference_count ) = 1;
        thread_struct->servId = servId_copy;
        thread_struct->UDN = UDN_copy;
        thread_struct->headers = headers;
        thread_struct->propertySet = propertySet;
        strcpy( thread_struct->sid, sid );
        thread_struct->eventKey = sub->eventKey++;
        thread_struct->reference_count = reference_count;
        thread_struct->device_handle = device_handle;

        TPJobInit( &job, ( start_routine ) genaNotifyThread,
                   thread_struct );
        TPJobSetFreeFunction( &job, ( free_routine ) free_notify_struct );
        TPJobSetPriority( &job, MED_PRIORITY );

        if( ( return_code =
              ThreadPoolAdd( &gSendThreadPool, &job, NULL ) ) != 0 ) {
            if( return_code == EOUTOFMEM ) {
                return_code = UPNP_E_OUTOF_MEMORY;
            }
        } else {
            return_code = GENA_SUCCESS;
        }
    }

    if( return_code != GENA_SUCCESS ) {

        free( reference_count );
        free( UDN_copy );
        free( servId_copy );
        free( thread_struct );
        ixmlFreeDOMString( propertySet );
        free( headers );
    }

    HandleUnlock(  );

    return return_code;
}

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
int
genaInitNotifyExt( IN UpnpDevice_Handle device_handle,
                   IN char *UDN,
                   IN char *servId,
                   IN IXML_Document * PropSet,
                   IN Upnp_SID sid )
{
    char *UDN_copy = NULL;
    char *servId_copy = NULL;
    char *headers = NULL;
    subscription *sub = NULL;
    service_info *service = NULL;
    int return_code = GENA_SUCCESS;
    int headers_size;
    int *reference_count = NULL;
    struct Handle_Info *handle_info;
    DOMString propertySet = NULL;

    ThreadPoolJob job;

    notify_thread_struct *thread_struct = NULL;

    DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                         "GENA BEGIN INITIAL NOTIFY EXT" ) );
    reference_count = ( int * )malloc( sizeof( int ) );

    if( reference_count == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    ( *reference_count ) = 0;

    UDN_copy = ( char * )malloc( strlen( UDN ) + 1 );
    if( UDN_copy == NULL ) {
        free( reference_count );
        return UPNP_E_OUTOF_MEMORY;
    }

    servId_copy = ( char * )malloc( strlen( servId ) + 1 );
    if( servId_copy == NULL ) {
        free( UDN_copy );
        free( reference_count );
        return UPNP_E_OUTOF_MEMORY;
    }

    strcpy( UDN_copy, UDN );
    strcpy( servId_copy, servId );

    HandleLock(  );

    if( GetHandleInfo( device_handle, &handle_info ) != HND_DEVICE ) {
        free( UDN_copy );
        free( reference_count );
        free( servId_copy );
        HandleUnlock(  );
        return GENA_E_BAD_HANDLE;
    }

    if( ( service = FindServiceId( &handle_info->ServiceTable,
                                   servId, UDN ) ) == NULL ) {
        free( UDN_copy );
        free( reference_count );
        free( servId_copy );
        HandleUnlock(  );
        return GENA_E_BAD_SERVICE;
    }
    DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                         "FOUND SERVICE IN INIT NOTFY EXT: UDN %s, ServID: %s\n",
                         UDN, servId ) );

    if( ( ( sub = GetSubscriptionSID( sid, service ) ) == NULL ) ||
        ( sub->active ) ) {
        free( UDN_copy );
        free( reference_count );
        free( servId_copy );
        HandleUnlock(  );
        return GENA_E_BAD_SID;
    }
    DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                         "FOUND SUBSCRIPTION IN INIT NOTIFY EXT: SID %s",
                         sid ) );

    sub->active = 1;

    propertySet = ixmlPrintDocument( PropSet );
    if( propertySet == NULL ) {
        free( UDN_copy );
        free( reference_count );
        free( servId_copy );
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                         "GENERATED PROPERY SET IN INIT EXT NOTIFY: %s",
                         propertySet ) );

    headers_size = strlen( "CONTENT-TYPE text/xml\r\n" ) +
        strlen( "CONTENT-LENGTH: \r\n" ) + MAX_CONTENT_LENGTH +
        strlen( "NT: upnp:event\r\n" ) +
        strlen( "NTS: upnp:propchange\r\n" ) + 1;

    headers = ( char * )malloc( headers_size );
    if( headers == NULL ) {
        free( UDN_copy );
        free( servId_copy );
        free( reference_count );
        ixmlFreeDOMString( propertySet );
        HandleUnlock(  );
        return UPNP_E_OUTOF_MEMORY;
    }

    sprintf( headers, "CONTENT-TYPE: text/xml\r\nCONTENT-LENGTH: "
             "%ld\r\nNT: upnp:event\r\nNTS: upnp:propchange\r\n",
             (long) strlen( propertySet ) + 1 );

    //schedule thread for initial notification

    thread_struct =
        ( notify_thread_struct * )
        malloc( sizeof( notify_thread_struct ) );

    if( thread_struct == NULL ) {
        return_code = UPNP_E_OUTOF_MEMORY;
    } else {
        ( *reference_count ) = 1;
        thread_struct->servId = servId_copy;
        thread_struct->UDN = UDN_copy;
        thread_struct->headers = headers;
        thread_struct->propertySet = propertySet;
        strcpy( thread_struct->sid, sid );
        thread_struct->eventKey = sub->eventKey++;
        thread_struct->reference_count = reference_count;
        thread_struct->device_handle = device_handle;

        TPJobInit( &job, ( start_routine ) genaNotifyThread,
                   thread_struct );
        TPJobSetFreeFunction( &job, ( free_routine ) free_notify_struct );
        TPJobSetPriority( &job, MED_PRIORITY );

        if( ( return_code =
              ThreadPoolAdd( &gSendThreadPool, &job, NULL ) ) != 0 ) {
            if( return_code == EOUTOFMEM ) {
                return_code = UPNP_E_OUTOF_MEMORY;
            }
        } else {
            return_code = GENA_SUCCESS;
        }
    }

    if( return_code != GENA_SUCCESS ) {
        ixmlFreeDOMString( propertySet );
        free( reference_count );
        free( UDN_copy );
        free( servId_copy );
        free( thread_struct );
        free( headers );
    }
    HandleUnlock(  );

    return return_code;
}

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
int
genaNotifyAllExt( IN UpnpDevice_Handle device_handle,
                  IN char *UDN,
                  IN char *servId,
                  IN IXML_Document * PropSet )
{
    char *headers = NULL;
    int headers_size;
    int return_code = GENA_SUCCESS;
    char *UDN_copy = NULL;
    char *servId_copy = NULL;
    int *reference_count = NULL;
    struct Handle_Info *handle_info;
    DOMString propertySet = NULL;
    ThreadPoolJob job;
    subscription *finger = NULL;

    notify_thread_struct *thread_struct = NULL;

    service_info *service = NULL;

    reference_count = ( int * )malloc( sizeof( int ) );

    if( reference_count == NULL )
        return UPNP_E_OUTOF_MEMORY;

    ( *reference_count = 0 );

    UDN_copy = ( char * )malloc( strlen( UDN ) + 1 );

    if( UDN_copy == NULL ) {
        free( reference_count );
        return UPNP_E_OUTOF_MEMORY;
    }

    servId_copy = ( char * )malloc( strlen( servId ) + 1 );

    if( servId_copy == NULL ) {
        free( UDN_copy );
        free( reference_count );
        return UPNP_E_OUTOF_MEMORY;
    }

    strcpy( UDN_copy, UDN );
    strcpy( servId_copy, servId );

    propertySet = ixmlPrintDocument( PropSet );
    if( propertySet == NULL ) {
        free( UDN_copy );
        free( servId_copy );
        free( reference_count );
        return UPNP_E_INVALID_PARAM;
    }

    headers_size = strlen( "CONTENT-TYPE text/xml\r\n" ) +
        strlen( "CONTENT-LENGTH: \r\n" ) + MAX_CONTENT_LENGTH +
        strlen( "NT: upnp:event\r\n" ) +
        strlen( "NTS: upnp:propchange\r\n" ) + 1;

    headers = ( char * )malloc( headers_size );
    if( headers == NULL ) {
        free( UDN_copy );
        free( servId_copy );
        ixmlFreeDOMString( propertySet );
        free( reference_count );
        return UPNP_E_OUTOF_MEMORY;
    }
    //changed to add null terminator at end of content
    //content length = (length in bytes of property set) + null char
    sprintf( headers, "CONTENT-TYPE: text/xml\r\nCONTENT-LENGTH: "
             "%ld\r\nNT: upnp:event\r\nNTS: upnp:propchange\r\n",
             (long) strlen( propertySet ) + 1 );

    HandleLock(  );

    if( GetHandleInfo( device_handle, &handle_info ) != HND_DEVICE )
        return_code = GENA_E_BAD_HANDLE;
    else {
        if( ( service = FindServiceId( &handle_info->ServiceTable,
                                       servId, UDN ) ) != NULL ) {
            finger = GetFirstSubscription( service );

            while( finger ) {
                thread_struct =
                    ( notify_thread_struct * )
                    malloc( sizeof( notify_thread_struct ) );
                if( thread_struct == NULL ) {
                    break;
                    return_code = UPNP_E_OUTOF_MEMORY;
                }

                ( *reference_count )++;
                thread_struct->reference_count = reference_count;
                thread_struct->UDN = UDN_copy;
                thread_struct->servId = servId_copy;
                thread_struct->headers = headers;
                thread_struct->propertySet = propertySet;
                strcpy( thread_struct->sid, finger->sid );
                thread_struct->eventKey = finger->eventKey++;
                thread_struct->device_handle = device_handle;
                //if overflow, wrap to 1
                if( finger->eventKey < 0 ) {
                    finger->eventKey = 1;
                }

                TPJobInit( &job, ( start_routine ) genaNotifyThread,
                           thread_struct );
                TPJobSetFreeFunction( &job,
                                      ( free_routine )
                                      free_notify_struct );
                TPJobSetPriority( &job, MED_PRIORITY );
                if( ( return_code = ThreadPoolAdd( &gSendThreadPool,
                                                   &job, NULL ) ) != 0 ) {
                    if( return_code == EOUTOFMEM ) {
                        return_code = UPNP_E_OUTOF_MEMORY;
                    }
                    break;
                }

                finger = GetNextSubscription( service, finger );
            }
        } else
            return_code = GENA_E_BAD_SERVICE;
    }

    if( ( *reference_count ) == 0 ) {
        free( reference_count );
        free( headers );
        ixmlFreeDOMString( propertySet );
        free( UDN_copy );
        free( servId_copy );
    }

    HandleUnlock(  );

    return return_code;
}

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
int
genaNotifyAll( IN UpnpDevice_Handle device_handle,
               IN char *UDN,
               IN char *servId,
               IN char **VarNames,
               IN char **VarValues,
               IN int var_count )
{
    char *headers = NULL;
    char *propertySet = NULL;
    int headers_size;
    int return_code = GENA_SUCCESS;
    char *UDN_copy = NULL;
    char *servId_copy = NULL;
    int *reference_count = NULL;
    struct Handle_Info *handle_info;
    ThreadPoolJob job;

    subscription *finger = NULL;

    notify_thread_struct *thread_struct = NULL;

    service_info *service = NULL;

    reference_count = ( int * )malloc( sizeof( int ) );

    if( reference_count == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    ( *reference_count = 0 );

    UDN_copy = ( char * )malloc( strlen( UDN ) + 1 );

    if( UDN_copy == NULL ) {
        free( reference_count );
        return UPNP_E_OUTOF_MEMORY;
    }

    servId_copy = ( char * )malloc( strlen( servId ) + 1 );
    if( servId_copy == NULL ) {
        free( UDN_copy );
        free( reference_count );
        return UPNP_E_OUTOF_MEMORY;
    }

    strcpy( UDN_copy, UDN );
    strcpy( servId_copy, servId );

    if( ( return_code = GeneratePropertySet( VarNames, VarValues,
                                             var_count,
                                             &propertySet ) ) !=
        XML_SUCCESS ) {
        free( UDN_copy );
        free( servId_copy );
        free( reference_count );
        return return_code;
    }

    headers_size = strlen( "CONTENT-TYPE text/xml\r\n" ) +
        strlen( "CONTENT-LENGTH: \r\n" ) + MAX_CONTENT_LENGTH +
        strlen( "NT: upnp:event\r\n" ) +
        strlen( "NTS: upnp:propchange\r\n" ) + 1;

    headers = ( char * )malloc( headers_size );
    if( headers == NULL ) {
        free( UDN_copy );
        free( servId_copy );
        ixmlFreeDOMString( propertySet );
        free( reference_count );
        return UPNP_E_OUTOF_MEMORY;
    }
    //changed to add null terminator at end of content
    //content length = (length in bytes of property set) + null char
    sprintf( headers, "CONTENT-TYPE: text/xml\r\nCONTENT-LENGTH: %ld\r\nNT:"
             " upnp:event\r\nNTS: upnp:propchange\r\n",
             (long) strlen( propertySet ) + 1 );

    HandleLock(  );

    if( GetHandleInfo( device_handle, &handle_info ) != HND_DEVICE ) {
        return_code = GENA_E_BAD_HANDLE;
    } else {
        if( ( service = FindServiceId( &handle_info->ServiceTable,
                                       servId, UDN ) ) != NULL ) {
            finger = GetFirstSubscription( service );

            while( finger ) {
                thread_struct =
                    ( notify_thread_struct * )
                    malloc( sizeof( notify_thread_struct ) );
                if( thread_struct == NULL ) {
                    return_code = UPNP_E_OUTOF_MEMORY;
                    break;
                }
                ( *reference_count )++;
                thread_struct->reference_count = reference_count;
                thread_struct->UDN = UDN_copy;
                thread_struct->servId = servId_copy;
                thread_struct->headers = headers;
                thread_struct->propertySet = propertySet;
                strcpy( thread_struct->sid, finger->sid );
                thread_struct->eventKey = finger->eventKey++;
                thread_struct->device_handle = device_handle;
                //if overflow, wrap to 1
                if( finger->eventKey < 0 ) {
                    finger->eventKey = 1;
                }

                TPJobInit( &job, ( start_routine ) genaNotifyThread,
                           thread_struct );
                TPJobSetFreeFunction( &job,
                                      ( free_routine )
                                      free_notify_struct );
                TPJobSetPriority( &job, MED_PRIORITY );

                if( ( return_code =
                      ThreadPoolAdd( &gSendThreadPool, &job, NULL ) )
                    != 0 ) {
                    if( return_code == EOUTOFMEM ) {
                        return_code = UPNP_E_OUTOF_MEMORY;
                        break;
                    }
                }

                finger = GetNextSubscription( service, finger );

            }
        } else {
            return_code = GENA_E_BAD_SERVICE;
        }
    }

    if( ( *reference_count ) == 0 ) {
        free( reference_count );
        free( headers );
        ixmlFreeDOMString( propertySet );
        free( UDN_copy );
        free( servId_copy );
    }
    HandleUnlock(  );

    return return_code;
}

/****************************************************************************
*	Function :	respond_ok
*
*	Parameters :
*			IN SOCKINFO *info :	socket connection of request
*			IN int time_out : accepted duration
*			IN subscription *sub : accepted subscription
*			IN http_message_t* request : http request
*
*	Description : Function to return OK message in the case 
*		of a subscription request.
*
*	Return :	static int
*		returns UPNP_E_SUCCESS if successful else returns appropriate error
*	Note :
****************************************************************************/
static int
respond_ok( IN SOCKINFO * info,
            IN int time_out,
            IN subscription * sub,
            IN http_message_t * request )
{
    int major,
      minor;
    membuffer response;
    int return_code;
    char timeout_str[100];
    int upnp_timeout = UPNP_TIMEOUT;

    http_CalcResponseVersion( request->major_version,
                              request->minor_version, &major, &minor );

    if( time_out >= 0 ) {
        sprintf( timeout_str, "TIMEOUT: Second-%d", time_out );
    } else {
        strcpy( timeout_str, "TIMEOUT: Second-infinite" );
    }

    membuffer_init( &response );
    response.size_inc = 30;
    if( http_MakeMessage( &response, major, minor,
                          "R" "D" "S" "N" "Xc" "ssc" "sc" "c",
                          HTTP_OK, 0, X_USER_AGENT,
                          "SID: ", sub->sid, timeout_str ) != 0 ) {
        membuffer_destroy( &response );
        error_respond( info, HTTP_INTERNAL_SERVER_ERROR, request );
        return UPNP_E_OUTOF_MEMORY;
    }

    return_code = http_SendMessage( info, &upnp_timeout, "b",
                                    response.buf, response.length );

    membuffer_destroy( &response );

    return return_code;
}

/****************************************************************************
*	Function :	create_url_list
*
*	Parameters :
*				IN memptr* url_list :	
*				OUT URL_list *out :	
*
*	Description :	Function to parse the Callback header value in 
*		subscription requests takes in a buffer containing URLS delimited by 
*		'<' and '>'. The entire buffer is copied into dynamic memory
*		and stored in the URL_list. Pointers to the individual urls within 
*		this buffer are allocated and stored in the URL_list. Only URLs with 
*		network addresses are considered(i.e. host:port or domain name)
*
*	Return :	int
*		if successful returns the number of URLs parsed 
*		else UPNP_E_OUTOF_MEMORY
*	Note :
****************************************************************************/
static int
create_url_list( IN memptr * url_list,
                 OUT URL_list * out )
{
    int URLcount = 0;
    int i;
    int return_code = 0;
    uri_type temp;
    token urls;
    token *URLS;

    urls.buff = url_list->buf;
    urls.size = url_list->length;
    URLS = &urls;

    out->size = 0;
    out->URLs = NULL;
    out->parsedURLs = NULL;

    for( i = 0; i < URLS->size; i++ ) {
        if( ( URLS->buff[i] == '<' ) && ( i + 1 < URLS->size ) ) {
            if( ( ( return_code = parse_uri( &URLS->buff[i + 1],
                                             URLS->size - i + 1,
                                             &temp ) ) == HTTP_SUCCESS )
                && ( temp.hostport.text.size != 0 ) ) {
                URLcount++;
            } else {
                if( return_code == UPNP_E_OUTOF_MEMORY ) {
                    return return_code;
                }
            }
        }
    }

    if( URLcount > 0 ) {
        out->URLs = ( char * )malloc( URLS->size + 1 );
        out->parsedURLs =
            ( uri_type * ) malloc( sizeof( uri_type ) * URLcount );
        if( ( out->URLs == NULL ) || ( out->parsedURLs == NULL ) ) {
            free( out->URLs );
            free( out->parsedURLs );
            out->URLs = NULL;
            out->parsedURLs = NULL;
            return UPNP_E_OUTOF_MEMORY;
        }
        memcpy( out->URLs, URLS->buff, URLS->size );
        out->URLs[URLS->size] = 0;
        URLcount = 0;
        for( i = 0; i < URLS->size; i++ ) {
            if( ( URLS->buff[i] == '<' ) && ( i + 1 < URLS->size ) ) {
                if( ( ( return_code =
                        parse_uri( &out->URLs[i + 1], URLS->size - i + 1,
                                   &out->parsedURLs[URLcount] ) ) ==
                      HTTP_SUCCESS )
                    && ( out->parsedURLs[URLcount].hostport.text.size !=
                         0 ) ) {
                    URLcount++;
                } else {
                    if( return_code == UPNP_E_OUTOF_MEMORY ) {
                        free( out->URLs );
                        free( out->parsedURLs );
                        out->URLs = NULL;
                        out->parsedURLs = NULL;
                        return return_code;
                    }
                }
            }
        }
    }
    out->size = URLcount;

    return URLcount;
}

/****************************************************************************
*	Function :	gena_process_subscription_request
*
*	Parameters :
*			IN SOCKINFO *info :	socket info of the device 
*			IN http_message_t* request : SUBSCRIPTION request from the control
*										point
*
*	Description :	This function handles a subscription request from a 
*		ctrl point. The socket is not closed on return.
*
*	Return :	void
*
*	Note :
****************************************************************************/
void
gena_process_subscription_request( IN SOCKINFO * info,
                                   IN http_message_t * request )
{
    Upnp_SID temp_sid;
    int return_code = 1;
    int time_out = 1801;
    service_info *service;
    struct Upnp_Subscription_Request request_struct;
    subscription *sub;
    uuid_upnp uid;
    struct Handle_Info *handle_info;
    void *cookie;
    Upnp_FunPtr callback_fun;
    UpnpDevice_Handle device_handle;
    memptr nt_hdr;
    char *event_url_path = NULL;
    memptr callback_hdr;
    memptr timeout_hdr;

    DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                         "Subscription Request Received:\n" ) );

    if( httpmsg_find_hdr( request, HDR_NT, &nt_hdr ) == NULL ) {
        error_respond( info, HTTP_BAD_REQUEST, request );
        return;
    }

    // check NT header
    //Windows Millenium Interoperability:
    //we accept either upnp:event, or upnp:propchange for the NT header
    if( memptr_cmp_nocase( &nt_hdr, "upnp:event" ) != 0 ) {
        error_respond( info, HTTP_PRECONDITION_FAILED, request );
        return;
    }

    // if a SID is present then the we have a bad request
    //  "incompatible headers"
    if( httpmsg_find_hdr( request, HDR_SID, NULL ) != NULL ) {
        error_respond( info, HTTP_BAD_REQUEST, request );
        return;
    }
    //look up service by eventURL
    if( ( event_url_path = str_alloc( request->uri.pathquery.buff,
                                      request->uri.pathquery.size ) ) ==
        NULL ) {
        error_respond( info, HTTP_INTERNAL_SERVER_ERROR, request );
        return;
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                         "SubscriptionRequest for event URL path: %s\n",
                         event_url_path );
         )

        HandleLock(  );

    // CURRENTLY, ONLY ONE DEVICE
    if( GetDeviceHandleInfo( &device_handle, &handle_info ) != HND_DEVICE ) {
        free( event_url_path );
        error_respond( info, HTTP_INTERNAL_SERVER_ERROR, request );
        HandleUnlock(  );
        return;
    }
    service = FindServiceEventURLPath( &handle_info->ServiceTable,
                                       event_url_path );
    free( event_url_path );

    if( service == NULL || !service->active ) {
        error_respond( info, HTTP_NOT_FOUND, request );
        HandleUnlock(  );
        return;
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                         "Subscription Request: Number of Subscriptions already %d\n "
                         "Max Subscriptions allowed: %d\n",
                         service->TotalSubscriptions,
                         handle_info->MaxSubscriptions ) );

    // too many subscriptions
    if( handle_info->MaxSubscriptions != -1 &&
        service->TotalSubscriptions >= handle_info->MaxSubscriptions ) {
        error_respond( info, HTTP_INTERNAL_SERVER_ERROR, request );
        HandleUnlock(  );
        return;
    }
    // generate new subscription
    sub = ( subscription * ) malloc( sizeof( subscription ) );
    if( sub == NULL ) {
        error_respond( info, HTTP_INTERNAL_SERVER_ERROR, request );
        HandleUnlock(  );
        return;
    }
    sub->eventKey = 0;
    sub->ToSendEventKey = 0;
    sub->active = 0;
    sub->next = NULL;
    sub->DeliveryURLs.size = 0;
    sub->DeliveryURLs.URLs = NULL;
    sub->DeliveryURLs.parsedURLs = NULL;

    // check for valid callbacks
    if( httpmsg_find_hdr( request, HDR_CALLBACK, &callback_hdr ) == NULL ||
        ( return_code = create_url_list( &callback_hdr,
                                         &sub->DeliveryURLs ) ) == 0 ) {
        error_respond( info, HTTP_PRECONDITION_FAILED, request );
        freeSubscriptionList( sub );
        HandleUnlock(  );
        return;
    }
    if( return_code == UPNP_E_OUTOF_MEMORY ) {
        error_respond( info, HTTP_INTERNAL_SERVER_ERROR, request );
        freeSubscriptionList( sub );
        HandleUnlock(  );
        return;
    }
    // set the timeout
    if( httpmsg_find_hdr( request, HDR_TIMEOUT, &timeout_hdr ) != NULL ) {
        if( matchstr( timeout_hdr.buf, timeout_hdr.length,
                      "%iSecond-%d%0", &time_out ) == PARSE_OK ) {
            // nothing
        } else if( memptr_cmp_nocase( &timeout_hdr, "Second-infinite" ) ==
                   0 ) {
            time_out = -1;      // infinite timeout
        } else {
            time_out = DEFAULT_TIMEOUT; // default is > 1800 seconds
        }
    }
    // replace infinite timeout with max timeout, if possible
    if( handle_info->MaxSubscriptionTimeOut != -1 ) {
        if( time_out == -1 ||
            time_out > handle_info->MaxSubscriptionTimeOut ) {
            time_out = handle_info->MaxSubscriptionTimeOut;
        }
    }
    if( time_out >= 0 ) {
        sub->expireTime = time( NULL ) + time_out;
    } else {
        sub->expireTime = 0;    // infinite time
    }

    //generate SID
    uuid_create( &uid );
    uuid_unpack( &uid, temp_sid );
    sprintf( sub->sid, "uuid:%s", temp_sid );

    // respond OK
    if( respond_ok( info, time_out, sub, request ) != UPNP_E_SUCCESS ) {
        freeSubscriptionList( sub );
        HandleUnlock(  );
        return;
    }
//+++Patch by shiang, we just accept one subscription for one remote ip/port addrss
{
    subscription *finger = service->subscriptionList;
    subscription *previous = NULL;
	uri_type *uriInfo = NULL;
	uri_type *newURIInfo = sub->DeliveryURLs.parsedURLs;

	while( finger ) {
		uriInfo = finger->DeliveryURLs.parsedURLs;
		if (!(memcmp(&newURIInfo->hostport.IPv4address, &uriInfo->hostport.IPv4address, sizeof(struct sockaddr_in))))
		{
			if( previous )
				previous->next = finger->next;
			else
				service->subscriptionList = finger->next;
			finger->next = NULL;
			freeSubscriptionList( finger );
			if(previous)
				finger = previous->next;
			else
				finger = service->subscriptionList;
			service->TotalSubscriptions--;
		} else {
			previous = finger;
			finger = finger->next;
		}
	}
}
//---Pathc by shiang, we just accept one subscription for one remote ip/port addrss

    //add to subscription list
    sub->next = service->subscriptionList;
    service->subscriptionList = sub;
    service->TotalSubscriptions++;

    //finally generate callback for init table dump
    request_struct.ServiceId = service->serviceId;
    request_struct.UDN = service->UDN;
    strcpy( ( char * )request_struct.Sid, sub->sid );

    //copy callback
    callback_fun = handle_info->Callback;
    cookie = handle_info->Cookie;

    HandleUnlock(  );

    //make call back with request struct
    //in the future should find a way of mainting
    //that the handle is not unregistered in the middle of a 
    //callback

    callback_fun( UPNP_EVENT_SUBSCRIPTION_REQUEST,
                  &request_struct, cookie );
}

/****************************************************************************
*	Function :	gena_process_subscription_renewal_request
*
*	Parameters :
*		IN SOCKINFO *info :	socket info of the device
*		IN http_message_t* request : subscription renewal request from the 
*									control point
*
*	Description :	This function handles a subscription renewal request 
*		from a ctrl point. The connection is not destroyed on return.
*
*	Return :	void
*
*	Note :
****************************************************************************/
void
gena_process_subscription_renewal_request( IN SOCKINFO * info,
                                           IN http_message_t * request )
{
    Upnp_SID sid;
    subscription *sub;
    int time_out = 1801;
    service_info *service;
    struct Handle_Info *handle_info;
    UpnpDevice_Handle device_handle;
    memptr temp_hdr;
    membuffer event_url_path;
    memptr timeout_hdr;

    // if a CALLBACK or NT header is present, then it is an error
    if( httpmsg_find_hdr( request, HDR_CALLBACK, NULL ) != NULL ||
        httpmsg_find_hdr( request, HDR_NT, NULL ) != NULL ) {
        error_respond( info, HTTP_BAD_REQUEST, request );
        return;
    }
    // get SID
    if( httpmsg_find_hdr( request, HDR_SID, &temp_hdr ) == NULL ||
        temp_hdr.length > SID_SIZE ) {
        error_respond( info, HTTP_PRECONDITION_FAILED, request );
        return;
    }
    memcpy( sid, temp_hdr.buf, temp_hdr.length );
    sid[temp_hdr.length] = '\0';

    // lookup service by eventURL
    membuffer_init( &event_url_path );
    if( membuffer_append( &event_url_path, request->uri.pathquery.buff,
                          request->uri.pathquery.size ) != 0 ) {
        error_respond( info, HTTP_INTERNAL_SERVER_ERROR, request );
        return;
    }

    HandleLock(  );

    // CURRENTLY, ONLY SUPPORT ONE DEVICE
    if( GetDeviceHandleInfo( &device_handle, &handle_info ) != HND_DEVICE ) {
        error_respond( info, HTTP_PRECONDITION_FAILED, request );
        membuffer_destroy( &event_url_path );
        return;
    }
    service = FindServiceEventURLPath( &handle_info->ServiceTable,
                                       event_url_path.buf );
    membuffer_destroy( &event_url_path );

    // get subscription
    if( service == NULL ||
        !service->active ||
        ( ( sub = GetSubscriptionSID( sid, service ) ) == NULL ) ) {
        error_respond( info, HTTP_PRECONDITION_FAILED, request );
        HandleUnlock(  );
        return;
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                         "Renew request: Number of subscriptions already: %d\n "
                         "Max Subscriptions allowed:%d\n",
                         service->TotalSubscriptions,
                         handle_info->MaxSubscriptions );
         )
        // too many subscriptions
        if( handle_info->MaxSubscriptions != -1 &&
            service->TotalSubscriptions > handle_info->MaxSubscriptions ) {
        error_respond( info, HTTP_INTERNAL_SERVER_ERROR, request );
        RemoveSubscriptionSID( sub->sid, service );
        HandleUnlock(  );
        return;
    }
    // set the timeout
    if( httpmsg_find_hdr( request, HDR_TIMEOUT, &timeout_hdr ) != NULL ) {
        if( matchstr( timeout_hdr.buf, timeout_hdr.length,
                      "%iSecond-%d%0", &time_out ) == PARSE_OK ) {

            //nothing

        } else if( memptr_cmp_nocase( &timeout_hdr, "Second-infinite" ) ==
                   0 ) {

            time_out = -1;      // inifinite timeout

        } else {
            time_out = DEFAULT_TIMEOUT; // default is > 1800 seconds

        }
    }

    // replace infinite timeout with max timeout, if possible
    if( handle_info->MaxSubscriptionTimeOut != -1 ) {
        if( time_out == -1 ||
            time_out > handle_info->MaxSubscriptionTimeOut ) {
            time_out = handle_info->MaxSubscriptionTimeOut;
        }
    }

    if( time_out == -1 ) {
        sub->expireTime = 0;
    } else {
        sub->expireTime = time( NULL ) + time_out;
    }

    if( respond_ok( info, time_out, sub, request ) != UPNP_E_SUCCESS ) {
        RemoveSubscriptionSID( sub->sid, service );
    }

    HandleUnlock(  );
}

/****************************************************************************
*	Function :	gena_process_unsubscribe_request
*
*	Parameters :
*			IN SOCKINFO *info :	socket info of the device
*			IN http_message_t* request : UNSUBSCRIBE request from the control 
*											point
*
*	Description : This function Handles a subscription cancellation request 
*		from a ctrl point. The connection is not destroyed on return.
*
*	Return :	void
*
*	Note :
****************************************************************************/
void
gena_process_unsubscribe_request( IN SOCKINFO * info,
                                  IN http_message_t * request )
{
    Upnp_SID sid;
    service_info *service;
    struct Handle_Info *handle_info;
    UpnpDevice_Handle device_handle;

    memptr temp_hdr;
    membuffer event_url_path;

    // if a CALLBACK or NT header is present, then it is an error
    if( httpmsg_find_hdr( request, HDR_CALLBACK, NULL ) != NULL ||
        httpmsg_find_hdr( request, HDR_NT, NULL ) != NULL ) {
        error_respond( info, HTTP_BAD_REQUEST, request );
        return;
    }
    // get SID
    if( httpmsg_find_hdr( request, HDR_SID, &temp_hdr ) == NULL ||
        temp_hdr.length > SID_SIZE ) {
        error_respond( info, HTTP_PRECONDITION_FAILED, request );
        return;
    }
    memcpy( sid, temp_hdr.buf, temp_hdr.length );
    sid[temp_hdr.length] = '\0';

    // lookup service by eventURL
    membuffer_init( &event_url_path );
    if( membuffer_append( &event_url_path, request->uri.pathquery.buff,
                          request->uri.pathquery.size ) != 0 ) {
        error_respond( info, HTTP_INTERNAL_SERVER_ERROR, request );
        return;
    }

    HandleLock(  );

    // CURRENTLY, ONLY SUPPORT ONE DEVICE
    if( GetDeviceHandleInfo( &device_handle, &handle_info ) != HND_DEVICE ) {
        error_respond( info, HTTP_PRECONDITION_FAILED, request );
        membuffer_destroy( &event_url_path );
        HandleUnlock(  );
        return;
    }
    service = FindServiceEventURLPath( &handle_info->ServiceTable,
                                       event_url_path.buf );
    membuffer_destroy( &event_url_path );

    // validate service
    if( service == NULL ||
        !service->active || GetSubscriptionSID( sid, service ) == NULL )
        //CheckSubscriptionSID(sid, service) == NULL )
    {
        error_respond( info, HTTP_PRECONDITION_FAILED, request );
        HandleUnlock(  );
        return;
    }

    RemoveSubscriptionSID( sid, service );
    error_respond( info, HTTP_OK, request );    // success

    HandleUnlock(  );
}

#endif // INCLUDE_DEVICE_APIS
#endif // EXCLUDE_GENA
