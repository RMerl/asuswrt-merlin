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
    
$Log: mDNSWin32.h,v $
Revision 1.30  2009/07/17 19:59:46  herscher
<rdar://problem/7062660> Update the womp settings for each network adapter immediately preceding the call to mDNSCoreMachineSleep().

Revision 1.29  2009/07/07 21:34:58  herscher
<rdar://problem/6713286> windows platform changes to support use as sleep proxy client

Revision 1.28  2009/04/24 04:55:26  herscher
<rdar://problem/3496833> Advertise SMB file sharing via Bonjour

Revision 1.27  2009/03/30 20:45:52  herscher
<rdar://problem/5925472> Current Bonjour code does not compile on Windows
<rdar://problem/6127927> B4Windows: uDNS: Should use randomized source ports and transaction IDs to avoid DNS cache poisoning
Remove VirtualPC workaround

Revision 1.26  2006/08/14 23:25:21  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.25  2006/07/06 00:05:44  cheshire
"dDNS.h" renamed to "uDNS.h"

Revision 1.24  2006/02/26 19:31:04  herscher
<rdar://problem/4455038> Bonjour For Windows takes 90 seconds to start. This was caused by a bad interaction between the VirtualPC check, and the removal of the WMI dependency.  The problem was fixed by: 1) checking to see if WMI is running before trying to talk to it.  2) Retrying the VirtualPC check every 10 seconds upon failure, stopping after 10 unsuccessful tries.

Revision 1.23  2005/10/05 20:55:14  herscher
<rdar://problem/4096464> Don't call SetLLRoute on loopback interface

Revision 1.22  2005/03/04 22:44:53  shersche
<rdar://problem/4022802> mDNSResponder did not notice changes to DNS server config

Revision 1.21  2005/03/03 02:29:00  shersche
Use the RegNames.h header file for registry key names

Revision 1.20  2005/01/25 08:12:52  shersche
<rdar://problem/3947417> Enable Unicast and add Dynamic DNS support.
Bug #: 3947417

Revision 1.19  2004/12/15 07:34:45  shersche
Add platform support for IPv4 and IPv6 unicast sockets

Revision 1.18  2004/10/11 21:53:15  shersche
<rdar://problem/3832450> Change GetWindowsVersionString link scoping from static to non-static so that it can be accessed from other compilation units. The information returned in this function will be used to determine what service dependencies to use when calling CreateService().
Bug #: 3832450

Revision 1.17  2004/09/17 01:08:57  cheshire
Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
  The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
  declared in that file are ONLY appropriate to single-address-space embedded applications.
  For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.

Revision 1.16  2004/08/05 05:43:01  shersche
<rdar://problem/3751566> Add HostDescriptionChangedCallback so callers can choose to handle it when mDNSWin32 core detects that the computer description string has changed
Bug #: 3751566

Revision 1.15  2004/07/26 05:42:50  shersche
use "Computer Description" for nicename if available, track dynamic changes to "Computer Description"

Revision 1.14  2004/07/13 21:24:25  rpantos
Fix for <rdar://problem/3701120>.

Revision 1.13  2004/06/24 15:23:24  shersche
Add InterfaceListChanged callback.  This callback is used in Service.c to add link local routes to the routing table
Submitted by: herscher

Revision 1.12  2004/06/18 05:22:16  rpantos
Integrate Scott's changes

Revision 1.11  2004/01/30 02:44:32  bradley
Added support for IPv6 (v4 & v6, v4-only, v6-only, AAAA over v4, etc.). Added support for DNS-SD
InterfaceID<->Interface Index mappings. Added support for loopback usage when no other interfaces
are available. Updated unlock signaling to no longer require timenow - NextScheduledTime to be >= 0
(it no longer is). Added unicast-capable detection to avoid using unicast when there is other mDNS
software running on the same machine. Removed unneeded sock_XtoY routines. Added support for
reporting HINFO records with the  Windows and mDNSResponder version information.

Revision 1.10  2003/10/24 23:23:02  bradley
Removed legacy port 53 support as it is no longer needed.

Revision 1.9  2003/08/20 06:21:25  bradley
Updated to latest internal version of the mDNSWindows platform layer: Added support
for Windows CE/PocketPC 2003; re-did interface-related code to emulate getifaddrs/freeifaddrs for
restricting usage to only active, multicast-capable, and non-point-to-point interfaces and to ease
the addition of IPv6 support in the future; Changed init code to serialize thread initialization to
enable ThreadID improvement to wakeup notification; Define platform support structure locally to
allow portable mDNS_Init usage; Removed dependence on modified mDNSCore: define interface ID<->name
structures/prototypes locally; Changed to use _beginthreadex()/_endthreadex() on non-Windows CE
platforms (re-mapped to CreateThread on Window CE) to avoid a leak in the Microsoft C runtime;
Added IPv4/IPv6 string<->address conversion routines; Cleaned up some code and added HeaderDoc.

Revision 1.8  2003/08/12 19:56:27  cheshire
Update to APSL 2.0

Revision 1.7  2003/07/23 02:23:01  cheshire
Updated mDNSPlatformUnlock() to work correctly, now that <rdar://problem/3160248>
"ScheduleNextTask needs to be smarter" has refined the way m->NextScheduledEvent is set

Revision 1.6  2003/07/02 21:20:04  cheshire
<rdar://problem/3313413> Update copyright notices, etc., in source code comments

Revision 1.5  2003/04/29 00:06:09  cheshire
<rdar://problem/3242673> mDNSWindows needs a wakeupEvent object to signal the main thread

Revision 1.4  2003/03/22 02:57:44  cheshire
Updated mDNSWindows to use new "mDNS_Execute" model (see "mDNSCore/Implementer Notes.txt")

Revision 1.3  2002/09/21 20:44:54  zarzycki
Added APSL info

Revision 1.2  2002/09/20 05:55:16  bradley
Multicast DNS platform plugin for Win32

*/

#ifndef	__MDNS_WIN32__
#define	__MDNS_WIN32__

#include	"CommonServices.h"

#if( !defined( _WIN32_WCE ) )
	#include	<mswsock.h>
#endif

#include	"mDNSEmbeddedAPI.h"
#include	"uDNS.h"

#ifdef	__cplusplus
	extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		mDNSInterfaceData

	@abstract	Structure containing interface-specific data.
*/

typedef struct	mDNSInterfaceData	mDNSInterfaceData;
struct	mDNSInterfaceData
{
	mDNSInterfaceData *			next;
	char						name[ 128 ];
	uint32_t					index;
	uint32_t					scopeID;
	SocketRef					sock;
#if( !defined( _WIN32_WCE ) )
	LPFN_WSARECVMSG				wsaRecvMsgFunctionPtr;
#endif
	HANDLE						readPendingEvent;
	NetworkInterfaceInfo		interfaceInfo;
	mDNSAddr					defaultAddr;
	mDNSBool					hostRegistered;
};

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	IdleThreadCallback

	@abstract	mDNSWin32 core will call out through this function pointer
				after calling mDNS_Execute
*/
typedef mDNSs32 (*IdleThreadCallback)(mDNS * const inMDNS, mDNSs32 interval);
//---------------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	InterfaceListChangedCallback

	@abstract	mDNSWin32 core will call out through this function pointer
				after detecting an interface list changed event
*/
typedef void (*InterfaceListChangedCallback)(mDNS * const inMDNS);
//---------------------------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	HostDescriptionChangedCallback

	@abstract	mDNSWin32 core will call out through this function pointer
				after detecting that the computer description has changed
*/
typedef void (*HostDescriptionChangedCallback)(mDNS * const inMDNS);
//---------------------------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		mDNS_PlatformSupport_struct

	@abstract	Structure containing platform-specific data.
*/

struct	mDNS_PlatformSupport_struct
{
	CRITICAL_SECTION			lock;
	mDNSBool					lockInitialized;
	HANDLE						cancelEvent;
	HANDLE						quitEvent;
	HANDLE						interfaceListChangedEvent;
	HANDLE						descChangedEvent;	// Computer description changed event
	HANDLE						tcpipChangedEvent;	// TCP/IP config changed
	HANDLE						ddnsChangedEvent;	// DynDNS config changed
	HANDLE						fileShareEvent;		// File Sharing changed
	HANDLE						firewallEvent;		// Firewall changed
	HANDLE						wakeupEvent;
	HANDLE						initEvent;
	time_t						nextDHCPLeaseExpires;
	mDNSBool					womp;				// Does any interface support wake-on-magic-packet
	HKEY						descKey;
	HKEY						tcpipKey;
	HKEY						ddnsKey;
	HKEY						fileShareKey;
	HKEY						firewallKey;
	mDNSBool					smbRegistered;
	ServiceRecordSet			smbSRS;
	mStatus						initStatus;
	mDNSBool					registeredLoopback4;
	SocketRef					interfaceListChangedSocket;
	int							interfaceCount;
	mDNSInterfaceData *			interfaceList;
	mDNSInterfaceData *			inactiveInterfaceList;
	DWORD						threadID;
	IdleThreadCallback			idleThreadCallback;
	InterfaceListChangedCallback	interfaceListChangedCallback;
	HostDescriptionChangedCallback	hostDescriptionChangedCallback;
	SocketRef						unicastSock4;
	HANDLE							unicastSock4ReadEvent;
	mDNSAddr						unicastSock4DestAddr;
#if( !defined( _WIN32_WCE ) )
	LPFN_WSARECVMSG					unicastSock4RecvMsgPtr;
#endif
	SocketRef						unicastSock6;
	HANDLE							unicastSock6ReadEvent;
	mDNSAddr						unicastSock6DestAddr;
#if( !defined( _WIN32_WCE ) )
	LPFN_WSARECVMSG					unicastSock6RecvMsgPtr;
#endif
};

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		ifaddrs

	@abstract	Interface information
*/

struct ifaddrs
{
	struct ifaddrs *	ifa_next;
	char *				ifa_name;
	u_int				ifa_flags;
	struct sockaddr	*	ifa_addr;
	struct sockaddr	*	ifa_netmask;
	struct sockaddr	*	ifa_broadaddr;
	struct sockaddr	*	ifa_dstaddr;
	BOOL				ifa_dhcpEnabled;
	time_t				ifa_dhcpLeaseExpires;
	mDNSu8				ifa_womp;
	void *				ifa_data;
	
	struct
	{
		uint32_t		index;
	
	}	ifa_extra;
};


extern void UpdateWOMPConfig();


#ifdef	__cplusplus
	}
#endif

#endif	// __MDNS_WIN32__
