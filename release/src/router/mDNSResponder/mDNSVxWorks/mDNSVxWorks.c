/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
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

$Log: mDNSVxWorks.c,v $
Revision 1.35  2009/01/13 05:31:35  mkrochma
<rdar://problem/6491367> Replace bzero, bcopy with mDNSPlatformMemZero, mDNSPlatformMemCopy, memset, memcpy

Revision 1.34  2008/10/03 18:25:18  cheshire
Instead of calling "m->MainCallback" function pointer directly, call mDNSCore routine "mDNS_ConfigChanged(m);"

Revision 1.33  2007/03/22 18:31:48  cheshire
Put dst parameter first in mDNSPlatformStrCopy/mDNSPlatformMemCopy, like conventional Posix strcpy/memcpy

Revision 1.32  2006/12/19 22:43:56  cheshire
Fix compiler warnings

Revision 1.31  2006/11/10 00:54:16  cheshire
<rdar://problem/4816598> Changing case of Computer Name doesn't work

Revision 1.30  2006/08/14 23:25:18  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.29  2006/03/19 02:00:12  cheshire
<rdar://problem/4073825> Improve logic for delaying packets after repeated interface transitions

Revision 1.28  2005/05/30 07:36:38  bradley
New implementation of the mDNS platform plugin for VxWorks 5.5 or later with IPv6 support.

*/

#if 0
#pragma mark == Configuration ==
#endif

//===========================================================================================================================
//	Configuration
//===========================================================================================================================

#define	DEBUG_NAME							"[mDNS] "
#define	MDNS_AAAA_OVER_IPV4					1	// 1=Send AAAA & A records over IPv4 & IPv6, 0=Send AAAA over IPv6, A over IPv4.
#define	MDNS_EXCLUDE_IPV4_ROUTABLE_IPV6		1	// 1=Don't use IPv6 socket if non-link-local IPv4 available on same interface.
#define	MDNS_ENABLE_PPP						0	// 1=Enable Unicast DNS over PPP interfaces. 0=Don't enable it.
#define MDNS_DEBUG_PACKETS					1	// 1=Enable debug output for packet send/recv if debug level high enough.
#define MDNS_DEBUG_SHOW						1	// 1=Enable console show routines.
#define	DEBUG_USE_DEFAULT_CATEGORY			1	// Set up to use the default category (see DebugServices.h for details).

#include	<stdarg.h>
#include	<stddef.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<time.h>

#include	"vxWorks.h"
#include	"config.h"

#include	<sys/types.h>

#include	<arpa/inet.h>
#include	<net/if.h>
#include	<net/if_dl.h>
#include	<net/if_types.h>
#include	<net/ifaddrs.h>
#include	<netinet6/in6_var.h>
#include	<netinet/if_ether.h>
#include	<netinet/in.h>
#include	<netinet/ip.h>
#include	<sys/ioctl.h>
#include	<sys/socket.h>
#include	<unistd.h>

#include	"ifLib.h"
#include	"inetLib.h"
#include	"pipeDrv.h"
#include	"selectLib.h"
#include	"semLib.h"
#include	"sockLib.h"
#include	"sysLib.h"
#include	"taskLib.h"
#include	"tickLib.h"

#include	"CommonServices.h"
#include	"DebugServices.h"
#include	"DNSCommon.h"
#include	"mDNSEmbeddedAPI.h"

#include	"mDNSVxWorks.h"

#if 0
#pragma mark == Constants ==
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

typedef uint8_t		MDNSPipeCommandCode;

#define kMDNSPipeCommandCodeInvalid			0
#define kMDNSPipeCommandCodeReschedule		1
#define kMDNSPipeCommandCodeReconfigure		2
#define kMDNSPipeCommandCodeQuit			3

#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

#if( DEBUG )
	mDNSlocal void	DebugMsg( DebugLevel inLevel, const char *inFormat, ... );
	
	#define dmsg( LEVEL, ARGS... )	DebugMsg( LEVEL, ## ARGS )
#else
	#define dmsg( LEVEL, ARGS... )
#endif

#if( DEBUG && MDNS_DEBUG_PACKETS )
	#define dpkt( LEVEL, ARGS... )	DebugMsg( LEVEL, ## ARGS )
#else
	#define dpkt( LEVEL, ARGS... )
#endif

#define	ForgetSem( X )		do { if( *( X ) ) { semDelete( ( *X ) ); *( X ) = 0; } } while( 0 )
#define	ForgetSocket( X )	do { if( IsValidSocket( *( X ) ) ) { close_compat( *( X ) ); *( X ) = kInvalidSocketRef; } } while( 0 )

// Interfaces

mDNSlocal mStatus	UpdateInterfaceList( mDNS *const inMDNS, mDNSs32 inUTC );
mDNSlocal NetworkInterfaceInfoVxWorks *	AddInterfaceToList( mDNS *const inMDNS, struct ifaddrs *inIFA, mDNSs32 inUTC );
mDNSlocal int	SetupActiveInterfaces( mDNS *const inMDNS, mDNSs32 inUTC );
mDNSlocal void	MarkAllInterfacesInactive( mDNS *const inMDNS, mDNSs32 inUTC );
mDNSlocal int	ClearInactiveInterfaces( mDNS *const inMDNS, mDNSs32 inUTC, mDNSBool inClosing );
mDNSlocal NetworkInterfaceInfoVxWorks *	FindRoutableIPv4( mDNS *const inMDNS, mDNSu32 inScopeID );
mDNSlocal NetworkInterfaceInfoVxWorks *	FindInterfaceByIndex( mDNS *const inMDNS, int inFamily, mDNSu32 inIndex );
mDNSlocal mStatus	SetupSocket( mDNS *const inMDNS, const mDNSAddr *inAddr, mDNSBool inMcast, int inFamily, SocketSet *inSS );
mDNSlocal mStatus	SockAddrToMDNSAddr( const struct sockaddr * const inSA, mDNSAddr *outIP );

// Commands

mDNSlocal mStatus	SetupCommandPipe( mDNS * const inMDNS );
mDNSlocal mStatus	TearDownCommandPipe( mDNS * const inMDNS );
mDNSlocal mStatus	SendCommand( const mDNS * const inMDNS, MDNSPipeCommandCode inCommandCode );
mDNSlocal mStatus	ProcessCommand( mDNS * const inMDNS );

// Threads

mDNSlocal void		Task( mDNS *inMDNS );
mDNSlocal mStatus	TaskInit( mDNS *inMDNS );
mDNSlocal void		TaskTerm( mDNS *inMDNS );
mDNSlocal void		TaskSetupSelect( mDNS *inMDNS, fd_set *outSet, int *outMaxFd, mDNSs32 inNextEvent, struct timeval *outTimeout );
mDNSlocal void		TaskProcessPackets( mDNS *inMDNS, SocketSet *inSS, SocketRef inSock );
mDNSlocal ssize_t
	mDNSRecvMsg( 
		SocketRef	inSock, 
		void *		inBuffer, 
		size_t		inBufferSize, 
		void *		outFrom, 
		size_t		inFromSize, 
		size_t *	outFromSize, 
		mDNSAddr *	outDstAddr, 
		uint32_t *	outIndex );

// DNSServices compatibility. When all clients move to DNS-SD, this section can be removed.

#ifdef	__cplusplus
	extern "C" {
#endif

typedef struct mDNSPlatformInterfaceInfo	mDNSPlatformInterfaceInfo;
struct	mDNSPlatformInterfaceInfo
{
	const char *		name;
	mDNSAddr			ip;
};

mDNSexport mStatus	mDNSPlatformInterfaceNameToID( mDNS * const inMDNS, const char *inName, mDNSInterfaceID *outID );
mDNSexport mStatus	mDNSPlatformInterfaceIDToInfo( mDNS * const inMDNS, mDNSInterfaceID inID, mDNSPlatformInterfaceInfo *outInfo );

#ifdef	__cplusplus
	}
#endif

#if 0
#pragma mark == Globals ==
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

debug_log_new_default_category( mdns );

mDNSexport mDNSs32					mDNSPlatformOneSecond;
mDNSlocal mDNSs32					gMDNSTicksToMicro		= 0;
mDNSlocal mDNS *					gMDNSPtr				= NULL;
mDNSlocal mDNS_PlatformSupport		gMDNSPlatformSupport;
mDNSlocal mDNSBool					gMDNSDeferIPv4			= mDNSfalse;
#if( DEBUG )
	DebugLevel						gMDNSDebugOverrideLevel	= kDebugLevelMax;
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	mDNSReconfigure
//===========================================================================================================================

void	mDNSReconfigure( void )
{	
	if( gMDNSPtr ) SendCommand( gMDNSPtr, kMDNSPipeCommandCodeReconfigure );
}

//===========================================================================================================================
//	mDNSDeferIPv4
//===========================================================================================================================

void	mDNSDeferIPv4( mDNSBool inDefer )
{
	gMDNSDeferIPv4 = inDefer;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	mDNSPlatformInit
//===========================================================================================================================

mStatus	mDNSPlatformInit( mDNS * const inMDNS )
{
	mStatus		err;
	int			id;
	
	mDNSPlatformOneSecond 	= sysClkRateGet();
	gMDNSTicksToMicro		= ( 1000000L / mDNSPlatformOneSecond );
	
	// Do minimal initialization to get the task started and so we can cleanup safely if an error occurs.
	
	mDNSPlatformMemZero( &gMDNSPlatformSupport, sizeof( gMDNSPlatformSupport ) );
	if( !inMDNS->p ) inMDNS->p	= &gMDNSPlatformSupport;
	inMDNS->p->unicastSS.info	= NULL;
	inMDNS->p->unicastSS.sockV4	= kInvalidSocketRef;
	inMDNS->p->unicastSS.sockV6	= kInvalidSocketRef;
	inMDNS->p->initErr			= mStatus_NotInitializedErr;
	inMDNS->p->commandPipe		= ERROR;
	inMDNS->p->taskID			= ERROR;
	
	inMDNS->p->lock = semMCreate( SEM_Q_FIFO );
	require_action( inMDNS->p->lock, exit, err = mStatus_NoMemoryErr );
	
	inMDNS->p->initEvent = semBCreate( SEM_Q_FIFO, SEM_EMPTY );
	require_action( inMDNS->p->initEvent, exit, err = mStatus_NoMemoryErr );
	
	inMDNS->p->quitEvent = semBCreate( SEM_Q_FIFO, SEM_EMPTY );
	require_action( inMDNS->p->quitEvent, exit, err = mStatus_NoMemoryErr );
	
	// Start the task and wait for it to initialize. The task does the full initialization from its own context
	// to avoid potential issues with stack space and APIs that key off the current task (e.g. watchdog timers).
	// We wait here until the init is complete because it needs to be ready to use as soon as this function returns.
	
	id = taskSpawn( "tMDNS", 102, 0, 16384, (FUNCPTR) Task, (int) inMDNS, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	err = translate_errno( id != ERROR, errno_compat(), mStatus_NoMemoryErr );
	require_noerr( err, exit );
	
	err = semTake( inMDNS->p->initEvent, WAIT_FOREVER );
	if( err == OK ) err = inMDNS->p->initErr;
	require_noerr( err, exit );
	
	gMDNSPtr = inMDNS;
	mDNSCoreInitComplete( inMDNS, err );
	
exit:
	if( err ) mDNSPlatformClose( inMDNS );
	return( err );
}

//===========================================================================================================================
//	mDNSPlatformClose
//===========================================================================================================================

void	mDNSPlatformClose( mDNS * const inMDNS )
{
	mStatus		err;
	
	check( inMDNS );
	
	gMDNSPtr = NULL;
	
	// Signal the task to quit and wait for it to signal back that it exited. Timeout in 10 seconds to handle a hung thread.
	
	if( inMDNS->p->taskID != ERROR )
	{
		SendCommand( inMDNS, kMDNSPipeCommandCodeQuit );
		if( inMDNS->p->quitEvent )
		{
			err = semTake( inMDNS->p->quitEvent, sysClkRateGet() * 10 );
			check_noerr( err );
		}
		inMDNS->p->taskID = ERROR;
	}
	
	// Clean up resources set up in mDNSPlatformInit. All other resources should have been cleaned up already by TaskTerm.
	
	ForgetSem( &inMDNS->p->quitEvent );
	ForgetSem( &inMDNS->p->initEvent );
	ForgetSem( &inMDNS->p->lock );
	
	dmsg( kDebugLevelNotice, DEBUG_NAME "CLOSED\n" );
}

//===========================================================================================================================
//	mDNSPlatformSendUDP
//===========================================================================================================================

mStatus
	mDNSPlatformSendUDP( 
		const mDNS * const			inMDNS, 
		const void * const			inMsg, 
		const mDNSu8 * const		inEnd, 
		mDNSInterfaceID 			inInterfaceID, 
		const mDNSAddr *			inDstIP, 
		mDNSIPPort 					inDstPort )
{
	mStatus								err;
	NetworkInterfaceInfoVxWorks *		info;
	SocketRef							sock;
	struct sockaddr_storage				to;
	int									n;
	
	// Set up the sockaddr to sent to and the socket to send on.
	
	info = (NetworkInterfaceInfoVxWorks *) inInterfaceID;
	if( inDstIP->type == mDNSAddrType_IPv4 )
	{
		struct sockaddr_in *		sa4;
		
		sa4 = (struct sockaddr_in *) &to;
		sa4->sin_len			= sizeof( *sa4 );
		sa4->sin_family			= AF_INET;
		sa4->sin_port			= inDstPort.NotAnInteger;
		sa4->sin_addr.s_addr	= inDstIP->ip.v4.NotAnInteger;
		sock = info ? info->ss.sockV4 : inMDNS->p->unicastSS.sockV4;
	}
	else if( inDstIP->type == mDNSAddrType_IPv6 )
	{
		struct sockaddr_in6 *		sa6;
		
		sa6 = (struct sockaddr_in6 *) &to;
		sa6->sin6_len		= sizeof( *sa6 );
		sa6->sin6_family	= AF_INET6;
		sa6->sin6_port		= inDstPort.NotAnInteger;
		sa6->sin6_flowinfo	= 0;
		sa6->sin6_addr		= *( (struct in6_addr *) &inDstIP->ip.v6 );
		sa6->sin6_scope_id	= info ? info->scopeID : 0;
		sock = info ? info->ss.sockV6 : inMDNS->p->unicastSS.sockV6;
	}
	else
	{
		dmsg( kDebugLevelError, DEBUG_NAME "%s: ERROR! destination is not an IPv4 or IPv6 address\n", __ROUTINE__ );
		err = mStatus_BadParamErr;
		goto exit;
	}
	
	// Send the packet if we've got a valid socket of this type. Note: mDNSCore may ask us to send an IPv4 packet and then 
	// an IPv6 multicast packet. If we don't have the corresponding type of socket available, quietly return an error.
	
	n = (int)( (mDNSu8 *) inEnd - (mDNSu8 *) inMsg );
	if( !IsValidSocket( sock ) )
	{
		dpkt( kDebugLevelChatty - 1, 
			DEBUG_NAME "DROP: %4d bytes,                                                     DST=[%#39a]:%5hu, IF=%8s(%u) %#p\n", 
			n, inDstIP, mDNSVal16( inDstPort ), info ? info->ifinfo.ifname : "unicast", info ? info->scopeID : 0, info );
		err = mStatus_Invalid;
		goto exit;
	}
	
	dpkt( kDebugLevelChatty, 
		DEBUG_NAME "SEND %4d bytes,                                                      DST=[%#39a]:%5hu, IF=%8s(%u) %#p\n", 
		n, inDstIP, mDNSVal16( inDstPort ), info ? info->ifinfo.ifname : "unicast", info ? info->scopeID : 0, info );
	
	n = sendto( sock, (mDNSu8 *) inMsg, n, 0, (struct sockaddr *) &to, to.ss_len );
	if( n < 0 )
	{
		// Don't warn about ARP failures or no route to host for unicast destinations.
		
		err = errno_compat();
		if( ( ( err == EHOSTDOWN ) || ( err == ENETDOWN ) || ( err == EHOSTUNREACH ) ) && !mDNSAddressIsAllDNSLinkGroup( inDstIP ) )
		{
			goto exit;
		}
		
		dmsg( kDebugLevelError, "%s: ERROR! sendto failed on %8s(%u) to %#a:%d, sock %d, err %d, time %u\n",
			__ROUTINE__, info ? info->ifinfo.ifname : "unicast", info ? info->scopeID : 0, inDstIP, mDNSVal16( inDstPort ), 
			sock, err, (unsigned int) inMDNS->timenow );
		if( err == 0 ) err = mStatus_UnknownErr;
		goto exit;
	}
	err = mStatus_NoError;
	
exit:
	return( err );
}

//===========================================================================================================================
//	Connection-oriented (TCP) functions
//===========================================================================================================================

mDNSexport mStatus mDNSPlatformTCPConnect(const mDNSAddr *dst, mDNSOpaque16 dstport, mDNSInterfaceID InterfaceID,
										  TCPConnectionCallback callback, void *context, int *descriptor)
	{
	(void)dst;			// Unused
	(void)dstport;		// Unused
	(void)InterfaceID;	// Unused
	(void)callback;		// Unused
	(void)context;		// Unused
	(void)descriptor;	// Unused
	return(mStatus_UnsupportedErr);
	}

mDNSexport void mDNSPlatformTCPCloseConnection(int sd)
	{
	(void)sd;			// Unused
	}

mDNSexport long mDNSPlatformReadTCP(int sd, void *buf, unsigned long buflen)
	{
	(void)sd;			// Unused
	(void)buf;			// Unused
	(void)buflen;		// Unused
	return(0);
	}

mDNSexport long mDNSPlatformWriteTCP(int sd, const char *msg, unsigned long len)
	{
	(void)sd;			// Unused
	(void)msg;			// Unused
	(void)len;			// Unused
	return(0);
	}

//===========================================================================================================================
//	mDNSPlatformLock
//===========================================================================================================================

void	mDNSPlatformLock( const mDNS * const inMDNS )
{
	check_string( inMDNS->p && ( inMDNS->p->taskID != ERROR ), "mDNS task not started" );
	
#if( DEBUG )
	if( semTake( inMDNS->p->lock, 60 * sysClkRateGet() ) != OK )
	{
		dmsg( kDebugLevelTragic, "\n### DEADLOCK DETECTED ### (sem=%#p, task=%#p)\n\n", inMDNS->p->lock, taskIdSelf() );
		debug_stack_trace();			// 1) Print Stack Trace.
		semShow( inMDNS->p->lock, 1 );	// 2) Print semaphore info, including which tasks are pending on it.
		taskSuspend( 0 );				// 3) Suspend task. Can be resumed from the console for debugging.
	}
#else
	semTake( inMDNS->p->lock, WAIT_FOREVER );
#endif
}

//===========================================================================================================================
//	mDNSPlatformUnlock
//===========================================================================================================================

void	mDNSPlatformUnlock( const mDNS * const inMDNS )
{
	check_string( inMDNS->p && ( inMDNS->p->taskID != ERROR ), "mDNS task not started" );
	
	// Wake up the mDNS task to handle any work initiated by an API call and to calculate the next event time.
	// We only need to wake up if we're not already inside the task. This avoids filling up the command queue.
	
	if( taskIdSelf() != inMDNS->p->taskID )
	{
		SendCommand( inMDNS, kMDNSPipeCommandCodeReschedule );
	}
	semGive( inMDNS->p->lock );
}

//===========================================================================================================================
//	mDNSPlatformStrLen
//===========================================================================================================================

mDNSu32	mDNSPlatformStrLen( const void *inSrc )
{
	check( inSrc );
	
	return( (mDNSu32) strlen( (const char *) inSrc ) );
}

//===========================================================================================================================
//	mDNSPlatformStrCopy
//===========================================================================================================================

void	mDNSPlatformStrCopy( void *inDst, const void *inSrc )
{
	check( inSrc );
	check( inDst );
	
	strcpy( (char *) inDst, (const char*) inSrc );
}

//===========================================================================================================================
//	mDNSPlatformMemCopy
//===========================================================================================================================

void	mDNSPlatformMemCopy( void *inDst, const void *inSrc, mDNSu32 inSize )
{
	check( inSrc );
	check( inDst );
	
	memcpy( inDst, inSrc, inSize );
}

//===========================================================================================================================
//	mDNSPlatformMemSame
//===========================================================================================================================

mDNSBool	mDNSPlatformMemSame( const void *inDst, const void *inSrc, mDNSu32 inSize )
{
	check( inSrc );
	check( inDst );
	
	return( memcmp( inSrc, inDst, inSize ) == 0 );
}

//===========================================================================================================================
//	mDNSPlatformMemZero
//===========================================================================================================================

void	mDNSPlatformMemZero( void *inDst, mDNSu32 inSize )
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
	if( inMem ) free( inMem );
}

//===========================================================================================================================
//	mDNSPlatformRandomSeed
//===========================================================================================================================

mDNSexport mDNSu32	mDNSPlatformRandomSeed( void )
{
	return( tickGet() );
}

//===========================================================================================================================
//	mDNSPlatformTimeInit
//===========================================================================================================================

mDNSexport mStatus	mDNSPlatformTimeInit( void )
{
	// No special setup is required on VxWorks -- we just use tickGet().
	
	return( mStatus_NoError );
}

//===========================================================================================================================
//	mDNSPlatformRawTime
//===========================================================================================================================

mDNSs32	mDNSPlatformRawTime( void )
{
	return( (mDNSs32) tickGet() );
}

//===========================================================================================================================
//	mDNSPlatformUTC
//===========================================================================================================================

mDNSexport mDNSs32	mDNSPlatformUTC( void )
{
	return( (mDNSs32) time( NULL ) );
}

//===========================================================================================================================
//	mDNSPlatformInterfaceIDfromInterfaceIndex
//===========================================================================================================================

mDNSexport mDNSInterfaceID	mDNSPlatformInterfaceIDfromInterfaceIndex( mDNS *const inMDNS, mDNSu32 inIndex )
{
	NetworkInterfaceInfoVxWorks *		i;
	
	if( inIndex == (mDNSu32) -1 ) return( mDNSInterface_LocalOnly );
	if( inIndex != 0 )
	{
		for( i = inMDNS->p->interfaceList; i; i = i->next )
		{
			// Don't get tricked by inactive interfaces with no InterfaceID set.
			
			if( i->ifinfo.InterfaceID && ( i->scopeID == inIndex ) ) return( i->ifinfo.InterfaceID );
		}
	}
	return( NULL );
}

//===========================================================================================================================
//	mDNSPlatformInterfaceIndexfromInterfaceID
//===========================================================================================================================

mDNSexport mDNSu32	mDNSPlatformInterfaceIndexfromInterfaceID( mDNS *const inMDNS, mDNSInterfaceID inID )
{
	NetworkInterfaceInfoVxWorks *		i;
	
	if( inID == mDNSInterface_LocalOnly ) return( (mDNSu32) -1 );
	if( inID )
	{
		// Don't use i->ifinfo.InterfaceID here, because we DO want to find inactive interfaces.
		
		for( i = inMDNS->p->interfaceList; i && ( (mDNSInterfaceID) i != inID ); i = i->next ) {}
		if( i ) return( i->scopeID );
	}
	return( 0 );
}

//===========================================================================================================================
//	mDNSPlatformInterfaceNameToID
//===========================================================================================================================

mStatus	mDNSPlatformInterfaceNameToID( mDNS * const inMDNS, const char *inName, mDNSInterfaceID *outID )
{
	NetworkInterfaceInfoVxWorks *		i;
	
	for( i = inMDNS->p->interfaceList; i; i = i->next )
	{
		// Don't get tricked by inactive interfaces with no InterfaceID set.
		
		if( i->ifinfo.InterfaceID && ( strcmp( i->ifinfo.ifname, inName ) == 0 ) )
		{
			*outID = (mDNSInterfaceID) i;
			return( mStatus_NoError );
		}
	}
	return( mStatus_NoSuchNameErr );
}

//===========================================================================================================================
//	mDNSPlatformInterfaceIDToInfo
//===========================================================================================================================

mStatus	mDNSPlatformInterfaceIDToInfo( mDNS * const inMDNS, mDNSInterfaceID inID, mDNSPlatformInterfaceInfo *outInfo )
{
	NetworkInterfaceInfoVxWorks *		i;
	
	// Don't use i->ifinfo.InterfaceID here, because we DO want to find inactive interfaces.
	
	for( i = inMDNS->p->interfaceList; i && ( (mDNSInterfaceID) i != inID ); i = i->next ) {}
	if( !i ) return( mStatus_NoSuchNameErr );
	
	outInfo->name 	= i->ifinfo.ifname;
	outInfo->ip 	= i->ifinfo.ip;
	return( mStatus_NoError );
}

//===========================================================================================================================
//	debugf_
//===========================================================================================================================

#if( MDNS_DEBUGMSGS > 0 )
mDNSexport void	debugf_( const char *inFormat, ... )
{
	char		buffer[ 512 ];
	va_list		args;
	
	va_start( args, inFormat );
	mDNS_vsnprintf( buffer, sizeof( buffer ), inFormat, args );
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
	
	va_start( args, inFormat );
	mDNS_vsnprintf( buffer, sizeof( buffer ), inFormat, args );
	va_end( args );
	
	dlog( kDebugLevelVerbose, "%s\n", buffer );
}
#endif

//===========================================================================================================================
//	LogMsg
//===========================================================================================================================

mDNSexport void	LogMsg( const char *inFormat, ... )
{
#if( DEBUG )
	char		buffer[ 512 ];
	va_list		args;
	
	va_start( args, inFormat );
	mDNS_vsnprintf( buffer, sizeof( buffer ), inFormat, args );
	va_end( args );
	
	dlog( kDebugLevelWarning, "%s\n", buffer );
#else
	DEBUG_UNUSED( inFormat );
#endif
}

#if( DEBUG )
//===========================================================================================================================
//	DebugMsg
//===========================================================================================================================

mDNSlocal void	DebugMsg( DebugLevel inLevel, const char *inFormat, ... )
{
	char		buffer[ 512 ];
	va_list		args;
	
	va_start( args, inFormat );
	mDNS_vsnprintf( buffer, sizeof( buffer ), inFormat, args );
	va_end( args );
	
	if( inLevel >= gMDNSDebugOverrideLevel ) inLevel = kDebugLevelMax;
	dlog( inLevel, "%s", buffer );
}
#endif

#if 0
#pragma mark -
#pragma mark == Interfaces ==
#endif

//===========================================================================================================================
//	UpdateInterfaceList
//===========================================================================================================================

#if( MDNS_ENABLE_PPP )
	
	// Note: This includes PPP dial-in interfaces (pppXYZ), but not PPP dial-out interface (pppdXYZ).
	
	#define IsCompatibleInterface( IFA )																		\
		( ( ( IFA )->ifa_flags & IFF_UP )																	&&	\
		  ( ( ( IFA )->ifa_addr->sa_family == AF_INET ) || ( ( IFA )->ifa_addr->sa_family == AF_INET6 ) )	&&	\
		  ( ( IFA )->ifa_netmask && ( ( IFA )->ifa_addr->sa_family == ( IFA )->ifa_netmask->sa_family ) )	&&	\
		  ( !( ( IFA )->ifa_flags & IFF_POINTOPOINT ) || ( strncmp( ( IFA )->ifa_name, "pppd", 4 ) != 0 ) ) )
#else
	#define IsCompatibleInterface( IFA )																				\
		( ( ( ( IFA )->ifa_flags & ( IFF_UP | IFF_MULTICAST | IFF_POINTOPOINT ) ) == ( IFF_UP | IFF_MULTICAST ) )	&&	\
		  ( ( ( IFA )->ifa_addr->sa_family == AF_INET ) || ( ( IFA )->ifa_addr->sa_family == AF_INET6 ) )			&&	\
		  ( ( IFA )->ifa_netmask && ( ( IFA )->ifa_addr->sa_family == ( IFA )->ifa_netmask->sa_family ) ) )
#endif

#define	IsLinkLocalSockAddr( SA )																	\
	( ( ( (const struct sockaddr *)( SA ) )->sa_family == AF_INET )   								\
	  ? ( ( ( (uint8_t *)( &( (const struct sockaddr_in *)( SA ) )->sin_addr ) )[ 0 ] == 169 ) && 	\
		  ( ( (uint8_t *)( &( (const struct sockaddr_in *)( SA ) )->sin_addr ) )[ 1 ] == 254 ) )	\
	  : IN6_IS_ADDR_LINKLOCAL( &( (const struct sockaddr_in6 *)( SA ) )->sin6_addr ) )

#define	FamilyToString( X )					\
	( ( ( X ) == AF_INET )	? "AF_INET"  :	\
	( ( ( X ) == AF_INET6 )	? "AF_INET6" :	\
	( ( ( X ) == AF_LINK )	? "AF_LINK"  :	\
							  "UNKNOWN" ) ) )

mDNSlocal mStatus	UpdateInterfaceList( mDNS *const inMDNS, mDNSs32 inUTC )
{
	mStatus								err;
	struct ifaddrs *					ifaList;
	struct ifaddrs *					ifa;
	int									family;
	mDNSBool							foundV4;
	mDNSBool							foundV6;
	struct ifaddrs *					loopbackV4;
	struct ifaddrs *					loopbackV6;
	mDNSEthAddr							primaryMAC;
	SocketRef							infoSock;
	char								defaultName[ 64 ];
	NetworkInterfaceInfoVxWorks *		i;
	domainlabel							nicelabel;
	domainlabel							hostlabel;
	domainlabel							tmp;
	
	ifaList		= NULL;	
	foundV4		= mDNSfalse;
	foundV6		= mDNSfalse;
	loopbackV4	= NULL;
	loopbackV6	= NULL;
	primaryMAC	= zeroEthAddr;
	
	// Set up an IPv6 socket so we can check the state of interfaces using SIOCGIFAFLAG_IN6.
	
	infoSock = socket( AF_INET6, SOCK_DGRAM, 0 );
	check_translated_errno( IsValidSocket( infoSock ), errno_compat(), kUnknownErr );
	
	// Run through the entire list of interfaces.
	
	err = getifaddrs( &ifaList );
	check_translated_errno( err == 0, errno_compat(), kUnknownErr );
	
	for( ifa = ifaList; ifa; ifa = ifa->ifa_next )
	{
		int		flags;
		
		family = ifa->ifa_addr->sa_family;
		dmsg( kDebugLevelVerbose, DEBUG_NAME "%s: %8s(%d), Flags 0x%08X, Family %8s(%2d)\n", __ROUTINE__, 
			ifa->ifa_name, if_nametoindex( ifa->ifa_name ), ifa->ifa_flags, FamilyToString( family ), family );
		
		// Save off the MAC address of the first Ethernet-ish interface.
		
		if( family == AF_LINK )
		{
			struct sockaddr_dl *		sdl;
			
			sdl = (struct sockaddr_dl *) ifa->ifa_addr;
			if( ( sdl->sdl_type == IFT_ETHER ) && ( sdl->sdl_alen == sizeof( primaryMAC ) && 
				mDNSSameEthAddress( &primaryMAC, &zeroEthAddr ) ) )
			{
				memcpy( primaryMAC.b, sdl->sdl_data + sdl->sdl_nlen, 6 );
			}
		}
		
		if( !IsCompatibleInterface( ifa ) ) continue;
		
		// If this is a link-local address and there's a non-link-local address on this interface, skip this alias.
		
		if( IsLinkLocalSockAddr( ifa->ifa_addr ) )
		{
			struct ifaddrs *		ifaLL;
			
			for( ifaLL = ifaList; ifaLL; ifaLL = ifaLL->ifa_next )
			{
				if( ifaLL->ifa_addr->sa_family != family )			continue;
				if( !IsCompatibleInterface( ifaLL ) )				continue;
				if( strcmp( ifaLL->ifa_name, ifa->ifa_name ) != 0 )	continue;
				if( !IsLinkLocalSockAddr( ifaLL->ifa_addr ) )		break;
			}
			if( ifaLL )
			{
				dmsg( kDebugLevelInfo, DEBUG_NAME "%s: %8s(%d) skipping link-local alias\n", __ROUTINE__, 
					ifa->ifa_name, if_nametoindex( ifa->ifa_name ) );
				continue;
			}
		}
		
		// If this is an IPv6 interface, perform additional checks to make sure it is really ready for use.
		// If this is a loopback interface, save it off since we may add it later if there are no other interfaces.
		// Otherwise, add the interface to the list.
		
		flags = 0;
		if( ( family == AF_INET6 ) && IsValidSocket( infoSock ) )
		{
			struct sockaddr_in6 *		sa6;
			struct in6_ifreq			ifr6;
			
			sa6 = (struct sockaddr_in6 *) ifa->ifa_addr;
			mDNSPlatformMemZero( &ifr6, sizeof( ifr6 ) );
			strcpy( ifr6.ifr_name, ifa->ifa_name );
			ifr6.ifr_addr = *sa6;
			if( ioctl( infoSock, SIOCGIFAFLAG_IN6, (int) &ifr6 ) != -1 )
			{
				flags = ifr6.ifr_ifru.ifru_flags6;
			}
		}

		// HACK: This excludes interfaces with IN6_IFF_DUPLICATED set instead of using IN6_IFF_NOTREADY (which is
		// HACK: IN6_IFF_TENTATIVE | IN6_IFF_DUPLICATED) because we currently do not get a notification when an
		// HACK: interface goes from the tentative state to the fully ready state. So as a short-term workaround, 
		// HACK: this allows tentative interfaces to be registered. We should revisit if we get notification hooks.
		
		if( flags & ( IN6_IFF_DUPLICATED | IN6_IFF_DETACHED | IN6_IFF_DEPRECATED | IN6_IFF_TEMPORARY ) )
		{
			dmsg( kDebugLevelNotice, DEBUG_NAME "%s: %8s(%d), SIOCGIFAFLAG_IN6 not ready yet (0x%X)\n", __ROUTINE__, 
				ifa->ifa_name, if_nametoindex( ifa->ifa_name ), flags );
			continue;
		}
		if( ifa->ifa_flags & IFF_LOOPBACK )
		{
			if( family == AF_INET )	loopbackV4 = ifa;
			else					loopbackV6 = ifa;
		}
		else
		{
			if( ( family == AF_INET ) && gMDNSDeferIPv4 && IsLinkLocalSockAddr( ifa->ifa_addr ) ) continue;
			i = AddInterfaceToList( inMDNS, ifa, inUTC );
			if( i && i->multicast )
			{
				if( family == AF_INET )	foundV4 = mDNStrue;
				else					foundV6 = mDNStrue;
			}
		}
	}
	
	// For efficiency, we don't register a loopback interface when other interfaces of that family are available.
	
	if( !foundV4 && loopbackV4 ) AddInterfaceToList( inMDNS, loopbackV4, inUTC );
	if( !foundV6 && loopbackV6 ) AddInterfaceToList( inMDNS, loopbackV6, inUTC );
	freeifaddrs( ifaList );
	if( IsValidSocket( infoSock ) ) close_compat( infoSock );
	
	// The list is complete. Set the McastTxRx setting for each interface. We always send and receive using IPv4.
	// To reduce traffic, we send and receive using IPv6 only on interfaces that have no routable IPv4 address.
	// Having a routable IPv4 address assigned is a reasonable indicator of being on a large, configured network,
	// which means there's a good chance that most or all the other devices on that network should also have v4.
	// By doing this we lose the ability to talk to true v6-only devices on that link, but we cut the packet rate in half.
	// At this time, reducing the packet rate is more important than v6-only devices on a large configured network,
	// so we are willing to make that sacrifice.
	
	for( i = inMDNS->p->interfaceList; i; i = i->next )
	{
		if( i->exists )
		{
			mDNSBool	txrx;
			
			txrx = i->multicast && ( ( i->ifinfo.ip.type == mDNSAddrType_IPv4 ) || !FindRoutableIPv4( inMDNS, i->scopeID ) );
			if( i->ifinfo.McastTxRx != txrx )
			{
				i->ifinfo.McastTxRx = txrx;
				i->exists			= 2;	// 2=state change; need to de-register and re-register this interface.
			}
		}
	}
	
	// Set up the user-specified, friendly name, which is allowed to be full UTF-8.
	
	mDNS_snprintf( defaultName, sizeof( defaultName ), "Device-%02X:%02X:%02X:%02X:%02X:%02X",
		primaryMAC.b[ 0 ], primaryMAC.b[ 1 ], primaryMAC.b[ 2 ], primaryMAC.b[ 3 ], primaryMAC.b[ 4 ], primaryMAC.b[ 5 ] );
	
	MakeDomainLabelFromLiteralString( &nicelabel, "Put Nice Name Here" ); // $$$ Implementers: Fill in nice name of device.
	if( nicelabel.c[ 0 ] == 0 ) MakeDomainLabelFromLiteralString( &nicelabel, defaultName );
	
	// Set up the RFC 1034-compliant label. If not set or it is not RFC 1034 compliant, try the user-specified nice name.
	
	MakeDomainLabelFromLiteralString( &tmp, "Put-DNS-Name-Here" ); // $$$ Implementers: Fill in DNS name of device.
	ConvertUTF8PstringToRFC1034HostLabel( tmp.c, &hostlabel );
	if( hostlabel.c[ 0 ] == 0 ) ConvertUTF8PstringToRFC1034HostLabel( nicelabel.c, &hostlabel );
	if( hostlabel.c[ 0 ] == 0 ) MakeDomainLabelFromLiteralString( &hostlabel, defaultName );
	
	// Update our globals and mDNS with the new labels.
	
	if( !SameDomainLabelCS( inMDNS->p->userNiceLabel.c, nicelabel.c ) )
	{
		dmsg( kDebugLevelInfo, DEBUG_NAME "Updating nicelabel to \"%#s\"\n", nicelabel.c );
		inMDNS->p->userNiceLabel	= nicelabel;
		inMDNS->nicelabel			= nicelabel;
	}
	if( !SameDomainLabelCS( inMDNS->p->userHostLabel.c, hostlabel.c ) )
	{
		dmsg( kDebugLevelInfo, DEBUG_NAME "Updating hostlabel to \"%#s\"\n", hostlabel.c );
		inMDNS->p->userHostLabel	= hostlabel;
		inMDNS->hostlabel			= hostlabel;
		mDNS_SetFQDN( inMDNS );
	}
	return( mStatus_NoError );
}

//===========================================================================================================================
//	AddInterfaceToList
//===========================================================================================================================

mDNSlocal NetworkInterfaceInfoVxWorks *	AddInterfaceToList( mDNS *const inMDNS, struct ifaddrs *inIFA, mDNSs32 inUTC )
{
	mStatus								err;
	mDNSAddr							ip;
	mDNSAddr							mask;
	mDNSu32								scopeID;
	NetworkInterfaceInfoVxWorks **		p;
	NetworkInterfaceInfoVxWorks *		i;
		
	i = NULL;
	
	err = SockAddrToMDNSAddr( inIFA->ifa_addr, &ip );
	require_noerr( err, exit );
	
	err = SockAddrToMDNSAddr( inIFA->ifa_netmask, &mask );
	require_noerr( err, exit );
	
	// Search for an existing interface with the same info. If found, just return that one.
	
	scopeID = if_nametoindex( inIFA->ifa_name );
	check( scopeID );
	for( p = &inMDNS->p->interfaceList; *p; p = &( *p )->next )
	{
		if( ( scopeID == ( *p )->scopeID ) && mDNSSameAddress( &ip, &( *p )->ifinfo.ip ) )
		{
			dmsg( kDebugLevelInfo, DEBUG_NAME "%s: Found existing interface %u with address %#a at %#p\n", __ROUTINE__, 
				scopeID, &ip, *p );
			( *p )->exists = mDNStrue;
			i = *p;
			goto exit;
		}
	}
	
	// Allocate the new interface info and fill it out.
	
	i = (NetworkInterfaceInfoVxWorks *) calloc( 1, sizeof( *i ) );
	require( i, exit );
	
	dmsg( kDebugLevelInfo, DEBUG_NAME "%s: Making   new   interface %u with address %#a at %#p\n", __ROUTINE__, scopeID, &ip, i );
	strncpy( i->ifinfo.ifname, inIFA->ifa_name, sizeof( i->ifinfo.ifname ) );	
	i->ifinfo.ifname[ sizeof( i->ifinfo.ifname ) - 1 ] = '\0';
	i->ifinfo.InterfaceID	= NULL;
	i->ifinfo.ip			= ip;
	i->ifinfo.mask			= mask;
	i->ifinfo.Advertise		= inMDNS->AdvertiseLocalAddresses;
	i->ifinfo.McastTxRx		= mDNSfalse; // For now; will be set up later at the end of UpdateInterfaceList.
	
	i->next					= NULL;
	i->exists				= mDNStrue;
	i->lastSeen				= inUTC;
	i->scopeID				= scopeID;
	i->family				= inIFA->ifa_addr->sa_family;
	i->multicast			= ( inIFA->ifa_flags & IFF_MULTICAST ) && !( inIFA->ifa_flags & IFF_POINTOPOINT );
	
	i->ss.info				= i;
	i->ss.sockV4			= kInvalidSocketRef;
	i->ss.sockV6			= kInvalidSocketRef;
	*p = i;
	
exit:
	return( i );
}

//===========================================================================================================================
//	SetupActiveInterfaces
//
//	Returns a count of non-link local IPv4 addresses registered.
//===========================================================================================================================

#define	mDNSAddressIsNonLinkLocalIPv4( X )	\
	( ( ( X )->type == mDNSAddrType_IPv4 ) && ( ( ( X )->ip.v4.b[ 0 ] != 169 ) || ( ( X )->ip.v4.b[ 1 ] != 254 ) ) )

mDNSlocal int	SetupActiveInterfaces( mDNS *const inMDNS, mDNSs32 inUTC )
{
	int									count;
	NetworkInterfaceInfoVxWorks *		i;
	
	count = 0;
	for( i = inMDNS->p->interfaceList; i; i = i->next )
	{
		NetworkInterfaceInfo *				n;
		NetworkInterfaceInfoVxWorks *		primary;
		
		if( !i->exists ) continue;
		
		// Search for the primary interface and sanity check it.
		
		n = &i->ifinfo;
		primary = FindInterfaceByIndex( inMDNS, i->family, i->scopeID );
		if( !primary )
		{
			dmsg( kDebugLevelError, DEBUG_NAME "%s: ERROR! didn't find %s(%u)\n", __ROUTINE__, i->ifinfo.ifname, i->scopeID );
			continue;
		}
		if( n->InterfaceID && ( n->InterfaceID != (mDNSInterfaceID) primary ) )
		{
			dmsg( kDebugLevelError, DEBUG_NAME "%s: ERROR! n->InterfaceID %#p != primary %#p\n", __ROUTINE__, 
				n->InterfaceID, primary );
			n->InterfaceID = NULL;
		}
		
		// If n->InterfaceID is set, it means we've already called mDNS_RegisterInterface() for this interface.
		// so we don't need to call it again. Otherwise, register the interface with mDNS.
		
		if( !n->InterfaceID )
		{
			mDNSBool		flapping;
			
			n->InterfaceID = (mDNSInterfaceID) primary;
			
			// If lastSeen == inUTC, then this is a brand-new interface, or an interface that never went away.
			// If lastSeen != inUTC, then this is an old interface, that went away for (inUTC - lastSeen) seconds.
			// If it's is an old one that went away and came back in less than a minute, we're in a flapping scenario.
			
			flapping = ( ( inUTC - i->lastSeen ) > 0 ) && ( ( inUTC - i->lastSeen ) < 60 );
			mDNS_RegisterInterface( inMDNS, n, flapping );
			if( mDNSAddressIsNonLinkLocalIPv4( &i->ifinfo.ip ) ) ++count;
			
			dmsg( kDebugLevelInfo, DEBUG_NAME "%s:   Registered    %8s(%u) InterfaceID %#p %#a%s%s\n", __ROUTINE__, 
				i->ifinfo.ifname, i->scopeID, primary, &n->ip, 
				flapping			? " (Flapping)" : "", 
				n->InterfaceActive	? " (Primary)"  : "" );
		}
		
		// Set up a socket if it's not already set up. If multicast is not enabled on this interface then we 
		// don't need a socket since unicast traffic will be handled on the unicast socket.
		
		if( n->McastTxRx )
		{
			mStatus		err;
			
			if( ( ( i->family == AF_INET  ) && !IsValidSocket( primary->ss.sockV4 ) ) || 
				( ( i->family == AF_INET6 ) && !IsValidSocket( primary->ss.sockV6 ) ) )
			{
				err = SetupSocket( inMDNS, &i->ifinfo.ip, mDNStrue, i->family, &primary->ss );
				check_noerr( err );
			}
		}
		else
		{
			dmsg( kDebugLevelInfo, DEBUG_NAME "%s:   No Tx/Rx on   %8s(%u) InterfaceID %#p %#a\n", __ROUTINE__, 
				i->ifinfo.ifname, i->scopeID, primary, &n->ip );
		}
	}
	return( count );
}

//===========================================================================================================================
//	MarkAllInterfacesInactive
//===========================================================================================================================

mDNSlocal void	MarkAllInterfacesInactive( mDNS *const inMDNS, mDNSs32 inUTC )
{
	NetworkInterfaceInfoVxWorks *		i;
	
	for( i = inMDNS->p->interfaceList; i; i = i->next )
	{
		if( !i->exists ) continue;
		i->lastSeen = inUTC;
		i->exists	= mDNSfalse;
	}
}

//===========================================================================================================================
//	ClearInactiveInterfaces
//
//	Returns count of non-link local IPv4 addresses de-registered.
//===========================================================================================================================

mDNSlocal int	ClearInactiveInterfaces( mDNS *const inMDNS, mDNSs32 inUTC, mDNSBool inClosing )
{
	int									count;
	NetworkInterfaceInfoVxWorks *		i;
	NetworkInterfaceInfoVxWorks **		p;
	
	// First pass:
	// If an interface is going away, then de-register it from mDNSCore.
	// We also have to de-register it if the primary interface that it's using for its InterfaceID is going away.
	// We have to do this because mDNSCore will use that InterfaceID when sending packets, and if the memory
	// it refers to has gone away, we'll crash. Don't actually close the sockets or free the memory yet though: 
	// When the last representative of an interface goes away mDNSCore may want to send goodbye packets on that 
	// interface. (Not yet implemented, but a good idea anyway.).
	
	count = 0;
	for( i = inMDNS->p->interfaceList; i; i = i->next )
	{
		NetworkInterfaceInfoVxWorks *		primary;
		
		// 1. If this interface is no longer active, or its InterfaceID is changing, de-register it.
		
		if( !i->ifinfo.InterfaceID ) continue;
		primary = FindInterfaceByIndex( inMDNS, i->family, i->scopeID );
		if( ( i->exists == 0 ) || ( i->exists == 2 ) || ( i->ifinfo.InterfaceID != (mDNSInterfaceID) primary ) )
		{
			dmsg( kDebugLevelInfo, DEBUG_NAME "%s: Deregistering %8s(%u) InterfaceID %#p %#a%s\n", __ROUTINE__, 
				i->ifinfo.ifname, i->scopeID, i->ifinfo.InterfaceID, &i->ifinfo.ip, 
				i->ifinfo.InterfaceActive ? " (Primary)" : "" );
			
			mDNS_DeregisterInterface( inMDNS, &i->ifinfo, mDNSfalse );
			i->ifinfo.InterfaceID = NULL;
			if( mDNSAddressIsNonLinkLocalIPv4( &i->ifinfo.ip ) ) ++count;
		}
	}
	
	// Second pass:
	// Now that everything that's going to de-register has done so, we can close sockets and free the memory.
	
	p = &inMDNS->p->interfaceList;
	while( *p )
	{
		i = *p;
		
		// 2. Close all our sockets. We'll recreate them later as needed.
		// (We may have previously had both v4 and v6, and we may not need both any more.).
		
		ForgetSocket( &i->ss.sockV4 );
		ForgetSocket( &i->ss.sockV6 );
		
		// 3. If no longer active, remove the interface from the list and free its memory.
		
		if( !i->exists )
		{
			mDNSBool		deleteIt;
			
			if( inClosing )
			{
				check_string( NumCacheRecordsForInterfaceID( inMDNS, (mDNSInterfaceID) i ) == 0, "closing with in-use records!" );
				deleteIt = mDNStrue;
			}
			else
			{
				if( i->lastSeen == inUTC ) i->lastSeen = inUTC - 1;
				deleteIt = ( NumCacheRecordsForInterfaceID( inMDNS, (mDNSInterfaceID) i ) == 0 ) && ( ( inUTC - i->lastSeen ) >= 60 );
			}
			dmsg( kDebugLevelInfo, DEBUG_NAME "%s: %-13s %8s(%u) InterfaceID %#p %#a Age %d%s\n", __ROUTINE__, 
				deleteIt ? "Deleting" : "Holding", i->ifinfo.ifname, i->scopeID, i->ifinfo.InterfaceID, &i->ifinfo.ip, 
				inUTC - i->lastSeen, i->ifinfo.InterfaceActive ? " (Primary)" : "" );
			if( deleteIt )
			{
				*p = i->next;
				free( i );
				continue;
			}
		}
		p = &i->next;
	}
	return( count );
}

//===========================================================================================================================
//	FindRoutableIPv4
//===========================================================================================================================

mDNSlocal NetworkInterfaceInfoVxWorks *	FindRoutableIPv4( mDNS *const inMDNS, mDNSu32 inScopeID )
{
#if( MDNS_EXCLUDE_IPV4_ROUTABLE_IPV6 )
	NetworkInterfaceInfoVxWorks *		i;
	
	for( i = inMDNS->p->interfaceList; i; i = i->next )
	{
		if( i->exists && ( i->scopeID == inScopeID ) && mDNSAddressIsNonLinkLocalIPv4( &i->ifinfo.ip ) )
		{
			break;
		}
	}
	return( i );
#else
	DEBUG_UNUSED( inMDNS );
	DEBUG_UNUSED( inScopeID );
	
	return( NULL );
#endif
}

//===========================================================================================================================
//	FindInterfaceByIndex
//===========================================================================================================================

mDNSlocal NetworkInterfaceInfoVxWorks *	FindInterfaceByIndex( mDNS *const inMDNS, int inFamily, mDNSu32 inIndex )
{
	NetworkInterfaceInfoVxWorks *		i;
	
	check( inIndex != 0 );
	
	for( i = inMDNS->p->interfaceList; i; i = i->next )
	{
		if( i->exists && ( i->scopeID == inIndex ) &&
			( MDNS_AAAA_OVER_IPV4															||
			  ( ( inFamily == AF_INET  ) && ( i->ifinfo.ip.type == mDNSAddrType_IPv4 ) )	||
			  ( ( inFamily == AF_INET6 ) && ( i->ifinfo.ip.type == mDNSAddrType_IPv6 ) ) ) )
		{
			return( i );
		}
	}
	return( NULL );
}

//===========================================================================================================================
//	SetupSocket
//===========================================================================================================================

mDNSlocal mStatus	SetupSocket( mDNS *const inMDNS, const mDNSAddr *inAddr, mDNSBool inMcast, int inFamily, SocketSet *inSS )
{
	mStatus			err;
	SocketRef *		sockPtr;
	mDNSIPPort		port;
	SocketRef		sock;
	const int		on = 1;
		
	check( inAddr );
	check( inSS );
	
	sockPtr	= ( inFamily == AF_INET ) ? &inSS->sockV4 : &inSS->sockV6;
	port	= ( inMcast || inMDNS->CanReceiveUnicastOn5353 ) ? MulticastDNSPort : zeroIPPort;
	
	sock = socket( inFamily, SOCK_DGRAM, IPPROTO_UDP );
	err = translate_errno( IsValidSocket( sock ), errno_compat(), mStatus_UnknownErr );
	require_noerr( err, exit );
	
	// Allow multiple listeners if this is a multicast port.
	
	if( port.NotAnInteger )
	{	
		err = setsockopt( sock, SOL_SOCKET, SO_REUSEPORT, (char *) &on, sizeof( on ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
	}
	
	// Set up the socket based on the family (IPv4 or IPv6).
	
	if( inFamily == AF_INET )
	{
		const int				ttlV4		= 255;
		const u_char			ttlV4Mcast	= 255;
		struct sockaddr_in		sa4;
		
		// Receive destination addresses so we know which address the packet was sent to.
		
		err = setsockopt( sock, IPPROTO_IP, IP_RECVDSTADDR, (char *) &on, sizeof( on ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
		// Receive interface indexes so we know which interface received the packet.
		
		err = setsockopt( sock, IPPROTO_IP, IP_RECVIF, (char *) &on, sizeof( on ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
		// Join the multicast group on this interface and specify the outgoing interface, if it's for multicast receiving.
		
		if( inMcast )
		{
			struct in_addr		addrV4;
			struct ip_mreq		mreqV4;
			
			addrV4.s_addr				= inAddr->ip.v4.NotAnInteger;
			mreqV4.imr_multiaddr.s_addr = AllDNSLinkGroup_v4.ip.v4.NotAnInteger;
			mreqV4.imr_interface		= addrV4;
			err = setsockopt( sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreqV4, sizeof( mreqV4 ) );
			check_translated_errno( err == 0, errno_compat(), kOptionErr );
			
			err = setsockopt( sock, IPPROTO_IP, IP_MULTICAST_IF, (char *) &addrV4, sizeof( addrV4 ) );
			check_translated_errno( err == 0, errno_compat(), kOptionErr );
		}
		
		// Send unicast packets with TTL 255 (helps against spoofing).
		
		err = setsockopt( sock, IPPROTO_IP, IP_TTL, (char *) &ttlV4, sizeof( ttlV4 ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
		// Send multicast packets with TTL 255 (helps against spoofing).
		
		err = setsockopt( sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &ttlV4Mcast, sizeof( ttlV4Mcast ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
		// Start listening for packets.
		
		mDNSPlatformMemZero( &sa4, sizeof( sa4 ) );
		sa4.sin_len			= sizeof( sa4 );
		sa4.sin_family		= AF_INET;
		sa4.sin_port		= port.NotAnInteger;
		sa4.sin_addr.s_addr	= htonl( INADDR_ANY ); // We want to receive multicasts AND unicasts on this socket.
		err = bind( sock, (struct sockaddr *) &sa4, sizeof( sa4 ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
	}
	else if( inFamily == AF_INET6 )
	{
		struct sockaddr_in6		sa6;
		const int				ttlV6 = 255;
		
		// Receive destination addresses and interface index so we know where the packet was received and intended.
		
		err = setsockopt( sock, IPPROTO_IPV6, IPV6_PKTINFO, (char *) &on, sizeof( on ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
		// Receive only IPv6 packets because otherwise, we may get IPv4 addresses as IPv4-mapped IPv6 addresses.
		
		err = setsockopt( sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *) &on, sizeof( on ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
		// Join the multicast group on this interface and specify the outgoing interface, if it's for multicast receiving.
		
		if( inMcast )
		{
			u_int					ifindex;
			struct ipv6_mreq		mreqV6;
			
			ifindex					= inSS->info->scopeID;
			mreqV6.ipv6mr_interface	= ifindex;
			mreqV6.ipv6mr_multiaddr	= *( (struct in6_addr * ) &AllDNSLinkGroup_v6.ip.v6 );
			err = setsockopt( sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *) &mreqV6, sizeof( mreqV6 ) );
			check_translated_errno( err == 0, errno_compat(), kOptionErr );
			
			err = setsockopt( sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char *) &ifindex, sizeof( ifindex ) );
			check_translated_errno( err == 0, errno_compat(), kOptionErr );
		}
		
		// Send unicast packets with TTL 255 (helps against spoofing).
		
		err = setsockopt( sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, (char *) &ttlV6, sizeof( ttlV6 ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
		// Send multicast packets with TTL 255 (helps against spoofing).
		
		err = setsockopt( sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *) &ttlV6, sizeof( ttlV6 ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
		// Receive our own packets for same-machine operation.
		
		err = setsockopt( sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (char *) &on, sizeof( on ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
		
		// Start listening for packets.
		
		mDNSPlatformMemZero( &sa6, sizeof( sa6 ) );
		sa6.sin6_len		= sizeof( sa6 );
		sa6.sin6_family		= AF_INET6;
		sa6.sin6_port		= port.NotAnInteger;
		sa6.sin6_flowinfo	= 0;
		sa6.sin6_addr		= in6addr_any; // We want to receive multicasts AND unicasts on this socket.
		sa6.sin6_scope_id	= 0;
		err = bind( sock, (struct sockaddr *) &sa6, sizeof( sa6 ) );
		check_translated_errno( err == 0, errno_compat(), kOptionErr );
	}
	else
	{
		dmsg( kDebugLevelError, DEBUG_NAME "%s: unsupport socket family (%d)\n", __ROUTINE__, inFamily );
		err = kUnsupportedErr;
		goto exit;
	}
	
	// Make the socket non-blocking so we can potentially get multiple packets per select call.
	
	err = ioctl( sock, FIONBIO, (int) &on );
	check_translated_errno( err == 0, errno_compat(), kOptionErr );
	
	*sockPtr = sock;
	sock = kInvalidSocketRef;
	err = mStatus_NoError;
	
exit:
	if( IsValidSocket( sock ) ) close_compat( sock );
	return( err );
}

//===========================================================================================================================
//	SockAddrToMDNSAddr
//===========================================================================================================================

mDNSlocal mStatus	SockAddrToMDNSAddr( const struct sockaddr * const inSA, mDNSAddr *outIP )
{
	mStatus		err;
	
	check( inSA );
	check( outIP );
	
	if( inSA->sa_family == AF_INET )
	{
		struct sockaddr_in *		sa4;
		
		sa4 = (struct sockaddr_in *) inSA;
		outIP->type 				= mDNSAddrType_IPv4;
		outIP->ip.v4.NotAnInteger	= sa4->sin_addr.s_addr;
		err = mStatus_NoError;
	}
	else if( inSA->sa_family == AF_INET6 )
	{
		struct sockaddr_in6 *		sa6;
		
		sa6 = (struct sockaddr_in6 *) inSA;
		outIP->type 	= mDNSAddrType_IPv6;
		outIP->ip.v6 	= *( (mDNSv6Addr *) &sa6->sin6_addr );
		if( IN6_IS_ADDR_LINKLOCAL( &sa6->sin6_addr ) ) outIP->ip.v6.w[ 1 ] = 0;
		err = mStatus_NoError;
	}
	else
	{
		dmsg( kDebugLevelError, DEBUG_NAME "%s: invalid sa_family (%d)\n", __ROUTINE__, inSA->sa_family );
		err = mStatus_BadParamErr;
	}
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Commands ==
#endif

//===========================================================================================================================
//	SetupCommandPipe
//===========================================================================================================================

mDNSlocal mStatus	SetupCommandPipe( mDNS * const inMDNS )
{
	mStatus		err;
		
	err = pipeDevCreate( "/pipe/mDNS", 32, 1 );
	check_translated_errno( err == 0, errno_compat(), kUnknownErr );
	
	inMDNS->p->commandPipe = open( "/pipe/mDNS", O_RDWR, 0 );
	err = translate_errno( inMDNS->p->commandPipe != ERROR, errno_compat(), mStatus_UnsupportedErr );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	TearDownCommandPipe
//===========================================================================================================================

mDNSlocal mStatus	TearDownCommandPipe( mDNS * const inMDNS )
{
	mStatus		err;
	
	if( inMDNS->p->commandPipe != ERROR )
	{
		err = close( inMDNS->p->commandPipe );
		check_translated_errno( err == 0, errno_compat(), kUnknownErr );
		inMDNS->p->commandPipe = ERROR;
		
		err = pipeDevDelete( "/pipe/mDNS", FALSE );
		check_translated_errno( err == 0, errno_compat(), kUnknownErr );
	}
	return( mStatus_NoError );
}

//===========================================================================================================================
//	SendCommand
//===========================================================================================================================

mDNSlocal mStatus	SendCommand( const mDNS * const inMDNS, MDNSPipeCommandCode inCommandCode )
{
	mStatus		err;
	
	require_action_quiet( inMDNS->p->commandPipe != ERROR, exit, err = mStatus_NotInitializedErr );
	
	err = write( inMDNS->p->commandPipe, &inCommandCode, sizeof( inCommandCode ) );
	err = translate_errno( err >= 0, errno_compat(), kWriteErr );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	ProcessCommand
//===========================================================================================================================

mDNSlocal mStatus	ProcessCommand( mDNS * const inMDNS )
{
	mStatus					err;
	MDNSPipeCommandCode		cmd;
	mDNSs32					utc;
		
	err = read( inMDNS->p->commandPipe, &cmd, sizeof( cmd ) );
	err = translate_errno( err >= 0, errno_compat(), kReadErr );
	require_noerr( err, exit );
	
	switch( cmd )
	{
		case kMDNSPipeCommandCodeReschedule:	// Reschedule: just break out to re-run mDNS_Execute.
			break;
		
		case kMDNSPipeCommandCodeReconfigure:	// Reconfigure: rebuild the interface list after a config change.
			dmsg( kDebugLevelInfo, DEBUG_NAME "***   NETWORK CONFIGURATION CHANGE   ***\n" );
			mDNSPlatformLock( inMDNS );
			
			utc = mDNSPlatformUTC();
			MarkAllInterfacesInactive( inMDNS, utc );
			UpdateInterfaceList( inMDNS, utc );
			ClearInactiveInterfaces( inMDNS, utc, mDNSfalse );
			SetupActiveInterfaces( inMDNS, utc );
			
			mDNSPlatformUnlock( inMDNS );
			mDNS_ConfigChanged(inMDNS);
			break;
		
		case kMDNSPipeCommandCodeQuit:			// Quit: just set a flag so the task exits cleanly.
			inMDNS->p->quit = mDNStrue;
			break;
		
		default:
			dmsg( kDebugLevelError, DEBUG_NAME "unknown pipe command (%d)\n", cmd );
			err = mStatus_BadParamErr;
			goto exit;
	}
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Threads ==
#endif

//===========================================================================================================================
//	Task
//===========================================================================================================================

mDNSlocal void	Task( mDNS *inMDNS )
{
	mStatus								err;
	mDNSs32								nextEvent;
	fd_set								readSet;
	int									maxFd;
	struct timeval						timeout;
	NetworkInterfaceInfoVxWorks *		i;
	int									fd;
	int									n;
	
	check( inMDNS );
	
	err = TaskInit( inMDNS );
	require_noerr( err, exit );
	
	while( !inMDNS->p->quit )
	{
		// Let mDNSCore do its work then wait for an event. On idle timeouts (n == 0), just loop back to mDNS_Exceute.
		
		nextEvent = mDNS_Execute( inMDNS );
		TaskSetupSelect( inMDNS, &readSet, &maxFd, nextEvent, &timeout );			
		n = select( maxFd + 1, &readSet, NULL, NULL, &timeout );
		check_translated_errno( n >= 0, errno_compat(), kUnknownErr );
		if( n == 0 ) continue;
		
		// Process interface-specific sockets with pending data.
		
		n = 0;
		for( i = inMDNS->p->interfaceList; i; i = i->next )
		{
			fd = i->ss.sockV4;
			if( IsValidSocket( fd ) && FD_ISSET( fd, &readSet ) )
			{
				TaskProcessPackets( inMDNS, &i->ss, fd );
				++n;
			}
			fd = i->ss.sockV6;
			if( IsValidSocket( fd ) && FD_ISSET( fd, &readSet ) )
			{
				TaskProcessPackets( inMDNS, &i->ss, fd );
				++n;
			}
		}
		
		// Process unicast sockets with pending data.
		
		fd = inMDNS->p->unicastSS.sockV4;
		if( IsValidSocket( fd ) && FD_ISSET( fd, &readSet ) )
		{
			TaskProcessPackets( inMDNS, &inMDNS->p->unicastSS, fd );
			++n;
		}
		fd = inMDNS->p->unicastSS.sockV6;
		if( IsValidSocket( fd ) && FD_ISSET( fd, &readSet ) )
		{
			TaskProcessPackets( inMDNS, &inMDNS->p->unicastSS, fd );
			++n;
		}
		
		// Processing pending commands.
		
		fd = inMDNS->p->commandPipe;
		check( fd >= 0 );
		if( FD_ISSET( fd, &readSet ) )
		{
			ProcessCommand( inMDNS );
			++n;
		}
		check_string( n > 0, "select said something was readable, but nothing was" );
	}

exit:
	TaskTerm( inMDNS );
}

//===========================================================================================================================
//	TaskInit
//===========================================================================================================================

mDNSlocal mStatus	TaskInit( mDNS *inMDNS )
{
	mStatus			err;
	mDNSs32			utc;
	socklen_t		len;
	
	inMDNS->p->taskID = taskIdSelf();
	
	err = SetupCommandPipe( inMDNS );
	require_noerr( err, exit );
	
	inMDNS->CanReceiveUnicastOn5353 = mDNStrue;
	
	// Set up the HINFO string using the description property (e.g. "Device V1.0").
	
	inMDNS->HIHardware.c[ 0 ] = 11;
	memcpy( &inMDNS->HIHardware.c[ 1 ], "Device V1.0", inMDNS->HIHardware.c[ 0 ] ); // $$$ Implementers: Fill in real info.
	
	// Set up the unicast sockets.
	
	err = SetupSocket( inMDNS, &zeroAddr, mDNSfalse, AF_INET, &inMDNS->p->unicastSS );
	check_noerr( err );
	if( err == mStatus_NoError )
	{
		struct sockaddr_in		sa4;
		
		len = sizeof( sa4 );
		err = getsockname( inMDNS->p->unicastSS.sockV4, (struct sockaddr *) &sa4, &len );
		check_translated_errno( err == 0, errno_compat(), kUnknownErr );
		if( err == 0 ) inMDNS->UnicastPort4.NotAnInteger = sa4.sin_port;
	}
	
	err = SetupSocket( inMDNS, &zeroAddr, mDNSfalse, AF_INET6, &inMDNS->p->unicastSS );
	check_noerr( err );
	if( err == mStatus_NoError )
	{
		struct sockaddr_in6		sa6;
		
		len = sizeof( sa6 );
		err = getsockname( inMDNS->p->unicastSS.sockV6, (struct sockaddr *) &sa6, &len );
		check_translated_errno( err == 0, errno_compat(), kUnknownErr );
		if( err == 0 ) inMDNS->UnicastPort6.NotAnInteger = sa6.sin6_port;
	}
	
	// Set up the interfaces.
	
	utc = mDNSPlatformUTC();
	UpdateInterfaceList( inMDNS, utc );
	SetupActiveInterfaces( inMDNS, utc );
	err = mStatus_NoError;
	
exit:
	// Signal the "ready" semaphore to indicate the task initialization code has completed (success or not).
	
	inMDNS->p->initErr = err;
	semGive( inMDNS->p->initEvent );
	return( err );
}

//===========================================================================================================================
//	TaskTerm
//===========================================================================================================================

mDNSlocal void	TaskTerm( mDNS *inMDNS )
{
	mStatus		err;
	mDNSs32		utc;
	
	// Tear down all interfaces.
	
	utc = mDNSPlatformUTC();
	MarkAllInterfacesInactive( inMDNS, utc );
	ClearInactiveInterfaces( inMDNS, utc, mDNStrue );
	check_string( !inMDNS->p->interfaceList, "LEAK: closing without deleting all interfaces" );
	
	// Close unicast sockets.
	
	ForgetSocket( &inMDNS->p->unicastSS.sockV4);
	ForgetSocket( &inMDNS->p->unicastSS.sockV6 );
	
	// Tear down everything else that was set up in TaskInit then signal back that we're done.
	
	err = TearDownCommandPipe( inMDNS );
	check_noerr( err );
	
	err = semGive( inMDNS->p->quitEvent );
	check_translated_errno( err == 0, errno_compat(), kUnknownErr );
}

//===========================================================================================================================
//	TaskSetupSelect
//===========================================================================================================================

mDNSlocal void	TaskSetupSelect( mDNS *inMDNS, fd_set *outSet, int *outMaxFd, mDNSs32 inNextEvent, struct timeval *outTimeout )
{
	NetworkInterfaceInfoVxWorks *		i;
	int									maxFd;
	int									fd;
	mDNSs32								delta;
	
	FD_ZERO( outSet );
	maxFd = -1;
	
	// Add the interface-specific sockets.
	
	for( i = inMDNS->p->interfaceList; i; i = i->next )
	{
		fd = i->ss.sockV4;
		if( IsValidSocket( fd ) )
		{
			FD_SET( fd, outSet );
			if( fd > maxFd ) maxFd = fd;
		}
		
		fd = i->ss.sockV6;
		if( IsValidSocket( fd ) )
		{
			FD_SET( fd, outSet );
			if( fd > maxFd ) maxFd = fd;
		}
	}
	
	// Add the unicast sockets.
	
	fd = inMDNS->p->unicastSS.sockV4;
	if( IsValidSocket( fd ) )
	{
		FD_SET( fd, outSet );
		if( fd > maxFd ) maxFd = fd;
	}
	
	fd = inMDNS->p->unicastSS.sockV6;
	if( IsValidSocket( fd ) )
	{
		FD_SET( fd, outSet );
		if( fd > maxFd ) maxFd = fd;
	}
	
	// Add the command pipe.
	
	fd = inMDNS->p->commandPipe;
	check( fd >= 0 );
	FD_SET( fd, outSet );
	if( fd > maxFd ) maxFd = fd;
	
	check( maxFd > 0 );
	*outMaxFd = maxFd;
	
	// Calculate how long to wait before performing idle processing.
	
	delta = inNextEvent - mDNS_TimeNow( inMDNS );
	if( delta <= 0 )
	{
		// The next task time is now or in the past. Set the timeout to fire immediately.
		
		outTimeout->tv_sec 	= 0;
		outTimeout->tv_usec	= 0;
	}
	else
	{
		// Calculate the seconds and microseconds until the timeout should occur. Add one to the ticks remainder 
		// before multiplying to account for integer rounding error and avoid firing the timeout too early.
		
		outTimeout->tv_sec	= delta / mDNSPlatformOneSecond;
		outTimeout->tv_usec	= ( ( delta % mDNSPlatformOneSecond ) + 1 ) * gMDNSTicksToMicro;
		if( outTimeout->tv_usec >= 1000000L )
		{
			outTimeout->tv_sec += 1;
			outTimeout->tv_usec = 0;
		}
	}
}

//===========================================================================================================================
//	TaskProcessPackets
//===========================================================================================================================

mDNSlocal void	TaskProcessPackets( mDNS *inMDNS, SocketSet *inSS, SocketRef inSock )
{
	mDNSu32						ifindex;
	ssize_t						n;
	mDNSu8 *					buf;
	size_t						size;
	struct sockaddr_storage		from;
	size_t						fromSize;
	mDNSAddr					destAddr;
	mDNSAddr					senderAddr;
	mDNSIPPort					senderPort;
	mDNSInterfaceID				id;
	
	buf  = (mDNSu8 *) &inMDNS->imsg;
	size = sizeof( inMDNS->imsg );
	for( ;; )
	{
		ifindex = 0;
		n = mDNSRecvMsg( inSock, buf, size, &from, sizeof( from ), &fromSize, &destAddr, &ifindex );		
		if( n < 0 ) break;
		if( from.ss_family == AF_INET )
		{
			struct sockaddr_in *		sa4;
			
			sa4 = (struct sockaddr_in *) &from;
			senderAddr.type					= mDNSAddrType_IPv4;
			senderAddr.ip.v4.NotAnInteger	= sa4->sin_addr.s_addr;
			senderPort.NotAnInteger			= sa4->sin_port;
		}
		else if( from.ss_family == AF_INET6 )
		{
			struct sockaddr_in6 *		sa6;
			
			sa6 = (struct sockaddr_in6 *) &from;
			senderAddr.type			= mDNSAddrType_IPv6;
			senderAddr.ip.v6		= *( (mDNSv6Addr *) &sa6->sin6_addr );
			senderPort.NotAnInteger	= sa6->sin6_port;
		}
		else
		{
			dmsg( kDebugLevelWarning, DEBUG_NAME "%s: WARNING! from addr unknown family %d\n", __ROUTINE__, from.ss_family );
			continue;
		}
		
		// Even though we indicated a specific interface when joining the multicast group, a weirdness of the 
		// sockets API means that even though this socket has only officially joined the multicast group
		// on one specific interface, the kernel will still deliver multicast packets to it no matter which
		// interface they arrive on. According to the official Unix Powers That Be, this is Not A Bug.
		// To work around this weirdness, we use the IP_RECVIF/IPV6_PKTINFO options to find the interface
		// on which the packet arrived, and ignore the packet if it really arrived on some other interface.
		
		if( mDNSAddrIsDNSMulticast( &destAddr ) )
		{
			if( !inSS->info || !inSS->info->exists )
			{
				dpkt( kDebugLevelChatty - 3, DEBUG_NAME "  ignored mcast, src=[%#39a],       dst=[%#39a],       if= unicast socket %d\n", 
					&senderAddr, &destAddr, inSock );
				continue;
			}
			if( ifindex != inSS->info->scopeID )
			{
				#if( DEBUG && MDNS_DEBUG_PACKETS )
					char		ifname[ IF_NAMESIZE ];
				#endif
				
				dpkt( kDebugLevelChatty - 3, 
					DEBUG_NAME "  ignored mcast, src=[%#39a]        dst=[%#39a],       if=%8s(%u) -- really for %8s(%u)\n", 
					&senderAddr, &destAddr, inSS->info->ifinfo.ifname, inSS->info->scopeID, 
					if_indextoname( ifindex, ifname ), ifindex );
				continue;
			}
			
			id = inSS->info->ifinfo.InterfaceID;
			dpkt( kDebugLevelChatty - 2, DEBUG_NAME "recv %4d bytes, src=[%#39a]:%5hu, dst=[%#39a],       if=%8s(%u) %#p\n", 
				n, &senderAddr, mDNSVal16( senderPort ), &destAddr, inSS->info->ifinfo.ifname, inSS->info->scopeID, id );
		}
		else
		{
			NetworkInterfaceInfoVxWorks *		i;
			
			// For unicast packets, try to find the matching interface.
			
			for( i = inMDNS->p->interfaceList; i && ( i->scopeID != ifindex ); i = i->next ) {}
			if( i ) id = i->ifinfo.InterfaceID;
			else	id = NULL;
		}
		mDNSCoreReceive( inMDNS, buf, buf + n, &senderAddr, senderPort, &destAddr, MulticastDNSPort, id );
	}
}

//===========================================================================================================================
//	mDNSRecvMsg
//===========================================================================================================================

mDNSlocal ssize_t
	mDNSRecvMsg( 
		SocketRef	inSock, 
		void *		inBuffer, 
		size_t		inBufferSize, 
		void *		outFrom, 
		size_t		inFromSize, 
		size_t *	outFromSize, 
		mDNSAddr *	outDstAddr, 
		uint32_t *	outIndex )
{
	struct msghdr			msg;
	struct iovec			iov;
	ssize_t					n;
	char					ancillary[ 1024 ];
	struct cmsghdr *		cmPtr;
	int						err;
	
	// Read a packet and any ancillary data. Note: EWOULDBLOCK errors are expected when we've read all pending packets.
	
	iov.iov_base		= (char *) inBuffer;
	iov.iov_len			= inBufferSize;
	msg.msg_name		= (caddr_t) outFrom;
	msg.msg_namelen		= inFromSize;
	msg.msg_iov			= &iov;
	msg.msg_iovlen		= 1;
	msg.msg_control		= (caddr_t) &ancillary;
	msg.msg_controllen	= sizeof( ancillary );
	msg.msg_flags		= 0;
	n = recvmsg( inSock, &msg, 0 );
	if( n < 0 )
	{
		err = errno_compat();
		if( err != EWOULDBLOCK ) dmsg( kDebugLevelWarning, DEBUG_NAME "%s: recvmsg(%d) returned %d, errno %d\n", 
			__ROUTINE__, inSock, n, err );
		goto exit;
	}
	if( msg.msg_controllen < sizeof( struct cmsghdr ) )
	{
		dmsg( kDebugLevelWarning, DEBUG_NAME "%s: recvmsg(%d) msg_controllen %d < sizeof( struct cmsghdr ) %u\n", 
			__ROUTINE__, inSock, msg.msg_controllen, sizeof( struct cmsghdr ) );
		n = mStatus_UnknownErr;
		goto exit;
	}
	if( msg.msg_flags & MSG_CTRUNC )
	{
		dmsg( kDebugLevelWarning, DEBUG_NAME "%s: recvmsg(%d) MSG_CTRUNC (%d recv'd)\n", __ROUTINE__, inSock, n );
		n = mStatus_BadFlagsErr;
		goto exit;
	}
	*outFromSize = msg.msg_namelen;
	
	// Parse each option out of the ancillary data.
	
	for( cmPtr = CMSG_FIRSTHDR( &msg ); cmPtr; cmPtr = CMSG_NXTHDR( &msg, cmPtr ) )
	{
		if( ( cmPtr->cmsg_level == IPPROTO_IP ) && ( cmPtr->cmsg_type == IP_RECVDSTADDR ) )
		{
			outDstAddr->type				= mDNSAddrType_IPv4;
			outDstAddr->ip.v4.NotAnInteger	= *( (mDNSu32 *) CMSG_DATA( cmPtr ) );
		}
		else if( ( cmPtr->cmsg_level == IPPROTO_IP ) && ( cmPtr->cmsg_type == IP_RECVIF ) )
		{
			struct sockaddr_dl *		sdl;
			
			sdl = (struct sockaddr_dl *) CMSG_DATA( cmPtr );
			*outIndex = sdl->sdl_index;
		}
		else if( ( cmPtr->cmsg_level == IPPROTO_IPV6 ) && ( cmPtr->cmsg_type == IPV6_PKTINFO ) )
		{
			struct in6_pktinfo *		pi6;
			
			pi6 = (struct in6_pktinfo *) CMSG_DATA( cmPtr );
			outDstAddr->type	= mDNSAddrType_IPv6;
			outDstAddr->ip.v6	= *( (mDNSv6Addr *) &pi6->ipi6_addr );
			*outIndex			= pi6->ipi6_ifindex;
		}
	}
	
exit:
	return( n );
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( DEBUG && MDNS_DEBUG_SHOW )
//===========================================================================================================================
//	mDNSShow
//===========================================================================================================================

void	mDNSShow( void );

void	mDNSShow( void )
{
	NetworkInterfaceInfoVxWorks *		i;
	int									num;
	AuthRecord *						r;
	mDNSs32								utc;
	
	// Globals
	
	dmsg( kDebugLevelMax, "\n-- mDNS globals --\n" );
	dmsg( kDebugLevelMax, "    sizeof( mDNS )           = %d\n", (int) sizeof( mDNS ) );
	dmsg( kDebugLevelMax, "    sizeof( ResourceRecord ) = %d\n", (int) sizeof( ResourceRecord ) );
	dmsg( kDebugLevelMax, "    sizeof( AuthRecord )     = %d\n", (int) sizeof( AuthRecord ) );
	dmsg( kDebugLevelMax, "    sizeof( CacheRecord )    = %d\n", (int) sizeof( CacheRecord ) );
	dmsg( kDebugLevelMax, "    mDNSPlatformOneSecond    = %ld\n", mDNSPlatformOneSecond );
	dmsg( kDebugLevelMax, "    gMDNSTicksToMicro        = %ld\n", gMDNSTicksToMicro );
	dmsg( kDebugLevelMax, "    gMDNSPtr                 = %#p\n", gMDNSPtr );
	if( !gMDNSPtr )
	{
		dmsg( kDebugLevelMax, "### mDNS not initialized\n" );
		return;
	}
	dmsg( kDebugLevelMax, "    nicelabel                = \"%#s\"\n", gMDNSPtr->nicelabel.c );
	dmsg( kDebugLevelMax, "    hostLabel                = \"%#s\"\n", gMDNSPtr->hostlabel.c );
	dmsg( kDebugLevelMax, "    MulticastHostname        = \"%##s\"\n", gMDNSPtr->MulticastHostname.c );
	dmsg( kDebugLevelMax, "    HIHardware               = \"%#s\"\n", gMDNSPtr->HIHardware.c );
	dmsg( kDebugLevelMax, "    HISoftware               = \"%#s\"\n", gMDNSPtr->HISoftware.c );
	dmsg( kDebugLevelMax, "    UnicastPort4/6           = %d/%d\n", 
		mDNSVal16( gMDNSPtr->UnicastPort4 ), mDNSVal16( gMDNSPtr->UnicastPort6 ) );
	dmsg( kDebugLevelMax, "    unicastSS.sockV4/V6      = %d/%d\n", 
		gMDNSPtr->p->unicastSS.sockV4, gMDNSPtr->p->unicastSS.sockV6 );
	dmsg( kDebugLevelMax, "    lock                     = %#p\n", gMDNSPtr->p->lock );
	dmsg( kDebugLevelMax, "    initEvent                = %#p\n", gMDNSPtr->p->initEvent );
	dmsg( kDebugLevelMax, "    initErr                  = %ld\n", gMDNSPtr->p->initErr );
	dmsg( kDebugLevelMax, "    quitEvent                = %#p\n", gMDNSPtr->p->quitEvent );
	dmsg( kDebugLevelMax, "    commandPipe              = %d\n", gMDNSPtr->p->commandPipe );
	dmsg( kDebugLevelMax, "    taskID                   = %#p\n", gMDNSPtr->p->taskID );
	dmsg( kDebugLevelMax, "\n" );
	
	// Interfaces
	
	utc = mDNSPlatformUTC();
	dmsg( kDebugLevelMax, "-- mDNS interfaces --\n" );
	num = 0;
	for( i = gMDNSPtr->p->interfaceList; i; i = i->next )
	{
		dmsg( kDebugLevelMax, "    interface %2d %8s(%u) [%#39a] %s, sockV4 %2d, sockV6 %2d, Age %d\n", 
			num, i->ifinfo.ifname, i->scopeID, &i->ifinfo.ip, 
			i->ifinfo.InterfaceID ? "      REGISTERED" : "*NOT* registered", 
			i->ss.sockV4, i->ss.sockV6, utc - i->lastSeen );
		++num;
	}
	dmsg( kDebugLevelMax, "\n" );
	
	// Resource Records
	
	dmsg( kDebugLevelMax, "-- mDNS resource records --\n" );
	num = 0;
	for( r = gMDNSPtr->ResourceRecords; r; r = r->next )
	{
		i = (NetworkInterfaceInfoVxWorks *) r->resrec.InterfaceID;
		if( r->resrec.rrtype == kDNSType_TXT )
		{
			RDataBody *			rd;
			const mDNSu8 *		txt;
			const mDNSu8 *		end;
			mDNSu8				size;
			int					nEntries;
						
			rd = &r->resrec.rdata->u;
			dmsg( kDebugLevelMax, "    record %2d: %#p %8s(%u): %4d %##s %s:\n", num, i, 
				i ? i->ifinfo.ifname	: "<any>", 
				i ? i->scopeID			: 0, 
				r->resrec.rdlength, r->resrec.name->c, DNSTypeName( r->resrec.rrtype ) );
			
			nEntries = 0;
			txt = rd->txt.c;
			end = txt + r->resrec.rdlength;
			while( txt < end )
			{
				size = *txt++;
				if( ( txt + size ) > end )
				{
					dmsg( kDebugLevelMax, "        ### ERROR! txt length byte too big (%u, %u max)\n", size, end - txt );
					break;
				}
				dmsg( kDebugLevelMax, "        string %2d (%3d bytes): \"%.*s\"\n", nEntries, size, size, txt );
				txt += size;
				++nEntries;
			}
		}
		else
		{
			dmsg( kDebugLevelMax, "    record %2d: %#p %8s(%u): %s\n", num, i, 
				i ? i->ifinfo.ifname	: "<any>", 
				i ? i->scopeID			: 0, 
				ARDisplayString( gMDNSPtr, r ) );
		}
		++num;
	}
	dmsg( kDebugLevelMax, "\n");
}
#endif	// DEBUG && MDNS_DEBUG_SHOW
