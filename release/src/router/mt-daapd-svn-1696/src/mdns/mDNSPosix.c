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
 *
 * Formatting notes:
 * This code follows the "Whitesmiths style" C indentation rules. Plenty of discussion
 * on C indentation can be found on the web, such as <http://www.kafejo.com/komp/1tbs.htm>,
 * but for the sake of brevity here I will say just this: Curly braces are not syntactially
 * part of an "if" statement; they are the beginning and ending markers of a compound statement;
 * therefore common sense dictates that if they are part of a compound statement then they
 * should be indented to the same level as everything else in that compound statement.
 * Indenting curly braces at the same level as the "if" implies that curly braces are
 * part of the "if", which is false. (This is as misleading as people who write "char* x,y;"
 * thinking that variables x and y are both of type "char*" -- and anyone who doesn't
 * understand why variable y is not of type "char*" just proves the point that poor code
 * layout leads people to unfortunate misunderstandings about how the C language really works.)

	Change History (most recent first):

$Log: mDNSPosix.c,v $
Revision 1.78.2.1  2006/08/29 06:24:34  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.78  2006/06/28 09:12:22  cheshire
Added debugging message

Revision 1.77  2006/03/19 02:00:11  cheshire
<rdar://problem/4073825> Improve logic for delaying packets after repeated interface transitions

Revision 1.76  2006/01/09 19:29:16  cheshire
<rdar://problem/4403128> Cap number of "sendto failed" messages we allow mDNSResponder to log

Revision 1.75  2006/01/05 22:04:57  cheshire
<rdar://problem/4399479> Log error message when send fails with "operation not permitted"

Revision 1.74  2006/01/05 21:45:27  cheshire
<rdar://problem/4400118> Fix uninitialized structure member in IPv6 code

Revision 1.73  2005/10/11 21:31:46  cheshire
<rdar://problem/4296177> Don't depend on IP_RECVTTL succeeding (not available on all platforms)

Revision 1.72  2005/09/08 20:45:26  cheshire
Default dot-local host name should be "Computer" not "Macintosh",
since the machine this is running on is most likely NOT a Mac.

Revision 1.71  2005/02/26 01:29:12  cheshire
Ignore multicasts accidentally delivered to our unicast receiving socket

Revision 1.70  2005/02/04 00:39:59  cheshire
Move ParseDNSServers() from PosixDaemon.c to mDNSPosix.c so all Posix client layers can use it

Revision 1.69  2004/12/18 02:03:28  cheshire
Need to #include "dns_sd.h"

Revision 1.68  2004/12/18 00:51:52  cheshire
Use symbolic constant kDNSServiceInterfaceIndexLocalOnly instead of (mDNSu32) ~0

Revision 1.67  2004/12/17 23:37:48  cheshire
<rdar://problem/3485365> Guard against repeating wireless dissociation/re-association
(and other repetitive configuration changes)

Revision 1.66  2004/12/01 04:27:28  cheshire
<rdar://problem/3872803> Darwin patches for Solaris and Suse
Don't use uint32_t, etc. -- they require stdint.h, which doesn't exist on FreeBSD 4.x, Solaris, etc.

Revision 1.65  2004/11/30 22:37:01  cheshire
Update copyright dates and add "Mode: C; tab-width: 4" headers

Revision 1.64  2004/11/23 03:39:47  cheshire
Let interface name/index mapping capability live directly in JNISupport.c,
instead of having to call through to the daemon via IPC to get this information.

Revision 1.63  2004/11/12 03:16:43  rpantos
rdar://problem/3809541 Add mDNSPlatformGetInterfaceByName, mDNSPlatformGetInterfaceName

Revision 1.62  2004/10/28 03:24:42  cheshire
Rename m->CanReceiveUnicastOn as m->CanReceiveUnicastOn5353

Revision 1.61  2004/10/16 00:17:01  cheshire
<rdar://problem/3770558> Replace IP TTL 255 check with local subnet source address check

Revision 1.60  2004/09/26 23:20:36  ksekar
<rdar://problem/3813108> Allow default registrations in multiple wide-area domains

Revision 1.59  2004/09/21 21:02:55  cheshire
Set up ifname before calling mDNS_RegisterInterface()

Revision 1.58  2004/09/17 01:08:54  cheshire
Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
  The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
  declared in that file are ONLY appropriate to single-address-space embedded applications.
  For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.

Revision 1.57  2004/09/17 00:19:11  cheshire
For consistency with AllDNSLinkGroupv6, rename AllDNSLinkGroup to AllDNSLinkGroupv4

Revision 1.56  2004/09/17 00:15:56  cheshire
Rename mDNSPlatformInit_ReceiveUnicast to mDNSPlatformInit_CanReceiveUnicast

Revision 1.55  2004/09/16 00:24:49  cheshire
<rdar://problem/3803162> Fix unsafe use of mDNSPlatformTimeNow()

Revision 1.54  2004/09/15 23:55:00  ksekar
<rdar://problem/3800597> mDNSPosix should #include stdint.h

Revision 1.53  2004/09/14 23:42:36  cheshire
<rdar://problem/3801296> Need to seed random number generator from platform-layer data

Revision 1.52  2004/08/25 16:42:13  ksekar
Fix Posix build - change mDNS_SetFQDNs to mDNS_SetFQDN, remove unicast
hostname parameter.

Revision 1.51  2004/08/14 03:22:42  cheshire
<rdar://problem/3762579> Dynamic DNS UI <-> mDNSResponder glue
Add GetUserSpecifiedDDNSName() routine
Convert ServiceRegDomain to domainname instead of C string
Replace mDNS_GenerateFQDN/mDNS_GenerateGlobalFQDN with mDNS_SetFQDNs

Revision 1.50  2004/08/11 01:20:20  cheshire
Declare private local functions using "mDNSlocal"

Revision 1.49  2004/07/26 22:49:31  ksekar
<rdar://problem/3651409>: Feature #9516: Need support for NATPMP in client

Revision 1.48  2004/07/20 01:47:36  rpantos
NOT_HAVE_SA_LEN applies to v6, too. And use more-portable s6_addr.

Revision 1.47  2004/06/25 00:26:27  rpantos
Changes to fix the Posix build on Solaris.

Revision 1.46  2004/05/13 04:54:20  ksekar
Unified list copy/free code.  Added symetric list for

Revision 1.45  2004/05/12 22:03:09  ksekar
Made GetSearchDomainList a true platform-layer call (declaration moved
from mDNSMacOSX.h to mDNSEmbeddedAPI.h), impelemted to return "local"
only on non-OSX platforms.  Changed call to return a copy of the list
to avoid shared memory issues.  Added a routine to free the list.

Revision 1.44  2004/04/21 02:49:11  cheshire
To reduce future confusion, renamed 'TxAndRx' to 'McastTxRx'

Revision 1.43  2004/04/14 23:09:29  ksekar
Support for TSIG signed dynamic updates.

Revision 1.42  2004/04/09 17:43:04  cheshire
Make sure to set the McastTxRx field so that duplicate suppression works correctly

Revision 1.41  2004/02/06 01:19:51  cheshire
Conditionally exclude IPv6 code unless HAVE_IPV6 is set

Revision 1.40  2004/02/05 01:00:01  rpantos
Fix some issues that turned up when building for FreeBSD.

Revision 1.39  2004/01/28 21:12:15  cheshire
Reconcile mDNSIPv6Support & HAVE_IPV6 into a single flag (HAVE_IPV6)

Revision 1.38  2004/01/27 20:15:23  cheshire
<rdar://problem/3541288>: Time to prune obsolete code for listening on port 53

Revision 1.37  2004/01/24 05:12:03  cheshire
<rdar://problem/3534352>: Need separate socket for issuing unicast queries

Revision 1.36  2004/01/24 04:59:16  cheshire
Fixes so that Posix/Linux, OS9, Windows, and VxWorks targets build again

Revision 1.35  2004/01/23 21:37:08  cheshire
For consistency, rename multicastSocket to multicastSocket4, and multicastSocketv6 to multicastSocket6

Revision 1.34  2004/01/22 03:43:09  cheshire
Export constants like mDNSInterface_LocalOnly so that the client layers can use them

Revision 1.33  2004/01/21 21:54:20  cheshire
<rdar://problem/3448144>: Don't try to receive unicast responses if we're not the first to bind to the UDP port

Revision 1.32  2004/01/20 01:49:28  rpantos
Tweak error handling of last checkin a bit.

Revision 1.31  2004/01/20 01:39:27  rpantos
Respond to If changes by rebuilding interface list.

Revision 1.30  2003/12/11 19:40:36  cheshire
Fix 'destAddr.type == senderAddr.type;' that should have said 'destAddr.type = senderAddr.type;'

Revision 1.29  2003/12/11 18:53:22  cheshire
Fix compiler warning reported by Paul Guyot

Revision 1.28  2003/12/11 03:03:51  rpantos
Clean up mDNSPosix so that it builds on OS X again.

Revision 1.27  2003/12/08 20:47:02  rpantos
Add support for mDNSResponder on Linux.

Revision 1.26  2003/11/14 20:59:09  cheshire
Clients can't use AssignDomainName macro because mDNSPlatformMemCopy is defined in mDNSPlatformFunctions.h.
Best solution is just to combine mDNSEmbeddedAPI.h and mDNSPlatformFunctions.h into a single file.

Revision 1.25  2003/10/30 19:25:49  cheshire
Fix signed/unsigned warning on certain compilers

Revision 1.24  2003/08/18 23:12:23  cheshire
<rdar://problem/3382647> mDNSResponder divide by zero in mDNSPlatformRawTime()

Revision 1.23  2003/08/12 19:56:26  cheshire
Update to APSL 2.0

Revision 1.22  2003/08/06 18:46:15  cheshire
LogMsg() errors are serious -- always report them to stderr, regardless of debugging level

Revision 1.21  2003/08/06 18:20:51  cheshire
Makefile cleanup

Revision 1.20  2003/08/05 23:56:26  cheshire
Update code to compile with the new mDNSCoreReceive() function that requires a TTL
(Right now mDNSPosix.c just reports 255 -- we should fix this)

Revision 1.19  2003/07/19 03:15:16  cheshire
Add generic MemAllocate/MemFree prototypes to mDNSPlatformFunctions.h,
and add the obvious trivial implementations to each platform support layer

Revision 1.18  2003/07/14 18:11:54  cheshire
Fix stricter compiler warnings

Revision 1.17  2003/07/13 01:08:38  cheshire
There's not much point running mDNS over a point-to-point link; exclude those

Revision 1.16  2003/07/02 21:19:59  cheshire
<rdar://problem/3313413> Update copyright notices, etc., in source code comments

Revision 1.15  2003/06/18 05:48:41  cheshire
Fix warnings

Revision 1.14  2003/05/26 03:21:30  cheshire
Tidy up address structure naming:
mDNSIPAddr         => mDNSv4Addr (for consistency with mDNSv6Addr)
mDNSAddr.addr.ipv4 => mDNSAddr.ip.v4
mDNSAddr.addr.ipv6 => mDNSAddr.ip.v6

Revision 1.13  2003/05/26 03:01:28  cheshire
<rdar://problem/3268904> sprintf/vsprintf-style functions are unsafe; use snprintf/vsnprintf instead

Revision 1.12  2003/05/21 03:49:18  cheshire
Fix warning

Revision 1.11  2003/05/06 00:00:50  cheshire
<rdar://problem/3248914> Rationalize naming of domainname manipulation functions

Revision 1.10  2003/04/25 01:45:57  cheshire
<rdar://problem/3240002> mDNS_RegisterNoSuchService needs to include a host name

Revision 1.9  2003/03/20 21:10:31  cheshire
Fixes done at IETF 56 to make mDNSProxyResponderPosix run on Solaris

Revision 1.8  2003/03/15 04:40:38  cheshire
Change type called "mDNSOpaqueID" to the more descriptive name "mDNSInterfaceID"

Revision 1.7  2003/03/13 03:46:21  cheshire
Fixes to make the code build on Linux

Revision 1.6  2003/03/08 00:35:56  cheshire
Switched to using new "mDNS_Execute" model (see "mDNSCore/Implementer Notes.txt")

Revision 1.5  2002/12/23 22:13:31  jgraessl
Reviewed by: Stuart Cheshire
Initial IPv6 support for mDNSResponder.

Revision 1.4  2002/09/27 01:47:45  cheshire
Workaround for Linux 2.0 systems that don't have IP_PKTINFO

Revision 1.3  2002/09/21 20:44:53  zarzycki
Added APSL info

Revision 1.2  2002/09/19 21:25:36  cheshire
mDNS_snprintf() doesn't need to be in a separate file

Revision 1.1  2002/09/17 06:24:34  cheshire
First checkin
*/

#include "mDNSEmbeddedAPI.h"           // Defines the interface provided to the client layer above
#include "mDNSPosix.h"				 // Defines the specific types needed to run mDNS on this platform
#include "dns_sd.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>                   // platform support for UTC time

#if USES_NETLINK
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#else // USES_NETLINK
#include <net/route.h>
#include <net/if.h>
#endif // USES_NETLINK

#include "mDNSUNP.h"
#include "GenLinkedList.h"

// ***************************************************************************
// Structures

// We keep a list of client-supplied event sources in PosixEventSource records 
struct PosixEventSource
	{
	mDNSPosixEventCallback		Callback;
	void						*Context;
	int							fd;
	struct  PosixEventSource	*Next;
	};
typedef struct PosixEventSource	PosixEventSource;

// Context record for interface change callback
struct IfChangeRec
	{
	int			NotifySD;
	mDNS*		mDNS;
	};
typedef struct IfChangeRec	IfChangeRec;

// Note that static data is initialized to zero in (modern) C.
static fd_set			gEventFDs;
static int				gMaxFD;					// largest fd in gEventFDs
static GenLinkedList	gEventSources;			// linked list of PosixEventSource's
static sigset_t			gEventSignalSet;		// Signals which event loop listens for
static sigset_t			gEventSignals;			// Signals which were received while inside loop

// ***************************************************************************
// Globals (for debugging)

static int num_registered_interfaces = 0;
static int num_pkts_accepted = 0;
static int num_pkts_rejected = 0;

// ***************************************************************************
// Functions

int gMDNSPlatformPosixVerboseLevel = 0;

#define PosixErrorToStatus(errNum) ((errNum) == 0 ? mStatus_NoError : mStatus_UnknownErr)

mDNSlocal void SockAddrTomDNSAddr(const struct sockaddr *const sa, mDNSAddr *ipAddr, mDNSIPPort *ipPort)
	{
	switch (sa->sa_family)
		{
		case AF_INET:
			{
			struct sockaddr_in* sin          = (struct sockaddr_in*)sa;
			ipAddr->type                     = mDNSAddrType_IPv4;
			ipAddr->ip.v4.NotAnInteger       = sin->sin_addr.s_addr;
			if (ipPort) ipPort->NotAnInteger = sin->sin_port;
			break;
			}

#if HAVE_IPV6
		case AF_INET6:
			{
			struct sockaddr_in6* sin6        = (struct sockaddr_in6*)sa;
#ifndef NOT_HAVE_SA_LEN
			assert(sin6->sin6_len == sizeof(*sin6));
#endif
			ipAddr->type                     = mDNSAddrType_IPv6;
			ipAddr->ip.v6                    = *(mDNSv6Addr*)&sin6->sin6_addr;
			if (ipPort) ipPort->NotAnInteger = sin6->sin6_port;
			break;
			}
#endif

		default:
			verbosedebugf("SockAddrTomDNSAddr: Uknown address family %d\n", sa->sa_family);
			ipAddr->type = mDNSAddrType_None;
			if (ipPort) ipPort->NotAnInteger = 0;
			break;
		}
	}

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark ***** Send and Receive
#endif

// mDNS core calls this routine when it needs to send a packet.
mDNSexport mStatus mDNSPlatformSendUDP(const mDNS *const m, const void *const msg, const mDNSu8 *const end,
	mDNSInterfaceID InterfaceID, const mDNSAddr *dst, mDNSIPPort dstPort)
	{
	int                     err = 0;
	struct sockaddr_storage to;
	PosixNetworkInterface * thisIntf = (PosixNetworkInterface *)(InterfaceID);
	int sendingsocket = -1;

	assert(m != NULL);
	assert(msg != NULL);
	assert(end != NULL);
	assert( (((char *) end) - ((char *) msg)) > 0 );
	assert(dstPort.NotAnInteger != 0);

	if (dst->type == mDNSAddrType_IPv4)
		{
		struct sockaddr_in *sin = (struct sockaddr_in*)&to;
#ifndef NOT_HAVE_SA_LEN
		sin->sin_len            = sizeof(*sin);
#endif
		sin->sin_family         = AF_INET;
		sin->sin_port           = dstPort.NotAnInteger;
		sin->sin_addr.s_addr    = dst->ip.v4.NotAnInteger;
		sendingsocket           = thisIntf ? thisIntf->multicastSocket4 : m->p->unicastSocket4;
		}

#if HAVE_IPV6
	else if (dst->type == mDNSAddrType_IPv6)
		{
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)&to;
		mDNSPlatformMemZero(sin6, sizeof(*sin6));
#ifndef NOT_HAVE_SA_LEN
		sin6->sin6_len            = sizeof(*sin6);
#endif
		sin6->sin6_family         = AF_INET6;
		sin6->sin6_port           = dstPort.NotAnInteger;
		sin6->sin6_addr           = *(struct in6_addr*)&dst->ip.v6;
		sendingsocket             = thisIntf ? thisIntf->multicastSocket6 : m->p->unicastSocket6;
		}
#endif

	if (sendingsocket >= 0)
		err = sendto(sendingsocket, msg, (char*)end - (char*)msg, 0, (struct sockaddr *)&to, GET_SA_LEN(to));

	if      (err > 0) err = 0;
	else if (err < 0)
		{
		static int MessageCount = 0;
        // Don't report EHOSTDOWN (i.e. ARP failure), ENETDOWN, or no route to host for unicast destinations
		if (!mDNSAddressIsAllDNSLinkGroup(dst))
			if (errno == EHOSTDOWN || errno == ENETDOWN || errno == EHOSTUNREACH || errno == ENETUNREACH) return(mStatus_TransientErr);

		if (MessageCount < 1000)
			{
			MessageCount++;
			if (thisIntf)
				LogMsg("mDNSPlatformSendUDP got error %d (%s) sending packet to %#a on interface %#a/%s/%d",
							  errno, strerror(errno), dst, &thisIntf->coreIntf.ip, thisIntf->intfName, thisIntf->index);
			else
				LogMsg("mDNSPlatformSendUDP got error %d (%s) sending packet to %#a", errno, strerror(errno), dst);
			}
		}

	return PosixErrorToStatus(err);
	}

// This routine is called when the main loop detects that data is available on a socket.
mDNSlocal void SocketDataReady(mDNS *const m, PosixNetworkInterface *intf, int skt)
	{
	mDNSAddr   senderAddr, destAddr;
	mDNSIPPort senderPort;
	ssize_t                 packetLen;
	DNSMessage              packet;
	struct my_in_pktinfo    packetInfo;
	struct sockaddr_storage from;
	socklen_t               fromLen;
	int                     flags;
	mDNSu8					ttl;
	mDNSBool                reject;
	const mDNSInterfaceID InterfaceID = intf ? intf->coreIntf.InterfaceID : NULL;

	assert(m    != NULL);
	assert(skt  >= 0);

	fromLen = sizeof(from);
	flags   = 0;
	packetLen = recvfrom_flags(skt, &packet, sizeof(packet), &flags, (struct sockaddr *) &from, &fromLen, &packetInfo, &ttl);

	if (packetLen >= 0)
		{
		SockAddrTomDNSAddr((struct sockaddr*)&from, &senderAddr, &senderPort);
		SockAddrTomDNSAddr((struct sockaddr*)&packetInfo.ipi_addr, &destAddr, NULL);

		// If we have broken IP_RECVDSTADDR functionality (so far
		// I've only seen this on OpenBSD) then apply a hack to
		// convince mDNS Core that this isn't a spoof packet.
		// Basically what we do is check to see whether the
		// packet arrived as a multicast and, if so, set its
		// destAddr to the mDNS address.
		//
		// I must admit that I could just be doing something
		// wrong on OpenBSD and hence triggering this problem
		// but I'm at a loss as to how.
		//
		// If this platform doesn't have IP_PKTINFO or IP_RECVDSTADDR, then we have
		// no way to tell the destination address or interface this packet arrived on,
		// so all we can do is just assume it's a multicast

		#if HAVE_BROKEN_RECVDSTADDR || (!defined(IP_PKTINFO) && !defined(IP_RECVDSTADDR))
			if ( (destAddr.NotAnInteger == 0) && (flags & MSG_MCAST) )
				{
				destAddr.type = senderAddr.type;
				if      (senderAddr.type == mDNSAddrType_IPv4) destAddr.ip.v4 = AllDNSLinkGroupv4;
				else if (senderAddr.type == mDNSAddrType_IPv6) destAddr.ip.v6 = AllDNSLinkGroupv6;
				}
		#endif

		// We only accept the packet if the interface on which it came
		// in matches the interface associated with this socket.
		// We do this match by name or by index, depending on which
		// information is available.  recvfrom_flags sets the name
		// to "" if the name isn't available, or the index to -1
		// if the index is available.  This accomodates the various
		// different capabilities of our target platforms.

		reject = mDNSfalse;
		if (!intf)
			{
			// Ignore multicasts accidentally delivered to our unicast receiving socket
			if (mDNSAddrIsDNSMulticast(&destAddr)) packetLen = -1;
			}
		else
			{
			if      ( packetInfo.ipi_ifname[0] != 0 ) reject = (strcmp(packetInfo.ipi_ifname, intf->intfName) != 0);
			else if ( packetInfo.ipi_ifindex != -1 )  reject = (packetInfo.ipi_ifindex != intf->index);
	
			if (reject)
				{
				verbosedebugf("SocketDataReady ignored a packet from %#a to %#a on interface %s/%d expecting %#a/%s/%d/%d",
					&senderAddr, &destAddr, packetInfo.ipi_ifname, packetInfo.ipi_ifindex,
					&intf->coreIntf.ip, intf->intfName, intf->index, skt);
				packetLen = -1;
				num_pkts_rejected++;
				if (num_pkts_rejected > (num_pkts_accepted + 1) * (num_registered_interfaces + 1) * 2)
					{
					fprintf(stderr,
						"*** WARNING: Received %d packets; Accepted %d packets; Rejected %d packets because of interface mismatch\n",
						num_pkts_accepted + num_pkts_rejected, num_pkts_accepted, num_pkts_rejected);
					num_pkts_accepted = 0;
					num_pkts_rejected = 0;
					}
				}
			else
				{
				verbosedebugf("SocketDataReady got a packet from %#a to %#a on interface %#a/%s/%d/%d",
					&senderAddr, &destAddr, &intf->coreIntf.ip, intf->intfName, intf->index, skt);
				num_pkts_accepted++;
				}
			}
		}

	if (packetLen >= 0)
		mDNSCoreReceive(m, &packet, (mDNSu8 *)&packet + packetLen,
			&senderAddr, senderPort, &destAddr, MulticastDNSPort, InterfaceID);
	}

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

mDNSexport int mDNSPlatformReadTCP(int sd, void *buf, int buflen)
	{
	(void)sd;			// Unused
	(void)buf;			// Unused
	(void)buflen;			// Unused
	return(0);
	}

mDNSexport int mDNSPlatformWriteTCP(int sd, const char *msg, int len)
	{
	(void)sd;			// Unused
	(void)msg;			// Unused
	(void)len;			// Unused
	return(0);
	}

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark ***** Get/Free Search Domain List
#endif

mDNSexport DNameListElem *mDNSPlatformGetSearchDomainList(void)
	{
	static DNameListElem tmp;
	static mDNSBool init = mDNSfalse;

	if (!init)
		{
		MakeDomainNameFromDNSNameString(&tmp.name, "local.");
		tmp.next = NULL;
		init = mDNStrue;
		}
	return mDNS_CopyDNameList(&tmp);
	}

mDNSexport DNameListElem *mDNSPlatformGetRegDomainList(void)
	{
	return NULL;
	}

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark ***** Init and Term
#endif

// This gets the current hostname, truncating it at the first dot if necessary
mDNSlocal void GetUserSpecifiedRFC1034ComputerName(domainlabel *const namelabel)
	{
	int len = 0;
	gethostname((char *)(&namelabel->c[1]), MAX_DOMAIN_LABEL);
	while (len < MAX_DOMAIN_LABEL && namelabel->c[len+1] && namelabel->c[len+1] != '.') len++;
	namelabel->c[0] = len;
	}

// On OS X this gets the text of the field labelled "Computer Name" in the Sharing Prefs Control Panel
// Other platforms can either get the information from the appropriate place,
// or they can alternatively just require all registering services to provide an explicit name
mDNSlocal void GetUserSpecifiedFriendlyComputerName(domainlabel *const namelabel)
	{
	// On Unix we have no better name than the host name, so we just use that.
	GetUserSpecifiedRFC1034ComputerName( namelabel);
	}

mDNSexport int ParseDNSServers(mDNS *m, const char *filePath)
	{
	char line[256];
	char nameserver[16];
	char keyword[10];
	int  numOfServers = 0;
	FILE *fp = fopen(filePath, "r");
	if (fp == NULL) return -1;
	while (fgets(line,sizeof(line),fp))
		{
		struct in_addr ina;
		line[255]='\0';		// just to be safe
		if (sscanf(line,"%10s %15s", keyword, nameserver) != 2) continue;	// it will skip whitespaces
		if (strncmp(keyword,"nameserver",10)) continue;
		if (inet_aton(nameserver, (struct in_addr *)&ina) != 0)
			{
			mDNSAddr DNSAddr;
			DNSAddr.type = mDNSAddrType_IPv4;
			DNSAddr.ip.v4.NotAnInteger = ina.s_addr;
			mDNS_AddDNSServer(m, &DNSAddr, NULL);
			numOfServers++;
			}
		}  
	return (numOfServers > 0) ? 0 : -1;
	}

// Searches the interface list looking for the named interface.
// Returns a pointer to if it found, or NULL otherwise.
mDNSlocal PosixNetworkInterface *SearchForInterfaceByName(mDNS *const m, const char *intfName)
	{
	PosixNetworkInterface *intf;

	assert(m != NULL);
	assert(intfName != NULL);

	intf = (PosixNetworkInterface*)(m->HostInterfaces);
	while ( (intf != NULL) && (strcmp(intf->intfName, intfName) != 0) )
		intf = (PosixNetworkInterface *)(intf->coreIntf.next);

	return intf;
	}

mDNSexport mDNSInterfaceID mDNSPlatformInterfaceIDfromInterfaceIndex(mDNS *const m, mDNSu32 index)
	{
	PosixNetworkInterface *intf;

	assert(m != NULL);

	if (index == kDNSServiceInterfaceIndexLocalOnly) return(mDNSInterface_LocalOnly);

	intf = (PosixNetworkInterface*)(m->HostInterfaces);
	while ( (intf != NULL) && (mDNSu32) intf->index != index) 
		intf = (PosixNetworkInterface *)(intf->coreIntf.next);

	return (mDNSInterfaceID) intf;
	}
	
mDNSexport mDNSu32 mDNSPlatformInterfaceIndexfromInterfaceID(mDNS *const m, mDNSInterfaceID id)
	{
	PosixNetworkInterface *intf;

	assert(m != NULL);

	if (id == mDNSInterface_LocalOnly) return(kDNSServiceInterfaceIndexLocalOnly);

	intf = (PosixNetworkInterface*)(m->HostInterfaces);
	while ( (intf != NULL) && (mDNSInterfaceID) intf != id)
		intf = (PosixNetworkInterface *)(intf->coreIntf.next);

	return intf ? intf->index : 0;
	}

// Frees the specified PosixNetworkInterface structure. The underlying
// interface must have already been deregistered with the mDNS core.
mDNSlocal void FreePosixNetworkInterface(PosixNetworkInterface *intf)
	{
	assert(intf != NULL);
	if (intf->intfName != NULL)        free((void *)intf->intfName);
	if (intf->multicastSocket4 != -1) assert(close(intf->multicastSocket4) == 0);
#if HAVE_IPV6
	if (intf->multicastSocket6 != -1) assert(close(intf->multicastSocket6) == 0);
#endif
	free(intf);
	}

// Grab the first interface, deregister it, free it, and repeat until done.
mDNSlocal void ClearInterfaceList(mDNS *const m)
	{
	assert(m != NULL);

	while (m->HostInterfaces)
		{
		PosixNetworkInterface *intf = (PosixNetworkInterface*)(m->HostInterfaces);
		mDNS_DeregisterInterface(m, &intf->coreIntf, mDNSfalse);
		if (gMDNSPlatformPosixVerboseLevel > 0) fprintf(stderr, "Deregistered interface %s\n", intf->intfName);
		FreePosixNetworkInterface(intf);
		}
	num_registered_interfaces = 0;
	num_pkts_accepted = 0;
	num_pkts_rejected = 0;
	}

// Sets up a send/receive socket.
// If mDNSIPPort port is non-zero, then it's a multicast socket on the specified interface
// If mDNSIPPort port is zero, then it's a randomly assigned port number, used for sending unicast queries
mDNSlocal int SetupSocket(struct sockaddr *intfAddr, mDNSIPPort port, int interfaceIndex, int *sktPtr)
	{
	int err = 0;
	static const int kOn = 1;
	static const int kIntTwoFiveFive = 255;
	static const unsigned char kByteTwoFiveFive = 255;
	const mDNSBool JoinMulticastGroup = (port.NotAnInteger != 0);
	
	(void) interfaceIndex;	// This parameter unused on plaforms that don't have IPv6
	assert(intfAddr != NULL);
	assert(sktPtr != NULL);
	assert(*sktPtr == -1);

	// Open the socket...
	if      (intfAddr->sa_family == AF_INET ) *sktPtr = socket(PF_INET,  SOCK_DGRAM, IPPROTO_UDP);
#if HAVE_IPV6
	else if (intfAddr->sa_family == AF_INET6) *sktPtr = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
#endif
	else return EINVAL;

	if (*sktPtr < 0) { err = errno; perror("socket"); }

	// ... with a shared UDP port, if it's for multicast receiving
	if (err == 0 && port.NotAnInteger)
		{
		#if defined(SO_REUSEPORT)
			err = setsockopt(*sktPtr, SOL_SOCKET, SO_REUSEPORT, &kOn, sizeof(kOn));
		#elif defined(SO_REUSEADDR)
			err = setsockopt(*sktPtr, SOL_SOCKET, SO_REUSEADDR, &kOn, sizeof(kOn));
		#else
			#error This platform has no way to avoid address busy errors on multicast.
		#endif
		if (err < 0) { err = errno; perror("setsockopt - SO_REUSExxxx"); }
		}

	// We want to receive destination addresses and interface identifiers.
	if (intfAddr->sa_family == AF_INET)
		{
		struct ip_mreq imr;
		struct sockaddr_in bindAddr;
		if (err == 0)
			{
			#if defined(IP_PKTINFO)									// Linux
				err = setsockopt(*sktPtr, IPPROTO_IP, IP_PKTINFO, &kOn, sizeof(kOn));
				if (err < 0) { err = errno; perror("setsockopt - IP_PKTINFO"); }
			#elif defined(IP_RECVDSTADDR) || defined(IP_RECVIF)		// BSD and Solaris
				#if defined(IP_RECVDSTADDR)
					err = setsockopt(*sktPtr, IPPROTO_IP, IP_RECVDSTADDR, &kOn, sizeof(kOn));
					if (err < 0) { err = errno; perror("setsockopt - IP_RECVDSTADDR"); }
				#endif
				#if defined(IP_RECVIF)
					if (err == 0)
						{
						err = setsockopt(*sktPtr, IPPROTO_IP, IP_RECVIF, &kOn, sizeof(kOn));
						if (err < 0) { err = errno; perror("setsockopt - IP_RECVIF"); }
						}
				#endif
			#else
				#warning This platform has no way to get the destination interface information -- will only work for single-homed hosts
			#endif
			}
	#if defined(IP_RECVTTL)									// Linux
		if (err == 0)
			{
			setsockopt(*sktPtr, IPPROTO_IP, IP_RECVTTL, &kOn, sizeof(kOn));
			// We no longer depend on being able to get the received TTL, so don't worry if the option fails
			}
	#endif

		// Add multicast group membership on this interface
		if (err == 0 && JoinMulticastGroup)
			{
			imr.imr_multiaddr.s_addr = AllDNSLinkGroupv4.NotAnInteger;
			imr.imr_interface        = ((struct sockaddr_in*)intfAddr)->sin_addr;
			err = setsockopt(*sktPtr, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(imr));
			if (err < 0) { err = errno; perror("setsockopt - IP_ADD_MEMBERSHIP"); }
			}

		// Specify outgoing interface too
		if (err == 0 && JoinMulticastGroup)
			{
			err = setsockopt(*sktPtr, IPPROTO_IP, IP_MULTICAST_IF, &((struct sockaddr_in*)intfAddr)->sin_addr, sizeof(struct in_addr));
			if (err < 0) { err = errno; perror("setsockopt - IP_MULTICAST_IF"); }
			}

		// Per the mDNS spec, send unicast packets with TTL 255
		if (err == 0)
			{
			err = setsockopt(*sktPtr, IPPROTO_IP, IP_TTL, &kIntTwoFiveFive, sizeof(kIntTwoFiveFive));
			if (err < 0) { err = errno; perror("setsockopt - IP_TTL"); }
			}

		// and multicast packets with TTL 255 too
		// There's some debate as to whether IP_MULTICAST_TTL is an int or a byte so we just try both.
		if (err == 0)
			{
			err = setsockopt(*sktPtr, IPPROTO_IP, IP_MULTICAST_TTL, &kByteTwoFiveFive, sizeof(kByteTwoFiveFive));
			if (err < 0 && errno == EINVAL)
				err = setsockopt(*sktPtr, IPPROTO_IP, IP_MULTICAST_TTL, &kIntTwoFiveFive, sizeof(kIntTwoFiveFive));
			if (err < 0) { err = errno; perror("setsockopt - IP_MULTICAST_TTL"); }
			}

		// And start listening for packets
		if (err == 0)
			{
			bindAddr.sin_family      = AF_INET;
			bindAddr.sin_port        = port.NotAnInteger;
			bindAddr.sin_addr.s_addr = INADDR_ANY; // Want to receive multicasts AND unicasts on this socket
			err = bind(*sktPtr, (struct sockaddr *) &bindAddr, sizeof(bindAddr));
			if (err < 0) { err = errno; perror("bind"); fflush(stderr); }
			}
		} // endif (intfAddr->sa_family == AF_INET)

#if HAVE_IPV6
	else if (intfAddr->sa_family == AF_INET6)
		{
		struct ipv6_mreq imr6;
		struct sockaddr_in6 bindAddr6;
	#if defined(IPV6_PKTINFO)
		if (err == 0)
			{
				err = setsockopt(*sktPtr, IPPROTO_IPV6, IPV6_PKTINFO, &kOn, sizeof(kOn));
				if (err < 0) { err = errno; perror("setsockopt - IPV6_PKTINFO"); }
			}
	#else
		#warning This platform has no way to get the destination interface information for IPv6 -- will only work for single-homed hosts
	#endif
	#if defined(IPV6_HOPLIMIT)
		if (err == 0)
			{
				err = setsockopt(*sktPtr, IPPROTO_IPV6, IPV6_HOPLIMIT, &kOn, sizeof(kOn));
				if (err < 0) { err = errno; perror("setsockopt - IPV6_HOPLIMIT"); }
			}
	#endif

		// Add multicast group membership on this interface
		if (err == 0 && JoinMulticastGroup)
			{
			imr6.ipv6mr_multiaddr       = *(const struct in6_addr*)&AllDNSLinkGroupv6;
			imr6.ipv6mr_interface       = interfaceIndex;
			//LogMsg("Joining %.16a on %d", &imr6.ipv6mr_multiaddr, imr6.ipv6mr_interface);
			err = setsockopt(*sktPtr, IPPROTO_IPV6, IPV6_JOIN_GROUP, &imr6, sizeof(imr6));
			if (err < 0)
				{
				err = errno;
				verbosedebugf("IPV6_JOIN_GROUP %.16a on %d failed.\n", &imr6.ipv6mr_multiaddr, imr6.ipv6mr_interface);
				perror("setsockopt - IPV6_JOIN_GROUP");
				}
			}

		// Specify outgoing interface too
		if (err == 0 && JoinMulticastGroup)
			{
			u_int	multicast_if = interfaceIndex;
			err = setsockopt(*sktPtr, IPPROTO_IPV6, IPV6_MULTICAST_IF, &multicast_if, sizeof(multicast_if));
			if (err < 0) { err = errno; perror("setsockopt - IPV6_MULTICAST_IF"); }
			}

		// We want to receive only IPv6 packets on this socket.
		// Without this option, we may get IPv4 addresses as mapped addresses.
		if (err == 0)
			{
			err = setsockopt(*sktPtr, IPPROTO_IPV6, IPV6_V6ONLY, &kOn, sizeof(kOn));
			if (err < 0) { err = errno; perror("setsockopt - IPV6_V6ONLY"); }
			}

		// Per the mDNS spec, send unicast packets with TTL 255
		if (err == 0)
			{
			err = setsockopt(*sktPtr, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &kIntTwoFiveFive, sizeof(kIntTwoFiveFive));
			if (err < 0) { err = errno; perror("setsockopt - IPV6_UNICAST_HOPS"); }
			}

		// and multicast packets with TTL 255 too
		// There's some debate as to whether IPV6_MULTICAST_HOPS is an int or a byte so we just try both.
		if (err == 0)
			{
			err = setsockopt(*sktPtr, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &kByteTwoFiveFive, sizeof(kByteTwoFiveFive));
			if (err < 0 && errno == EINVAL)
				err = setsockopt(*sktPtr, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &kIntTwoFiveFive, sizeof(kIntTwoFiveFive));
			if (err < 0) { err = errno; perror("setsockopt - IPV6_MULTICAST_HOPS"); }
			}

		// And start listening for packets
		if (err == 0)
			{
			mDNSPlatformMemZero(&bindAddr6, sizeof(bindAddr6));
#ifndef NOT_HAVE_SA_LEN
			bindAddr6.sin6_len         = sizeof(bindAddr6);
#endif
			bindAddr6.sin6_family      = AF_INET6;
			bindAddr6.sin6_port        = port.NotAnInteger;
			bindAddr6.sin6_flowinfo    = 0;
			bindAddr6.sin6_addr        = in6addr_any; // Want to receive multicasts AND unicasts on this socket
			bindAddr6.sin6_scope_id    = 0;
			err = bind(*sktPtr, (struct sockaddr *) &bindAddr6, sizeof(bindAddr6));
			if (err < 0) { err = errno; perror("bind"); fflush(stderr); }
			}
		} // endif (intfAddr->sa_family == AF_INET6)
#endif

	// Set the socket to non-blocking.
	if (err == 0)
		{
		err = fcntl(*sktPtr, F_GETFL, 0);
		if (err < 0) err = errno;
		else
			{
			err = fcntl(*sktPtr, F_SETFL, err | O_NONBLOCK);
			if (err < 0) err = errno;
			}
		}

	// Clean up
	if (err != 0 && *sktPtr != -1) { assert(close(*sktPtr) == 0); *sktPtr = -1; }
	assert( (err == 0) == (*sktPtr != -1) );
	return err;
	}

// Creates a PosixNetworkInterface for the interface whose IP address is
// intfAddr and whose name is intfName and registers it with mDNS core.
mDNSlocal int SetupOneInterface(mDNS *const m, struct sockaddr *intfAddr, struct sockaddr *intfMask, const char *intfName, int intfIndex)
	{
	int err = 0;
	PosixNetworkInterface *intf;
	PosixNetworkInterface *alias = NULL;

	assert(m != NULL);
	assert(intfAddr != NULL);
	assert(intfName != NULL);
	assert(intfMask != NULL);

	// Allocate the interface structure itself.
	intf = (PosixNetworkInterface*)malloc(sizeof(*intf));
	if (intf == NULL) { assert(0); err = ENOMEM; }

	// And make a copy of the intfName.
	if (err == 0)
		{
		intf->intfName = strdup(intfName);
		if (intf->intfName == NULL) { assert(0); err = ENOMEM; }
		}

	if (err == 0)
		{
		// Set up the fields required by the mDNS core.
		SockAddrTomDNSAddr(intfAddr, &intf->coreIntf.ip, NULL);
		SockAddrTomDNSAddr(intfMask, &intf->coreIntf.mask, NULL);
		//LogMsg("SetupOneInterface: %#a %#a",  &intf->coreIntf.ip,  &intf->coreIntf.mask);
		strncpy(intf->coreIntf.ifname, intfName, sizeof(intf->coreIntf.ifname));
		intf->coreIntf.ifname[sizeof(intf->coreIntf.ifname)-1] = 0;
		intf->coreIntf.Advertise = m->AdvertiseLocalAddresses;
		intf->coreIntf.McastTxRx = mDNStrue;

		// Set up the extra fields in PosixNetworkInterface.
		assert(intf->intfName != NULL);         // intf->intfName already set up above
		intf->index                = intfIndex;
		intf->multicastSocket4     = -1;
#if HAVE_IPV6
		intf->multicastSocket6     = -1;
#endif
		alias                      = SearchForInterfaceByName(m, intf->intfName);
		if (alias == NULL) alias   = intf;
		intf->coreIntf.InterfaceID = (mDNSInterfaceID)alias;

		if (alias != intf)
			debugf("SetupOneInterface: %s %#a is an alias of %#a", intfName, &intf->coreIntf.ip, &alias->coreIntf.ip);
		}

	// Set up the multicast socket
	if (err == 0)
		{
		if (alias->multicastSocket4 == -1 && intfAddr->sa_family == AF_INET)
			err = SetupSocket(intfAddr, MulticastDNSPort, intf->index, &alias->multicastSocket4);
#if HAVE_IPV6
		else if (alias->multicastSocket6 == -1 && intfAddr->sa_family == AF_INET6)
			err = SetupSocket(intfAddr, MulticastDNSPort, intf->index, &alias->multicastSocket6);
#endif
		}

	// The interface is all ready to go, let's register it with the mDNS core.
	if (err == 0)
		err = mDNS_RegisterInterface(m, &intf->coreIntf, mDNSfalse);

	// Clean up.
	if (err == 0)
		{
		num_registered_interfaces++;
		debugf("SetupOneInterface: %s %#a Registered", intf->intfName, &intf->coreIntf.ip);
		if (gMDNSPlatformPosixVerboseLevel > 0)
			fprintf(stderr, "Registered interface %s\n", intf->intfName);
		}
	else
		{
		// Use intfName instead of intf->intfName in the next line to avoid dereferencing NULL.
		debugf("SetupOneInterface: %s %#a failed to register %d", intfName, &intf->coreIntf.ip, err);
		if (intf) { FreePosixNetworkInterface(intf); intf = NULL; }
		}

	assert( (err == 0) == (intf != NULL) );

	return err;
	}

// Call get_ifi_info() to obtain a list of active interfaces and call SetupOneInterface() on each one.
mDNSlocal int SetupInterfaceList(mDNS *const m)
	{
	mDNSBool        foundav4       = mDNSfalse;
	int             err            = 0;
	struct ifi_info *intfList      = get_ifi_info(AF_INET, mDNStrue);
	struct ifi_info *firstLoopback = NULL;

	assert(m != NULL);
	debugf("SetupInterfaceList");

	if (intfList == NULL) err = ENOENT;

#if HAVE_IPV6
	if (err == 0)		/* Link the IPv6 list to the end of the IPv4 list */
		{
		struct ifi_info **p = &intfList;
		while (*p) p = &(*p)->ifi_next;
		*p = get_ifi_info(AF_INET6, mDNStrue);
		}
#endif

	if (err == 0)
		{
		struct ifi_info *i = intfList;
		while (i)
			{
			if (     ((i->ifi_addr->sa_family == AF_INET)
#if HAVE_IPV6
					  || (i->ifi_addr->sa_family == AF_INET6)
#endif
				) &&  (i->ifi_flags & IFF_UP) && !(i->ifi_flags & IFF_POINTOPOINT) )
				{
				if (i->ifi_flags & IFF_LOOPBACK)
					{
					if (firstLoopback == NULL)
						firstLoopback = i;
					}
				else
					{
					if (SetupOneInterface(m, i->ifi_addr, i->ifi_netmask, i->ifi_name, i->ifi_index) == 0)
						if (i->ifi_addr->sa_family == AF_INET)
							foundav4 = mDNStrue;
					}
				}
			i = i->ifi_next;
			}

		// If we found no normal interfaces but we did find a loopback interface, register the
		// loopback interface.  This allows self-discovery if no interfaces are configured.
		// Temporary workaround: Multicast loopback on IPv6 interfaces appears not to work.
		// In the interim, we skip loopback interface only if we found at least one v4 interface to use
		// if ( (m->HostInterfaces == NULL) && (firstLoopback != NULL) )
		if ( !foundav4 && firstLoopback )
			(void) SetupOneInterface(m, firstLoopback->ifi_addr, firstLoopback->ifi_netmask, firstLoopback->ifi_name, firstLoopback->ifi_index);
		}

	// Clean up.
	if (intfList != NULL) free_ifi_info(intfList);
	return err;
	}

#if USES_NETLINK

// See <http://www.faqs.org/rfcs/rfc3549.html> for a description of NetLink

// Open a socket that will receive interface change notifications
mDNSlocal mStatus OpenIfNotifySocket( int *pFD)
	{
	mStatus					err = mStatus_NoError;
	struct sockaddr_nl		snl;
	int sock;
	int ret;

	sock = socket( AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (sock < 0)
		return errno;

	// Configure read to be non-blocking because inbound msg size is not known in advance
	(void) fcntl( sock, F_SETFL, O_NONBLOCK);

	/* Subscribe the socket to Link & IP addr notifications. */
	bzero( &snl, sizeof snl);
	snl.nl_family = AF_NETLINK;
	snl.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;
	ret = bind( sock, (struct sockaddr *) &snl, sizeof snl);
	if ( 0 == ret)
		*pFD = sock;
	else
		err = errno;

	return err;
	}

#if MDNS_DEBUGMSGS
mDNSlocal void		PrintNetLinkMsg( const struct nlmsghdr *pNLMsg)
	{
	const char *kNLMsgTypes[] = { "", "NLMSG_NOOP", "NLMSG_ERROR", "NLMSG_DONE", "NLMSG_OVERRUN" };
	const char *kNLRtMsgTypes[] = { "RTM_NEWLINK", "RTM_DELLINK", "RTM_GETLINK", "RTM_NEWADDR", "RTM_DELADDR", "RTM_GETADDR" };

	printf( "nlmsghdr len=%d, type=%s, flags=0x%x\n", pNLMsg->nlmsg_len, 
			pNLMsg->nlmsg_type < RTM_BASE ? kNLMsgTypes[ pNLMsg->nlmsg_type] : kNLRtMsgTypes[ pNLMsg->nlmsg_type - RTM_BASE], 
			pNLMsg->nlmsg_flags);

	if ( RTM_NEWLINK <= pNLMsg->nlmsg_type && pNLMsg->nlmsg_type <= RTM_GETLINK)
		{
		struct ifinfomsg	*pIfInfo = (struct ifinfomsg*) NLMSG_DATA( pNLMsg);
		printf( "ifinfomsg family=%d, type=%d, index=%d, flags=0x%x, change=0x%x\n", pIfInfo->ifi_family, 
				pIfInfo->ifi_type, pIfInfo->ifi_index, pIfInfo->ifi_flags, pIfInfo->ifi_change);
		
		}
	else if ( RTM_NEWADDR <= pNLMsg->nlmsg_type && pNLMsg->nlmsg_type <= RTM_GETADDR)
		{
		struct ifaddrmsg	*pIfAddr = (struct ifaddrmsg*) NLMSG_DATA( pNLMsg);
		printf( "ifaddrmsg family=%d, index=%d, flags=0x%x\n", pIfAddr->ifa_family, 
				pIfAddr->ifa_index, pIfAddr->ifa_flags);
		}
	printf( "\n");
	}
#endif

mDNSlocal mDNSu32		ProcessRoutingNotification( int sd)
// Read through the messages on sd and if any indicate that any interface records should
// be torn down and rebuilt, return affected indices as a bitmask. Otherwise return 0.
	{
	ssize_t					readCount;
	char					buff[ 4096];	
	struct nlmsghdr			*pNLMsg = (struct nlmsghdr*) buff;
	mDNSu32				result = 0;
	
	// The structure here is more complex than it really ought to be because,
	// unfortunately, there's no good way to size a buffer in advance large
	// enough to hold all pending data and so avoid message fragmentation.
	// (Note that FIONREAD is not supported on AF_NETLINK.)

	readCount = read( sd, buff, sizeof buff);
	while ( 1)
		{
		// Make sure we've got an entire nlmsghdr in the buffer, and payload, too.
		// If not, discard already-processed messages in buffer and read more data.
		if ( ( (char*) &pNLMsg[1] > ( buff + readCount)) ||	// i.e. *pNLMsg extends off end of buffer 
			 ( (char*) pNLMsg + pNLMsg->nlmsg_len > ( buff + readCount)))
			{
			if ( buff < (char*) pNLMsg)		// we have space to shuffle
				{
				// discard processed data
				readCount -= ( (char*) pNLMsg - buff);
				memmove( buff, pNLMsg, readCount);
				pNLMsg = (struct nlmsghdr*) buff;

				// read more data
				readCount += read( sd, buff + readCount, sizeof buff - readCount);
				continue;					// spin around and revalidate with new readCount
				}
			else
				break;	// Otherwise message does not fit in buffer
			}

#if MDNS_DEBUGMSGS
		PrintNetLinkMsg( pNLMsg);
#endif

		// Process the NetLink message
		if ( pNLMsg->nlmsg_type == RTM_GETLINK || pNLMsg->nlmsg_type == RTM_NEWLINK)
			result |= 1 << ((struct ifinfomsg*) NLMSG_DATA( pNLMsg))->ifi_index;
		else if ( pNLMsg->nlmsg_type == RTM_DELADDR || pNLMsg->nlmsg_type == RTM_NEWADDR)
			result |= 1 << ((struct ifaddrmsg*) NLMSG_DATA( pNLMsg))->ifa_index;

		// Advance pNLMsg to the next message in the buffer
		if ( ( pNLMsg->nlmsg_flags & NLM_F_MULTI) != 0 && pNLMsg->nlmsg_type != NLMSG_DONE)
			{
			ssize_t	len = readCount - ( (char*)pNLMsg - buff);
			pNLMsg = NLMSG_NEXT( pNLMsg, len);
			}
		else
			break;	// all done!
		}

	return result;
	}

#else // USES_NETLINK

// Open a socket that will receive interface change notifications
mDNSlocal mStatus OpenIfNotifySocket( int *pFD)
	{
	*pFD = socket( AF_ROUTE, SOCK_RAW, 0);

	if ( *pFD < 0)
		return mStatus_UnknownErr;

	// Configure read to be non-blocking because inbound msg size is not known in advance
	(void) fcntl( *pFD, F_SETFL, O_NONBLOCK);

	return mStatus_NoError;
	}

#if MDNS_DEBUGMSGS
mDNSlocal void		PrintRoutingSocketMsg( const struct ifa_msghdr *pRSMsg)
	{
	const char *kRSMsgTypes[] = { "", "RTM_ADD", "RTM_DELETE", "RTM_CHANGE", "RTM_GET", "RTM_LOSING",
					"RTM_REDIRECT", "RTM_MISS", "RTM_LOCK", "RTM_OLDADD", "RTM_OLDDEL", "RTM_RESOLVE", 
					"RTM_NEWADDR", "RTM_DELADDR", "RTM_IFINFO", "RTM_NEWMADDR", "RTM_DELMADDR" };

	int		index = pRSMsg->ifam_type == RTM_IFINFO ? ((struct if_msghdr*) pRSMsg)->ifm_index : pRSMsg->ifam_index;

	printf( "ifa_msghdr len=%d, type=%s, index=%d\n", pRSMsg->ifam_msglen, kRSMsgTypes[ pRSMsg->ifam_type], index);
	}
#endif

mDNSlocal mDNSu32		ProcessRoutingNotification( int sd)
// Read through the messages on sd and if any indicate that any interface records should
// be torn down and rebuilt, return affected indices as a bitmask. Otherwise return 0.
	{
	ssize_t					readCount;
	char					buff[ 4096];	
	struct ifa_msghdr		*pRSMsg = (struct ifa_msghdr*) buff;
	mDNSu32				result = 0;

	readCount = read( sd, buff, sizeof buff);
	if ( readCount < (ssize_t) sizeof( struct ifa_msghdr))
		return mStatus_UnsupportedErr;		// cannot decipher message

#if MDNS_DEBUGMSGS
	PrintRoutingSocketMsg( pRSMsg);
#endif

	// Process the message
	if ( pRSMsg->ifam_type == RTM_NEWADDR || pRSMsg->ifam_type == RTM_DELADDR ||
		 pRSMsg->ifam_type == RTM_IFINFO)
		{
		if ( pRSMsg->ifam_type == RTM_IFINFO)
			result |= 1 << ((struct if_msghdr*) pRSMsg)->ifm_index;
		else
			result |= 1 << pRSMsg->ifam_index;
		}

	return result;
	}

#endif // USES_NETLINK

// Called when data appears on interface change notification socket
mDNSlocal void InterfaceChangeCallback( void *context)
	{
	IfChangeRec		*pChgRec = (IfChangeRec*) context;
	fd_set			readFDs;
	mDNSu32		changedInterfaces = 0;
	struct timeval	zeroTimeout = { 0, 0 };
	
	FD_ZERO( &readFDs);
	FD_SET( pChgRec->NotifySD, &readFDs);
	
	do
	{
		changedInterfaces |= ProcessRoutingNotification( pChgRec->NotifySD);
	}
	while ( 0 < select( pChgRec->NotifySD + 1, &readFDs, (fd_set*) NULL, (fd_set*) NULL, &zeroTimeout));

	// Currently we rebuild the entire interface list whenever any interface change is
	// detected. If this ever proves to be a performance issue in a multi-homed 
	// configuration, more care should be paid to changedInterfaces.
	if ( changedInterfaces)
		mDNSPlatformPosixRefreshInterfaceList( pChgRec->mDNS);
	}

// Register with either a Routing Socket or RtNetLink to listen for interface changes.
mDNSlocal mStatus WatchForInterfaceChange(mDNS *const m)
	{
	mStatus		err;
	IfChangeRec	*pChgRec;

	pChgRec = (IfChangeRec*) mDNSPlatformMemAllocate( sizeof *pChgRec);
	if ( pChgRec == NULL)
		return mStatus_NoMemoryErr;

	pChgRec->mDNS = m;
	err = OpenIfNotifySocket( &pChgRec->NotifySD);
	if ( err == 0)
		err = mDNSPosixAddFDToEventLoop( pChgRec->NotifySD, InterfaceChangeCallback, pChgRec);

	return err;
	}

// Test to see if we're the first client running on UDP port 5353, by trying to bind to 5353 without using SO_REUSEPORT.
// If we fail, someone else got here first. That's not a big problem; we can share the port for multicast responses --
// we just need to be aware that we shouldn't expect to successfully receive unicast UDP responses.
mDNSlocal mDNSBool mDNSPlatformInit_CanReceiveUnicast(void)
	{
	int err;
	int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	struct sockaddr_in s5353;
	s5353.sin_family      = AF_INET;
	s5353.sin_port        = MulticastDNSPort.NotAnInteger;
	s5353.sin_addr.s_addr = 0;
	err = bind(s, (struct sockaddr *)&s5353, sizeof(s5353));
	close(s);
	if (err) debugf("No unicast UDP responses");
	else     debugf("Unicast UDP responses okay");
	return(err == 0);
	}

// mDNS core calls this routine to initialise the platform-specific data.
mDNSexport mStatus mDNSPlatformInit(mDNS *const m)
	{
	int err = 0;
	struct sockaddr sa;
	assert(m != NULL);

	if (mDNSPlatformInit_CanReceiveUnicast()) m->CanReceiveUnicastOn5353 = mDNStrue;

	// Tell mDNS core the names of this machine.

	// Set up the nice label
	m->nicelabel.c[0] = 0;
	GetUserSpecifiedFriendlyComputerName(&m->nicelabel);
	if (m->nicelabel.c[0] == 0) MakeDomainLabelFromLiteralString(&m->nicelabel, "Computer");

	// Set up the RFC 1034-compliant label
	m->hostlabel.c[0] = 0;
	GetUserSpecifiedRFC1034ComputerName(&m->hostlabel);
	if (m->hostlabel.c[0] == 0) MakeDomainLabelFromLiteralString(&m->hostlabel, "Computer");

	mDNS_SetFQDN(m);

	sa.sa_family = AF_INET;
	m->p->unicastSocket4 = -1;
	if (err == mStatus_NoError) err = SetupSocket(&sa, zeroIPPort, 0, &m->p->unicastSocket4);
#if HAVE_IPV6
	sa.sa_family = AF_INET6;
	m->p->unicastSocket6 = -1;
	if (err == mStatus_NoError) err = SetupSocket(&sa, zeroIPPort, 0, &m->p->unicastSocket6);
#endif

	// Tell mDNS core about the network interfaces on this machine.
	if (err == mStatus_NoError) err = SetupInterfaceList(m);

	// Tell mDNS core about DNS Servers
	if (err == mStatus_NoError) ParseDNSServers(m, uDNS_SERVERS_FILE);

	if (err == mStatus_NoError)
		{
		err = WatchForInterfaceChange(m);
		// Failure to observe interface changes is non-fatal.
		if ( err != mStatus_NoError)
			{
			fprintf(stderr, "mDNS(%d) WARNING: Unable to detect interface changes (%d).\n", getpid(), err);
			err = mStatus_NoError;
			}
		}

	// We don't do asynchronous initialization on the Posix platform, so by the time
	// we get here the setup will already have succeeded or failed.  If it succeeded,
	// we should just call mDNSCoreInitComplete() immediately.
	if (err == mStatus_NoError)
		mDNSCoreInitComplete(m, mStatus_NoError);

	return PosixErrorToStatus(err);
	}

// mDNS core calls this routine to clean up the platform-specific data.
// In our case all we need to do is to tear down every network interface.
mDNSexport void mDNSPlatformClose(mDNS *const m)
	{
	assert(m != NULL);
	ClearInterfaceList(m);
	if (m->p->unicastSocket4 != -1) assert(close(m->p->unicastSocket4) == 0);
#if HAVE_IPV6
	if (m->p->unicastSocket6 != -1) assert(close(m->p->unicastSocket6) == 0);
#endif
	}

mDNSexport mStatus mDNSPlatformPosixRefreshInterfaceList(mDNS *const m)
	{
	int err;
	ClearInterfaceList(m);
	err = SetupInterfaceList(m);
	return PosixErrorToStatus(err);
	}

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark ***** Locking
#endif

// On the Posix platform, locking is a no-op because we only ever enter
// mDNS core on the main thread.

// mDNS core calls this routine when it wants to prevent
// the platform from reentering mDNS core code.
mDNSexport void    mDNSPlatformLock   (const mDNS *const m)
	{
	(void) m;	// Unused
	}

// mDNS core calls this routine when it release the lock taken by
// mDNSPlatformLock and allow the platform to reenter mDNS core code.
mDNSexport void    mDNSPlatformUnlock (const mDNS *const m)
	{
	(void) m;	// Unused
	}

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark ***** Strings
#endif

// mDNS core calls this routine to copy C strings.
// On the Posix platform this maps directly to the ANSI C strcpy.
mDNSexport void    mDNSPlatformStrCopy(const void *src,       void *dst)
	{
	strcpy((char *)dst, (char *)src);
	}

// mDNS core calls this routine to get the length of a C string.
// On the Posix platform this maps directly to the ANSI C strlen.
mDNSexport mDNSu32  mDNSPlatformStrLen (const void *src)
	{
	return strlen((char*)src);
	}

// mDNS core calls this routine to copy memory.
// On the Posix platform this maps directly to the ANSI C memcpy.
mDNSexport void    mDNSPlatformMemCopy(const void *src,       void *dst, mDNSu32 len)
	{
	memcpy(dst, src, len);
	}

// mDNS core calls this routine to test whether blocks of memory are byte-for-byte
// identical. On the Posix platform this is a simple wrapper around ANSI C memcmp.
mDNSexport mDNSBool mDNSPlatformMemSame(const void *src, const void *dst, mDNSu32 len)
	{
	return memcmp(dst, src, len) == 0;
	}

// mDNS core calls this routine to clear blocks of memory.
// On the Posix platform this is a simple wrapper around ANSI C memset.
mDNSexport void    mDNSPlatformMemZero(                       void *dst, mDNSu32 len)
	{
	memset(dst, 0, len);
	}

mDNSexport void *  mDNSPlatformMemAllocate(mDNSu32 len) { return(malloc(len)); }
mDNSexport void    mDNSPlatformMemFree    (void *mem)   { free(mem); }

mDNSexport mDNSu32 mDNSPlatformRandomSeed(void)
	{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return(tv.tv_usec);
	}

mDNSexport mDNSs32  mDNSPlatformOneSecond = 1024;

mDNSexport mStatus mDNSPlatformTimeInit(void)
	{
	// No special setup is required on Posix -- we just use gettimeofday();
	// This is not really safe, because gettimeofday can go backwards if the user manually changes the date or time
	// We should find a better way to do this
	return(mStatus_NoError);
	}

mDNSexport mDNSs32  mDNSPlatformRawTime()
	{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	// tv.tv_sec is seconds since 1st January 1970 (GMT, with no adjustment for daylight savings time)
	// tv.tv_usec is microseconds since the start of this second (i.e. values 0 to 999999)
	// We use the lower 22 bits of tv.tv_sec for the top 22 bits of our result
	// and we multiply tv.tv_usec by 16 / 15625 to get a value in the range 0-1023 to go in the bottom 10 bits.
	// This gives us a proper modular (cyclic) counter that has a resolution of roughly 1ms (actually 1/1024 second)
	// and correctly cycles every 2^22 seconds (4194304 seconds = approx 48 days).
	return( (tv.tv_sec << 10) | (tv.tv_usec * 16 / 15625) );
	}

mDNSexport mDNSs32 mDNSPlatformUTC(void)
	{
	return time(NULL);
	}

mDNSlocal void mDNSPosixAddToFDSet(int *nfds, fd_set *readfds, int s)
	{
	if (*nfds < s + 1) *nfds = s + 1;
	FD_SET(s, readfds);
	}

mDNSexport void mDNSPosixGetFDSet(mDNS *m, int *nfds, fd_set *readfds, struct timeval *timeout)
	{
	mDNSs32 ticks;
	struct timeval interval;

	// 1. Call mDNS_Execute() to let mDNSCore do what it needs to do
	mDNSs32 nextevent = mDNS_Execute(m);

	// 2. Build our list of active file descriptors
	PosixNetworkInterface *info = (PosixNetworkInterface *)(m->HostInterfaces);
	if (m->p->unicastSocket4 != -1) mDNSPosixAddToFDSet(nfds, readfds, m->p->unicastSocket4);
#if HAVE_IPV6
	if (m->p->unicastSocket6 != -1) mDNSPosixAddToFDSet(nfds, readfds, m->p->unicastSocket6);
#endif
	while (info)
		{
		if (info->multicastSocket4 != -1) mDNSPosixAddToFDSet(nfds, readfds, info->multicastSocket4);
#if HAVE_IPV6
		if (info->multicastSocket6 != -1) mDNSPosixAddToFDSet(nfds, readfds, info->multicastSocket6);
#endif
		info = (PosixNetworkInterface *)(info->coreIntf.next);
		}

	// 3. Calculate the time remaining to the next scheduled event (in struct timeval format)
	ticks = nextevent - mDNS_TimeNow(m);
	if (ticks < 1) ticks = 1;
	interval.tv_sec  = ticks >> 10;						// The high 22 bits are seconds
	interval.tv_usec = ((ticks & 0x3FF) * 15625) / 16;	// The low 10 bits are 1024ths

	// 4. If client's proposed timeout is more than what we want, then reduce it
	if (timeout->tv_sec > interval.tv_sec ||
		(timeout->tv_sec == interval.tv_sec && timeout->tv_usec > interval.tv_usec))
		*timeout = interval;
	}

mDNSexport void mDNSPosixProcessFDSet(mDNS *const m, fd_set *readfds)
	{
	PosixNetworkInterface *info;
	assert(m       != NULL);
	assert(readfds != NULL);
	info = (PosixNetworkInterface *)(m->HostInterfaces);

	if (m->p->unicastSocket4 != -1 && FD_ISSET(m->p->unicastSocket4, readfds))
		{
		FD_CLR(m->p->unicastSocket4, readfds);
		SocketDataReady(m, NULL, m->p->unicastSocket4);
		}
#if HAVE_IPV6
	if (m->p->unicastSocket6 != -1 && FD_ISSET(m->p->unicastSocket6, readfds))
		{
		FD_CLR(m->p->unicastSocket6, readfds);
		SocketDataReady(m, NULL, m->p->unicastSocket6);
		}
#endif

	while (info)
		{
		if (info->multicastSocket4 != -1 && FD_ISSET(info->multicastSocket4, readfds))
			{
			FD_CLR(info->multicastSocket4, readfds);
			SocketDataReady(m, info, info->multicastSocket4);
			}
#if HAVE_IPV6
		if (info->multicastSocket6 != -1 && FD_ISSET(info->multicastSocket6, readfds))
			{
			FD_CLR(info->multicastSocket6, readfds);
			SocketDataReady(m, info, info->multicastSocket6);
			}
#endif
		info = (PosixNetworkInterface *)(info->coreIntf.next);
		}
	}

// update gMaxFD
mDNSlocal void	DetermineMaxEventFD( void )
	{
	PosixEventSource	*iSource;
	
	gMaxFD = 0;
	for ( iSource=(PosixEventSource*)gEventSources.Head; iSource; iSource = iSource->Next)
		if ( gMaxFD < iSource->fd)
			gMaxFD = iSource->fd;
	}

// Add a file descriptor to the set that mDNSPosixRunEventLoopOnce() listens to.
mStatus mDNSPosixAddFDToEventLoop( int fd, mDNSPosixEventCallback callback, void *context)
	{
	PosixEventSource	*newSource;
	
	if ( gEventSources.LinkOffset == 0)
		InitLinkedList( &gEventSources, offsetof( PosixEventSource, Next));

	if ( fd >= (int) FD_SETSIZE || fd < 0)
		return mStatus_UnsupportedErr;
	if ( callback == NULL)
		return mStatus_BadParamErr;

	newSource = (PosixEventSource*) malloc( sizeof *newSource);
	if ( NULL == newSource)
		return mStatus_NoMemoryErr;

	newSource->Callback = callback;
	newSource->Context = context;
	newSource->fd = fd;

	AddToTail( &gEventSources, newSource);
	FD_SET( fd, &gEventFDs);

	DetermineMaxEventFD();

	return mStatus_NoError;
	}

// Remove a file descriptor from the set that mDNSPosixRunEventLoopOnce() listens to.
mStatus mDNSPosixRemoveFDFromEventLoop( int fd)
	{
	PosixEventSource	*iSource;
	
	for ( iSource=(PosixEventSource*)gEventSources.Head; iSource; iSource = iSource->Next)
		{
		if ( fd == iSource->fd)
			{
			FD_CLR( fd, &gEventFDs);
			RemoveFromList( &gEventSources, iSource);
			free( iSource);
			DetermineMaxEventFD();
			return mStatus_NoError;
			}
		}
	return mStatus_NoSuchNameErr;
	}

// Simply note the received signal in gEventSignals.
mDNSlocal void	NoteSignal( int signum)
	{
	sigaddset( &gEventSignals, signum);
	}

// Tell the event package to listen for signal and report it in mDNSPosixRunEventLoopOnce().
mStatus mDNSPosixListenForSignalInEventLoop( int signum)
	{
	struct sigaction	action;
	mStatus				err;

	bzero( &action, sizeof action);		// more portable than member-wise assignment
	action.sa_handler = NoteSignal;
	err = sigaction( signum, &action, (struct sigaction*) NULL);
	
	sigaddset( &gEventSignalSet, signum);

	return err;
	}

// Tell the event package to stop listening for signal in mDNSPosixRunEventLoopOnce().
mStatus mDNSPosixIgnoreSignalInEventLoop( int signum)
	{
	struct sigaction	action;
	mStatus				err;

	bzero( &action, sizeof action);		// more portable than member-wise assignment
	action.sa_handler = SIG_DFL;
	err = sigaction( signum, &action, (struct sigaction*) NULL);
	
	sigdelset( &gEventSignalSet, signum);

	return err;
	}

// Do a single pass through the attendent event sources and dispatch any found to their callbacks.
// Return as soon as internal timeout expires, or a signal we're listening for is received.
mStatus mDNSPosixRunEventLoopOnce( mDNS *m, const struct timeval *pTimeout, 
									sigset_t *pSignalsReceived, mDNSBool *pDataDispatched)
	{
	fd_set			listenFDs = gEventFDs;
	int				fdMax = 0, numReady;
	struct timeval	timeout = *pTimeout;
	
	// Include the sockets that are listening to the wire in our select() set
	mDNSPosixGetFDSet( m, &fdMax, &listenFDs, &timeout);	// timeout may get modified
	if ( fdMax < gMaxFD)
		fdMax = gMaxFD;

	numReady = select( fdMax + 1, &listenFDs, (fd_set*) NULL, (fd_set*) NULL, &timeout);

	// If any data appeared, invoke its callback
	if ( numReady > 0)
		{
		PosixEventSource	*iSource;

		(void) mDNSPosixProcessFDSet( m, &listenFDs);	// call this first to process wire data for clients

		for ( iSource=(PosixEventSource*)gEventSources.Head; iSource; iSource = iSource->Next)
			{
			if ( FD_ISSET( iSource->fd, &listenFDs))
				{
				iSource->Callback( iSource->Context);
				break;	// in case callback removed elements from gEventSources
				}
			}
		*pDataDispatched = mDNStrue;
		}
	else
		*pDataDispatched = mDNSfalse;

	(void) sigprocmask( SIG_BLOCK, &gEventSignalSet, (sigset_t*) NULL);
	*pSignalsReceived = gEventSignals;
	sigemptyset( &gEventSignals);
	(void) sigprocmask( SIG_UNBLOCK, &gEventSignalSet, (sigset_t*) NULL);

	return mStatus_NoError;
	}
