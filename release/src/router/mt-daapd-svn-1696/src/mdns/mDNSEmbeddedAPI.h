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


   NOTE:
   If you're building an application that uses DNS Service Discovery
   this is probably NOT the header file you're looking for.
   In most cases you will want to use /usr/include/dns_sd.h instead.

   This header file defines the lowest level raw interface to mDNSCore,
   which is appropriate *only* on tiny embedded systems where everything
   runs in a single address space and memory is extremely constrained.
   All the APIs here are malloc-free, which means that the caller is
   responsible for passing in a pointer to the relevant storage that
   will be used in the execution of that call, and (when called with
   correct parameters) all the calls are guaranteed to succeed. There
   is never a case where a call can suffer intermittent failures because
   the implementation calls malloc() and sometimes malloc() returns NULL
   because memory is so limited that no more is available.
   This is primarily for devices that need to have precisely known fixed
   memory requirements, with absolutely no uncertainty or run-time variation,
   but that certainty comes at a cost of more difficult programming.
   
   For applications running on general-purpose desktop operating systems
   (Mac OS, Linux, Solaris, Windows, etc.) the API you should use is
   /usr/include/dns_sd.h, which defines the API by which multiple
   independent client processes communicate their DNS Service Discovery
   requests to a single "mdnsd" daemon running in the background.
   
   Even on platforms that don't run multiple independent processes in
   multiple independent address spaces, you can still use the preferred
   dns_sd.h APIs by linking in "dnssd_clientshim.c", which implements
   the standard "dns_sd.h" API calls, allocates any required storage
   using malloc(), and then calls through to the low-level malloc-free
   mDNSCore routines defined here. This has the benefit that even though
   you're running on a small embedded system with a single address space,
   you can still use the exact same client C code as you'd use on a
   general-purpose desktop system.


    Change History (most recent first):

$Log: mDNSEmbeddedAPI.h,v $
Revision 1.296.2.1  2006/08/29 06:24:22  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.296  2006/06/29 05:28:01  cheshire
Added comment about mDNSlocal and mDNSexport

Revision 1.295  2006/06/29 03:02:43  cheshire
<rdar://problem/4607042> mDNSResponder NXDOMAIN and CNAME support

Revision 1.294  2006/06/28 06:50:08  cheshire
In future we may want to change definition of mDNSs32 from "signed long" to "signed int"
I doubt anyone is building mDNSResponder on systems where int is 16-bits,
but lets add a compile-time assertion to make sure.

Revision 1.293  2006/06/12 18:00:43  cheshire
To make code a little more defensive, check _ILP64 before _LP64,
in case both are set by mistake on some platforms

Revision 1.292  2006/03/19 17:00:57  cheshire
Define symbol MaxMsg instead of using hard-coded constant value '80'

Revision 1.291  2006/03/19 02:00:07  cheshire
<rdar://problem/4073825> Improve logic for delaying packets after repeated interface transitions

Revision 1.290  2006/03/08 22:42:23  cheshire
Fix spelling mistake: LocalReverseMapomain -> LocalReverseMapDomain

Revision 1.289  2006/02/26 00:54:41  cheshire
Fixes to avoid code generation warning/error on FreeBSD 7

Revision 1.288  2005/12/21 03:24:58  cheshire
<rdar://problem/4388858> Code changes required to compile on EFI

Revision 1.287  2005/10/20 00:10:33  cheshire
<rdar://problem/4290265> Add check to avoid crashing NAT gateways that have buggy DNS relay code

Revision 1.286  2005/09/24 01:09:40  cheshire
Fix comment typos

Revision 1.285  2005/09/16 20:57:47  cheshire
Add macro mDNS_TimeNow_NoLock(m) to get properly adjusted time without also acquiring lock

Revision 1.284  2005/07/29 18:04:22  ksekar
<rdar://problem/4137930> Hostname registration should register IPv6 AAAA record with DNS Update

Revision 1.283  2005/05/13 20:45:09  ksekar
<rdar://problem/4074400> Rapid wide-area txt record updates don't work

Revision 1.282  2005/03/16 00:42:32  ksekar
<rdar://problem/4012279> Long-lived queries not working on Windows

Revision 1.281  2005/02/25 17:47:44  ksekar
<rdar://problem/4021868> SendServiceRegistration fails on wake from sleep

Revision 1.280  2005/02/25 04:21:00  cheshire
<rdar://problem/4015377> mDNS -F returns the same domain multiple times with different casing

Revision 1.279  2005/02/17 01:56:14  cheshire
Increase ifname field to 64 bytes

Revision 1.278  2005/02/09 23:38:51  ksekar
<rdar://problem/3993508> Reregister hostname when DNS server changes but IP address does not

Revision 1.277  2005/02/09 23:31:12  ksekar
<rdar://problem/3984374> NAT-PMP response callback should return a boolean indicating if the packet matched the request

Revision 1.276  2005/02/01 19:33:29  ksekar
<rdar://problem/3985239> Keychain format too restrictive

Revision 1.275  2005/01/27 22:57:55  cheshire
Fix compile errors on gcc4

Revision 1.274  2005/01/19 21:01:54  ksekar
<rdar://problem/3955355> uDNS needs to support subtype registration and browsing

Revision 1.273  2005/01/19 19:15:31  ksekar
Refinement to <rdar://problem/3954575> - Simplify mDNS_PurgeResultsForDomain logic and move into daemon layer

Revision 1.272  2005/01/18 18:10:55  ksekar
<rdar://problem/3954575> Use 10.4 resolver API to get search domains

Revision 1.271  2005/01/15 00:56:41  ksekar
<rdar://problem/3954575> Unicast services don't disappear when logging
out of VPN

Revision 1.270  2005/01/14 18:34:22  ksekar
<rdar://problem/3954571> Services registered outside of firewall don't succeed after location change

Revision 1.269  2005/01/11 22:50:52  ksekar
Fixed constant naming (was using kLLQ_DefLease for update leases)

Revision 1.268  2004/12/22 22:25:47  ksekar
<rdar://problem/3734265> NATPMP: handle location changes

Revision 1.267  2004/12/22 00:13:49  ksekar
<rdar://problem/3873993> Change version, port, and polling interval for LLQ

Revision 1.266  2004/12/18 03:13:45  cheshire
<rdar://problem/3751638> kDNSServiceInterfaceIndexLocalOnly should return all local records

Revision 1.265  2004/12/17 23:37:45  cheshire
<rdar://problem/3485365> Guard against repeating wireless dissociation/re-association
(and other repetitive configuration changes)

Revision 1.264  2004/12/17 05:25:46  cheshire
<rdar://problem/3925163> Shorten DNS-SD queries to avoid NAT bugs

Revision 1.263  2004/12/16 20:40:25  cheshire
Fix compile warnings

Revision 1.262  2004/12/16 20:13:00  cheshire
<rdar://problem/3324626> Cache memory management improvements

Revision 1.261  2004/12/14 21:21:20  ksekar
<rdar://problem/3825979> NAT-PMP: Update response format to contain "Seconds Since Boot"

Revision 1.260  2004/12/12 23:51:42  ksekar
<rdar://problem/3845683> Wide-area registrations should fallback to using DHCP hostname as target

Revision 1.259  2004/12/11 20:55:29  ksekar
<rdar://problem/3916479> Clean up registration state machines

Revision 1.258  2004/12/10 20:48:32  cheshire
<rdar://problem/3705229> Need to pick final EDNS numbers for LLQ and GC

Revision 1.257  2004/12/10 02:09:23  cheshire
<rdar://problem/3898376> Modify default TTLs

Revision 1.256  2004/12/09 03:15:40  ksekar
<rdar://problem/3806610> use _legacy instead of _default to find "empty string" browse domains

Revision 1.255  2004/12/07 22:48:37  cheshire
Tidying

Revision 1.254  2004/12/07 21:26:04  ksekar
<rdar://problem/3908336> DNSServiceRegisterRecord() can crash on deregistration

Revision 1.253  2004/12/07 20:42:33  cheshire
Add explicit context parameter to mDNS_RemoveRecordFromService()

Revision 1.252  2004/12/07 03:02:12  ksekar
Fixed comments, grouped unicast-specific routines together

Revision 1.251  2004/12/06 21:15:22  ksekar
<rdar://problem/3884386> mDNSResponder crashed in CheckServiceRegistrations

Revision 1.250  2004/12/04 02:12:45  cheshire
<rdar://problem/3517236> mDNSResponder puts LargeCacheRecord on the stack

Revision 1.249  2004/12/03 05:18:33  ksekar
<rdar://problem/3810596> mDNSResponder needs to return more specific TSIG errors

Revision 1.248  2004/12/02 20:03:48  ksekar
<rdar://problem/3889647> Still publishes wide-area domains even after switching to a local subnet

Revision 1.247  2004/12/01 20:57:19  ksekar
<rdar://problem/3873921> Wide Area Service Discovery must be split-DNS aware

Revision 1.246  2004/11/29 23:26:32  cheshire
Added NonZeroTime() function, which usually returns the value given, with the exception
that if the value given is zero, it returns one instead. For timer values where zero is
used to mean "not set", this can be used to ensure that setting them to the result of an
interval computation (e.g. "now+interval") does not inadvertently result in a zero value.

Revision 1.245  2004/11/25 01:28:09  cheshire
<rdar://problem/3557050> Need to implement random delay for 'QU' unicast replies (and set cache flush bit too)

Revision 1.244  2004/11/24 22:00:59  cheshire
Move definition of mDNSAddressIsAllDNSLinkGroup() from mDNSMacOSX.c to mDNSEmbeddedAPI.h

Revision 1.243  2004/11/23 22:43:53  cheshire
Tidy up code alignment

Revision 1.242  2004/11/23 03:39:46  cheshire
Let interface name/index mapping capability live directly in JNISupport.c,
instead of having to call through to the daemon via IPC to get this information.

Revision 1.241  2004/11/22 17:16:19  ksekar
<rdar://problem/3854298> Unicast services don't disappear when you disable all networking

Revision 1.240  2004/11/19 02:32:43  ksekar
Wide-Area Security: Add LLQ-ID to events

Revision 1.239  2004/11/15 20:09:23  ksekar
<rdar://problem/3719050> Wide Area support for Add/Remove record

Revision 1.238  2004/11/12 03:16:48  rpantos
rdar://problem/3809541 Add mDNSPlatformGetInterfaceByName, mDNSPlatformGetInterfaceName

Revision 1.237  2004/11/10 20:40:53  ksekar
<rdar://problem/3868216> LLQ mobility fragile on non-primary interface

Revision 1.236  2004/11/01 20:36:11  ksekar
<rdar://problem/3802395> mDNSResponder should not receive Keychain Notifications

Revision 1.235  2004/11/01 17:48:14  cheshire
Changed SOA serial number back to signed. RFC 1035 may describe it as "unsigned", but
it's wrong. The SOA serial is a modular counter, as explained in "DNS & BIND", page
137. Since C doesn't have a modular type, we used signed, C's closest approximation.

Revision 1.234  2004/10/29 21:59:02  ksekar
SOA serial should be a unsigned integer, as per RFC 1035

Revision 1.233  2004/10/28 03:24:41  cheshire
Rename m->CanReceiveUnicastOn as m->CanReceiveUnicastOn5353

Revision 1.232  2004/10/26 06:20:23  cheshire
Add mDNSAddressIsValidNonZero() macro

Revision 1.231  2004/10/26 06:11:41  cheshire
Add improved logging to aid in diagnosis of <rdar://problem/3842714> mDNSResponder crashed

Revision 1.230  2004/10/26 03:52:02  cheshire
Update checkin comments

Revision 1.229  2004/10/25 19:30:52  ksekar
<rdar://problem/3827956> Simplify dynamic host name structures

Revision 1.228  2004/10/23 01:16:00  cheshire
<rdar://problem/3851677> uDNS operations not always reliable on multi-homed hosts

Revision 1.227  2004/10/22 20:52:07  ksekar
<rdar://problem/3799260> Create NAT port mappings for Long Lived Queries

Revision 1.226  2004/10/20 01:50:40  cheshire
<rdar://problem/3844991> Cannot resolve non-local registrations using the mach API
Implemented ForceMCast mode for AuthRecords as well as for Questions

Revision 1.225  2004/10/19 21:33:17  cheshire
<rdar://problem/3844991> Cannot resolve non-local registrations using the mach API
Added flag 'kDNSServiceFlagsForceMulticast'. Passing through an interface id for a unicast name
doesn't force multicast unless you set this flag to indicate explicitly that this is what you want

Revision 1.224  2004/10/16 00:16:59  cheshire
<rdar://problem/3770558> Replace IP TTL 255 check with local subnet source address check

Revision 1.223  2004/10/15 23:00:17  ksekar
<rdar://problem/3799242> Need to update LLQs on location changes

Revision 1.222  2004/10/12 02:49:20  ksekar
<rdar://problem/3831228> Clean up LLQ sleep/wake, error handling

Revision 1.221  2004/10/10 06:57:15  cheshire
Change definition of "localdomain" to make code compile a little smaller

Revision 1.220  2004/10/06 01:44:19  cheshire
<rdar://problem/3813936> Resolving too quickly sometimes returns stale TXT record

Revision 1.219  2004/10/03 23:18:58  cheshire
Move address comparison macros from DNSCommon.h to mDNSEmbeddedAPI.h

Revision 1.218  2004/10/03 23:14:12  cheshire
Add "mDNSEthAddr" type and "zeroEthAddr" constant

Revision 1.217  2004/09/30 00:24:56  ksekar
<rdar://problem/3695802> Dynamically update default registration domains on config change

Revision 1.216  2004/09/27 23:24:32  cheshire
Fix typo: SOA refresh interval is supposed to be unsigned

Revision 1.215  2004/09/26 23:20:35  ksekar
<rdar://problem/3813108> Allow default registrations in multiple wide-area domains

Revision 1.214  2004/09/25 02:41:39  cheshire
<rdar://problem/3637266> Deliver near-pending "remove" events before new "add" events

Revision 1.213  2004/09/25 02:24:27  cheshire
Removed unused rr->UseCount

Revision 1.212  2004/09/24 20:57:39  cheshire
<rdar://problem/3680902> Eliminate inappropriate casts that cause misaligned-address errors

Revision 1.211  2004/09/24 20:33:22  cheshire
Remove unused DNSDigest_MD5 declaration

Revision 1.210  2004/09/23 20:21:07  cheshire
<rdar://problem/3426876> Refine "immediate answer burst; restarting exponential backoff sequence" logic
Associate a unique sequence number with each received packet, and only increment the count of recent answer
packets if the packet sequence number for this answer record is not one we've already seen and counted.

Revision 1.209  2004/09/23 20:14:39  cheshire
Rename "question->RecentAnswers" to "question->RecentAnswerPkts"

Revision 1.208  2004/09/23 00:50:53  cheshire
<rdar://problem/3419452> Don't send a (DE) if a service is unregistered after wake from sleep

Revision 1.207  2004/09/22 02:34:46  cheshire
Move definitions of default TTL times from mDNS.c to mDNSEmbeddedAPI.h

Revision 1.206  2004/09/22 00:41:59  cheshire
Move tcp connection status codes into the legal range allocated for mDNS use

Revision 1.205  2004/09/21 23:40:11  ksekar
<rdar://problem/3810349> mDNSResponder to return errors on NAT traversal failure

Revision 1.204  2004/09/21 23:29:50  cheshire
<rdar://problem/3680045> DNSServiceResolve should delay sending packets

Revision 1.203  2004/09/21 20:58:22  cheshire
Add ifname field to NetworkInterfaceInfo_struct

Revision 1.202  2004/09/17 00:46:34  cheshire
mDNS_TimeNow should take const mDNS parameter

Revision 1.201  2004/09/17 00:31:51  cheshire
For consistency with ipv6, renamed rdata field 'ip' to 'ipv4'

Revision 1.200  2004/09/17 00:19:10  cheshire
For consistency with AllDNSLinkGroupv6, rename AllDNSLinkGroup to AllDNSLinkGroupv4

Revision 1.199  2004/09/16 21:59:16  cheshire
For consistency with zerov6Addr, rename zeroIPAddr to zerov4Addr

Revision 1.198  2004/09/16 21:36:36  cheshire
<rdar://problem/3803162> Fix unsafe use of mDNSPlatformTimeNow()
Changes to add necessary locking calls around unicast DNS operations

Revision 1.197  2004/09/16 00:24:48  cheshire
<rdar://problem/3803162> Fix unsafe use of mDNSPlatformTimeNow()

Revision 1.196  2004/09/14 23:42:35  cheshire
<rdar://problem/3801296> Need to seed random number generator from platform-layer data

Revision 1.195  2004/09/14 23:27:46  cheshire
Fix compile errors

Revision 1.194  2004/09/10 00:49:57  cheshire
<rdar://problem/3787644> Add error code kDNSServiceErr_Firewall, for future use

Revision 1.193  2004/09/03 19:23:05  ksekar
<rdar://problem/3788460>: Need retransmission mechanism for wide-area service registrations

Revision 1.192  2004/09/02 03:48:47  cheshire
<rdar://problem/3709039> Disable targeted unicast query support by default
1. New flag kDNSServiceFlagsAllowRemoteQuery to indicate we want to allow remote queries for this record
2. New field AllowRemoteQuery in AuthRecord structure
3. uds_daemon.c sets AllowRemoteQuery if kDNSServiceFlagsAllowRemoteQuery is set
4. mDNS.c only answers remote queries if AllowRemoteQuery is set

Revision 1.191  2004/08/25 00:37:27  ksekar
<rdar://problem/3774635>: Cleanup DynDNS hostname registration code

Revision 1.190  2004/08/18 17:35:41  ksekar
<rdar://problem/3651443>: Feature #9586: Need support for Legacy NAT gateways

Revision 1.189  2004/08/14 03:22:41  cheshire
<rdar://problem/3762579> Dynamic DNS UI <-> mDNSResponder glue
Add GetUserSpecifiedDDNSName() routine
Convert ServiceRegDomain to domainname instead of C string
Replace mDNS_GenerateFQDN/mDNS_GenerateGlobalFQDN with mDNS_SetFQDNs

Revision 1.188  2004/08/13 23:46:58  cheshire
"asyncronous" -> "asynchronous"

Revision 1.187  2004/08/13 23:37:02  cheshire
Now that we do both uDNS and mDNS, global replace "uDNS_info.hostname" with
"uDNS_info.UnicastHostname" for clarity

Revision 1.186  2004/08/13 23:25:00  cheshire
Now that we do both uDNS and mDNS, global replace "m->hostname" with
"m->MulticastHostname" for clarity

Revision 1.185  2004/08/12 00:32:36  ksekar
<rdar://problem/3759567>: LLQ Refreshes never terminate if unanswered

Revision 1.184  2004/08/11 17:09:31  cheshire
Add comment clarifying the applicability of these APIs

Revision 1.183  2004/08/10 23:19:14  ksekar
<rdar://problem/3722542>: DNS Extension daemon for Wide Area Service Discovery
Moved routines/constants to allow extern access for garbage collection daemon

Revision 1.182  2004/07/30 17:40:06  ksekar
<rdar://problem/3739115>: TXT Record updates not available for wide-area services

Revision 1.181  2004/07/29 19:27:15  ksekar
NATPMP Support - minor fixes and cleanup

Revision 1.180  2004/07/29 02:03:35  ksekar
Delete unused #define and structure field

Revision 1.179  2004/07/26 22:49:30  ksekar
<rdar://problem/3651409>: Feature #9516: Need support for NATPMP in client

Revision 1.178  2004/07/13 21:24:24  rpantos
Fix for <rdar://problem/3701120>.

Revision 1.177  2004/06/05 00:04:26  cheshire
<rdar://problem/3668639>: wide-area domains should be returned in reg. domain enumeration

Revision 1.176  2004/06/04 08:58:29  ksekar
<rdar://problem/3668624>: Keychain integration for secure dynamic update

Revision 1.175  2004/06/04 00:15:06  cheshire
Move misplaced brackets

Revision 1.174  2004/06/03 23:30:16  cheshire
Remove extraneous blank lines and white space

Revision 1.173  2004/06/03 03:09:58  ksekar
<rdar://problem/3668626>: Garbage Collection for Dynamic Updates

Revision 1.172  2004/06/01 23:46:50  ksekar
<rdar://problem/3675149>: DynDNS: dynamically look up LLQ/Update ports

Revision 1.171  2004/05/28 23:42:37  ksekar
<rdar://problem/3258021>: Feature: DNS server->client notification on record changes (#7805)

Revision 1.170  2004/05/18 23:51:25  cheshire
Tidy up all checkin comments to use consistent "<rdar://problem/xxxxxxx>" format for bug numbers

Revision 1.169  2004/05/13 04:54:20  ksekar
Unified list copy/free code.  Added symetric list for

Revision 1.168  2004/05/12 22:03:09  ksekar
Made GetSearchDomainList a true platform-layer call (declaration moved
from mDNSMacOSX.h to mDNSEmbeddedAPI.h), implemented to return "local"
only on non-OSX platforms.  Changed call to return a copy of the list
to avoid shared memory issues.  Added a routine to free the list.

Revision 1.167  2004/04/22 04:07:01  cheshire
Fix from Bob Bradley: Don't try to do inline functions on compilers that don't support it

Revision 1.166  2004/04/22 03:15:56  cheshire
Fix use of "struct __attribute__((__packed__))" so it only applies on GCC >= 2.9

Revision 1.165  2004/04/22 03:05:28  cheshire
kDNSClass_ANY should be kDNSQClass_ANY

Revision 1.164  2004/04/21 02:55:03  cheshire
Update comments describing 'InterfaceActive' field

Revision 1.163  2004/04/21 02:49:11  cheshire
To reduce future confusion, renamed 'TxAndRx' to 'McastTxRx'

Revision 1.162  2004/04/15 00:51:28  bradley
Minor tweaks for Windows and C++ builds. Added casts for signed/unsigned integers and 64-bit pointers.
Prefix some functions with mDNS to avoid conflicts. Disable benign warnings on Microsoft compilers.

Revision 1.161  2004/04/14 23:09:28  ksekar
Support for TSIG signed dynamic updates.

Revision 1.160  2004/04/09 17:40:26  cheshire
Remove unnecessary "Multicast" field -- it duplicates the semantics of the existing McastTxRx field

Revision 1.159  2004/04/09 16:37:15  cheshire
Suggestion from Bob Bradley:
Move NumCacheRecordsForInterfaceID() to DNSCommon.c so it's available to all platform layers

Revision 1.158  2004/04/02 19:38:33  cheshire
Update comment about typical RR TTLs

Revision 1.157  2004/04/02 19:35:53  cheshire
Add clarifying comments about legal mDNSInterfaceID values

Revision 1.156  2004/04/02 19:19:48  cheshire
Add code to do optional logging of multi-packet KA list time intervals

Revision 1.155  2004/03/24 00:29:45  ksekar
Make it safe to call StopQuery in a unicast question callback

Revision 1.154  2004/03/20 01:05:49  cheshire
Test __LP64__ and __ILP64__ to compile properly on a wider range of 64-bit architectures

Revision 1.153  2004/03/13 01:57:33  ksekar
<rdar://problem/3192546>: DynDNS: Dynamic update of service records

Revision 1.152  2004/03/09 02:27:16  cheshire
Remove erroneous underscore in 'packed_struct' (makes no difference now, but might in future)

Revision 1.151  2004/03/02 03:21:56  cheshire
<rdar://problem/3549576> Properly support "_services._dns-sd._udp" meta-queries

Revision 1.150  2004/02/21 02:06:24  cheshire
Can't use anonymous unions -- they're non-standard and don't work on all compilers

Revision 1.149  2004/02/06 23:04:19  ksekar
Basic Dynamic Update support via mDNS_Register (dissabled via
UNICAST_REGISTRATION #define)

Revision 1.148  2004/02/03 19:47:36  ksekar
Added an asynchronous state machine mechanism to uDNS.c, including
calls to find the parent zone for a domain name.  Changes include code
in repository previously dissabled via "#if 0 incomplete".  Codepath
is currently unused, and will be called to create update records, etc.

Revision 1.147  2004/02/03 18:57:35  cheshire
Update comment for "IsLocalDomain()"

Revision 1.146  2004/01/30 02:20:24  bradley
Map inline to __inline when building with Microsoft C compilers since they do not support C99 inline.

Revision 1.145  2004/01/29 02:59:17  ksekar
Unicast DNS: Changed from a resource record oriented question/response
matching to packet based matching.  New callback architecture allows
collections of records in a response to be processed differently
depending on the nature of the request, and allows the same structure
to be used for internal and client-driven queries with different processing needs.

Revision 1.144  2004/01/28 20:20:45  ksekar
Unified ActiveQueries and ActiveInternalQueries lists, using a flag to
demux them.  Check-in includes work-in-progress code, #ifdef'd out.

Revision 1.143  2004/01/28 03:41:00  cheshire
<rdar://problem/3541946>: Need ability to do targeted queries as well as multicast queries

Revision 1.142  2004/01/28 02:30:07  ksekar
Added default Search Domains to unicast browsing, controlled via
Networking sharing prefs pane.  Stopped sending unicast messages on
every interface.  Fixed unicast resolving via mach-port API.

Revision 1.141  2004/01/27 20:15:22  cheshire
<rdar://problem/3541288>: Time to prune obsolete code for listening on port 53

Revision 1.140  2004/01/24 23:37:08  cheshire
At Kiren's suggestion, made functions to convert mDNSOpaque16s to/from integer values

Revision 1.139  2004/01/24 08:46:26  bradley
Added InterfaceID<->Index platform interfaces since they are now used by all platforms for the DNS-SD APIs.

Revision 1.138  2004/01/24 04:59:15  cheshire
Fixes so that Posix/Linux, OS9, Windows, and VxWorks targets build again

Revision 1.137  2004/01/24 03:40:56  cheshire
Move mDNSAddrIsDNSMulticast() from DNSCommon.h to mDNSEmbeddedAPI.h so clients can use it

Revision 1.136  2004/01/24 03:38:27  cheshire
Fix minor syntactic error: Headers should use "extern" declarations, not "mDNSexport"

Revision 1.135  2004/01/23 23:23:15  ksekar
Added TCP support for truncated unicast messages.

Revision 1.134  2004/01/22 03:54:11  cheshire
Create special meta-interface 'mDNSInterface_ForceMCast' (-2),
which means "do this query via multicast, even if it's apparently a unicast domain"

Revision 1.133  2004/01/22 03:48:41  cheshire
Make sure uDNS client doesn't accidentally use query ID zero

Revision 1.132  2004/01/22 03:43:08  cheshire
Export constants like mDNSInterface_LocalOnly so that the client layers can use them

Revision 1.131  2004/01/21 21:53:18  cheshire
<rdar://problem/3448144>: Don't try to receive unicast responses if we're not the first to bind to the UDP port

Revision 1.130  2003/12/14 05:05:29  cheshire
Add comments explaining mDNS_Init_NoCache and mDNS_Init_ZeroCacheSize

Revision 1.129  2003/12/13 03:05:27  ksekar
<rdar://problem/3192548>: DynDNS: Unicast query of service records

Revision 1.128  2003/12/01 21:44:23  cheshire
Add mStatus_BadInterfaceErr = -65552 for consistency with dns_sd.h

Revision 1.127  2003/12/01 18:26:37  cheshire
Also pack the OpaqueXX union types. Otherwise, on some systems, mDNSOpaque16 is four bytes!

Revision 1.126  2003/12/01 18:23:48  cheshire
<rdar://problem/3464646>: Scalar size problem in mDNS code on some 64-bit architectures

Revision 1.125  2003/11/22 00:18:27  cheshire
Add compile-time asserts to verify correct sizes of mDNSu32, mDNSOpaque16, etc.

Revision 1.124  2003/11/20 22:59:54  cheshire
Changed runtime checks in mDNS.c to be compile-time checks in mDNSEmbeddedAPI.h
Thanks to Bob Bradley for suggesting the ingenious compiler trick to make this work.

Revision 1.123  2003/11/20 22:53:01  cheshire
Add comment about MAX_ESCAPED_DOMAIN_LABEL

Revision 1.122  2003/11/20 20:49:53  cheshire
Another fix from HP: Use packedstruct macro to ensure proper packing for on-the-wire packet structures

Revision 1.121  2003/11/20 05:01:38  cheshire
Update comments; add explanation of Advertise/DontAdvertiseLocalAddresses

Revision 1.120  2003/11/14 20:59:08  cheshire
Clients can't use AssignDomainName macro because mDNSPlatformMemCopy is defined in mDNSPlatformFunctions.h.
Best solution is just to combine mDNSEmbeddedAPI.h and mDNSPlatformFunctions.h into a single file.

Revision 1.119  2003/11/14 19:47:52  cheshire
Define symbol MAX_ESCAPED_DOMAIN_NAME to indicate recommended buffer size for ConvertDomainNameToCString

Revision 1.118  2003/11/14 19:18:34  cheshire
Move AssignDomainName macro to mDNSEmbeddedAPI.h to that client layers can use it too

Revision 1.117  2003/11/08 23:32:24  cheshire
Gave name to anonymous struct, to avoid errors on certain compilers.
(Thanks to ramaprasad.kr@hp.com for reporting this.)

Revision 1.116  2003/11/07 03:32:56  cheshire
<rdar://problem/3472153> mDNSResponder delivers answers in inconsistent order
This is the real fix. Checkin 1.312 was overly simplistic; Calling GetFreeCacheRR() can sometimes
purge records from the cache, causing tail pointer *rp to be stale on return. The correct fix is
to maintain a system-wide tail pointer for each cache slot, and then if neccesary GetFreeCacheRR()
can update this pointer, so that mDNSCoreReceiveResponse() appends records in the right place.

Revision 1.115  2003/09/23 00:53:54  cheshire
NumFailedProbes should be unsigned

Revision 1.114  2003/08/29 19:44:15  cheshire
<rdar://problem/3400967> Traffic reduction: Eliminate synchronized QUs when a new service appears
1. Use m->RandomQueryDelay to impose a random delay in the range 0-500ms on queries
   that already have at least one unique answer in the cache
2. For these queries, go straight to QM, skipping QU

Revision 1.113  2003/08/21 19:31:58  cheshire
Cosmetic: Swap order of fields

Revision 1.112  2003/08/21 19:27:36  cheshire
<rdar://problem/3387878> Traffic reduction: No need to announce record for longer than TTL

Revision 1.111  2003/08/21 02:21:50  cheshire
<rdar://problem/3386473> Efficiency: Reduce repeated queries

Revision 1.110  2003/08/20 23:39:31  cheshire
<rdar://problem/3344098> Review syslog messages, and remove as appropriate

Revision 1.109  2003/08/19 22:24:10  cheshire
Comment change

Revision 1.108  2003/08/19 22:20:00  cheshire
<rdar://problem/3376721> Don't use IPv6 on interfaces that have a routable IPv4 address configured
More minor refinements

Revision 1.107  2003/08/19 06:48:25  cheshire
<rdar://problem/3376552> Guard against excessive record updates
Each record starts with 10 UpdateCredits.
Every update consumes one UpdateCredit.
UpdateCredits are replenished at a rate of one one per minute, up to a maximum of 10.
As the number of UpdateCredits declines, the number of announcements is similarly scaled back.
When fewer than 5 UpdateCredits remain, the first announcement is also delayed by an increasing amount.

Revision 1.106  2003/08/19 04:49:28  cheshire
<rdar://problem/3368159> Interaction between v4, v6 and dual-stack hosts not working quite right
1. A dual-stack host should only suppress its own query if it sees the same query from other hosts on BOTH IPv4 and IPv6.
2. When we see the first v4 (or first v6) member of a group, we re-trigger questions and probes on that interface.
3. When we see the last v4 (or v6) member of a group go away, we revalidate all the records received on that interface.

Revision 1.105  2003/08/19 02:33:37  cheshire
Update comments

Revision 1.104  2003/08/19 02:31:11  cheshire
<rdar://problem/3378386> mDNSResponder overenthusiastic with final expiration queries
Final expiration queries now only mark the question for sending on the particular interface
pertaining to the record that's expiring.

Revision 1.103  2003/08/18 19:05:44  cheshire
<rdar://problem/3382423> UpdateRecord not working right
Added "newrdlength" field to hold new length of updated rdata

Revision 1.102  2003/08/16 03:39:00  cheshire
<rdar://problem/3338440> InterfaceID -1 indicates "local only"

Revision 1.101  2003/08/15 20:16:02  cheshire
<rdar://problem/3366590> mDNSResponder takes too much RPRVT
We want to avoid touching the rdata pages, so we don't page them in.
1. RDLength was stored with the rdata, which meant touching the page just to find the length.
   Moved this from the RData to the ResourceRecord object.
2. To avoid unnecessarily touching the rdata just to compare it,
   compute a hash of the rdata and store the hash in the ResourceRecord object.

Revision 1.100  2003/08/14 19:29:04  cheshire
<rdar://problem/3378473> Include cache records in SIGINFO output
Moved declarations of DNSTypeName() and GetRRDisplayString to mDNSEmbeddedAPI.h so daemon.c can use them

Revision 1.99  2003/08/14 02:17:05  cheshire
<rdar://problem/3375491> Split generic ResourceRecord type into two separate types: AuthRecord and CacheRecord

Revision 1.98  2003/08/12 19:56:23  cheshire
Update to APSL 2.0

Revision 1.97  2003/08/12 14:59:27  cheshire
<rdar://problem/3374490> Rate-limiting blocks some legitimate responses
When setting LastMCTime also record LastMCInterface. When checking LastMCTime to determine
whether to suppress the response, also check LastMCInterface to see if it matches.

Revision 1.96  2003/08/12 13:57:04  cheshire
<rdar://problem/3323817> Improve cache performance
Changed the number of hash table slots from 37 to 499

Revision 1.95  2003/08/09 00:55:02  cheshire
<rdar://problem/3366553> mDNSResponder is taking 20-30% of the CPU
Don't scan the whole cache after every packet.

Revision 1.94  2003/08/09 00:35:29  cheshire

Revision 1.93  2003/08/08 18:55:48  cheshire
<rdar://problem/3370365> Guard against time going backwards

Revision 1.92  2003/08/08 18:36:04  cheshire
<rdar://problem/3344154> Only need to revalidate on interface removal on platforms that have the PhantomInterfaces bug

Revision 1.91  2003/08/06 21:33:39  cheshire
Fix compiler warnings on PocketPC 2003 (Windows CE)

Revision 1.90  2003/08/06 20:30:17  cheshire
Add structure definition for rdataMX (not currently used, but good to have it for completeness)

Revision 1.89  2003/08/06 18:58:19  cheshire
Update comments

Revision 1.88  2003/07/24 23:45:44  cheshire
To eliminate compiler warnings, changed definition of mDNSBool from
"unsigned char" to "int", since "int" is in fact truly the type that C uses
for the result of comparison operators (a<b) and logical operators (a||b)

Revision 1.87  2003/07/22 23:57:20  cheshire
Move platform-layer function prototypes from mDNSEmbeddedAPI.h to mDNSPlatformFunctions.h where they belong

Revision 1.86  2003/07/20 03:52:02  ksekar
<rdar://problem/3320722>: Feature: New DNS-SD APIs (#7875) (mDNSResponder component)
Added error type for incompatibility between daemon and client versions

Revision 1.85  2003/07/19 03:23:13  cheshire
<rdar://problem/2986147> mDNSResponder needs to receive and cache larger records

Revision 1.84  2003/07/18 23:52:12  cheshire
To improve consistency of field naming, global search-and-replace:
NextProbeTime    -> NextScheduledProbe
NextResponseTime -> NextScheduledResponse

Revision 1.83  2003/07/18 00:29:59  cheshire
<rdar://problem/3268878> Remove mDNSResponder version from packet header and use HINFO record instead

Revision 1.82  2003/07/17 17:35:04  cheshire
<rdar://problem/3325583> Rate-limit responses, to guard against packet flooding

Revision 1.81  2003/07/16 05:01:36  cheshire
Add fields 'LargeAnswers' and 'ExpectUnicastResponse' in preparation for
<rdar://problem/3315761> Need to implement "unicast response" request, using top bit of qclass

Revision 1.80  2003/07/15 01:55:12  cheshire
<rdar://problem/3315777> Need to implement service registration with subtypes

Revision 1.79  2003/07/13 02:28:00  cheshire
<rdar://problem/3325166> SendResponses didn't all its responses
Delete all references to RRInterfaceActive -- it's now superfluous

Revision 1.78  2003/07/13 01:47:53  cheshire
Fix one error and one warning in the Windows build

Revision 1.77  2003/07/11 01:32:38  cheshire
Syntactic cleanup (no change to funcationality): Now that we only have one host name,
rename field "hostname1" to "hostname", and field "RR_A1" to "RR_A".

Revision 1.76  2003/07/11 01:28:00  cheshire
<rdar://problem/3161289> No more local.arpa

Revision 1.75  2003/07/02 21:19:45  cheshire
<rdar://problem/3313413> Update copyright notices, etc., in source code comments

Revision 1.74  2003/07/02 02:41:23  cheshire
<rdar://problem/2986146> mDNSResponder needs to start with a smaller cache and then grow it as needed

Revision 1.73  2003/06/10 04:24:39  cheshire
<rdar://problem/3283637> React when we observe other people query unsuccessfully for a record that's in our cache
Some additional refinements:
Don't try to do this for unicast-response queries
better tracking of Qs and KAs in multi-packet KA lists

Revision 1.72  2003/06/10 01:46:27  cheshire
Add better comments explaining how these data structures are intended to be used from the client layer

Revision 1.71  2003/06/07 06:45:05  cheshire
<rdar://problem/3283666> No need for multiple machines to all be sending the same queries

Revision 1.70  2003/06/07 04:50:53  cheshire
<rdar://problem/3283637> React when we observe other people query unsuccessfully for a record that's in our cache

Revision 1.69  2003/06/07 04:22:17  cheshire
Add MsgBuffer for error log and debug messages

Revision 1.68  2003/06/07 01:46:38  cheshire
<rdar://problem/3283540> When query produces zero results, call mDNS_Reconfirm() on any antecedent records

Revision 1.67  2003/06/07 01:22:14  cheshire
<rdar://problem/3283516> mDNSResponder needs an mDNS_Reconfirm() function

Revision 1.66  2003/06/07 00:59:43  cheshire
<rdar://problem/3283454> Need some randomness to spread queries on the network

Revision 1.65  2003/06/06 21:41:11  cheshire
For consistency, mDNS_StopQuery() should return an mStatus result, just like all the other mDNSCore routines

Revision 1.64  2003/06/06 21:38:55  cheshire
Renamed 'NewData' as 'FreshData' (The data may not be new data, just a refresh of data that we
already had in our cache. This refreshes our TTL on the data, but the data itself stays the same.)

Revision 1.63  2003/06/06 17:20:14  cheshire
For clarity, rename question fields name/rrtype/rrclass as qname/qtype/qclass
(Global search-and-replace; no functional change to code execution.)

Revision 1.62  2003/06/04 01:25:33  cheshire
<rdar://problem/3274950> Cannot perform multi-packet known-answer suppression messages
Display time interval between first and subsequent queries

Revision 1.61  2003/06/03 05:02:16  cheshire
<rdar://problem/3277080> Duplicate registrations not handled as efficiently as they should be

Revision 1.60  2003/05/31 00:09:49  cheshire
<rdar://problem/3274862> Add ability to discover what services are on a network

Revision 1.59  2003/05/29 06:11:35  cheshire
<rdar://problem/3272214>:	Report if there appear to be too many "Resolve" callbacks

Revision 1.58  2003/05/29 05:48:06  cheshire
Minor fix for when generating printf warnings: mDNS_snprintf arguments are now 3,4

Revision 1.57  2003/05/26 03:21:27  cheshire
Tidy up address structure naming:
mDNSIPAddr         => mDNSv4Addr (for consistency with mDNSv6Addr)
mDNSAddr.addr.ipv4 => mDNSAddr.ip.v4
mDNSAddr.addr.ipv6 => mDNSAddr.ip.v6

Revision 1.56  2003/05/26 03:01:27  cheshire
<rdar://problem/3268904> sprintf/vsprintf-style functions are unsafe; use snprintf/vsnprintf instead

Revision 1.55  2003/05/26 00:47:30  cheshire
Comment clarification

Revision 1.54  2003/05/24 16:39:48  cheshire
<rdar://problem/3268631> SendResponses also needs to handle multihoming better

Revision 1.53  2003/05/23 02:15:37  cheshire
Fixed misleading use of the term "duplicate suppression" where it should have
said "known answer suppression". (Duplicate answer suppression is something
different, and duplicate question suppression is yet another thing, so the use
of the completely vague term "duplicate suppression" was particularly bad.)

Revision 1.52  2003/05/22 02:29:22  cheshire
<rdar://problem/2984918> SendQueries needs to handle multihoming better
Complete rewrite of SendQueries. Works much better now :-)

Revision 1.51  2003/05/21 20:14:55  cheshire
Fix comments and warnings

Revision 1.50  2003/05/14 07:08:36  cheshire
<rdar://problem/3159272> mDNSResponder should be smarter about reconfigurations
Previously, when there was any network configuration change, mDNSResponder
would tear down the entire list of active interfaces and start again.
That was very disruptive, and caused the entire cache to be flushed,
and caused lots of extra network traffic. Now it only removes interfaces
that have really gone, and only adds new ones that weren't there before.

Revision 1.49  2003/05/07 01:49:36  cheshire
Remove "const" in ConstructServiceName prototype

Revision 1.48  2003/05/07 00:18:44  cheshire
Fix typo: "kDNSQClass_Mask" should be "kDNSClass_Mask"

Revision 1.47  2003/05/06 00:00:46  cheshire
<rdar://problem/3248914> Rationalize naming of domainname manipulation functions

Revision 1.46  2003/04/30 20:39:09  cheshire
Add comment

Revision 1.45  2003/04/29 00:40:50  cheshire
Fix compiler warnings

Revision 1.44  2003/04/26 02:41:56  cheshire
<rdar://problem/3241281> Change timenow from a local variable to a structure member

Revision 1.43  2003/04/25 01:45:56  cheshire
<rdar://problem/3240002> mDNS_RegisterNoSuchService needs to include a host name

Revision 1.42  2003/04/15 20:58:31  jgraessl

<rdar://problem/3229014> Added a hash to lookup records in the cache.

Revision 1.41  2003/04/15 18:09:13  jgraessl

<rdar://problem/3228892>
Reviewed by: Stuart Cheshire
Added code to keep track of when the next cache item will expire so we can
call TidyRRCache only when necessary.

Revision 1.40  2003/03/29 01:55:19  cheshire
<rdar://problem/3212360> mDNSResponder sometimes suffers false self-conflicts when it sees its own packets
Solution: Major cleanup of packet timing and conflict handling rules

Revision 1.39  2003/03/27 03:30:55  cheshire
<rdar://problem/3210018> Name conflicts not handled properly, resulting in memory corruption, and eventual crash
Problem was that HostNameCallback() was calling mDNS_DeregisterInterface(), which is not safe in a callback
Fixes:
1. Make mDNS_DeregisterInterface() safe to call from a callback
2. Make HostNameCallback() use mDNS_DeadvertiseInterface() instead
   (it never really needed to deregister the interface at all)

Revision 1.38  2003/03/15 04:40:36  cheshire
Change type called "mDNSOpaqueID" to the more descriptive name "mDNSInterfaceID"

Revision 1.37  2003/03/14 21:34:11  cheshire
<rdar://problem/3176950> Can't setup and print to Lexmark PS printers via Airport Extreme
Increase size of cache rdata from 512 to 768

Revision 1.36  2003/03/05 03:38:35  cheshire
<rdar://problem/3185731> Bogus error message in console: died or deallocated, but no record of client can be found!
Fixed by leaving client in list after conflict, until client explicitly deallocates

Revision 1.35  2003/02/21 02:47:54  cheshire
<rdar://problem/3099194> mDNSResponder needs performance improvements
Several places in the code were calling CacheRRActive(), which searched the entire
question list every time, to see if this cache resource record answers any question.
Instead, we now have a field "CRActiveQuestion" in the resource record structure

Revision 1.34  2003/02/21 01:54:08  cheshire
<rdar://problem/3099194> mDNSResponder needs performance improvements
Switched to using new "mDNS_Execute" model (see "Implementer Notes.txt")

Revision 1.33  2003/02/20 06:48:32  cheshire
<rdar://problem/3169535> Xserve RAID needs to do interface-specific registrations
Reviewed by: Josh Graessley, Bob Bradley

Revision 1.32  2003/01/31 03:35:59  cheshire
<rdar://problem/3147097> mDNSResponder sometimes fails to find the correct results
When there were *two* active questions in the list, they were incorrectly
finding *each other* and *both* being marked as duplicates of another question

Revision 1.31  2003/01/29 02:46:37  cheshire
Fix for IPv6:
A physical interface is identified solely by its InterfaceID (not by IP and type).
On a given InterfaceID, mDNSCore may send both v4 and v6 multicasts.
In cases where the requested outbound protocol (v4 or v6) is not supported on
that InterfaceID, the platform support layer should simply discard that packet.

Revision 1.30  2003/01/29 01:47:08  cheshire
Rename 'Active' to 'CRActive' or 'InterfaceActive' for improved clarity

Revision 1.29  2003/01/28 05:23:43  cheshire
<rdar://problem/3147097> mDNSResponder sometimes fails to find the correct results
Add 'Active' flag for interfaces

Revision 1.28  2003/01/28 01:35:56  cheshire
Revise comment about ThisQInterval to reflect new semantics

Revision 1.27  2003/01/13 23:49:42  jgraessl
Merged changes for the following fixes in to top of tree:
<rdar://problem/3086540>  computer name changes not handled properly
<rdar://problem/3124348>  service name changes are not properly handled
<rdar://problem/3124352>  announcements sent in pairs, failing chattiness test

Revision 1.26  2002/12/23 22:13:28  jgraessl

Reviewed by: Stuart Cheshire
Initial IPv6 support for mDNSResponder.

Revision 1.25  2002/09/21 20:44:49  zarzycki
Added APSL info

Revision 1.24  2002/09/19 23:47:35  cheshire
Added mDNS_RegisterNoSuchService() function for assertion of non-existence
of a particular named service

Revision 1.23  2002/09/19 21:25:34  cheshire
mDNS_snprintf() doesn't need to be in a separate file

Revision 1.22  2002/09/19 04:20:43  cheshire
Remove high-ascii characters that confuse some systems

Revision 1.21  2002/09/17 01:06:35  cheshire
Change mDNS_AdvertiseLocalAddresses to be a parameter to mDNS_Init()

Revision 1.20  2002/09/16 18:41:41  cheshire
Merge in license terms from Quinn's copy, in preparation for Darwin release

*/

#ifndef __mDNSClientAPI_h
#define __mDNSClientAPI_h

#if defined(EFI32) || defined(EFI64)
// EFI doesn't have stdarg.h
#include "Tiano.h"
#define va_list			VA_LIST
#define va_start(a, b)	VA_START(a, b)
#define va_end(a)		VA_END(a)
#define va_arg(a, b)	VA_ARG(a, b)
#else
#include <stdarg.h>		// stdarg.h is required for for va_list support for the mDNS_vsnprintf declaration
#endif

#include "mDNSDebug.h"

#ifdef __cplusplus
	extern "C" {
#endif

// ***************************************************************************
// Function scope indicators

// If you see "mDNSlocal" before a function name in a C file, it means the function is not callable outside this file
#ifndef mDNSlocal
#define mDNSlocal static
#endif
// If you see "mDNSexport" before a symbol in a C file, it means the symbol is exported for use by clients
// For every "mDNSexport" in a C file, there needs to be a corresponding "extern" declaration in some header file
// (When a C file #includes a header file, the "extern" declarations tell the compiler:
// "This symbol exists -- but not necessarily in this C file.")
#ifndef mDNSexport
#define mDNSexport
#endif

// Explanation: These local/export markers are a little habit of mine for signaling the programmers' intentions.
// When "mDNSlocal" is just a synonym for "static", and "mDNSexport" is a complete no-op, you could be
// forgiven for asking what purpose they serve. The idea is that if you see "mDNSexport" in front of a
// function definition it means the programmer intended it to be exported and callable from other files
// in the project. If you see "mDNSlocal" in front of a function definition it means the programmer
// intended it to be private to that file. If you see neither in front of a function definition it
// means the programmer forgot (so you should work out which it is supposed to be, and fix it).
// Using "mDNSlocal" instead of "static" makes it easier to do a textual searches for one or the other.
// For example you can do a search for "static" to find if any functions declare any local variables as "static"
// (generally a bad idea unless it's also "const", because static storage usually risks being non-thread-safe)
// without the results being cluttered with hundreds of matches for functions declared static.
// - Stuart Cheshire

// ***************************************************************************
// Structure packing macro

// If we're not using GNUC, it's not fatal.
// Most compilers naturally pack the on-the-wire structures correctly anyway, so a plain "struct" is usually fine.
// In the event that structures are not packed correctly, mDNS_Init() will detect this and report an error, so the
// developer will know what's wrong, and can investigate what needs to be done on that compiler to provide proper packing.
#ifndef packedstruct
 #if ((__GNUC__ > 2) || ((__GNUC__ == 2) && (__GNUC_MINOR__ >= 9)))
  #define packedstruct struct __attribute__((__packed__))
  #define packedunion  union  __attribute__((__packed__))
 #else
  #define packedstruct struct
  #define packedunion  union
 #endif
#endif

// ***************************************************************************
#if 0
#pragma mark - DNS Resource Record class and type constants
#endif

typedef enum							// From RFC 1035
	{
	kDNSClass_IN               = 1,		// Internet
	kDNSClass_CS               = 2,		// CSNET
	kDNSClass_CH               = 3,		// CHAOS
	kDNSClass_HS               = 4,		// Hesiod
	kDNSClass_NONE             = 254,	// Used in DNS UPDATE [RFC 2136]

	kDNSClass_Mask             = 0x7FFF,// Multicast DNS uses the bottom 15 bits to identify the record class...
	kDNSClass_UniqueRRSet      = 0x8000,// ... and the top bit indicates that all other cached records are now invalid

	kDNSQClass_ANY             = 255,	// Not a DNS class, but a DNS query class, meaning "all classes"
	kDNSQClass_UnicastResponse = 0x8000	// Top bit set in a question means "unicast response acceptable"
	} DNS_ClassValues;

typedef enum				// From RFC 1035
	{
	kDNSType_A = 1,			//  1 Address
	kDNSType_NS,			//  2 Name Server
	kDNSType_MD,			//  3 Mail Destination
	kDNSType_MF,			//  4 Mail Forwarder
	kDNSType_CNAME,			//  5 Canonical Name
	kDNSType_SOA,			//  6 Start of Authority
	kDNSType_MB,			//  7 Mailbox
	kDNSType_MG,			//  8 Mail Group
	kDNSType_MR,			//  9 Mail Rename
	kDNSType_NULL,			// 10 NULL RR
	kDNSType_WKS,			// 11 Well-known-service
	kDNSType_PTR,			// 12 Domain name pointer
	kDNSType_HINFO,			// 13 Host information
	kDNSType_MINFO,			// 14 Mailbox information
	kDNSType_MX,			// 15 Mail Exchanger
	kDNSType_TXT,			// 16 Arbitrary text string

	kDNSType_AAAA = 28,		// 28 IPv6 address
	kDNSType_SRV  = 33,		// 33 Service record
	kDNSType_OPT  = 41,     // EDNS0 OPT record
	kDNSType_TSIG = 250,    // 250 Transaction Signature

	kDNSQType_ANY = 255		// Not a DNS type, but a DNS query type, meaning "all types"
	} DNS_TypeValues;

// ***************************************************************************
#if 0
#pragma mark - Simple types
#endif

// mDNS defines its own names for these common types to simplify portability across
// multiple platforms that may each have their own (different) names for these types.
typedef          int   mDNSBool;
typedef   signed char  mDNSs8;
typedef unsigned char  mDNSu8;
typedef   signed short mDNSs16;
typedef unsigned short mDNSu16;

// <http://gcc.gnu.org/onlinedocs/gcc-3.3.3/cpp/Common-Predefined-Macros.html> says
//   __LP64__ _LP64
//   These macros are defined, with value 1, if (and only if) the compilation is
//   for a target where long int and pointer both use 64-bits and int uses 32-bit.
// <http://www.intel.com/software/products/compilers/clin/docs/ug/lin1077.htm> says
//   Macro Name __LP64__ Value 1
// A quick Google search for "defined(__LP64__)" OR "#ifdef __LP64__" gives 2590 hits and
// a search for "#if __LP64__" gives only 12, so I think we'll go with the majority and use defined()
#if defined(_ILP64) || defined(__ILP64__)
typedef   signed int32 mDNSs32;
typedef unsigned int32 mDNSu32;
#elif defined(_LP64) || defined(__LP64__)
typedef   signed int   mDNSs32;
typedef unsigned int   mDNSu32;
#else
typedef   signed long  mDNSs32;
typedef unsigned long  mDNSu32;
//typedef   signed int mDNSs32;
//typedef unsigned int mDNSu32;
#endif

// To enforce useful type checking, we make mDNSInterfaceID be a pointer to a dummy struct
// This way, mDNSInterfaceIDs can be assigned, and compared with each other, but not with other types
// Declaring the type to be the typical generic "void *" would lack this type checking
typedef struct mDNSInterfaceID_dummystruct { void *dummy; } *mDNSInterfaceID;

// These types are for opaque two- and four-byte identifiers.
// The "NotAnInteger" fields of the unions allow the value to be conveniently passed around in a
// register for the sake of efficiency, and compared for equality or inequality, but don't forget --
// just because it is in a register doesn't mean it is an integer. Operations like greater than,
// less than, add, multiply, increment, decrement, etc., are undefined for opaque identifiers,
// and if you make the mistake of trying to do those using the NotAnInteger field, then you'll
// find you get code that doesn't work consistently on big-endian and little-endian machines.
typedef packedunion { mDNSu8 b[ 2]; mDNSu16 NotAnInteger; } mDNSOpaque16;
typedef packedunion { mDNSu8 b[ 4]; mDNSu32 NotAnInteger; } mDNSOpaque32;
typedef packedunion { mDNSu8 b[ 6]; mDNSu16 w[3]; mDNSu32 l[1]; } mDNSOpaque48;
typedef packedunion { mDNSu8 b[16]; mDNSu16 w[8]; mDNSu32 l[4]; } mDNSOpaque128;

typedef mDNSOpaque16  mDNSIPPort;		// An IP port is a two-byte opaque identifier (not an integer)
typedef mDNSOpaque32  mDNSv4Addr;		// An IP address is a four-byte opaque identifier (not an integer)
typedef mDNSOpaque128 mDNSv6Addr;		// An IPv6 address is a 16-byte opaque identifier (not an integer)
typedef mDNSOpaque48  mDNSEthAddr;		// An Ethernet address is a six-byte opaque identifier (not an integer)

enum
	{
	mDNSAddrType_None    = 0,
	mDNSAddrType_IPv4    = 4,
	mDNSAddrType_IPv6    = 6,
	mDNSAddrType_Unknown = ~0	// Special marker value used in known answer list recording
	};

typedef struct
	{
	mDNSs32 type;
	union { mDNSv6Addr v6; mDNSv4Addr v4; } ip;
	} mDNSAddr;

enum { mDNSfalse = 0, mDNStrue = 1 };

#define mDNSNULL 0L

enum
	{
	mStatus_Waiting           = 1,
	mStatus_NoError           = 0,

	// mDNS return values are in the range FFFE FF00 (-65792) to FFFE FFFF (-65537)
	// The top end of the range (FFFE FFFF) is used for error codes;
	// the bottom end of the range (FFFE FF00) is used for non-error values;

	// Error codes:
	mStatus_UnknownErr        = -65537,		// First value: 0xFFFE FFFF
	mStatus_NoSuchNameErr     = -65538,
	mStatus_NoMemoryErr       = -65539,
	mStatus_BadParamErr       = -65540,
	mStatus_BadReferenceErr   = -65541,
	mStatus_BadStateErr       = -65542,
	mStatus_BadFlagsErr       = -65543,
	mStatus_UnsupportedErr    = -65544,
	mStatus_NotInitializedErr = -65545,
	mStatus_NoCache           = -65546,
	mStatus_AlreadyRegistered = -65547,
	mStatus_NameConflict      = -65548,
	mStatus_Invalid           = -65549,
	mStatus_Firewall          = -65550,
	mStatus_Incompatible      = -65551,
	mStatus_BadInterfaceErr   = -65552,
	mStatus_Refused           = -65553,
	mStatus_NoSuchRecord      = -65554,
	mStatus_NoAuth            = -65555,
	mStatus_NoSuchKey         = -65556,
	mStatus_NATTraversal      = -65557,
	mStatus_DoubleNAT         = -65558,
	mStatus_BadTime           = -65559,
	mStatus_BadSig            = -65560,     // while we define this per RFC 2845, BIND 9 returns Refused for bad/missing signatures
	mStatus_BadKey            = -65561,
	mStatus_TransientErr      = -65562,     // transient failures, e.g. sending packets shortly after a network transition or wake from sleep
	// -65563 to -65786 currently unused; available for allocation

	// tcp connection status
	mStatus_ConnPending       = -65787,
	mStatus_ConnFailed        = -65788,
	mStatus_ConnEstablished   = -65789,

	// Non-error values:
	mStatus_GrowCache         = -65790,
	mStatus_ConfigChanged     = -65791,
	mStatus_MemFree           = -65792		// Last value: 0xFFFE FF00
	
	// mStatus_MemFree is the last legal mDNS error code, at the end of the range allocated for mDNS
	};

typedef mDNSs32 mStatus;

// RFC 1034/1035 specify that a domain label consists of a length byte plus up to 63 characters
#define MAX_DOMAIN_LABEL 63
typedef struct { mDNSu8 c[ 64]; } domainlabel;		// One label: length byte and up to 63 characters

// RFC 1034/1035 specify that a domain name, including length bytes, data bytes, and terminating zero, may be up to 255 bytes long
#define MAX_DOMAIN_NAME 255
typedef struct { mDNSu8 c[256]; } domainname;		// Up to 255 bytes of length-prefixed domainlabels

typedef struct { mDNSu8 c[256]; } UTF8str255;		// Null-terminated C string

// The longest legal textual form of a DNS name is 1005 bytes, including the C-string terminating NULL at the end.
// Explanation:
// When a native domainname object is converted to printable textual form using ConvertDomainNameToCString(),
// non-printing characters are represented in the conventional DNS way, as '\ddd', where ddd is a three-digit decimal number.
// The longest legal domain name is 255 bytes, in the form of four labels as shown below:
// Length byte, 63 data bytes, length byte, 63 data bytes, length byte, 63 data bytes, length byte, 61 data bytes, zero byte.
// Each label is encoded textually as characters followed by a trailing dot.
// If every character has to be represented as a four-byte escape sequence, then this makes the maximum textual form four labels
// plus the C-string terminating NULL as shown below:
// 63*4+1 + 63*4+1 + 63*4+1 + 61*4+1 + 1 = 1005.
// Note that MAX_ESCAPED_DOMAIN_LABEL is not normally used: If you're only decoding a single label, escaping is usually not required.
// It is for domain names, where dots are used as label separators, that proper escaping is vital.
#define MAX_ESCAPED_DOMAIN_LABEL 254
#define MAX_ESCAPED_DOMAIN_NAME 1005

// Most records have a TTL of 75 minutes, so that their 80% cache-renewal query occurs once per hour.
// For records containing a hostname (in the name on the left, or in the rdata on the right),
// like A, AAAA, reverse-mapping PTR, and SRV, we use a two-minute TTL by default, because we don't want
// them to hang around for too long in the cache if the host in question crashes or otherwise goes away.
// Wide-area service discovery records have a very short TTL to avoid poluting intermediate caches with
// dynamic records.  When discovered via Long Lived Queries (with change notifications), resource record
// TTLs can be safely ignored.
	
#define kStandardTTL (3600UL * 100 / 80)
#define kHostNameTTL 120UL
#define kWideAreaTTL 3

#define DefaultTTLforRRType(X) (((X) == kDNSType_A || (X) == kDNSType_AAAA || (X) == kDNSType_SRV) ? kHostNameTTL : kStandardTTL)

// ***************************************************************************
#if 0
#pragma mark - DNS Message structures
#endif

#define mDNS_numZones   numQuestions
#define mDNS_numPrereqs numAnswers
#define mDNS_numUpdates numAuthorities

typedef packedstruct
	{
	mDNSOpaque16 id;
	mDNSOpaque16 flags;
	mDNSu16 numQuestions;
	mDNSu16 numAnswers;
	mDNSu16 numAuthorities;
	mDNSu16 numAdditionals;
	} DNSMessageHeader;

// We can send and receive packets up to 9000 bytes (Ethernet Jumbo Frame size, if that ever becomes widely used)
// However, in the normal case we try to limit packets to 1500 bytes so that we don't get IP fragmentation on standard Ethernet
// 40 (IPv6 header) + 8 (UDP header) + 12 (DNS message header) + 1440 (DNS message body) = 1500 total
#define AbsoluteMaxDNSMessageData 8940
#define NormalMaxDNSMessageData 1440
typedef packedstruct
	{
	DNSMessageHeader h;						// Note: Size 12 bytes
	mDNSu8 data[AbsoluteMaxDNSMessageData];	// 40 (IPv6) + 8 (UDP) + 12 (DNS header) + 8940 (data) = 9000
	} DNSMessage;

// ***************************************************************************
#if 0
#pragma mark - Resource Record structures
#endif

// Authoritative Resource Records:
// There are four basic types: Shared, Advisory, Unique, Known Unique

// * Shared Resource Records do not have to be unique
// -- Shared Resource Records are used for DNS-SD service PTRs
// -- It is okay for several hosts to have RRs with the same name but different RDATA
// -- We use a random delay on responses to reduce collisions when all the hosts respond to the same query
// -- These RRs typically have moderately high TTLs (e.g. one hour)
// -- These records are announced on startup and topology changes for the benefit of passive listeners
// -- These records send a goodbye packet when deregistering
//
// * Advisory Resource Records are like Shared Resource Records, except they don't send a goodbye packet
//
// * Unique Resource Records should be unique among hosts within any given mDNS scope
// -- The majority of Resource Records are of this type
// -- If two entities on the network have RRs with the same name but different RDATA, this is a conflict
// -- Responses may be sent immediately, because only one host should be responding to any particular query
// -- These RRs typically have low TTLs (e.g. a few minutes)
// -- On startup and after topology changes, a host issues queries to verify uniqueness

// * Known Unique Resource Records are treated like Unique Resource Records, except that mDNS does
// not have to verify their uniqueness because this is already known by other means (e.g. the RR name
// is derived from the host's IP or Ethernet address, which is already known to be a unique identifier).

// Summary of properties of different record types:
// Probe?    Does this record type send probes before announcing?
// Conflict? Does this record type react if we observe an apparent conflict?
// Goodbye?  Does this record type send a goodbye packet on departure?
//
//               Probe? Conflict? Goodbye? Notes
// Unregistered                            Should not appear in any list (sanity check value)
// Shared         No      No       Yes     e.g. Service PTR record
// Deregistering  No      No       Yes     Shared record about to announce its departure and leave the list
// Advisory       No      No       No
// Unique         Yes     Yes      No      Record intended to be unique -- will probe to verify
// Verified       Yes     Yes      No      Record has completed probing, and is verified unique
// KnownUnique    No      Yes      No      Record is assumed by other means to be unique

// Valid lifecycle of a record:
// Unregistered ->                   Shared      -> Deregistering -(goodbye)-> Unregistered
// Unregistered ->                   Advisory                               -> Unregistered
// Unregistered -> Unique -(probe)-> Verified                               -> Unregistered
// Unregistered ->                   KnownUnique                            -> Unregistered

// Each Authoritative kDNSRecordType has only one bit set. This makes it easy to quickly see if a record
// is one of a particular set of types simply by performing the appropriate bitwise masking operation.

// Cache Resource Records (received from the network):
// There are four basic types: Answer, Unique Answer, Additional, Unique Additional
// Bit 7 (the top bit) of kDNSRecordType is always set for Cache Resource Records; always clear for Authoritative Resource Records
// Bit 6 (value 0x40) is set for answer records; clear for additional records
// Bit 5 (value 0x20) is set for records received with the kDNSClass_UniqueRRSet

enum
	{
	kDNSRecordTypeUnregistered     = 0x00,	// Not currently in any list
	kDNSRecordTypeDeregistering    = 0x01,	// Shared record about to announce its departure and leave the list

	kDNSRecordTypeUnique           = 0x02,	// Will become a kDNSRecordTypeVerified when probing is complete

	kDNSRecordTypeAdvisory         = 0x04,	// Like Shared, but no goodbye packet
	kDNSRecordTypeShared           = 0x08,	// Shared means record name does not have to be unique -- use random delay on responses

	kDNSRecordTypeVerified         = 0x10,	// Unique means mDNS should check that name is unique (and then send immediate responses)
	kDNSRecordTypeKnownUnique      = 0x20,	// Known Unique means mDNS can assume name is unique without checking
	                                        // For Dynamic Update records, Known Unique means the record must already exist on the server.
	kDNSRecordTypeUniqueMask       = (kDNSRecordTypeUnique | kDNSRecordTypeVerified | kDNSRecordTypeKnownUnique),
	kDNSRecordTypeActiveMask       = (kDNSRecordTypeAdvisory | kDNSRecordTypeShared | kDNSRecordTypeVerified | kDNSRecordTypeKnownUnique),

	kDNSRecordTypePacketAdd        = 0x80,	// Received in the Additional  Section of a DNS Response
	kDNSRecordTypePacketAddUnique  = 0x90,	// Received in the Additional  Section of a DNS Response with kDNSClass_UniqueRRSet set
	kDNSRecordTypePacketAuth       = 0xA0,	// Received in the Authorities Section of a DNS Response
	kDNSRecordTypePacketAuthUnique = 0xB0,	// Received in the Authorities Section of a DNS Response with kDNSClass_UniqueRRSet set
	kDNSRecordTypePacketAns        = 0xC0,	// Received in the Answer      Section of a DNS Response
	kDNSRecordTypePacketAnsUnique  = 0xD0,	// Received in the Answer      Section of a DNS Response with kDNSClass_UniqueRRSet set

	kDNSRecordTypePacketAnsMask    = 0x40,	// True for PacketAns and    PacketAnsUnique
	kDNSRecordTypePacketUniqueMask = 0x10	// True for PacketAddUnique, PacketAnsUnique, PacketAuthUnique
	};

typedef packedstruct { mDNSu16 priority; mDNSu16 weight; mDNSIPPort port; domainname target;   } rdataSRV;
typedef packedstruct { mDNSu16 preference;                                domainname exchange; } rdataMX;
typedef packedstruct
	{
	domainname mname;
	domainname rname;
	mDNSs32 serial;		// Modular counter; increases when zone changes
	mDNSu32 refresh;	// Time in seconds that a slave waits after successful replication of the database before it attempts replication again
	mDNSu32 retry;		// Time in seconds that a slave waits after an unsuccessful replication attempt before it attempts replication again
	mDNSu32 expire;		// Time in seconds that a slave holds on to old data while replication attempts remain unsuccessful
	mDNSu32 min;		// Nominally the minimum record TTL for this zone, in seconds; also used for negative caching.
	} rdataSOA;

typedef packedstruct
	{
	mDNSu16 vers;
	mDNSu16 llqOp;
	mDNSu16 err;
	mDNSu8 id[8];
	mDNSu32 lease;
	} LLQOptData;

#define LLQ_OPTLEN ((3 * sizeof(mDNSu16)) + 8 + sizeof(mDNSu32))
// Windows adds pad bytes to sizeof(LLQOptData).  Use this macro when setting length fields or validating option rdata from
// off the wire.  Use sizeof(LLQOptData) when dealing with structures (e.g. memcpy).  Never memcpy between on-the-wire
// representation and a structure
	
// NOTE: rdataOpt format may be repeated an arbitrary number of times in a single resource record
typedef packedstruct
	{
	mDNSu16 opt;
	mDNSu16 optlen;
	union { LLQOptData llq; mDNSu32 lease; } OptData;
	} rdataOpt;

// StandardAuthRDSize is 264 (256+8), which is large enough to hold a maximum-sized SRV record
// MaximumRDSize is 8K the absolute maximum we support (at least for now)
#define StandardAuthRDSize 264
#define MaximumRDSize 8192

// InlineCacheRDSize is 68
// Records received from the network with rdata this size or less have their rdata stored right in the CacheRecord object
// Records received from the network with rdata larger than this have additional storage allocated for the rdata
// A quick unscientific sample from a busy network at Apple with lots of machines revealed this:
// 1461 records in cache
// 292 were one-byte TXT records
// 136 were four-byte A records
// 184 were sixteen-byte AAAA records
// 780 were various PTR, TXT and SRV records from 12-64 bytes
// Only 69 records had rdata bigger than 64 bytes
// Note that since CacheRecord object and a CacheGroup object are allocated out of the same pool, it's sensible to
// have them both be the same size. Making one smaller without making the other smaller won't actually save any memory.
#define InlineCacheRDSize 68

#define InlineCacheGroupNameSize 144

typedef union
	{
	mDNSu8      data[StandardAuthRDSize];
	mDNSv4Addr  ipv4;		// For 'A' record
	mDNSv6Addr  ipv6;		// For 'AAAA' record
	domainname  name;		// For PTR, NS, and CNAME records
	UTF8str255  txt;		// For TXT record
	rdataSRV    srv;		// For SRV record
	rdataMX     mx;			// For MX record
	rdataSOA    soa;        // For SOA record
	rdataOpt    opt;        // For eDNS0 opt record
	} RDataBody;

typedef struct
	{
	mDNSu16    MaxRDLength;	// Amount of storage allocated for rdata (usually sizeof(RDataBody))
	RDataBody  u;
	} RData;
#define sizeofRDataHeader (sizeof(RData) - sizeof(RDataBody))

typedef struct AuthRecord_struct AuthRecord;
typedef struct CacheRecord_struct CacheRecord;
typedef struct CacheGroup_struct CacheGroup;
typedef struct DNSQuestion_struct DNSQuestion;
typedef struct mDNS_struct mDNS;
typedef struct mDNS_PlatformSupport_struct mDNS_PlatformSupport;
typedef struct NATTraversalInfo_struct NATTraversalInfo;

// Note: Within an mDNSRecordCallback mDNS all API calls are legal except mDNS_Init(), mDNS_Close(), mDNS_Execute()
typedef void mDNSRecordCallback(mDNS *const m, AuthRecord *const rr, mStatus result);

// Note:
// Restrictions: An mDNSRecordUpdateCallback may not make any mDNS API calls.
// The intent of this callback is to allow the client to free memory, if necessary.
// The internal data structures of the mDNS code may not be in a state where mDNS API calls may be made safely.
typedef void mDNSRecordUpdateCallback(mDNS *const m, AuthRecord *const rr, RData *OldRData);

typedef struct
	{
	mDNSu8          RecordType;			// See enum above
	mDNSInterfaceID InterfaceID;		// Set if this RR is specific to one interface
										// For records received off the wire, InterfaceID is *always* set to the receiving interface
										// For our authoritative records, InterfaceID is usually zero, except for those few records
										// that are interface-specific (e.g. address records, especially linklocal addresses)
	domainname     *name;
	mDNSu16         rrtype;
	mDNSu16         rrclass;
	mDNSu32         rroriginalttl;		// In seconds
	mDNSu16         rdlength;			// Size of the raw rdata, in bytes
	mDNSu16         rdestimate;			// Upper bound on size of rdata after name compression
	mDNSu32         namehash;			// Name-based (i.e. case-insensitive) hash of name
	mDNSu32         rdatahash;			// For rdata containing domain name (e.g. PTR, SRV, CNAME etc.), case-insensitive name hash
										// else, for all other rdata, 32-bit hash of the raw rdata
										// Note: This requirement is important. Various routines like AddAdditionalsToResponseList(),
										// ReconfirmAntecedents(), etc., use rdatahash as a pre-flight check to see
										// whether it's worth doing a full SameDomainName() call. If the rdatahash
										// is not a correct case-insensitive name hash, they'll get false negatives.
	RData           *rdata;				// Pointer to storage for this rdata
	} ResourceRecord;

// Unless otherwise noted, states may apply to either independent record registrations or service registrations	
typedef enum
	{
	regState_FetchingZoneData  = 1,     // getting info - update not sent
	regState_Pending           = 2,     // update sent, reply not received
	regState_Registered        = 3,     // update sent, reply received
	regState_DeregPending      = 4,     // dereg sent, reply not received
	regState_DeregDeferred     = 5,     // dereg requested while in Pending state - send dereg AFTER registration is confirmed
	regState_Cancelled         = 6,     // update not sent, reg. cancelled by client
	regState_Unregistered      = 8,     // not in any list
	regState_Refresh           = 9,     // outstanding refresh (or target change) message
	regState_NATMap            = 10,    // establishing NAT port mapping or learning public address
	regState_UpdatePending     = 11,    // update in flight as result of mDNS_Update call
	regState_NoTarget          = 12,    // service registration pending registration of hostname (ServiceRegistrations only)
    regState_ExtraQueued       = 13,    // extra record to be registered upon completion of service registration (RecordRegistrations only)
	regState_NATError          = 14     // unable to complete NAT traversal
	} regState_t; 

// context for both ServiceRecordSet and individual AuthRec structs
typedef struct
	{
	// registration/lease state
	regState_t   state;
	mDNSBool     lease;    // dynamic update contains (should contain) lease option
	mDNSs32      expire;   // expiration of lease (-1 for static)
	mDNSBool     TestForSelfConflict;  // on name conflict, check if we're just seeing our own orphaned records
	
	// identifier to match update request and response
	mDNSOpaque16 id;
	
	// server info
	domainname   zone;     // the zone that is updated
	mDNSAddr     ns;       // primary name server for the record's zone    !!!KRS not technically correct to cache longer than TTL
	mDNSIPPort   port;     // port on which server accepts dynamic updates
	
	// NAT traversal context
	NATTraversalInfo *NATinfo; // may be NULL

	// state for deferred operations
    mDNSBool     ClientCallbackDeferred;  // invoke client callback on completion of pending operation(s)
	mStatus      DeferredStatus;          // status to deliver when above flag is set
    mDNSBool     SRVUpdateDeferred;       // do we need to change target or port once current operation completes?
    mDNSBool     SRVChanged;              // temporarily deregistered service because its SRV target or port changed

    // uDNS_UpdateRecord support fields
    RData *OrigRData;      mDNSu16 OrigRDLen;     // previously registered, being deleted
    RData *InFlightRData;  mDNSu16 InFlightRDLen; // currently being registered
    RData *QueuedRData;    mDNSu16 QueuedRDLen;   // if the client call Update while an update is in flight, we must finish the
                                                  // pending operation (re-transmitting if necessary) THEN register the queued update
	mDNSRecordUpdateCallback *UpdateRDCallback;   // client callback to free old rdata
	} uDNS_RegInfo;

struct AuthRecord_struct
	{
	// For examples of how to set up this structure for use in mDNS_Register(),
	// see mDNS_AdvertiseInterface() or mDNS_RegisterService().
	// Basically, resrec and persistent metadata need to be set up before calling mDNS_Register().
	// mDNS_SetupResourceRecord() is avaliable as a helper routine to set up most fields to sensible default values for you

	AuthRecord     *next;				// Next in list; first element of structure for efficiency reasons
	// Field Group 1: Common ResourceRecord fields
	ResourceRecord  resrec;
	uDNS_RegInfo uDNS_info;

	// Field Group 2: Persistent metadata for Authoritative Records
	AuthRecord     *Additional1;		// Recommended additional record to include in response
	AuthRecord     *Additional2;		// Another additional
	AuthRecord     *DependentOn;		// This record depends on another for its uniqueness checking
	AuthRecord     *RRSet;				// This unique record is part of an RRSet
	mDNSRecordCallback *RecordCallback;	// Callback function to call for state changes, and to free memory asynchronously on deregistration
	void           *RecordContext;		// Context parameter for the callback function
	mDNSu8          HostTarget;			// Set if the target of this record (PTR, CNAME, SRV, etc.) is our host name
	mDNSu8          AllowRemoteQuery;	// Set if we allow hosts not on the local link to query this record
	mDNSu8          ForceMCast;			// Set by client to advertise solely via multicast, even for apparently unicast names

	// Field Group 3: Transient state for Authoritative Records
	mDNSu8          Acknowledged;		// Set if we've given the success callback to the client
	mDNSu8          ProbeCount;			// Number of probes remaining before this record is valid (kDNSRecordTypeUnique)
	mDNSu8          AnnounceCount;		// Number of announcements remaining (kDNSRecordTypeShared)
	mDNSu8          RequireGoodbye;		// Set if this RR has been announced on the wire and will require a goodbye packet
	mDNSu8          LocalAnswer;		// Set if this RR has been delivered to LocalOnly questions
	mDNSu8          IncludeInProbe;		// Set if this RR is being put into a probe right now
	mDNSInterfaceID ImmedAnswer;		// Someone on this interface issued a query we need to answer (all-ones for all interfaces)
	mDNSu8          ImmedUnicast;		// Set if we may send our response directly via unicast to the requester
#if MDNS_LOG_ANSWER_SUPPRESSION_TIMES
	mDNSs32         ImmedAnswerMarkTime;
#endif
	mDNSInterfaceID ImmedAdditional;	// Hint that we might want to also send this record, just to be helpful
	mDNSInterfaceID SendRNow;			// The interface this query is being sent on right now
	mDNSv4Addr      v4Requester;		// Recent v4 query for this record, or all-ones if more than one recent query
	mDNSv6Addr      v6Requester;		// Recent v6 query for this record, or all-ones if more than one recent query
	AuthRecord     *NextResponse;		// Link to the next element in the chain of responses to generate
	const mDNSu8   *NR_AnswerTo;		// Set if this record was selected by virtue of being a direct answer to a question
	AuthRecord     *NR_AdditionalTo;	// Set if this record was selected by virtue of being additional to another
	mDNSs32         ThisAPInterval;		// In platform time units: Current interval for announce/probe
	mDNSs32         AnnounceUntil;		// In platform time units: Creation time + TTL
	mDNSs32         LastAPTime;			// In platform time units: Last time we sent announcement/probe
	mDNSs32         LastMCTime;			// Last time we multicast this record (used to guard against packet-storm attacks)
	mDNSInterfaceID LastMCInterface;	// Interface this record was multicast on at the time LastMCTime was recorded
	RData          *NewRData;			// Set if we are updating this record with new rdata
	mDNSu16         newrdlength;		// ... and the length of the new RData
	mDNSRecordUpdateCallback *UpdateCallback;
	mDNSu32         UpdateCredits;		// Token-bucket rate limiting of excessive updates
	mDNSs32         NextUpdateCredit;	// Time next token is added to bucket
	mDNSs32         UpdateBlocked;		// Set if update delaying is in effect

	domainname      namestorage;
	RData           rdatastorage;		// Normally the storage is right here, except for oversized records
	// rdatastorage MUST be the last thing in the structure -- when using oversized AuthRecords, extra bytes
	// are appended after the end of the AuthRecord, logically augmenting the size of the rdatastorage
	// DO NOT ADD ANY MORE FIELDS HERE
	};

// Wrapper struct for Auth Records for higher-level code that cannot use the AuthRecord's ->next pointer field
typedef struct ARListElem
	{
	struct ARListElem *next;
	AuthRecord ar;          // Note: Must be last struct in field to accomodate oversized AuthRecords
	} ARListElem;

struct CacheGroup_struct				// Header object for a list of CacheRecords with the same name
	{
	CacheGroup     *next;				// Next CacheGroup object in this hash table bucket
	mDNSu32         namehash;			// Name-based (i.e. case insensitive) hash of name
	CacheRecord    *members;			// List of CacheRecords with this same name
	CacheRecord   **rrcache_tail;		// Tail end of that list
	domainname     *name;				// Common name for all CacheRecords in this list
	mDNSu8          namestorage[InlineCacheGroupNameSize];
	};

struct CacheRecord_struct
	{
	CacheRecord    *next;				// Next in list; first element of structure for efficiency reasons
	ResourceRecord  resrec;

	// Transient state for Cache Records
	CacheRecord    *NextInKAList;		// Link to the next element in the chain of known answers to send
	mDNSs32         TimeRcvd;			// In platform time units
	mDNSs32         DelayDelivery;		// Set if we want to defer delivery of this answer to local clients
	mDNSs32         NextRequiredQuery;	// In platform time units
	mDNSs32         LastUsed;			// In platform time units
	DNSQuestion    *CRActiveQuestion;	// Points to an active question referencing this answer
	mDNSu32         UnansweredQueries;	// Number of times we've issued a query for this record without getting an answer
	mDNSs32         LastUnansweredTime;	// In platform time units; last time we incremented UnansweredQueries
	mDNSu32         MPUnansweredQ;		// Multi-packet query handling: Number of times we've seen a query for this record
	mDNSs32         MPLastUnansweredQT;	// Multi-packet query handling: Last time we incremented MPUnansweredQ
	mDNSu32         MPUnansweredKA;		// Multi-packet query handling: Number of times we've seen this record in a KA list
	mDNSBool        MPExpectingKA;		// Multi-packet query handling: Set when we increment MPUnansweredQ; allows one KA
	CacheRecord    *NextInCFList;		// Set if this is in the list of records we just received with the cache flush bit set

	struct { mDNSu16 MaxRDLength; mDNSu8 data[InlineCacheRDSize]; } rdatastorage;	// Storage for small records is right here
	};

// Storage sufficient to hold either a CacheGroup header or a CacheRecord
typedef union CacheEntity_union CacheEntity;
union CacheEntity_union { CacheEntity *next; CacheGroup cg; CacheRecord cr; };

typedef struct
	{
	CacheRecord r;
	mDNSu8 _extradata[MaximumRDSize-InlineCacheRDSize];		// Glue on the necessary number of extra bytes
	domainname namestorage;									// Needs to go *after* the extra rdata bytes
	} LargeCacheRecord;

typedef struct uDNS_HostnameInfo
	{
	struct uDNS_HostnameInfo *next;
    domainname fqdn;
    AuthRecord *arv4;                         // registered IPv4 address record
	AuthRecord *arv6;                         // registered IPv6 address record
	mDNSRecordCallback *StatusCallback;       // callback to deliver success or error code to client layer
	const void *StatusContext;                // Client Context
	} uDNS_HostnameInfo;

enum
   	{
   	DNSServer_Untested = 0,
   	DNSServer_Failed   = 1,
   	DNSServer_Passed   = 2
   	};

typedef struct DNSServer
	{
    struct DNSServer *next;
    mDNSAddr   addr;
    mDNSBool   del;			// Set when we're planning to delete this from the list
    mDNSu32    teststate;	// Have we sent bug-detection query to this server?
    domainname domain;		// name->server matching for "split dns"
	} DNSServer;

typedef struct NetworkInterfaceInfo_struct NetworkInterfaceInfo;

// A NetworkInterfaceInfo_struct serves two purposes:
// 1. It holds the address, PTR and HINFO records to advertise a given IP address on a given physical interface
// 2. It tells mDNSCore which physical interfaces are available; each physical interface has its own unique InterfaceID.
//    Since there may be multiple IP addresses on a single physical interface,
//    there may be multiple NetworkInterfaceInfo_structs with the same InterfaceID.
//    In this case, to avoid sending the same packet n times, when there's more than one
//    struct with the same InterfaceID, mDNSCore picks one member of the set to be the
//    active representative of the set; all others have the 'InterfaceActive' flag unset.

struct NetworkInterfaceInfo_struct
	{
	// Internal state fields. These are used internally by mDNSCore; the client layer needn't be concerned with them.
	NetworkInterfaceInfo *next;

	mDNSBool        InterfaceActive;	// Set if interface is sending & receiving packets (see comment above)
	mDNSBool        IPv4Available;		// If InterfaceActive, set if v4 available on this InterfaceID
	mDNSBool        IPv6Available;		// If InterfaceActive, set if v6 available on this InterfaceID

	// Standard AuthRecords that every Responder host should have (one per active IP address)
	AuthRecord RR_A;					// 'A' or 'AAAA' (address) record for our ".local" name
	AuthRecord RR_PTR;					// PTR (reverse lookup) record
	AuthRecord RR_HINFO;

	// Client API fields: The client must set up these fields *before* calling mDNS_RegisterInterface()
	mDNSInterfaceID InterfaceID;		// Identifies physical interface; MUST NOT be 0, -1, or -2
	mDNSAddr        ip;					// The IPv4 or IPv6 address to advertise
	mDNSAddr        mask;
	char            ifname[64];			// Windows uses a GUID string for the interface name, which doesn't fit in 16 bytes
	mDNSBool        Advertise;			// False if you are only searching on this interface
	mDNSBool        McastTxRx;			// Send/Receive multicast on this { InterfaceID, address family } ?
	};

typedef struct ExtraResourceRecord_struct ExtraResourceRecord;
struct ExtraResourceRecord_struct
	{
	ExtraResourceRecord *next;
    mDNSu32 ClientID;  // Opaque ID field to be used by client to map an AddRecord call to a set of Extra records
	AuthRecord r;
	// Note: Add any additional fields *before* the AuthRecord in this structure, not at the end.
	// In some cases clients can allocate larger chunks of memory and set r->rdata->MaxRDLength to indicate
	// that this extra memory is available, which would result in any fields after the AuthRecord getting smashed
	};

// Note: Within an mDNSServiceCallback mDNS all API calls are legal except mDNS_Init(), mDNS_Close(), mDNS_Execute()
typedef struct ServiceRecordSet_struct ServiceRecordSet;
typedef void mDNSServiceCallback(mDNS *const m, ServiceRecordSet *const sr, mStatus result);
struct ServiceRecordSet_struct
	{
	// Internal state fields. These are used internally by mDNSCore; the client layer needn't be concerned with them.
	// No fields need to be set up by the client prior to calling mDNS_RegisterService();
	// all required data is passed as parameters to that function.
	ServiceRecordSet    *next;
	uDNS_RegInfo        uDNS_info;
	mDNSServiceCallback *ServiceCallback;
	void                *ServiceContext;
	ExtraResourceRecord *Extras;	// Optional list of extra AuthRecords attached to this service registration
	mDNSu32              NumSubTypes;
	AuthRecord          *SubTypes;
	mDNSBool             Conflict;	// Set if this record set was forcibly deregistered because of a conflict
	domainname           Host;		// Set if this service record does not use the standard target host name
	AuthRecord           RR_ADV;	// e.g. _services._dns-sd._udp.local. PTR _printer._tcp.local.
	AuthRecord           RR_PTR;	// e.g. _printer._tcp.local.        PTR Name._printer._tcp.local.
	AuthRecord           RR_SRV;	// e.g. Name._printer._tcp.local.   SRV 0 0 port target
	AuthRecord           RR_TXT;	// e.g. Name._printer._tcp.local.   TXT PrintQueueName
	// Don't add any fields after AuthRecord RR_TXT.
	// This is where the implicit extra space goes if we allocate a ServiceRecordSet containing an oversized RR_TXT record
	};

// ***************************************************************************
#if 0
#pragma mark - Question structures
#endif

// We record the last eight instances of each duplicate query
// This gives us v4/v6 on each of Ethernet/AirPort and Firewire, and two free slots "for future expansion"
// If the host has more active interfaces that this it is not fatal -- duplicate question suppression will degrade gracefully.
// Since we will still remember the last eight, the busiest interfaces will still get the effective duplicate question suppression.
#define DupSuppressInfoSize 8

typedef struct
	{
	mDNSs32               Time;
	mDNSInterfaceID       InterfaceID;
	mDNSs32               Type;				// v4 or v6?
	} DupSuppressInfo;

typedef enum
	{
	// Setup states
	LLQ_UnInit            = 0,
	LLQ_GetZoneInfo       = 1,
	LLQ_InitialRequest    = 2,
	LLQ_SecondaryRequest  = 3,
	LLQ_Refresh           = 4,
	LLQ_Retry             = 5,
	LLQ_Established       = 6,	
	LLQ_Suspended         = 7,   
	LLQ_SuspendDeferred   = 8, // suspend once we get zone info
	LLQ_SuspendedPoll     = 9, // suspended from polling state
	LLQ_NatMapWait        = 10,
	
	// Established/error states
	LLQ_Static            = 16,
	LLQ_Poll              = 17,
	LLQ_Error             = 18,
	LLQ_Cancelled         = 19
	} LLQ_State;

typedef struct
	{
	LLQ_State state;
	mDNSAddr servAddr;
	mDNSIPPort servPort;
	DNSQuestion *question;
	mDNSu32 origLease;  // seconds (relative)
	mDNSs32 retry;  // ticks (absolute)
	mDNSs32 expire; // ticks (absolute)
    mDNSs16 ntries;
	mDNSu8 id[8];
	mDNSBool deriveRemovesOnResume;
    mDNSBool NATMap;        // does this LLQ use the global LLQ NAT mapping?
	} LLQ_Info;

// LLQ constants
#define kDNSOpt_LLQ	   1
#define kDNSOpt_Lease  2
#define kLLQ_Vers      1
#define kLLQ_DefLease  7200 // 2 hours
#define kLLQ_MAX_TRIES 3    // retry an operation 3 times max
#define kLLQ_INIT_RESEND 2 // resend an un-ack'd packet after 2 seconds, then double for each additional
#define kLLQ_DEF_RETRY 1800 // retry a failed operation after 30 minutes
// LLQ Operation Codes
#define kLLQOp_Setup     1
#define kLLQOp_Refresh   2
#define kLLQOp_Event     3

#define LLQ_OPT_RDLEN ((2 * sizeof(mDNSu16)) + LLQ_OPTLEN)
#define LEASE_OPT_RDLEN (2 * sizeof(mDNSu16)) + sizeof(mDNSs32)

// LLQ Errror Codes
enum
	{
	LLQErr_NoError = 0,
	LLQErr_ServFull = 1,
	LLQErr_Static = 2,
	LLQErr_FormErr = 3,
	LLQErr_NoSuchLLQ = 4,
	LLQErr_BadVers = 5,
	LLQErr_UnknownErr = 6
	};

typedef void (*InternalResponseHndlr)(mDNS *const m, DNSMessage *msg, const  mDNSu8 *end, DNSQuestion *question, void *internalContext);
typedef struct
	{
	mDNSOpaque16          id;
	mDNSBool              internal;
	InternalResponseHndlr responseCallback;   // NULL if internal field is false
	LLQ_Info              *llq;               // NULL for 1-shot queries
    mDNSBool              Answered;           // have we received an answer (including NXDOMAIN) for this question?
    CacheRecord           *knownAnswers;
    mDNSs32               RestartTime;        // Mark when we restart a suspended query
    void *context;
	} uDNS_QuestionInfo;

// Note: Within an mDNSQuestionCallback mDNS all API calls are legal except mDNS_Init(), mDNS_Close(), mDNS_Execute()
typedef void mDNSQuestionCallback(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, mDNSBool AddRecord);
struct DNSQuestion_struct
	{
	// Internal state fields. These are used internally by mDNSCore; the client layer needn't be concerned with them.
	DNSQuestion          *next;
	mDNSu32               qnamehash;
	mDNSs32               DelayAnswering;	// Set if we want to defer answering this question until the cache settles
	mDNSs32               LastQTime;		// Last scheduled transmission of this Q on *all* applicable interfaces
	mDNSs32               ThisQInterval;	// LastQTime + ThisQInterval is the next scheduled transmission of this Q
											// ThisQInterval > 0 for an active question;
											// ThisQInterval = 0 for a suspended question that's still in the list
											// ThisQInterval = -1 for a cancelled question that's been removed from the list
	mDNSs32               LastAnswerPktNum;	// The sequence number of the last response packet containing an answer to this Q
	mDNSu32               RecentAnswerPkts;	// Number of answers since the last time we sent this query
	mDNSu32               CurrentAnswers;	// Number of records currently in the cache that answer this question
	mDNSu32               LargeAnswers;		// Number of answers with rdata > 1024 bytes
	mDNSu32               UniqueAnswers;	// Number of answers received with kDNSClass_UniqueRRSet bit set
	mDNSInterfaceID       FlappingInterface;// Set when an interface goes away, to flag if removes are delivered for this Q
	DNSQuestion          *DuplicateOf;
	DNSQuestion          *NextInDQList;
	DupSuppressInfo       DupSuppress[DupSuppressInfoSize];
	mDNSInterfaceID       SendQNow;			// The interface this query is being sent on right now
	mDNSBool              SendOnAll;		// Set if we're sending this question on all active interfaces
	mDNSu32               RequestUnicast;	// Non-zero if we want to send query with kDNSQClass_UnicastResponse bit set
	mDNSs32               LastQTxTime;		// Last time this Q was sent on one (but not necessarily all) interfaces
	uDNS_QuestionInfo     uDNS_info;

	// Client API fields: The client must set up these fields *before* calling mDNS_StartQuery()
	mDNSInterfaceID       InterfaceID;		// Non-zero if you want to issue queries only on a single specific IP interface
	mDNSAddr              Target;			// Non-zero if you want to direct queries to a specific unicast target address
	mDNSIPPort            TargetPort;		// Must be set if Target is set
	mDNSOpaque16          TargetQID;		// Must be set if Target is set
	domainname            qname;
	mDNSu16               qtype;
	mDNSu16               qclass;
	mDNSBool              LongLived;        // Set by client for calls to mDNS_StartQuery to indicate LLQs to unicast layer.
	mDNSBool              ExpectUnique;		// Set by client if it's expecting unique RR(s) for this question, not shared RRs
	mDNSBool              ForceMCast;		// Set by client to force mDNS query, even for apparently uDNS names
	mDNSBool              ReturnCNAME;		// Set by client to request callbacks for intermediate CNAME records
	mDNSQuestionCallback *QuestionCallback;
	void                 *QuestionContext;
	};

typedef struct
	{
	// Client API fields: The client must set up name and InterfaceID *before* calling mDNS_StartResolveService()
	// When the callback is invoked, ip, port, TXTlen and TXTinfo will have been filled in with the results learned from the network.
	domainname      name;
	mDNSInterfaceID InterfaceID;		// ID of the interface the response was received on
	mDNSAddr        ip;					// Remote (destination) IP address where this service can be accessed
	mDNSIPPort      port;				// Port where this service can be accessed
	mDNSu16         TXTlen;
	mDNSu8          TXTinfo[2048];		// Additional demultiplexing information (e.g. LPR queue name)
	} ServiceInfo;

// Note: Within an mDNSServiceInfoQueryCallback mDNS all API calls are legal except mDNS_Init(), mDNS_Close(), mDNS_Execute()
typedef struct ServiceInfoQuery_struct ServiceInfoQuery;
typedef void mDNSServiceInfoQueryCallback(mDNS *const m, ServiceInfoQuery *query);
struct ServiceInfoQuery_struct
	{
	// Internal state fields. These are used internally by mDNSCore; the client layer needn't be concerned with them.
	// No fields need to be set up by the client prior to calling mDNS_StartResolveService();
	// all required data is passed as parameters to that function.
	// The ServiceInfoQuery structure memory is working storage for mDNSCore to discover the requested information
	// and place it in the ServiceInfo structure. After the client has called mDNS_StopResolveService(), it may
	// dispose of the ServiceInfoQuery structure while retaining the results in the ServiceInfo structure.
	DNSQuestion                   qSRV;
	DNSQuestion                   qTXT;
	DNSQuestion                   qAv4;
	DNSQuestion                   qAv6;
	mDNSu8                        GotSRV;
	mDNSu8                        GotTXT;
	mDNSu8                        GotADD;
	mDNSu32                       Answers;
	ServiceInfo                  *info;
	mDNSServiceInfoQueryCallback *ServiceInfoQueryCallback;
	void                         *ServiceInfoQueryContext;
	};

// ***************************************************************************
#if 0
#pragma mark - NAT Traversal structures and constants
#endif

#define NATMAP_INIT_RETRY (mDNSPlatformOneSecond / 4)          // start at 250ms w/ exponential decay
#define NATMAP_MAX_RETRY mDNSPlatformOneSecond                 // back off to once per second
#define NATMAP_MAX_TRIES 3                                     // for max 3 tries
#define NATMAP_DEFAULT_LEASE (60 * 60)  // lease life in seconds
#define NATMAP_VERS 0
#define NATMAP_RESPONSE_MASK 0x80

typedef enum
	{
	NATOp_AddrRequest = 0,
	NATOp_MapUDP      = 1,
	NATOp_MapTCP      = 2
	} NATOp_t;

enum
   	{
   	NATErr_None = 0,
   	NATErr_Vers = 1,
   	NATErr_Refused = 2,
   	NATErr_NetFail = 3,
   	NATErr_Res = 4,
   	NATErr_Opcode = 5
   	};

typedef mDNSu16 NATErr_t;

typedef enum
	{
	NATState_Init         = 0,
	NATState_Request      = 1,
	NATState_Established  = 2,
	NATState_Legacy       = 3,
	NATState_Error        = 4,
	NATState_Refresh      = 5,
	NATState_Deleted      = 6
	} NATState_t;
// Note: we have no explicit "cancelled" state, where a service/interface is deregistered while we
 // have an outstanding NAT request.  This is conveyed by the "reg" pointer being set to NULL

typedef packedstruct
	{
	mDNSu8 vers;
	mDNSu8 opcode;
	} NATAddrRequest;	
	
typedef packedstruct
	{
	mDNSu8 vers;
	mDNSu8 opcode;
	mDNSOpaque16 err;
	mDNSOpaque32 uptime;
	mDNSv4Addr   PubAddr;
	} NATAddrReply;

typedef packedstruct
	{
	mDNSu8 vers;
	mDNSu8 opcode;
	mDNSOpaque16 unused;
	mDNSIPPort priv;
	mDNSIPPort pub;
	mDNSOpaque32 lease;
	} NATPortMapRequest;
	
typedef packedstruct
	{
	mDNSu8 vers;
	mDNSu8 opcode;
	mDNSOpaque16 err;
	mDNSOpaque32 uptime;
	mDNSIPPort priv;
	mDNSIPPort pub;
	mDNSOpaque32 lease;
	} NATPortMapReply;
	
// Pass NULL for pkt on error (including timeout)
typedef mDNSBool (*NATResponseHndlr)(NATTraversalInfo *n, mDNS *m, mDNSu8 *pkt, mDNSu16 len);

struct NATTraversalInfo_struct
	{
	NATOp_t op;
	NATResponseHndlr ReceiveResponse;
	union { AuthRecord *RecordRegistration; ServiceRecordSet *ServiceRegistration; } reg;
    mDNSAddr Router;
    mDNSIPPort PublicPort;
    union { NATAddrRequest AddrReq; NATPortMapRequest PortReq; } request;
	mDNSs32 retry;                   // absolute time when we retry
	mDNSs32 RetryInterval;           // delta between time sent and retry
	int ntries;
	NATState_t state;
	NATTraversalInfo *next;
	};

// ***************************************************************************
#if 0
#pragma mark - Main mDNS object, used to hold all the mDNS state
#endif

typedef void mDNSCallback(mDNS *const m, mStatus result);

#define CACHE_HASH_SLOTS 499

enum
	{
	mDNS_KnownBug_PhantomInterfaces = 1
	};

typedef struct
	{
	mDNSs32          nextevent;
	DNSQuestion      *ActiveQueries;     //!!!KRS this should be a hashtable (hash on messageID)
	DNSQuestion      *CurrentQuery;      // pointer to ActiveQueries list being examined in a loop.  Functions that remove
										 // elements from the ActiveQueries list must update this pointer (if non-NULL) as necessary.
										 //!!!KRS do the same for registration lists
	ServiceRecordSet *ServiceRegistrations;
	AuthRecord       *RecordRegistrations;
	NATTraversalInfo *NATTraversals;
	mDNSu16          NextMessageID;
    DNSServer        *Servers;           // list of DNS servers
	mDNSAddr         Router;
	mDNSAddr         AdvertisedV4;       // IPv4 address pointed to by hostname
	mDNSAddr         MappedV4;           // Cache of public address if PrimaryIP is behind a NAT
	mDNSAddr         AdvertisedV6;       // IPv6 address pointed to by hostname
    NATTraversalInfo *LLQNatInfo;        // Nat port mapping to receive LLQ events
	domainname       ServiceRegDomain;   // (going away w/ multi-user support)
	struct uDNS_AuthInfo *AuthInfoList;  // list of domains requiring authentication for updates.
	uDNS_HostnameInfo *Hostnames;        // List of registered hostnames + hostname metadata
    DNSQuestion      ReverseMap;         // Reverse-map query to find static hostname for service target
    mDNSBool         ReverseMapActive;   // Is above query active?
    domainname       StaticHostname;     // Current answer to reverse-map query (above)
    mDNSBool         DelaySRVUpdate;     // Delay SRV target/port update to avoid "flap"
    mDNSs32          NextSRVUpdate;      // Time to perform delayed update
	} uDNS_GlobalInfo;

struct mDNS_struct
	{
	// Internal state fields. These hold the main internal state of mDNSCore;
	// the client layer needn't be concerned with them.
	// No fields need to be set up by the client prior to calling mDNS_Init();
	// all required data is passed as parameters to that function.

	mDNS_PlatformSupport *p;			// Pointer to platform-specific data of indeterminite size
	mDNSu32  KnownBugs;
	mDNSBool CanReceiveUnicastOn5353;
	mDNSBool AdvertiseLocalAddresses;
	mStatus mDNSPlatformStatus;
	mDNSIPPort UnicastPort4;
	mDNSIPPort UnicastPort6;
	mDNSCallback *MainCallback;
	void         *MainContext;

	// For debugging: To catch and report locking failures
	mDNSu32 mDNS_busy;					// Incremented between mDNS_Lock/mDNS_Unlock section
	mDNSu32 mDNS_reentrancy;			// Incremented when calling a client callback
	mDNSu8  mDNS_shutdown;				// Set when we're shutting down, allows us to skip some unnecessary steps
	mDNSu8  lock_rrcache;				// For debugging: Set at times when these lists may not be modified
	mDNSu8  lock_Questions;
	mDNSu8  lock_Records;
	#define MaxMsg 120
	char MsgBuffer[MaxMsg];				// Temp storage used while building error log messages

	// Task Scheduling variables
	mDNSs32  timenow_adjust;			// Correction applied if we ever discover time went backwards
	mDNSs32  timenow;					// The time that this particular activation of the mDNS code started
	mDNSs32  timenow_last;				// The time the last time we ran
	mDNSs32  NextScheduledEvent;		// Derived from values below
	mDNSs32  SuppressSending;			// Don't send *any* packets during this time
	mDNSs32  NextCacheCheck;			// Next time to refresh cache record before it expires
	mDNSs32  NextScheduledQuery;		// Next time to send query in its exponential backoff sequence
	mDNSs32  NextScheduledProbe;		// Next time to probe for new authoritative record
	mDNSs32  NextScheduledResponse;		// Next time to send authoritative record(s) in responses
	mDNSs32  ExpectUnicastResponse;		// Set when we send a query with the kDNSQClass_UnicastResponse bit set
	mDNSs32  RandomQueryDelay;			// For de-synchronization of query packets on the wire
	mDNSu32  RandomReconfirmDelay;		// For de-synchronization of reconfirmation queries on the wire
	mDNSs32  PktNum;					// Unique sequence number assigned to each received packet
	mDNSBool SendDeregistrations;		// Set if we need to send deregistrations (immediately)
	mDNSBool SendImmediateAnswers;		// Set if we need to send answers (immediately -- or as soon as SuppressSending clears)
	mDNSBool SleepState;				// Set if we're sleeping (send no more packets)

	// These fields only required for mDNS Searcher...
	DNSQuestion *Questions;				// List of all registered questions, active and inactive
	DNSQuestion *NewQuestions;			// Fresh questions not yet answered from cache
	DNSQuestion *CurrentQuestion;		// Next question about to be examined in AnswerLocalQuestions()
	DNSQuestion *LocalOnlyQuestions;	// Questions with InterfaceID set to mDNSInterface_LocalOnly
	DNSQuestion *NewLocalOnlyQuestions;	// Fresh local-only questions not yet answered
	mDNSu32 rrcache_size;				// Total number of available cache entries
	mDNSu32 rrcache_totalused;			// Number of cache entries currently occupied
	mDNSu32 rrcache_active;				// Number of cache entries currently occupied by records that answer active questions
	mDNSu32 rrcache_report;
	CacheEntity *rrcache_free;
	CacheGroup *rrcache_hash[CACHE_HASH_SLOTS];

	// Fields below only required for mDNS Responder...
	domainlabel nicelabel;				// Rich text label encoded using canonically precomposed UTF-8
	domainlabel hostlabel;				// Conforms to RFC 1034 "letter-digit-hyphen" ARPANET host name rules
	domainname  MulticastHostname;		// Fully Qualified "dot-local" Host Name, e.g. "Foo.local."
	UTF8str255  HIHardware;
	UTF8str255  HISoftware;
	AuthRecord *ResourceRecords;
	AuthRecord *DuplicateRecords;		// Records currently 'on hold' because they are duplicates of existing records
	AuthRecord *NewLocalRecords;		// Fresh local-only records not yet delivered to local-only questions
	AuthRecord *CurrentRecord;			// Next AuthRecord about to be examined
	NetworkInterfaceInfo *HostInterfaces;
	mDNSs32 ProbeFailTime;
	mDNSu32 NumFailedProbes;
	mDNSs32 SuppressProbes;

	// unicast-specific data
	uDNS_GlobalInfo uDNS_info;
	mDNSs32 SuppressStdPort53Queries;	// Wait before allowing the next standard unicast query to the user's configured DNS server

	// Fixed storage, to avoid creating large objects on the stack
	DNSMessage imsg;		// Incoming message received from wire
	DNSMessage omsg;		// Outgoing message we're building
	LargeCacheRecord rec;	// Resource Record extracted from received message
	};

#define FORALL_CACHERECORDS(SLOT,CG,CR)                          \
	for ((SLOT) = 0; (SLOT) < CACHE_HASH_SLOTS; (SLOT)++)        \
		for((CG)=m->rrcache_hash[(SLOT)]; (CG); (CG)=(CG)->next) \
			for ((CR) = (CG)->members; (CR); (CR)=(CR)->next)

// ***************************************************************************
#if 0
#pragma mark - Useful Static Constants
#endif

extern const mDNSIPPort      zeroIPPort;
extern const mDNSv4Addr      zerov4Addr;
extern const mDNSv6Addr      zerov6Addr;
extern const mDNSEthAddr     zeroEthAddr;
extern const mDNSv4Addr      onesIPv4Addr;
extern const mDNSv6Addr      onesIPv6Addr;
extern const mDNSAddr        zeroAddr;

extern const mDNSInterfaceID mDNSInterface_Any;				// Zero
extern const mDNSInterfaceID mDNSInterface_LocalOnly;		// Special value

extern const mDNSIPPort      UnicastDNSPort;
extern const mDNSIPPort      NATPMPPort;
extern const mDNSIPPort      DNSEXTPort;
extern const mDNSIPPort      MulticastDNSPort;
extern const mDNSIPPort      LoopbackIPCPort;

extern const mDNSv4Addr      AllDNSAdminGroup;
#define AllDNSLinkGroupv4 (AllDNSLinkGroup_v4.ip.v4)
#define AllDNSLinkGroupv6 (AllDNSLinkGroup_v6.ip.v6)
extern const mDNSAddr        AllDNSLinkGroup_v4;
extern const mDNSAddr        AllDNSLinkGroup_v6;

extern const mDNSOpaque16 zeroID;
extern const mDNSOpaque16 QueryFlags;
extern const mDNSOpaque16 uQueryFlags;
extern const mDNSOpaque16 ResponseFlags;
extern const mDNSOpaque16 UpdateReqFlags;
extern const mDNSOpaque16 UpdateRespFlags;

#define localdomain (*(const domainname *)"\x5" "local")
#define LocalReverseMapDomain (*(const domainname *)"\x3" "254" "\x3" "169" "\x7" "in-addr" "\x4" "arpa")
	
// ***************************************************************************
#if 0
#pragma mark - Inline functions
#endif

#if (defined(_MSC_VER))
	#define mDNSinline static __inline
#elif ((__GNUC__ > 2) || ((__GNUC__ == 2) && (__GNUC_MINOR__ >= 9)))
	#define mDNSinline static inline
#endif

// If we're not doing inline functions, then this header needs to have the extern declarations
#if !defined(mDNSinline)
extern mDNSs32      NonZeroTime(mDNSs32 t);
extern mDNSu16      mDNSVal16(mDNSOpaque16 x);
extern mDNSu32      mDNSVal32(mDNSOpaque32 x);
extern mDNSOpaque16 mDNSOpaque16fromIntVal(mDNSu16 v);
extern mDNSOpaque32 mDNSOpaque32fromIntVal(mDNSu32 v);
#endif

// If we're compiling the particular C file that instantiates our inlines, then we
// define "mDNSinline" (to empty string) so that we generate code in the following section
#if (!defined(mDNSinline) && mDNS_InstantiateInlines)
#define mDNSinline
#endif

#ifdef mDNSinline

mDNSinline mDNSs32 NonZeroTime(mDNSs32 t) { if (t) return(t); else return(1); }

mDNSinline mDNSu16 mDNSVal16(mDNSOpaque16 x) { return((mDNSu16)((mDNSu16)x.b[0] <<  8 | (mDNSu16)x.b[1])); }
mDNSinline mDNSu32 mDNSVal32(mDNSOpaque32 x) { return((mDNSu32)((mDNSu32)x.b[0] << 24 | (mDNSu32)x.b[1] << 16 | (mDNSu32)x.b[2] << 8 | (mDNSu32)x.b[3])); }

mDNSinline mDNSOpaque16 mDNSOpaque16fromIntVal(mDNSu16 v)
	{
	mDNSOpaque16 x;
	x.b[0] = (mDNSu8)(v >> 8);
	x.b[1] = (mDNSu8)(v & 0xFF);
	return(x);
	}

mDNSinline mDNSOpaque32 mDNSOpaque32fromIntVal(mDNSu32 v)
	{
	mDNSOpaque32 x;
	x.b[0] = (mDNSu8) (v >> 24)        ;
	x.b[1] = (mDNSu8)((v >> 16) & 0xFF);
	x.b[2] = (mDNSu8)((v >> 8 ) & 0xFF);
	x.b[3] = (mDNSu8)((v      ) & 0xFF);
	return x;
	}

#endif

// ***************************************************************************
#if 0
#pragma mark - Main Client Functions
#endif

// Every client should call mDNS_Init, passing in storage for the mDNS object and the mDNS_PlatformSupport object.
//
// Clients that are only advertising services should use mDNS_Init_NoCache and mDNS_Init_ZeroCacheSize.
// Clients that plan to perform queries (mDNS_StartQuery, mDNS_StartBrowse, mDNS_StartResolveService, etc.)
// need to provide storage for the resource record cache, or the query calls will return 'mStatus_NoCache'.
// The rrcachestorage parameter is the address of memory for the resource record cache, and
// the rrcachesize parameter is the number of entries in the CacheRecord array passed in.
// (i.e. the size of the cache memory needs to be sizeof(CacheRecord) * rrcachesize).
// OS X 10.3 Panther uses an initial cache size of 64 entries, and then mDNSCore sends an
// mStatus_GrowCache message if it needs more.
//
// Most clients should use mDNS_Init_AdvertiseLocalAddresses. This causes mDNSCore to automatically
// create the correct address records for all the hosts interfaces. If you plan to advertise
// services being offered by the local machine, this is almost always what you want.
// There are two cases where you might use mDNS_Init_DontAdvertiseLocalAddresses:
// 1. A client-only device, that browses for services but doesn't advertise any of its own.
// 2. A proxy-registration service, that advertises services being offered by other machines, and takes
//    the appropriate steps to manually create the correct address records for those other machines.
// In principle, a proxy-like registration service could manually create address records for its own machine too,
// but this would be pointless extra effort when using mDNS_Init_AdvertiseLocalAddresses does that for you.
//
// When mDNS has finished setting up the client's callback is called
// A client can also spin and poll the mDNSPlatformStatus field to see when it changes from mStatus_Waiting to mStatus_NoError
//
// Call mDNS_Close to tidy up before exiting
//
// Call mDNS_Register with a completed AuthRecord object to register a resource record
// If the resource record type is kDNSRecordTypeUnique (or kDNSknownunique) then if a conflicting resource record is discovered,
// the resource record's mDNSRecordCallback will be called with error code mStatus_NameConflict. The callback should deregister
// the record, and may then try registering the record again after picking a new name (e.g. by automatically appending a number).
// Following deregistration, the RecordCallback will be called with result mStatus_MemFree to signal that it is safe to deallocate
// the record's storage (memory must be freed asynchronously to allow for goodbye packets and dynamic update deregistration).
//
// Call mDNS_StartQuery to initiate a query. mDNS will proceed to issue Multicast DNS query packets, and any time a response
// is received containing a record which matches the question, the DNSQuestion's mDNSAnswerCallback function will be called
// Call mDNS_StopQuery when no more answers are required
//
// Care should be taken on multi-threaded or interrupt-driven environments.
// The main mDNS routines call mDNSPlatformLock() on entry and mDNSPlatformUnlock() on exit;
// each platform layer needs to implement these appropriately for its respective platform.
// For example, if the support code on a particular platform implements timer callbacks at interrupt time, then
// mDNSPlatformLock/Unlock need to disable interrupts or do similar concurrency control to ensure that the mDNS
// code is not entered by an interrupt-time timer callback while in the middle of processing a client call.

extern mStatus mDNS_Init      (mDNS *const m, mDNS_PlatformSupport *const p,
								CacheEntity *rrcachestorage, mDNSu32 rrcachesize,
								mDNSBool AdvertiseLocalAddresses,
								mDNSCallback *Callback, void *Context);
// See notes above on use of NoCache/ZeroCacheSize
#define mDNS_Init_NoCache                     mDNSNULL
#define mDNS_Init_ZeroCacheSize               0
// See notes above on use of Advertise/DontAdvertiseLocalAddresses
#define mDNS_Init_AdvertiseLocalAddresses     mDNStrue
#define mDNS_Init_DontAdvertiseLocalAddresses mDNSfalse
#define mDNS_Init_NoInitCallback              mDNSNULL
#define mDNS_Init_NoInitCallbackContext       mDNSNULL

extern void    mDNS_GrowCache (mDNS *const m, CacheEntity *storage, mDNSu32 numrecords);
extern void    mDNS_Close     (mDNS *const m);
extern mDNSs32 mDNS_Execute   (mDNS *const m);

extern mStatus mDNS_Register  (mDNS *const m, AuthRecord *const rr);
extern mStatus mDNS_Update    (mDNS *const m, AuthRecord *const rr, mDNSu32 newttl,
								const mDNSu16 newrdlength, RData *const newrdata, mDNSRecordUpdateCallback *Callback);
extern mStatus mDNS_Deregister(mDNS *const m, AuthRecord *const rr);

extern mStatus mDNS_StartQuery(mDNS *const m, DNSQuestion *const question);
extern mStatus mDNS_StopQuery (mDNS *const m, DNSQuestion *const question);
extern mStatus mDNS_Reconfirm (mDNS *const m, CacheRecord *const cacherr);
extern mStatus mDNS_ReconfirmByValue(mDNS *const m, ResourceRecord *const rr);
extern mDNSs32 mDNS_TimeNow(const mDNS *const m);

// ***************************************************************************
#if 0
#pragma mark - Platform support functions that are accessible to the client layer too
#endif

extern mDNSs32  mDNSPlatformOneSecond;

// ***************************************************************************
#if 0
#pragma mark - General utility and helper functions
#endif

// mDNS_RegisterService is a single call to register the set of resource records associated with a given named service.
//
// mDNS_StartResolveService is single call which is equivalent to multiple calls to mDNS_StartQuery,
// to find the IP address, port number, and demultiplexing information for a given named service.
// As with mDNS_StartQuery, it executes asynchronously, and calls the ServiceInfoQueryCallback when the answer is
// found. After the service is resolved, the client should call mDNS_StopResolveService to complete the transaction.
// The client can also call mDNS_StopResolveService at any time to abort the transaction.
//
// mDNS_AddRecordToService adds an additional record to a Service Record Set.  This record may be deregistered
// via mDNS_RemoveRecordFromService, or by deregistering the service.  mDNS_RemoveRecordFromService is passed a
// callback to free the memory associated with the extra RR when it is safe to do so.  The ExtraResourceRecord
// object can be found in the record's context pointer.
	
// mDNS_GetBrowseDomains is a special case of the mDNS_StartQuery call, where the resulting answers
// are a list of PTR records indicating (in the rdata) domains that are recommended for browsing.
// After getting the list of domains to browse, call mDNS_StopQuery to end the search.
// mDNS_GetDefaultBrowseDomain returns the name of the domain that should be highlighted by default.
//
// mDNS_GetRegistrationDomains and mDNS_GetDefaultRegistrationDomain are the equivalent calls to get the list
// of one or more domains that should be offered to the user as choices for where they may register their service,
// and the default domain in which to register in the case where the user has made no selection.

extern void    mDNS_SetupResourceRecord(AuthRecord *rr, RData *RDataStorage, mDNSInterfaceID InterfaceID,
               mDNSu16 rrtype, mDNSu32 ttl, mDNSu8 RecordType, mDNSRecordCallback Callback, void *Context);

extern mStatus mDNS_RegisterService  (mDNS *const m, ServiceRecordSet *sr,
               const domainlabel *const name, const domainname *const type, const domainname *const domain,
               const domainname *const host, mDNSIPPort port, const mDNSu8 txtinfo[], mDNSu16 txtlen,
               AuthRecord *SubTypes, mDNSu32 NumSubTypes,
               const mDNSInterfaceID InterfaceID, mDNSServiceCallback Callback, void *Context);
extern mStatus mDNS_AddRecordToService(mDNS *const m, ServiceRecordSet *sr, ExtraResourceRecord *extra, RData *rdata, mDNSu32 ttl);
extern mStatus mDNS_RemoveRecordFromService(mDNS *const m, ServiceRecordSet *sr, ExtraResourceRecord *extra, mDNSRecordCallback MemFreeCallback, void *Context);
extern mStatus mDNS_RenameAndReregisterService(mDNS *const m, ServiceRecordSet *const sr, const domainlabel *newname);
extern mStatus mDNS_DeregisterService(mDNS *const m, ServiceRecordSet *sr);

extern mStatus mDNS_RegisterNoSuchService(mDNS *const m, AuthRecord *const rr,
               const domainlabel *const name, const domainname *const type, const domainname *const domain,
               const domainname *const host,
               const mDNSInterfaceID InterfaceID, mDNSRecordCallback Callback, void *Context);
#define        mDNS_DeregisterNoSuchService mDNS_Deregister

extern mStatus mDNS_StartBrowse(mDNS *const m, DNSQuestion *const question,
               const domainname *const srv, const domainname *const domain,
               const mDNSInterfaceID InterfaceID, mDNSBool ForceMCast, mDNSQuestionCallback *Callback, void *Context);
#define        mDNS_StopBrowse mDNS_StopQuery

extern mStatus mDNS_StartResolveService(mDNS *const m, ServiceInfoQuery *query, ServiceInfo *info, mDNSServiceInfoQueryCallback *Callback, void *Context);
extern void    mDNS_StopResolveService (mDNS *const m, ServiceInfoQuery *query);

typedef enum
	{
	mDNS_DomainTypeBrowse              = 0,
	mDNS_DomainTypeBrowseDefault       = 1,
	mDNS_DomainTypeBrowseLegacy        = 2,
	mDNS_DomainTypeRegistration        = 3,
	mDNS_DomainTypeRegistrationDefault = 4,
	
	mDNS_DomainTypeMax = 4
	} mDNS_DomainType;

extern const char *const mDNS_DomainTypeNames[];

extern mStatus mDNS_GetDomains(mDNS *const m, DNSQuestion *const question, mDNS_DomainType DomainType, const domainname *dom,
								const mDNSInterfaceID InterfaceID, mDNSQuestionCallback *Callback, void *Context);
#define        mDNS_StopGetDomains mDNS_StopQuery
extern mStatus mDNS_AdvertiseDomains(mDNS *const m, AuthRecord *rr, mDNS_DomainType DomainType, const mDNSInterfaceID InterfaceID, char *domname);
#define        mDNS_StopAdvertiseDomains mDNS_Deregister

// ***************************************************************************
#if 0
#pragma mark - DNS name utility functions
#endif

// In order to expose the full capabilities of the DNS protocol (which allows any arbitrary eight-bit values
// in domain name labels, including unlikely characters like ascii nulls and even dots) all the mDNS APIs
// work with DNS's native length-prefixed strings. For convenience in C, the following utility functions
// are provided for converting between C's null-terminated strings and DNS's length-prefixed strings.

// Assignment
// A simple C structure assignment of a domainname can cause a protection fault by accessing unmapped memory,
// because that object is defined to be 256 bytes long, but not all domainname objects are truly the full size.
// This macro uses mDNSPlatformMemCopy() to make sure it only touches the actual bytes that are valid.
#define AssignDomainName(DST, SRC) mDNSPlatformMemCopy((SRC)->c, (DST)->c, DomainNameLength((SRC)))

// Comparison functions
extern mDNSBool SameDomainLabel(const mDNSu8 *a, const mDNSu8 *b);
extern mDNSBool SameDomainName(const domainname *const d1, const domainname *const d2);
extern mDNSBool IsLocalDomain(const domainname *d);     // returns true for domains that by default should be looked up using link-local multicast

// Get total length of domain name, in native DNS format, including terminal root label
//   (e.g. length of "com." is 5 (length byte, three data bytes, final zero)
extern mDNSu16  DomainNameLength(const domainname *const name);

// Append functions to append one or more labels to an existing native format domain name:
//   AppendLiteralLabelString adds a single label from a literal C string, with no escape character interpretation.
//   AppendDNSNameString      adds zero or more labels from a C string using conventional DNS dots-and-escaping interpretation
//   AppendDomainLabel        adds a single label from a native format domainlabel
//   AppendDomainName         adds zero or more labels from a native format domainname
extern mDNSu8  *AppendLiteralLabelString(domainname *const name, const char *cstr);
extern mDNSu8  *AppendDNSNameString     (domainname *const name, const char *cstr);
extern mDNSu8  *AppendDomainLabel       (domainname *const name, const domainlabel *const label);
extern mDNSu8  *AppendDomainName        (domainname *const name, const domainname *const append);

// Convert from null-terminated string to native DNS format:
//   The DomainLabel form makes a single label from a literal C string, with no escape character interpretation.
//   The DomainName form makes native format domain name from a C string using conventional DNS interpretation:
//     dots separate labels, and within each label, '\.' represents a literal dot, '\\' represents a literal
//     backslash and backslash with three decimal digits (e.g. \000) represents an arbitrary byte value.
extern mDNSBool MakeDomainLabelFromLiteralString(domainlabel *const label, const char *cstr);
extern mDNSu8  *MakeDomainNameFromDNSNameString (domainname  *const name,  const char *cstr);

// Convert native format domainlabel or domainname back to C string format
// IMPORTANT:
// When using ConvertDomainLabelToCString, the target buffer must be MAX_ESCAPED_DOMAIN_LABEL (254) bytes long
// to guarantee there will be no buffer overrun. It is only safe to use a buffer shorter than this in rare cases
// where the label is known to be constrained somehow (for example, if the label is known to be either "_tcp" or "_udp").
// Similarly, when using ConvertDomainNameToCString, the target buffer must be MAX_ESCAPED_DOMAIN_NAME (1005) bytes long.
// See definitions of MAX_ESCAPED_DOMAIN_LABEL and MAX_ESCAPED_DOMAIN_NAME for more detailed explanation.
extern char    *ConvertDomainLabelToCString_withescape(const domainlabel *const name, char *cstr, char esc);
#define         ConvertDomainLabelToCString_unescaped(D,C) ConvertDomainLabelToCString_withescape((D), (C), 0)
#define         ConvertDomainLabelToCString(D,C)           ConvertDomainLabelToCString_withescape((D), (C), '\\')
extern char    *ConvertDomainNameToCString_withescape(const domainname *const name, char *cstr, char esc);
#define         ConvertDomainNameToCString_unescaped(D,C) ConvertDomainNameToCString_withescape((D), (C), 0)
#define         ConvertDomainNameToCString(D,C)           ConvertDomainNameToCString_withescape((D), (C), '\\')

extern void     ConvertUTF8PstringToRFC1034HostLabel(const mDNSu8 UTF8Name[], domainlabel *const hostlabel);

extern mDNSu8  *ConstructServiceName(domainname *const fqdn, const domainlabel *name, const domainname *type, const domainname *const domain);
extern mDNSBool DeconstructServiceName(const domainname *const fqdn, domainlabel *const name, domainname *const type, domainname *const domain);

// Note: Some old functions have been replaced by more sensibly-named versions.
// You can uncomment the hash-defines below if you don't want to have to change your source code right away.
// When updating your code, note that (unlike the old versions) *all* the new routines take the target object
// as their first parameter.
//#define ConvertCStringToDomainName(SRC,DST)  MakeDomainNameFromDNSNameString((DST),(SRC))
//#define ConvertCStringToDomainLabel(SRC,DST) MakeDomainLabelFromLiteralString((DST),(SRC))
//#define AppendStringLabelToName(DST,SRC)     AppendLiteralLabelString((DST),(SRC))
//#define AppendStringNameToName(DST,SRC)      AppendDNSNameString((DST),(SRC))
//#define AppendDomainLabelToName(DST,SRC)     AppendDomainLabel((DST),(SRC))
//#define AppendDomainNameToName(DST,SRC)      AppendDomainName((DST),(SRC))

// ***************************************************************************
#if 0
#pragma mark - Other utility functions and macros
#endif

// mDNS_vsnprintf/snprintf return the number of characters written, excluding the final terminating null.
// The output is always null-terminated: for example, if the output turns out to be exactly buflen long,
// then the output will be truncated by one character to allow space for the terminating null.
// Unlike standard C vsnprintf/snprintf, they return the number of characters *actually* written,
// not the number of characters that *would* have been printed were buflen unlimited.
extern mDNSu32 mDNS_vsnprintf(char *sbuffer, mDNSu32 buflen, const char *fmt, va_list arg);
extern mDNSu32 mDNS_snprintf(char *sbuffer, mDNSu32 buflen, const char *fmt, ...) IS_A_PRINTF_STYLE_FUNCTION(3,4);
extern mDNSu32 NumCacheRecordsForInterfaceID(const mDNS *const m, mDNSInterfaceID id);
extern char *DNSTypeName(mDNSu16 rrtype);
extern char *GetRRDisplayString_rdb(const ResourceRecord *rr, RDataBody *rd, char *buffer);
#define RRDisplayString(m, rr) GetRRDisplayString_rdb(rr, &(rr)->rdata->u, (m)->MsgBuffer)
#define ARDisplayString(m, rr) GetRRDisplayString_rdb(&(rr)->resrec, &(rr)->resrec.rdata->u, (m)->MsgBuffer)
#define CRDisplayString(m, rr) GetRRDisplayString_rdb(&(rr)->resrec, &(rr)->resrec.rdata->u, (m)->MsgBuffer)
extern mDNSBool mDNSSameAddress(const mDNSAddr *ip1, const mDNSAddr *ip2);
extern void IncrementLabelSuffix(domainlabel *name, mDNSBool RichText);
extern mDNSBool IsPrivateV4Addr(mDNSAddr *addr);  // returns true for RFC1918 private addresses

#define mDNSSameIPv4Address(A,B) ((A).NotAnInteger == (B).NotAnInteger)
#define mDNSSameIPv6Address(A,B) ((A).l[0] == (B).l[0] && (A).l[1] == (B).l[1] && (A).l[2] == (B).l[2] && (A).l[3] == (B).l[3])
#define mDNSSameEthAddress(A,B)  ((A)->w[0] == (B)->w[0] && (A)->w[1] == (B)->w[1] && (A)->w[2] == (B)->w[2])

#define mDNSIPv4AddressIsZero(A) mDNSSameIPv4Address((A), zerov4Addr)
#define mDNSIPv6AddressIsZero(A) mDNSSameIPv6Address((A), zerov6Addr)

#define mDNSIPv4AddressIsOnes(A) mDNSSameIPv4Address((A), onesIPv4Addr)
#define mDNSIPv6AddressIsOnes(A) mDNSSameIPv6Address((A), onesIPv6Addr)

#define mDNSAddressIsAllDNSLinkGroup(X) (                                                     \
	((X)->type == mDNSAddrType_IPv4 && mDNSSameIPv4Address((X)->ip.v4, AllDNSLinkGroupv4)) || \
	((X)->type == mDNSAddrType_IPv6 && mDNSSameIPv6Address((X)->ip.v6, AllDNSLinkGroupv6))    )

#define mDNSAddressIsZero(X) (                                                \
	((X)->type == mDNSAddrType_IPv4 && mDNSIPv4AddressIsZero((X)->ip.v4))  || \
	((X)->type == mDNSAddrType_IPv6 && mDNSIPv6AddressIsZero((X)->ip.v6))     )

#define mDNSAddressIsValidNonZero(X) (                                        \
	((X)->type == mDNSAddrType_IPv4 && !mDNSIPv4AddressIsZero((X)->ip.v4)) || \
	((X)->type == mDNSAddrType_IPv6 && !mDNSIPv6AddressIsZero((X)->ip.v6))    )

#define mDNSAddressIsOnes(X) (                                                \
	((X)->type == mDNSAddrType_IPv4 && mDNSIPv4AddressIsOnes((X)->ip.v4))  || \
	((X)->type == mDNSAddrType_IPv6 && mDNSIPv6AddressIsOnes((X)->ip.v6))     )

#define mDNSAddressIsValid(X) (                                                                                             \
	((X)->type == mDNSAddrType_IPv4) ? !(mDNSIPv4AddressIsZero((X)->ip.v4) || mDNSIPv4AddressIsOnes((X)->ip.v4)) :          \
	((X)->type == mDNSAddrType_IPv6) ? !(mDNSIPv6AddressIsZero((X)->ip.v6) || mDNSIPv6AddressIsOnes((X)->ip.v6)) : mDNSfalse)


// ***************************************************************************
#if 0
#pragma mark - Authentication Support
#endif

#define HMAC_LEN    64
#define HMAC_IPAD   0x36
#define HMAC_OPAD   0x5c
#define MD5_LEN     16

// padded keys for inned/outer hash rounds
typedef struct
	{
	mDNSu8 ipad[HMAC_LEN];
	mDNSu8 opad[HMAC_LEN];
	} HMAC_Key;

// Internal data structure to maintain authentication information for an update domain
typedef struct uDNS_AuthInfo
	{
	domainname zone;
	domainname keyname;
	HMAC_Key key;
	struct uDNS_AuthInfo *next;
	} uDNS_AuthInfo;

// Unicast DNS and Dynamic Update specific Client Calls
//
// mDNS_SetSecretForZone tells the core to authenticate (via TSIG with an HMAC_MD5 hash of the shared secret)
// when dynamically updating a given zone (and its subdomains).  The key used in authentication must be in
// domain name format.  The shared secret must be a null-terminated base64 encoded string.  A minimum size of
// 16 bytes (128 bits) is recommended for an MD5 hash as per RFC 2485.
// Calling this routine multiple times for a zone replaces previously entered values.  Call with a NULL key
// to dissable authentication for the zone.

extern mStatus mDNS_SetSecretForZone(mDNS *m, const domainname *zone, const domainname *key, const char *sharedSecret);

// Hostname/Unicast Interface Configuration

// All hostnames advertised point to one IPv4 address and/or one IPv6 address, set via SetPrimaryInterfaceInfo.  Invoking this routine
// updates all existing hostnames to point to the new address.
	
// A hostname is added via AddDynDNSHostName, which points to the primary interface's v4 and/or v6 addresss

// The status callback is invoked to convey success or failure codes - the callback should not modify the AuthRecord or free memory.
// Added hostnames may be removed (deregistered) via mDNS_RemoveDynDNSHostName.

// Host domains added prior to specification of the primary interface address and computer name will be deferred until
// these values are initialized.

// When routable V4 interfaces are added or removed, mDNS_UpdateLLQs should be called to re-estabish LLQs in case the
// destination address for events (i.e. the route) has changed.  For performance reasons, the caller is responsible for 
// batching changes, e.g.  calling the routine only once if multiple interfaces are simultanously removed or added.

// DNS servers used to resolve unicast queries are specified by mDNS_AddDNSServer, and may later be removed via mDNS_DeleteDNSServers.
// For "split" DNS configurations, in which queries for different domains are sent to different servers (e.g. VPN and external),
// a domain may be associated with a DNS server.  For standard configurations, specify the root label (".") or NULL.
	
extern void mDNS_AddDynDNSHostName(mDNS *m, const domainname *fqdn, mDNSRecordCallback *StatusCallback, const void *StatusContext);
extern void mDNS_RemoveDynDNSHostName(mDNS *m, const domainname *fqdn);
extern void mDNS_SetPrimaryInterfaceInfo(mDNS *m, const mDNSAddr *v4addr,  const mDNSAddr *v6addr, const mDNSAddr *router);
extern void mDNS_UpdateLLQs(mDNS *m);
extern void mDNS_AddDNSServer(mDNS *const m, const mDNSAddr *dnsAddr, const domainname *domain);
extern void mDNS_DeleteDNSServers(mDNS *const m);

// Routines called by the core, exported by DNSDigest.c

// Convert a base64 encoded key into a binary byte stream
extern mDNSs32 DNSDigest_Base64ToBin(const char *src, mDNSu8 *target, mDNSu32 targsize);

// Convert an arbitrary binary key (of any length) into an HMAC key (stored in AuthInfo struct)
extern void DNSDigest_ConstructHMACKey(uDNS_AuthInfo *info, const mDNSu8 *key, mDNSu32 len);

// sign a DNS message.  The message must be compete, with all values in network byte order.  end points to the end
// of the message, and is modified by this routine.  numAdditionals is a pointer to the number of additional
// records in HOST byte order, which is incremented upon successful completion of this routine.  The function returns
// the new end pointer on success, and NULL on failure.
extern mDNSu8 *DNSDigest_SignMessage(DNSMessage *msg, mDNSu8 **end, mDNSu16 *numAdditionals, uDNS_AuthInfo *info);

// ***************************************************************************
#if 0
#pragma mark - PlatformSupport interface
#endif

// This section defines the interface to the Platform Support layer.
// Normal client code should not use any of types defined here, or directly call any of the functions defined here.
// The definitions are placed here because sometimes clients do use these calls indirectly, via other supported client operations.
// For example, AssignDomainName is a macro defined using mDNSPlatformMemCopy()

// Every platform support module must provide the following functions.
// mDNSPlatformInit() typically opens a communication endpoint, and starts listening for mDNS packets.
// When Setup is complete, the platform support layer calls mDNSCoreInitComplete().
// mDNSPlatformSendUDP() sends one UDP packet
// When a packet is received, the PlatformSupport code calls mDNSCoreReceive()
// mDNSPlatformClose() tidies up on exit
//
// Note: mDNSPlatformMemAllocate/mDNSPlatformMemFree are only required for handling oversized resource records and unicast DNS.
// If your target platform has a well-defined specialized application, and you know that all the records it uses
// are InlineCacheRDSize or less, then you can just make a simple mDNSPlatformMemAllocate() stub that always returns
// NULL. InlineCacheRDSize is a compile-time constant, which is set by default to 64. If you need to handle records
// a little larger than this and you don't want to have to implement run-time allocation and freeing, then you
// can raise the value of this constant to a suitable value (at the expense of increased memory usage).
//
// USE CAUTION WHEN CALLING mDNSPlatformRawTime: The m->timenow_adjust correction factor needs to be added
// Generally speaking:
// Code that's protected by the main mDNS lock should just use the m->timenow value
// Code outside the main mDNS lock should use mDNS_TimeNow(m) to get properly adjusted time
// In certain cases there may be reasons why it's necessary to get the time without taking the lock first
// (e.g. inside the routines that are doing the locking and unlocking, where a call to get the lock would result in a
// recursive loop); in these cases use mDNS_TimeNow_NoLock(m) to get mDNSPlatformRawTime with the proper correction factor added.
//
// mDNSPlatformUTC returns the time, in seconds, since Jan 1st 1970 UTC and is required for generating TSIG records

extern mStatus  mDNSPlatformInit        (mDNS *const m);
extern void     mDNSPlatformClose       (mDNS *const m);
extern mStatus  mDNSPlatformSendUDP(const mDNS *const m, const void *const msg, const mDNSu8 *const end,
mDNSInterfaceID InterfaceID, const mDNSAddr *dst, mDNSIPPort dstport);

extern void     mDNSPlatformLock        (const mDNS *const m);
extern void     mDNSPlatformUnlock      (const mDNS *const m);

extern void     mDNSPlatformStrCopy     (const void *src,       void *dst);
extern mDNSu32  mDNSPlatformStrLen      (const void *src);
extern void     mDNSPlatformMemCopy     (const void *src,       void *dst, mDNSu32 len);
extern mDNSBool mDNSPlatformMemSame     (const void *src, const void *dst, mDNSu32 len);
extern void     mDNSPlatformMemZero     (                       void *dst, mDNSu32 len);
extern void *   mDNSPlatformMemAllocate (mDNSu32 len);
extern void     mDNSPlatformMemFree     (void *mem);
extern mDNSu32  mDNSPlatformRandomSeed  (void);
extern mStatus  mDNSPlatformTimeInit    (void);
extern mDNSs32  mDNSPlatformRawTime     (void);
extern mDNSs32  mDNSPlatformUTC         (void);
#define mDNS_TimeNow_NoLock(m) (mDNSPlatformRawTime() + m->timenow_adjust)

// Platform support modules should provide the following functions to map between opaque interface IDs
// and interface indexes in order to support the DNS-SD API. If your target platform does not support
// multiple interfaces and/or does not support the DNS-SD API, these functions can be empty.
extern mDNSInterfaceID mDNSPlatformInterfaceIDfromInterfaceIndex(mDNS *const m, mDNSu32 index);
extern mDNSu32 mDNSPlatformInterfaceIndexfromInterfaceID(mDNS *const m, mDNSInterfaceID id);

// Every platform support module must provide the following functions if it is to support unicast DNS
// and Dynamic Update.
// All TCP socket operations implemented by the platform layer MUST NOT BLOCK.
// mDNSPlatformTCPConnect initiates a TCP connection with a peer, adding the socket descriptor to the
// main event loop.  The return value indicates whether the connection succeeded, failed, or is pending
// (i.e. the call would block.)  On return, the descriptor parameter is set to point to the connected socket.
// The TCPConnectionCallback is subsequently invoked when the connection
// completes (in which case the ConnectionEstablished parameter is true), or data is available for
// reading on the socket (indicated by the ConnectionEstablished parameter being false.)  If the connection
// asynchronously fails, the TCPConnectionCallback should be invoked as usual, with the error being
// returned in subsequent calls to PlatformReadTCP or PlatformWriteTCP.  (This allows for platforms
// with limited asynchronous error detection capabilities.)  PlatformReadTCP and PlatformWriteTCP must
// return the number of bytes read/written, 0 if the call would block, and -1 if an error.
// PlatformTCPCloseConnection must close the connection to the peer and remove the descriptor from the
// event loop.  CloseConnectin may be called at any time, including in a ConnectionCallback.

typedef void (*TCPConnectionCallback)(int sd, void *context, mDNSBool ConnectionEstablished);
extern mStatus mDNSPlatformTCPConnect(const mDNSAddr *dst, mDNSOpaque16 dstport, mDNSInterfaceID InterfaceID,
										  TCPConnectionCallback callback, void *context, int *descriptor);
extern void mDNSPlatformTCPCloseConnection(int sd);
extern int mDNSPlatformReadTCP(int sd, void *buf, int buflen);
extern int mDNSPlatformWriteTCP(int sd, const char *msg, int len);

// Platforms that support unicast browsing and dynamic update registration for clients who do not specify a domain
// in browse/registration calls must implement these routines to get the "default" browse/registration list.
// The Get() functions must return a linked list of DNameListElem structs, allocated via mDNSPlatformMemAllocate.
// Platforms may implement the Get() calls via the mDNS_CopyDNameList() helper routine.
// Callers should free lists obtained via the Get() calls with th mDNS_FreeDNameList routine, provided by the core.

typedef struct DNameListElem
	{
	domainname name;
	struct DNameListElem *next;
	} DNameListElem;

extern DNameListElem *mDNSPlatformGetSearchDomainList(void);
extern DNameListElem *mDNSPlatformGetRegDomainList(void);

// Helper functions provided by the core
extern DNameListElem *mDNS_CopyDNameList(const DNameListElem *orig);
extern void mDNS_FreeDNameList(DNameListElem *list);

#ifdef _LEGACY_NAT_TRAVERSAL_
// Support for legacy NAT traversal protocols, implemented by the platform layer and callable by the core.

#define DYN_PORT_MIN 49152 // ephemeral port range
#define DYN_PORT_MAX 65535
#define LEGACY_NATMAP_MAX_TRIES 4 // if our desired mapping is taken, how many times we try mapping to a random port

extern mStatus LNT_GetPublicIP(mDNSOpaque32 *ip);
extern mStatus LNT_MapPort(mDNSIPPort priv, mDNSIPPort pub, mDNSBool tcp);
extern mStatus LNT_UnmapPort(mDNSIPPort PubPort, mDNSBool tcp);
#endif // _LEGACY_NAT_TRAVERSAL_

// The core mDNS code provides these functions, for the platform support code to call at appropriate times
//
// mDNS_SetFQDN() is called once on startup (typically from mDNSPlatformInit())
// and then again on each subsequent change of the host name.
//
// mDNS_RegisterInterface() is used by the platform support layer to inform mDNSCore of what
// physical and/or logical interfaces are available for sending and receiving packets.
// Typically it is called on startup for each available interface, but register/deregister may be
// called again later, on multiple occasions, to inform the core of interface configuration changes.
// If set->Advertise is set non-zero, then mDNS_RegisterInterface() also registers the standard
// resource records that should be associated with every publicised IP address/interface:
// -- Name-to-address records (A/AAAA)
// -- Address-to-name records (PTR)
// -- Host information (HINFO)
// IMPORTANT: The specified mDNSInterfaceID MUST NOT be 0, -1, or -2; these values have special meaning
// mDNS_RegisterInterface does not result in the registration of global hostnames via dynamic update -
// see mDNS_SetPrimaryInterfaceInfo, mDNS_AddDynDNSHostName, etc. for this purpose.
// Note that the set may be deallocated immediately after it is deregistered via mDNS_DeegisterInterface.
//
// mDNS_RegisterDNS() is used by the platform support layer to provide the core with the addresses of
// available domain name servers for unicast queries/updates.  RegisterDNS() should be called once for
// each name server, typically at startup, or when a new name server becomes available.  DeregiterDNS()
// must be called whenever a registered name server becomes unavailable.  DeregisterDNSList deregisters
// all registered servers.  mDNS_DNSRegistered() returns true if one or more servers are registered in the core.
//
// mDNSCoreInitComplete() is called when the platform support layer is finished.
// Typically this is at the end of mDNSPlatformInit(), but may be later
// (on platforms like OT that allow asynchronous initialization of the networking stack).
//
// mDNSCoreReceive() is called when a UDP packet is received
//
// mDNSCoreMachineSleep() is called when the machine sleeps or wakes
// (This refers to heavyweight laptop-style sleep/wake that disables network access,
// not lightweight second-by-second CPU power management modes.)

extern void     mDNS_SetFQDN(mDNS *const m);
extern mStatus  mDNS_RegisterInterface  (mDNS *const m, NetworkInterfaceInfo *set, mDNSBool flapping);
extern void     mDNS_DeregisterInterface(mDNS *const m, NetworkInterfaceInfo *set, mDNSBool flapping);
extern void     mDNSCoreInitComplete(mDNS *const m, mStatus result);
extern void     mDNSCoreReceive(mDNS *const m, void *const msg, const mDNSu8 *const end,
								const mDNSAddr *const srcaddr, const mDNSIPPort srcport,
								const mDNSAddr *const dstaddr, const mDNSIPPort dstport, const mDNSInterfaceID InterfaceID);
extern void     mDNSCoreMachineSleep(mDNS *const m, mDNSBool wake);

extern mDNSBool mDNSAddrIsDNSMulticast(const mDNSAddr *ip);

// ***************************************************************************
#if 0
#pragma mark - Compile-Time assertion checks
#endif

// Some C compiler cleverness. We can make the compiler check certain things for
// us, and report compile-time errors if anything is wrong. The usual way to do
// this would be to use a run-time "if" statement, but then you don't find out
// what's wrong until you run the software. This way, if the assertion condition
// is false, the array size is negative, and the complier complains immediately.

struct mDNS_CompileTimeAssertionChecks
	{
	// Check that the compiler generated our on-the-wire packet format structure definitions
	// properly packed, without adding padding bytes to align fields on 32-bit or 64-bit boundaries.
	char assert0[(sizeof(rdataSRV)         == 262                          ) ? 1 : -1];
	char assert1[(sizeof(DNSMessageHeader) ==  12                          ) ? 1 : -1];
	char assert2[(sizeof(DNSMessage)       ==  12+AbsoluteMaxDNSMessageData) ? 1 : -1];
	char assert3[(sizeof(mDNSs8)           ==   1                          ) ? 1 : -1];
	char assert4[(sizeof(mDNSu8)           ==   1                          ) ? 1 : -1];
	char assert5[(sizeof(mDNSs16)          ==   2                          ) ? 1 : -1];
	char assert6[(sizeof(mDNSu16)          ==   2                          ) ? 1 : -1];
	char assert7[(sizeof(mDNSs32)          ==   4                          ) ? 1 : -1];
	char assert8[(sizeof(mDNSu32)          ==   4                          ) ? 1 : -1];
	char assert9[(sizeof(mDNSOpaque16)     ==   2                          ) ? 1 : -1];
	char assertA[(sizeof(mDNSOpaque32)     ==   4                          ) ? 1 : -1];
	char assertB[(sizeof(mDNSOpaque128)    ==  16                          ) ? 1 : -1];
	char assertC[(sizeof(CacheRecord  )    >=  sizeof(CacheGroup)          ) ? 1 : -1];
	char assertD[(sizeof(int)              >=  4                           ) ? 1 : -1];
	};

// ***************************************************************************

#ifdef __cplusplus
	}
#endif

#endif
