/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002-2006 Apple Computer, Inc. All rights reserved.
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
 * This code is completely 100% portable C. It does not depend on any external header files
 * from outside the mDNS project -- all the types it expects to find are defined right here.
 * 
 * The previous point is very important: This file does not depend on any external
 * header files. It should complile on *any* platform that has a C compiler, without
 * making *any* assumptions about availability of so-called "standard" C functions,
 * routines, or types (which may or may not be present on any given platform).

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

$Log: mDNS.c,v $
Revision 1.537.2.1  2006/08/29 06:24:22  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.537  2006/03/19 02:00:07  cheshire
<rdar://problem/4073825> Improve logic for delaying packets after repeated interface transitions

Revision 1.536  2006/03/08 23:29:53  cheshire
<rdar://problem/4468716> Improve "Service Renamed" log message

Revision 1.535  2006/03/02 20:41:17  cheshire
<rdar://problem/4111464> After record update, old record sometimes remains in cache
Minor code tidying and comments to reduce the risk of similar programming errors in future

Revision 1.534  2006/03/02 03:25:46  cheshire
<rdar://problem/4111464> After record update, old record sometimes remains in cache
Code to harmonize RRSet TTLs was inadvertently rescuing expiring records

Revision 1.533  2006/02/26 00:54:41  cheshire
Fixes to avoid code generation warning/error on FreeBSD 7

Revision 1.532  2005/12/02 20:24:36  cheshire
<rdar://problem/4363209> Adjust cutoff time for KA list by one second

Revision 1.531  2005/12/02 19:05:42  cheshire
Tidy up constants

Revision 1.530  2005/11/07 01:49:48  cheshire
For consistency, use NonZeroTime() function instead of ?: expression

Revision 1.529  2005/10/25 23:42:24  cheshire
<rdar://problem/4316057> Error in ResolveSimultaneousProbe() when type or class don't match
Changed switch statement to an "if"

Revision 1.528  2005/10/25 23:34:22  cheshire
<rdar://problem/4316048> RequireGoodbye state not set/respected sometimes when machine going to sleep

Revision 1.527  2005/10/25 22:43:59  cheshire
Add clarifying comments

Revision 1.526  2005/10/20 00:10:33  cheshire
<rdar://problem/4290265> Add check to avoid crashing NAT gateways that have buggy DNS relay code

Revision 1.525  2005/09/24 00:47:17  cheshire
Fix comment typos

Revision 1.524  2005/09/16 21:06:49  cheshire
Use mDNS_TimeNow_NoLock macro, instead of writing "mDNSPlatformRawTime() + m->timenow_adjust" all over the place

Revision 1.523  2005/03/21 00:33:51  shersche
<rdar://problem/4021486> Fix build warnings on Win32 platform

Revision 1.522  2005/03/04 21:48:12  cheshire
<rdar://problem/4037283> Fractional time rounded down instead of up on platforms with coarse clock granularity

Revision 1.521  2005/02/25 04:21:00  cheshire
<rdar://problem/4015377> mDNS -F returns the same domain multiple times with different casing

Revision 1.520  2005/02/16 01:14:11  cheshire
Convert RR Cache LogOperation() calls to debugf()

Revision 1.519  2005/02/15 01:57:20  cheshire
When setting "q->LastQTxTime = m->timenow", must also clear q->RecentAnswerPkts to zero

Revision 1.518  2005/02/10 22:35:17  cheshire
<rdar://problem/3727944> Update name

Revision 1.517  2005/02/03 00:21:21  cheshire
Update comments about BIND named and zero-length TXT records

Revision 1.516  2005/01/28 06:06:32  cheshire
Update comment

Revision 1.515  2005/01/27 00:21:49  cheshire
<rdar://problem/3973798> Remove mDNSResponder sleep/wake syslog message

Revision 1.514  2005/01/21 01:33:45  cheshire
<rdar://problem/3962979> Shutdown time regression: mDNSResponder not responding to SIGTERM

Revision 1.513  2005/01/21 00:07:54  cheshire
<rdar://problem/3962717> Infinite loop when the same service is registered twice, and then suffers a name conflict

Revision 1.512  2005/01/20 00:37:45  cheshire
<rdar://problem/3941448> mDNSResponder crashed in mDNSCoreReceiveResponse
Take care not to recycle records while they are on the CacheFlushRecords list

Revision 1.511  2005/01/19 22:48:53  cheshire
<rdar://problem/3955355> Handle services with subtypes correctly when doing mDNS_RenameAndReregisterService()

Revision 1.510  2005/01/19 03:12:45  cheshire
Move LocalRecordReady() macro from mDNS.c to DNSCommon.h

Revision 1.509  2005/01/19 03:08:49  cheshire
<rdar://problem/3961051> CPU Spin in mDNSResponder
Log messages to help catch and report CPU spins

Revision 1.508  2005/01/18 18:56:32  cheshire
<rdar://problem/3934245> QU responses not promoted to multicast responses when appropriate

Revision 1.507  2005/01/18 01:12:07  cheshire
<rdar://problem/3956258> Logging into VPN causes mDNSResponder to reissue multicast probes

Revision 1.506  2005/01/17 23:28:53  cheshire
Fix compile error

Revision 1.505  2005/01/11 02:02:56  shersche
Move variable declaration to the beginning of statement block

Revision 1.504  2004/12/20 20:24:35  cheshire
<rdar://problem/3928456> Network efficiency: Don't keep polling if we have at least one unique-type answer

Revision 1.503  2004/12/20 18:41:47  cheshire
<rdar://problem/3591622> Low memory support: Provide answers even when we don't have cache space

Revision 1.502  2004/12/20 18:04:08  cheshire
<rdar://problem/3923098> For now, don't put standard wide-area unicast responses in our main cache

Revision 1.501  2004/12/19 23:50:18  cheshire
<rdar://problem/3751638> kDNSServiceInterfaceIndexLocalOnly should return all local records
Don't show "No active interface to send" messages for kDNSServiceInterfaceIndexLocalOnly services

Revision 1.500  2004/12/18 03:13:46  cheshire
<rdar://problem/3751638> kDNSServiceInterfaceIndexLocalOnly should return all local records

Revision 1.499  2004/12/17 23:37:45  cheshire
<rdar://problem/3485365> Guard against repeating wireless dissociation/re-association
(and other repetitive configuration changes)

Revision 1.498  2004/12/17 05:25:46  cheshire
<rdar://problem/3925163> Shorten DNS-SD queries to avoid NAT bugs

Revision 1.497  2004/12/17 03:20:58  cheshire
<rdar://problem/3925168> Don't send unicast replies we know will be ignored

Revision 1.496  2004/12/16 22:18:26  cheshire
Make AddressIsLocalSubnet() a little more selective -- ignore point-to-point interfaces

Revision 1.495  2004/12/16 21:27:37  ksekar
Fixed build failures when compiled with verbose debugging messages

Revision 1.494  2004/12/16 20:46:56  cheshire
Fix compiler warnings

Revision 1.493  2004/12/16 20:13:00  cheshire
<rdar://problem/3324626> Cache memory management improvements

Revision 1.492  2004/12/16 08:03:24  shersche
Fix compilation error when UNICAST_DISABLED is set

Revision 1.491  2004/12/11 01:52:11  cheshire
<rdar://problem/3785820> Support kDNSServiceFlagsAllowRemoteQuery for registering services too

Revision 1.490  2004/12/10 20:06:25  cheshire
<rdar://problem/3915074> Reduce egregious stack space usage
Reduced SendDelayedUnicastResponse() stack frame from 9K to 112 bytes

Revision 1.489  2004/12/10 20:03:43  cheshire
<rdar://problem/3915074> Reduce egregious stack space usage
Reduced mDNSCoreReceiveQuery() stack frame from 9K to 144 bytes

Revision 1.488  2004/12/10 19:50:41  cheshire
<rdar://problem/3915074> Reduce egregious stack space usage
Reduced SendResponses() stack frame from 9K to 176 bytes

Revision 1.487  2004/12/10 19:39:13  cheshire
<rdar://problem/3915074> Reduce egregious stack space usage
Reduced SendQueries() stack frame from 18K to 112 bytes

Revision 1.486  2004/12/10 14:16:17  cheshire
<rdar://problem/3889788> Relax update rate limiting
We now allow an average rate of ten updates per minute.
Updates in excess of that are rate limited, but more gently than before.

Revision 1.485  2004/12/10 02:09:24  cheshire
<rdar://problem/3898376> Modify default TTLs

Revision 1.484  2004/12/09 03:15:40  ksekar
<rdar://problem/3806610> use _legacy instead of _default to find "empty string" browse domains

Revision 1.483  2004/12/07 23:00:14  ksekar
<rdar://problem/3908336> DNSServiceRegisterRecord() can crash on deregistration:
Call RecordProbeFailure even if there is no record callback

Revision 1.482  2004/12/07 22:49:06  cheshire
<rdar://problem/3908850> BIND doesn't allow zero-length TXT records

Revision 1.481  2004/12/07 21:26:04  ksekar
<rdar://problem/3908336> DNSServiceRegisterRecord() can crash on deregistration

Revision 1.480  2004/12/07 20:42:33  cheshire
Add explicit context parameter to mDNS_RemoveRecordFromService()

Revision 1.479  2004/12/07 17:50:49  ksekar
<rdar://problem/3908850> BIND doesn't allow zero-length TXT records

Revision 1.478  2004/12/06 21:15:22  ksekar
<rdar://problem/3884386> mDNSResponder crashed in CheckServiceRegistrations

Revision 1.477  2004/12/04 02:12:45  cheshire
<rdar://problem/3517236> mDNSResponder puts LargeCacheRecord on the stack

Revision 1.476  2004/11/29 23:34:31  cheshire
On platforms with coarse time resolutions, ORing time values with one to ensure they are non-zero
is crude, and effectively halves the time resolution. The more selective NonZeroTime() function
only nudges the time value to 1 if the interval calculation happens to result in the value zero.

Revision 1.475  2004/11/29 23:13:31  cheshire
<rdar://problem/3484552> All unique records in a set should have the cache flush bit set
Additional check: Make sure we don't unnecessarily send packets containing only additionals.
(This could occur with multi-packet KA lists, if the answer and additionals were marked
by the query packet, and then the answer were later suppressed in a subsequent KA packet.)

Revision 1.474  2004/11/29 17:18:12  cheshire
Remove "Unknown DNS packet type" message for update responses

Revision 1.473  2004/11/25 01:57:52  cheshire
<rdar://problem/3484552> All unique records in a set should have the cache flush bit set

Revision 1.472  2004/11/25 01:28:09  cheshire
<rdar://problem/3557050> Need to implement random delay for 'QU' unicast replies (and set cache flush bit too)

Revision 1.471  2004/11/25 01:10:13  cheshire
Move code to add additional records to a subroutine called AddAdditionalsToResponseList()

Revision 1.470  2004/11/24 21:54:44  cheshire
<rdar://problem/3894475> mDNSCore not receiving unicast responses properly

Revision 1.469  2004/11/24 04:50:39  cheshire
Minor tidying

Revision 1.468  2004/11/24 01:47:07  cheshire
<rdar://problem/3780207> DNSServiceRegisterRecord should call CallBack on success.

Revision 1.467  2004/11/24 01:41:28  cheshire
Rename CompleteProbing() to AcknowledgeRecord()

Revision 1.466  2004/11/23 21:08:07  ksekar
Don't use ID to demux multicast/unicast now that unicast uses random IDs

Revision 1.465  2004/11/15 20:09:21  ksekar
<rdar://problem/3719050> Wide Area support for Add/Remove record

Revision 1.464  2004/11/03 01:44:36  cheshire
Update debugging messages

Revision 1.463  2004/10/29 02:38:48  cheshire
Fix Windows compile errors

Revision 1.462  2004/10/28 19:21:07  cheshire
Guard against registering interface with zero InterfaceID

Revision 1.461  2004/10/28 19:02:16  cheshire
Remove \n from LogMsg() call

Revision 1.460  2004/10/28 03:24:40  cheshire
Rename m->CanReceiveUnicastOn as m->CanReceiveUnicastOn5353

Revision 1.459  2004/10/26 22:34:37  cheshire
<rdar://problem/3468995> Need to protect mDNSResponder from unbounded packet flooding

Revision 1.458  2004/10/26 20:45:28  cheshire
Show mask in "invalid mask" message

Revision 1.457  2004/10/26 06:28:36  cheshire
Now that we don't check IP TTL any more, remove associated log message

Revision 1.456  2004/10/26 06:21:42  cheshire
Adjust mask validity check to allow an all-ones mask (for IPv6 ::1 loopback address)

Revision 1.455  2004/10/26 06:11:40  cheshire
Add improved logging to aid in diagnosis of <rdar://problem/3842714> mDNSResponder crashed

Revision 1.454  2004/10/23 01:16:00  cheshire
<rdar://problem/3851677> uDNS operations not always reliable on multi-homed hosts

Revision 1.453  2004/10/22 20:52:06  ksekar
<rdar://problem/3799260> Create NAT port mappings for Long Lived Queries

Revision 1.452  2004/10/20 01:50:40  cheshire
<rdar://problem/3844991> Cannot resolve non-local registrations using the mach API
Implemented ForceMCast mode for AuthRecords as well as for Questions

Revision 1.451  2004/10/19 21:33:15  cheshire
<rdar://problem/3844991> Cannot resolve non-local registrations using the mach API
Added flag 'kDNSServiceFlagsForceMulticast'. Passing through an interface id for a unicast name
doesn't force multicast unless you set this flag to indicate explicitly that this is what you want

Revision 1.450  2004/10/19 17:42:59  ksekar
Fixed compiler warnings for non-debug builds.

Revision 1.449  2004/10/18 22:57:07  cheshire
<rdar://problem/3711302> Seen in console: Ignored apparent spoof mDNS Response with TTL 1

Revision 1.448  2004/10/16 00:16:59  cheshire
<rdar://problem/3770558> Replace IP TTL 255 check with local subnet source address check

Revision 1.447  2004/10/15 00:51:21  cheshire
<rdar://problem/3711302> Seen in console: Ignored apparent spoof mDNS Response with TTL 1

Revision 1.446  2004/10/14 00:43:34  cheshire
<rdar://problem/3815984> Services continue to announce SRV and HINFO

Revision 1.445  2004/10/12 21:07:09  cheshire
Set up m->p in mDNS_Init() before calling mDNSPlatformTimeInit()

Revision 1.444  2004/10/11 17:54:16  ksekar
Changed hashtable pointer output from debugf to verbosedebugf.

Revision 1.443  2004/10/10 07:05:45  cheshire
For consistency, use symbol "localdomain" instead of literal string

Revision 1.442  2004/10/08 20:25:10  cheshire
Change of plan for <rdar://problem/3831716> -- we're not going to do that at this time

Revision 1.441  2004/10/08 03:25:01  ksekar
<rdar://problem/3831716> domain enumeration should use LLQs

Revision 1.440  2004/10/06 01:44:19  cheshire
<rdar://problem/3813936> Resolving too quickly sometimes returns stale TXT record

Revision 1.439  2004/10/03 23:14:11  cheshire
Add "mDNSEthAddr" type and "zeroEthAddr" constant

Revision 1.438  2004/09/29 23:07:04  cheshire
Patch from Pavel Repin to fix compile error on Windows

Revision 1.437  2004/09/28 02:23:50  cheshire
<rdar://problem/3637266> Deliver near-pending "remove" events before new "add" events
Don't need to search the entire cache for nearly-expired records -- just the appropriate hash slot
For records with the cache flush bit set, defer the decision until the end of the packet

Revision 1.436  2004/09/28 01:27:04  cheshire
Update incorrect log message

Revision 1.435  2004/09/25 02:41:39  cheshire
<rdar://problem/3637266> Deliver near-pending "remove" events before new "add" events

Revision 1.434  2004/09/25 02:32:06  cheshire
Update comments

Revision 1.433  2004/09/25 02:24:27  cheshire
Removed unused rr->UseCount

Revision 1.432  2004/09/24 21:35:17  cheshire
<rdar://problem/3561220> Browses are no longer piggybacking on other browses
TargetPort and TargetQID are allowed to be undefined if no question->Target is set

Revision 1.431  2004/09/24 21:33:12  cheshire
Adjust comment

Revision 1.430  2004/09/24 02:15:49  cheshire
<rdar://problem/3680865> Late conflicts don't send goodbye packets on other interfaces

Revision 1.429  2004/09/24 00:20:21  cheshire
<rdar://problem/3483349> Any rrtype is a conflict for unique records

Revision 1.428  2004/09/24 00:12:25  cheshire
Get rid of unused RRUniqueOrKnownUnique(RR)

Revision 1.427  2004/09/23 20:44:11  cheshire
<rdar://problem/3813148> Reduce timeout before expiring records on failure

Revision 1.426  2004/09/23 20:21:07  cheshire
<rdar://problem/3426876> Refine "immediate answer burst; restarting exponential backoff sequence" logic
Associate a unique sequence number with each received packet, and only increment the count of recent answer
packets if the packet sequence number for this answer record is not one we've already seen and counted.

Revision 1.425  2004/09/23 20:14:38  cheshire
Rename "question->RecentAnswers" to "question->RecentAnswerPkts"

Revision 1.424  2004/09/23 00:58:36  cheshire
<rdar://problem/3781269> Rate limiting interferes with updating TXT records

Revision 1.423  2004/09/23 00:50:53  cheshire
<rdar://problem/3419452> Don't send a (DE) if a service is unregistered after wake from sleep

Revision 1.422  2004/09/22 02:34:46  cheshire
Move definitions of default TTL times from mDNS.c to mDNSEmbeddedAPI.h

Revision 1.421  2004/09/21 23:29:49  cheshire
<rdar://problem/3680045> DNSServiceResolve should delay sending packets

Revision 1.420  2004/09/21 23:01:42  cheshire
Update debugf messages

Revision 1.419  2004/09/21 19:51:14  cheshire
Move "Starting time value" message from mDNS.c to mDNSMacOSX/daemon.c

Revision 1.418  2004/09/21 18:40:17  cheshire
<rdar://problem/3376752> Adjust default record TTLs

Revision 1.417  2004/09/21 17:32:16  cheshire
<rdar://problem/3809484> Rate limiting imposed too soon

Revision 1.416  2004/09/20 23:52:01  cheshire
CFSocket{Puma}.c renamed to mDNSMacOSX{Puma}.c

Revision 1.415  2004/09/18 01:14:09  cheshire
<rdar://problem/3485375> Resolve() should not bother doing AAAA queries on machines with no IPv6 interfaces

Revision 1.414  2004/09/18 01:06:48  cheshire
Add comments

Revision 1.413  2004/09/17 01:08:48  cheshire
Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
  The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
  declared in that file are ONLY appropriate to single-address-space embedded applications.
  For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.

Revision 1.412  2004/09/17 00:46:33  cheshire
mDNS_TimeNow should take const mDNS parameter

Revision 1.411  2004/09/17 00:31:51  cheshire
For consistency with ipv6, renamed rdata field 'ip' to 'ipv4'

Revision 1.410  2004/09/17 00:19:10  cheshire
For consistency with AllDNSLinkGroupv6, rename AllDNSLinkGroup to AllDNSLinkGroupv4

Revision 1.409  2004/09/16 21:59:15  cheshire
For consistency with zerov6Addr, rename zeroIPAddr to zerov4Addr

Revision 1.408  2004/09/16 21:36:36  cheshire
<rdar://problem/3803162> Fix unsafe use of mDNSPlatformTimeNow()
Changes to add necessary locking calls around unicast DNS operations

Revision 1.407  2004/09/16 02:29:39  cheshire
Moved mDNS_Lock/mDNS_Unlock to DNSCommon.c; Added necessary locking around
uDNS_ReceiveMsg, uDNS_StartQuery, uDNS_UpdateRecord, uDNS_RegisterService

Revision 1.406  2004/09/16 01:58:14  cheshire
Fix compiler warnings

Revision 1.405  2004/09/16 00:24:48  cheshire
<rdar://problem/3803162> Fix unsafe use of mDNSPlatformTimeNow()

Revision 1.404  2004/09/15 21:44:11  cheshire
<rdar://problem/3681031> Randomize initial timenow_adjust value in mDNS_Init
Show time value in log to help diagnose errors

Revision 1.403  2004/09/15 00:46:32  ksekar
Changed debugf to verbosedebugf in CheckCacheExpiration

Revision 1.402  2004/09/14 23:59:55  cheshire
<rdar://problem/3681031> Randomize initial timenow_adjust value in mDNS_Init

Revision 1.401  2004/09/14 23:27:46  cheshire
Fix compile errors

Revision 1.400  2004/09/02 03:48:47  cheshire
<rdar://problem/3709039> Disable targeted unicast query support by default
1. New flag kDNSServiceFlagsAllowRemoteQuery to indicate we want to allow remote queries for this record
2. New field AllowRemoteQuery in AuthRecord structure
3. uds_daemon.c sets AllowRemoteQuery if kDNSServiceFlagsAllowRemoteQuery is set
4. mDNS.c only answers remote queries if AllowRemoteQuery is set

Revision 1.399  2004/09/02 01:39:40  cheshire
For better readability, follow consistent convention that QR bit comes first, followed by OP bits

Revision 1.398  2004/09/01 03:59:29  ksekar
<rdar://problem/3783453>: Conditionally compile out uDNS code on Windows

Revision 1.397  2004/08/25 22:04:25  rpantos
Fix the standard Windows compile error.

Revision 1.396  2004/08/25 00:37:27  ksekar
<rdar://problem/3774635>: Cleanup DynDNS hostname registration code

Revision 1.395  2004/08/18 17:21:18  ksekar
Removed double-call of uDNS_AdvertiseInterface from mDNS_SetFQDNs()

Revision 1.394  2004/08/14 03:22:41  cheshire
<rdar://problem/3762579> Dynamic DNS UI <-> mDNSResponder glue
Add GetUserSpecifiedDDNSName() routine
Convert ServiceRegDomain to domainname instead of C string
Replace mDNS_GenerateFQDN/mDNS_GenerateGlobalFQDN with mDNS_SetFQDNs

Revision 1.393  2004/08/13 23:42:52  cheshire
Removed unused "zeroDomainNamePtr"

Revision 1.392  2004/08/13 23:37:02  cheshire
Now that we do both uDNS and mDNS, global replace "uDNS_info.hostname" with
"uDNS_info.UnicastHostname" for clarity

Revision 1.391  2004/08/13 23:25:00  cheshire
Now that we do both uDNS and mDNS, global replace "m->hostname" with
"m->MulticastHostname" for clarity

Revision 1.390  2004/08/11 02:17:01  cheshire
<rdar://problem/3514236> Registering service with port number 0 should create a "No Such Service" record

Revision 1.389  2004/08/10 23:19:14  ksekar
<rdar://problem/3722542>: DNS Extension daemon for Wide Area Service Discovery
Moved routines/constants to allow extern access for garbage collection daemon

Revision 1.388  2004/07/30 17:40:06  ksekar
<rdar://problem/3739115>: TXT Record updates not available for wide-area services

Revision 1.387  2004/07/26 22:49:30  ksekar
<rdar://problem/3651409>: Feature #9516: Need support for NATPMP in client

Revision 1.386  2004/07/13 21:24:24  rpantos
Fix for <rdar://problem/3701120>.

Revision 1.385  2004/06/18 19:09:59  cheshire
<rdar://problem/3588761> Current method of doing subtypes causes name collisions

Revision 1.384  2004/06/15 04:31:23  cheshire
Make sure to clear m->CurrentRecord at the end of AnswerNewLocalOnlyQuestion()

Revision 1.383  2004/06/11 00:04:59  cheshire
<rdar://problem/3595602> TTL must be greater than zero for DNSServiceRegisterRecord

Revision 1.382  2004/06/08 04:59:40  cheshire
Tidy up wording -- log messages are already prefixed with "mDNSResponder", so don't need to repeat it

Revision 1.381  2004/06/05 00:57:30  cheshire
Remove incorrect LogMsg()

Revision 1.380  2004/06/05 00:04:26  cheshire
<rdar://problem/3668639>: wide-area domains should be returned in reg. domain enumeration

Revision 1.379  2004/05/28 23:42:36  ksekar
<rdar://problem/3258021>: Feature: DNS server->client notification on record changes (#7805)

Revision 1.378  2004/05/25 17:25:25  cheshire
Remove extraneous blank lines and white space

Revision 1.377  2004/05/18 23:51:25  cheshire
Tidy up all checkin comments to use consistent "<rdar://problem/xxxxxxx>" format for bug numbers

Revision 1.376  2004/05/05 18:30:44  ksekar
Restored surpressed Cache Tail debug messages.

Revision 1.375  2004/04/26 21:36:25  cheshire
Only send IPv4 (or v6) multicast when IPv4 (or v6) multicast send/receive
is indicated as being available on that interface

Revision 1.374  2004/04/21 02:53:26  cheshire
Typo in debugf statement

Revision 1.373  2004/04/21 02:49:11  cheshire
To reduce future confusion, renamed 'TxAndRx' to 'McastTxRx'

Revision 1.372  2004/04/21 02:38:51  cheshire
Add debugging checks

Revision 1.371  2004/04/14 23:09:28  ksekar
Support for TSIG signed dynamic updates.

Revision 1.370  2004/04/09 17:40:26  cheshire
Remove unnecessary "Multicast" field -- it duplicates the semantics of the existing McastTxRx field

Revision 1.369  2004/04/09 16:34:00  cheshire
Debugging code for later; currently unused

Revision 1.368  2004/04/02 19:19:48  cheshire
Add code to do optional logging of multi-packet KA list time intervals

Revision 1.367  2004/03/20 03:16:10  cheshire
Minor refinement to "Excessive update rate" message

Revision 1.366  2004/03/20 03:12:57  cheshire
<rdar://problem/3587619>: UpdateCredits not granted promptly enough

Revision 1.365  2004/03/19 23:51:22  cheshire
Change to use symbolic constant kUpdateCreditRefreshInterval instead of (mDNSPlatformOneSecond * 60)

Revision 1.364  2004/03/13 01:57:33  ksekar
<rdar://problem/3192546>: DynDNS: Dynamic update of service records

Revision 1.363  2004/03/12 21:00:51  cheshire
Also show port numbers when logging "apparent spoof mDNS Response" messages

Revision 1.362  2004/03/12 08:58:18  cheshire
Guard against empty TXT records

Revision 1.361  2004/03/09 03:00:46  cheshire
<rdar://problem/3581961> Don't take lock until after mDNS_Update() has validated that the data is good.

Revision 1.360  2004/03/08 02:52:41  cheshire
Minor debugging fix: Make sure 'target' is initialized so we don't crash writing debugging log messages

Revision 1.359  2004/03/02 03:21:56  cheshire
<rdar://problem/3549576> Properly support "_services._dns-sd._udp" meta-queries

Revision 1.358  2004/02/20 08:18:34  cheshire
<rdar://problem/3564799>: mDNSResponder sometimes announces AAAA records unnecessarily

Revision 1.357  2004/02/18 01:47:41  cheshire
<rdar://problem/3553472>: Insufficient delay waiting for multi-packet KA lists causes AirPort traffic storms

Revision 1.356  2004/02/06 23:04:19  ksekar
Basic Dynamic Update support via mDNS_Register (dissabled via
UNICAST_REGISTRATION #define)

Revision 1.355  2004/02/05 09:32:33  cheshire
Fix from Bob Bradley: When using the "%.*s" string form,
guard against truncating in the middle of a multi-byte UTF-8 character.

Revision 1.354  2004/02/05 09:30:22  cheshire
Update comments

Revision 1.353  2004/01/28 03:41:00  cheshire
<rdar://problem/3541946>: Need ability to do targeted queries as well as multicast queries

Revision 1.352  2004/01/28 02:30:07  ksekar
Added default Search Domains to unicast browsing, controlled via
Networking sharing prefs pane.  Stopped sending unicast messages on
every interface.  Fixed unicast resolving via mach-port API.

Revision 1.351  2004/01/27 20:15:22  cheshire
<rdar://problem/3541288>: Time to prune obsolete code for listening on port 53

Revision 1.350  2004/01/24 23:38:16  cheshire
Use mDNSVal16() instead of shifting and ORing operations

Revision 1.349  2004/01/23 23:23:14  ksekar
Added TCP support for truncated unicast messages.

Revision 1.348  2004/01/22 03:54:11  cheshire
Create special meta-interface 'mDNSInterface_ForceMCast' (-2),
which means "do this query via multicast, even if it's apparently a unicast domain"

Revision 1.347  2004/01/22 03:50:49  cheshire
If the client has specified an explicit InterfaceID, then do query by multicast, not unicast

Revision 1.346  2004/01/22 03:48:41  cheshire
Make sure uDNS client doesn't accidentally use query ID zero

Revision 1.345  2004/01/22 03:43:08  cheshire
Export constants like mDNSInterface_LocalOnly so that the client layers can use them

Revision 1.344  2004/01/21 21:53:18  cheshire
<rdar://problem/3448144>: Don't try to receive unicast responses if we're not the first to bind to the UDP port

Revision 1.343  2003/12/23 00:07:47  cheshire
Make port number in debug message be five-character field, left justified

Revision 1.342  2003/12/20 01:34:28  cheshire
<rdar://problem/3515876>: Error putting additional records into packets
Another fix from Rampi: responseptr needs to be updated inside the "for" loop,
after every record, not once at the end.

Revision 1.341  2003/12/18 22:56:12  cheshire
<rdar://problem/3510798>: Reduce syslog messages about ignored spoof packets

Revision 1.340  2003/12/16 02:31:37  cheshire
Minor update to comments

Revision 1.339  2003/12/13 05:50:33  bradley
Fixed crash with mDNS_Lock/Unlock being called for the initial GrowCache before the platform
layer has been initialized. Protect mDNS_reentrancy when completing the core initialization to
fix a race condition during async initialization. Fixed buffer overrun for 1 byte mDNS_snprintf.

Revision 1.338  2003/12/13 03:05:27  ksekar
<rdar://problem/3192548>: DynDNS: Unicast query of service records

Revision 1.337  2003/12/01 21:46:05  cheshire
mDNS_StartQuery returns mStatus_BadInterfaceErr if the specified interface does not exist

Revision 1.336  2003/12/01 21:26:19  cheshire
Guard against zero-length sbuffer in mDNS_vsnprintf()

Revision 1.335  2003/12/01 20:27:48  cheshire
Display IPv6 addresses correctly (e.g. in log messages) on little-endian processors

Revision 1.334  2003/11/20 22:59:53  cheshire
Changed runtime checks in mDNS.c to be compile-time checks in mDNSEmbeddedAPI.h
Thanks to Bob Bradley for suggesting the ingenious compiler trick to make this work.

Revision 1.333  2003/11/20 20:49:53  cheshire
Another fix from HP: Use packedstruct macro to ensure proper packing for on-the-wire packet structures

Revision 1.332  2003/11/20 05:47:37  cheshire
<rdar://problem/3490355>: Don't exclude known answers whose expiry time is before the next query
Now that we only include answers in the known answer list if they are less than
halfway to expiry, the check to also see if we have another query scheduled
before the record expires is no longer necessary (and in fact, not correct).

Revision 1.331  2003/11/19 22:31:48  cheshire
When automatically adding A records to SRVs, add them as additionals, not answers

Revision 1.330  2003/11/19 22:28:50  cheshire
Increment/Decrement mDNS_reentrancy around calls to m->MainCallback()
to allow client to make mDNS calls (specifically the call to mDNS_GrowCache())

Revision 1.329  2003/11/19 22:19:24  cheshire
Show log message when ignoring packets with bad TTL.
This is to help diagnose problems on Linux versions that may not report the TTL reliably.

Revision 1.328  2003/11/19 22:06:38  cheshire
Show log messages when a service or hostname is renamed

Revision 1.327  2003/11/19 22:03:44  cheshire
Move common "m->NextScheduledResponse = m->timenow" to before "if" statement

Revision 1.326  2003/11/17 22:27:02  cheshire
Another fix from ramaprasad.kr@hp.com: Improve reply delay computation
on platforms that have native clock rates below fifty ticks per second.

Revision 1.325  2003/11/17 20:41:44  cheshire
Fix some missing mDNS_Lock(m)/mDNS_Unlock(m) calls.

Revision 1.324  2003/11/17 20:36:32  cheshire
Function rename: Remove "mDNS_" prefix from AdvertiseInterface() and
DeadvertiseInterface() -- they're internal private routines, not API routines.

Revision 1.323  2003/11/14 20:59:08  cheshire
Clients can't use AssignDomainName macro because mDNSPlatformMemCopy is defined in mDNSPlatformFunctions.h.
Best solution is just to combine mDNSEmbeddedAPI.h and mDNSPlatformFunctions.h into a single file.

Revision 1.322  2003/11/14 19:47:52  cheshire
Define symbol MAX_ESCAPED_DOMAIN_NAME to indicate recommended buffer size for ConvertDomainNameToCString

Revision 1.321  2003/11/14 19:18:34  cheshire
Move AssignDomainName macro to mDNSEmbeddedAPI.h to that client layers can use it too

Revision 1.320  2003/11/13 06:45:04  cheshire
Fix compiler warning on certain compilers

Revision 1.319  2003/11/13 00:47:40  cheshire
<rdar://problem/3437556> We should delay AAAA record query if A record already in cache.

Revision 1.318  2003/11/13 00:33:26  cheshire
Change macro "RRIsAddressType" to "RRTypeIsAddressType"

Revision 1.317  2003/11/13 00:10:49  cheshire
<rdar://problem/3436412>: Verify that rr data is different before updating.

Revision 1.316  2003/11/08 23:37:54  cheshire
Give explicit zero initializers to blank static structure, required by certain compilers.
(Thanks to ramaprasad.kr@hp.com for reporting this.)

Revision 1.315  2003/11/07 03:32:56  cheshire
<rdar://problem/3472153> mDNSResponder delivers answers in inconsistent order
This is the real fix. Checkin 1.312 was overly simplistic; Calling GetFreeCacheRR() can sometimes
purge records from the cache, causing tail pointer *rp to be stale on return. The correct fix is
to maintain a system-wide tail pointer for each cache slot, and then if neccesary GetFreeCacheRR()
can update this pointer, so that mDNSCoreReceiveResponse() appends records in the right place.

Revision 1.314  2003/11/07 03:19:49  cheshire
Minor variable renaming for clarity

Revision 1.313  2003/11/07 03:14:49  cheshire
Previous checkin proved to be overly simplistic; reversing

Revision 1.312  2003/11/03 23:45:15  cheshire
<rdar://problem/3472153> mDNSResponder delivers answers in inconsistent order
Build cache lists in FIFO order, not customary C LIFO order
(Append new elements to tail of cache list, instead of prepending at the head.)

Revision 1.311  2003/10/09 18:00:11  cheshire
Another compiler warning fix.

Revision 1.310  2003/10/07 20:27:05  cheshire
Patch from Bob Bradley, to fix warning and compile error on Windows

Revision 1.309  2003/09/26 01:06:36  cheshire
<rdar://problem/3427923> Set kDNSClass_UniqueRRSet bit for updates too
Made new routine HaveSentEntireRRSet() to check if flag should be set

Revision 1.308  2003/09/23 01:05:01  cheshire
Minor changes to comments and debugf() message

Revision 1.307  2003/09/09 20:13:30  cheshire
<rdar://problem/3411105> Don't send a Goodbye record if we never announced it
Ammend checkin 1.304: Off-by-one error: By this place in the function we've already decremented
rr->AnnounceCount, so the check needs to be for InitialAnnounceCount-1, not InitialAnnounceCount

Revision 1.306  2003/09/09 03:00:03  cheshire
<rdar://problem/3413099> Services take a long time to disappear when switching networks.
Added two constants: kDefaultReconfirmTimeForNoAnswer and kDefaultReconfirmTimeForCableDisconnect

Revision 1.305  2003/09/09 02:49:31  cheshire
<rdar://problem/3413975> Initial probes and queries not grouped on wake-from-sleep

Revision 1.304  2003/09/09 02:41:19  cheshire
<rdar://problem/3411105> Don't send a Goodbye record if we never announced it

Revision 1.303  2003/09/05 19:55:02  cheshire
<rdar://problem/3409533> Include address records when announcing SRV records

Revision 1.302  2003/09/05 00:01:36  cheshire
<rdar://problem/3407549> Don't accelerate queries that have large KA lists

Revision 1.301  2003/09/04 22:51:13  cheshire
<rdar://problem/3398213> Group probes and goodbyes better

Revision 1.300  2003/09/03 02:40:37  cheshire
<rdar://problem/3404842> mDNSResponder complains about '_'s
Underscores are not supposed to be legal in standard DNS names, but IANA appears
to have allowed them in previous service name registrations, so we should too.

Revision 1.299  2003/09/03 02:33:09  cheshire
<rdar://problem/3404795> CacheRecordRmv ERROR
Don't update m->NewQuestions until *after* CheckCacheExpiration();

Revision 1.298  2003/09/03 01:47:01  cheshire
<rdar://problem/3319418> Services always in a state of flux
Change mDNS_Reconfirm_internal() minimum timeout from 5 seconds to 45-60 seconds

Revision 1.297  2003/08/29 19:44:15  cheshire
<rdar://problem/3400967> Traffic reduction: Eliminate synchronized QUs when a new service appears
1. Use m->RandomQueryDelay to impose a random delay in the range 0-500ms on queries
   that already have at least one unique answer in the cache
2. For these queries, go straight to QM, skipping QU

Revision 1.296  2003/08/29 19:08:21  cheshire
<rdar://problem/3400986> Traffic reduction: Eliminate huge KA lists after wake from sleep
Known answers are no longer eligible to go in the KA list if they are more than half-way to their expiry time.

Revision 1.295  2003/08/28 01:10:59  cheshire
<rdar://problem/3396034> Add syslog message to report when query is reset because of immediate answer burst

Revision 1.294  2003/08/27 02:30:22  cheshire
<rdar://problem/3395909> Traffic Reduction: Inefficiencies in DNSServiceResolverResolve()
One more change: "query->GotTXT" is now a straightforward bi-state boolean again

Revision 1.293  2003/08/27 02:25:31  cheshire
<rdar://problem/3395909> Traffic Reduction: Inefficiencies in DNSServiceResolverResolve()

Revision 1.292  2003/08/21 19:27:36  cheshire
<rdar://problem/3387878> Traffic reduction: No need to announce record for longer than TTL

Revision 1.291  2003/08/21 18:57:44  cheshire
<rdar://problem/3387140> Synchronized queries on the network

Revision 1.290  2003/08/21 02:25:23  cheshire
Minor changes to comments and debugf() messages

Revision 1.289  2003/08/21 02:21:50  cheshire
<rdar://problem/3386473> Efficiency: Reduce repeated queries

Revision 1.288  2003/08/20 23:39:30  cheshire
<rdar://problem/3344098> Review syslog messages, and remove as appropriate

Revision 1.287  2003/08/20 20:47:18  cheshire
Fix compiler warning

Revision 1.286  2003/08/20 02:18:51  cheshire
<rdar://problem/3344098> Cleanup: Review syslog messages

Revision 1.285  2003/08/20 01:59:06  cheshire
<rdar://problem/3384478> rdatahash and rdnamehash not updated after changing rdata
Made new routine SetNewRData() to update rdlength, rdestimate, rdatahash and rdnamehash in one place

Revision 1.284  2003/08/19 22:20:00  cheshire
<rdar://problem/3376721> Don't use IPv6 on interfaces that have a routable IPv4 address configured
More minor refinements

Revision 1.283  2003/08/19 22:16:27  cheshire
Minor fix: Add missing "mDNS_Unlock(m);" in mDNS_DeregisterInterface() error case.

Revision 1.282  2003/08/19 06:48:25  cheshire
<rdar://problem/3376552> Guard against excessive record updates
Each record starts with 10 UpdateCredits.
Every update consumes one UpdateCredit.
UpdateCredits are replenished at a rate of one one per minute, up to a maximum of 10.
As the number of UpdateCredits declines, the number of announcements is similarly scaled back.
When fewer than 5 UpdateCredits remain, the first announcement is also delayed by an increasing amount.

Revision 1.281  2003/08/19 04:49:28  cheshire
<rdar://problem/3368159> Interaction between v4, v6 and dual-stack hosts not working quite right
1. A dual-stack host should only suppress its own query if it sees the same query from other hosts on BOTH IPv4 and IPv6.
2. When we see the first v4 (or first v6) member of a group, we re-trigger questions and probes on that interface.
3. When we see the last v4 (or v6) member of a group go away, we revalidate all the records received on that interface.

Revision 1.280  2003/08/19 02:33:36  cheshire
Update comments

Revision 1.279  2003/08/19 02:31:11  cheshire
<rdar://problem/3378386> mDNSResponder overenthusiastic with final expiration queries
Final expiration queries now only mark the question for sending on the particular interface
pertaining to the record that's expiring.

Revision 1.278  2003/08/18 22:53:37  cheshire
<rdar://problem/3382647> mDNSResponder divide by zero in mDNSPlatformRawTime()

Revision 1.277  2003/08/18 19:05:44  cheshire
<rdar://problem/3382423> UpdateRecord not working right
Added "newrdlength" field to hold new length of updated rdata

Revision 1.276  2003/08/16 03:39:00  cheshire
<rdar://problem/3338440> InterfaceID -1 indicates "local only"

Revision 1.275  2003/08/16 02:51:27  cheshire
<rdar://problem/3366590> mDNSResponder takes too much RPRVT
Don't try to compute namehash etc, until *after* validating the name

Revision 1.274  2003/08/16 01:12:40  cheshire
<rdar://problem/3366590> mDNSResponder takes too much RPRVT
Now that the minimum rdata object size has been reduced to 64 bytes, it is no longer safe to do a
simple C structure assignment of a domainname, because that object is defined to be 256 bytes long,
and in the process of copying it, the C compiler may run off the end of the rdata object into
unmapped memory. All assignments of domainname objects of uncertain size are now replaced with a
call to the macro AssignDomainName(), which is careful to copy only as many bytes as are valid.

Revision 1.273  2003/08/15 20:16:02  cheshire
<rdar://problem/3366590> mDNSResponder takes too much RPRVT
We want to avoid touching the rdata pages, so we don't page them in.
1. RDLength was stored with the rdata, which meant touching the page just to find the length.
   Moved this from the RData to the ResourceRecord object.
2. To avoid unnecessarily touching the rdata just to compare it,
   compute a hash of the rdata and store the hash in the ResourceRecord object.

Revision 1.272  2003/08/14 19:29:04  cheshire
<rdar://problem/3378473> Include cache records in SIGINFO output
Moved declarations of DNSTypeName() and GetRRDisplayString to mDNSEmbeddedAPI.h so daemon.c can use them

Revision 1.271  2003/08/14 02:17:05  cheshire
<rdar://problem/3375491> Split generic ResourceRecord type into two separate types: AuthRecord and CacheRecord

Revision 1.270  2003/08/13 17:07:28  ksekar
<rdar://problem/3376458>: Extra RR linked to list even if registration fails - causes crash
Added check to result of mDNS_Register() before linking extra record into list.

Revision 1.269  2003/08/12 19:56:23  cheshire
Update to APSL 2.0

Revision 1.268  2003/08/12 15:01:10  cheshire
Add comments

Revision 1.267  2003/08/12 14:59:27  cheshire
<rdar://problem/3374490> Rate-limiting blocks some legitimate responses
When setting LastMCTime also record LastMCInterface. When checking LastMCTime to determine
whether to suppress the response, also check LastMCInterface to see if it matches.

Revision 1.266  2003/08/12 12:47:16  cheshire
In mDNSCoreMachineSleep debugf message, display value of m->timenow

Revision 1.265  2003/08/11 20:04:28  cheshire
<rdar://problem/3366553> Improve efficiency by restricting cases where we have to walk the entire cache

Revision 1.264  2003/08/09 00:55:02  cheshire
<rdar://problem/3366553> mDNSResponder is taking 20-30% of the CPU
Don't scan the whole cache after every packet.

Revision 1.263  2003/08/09 00:35:29  cheshire
Moved AnswerNewQuestion() later in the file, in preparation for next checkin

Revision 1.262  2003/08/08 19:50:33  cheshire
<rdar://problem/3370332> Remove "Cache size now xxx" messages

Revision 1.261  2003/08/08 19:18:45  cheshire
<rdar://problem/3271219> Only retrigger questions on platforms with the "PhantomInterfaces" bug

Revision 1.260  2003/08/08 18:55:48  cheshire
<rdar://problem/3370365> Guard against time going backwards

Revision 1.259  2003/08/08 18:36:04  cheshire
<rdar://problem/3344154> Only need to revalidate on interface removal on platforms that have the PhantomInterfaces bug

Revision 1.258  2003/08/08 16:22:05  cheshire
<rdar://problem/3335473> Need to check validity of TXT (and other) records
Remove unneeded LogMsg

Revision 1.257  2003/08/07 01:41:08  cheshire
<rdar://problem/3367346> Ignore packets with invalid source address (all zeroes or all ones)

Revision 1.256  2003/08/06 23:25:51  cheshire
<rdar://problem/3290674> Increase TTL for A/AAAA/SRV from one minute to four

Revision 1.255  2003/08/06 23:22:50  cheshire
Add symbolic constants: kDefaultTTLforUnique (one minute) and kDefaultTTLforShared (two hours)

Revision 1.254  2003/08/06 21:33:39  cheshire
Fix compiler warnings on PocketPC 2003 (Windows CE)

Revision 1.253  2003/08/06 20:43:57  cheshire
<rdar://problem/3335473> Need to check validity of TXT (and other) records
Created ValidateDomainName() and ValidateRData(), used by mDNS_Register_internal() and mDNS_Update()

Revision 1.252  2003/08/06 20:35:47  cheshire
Enhance debugging routine GetRRDisplayString() so it can also be used to display
other RDataBody objects, not just the one currently attached the given ResourceRecord

Revision 1.251  2003/08/06 19:07:34  cheshire
<rdar://problem/3366251> mDNSResponder not inhibiting multicast responses as much as it should
Was checking LastAPTime instead of LastMCTime

Revision 1.250  2003/08/06 19:01:55  cheshire
Update comments

Revision 1.249  2003/08/06 00:13:28  cheshire
Tidy up debugf messages

Revision 1.248  2003/08/05 22:20:15  cheshire
<rdar://problem/3330324> Need to check IP TTL on responses

Revision 1.247  2003/08/05 00:56:39  cheshire
<rdar://problem/3357075> mDNSResponder sending additional records, even after precursor record suppressed

Revision 1.246  2003/08/04 19:20:49  cheshire
Add kDNSQType_ANY to list in DNSTypeName() so it can be displayed in debugging messages

Revision 1.245  2003/08/02 01:56:29  cheshire
For debugging: log message if we ever get more than one question in a truncated packet

Revision 1.244  2003/08/01 23:55:32  cheshire
Fix for compiler warnings on Windows, submitted by Bob Bradley

Revision 1.243  2003/07/25 02:26:09  cheshire
Typo: FIxed missing semicolon

Revision 1.242  2003/07/25 01:18:41  cheshire
Fix memory leak on shutdown in mDNS_Close() (detected in Windows version)

Revision 1.241  2003/07/23 21:03:42  cheshire
Only show "Found record..." debugf message in verbose mode

Revision 1.240  2003/07/23 21:01:11  cheshire
<rdar://problem/3340584> Need Nagle-style algorithm to coalesce multiple packets into one
After sending a packet, suppress further sending for the next 100ms.

Revision 1.239  2003/07/22 01:30:05  cheshire
<rdar://problem/3329099> Don't try to add the same question to the duplicate-questions list more than once

Revision 1.238  2003/07/22 00:10:20  cheshire
<rdar://problem/3337355> ConvertDomainLabelToCString() needs to escape escape characters

Revision 1.237  2003/07/19 03:23:13  cheshire
<rdar://problem/2986147> mDNSResponder needs to receive and cache larger records

Revision 1.236  2003/07/19 03:04:55  cheshire
Fix warnings; some debugf message improvements

Revision 1.235  2003/07/19 00:03:32  cheshire
<rdar://problem/3160248> ScheduleNextTask needs to be smarter after a no-op packet is received
ScheduleNextTask is quite an expensive operation.
We don't need to do all that work after receiving a no-op packet that didn't change our state.

Revision 1.234  2003/07/18 23:52:11  cheshire
To improve consistency of field naming, global search-and-replace:
NextProbeTime    -> NextScheduledProbe
NextResponseTime -> NextScheduledResponse

Revision 1.233  2003/07/18 00:29:59  cheshire
<rdar://problem/3268878> Remove mDNSResponder version from packet header and use HINFO record instead

Revision 1.232  2003/07/18 00:11:38  cheshire
Add extra case to switch statements to handle HINFO data for Get, Put and Display
(In all but GetRDLength(), this is is just a fall-through to kDNSType_TXT)

Revision 1.231  2003/07/18 00:06:37  cheshire
To make code a little easier to read in GetRDLength(), search-and-replace "rr->rdata->u." with "rd->"

Revision 1.230  2003/07/17 18:16:54  cheshire
<rdar://problem/3319418> Services always in a state of flux
In preparation for working on this, made some debugf messages a little more selective

Revision 1.229  2003/07/17 17:35:04  cheshire
<rdar://problem/3325583> Rate-limit responses, to guard against packet flooding

Revision 1.228  2003/07/16 20:50:27  cheshire
<rdar://problem/3315761> Need to implement "unicast response" request, using top bit of qclass

Revision 1.227  2003/07/16 05:01:36  cheshire
Add fields 'LargeAnswers' and 'ExpectUnicastResponse' in preparation for
<rdar://problem/3315761> Need to implement "unicast response" request, using top bit of qclass

Revision 1.226  2003/07/16 04:51:44  cheshire
Fix use of constant 'mDNSPlatformOneSecond' where it should have said 'InitialQuestionInterval'

Revision 1.225  2003/07/16 04:46:41  cheshire
Minor wording cleanup: The correct DNS term is "response", not "reply"

Revision 1.224  2003/07/16 04:39:02  cheshire
Textual cleanup (no change to functionality):
Construct "c >= 'A' && c <= 'Z'" appears in too many places; replaced with macro "mDNSIsUpperCase(c)"

Revision 1.223  2003/07/16 00:09:22  cheshire
Textual cleanup (no change to functionality):
Construct "((mDNSs32)rr->rroriginalttl * mDNSPlatformOneSecond)" appears in too many places;
replace with macro "TicksTTL(rr)"
Construct "rr->TimeRcvd + ((mDNSs32)rr->rroriginalttl * mDNSPlatformOneSecond)"
replaced with macro "RRExpireTime(rr)"

Revision 1.222  2003/07/15 23:40:46  cheshire
Function rename: UpdateDupSuppressInfo() is more accurately called ExpireDupSuppressInfo()

Revision 1.221  2003/07/15 22:17:56  cheshire
<rdar://problem/3328394> mDNSResponder is not being efficient when doing certain queries

Revision 1.220  2003/07/15 02:12:51  cheshire
Slight tidy-up of debugf messages and comments

Revision 1.219  2003/07/15 01:55:12  cheshire
<rdar://problem/3315777> Need to implement service registration with subtypes

Revision 1.218  2003/07/14 16:26:06  cheshire
<rdar://problem/3324795> Duplicate query suppression not working right
Refinement: Don't record DS information for a question in the first quarter second
right after we send it -- in the case where a question happens to be accelerated by
the maximum allowed amount, we don't want it to then be suppressed because the previous
time *we* sent that question falls (just) within the valid duplicate suppression window.

Revision 1.217  2003/07/13 04:43:53  cheshire
<rdar://problem/3325169> Services on multiple interfaces not always resolving
Minor refinement: No need to make address query broader than the original SRV query that provoked it

Revision 1.216  2003/07/13 03:13:17  cheshire
<rdar://problem/3325169> Services on multiple interfaces not always resolving
If we get an identical SRV on a second interface, convert address queries to non-specific

Revision 1.215  2003/07/13 02:28:00  cheshire
<rdar://problem/3325166> SendResponses didn't all its responses
Delete all references to RRInterfaceActive -- it's now superfluous

Revision 1.214  2003/07/13 01:47:53  cheshire
Fix one error and one warning in the Windows build

Revision 1.213  2003/07/12 04:25:48  cheshire
Fix minor signed/unsigned warnings

Revision 1.212  2003/07/12 01:59:11  cheshire
Minor changes to debugf messages

Revision 1.211  2003/07/12 01:47:01  cheshire
<rdar://problem/3324495> After name conflict, appended number should be higher than previous number

Revision 1.210  2003/07/12 01:43:28  cheshire
<rdar://problem/3324795> Duplicate query suppression not working right
The correct cutoff time for duplicate query suppression is timenow less one-half the query interval.
The code was incorrectly using the last query time plus one-half the query interval.
This was only correct in the case where query acceleration was not in effect.

Revision 1.209  2003/07/12 01:27:50  cheshire
<rdar://problem/3320079> Hostname conflict naming should not use two hyphens
Fix missing "-1" in RemoveLabelSuffix()

Revision 1.208  2003/07/11 01:32:38  cheshire
Syntactic cleanup (no change to funcationality): Now that we only have one host name,
rename field "hostname1" to "hostname", and field "RR_A1" to "RR_A".

Revision 1.207  2003/07/11 01:28:00  cheshire
<rdar://problem/3161289> No more local.arpa

Revision 1.206  2003/07/11 00:45:02  cheshire
<rdar://problem/3321909> Client should get callback confirming successful host name registration

Revision 1.205  2003/07/11 00:40:18  cheshire
Tidy up debug message in HostNameCallback()

Revision 1.204  2003/07/11 00:20:32  cheshire
<rdar://problem/3320087> mDNSResponder should log a message after 16 unsuccessful probes

Revision 1.203  2003/07/10 23:53:41  cheshire
<rdar://problem/3320079> Hostname conflict naming should not use two hyphens

Revision 1.202  2003/07/04 02:23:20  cheshire
<rdar://problem/3311955> Responder too aggressive at flushing stale data
Changed mDNSResponder to require four unanswered queries before purging a record, instead of two.

Revision 1.201  2003/07/04 01:09:41  cheshire
<rdar://problem/3315775> Need to implement subtype queries
Modified ConstructServiceName() to allow three-part service types

Revision 1.200  2003/07/03 23:55:26  cheshire
Minor change to wording of syslog warning messages

Revision 1.199  2003/07/03 23:51:13  cheshire
<rdar://problem/3315652>:	Lots of "have given xxx answers" syslog warnings
Added more detailed debugging information

Revision 1.198  2003/07/03 22:19:30  cheshire
<rdar://problem/3314346> Bug fix in 3274153 breaks TiVo
Make exception to allow _tivo_servemedia._tcp.

Revision 1.197  2003/07/02 22:33:05  cheshire
<rdar://problem/2986146> mDNSResponder needs to start with a smaller cache and then grow it as needed
Minor refinements:
When cache is exhausted, verify that rrcache_totalused == rrcache_size and report if not
Allow cache to grow to 512 records before considering it a potential denial-of-service attack

Revision 1.196  2003/07/02 21:19:45  cheshire
<rdar://problem/3313413> Update copyright notices, etc., in source code comments

Revision 1.195  2003/07/02 19:56:58  cheshire
<rdar://problem/2986146> mDNSResponder needs to start with a smaller cache and then grow it as needed
Minor refinement: m->rrcache_active was not being decremented when
an active record was deleted because its TTL expired

Revision 1.194  2003/07/02 18:47:40  cheshire
Minor wording change to log messages

Revision 1.193  2003/07/02 02:44:13  cheshire
Fix warning in non-debug build

Revision 1.192  2003/07/02 02:41:23  cheshire
<rdar://problem/2986146> mDNSResponder needs to start with a smaller cache and then grow it as needed

Revision 1.191  2003/07/02 02:30:51  cheshire
HashSlot() returns an array index. It can't be negative; hence it should not be signed.

Revision 1.190  2003/06/27 00:03:05  vlubet
<rdar://problem/3304625> Merge of build failure fix for gcc 3.3

Revision 1.189  2003/06/11 19:24:03  cheshire
<rdar://problem/3287141> Crash in SendQueries/SendResponses when no active interfaces
Slight refinement to previous checkin

Revision 1.188  2003/06/10 20:33:28  cheshire
<rdar://problem/3287141> Crash in SendQueries/SendResponses when no active interfaces

Revision 1.187  2003/06/10 04:30:44  cheshire
<rdar://problem/3286234> Need to re-probe/re-announce on configuration change
Only interface-specific records were re-probing and re-announcing, not non-specific records.

Revision 1.186  2003/06/10 04:24:39  cheshire
<rdar://problem/3283637> React when we observe other people query unsuccessfully for a record that's in our cache
Some additional refinements:
Don't try to do this for unicast-response queries
better tracking of Qs and KAs in multi-packet KA lists

Revision 1.185  2003/06/10 03:52:49  cheshire
Update comments and debug messages

Revision 1.184  2003/06/10 02:26:39  cheshire
<rdar://problem/3283516> mDNSResponder needs an mDNS_Reconfirm() function
Make mDNS_Reconfirm() call mDNS_Lock(), like the other API routines

Revision 1.183  2003/06/09 18:53:13  cheshire
Simplify some debugf() statements (replaced block of 25 lines with 2 lines)

Revision 1.182  2003/06/09 18:38:42  cheshire
<rdar://problem/3285082> Need to be more tolerant when there are mDNS proxies on the network
Only issue a correction if the TTL in the proxy packet is less than half the correct value.

Revision 1.181  2003/06/07 06:45:05  cheshire
<rdar://problem/3283666> No need for multiple machines to all be sending the same queries

Revision 1.180  2003/06/07 06:31:07  cheshire
Create little four-line helper function "FindIdenticalRecordInCache()"

Revision 1.179  2003/06/07 06:28:13  cheshire
For clarity, change name of "DNSQuestion q" to "DNSQuestion pktq"

Revision 1.178  2003/06/07 06:25:12  cheshire
Update some comments

Revision 1.177  2003/06/07 04:50:53  cheshire
<rdar://problem/3283637> React when we observe other people query unsuccessfully for a record that's in our cache

Revision 1.176  2003/06/07 04:33:26  cheshire
<rdar://problem/3283540> When query produces zero results, call mDNS_Reconfirm() on any antecedent records
Minor change: Increment/decrement logic for q->CurrentAnswers should be in
CacheRecordAdd() and CacheRecordRmv(), not AnswerQuestionWithResourceRecord()

Revision 1.175  2003/06/07 04:11:52  cheshire
Minor changes to comments and debug messages

Revision 1.174  2003/06/07 01:46:38  cheshire
<rdar://problem/3283540> When query produces zero results, call mDNS_Reconfirm() on any antecedent records

Revision 1.173  2003/06/07 01:22:13  cheshire
<rdar://problem/3283516> mDNSResponder needs an mDNS_Reconfirm() function

Revision 1.172  2003/06/07 00:59:42  cheshire
<rdar://problem/3283454> Need some randomness to spread queries on the network

Revision 1.171  2003/06/06 21:41:10  cheshire
For consistency, mDNS_StopQuery() should return an mStatus result, just like all the other mDNSCore routines

Revision 1.170  2003/06/06 21:38:55  cheshire
Renamed 'NewData' as 'FreshData' (The data may not be new data, just a refresh of data that we
already had in our cache. This refreshes our TTL on the data, but the data itself stays the same.)

Revision 1.169  2003/06/06 21:35:55  cheshire
Fix mis-named macro: GetRRHostNameTarget is really GetRRDomainNameTarget
(the target is a domain name, but not necessarily a host name)

Revision 1.168  2003/06/06 21:33:31  cheshire
Instead of using (mDNSPlatformOneSecond/2) all over the place, define a constant "InitialQuestionInterval"

Revision 1.167  2003/06/06 21:30:42  cheshire
<rdar://problem/3282962> Don't delay queries for shared record types

Revision 1.166  2003/06/06 17:20:14  cheshire
For clarity, rename question fields name/rrtype/rrclass as qname/qtype/qclass
(Global search-and-replace; no functional change to code execution.)

Revision 1.165  2003/06/04 02:53:21  cheshire
Add some "#pragma warning" lines so it compiles clean on Microsoft compilers

Revision 1.164  2003/06/04 01:25:33  cheshire
<rdar://problem/3274950> Cannot perform multi-packet known-answer suppression messages
Display time interval between first and subsequent queries

Revision 1.163  2003/06/03 19:58:14  cheshire
<rdar://problem/3277665> mDNS_DeregisterService() fixes:
When forcibly deregistering after a conflict, ensure we don't send an incorrect goodbye packet.
Guard against a couple of possible mDNS_DeregisterService() race conditions.

Revision 1.162  2003/06/03 19:30:39  cheshire
Minor addition refinements for
<rdar://problem/3277080> Duplicate registrations not handled as efficiently as they should be

Revision 1.161  2003/06/03 18:29:03  cheshire
Minor changes to comments and debugf() messages

Revision 1.160  2003/06/03 05:02:16  cheshire
<rdar://problem/3277080> Duplicate registrations not handled as efficiently as they should be

Revision 1.159  2003/06/03 03:31:57  cheshire
<rdar://problem/3277033> False self-conflict when there are duplicate registrations on one machine

Revision 1.158  2003/06/02 22:57:09  cheshire
Minor clarifying changes to comments and log messages;
IdenticalResourceRecordAnyInterface() is really more accurately called just IdenticalResourceRecord()

Revision 1.157  2003/05/31 00:09:49  cheshire
<rdar://problem/3274862> Add ability to discover what services are on a network

Revision 1.156  2003/05/30 23:56:49  cheshire
<rdar://problem/3274847> Crash after error in mDNS_RegisterService()
Need to set "sr->Extras = mDNSNULL" before returning

Revision 1.155  2003/05/30 23:48:00  cheshire
<rdar://problem/3274832> Announcements not properly grouped
Due to inconsistent setting of rr->LastAPTime at different places in the
code, announcements were not properly grouped into a single packet.
Fixed by creating a single routine called InitializeLastAPTime().

Revision 1.154  2003/05/30 23:38:14  cheshire
<rdar://problem/3274814> Fix error in IPv6 reverse-mapping PTR records
Wrote buffer[32] where it should have said buffer[64]

Revision 1.153  2003/05/30 19:10:56  cheshire
<rdar://problem/3274153> ConstructServiceName needs to be more restrictive

Revision 1.152  2003/05/29 22:39:16  cheshire
<rdar://problem/3273209> Don't truncate strings in the middle of a UTF-8 character

Revision 1.151  2003/05/29 06:35:42  cheshire
<rdar://problem/3272221> mDNSCoreReceiveResponse() purging wrong record

Revision 1.150  2003/05/29 06:25:45  cheshire
<rdar://problem/3272218> Need to call CheckCacheExpiration() *before* AnswerNewQuestion()

Revision 1.149  2003/05/29 06:18:39  cheshire
<rdar://problem/3272217> Split AnswerLocalQuestions into CacheRecordAdd and CacheRecordRmv

Revision 1.148  2003/05/29 06:11:34  cheshire
<rdar://problem/3272214> Report if there appear to be too many "Resolve" callbacks

Revision 1.147  2003/05/29 06:01:18  cheshire
Change some debugf() calls to LogMsg() calls to help with debugging

Revision 1.146  2003/05/28 21:00:44  cheshire
Re-enable "immediate answer burst" debugf message

Revision 1.145  2003/05/28 20:57:44  cheshire
<rdar://problem/3271550> mDNSResponder reports "Cannot perform multi-packet
known-answer suppression ..." This is a known issue caused by a bug in the OS X 10.2
version of mDNSResponder, so for now we should suppress this warning message.

Revision 1.144  2003/05/28 18:05:12  cheshire
<rdar://problem/3009899> mDNSResponder allows invalid service registrations
Fix silly mistake: old logic allowed "TDP" and "UCP" as valid names

Revision 1.143  2003/05/28 04:31:29  cheshire
<rdar://problem/3270733> mDNSResponder not sending probes at the prescribed time

Revision 1.142  2003/05/28 03:13:07  cheshire
<rdar://problem/3009899> mDNSResponder allows invalid service registrations
Require that the transport protocol be _udp or _tcp

Revision 1.141  2003/05/28 02:19:12  cheshire
<rdar://problem/3270634> Misleading messages generated by iChat
Better fix: Only generate the log message for queries where the TC bit is set.

Revision 1.140  2003/05/28 01:55:24  cheshire
Minor change to log messages

Revision 1.139  2003/05/28 01:52:51  cheshire
<rdar://problem/3270634> Misleading messages generated by iChat

Revision 1.138  2003/05/27 22:35:00  cheshire
<rdar://problem/3270277> mDNS_RegisterInterface needs to retrigger questions

Revision 1.137  2003/05/27 20:04:33  cheshire
<rdar://problem/3269900> mDNSResponder crash in mDNS_vsnprintf()

Revision 1.136  2003/05/27 18:50:07  cheshire
<rdar://problem/3269768> mDNS_StartResolveService doesn't inform client of port number changes

Revision 1.135  2003/05/26 04:57:28  cheshire
<rdar://problem/3268953> Delay queries when there are already answers in the cache

Revision 1.134  2003/05/26 04:54:54  cheshire
<rdar://problem/3268904> sprintf/vsprintf-style functions are unsafe; use snprintf/vsnprintf instead
Accidentally deleted '%' case from the switch statement

Revision 1.133  2003/05/26 03:21:27  cheshire
Tidy up address structure naming:
mDNSIPAddr         => mDNSv4Addr (for consistency with mDNSv6Addr)
mDNSAddr.addr.ipv4 => mDNSAddr.ip.v4
mDNSAddr.addr.ipv6 => mDNSAddr.ip.v6

Revision 1.132  2003/05/26 03:01:26  cheshire
<rdar://problem/3268904> sprintf/vsprintf-style functions are unsafe; use snprintf/vsnprintf instead

Revision 1.131  2003/05/26 00:42:05  cheshire
<rdar://problem/3268876> Temporarily include mDNSResponder version in packets

Revision 1.130  2003/05/24 16:39:48  cheshire
<rdar://problem/3268631> SendResponses also needs to handle multihoming better

Revision 1.129  2003/05/23 02:15:37  cheshire
Fixed misleading use of the term "duplicate suppression" where it should have
said "known answer suppression". (Duplicate answer suppression is something
different, and duplicate question suppression is yet another thing, so the use
of the completely vague term "duplicate suppression" was particularly bad.)

Revision 1.128  2003/05/23 01:55:13  cheshire
<rdar://problem/3267127> After name change, mDNSResponder needs to re-probe for name uniqueness

Revision 1.127  2003/05/23 01:02:15  ksekar
<rdar://problem/3032577>: mDNSResponder needs to include unique id in default name

Revision 1.126  2003/05/22 02:29:22  cheshire
<rdar://problem/2984918> SendQueries needs to handle multihoming better
Complete rewrite of SendQueries. Works much better now :-)

Revision 1.125  2003/05/22 01:50:45  cheshire
Fix warnings, and improve log messages

Revision 1.124  2003/05/22 01:41:50  cheshire
DiscardDeregistrations doesn't need InterfaceID parameter

Revision 1.123  2003/05/22 01:38:55  cheshire
Change bracketing of #pragma mark

Revision 1.122  2003/05/21 19:59:04  cheshire
<rdar://problem/3148431> ER: Tweak responder's default name conflict behavior
Minor refinements; make sure we don't truncate in the middle of a multi-byte UTF-8 character

Revision 1.121  2003/05/21 17:54:07  ksekar
<rdar://problem/3148431> ER: Tweak responder's default name conflict behavior
New rename behavior - domain name "foo" becomes "foo--2" on conflict, richtext name becomes "foo (2)"

Revision 1.120  2003/05/19 22:14:14  ksekar
<rdar://problem/3162914> mDNS probe denials/conflicts not detected unless conflict is of the same type

Revision 1.119  2003/05/16 01:34:10  cheshire
Fix some warnings

Revision 1.118  2003/05/14 18:48:40  cheshire
<rdar://problem/3159272> mDNSResponder should be smarter about reconfigurations
More minor refinements:
mDNSMacOSX.c needs to do *all* its mDNS_DeregisterInterface calls before freeing memory
mDNS_DeregisterInterface revalidates cache record when *any* representative of an interface goes away

Revision 1.117  2003/05/14 07:08:36  cheshire
<rdar://problem/3159272> mDNSResponder should be smarter about reconfigurations
Previously, when there was any network configuration change, mDNSResponder
would tear down the entire list of active interfaces and start again.
That was very disruptive, and caused the entire cache to be flushed,
and caused lots of extra network traffic. Now it only removes interfaces
that have really gone, and only adds new ones that weren't there before.

Revision 1.116  2003/05/14 06:51:56  cheshire
<rdar://problem/3027144> mDNSResponder doesn't refresh server info if changed during sleep

Revision 1.115  2003/05/14 06:44:31  cheshire
Improve debugging message

Revision 1.114  2003/05/07 01:47:03  cheshire
<rdar://problem/3250330> Also protect against NULL domainlabels

Revision 1.113  2003/05/07 00:28:18  cheshire
<rdar://problem/3250330> Need to make mDNSResponder more defensive against bad clients

Revision 1.112  2003/05/06 00:00:46  cheshire
<rdar://problem/3248914> Rationalize naming of domainname manipulation functions

Revision 1.111  2003/05/05 23:42:08  cheshire
<rdar://problem/3245631> Resolves never succeed
Was setting "rr->LastAPTime = timenow - rr->LastAPTime"
instead of  "rr->LastAPTime = timenow - rr->ThisAPInterval"

Revision 1.110  2003/04/30 21:09:59  cheshire
<rdar://problem/3244727> mDNS_vsnprintf needs to be more defensive against invalid domain names

Revision 1.109  2003/04/26 02:41:56  cheshire
<rdar://problem/3241281> Change timenow from a local variable to a structure member

Revision 1.108  2003/04/25 01:45:56  cheshire
<rdar://problem/3240002> mDNS_RegisterNoSuchService needs to include a host name

Revision 1.107  2003/04/25 00:41:31  cheshire
<rdar://problem/3239912> Create single routine PurgeCacheResourceRecord(), to avoid bugs in future

Revision 1.106  2003/04/22 03:14:45  cheshire
<rdar://problem/3232229> Include Include instrumented mDNSResponder in panther now

Revision 1.105  2003/04/22 01:07:43  cheshire
<rdar://problem/3176248> DNSServiceRegistrationUpdateRecord should support a default ttl
If TTL parameter is zero, leave record TTL unchanged

Revision 1.104  2003/04/21 19:15:52  cheshire
Fix some compiler warnings

Revision 1.103  2003/04/19 02:26:35  cheshire
<rdar://problem/3233804> Incorrect goodbye packet after conflict

Revision 1.102  2003/04/17 03:06:28  cheshire
<rdar://problem/3231321> No need to query again when a service goes away
Set UnansweredQueries to 2 when receiving a "goodbye" packet

Revision 1.101  2003/04/15 20:58:31  jgraessl
<rdar://problem/3229014> Added a hash to lookup records in the cache.

Revision 1.100  2003/04/15 18:53:14  cheshire
<rdar://problem/3229064> Bug in ScheduleNextTask
mDNS.c 1.94 incorrectly combined two "if" statements into one.

Revision 1.99  2003/04/15 18:09:13  jgraessl
<rdar://problem/3228892>
Reviewed by: Stuart Cheshire
Added code to keep track of when the next cache item will expire so we can
call TidyRRCache only when necessary.

Revision 1.98  2003/04/03 03:43:55  cheshire
<rdar://problem/3216837> Off-by-one error in probe rate limiting

Revision 1.97  2003/04/02 01:48:17  cheshire
<rdar://problem/3212360> mDNSResponder sometimes suffers false self-conflicts when it sees its own packets
Additional fix pointed out by Josh:
Also set ProbeFailTime when incrementing NumFailedProbes when resetting a record back to probing state

Revision 1.96  2003/04/01 23:58:55  cheshire
Minor comment changes

Revision 1.95  2003/04/01 23:46:05  cheshire
<rdar://problem/3214832> mDNSResponder can get stuck in infinite loop after many location cycles
mDNS_DeregisterInterface() flushes the RR cache by marking all records received on that interface
to expire in one second. However, if a mDNS_StartResolveService() call is made in that one-second
window, it can get an SRV answer from one of those soon-to-be-deleted records, resulting in
FoundServiceInfoSRV() making an interface-specific query on the interface that was just removed.

Revision 1.94  2003/03/29 01:55:19  cheshire
<rdar://problem/3212360> mDNSResponder sometimes suffers false self-conflicts when it sees its own packets
Solution: Major cleanup of packet timing and conflict handling rules

Revision 1.93  2003/03/28 01:54:36  cheshire
Minor tidyup of IPv6 (AAAA) code

Revision 1.92  2003/03/27 03:30:55  cheshire
<rdar://problem/3210018> Name conflicts not handled properly, resulting in memory corruption, and eventual crash
Problem was that HostNameCallback() was calling mDNS_DeregisterInterface(), which is not safe in a callback
Fixes:
1. Make mDNS_DeregisterInterface() safe to call from a callback
2. Make HostNameCallback() use DeadvertiseInterface() instead
   (it never really needed to deregister the interface at all)

Revision 1.91  2003/03/15 04:40:36  cheshire
Change type called "mDNSOpaqueID" to the more descriptive name "mDNSInterfaceID"

Revision 1.90  2003/03/14 20:26:37  cheshire
Reduce debugging messages (reclassify some "debugf" as "verbosedebugf")

Revision 1.89  2003/03/12 19:57:50  cheshire
Fixed typo in debug message

Revision 1.88  2003/03/12 00:17:44  cheshire
<rdar://problem/3195426> GetFreeCacheRR needs to be more willing to throw away recent records

Revision 1.87  2003/03/11 01:27:20  cheshire
Reduce debugging messages (reclassify some "debugf" as "verbosedebugf")

Revision 1.86  2003/03/06 20:44:33  cheshire
Comment tidyup

Revision 1.85  2003/03/05 03:38:35  cheshire
<rdar://problem/3185731> Bogus error message in console: died or deallocated, but no record of client can be found!
Fixed by leaving client in list after conflict, until client explicitly deallocates

Revision 1.84  2003/03/05 01:27:30  cheshire
<rdar://problem/3185482> Different TTL for multicast versus unicast responses
When building unicast responses, record TTLs are capped to 10 seconds

Revision 1.83  2003/03/04 23:48:52  cheshire
<rdar://problem/3188865> Double probes after wake from sleep
Don't reset record type to kDNSRecordTypeUnique if record is DependentOn another

Revision 1.82  2003/03/04 23:38:29  cheshire
<rdar://problem/3099194> mDNSResponder needs performance improvements
Only set rr->CRActiveQuestion to point to the
currently active representative of a question set

Revision 1.81  2003/02/21 03:35:34  cheshire
<rdar://problem/3179007> mDNSResponder needs to include AAAA records in additional answer section

Revision 1.80  2003/02/21 02:47:53  cheshire
<rdar://problem/3099194> mDNSResponder needs performance improvements
Several places in the code were calling CacheRRActive(), which searched the entire
question list every time, to see if this cache resource record answers any question.
Instead, we now have a field "CRActiveQuestion" in the resource record structure

Revision 1.79  2003/02/21 01:54:07  cheshire
<rdar://problem/3099194> mDNSResponder needs performance improvements
Switched to using new "mDNS_Execute" model (see "Implementer Notes.txt")

Revision 1.78  2003/02/20 06:48:32  cheshire
<rdar://problem/3169535> Xserve RAID needs to do interface-specific registrations
Reviewed by: Josh Graessley, Bob Bradley

Revision 1.77  2003/01/31 03:35:59  cheshire
<rdar://problem/3147097> mDNSResponder sometimes fails to find the correct results
When there were *two* active questions in the list, they were incorrectly
finding *each other* and *both* being marked as duplicates of another question

Revision 1.76  2003/01/29 02:46:37  cheshire
Fix for IPv6:
A physical interface is identified solely by its InterfaceID (not by IP and type).
On a given InterfaceID, mDNSCore may send both v4 and v6 multicasts.
In cases where the requested outbound protocol (v4 or v6) is not supported on
that InterfaceID, the platform support layer should simply discard that packet.

Revision 1.75  2003/01/29 01:47:40  cheshire
Rename 'Active' to 'CRActive' or 'InterfaceActive' for improved clarity

Revision 1.74  2003/01/28 05:26:25  cheshire
<rdar://problem/3147097> mDNSResponder sometimes fails to find the correct results
Add 'Active' flag for interfaces

Revision 1.73  2003/01/28 03:45:12  cheshire
Fixed missing "not" in "!mDNSAddrIsDNSMulticast(dstaddr)"

Revision 1.72  2003/01/28 01:49:48  cheshire
<rdar://problem/3147097> mDNSResponder sometimes fails to find the correct results
FindDuplicateQuestion() was incorrectly finding the question itself in the list,
and incorrectly marking it as a duplicate (of itself), so that it became inactive.

Revision 1.71  2003/01/28 01:41:44  cheshire
<rdar://problem/3153091> Race condition when network change causes bad stuff
When an interface goes away, interface-specific questions on that interface become orphaned.
Orphan questions cause HaveQueries to return true, but there's no interface to send them on.
Fix: mDNS_DeregisterInterface() now calls DeActivateInterfaceQuestions()

Revision 1.70  2003/01/23 19:00:20  cheshire
Protect against infinite loops in mDNS_Execute

Revision 1.69  2003/01/21 22:56:32  jgraessl
<rdar://problem/3124348>  service name changes are not properly handled
Submitted by: Stuart Cheshire
Reviewed by: Joshua Graessley
Applying changes for 3124348 to main branch. 3124348 changes went in to a
branch for SU.

Revision 1.68  2003/01/17 04:09:27  cheshire
<rdar://problem/3141038> mDNSResponder Resolves are unreliable on multi-homed hosts

Revision 1.67  2003/01/17 03:56:45  cheshire
Default 24-hour TTL is far too long. Changing to two hours.

Revision 1.66  2003/01/13 23:49:41  jgraessl
Merged changes for the following fixes in to top of tree:
<rdar://problem/3086540>  computer name changes not handled properly
<rdar://problem/3124348>  service name changes are not properly handled
<rdar://problem/3124352>  announcements sent in pairs, failing chattiness test

Revision 1.65  2002/12/23 22:13:28  jgraessl
Reviewed by: Stuart Cheshire
Initial IPv6 support for mDNSResponder.

Revision 1.64  2002/11/26 20:49:06  cheshire
<rdar://problem/3104543> RFC 1123 allows the first character of a name label to be either a letter or a digit

Revision 1.63  2002/09/21 20:44:49  zarzycki
Added APSL info

Revision 1.62  2002/09/20 03:25:37  cheshire
Fix some compiler warnings

Revision 1.61  2002/09/20 01:05:24  cheshire
Don't kill the Extras list in mDNS_DeregisterService()

Revision 1.60  2002/09/19 23:47:35  cheshire
Added mDNS_RegisterNoSuchService() function for assertion of non-existence
of a particular named service

Revision 1.59  2002/09/19 21:25:34  cheshire
mDNS_snprintf() doesn't need to be in a separate file

Revision 1.58  2002/09/19 04:20:43  cheshire
Remove high-ascii characters that confuse some systems

Revision 1.57  2002/09/17 01:07:08  cheshire
Change mDNS_AdvertiseLocalAddresses to be a parameter to mDNS_Init()

Revision 1.56  2002/09/16 19:44:17  cheshire
Merge in license terms from Quinn's copy, in preparation for Darwin release
*/

#include "DNSCommon.h"                  // Defines general DNS untility routines
#include "uDNS.h"						// Defines entry points into unicast-specific routines
// Disable certain benign warnings with Microsoft compilers
#if(defined(_MSC_VER))
	// Disable "conditional expression is constant" warning for debug macros.
	// Otherwise, this generates warnings for the perfectly natural construct "while(1)"
	// If someone knows a variant way of writing "while(1)" that doesn't generate warning messages, please let us know
	#pragma warning(disable:4127)
	
	// Disable "assignment within conditional expression".
	// Other compilers understand the convention that if you place the assignment expression within an extra pair
	// of parentheses, this signals to the compiler that you really intended an assignment and no warning is necessary.
	// The Microsoft compiler doesn't understand this convention, so in the absense of any other way to signal
	// to the compiler that the assignment is intentional, we have to just turn this warning off completely.
	#pragma warning(disable:4706)
#endif

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Program Constants
#endif

mDNSexport const mDNSIPPort      zeroIPPort        = { { 0 } };
mDNSexport const mDNSv4Addr      zerov4Addr        = { { 0 } };
mDNSexport const mDNSv6Addr      zerov6Addr        = { { 0 } };
mDNSexport const mDNSEthAddr     zeroEthAddr       = { { 0 } };
mDNSexport const mDNSv4Addr      onesIPv4Addr      = { { 255, 255, 255, 255 } };
mDNSexport const mDNSv6Addr      onesIPv6Addr      = { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 } };
mDNSexport const mDNSAddr        zeroAddr          = { mDNSAddrType_None, {{{ 0 }}} };

mDNSexport const mDNSInterfaceID mDNSInterface_Any        = 0;
mDNSexport const mDNSInterfaceID mDNSInterface_LocalOnly  = (mDNSInterfaceID)1;

mDNSlocal  const mDNSInterfaceID mDNSInterfaceMark        = (mDNSInterfaceID)~0;

#define UnicastDNSPortAsNumber   53
#define NATPMPPortAsNumber       5351
#define DNSEXTPortAsNumber       5352		// Port used for end-to-end DNS operations like LLQ, Updates with Leases, etc.
#define MulticastDNSPortAsNumber 5353
#define LoopbackIPCPortAsNumber  5354

mDNSexport const mDNSIPPort UnicastDNSPort     = { { UnicastDNSPortAsNumber   >> 8, UnicastDNSPortAsNumber   & 0xFF } };
mDNSexport const mDNSIPPort NATPMPPort         = { { NATPMPPortAsNumber       >> 8, NATPMPPortAsNumber       & 0xFF } };
mDNSexport const mDNSIPPort DNSEXTPort         = { { DNSEXTPortAsNumber       >> 8, DNSEXTPortAsNumber       & 0xFF } };
mDNSexport const mDNSIPPort MulticastDNSPort   = { { MulticastDNSPortAsNumber >> 8, MulticastDNSPortAsNumber & 0xFF } };
mDNSexport const mDNSIPPort LoopbackIPCPort    = { { LoopbackIPCPortAsNumber  >> 8, LoopbackIPCPortAsNumber  & 0xFF } };

mDNSexport const mDNSv4Addr AllDNSAdminGroup   = { { 239, 255, 255, 251 } };
mDNSexport const mDNSAddr   AllDNSLinkGroup_v4 = { mDNSAddrType_IPv4, { { { 224,   0,   0, 251 } } } };
mDNSexport const mDNSAddr   AllDNSLinkGroup_v6 = { mDNSAddrType_IPv6, { { { 0xFF,0x02,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0xFB } } } };

mDNSexport const mDNSOpaque16 zeroID          = { { 0, 0 } };
mDNSexport const mDNSOpaque16 QueryFlags      = { { kDNSFlag0_QR_Query    | kDNSFlag0_OP_StdQuery,                0 } };
mDNSexport const mDNSOpaque16 uQueryFlags     = { { kDNSFlag0_QR_Query    | kDNSFlag0_OP_StdQuery | kDNSFlag0_RD, 0 } };
mDNSexport const mDNSOpaque16 ResponseFlags   = { { kDNSFlag0_QR_Response | kDNSFlag0_OP_StdQuery | kDNSFlag0_AA, 0 } };
mDNSexport const mDNSOpaque16 UpdateReqFlags  = { { kDNSFlag0_QR_Query    | kDNSFlag0_OP_Update,                  0 } };
mDNSexport const mDNSOpaque16 UpdateRespFlags = { { kDNSFlag0_QR_Response | kDNSFlag0_OP_Update,                  0 } };

// Any records bigger than this are considered 'large' records
#define SmallRecordLimit 1024

#define kMaxUpdateCredits 10
#define kUpdateCreditRefreshInterval (mDNSPlatformOneSecond * 6)

mDNSexport const char *const mDNS_DomainTypeNames[] =
	{
	 "b._dns-sd._udp.",		// Browse
	"db._dns-sd._udp.",		// Default Browse
	"lb._dns-sd._udp.",		// Legacy Browse
	 "r._dns-sd._udp.",		// Registration
	"dr._dns-sd._udp."		// Default Registration
	};

#ifdef UNICAST_DISABLED
#define uDNS_IsActiveQuery(q, u) mDNSfalse
#endif

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Specialized mDNS version of vsnprintf
#endif

static const struct mDNSprintf_format
	{
	unsigned      leftJustify : 1;
	unsigned      forceSign : 1;
	unsigned      zeroPad : 1;
	unsigned      havePrecision : 1;
	unsigned      hSize : 1;
	unsigned      lSize : 1;
	char          altForm;
	char          sign;		// +, - or space
	unsigned int  fieldWidth;
	unsigned int  precision;
	} mDNSprintf_format_default = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

mDNSexport mDNSu32 mDNS_vsnprintf(char *sbuffer, mDNSu32 buflen, const char *fmt, va_list arg)
	{
	mDNSu32 nwritten = 0;
	int c;
	if (buflen == 0) return(0);
	buflen--;		// Pre-reserve one space in the buffer for the terminating null
	if (buflen == 0) goto exit;

	for (c = *fmt; c != 0; c = *++fmt)
		{
		if (c != '%')
			{
			*sbuffer++ = (char)c;
			if (++nwritten >= buflen) goto exit;
			}
		else
			{
			unsigned int i=0, j;
			// The mDNS Vsprintf Argument Conversion Buffer is used as a temporary holding area for
			// generating decimal numbers, hexdecimal numbers, IP addresses, domain name strings, etc.
			// The size needs to be enough for a 256-byte domain name plus some error text.
			#define mDNS_VACB_Size 300
			char mDNS_VACB[mDNS_VACB_Size];
			#define mDNS_VACB_Lim (&mDNS_VACB[mDNS_VACB_Size])
			#define mDNS_VACB_Remain(s) ((mDNSu32)(mDNS_VACB_Lim - s))
			char *s = mDNS_VACB_Lim, *digits;
			struct mDNSprintf_format F = mDNSprintf_format_default;
	
			while (1)	//  decode flags
				{
				c = *++fmt;
				if      (c == '-') F.leftJustify = 1;
				else if (c == '+') F.forceSign = 1;
				else if (c == ' ') F.sign = ' ';
				else if (c == '#') F.altForm++;
				else if (c == '0') F.zeroPad = 1;
				else break;
				}
	
			if (c == '*')	//  decode field width
				{
				int f = va_arg(arg, int);
				if (f < 0) { f = -f; F.leftJustify = 1; }
				F.fieldWidth = (unsigned int)f;
				c = *++fmt;
				}
			else
				{
				for (; c >= '0' && c <= '9'; c = *++fmt)
					F.fieldWidth = (10 * F.fieldWidth) + (c - '0');
				}
	
			if (c == '.')	//  decode precision
				{
				if ((c = *++fmt) == '*')
					{ F.precision = va_arg(arg, unsigned int); c = *++fmt; }
				else for (; c >= '0' && c <= '9'; c = *++fmt)
						F.precision = (10 * F.precision) + (c - '0');
				F.havePrecision = 1;
				}
	
			if (F.leftJustify) F.zeroPad = 0;
	
			conv:
			switch (c)	//  perform appropriate conversion
				{
				unsigned long n;
				case 'h' :	F.hSize = 1; c = *++fmt; goto conv;
				case 'l' :	// fall through
				case 'L' :	F.lSize = 1; c = *++fmt; goto conv;
				case 'd' :
				case 'i' :	if (F.lSize) n = (unsigned long)va_arg(arg, long);
							else n = (unsigned long)va_arg(arg, int);
							if (F.hSize) n = (short) n;
							if ((long) n < 0) { n = (unsigned long)-(long)n; F.sign = '-'; }
							else if (F.forceSign) F.sign = '+';
							goto decimal;
				case 'u' :	if (F.lSize) n = va_arg(arg, unsigned long);
							else n = va_arg(arg, unsigned int);
							if (F.hSize) n = (unsigned short) n;
							F.sign = 0;
							goto decimal;
				decimal:	if (!F.havePrecision)
								{
								if (F.zeroPad)
									{
									F.precision = F.fieldWidth;
									if (F.sign) --F.precision;
									}
								if (F.precision < 1) F.precision = 1;
								}
							if (F.precision > mDNS_VACB_Size - 1)
								F.precision = mDNS_VACB_Size - 1;
							for (i = 0; n; n /= 10, i++) *--s = (char)(n % 10 + '0');
							for (; i < F.precision; i++) *--s = '0';
							if (F.sign) { *--s = F.sign; i++; }
							break;
	
				case 'o' :	if (F.lSize) n = va_arg(arg, unsigned long);
							else n = va_arg(arg, unsigned int);
							if (F.hSize) n = (unsigned short) n;
							if (!F.havePrecision)
								{
								if (F.zeroPad) F.precision = F.fieldWidth;
								if (F.precision < 1) F.precision = 1;
								}
							if (F.precision > mDNS_VACB_Size - 1)
								F.precision = mDNS_VACB_Size - 1;
							for (i = 0; n; n /= 8, i++) *--s = (char)(n % 8 + '0');
							if (F.altForm && i && *s != '0') { *--s = '0'; i++; }
							for (; i < F.precision; i++) *--s = '0';
							break;
	
				case 'a' :	{
							unsigned char *a = va_arg(arg, unsigned char *);
							if (!a) { static char emsg[] = "<<NULL>>"; s = emsg; i = sizeof(emsg)-1; }
							else
								{
								s = mDNS_VACB;	// Adjust s to point to the start of the buffer, not the end
								if (F.altForm)
									{
									mDNSAddr *ip = (mDNSAddr*)a;
									switch (ip->type)
										{
										case mDNSAddrType_IPv4: F.precision =  4; a = (unsigned char *)&ip->ip.v4; break;
										case mDNSAddrType_IPv6: F.precision = 16; a = (unsigned char *)&ip->ip.v6; break;
										default:                F.precision =  0; break;
										}
									}
								switch (F.precision)
									{
									case  4: i = mDNS_snprintf(mDNS_VACB, sizeof(mDNS_VACB), "%d.%d.%d.%d",
														a[0], a[1], a[2], a[3]); break;
									case  6: i = mDNS_snprintf(mDNS_VACB, sizeof(mDNS_VACB), "%02X:%02X:%02X:%02X:%02X:%02X",
														a[0], a[1], a[2], a[3], a[4], a[5]); break;
									case 16: i = mDNS_snprintf(mDNS_VACB, sizeof(mDNS_VACB),
														"%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X",
														a[0x0], a[0x1], a[0x2], a[0x3], a[0x4], a[0x5], a[0x6], a[0x7],
														a[0x8], a[0x9], a[0xA], a[0xB], a[0xC], a[0xD], a[0xE], a[0xF]); break;
									default: i = mDNS_snprintf(mDNS_VACB, sizeof(mDNS_VACB), "%s", "<< ERROR: Must specify"
														" address size (i.e. %.4a=IPv4, %.6a=Ethernet, %.16a=IPv6) >>"); break;
									}
								}
							}
							break;
	
				case 'p' :	F.havePrecision = F.lSize = 1;
							F.precision = 8;
				case 'X' :	digits = "0123456789ABCDEF";
							goto hexadecimal;
				case 'x' :	digits = "0123456789abcdef";
				hexadecimal:if (F.lSize) n = va_arg(arg, unsigned long);
							else n = va_arg(arg, unsigned int);
							if (F.hSize) n = (unsigned short) n;
							if (!F.havePrecision)
								{
								if (F.zeroPad)
									{
									F.precision = F.fieldWidth;
									if (F.altForm) F.precision -= 2;
									}
								if (F.precision < 1) F.precision = 1;
								}
							if (F.precision > mDNS_VACB_Size - 1)
								F.precision = mDNS_VACB_Size - 1;
							for (i = 0; n; n /= 16, i++) *--s = digits[n % 16];
							for (; i < F.precision; i++) *--s = '0';
							if (F.altForm) { *--s = (char)c; *--s = '0'; i += 2; }
							break;
	
				case 'c' :	*--s = (char)va_arg(arg, int); i = 1; break;
	
				case 's' :	s = va_arg(arg, char *);
							if (!s) { static char emsg[] = "<<NULL>>"; s = emsg; i = sizeof(emsg)-1; }
							else switch (F.altForm)
								{
								case 0: i=0;
										if (!F.havePrecision)				// C string
											while(s[i]) i++;
										else
											{
											while ((i < F.precision) && s[i]) i++;
											// Make sure we don't truncate in the middle of a UTF-8 character
											// If last character we got was any kind of UTF-8 multi-byte character,
											// then see if we have to back up.
											// This is not as easy as the similar checks below, because
											// here we can't assume it's safe to examine the *next* byte, so we
											// have to confine ourselves to working only backwards in the string.
											j = i;		// Record where we got to
											// Now, back up until we find first non-continuation-char
											while (i>0 && (s[i-1] & 0xC0) == 0x80) i--;
											// Now s[i-1] is the first non-continuation-char
											// and (j-i) is the number of continuation-chars we found
											if (i>0 && (s[i-1] & 0xC0) == 0xC0)	// If we found a start-char
												{
												i--;		// Tentatively eliminate this start-char as well
												// Now (j-i) is the number of characters we're considering eliminating.
												// To be legal UTF-8, the start-char must contain (j-i) one-bits,
												// followed by a zero bit. If we shift it right by (7-(j-i)) bits
												// (with sign extension) then the result has to be 0xFE.
												// If this is right, then we reinstate the tentatively eliminated bytes.
												if (((j-i) < 7) && (((s[i] >> (7-(j-i))) & 0xFF) == 0xFE)) i = j;
												}
											}
										break;
								case 1: i = (unsigned char) *s++; break;	// Pascal string
								case 2: {									// DNS label-sequence name
										unsigned char *a = (unsigned char *)s;
										s = mDNS_VACB;	// Adjust s to point to the start of the buffer, not the end
										if (*a == 0) *s++ = '.';	// Special case for root DNS name
										while (*a)
											{
											if (*a > 63)
												{ s += mDNS_snprintf(s, mDNS_VACB_Remain(s), "<<INVALID LABEL LENGTH %u>>", *a); break; }
											if (s + *a >= &mDNS_VACB[254])
												{ s += mDNS_snprintf(s, mDNS_VACB_Remain(s), "<<NAME TOO LONG>>"); break; }
											s += mDNS_snprintf(s, mDNS_VACB_Remain(s), "%#s.", a);
											a += 1 + *a;
											}
										i = (mDNSu32)(s - mDNS_VACB);
										s = mDNS_VACB;	// Reset s back to the start of the buffer
										break;
										}
								}
							// Make sure we don't truncate in the middle of a UTF-8 character (see similar comment below)
							if (F.havePrecision && i > F.precision)
								{ i = F.precision; while (i>0 && (s[i] & 0xC0) == 0x80) i--; }
							break;
	
				case 'n' :	s = va_arg(arg, char *);
							if      (F.hSize) * (short *) s = (short)nwritten;
							else if (F.lSize) * (long  *) s = (long)nwritten;
							else              * (int   *) s = (int)nwritten;
							continue;
	
				default:	s = mDNS_VACB;
							i = mDNS_snprintf(mDNS_VACB, sizeof(mDNS_VACB), "<<UNKNOWN FORMAT CONVERSION CODE %%%c>>", c);

				case '%' :	*sbuffer++ = (char)c;
							if (++nwritten >= buflen) goto exit;
							break;
				}
	
			if (i < F.fieldWidth && !F.leftJustify)			// Pad on the left
				do	{
					*sbuffer++ = ' ';
					if (++nwritten >= buflen) goto exit;
					} while (i < --F.fieldWidth);
	
			// Make sure we don't truncate in the middle of a UTF-8 character.
			// Note: s[i] is the first eliminated character; i.e. the next character *after* the last character of the
			// allowed output. If s[i] is a UTF-8 continuation character, then we've cut a unicode character in half,
			// so back up 'i' until s[i] is no longer a UTF-8 continuation character. (if the input was proprly
			// formed, s[i] will now be the UTF-8 start character of the multi-byte character we just eliminated).
			if (i > buflen - nwritten)
				{ i = buflen - nwritten; while (i>0 && (s[i] & 0xC0) == 0x80) i--; }
			for (j=0; j<i; j++) *sbuffer++ = *s++;			// Write the converted result
			nwritten += i;
			if (nwritten >= buflen) goto exit;
	
			for (; i < F.fieldWidth; i++)					// Pad on the right
				{
				*sbuffer++ = ' ';
				if (++nwritten >= buflen) goto exit;
				}
			}
		}
	exit:
	*sbuffer++ = 0;
	return(nwritten);
	}

mDNSexport mDNSu32 mDNS_snprintf(char *sbuffer, mDNSu32 buflen, const char *fmt, ...)
	{
	mDNSu32 length;
	
	va_list ptr;
	va_start(ptr,fmt);
	length = mDNS_vsnprintf(sbuffer, buflen, fmt, ptr);
	va_end(ptr);
	
	return(length);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - General Utility Functions
#endif

#define InitialQuestionInterval (mDNSPlatformOneSecond/2)
#define ActiveQuestion(Q) ((Q)->ThisQInterval > 0 && !(Q)->DuplicateOf)
#define TimeToSendThisQuestion(Q,time) (ActiveQuestion(Q) && (time) - ((Q)->LastQTime + (Q)->ThisQInterval) >= 0)

mDNSlocal void SetNextQueryTime(mDNS *const m, const DNSQuestion *const q)
	{
	if (ActiveQuestion(q))
		if (m->NextScheduledQuery - (q->LastQTime + q->ThisQInterval) > 0)
			m->NextScheduledQuery = (q->LastQTime + q->ThisQInterval);
	}

mDNSlocal CacheGroup *CacheGroupForName(const mDNS *const m, const mDNSu32 slot, const mDNSu32 namehash, const domainname *const name)
	{
	CacheGroup *cg;
	for (cg = m->rrcache_hash[slot]; cg; cg=cg->next)
		if (cg->namehash == namehash && SameDomainName(cg->name, name))
			break;
	return(cg);
	}

mDNSlocal CacheGroup *CacheGroupForRecord(const mDNS *const m, const mDNSu32 slot, const ResourceRecord *const rr)
	{
	return(CacheGroupForName(m, slot, rr->namehash, rr->name));
	}

mDNSlocal mDNSBool AddressIsLocalSubnet(mDNS *const m, const mDNSInterfaceID InterfaceID, const mDNSAddr *addr)
	{
	NetworkInterfaceInfo *intf;

	if (addr->type == mDNSAddrType_IPv4)
		{
		if (addr->ip.v4.b[0] == 169 && addr->ip.v4.b[1] == 254) return(mDNStrue);
		for (intf = m->HostInterfaces; intf; intf = intf->next)
			if (intf->ip.type == addr->type && intf->InterfaceID == InterfaceID && intf->McastTxRx)
				if (((intf->ip.ip.v4.NotAnInteger ^ addr->ip.v4.NotAnInteger) & intf->mask.ip.v4.NotAnInteger) == 0)
					return(mDNStrue);
		}

	if (addr->type == mDNSAddrType_IPv6)
		{
		if (addr->ip.v6.b[0] == 0xFE && addr->ip.v6.b[1] == 0x80) return(mDNStrue);
		for (intf = m->HostInterfaces; intf; intf = intf->next)
			if (intf->ip.type == addr->type && intf->InterfaceID == InterfaceID && intf->McastTxRx)
				if ((((intf->ip.ip.v6.l[0] ^ addr->ip.v6.l[0]) & intf->mask.ip.v6.l[0]) == 0) &&
					(((intf->ip.ip.v6.l[1] ^ addr->ip.v6.l[1]) & intf->mask.ip.v6.l[1]) == 0) &&
					(((intf->ip.ip.v6.l[2] ^ addr->ip.v6.l[2]) & intf->mask.ip.v6.l[2]) == 0) &&
					(((intf->ip.ip.v6.l[3] ^ addr->ip.v6.l[3]) & intf->mask.ip.v6.l[3]) == 0))
						return(mDNStrue);
		}

	return(mDNSfalse);
	}

// Set up a AuthRecord with sensible default values.
// These defaults may be overwritten with new values before mDNS_Register is called
mDNSexport void mDNS_SetupResourceRecord(AuthRecord *rr, RData *RDataStorage, mDNSInterfaceID InterfaceID,
	mDNSu16 rrtype, mDNSu32 ttl, mDNSu8 RecordType, mDNSRecordCallback Callback, void *Context)
	{
	mDNSPlatformMemZero(&rr->uDNS_info, sizeof(uDNS_RegInfo));
	// Don't try to store a TTL bigger than we can represent in platform time units
	if (ttl > 0x7FFFFFFFUL / mDNSPlatformOneSecond)
		ttl = 0x7FFFFFFFUL / mDNSPlatformOneSecond;
	else if (ttl == 0)		// And Zero TTL is illegal
		ttl = DefaultTTLforRRType(rrtype);

	// Field Group 1: The actual information pertaining to this resource record
	rr->resrec.RecordType        = RecordType;
	rr->resrec.InterfaceID       = InterfaceID;
	rr->resrec.name              = &rr->namestorage;
	rr->resrec.rrtype            = rrtype;
	rr->resrec.rrclass           = kDNSClass_IN;
	rr->resrec.rroriginalttl     = ttl;
//	rr->resrec.rdlength          = MUST set by client and/or in mDNS_Register_internal
//	rr->resrec.rdestimate        = set in mDNS_Register_internal
//	rr->resrec.rdata             = MUST be set by client

	if (RDataStorage)
		rr->resrec.rdata = RDataStorage;
	else
		{
		rr->resrec.rdata = &rr->rdatastorage;
		rr->resrec.rdata->MaxRDLength = sizeof(RDataBody);
		}

	// Field Group 2: Persistent metadata for Authoritative Records
	rr->Additional1       = mDNSNULL;
	rr->Additional2       = mDNSNULL;
	rr->DependentOn       = mDNSNULL;
	rr->RRSet             = mDNSNULL;
	rr->RecordCallback    = Callback;
	rr->RecordContext     = Context;

	rr->HostTarget        = mDNSfalse;
	rr->AllowRemoteQuery  = mDNSfalse;
	rr->ForceMCast        = mDNSfalse;

	// Field Group 3: Transient state for Authoritative Records (set in mDNS_Register_internal)
	
	rr->namestorage.c[0]  = 0;		// MUST be set by client before calling mDNS_Register()
	}

// For a single given DNSQuestion, deliver an add/remove result for the single given AuthRecord
// Used by AnswerLocalQuestions() and AnswerNewLocalOnlyQuestion()
mDNSlocal void AnswerLocalOnlyQuestionWithResourceRecord(mDNS *const m, DNSQuestion *q, AuthRecord *rr, mDNSBool AddRecord)
	{
	// Indicate that we've given at least one positive answer for this record, so we should be prepared to send a goodbye for it
	if (AddRecord) rr->LocalAnswer = mDNStrue;
	m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
	if (q->QuestionCallback)
		q->QuestionCallback(m, q, &rr->resrec, AddRecord);
	m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
	}

// When a new local AuthRecord is created or deleted, AnswerLocalQuestions() runs though our LocalOnlyQuestions delivering answers
// to each, stopping if it reaches a NewLocalOnlyQuestion -- brand-new questions are handled by AnswerNewLocalOnlyQuestion().
// If the AuthRecord is marked mDNSInterface_LocalOnly, then we also deliver it to any other questions we have using mDNSInterface_Any.
// Used by AnswerForNewLocalRecords() and mDNS_Deregister_internal()
mDNSlocal void AnswerLocalQuestions(mDNS *const m, AuthRecord *rr, mDNSBool AddRecord)
	{
	if (m->CurrentQuestion) LogMsg("AnswerLocalQuestions ERROR m->CurrentQuestion already set");

	m->CurrentQuestion = m->LocalOnlyQuestions;
	while (m->CurrentQuestion && m->CurrentQuestion != m->NewLocalOnlyQuestions)
		{
		DNSQuestion *q = m->CurrentQuestion;
		m->CurrentQuestion = q->next;
		if (ResourceRecordAnswersQuestion(&rr->resrec, q))
			AnswerLocalOnlyQuestionWithResourceRecord(m, q, rr, AddRecord);			// MUST NOT dereference q again
		}

	// If this AuthRecord is marked LocalOnly, then we want to deliver it to all local 'mDNSInterface_Any' questions
	if (rr->resrec.InterfaceID == mDNSInterface_LocalOnly)
		{
		m->CurrentQuestion = m->Questions;
		while (m->CurrentQuestion && m->CurrentQuestion != m->NewQuestions)
			{
			DNSQuestion *q = m->CurrentQuestion;
			m->CurrentQuestion = q->next;
			if (ResourceRecordAnswersQuestion(&rr->resrec, q))
				AnswerLocalOnlyQuestionWithResourceRecord(m, q, rr, AddRecord);		// MUST NOT dereference q again
			}
		}

	m->CurrentQuestion = mDNSNULL;
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Resource Record Utility Functions
#endif

#define RRTypeIsAddressType(T) ((T) == kDNSType_A || (T) == kDNSType_AAAA)

#define ResourceRecordIsValidAnswer(RR) ( ((RR)->             resrec.RecordType & kDNSRecordTypeActiveMask)  && \
		((RR)->Additional1 == mDNSNULL || ((RR)->Additional1->resrec.RecordType & kDNSRecordTypeActiveMask)) && \
		((RR)->Additional2 == mDNSNULL || ((RR)->Additional2->resrec.RecordType & kDNSRecordTypeActiveMask)) && \
		((RR)->DependentOn == mDNSNULL || ((RR)->DependentOn->resrec.RecordType & kDNSRecordTypeActiveMask))  )

#define ResourceRecordIsValidInterfaceAnswer(RR, INTID) \
	(ResourceRecordIsValidAnswer(RR) && \
	((RR)->resrec.InterfaceID == mDNSInterface_Any || (RR)->resrec.InterfaceID == (INTID)))

#define DefaultProbeCountForTypeUnique ((mDNSu8)3)
#define DefaultProbeCountForRecordType(X)      ((X) == kDNSRecordTypeUnique ? DefaultProbeCountForTypeUnique : (mDNSu8)0)

#define InitialAnnounceCount ((mDNSu8)10)

// Note that the announce intervals use exponential backoff, doubling each time. The probe intervals do not.
// This means that because the announce interval is doubled after sending the first packet, the first
// observed on-the-wire inter-packet interval between announcements is actually one second.
// The half-second value here may be thought of as a conceptual (non-existent) half-second delay *before* the first packet is sent.
#define DefaultProbeIntervalForTypeUnique (mDNSPlatformOneSecond/4)
#define DefaultAnnounceIntervalForTypeShared (mDNSPlatformOneSecond/2)
#define DefaultAnnounceIntervalForTypeUnique (mDNSPlatformOneSecond/2)

#define DefaultAPIntervalForRecordType(X)  ((X) & (kDNSRecordTypeAdvisory | kDNSRecordTypeShared     ) ? DefaultAnnounceIntervalForTypeShared : \
											(X) & (kDNSRecordTypeUnique                              ) ? DefaultProbeIntervalForTypeUnique    : \
											(X) & (kDNSRecordTypeVerified | kDNSRecordTypeKnownUnique) ? DefaultAnnounceIntervalForTypeUnique : 0)

#define TimeToAnnounceThisRecord(RR,time) ((RR)->AnnounceCount && (time) - ((RR)->LastAPTime + (RR)->ThisAPInterval) >= 0)
#define TimeToSendThisRecord(RR,time) ((TimeToAnnounceThisRecord(RR,time) || (RR)->ImmedAnswer) && ResourceRecordIsValidAnswer(RR))
#define TicksTTL(RR) ((mDNSs32)(RR)->resrec.rroriginalttl * mDNSPlatformOneSecond)
#define RRExpireTime(RR) ((RR)->TimeRcvd + TicksTTL(RR))

#define MaxUnansweredQueries 4

// SameResourceRecordSignature returns true if two resources records have the same name, type, and class, and may be sent
// (or were received) on the same interface (i.e. if *both* records specify an interface, then it has to match).
// TTL and rdata may differ.
// This is used for cache flush management:
// When sending a unique record, all other records matching "SameResourceRecordSignature" must also be sent
// When receiving a unique record, all old cache records matching "SameResourceRecordSignature" are flushed
mDNSlocal mDNSBool SameResourceRecordSignature(const ResourceRecord *const r1, const ResourceRecord *const r2)
	{
	if (!r1) { LogMsg("SameResourceRecordSignature ERROR: r1 is NULL"); return(mDNSfalse); }
	if (!r2) { LogMsg("SameResourceRecordSignature ERROR: r2 is NULL"); return(mDNSfalse); }
	if (r1->InterfaceID &&
		r2->InterfaceID &&
		r1->InterfaceID != r2->InterfaceID) return(mDNSfalse);
	return(mDNSBool)(
		r1->rrtype == r2->rrtype &&
		r1->rrclass == r2->rrclass &&
		r1->namehash == r2->namehash &&
		SameDomainName(r1->name, r2->name));
	}

// PacketRRMatchesSignature behaves as SameResourceRecordSignature, except that types may differ if our
// authoratative record is unique (as opposed to shared). For unique records, we are supposed to have
// complete ownership of *all* types for this name, so *any* record type with the same name is a conflict.
// In addition, when probing we send our questions with the wildcard type kDNSQType_ANY,
// so a response of any type should match, even if it is not actually the type the client plans to use.
mDNSlocal mDNSBool PacketRRMatchesSignature(const CacheRecord *const pktrr, const AuthRecord *const authrr)
	{
	if (!pktrr)  { LogMsg("PacketRRMatchesSignature ERROR: pktrr is NULL"); return(mDNSfalse); }
	if (!authrr) { LogMsg("PacketRRMatchesSignature ERROR: authrr is NULL"); return(mDNSfalse); }
	if (pktrr->resrec.InterfaceID &&
		authrr->resrec.InterfaceID &&
		pktrr->resrec.InterfaceID != authrr->resrec.InterfaceID) return(mDNSfalse);
	if (!(authrr->resrec.RecordType & kDNSRecordTypeUniqueMask) && pktrr->resrec.rrtype != authrr->resrec.rrtype) return(mDNSfalse);
	return(mDNSBool)(
		pktrr->resrec.rrclass == authrr->resrec.rrclass &&
		pktrr->resrec.namehash == authrr->resrec.namehash &&
		SameDomainName(pktrr->resrec.name, authrr->resrec.name));
	}

// IdenticalResourceRecord returns true if two resources records have
// the same name, type, class, and identical rdata (InterfaceID and TTL may differ)
mDNSlocal mDNSBool IdenticalResourceRecord(const ResourceRecord *const r1, const ResourceRecord *const r2)
	{
	if (!r1) { LogMsg("IdenticalResourceRecord ERROR: r1 is NULL"); return(mDNSfalse); }
	if (!r2) { LogMsg("IdenticalResourceRecord ERROR: r2 is NULL"); return(mDNSfalse); }
	if (r1->rrtype != r2->rrtype || r1->rrclass != r2->rrclass || r1->namehash != r2->namehash || !SameDomainName(r1->name, r2->name))
		return(mDNSfalse);
	return(SameRData(r1, r2));
	}

// CacheRecord *ks is the CacheRecord from the known answer list in the query.
// This is the information that the requester believes to be correct.
// AuthRecord *rr is the answer we are proposing to give, if not suppressed.
// This is the information that we believe to be correct.
// We've already determined that we plan to give this answer on this interface
// (either the record is non-specific, or it is specific to this interface)
// so now we just need to check the name, type, class, rdata and TTL.
mDNSlocal mDNSBool ShouldSuppressKnownAnswer(const CacheRecord *const ka, const AuthRecord *const rr)
	{
	// If RR signature is different, or data is different, then don't suppress our answer
	if (!IdenticalResourceRecord(&ka->resrec, &rr->resrec)) return(mDNSfalse);
	
	// If the requester's indicated TTL is less than half the real TTL,
	// we need to give our answer before the requester's copy expires.
	// If the requester's indicated TTL is at least half the real TTL,
	// then we can suppress our answer this time.
	// If the requester's indicated TTL is greater than the TTL we believe,
	// then that's okay, and we don't need to do anything about it.
	// (If two responders on the network are offering the same information,
	// that's okay, and if they are offering the information with different TTLs,
	// the one offering the lower TTL should defer to the one offering the higher TTL.)
	return(mDNSBool)(ka->resrec.rroriginalttl >= rr->resrec.rroriginalttl / 2);
	}

mDNSlocal void SetNextAnnounceProbeTime(mDNS *const m, const AuthRecord *const rr)
	{
	if (rr->resrec.RecordType == kDNSRecordTypeUnique)
		{
		//LogMsg("ProbeCount %d Next %ld %s",
		//	rr->ProbeCount, (rr->LastAPTime + rr->ThisAPInterval) - m->timenow, ARDisplayString(m, rr));
		if (m->NextScheduledProbe - (rr->LastAPTime + rr->ThisAPInterval) >= 0)
			m->NextScheduledProbe = (rr->LastAPTime + rr->ThisAPInterval);
		}
	else if (rr->AnnounceCount && ResourceRecordIsValidAnswer(rr))
		{
		if (m->NextScheduledResponse - (rr->LastAPTime + rr->ThisAPInterval) >= 0)
			m->NextScheduledResponse = (rr->LastAPTime + rr->ThisAPInterval);
		}
	}

mDNSlocal void InitializeLastAPTime(mDNS *const m, AuthRecord *const rr)
	{
	// To allow us to aggregate probes when a group of services are registered together,
	// the first probe is delayed 1/4 second. This means the common-case behaviour is:
	// 1/4 second wait; probe
	// 1/4 second wait; probe
	// 1/4 second wait; probe
	// 1/4 second wait; announce (i.e. service is normally announced exactly one second after being registered)

	// If we have no probe suppression time set, or it is in the past, set it now
	if (m->SuppressProbes == 0 || m->SuppressProbes - m->timenow < 0)
		{
		m->SuppressProbes = NonZeroTime(m->timenow + DefaultProbeIntervalForTypeUnique);
		// If we already have a probe scheduled to go out sooner, then use that time to get better aggregation
		if (m->SuppressProbes - m->NextScheduledProbe >= 0)
			m->SuppressProbes = m->NextScheduledProbe;
		// If we already have a query scheduled to go out sooner, then use that time to get better aggregation
		if (m->SuppressProbes - m->NextScheduledQuery >= 0)
			m->SuppressProbes = m->NextScheduledQuery;
		}
	
	// We announce to flush stale data from other caches. It is a reasonable assumption that any
	// old stale copies will probably have the same TTL we're using, so announcing longer than
	// this serves no purpose -- any stale copies of that record will have expired by then anyway.
	rr->AnnounceUntil   = m->timenow + TicksTTL(rr);
	rr->LastAPTime      = m->SuppressProbes - rr->ThisAPInterval;
	// Set LastMCTime to now, to inhibit multicast responses
	// (no need to send additional multicast responses when we're announcing anyway)
	rr->LastMCTime      = m->timenow;
	rr->LastMCInterface = mDNSInterfaceMark;
	
	// If this is a record type that's not going to probe, then delay its first announcement so that
	// it will go out synchronized with the first announcement for the other records that *are* probing.
	// This is a minor performance tweak that helps keep groups of related records synchronized together.
	// The addition of "rr->ThisAPInterval / 2" is to make sure that, in the event that any of the probes are
	// delayed by a few milliseconds, this announcement does not inadvertently go out *before* the probing is complete.
	// When the probing is complete and those records begin to announce, these records will also be picked up and accelerated,
	// because they will meet the criterion of being at least half-way to their scheduled announcement time.
	if (rr->resrec.RecordType != kDNSRecordTypeUnique)
		rr->LastAPTime += DefaultProbeIntervalForTypeUnique * DefaultProbeCountForTypeUnique + rr->ThisAPInterval / 2;
	
	SetNextAnnounceProbeTime(m, rr);
	}

#define HashSlot(X) (DomainNameHashValue(X) % CACHE_HASH_SLOTS)

mDNSlocal void SetTargetToHostName(mDNS *const m, AuthRecord *const rr)
	{
	domainname *target = GetRRDomainNameTarget(&rr->resrec);

	if (!target) debugf("SetTargetToHostName: Don't know how to set the target of rrtype %d", rr->resrec.rrtype);

	if (target && SameDomainName(target, &m->MulticastHostname))
		debugf("SetTargetToHostName: Target of %##s is already %##s", rr->resrec.name->c, target->c);
	
	if (target && !SameDomainName(target, &m->MulticastHostname))
		{
		AssignDomainName(target, &m->MulticastHostname);
		SetNewRData(&rr->resrec, mDNSNULL, 0);
		
		// If we're in the middle of probing this record, we need to start again,
		// because changing its rdata may change the outcome of the tie-breaker.
		// (If the record type is kDNSRecordTypeUnique (unconfirmed unique) then DefaultProbeCountForRecordType is non-zero.)
		rr->ProbeCount     = DefaultProbeCountForRecordType(rr->resrec.RecordType);

		// If we've announced this record, we really should send a goodbye packet for the old rdata before
		// changing to the new rdata. However, in practice, we only do SetTargetToHostName for unique records,
		// so when we announce them we'll set the kDNSClass_UniqueRRSet and clear any stale data that way.
		if (rr->RequireGoodbye && rr->resrec.RecordType == kDNSRecordTypeShared)
			debugf("Have announced shared record %##s (%s) at least once: should have sent a goodbye packet before updating",
				rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));

		rr->AnnounceCount  = InitialAnnounceCount;
		rr->RequireGoodbye = mDNSfalse;
		rr->ThisAPInterval = DefaultAPIntervalForRecordType(rr->resrec.RecordType);
		InitializeLastAPTime(m,rr);
		}
	}

mDNSlocal void AcknowledgeRecord(mDNS *const m, AuthRecord *const rr)
	{
	if (!rr->Acknowledged && rr->RecordCallback)
		{
		// CAUTION: MUST NOT do anything more with rr after calling rr->Callback(), because the client's callback function
		// is allowed to do anything, including starting/stopping queries, registering/deregistering records, etc.
		rr->Acknowledged = mDNStrue;
		m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
		rr->RecordCallback(m, rr, mStatus_NoError);
		m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
		}
	}

// Two records qualify to be local duplicates if the RecordTypes are the same, or if one is Unique and the other Verified
#define RecordLDT(A,B) ((A)->resrec.RecordType == (B)->resrec.RecordType || \
	((A)->resrec.RecordType | (B)->resrec.RecordType) == (kDNSRecordTypeUnique | kDNSRecordTypeVerified))
#define RecordIsLocalDuplicate(A,B) \
	((A)->resrec.InterfaceID == (B)->resrec.InterfaceID && RecordLDT((A),(B)) && IdenticalResourceRecord(&(A)->resrec, &(B)->resrec))

mDNSlocal mStatus mDNS_Register_internal(mDNS *const m, AuthRecord *const rr)
	{
	domainname *target = GetRRDomainNameTarget(&rr->resrec);
	AuthRecord *r;
	AuthRecord **p = &m->ResourceRecords;
	AuthRecord **d = &m->DuplicateRecords;

	mDNSPlatformMemZero(&rr->uDNS_info, sizeof(uDNS_RegInfo));

	if ((mDNSs32)rr->resrec.rroriginalttl <= 0)
		{ LogMsg("mDNS_Register_internal: TTL must be 1 - 0x7FFFFFFF %s", ARDisplayString(m, rr)); return(mStatus_BadParamErr); }
	
#ifndef UNICAST_DISABLED
    if (rr->resrec.InterfaceID == mDNSInterface_LocalOnly || rr->ForceMCast || IsLocalDomain(rr->resrec.name))
    	rr->uDNS_info.id = zeroID;
    else return uDNS_RegisterRecord(m, rr);
#endif
	
	while (*p && *p != rr) p=&(*p)->next;
	while (*d && *d != rr) d=&(*d)->next;
	if (*d || *p)
		{
		LogMsg("Error! Tried to register a AuthRecord %p %##s (%s) that's already in the list",
			rr, rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
		return(mStatus_AlreadyRegistered);
		}

	if (rr->DependentOn)
		{
		if (rr->resrec.RecordType == kDNSRecordTypeUnique)
			rr->resrec.RecordType =  kDNSRecordTypeVerified;
		else
			{
			LogMsg("mDNS_Register_internal: ERROR! %##s (%s): rr->DependentOn && RecordType != kDNSRecordTypeUnique",
				rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
			return(mStatus_Invalid);
			}
		if (!(rr->DependentOn->resrec.RecordType & (kDNSRecordTypeUnique | kDNSRecordTypeVerified)))
			{
			LogMsg("mDNS_Register_internal: ERROR! %##s (%s): rr->DependentOn->RecordType bad type %X",
				rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype), rr->DependentOn->resrec.RecordType);
			return(mStatus_Invalid);
			}
		}

	// If this resource record is referencing a specific interface, make sure it exists
	if (rr->resrec.InterfaceID && rr->resrec.InterfaceID != mDNSInterface_LocalOnly)
		{
		NetworkInterfaceInfo *intf;
		for (intf = m->HostInterfaces; intf; intf = intf->next)
			if (intf->InterfaceID == rr->resrec.InterfaceID) break;
		if (!intf)
			{
			debugf("mDNS_Register_internal: Bogus InterfaceID %p in resource record", rr->resrec.InterfaceID);
			return(mStatus_BadReferenceErr);
			}
		}

	rr->next = mDNSNULL;

	// Field Group 1: Persistent metadata for Authoritative Records
//	rr->Additional1       = set to mDNSNULL in mDNS_SetupResourceRecord; may be overridden by client
//	rr->Additional2       = set to mDNSNULL in mDNS_SetupResourceRecord; may be overridden by client
//	rr->DependentOn       = set to mDNSNULL in mDNS_SetupResourceRecord; may be overridden by client
//	rr->RRSet             = set to mDNSNULL in mDNS_SetupResourceRecord; may be overridden by client
//	rr->Callback          = already set in mDNS_SetupResourceRecord
//	rr->Context           = already set in mDNS_SetupResourceRecord
//	rr->RecordType        = already set in mDNS_SetupResourceRecord
//	rr->HostTarget        = set to mDNSfalse in mDNS_SetupResourceRecord; may be overridden by client
//	rr->AllowRemoteQuery  = set to mDNSfalse in mDNS_SetupResourceRecord; may be overridden by client
	// Make sure target is not uninitialized data, or we may crash writing debugging log messages
	if (rr->HostTarget && target) target->c[0] = 0;

	// Field Group 2: Transient state for Authoritative Records
	rr->Acknowledged      = mDNSfalse;
	rr->ProbeCount        = DefaultProbeCountForRecordType(rr->resrec.RecordType);
	rr->AnnounceCount     = InitialAnnounceCount;
	rr->RequireGoodbye    = mDNSfalse;
	rr->LocalAnswer       = mDNSfalse;
	rr->IncludeInProbe    = mDNSfalse;
	rr->ImmedAnswer       = mDNSNULL;
	rr->ImmedUnicast      = mDNSfalse;
	rr->ImmedAdditional   = mDNSNULL;
	rr->SendRNow          = mDNSNULL;
	rr->v4Requester       = zerov4Addr;
	rr->v6Requester       = zerov6Addr;
	rr->NextResponse      = mDNSNULL;
	rr->NR_AnswerTo       = mDNSNULL;
	rr->NR_AdditionalTo   = mDNSNULL;
	rr->ThisAPInterval    = DefaultAPIntervalForRecordType(rr->resrec.RecordType);
	if (!rr->HostTarget) InitializeLastAPTime(m, rr);
//	rr->AnnounceUntil     = Set for us in InitializeLastAPTime()
//	rr->LastAPTime        = Set for us in InitializeLastAPTime()
//	rr->LastMCTime        = Set for us in InitializeLastAPTime()
//	rr->LastMCInterface   = Set for us in InitializeLastAPTime()
	rr->NewRData          = mDNSNULL;
	rr->newrdlength       = 0;
	rr->UpdateCallback    = mDNSNULL;
	rr->UpdateCredits     = kMaxUpdateCredits;
	rr->NextUpdateCredit  = 0;
	rr->UpdateBlocked     = 0;

//	rr->resrec.interface         = already set in mDNS_SetupResourceRecord
//	rr->resrec.name->c            = MUST be set by client
//	rr->resrec.rrtype            = already set in mDNS_SetupResourceRecord
//	rr->resrec.rrclass           = already set in mDNS_SetupResourceRecord
//	rr->resrec.rroriginalttl     = already set in mDNS_SetupResourceRecord
//	rr->resrec.rdata             = MUST be set by client, unless record type is CNAME or PTR and rr->HostTarget is set

	if (rr->HostTarget)
		SetTargetToHostName(m, rr);	// Also sets rdlength and rdestimate for us, and calls InitializeLastAPTime();
	else
		{
		rr->resrec.rdlength   = GetRDLength(&rr->resrec, mDNSfalse);
		rr->resrec.rdestimate = GetRDLength(&rr->resrec, mDNStrue);
		}

	if (!ValidateDomainName(rr->resrec.name))
		{ LogMsg("Attempt to register record with invalid name: %s", ARDisplayString(m, rr)); return(mStatus_Invalid); }

	// BIND named (name daemon) doesn't allow TXT records with zero-length rdata. This is strictly speaking correct,
	// since RFC 1035 specifies a TXT record as "One or more <character-string>s", not "Zero or more <character-string>s".
	// Since some legacy apps try to create zero-length TXT records, we'll silently correct it here.
	if (rr->resrec.rrtype == kDNSType_TXT && rr->resrec.rdlength == 0) { rr->resrec.rdlength = 1; rr->resrec.rdata->u.txt.c[0] = 0; }

	// Don't do this until *after* we've set rr->resrec.rdlength
	if (!ValidateRData(rr->resrec.rrtype, rr->resrec.rdlength, rr->resrec.rdata))
		{ LogMsg("Attempt to register record with invalid rdata: %s", ARDisplayString(m, rr)); return(mStatus_Invalid); }

	rr->resrec.namehash   = DomainNameHashValue(rr->resrec.name);
	rr->resrec.rdatahash  = target ? DomainNameHashValue(target) : RDataHashValue(rr->resrec.rdlength, &rr->resrec.rdata->u);
	
	if (rr->resrec.InterfaceID == mDNSInterface_LocalOnly)
		{
		// If this is supposed to be unique, make sure we don't have any name conflicts
		if (rr->resrec.RecordType & kDNSRecordTypeUniqueMask)
			{
			const AuthRecord *s1 = rr->RRSet ? rr->RRSet : rr;
			for (r = m->ResourceRecords; r; r=r->next)
				{
				const AuthRecord *s2 = r->RRSet ? r->RRSet : r;
				if (s1 != s2 && SameResourceRecordSignature(&r->resrec, &rr->resrec) && !SameRData(&r->resrec, &rr->resrec))
					break;
				}
			if (r)	// If we found a conflict, set RecordType = kDNSRecordTypeDeregistering so we'll deliver the callback
				{
				debugf("Name conflict %p %##s (%s)", rr, rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
				rr->resrec.RecordType    = kDNSRecordTypeDeregistering;
				rr->resrec.rroriginalttl = 0;
				rr->ImmedAnswer          = mDNSInterfaceMark;
				m->NextScheduledResponse = m->timenow;
				}
			}
		}

	// Now that we've finished building our new record, make sure it's not identical to one we already have
	for (r = m->ResourceRecords; r; r=r->next) if (RecordIsLocalDuplicate(r, rr)) break;
	
	if (r)
		{
		debugf("Adding to duplicate list %p %s", rr, ARDisplayString(m,rr));
		*d = rr;
		// If the previous copy of this record is already verified unique,
		// then indicate that we should move this record promptly to kDNSRecordTypeUnique state.
		// Setting ProbeCount to zero will cause SendQueries() to advance this record to
		// kDNSRecordTypeVerified state and call the client callback at the next appropriate time.
		if (rr->resrec.RecordType == kDNSRecordTypeUnique && r->resrec.RecordType == kDNSRecordTypeVerified)
			rr->ProbeCount = 0;
		}
	else
		{
		debugf("Adding to active record list %p %s", rr, ARDisplayString(m,rr));
		if (!m->NewLocalRecords) m->NewLocalRecords = rr;
		*p = rr;
		}

	// For records that are not going to probe, acknowledge them right away
	if (rr->resrec.RecordType != kDNSRecordTypeUnique && rr->resrec.RecordType != kDNSRecordTypeDeregistering)
		AcknowledgeRecord(m, rr);

	return(mStatus_NoError);
	}

mDNSlocal void RecordProbeFailure(mDNS *const m, const AuthRecord *const rr)
	{
	m->ProbeFailTime = m->timenow;
	m->NumFailedProbes++;
	// If we've had fifteen or more probe failures, rate-limit to one every five seconds.
	// If a bunch of hosts have all been configured with the same name, then they'll all
	// conflict and run through the same series of names: name-2, name-3, name-4, etc.,
	// up to name-10. After that they'll start adding random increments in the range 1-100,
	// so they're more likely to branch out in the available namespace and settle on a set of
	// unique names quickly. If after five more tries the host is still conflicting, then we
	// may have a serious problem, so we start rate-limiting so we don't melt down the network.
	if (m->NumFailedProbes >= 15)
		{
		m->SuppressProbes = NonZeroTime(m->timenow + mDNSPlatformOneSecond * 5);
		LogMsg("Excessive name conflicts (%lu) for %##s (%s); rate limiting in effect",
			m->NumFailedProbes, rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
		}
	}

mDNSlocal void CompleteRDataUpdate(mDNS *const m, AuthRecord *const rr)
	{
	RData *OldRData = rr->resrec.rdata;
	SetNewRData(&rr->resrec, rr->NewRData, rr->newrdlength);	// Update our rdata
	rr->NewRData = mDNSNULL;									// Clear the NewRData pointer ...
	if (rr->UpdateCallback)
		rr->UpdateCallback(m, rr, OldRData);					// ... and let the client know
	}

// mDNS_Dereg_normal is used for most calls to mDNS_Deregister_internal
// mDNS_Dereg_conflict is used to indicate that this record is being forcibly deregistered because of a conflict
// mDNS_Dereg_repeat is used when cleaning up, for records that may have already been forcibly deregistered
typedef enum { mDNS_Dereg_normal, mDNS_Dereg_conflict, mDNS_Dereg_repeat } mDNS_Dereg_type;

// NOTE: mDNS_Deregister_internal can call a user callback, which may change the record list and/or question list.
// Any code walking either list must use the CurrentQuestion and/or CurrentRecord mechanism to protect against this.
mDNSlocal mStatus mDNS_Deregister_internal(mDNS *const m, AuthRecord *const rr, mDNS_Dereg_type drt)
	{
	AuthRecord *r2;
	mDNSu8 RecordType = rr->resrec.RecordType;
	AuthRecord **p = &m->ResourceRecords;	// Find this record in our list of active records

#ifndef UNICAST_DISABLED
    if (!(rr->resrec.InterfaceID == mDNSInterface_LocalOnly || rr->ForceMCast || IsLocalDomain(rr->resrec.name)))
		return uDNS_DeregisterRecord(m, rr);
#endif
	
	while (*p && *p != rr) p=&(*p)->next;

	if (*p)
		{
		// We found our record on the main list. See if there are any duplicates that need special handling.
		if (drt == mDNS_Dereg_conflict)		// If this was a conflict, see that all duplicates get the same treatment
			{
			// Scan for duplicates of rr, and mark them for deregistration at the end of this routine, after we've finished
			// deregistering rr. We need to do this scan *before* we give the client the chance to free and reuse the rr memory.
			for (r2 = m->DuplicateRecords; r2; r2=r2->next) if (RecordIsLocalDuplicate(r2, rr)) r2->ProbeCount = 0xFF;						
			}
		else
			{
			// Before we delete the record (and potentially send a goodbye packet)
			// first see if we have a record on the duplicate list ready to take over from it.
			AuthRecord **d = &m->DuplicateRecords;
			while (*d && !RecordIsLocalDuplicate(*d, rr)) d=&(*d)->next;
			if (*d)
				{
				AuthRecord *dup = *d;
				debugf("Duplicate record %p taking over from %p %##s (%s)",
					dup, rr, rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
				*d        = dup->next;		// Cut replacement record from DuplicateRecords list
				dup->next = rr->next;		// And then...
				rr->next  = dup;			// ... splice it in right after the record we're about to delete
				dup->resrec.RecordType        = rr->resrec.RecordType;
				dup->ProbeCount      = rr->ProbeCount;
				dup->AnnounceCount   = rr->AnnounceCount;
				dup->RequireGoodbye  = rr->RequireGoodbye;
				dup->ImmedAnswer     = rr->ImmedAnswer;
				dup->ImmedUnicast    = rr->ImmedUnicast;
				dup->ImmedAdditional = rr->ImmedAdditional;
				dup->v4Requester     = rr->v4Requester;
				dup->v6Requester     = rr->v6Requester;
				dup->ThisAPInterval  = rr->ThisAPInterval;
				dup->AnnounceUntil   = rr->AnnounceUntil;
				dup->LastAPTime      = rr->LastAPTime;
				dup->LastMCTime      = rr->LastMCTime;
				dup->LastMCInterface = rr->LastMCInterface;
				rr->RequireGoodbye = mDNSfalse;
				}
			}
		}
	else
		{
		// We didn't find our record on the main list; try the DuplicateRecords list instead.
		p = &m->DuplicateRecords;
		while (*p && *p != rr) p=&(*p)->next;
		// If we found our record on the duplicate list, then make sure we don't send a goodbye for it
		if (*p) rr->RequireGoodbye = mDNSfalse;
		if (*p) debugf("DNS_Deregister_internal: Deleting DuplicateRecord %p %##s (%s)",
			rr, rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
		}

	if (!*p)
		{
		// No need to log an error message if we already know this is a potentially repeated deregistration
		if (drt != mDNS_Dereg_repeat)
			LogMsg("mDNS_Deregister_internal: Record %p %##s (%s) not found in list",
				rr, rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
		return(mStatus_BadReferenceErr);
		}

	// If this is a shared record and we've announced it at least once,
	// we need to retract that announcement before we delete the record
	if (RecordType == kDNSRecordTypeShared && rr->RequireGoodbye)
		{
		verbosedebugf("mDNS_Deregister_internal: Sending deregister for %##s (%s)",
			rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
		rr->resrec.RecordType    = kDNSRecordTypeDeregistering;
		rr->resrec.rroriginalttl = 0;
		rr->ImmedAnswer          = mDNSInterfaceMark;
		if (m->NextScheduledResponse - (m->timenow + mDNSPlatformOneSecond/10) >= 0)
			m->NextScheduledResponse = (m->timenow + mDNSPlatformOneSecond/10);
		}
	else
		{
		*p = rr->next;					// Cut this record from the list
		// If someone is about to look at this, bump the pointer forward
		if (m->CurrentRecord   == rr) m->CurrentRecord   = rr->next;
		if (m->NewLocalRecords == rr) m->NewLocalRecords = rr->next;
		rr->next = mDNSNULL;

		if      (RecordType == kDNSRecordTypeUnregistered)
			debugf("mDNS_Deregister_internal: Record %##s (%s) already marked kDNSRecordTypeUnregistered",
				rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
		else if (RecordType == kDNSRecordTypeDeregistering)
			debugf("mDNS_Deregister_internal: Record %##s (%s) already marked kDNSRecordTypeDeregistering",
				rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
		else
			{
			verbosedebugf("mDNS_Deregister_internal: Deleting record for %##s (%s)",
				rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
			rr->resrec.RecordType = kDNSRecordTypeUnregistered;
			}

		if ((drt == mDNS_Dereg_conflict || drt == mDNS_Dereg_repeat) && RecordType == kDNSRecordTypeShared)
			debugf("mDNS_Deregister_internal: Cannot have a conflict on a shared record! %##s (%s)",
				rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));

		// If we have an update queued up which never executed, give the client a chance to free that memory
		if (rr->NewRData) CompleteRDataUpdate(m, rr);	// Update our rdata, clear the NewRData pointer, and return memory to the client
		
		if (rr->LocalAnswer) AnswerLocalQuestions(m, rr, mDNSfalse);

		// CAUTION: MUST NOT do anything more with rr after calling rr->Callback(), because the client's callback function
		// is allowed to do anything, including starting/stopping queries, registering/deregistering records, etc.
		// In this case the likely client action to the mStatus_MemFree message is to free the memory,
		// so any attempt to touch rr after this is likely to lead to a crash.
		m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
		if (drt != mDNS_Dereg_conflict)
			{
			if (rr->RecordCallback) rr->RecordCallback(m, rr, mStatus_MemFree);			// MUST NOT touch rr after this
			}
		else
			{
			RecordProbeFailure(m, rr);
			if (rr->RecordCallback) rr->RecordCallback(m, rr, mStatus_NameConflict);	// MUST NOT touch rr after this
			// Now that we've finished deregistering rr, check our DuplicateRecords list for any that we marked previously.
			// Note that with all the client callbacks going on, by the time we get here all the
			// records we marked may have been explicitly deregistered by the client anyway.
			r2 = m->DuplicateRecords;
			while (r2)
				{
				if (r2->ProbeCount != 0xFF) r2 = r2->next;
				else { mDNS_Deregister_internal(m, r2, mDNS_Dereg_conflict); r2 = m->DuplicateRecords; }
				}
			}
		m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
		}
	return(mStatus_NoError);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark -
#pragma mark - Packet Sending Functions
#endif

mDNSlocal void AddRecordToResponseList(AuthRecord ***nrpp, AuthRecord *rr, AuthRecord *add)
	{
	if (rr->NextResponse == mDNSNULL && *nrpp != &rr->NextResponse)
		{
		**nrpp = rr;
		// NR_AdditionalTo must point to a record with NR_AnswerTo set (and not NR_AdditionalTo)
		// If 'add' does not meet this requirement, then follow its NR_AdditionalTo pointer to a record that does
		// The referenced record will definitely be acceptable (by recursive application of this rule)
		if (add && add->NR_AdditionalTo) add = add->NR_AdditionalTo;
		rr->NR_AdditionalTo = add;
		*nrpp = &rr->NextResponse;
		}
	debugf("AddRecordToResponseList: %##s (%s) already in list", rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
	}

mDNSlocal void AddAdditionalsToResponseList(mDNS *const m, AuthRecord *ResponseRecords, AuthRecord ***nrpp, const mDNSInterfaceID InterfaceID)
	{
	AuthRecord  *rr, *rr2;
	for (rr=ResponseRecords; rr; rr=rr->NextResponse)			// For each record we plan to put
		{
		// (Note: This is an "if", not a "while". If we add a record, we'll find it again
		// later in the "for" loop, and we will follow further "additional" links then.)
		if (rr->Additional1 && ResourceRecordIsValidInterfaceAnswer(rr->Additional1, InterfaceID))
			AddRecordToResponseList(nrpp, rr->Additional1, rr);

		if (rr->Additional2 && ResourceRecordIsValidInterfaceAnswer(rr->Additional2, InterfaceID))
			AddRecordToResponseList(nrpp, rr->Additional2, rr);
		
		// For SRV records, automatically add the Address record(s) for the target host
		if (rr->resrec.rrtype == kDNSType_SRV)
			for (rr2=m->ResourceRecords; rr2; rr2=rr2->next)					// Scan list of resource records
				if (RRTypeIsAddressType(rr2->resrec.rrtype) &&					// For all address records (A/AAAA) ...
					ResourceRecordIsValidInterfaceAnswer(rr2, InterfaceID) &&	// ... which are valid for answer ...
					rr->resrec.rdatahash == rr2->resrec.namehash &&			// ... whose name is the name of the SRV target
					SameDomainName(&rr->resrec.rdata->u.srv.target, rr2->resrec.name))
					AddRecordToResponseList(nrpp, rr2, rr);
		}
	}

mDNSlocal void SendDelayedUnicastResponse(mDNS *const m, const mDNSAddr *const dest, const mDNSInterfaceID InterfaceID)
	{
	AuthRecord *rr;
	AuthRecord  *ResponseRecords = mDNSNULL;
	AuthRecord **nrp             = &ResponseRecords;

	// Make a list of all our records that need to be unicast to this destination
	for (rr = m->ResourceRecords; rr; rr=rr->next)
		{
		// If we find we can no longer unicast this answer, clear ImmedUnicast
		if (rr->ImmedAnswer == mDNSInterfaceMark               ||
			mDNSSameIPv4Address(rr->v4Requester, onesIPv4Addr) ||
			mDNSSameIPv6Address(rr->v6Requester, onesIPv6Addr)  )
			rr->ImmedUnicast = mDNSfalse;

		if (rr->ImmedUnicast && rr->ImmedAnswer == InterfaceID)
			if ((dest->type == mDNSAddrType_IPv4 && mDNSSameIPv4Address(rr->v4Requester, dest->ip.v4)) ||
				(dest->type == mDNSAddrType_IPv6 && mDNSSameIPv6Address(rr->v6Requester, dest->ip.v6)))
				{
				rr->ImmedAnswer  = mDNSNULL;				// Clear the state fields
				rr->ImmedUnicast = mDNSfalse;
				rr->v4Requester  = zerov4Addr;
				rr->v6Requester  = zerov6Addr;
				if (rr->NextResponse == mDNSNULL && nrp != &rr->NextResponse)	// rr->NR_AnswerTo
					{ rr->NR_AnswerTo = (mDNSu8*)~0; *nrp = rr; nrp = &rr->NextResponse; }
				}
		}

	AddAdditionalsToResponseList(m, ResponseRecords, &nrp, InterfaceID);

	while (ResponseRecords)
		{
		mDNSu8 *responseptr = m->omsg.data;
		mDNSu8 *newptr;
		InitializeDNSMessage(&m->omsg.h, zeroID, ResponseFlags);
		
		// Put answers in the packet
		while (ResponseRecords && ResponseRecords->NR_AnswerTo)
			{
			rr = ResponseRecords;
			if (rr->resrec.RecordType & kDNSRecordTypeUniqueMask)
				rr->resrec.rrclass |= kDNSClass_UniqueRRSet;		// Temporarily set the cache flush bit so PutResourceRecord will set it
			newptr = PutResourceRecord(&m->omsg, responseptr, &m->omsg.h.numAnswers, &rr->resrec);
			rr->resrec.rrclass &= ~kDNSClass_UniqueRRSet;			// Make sure to clear cache flush bit back to normal state
			if (!newptr && m->omsg.h.numAnswers) break;	// If packet full, send it now
			if (newptr) responseptr = newptr;
			ResponseRecords = rr->NextResponse;
			rr->NextResponse    = mDNSNULL;
			rr->NR_AnswerTo     = mDNSNULL;
			rr->NR_AdditionalTo = mDNSNULL;
			rr->RequireGoodbye  = mDNStrue;
			}
		
		// Add additionals, if there's space
		while (ResponseRecords && !ResponseRecords->NR_AnswerTo)
			{
			rr = ResponseRecords;
			if (rr->resrec.RecordType & kDNSRecordTypeUniqueMask)
				rr->resrec.rrclass |= kDNSClass_UniqueRRSet;		// Temporarily set the cache flush bit so PutResourceRecord will set it
			newptr = PutResourceRecord(&m->omsg, responseptr, &m->omsg.h.numAdditionals, &rr->resrec);
			rr->resrec.rrclass &= ~kDNSClass_UniqueRRSet;			// Make sure to clear cache flush bit back to normal state
			
			if (newptr) responseptr = newptr;
			if (newptr && m->omsg.h.numAnswers) rr->RequireGoodbye = mDNStrue;
			else if (rr->resrec.RecordType & kDNSRecordTypeUniqueMask) rr->ImmedAnswer = mDNSInterfaceMark;
			ResponseRecords = rr->NextResponse;
			rr->NextResponse    = mDNSNULL;
			rr->NR_AnswerTo     = mDNSNULL;
			rr->NR_AdditionalTo = mDNSNULL;
			}

		if (m->omsg.h.numAnswers) mDNSSendDNSMessage(m, &m->omsg, responseptr, mDNSInterface_Any, dest, MulticastDNSPort, -1, mDNSNULL);
		}
	}

mDNSlocal void CompleteDeregistration(mDNS *const m, AuthRecord *rr)
	{
	// Clearing rr->RequireGoodbye signals mDNS_Deregister_internal()
	// that it should go ahead and immediately dispose of this registration
	rr->resrec.RecordType = kDNSRecordTypeShared;
	rr->RequireGoodbye    = mDNSfalse;
	mDNS_Deregister_internal(m, rr, mDNS_Dereg_normal);		// Don't touch rr after this
	}

// NOTE: DiscardDeregistrations calls mDNS_Deregister_internal which can call a user callback, which may change
// the record list and/or question list.
// Any code walking either list must use the CurrentQuestion and/or CurrentRecord mechanism to protect against this.
mDNSlocal void DiscardDeregistrations(mDNS *const m)
	{
	if (m->CurrentRecord) LogMsg("DiscardDeregistrations ERROR m->CurrentRecord already set");
	m->CurrentRecord = m->ResourceRecords;
	
	while (m->CurrentRecord)
		{
		AuthRecord *rr = m->CurrentRecord;
		if (rr->resrec.RecordType == kDNSRecordTypeDeregistering)
			CompleteDeregistration(m, rr);		// Don't touch rr after this
		else
			m->CurrentRecord = rr->next;
		}
	}

mDNSlocal void GrantUpdateCredit(AuthRecord *rr)
	{
	if (++rr->UpdateCredits >= kMaxUpdateCredits) rr->NextUpdateCredit = 0;
	else rr->NextUpdateCredit = NonZeroTime(rr->NextUpdateCredit + kUpdateCreditRefreshInterval);
	}

// Note about acceleration of announcements to facilitate automatic coalescing of
// multiple independent threads of announcements into a single synchronized thread:
// The announcements in the packet may be at different stages of maturity;
// One-second interval, two-second interval, four-second interval, and so on.
// After we've put in all the announcements that are due, we then consider
// whether there are other nearly-due announcements that are worth accelerating.
// To be eligible for acceleration, a record MUST NOT be older (further along
// its timeline) than the most mature record we've already put in the packet.
// In other words, younger records can have their timelines accelerated to catch up
// with their elder bretheren; this narrows the age gap and helps them eventually get in sync.
// Older records cannot have their timelines accelerated; this would just widen
// the gap between them and their younger bretheren and get them even more out of sync.

// NOTE: SendResponses calls mDNS_Deregister_internal which can call a user callback, which may change
// the record list and/or question list.
// Any code walking either list must use the CurrentQuestion and/or CurrentRecord mechanism to protect against this.
mDNSlocal void SendResponses(mDNS *const m)
	{
	int pktcount = 0;
	AuthRecord *rr, *r2;
	mDNSs32 maxExistingAnnounceInterval = 0;
	const NetworkInterfaceInfo *intf = GetFirstActiveInterface(m->HostInterfaces);

	m->NextScheduledResponse = m->timenow + 0x78000000;

	for (rr = m->ResourceRecords; rr; rr=rr->next)
		if (rr->ImmedUnicast)
			{
			mDNSAddr v4 = { mDNSAddrType_IPv4, {{{0}}} };
			mDNSAddr v6 = { mDNSAddrType_IPv6, {{{0}}} };
			v4.ip.v4 = rr->v4Requester;
			v6.ip.v6 = rr->v6Requester;
			if (!mDNSIPv4AddressIsZero(rr->v4Requester)) SendDelayedUnicastResponse(m, &v4, rr->ImmedAnswer);
			if (!mDNSIPv6AddressIsZero(rr->v6Requester)) SendDelayedUnicastResponse(m, &v6, rr->ImmedAnswer);
			if (rr->ImmedUnicast)
				{
				LogMsg("SendResponses: ERROR: rr->ImmedUnicast still set: %s", ARDisplayString(m, rr));
				rr->ImmedUnicast = mDNSfalse;
				}
			}

	// ***
	// *** 1. Setup: Set the SendRNow and ImmedAnswer fields to indicate which interface(s) the records need to be sent on
	// ***

	// Run through our list of records, and decide which ones we're going to announce on all interfaces
	for (rr = m->ResourceRecords; rr; rr=rr->next)
		{
		while (rr->NextUpdateCredit && m->timenow - rr->NextUpdateCredit >= 0) GrantUpdateCredit(rr);
		if (TimeToAnnounceThisRecord(rr, m->timenow) && ResourceRecordIsValidAnswer(rr))
			{
			rr->ImmedAnswer = mDNSInterfaceMark;		// Send on all interfaces
			if (maxExistingAnnounceInterval < rr->ThisAPInterval)
				maxExistingAnnounceInterval = rr->ThisAPInterval;
			if (rr->UpdateBlocked) rr->UpdateBlocked = 0;
			}
		}

	// Any interface-specific records we're going to send are marked as being sent on all appropriate interfaces (which is just one)
	// Eligible records that are more than half-way to their announcement time are accelerated
	for (rr = m->ResourceRecords; rr; rr=rr->next)
		if ((rr->resrec.InterfaceID && rr->ImmedAnswer) ||
			(rr->ThisAPInterval <= maxExistingAnnounceInterval &&
			TimeToAnnounceThisRecord(rr, m->timenow + rr->ThisAPInterval/2) &&
			ResourceRecordIsValidAnswer(rr)))
			rr->ImmedAnswer = mDNSInterfaceMark;		// Send on all interfaces

	// When sending SRV records (particularly when announcing a new service) automatically add related Address record(s) as additionals
	// NOTE: Currently all address records are interface-specific, so it's safe to set ImmedAdditional to their InterfaceID,
	// which will be non-null. If by some chance there is an address record that's not interface-specific (should never happen)
	// then all that means is that it won't get sent -- which would not be the end of the world.
	for (rr = m->ResourceRecords; rr; rr=rr->next)
		if (rr->ImmedAnswer && rr->resrec.rrtype == kDNSType_SRV)
			for (r2=m->ResourceRecords; r2; r2=r2->next)				// Scan list of resource records
				if (RRTypeIsAddressType(r2->resrec.rrtype) &&			// For all address records (A/AAAA) ...
					ResourceRecordIsValidAnswer(r2) &&					// ... which are valid for answer ...
					rr->LastMCTime - r2->LastMCTime >= 0 &&				// ... which we have not sent recently ...
					rr->resrec.rdatahash == r2->resrec.namehash &&		// ... whose name is the name of the SRV target
					SameDomainName(&rr->resrec.rdata->u.srv.target, r2->resrec.name) &&
					(rr->ImmedAnswer == mDNSInterfaceMark || rr->ImmedAnswer == r2->resrec.InterfaceID))
					r2->ImmedAdditional = r2->resrec.InterfaceID;		// ... then mark this address record for sending too

	// If there's a record which is supposed to be unique that we're going to send, then make sure that we give
	// the whole RRSet as an atomic unit. That means that if we have any other records with the same name/type/class
	// then we need to mark them for sending too. Otherwise, if we set the kDNSClass_UniqueRRSet bit on a
	// record, then other RRSet members that have not been sent recently will get flushed out of client caches.
	// -- If a record is marked to be sent on a certain interface, make sure the whole set is marked to be sent on that interface
	// -- If any record is marked to be sent on all interfaces, make sure the whole set is marked to be sent on all interfaces
	for (rr = m->ResourceRecords; rr; rr=rr->next)
		if (rr->resrec.RecordType & kDNSRecordTypeUniqueMask)
			{
			if (rr->ImmedAnswer)			// If we're sending this as answer, see that its whole RRSet is similarly marked
				{
				for (r2 = m->ResourceRecords; r2; r2=r2->next)
					if (ResourceRecordIsValidAnswer(r2))
						if (r2->ImmedAnswer != mDNSInterfaceMark &&
							r2->ImmedAnswer != rr->ImmedAnswer && SameResourceRecordSignature(&r2->resrec, &rr->resrec))
							r2->ImmedAnswer = rr->ImmedAnswer;
				}
			else if (rr->ImmedAdditional)	// If we're sending this as additional, see that its whole RRSet is similarly marked
				{
				for (r2 = m->ResourceRecords; r2; r2=r2->next)
					if (ResourceRecordIsValidAnswer(r2))
						if (r2->ImmedAdditional != rr->ImmedAdditional && SameResourceRecordSignature(&r2->resrec, &rr->resrec))
							r2->ImmedAdditional = rr->ImmedAdditional;
				}
			}

	// Now set SendRNow state appropriately
	for (rr = m->ResourceRecords; rr; rr=rr->next)
		{
		if (rr->ImmedAnswer == mDNSInterfaceMark)		// Sending this record on all appropriate interfaces
			{
			rr->SendRNow = !intf ? mDNSNULL : (rr->resrec.InterfaceID) ? rr->resrec.InterfaceID : intf->InterfaceID;
			rr->ImmedAdditional = mDNSNULL;				// No need to send as additional if sending as answer
			rr->LastMCTime      = m->timenow;
			rr->LastMCInterface = rr->ImmedAnswer;
			// If we're announcing this record, and it's at least half-way to its ordained time, then consider this announcement done
			if (TimeToAnnounceThisRecord(rr, m->timenow + rr->ThisAPInterval/2))
				{
				rr->AnnounceCount--;
				rr->ThisAPInterval *= 2;
				rr->LastAPTime = m->timenow;
				if (rr->LastAPTime + rr->ThisAPInterval - rr->AnnounceUntil >= 0) rr->AnnounceCount = 0;
				debugf("Announcing %##s (%s) %d", rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype), rr->AnnounceCount);
				}
			}
		else if (rr->ImmedAnswer)						// Else, just respond to a single query on single interface:
			{
			rr->SendRNow        = rr->ImmedAnswer;		// Just respond on that interface
			rr->ImmedAdditional = mDNSNULL;				// No need to send as additional too
			rr->LastMCTime      = m->timenow;
			rr->LastMCInterface = rr->ImmedAnswer;
			}
		SetNextAnnounceProbeTime(m, rr);
		//if (rr->SendRNow) LogMsg("%-15.4a %s", &rr->v4Requester, ARDisplayString(m, rr));
		}

	// ***
	// *** 2. Loop through interface list, sending records as appropriate
	// ***

	while (intf)
		{
		int numDereg    = 0;
		int numAnnounce = 0;
		int numAnswer   = 0;
		mDNSu8 *responseptr = m->omsg.data;
		mDNSu8 *newptr;
		InitializeDNSMessage(&m->omsg.h, zeroID, ResponseFlags);
	
		// First Pass. Look for:
		// 1. Deregistering records that need to send their goodbye packet
		// 2. Updated records that need to retract their old data
		// 3. Answers and announcements we need to send
		// In all cases, if we fail, and we've put at least one answer, we break out of the for loop so we can
		// send this packet and then try again.
		// If we have not put even one answer, then we don't bail out. We pretend we succeeded anyway,
		// because otherwise we'll end up in an infinite loop trying to send a record that will never fit.
		for (rr = m->ResourceRecords; rr; rr=rr->next)
			if (rr->SendRNow == intf->InterfaceID)
				{
				if (rr->resrec.RecordType == kDNSRecordTypeDeregistering)
					{
					newptr = PutResourceRecordTTL(&m->omsg, responseptr, &m->omsg.h.numAnswers, &rr->resrec, 0);
					if (!newptr && m->omsg.h.numAnswers) break;
					numDereg++;
					responseptr = newptr;
					}
				else if (rr->NewRData && !m->SleepState)					// If we have new data for this record
					{
					RData *OldRData     = rr->resrec.rdata;
					mDNSu16 oldrdlength = rr->resrec.rdlength;
					// See if we should send a courtesy "goodbye" for the old data before we replace it.
					if (ResourceRecordIsValidAnswer(rr) && rr->RequireGoodbye)
						{
						newptr = PutResourceRecordTTL(&m->omsg, responseptr, &m->omsg.h.numAnswers, &rr->resrec, 0);
						if (!newptr && m->omsg.h.numAnswers) break;
						numDereg++;
						responseptr = newptr;
						rr->RequireGoodbye = mDNSfalse;
						}
					// Now try to see if we can fit the update in the same packet (not fatal if we can't)
					SetNewRData(&rr->resrec, rr->NewRData, rr->newrdlength);
					if (rr->resrec.RecordType & kDNSRecordTypeUniqueMask)
						rr->resrec.rrclass |= kDNSClass_UniqueRRSet;		// Temporarily set the cache flush bit so PutResourceRecord will set it
					newptr = PutResourceRecord(&m->omsg, responseptr, &m->omsg.h.numAnswers, &rr->resrec);
					rr->resrec.rrclass &= ~kDNSClass_UniqueRRSet;			// Make sure to clear cache flush bit back to normal state
					if (newptr) { responseptr = newptr; rr->RequireGoodbye = mDNStrue; }
					SetNewRData(&rr->resrec, OldRData, oldrdlength);
					}
				else
					{
					if (rr->resrec.RecordType & kDNSRecordTypeUniqueMask)
						rr->resrec.rrclass |= kDNSClass_UniqueRRSet;		// Temporarily set the cache flush bit so PutResourceRecord will set it
					newptr = PutResourceRecordTTL(&m->omsg, responseptr, &m->omsg.h.numAnswers, &rr->resrec, m->SleepState ? 0 : rr->resrec.rroriginalttl);
					rr->resrec.rrclass &= ~kDNSClass_UniqueRRSet;			// Make sure to clear cache flush bit back to normal state
					if (!newptr && m->omsg.h.numAnswers) break;
					rr->RequireGoodbye = (mDNSu8) (!m->SleepState);
					if (rr->LastAPTime == m->timenow) numAnnounce++; else numAnswer++;
					responseptr = newptr;
					}
				// If sending on all interfaces, go to next interface; else we're finished now
				if (rr->ImmedAnswer == mDNSInterfaceMark && rr->resrec.InterfaceID == mDNSInterface_Any)
					rr->SendRNow = GetNextActiveInterfaceID(intf);
				else
					rr->SendRNow = mDNSNULL;
				}
	
		// Second Pass. Add additional records, if there's space.
		newptr = responseptr;
		for (rr = m->ResourceRecords; rr; rr=rr->next)
			if (rr->ImmedAdditional == intf->InterfaceID)
				if (ResourceRecordIsValidAnswer(rr))
					{
					// If we have at least one answer already in the packet, then plan to add additionals too
					mDNSBool SendAdditional = (m->omsg.h.numAnswers > 0);
					
					// If we're not planning to send any additionals, but this record is a unique one, then
					// make sure we haven't already sent any other members of its RRSet -- if we have, then they
					// will have had the cache flush bit set, so now we need to finish the job and send the rest.
					if (!SendAdditional && (rr->resrec.RecordType & kDNSRecordTypeUniqueMask))
						{
						const AuthRecord *a;
						for (a = m->ResourceRecords; a; a=a->next)
							if (a->LastMCTime      == m->timenow &&
								a->LastMCInterface == intf->InterfaceID &&
								SameResourceRecordSignature(&a->resrec, &rr->resrec)) { SendAdditional = mDNStrue; break; }
						}
					if (!SendAdditional)					// If we don't want to send this after all,
						rr->ImmedAdditional = mDNSNULL;		// then cancel its ImmedAdditional field
					else if (newptr)						// Else, try to add it if we can
						{
						if (rr->resrec.RecordType & kDNSRecordTypeUniqueMask)
							rr->resrec.rrclass |= kDNSClass_UniqueRRSet;	// Temporarily set the cache flush bit so PutResourceRecord will set it
						newptr = PutResourceRecord(&m->omsg, newptr, &m->omsg.h.numAdditionals, &rr->resrec);
						rr->resrec.rrclass &= ~kDNSClass_UniqueRRSet;		// Make sure to clear cache flush bit back to normal state
						if (newptr)
							{
							responseptr = newptr;
							rr->ImmedAdditional = mDNSNULL;
							rr->RequireGoodbye = mDNStrue;
							// If we successfully put this additional record in the packet, we record LastMCTime & LastMCInterface.
							// This matters particularly in the case where we have more than one IPv6 (or IPv4) address, because otherwise,
							// when we see our own multicast with the cache flush bit set, if we haven't set LastMCTime, then we'll get
							// all concerned and re-announce our record again to make sure it doesn't get flushed from peer caches.
							rr->LastMCTime      = m->timenow;
							rr->LastMCInterface = intf->InterfaceID;
							}
						}
					}
	
		if (m->omsg.h.numAnswers > 0 || m->omsg.h.numAdditionals)
			{
			debugf("SendResponses: Sending %d Deregistration%s, %d Announcement%s, %d Answer%s, %d Additional%s on %p",
				numDereg,                  numDereg                  == 1 ? "" : "s",
				numAnnounce,               numAnnounce               == 1 ? "" : "s",
				numAnswer,                 numAnswer                 == 1 ? "" : "s",
				m->omsg.h.numAdditionals, m->omsg.h.numAdditionals == 1 ? "" : "s", intf->InterfaceID);
			if (intf->IPv4Available) mDNSSendDNSMessage(m, &m->omsg, responseptr, intf->InterfaceID, &AllDNSLinkGroup_v4, MulticastDNSPort, -1, mDNSNULL);
			if (intf->IPv6Available) mDNSSendDNSMessage(m, &m->omsg, responseptr, intf->InterfaceID, &AllDNSLinkGroup_v6, MulticastDNSPort, -1, mDNSNULL);
			if (!m->SuppressSending) m->SuppressSending = NonZeroTime(m->timenow + (mDNSPlatformOneSecond+9)/10);
			if (++pktcount >= 1000) { LogMsg("SendResponses exceeded loop limit %d: giving up", pktcount); break; }
			// There might be more things to send on this interface, so go around one more time and try again.
			}
		else	// Nothing more to send on this interface; go to next
			{
			const NetworkInterfaceInfo *next = GetFirstActiveInterface(intf->next);
			#if MDNS_DEBUGMSGS && 0
			const char *const msg = next ? "SendResponses: Nothing more on %p; moving to %p" : "SendResponses: Nothing more on %p";
			debugf(msg, intf, next);
			#endif
			intf = next;
			}
		}

	// ***
	// *** 3. Cleanup: Now that everything is sent, call client callback functions, and reset state variables
	// ***

	if (m->CurrentRecord) LogMsg("SendResponses: ERROR m->CurrentRecord already set");
	m->CurrentRecord = m->ResourceRecords;
	while (m->CurrentRecord)
		{
		rr = m->CurrentRecord;
		m->CurrentRecord = rr->next;

		if (rr->SendRNow)
			{
			if (rr->resrec.InterfaceID != mDNSInterface_LocalOnly)
				LogMsg("SendResponses: No active interface to send: %s", ARDisplayString(m, rr));
			rr->SendRNow = mDNSNULL;
			}

		if (rr->ImmedAnswer)
			{
			if (rr->NewRData) CompleteRDataUpdate(m,rr);	// Update our rdata, clear the NewRData pointer, and return memory to the client
	
			if (rr->resrec.RecordType == kDNSRecordTypeDeregistering)
				CompleteDeregistration(m, rr);		// Don't touch rr after this
			else
				{
				rr->ImmedAnswer  = mDNSNULL;
				rr->ImmedUnicast = mDNSfalse;
				rr->v4Requester  = zerov4Addr;
				rr->v6Requester  = zerov6Addr;
				}
			}
		}
	verbosedebugf("SendResponses: Next in %ld ticks", m->NextScheduledResponse - m->timenow);
	}

// Calling CheckCacheExpiration() is an expensive operation because it has to look at the entire cache,
// so we want to be lazy about how frequently we do it.
// 1. If a cache record is currently referenced by *no* active questions,
//    then we don't mind expiring it up to a minute late (who will know?)
// 2. Else, if a cache record is due for some of its final expiration queries,
//    we'll allow them to be late by up to 2% of the TTL
// 3. Else, if a cache record has completed all its final expiration queries without success,
//    and is expiring, and had an original TTL more than ten seconds, we'll allow it to be one second late
// 4. Else, it is expiring and had an original TTL of ten seconds or less (includes explicit goodbye packets),
//    so allow at most 1/10 second lateness
#define CacheCheckGracePeriod(RR) (                                                   \
	((RR)->DelayDelivery                           ) ? (mDNSPlatformOneSecond/10)   : \
	((RR)->CRActiveQuestion == mDNSNULL            ) ? (60 * mDNSPlatformOneSecond) : \
	((RR)->UnansweredQueries < MaxUnansweredQueries) ? (TicksTTL(rr)/50)            : \
	((RR)->resrec.rroriginalttl > 10               ) ? (mDNSPlatformOneSecond)      : (mDNSPlatformOneSecond/10))

// Note: MUST call SetNextCacheCheckTime any time we change:
// rr->TimeRcvd
// rr->resrec.rroriginalttl
// rr->UnansweredQueries
// rr->CRActiveQuestion
// Also, any time we set rr->DelayDelivery we should call SetNextCacheCheckTime to ensure m->NextCacheCheck is set if necessary
// Clearing rr->DelayDelivery does not require a call to SetNextCacheCheckTime
mDNSlocal void SetNextCacheCheckTime(mDNS *const m, CacheRecord *const rr)
	{
	rr->NextRequiredQuery = RRExpireTime(rr);

	// If we have an active question, then see if we want to schedule a refresher query for this record.
	// Usually we expect to do four queries, at 80-82%, 85-87%, 90-92% and then 95-97% of the TTL.
	if (rr->CRActiveQuestion && rr->UnansweredQueries < MaxUnansweredQueries)
		{
		rr->NextRequiredQuery -= TicksTTL(rr)/20 * (MaxUnansweredQueries - rr->UnansweredQueries);
		rr->NextRequiredQuery += mDNSRandom((mDNSu32)TicksTTL(rr)/50);
		verbosedebugf("SetNextCacheCheckTime: %##s (%s) NextRequiredQuery in %ld sec CacheCheckGracePeriod %d ticks",
			rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype),
			(rr->NextRequiredQuery - m->timenow) / mDNSPlatformOneSecond, CacheCheckGracePeriod(rr));
		}

	if (m->NextCacheCheck - (rr->NextRequiredQuery + CacheCheckGracePeriod(rr)) > 0)
		m->NextCacheCheck = (rr->NextRequiredQuery + CacheCheckGracePeriod(rr));
	
	if (rr->DelayDelivery)
		if (m->NextCacheCheck - rr->DelayDelivery > 0)
			m->NextCacheCheck = rr->DelayDelivery;
	}

#define kMinimumReconfirmTime                     ((mDNSu32)mDNSPlatformOneSecond *  5)
#define kDefaultReconfirmTimeForWake              ((mDNSu32)mDNSPlatformOneSecond *  5)
#define kDefaultReconfirmTimeForNoAnswer          ((mDNSu32)mDNSPlatformOneSecond * 15)
#define kDefaultReconfirmTimeForFlappingInterface ((mDNSu32)mDNSPlatformOneSecond * 30)

mDNSlocal mStatus mDNS_Reconfirm_internal(mDNS *const m, CacheRecord *const rr, mDNSu32 interval)
	{
	if (interval < kMinimumReconfirmTime)
		interval = kMinimumReconfirmTime;
	if (interval > 0x10000000)	// Make sure interval doesn't overflow when we multiply by four below
		interval = 0x10000000;

	// If the expected expiration time for this record is more than interval+33%, then accelerate its expiration
	if (RRExpireTime(rr) - m->timenow > (mDNSs32)((interval * 4) / 3))
		{
		// Add a 33% random amount to the interval, to avoid synchronization between multiple hosts
		// For all the reconfirmations in a given batch, we want to use the same random value
		// so that the reconfirmation questions can be grouped into a single query packet
		if (!m->RandomReconfirmDelay) m->RandomReconfirmDelay = 1 + mDNSRandom(0x3FFFFFFF);
		interval += mDNSRandomFromFixedSeed(m->RandomReconfirmDelay, interval/3);
		rr->TimeRcvd          = m->timenow - (mDNSs32)interval * 3;
		rr->resrec.rroriginalttl     = (interval * 4 + mDNSPlatformOneSecond - 1) / mDNSPlatformOneSecond;
		SetNextCacheCheckTime(m, rr);
		}
	debugf("mDNS_Reconfirm_internal:%6ld ticks to go for %s", RRExpireTime(rr) - m->timenow, CRDisplayString(m, rr));
	return(mStatus_NoError);
	}

#define MaxQuestionInterval         (3600 * mDNSPlatformOneSecond)

// BuildQuestion puts a question into a DNS Query packet and if successful, updates the value of queryptr.
// It also appends to the list of known answer records that need to be included,
// and updates the forcast for the size of the known answer section.
mDNSlocal mDNSBool BuildQuestion(mDNS *const m, DNSMessage *query, mDNSu8 **queryptr, DNSQuestion *q,
	CacheRecord ***kalistptrptr, mDNSu32 *answerforecast)
	{
	mDNSBool ucast = (q->LargeAnswers || q->RequestUnicast) && m->CanReceiveUnicastOn5353;
	mDNSu16 ucbit = (mDNSu16)(ucast ? kDNSQClass_UnicastResponse : 0);
	const mDNSu8 *const limit = query->data + NormalMaxDNSMessageData;
	mDNSu8 *newptr = putQuestion(query, *queryptr, limit, &q->qname, q->qtype, (mDNSu16)(q->qclass | ucbit));
	if (!newptr)
		{
		debugf("BuildQuestion: No more space in this packet for question %##s", q->qname.c);
		return(mDNSfalse);
		}
	else if (newptr + *answerforecast >= limit)
		{
		verbosedebugf("BuildQuestion: Retracting question %##s new forecast total %d",
			q->qname.c, newptr + *answerforecast - query->data);
		query->h.numQuestions--;
		return(mDNSfalse);
		}
	else
		{
		mDNSu32 forecast = *answerforecast;
		const mDNSu32 slot = HashSlot(&q->qname);
		CacheGroup *cg = CacheGroupForName(m, slot, q->qnamehash, &q->qname);
		CacheRecord *rr;
		CacheRecord **ka = *kalistptrptr;	// Make a working copy of the pointer we're going to update

		for (rr = cg ? cg->members : mDNSNULL; rr; rr=rr->next)				// If we have a resource record in our cache,
			if (rr->resrec.InterfaceID == q->SendQNow &&					// received on this interface
				rr->NextInKAList == mDNSNULL && ka != &rr->NextInKAList &&	// which is not already in the known answer list
				rr->resrec.rdlength <= SmallRecordLimit &&					// which is small enough to sensibly fit in the packet
				ResourceRecordAnswersQuestion(&rr->resrec, q) &&			// which answers our question
				rr->TimeRcvd + TicksTTL(rr)/2 - m->timenow >				// and its half-way-to-expiry time is at least 1 second away
												mDNSPlatformOneSecond)		// (also ensures we never include goodbye records with TTL=1)
				{
				*ka = rr;	// Link this record into our known answer chain
				ka = &rr->NextInKAList;
				// We forecast: compressed name (2) type (2) class (2) TTL (4) rdlength (2) rdata (n)
				forecast += 12 + rr->resrec.rdestimate;
				// If we're trying to put more than one question in this packet, and it doesn't fit
				// then undo that last question and try again next time
				if (query->h.numQuestions > 1 && newptr + forecast >= limit)
					{
					debugf("BuildQuestion: Retracting question %##s (%s) new forecast total %d",
						q->qname.c, DNSTypeName(q->qtype), newptr + forecast - query->data);
					query->h.numQuestions--;
					ka = *kalistptrptr;		// Go back to where we started and retract these answer records
					while (*ka) { CacheRecord *rr = *ka; *ka = mDNSNULL; ka = &rr->NextInKAList; }
					return(mDNSfalse);		// Return false, so we'll try again in the next packet
					}
				}

		// Traffic reduction:
		// If we already have at least one unique answer in the cache,
		// OR we have so many shared answers that the KA list is too big to fit in one packet
		// The we suppress queries number 3 and 5:
		// Query 1 (immediately;      ThisQInterval =  1 sec; request unicast replies)
		// Query 2 (after  1 second;  ThisQInterval =  2 sec; send normally)
		// Query 3 (after  2 seconds; ThisQInterval =  4 sec; may suppress)
		// Query 4 (after  4 seconds; ThisQInterval =  8 sec; send normally)
		// Query 5 (after  8 seconds; ThisQInterval = 16 sec; may suppress)
		// Query 6 (after 16 seconds; ThisQInterval = 32 sec; send normally)
		if (q->UniqueAnswers || newptr + forecast >= limit)
			if (q->ThisQInterval == InitialQuestionInterval * 8 || q->ThisQInterval == InitialQuestionInterval * 32)
				{
				query->h.numQuestions--;
				ka = *kalistptrptr;		// Go back to where we started and retract these answer records
				while (*ka) { CacheRecord *rr = *ka; *ka = mDNSNULL; ka = &rr->NextInKAList; }
				return(mDNStrue);		// Return true: pretend we succeeded, even though we actually suppressed this question
				}

		// Success! Update our state pointers, increment UnansweredQueries as appropriate, and return
		*queryptr        = newptr;				// Update the packet pointer
		*answerforecast  = forecast;			// Update the forecast
		*kalistptrptr    = ka;					// Update the known answer list pointer
		if (ucast) m->ExpectUnicastResponse = m->timenow;

		for (rr = cg ? cg->members : mDNSNULL; rr; rr=rr->next)				// For every resource record in our cache,
			if (rr->resrec.InterfaceID == q->SendQNow &&					// received on this interface
				rr->NextInKAList == mDNSNULL && ka != &rr->NextInKAList &&	// which is not in the known answer list
				ResourceRecordAnswersQuestion(&rr->resrec, q))				// which answers our question
					{
					rr->UnansweredQueries++;								// indicate that we're expecting a response
					rr->LastUnansweredTime = m->timenow;
					SetNextCacheCheckTime(m, rr);
					}

		return(mDNStrue);
		}
	}

mDNSlocal void ReconfirmAntecedents(mDNS *const m, DNSQuestion *q)
	{
	mDNSu32 slot;
	CacheGroup *cg;
	CacheRecord *rr;
	domainname *target;
	FORALL_CACHERECORDS(slot, cg, rr)
		if ((target = GetRRDomainNameTarget(&rr->resrec)) && rr->resrec.rdatahash == q->qnamehash && SameDomainName(target, &q->qname))
			mDNS_Reconfirm_internal(m, rr, kDefaultReconfirmTimeForNoAnswer);
	}

// Only DupSuppressInfos newer than the specified 'time' are allowed to remain active
mDNSlocal void ExpireDupSuppressInfo(DupSuppressInfo ds[DupSuppressInfoSize], mDNSs32 time)
	{
	int i;
	for (i=0; i<DupSuppressInfoSize; i++) if (ds[i].Time - time < 0) ds[i].InterfaceID = mDNSNULL;
	}

mDNSlocal void ExpireDupSuppressInfoOnInterface(DupSuppressInfo ds[DupSuppressInfoSize], mDNSs32 time, mDNSInterfaceID InterfaceID)
	{
	int i;
	for (i=0; i<DupSuppressInfoSize; i++) if (ds[i].InterfaceID == InterfaceID && ds[i].Time - time < 0) ds[i].InterfaceID = mDNSNULL;
	}

mDNSlocal mDNSBool SuppressOnThisInterface(const DupSuppressInfo ds[DupSuppressInfoSize], const NetworkInterfaceInfo * const intf)
	{
	int i;
	mDNSBool v4 = !intf->IPv4Available;		// If this interface doesn't do v4, we don't need to find a v4 duplicate of this query
	mDNSBool v6 = !intf->IPv6Available;		// If this interface doesn't do v6, we don't need to find a v6 duplicate of this query
	for (i=0; i<DupSuppressInfoSize; i++)
		if (ds[i].InterfaceID == intf->InterfaceID)
			{
			if      (ds[i].Type == mDNSAddrType_IPv4) v4 = mDNStrue;
			else if (ds[i].Type == mDNSAddrType_IPv6) v6 = mDNStrue;
			if (v4 && v6) return(mDNStrue);
			}
	return(mDNSfalse);
	}

mDNSlocal int RecordDupSuppressInfo(DupSuppressInfo ds[DupSuppressInfoSize], mDNSs32 Time, mDNSInterfaceID InterfaceID, mDNSs32 Type)
	{
	int i, j;

	// See if we have this one in our list somewhere already
	for (i=0; i<DupSuppressInfoSize; i++) if (ds[i].InterfaceID == InterfaceID && ds[i].Type == Type) break;

	// If not, find a slot we can re-use
	if (i >= DupSuppressInfoSize)
		{
		i = 0;
		for (j=1; j<DupSuppressInfoSize && ds[i].InterfaceID; j++)
			if (!ds[j].InterfaceID || ds[j].Time - ds[i].Time < 0)
				i = j;
		}
	
	// Record the info about this query we saw
	ds[i].Time        = Time;
	ds[i].InterfaceID = InterfaceID;
	ds[i].Type        = Type;
	
	return(i);
	}

mDNSlocal mDNSBool AccelerateThisQuery(mDNS *const m, DNSQuestion *q)
	{
	// If more than 90% of the way to the query time, we should unconditionally accelerate it
	if (TimeToSendThisQuestion(q, m->timenow + q->ThisQInterval/10))
		return(mDNStrue);

	// If half-way to next scheduled query time, only accelerate if it will add less than 512 bytes to the packet
	if (TimeToSendThisQuestion(q, m->timenow + q->ThisQInterval/2))
		{
		// We forecast: qname (n) type (2) class (2)
		mDNSu32 forecast = (mDNSu32)DomainNameLength(&q->qname) + 4;
		const mDNSu32 slot = HashSlot(&q->qname);
		CacheGroup *cg = CacheGroupForName(m, slot, q->qnamehash, &q->qname);
		CacheRecord *rr;
		for (rr = cg ? cg->members : mDNSNULL; rr; rr=rr->next)				// If we have a resource record in our cache,
			if (rr->resrec.rdlength <= SmallRecordLimit &&					// which is small enough to sensibly fit in the packet
				ResourceRecordAnswersQuestion(&rr->resrec, q) &&			// which answers our question
				rr->TimeRcvd + TicksTTL(rr)/2 - m->timenow >= 0 &&			// and it is less than half-way to expiry
				rr->NextRequiredQuery - (m->timenow + q->ThisQInterval) > 0)// and we'll ask at least once again before NextRequiredQuery
				{
				// We forecast: compressed name (2) type (2) class (2) TTL (4) rdlength (2) rdata (n)
				forecast += 12 + rr->resrec.rdestimate;
				if (forecast >= 512) return(mDNSfalse);	// If this would add 512 bytes or more to the packet, don't accelerate
				}
		return(mDNStrue);
		}

	return(mDNSfalse);
	}

// How Standard Queries are generated:
// 1. The Question Section contains the question
// 2. The Additional Section contains answers we already know, to suppress duplicate responses

// How Probe Queries are generated:
// 1. The Question Section contains queries for the name we intend to use, with QType=ANY because
// if some other host is already using *any* records with this name, we want to know about it.
// 2. The Authority Section contains the proposed values we intend to use for one or more
// of our records with that name (analogous to the Update section of DNS Update packets)
// because if some other host is probing at the same time, we each want to know what the other is
// planning, in order to apply the tie-breaking rule to see who gets to use the name and who doesn't.

mDNSlocal void SendQueries(mDNS *const m)
	{
	mDNSu32 slot;
	CacheGroup *cg;
	CacheRecord *cr;
	AuthRecord *ar;
	int pktcount = 0;
	DNSQuestion *q;
	// For explanation of maxExistingQuestionInterval logic, see comments for maxExistingAnnounceInterval
	mDNSs32 maxExistingQuestionInterval = 0;
	const NetworkInterfaceInfo *intf = GetFirstActiveInterface(m->HostInterfaces);
	CacheRecord *KnownAnswerList = mDNSNULL;

	// 1. If time for a query, work out what we need to do
	if (m->timenow - m->NextScheduledQuery >= 0)
		{
		CacheRecord *rr;
		m->NextScheduledQuery = m->timenow + 0x78000000;

		// We're expecting to send a query anyway, so see if any expiring cache records are close enough
		// to their NextRequiredQuery to be worth batching them together with this one
		FORALL_CACHERECORDS(slot, cg, rr)
			if (rr->CRActiveQuestion && rr->UnansweredQueries < MaxUnansweredQueries)
				if (m->timenow + TicksTTL(rr)/50 - rr->NextRequiredQuery >= 0)
					{
					q = rr->CRActiveQuestion;
					ExpireDupSuppressInfoOnInterface(q->DupSuppress, m->timenow - TicksTTL(rr)/20, rr->resrec.InterfaceID);
					if (q->Target.type) q->SendQNow = mDNSInterfaceMark;	// If unicast query, mark it
					else if (q->SendQNow == mDNSNULL)               q->SendQNow = rr->resrec.InterfaceID;
					else if (q->SendQNow != rr->resrec.InterfaceID) q->SendQNow = mDNSInterfaceMark;
					}

		// Scan our list of questions to see which *unicast* queries need to be sent
		for (q = m->Questions; q; q=q->next)
			if (q->Target.type && (q->SendQNow || TimeToSendThisQuestion(q, m->timenow)))
				{
				mDNSu8       *qptr        = m->omsg.data;
				const mDNSu8 *const limit = m->omsg.data + sizeof(m->omsg.data);
				InitializeDNSMessage(&m->omsg.h, q->TargetQID, QueryFlags);
				qptr = putQuestion(&m->omsg, qptr, limit, &q->qname, q->qtype, q->qclass);
				mDNSSendDNSMessage(m, &m->omsg, qptr, mDNSInterface_Any, &q->Target, q->TargetPort, -1, mDNSNULL);
				q->ThisQInterval   *= 2;
				if (q->ThisQInterval > MaxQuestionInterval)
					q->ThisQInterval = MaxQuestionInterval;
				q->LastQTime        = m->timenow;
				q->LastQTxTime      = m->timenow;
				q->RecentAnswerPkts = 0;
				q->SendQNow         = mDNSNULL;
				m->ExpectUnicastResponse = m->timenow;
				}
	
		// Scan our list of questions to see which *multicast* queries we're definitely going to send
		for (q = m->Questions; q; q=q->next)
			if (!q->Target.type && TimeToSendThisQuestion(q, m->timenow))
				{
				q->SendQNow = mDNSInterfaceMark;		// Mark this question for sending on all interfaces
				if (maxExistingQuestionInterval < q->ThisQInterval)
					maxExistingQuestionInterval = q->ThisQInterval;
				}
	
		// Scan our list of questions
		// (a) to see if there are any more that are worth accelerating, and
		// (b) to update the state variables for *all* the questions we're going to send
		for (q = m->Questions; q; q=q->next)
			{
			if (q->SendQNow ||
				(!q->Target.type && ActiveQuestion(q) && q->ThisQInterval <= maxExistingQuestionInterval && AccelerateThisQuery(m,q)))
				{
				// If at least halfway to next query time, advance to next interval
				// If less than halfway to next query time, then
				// treat this as logically a repeat of the last transmission, without advancing the interval
				if (m->timenow - (q->LastQTime + q->ThisQInterval/2) >= 0)
					{
					q->SendQNow = mDNSInterfaceMark;	// Mark this question for sending on all interfaces
					q->ThisQInterval *= 2;
					if (q->ThisQInterval > MaxQuestionInterval)
						q->ThisQInterval = MaxQuestionInterval;
					else if (q->CurrentAnswers == 0 && q->ThisQInterval == InitialQuestionInterval * 8)
						{
						debugf("SendQueries: Zero current answers for %##s (%s); will reconfirm antecedents",
							q->qname.c, DNSTypeName(q->qtype));
						ReconfirmAntecedents(m, q);		// Sending third query, and no answers yet; time to begin doubting the source
						}
					}

				// Mark for sending. (If no active interfaces, then don't even try.)
				q->SendOnAll = (q->SendQNow == mDNSInterfaceMark);
				if (q->SendOnAll)
					{
					q->SendQNow  = !intf ? mDNSNULL : (q->InterfaceID) ? q->InterfaceID : intf->InterfaceID;
					q->LastQTime = m->timenow;
					}

				// If we recorded a duplicate suppression for this question less than half an interval ago,
				// then we consider it recent enough that we don't need to do an identical query ourselves.
				ExpireDupSuppressInfo(q->DupSuppress, m->timenow - q->ThisQInterval/2);

				q->LastQTxTime      = m->timenow;
				q->RecentAnswerPkts = 0;
				if (q->RequestUnicast) q->RequestUnicast--;
				}
			// For all questions (not just the ones we're sending) check what the next scheduled event will be
			SetNextQueryTime(m,q);
			}
		}

	// 2. Scan our authoritative RR list to see what probes we might need to send
	if (m->timenow - m->NextScheduledProbe >= 0)
		{
		m->NextScheduledProbe = m->timenow + 0x78000000;

		if (m->CurrentRecord) LogMsg("SendQueries:   ERROR m->CurrentRecord already set");
		m->CurrentRecord = m->ResourceRecords;
		while (m->CurrentRecord)
			{
			AuthRecord *rr = m->CurrentRecord;
			m->CurrentRecord = rr->next;
			if (rr->resrec.RecordType == kDNSRecordTypeUnique)			// For all records that are still probing...
				{
				// 1. If it's not reached its probe time, just make sure we update m->NextScheduledProbe correctly
				if (m->timenow - (rr->LastAPTime + rr->ThisAPInterval) < 0)
					{
					SetNextAnnounceProbeTime(m, rr);
					}
				// 2. else, if it has reached its probe time, mark it for sending and then update m->NextScheduledProbe correctly
				else if (rr->ProbeCount)
					{
					// Mark for sending. (If no active interfaces, then don't even try.)
					rr->SendRNow   = !intf ? mDNSNULL : (rr->resrec.InterfaceID) ? rr->resrec.InterfaceID : intf->InterfaceID;
					rr->LastAPTime = m->timenow;
					rr->ProbeCount--;
					SetNextAnnounceProbeTime(m, rr);
					}
				// else, if it has now finished probing, move it to state Verified,
				// and update m->NextScheduledResponse so it will be announced
				else
					{
					AuthRecord *r2;
					rr->resrec.RecordType     = kDNSRecordTypeVerified;
					rr->ThisAPInterval = DefaultAnnounceIntervalForTypeUnique;
					rr->LastAPTime     = m->timenow - DefaultAnnounceIntervalForTypeUnique;
					SetNextAnnounceProbeTime(m, rr);
					// If we have any records on our duplicate list that match this one, they have now also completed probing
					for (r2 = m->DuplicateRecords; r2; r2=r2->next)
						if (r2->resrec.RecordType == kDNSRecordTypeUnique && RecordIsLocalDuplicate(r2, rr))
							r2->ProbeCount = 0;
					AcknowledgeRecord(m, rr);
					}
				}
			}
		m->CurrentRecord = m->DuplicateRecords;
		while (m->CurrentRecord)
			{
			AuthRecord *rr = m->CurrentRecord;
			m->CurrentRecord = rr->next;
			if (rr->resrec.RecordType == kDNSRecordTypeUnique && rr->ProbeCount == 0)
				AcknowledgeRecord(m, rr);
			}
		}

	// 3. Now we know which queries and probes we're sending,
	// go through our interface list sending the appropriate queries on each interface
	while (intf)
		{
		AuthRecord *rr;
		mDNSu8 *queryptr = m->omsg.data;
		InitializeDNSMessage(&m->omsg.h, zeroID, QueryFlags);
		if (KnownAnswerList) verbosedebugf("SendQueries:   KnownAnswerList set... Will continue from previous packet");
		if (!KnownAnswerList)
			{
			// Start a new known-answer list
			CacheRecord **kalistptr = &KnownAnswerList;
			mDNSu32 answerforecast = 0;
			
			// Put query questions in this packet
			for (q = m->Questions; q; q=q->next)
				if (q->SendQNow == intf->InterfaceID)
					{
					debugf("SendQueries: %s question for %##s (%s) at %d forecast total %d",
						SuppressOnThisInterface(q->DupSuppress, intf) ? "Suppressing" : "Putting    ",
						q->qname.c, DNSTypeName(q->qtype), queryptr - m->omsg.data, queryptr + answerforecast - m->omsg.data);
					// If we're suppressing this question, or we successfully put it, update its SendQNow state
					if (SuppressOnThisInterface(q->DupSuppress, intf) ||
						BuildQuestion(m, &m->omsg, &queryptr, q, &kalistptr, &answerforecast))
							q->SendQNow = (q->InterfaceID || !q->SendOnAll) ? mDNSNULL : GetNextActiveInterfaceID(intf);
					}

			// Put probe questions in this packet
			for (rr = m->ResourceRecords; rr; rr=rr->next)
				if (rr->SendRNow == intf->InterfaceID)
					{
					mDNSBool ucast = (rr->ProbeCount >= DefaultProbeCountForTypeUnique-1) && m->CanReceiveUnicastOn5353;
					mDNSu16 ucbit = (mDNSu16)(ucast ? kDNSQClass_UnicastResponse : 0);
					const mDNSu8 *const limit = m->omsg.data + ((m->omsg.h.numQuestions) ? NormalMaxDNSMessageData : AbsoluteMaxDNSMessageData);
					mDNSu8 *newptr = putQuestion(&m->omsg, queryptr, limit, rr->resrec.name, kDNSQType_ANY, (mDNSu16)(rr->resrec.rrclass | ucbit));
					// We forecast: compressed name (2) type (2) class (2) TTL (4) rdlength (2) rdata (n)
					mDNSu32 forecast = answerforecast + 12 + rr->resrec.rdestimate;
					if (newptr && newptr + forecast < limit)
						{
						queryptr       = newptr;
						answerforecast = forecast;
						rr->SendRNow = (rr->resrec.InterfaceID) ? mDNSNULL : GetNextActiveInterfaceID(intf);
						rr->IncludeInProbe = mDNStrue;
						verbosedebugf("SendQueries:   Put Question %##s (%s) probecount %d",
							rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype), rr->ProbeCount);
						}
					else
						{
						verbosedebugf("SendQueries:   Retracting Question %##s (%s)",
							rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
						m->omsg.h.numQuestions--;
						}
					}
				}

		// Put our known answer list (either new one from this question or questions, or remainder of old one from last time)
		while (KnownAnswerList)
			{
			CacheRecord *rr = KnownAnswerList;
			mDNSu32 SecsSinceRcvd = ((mDNSu32)(m->timenow - rr->TimeRcvd)) / mDNSPlatformOneSecond;
			mDNSu8 *newptr = PutResourceRecordTTL(&m->omsg, queryptr, &m->omsg.h.numAnswers, &rr->resrec, rr->resrec.rroriginalttl - SecsSinceRcvd);
			if (newptr)
				{
				verbosedebugf("SendQueries:   Put %##s (%s) at %d - %d",
					rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype), queryptr - m->omsg.data, newptr - m->omsg.data);
				queryptr = newptr;
				KnownAnswerList = rr->NextInKAList;
				rr->NextInKAList = mDNSNULL;
				}
			else
				{
				// If we ran out of space and we have more than one question in the packet, that's an error --
				// we shouldn't have put more than one question if there was a risk of us running out of space.
				if (m->omsg.h.numQuestions > 1)
					LogMsg("SendQueries:   Put %d answers; No more space for known answers", m->omsg.h.numAnswers);
				m->omsg.h.flags.b[0] |= kDNSFlag0_TC;
				break;
				}
			}

		for (rr = m->ResourceRecords; rr; rr=rr->next)
			if (rr->IncludeInProbe)
				{
				mDNSu8 *newptr = PutResourceRecord(&m->omsg, queryptr, &m->omsg.h.numAuthorities, &rr->resrec);
				rr->IncludeInProbe = mDNSfalse;
				if (newptr) queryptr = newptr;
				else LogMsg("SendQueries:   How did we fail to have space for the Update record %##s (%s)?",
					rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
				}
		
		if (queryptr > m->omsg.data)
			{
			if ((m->omsg.h.flags.b[0] & kDNSFlag0_TC) && m->omsg.h.numQuestions > 1)
				LogMsg("SendQueries: Should not have more than one question (%d) in a truncated packet", m->omsg.h.numQuestions);
			debugf("SendQueries:   Sending %d Question%s %d Answer%s %d Update%s on %p",
				m->omsg.h.numQuestions,   m->omsg.h.numQuestions   == 1 ? "" : "s",
				m->omsg.h.numAnswers,     m->omsg.h.numAnswers     == 1 ? "" : "s",
				m->omsg.h.numAuthorities, m->omsg.h.numAuthorities == 1 ? "" : "s", intf->InterfaceID);
			if (intf->IPv4Available) mDNSSendDNSMessage(m, &m->omsg, queryptr, intf->InterfaceID, &AllDNSLinkGroup_v4, MulticastDNSPort, -1, mDNSNULL);
			if (intf->IPv6Available) mDNSSendDNSMessage(m, &m->omsg, queryptr, intf->InterfaceID, &AllDNSLinkGroup_v6, MulticastDNSPort, -1, mDNSNULL);
			if (!m->SuppressSending) m->SuppressSending = NonZeroTime(m->timenow + (mDNSPlatformOneSecond+9)/10);
			if (++pktcount >= 1000)
				{ LogMsg("SendQueries exceeded loop limit %d: giving up", pktcount); break; }
			// There might be more records left in the known answer list, or more questions to send
			// on this interface, so go around one more time and try again.
			}
		else	// Nothing more to send on this interface; go to next
			{
			const NetworkInterfaceInfo *next = GetFirstActiveInterface(intf->next);
			#if MDNS_DEBUGMSGS && 0
			const char *const msg = next ? "SendQueries:   Nothing more on %p; moving to %p" : "SendQueries:   Nothing more on %p";
			debugf(msg, intf, next);
			#endif
			intf = next;
			}
		}

	// 4. Final housekeeping
	
	// 4a. Debugging check: Make sure we announced all our records
	for (ar = m->ResourceRecords; ar; ar=ar->next)
		if (ar->SendRNow)
			{
			if (ar->resrec.InterfaceID != mDNSInterface_LocalOnly)
				LogMsg("SendQueries: No active interface to send: %s", ARDisplayString(m, ar));
			ar->SendRNow = mDNSNULL;
			}

	// 4b. When we have lingering cache records that we're keeping around for a few seconds in the hope
	// that their interface which went away might come back again, the logic will want to send queries
	// for those records, but we can't because their interface isn't here any more, so to keep the
	// state machine ticking over we just pretend we did so.
	// If the interface does not come back in time, the cache record will expire naturally
	FORALL_CACHERECORDS(slot, cg, cr)
		if (cr->CRActiveQuestion && cr->UnansweredQueries < MaxUnansweredQueries && m->timenow - cr->NextRequiredQuery >= 0)
			{
			cr->UnansweredQueries++;
			cr->CRActiveQuestion->SendQNow = mDNSNULL;
			SetNextCacheCheckTime(m, cr);
			}

	// 4c. Debugging check: Make sure we sent all our planned questions
	// Do this AFTER the lingering cache records check above, because that will prevent spurious warnings for questions
	// we legitimately couldn't send because the interface is no longer available
	for (q = m->Questions; q; q=q->next)
		if (q->SendQNow)
			{
			LogMsg("SendQueries: No active interface to send: %##s %s", q->qname.c, DNSTypeName(q->qtype));
			q->SendQNow = mDNSNULL;
			}
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - RR List Management & Task Management
#endif

// NOTE: AnswerQuestionWithResourceRecord can call a user callback, which may change the record list and/or question list.
// Any code walking either list must use the CurrentQuestion and/or CurrentRecord mechanism to protect against this.
mDNSlocal void AnswerQuestionWithResourceRecord(mDNS *const m, DNSQuestion *q, CacheRecord *rr, mDNSBool AddRecord)
	{
	verbosedebugf("AnswerQuestionWithResourceRecord:%4lu %s TTL%6lu %##s (%s)",
		q->CurrentAnswers, AddRecord ? "Add" : "Rmv", rr->resrec.rroriginalttl, rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));

	// Note: Use caution here. In the case of records with rr->DelayDelivery set, AnswerQuestionWithResourceRecord(... mDNStrue)
	// may be called twice, once when the record is received, and again when it's time to notify local clients.
	// If any counters or similar are added here, care must be taken to ensure that they are not double-incremented by this.

	rr->LastUsed = m->timenow;
	if (ActiveQuestion(q) && rr->CRActiveQuestion != q)
		{
		if (!rr->CRActiveQuestion) m->rrcache_active++;	// If not previously active, increment rrcache_active count
		rr->CRActiveQuestion = q;						// We know q is non-null
		SetNextCacheCheckTime(m, rr);
		}

	// If this is:
	// (a) a no-cache add, where we've already done at least one 'QM' query, or
	// (b) a normal add, where we have at least one unique-type answer,
	// then there's no need to keep polling the network.
	// (If we have an answer in the cache, then we'll automatically ask again in time to stop it expiring.)
	if ((AddRecord == 2 && !q->RequestUnicast) ||
		(AddRecord == 1 && (q->ExpectUnique || (rr->resrec.RecordType & kDNSRecordTypePacketUniqueMask))))
		if (ActiveQuestion(q))
			{
			q->LastQTime      = m->timenow;
			q->LastQTxTime    = m->timenow;
			q->RecentAnswerPkts = 0;
			q->ThisQInterval  = MaxQuestionInterval;
			q->RequestUnicast = mDNSfalse;
			}

	if (rr->DelayDelivery) return;		// We'll come back later when CacheRecordDeferredAdd() calls us

	m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
	if (q->QuestionCallback)
		q->QuestionCallback(m, q, &rr->resrec, AddRecord);
	m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
	// CAUTION: MUST NOT do anything more with q after calling q->QuestionCallback(), because the client's callback function
	// is allowed to do anything, including starting/stopping queries, registering/deregistering records, etc.
	// Right now the only routines that call AnswerQuestionWithResourceRecord() are CacheRecordAdd(), CacheRecordRmv()
	// and AnswerNewQuestion(), and all of them use the "m->CurrentQuestion" mechanism to protect against questions
	// being deleted out from under them.
	}

mDNSlocal void CacheRecordDeferredAdd(mDNS *const m, CacheRecord *rr)
	{
	rr->DelayDelivery = 0;		// Note, only need to call SetNextCacheCheckTime() when DelayDelivery is set, not when it's cleared
	if (m->CurrentQuestion) LogMsg("CacheRecordDeferredAdd ERROR m->CurrentQuestion already set");
	m->CurrentQuestion = m->Questions;
	while (m->CurrentQuestion && m->CurrentQuestion != m->NewQuestions)
		{
		DNSQuestion *q = m->CurrentQuestion;
		m->CurrentQuestion = q->next;
		if (ResourceRecordAnswersQuestion(&rr->resrec, q))
			AnswerQuestionWithResourceRecord(m, q, rr, mDNStrue);
		}
	m->CurrentQuestion = mDNSNULL;
	}

mDNSlocal mDNSs32 CheckForSoonToExpireRecords(mDNS *const m, const domainname *const name, const mDNSu32 namehash, const mDNSu32 slot)
	{
	const mDNSs32 threshhold = m->timenow + mDNSPlatformOneSecond;	// See if there are any records expiring within one second
	const mDNSs32 start      = m->timenow - 0x10000000;
	mDNSs32 delay = start;
	CacheGroup *cg = CacheGroupForName(m, slot, namehash, name);
	CacheRecord *rr;
	for (rr = cg ? cg->members : mDNSNULL; rr; rr=rr->next)
		if (rr->resrec.namehash == namehash && SameDomainName(rr->resrec.name, name))
			if (threshhold - RRExpireTime(rr) >= 0)		// If we have records about to expire within a second
				if (delay - RRExpireTime(rr) < 0)		// then delay until after they've been deleted
					delay = RRExpireTime(rr);
	if (delay - start > 0) return(NonZeroTime(delay));
	else return(0);
	}

// CacheRecordAdd is only called from mDNSCoreReceiveResponse, *never* directly as a result of a client API call.
// If new questions are created as a result of invoking client callbacks, they will be added to
// the end of the question list, and m->NewQuestions will be set to indicate the first new question.
// rr is a new CacheRecord just received into our cache
// (kDNSRecordTypePacketAns/PacketAnsUnique/PacketAdd/PacketAddUnique).
// NOTE: CacheRecordAdd calls AnswerQuestionWithResourceRecord which can call a user callback,
// which may change the record list and/or question list.
// Any code walking either list must use the CurrentQuestion and/or CurrentRecord mechanism to protect against this.
mDNSlocal void CacheRecordAdd(mDNS *const m, CacheRecord *rr)
	{
	if (m->CurrentQuestion) LogMsg("CacheRecordAdd ERROR m->CurrentQuestion already set");
	m->CurrentQuestion = m->Questions;
	while (m->CurrentQuestion && m->CurrentQuestion != m->NewQuestions)
		{
		DNSQuestion *q = m->CurrentQuestion;
		m->CurrentQuestion = q->next;
		if (ResourceRecordAnswersQuestion(&rr->resrec, q))
			{
			// If this question is one that's actively sending queries, and it's received ten answers within one
			// second of sending the last query packet, then that indicates some radical network topology change,
			// so reset its exponential backoff back to the start. We must be at least at the eight-second interval
			// to do this. If we're at the four-second interval, or less, there's not much benefit accelerating
			// because we will anyway send another query within a few seconds. The first reset query is sent out
			// randomized over the next four seconds to reduce possible synchronization between machines.
			if (q->LastAnswerPktNum != m->PktNum)
				{
				q->LastAnswerPktNum = m->PktNum;
				if (ActiveQuestion(q) && ++q->RecentAnswerPkts >= 10 &&
					q->ThisQInterval > InitialQuestionInterval*32 && m->timenow - q->LastQTxTime < mDNSPlatformOneSecond)
					{
					LogMsg("CacheRecordAdd: %##s (%s) got immediate answer burst; restarting exponential backoff sequence",
						q->qname.c, DNSTypeName(q->qtype));
					q->LastQTime      = m->timenow - InitialQuestionInterval + (mDNSs32)mDNSRandom((mDNSu32)mDNSPlatformOneSecond*4);
					q->ThisQInterval  = InitialQuestionInterval;
					SetNextQueryTime(m,q);
					}
				}
			verbosedebugf("CacheRecordAdd %p %##s (%s) %lu",
				rr, rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype), rr->resrec.rroriginalttl);
			q->CurrentAnswers++;
			if (rr->resrec.rdlength > SmallRecordLimit) q->LargeAnswers++;
			if (rr->resrec.RecordType & kDNSRecordTypePacketUniqueMask) q->UniqueAnswers++;
			if (q->CurrentAnswers > 4000)
				{
				static int msgcount = 0;
				if (msgcount++ < 10)
					LogMsg("CacheRecordAdd: %##s (%s) has %d answers; shedding records to resist DOS attack",
						q->qname.c, DNSTypeName(q->qtype), q->CurrentAnswers);
				rr->resrec.rroriginalttl = 1;
				rr->UnansweredQueries = MaxUnansweredQueries;
				}
			AnswerQuestionWithResourceRecord(m, q, rr, mDNStrue);
			// MUST NOT dereference q again after calling AnswerQuestionWithResourceRecord()
			}
		}
	m->CurrentQuestion = mDNSNULL;
	SetNextCacheCheckTime(m, rr);
	}

// NoCacheAnswer is only called from mDNSCoreReceiveResponse, *never* directly as a result of a client API call.
// If new questions are created as a result of invoking client callbacks, they will be added to
// the end of the question list, and m->NewQuestions will be set to indicate the first new question.
// rr is a new CacheRecord just received from the wire (kDNSRecordTypePacketAns/AnsUnique/Add/AddUnique)
// but we don't have any place to cache it. We'll deliver question 'add' events now, but we won't have any
// way to deliver 'remove' events in future, nor will we be able to include this in known-answer lists,
// so we immediately bump ThisQInterval up to MaxQuestionInterval to avoid pounding the network.
// NOTE: NoCacheAnswer calls AnswerQuestionWithResourceRecord which can call a user callback,
// which may change the record list and/or question list.
// Any code walking either list must use the CurrentQuestion and/or CurrentRecord mechanism to protect against this.
mDNSlocal void NoCacheAnswer(mDNS *const m, CacheRecord *rr)
	{
	LogMsg("No cache space: Delivering non-cached result for %##s", m->rec.r.resrec.name->c);
	if (m->CurrentQuestion) LogMsg("NoCacheAnswer ERROR m->CurrentQuestion already set");
	m->CurrentQuestion = m->Questions;
	while (m->CurrentQuestion)
		{
		DNSQuestion *q = m->CurrentQuestion;
		m->CurrentQuestion = q->next;
		if (ResourceRecordAnswersQuestion(&rr->resrec, q))
			AnswerQuestionWithResourceRecord(m, q, rr, 2);	// Value '2' indicates "don't expect 'remove' events for this"
		// MUST NOT dereference q again after calling AnswerQuestionWithResourceRecord()
		}
	m->CurrentQuestion = mDNSNULL;
	}

// CacheRecordRmv is only called from CheckCacheExpiration, which is called from mDNS_Execute
// If new questions are created as a result of invoking client callbacks, they will be added to
// the end of the question list, and m->NewQuestions will be set to indicate the first new question.
// rr is an existing cache CacheRecord that just expired and is being deleted
// (kDNSRecordTypePacketAns/PacketAnsUnique/PacketAdd/PacketAddUnique).
// NOTE: CacheRecordRmv calls AnswerQuestionWithResourceRecord which can call a user callback,
// which may change the record list and/or question list.
// Any code walking either list must use the CurrentQuestion and/or CurrentRecord mechanism to protect against this.
mDNSlocal void CacheRecordRmv(mDNS *const m, CacheRecord *rr)
	{
	if (m->CurrentQuestion) LogMsg("CacheRecordRmv ERROR m->CurrentQuestion already set");
	m->CurrentQuestion = m->Questions;
	while (m->CurrentQuestion && m->CurrentQuestion != m->NewQuestions)
		{
		DNSQuestion *q = m->CurrentQuestion;
		m->CurrentQuestion = q->next;
		if (ResourceRecordAnswersQuestion(&rr->resrec, q))
			{
			verbosedebugf("CacheRecordRmv %p %s", rr, CRDisplayString(m, rr));
			if (q->CurrentAnswers == 0)
				LogMsg("CacheRecordRmv ERROR: How can CurrentAnswers already be zero for %p %##s (%s)?",
					q, q->qname.c, DNSTypeName(q->qtype));
			else
				{
				q->CurrentAnswers--;
				if (rr->resrec.rdlength > SmallRecordLimit) q->LargeAnswers--;
				if (rr->resrec.RecordType & kDNSRecordTypePacketUniqueMask) q->UniqueAnswers--;
				}
			if (q->CurrentAnswers == 0)
				{
				debugf("CacheRecordRmv: Zero current answers for %##s (%s); will reconfirm antecedents",
					q->qname.c, DNSTypeName(q->qtype));
				ReconfirmAntecedents(m, q);
				}
			q->FlappingInterface = mDNSNULL;
			AnswerQuestionWithResourceRecord(m, q, rr, mDNSfalse);
			// MUST NOT dereference q again after calling AnswerQuestionWithResourceRecord()
			}
		}
	m->CurrentQuestion = mDNSNULL;
	}

mDNSlocal void ReleaseCacheEntity(mDNS *const m, CacheEntity *e)
	{
#if MACOSX_MDNS_MALLOC_DEBUGGING >= 1
	unsigned int i;
	for (i=0; i<sizeof(*e); i++) ((char*)e)[i] = 0xFF;
#endif
	e->next = m->rrcache_free;
	m->rrcache_free = e;
	m->rrcache_totalused--;
	}

mDNSlocal void ReleaseCacheGroup(mDNS *const m, CacheGroup **cp)
	{
	CacheEntity *e = (CacheEntity *)(*cp);
	//LogMsg("ReleaseCacheGroup: Releasing CacheGroup for %p, %##s", (*cp)->name->c, (*cp)->name->c);
	if ((*cp)->rrcache_tail != &(*cp)->members)
		LogMsg("ERROR: (*cp)->members == mDNSNULL but (*cp)->rrcache_tail != &(*cp)->members)");
	//if ((*cp)->name != (domainname*)((*cp)->namestorage))
	//	LogMsg("ReleaseCacheGroup: %##s, %p %p", (*cp)->name->c, (*cp)->name, (domainname*)((*cp)->namestorage));
	if ((*cp)->name != (domainname*)((*cp)->namestorage)) mDNSPlatformMemFree((*cp)->name);
	(*cp)->name = mDNSNULL;
	*cp = (*cp)->next;			// Cut record from list
	ReleaseCacheEntity(m, e);
	}

mDNSlocal void ReleaseCacheRecord(mDNS *const m, CacheRecord *r)
	{
	if (r->resrec.rdata && r->resrec.rdata != (RData*)&r->rdatastorage) mDNSPlatformMemFree(r->resrec.rdata);
	r->resrec.rdata = mDNSNULL;
	ReleaseCacheEntity(m, (CacheEntity *)r);
	}

// Note: We want to be careful that we deliver all the CacheRecordRmv calls before delivering
// CacheRecordDeferredAdd calls. The in-order nature of the cache lists ensures that all
// callbacks for old records are delivered before callbacks for newer records.
mDNSlocal void CheckCacheExpiration(mDNS *const m, CacheGroup *cg)
	{
	CacheRecord **rp = &cg->members;

	if (m->lock_rrcache) { LogMsg("CheckCacheExpiration ERROR! Cache already locked!"); return; }
	m->lock_rrcache = 1;

	while (*rp)
		{
		CacheRecord *const rr = *rp;
		mDNSs32 event = RRExpireTime(rr);
		if (m->timenow - event >= 0)	// If expired, delete it
			{
			*rp = rr->next;				// Cut it from the list
			verbosedebugf("CheckCacheExpiration: Deleting %s", CRDisplayString(m, rr));
			if (rr->CRActiveQuestion)	// If this record has one or more active questions, tell them it's going away
				{
				CacheRecordRmv(m, rr);
				m->rrcache_active--;
				}
			ReleaseCacheRecord(m, rr);
			}
		else							// else, not expired; see if we need to query
			{
			if (rr->DelayDelivery && rr->DelayDelivery - m->timenow > 0)
				event = rr->DelayDelivery;
			else
				{
				if (rr->DelayDelivery) CacheRecordDeferredAdd(m, rr);
				if (rr->CRActiveQuestion && rr->UnansweredQueries < MaxUnansweredQueries)
					{
					if (m->timenow - rr->NextRequiredQuery < 0)		// If not yet time for next query
						event = rr->NextRequiredQuery;				// then just record when we want the next query
					else											// else trigger our question to go out now
						{
						// Set NextScheduledQuery to timenow so that SendQueries() will run.
						// SendQueries() will see that we have records close to expiration, and send FEQs for them.
						m->NextScheduledQuery = m->timenow;
						// After sending the query we'll increment UnansweredQueries and call SetNextCacheCheckTime(),
						// which will correctly update m->NextCacheCheck for us.
						event = m->timenow + 0x3FFFFFFF;
						}
					}
				}
			verbosedebugf("CheckCacheExpiration:%6d %5d %s",
				(event-m->timenow) / mDNSPlatformOneSecond, CacheCheckGracePeriod(rr), CRDisplayString(m, rr));
			if (m->NextCacheCheck - (event + CacheCheckGracePeriod(rr)) > 0)
				m->NextCacheCheck = (event + CacheCheckGracePeriod(rr));
			rp = &rr->next;
			}
		}
	if (cg->rrcache_tail != rp) verbosedebugf("CheckCacheExpiration: Updating CacheGroup tail from %p to %p", cg->rrcache_tail, rp);
	cg->rrcache_tail = rp;
	m->lock_rrcache = 0;
	}

mDNSlocal void AnswerNewQuestion(mDNS *const m)
	{
	mDNSBool ShouldQueryImmediately = mDNStrue;
	CacheRecord *rr;
	DNSQuestion *q = m->NewQuestions;		// Grab the question we're going to answer
	const mDNSu32 slot = HashSlot(&q->qname);
	CacheGroup *cg = CacheGroupForName(m, slot, q->qnamehash, &q->qname);

	verbosedebugf("AnswerNewQuestion: Answering %##s (%s)", q->qname.c, DNSTypeName(q->qtype));

	if (cg) CheckCacheExpiration(m, cg);
	m->NewQuestions = q->next;				// Advance NewQuestions to the next *after* calling CheckCacheExpiration();

	if (m->lock_rrcache) LogMsg("AnswerNewQuestion ERROR! Cache already locked!");
	// This should be safe, because calling the client's question callback may cause the
	// question list to be modified, but should not ever cause the rrcache list to be modified.
	// If the client's question callback deletes the question, then m->CurrentQuestion will	
	// be advanced, and we'll exit out of the loop
	m->lock_rrcache = 1;
	if (m->CurrentQuestion) LogMsg("AnswerNewQuestion ERROR m->CurrentQuestion already set");
	m->CurrentQuestion = q;		// Indicate which question we're answering, so we'll know if it gets deleted

	if (q->InterfaceID == mDNSInterface_Any)	// If 'mDNSInterface_Any' question, see if we want to tell it about LocalOnly records
		{
		if (m->CurrentRecord) LogMsg("AnswerNewLocalOnlyQuestion ERROR m->CurrentRecord already set");
		m->CurrentRecord = m->ResourceRecords;
		while (m->CurrentRecord && m->CurrentRecord != m->NewLocalRecords)
			{
			AuthRecord *rr = m->CurrentRecord;
			m->CurrentRecord = rr->next;
			if (rr->resrec.InterfaceID == mDNSInterface_LocalOnly)
				if (ResourceRecordAnswersQuestion(&rr->resrec, q))
					{
					AnswerLocalOnlyQuestionWithResourceRecord(m, q, rr, mDNStrue);
					// MUST NOT dereference q again after calling AnswerLocalOnlyQuestionWithResourceRecord()
					if (m->CurrentQuestion != q) break;		// If callback deleted q, then we're finished here
					}
			}
		m->CurrentRecord   = mDNSNULL;
		}

	for (rr = cg ? cg->members : mDNSNULL; rr; rr=rr->next)
		if (ResourceRecordAnswersQuestion(&rr->resrec, q))
			{
			// SecsSinceRcvd is whole number of elapsed seconds, rounded down
			mDNSu32 SecsSinceRcvd = ((mDNSu32)(m->timenow - rr->TimeRcvd)) / mDNSPlatformOneSecond;
			if (rr->resrec.rroriginalttl <= SecsSinceRcvd)
				{
				LogMsg("AnswerNewQuestion: How is rr->resrec.rroriginalttl %lu <= SecsSinceRcvd %lu for %##s (%s)",
					rr->resrec.rroriginalttl, SecsSinceRcvd, rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
				continue;	// Go to next one in loop
				}

			// If this record set is marked unique, then that means we can reasonably assume we have the whole set
			// -- we don't need to rush out on the network and query immediately to see if there are more answers out there
			if ((rr->resrec.RecordType & kDNSRecordTypePacketUniqueMask) || (q->ExpectUnique))
				ShouldQueryImmediately = mDNSfalse;
			q->CurrentAnswers++;
			if (rr->resrec.rdlength > SmallRecordLimit) q->LargeAnswers++;
			if (rr->resrec.RecordType & kDNSRecordTypePacketUniqueMask) q->UniqueAnswers++;
			AnswerQuestionWithResourceRecord(m, q, rr, mDNStrue);
			// MUST NOT dereference q again after calling AnswerQuestionWithResourceRecord()
			if (m->CurrentQuestion != q) break;		// If callback deleted q, then we're finished here
			}
		else if (RRTypeIsAddressType(rr->resrec.rrtype) && RRTypeIsAddressType(q->qtype))
			if (rr->resrec.namehash == q->qnamehash && SameDomainName(rr->resrec.name, &q->qname))
				ShouldQueryImmediately = mDNSfalse;

	if (ShouldQueryImmediately && m->CurrentQuestion == q)
		{
		q->ThisQInterval  = InitialQuestionInterval;
		q->LastQTime      = m->timenow - q->ThisQInterval;
		m->NextScheduledQuery = m->timenow;
		}
	m->CurrentQuestion = mDNSNULL;
	m->lock_rrcache = 0;
	}

// When a NewLocalOnlyQuestion is created, AnswerNewLocalOnlyQuestion runs though our ResourceRecords delivering any
// appropriate answers, stopping if it reaches a NewLocalRecord -- these will be handled by AnswerLocalQuestions
mDNSlocal void AnswerNewLocalOnlyQuestion(mDNS *const m)
	{
	DNSQuestion *q = m->NewLocalOnlyQuestions;		// Grab the question we're going to answer
	m->NewLocalOnlyQuestions = q->next;				// Advance NewQuestions to the next (if any)

	debugf("AnswerNewLocalOnlyQuestion: Answering %##s (%s)", q->qname.c, DNSTypeName(q->qtype));

	if (m->CurrentQuestion) LogMsg("AnswerNewLocalOnlyQuestion ERROR m->CurrentQuestion already set");
	m->CurrentQuestion = q;		// Indicate which question we're answering, so we'll know if it gets deleted

	if (m->CurrentRecord) LogMsg("AnswerNewLocalOnlyQuestion ERROR m->CurrentRecord already set");
	m->CurrentRecord = m->ResourceRecords;
	while (m->CurrentRecord && m->CurrentRecord != m->NewLocalRecords)
		{
		AuthRecord *rr = m->CurrentRecord;
		m->CurrentRecord = rr->next;
		if (ResourceRecordAnswersQuestion(&rr->resrec, q))
			{
			AnswerLocalOnlyQuestionWithResourceRecord(m, q, rr, mDNStrue);
			// MUST NOT dereference q again after calling AnswerLocalOnlyQuestionWithResourceRecord()
			if (m->CurrentQuestion != q) break;		// If callback deleted q, then we're finished here
			}
		}

	m->CurrentQuestion = mDNSNULL;
	m->CurrentRecord   = mDNSNULL;
	}

mDNSlocal CacheEntity *GetCacheEntity(mDNS *const m, const CacheGroup *const PreserveCG)
	{
	CacheEntity *e = mDNSNULL;

	if (m->lock_rrcache) { LogMsg("GetFreeCacheRR ERROR! Cache already locked!"); return(mDNSNULL); }
	m->lock_rrcache = 1;
	
	// If we have no free records, ask the client layer to give us some more memory
	if (!m->rrcache_free && m->MainCallback)
		{
		if (m->rrcache_totalused != m->rrcache_size)
			LogMsg("GetFreeCacheRR: count mismatch: m->rrcache_totalused %lu != m->rrcache_size %lu",
				m->rrcache_totalused, m->rrcache_size);
		
		// We don't want to be vulnerable to a malicious attacker flooding us with an infinite
		// number of bogus records so that we keep growing our cache until the machine runs out of memory.
		// To guard against this, if we're actively using less than 1/32 of our cache, then we
		// purge all the unused records and recycle them, instead of allocating more memory.
		if (m->rrcache_size >= 512 && m->rrcache_size / 32 > m->rrcache_active)
			debugf("Possible denial-of-service attack in progress: m->rrcache_size %lu; m->rrcache_active %lu",
				m->rrcache_size, m->rrcache_active);
		else
			{
			m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
			m->MainCallback(m, mStatus_GrowCache);
			m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
			}
		}
	
	// If we still have no free records, recycle all the records we can.
	// Enumerating the entire cache is moderately expensive, so when we do it, we reclaim all the records we can in one pass.
	if (!m->rrcache_free)
		{
		#if MDNS_DEBUGMSGS
		mDNSu32 oldtotalused = m->rrcache_totalused;
		#endif
		mDNSu32 slot;
		for (slot = 0; slot < CACHE_HASH_SLOTS; slot++)
			{
			CacheGroup **cp = &m->rrcache_hash[slot];
			while (*cp)
				{
				CacheRecord **rp = &(*cp)->members;
				while (*rp)
					{
					// Records that answer still-active questions are not candidates for recycling
					// Records that are currently linked into the CacheFlushRecords list may not be recycled, or we'll crash
					if ((*rp)->CRActiveQuestion || (*rp)->NextInCFList)
						rp=&(*rp)->next;
					else
						{
						CacheRecord *rr = *rp;
						*rp = (*rp)->next;			// Cut record from list
						ReleaseCacheRecord(m, rr);
						}
					}
				if ((*cp)->rrcache_tail != rp)
					verbosedebugf("GetFreeCacheRR: Updating rrcache_tail[%lu] from %p to %p", slot, (*cp)->rrcache_tail, rp);
				(*cp)->rrcache_tail = rp;
				if ((*cp)->members || (*cp)==PreserveCG) cp=&(*cp)->next;
				else ReleaseCacheGroup(m, cp);
				}
			}
		#if MDNS_DEBUGMSGS
		debugf("Clear unused records; m->rrcache_totalused was %lu; now %lu", oldtotalused, m->rrcache_totalused);
		#endif
		}

	if (m->rrcache_free)	// If there are records in the free list, take one
		{
		e = m->rrcache_free;
		m->rrcache_free = e->next;
		if (++m->rrcache_totalused >= m->rrcache_report)
			{
			debugf("RR Cache now using %ld objects", m->rrcache_totalused);
			if (m->rrcache_report < 100) m->rrcache_report += 10;
			else                         m->rrcache_report += 100;
			}
		mDNSPlatformMemZero(e, sizeof(*e));
		}

	m->lock_rrcache = 0;

	return(e);
	}

mDNSlocal CacheRecord *GetCacheRecord(mDNS *const m, CacheGroup *cg, mDNSu16 RDLength)
	{
	CacheRecord *r = (CacheRecord *)GetCacheEntity(m, cg);
	if (r)
		{
		r->resrec.rdata = (RData*)&r->rdatastorage;	// By default, assume we're usually going to be using local storage
		if (RDLength > InlineCacheRDSize)			// If RDLength is too big, allocate extra storage
			{
			r->resrec.rdata = (RData*)mDNSPlatformMemAllocate(sizeofRDataHeader + RDLength);
			if (r->resrec.rdata) r->resrec.rdata->MaxRDLength = r->resrec.rdlength = RDLength;
			else { ReleaseCacheEntity(m, (CacheEntity*)r); r = mDNSNULL; }
			}
		}
	return(r);
	}

mDNSlocal CacheGroup *GetCacheGroup(mDNS *const m, const mDNSu32 slot, const ResourceRecord *const rr)
	{
	mDNSu16 namelen = DomainNameLength(rr->name);
	CacheGroup *cg = (CacheGroup*)GetCacheEntity(m, mDNSNULL);
	if (!cg) { LogMsg("GetCacheGroup: Failed to allocate memory for %##s", rr->name->c); return(mDNSNULL); }
	cg->next         = m->rrcache_hash[slot];
	cg->namehash     = rr->namehash;
	cg->members      = mDNSNULL;
	cg->rrcache_tail = &cg->members;
	cg->name         = (domainname*)cg->namestorage;
	//LogMsg("GetCacheGroup: %-10s %d-byte cache name %##s",
	//	(namelen > InlineCacheGroupNameSize) ? "Allocating" : "Inline", namelen, rr->name->c);
	if (namelen > InlineCacheGroupNameSize) cg->name = mDNSPlatformMemAllocate(namelen);
	if (!cg->name)
		{
		LogMsg("GetCacheGroup: Failed to allocate name storage for %##s", rr->name->c);
		ReleaseCacheEntity(m, (CacheEntity*)cg);
		return(mDNSNULL);
		}
	AssignDomainName(cg->name, rr->name);

	if (CacheGroupForRecord(m, slot, rr)) LogMsg("GetCacheGroup: Already have CacheGroup for %##s", rr->name->c);
	m->rrcache_hash[slot] = cg;
	if (CacheGroupForRecord(m, slot, rr) != cg) LogMsg("GetCacheGroup: Not finding CacheGroup for %##s", rr->name->c);
	
	return(cg);
	}

mDNSlocal void PurgeCacheResourceRecord(mDNS *const m, CacheRecord *rr)
	{
	// Make sure we mark this record as thoroughly expired -- we don't ever want to give
	// a positive answer using an expired record (e.g. from an interface that has gone away).
	// We don't want to clear CRActiveQuestion here, because that would leave the record subject to
	// summary deletion without giving the proper callback to any questions that are monitoring it.
	// By setting UnansweredQueries to MaxUnansweredQueries we ensure it won't trigger any further expiration queries.
	rr->TimeRcvd          = m->timenow - mDNSPlatformOneSecond * 60;
	rr->UnansweredQueries = MaxUnansweredQueries;
	rr->resrec.rroriginalttl     = 0;
	SetNextCacheCheckTime(m, rr);
	}

mDNSexport mDNSs32 mDNS_TimeNow(const mDNS *const m)
	{
	mDNSs32 time;
	mDNSPlatformLock(m);
	if (m->mDNS_busy)
		{
		LogMsg("mDNS_TimeNow called while holding mDNS lock. This is incorrect. Code protected by lock should just use m->timenow.");
		if (!m->timenow) LogMsg("mDNS_TimeNow: m->mDNS_busy is %ld but m->timenow not set", m->mDNS_busy);
		}
	
	if (m->timenow) time = m->timenow;
	else            time = mDNS_TimeNow_NoLock(m);
	mDNSPlatformUnlock(m);
	return(time);
	}

mDNSexport mDNSs32 mDNS_Execute(mDNS *const m)
	{
	mDNS_Lock(m);	// Must grab lock before trying to read m->timenow

	if (m->timenow - m->NextScheduledEvent >= 0)
		{
		int i;

		verbosedebugf("mDNS_Execute");
		if (m->CurrentQuestion) LogMsg("mDNS_Execute: ERROR! m->CurrentQuestion already set");
	
		// 1. If we're past the probe suppression time, we can clear it
		if (m->SuppressProbes && m->timenow - m->SuppressProbes >= 0) m->SuppressProbes = 0;
	
		// 2. If it's been more than ten seconds since the last probe failure, we can clear the counter
		if (m->NumFailedProbes && m->timenow - m->ProbeFailTime >= mDNSPlatformOneSecond * 10) m->NumFailedProbes = 0;
		
		// 3. Purge our cache of stale old records
		if (m->rrcache_size && m->timenow - m->NextCacheCheck >= 0)
			{
			mDNSu32 slot;
			m->NextCacheCheck = m->timenow + 0x3FFFFFFF;
			for (slot = 0; slot < CACHE_HASH_SLOTS; slot++)
				{
				CacheGroup **cp = &m->rrcache_hash[slot];
				while (*cp)
					{
					CheckCacheExpiration(m, *cp);
					if ((*cp)->members) cp=&(*cp)->next;
					else ReleaseCacheGroup(m, cp);
					}
				}
			LogOperation("Cache checked. Next in %ld ticks", m->NextCacheCheck - m->timenow);
			}
	
		// 4. See if we can answer any of our new local questions from the cache
		for (i=0; m->NewQuestions && i<1000; i++)
			{
			if (m->NewQuestions->DelayAnswering && m->timenow - m->NewQuestions->DelayAnswering < 0) break;
			AnswerNewQuestion(m);
			}
		if (i >= 1000) LogMsg("mDNS_Execute: AnswerNewQuestion exceeded loop limit");
		
		for (i=0; m->NewLocalOnlyQuestions && i<1000; i++) AnswerNewLocalOnlyQuestion(m);
		if (i >= 1000) LogMsg("mDNS_Execute: AnswerNewLocalOnlyQuestion exceeded loop limit");

		for (i=0; i<1000 && m->NewLocalRecords && LocalRecordReady(m->NewLocalRecords); i++)
			{
			AuthRecord *rr = m->NewLocalRecords;
			m->NewLocalRecords = m->NewLocalRecords->next;
			AnswerLocalQuestions(m, rr, mDNStrue);
			}
		if (i >= 1000) LogMsg("mDNS_Execute: AnswerForNewLocalRecords exceeded loop limit");

		// 5. See what packets we need to send
		if (m->mDNSPlatformStatus != mStatus_NoError || m->SleepState) DiscardDeregistrations(m);
		else if (m->SuppressSending == 0 || m->timenow - m->SuppressSending >= 0)
			{
			// If the platform code is ready, and we're not suppressing packet generation right now
			// then send our responses, probes, and questions.
			// We check the cache first, because there might be records close to expiring that trigger questions to refresh them.
			// We send queries next, because there might be final-stage probes that complete their probing here, causing
			// them to advance to announcing state, and we want those to be included in any announcements we send out.
			// Finally, we send responses, including the previously mentioned records that just completed probing.
			m->SuppressSending = 0;
	
			// 6. Send Query packets. This may cause some probing records to advance to announcing state
			if (m->timenow - m->NextScheduledQuery >= 0 || m->timenow - m->NextScheduledProbe >= 0) SendQueries(m);
			if (m->timenow - m->NextScheduledQuery >= 0)
				{
				LogMsg("mDNS_Execute: SendQueries didn't send all its queries; will try again in one second");
				m->NextScheduledQuery = m->timenow + mDNSPlatformOneSecond;
				}
			if (m->timenow - m->NextScheduledProbe >= 0)
				{
				LogMsg("mDNS_Execute: SendQueries didn't send all its probes; will try again in one second");
				m->NextScheduledProbe = m->timenow + mDNSPlatformOneSecond;
				}
	
			// 7. Send Response packets, including probing records just advanced to announcing state
			if (m->timenow - m->NextScheduledResponse >= 0) SendResponses(m);
			if (m->timenow - m->NextScheduledResponse >= 0)
				{
				LogMsg("mDNS_Execute: SendResponses didn't send all its responses; will try again in one second");
				m->NextScheduledResponse = m->timenow + mDNSPlatformOneSecond;
				}
			}

		// Clear RandomDelay values, ready to pick a new different value next time
		m->RandomQueryDelay     = 0;
		m->RandomReconfirmDelay = 0;
		}

	// Note about multi-threaded systems:
	// On a multi-threaded system, some other thread could run right after the mDNS_Unlock(),
	// performing mDNS API operations that change our next scheduled event time.
	//
	// On multi-threaded systems (like the current Windows implementation) that have a single main thread
	// calling mDNS_Execute() (and other threads allowed to call mDNS API routines) it is the responsibility
	// of the mDNSPlatformUnlock() routine to signal some kind of stateful condition variable that will
	// signal whatever blocking primitive the main thread is using, so that it will wake up and execute one
	// more iteration of its loop, and immediately call mDNS_Execute() again. The signal has to be stateful
	// in the sense that if the main thread has not yet entered its blocking primitive, then as soon as it
	// does, the state of the signal will be noticed, causing the blocking primitive to return immediately
	// without blocking. This avoids the race condition between the signal from the other thread arriving
	// just *before* or just *after* the main thread enters the blocking primitive.
	//
	// On multi-threaded systems (like the current Mac OS 9 implementation) that are entirely timer-driven,
	// with no main mDNS_Execute() thread, it is the responsibility of the mDNSPlatformUnlock() routine to
	// set the timer according to the m->NextScheduledEvent value, and then when the timer fires, the timer
	// callback function should call mDNS_Execute() (and ignore the return value, which may already be stale
	// by the time it gets to the timer callback function).

#ifndef UNICAST_DISABLED
	uDNS_Execute(m);
#endif
	mDNS_Unlock(m);		// Calling mDNS_Unlock is what gives m->NextScheduledEvent its new value
	return(m->NextScheduledEvent);
	}

// Call mDNSCoreMachineSleep(m, mDNStrue) when the machine is about to go to sleep.
// Call mDNSCoreMachineSleep(m, mDNSfalse) when the machine is has just woken up.
// Normally, the platform support layer below mDNSCore should call this, not the client layer above.
// Note that sleep/wake calls do not have to be paired one-for-one; it is acceptable to call
// mDNSCoreMachineSleep(m, mDNSfalse) any time there is reason to believe that the machine may have just
// found itself in a new network environment. For example, if the Ethernet hardware indicates that the
// cable has just been connected, the platform support layer should call mDNSCoreMachineSleep(m, mDNSfalse)
// to make mDNSCore re-issue its outstanding queries, probe for record uniqueness, etc.
// While it is safe to call mDNSCoreMachineSleep(m, mDNSfalse) at any time, it does cause extra network
// traffic, so it should only be called when there is legitimate reason to believe the machine
// may have become attached to a new network.
mDNSexport void mDNSCoreMachineSleep(mDNS *const m, mDNSBool sleepstate)
	{
	AuthRecord *rr;

	mDNS_Lock(m);

	m->SleepState = sleepstate;
	LogOperation("%s at %ld", sleepstate ? "Sleeping" : "Waking", m->timenow);

	if (sleepstate)
		{
#ifndef UNICAST_DISABLED
		uDNS_Sleep(m);
#endif		
		// Mark all the records we need to deregister and send them
		for (rr = m->ResourceRecords; rr; rr=rr->next)
			if (rr->resrec.RecordType == kDNSRecordTypeShared && rr->RequireGoodbye)
				rr->ImmedAnswer = mDNSInterfaceMark;
		SendResponses(m);
		}
	else
		{
		DNSQuestion *q;
		mDNSu32 slot;
		CacheGroup *cg;
		CacheRecord *cr;

#ifndef UNICAST_DISABLED
		uDNS_Wake(m);
#endif
        // 1. Retrigger all our questions
		for (q = m->Questions; q; q=q->next)				// Scan our list of questions
			if (ActiveQuestion(q))
				{
				q->ThisQInterval    = InitialQuestionInterval;	// MUST be > zero for an active question
				q->RequestUnicast   = 2;						// Set to 2 because is decremented once *before* we check it
				q->LastQTime        = m->timenow - q->ThisQInterval;
				q->RecentAnswerPkts = 0;
				ExpireDupSuppressInfo(q->DupSuppress, m->timenow);
				m->NextScheduledQuery = m->timenow;
				}

		// 2. Re-validate our cache records
		m->NextCacheCheck  = m->timenow;
		FORALL_CACHERECORDS(slot, cg, cr)
			mDNS_Reconfirm_internal(m, cr, kDefaultReconfirmTimeForWake);

		// 3. Retrigger probing and announcing for all our authoritative records
		for (rr = m->ResourceRecords; rr; rr=rr->next)
			{
			if (rr->resrec.RecordType == kDNSRecordTypeVerified && !rr->DependentOn) rr->resrec.RecordType = kDNSRecordTypeUnique;
			rr->ProbeCount     = DefaultProbeCountForRecordType(rr->resrec.RecordType);
			rr->AnnounceCount  = InitialAnnounceCount;
			rr->ThisAPInterval = DefaultAPIntervalForRecordType(rr->resrec.RecordType);
			InitializeLastAPTime(m, rr);
			}
		}

	mDNS_Unlock(m);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Packet Reception Functions
#endif

#define MustSendRecord(RR) ((RR)->NR_AnswerTo || (RR)->NR_AdditionalTo)

mDNSlocal mDNSu8 *GenerateUnicastResponse(const DNSMessage *const query, const mDNSu8 *const end,
	const mDNSInterfaceID InterfaceID, mDNSBool LegacyQuery, DNSMessage *const response, AuthRecord *ResponseRecords)
	{
	mDNSu8          *responseptr     = response->data;
	const mDNSu8    *const limit     = response->data + sizeof(response->data);
	const mDNSu8    *ptr             = query->data;
	AuthRecord  *rr;
	mDNSu32          maxttl = 0x70000000;
	int i;

	// Initialize the response fields so we can answer the questions
	InitializeDNSMessage(&response->h, query->h.id, ResponseFlags);

	// ***
	// *** 1. Write out the list of questions we are actually going to answer with this packet
	// ***
	if (LegacyQuery)
		{
		maxttl = 10;
		for (i=0; i<query->h.numQuestions; i++)						// For each question...
			{
			DNSQuestion q;
			ptr = getQuestion(query, ptr, end, InterfaceID, &q);	// get the question...
			if (!ptr) return(mDNSNULL);
	
			for (rr=ResponseRecords; rr; rr=rr->NextResponse)		// and search our list of proposed answers
				{
				if (rr->NR_AnswerTo == ptr)							// If we're going to generate a record answering this question
					{												// then put the question in the question section
					responseptr = putQuestion(response, responseptr, limit, &q.qname, q.qtype, q.qclass);
					if (!responseptr) { debugf("GenerateUnicastResponse: Ran out of space for questions!"); return(mDNSNULL); }
					break;		// break out of the ResponseRecords loop, and go on to the next question
					}
				}
			}
	
		if (response->h.numQuestions == 0) { LogMsg("GenerateUnicastResponse: ERROR! Why no questions?"); return(mDNSNULL); }
		}

	// ***
	// *** 2. Write Answers
	// ***
	for (rr=ResponseRecords; rr; rr=rr->NextResponse)
		if (rr->NR_AnswerTo)
			{
			mDNSu8 *p = PutResourceRecordCappedTTL(response, responseptr, &response->h.numAnswers, &rr->resrec, maxttl);
			if (p) responseptr = p;
			else { debugf("GenerateUnicastResponse: Ran out of space for answers!"); response->h.flags.b[0] |= kDNSFlag0_TC; }
			}

	// ***
	// *** 3. Write Additionals
	// ***
	for (rr=ResponseRecords; rr; rr=rr->NextResponse)
		if (rr->NR_AdditionalTo && !rr->NR_AnswerTo)
			{
			mDNSu8 *p = PutResourceRecordCappedTTL(response, responseptr, &response->h.numAdditionals, &rr->resrec, maxttl);
			if (p) responseptr = p;
			else debugf("GenerateUnicastResponse: No more space for additionals");
			}

	return(responseptr);
	}

// AuthRecord *our is our Resource Record
// CacheRecord *pkt is the Resource Record from the response packet we've witnessed on the network
// Returns 0 if there is no conflict
// Returns +1 if there was a conflict and we won
// Returns -1 if there was a conflict and we lost and have to rename
mDNSlocal int CompareRData(AuthRecord *our, CacheRecord *pkt)
	{
	mDNSu8 ourdata[256], *ourptr = ourdata, *ourend;
	mDNSu8 pktdata[256], *pktptr = pktdata, *pktend;
	if (!our) { LogMsg("CompareRData ERROR: our is NULL"); return(+1); }
	if (!pkt) { LogMsg("CompareRData ERROR: pkt is NULL"); return(+1); }

	ourend = putRData(mDNSNULL, ourdata, ourdata + sizeof(ourdata), &our->resrec);
	pktend = putRData(mDNSNULL, pktdata, pktdata + sizeof(pktdata), &pkt->resrec);
	while (ourptr < ourend && pktptr < pktend && *ourptr == *pktptr) { ourptr++; pktptr++; }
	if (ourptr >= ourend && pktptr >= pktend) return(0);			// If data identical, not a conflict

	if (ourptr >= ourend) return(-1);								// Our data ran out first; We lost
	if (pktptr >= pktend) return(+1);								// Packet data ran out first; We won
	if (*pktptr > *ourptr) return(-1);								// Our data is numerically lower; We lost
	if (*pktptr < *ourptr) return(+1);								// Packet data is numerically lower; We won
	
	LogMsg("CompareRData ERROR: Invalid state");
	return(-1);
	}

// See if we have an authoritative record that's identical to this packet record,
// whose canonical DependentOn record is the specified master record.
// The DependentOn pointer is typically used for the TXT record of service registrations
// It indicates that there is no inherent conflict detection for the TXT record
// -- it depends on the SRV record to resolve name conflicts
// If we find any identical ResourceRecords in our authoritative list, then follow their DependentOn
// pointer chain (if any) to make sure we reach the canonical DependentOn record
// If the record has no DependentOn, then just return that record's pointer
// Returns NULL if we don't have any local RRs that are identical to the one from the packet
mDNSlocal mDNSBool MatchDependentOn(const mDNS *const m, const CacheRecord *const pktrr, const AuthRecord *const master)
	{
	const AuthRecord *r1;
	for (r1 = m->ResourceRecords; r1; r1=r1->next)
		{
		if (IdenticalResourceRecord(&r1->resrec, &pktrr->resrec))
			{
			const AuthRecord *r2 = r1;
			while (r2->DependentOn) r2 = r2->DependentOn;
			if (r2 == master) return(mDNStrue);
			}
		}
	for (r1 = m->DuplicateRecords; r1; r1=r1->next)
		{
		if (IdenticalResourceRecord(&r1->resrec, &pktrr->resrec))
			{
			const AuthRecord *r2 = r1;
			while (r2->DependentOn) r2 = r2->DependentOn;
			if (r2 == master) return(mDNStrue);
			}
		}
	return(mDNSfalse);
	}

// Find the canonical RRSet pointer for this RR received in a packet.
// If we find any identical AuthRecord in our authoritative list, then follow its RRSet
// pointers (if any) to make sure we return the canonical member of this name/type/class
// Returns NULL if we don't have any local RRs that are identical to the one from the packet
mDNSlocal const AuthRecord *FindRRSet(const mDNS *const m, const CacheRecord *const pktrr)
	{
	const AuthRecord *rr;
	for (rr = m->ResourceRecords; rr; rr=rr->next)
		{
		if (IdenticalResourceRecord(&rr->resrec, &pktrr->resrec))
			{
			while (rr->RRSet && rr != rr->RRSet) rr = rr->RRSet;
			return(rr);
			}
		}
	return(mDNSNULL);
	}

// PacketRRConflict is called when we've received an RR (pktrr) which has the same name
// as one of our records (our) but different rdata.
// 1. If our record is not a type that's supposed to be unique, we don't care.
// 2a. If our record is marked as dependent on some other record for conflict detection, ignore this one.
// 2b. If the packet rr exactly matches one of our other RRs, and *that* record's DependentOn pointer
//     points to our record, ignore this conflict (e.g. the packet record matches one of our
//     TXT records, and that record is marked as dependent on 'our', its SRV record).
// 3. If we have some *other* RR that exactly matches the one from the packet, and that record and our record
//    are members of the same RRSet, then this is not a conflict.
mDNSlocal mDNSBool PacketRRConflict(const mDNS *const m, const AuthRecord *const our, const CacheRecord *const pktrr)
	{
	const AuthRecord *ourset = our->RRSet ? our->RRSet : our;

	// If not supposed to be unique, not a conflict
	if (!(our->resrec.RecordType & kDNSRecordTypeUniqueMask)) return(mDNSfalse);

	// If a dependent record, not a conflict
	if (our->DependentOn || MatchDependentOn(m, pktrr, our)) return(mDNSfalse);

	// If the pktrr matches a member of ourset, not a conflict
	if (FindRRSet(m, pktrr) == ourset) return(mDNSfalse);

	// Okay, this is a conflict
	return(mDNStrue);
	}

// NOTE: ResolveSimultaneousProbe calls mDNS_Deregister_internal which can call a user callback, which may change
// the record list and/or question list.
// Any code walking either list must use the CurrentQuestion and/or CurrentRecord mechanism to protect against this.
mDNSlocal void ResolveSimultaneousProbe(mDNS *const m, const DNSMessage *const query, const mDNSu8 *const end,
	DNSQuestion *q, AuthRecord *our)
	{
	int i;
	const mDNSu8 *ptr = LocateAuthorities(query, end);
	mDNSBool FoundUpdate = mDNSfalse;

	for (i = 0; i < query->h.numAuthorities; i++)
		{
		ptr = GetLargeResourceRecord(m, query, ptr, end, q->InterfaceID, kDNSRecordTypePacketAuth, &m->rec);
		if (!ptr) break;
		if (ResourceRecordAnswersQuestion(&m->rec.r.resrec, q))
			{
			FoundUpdate = mDNStrue;
			if (PacketRRConflict(m, our, &m->rec.r))
				{
				int result          = (int)our->resrec.rrclass - (int)m->rec.r.resrec.rrclass;
				if (!result) result = (int)our->resrec.rrtype  - (int)m->rec.r.resrec.rrtype;
				if (!result) result = CompareRData(our, &m->rec.r);
				if (result > 0)
					debugf("ResolveSimultaneousProbe: %##s (%s): We won",  our->resrec.name->c, DNSTypeName(our->resrec.rrtype));
				else if (result < 0)
					{
					debugf("ResolveSimultaneousProbe: %##s (%s): We lost", our->resrec.name->c, DNSTypeName(our->resrec.rrtype));
					mDNS_Deregister_internal(m, our, mDNS_Dereg_conflict);
					goto exit;
					}
				}
			}
		m->rec.r.resrec.RecordType = 0;		// Clear RecordType to show we're not still using it
		}
	if (!FoundUpdate)
		debugf("ResolveSimultaneousProbe: %##s (%s): No Update Record found", our->resrec.name->c, DNSTypeName(our->resrec.rrtype));
exit:
	m->rec.r.resrec.RecordType = 0;		// Clear RecordType to show we're not still using it
	}

mDNSlocal CacheRecord *FindIdenticalRecordInCache(const mDNS *const m, ResourceRecord *pktrr)
	{
	mDNSu32 slot = HashSlot(pktrr->name);
	CacheGroup *cg = CacheGroupForRecord(m, slot, pktrr);
	CacheRecord *rr;
	for (rr = cg ? cg->members : mDNSNULL; rr; rr=rr->next)
		if (pktrr->InterfaceID == rr->resrec.InterfaceID && IdenticalResourceRecord(pktrr, &rr->resrec)) break;
	return(rr);
	}

// ProcessQuery examines a received query to see if we have any answers to give
mDNSlocal mDNSu8 *ProcessQuery(mDNS *const m, const DNSMessage *const query, const mDNSu8 *const end,
	const mDNSAddr *srcaddr, const mDNSInterfaceID InterfaceID, mDNSBool LegacyQuery, mDNSBool QueryWasMulticast,
	mDNSBool QueryWasLocalUnicast, DNSMessage *const response)
	{
	mDNSBool      FromLocalSubnet    = AddressIsLocalSubnet(m, InterfaceID, srcaddr);
	AuthRecord   *ResponseRecords    = mDNSNULL;
	AuthRecord  **nrp                = &ResponseRecords;
	CacheRecord  *ExpectedAnswers    = mDNSNULL;			// Records in our cache we expect to see updated
	CacheRecord **eap                = &ExpectedAnswers;
	DNSQuestion  *DupQuestions       = mDNSNULL;			// Our questions that are identical to questions in this packet
	DNSQuestion **dqp                = &DupQuestions;
	mDNSs32       delayresponse      = 0;
	mDNSBool      SendLegacyResponse = mDNSfalse;
	const mDNSu8 *ptr                = query->data;
	mDNSu8       *responseptr        = mDNSNULL;
	AuthRecord   *rr;
	int i;

	// ***
	// *** 1. Parse Question Section and mark potential answers
	// ***
	for (i=0; i<query->h.numQuestions; i++)						// For each question...
		{
		mDNSBool QuestionNeedsMulticastResponse;
		int NumAnswersForThisQuestion = 0;
		DNSQuestion pktq, *q;
		ptr = getQuestion(query, ptr, end, InterfaceID, &pktq);	// get the question...
		if (!ptr) goto exit;

		// The only queries that *need* a multicast response are:
		// * Queries sent via multicast
		// * from port 5353
		// * that don't have the kDNSQClass_UnicastResponse bit set
		// These queries need multicast responses because other clients will:
		// * suppress their own identical questions when they see these questions, and
		// * expire their cache records if they don't see the expected responses
		// For other queries, we may still choose to send the occasional multicast response anyway,
		// to keep our neighbours caches warm, and for ongoing conflict detection.
		QuestionNeedsMulticastResponse = QueryWasMulticast && !LegacyQuery && !(pktq.qclass & kDNSQClass_UnicastResponse);
		// Clear the UnicastResponse flag -- don't want to confuse the rest of the code that follows later
		pktq.qclass &= ~kDNSQClass_UnicastResponse;
		
		// Note: We use the m->CurrentRecord mechanism here because calling ResolveSimultaneousProbe
		// can result in user callbacks which may change the record list and/or question list.
		// Also note: we just mark potential answer records here, without trying to build the
		// "ResponseRecords" list, because we don't want to risk user callbacks deleting records
		// from that list while we're in the middle of trying to build it.
		if (m->CurrentRecord) LogMsg("ProcessQuery ERROR m->CurrentRecord already set");
		m->CurrentRecord = m->ResourceRecords;
		while (m->CurrentRecord)
			{
			rr = m->CurrentRecord;
			m->CurrentRecord = rr->next;
			if (ResourceRecordAnswersQuestion(&rr->resrec, &pktq) && (QueryWasMulticast || QueryWasLocalUnicast || rr->AllowRemoteQuery))
				{
				if (rr->resrec.RecordType == kDNSRecordTypeUnique)
					ResolveSimultaneousProbe(m, query, end, &pktq, rr);
				else if (ResourceRecordIsValidAnswer(rr))
					{
					NumAnswersForThisQuestion++;
					// Notes:
					// NR_AnswerTo pointing into query packet means "answer via immediate legacy unicast" (may *also* choose to multicast)
					// NR_AnswerTo == (mDNSu8*)~1             means "answer via delayed unicast" (to modern querier; may promote to multicast instead)
					// NR_AnswerTo == (mDNSu8*)~0             means "definitely answer via multicast" (can't downgrade to unicast later)
					// If we're not multicasting this record because the kDNSQClass_UnicastResponse bit was set,
					// but the multicast querier is not on a matching subnet (e.g. because of overalyed subnets on one link)
					// then we'll multicast it anyway (if we unicast, the receiver will ignore it because it has an apparently non-local source)
					if (QuestionNeedsMulticastResponse || (!FromLocalSubnet && QueryWasMulticast && !LegacyQuery))
						{
						// We only mark this question for sending if it is at least one second since the last time we multicast it
						// on this interface. If it is more than a second, or LastMCInterface is different, then we may multicast it.
						// This is to guard against the case where someone blasts us with queries as fast as they can.
						if (m->timenow - (rr->LastMCTime + mDNSPlatformOneSecond) >= 0 ||
							(rr->LastMCInterface != mDNSInterfaceMark && rr->LastMCInterface != InterfaceID))
							rr->NR_AnswerTo = (mDNSu8*)~0;
						}
					else if (!rr->NR_AnswerTo) rr->NR_AnswerTo = LegacyQuery ? ptr : (mDNSu8*)~1;
					}
				}
			}

		// If we couldn't answer this question, someone else might be able to,
		// so use random delay on response to reduce collisions
		if (NumAnswersForThisQuestion == 0) delayresponse = mDNSPlatformOneSecond;	// Divided by 50 = 20ms

		// We only do the following accelerated cache expiration processing and duplicate question suppression processing
		// for multicast queries with multicast responses.
		// For any query generating a unicast response we don't do this because we can't assume we will see the response
		if (QuestionNeedsMulticastResponse)
			{
			const mDNSu32 slot = HashSlot(&pktq.qname);
			CacheGroup *cg = CacheGroupForName(m, slot, pktq.qnamehash, &pktq.qname);
			CacheRecord *rr;

			// Make a list indicating which of our own cache records we expect to see updated as a result of this query
			// Note: Records larger than 1K are not habitually multicast, so don't expect those to be updated
			for (rr = cg ? cg->members : mDNSNULL; rr; rr=rr->next)
				if (ResourceRecordAnswersQuestion(&rr->resrec, &pktq) && rr->resrec.rdlength <= SmallRecordLimit)
					if (!rr->NextInKAList && eap != &rr->NextInKAList)
						{
						*eap = rr;
						eap = &rr->NextInKAList;
						if (rr->MPUnansweredQ == 0 || m->timenow - rr->MPLastUnansweredQT >= mDNSPlatformOneSecond)
							{
							// Although MPUnansweredQ is only really used for multi-packet query processing,
							// we increment it for both single-packet and multi-packet queries, so that it stays in sync
							// with the MPUnansweredKA value, which by necessity is incremented for both query types.
							rr->MPUnansweredQ++;
							rr->MPLastUnansweredQT = m->timenow;
							rr->MPExpectingKA = mDNStrue;
							}
						}
	
			// Check if this question is the same as any of mine.
			// We only do this for non-truncated queries. Right now it would be too complicated to try
			// to keep track of duplicate suppression state between multiple packets, especially when we
			// can't guarantee to receive all of the Known Answer packets that go with a particular query.
			if (!(query->h.flags.b[0] & kDNSFlag0_TC))
				for (q = m->Questions; q; q=q->next)
					if (!q->Target.type && ActiveQuestion(q) && m->timenow - q->LastQTxTime > mDNSPlatformOneSecond / 4)
						if (!q->InterfaceID || q->InterfaceID == InterfaceID)
							if (q->NextInDQList == mDNSNULL && dqp != &q->NextInDQList)
								if (q->qtype == pktq.qtype &&
									q->qclass == pktq.qclass &&
									q->qnamehash == pktq.qnamehash && SameDomainName(&q->qname, &pktq.qname))
									{ *dqp = q; dqp = &q->NextInDQList; }
			}
		}

	// ***
	// *** 2. Now we can safely build the list of marked answers
	// ***
	for (rr = m->ResourceRecords; rr; rr=rr->next)				// Now build our list of potential answers
		if (rr->NR_AnswerTo)									// If we marked the record...
			AddRecordToResponseList(&nrp, rr, mDNSNULL);		// ... add it to the list

	// ***
	// *** 3. Add additional records
	// ***
	AddAdditionalsToResponseList(m, ResponseRecords, &nrp, InterfaceID);

	// ***
	// *** 4. Parse Answer Section and cancel any records disallowed by Known-Answer list
	// ***
	for (i=0; i<query->h.numAnswers; i++)						// For each record in the query's answer section...
		{
		// Get the record...
		AuthRecord *rr;
		CacheRecord *ourcacherr;
		ptr = GetLargeResourceRecord(m, query, ptr, end, InterfaceID, kDNSRecordTypePacketAns, &m->rec);
		if (!ptr) goto exit;

		// See if this Known-Answer suppresses any of our currently planned answers
		for (rr=ResponseRecords; rr; rr=rr->NextResponse)
			if (MustSendRecord(rr) && ShouldSuppressKnownAnswer(&m->rec.r, rr))
				{ rr->NR_AnswerTo = mDNSNULL; rr->NR_AdditionalTo = mDNSNULL; }

		// See if this Known-Answer suppresses any previously scheduled answers (for multi-packet KA suppression)
		for (rr=m->ResourceRecords; rr; rr=rr->next)
			{
			// If we're planning to send this answer on this interface, and only on this interface, then allow KA suppression
			if (rr->ImmedAnswer == InterfaceID && ShouldSuppressKnownAnswer(&m->rec.r, rr))
				{
				if (srcaddr->type == mDNSAddrType_IPv4)
					{
					if (mDNSSameIPv4Address(rr->v4Requester, srcaddr->ip.v4)) rr->v4Requester = zerov4Addr;
					}
				else if (srcaddr->type == mDNSAddrType_IPv6)
					{
					if (mDNSSameIPv6Address(rr->v6Requester, srcaddr->ip.v6)) rr->v6Requester = zerov6Addr;
					}
				if (mDNSIPv4AddressIsZero(rr->v4Requester) && mDNSIPv6AddressIsZero(rr->v6Requester))
					{
					rr->ImmedAnswer  = mDNSNULL;
					rr->ImmedUnicast = mDNSfalse;
#if MDNS_LOG_ANSWER_SUPPRESSION_TIMES
					LogMsg("Suppressed after%4d: %s", m->timenow - rr->ImmedAnswerMarkTime, ARDisplayString(m, rr));
#endif
					}
				}
			}

		// See if this Known-Answer suppresses any answers we were expecting for our cache records. We do this always,
		// even if the TC bit is not set (the TC bit will *not* be set in the *last* packet of a multi-packet KA list).
		ourcacherr = FindIdenticalRecordInCache(m, &m->rec.r.resrec);
		if (ourcacherr && ourcacherr->MPExpectingKA && m->timenow - ourcacherr->MPLastUnansweredQT < mDNSPlatformOneSecond)
			{
			ourcacherr->MPUnansweredKA++;
			ourcacherr->MPExpectingKA = mDNSfalse;
			}

		// Having built our ExpectedAnswers list from the questions in this packet, we can definitively
		// remove from our ExpectedAnswers list any records that are suppressed in the very same packet.
		// For answers that are suppressed in subsequent KA list packets, we rely on the MPQ/MPKA counting to track them.
		eap = &ExpectedAnswers;
		while (*eap)
			{
			CacheRecord *rr = *eap;
			if (rr->resrec.InterfaceID == InterfaceID && IdenticalResourceRecord(&m->rec.r.resrec, &rr->resrec))
				{ *eap = rr->NextInKAList; rr->NextInKAList = mDNSNULL; }
			else eap = &rr->NextInKAList;
			}
		
		// See if this Known-Answer is a surprise to us. If so, we shouldn't suppress our own query.
		if (!ourcacherr)
			{
			dqp = &DupQuestions;
			while (*dqp)
				{
				DNSQuestion *q = *dqp;
				if (ResourceRecordAnswersQuestion(&m->rec.r.resrec, q))
					{ *dqp = q->NextInDQList; q->NextInDQList = mDNSNULL; }
				else dqp = &q->NextInDQList;
				}
			}
		m->rec.r.resrec.RecordType = 0;		// Clear RecordType to show we're not still using it
		}

	// ***
	// *** 5. Cancel any additionals that were added because of now-deleted records
	// ***
	for (rr=ResponseRecords; rr; rr=rr->NextResponse)
		if (rr->NR_AdditionalTo && !MustSendRecord(rr->NR_AdditionalTo))
			{ rr->NR_AnswerTo = mDNSNULL; rr->NR_AdditionalTo = mDNSNULL; }

	// ***
	// *** 6. Mark the send flags on the records we plan to send
	// ***
	for (rr=ResponseRecords; rr; rr=rr->NextResponse)
		{
		if (rr->NR_AnswerTo)
			{
			mDNSBool SendMulticastResponse = mDNSfalse;		// Send modern multicast response
			mDNSBool SendUnicastResponse   = mDNSfalse;		// Send modern unicast response (not legacy unicast response)
			
			// If it's been a while since we multicast this, then send a multicast response for conflict detection, etc.
			if (m->timenow - (rr->LastMCTime + TicksTTL(rr)/4) >= 0)
				{
				SendMulticastResponse = mDNStrue;
				// If this record was marked for modern (delayed) unicast response, then mark it as promoted to
				// multicast response instead (don't want to end up ALSO setting SendUnicastResponse in the check below).
				// If this record was marked for legacy unicast response, then we mustn't change the NR_AnswerTo value.
				if (rr->NR_AnswerTo == (mDNSu8*)~1) rr->NR_AnswerTo = (mDNSu8*)~0;
				}
			
			// If the client insists on a multicast response, then we'd better send one
			if      (rr->NR_AnswerTo == (mDNSu8*)~0) SendMulticastResponse = mDNStrue;
			else if (rr->NR_AnswerTo == (mDNSu8*)~1) SendUnicastResponse   = mDNStrue;
			else if (rr->NR_AnswerTo)                SendLegacyResponse    = mDNStrue;
	
			if (SendMulticastResponse || SendUnicastResponse)
				{
#if MDNS_LOG_ANSWER_SUPPRESSION_TIMES
				rr->ImmedAnswerMarkTime = m->timenow;
#endif
				m->NextScheduledResponse = m->timenow;
				// If we're already planning to send this on another interface, just send it on all interfaces
				if (rr->ImmedAnswer && rr->ImmedAnswer != InterfaceID)
					rr->ImmedAnswer = mDNSInterfaceMark;
				else
					{
					rr->ImmedAnswer = InterfaceID;			// Record interface to send it on
					if (SendUnicastResponse) rr->ImmedUnicast = mDNStrue;
					if (srcaddr->type == mDNSAddrType_IPv4)
						{
						if      (mDNSIPv4AddressIsZero(rr->v4Requester))                rr->v4Requester = srcaddr->ip.v4;
						else if (!mDNSSameIPv4Address(rr->v4Requester, srcaddr->ip.v4)) rr->v4Requester = onesIPv4Addr;
						}
					else if (srcaddr->type == mDNSAddrType_IPv6)
						{
						if      (mDNSIPv6AddressIsZero(rr->v6Requester))                rr->v6Requester = srcaddr->ip.v6;
						else if (!mDNSSameIPv6Address(rr->v6Requester, srcaddr->ip.v6)) rr->v6Requester = onesIPv6Addr;
						}
					}
				}
			// If TC flag is set, it means we should expect that additional known answers may be coming in another packet,
			// so we allow roughly half a second before deciding to reply (we've observed inter-packet delays of 100-200ms on 802.11)
			// else, if record is a shared one, spread responses over 100ms to avoid implosion of simultaneous responses
			// else, for a simple unique record reply, we can reply immediately; no need for delay
			if      (query->h.flags.b[0] & kDNSFlag0_TC)            delayresponse = mDNSPlatformOneSecond * 20;	// Divided by 50 = 400ms
			else if (rr->resrec.RecordType == kDNSRecordTypeShared) delayresponse = mDNSPlatformOneSecond;		// Divided by 50 = 20ms
			}
		else if (rr->NR_AdditionalTo && rr->NR_AdditionalTo->NR_AnswerTo == (mDNSu8*)~0)
			{
			// Since additional records are an optimization anyway, we only ever send them on one interface at a time
			// If two clients on different interfaces do queries that invoke the same optional additional answer,
			// then the earlier client is out of luck
			rr->ImmedAdditional = InterfaceID;
			// No need to set m->NextScheduledResponse here
			// We'll send these additional records when we send them, or not, as the case may be
			}
		}

	// ***
	// *** 7. If we think other machines are likely to answer these questions, set our packet suppression timer
	// ***
	if (delayresponse && (!m->SuppressSending || (m->SuppressSending - m->timenow) < (delayresponse + 49) / 50))
		{
#if MDNS_LOG_ANSWER_SUPPRESSION_TIMES
		mDNSs32 oldss = m->SuppressSending;
		if (oldss && delayresponse)
			LogMsg("Current SuppressSending delay%5ld; require%5ld", m->SuppressSending - m->timenow, (delayresponse + 49) / 50);
#endif
		// Pick a random delay:
		// We start with the base delay chosen above (typically either 1 second or 20 seconds),
		// and add a random value in the range 0-5 seconds (making 1-6 seconds or 20-25 seconds).
		// This is an integer value, with resolution determined by the platform clock rate.
		// We then divide that by 50 to get the delay value in ticks. We defer the division until last
		// to get better results on platforms with coarse clock granularity (e.g. ten ticks per second).
		// The +49 before dividing is to ensure we round up, not down, to ensure that even
		// on platforms where the native clock rate is less than fifty ticks per second,
		// we still guarantee that the final calculated delay is at least one platform tick.
		// We want to make sure we don't ever allow the delay to be zero ticks,
		// because if that happens we'll fail the Bonjour Conformance Test.
		// Our final computed delay is 20-120ms for normal delayed replies,
		// or 400-500ms in the case of multi-packet known-answer lists.
		m->SuppressSending = m->timenow + (delayresponse + (mDNSs32)mDNSRandom((mDNSu32)mDNSPlatformOneSecond*5) + 49) / 50;
		if (m->SuppressSending == 0) m->SuppressSending = 1;
#if MDNS_LOG_ANSWER_SUPPRESSION_TIMES
		if (oldss && delayresponse)
			LogMsg("Set     SuppressSending to   %5ld", m->SuppressSending - m->timenow);
#endif
		}

	// ***
	// *** 8. If query is from a legacy client, or from a new client requesting a unicast reply, then generate a unicast response too
	// ***
	if (SendLegacyResponse)
		responseptr = GenerateUnicastResponse(query, end, InterfaceID, LegacyQuery, response, ResponseRecords);

exit:
	m->rec.r.resrec.RecordType = 0;		// Clear RecordType to show we're not still using it
	
	// ***
	// *** 9. Finally, clear our link chains ready for use next time
	// ***
	while (ResponseRecords)
		{
		rr = ResponseRecords;
		ResponseRecords = rr->NextResponse;
		rr->NextResponse    = mDNSNULL;
		rr->NR_AnswerTo     = mDNSNULL;
		rr->NR_AdditionalTo = mDNSNULL;
		}
	
	while (ExpectedAnswers)
		{
		CacheRecord *rr;
		rr = ExpectedAnswers;
		ExpectedAnswers = rr->NextInKAList;
		rr->NextInKAList = mDNSNULL;
		
		// For non-truncated queries, we can definitively say that we should expect
		// to be seeing a response for any records still left in the ExpectedAnswers list
		if (!(query->h.flags.b[0] & kDNSFlag0_TC))
			if (rr->UnansweredQueries == 0 || m->timenow - rr->LastUnansweredTime >= mDNSPlatformOneSecond)
				{
				rr->UnansweredQueries++;
				rr->LastUnansweredTime = m->timenow;
				if (rr->UnansweredQueries > 1)
					debugf("ProcessQuery: (!TC) UAQ %lu MPQ %lu MPKA %lu %s",
						rr->UnansweredQueries, rr->MPUnansweredQ, rr->MPUnansweredKA, CRDisplayString(m, rr));
				SetNextCacheCheckTime(m, rr);
				}

		// If we've seen multiple unanswered queries for this record,
		// then mark it to expire in five seconds if we don't get a response by then.
		if (rr->UnansweredQueries >= MaxUnansweredQueries)
			{
			// Only show debugging message if this record was not about to expire anyway
			if (RRExpireTime(rr) - m->timenow > 4 * mDNSPlatformOneSecond)
				debugf("ProcessQuery: (Max) UAQ %lu MPQ %lu MPKA %lu mDNS_Reconfirm() for %s",
					rr->UnansweredQueries, rr->MPUnansweredQ, rr->MPUnansweredKA, CRDisplayString(m, rr));
			mDNS_Reconfirm_internal(m, rr, kDefaultReconfirmTimeForNoAnswer);
			}
		// Make a guess, based on the multi-packet query / known answer counts, whether we think we
		// should have seen an answer for this. (We multiply MPQ by 4 and MPKA by 5, to allow for
		// possible packet loss of up to 20% of the additional KA packets.)
		else if (rr->MPUnansweredQ * 4 > rr->MPUnansweredKA * 5 + 8)
			{
			// We want to do this conservatively.
			// If there are so many machines on the network that they have to use multi-packet known-answer lists,
			// then we don't want them to all hit the network simultaneously with their final expiration queries.
			// By setting the record to expire in four minutes, we achieve two things:
			// (a) the 90-95% final expiration queries will be less bunched together
			// (b) we allow some time for us to witness enough other failed queries that we don't have to do our own
			mDNSu32 remain = (mDNSu32)(RRExpireTime(rr) - m->timenow) / 4;
			if (remain > 240 * (mDNSu32)mDNSPlatformOneSecond)
				remain = 240 * (mDNSu32)mDNSPlatformOneSecond;
			
			// Only show debugging message if this record was not about to expire anyway
			if (RRExpireTime(rr) - m->timenow > 4 * mDNSPlatformOneSecond)
				debugf("ProcessQuery: (MPQ) UAQ %lu MPQ %lu MPKA %lu mDNS_Reconfirm() for %s",
					rr->UnansweredQueries, rr->MPUnansweredQ, rr->MPUnansweredKA, CRDisplayString(m, rr));

			if (remain <= 60 * (mDNSu32)mDNSPlatformOneSecond)
				rr->UnansweredQueries++;	// Treat this as equivalent to one definite unanswered query
			rr->MPUnansweredQ  = 0;			// Clear MPQ/MPKA statistics
			rr->MPUnansweredKA = 0;
			rr->MPExpectingKA  = mDNSfalse;
			
			if (remain < kDefaultReconfirmTimeForNoAnswer)
				remain = kDefaultReconfirmTimeForNoAnswer;
			mDNS_Reconfirm_internal(m, rr, remain);
			}
		}
	
	while (DupQuestions)
		{
		int i;
		DNSQuestion *q = DupQuestions;
		DupQuestions = q->NextInDQList;
		q->NextInDQList = mDNSNULL;
		i = RecordDupSuppressInfo(q->DupSuppress, m->timenow, InterfaceID, srcaddr->type);
		debugf("ProcessQuery: Recorded DSI for %##s (%s) on %p/%s %d", q->qname.c, DNSTypeName(q->qtype), InterfaceID,
			srcaddr->type == mDNSAddrType_IPv4 ? "v4" : "v6", i);
		}
	
	return(responseptr);
	}

mDNSlocal void mDNSCoreReceiveQuery(mDNS *const m, const DNSMessage *const msg, const mDNSu8 *const end,
	const mDNSAddr *srcaddr, const mDNSIPPort srcport, const mDNSAddr *dstaddr, mDNSIPPort dstport,
	const mDNSInterfaceID InterfaceID)
	{
	mDNSu8    *responseend = mDNSNULL;
	mDNSBool   QueryWasLocalUnicast = !mDNSAddrIsDNSMulticast(dstaddr) && AddressIsLocalSubnet(m, InterfaceID, srcaddr);
	
	if (!InterfaceID && mDNSAddrIsDNSMulticast(dstaddr))
		{
		LogMsg("Ignoring Query from %#-15a:%-5d to %#-15a:%-5d on 0x%p with "
			"%2d Question%s %2d Answer%s %2d Authorit%s %2d Additional%s (Multicast, but no InterfaceID)",
			srcaddr, mDNSVal16(srcport), dstaddr, mDNSVal16(dstport), InterfaceID,
			msg->h.numQuestions,   msg->h.numQuestions   == 1 ? ", " : "s,",
			msg->h.numAnswers,     msg->h.numAnswers     == 1 ? ", " : "s,",
			msg->h.numAuthorities, msg->h.numAuthorities == 1 ? "y,  " : "ies,",
			msg->h.numAdditionals, msg->h.numAdditionals == 1 ? "" : "s");
		return;
		}
	
	verbosedebugf("Received Query from %#-15a:%-5d to %#-15a:%-5d on 0x%p with "
		"%2d Question%s %2d Answer%s %2d Authorit%s %2d Additional%s",
		srcaddr, mDNSVal16(srcport), dstaddr, mDNSVal16(dstport), InterfaceID,
		msg->h.numQuestions,   msg->h.numQuestions   == 1 ? ", " : "s,",
		msg->h.numAnswers,     msg->h.numAnswers     == 1 ? ", " : "s,",
		msg->h.numAuthorities, msg->h.numAuthorities == 1 ? "y,  " : "ies,",
		msg->h.numAdditionals, msg->h.numAdditionals == 1 ? "" : "s");
	
	responseend = ProcessQuery(m, msg, end, srcaddr, InterfaceID,
		(srcport.NotAnInteger != MulticastDNSPort.NotAnInteger), mDNSAddrIsDNSMulticast(dstaddr), QueryWasLocalUnicast, &m->omsg);

	if (responseend)	// If responseend is non-null, that means we built a unicast response packet
		{
		debugf("Unicast Response: %d Question%s, %d Answer%s, %d Additional%s to %#-15a:%d on %p/%ld",
			m->omsg.h.numQuestions,   m->omsg.h.numQuestions   == 1 ? "" : "s",
			m->omsg.h.numAnswers,     m->omsg.h.numAnswers     == 1 ? "" : "s",
			m->omsg.h.numAdditionals, m->omsg.h.numAdditionals == 1 ? "" : "s",
			srcaddr, mDNSVal16(srcport), InterfaceID, srcaddr->type);
		mDNSSendDNSMessage(m, &m->omsg, responseend, InterfaceID, srcaddr, srcport, -1, mDNSNULL);
		}
	}

// NOTE: mDNSCoreReceiveResponse calls mDNS_Deregister_internal which can call a user callback, which may change
// the record list and/or question list.
// Any code walking either list must use the CurrentQuestion and/or CurrentRecord mechanism to protect against this.
mDNSlocal void mDNSCoreReceiveResponse(mDNS *const m,
	const DNSMessage *const response, const mDNSu8 *end,
	const mDNSAddr *srcaddr, const mDNSIPPort srcport, const mDNSAddr *dstaddr, mDNSIPPort dstport,
	const mDNSInterfaceID InterfaceID)
	{
	int i;

	// We ignore questions (if any) in a DNS response packet
	const mDNSu8 *ptr = LocateAnswers(response, end);

	// "(CacheRecord*)1" is a special (non-zero) end-of-list marker
	// We use this non-zero marker so that records in our CacheFlushRecords list will always have NextInCFList
	// set non-zero, and that tells GetCacheEntity() that they're not, at this moment, eligible for recycling.
	CacheRecord *CacheFlushRecords = (CacheRecord*)1;
	CacheRecord **cfp = &CacheFlushRecords;

	// All records in a DNS response packet are treated as equally valid statements of truth. If we want
	// to guard against spoof responses, then the only credible protection against that is cryptographic
	// security, e.g. DNSSEC., not worring about which section in the spoof packet contained the record
	int totalrecords = response->h.numAnswers + response->h.numAuthorities + response->h.numAdditionals;

	(void)srcaddr;	// Currently used only for display in debugging message
	(void)srcport;
	(void)dstport;

	verbosedebugf("Received Response from %#-15a addressed to %#-15a on %p with "
		"%2d Question%s %2d Answer%s %2d Authorit%s %2d Additional%s",
		srcaddr, dstaddr, InterfaceID,
		response->h.numQuestions,   response->h.numQuestions   == 1 ? ", " : "s,",
		response->h.numAnswers,     response->h.numAnswers     == 1 ? ", " : "s,",
		response->h.numAuthorities, response->h.numAuthorities == 1 ? "y,  " : "ies,",
		response->h.numAdditionals, response->h.numAdditionals == 1 ? "" : "s");

	// If we get a unicast response when we weren't expecting one, then we assume it is someone trying to spoof us
	if (!mDNSAddrIsDNSMulticast(dstaddr))
		{
		if (!AddressIsLocalSubnet(m, InterfaceID, srcaddr) || (mDNSu32)(m->timenow - m->ExpectUnicastResponse) > (mDNSu32)(mDNSPlatformOneSecond*2))
			return;
		// For now we don't put standard wide-area unicast responses in our main cache
		// (Later we should fix this and cache all known results in a unified manner.)
		if (response->h.id.NotAnInteger != 0 || srcport.NotAnInteger != MulticastDNSPort.NotAnInteger)
			return;
		}

	for (i = 0; i < totalrecords && ptr && ptr < end; i++)
		{
		const mDNSu8 RecordType = (mDNSu8)((i < response->h.numAnswers) ? kDNSRecordTypePacketAns : kDNSRecordTypePacketAdd);
		ptr = GetLargeResourceRecord(m, response, ptr, end, InterfaceID, RecordType, &m->rec);
		if (!ptr) goto exit;		// Break out of the loop and clean up our CacheFlushRecords list before exiting

		// 1. Check that this packet resource record does not conflict with any of ours
		if (m->CurrentRecord) LogMsg("mDNSCoreReceiveResponse ERROR m->CurrentRecord already set");
		m->CurrentRecord = m->ResourceRecords;
		while (m->CurrentRecord)
			{
			AuthRecord *rr = m->CurrentRecord;
			m->CurrentRecord = rr->next;
			if (PacketRRMatchesSignature(&m->rec.r, rr))		// If interface, name, type (if shared record) and class match...
				{
				// ... check to see if type and rdata are identical
				if (m->rec.r.resrec.rrtype == rr->resrec.rrtype && SameRData(&m->rec.r.resrec, &rr->resrec))
					{
					// If the RR in the packet is identical to ours, just check they're not trying to lower the TTL on us
					if (m->rec.r.resrec.rroriginalttl >= rr->resrec.rroriginalttl/2 || m->SleepState)
						{
						// If we were planning to send on this -- and only this -- interface, then we don't need to any more
						if      (rr->ImmedAnswer == InterfaceID) { rr->ImmedAnswer = mDNSNULL; rr->ImmedUnicast = mDNSfalse; }
						}
					else
						{
						if      (rr->ImmedAnswer == mDNSNULL)    { rr->ImmedAnswer = InterfaceID;       m->NextScheduledResponse = m->timenow; }
						else if (rr->ImmedAnswer != InterfaceID) { rr->ImmedAnswer = mDNSInterfaceMark; m->NextScheduledResponse = m->timenow; }
						}
					}
				// else, the packet RR has different type or different rdata -- check to see if this is a conflict
				else if (m->rec.r.resrec.rroriginalttl > 0 && PacketRRConflict(m, rr, &m->rec.r))
					{
					debugf("mDNSCoreReceiveResponse: Our Record: %08lX %s", rr->     resrec.rdatahash, ARDisplayString(m, rr));
					debugf("mDNSCoreReceiveResponse: Pkt Record: %08lX %s", m->rec.r.resrec.rdatahash, CRDisplayString(m, &m->rec.r));

					// If this record is marked DependentOn another record for conflict detection purposes,
					// then *that* record has to be bumped back to probing state to resolve the conflict
					while (rr->DependentOn) rr = rr->DependentOn;

					// If we've just whacked this record's ProbeCount, don't need to do it again
					if (rr->ProbeCount <= DefaultProbeCountForTypeUnique)
						{
						// If we'd previously verified this record, put it back to probing state and try again
						if (rr->resrec.RecordType == kDNSRecordTypeVerified)
							{
							debugf("mDNSCoreReceiveResponse: Reseting to Probing: %##s (%s)", rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
							rr->resrec.RecordType     = kDNSRecordTypeUnique;
							rr->ProbeCount     = DefaultProbeCountForTypeUnique + 1;
							rr->ThisAPInterval = DefaultAPIntervalForRecordType(kDNSRecordTypeUnique);
							InitializeLastAPTime(m, rr);
							RecordProbeFailure(m, rr);	// Repeated late conflicts also cause us to back off to the slower probing rate
							}
						// If we're probing for this record, we just failed
						else if (rr->resrec.RecordType == kDNSRecordTypeUnique)
							{
							debugf("mDNSCoreReceiveResponse: Will rename %##s (%s)", rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
							mDNS_Deregister_internal(m, rr, mDNS_Dereg_conflict);
							}
						// We assumed this record must be unique, but we were wrong.
						// (e.g. There are two mDNSResponders on the same machine giving
						// different answers for the reverse mapping record.)
						// This is simply a misconfiguration, and we don't try to recover from it.
						else if (rr->resrec.RecordType == kDNSRecordTypeKnownUnique)
							{
							debugf("mDNSCoreReceiveResponse: Unexpected conflict on %##s (%s) -- discarding our record",
								rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
							mDNS_Deregister_internal(m, rr, mDNS_Dereg_conflict);
							}
						else
							debugf("mDNSCoreReceiveResponse: Unexpected record type %X %##s (%s)",
								rr->resrec.RecordType, rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype));
						}
					}
				// Else, matching signature, different type or rdata, but not a considered a conflict.
				// If the packet record has the cache-flush bit set, then we check to see if we
				// have any record(s) of the same type that we should re-assert to rescue them
				// (see note about "multi-homing and bridged networks" at the end of this function).
				else if (m->rec.r.resrec.rrtype == rr->resrec.rrtype)
					if ((m->rec.r.resrec.RecordType & kDNSRecordTypePacketUniqueMask) && m->timenow - rr->LastMCTime > mDNSPlatformOneSecond/2)
						{ rr->ImmedAnswer = mDNSInterfaceMark; m->NextScheduledResponse = m->timenow; }
				}
			}

		// 2. See if we want to add this packet resource record to our cache
		if (m->rrcache_size)	// Only try to cache answers if we have a cache to put them in
			{
			const mDNSu32 slot = HashSlot(m->rec.r.resrec.name);
			CacheGroup *cg = CacheGroupForRecord(m, slot, &m->rec.r.resrec);
			CacheRecord *rr;
			// 2a. Check if this packet resource record is already in our cache
			for (rr = cg ? cg->members : mDNSNULL; rr; rr=rr->next)
				{
				// If we found this exact resource record, refresh its TTL
				if (rr->resrec.InterfaceID == InterfaceID && IdenticalResourceRecord(&m->rec.r.resrec, &rr->resrec))
					{
					if (m->rec.r.resrec.rdlength > InlineCacheRDSize)
						verbosedebugf("Found record size %5d interface %p already in cache: %s",
							m->rec.r.resrec.rdlength, InterfaceID, CRDisplayString(m, &m->rec.r));
					rr->TimeRcvd  = m->timenow;
					
					if (m->rec.r.resrec.RecordType & kDNSRecordTypePacketUniqueMask)
						{
						// If this packet record has the kDNSClass_UniqueRRSet flag set, then add it to our cache flushing list
						if (rr->NextInCFList == mDNSNULL && cfp != &rr->NextInCFList)
							{ *cfp = rr; cfp = &rr->NextInCFList; *cfp = (CacheRecord*)1; }

						// If this packet record is marked unique, and our previous cached copy was not, then fix it
						if (!(rr->resrec.RecordType & kDNSRecordTypePacketUniqueMask))
							{
							DNSQuestion *q;
							for (q = m->Questions; q; q=q->next) if (ResourceRecordAnswersQuestion(&rr->resrec, q)) q->UniqueAnswers++;
							rr->resrec.RecordType = m->rec.r.resrec.RecordType;
							}
						}

					if (!mDNSPlatformMemSame(m->rec.r.resrec.rdata->u.data, rr->resrec.rdata->u.data, m->rec.r.resrec.rdlength))
						{
						// If the rdata of the packet record differs in name capitalization from the record in our cache
						// then mDNSPlatformMemSame will detect this. In this case, throw the old record away, so that clients get
						// a 'remove' event for the record with the old capitalization, and then an 'add' event for the new one.
						rr->resrec.rroriginalttl = 0;
						rr->UnansweredQueries = MaxUnansweredQueries;
						SetNextCacheCheckTime(m, rr);
						// DO NOT break out here -- we want to continue as if we never found it
						}
					else if (m->rec.r.resrec.rroriginalttl > 0)
						{
						rr->resrec.rroriginalttl = m->rec.r.resrec.rroriginalttl;
						rr->UnansweredQueries = 0;
						rr->MPUnansweredQ     = 0;
						rr->MPUnansweredKA    = 0;
						rr->MPExpectingKA     = mDNSfalse;
						SetNextCacheCheckTime(m, rr);
						break;
						}
					else
						{
						// If the packet TTL is zero, that means we're deleting this record.
						// To give other hosts on the network a chance to protest, we push the deletion
						// out one second into the future. Also, we set UnansweredQueries to MaxUnansweredQueries.
						// Otherwise, we'll do final queries for this record at 80% and 90% of its apparent
						// lifetime (800ms and 900ms from now) which is a pointless waste of network bandwidth.
						rr->resrec.rroriginalttl = 1;
						rr->UnansweredQueries = MaxUnansweredQueries;
						SetNextCacheCheckTime(m, rr);
						break;
						}
					}
				}

			// If packet resource record not in our cache, add it now
			// (unless it is just a deletion of a record we never had, in which case we don't care)
			if (!rr && m->rec.r.resrec.rroriginalttl > 0)
				{
				// If we don't have a CacheGroup for this name, make one now
				if (!cg) cg = GetCacheGroup(m, slot, &m->rec.r.resrec);
				if (cg) rr = GetCacheRecord(m, cg, m->rec.r.resrec.rdlength);	// Make a cache record, being careful not to recycle cg
				if (!rr) NoCacheAnswer(m, &m->rec.r);
				else
					{
					RData *saveptr = rr->resrec.rdata;		// Save the rr->resrec.rdata pointer
					*rr = m->rec.r;							// Block copy the CacheRecord object
					rr->resrec.rdata = saveptr;				// Restore rr->resrec.rdata after the structure assignment
					rr->resrec.name  = cg->name;			// And set rr->resrec.name to point into our CacheGroup header
					if (rr->resrec.RecordType & kDNSRecordTypePacketUniqueMask)
						{ *cfp = rr; cfp = &rr->NextInCFList; *cfp = (CacheRecord*)1; }
					// If this is an oversized record with external storage allocated, copy rdata to external storage
					if (rr->resrec.rdata != (RData*)&rr->rdatastorage && !(m->rec.r.resrec.rdlength > InlineCacheRDSize))
						LogMsg("rr->resrec.rdata != &rr->rdatastorage but length <= InlineCacheRDSize %##s", m->rec.r.resrec.name->c);
					if (m->rec.r.resrec.rdlength > InlineCacheRDSize)
						mDNSPlatformMemCopy(m->rec.r.resrec.rdata, rr->resrec.rdata, sizeofRDataHeader + m->rec.r.resrec.rdlength);
					rr->next = mDNSNULL;					// Clear 'next' pointer
					*(cg->rrcache_tail) = rr;				// Append this record to tail of cache slot list
					cg->rrcache_tail = &(rr->next);			// Advance tail pointer
					if (rr->resrec.RecordType & kDNSRecordTypePacketUniqueMask)	// If marked unique, assume we may have
						rr->DelayDelivery = m->timenow + mDNSPlatformOneSecond;	// to delay delivery of this 'add' event
					else
						rr->DelayDelivery = CheckForSoonToExpireRecords(m, rr->resrec.name, rr->resrec.namehash, slot);
					CacheRecordAdd(m, rr);  // CacheRecordAdd calls SetNextCacheCheckTime(m, rr); for us
					}
				}
			}
		m->rec.r.resrec.RecordType = 0;		// Clear RecordType to show we're not still using it
		}

exit:
	m->rec.r.resrec.RecordType = 0;		// Clear RecordType to show we're not still using it

	// If we've just received one or more records with their cache flush bits set,
	// then scan that cache slot to see if there are any old stale records we need to flush
	while (CacheFlushRecords != (CacheRecord*)1)
		{
		CacheRecord *r1 = CacheFlushRecords, *r2;
		const mDNSu32 slot = HashSlot(r1->resrec.name);
		CacheGroup *cg = CacheGroupForRecord(m, slot, &r1->resrec);
		CacheFlushRecords = CacheFlushRecords->NextInCFList;
		r1->NextInCFList = mDNSNULL;
		for (r2 = cg ? cg->members : mDNSNULL; r2; r2=r2->next)
			if (SameResourceRecordSignature(&r1->resrec, &r2->resrec))
				{
				// If record was recently positively received
				// (i.e. not counting goodbye packets or cache flush events that set the TTL to 1)
				// then we need to ensure the whole RRSet has the same TTL (as required by DNS semantics)
				if (r2->resrec.rroriginalttl > 1 && m->timenow - r2->TimeRcvd < mDNSPlatformOneSecond)
					{
					if (r2->resrec.rroriginalttl != r1->resrec.rroriginalttl)
						LogMsg("Correcting TTL from %4d to %4d for %s",
							r2->resrec.rroriginalttl, r1->resrec.rroriginalttl, CRDisplayString(m, r2));
					r2->resrec.rroriginalttl = r1->resrec.rroriginalttl;
					r2->TimeRcvd = m->timenow;
					}
				else				// else, if record is old, mark it to be flushed
					{
					verbosedebugf("Cache flush %p X %p %s", r1, r2, CRDisplayString(m, r2));
					// We set stale records to expire in one second.
					// This gives the owner a chance to rescue it if necessary.
					// This is important in the case of multi-homing and bridged networks:
					//   Suppose host X is on Ethernet. X then connects to an AirPort base station, which happens to be
					//   bridged onto the same Ethernet. When X announces its AirPort IP address with the cache-flush bit
					//   set, the AirPort packet will be bridged onto the Ethernet, and all other hosts on the Ethernet
					//   will promptly delete their cached copies of the (still valid) Ethernet IP address record.
					//   By delaying the deletion by one second, we give X a change to notice that this bridging has
					//   happened, and re-announce its Ethernet IP address to rescue it from deletion from all our caches.
					// We set UnansweredQueries to MaxUnansweredQueries to avoid expensive and unnecessary
					// final expiration queries for this record.
					r2->resrec.rroriginalttl = 1;
					r2->TimeRcvd          = m->timenow;
					r2->UnansweredQueries = MaxUnansweredQueries;
					}
				SetNextCacheCheckTime(m, r2);
				}
		if (r1->DelayDelivery)	// If we were planning to delay delivery of this record, see if we still need to
			{
			// Note, only need to call SetNextCacheCheckTime() when DelayDelivery is set, not when it's cleared
			r1->DelayDelivery = CheckForSoonToExpireRecords(m, r1->resrec.name, r1->resrec.namehash, slot);
			if (!r1->DelayDelivery) CacheRecordDeferredAdd(m, r1);
			}
		}
	}

mDNSexport void mDNSCoreReceive(mDNS *const m, void *const pkt, const mDNSu8 *const end,
	const mDNSAddr *const srcaddr, const mDNSIPPort srcport, const mDNSAddr *const dstaddr, const mDNSIPPort dstport,
	const mDNSInterfaceID InterfaceID)
	{
	DNSMessage  *msg  = (DNSMessage *)pkt;
	const mDNSu8 StdQ = kDNSFlag0_QR_Query    | kDNSFlag0_OP_StdQuery;
	const mDNSu8 StdR = kDNSFlag0_QR_Response | kDNSFlag0_OP_StdQuery;
	mDNSu8 QR_OP;
	mDNSu8 *ptr = mDNSNULL;
	const mDNSu8 UpdateR = kDNSFlag0_QR_Response | kDNSFlag0_OP_Update;

#ifndef UNICAST_DISABLED	
	if (srcport.NotAnInteger == NATPMPPort.NotAnInteger)
		{
		mDNS_Lock(m);
		uDNS_ReceiveNATMap(m, pkt, (mDNSu16)(end - (mDNSu8 *)pkt));
		mDNS_Unlock(m);
		return;
		}
#endif		
	if ((unsigned)(end - (mDNSu8 *)pkt) < sizeof(DNSMessageHeader)) { LogMsg("DNS Message too short"); return; }
	QR_OP = (mDNSu8)(msg->h.flags.b[0] & kDNSFlag0_QROP_Mask);
	// Read the integer parts which are in IETF byte-order (MSB first, LSB second)
	ptr = (mDNSu8 *)&msg->h.numQuestions;
	msg->h.numQuestions   = (mDNSu16)((mDNSu16)ptr[0] <<  8 | ptr[1]);
	msg->h.numAnswers     = (mDNSu16)((mDNSu16)ptr[2] <<  8 | ptr[3]);
	msg->h.numAuthorities = (mDNSu16)((mDNSu16)ptr[4] <<  8 | ptr[5]);
	msg->h.numAdditionals = (mDNSu16)((mDNSu16)ptr[6] <<  8 | ptr[7]);

	if (!m) { LogMsg("mDNSCoreReceive ERROR m is NULL"); return; }
	
	// We use zero addresses and all-ones addresses at various places in the code to indicate special values like "no address"
	// If we accept and try to process a packet with zero or all-ones source address, that could really mess things up
	if (!mDNSAddressIsValid(srcaddr)) { debugf("mDNSCoreReceive ignoring packet from %#a", srcaddr); return; }

	mDNS_Lock(m);
	m->PktNum++;
#ifndef UNICAST_DISABLED
	if (!mDNSAddressIsAllDNSLinkGroup(dstaddr) && (QR_OP == StdR || QR_OP == UpdateR))
		uDNS_ReceiveMsg(m, msg, end, srcaddr, srcport, dstaddr, dstport, InterfaceID);
		// Note: mDNSCore also needs to get access to received unicast responses
#endif	
	if      (QR_OP == StdQ) mDNSCoreReceiveQuery   (m, msg, end, srcaddr, srcport, dstaddr, dstport, InterfaceID);
	else if (QR_OP == StdR) mDNSCoreReceiveResponse(m, msg, end, srcaddr, srcport, dstaddr, dstport, InterfaceID);
	else if (QR_OP != UpdateR)
		LogMsg("Unknown DNS packet type %02X%02X from %#-15a:%-5d to %#-15a:%-5d on %p (ignored)",
			msg->h.flags.b[0], msg->h.flags.b[1], srcaddr, mDNSVal16(srcport), dstaddr, mDNSVal16(dstport), InterfaceID);

	// Packet reception often causes a change to the task list:
	// 1. Inbound queries can cause us to need to send responses
	// 2. Conflicing response packets received from other hosts can cause us to need to send defensive responses
	// 3. Other hosts announcing deletion of shared records can cause us to need to re-assert those records
	// 4. Response packets that answer questions may cause our client to issue new questions
	mDNS_Unlock(m);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark -
#pragma mark - Searcher Functions
#endif

#define SameQTarget(A,B) (mDNSSameAddress(&(A)->Target, &(B)->Target) && (A)->TargetPort.NotAnInteger == (B)->TargetPort.NotAnInteger)

mDNSlocal DNSQuestion *FindDuplicateQuestion(const mDNS *const m, const DNSQuestion *const question)
	{
	DNSQuestion *q;
	// Note: A question can only be marked as a duplicate of one that occurs *earlier* in the list.
	// This prevents circular references, where two questions are each marked as a duplicate of the other.
	// Accordingly, we break out of the loop when we get to 'question', because there's no point searching
	// further in the list.
	for (q = m->Questions; q && q != question; q=q->next)		// Scan our list of questions
		if (q->InterfaceID == question->InterfaceID &&			// for another question with the same InterfaceID,
			SameQTarget(q, question)                &&			// and same unicast/multicast target settings
			q->qtype       == question->qtype       &&			// type,
			q->qclass      == question->qclass      &&			// class,
			q->qnamehash   == question->qnamehash   &&
			SameDomainName(&q->qname, &question->qname))		// and name
			return(q);
	return(mDNSNULL);
	}

// This is called after a question is deleted, in case other identical questions were being
// suppressed as duplicates
mDNSlocal void UpdateQuestionDuplicates(mDNS *const m, const DNSQuestion *const question)
	{
	DNSQuestion *q;
	for (q = m->Questions; q; q=q->next)		// Scan our list of questions
		if (q->DuplicateOf == question)			// To see if any questions were referencing this as their duplicate
			{
			q->ThisQInterval    = question->ThisQInterval;
			q->RequestUnicast   = question->RequestUnicast;
			q->LastQTime        = question->LastQTime;
			q->RecentAnswerPkts = 0;
			q->DuplicateOf      = FindDuplicateQuestion(m, q);
			q->LastQTxTime      = question->LastQTxTime;
			SetNextQueryTime(m,q);
			}
	}

#define ValidQuestionTarget(Q) (((Q)->Target.type == mDNSAddrType_IPv4 || (Q)->Target.type == mDNSAddrType_IPv6) && \
	((Q)->TargetPort.NotAnInteger == UnicastDNSPort.NotAnInteger || (Q)->TargetPort.NotAnInteger == MulticastDNSPort.NotAnInteger))

mDNSlocal mStatus mDNS_StartQuery_internal(mDNS *const m, DNSQuestion *const question)
	{
	if (question->Target.type && !ValidQuestionTarget(question))
		{
		LogMsg("Warning! Target.type = %ld port = %u (Client forgot to initialize before calling mDNS_StartQuery?)",
			question->Target.type, mDNSVal16(question->TargetPort));
		question->Target.type = mDNSAddrType_None;
		}

	if (!question->Target.type)		// No question->Target specified, so clear TargetPort and TargetQID
		{
		question->TargetPort = zeroIPPort;
		question->TargetQID  = zeroID;
		}

#ifndef UNICAST_DISABLED	
	// If the client has specified 'kDNSServiceFlagsForceMulticast'
	// then we do a multicast query on that interface, even for unicast domains.
    if (question->InterfaceID == mDNSInterface_LocalOnly || question->ForceMCast || IsLocalDomain(&question->qname))
    	question->uDNS_info.id = zeroID;
    else return uDNS_StartQuery(m, question);
#else
    question->uDNS_info.id = zeroID;
#endif // UNICAST_DISABLED
	
	//LogOperation("mDNS_StartQuery %##s (%s)", question->qname.c, DNSTypeName(question->qtype));
	
	if (m->rrcache_size == 0)	// Can't do queries if we have no cache space allocated
		return(mStatus_NoCache);
	else
		{
		int i;
		// Note: It important that new questions are appended at the *end* of the list, not prepended at the start
		DNSQuestion **q = &m->Questions;
		if (question->InterfaceID == mDNSInterface_LocalOnly) q = &m->LocalOnlyQuestions;
		while (*q && *q != question) q=&(*q)->next;

		if (*q)
			{
			LogMsg("Error! Tried to add a question %##s (%s) that's already in the active list",
				question->qname.c, DNSTypeName(question->qtype));
			return(mStatus_AlreadyRegistered);
			}

		// If this question is referencing a specific interface, verify it exists
		if (question->InterfaceID && question->InterfaceID != mDNSInterface_LocalOnly)
			{
			NetworkInterfaceInfo *intf;
			for (intf = m->HostInterfaces; intf; intf = intf->next)
				if (intf->InterfaceID == question->InterfaceID) break;
			if (!intf)
				LogMsg("Note: InterfaceID %p for question %##s not currently found in active interface list",
					question->InterfaceID, question->qname.c);
			}

		if (!ValidateDomainName(&question->qname))
			{
			LogMsg("Attempt to start query with invalid qname %##s (%s)", question->qname.c, DNSTypeName(question->qtype));
			return(mStatus_Invalid);
			}

		// Note: In the case where we already have the answer to this question in our cache, that may be all the client
		// wanted, and they may immediately cancel their question. In this case, sending an actual query on the wire would
		// be a waste. For that reason, we schedule our first query to go out in half a second. If AnswerNewQuestion() finds
		// that we have *no* relevant answers currently in our cache, then it will accelerate that to go out immediately.
		if (!m->RandomQueryDelay) m->RandomQueryDelay = 1 + (mDNSs32)mDNSRandom((mDNSu32)InitialQuestionInterval);

		question->next              = mDNSNULL;
		question->qnamehash         = DomainNameHashValue(&question->qname);	// MUST do this before FindDuplicateQuestion()
		question->DelayAnswering    = CheckForSoonToExpireRecords(m, &question->qname, question->qnamehash, HashSlot(&question->qname));
		question->ThisQInterval     = InitialQuestionInterval * 2;			// MUST be > zero for an active question
		question->RequestUnicast    = 2;										// Set to 2 because is decremented once *before* we check it
		question->LastQTime         = m->timenow - m->RandomQueryDelay;		// Avoid inter-machine synchronization
		question->LastAnswerPktNum  = m->PktNum;
		question->RecentAnswerPkts  = 0;
		question->CurrentAnswers    = 0;
		question->LargeAnswers      = 0;
		question->UniqueAnswers     = 0;
		question->FlappingInterface = mDNSNULL;
		question->DuplicateOf       = FindDuplicateQuestion(m, question);
		question->NextInDQList      = mDNSNULL;
		for (i=0; i<DupSuppressInfoSize; i++)
			question->DupSuppress[i].InterfaceID = mDNSNULL;
		// question->InterfaceID must be already set by caller
		question->SendQNow          = mDNSNULL;
		question->SendOnAll         = mDNSfalse;
		question->LastQTxTime       = m->timenow;

		if (!question->DuplicateOf)
			verbosedebugf("mDNS_StartQuery_internal: Question %##s (%s) %p %d (%p) started",
				question->qname.c, DNSTypeName(question->qtype), question->InterfaceID,
				question->LastQTime + question->ThisQInterval - m->timenow, question);
		else
			verbosedebugf("mDNS_StartQuery_internal: Question %##s (%s) %p %d (%p) duplicate of (%p)",
				question->qname.c, DNSTypeName(question->qtype), question->InterfaceID,
				question->LastQTime + question->ThisQInterval - m->timenow, question, question->DuplicateOf);

		*q = question;
		if (question->InterfaceID == mDNSInterface_LocalOnly)
			{
			if (!m->NewLocalOnlyQuestions) m->NewLocalOnlyQuestions = question;
			}
		else
			{
			if (!m->NewQuestions) m->NewQuestions = question;
			SetNextQueryTime(m,question);
			}

		return(mStatus_NoError);
		}
	}

mDNSlocal mStatus mDNS_StopQuery_internal(mDNS *const m, DNSQuestion *const question)
	{
	const mDNSu32 slot = HashSlot(&question->qname);
	CacheGroup *cg = CacheGroupForName(m, slot, question->qnamehash, &question->qname);
	CacheRecord *rr;
	DNSQuestion **q = &m->Questions;

    if (uDNS_IsActiveQuery(question, &m->uDNS_info)) return uDNS_StopQuery(m, question);
	
	if (question->InterfaceID == mDNSInterface_LocalOnly) q = &m->LocalOnlyQuestions;
	while (*q && *q != question) q=&(*q)->next;
	if (*q) *q = (*q)->next;
	else
		{
		if (question->ThisQInterval >= 0)	// Only log error message if the query was supposed to be active
			LogMsg("mDNS_StopQuery_internal: Question %##s (%s) not found in active list",
				question->qname.c, DNSTypeName(question->qtype));
		return(mStatus_BadReferenceErr);
		}

	// Take care to cut question from list *before* calling UpdateQuestionDuplicates
	UpdateQuestionDuplicates(m, question);
	// But don't trash ThisQInterval until afterwards.
	question->ThisQInterval = -1;

	// If there are any cache records referencing this as their active question, then see if any other
	// question that is also referencing them, else their CRActiveQuestion needs to get set to NULL.
	for (rr = cg ? cg->members : mDNSNULL; rr; rr=rr->next)
		{
		if (rr->CRActiveQuestion == question)
			{
			DNSQuestion *q;
			for (q = m->Questions; q; q=q->next)		// Scan our list of questions
				if (ActiveQuestion(q) && ResourceRecordAnswersQuestion(&rr->resrec, q))
					break;
			verbosedebugf("mDNS_StopQuery_internal: Cache RR %##s (%s) setting CRActiveQuestion to %p",
				rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype), q);
			rr->CRActiveQuestion = q;		// Question used to be active; new value may or may not be null
			if (!q) m->rrcache_active--;	// If no longer active, decrement rrcache_active count
			}
		}

	// If we just deleted the question that CacheRecordAdd() or CacheRecordRmv()is about to look at,
	// bump its pointer forward one question.
	if (m->CurrentQuestion == question)
		{
		debugf("mDNS_StopQuery_internal: Just deleted the currently active question: %##s (%s)",
			question->qname.c, DNSTypeName(question->qtype));
		m->CurrentQuestion = question->next;
		}

	if (m->NewQuestions == question)
		{
		debugf("mDNS_StopQuery_internal: Just deleted a new question that wasn't even answered yet: %##s (%s)",
			question->qname.c, DNSTypeName(question->qtype));
		m->NewQuestions = question->next;
		}

	if (m->NewLocalOnlyQuestions == question) m->NewLocalOnlyQuestions = question->next;

	// Take care not to trash question->next until *after* we've updated m->CurrentQuestion and m->NewQuestions
	question->next = mDNSNULL;
	return(mStatus_NoError);
	}

mDNSexport mStatus mDNS_StartQuery(mDNS *const m, DNSQuestion *const question)
	{
	mStatus status;
	mDNS_Lock(m);
	status = mDNS_StartQuery_internal(m, question);
	mDNS_Unlock(m);
	return(status);
	}

mDNSexport mStatus mDNS_StopQuery(mDNS *const m, DNSQuestion *const question)
	{
	mStatus status;
	mDNS_Lock(m);
	status = mDNS_StopQuery_internal(m, question);
	mDNS_Unlock(m);
	return(status);
	}

mDNSexport mStatus mDNS_Reconfirm(mDNS *const m, CacheRecord *const rr)
	{
	mStatus status;
	mDNS_Lock(m);
	status = mDNS_Reconfirm_internal(m, rr, kDefaultReconfirmTimeForNoAnswer);
	mDNS_Unlock(m);
	return(status);
	}

mDNSexport mStatus mDNS_ReconfirmByValue(mDNS *const m, ResourceRecord *const rr)
	{
	mStatus status = mStatus_BadReferenceErr;
	CacheRecord *cr;
	mDNS_Lock(m);
	cr = FindIdenticalRecordInCache(m, rr);
	if (cr) status = mDNS_Reconfirm_internal(m, cr, kDefaultReconfirmTimeForNoAnswer);
	mDNS_Unlock(m);
	return(status);
	}

mDNSexport mStatus mDNS_StartBrowse(mDNS *const m, DNSQuestion *const question,
	const domainname *const srv, const domainname *const domain,
	const mDNSInterfaceID InterfaceID, mDNSBool ForceMCast, mDNSQuestionCallback *Callback, void *Context)
	{
	question->InterfaceID      = InterfaceID;
	question->Target           = zeroAddr;
	question->qtype            = kDNSType_PTR;
	question->qclass           = kDNSClass_IN;
	question->LongLived        = mDNSfalse;
	question->ExpectUnique     = mDNSfalse;
	question->ForceMCast       = ForceMCast;
	question->QuestionCallback = Callback;
	question->QuestionContext  = Context;
	if (!ConstructServiceName(&question->qname, mDNSNULL, srv, domain)) return(mStatus_BadParamErr);

#ifndef UNICAST_DISABLED
    if (question->InterfaceID == mDNSInterface_LocalOnly || question->ForceMCast || IsLocalDomain(&question->qname))
    	{
		question->LongLived = mDNSfalse;
		question->uDNS_info.id = zeroID;
		return(mDNS_StartQuery(m, question));
		}
	else
		{
		mStatus status;
		// Need to explicitly lock here, because mDNS_StartQuery does locking but uDNS_StartQuery does not
		mDNS_Lock(m);
		question->LongLived = mDNStrue;
		status = uDNS_StartQuery(m, question);
		mDNS_Unlock(m);
		return(status);
		}
#else
	return(mDNS_StartQuery(m, question));
#endif // UNICAST_DISABLED
	}

mDNSlocal mDNSBool MachineHasActiveIPv6(mDNS *const m)
	{
	NetworkInterfaceInfo *intf;
	for (intf = m->HostInterfaces; intf; intf = intf->next)
	if (intf->ip.type == mDNSAddrType_IPv6) return(mDNStrue);
	return(mDNSfalse);
	}

mDNSlocal void FoundServiceInfoSRV(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, mDNSBool AddRecord)
	{
	ServiceInfoQuery *query = (ServiceInfoQuery *)question->QuestionContext;
	mDNSBool PortChanged = (mDNSBool)(query->info->port.NotAnInteger != answer->rdata->u.srv.port.NotAnInteger);
	if (!AddRecord) return;
	if (answer->rrtype != kDNSType_SRV) return;

	query->info->port = answer->rdata->u.srv.port;

	// If this is our first answer, then set the GotSRV flag and start the address query
	if (!query->GotSRV)
		{
		query->GotSRV             = mDNStrue;
		query->qAv4.InterfaceID   = answer->InterfaceID;
		AssignDomainName(&query->qAv4.qname, &answer->rdata->u.srv.target);
		query->qAv6.InterfaceID   = answer->InterfaceID;
		AssignDomainName(&query->qAv6.qname, &answer->rdata->u.srv.target);
		mDNS_StartQuery(m, &query->qAv4);
		// Only do the AAAA query if this machine actually has IPv6 active
		if (MachineHasActiveIPv6(m)) mDNS_StartQuery(m, &query->qAv6);
		}
	// If this is not our first answer, only re-issue the address query if the target host name has changed
	else if ((query->qAv4.InterfaceID != query->qSRV.InterfaceID && query->qAv4.InterfaceID != answer->InterfaceID) ||
		!SameDomainName(&query->qAv4.qname, &answer->rdata->u.srv.target))
		{
		mDNS_StopQuery(m, &query->qAv4);
		if (query->qAv6.ThisQInterval >= 0) mDNS_StopQuery(m, &query->qAv6);
		if (SameDomainName(&query->qAv4.qname, &answer->rdata->u.srv.target) && !PortChanged)
			{
			// If we get here, it means:
			// 1. This is not our first SRV answer
			// 2. The interface ID is different, but the target host and port are the same
			// This implies that we're seeing the exact same SRV record on more than one interface, so we should
			// make our address queries at least as broad as the original SRV query so that we catch all the answers.
			query->qAv4.InterfaceID = query->qSRV.InterfaceID;	// Will be mDNSInterface_Any, or a specific interface
			query->qAv6.InterfaceID = query->qSRV.InterfaceID;
			}
		else
			{
			query->qAv4.InterfaceID   = answer->InterfaceID;
			AssignDomainName(&query->qAv4.qname, &answer->rdata->u.srv.target);
			query->qAv6.InterfaceID   = answer->InterfaceID;
			AssignDomainName(&query->qAv6.qname, &answer->rdata->u.srv.target);
			}
		debugf("FoundServiceInfoSRV: Restarting address queries for %##s", query->qAv4.qname.c);
		mDNS_StartQuery(m, &query->qAv4);
		// Only do the AAAA query if this machine actually has IPv6 active
		if (MachineHasActiveIPv6(m)) mDNS_StartQuery(m, &query->qAv6);
		}
	else if (query->ServiceInfoQueryCallback && query->GotADD && query->GotTXT && PortChanged)
		{
		if (++query->Answers >= 100)
			debugf("**** WARNING **** Have given %lu answers for %##s (SRV) %##s %u",
				query->Answers, query->qSRV.qname.c, answer->rdata->u.srv.target.c,
				mDNSVal16(answer->rdata->u.srv.port));
		query->ServiceInfoQueryCallback(m, query);
		}
	// CAUTION: MUST NOT do anything more with query after calling query->Callback(), because the client's
	// callback function is allowed to do anything, including deleting this query and freeing its memory.
	}

mDNSlocal void FoundServiceInfoTXT(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, mDNSBool AddRecord)
	{
	ServiceInfoQuery *query = (ServiceInfoQuery *)question->QuestionContext;
	if (!AddRecord) return;
	if (answer->rrtype != kDNSType_TXT) return;
	if (answer->rdlength > sizeof(query->info->TXTinfo)) return;

	query->GotTXT       = mDNStrue;
	query->info->TXTlen = answer->rdlength;
	query->info->TXTinfo[0] = 0;		// In case answer->rdlength is zero
	mDNSPlatformMemCopy(answer->rdata->u.txt.c, query->info->TXTinfo, answer->rdlength);

	verbosedebugf("FoundServiceInfoTXT: %##s GotADD=%d", query->info->name.c, query->GotADD);

	// CAUTION: MUST NOT do anything more with query after calling query->Callback(), because the client's
	// callback function is allowed to do anything, including deleting this query and freeing its memory.
	if (query->ServiceInfoQueryCallback && query->GotADD)
		{
		if (++query->Answers >= 100)
			debugf("**** WARNING **** have given %lu answers for %##s (TXT) %#s...",
				query->Answers, query->qSRV.qname.c, answer->rdata->u.txt.c);
		query->ServiceInfoQueryCallback(m, query);
		}
	}

mDNSlocal void FoundServiceInfo(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, mDNSBool AddRecord)
	{
	ServiceInfoQuery *query = (ServiceInfoQuery *)question->QuestionContext;
	//LogOperation("FoundServiceInfo %d %s", AddRecord, RRDisplayString(m, answer));
	if (!AddRecord) return;
	
	if (answer->rrtype == kDNSType_A)
		{
		query->info->ip.type = mDNSAddrType_IPv4;
		query->info->ip.ip.v4 = answer->rdata->u.ipv4;
		}
	else if (answer->rrtype == kDNSType_AAAA)
		{
		query->info->ip.type = mDNSAddrType_IPv6;
		query->info->ip.ip.v6 = answer->rdata->u.ipv6;
		}
	else
		{
		debugf("FoundServiceInfo: answer %##s type %d (%s) unexpected", answer->name->c, answer->rrtype, DNSTypeName(answer->rrtype));
		return;
		}

	query->GotADD = mDNStrue;
	query->info->InterfaceID = answer->InterfaceID;

	verbosedebugf("FoundServiceInfo v%ld: %##s GotTXT=%d", query->info->ip.type, query->info->name.c, query->GotTXT);

	// CAUTION: MUST NOT do anything more with query after calling query->Callback(), because the client's
	// callback function is allowed to do anything, including deleting this query and freeing its memory.
	if (query->ServiceInfoQueryCallback && query->GotTXT)
		{
		if (++query->Answers >= 100)
			debugf(answer->rrtype == kDNSType_A ?
				"**** WARNING **** have given %lu answers for %##s (A) %.4a" :
				"**** WARNING **** have given %lu answers for %##s (AAAA) %.16a",
				query->Answers, query->qSRV.qname.c, &answer->rdata->u.data);
		query->ServiceInfoQueryCallback(m, query);
		}
	}

// On entry, the client must have set the name and InterfaceID fields of the ServiceInfo structure
// If the query is not interface-specific, then InterfaceID may be zero
// Each time the Callback is invoked, the remainder of the fields will have been filled in
// In addition, InterfaceID will be updated to give the interface identifier corresponding to that response
mDNSexport mStatus mDNS_StartResolveService(mDNS *const m,
	ServiceInfoQuery *query, ServiceInfo *info, mDNSServiceInfoQueryCallback *Callback, void *Context)
	{
	mStatus status;
	mDNS_Lock(m);

	query->qSRV.ThisQInterval       = -1;		// So that mDNS_StopResolveService() knows whether to cancel this question
	query->qSRV.InterfaceID         = info->InterfaceID;
	query->qSRV.Target              = zeroAddr;
	AssignDomainName(&query->qSRV.qname, &info->name);
	query->qSRV.qtype               = kDNSType_SRV;
	query->qSRV.qclass              = kDNSClass_IN;
	query->qSRV.LongLived           = mDNSfalse;
	query->qSRV.ExpectUnique        = mDNStrue;
	query->qSRV.ForceMCast          = mDNSfalse;
	query->qSRV.QuestionCallback    = FoundServiceInfoSRV;
	query->qSRV.QuestionContext     = query;

	query->qTXT.ThisQInterval       = -1;		// So that mDNS_StopResolveService() knows whether to cancel this question
	query->qTXT.InterfaceID         = info->InterfaceID;
	query->qTXT.Target              = zeroAddr;
	AssignDomainName(&query->qTXT.qname, &info->name);
	query->qTXT.qtype               = kDNSType_TXT;
	query->qTXT.qclass              = kDNSClass_IN;
	query->qTXT.LongLived           = mDNSfalse;
	query->qTXT.ExpectUnique        = mDNStrue;
	query->qTXT.ForceMCast          = mDNSfalse;
	query->qTXT.QuestionCallback    = FoundServiceInfoTXT;
	query->qTXT.QuestionContext     = query;

	query->qAv4.ThisQInterval       = -1;		// So that mDNS_StopResolveService() knows whether to cancel this question
	query->qAv4.InterfaceID         = info->InterfaceID;
	query->qAv4.Target              = zeroAddr;
	query->qAv4.qname.c[0]          = 0;
	query->qAv4.qtype               = kDNSType_A;
	query->qAv4.qclass              = kDNSClass_IN;
	query->qAv4.LongLived           = mDNSfalse;
	query->qAv4.ExpectUnique        = mDNStrue;
	query->qAv4.ForceMCast          = mDNSfalse;
	query->qAv4.QuestionCallback    = FoundServiceInfo;
	query->qAv4.QuestionContext     = query;

	query->qAv6.ThisQInterval       = -1;		// So that mDNS_StopResolveService() knows whether to cancel this question
	query->qAv6.InterfaceID         = info->InterfaceID;
	query->qAv6.Target              = zeroAddr;
	query->qAv6.qname.c[0]          = 0;
	query->qAv6.qtype               = kDNSType_AAAA;
	query->qAv6.qclass              = kDNSClass_IN;
	query->qAv6.LongLived           = mDNSfalse;
	query->qAv6.ExpectUnique        = mDNStrue;
	query->qAv6.ForceMCast          = mDNSfalse;
	query->qAv6.QuestionCallback    = FoundServiceInfo;
	query->qAv6.QuestionContext     = query;

	query->GotSRV                   = mDNSfalse;
	query->GotTXT                   = mDNSfalse;
	query->GotADD                   = mDNSfalse;
	query->Answers                  = 0;

	query->info                     = info;
	query->ServiceInfoQueryCallback = Callback;
	query->ServiceInfoQueryContext  = Context;

//	info->name      = Must already be set up by client
//	info->interface = Must already be set up by client
	info->ip        = zeroAddr;
	info->port      = zeroIPPort;
	info->TXTlen    = 0;

	// We use mDNS_StartQuery_internal here because we're already holding the lock
	status = mDNS_StartQuery_internal(m, &query->qSRV);
	if (status == mStatus_NoError) status = mDNS_StartQuery_internal(m, &query->qTXT);
	if (status != mStatus_NoError) mDNS_StopResolveService(m, query);

	mDNS_Unlock(m);
	return(status);
	}

mDNSexport void    mDNS_StopResolveService (mDNS *const m, ServiceInfoQuery *q)
	{
	mDNS_Lock(m);
	// We use mDNS_StopQuery_internal here because we're already holding the lock
	if (q->qSRV.ThisQInterval >= 0 || uDNS_IsActiveQuery(&q->qSRV, &m->uDNS_info)) mDNS_StopQuery_internal(m, &q->qSRV);
	if (q->qTXT.ThisQInterval >= 0 || uDNS_IsActiveQuery(&q->qTXT, &m->uDNS_info)) mDNS_StopQuery_internal(m, &q->qTXT);
	if (q->qAv4.ThisQInterval >= 0 || uDNS_IsActiveQuery(&q->qAv4, &m->uDNS_info)) mDNS_StopQuery_internal(m, &q->qAv4);
	if (q->qAv6.ThisQInterval >= 0 || uDNS_IsActiveQuery(&q->qAv6, &m->uDNS_info)) mDNS_StopQuery_internal(m, &q->qAv6);
	mDNS_Unlock(m);
	}

mDNSexport mStatus mDNS_GetDomains(mDNS *const m, DNSQuestion *const question, mDNS_DomainType DomainType, const domainname *dom,
	const mDNSInterfaceID InterfaceID, mDNSQuestionCallback *Callback, void *Context)
	{
	question->InterfaceID      = InterfaceID;
	question->Target           = zeroAddr;
	question->qtype            = kDNSType_PTR;
	question->qclass           = kDNSClass_IN;
	question->LongLived        = mDNSfalse;
	question->ExpectUnique     = mDNSfalse;
	question->ForceMCast       = mDNSfalse;
	question->QuestionCallback = Callback;
	question->QuestionContext  = Context;
	if (DomainType > mDNS_DomainTypeMax) return(mStatus_BadParamErr);
	if (!MakeDomainNameFromDNSNameString(&question->qname, mDNS_DomainTypeNames[DomainType])) return(mStatus_BadParamErr);
	if (!dom) dom = &localdomain;
	if (!AppendDomainName(&question->qname, dom)) return(mStatus_BadParamErr);
	return(mDNS_StartQuery(m, question));
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Responder Functions
#endif

mDNSexport mStatus mDNS_Register(mDNS *const m, AuthRecord *const rr)
	{
	mStatus status;
	mDNS_Lock(m);
	status = mDNS_Register_internal(m, rr);
	mDNS_Unlock(m);
	return(status);
	}

mDNSexport mStatus mDNS_Update(mDNS *const m, AuthRecord *const rr, mDNSu32 newttl,
	const mDNSu16 newrdlength, RData *const newrdata, mDNSRecordUpdateCallback *Callback)
	{
#ifndef UNICAST_DISABLED
	mDNSBool unicast = !(rr->resrec.InterfaceID == mDNSInterface_LocalOnly || IsLocalDomain(rr->resrec.name));
#else
	mDNSBool unicast = mDNSfalse;
#endif

	if (!ValidateRData(rr->resrec.rrtype, newrdlength, newrdata))
		{
		LogMsg("Attempt to update record with invalid rdata: %s", GetRRDisplayString_rdb(&rr->resrec, &newrdata->u, m->MsgBuffer));
		return(mStatus_Invalid);
		}
	
	mDNS_Lock(m);

	// If TTL is unspecified, leave TTL unchanged
	if (newttl == 0) newttl = rr->resrec.rroriginalttl;

	// If we already have an update queued up which has not gone through yet,
	// give the client a chance to free that memory
	if (!unicast && rr->NewRData)
		{
		RData *n = rr->NewRData;
		rr->NewRData = mDNSNULL;			// Clear the NewRData pointer ...
		if (rr->UpdateCallback)
			rr->UpdateCallback(m, rr, n);	// ...and let the client free this memory, if necessary
		}

	rr->NewRData             = newrdata;
	rr->newrdlength          = newrdlength;
	rr->UpdateCallback       = Callback;

	if (unicast) { mStatus status = uDNS_UpdateRecord(m, rr); mDNS_Unlock(m); return(status); }

	if (rr->resrec.rroriginalttl == newttl &&
		rr->resrec.rdlength == newrdlength && mDNSPlatformMemSame(rr->resrec.rdata->u.data, newrdata->u.data, newrdlength))
		CompleteRDataUpdate(m, rr);
	else
		{
		domainlabel name;
		domainname type, domain;
		DeconstructServiceName(rr->resrec.name, &name, &type, &domain);
		rr->AnnounceCount = InitialAnnounceCount;
		// iChat often does suprious record updates where no data has changed. For the _presence service type, using
		// name/value pairs, the mDNSPlatformMemSame() check above catches this and correctly suppresses the wasteful
		// update. For the _ichat service type, the XML encoding introduces spurious noise differences into the data
		// even though there's no actual semantic change, so the mDNSPlatformMemSame() check doesn't help us.
		// To work around this, we simply unilaterally limit all legacy _ichat-type updates to a single announcement.
		if (SameDomainLabel(type.c, (mDNSu8*)"\x6_ichat")) rr->AnnounceCount = 1;
		rr->ThisAPInterval       = DefaultAPIntervalForRecordType(rr->resrec.RecordType);
		InitializeLastAPTime(m, rr);
		while (rr->NextUpdateCredit && m->timenow - rr->NextUpdateCredit >= 0) GrantUpdateCredit(rr);
		if (!rr->UpdateBlocked && rr->UpdateCredits) rr->UpdateCredits--;
		if (!rr->NextUpdateCredit) rr->NextUpdateCredit = NonZeroTime(m->timenow + kUpdateCreditRefreshInterval);
		if (rr->AnnounceCount > rr->UpdateCredits + 1) rr->AnnounceCount = (mDNSu8)(rr->UpdateCredits + 1);
		if (rr->UpdateCredits <= 5)
			{
			mDNSu32 delay = 6 - rr->UpdateCredits;		// Delay 1 second, then 2, then 3, etc. up to 6 seconds maximum
			if (!rr->UpdateBlocked) rr->UpdateBlocked = NonZeroTime(m->timenow + (mDNSs32)delay * mDNSPlatformOneSecond);
			rr->ThisAPInterval *= 4;
			rr->LastAPTime = rr->UpdateBlocked - rr->ThisAPInterval;
			LogMsg("Excessive update rate for %##s; delaying announcement by %ld second%s",
				rr->resrec.name->c, delay, delay > 1 ? "s" : "");
			}
		rr->resrec.rroriginalttl = newttl;
		}

	mDNS_Unlock(m);
	return(mStatus_NoError);
	}

// NOTE: mDNS_Deregister calls mDNS_Deregister_internal which can call a user callback, which may change
// the record list and/or question list.
// Any code walking either list must use the CurrentQuestion and/or CurrentRecord mechanism to protect against this.
mDNSexport mStatus mDNS_Deregister(mDNS *const m, AuthRecord *const rr)
	{
	mStatus status;
	mDNS_Lock(m);
	status = mDNS_Deregister_internal(m, rr, mDNS_Dereg_normal);
	mDNS_Unlock(m);
	return(status);
	}

mDNSexport void mDNS_HostNameCallback(mDNS *const m, AuthRecord *const rr, mStatus result);

mDNSlocal NetworkInterfaceInfo *FindFirstAdvertisedInterface(mDNS *const m)
	{
	NetworkInterfaceInfo *intf;
	for (intf = m->HostInterfaces; intf; intf = intf->next)
		if (intf->Advertise) break;
	return(intf);
	}

mDNSlocal void AdvertiseInterface(mDNS *const m, NetworkInterfaceInfo *set)
	{
	char buffer[256];
	NetworkInterfaceInfo *primary = FindFirstAdvertisedInterface(m);
	if (!primary) primary = set; // If no existing advertised interface, this new NetworkInterfaceInfo becomes our new primary

	// Send dynamic update for non-linklocal IPv4 Addresses
	mDNS_SetupResourceRecord(&set->RR_A,     mDNSNULL, set->InterfaceID, kDNSType_A,     kHostNameTTL, kDNSRecordTypeUnique,      mDNS_HostNameCallback, set);
	mDNS_SetupResourceRecord(&set->RR_PTR,   mDNSNULL, set->InterfaceID, kDNSType_PTR,   kHostNameTTL, kDNSRecordTypeKnownUnique, mDNSNULL, mDNSNULL);
	mDNS_SetupResourceRecord(&set->RR_HINFO, mDNSNULL, set->InterfaceID, kDNSType_HINFO, kHostNameTTL, kDNSRecordTypeUnique,      mDNSNULL, mDNSNULL);

#if ANSWER_REMOTE_HOSTNAME_QUERIES
	set->RR_A    .AllowRemoteQuery  = mDNStrue;
	set->RR_PTR  .AllowRemoteQuery  = mDNStrue;
	set->RR_HINFO.AllowRemoteQuery  = mDNStrue;
#endif
	// 1. Set up Address record to map from host name ("foo.local.") to IP address
	// 2. Set up reverse-lookup PTR record to map from our address back to our host name
	AssignDomainName(set->RR_A.resrec.name, &m->MulticastHostname);
	if (set->ip.type == mDNSAddrType_IPv4)
		{
		set->RR_A.resrec.rrtype = kDNSType_A;
		set->RR_A.resrec.rdata->u.ipv4 = set->ip.ip.v4;
		// Note: This is reverse order compared to a normal dotted-decimal IP address
		mDNS_snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d.in-addr.arpa.",
			set->ip.ip.v4.b[3], set->ip.ip.v4.b[2], set->ip.ip.v4.b[1], set->ip.ip.v4.b[0]);
		}
	else if (set->ip.type == mDNSAddrType_IPv6)
		{
		int i;
		set->RR_A.resrec.rrtype = kDNSType_AAAA;
		set->RR_A.resrec.rdata->u.ipv6 = set->ip.ip.v6;
		for (i = 0; i < 16; i++)
			{
			static const char hexValues[] = "0123456789ABCDEF";
			buffer[i * 4    ] = hexValues[set->ip.ip.v6.b[15 - i] & 0x0F];
			buffer[i * 4 + 1] = '.';
			buffer[i * 4 + 2] = hexValues[set->ip.ip.v6.b[15 - i] >> 4];
			buffer[i * 4 + 3] = '.';
			}
		mDNS_snprintf(&buffer[64], sizeof(buffer)-64, "ip6.arpa.");
		}

	MakeDomainNameFromDNSNameString(set->RR_PTR.resrec.name, buffer);
	set->RR_PTR.HostTarget = mDNStrue;	// Tell mDNS that the target of this PTR is to be kept in sync with our host name
	set->RR_PTR.ForceMCast = mDNStrue;	// This PTR points to our dot-local name, so don't ever try to write it into a uDNS server

	set->RR_A.RRSet = &primary->RR_A;	// May refer to self

	mDNS_Register_internal(m, &set->RR_A);
	mDNS_Register_internal(m, &set->RR_PTR);

	if (m->HIHardware.c[0] > 0 && m->HISoftware.c[0] > 0 && m->HIHardware.c[0] + m->HISoftware.c[0] <= 254)
		{
		mDNSu8 *p = set->RR_HINFO.resrec.rdata->u.data;
		AssignDomainName(set->RR_HINFO.resrec.name, &m->MulticastHostname);
		set->RR_HINFO.DependentOn = &set->RR_A;
		mDNSPlatformMemCopy(&m->HIHardware, p, 1 + (mDNSu32)m->HIHardware.c[0]);
		p += 1 + (int)p[0];
		mDNSPlatformMemCopy(&m->HISoftware, p, 1 + (mDNSu32)m->HISoftware.c[0]);
		mDNS_Register_internal(m, &set->RR_HINFO);
		}
	else
		{
		debugf("Not creating HINFO record: platform support layer provided no information");
		set->RR_HINFO.resrec.RecordType = kDNSRecordTypeUnregistered;
		}
	}

mDNSlocal void DeadvertiseInterface(mDNS *const m, NetworkInterfaceInfo *set)
	{
	NetworkInterfaceInfo *intf;
	
    // If we still have address records referring to this one, update them
	NetworkInterfaceInfo *primary = FindFirstAdvertisedInterface(m);
	AuthRecord *A = primary ? &primary->RR_A : mDNSNULL;
	for (intf = m->HostInterfaces; intf; intf = intf->next)
		if (intf->RR_A.RRSet == &set->RR_A)
			intf->RR_A.RRSet = A;

	// Unregister these records.
	// When doing the mDNS_Close processing, we first call DeadvertiseInterface for each interface, so by the time the platform
	// support layer gets to call mDNS_DeregisterInterface, the address and PTR records have already been deregistered for it.
	// Also, in the event of a name conflict, one or more of our records will have been forcibly deregistered.
	// To avoid unnecessary and misleading warning messages, we check the RecordType before calling mDNS_Deregister_internal().
	if (set->RR_A.    resrec.RecordType) mDNS_Deregister_internal(m, &set->RR_A,     mDNS_Dereg_normal);
	if (set->RR_PTR.  resrec.RecordType) mDNS_Deregister_internal(m, &set->RR_PTR,   mDNS_Dereg_normal);
	if (set->RR_HINFO.resrec.RecordType) mDNS_Deregister_internal(m, &set->RR_HINFO, mDNS_Dereg_normal);
	}

mDNSexport void mDNS_SetFQDN(mDNS *const m)
	{
	domainname newmname;
	NetworkInterfaceInfo *intf;
	AuthRecord *rr;
	newmname.c[0] = 0;

	if (!AppendDomainLabel(&newmname, &m->hostlabel))  { LogMsg("ERROR: mDNS_SetFQDN: Cannot create MulticastHostname"); return; }
	if (!AppendLiteralLabelString(&newmname, "local")) { LogMsg("ERROR: mDNS_SetFQDN: Cannot create MulticastHostname"); return; }
	if (SameDomainName(&m->MulticastHostname, &newmname)) { LogMsg("mDNS_SetFQDN - hostname unchanged"); return; }

	mDNS_Lock(m);
	AssignDomainName(&m->MulticastHostname, &newmname);

	// 1. Stop advertising our address records on all interfaces
	for (intf = m->HostInterfaces; intf; intf = intf->next)
		if (intf->Advertise) DeadvertiseInterface(m, intf);

	// 2. Start advertising our address records using the new name
	for (intf = m->HostInterfaces; intf; intf = intf->next)
		if (intf->Advertise) AdvertiseInterface(m, intf);

	// 3. Make sure that any SRV records (and the like) that reference our
	// host name in their rdata get updated to reference this new host name
	for (rr = m->ResourceRecords;  rr; rr=rr->next) if (rr->HostTarget) SetTargetToHostName(m, rr);
	for (rr = m->DuplicateRecords; rr; rr=rr->next) if (rr->HostTarget) SetTargetToHostName(m, rr);
   
	mDNS_Unlock(m);
	}

mDNSexport void mDNS_HostNameCallback(mDNS *const m, AuthRecord *const rr, mStatus result)
	{
	(void)rr;	// Unused parameter
	
	#if MDNS_DEBUGMSGS
		{
		char *msg = "Unknown result";
		if      (result == mStatus_NoError)      msg = "Name registered";
		else if (result == mStatus_NameConflict) msg = "Name conflict";
		debugf("mDNS_HostNameCallback: %##s (%s) %s (%ld)", rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype), msg, result);
		}
	#endif

	if (result == mStatus_NoError)
		{
		// Notify the client that the host name is successfully registered
		if (m->MainCallback)
			{
			m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
			m->MainCallback(m, result);
			m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
			}
		}
	else if (result == mStatus_NameConflict)
		{
		domainlabel oldlabel = m->hostlabel;

		// 1. First give the client callback a chance to pick a new name
		if (m->MainCallback)
			{
			m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
			m->MainCallback(m, mStatus_NameConflict);
			m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
			}

		// 2. If the client callback didn't do it, add (or increment) an index ourselves
		if (SameDomainLabel(m->hostlabel.c, oldlabel.c))
			IncrementLabelSuffix(&m->hostlabel, mDNSfalse);
		
		// 3. Generate the FQDNs from the hostlabel,
		// and make sure all SRV records, etc., are updated to reference our new hostname
		mDNS_SetFQDN(m);
		LogMsg("Local Hostname %#s.local already in use; will try %#s.local instead", oldlabel.c, m->hostlabel.c);
		}
	else if (result == mStatus_MemFree)
		{
		// .local hostnames do not require goodbyes - we ignore the MemFree (which is sent directly by
		// mDNS_Deregister_internal), and allow the caller to deallocate immediately following mDNS_DeadvertiseInterface
		debugf("mDNS_HostNameCallback: MemFree (ignored)");
		}
	else
		LogMsg("mDNS_HostNameCallback: Unknown error %ld for registration of record %s", result,  rr->resrec.name->c);
	}

mDNSlocal void UpdateInterfaceProtocols(mDNS *const m, NetworkInterfaceInfo *active)
	{
	NetworkInterfaceInfo *intf;
	active->IPv4Available = mDNSfalse;
	active->IPv6Available = mDNSfalse;
	for (intf = m->HostInterfaces; intf; intf = intf->next)
		if (intf->InterfaceID == active->InterfaceID)
			{
			if (intf->ip.type == mDNSAddrType_IPv4 && intf->McastTxRx) active->IPv4Available = mDNStrue;
			if (intf->ip.type == mDNSAddrType_IPv6 && intf->McastTxRx) active->IPv6Available = mDNStrue;
			}
	}

mDNSexport mStatus mDNS_RegisterInterface(mDNS *const m, NetworkInterfaceInfo *set, mDNSBool flapping)
	{
	mDNSBool FirstOfType = mDNStrue;
	NetworkInterfaceInfo **p = &m->HostInterfaces;

	if (!set->InterfaceID)
		{ LogMsg("Error! Tried to register a NetworkInterfaceInfo %#a with zero InterfaceID", &set->ip); return(mStatus_Invalid); }

	if (!mDNSAddressIsValidNonZero(&set->mask))
		{ LogMsg("Error! Tried to register a NetworkInterfaceInfo %#a with invalid mask %#a", &set->ip, &set->mask); return(mStatus_Invalid); }

	mDNS_Lock(m);
	
	// Assume this interface will be active now, unless we find a duplicate already in the list
	set->InterfaceActive = mDNStrue;
	set->IPv4Available   = (set->ip.type == mDNSAddrType_IPv4 && set->McastTxRx);
	set->IPv6Available   = (set->ip.type == mDNSAddrType_IPv6 && set->McastTxRx);

	// Scan list to see if this InterfaceID is already represented
	while (*p)
		{
		if (*p == set)
			{
			LogMsg("Error! Tried to register a NetworkInterfaceInfo that's already in the list");
			mDNS_Unlock(m);
			return(mStatus_AlreadyRegistered);
			}

		if ((*p)->InterfaceID == set->InterfaceID)
			{
			// This InterfaceID already represented by a different interface in the list, so mark this instance inactive for now
			set->InterfaceActive = mDNSfalse;
			if (set->ip.type == (*p)->ip.type) FirstOfType = mDNSfalse;
			if (set->ip.type == mDNSAddrType_IPv4 && set->McastTxRx) (*p)->IPv4Available = mDNStrue;
			if (set->ip.type == mDNSAddrType_IPv6 && set->McastTxRx) (*p)->IPv6Available = mDNStrue;
			}

		p=&(*p)->next;
		}

	set->next = mDNSNULL;
	*p = set;
	
	if (set->Advertise)
		AdvertiseInterface(m, set);

	LogOperation("mDNS_RegisterInterface: InterfaceID %p %s (%#a) %s", set->InterfaceID, set->ifname, &set->ip,
		set->InterfaceActive ?
			"not represented in list; marking active and retriggering queries" :
			"already represented in list; marking inactive for now");
	
	// In early versions of OS X the IPv6 address remains on an interface even when the interface is turned off,
	// giving the false impression that there's an active representative of this interface when there really isn't.
	// Therefore, when registering an interface, we want to re-trigger our questions and re-probe our Resource Records,
	// even if we believe that we previously had an active representative of this interface.
	if (set->McastTxRx && ((m->KnownBugs & mDNS_KnownBug_PhantomInterfaces) || FirstOfType || set->InterfaceActive))
		{
		DNSQuestion *q;
		AuthRecord *rr;
		// If flapping, delay between first and second queries is eight seconds instead of one
		mDNSs32 delay    = flapping ? mDNSPlatformOneSecond   * 5 : 0;
		mDNSu8  announce = flapping ? (mDNSu8)1                   : InitialAnnounceCount;

		// Use a small amount of randomness:
		// In the case of a network administrator turning on an Ethernet hub so that all the
		// connected machines establish link at exactly the same time, we don't want them all
		// to go and hit the network with identical queries at exactly the same moment.
		if (!m->SuppressSending) m->SuppressSending = m->timenow + (mDNSs32)mDNSRandom((mDNSu32)InitialQuestionInterval);
		
		if (flapping)
			{
			LogMsg("Note: Frequent transitions for interface %s (%#a); network traffic reduction measures in effect", set->ifname, &set->ip);
			if (!m->SuppressProbes ||
				m->SuppressProbes - (m->timenow + delay) < 0)
				m->SuppressProbes = (m->timenow + delay);
			}

		for (q = m->Questions; q; q=q->next)							// Scan our list of questions
			if (!q->InterfaceID || q->InterfaceID == set->InterfaceID)	// If non-specific Q, or Q on this specific interface,
				{														// then reactivate this question
				mDNSs32 initial  = (flapping && q->FlappingInterface != set->InterfaceID) ? InitialQuestionInterval * 8 : InitialQuestionInterval;
				mDNSs32 qdelay   = (flapping && q->FlappingInterface != set->InterfaceID) ? mDNSPlatformOneSecond   * 5 : 0;
				if (flapping && q->FlappingInterface == set->InterfaceID)
					LogOperation("No cache records for %##s (%s) expired; no need for immediate question", q->qname.c, DNSTypeName(q->qtype));
					
				if (!q->ThisQInterval || q->ThisQInterval > initial)
					{
					q->ThisQInterval = initial;
					q->RequestUnicast = 2; // Set to 2 because is decremented once *before* we check it
					}
				if (q->LastQTime - (m->timenow - q->ThisQInterval + qdelay) > 0)
					q->LastQTime = (m->timenow - q->ThisQInterval + qdelay);
				q->RecentAnswerPkts = 0;
				SetNextQueryTime(m,q);
				}
		
		// For all our non-specific authoritative resource records (and any dormant records specific to this interface)
		// we now need them to re-probe if necessary, and then re-announce.
		for (rr = m->ResourceRecords; rr; rr=rr->next)
			if (!rr->resrec.InterfaceID || rr->resrec.InterfaceID == set->InterfaceID)
				{
				if (rr->resrec.RecordType == kDNSRecordTypeVerified && !rr->DependentOn) rr->resrec.RecordType = kDNSRecordTypeUnique;
				rr->ProbeCount     = DefaultProbeCountForRecordType(rr->resrec.RecordType);
				if (rr->AnnounceCount < announce) rr->AnnounceCount  = announce;
				rr->ThisAPInterval = DefaultAPIntervalForRecordType(rr->resrec.RecordType);
				InitializeLastAPTime(m, rr);
				}
		}

	mDNS_Unlock(m);
	return(mStatus_NoError);
	}

// NOTE: mDNS_DeregisterInterface calls mDNS_Deregister_internal which can call a user callback, which may change
// the record list and/or question list.
// Any code walking either list must use the CurrentQuestion and/or CurrentRecord mechanism to protect against this.
mDNSexport void mDNS_DeregisterInterface(mDNS *const m, NetworkInterfaceInfo *set, mDNSBool flapping)
	{
	NetworkInterfaceInfo **p = &m->HostInterfaces;
	
	mDNSBool revalidate = mDNSfalse;
	// If this platform has the "phantom interfaces" known bug (e.g. Jaguar), we have to revalidate records every
	// time an interface goes away. Otherwise, when you disconnect the Ethernet cable, the system reports that it
	// still has an IPv6 address, and if we don't revalidate those records don't get deleted in a timely fashion.
	if (m->KnownBugs & mDNS_KnownBug_PhantomInterfaces) revalidate = mDNStrue;

	mDNS_Lock(m);

	// Find this record in our list
	while (*p && *p != set) p=&(*p)->next;
	if (!*p) { debugf("mDNS_DeregisterInterface: NetworkInterfaceInfo not found in list"); mDNS_Unlock(m); return; }

	// Unlink this record from our list
	*p = (*p)->next;
	set->next = mDNSNULL;

	if (!set->InterfaceActive)
		{
		// If this interface not the active member of its set, update the v4/v6Available flags for the active member
		NetworkInterfaceInfo *intf;
		for (intf = m->HostInterfaces; intf; intf = intf->next)
			if (intf->InterfaceActive && intf->InterfaceID == set->InterfaceID)
				UpdateInterfaceProtocols(m, intf);
		}
	else
		{
		NetworkInterfaceInfo *intf;
		for (intf = m->HostInterfaces; intf; intf = intf->next)
			if (intf->InterfaceID == set->InterfaceID)
				break;
		if (intf)
			{
			LogOperation("mDNS_DeregisterInterface: Another representative of InterfaceID %p %s (%#a) exists;"
				" making it active", set->InterfaceID, set->ifname, &set->ip);
			intf->InterfaceActive = mDNStrue;
			UpdateInterfaceProtocols(m, intf);
			
			// See if another representative *of the same type* exists. If not, we mave have gone from
			// dual-stack to v6-only (or v4-only) so we need to reconfirm which records are still valid.
			for (intf = m->HostInterfaces; intf; intf = intf->next)
				if (intf->InterfaceID == set->InterfaceID && intf->ip.type == set->ip.type)
					break;
			if (!intf) revalidate = mDNStrue;
			}
		else
			{
			mDNSu32 slot;
			CacheGroup *cg;
			CacheRecord *rr;
			DNSQuestion *q;
			LogOperation("mDNS_DeregisterInterface: Last representative of InterfaceID %p %s (%#a) deregistered;"
				" marking questions etc. dormant", set->InterfaceID, set->ifname, &set->ip);

			if (flapping)
				LogMsg("Note: Frequent transitions for interface %s (%#a); network traffic reduction measures in effect",
					set->ifname, &set->ip);

			// 1. Deactivate any questions specific to this interface, and tag appropriate questions
			// so that mDNS_RegisterInterface() knows how swiftly it needs to reactivate them
			for (q = m->Questions; q; q=q->next)
				{
				if (q->InterfaceID == set->InterfaceID) q->ThisQInterval = 0;
				if (!q->InterfaceID || q->InterfaceID == set->InterfaceID)
					q->FlappingInterface = set->InterfaceID;
				}

			// 2. Flush any cache records received on this interface
			revalidate = mDNSfalse;		// Don't revalidate if we're flushing the records
			FORALL_CACHERECORDS(slot, cg, rr)
				if (rr->resrec.InterfaceID == set->InterfaceID)
					{
					// If this interface is deemed flapping,
					// postpone deleting the cache records in case the interface comes back again
					if (!flapping) PurgeCacheResourceRecord(m, rr);
					else mDNS_Reconfirm_internal(m, rr, kDefaultReconfirmTimeForFlappingInterface);
					}
			}
		}

	// If we were advertising on this interface, deregister those address and reverse-lookup records now
	if (set->Advertise) DeadvertiseInterface(m, set);

	// If we have any cache records received on this interface that went away, then re-verify them.
	// In some versions of OS X the IPv6 address remains on an interface even when the interface is turned off,
	// giving the false impression that there's an active representative of this interface when there really isn't.
	// Don't need to do this when shutting down, because *all* interfaces are about to go away
	if (revalidate && !m->mDNS_shutdown)
		{
		mDNSu32 slot;
		CacheGroup *cg;
		CacheRecord *rr;
		m->NextCacheCheck = m->timenow;
		FORALL_CACHERECORDS(slot, cg, rr)
			if (rr->resrec.InterfaceID == set->InterfaceID)
				mDNS_Reconfirm_internal(m, rr, kDefaultReconfirmTimeForFlappingInterface);
		}

	mDNS_Unlock(m);
	}

mDNSlocal void ServiceCallback(mDNS *const m, AuthRecord *const rr, mStatus result)
	{
	ServiceRecordSet *sr = (ServiceRecordSet *)rr->RecordContext;
	(void)m;	// Unused parameter

	#if MDNS_DEBUGMSGS
		{
		char *msg = "Unknown result";
		if      (result == mStatus_NoError)      msg = "Name Registered";
		else if (result == mStatus_NameConflict) msg = "Name Conflict";
		else if (result == mStatus_MemFree)      msg = "Memory Free";
		debugf("ServiceCallback: %##s (%s) %s (%ld)", rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype), msg, result);
		}
	#endif

	// Only pass on the NoError acknowledgement for the SRV record (when it finishes probing)
	if (result == mStatus_NoError && rr != &sr->RR_SRV) return;

	// If we got a name conflict on either SRV or TXT, forcibly deregister this service, and record that we did that
	if (result == mStatus_NameConflict)
		{
		sr->Conflict = mDNStrue;				// Record that this service set had a conflict
		mDNS_DeregisterService(m, sr);			// Unlink the records from our list
		return;
		}
	
	if (result == mStatus_MemFree)
		{
		// If the PTR record or any of the subtype PTR records are still in the process of deregistering,
		// don't pass on the NameConflict/MemFree message until every record is finished cleaning up.
		mDNSu32 i;
		if (sr->RR_PTR.resrec.RecordType != kDNSRecordTypeUnregistered) return;
		for (i=0; i<sr->NumSubTypes; i++) if (sr->SubTypes[i].resrec.RecordType != kDNSRecordTypeUnregistered) return;

		// If this ServiceRecordSet was forcibly deregistered, and now its memory is ready for reuse,
		// then we can now report the NameConflict to the client
		if (sr->Conflict) result = mStatus_NameConflict;
		}

	// CAUTION: MUST NOT do anything more with sr after calling sr->Callback(), because the client's callback
	// function is allowed to do anything, including deregistering this service and freeing its memory.
	if (sr->ServiceCallback)
		sr->ServiceCallback(m, sr, result);
	}

mDNSlocal void NSSCallback(mDNS *const m, AuthRecord *const rr, mStatus result)
	{
	ServiceRecordSet *sr = (ServiceRecordSet *)rr->RecordContext;
	if (sr->ServiceCallback)
		sr->ServiceCallback(m, sr, result);
	}

// Note:
// Name is first label of domain name (any dots in the name are actual dots, not label separators)
// Type is service type (e.g. "_ipp._tcp.")
// Domain is fully qualified domain name (i.e. ending with a null label)
// We always register a TXT, even if it is empty (so that clients are not
// left waiting forever looking for a nonexistent record.)
// If the host parameter is mDNSNULL or the root domain (ASCII NUL),
// then the default host name (m->MulticastHostname) is automatically used
mDNSexport mStatus mDNS_RegisterService(mDNS *const m, ServiceRecordSet *sr,
	const domainlabel *const name, const domainname *const type, const domainname *const domain,
	const domainname *const host, mDNSIPPort port, const mDNSu8 txtinfo[], mDNSu16 txtlen,
	AuthRecord *SubTypes, mDNSu32 NumSubTypes,
	const mDNSInterfaceID InterfaceID, mDNSServiceCallback Callback, void *Context)
	{
	mStatus err;
	mDNSu32 i;

	sr->ServiceCallback = Callback;
	sr->ServiceContext  = Context;
	sr->Extras          = mDNSNULL;
	sr->NumSubTypes     = NumSubTypes;
	sr->SubTypes        = SubTypes;
	sr->Conflict        = mDNSfalse;
	if (host && host->c[0]) sr->Host = *host;
	else sr->Host.c[0] = 0;
	
	// If port number is zero, that means the client is really trying to do a RegisterNoSuchService
	if (!port.NotAnInteger)
		return(mDNS_RegisterNoSuchService(m, &sr->RR_SRV, name, type, domain, mDNSNULL, mDNSInterface_Any, NSSCallback, sr));

	// Initialize the AuthRecord objects to sane values
	mDNS_SetupResourceRecord(&sr->RR_ADV, mDNSNULL, InterfaceID, kDNSType_PTR, kStandardTTL, kDNSRecordTypeAdvisory, ServiceCallback, sr);
	mDNS_SetupResourceRecord(&sr->RR_PTR, mDNSNULL, InterfaceID, kDNSType_PTR, kStandardTTL, kDNSRecordTypeShared,   ServiceCallback, sr);
	mDNS_SetupResourceRecord(&sr->RR_SRV, mDNSNULL, InterfaceID, kDNSType_SRV, kHostNameTTL, kDNSRecordTypeUnique,   ServiceCallback, sr);
	mDNS_SetupResourceRecord(&sr->RR_TXT, mDNSNULL, InterfaceID, kDNSType_TXT, kStandardTTL, kDNSRecordTypeUnique,   ServiceCallback, sr);

	// If the client is registering an oversized TXT record,
	// it is the client's responsibility to alloate a ServiceRecordSet structure that is large enough for it
	if (sr->RR_TXT.resrec.rdata->MaxRDLength < txtlen)
		sr->RR_TXT.resrec.rdata->MaxRDLength = txtlen;

	// Set up the record names
	// For now we only create an advisory record for the main type, not for subtypes
	// We need to gain some operational experience before we decide if there's a need to create them for subtypes too
	if (ConstructServiceName(sr->RR_ADV.resrec.name, (domainlabel*)"\x09_services", (domainname*)"\x07_dns-sd\x04_udp", domain) == mDNSNULL)
		return(mStatus_BadParamErr);
	if (ConstructServiceName(sr->RR_PTR.resrec.name, mDNSNULL, type, domain) == mDNSNULL) return(mStatus_BadParamErr);
	if (ConstructServiceName(sr->RR_SRV.resrec.name, name,     type, domain) == mDNSNULL) return(mStatus_BadParamErr);
	AssignDomainName(sr->RR_TXT.resrec.name, sr->RR_SRV.resrec.name);
	
	// 1. Set up the ADV record rdata to advertise our service type
	AssignDomainName(&sr->RR_ADV.resrec.rdata->u.name, sr->RR_PTR.resrec.name);

	// 2. Set up the PTR record rdata to point to our service name
	// We set up two additionals, so when a client asks for this PTR we automatically send the SRV and the TXT too
	AssignDomainName(&sr->RR_PTR.resrec.rdata->u.name, sr->RR_SRV.resrec.name);
	sr->RR_PTR.Additional1 = &sr->RR_SRV;
	sr->RR_PTR.Additional2 = &sr->RR_TXT;

	// 2a. Set up any subtype PTRs to point to our service name
	// If the client is using subtypes, it is the client's responsibility to have
	// already set the first label of the record name to the subtype being registered
	for (i=0; i<NumSubTypes; i++)
		{
		domainname st;
		AssignDomainName(&st, sr->SubTypes[i].resrec.name);
		st.c[1+st.c[0]] = 0;			// Only want the first label, not the whole FQDN (particularly for mDNS_RenameAndReregisterService())
		AppendDomainName(&st, type);
		mDNS_SetupResourceRecord(&sr->SubTypes[i], mDNSNULL, InterfaceID, kDNSType_PTR, kStandardTTL, kDNSRecordTypeShared, ServiceCallback, sr);
		if (ConstructServiceName(sr->SubTypes[i].resrec.name, mDNSNULL, &st, domain) == mDNSNULL) return(mStatus_BadParamErr);
		AssignDomainName(&sr->SubTypes[i].resrec.rdata->u.name, sr->RR_SRV.resrec.name);
		sr->SubTypes[i].Additional1 = &sr->RR_SRV;
		sr->SubTypes[i].Additional2 = &sr->RR_TXT;
		}

	// 3. Set up the SRV record rdata.
	sr->RR_SRV.resrec.rdata->u.srv.priority = 0;
	sr->RR_SRV.resrec.rdata->u.srv.weight   = 0;
	sr->RR_SRV.resrec.rdata->u.srv.port     = port;

	// Setting HostTarget tells DNS that the target of this SRV is to be automatically kept in sync with our host name
	if (sr->Host.c[0]) AssignDomainName(&sr->RR_SRV.resrec.rdata->u.srv.target, &sr->Host);
	else { sr->RR_SRV.HostTarget = mDNStrue; sr->RR_SRV.resrec.rdata->u.srv.target.c[0] = '\0'; }

	// 4. Set up the TXT record rdata,
	// and set DependentOn because we're depending on the SRV record to find and resolve conflicts for us
	if (txtinfo == mDNSNULL) sr->RR_TXT.resrec.rdlength = 0;
	else if (txtinfo != sr->RR_TXT.resrec.rdata->u.txt.c)
		{
		sr->RR_TXT.resrec.rdlength = txtlen;
		if (sr->RR_TXT.resrec.rdlength > sr->RR_TXT.resrec.rdata->MaxRDLength) return(mStatus_BadParamErr);
		mDNSPlatformMemCopy(txtinfo, sr->RR_TXT.resrec.rdata->u.txt.c, txtlen);
		}
	sr->RR_TXT.DependentOn = &sr->RR_SRV;

#ifndef UNICAST_DISABLED	
	// If the client has specified an explicit InterfaceID,
	// then we do a multicast registration on that interface, even for unicast domains.
	if (!(InterfaceID == mDNSInterface_LocalOnly || IsLocalDomain(sr->RR_SRV.resrec.name)))
		{
		mStatus status;
		mDNS_Lock(m);
		// BIND named (name daemon) doesn't allow TXT records with zero-length rdata. This is strictly speaking correct,
		// since RFC 1035 specifies a TXT record as "One or more <character-string>s", not "Zero or more <character-string>s".
		// Since some legacy apps try to create zero-length TXT records, we'll silently correct it here.
		// (We have to duplicate this check here because uDNS_RegisterService() bypasses the usual mDNS_Register_internal() bottleneck)
		if (!sr->RR_TXT.resrec.rdlength) { sr->RR_TXT.resrec.rdlength = 1; sr->RR_TXT.resrec.rdata->u.txt.c[0] = 0; }
		status = uDNS_RegisterService(m, sr);
		mDNS_Unlock(m);
		return(status);
		}
#endif
	mDNS_Lock(m);
	err = mDNS_Register_internal(m, &sr->RR_SRV);
	if (!err) err = mDNS_Register_internal(m, &sr->RR_TXT);
	// We register the RR_PTR last, because we want to be sure that in the event of a forced call to
	// mDNS_Close, the RR_PTR will be the last one to be forcibly deregistered, since that is what triggers
	// the mStatus_MemFree callback to ServiceCallback, which in turn passes on the mStatus_MemFree back to
	// the client callback, which is then at liberty to free the ServiceRecordSet memory at will. We need to
	// make sure we've deregistered all our records and done any other necessary cleanup before that happens.
	if (!err) err = mDNS_Register_internal(m, &sr->RR_ADV);
	for (i=0; i<NumSubTypes; i++) if (!err) err = mDNS_Register_internal(m, &sr->SubTypes[i]);
	if (!err) err = mDNS_Register_internal(m, &sr->RR_PTR);

	mDNS_Unlock(m);
	
	if (err) mDNS_DeregisterService(m, sr);
	return(err);
	}

mDNSexport mStatus mDNS_AddRecordToService(mDNS *const m, ServiceRecordSet *sr,
	ExtraResourceRecord *extra, RData *rdata, mDNSu32 ttl)
	{
	ExtraResourceRecord **e;
	mStatus status;

	extra->next = mDNSNULL;
	mDNS_SetupResourceRecord(&extra->r, rdata, sr->RR_PTR.resrec.InterfaceID,
		extra->r.resrec.rrtype, ttl, kDNSRecordTypeUnique, ServiceCallback, sr);
	AssignDomainName(extra->r.resrec.name, sr->RR_SRV.resrec.name);
	
#ifndef UNICAST_DISABLED
	if (!(sr->RR_SRV.resrec.InterfaceID == mDNSInterface_LocalOnly || IsLocalDomain(sr->RR_SRV.resrec.name)))
		{
		mDNS_Lock(m);
		// BIND named (name daemon) doesn't allow TXT records with zero-length rdata. This is strictly speaking correct,
		// since RFC 1035 specifies a TXT record as "One or more <character-string>s", not "Zero or more <character-string>s".
		// Since some legacy apps try to create zero-length TXT records, we'll silently correct it here.
		// (We have to duplicate this check here because uDNS_AddRecordToService() bypasses the usual mDNS_Register_internal() bottleneck)
		if (extra->r.resrec.rrtype == kDNSType_TXT && extra->r.resrec.rdlength == 0)
			{ extra->r.resrec.rdlength = 1; extra->r.resrec.rdata->u.txt.c[0] = 0; }
		status = uDNS_AddRecordToService(m, sr, extra);
		mDNS_Unlock(m);
		return status;
		}
#endif
	
	mDNS_Lock(m);
	e = &sr->Extras;
	while (*e) e = &(*e)->next;

	if (ttl == 0) ttl = kStandardTTL;

	extra->r.DependentOn = &sr->RR_SRV;
	
	debugf("mDNS_AddRecordToService adding record to %##s", extra->r.resrec.name->c);
	
	status = mDNS_Register_internal(m, &extra->r);
	if (status == mStatus_NoError) *e = extra;
	mDNS_Unlock(m);
	return(status);
	}

mDNSexport mStatus mDNS_RemoveRecordFromService(mDNS *const m, ServiceRecordSet *sr, ExtraResourceRecord *extra,
	mDNSRecordCallback MemFreeCallback, void *Context)
	{
	ExtraResourceRecord **e;
	mStatus status;

	mDNS_Lock(m);
	e = &sr->Extras;
	while (*e && *e != extra) e = &(*e)->next;
	if (!*e)
		{
		debugf("mDNS_RemoveRecordFromService failed to remove record from %##s", extra->r.resrec.name->c);
		status = mStatus_BadReferenceErr;
		}
	else
		{
		debugf("mDNS_RemoveRecordFromService removing record from %##s", extra->r.resrec.name->c);
		extra->r.RecordCallback = MemFreeCallback;
		extra->r.RecordContext  = Context;
		*e = (*e)->next;
#ifndef UNICAST_DISABLED	
		if (!(sr->RR_SRV.resrec.InterfaceID == mDNSInterface_LocalOnly || IsLocalDomain(sr->RR_SRV.resrec.name)))
			status = uDNS_DeregisterRecord(m, &extra->r);
		else
#endif
		status = mDNS_Deregister_internal(m, &extra->r, mDNS_Dereg_normal);
		}
	mDNS_Unlock(m);
	return(status);
	}

mDNSexport mStatus mDNS_RenameAndReregisterService(mDNS *const m, ServiceRecordSet *const sr, const domainlabel *newname)
	{
	// NOTE: Don't need to use mDNS_Lock(m) here, because this code is just using public routines
	// mDNS_RegisterService() and mDNS_AddRecordToService(), which do the right locking internally.
	domainlabel name1, name2;
	domainname type, domain;
	domainname *host = mDNSNULL;
	ExtraResourceRecord *extras = sr->Extras;
	mStatus err;

	DeconstructServiceName(sr->RR_SRV.resrec.name, &name1, &type, &domain);
	if (!newname)
		{
		name2 = name1;
		IncrementLabelSuffix(&name2, mDNStrue);
		newname = &name2;
		}
	
	if (SameDomainName(&domain, &localdomain))
		LogMsg("%##s service renamed from \"%#s\" to \"%#s\"", type.c, name1.c, newname->c);
	else LogMsg("%##s service (domain %##s) renamed from \"%#s\" to \"%#s\"",type.c, domain.c, name1.c, newname->c);

	if (sr->RR_SRV.HostTarget == mDNSfalse && sr->Host.c[0]) host = &sr->Host;
	
	err = mDNS_RegisterService(m, sr, newname, &type, &domain,
		host, sr->RR_SRV.resrec.rdata->u.srv.port, sr->RR_TXT.resrec.rdata->u.txt.c, sr->RR_TXT.resrec.rdlength,
		sr->SubTypes, sr->NumSubTypes,
		sr->RR_PTR.resrec.InterfaceID, sr->ServiceCallback, sr->ServiceContext);

	// mDNS_RegisterService() just reset sr->Extras to NULL.
	// Fortunately we already grabbed ourselves a copy of this pointer (above), so we can now run
	// through the old list of extra records, and re-add them to our freshly created service registration
	while (!err && extras)
		{
		ExtraResourceRecord *e = extras;
		extras = extras->next;
		err = mDNS_AddRecordToService(m, sr, e, e->r.resrec.rdata, e->r.resrec.rroriginalttl);
		}
	
	return(err);
	}

// NOTE: mDNS_DeregisterService calls mDNS_Deregister_internal which can call a user callback,
// which may change the record list and/or question list.
// Any code walking either list must use the CurrentQuestion and/or CurrentRecord mechanism to protect against this.
mDNSexport mStatus mDNS_DeregisterService(mDNS *const m, ServiceRecordSet *sr)
	{
	// If port number is zero, that means this was actually registered using mDNS_RegisterNoSuchService()
	if (!sr->RR_SRV.resrec.rdata->u.srv.port.NotAnInteger) return(mDNS_DeregisterNoSuchService(m, &sr->RR_SRV));

#ifndef UNICAST_DISABLED	
	if (!(sr->RR_SRV.resrec.InterfaceID == mDNSInterface_LocalOnly || IsLocalDomain(sr->RR_SRV.resrec.name)))
		{
		mStatus status;
		mDNS_Lock(m);
		status = uDNS_DeregisterService(m, sr);
		mDNS_Unlock(m);
		return(status);
		}
#endif
	if (sr->RR_PTR.resrec.RecordType == kDNSRecordTypeUnregistered)
		{
		debugf("Service set for %##s already deregistered", sr->RR_SRV.resrec.name->c);
		return(mStatus_BadReferenceErr);
		}
	else if (sr->RR_PTR.resrec.RecordType == kDNSRecordTypeDeregistering)
		{
		debugf("Service set for %##s already in the process of deregistering", sr->RR_SRV.resrec.name->c);
		return(mStatus_NoError);
		}
	else
		{
		mDNSu32 i;
		mStatus status;
		ExtraResourceRecord *e;
		mDNS_Lock(m);
		e = sr->Extras;
	
		// We use mDNS_Dereg_repeat because, in the event of a collision, some or all of the
		// SRV, TXT, or Extra records could have already been automatically deregistered, and that's okay
		mDNS_Deregister_internal(m, &sr->RR_SRV, mDNS_Dereg_repeat);
		mDNS_Deregister_internal(m, &sr->RR_TXT, mDNS_Dereg_repeat);
		
		mDNS_Deregister_internal(m, &sr->RR_ADV, mDNS_Dereg_normal);
	
		// We deregister all of the extra records, but we leave the sr->Extras list intact
		// in case the client wants to do a RenameAndReregister and reinstate the registration
		while (e)
			{
			mDNS_Deregister_internal(m, &e->r, mDNS_Dereg_repeat);
			e = e->next;
			}

		for (i=0; i<sr->NumSubTypes; i++)
			mDNS_Deregister_internal(m, &sr->SubTypes[i], mDNS_Dereg_normal);

		// Be sure to deregister the PTR last!
		// Deregistering this record is what triggers the mStatus_MemFree callback to ServiceCallback,
		// which in turn passes on the mStatus_MemFree (or mStatus_NameConflict) back to the client callback,
		// which is then at liberty to free the ServiceRecordSet memory at will. We need to make sure
		// we've deregistered all our records and done any other necessary cleanup before that happens.
		status = mDNS_Deregister_internal(m, &sr->RR_PTR, mDNS_Dereg_normal);
		mDNS_Unlock(m);
		return(status);
		}
	}

// Create a registration that asserts that no such service exists with this name.
// This can be useful where there is a given function is available through several protocols.
// For example, a printer called "Stuart's Printer" may implement printing via the "pdl-datastream" and "IPP"
// protocols, but not via "LPR". In this case it would be prudent for the printer to assert the non-existence of an
// "LPR" service called "Stuart's Printer". Without this precaution, another printer than offers only "LPR" printing
// could inadvertently advertise its service under the same name "Stuart's Printer", which might be confusing for users.
mDNSexport mStatus mDNS_RegisterNoSuchService(mDNS *const m, AuthRecord *const rr,
	const domainlabel *const name, const domainname *const type, const domainname *const domain,
	const domainname *const host,
	const mDNSInterfaceID InterfaceID, mDNSRecordCallback Callback, void *Context)
	{
	mDNS_SetupResourceRecord(rr, mDNSNULL, InterfaceID, kDNSType_SRV, kHostNameTTL, kDNSRecordTypeUnique, Callback, Context);
	if (ConstructServiceName(rr->resrec.name, name, type, domain) == mDNSNULL) return(mStatus_BadParamErr);
	rr->resrec.rdata->u.srv.priority    = 0;
	rr->resrec.rdata->u.srv.weight      = 0;
	rr->resrec.rdata->u.srv.port        = zeroIPPort;
	if (host && host->c[0]) AssignDomainName(&rr->resrec.rdata->u.srv.target, host);
	else rr->HostTarget = mDNStrue;
	return(mDNS_Register(m, rr));
	}

mDNSexport mStatus mDNS_AdvertiseDomains(mDNS *const m, AuthRecord *rr,
	mDNS_DomainType DomainType, const mDNSInterfaceID InterfaceID, char *domname)
	{
	mDNS_SetupResourceRecord(rr, mDNSNULL, InterfaceID, kDNSType_PTR, kStandardTTL, kDNSRecordTypeShared, mDNSNULL, mDNSNULL);
	if (!MakeDomainNameFromDNSNameString(rr->resrec.name, mDNS_DomainTypeNames[DomainType])) return(mStatus_BadParamErr);
	if (!MakeDomainNameFromDNSNameString(&rr->resrec.rdata->u.name, domname))                 return(mStatus_BadParamErr);
	return(mDNS_Register(m, rr));
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark -
#pragma mark - Startup and Shutdown
#endif

mDNSlocal void mDNS_GrowCache_internal(mDNS *const m, CacheEntity *storage, mDNSu32 numrecords)
	{
	if (storage && numrecords)
		{
		mDNSu32 i;
		debugf("Adding cache storage for %d more records (%d bytes)", numrecords, numrecords*sizeof(CacheEntity));
		for (i=0; i<numrecords; i++) storage[i].next = &storage[i+1];
		storage[numrecords-1].next = m->rrcache_free;
		m->rrcache_free = storage;
		m->rrcache_size += numrecords;
		}
	}

mDNSexport void mDNS_GrowCache(mDNS *const m, CacheEntity *storage, mDNSu32 numrecords)
	{
	mDNS_Lock(m);
	mDNS_GrowCache_internal(m, storage, numrecords);
	mDNS_Unlock(m);
	}

mDNSexport mStatus mDNS_Init(mDNS *const m, mDNS_PlatformSupport *const p,
	CacheEntity *rrcachestorage, mDNSu32 rrcachesize,
	mDNSBool AdvertiseLocalAddresses, mDNSCallback *Callback, void *Context)
	{
	mDNSu32 slot;
	mDNSs32 timenow;
	mStatus result;
	
	if (!rrcachestorage) rrcachesize = 0;
	
	m->p                       = p;
	m->KnownBugs               = 0;
	m->CanReceiveUnicastOn5353 = mDNSfalse; // Assume we can't receive unicasts on 5353, unless platform layer tells us otherwise
	m->AdvertiseLocalAddresses = AdvertiseLocalAddresses;
	m->mDNSPlatformStatus      = mStatus_Waiting;
	m->UnicastPort4            = zeroIPPort;
	m->UnicastPort6            = zeroIPPort;
	m->MainCallback            = Callback;
	m->MainContext             = Context;
	m->rec.r.resrec.RecordType = 0;

	// For debugging: To catch and report locking failures
	m->mDNS_busy               = 0;
	m->mDNS_reentrancy         = 0;
	m->mDNS_shutdown           = mDNSfalse;
	m->lock_rrcache            = 0;
	m->lock_Questions          = 0;
	m->lock_Records            = 0;

	// Task Scheduling variables
	result = mDNSPlatformTimeInit();
	if (result != mStatus_NoError) return(result);
	m->timenow_adjust = (mDNSs32)mDNSRandom(0xFFFFFFFF);
	timenow = mDNS_TimeNow_NoLock(m);

	m->timenow                 = 0;		// MUST only be set within mDNS_Lock/mDNS_Unlock section
	m->timenow_last            = timenow;
	m->NextScheduledEvent      = timenow;
	m->SuppressSending         = timenow;
	m->NextCacheCheck          = timenow + 0x78000000;
	m->NextScheduledQuery      = timenow + 0x78000000;
	m->NextScheduledProbe      = timenow + 0x78000000;
	m->NextScheduledResponse   = timenow + 0x78000000;
	m->ExpectUnicastResponse   = timenow + 0x78000000;
	m->RandomQueryDelay        = 0;
	m->RandomReconfirmDelay    = 0;
	m->PktNum                  = 0;
	m->SendDeregistrations     = mDNSfalse;
	m->SendImmediateAnswers    = mDNSfalse;
	m->SleepState              = mDNSfalse;

	// These fields only required for mDNS Searcher...
	m->Questions               = mDNSNULL;
	m->NewQuestions            = mDNSNULL;
	m->CurrentQuestion         = mDNSNULL;
	m->LocalOnlyQuestions      = mDNSNULL;
	m->NewLocalOnlyQuestions   = mDNSNULL;
	m->rrcache_size            = 0;
	m->rrcache_totalused       = 0;
	m->rrcache_active          = 0;
	m->rrcache_report          = 10;
	m->rrcache_free            = mDNSNULL;

	for (slot = 0; slot < CACHE_HASH_SLOTS; slot++) m->rrcache_hash[slot] = mDNSNULL;

	mDNS_GrowCache_internal(m, rrcachestorage, rrcachesize);

	// Fields below only required for mDNS Responder...
	m->hostlabel.c[0]          = 0;
	m->nicelabel.c[0]          = 0;
	m->MulticastHostname.c[0]  = 0;
	m->HIHardware.c[0]         = 0;
	m->HISoftware.c[0]         = 0;
	m->ResourceRecords         = mDNSNULL;
	m->DuplicateRecords        = mDNSNULL;
	m->NewLocalRecords         = mDNSNULL;
	m->CurrentRecord           = mDNSNULL;
	m->HostInterfaces          = mDNSNULL;
	m->ProbeFailTime           = 0;
	m->NumFailedProbes         = 0;
	m->SuppressProbes          = 0;

#ifndef UNICAST_DISABLED	
	uDNS_Init(m);
	m->SuppressStdPort53Queries = 0;
#endif
	result = mDNSPlatformInit(m);

	return(result);
	}

mDNSexport void mDNSCoreInitComplete(mDNS *const m, mStatus result)
	{
	m->mDNSPlatformStatus = result;
	if (m->MainCallback)
		{
		mDNS_Lock(m);
		m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
		m->MainCallback(m, mStatus_NoError);
		m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
		mDNS_Unlock(m);
		}
	}

mDNSexport void mDNS_Close(mDNS *const m)
	{
	mDNSu32 rrcache_active = 0;
	mDNSu32 rrcache_totalused = 0;
	mDNSu32 slot;
	NetworkInterfaceInfo *intf;
	mDNS_Lock(m);
	
	m->mDNS_shutdown = mDNStrue;

#ifndef UNICAST_DISABLED	
	uDNS_Close(m);
#endif
	rrcache_totalused = m->rrcache_totalused;
	for (slot = 0; slot < CACHE_HASH_SLOTS; slot++)
		{
		while(m->rrcache_hash[slot])
			{
			CacheGroup *cg = m->rrcache_hash[slot];
			while (cg->members)
				{
				CacheRecord *rr = cg->members;
				cg->members = cg->members->next;
				if (rr->CRActiveQuestion) rrcache_active++;
				ReleaseCacheRecord(m, rr);
				}
			cg->rrcache_tail = &cg->members;
			ReleaseCacheGroup(m, &m->rrcache_hash[slot]);
			}
		}
	debugf("mDNS_Close: RR Cache was using %ld records, %lu active", rrcache_totalused, rrcache_active);
	if (rrcache_active != m->rrcache_active)
		LogMsg("*** ERROR *** rrcache_active %lu != m->rrcache_active %lu", rrcache_active, m->rrcache_active);

	for (intf = m->HostInterfaces; intf; intf = intf->next)
		if (intf->Advertise)
			DeadvertiseInterface(m, intf);

	// Make sure there are nothing but deregistering records remaining in the list
	if (m->CurrentRecord) LogMsg("mDNS_Close ERROR m->CurrentRecord already set");
	m->CurrentRecord = m->ResourceRecords;
	while (m->CurrentRecord)
		{
		AuthRecord *rr = m->CurrentRecord;
		if (rr->resrec.RecordType != kDNSRecordTypeDeregistering)
			{
			debugf("mDNS_Close: Record type %X still in ResourceRecords list %##s", rr->resrec.RecordType, rr->resrec.name->c);
			mDNS_Deregister_internal(m, rr, mDNS_Dereg_normal);
			}
		else
			m->CurrentRecord = rr->next;
		}

	if (m->ResourceRecords) debugf("mDNS_Close: Sending final packets for deregistering records");
	else debugf("mDNS_Close: No deregistering records remain");

	// If any deregistering records remain, send their deregistration announcements before we exit
	if (m->mDNSPlatformStatus != mStatus_NoError) DiscardDeregistrations(m);
	else if (m->ResourceRecords) SendResponses(m);
	if (m->ResourceRecords) LogMsg("mDNS_Close failed to send goodbye for: %s", ARDisplayString(m, m->ResourceRecords));
	
	mDNS_Unlock(m);
	debugf("mDNS_Close: mDNSPlatformClose");
	mDNSPlatformClose(m);
	debugf("mDNS_Close: done");
	}
