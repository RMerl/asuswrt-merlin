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

    Change History (most recent first):

$Log: mDNSMacOSX.h,v $
Revision 1.104  2009/06/25 23:36:56  cheshire
To facilitate testing, added command-line switch "-OfferSleepProxyService"
to re-enable the previously-supported mode of operation where we offer
sleep proxy service on desktop Macs that are set to never sleep.

Revision 1.103  2009/04/11 00:20:13  jessic2
<rdar://problem/4426780> Daemon: Should be able to turn on LogOperation dynamically

Revision 1.102  2009/04/06 22:14:02  cheshire
Need to include IOKit/pwr_mgt/IOPM.h to build for AppleTV

Revision 1.101  2009/04/02 22:21:17  mcguire
<rdar://problem/6577409> Adopt IOPM APIs

Revision 1.100  2009/02/12 20:57:26  cheshire
Renamed 'LogAllOperation' switch to 'LogClientOperations'; added new 'LogSleepProxyActions' switch

Revision 1.99  2009/02/11 02:31:05  cheshire
Move SystemWakeForNetworkAccessEnabled into mDNS structure so it's accessible to mDNSCore routines

Revision 1.98  2009/02/07 02:52:19  cheshire
<rdar://problem/6084043> Sleep Proxy: Need to adopt IOPMConnection

Revision 1.97  2009/02/02 22:13:01  cheshire
Added SystemWakeForNetworkAccessEnabled setting

Revision 1.96  2009/01/17 04:13:57  cheshire
Added version symbols for Cheetah and Puma

Revision 1.95  2009/01/16 02:32:55  cheshire
Added SysEventNotifier and SysEventKQueue in mDNS_PlatformSupport_struct

Revision 1.94  2009/01/15 22:24:53  cheshire
Removed unused ifa_name field from NetworkInterfaceInfoOSX_struct

Revision 1.93  2008/12/10 19:30:01  cheshire
Added symbolic names for the various OS X versions

Revision 1.92  2008/10/31 23:49:38  mkrochma
Increased sizecheck limit

Revision 1.91  2008/10/31 22:48:27  cheshire
Added SCPreferencesRef to mDNS_PlatformSupport_struct

Revision 1.90  2008/10/30 01:04:35  cheshire
Added WakeAtUTC and SleepTime fields to mDNS_PlatformSupport_struct

Revision 1.89  2008/10/29 18:38:33  mcguire
Increase sizecheck limits to account for CFRunLoop added to mDNS_PlatformSupport_struct in 64bit builds

Revision 1.88  2008/10/28 20:33:56  cheshire
Added CFRunLoopRef in mDNS_PlatformSupport_struct, to hold reference to our main thread's CFRunLoop

Revision 1.87  2008/10/28 18:32:09  cheshire
Added CFSocketRef and CFRunLoopSourceRef in NetworkInterfaceInfoOSX_struct

Revision 1.86  2008/10/22 23:23:53  cheshire
Moved definition of OSXVers from daemon.c into mDNSMacOSX.c

Revision 1.85  2008/10/21 00:12:00  cheshire
Added BPF-related fields in NetworkInterfaceInfoOSX_struct

Revision 1.84  2008/10/14 20:20:44  cheshire
Increase sizecheck limits to account for DNSQuestions added to NetworkInterfaceInfo_struct

Revision 1.83  2008/10/07 21:41:57  mcguire
Increase sizecheck limits to account for DNSQuestion added to NetworkInterfaceInfo_struct in 64bit builds

Revision 1.82  2008/10/07 15:56:24  cheshire
Increase sizecheck limits to account for DNSQuestion added to NetworkInterfaceInfo_struct

Revision 1.81  2008/10/03 00:26:25  cheshire
Export DictionaryIsEnabled() so it's callable from other files

Revision 1.80  2008/10/02 22:47:01  cheshire
<rdar://problem/6134215> Sleep Proxy: Mac with Internet Sharing should also offer Sleep Proxy service
Added SCPreferencesRef so we can track whether Internet Sharing is on or off

Revision 1.79  2008/07/30 00:55:56  mcguire
<rdar://problem/3988320> Should use randomized source ports and transaction IDs to avoid DNS cache poisoning
Additional fixes so that we know when a socket has been closed while in a loop reading from it

Revision 1.78  2008/07/25 22:34:11  mcguire
fix sizecheck issues for 64bit

Revision 1.77  2008/07/24 20:23:04  cheshire
<rdar://problem/3988320> Should use randomized source ports and transaction IDs to avoid DNS cache poisoning

Revision 1.76  2008/07/01 01:40:01  mcguire
<rdar://problem/5823010> 64-bit fixes

Revision 1.75  2007/12/14 00:45:21  cheshire
Add SleepLimit and SleepCookie, for when we need to delay sleep until TLS/TCP record deregistration completes

Revision 1.74  2007/11/02 20:18:13  cheshire
<rdar://problem/5575583> BTMM: Work around keychain notification bug <rdar://problem/5124399>

Revision 1.73  2007/10/17 18:42:06  cheshire
Export SetDomainSecrets so its callable from other files

Revision 1.72  2007/08/01 16:09:14  cheshire
Removed unused NATTraversalInfo substructure from AuthRecord; reduced structure sizecheck values accordingly

Revision 1.71  2007/07/27 23:57:23  cheshire
Added compile-time structure size checks

Revision 1.70  2007/07/11 02:55:50  cheshire
<rdar://problem/5303807> Register IPv6-only hostname and don't create port mappings for AutoTunnel services
Remove unused DefaultRegDomainChanged/DefaultBrowseDomainChanged

Revision 1.69  2007/05/08 00:56:17  cheshire
<rdar://problem/4118503> Share single socket instead of creating separate socket for each active interface

Revision 1.68  2007/04/24 00:10:15  cheshire
Increase WatchDogReportingThreshold to 250ms for customer builds

Revision 1.67  2007/04/21 21:47:47  cheshire
<rdar://problem/4376383> Daemon: Add watchdog timer

Revision 1.66  2007/04/07 01:01:48  cheshire
<rdar://problem/5095167> mDNSResponder periodically blocks in SSLRead

Revision 1.65  2007/03/07 02:50:50  cheshire
<rdar://problem/4574528> Name conflict dialog doesn't appear if Bonjour is persistantly unable to find an available hostname

Revision 1.64  2007/03/06 23:29:50  cheshire
<rdar://problem/4331696> Need to call IONotificationPortDestroy on shutdown

Revision 1.63  2007/02/07 19:32:00  cheshire
<rdar://problem/4980353> All mDNSResponder components should contain version strings in SCCS-compatible format

Revision 1.62  2007/01/05 08:30:49  cheshire
Trim excessive "$Log" checkin history from before 2006
(checkin history still available via "cvs log ..." of course)

Revision 1.61  2006/08/14 23:24:40  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.60  2006/07/27 03:24:35  cheshire
<rdar://problem/4049048> Convert mDNSResponder to use kqueue
Further refinement: Declare KQueueEntry parameter "const"

Revision 1.59  2006/07/27 02:59:25  cheshire
<rdar://problem/4049048> Convert mDNSResponder to use kqueue
Further refinements: CFRunLoop thread needs to explicitly wake the kqueue thread
after releasing BigMutex, in case actions it took have resulted in new work for the
kqueue thread (e.g. NetworkChanged events may result in the kqueue thread having to
add new active interfaces to its list, and consequently schedule queries to be sent).

Revision 1.58  2006/07/22 06:08:29  cheshire
<rdar://problem/4049048> Convert mDNSResponder to use kqueue
Further changes

Revision 1.57  2006/07/22 03:43:26  cheshire
<rdar://problem/4049048> Convert mDNSResponder to use kqueue

Revision 1.56  2006/07/05 23:37:26  cheshire
Remove unused LegacyNATInit/LegacyNATDestroy declarations

Revision 1.55  2006/06/29 05:33:30  cheshire
<rdar://problem/4607043> mDNSResponder conditional compilation options

Revision 1.54  2006/03/19 03:27:49  cheshire
<rdar://problem/4118624> Suppress "interface flapping" logic for loopback

Revision 1.53  2006/03/19 02:00:09  cheshire
<rdar://problem/4073825> Improve logic for delaying packets after repeated interface transitions

Revision 1.52  2006/01/05 21:41:49  cheshire
<rdar://problem/4108164> Reword "mach_absolute_time went backwards" dialog

*/

#ifndef __mDNSOSX_h
#define __mDNSOSX_h

#ifdef  __cplusplus
    extern "C" {
#endif

#include <SystemConfiguration/SystemConfiguration.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "mDNSEmbeddedAPI.h"  // for domain name structure

typedef struct NetworkInterfaceInfoOSX_struct NetworkInterfaceInfoOSX;

typedef void (*KQueueEventCallback)(int fd, short filter, void *context);
typedef struct
	{
	KQueueEventCallback	 KQcallback;
	void                *KQcontext;
	const char const    *KQtask;		// For debugging messages
	} KQueueEntry;

typedef struct
	{
	mDNSIPPort port; // MUST BE FIRST FIELD -- UDPSocket_struct begins with a KQSocketSet,
	// and mDNSCore requires every UDPSocket_struct to begin with a mDNSIPPort port
	mDNS                    *m;
	int                      sktv4;
	KQueueEntry				 kqsv4;
	int                      sktv6;
	KQueueEntry	             kqsv6;
	int                     *closeFlag;
	} KQSocketSet;

struct UDPSocket_struct
	{
	KQSocketSet ss;		// First field of KQSocketSet has to be mDNSIPPort -- mDNSCore requires every UDPSocket_struct to begin with mDNSIPPort port
	};

struct NetworkInterfaceInfoOSX_struct
	{
	NetworkInterfaceInfo     ifinfo;			// MUST be the first element in this structure
	NetworkInterfaceInfoOSX *next;
	mDNS                    *m;
	mDNSu8                   Exists;			// 1 = currently exists in getifaddrs list; 0 = doesn't
												// 2 = exists, but McastTxRx state changed
	mDNSu8                   Flashing;			// Set if interface appeared for less than 60 seconds and then vanished
	mDNSu8                   Occulting;			// Set if interface vanished for less than 60 seconds and then came back
	mDNSs32                  AppearanceTime;	// Time this interface appeared most recently in getifaddrs list
												// i.e. the first time an interface is seen, AppearanceTime is set.
												// If an interface goes away temporarily and then comes back then
												// AppearanceTime is updated to the time of the most recent appearance.
	mDNSs32                  LastSeen;			// If Exists==0, last time this interface appeared in getifaddrs list
	unsigned int             ifa_flags;
	struct in_addr           ifa_v4addr;
	mDNSu32                  scope_id;			// interface index / IPv6 scope ID
	mDNSEthAddr              BSSID;				// BSSID of 802.11 base station, if applicable
	u_short                  sa_family;
	int                      BPF_fd;			// -1 uninitialized; -2 requested BPF; -3 failed
	u_int                    BPF_len;
	CFSocketRef              BPF_cfs;
	CFRunLoopSourceRef       BPF_rls;
	NetworkInterfaceInfoOSX	*Registered;		// non-NULL means registered with mDNS Core
	};

struct mDNS_PlatformSupport_struct
	{
	NetworkInterfaceInfoOSX *InterfaceList;
	KQSocketSet              permanentsockets;
	domainlabel              userhostlabel;		// The hostlabel as it was set in System Preferences the last time we looked
	domainlabel              usernicelabel;		// The nicelabel as it was set in System Preferences the last time we looked
	mDNSs32                  NotifyUser;
	mDNSs32                  HostNameConflict;	// Time we experienced conflict on our link-local host name
	mDNSs32                  NetworkChanged;
	
	// KeyChain frequently fails to notify clients of change events. To work around this
	// we set a timer and periodically poll to detect if any changes have occurred.
	// Without this Back To My Mac just does't work for a large number of users.
	// See <rdar://problem/5124399> Not getting Keychain Changed events when enabling BTMM
	mDNSs32                  KeyChainBugTimer;
	mDNSs32                  KeyChainBugInterval;

	CFRunLoopRef             CFRunLoop;
	SCDynamicStoreRef        Store;
	CFRunLoopSourceRef       StoreRLS;
	CFRunLoopSourceRef       PMRLS;
	int                      SysEventNotifier;
	KQueueEntry              SysEventKQueue;
	IONotificationPortRef    PowerPortRef;
	io_connect_t             PowerConnection;
	io_object_t              PowerNotifier;
#ifdef kIOPMAcknowledgmentOptionSystemCapabilityRequirements
	IOPMConnection           IOPMConnection;
#endif
	SCPreferencesRef         SCPrefs;
	long                     SleepCookie;		// Cookie we need to pass to IOAllowPowerChange()
	long                     WakeAtUTC;
	mDNSs32                  RequestReSleep;
	pthread_mutex_t          BigMutex;
	mDNSs32                  BigMutexStartTime;
	int						 WakeKQueueLoopFD;
	};

extern int OfferSleepProxyService;
extern int OSXVers;
#define OSXVers_Base              4
#define OSXVers_10_0_Cheetah      4
#define OSXVers_10_1_Puma         5
#define OSXVers_10_2_Jaguar       6
#define OSXVers_10_3_Panther      7
#define OSXVers_10_4_Tiger        8
#define OSXVers_10_5_Leopard      9
#define OSXVers_10_6_SnowLeopard 10

extern int KQueueFD;

extern void NotifyOfElusiveBug(const char *title, const char *msg);	// Both strings are UTF-8 text
extern void SetDomainSecrets(mDNS *m);
extern void mDNSMacOSXNetworkChanged(mDNS *const m);
extern int mDNSMacOSXSystemBuildNumber(char *HINFO_SWstring);
extern NetworkInterfaceInfoOSX *IfindexToInterfaceInfoOSX(const mDNS *const m, mDNSInterfaceID ifindex);

extern int KQueueSet(int fd, u_short flags, short filter, const KQueueEntry *const entryRef);

// When events are processed on the non-kqueue thread (i.e. CFRunLoop notifications like Sleep/Wake,
// Interface changes, Keychain changes, etc.) they must use KQueueLock/KQueueUnlock to lock out the kqueue thread
extern void KQueueLock(mDNS *const m);
extern void KQueueUnlock(mDNS *const m, const char const *task);

extern mDNSBool DictionaryIsEnabled(CFDictionaryRef dict);

// If any event takes more than WatchDogReportingThreshold milliseconds to be processed, we log a warning message
// General event categories are:
//  o Mach client request initiated / terminated
//  o UDS client request
//  o Handling UDP packets received from the network
//  o Environmental change events:
//    - network interface changes
//    - sleep/wake
//    - keychain changes
//  o Name conflict dialog dismissal
//  o Reception of Unix signal (e.g. SIGINFO)
//  o Idle task processing
// If we find that we're getting warnings for any of these categories, and it's not evident
// what's causing the problem, we may need to subdivide some categories into finer-grained
// sub-categories (e.g. "Idle task processing" covers a pretty broad range of sub-tasks).

extern int WatchDogReportingThreshold;

struct CompileTimeAssertionChecks_mDNSMacOSX
	{
	// Check our structures are reasonable sizes. Including overly-large buffers, or embedding
	// other overly-large structures instead of having a pointer to them, can inadvertently
	// cause structure sizes (and therefore memory usage) to balloon unreasonably.
	char sizecheck_NetworkInterfaceInfoOSX[(sizeof(NetworkInterfaceInfoOSX) <=  7000) ? 1 : -1];
	char sizecheck_mDNS_PlatformSupport   [(sizeof(mDNS_PlatformSupport)    <=   476) ? 1 : -1];
	};

#ifdef  __cplusplus
    }
#endif

#endif
