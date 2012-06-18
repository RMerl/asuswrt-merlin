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
#ifdef INCLUDE_CLIENT_APIS

#include "gena.h"
#include "sysdep.h"
#include "uuid.h"
#include "upnpapi.h"
#include "parsetools.h"
#include "statcodes.h"
#include "httpparser.h"
#include "httpreadwrite.h"

extern ithread_mutex_t GlobalClientSubscribeMutex;

/************************************************************************
* Function : GenaAutoRenewSubscription									
*																	
* Parameters:														
*	IN void *input: Thread data(upnp_timeout *) needed to send the renewal
*
* Description:														
*	This is a thread function to send the renewal just before the 
*	subscription times out.
*
* Returns: VOID
*	
***************************************************************************/
static void
GenaAutoRenewSubscription( IN void *input )
{
    upnp_timeout *event = ( upnp_timeout * ) input;
    void *cookie;
    Upnp_FunPtr callback_fun;
    struct Handle_Info *handle_info;
    struct Upnp_Event_Subscribe *sub_struct =
        ( struct Upnp_Event_Subscribe * )
        event->Event;

    int send_callback = 0;
    int eventType = 0;

    if( AUTO_RENEW_TIME == 0 ) {
        DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                             "GENA SUB EXPIRED" ) );
        sub_struct->ErrCode = UPNP_E_SUCCESS;
        send_callback = 1;
        eventType = UPNP_EVENT_SUBSCRIPTION_EXPIRED;
    } else {
        DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                             "GENA AUTO RENEW" ) );
        if( ( ( sub_struct->ErrCode = genaRenewSubscription( event->handle,
                                                             sub_struct->
                                                             Sid,
                                                             &sub_struct->
                                                             TimeOut ) ) !=
              UPNP_E_SUCCESS )
            && ( sub_struct->ErrCode != GENA_E_BAD_SID )
            && ( sub_struct->ErrCode != GENA_E_BAD_HANDLE ) ) {
            send_callback = 1;
            eventType = UPNP_EVENT_AUTORENEWAL_FAILED;
        }
    }
    if( send_callback ) {
        HandleLock(  );
        if( GetHandleInfo( event->handle, &handle_info ) != HND_CLIENT ) {
            HandleUnlock(  );
            free_upnp_timeout( event );
            return;
        }
        DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                             "HANDLE IS VALID" ) );
        callback_fun = handle_info->Callback;
        cookie = handle_info->Cookie;
        HandleUnlock(  );
        //make callback

        callback_fun( eventType, event->Event, cookie );
    }

    free_upnp_timeout( event );
}

/************************************************************************
* Function : ScheduleGenaAutoRenew									
*																	
* Parameters:														
*	IN int client_handle: Handle that also contains the subscription list
*	IN int TimeOut: The time out value of the subscription
*	IN client_subscription * sub: Subscription being renewed
*
* Description:														
*	This function schedules a job to renew the subscription just before
*	time out.
*
* Returns: int
*	return GENA_E_SUCCESS if successful else returns appropriate error
***************************************************************************/
static int
ScheduleGenaAutoRenew( IN int client_handle,
                       IN int TimeOut,
                       IN client_subscription * sub )
{
    struct Upnp_Event_Subscribe *RenewEventStruct = NULL;
    upnp_timeout *RenewEvent = NULL;
    int return_code = GENA_SUCCESS;
    ThreadPoolJob job;

    if( TimeOut == UPNP_INFINITE ) {
        return GENA_SUCCESS;
    }

    RenewEventStruct = ( struct Upnp_Event_Subscribe * )malloc( sizeof
                                                                ( struct
                                                                  Upnp_Event_Subscribe ) );

    if( RenewEventStruct == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    RenewEvent = ( upnp_timeout * ) malloc( sizeof( upnp_timeout ) );

    if( RenewEvent == NULL ) {
        free( RenewEventStruct );
        return UPNP_E_OUTOF_MEMORY;
    }
    //schedule expire event
    strcpy( RenewEventStruct->Sid, sub->sid );
    RenewEventStruct->ErrCode = UPNP_E_SUCCESS;
    strncpy( RenewEventStruct->PublisherUrl, sub->EventURL,
             NAME_SIZE - 1 );
    RenewEventStruct->TimeOut = TimeOut;

    //RenewEvent->EventType=UPNP_EVENT_SUBSCRIPTION_EXPIRE;
    RenewEvent->handle = client_handle;
    RenewEvent->Event = RenewEventStruct;

    TPJobInit( &job, ( start_routine ) GenaAutoRenewSubscription,
               RenewEvent );
    TPJobSetFreeFunction( &job, ( free_routine ) free_upnp_timeout );
    TPJobSetPriority( &job, MED_PRIORITY );

    //Schedule the job
    if( ( return_code = TimerThreadSchedule( &gTimerThread,
                                             TimeOut - AUTO_RENEW_TIME,
                                             REL_SEC, &job, SHORT_TERM,
                                             &( RenewEvent->
                                                eventId ) ) ) !=
        UPNP_E_SUCCESS ) {
        free( RenewEvent );
        free( RenewEventStruct );
        return return_code;
    }

    sub->RenewEventId = RenewEvent->eventId;
    return GENA_SUCCESS;
}

/************************************************************************
* Function : gena_unsubscribe									
*																	
* Parameters:														
*	IN char *url: Event URL of the service
*	IN char *sid: The subcription ID.
*	OUT http_parser_t* response: The UNSUBCRIBE response from the device
*
* Description:														
*	This function sends the UNSUBCRIBE gena request and recieves the 
*	response from the device and returns it as a parameter
*
* Returns: int
*	return 0 if successful else returns appropriate error
***************************************************************************/
static int
gena_unsubscribe( IN char *url,
                  IN char *sid,
                  OUT http_parser_t * response )
{
    int return_code;
    uri_type dest_url;
    membuffer request;

    // parse url
    return_code = http_FixStrUrl( url, strlen( url ), &dest_url );
    if( return_code != 0 ) {
        return return_code;
    }
    // make request msg
    membuffer_init( &request );
    request.size_inc = 30;
    return_code = http_MakeMessage( &request, 1, 1,
                                    "q" "ssc" "U" "c",
                                    HTTPMETHOD_UNSUBSCRIBE, &dest_url,
                                    "SID: ", sid );

    //Not able to make the message so destroy the existing buffer
    if( return_code != 0 ) {
        membuffer_destroy( &request );
        return return_code;
    }
    // send request and get reply
    return_code = http_RequestAndResponse( &dest_url, request.buf,
                                           request.length,
                                           HTTPMETHOD_UNSUBSCRIBE,
                                           HTTP_DEFAULT_TIMEOUT,
                                           response );

    membuffer_destroy( &request );

    if( return_code != 0 )
        httpmsg_destroy( &response->msg );

    if( return_code == 0 && response->msg.status_code != HTTP_OK ) {
        return_code = UPNP_E_UNSUBSCRIBE_UNACCEPTED;
        httpmsg_destroy( &response->msg );
    }

    return return_code;
}

/************************************************************************
* Function : gena_subscribe									
*																	
* Parameters:														
*	IN char *url: url of service to subscribe
*	INOUT int* timeout:subscription time desired (in secs)
*	IN char* renewal_sid:for renewal, this contains a currently h
*						 held subscription SID. For first time 
*						 subscription, this must be NULL
*	OUT char** sid: SID returned by the subscription or renew msg
*
* Description:														
*	This function subscribes or renew subscription
*
* Returns: int
*	return 0 if successful else returns appropriate error
***************************************************************************/
static int
gena_subscribe( IN char *url,
                INOUT int *timeout,
                IN char *renewal_sid,
                OUT char **sid )
{
    int return_code;
    memptr sid_hdr,
      timeout_hdr;
    char timeout_str[25];
    membuffer request;
    uri_type dest_url;
    http_parser_t response;

    *sid = NULL;                // init

    // request timeout to string
    if ( timeout == NULL ) {
        timeout = (int *)malloc(sizeof(int));
        if(timeout == 0) return  UPNP_E_OUTOF_MEMORY;
        sprintf( timeout_str, "%d", CP_MINIMUM_SUBSCRIPTION_TIME );
    } else if( ( *timeout > 0 )&& ( *timeout < CP_MINIMUM_SUBSCRIPTION_TIME ) ) {
        sprintf( timeout_str, "%d", CP_MINIMUM_SUBSCRIPTION_TIME );
    } else if( *timeout >= 0 ) {
        sprintf( timeout_str, "%d", *timeout );
    } else {
        strcpy( timeout_str, "infinite" );
    }

    // parse url
    return_code = http_FixStrUrl( url, strlen( url ), &dest_url );
    if( return_code != 0 ) {
        return return_code;
    }
    // make request msg
    membuffer_init( &request );
    request.size_inc = 30;
    if( renewal_sid ) {
        // renew subscription
        return_code = http_MakeMessage( &request, 1, 1,
                                        "q" "ssc" "ssc" "c",
                                        HTTPMETHOD_SUBSCRIBE, &dest_url,
                                        "SID: ", renewal_sid,
                                        "TIMEOUT: Second-", timeout_str );
    } else {
        // subscribe
        return_code = http_MakeMessage( &request, 1, 1,
                                        "q" "sssdsscc",
                                        HTTPMETHOD_SUBSCRIBE, &dest_url,
                                        "CALLBACK: <http://", LOCAL_HOST,
                                        ":", LOCAL_PORT,
                                        "/>\r\n" "NT: upnp:event\r\n"
                                        "TIMEOUT: Second-", timeout_str );
    }
    if( return_code != 0 ) {
        return return_code;
    }
    // send request and get reply
    return_code = http_RequestAndResponse( &dest_url, request.buf,
                                           request.length,
                                           HTTPMETHOD_SUBSCRIBE,
                                           HTTP_DEFAULT_TIMEOUT,
                                           &response );

    membuffer_destroy( &request );

    if( return_code != 0 ) {
        httpmsg_destroy( &response.msg );
        return return_code;
    }
    if( response.msg.status_code != HTTP_OK ) {
        httpmsg_destroy( &response.msg );
        return UPNP_E_SUBSCRIBE_UNACCEPTED;
    }
    // get SID and TIMEOUT
    if( httpmsg_find_hdr( &response.msg, HDR_SID, &sid_hdr ) == NULL ||
        sid_hdr.length == 0 ||
        httpmsg_find_hdr( &response.msg,
                          HDR_TIMEOUT, &timeout_hdr ) == NULL ||
        timeout_hdr.length == 0 ) {
        httpmsg_destroy( &response.msg );
        return UPNP_E_BAD_RESPONSE;
    }
    // save timeout
    if( matchstr( timeout_hdr.buf, timeout_hdr.length, "%iSecond-%d%0",
                  timeout ) == PARSE_OK ) {
        // nothing  
    } else if( memptr_cmp_nocase( &timeout_hdr, "Second-infinite" ) == 0 ) {
        *timeout = -1;
    } else {
        httpmsg_destroy( &response.msg );
        return UPNP_E_BAD_RESPONSE;
    }

    // save SID
    *sid = str_alloc( sid_hdr.buf, sid_hdr.length );
    if( *sid == NULL ) {
        httpmsg_destroy( &response.msg );
        return UPNP_E_OUTOF_MEMORY;
    }

    httpmsg_destroy( &response.msg );
    return UPNP_E_SUCCESS;
}

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
int
genaUnregisterClient( IN UpnpClient_Handle client_handle )
{
    client_subscription sub_copy;
    int return_code = UPNP_E_SUCCESS;
    struct Handle_Info *handle_info = NULL;
    http_parser_t response;

    while( TRUE ) {
        HandleLock(  );
        if( GetHandleInfo( client_handle, &handle_info ) != HND_CLIENT ) {
            HandleUnlock(  );
            return GENA_E_BAD_HANDLE;
        }

        if( handle_info->ClientSubList == NULL ) {
            return_code = UPNP_E_SUCCESS;
            break;
        }

        return_code = copy_client_subscription( handle_info->ClientSubList,
                                                &sub_copy );
        if( return_code != HTTP_SUCCESS ) {
            break;
        }

        RemoveClientSubClientSID( &handle_info->ClientSubList,
                                  sub_copy.sid );

        HandleUnlock(  );

        return_code = gena_unsubscribe( sub_copy.EventURL,
                                        sub_copy.ActualSID, &response );
        if( return_code == 0 ) {
            httpmsg_destroy( &response.msg );
        }

        free_client_subscription( &sub_copy );
    }

    freeClientSubList( handle_info->ClientSubList );
    HandleUnlock(  );
    return return_code;
}

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
int
genaUnSubscribe( IN UpnpClient_Handle client_handle,
                 IN const Upnp_SID in_sid )
{
    client_subscription *sub;
    int return_code = GENA_SUCCESS;
    struct Handle_Info *handle_info;
    client_subscription sub_copy;
    http_parser_t response;

    HandleLock(  );

    // validate handle and sid

    if( GetHandleInfo( client_handle, &handle_info ) != HND_CLIENT ) {
        HandleUnlock(  );
        return GENA_E_BAD_HANDLE;
    }

    if( ( sub =
          GetClientSubClientSID( handle_info->ClientSubList, in_sid ) )
        == NULL ) {
        HandleUnlock(  );
        return GENA_E_BAD_SID;
    }

    return_code = copy_client_subscription( sub, &sub_copy );

    HandleUnlock(  );

    return_code = gena_unsubscribe( sub_copy.EventURL, sub_copy.ActualSID,
                                    &response );

    if( return_code == 0 ) {
        httpmsg_destroy( &response.msg );
    }

    free_client_subscription( &sub_copy );

    HandleLock(  );

    if( GetHandleInfo( client_handle, &handle_info ) != HND_CLIENT ) {
        HandleUnlock(  );
        return GENA_E_BAD_HANDLE;
    }

    RemoveClientSubClientSID( &handle_info->ClientSubList, in_sid );

    HandleUnlock(  );

    return return_code;
}

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
int
genaSubscribe( IN UpnpClient_Handle client_handle,
               IN char *PublisherURL,
               INOUT int *TimeOut,
               OUT Upnp_SID out_sid )
{
    int return_code = GENA_SUCCESS;
    client_subscription *newSubscription = NULL;
    uuid_upnp uid;
    Upnp_SID temp_sid;
    char *ActualSID = NULL;
    struct Handle_Info *handle_info;
    char *EventURL = NULL;

    DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                         "GENA SUBSCRIBE BEGIN" ) );
    HandleLock(  );

    memset( out_sid, 0, sizeof( Upnp_SID ) );

    // validate handle
    if( GetHandleInfo( client_handle, &handle_info ) != HND_CLIENT ) {
        HandleUnlock(  );
        return GENA_E_BAD_HANDLE;
    }
    HandleUnlock(  );

    // subscribe
    SubscribeLock(  );
    return_code =
        gena_subscribe( PublisherURL, TimeOut, NULL, &ActualSID );
    HandleLock(  );
    if( return_code != UPNP_E_SUCCESS ) {
        DBGONLY( UpnpPrintf( UPNP_CRITICAL, GENA, __FILE__, __LINE__,
                             "SUBSCRIBE FAILED in transfer error code: %d returned\n",
                             return_code ) );
        goto error_handler;
    }

    if( GetHandleInfo( client_handle, &handle_info ) != HND_CLIENT ) {
        return_code = GENA_E_BAD_HANDLE;
        goto error_handler;
    }
    // generate client SID
    uuid_create( &uid );
    uuid_unpack( &uid, temp_sid );
    sprintf( out_sid, "uuid:%s", temp_sid );

    // create event url
    EventURL = ( char * )malloc( strlen( PublisherURL ) + 1 );
    if( EventURL == NULL ) {
        return_code = UPNP_E_OUTOF_MEMORY;
        goto error_handler;
    }

    strcpy( EventURL, PublisherURL );

    // fill subscription
    newSubscription =
        ( client_subscription * ) malloc( sizeof( client_subscription ) );
    if( newSubscription == NULL ) {
        return_code = UPNP_E_OUTOF_MEMORY;
        goto error_handler;
    }
    newSubscription->EventURL = EventURL;
    newSubscription->ActualSID = ActualSID;
    strcpy( newSubscription->sid, out_sid );
    newSubscription->RenewEventId = -1;
    newSubscription->next = handle_info->ClientSubList;
    handle_info->ClientSubList = newSubscription;

    // schedule expiration event
    return_code = ScheduleGenaAutoRenew( client_handle, *TimeOut,
                                         newSubscription );

  error_handler:
    if( return_code != UPNP_E_SUCCESS ) {
        free( ActualSID );
        free( EventURL );
        free( newSubscription );
    }
    HandleUnlock(  );
    SubscribeUnlock(  );
    return return_code;
}

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
int
genaRenewSubscription( IN UpnpClient_Handle client_handle,
                       IN const Upnp_SID in_sid,
                       INOUT int *TimeOut )
{
    int return_code = GENA_SUCCESS;
    client_subscription *sub;
    client_subscription sub_copy;
    struct Handle_Info *handle_info;

    char *ActualSID;
    ThreadPoolJob tempJob;

    HandleLock(  );

    // validate handle and sid
    if( GetHandleInfo( client_handle, &handle_info ) != HND_CLIENT ) {
        HandleUnlock(  );
        return GENA_E_BAD_HANDLE;
    }

    if( ( sub = GetClientSubClientSID( handle_info->ClientSubList,
                                       in_sid ) ) == NULL ) {
        HandleUnlock(  );
        return GENA_E_BAD_SID;
    }
    // remove old events
    if( TimerThreadRemove( &gTimerThread, sub->RenewEventId, &tempJob ) ==
        0 ) {

        free_upnp_timeout( ( upnp_timeout * ) tempJob.arg );
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, GENA, __FILE__, __LINE__,
                         "REMOVED AUTO RENEW  EVENT" ) );

    sub->RenewEventId = -1;
    return_code = copy_client_subscription( sub, &sub_copy );

    HandleUnlock(  );

    if( return_code != HTTP_SUCCESS ) {
        return return_code;
    }

    return_code = gena_subscribe( sub_copy.EventURL, TimeOut,
                                  sub_copy.ActualSID, &ActualSID );
    HandleLock(  );

    if( GetHandleInfo( client_handle, &handle_info ) != HND_CLIENT ) {
        HandleUnlock(  );
        if( return_code == UPNP_E_SUCCESS ) {
            free( ActualSID );
        }
        return GENA_E_BAD_HANDLE;
    }
    // we just called GetHandleInfo, so we don't check for return value
    //GetHandleInfo(client_handle, &handle_info);

    if( return_code != UPNP_E_SUCCESS ) {
        // network failure (remove client sub)
        RemoveClientSubClientSID( &handle_info->ClientSubList, in_sid );
        free_client_subscription( &sub_copy );
        HandleUnlock(  );
        return return_code;
    }
    // get subscription
    if( ( sub = GetClientSubClientSID( handle_info->ClientSubList,
                                       in_sid ) ) == NULL ) {
        free( ActualSID );
        free_client_subscription( &sub_copy );
        HandleUnlock(  );
        return GENA_E_BAD_SID;
    }
    // store actual sid
    free( sub->ActualSID );
    sub->ActualSID = ActualSID;

    // start renew subscription timer
    return_code = ScheduleGenaAutoRenew( client_handle, *TimeOut, sub );
    if( return_code != GENA_SUCCESS ) {
        RemoveClientSubClientSID( &handle_info->ClientSubList, sub->sid );
    }
    free_client_subscription( &sub_copy );
    HandleUnlock(  );
    return return_code;
}

/************************************************************************
* Function : gena_process_notification_event
*																	
* Parameters:														
*	IN SOCKINFO *info: Socket structure containing the device socket 
*					information
*	IN http_message_t* event: The http message contains the GENA 
*								notification
*
* Description:														
*	This function processes NOTIFY events that are sent by devices. 
*	called by genacallback()
*
* Returns: void
*
* Note : called by genacallback()
****************************************************************************/
void
gena_process_notification_event( IN SOCKINFO * info,
                                 IN http_message_t * event )
{
    struct Upnp_Event event_struct;
    int eventKey;
    token sid;
    client_subscription *subscription;
    IXML_Document *ChangedVars;
    struct Handle_Info *handle_info;
    void *cookie;
    Upnp_FunPtr callback;
    UpnpClient_Handle client_handle;

    memptr sid_hdr;
    memptr nt_hdr,
      nts_hdr;
    memptr seq_hdr;

    // get SID
    if( httpmsg_find_hdr( event, HDR_SID, &sid_hdr ) == NULL ) {
        error_respond( info, HTTP_PRECONDITION_FAILED, event );

        return;
    }
    sid.buff = sid_hdr.buf;
    sid.size = sid_hdr.length;

    // get event key
    if( httpmsg_find_hdr( event, HDR_SEQ, &seq_hdr ) == NULL ||
        matchstr( seq_hdr.buf, seq_hdr.length, "%d%0", &eventKey )
        != PARSE_OK ) {
        error_respond( info, HTTP_BAD_REQUEST, event );

        return;
    }
    // get NT and NTS headers
    if( httpmsg_find_hdr( event, HDR_NT, &nt_hdr ) == NULL ||
        httpmsg_find_hdr( event, HDR_NTS, &nts_hdr ) == NULL ) {
        error_respond( info, HTTP_BAD_REQUEST, event );

        return;
    }
    // verify NT and NTS headers
    if( memptr_cmp( &nt_hdr, "upnp:event" ) != 0 ||
        memptr_cmp( &nts_hdr, "upnp:propchange" ) != 0 ) {
        error_respond( info, HTTP_PRECONDITION_FAILED, event );

        return;
    }
    // parse the content (should be XML)
    if( !has_xml_content_type( event ) ||
        event->msg.length == 0 ||
        ( ixmlParseBufferEx( event->entity.buf, &ChangedVars ) ) !=
        IXML_SUCCESS ) {
        error_respond( info, HTTP_BAD_REQUEST, event );

        return;
    }

    HandleLock(  );

    // get client info
    if( GetClientHandleInfo( &client_handle, &handle_info ) != HND_CLIENT ) {
        error_respond( info, HTTP_PRECONDITION_FAILED, event );
        HandleUnlock(  );
        ixmlDocument_free( ChangedVars );

        return;
    }
    // get subscription based on SID
    if( ( subscription = GetClientSubActualSID( handle_info->ClientSubList,
                                                &sid ) ) == NULL ) {
        if( eventKey == 0 ) {
            // wait until we've finished processing a subscription 
            //   (if we are in the middle)
            // this is to avoid mistakenly rejecting the first event if we 
            //   receive it before the subscription response
            HandleUnlock(  );

            // try and get Subscription Lock 
            //   (in case we are in the process of subscribing)
            SubscribeLock(  );

            // get HandleLock again
            HandleLock(  );

            if( GetClientHandleInfo( &client_handle, &handle_info )
                != HND_CLIENT ) {
                error_respond( info, HTTP_PRECONDITION_FAILED, event );
                SubscribeUnlock(  );
                HandleUnlock(  );
                ixmlDocument_free( ChangedVars );

                return;
            }

            if( ( subscription =
                  GetClientSubActualSID( handle_info->ClientSubList,
                                         &sid ) ) == NULL ) {
                error_respond( info, HTTP_PRECONDITION_FAILED, event );
                SubscribeUnlock(  );
                HandleUnlock(  );
                ixmlDocument_free( ChangedVars );

                return;
            }

            SubscribeUnlock(  );
        } else {
            error_respond( info, HTTP_PRECONDITION_FAILED, event );
            HandleUnlock(  );
            ixmlDocument_free( ChangedVars );

            return;
        }
    }

    error_respond( info, HTTP_OK, event );  // success

    // fill event struct
    strcpy( event_struct.Sid, subscription->sid );
    event_struct.EventKey = eventKey;
    event_struct.ChangedVariables = ChangedVars;

    // copy callback
    callback = handle_info->Callback;
    cookie = handle_info->Cookie;

    HandleUnlock(  );

    // make callback with event struct
    // In future, should find a way of mainting
    // that the handle is not unregistered in the middle of a
    // callback
    callback( UPNP_EVENT_RECEIVED, &event_struct, cookie );

    ixmlDocument_free( ChangedVars );
}

#endif // INCLUDE_CLIENT_APIS
#endif // EXCLUDE_GENA
