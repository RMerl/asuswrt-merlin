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

//File upnpapi.c
#include "config.h"
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "upnpapi.h"
#include "httpreadwrite.h"
#include "ssdplib.h"
#include "soaplib.h"
#include "ThreadPool.h"
#include "membuffer.h"
#include <sys/ioctl.h>
#include <linux/if.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


#include "httpreadwrite.h"

//************************************
//Needed for GENA
#include "gena.h"
#include "service_table.h"
#include "miniserver.h"
//*******************************************

/*
 ********************* */
#ifdef INTERNAL_WEB_SERVER
#include "webserver.h"
#include "urlconfig.h"
#endif // INTERNAL_WEB_SERVER
/*
 ****************** */

//+++Add by shiang, move from upnp/src/inc/upnpapi.h
virtualDirList *pVirtualDirList;
//---Add by shiang, move from upnp/src/inc/upnpapi.h

//Mutex to synchronize the subscription handling at the client side
CLIENTONLY( ithread_mutex_t GlobalClientSubscribeMutex;
     )
    //Mutex to synchronize handles ( root device or control point handle)
     ithread_mutex_t GlobalHndMutex;

//Mutex to synchronize the uuid creation process
     ithread_mutex_t gUUIDMutex;

     TimerThread gTimerThread;

     ThreadPool gRecvThreadPool;

     ThreadPool gSendThreadPool;

//+++Add by shiang for WSC
	ThreadPool gMiniServerThreadPool;
//---Add by shiang for WSC

//Flag to indicate the state of web server
     WebServerState bWebServerState = WEB_SERVER_DISABLED;

// static buffer to store the local host ip address or host name
     char LOCAL_HOST[LINE_SIZE];

// local port for the mini-server
     unsigned short LOCAL_PORT;

// UPnP device and control point handle table 
     void *HandleTable[NUM_HANDLE];

//This structure is for virtual directory callbacks
     struct UpnpVirtualDirCallbacks virtualDirCallback;

// a local dir which serves as webserver root
     extern membuffer gDocumentRootDir;

// Maximum content-length that the SDK will process on an incoming packet. 
// Content-Length exceeding this size will be not processed and error 413 
// (HTTP Error Code) will be returned to the remote end point.
size_t g_maxContentLength = DEFAULT_SOAP_CONTENT_LENGTH; // in bytes

// Global variable to denote the state of Upnp SDK 
//    = 0 if uninitialized, = 1 if initialized.
     int UpnpSdkInit = 0;

// Global variable to denote the state of Upnp SDK device registration.
// = 0 if unregistered, = 1 if registered.
     int UpnpSdkDeviceRegistered = 0;

// Global variable to denote the state of Upnp SDK client registration.
// = 0 if unregistered, = 1 if registered.
     int UpnpSdkClientRegistered = 0;

/****************************************************************************
 * Function: UpnpInit
 *
 *  Parameters:		
 *		IN const char * HostIP: Local IP Address
 *		IN short DestPort: Local Port to listen for incoming connections
 *  Description:
 *      Initializes 
 *		 - Mutex objects, 
 *		 - Handle Table
 *		 - Thread Pool and Thread Pool Attributes
 *		 - MiniServer(starts listening for incoming requests) 
 *				and WebServer (Sends request to the 
 *		        Upper Layer after HTTP Parsing)
 *		 - Checks for IP Address passed as an argument. IF NULL, 
 *                 gets local host name
 *		 - Sets GENA and SOAP Callbacks.
 *		 - Starts the timer thread.
 *
 *  Returns:
 *      UPNP_E_SUCCESS on success, nonzero on failure.
 *      UPNP_E_INIT_FAILED if Initialization fails.
 *      UPNP_E_INIT if UPnP is already initialized
 *****************************************************************************/
     int UpnpInit( IN const char *HostIP,
                   IN unsigned short DestPort )
{
    int retVal = 0;
    ThreadPoolAttr attr;

    if( UpnpSdkInit == 1 ) {
        // already initialized
        return UPNP_E_INIT;
    }

    membuffer_init( &gDocumentRootDir );

    srand( time( NULL ) );      // needed by SSDP or other parts

    DBGONLY( if( InitLog(  ) != UPNP_E_SUCCESS )
             return UPNP_E_INIT_FAILED; );

    DBGONLY( UpnpPrintf
             ( UPNP_INFO, API, __FILE__, __LINE__, "Inside UpnpInit \n" );
         )
        //initialize mutex
        if( ithread_mutex_init( &GlobalHndMutex, NULL ) != 0 ) {
        return UPNP_E_INIT_FAILED;
    }

    if( ithread_mutex_init( &gUUIDMutex, NULL ) != 0 ) {
        return UPNP_E_INIT_FAILED;
    }
    //initialize subscribe mutex
    CLIENTONLY( if
                ( ithread_mutex_init( &GlobalClientSubscribeMutex, NULL )
                  != 0 ) {
                return UPNP_E_INIT_FAILED;}
     )

        HandleLock(  );
    if( HostIP != NULL )
        strcpy( LOCAL_HOST, HostIP );
    else {
        if( getlocalhostname( LOCAL_HOST ) != UPNP_E_SUCCESS ) {
            HandleUnlock(  );
            return UPNP_E_INIT_FAILED;
        }
    }

    if( UpnpSdkInit != 0 ) {
        HandleUnlock(  );
        return UPNP_E_INIT;
    }

    InitHandleList(  );
    HandleUnlock(  );

    TPAttrInit( &attr );
    TPAttrSetMaxThreads( &attr, MAX_THREADS );
    TPAttrSetMinThreads( &attr, MIN_THREADS );
    TPAttrSetJobsPerThread( &attr, JOBS_PER_THREAD );
    TPAttrSetIdleTime( &attr, THREAD_IDLE_TIME );

    if( ThreadPoolInit( &gSendThreadPool, &attr ) != UPNP_E_SUCCESS ) {
        UpnpSdkInit = 0;
        UpnpFinish(  );
        return UPNP_E_INIT_FAILED;
    }

    if( ThreadPoolInit( &gRecvThreadPool, &attr ) != UPNP_E_SUCCESS ) {
        UpnpSdkInit = 0;
        UpnpFinish(  );
        return UPNP_E_INIT_FAILED;
    }

//+++Patch by shiang for WSC
	if( ThreadPoolInit( &gMiniServerThreadPool, &attr ) != UPNP_E_SUCCESS ) 
	{
		UpnpSdkInit = 0;
		UpnpFinish(  );
		return UPNP_E_INIT_FAILED;
	}
//---Patch by shiang for WSC

    UpnpSdkInit = 1;
#if EXCLUDE_SOAP == 0
    DEVICEONLY( SetSoapCallback( soap_device_callback );
         );
#endif
#if EXCLUDE_GENA == 0
    SetGenaCallback( genaCallback );
#endif

    if( ( retVal = TimerThreadInit( &gTimerThread,
                                    &gSendThreadPool ) ) !=
        UPNP_E_SUCCESS ) {
        UpnpSdkInit = 0;
        UpnpFinish(  );
        return retVal;
    }
#if EXCLUDE_MINISERVER == 0
    if( ( retVal = StartMiniServer( DestPort ) ) <= 0 ) {
        DBGONLY( UpnpPrintf( UPNP_CRITICAL, API, __FILE__, __LINE__,
                             "Miniserver failed to start" );
             )
            UpnpFinish(  );
        UpnpSdkInit = 0;
        if( retVal != -1 )
            return retVal;
        else                    // if miniserver is already running for unknown reasons!
            return UPNP_E_INIT_FAILED;
    }
#endif
    DestPort = retVal;
    LOCAL_PORT = DestPort;

#if EXCLUDE_WEB_SERVER == 0
    if( ( retVal =
          UpnpEnableWebserver( WEB_SERVER_ENABLED ) ) != UPNP_E_SUCCESS ) {
        UpnpFinish(  );
        UpnpSdkInit = 0;
        return retVal;
    }
#endif

    DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                         "Host Ip: %s Host Port: %d\n", LOCAL_HOST,
                         LOCAL_PORT ) );

    DBGONLY( UpnpPrintf
             ( UPNP_INFO, API, __FILE__, __LINE__, "Exiting UpnpInit \n" );
         )

        return UPNP_E_SUCCESS;

} /***************** end of UpnpInit ******************/

DBGONLY(
static void 
PrintThreadPoolStats (const char* DbgFileName, int DbgLineNo,
		      const char* msg, const ThreadPoolStats* const stats)
{
	UpnpPrintf (UPNP_INFO, API, DbgFileName, DbgLineNo, 
		    "%s \n High Jobs pending = %d \nMed Jobs Pending = %d\n"
		    " Low Jobs Pending = %d \nWorker Threads = %d\n"
		    "Idle Threads = %d\nPersistent Threads = %d\n"
		    "Average Time spent in High Q = %lf\n"
		    "Average Time spent in Med Q = %lf\n"
		    "Average Time spent in Low Q = %lf\n"
		    "Max Threads Used: %d\nTotal Work Time= %lf\n"
		    "Total Idle Time = %lf\n",
		    msg,
		    stats->currentJobsHQ, stats->currentJobsMQ,
		    stats->currentJobsLQ, stats->workerThreads,
		    stats->idleThreads, stats->persistentThreads,
		    stats->avgWaitHQ, stats->avgWaitMQ, stats->avgWaitLQ,
		    stats->maxThreads, stats->totalWorkTime,
		    stats->totalIdleTime );
})

     
/****************************************************************************
 * Function: UpnpFinish
 *
 *  Parameters:	NONE
 *
 *  Description:
 *      Checks for pending jobs and threads 
 *		Unregisters either the client or device 
 *		Shuts down the Timer Thread
 *		Stops the Mini Server
 *		Uninitializes the Thread Pool
 *		For Win32 cleans up Winsock Interface 
 *		Cleans up mutex objects
 *
 *  Return Values:
 *      UPNP_E_SUCCESS on success, nonzero on failure.
 *****************************************************************************/
int
UpnpFinish(  )
{
    DEVICEONLY( UpnpDevice_Handle device_handle;
         )
        CLIENTONLY( UpnpClient_Handle client_handle;
         )
    struct Handle_Info *temp;

    DBGONLY( ThreadPoolStats stats;
         )

        if( UpnpSdkInit != 1 )
        return UPNP_E_FINISH;

    DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                         "Inside UpnpFinish : UpnpSdkInit is :%d:\n",
                         UpnpSdkInit ); if( UpnpSdkInit == 1 ) {
             UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                         "UpnpFinish : UpnpSdkInit is ONE\n" );}
             ThreadPoolGetStats( &gRecvThreadPool, &stats );
             PrintThreadPoolStats (__FILE__, __LINE__,
				   "Recv Thread Pool", &stats);
             ThreadPoolGetStats( &gSendThreadPool, &stats );
             PrintThreadPoolStats (__FILE__, __LINE__,
				   "Send Thread Pool", &stats);
	    )
#ifdef INCLUDE_DEVICE_APIS
        if( GetDeviceHandleInfo( &device_handle, &temp ) == HND_DEVICE )
            UpnpUnRegisterRootDevice( device_handle );
#endif

#ifdef INCLUDE_CLIENT_APIS
    if( GetClientHandleInfo( &client_handle, &temp ) == HND_CLIENT )
        UpnpUnRegisterClient( client_handle );
#endif

    TimerThreadShutdown( &gTimerThread );

    StopMiniServer(  );

#if EXCLUDE_WEB_SERVER == 0
    web_server_destroy(  );
#endif

    ThreadPoolShutdown( &gSendThreadPool );
    ThreadPoolShutdown( &gRecvThreadPool );
//+++Patch by shiang for WSC
	ThreadPoolShutdown( &gMiniServerThreadPool );
//---Patch by shiang for WSC

    DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__, "Exiting UpnpFinish : UpnpSdkInit is :%d:\n", UpnpSdkInit ); ThreadPoolGetStats( &gRecvThreadPool, &stats ); PrintThreadPoolStats ( __FILE__, __LINE__, "Recv Thread Pool", &stats); ThreadPoolGetStats( &gSendThreadPool, &stats ); PrintThreadPoolStats (__FILE__, __LINE__, "Send Thread Pool", &stats); )   // DBGONLY
        DBGONLY( CloseLog(  );
         );

    CLIENTONLY( ithread_mutex_destroy( &GlobalClientSubscribeMutex );
         )

        ithread_mutex_destroy( &GlobalHndMutex );
    ithread_mutex_destroy( &gUUIDMutex );

    // remove all virtual dirs
    UpnpRemoveAllVirtualDirs(  );

    UpnpSdkInit = 0;

    return UPNP_E_SUCCESS;

}  /********************* End of  UpnpFinish  *************************/

/****************************************************************************
 * Function: UpnpGetServerPort
 *
 *  Parameters:	NONE
 *
 *  Description:
 *      Gives back the miniserver port.
 *
 *  Return Values:
 *      local port on success, zero on failure.
 *****************************************************************************/
unsigned short
UpnpGetServerPort( void )
{

    if( UpnpSdkInit != 1 )
        return 0;

    return LOCAL_PORT;
}

/***************************************************************************
 * Function: UpnpGetServerIpAddress
 *
 *  Parameters:	NONE
 *
 *  Description:
 *      Gives back the local ipaddress.
 *
 *  Return Values: char *
 *      return the IP address string on success else NULL of failure
 ***************************************************************************/
char *
UpnpGetServerIpAddress( void )
{

    if( UpnpSdkInit != 1 )
        return NULL;

    return LOCAL_HOST;
}

#ifdef INCLUDE_DEVICE_APIS
#if 0

/****************************************************************************
 * Function: UpnpAddRootDevice
 *
 *  Parameters:	
 *		IN const char *DescURL: Location of the root device 
 *								description xml file
 *		IN UpnpDevice_Handle Hnd: The device handle
 *
 *  Description:
 *      downloads the description file and update the service table of the
 *	device. This function has been deprecated.
 *
 *  Return Values:
 *      UPNP_E_SUCCESS on success, nonzero on failure.
 *****************************************************************************/
int
UpnpAddRootDevice( IN const char *DescURL,
                   IN UpnpDevice_Handle Hnd )
{
    int retVal = 0;
    struct Handle_Info *HInfo;
    IXML_Document *temp;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    if( ( retVal =
          UpnpDownloadXmlDoc( DescURL, &( temp ) ) ) != UPNP_E_SUCCESS ) {
        return retVal;
    }

    HandleLock(  );
    if( GetHandleInfo( Hnd, &HInfo ) == UPNP_E_INVALID_HANDLE ) {
        HandleUnlock(  );
        ixmlDocument_free( temp );
        return UPNP_E_INVALID_HANDLE;
    }

    if( addServiceTable
        ( ( IXML_Node * ) temp, &HInfo->ServiceTable, DescURL ) ) {

        DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "UpnpAddRootDevice: GENA Service Table \n" );
                 UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "Here are the known services: \n" );
                 printServiceTable( &HInfo->ServiceTable, UPNP_INFO, API );
             )
    } else {
        DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "\nUpnpAddRootDevice: No Eventing Support Found \n" );
             )
    }

    ixmlDocument_free( temp );
    HandleUnlock(  );

    return UPNP_E_SUCCESS;
}
#endif // 0
#endif //INCLUDE_DEVICE_APIS

#ifdef INCLUDE_DEVICE_APIS

/****************************************************************************
 * Function: UpnpRegisterRootDevice
 *
 *  Parameters:	
 *		IN const char *DescUrl:Pointer to a string containing the 
 *                           description URL for this root device 
 *                           instance. 
 *		IN Upnp_FunPtr Callback: Pointer to the callback function for 
 *                               receiving asynchronous events. 
 *		IN const void *Cookie: Pointer to user data returned with the 
 *								callback function when invoked.
 *		OUT UpnpDevice_Handle *Hnd: Pointer to a variable to store the 
 *                                  new device handle.
 *
 *  Description:
 *      This function registers a device application with
 *  the UPnP Library.  A device application cannot make any other API
 *  calls until it registers using this function.  
 *
 *  Return Values:
 *      UPNP_E_SUCCESS on success, nonzero on failure.
 *****************************************************************************/
int
UpnpRegisterRootDevice( IN const char *DescUrl,
                        IN Upnp_FunPtr Fun,
                        IN const void *Cookie,
                        OUT UpnpDevice_Handle * Hnd )
{

    struct Handle_Info *HInfo;
    int retVal = 0;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                         "Inside UpnpRegisterRootDevice \n" );
         )
        HandleLock(  );

    if( UpnpSdkDeviceRegistered ) {
        HandleUnlock(  );
        return UPNP_E_ALREADY_REGISTERED;
    }

    if( Hnd == NULL || Fun == NULL ||
        DescUrl == NULL || strlen( DescUrl ) == 0 ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }

    if( ( *Hnd = GetFreeHandle(  ) ) == UPNP_E_OUTOF_HANDLE ) {
        HandleUnlock(  );
        return UPNP_E_OUTOF_MEMORY;
    }

    HInfo = ( struct Handle_Info * )malloc( sizeof( struct Handle_Info ) );
    if( HInfo == NULL ) {
        HandleUnlock(  );
        return UPNP_E_OUTOF_MEMORY;
    }
    HandleTable[*Hnd] = HInfo;

    DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                         "Root device URL is %s\n", DescUrl );
         )

        HInfo->aliasInstalled = 0;
    HInfo->HType = HND_DEVICE;
    strcpy( HInfo->DescURL, DescUrl );
    HInfo->Callback = Fun;
    HInfo->Cookie = ( void * )Cookie;
    HInfo->MaxAge = DEFAULT_MAXAGE;
    HInfo->DeviceList = NULL;
    HInfo->ServiceList = NULL;
    HInfo->DescDocument = NULL;
    CLIENTONLY( ListInit( &HInfo->SsdpSearchList, NULL, NULL );
         );
    CLIENTONLY( HInfo->ClientSubList = NULL;
         )
        HInfo->MaxSubscriptions = UPNP_INFINITE;
    HInfo->MaxSubscriptionTimeOut = UPNP_INFINITE;

    if( ( retVal =
          UpnpDownloadXmlDoc( HInfo->DescURL, &( HInfo->DescDocument ) ) )
        != UPNP_E_SUCCESS ) {
        CLIENTONLY( ListDestroy( &HInfo->SsdpSearchList, 0 ) );
        FreeHandle( *Hnd );
        HandleUnlock(  );
        return retVal;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "UpnpRegisterRootDevice: Valid Description\n" );
             UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                         "UpnpRegisterRootDevice: DescURL : %s\n",
                         HInfo->DescURL );
         )

        HInfo->DeviceList =
        ixmlDocument_getElementsByTagName( HInfo->DescDocument, "device" );
    if( HInfo->DeviceList == NULL ) {
        CLIENTONLY( ListDestroy( &HInfo->SsdpSearchList, 0 ) );
        ixmlDocument_free( HInfo->DescDocument );
        FreeHandle( *Hnd );
        HandleUnlock(  );
        DBGONLY( UpnpPrintf( UPNP_CRITICAL, API, __FILE__, __LINE__,
                             "UpnpRegisterRootDevice: No devices found for RootDevice\n" );
             )
            return UPNP_E_INVALID_DESC;
    }

    HInfo->ServiceList =
        ixmlDocument_getElementsByTagName( HInfo->DescDocument,
                                           "serviceList" );
    if( HInfo->ServiceList == NULL ) {
        CLIENTONLY( ListDestroy( &HInfo->SsdpSearchList, 0 ) );
        ixmlNodeList_free( HInfo->DeviceList );
        ixmlDocument_free( HInfo->DescDocument );
        FreeHandle( *Hnd );
        HandleUnlock(  );
        DBGONLY( UpnpPrintf( UPNP_CRITICAL, API, __FILE__, __LINE__,
                             "UpnpRegisterRootDevice: No services found for RootDevice\n" );
             )
            return UPNP_E_INVALID_DESC;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "UpnpRegisterRootDevice: Gena Check\n" );
         )
        //*******************************
        //GENA SET UP
        //*******************************
        if( getServiceTable( ( IXML_Node * ) HInfo->DescDocument,
                             &HInfo->ServiceTable, HInfo->DescURL ) ) {
        DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "UpnpRegisterRootDevice: GENA Service Table \n" );
                 UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "Here are the known services: \n" );
                 printServiceTable( &HInfo->ServiceTable, UPNP_INFO, API );
             )
    } else {
        CLIENTONLY( ListDestroy( &HInfo->SsdpSearchList, 0 ) );
        FreeHandle( *Hnd );
        HandleUnlock(  );
        DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "\nUpnpRegisterRootDevice: Errors retrieving service table \n" );
             )
            return UPNP_E_INVALID_DESC;
    }

    UpnpSdkDeviceRegistered = 1;
    HandleUnlock(  );

    DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                         "Exiting RegisterRootDevice Successfully\n" );
         )

        return UPNP_E_SUCCESS;

}  /****************** End of UpnpRegisterRootDevice  *********************/

#endif // INCLUDE_DEVICE_APIS

#ifdef INCLUDE_DEVICE_APIS
#if 0

/****************************************************************************
 * Function: UpnpRemoveRootDevice
 *
 *  Parameters:	
 *		IN const char *DescURL: Location of the root device 
 *								description xml file
 *		IN UpnpDevice_Handle Hnd: The device handle
 *
 *  Description:
 *      downloads the description file and update the service table of the
 *	device. This function has been deprecated.
 *
 *  Return Values:
 *      UPNP_E_SUCCESS on success, nonzero on failure.
 *****************************************************************************/
int
UpnpRemoveRootDevice( IN const char *DescURL,
                      IN UpnpDevice_Handle Hnd )
{
    int retVal = 0;
    struct Handle_Info *HInfo;

    IXML_Document *temp;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    if( ( retVal =
          UpnpDownloadXmlDoc( DescURL, &( temp ) ) ) != UPNP_E_SUCCESS ) {
        return retVal;
    }

    HandleLock(  );
    if( GetHandleInfo( Hnd, &HInfo ) == UPNP_E_INVALID_HANDLE ) {
        HandleUnlock(  );
        ixmlDocument_free( temp );
        return UPNP_E_INVALID_HANDLE;
    }

    if( removeServiceTable( ( IXML_Node * ) temp, &HInfo->ServiceTable ) ) {

        DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "UpnpRemoveRootDevice: GENA Service Table \n" );
                 UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "Here are the known services: \n" );
                 printServiceTable( &HInfo->ServiceTable, UPNP_INFO, API );
             )
    } else {
        DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "\nUpnpRemoveRootDevice: No Services Removed\n" );
                 UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "Here are the known services: \n" );
                 printServiceTable( &HInfo->ServiceTable, UPNP_INFO, API );
             )
    }

    HandleUnlock(  );

    ixmlDocument_free( temp );
    return UPNP_E_SUCCESS;
}
#endif //0
#endif //INCLUDE_DEVICE_APIS

#ifdef INCLUDE_DEVICE_APIS

/****************************************************************************
 * Function: UpnpUnRegisterRootDevice
 *
 *  Parameters:	
 *		IN UpnpDevice_Handle Hnd: The handle of the device instance 
 *                                to unregister
 *  Description:
 *      This function unregisters a root device registered with 
 *  UpnpRegisterRootDevice} or UpnpRegisterRootDevice2. After this call, the 
 *  UpnpDevice_Handle Hnd is no longer valid. For all advertisements that 
 *  have not yet expired, the UPnP library sends a device unavailable message 
 *  automatically. 
 *
 *  Return Values:
 *      UPNP_E_SUCCESS on success, nonzero on failure.
 *****************************************************************************/
int
UpnpUnRegisterRootDevice( IN UpnpDevice_Handle Hnd )
{
    int retVal = 0;
    struct Handle_Info *HInfo = NULL;

    // struct Handle_Info *info=NULL;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    HandleLock(  );
    if( !UpnpSdkDeviceRegistered ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    HandleUnlock(  );

    DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                         "Inside UpnpUnRegisterRootDevice \n" );
         )
#if EXCLUDE_GENA == 0
        if( genaUnregisterDevice( Hnd ) != UPNP_E_SUCCESS )
        return UPNP_E_INVALID_HANDLE;
#endif

    HandleLock(  );
    if( GetHandleInfo( Hnd, &HInfo ) == UPNP_E_INVALID_HANDLE ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    HandleUnlock(  );

#if EXCLUDE_SSDP == 0
    retVal = AdvertiseAndReply( -1, Hnd, 0, ( struct sockaddr_in * )NULL,
                                ( char * )NULL, ( char * )NULL,
                                ( char * )NULL, HInfo->MaxAge );
#endif

    HandleLock(  );
    if( GetHandleInfo( Hnd, &HInfo ) == UPNP_E_INVALID_HANDLE ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    //info = (struct Handle_Info *) HandleTable[Hnd];
    ixmlNodeList_free( HInfo->DeviceList );
    ixmlNodeList_free( HInfo->ServiceList );
    ixmlDocument_free( HInfo->DescDocument );

    CLIENTONLY( ListDestroy( &HInfo->SsdpSearchList, 0 ) );

#ifdef INTERNAL_WEB_SERVER
    if( HInfo->aliasInstalled ) {
        web_server_set_alias( NULL, NULL, 0, 0 );
    }
#endif // INTERNAL_WEB_SERVER

    FreeHandle( Hnd );
    UpnpSdkDeviceRegistered = 0;
    HandleUnlock(  );

    DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                         "Exiting UpnpUnRegisterRootDevice \n" );
         )

        return retVal;

}  /****************** End of UpnpUnRegisterRootDevice *********************/

#endif //INCLUDE_DEVICE_APIS

// *************************************************************
#ifdef INCLUDE_DEVICE_APIS
#ifdef INTERNAL_WEB_SERVER

/**************************************************************************
 * Function: GetNameForAlias
 *
 *  Parameters:	
 *		IN char *name: name of the file
 *		OUT char** alias: pointer to alias string 
 *
 *  Description:
 *      This function determines alias for given name which is a file name 
 *  or URL.
 *
 *  Return Values:
 *      UPNP_E_SUCCESS on success, nonzero on failure.
 ***************************************************************************/
static int
GetNameForAlias( IN char *name,
                 OUT char **alias )
{
    char *ext;
    char *al;

    ext = strrchr( name, '.' );
    if( ext == NULL || strcasecmp( ext, ".xml" ) != 0 ) {
        return UPNP_E_EXT_NOT_XML;
    }

    al = strrchr( name, '/' );
    if( al == NULL ) {
        *alias = name;
    } else {
        *alias = al;
    }

    return UPNP_E_SUCCESS;
}

/**************************************************************************
 * Function: get_server_addr
 *
 *  Parameters:	
 *		OUT struct sockaddr_in* serverAddr: pointer to server address
 *											structure 
 *
 *  Description:
 *      This function fills the sockadr_in with miniserver information.
 *
 *  Return Values: VOID
 *      
 ***************************************************************************/
static void
get_server_addr( OUT struct sockaddr_in *serverAddr )
{
    memset( serverAddr, 0, sizeof( struct sockaddr_in ) );

    serverAddr->sin_family = AF_INET;
    serverAddr->sin_port = htons( LOCAL_PORT );
    //inet_aton( LOCAL_HOST, &serverAddr->sin_addr );
    serverAddr->sin_addr.s_addr = inet_addr( LOCAL_HOST );
}

/**************************************************************************
 * Function: GetDescDocumentAndURL ( In the case of device)
 *
 *  Parameters:	
 *		IN Upnp_DescType descriptionType: pointer to server address
 *											structure 
 *		IN char* description:
 *		IN unsigned int bufferLen:
 *		IN int config_baseURL:
 *		OUT IXML_Document **xmlDoc:
 *		OUT char descURL[LINE_SIZE]: 
 *
 *  Description:
 *      This function fills the sockadr_in with miniserver information.
 *
 *  Return Values: VOID
 *      
 ***************************************************************************/
static int
GetDescDocumentAndURL( IN Upnp_DescType descriptionType,
                       IN char *description,
                       IN unsigned int bufferLen,
                       IN int config_baseURL,
                       OUT IXML_Document ** xmlDoc,
                       OUT char descURL[LINE_SIZE] )
{
    int retVal = 0;
    char *membuf = NULL;
    char aliasStr[LINE_SIZE];
    char *temp_str = NULL;
    FILE *fp = NULL;
    unsigned fileLen;
    unsigned num_read;
    time_t last_modified;
    struct stat file_info;
    struct sockaddr_in serverAddr;
    int rc = UPNP_E_SUCCESS;

    if( description == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }
    // non-URL description must have configuration specified
    if( descriptionType != UPNPREG_URL_DESC && ( !config_baseURL ) ) {
        return UPNP_E_INVALID_PARAM;
    }
    // get XML doc and last modified time
    if( descriptionType == UPNPREG_URL_DESC ) {
        if( ( retVal =
              UpnpDownloadXmlDoc( description,
                                  xmlDoc ) ) != UPNP_E_SUCCESS ) {
            return retVal;
        }
        last_modified = time( NULL );
    } else if( descriptionType == UPNPREG_FILENAME_DESC ) {
        retVal = stat( description, &file_info );
        if( retVal == -1 ) {
            return UPNP_E_FILE_NOT_FOUND;
        }
        fileLen = file_info.st_size;
        last_modified = file_info.st_mtime;

        if( ( fp = fopen( description, "rb" ) ) == NULL ) {
            return UPNP_E_FILE_NOT_FOUND;
        }

        if( ( membuf = ( char * )malloc( fileLen + 1 ) ) == NULL ) {
            fclose( fp );
            return UPNP_E_OUTOF_MEMORY;
        }

        num_read = fread( membuf, 1, fileLen, fp );
        if( num_read != fileLen ) {
            fclose( fp );
            free( membuf );
            return UPNP_E_FILE_READ_ERROR;
        }

        membuf[fileLen] = 0;
        fclose( fp );
        rc = ixmlParseBufferEx( membuf, xmlDoc );
        free( membuf );
    } else if( descriptionType == UPNPREG_BUF_DESC ) {
        last_modified = time( NULL );
        rc = ixmlParseBufferEx( description, xmlDoc );
    } else {
        return UPNP_E_INVALID_PARAM;
    }

    if( rc != IXML_SUCCESS && descriptionType != UPNPREG_URL_DESC ) {
        if( rc == IXML_INSUFFICIENT_MEMORY ) {
            return UPNP_E_OUTOF_MEMORY;
        } else {
            return UPNP_E_INVALID_DESC;
        }
    }
    // determine alias
    if( config_baseURL ) {
        if( descriptionType == UPNPREG_BUF_DESC ) {
            strcpy( aliasStr, "description.xml" );
        } else                  // URL or filename
        {
            retVal = GetNameForAlias( description, &temp_str );
            if( retVal != UPNP_E_SUCCESS ) {
                ixmlDocument_free( *xmlDoc );
                return retVal;
            }
            if( strlen( temp_str ) > ( LINE_SIZE - 1 ) ) {
                ixmlDocument_free( *xmlDoc );
                free( temp_str );
                return UPNP_E_URL_TOO_BIG;
            }
            strcpy( aliasStr, temp_str );
        }

        get_server_addr( &serverAddr );

        // config
        retVal = configure_urlbase( *xmlDoc, &serverAddr,
                                    aliasStr, last_modified, descURL );
        if( retVal != UPNP_E_SUCCESS ) {
            ixmlDocument_free( *xmlDoc );
            return retVal;
        }
    } else                      // manual
    {
        if( strlen( description ) > ( LINE_SIZE - 1 ) ) {
            ixmlDocument_free( *xmlDoc );
            return UPNP_E_URL_TOO_BIG;
        }
        strcpy( descURL, description );
    }

    assert( *xmlDoc != NULL );

    return UPNP_E_SUCCESS;
}

#else // no web server

/**************************************************************************
 * Function: GetDescDocumentAndURL ( In the case of control point)
 *
 *  Parameters:	
 *		IN Upnp_DescType descriptionType: pointer to server address
 *											structure 
 *		IN char* description:
 *		IN unsigned int bufferLen:
 *		IN int config_baseURL:
 *		OUT IXML_Document **xmlDoc:
 *		OUT char *descURL: 
 *
 *  Description:
 *      This function fills the sockadr_in with miniserver information.
 *
 *  Return Values: VOID
 *      
 ***************************************************************************/
static int
GetDescDocumentAndURL( IN Upnp_DescType descriptionType,
                       IN char *description,
                       IN unsigned int bufferLen,
                       IN int config_baseURL,
                       OUT IXML_Document ** xmlDoc,
                       OUT char *descURL )
{
    int retVal;

    if( ( descriptionType != UPNPREG_URL_DESC ) || config_baseURL ) {
        return UPNP_E_NO_WEB_SERVER;
    }

    if( description == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }

    if( strlen( description ) > ( LINE_SIZE - 1 ) ) {
        return UPNP_E_URL_TOO_BIG;
    }
    strcpy( descURL, description );

    if( ( retVal =
          UpnpDownloadXmlDoc( description, xmlDoc ) ) != UPNP_E_SUCCESS ) {
        return retVal;
    }

    return UPNP_E_SUCCESS;
}

#endif // INTERNAL_WEB_SERVER
// ********************************************************

/****************************************************************************
 * Function: UpnpRegisterRootDevice2
 *
 *  Parameters:	
 *		IN Upnp_DescType descriptionType: The type of description document.
 *		IN const char* description:  Treated as a URL, file name or 
 *                                   memory buffer depending on 
 *                                   description type. 
 *		IN size_t bufferLen: Length of memory buffer if passing a description
 *                           in a buffer, otherwize ignored.
 *		IN int config_baseURL: If nonzero, URLBase of description document is 
 *								configured and the description is served 
 *                               using the internal web server.
 *		IN Upnp_FunPtr Fun: Pointer to the callback function for 
 *							receiving asynchronous events. 
 *		IN const void* Cookie: Pointer to user data returned with the 
 *								callback function when invoked. 
 *		OUT UpnpDevice_Handle* Hnd: Pointer to a variable to store 
 *									the new device handle.
 *
 *  Description:
 *      This function is similar to  UpnpRegisterRootDevice except that
 *  it also allows the description document to be specified as a file or 
 *  a memory buffer. The description can also be configured to have the
 *  correct IP and port address.
 *
 *  Return Values:
 *      UPNP_E_SUCCESS on success, nonzero on failure.
 *****************************************************************************/
int
UpnpRegisterRootDevice2( IN Upnp_DescType descriptionType,
                         IN const char *description_const,
                         IN size_t bufferLen,   // ignored unless descType == UPNPREG_BUF_DESC

                         IN int config_baseURL,
                         IN Upnp_FunPtr Fun,
                         IN const void *Cookie,
                         OUT UpnpDevice_Handle * Hnd )
{
    struct Handle_Info *HInfo;
    int retVal = 0;
    char *description = ( char * )description_const;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                         "Inside UpnpRegisterRootDevice2 \n" );
         )

        if( Hnd == NULL || Fun == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }

    HandleLock(  );

    if( UpnpSdkDeviceRegistered ) {
        HandleUnlock(  );
        return UPNP_E_ALREADY_REGISTERED;
    }

    if( ( *Hnd = GetFreeHandle(  ) ) == UPNP_E_OUTOF_HANDLE ) {
        HandleUnlock(  );
        return UPNP_E_OUTOF_MEMORY;
    }

    HInfo = ( struct Handle_Info * )malloc( sizeof( struct Handle_Info ) );
    if( HInfo == NULL ) {
        HandleUnlock(  );
        return UPNP_E_OUTOF_MEMORY;
    }
    HandleTable[*Hnd] = HInfo;

    // prevent accidental removal of a non-existent alias
    HInfo->aliasInstalled = 0;

    retVal = GetDescDocumentAndURL( descriptionType, description,
                                    bufferLen, config_baseURL,
                                    &HInfo->DescDocument, HInfo->DescURL );
    //HInfo->DescAlias );

    if( retVal != UPNP_E_SUCCESS ) {
        FreeHandle( *Hnd );
        HandleUnlock(  );
        return retVal;
    }

    HInfo->aliasInstalled = ( config_baseURL != 0 );

    HInfo->HType = HND_DEVICE;
    HInfo->Callback = Fun;
    HInfo->Cookie = ( void * )Cookie;
    HInfo->MaxAge = DEFAULT_MAXAGE;
    HInfo->DeviceList = NULL;
    HInfo->ServiceList = NULL;
    CLIENTONLY( HInfo->ClientSubList = NULL;
         )
        CLIENTONLY( ListInit( &HInfo->SsdpSearchList, NULL, NULL );
         );
    HInfo->MaxSubscriptions = UPNP_INFINITE;
    HInfo->MaxSubscriptionTimeOut = UPNP_INFINITE;

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "UpnpRegisterRootDevice2: Valid Description\n" );
             UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "UpnpRegisterRootDevice2: DescURL : %s\n",
                         HInfo->DescURL );
         )

        HInfo->DeviceList =
        ixmlDocument_getElementsByTagName( HInfo->DescDocument, "device" );

    if( HInfo->DeviceList == NULL ) {
        CLIENTONLY( ListDestroy( &HInfo->SsdpSearchList, 0 ) );
        ixmlDocument_free( HInfo->DescDocument );
        FreeHandle( *Hnd );
        HandleUnlock(  );
        DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "UpnpRegisterRootDevice2: No devices found for RootDevice\n" );
             )
            return UPNP_E_INVALID_DESC;
    }

    HInfo->ServiceList =
        ixmlDocument_getElementsByTagName( HInfo->DescDocument,
                                           "serviceList" );

    if( HInfo->ServiceList == NULL ) {
        CLIENTONLY( ListDestroy( &HInfo->SsdpSearchList, 0 ) );
        ixmlNodeList_free( HInfo->DeviceList );
        ixmlDocument_free( HInfo->DescDocument );
        FreeHandle( *Hnd );
        HandleUnlock(  );
        DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "UpnpRegisterRootDevice2: No services found for RootDevice\n" );
             )
            return UPNP_E_INVALID_DESC;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "UpnpRegisterRootDevice2: Gena Check\n" );
         )
        //*******************************
        //GENA SET UP
        //*******************************
        if( getServiceTable( ( IXML_Node * ) HInfo->DescDocument,
                             &HInfo->ServiceTable, HInfo->DescURL ) ) {
        DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                             "UpnpRegisterRootDevice2: GENA Service Table \n" );
             )
    } else {
        CLIENTONLY( ListDestroy( &HInfo->SsdpSearchList, 0 ) );
        FreeHandle( *Hnd );
        HandleUnlock(  );
        DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "\nUpnpRegisterRootDevice: Errors retrieving service table \n" );
             )
            return UPNP_E_INVALID_DESC;
    }
    UpnpSdkDeviceRegistered = 1;
    HandleUnlock(  );
    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting RegisterRootDevice2 Successfully\n" );
         )
        return UPNP_E_SUCCESS;

}  /****************** End of UpnpRegisterRootDevice2  *********************/

#endif //INCLUDE_DEVICE_APIS

#ifdef INCLUDE_CLIENT_APIS

/**************************************************************************
 * Function: UpnpRegisterClient
 *
 *  Parameters:	
 *		IN Upnp_FunPtr Fun:  Pointer to a function for receiving 
 *							 asynchronous events.
 *		IN const void * Cookie: Pointer to user data returned with the 
 *								callback function when invoked.
 *		OUT UpnpClient_Handle *Hnd: Pointer to a variable to store 
 *									the new control point handle.
 *
 *  Description:
 *      This function registers a control point application with the
 *  UPnP Library.  A control point application cannot make any other API 
 *  calls until it registers using this function.
 *
 *  Return Values: int
 *      
 ***************************************************************************/
int
UpnpRegisterClient( IN Upnp_FunPtr Fun,
                    IN const void *Cookie,
                    OUT UpnpClient_Handle * Hnd )
{
    struct Handle_Info *HInfo;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpRegisterClient \n" );
         )

        if( Fun == NULL || Hnd == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }

    HandleLock(  );

    if( UpnpSdkClientRegistered ) {
        HandleUnlock(  );
        return UPNP_E_ALREADY_REGISTERED;
    }

    if( ( *Hnd = GetFreeHandle(  ) ) == UPNP_E_OUTOF_HANDLE ) {
        HandleUnlock(  );
        return UPNP_E_OUTOF_MEMORY;
    }
    HInfo = ( struct Handle_Info * )malloc( sizeof( struct Handle_Info ) );
    if( HInfo == NULL ) {
        HandleUnlock(  );
        return UPNP_E_OUTOF_MEMORY;
    }

    HInfo->HType = HND_CLIENT;
    HInfo->Callback = Fun;
    HInfo->Cookie = ( void * )Cookie;
    DEVICEONLY( HInfo->MaxAge = 0;
		)
    HInfo->ClientSubList = NULL;
    ListInit( &HInfo->SsdpSearchList, NULL, NULL );
    DEVICEONLY( HInfo->MaxSubscriptions = UPNP_INFINITE;
         )
        DEVICEONLY( HInfo->MaxSubscriptionTimeOut = UPNP_INFINITE;
         )

        HandleTable[*Hnd] = HInfo;

    UpnpSdkClientRegistered = 1;

    HandleUnlock(  );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpRegisterClient \n" );
         )

        return UPNP_E_SUCCESS;

}  /****************** End of UpnpRegisterClient   *********************/
#endif // INCLUDE_CLIENT_APIS

#ifdef INCLUDE_CLIENT_APIS

/****************************************************************************
 * Function: UpnpUnRegisterClient
 *
 *  Parameters:	
 *		IN UpnpClient_Handle Hnd: The handle of the control point instance 
 *                                to unregister
 *  Description:
 *      This function unregisters a client registered with 
 *  UpnpRegisterclient or UpnpRegisterclient2. After this call, the 
 *  UpnpDevice_Handle Hnd is no longer valid. The UPnP Library generates 
 *	no more callbacks after this function returns.
 *
 *  Return Values:
 *      UPNP_E_SUCCESS on success, nonzero on failure.
 *****************************************************************************/
int
UpnpUnRegisterClient( IN UpnpClient_Handle Hnd )
{
    struct Handle_Info *HInfo;
    ListNode *node = NULL;
    SsdpSearchArg *searchArg = NULL;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpUnRegisterClient \n" );
         )
        HandleLock(  );
    if( !UpnpSdkClientRegistered ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    HandleUnlock(  );

#if EXCLUDE_GENA == 0
    if( genaUnregisterClient( Hnd ) != UPNP_E_SUCCESS )
        return UPNP_E_INVALID_HANDLE;
#endif
    HandleLock(  );
    if( GetHandleInfo( Hnd, &HInfo ) == UPNP_E_INVALID_HANDLE ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    //clean up search list
    node = ListHead( &HInfo->SsdpSearchList );
    while( node != NULL ) {
        searchArg = ( SsdpSearchArg * ) node->item;
        if( searchArg ) {
            free( searchArg->searchTarget );
            free( searchArg );
        }
        ListDelNode( &HInfo->SsdpSearchList, node, 0 );
        node = ListHead( &HInfo->SsdpSearchList );
    }

    ListDestroy( &HInfo->SsdpSearchList, 0 );
    FreeHandle( Hnd );
    UpnpSdkClientRegistered = 0;
    HandleUnlock(  );
    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpUnRegisterClient \n" );
         )
        return UPNP_E_SUCCESS;

}  /****************** End of UpnpUnRegisterClient *********************/
#endif // INCLUDE_CLIENT_APIS

//-----------------------------------------------------------------------------
//
//                                   SSDP interface
//
//-----------------------------------------------------------------------------

#ifdef INCLUDE_DEVICE_APIS
#if EXCLUDE_SSDP == 0

/**************************************************************************
 * Function: UpnpSendAdvertisement 
 *
 *  Parameters:	
 *		IN UpnpDevice_Handle Hnd: handle of the device instance
 *		IN int Exp : Timer for resending the advertisement
 *
 *  Description:
 *      This function sends the device advertisement. It also schedules a
 *	job for the next advertisement after "Exp" time.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpSendAdvertisement( IN UpnpDevice_Handle Hnd,
                       IN int Exp )
{
    struct Handle_Info *SInfo = NULL;
    int retVal = 0,
     *ptrMx;
    upnp_timeout *adEvent;
    ThreadPoolJob job;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpSendAdvertisement \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_DEVICE ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    if( Exp < 1 )
        Exp = DEFAULT_MAXAGE;
    SInfo->MaxAge = Exp;
    HandleUnlock(  );
    retVal = AdvertiseAndReply( 1, Hnd, 0, ( struct sockaddr_in * )NULL,
                                ( char * )NULL, ( char * )NULL,
                                ( char * )NULL, Exp );

    if( retVal != UPNP_E_SUCCESS )
        return retVal;
    ptrMx = ( int * )malloc( sizeof( int ) );
    if( ptrMx == NULL )
        return UPNP_E_OUTOF_MEMORY;
    adEvent = ( upnp_timeout * ) malloc( sizeof( upnp_timeout ) );

    if( adEvent == NULL ) {
        free( ptrMx );
        return UPNP_E_OUTOF_MEMORY;
    }
    *ptrMx = Exp;
    adEvent->handle = Hnd;
    adEvent->Event = ptrMx;

    HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_DEVICE ) {
        HandleUnlock(  );
        free( adEvent );
        free( ptrMx );
        return UPNP_E_INVALID_HANDLE;
    }
#ifdef SSDP_PACKET_DISTRIBUTE
    TPJobInit( &job, ( start_routine ) AutoAdvertise, adEvent );
    TPJobSetFreeFunction( &job, ( free_routine ) free_upnp_timeout );
    TPJobSetPriority( &job, MED_PRIORITY );
    if( ( retVal = TimerThreadSchedule( &gTimerThread,
                                        ( ( Exp / 2 ) -
                                          ( AUTO_ADVERTISEMENT_TIME ) ),
                                        REL_SEC, &job, SHORT_TERM,
                                        &( adEvent->eventId ) ) )
        != UPNP_E_SUCCESS ) {
        HandleUnlock(  );
        free( adEvent );
        free( ptrMx );
        return retVal;
    }
#else
    TPJobInit( &job, ( start_routine ) AutoAdvertise, adEvent );
    TPJobSetFreeFunction( &job, ( free_routine ) free_upnp_timeout );
    TPJobSetPriority( &job, MED_PRIORITY );
    if( ( retVal = TimerThreadSchedule( &gTimerThread,
                                        Exp - AUTO_ADVERTISEMENT_TIME,
                                        REL_SEC, &job, SHORT_TERM,
                                        &( adEvent->eventId ) ) )
        != UPNP_E_SUCCESS ) {
        HandleUnlock(  );
        free( adEvent );
        free( ptrMx );
        return retVal;
    }
#endif

    HandleUnlock(  );
    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpSendAdvertisement \n" ); )

        return retVal;

}  /****************** End of UpnpSendAdvertisement *********************/
#endif // INCLUDE_DEVICE_APIS
#endif
#if EXCLUDE_SSDP == 0
#ifdef INCLUDE_CLIENT_APIS

/**************************************************************************
 * Function: UpnpSendAdvertisement 
 *
 *  Parameters:	
 *		IN UpnpClient_Handle Hnd: handle of the control point instance
 *		IN int Mx : Maximum time to wait for the search reply
 *		IN const char *Target_const: 
 *		IN const void *Cookie_const:
 *
 *  Description:
 *      This function searches for the devices for the provided maximum time.
 *	It is a asynchronous function. It schedules a search job and returns. 
 *	client is notified about the search results after search timer.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpSearchAsync( IN UpnpClient_Handle Hnd,
                 IN int Mx,
                 IN const char *Target_const,
                 IN const void *Cookie_const )
{
    struct Handle_Info *SInfo = NULL;
    char *Target = ( char * )Target_const;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpSearchAsync \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_CLIENT ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    if( Mx < 1 )
        Mx = DEFAULT_MX;

    if( Target == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }

    HandleUnlock(  );
    SearchByTarget( Mx, Target, ( void * )Cookie_const );

    //HandleUnlock();

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpSearchAsync \n" );
         )

        return UPNP_E_SUCCESS;

}  /****************** End of UpnpSearchAsync *********************/
#endif // INCLUDE_CLIENT_APIS
#endif
//-----------------------------------------------------------------------------
//
//                                   GENA interface 
//
//-----------------------------------------------------------------------------

#if EXCLUDE_GENA == 0
#ifdef INCLUDE_DEVICE_APIS

/**************************************************************************
 * Function: UpnpSetMaxSubscriptions 
 *
 *  Parameters:	
 *		IN UpnpDevice_Handle Hnd: The handle of the device for which
 *						the maximum subscriptions is being set.
 *		IN int MaxSubscriptions: The maximum number of subscriptions to be
 *				  allowed per service.
 *
 *  Description:
 *      This function sets the maximum subscriptions of the control points
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpSetMaxSubscriptions( IN UpnpDevice_Handle Hnd,
                         IN int MaxSubscriptions )
{
    struct Handle_Info *SInfo = NULL;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpSetMaxSubscriptions \n" );
         )

        HandleLock(  );
    if( ( ( MaxSubscriptions != UPNP_INFINITE )
          && ( MaxSubscriptions < 0 ) )
        || ( GetHandleInfo( Hnd, &SInfo ) != HND_DEVICE ) ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    SInfo->MaxSubscriptions = MaxSubscriptions;
    HandleUnlock(  );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpSetMaxSubscriptions \n" );
         )

        return UPNP_E_SUCCESS;

}  /***************** End of UpnpSetMaxSubscriptions ********************/
#endif // INCLUDE_DEVICE_APIS

#ifdef INCLUDE_DEVICE_APIS

/**************************************************************************
 * Function: UpnpSetMaxSubscriptionTimeOut 
 *
 *  Parameters:	
 *		IN UpnpDevice_Handle Hnd: The handle of the device for which the
 *								maximum subscription time-out is being set.
 *		IN int MaxSubscriptionTimeOut:The maximum subscription time-out 
 *									to be accepted
 *
 *  Description:
 *      This function sets the maximum subscription timer. Control points
 *	will require to send the subscription request before timeout.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpSetMaxSubscriptionTimeOut( IN UpnpDevice_Handle Hnd,
                               IN int MaxSubscriptionTimeOut )
{
    struct Handle_Info *SInfo = NULL;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpSetMaxSubscriptionTimeOut \n" );
         )

        HandleLock(  );

    if( ( ( MaxSubscriptionTimeOut != UPNP_INFINITE )
          && ( MaxSubscriptionTimeOut < 0 ) )
        || ( GetHandleInfo( Hnd, &SInfo ) != HND_DEVICE ) ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }

    SInfo->MaxSubscriptionTimeOut = MaxSubscriptionTimeOut;
    HandleUnlock(  );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpSetMaxSubscriptionTimeOut \n" );
         )

        return UPNP_E_SUCCESS;

}  /****************** End of UpnpSetMaxSubscriptionTimeOut ******************/
#endif // INCLUDE_DEVICE_APIS

#ifdef INCLUDE_CLIENT_APIS

/**************************************************************************
 * Function: UpnpSubscribeAsync 
 *
 *  Parameters:	
 *		IN UpnpClient_Handle Hnd: The handle of the control point for which 
 *								the subscription request is to be sent.
 *		IN const char * EvtUrl_const: URL that control point wants to 
 *								subscribe
 *		IN int TimeOut: The requested subscription time.  Upon 
 *                      return, it contains the actual subscription time 
 *						returned from the service
 *		IN Upnp_FunPtr Fun : callback function to tell result of the 
 *							subscription request
 *		IN const void * Cookie_const: cookie passed by client to give back 
 *				in the callback function.
 *
 *  Description:
 *      This function performs the same operation as UpnpSubscribeAsync
 *	but returns immediately and calls the registered callback function 
 *	when the operation is complete.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpSubscribeAsync( IN UpnpClient_Handle Hnd,
                    IN const char *EvtUrl_const,
                    IN int TimeOut,
                    IN Upnp_FunPtr Fun,
                    IN const void *Cookie_const )
{
    struct Handle_Info *SInfo = NULL;
    struct UpnpNonblockParam *Param;
    char *EvtUrl = ( char * )EvtUrl_const;
    ThreadPoolJob job;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpSubscribeAsync \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_CLIENT ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    if( EvtUrl == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( TimeOut != UPNP_INFINITE && TimeOut < 1 ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( Fun == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }

    Param =
        ( struct UpnpNonblockParam * )
        malloc( sizeof( struct UpnpNonblockParam ) );
    if( Param == NULL ) {
        HandleUnlock(  );
        return UPNP_E_OUTOF_MEMORY;
    }
    HandleUnlock(  );

    Param->FunName = SUBSCRIBE;
    Param->Handle = Hnd;
    strcpy( Param->Url, EvtUrl );
    Param->TimeOut = TimeOut;
    Param->Fun = Fun;
    Param->Cookie = ( void * )Cookie_const;

    TPJobInit( &job, ( start_routine ) UpnpThreadDistribution, Param );
    TPJobSetFreeFunction( &job, ( free_routine ) free );
    TPJobSetPriority( &job, MED_PRIORITY );
    ThreadPoolAdd( &gSendThreadPool, &job, NULL );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpSubscribeAsync \n" );
         )

        return UPNP_E_SUCCESS;

}  /****************** End of UpnpSubscribeAsync *********************/
#endif // INCLUDE_CLIENT_APIS

#ifdef INCLUDE_CLIENT_APIS

/**************************************************************************
 * Function: UpnpSubscribe 
 *
 *  Parameters:	
 *		IN UpnpClient_Handle Hnd: The handle of the control point.
 *		IN const char *PublisherUrl: The URL of the service to subscribe to.
 *		INOUT int *TimeOut: Pointer to a variable containing the requested 
 *					subscription time.  Upon return, it contains the
 *					actual subscription time returned from the service.
 *		OUT Upnp_SID SubsId: Pointer to a variable to receive the 
 *							subscription ID (SID). 
 *
 *  Description:
 *      This function registers a control point to receive event
 *  notifications from another device.  This operation is synchronous
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpSubscribe( IN UpnpClient_Handle Hnd,
               IN const char *EvtUrl_const,
               INOUT int *TimeOut,
               OUT Upnp_SID SubsId )
{
    struct Handle_Info *SInfo = NULL;
    int RetVal;
    char *EvtUrl = ( char * )EvtUrl_const;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpSubscribe \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_CLIENT ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    if( EvtUrl == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( TimeOut == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( SubsId == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    HandleUnlock(  );
    RetVal = genaSubscribe( Hnd, EvtUrl, TimeOut, SubsId );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpSubscribe \n" );
         )

        return RetVal;

}  /****************** End of UpnpSubscribe  *********************/
#endif // INCLUDE_CLIENT_APIS

#ifdef INCLUDE_CLIENT_APIS

/**************************************************************************
 * Function: UpnpUnSubscribe 
 *
 *  Parameters:	
 *		IN UpnpClient_Handle Hnd: The handle of the control point.
 *		IN Upnp_SID SubsId: The ID returned when the control point 
 *                                 subscribed to the service.
 *
 *  Description:
 *      This function removes the subscription of  a control point from a 
 *  service previously subscribed to using UpnpSubscribe or 
 *  UpnpSubscribeAsync. This is a synchronous call.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpUnSubscribe( IN UpnpClient_Handle Hnd,
                 IN Upnp_SID SubsId )
{
    struct Handle_Info *SInfo = NULL;
    int RetVal;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpUnSubscribe \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_CLIENT ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    if( SubsId == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    HandleUnlock(  );
    RetVal = genaUnSubscribe( Hnd, SubsId );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpUnSubscribe \n" );
         )

        return RetVal;

}  /****************** End of UpnpUnSubscribe  *********************/
#endif // INCLUDE_CLIENT_APIS

#ifdef INCLUDE_CLIENT_APIS

/**************************************************************************
 * Function: UpnpUnSubscribeAsync 
 *
 *  Parameters:	
 *		IN UpnpClient_Handle Hnd: The handle of the subscribed control 
 *                                point. 
 *		IN Upnp_SID SubsId:	The ID returned when the control point 
 *                           subscribed to the service.
 *		IN Upnp_FunPtr Fun:Pointer to a callback function to be called
 *                          when the operation is complete. 
 *		IN const void *Cookie:Pointer to user data to pass to the
                              callback function when invoked.
 *
 *  Description:
 *      This function removes a subscription of a control point
 *  from a service previously subscribed to using UpnpSubscribe or
 *	UpnpSubscribeAsync,generating a callback when the operation is complete.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpUnSubscribeAsync( IN UpnpClient_Handle Hnd,
                      IN Upnp_SID SubsId,
                      IN Upnp_FunPtr Fun,
                      IN const void *Cookie_const )
{
    ThreadPoolJob job;
    struct Handle_Info *SInfo = NULL;
    struct UpnpNonblockParam *Param;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpUnSubscribeAsync \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_CLIENT ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    if( SubsId == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( Fun == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }

    HandleUnlock(  );
    Param =
        ( struct UpnpNonblockParam * )
        malloc( sizeof( struct UpnpNonblockParam ) );
    if( Param == NULL )
        return UPNP_E_OUTOF_MEMORY;

    Param->FunName = UNSUBSCRIBE;
    Param->Handle = Hnd;
    strcpy( Param->SubsId, SubsId );
    Param->Fun = Fun;
    Param->Cookie = ( void * )Cookie_const;
    TPJobInit( &job, ( start_routine ) UpnpThreadDistribution, Param );
    TPJobSetFreeFunction( &job, ( free_routine ) free );
    TPJobSetPriority( &job, MED_PRIORITY );
    ThreadPoolAdd( &gSendThreadPool, &job, NULL );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpUnSubscribeAsync \n" );
         )

        return UPNP_E_SUCCESS;

}  /****************** End of UpnpUnSubscribeAsync  *********************/
#endif // INCLUDE_CLIENT_APIS

#ifdef INCLUDE_CLIENT_APIS

/**************************************************************************
 * Function: UpnpRenewSubscription 
 *
 *  Parameters:	
 *		IN UpnpClient_Handle Hnd: The handle of the control point that 
 *                                is renewing the subscription.
 *		INOUT int *TimeOut: Pointer to a variable containing the 
 *                          requested subscription time.  Upon return, 
 *                          it contains the actual renewal time. 
 *		IN Upnp_SID SubsId: The ID for the subscription to renew. 
 *
 *  Description:
 *      This function renews a subscription that is about to 
 *  expire.  This function is synchronous.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpRenewSubscription( IN UpnpClient_Handle Hnd,
                       INOUT int *TimeOut,
                       IN Upnp_SID SubsId )
{
    struct Handle_Info *SInfo = NULL;
    int RetVal;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpRenewSubscription \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_CLIENT ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    if( TimeOut == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( SubsId == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    HandleUnlock(  );
    RetVal = genaRenewSubscription( Hnd, SubsId, TimeOut );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpRenewSubscription \n" );
         )

        return RetVal;

}  /****************** End of UpnpRenewSubscription  *********************/
#endif // INCLUDE_CLIENT_APIS

#ifdef INCLUDE_CLIENT_APIS

/**************************************************************************
 * Function: UpnpRenewSubscriptionAsync 
 *
 *  Parameters:	
 *		IN UpnpClient_Handle Hnd: The handle of the control point that 
 *                                is renewing the subscription. 
 *		IN int TimeOut         : The requested subscription time.  The 
 *                                actual timeout value is returned when 
 *                                the callback function is called. 
 *		IN Upnp_SID SubsId     : The ID for the subscription to renew. 
 *		IN Upnp_FunPtr Fun     : Pointer to a callback function to be 
 *                               invoked when the renewal is complete. 
 *		IN const void *Cookie  : Pointer to user data passed 
 *                                to the callback function when invoked.
 *
 *  Description:
 *      This function renews a subscription that is about
 *  to expire, generating a callback when the operation is complete.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpRenewSubscriptionAsync( IN UpnpClient_Handle Hnd,
                            INOUT int TimeOut,
                            IN Upnp_SID SubsId,
                            IN Upnp_FunPtr Fun,
                            IN const void *Cookie_const )
{
    ThreadPoolJob job;
    struct Handle_Info *SInfo = NULL;
    struct UpnpNonblockParam *Param;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpRenewSubscriptionAsync \n" );
         )
        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_CLIENT ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    if( TimeOut != UPNP_INFINITE && TimeOut < 1 ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( SubsId == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( Fun == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    HandleUnlock(  );

    Param =
        ( struct UpnpNonblockParam * )
        malloc( sizeof( struct UpnpNonblockParam ) );
    if( Param == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    Param->FunName = RENEW;
    Param->Handle = Hnd;
    strcpy( Param->SubsId, SubsId );
    Param->Fun = Fun;
    Param->Cookie = ( void * )Cookie_const;
    Param->TimeOut = TimeOut;

    TPJobInit( &job, ( start_routine ) UpnpThreadDistribution, Param );
    TPJobSetFreeFunction( &job, ( free_routine ) free );
    TPJobSetPriority( &job, MED_PRIORITY );
    ThreadPoolAdd( &gSendThreadPool, &job, NULL );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpRenewSubscriptionAsync \n" );
         )

        return UPNP_E_SUCCESS;

}  /****************** End of UpnpRenewSubscriptionAsync *******************/
#endif // INCLUDE_CLIENT_APIS

#ifdef INCLUDE_DEVICE_APIS

/**************************************************************************
 * Function: UpnpNotify 
 *
 *  Parameters:	
 *		IN UpnpDevice_Handle   :The handle to the device sending the event.
 *		IN const char *DevID   :The device ID of the subdevice of the 
 *                              service generating the event. 
 *		IN const char *ServID  :The unique identifier of the service 
 *                              generating the event. 
 *		IN const char **VarName:Pointer to an array of variables that 
 *                              have changed. 
 *		IN const char **NewVal :Pointer to an array of new values for 
 *                              those variables. 
 *		IN int cVariables      :The count of variables included in this 
 *                              notification. 
 *
 *  Description:
 *      This function sends out an event change notification to all
 *  control points subscribed to a particular service.  This function is
 *  synchronous and generates no callbacks.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpNotify( IN UpnpDevice_Handle Hnd,
            IN const char *DevID_const,
            IN const char *ServName_const,
            IN const char **VarName_const,
            IN const char **NewVal_const,
            IN int cVariables )
{

    struct Handle_Info *SInfo = NULL;
    int retVal;
    char *DevID = ( char * )DevID_const;
    char *ServName = ( char * )ServName_const;
    char **VarName = ( char ** )VarName_const;
    char **NewVal = ( char ** )NewVal_const;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpNotify \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_DEVICE ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    if( DevID == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( ServName == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( VarName == NULL || NewVal == NULL || cVariables < 0 ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }

    HandleUnlock(  );
    retVal =
        genaNotifyAll( Hnd, DevID, ServName, VarName, NewVal, cVariables );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpNotify \n" );
         )

        return retVal;

} /****************** End of UpnpNotify *********************/

/**************************************************************************
 * Function: UpnpNotifyExt 
 *
 *  Parameters:	
 *    IN UpnpDevice_Handle	: The handle to the device sending the 
 *                              event.
 *	  IN const char *DevID	: The device ID of the subdevice of the 
 *                              service generating the event.
 *    IN const char *ServID,  : The unique identifier of the service 
 *                              generating the event. 
 *    IN IXML_Document *PropSet:The DOM document for the property set. 
 *                              Property set documents must conform to
 *                              the XML schema defined in section 4.3 of 
 *                              the Universal Plug and Play Device 
 *                              Architecture specification. 
 *
 *  Description:
 *      This function is similar to UpnpNotify except that it takes
 *  a DOM document for the event rather than an array of strings. This 
 *  function is synchronous and generates no callbacks.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpNotifyExt( IN UpnpDevice_Handle Hnd,
               IN const char *DevID_const,
               IN const char *ServName_const,
               IN IXML_Document * PropSet )
{

    struct Handle_Info *SInfo = NULL;
    int retVal;
    char *DevID = ( char * )DevID_const;
    char *ServName = ( char * )ServName_const;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpNotify \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_DEVICE ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    if( DevID == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( ServName == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }

    HandleUnlock(  );
    retVal = genaNotifyAllExt( Hnd, DevID, ServName, PropSet );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpNotify \n" );
         )

        return retVal;

}  /****************** End of UpnpNotify *********************/

#endif // INCLUDE_DEVICE_APIS

#ifdef INCLUDE_DEVICE_APIS

/**************************************************************************
 * Function: UpnpAcceptSubscription 
 *
 *  Parameters:	
 *	   IN UpnpDevice_Handle Hnd: The handle of the device. 
 *	   IN const char *DevID    : The device ID of the subdevice of the 
 *                               service generating the event. 
 *	   IN const char *ServID   : The unique service identifier of the 
 *                               service generating the event.
 *	   IN const char **VarName : Pointer to an array of event variables.
 *	   IN const char **NewVal  : Pointer to an array of values for 
 *                               the event variables.
 *     IN int cVariables       : The number of event variables in 
 *                               VarName. 
 *     IN Upnp_SID SubsId      : The subscription ID of the newly 
 *                               registered control point. 
 *
 *  Description:
 *      This function accepts a subscription request and sends
 *  out the current state of the eventable variables for a service.  
 *  The device application should call this function when it receives a 
 *  UPNP_EVENT_SUBSCRIPTION_REQUEST callback. This function is sychronous
 *  and generates no callbacks.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpAcceptSubscription( IN UpnpDevice_Handle Hnd,
                        IN const char *DevID_const,
                        IN const char *ServName_const,
                        IN const char **VarName_const,
                        IN const char **NewVal_const,
                        int cVariables,
                        IN Upnp_SID SubsId )
{
    struct Handle_Info *SInfo = NULL;
    int retVal;
    char *DevID = ( char * )DevID_const;
    char *ServName = ( char * )ServName_const;
    char **VarName = ( char ** )VarName_const;
    char **NewVal = ( char ** )NewVal_const;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpAcceptSubscription \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_DEVICE ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    if( DevID == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( ServName == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( SubsId == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( VarName == NULL || NewVal == NULL || cVariables < 0 ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }

    HandleUnlock(  );
    retVal =
        genaInitNotify( Hnd, DevID, ServName, VarName, NewVal, cVariables,
                        SubsId );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpAcceptSubscription \n" );
         )
        return retVal;

}  /***************** End of UpnpAcceptSubscription *********************/

/**************************************************************************
 * Function: UpnpAcceptSubscriptionExt 
 *
 *  Parameters:	
 *   IN UpnpDevice_Handle Hnd: The handle of the device. 
 *  IN const char *DevID,    : The device ID of the subdevice of the 
 *                                service generating the event. 
 *  IN const char *ServID,   : The unique service identifier of the service 
 *                                generating the event. 
 *  IN IXML_Document *PropSet: The DOM document for the property set. 
 *                             Property set documents must conform to
 *                             the XML schema defined in section 4.3 of the
 *                             Universal Plug and Play Device Architecture
 *                             specification. 
 *  IN Upnp_SID SubsId       : The subscription ID of the newly 
 *                             registered control point. 
 *
 *  Description:
 *      This function is similar to UpnpAcceptSubscription
 *  except that it takes a DOM document for the variables to event rather
 *  than an array of strings. This function is sychronous
 *  and generates no callbacks.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpAcceptSubscriptionExt( IN UpnpDevice_Handle Hnd,
                           IN const char *DevID_const,
                           IN const char *ServName_const,
                           IN IXML_Document * PropSet,
                           IN Upnp_SID SubsId )
{
    struct Handle_Info *SInfo = NULL;
    int retVal;
    char *DevID = ( char * )DevID_const;
    char *ServName = ( char * )ServName_const;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpAcceptSubscription \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_DEVICE ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    if( DevID == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( ServName == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }
    if( SubsId == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }

    if( PropSet == NULL ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_PARAM;
    }

    HandleUnlock(  );
    retVal = genaInitNotifyExt( Hnd, DevID, ServName, PropSet, SubsId );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpAcceptSubscription \n" );
         )

        return retVal;

}  /****************** End of UpnpAcceptSubscription *********************/

#endif // INCLUDE_DEVICE_APIS
#endif // EXCLUDE_GENA == 0

//---------------------------------------------------------------------------
//
//                                   SOAP interface 
//
//---------------------------------------------------------------------------
#if EXCLUDE_SOAP == 0
#ifdef INCLUDE_CLIENT_APIS

/**************************************************************************
 * Function: UpnpSendAction 
 *
 *  Parameters:	
 *		IN UpnpClient_Handle Hnd   : The handle of the control point 
 *                                   sending the action. 
 *		IN const char *ActionURL: The action URL of the service. 
 *		IN const char *ServiceType: The type of the service. 
 *		IN const char *DevUDN     : This parameter is ignored. 
 *		IN IXML_Document *Action  : The DOM document for the action. 
 *		OUT IXML_Document **RespNode : The DOM document for the response 
 *                                  to the action.  The UPnP Library allocates 
 *                                  this document and the caller needs to free 
 *                                   it.  
 *  
 *  Description:
 *      This function sends a message to change a state variable
 *  in a service.  This is a synchronous call that does not return until the 
 *  action is complete.
 * 
 *  Note that a positive return value indicates a SOAP-protocol error code.
 *  In this case,  the error description can be retrieved from RespNode.
 *  A negative return value indicates a UPnP Library error.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpSendAction( IN UpnpClient_Handle Hnd,
                IN const char *ActionURL_const,
                IN const char *ServiceType_const,
                IN const char *DevUDN_const,
                IN IXML_Document * Action,
                OUT IXML_Document ** RespNodePtr )
{
    struct Handle_Info *SInfo = NULL;
    int retVal = 0;
    char *ActionURL = ( char * )ActionURL_const;
    char *ServiceType = ( char * )ServiceType_const;

    //char *DevUDN = (char *)DevUDN_const;  // udn not used?

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpSendAction \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_CLIENT ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    HandleUnlock(  );

    if( ActionURL == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }
    if( ServiceType == NULL || Action == NULL || RespNodePtr == NULL
        || DevUDN_const != NULL ) {
        return UPNP_E_INVALID_PARAM;
    }

    retVal = SoapSendAction( ActionURL, ServiceType, Action, RespNodePtr );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpSendAction \n" );
         )

        return retVal;

}  /****************** End of UpnpSendAction *********************/

/**************************************************************************
 * Function: UpnpSendActionEx 
 *
 *  Parameters:	
 *		IN UpnpClient_Handle Hnd: The handle of the control point 
 *											sending the action. 
 *		IN const char *ActionURL_const: The action URL of the service. 
 *		IN const char *ServiceType_const: The type of the service. 
 *		IN const char *DevUDN_const : This parameter is ignored. 
 *		IN IXML_Document *Header    : The DOM document for the SOAP header. 
 *								      This may be NULL if the header is not
 *								      required. 
 *		IN IXML_Document *Action     :   The DOM document for the action. 
 *		OUT IXML_Document **RespNodePtr: The DOM document for the response to
 *										the action.  The UPnP library 
 *										allocates this document and the 
 *										needs to free
 *  
 *  Description:
 *      this function sends a message to change a state variable in a 
 *  service.  This is a synchronous call that does not return until the 
 *  action is complete.
 *
 *  Note that a positive return value indicates a SOAP-protocol error code.
 *  In this case,  the error description can be retrieved from {\bf RespNode}.
 *  A negative return value indicates a UPnP Library error.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpSendActionEx( IN UpnpClient_Handle Hnd,
                  IN const char *ActionURL_const,
                  IN const char *ServiceType_const,
                  IN const char *DevUDN_const,
                  IN IXML_Document * Header,
                  IN IXML_Document * Action,
                  OUT IXML_Document ** RespNodePtr )
{

    struct Handle_Info *SInfo = NULL;
    int retVal = 0;
    char *ActionURL = ( char * )ActionURL_const;
    char *ServiceType = ( char * )ServiceType_const;

    //char *DevUDN = (char *)DevUDN_const;  // udn not used?

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpSendActionEx \n" );
         )

        if( Header == NULL ) {
        retVal = UpnpSendAction( Hnd, ActionURL_const, ServiceType_const,
                                 DevUDN_const, Action, RespNodePtr );
        return retVal;
    }

    HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_CLIENT ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    HandleUnlock(  );

    if( ActionURL == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }
    if( ServiceType == NULL || Action == NULL || RespNodePtr == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }

    retVal = SoapSendActionEx( ActionURL, ServiceType, Header,
                               Action, RespNodePtr );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpSendAction \n" );
         )

        return retVal;

}  /****************** End of UpnpSendActionEx *********************/

/**************************************************************************
 * Function: UpnpSendActionAsync 
 *
 *  Parameters:	
 *		IN UpnpClient_Handle Hnd   : The handle of the control point 
 *                                   sending the action. 
 *		IN const char *ActionURL   : The action URL of the service. 
 *		IN const char *ServiceType : The type of the service. 
 *		IN const char *DevUDN      : This parameter is ignored. 
 *		IN IXML_Document *Action   : The DOM document for the action to 
 *                                   perform on this device. 
 *		IN Upnp_FunPtr Fun,        : Pointer to a callback function to 
 *                                  be invoked when the operation completes
 *		IN const void *Cookie      : Pointer to user data that to be 
 *                                  passed to the callback when invoked.
 *
 *  
 *  Description:
 *      this function sends a message to change a state variable
 *  in a service, generating a callback when the operation is complete.
 *  See UpnpSendAction for comments on positive return values. These 
 *  positive return values are sent in the event struct associated with the
 *  UPNP_CONTROL_ACTION_COMPLETE event.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpSendActionAsync( IN UpnpClient_Handle Hnd,
                     IN const char *ActionURL_const,
                     IN const char *ServiceType_const,
                     IN const char *DevUDN_const,
                     IN IXML_Document * Act,
                     IN Upnp_FunPtr Fun,
                     IN const void *Cookie_const )
{
    ThreadPoolJob job;
    struct Handle_Info *SInfo = NULL;
    struct UpnpNonblockParam *Param;
    DOMString tmpStr;
    char *ActionURL = ( char * )ActionURL_const;
    char *ServiceType = ( char * )ServiceType_const;

    //char *DevUDN = (char *)DevUDN_const;
    int rc;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpSendActionAsync \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_CLIENT ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    HandleUnlock(  );

    if( ActionURL == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }
    if( ServiceType == NULL ||
        Act == NULL || Fun == NULL || DevUDN_const != NULL ) {
        return UPNP_E_INVALID_PARAM;
    }
    tmpStr = ixmlPrintDocument( Act );
    if( tmpStr == NULL ) {
        return UPNP_E_INVALID_ACTION;
    }

    Param =
        ( struct UpnpNonblockParam * )
        malloc( sizeof( struct UpnpNonblockParam ) );

    if( Param == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    Param->FunName = ACTION;
    Param->Handle = Hnd;
    strcpy( Param->Url, ActionURL );
    strcpy( Param->ServiceType, ServiceType );

    rc = ixmlParseBufferEx( tmpStr, &( Param->Act ) );
    if( rc != IXML_SUCCESS ) {
        free( Param );
        ixmlFreeDOMString( tmpStr );
        if( rc == IXML_INSUFFICIENT_MEMORY ) {
            return UPNP_E_OUTOF_MEMORY;
        } else {
            return UPNP_E_INVALID_ACTION;
        }
    }
    ixmlFreeDOMString( tmpStr );
    Param->Cookie = ( void * )Cookie_const;
    Param->Fun = Fun;

    TPJobInit( &job, ( start_routine ) UpnpThreadDistribution, Param );
    TPJobSetFreeFunction( &job, ( free_routine ) free );

    TPJobSetPriority( &job, MED_PRIORITY );
    ThreadPoolAdd( &gSendThreadPool, &job, NULL );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpSendActionAsync \n" );
         )

        return UPNP_E_SUCCESS;

}  /****************** End of UpnpSendActionAsync *********************/

/*************************************************************************
 * Function: UpnpSendActionExAsync 
 *
 *  Parameters:	
 *    IN UpnpClient_Handle Hnd		   : The handle of the control point 
 *											sending the action. 
 *  IN const char	*ActionURL_const   : The action URL of the service. 
 *  IN const char	*ServiceType_const : The type of the service. 
 *  IN const char	*DevUDN_const      : This parameter is ignored. 
 *  IN IXML_Document *Header		   : The DOM document for the SOAP header. 
 *										This may be NULL if the header is not
 *										required. 
 *  IN IXML_Document *Act				: The DOM document for the action to 
 *										perform on this device. 
 *  IN Upnp_FunPtr Fun					: Pointer to a callback function to 
 *										be invoked when the operation 
 *										completes. 
 *  IN const void *Cookie_const			: Pointer to user data that to be
 *										passed to the callback when invoked. 
 *
 *  Description:
 *      this function sends sends a message to change a state variable
 *  in a service, generating a callback when the operation is complete.
 *  See UpnpSendAction for comments on positive return values. These 
 *  positive return values are sent in the event struct associated with 
 *  the UPNP_CONTROL_ACTION_COMPLETE event.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpSendActionExAsync( IN UpnpClient_Handle Hnd,
                       IN const char *ActionURL_const,
                       IN const char *ServiceType_const,
                       IN const char *DevUDN_const,
                       IN IXML_Document * Header,
                       IN IXML_Document * Act,
                       IN Upnp_FunPtr Fun,
                       IN const void *Cookie_const )
{
    struct Handle_Info *SInfo = NULL;
    struct UpnpNonblockParam *Param;
    DOMString tmpStr;
    DOMString headerStr = NULL;
    char *ActionURL = ( char * )ActionURL_const;
    char *ServiceType = ( char * )ServiceType_const;
    ThreadPoolJob job;
    int retVal = 0;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpSendActionExAsync \n" );
         )

        if( Header == NULL ) {
        retVal = UpnpSendActionAsync( Hnd, ActionURL_const,
                                      ServiceType_const, DevUDN_const, Act,
                                      Fun, Cookie_const );
        return retVal;
    }

    HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_CLIENT ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    HandleUnlock(  );

    if( ActionURL == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }
    if( ServiceType == NULL || Act == NULL || Fun == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }

    headerStr = ixmlPrintDocument( Header );

    tmpStr = ixmlPrintDocument( Act );
    if( tmpStr == NULL ) {
        return UPNP_E_INVALID_ACTION;
    }

    Param =
        ( struct UpnpNonblockParam * )
        malloc( sizeof( struct UpnpNonblockParam ) );
    if( Param == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    Param->FunName = ACTION;
    Param->Handle = Hnd;
    strcpy( Param->Url, ActionURL );
    strcpy( Param->ServiceType, ServiceType );
    retVal = ixmlParseBufferEx( headerStr, &( Param->Header ) );
    if( retVal != IXML_SUCCESS ) {
        ixmlFreeDOMString( tmpStr );
        ixmlFreeDOMString( headerStr );
        if( retVal == IXML_INSUFFICIENT_MEMORY ) {
            return UPNP_E_OUTOF_MEMORY;
        } else {
            return UPNP_E_INVALID_ACTION;
        }
    }

    retVal = ixmlParseBufferEx( tmpStr, &( Param->Act ) );
    if( retVal != IXML_SUCCESS ) {
        ixmlFreeDOMString( tmpStr );
        ixmlFreeDOMString( headerStr );
        ixmlDocument_free( Param->Header );
        if( retVal == IXML_INSUFFICIENT_MEMORY ) {
            return UPNP_E_OUTOF_MEMORY;
        } else {
            return UPNP_E_INVALID_ACTION;
        }

    }

    ixmlFreeDOMString( tmpStr );
    ixmlFreeDOMString( headerStr );

    Param->Cookie = ( void * )Cookie_const;
    Param->Fun = Fun;

    TPJobInit( &job, ( start_routine ) UpnpThreadDistribution, Param );
    TPJobSetFreeFunction( &job, ( free_routine ) free );

    TPJobSetPriority( &job, MED_PRIORITY );
    ThreadPoolAdd( &gSendThreadPool, &job, NULL );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpSendActionAsync \n" );
         )

        return UPNP_E_SUCCESS;

}  /****************** End of UpnpSendActionExAsync *********************/

/*************************************************************************
 * Function: UpnpGetServiceVarStatusAsync 
 *
 *  Parameters:	
 *    IN UpnpClient_Handle Hnd: The handle of the control point. 
 *    IN const char *ActionURL: The URL of the service. 
 *    IN const char *VarName  : The name of the variable to query. 
 *    IN Upnp_FunPtr Fun,     : Pointer to a callback function to 
 *                                be invoked when the operation is complete. 
 *    IN const void *Cookie   : Pointer to user data to pass to the 
 *                                callback function when invoked. 
 *
 *  Description:
 *      this function queries the state of a variable of a 
 *  service, generating a callback when the operation is complete.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpGetServiceVarStatusAsync( IN UpnpClient_Handle Hnd,
                              IN const char *ActionURL_const,
                              IN const char *VarName_const,
                              IN Upnp_FunPtr Fun,
                              IN const void *Cookie_const )
{
    ThreadPoolJob job;
    struct Handle_Info *SInfo = NULL;
    struct UpnpNonblockParam *Param;
    char *ActionURL = ( char * )ActionURL_const;
    char *VarName = ( char * )VarName_const;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpGetServiceVarStatusAsync \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_CLIENT ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }
    HandleUnlock(  );

    if( ActionURL == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }
    if( VarName == NULL || Fun == NULL )
        return UPNP_E_INVALID_PARAM;

    Param =
        ( struct UpnpNonblockParam * )
        malloc( sizeof( struct UpnpNonblockParam ) );
    if( Param == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    Param->FunName = STATUS;
    Param->Handle = Hnd;
    strcpy( Param->Url, ActionURL );
    strcpy( Param->VarName, VarName );
    Param->Fun = Fun;
    Param->Cookie = ( void * )Cookie_const;

    TPJobInit( &job, ( start_routine ) UpnpThreadDistribution, Param );
    TPJobSetFreeFunction( &job, ( free_routine ) free );

    TPJobSetPriority( &job, MED_PRIORITY );

    ThreadPoolAdd( &gSendThreadPool, &job, NULL );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpGetServiceVarStatusAsync \n" );
         )

        return UPNP_E_SUCCESS;

}  /****************** End of UpnpGetServiceVarStatusAsync ****************/

/**************************************************************************
 * Function: UpnpGetServiceVarStatus 
 *
 *  Parameters:	
 *    IN UpnpClient_Handle Hnd: The handle of the control point.
 *    IN const char *ActionURL: The URL of the service. 
 *    IN const char *VarName: The name of the variable to query. 
 *    OUT DOMString *StVarVal: The pointer to store the value 
 *                             for VarName. The UPnP Library 
 *                             allocates this string and the caller 
 *                             needs to free it.
 *  
 *  Description:
 *      this function queries the state of a state 
 *  variable of a service on another device.  This is a synchronous call.
 *  A positive return value indicates a SOAP error code, whereas a negative
 *  return code indicates a UPnP SDK error code.
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpGetServiceVarStatus( IN UpnpClient_Handle Hnd,
                         IN const char *ActionURL_const,
                         IN const char *VarName_const,
                         OUT DOMString * StVar )
{
    struct Handle_Info *SInfo = NULL;
    int retVal = 0;
    char *StVarPtr;
    char *ActionURL = ( char * )ActionURL_const;
    char *VarName = ( char * )VarName_const;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpGetServiceVarStatus \n" );
         )

        HandleLock(  );
    if( GetHandleInfo( Hnd, &SInfo ) != HND_CLIENT ) {
        HandleUnlock(  );
        return UPNP_E_INVALID_HANDLE;
    }

    HandleUnlock(  );

    if( ActionURL == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }
    if( VarName == NULL || StVar == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }

    retVal = SoapGetServiceVarStatus( ActionURL, VarName, &StVarPtr );
    *StVar = StVarPtr;

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpGetServiceVarStatus \n" );
         )

        return retVal;

}  /****************** End of UpnpGetServiceVarStatus *********************/
#endif // INCLUDE_CLIENT_APIS
#endif // EXCLUDE_SOAP

//---------------------------------------------------------------------------
//
//                                   Client API's 
//
//---------------------------------------------------------------------------

/**************************************************************************
 * Function: UpnpOpenHttpPost 
 *
 *  Parameters:	
 *  
 *  Description:
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/

int
UpnpOpenHttpPost( IN const char *url,
                  IN OUT void **handle,
                  IN const char *contentType,
                  IN int contentLength,
                  IN int timeout )
{
    return http_OpenHttpPost( url, handle, contentType, contentLength,
                              timeout );
}

/**************************************************************************
 * Function: UpnpWriteHttpPost 
 *
 *  Parameters:	
 *  
 *  Description:
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpWriteHttpPost( IN void *handle,
                   IN char *buf,
                   IN unsigned int *size,
                   IN int timeout )
{
    return http_WriteHttpPost( handle, buf, size, timeout );
}

/**************************************************************************
 * Function: UpnpCloseHttpPost 
 *
 *  Parameters:	
 *  
 *  Description:
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpCloseHttpPost( IN void *handle,
                   IN OUT int *httpStatus,
                   int timeout )
{
    return http_CloseHttpPost( handle, httpStatus, timeout );
}

/**************************************************************************
 * Function: UpnpOpenHttpGet 
 *
 *  Parameters:	
 *  
 *  Description:
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpOpenHttpGet( IN const char *url_str,
                 IN OUT void **Handle,
                 IN OUT char **contentType,
                 OUT int *contentLength,
                 OUT int *httpStatus,
                 IN int timeout )
{
    return http_OpenHttpGet( url_str, Handle, contentType, contentLength,
                             httpStatus, timeout );
}

/**************************************************************************
 * Function: UpnpOpenHttpGetEx
 *
 *  Parameters:	
 *  
 *  Description:
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpOpenHttpGetEx( IN const char *url_str,
                   IN OUT void **Handle,
                   IN OUT char **contentType,
                   OUT int *contentLength,
                   OUT int *httpStatus,
                   IN int lowRange,
                   IN int highRange,
                   IN int timeout )
{
    return http_OpenHttpGetEx( url_str,
                               Handle,
                               contentType,
                               contentLength,
                               httpStatus, lowRange, highRange, timeout );
}

/**************************************************************************
 * Function: UpnpCloseHttpGet 
 *
 *  Parameters:	
 *  
 *  Description:
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpCloseHttpGet( IN void *Handle )
{
    return http_CloseHttpGet( Handle );
}

/**************************************************************************
 * Function: UpnpReadHttpGet 
 *
 *  Parameters:	
 *  
 *  Description:
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpReadHttpGet( IN void *Handle,
                 IN OUT char *buf,
                 IN OUT unsigned int *size,
                 IN int timeout )
{
    return http_ReadHttpGet( Handle, buf, size, timeout );
}

/**************************************************************************
 * Function: UpnpDownloadUrlItem 
 *
 *  Parameters:	
 *  
 *  Description:
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpDownloadUrlItem( const char *url,
                     char **outBuf,
                     char *contentType )
{
    int ret_code;
    int dummy;

    if( url == NULL || outBuf == NULL || contentType == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }

    ret_code = http_Download( url, HTTP_DEFAULT_TIMEOUT, outBuf, &dummy,
                              contentType );
    if( ret_code > 0 ) {
        // error reply was received
        ret_code = UPNP_E_INVALID_URL;
    }

    return ret_code;
}

/**************************************************************************
 * Function: UpnpDownloadXmlDoc 
 *
 *  Parameters:	
 *  
 *  Description:
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else sends appropriate error.
 ***************************************************************************/
int
UpnpDownloadXmlDoc( const char *url,
                    IXML_Document ** xmlDoc )
{
    int ret_code;
    char *xml_buf;
    char content_type[LINE_SIZE];

    if( url == NULL || xmlDoc == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }

    ret_code = UpnpDownloadUrlItem( url, &xml_buf, content_type );
    if( ret_code != UPNP_E_SUCCESS ) {
        DBGONLY( UpnpPrintf( UPNP_CRITICAL, API, __FILE__, __LINE__,
                             "retCode: %d\n", ret_code );
             )
            return ret_code;
    }

    if( strncasecmp( content_type, "text/xml", strlen( "text/xml" ) ) ) {
        free( xml_buf );
        DBGONLY( UpnpPrintf( UPNP_CRITICAL, API, __FILE__, __LINE__,
                             "Not text/xml\n" );
             )
            return UPNP_E_INVALID_DESC;
    }

    ret_code = ixmlParseBufferEx( xml_buf, xmlDoc );
    free( xml_buf );

    if( ret_code != IXML_SUCCESS ) {
        DBGONLY( UpnpPrintf( UPNP_CRITICAL, API, __FILE__, __LINE__,
                             "Invalid desc\n" );
             )
            if( ret_code == IXML_INSUFFICIENT_MEMORY ) {
            return UPNP_E_OUTOF_MEMORY;
        } else {
            return UPNP_E_INVALID_DESC;
        }
    } else {
        DBGONLY( xml_buf = ixmlPrintDocument( ( IXML_Node * ) * xmlDoc );
                 UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                             "Printing the Parsed xml document \n %s\n",
                             xml_buf );
                 UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                             "****************** END OF Parsed XML Doc *****************\n" );
                 ixmlFreeDOMString( xml_buf );
                 UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                             "Exiting UpnpDownloadXmlDoc\n" ); )

            return UPNP_E_SUCCESS;
    }
}

//----------------------------------------------------------------------------
//
//                          UPNP-API  Internal function implementation
//
//----------------------------------------------------------------------------

#ifdef INCLUDE_CLIENT_APIS

/**************************************************************************
 * Function: UpnpThreadDistribution 
 *
 *  Parameters:	
 *  
 *  Description:
 *		Function to schedule async functions in threadpool.
 *  Return Values: VOID
 *      
 ***************************************************************************/
void
UpnpThreadDistribution( struct UpnpNonblockParam *Param )
{

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside UpnpThreadDistribution \n" );
         )

        switch ( Param->FunName ) {
#if EXCLUDE_GENA == 0
        CLIENTONLY( case SUBSCRIBE:
{
struct Upnp_Event_Subscribe Evt;
Evt.ErrCode = genaSubscribe( Param->Handle, Param->Url,
                            ( int * )&( Param->TimeOut ),
                            ( char * )Evt.Sid );
strcpy( Evt.PublisherUrl, Param->Url ); Evt.TimeOut = Param->TimeOut;
Param->Fun( UPNP_EVENT_SUBSCRIBE_COMPLETE, &Evt, Param->Cookie );
free( Param ); break;}
        case UNSUBSCRIBE:
                            {
                            struct Upnp_Event_Subscribe Evt;
                            Evt.ErrCode =
                            genaUnSubscribe( Param->Handle,
                                             Param->SubsId );
                            strcpy( ( char * )Evt.Sid, Param->SubsId );
                            strcpy( Evt.PublisherUrl, "" );
                            Evt.TimeOut = 0;
                            Param->Fun( UPNP_EVENT_UNSUBSCRIBE_COMPLETE,
                                        &Evt, Param->Cookie );
                            free( Param ); break;}
        case RENEW:
                            {
                            struct Upnp_Event_Subscribe Evt;
                            Evt.ErrCode =
                            genaRenewSubscription( Param->Handle,
                                                   Param->SubsId,
                                                   &( Param->TimeOut ) );
                            Evt.TimeOut = Param->TimeOut;
                            strcpy( ( char * )Evt.Sid, Param->SubsId );
                            Param->Fun( UPNP_EVENT_RENEWAL_COMPLETE, &Evt,
                                        Param->Cookie ); free( Param );
                            break;}
             )
#endif
#if EXCLUDE_SOAP == 0
        case ACTION:
            {
                struct Upnp_Action_Complete Evt;

                Evt.ActionResult = NULL;
#ifdef INCLUDE_CLIENT_APIS

                Evt.ErrCode =
                    SoapSendAction( Param->Url, Param->ServiceType,
                                    Param->Act, &Evt.ActionResult );
#endif

                Evt.ActionRequest = Param->Act;
                strcpy( Evt.CtrlUrl, Param->Url );

                Param->Fun( UPNP_CONTROL_ACTION_COMPLETE, &Evt,
                            Param->Cookie );

                ixmlDocument_free( Evt.ActionRequest );
                ixmlDocument_free( Evt.ActionResult );
                free( Param );
                break;
            }
        case STATUS:
            {
                struct Upnp_State_Var_Complete Evt;

#ifdef INCLUDE_CLIENT_APIS

                Evt.ErrCode = SoapGetServiceVarStatus( Param->Url,
                                                       Param->VarName,
                                                       &( Evt.
                                                          CurrentVal ) );
#endif
                strcpy( Evt.StateVarName, Param->VarName );
                strcpy( Evt.CtrlUrl, Param->Url );

                Param->Fun( UPNP_CONTROL_GET_VAR_COMPLETE, &Evt,
                            Param->Cookie );
                free( Evt.CurrentVal );
                free( Param );
                break;
            }
#endif //EXCLUDE_SOAP
        default:
            break;
    }                           // end of switch(Param->FunName)

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Exiting UpnpThreadDistribution \n" );
         )

}  /****************** End of UpnpThreadDistribution  *********************/
#endif

/**************************************************************************
 * Function: GetCallBackFn 
 *
 *  Parameters:	
 *  
 *  Description:
 *		This function is to get callback function ptr from a handle
 *  Return Values: Upnp_FunPtr
 *      
 ***************************************************************************/
Upnp_FunPtr
GetCallBackFn( UpnpClient_Handle Hnd )
{
    return ( ( struct Handle_Info * )HandleTable[Hnd] )->Callback;

}  /****************** End of GetCallBackFn *********************/

/**************************************************************************
 * Function: InitHandleList 
 *
 *  Parameters:	VOID
 *  
 *  Description:
 *		This function is to initialize handle table
 *  Return Values: VOID
 *      
 ***************************************************************************/
void
InitHandleList(  )
{
    int i;

    for( i = 0; i < NUM_HANDLE; i++ )
        HandleTable[i] = NULL;

}  /****************** End of InitHandleList *********************/

/**************************************************************************
 * Function: GetFreeHandle 
 *
 *  Parameters:	VOID
 *  
 *  Description:
 *		This function is to get a free handle
 *  Return Values: VOID
 *      
 ***************************************************************************/
int
GetFreeHandle(  )
{
    int i = 1;

    /*
       Handle 0 is not used as NULL translates to 0 when passed as a handle 
     */
    while( i < NUM_HANDLE ) {
        if( HandleTable[i++] == NULL )
            break;
    }

    if( i == NUM_HANDLE )
        return UPNP_E_OUTOF_HANDLE; //Error
    else
        return --i;

}  /****************** End of GetFreeHandle *********************/

/**************************************************************************
 * Function: GetClientHandleInfo 
 *
 *  Parameters:	
 * 		IN UpnpClient_Handle *client_handle_out: client handle pointer ( key 
 *													for the client handle
 *													structure).
 *		OUT struct Handle_Info **HndInfo: Client handle structure passed by 
 *											this function.
 *
 *  Description:
 *		This function is to get client handle info
 *
 *  Return Values: HND_CLIENT
 *      
 ***************************************************************************/
//Assumes at most one client
Upnp_Handle_Type
GetClientHandleInfo( IN UpnpClient_Handle * client_handle_out,
                     OUT struct Handle_Info ** HndInfo )
{
    ( *client_handle_out ) = 1;
    if( GetHandleInfo( 1, HndInfo ) == HND_CLIENT ) {
        return HND_CLIENT;
    }
    ( *client_handle_out ) = 2;
    if( GetHandleInfo( 2, HndInfo ) == HND_CLIENT ) {
        return HND_CLIENT;
    }
    ( *client_handle_out ) = -1;
    return HND_INVALID;

}  /****************** End of GetClientHandleInfo *********************/

/**************************************************************************
 * Function: GetDeviceHandleInfo 
 *
 *  Parameters:	
 * 		IN UpnpDevice_Handle * device_handle_out: device handle pointer ( key 
 *													for the client handle
 *													structure).
 *		OUT struct Handle_Info **HndInfo: Device handle structure passed by 
 *											this function.
 *  
 *  Description:
 *		This function is to get device handle info.
 *  Return Values: HND_DEVICE
 *      
 ***************************************************************************/
Upnp_Handle_Type
GetDeviceHandleInfo( UpnpDevice_Handle * device_handle_out,
                     struct Handle_Info ** HndInfo )
{
    ( *device_handle_out ) = 1;
    if( GetHandleInfo( 1, HndInfo ) == HND_DEVICE )
        return HND_DEVICE;

    ( *device_handle_out ) = 2;
    if( GetHandleInfo( 2, HndInfo ) == HND_DEVICE )
        return HND_DEVICE;
    ( *device_handle_out ) = -1;

    return HND_INVALID;

}  /****************** End of GetDeviceHandleInfo *********************/

/**************************************************************************
 * Function: GetDeviceHandleInfo 
 *
 *  Parameters:	
 * 		IN UpnpClient_Handle * device_handle_out: handle pointer ( key 
 *													for the client handle
 *													structure).
 *		OUT struct Handle_Info **HndInfo: handle structure passed by 
 *											this function.
 *  
 *  Description:
 *		This function is to get  handle info.
 *  Return Values: HND_DEVICE
 *      
 ***************************************************************************/
Upnp_Handle_Type
GetHandleInfo( UpnpClient_Handle Hnd,
               struct Handle_Info ** HndInfo )
{

    DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                         "GetHandleInfo: Handle is %d\n", Hnd );
         )

        if( Hnd < 1 || Hnd >= NUM_HANDLE ) {

        DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "GetHandleInfo : Handle out of range\n" );
             )
            return UPNP_E_INVALID_HANDLE;
    }
    if( HandleTable[Hnd] != NULL ) {

        *HndInfo = ( struct Handle_Info * )HandleTable[Hnd];
        return ( ( struct Handle_Info * )*HndInfo )->HType;
    }

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "GetHandleInfo : exiting\n" );
         )

        return UPNP_E_INVALID_HANDLE;

}  /****************** End of GetHandleInfo *********************/

/**************************************************************************
 * Function: FreeHandle 
 *
 *  Parameters:	
 * 		IN int Upnp_Handle: handle index 
 *  
 *  Description:
 *		This function is to to free handle info.
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else return appropriate error
 ***************************************************************************/
int
FreeHandle( int Upnp_Handle )
{
    if( Upnp_Handle < 1 || Upnp_Handle >= NUM_HANDLE ) {
        DBGONLY( UpnpPrintf( UPNP_CRITICAL, API, __FILE__, __LINE__,
                             "FreeHandleInfo : Handle out of range\n" );
             )
            return UPNP_E_INVALID_HANDLE;
    }

    if( HandleTable[Upnp_Handle] == NULL ) {
        return UPNP_E_INVALID_HANDLE;
    }
    free( HandleTable[Upnp_Handle] );
    HandleTable[Upnp_Handle] = NULL;
    return UPNP_E_SUCCESS;

}  /****************** End of FreeHandle *********************/

// **DBG****************************************************
DBGONLY(

/**************************************************************************
 * Function: PrintHandleInfo 
 *
 *  Parameters:	
 * 		IN UpnpClient_Handle Hnd: handle index 
 *  
 *  Description:
 *		This function is to print handle info.
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else return appropriate error
 ***************************************************************************/
            int PrintHandleInfo( IN UpnpClient_Handle Hnd ) {
            struct Handle_Info * HndInfo; if( HandleTable[Hnd] != NULL ) {
            HndInfo = HandleTable[Hnd];
            DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                                 "Printing information for Handle_%d\n",
                                 Hnd );
                     UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                                 "HType_%d\n", HndInfo->HType );
		     DEVICEONLY(
                     if( HndInfo->HType !=
                         HND_CLIENT ) UpnpPrintf( UPNP_ALL, API, __FILE__,
                                                  __LINE__, "DescURL_%s\n",
                                                  HndInfo->DescURL ); )
		     )
            }
            else
            {
            return UPNP_E_INVALID_HANDLE;}

            return UPNP_E_SUCCESS;}
   /****************** End of PrintHandleInfo *********************/

            void printNodes( IXML_Node * tmpRoot, int depth ) {
            int i;
            IXML_NodeList * NodeList1;
            IXML_Node * ChildNode1;
            unsigned short NodeType;
            DOMString NodeValue;
            const DOMString NodeName;
            NodeList1 = ixmlNode_getChildNodes( tmpRoot );
            for( i = 0; i < 100; i++ ) {
            ChildNode1 = ixmlNodeList_item( NodeList1, i );
            if( ChildNode1 == NULL ) {
            break;}

            printNodes( ChildNode1, depth + 1 );
            NodeType = ixmlNode_getNodeType( ChildNode1 );
            NodeValue = ixmlNode_getNodeValue( ChildNode1 );
            NodeName = ixmlNode_getNodeName( ChildNode1 );
            DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                                 "DEPTH-%2d-IXML_Node Type %d, "
                                 "IXML_Node Name: %s, IXML_Node Value: %s\n",
                                 depth, NodeType, NodeName, NodeValue ); )
            }

            }
   /****************** End of printNodes *********************/

 )                              // dbgonly

    //********************************************************
    //* Name: getlocalhostname
    //* Description:  Function to get local IP address
    //*               Gets the ip address for the DEFAULT_INTERFACE 
    //*               interface which is up and not a loopback
    //*               assumes at most MAX_INTERFACES interfaces
    //* Called by:    UpnpInit
    //* In:           char *out
    //* Out:          Ip address
    //* Return codes: UPNP_E_SUCCESS
    //* Error codes:  UPNP_E_INIT
    //********************************************************

 /**************************************************************************
 * Function: getlocalhostname 
 *
 *  Parameters:	
 * 		OUT char *out: IP address of the interface.
 *  
 *  Description:
 *		This function is to get local IP address. It gets the ip address for 
 *	the DEFAULT_INTERFACE interface which is up and not a loopback
 *	assumes at most MAX_INTERFACES interfaces
 *
 *  Return Values: int
 *      UPNP_E_SUCCESS if successful else return appropriate error
 ***************************************************************************/
    int getlocalhostname( OUT char *out ) {

    char szBuffer[MAX_INTERFACES * sizeof( struct ifreq )];
    struct ifconf ifConf;
    struct ifreq ifReq;
    int nResult;
    int i;
    int LocalSock;
    struct sockaddr_in LocalAddr;
    int j = 0;

    // Create an unbound datagram socket to do the SIOCGIFADDR ioctl on. 
    if( ( LocalSock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ) ) < 0 ) {
        DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                             "Can't create addrlist socket\n" );
             )
            return UPNP_E_INIT;
    }
    // Get the interface configuration information... 
    ifConf.ifc_len = sizeof szBuffer;
    ifConf.ifc_ifcu.ifcu_buf = ( caddr_t ) szBuffer;
    nResult = ioctl( LocalSock, SIOCGIFCONF, &ifConf );

    if( nResult < 0 ) {
        DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                             "DiscoverInterfaces: SIOCGIFCONF returned error\n" );
             )

            return UPNP_E_INIT;
    }
    // Cycle through the list of interfaces looking for IP addresses. 

    for( i = 0; ( ( i < ifConf.ifc_len ) && ( j < DEFAULT_INTERFACE ) ); ) {
        struct ifreq *pifReq =
            ( struct ifreq * )( ( caddr_t ) ifConf.ifc_req + i );
        i += sizeof *pifReq;

        // See if this is the sort of interface we want to deal with.
        strcpy( ifReq.ifr_name, pifReq->ifr_name );
        if( ioctl( LocalSock, SIOCGIFFLAGS, &ifReq ) < 0 ) {
            DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                                 "Can't get interface flags for %s:\n",
                                 ifReq.ifr_name );
                 )

        }
        // Skip loopback, point-to-point and down interfaces, 
        // except don't skip down interfaces
        // if we're trying to get a list of configurable interfaces. 
        if( ( ifReq.ifr_flags & IFF_LOOPBACK )
            || ( !( ifReq.ifr_flags & IFF_UP ) ) ) {
            continue;
        }
        if( pifReq->ifr_addr.sa_family == AF_INET ) {
            // Get a pointer to the address...
            memcpy( &LocalAddr, &pifReq->ifr_addr,
                    sizeof pifReq->ifr_addr );

            // We don't want the loopback interface. 
            if( LocalAddr.sin_addr.s_addr == htonl( INADDR_LOOPBACK ) ) {
                continue;
            }

        }
        //increment j if we found an address which is not loopback
        //and is up
        j++;

    }
    close( LocalSock );

    strncpy( out, inet_ntoa( LocalAddr.sin_addr ), LINE_SIZE );

    DBGONLY( UpnpPrintf( UPNP_ALL, API, __FILE__, __LINE__,
                         "Inside getlocalhostname : after strncpy %s\n",
                         out );
         )
        return UPNP_E_SUCCESS;
    }

#ifdef INCLUDE_DEVICE_APIS
#if EXCLUDE_SSDP == 0

 /**************************************************************************
 * Function: AutoAdvertise 
 *
 *  Parameters:	
 * 		IN void *input: information provided to the thread.
 *  
 *  Description:
 *		This function is a timer thread scheduled by UpnpSendAdvertisement 
 *	to the send advetisement again. 
 *
 *  Return Values: VOID
 *     
 ***************************************************************************/
void
AutoAdvertise( void *input )
{
    upnp_timeout *event = ( upnp_timeout * ) input;

    UpnpSendAdvertisement( event->handle, *( ( int * )event->Event ) );
    free_upnp_timeout( event );
}
#endif //INCLUDE_DEVICE_APIS
#endif

/*
 **************************** */
#ifdef INTERNAL_WEB_SERVER

 /**************************************************************************
 * Function: UpnpSetWebServerRootDir 
 *
 *  Parameters:	
 *   IN const char* rootDir:Path of the root directory of the web 
 *                          server. 
 *  
 *  Description:
 *		This function sets the document root directory for
 *  the internal web server. This directory is considered the
 *  root directory (i.e. "/") of the web server.
 *  This function also activates or deactivates the web server.
 *  To disable the web server, pass NULL for rootDir to 
 *  activate, pass a valid directory string.
 *  
 *  Note that this function is not available when the web server is not
 *  compiled into the UPnP Library.
 *
 *  Return Values: int
 *     UPNP_E_SUCCESS if successful else returns appropriate error
 ***************************************************************************/
int
UpnpSetWebServerRootDir( IN const char *rootDir )
{
    if( UpnpSdkInit == 0 )
        return UPNP_E_FINISH;
    if( ( rootDir == NULL ) || ( strlen( rootDir ) == 0 ) ) {
        return UPNP_E_INVALID_PARAM;
    }

    membuffer_destroy( &gDocumentRootDir );

    return ( web_server_set_root_dir( rootDir ) );
}
#endif // INTERNAL_WEB_SERVER
/*
 *************************** */

 /**************************************************************************
 * Function: UpnpAddVirtualDir 
 *
 *  Parameters:	
 *   IN const char *newDirName:The name of the new directory mapping to add.
 *  
 *  Description:
 *		This function adds a virtual directory mapping.
 *
 *  All webserver requests containing the given directory are read using
 *  functions contained in a UpnpVirtualDirCallbacks structure registered
 *  via UpnpSetVirtualDirCallbacks.
 *  
 *  Note that this function is not available when the web server is not
 *  compiled into the UPnP Library.
 *
 *  Return Values: int
 *     UPNP_E_SUCCESS if successful else returns appropriate error
 ***************************************************************************/
int
UpnpAddVirtualDir( IN const char *newDirName )
{

    virtualDirList *pNewVirtualDir,
     *pLast;
    virtualDirList *pCurVirtualDir;
    char dirName[NAME_SIZE];

    if( UpnpSdkInit != 1 ) {
        // SDK is not initialized
        return UPNP_E_FINISH;
    }

    if( ( newDirName == NULL ) || ( strlen( newDirName ) == 0 ) ) {
        return UPNP_E_INVALID_PARAM;
    }

    if( *newDirName != '/' ) {
        dirName[0] = '/';
        strcpy( dirName + 1, newDirName );
    } else {
        strcpy( dirName, newDirName );
    }

    pCurVirtualDir = pVirtualDirList;
    while( pCurVirtualDir != NULL ) {
        // already has this entry
        if( strcmp( pCurVirtualDir->dirName, dirName ) == 0 ) {
            return UPNP_E_SUCCESS;
        }

        pCurVirtualDir = pCurVirtualDir->next;
    }

    pNewVirtualDir =
        ( virtualDirList * ) malloc( sizeof( virtualDirList ) );
    if( pNewVirtualDir == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }
    pNewVirtualDir->next = NULL;
    strcpy( pNewVirtualDir->dirName, dirName );
    *( pNewVirtualDir->dirName + strlen( dirName ) ) = 0;

    if( pVirtualDirList == NULL ) { // first virtual dir
        pVirtualDirList = pNewVirtualDir;
    } else {
        pLast = pVirtualDirList;
        while( pLast->next != NULL ) {
            pLast = pLast->next;
        }
        pLast->next = pNewVirtualDir;
    }

    return UPNP_E_SUCCESS;
}

 /**************************************************************************
 * Function: UpnpRemoveVirtualDir 
 *
 *  Parameters:	
 *   IN const char *newDirName:The name of the directory mapping to remove.
 *  
 *  Description:
 *		This function removes a virtual directory mapping.
 *
 *  Return Values: int
 *     UPNP_E_SUCCESS if successful else returns appropriate error
 ***************************************************************************/
int
UpnpRemoveVirtualDir( IN const char *dirName )
{

    virtualDirList *pPrev;
    virtualDirList *pCur;
    int found = 0;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    if( dirName == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }

    if( pVirtualDirList == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }
    //
    // Handle the special case where the directory that we are
    // removing is the first and only one in the list.
    //

    if( ( pVirtualDirList->next == NULL ) &&
        ( strcmp( pVirtualDirList->dirName, dirName ) == 0 ) ) {
        free( pVirtualDirList );
        pVirtualDirList = NULL;
        return UPNP_E_SUCCESS;
    }

    pCur = pVirtualDirList;
    pPrev = pCur;

    while( pCur != NULL ) {
        if( strcmp( pCur->dirName, dirName ) == 0 ) {
            pPrev->next = pCur->next;
            free( pCur );
            found = 1;
            break;
        } else {
            pPrev = pCur;
            pCur = pCur->next;
        }
    }

    if( found == 1 )
        return UPNP_E_SUCCESS;
    else
        return UPNP_E_INVALID_PARAM;

}

 /**************************************************************************
 * Function: UpnpRemoveAllVirtualDirs 
 *
 *  Parameters:	VOID
 *  
 *  Description:
 *		This function removes all the virtual directory mappings.
 *
 *  Return Values: VOID
 *     
 ***************************************************************************/
void
UpnpRemoveAllVirtualDirs(  )
{

    virtualDirList *pCur;
    virtualDirList *pNext;

    if( UpnpSdkInit != 1 ) {
        return;
    }

    pCur = pVirtualDirList;

    while( pCur != NULL ) {
        pNext = pCur->next;
        free( pCur );

        pCur = pNext;
    }

    pVirtualDirList = NULL;

}

 /**************************************************************************
 * Function: UpnpEnableWebserver 
 *
 *  Parameters:	
 *		IN int enable: TRUE to enable, FALSE to disable.
 *  
 *  Description:
 *		This function enables or disables the webserver.  A value of
 *  TRUE enables the webserver, FALSE disables it.
 *
 *  Return Values: int
 *     UPNP_E_SUCCESS if successful else returns appropriate error
 ***************************************************************************/
int
UpnpEnableWebserver( IN int enable )
{
    int retVal;

    if( UpnpSdkInit != 1 ) {
        return UPNP_E_FINISH;
    }

    switch ( enable ) {
#ifdef INTERNAL_WEB_SERVER
        case TRUE:
            if( ( retVal = web_server_init(  ) ) != UPNP_E_SUCCESS ) {
                return retVal;
            }
            bWebServerState = WEB_SERVER_ENABLED;
            SetHTTPGetCallback( web_server_callback );
            break;

        case FALSE:
            web_server_destroy(  );
            bWebServerState = WEB_SERVER_DISABLED;
            SetHTTPGetCallback( NULL );
            break;
#endif
        default:
            return UPNP_E_INVALID_PARAM;
    }

    return UPNP_E_SUCCESS;
}

 /**************************************************************************
 * Function: UpnpIsWebserverEnabled 
 *
 *  Parameters:	VOID
 *  
 *  Description:
 *		This function  checks if the webserver is enabled or disabled. 
 *
 *  Return Values: int
 *      1, if webserver enabled
 *		0, if webserver disabled
 ***************************************************************************/
int
UpnpIsWebserverEnabled(  )
{
    if( UpnpSdkInit != 1 ) {
        return 0;
    }

    return ( bWebServerState == WEB_SERVER_ENABLED );
}

 /**************************************************************************
 * Function: UpnpSetVirtualDirCallbacks 
 *
 *  Parameters:	
 *		IN struct UpnpVirtualDirCallbacks *callbacks:a structure that 
 *									contains the callback functions.
 *	
 *  Description:
 *		This function sets the callback function to be used to 
 *  access a virtual directory.
 *
 *  Return Values: int
 *		UPNP_E_SUCCESS on success, or UPNP_E_INVALID_PARAM
 ***************************************************************************/
int
UpnpSetVirtualDirCallbacks( IN struct UpnpVirtualDirCallbacks *callbacks )
{
    struct UpnpVirtualDirCallbacks *pCallback;

    if( UpnpSdkInit != 1 ) {
        // SDK is not initialized
        return UPNP_E_FINISH;
    }

    pCallback = &virtualDirCallback;

    if( callbacks == NULL )
        return UPNP_E_INVALID_PARAM;

    pCallback->get_info = callbacks->get_info;
    pCallback->open = callbacks->open;
    pCallback->close = callbacks->close;
    pCallback->read = callbacks->read;
    pCallback->write = callbacks->write;
    pCallback->seek = callbacks->seek;

    return UPNP_E_SUCCESS;
}

 /**************************************************************************
 * Function: UpnpFree 
 *
 *  Parameters:	
 *		IN void *item:The item to free.
 *	
 *  Description:
 *		This function free the memory allocated by tbe UPnP library
 *
 *  Return Values: VOID
 *		
 ***************************************************************************/
void
UpnpFree( IN void *item )
{
    if( item )
        free( item );
}


/**************************************************************************
 * Function: UpnpSetContentLength
 * OBSOLETE METHOD : use {\bf UpnpSetMaxContentLength} instead.
 ***************************************************************************/
int
UpnpSetContentLength( IN UpnpClient_Handle Hnd,
                               /** The handle of the device instance
                                  for which the coincoming content length needs
                                  to be set. */

                      IN int contentLength
                               /** Permissible content length  */
     )
{
    int errCode = UPNP_E_SUCCESS;
    struct Handle_Info *HInfo = NULL;

    do {
        if( UpnpSdkInit != 1 ) {
            errCode = UPNP_E_FINISH;
            break;
        }

        HandleLock(  );

        errCode = GetHandleInfo( Hnd, &HInfo );

        if( errCode != HND_DEVICE ) {
            errCode = UPNP_E_INVALID_HANDLE;
            break;
        }

        if( contentLength > MAX_SOAP_CONTENT_LENGTH ) {
            errCode = UPNP_E_OUTOF_BOUNDS;
            break;
        }
	
        g_maxContentLength = contentLength;

    } while( 0 );

    HandleUnlock(  );
    return errCode;

}


/**************************************************************************
 * Function: UpnpSetMaxContentLength
 *
 *  Parameters:	
 *      IN int contentLength                The maximum size to be set 
 *	
 *  Description:
 *      Sets the maximum content-length that the SDK will process on an 
 *      incoming SOAP requests or responses. This API allows devices that have
 *      memory constraints to exhibit consistent behaviour if the size of the 
 *      incoming SOAP message exceeds the memory that device can allocate. 
 *      The default maximum content-length is {\tt DEFAULT_SOAP_CONTENT_LENGTH}
 *      = 16K bytes.
 *
 *  Return Values: int :
 *    UPNP_E_SUCCESS            : The operation completed successfully.
 *		
 ***************************************************************************/
int
UpnpSetMaxContentLength (
                      IN size_t contentLength
                               /** Permissible content length, in bytes  */
     )
{
    int errCode = UPNP_E_SUCCESS;

    do {
        if( UpnpSdkInit != 1 ) {
            errCode = UPNP_E_FINISH;
            break;
        }

        g_maxContentLength = contentLength;

    } while( 0 );

    return errCode;

}

/*********************** END OF FILE upnpapi.c :) ************************/
