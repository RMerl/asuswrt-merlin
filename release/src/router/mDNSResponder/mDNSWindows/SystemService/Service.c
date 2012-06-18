/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

    Change History (most recent first):
    
$Log: Service.c,v $
Revision 1.49  2009/07/17 19:59:46  herscher
<rdar://problem/7062660> Update the womp settings for each network adapter immediately preceding the call to mDNSCoreMachineSleep().

Revision 1.48  2009/07/09 21:34:14  herscher
<rdar://problem/3775717> SDK: Port mDNSNetMonitor to Windows. Refactor the system service slightly by removing the main() function from Service.c so that mDNSNetMonitor can link to functions defined in Service.c

Revision 1.47  2009/07/07 21:35:06  herscher
<rdar://problem/6713286> windows platform changes to support use as sleep proxy client

Revision 1.46  2009/06/05 18:28:24  herscher
<rdar://problem/6125087> mDNSResponder should be able to identify VPN adapters generically
<rdar://problem/6885843> WIN7: Bonjour removes the default gateway entry and thereby breaks network connectivity

Revision 1.45  2009/04/30 20:07:51  mcguire
<rdar://problem/6822674> Support multiple UDSs from launchd

Revision 1.44  2009/03/30 20:41:36  herscher
<rdar://problem/5925472> Current Bonjour code does not compile on Windows
<rdar://problem/6330821> Bonjour for Windows incompatible w/Juniper Network Connect. Do a substring search for "Juniper" in the description field
 of the network adapter.
<rdar://problem/6122028> Bonjour for Windows incompatible w/Cisco AnyConnect VPN Client
<rdar://problem/5652098> Bonjour for Windows incompatible w/Juniper Network Connect. Update the adapter name per info received from Juniper
<rdar://problem/5781566> Core: Default Gateway set to 0.0.0.0 on Vista causes ARP flood
<rdar://problem/5991983> Change the registry values from Apple Computer, Inc. to Apple Inc.
<rdar://problem/5301328> wchar/sizeof mismatch in CheckFirewall()

Revision 1.43  2009/01/13 05:31:35  mkrochma
<rdar://problem/6491367> Replace bzero, bcopy with mDNSPlatformMemZero, mDNSPlatformMemCopy, memset, memcpy

Revision 1.42  2007/02/14 01:58:19  cheshire
<rdar://problem/4995831> Don't delete Unix Domain Socket on exit if we didn't create it on startup

Revision 1.41  2007/02/06 19:06:49  cheshire
<rdar://problem/3956518> Need to go native with launchd

Revision 1.40  2007/01/05 05:46:08  cheshire
Add mDNS *const m parameter to udsserver_handle_configchange()

Revision 1.39  2006/08/14 23:26:07  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.38  2005/10/05 20:55:15  herscher
<rdar://problem/4096464> Don't call SetLLRoute on loopback interface

Revision 1.37  2005/10/05 18:05:28  herscher
<rdar://problem/4192011> Save Wide-Area preferences in a different spot in the registry so they don't get removed when doing an update install.

Revision 1.36  2005/09/11 22:12:42  herscher
<rdar://problem/4247793> Remove dependency on WMI.  Ensure that the Windows firewall is turned on before trying to configure it.

Revision 1.35  2005/06/30 18:29:49  shersche
<rdar://problem/4090059> Don't overwrite the localized service description text

Revision 1.34  2005/04/22 07:34:23  shersche
Check an interface's address and make sure it's valid before using it to set link-local routes.

Revision 1.33  2005/04/13 17:48:23  shersche
<rdar://problem/4079667> Make sure there is only one default route for link-local addresses.

Revision 1.32  2005/04/06 01:32:05  shersche
Remove default route for link-local addressing when another interface comes up with a routable IPv4 address

Revision 1.31  2005/04/06 01:00:11  shersche
<rdar://problem/4080127> GetFullPathName() should be passed the number of TCHARs in the path buffer, not the size in bytes of the path buffer.

Revision 1.30  2005/04/06 00:52:43  shersche
<rdar://problem/4079667> Only add default route if there are no other routable IPv4 addresses on any of the other interfaces. More work needs to be done to correctly configure the routing table when multiple interfaces are extant and none of them have routable IPv4 addresses.

Revision 1.29  2005/03/06 05:21:56  shersche
<rdar://problem/4037635> Fix corrupt UTF-8 name when non-ASCII system name used, enabled unicode support

Revision 1.28  2005/03/03 02:27:24  shersche
Include the RegNames.h header file for names of registry keys

Revision 1.27  2005/03/02 20:12:59  shersche
Update name

Revision 1.26  2005/02/15 08:00:27  shersche
<rdar://problem/4007151> Update name

Revision 1.25  2005/02/10 22:35:36  cheshire
<rdar://problem/3727944> Update name

Revision 1.24  2005/01/27 20:02:43  cheshire
udsSupportRemoveFDFromEventLoop() needs to close the SocketRef as well

Revision 1.23  2005/01/25 08:14:15  shersche
Change CacheRecord to CacheEntity

Revision 1.22  2004/12/10 13:18:40  cheshire
Create no-op function RecordUpdatedNiceLabel(), required by uds_daemon.c

Revision 1.21  2004/11/10 04:03:41  shersche
Remove SharedAccess dependency.  This causes problems on XP SP1, and isn't necessary for XP SP2 because we already are dependent on WMI, which itself is dependent on SharedAccess.

Revision 1.20  2004/10/14 21:44:05  shersche
<rdar://problem/3838237> Fix a race condition between the socket thread and the main processing thread that resulted in the socket thread accessing a previously deleted Win32EventSource object.
Bug #: 3838237

Revision 1.19  2004/10/12 17:59:55  shersche
<rdar://problem/3718122> Disable routing table modifications when Nortel VPN adapter is active
Bug #: 3718122

Revision 1.18  2004/10/11 21:57:50  shersche
<rdar://problem/3832450> The SharedAccess service dependency causes a circular dependency on Windows Server 2003.  Only add the SharedAccess service dependency if running on XP.  All other platforms do not manipulate the firewall and thus are not dependent on it.
Bug #: 3832450

Revision 1.17  2004/09/17 01:08:58  cheshire
Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
  The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
  declared in that file are ONLY appropriate to single-address-space embedded applications.
  For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.

Revision 1.16  2004/09/16 18:49:34  shersche
Remove the XP SP2 check before attempting to manage the firewall. There is a race condition in the SP2 updater such that upon first reboot after the upgrade, mDNSResponder might not know that it is running under SP2 yet.  This necessitates a second reboot before the firewall is managed.  Removing the check will cause mDNSResponder to try and manage the firewall everytime it boots up, if and only if it hasn't managed the firewall a previous time.

Revision 1.15  2004/09/15 17:13:33  shersche
Change Firewall name

Revision 1.14  2004/09/15 09:37:25  shersche
Add SharedAccess to dependency list, call CheckFirewall after sending status back to SCM

Revision 1.13  2004/09/13 07:35:10  shersche
<rdar://problem/3762235> Add mDNSResponder to Windows Firewall application list if SP2 is detected and app hasn't been added before
Bug #: 3762235

Revision 1.12  2004/09/11 21:18:32  shersche
<rdar://problem/3779502> Add route to ARP everything when a 169.254.x.x address is selected
Bug #: 3779502

Revision 1.11  2004/09/11 05:39:19  shersche
<rdar://problem/3780203> Detect power managment state changes, calling mDNSCoreMachineSleep(m, true) on sleep, and mDNSCoreMachineSleep(m, false) on resume
Bug #: 3780203

Revision 1.10  2004/08/16 21:45:24  shersche
Use the full pathname of executable when calling CreateService()
Submitted by: prepin@zetron.com

Revision 1.9  2004/08/11 01:59:41  cheshire
Remove "mDNS *globalInstance" parameter from udsserver_init()

Revision 1.8  2004/08/05 05:40:05  shersche
<rdar://problem/3751566> Only invoke SetConsoleCtrlHandler when running directly from command line.
<rdar://problem/3751481> Invoke udsserver_handle_configchange() when the computer description changes
Bug #: 3751481, 3751566

Revision 1.7  2004/07/26 05:35:07  shersche
ignore non-enet interfaces when setting up link-local routing

Revision 1.6  2004/07/20 06:48:26  shersche
<rdar://problem/3718122> Allow registry entries to dictate whether to manage link local routing
Bug #: 3718122

Revision 1.5  2004/07/09 19:08:07  shersche
<rdar://problem/3713762> ServiceSetupEventLogging() errors are handled gracefully
Bug #: 3713762

Revision 1.4  2004/06/24 20:58:15  shersche
Fix compiler error in Release build
Submitted by: herscher

Revision 1.3  2004/06/24 15:28:53  shersche
Automatically setup routes to link-local addresses upon interface list change events.
Submitted by: herscher

Revision 1.2  2004/06/23 16:56:00  shersche
<rdar://problem/3697326> locked call to udsserver_idle().
Bug #: 3697326
Submitted by: herscher

Revision 1.1  2004/06/18 04:16:41  rpantos
Move up one level.

Revision 1.1  2004/01/30 02:58:39  bradley
mDNSResponder Windows Service. Provides global Bonjour support with an IPC interface.

*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<stddef.h>


#include	"CommonServices.h"
#include	"DebugServices.h"
#include	"RegNames.h"

#include	"uds_daemon.h"
#include	"GenLinkedList.h"
#include	"Service.h"

#include	"Resource.h"

#include	"mDNSEmbeddedAPI.h"
#include	"mDNSWin32.h"

#include	"Firewall.h"

#if( !TARGET_OS_WINDOWS_CE )
	#include	<mswsock.h>
	#include	<process.h>
	#include	<ipExport.h>
	#include	<ws2def.h>
	#include	<ws2ipdef.h>
	#include	<iphlpapi.h>
	#include	<netioapi.h>
	#include	<iptypes.h>
	#include	<powrprof.h>
#endif

#ifndef HeapEnableTerminationOnCorruption
#	define HeapEnableTerminationOnCorruption (HEAP_INFORMATION_CLASS)1
#endif

#if 0
#pragma mark == Constants ==
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

#define	DEBUG_NAME							"[Server] "
#define kServiceFirewallName				L"Bonjour"
#define	kServiceDependencies				TEXT("Tcpip\0\0")
#define	kDNSServiceCacheEntryCountDefault	512
#define kRetryFirewallPeriod				30 * 1000
#define kDefValueSize						MAX_PATH + 1
#define kZeroIndex							0
#define kDefaultRouteMetric					399

#define RR_CACHE_SIZE 500
static CacheEntity gRRCache[RR_CACHE_SIZE];
#if 0
#pragma mark == Structures ==
#endif

//===========================================================================================================================
//	Structures
//===========================================================================================================================
//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	EventSourceFlags

	@abstract	Session flags.
	
	@constant	EventSourceFlagsNone			No flags.
	@constant	EventSourceFlagsThreadDone		Thread is no longer active.
	@constant	EventSourceFlagsNoClose			Do not close the session when the thread exits.
	@constant	EventSourceFinalized			Finalize has been called for this session
*/

typedef uint32_t		EventSourceFlags;

#define	EventSourceFlagsNone			0
#define	EventSourceFlagsThreadDone		( 1 << 2 )
#define	EventSourceFlagsNoClose			( 1 << 3 )
#define EventSourceFinalized			( 1 << 4 )


typedef struct Win32EventSource
{
	EventSourceFlags			flags;
	HANDLE						threadHandle;
	unsigned					threadID;
	HANDLE						socketEvent;
	HANDLE						closeEvent;
	udsEventCallback			callback;
	void					*	context;
	DWORD						waitCount;
	HANDLE						waitList[2];
	SOCKET						sock;
	struct Win32EventSource	*	next;
} Win32EventSource;


#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================
static void			Usage( void );
static BOOL WINAPI	ConsoleControlHandler( DWORD inControlEvent );
static OSStatus		InstallService( LPCTSTR inName, LPCTSTR inDisplayName, LPCTSTR inDescription, LPCTSTR inPath );
static OSStatus		RemoveService( LPCTSTR inName );
static OSStatus		SetServiceParameters();
static OSStatus		GetServiceParameters();
static OSStatus		CheckFirewall();
static OSStatus		SetServiceInfo( SC_HANDLE inSCM, LPCTSTR inServiceName, LPCTSTR inDescription );
static void			ReportStatus( int inType, const char *inFormat, ... );

static void WINAPI	ServiceMain( DWORD argc, LPTSTR argv[] );
static OSStatus		ServiceSetupEventLogging( void );
static DWORD WINAPI	ServiceControlHandler( DWORD inControl, DWORD inEventType, LPVOID inEventData, LPVOID inContext );

static OSStatus		ServiceRun( int argc, LPTSTR argv[] );
static void			ServiceStop( void );

static OSStatus		ServiceSpecificInitialize( int argc, LPTSTR  argv[] );
static OSStatus		ServiceSpecificRun( int argc, LPTSTR argv[] );
static OSStatus		ServiceSpecificStop( void );
static void			ServiceSpecificFinalize( int argc, LPTSTR argv[] );
static mStatus		EventSourceFinalize(Win32EventSource * source);
static void			EventSourceLock();
static void			EventSourceUnlock();
static mDNSs32		udsIdle(mDNS * const inMDNS, mDNSs32 interval);
static void			CoreCallback(mDNS * const inMDNS, mStatus result);
static void			HostDescriptionChanged(mDNS * const inMDNS);
static mDNSu8		SystemWakeForNetworkAccess( LARGE_INTEGER * timeout );
static OSStatus		GetRouteDestination(DWORD * ifIndex, DWORD * address);
static OSStatus		SetLLRoute( mDNS * const inMDNS );
static bool			HaveRoute( PMIB_IPFORWARDROW rowExtant, unsigned long addr, unsigned long metric );
static bool			IsValidAddress( const char * addr );
static bool			IsNortelVPN( IP_ADAPTER_INFO * pAdapter );
static bool			IsJuniperVPN( IP_ADAPTER_INFO * pAdapter );
static bool			IsCiscoVPN( IP_ADAPTER_INFO * pAdapter );
static const char * strnistr( const char * string, const char * subString, size_t max );

#if defined(UNICODE)
#	define StrLen(X)	wcslen(X)
#	define StrCmp(X,Y)	wcscmp(X,Y)
#else
#	define StrLen(X)	strlen(X)
#	define StrCmp(X,Y)	strcmp(X,Y)
#endif


#define kLLNetworkAddr      "169.254.0.0"
#define kLLNetworkAddrMask  "255.255.0.0"


#include	"mDNSEmbeddedAPI.h"

#if 0
#pragma mark == Globals ==
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================
#define gMDNSRecord mDNSStorage
DEBUG_LOCAL	mDNS_PlatformSupport		gPlatformStorage;
DEBUG_LOCAL BOOL						gServiceQuietMode		= FALSE;
DEBUG_LOCAL SERVICE_TABLE_ENTRY			gServiceDispatchTable[] = 
{
	{ kServiceName,	ServiceMain }, 
	{ NULL, 		NULL }
};
DEBUG_LOCAL SERVICE_STATUS				gServiceStatus;
DEBUG_LOCAL SERVICE_STATUS_HANDLE		gServiceStatusHandle 	= NULL;
DEBUG_LOCAL HANDLE						gServiceEventSource		= NULL;
DEBUG_LOCAL bool						gServiceAllowRemote		= false;
DEBUG_LOCAL int							gServiceCacheEntryCount	= 0;	// 0 means to use the DNS-SD default.
DEBUG_LOCAL bool						gServiceManageLLRouting = true;
DEBUG_LOCAL int							gWaitCount				= 0;
DEBUG_LOCAL HANDLE					*	gWaitList				= NULL;
DEBUG_LOCAL HANDLE						gStopEvent				= NULL;
DEBUG_LOCAL HANDLE						gSPSWakeupEvent			= NULL;
DEBUG_LOCAL HANDLE						gSPSSleepEvent			= NULL;
DEBUG_LOCAL CRITICAL_SECTION			gEventSourceLock;
DEBUG_LOCAL GenLinkedList				gEventSources;
DEBUG_LOCAL BOOL						gRetryFirewall			= FALSE;
DEBUG_LOCAL DWORD						gOSMajorVersion;
DEBUG_LOCAL DWORD						gOSMinorVersion;

typedef DWORD ( WINAPI * GetIpInterfaceEntryFunctionPtr )( PMIB_IPINTERFACE_ROW );
mDNSlocal HMODULE								gIPHelperLibraryInstance		= NULL;
mDNSlocal GetIpInterfaceEntryFunctionPtr		gGetIpInterfaceEntryFunctionPtr	= NULL;


#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	Main
//===========================================================================================================================
int	Main( int argc, LPTSTR argv[] )
{
	OSStatus		err;
	BOOL			ok;
	BOOL			start;
	int				i;

	HeapSetInformation( NULL, HeapEnableTerminationOnCorruption, NULL, 0 );
	
	debug_initialize( kDebugOutputTypeMetaConsole );
	debug_set_property( kDebugPropertyTagPrintLevel, kDebugLevelVerbose );

	// Default to automatically starting the service dispatcher if no extra arguments are specified.
	
	start = ( argc <= 1 );
	
	// Parse arguments.
	
	for( i = 1; i < argc; ++i )
	{
		if( StrCmp( argv[ i ], TEXT("-install") ) == 0 )			// Install
		{
			TCHAR desc[ 256 ];
			
			desc[ 0 ] = 0;
			LoadString( GetModuleHandle( NULL ), IDS_SERVICE_DESCRIPTION, desc, sizeof( desc ) );
			err = InstallService( kServiceName, kServiceName, desc, argv[0] );
			if( err )
			{
				ReportStatus( EVENTLOG_ERROR_TYPE, "install service failed (%d)\n", err );
				goto exit;
			}
		}
		else if( StrCmp( argv[ i ], TEXT("-remove") ) == 0 )		// Remove
		{
			err = RemoveService( kServiceName );
			if( err )
			{
				ReportStatus( EVENTLOG_ERROR_TYPE, "remove service failed (%d)\n", err );
				goto exit;
			}
		}
		else if( StrCmp( argv[ i ], TEXT("-start") ) == 0 )		// Start
		{
			start = TRUE;
		}
		else if( StrCmp( argv[ i ], TEXT("-server") ) == 0 )		// Server
		{
			err = RunDirect( argc, argv );
			if( err )
			{
				ReportStatus( EVENTLOG_ERROR_TYPE, "run service directly failed (%d)\n", err );
			}
			goto exit;
		}
		else if( StrCmp( argv[ i ], TEXT("-q") ) == 0 )			// Quiet Mode (toggle)
		{
			gServiceQuietMode = !gServiceQuietMode;
		}
		else if( ( StrCmp( argv[ i ], TEXT("-help") ) == 0 ) || 	// Help
				 ( StrCmp( argv[ i ], TEXT("-h") ) == 0 ) )
		{
			Usage();
			err = 0;
			break;
		}
		else
		{
			Usage();
			err = kParamErr;
			break;
		}
	}
	
	// Start the service dispatcher if requested. This does not return until all services have terminated. If any 
	// global initialization is needed, it should be done before starting the service dispatcher, but only if it 
	// will take less than 30 seconds. Otherwise, use a separate thread for it and start the dispatcher immediately.
	
	if( start )
	{
		ok = StartServiceCtrlDispatcher( gServiceDispatchTable );
		err = translate_errno( ok, (OSStatus) GetLastError(), kInUseErr );
		if( err != kNoErr )
		{
			ReportStatus( EVENTLOG_ERROR_TYPE, "start service dispatcher failed (%d)\n", err );
			goto exit;
		}
	}
	err = 0;
	
exit:
	dlog( kDebugLevelTrace, DEBUG_NAME "exited (%d %m)\n", err, err );
	return( (int) err );
}

//===========================================================================================================================
//	Usage
//===========================================================================================================================

static void	Usage( void )
{
	fprintf( stderr, "\n" );
	fprintf( stderr, "mDNSResponder 1.0d1\n" );
	fprintf( stderr, "\n" );
	fprintf( stderr, "    <no args>    Runs the service normally\n" );
	fprintf( stderr, "    -install     Creates the service and starts it\n" );
	fprintf( stderr, "    -remove      Stops the service and deletes it\n" );
	fprintf( stderr, "    -start       Starts the service dispatcher after processing all other arguments\n" );
	fprintf( stderr, "    -server      Runs the service directly as a server (for debugging)\n" );
	fprintf( stderr, "    -q           Toggles Quiet Mode (no events or output)\n" );
	fprintf( stderr, "    -remote      Allow remote connections\n" );
	fprintf( stderr, "    -cache n     Number of mDNS cache entries (defaults to %d)\n", kDNSServiceCacheEntryCountDefault );
	fprintf( stderr, "    -h[elp]      Display Help/Usage\n" );
	fprintf( stderr, "\n" );
}

//===========================================================================================================================
//	ConsoleControlHandler
//===========================================================================================================================

static BOOL WINAPI	ConsoleControlHandler( DWORD inControlEvent )
{
	BOOL			handled;
	OSStatus		err;
	
	handled = FALSE;
	switch( inControlEvent )
	{
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			err = ServiceSpecificStop();
			require_noerr( err, exit );
			
			handled = TRUE;
			break;
		
		default:
			break;
	}
	
exit:
	return( handled );
}

//===========================================================================================================================
//	InstallService
//===========================================================================================================================

static OSStatus	InstallService( LPCTSTR inName, LPCTSTR inDisplayName, LPCTSTR inDescription, LPCTSTR inPath )
{
	OSStatus		err;
	SC_HANDLE		scm;
	SC_HANDLE		service;
	BOOL			ok;
	TCHAR			fullPath[ MAX_PATH ];
	TCHAR *			namePtr;
	DWORD			size;
	
	scm		= NULL;
	service = NULL;
	
	// Get a full path to the executable since a relative path may have been specified.
	
	size = GetFullPathName( inPath, MAX_PATH, fullPath, &namePtr );
	err = translate_errno( size > 0, (OSStatus) GetLastError(), kPathErr );
	require_noerr( err, exit );
	
	// Create the service and start it.
	
	scm = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
	err = translate_errno( scm, (OSStatus) GetLastError(), kOpenErr );
	require_noerr( err, exit );
	
	service = CreateService( scm, inName, inDisplayName, SERVICE_ALL_ACCESS, SERVICE_WIN32_SHARE_PROCESS, 
							 SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, fullPath, NULL, NULL, kServiceDependencies, 
							 NULL, NULL );
	err = translate_errno( service, (OSStatus) GetLastError(), kDuplicateErr );
	require_noerr( err, exit );

	err = SetServiceParameters();
	check_noerr( err );
	
	if( inDescription )
	{
		err = SetServiceInfo( scm, inName, inDescription );
		check_noerr( err );
	}

	ok = StartService( service, 0, NULL );
	err = translate_errno( ok, (OSStatus) GetLastError(), kInUseErr );
	require_noerr( err, exit );
	
	ReportStatus( EVENTLOG_SUCCESS, "installed service \"%s\"/\"%s\" at \"%s\"\n", inName, inDisplayName, inPath );
	err = kNoErr;
	
exit:
	if( service )
	{
		CloseServiceHandle( service );
	}
	if( scm )
	{
		CloseServiceHandle( scm );
	}
	return( err );
}

//===========================================================================================================================
//	RemoveService
//===========================================================================================================================

static OSStatus	RemoveService( LPCTSTR inName )
{
	OSStatus			err;
	SC_HANDLE			scm;
	SC_HANDLE			service;
	BOOL				ok;
	SERVICE_STATUS		status;
	
	scm		= NULL;
	service = NULL;
	
	// Open a connection to the service.
	
	scm = OpenSCManager( 0, 0, SC_MANAGER_ALL_ACCESS );
	err = translate_errno( scm, (OSStatus) GetLastError(), kOpenErr );
	require_noerr( err, exit );
	
	service = OpenService( scm, inName, SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE );
	err = translate_errno( service, (OSStatus) GetLastError(), kNotFoundErr );
	require_noerr( err, exit );
	
	// Stop the service, if it is not already stopped, then delete it.
	
	ok = QueryServiceStatus( service, &status );
	err = translate_errno( ok, (OSStatus) GetLastError(), kAuthenticationErr );
	require_noerr( err, exit );
	
	if( status.dwCurrentState != SERVICE_STOPPED )
	{
		ok = ControlService( service, SERVICE_CONTROL_STOP, &status );
		check_translated_errno( ok, (OSStatus) GetLastError(), kAuthenticationErr );
	}
	
	ok = DeleteService( service );
	err = translate_errno( ok, (OSStatus) GetLastError(), kDeletedErr );
	require_noerr( err, exit );
		
	ReportStatus( EVENTLOG_SUCCESS, "Removed service \"%s\"\n", inName );
	err = ERROR_SUCCESS;
	
exit:
	if( service )
	{
		CloseServiceHandle( service );
	}
	if( scm )
	{
		CloseServiceHandle( scm );
	}
	return( err );
}



//===========================================================================================================================
//	SetServiceParameters
//===========================================================================================================================

static OSStatus SetServiceParameters()
{
	DWORD 			value;
	DWORD			valueLen = sizeof(DWORD);
	DWORD			type;
	OSStatus		err;
	HKEY			key;

	key = NULL;

	//
	// Add/Open Parameters section under service entry in registry
	//
	err = RegCreateKey( HKEY_LOCAL_MACHINE, kServiceParametersNode, &key );
	require_noerr( err, exit );
	
	//
	// If the value isn't already there, then we create it
	//
	err = RegQueryValueEx(key, kServiceManageLLRouting, 0, &type, (LPBYTE) &value, &valueLen);

	if (err != ERROR_SUCCESS)
	{
		value = 1;

		err = RegSetValueEx( key, kServiceManageLLRouting, 0, REG_DWORD, (const LPBYTE) &value, sizeof(DWORD) );
		require_noerr( err, exit );
	}

exit:

	if ( key )
	{
		RegCloseKey( key );
	}

	return( err );
}



//===========================================================================================================================
//	GetServiceParameters
//===========================================================================================================================

static OSStatus GetServiceParameters()
{
	DWORD 			value;
	DWORD			valueLen;
	DWORD			type;
	OSStatus		err;
	HKEY			key;

	key = NULL;

	//
	// Add/Open Parameters section under service entry in registry
	//
	err = RegCreateKey( HKEY_LOCAL_MACHINE, kServiceParametersNode, &key );
	require_noerr( err, exit );
	
	valueLen = sizeof(DWORD);
	err = RegQueryValueEx(key, kServiceManageLLRouting, 0, &type, (LPBYTE) &value, &valueLen);
	if (err == ERROR_SUCCESS)
	{
		gServiceManageLLRouting = (value) ? true : false;
	}

	valueLen = sizeof(DWORD);
	err = RegQueryValueEx(key, kServiceCacheEntryCount, 0, &type, (LPBYTE) &value, &valueLen);
	if (err == ERROR_SUCCESS)
	{
		gServiceCacheEntryCount = value;
	}

exit:

	if ( key )
	{
		RegCloseKey( key );
	}

	return( err );
}


//===========================================================================================================================
//	CheckFirewall
//===========================================================================================================================

static OSStatus CheckFirewall()
{
	DWORD 					value;
	DWORD					valueLen;
	DWORD					type;
	ENUM_SERVICE_STATUS	*	lpService = NULL;
	SC_HANDLE				sc = NULL;
	HKEY					key = NULL;
	BOOL					ok;
	DWORD					bytesNeeded = 0;
	DWORD					srvCount;
	DWORD					resumeHandle = 0;
	DWORD					srvType;
	DWORD					srvState;
	DWORD					dwBytes = 0;
	DWORD					i;
	BOOL					isRunning = FALSE;
	OSStatus				err = kUnknownErr;
	
	// Check to see if the firewall service is running.  If it isn't, then
	// we want to return immediately

	sc = OpenSCManager( NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE );
	err = translate_errno( sc, GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	srvType		=	SERVICE_WIN32;
	srvState	=	SERVICE_STATE_ALL;

	for ( ;; )
	{
		// Call EnumServicesStatus using the handle returned by OpenSCManager

		ok = EnumServicesStatus ( sc, srvType, srvState, lpService, dwBytes, &bytesNeeded, &srvCount, &resumeHandle );

		if ( ok || ( GetLastError() != ERROR_MORE_DATA ) )
		{
			break;
		}

		if ( lpService )
		{
			free( lpService );
		}

		dwBytes = bytesNeeded;

		lpService = ( ENUM_SERVICE_STATUS* ) malloc( dwBytes );
		require_action( lpService, exit, err = mStatus_NoMemoryErr );
	}

	err = translate_errno( ok, GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	for ( i = 0; i < srvCount; i++ )
	{
		if ( wcscmp( lpService[i].lpServiceName, L"SharedAccess" ) == 0 )
		{
			if ( lpService[i].ServiceStatus.dwCurrentState == SERVICE_RUNNING )
			{
				isRunning = TRUE;
			}

			break;
		}
	}

	require_action( isRunning, exit, err = kUnknownErr );

	// Check to see if we've managed the firewall.
	// This package might have been installed, then
	// the OS was upgraded to SP2 or above.  If that's
	// the case, then we need to manipulate the firewall
	// so networking works correctly.

	err = RegCreateKey( HKEY_LOCAL_MACHINE, kServiceParametersNode, &key );
	require_noerr( err, exit );

	valueLen = sizeof(DWORD);
	err = RegQueryValueEx(key, kServiceManageFirewall, 0, &type, (LPBYTE) &value, &valueLen);
	
	if ((err != ERROR_SUCCESS) || (value == 0))
	{
		wchar_t	fullPath[ MAX_PATH ];
		DWORD	size;

		// Get a full path to the executable

		size = GetModuleFileNameW( NULL, fullPath, MAX_PATH );
		err = translate_errno( size > 0, (OSStatus) GetLastError(), kPathErr );
		require_noerr( err, exit );

		err = mDNSAddToFirewall(fullPath, kServiceFirewallName);
		require_noerr( err, exit );

		value = 1;
		err = RegSetValueEx( key, kServiceManageFirewall, 0, REG_DWORD, (const LPBYTE) &value, sizeof( DWORD ) );
		require_noerr( err, exit );
	}
	
exit:

	if ( key )
	{
		RegCloseKey( key );
	}
	
	if ( lpService )
	{
		free( lpService );
	}

	if ( sc )
	{
		CloseServiceHandle ( sc );
	}

	return( err );
}



//===========================================================================================================================
//	SetServiceInfo
//===========================================================================================================================

static OSStatus	SetServiceInfo( SC_HANDLE inSCM, LPCTSTR inServiceName, LPCTSTR inDescription )
{
	OSStatus				err;
	SC_LOCK					lock;
	SC_HANDLE				service;
	SERVICE_DESCRIPTION		description;
	SERVICE_FAILURE_ACTIONS	actions;
	SC_ACTION				action;
	BOOL					ok;
	
	check( inServiceName );
	check( inDescription );
	
	lock 	= NULL;
	service	= NULL;
	
	// Open the database (if not provided) and lock it to prevent other access while re-configuring.
	
	if( !inSCM )
	{
		inSCM = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
		err = translate_errno( inSCM, (OSStatus) GetLastError(), kOpenErr );
		require_noerr( err, exit );
	}
	
	lock = LockServiceDatabase( inSCM );
	err = translate_errno( lock, (OSStatus) GetLastError(), kInUseErr );
	require_noerr( err, exit );
	
	// Open a handle to the service. 

	service = OpenService( inSCM, inServiceName, SERVICE_CHANGE_CONFIG|SERVICE_START );
	err = translate_errno( service, (OSStatus) GetLastError(), kNotFoundErr );
	require_noerr( err, exit );
	
	// Change the description.
	
	description.lpDescription = (LPTSTR) inDescription;
	ok = ChangeServiceConfig2( service, SERVICE_CONFIG_DESCRIPTION, &description );
	err = translate_errno( ok, (OSStatus) GetLastError(), kParamErr );
	require_noerr( err, exit );
	
	actions.dwResetPeriod	=	INFINITE;
	actions.lpRebootMsg		=	NULL;
	actions.lpCommand		=	NULL;
	actions.cActions		=	1;
	actions.lpsaActions		=	&action;
	action.Delay			=	500;
	action.Type				=	SC_ACTION_RESTART;

	ok = ChangeServiceConfig2( service, SERVICE_CONFIG_FAILURE_ACTIONS, &actions );
	err = translate_errno( ok, (OSStatus) GetLastError(), kParamErr );
	require_noerr( err, exit );
	
	err = ERROR_SUCCESS;
	
exit:
	// Close the service and release the lock.
	
	if( service )
	{
		CloseServiceHandle( service );
	}
	if( lock )
	{
		UnlockServiceDatabase( lock ); 
	}
	return( err );
}

//===========================================================================================================================
//	ReportStatus
//===========================================================================================================================

static void	ReportStatus( int inType, const char *inFormat, ... )
{
	if( !gServiceQuietMode )
	{
		va_list		args;
		
		va_start( args, inFormat );
		if( gServiceEventSource )
		{
			char				s[ 1024 ];
			BOOL				ok;
			const char *		array[ 1 ];
			
			vsprintf( s, inFormat, args );
			array[ 0 ] = s;
			ok = ReportEventA( gServiceEventSource, (WORD) inType, 0, 0x20000001L, NULL, 1, 0, array, NULL );
			check_translated_errno( ok, GetLastError(), kUnknownErr );
		}
		else
		{
			int		n;
			
			n = vfprintf( stderr, inFormat, args );
			check( n >= 0 );
		}
		va_end( args );
	}
}

//===========================================================================================================================
//	RunDirect
//===========================================================================================================================

int	RunDirect( int argc, LPTSTR argv[] )
{
	OSStatus		err;
	BOOL			initialized;
   BOOL        ok;
	
	initialized = FALSE;

	// Install a Console Control Handler to handle things like control-c signals.
	
	ok = SetConsoleCtrlHandler( ConsoleControlHandler, TRUE );
	err = translate_errno( ok, (OSStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );
	
	err = ServiceSpecificInitialize( argc, argv );
	require_noerr( err, exit );
	initialized = TRUE;
	
	// Run the service. This does not return until the service quits or is stopped.
	
	ReportStatus( EVENTLOG_SUCCESS, "Running \"%s\" service directly\n", kServiceName );
	
	err = ServiceSpecificRun( argc, argv );
	require_noerr( err, exit );
	
	// Clean up.
	
exit:
	if( initialized )
	{
		ServiceSpecificFinalize( argc, argv );
	}
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	ServiceMain
//===========================================================================================================================

static void WINAPI ServiceMain( DWORD argc, LPTSTR argv[] )
{
	OSStatus		err;
	BOOL			ok;
	
	err = ServiceSetupEventLogging();
	check_noerr( err );

	err = GetServiceParameters();
	check_noerr( err );
	
	// Initialize the service status and register the service control handler with the name of the service.
	
	gServiceStatus.dwServiceType 				= SERVICE_WIN32_SHARE_PROCESS;
	gServiceStatus.dwCurrentState 				= 0;
	gServiceStatus.dwControlsAccepted 			= SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_POWEREVENT;
	gServiceStatus.dwWin32ExitCode 				= NO_ERROR;
	gServiceStatus.dwServiceSpecificExitCode 	= NO_ERROR;
	gServiceStatus.dwCheckPoint 				= 0;
	gServiceStatus.dwWaitHint 					= 0;
	
	gServiceStatusHandle = RegisterServiceCtrlHandlerEx( argv[ 0 ], ServiceControlHandler, NULL );
	err = translate_errno( gServiceStatusHandle, (OSStatus) GetLastError(), kInUseErr );
	require_noerr( err, exit );
	
	// Mark the service as starting.

	gServiceStatus.dwCurrentState 	= SERVICE_START_PENDING;
	gServiceStatus.dwCheckPoint	 	= 0;
	gServiceStatus.dwWaitHint 		= 5000;	// 5 seconds
	ok = SetServiceStatus( gServiceStatusHandle, &gServiceStatus );
	check_translated_errno( ok, GetLastError(), kParamErr );
	
	// Run the service. This does not return until the service quits or is stopped.
	
	err = ServiceRun( (int) argc, argv );
	if( err != kNoErr )
	{
		gServiceStatus.dwWin32ExitCode				= ERROR_SERVICE_SPECIFIC_ERROR;
		gServiceStatus.dwServiceSpecificExitCode 	= (DWORD) err;
	}
	
	// Service-specific work is done so mark the service as stopped.
	
	gServiceStatus.dwCurrentState = SERVICE_STOPPED;
	ok = SetServiceStatus( gServiceStatusHandle, &gServiceStatus );
	check_translated_errno( ok, GetLastError(), kParamErr );
	
	// Note: The service status handle should not be closed according to Microsoft documentation.
	
exit:
	if( gServiceEventSource )
	{
		ok = DeregisterEventSource( gServiceEventSource );
		check_translated_errno( ok, GetLastError(), kUnknownErr );
		gServiceEventSource = NULL;
	}
}

//===========================================================================================================================
//	ServiceSetupEventLogging
//===========================================================================================================================

static OSStatus	ServiceSetupEventLogging( void )
{
	OSStatus			err;
	HKEY				key;
	LPCTSTR				s;
	DWORD				typesSupported;
	TCHAR				path[ MAX_PATH ];
	DWORD 				n;
	
	key = NULL;
	
	// Add/Open source name as a sub-key under the Application key in the EventLog registry key.

	s = TEXT("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\") kServiceName;
	err = RegCreateKey( HKEY_LOCAL_MACHINE, s, &key );
	require_noerr( err, exit );
	
	// Add the name to the EventMessageFile subkey.

	path[ 0 ] = '\0';
	GetModuleFileName( NULL, path, MAX_PATH );
	n = (DWORD) ( ( StrLen( path ) + 1 ) * sizeof( TCHAR ) );
	err = RegSetValueEx( key, TEXT("EventMessageFile"), 0, REG_EXPAND_SZ, (const LPBYTE) path, n );
	require_noerr( err, exit );
	
	// Set the supported event types in the TypesSupported subkey.
	
	typesSupported = 0 
					 | EVENTLOG_SUCCESS
					 | EVENTLOG_ERROR_TYPE
					 | EVENTLOG_WARNING_TYPE
					 | EVENTLOG_INFORMATION_TYPE
					 | EVENTLOG_AUDIT_SUCCESS
					 | EVENTLOG_AUDIT_FAILURE; 
	err = RegSetValueEx( key, TEXT("TypesSupported"), 0, REG_DWORD, (const LPBYTE) &typesSupported, sizeof( DWORD ) );
	require_noerr( err, exit );
	
	// Set up the event source.
	
	gServiceEventSource = RegisterEventSource( NULL, kServiceName );
	err = translate_errno( gServiceEventSource, (OSStatus) GetLastError(), kParamErr );
	require_noerr( err, exit );
		
exit:
	if( key )
	{
		RegCloseKey( key );
	}
	return( err );
}

//===========================================================================================================================
//	ServiceControlHandler
//===========================================================================================================================

static DWORD WINAPI	ServiceControlHandler( DWORD inControl, DWORD inEventType, LPVOID inEventData, LPVOID inContext )
{
	BOOL		setStatus;
	BOOL		ok;

	DEBUG_UNUSED( inEventData );
	DEBUG_UNUSED( inContext );
	
	setStatus = TRUE;
	switch( inControl )
	{
		case SERVICE_CONTROL_STOP:
			dlog( kDebugLevelInfo, DEBUG_NAME "ServiceControlHandler: SERVICE_CONTROL_STOP\n" );
			
			ServiceStop();
			setStatus = FALSE;
			break;
		
		case SERVICE_CONTROL_POWEREVENT:

			if (inEventType == PBT_APMSUSPEND)
			{
				LARGE_INTEGER timeout;

				dlog( kDebugLevelInfo, DEBUG_NAME "ServiceControlHandler: PBT_APMSUSPEND\n" );

				mDNSPlatformLock( &gMDNSRecord );
				
				gMDNSRecord.SystemWakeOnLANEnabled = SystemWakeForNetworkAccess( &timeout );
				
				if ( gMDNSRecord.SystemWakeOnLANEnabled )
				{
					ok = SetWaitableTimer( gSPSWakeupEvent, &timeout, 0, NULL, NULL, TRUE );
					check( ok );
				}

				mDNSPlatformUnlock( &gMDNSRecord );

				mDNSCoreMachineSleep(&gMDNSRecord, TRUE);
			}
			else if (inEventType == PBT_APMRESUMESUSPEND)
			{
				dlog( kDebugLevelInfo, DEBUG_NAME "ServiceControlHandler: PBT_APMRESUMESUSPEND\n" );

				mDNSPlatformLock( &gMDNSRecord );

				if ( gSPSWakeupEvent )
				{
					CancelWaitableTimer( gSPSWakeupEvent );
				}

				if ( gSPSSleepEvent )
				{
					CancelWaitableTimer( gSPSSleepEvent );
				}

				mDNSPlatformUnlock( &gMDNSRecord );

				mDNSCoreMachineSleep(&gMDNSRecord, FALSE);
			}
		
			break;

		default:
			dlog( kDebugLevelNotice, DEBUG_NAME "ServiceControlHandler: event (0x%08X)\n", inControl );
			break;
	}
	
	if( setStatus && gServiceStatusHandle )
	{
		ok = SetServiceStatus( gServiceStatusHandle, &gServiceStatus );
		check_translated_errno( ok, GetLastError(), kUnknownErr );
	}

	return NO_ERROR;
}

//===========================================================================================================================
//	ServiceRun
//===========================================================================================================================

static OSStatus	ServiceRun( int argc, LPTSTR argv[] )
{
	OSStatus		err;
	BOOL			initialized;
	BOOL			ok;
	
	DEBUG_UNUSED( argc );
	DEBUG_UNUSED( argv );
	
	initialized = FALSE;
	
	// <rdar://problem/5727548> Make the service as running before we call ServiceSpecificInitialize. We've
	// had reports that some machines with McAfee firewall installed cause a problem with iTunes installation.
	// We think that the firewall product is interferring with code in ServiceSpecificInitialize. So as a
	// simple workaround, we'll mark us as running *before* we call ServiceSpecificInitialize. This will unblock
	// any installers that are waiting for our state to change.

	gServiceStatus.dwCurrentState = SERVICE_RUNNING;
	ok = SetServiceStatus( gServiceStatusHandle, &gServiceStatus );
	check_translated_errno( ok, GetLastError(), kParamErr );

	// Initialize the service-specific stuff
	
	err = ServiceSpecificInitialize( argc, argv );
	require_noerr( err, exit );
	initialized = TRUE;
	
	err = CheckFirewall();
	check_noerr( err );

	if ( err )
	{
		gRetryFirewall = TRUE;
	}
	
	// Run the service-specific stuff. This does not return until the service quits or is stopped.
	
	ReportStatus( EVENTLOG_INFORMATION_TYPE, "mDNSResponder started\n" );
	err = ServiceSpecificRun( argc, argv );
	ReportStatus( EVENTLOG_INFORMATION_TYPE, "mDNSResponder stopped (%d)\n", err );
	require_noerr( err, exit );
	
	// Service stopped. Clean up and we're done.
	
exit:
	if( initialized )
	{
		ServiceSpecificFinalize( argc, argv );
	}
	return( err );
}

//===========================================================================================================================
//	ServiceStop
//===========================================================================================================================

static void	ServiceStop( void )
{
	BOOL			ok;
	OSStatus		err;
	
	// Signal the event to cause the service to exit.
	
	if( gServiceStatusHandle )
	{
		gServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		ok = SetServiceStatus( gServiceStatusHandle, &gServiceStatus );
		check_translated_errno( ok, GetLastError(), kParamErr );
	}
		
	err = ServiceSpecificStop();
	check_noerr( err );
}

#if 0
#pragma mark -
#pragma mark == Service Specific ==
#endif

//===========================================================================================================================
//	ServiceSpecificInitialize
//===========================================================================================================================

static OSStatus	ServiceSpecificInitialize( int argc, LPTSTR argv[] )
{
	OSVERSIONINFO osInfo;
	OSStatus err;
	BOOL ok;
	
	DEBUG_UNUSED( argc );
	DEBUG_UNUSED( argv );
	
	mDNSPlatformMemZero( &gMDNSRecord, sizeof gMDNSRecord);
	mDNSPlatformMemZero( &gPlatformStorage, sizeof gPlatformStorage);

	gPlatformStorage.idleThreadCallback = udsIdle;
	gPlatformStorage.hostDescriptionChangedCallback = HostDescriptionChanged;

	InitializeCriticalSection(&gEventSourceLock);
	
	gStopEvent	=	CreateEvent(NULL, FALSE, FALSE, NULL);
	err = translate_errno( gStopEvent, errno_compat(), kNoResourcesErr );
	require_noerr( err, exit );

	gSPSWakeupEvent = CreateWaitableTimer( NULL, FALSE, NULL );
	err = translate_errno( gSPSWakeupEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	gSPSSleepEvent = CreateWaitableTimer( NULL, FALSE, NULL );
	err = translate_errno( gSPSSleepEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	err = mDNS_Init( &gMDNSRecord, &gPlatformStorage, gRRCache, RR_CACHE_SIZE, mDNS_Init_AdvertiseLocalAddresses, CoreCallback, mDNS_Init_NoInitCallbackContext); 
	require_noerr( err, exit);

	err = udsserver_init(mDNSNULL, 0);
	require_noerr( err, exit);

	//
	// Get the version of Windows that we're running on
	//
	osInfo.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
	ok = GetVersionEx( &osInfo );
	err = translate_errno( ok, (OSStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );
	gOSMajorVersion = osInfo.dwMajorVersion;
	gOSMinorVersion = osInfo.dwMinorVersion;

	SetLLRoute( &gMDNSRecord );

exit:
	if( err != kNoErr )
	{
		ServiceSpecificFinalize( argc, argv );
	}
	return( err );
}

//===========================================================================================================================
//	ServiceSpecificRun
//===========================================================================================================================

static OSStatus	ServiceSpecificRun( int argc, LPTSTR argv[] )
{
	DWORD	timeout;
	DWORD result;
	
	DEBUG_UNUSED( argc );
	DEBUG_UNUSED( argv );

	// Main event loop. Process connection requests and state changes (i.e. quit).

	timeout = ( gRetryFirewall ) ? kRetryFirewallPeriod : INFINITE;

	for ( ;; )
	{
		HANDLE	waitList[ 3 ];

		waitList[ 0 ] = gStopEvent;
		waitList[ 1 ] = gSPSWakeupEvent;
		waitList[ 2 ] = gSPSSleepEvent;

		result = WaitForMultipleObjects( 3, waitList, FALSE, timeout );

		if ( result == WAIT_OBJECT_0 )
		{
			break;
		}
		else if ( result == WAIT_OBJECT_0 + 1 )
		{
			__int64			temp;
			LARGE_INTEGER	timeout;

			dlog( kDebugLevelInfo, DEBUG_NAME "setting suspend event\n" );

			// Stay awake for 60 seconds

			temp				= -60 * 10000000;
			timeout.LowPart		= (DWORD) ( temp & 0xFFFFFFFF );
			timeout.HighPart	= (LONG)  ( temp >> 32 );

			SetWaitableTimer( gSPSSleepEvent, &timeout, 0, NULL, NULL, TRUE );
		}
		else if ( result == WAIT_OBJECT_0 + 2 )
		{
			dlog( kDebugLevelInfo, DEBUG_NAME "suspending machine\n" );
			SetSuspendState( FALSE, FALSE, FALSE );
		}
		else if ( result == WAIT_TIMEOUT )
		{
			OSStatus err;

			err = CheckFirewall();
			check_noerr( err );

			timeout = INFINITE;
		}
		else
		{
			// Unexpected wait result.
			dlog( kDebugLevelWarning, DEBUG_NAME "%s: unexpected wait result (result=0x%08X)\n", __ROUTINE__, result );
		}
	}

	return kNoErr;
}

//===========================================================================================================================
//	ServiceSpecificStop
//===========================================================================================================================

static OSStatus	ServiceSpecificStop( void )
{
	OSStatus	err;
	BOOL	 	ok;

	ok = SetEvent(gStopEvent);
	err = translate_errno( ok, (OSStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );
exit:
	return( err );
}

//===========================================================================================================================
//	ServiceSpecificFinalize
//===========================================================================================================================

static void	ServiceSpecificFinalize( int argc, LPTSTR argv[] )
{
	DEBUG_UNUSED( argc );
	DEBUG_UNUSED( argv );
	
	//
	// clean up any open sessions
	//
	while (gEventSources.Head)
	{
		EventSourceFinalize((Win32EventSource*) gEventSources.Head);
	}
	//
	// give a chance for the udsserver code to clean up
	//
	udsserver_exit();

	//
	// and finally close down the mDNSCore
	//
	mDNS_Close(&gMDNSRecord);

	//
	// clean up the event sources mutex...no one should be using it now
	//
	DeleteCriticalSection(&gEventSourceLock);

	if ( gSPSWakeupEvent )
	{
		CloseHandle( gSPSWakeupEvent );
		gSPSWakeupEvent = NULL;
	}

	if ( gSPSSleepEvent )
	{
		CloseHandle( gSPSSleepEvent );
		gSPSSleepEvent = NULL;
	}

	if ( gStopEvent )
	{
		CloseHandle( gStopEvent );
		gStopEvent = NULL;
	}

	//
	// clean up loaded library
	//

	if( gIPHelperLibraryInstance )
	{
		gGetIpInterfaceEntryFunctionPtr = NULL;
		
		FreeLibrary( gIPHelperLibraryInstance );
		gIPHelperLibraryInstance = NULL;
	}
}


static void
CoreCallback(mDNS * const inMDNS, mStatus status)
{
	if (status == mStatus_ConfigChanged)
	{
		SetLLRoute( inMDNS );
	}
}


static mDNSs32
udsIdle(mDNS * const inMDNS, mDNSs32 interval)
{
	DEBUG_UNUSED( inMDNS );

	//
	// rdar://problem/3697326
	//
	// udsserver_idle wasn't being locked.  This resulted
	// in multiple threads contesting for the all_requests
	// data structure in uds_daemon.c
	//
	mDNSPlatformLock(&gMDNSRecord);

	interval = udsserver_idle(interval);

	mDNSPlatformUnlock(&gMDNSRecord);

	return interval;
}


static void
HostDescriptionChanged(mDNS * const inMDNS)
{
	DEBUG_UNUSED( inMDNS );

	udsserver_handle_configchange(inMDNS);
}


mDNSlocal unsigned WINAPI
udsSocketThread(LPVOID inParam)
{
	Win32EventSource	*	source		=	(Win32EventSource*) inParam;
	DWORD					threadID	=	GetCurrentThreadId();
	DWORD					waitCount;
	HANDLE					waitList[2];
	bool					safeToClose;
	bool					done;
	bool					locked		= false;
	mStatus					err			= 0;

	waitCount	= source->waitCount;
	waitList[0] = source->waitList[0];
	waitList[1] = source->waitList[1];
	done		= (bool) (source->flags & EventSourceFinalized);

	while (!done)
	{
		DWORD result;

		result = WaitForMultipleObjects(waitCount, waitList, FALSE, INFINITE);
		
		mDNSPlatformLock(&gMDNSRecord);
		locked = true;

		// <rdar://problem/3838237>
		//
		// Look up the source by the thread id.  This will ensure that the 
		// source is still extant.  It could already have been deleted
		// by the processing thread.
		//

		EventSourceLock();

		for (source = gEventSources.Head; source; source = source->next)
		{
			if (source->threadID == threadID)
			{
				break;
			}
		}

		EventSourceUnlock();
		
		if (source == NULL)
		{
			goto exit;
		}

		//
		// socket event
		//
		if (result == WAIT_OBJECT_0)
		{
			source->callback( (int) source->sock, 0, source->context);
		}
		//
		// close event
		//
		else if (result == WAIT_OBJECT_0 + 1)
		{
			//
			// this is a bit of a hack.  we want to clean up the internal data structures
			// so we'll go in here and it will clean up for us
			//
			shutdown(source->sock, 2);
			source->callback( (int) source->sock, 0, source->context);

			break;
		}
		else
		{
			// Unexpected wait result.
			dlog( kDebugLevelWarning, DEBUG_NAME "%s: unexpected wait result (result=0x%08X)\n", __ROUTINE__, result );
			goto exit;
		}

		done   = (bool) (source->flags & EventSourceFinalized);
		
		mDNSPlatformUnlock(&gMDNSRecord);
		locked = false;
	}

	EventSourceLock();
	source->flags |= EventSourceFlagsThreadDone;
	safeToClose = !( source->flags & EventSourceFlagsNoClose );
	EventSourceUnlock();

	if( safeToClose )
	{
		EventSourceFinalize( source );
	}

exit:

	if ( locked )
	{
		mDNSPlatformUnlock(&gMDNSRecord);
	}

	_endthreadex_compat( (unsigned) err );
	return( (unsigned) err );	
}


mStatus
udsSupportAddFDToEventLoop( SocketRef fd, udsEventCallback callback, void *context)
{
	Win32EventSource *  newSource;
	DWORD				result;
	mStatus				err;

	newSource = malloc(sizeof(Win32EventSource));
	require_action( newSource, exit, err = mStatus_NoMemoryErr );
	mDNSPlatformMemZero(newSource, sizeof(Win32EventSource));

	newSource->flags	= 0;
	newSource->sock		= (SOCKET) fd;
	newSource->callback	= callback;
	newSource->context	= context;

	newSource->socketEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	err = translate_errno( newSource->socketEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	newSource->closeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	err = translate_errno( newSource->closeEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	err = WSAEventSelect(newSource->sock, newSource->socketEvent, FD_ACCEPT|FD_READ|FD_CLOSE);
	err = translate_errno( err == 0, errno_compat(), kNoResourcesErr );
	require_noerr( err, exit );

	newSource->waitCount = 0;
	newSource->waitList[ newSource->waitCount++ ] = newSource->socketEvent;
	newSource->waitList[ newSource->waitCount++ ] = newSource->closeEvent;

	//
	// lock the list
	//
	EventSourceLock();
	
	// add the event source to the end of the list, while checking
	// to see if the list needs to be initialized
	//
	if ( gEventSources.LinkOffset == 0)
	{
		InitLinkedList( &gEventSources, offsetof( Win32EventSource, next));
	}

	AddToTail( &gEventSources, newSource);

	//
	// no longer using the list
	//
	EventSourceUnlock();

	// Create thread with _beginthreadex() instead of CreateThread() to avoid memory leaks when using static run-time 
	// libraries. See <http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dllproc/base/createthread.asp>.
	// Create the thread suspended then resume it so the thread handle and ID are valid before the thread starts running.
	newSource->threadHandle = (HANDLE) _beginthreadex_compat( NULL, 0, udsSocketThread, newSource, CREATE_SUSPENDED, &newSource->threadID );
	err = translate_errno( newSource->threadHandle, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	result = ResumeThread( newSource->threadHandle );
	err = translate_errno( result != (DWORD) -1, errno_compat(), kNoResourcesErr );
	require_noerr( err, exit );

exit:

	if (err && newSource)
	{
		EventSourceFinalize(newSource);
	}

	return err;
}


mStatus
udsSupportRemoveFDFromEventLoop( SocketRef fd)		// Note: This also CLOSES the socket
{
	Win32EventSource	*	source;
	mStatus					err = mStatus_NoError;
	
	//
	// find the event source
	//
	EventSourceLock();

	for (source = gEventSources.Head; source; source = source->next)
	{
		if (source->sock == (SOCKET) fd)
		{
			break;
		}
	}

	//
	// if we found him, finalize him
	//
	if (source != NULL)
	{
		EventSourceFinalize(source);
	}

	//
	// done with the list
	//
	EventSourceUnlock();
	
	closesocket(fd);

	return err;
}


mDNSexport void RecordUpdatedNiceLabel(mDNS *const m, mDNSs32 delay)
	{
	(void)m;
	(void)delay;
	// No-op, for now
	}


static mStatus
EventSourceFinalize(Win32EventSource * source)
{
	OSStatus				err;
	bool					locked;
	Win32EventSource	*	inserted;
	bool				sameThread;
	bool				deferClose;
	BOOL				ok;
	DWORD 				threadID;
	DWORD				result;
	
	check( source );
	
	// Find the session in the list.
	
	EventSourceLock();
	locked = true;
	
	for( inserted = (Win32EventSource*) gEventSources.Head; inserted; inserted = inserted->next )
	{
		if( inserted == source )
		{
			break;
		}
	}
	require_action( inserted, exit, err = kNotFoundErr );

	//
	// note that we've had finalize called
	//
	source->flags |= EventSourceFinalized;
	
	// If we're being called from the same thread as the session (e.g. message callback is closing the session) then 
	// we must defer the close until the thread is done because the thread is still using the session object.
	
	deferClose	= false;
	threadID	= GetCurrentThreadId();
	sameThread	= source->threadHandle && ( threadID == source->threadID );
	if( sameThread && !( source->flags & EventSourceFlagsThreadDone ) )
	{
		source->flags &= ~EventSourceFlagsNoClose;
		deferClose = true;
	}
	
	// If the thread we're not being called from the session thread, but the thread has already marked itself as
	// as done (e.g. session closed from something like a peer disconnect and at the same time the client also 
	// tried to close) then we only want to continue with the close if the thread is not going to close itself.
	
	if( !sameThread && ( source->flags & EventSourceFlagsThreadDone ) && !( source->flags & EventSourceFlagsNoClose ) )
	{
		deferClose = true;
	}
	
	// Signal a close so the thread exits.
	
	if( source->closeEvent )
	{
		ok = SetEvent( source->closeEvent );
		check_translated_errno( ok, errno_compat(), kUnknownErr );
	}	
	if( deferClose )
	{
		err = kNoErr;
		goto exit;
	}
	
	source->flags |= EventSourceFlagsNoClose;
	
	// Remove the session from the list.
	RemoveFromList(&gEventSources, source);
	
	EventSourceUnlock();
	locked = false;
	
	// Wait for the thread to exit. Give up after 3 seconds to handle a hung thread.
	
	if( source->threadHandle && ( threadID != source->threadID ) )
	{
		result = WaitForSingleObject( source->threadHandle, 3 * 1000 );
		check_translated_errno( result == WAIT_OBJECT_0, (OSStatus) GetLastError(), result );
	}
	
	// Release the thread.
	
	if( source->threadHandle )
	{
		ok = CloseHandle( source->threadHandle );
		check_translated_errno( ok, errno_compat(), kUnknownErr );
		source->threadHandle = NULL;
	}
	
	// Release the socket event.
	
	if( source->socketEvent )
	{
		ok = CloseHandle( source->socketEvent );
		check_translated_errno( ok, errno_compat(), kUnknownErr );
		source->socketEvent = NULL;
	}
	
	// Release the close event.
	
	if( source->closeEvent )
	{
		ok = CloseHandle( source->closeEvent );
		check_translated_errno( ok, errno_compat(), kUnknownErr );
		source->closeEvent = NULL;
	}
	
	// Release the memory used by the object.
	free ( source );

	err = kNoErr;
	
	dlog( kDebugLevelNotice, DEBUG_NAME "session closed\n" );
	
exit:

	if( locked )
	{
		EventSourceUnlock();
	}

	return( err );
}


static void
EventSourceLock()
{
	EnterCriticalSection(&gEventSourceLock);
}


static void
EventSourceUnlock()
{
	LeaveCriticalSection(&gEventSourceLock);
}


//===========================================================================================================================
//	SystemWakeForNetworkAccess
//===========================================================================================================================

mDNSu8
SystemWakeForNetworkAccess( LARGE_INTEGER * timeout )
{
	HKEY					key = NULL;
	DWORD					dwSize;
	DWORD					enabled;
	mDNSu8					ok;
	SYSTEM_POWER_STATUS		powerStatus;
	time_t					startTime;
	time_t					nextWakeupTime;
	time_t					delta;
	__int64					delta64;
	DWORD					err;

	dlog( kDebugLevelInfo, DEBUG_NAME "SystemWakeForNetworkAccess\n" );

	// Make sure we have a timer

	require_action( gSPSWakeupEvent != NULL, exit, ok = FALSE );
	require_action( gSPSSleepEvent != NULL, exit, ok = FALSE );

	// Make sure the user enabled bonjour sleep proxy client 
	
	err = RegCreateKey( HKEY_LOCAL_MACHINE, kServiceParametersNode L"\\Power Management", &key );
	require_action( !err, exit, ok = FALSE );
	dwSize = sizeof( DWORD );
	err = RegQueryValueEx( key, L"Enabled", NULL, NULL, (LPBYTE) &enabled, &dwSize );
	require_action( !err, exit, ok = FALSE );
	require_action( enabled, exit, ok = FALSE );
	
	// Make sure machine is on AC power
	
	ok = ( mDNSu8 ) GetSystemPowerStatus( &powerStatus );
	require_action( ok, exit, ok = FALSE );
	require_action( powerStatus.ACLineStatus == AC_LINE_ONLINE, exit, ok = FALSE );

	// Now make sure we have a network interface that does wake-on-lan

	UpdateWOMPConfig( &gMDNSRecord );
	require_action( gMDNSRecord.p->womp, exit, ok = FALSE );

	// Calculate next wake up time

	startTime		= time( NULL );
	nextWakeupTime	= startTime + ( 120 * 60 );
	
	if ( gMDNSRecord.p->nextDHCPLeaseExpires < nextWakeupTime )
	{
		nextWakeupTime = gMDNSRecord.p->nextDHCPLeaseExpires;
	}

	// Finally calculate the next relative wakeup time

	delta = ( time_t ) ( ( ( double )( nextWakeupTime - startTime ) ) * 0.9 );
	delta64 = -delta * 10000000;
	timeout->LowPart  = (DWORD) ( delta64 & 0xFFFFFFFF );
	timeout->HighPart = (LONG)  ( delta64 >> 32 );

	dlog( kDebugLevelInfo, DEBUG_NAME "enabling sleep proxy client with next wakeup time %d seconds from now\n", delta );

	ok = TRUE;

exit:

	if ( key )
	{
		RegCloseKey( key );
	}

	return ok;
}


//===========================================================================================================================
//	HaveRoute
//===========================================================================================================================

static bool
HaveRoute( PMIB_IPFORWARDROW rowExtant, unsigned long addr, unsigned long metric )
{
	PMIB_IPFORWARDTABLE	pIpForwardTable	= NULL;
	DWORD				dwSize			= 0;
	BOOL				bOrder			= FALSE;
	OSStatus			err;
	bool				found			= false;
	unsigned long int	i;

	//
	// Find out how big our buffer needs to be.
	//
	err = GetIpForwardTable(NULL, &dwSize, bOrder);
	require_action( err == ERROR_INSUFFICIENT_BUFFER, exit, err = kUnknownErr );

	//
	// Allocate the memory for the table
	//
	pIpForwardTable = (PMIB_IPFORWARDTABLE) malloc( dwSize );
	require_action( pIpForwardTable, exit, err = kNoMemoryErr );
  
	//
	// Now get the table.
	//
	err = GetIpForwardTable(pIpForwardTable, &dwSize, bOrder);
	require_noerr( err, exit );

	//
	// Search for the row in the table we want.
	//
	for ( i = 0; i < pIpForwardTable->dwNumEntries; i++)
	{
		if ( ( pIpForwardTable->table[i].dwForwardDest == addr ) && ( !metric || ( pIpForwardTable->table[i].dwForwardMetric1 == metric ) ) )
		{
			memcpy( rowExtant, &(pIpForwardTable->table[i]), sizeof(*rowExtant) );
			found = true;
			break;
		}
	}

exit:

	if ( pIpForwardTable != NULL ) 
	{
		free(pIpForwardTable);
	}
    
	return found;
}


//===========================================================================================================================
//	IsValidAddress
//===========================================================================================================================

static bool
IsValidAddress( const char * addr )
{
	return ( addr && ( strcmp( addr, "0.0.0.0" ) != 0 ) ) ? true : false;
}	


//===========================================================================================================================
//	GetAdditionalMetric
//===========================================================================================================================

static ULONG
GetAdditionalMetric( DWORD ifIndex )
{
	ULONG metric = 0;

	if( !gIPHelperLibraryInstance )
	{
		gIPHelperLibraryInstance = LoadLibrary( TEXT( "Iphlpapi" ) );

		gGetIpInterfaceEntryFunctionPtr = 
				(GetIpInterfaceEntryFunctionPtr) GetProcAddress( gIPHelperLibraryInstance, "GetIpInterfaceEntry" );

		if( !gGetIpInterfaceEntryFunctionPtr )
		{		
			BOOL ok;
				
			ok = FreeLibrary( gIPHelperLibraryInstance );
			check_translated_errno( ok, GetLastError(), kUnknownErr );
			gIPHelperLibraryInstance = NULL;
		}
	}

	if ( gGetIpInterfaceEntryFunctionPtr )
	{
		MIB_IPINTERFACE_ROW row;
		DWORD err;

		ZeroMemory( &row, sizeof( MIB_IPINTERFACE_ROW ) );
		row.Family = AF_INET;
		row.InterfaceIndex = ifIndex;
		err = gGetIpInterfaceEntryFunctionPtr( &row );
		require_noerr( err, exit );
		metric = row.Metric + 256;
	}

exit:

	return metric;
}


//===========================================================================================================================
//	SetLLRoute
//===========================================================================================================================

static OSStatus
SetLLRoute( mDNS * const inMDNS )
{
	OSStatus err = kNoErr;

	DEBUG_UNUSED( inMDNS );

	//
	// <rdar://problem/4096464> Don't call SetLLRoute on loopback
	// <rdar://problem/6885843> Default route on Windows 7 breaks network connectivity
	// 
	// Don't mess w/ the routing table on Vista and later OSes, as 
	// they have a permanent route to link-local addresses. Otherwise,
	// set a route to link local addresses (169.254.0.0)
	//
	if ( ( gOSMajorVersion < 6 ) && gServiceManageLLRouting && !gPlatformStorage.registeredLoopback4 )
	{
		DWORD				ifIndex;
		MIB_IPFORWARDROW	rowExtant;
		bool				addRoute;
		MIB_IPFORWARDROW	row;

		ZeroMemory(&row, sizeof(row));

		err = GetRouteDestination(&ifIndex, &row.dwForwardNextHop);
		require_noerr( err, exit );
		row.dwForwardDest		= inet_addr(kLLNetworkAddr);
		row.dwForwardIfIndex	= ifIndex;
		row.dwForwardMask		= inet_addr(kLLNetworkAddrMask);
		row.dwForwardType		= 3;
		row.dwForwardProto		= MIB_IPPROTO_NETMGMT;
		row.dwForwardAge		= 0;
		row.dwForwardPolicy		= 0;
		row.dwForwardMetric1	= 20 + GetAdditionalMetric( ifIndex );
		row.dwForwardMetric2	= (DWORD) - 1;
		row.dwForwardMetric3	= (DWORD) - 1;
		row.dwForwardMetric4	= (DWORD) - 1;
		row.dwForwardMetric5	= (DWORD) - 1;

		addRoute = true;

		//
		// check to make sure we don't already have a route
		//
		if ( HaveRoute( &rowExtant, inet_addr( kLLNetworkAddr ), 0 ) )
		{
			//
			// set the age to 0 so that we can do a memcmp.
			//
			rowExtant.dwForwardAge = 0;

			//
			// check to see if this route is the same as our route
			//
			if (memcmp(&row, &rowExtant, sizeof(row)) != 0)
			{
				//
				// if it isn't then delete this entry
				//
				DeleteIpForwardEntry(&rowExtant);
			}
			else
			{
				//
				// else it is, so we don't want to create another route
				//
				addRoute = false;
			}
		}

		if (addRoute && row.dwForwardNextHop)
		{
			err = CreateIpForwardEntry(&row);
			check_noerr( err );
		}
	}

exit:

	return ( err );
}


//===========================================================================================================================
//	GetRouteDestination
//===========================================================================================================================

static OSStatus
GetRouteDestination(DWORD * ifIndex, DWORD * address)
{
	struct in_addr		ia;
	IP_ADAPTER_INFO	*	pAdapterInfo	=	NULL;
	IP_ADAPTER_INFO	*	pAdapter		=	NULL;
	ULONG				bufLen;
	mDNSBool			done			=	mDNSfalse;
	OSStatus			err;

	//
	// GetBestInterface will fail if there is no default gateway
	// configured.  If that happens, we will just take the first
	// interface in the list. MSDN support says there is no surefire
	// way to manually determine what the best interface might
	// be for a particular network address.
	//
	ia.s_addr	=	inet_addr(kLLNetworkAddr);
	err			=	GetBestInterface(*(IPAddr*) &ia, ifIndex);

	if (err)
	{
		*ifIndex = 0;
	}

	//
	// Make an initial call to GetAdaptersInfo to get
	// the necessary size into the bufLen variable
	//
	err = GetAdaptersInfo( NULL, &bufLen);
	require_action( err == ERROR_BUFFER_OVERFLOW, exit, err = kUnknownErr );

	pAdapterInfo = (IP_ADAPTER_INFO*) malloc( bufLen );
	require_action( pAdapterInfo, exit, err = kNoMemoryErr );
	
	err = GetAdaptersInfo( pAdapterInfo, &bufLen);
	require_noerr( err, exit );
	
	pAdapter	=	pAdapterInfo;
	err			=	kUnknownErr;
			
	// <rdar://problem/3718122>
	// <rdar://problem/5652098>
	//
	// Look for the Nortel VPN virtual interface, along with Juniper virtual interface.
	//
	// If these interfaces are active (i.e., has a non-zero IP Address),
	// then we want to disable routing table modifications.

	while (pAdapter)
	{
		if ( ( IsNortelVPN( pAdapter ) || IsJuniperVPN( pAdapter ) || IsCiscoVPN( pAdapter ) ) &&
			 ( inet_addr( pAdapter->IpAddressList.IpAddress.String ) != 0 ) )
		{
			dlog( kDebugLevelTrace, DEBUG_NAME "disabling routing table management due to VPN incompatibility" );
			goto exit;
		}

		pAdapter = pAdapter->Next;
	}

	while ( !done )
	{
		pAdapter	=	pAdapterInfo;
		err			=	kUnknownErr;

		while (pAdapter)
		{
			// If we don't have an interface selected, choose the first one that is of type ethernet and
			// has a valid IP Address

			if ((pAdapter->Type == MIB_IF_TYPE_ETHERNET) && ( IsValidAddress( pAdapter->IpAddressList.IpAddress.String ) ) && (!(*ifIndex) || (pAdapter->Index == (*ifIndex))))
			{
				*address =	inet_addr( pAdapter->IpAddressList.IpAddress.String );
				*ifIndex =  pAdapter->Index;
				err		 =	kNoErr;
				break;
			}
		
			pAdapter = pAdapter->Next;
		}

		// If we found the right interface, or we weren't trying to find a specific interface then we're done

		if ( !err || !( *ifIndex) )
		{
			done = mDNStrue;
		}

		// Otherwise, try again by wildcarding the interface

		else
		{
			*ifIndex = 0;
		}
	} 

exit:

	if ( pAdapterInfo != NULL )
	{
		free( pAdapterInfo );
	}

	return( err );
}


static bool
IsNortelVPN( IP_ADAPTER_INFO * pAdapter )
{
	return ((pAdapter->Type == MIB_IF_TYPE_ETHERNET) &&
		    (pAdapter->AddressLength == 6) &&
		    (pAdapter->Address[0] == 0x44) &&
		    (pAdapter->Address[1] == 0x45) &&
		    (pAdapter->Address[2] == 0x53) &&
		    (pAdapter->Address[3] == 0x54) &&
		    (pAdapter->Address[4] == 0x42) &&
			(pAdapter->Address[5] == 0x00)) ? true : false;
}


static bool
IsJuniperVPN( IP_ADAPTER_INFO * pAdapter )
{	
	return ( strnistr( pAdapter->Description, "Juniper", sizeof( pAdapter->Description  ) ) != NULL ) ? true : false;
}


static bool
IsCiscoVPN( IP_ADAPTER_INFO * pAdapter )
{
	return ((pAdapter->Type == MIB_IF_TYPE_ETHERNET) &&
		    (pAdapter->AddressLength == 6) &&
		    (pAdapter->Address[0] == 0x00) &&
		    (pAdapter->Address[1] == 0x05) &&
		    (pAdapter->Address[2] == 0x9a) &&
		    (pAdapter->Address[3] == 0x3c) &&
		    (pAdapter->Address[4] == 0x7a) &&
			(pAdapter->Address[5] == 0x00)) ? true : false;
}


static const char *
strnistr( const char * string, const char * subString, size_t max )
{
	size_t       subStringLen;
	size_t       offset;
	size_t       maxOffset;
	size_t       stringLen;
	const char * pPos;

	if ( ( string == NULL ) || ( subString == NULL ) )
	{
		return string;
	}

	stringLen = ( max > strlen( string ) ) ? strlen( string ) : max;

	if ( stringLen == 0 )
	{
		return NULL;
	}
	
	subStringLen = strlen( subString );

	if ( subStringLen == 0 )
	{
		return string;
	}

	if ( subStringLen > stringLen )
	{
		return NULL;
	}

	maxOffset = stringLen - subStringLen;
	pPos      = string;

	for ( offset = 0; offset <= maxOffset; offset++ )
	{
		if ( _strnicmp( pPos, subString, subStringLen ) == 0 )
		{
			return pPos;
		}

		pPos++;
	}

	return NULL;
}

