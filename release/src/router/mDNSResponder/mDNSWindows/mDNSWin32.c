/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
    
$Log: mDNSWin32.c,v $
Revision 1.145  2009/07/20 04:07:41  herscher
<rdar://problem/6145339> Bonjour does not list IPv6 loopback address when all network adapters are disabled

Revision 1.144  2009/07/17 19:59:46  herscher
<rdar://problem/7062660> Update the womp settings for each network adapter immediately preceding the call to mDNSCoreMachineSleep().

Revision 1.143  2009/07/07 21:34:58  herscher
<rdar://problem/6713286> windows platform changes to support use as sleep proxy client

Revision 1.142  2009/06/25 21:11:02  herscher
<rdar://problem/7003607> Platform layer doesn't correctly initialize the port field of TCP and UDP socket structures.

Revision 1.141  2009/06/22 23:25:05  herscher
<rdar://problem/5265747> ControlPanel doesn't display key and password in dialog box. Refactor Lsa calls into Secret.h and Secret.c, which is used by both the ControlPanel and mDNSResponder system service.

Revision 1.140  2009/04/24 04:55:26  herscher
<rdar://problem/3496833> Advertise SMB file sharing via Bonjour

Revision 1.139  2009/04/01 20:06:31  herscher
<rdar://problem/5629676> Corrupt computer name string used

Revision 1.138  2009/04/01 19:31:54  herscher
Remove extraneous printf

Revision 1.137  2009/04/01 17:50:15  mcguire
cleanup mDNSRandom

Revision 1.136  2009/03/30 20:53:10  herscher
<rdar://problem/5925472> Current Bonjour code does not compile on Windows
<rdar://problem/6220642> iTunes 8: Bonjour doesn't work after upgrading iTunes 8.
<rdar://problem/6145992> Reduce sleep from 3 seconds to 2 seconds
<rdar://problem/6145992> Bonjour Service sometimes only gets a IPv6 link-local address after a network changed event
<rdar://problem/6145913> Bonjour For Windows doesn't show IPv4 loopback intermittently when no network interfaces are active
<rdar://problem/6143633> Bonjour service does not publish ANY IPv6 address on Windows Vista
<rdar://problem/6136296> Bonjour 105A6: mDNSResponder is crashing on startup
<rdar://problem/6127927> B4Windows: uDNS: Should use randomized source ports and transaction IDs to avoid DNS cache poisoning
<rdar://problem/5270738> Remove Virtual PC check from mDNSResponder code

Revision 1.135  2009/01/13 05:31:35  mkrochma
<rdar://problem/6491367> Replace bzero, bcopy with mDNSPlatformMemZero, mDNSPlatformMemCopy, memset, memcpy

Revision 1.134  2008/10/23 22:33:25  cheshire
Changed "NOTE:" to "Note:" so that BBEdit 9 stops putting those comment lines into the funtion popup menu

Revision 1.133  2008/10/22 17:19:57  cheshire
Don't need to define BPF_fd any more (it's now per-interface, not global)

Revision 1.132  2008/10/03 23:34:08  cheshire
Added skeleton definition of mDNSPlatformSendRawPacket

Revision 1.131  2008/10/03 18:25:18  cheshire
Instead of calling "m->MainCallback" function pointer directly, call mDNSCore routine "mDNS_ConfigChanged(m);"

Revision 1.130  2007/11/16 18:53:56  cheshire
TCPSocketFlags needs to be first field of TCPSocket_struct

Revision 1.129  2007/10/17 22:52:26  cheshire
Get rid of unused mDNS_UpdateLLQs()

Revision 1.128  2007/09/12 19:23:17  cheshire
Get rid of unnecessary mDNSPlatformTCPIsConnected() routine

Revision 1.127  2007/07/20 00:54:22  cheshire
<rdar://problem/4641118> Need separate SCPreferences for per-user .Mac settings

Revision 1.126  2007/07/11 02:56:20  cheshire
<rdar://problem/5303807> Register IPv6-only hostname and don't create port mappings for AutoTunnel services
Remove unused mDNSPlatformDefaultRegDomainChanged

Revision 1.125  2007/06/20 01:10:13  cheshire
<rdar://problem/5280520> Sync iPhone changes into main mDNSResponder code

Revision 1.124  2007/04/26 00:35:16  cheshire
<rdar://problem/5140339> uDNS: Domain discovery not working over VPN
Fixes to make sure results update correctly when connectivity changes (e.g. a DNS server
inside the firewall may give answers where a public one gives none, and vice versa.)

Revision 1.123  2007/04/18 21:00:40  cheshire
Use mDNS_AddSearchDomain_CString() instead of MakeDomainNameFromDNSNameString ... mDNS_AddSearchDomain

Revision 1.122  2007/04/17 19:21:29  cheshire
<rdar://problem/5140339> Domain discovery not working over VPN

Revision 1.121  2007/04/05 20:40:37  cheshire
Remove unused mDNSPlatformTCPGetFlags()

Revision 1.120  2007/03/28 20:59:27  cheshire
<rdar://problem/4743285> Remove inappropriate use of IsPrivateV4Addr()

Revision 1.119  2007/03/28 15:56:38  cheshire
<rdar://problem/5085774> Add listing of NAT port mapping and GetAddrInfo requests in SIGINFO output

Revision 1.118  2007/03/22 18:31:49  cheshire
Put dst parameter first in mDNSPlatformStrCopy/mDNSPlatformMemCopy, like conventional Posix strcpy/memcpy

Revision 1.117  2007/03/21 00:30:07  cheshire
<rdar://problem/4789455> Multiple errors in DNameList-related code

Revision 1.116  2007/03/20 17:07:16  cheshire
Rename "struct uDNS_TCPSocket_struct" to "TCPSocket", "struct uDNS_UDPSocket_struct" to "UDPSocket"

Revision 1.115  2007/02/08 21:12:28  cheshire
<rdar://problem/4386497> Stop reading /etc/mDNSResponder.conf on every sleep/wake

Revision 1.114  2007/01/05 08:31:01  cheshire
Trim excessive "$Log" checkin history from before 2006
(checkin history still available via "cvs log ..." of course)

Revision 1.113  2007/01/04 23:12:20  cheshire
Remove unused mDNSPlatformDefaultBrowseDomainChanged

Revision 1.112  2006/12/22 20:59:51  cheshire
<rdar://problem/4742742> Read *all* DNS keys from keychain,
 not just key for the system-wide default registration domain

Revision 1.111  2006/12/19 22:43:56  cheshire
Fix compiler warnings

Revision 1.110  2006/09/27 00:47:40  herscher
Fix compile error caused by changes to the tcp callback api.

Revision 1.109  2006/08/14 23:25:21  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.108  2006/07/06 00:06:21  cheshire
<rdar://problem/4472014> Add Private DNS client functionality to mDNSResponder

Revision 1.107  2006/03/19 02:00:13  cheshire
<rdar://problem/4073825> Improve logic for delaying packets after repeated interface transitions

Revision 1.106  2006/02/26 19:31:05  herscher
<rdar://problem/4455038> Bonjour For Windows takes 90 seconds to start. This was caused by a bad interaction between the VirtualPC check, and the removal of the WMI dependency.  The problem was fixed by: 1) checking to see if WMI is running before trying to talk to it.  2) Retrying the VirtualPC check every 10 seconds upon failure, stopping after 10 unsuccessful tries.

	To Do:
	
	- Get unicode name of machine for nice name instead of just the host name.
	- Use the IPv6 Internet Connection Firewall API to allow IPv6 mDNS without manually changing the firewall.
	- Get DNS server address(es) from Windows and provide them to the uDNS layer.
	- Implement TCP support for truncated packets (only stubs now).	

*/

#include	<stdarg.h>
#include	<stddef.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#include	"CommonServices.h"
#include	"DebugServices.h"
#include	"Firewall.h"
#include	"RegNames.h"
#include	"Secret.h"
#include	<dns_sd.h>

#include	<Iphlpapi.h>
#if( !TARGET_OS_WINDOWS_CE )
	#include	<mswsock.h>
	#include	<process.h>
	#include	<ntsecapi.h>
	#include	<lm.h>
	#include	<winioctl.h>
	#include	<ntddndis.h>        // This defines the IOCTL constants.
#endif

#include	"mDNSEmbeddedAPI.h"
#include	"DNSCommon.h"
#include	"mDNSWin32.h"

#if 0
#pragma mark == Constants ==
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

#define	DEBUG_NAME									"[mDNSWin32] "

#define	MDNS_WINDOWS_USE_IPV6_IF_ADDRS				1
#define	MDNS_WINDOWS_ENABLE_IPV4					1
#define	MDNS_WINDOWS_ENABLE_IPV6					1
#define	MDNS_FIX_IPHLPAPI_PREFIX_BUG				1
#define MDNS_SET_HINFO_STRINGS						0

#define	kMDNSDefaultName							"My Computer"

#define	kWinSockMajorMin							2
#define	kWinSockMinorMin							2

#define	kWaitListCancelEvent						( WAIT_OBJECT_0 + 0 )
#define	kWaitListInterfaceListChangedEvent			( WAIT_OBJECT_0 + 1 )
#define	kWaitListWakeupEvent						( WAIT_OBJECT_0 + 2 )
#define kWaitListComputerDescriptionEvent			( WAIT_OBJECT_0 + 3 )
#define kWaitListTCPIPEvent							( WAIT_OBJECT_0 + 4 )
#define kWaitListDynDNSEvent						( WAIT_OBJECT_0 + 5 )
#define kWaitListFileShareEvent						( WAIT_OBJECT_0 + 6 )
#define kWaitListFirewallEvent						( WAIT_OBJECT_0 + 7 )
#define	kWaitListFixedItemCount						8 + MDNS_WINDOWS_ENABLE_IPV4 + MDNS_WINDOWS_ENABLE_IPV6

#define kRegistryMaxKeyLength						255
#define kRegistryMaxValueName						16383

#if( !TARGET_OS_WINDOWS_CE )
	static GUID										kWSARecvMsgGUID = WSAID_WSARECVMSG;
#endif

#define kIPv6IfIndexBase							(10000000L)
#define SMBPortAsNumber								445
#define DEVICE_PREFIX								"\\\\.\\"


#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

mDNSlocal mStatus			SetupSynchronizationObjects( mDNS * const inMDNS );
mDNSlocal mStatus			TearDownSynchronizationObjects( mDNS * const inMDNS );
mDNSlocal mStatus			SetupNiceName( mDNS * const inMDNS );
mDNSlocal mStatus			SetupHostName( mDNS * const inMDNS );
mDNSlocal mStatus			SetupName( mDNS * const inMDNS );
mDNSlocal mStatus			SetupInterfaceList( mDNS * const inMDNS );
mDNSlocal mStatus			TearDownInterfaceList( mDNS * const inMDNS );
mDNSlocal mStatus			SetupInterface( mDNS * const inMDNS, const struct ifaddrs *inIFA, mDNSInterfaceData **outIFD );
mDNSlocal mStatus			TearDownInterface( mDNS * const inMDNS, mDNSInterfaceData *inIFD );
mDNSlocal mStatus			SetupSocket( mDNS * const inMDNS, const struct sockaddr *inAddr, mDNSIPPort port, SocketRef *outSocketRef  );
mDNSlocal mStatus			SockAddrToMDNSAddr( const struct sockaddr * const inSA, mDNSAddr *outIP, mDNSIPPort *outPort );
mDNSlocal mStatus			SetupNotifications( mDNS * const inMDNS );
mDNSlocal mStatus			TearDownNotifications( mDNS * const inMDNS );

mDNSlocal mStatus			SetupThread( mDNS * const inMDNS );
mDNSlocal mStatus			TearDownThread( const mDNS * const inMDNS );
mDNSlocal unsigned WINAPI	ProcessingThread( LPVOID inParam );
mDNSlocal mStatus 			ProcessingThreadInitialize( mDNS * const inMDNS );
mDNSlocal mStatus			ProcessingThreadSetupWaitList( mDNS * const inMDNS, HANDLE **outWaitList, int *outWaitListCount );
mDNSlocal void				ProcessingThreadProcessPacket( mDNS *inMDNS, mDNSInterfaceData *inIFD, UDPSocket * inUDPSocket, SocketRef inSock );
mDNSlocal void				ProcessingThreadInterfaceListChanged( mDNS *inMDNS );
mDNSlocal void				ProcessingThreadComputerDescriptionChanged( mDNS * inMDNS );
mDNSlocal void				ProcessingThreadTCPIPConfigChanged( mDNS * inMDNS );
mDNSlocal void				ProcessingThreadDynDNSConfigChanged( mDNS * inMDNS );
mDNSlocal void				ProcessingThreadFileShareChanged( mDNS * inMDNS );
mDNSlocal void				ProcessingThreadFirewallChanged( mDNS * inMDNS );
mDNSlocal OSStatus			GetWindowsVersionString( char *inBuffer, size_t inBufferSize );

mDNSlocal int				getifaddrs( struct ifaddrs **outAddrs );
mDNSlocal void				freeifaddrs( struct ifaddrs *inAddrs );



// Platform Accessors

#ifdef	__cplusplus
	extern "C" {
#endif

typedef struct mDNSPlatformInterfaceInfo	mDNSPlatformInterfaceInfo;
struct	mDNSPlatformInterfaceInfo
{
	const char *		name;
	mDNSAddr			ip;
};

struct TCPSocket_struct
{
	TCPSocketFlags flags;		// MUST BE FIRST FIELD -- mDNSCore expects every TCPSocket_struct to begin with TCPSocketFlags flags
	SocketRef				fd;
	BOOL					connected;
	TCPConnectionCallback	callback;
	void				*	context;
	HANDLE					pendingEvent;
	TCPSocket *			next;
};

struct UDPSocket_struct
{
	mDNSIPPort					port; // MUST BE FIRST FIELD -- mDNSCoreReceive expects every UDPSocket_struct to begin with mDNSIPPort port
	SocketRef					sock;
	HANDLE						readEvent;
	mDNSAddr					dstAddr;
	LPFN_WSARECVMSG				recvMsgPtr;
	struct UDPSocket_struct *	next;
};





mDNSexport mStatus	mDNSPlatformInterfaceNameToID( mDNS * const inMDNS, const char *inName, mDNSInterfaceID *outID );
mDNSexport mStatus	mDNSPlatformInterfaceIDToInfo( mDNS * const inMDNS, mDNSInterfaceID inID, mDNSPlatformInterfaceInfo *outInfo );

// Utilities

#if( MDNS_WINDOWS_USE_IPV6_IF_ADDRS )
	mDNSlocal int	getifaddrs_ipv6( struct ifaddrs **outAddrs );
#endif

#if( !TARGET_OS_WINDOWS_CE )
	mDNSlocal int	getifaddrs_ipv4( struct ifaddrs **outAddrs );
#endif

#if( TARGET_OS_WINDOWS_CE )
	mDNSlocal int	getifaddrs_ce( struct ifaddrs **outAddrs );
#endif

mDNSlocal DWORD				GetPrimaryInterface();
mDNSlocal mStatus			AddressToIndexAndMask( struct sockaddr * address, uint32_t * index, struct sockaddr * mask );
mDNSlocal mDNSBool			CanReceiveUnicast( void );
mDNSlocal mDNSBool			IsPointToPoint( IP_ADAPTER_UNICAST_ADDRESS * addr );

mDNSlocal mStatus			StringToAddress( mDNSAddr * ip, LPSTR string );
mDNSlocal mStatus			RegQueryString( HKEY key, LPCSTR param, LPSTR * string, DWORD * stringLen, DWORD * enabled );
mDNSlocal struct ifaddrs*	myGetIfAddrs(int refresh);
mDNSlocal OSStatus			TCHARtoUTF8( const TCHAR *inString, char *inBuffer, size_t inBufferSize );
mDNSlocal OSStatus			WindowsLatin1toUTF8( const char *inString, char *inBuffer, size_t inBufferSize );
mDNSlocal void				FreeTCPSocket( TCPSocket *sock );
mDNSlocal void				FreeUDPSocket( UDPSocket * sock );
mDNSlocal mStatus           SetupAddr(mDNSAddr *ip, const struct sockaddr *const sa);
mDNSlocal void				GetDDNSFQDN( domainname *const fqdn );
#ifdef UNICODE
mDNSlocal void				GetDDNSDomains( DNameListElem ** domains, LPCWSTR lpSubKey );
#else
mDNSlocal void				GetDDNSDomains( DNameListElem ** domains, LPCSTR lpSubKey );
#endif
mDNSlocal void				SetDomainSecrets( mDNS * const m );
mDNSlocal void				SetDomainSecret( mDNS * const m, const domainname * inDomain );
mDNSlocal void				CheckFileShares( mDNS * const m );
mDNSlocal mDNSu8			IsWOMPEnabled( const char * adapterName );

#ifdef	__cplusplus
	}
#endif

#if 0
#pragma mark == Globals ==
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

mDNSlocal mDNS_PlatformSupport	gMDNSPlatformSupport;
mDNSs32							mDNSPlatformOneSecond = 0;
mDNSlocal TCPSocket *		gTCPConnectionList		= NULL;
mDNSlocal int					gTCPConnections			= 0;
mDNSlocal UDPSocket *				gUDPSocketList			= NULL;
mDNSlocal int						gUDPSockets				= 0;
mDNSlocal BOOL					gWaitListChanged		= FALSE;

#if( MDNS_WINDOWS_USE_IPV6_IF_ADDRS )

	typedef DWORD
		( WINAPI * GetAdaptersAddressesFunctionPtr )( 
			ULONG 					inFamily, 
			DWORD 					inFlags, 
			PVOID 					inReserved, 
			PIP_ADAPTER_ADDRESSES 	inAdapter, 
			PULONG					outBufferSize );

	mDNSlocal HMODULE								gIPHelperLibraryInstance			= NULL;
	mDNSlocal GetAdaptersAddressesFunctionPtr		gGetAdaptersAddressesFunctionPtr	= NULL;

#endif


#ifndef HCRYPTPROV
   typedef ULONG_PTR HCRYPTPROV;    // WinCrypt.h, line 249
#endif


#ifndef CRYPT_MACHINE_KEYSET
#	define CRYPT_MACHINE_KEYSET    0x00000020
#endif

#ifndef CRYPT_NEWKEYSET
#	define CRYPT_NEWKEYSET         0x00000008
#endif

#ifndef PROV_RSA_FULL
#  define PROV_RSA_FULL 1
#endif

typedef BOOL (__stdcall *fnCryptGenRandom)( HCRYPTPROV, DWORD, BYTE* ); 
typedef BOOL (__stdcall *fnCryptAcquireContext)( HCRYPTPROV*, LPCTSTR, LPCTSTR, DWORD, DWORD);
typedef BOOL (__stdcall *fnCryptReleaseContext)(HCRYPTPROV, DWORD);

static fnCryptAcquireContext g_lpCryptAcquireContext 	= NULL;
static fnCryptReleaseContext g_lpCryptReleaseContext 	= NULL;
static fnCryptGenRandom		 g_lpCryptGenRandom 		= NULL;
static HINSTANCE			 g_hAAPI32 					= NULL;
static HCRYPTPROV			 g_hProvider 				= ( ULONG_PTR ) NULL;


#if 0
#pragma mark -
#pragma mark == Platform Support ==
#endif

//===========================================================================================================================
//	mDNSPlatformInit
//===========================================================================================================================

mDNSexport mStatus	mDNSPlatformInit( mDNS * const inMDNS )
{
	mStatus		err;
	WSADATA		wsaData;
	int			supported;
	struct sockaddr_in	sa4;
	struct sockaddr_in6 sa6;
	int					sa4len;
	int					sa6len;
	
	dlog( kDebugLevelTrace, DEBUG_NAME "platform init\n" );
	
	// Initialize variables. If the PlatformSupport pointer is not null then just assume that a non-Apple client is 
	// calling mDNS_Init and wants to provide its own storage for the platform-specific data so do not overwrite it.
	
	mDNSPlatformMemZero( &gMDNSPlatformSupport, sizeof( gMDNSPlatformSupport ) );
	if( !inMDNS->p ) inMDNS->p				= &gMDNSPlatformSupport;
	inMDNS->p->interfaceListChangedSocket	= kInvalidSocketRef;
	mDNSPlatformOneSecond 					= 1000;		// Use milliseconds as the quantum of time
	
	// Startup WinSock 2.2 or later.
	
	err = WSAStartup( MAKEWORD( kWinSockMajorMin, kWinSockMinorMin ), &wsaData );
	require_noerr( err, exit );
	
	supported = ( ( LOBYTE( wsaData.wVersion ) == kWinSockMajorMin ) && ( HIBYTE( wsaData.wVersion ) == kWinSockMinorMin ) );
	require_action( supported, exit, err = mStatus_UnsupportedErr );
	
	inMDNS->CanReceiveUnicastOn5353 = CanReceiveUnicast();
	
	// Setup the HINFO HW/SW strings.
	
#if ( MDNS_SET_HINFO_STRINGS )
	err = GetWindowsVersionString( (char *) &inMDNS->HIHardware.c[ 1 ], sizeof( inMDNS->HIHardware.c ) - 2 );
	check_noerr( err );
	// Note that GetWindowsVersionString guarantees that the resulting string is always null-terminated,
	// so the following strlen call is safe
	inMDNS->HIHardware.c[ 0 ] = (mDNSu8) mDNSPlatformStrLen( &inMDNS->HIHardware.c[ 1 ] );
	dlog( kDebugLevelInfo, DEBUG_NAME "HIHardware: %#s\n", inMDNS->HIHardware.c );
	
	mDNS_snprintf( (char *) &inMDNS->HISoftware.c[ 1 ], sizeof( inMDNS->HISoftware.c ) - 2, 
		"mDNSResponder (%s %s)", __DATE__, __TIME__ );
	inMDNS->HISoftware.c[ 0 ] = (mDNSu8) mDNSPlatformStrLen( &inMDNS->HISoftware.c[ 1 ] );
	dlog( kDebugLevelInfo, DEBUG_NAME "HISoftware: %#s\n", inMDNS->HISoftware.c );
#endif
	
	// Set up the IPv4 unicast socket

	inMDNS->p->unicastSock4				= INVALID_SOCKET;
	inMDNS->p->unicastSock4ReadEvent	= NULL;
	inMDNS->p->unicastSock4RecvMsgPtr	= NULL;

#if ( MDNS_WINDOWS_ENABLE_IPV4 )

	sa4.sin_family		= AF_INET;
	sa4.sin_addr.s_addr = INADDR_ANY;
	err = SetupSocket( inMDNS, (const struct sockaddr*) &sa4, zeroIPPort, &inMDNS->p->unicastSock4 );
	check_noerr( err );
	sa4len = sizeof( sa4 );
	err = getsockname( inMDNS->p->unicastSock4, (struct sockaddr*) &sa4, &sa4len );
	require_noerr( err, exit );
	inMDNS->UnicastPort4.NotAnInteger = sa4.sin_port;
	inMDNS->p->unicastSock4ReadEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	err = translate_errno( inMDNS->p->unicastSock4ReadEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );
	err = WSAEventSelect( inMDNS->p->unicastSock4, inMDNS->p->unicastSock4ReadEvent, FD_READ );
	require_noerr( err, exit );
#if( !TARGET_OS_WINDOWS_CE )
	{
		DWORD size;

		err = WSAIoctl( inMDNS->p->unicastSock4, SIO_GET_EXTENSION_FUNCTION_POINTER, &kWSARecvMsgGUID, 
							sizeof( kWSARecvMsgGUID ), &inMDNS->p->unicastSock4RecvMsgPtr, sizeof( inMDNS->p->unicastSock4RecvMsgPtr ), &size, NULL, NULL );
		
		if ( err )
		{
			inMDNS->p->unicastSock4RecvMsgPtr = NULL;
		}
	}
#endif

#endif

	// Set up the IPv6 unicast socket

	inMDNS->p->unicastSock6				= INVALID_SOCKET;
	inMDNS->p->unicastSock6ReadEvent	= NULL;
	inMDNS->p->unicastSock6RecvMsgPtr	= NULL;

#if ( MDNS_WINDOWS_ENABLE_IPV6 )

	sa6.sin6_family		= AF_INET6;
	sa6.sin6_addr		= in6addr_any;
	sa6.sin6_scope_id	= 0;

	// This call will fail if the machine hasn't installed IPv6.  In that case,
	// the error will be WSAEAFNOSUPPORT.

	err = SetupSocket( inMDNS, (const struct sockaddr*) &sa6, zeroIPPort, &inMDNS->p->unicastSock6 );
	require_action( !err || ( err == WSAEAFNOSUPPORT ), exit, err = (mStatus) WSAGetLastError() );
	inMDNS->p->unicastSock6ReadEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	err = translate_errno( inMDNS->p->unicastSock6ReadEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );
	
	// If we weren't able to create the socket (because IPv6 hasn't been installed) don't do this

	if ( inMDNS->p->unicastSock6 != INVALID_SOCKET )
	{
		sa6len = sizeof( sa6 );
		err = getsockname( inMDNS->p->unicastSock6, (struct sockaddr*) &sa6, &sa6len );
		require_noerr( err, exit );
		inMDNS->UnicastPort6.NotAnInteger = sa6.sin6_port;

		err = WSAEventSelect( inMDNS->p->unicastSock6, inMDNS->p->unicastSock6ReadEvent, FD_READ );
		require_noerr( err, exit );

#if( !TARGET_OS_WINDOWS_CE )
		{
			DWORD size;

			err = WSAIoctl( inMDNS->p->unicastSock6, SIO_GET_EXTENSION_FUNCTION_POINTER, &kWSARecvMsgGUID, 
						sizeof( kWSARecvMsgGUID ), &inMDNS->p->unicastSock6RecvMsgPtr, sizeof( inMDNS->p->unicastSock6RecvMsgPtr ), &size, NULL, NULL );
		
			if ( err != 0 )
			{
				inMDNS->p->unicastSock6RecvMsgPtr = NULL;
			}
		}
#endif
	}

#endif

	// Set up the mDNS thread.
	
	err = SetupSynchronizationObjects( inMDNS );
	require_noerr( err, exit );
	
	err = SetupThread( inMDNS );
	require_noerr( err, exit );

	// Notify core of domain secret keys

	SetDomainSecrets( inMDNS );
	
	// Success!

	mDNSCoreInitComplete( inMDNS, err );

	// See if we need to advertise file sharing

	inMDNS->p->smbRegistered = mDNSfalse;
	CheckFileShares( inMDNS );
	
exit:
	if( err )
	{
		mDNSPlatformClose( inMDNS );
	}
	dlog( kDebugLevelTrace, DEBUG_NAME "platform init done (err=%d %m)\n", err, err );
	return( err );
}

//===========================================================================================================================
//	mDNSPlatformClose
//===========================================================================================================================

mDNSexport void	mDNSPlatformClose( mDNS * const inMDNS )
{
	mStatus		err;
	
	dlog( kDebugLevelTrace, DEBUG_NAME "platform close\n" );
	check( inMDNS );
	
	// Tear everything down in reverse order to how it was set up.
		
	err = TearDownThread( inMDNS );
	check_noerr( err );
	
	err = TearDownInterfaceList( inMDNS );
	check_noerr( err );
	check( !inMDNS->p->inactiveInterfaceList );
		
	err = TearDownSynchronizationObjects( inMDNS );
	check_noerr( err );

#if ( MDNS_WINDOWS_ENABLE_IPV4 )

	if ( inMDNS->p->unicastSock4ReadEvent )
	{
		CloseHandle( inMDNS->p->unicastSock4ReadEvent );
		inMDNS->p->unicastSock4ReadEvent = 0;
	}
	
	if ( IsValidSocket( inMDNS->p->unicastSock4 ) )
	{
		close_compat( inMDNS->p->unicastSock4 );
	}

#endif
	
#if ( MDNS_WINDOWS_ENABLE_IPV6 )

	if ( inMDNS->p->unicastSock6ReadEvent )
	{
		CloseHandle( inMDNS->p->unicastSock6ReadEvent );
		inMDNS->p->unicastSock6ReadEvent = 0;
	}
	
	if ( IsValidSocket( inMDNS->p->unicastSock6 ) )
	{
		close_compat( inMDNS->p->unicastSock6 );
	}

#endif

	// Free the DLL needed for IPv6 support.
	
#if( MDNS_WINDOWS_USE_IPV6_IF_ADDRS )
	if( gIPHelperLibraryInstance )
	{
		gGetAdaptersAddressesFunctionPtr = NULL;
		
		FreeLibrary( gIPHelperLibraryInstance );
		gIPHelperLibraryInstance = NULL;
	}
#endif

	if ( g_hAAPI32 )
	{
		// Release any resources

		if ( g_hProvider && g_lpCryptReleaseContext )
		{
			( g_lpCryptReleaseContext )( g_hProvider, 0 );
		}

		// Free the AdvApi32.dll

		FreeLibrary( g_hAAPI32 );

		// And reset all the data

		g_lpCryptAcquireContext = NULL;
		g_lpCryptReleaseContext = NULL;
		g_lpCryptGenRandom 		= NULL;
		g_hProvider 			= ( ULONG_PTR ) NULL;
		g_hAAPI32				= NULL;
	}

	WSACleanup();
	
	dlog( kDebugLevelTrace, DEBUG_NAME "platform close done\n" );
}


//===========================================================================================================================
//	mDNSPlatformLock
//===========================================================================================================================

mDNSexport void	mDNSPlatformLock( const mDNS * const inMDNS )
{
	check( inMDNS );
	
	if ( inMDNS->p->lockInitialized )
	{
		EnterCriticalSection( &inMDNS->p->lock );
	}
}

//===========================================================================================================================
//	mDNSPlatformUnlock
//===========================================================================================================================

mDNSexport void	mDNSPlatformUnlock( const mDNS * const inMDNS )
{
	check( inMDNS );
	check( inMDNS->p );

	if ( inMDNS->p->lockInitialized )
	{
		check( inMDNS->p->threadID );
	
		// Signal a wakeup event if when called from a task other than the mDNS task since if we are called from mDNS task, 
		// we'll loop back and call mDNS_Execute anyway. Signaling is needed to re-evaluate the wakeup via mDNS_Execute.
	
		if( GetCurrentThreadId() != inMDNS->p->threadID )
		{
			BOOL		wasSet;
		
			wasSet = SetEvent( inMDNS->p->wakeupEvent );
			check_translated_errno( wasSet, GetLastError(), kUnknownErr );
		}
		LeaveCriticalSection( &inMDNS->p->lock );
	}
}

//===========================================================================================================================
//	mDNSPlatformStrCopy
//===========================================================================================================================

mDNSexport void	mDNSPlatformStrCopy( void *inDst, const void *inSrc )
{
	check( inSrc );
	check( inDst );
	
	strcpy( (char *) inDst, (const char*) inSrc );
}

//===========================================================================================================================
//	mDNSPlatformStrLen
//===========================================================================================================================

mDNSexport mDNSu32	mDNSPlatformStrLen( const void *inSrc )
{
	check( inSrc );
	
	return( (mDNSu32) strlen( (const char *) inSrc ) );
}

//===========================================================================================================================
//	mDNSPlatformMemCopy
//===========================================================================================================================

mDNSexport void	mDNSPlatformMemCopy( void *inDst, const void *inSrc, mDNSu32 inSize )
{
	check( inSrc );
	check( inDst );
	
	memcpy( inDst, inSrc, inSize );
}

//===========================================================================================================================
//	mDNSPlatformMemSame
//===========================================================================================================================

mDNSexport mDNSBool	mDNSPlatformMemSame( const void *inDst, const void *inSrc, mDNSu32 inSize )
{
	check( inSrc );
	check( inDst );
	
	return( (mDNSBool)( memcmp( inSrc, inDst, inSize ) == 0 ) );
}

//===========================================================================================================================
//	mDNSPlatformMemZero
//===========================================================================================================================

mDNSexport void	mDNSPlatformMemZero( void *inDst, mDNSu32 inSize )
{
	check( inDst );
	
	memset( inDst, 0, inSize );
}

//===========================================================================================================================
//	mDNSPlatformMemAllocate
//===========================================================================================================================

mDNSexport void *	mDNSPlatformMemAllocate( mDNSu32 inSize )
{
	void *		mem;
	
	check( inSize > 0 );
	
	mem = malloc( inSize );
	check( mem );
	
	return( mem );
}

//===========================================================================================================================
//	mDNSPlatformMemFree
//===========================================================================================================================

mDNSexport void	mDNSPlatformMemFree( void *inMem )
{
	check( inMem );
	
	free( inMem );
}

//===========================================================================================================================
//	mDNSPlatformRandomNumber
//===========================================================================================================================

mDNSexport mDNSu32 mDNSPlatformRandomNumber(void)
{
	mDNSu32		randomNumber = 0;
	BOOL		bResult;
	OSStatus	err = 0;

	if ( !g_hAAPI32 )
	{
		g_hAAPI32 = LoadLibrary( TEXT("AdvAPI32.dll") );
		err = translate_errno( g_hAAPI32 != NULL, GetLastError(), mStatus_UnknownErr );
		require_noerr( err, exit );
	}

	// Function Pointer: CryptAcquireContext

	if ( !g_lpCryptAcquireContext )
	{
		g_lpCryptAcquireContext = ( fnCryptAcquireContext )
#ifdef UNICODE
			( GetProcAddress( g_hAAPI32, "CryptAcquireContextW" ) );
#else
			( GetProcAddress( g_hAAPI32, "CryptAcquireContextA" ) );
#endif
		err = translate_errno( g_lpCryptAcquireContext != NULL, GetLastError(), mStatus_UnknownErr );
		require_noerr( err, exit );
	}

	// Function Pointer: CryptReleaseContext

	if ( !g_lpCryptReleaseContext )
	{
		g_lpCryptReleaseContext = ( fnCryptReleaseContext )
         ( GetProcAddress( g_hAAPI32, "CryptReleaseContext" ) );
		err = translate_errno( g_lpCryptReleaseContext != NULL, GetLastError(), mStatus_UnknownErr );
		require_noerr( err, exit );
	}
      
	// Function Pointer: CryptGenRandom

	if ( !g_lpCryptGenRandom )
	{
		g_lpCryptGenRandom = ( fnCryptGenRandom )
          ( GetProcAddress( g_hAAPI32, "CryptGenRandom" ) );
		err = translate_errno( g_lpCryptGenRandom != NULL, GetLastError(), mStatus_UnknownErr );
		require_noerr( err, exit );
	}

	// Setup

	if ( !g_hProvider )
	{
		bResult = (*g_lpCryptAcquireContext)( &g_hProvider, NULL, NULL, PROV_RSA_FULL, CRYPT_MACHINE_KEYSET );

		if ( !bResult )
		{
			bResult =  ( *g_lpCryptAcquireContext)( &g_hProvider, NULL, NULL, PROV_RSA_FULL, CRYPT_MACHINE_KEYSET | CRYPT_NEWKEYSET );
			err = translate_errno( bResult, GetLastError(), mStatus_UnknownErr );
			require_noerr( err, exit );
		}
	}

	bResult = (*g_lpCryptGenRandom)( g_hProvider, sizeof( randomNumber ), ( BYTE* ) &randomNumber );
	err = translate_errno( bResult, GetLastError(), mStatus_UnknownErr );
	require_noerr( err, exit );

exit:

	if ( err )
	{
		randomNumber = rand();
	}

	return randomNumber;
}

//===========================================================================================================================
//	mDNSPlatformTimeInit
//===========================================================================================================================

mDNSexport mStatus	mDNSPlatformTimeInit( void )
{
	// No special setup is required on Windows -- we just use GetTickCount().
	return( mStatus_NoError );
}

//===========================================================================================================================
//	mDNSPlatformRawTime
//===========================================================================================================================

mDNSexport mDNSs32	mDNSPlatformRawTime( void )
{
	return( (mDNSs32) GetTickCount() );
}

//===========================================================================================================================
//	mDNSPlatformUTC
//===========================================================================================================================

mDNSexport mDNSs32	mDNSPlatformUTC( void )
{
	return ( mDNSs32 ) time( NULL );
}

//===========================================================================================================================
//	mDNSPlatformInterfaceNameToID
//===========================================================================================================================

mDNSexport mStatus	mDNSPlatformInterfaceNameToID( mDNS * const inMDNS, const char *inName, mDNSInterfaceID *outID )
{
	mStatus					err;
	mDNSInterfaceData *		ifd;
	
	check( inMDNS );
	check( inMDNS->p );
	check( inName );
	
	// Search for an interface with the specified name,
	
	for( ifd = inMDNS->p->interfaceList; ifd; ifd = ifd->next )
	{
		if( strcmp( ifd->name, inName ) == 0 )
		{
			break;
		}
	}
	require_action_quiet( ifd, exit, err = mStatus_NoSuchNameErr );
	
	// Success!
	
	if( outID )
	{
		*outID = (mDNSInterfaceID) ifd;
	}
	err = mStatus_NoError;
	
exit:
	return( err );
}

//===========================================================================================================================
//	mDNSPlatformInterfaceIDToInfo
//===========================================================================================================================

mDNSexport mStatus	mDNSPlatformInterfaceIDToInfo( mDNS * const inMDNS, mDNSInterfaceID inID, mDNSPlatformInterfaceInfo *outInfo )
{
	mStatus					err;
	mDNSInterfaceData *		ifd;
	
	check( inMDNS );
	check( inID );
	check( outInfo );
	
	// Search for an interface with the specified ID,
	
	for( ifd = inMDNS->p->interfaceList; ifd; ifd = ifd->next )
	{
		if( ifd == (mDNSInterfaceData *) inID )
		{
			break;
		}
	}
	require_action_quiet( ifd, exit, err = mStatus_NoSuchNameErr );
	
	// Success!
	
	outInfo->name 	= ifd->name;
	outInfo->ip 	= ifd->interfaceInfo.ip;
	err 			= mStatus_NoError;
	
exit:
	return( err );
}

//===========================================================================================================================
//	mDNSPlatformInterfaceIDfromInterfaceIndex
//===========================================================================================================================

mDNSexport mDNSInterfaceID	mDNSPlatformInterfaceIDfromInterfaceIndex( mDNS * const inMDNS, mDNSu32 inIndex )
{
	mDNSInterfaceID		id;
	
	id = mDNSNULL;
	if( inIndex == kDNSServiceInterfaceIndexLocalOnly )
	{
		id = mDNSInterface_LocalOnly;
	}
	else if( inIndex != 0 )
	{
		mDNSInterfaceData *		ifd;
		
		for( ifd = inMDNS->p->interfaceList; ifd; ifd = ifd->next )
		{
			if( ( ifd->scopeID == inIndex ) && ifd->interfaceInfo.InterfaceActive )
			{
				id = ifd->interfaceInfo.InterfaceID;
				break;
			}
		}
		check( ifd );
	}
	return( id );
}

//===========================================================================================================================
//	mDNSPlatformInterfaceIndexfromInterfaceID
//===========================================================================================================================
	
mDNSexport mDNSu32	mDNSPlatformInterfaceIndexfromInterfaceID( mDNS * const inMDNS, mDNSInterfaceID inID )
{
	mDNSu32		index;
	
	index = 0;
	if( inID == mDNSInterface_LocalOnly )
	{
		index = (mDNSu32) kDNSServiceInterfaceIndexLocalOnly;
	}
	else if( inID )
	{
		mDNSInterfaceData *		ifd;
		
		// Search active interfaces.
		for( ifd = inMDNS->p->interfaceList; ifd; ifd = ifd->next )
		{
			if( (mDNSInterfaceID) ifd == inID )
			{
				index = ifd->scopeID;
				break;
			}
		}
		
		// Search inactive interfaces too so remove events for inactive interfaces report the old interface index.
		
		if( !ifd )
		{
			for( ifd = inMDNS->p->inactiveInterfaceList; ifd; ifd = ifd->next )
			{
				if( (mDNSInterfaceID) ifd == inID )
				{
					index = ifd->scopeID;
					break;
				}
			}
		}
		check( ifd );
	}
	return( index );
}

//===========================================================================================================================
//	mDNSPlatformTCPSocket
//===========================================================================================================================

TCPSocket *
mDNSPlatformTCPSocket
	(
	mDNS			* const m,
	TCPSocketFlags		flags,
	mDNSIPPort			*	port 
	)
{
	TCPSocket *		sock    = NULL;
	u_long				on		= 1;  // "on" for setsockopt
	struct sockaddr_in	saddr;
	int					len;
	mStatus				err		= mStatus_NoError;

	DEBUG_UNUSED( m );

	require_action( flags == 0, exit, err = mStatus_UnsupportedErr );

	// Setup connection data object

	sock = (TCPSocket *) malloc( sizeof( TCPSocket ) );
	require_action( sock, exit, err = mStatus_NoMemoryErr );
	mDNSPlatformMemZero( sock, sizeof( TCPSocket ) );

	sock->fd		= INVALID_SOCKET;
	sock->flags		= flags;

	mDNSPlatformMemZero(&saddr, sizeof(saddr));
	saddr.sin_family		= AF_INET;
	saddr.sin_addr.s_addr	= htonl( INADDR_ANY );
	saddr.sin_port			= port->NotAnInteger;
	
	// Create the socket

	sock->fd = socket(AF_INET, SOCK_STREAM, 0);
	err = translate_errno( sock->fd != INVALID_SOCKET, WSAGetLastError(), mStatus_UnknownErr );
	require_noerr( err, exit );

	// bind

	err = bind( sock->fd, ( struct sockaddr* ) &saddr, sizeof( saddr )  );
	err = translate_errno( err == 0, WSAGetLastError(), mStatus_UnknownErr );
	require_noerr( err, exit );

	// Set it to be non-blocking

	err = ioctlsocket( sock->fd, FIONBIO, &on );
	err = translate_errno( err == 0, WSAGetLastError(), mStatus_UnknownErr );
	require_noerr( err, exit );

	// Get port number

	mDNSPlatformMemZero( &saddr, sizeof( saddr ) );
	len = sizeof( saddr );

	err = getsockname( sock->fd, ( struct sockaddr* ) &saddr, &len );
	err = translate_errno( err == 0, WSAGetLastError(), mStatus_UnknownErr );
	require_noerr( err, exit );

	port->NotAnInteger = saddr.sin_port;

exit:

	if ( err && sock )
	{
		FreeTCPSocket( sock );
		sock = mDNSNULL;
	}

	return sock;
} 

//===========================================================================================================================
//	mDNSPlatformTCPConnect
//===========================================================================================================================

mStatus
mDNSPlatformTCPConnect
	(
	TCPSocket *			sock,
	const mDNSAddr		*	inDstIP, 
	mDNSOpaque16 			inDstPort, 
	mDNSInterfaceID			inInterfaceID,
	TCPConnectionCallback	inCallback, 
	void *					inContext
	)
{
	struct sockaddr_in	saddr;
	mStatus				err		= mStatus_NoError;

	DEBUG_UNUSED( inInterfaceID );
	
	if ( inDstIP->type != mDNSAddrType_IPv4 )
	{
		LogMsg("ERROR: mDNSPlatformTCPConnect - attempt to connect to an IPv6 address: operation not supported");
		return mStatus_UnknownErr;
	}

	// Setup connection data object

	sock->callback = inCallback;
	sock->context = inContext;

	mDNSPlatformMemZero(&saddr, sizeof(saddr));
	saddr.sin_family	= AF_INET;
	saddr.sin_port		= inDstPort.NotAnInteger;
	memcpy(&saddr.sin_addr, &inDstIP->ip.v4.NotAnInteger, sizeof(saddr.sin_addr));

	// Try and do connect

	err = connect( sock->fd, ( struct sockaddr* ) &saddr, sizeof( saddr ) );
	require_action( !err || ( WSAGetLastError() == WSAEWOULDBLOCK ), exit, err = mStatus_ConnFailed );
	sock->connected		= !err ? TRUE : FALSE;
	sock->pendingEvent	= CreateEvent( NULL, FALSE, FALSE, NULL );
	err = translate_errno( sock->pendingEvent, GetLastError(), mStatus_UnknownErr );
	require_noerr( err, exit );
	err = WSAEventSelect( sock->fd, sock->pendingEvent, FD_CONNECT|FD_READ|FD_CLOSE );
	require_noerr( err, exit );

	// Bookkeeping

	sock->next			= gTCPConnectionList;
	gTCPConnectionList	= sock;
	gTCPConnections++;
	gWaitListChanged	= TRUE;

exit:

	if ( !err )
	{
		err = sock->connected ? mStatus_ConnEstablished : mStatus_ConnPending;
	}

	return err;
}

//===========================================================================================================================
//	mDNSPlatformTCPAccept
//===========================================================================================================================

mDNSexport 
mDNSexport TCPSocket *mDNSPlatformTCPAccept( TCPSocketFlags flags, int fd )
	{
	TCPSocket	*	sock = NULL;
	mStatus							err = mStatus_NoError;

	require_action( !flags, exit, err = mStatus_UnsupportedErr );

	sock = malloc( sizeof( TCPSocket ) );
	require_action( sock, exit, err = mStatus_NoMemoryErr );
	
	mDNSPlatformMemZero( sock, sizeof( *sock ) );

	sock->fd	= fd;
	sock->flags = flags;

exit:

	if ( err && sock )
	{
		free( sock );
		sock = NULL;
	}

	return sock;
	}


//===========================================================================================================================
//	mDNSPlatformTCPCloseConnection
//===========================================================================================================================

mDNSexport void	mDNSPlatformTCPCloseConnection( TCPSocket *sock )
{
	TCPSocket *	inserted  = gTCPConnectionList;
	TCPSocket *	last = NULL;

	while ( inserted )
	{
		if ( inserted == sock )
		{
			if ( last == NULL )
			{
				gTCPConnectionList = inserted->next;
			}
			else
			{
				last->next = inserted->next;
			}

			gTCPConnections--;
			gWaitListChanged = TRUE;

			break;
		}

		last		= inserted;
		inserted 	= inserted->next;
	}

	FreeTCPSocket( sock );
}

//===========================================================================================================================
//	mDNSPlatformReadTCP
//===========================================================================================================================

mDNSexport long	mDNSPlatformReadTCP( TCPSocket *sock, void *inBuffer, unsigned long inBufferSize, mDNSBool * closed )
{
	int	nread;

	nread = recv( sock->fd, inBuffer, inBufferSize, 0);

	if ( nread < 0 )
	{
		if ( WSAGetLastError() == WSAEWOULDBLOCK )
		{
			nread = 0;
		}
		else
		{
			nread = -1;
		}
	}
	else if ( !nread )
	{
		*closed = mDNStrue;
	}
		
	return nread;
}

//===========================================================================================================================
//	mDNSPlatformWriteTCP
//===========================================================================================================================

mDNSexport long	mDNSPlatformWriteTCP( TCPSocket *sock, const char *inMsg, unsigned long inMsgSize )
{
	int			nsent;
	OSStatus	err;

	nsent = send( sock->fd, inMsg, inMsgSize, 0 );

	err = translate_errno( ( nsent >= 0 ) || ( WSAGetLastError() == WSAEWOULDBLOCK ), WSAGetLastError(), mStatus_UnknownErr );
	require_noerr( err, exit );

	if ( nsent < 0)
	{
		nsent = 0;
	}
		
exit:

	return nsent;
}

//===========================================================================================================================
//	mDNSPlatformTCPGetFD
//===========================================================================================================================

mDNSexport int mDNSPlatformTCPGetFD(TCPSocket *sock )
    {
    return ( int ) sock->fd;
    }

	
//===========================================================================================================================
//	mDNSPlatformUDPSocket
//===========================================================================================================================

mDNSexport UDPSocket* mDNSPlatformUDPSocket(mDNS *const m, const mDNSIPPort requestedport)
{
	UDPSocket*	sock	= NULL;
	mDNSIPPort	port	= requestedport;
	mStatus		err		= mStatus_NoError;
	unsigned	i;

	// Setup connection data object

	sock = (struct UDPSocket_struct*) malloc( sizeof( struct UDPSocket_struct ) );
	require_action( sock, exit, err = mStatus_NoMemoryErr );
	memset( sock, 0, sizeof( struct UDPSocket_struct ) );

	// Create the socket

	sock->recvMsgPtr	= m->p->unicastSock4RecvMsgPtr;
	sock->dstAddr		= m->p->unicastSock4DestAddr;
	sock->sock			= INVALID_SOCKET;

	// Try at most 10000 times to get a unique random port

	for (i=0; i<10000; i++)
	{
		struct sockaddr_in saddr;

		saddr.sin_family		= AF_INET;
		saddr.sin_addr.s_addr	= 0;

		// The kernel doesn't do cryptographically strong random port
		// allocation, so we do it ourselves here

        if (mDNSIPPortIsZero(requestedport))
		{
			port = mDNSOpaque16fromIntVal( ( mDNSu16 ) ( 0xC000 + mDNSRandom(0x3FFF) ) );
		}

		saddr.sin_port = port.NotAnInteger;

        err = SetupSocket(m, ( struct sockaddr* ) &saddr, port, &sock->sock );
        if (!err) break;
	}

	require_noerr( err, exit );

	// Set the port

	sock->port = port;

	// Set it up with Windows Eventing

	sock->readEvent	= CreateEvent( NULL, FALSE, FALSE, NULL );
	err = translate_errno( sock->readEvent, GetLastError(), mStatus_UnknownErr );
	require_noerr( err, exit );
	err = WSAEventSelect( sock->sock, sock->readEvent, FD_READ );
	require_noerr( err, exit );

	// Bookkeeping

	sock->next			= gUDPSocketList;
	gUDPSocketList		= sock;
	gUDPSockets++;
	gWaitListChanged	= TRUE;

exit:

	if ( err )
	{
		if ( sock )
		{
			FreeUDPSocket( sock );
			sock = NULL;
		}
	}

	return sock;
}
	
//===========================================================================================================================
//	mDNSPlatformUDPClose
//===========================================================================================================================
	
mDNSexport void mDNSPlatformUDPClose( UDPSocket *sock )
{
	UDPSocket	*	current  = gUDPSocketList;
	UDPSocket	*	last = NULL;

	while ( current )
	{
		if ( current == sock )
		{
			if ( last == NULL )
			{
				gUDPSocketList = sock->next;
			}
			else
			{
				last->next = sock->next;
			}

			FreeUDPSocket( sock );

			gUDPSockets--;
			gWaitListChanged = TRUE;

			break;
		}

		last	= current;
		current	= current->next;
	}
}


//===========================================================================================================================
//	mDNSPlatformSendUDP
//===========================================================================================================================

mDNSexport mStatus
	mDNSPlatformSendUDP( 
		const mDNS * const			inMDNS, 
		const void * const	        inMsg, 
		const mDNSu8 * const		inMsgEnd, 
		mDNSInterfaceID 			inInterfaceID, 
		UDPSocket *					inSrcSocket,
		const mDNSAddr *			inDstIP, 
		mDNSIPPort 					inDstPort )
{
	SOCKET						sendingsocket = INVALID_SOCKET;
	mStatus						err = mStatus_NoError;
	mDNSInterfaceData *			ifd = (mDNSInterfaceData*) inInterfaceID;
	struct sockaddr_storage		addr;
	int							n;
	
	DEBUG_USE_ONLY( inMDNS );
	
	n = (int)( inMsgEnd - ( (const mDNSu8 * const) inMsg ) );
	check( inMDNS );
	check( inMsg );
	check( inMsgEnd );
	check( inDstIP );
	
	dlog( kDebugLevelChatty, DEBUG_NAME "platform send %d bytes to %#a:%u\n", n, inDstIP, ntohs( inDstPort.NotAnInteger ) );
	
	if( inDstIP->type == mDNSAddrType_IPv4 )
	{
		struct sockaddr_in *		sa4;
		
		sa4						= (struct sockaddr_in *) &addr;
		sa4->sin_family			= AF_INET;
		sa4->sin_port			= inDstPort.NotAnInteger;
		sa4->sin_addr.s_addr	= inDstIP->ip.v4.NotAnInteger;
		sendingsocket           = ifd ? ifd->sock : inMDNS->p->unicastSock4;

		if (inSrcSocket) { sendingsocket = inSrcSocket->sock; debugf("mDNSPlatformSendUDP using port %d, static port %d, sock %d", mDNSVal16(inSrcSocket->port), inMDNS->p->unicastSock4, sendingsocket); }
	}
	else if( inDstIP->type == mDNSAddrType_IPv6 )
	{
		struct sockaddr_in6 *		sa6;
		
		sa6					= (struct sockaddr_in6 *) &addr;
		sa6->sin6_family	= AF_INET6;
		sa6->sin6_port		= inDstPort.NotAnInteger;
		sa6->sin6_flowinfo	= 0;
		sa6->sin6_addr		= *( (struct in6_addr *) &inDstIP->ip.v6 );
		sa6->sin6_scope_id	= 0;	// Windows requires the scope ID to be zero. IPV6_MULTICAST_IF specifies interface.
		sendingsocket		= ifd ? ifd->sock : inMDNS->p->unicastSock6;
	}
	else
	{
		dlog( kDebugLevelError, DEBUG_NAME "%s: dst is not an IPv4 or IPv6 address (type=%d)\n", __ROUTINE__, inDstIP->type );
		err = mStatus_BadParamErr;
		goto exit;
	}
	
	if (IsValidSocket(sendingsocket))
	{
		n = sendto( sendingsocket, (char *) inMsg, n, 0, (struct sockaddr *) &addr, sizeof( addr ) );
		err = translate_errno( n > 0, errno_compat(), kWriteErr );

		if ( err )
		{
			// Don't report EHOSTDOWN (i.e. ARP failure), ENETDOWN, or no route to host for unicast destinations

			if ( !mDNSAddressIsAllDNSLinkGroup( inDstIP ) && ( WSAGetLastError() == WSAEHOSTDOWN || WSAGetLastError() == WSAENETDOWN || WSAGetLastError() == WSAEHOSTUNREACH || WSAGetLastError() == WSAENETUNREACH ) )
			{
				err = mStatus_TransientErr;
			}
			else
			{
				require_noerr( err, exit );
			}
		}
	}
	
exit:
	return( err );
}


mDNSexport void mDNSPlatformUpdateProxyList(mDNS *const m, const mDNSInterfaceID InterfaceID)
	{
	DEBUG_UNUSED( m );
	DEBUG_UNUSED( InterfaceID );
	}

//===========================================================================================================================
//	mDNSPlatformSendRawPacket
//===========================================================================================================================
	
mDNSexport void mDNSPlatformSendRawPacket(const void *const msg, const mDNSu8 *const end, mDNSInterfaceID InterfaceID)
	{
	DEBUG_UNUSED( msg );
	DEBUG_UNUSED( end );
	DEBUG_UNUSED( InterfaceID );
	}

mDNSexport void mDNSPlatformReceiveRawPacket(const void *const msg, const mDNSu8 *const end, mDNSInterfaceID InterfaceID)
	{
	DEBUG_UNUSED( msg );
	DEBUG_UNUSED( end );
	DEBUG_UNUSED( InterfaceID );
	}

mDNSexport void mDNSPlatformSetLocalARP( const mDNSv4Addr * const tpa, const mDNSEthAddr * const tha, mDNSInterfaceID InterfaceID )
	{
	DEBUG_UNUSED( tpa );
	DEBUG_UNUSED( tha );
	DEBUG_UNUSED( InterfaceID );
	}

mDNSexport void mDNSPlatformWriteDebugMsg(const char *msg)
	{
	fprintf( stderr, msg );
	}

mDNSexport void mDNSPlatformWriteLogMsg( const char * ident, const char * msg, int flags )
	{
	DEBUG_UNUSED( ident );
	DEBUG_UNUSED( msg );
	DEBUG_UNUSED( flags );
	}

mDNSexport void mDNSPlatformSourceAddrForDest( mDNSAddr * const src, const mDNSAddr * const dst )
	{
	DEBUG_UNUSED( src );
	DEBUG_UNUSED( dst );
	}

//===========================================================================================================================
//	mDNSPlatformTLSSetupCerts
//===========================================================================================================================

mDNSexport mStatus
mDNSPlatformTLSSetupCerts(void)
{
	return mStatus_UnsupportedErr;
}

//===========================================================================================================================
//	mDNSPlatformTLSTearDownCerts
//===========================================================================================================================

mDNSexport void
mDNSPlatformTLSTearDownCerts(void)
{
}

//===========================================================================================================================
//	mDNSPlatformSetDNSConfig
//===========================================================================================================================

mDNSlocal void SetDNSServers( mDNS *const m );
mDNSlocal void SetSearchDomainList( void );

mDNSexport void mDNSPlatformSetDNSConfig(mDNS *const m, mDNSBool setservers, mDNSBool setsearch, domainname *const fqdn, DNameListElem **regDomains, DNameListElem **browseDomains)
{
	if (setservers) SetDNSServers(m);
	if (setsearch) SetSearchDomainList();
	
	if ( fqdn )
	{
		GetDDNSFQDN( fqdn );
	}

	if ( browseDomains )
	{
		GetDDNSDomains( browseDomains, kServiceParametersNode TEXT("\\DynDNS\\Setup\\") kServiceDynDNSBrowseDomains );
	}

	if ( regDomains )
	{
		GetDDNSDomains( regDomains, kServiceParametersNode TEXT("\\DynDNS\\Setup\\") kServiceDynDNSRegistrationDomains );
	}
}


//===========================================================================================================================
//	mDNSPlatformDynDNSHostNameStatusChanged
//===========================================================================================================================

mDNSexport void
mDNSPlatformDynDNSHostNameStatusChanged(const domainname *const dname, const mStatus status)
{
	char		uname[MAX_ESCAPED_DOMAIN_NAME];
	BYTE		bStatus;
	LPCTSTR		name;
	HKEY		key = NULL;
	mStatus		err;
	char	*	p;
	
	ConvertDomainNameToCString(dname, uname);
	
	p = uname;

	while (*p)
	{
		*p = (char) tolower(*p);
		if (!(*(p+1)) && *p == '.') *p = 0; // if last character, strip trailing dot
		p++;
	}

	check( strlen( p ) <= MAX_ESCAPED_DOMAIN_NAME );
	name = kServiceParametersNode TEXT("\\DynDNS\\State\\HostNames");
	err = RegCreateKey( HKEY_LOCAL_MACHINE, name, &key );
	require_noerr( err, exit );

	bStatus = ( status ) ? 0 : 1;
	err = RegSetValueEx( key, kServiceDynDNSStatus, 0, REG_DWORD, (const LPBYTE) &bStatus, sizeof(DWORD) );
	require_noerr( err, exit );

exit:

	if ( key )
	{
		RegCloseKey( key );
	}

	return;
}


//===========================================================================================================================
//	SetDomainSecrets
//===========================================================================================================================

// This routine needs to be called whenever the system secrets database changes.
// We call it from ProcessingThreadDynDNSConfigChanged and mDNSPlatformInit

mDNSlocal void
SetDomainSecrets( mDNS * const m )
{
	DomainAuthInfo *ptr;
	domainname		fqdn;
	DNameListElem * regDomains = NULL;

	// Rather than immediately deleting all keys now, we mark them for deletion in ten seconds.
	// In the case where the user simultaneously removes their DDNS host name and the key
	// for it, this gives mDNSResponder ten seconds to gracefully delete the name from the
	// server before it loses access to the necessary key. Otherwise, we'd leave orphaned
	// address records behind that we no longer have permission to delete.
	
	for (ptr = m->AuthInfoList; ptr; ptr = ptr->next)
		ptr->deltime = NonZeroTime(m->timenow + mDNSPlatformOneSecond*10);

	GetDDNSFQDN( &fqdn );

	if ( fqdn.c[ 0 ] )
	{
		SetDomainSecret( m, &fqdn );
	}

	GetDDNSDomains( &regDomains, kServiceParametersNode TEXT("\\DynDNS\\Setup\\") kServiceDynDNSRegistrationDomains );

	while ( regDomains )
	{
		DNameListElem * current = regDomains;
		SetDomainSecret( m, &current->name );
		regDomains = regDomains->next;
		free( current );
	}
}


//===========================================================================================================================
//	SetSearchDomainList
//===========================================================================================================================

mDNSlocal void SetDomainFromDHCP( void );
mDNSlocal void SetReverseMapSearchDomainList( void );

mDNSlocal void
SetSearchDomainList( void )
{
	char			*	searchList	= NULL;
	DWORD				searchListLen;
	//DNameListElem	*	head = NULL;
	//DNameListElem	*	current = NULL;
	char			*	tok;
	HKEY				key;
	mStatus				err;

	err = RegCreateKey( HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"), &key );
	require_noerr( err, exit );

	err = RegQueryString( key, "SearchList", &searchList, &searchListLen, NULL );
	require_noerr( err, exit );

	// Windows separates the search domains with ','

	tok = strtok( searchList, "," );
	while ( tok )
	{
		if ( ( strcmp( tok, "" ) != 0 ) && ( strcmp( tok, "." ) != 0 ) )
			mDNS_AddSearchDomain_CString(tok);
		tok = strtok( NULL, "," );
	}

exit:

	if ( searchList ) 
	{
		free( searchList );
	}

	if ( key )
	{
		RegCloseKey( key );
	}

	SetDomainFromDHCP();
	SetReverseMapSearchDomainList();
}


//===========================================================================================================================
//	SetReverseMapSearchDomainList
//===========================================================================================================================

mDNSlocal void
SetReverseMapSearchDomainList( void )
{
	struct ifaddrs	*	ifa;

	ifa = myGetIfAddrs( 1 );
	while (ifa)
	{
		mDNSAddr addr;
		
		if (ifa->ifa_addr->sa_family == AF_INET && !SetupAddr(&addr, ifa->ifa_addr) && !(ifa->ifa_flags & IFF_LOOPBACK) && ifa->ifa_netmask)
		{
			mDNSAddr	netmask;
			char		buffer[256];
			
			if (!SetupAddr(&netmask, ifa->ifa_netmask))
			{
				sprintf(buffer, "%d.%d.%d.%d.in-addr.arpa.", addr.ip.v4.b[3] & netmask.ip.v4.b[3],
                                                             addr.ip.v4.b[2] & netmask.ip.v4.b[2],
                                                             addr.ip.v4.b[1] & netmask.ip.v4.b[1],
                                                             addr.ip.v4.b[0] & netmask.ip.v4.b[0]);
				mDNS_AddSearchDomain_CString(buffer);
			}
		}
	
		ifa = ifa->ifa_next;
	}

	return;
}


//===========================================================================================================================
//	SetDNSServers
//===========================================================================================================================

mDNSlocal void
SetDNSServers( mDNS *const m )
{
	PIP_PER_ADAPTER_INFO	pAdapterInfo	=	NULL;
	FIXED_INFO			*	fixedInfo	= NULL;
	ULONG					bufLen		= 0;	
	IP_ADDR_STRING		*	dnsServerList;
	IP_ADDR_STRING		*	ipAddr;
	DWORD					index;
	int						i			= 0;
	mStatus					err			= kUnknownErr;

	// Get the primary interface.

	index = GetPrimaryInterface();

	// This should have the interface index of the primary index.  Fall back in cases where
	// it can't be determined.

	if ( index )
	{
		bufLen = 0;

		for ( i = 0; i < 100; i++ )
		{
			err = GetPerAdapterInfo( index, pAdapterInfo, &bufLen );

			if ( err != ERROR_BUFFER_OVERFLOW )
			{
				break;
			}

			pAdapterInfo = (PIP_PER_ADAPTER_INFO) realloc( pAdapterInfo, bufLen );
			require_action( pAdapterInfo, exit, err = mStatus_NoMemoryErr );
		}

		require_noerr( err, exit );

		dnsServerList = &pAdapterInfo->DnsServerList;
	}
	else
	{
		bufLen = sizeof( FIXED_INFO );

		for ( i = 0; i < 100; i++ )
		{
			if ( fixedInfo )
			{
				GlobalFree( fixedInfo );
				fixedInfo = NULL;
			}

			fixedInfo = (FIXED_INFO*) GlobalAlloc( GPTR, bufLen );
			require_action( fixedInfo, exit, err = mStatus_NoMemoryErr );
	   
			err = GetNetworkParams( fixedInfo, &bufLen );

			if ( err != ERROR_BUFFER_OVERFLOW )
			{
				break;
			}
		}

		require_noerr( err, exit );

		dnsServerList = &fixedInfo->DnsServerList;
	}

	for ( ipAddr = dnsServerList; ipAddr; ipAddr = ipAddr->Next )
	{
		mDNSAddr addr;
		err = StringToAddress( &addr, ipAddr->IpAddress.String );
		if ( !err ) mDNS_AddDNSServer(m, mDNSNULL, mDNSInterface_Any, &addr, UnicastDNSPort);
	}

exit:

	if ( pAdapterInfo )
	{
		free( pAdapterInfo );
	}

	if ( fixedInfo )
	{
		GlobalFree( fixedInfo );
	}
}


//===========================================================================================================================
//	SetDomainFromDHCP
//===========================================================================================================================

mDNSlocal void
SetDomainFromDHCP( void )
{
	int					i			= 0;
	IP_ADAPTER_INFO *	pAdapterInfo;
	IP_ADAPTER_INFO *	pAdapter;
	DWORD				bufLen;
	DWORD				index;
	HKEY				key = NULL;
	LPSTR				domain = NULL;
	DWORD				dwSize;
	mStatus				err = mStatus_NoError;

	pAdapterInfo	= NULL;
	
	for ( i = 0; i < 100; i++ )
	{
		err = GetAdaptersInfo( pAdapterInfo, &bufLen);

		if ( err != ERROR_BUFFER_OVERFLOW )
		{
			break;
		}

		pAdapterInfo = (IP_ADAPTER_INFO*) realloc( pAdapterInfo, bufLen );
		require_action( pAdapterInfo, exit, err = kNoMemoryErr );
	}

	require_noerr( err, exit );

	index = GetPrimaryInterface();

	for ( pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next )
	{
		if ( pAdapter->IpAddressList.IpAddress.String &&
		     pAdapter->IpAddressList.IpAddress.String[0] &&
		     pAdapter->GatewayList.IpAddress.String &&
		     pAdapter->GatewayList.IpAddress.String[0] &&
		     ( !index || ( pAdapter->Index == index ) ) )
		{
			// Found one that will work

			char keyName[1024];

			_snprintf( keyName, 1024, "%s%s", "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\", pAdapter->AdapterName );

			err = RegCreateKeyA( HKEY_LOCAL_MACHINE, keyName, &key );
			require_noerr( err, exit );

			err = RegQueryString( key, "Domain", &domain, &dwSize, NULL );
			check_noerr( err );

			if ( !domain || !domain[0] )
			{
				if ( domain )
				{
					free( domain );
					domain = NULL;
				}

				err = RegQueryString( key, "DhcpDomain", &domain, &dwSize, NULL );
				check_noerr( err );
			}

			if ( domain && domain[0] ) mDNS_AddSearchDomain_CString(domain);

			break;
		}
	}

exit:

	if ( pAdapterInfo )
	{
		free( pAdapterInfo );
	}

	if ( domain )
	{
		free( domain );
	}

	if ( key )
	{
		RegCloseKey( key );
	}
}


//===========================================================================================================================
//	mDNSPlatformGetPrimaryInterface
//===========================================================================================================================

mDNSexport mStatus
mDNSPlatformGetPrimaryInterface( mDNS * const m, mDNSAddr * v4, mDNSAddr * v6, mDNSAddr * router )
{
	IP_ADAPTER_INFO *	pAdapterInfo;
	IP_ADAPTER_INFO *	pAdapter;
	DWORD				bufLen;
	int					i;
	BOOL				found;
	DWORD				index;
	mStatus				err = mStatus_NoError;

	DEBUG_UNUSED( m );

	*v6 = zeroAddr;

	pAdapterInfo	= NULL;
	bufLen			= 0;
	found			= FALSE;

	for ( i = 0; i < 100; i++ )
	{
		err = GetAdaptersInfo( pAdapterInfo, &bufLen);

		if ( err != ERROR_BUFFER_OVERFLOW )
		{
			break;
		}

		pAdapterInfo = (IP_ADAPTER_INFO*) realloc( pAdapterInfo, bufLen );
		require_action( pAdapterInfo, exit, err = kNoMemoryErr );
	}

	require_noerr( err, exit );

	index = GetPrimaryInterface();

	for ( pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next )
	{
		if ( pAdapter->IpAddressList.IpAddress.String &&
		     pAdapter->IpAddressList.IpAddress.String[0] &&
		     pAdapter->GatewayList.IpAddress.String &&
		     pAdapter->GatewayList.IpAddress.String[0] &&
		     ( StringToAddress( v4, pAdapter->IpAddressList.IpAddress.String ) == mStatus_NoError ) &&
		     ( StringToAddress( router, pAdapter->GatewayList.IpAddress.String ) == mStatus_NoError ) &&
		     ( !index || ( pAdapter->Index == index ) ) )
		{
			// Found one that will work

			found = TRUE;
			break;
		}
	}

exit:

	if ( pAdapterInfo )
	{
		free( pAdapterInfo );
	}

	return err;
}


#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	debugf_
//===========================================================================================================================

#if( MDNS_DEBUGMSGS )
mDNSexport void	debugf_( const char *inFormat, ... )
{
	char		buffer[ 512 ];
    va_list		args;
    mDNSu32		length;
	
	va_start( args, inFormat );
	length = mDNS_vsnprintf( buffer, sizeof( buffer ), inFormat, args );
	va_end( args );
	
	dlog( kDebugLevelInfo, "%s\n", buffer );
}
#endif

//===========================================================================================================================
//	verbosedebugf_
//===========================================================================================================================

#if( MDNS_DEBUGMSGS > 1 )
mDNSexport void	verbosedebugf_( const char *inFormat, ... )
{
	char		buffer[ 512 ];
    va_list		args;
    mDNSu32		length;
	
	va_start( args, inFormat );
	length = mDNS_vsnprintf( buffer, sizeof( buffer ), inFormat, args );
	va_end( args );
	
	dlog( kDebugLevelVerbose, "%s\n", buffer );
}
#endif

//===========================================================================================================================
//	LogMsg
//===========================================================================================================================

/*
mDNSexport void	LogMsg( const char *inFormat, ... )
{
	char		buffer[ 512 ];
    va_list		args;
    mDNSu32		length;
	
	va_start( args, inFormat );
	length = mDNS_vsnprintf( buffer, sizeof( buffer ), inFormat, args );
	va_end( args );
	
	dlog( kDebugLevelWarning, "%s\n", buffer );
}
*/

#if 0
#pragma mark -
#pragma mark == Platform Internals  ==
#endif

//===========================================================================================================================
//	SetupSynchronizationObjects
//===========================================================================================================================

mDNSlocal mStatus	SetupSynchronizationObjects( mDNS * const inMDNS )
{
	mStatus		err;
		
	InitializeCriticalSection( &inMDNS->p->lock );
	inMDNS->p->lockInitialized = mDNStrue;
	
	inMDNS->p->cancelEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	err = translate_errno( inMDNS->p->cancelEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );
	
	inMDNS->p->quitEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	err = translate_errno( inMDNS->p->quitEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );
	
	inMDNS->p->interfaceListChangedEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	err = translate_errno( inMDNS->p->interfaceListChangedEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );
	
	inMDNS->p->wakeupEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	err = translate_errno( inMDNS->p->wakeupEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );
	
exit:
	if( err )
	{
		TearDownSynchronizationObjects( inMDNS );
	}
	return( err );
}

//===========================================================================================================================
//	TearDownSynchronizationObjects
//===========================================================================================================================

mDNSlocal mStatus	TearDownSynchronizationObjects( mDNS * const inMDNS )
{
	if( inMDNS->p->quitEvent )
	{
		CloseHandle( inMDNS->p->quitEvent );
		inMDNS->p->quitEvent = 0;
	}
	if( inMDNS->p->cancelEvent )
	{
		CloseHandle( inMDNS->p->cancelEvent );
		inMDNS->p->cancelEvent = 0;
	}
	if( inMDNS->p->interfaceListChangedEvent )
	{
		CloseHandle( inMDNS->p->interfaceListChangedEvent );
		inMDNS->p->interfaceListChangedEvent = 0;
	}
	if( inMDNS->p->wakeupEvent )
	{
		CloseHandle( inMDNS->p->wakeupEvent );
		inMDNS->p->wakeupEvent = 0;
	}
	if( inMDNS->p->lockInitialized )
	{
		DeleteCriticalSection( &inMDNS->p->lock );
		inMDNS->p->lockInitialized = mDNSfalse;
	}
	return( mStatus_NoError );
}


//===========================================================================================================================
//	SetupNiceName
//===========================================================================================================================

mDNSlocal mStatus	SetupNiceName( mDNS * const inMDNS )
{
	mStatus		err = 0;
	char		utf8[ 256 ];
	
	check( inMDNS );
	
	// Set up the nice name.
	utf8[0] = '\0';

	// First try and open the registry key that contains the computer description value
	if (inMDNS->p->descKey == NULL)
	{
		LPCTSTR s = TEXT("SYSTEM\\CurrentControlSet\\Services\\lanmanserver\\parameters");
		err = RegOpenKeyEx( HKEY_LOCAL_MACHINE, s, 0, KEY_READ, &inMDNS->p->descKey);
		check_translated_errno( err == 0, errno_compat(), kNameErr );

		if (err)
		{
			inMDNS->p->descKey = NULL;
		}
	}

	// if we opened it...
	if (inMDNS->p->descKey != NULL)
	{
		TCHAR	desc[256];
		DWORD	descSize = sizeof( desc );

		// look for the computer description
		err = RegQueryValueEx(inMDNS->p->descKey, TEXT("srvcomment"), 0, NULL, (LPBYTE) &desc, &descSize);
		
		if ( !err )
		{
			err = TCHARtoUTF8( desc, utf8, sizeof( utf8 ) );
		}

		if ( err )
		{
			utf8[ 0 ] = '\0';
		}
	}

	// if we can't find it in the registry, then use the hostname of the machine
	if ( err || ( utf8[ 0 ] == '\0' ) )
	{
		TCHAR hostname[256];
		DWORD hostnameLen = sizeof( hostname ) / sizeof( TCHAR );
		BOOL  ok;

		ok = GetComputerNameExW( ComputerNamePhysicalDnsHostname, hostname, &hostnameLen );
		err = translate_errno( ok, (mStatus) GetLastError(), kNameErr );
		check_noerr( err );
		
		if( !err )
		{
			err = TCHARtoUTF8( hostname, utf8, sizeof( utf8 ) );
		}

		if ( err )
		{
			utf8[ 0 ] = '\0';
		}
	}

	// if we can't get the hostname
	if ( err || ( utf8[ 0 ] == '\0' ) )
	{
		// Invalidate name so fall back to a default name.
		
		strcpy( utf8, kMDNSDefaultName );
	}

	utf8[ sizeof( utf8 ) - 1 ]	= '\0';	
	inMDNS->nicelabel.c[ 0 ]	= (mDNSu8) (strlen( utf8 ) < MAX_DOMAIN_LABEL ? strlen( utf8 ) : MAX_DOMAIN_LABEL);
	memcpy( &inMDNS->nicelabel.c[ 1 ], utf8, inMDNS->nicelabel.c[ 0 ] );
	
	dlog( kDebugLevelInfo, DEBUG_NAME "nice name \"%.*s\"\n", inMDNS->nicelabel.c[ 0 ], &inMDNS->nicelabel.c[ 1 ] );
	
	return( err );
}

//===========================================================================================================================
//	SetupHostName
//===========================================================================================================================

mDNSlocal mStatus	SetupHostName( mDNS * const inMDNS )
{
	mStatus		err = 0;
	char		tempString[ 256 ];
	DWORD		tempStringLen;
	domainlabel tempLabel;
	BOOL		ok;
	
	check( inMDNS );

	// Set up the nice name.
	tempString[ 0 ] = '\0';

	// use the hostname of the machine
	tempStringLen = sizeof( tempString );
	ok = GetComputerNameExA( ComputerNamePhysicalDnsHostname, tempString, &tempStringLen );
	err = translate_errno( ok, (mStatus) GetLastError(), kNameErr );
	check_noerr( err );

	// if we can't get the hostname
	if( err || ( tempString[ 0 ] == '\0' ) )
	{
		// Invalidate name so fall back to a default name.
		
		strcpy( tempString, kMDNSDefaultName );
	}

	tempString[ sizeof( tempString ) - 1 ] = '\0';
	tempLabel.c[ 0 ] = (mDNSu8) (strlen( tempString ) < MAX_DOMAIN_LABEL ? strlen( tempString ) : MAX_DOMAIN_LABEL );
	memcpy( &tempLabel.c[ 1 ], tempString, tempLabel.c[ 0 ] );
	
	// Set up the host name.
	
	ConvertUTF8PstringToRFC1034HostLabel( tempLabel.c, &inMDNS->hostlabel );
	if( inMDNS->hostlabel.c[ 0 ] == 0 )
	{
		// Nice name has no characters that are representable as an RFC1034 name (e.g. Japanese) so use the default.
		
		MakeDomainLabelFromLiteralString( &inMDNS->hostlabel, kMDNSDefaultName );
	}

	check( inMDNS->hostlabel.c[ 0 ] != 0 );
	
	mDNS_SetFQDN( inMDNS );
	
	dlog( kDebugLevelInfo, DEBUG_NAME "host name \"%.*s\"\n", inMDNS->hostlabel.c[ 0 ], &inMDNS->hostlabel.c[ 1 ] );
	
	return( err );
}

//===========================================================================================================================
//	SetupName
//===========================================================================================================================

mDNSlocal mStatus	SetupName( mDNS * const inMDNS )
{
	mStatus		err = 0;
	
	check( inMDNS );
	
	err = SetupNiceName( inMDNS );
	check_noerr( err );

	err = SetupHostName( inMDNS );
	check_noerr( err );

	return err;
}


//===========================================================================================================================
//	SetupInterfaceList
//===========================================================================================================================

mDNSlocal mStatus	SetupInterfaceList( mDNS * const inMDNS )
{
	mStatus						err;
	mDNSInterfaceData **		next;
	mDNSInterfaceData *			ifd;
	struct ifaddrs *			addrs;
	struct ifaddrs *			p;
	struct ifaddrs *			loopbackv4;
	struct ifaddrs *			loopbackv6;
	u_int						flagMask;
	u_int						flagTest;
	mDNSBool					foundv4;
	mDNSBool					foundv6;
	mDNSBool					foundUnicastSock4DestAddr;
	mDNSBool					foundUnicastSock6DestAddr;
	
	dlog( kDebugLevelTrace, DEBUG_NAME "setting up interface list\n" );
	check( inMDNS );
	check( inMDNS->p );
	
	inMDNS->p->registeredLoopback4	= mDNSfalse;
	inMDNS->p->nextDHCPLeaseExpires = 0xFFFFFFFF;
	inMDNS->p->womp					= mDNSfalse;
	addrs							= NULL;
	foundv4							= mDNSfalse;
	foundv6							= mDNSfalse;
	foundUnicastSock4DestAddr		= mDNSfalse;
	foundUnicastSock6DestAddr		= mDNSfalse;
	
	// Tear down any existing interfaces that may be set up.
	
	TearDownInterfaceList( inMDNS );

	// Set up the name of this machine.
	
	err = SetupName( inMDNS );
	check_noerr( err );
	
	// Set up the interface list change notification.
	
	err = SetupNotifications( inMDNS );
	check_noerr( err );
	
	// Set up IPv4 interface(s). We have to set up IPv4 first so any IPv6 interface with an IPv4-routable address
	// can refer to the IPv4 interface when it registers to allow DNS AAAA records over the IPv4 interface.
	
	err = getifaddrs( &addrs );
	require_noerr( err, exit );
	
	loopbackv4	= NULL;
	loopbackv6	= NULL;
	next		= &inMDNS->p->interfaceList;

	flagMask = IFF_UP | IFF_MULTICAST;
	flagTest = IFF_UP | IFF_MULTICAST;
	
#if( MDNS_WINDOWS_ENABLE_IPV4 )
	for( p = addrs; p; p = p->ifa_next )
	{
		if( !p->ifa_addr || ( p->ifa_addr->sa_family != AF_INET ) || ( ( p->ifa_flags & flagMask ) != flagTest ) )
		{
			continue;
		}
		if( p->ifa_flags & IFF_LOOPBACK )
		{
			if( !loopbackv4 )
			{
				loopbackv4 = p;
			}
			continue;
		}
		dlog( kDebugLevelVerbose, DEBUG_NAME "Interface %40s (0x%08X) %##a\n", 
			p->ifa_name ? p->ifa_name : "<null>", p->ifa_extra.index, p->ifa_addr );
		
		err = SetupInterface( inMDNS, p, &ifd );
		require_noerr( err, exit );

		// If this guy is point-to-point (ifd->interfaceInfo.McastTxRx == 0 ) we still want to
		// register him, but we also want to note that we haven't found a v4 interface
		// so that we register loopback so same host operations work
 		
		if ( ifd->interfaceInfo.McastTxRx == mDNStrue )
		{
			foundv4 = mDNStrue;
		}

		if ( p->ifa_dhcpEnabled && ( p->ifa_dhcpLeaseExpires < inMDNS->p->nextDHCPLeaseExpires ) )
		{
			inMDNS->p->nextDHCPLeaseExpires = p->ifa_dhcpLeaseExpires;
		}

		// If we're on a platform that doesn't have WSARecvMsg(), there's no way
		// of determing the destination address of a packet that is sent to us.
		// For multicast packets, that's easy to determine.  But for the unicast
		// sockets, we'll fake it by taking the address of the first interface
		// that is successfully setup.

		if ( !foundUnicastSock4DestAddr )
		{
			inMDNS->p->unicastSock4DestAddr = ifd->interfaceInfo.ip;
			foundUnicastSock4DestAddr = TRUE;
		}
			
		*next = ifd;
		next  = &ifd->next;
		++inMDNS->p->interfaceCount;
	}
#endif
	
	// Set up IPv6 interface(s) after IPv4 is set up (see IPv4 notes above for reasoning).
	
#if( MDNS_WINDOWS_ENABLE_IPV6 )
	for( p = addrs; p; p = p->ifa_next )
	{
		if( !p->ifa_addr || ( p->ifa_addr->sa_family != AF_INET6 ) || ( ( p->ifa_flags & flagMask ) != flagTest ) )
		{
			continue;
		}
		if( p->ifa_flags & IFF_LOOPBACK )
		{
			if( !loopbackv6 )
			{
				loopbackv6 = p;
			}
			continue;
		}
		dlog( kDebugLevelVerbose, DEBUG_NAME "Interface %40s (0x%08X) %##a\n", 
			p->ifa_name ? p->ifa_name : "<null>", p->ifa_extra.index, p->ifa_addr );
		
		err = SetupInterface( inMDNS, p, &ifd );
		require_noerr( err, exit );
				
		// If this guy is point-to-point (ifd->interfaceInfo.McastTxRx == 0 ) we still want to
		// register him, but we also want to note that we haven't found a v4 interface
		// so that we register loopback so same host operations work
 		
		if ( ifd->interfaceInfo.McastTxRx == mDNStrue )
		{
			foundv6 = mDNStrue;
		}

		// If we're on a platform that doesn't have WSARecvMsg(), there's no way
		// of determing the destination address of a packet that is sent to us.
		// For multicast packets, that's easy to determine.  But for the unicast
		// sockets, we'll fake it by taking the address of the first interface
		// that is successfully setup.

		if ( !foundUnicastSock6DestAddr )
		{
			inMDNS->p->unicastSock6DestAddr = ifd->interfaceInfo.ip;
			foundUnicastSock6DestAddr = TRUE;
		}

		*next = ifd;
		next  = &ifd->next;
		++inMDNS->p->interfaceCount;
	}
#endif

	// If there are no real interfaces, but there is a loopback interface, use that so same-machine operations work.

#if( !MDNS_WINDOWS_ENABLE_IPV4 && !MDNS_WINDOWS_ENABLE_IPV6 )
	
	flagMask |= IFF_LOOPBACK;
	flagTest |= IFF_LOOPBACK;
	
	for( p = addrs; p; p = p->ifa_next )
	{
		if( !p->ifa_addr || ( ( p->ifa_flags & flagMask ) != flagTest ) )
		{
			continue;
		}
		if( ( p->ifa_addr->sa_family != AF_INET ) && ( p->ifa_addr->sa_family != AF_INET6 ) )
		{
			continue;
		}
		
		v4loopback = p;
		break;
	}
	
#endif
	
	if ( !foundv4 && loopbackv4 )
	{
		dlog( kDebugLevelInfo, DEBUG_NAME "Interface %40s (0x%08X) %##a\n", 
			loopbackv4->ifa_name ? loopbackv4->ifa_name : "<null>", loopbackv4->ifa_extra.index, loopbackv4->ifa_addr );
		
		err = SetupInterface( inMDNS, loopbackv4, &ifd );
		require_noerr( err, exit );

		inMDNS->p->registeredLoopback4 = mDNStrue;
		
#if( MDNS_WINDOWS_ENABLE_IPV4 )

		// If we're on a platform that doesn't have WSARecvMsg(), there's no way
		// of determing the destination address of a packet that is sent to us.
		// For multicast packets, that's easy to determine.  But for the unicast
		// sockets, we'll fake it by taking the address of the first interface
		// that is successfully setup.

		if ( !foundUnicastSock4DestAddr )
		{
			inMDNS->p->unicastSock4DestAddr = ifd->defaultAddr;
			foundUnicastSock4DestAddr = TRUE;
		}
#endif

		*next = ifd;
		next  = &ifd->next;
		++inMDNS->p->interfaceCount;
	}

	if ( !foundv6 && loopbackv6 )
	{
		dlog( kDebugLevelInfo, DEBUG_NAME "Interface %40s (0x%08X) %##a\n", 
			loopbackv6->ifa_name ? loopbackv6->ifa_name : "<null>", loopbackv6->ifa_extra.index, loopbackv6->ifa_addr );
		
		err = SetupInterface( inMDNS, loopbackv6, &ifd );
		require_noerr( err, exit );
		
#if( MDNS_WINDOWS_ENABLE_IPV6 )

		// If we're on a platform that doesn't have WSARecvMsg(), there's no way
		// of determing the destination address of a packet that is sent to us.
		// For multicast packets, that's easy to determine.  But for the unicast
		// sockets, we'll fake it by taking the address of the first interface
		// that is successfully setup.

		if ( !foundUnicastSock6DestAddr )
		{
			inMDNS->p->unicastSock6DestAddr = ifd->defaultAddr;
			foundUnicastSock6DestAddr = TRUE;
		}
#endif

		*next = ifd;
		next  = &ifd->next;
		++inMDNS->p->interfaceCount;
	}

exit:
	if( err )
	{
		TearDownInterfaceList( inMDNS );
	}
	if( addrs )
	{
		freeifaddrs( addrs );
	}
	dlog( kDebugLevelTrace, DEBUG_NAME "setting up interface list done (err=%d %m)\n", err, err );
	return( err );
}

//===========================================================================================================================
//	TearDownInterfaceList
//===========================================================================================================================

mDNSlocal mStatus	TearDownInterfaceList( mDNS * const inMDNS )
{
	mStatus					err;
	mDNSInterfaceData **		p;
	mDNSInterfaceData *		ifd;
	
	dlog( kDebugLevelTrace, DEBUG_NAME "tearing down interface list\n" );
	check( inMDNS );
	check( inMDNS->p );
	
	// Free any interfaces that were previously marked inactive and are no longer referenced by the mDNS cache.
	// Interfaces are marked inactive, but not deleted immediately if they were still referenced by the mDNS cache
	// so that remove events that occur after an interface goes away can still report the correct interface.

	p = &inMDNS->p->inactiveInterfaceList;
	while( *p )
	{
		ifd = *p;
		if( NumCacheRecordsForInterfaceID( inMDNS, (mDNSInterfaceID) ifd ) > 0 )
		{
			p = &ifd->next;
			continue;
		}
		
		dlog( kDebugLevelInfo, DEBUG_NAME "freeing unreferenced, inactive interface %#p %#a\n", ifd, &ifd->interfaceInfo.ip );
		*p = ifd->next;
		free( ifd );
	}
	
	// Tear down interface list change notifications.
	
	err = TearDownNotifications( inMDNS );
	check_noerr( err );
	
	// Tear down all the interfaces.
	
	while( inMDNS->p->interfaceList )
	{
		ifd = inMDNS->p->interfaceList;
		inMDNS->p->interfaceList = ifd->next;
		
		TearDownInterface( inMDNS, ifd );
	}
	inMDNS->p->interfaceCount = 0;
	
	dlog( kDebugLevelTrace, DEBUG_NAME "tearing down interface list done\n" );
	return( mStatus_NoError );
}

//===========================================================================================================================
//	SetupInterface
//===========================================================================================================================

mDNSlocal mStatus	SetupInterface( mDNS * const inMDNS, const struct ifaddrs *inIFA, mDNSInterfaceData **outIFD )
{
	mDNSInterfaceData	*	ifd;
	mDNSInterfaceData	*	p;
	SocketRef				sock;
	mStatus					err;
	
	ifd = NULL;
	dlog( kDebugLevelTrace, DEBUG_NAME "setting up interface\n" );
	check( inMDNS );
	check( inMDNS->p );
	check( inIFA );
	check( inIFA->ifa_addr );
	check( outIFD );
	
	// Allocate memory for the interface and initialize it.
	
	ifd = (mDNSInterfaceData *) calloc( 1, sizeof( *ifd ) );
	require_action( ifd, exit, err = mStatus_NoMemoryErr );
	ifd->sock		= kInvalidSocketRef;
	ifd->index		= inIFA->ifa_extra.index;
	ifd->scopeID	= inIFA->ifa_extra.index;
	check( strlen( inIFA->ifa_name ) < sizeof( ifd->name ) );
	strncpy( ifd->name, inIFA->ifa_name, sizeof( ifd->name ) - 1 );
	ifd->name[ sizeof( ifd->name ) - 1 ] = '\0';
	
	strncpy(ifd->interfaceInfo.ifname, inIFA->ifa_name, sizeof(ifd->interfaceInfo.ifname));
	ifd->interfaceInfo.ifname[sizeof(ifd->interfaceInfo.ifname)-1] = 0;
	
	// We always send and receive using IPv4, but to reduce traffic, we send and receive using IPv6 only on interfaces 
	// that have no routable IPv4 address. Having a routable IPv4 address assigned is a reasonable indicator of being 
	// on a large configured network, which means there's a good chance that most or all the other devices on that 
	// network should also have v4. By doing this we lose the ability to talk to true v6-only devices on that link, 
	// but we cut the packet rate in half. At this time, reducing the packet rate is more important than v6-only 
	// devices on a large configured network, so we are willing to make that sacrifice.
	
	ifd->interfaceInfo.McastTxRx   = ( ( inIFA->ifa_flags & IFF_MULTICAST ) && !( inIFA->ifa_flags & IFF_POINTTOPOINT ) ) ? mDNStrue : mDNSfalse;
	ifd->interfaceInfo.InterfaceID = NULL;

	for( p = inMDNS->p->interfaceList; p; p = p->next )
	{
		if ( strcmp( p->name, ifd->name ) == 0 )
		{
			if (!ifd->interfaceInfo.InterfaceID)
			{
				ifd->interfaceInfo.InterfaceID	= (mDNSInterfaceID) p;
			}

			if ( ( inIFA->ifa_addr->sa_family != AF_INET ) &&
			     ( p->interfaceInfo.ip.type == mDNSAddrType_IPv4 ) &&
			     ( p->interfaceInfo.ip.ip.v4.b[ 0 ] != 169 || p->interfaceInfo.ip.ip.v4.b[ 1 ] != 254 ) )
			{
				ifd->interfaceInfo.McastTxRx = mDNSfalse;
			}

			break;
		}
	}

	if ( !ifd->interfaceInfo.InterfaceID )
	{
		ifd->interfaceInfo.InterfaceID = (mDNSInterfaceID) ifd;
	}

	// Set up a socket for this interface (if needed).
	
	if( ifd->interfaceInfo.McastTxRx )
	{
		err = SetupSocket( inMDNS, inIFA->ifa_addr, MulticastDNSPort, &sock );
		require_noerr( err, exit );
		ifd->sock = sock;
		ifd->defaultAddr = ( inIFA->ifa_addr->sa_family == AF_INET6 ) ? AllDNSLinkGroup_v6 : AllDNSLinkGroup_v4;
		
		// Get a ptr to the WSARecvMsg function, if supported. Otherwise, we'll fallback to recvfrom.

		#if( !TARGET_OS_WINDOWS_CE )
		{
			DWORD size;
			
			err = WSAIoctl( sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &kWSARecvMsgGUID, sizeof( kWSARecvMsgGUID ),
					&ifd->wsaRecvMsgFunctionPtr, sizeof( ifd->wsaRecvMsgFunctionPtr ), &size, NULL, NULL );

			if ( err )
			{
				ifd->wsaRecvMsgFunctionPtr = NULL;
			}
		}
		#endif

		// Set up the read pending event and associate it so we can block until data is available for this socket.
		
		ifd->readPendingEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		err = translate_errno( ifd->readPendingEvent, (mStatus) GetLastError(), kUnknownErr );
		require_noerr( err, exit );
		
		err = WSAEventSelect( ifd->sock, ifd->readPendingEvent, FD_READ );
		require_noerr( err, exit );
	}
	else
	{
		// Create a placeholder event so WaitForMultipleObjects Handle slot for this interface is valid.
		
		ifd->readPendingEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		err = translate_errno( ifd->readPendingEvent, (mStatus) GetLastError(), kUnknownErr );
		require_noerr( err, exit );
	}

	if ( inIFA->ifa_dhcpEnabled && ( inIFA->ifa_dhcpLeaseExpires < inMDNS->p->nextDHCPLeaseExpires ) )
	{
		inMDNS->p->nextDHCPLeaseExpires = inIFA->ifa_dhcpLeaseExpires;
	}

	ifd->interfaceInfo.NetWake = inIFA->ifa_womp;

	if ( ifd->interfaceInfo.NetWake )
	{
		inMDNS->p->womp = TRUE;
	}

	// Register this interface with mDNS.
	
	err = SockAddrToMDNSAddr( inIFA->ifa_addr, &ifd->interfaceInfo.ip, NULL );
	require_noerr( err, exit );
	
	err = SockAddrToMDNSAddr( inIFA->ifa_netmask, &ifd->interfaceInfo.mask, NULL );
	require_noerr( err, exit );
	
	ifd->interfaceInfo.Advertise = ( mDNSu8 ) inMDNS->AdvertiseLocalAddresses;
	
	err = mDNS_RegisterInterface( inMDNS, &ifd->interfaceInfo, mDNSfalse );
	require_noerr( err, exit );
	ifd->hostRegistered = mDNStrue;
	
	dlog( kDebugLevelInfo, DEBUG_NAME "Registered interface %##a with mDNS\n", inIFA->ifa_addr );
	
	// Success!
	
	*outIFD = ifd;
	ifd = NULL;
	
exit:

	if( ifd )
	{
		TearDownInterface( inMDNS, ifd );
	}
	dlog( kDebugLevelTrace, DEBUG_NAME "setting up interface done (err=%d %m)\n", err, err );
	return( err );
}

//===========================================================================================================================
//	TearDownInterface
//===========================================================================================================================

mDNSlocal mStatus	TearDownInterface( mDNS * const inMDNS, mDNSInterfaceData *inIFD )
{
	SocketRef		sock;
	
	check( inMDNS );
	check( inIFD );
	
	// Deregister this interface with mDNS.
	
	dlog( kDebugLevelInfo, DEBUG_NAME "Deregistering interface %#a with mDNS\n", &inIFD->interfaceInfo.ip );
	
	if( inIFD->hostRegistered )
	{
		inIFD->hostRegistered = mDNSfalse;
		mDNS_DeregisterInterface( inMDNS, &inIFD->interfaceInfo, mDNSfalse );
	}
	
	// Tear down the multicast socket.
	
	if( inIFD->readPendingEvent )
	{
		CloseHandle( inIFD->readPendingEvent );
		inIFD->readPendingEvent = 0;
	}
	
	sock = inIFD->sock;
	inIFD->sock = kInvalidSocketRef;
	if( IsValidSocket( sock ) )
	{
		close_compat( sock );
	}
	
	// If the interface is still referenced by items in the mDNS cache then put it on the inactive list. This keeps 
	// the InterfaceID valid so remove events report the correct interface. If it is no longer referenced, free it.

	if( NumCacheRecordsForInterfaceID( inMDNS, (mDNSInterfaceID) inIFD ) > 0 )
	{
		inIFD->next = inMDNS->p->inactiveInterfaceList;
		inMDNS->p->inactiveInterfaceList = inIFD;
		dlog( kDebugLevelInfo, DEBUG_NAME "deferring free of interface %#p %#a\n", inIFD, &inIFD->interfaceInfo.ip );
	}
	else
	{
		dlog( kDebugLevelInfo, DEBUG_NAME "freeing interface %#p %#a immediately\n", inIFD, &inIFD->interfaceInfo.ip );
		free( inIFD );
	}
	return( mStatus_NoError );
}

//===========================================================================================================================
//	SetupSocket
//===========================================================================================================================

mDNSlocal mStatus	SetupSocket( mDNS * const inMDNS, const struct sockaddr *inAddr, mDNSIPPort port, SocketRef *outSocketRef  )
{
	mStatus			err;
	SocketRef		sock;
	int				option;
	
	DEBUG_UNUSED( inMDNS );
	
	dlog( kDebugLevelTrace, DEBUG_NAME "setting up socket %##a\n", inAddr );
	check( inMDNS );
	check( outSocketRef );
	
	// Set up an IPv4 or IPv6 UDP socket.
	
	sock = socket( inAddr->sa_family, SOCK_DGRAM, IPPROTO_UDP );
	err = translate_errno( IsValidSocket( sock ), errno_compat(), kUnknownErr );
	require_noerr( err, exit );
		
	// Turn on reuse address option so multiple servers can listen for Multicast DNS packets,
	// if we're creating a multicast socket
	
	if ( port.NotAnInteger )
	{
		option = 1;
		err = setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, (char *) &option, sizeof( option ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
	}
	
	if( inAddr->sa_family == AF_INET )
	{
		mDNSv4Addr				ipv4;
		struct sockaddr_in		sa4;
		struct ip_mreq			mreqv4;
		
		// Bind the socket to the desired port
		
		ipv4.NotAnInteger 	= ( (const struct sockaddr_in *) inAddr )->sin_addr.s_addr;
		mDNSPlatformMemZero( &sa4, sizeof( sa4 ) );
		sa4.sin_family 		= AF_INET;
		sa4.sin_port 		= port.NotAnInteger;
		sa4.sin_addr.s_addr	= ipv4.NotAnInteger;
		
		err = bind( sock, (struct sockaddr *) &sa4, sizeof( sa4 ) );
		check_translated_errno( err == 0, errno_compat(), kUnknownErr );
		
		// Turn on option to receive destination addresses and receiving interface.
		
		option = 1;
		err = setsockopt( sock, IPPROTO_IP, IP_PKTINFO, (char *) &option, sizeof( option ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
		if (port.NotAnInteger)
		{
			// Join the all-DNS multicast group so we receive Multicast DNS packets

			mreqv4.imr_multiaddr.s_addr = AllDNSLinkGroup_v4.ip.v4.NotAnInteger;
			mreqv4.imr_interface.s_addr = ipv4.NotAnInteger;
			err = setsockopt( sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreqv4, sizeof( mreqv4 ) );
			check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
			// Specify the interface to send multicast packets on this socket.
		
			sa4.sin_addr.s_addr = ipv4.NotAnInteger;
			err = setsockopt( sock, IPPROTO_IP, IP_MULTICAST_IF, (char *) &sa4.sin_addr, sizeof( sa4.sin_addr ) );
			check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
			// Enable multicast loopback so we receive multicast packets we send (for same-machine operations).
		
			option = 1;
			err = setsockopt( sock, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &option, sizeof( option ) );
			check_translated_errno( err == 0, errno_compat(), kOptionErr );
		}

		// Send unicast packets with TTL 255 (helps against spoofing).
		
		option = 255;
		err = setsockopt( sock, IPPROTO_IP, IP_TTL, (char *) &option, sizeof( option ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );

		// Send multicast packets with TTL 255 (helps against spoofing).
		
		option = 255;
		err = setsockopt( sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &option, sizeof( option ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );

	}
	else if( inAddr->sa_family == AF_INET6 )
	{
		struct sockaddr_in6 *		sa6p;
		struct sockaddr_in6			sa6;
		struct ipv6_mreq			mreqv6;
		
		sa6p = (struct sockaddr_in6 *) inAddr;
		
		// Bind the socket to the desired port
		
		mDNSPlatformMemZero( &sa6, sizeof( sa6 ) );
		sa6.sin6_family		= AF_INET6;
		sa6.sin6_port		= port.NotAnInteger;
		sa6.sin6_flowinfo	= 0;
		sa6.sin6_addr		= sa6p->sin6_addr;
		sa6.sin6_scope_id	= sa6p->sin6_scope_id;
		
		err = bind( sock, (struct sockaddr *) &sa6, sizeof( sa6 ) );
		check_translated_errno( err == 0, errno_compat(), kUnknownErr );
		
		// Turn on option to receive destination addresses and receiving interface.
		
		option = 1;
		err = setsockopt( sock, IPPROTO_IPV6, IPV6_PKTINFO, (char *) &option, sizeof( option ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
		// We only want to receive IPv6 packets (not IPv4-mapped IPv6 addresses) because we have a separate socket 
		// for IPv4, but the IPv6 stack in Windows currently doesn't support IPv4-mapped IPv6 addresses and doesn't
		// support the IPV6_V6ONLY socket option so the following code would typically not be executed (or needed).
		
		#if( defined( IPV6_V6ONLY ) )
			option = 1;
			err = setsockopt( sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *) &option, sizeof( option ) );
			check_translated_errno( err == 0, errno_compat(), kOptionErr );		
		#endif
		
		if ( port.NotAnInteger )
		{
			// Join the all-DNS multicast group so we receive Multicast DNS packets.
		
			mreqv6.ipv6mr_multiaddr = *( (struct in6_addr *) &AllDNSLinkGroup_v6.ip.v6 );
			mreqv6.ipv6mr_interface = sa6p->sin6_scope_id;
			err = setsockopt( sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *) &mreqv6, sizeof( mreqv6 ) );
			check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
			// Specify the interface to send multicast packets on this socket.
		
			option = (int) sa6p->sin6_scope_id;
			err = setsockopt( sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char *) &option, sizeof( option ) );
			check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
			// Enable multicast loopback so we receive multicast packets we send (for same-machine operations).
			
			option = 1;
			err = setsockopt( sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (char *) &option, sizeof( option ) );
			check_translated_errno( err == 0, errno_compat(), kOptionErr );
		}

		// Send unicast packets with TTL 255 (helps against spoofing).
		
		option = 255;
		err = setsockopt( sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, (char *) &option, sizeof( option ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );

		// Send multicast packets with TTL 255 (helps against spoofing).
			
		option = 255;
		err = setsockopt( sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *) &option, sizeof( option ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
	}
	else
	{
		dlog( kDebugLevelError, DEBUG_NAME "%s: unsupport socket family (%d)\n", __ROUTINE__, inAddr->sa_family );
		err = kUnsupportedErr;
		goto exit;
	}
	
	// Success!
	
	*outSocketRef = sock;
	sock = kInvalidSocketRef;
	err = mStatus_NoError;
	
exit:
	if( IsValidSocket( sock ) )
	{
		close_compat( sock );
	}
	return( err );
}

//===========================================================================================================================
//	SetupSocket
//===========================================================================================================================

mDNSlocal mStatus	SockAddrToMDNSAddr( const struct sockaddr * const inSA, mDNSAddr *outIP, mDNSIPPort *outPort )
{
	mStatus		err;
	
	check( inSA );
	check( outIP );
	
	if( inSA->sa_family == AF_INET )
	{
		struct sockaddr_in *		sa4;
		
		sa4 						= (struct sockaddr_in *) inSA;
		outIP->type 				= mDNSAddrType_IPv4;
		outIP->ip.v4.NotAnInteger	= sa4->sin_addr.s_addr;
		if( outPort )
		{
			outPort->NotAnInteger	= sa4->sin_port;
		}
		err = mStatus_NoError;
	}
	else if( inSA->sa_family == AF_INET6 )
	{
		struct sockaddr_in6 *		sa6;
		
		sa6 			= (struct sockaddr_in6 *) inSA;
		outIP->type 	= mDNSAddrType_IPv6;
		outIP->ip.v6 	= *( (mDNSv6Addr *) &sa6->sin6_addr );
		if( IN6_IS_ADDR_LINKLOCAL( &sa6->sin6_addr ) )
		{
			outIP->ip.v6.w[ 1 ] = 0;
		}
		if( outPort )
		{
			outPort->NotAnInteger = sa6->sin6_port;
		}
		err = mStatus_NoError;
	}
	else
	{
		dlog( kDebugLevelError, DEBUG_NAME "%s: invalid sa_family %d", __ROUTINE__, inSA->sa_family );
		err = mStatus_BadParamErr;
	}
	return( err );
}

//===========================================================================================================================
//	SetupNotifications
//===========================================================================================================================

mDNSlocal mStatus	SetupNotifications( mDNS * const inMDNS )
{
	mStatus				err;
	SocketRef			sock;
	unsigned long		param;
	int					inBuffer;
	int					outBuffer;
	DWORD				outSize;
	
	// Register to listen for address list changes.
	
	sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	err = translate_errno( IsValidSocket( sock ), errno_compat(), kUnknownErr );
	require_noerr( err, exit );
	inMDNS->p->interfaceListChangedSocket = sock;
	
	// Make the socket non-blocking so the WSAIoctl returns immediately with WSAEWOULDBLOCK. It will set the event 
	// when a change to the interface list is detected.
	
	param = 1;
	err = ioctlsocket( sock, FIONBIO, &param );
	err = translate_errno( err == 0, errno_compat(), kUnknownErr );
	require_noerr( err, exit );
	
	inBuffer	= 0;
	outBuffer	= 0;
	err = WSAIoctl( sock, SIO_ADDRESS_LIST_CHANGE, &inBuffer, 0, &outBuffer, 0, &outSize, NULL, NULL );
	if( err < 0 )
	{
		check( errno_compat() == WSAEWOULDBLOCK );
	}
	
	err = WSAEventSelect( sock, inMDNS->p->interfaceListChangedEvent, FD_ADDRESS_LIST_CHANGE );
	err = translate_errno( err == 0, errno_compat(), kUnknownErr );
	require_noerr( err, exit );

	inMDNS->p->descChangedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	err = translate_errno( inMDNS->p->descChangedEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	if (inMDNS->p->descKey != NULL)
	{
		err = RegNotifyChangeKeyValue(inMDNS->p->descKey, TRUE, REG_NOTIFY_CHANGE_LAST_SET, inMDNS->p->descChangedEvent, TRUE);
		require_noerr( err, exit );
	}

	// This will catch all changes to tcp/ip networking, including changes to the domain search list

	inMDNS->p->tcpipChangedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	err = translate_errno( inMDNS->p->tcpipChangedEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	err = RegCreateKey( HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"), &inMDNS->p->tcpipKey );
	require_noerr( err, exit );

	err = RegNotifyChangeKeyValue(inMDNS->p->tcpipKey, TRUE, REG_NOTIFY_CHANGE_NAME|REG_NOTIFY_CHANGE_LAST_SET, inMDNS->p->tcpipChangedEvent, TRUE);
	require_noerr( err, exit );

	// This will catch all changes to ddns configuration

	inMDNS->p->ddnsChangedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	err = translate_errno( inMDNS->p->ddnsChangedEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	err = RegCreateKey( HKEY_LOCAL_MACHINE, kServiceParametersNode TEXT("\\DynDNS\\Setup"), &inMDNS->p->ddnsKey );
	require_noerr( err, exit );

	err = RegNotifyChangeKeyValue(inMDNS->p->ddnsKey, TRUE, REG_NOTIFY_CHANGE_NAME|REG_NOTIFY_CHANGE_LAST_SET, inMDNS->p->ddnsChangedEvent, TRUE);
	require_noerr( err, exit );

	// This will catch all changes to file sharing

	inMDNS->p->fileShareEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	err = translate_errno( inMDNS->p->fileShareEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	err = RegCreateKey( HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\Services\\lanmanserver\\Shares"), &inMDNS->p->fileShareKey );
	require_noerr( err, exit );

	err = RegNotifyChangeKeyValue(inMDNS->p->fileShareKey, TRUE, REG_NOTIFY_CHANGE_NAME|REG_NOTIFY_CHANGE_LAST_SET, inMDNS->p->fileShareEvent, TRUE);
	require_noerr( err, exit );

	// This will catch changes to the Windows firewall

	inMDNS->p->firewallEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	err = translate_errno( inMDNS->p->firewallEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	// Just to make sure that initialization doesn't fail on some old OS
	// that doesn't have this key, we'll only add the notification if
	// the key exists.

	err = RegCreateKey( HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\FirewallRules"), &inMDNS->p->firewallKey );
	
	if ( !err )
	{
		err = RegNotifyChangeKeyValue(inMDNS->p->firewallKey, TRUE, REG_NOTIFY_CHANGE_NAME|REG_NOTIFY_CHANGE_LAST_SET, inMDNS->p->firewallEvent, TRUE);
		require_noerr( err, exit );
	}
	else
	{
		err = mStatus_NoError;
	}

exit:
	if( err )
	{
		TearDownNotifications( inMDNS );
	}
	return( err );
}

//===========================================================================================================================
//	TearDownNotifications
//===========================================================================================================================

mDNSlocal mStatus	TearDownNotifications( mDNS * const inMDNS )
{
	if( IsValidSocket( inMDNS->p->interfaceListChangedSocket ) )
	{
		close_compat( inMDNS->p->interfaceListChangedSocket );
		inMDNS->p->interfaceListChangedSocket = kInvalidSocketRef;
	}

	if ( inMDNS->p->descChangedEvent != NULL )
	{
		CloseHandle( inMDNS->p->descChangedEvent );
		inMDNS->p->descChangedEvent = NULL;
	}

	if ( inMDNS->p->descKey != NULL )
	{
		RegCloseKey( inMDNS->p->descKey );
		inMDNS->p->descKey = NULL;
	}

	if ( inMDNS->p->tcpipChangedEvent != NULL )
	{
		CloseHandle( inMDNS->p->tcpipChangedEvent );
		inMDNS->p->tcpipChangedEvent = NULL;
	}

	if ( inMDNS->p->ddnsChangedEvent != NULL )
	{
		CloseHandle( inMDNS->p->ddnsChangedEvent );
		inMDNS->p->ddnsChangedEvent = NULL;
	}

	if ( inMDNS->p->ddnsKey != NULL )
	{
		RegCloseKey( inMDNS->p->ddnsKey );
		inMDNS->p->ddnsKey = NULL;
	}

	if ( inMDNS->p->fileShareEvent != NULL )
	{
		CloseHandle( inMDNS->p->fileShareEvent );
		inMDNS->p->fileShareEvent = NULL;
	}

	if ( inMDNS->p->fileShareKey != NULL )
	{
		RegCloseKey( inMDNS->p->fileShareKey );
		inMDNS->p->fileShareKey = NULL;
	}

	if ( inMDNS->p->firewallEvent != NULL )
	{
		CloseHandle( inMDNS->p->firewallEvent );
		inMDNS->p->firewallEvent = NULL;
	}

	if ( inMDNS->p->firewallKey != NULL )
	{
		RegCloseKey( inMDNS->p->firewallKey );
		inMDNS->p->firewallKey = NULL;
	}

	return( mStatus_NoError );
}


#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	SetupThread
//===========================================================================================================================

mDNSlocal mStatus	SetupThread( mDNS * const inMDNS )
{
	mStatus			err;
	HANDLE			threadHandle;
	unsigned		threadID;
	DWORD			result;
	
	dlog( kDebugLevelTrace, DEBUG_NAME "setting up thread\n" );
	
	// To avoid a race condition with the thread ID needed by the unlocking code, we need to make sure the
	// thread has fully initialized. To do this, we create the thread then wait for it to signal it is ready.
	
	inMDNS->p->initEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	err = translate_errno( inMDNS->p->initEvent, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	inMDNS->p->initStatus = mStatus_Invalid;
	
	// Create thread with _beginthreadex() instead of CreateThread() to avoid memory leaks when using static run-time 
	// libraries. See <http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dllproc/base/createthread.asp>.
	
	threadHandle = (HANDLE) _beginthreadex_compat( NULL, 0, ProcessingThread, inMDNS, 0, &threadID );
	err = translate_errno( threadHandle, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );
		
	result = WaitForSingleObject( inMDNS->p->initEvent, INFINITE );
	err = translate_errno( result == WAIT_OBJECT_0, (mStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );
	err = inMDNS->p->initStatus;
	require_noerr( err, exit );
	
exit:
	if( inMDNS->p->initEvent )
	{
		CloseHandle( inMDNS->p->initEvent );
		inMDNS->p->initEvent = 0;
	}
	dlog( kDebugLevelTrace, DEBUG_NAME "setting up thread done (err=%d %m)\n", err, err );
	return( err );
}

//===========================================================================================================================
//	TearDownThread
//===========================================================================================================================

mDNSlocal mStatus	TearDownThread( const mDNS * const inMDNS )
{
	// Signal the cancel event to cause the thread to exit. Then wait for the quit event to be signal indicating it did 
	// exit. If the quit event is not signal in 5 seconds, just give up and close anyway sinec the thread is probably hung.
	
	if( inMDNS->p->cancelEvent )
	{
		BOOL		wasSet;
		DWORD		result;
		
		wasSet = SetEvent( inMDNS->p->cancelEvent );
		check_translated_errno( wasSet, GetLastError(), kUnknownErr );
		
		if( inMDNS->p->quitEvent )
		{
			result = WaitForSingleObject( inMDNS->p->quitEvent, 5 * 1000 );
			check_translated_errno( result == WAIT_OBJECT_0, GetLastError(), kUnknownErr );
		}
	}
	return( mStatus_NoError );
}

//===========================================================================================================================
//	ProcessingThread
//===========================================================================================================================

mDNSlocal unsigned WINAPI	ProcessingThread( LPVOID inParam )
{
	mDNS *			m;
	int				done;
	mStatus			err;
	HANDLE *		waitList;
	int				waitListCount;
	DWORD			result;
	BOOL			wasSet;
	
	check( inParam );
		
	m = (mDNS *) inParam;
	err = ProcessingThreadInitialize( m );
	check( !err );
	
	done = 0;
	while( !done )
	{
		// Set up the list of objects we'll be waiting on.
		
		waitList 		= NULL;
		waitListCount	= 0;
		err = ProcessingThreadSetupWaitList( m, &waitList, &waitListCount );
		require_noerr( err, exit );
		
		// Main processing loop.
		
		gWaitListChanged = FALSE;

		for( ;; )
		{
			// Give the mDNS core a chance to do its work and determine next event time.
			
			mDNSs32 interval = mDNS_Execute(m) - mDNS_TimeNow(m);

			if ( gWaitListChanged )
			{
				break;
			}

			if (m->p->idleThreadCallback)
			{
				interval = m->p->idleThreadCallback(m, interval);
			}
			if      (interval < 0)						interval = 0;
			else if (interval > (0x7FFFFFFF / 1000))	interval = 0x7FFFFFFF / mDNSPlatformOneSecond;
			else										interval = (interval * 1000) / mDNSPlatformOneSecond;
			
			// Wait until something occurs (e.g. cancel, incoming packet, or timeout).
						
			result = WaitForMultipleObjects( (DWORD) waitListCount, waitList, FALSE, (DWORD) interval );
			check( result != WAIT_FAILED );

			if ( result != WAIT_FAILED )
			{
				if( result == WAIT_TIMEOUT )
				{
					// Next task timeout occurred. Loop back up to give mDNS core a chance to work.
					
					dlog( kDebugLevelChatty - 1, DEBUG_NAME "timeout\n" );
					continue;
				}
				else if( result == kWaitListCancelEvent )
				{
					// Cancel event. Set the done flag and break to exit.
					
					dlog( kDebugLevelVerbose, DEBUG_NAME "canceling...\n" );
					done = 1;
					break;
				}
				else if( result == kWaitListInterfaceListChangedEvent )
				{
					// It would be nice to come up with a more elegant solution to this, but it seems that
					// GetAdaptersAddresses doesn't always stay in sync after network changed events.  So as
					// as a simple workaround, we'll pause for a couple of seconds before processing the change.

					// We arrived at 2 secs by trial and error. We could reproduce the problem after sleeping
					// for 500 msec and 750 msec, but couldn't after sleeping for 1 sec.  We added another
					// second on top of that to account for machine load or some other exigency.

					Sleep( 2000 );

					// Interface list changed event. Break out of the inner loop to re-setup the wait list.
					
					ProcessingThreadInterfaceListChanged( m );
					break;
				}
				else if( result == kWaitListWakeupEvent )
				{
					// Wakeup event due to an mDNS API call. Loop back to call mDNS_Execute.
					
					dlog( kDebugLevelChatty - 1, DEBUG_NAME "wakeup for mDNS_Execute\n" );
					continue;
				}
				else if ( result == kWaitListComputerDescriptionEvent )
				{
					//
					// The computer description might have changed
					//
					ProcessingThreadComputerDescriptionChanged( m );
					break;
				}
				else if ( result == kWaitListTCPIPEvent )
				{	
					//
					// The TCP/IP might have changed
					//
					ProcessingThreadTCPIPConfigChanged( m );
					break;
				}
				else if ( result == kWaitListDynDNSEvent )
				{
					//
					// The DynDNS config might have changed
					//
					ProcessingThreadDynDNSConfigChanged( m );
					break;
				}
				else if ( result == kWaitListFileShareEvent )
				{
					//
					// File sharing changed
					//
					ProcessingThreadFileShareChanged( m );
					break;
				}
				else if ( result == kWaitListFirewallEvent )
				{
					//
					// Firewall configuration changed
					//
					ProcessingThreadFirewallChanged( m );
				}
				else
				{
					int		waitItemIndex;
					
					// Socket data available event. Determine which socket and process the packet.
					
					waitItemIndex = (int)( ( (int) result ) - WAIT_OBJECT_0 );
					dlog( kDebugLevelChatty, DEBUG_NAME "socket data available on socket index %d\n", waitItemIndex );
					check( ( waitItemIndex >= 0 ) && ( waitItemIndex < waitListCount ) );

					if( ( waitItemIndex >= 0 ) && ( waitItemIndex < waitListCount ) )
					{
						HANDLE					signaledObject;
						int						n = 0;
						mDNSInterfaceData	*	ifd;
						TCPSocket *			tcd;
						UDPSocket *			sock;
						
						signaledObject = waitList[ waitItemIndex ];
	
#if ( MDNS_WINDOWS_ENABLE_IPV4 )
						if ( m->p->unicastSock4ReadEvent == signaledObject )
						{
							ProcessingThreadProcessPacket( m, NULL, NULL, m->p->unicastSock4 );
							++n;
						}
#endif
						
#if ( MDNS_WINDOWS_ENABLE_IPV6 )
						if ( m->p->unicastSock6ReadEvent == signaledObject )
						{
							ProcessingThreadProcessPacket( m, NULL, NULL, m->p->unicastSock6 );
							++n;
						}
#endif
					
						for( ifd = m->p->interfaceList; ifd; ifd = ifd->next )
						{
							if( ifd->readPendingEvent == signaledObject )
							{
								ProcessingThreadProcessPacket( m, ifd, NULL, ifd->sock );
								++n;
							}
						}
	
						for ( tcd = gTCPConnectionList; tcd; tcd = tcd->next )
						{
							if ( tcd->pendingEvent == signaledObject )
							{
								mDNSBool connect = FALSE;
	
								if ( !tcd->connected )
								{
									tcd->connected	= mDNStrue;
									connect			= mDNStrue;
								}
	
								tcd->callback( tcd, tcd->context, connect, 0 );
	
								++n;
	
								break;
							}
						}
	
						for ( sock = gUDPSocketList; sock; sock = sock->next )
						{
							if ( sock->readEvent == signaledObject )
							{
								ProcessingThreadProcessPacket( m, NULL, sock, sock->sock );
	
								++n;
	
								break;
							}
						}

						check( n > 0 );
					}
					else
					{
						// Unexpected wait result.
					
						dlog( kDebugLevelWarning, DEBUG_NAME "%s: unexpected wait result (result=0x%08X)\n", __ROUTINE__, result );
					}
				}
			}
			else
			{
				Sleep( 3 * 1000 );
				err = ProcessingThreadInitialize( m );
				check( err );
				break;
			}
		}
		
		// Release the wait list.
		
		if( waitList )
		{
			free( waitList );
			waitList = NULL;
			waitListCount = 0;
		}
	}
	
	// Signal the quit event to indicate that the thread is finished.

exit:
	wasSet = SetEvent( m->p->quitEvent );
	check_translated_errno( wasSet, GetLastError(), kUnknownErr );
	
	// Call _endthreadex() explicitly instead of just exiting normally to avoid memory leaks when using static run-time
	// libraries. See <http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dllproc/base/createthread.asp>.
	
	_endthreadex_compat( 0 );
	return( 0 );
}

//===========================================================================================================================
//	ProcessingThreadInitialize
//===========================================================================================================================

mDNSlocal mStatus ProcessingThreadInitialize( mDNS * const inMDNS )
{
	mStatus		err;
	BOOL		wasSet;
	
	inMDNS->p->threadID = GetCurrentThreadId();

	err = SetupInterfaceList( inMDNS );
	require_noerr( err, exit );

	err = uDNS_SetupDNSConfig( inMDNS );
	require_noerr( err, exit );

exit:

	if( err )
	{
		TearDownInterfaceList( inMDNS );
	}
	inMDNS->p->initStatus = err;
	
	wasSet = SetEvent( inMDNS->p->initEvent );
	check_translated_errno( wasSet, GetLastError(), kUnknownErr );
	return( err );
}

//===========================================================================================================================
//	ProcessingThreadSetupWaitList
//===========================================================================================================================

mDNSlocal mStatus	ProcessingThreadSetupWaitList( mDNS * const inMDNS, HANDLE **outWaitList, int *outWaitListCount )
{
	mStatus					err;
	int						waitListCount;
	HANDLE *				waitList;
	HANDLE *				waitItemPtr;
	mDNSInterfaceData	*	ifd;
	TCPSocket *			tcd;
	UDPSocket *			sock;
	
	dlog( kDebugLevelTrace, DEBUG_NAME "thread setting up wait list\n" );
	check( inMDNS );
	check( inMDNS->p );
	check( outWaitList );
	check( outWaitListCount );
	
	// Allocate an array to hold all the objects to wait on.
	
	waitListCount = kWaitListFixedItemCount + inMDNS->p->interfaceCount + gTCPConnections + gUDPSockets;
	waitList = (HANDLE *) malloc( waitListCount * sizeof( *waitList ) );
	require_action( waitList, exit, err = mStatus_NoMemoryErr );
	waitItemPtr = waitList;
	
	// Add the fixed wait items to the beginning of the list.
	
	*waitItemPtr++ = inMDNS->p->cancelEvent;
	*waitItemPtr++ = inMDNS->p->interfaceListChangedEvent;
	*waitItemPtr++ = inMDNS->p->wakeupEvent;
	*waitItemPtr++ = inMDNS->p->descChangedEvent;
	*waitItemPtr++ = inMDNS->p->tcpipChangedEvent;
	*waitItemPtr++ = inMDNS->p->ddnsChangedEvent;
	*waitItemPtr++ = inMDNS->p->fileShareEvent;
	*waitItemPtr++ = inMDNS->p->firewallEvent;
	
	// Append all the dynamic wait items to the list.
#if ( MDNS_WINDOWS_ENABLE_IPV4 )
	*waitItemPtr++ = inMDNS->p->unicastSock4ReadEvent;
#endif

#if ( MDNS_WINDOWS_ENABLE_IPV6 )
	*waitItemPtr++ = inMDNS->p->unicastSock6ReadEvent;
#endif

	for( ifd = inMDNS->p->interfaceList; ifd; ifd = ifd->next )
	{
		*waitItemPtr++ = ifd->readPendingEvent;
	}

	for ( tcd = gTCPConnectionList; tcd; tcd = tcd->next )
	{
		*waitItemPtr++ = tcd->pendingEvent;
	}

	for ( sock = gUDPSocketList; sock; sock = sock->next )
	{
		*waitItemPtr++ = sock->readEvent;
	}

	check( (int)( waitItemPtr - waitList ) == waitListCount );
	
	*outWaitList 		= waitList;
	*outWaitListCount	= waitListCount;
	waitList			= NULL;
	err					= mStatus_NoError;
	
exit:
	if( waitList )
	{
		free( waitList );
	}
	dlog( kDebugLevelTrace, DEBUG_NAME "thread setting up wait list done (err=%d %m)\n", err, err );
	return( err );
}

//===========================================================================================================================
//	ProcessingThreadProcessPacket
//===========================================================================================================================

mDNSlocal void	ProcessingThreadProcessPacket( mDNS *inMDNS, mDNSInterfaceData *inIFD, UDPSocket * inUDPSocket, SocketRef inSock )
{
	OSStatus					err;
	const mDNSInterfaceID		iid = inIFD ? inIFD->interfaceInfo.InterfaceID : NULL;
	LPFN_WSARECVMSG				recvMsgPtr;
	mDNSAddr					srcAddr;
	mDNSIPPort					srcPort;
	mDNSAddr					dstAddr;
	mDNSIPPort					dstPort;
	mDNSu8						ttl;
	struct sockaddr_storage		addr;
	DNSMessage					packet;
	mDNSu8 *					end;
	int							n;
	
	check( inMDNS );
	check( IsValidSocket( inSock ) );
	
	// Set up the default in case the packet info options are not supported or reported correctly.
	
	if ( inIFD )
	{
		recvMsgPtr	= inIFD->wsaRecvMsgFunctionPtr;
		dstAddr		= inIFD->defaultAddr;
		dstPort		= MulticastDNSPort;
		ttl			= 255;
	}
	else if ( inUDPSocket )
	{
		recvMsgPtr	= inUDPSocket->recvMsgPtr;
		dstAddr		= inUDPSocket->dstAddr;
		dstPort		= inUDPSocket->port;
		ttl			= 255;
	}
	else if ( inSock == inMDNS->p->unicastSock4 )
	{
		recvMsgPtr	= inMDNS->p->unicastSock4RecvMsgPtr;
		dstAddr		= inMDNS->p->unicastSock4DestAddr;
		dstPort		= inMDNS->UnicastPort4;
		ttl			= 255;
	}
	else if ( inSock == inMDNS->p->unicastSock6 )
	{
		recvMsgPtr	= inMDNS->p->unicastSock6RecvMsgPtr;
		dstAddr		= inMDNS->p->unicastSock6DestAddr;
		dstPort		= inMDNS->UnicastPort6;
		ttl			= 255;
	}
	else
	{
		dlog( kDebugLevelError, DEBUG_NAME "packet received on unknown socket\n" );
		goto exit;
	}

#if( !TARGET_OS_WINDOWS_CE )
	if( recvMsgPtr )
	{
		WSAMSG				msg;
		WSABUF				buf;
		uint8_t				controlBuffer[ 128 ];
		DWORD				size;
		LPWSACMSGHDR		header;
		
		// Set up the buffer and read the packet.
		
		msg.name			= (LPSOCKADDR) &addr;
		msg.namelen			= (INT) sizeof( addr );
		buf.buf				= (char *) &packet;
		buf.len				= (u_long) sizeof( packet );
		msg.lpBuffers		= &buf;
		msg.dwBufferCount	= 1;
		msg.Control.buf		= (char *) controlBuffer;
		msg.Control.len		= (u_long) sizeof( controlBuffer );
		msg.dwFlags			= 0;
				
		err = recvMsgPtr( inSock, &msg, &size, NULL, NULL );
		err = translate_errno( err == 0, (OSStatus) WSAGetLastError(), kUnknownErr );
		require_noerr( err, exit );
		n = (int) size;
		
		// Parse the control information. Reject packets received on the wrong interface.
		
		for( header = WSA_CMSG_FIRSTHDR( &msg ); header; header = WSA_CMSG_NXTHDR( &msg, header ) )
		{
			if( ( header->cmsg_level == IPPROTO_IP ) && ( header->cmsg_type == IP_PKTINFO ) )
			{
				IN_PKTINFO *		ipv4PacketInfo;
				
				ipv4PacketInfo = (IN_PKTINFO *) WSA_CMSG_DATA( header );

				if ( inIFD )
				{
					require_action( ipv4PacketInfo->ipi_ifindex == inIFD->index, exit, err = kMismatchErr );
				}

				dstAddr.type 				= mDNSAddrType_IPv4;
				dstAddr.ip.v4.NotAnInteger	= ipv4PacketInfo->ipi_addr.s_addr;
			}
			else if( ( header->cmsg_level == IPPROTO_IPV6 ) && ( header->cmsg_type == IPV6_PKTINFO ) )
			{
				IN6_PKTINFO *		ipv6PacketInfo;
				
				ipv6PacketInfo = (IN6_PKTINFO *) WSA_CMSG_DATA( header );

				if ( inIFD )
				{
					require_action( ipv6PacketInfo->ipi6_ifindex == ( inIFD->index - kIPv6IfIndexBase), exit, err = kMismatchErr );
				}

				dstAddr.type	= mDNSAddrType_IPv6;
				dstAddr.ip.v6	= *( (mDNSv6Addr *) &ipv6PacketInfo->ipi6_addr );
			}
		}
	}
	else
#endif
	{
		int	addrSize;
		
		addrSize = sizeof( addr );
		n = recvfrom( inSock, (char *) &packet, sizeof( packet ), 0, (struct sockaddr *) &addr, &addrSize );
		err = translate_errno( n > 0, errno_compat(), kUnknownErr );
		require_noerr( err, exit );
	}
	SockAddrToMDNSAddr( (struct sockaddr *) &addr, &srcAddr, &srcPort );
	
	// Dispatch the packet to mDNS.
	
	dlog( kDebugLevelChatty, DEBUG_NAME "packet received\n" );
	dlog( kDebugLevelChatty, DEBUG_NAME "    size      = %d\n", n );
	dlog( kDebugLevelChatty, DEBUG_NAME "    src       = %#a:%u\n", &srcAddr, ntohs( srcPort.NotAnInteger ) );
	dlog( kDebugLevelChatty, DEBUG_NAME "    dst       = %#a:%u\n", &dstAddr, ntohs( dstPort.NotAnInteger ) );

	if ( inIFD )
	{
		dlog( kDebugLevelChatty, DEBUG_NAME "    interface = %#a (index=0x%08X)\n", &inIFD->interfaceInfo.ip, (int) inIFD->index );
	}

	dlog( kDebugLevelChatty, DEBUG_NAME "\n" );
	
	end = ( (mDNSu8 *) &packet ) + n;
	mDNSCoreReceive( inMDNS, &packet, end, &srcAddr, srcPort, &dstAddr, dstPort, iid );
	
exit:
	return;
}

//===========================================================================================================================
//	ProcessingThreadInterfaceListChanged
//===========================================================================================================================

mDNSlocal void	ProcessingThreadInterfaceListChanged( mDNS *inMDNS )
{
	mStatus		err;
	
	dlog( kDebugLevelInfo, DEBUG_NAME "interface list changed\n" );
	check( inMDNS );

	if (inMDNS->p->interfaceListChangedCallback)
	{
		inMDNS->p->interfaceListChangedCallback(inMDNS);
	}
	
	mDNSPlatformLock( inMDNS );
	
	// Tear down the existing interfaces and set up new ones using the new IP info.
	
	err = TearDownInterfaceList( inMDNS );
	check_noerr( err );
	
	err = SetupInterfaceList( inMDNS );
	check_noerr( err );
		
	err = uDNS_SetupDNSConfig( inMDNS );
	check_noerr( err );

	mDNSPlatformUnlock( inMDNS );
	
	// Inform clients of the change.
	
	mDNS_ConfigChanged(inMDNS);
	
	// Force mDNS to update.
	
	mDNSCoreMachineSleep( inMDNS, mDNSfalse ); // What is this for? Mac OS X does not do this
}


//===========================================================================================================================
//	ProcessingThreadComputerDescriptionChanged
//===========================================================================================================================
mDNSlocal void	ProcessingThreadComputerDescriptionChanged( mDNS *inMDNS )
{
	mStatus		err;
	
	dlog( kDebugLevelInfo, DEBUG_NAME "computer description has changed\n" );
	check( inMDNS );

	mDNSPlatformLock( inMDNS );

	// redo the names
	SetupNiceName( inMDNS );

	if (inMDNS->p->hostDescriptionChangedCallback)
	{
		inMDNS->p->hostDescriptionChangedCallback(inMDNS);
	}
	
	// and reset the event handler
	if ((inMDNS->p->descKey != NULL) && (inMDNS->p->descChangedEvent))
	{
		err = RegNotifyChangeKeyValue(inMDNS->p->descKey, TRUE, REG_NOTIFY_CHANGE_LAST_SET, inMDNS->p->descChangedEvent, TRUE);
		check_noerr( err );
	}

	mDNSPlatformUnlock( inMDNS );
}


//===========================================================================================================================
//	ProcessingThreadTCPIPConfigChanged
//===========================================================================================================================
mDNSlocal void ProcessingThreadTCPIPConfigChanged( mDNS * inMDNS )
{
	mStatus		err;
	
	dlog( kDebugLevelInfo, DEBUG_NAME "TCP/IP config has changed\n" );
	check( inMDNS );

	mDNSPlatformLock( inMDNS );

	err = uDNS_SetupDNSConfig( inMDNS );
	check_noerr( err );

	// and reset the event handler

	if ( ( inMDNS->p->tcpipKey != NULL ) && ( inMDNS->p->tcpipChangedEvent ) )
	{
		err = RegNotifyChangeKeyValue( inMDNS->p->tcpipKey, TRUE, REG_NOTIFY_CHANGE_NAME|REG_NOTIFY_CHANGE_LAST_SET, inMDNS->p->tcpipChangedEvent, TRUE );
		check_noerr( err );
	}

	mDNSPlatformUnlock( inMDNS );
}


//===========================================================================================================================
//	ProcessingThreadDynDNSConfigChanged
//===========================================================================================================================
mDNSlocal void	ProcessingThreadDynDNSConfigChanged( mDNS *inMDNS )
{
	mStatus		err;
	
	dlog( kDebugLevelInfo, DEBUG_NAME "DynDNS config has changed\n" );
	check( inMDNS );

	SetDomainSecrets( inMDNS );

	mDNSPlatformLock( inMDNS );

	err = uDNS_SetupDNSConfig( inMDNS );
	check_noerr( err );

	// and reset the event handler

	if ((inMDNS->p->ddnsKey != NULL) && (inMDNS->p->ddnsChangedEvent))
	{
		err = RegNotifyChangeKeyValue(inMDNS->p->ddnsKey, TRUE, REG_NOTIFY_CHANGE_NAME|REG_NOTIFY_CHANGE_LAST_SET, inMDNS->p->ddnsChangedEvent, TRUE);
		check_noerr( err );
	}

	mDNSPlatformUnlock( inMDNS );
}


//===========================================================================================================================
//	ProcessingThreadFileShareChanged
//===========================================================================================================================
mDNSlocal void	ProcessingThreadFileShareChanged( mDNS *inMDNS )
{
	mStatus err;
	
	dlog( kDebugLevelInfo, DEBUG_NAME "File shares has changed\n" );
	check( inMDNS );

	CheckFileShares( inMDNS );

	// and reset the event handler
	
	if ((inMDNS->p->fileShareKey != NULL) && (inMDNS->p->fileShareEvent))
	{
		err = RegNotifyChangeKeyValue(inMDNS->p->fileShareKey, TRUE, REG_NOTIFY_CHANGE_NAME|REG_NOTIFY_CHANGE_LAST_SET, inMDNS->p->fileShareEvent, TRUE);
		check_noerr( err );
	}
}


//===========================================================================================================================
//	ProcessingThreadFileShareChanged
//===========================================================================================================================
mDNSlocal void	ProcessingThreadFirewallChanged( mDNS *inMDNS )
{
	mStatus err;
	
	dlog( kDebugLevelInfo, DEBUG_NAME "Firewall has changed\n" );
	check( inMDNS );

	CheckFileShares( inMDNS );

	// and reset the event handler
	
	if ((inMDNS->p->firewallKey != NULL) && (inMDNS->p->firewallEvent))
	{
		err = RegNotifyChangeKeyValue(inMDNS->p->firewallKey, TRUE, REG_NOTIFY_CHANGE_NAME|REG_NOTIFY_CHANGE_LAST_SET, inMDNS->p->firewallEvent, TRUE);
		check_noerr( err );
	}
}



#if 0
#pragma mark -
#pragma mark == Utilities ==
#endif

//===========================================================================================================================
//	getifaddrs
//===========================================================================================================================

mDNSlocal int	getifaddrs( struct ifaddrs **outAddrs )
{
	int		err;
	
#if( MDNS_WINDOWS_USE_IPV6_IF_ADDRS && !TARGET_OS_WINDOWS_CE )
	
	// Try to the load the GetAdaptersAddresses function from the IP Helpers DLL. This API is only available on Windows
	// XP or later. Looking up the symbol at runtime allows the code to still work on older systems without that API.
	
	if( !gIPHelperLibraryInstance )
	{
		gIPHelperLibraryInstance = LoadLibrary( TEXT( "Iphlpapi" ) );
		if( gIPHelperLibraryInstance )
		{
			gGetAdaptersAddressesFunctionPtr = 
				(GetAdaptersAddressesFunctionPtr) GetProcAddress( gIPHelperLibraryInstance, "GetAdaptersAddresses" );
			if( !gGetAdaptersAddressesFunctionPtr )
			{
				BOOL		ok;
				
				ok = FreeLibrary( gIPHelperLibraryInstance );
				check_translated_errno( ok, GetLastError(), kUnknownErr );
				gIPHelperLibraryInstance = NULL;
			}
		}
	}
	
	// Use the new IPv6-capable routine if supported. Otherwise, fall back to the old and compatible IPv4-only code.
	// <rdar://problem/4278934>  Fall back to using getifaddrs_ipv4 if getifaddrs_ipv6 fails
	// <rdar://problem/6145913>  Fall back to using getifaddrs_ipv4 if getifaddrs_ipv6 returns no addrs

	if( !gGetAdaptersAddressesFunctionPtr || ( ( ( err = getifaddrs_ipv6( outAddrs ) ) != mStatus_NoError ) || ( ( outAddrs != NULL ) && ( *outAddrs == NULL ) ) ) )
	{
		err = getifaddrs_ipv4( outAddrs );
		require_noerr( err, exit );
	}
	
#elif( !TARGET_OS_WINDOWS_CE )

	err = getifaddrs_ipv4( outAddrs );
	require_noerr( err, exit );

#else

	err = getifaddrs_ce( outAddrs );
	require_noerr( err, exit );

#endif

exit:
	return( err );
}

#if( MDNS_WINDOWS_USE_IPV6_IF_ADDRS )
//===========================================================================================================================
//	getifaddrs_ipv6
//===========================================================================================================================

mDNSlocal int	getifaddrs_ipv6( struct ifaddrs **outAddrs )
{
	DWORD						err;
	int							i;
	DWORD						flags;
	struct ifaddrs *			head;
	struct ifaddrs **			next;
	IP_ADAPTER_ADDRESSES *		iaaList;
	ULONG						iaaListSize;
	IP_ADAPTER_ADDRESSES *		iaa;
	size_t						size;
	struct ifaddrs *			ifa;
	
	check( gGetAdaptersAddressesFunctionPtr );
	
	head	= NULL;
	next	= &head;
	iaaList	= NULL;
	
	// Get the list of interfaces. The first call gets the size and the second call gets the actual data.
	// This loops to handle the case where the interface changes in the window after getting the size, but before the
	// second call completes. A limit of 100 retries is enforced to prevent infinite loops if something else is wrong.
	
	flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;
	i = 0;
	for( ;; )
	{
		iaaListSize = 0;
		err = gGetAdaptersAddressesFunctionPtr( AF_UNSPEC, flags, NULL, NULL, &iaaListSize );
		check( err == ERROR_BUFFER_OVERFLOW );
		check( iaaListSize >= sizeof( IP_ADAPTER_ADDRESSES ) );
		
		iaaList = (IP_ADAPTER_ADDRESSES *) malloc( iaaListSize );
		require_action( iaaList, exit, err = ERROR_NOT_ENOUGH_MEMORY );
		
		err = gGetAdaptersAddressesFunctionPtr( AF_UNSPEC, flags, NULL, iaaList, &iaaListSize );
		if( err == ERROR_SUCCESS ) break;
		
		free( iaaList );
		iaaList = NULL;
		++i;
		require( i < 100, exit );
		dlog( kDebugLevelWarning, "%s: retrying GetAdaptersAddresses after %d failure(s) (%d %m)\n", __ROUTINE__, i, err, err );
	}
	
	for( iaa = iaaList; iaa; iaa = iaa->Next )
	{
		int								addrIndex;
		IP_ADAPTER_UNICAST_ADDRESS	*	addr;
		DWORD							ipv6IfIndex;
		IP_ADAPTER_PREFIX			*	firstPrefix;

		if( iaa->IfIndex > 0xFFFFFF )
		{
			dlog( kDebugLevelAlert, DEBUG_NAME "%s: IPv4 ifindex out-of-range (0x%08X)\n", __ROUTINE__, iaa->IfIndex );
		}
		if( iaa->Ipv6IfIndex > 0xFF )
		{
			dlog( kDebugLevelAlert, DEBUG_NAME "%s: IPv6 ifindex out-of-range (0x%08X)\n", __ROUTINE__, iaa->Ipv6IfIndex );
		}

		// For IPv4 interfaces, there seems to be a bug in iphlpapi.dll that causes the 
		// following code to crash when iterating through the prefix list.  This seems
		// to occur when iaa->Ipv6IfIndex != 0 when IPv6 is not installed on the host.
		// This shouldn't happen according to Microsoft docs which states:
		//
		//     "Ipv6IfIndex contains 0 if IPv6 is not available on the interface."
		//
		// So the data structure seems to be corrupted when we return from
		// GetAdaptersAddresses(). The bug seems to occur when iaa->Length <
		// sizeof(IP_ADAPTER_ADDRESSES), so when that happens, we'll manually
		// modify iaa to have the correct values.

		if ( iaa->Length >= sizeof( IP_ADAPTER_ADDRESSES ) )
		{
			ipv6IfIndex = iaa->Ipv6IfIndex;
			firstPrefix = iaa->FirstPrefix;
		}
		else
		{
			ipv6IfIndex	= 0;
			firstPrefix = NULL;
		}

		// Skip pseudo and tunnel interfaces.
		
		if( ( ( ipv6IfIndex == 1 ) && ( iaa->IfType != IF_TYPE_SOFTWARE_LOOPBACK ) ) || ( iaa->IfType == IF_TYPE_TUNNEL ) )
		{
			continue;
		}
		
		// Add each address as a separate interface to emulate the way getifaddrs works.
		
		for( addrIndex = 0, addr = iaa->FirstUnicastAddress; addr; ++addrIndex, addr = addr->Next )
		{			
			int						family;
			int						prefixIndex;
			IP_ADAPTER_PREFIX *		prefix;
			ULONG					prefixLength;
			uint32_t				ipv4Index;
			struct sockaddr_in		ipv4Netmask;

			family = addr->Address.lpSockaddr->sa_family;
			if( ( family != AF_INET ) && ( family != AF_INET6 ) ) continue;
			
			// <rdar://problem/6220642> iTunes 8: Bonjour doesn't work after upgrading iTunes 8
			// Seems as if the problem here is a buggy implementation of some network interface
			// driver. It is reporting that is has a link-local address when it is actually
			// disconnected. This was causing a problem in AddressToIndexAndMask.
			// The solution is to call AddressToIndexAndMask first, and if unable to lookup
			// the address, to ignore that address.

			ipv4Index = 0;
			memset( &ipv4Netmask, 0, sizeof( ipv4Netmask ) );
			
			if ( family == AF_INET )
			{
				err = AddressToIndexAndMask( addr->Address.lpSockaddr, &ipv4Index, ( struct sockaddr* ) &ipv4Netmask );
				
				if ( err )
				{
					err = 0;
					continue;
				}
			}

			ifa = (struct ifaddrs *) calloc( 1, sizeof( struct ifaddrs ) );
			require_action( ifa, exit, err = WSAENOBUFS );
			
			*next = ifa;
			next  = &ifa->ifa_next;
			
			// Get the name.
			
			size = strlen( iaa->AdapterName ) + 1;
			ifa->ifa_name = (char *) malloc( size );
			require_action( ifa->ifa_name, exit, err = WSAENOBUFS );
			memcpy( ifa->ifa_name, iaa->AdapterName, size );
			
			// Get interface flags.
			
			ifa->ifa_flags = 0;
			if( iaa->OperStatus == IfOperStatusUp ) 		ifa->ifa_flags |= IFF_UP;
			if( iaa->IfType == IF_TYPE_SOFTWARE_LOOPBACK )	ifa->ifa_flags |= IFF_LOOPBACK;
			else if ( IsPointToPoint( addr ) )				ifa->ifa_flags |= IFF_POINTTOPOINT;
			if( !( iaa->Flags & IP_ADAPTER_NO_MULTICAST ) )	ifa->ifa_flags |= IFF_MULTICAST;

			
			// <rdar://problem/4045657> Interface index being returned is 512
			//
			// Windows does not have a uniform scheme for IPv4 and IPv6 interface indexes.
			// This code used to shift the IPv4 index up to ensure uniqueness between
			// it and IPv6 indexes.  Although this worked, it was somewhat confusing to developers, who
			// then see interface indexes passed back that don't correspond to anything
			// that is seen in Win32 APIs or command line tools like "route".  As a relatively
			// small percentage of developers are actively using IPv6, it seems to 
			// make sense to make our use of IPv4 as confusion free as possible.
			// So now, IPv6 interface indexes will be shifted up by a
			// constant value which will serve to uniquely identify them, and we will
			// leave IPv4 interface indexes unmodified.
			
			switch( family )
			{
				case AF_INET:  ifa->ifa_extra.index = iaa->IfIndex; break;
				case AF_INET6: ifa->ifa_extra.index = ipv6IfIndex + kIPv6IfIndexBase;	 break;
				default: break;
			}

			// Get lease lifetime

			if ( ( iaa->IfType != IF_TYPE_SOFTWARE_LOOPBACK ) && ( addr->LeaseLifetime != 0 ) && ( addr->LeaseLifetime != 0xFFFFFFFF ) )
			{
				ifa->ifa_dhcpEnabled		= TRUE;
				ifa->ifa_dhcpLeaseExpires	= time( NULL ) + addr->LeaseLifetime;
			}
			else
			{
				ifa->ifa_dhcpEnabled		= FALSE;
				ifa->ifa_dhcpLeaseExpires	= 0;
			}

			// Get WakeOnLAN settings

			if ( iaa->IfType == IF_TYPE_ETHERNET_CSMACD )
			{
				ifa->ifa_womp = IsWOMPEnabled( iaa->AdapterName );
			}
			
			// Get address.
			
			switch( family )
			{
				case AF_INET:
				case AF_INET6:
					ifa->ifa_addr = (struct sockaddr *) calloc( 1, (size_t) addr->Address.iSockaddrLength );
					require_action( ifa->ifa_addr, exit, err = WSAENOBUFS );
					memcpy( ifa->ifa_addr, addr->Address.lpSockaddr, (size_t) addr->Address.iSockaddrLength );
					break;
				
				default:
					break;
			}
			check( ifa->ifa_addr );
			
			// Get subnet mask (IPv4)/link prefix (IPv6). It is specified as a bit length (e.g. 24 for 255.255.255.0).
			
			prefixLength = 0;
			for( prefixIndex = 0, prefix = firstPrefix; prefix; ++prefixIndex, prefix = prefix->Next )
			{
				if( ( prefix->Address.lpSockaddr->sa_family == family ) && ( prefixIndex == addrIndex ) )
				{
					check_string( prefix->Address.lpSockaddr->sa_family == family, "addr family != netmask family" );
					prefixLength = prefix->PrefixLength;
					break;
				}
			}
			switch( family )
			{
				case AF_INET:
				{
					struct sockaddr_in * sa4;
					
					sa4 = (struct sockaddr_in *) calloc( 1, sizeof( *sa4 ) );
					require_action( sa4, exit, err = WSAENOBUFS );
					sa4->sin_family = AF_INET;
					sa4->sin_addr.s_addr = ipv4Netmask.sin_addr.s_addr;

					dlog( kDebugLevelInfo, DEBUG_NAME "%s: IPv4 mask = %s\n", __ROUTINE__, inet_ntoa( sa4->sin_addr ) );
					ifa->ifa_netmask = (struct sockaddr *) sa4;
					break;
				}
				
				case AF_INET6:
				{
					struct sockaddr_in6 *		sa6;
					int							len;
					int							maskIndex;
					uint8_t						maskByte;
					
					require_action( prefixLength <= 128, exit, err = ERROR_INVALID_DATA );
					
					sa6 = (struct sockaddr_in6 *) calloc( 1, sizeof( *sa6 ) );
					require_action( sa6, exit, err = WSAENOBUFS );
					sa6->sin6_family = AF_INET6;
					
					if( prefixLength == 0 )
					{
						dlog( kDebugLevelWarning, DEBUG_NAME "%s: IPv6 link prefix 0, defaulting to /128\n", __ROUTINE__ );
						prefixLength = 128;
					}
					maskIndex = 0;
					for( len = (int) prefixLength; len > 0; len -= 8 )
					{
						if( len >= 8 ) maskByte = 0xFF;
						else		   maskByte = (uint8_t)( ( 0xFFU << ( 8 - len ) ) & 0xFFU );
						sa6->sin6_addr.s6_addr[ maskIndex++ ] = maskByte;
					}
					ifa->ifa_netmask = (struct sockaddr *) sa6;
					break;
				}
				
				default:
					break;
			}
		}
	}
	
	// Success!
	
	if( outAddrs )
	{
		*outAddrs = head;
		head = NULL;
	}
	err = ERROR_SUCCESS;
	
exit:
	if( head )
	{
		freeifaddrs( head );
	}
	if( iaaList )
	{
		free( iaaList );
	}
	return( (int) err );
}

#endif	// MDNS_WINDOWS_USE_IPV6_IF_ADDRS

#if( !TARGET_OS_WINDOWS_CE )
//===========================================================================================================================
//	getifaddrs_ipv4
//===========================================================================================================================

mDNSlocal int	getifaddrs_ipv4( struct ifaddrs **outAddrs )
{
	int						err;
	SOCKET					sock;
	DWORD					size;
	DWORD					actualSize;
	INTERFACE_INFO *		buffer;
	INTERFACE_INFO *		tempBuffer;
	INTERFACE_INFO *		ifInfo;
	IP_ADAPTER_INFO *		pAdapterInfo;
	IP_ADAPTER_INFO *		pAdapter;
	ULONG					bufLen;
	int						n;
	int						i;
	struct ifaddrs *		head;
	struct ifaddrs **		next;
	struct ifaddrs *		ifa;
	
	sock	= INVALID_SOCKET;
	buffer	= NULL;
	head	= NULL;
	next	= &head;
	pAdapterInfo = NULL;
	
	// Get the interface list. WSAIoctl is called with SIO_GET_INTERFACE_LIST, but since this does not provide a 
	// way to determine the size of the interface list beforehand, we have to start with an initial size guess and
	// call WSAIoctl repeatedly with increasing buffer sizes until it succeeds. Limit this to 100 tries for safety.
	
	sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	err = translate_errno( IsValidSocket( sock ), errno_compat(), kUnknownErr );
	require_noerr( err, exit );
		
	n = 0;
	size = 16 * sizeof( INTERFACE_INFO );
	for( ;; )
	{
		tempBuffer = (INTERFACE_INFO *) realloc( buffer, size );
		require_action( tempBuffer, exit, err = WSAENOBUFS );
		buffer = tempBuffer;
		
		err = WSAIoctl( sock, SIO_GET_INTERFACE_LIST, NULL, 0, buffer, size, &actualSize, NULL, NULL );
		if( err == 0 )
		{
			break;
		}
		
		++n;
		require_action( n < 100, exit, err = WSAEADDRNOTAVAIL );
		
		size += ( 16 * sizeof( INTERFACE_INFO ) );
	}
	check( actualSize <= size );
	check( ( actualSize % sizeof( INTERFACE_INFO ) ) == 0 );
	n = (int)( actualSize / sizeof( INTERFACE_INFO ) );

	// Now call GetAdaptersInfo so we can get DHCP information for each interface

	pAdapterInfo	= NULL;
	bufLen			= 0;
	
	for ( i = 0; i < 100; i++ )
	{
		err = GetAdaptersInfo( pAdapterInfo, &bufLen);

		if ( err != ERROR_BUFFER_OVERFLOW )
		{
			break;
		}

		pAdapterInfo = (IP_ADAPTER_INFO*) realloc( pAdapterInfo, bufLen );

		if ( !pAdapterInfo )
		{
			break;
		}
	}
	
	// Process the raw interface list and build a linked list of IPv4 interfaces.
	
	for( i = 0; i < n; ++i )
	{
		uint32_t ifIndex;
		struct sockaddr_in netmask;
		
		ifInfo = &buffer[ i ];
		if( ifInfo->iiAddress.Address.sa_family != AF_INET )
		{
			continue;
		}
		
		// <rdar://problem/6220642> iTunes 8: Bonjour doesn't work after upgrading iTunes 8
		// See comment in getifaddrs_ipv6

		ifIndex = 0;
		memset( &netmask, 0, sizeof( netmask ) );
		err = AddressToIndexAndMask( ( struct sockaddr* ) &ifInfo->iiAddress.AddressIn, &ifIndex, ( struct sockaddr* ) &netmask );

		if ( err )
		{
			continue;
		}

		ifa = (struct ifaddrs *) calloc( 1, sizeof( struct ifaddrs ) );
		require_action( ifa, exit, err = WSAENOBUFS );
		
		*next = ifa;
		next  = &ifa->ifa_next;
		
		// Get the name.
		
		ifa->ifa_name = (char *) malloc( 16 );
		require_action( ifa->ifa_name, exit, err = WSAENOBUFS );
		sprintf( ifa->ifa_name, "%d", i + 1 );
		
		// Get interface flags.
		
		ifa->ifa_flags = (u_int) ifInfo->iiFlags;
		
		// Get addresses.
		
		if ( ifInfo->iiAddress.Address.sa_family == AF_INET )
		{
			struct sockaddr_in *		sa4;
			
			sa4 = &ifInfo->iiAddress.AddressIn;
			ifa->ifa_addr = (struct sockaddr *) calloc( 1, sizeof( *sa4 ) );
			require_action( ifa->ifa_addr, exit, err = WSAENOBUFS );
			memcpy( ifa->ifa_addr, sa4, sizeof( *sa4 ) );

			ifa->ifa_netmask = (struct sockaddr*) calloc(1, sizeof( *sa4 ) );
			require_action( ifa->ifa_netmask, exit, err = WSAENOBUFS );

			// <rdar://problem/4076478> Service won't start on Win2K. The address
			// family field was not being initialized.

			ifa->ifa_netmask->sa_family = AF_INET;
			( ( struct sockaddr_in* ) ifa->ifa_netmask )->sin_addr = netmask.sin_addr;
			ifa->ifa_extra.index = ifIndex;
		}
		else
		{
			// Emulate an interface index.
		
			ifa->ifa_extra.index = (uint32_t)( i + 1 );
		}

		// Now get DHCP configuration information

		for ( pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next )
		{
			if ( strcmp( inet_ntoa( ifInfo->iiAddress.AddressIn.sin_addr ), pAdapter->IpAddressList.IpAddress.String ) == 0 )
			{
				ifa->ifa_dhcpEnabled		= pAdapter->DhcpEnabled;
				ifa->ifa_dhcpLeaseExpires	= pAdapter->LeaseExpires;
				ifa->ifa_womp				= IsWOMPEnabled( pAdapter->AdapterName );
				break;
			}
		}
	}
	
	// Success!
	
	if( outAddrs )
	{
		*outAddrs = head;
		head = NULL;
	}
	err = 0;
	
exit:

	if ( pAdapterInfo )
	{
		free( pAdapterInfo );
	}
	if( head )
	{
		freeifaddrs( head );
	}
	if( buffer )
	{
		free( buffer );
	}
	if( sock != INVALID_SOCKET )
	{
		closesocket( sock );
	}
	return( err );
}
#endif	// !TARGET_OS_WINDOWS_CE )

#if( TARGET_OS_WINDOWS_CE )
//===========================================================================================================================
//	getifaddrs_ce
//===========================================================================================================================

mDNSlocal int	getifaddrs_ce( struct ifaddrs **outAddrs )
{
	int							err;
	SocketRef					sock;
	DWORD						size;
	void *						buffer;
	SOCKET_ADDRESS_LIST *		addressList;
	struct ifaddrs *			head;
	struct ifaddrs **			next;
	struct ifaddrs *			ifa;
	int							n;
	int							i;

	sock 	= kInvalidSocketRef;
	buffer	= NULL;
	head	= NULL;
	next	= &head;
	
	// Open a temporary socket because one is needed to use WSAIoctl (we'll close it before exiting this function).
	
	sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	err = translate_errno( IsValidSocket( sock ), errno_compat(), kUnknownErr );
	require_noerr( err, exit );
		
	// Call WSAIoctl with SIO_ADDRESS_LIST_QUERY and pass a null buffer. This call will fail, but the size needed to 
	// for the request will be filled in. Once we know the size, allocate a buffer to hold the entire list.
	//
	// Note: Due to a bug in Windows CE, the size returned by WSAIoctl is not enough so double it as a workaround.
	
	size = 0;
	WSAIoctl( sock, SIO_ADDRESS_LIST_QUERY, NULL, 0, NULL, 0, &size, NULL, NULL );
	require_action( size > 0, exit, err = -1 );
	size *= 2;
	
	buffer = calloc( 1, size );
	require_action( buffer, exit, err = -1 );
	
	// We now know the size of the list and have a buffer to hold so call WSAIoctl again to get it.
	
	err = WSAIoctl( sock, SIO_ADDRESS_LIST_QUERY, NULL, 0, buffer, size, &size, NULL, NULL );
	require_noerr( err, exit );
	addressList = (SOCKET_ADDRESS_LIST *) buffer;
	
	// Process the raw interface list and build a linked list of interfaces.
	//
	// Note: Due to a bug in Windows CE, the iAddressCount field is always 0 so use 1 in that case.
	
	n = addressList->iAddressCount;
	if( n == 0 )
	{
		n = 1;
	}
	for( i = 0; i < n; ++i )
	{
		ifa = (struct ifaddrs *) calloc( 1, sizeof( struct ifaddrs ) );
		require_action( ifa, exit, err = WSAENOBUFS );
		
		*next = ifa;
		next  = &ifa->ifa_next;
		
		// Get the name.
		
		ifa->ifa_name = (char *) malloc( 16 );
		require_action( ifa->ifa_name, exit, err = WSAENOBUFS );
		sprintf( ifa->ifa_name, "%d", i + 1 );
		
		// Get flags. Note: SIO_ADDRESS_LIST_QUERY does not report flags so just fake IFF_UP and IFF_MULTICAST.
		
		ifa->ifa_flags = IFF_UP | IFF_MULTICAST;
		
		// Get addresses.
		
		switch( addressList->Address[ i ].lpSockaddr->sa_family )
		{
			case AF_INET:
			{
				struct sockaddr_in *		sa4;
				
				sa4 = (struct sockaddr_in *) addressList->Address[ i ].lpSockaddr;
				ifa->ifa_addr = (struct sockaddr *) calloc( 1, sizeof( *sa4 ) );
				require_action( ifa->ifa_addr, exit, err = WSAENOBUFS );
				memcpy( ifa->ifa_addr, sa4, sizeof( *sa4 ) );
				break;
			}
			
			default:
				break;
		}
	}
	
	// Success!
	
	if( outAddrs )
	{
		*outAddrs = head;
		head = NULL;
	}
	err = 0;
	
exit:
	if( head )
	{
		freeifaddrs( head );
	}
	if( buffer )
	{
		free( buffer );
	}
	if( sock != INVALID_SOCKET )
	{
		closesocket( sock );
	}
	return( err );
}
#endif	// TARGET_OS_WINDOWS_CE )

//===========================================================================================================================
//	freeifaddrs
//===========================================================================================================================

mDNSlocal void	freeifaddrs( struct ifaddrs *inIFAs )
{
	struct ifaddrs *		p;
	struct ifaddrs *		q;
	
	// Free each piece of the structure. Set to null after freeing to handle macro-aliased fields.
	
	for( p = inIFAs; p; p = q )
	{
		q = p->ifa_next;
		
		if( p->ifa_name )
		{
			free( p->ifa_name );
			p->ifa_name = NULL;
		}
		if( p->ifa_addr )
		{
			free( p->ifa_addr );
			p->ifa_addr = NULL;
		}
		if( p->ifa_netmask )
		{
			free( p->ifa_netmask );
			p->ifa_netmask = NULL;
		}
		if( p->ifa_broadaddr )
		{
			free( p->ifa_broadaddr );
			p->ifa_broadaddr = NULL;
		}
		if( p->ifa_dstaddr )
		{
			free( p->ifa_dstaddr );
			p->ifa_dstaddr = NULL;
		}
		if( p->ifa_data )
		{
			free( p->ifa_data );
			p->ifa_data = NULL;
		}
		free( p );
	}
}


//===========================================================================================================================
//	GetPrimaryInterface
//===========================================================================================================================

mDNSlocal DWORD
GetPrimaryInterface()
{
	PMIB_IPFORWARDTABLE	pIpForwardTable	= NULL;
	DWORD				dwSize			= 0;
	BOOL				bOrder			= FALSE;
	OSStatus			err;
	DWORD				index			= 0;
	DWORD				metric			= 0;
	unsigned long int	i;

	// Find out how big our buffer needs to be.

	err = GetIpForwardTable(NULL, &dwSize, bOrder);
	require_action( err == ERROR_INSUFFICIENT_BUFFER, exit, err = kUnknownErr );

	// Allocate the memory for the table

	pIpForwardTable = (PMIB_IPFORWARDTABLE) malloc( dwSize );
	require_action( pIpForwardTable, exit, err = kNoMemoryErr );
  
	// Now get the table.

	err = GetIpForwardTable(pIpForwardTable, &dwSize, bOrder);
	require_noerr( err, exit );


	// Search for the row in the table we want.

	for ( i = 0; i < pIpForwardTable->dwNumEntries; i++)
	{
		// Look for a default route

		if ( pIpForwardTable->table[i].dwForwardDest == 0 )
		{
			if ( index && ( pIpForwardTable->table[i].dwForwardMetric1 >= metric ) )
			{
				continue;
			}

			index	= pIpForwardTable->table[i].dwForwardIfIndex;
			metric	= pIpForwardTable->table[i].dwForwardMetric1;
		}
	}

exit:

	if ( pIpForwardTable != NULL )
	{
		free( pIpForwardTable );
	}

	return index;
}


//===========================================================================================================================
//	AddressToIndexAndMask
//===========================================================================================================================

mDNSlocal mStatus
AddressToIndexAndMask( struct sockaddr * addr, uint32_t * ifIndex, struct sockaddr * mask  )
{
	// Before calling AddIPAddress we use GetIpAddrTable to get
	// an adapter to which we can add the IP.
	
	PMIB_IPADDRTABLE	pIPAddrTable	= NULL;
	DWORD				dwSize			= 0;
	mStatus				err				= mStatus_UnknownErr;
	DWORD				i;

	// For now, this is only for IPv4 addresses.  That is why we can safely cast
	// addr's to sockaddr_in.

	require_action( addr->sa_family == AF_INET, exit, err = mStatus_UnknownErr );

	// Make an initial call to GetIpAddrTable to get the
	// necessary size into the dwSize variable

	for ( i = 0; i < 100; i++ )
	{
		err = GetIpAddrTable( pIPAddrTable, &dwSize, 0 );

		if ( err != ERROR_INSUFFICIENT_BUFFER )
		{
			break;
		}

		pIPAddrTable = (MIB_IPADDRTABLE *) realloc( pIPAddrTable, dwSize );
		require_action( pIPAddrTable, exit, err = WSAENOBUFS );
	}

	require_noerr( err, exit );
	err = mStatus_UnknownErr;

	for ( i = 0; i < pIPAddrTable->dwNumEntries; i++ )
	{
		if ( ( ( struct sockaddr_in* ) addr )->sin_addr.s_addr == pIPAddrTable->table[i].dwAddr )
		{
			*ifIndex											= pIPAddrTable->table[i].dwIndex;
			( ( struct sockaddr_in*) mask )->sin_addr.s_addr	= pIPAddrTable->table[i].dwMask;
			err													= mStatus_NoError;
			break;
		}
	}

exit:

	if ( pIPAddrTable )
	{
		free( pIPAddrTable );
	}

	return err;
}


//===========================================================================================================================
//	CanReceiveUnicast
//===========================================================================================================================

mDNSlocal mDNSBool	CanReceiveUnicast( void )
{
	mDNSBool				ok;
	SocketRef				sock;
	struct sockaddr_in		addr;
	
	// Try to bind to the port without the SO_REUSEADDR option to test if someone else has already bound to it.
	
	sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	check_translated_errno( IsValidSocket( sock ), errno_compat(), kUnknownErr );
	ok = IsValidSocket( sock );
	if( ok )
	{
		mDNSPlatformMemZero( &addr, sizeof( addr ) );
		addr.sin_family			= AF_INET;
		addr.sin_port			= MulticastDNSPort.NotAnInteger;
		addr.sin_addr.s_addr	= htonl( INADDR_ANY );
		
		ok = ( bind( sock, (struct sockaddr *) &addr, sizeof( addr ) ) == 0 );
		close_compat( sock );
	}
	
	dlog( kDebugLevelInfo, DEBUG_NAME "Unicast UDP responses %s\n", ok ? "okay" : "*not allowed*" );
	return( ok );
}


//===========================================================================================================================
//	IsPointToPoint
//===========================================================================================================================

mDNSlocal mDNSBool IsPointToPoint( IP_ADAPTER_UNICAST_ADDRESS * addr )
{
	struct ifaddrs	*	addrs	=	NULL;
	struct ifaddrs	*	p		=	NULL;
	OSStatus			err;
	mDNSBool			ret		=	mDNSfalse;

	// For now, only works for IPv4 interfaces

	if ( addr->Address.lpSockaddr->sa_family == AF_INET )
	{
		// The getifaddrs_ipv4 call will give us correct information regarding IFF_POINTTOPOINT flags.

		err = getifaddrs_ipv4( &addrs );
		require_noerr( err, exit );

		for ( p = addrs; p; p = p->ifa_next )
		{
			if ( ( addr->Address.lpSockaddr->sa_family == p->ifa_addr->sa_family ) &&
			     ( ( ( struct sockaddr_in* ) addr->Address.lpSockaddr )->sin_addr.s_addr == ( ( struct sockaddr_in* ) p->ifa_addr )->sin_addr.s_addr ) )
			{
				ret = ( p->ifa_flags & IFF_POINTTOPOINT ) ? mDNStrue : mDNSfalse;
				break;
			}
		}
	}

exit:

	if ( addrs )
	{
		freeifaddrs( addrs );
	}

	return ret;
}


//===========================================================================================================================
//	GetWindowsVersionString
//===========================================================================================================================

mDNSlocal OSStatus	GetWindowsVersionString( char *inBuffer, size_t inBufferSize )
{
#if( !defined( VER_PLATFORM_WIN32_CE ) )
	#define VER_PLATFORM_WIN32_CE		3
#endif

	OSStatus				err;
	OSVERSIONINFO			osInfo;
	BOOL					ok;
	const char *			versionString;
	DWORD					platformID;
	DWORD					majorVersion;
	DWORD					minorVersion;
	DWORD					buildNumber;
	
	versionString = "unknown Windows version";
	
	osInfo.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
	ok = GetVersionEx( &osInfo );
	err = translate_errno( ok, (OSStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );
	
	platformID		= osInfo.dwPlatformId;
	majorVersion	= osInfo.dwMajorVersion;
	minorVersion	= osInfo.dwMinorVersion;
	buildNumber		= osInfo.dwBuildNumber & 0xFFFF;
	
	if( ( platformID == VER_PLATFORM_WIN32_WINDOWS ) && ( majorVersion == 4 ) )
	{
		if( ( minorVersion < 10 ) && ( buildNumber == 950 ) )
		{
			versionString	= "Windows 95";
		}
		else if( ( minorVersion < 10 ) && ( ( buildNumber > 950 ) && ( buildNumber <= 1080 ) ) )
		{
			versionString	= "Windows 95 SP1";
		}
		else if( ( minorVersion < 10 ) && ( buildNumber > 1080 ) )
		{
			versionString	= "Windows 95 OSR2";
		}
		else if( ( minorVersion == 10 ) && ( buildNumber == 1998 ) )
		{
			versionString	= "Windows 98";
		}
		else if( ( minorVersion == 10 ) && ( ( buildNumber > 1998 ) && ( buildNumber < 2183 ) ) )
		{
			versionString	= "Windows 98 SP1";
		}
		else if( ( minorVersion == 10 ) && ( buildNumber >= 2183 ) )
		{
			versionString	= "Windows 98 SE";
		}
		else if( minorVersion == 90 )
		{
			versionString	= "Windows ME";
		}
	}
	else if( platformID == VER_PLATFORM_WIN32_NT )
	{
		if( ( majorVersion == 3 ) && ( minorVersion == 51 ) )
		{
			versionString	= "Windows NT 3.51";
		}
		else if( ( majorVersion == 4 ) && ( minorVersion == 0 ) )
		{
			versionString	= "Windows NT 4";
		}
		else if( ( majorVersion == 5 ) && ( minorVersion == 0 ) )
		{
			versionString	= "Windows 2000";
		}
		else if( ( majorVersion == 5 ) && ( minorVersion == 1 ) )
		{
			versionString	= "Windows XP";
		}
		else if( ( majorVersion == 5 ) && ( minorVersion == 2 ) )
		{
			versionString	= "Windows Server 2003";
		}
	}
	else if( platformID == VER_PLATFORM_WIN32_CE )
	{
		versionString		= "Windows CE";
	}
	
exit:
	if( inBuffer && ( inBufferSize > 0 ) )
	{
		inBufferSize -= 1;
		strncpy( inBuffer, versionString, inBufferSize );
		inBuffer[ inBufferSize ] = '\0';
	}
	return( err );
}


//===========================================================================================================================
//	RegQueryString
//===========================================================================================================================

mDNSlocal mStatus
RegQueryString( HKEY key, LPCSTR valueName, LPSTR * string, DWORD * stringLen, DWORD * enabled )
{
	DWORD	type;
	int		i;
	mStatus err;

	*stringLen	= MAX_ESCAPED_DOMAIN_NAME;
	*string		= NULL;
	i			= 0;

	do
	{
		if ( *string )
		{
			free( *string );
		}

		*string = (char*) malloc( *stringLen );
		require_action( *string, exit, err = mStatus_NoMemoryErr );

		err = RegQueryValueExA( key, valueName, 0, &type, (LPBYTE) *string, stringLen );

		i++;
	}
	while ( ( err == ERROR_MORE_DATA ) && ( i < 100 ) );

	require_noerr_quiet( err, exit );

	if ( enabled )
	{
		DWORD dwSize = sizeof( DWORD );

		err = RegQueryValueEx( key, TEXT("Enabled"), NULL, NULL, (LPBYTE) enabled, &dwSize );
		check_noerr( err );

		err = kNoErr;
	}

exit:

	return err;
}


//===========================================================================================================================
//	StringToAddress
//===========================================================================================================================

mDNSlocal mStatus StringToAddress( mDNSAddr * ip, LPSTR string )
{
	struct sockaddr_in6 sa6;
	struct sockaddr_in	sa4;
	INT					dwSize;
	mStatus				err;

	sa6.sin6_family	= AF_INET6;
	dwSize			= sizeof( sa6 );

	err = WSAStringToAddressA( string, AF_INET6, NULL, (struct sockaddr*) &sa6, &dwSize );

	if ( err == mStatus_NoError )
	{
		err = SetupAddr( ip, (struct sockaddr*) &sa6 );
		require_noerr( err, exit );
	}
	else
	{
		sa4.sin_family = AF_INET;
		dwSize = sizeof( sa4 );

		err = WSAStringToAddressA( string, AF_INET, NULL, (struct sockaddr*) &sa4, &dwSize );
		err = translate_errno( err == 0, WSAGetLastError(), kUnknownErr );
		require_noerr( err, exit );
			
		err = SetupAddr( ip, (struct sockaddr*) &sa4 );
		require_noerr( err, exit );
	}

exit:

	return err;
}


//===========================================================================================================================
//	myGetIfAddrs
//===========================================================================================================================

mDNSlocal struct ifaddrs*
myGetIfAddrs(int refresh)
{
	static struct ifaddrs *ifa = NULL;
	
	if (refresh && ifa)
	{
		freeifaddrs(ifa);
		ifa = NULL;
	}
	
	if (ifa == NULL)
	{
		getifaddrs(&ifa);
	}
	
	return ifa;
}


//===========================================================================================================================
//	TCHARtoUTF8
//===========================================================================================================================

mDNSlocal OSStatus
TCHARtoUTF8( const TCHAR *inString, char *inBuffer, size_t inBufferSize )
{
#if( defined( UNICODE ) || defined( _UNICODE ) )
	OSStatus		err;
	int				len;
	
	len = WideCharToMultiByte( CP_UTF8, 0, inString, -1, inBuffer, (int) inBufferSize, NULL, NULL );
	err = translate_errno( len > 0, errno_compat(), kUnknownErr );
	require_noerr( err, exit );
	
exit:
	return( err );
#else
	return( WindowsLatin1toUTF8( inString, inBuffer, inBufferSize ) );
#endif
}


//===========================================================================================================================
//	WindowsLatin1toUTF8
//===========================================================================================================================

mDNSlocal OSStatus
WindowsLatin1toUTF8( const char *inString, char *inBuffer, size_t inBufferSize )
{
	OSStatus		err;
	WCHAR *			utf16;
	int				len;
	
	utf16 = NULL;
	
	// Windows doesn't support going directly from Latin-1 to UTF-8 so we have to go from Latin-1 to UTF-16 first.
	
	len = MultiByteToWideChar( CP_ACP, 0, inString, -1, NULL, 0 );
	err = translate_errno( len > 0, errno_compat(), kUnknownErr );
	require_noerr( err, exit );
	
	utf16 = (WCHAR *) malloc( len * sizeof( *utf16 ) );
	require_action( utf16, exit, err = kNoMemoryErr );
	
	len = MultiByteToWideChar( CP_ACP, 0, inString, -1, utf16, len );
	err = translate_errno( len > 0, errno_compat(), kUnknownErr );
	require_noerr( err, exit );
	
	// Now convert the temporary UTF-16 to UTF-8.
	
	len = WideCharToMultiByte( CP_UTF8, 0, utf16, -1, inBuffer, (int) inBufferSize, NULL, NULL );
	err = translate_errno( len > 0, errno_compat(), kUnknownErr );
	require_noerr( err, exit );

exit:
	if( utf16 ) free( utf16 );
	return( err );
}


//===========================================================================================================================
//	FreeTCPConnectionData
//===========================================================================================================================

mDNSlocal void
FreeTCPSocket( TCPSocket *sock )
{
	check( sock );

	if ( sock->pendingEvent )
	{
		CloseHandle( sock->pendingEvent );
	}

	if ( sock->fd != INVALID_SOCKET )
	{
		closesocket( sock->fd );
	}

	free( sock );
}


//===========================================================================================================================
//  FreeUDPSocket
//===========================================================================================================================

mDNSlocal void
FreeUDPSocket( UDPSocket * sock )
{
    check( sock );

    if ( sock->readEvent )
    {
        CloseHandle( sock->readEvent );
    }

    if ( sock->sock != INVALID_SOCKET )
    {
        closesocket( sock->sock );
    }

    free( sock );
}

//===========================================================================================================================
//	SetupAddr
//===========================================================================================================================

mDNSlocal mStatus SetupAddr(mDNSAddr *ip, const struct sockaddr *const sa)
	{
	if (!sa) { LogMsg("SetupAddr ERROR: NULL sockaddr"); return(mStatus_Invalid); }

	if (sa->sa_family == AF_INET)
		{
		struct sockaddr_in *ifa_addr = (struct sockaddr_in *)sa;
		ip->type = mDNSAddrType_IPv4;
		ip->ip.v4.NotAnInteger = ifa_addr->sin_addr.s_addr;
		return(mStatus_NoError);
		}

	if (sa->sa_family == AF_INET6)
		{
		struct sockaddr_in6 *ifa_addr = (struct sockaddr_in6 *)sa;
		ip->type = mDNSAddrType_IPv6;
		if (IN6_IS_ADDR_LINKLOCAL(&ifa_addr->sin6_addr)) ifa_addr->sin6_addr.u.Word[1] = 0;
		ip->ip.v6 = *(mDNSv6Addr*)&ifa_addr->sin6_addr;
		return(mStatus_NoError);
		}

	LogMsg("SetupAddr invalid sa_family %d", sa->sa_family);
	return(mStatus_Invalid);
	}


mDNSlocal void GetDDNSFQDN( domainname *const fqdn )
{
	LPSTR		name = NULL;
	DWORD		dwSize;
	DWORD		enabled;
	HKEY		key = NULL;
	OSStatus	err;

	check( fqdn );

	// Initialize

	fqdn->c[0] = '\0';

	// Get info from Bonjour registry key

	err = RegCreateKey( HKEY_LOCAL_MACHINE, kServiceParametersNode TEXT("\\DynDNS\\Setup\\") kServiceDynDNSHostNames, &key );
	require_noerr( err, exit );

	err = RegQueryString( key, "", &name, &dwSize, &enabled );
	if ( !err && ( name[0] != '\0' ) && enabled )
	{
		if ( !MakeDomainNameFromDNSNameString( fqdn, name ) || !fqdn->c[0] )
		{
			dlog( kDebugLevelError, "bad DDNS host name in registry: %s", name[0] ? name : "(unknown)");
		}
	}

exit:

	if ( key )
	{
		RegCloseKey( key );
		key = NULL;
	}

	if ( name )
	{
		free( name );
		name = NULL;
	}
}


#ifdef UNICODE
mDNSlocal void GetDDNSDomains( DNameListElem ** domains, LPCWSTR lpSubKey )
#else
mDNSlocal void GetDDNSConfig( DNameListElem ** domains, LPCSTR lpSubKey )
#endif
{
	char		subKeyName[kRegistryMaxKeyLength + 1];
	DWORD		cSubKeys = 0;
	DWORD		cbMaxSubKey;
	DWORD		cchMaxClass;
	DWORD		dwSize;
	HKEY		key = NULL;
	HKEY		subKey = NULL;
	domainname	dname;
	DWORD		i;
	OSStatus	err;

	check( domains );

	// Initialize

	*domains = NULL;

	err = RegCreateKey( HKEY_LOCAL_MACHINE, lpSubKey, &key );
	require_noerr( err, exit );

	// Get information about this node

	err = RegQueryInfoKey( key, NULL, NULL, NULL, &cSubKeys, &cbMaxSubKey, &cchMaxClass, NULL, NULL, NULL, NULL, NULL );       
	require_noerr( err, exit );

	for ( i = 0; i < cSubKeys; i++)
	{
		DWORD enabled;

		dwSize = kRegistryMaxKeyLength;
        
		err = RegEnumKeyExA( key, i, subKeyName, &dwSize, NULL, NULL, NULL, NULL );

		if ( !err )
		{
			err = RegOpenKeyExA( key, subKeyName, 0, KEY_READ, &subKey );
			require_noerr( err, exit );

			dwSize = sizeof( DWORD );
			err = RegQueryValueExA( subKey, "Enabled", NULL, NULL, (LPBYTE) &enabled, &dwSize );

			if ( !err && ( subKeyName[0] != '\0' ) && enabled )
			{
				if ( !MakeDomainNameFromDNSNameString( &dname, subKeyName ) || !dname.c[0] )
				{
					dlog( kDebugLevelError, "bad DDNS domain in registry: %s", subKeyName[0] ? subKeyName : "(unknown)");
				}
				else
				{
					DNameListElem * domain = (DNameListElem*) malloc( sizeof( DNameListElem ) );
					require_action( domain, exit, err = mStatus_NoMemoryErr );
					
					AssignDomainName(&domain->name, &dname);
					domain->next = *domains;

					*domains = domain;
				}
			}

			RegCloseKey( subKey );
			subKey = NULL;
		}
	}

exit:

	if ( subKey )
	{
		RegCloseKey( subKey );
	}

	if ( key )
	{
		RegCloseKey( key );
	}
}


mDNSlocal void SetDomainSecret( mDNS * const m, const domainname * inDomain )
{
	char					domainUTF8[ 256 ];
	DomainAuthInfo			*foundInList;
	DomainAuthInfo			*ptr;
	char					outDomain[ 256 ];
	char					outKey[ 256 ];
	char					outSecret[ 256 ];
	OSStatus				err;
	
	ConvertDomainNameToCString( inDomain, domainUTF8 );
	
	// If we're able to find a secret for this domain

	if ( LsaGetSecret( domainUTF8, outDomain, sizeof( outDomain ), outKey, sizeof( outKey ), outSecret, sizeof( outSecret ) ) )
	{
		domainname domain;
		domainname key;

		// Tell the core about this secret

		MakeDomainNameFromDNSNameString( &domain, outDomain );
		MakeDomainNameFromDNSNameString( &key, outKey );

		for (foundInList = m->AuthInfoList; foundInList; foundInList = foundInList->next)
			if (SameDomainName(&foundInList->domain, &domain ) ) break;

		ptr = foundInList;
	
		if (!ptr)
		{
			ptr = (DomainAuthInfo*)malloc(sizeof(DomainAuthInfo));
			require_action( ptr, exit, err = mStatus_NoMemoryErr );
		}

		err = mDNS_SetSecretForDomain(m, ptr, &domain, &key, outSecret, mDNSfalse );
		require_action( err != mStatus_BadParamErr, exit, if (!foundInList ) mDNSPlatformMemFree( ptr ) );

		debugf("Setting shared secret for zone %s with key %##s", outDomain, key.c);
	}

exit:

	return;
}


mDNSlocal void
CheckFileShares( mDNS * const m )
{
	PSHARE_INFO_1	bufPtr = ( PSHARE_INFO_1 ) NULL;
	DWORD			entriesRead = 0;
	DWORD			totalEntries = 0;
	DWORD			resume = 0;
	mDNSBool		enabled = mDNSfalse;
	NET_API_STATUS  res;
	mStatus			err;

	check( m );

	if ( mDNSIsFileAndPrintSharingEnabled() )
	{
		dlog( kDebugLevelTrace, DEBUG_NAME "file and print sharing is enabled\n" );

		res = NetShareEnum( NULL, 1, ( LPBYTE* )&bufPtr, MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, &resume );

		if ( ( res == ERROR_SUCCESS ) || ( res == ERROR_MORE_DATA ) )
		{
			PSHARE_INFO_1 p = bufPtr;
			DWORD i;

			for( i = 0; i <= totalEntries; i++ ) 
			{
				// We are only interested if the user is sharing anything other 
				// than the built-in "print$" source

				if ( ( p->shi1_type == STYPE_DISKTREE ) && ( wcscmp( p->shi1_netname, TEXT( "print$" ) ) != 0 ) )
				{
					enabled = mDNStrue;
					break;
				}

				p++;
			}

			NetApiBufferFree( bufPtr );
			bufPtr = NULL;
		}
	}

	if ( enabled && !m->p->smbRegistered )
	{
		domainname		type;
		domainname		domain;
		mDNSIPPort		port = { { SMBPortAsNumber >> 8, SMBPortAsNumber & 0xFF } };
		mDNSInterfaceID iid = mDNSPlatformInterfaceIDfromInterfaceIndex( m, 0 );

		dlog( kDebugLevelTrace, DEBUG_NAME "registering smb type\n" );
		
		MakeDomainNameFromDNSNameString(&type, "_smb._tcp" );
		MakeDomainNameFromDNSNameString(&domain, "local.");

		err = mDNS_RegisterService( m, &m->p->smbSRS, &m->nicelabel, &type, &domain, NULL, port, NULL, 0, NULL, 0, iid, /* callback */ NULL, /* context */ NULL);
		require_noerr( err, exit );

		m->p->smbRegistered = mDNStrue;
	}
	else if ( !enabled && m->p->smbRegistered )
	{
		dlog( kDebugLevelTrace, DEBUG_NAME "deregistering smb type\n" );
		
		err = mDNS_DeregisterService( m, &m->p->smbSRS );
		require_noerr( err, exit );

		m->p->smbRegistered = mDNSfalse;
	}

exit:

	return;
}


void
UpdateWOMPConfig( mDNS * const m )
{
	mDNSInterfaceData * ifd;

	mDNSPlatformLock( m );

	m->p->womp = mDNSfalse;

	for( ifd = m->p->interfaceList; ifd; ifd = ifd->next )
	{
		ifd->interfaceInfo.NetWake = IsWOMPEnabled( ifd->name );

		if ( ifd->interfaceInfo.NetWake )
		{
			m->p->womp = mDNStrue;
		}
	}

	mDNSPlatformUnlock( m );
}


mDNSlocal mDNSu8
IsWOMPEnabled( const char * adapterName )
{
	char						fileName[80];
	NDIS_OID					oid;
    DWORD						count;
    HANDLE						handle	= INVALID_HANDLE_VALUE;
	NDIS_PNP_CAPABILITIES	*	pNPC	= NULL;
	int							err;
	mDNSu8						ok		= TRUE;
	
    // Construct a device name to pass to CreateFile

	strncpy_s( fileName, sizeof( fileName ), DEVICE_PREFIX, strlen( DEVICE_PREFIX ) );
	strcat_s( fileName, sizeof( fileName ), adapterName );
    handle = CreateFileA( fileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, INVALID_HANDLE_VALUE );
	require_action ( handle != INVALID_HANDLE_VALUE, exit, ok = FALSE );

	// We successfully opened the driver, format the IOCTL to pass the driver.
		
	oid = OID_PNP_CAPABILITIES;
	pNPC = ( NDIS_PNP_CAPABILITIES * ) malloc( sizeof( NDIS_PNP_CAPABILITIES ) );
	require_action( pNPC != NULL, exit, ok = FALSE );
	ok = ( mDNSu8 ) DeviceIoControl( handle, IOCTL_NDIS_QUERY_GLOBAL_STATS, &oid, sizeof( oid ), pNPC, sizeof( NDIS_PNP_CAPABILITIES ), &count, NULL );
	err = translate_errno( ok, GetLastError(), kUnknownErr );
	require_action( !err, exit, ok = FALSE );
	ok = ( mDNSu8 ) ( ( count == sizeof( NDIS_PNP_CAPABILITIES ) ) && ( pNPC->Flags & NDIS_DEVICE_WAKE_ON_MAGIC_PACKET_ENABLE ) );
       
exit:

	if ( pNPC != NULL )
	{
		free( pNPC );
	}

    if ( handle != INVALID_HANDLE_VALUE )
    {
		CloseHandle( handle );
    }

	return ( mDNSu8 ) ok;
}
