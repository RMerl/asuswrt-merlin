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
#ifdef INCLUDE_DEVICE_APIS
#if EXCLUDE_SSDP == 0
#include <assert.h>
#include <stdio.h>
#include "ssdplib.h"
#include "upnpapi.h"
#include "ThreadPool.h"
#include "httpparser.h"
#include "httpreadwrite.h"
#include "statcodes.h"
#include "unixutil.h"

#define MSGTYPE_SHUTDOWN		0
#define MSGTYPE_ADVERTISEMENT	1
#define MSGTYPE_REPLY			2

/************************************************************************
* Function : advertiseAndReplyThread									
*																	
* Parameters:														
*		IN void *data: Structure containing the search request
*
* Description:														
*	This function is a wrapper function to reply the search request 
*	coming from the control point.
*
* Returns: void *
*	always return NULL
***************************************************************************/
void *
advertiseAndReplyThread( IN void *data )
{
    SsdpSearchReply *arg = ( SsdpSearchReply * ) data;

    AdvertiseAndReply( 0, arg->handle,
                       arg->event.RequestType,
                       &arg->dest_addr,
                       arg->event.DeviceType,
                       arg->event.UDN,
                       arg->event.ServiceType, arg->MaxAge );
    free( arg );

    return NULL;
}

/************************************************************************
* Function : ssdp_handle_device_request									
*																	
* Parameters:														
*		IN http_message_t* hmsg: SSDP search request from the control point
*		IN struct sockaddr_in* dest_addr: The address info of control point
*
* Description:														
*	This function handles the search request. It do the sanity checks of
*	the request and then schedules a thread to send a random time reply (
*	random within maximum time given by the control point to reply).
*
* Returns: void *
*	1 if successful else appropriate error
***************************************************************************/
void
ssdp_handle_device_request( IN http_message_t * hmsg,
                            IN struct sockaddr_in *dest_addr )
{
#define MX_FUDGE_FACTOR 10

    int handle;
    struct Handle_Info *dev_info = NULL;
    memptr hdr_value;
    int mx;
    char save_char;
    SsdpEvent event;
    int ret_code;
    SsdpSearchReply *threadArg = NULL;
    ThreadPoolJob job;
    int replyTime;
    int maxAge;

    // check man hdr
    if( httpmsg_find_hdr( hmsg, HDR_MAN, &hdr_value ) == NULL ||
        memptr_cmp( &hdr_value, "\"ssdp:discover\"" ) != 0 ) {
        return;                 // bad or missing hdr
    }
    // MX header
    if( httpmsg_find_hdr( hmsg, HDR_MX, &hdr_value ) == NULL ||
        ( mx = raw_to_int( &hdr_value, 10 ) ) < 0 ) {
        return;
    }
    // ST header
    if( httpmsg_find_hdr( hmsg, HDR_ST, &hdr_value ) == NULL ) {
        return;
    }
    save_char = hdr_value.buf[hdr_value.length];
    hdr_value.buf[hdr_value.length] = '\0';
    ret_code = ssdp_request_type( hdr_value.buf, &event );
    hdr_value.buf[hdr_value.length] = save_char;    // restore
    if( ret_code == -1 ) {
        return;                 // bad ST header
    }

    HandleLock(  );
    // device info
    if( GetDeviceHandleInfo( &handle, &dev_info ) != HND_DEVICE ) {
        HandleUnlock(  );
        return;                 // no info found
    }
    maxAge = dev_info->MaxAge;
    HandleUnlock(  );

    DBGONLY( UpnpPrintf( UPNP_PACKET, API, __FILE__, __LINE__,
                         "ssdp_handle_device_request with Cmd %d SEARCH\n",
                         event.Cmd );
             UpnpPrintf( UPNP_PACKET, API, __FILE__, __LINE__,
                         "MAX-AGE     =  %d\n", maxAge );
             UpnpPrintf( UPNP_PACKET, API, __FILE__, __LINE__,
                         "MX     =  %d\n", event.Mx );
             UpnpPrintf( UPNP_PACKET, API, __FILE__, __LINE__,
                         "DeviceType   =  %s\n", event.DeviceType );
             UpnpPrintf( UPNP_PACKET, API, __FILE__, __LINE__,
                         "DeviceUuid   =  %s\n", event.UDN );
             UpnpPrintf( UPNP_PACKET, API, __FILE__, __LINE__,
                         "ServiceType =  %s\n", event.ServiceType ); )

        threadArg =
        ( SsdpSearchReply * ) malloc( sizeof( SsdpSearchReply ) );

    if( threadArg == NULL ) {
        return;
    }
    threadArg->handle = handle;
    threadArg->dest_addr = ( *dest_addr );
    threadArg->event = event;
    threadArg->MaxAge = maxAge;

    TPJobInit( &job, advertiseAndReplyThread, threadArg );
    TPJobSetFreeFunction( &job, ( free_routine ) free );

    //Subtract a percentage from the mx
    //to allow for network and processing delays
    // (i.e. if search is for 30 seconds, 
    //       respond withing 0 - 27 seconds)

    if( mx >= 2 ) {
        mx -= MAXVAL( 1, mx / MX_FUDGE_FACTOR );
    }

    if( mx < 1 ) {
        mx = 1;
    }

    replyTime = rand(  ) % mx;

    TimerThreadSchedule( &gTimerThread, replyTime, REL_SEC, &job,
                         SHORT_TERM, NULL );
}

/************************************************************************
* Function : NewRequestHandler									
*																	
* Parameters:														
*		IN struct sockaddr_in * DestAddr: Ip address, to send the reply.
*		IN int NumPacket: Number of packet to be sent.
*		IN char **RqPacket:Number of packet to be sent.
*
* Description:														
*	This function works as a request handler which passes the HTTP 
*	request string to multicast channel then
*
* Returns: void *
*	1 if successful else appropriate error
***************************************************************************/
static int
NewRequestHandler( IN struct sockaddr_in *DestAddr,
                   IN int NumPacket,
                   IN char **RqPacket )
{
    int ReplySock,
      socklen = sizeof( struct sockaddr_in );
    int NumCopy,
      Index;
    unsigned long replyAddr = inet_addr( LOCAL_HOST );
    int ttl = 4;                //a/c to UPNP Spec

    ReplySock = socket( AF_INET, SOCK_DGRAM, 0 );
    if( ReplySock == UPNP_INVALID_SOCKET ) {
        DBGONLY( UpnpPrintf( UPNP_INFO, SSDP, __FILE__, __LINE__,
                             "SSDP_LIB: New Request Handler:"
                             "Error in socket operation !!!\n" ) );

        return UPNP_E_OUTOF_SOCKET;
    }

    setsockopt( ReplySock, IPPROTO_IP, IP_MULTICAST_IF,
                ( char * )&replyAddr, sizeof( replyAddr ) );
    setsockopt( ReplySock, IPPROTO_IP, IP_MULTICAST_TTL,
                ( char * )&ttl, sizeof( int ) );

    for( Index = 0; Index < NumPacket; Index++ ) {
        int rc;

        NumCopy = 0;
        while( NumCopy < NUM_COPY ) {
            DBGONLY( UpnpPrintf( UPNP_INFO, SSDP, __FILE__, __LINE__,
                                 ">>> SSDP SEND >>>\n%s\n",
                                 *( RqPacket + Index ) );
                 )
                rc = sendto( ReplySock, *( RqPacket + Index ),
                             strlen( *( RqPacket + Index ) ),
                             0, ( struct sockaddr * )DestAddr, socklen );
            imillisleep( SSDP_PAUSE );
            ++NumCopy;
        }
    }

    shutdown( ReplySock, SD_BOTH );
    UpnpCloseSocket( ReplySock );
    return UPNP_E_SUCCESS;
}

/************************************************************************
* Function : CreateServiceRequestPacket									
*																	
* Parameters:														
*	IN int msg_type : type of the message ( Search Reply, Advertisement 
*												or Shutdown )
*	IN char * nt : ssdp type
*	IN char * usn : unique service name ( go in the HTTP Header)
*	IN char * location :Location URL.
*	IN int  duration :Service duration in sec.
*	OUT char** packet :Output buffer filled with HTTP statement.
*
* Description:														
*	This function creates a HTTP request packet.  Depending 
*	on the input parameter it either creates a service advertisement
*   request or service shutdown request etc.
*
* Returns: void
*	
***************************************************************************/
void
CreateServicePacket( IN int msg_type,
                     IN char *nt,
                     IN char *usn,
                     IN char *location,
                     IN int duration,
                     OUT char **packet )
{
    int ret_code;
    char *nts;
    membuffer buf;

    //Notf=0 means service shutdown, 
    //Notf=1 means service advertisement, Notf =2 means reply   

    membuffer_init( &buf );
    buf.size_inc = 30;

    *packet = NULL;

    if( msg_type == MSGTYPE_REPLY ) {
/* -- PATCH START - Sergey 'Jin' Bostandzhyan <jin_eld at users.sourceforge.net> */
        ret_code = http_MakeMessage( &buf, 1, 1,
                                     "R" "sdc" "D" "s" "ssc" "S" "Xc" "ssc"
                                     "ssc" "c", HTTP_OK,
                                     "CACHE-CONTROL: max-age=", duration,
                                     "EXT:\r\n", "LOCATION: ", location,
                                     X_USER_AGENT,
                                     "ST: ", nt, "USN: ", usn );
/* -- PATCH END - */
        
        if( ret_code != 0 ) {
            return;
        }
    } else if( msg_type == MSGTYPE_ADVERTISEMENT ||
               msg_type == MSGTYPE_SHUTDOWN ) {
        if( msg_type == MSGTYPE_ADVERTISEMENT ) {
            nts = "ssdp:alive";
        } else                  // shutdown
        {
            nts = "ssdp:byebye";
        }

        // NOTE: The CACHE-CONTROL and LOCATION headers are not present in
        //  a shutdown msg, but are present here for MS WinMe interop.

/* -- PATCH START - Sergey 'Jin' Bostandzhyan <jin_eld at users.sourceforge.net> */
        ret_code = http_MakeMessage( &buf, 1, 1,
                                     "Q" "sssdc" "sdc" "ssc" "ssc" "ssc"
                                     "S" "Xc" "ssc" "c", HTTPMETHOD_NOTIFY, "*",
                                     1, "HOST: ", SSDP_IP, ":", SSDP_PORT,
                                     "CACHE-CONTROL: max-age=", duration,
                                     "LOCATION: ", location, "NT: ", nt,
                                     "NTS: ", nts, X_USER_AGENT, "USN: ", usn );
/* -- PATCH END - */        
        if( ret_code != 0 ) {
            return;
        }

    } else {
        assert( 0 );            // unknown msg
    }

    *packet = membuffer_detach( &buf ); // return msg

    membuffer_destroy( &buf );

    return;
}

/************************************************************************
* Function : DeviceAdvertisement									
*																	
* Parameters:														
*	IN char * DevType : type of the device
*	IN int RootDev: flag to indicate if the device is root device
*	IN char * nt : ssdp type
*	IN char * usn : unique service name
*	IN char * location :Location URL.
*	IN int  duration :Service duration in sec.
*
* Description:														
*	This function creates the device advertisement request based on 
*	the input parameter, and send it to the multicast channel.
*
* Returns: int
*	UPNP_E_SUCCESS if successful else appropriate error
***************************************************************************/
int
DeviceAdvertisement( IN char *DevType,
                     int RootDev,
                     char *Udn,
                     IN char *Location,
                     IN int Duration )
{
    struct sockaddr_in DestAddr;

    //char Mil_Nt[LINE_SIZE]
    char Mil_Usn[LINE_SIZE];
    char *msgs[3];
    int ret_code;

    DBGONLY( UpnpPrintf( UPNP_INFO, SSDP, __FILE__, __LINE__,
                         "In function SendDeviceAdvertisemenrt\n" );
         )

        DestAddr.sin_family = AF_INET;
    DestAddr.sin_addr.s_addr = inet_addr( SSDP_IP );
    DestAddr.sin_port = htons( SSDP_PORT );

    msgs[0] = NULL;
    msgs[1] = NULL;
    msgs[2] = NULL;

    //If deviceis a root device , here we need to 
    //send 3 advertisement or reply
    if( RootDev ) {
        sprintf( Mil_Usn, "%s::upnp:rootdevice", Udn );
        CreateServicePacket( MSGTYPE_ADVERTISEMENT, "upnp:rootdevice",
                             Mil_Usn, Location, Duration, &msgs[0] );
    }
    // both root and sub-devices need to send these two messages
    //

    CreateServicePacket( MSGTYPE_ADVERTISEMENT, Udn, Udn,
                         Location, Duration, &msgs[1] );

    sprintf( Mil_Usn, "%s::%s", Udn, DevType );
    CreateServicePacket( MSGTYPE_ADVERTISEMENT, DevType, Mil_Usn,
                         Location, Duration, &msgs[2] );

    // check error
    if( ( RootDev && msgs[0] == NULL ) ||
        msgs[1] == NULL || msgs[2] == NULL ) {
        free( msgs[0] );
        free( msgs[1] );
        free( msgs[2] );
        return UPNP_E_OUTOF_MEMORY;
    }
    // send packets
    if( RootDev ) {
        // send 3 msg types
        ret_code = NewRequestHandler( &DestAddr, 3, &msgs[0] );
    } else                      // sub-device
    {
        // send 2 msg types
        ret_code = NewRequestHandler( &DestAddr, 2, &msgs[1] );
    }

    // free msgs
    free( msgs[0] );
    free( msgs[1] );
    free( msgs[2] );

    return ret_code;
}

/************************************************************************
* Function : SendReply									
*																	
* Parameters:	
*	IN struct sockaddr_in * DestAddr:destination IP address.
*	IN char *DevType: Device type
*	IN int RootDev: 1 means root device 0 means embedded device.
*	IN char * Udn: Device UDN
*	IN char * Location: Location of Device description document.
*	IN int  Duration :Life time of this device.
*	IN int ByType:
*
* Description:														
*	This function creates the reply packet based on the input parameter, 
*	and send it to the client addesss given in its input parameter DestAddr.
*
* Returns: int
*	UPNP_E_SUCCESS if successful else appropriate error
***************************************************************************/
int
SendReply( IN struct sockaddr_in *DestAddr,
           IN char *DevType,
           IN int RootDev,
           IN char *Udn,
           IN char *Location,
           IN int Duration,
           IN int ByType )
{
    int ret_code;
    char *msgs[2];
    int num_msgs;
    char Mil_Usn[LINE_SIZE];
    int i;

    msgs[0] = NULL;
    msgs[1] = NULL;

    if( RootDev ) {
        // one msg for root device
        num_msgs = 1;

        sprintf( Mil_Usn, "%s::upnp:rootdevice", Udn );
        CreateServicePacket( MSGTYPE_REPLY, "upnp:rootdevice",
                             Mil_Usn, Location, Duration, &msgs[0] );
    } else {
        // two msgs for embedded devices
        num_msgs = 1;

        //NK: FIX for extra response when someone searches by udn
        if( !ByType ) {
            CreateServicePacket( MSGTYPE_REPLY, Udn, Udn, Location,
                                 Duration, &msgs[0] );
        } else {
            sprintf( Mil_Usn, "%s::%s", Udn, DevType );
            CreateServicePacket( MSGTYPE_REPLY, DevType, Mil_Usn,
                                 Location, Duration, &msgs[0] );
        }
    }

    // check error
    for( i = 0; i < num_msgs; i++ ) {
        if( msgs[i] == NULL ) {
            free( msgs[0] );
            return UPNP_E_OUTOF_MEMORY;
        }
    }

    // send msgs
    ret_code = NewRequestHandler( DestAddr, num_msgs, msgs );
    for( i = 0; i < num_msgs; i++ ) {
        if( msgs[i] != NULL )
            free( msgs[i] );
    }

    return ret_code;
}

/************************************************************************
* Function : DeviceReply									
*																	
* Parameters:	
*	IN struct sockaddr_in * DestAddr:destination IP address.
*	IN char *DevType: Device type
*	IN int RootDev: 1 means root device 0 means embedded device.
*	IN char * Udn: Device UDN
*	IN char * Location: Location of Device description document.
*	IN int  Duration :Life time of this device.

* Description:														
*	This function creates the reply packet based on the input parameter, 
*	and send it to the client address given in its input parameter DestAddr.
*
* Returns: int
*	UPNP_E_SUCCESS if successful else appropriate error
***************************************************************************/
int
DeviceReply( IN struct sockaddr_in *DestAddr,
             IN char *DevType,
             IN int RootDev,
             IN char *Udn,
             IN char *Location,
             IN int Duration )
{
    char *szReq[3],
      Mil_Nt[LINE_SIZE],
      Mil_Usn[LINE_SIZE];
    int RetVal;

    szReq[0] = NULL;
    szReq[1] = NULL;
    szReq[2] = NULL;

    // create 2 or 3 msgs

    if( RootDev ) {
        // 3 replies for root device
        strcpy( Mil_Nt, "upnp:rootdevice" );
        sprintf( Mil_Usn, "%s::upnp:rootdevice", Udn );
        CreateServicePacket( MSGTYPE_REPLY, Mil_Nt, Mil_Usn,
                             Location, Duration, &szReq[0] );
    }

    sprintf( Mil_Nt, "%s", Udn );
    sprintf( Mil_Usn, "%s", Udn );
    CreateServicePacket( MSGTYPE_REPLY, Mil_Nt, Mil_Usn,
                         Location, Duration, &szReq[1] );

    sprintf( Mil_Nt, "%s", DevType );
    sprintf( Mil_Usn, "%s::%s", Udn, DevType );
    CreateServicePacket( MSGTYPE_REPLY, Mil_Nt, Mil_Usn,
                         Location, Duration, &szReq[2] );

    // check error

    if( ( RootDev && szReq[0] == NULL ) ||
        szReq[1] == NULL || szReq[2] == NULL ) {
        free( szReq[0] );
        free( szReq[1] );
        free( szReq[2] );
        return UPNP_E_OUTOF_MEMORY;
    }
    // send replies
    if( RootDev ) {
        RetVal = NewRequestHandler( DestAddr, 3, szReq );
    } else {
        RetVal = NewRequestHandler( DestAddr, 2, &szReq[1] );
    }

    // free
    free( szReq[0] );
    free( szReq[1] );
    free( szReq[2] );

    return RetVal;
}

/************************************************************************
* Function : ServiceAdvertisement									
*																	
* Parameters:	
*	IN char * Udn: Device UDN
*	IN char *ServType: Service Type.
*	IN char * Location: Location of Device description document.
*	IN int  Duration :Life time of this device.

* Description:														
*	This function creates the advertisement packet based 
*	on the input parameter, and send it to the multicast channel.

*
* Returns: int
*	UPNP_E_SUCCESS if successful else appropriate error
***************************************************************************/
int
ServiceAdvertisement( IN char *Udn,
                      IN char *ServType,
                      IN char *Location,
                      IN int Duration )
{
    char Mil_Usn[LINE_SIZE];
    char *szReq[1];
    struct sockaddr_in DestAddr;
    int RetVal;

    DestAddr.sin_family = AF_INET;
    DestAddr.sin_addr.s_addr = inet_addr( SSDP_IP );
    DestAddr.sin_port = htons( SSDP_PORT );

    sprintf( Mil_Usn, "%s::%s", Udn, ServType );

    //CreateServiceRequestPacket(1,szReq[0],Mil_Nt,Mil_Usn,
    //Server,Location,Duration);
    CreateServicePacket( MSGTYPE_ADVERTISEMENT, ServType, Mil_Usn,
                         Location, Duration, &szReq[0] );
    if( szReq[0] == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    RetVal = NewRequestHandler( &DestAddr, 1, szReq );

    free( szReq[0] );
    return RetVal;
}

/************************************************************************
* Function : ServiceReply									
*																	
* Parameters:	
*	IN struct sockaddr_in *DestAddr:
*	IN char * Udn: Device UDN
*	IN char *ServType: Service Type.
*	IN char * Location: Location of Device description document.
*	IN int  Duration :Life time of this device.

* Description:														
*	This function creates the advertisement packet based 
*	on the input parameter, and send it to the multicast channel.

*
* Returns: int
*	UPNP_E_SUCCESS if successful else appropriate error
***************************************************************************/
int
ServiceReply( IN struct sockaddr_in *DestAddr,
              IN char *ServType,
              IN char *Udn,
              IN char *Location,
              IN int Duration )
{
    char Mil_Usn[LINE_SIZE];
    char *szReq[1];
    int RetVal;

    szReq[0] = NULL;

    sprintf( Mil_Usn, "%s::%s", Udn, ServType );

    CreateServicePacket( MSGTYPE_REPLY, ServType, Mil_Usn,
                         Location, Duration, &szReq[0] );
    if( szReq[0] == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    RetVal = NewRequestHandler( DestAddr, 1, szReq );

    free( szReq[0] );
    return RetVal;
}

/************************************************************************
* Function : ServiceShutdown									
*																	
* Parameters:	
*	IN char * Udn: Device UDN
*	IN char *ServType: Service Type.
*	IN char * Location: Location of Device description document.
*	IN int  Duration :Service duration in sec.

* Description:														
*	This function creates a HTTP service shutdown request packet 
*	and sent it to the multicast channel through RequestHandler.
*
* Returns: int
*	UPNP_E_SUCCESS if successful else appropriate error
***************************************************************************/
int
ServiceShutdown( IN char *Udn,
                 IN char *ServType,
                 IN char *Location,
                 IN int Duration )
{
    char Mil_Usn[LINE_SIZE];
    char *szReq[1];
    struct sockaddr_in DestAddr;
    int RetVal;

    DestAddr.sin_family = AF_INET;
    DestAddr.sin_addr.s_addr = inet_addr( SSDP_IP );
    DestAddr.sin_port = htons( SSDP_PORT );

    //sprintf(Mil_Nt,"%s",ServType);
    sprintf( Mil_Usn, "%s::%s", Udn, ServType );
    //CreateServiceRequestPacket(0,szReq[0],Mil_Nt,Mil_Usn,
    //Server,Location,Duration);
    CreateServicePacket( MSGTYPE_SHUTDOWN, ServType, Mil_Usn,
                         Location, Duration, &szReq[0] );
    if( szReq[0] == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }
    RetVal = NewRequestHandler( &DestAddr, 1, szReq );

    free( szReq[0] );
    return RetVal;
}

/************************************************************************
* Function : DeviceShutdown									
*																	
* Parameters:	
*	IN char *DevType: Device Type.
*	IN int RootDev:1 means root device.
*	IN char * Udn: Device UDN
*	IN char * Location: Location URL
*	IN int  Duration :Device duration in sec.
*
* Description:														
*	This function creates a HTTP device shutdown request packet 
*	and sent it to the multicast channel through RequestHandler.
*
* Returns: int
*	UPNP_E_SUCCESS if successful else appropriate error
***************************************************************************/
int
DeviceShutdown( IN char *DevType,
                IN int RootDev,
                IN char *Udn,
                IN char *_Server,
                IN char *Location,
                IN int Duration )
{
    struct sockaddr_in DestAddr;
    char *msgs[3];
    char Mil_Usn[LINE_SIZE];
    int ret_code;

    msgs[0] = NULL;
    msgs[1] = NULL;
    msgs[2] = NULL;

    DestAddr.sin_family = AF_INET;
    DestAddr.sin_addr.s_addr = inet_addr( SSDP_IP );
    DestAddr.sin_port = htons( SSDP_PORT );

    // root device has one extra msg
    if( RootDev ) {
        sprintf( Mil_Usn, "%s::upnp:rootdevice", Udn );
        CreateServicePacket( MSGTYPE_SHUTDOWN, "upnp:rootdevice",
                             Mil_Usn, Location, Duration, &msgs[0] );
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, SSDP, __FILE__, __LINE__,
                         "In function DeviceShutdown\n" ); )
        // both root and sub-devices need to send these two messages
        CreateServicePacket( MSGTYPE_SHUTDOWN, Udn, Udn,
                             Location, Duration, &msgs[1] );

    sprintf( Mil_Usn, "%s::%s", Udn, DevType );
    CreateServicePacket( MSGTYPE_SHUTDOWN, DevType, Mil_Usn,
                         Location, Duration, &msgs[2] );

    // check error
    if( ( RootDev && msgs[0] == NULL ) ||
        msgs[1] == NULL || msgs[2] == NULL ) {
        free( msgs[0] );
        free( msgs[1] );
        free( msgs[2] );
        return UPNP_E_OUTOF_MEMORY;
    }
    // send packets
    if( RootDev ) {
        // send 3 msg types
        ret_code = NewRequestHandler( &DestAddr, 3, &msgs[0] );
    } else                      // sub-device
    {
        // send 2 msg types
        ret_code = NewRequestHandler( &DestAddr, 2, &msgs[1] );
    }

    // free msgs
    free( msgs[0] );
    free( msgs[1] );
    free( msgs[2] );

    return ret_code;
}

#endif // EXCLUDE_SSDP
#endif // INCLUDE_DEVICE_APIS
