/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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

	Contains:	mDNS platform plugin for VxWorks.

	Copyright:  Copyright (C) 2002-2004 Apple Computer, Inc., All Rights Reserved.

	Change History (most recent first):

$Log: mDNSVxWorksIPv4Only.c,v $
Revision 1.34  2009/01/13 05:31:35  mkrochma
<rdar://problem/6491367> Replace bzero, bcopy with mDNSPlatformMemZero, mDNSPlatformMemCopy, memset, memcpy

Revision 1.33  2008/11/04 19:51:13  cheshire
Updated comment about MAX_ESCAPED_DOMAIN_NAME size (should be 1009, not 1005)

Revision 1.32  2008/10/03 18:25:18  cheshire
Instead of calling "m->MainCallback" function pointer directly, call mDNSCore routine "mDNS_ConfigChanged(m);"

Revision 1.31  2007/03/22 18:31:49  cheshire
Put dst parameter first in mDNSPlatformStrCopy/mDNSPlatformMemCopy, like conventional Posix strcpy/memcpy

Revision 1.30  2006/12/19 22:43:56  cheshire
Fix compiler warnings

Revision 1.29  2006/08/14 23:25:18  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.28  2006/03/19 02:00:12  cheshire
<rdar://problem/4073825> Improve logic for delaying packets after repeated interface transitions

Revision 1.27  2004/12/17 23:37:49  cheshire
<rdar://problem/3485365> Guard against repeating wireless dissociation/re-association
(and other repetitive configuration changes)

Revision 1.26  2004/10/28 02:00:35  cheshire
<rdar://problem/3841770> Call pipeDevDelete when disposing of commandPipe

Revision 1.25  2004/10/16 00:17:01  cheshire
<rdar://problem/3770558> Replace IP TTL 255 check with local subnet source address check

Revision 1.24  2004/09/21 21:02:56  cheshire
Set up ifname before calling mDNS_RegisterInterface()

Revision 1.23  2004/09/17 01:08:57  cheshire
Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
  The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
  declared in that file are ONLY appropriate to single-address-space embedded applications.
  For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.

Revision 1.22  2004/09/17 00:19:11  cheshire
For consistency with AllDNSLinkGroupv6, rename AllDNSLinkGroup to AllDNSLinkGroupv4

Revision 1.21  2004/09/16 00:24:50  cheshire
<rdar://problem/3803162> Fix unsafe use of mDNSPlatformTimeNow()

Revision 1.20  2004/09/14 23:42:36  cheshire
<rdar://problem/3801296> Need to seed random number generator from platform-layer data

Revision 1.19  2004/09/14 23:16:09  cheshire
mDNS_SetFQDNs has been renamed to mDNS_SetFQDN

Revision 1.18  2004/08/14 03:22:42  cheshire
<rdar://problem/3762579> Dynamic DNS UI <-> mDNSResponder glue
Add GetUserSpecifiedDDNSName() routine
Convert ServiceRegDomain to domainname instead of C string
Replace mDNS_GenerateFQDN/mDNS_GenerateGlobalFQDN with mDNS_SetFQDNs

Revision 1.17  2004/07/29 19:26:03  ksekar
Plaform-level changes for NAT-PMP support

Revision 1.16  2004/04/22 05:11:28  bradley
Added mDNSPlatformUTC for TSIG signed dynamic updates.

Revision 1.15  2004/04/21 02:49:12  cheshire
To reduce future confusion, renamed 'TxAndRx' to 'McastTxRx'

Revision 1.14  2004/04/09 17:43:04  cheshire
Make sure to set the McastTxRx field so that duplicate suppression works correctly

Revision 1.13  2004/01/27 20:15:24  cheshire
<rdar://problem/3541288>: Time to prune obsolete code for listening on port 53

Revision 1.12  2004/01/24 09:12:37  bradley
Avoid TOS socket options to workaround a TOS routing problem with VxWorks and multiple interfaces
when sending unicast responses, which resulted in packets going out the wrong interface.

Revision 1.11  2004/01/24 04:59:16  cheshire
Fixes so that Posix/Linux, OS9, Windows, and VxWorks targets build again

Revision 1.10  2003/11/14 21:27:09  cheshire
<rdar://problem/3484766>: Security: Crashing bug in mDNSResponder
Fix code that should use buffer size MAX_ESCAPED_DOMAIN_NAME (1009) instead of 256-byte buffers.

Revision 1.9  2003/11/14 20:59:09  cheshire
Clients can't use AssignDomainName macro because mDNSPlatformMemCopy is defined in mDNSPlatformFunctions.h.
Best solution is just to combine mDNSEmbeddedAPI.h and mDNSPlatformFunctions.h into a single file.

Revision 1.8  2003/10/28 10:08:27  bradley
Removed legacy port 53 support as it is no longer needed.

Revision 1.7  2003/08/20 05:58:54  bradley
Removed dependence on modified mDNSCore: define structures/prototypes locally.

Revision 1.6  2003/08/18 23:19:05  cheshire
<rdar://problem/3382647> mDNSResponder divide by zero in mDNSPlatformRawTime()

Revision 1.5  2003/08/15 00:05:04  bradley
Updated to use name/InterfaceID from new AuthRecord resrec field. Added output of new record sizes.

Revision 1.4  2003/08/14 02:19:55  cheshire
<rdar://problem/3375491> Split generic ResourceRecord type into two separate types: AuthRecord and CacheRecord

Revision 1.3  2003/08/12 19:56:27  cheshire
Update to APSL 2.0

Revision 1.2  2003/08/05 23:58:34  cheshire
Update code to compile with the new mDNSCoreReceive() function that requires a TTL
Right now this platform layer just reports 255 instead of returning the real value -- we should fix this

Revision 1.1  2003/08/02 10:06:48  bradley
mDNS platform plugin for VxWorks.


	Notes for non-Apple platforms:

		TARGET_NON_APPLE should be defined to 1 to avoid relying on Apple-only header files, macros, or functions.

	To Do:

		- Add support for IPv6 (needs VxWorks IPv6 support).
*/

// Set up the debug library to use the default category (see DebugServicesLite.h for details).

#if( !TARGET_NON_APPLE )
	#define	DEBUG_USE_DEFAULT_CATEGORY		1
#endif

#include	<stdarg.h>
#include	<stddef.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#include	<sys/types.h>
#include	<arpa/inet.h>
#include	<fcntl.h>
#include	<netinet/if_ether.h>
#include	<netinet/in.h>
#include	<netinet/ip.h>
#include	<sys/ioctl.h>
#include	<sys/socket.h>
#include	<unistd.h>

#include	"vxWorks.h"
#include	"ifLib.h"
#include	"inetLib.h"
#include	"pipeDrv.h"
#include	"selectLib.h"
#include	"semLib.h"
#include	"sockLib.h"
#include	"sysLib.h"
#include	"taskLib.h"
#include	"tickLib.h"

#include	"config.h"

#if( !TARGET_NON_APPLE )
	#include	"ACP/ACPUtilities.h"
	#include	"Support/DebugServicesLite.h"
	#include	"Support/MiscUtilities.h"
#endif

#include	"mDNSEmbeddedAPI.h"

#include	"mDNSVxWorks.h"

#if 0
#pragma mark == Preprocessor ==
#endif

//===========================================================================================================================
//	Preprocessor
//===========================================================================================================================

#if( !TARGET_NON_APPLE )
	debug_log_new_default_category( mdns );
#endif

#if 0
#pragma mark == Constants ==
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

#define	DEBUG_NAME						"[mDNS] "

#define	kMDNSDefaultName				"My-Device"

#define	kMDNSTaskName					"tMDNS"
#define	kMDNSTaskPriority				102
#define	kMDNSTaskStackSize				49152

#define	kMDNSPipeName					"/pipe/mDNS"
#define	kMDNSPipeMessageQueueSize		32
#define	kMDNSPipeMessageSize			1

#define	kInvalidSocketRef				-1

typedef uint8_t		MDNSPipeCommandCode;
enum
{
	kMDNSPipeCommandCodeInvalid			= 0, 
	kMDNSPipeCommandCodeReschedule 		= 1, 
	kMDNSPipeCommandCodeReconfigure		= 2, 
	kMDNSPipeCommandCodeQuit			= 3
};

#if 0
#pragma mark == Structures ==
#endif

//===========================================================================================================================
//	Structures
//===========================================================================================================================

typedef int		MDNSSocketRef;

struct	MDNSInterfaceItem
{
	MDNSInterfaceItem *			next;
	char						name[ 32 ];
	MDNSSocketRef				multicastSocketRef;
	MDNSSocketRef				sendingSocketRef;
	NetworkInterfaceInfo		hostSet;
	mDNSBool					hostRegistered;
	
	int							sendMulticastCounter;
	int							sendUnicastCounter;
	int							sendErrorCounter;
	
	int							recvCounter;
	int							recvErrorCounter;
	int							recvLoopCounter;
};

#if 0
#pragma mark == Macros ==
#endif

//===========================================================================================================================
//	Macros
//===========================================================================================================================

#if( TARGET_NON_APPLE )
	
	// Do-nothing versions of the debugging macros for non-Apple platforms.
	
	#define check(assertion)
	#define check_string( assertion, cstring )
	#define check_noerr(err)
	#define check_noerr_string( error, cstring )
	#define	check_errno( assertion, errno_value )
	#define debug_string( cstring )
	#define require( assertion, label )                             		do { if( !(assertion) ) goto label; } while(0)
	#define require_string( assertion, label, string )						require(assertion, label)
	#define require_quiet( assertion, label )								require( assertion, label )
	#define require_noerr( error, label )									do { if( (error) != 0 ) goto label; } while(0)
	#define require_noerr_quiet( assertion, label )							require_noerr( assertion, label )
	#define require_noerr_action( error, label, action )					do { if( (error) != 0 ) { {action;}; goto label; } } while(0)
	#define require_noerr_action_quiet( assertion, label, action )			require_noerr_action( assertion, label, action )
	#define require_action( assertion, label, action )						do { if( !(assertion) ) { {action;}; goto label; } } while(0)
	#define require_action_quiet( assertion, label, action )				require_action( assertion, label, action )
	#define require_action_string( assertion, label, action, cstring )		do { if( !(assertion) ) { {action;}; goto label; } } while(0)
	#define	require_errno( assertion, errno_value, label )					do { if( !(assertion) ) goto label; } while(0)
	#define	require_errno_action( assertion, errno_value, label, action )	do { if( !(assertion) ) { {action;}; goto label; } } while(0)
	
	#define	dlog( ARGS... )
	
	#define	DEBUG_UNUSED( X )			(void)( X )
#endif

#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

// ifIndexToIfp is in net/if.c, but not exported by net/if.h so provide it here.

extern struct ifnet * ifIndexToIfp(int ifIndex);

// Platform Internals

mDNSlocal void		SetupNames( mDNS * const inMDNS );
mDNSlocal mStatus	SetupInterfaceList( mDNS * const inMDNS );
mDNSlocal mStatus	TearDownInterfaceList( mDNS * const inMDNS );
mDNSlocal mStatus	SetupInterface( mDNS * const inMDNS, const struct ifaddrs *inAddr, MDNSInterfaceItem **outItem );
mDNSlocal mStatus	TearDownInterface( mDNS * const inMDNS, MDNSInterfaceItem *inItem );
mDNSlocal mStatus
	SetupSocket( 
		mDNS * const 			inMDNS, 
		const struct ifaddrs *	inAddr, 
		mDNSIPPort 				inPort, 
		MDNSSocketRef *			outSocketRef );

// Commands

mDNSlocal mStatus	SetupCommandPipe( mDNS * const inMDNS );
mDNSlocal mStatus	TearDownCommandPipe( mDNS * const inMDNS );
mDNSlocal mStatus	SendCommand( const mDNS * const inMDNS, MDNSPipeCommandCode inCommandCode );
mDNSlocal mStatus	ProcessCommand( mDNS * const inMDNS );
mDNSlocal void		ProcessCommandReconfigure( mDNS *inMDNS );

// Threads

mDNSlocal mStatus	SetupTask( mDNS * const inMDNS );
mDNSlocal mStatus	TearDownTask( mDNS * const inMDNS );
mDNSlocal void		Task( mDNS *inMDNS );
mDNSlocal mStatus	TaskInit( mDNS *inMDNS );
mDNSlocal void		TaskSetupReadSet( mDNS *inMDNS, fd_set *outReadSet, int *outMaxSocket );
mDNSlocal void		TaskSetupTimeout( mDNS *inMDNS, mDNSs32 inNextTaskTime, struct timeval *outTimeout );
mDNSlocal void		TaskProcessPacket( mDNS *inMDNS, MDNSInterfaceItem *inItem, MDNSSocketRef inSocketRef );

// Utilities

#if( TARGET_NON_APPLE )
	mDNSlocal void	GenerateUniqueHostName( char *outName, long *ioSeed );
	mDNSlocal void	GenerateUniqueDNSName( char *outName, long *ioSeed );
#endif

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

mDNSlocal mDNS *					gMDNSPtr							= NULL;
mDNSlocal mDNS_PlatformSupport		gMDNSPlatformSupport;
mDNSlocal mDNSs32					gMDNSTicksToMicrosecondsMultiplier	= 0;

// Platform support

mDNSs32								mDNSPlatformOneSecond;

#if 0
#pragma mark -
#pragma mark == Public APIs ==
#endif

//===========================================================================================================================
//	mDNSReconfigure
//===========================================================================================================================

void	mDNSReconfigure( void )
{
	// Send a "reconfigure" command to the MDNS task.
	
	if( gMDNSPtr )
	{
		SendCommand( gMDNSPtr, kMDNSPipeCommandCodeReconfigure );
	}
}

#if 0
#pragma mark -
#pragma mark == Platform Support ==
#endif

//===========================================================================================================================
//	mDNSPlatformInit
//===========================================================================================================================

mStatus	mDNSPlatformInit( mDNS * const inMDNS )
{
	mStatus		err;
	
	dlog( kDebugLevelInfo, DEBUG_NAME "platform init\n" );
	
	// Initialize variables.

	mDNSPlatformMemZero( &gMDNSPlatformSupport, sizeof( gMDNSPlatformSupport ) );
	inMDNS->p							= &gMDNSPlatformSupport;
	inMDNS->p->commandPipe				= ERROR;
	inMDNS->p->task						= ERROR;
	inMDNS->p->rescheduled 				= 1;		// Default to rescheduled until fully initialized.
	mDNSPlatformOneSecond 				= sysClkRateGet();
	gMDNSTicksToMicrosecondsMultiplier	= ( 1000000L / mDNSPlatformOneSecond );
	
	// Allocate semaphores.
	
	inMDNS->p->lockID = semMCreate( SEM_Q_FIFO );
	require_action( inMDNS->p->lockID, exit, err = mStatus_NoMemoryErr );
	
	inMDNS->p->readyEvent = semBCreate( SEM_Q_FIFO, SEM_EMPTY );
	require_action( inMDNS->p->readyEvent, exit, err = mStatus_NoMemoryErr );
	
	inMDNS->p->quitEvent = semBCreate( SEM_Q_FIFO, SEM_EMPTY );
	require_action( inMDNS->p->quitEvent, exit, err = mStatus_NoMemoryErr );
	
	gMDNSPtr = inMDNS;
	
	// Set up the task and wait for it to initialize. Initialization is done from the task instead of here to avoid 
	// stack space issues. Some of the initialization may require a larger stack than the current task supports.
	
	err = SetupTask( inMDNS );
	require_noerr( err, exit );
	
	err = semTake( inMDNS->p->readyEvent, WAIT_FOREVER );
	require_noerr( err, exit );
	err = inMDNS->p->taskInitErr;
	require_noerr( err, exit );
	
	mDNSCoreInitComplete( inMDNS, err );
	
exit:
	if( err )
	{
		mDNSPlatformClose( inMDNS );
	}
	dlog( kDebugLevelInfo, DEBUG_NAME "platform init done (err=%ld)\n", err );
	return( err );
}

//===========================================================================================================================
//	mDNSPlatformClose
//===========================================================================================================================

void	mDNSPlatformClose( mDNS * const inMDNS )
{
	mStatus		err;
	
	dlog( kDebugLevelInfo, DEBUG_NAME "platform close\n" );
	check( inMDNS );
	
	// Tear everything down.
	
	err = TearDownTask( inMDNS );
	check_noerr( err );
	
	err = TearDownInterfaceList( inMDNS );
	check_noerr( err );
		
	err = TearDownCommandPipe( inMDNS );
	check_noerr( err );
	
	gMDNSPtr = NULL;
	
	// Release semaphores.
	
	if( inMDNS->p->quitEvent )
	{
		semDelete( inMDNS->p->quitEvent );
		inMDNS->p->quitEvent = 0;
	}
	if( inMDNS->p->readyEvent )
	{
		semDelete( inMDNS->p->readyEvent );
		inMDNS->p->readyEvent = 0;
	}
	if( inMDNS->p->lockID )
	{
		semDelete( inMDNS->p->lockID );
		inMDNS->p->lockID = 0;
	}
	
	dlog( kDebugLevelInfo, DEBUG_NAME "platform close done\n" );
}

//===========================================================================================================================
//	mDNSPlatformSendUDP
//===========================================================================================================================

mStatus
	mDNSPlatformSendUDP( 
		const mDNS * const			inMDNS, 
		const void * const	        inMsg, 
		const mDNSu8 * const		inMsgEnd, 
		mDNSInterfaceID 			inInterfaceID, 
		const mDNSAddr *			inDstIP, 
		mDNSIPPort 					inDstPort )
{
	mStatus					err;
	MDNSInterfaceItem *		item;
	struct sockaddr_in		addr;
	int						n;
	
	dlog( kDebugLevelChatty, DEBUG_NAME "platform send UDP\n" );
	
	// Check parameters.
	
	check( inMDNS );
	check( inMsg );
	check( inMsgEnd );
	check( inInterfaceID );
	check( inDstIP );
	if( inDstIP->type != mDNSAddrType_IPv4 )
	{
		err = mStatus_BadParamErr;
		goto exit;
	}

#if( DEBUG )
	// Make sure the InterfaceID is valid.
	
	for( item = inMDNS->p->interfaceList; item; item = item->next )
	{
		if( item == (MDNSInterfaceItem *) inInterfaceID )
		{
			break;
		}
	}
	require_action( item, exit, err = mStatus_NoSuchNameErr );
#endif

	// Send the packet.
	
	item = (MDNSInterfaceItem *) inInterfaceID;
	check( item->sendingSocketRef != kInvalidSocketRef );
	
	mDNSPlatformMemZero( &addr, sizeof( addr ) );
	addr.sin_family 		= AF_INET;
	addr.sin_port 			= inDstPort.NotAnInteger;
	addr.sin_addr.s_addr 	= inDstIP->ip.v4.NotAnInteger;
	
	n = inMsgEnd - ( (const mDNSu8 * const) inMsg );
	n = sendto( item->sendingSocketRef, (char *) inMsg, n, 0, (struct sockaddr *) &addr, sizeof( addr ) );
	check_errno( n, errno );
	
	item->sendErrorCounter 		+= ( n < 0 );
	item->sendMulticastCounter 	+= ( inDstPort.NotAnInteger == MulticastDNSPort.NotAnInteger );
	item->sendUnicastCounter 	+= ( inDstPort.NotAnInteger != MulticastDNSPort.NotAnInteger );
	
	dlog( kDebugLevelChatty, DEBUG_NAME "sent (to=%u.%u.%u.%u:%hu)\n", 
		  inDstIP->ip.v4.b[ 0 ], inDstIP->ip.v4.b[ 1 ], inDstIP->ip.v4.b[ 2 ], inDstIP->ip.v4.b[ 3 ], 
		  htons( inDstPort.NotAnInteger ) );
	err = mStatus_NoError;

exit:
	dlog( kDebugLevelChatty, DEBUG_NAME "platform send UDP done\n" );
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
	(void)buflen;			// Unused
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
	check( inMDNS->p->lockID );
	
	if( inMDNS->p->lockID )
	{
		#if( TARGET_NON_APPLE )
			semTake( inMDNS->p->lockID, WAIT_FOREVER );
		#else
			semTakeDeadlockDetect( inMDNS->p->lockID, WAIT_FOREVER );
		#endif
	}
}

//===========================================================================================================================
//	mDNSPlatformUnlock
//===========================================================================================================================

void	mDNSPlatformUnlock( const mDNS * const inMDNS )
{
	check( inMDNS );
	check( inMDNS->p );
	check( inMDNS->p->lockID );
	check_string( inMDNS->p->task != ERROR, "mDNS task not started" );
	
	// When an API routine is called, "m->NextScheduledEvent" is reset to "timenow" before calling mDNSPlatformUnlock()
	// Since our main mDNS_Execute() loop is on a different thread, we need to wake up that thread to:
	// (a) handle immediate work (if any) resulting from this API call
	// (b) calculate the next sleep time between now and the next interesting event
	
	if( ( mDNS_TimeNow(inMDNS) - inMDNS->NextScheduledEvent ) >= 0 )
	{
		// We only need to send the reschedule event when called from a task other than the mDNS task since if we are 
		// called from mDNS task, we'll loop back and call mDNS_Execute. This avoids filling up the command queue.
		
		if( ( inMDNS->p->rescheduled++ == 0 ) && ( taskIdSelf() != inMDNS->p->task ) )
		{
			SendCommand( inMDNS, kMDNSPipeCommandCodeReschedule );
		}
	}
	
	if( inMDNS->p->lockID )
	{
		semGive( inMDNS->p->lockID );
	}
}

//===========================================================================================================================
//	mDNSPlatformStrLen
//===========================================================================================================================

mDNSu32  mDNSPlatformStrLen( const void *inSrc )
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
	
	free( inMem );
}

//===========================================================================================================================
//	mDNSPlatformRandomSeed
//===========================================================================================================================

mDNSexport mDNSu32 mDNSPlatformRandomSeed(void)
{
	return( tickGet() );
}

//===========================================================================================================================
//	mDNSPlatformTimeInit
//===========================================================================================================================

mDNSexport mStatus mDNSPlatformTimeInit( void )
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
	return( -1 );
}

//===========================================================================================================================
//	mDNSPlatformInterfaceNameToID
//===========================================================================================================================

mStatus	mDNSPlatformInterfaceNameToID( mDNS * const inMDNS, const char *inName, mDNSInterfaceID *outID )
{
	mStatus					err;
	MDNSInterfaceItem *		ifd;
	
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
	if( !ifd )
	{
		err = mStatus_NoSuchNameErr;
		goto exit;
	}
		
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

mStatus	mDNSPlatformInterfaceIDToInfo( mDNS * const inMDNS, mDNSInterfaceID inID, mDNSPlatformInterfaceInfo *outInfo )
{
	mStatus					err;
	MDNSInterfaceItem *		ifd;
	
	check( inMDNS );
	check( inID );
	check( outInfo );
	
	// Search for an interface with the specified ID,
	
	for( ifd = inMDNS->p->interfaceList; ifd; ifd = ifd->next )
	{
		if( ifd == (MDNSInterfaceItem *) inID )
		{
			break;
		}
	}
	if( !ifd )
	{
		err = mStatus_NoSuchNameErr;
		goto exit;
	}
	
	// Success!
	
	outInfo->name 	= ifd->name;
	outInfo->ip 	= ifd->hostSet.ip;
	err 			= mStatus_NoError;
	
exit:
	return( err );
}

//===========================================================================================================================
//	debugf_
//===========================================================================================================================

#if( MDNS_DEBUGMSGS )
mDNSexport void debugf_( const char *format, ... )
{
	char		buffer[ 512 ];
    va_list		args;
    mDNSu32		length;
    
	va_start( args, format );
	length = mDNS_vsnprintf( buffer, sizeof( buffer ), format, args );
	va_end( args );

	dlog( kDebugLevelInfo, "%s\n", buffer );
}
#endif

//===========================================================================================================================
//	verbosedebugf_
//===========================================================================================================================

#if( MDNS_DEBUGMSGS > 1 )
mDNSexport void verbosedebugf_( const char *format, ... )
{
	char		buffer[ 512 ];
    va_list		args;
    mDNSu32		length;
    
	va_start( args, format );
	length = mDNS_vsnprintf( buffer, sizeof( buffer ), format, args );
	va_end( args );
	
	dlog( kDebugLevelVerbose, "%s\n", buffer );
}
#endif

//===========================================================================================================================
//	LogMsg
//===========================================================================================================================

void LogMsg( const char *inFormat, ... )
{
	char		buffer[ 512 ];
    va_list		args;
    mDNSu32		length;
	
	va_start( args, inFormat );
	length = mDNS_vsnprintf( buffer, sizeof( buffer ), inFormat, args );
	va_end( args );
	
	dlog( kDebugLevelWarning, "%s\n", buffer );
}

#if 0
#pragma mark -
#pragma mark == Platform Internals ==
#endif

//===========================================================================================================================
//	SetupNames
//===========================================================================================================================

mDNSlocal void	SetupNames( mDNS * const inMDNS )
{
	char			tempCString[ 128 ];
	mDNSu8			tempPString[ 128 ];
	mDNSu8 *		namePtr;
	
	// Set up the host name.
	
	tempCString[ 0 ] = '\0';
	GenerateUniqueHostName( tempCString, NULL );
	check( tempCString[ 0 ] != '\0' );
	if( tempCString[ 0 ] == '\0' )
	{
		// No name so use the default.
		
		strcpy( tempCString, kMDNSDefaultName );
	}
	inMDNS->nicelabel.c[ 0 ] = strlen( tempCString );
	memcpy( &inMDNS->nicelabel.c[ 1 ], tempCString, inMDNS->nicelabel.c[ 0 ] );
	check( inMDNS->nicelabel.c[ 0 ] > 0 );
	
	// Set up the DNS name.
	
	tempCString[ 0 ] = '\0';
	GenerateUniqueDNSName( tempCString, NULL );
	if( tempCString[ 0 ] != '\0' )
	{
		tempPString[ 0 ] = strlen( tempCString );
		memcpy( &tempPString[ 1 ], tempCString, tempPString[ 0 ] );
		namePtr = tempPString;
	}
	else
	{
		// No DNS name so use the host name.
		
		namePtr = inMDNS->nicelabel.c;
	}
	ConvertUTF8PstringToRFC1034HostLabel( namePtr, &inMDNS->hostlabel );
	if( inMDNS->hostlabel.c[ 0 ] == 0 )
	{
		// Nice name has no characters that are representable as an RFC 1034 name (e.g. Japanese) so use the default.
		
		MakeDomainLabelFromLiteralString( &inMDNS->hostlabel, kMDNSDefaultName );
	}
	check( inMDNS->hostlabel.c[ 0 ] > 0 );

	mDNS_SetFQDN( inMDNS );
	
	dlog( kDebugLevelInfo, DEBUG_NAME "nice name \"%.*s\"\n", inMDNS->nicelabel.c[ 0 ], &inMDNS->nicelabel.c[ 1 ] );
	dlog( kDebugLevelInfo, DEBUG_NAME "host name \"%.*s\"\n", inMDNS->hostlabel.c[ 0 ], &inMDNS->hostlabel.c[ 1 ] );
}

//===========================================================================================================================
//	SetupInterfaceList
//===========================================================================================================================

mDNSlocal mStatus	SetupInterfaceList( mDNS * const inMDNS )
{
	mStatus						err;
	struct ifaddrs *			addrs;
	struct ifaddrs *			p;
	uint32_t					flagMask;
	uint32_t					flagTest;
	MDNSInterfaceItem **		next;
	MDNSInterfaceItem *			item;
	
	addrs = NULL;
	
	dlog( kDebugLevelVerbose, DEBUG_NAME "setting up interface list\n" );
	check( inMDNS );
	
	// Tear down any existing interfaces that may be set up.
	
	TearDownInterfaceList( inMDNS );
	inMDNS->p->interfaceList = NULL;
	next = &inMDNS->p->interfaceList;
	
	// Set up each interface that is active, multicast-capable, and not the loopback interface or point-to-point.
	
	flagMask = IFF_UP | IFF_MULTICAST | IFF_LOOPBACK | IFF_POINTOPOINT;
	flagTest = IFF_UP | IFF_MULTICAST;
	
	err = getifaddrs( &addrs );
	require_noerr( err, exit );
	
	for( p = addrs; p; p = p->ifa_next )
	{
		if( ( p->ifa_flags & flagMask ) == flagTest )
		{
			err = SetupInterface( inMDNS, p, &item );
			require_noerr( err, exit );
			
			*next = item;
			next = &item->next;
		}
	}
	err = mStatus_NoError;
	
exit:
	if( addrs )
	{
		freeifaddrs( addrs );
	}
	if( err )
	{
		TearDownInterfaceList( inMDNS );
	}
	dlog( kDebugLevelVerbose, DEBUG_NAME "setting up interface list done (err=%ld)\n", err );
	return( err );
}

//===========================================================================================================================
//	TearDownInterfaceList
//===========================================================================================================================

mDNSlocal mStatus	TearDownInterfaceList( mDNS * const inMDNS )
{
	dlog( kDebugLevelVerbose, DEBUG_NAME "tearing down interface list\n" );
	check( inMDNS );
	
	// Tear down all the interfaces.
	
	while( inMDNS->p->interfaceList )
	{
		MDNSInterfaceItem *		item;
		
		item = inMDNS->p->interfaceList;
		inMDNS->p->interfaceList = item->next;
		
		TearDownInterface( inMDNS, item );
	}
	
	dlog( kDebugLevelVerbose, DEBUG_NAME "tearing down interface list done\n" );
	return( mStatus_NoError );
}

//===========================================================================================================================
//	SetupInterface
//===========================================================================================================================

mDNSlocal mStatus	SetupInterface( mDNS * const inMDNS, const struct ifaddrs *inAddr, MDNSInterfaceItem **outItem )
{
	mStatus							err;
	MDNSInterfaceItem *				item;
	MDNSSocketRef					socketRef;
	const struct sockaddr_in *		ipv4, *mask;
	
	dlog( kDebugLevelVerbose, DEBUG_NAME "setting up interface (name=%s)\n", inAddr->ifa_name );
	check( inMDNS );
	check( inAddr );
	check( inAddr->ifa_addr );
	ipv4 = (const struct sockaddr_in *) inAddr->ifa_addr;
	mask = (const struct sockaddr_in *) inAddr->ifa_netmask;
	check( outItem );
	
	// Allocate memory for the info item.
	
	item = (MDNSInterfaceItem *) calloc( 1, sizeof( *item ) );
	require_action( item, exit, err = mStatus_NoMemoryErr );
	strcpy( item->name, inAddr->ifa_name );
	item->multicastSocketRef	= kInvalidSocketRef;
	item->sendingSocketRef		= kInvalidSocketRef;
	
	// Set up the multicast DNS (port 5353) socket for this interface.
	
	err = SetupSocket( inMDNS, inAddr, MulticastDNSPort, &socketRef );
	require_noerr( err, exit );
	item->multicastSocketRef = socketRef;
		
	// Set up the sending socket for this interface.
	
	err = SetupSocket( inMDNS, inAddr, zeroIPPort, &socketRef );
	require_noerr( err, exit );
	item->sendingSocketRef = socketRef;
	
	// Register this interface with mDNS.
	
	item->hostSet.InterfaceID             = (mDNSInterfaceID) item;
	item->hostSet.ip  .type               = mDNSAddrType_IPv4;
	item->hostSet.ip  .ip.v4.NotAnInteger = ipv4->sin_addr.s_addr;
	item->hostSet.mask.type               = mDNSAddrType_IPv4;
	item->hostSet.mask.ip.v4.NotAnInteger = mask->sin_addr.s_addr;
	item->hostSet.ifname[0]               = 0;
	item->hostSet.Advertise               = inMDNS->AdvertiseLocalAddresses;
	item->hostSet.McastTxRx               = mDNStrue;

	err = mDNS_RegisterInterface( inMDNS, &item->hostSet, mDNSfalse );
	require_noerr( err, exit );
	item->hostRegistered = mDNStrue;
	
	dlog( kDebugLevelInfo, DEBUG_NAME "Registered IP address: %u.%u.%u.%u\n", 
		  item->hostSet.ip.ip.v4.b[ 0 ], item->hostSet.ip.ip.v4.b[ 1 ], 
		  item->hostSet.ip.ip.v4.b[ 2 ], item->hostSet.ip.ip.v4.b[ 3 ] );
	
	// Success!
	
	*outItem = item;
	item = NULL;
	
exit:
	if( item )
	{
		TearDownInterface( inMDNS, item );
	}
	dlog( kDebugLevelVerbose, DEBUG_NAME "setting up interface done (name=%s, err=%ld)\n", inAddr->ifa_name, err );
	return( err );
}

//===========================================================================================================================
//	TearDownInterface
//===========================================================================================================================

mDNSlocal mStatus	TearDownInterface( mDNS * const inMDNS, MDNSInterfaceItem *inItem )
{
	MDNSSocketRef		socketRef;
	
	check( inMDNS );
	check( inItem );
	
	// Deregister this interface with mDNS.
	
	dlog( kDebugLevelInfo, DEBUG_NAME "Deregistering IP address: %u.%u.%u.%u\n", 
		  inItem->hostSet.ip.ip.v4.b[ 0 ], inItem->hostSet.ip.ip.v4.b[ 1 ], 
		  inItem->hostSet.ip.ip.v4.b[ 2 ], inItem->hostSet.ip.ip.v4.b[ 3 ] );
	
	if( inItem->hostRegistered )
	{
		inItem->hostRegistered = mDNSfalse;
		mDNS_DeregisterInterface( inMDNS, &inItem->hostSet, mDNSfalse );
	}
	
	// Close the multicast socket.
	
	socketRef = inItem->multicastSocketRef;
	inItem->multicastSocketRef = kInvalidSocketRef;
	if( socketRef != kInvalidSocketRef )
	{
		dlog( kDebugLevelVerbose, DEBUG_NAME "tearing down multicast socket %d\n", socketRef );
		close( socketRef );
	}
		
	// Close the sending socket.
	
	socketRef = inItem->sendingSocketRef;
	inItem->sendingSocketRef = kInvalidSocketRef;
	if( socketRef != kInvalidSocketRef )
	{
		dlog( kDebugLevelVerbose, DEBUG_NAME "tearing down sending socket %d\n", socketRef );
		close( socketRef );
	}
	
	// Free the memory used by the interface info.
	
	free( inItem );	
	return( mStatus_NoError );
}

//===========================================================================================================================
//	SetupSocket
//===========================================================================================================================

mDNSlocal mStatus
	SetupSocket( 
		mDNS * const 			inMDNS, 
		const struct ifaddrs *	inAddr, 
		mDNSIPPort 				inPort, 
		MDNSSocketRef *			outSocketRef  )
{
	mStatus							err;
	MDNSSocketRef					socketRef;
	int								option;
	unsigned char					optionByte;
	struct ip_mreq					mreq;
	const struct sockaddr_in *		ipv4;
	struct sockaddr_in				addr;
	mDNSv4Addr						ip;
	
	dlog( kDebugLevelVerbose, DEBUG_NAME "setting up socket done\n" );
	check( inMDNS );
	check( inAddr );
	check( inAddr->ifa_addr );
	ipv4 = (const struct sockaddr_in *) inAddr->ifa_addr;
	check( outSocketRef );
	
	// Set up a UDP socket for multicast DNS. 
	
	socketRef = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	require_errno_action( socketRef, errno, exit, err = mStatus_UnknownErr );
		
	// A port of zero means this socket is for sending and should be set up for sending. Otherwise, it is for receiving 
	// and should be set up for receiving. The reason for separate sending vs receiving sockets is to workaround problems
	// with VxWorks IP stack when using dynamic IP configuration such as DHCP (problems binding to wildcard IP when the
	// IP address later changes). Since we have to bind the Multicast DNS address to workaround these issues we have to
	// use a separate sending socket since it is illegal to send a packet with a multicast source address (RFC 1122).
	
	if( inPort.NotAnInteger != zeroIPPort.NotAnInteger )
	{	
		// Turn on reuse port option so multiple servers can listen for Multicast DNS packets.
		
		option = 1;
		err = setsockopt( socketRef, SOL_SOCKET, SO_REUSEADDR, (char *) &option, sizeof( option ) );
		check_errno( err, errno );
		
		// Join the all-DNS multicast group so we receive Multicast DNS packets.
		
		ip.NotAnInteger 			= ipv4->sin_addr.s_addr;
		mreq.imr_multiaddr.s_addr 	= AllDNSLinkGroup_v4.ip.v4.NotAnInteger;
		mreq.imr_interface.s_addr 	= ip.NotAnInteger;
		err = setsockopt( socketRef, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq, sizeof( mreq ) );
		check_errno( err, errno );
		
		// Bind to the multicast DNS address and port 5353.
		
		mDNSPlatformMemZero( &addr, sizeof( addr ) );
		addr.sin_family 		= AF_INET;
		addr.sin_port 			= inPort.NotAnInteger;
		addr.sin_addr.s_addr 	= AllDNSLinkGroup_v4.ip.v4.NotAnInteger;
		err = bind( socketRef, (struct sockaddr *) &addr, sizeof( addr ) );
		check_errno( err, errno );
		
		dlog( kDebugLevelVerbose, DEBUG_NAME "setting up socket done (%s, %u.%u.%u.%u:%u, %d)\n", 
			  inAddr->ifa_name, ip.b[ 0 ], ip.b[ 1 ], ip.b[ 2 ], ip.b[ 3 ], ntohs( inPort.NotAnInteger ), socketRef );
	}
	else
	{
		// Bind to the interface address and multicast DNS port.
		
		ip.NotAnInteger 		= ipv4->sin_addr.s_addr;
		mDNSPlatformMemZero( &addr, sizeof( addr ) );
		addr.sin_family 		= AF_INET;
		addr.sin_port 			= MulticastDNSPort.NotAnInteger;
		addr.sin_addr.s_addr 	= ip.NotAnInteger;
		err = bind( socketRef, (struct sockaddr *) &addr, sizeof( addr ) );
		check_errno( err, errno );
		
		// Direct multicast packets to the specified interface.
		
		addr.sin_addr.s_addr = ip.NotAnInteger;
		err = setsockopt( socketRef, IPPROTO_IP, IP_MULTICAST_IF, (char *) &addr.sin_addr, sizeof( addr.sin_addr ) );
		check_errno( err, errno );
		
		// Set the TTL of outgoing unicast packets to 255 (helps against spoofing).
		
		option = 255;
		err = setsockopt( socketRef, IPPROTO_IP, IP_TTL, (char *) &option, sizeof( option ) );
		check_errno( err, errno );
		
		// Set the TTL of outgoing multicast packets to 255 (helps against spoofing).
		
		optionByte = 255;
		err = setsockopt( socketRef, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &optionByte, sizeof( optionByte ) );
		check_errno( err, errno );
		
		// WARNING: Setting this option causes unicast responses to be routed to the wrong interface so they are 
		// WARNING: disabled. These options were only hints to improve 802.11 performance (and not implemented) anyway.
		
#if 0
		// Mark packets as high-throughput/low-delay (i.e. lowest reliability) to maximize 802.11 multicast rate.
		
		option = IPTOS_LOWDELAY | IPTOS_THROUGHPUT;
		err = setsockopt( socketRef, IPPROTO_IP, IP_TOS, (char *) &option, sizeof( option ) );
		check_errno( err, errno );
#endif
	
		dlog( kDebugLevelVerbose, DEBUG_NAME "setting up sending socket done (%s, %u.%u.%u.%u, %d)\n", 
			  inAddr->ifa_name, ip.b[ 0 ], ip.b[ 1 ], ip.b[ 2 ], ip.b[ 3 ], socketRef );
	}
	
	// Success!
	
	*outSocketRef = socketRef;
	socketRef = kInvalidSocketRef;
	err = mStatus_NoError;
	
exit:
	if( socketRef != kInvalidSocketRef )
	{
		close( socketRef );
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
	
	// Clean up any leftover command pipe.
	
	TearDownCommandPipe( inMDNS );
	
	// Create the pipe device and open it.
	
	pipeDevCreate( kMDNSPipeName, kMDNSPipeMessageQueueSize, kMDNSPipeMessageSize );
	
	inMDNS->p->commandPipe = open( kMDNSPipeName, O_RDWR, 0 );
	require_errno_action( inMDNS->p->commandPipe, errno, exit, err = mStatus_UnsupportedErr );
	
	err = mStatus_NoError;
	
exit:
	return( err );
}

//===========================================================================================================================
//	TearDownCommandPipe
//===========================================================================================================================

mDNSlocal mStatus	TearDownCommandPipe( mDNS * const inMDNS )
{
	if( inMDNS->p->commandPipe != ERROR )
	{
		close( inMDNS->p->commandPipe );
#ifdef _WRS_VXWORKS_5_X
		// pipeDevDelete is not defined in older versions of VxWorks
		pipeDevDelete( kMDNSPipeName, FALSE );
#endif
		inMDNS->p->commandPipe = ERROR;
	}	
	return( mStatus_NoError );
}

//===========================================================================================================================
//	SendCommand
//===========================================================================================================================

mDNSlocal mStatus	SendCommand( const mDNS * const inMDNS, MDNSPipeCommandCode inCommandCode )
{
	mStatus		err;
	
	require_action( inMDNS->p->commandPipe != ERROR, exit, err = mStatus_NotInitializedErr );
	
	err = write( inMDNS->p->commandPipe, &inCommandCode, sizeof( inCommandCode ) );
	require_errno( err, errno, exit );
	
	err = mStatus_NoError;
	
exit:
	return( err );
}

//===========================================================================================================================
//	ProcessCommand
//===========================================================================================================================

mDNSlocal mStatus	ProcessCommand( mDNS * const inMDNS )
{
	mStatus					err;
	MDNSPipeCommandCode		commandCode;
	
	require_action( inMDNS->p->commandPipe != ERROR, exit, err = mStatus_NotInitializedErr );
	
	// Read the command code from the pipe and dispatch it.
	
	err = read( inMDNS->p->commandPipe, &commandCode, sizeof( commandCode ) );
	require_errno( err, errno, exit );
	
	switch( commandCode )
	{
		case kMDNSPipeCommandCodeReschedule:
			
			// Reschedule event. Do nothing here, but this will cause mDNS_Execute to run before waiting again.
			
			dlog( kDebugLevelChatty, DEBUG_NAME "reschedule\n" );
			break;
		
		case kMDNSPipeCommandCodeReconfigure:
			ProcessCommandReconfigure( inMDNS );
			break;
		
		case kMDNSPipeCommandCodeQuit:
		
			// Quit requested. Set quit flag and bump the config ID to let the thread exit normally.
			
			dlog( kDebugLevelVerbose, DEBUG_NAME "processing pipe quit command\n" );
			inMDNS->p->quit = mDNStrue;
			++inMDNS->p->configID;
			break;
				
		default:
			dlog( kDebugLevelError, DEBUG_NAME "unknown pipe command code (code=0x%08X)\n", commandCode );
			err = mStatus_BadParamErr;
			goto exit;
			break;
	}
	err = mStatus_NoError;

exit:
	return( err );
}

//===========================================================================================================================
//	ProcessCommandReconfigure
//===========================================================================================================================

mDNSlocal void	ProcessCommandReconfigure( mDNS *inMDNS )
{
	mStatus		err;
	
	dlog( kDebugLevelVerbose, DEBUG_NAME "processing pipe reconfigure command\n" );
	
	// Tear down the existing interfaces and set up new ones using the new IP info.
	
	mDNSPlatformLock( inMDNS );
	
	err = TearDownInterfaceList( inMDNS );
	check_noerr( err );
	
	err = SetupInterfaceList( inMDNS );
	check_noerr( err );
	
	mDNSPlatformUnlock( inMDNS );
	
	// Inform clients of the change.
	
	mDNS_ConfigChanged(m);
	
	// Force mDNS to update.
	
	mDNSCoreMachineSleep( inMDNS, mDNSfalse ); // What is this for? Mac OS X does not do this
	
	// Bump the config ID so the main processing loop detects the configuration change.
	
	++inMDNS->p->configID;
}

#if 0
#pragma mark -
#pragma mark == Threads ==
#endif

//===========================================================================================================================
//	SetupTask
//===========================================================================================================================

mDNSlocal mStatus	SetupTask( mDNS * const inMDNS )
{
	mStatus		err;
	int			task;
	
	dlog( kDebugLevelVerbose, DEBUG_NAME "setting up thread\n" );
	check( inMDNS );
	
	// Create our main thread. Note: The task will save off its ID in the globals. We cannot do it here because the 
	// task invokes code that needs it and the task may begin execution before taskSpawn returns the task ID.
	// This also means code in this thread context cannot rely on the task ID until the task has fully initialized.
	
	task = taskSpawn( kMDNSTaskName, kMDNSTaskPriority, 0, kMDNSTaskStackSize, (FUNCPTR) Task, 
					  (int) inMDNS, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	require_action( task != ERROR, exit, err = mStatus_NoMemoryErr );
	
	err = mStatus_NoError;
	
exit:
	dlog( kDebugLevelVerbose, DEBUG_NAME "setting up thread done (err=%ld, id=%d)\n", err, task );
	return( err );
}

//===========================================================================================================================
//	TearDownTask
//===========================================================================================================================

mDNSlocal mStatus	TearDownTask( mDNS * const inMDNS )
{
	mStatus		err;
	
	dlog( kDebugLevelVerbose, DEBUG_NAME "tearing down thread\n" );
	check( inMDNS );
	
	// Send a quit command to cause the thread to exit.
	
	SendCommand( inMDNS, kMDNSPipeCommandCodeQuit );
	
	// Wait for the thread to signal it has exited. Timeout in 10 seconds to handle a hung thread.
	
	if( inMDNS->p->quitEvent )
	{
		err = semTake( inMDNS->p->quitEvent, sysClkRateGet() * 10 );
		check_noerr( err );
	}
	err = mStatus_NoError;
	
	dlog( kDebugLevelVerbose, DEBUG_NAME "tearing down thread done (err=%ld)\n", err );
	return( err );
}

//===========================================================================================================================
//	Task
//===========================================================================================================================

mDNSlocal void	Task( mDNS *inMDNS )
{
	mStatus					err;
	fd_set					allReadSet;
	MDNSInterfaceItem *		item;
	int						maxSocket;
	long					configID;
	struct timeval			timeout;
	
	dlog( kDebugLevelVerbose, DEBUG_NAME "task starting\n" );
	check( inMDNS );
	
	// Set up everything up.
	
	err = TaskInit( inMDNS );
	require_noerr( err, exit );
	
	// Main Processing Loop.
	
	while( !inMDNS->p->quit )
	{
		// Set up the read set here to avoid the overhead of setting it up each iteration of the main processing loop. 
		// If the configuration changes, the server ID will be bumped, causing this code to set up the read set again.
		
		TaskSetupReadSet( inMDNS, &allReadSet, &maxSocket );
		configID = inMDNS->p->configID;
		dlog( kDebugLevelVerbose, DEBUG_NAME "task starting processing loop (configID=%ld)\n", configID );
		
		while( configID == inMDNS->p->configID )
		{
			mDNSs32		nextTaskTime;
			fd_set		readSet;
			int			n;
			
			// Give the mDNS core a chance to do its work. Reset the rescheduled flag before calling mDNS_Execute
			// so anything that needs processing during or after causes a re-schedule to wake up the thread. The
			// reschedule flag is set to 1 after processing a waking up to prevent redundant reschedules while
			// processing packets. This introduces a window for a race condition because the thread wake-up and
			// reschedule set are not atomic, but this would be benign. Even if the reschedule flag is "corrupted"
			// like this, it would only result in a redundant reschedule since it will loop back to mDNS_Execute.
			
			inMDNS->p->rescheduled = 0;
			nextTaskTime = mDNS_Execute( inMDNS );
			TaskSetupTimeout( inMDNS, nextTaskTime, &timeout );
			
			// Wait until something occurs (e.g. command, incoming packet, or timeout).
			
			readSet = allReadSet;
			n = select( maxSocket + 1, &readSet, NULL, NULL, &timeout );
			inMDNS->p->rescheduled = 1;
			check_errno( n, errno );
			dlog( kDebugLevelChatty - 1, DEBUG_NAME "task select result = %d\n", n );
			if( n == 0 )
			{
				// Next task timeout occurred. Loop back up to give mDNS core a chance to work.
				
				dlog( kDebugLevelChatty, DEBUG_NAME "next task timeout occurred (%ld)\n", mDNS_TimeNow(inMDNS) );
				continue;
			}
			
			// Scan the read set to determine if any sockets have something pending and process them.
			
			n = 0;
			for( item = inMDNS->p->interfaceList; item; item = item->next )
			{
				if( FD_ISSET( item->multicastSocketRef, &readSet ) )
				{
					TaskProcessPacket( inMDNS, item, item->multicastSocketRef );
					++n;
				}
			}
			
			// Check for a pending command and process it.
			
			if( FD_ISSET( inMDNS->p->commandPipe, &readSet ) )
			{
				ProcessCommand( inMDNS );
				++n;
			}
			check( n > 0 );
		}
	}

exit:
	// Signal we've quit.
	
	check( inMDNS->p->quitEvent );
	semGive( inMDNS->p->quitEvent );
	
	dlog( kDebugLevelInfo, DEBUG_NAME "task ended\n" );
}

//===========================================================================================================================
//	TaskInit
//===========================================================================================================================

mDNSlocal mStatus	TaskInit( mDNS *inMDNS )
{
	mStatus		err;
	
	dlog( kDebugLevelVerbose, DEBUG_NAME "task init\n" );
	check( inMDNS->p->readyEvent );
	
	inMDNS->p->task = taskIdSelf();
	
	err = SetupCommandPipe( inMDNS );
	require_noerr( err, exit );
	
	SetupNames( inMDNS );
	
	err = SetupInterfaceList( inMDNS );
	require_noerr( err, exit );
	
exit:
	// Signal the "ready" semaphore to indicate the task initialization code has completed (success or not).
	
	inMDNS->p->taskInitErr = err;
	semGive( inMDNS->p->readyEvent );

	dlog( kDebugLevelVerbose, DEBUG_NAME "task init done (err=%ld)\n", err );
	return( err );
}

//===========================================================================================================================
//	TaskSetupReadSet
//===========================================================================================================================

mDNSlocal void	TaskSetupReadSet( mDNS *inMDNS, fd_set *outReadSet, int *outMaxSocket )
{
	MDNSInterfaceItem *		item;
	int						maxSocket;
	
	dlog( kDebugLevelVerbose, DEBUG_NAME "task setting up read set\n" );
	check( inMDNS );
	check( outReadSet );
	check( outMaxSocket );
	
	// Initialize the read set. Default the max socket to -1 so "maxSocket + 1" (as needed by select) is zero. This 
	// should never happen since we should always have at least one interface, but it's just to be safe.
	
	FD_ZERO( outReadSet );
	maxSocket = -1;
	
	// Add all the receiving sockets to the read set.
	
	for( item = inMDNS->p->interfaceList; item; item = item->next )
	{
		FD_SET( item->multicastSocketRef, outReadSet );
		if( item->multicastSocketRef > maxSocket )
		{
			maxSocket = item->multicastSocketRef;
		}
	}
	
	// Add the command pipe to the read set.
	
	FD_SET( inMDNS->p->commandPipe, outReadSet );
	if( inMDNS->p->commandPipe > maxSocket )
	{
		maxSocket = inMDNS->p->commandPipe;
	}
	check( maxSocket > 0 );
	*outMaxSocket = maxSocket;
	
	dlog( kDebugLevelVerbose, DEBUG_NAME "task setting up read set done (maxSocket=%d)\n", maxSocket );
}

//===========================================================================================================================
//	TaskSetupTimeout
//===========================================================================================================================

mDNSlocal void	TaskSetupTimeout( mDNS *inMDNS, mDNSs32 inNextTaskTime, struct timeval *outTimeout )
{
	mDNSs32		delta;
	
	// Calculate how long to wait before performing idle processing.
	
	delta = inNextTaskTime - mDNS_TimeNow(inMDNS);
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
		
		outTimeout->tv_sec  = delta / mDNSPlatformOneSecond;
		outTimeout->tv_usec = ( ( delta % mDNSPlatformOneSecond ) + 1 ) * gMDNSTicksToMicrosecondsMultiplier;
		
		// Check if the microseconds is more than 1 second. If so, bump the seconds instead.
		
		if( outTimeout->tv_usec >= 1000000L )
		{
			outTimeout->tv_sec += 1;
			outTimeout->tv_usec = 0;
		}
	}
	
	dlog( kDebugLevelChatty, DEBUG_NAME "next task in %ld:%ld seconds (%ld)\n", 
		  outTimeout->tv_sec, outTimeout->tv_usec, inNextTaskTime );
}
//===========================================================================================================================
//	TaskProcessPacket
//===========================================================================================================================

mDNSlocal void	TaskProcessPacket( mDNS *inMDNS, MDNSInterfaceItem *inItem, MDNSSocketRef inSocketRef )
{
	int						n;
	DNSMessage				packet;
	struct sockaddr_in		addr;
	int						addrSize;
	mDNSu8 *				packetEndPtr;
	mDNSAddr				srcAddr;
	mDNSIPPort				srcPort;
	mDNSAddr				dstAddr;
	mDNSIPPort				dstPort;
		
	// Receive the packet.
	
	addrSize = sizeof( addr );
	n = recvfrom( inSocketRef, (char *) &packet, sizeof( packet ), 0, (struct sockaddr *) &addr, &addrSize );
	check( n >= 0 );
	if( n >= 0 )
	{
		// Set up the src/dst/interface info.
		
		srcAddr.type				= mDNSAddrType_IPv4;
		srcAddr.ip.v4.NotAnInteger 	= addr.sin_addr.s_addr;
		srcPort.NotAnInteger		= addr.sin_port;
		dstAddr.type				= mDNSAddrType_IPv4;
		dstAddr.ip.v4				= AllDNSLinkGroup_v4.ip.v4;
		dstPort						= MulticastDNSPort;
		
		dlog( kDebugLevelChatty, DEBUG_NAME "packet received\n" );
		dlog( kDebugLevelChatty, DEBUG_NAME "    size      = %d\n", n );
		dlog( kDebugLevelChatty, DEBUG_NAME "    src       = %u.%u.%u.%u:%hu\n", 
			  srcAddr.ip.v4.b[ 0 ], srcAddr.ip.v4.b[ 1 ], srcAddr.ip.v4.b[ 2 ], srcAddr.ip.v4.b[ 3 ], 
			  ntohs( srcPort.NotAnInteger ) );
		dlog( kDebugLevelChatty, DEBUG_NAME "    dst       = %u.%u.%u.%u:%hu\n", 
			  dstAddr.ip.v4.b[ 0 ], dstAddr.ip.v4.b[ 1 ], dstAddr.ip.v4.b[ 2 ], dstAddr.ip.v4.b[ 3 ], 
			  ntohs( dstPort.NotAnInteger ) );
		dlog( kDebugLevelChatty, DEBUG_NAME "    interface = 0x%08X\n", (int) inItem->hostSet.InterfaceID );
		dlog( kDebugLevelChatty, DEBUG_NAME "--\n" );
		
		// Dispatch the packet to mDNS.
		
		packetEndPtr = ( (mDNSu8 *) &packet ) + n;
		mDNSCoreReceive( inMDNS, &packet, packetEndPtr, &srcAddr, srcPort, &dstAddr, dstPort, inItem->hostSet.InterfaceID );
	}
	
	// Update counters.
	
	inItem->recvCounter			+= 1;
	inItem->recvErrorCounter 	+= ( n < 0 );
}

#if 0
#pragma mark -
#pragma mark == Utilities ==
#endif

#if( TARGET_NON_APPLE )
//===========================================================================================================================
//	GenerateUniqueHostName
//
//	Non-Apple platform stub routine to generate a unique name for the device. Should be implemented to return a unique name.
//===========================================================================================================================

mDNSlocal void	GenerateUniqueHostName( char *outName, long *ioSeed )
{
	DEBUG_UNUSED( ioSeed );
	
	// $$$ Non-Apple Platforms: Fill in appropriate name for device.
	
	mDNSPlatformStrCopy( outName, kMDNSDefaultName );
}

//===========================================================================================================================
//	GenerateUniqueDNSName
//
//	Non-Apple platform stub routine to generate a unique RFC 1034-compatible DNS name for the device. Should be 
//	implemented to return a unique name.
//===========================================================================================================================

mDNSlocal void	GenerateUniqueDNSName( char *outName, long *ioSeed )
{
	DEBUG_UNUSED( ioSeed );
	
	// $$$ Non-Apple Platforms: Fill in appropriate DNS name for device.
	
	mDNSPlatformStrCopy( outName, kMDNSDefaultName );
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	getifaddrs
//===========================================================================================================================

int	getifaddrs( struct ifaddrs **outAddrs )
{
	int						err;
	struct ifaddrs *		head;
	struct ifaddrs **		next;
	struct ifaddrs *		ifa;
	int						i;
	struct ifnet *			ifp;
	char					ipString[ INET_ADDR_LEN ];
	int						n;
	
	head = NULL;
	next = &head;
	
	i = 1;
	for( ;; )
	{
		ifp = ifIndexToIfp( i );
		if( !ifp )
		{
			break;
		}
		++i;
		
		// Allocate and initialize the ifaddrs structure and attach it to the linked list.
		
		ifa = (struct ifaddrs *) calloc( 1, sizeof( struct ifaddrs ) );
		require_action( ifa, exit, err = ENOMEM );
		
		*next = ifa;
		next  = &ifa->ifa_next;
		
		// Fetch the name.
		
		ifa->ifa_name = (char *) malloc( 16 );
		require_action( ifa->ifa_name, exit, err = ENOMEM );
		
		n = sprintf( ifa->ifa_name, "%s%d", ifp->if_name, ifp->if_unit );
		require_action( n < 16, exit, err = ENOBUFS );
		
		// Fetch the address.
		
		ifa->ifa_addr = (struct sockaddr *) calloc( 1, sizeof( struct sockaddr_in ) );
		require_action( ifa->ifa_addr, exit, err = ENOMEM );
		
		ipString[ 0 ] = '\0';
		#if( TARGET_NON_APPLE )
			err = ifAddrGet( ifa->ifa_name, ipString );
			require_noerr( err, exit );
		#else
			err = ifAddrGetNonAlias( ifa->ifa_name, ipString );
			require_noerr( err, exit );
		#endif
		
		err = sock_pton( ipString, AF_INET, ifa->ifa_addr, 0, NULL );
		require_noerr( err, exit );
		
		// Fetch flags.
		
		ifa->ifa_flags = ifp->if_flags;
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
	return( err );
}

//===========================================================================================================================
//	freeifaddrs
//===========================================================================================================================

void	freeifaddrs( struct ifaddrs *inAddrs )
{
	struct ifaddrs *		p;
	struct ifaddrs *		q;
	
	// Free each piece of the structure. Set to null after freeing to handle macro-aliased fields.
	
	for( p = inAddrs; p; p = q )
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
//	sock_pton
//===========================================================================================================================

int	sock_pton( const char *inString, int inFamily, void *outAddr, size_t inAddrSize, size_t *outAddrSize )
{
	int		err;
	
	if( inFamily == AF_INET )
	{
		struct sockaddr_in *		ipv4;
		
		if( inAddrSize == 0 )
		{
			inAddrSize = sizeof( struct sockaddr_in );
		}
		if( inAddrSize < sizeof( struct sockaddr_in ) )
		{
			err = EINVAL;
			goto exit;
		}
		
		ipv4 = (struct sockaddr_in *) outAddr;
		err = inet_aton( (char *) inString, &ipv4->sin_addr );
		if( err == 0 )
		{
			ipv4->sin_family = AF_INET;
			if( outAddrSize )
			{
				*outAddrSize = sizeof( struct sockaddr_in );
			}
		}
	}
#if( defined( AF_INET6 ) )
	else if( inFamily == AF_INET6 )		// $$$ TO DO: Add IPv6 support.
	{
		err = EAFNOSUPPORT;
		goto exit;
	}
#endif
	else
	{
		err = EAFNOSUPPORT;
		goto exit;
	}

exit:
	return( err );
}

//===========================================================================================================================
//	sock_ntop
//===========================================================================================================================

char *	sock_ntop( const void *inAddr, size_t inAddrSize, char *inBuffer, size_t inBufferSize )
{
	const struct sockaddr *		addr;
	
	addr = (const struct sockaddr *) inAddr;
	if( addr->sa_family == AF_INET )
	{
		struct sockaddr_in *		ipv4;
		
		if( inAddrSize == 0 )
		{
			inAddrSize = sizeof( struct sockaddr_in );
		}
		if( inAddrSize < sizeof( struct sockaddr_in ) )
		{
			errno = EINVAL;
			inBuffer = NULL;
			goto exit;
		}
		if( inBufferSize < 16 )
		{
			errno = ENOBUFS;
			inBuffer = NULL;
			goto exit;
		}
		
		ipv4 = (struct sockaddr_in *) addr;
		inet_ntoa_b( ipv4->sin_addr, inBuffer );
	}
#if( defined( AF_INET6 ) )
	else if( addr->sa_family == AF_INET6 )	// $$$ TO DO: Add IPv6 support.
	{		
		errno = EAFNOSUPPORT;
		inBuffer = NULL;
		goto exit;
	}
#endif
	else
	{
		errno = EAFNOSUPPORT;
		inBuffer = NULL;
		goto exit;
	}
	
exit:
	return( inBuffer );
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( DEBUG )

void	mDNSShow( BOOL inShowRecords );
void	mDNSShowRecords( void );
void	mDNSShowTXT( const void *inTXT, size_t inTXTSize );

//===========================================================================================================================
//	mDNSShow
//===========================================================================================================================

void	mDNSShow( BOOL inShowRecords )
{
	MDNSInterfaceItem *		item;
	mDNSAddr				ip;
	int						n;
	
	if( !gMDNSPtr )
	{
		printf( "### mDNS not initialized\n" );
		return;
	}
	
	// Globals
	
	printf( "\n-- mDNS globals --\n" );
	printf( "    sizeof( mDNS )                     = %d\n", (int) sizeof( mDNS ) );
	printf( "    sizeof( ResourceRecord )           = %d\n", (int) sizeof( ResourceRecord ) );
	printf( "    sizeof( AuthRecord )               = %d\n", (int) sizeof( AuthRecord ) );
	printf( "    sizeof( CacheRecord )              = %d\n", (int) sizeof( CacheRecord ) );
	printf( "    gMDNSPtr                           = 0x%08lX\n", (unsigned long) gMDNSPtr );
	printf( "    gMDNSTicksToMicrosecondsMultiplier = %ld\n", gMDNSTicksToMicrosecondsMultiplier );
	printf( "    lockID                             = 0x%08lX\n", (unsigned long) gMDNSPtr->p->lockID );
	printf( "    readyEvent                         = 0x%08lX\n", (unsigned long) gMDNSPtr->p->readyEvent );
	printf( "    taskInitErr                        = %ld\n", gMDNSPtr->p->taskInitErr );
	printf( "    quitEvent                          = 0x%08lX\n", (unsigned long) gMDNSPtr->p->quitEvent );
	printf( "    commandPipe                        = %d\n", gMDNSPtr->p->commandPipe );
	printf( "    task                               = 0x%08lX\n", (unsigned long) gMDNSPtr->p->task );
	printf( "    quit                               = %d\n", gMDNSPtr->p->quit );
	printf( "    configID                           = %ld\n", gMDNSPtr->p->configID );
	printf( "    rescheduled                        = %d\n", gMDNSPtr->p->rescheduled );
	printf( "    nicelabel                          = \"%.*s\"\n", gMDNSPtr->nicelabel.c[ 0 ], (char *) &gMDNSPtr->nicelabel.c[ 1 ] );
	printf( "    hostLabel                          = \"%.*s\"\n", gMDNSPtr->hostlabel.c[ 0 ], (char *) &gMDNSPtr->hostlabel.c[ 1 ] );
	printf( "\n");
	
	// Interfaces
	
	printf( "\n-- mDNS interfaces --\n" );
	n = 1;
	for( item = gMDNSPtr->p->interfaceList; item; item = item->next )
	{
		printf( "    -- interface %u --\n", n );
		printf( "        name                           = \"%s\"\n", item->name );
		printf( "        multicastSocketRef             = %d\n", item->multicastSocketRef );
		printf( "        sendingSocketRef               = %d\n", item->sendingSocketRef );
		ip = item->hostSet.ip;
		printf( "        hostSet.ip                     = %u.%u.%u.%u\n", ip.ip.v4.b[ 0 ], ip.ip.v4.b[ 1 ], 
																		  ip.ip.v4.b[ 2 ], ip.ip.v4.b[ 3 ] );
		printf( "        hostSet.advertise              = %s\n", item->hostSet.Advertise ? "YES" : "NO" );
		printf( "        hostRegistered                 = %s\n", item->hostRegistered ? "YES" : "NO" );
		printf( "        --\n" );
		printf( "        sendMulticastCounter           = %d\n", item->sendMulticastCounter );
		printf( "        sendUnicastCounter             = %d\n", item->sendUnicastCounter );
		printf( "        sendErrorCounter               = %d\n", item->sendErrorCounter );
		printf( "        recvCounter                    = %d\n", item->recvCounter );
		printf( "        recvErrorCounter               = %d\n", item->recvErrorCounter );
		printf( "        recvLoopCounter                = %d\n", item->recvLoopCounter );
		printf( "\n" );
		++n;
	}
	
	// Resource Records
	
	if( inShowRecords )
	{
		mDNSShowRecords();
	}
}

//===========================================================================================================================
//	mDNSShowRecords
//===========================================================================================================================

void	mDNSShowRecords( void )
{
	MDNSInterfaceItem *		item;
	int						n;
	AuthRecord *			record;
	char					name[ MAX_ESCAPED_DOMAIN_NAME ];
	
	printf( "\n-- mDNS resource records --\n" );
	n = 1;
	for( record = gMDNSPtr->ResourceRecords; record; record = record->next )
	{
		item = (MDNSInterfaceItem *) record->resrec.InterfaceID;
		ConvertDomainNameToCString( &record->resrec.name, name );
		printf( "    -- record %d --\n", n );
		printf( "        interface = 0x%08X (%s)\n", (int) item, item ? item->name : "<any>" );
		printf( "        name      = \"%s\"\n", name );
		printf( "\n" );
		++n;
	}
	printf( "\n");
}

//===========================================================================================================================
//	mDNSShowTXT
//===========================================================================================================================

void	mDNSShowTXT( const void *inTXT, size_t inTXTSize )
{
	const mDNSu8 *		p;
	const mDNSu8 *		end;
	int					i;
	mDNSu8				size;
	
	printf( "\nTXT record (%u bytes):\n\n", inTXTSize );
	
	p 	= (const mDNSu8 *) inTXT;
	end = p + inTXTSize;
	i 	= 0;
	
	while( p < end )
	{
		size = *p++;
		if( ( p + size ) > end )
		{
			printf( "\n### MALFORMED TXT RECORD (length byte too big for record)\n\n" );
			break;
		}
		printf( "%2d (%3d bytes): \"%.*s\"\n", i, size, size, p );
		p += size;
		++i;
	}
	printf( "\n" );
}
#endif	// DEBUG
