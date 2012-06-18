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

 * To Do:
 * Elimate all mDNSPlatformMemAllocate/mDNSPlatformMemFree from this code -- the core code
 * is supposed to be malloc-free so that it runs in constant memory determined at compile-time.
 * Any dynamic run-time requirements should be handled by the platform layer below or client layer above

	Change History (most recent first):

$Log: uDNS.c,v $
Revision 1.617  2009/06/30 20:51:02  cheshire
Improved "Error! Tried to add a NAT traversal that's already in the active list" debugging message

Revision 1.616  2009/05/27 20:29:36  cheshire
<rdar://problem/6926465> Sleep is delayed by 10 seconds if BTMM is on
After receiving confirmation of LLQ deletion, need to schedule another evaluation of whether we're ready to sleep yet

Revision 1.615  2009/05/05 01:32:50  jessic2
<rdar://problem/6830541> regservice_callback: instance->request is NULL 0 -- Clean up spurious logs resulting from fixing this bug.

Revision 1.614  2009/04/24 02:17:57  mcguire
<rdar://problem/5264124> uDNS: Not always respecting preference order of DNS servers

Revision 1.613  2009/04/23 22:06:29  cheshire
Added CacheRecord and InterfaceID parameters to MakeNegativeCacheRecord, in preparation for:
<rdar://problem/3476350> Return negative answers when host knows authoritatively that no answer exists

Revision 1.612  2009/04/22 01:19:57  jessic2
<rdar://problem/6814585> Daemon: mDNSResponder is logging garbage for error codes because it's using %ld for int 32

Revision 1.611  2009/04/15 20:42:51  mcguire
<rdar://problem/6768947> uDNS: Treat RCODE 5 (Refused) responses as failures

Revision 1.610  2009/04/15 01:10:39  jessic2
<rdar://problem/6466541> BTMM: Add support for setting kDNSServiceErr_NoSuchRecord in DynamicStore

Revision 1.609  2009/04/11 00:19:45  jessic2
<rdar://problem/4426780> Daemon: Should be able to turn on LogOperation dynamically

Revision 1.608  2009/04/06 23:44:59  cheshire
<rdar://problem/6757838> mDNSResponder thrashing kernel lock in the UDP close path, hurting SPECweb performance

Revision 1.607  2009/04/02 22:36:34  jessic2
Fix crash when calling debugf with null opt

Revision 1.606  2009/03/26 03:59:00  jessic2
Changes for <rdar://problem/6492552&6492593&6492609&6492613&6492628&6492640&6492699>

Revision 1.605  2009/03/04 00:40:14  cheshire
Updated DNS server error codes to be more consistent with definitions at
<http://www.iana.org/assignments/dns-parameters>

Revision 1.604  2009/02/27 03:08:47  cheshire
<rdar://problem/6547720> Crash while shutting down when "local" is in the user's DNS searchlist

Revision 1.603  2009/02/27 02:56:57  cheshire
Moved struct SearchListElem definition from uDNS.c into mDNSEmbeddedAPI.h

Revision 1.602  2009/02/13 06:29:54  cheshire
Converted LogOperation messages to LogInfo

Revision 1.601  2009/02/12 20:57:25  cheshire
Renamed 'LogAllOperation' switch to 'LogClientOperations'; added new 'LogSleepProxyActions' switch

Revision 1.600  2009/01/31 21:05:12  cheshire
Improved "Failed to obtain NAT port mapping" debugging log message

Revision 1.599  2009/01/23 00:38:36  mcguire
<rdar://problem/5570906> BTMM: Doesn't work with Linksys WRT54GS firmware 4.71.1

Revision 1.598  2009/01/21 03:43:57  mcguire
<rdar://problem/6511765> BTMM: Add support for setting kDNSServiceErr_NATPortMappingDisabled in DynamicStore

Revision 1.597  2009/01/10 01:55:49  cheshire
Added LogOperation message showing when domains are added and removed in FoundDomain

Revision 1.596  2008/12/19 20:23:33  mcguire
<rdar://problem/6459269> Lots of duplicate log messages about failure to bind to NAT-PMP Announcement port

Revision 1.595  2008/12/18 23:32:19  mcguire
<rdar://problem/6019470> BTMM: Include the question in the LLQ notification acknowledgment

Revision 1.594  2008/12/10 02:25:31  cheshire
Minor fixes to use of LogClientOperations symbol

Revision 1.593  2008/12/10 02:11:42  cheshire
ARMv5 compiler doesn't like uncommented stuff after #endif

Revision 1.592  2008/12/06 01:42:55  mcguire
<rdar://problem/6418958> Need to exponentially back-off after failure to get public address

Revision 1.591  2008/12/06 00:17:11  cheshire
<rdar://problem/6380477> mDNS_StopNATOperation doesn't handle duplicate NAT mapping requests properly
Refinement: For duplicate ssh mappings we want to suppress the syslog warning message, but not the "unmap = mDNSfalse"

Revision 1.590  2008/12/04 20:57:36  mcguire
fix build

Revision 1.589  2008/12/04 02:24:09  cheshire
Improved NAT-PMP debugging messages

Revision 1.588  2008/11/26 20:38:08  cheshire
Changed some "LogOperation" debugging messages to "debugf"

Revision 1.587  2008/11/26 19:53:26  cheshire
Don't overwrite srs->NATinfo.IntPort in StartSRVNatMap()

Revision 1.586  2008/11/25 23:43:07  cheshire
<rdar://problem/5745355> Crashes at ServiceRegistrationGotZoneData + 397
Made code more defensive to guard against ServiceRegistrationGotZoneData being called with invalid ServiceRecordSet object

Revision 1.585  2008/11/25 22:46:30  cheshire
For ease of code searching, renamed ZoneData field of ServiceRecordSet_struct from "nta" to "srs_nta"

Revision 1.584  2008/11/24 19:46:40  cheshire
When sending query over TCP, don't include LLQ option when we're talking to a conventional DNS server or cache

Revision 1.583  2008/11/21 00:34:58  cheshire
<rdar://problem/6380477> mDNS_StopNATOperation doesn't handle duplicate NAT mapping requests properly

Revision 1.582  2008/11/20 02:23:37  mcguire
<rdar://problem/6041208> need to handle URLBase

Revision 1.581  2008/11/20 01:51:19  cheshire
Exported RecreateNATMappings so it's callable from other files

Revision 1.580  2008/11/13 19:08:45  cheshire
Fixed code to handle rdataOPT properly

Revision 1.579  2008/11/07 00:18:01  mcguire
<rdar://problem/6351068> uDNS: Supress reverse DNS query until required

Revision 1.578  2008/11/04 22:21:46  cheshire
Changed zone field of AuthRecord_struct from domainname to pointer, saving 252 bytes per AuthRecord

Revision 1.577  2008/10/29 21:37:01  cheshire
Removed some old debugging messages

Revision 1.576  2008/10/23 22:25:57  cheshire
Renamed field "id" to more descriptive "updateid"

Revision 1.575  2008/10/20 02:07:49  mkrochma
<rdar://problem/6296804> Remove Note: DNS Server <ip> for domain <d> registered more than once

Revision 1.574  2008/10/14 19:06:45  cheshire
In uDNS_ReceiveMsg(), only do checkUpdateResult() for uDNS records

Revision 1.573  2008/09/25 20:43:44  cheshire
<rdar://problem/6245044> Stop using separate m->ServiceRegistrations list
In UpdateSRVRecords, call mDNS_SetFQDN(m) to update AutoTarget SRV records on the main m->ResourceRecords list

Revision 1.572  2008/09/24 23:48:05  cheshire
Don't need to pass whole ServiceRecordSet reference to GetServiceTarget;
it only needs to access the embedded SRV member of the set

Revision 1.571  2008/09/23 22:56:53  cheshire
<rdar://problem/5298845> Remove dnsbugtest query

Revision 1.570  2008/09/23 01:30:18  cheshire
The putLLQ() routine was not setting the OPT record's rrclass to NormalMaxDNSMessageData

Revision 1.569  2008/07/25 22:34:11  mcguire
fix sizecheck issues for 64bit

Revision 1.568  2008/07/24 20:23:03  cheshire
<rdar://problem/3988320> Should use randomized source ports and transaction IDs to avoid DNS cache poisoning

Revision 1.567  2008/07/01 01:40:00  mcguire
<rdar://problem/5823010> 64-bit fixes

Revision 1.566  2008/06/26 17:24:11  mkrochma
<rdar://problem/5450912> BTMM: Stop listening on UDP 5351 for NAT Status Announcements

Revision 1.565  2008/06/21 19:06:58  mcguire
<rdar://problem/4206534> Use all configured DNS servers

Revision 1.564  2008/06/19 23:42:03  mcguire
<rdar://problem/4206534> Use all configured DNS servers

Revision 1.563  2008/06/19 17:46:14  mcguire
<rdar://problem/4206534> Use all configured DNS servers
Don't do extra work for log messages if we're not going to log

Revision 1.562  2008/06/19 17:35:19  mcguire
<rdar://problem/4206534> Use all configured DNS servers
cleanup log messages
check for null pointers

Revision 1.561  2008/06/19 01:20:49  mcguire
<rdar://problem/4206534> Use all configured DNS servers

Revision 1.560  2008/05/31 01:51:09  mcguire
fixed typo in log message

Revision 1.559  2008/04/15 22:37:58  mkrochma
Change LogMsg to LogOperation

Revision 1.558  2008/03/17 18:02:35  mkrochma
Add space to log message for consistency

Revision 1.557  2008/03/14 19:58:38  mcguire
<rdar://problem/5500969> BTMM: Need ability to identify version of mDNSResponder client
Make sure we add the record when sending LLQ refreshes

Revision 1.556  2008/03/07 23:55:05  cheshire
<rdar://problem/5787898> LLQ refresh randomization not working properly

Revision 1.555  2008/03/07 23:25:56  cheshire
Improved debugging messages

Revision 1.554  2008/03/07 18:56:03  cheshire
<rdar://problem/5777647> dnsbugtest query every three seconds when source IP address of response doesn't match

Revision 1.553  2008/03/06 02:48:34  mcguire
<rdar://problem/5321824> write status to the DS

Revision 1.552  2008/03/05 01:56:42  cheshire
<rdar://problem/5687667> BTMM: Don't fallback to unencrypted operations when SRV lookup fails

Revision 1.551  2008/03/01 01:43:04  cheshire
<rdar://problem/5631565> BTMM: Lots of "Error getting external address 3" when double-NATed prevents sleep
Added code to suppress logging of multiple identical error results

Revision 1.550  2008/03/01 01:34:47  cheshire
<rdar://problem/5736313> BTMM: Double-NAT'd machines register all but AutoTunnel v4 address records
Further refinements

Revision 1.549  2008/02/29 01:35:37  mcguire
<rdar://problem/5736313> BTMM: Double-NAT'd machines register all but AutoTunnel v4 address records

Revision 1.548  2008/02/20 23:54:18  cheshire
<rdar://problem/5661518> "Failed to obtain NAT port mapping" syslog messages
Improved log message so it tells us more about what's going on

Revision 1.547  2008/02/20 00:41:09  cheshire
Change "PrivateQueryGotZoneData ... invoked with error code" from LogMsg to LogOperation

Revision 1.546  2008/02/19 23:26:50  cheshire
<rdar://problem/5661661> BTMM: Too many members.mac.com SOA queries

Revision 1.545  2007/12/22 02:25:29  cheshire
<rdar://problem/5661128> Records and Services sometimes not re-registering on wake from sleep

Revision 1.544  2007/12/18 00:40:11  cheshire
<rdar://problem/5526796> Need to remove active LLQs from server upon question cancellation, on sleep, and on shutdown
Reordered code to avoid double-TSIGs in some cases

Revision 1.543  2007/12/17 23:57:43  cheshire
<rdar://problem/5526796> Need to remove active LLQs from server upon question cancellation, on sleep, and on shutdown
Need to include TSIG signature when sending LLQ cancellations over TLS

Revision 1.542  2007/12/15 01:12:27  cheshire
<rdar://problem/5526796> Need to remove active LLQs from server upon question cancellation, on sleep, and on shutdown

Revision 1.541  2007/12/15 00:18:51  cheshire
Renamed question->origLease to question->ReqLease

Revision 1.540  2007/12/14 23:55:28  cheshire
Moved "struct tcpInfo_t" definition from uDNS.c to mDNSEmbeddedAPI.h

Revision 1.539  2007/12/14 20:44:24  cheshire
<rdar://problem/5526800> BTMM: Need to deregister records and services on shutdown/sleep
SleepRecordRegistrations/WakeRecordRegistrations should only operate on uDNS records

Revision 1.538  2007/12/14 01:13:40  cheshire
<rdar://problem/5526800> BTMM: Need to deregister records and services on shutdown/sleep
Additional fixes (existing code to deregister private records and services didn't work at all)

Revision 1.537  2007/12/11 00:18:25  cheshire
<rdar://problem/5569316> BTMM: My iMac has a "ghost" ID associated with it
There were cases where the code was incorrectly clearing the "uselease" flag, and never resetting it.

Revision 1.536  2007/12/10 23:07:00  cheshire
Removed some unnecessary log messages

Revision 1.535  2007/12/06 00:22:27  mcguire
<rdar://problem/5604567> BTMM: Doesn't work with Linksys WAG300N 1.01.06 (sending from 1026/udp)

Revision 1.534  2007/12/04 00:49:37  cheshire
<rdar://problem/5607082> BTMM: mDNSResponder taking 100 percent CPU after upgrading to 10.5.1

Revision 1.533  2007/12/01 01:21:27  jgraessley
<rdar://problem/5623140> mDNSResponder unicast DNS improvements

Revision 1.532  2007/11/30 20:16:44  cheshire
Fixed compile warning: declaration of 'end' shadows a previous local

Revision 1.531  2007/11/28 22:00:09  cheshire
In StartSRVNatMap, change "mDNSu8 *p" to "const mDNSu8 *p"

Revision 1.530  2007/11/16 22:19:40  cheshire
<rdar://problem/5547474> mDNSResponder leaks on network changes
The "connection failed" code path in MakeTCPConn was not disposing of the TCPSocket it had created

Revision 1.529  2007/11/15 22:52:29  cheshire
<rdar://problem/5589039> ERROR: mDNSPlatformWriteTCP - send Broken pipe

Revision 1.528  2007/11/02 21:32:30  cheshire
<rdar://problem/5575593> BTMM: Deferring deregistration of record log messages on sleep/wake

Revision 1.527  2007/11/01 16:08:51  cheshire
Tidy up alignment of "SetRecordRetry refresh" log messages

Revision 1.526  2007/10/31 19:26:55  cheshire
Don't need to log "Permanently abandoning service registration" message when we're intentionally deleting a service

Revision 1.525  2007/10/30 23:58:59  cheshire
<rdar://problem/5496734> BTMM: Need to retry registrations after failures
After failure, double retry interval up to maximum of 30 minutes

Revision 1.524  2007/10/30 20:10:47  cheshire
<rdar://problem/5496734> BTMM: Need to retry registrations after failures

Revision 1.523  2007/10/30 00:54:31  cheshire
<rdar://problem/5496734> BTMM: Need to retry registrations after failures
Fixed timing logic to double retry interval properly

Revision 1.522  2007/10/30 00:04:43  cheshire
<rdar://problem/5496734> BTMM: Need to retry registrations after failures
Made the code not give up and abandon the record when it gets an error in regState_UpdatePending state

Revision 1.521  2007/10/29 23:58:52  cheshire
<rdar://problem/5536979> BTMM: Need to create NAT port mapping for receiving LLQ events
Use standard "if (mDNSIPv4AddressIsOnes(....ExternalAddress))" mechanism to determine whether callback has been invoked yet

Revision 1.520  2007/10/29 21:48:36  cheshire
<rdar://problem/5519458> BTMM: Machines don't appear in the sidebar on wake from sleep
Added 10% random variation on LLQ renewal time, to reduce unintended timing correlation between multiple machines

Revision 1.519  2007/10/29 21:37:00  cheshire
<rdar://problem/5496734> BTMM: Need to retry registrations after failures
Added 10% random variation on record refresh time, to reduce accidental timing correlation between multiple machines

Revision 1.518  2007/10/26 23:41:29  cheshire
<rdar://problem/5496734> BTMM: Need to retry registrations after failures

Revision 1.517  2007/10/25 23:30:12  cheshire
Private DNS registered records now deregistered on sleep and re-registered on wake

Revision 1.516  2007/10/25 22:53:52  cheshire
<rdar://problem/5496734> BTMM: Need to retry registrations after failures
Don't unlinkSRS and permanently give up at the first sign of trouble

Revision 1.515  2007/10/25 21:08:07  cheshire
Don't try to send record registrations/deletions before we have our server address

Revision 1.514  2007/10/25 20:48:47  cheshire
For naming consistency (with AuthRecord's UpdateServer) renamed 'ns' to 'SRSUpdateServer'

Revision 1.513  2007/10/25 20:06:13  cheshire
Don't try to do SOA queries using private DNS (TLS over TCP) queries

Revision 1.512  2007/10/25 18:25:15  cheshire
<rdar://problem/5496734> BTMM: Need to retry registrations after failures
Don't need a NAT mapping for autotunnel services

Revision 1.511  2007/10/25 00:16:23  cheshire
<rdar://problem/5496734> BTMM: Need to retry registrations after failures
Fixed retry timing logic; when DNS server returns an error code, we should retry later,
instead of just deleting our record ("UnlinkAuthRecord") and completely giving up

Revision 1.510  2007/10/24 22:40:06  cheshire
Renamed: RecordRegistrationCallback          -> RecordRegistrationGotZoneData
Renamed: ServiceRegistrationZoneDataComplete -> ServiceRegistrationGotZoneData

Revision 1.509  2007/10/24 00:54:07  cheshire
<rdar://problem/5496734> BTMM: Need to retry registrations after failures

Revision 1.508  2007/10/24 00:05:03  cheshire
<rdar://problem/5519458> BTMM: Machines don't appear in the sidebar on wake from sleep
When sending TLS/TCP LLQ setup request over VPN, need to set EventPort to 5353, not zero

Revision 1.507  2007/10/23 00:33:36  cheshire
Improved debugging messages

Revision 1.506  2007/10/22 19:54:13  cheshire
<rdar://problem/5519458> BTMM: Machines don't appear in the sidebar on wake from sleep
Only put EventPort in LLQ request when sending from an RFC 1918 source address, not when sending over VPN

Revision 1.505  2007/10/19 22:08:49  cheshire
<rdar://problem/5519458> BTMM: Machines don't appear in the sidebar on wake from sleep
Additional fixes and refinements

Revision 1.504  2007/10/18 23:06:43  cheshire
<rdar://problem/5519458> BTMM: Machines don't appear in the sidebar on wake from sleep
Additional fixes and refinements

Revision 1.503  2007/10/18 20:23:17  cheshire
Moved SuspendLLQs into mDNS.c, since it's only called from one place

Revision 1.502  2007/10/17 22:49:54  cheshire
<rdar://problem/5519458> BTMM: Machines don't appear in the sidebar on wake from sleep

Revision 1.501  2007/10/17 22:37:23  cheshire
<rdar://problem/5536979> BTMM: Need to create NAT port mapping for receiving LLQ events

Revision 1.500  2007/10/17 21:53:51  cheshire
Improved debugging messages; renamed startLLQHandshakeCallback to LLQGotZoneData

Revision 1.499  2007/10/16 21:16:50  cheshire
Get rid of unused uDNS_Sleep() routine

Revision 1.498  2007/10/16 20:59:41  cheshire
Export SuspendLLQs/SleepServiceRegistrations/SleepRecordRegistrations so they're callable from other files

Revision 1.497  2007/10/05 18:09:44  cheshire
<rdar://problem/5524841> Services advertised with wrong target host

Revision 1.496  2007/10/04 22:38:59  cheshire
Added LogOperation message showing new q->ThisQInterval after sending uDNS query packet

Revision 1.495  2007/10/03 00:16:19  cheshire
In PrivateQueryGotZoneData, need to grab lock before calling SetNextQueryTime

Revision 1.494  2007/10/02 21:11:08  cheshire
<rdar://problem/5518270> LLQ refreshes don't work, which breaks BTMM browsing

Revision 1.493  2007/10/02 19:50:23  cheshire
Improved debugging message

Revision 1.492  2007/09/29 03:15:43  cheshire
<rdar://problem/5513168> BTMM: mDNSResponder memory corruption in GetAuthInfoForName_internal
Use AutoTunnelUnregistered macro instead of checking record state directly

Revision 1.491  2007/09/29 01:33:45  cheshire
<rdar://problem/5513168> BTMM: mDNSResponder memory corruption in GetAuthInfoForName_internal

Revision 1.490  2007/09/29 01:06:17  mcguire
<rdar://problem/5507862> 9A564: mDNSResponder crash in mDNS_Execute

Revision 1.489  2007/09/27 22:02:33  cheshire
<rdar://problem/5464941> BTMM: Registered records in BTMM don't get removed from server after calling RemoveRecord

Revision 1.488  2007/09/27 21:20:17  cheshire
Improved debugging syslog messages

Revision 1.487  2007/09/27 18:55:11  cheshire
<rdar://problem/5477165> BTMM: Multiple SRV records get registered after changing Computer Name

Revision 1.486  2007/09/27 17:42:49  cheshire
Fix naming: for consistency, "kDNSFlag1_RC" should be "kDNSFlag1_RC_Mask"

Revision 1.485  2007/09/27 02:16:30  cheshire
<rdar://problem/5500111> BTMM: LLQ refreshes being sent in the clear to the wrong port

Revision 1.484  2007/09/27 00:25:39  cheshire
Added ttl_seconds parameter to MakeNegativeCacheRecord in preparation for:
<rdar://problem/4947392> uDNS: Use SOA to determine TTL for negative answers

Revision 1.483  2007/09/26 23:16:58  cheshire
<rdar://problem/5496399> BTMM: Leopard sending excessive LLQ registration requests to .Mac

Revision 1.482  2007/09/26 22:06:02  cheshire
<rdar://problem/5507399> BTMM: No immediate failure notifications for BTMM names

Revision 1.481  2007/09/26 00:49:46  cheshire
Improve packet logging to show sent and received packets,
transport protocol (UDP/TCP/TLS) and source/destination address:port

Revision 1.480  2007/09/21 21:08:52  cheshire
Get rid of unnecessary DumpPacket() calls -- it makes more sense
to do this in mDNSSendDNSMessage and mDNSCoreReceive instead

Revision 1.479  2007/09/21 20:01:17  cheshire
<rdar://problem/5496750> BTMM: Skip directly to member name in SOA queries to avoid sending names in the clear

Revision 1.478  2007/09/21 19:29:14  cheshire
Added dump of uDNS questions when in MDNS_LOG_VERBOSE_DEBUG mode

Revision 1.477  2007/09/20 02:29:37  cheshire
<rdar://problem/4038277> BTMM: Not getting LLQ remove events when logging out of VPN or disconnecting from network

Revision 1.476  2007/09/20 01:19:49  cheshire
Improve debugging messages: report startLLQHandshake errors; show state in uDNS_StopLongLivedQuery message

Revision 1.475  2007/09/19 23:51:26  cheshire
<rdar://problem/5480517> BTMM: Need to log a message when NAT port mapping fails

Revision 1.474  2007/09/19 20:32:09  cheshire
Export GetAuthInfoForName so it's callable from other files

Revision 1.473  2007/09/18 21:42:29  cheshire
To reduce programming mistakes, renamed ExtPort to RequestedPort

Revision 1.472  2007/09/14 21:26:08  cheshire
<rdar://problem/5482627> BTMM: Need to manually avoid port conflicts when using UPnP gateways

Revision 1.471  2007/09/14 01:07:10  cheshire
If UPnP NAT gateway returns 0.0.0.0 as external address (e.g. because it hasn't
got a DHCP address yet) then retry periodically until it gives us a real address.

Revision 1.470  2007/09/13 00:36:26  cheshire
<rdar://problem/5477360> NAT Reboot detection logic incorrect

Revision 1.469  2007/09/13 00:28:50  cheshire
<rdar://problem/5477354> Host records not updated on NAT address change

Revision 1.468  2007/09/13 00:16:41  cheshire
<rdar://problem/5468706> Miscellaneous NAT Traversal improvements

Revision 1.467  2007/09/12 23:03:08  cheshire
<rdar://problem/5476978> DNSServiceNATPortMappingCreate callback not giving correct interface index

Revision 1.466  2007/09/12 22:19:29  cheshire
<rdar://problem/5476977> Need to listen for port 5350 NAT-PMP announcements

Revision 1.465  2007/09/12 19:22:19  cheshire
Variable renaming in preparation for upcoming fixes e.g. priv/pub renamed to intport/extport
Made NAT Traversal packet handlers take typed data instead of anonymous "mDNSu8 *" byte pointers

Revision 1.464  2007/09/12 01:22:13  cheshire
Improve validatelists() checking to detect when 'next' pointer gets smashed to ~0

Revision 1.463  2007/09/11 20:23:28  vazquez
<rdar://problem/5466719> CrashTracer: 3 crashes in mDNSResponder at mDNSResponder: natTraversalHandlePortMapReply + 107
Make sure we clean up NATTraversals before free'ing HostnameInfo

Revision 1.462  2007/09/11 19:19:16  cheshire
Correct capitalization of "uPNP" to "UPnP"

Revision 1.461  2007/09/10 22:08:17  cheshire
Rename uptime => upseconds and LastNATUptime => LastNATupseconds to make it clear these time values are in seconds

Revision 1.460  2007/09/07 21:47:43  vazquez
<rdar://problem/5460210> BTMM: SetupSocket 5351 failed; Can't allocate UDP multicast socket spew on wake from sleep with internet sharing on
Try to allocate using port 5350 if we get a failure, and only log message if that fails too.

Revision 1.459  2007/09/07 01:01:05  cheshire
<rdar://problem/5464844> BTMM: Services being registered and deregistered in a loop
In hndlServiceUpdateReply, need to clear SRVUpdateDeferred

Revision 1.458  2007/09/06 19:14:33  cheshire
Fixed minor error introduced in 1.379 (an "if" statement was deleted but the "else" following it was left there)

Revision 1.457  2007/09/05 21:48:01  cheshire
<rdar://problem/5385864> BTMM: mDNSResponder flushes wide-area Bonjour records after an hour for a zone.
Now that we're respecting the TTL of uDNS records in the cache, the LLQ maintenance code needs
to update the cache lifetimes of all relevant records every time it successfully renews an LLQ,
otherwise those records will expire and vanish from the cache.

Revision 1.456  2007/09/05 21:00:17  cheshire
<rdar://problem/5457287> mDNSResponder taking up 100% CPU in ReissueBlockedQuestions
Additional refinement: ThisQInterval needs to be restored in tcpCallback, not in PrivateQueryGotZoneData

Revision 1.455  2007/09/05 20:53:06  cheshire
Tidied up alignment of code layout; code was clearing m->tcpAddrInfo.sock instead of m->tcpDeviceInfo.sock

Revision 1.454  2007/09/05 02:32:55  cheshire
Fixed posix build error (mixed declarations and code)

Revision 1.453  2007/09/05 02:26:57  cheshire
<rdar://problem/5457287> mDNSResponder taking up 100% CPU in ReissueBlockedQuestions
In PrivateQueryGotZoneData, restore q->ThisQInterval to non-zero value after GetZoneData completes

Revision 1.452  2007/08/31 22:58:22  cheshire
If we have an existing TCP connection we should re-use it instead of just bailing out
After receiving dnsbugtest response, need to set m->NextScheduledQuery to cause queries to be re-issued

Revision 1.451  2007/08/31 18:49:49  vazquez
<rdar://problem/5393719> BTMM: Need to properly deregister when stopping BTMM

Revision 1.450  2007/08/30 22:50:04  mcguire
<rdar://problem/5430628> BTMM: Tunneled services are registered when autotunnel can't be setup

Revision 1.449  2007/08/30 00:43:17  cheshire
Need to clear m->rec.r.resrec.RecordType before returning from uDNS_recvLLQResponse

Revision 1.448  2007/08/30 00:18:46  cheshire
<rdar://problem/5448804> Error messages: "SendServiceRegistration: Already have TCP connection..."

Revision 1.447  2007/08/29 01:18:33  cheshire
<rdar://problem/5400181> BTMM: Tunneled services do not need NAT port mappings
Only create NAT mappings for SRV records with AutoTarget set to Target_AutoHostAndNATMAP

Revision 1.446  2007/08/28 23:58:42  cheshire
Rename HostTarget -> AutoTarget

Revision 1.445  2007/08/28 23:53:21  cheshire
Rename serviceRegistrationCallback -> ServiceRegistrationZoneDataComplete

Revision 1.444  2007/08/27 20:29:20  cheshire
Additional debugging messages

Revision 1.443  2007/08/24 23:18:28  cheshire
mDNS_SetSecretForDomain is called with lock held; needs to use
GetAuthInfoForName_internal() instead of external version GetAuthInfoForName()

Revision 1.442  2007/08/24 22:43:06  cheshire
Tidied up coded layout

Revision 1.441  2007/08/24 01:20:55  cheshire
<rdar://problem/5434381> BTMM: Memory corruption in KeychainChanged event handling

Revision 1.440  2007/08/24 00:15:20  cheshire
Renamed GetAuthInfoForName() to GetAuthInfoForName_internal() to make it clear that it may only be called with the lock held

Revision 1.439  2007/08/23 21:47:09  vazquez
<rdar://problem/5427316> BTMM: mDNSResponder sends NAT-PMP packets on public network
make sure we clean up port mappings on base stations by sending a lease value of 0,
and only send NAT-PMP packets on private networks; also save some memory by
not using packet structs in NATTraversals.

Revision 1.438  2007/08/22 17:50:08  vazquez
<rdar://problem/5399276> Need to handle errors returned by NAT-PMP routers properly
Propagate router errors to clients, and stop logging spurious "message too short" logs.

Revision 1.437  2007/08/18 00:54:15  mcguire
<rdar://problem/5413147> BTMM: Should not register private addresses or zeros

Revision 1.436  2007/08/08 21:07:48  vazquez
<rdar://problem/5244687> BTMM: Need to advertise model information via wide-area bonjour

Revision 1.435  2007/08/03 02:04:09  vazquez
<rdar://problem/5371843> BTMM: Private LLQs never fall back to polling
Fix case where NAT-PMP returns an external address but does not support
port mappings. Undo previous change and now, if the router returns an
error in the reply packet we respect it.

Revision 1.434  2007/08/02 21:03:05  vazquez
Change NAT logic to fix case where base station with port mapping turned off
returns an external address but does not make port mappings.

Revision 1.433  2007/08/02 03:30:11  vazquez
<rdar://problem/5371843> BTMM: Private LLQs never fall back to polling

Revision 1.432  2007/08/01 18:15:19  cheshire
Fixed crash in tcpCallback; fixed some problems with LLQ setup behind NAT

Revision 1.431  2007/08/01 16:11:06  cheshire
Fixed "mixed declarations and code" compiler error in Posix build

Revision 1.430  2007/08/01 16:09:13  cheshire
Removed unused NATTraversalInfo substructure from AuthRecord; reduced structure sizecheck values accordingly

Revision 1.429  2007/08/01 03:09:22  cheshire
<rdar://problem/5344587> BTMM: Create NAT port mapping for autotunnel port

Revision 1.428  2007/08/01 01:43:36  cheshire
Need to do mDNS_DropLockBeforeCallback/ReclaimLock around invokation of NAT client callback

Revision 1.427  2007/08/01 01:31:13  cheshire
Need to initialize traversal->tcpInfo fields or code may crash

Revision 1.426  2007/08/01 01:15:57  cheshire
<rdar://problem/5375791> Need to invoke NAT client callback when not on RFC1918 private network

Revision 1.425  2007/08/01 00:04:14  cheshire
<rdar://problem/5261696> Crash in tcpKQSocketCallback
Half-open TCP connections were not being cancelled properly

Revision 1.424  2007/07/31 02:28:35  vazquez
<rdar://problem/3734269> NAT-PMP: Detect public IP address changes and base station reboot

Revision 1.423  2007/07/30 23:31:26  cheshire
Code for respecting TTL received in uDNS responses should exclude LLQ-type responses

Revision 1.422  2007/07/28 01:25:57  cheshire
<rdar://problem/4780038> BTMM: Add explicit UDP event port to LLQ setup request, to fix LLQs not working behind NAT

Revision 1.421  2007/07/28 00:04:14  cheshire
Various fixes for comments and debugging messages

Revision 1.420  2007/07/27 23:59:18  cheshire
Added compile-time structure size checks

Revision 1.419  2007/07/27 20:52:29  cheshire
Made uDNS_recvLLQResponse() return tri-state result: LLQ_Not, LLQ_First, or LLQ_Events

Revision 1.418  2007/07/27 20:32:05  vazquez
Flag a UPnP NAT traversal before starting a UPnP port mapping, and make sure all
calls to mDNS_StopNATOperation() go through the UPnP code

Revision 1.417  2007/07/27 20:19:42  cheshire
Use MDNS_LOG_VERBOSE_DEBUG for dumping out packets instead of MDNS_LOG_DEBUG

Revision 1.416  2007/07/27 19:59:28  cheshire
MUST NOT touch m->CurrentQuestion (or q) after calling AnswerCurrentQuestionWithResourceRecord()

Revision 1.415  2007/07/27 19:51:01  cheshire
Use symbol QC_addnocache instead of literal constant "2"

Revision 1.414  2007/07/27 19:30:39  cheshire
Changed mDNSQuestionCallback parameter from mDNSBool to QC_result,
to properly reflect tri-state nature of the possible responses

Revision 1.413  2007/07/27 18:44:01  cheshire
Rename "AnswerQuestionWithResourceRecord" to more informative "AnswerCurrentQuestionWithResourceRecord"

Revision 1.412  2007/07/27 18:38:56  cheshire
Rename "uDNS_CheckQuery" to more informative "uDNS_CheckCurrentQuestion"

Revision 1.411  2007/07/27 00:57:13  cheshire
Create hostname address records using standard kHostNameTTL (2 minutes) instead of 1 second

Revision 1.410  2007/07/25 21:41:00  vazquez
Make sure we clean up opened sockets when there are network transitions and when changing
port mappings

Revision 1.409  2007/07/25 03:05:02  vazquez
Fixes for:
<rdar://problem/5338913> LegacyNATTraversal: UPnP heap overflow
<rdar://problem/5338933> LegacyNATTraversal: UPnP stack buffer overflow
and a myriad of other security problems

Revision 1.408  2007/07/24 21:47:51  cheshire
Don't do mDNS_StopNATOperation() for operations we never started

Revision 1.407  2007/07/24 17:23:33  cheshire
<rdar://problem/5357133> Add list validation checks for debugging

Revision 1.406  2007/07/24 04:14:30  cheshire
<rdar://problem/5356281> LLQs not working in with NAT Traversal

Revision 1.405  2007/07/24 01:29:03  cheshire
<rdar://problem/5356026> DNSServiceNATPortMappingCreate() returns stale external address information

Revision 1.404  2007/07/20 23:10:51  cheshire
Fix code layout

Revision 1.403  2007/07/20 20:12:37  cheshire
Rename "mDNS_DomainTypeBrowseLegacy" as "mDNS_DomainTypeBrowseAutomatic"

Revision 1.402  2007/07/20 00:54:20  cheshire
<rdar://problem/4641118> Need separate SCPreferences for per-user .Mac settings

Revision 1.401  2007/07/18 03:23:33  cheshire
In GetServiceTarget, need to call SetupLocalAutoTunnelInterface_internal to bring up tunnel on demand, if necessary

Revision 1.400  2007/07/18 02:30:25  cheshire
Defer AutoTunnel server record advertising until we have at least one service to advertise
Do AutoTunnel target host selection in GetServiceTarget (instead of uDNS_RegisterService)

Revision 1.399  2007/07/18 01:02:28  cheshire
<rdar://problem/5304766> Register IPSec tunnel with IPv4-only hostname and create NAT port mappings
Declare records as kDNSRecordTypeKnownUnique so we don't get name conflicts with ourselves

Revision 1.398  2007/07/16 23:54:48  cheshire
<rdar://problem/5338850> Crash when removing or changing DNS keys

Revision 1.397  2007/07/16 20:13:31  vazquez
<rdar://problem/3867231> LegacyNATTraversal: Need complete rewrite

Revision 1.396  2007/07/14 00:33:04  cheshire
Remove temporary IPv4LL tunneling mode now that IPv6-over-IPv4 is working

Revision 1.395  2007/07/12 23:56:23  cheshire
Change "GetZoneData GOT SRV" message to debugf to reduce verbosity in syslog

Revision 1.394  2007/07/12 23:36:08  cheshire
Changed some 'LogOperation' calls to 'debugf' to reduce verbosity in syslog

Revision 1.393  2007/07/12 22:15:10  cheshire
Modified mDNS_SetSecretForDomain() so it can be called to update an existing entry

Revision 1.392  2007/07/12 02:51:27  cheshire
<rdar://problem/5303834> Automatically configure IPSec policy when resolving services

Revision 1.391  2007/07/11 23:16:31  cheshire
<rdar://problem/5304766> Register IPSec tunnel with IPv4-only hostname and create NAT port mappings
Need to prepend _autotunnel._udp to start of AutoTunnel SRV record name

Revision 1.390  2007/07/11 22:47:55  cheshire
<rdar://problem/5303807> Register IPv6-only hostname and don't create port mappings for services
In mDNS_SetSecretForDomain(), don't register records until after we've validated the parameters

Revision 1.389  2007/07/11 21:33:10  cheshire
<rdar://problem/5304766> Register IPSec tunnel with IPv4-only hostname and create NAT port mappings
Set up and register AutoTunnelTarget and AutoTunnelService DNS records

Revision 1.388  2007/07/11 19:27:10  cheshire
<rdar://problem/5303807> Register IPv6-only hostname and don't create port mappings for services
For temporary testing fake up an IPv4LL address instead of IPv6 ULA

Revision 1.387  2007/07/11 03:04:08  cheshire
<rdar://problem/5303807> Register IPv6-only hostname and don't create port mappings for AutoTunnel services
Add AutoTunnel parameter to mDNS_SetSecretForDomain; Set up AutoTunnel information for domains that require it

Revision 1.386  2007/07/10 01:57:28  cheshire
<rdar://problem/5196524> uDNS: mDNSresponder is leaking TCP connections to DNS server
Turned vast chunks of replicated code into a subroutine MakeTCPConn(...);
Made routines hold on to the reference it returns instead of leaking it

Revision 1.385  2007/07/09 23:50:18  cheshire
unlinkSRS needs to call mDNS_StopNATOperation_internal(), not mDNS_StopNATOperation()

Revision 1.384  2007/07/06 21:20:21  cheshire
Fix scheduling error (was causing "Task Scheduling Error: Continuously busy for more than a second")

Revision 1.383  2007/07/06 18:59:59  cheshire
Avoid spinning in an infinite loop when uDNS_SendNATMsg() returns an error

Revision 1.382  2007/07/04 00:49:43  vazquez
Clean up extraneous comments

Revision 1.381  2007/07/03 00:41:14  vazquez
 More changes for <rdar://problem/5301908> Clean up NAT state machine (necessary for 6 other fixes)
 Safely deal with packet replies and client callbacks

Revision 1.380  2007/07/02 22:08:47  cheshire
Fixed crash in "Received public IP address" message

Revision 1.379  2007/06/29 00:08:49  vazquez
<rdar://problem/5301908> Clean up NAT state machine (necessary for 6 other fixes)

Revision 1.378  2007/06/27 20:25:10  cheshire
Expanded dnsbugtest comment, explaining requirement that we also need these
test queries to black-hole before they get to the root name servers.

Revision 1.377  2007/06/22 21:27:21  cheshire
Modified "could not convert shared secret from base64" log message

Revision 1.376  2007/06/20 01:10:12  cheshire
<rdar://problem/5280520> Sync iPhone changes into main mDNSResponder code

Revision 1.375  2007/06/15 21:54:51  cheshire
<rdar://problem/4883206> Add packet logging to help debugging private browsing over TLS

Revision 1.374  2007/06/12 02:15:26  cheshire
Fix incorrect "DNS Server passed" LogOperation message

Revision 1.373  2007/05/31 00:25:43  cheshire
<rdar://problem/5238688> Only send dnsbugtest query for questions where it's warranted

Revision 1.372  2007/05/25 17:03:45  cheshire
lenptr needs to be declared unsigned, otherwise sign extension can mess up the shifting and ORing operations

Revision 1.371  2007/05/24 00:11:44  cheshire
Remove unnecessary lenbuf field from tcpInfo_t

Revision 1.370  2007/05/23 00:30:59  cheshire
Don't change question->TargetQID when repeating query over TCP

Revision 1.369  2007/05/21 18:04:40  cheshire
Updated comments -- port_mapping_create_reply renamed to port_mapping_reply

Revision 1.368  2007/05/17 19:12:16  cheshire
Updated comment about finding matching pair of sockets

Revision 1.367  2007/05/15 23:38:00  cheshire
Need to grab lock before calling SendRecordRegistration();

Revision 1.366  2007/05/15 00:43:05  cheshire
<rdar://problem/4983538> uDNS serviceRegistrationCallback locking failures

Revision 1.365  2007/05/10 21:19:18  cheshire
Rate-limit DNS test queries to at most one per three seconds
(useful when we have a dozen active WAB queries, and then we join a new network)

Revision 1.364  2007/05/07 20:43:45  cheshire
<rdar://problem/4241419> Reduce the number of queries and announcements

Revision 1.363  2007/05/04 22:12:48  cheshire
Work towards solving <rdar://problem/5176892> "uDNS_CheckQuery: LastQTime" log messages
When code gets in this invalid state, double ThisQInterval each time, to avoid excessive logging

Revision 1.362  2007/05/04 21:23:05  cheshire
<rdar://problem/5167263> Private DNS always returns no answers in the initial LLQ setup response
Preparatory work to enable us to do a four-way LLQ handshake over TCP, if we decide that's what we want

Revision 1.361  2007/05/03 23:50:48  cheshire
<rdar://problem/4669229> mDNSResponder ignores bogus null target in SRV record
In the case of negative answers for the address record, set the server address to zerov4Addr

Revision 1.360  2007/05/03 22:40:38  cheshire
<rdar://problem/4669229> mDNSResponder ignores bogus null target in SRV record

Revision 1.359  2007/05/02 22:21:33  cheshire
<rdar://problem/5167331> RegisterRecord and RegisterService need to cancel StartGetZoneData

Revision 1.358  2007/05/01 21:46:31  cheshire
Move GetLLQOptData/GetPktLease from uDNS.c into DNSCommon.c so that dnsextd can use them

Revision 1.357  2007/05/01 01:33:49  cheshire
Removed "#define LLQ_Info DNSQuestion" and manually reconciled code that was still referring to "LLQ_Info"

Revision 1.356  2007/04/30 21:51:06  cheshire
Updated comments

Revision 1.355  2007/04/30 21:33:38  cheshire
Fix crash when a callback unregisters a service while the UpdateSRVRecords() loop
is iterating through the m->ServiceRegistrations list

Revision 1.354  2007/04/30 01:30:04  cheshire
GetZoneData_QuestionCallback needs to call client callback function on error, so client knows operation is finished
RecordRegistrationCallback and serviceRegistrationCallback need to clear nta reference when they're invoked

Revision 1.353  2007/04/28 01:28:25  cheshire
Fixed memory leak on error path in FoundDomain

Revision 1.352  2007/04/27 19:49:53  cheshire
In uDNS_ReceiveTestQuestionResponse, also check that srcport matches

Revision 1.351  2007/04/27 19:28:02  cheshire
Any code that calls StartGetZoneData needs to keep a handle to the structure, so
it can cancel it if necessary. (First noticed as a crash in Apple Remote Desktop
-- it would start a query and then quickly cancel it, and then when
StartGetZoneData completed, it had a dangling pointer and crashed.)

Revision 1.350  2007/04/26 22:47:14  cheshire
Defensive coding: tcpCallback only needs to check "if (closed)", not "if (!n && closed)"

Revision 1.349  2007/04/26 16:04:06  cheshire
In mDNS_AddDNSServer, check whether port matches
In uDNS_CheckQuery, handle case where startLLQHandshake changes q->llq->state to LLQ_Poll

Revision 1.348  2007/04/26 04:01:59  cheshire
Copy-and-paste error: Test should be "if (result == DNSServer_Passed)" not "if (result == DNSServer_Failed)"

Revision 1.347  2007/04/26 00:35:15  cheshire
<rdar://problem/5140339> uDNS: Domain discovery not working over VPN
Fixes to make sure results update correctly when connectivity changes (e.g. a DNS server
inside the firewall may give answers where a public one gives none, and vice versa.)

Revision 1.346  2007/04/25 19:16:59  cheshire
Don't set SuppressStdPort53Queries unless we do actually send a DNS packet

Revision 1.345  2007/04/25 18:05:11  cheshire
Don't try to restart inactive (duplicate) queries

Revision 1.344  2007/04/25 17:54:07  cheshire
Don't cancel Private LLQs using a clear-text UDP packet

Revision 1.343  2007/04/25 16:40:08  cheshire
Add comment explaining uDNS_recvLLQResponse logic

Revision 1.342  2007/04/25 02:14:38  cheshire
<rdar://problem/4246187> uDNS: Identical client queries should reference a single shared core query
Additional fixes to make LLQs work properly

Revision 1.341  2007/04/24 02:07:42  cheshire
<rdar://problem/4246187> Identical client queries should reference a single shared core query
Deleted some more redundant code

Revision 1.340  2007/04/23 22:01:23  cheshire
<rdar://problem/5094009> IPv6 filtering in AirPort base station breaks Wide-Area Bonjour
As of March 2007, AirPort base stations now block incoming IPv6 connections by default, so there's no point
advertising IPv6 addresses in DNS any more -- we have to assume that most of the time a host's IPv6 address
probably won't work for incoming connections (but its IPv4 address probably will, using NAT-PMP).

Revision 1.339  2007/04/22 06:02:03  cheshire
<rdar://problem/4615977> Query should immediately return failure when no server

Revision 1.338  2007/04/21 19:44:11  cheshire
Improve uDNS_HandleNATPortMapReply log message

Revision 1.337  2007/04/21 02:03:00  cheshire
Also need to set AddressRec->resrec.RecordType in the NAT case too

Revision 1.336  2007/04/20 21:16:12  cheshire
Fixed bogus double-registration of host name -- was causing these warning messages in syslog:
Error! Tried to register AuthRecord 0181FB0C host.example.com. (Addr) that's already in the list

Revision 1.335  2007/04/19 23:57:20  cheshire
Temporary workaround for some AirPort base stations that don't seem to like us requesting public port zero

Revision 1.334  2007/04/19 23:21:51  cheshire
Fixed a couple of places where the StartGetZoneData check was backwards

Revision 1.333  2007/04/19 22:50:53  cheshire
<rdar://problem/4246187> Identical client queries should reference a single shared core query

Revision 1.332  2007/04/19 20:34:32  cheshire
Add debugging log message in uDNS_CheckQuery()

Revision 1.331  2007/04/19 20:06:41  cheshire
Rename field 'Private' (sounds like a boolean) to more informative 'AuthInfo' (it's a DomainAuthInfo pointer)

Revision 1.330  2007/04/19 19:51:54  cheshire
Get rid of unnecessary initializeQuery() routine

Revision 1.329  2007/04/19 18:03:52  cheshire
Improved "mDNS_AddSearchDomain" log message

Revision 1.328  2007/04/18 20:57:20  cheshire
Commented out "GetAuthInfoForName none found" debugging message

Revision 1.327  2007/04/17 19:21:29  cheshire
<rdar://problem/5140339> Domain discovery not working over VPN

Revision 1.326  2007/04/16 20:49:39  cheshire
Fix compile errors for mDNSPosix build

Revision 1.325  2007/04/05 22:55:35  cheshire
<rdar://problem/5077076> Records are ending up in Lighthouse without expiry information

Revision 1.324  2007/04/05 20:43:30  cheshire
Collapse sprawling code onto one line -- this is part of a bigger block of identical
code that has been copied-and-pasted into six different places in the same file.
This really needs to be turned into a subroutine.

Revision 1.323  2007/04/04 21:48:52  cheshire
<rdar://problem/4720694> Combine unicast authoritative answer list with multicast list

Revision 1.322  2007/04/03 19:53:06  cheshire
Use mDNSSameIPPort (and similar) instead of accessing internal fields directly

Revision 1.321  2007/04/02 23:44:09  cheshire
Minor code tidying

Revision 1.320  2007/03/31 01:26:13  cheshire
Take out GetAuthInfoForName syslog message

Revision 1.319  2007/03/31 01:10:53  cheshire
Add debugging

Revision 1.318  2007/03/31 00:17:11  cheshire
Remove some LogMsgs

Revision 1.317  2007/03/29 00:09:31  cheshire
Improve "uDNS_InitLongLivedQuery" log message

Revision 1.316  2007/03/28 21:16:27  cheshire
Remove DumpPacket() call now that OPT pseudo-RR rrclass bug is fixed

Revision 1.315  2007/03/28 21:02:18  cheshire
<rdar://problem/3810563> Wide-Area Bonjour should work on multi-subnet private network

Revision 1.314  2007/03/28 15:56:37  cheshire
<rdar://problem/5085774> Add listing of NAT port mapping and GetAddrInfo requests in SIGINFO output

Revision 1.313  2007/03/28 01:27:32  cheshire
<rdar://problem/4996439> Unicast DNS polling server every three seconds
StartLLQPolling was using INIT_UCAST_POLL_INTERVAL instead of LLQ_POLL_INTERVAL for the retry interval

Revision 1.312  2007/03/27 23:48:21  cheshire
Use mDNS_StopGetDomains(), not mDNS_StopQuery()

Revision 1.311  2007/03/27 22:47:51  cheshire
Remove unnecessary "*(long*)0 = 0;" to generate crash and stack trace

Revision 1.310  2007/03/24 01:24:13  cheshire
Add validator for uDNS data structures; fixed crash in RegisterSearchDomains()

Revision 1.309  2007/03/24 00:47:53  cheshire
<rdar://problem/4983538> serviceRegistrationCallback: Locking Failure! mDNS_busy (1) != mDNS_reentrancy (2)
Locking in this file is all messed up. For now we'll just work around the issue.

Revision 1.308  2007/03/24 00:41:33  cheshire
Minor code cleanup (move variable declarations to minimum enclosing scope)

Revision 1.307  2007/03/21 23:06:00  cheshire
Rename uDNS_HostnameInfo to HostnameInfo; deleted some unused fields

Revision 1.306  2007/03/21 00:30:03  cheshire
<rdar://problem/4789455> Multiple errors in DNameList-related code

Revision 1.305  2007/03/20 17:07:15  cheshire
Rename "struct uDNS_TCPSocket_struct" to "TCPSocket", "struct uDNS_UDPSocket_struct" to "UDPSocket"

Revision 1.304  2007/03/17 00:02:11  cheshire
<rdar://problem/5067013> NAT-PMP: Lease TTL is being ignored

Revision 1.303  2007/03/10 03:26:44  cheshire
<rdar://problem/4961667> uDNS: LLQ refresh response packet causes cached records to be removed from cache

Revision 1.302  2007/03/10 02:29:58  cheshire
Added comments about NAT-PMP response functions

Revision 1.301  2007/03/10 02:02:58  cheshire
<rdar://problem/4961667> uDNS: LLQ refresh response packet causes cached records to be removed from cache
Eliminate unnecessary "InternalResponseHndlr responseCallback" function pointer

Revision 1.300  2007/03/08 18:56:00  cheshire
Fixed typo: "&v4.ip.v4.b[0]" is always non-zero (ampersand should not be there)

Revision 1.299  2007/02/28 01:45:47  cheshire
<rdar://problem/4683261> NAT-PMP: Port mapping refreshes should contain actual public port
<rdar://problem/5027863> Byte order bugs in uDNS.c, uds_daemon.c, dnssd_clientstub.c

Revision 1.298  2007/02/14 03:16:39  cheshire
<rdar://problem/4789477> Eliminate unnecessary malloc/free in mDNSCore code

Revision 1.297  2007/02/08 21:12:28  cheshire
<rdar://problem/4386497> Stop reading /etc/mDNSResponder.conf on every sleep/wake

Revision 1.296  2007/01/29 16:03:22  cheshire
Fix unused parameter warning

Revision 1.295  2007/01/27 03:34:27  cheshire
Made GetZoneData use standard queries (and cached results);
eliminated GetZoneData_Callback() packet response handler

Revision 1.294  2007/01/25 00:40:16  cheshire
Unified CNAME-following functionality into cache management code (which means CNAME-following
should now also work for mDNS queries too); deleted defunct pktResponseHndlr() routine.

Revision 1.293  2007/01/23 02:56:11  cheshire
Store negative results in the cache, instead of generating them out of pktResponseHndlr()

Revision 1.292  2007/01/20 01:32:40  cheshire
Update comments and debugging messages

Revision 1.291  2007/01/20 00:07:02  cheshire
When we have credentials in the keychain for a domain, we attempt private queries, but
if the authoritative server is not set up for private queries (i.e. no _dns-query-tls
or _dns-llq-tls record) then we need to fall back to conventional non-private queries.

Revision 1.290  2007/01/19 23:41:45  cheshire
Need to clear m->rec.r.resrec.RecordType after calling GetLLQOptData()

Revision 1.289  2007/01/19 23:32:07  cheshire
Eliminate pointless timenow variable

Revision 1.288  2007/01/19 23:26:08  cheshire
Right now tcpCallback does not run holding the lock, so no need to drop the lock before invoking callbacks

Revision 1.287  2007/01/19 22:55:41  cheshire
Eliminate redundant identical parameters to GetZoneData_StartQuery()

Revision 1.286  2007/01/19 21:17:33  cheshire
StartLLQPolling needs to call SetNextQueryTime() to cause query to be done in a timely fashion

Revision 1.285  2007/01/19 18:39:11  cheshire
Fix a bunch of parameters that should have been declared "const"

Revision 1.284  2007/01/19 18:28:28  cheshire
Improved debugging messages

Revision 1.283  2007/01/19 18:09:33  cheshire
Fixed getLLQAtIndex (now called GetLLQOptData):
1. It incorrectly assumed all EDNS0 OPT records are the same size (it ignored optlen)
2. It used inefficient memory copying instead of just returning a pointer

Revision 1.282  2007/01/17 22:06:01  cheshire
Replace duplicated literal constant "{ { 0 } }" with symbol "zeroIPPort"

Revision 1.281  2007/01/17 21:58:13  cheshire
For clarity, rename ntaContext field "isPrivate" to "ntaPrivate"

Revision 1.280  2007/01/17 21:46:02  cheshire
Remove redundant duplicated "isPrivate" field from LLQ_Info

Revision 1.279  2007/01/17 21:35:31  cheshire
For clarity, rename zoneData_t field "isPrivate" to "zonePrivate"

Revision 1.278  2007/01/16 03:04:16  cheshire
<rdar://problem/4917539> Add support for one-shot private queries as well as long-lived private queries
Don't cache result of ntaContextSRV(context) in a local variable --
the macro evaluates to a different result after we clear "context->isPrivate"

Revision 1.277  2007/01/10 22:51:58  cheshire
<rdar://problem/4917539> Add support for one-shot private queries as well as long-lived private queries

Revision 1.276  2007/01/10 02:09:30  cheshire
Better LogOperation record of keys read from System Keychain

Revision 1.275  2007/01/09 22:37:18  cheshire
Provide ten-second grace period for deleted keys, to give mDNSResponder
time to delete host name before it gives up access to the required key.

Revision 1.274  2007/01/09 01:16:32  cheshire
Improve "ERROR m->CurrentQuestion already set" debugging messages

Revision 1.273  2007/01/08 23:58:00  cheshire
Don't request regDomain and browseDomains in uDNS_SetupDNSConfig() -- it just ignores those results

Revision 1.272  2007/01/05 08:30:42  cheshire
Trim excessive "$Log" checkin history from before 2006
(checkin history still available via "cvs log ..." of course)

Revision 1.271  2007/01/05 06:34:03  cheshire
Improve "ERROR m->CurrentQuestion already set" debugging messages

Revision 1.270  2007/01/05 05:44:33  cheshire
Move automatic browse/registration management from uDNS.c to mDNSShared/uds_daemon.c,
so that mDNSPosix embedded clients will compile again

Revision 1.269  2007/01/04 23:11:13  cheshire
<rdar://problem/4720673> uDNS: Need to start caching unicast records
When an automatic browsing domain is removed, generate appropriate "remove" events for legacy queries

Revision 1.268  2007/01/04 22:06:38  cheshire
Fixed crash in LLQNatMapComplete()

Revision 1.267  2007/01/04 21:45:20  cheshire
Added mDNS_DropLockBeforeCallback/mDNS_ReclaimLockAfterCallback macros,
to do additional lock sanity checking around callback invocations

Revision 1.266  2007/01/04 21:01:20  cheshire
<rdar://problem/4607042> mDNSResponder NXDOMAIN and CNAME support
Only return NXDOMAIN results to clients that request them using kDNSServiceFlagsReturnIntermediates

Revision 1.265  2007/01/04 20:47:17  cheshire
Fixed crash in CheckForUnreferencedLLQMapping()

Revision 1.264  2007/01/04 20:39:27  cheshire
Fix locking mismatch

Revision 1.263  2007/01/04 02:39:53  cheshire
<rdar://problem/4030599> Hostname passed into DNSServiceRegister ignored for Wide-Area service registrations

Revision 1.262  2007/01/04 00:29:25  cheshire
Covert LogMsg() in GetAuthInfoForName to LogOperation()

Revision 1.261  2006/12/22 20:59:49  cheshire
<rdar://problem/4742742> Read *all* DNS keys from keychain,
 not just key for the system-wide default registration domain

Revision 1.260  2006/12/21 00:06:07  cheshire
Don't need to do mDNSPlatformMemZero() -- mDNS_SetupResourceRecord() does it for us

Revision 1.259  2006/12/20 04:07:36  cheshire
Remove uDNS_info substructure from AuthRecord_struct

Revision 1.258  2006/12/19 22:49:24  cheshire
Remove uDNS_info substructure from ServiceRecordSet_struct

Revision 1.257  2006/12/19 02:38:20  cheshire
Get rid of unnecessary duplicate query ID field from DNSQuestion_struct

Revision 1.256  2006/12/19 02:18:48  cheshire
Get rid of unnecessary duplicate "void *context" field from DNSQuestion_struct

Revision 1.255  2006/12/16 01:58:31  cheshire
<rdar://problem/4720673> uDNS: Need to start caching unicast records

Revision 1.254  2006/12/15 19:23:39  cheshire
Use new DomainNameLengthLimit() function, to be more defensive against malformed
data received from the network.

Revision 1.253  2006/12/01 07:43:34  herscher
Fix byte ordering problem for one-shot TCP queries.
Iterate more intelligently over duplicates in uDNS_ReceiveMsg to avoid spin loops.

Revision 1.252  2006/11/30 23:07:57  herscher
<rdar://problem/4765644> uDNS: Sync up with Lighthouse changes for Private DNS

Revision 1.251  2006/11/28 21:42:11  mkrochma
Work around a crashing bug that was introduced by uDNS and mDNS code unification

Revision 1.250  2006/11/18 05:01:30  cheshire
Preliminary support for unifying the uDNS and mDNS code,
including caching of uDNS answers

Revision 1.249  2006/11/10 07:44:04  herscher
<rdar://problem/4825493> Fix Daemon locking failures while toggling BTMM

Revision 1.248  2006/11/08 04:26:53  cheshire
Fix typo in debugging message

Revision 1.247  2006/10/20 05:35:04  herscher
<rdar://problem/4720713> uDNS: Merge unicast active question list with multicast list.

Revision 1.246  2006/10/11 19:29:41  herscher
<rdar://problem/4744553> uDNS: mDNSResponder-111 using 100% CPU

Revision 1.245  2006/10/04 22:21:15  herscher
Tidy up references to mDNS_struct introduced when the embedded uDNS_info struct was removed.

Revision 1.244  2006/10/04 21:51:27  herscher
Replace calls to mDNSPlatformTimeNow(m) with m->timenow

Revision 1.243  2006/10/04 21:38:59  herscher
Remove uDNS_info substructure from DNSQuestion_struct

Revision 1.242  2006/09/27 00:51:46  herscher
Fix compile error when _LEGACY_NAT_TRAVERSAL_ is not defined

Revision 1.241  2006/09/26 01:54:47  herscher
<rdar://problem/4245016> NAT Port Mapping API (for both NAT-PMP and UPnP Gateway Protocol)

Revision 1.240  2006/09/15 21:20:15  cheshire
Remove uDNS_info substructure from mDNS_struct

Revision 1.239  2006/08/16 02:52:56  mkrochma
<rdar://problem/4104154> Actually fix it this time

Revision 1.238  2006/08/16 00:31:50  mkrochma
<rdar://problem/4386944> Get rid of NotAnInteger references

Revision 1.237  2006/08/15 23:38:17  mkrochma
<rdar://problem/4104154> Requested Public Port field should be set to zero on mapping deletion

Revision 1.236  2006/08/14 23:24:23  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.235  2006/07/30 05:45:36  cheshire
<rdar://problem/4304215> Eliminate MIN_UCAST_PERIODIC_EXEC

Revision 1.234  2006/07/22 02:58:36  cheshire
Code was clearing namehash twice instead of namehash and rdatahash

Revision 1.233  2006/07/20 19:46:51  mkrochma
<rdar://problem/4472014> Add Private DNS client functionality to mDNSResponder
Fix Private DNS

Revision 1.232  2006/07/15 02:01:29  cheshire
<rdar://problem/4472014> Add Private DNS client functionality to mDNSResponder
Fix broken "empty string" browsing

Revision 1.231  2006/07/05 23:28:22  cheshire
<rdar://problem/4472014> Add Private DNS client functionality to mDNSResponder

Revision 1.230  2006/06/29 03:02:44  cheshire
<rdar://problem/4607042> mDNSResponder NXDOMAIN and CNAME support

Revision 1.229  2006/03/02 22:03:41  cheshire
<rdar://problem/4395331> Spurious warning "GetLargeResourceRecord: m->rec appears to be already in use"
Refinement: m->rec.r.resrec.RecordType needs to be cleared *every* time around for loop, not just once at the end

Revision 1.228  2006/02/26 00:54:42  cheshire
Fixes to avoid code generation warning/error on FreeBSD 7

Revision 1.227  2006/01/09 20:47:05  cheshire
<rdar://problem/4395331> Spurious warning "GetLargeResourceRecord: m->rec appears to be already in use"

*/

#include "uDNS.h"

#if(defined(_MSC_VER))
	// Disable "assignment within conditional expression".
	// Other compilers understand the convention that if you place the assignment expression within an extra pair
	// of parentheses, this signals to the compiler that you really intended an assignment and no warning is necessary.
	// The Microsoft compiler doesn't understand this convention, so in the absense of any other way to signal
	// to the compiler that the assignment is intentional, we have to just turn this warning off completely.
	#pragma warning(disable:4706)
#endif

// For domain enumeration and automatic browsing
// This is the user's DNS search list.
// In each of these domains we search for our special pointer records (lb._dns-sd._udp.<domain>, etc.)
// to discover recommended domains for domain enumeration (browse, default browse, registration,
// default registration) and possibly one or more recommended automatic browsing domains.
mDNSexport SearchListElem *SearchList = mDNSNULL;

// Temporary workaround to make ServiceRecordSet list management safe.
// Ideally a ServiceRecordSet shouldn't be a special entity that's given special treatment by the uDNS code
// -- it should just be a grouping of records that are treated the same as any other registered records.
// In that case it may no longer be necessary to keep an explicit list of ServiceRecordSets, which in turn
// would avoid the perils of modifying that list cleanly while some other piece of code is iterating through it.
ServiceRecordSet *CurrentServiceRecordSet = mDNSNULL;

// The value can be set to true by the Platform code e.g., MacOSX uses the plist mechanism
mDNSBool StrictUnicastOrdering = mDNSfalse;

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - General Utility Functions
#endif

// Unlink an AuthRecord from the m->ResourceRecords list.
// This seems risky. Probably some (or maybe all) of the places calling UnlinkAuthRecord to directly
// remove a record from the list should actually be using mDNS_Deregister/mDNS_Deregister_internal.
mDNSlocal mStatus UnlinkAuthRecord(mDNS *const m, AuthRecord *const rr)
	{
	AuthRecord **list = &m->ResourceRecords;
	if (m->NewLocalRecords == rr) m->NewLocalRecords = rr->next;
	if (m->CurrentRecord == rr) m->CurrentRecord = rr->next;
	while (*list && *list != rr) list = &(*list)->next;
	if (!*list)
		{
		list = &m->DuplicateRecords;
		while (*list && *list != rr) list = &(*list)->next;
		}
	if (*list) { *list = rr->next; rr->next = mDNSNULL; return(mStatus_NoError); }
	LogMsg("ERROR: UnlinkAuthRecord - no such active record %##s", rr->resrec.name->c);
	return(mStatus_NoSuchRecord);
	}

// unlinkSRS is an internal routine (i.e. must be called with the lock already held)
mDNSlocal void unlinkSRS(mDNS *const m, ServiceRecordSet *srs)
	{
	ServiceRecordSet **p;

	if (srs->NATinfo.clientContext)
		{
		mDNS_StopNATOperation_internal(m, &srs->NATinfo);
		srs->NATinfo.clientContext = mDNSNULL;
		}

	for (p = &m->ServiceRegistrations; *p; p = &(*p)->uDNS_next)
		if (*p == srs)
			{
			ExtraResourceRecord *e;
			*p = srs->uDNS_next;
			if (CurrentServiceRecordSet == srs)
				CurrentServiceRecordSet = srs->uDNS_next;
			srs->uDNS_next = mDNSNULL;
			for (e=srs->Extras; e; e=e->next)
				if (UnlinkAuthRecord(m, &e->r))
					LogMsg("unlinkSRS: extra record %##s not found", e->r.resrec.name->c);
			return;
			}
	LogMsg("ERROR: unlinkSRS - SRS not found in ServiceRegistrations list %##s", srs->RR_SRV.resrec.name->c);
	}

// set retry timestamp for record with exponential backoff
// (for service record sets, use RR_SRV as representative for time checks
mDNSlocal void SetRecordRetry(mDNS *const m, AuthRecord *rr, mStatus SendErr)
	{
	mDNSs32 elapsed = m->timenow - rr->LastAPTime;
	rr->LastAPTime = m->timenow;

#if 0
	// Code for stress-testing registration renewal code
	if (rr->expire && rr->expire - m->timenow > mDNSPlatformOneSecond * 120)
		{
		LogInfo("Adjusting expiry from %d to 120 seconds for %s",
			(rr->expire - m->timenow) / mDNSPlatformOneSecond, ARDisplayString(m, rr));
		rr->expire = m->timenow + mDNSPlatformOneSecond * 120;
		}
#endif

	if (rr->expire && rr->expire - m->timenow > mDNSPlatformOneSecond)
		{
		mDNSs32 remaining = rr->expire - m->timenow;
		rr->ThisAPInterval = remaining/2 + mDNSRandom(remaining/10);
		debugf("SetRecordRetry refresh in %4d of %4d for %s",
			rr->ThisAPInterval / mDNSPlatformOneSecond, (rr->expire - m->timenow) / mDNSPlatformOneSecond, ARDisplayString(m, rr));
		return;
		}

	rr->expire = 0;

	// If at least half our our time interval has elapsed, it's time to double rr->ThisAPInterval
	// If resulting interval is too small, set to at least INIT_UCAST_POLL_INTERVAL (3 seconds)
	// If resulting interval is too large, set to at most 30 minutes
	if (rr->ThisAPInterval / 2 <= elapsed) rr->ThisAPInterval *= 2;
	if (rr->ThisAPInterval < INIT_UCAST_POLL_INTERVAL || SendErr == mStatus_TransientErr)
		rr->ThisAPInterval = INIT_UCAST_POLL_INTERVAL;
	rr->ThisAPInterval += mDNSRandom(rr->ThisAPInterval/20);
	if (rr->ThisAPInterval > 30 * 60 * mDNSPlatformOneSecond)
		rr->ThisAPInterval = 30 * 60 * mDNSPlatformOneSecond;

	LogInfo("SetRecordRetry retry   in %4d for %s", rr->ThisAPInterval / mDNSPlatformOneSecond, ARDisplayString(m, rr));
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Name Server List Management
#endif

mDNSexport DNSServer *mDNS_AddDNSServer(mDNS *const m, const domainname *d, const mDNSInterfaceID interface, const mDNSAddr *addr, const mDNSIPPort port)
	{
	DNSServer **p = &m->DNSServers;
	DNSServer *tmp = mDNSNULL;
	
	if (!d) d = (const domainname *)"";

	LogInfo("mDNS_AddDNSServer: Adding %#a for %##s", addr, d->c);
	if (m->mDNS_busy != m->mDNS_reentrancy+1)
		LogMsg("mDNS_AddDNSServer: Lock not held! mDNS_busy (%ld) mDNS_reentrancy (%ld)", m->mDNS_busy, m->mDNS_reentrancy);

	while (*p)	// Check if we already have this {interface,address,port,domain} tuple registered
		{
		if ((*p)->interface == interface && (*p)->teststate != DNSServer_Disabled &&
			mDNSSameAddress(&(*p)->addr, addr) && mDNSSameIPPort((*p)->port, port) && SameDomainName(&(*p)->domain, d))
			{
			if (!((*p)->flags & DNSServer_FlagDelete)) debugf("Note: DNS Server %#a:%d for domain %##s (%p) registered more than once", addr, mDNSVal16(port), d->c, interface);
			(*p)->flags &= ~DNSServer_FlagDelete;
			tmp = *p;
			*p = tmp->next;
			tmp->next = mDNSNULL;
			}
		else
			p=&(*p)->next;
		}

	if (tmp) *p = tmp; // move to end of list, to ensure ordering from platform layer
	else
		{
		// allocate, add to list
		*p = mDNSPlatformMemAllocate(sizeof(**p));
		if (!*p) LogMsg("Error: mDNS_AddDNSServer - malloc");
		else
			{
			(*p)->interface = interface;
			(*p)->addr      = *addr;
			(*p)->port      = port;
			(*p)->flags     = DNSServer_FlagNew;
			(*p)->teststate = /* DNSServer_Untested */ DNSServer_Passed;
			(*p)->lasttest  = m->timenow - INIT_UCAST_POLL_INTERVAL;
			AssignDomainName(&(*p)->domain, d);
			(*p)->next = mDNSNULL;
			}
		}
	(*p)->penaltyTime = 0;
	return(*p);
	}

// PenalizeDNSServer is called when the number of queries to the unicast
// DNS server exceeds MAX_UCAST_UNANSWERED_QUERIES or when we receive an
// error e.g., SERV_FAIL from DNS server. QueryFail is TRUE if this function
// is called when we exceed MAX_UCAST_UNANSWERED_QUERIES

mDNSexport void PenalizeDNSServer(mDNS *const m, DNSQuestion *q, mDNSBool QueryFail)
	{
	DNSServer *orig = q->qDNSServer;
	
	if (m->mDNS_busy != m->mDNS_reentrancy+1)
		LogMsg("PenalizeDNSServer: Lock not held! mDNS_busy (%ld) mDNS_reentrancy (%ld)", m->mDNS_busy, m->mDNS_reentrancy);

	if (!q->qDNSServer)
		{
		LogMsg("PenalizeDNSServer: Null DNS server for %##s (%s) %d", q->qname.c, DNSTypeName(q->qtype), q->unansweredQueries);
		goto end;
		}

	if (QueryFail)
		{
			LogInfo("PenalizeDNSServer: DNS server %#a:%d (%##s) %d unanswered queries for %##s (%s)",
		&q->qDNSServer->addr, mDNSVal16(q->qDNSServer->port), q->qDNSServer->domain.c, q->unansweredQueries, q->qname.c, DNSTypeName(q->qtype));
		}
	else
		{
			LogInfo("PenalizeDNSServer: DNS server %#a:%d (%##s) Server Error for %##s (%s)",
		&q->qDNSServer->addr, mDNSVal16(q->qDNSServer->port), q->qDNSServer->domain.c, q->qname.c, DNSTypeName(q->qtype));
		}


	// If strict ordering of unicast servers needs to be preserved, we just lookup
	// the next best match server below
	//
	// If strict ordering is not required which is the default behavior, we penalize the server
	// for DNSSERVER_PENALTY_TIME. We may also use additional logic e.g., don't penalize for PTR
	// in the future.

	if (!StrictUnicastOrdering)
		{
		LogInfo("PenalizeDNSServer: Strict Unicast Ordering is FALSE");
		// We penalize the server so that new queries don't pick this server for DNSSERVER_PENALTY_TIME
		// XXX Include other logic here to see if this server should really be penalized
		//
		if (q->qtype == kDNSType_PTR)
			{
			LogInfo("PenalizeDNSServer: Not Penalizing PTR question");
			}
		else
			{
			LogInfo("PenalizeDNSServer: Penalizing question type %d", q->qtype);
			q->qDNSServer->penaltyTime = NonZeroTime(m->timenow + DNSSERVER_PENALTY_TIME);
			}
		}
	else
		{
		LogInfo("PenalizeDNSServer: Strict Unicast Ordering is TRUE");
		}

end:
	q->qDNSServer = GetServerForName(m, &q->qname, q->qDNSServer);

	if ((q->qDNSServer != orig) && (QueryFail))
		{
		// We picked a new server. In the case where QueryFail is true, the code has already incremented the interval
		// and to compensate that we decrease it here.  When two queries are sent, the QuestionIntervalStep is at 9. We just
		// move it back to 3 here when we pick a new server. We can't start at 1 because if we have two servers failing, we will never
		// backoff 
		//
		q->ThisQInterval = q->ThisQInterval / QuestionIntervalStep;
		if (q->qDNSServer) LogInfo("PenalizeDNSServer: Server for %##s (%s) changed to %#a:%d (%##s), Question Interval %u", q->qname.c, DNSTypeName(q->qtype), &q->qDNSServer->addr, mDNSVal16(q->qDNSServer->port), q->qDNSServer->domain.c, q->ThisQInterval);
		else               LogInfo("PenalizeDNSServer: Server for %##s (%s) changed to <null>, Question Interval %u",        q->qname.c, DNSTypeName(q->qtype), q->ThisQInterval);

		}
	else 
		{
		// if we are here it means,
		//
		// 1) We picked the same server, QueryFail = false
		// 2) We picked the same server, QueryFail = true 
		// 3) We picked a different server, QueryFail = false
		//
		// For all these three cases, ThisQInterval is already set properly

		if (q->qDNSServer) 
			{
			if (q->qDNSServer != orig)
				{
				LogInfo("PenalizeDNSServer: Server for %##s (%s) changed to %#a:%d (%##s)", q->qname.c, DNSTypeName(q->qtype), &q->qDNSServer->addr, mDNSVal16(q->qDNSServer->port), q->qDNSServer->domain.c);
				}
				else
				{
				LogInfo("PenalizeDNSServer: Server for %##s (%s) remains the same at %#a:%d (%##s)", q->qname.c, DNSTypeName(q->qtype), &q->qDNSServer->addr, mDNSVal16(q->qDNSServer->port), q->qDNSServer->domain.c);
				}
			}
		else
			{ 
			LogInfo("PenalizeDNSServer: Server for %##s (%s) changed to <null>",        q->qname.c, DNSTypeName(q->qtype));
			}
		}
	q->unansweredQueries = 0;
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - authorization management
#endif

mDNSlocal DomainAuthInfo *GetAuthInfoForName_direct(mDNS *m, const domainname *const name)
	{
	const domainname *n = name;
	while (n->c[0])
		{
		DomainAuthInfo *ptr;
		for (ptr = m->AuthInfoList; ptr; ptr = ptr->next)
			if (SameDomainName(&ptr->domain, n))
				{
				debugf("GetAuthInfoForName %##s Matched %##s Key name %##s", name->c, ptr->domain.c, ptr->keyname.c);
				return(ptr);
				}
		n = (const domainname *)(n->c + 1 + n->c[0]);
		}
	//LogInfo("GetAuthInfoForName none found for %##s", name->c);
	return mDNSNULL;
	}

// MUST be called with lock held
mDNSexport DomainAuthInfo *GetAuthInfoForName_internal(mDNS *m, const domainname *const name)
	{
	DomainAuthInfo **p = &m->AuthInfoList;

	if (m->mDNS_busy != m->mDNS_reentrancy+1)
		LogMsg("GetAuthInfoForName_internal: Lock not held! mDNS_busy (%ld) mDNS_reentrancy (%ld)", m->mDNS_busy, m->mDNS_reentrancy);

	// First purge any dead keys from the list
	while (*p)
		{
		if ((*p)->deltime && m->timenow - (*p)->deltime >= 0 && AutoTunnelUnregistered(*p))
			{
			DNSQuestion *q;
			DomainAuthInfo *info = *p;
			LogInfo("GetAuthInfoForName_internal deleting expired key %##s %##s", info->domain.c, info->keyname.c);
			*p = info->next;	// Cut DomainAuthInfo from list *before* scanning our question list updating AuthInfo pointers
			for (q = m->Questions; q; q=q->next)
				if (q->AuthInfo == info)
					{
					q->AuthInfo = GetAuthInfoForName_direct(m, &q->qname);
					debugf("GetAuthInfoForName_internal updated q->AuthInfo from %##s to %##s for %##s (%s)",
						info->domain.c, q->AuthInfo ? q->AuthInfo->domain.c : mDNSNULL, q->qname.c, DNSTypeName(q->qtype));
					}

			// Probably not essential, but just to be safe, zero out the secret key data
			// so we don't leave it hanging around in memory
			// (where it could potentially get exposed via some other bug)
			mDNSPlatformMemZero(info, sizeof(*info));
			mDNSPlatformMemFree(info);
			}
		else
			p = &(*p)->next;
		}

	return(GetAuthInfoForName_direct(m, name));
	}

mDNSexport DomainAuthInfo *GetAuthInfoForName(mDNS *m, const domainname *const name)
	{
	DomainAuthInfo *d;
	mDNS_Lock(m);
	d = GetAuthInfoForName_internal(m, name);
	mDNS_Unlock(m);
	return(d);
	}

// MUST be called with the lock held
mDNSexport mStatus mDNS_SetSecretForDomain(mDNS *m, DomainAuthInfo *info,
	const domainname *domain, const domainname *keyname, const char *b64keydata, mDNSBool AutoTunnel)
	{
	DNSQuestion *q;
	DomainAuthInfo **p = &m->AuthInfoList;
	if (!info || !b64keydata) { LogMsg("mDNS_SetSecretForDomain: ERROR: info %p b64keydata %p", info, b64keydata); return(mStatus_BadParamErr); }

	LogInfo("mDNS_SetSecretForDomain: domain %##s key %##s%s", domain->c, keyname->c, AutoTunnel ? " AutoTunnel" : "");

	info->AutoTunnel = AutoTunnel;
	AssignDomainName(&info->domain,  domain);
	AssignDomainName(&info->keyname, keyname);
	mDNS_snprintf(info->b64keydata, sizeof(info->b64keydata), "%s", b64keydata);

	if (DNSDigest_ConstructHMACKeyfromBase64(info, b64keydata) < 0)
		{
		LogMsg("mDNS_SetSecretForDomain: ERROR: Could not convert shared secret from base64: domain %##s key %##s %s", domain->c, keyname->c, mDNS_LoggingEnabled ? b64keydata : "");
		return(mStatus_BadParamErr);
		}

	// Don't clear deltime until after we've ascertained that b64keydata is valid
	info->deltime = 0;

	while (*p && (*p) != info) p=&(*p)->next;
	if (*p) return(mStatus_AlreadyRegistered);

	// Caution: Only zero AutoTunnelHostRecord.namestorage and AutoTunnelNAT.clientContext AFTER we've determined that this is a NEW DomainAuthInfo
	// being added to the list. Otherwise we risk smashing our AutoTunnel host records and NATOperation that are already active and in use.
	info->AutoTunnelHostRecord.resrec.RecordType = kDNSRecordTypeUnregistered;
	info->AutoTunnelHostRecord.namestorage.c[0] = 0;
	info->AutoTunnelTarget    .resrec.RecordType = kDNSRecordTypeUnregistered;
	info->AutoTunnelDeviceInfo.resrec.RecordType = kDNSRecordTypeUnregistered;
	info->AutoTunnelService   .resrec.RecordType = kDNSRecordTypeUnregistered;
	info->AutoTunnelNAT.clientContext = mDNSNULL;
	info->next = mDNSNULL;
	*p = info;

	// Check to see if adding this new DomainAuthInfo has changed the credentials for any of our questions
	for (q = m->Questions; q; q=q->next)
		{
		DomainAuthInfo *newinfo = GetAuthInfoForQuestion(m, q);
		if (q->AuthInfo != newinfo)
			{
			debugf("mDNS_SetSecretForDomain updating q->AuthInfo from %##s to %##s for %##s (%s)",
				q->AuthInfo ? q->AuthInfo->domain.c : mDNSNULL,
				newinfo     ? newinfo    ->domain.c : mDNSNULL, q->qname.c, DNSTypeName(q->qtype));
			q->AuthInfo = newinfo;
			}
		}

	return(mStatus_NoError);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - NAT Traversal
#endif

mDNSlocal mStatus uDNS_SendNATMsg(mDNS *m, NATTraversalInfo *info)
	{
	mStatus err = mStatus_NoError;

	// send msg if we have a router and it is a private address
	if (!mDNSIPv4AddressIsZero(m->Router.ip.v4) && mDNSv4AddrIsRFC1918(&m->Router.ip.v4))
		{
		union { NATAddrRequest NATAddrReq; NATPortMapRequest NATPortReq; } u = { { NATMAP_VERS, NATOp_AddrRequest } } ;
		const mDNSu8 *end = (mDNSu8 *)&u + sizeof(NATAddrRequest);
	
		if (info)			// For NATOp_MapUDP and NATOp_MapTCP, fill in additional fields
			{
			mDNSu8 *p = (mDNSu8 *)&u.NATPortReq.NATReq_lease;
			u.NATPortReq.opcode  = info->Protocol;
			u.NATPortReq.unused  = zeroID;
			u.NATPortReq.intport = info->IntPort;
			u.NATPortReq.extport = info->RequestedPort;
			p[0] = (mDNSu8)((info->NATLease >> 24) &  0xFF);
			p[1] = (mDNSu8)((info->NATLease >> 16) &  0xFF);
			p[2] = (mDNSu8)((info->NATLease >>  8) &  0xFF);
			p[3] = (mDNSu8)( info->NATLease        &  0xFF);
			end = (mDNSu8 *)&u + sizeof(NATPortMapRequest);
			}

		err = mDNSPlatformSendUDP(m, (mDNSu8 *)&u, end, 0, mDNSNULL, &m->Router, NATPMPPort);

#ifdef _LEGACY_NAT_TRAVERSAL_
		if (mDNSIPPortIsZero(m->UPnPRouterPort) || mDNSIPPortIsZero(m->UPnPSOAPPort)) LNT_SendDiscoveryMsg(m);
		else if (info) err = LNT_MapPort(m, info);
		else err = LNT_GetExternalAddress(m);
#endif // _LEGACY_NAT_TRAVERSAL_
		}
	return(err);
	}

mDNSexport void RecreateNATMappings(mDNS *const m)
	{
	NATTraversalInfo *n;
	for (n = m->NATTraversals; n; n=n->next)
		{
		n->ExpiryTime    = 0;		// Mark this mapping as expired
		n->retryInterval = NATMAP_INIT_RETRY;
		n->retryPortMap  = m->timenow;
#ifdef _LEGACY_NAT_TRAVERSAL_
		if (n->tcpInfo.sock) { mDNSPlatformTCPCloseConnection(n->tcpInfo.sock); n->tcpInfo.sock = mDNSNULL; }
#endif // _LEGACY_NAT_TRAVERSAL_
		}

	m->NextScheduledNATOp = m->timenow;		// Need to send packets immediately
	}

mDNSexport void natTraversalHandleAddressReply(mDNS *const m, mDNSu16 err, mDNSv4Addr ExtAddr)
	{
	static mDNSu16 last_err = 0;
	
	if (err)
		{
		if (err != last_err) LogMsg("Error getting external address %d", err);
		ExtAddr = zerov4Addr;
		}
	else
		{
		LogInfo("Received external IP address %.4a from NAT", &ExtAddr);
		if (mDNSv4AddrIsRFC1918(&ExtAddr))
			LogMsg("Double NAT (external NAT gateway address %.4a is also a private RFC 1918 address)", &ExtAddr);
		if (mDNSIPv4AddressIsZero(ExtAddr))
			err = NATErr_NetFail; // fake error to handle routers that pathologically report success with the zero address
		}
		
	if (!mDNSSameIPv4Address(m->ExternalAddress, ExtAddr))
		{
		m->ExternalAddress = ExtAddr;
		RecreateNATMappings(m);		// Also sets NextScheduledNATOp for us
		}

	if (!err) // Success, back-off to maximum interval
		m->retryIntervalGetAddr = NATMAP_MAX_RETRY_INTERVAL;
	else if (!last_err) // Failure after success, retry quickly (then back-off exponentially)
		m->retryIntervalGetAddr = NATMAP_INIT_RETRY;
	// else back-off normally in case of pathological failures

	m->retryGetAddr = m->timenow + m->retryIntervalGetAddr;
	if (m->NextScheduledNATOp - m->retryIntervalGetAddr > 0)
		m->NextScheduledNATOp = m->retryIntervalGetAddr;

	last_err = err;
	}

// Both places that call NATSetNextRenewalTime() update m->NextScheduledNATOp correctly afterwards
mDNSlocal void NATSetNextRenewalTime(mDNS *const m, NATTraversalInfo *n)
	{
	n->retryInterval = (n->ExpiryTime - m->timenow)/2;
	if (n->retryInterval < NATMAP_MIN_RETRY_INTERVAL)	// Min retry interval is 2 seconds
		n->retryInterval = NATMAP_MIN_RETRY_INTERVAL;
	n->retryPortMap = m->timenow + n->retryInterval;
	}

// Note: When called from handleLNTPortMappingResponse() only pkt->err, pkt->extport and pkt->NATRep_lease fields are filled in
mDNSexport void natTraversalHandlePortMapReply(mDNS *const m, NATTraversalInfo *n, const mDNSInterfaceID InterfaceID, mDNSu16 err, mDNSIPPort extport, mDNSu32 lease)
	{
	const char *prot = n->Protocol == NATOp_MapUDP ? "UDP" : n->Protocol == NATOp_MapTCP ? "TCP" : "?";
	(void)prot;
	n->NewResult = err;
	if (err || lease == 0 || mDNSIPPortIsZero(extport))
		{
		LogInfo("natTraversalHandlePortMapReply: %p Response %s Port %5d External Port %5d lease %d error %d",
			n, prot, mDNSVal16(n->IntPort), mDNSVal16(extport), lease, err);
		n->retryInterval = NATMAP_MAX_RETRY_INTERVAL;
		n->retryPortMap = m->timenow + NATMAP_MAX_RETRY_INTERVAL;
		// No need to set m->NextScheduledNATOp here, since we're only ever extending the m->retryPortMap time
		if      (err == NATErr_Refused)                     n->NewResult = mStatus_NATPortMappingDisabled;
		else if (err > NATErr_None && err <= NATErr_Opcode) n->NewResult = mStatus_NATPortMappingUnsupported;
		}
	else
		{
		if (lease > 999999999UL / mDNSPlatformOneSecond)
			lease = 999999999UL / mDNSPlatformOneSecond;
		n->ExpiryTime = NonZeroTime(m->timenow + lease * mDNSPlatformOneSecond);
	
		if (!mDNSSameIPPort(n->RequestedPort, extport))
			LogInfo("natTraversalHandlePortMapReply: %p Response %s Port %5d External Port %5d changed to %5d",
				n, prot, mDNSVal16(n->IntPort), mDNSVal16(n->RequestedPort), mDNSVal16(extport));

		n->InterfaceID   = InterfaceID;
		n->RequestedPort = extport;
	
		LogInfo("natTraversalHandlePortMapReply: %p Response %s Port %5d External Port %5d lease %d",
			n, prot, mDNSVal16(n->IntPort), mDNSVal16(extport), lease);
	
		NATSetNextRenewalTime(m, n);			// Got our port mapping; now set timer to renew it at halfway point
		m->NextScheduledNATOp = m->timenow;		// May need to invoke client callback immediately
		}
	}

// Must be called with the mDNS_Lock held
mDNSexport mStatus mDNS_StartNATOperation_internal(mDNS *const m, NATTraversalInfo *traversal)
	{
	NATTraversalInfo **n;
	
	LogInfo("mDNS_StartNATOperation_internal Protocol %d IntPort %d RequestedPort %d NATLease %d",
		traversal->Protocol, mDNSVal16(traversal->IntPort), mDNSVal16(traversal->RequestedPort), traversal->NATLease);

	// Note: It important that new traversal requests are appended at the *end* of the list, not prepended at the start
	for (n = &m->NATTraversals; *n; n=&(*n)->next)
		{
		if (traversal == *n)
			{
			LogMsg("Error! Tried to add a NAT traversal that's already in the active list: request %p Prot %d Int %d TTL %d",
				traversal, traversal->Protocol, mDNSVal16(traversal->IntPort), traversal->NATLease);
			return(mStatus_AlreadyRegistered);
			}
		if (traversal->Protocol && traversal->Protocol == (*n)->Protocol && mDNSSameIPPort(traversal->IntPort, (*n)->IntPort) &&
			!mDNSSameIPPort(traversal->IntPort, SSHPort))
			LogMsg("Warning: Created port mapping request %p Prot %d Int %d TTL %d "
				"duplicates existing port mapping request %p Prot %d Int %d TTL %d",
				traversal, traversal->Protocol, mDNSVal16(traversal->IntPort), traversal->NATLease,
				*n,        (*n)     ->Protocol, mDNSVal16((*n)     ->IntPort), (*n)     ->NATLease);
		}

	// Initialize necessary fields
	traversal->next            = mDNSNULL;
	traversal->ExpiryTime      = 0;
	traversal->retryInterval   = NATMAP_INIT_RETRY;
	traversal->retryPortMap    = m->timenow;
	traversal->NewResult       = mStatus_NoError;
	traversal->ExternalAddress = onesIPv4Addr;
	traversal->ExternalPort    = zeroIPPort;
	traversal->Lifetime        = 0;
	traversal->Result          = mStatus_NoError;

	// set default lease if necessary
	if (!traversal->NATLease) traversal->NATLease = NATMAP_DEFAULT_LEASE;

#ifdef _LEGACY_NAT_TRAVERSAL_
	mDNSPlatformMemZero(&traversal->tcpInfo, sizeof(traversal->tcpInfo));
#endif // _LEGACY_NAT_TRAVERSAL_

	if (!m->NATTraversals)		// If this is our first NAT request, kick off an address request too
		{
		m->retryGetAddr         = m->timenow;
		m->retryIntervalGetAddr = NATMAP_INIT_RETRY;
		}

	m->NextScheduledNATOp = m->timenow;	// This will always trigger sending the packet ASAP, and generate client callback if necessary

	*n = traversal;		// Append new NATTraversalInfo to the end of our list

	return(mStatus_NoError);
	}

// Must be called with the mDNS_Lock held
mDNSexport mStatus mDNS_StopNATOperation_internal(mDNS *m, NATTraversalInfo *traversal)
	{
	mDNSBool unmap = mDNStrue;
	NATTraversalInfo *p;
	NATTraversalInfo **ptr = &m->NATTraversals;

	while (*ptr && *ptr != traversal) ptr=&(*ptr)->next;
	if (*ptr) *ptr = (*ptr)->next;		// If we found it, cut this NATTraversalInfo struct from our list
	else
		{
		LogMsg("mDNS_StopNATOperation: NATTraversalInfo %p not found in list", traversal);
		return(mStatus_BadReferenceErr);
		}

	LogInfo("mDNS_StopNATOperation_internal %d %d %d %d",
		traversal->Protocol, mDNSVal16(traversal->IntPort), mDNSVal16(traversal->RequestedPort), traversal->NATLease);

	if (m->CurrentNATTraversal == traversal)
		m->CurrentNATTraversal = m->CurrentNATTraversal->next;

	if (traversal->Protocol)
		for (p = m->NATTraversals; p; p=p->next)
			if (traversal->Protocol == p->Protocol && mDNSSameIPPort(traversal->IntPort, p->IntPort))
				{
				if (!mDNSSameIPPort(traversal->IntPort, SSHPort))
					LogMsg("Warning: Removed port mapping request %p Prot %d Int %d TTL %d "
						"duplicates existing port mapping request %p Prot %d Int %d TTL %d",
						traversal, traversal->Protocol, mDNSVal16(traversal->IntPort), traversal->NATLease,
						p,         p        ->Protocol, mDNSVal16(p        ->IntPort), p        ->NATLease);
				unmap = mDNSfalse;
				}

	if (traversal->ExpiryTime && unmap)
		{
		traversal->NATLease = 0;
		traversal->retryInterval = 0;
		uDNS_SendNATMsg(m, traversal);
		}

	// Even if we DIDN'T make a successful UPnP mapping yet, we might still have a partially-open TCP connection we need to clean up
	#ifdef _LEGACY_NAT_TRAVERSAL_
		{
		mStatus err = LNT_UnmapPort(m, traversal);
		if (err) LogMsg("Legacy NAT Traversal - unmap request failed with error %d", err);
		}
	#endif // _LEGACY_NAT_TRAVERSAL_

	return(mStatus_NoError);
	}

mDNSexport mStatus mDNS_StartNATOperation(mDNS *m, NATTraversalInfo *traversal)
	{
	mStatus status;
	mDNS_Lock(m);
	status = mDNS_StartNATOperation_internal(m, traversal);
	mDNS_Unlock(m);
	return(status);
	}

mDNSexport mStatus mDNS_StopNATOperation(mDNS *m, NATTraversalInfo *traversal)
	{
	mStatus status;
	mDNS_Lock(m);
	status = mDNS_StopNATOperation_internal(m, traversal);
	mDNS_Unlock(m);
	return(status);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Long-Lived Queries
#endif

// Lock must be held -- otherwise m->timenow is undefined
mDNSlocal void StartLLQPolling(mDNS *const m, DNSQuestion *q)
	{
	debugf("StartLLQPolling: %##s", q->qname.c);
	q->state = LLQ_Poll;
	q->ThisQInterval = INIT_UCAST_POLL_INTERVAL;
	// We want to send our poll query ASAP, but the "+ 1" is because if we set the time to now,
	// we risk causing spurious "SendQueries didn't send all its queries" log messages
	q->LastQTime     = m->timenow - q->ThisQInterval + 1;
	SetNextQueryTime(m, q);
#if APPLE_OSX_mDNSResponder
	UpdateAutoTunnelDomainStatuses(m);
#endif
	}

mDNSlocal mDNSu8 *putLLQ(DNSMessage *const msg, mDNSu8 *ptr, const DNSQuestion *const question, const LLQOptData *const data)
	{
	AuthRecord rr;
	ResourceRecord *opt = &rr.resrec;
	rdataOPT *optRD;

	//!!!KRS when we implement multiple llqs per message, we'll need to memmove anything past the question section
	ptr = putQuestion(msg, ptr, msg->data + AbsoluteMaxDNSMessageData, &question->qname, question->qtype, question->qclass);
	if (!ptr) { LogMsg("ERROR: putLLQ - putQuestion"); return mDNSNULL; }

	// locate OptRR if it exists, set pointer to end
	// !!!KRS implement me

	// format opt rr (fields not specified are zero-valued)
	mDNS_SetupResourceRecord(&rr, mDNSNULL, mDNSInterface_Any, kDNSType_OPT, kStandardTTL, kDNSRecordTypeKnownUnique, mDNSNULL, mDNSNULL);
	opt->rrclass    = NormalMaxDNSMessageData;
	opt->rdlength   = sizeof(rdataOPT);	// One option in this OPT record
	opt->rdestimate = sizeof(rdataOPT);

	optRD = &rr.resrec.rdata->u.opt[0];
	optRD->opt = kDNSOpt_LLQ;
	optRD->u.llq = *data;
	ptr = PutResourceRecordTTLJumbo(msg, ptr, &msg->h.numAdditionals, opt, 0);
	if (!ptr) { LogMsg("ERROR: putLLQ - PutResourceRecordTTLJumbo"); return mDNSNULL; }

	return ptr;
	}

// Normally we'd just request event packets be sent directly to m->LLQNAT.ExternalPort, except...
// with LLQs over TLS/TCP we're doing a weird thing where instead of requesting packets be sent to ExternalAddress:ExternalPort
// we're requesting that packets be sent to ExternalPort, but at the source address of our outgoing TCP connection.
// Normally, after going through the NAT gateway, the source address of our outgoing TCP connection is the same as ExternalAddress,
// so this is fine, except when the TCP connection ends up going over a VPN tunnel instead.
// To work around this, if we find that the source address for our TCP connection is not a private address, we tell the Dot Mac
// LLQ server to send events to us directly at port 5353 on that address, instead of at our mapped external NAT port.

mDNSlocal mDNSu16 GetLLQEventPort(const mDNS *const m, const mDNSAddr *const dst)
	{
	mDNSAddr src;
	mDNSPlatformSourceAddrForDest(&src, dst);
	//LogMsg("GetLLQEventPort: src %#a for dst %#a (%d)", &src, dst, mDNSv4AddrIsRFC1918(&src.ip.v4) ? mDNSVal16(m->LLQNAT.ExternalPort) : 0);
	return(mDNSv4AddrIsRFC1918(&src.ip.v4) ? mDNSVal16(m->LLQNAT.ExternalPort) : mDNSVal16(MulticastDNSPort));
	}

// Normally called with llq set.
// May be called with llq NULL, when retransmitting a lost Challenge Response
mDNSlocal void sendChallengeResponse(mDNS *const m, DNSQuestion *const q, const LLQOptData *llq)
	{
	mDNSu8 *responsePtr = m->omsg.data;
	LLQOptData llqBuf;

	if (q->ntries++ == kLLQ_MAX_TRIES)
		{
		LogMsg("sendChallengeResponse: %d failed attempts for LLQ %##s", kLLQ_MAX_TRIES, q->qname.c);
		StartLLQPolling(m,q);
		return;
		}

	if (!llq)		// Retransmission: need to make a new LLQOptData
		{
		llqBuf.vers     = kLLQ_Vers;
		llqBuf.llqOp    = kLLQOp_Setup;
		llqBuf.err      = LLQErr_NoError;	// Don't need to tell server UDP notification port when sending over UDP
		llqBuf.id       = q->id;
		llqBuf.llqlease = q->ReqLease;
		llq = &llqBuf;
		}

	q->LastQTime     = m->timenow;
	q->ThisQInterval = q->tcp ? 0 : (kLLQ_INIT_RESEND * q->ntries * mDNSPlatformOneSecond);		// If using TCP, don't need to retransmit
	SetNextQueryTime(m, q);

	// To simulate loss of challenge response packet, uncomment line below
	//if (q->ntries == 1) return;

	InitializeDNSMessage(&m->omsg.h, q->TargetQID, uQueryFlags);
	responsePtr = putLLQ(&m->omsg, responsePtr, q, llq);
	if (responsePtr)
		{
		mStatus err = mDNSSendDNSMessage(m, &m->omsg, responsePtr, mDNSInterface_Any, q->LocalSocket, &q->servAddr, q->servPort, q->tcp ? q->tcp->sock : mDNSNULL, q->AuthInfo);
		if (err)
			{
			LogMsg("sendChallengeResponse: mDNSSendDNSMessage%s failed: %d", q->tcp ? " (TCP)" : "", err);
			if (q->tcp) { DisposeTCPConn(q->tcp); q->tcp = mDNSNULL; }
			}
		}
	else StartLLQPolling(m,q);
	}

mDNSlocal void SetLLQTimer(mDNS *const m, DNSQuestion *const q, const LLQOptData *const llq)
	{
	mDNSs32 lease = (mDNSs32)llq->llqlease * mDNSPlatformOneSecond;
	q->ReqLease      = llq->llqlease;
	q->LastQTime     = m->timenow;
	q->expire        = m->timenow + lease;
	q->ThisQInterval = lease/2 + mDNSRandom(lease/10);
	debugf("SetLLQTimer setting %##s (%s) to %d %d", q->qname.c, DNSTypeName(q->qtype), lease/mDNSPlatformOneSecond, q->ThisQInterval/mDNSPlatformOneSecond);
	SetNextQueryTime(m, q);
	}

mDNSlocal void recvSetupResponse(mDNS *const m, mDNSu8 rcode, DNSQuestion *const q, const LLQOptData *const llq)
	{
	if (rcode && rcode != kDNSFlag1_RC_NXDomain)
		{ LogMsg("ERROR: recvSetupResponse %##s (%s) - rcode && rcode != kDNSFlag1_RC_NXDomain", q->qname.c, DNSTypeName(q->qtype)); return; }

	if (llq->llqOp != kLLQOp_Setup)
		{ LogMsg("ERROR: recvSetupResponse %##s (%s) - bad op %d", q->qname.c, DNSTypeName(q->qtype), llq->llqOp); return; }

	if (llq->vers != kLLQ_Vers)
		{ LogMsg("ERROR: recvSetupResponse %##s (%s) - bad vers %d", q->qname.c, DNSTypeName(q->qtype), llq->vers); return; }

	if (q->state == LLQ_InitialRequest)
		{
		//LogInfo("Got LLQ_InitialRequest");

		if (llq->err) { LogMsg("recvSetupResponse - received llq->err %d from server", llq->err); StartLLQPolling(m,q); return; }
	
		if (q->ReqLease != llq->llqlease)
			debugf("recvSetupResponse: requested lease %lu, granted lease %lu", q->ReqLease, llq->llqlease);
	
		// cache expiration in case we go to sleep before finishing setup
		q->ReqLease = llq->llqlease;
		q->expire = m->timenow + ((mDNSs32)llq->llqlease * mDNSPlatformOneSecond);
	
		// update state
		q->state  = LLQ_SecondaryRequest;
		q->id     = llq->id;
		q->ntries = 0; // first attempt to send response
		sendChallengeResponse(m, q, llq);
		}
	else if (q->state == LLQ_SecondaryRequest)
		{
		//LogInfo("Got LLQ_SecondaryRequest");

		// Fix this immediately if not sooner.  Copy the id from the LLQOptData into our DNSQuestion struct.  This is only
		// an issue for private LLQs, because we skip parts 2 and 3 of the handshake.  This is related to a bigger
		// problem of the current implementation of TCP LLQ setup: we're not handling state transitions correctly
		// if the server sends back SERVFULL or STATIC.
		if (q->AuthInfo)
			{
			LogInfo("Private LLQ_SecondaryRequest; copying id %08X%08X", llq->id.l[0], llq->id.l[1]);
			q->id = llq->id;
			}

		if (llq->err) { LogMsg("ERROR: recvSetupResponse %##s (%s) code %d from server", q->qname.c, DNSTypeName(q->qtype), llq->err); StartLLQPolling(m,q); return; }
		if (!mDNSSameOpaque64(&q->id, &llq->id))
			{ LogMsg("recvSetupResponse - ID changed.  discarding"); return; } // this can happen rarely (on packet loss + reordering)
		q->state         = LLQ_Established;
		q->ntries        = 0;
		SetLLQTimer(m, q, llq);
#if APPLE_OSX_mDNSResponder
		UpdateAutoTunnelDomainStatuses(m);
#endif
		}
	}

mDNSexport uDNS_LLQType uDNS_recvLLQResponse(mDNS *const m, const DNSMessage *const msg, const mDNSu8 *const end, const mDNSAddr *const srcaddr, const mDNSIPPort srcport)
	{
	DNSQuestion pktQ, *q;
	if (msg->h.numQuestions && getQuestion(msg, msg->data, end, 0, &pktQ))
		{
		const rdataOPT *opt = GetLLQOptData(m, msg, end);

		for (q = m->Questions; q; q = q->next)
			{
			if (!mDNSOpaque16IsZero(q->TargetQID) && q->LongLived && q->qtype == pktQ.qtype && q->qnamehash == pktQ.qnamehash && SameDomainName(&q->qname, &pktQ.qname))
				{
				debugf("uDNS_recvLLQResponse found %##s (%s) %d %#a %#a %X %X %X %X %d",
					q->qname.c, DNSTypeName(q->qtype), q->state, srcaddr, &q->servAddr,
					opt ? opt->u.llq.id.l[0] : 0, opt ? opt->u.llq.id.l[1] : 0, q->id.l[0], q->id.l[1], opt ? opt->u.llq.llqOp : 0);
				if (q->state == LLQ_Poll) debugf("uDNS_LLQ_Events: q->state == LLQ_Poll msg->h.id %d q->TargetQID %d", mDNSVal16(msg->h.id), mDNSVal16(q->TargetQID));
				if (q->state == LLQ_Poll && mDNSSameOpaque16(msg->h.id, q->TargetQID))
					{
					m->rec.r.resrec.RecordType = 0;		// Clear RecordType to show we're not still using it
					debugf("uDNS_recvLLQResponse got poll response; moving to LLQ_InitialRequest for %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
					q->state         = LLQ_InitialRequest;
					q->servPort      = zeroIPPort;		// Clear servPort so that startLLQHandshake will retry the GetZoneData processing
					q->ThisQInterval = LLQ_POLL_INTERVAL + mDNSRandom(LLQ_POLL_INTERVAL/10);	// Retry LLQ setup in approx 15 minutes
					q->LastQTime     = m->timenow;
					SetNextQueryTime(m, q);
					return uDNS_LLQ_Entire;		// uDNS_LLQ_Entire means flush stale records; assume a large effective TTL
					}
				// Note: In LLQ Event packets, the msg->h.id does not match our q->TargetQID, because in that case the msg->h.id nonce is selected by the server
				else if (opt && q->state == LLQ_Established && opt->u.llq.llqOp == kLLQOp_Event && mDNSSameOpaque64(&opt->u.llq.id, &q->id))
					{
					mDNSu8 *ackEnd;
					//debugf("Sending LLQ ack for %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
					InitializeDNSMessage(&m->omsg.h, msg->h.id, ResponseFlags);
					ackEnd = putLLQ(&m->omsg, m->omsg.data, q, &opt->u.llq);
					if (ackEnd) mDNSSendDNSMessage(m, &m->omsg, ackEnd, mDNSInterface_Any, q->LocalSocket, srcaddr, srcport, mDNSNULL, mDNSNULL);
					m->rec.r.resrec.RecordType = 0;		// Clear RecordType to show we're not still using it
					debugf("uDNS_LLQ_Events: q->state == LLQ_Established msg->h.id %d q->TargetQID %d", mDNSVal16(msg->h.id), mDNSVal16(q->TargetQID));
					return uDNS_LLQ_Events;
					}
				if (opt && mDNSSameOpaque16(msg->h.id, q->TargetQID))
					{
					if (q->state == LLQ_Established && opt->u.llq.llqOp == kLLQOp_Refresh && mDNSSameOpaque64(&opt->u.llq.id, &q->id) && msg->h.numAdditionals && !msg->h.numAnswers)
						{
						if (opt->u.llq.err != LLQErr_NoError) LogMsg("recvRefreshReply: received error %d from server", opt->u.llq.err);
						else
							{
							//LogInfo("Received refresh confirmation ntries %d for %##s (%s)", q->ntries, q->qname.c, DNSTypeName(q->qtype));
							// If we're waiting to go to sleep, then this LLQ deletion may have been the thing
							// we were waiting for, so schedule another check to see if we can sleep now.
							if (opt->u.llq.llqlease == 0 && m->SleepLimit) m->NextScheduledSPRetry = m->timenow;
							GrantCacheExtensions(m, q, opt->u.llq.llqlease);
							SetLLQTimer(m, q, &opt->u.llq);
							q->ntries = 0;
							}
						m->rec.r.resrec.RecordType = 0;		// Clear RecordType to show we're not still using it
						return uDNS_LLQ_Ignore;
						}
					if (q->state < LLQ_Established && mDNSSameAddress(srcaddr, &q->servAddr))
						{
						LLQ_State oldstate = q->state;
						recvSetupResponse(m, msg->h.flags.b[1] & kDNSFlag1_RC_Mask, q, &opt->u.llq);
						m->rec.r.resrec.RecordType = 0;		// Clear RecordType to show we're not still using it
						// We have a protocol anomaly here in the LLQ definition.
						// Both the challenge packet from the server and the ack+answers packet have opt->u.llq.llqOp == kLLQOp_Setup.
						// However, we need to treat them differently:
						// The challenge packet has no answers in it, and tells us nothing about whether our cache entries
						// are still valid, so this packet should not cause us to do anything that messes with our cache.
						// The ack+answers packet gives us the whole truth, so we should handle it by updating our cache
						// to match the answers in the packet, and only the answers in the packet.
						return (oldstate == LLQ_SecondaryRequest ? uDNS_LLQ_Entire : uDNS_LLQ_Ignore);
						}
					}
				}
			}
		m->rec.r.resrec.RecordType = 0;		// Clear RecordType to show we're not still using it
		}
	return uDNS_LLQ_Not;
	}

// Stub definition of TCPSocket_struct so we can access flags field. (Rest of TCPSocket_struct is platform-dependent.)
struct TCPSocket_struct { TCPSocketFlags flags; /* ... */ };

// tcpCallback is called to handle events (e.g. connection opening and data reception) on TCP connections for
// Private DNS operations -- private queries, private LLQs, private record updates and private service updates
mDNSlocal void tcpCallback(TCPSocket *sock, void *context, mDNSBool ConnectionEstablished, mStatus err)
	{
	tcpInfo_t *tcpInfo = (tcpInfo_t *)context;
	mDNSBool   closed  = mDNSfalse;
	mDNS      *m       = tcpInfo->m;
	DNSQuestion *const q = tcpInfo->question;
	tcpInfo_t **backpointer =
		q                 ? &q           ->tcp :
		tcpInfo->srs      ? &tcpInfo->srs->tcp :
		tcpInfo->rr       ? &tcpInfo->rr ->tcp : mDNSNULL;
	if (backpointer && *backpointer != tcpInfo)
		LogMsg("tcpCallback: %d backpointer %p incorrect tcpInfo %p question %p srs %p rr %p",
			mDNSPlatformTCPGetFD(tcpInfo->sock), *backpointer, tcpInfo, q, tcpInfo->srs, tcpInfo->rr);

	if (err) goto exit;

	if (ConnectionEstablished)
		{
		mDNSu8    *end = ((mDNSu8*) &tcpInfo->request) + tcpInfo->requestLen;
		DomainAuthInfo *AuthInfo;

		// Defensive coding for <rdar://problem/5546824> Crash in mDNSResponder at GetAuthInfoForName_internal + 366
		// Don't know yet what's causing this, but at least we can be cautious and try to avoid crashing if we find our pointers in an unexpected state
		if (tcpInfo->srs && tcpInfo->srs->RR_SRV.resrec.name != &tcpInfo->srs->RR_SRV.namestorage)
			LogMsg("tcpCallback: ERROR: tcpInfo->srs->RR_SRV.resrec.name %p != &tcpInfo->srs->RR_SRV.namestorage %p",
				tcpInfo->srs->RR_SRV.resrec.name, &tcpInfo->srs->RR_SRV.namestorage);
		if (tcpInfo->rr && tcpInfo->rr->resrec.name != &tcpInfo->rr->namestorage)
			LogMsg("tcpCallback: ERROR: tcpInfo->rr->resrec.name %p != &tcpInfo->rr->namestorage %p",
				tcpInfo->rr->resrec.name, &tcpInfo->rr->namestorage);
		if (tcpInfo->srs && tcpInfo->srs->RR_SRV.resrec.name != &tcpInfo->srs->RR_SRV.namestorage) return;
		if (tcpInfo->rr  && tcpInfo->rr->        resrec.name != &tcpInfo->rr->        namestorage) return;

		AuthInfo =  tcpInfo->srs ? GetAuthInfoForName(m, tcpInfo->srs->RR_SRV.resrec.name) :
					tcpInfo->rr  ? GetAuthInfoForName(m, tcpInfo->rr->resrec.name)         : mDNSNULL;

		// connection is established - send the message
		if (q && q->LongLived && q->state == LLQ_Established)
			{
			// Lease renewal over TCP, resulting from opening a TCP connection in sendLLQRefresh
			end = ((mDNSu8*) &tcpInfo->request) + tcpInfo->requestLen;
			}
		else if (q && q->LongLived && q->state != LLQ_Poll && !mDNSIPPortIsZero(m->LLQNAT.ExternalPort) && !mDNSIPPortIsZero(q->servPort))
			{
			// Notes:
			// If we have a NAT port mapping, ExternalPort is the external port
			// If we have a routable address so we don't need a port mapping, ExternalPort is the same as our own internal port
			// If we need a NAT port mapping but can't get one, then ExternalPort is zero
			LLQOptData llqData;			// set llq rdata
			llqData.vers  = kLLQ_Vers;
			llqData.llqOp = kLLQOp_Setup;
			llqData.err   = GetLLQEventPort(m, &tcpInfo->Addr);	// We're using TCP; tell server what UDP port to send notifications to
			LogInfo("tcpCallback: eventPort %d", llqData.err);
			llqData.id    = zeroOpaque64;
			llqData.llqlease = kLLQ_DefLease;
			InitializeDNSMessage(&tcpInfo->request.h, q->TargetQID, uQueryFlags);
			end = putLLQ(&tcpInfo->request, tcpInfo->request.data, q, &llqData);
			if (!end) { LogMsg("ERROR: tcpCallback - putLLQ"); err = mStatus_UnknownErr; goto exit; }
			AuthInfo = q->AuthInfo;		// Need to add TSIG to this message
			q->ntries = 0; // Reset ntries so that tcp/tls connection failures don't affect sendChallengeResponse failures
			}
		else if (q)
			{
			// LLQ Polling mode or non-LLQ uDNS over TCP
			InitializeDNSMessage(&tcpInfo->request.h, q->TargetQID, uQueryFlags);
			end = putQuestion(&tcpInfo->request, tcpInfo->request.data, tcpInfo->request.data + AbsoluteMaxDNSMessageData, &q->qname, q->qtype, q->qclass);
			AuthInfo = q->AuthInfo;		// Need to add TSIG to this message
			}

		err = mDNSSendDNSMessage(m, &tcpInfo->request, end, mDNSInterface_Any, mDNSNULL, &tcpInfo->Addr, tcpInfo->Port, sock, AuthInfo);
		if (err) { debugf("ERROR: tcpCallback: mDNSSendDNSMessage - %d", err); err = mStatus_UnknownErr; goto exit; }

		// Record time we sent this question
		if (q)
			{
			mDNS_Lock(m);
			q->LastQTime = m->timenow;
			if (q->ThisQInterval < (256 * mDNSPlatformOneSecond))	// Now we have a TCP connection open, make sure we wait at least 256 seconds before retrying
				q->ThisQInterval = (256 * mDNSPlatformOneSecond);
			SetNextQueryTime(m, q);
			mDNS_Unlock(m);
			}
		}
	else
		{
		long n;
		if (tcpInfo->nread < 2)			// First read the two-byte length preceeding the DNS message
			{
			mDNSu8 *lenptr = (mDNSu8 *)&tcpInfo->replylen;
			n = mDNSPlatformReadTCP(sock, lenptr + tcpInfo->nread, 2 - tcpInfo->nread, &closed);
			if (n < 0)
				{
				LogMsg("ERROR: tcpCallback - attempt to read message length failed (%d)", n);
				err = mStatus_ConnFailed;
				goto exit;
				}
			else if (closed)
				{
				// It's perfectly fine for this socket to close after the first reply. The server might
				// be sending gratuitous replies using UDP and doesn't have a need to leave the TCP socket open.
				// We'll only log this event if we've never received a reply before.
				// BIND 9 appears to close an idle connection after 30 seconds.
				if (tcpInfo->numReplies == 0)
					{
					LogMsg("ERROR: socket closed prematurely tcpInfo->nread = %d", tcpInfo->nread);
					err = mStatus_ConnFailed;
					goto exit;
					}
				else
					{
					// Note that we may not be doing the best thing if an error occurs after we've sent a second request
					// over this tcp connection.  That is, we only track whether we've received at least one response
					// which may have been to a previous request sent over this tcp connection.
					if (backpointer) *backpointer = mDNSNULL; // Clear client backpointer FIRST so we don't risk double-disposing our tcpInfo_t 
					DisposeTCPConn(tcpInfo);
					return;
					}
				}

			tcpInfo->nread += n;
			if (tcpInfo->nread < 2) goto exit;

			tcpInfo->replylen = (mDNSu16)((mDNSu16)lenptr[0] << 8 | lenptr[1]);
			if (tcpInfo->replylen < sizeof(DNSMessageHeader))
				{ LogMsg("ERROR: tcpCallback - length too short (%d bytes)", tcpInfo->replylen); err = mStatus_UnknownErr; goto exit; }

			tcpInfo->reply = mDNSPlatformMemAllocate(tcpInfo->replylen);
			if (!tcpInfo->reply) { LogMsg("ERROR: tcpCallback - malloc failed"); err = mStatus_NoMemoryErr; goto exit; }
			}

		n = mDNSPlatformReadTCP(sock, ((char *)tcpInfo->reply) + (tcpInfo->nread - 2), tcpInfo->replylen - (tcpInfo->nread - 2), &closed);

		if (n < 0)
			{
			LogMsg("ERROR: tcpCallback - read returned %d", n);
			err = mStatus_ConnFailed;
			goto exit;
			}
		else if (closed)
			{
			if (tcpInfo->numReplies == 0)
				{
				LogMsg("ERROR: socket closed prematurely tcpInfo->nread = %d", tcpInfo->nread);
				err = mStatus_ConnFailed;
				goto exit;
				}
			else
				{
				// Note that we may not be doing the best thing if an error occurs after we've sent a second request
				// over this tcp connection.  That is, we only track whether we've received at least one response
				// which may have been to a previous request sent over this tcp connection.
				if (backpointer) *backpointer = mDNSNULL; // Clear client backpointer FIRST so we don't risk double-disposing our tcpInfo_t 
				DisposeTCPConn(tcpInfo);
				return;
				}
			}

		tcpInfo->nread += n;

		if ((tcpInfo->nread - 2) == tcpInfo->replylen)
			{
			AuthRecord *rr    = tcpInfo->rr;
			DNSMessage *reply = tcpInfo->reply;
			mDNSu8     *end   = (mDNSu8 *)tcpInfo->reply + tcpInfo->replylen;
			mDNSAddr    Addr  = tcpInfo->Addr;
			mDNSIPPort  Port  = tcpInfo->Port;
			tcpInfo->numReplies++;
			tcpInfo->reply    = mDNSNULL;	// Detach reply buffer from tcpInfo_t, to make sure client callback can't cause it to be disposed
			tcpInfo->nread    = 0;
			tcpInfo->replylen = 0;
			
			// If we're going to dispose this connection, do it FIRST, before calling client callback
			// Note: Sleep code depends on us clearing *backpointer here -- it uses the clearing of rr->tcp and srs->tcp
			// as the signal that the DNS deregistration operation with the server has completed, and the machine may now sleep
			if (backpointer)
				if (!q || !q->LongLived || m->SleepState)
					{ *backpointer = mDNSNULL; DisposeTCPConn(tcpInfo); }
			
			if (rr && rr->resrec.RecordType == kDNSRecordTypeDeregistering)
				{
				mDNS_Lock(m);
				LogInfo("tcpCallback: CompleteDeregistration %s", ARDisplayString(m, rr));
				CompleteDeregistration(m, rr);		// Don't touch rr after this
				mDNS_Unlock(m);
				}
			else
				mDNSCoreReceive(m, reply, end, &Addr, Port, (sock->flags & kTCPSocketFlags_UseTLS) ? (mDNSAddr *)1 : mDNSNULL, zeroIPPort, 0);
			// USE CAUTION HERE: Invoking mDNSCoreReceive may have caused the environment to change, including canceling this operation itself
			
			mDNSPlatformMemFree(reply);
			return;
			}
		}

exit:

	if (err)
		{
		// Clear client backpointer FIRST -- that way if one of the callbacks cancels its operation
		// we won't end up double-disposing our tcpInfo_t
		if (backpointer) *backpointer = mDNSNULL;

		mDNS_Lock(m);		// Need to grab the lock to get m->timenow

		if (q)
			{
			if (q->ThisQInterval == 0)
				{
				// We get here when we fail to establish a new TCP/TLS connection that would have been used for a new LLQ request or an LLQ renewal.
				// Note that ThisQInterval is also zero when sendChallengeResponse resends the LLQ request on an extant TCP/TLS connection.
				q->LastQTime = m->timenow;
				if (q->LongLived)
					{
					// We didn't get the chance to send our request packet before the TCP/TLS connection failed.
					// We want to retry quickly, but want to back off exponentially in case the server is having issues.
					// Since ThisQInterval was 0, we can't just multiply by QuestionIntervalStep, we must track the number
					// of TCP/TLS connection failures using ntries.
					mDNSu32 count = q->ntries + 1; // want to wait at least 1 second before retrying

					q->ThisQInterval = InitialQuestionInterval;

					for (;count;count--)
						q->ThisQInterval *= QuestionIntervalStep;

					if (q->ThisQInterval > LLQ_POLL_INTERVAL)
						q->ThisQInterval = LLQ_POLL_INTERVAL;
					else
						q->ntries++;
						
					LogMsg("tcpCallback: stream connection for LLQ %##s (%s) failed %d times, retrying in %d ms", q->qname.c, DNSTypeName(q->qtype), q->ntries, q->ThisQInterval);
					}
				else
					{
					q->ThisQInterval = MAX_UCAST_POLL_INTERVAL;
					LogMsg("tcpCallback: stream connection for %##s (%s) failed, retrying in %d ms", q->qname.c, DNSTypeName(q->qtype), q->ThisQInterval);
					}
				SetNextQueryTime(m, q);
				}
			else if (q->LastQTime + q->ThisQInterval - m->timenow > (q->LongLived ? LLQ_POLL_INTERVAL : MAX_UCAST_POLL_INTERVAL))
				{
				// If we get an error and our next scheduled query for this question is more than the max interval from now,
				// reset the next query to ensure we wait no longer the maximum interval from now before trying again.
				q->LastQTime     = m->timenow;
				q->ThisQInterval = q->LongLived ? LLQ_POLL_INTERVAL : MAX_UCAST_POLL_INTERVAL;
				SetNextQueryTime(m, q);
				LogMsg("tcpCallback: stream connection for %##s (%s) failed, retrying in %d ms", q->qname.c, DNSTypeName(q->qtype), q->ThisQInterval);
				}
			
			// We're about to dispose of the TCP connection, so we must reset the state to retry over TCP/TLS
			// because sendChallengeResponse will send the query via UDP if we don't have a tcp pointer.
			// Resetting to LLQ_InitialRequest will cause uDNS_CheckCurrentQuestion to call startLLQHandshake, which
			// will attempt to establish a new tcp connection.
			if (q->LongLived && q->state == LLQ_SecondaryRequest)
				q->state = LLQ_InitialRequest;
			
			// ConnFailed may happen if the server sends a TCP reset or TLS fails, in which case we want to retry establishing the LLQ
			// quickly rather than switching to polling mode.  This case is handled by the above code to set q->ThisQInterval just above.
			// If the error isn't ConnFailed, then the LLQ is in bad shape, so we switch to polling mode.
			if (err != mStatus_ConnFailed)
				{
				if (q->LongLived && q->state != LLQ_Poll) StartLLQPolling(m, q);
				}
			}

		if (tcpInfo->rr) SetRecordRetry(m, tcpInfo->rr, mStatus_NoError);

		if (tcpInfo->srs) SetRecordRetry(m, &tcpInfo->srs->RR_SRV, mStatus_NoError);

		mDNS_Unlock(m);

		DisposeTCPConn(tcpInfo);
		}
	}

mDNSlocal tcpInfo_t *MakeTCPConn(mDNS *const m, const DNSMessage *const msg, const mDNSu8 *const end,
	TCPSocketFlags flags, const mDNSAddr *const Addr, const mDNSIPPort Port,
	DNSQuestion *const question, ServiceRecordSet *const srs, AuthRecord *const rr)
	{
	mStatus err;
	mDNSIPPort srcport = zeroIPPort;
	tcpInfo_t *info = (tcpInfo_t *)mDNSPlatformMemAllocate(sizeof(tcpInfo_t));
	if (!info) { LogMsg("ERROR: MakeTCP - memallocate failed"); return(mDNSNULL); }
	mDNSPlatformMemZero(info, sizeof(tcpInfo_t));

	info->m          = m;
	info->sock       = mDNSPlatformTCPSocket(m, flags, &srcport);
	info->requestLen = 0;
	info->question   = question;
	info->srs        = srs;
	info->rr         = rr;
	info->Addr       = *Addr;
	info->Port       = Port;
	info->reply      = mDNSNULL;
	info->replylen   = 0;
	info->nread      = 0;
	info->numReplies = 0;

	if (msg)
		{
		info->requestLen = (int) (end - ((mDNSu8*)msg));
		mDNSPlatformMemCopy(&info->request, msg, info->requestLen);
		}

	if (!info->sock) { LogMsg("SendServiceRegistration: unable to create TCP socket"); mDNSPlatformMemFree(info); return(mDNSNULL); }
	err = mDNSPlatformTCPConnect(info->sock, Addr, Port, 0, tcpCallback, info);

	// Probably suboptimal here.
	// Instead of returning mDNSNULL here on failure, we should probably invoke the callback with an error code.
	// That way clients can put all the error handling and retry/recovery code in one place,
	// instead of having to handle immediate errors in one place and async errors in another.
	// Also: "err == mStatus_ConnEstablished" probably never happens.

	// Don't need to log "connection failed" in customer builds -- it happens quite often during sleep, wake, configuration changes, etc.
	if      (err == mStatus_ConnEstablished) { tcpCallback(info->sock, info, mDNStrue, mStatus_NoError); }
	else if (err != mStatus_ConnPending    ) { LogInfo("MakeTCPConnection: connection failed"); DisposeTCPConn(info); return(mDNSNULL); }
	return(info);
	}

mDNSexport void DisposeTCPConn(struct tcpInfo_t *tcp)
	{
	mDNSPlatformTCPCloseConnection(tcp->sock);
	if (tcp->reply) mDNSPlatformMemFree(tcp->reply);
	mDNSPlatformMemFree(tcp);
	}

// Lock must be held
mDNSexport void startLLQHandshake(mDNS *m, DNSQuestion *q)
	{
	if (mDNSIPv4AddressIsOnes(m->LLQNAT.ExternalAddress))
		{
		LogInfo("startLLQHandshake: waiting for NAT status for %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
		q->ThisQInterval = LLQ_POLL_INTERVAL + mDNSRandom(LLQ_POLL_INTERVAL/10);	// Retry in approx 15 minutes
		q->LastQTime = m->timenow;
		SetNextQueryTime(m, q);
		return;
		}

	if (mDNSIPPortIsZero(m->LLQNAT.ExternalPort))
		{
		LogInfo("startLLQHandshake: Cannot receive inbound packets; will poll for %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
		StartLLQPolling(m, q);
		return;
		}

	if (mDNSIPPortIsZero(q->servPort))
		{
		debugf("startLLQHandshake: StartGetZoneData for %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
		q->ThisQInterval = LLQ_POLL_INTERVAL + mDNSRandom(LLQ_POLL_INTERVAL/10);	// Retry in approx 15 minutes
		q->LastQTime     = m->timenow;
		SetNextQueryTime(m, q);
		q->servAddr = zeroAddr;
		// We know q->servPort is zero because of check above
		if (q->nta) CancelGetZoneData(m, q->nta);
		q->nta = StartGetZoneData(m, &q->qname, ZoneServiceLLQ, LLQGotZoneData, q);
		return;
		}

	if (q->AuthInfo)
		{
		if (q->tcp) LogInfo("startLLQHandshake: Disposing existing TCP connection for %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
		if (q->tcp) DisposeTCPConn(q->tcp);
		q->tcp = MakeTCPConn(m, mDNSNULL, mDNSNULL, kTCPSocketFlags_UseTLS, &q->servAddr, q->servPort, q, mDNSNULL, mDNSNULL);
		if (!q->tcp)
			q->ThisQInterval = mDNSPlatformOneSecond * 5;	// If TCP failed (transient networking glitch) try again in five seconds
		else
			{
			q->state         = LLQ_SecondaryRequest;		// Right now, for private DNS, we skip the four-way LLQ handshake
			q->ReqLease      = kLLQ_DefLease;
			q->ThisQInterval = 0;
			}
		q->LastQTime     = m->timenow;
		SetNextQueryTime(m, q);
		}
	else
		{
		debugf("startLLQHandshake: m->AdvertisedV4 %#a%s Server %#a:%d%s %##s (%s)",
			&m->AdvertisedV4,                     mDNSv4AddrIsRFC1918(&m->AdvertisedV4.ip.v4) ? " (RFC 1918)" : "",
			&q->servAddr, mDNSVal16(q->servPort), mDNSAddrIsRFC1918(&q->servAddr)             ? " (RFC 1918)" : "",
			q->qname.c, DNSTypeName(q->qtype));

		if (q->ntries++ >= kLLQ_MAX_TRIES)
			{
			LogMsg("startLLQHandshake: %d failed attempts for LLQ %##s Polling.", kLLQ_MAX_TRIES, q->qname.c);
			StartLLQPolling(m, q);
			}
		else
			{
			mDNSu8 *end;
			LLQOptData llqData;

			// set llq rdata
			llqData.vers  = kLLQ_Vers;
			llqData.llqOp = kLLQOp_Setup;
			llqData.err   = LLQErr_NoError;	// Don't need to tell server UDP notification port when sending over UDP
			llqData.id    = zeroOpaque64;
			llqData.llqlease = kLLQ_DefLease;
	
			InitializeDNSMessage(&m->omsg.h, q->TargetQID, uQueryFlags);
			end = putLLQ(&m->omsg, m->omsg.data, q, &llqData);
			if (!end) { LogMsg("ERROR: startLLQHandshake - putLLQ"); StartLLQPolling(m,q); return; }
	
			mDNSSendDNSMessage(m, &m->omsg, end, mDNSInterface_Any, q->LocalSocket, &q->servAddr, q->servPort, mDNSNULL, mDNSNULL);
	
			// update question state
			q->state         = LLQ_InitialRequest;
			q->ReqLease      = kLLQ_DefLease;
			q->ThisQInterval = (kLLQ_INIT_RESEND * mDNSPlatformOneSecond);
			q->LastQTime     = m->timenow;
			SetNextQueryTime(m, q);
			}
		}
	}

// forward declaration so GetServiceTarget can do reverse lookup if needed
mDNSlocal void GetStaticHostname(mDNS *m);

mDNSexport const domainname *GetServiceTarget(mDNS *m, AuthRecord *const rr)
	{
	debugf("GetServiceTarget %##s", rr->resrec.name->c);

	if (!rr->AutoTarget)		// If not automatically tracking this host's current name, just return the existing target
		return(&rr->resrec.rdata->u.srv.target);
	else
		{
#if APPLE_OSX_mDNSResponder
		DomainAuthInfo *AuthInfo = GetAuthInfoForName_internal(m, rr->resrec.name);
		if (AuthInfo && AuthInfo->AutoTunnel)
			{
			// If this AutoTunnel is not yet active, start it now (which entails activating its NAT Traversal request,
			// which will subsequently advertise the appropriate records when the NAT Traversal returns a result)
			if (!AuthInfo->AutoTunnelNAT.clientContext && m->AutoTunnelHostAddr.b[0])
				SetupLocalAutoTunnelInterface_internal(m);
			if (AuthInfo->AutoTunnelHostRecord.namestorage.c[0] == 0) return(mDNSNULL);
			return(&AuthInfo->AutoTunnelHostRecord.namestorage);
			}
		else
#endif // APPLE_OSX_mDNSResponder
			{
			const int srvcount = CountLabels(rr->resrec.name);
			HostnameInfo *besthi = mDNSNULL, *hi;
			int best = 0;
			for (hi = m->Hostnames; hi; hi = hi->next)
				if (hi->arv4.state == regState_Registered || hi->arv4.state == regState_Refresh ||
					hi->arv6.state == regState_Registered || hi->arv6.state == regState_Refresh)
					{
					int x, hostcount = CountLabels(&hi->fqdn);
					for (x = hostcount < srvcount ? hostcount : srvcount; x > 0 && x > best; x--)
						if (SameDomainName(SkipLeadingLabels(rr->resrec.name, srvcount - x), SkipLeadingLabels(&hi->fqdn, hostcount - x)))
							{ best = x; besthi = hi; }
					}
	
			if (besthi) return(&besthi->fqdn);
			}
		if (m->StaticHostname.c[0]) return(&m->StaticHostname);
		else GetStaticHostname(m); // asynchronously do reverse lookup for primary IPv4 address
		return(mDNSNULL);
		}
	}

// Called with lock held
mDNSlocal void SendServiceRegistration(mDNS *m, ServiceRecordSet *srs)
	{
	mDNSu8 *ptr = m->omsg.data;
	mDNSu8 *end = (mDNSu8 *)&m->omsg + sizeof(DNSMessage);
	mDNSOpaque16 id;
	mStatus err = mStatus_NoError;
	const domainname *target;
	mDNSu32 i;

	if (m->mDNS_busy != m->mDNS_reentrancy+1)
		LogMsg("SendServiceRegistration: Lock not held! mDNS_busy (%ld) mDNS_reentrancy (%ld)", m->mDNS_busy, m->mDNS_reentrancy);

	if (mDNSIPv4AddressIsZero(srs->SRSUpdateServer.ip.v4))	// Don't know our UpdateServer yet
		{
		srs->RR_SRV.LastAPTime = m->timenow;
		if (srs->RR_SRV.ThisAPInterval < mDNSPlatformOneSecond * 5)
			srs->RR_SRV.ThisAPInterval = mDNSPlatformOneSecond * 5;
		return;
		}

	if (srs->state == regState_Registered) srs->state = regState_Refresh;

	id = mDNS_NewMessageID(m);
	InitializeDNSMessage(&m->omsg.h, id, UpdateReqFlags);

	// setup resource records
	SetNewRData(&srs->RR_PTR.resrec, mDNSNULL, 0);		// Update rdlength, rdestimate, rdatahash
	SetNewRData(&srs->RR_TXT.resrec, mDNSNULL, 0);		// Update rdlength, rdestimate, rdatahash

	// replace port w/ NAT mapping if necessary
	if (srs->RR_SRV.AutoTarget == Target_AutoHostAndNATMAP && !mDNSIPPortIsZero(srs->NATinfo.ExternalPort))
		srs->RR_SRV.resrec.rdata->u.srv.port = srs->NATinfo.ExternalPort;

	// construct update packet
	// set zone
	ptr = putZone(&m->omsg, ptr, end, &srs->zone, mDNSOpaque16fromIntVal(srs->RR_SRV.resrec.rrclass));
	if (!ptr) { err = mStatus_UnknownErr; goto exit; }

	if (srs->TestForSelfConflict)
		{
		// update w/ prereq that SRV already exist to make sure previous registration was ours, and delete any stale TXT records
		if (!(ptr = PutResourceRecordTTLJumbo(&m->omsg, ptr, &m->omsg.h.mDNS_numPrereqs, &srs->RR_SRV.resrec, 0))) { err = mStatus_UnknownErr; goto exit; }
		if (!(ptr = putDeleteRRSet(&m->omsg, ptr, srs->RR_TXT.resrec.name, srs->RR_TXT.resrec.rrtype)))            { err = mStatus_UnknownErr; goto exit; }
		}

	else if (srs->state != regState_Refresh && srs->state != regState_UpdatePending)
		{
		// use SRV name for prereq
		//ptr = putPrereqNameNotInUse(srs->RR_SRV.resrec.name, &m->omsg, ptr, end);

		// For now, until we implement RFC 4701 (DHCID RR) to detect whether an existing record is someone else using the name, or just a
		// stale echo of our own previous registration before we changed our host name, we just overwrite whatever may have already been there
		ptr = putDeleteRRSet(&m->omsg, ptr, srs->RR_SRV.resrec.name, kDNSQType_ANY);
		if (!ptr) { err = mStatus_UnknownErr; goto exit; }
		}

	//!!!KRS Need to do bounds checking and use TCP if it won't fit!!!
	if (!(ptr = PutResourceRecordTTLJumbo(&m->omsg, ptr, &m->omsg.h.mDNS_numUpdates, &srs->RR_PTR.resrec, srs->RR_PTR.resrec.rroriginalttl))) { err = mStatus_UnknownErr; goto exit; }

	for (i = 0; i < srs->NumSubTypes; i++)
		if (!(ptr = PutResourceRecordTTLJumbo(&m->omsg, ptr, &m->omsg.h.mDNS_numUpdates, &srs->SubTypes[i].resrec, srs->SubTypes[i].resrec.rroriginalttl))) { err = mStatus_UnknownErr; goto exit; }

	if (srs->state == regState_UpdatePending) // we're updating the txt record
		{
		AuthRecord *txt = &srs->RR_TXT;
		// delete old RData
		SetNewRData(&txt->resrec, txt->OrigRData, txt->OrigRDLen);
		if (!(ptr = putDeletionRecord(&m->omsg, ptr, &srs->RR_TXT.resrec))) { err = mStatus_UnknownErr; goto exit; }	// delete old rdata

		// add new RData
		SetNewRData(&txt->resrec, txt->InFlightRData, txt->InFlightRDLen);
		if (!(ptr = PutResourceRecordTTLJumbo(&m->omsg, ptr, &m->omsg.h.mDNS_numUpdates, &srs->RR_TXT.resrec, srs->RR_TXT.resrec.rroriginalttl))) { err = mStatus_UnknownErr; goto exit; }
		}
	else
		if (!(ptr = PutResourceRecordTTLJumbo(&m->omsg, ptr, &m->omsg.h.mDNS_numUpdates, &srs->RR_TXT.resrec, srs->RR_TXT.resrec.rroriginalttl))) { err = mStatus_UnknownErr; goto exit; }

	target = GetServiceTarget(m, &srs->RR_SRV);
	if (!target || target->c[0] == 0)
		{
		debugf("SendServiceRegistration - no target for %##s", srs->RR_SRV.resrec.name->c);
		srs->state = regState_NoTarget;
		return;
		}

	if (!SameDomainName(target, &srs->RR_SRV.resrec.rdata->u.srv.target))
		{
		AssignDomainName(&srs->RR_SRV.resrec.rdata->u.srv.target, target);
		SetNewRData(&srs->RR_SRV.resrec, mDNSNULL, 0);		// Update rdlength, rdestimate, rdatahash
		}

	ptr = PutResourceRecordTTLJumbo(&m->omsg, ptr, &m->omsg.h.mDNS_numUpdates, &srs->RR_SRV.resrec, srs->RR_SRV.resrec.rroriginalttl);
	if (!ptr) { err = mStatus_UnknownErr; goto exit; }

	if (srs->srs_uselease)
		{ ptr = putUpdateLease(&m->omsg, ptr, DEFAULT_UPDATE_LEASE); if (!ptr) { err = mStatus_UnknownErr; goto exit; } }

	if (srs->state != regState_Refresh && srs->state != regState_DeregDeferred && srs->state != regState_UpdatePending)
		srs->state = regState_Pending;

	srs->id = id;

	if (srs->Private)
		{
		if (srs->tcp) LogInfo("SendServiceRegistration: Disposing existing TCP connection for %s", ARDisplayString(m, &srs->RR_SRV));
		if (srs->tcp) DisposeTCPConn(srs->tcp);
		srs->tcp = MakeTCPConn(m, &m->omsg, ptr, kTCPSocketFlags_UseTLS, &srs->SRSUpdateServer, srs->SRSUpdatePort, mDNSNULL, srs, mDNSNULL);
		if (!srs->tcp) srs->RR_SRV.ThisAPInterval = mDNSPlatformOneSecond * 5; // If failed to make TCP connection, try again in ten seconds (5*2)
		else if (srs->RR_SRV.ThisAPInterval < mDNSPlatformOneSecond * 30) srs->RR_SRV.ThisAPInterval = mDNSPlatformOneSecond * 30;
		}
	else
		{
		err = mDNSSendDNSMessage(m, &m->omsg, ptr, mDNSInterface_Any, mDNSNULL, &srs->SRSUpdateServer, srs->SRSUpdatePort, mDNSNULL, GetAuthInfoForName_internal(m, srs->RR_SRV.resrec.name));
		if (err) debugf("ERROR: SendServiceRegistration - mDNSSendDNSMessage - %d", err);
		}

	SetRecordRetry(m, &srs->RR_SRV, err);
	return;

exit:
	if (err)
		{
		LogMsg("SendServiceRegistration ERROR formatting message %d!! Permanently abandoning service registration %##s", err, srs->RR_SRV.resrec.name->c);
		unlinkSRS(m, srs);
		srs->state = regState_Unregistered;

		mDNS_DropLockBeforeCallback();
		srs->ServiceCallback(m, srs, err);
		mDNS_ReclaimLockAfterCallback();
		// CAUTION: MUST NOT do anything more with rr after calling rr->Callback(), because the client's callback function
		// is allowed to do anything, including starting/stopping queries, registering/deregistering records, etc.
		}
	}

mDNSlocal const domainname *PUBLIC_UPDATE_SERVICE_TYPE  = (const domainname*)"\x0B_dns-update"     "\x04_udp";
mDNSlocal const domainname *PUBLIC_LLQ_SERVICE_TYPE     = (const domainname*)"\x08_dns-llq"        "\x04_udp";

mDNSlocal const domainname *PRIVATE_UPDATE_SERVICE_TYPE = (const domainname*)"\x0F_dns-update-tls" "\x04_tcp";
mDNSlocal const domainname *PRIVATE_QUERY_SERVICE_TYPE  = (const domainname*)"\x0E_dns-query-tls"  "\x04_tcp";
mDNSlocal const domainname *PRIVATE_LLQ_SERVICE_TYPE    = (const domainname*)"\x0C_dns-llq-tls"    "\x04_tcp";

#define ZoneDataSRV(X) (\
	(X)->ZoneService == ZoneServiceUpdate ? ((X)->ZonePrivate ? PRIVATE_UPDATE_SERVICE_TYPE : PUBLIC_UPDATE_SERVICE_TYPE) : \
	(X)->ZoneService == ZoneServiceQuery  ? ((X)->ZonePrivate ? PRIVATE_QUERY_SERVICE_TYPE  : (const domainname*)""     ) : \
	(X)->ZoneService == ZoneServiceLLQ    ? ((X)->ZonePrivate ? PRIVATE_LLQ_SERVICE_TYPE    : PUBLIC_LLQ_SERVICE_TYPE   ) : (const domainname*)"")

// Forward reference: GetZoneData_StartQuery references GetZoneData_QuestionCallback, and
// GetZoneData_QuestionCallback calls GetZoneData_StartQuery
mDNSlocal mStatus GetZoneData_StartQuery(mDNS *const m, ZoneData *zd, mDNSu16 qtype);

// GetZoneData_QuestionCallback is called from normal client callback context (core API calls allowed)
mDNSlocal void GetZoneData_QuestionCallback(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
	{
	ZoneData *zd = (ZoneData*)question->QuestionContext;

	debugf("GetZoneData_QuestionCallback: %s %s", AddRecord ? "Add" : "Rmv", RRDisplayString(m, answer));

	if (!AddRecord) return;												// Don't care about REMOVE events
	if (AddRecord == QC_addnocache && answer->rdlength == 0) return;	// Don't care about transient failure indications
	if (answer->rrtype != question->qtype) return;						// Don't care about CNAMEs

	if (answer->rrtype == kDNSType_SOA)
		{
		debugf("GetZoneData GOT SOA %s", RRDisplayString(m, answer));
		mDNS_StopQuery(m, question);
		if (answer->rdlength)
			{
			AssignDomainName(&zd->ZoneName, answer->name);
			zd->ZoneClass = answer->rrclass;
			AssignDomainName(&zd->question.qname, &zd->ZoneName);
			GetZoneData_StartQuery(m, zd, kDNSType_SRV);
			}
		else if (zd->CurrentSOA->c[0])
			{
			zd->CurrentSOA = (domainname *)(zd->CurrentSOA->c + zd->CurrentSOA->c[0]+1);
			AssignDomainName(&zd->question.qname, zd->CurrentSOA);
			GetZoneData_StartQuery(m, zd, kDNSType_SOA);
			}
		else
			{
			LogInfo("GetZoneData recursed to root label of %##s without finding SOA", zd->ChildName.c);
			zd->ZoneDataCallback(m, mStatus_NoSuchNameErr, zd);
			mDNSPlatformMemFree(zd);
			}
		}
	else if (answer->rrtype == kDNSType_SRV)
		{
		debugf("GetZoneData GOT SRV %s", RRDisplayString(m, answer));
		mDNS_StopQuery(m, question);
// Right now we don't want to fail back to non-encrypted operations
// If the AuthInfo has the AutoTunnel field set, then we want private or nothing
// <rdar://problem/5687667> BTMM: Don't fallback to unencrypted operations when SRV lookup fails
#if 0
		if (!answer->rdlength && zd->ZonePrivate && zd->ZoneService != ZoneServiceQuery)
			{
			zd->ZonePrivate = mDNSfalse;	// Causes ZoneDataSRV() to yield a different SRV name when building the query
			GetZoneData_StartQuery(m, zd, kDNSType_SRV);		// Try again, non-private this time
			}
		else
#endif
			{
			if (answer->rdlength)
				{
				AssignDomainName(&zd->Host, &answer->rdata->u.srv.target);
				zd->Port = answer->rdata->u.srv.port;
				AssignDomainName(&zd->question.qname, &zd->Host);
				GetZoneData_StartQuery(m, zd, kDNSType_A);
				}
			else
				{
				zd->ZonePrivate = mDNSfalse;
				zd->Host.c[0] = 0;
				zd->Port = zeroIPPort;
				zd->Addr = zeroAddr;
				zd->ZoneDataCallback(m, mStatus_NoError, zd);
				mDNSPlatformMemFree(zd);
				}
			}
		}
	else if (answer->rrtype == kDNSType_A)
		{
		debugf("GetZoneData GOT A %s", RRDisplayString(m, answer));
		mDNS_StopQuery(m, question);
		zd->Addr.type  = mDNSAddrType_IPv4;
		zd->Addr.ip.v4 = (answer->rdlength == 4) ? answer->rdata->u.ipv4 : zerov4Addr;
		// In order to simulate firewalls blocking our outgoing TCP connections, returning immediate ICMP errors or TCP resets,
		// the code below will make us try to connect to loopback, resulting in an immediate "port unreachable" failure.
		// This helps us test to make sure we handle this case gracefully
		// <rdar://problem/5607082> BTMM: mDNSResponder taking 100 percent CPU after upgrading to 10.5.1
#if 0
		zd->Addr.ip.v4.b[0] = 127;
		zd->Addr.ip.v4.b[1] = 0;
		zd->Addr.ip.v4.b[2] = 0;
		zd->Addr.ip.v4.b[3] = 1;
#endif
		zd->ZoneDataCallback(m, mStatus_NoError, zd);
		mDNSPlatformMemFree(zd);
		}
	}

// GetZoneData_StartQuery is called from normal client context (lock not held, or client callback)
mDNSlocal mStatus GetZoneData_StartQuery(mDNS *const m, ZoneData *zd, mDNSu16 qtype)
	{
	if (qtype == kDNSType_SRV)
		{
		AssignDomainName(&zd->question.qname, ZoneDataSRV(zd));
		AppendDomainName(&zd->question.qname, &zd->ZoneName);
		debugf("lookupDNSPort %##s", zd->question.qname.c);
		}

	zd->question.ThisQInterval       = -1;		// So that GetZoneData_QuestionCallback() knows whether to cancel this question (Is this necessary?)
	zd->question.InterfaceID         = mDNSInterface_Any;
	zd->question.Target              = zeroAddr;
	//zd->question.qname.c[0]        = 0;			// Already set
	zd->question.qtype               = qtype;
	zd->question.qclass              = kDNSClass_IN;
	zd->question.LongLived           = mDNSfalse;
	zd->question.ExpectUnique        = mDNStrue;
	zd->question.ForceMCast          = mDNSfalse;
	zd->question.ReturnIntermed      = mDNStrue;
	zd->question.QuestionCallback    = GetZoneData_QuestionCallback;
	zd->question.QuestionContext     = zd;

	//LogMsg("GetZoneData_StartQuery %##s (%s) %p", zd->question.qname.c, DNSTypeName(zd->question.qtype), zd->question.Private);
	return(mDNS_StartQuery(m, &zd->question));
	}

// StartGetZoneData is an internal routine (i.e. must be called with the lock already held)
mDNSexport ZoneData *StartGetZoneData(mDNS *const m, const domainname *const name, const ZoneService target, ZoneDataCallback callback, void *ZoneDataContext)
	{
	DomainAuthInfo *AuthInfo = GetAuthInfoForName_internal(m, name);
	int initialskip = (AuthInfo && AuthInfo->AutoTunnel) ? DomainNameLength(name) - DomainNameLength(&AuthInfo->domain) : 0;
	ZoneData *zd = (ZoneData*)mDNSPlatformMemAllocate(sizeof(ZoneData));
	if (!zd) { LogMsg("ERROR: StartGetZoneData - mDNSPlatformMemAllocate failed"); return mDNSNULL; }
	mDNSPlatformMemZero(zd, sizeof(ZoneData));
	AssignDomainName(&zd->ChildName, name);
	zd->ZoneService      = target;
	zd->CurrentSOA       = (domainname *)(&zd->ChildName.c[initialskip]);
	zd->ZoneName.c[0]    = 0;
	zd->ZoneClass        = 0;
	zd->Host.c[0]        = 0;
	zd->Port             = zeroIPPort;
	zd->Addr             = zeroAddr;
	zd->ZonePrivate      = AuthInfo && AuthInfo->AutoTunnel ? mDNStrue : mDNSfalse;
	zd->ZoneDataCallback = callback;
	zd->ZoneDataContext  = ZoneDataContext;

	zd->question.QuestionContext = zd;
	AssignDomainName(&zd->question.qname, zd->CurrentSOA);

	mDNS_DropLockBeforeCallback();		// GetZoneData_StartQuery expects to be called from a normal callback, so we emulate that here
	GetZoneData_StartQuery(m, zd, kDNSType_SOA);
	mDNS_ReclaimLockAfterCallback();

	return zd;
	}

// GetZoneData queries are a special case -- even if we have a key for them, we don't do them privately,
// because that would result in an infinite loop (i.e. to do a private query we first need to get
// the _dns-query-tls SRV record for the zone, and we can't do *that* privately because to do so
// we'd need to already know the _dns-query-tls SRV record.
// Also, as a general rule, we never do SOA queries privately
mDNSexport DomainAuthInfo *GetAuthInfoForQuestion(mDNS *m, const DNSQuestion *const q)	// Must be called with lock held
	{
	if (q->QuestionCallback == GetZoneData_QuestionCallback) return(mDNSNULL);
	if (q->qtype            == kDNSType_SOA                ) return(mDNSNULL);
	return(GetAuthInfoForName_internal(m, &q->qname));
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - host name and interface management
#endif

// Called in normal client context (lock not held)
mDNSlocal void CompleteSRVNatMap(mDNS *m, NATTraversalInfo *n)
	{
	ServiceRecordSet *srs = (ServiceRecordSet *)n->clientContext;
	debugf("SRVNatMap complete %.4a IntPort %u ExternalPort %u NATLease %u", &n->ExternalAddress, mDNSVal16(n->IntPort), mDNSVal16(n->ExternalPort), n->NATLease);

	if (!srs) { LogMsg("CompleteSRVNatMap called with unknown ServiceRecordSet object"); return; }
	if (!n->NATLease) return;

	mDNS_Lock(m);
	if (!mDNSIPv4AddressIsZero(srs->SRSUpdateServer.ip.v4))
		SendServiceRegistration(m, srs);	// non-zero server address means we already have necessary zone data to send update
	else
		{
		// SHOULD NEVER HAPPEN!
		LogInfo("ERROR: CompleteSRVNatMap called but srs->SRSUpdateServer.ip.v4 is zero!");
		srs->state = regState_FetchingZoneData;
		if (srs->srs_nta) CancelGetZoneData(m, srs->srs_nta); // Make sure we cancel old one before we start a new one
		srs->srs_nta = StartGetZoneData(m, srs->RR_SRV.resrec.name, ZoneServiceUpdate, ServiceRegistrationGotZoneData, srs);
		}
	mDNS_Unlock(m);
	}

mDNSlocal void StartSRVNatMap(mDNS *m, ServiceRecordSet *srs)
	{
	const mDNSu8 *p = srs->RR_PTR.resrec.name->c;
	if (p[0]) p += 1 + p[0];
	if      (SameDomainLabel(p, (mDNSu8 *)"\x4" "_tcp")) srs->NATinfo.Protocol = NATOp_MapTCP;
	else if (SameDomainLabel(p, (mDNSu8 *)"\x4" "_udp")) srs->NATinfo.Protocol = NATOp_MapUDP;
	else { LogMsg("StartSRVNatMap: could not determine transport protocol of service %##s", srs->RR_SRV.resrec.name->c); return; }
	
	if (srs->NATinfo.clientContext) mDNS_StopNATOperation_internal(m, &srs->NATinfo);
	// Don't try to set IntPort here --
	// SendServiceRegistration overwrites srs->RR_SRV.resrec.rdata->u.srv.port with external (mapped) port number
	//srs->NATinfo.IntPort      = srs->RR_SRV.resrec.rdata->u.srv.port;
	srs->NATinfo.RequestedPort  = srs->RR_SRV.resrec.rdata->u.srv.port;
	srs->NATinfo.NATLease       = 0;		// Request default lease
	srs->NATinfo.clientCallback = CompleteSRVNatMap;
	srs->NATinfo.clientContext  = srs;
	mDNS_StartNATOperation_internal(m, &srs->NATinfo);
	}

// Called in normal callback context (i.e. mDNS_busy and mDNS_reentrancy are both 1)
mDNSexport void ServiceRegistrationGotZoneData(mDNS *const m, mStatus err, const ZoneData *zoneData)
	{
	ServiceRecordSet *srs = (ServiceRecordSet *)zoneData->ZoneDataContext;

	if (m->mDNS_busy != m->mDNS_reentrancy)
		LogMsg("ServiceRegistrationGotZoneData: mDNS_busy (%ld) != mDNS_reentrancy (%ld)", m->mDNS_busy, m->mDNS_reentrancy);

	if (!srs->RR_SRV.resrec.rdata)
		{ LogMsg("ServiceRegistrationGotZoneData: ERROR: srs->RR_SRV.resrec.rdata is NULL"); return; }

	srs->srs_nta = mDNSNULL;

	// Start off assuming we're going to use a lease
	// If we get an error from the server, and the update port as given in the SRV record is 53, then we'll retry without the lease option
	srs->srs_uselease = mDNStrue;

	if (err || !zoneData) return;

	if (mDNSIPPortIsZero(zoneData->Port) || mDNSAddressIsZero(&zoneData->Addr)) return;

	// cache zone data
	AssignDomainName(&srs->zone, &zoneData->ZoneName);
	srs->SRSUpdateServer.type = mDNSAddrType_IPv4;
	srs->SRSUpdateServer      = zoneData->Addr;
	srs->SRSUpdatePort        = zoneData->Port;
	srs->Private              = zoneData->ZonePrivate;

	srs->RR_SRV.LastAPTime     = m->timenow;
	srs->RR_SRV.ThisAPInterval = 0;

	debugf("ServiceRegistrationGotZoneData My IPv4 %#a%s Server %#a:%d%s for %##s",
		&m->AdvertisedV4, mDNSv4AddrIsRFC1918(&m->AdvertisedV4.ip.v4) ? " (RFC1918)" : "",
		&srs->SRSUpdateServer, mDNSVal16(srs->SRSUpdatePort), mDNSAddrIsRFC1918(&srs->SRSUpdateServer) ? " (RFC1918)" : "",
		srs->RR_SRV.resrec.name->c);

	// If we have non-zero service port (always?)
	// and a private address, and update server is non-private
	// and this service is AutoTarget
	// then initiate a NAT mapping request. On completion it will do SendServiceRegistration() for us
	if (!mDNSIPPortIsZero(srs->RR_SRV.resrec.rdata->u.srv.port) &&
		mDNSv4AddrIsRFC1918(&m->AdvertisedV4.ip.v4) && !mDNSAddrIsRFC1918(&srs->SRSUpdateServer) &&
		srs->RR_SRV.AutoTarget == Target_AutoHostAndNATMAP)
		{
		srs->state = regState_NATMap;
		debugf("ServiceRegistrationGotZoneData StartSRVNatMap");
		StartSRVNatMap(m, srs);
		}
	else
		{
		mDNS_Lock(m);
		SendServiceRegistration(m, srs);
		mDNS_Unlock(m);
		}
	}

mDNSlocal void SendServiceDeregistration(mDNS *m, ServiceRecordSet *srs)
	{
	mDNSOpaque16 id;
	mDNSu8 *ptr = m->omsg.data;
	mDNSu8 *end = (mDNSu8 *)&m->omsg + sizeof(DNSMessage);
	mStatus err = mStatus_UnknownErr;
	mDNSu32 i;

	if (mDNSIPv4AddressIsZero(srs->SRSUpdateServer.ip.v4))	// Don't know our UpdateServer yet
		{
		srs->RR_SRV.LastAPTime = m->timenow;
		if (srs->RR_SRV.ThisAPInterval < mDNSPlatformOneSecond * 5)
			srs->RR_SRV.ThisAPInterval = mDNSPlatformOneSecond * 5;
		return;
		}

	id = mDNS_NewMessageID(m);
	InitializeDNSMessage(&m->omsg.h, id, UpdateReqFlags);

	// put zone
	ptr = putZone(&m->omsg, ptr, end, &srs->zone, mDNSOpaque16fromIntVal(srs->RR_SRV.resrec.rrclass));
	if (!ptr) { LogMsg("ERROR: SendServiceDeregistration - putZone"); err = mStatus_UnknownErr; goto exit; }

	if (!(ptr = putDeleteAllRRSets(&m->omsg, ptr, srs->RR_SRV.resrec.name))) { err = mStatus_UnknownErr; goto exit; } // this deletes SRV, TXT, and Extras
	if (!(ptr = putDeletionRecord(&m->omsg, ptr, &srs->RR_PTR.resrec))) { err = mStatus_UnknownErr; goto exit; }
	for (i = 0; i < srs->NumSubTypes; i++)
		if (!(ptr = putDeletionRecord(&m->omsg, ptr, &srs->SubTypes[i].resrec))) { err = mStatus_UnknownErr; goto exit; }

	srs->id    = id;
	srs->state = regState_DeregPending;
	srs->RR_SRV.expire = 0;		// Indicate that we have no active registration any more

	if (srs->Private)
		{
		LogInfo("SendServiceDeregistration TCP %p %s", srs->tcp, ARDisplayString(m, &srs->RR_SRV));
		if (srs->tcp) LogInfo("SendServiceDeregistration: Disposing existing TCP connection for %s", ARDisplayString(m, &srs->RR_SRV));
		if (srs->tcp) DisposeTCPConn(srs->tcp);
		srs->tcp = MakeTCPConn(m, &m->omsg, ptr, kTCPSocketFlags_UseTLS, &srs->SRSUpdateServer, srs->SRSUpdatePort, mDNSNULL, srs, mDNSNULL);
		if (!srs->tcp) srs->RR_SRV.ThisAPInterval = mDNSPlatformOneSecond * 5; // If failed to make TCP connection, try again in ten seconds (5*2)
		else if (srs->RR_SRV.ThisAPInterval < mDNSPlatformOneSecond * 30) srs->RR_SRV.ThisAPInterval = mDNSPlatformOneSecond * 30;
		}
	else
		{
		err = mDNSSendDNSMessage(m, &m->omsg, ptr, mDNSInterface_Any, mDNSNULL, &srs->SRSUpdateServer, srs->SRSUpdatePort, mDNSNULL, GetAuthInfoForName_internal(m, srs->RR_SRV.resrec.name));
		if (err && err != mStatus_TransientErr) { debugf("ERROR: SendServiceDeregistration - mDNSSendDNSMessage - %d", err); goto exit; }
		}

	SetRecordRetry(m, &srs->RR_SRV, err);
	return;

exit:
	if (err)
		{
		LogMsg("SendServiceDeregistration ERROR formatting message %d!! Permanently abandoning service registration %##s", err, srs->RR_SRV.resrec.name->c);
		unlinkSRS(m, srs);
		srs->state = regState_Unregistered;
		}
	}

// Called with lock held
mDNSlocal void UpdateSRV(mDNS *m, ServiceRecordSet *srs)
	{
	ExtraResourceRecord *e;

	// Target change if:
	// We have a target and were previously waiting for one, or
	// We had a target and no longer do, or
	// The target has changed

	domainname *curtarget = &srs->RR_SRV.resrec.rdata->u.srv.target;
	const domainname *const nt = GetServiceTarget(m, &srs->RR_SRV);
	const domainname *const newtarget = nt ? nt : (domainname*)"";
	mDNSBool TargetChanged = (newtarget->c[0] && srs->state == regState_NoTarget) || !SameDomainName(curtarget, newtarget);
	mDNSBool HaveZoneData  = !mDNSIPv4AddressIsZero(srs->SRSUpdateServer.ip.v4);

	// Nat state change if:
	// We were behind a NAT, and now we are behind a new NAT, or
	// We're not behind a NAT but our port was previously mapped to a different external port
	// We were not behind a NAT and now we are

	mDNSIPPort port        = srs->RR_SRV.resrec.rdata->u.srv.port;
	mDNSBool NowNeedNATMAP = (srs->RR_SRV.AutoTarget == Target_AutoHostAndNATMAP && !mDNSIPPortIsZero(port) && mDNSv4AddrIsRFC1918(&m->AdvertisedV4.ip.v4) && !mDNSAddrIsRFC1918(&srs->SRSUpdateServer));
	mDNSBool WereBehindNAT = (srs->NATinfo.clientContext != mDNSNULL);
	mDNSBool PortWasMapped = (srs->NATinfo.clientContext && !mDNSSameIPPort(srs->NATinfo.RequestedPort, port));		// I think this is always false -- SC Sept 07
	mDNSBool NATChanged    = (!WereBehindNAT && NowNeedNATMAP) || (!NowNeedNATMAP && PortWasMapped);

	debugf("UpdateSRV %##s newtarget %##s TargetChanged %d HaveZoneData %d port %d NowNeedNATMAP %d WereBehindNAT %d PortWasMapped %d NATChanged %d",
		srs->RR_SRV.resrec.name->c, newtarget,
		TargetChanged, HaveZoneData, mDNSVal16(port), NowNeedNATMAP, WereBehindNAT, PortWasMapped, NATChanged);

	if (m->mDNS_busy != m->mDNS_reentrancy+1)
		LogMsg("UpdateSRV: Lock not held! mDNS_busy (%ld) mDNS_reentrancy (%ld)", m->mDNS_busy, m->mDNS_reentrancy);

	if (!TargetChanged && !NATChanged) return;

	switch(srs->state)
		{
		case regState_FetchingZoneData:
		case regState_DeregPending:
		case regState_DeregDeferred:
		case regState_Unregistered:
		case regState_NATMap:
		case regState_ExtraQueued:
			// In these states, the SRV has either not yet been registered (it will get up-to-date information when it is)
			// or is in the process of, or has already been, deregistered
			return;

		case regState_Pending:
		case regState_Refresh:
		case regState_UpdatePending:
			// let the in-flight operation complete before updating
			srs->SRVUpdateDeferred = mDNStrue;
			return;

		case regState_NATError:
			if (!NATChanged) return;
			// if nat changed, register if we have a target (below)

		case regState_NoTarget:
			if (newtarget->c[0])
				{
				debugf("UpdateSRV: %s service %##s", HaveZoneData ? (NATChanged && NowNeedNATMAP ? "Starting Port Map for" : "Registering") : "Getting Zone Data for", srs->RR_SRV.resrec.name->c);
				if (!HaveZoneData)
					{
					srs->state = regState_FetchingZoneData;
					if (srs->srs_nta) CancelGetZoneData(m, srs->srs_nta); // Make sure we cancel old one before we start a new one
					srs->srs_nta = StartGetZoneData(m, srs->RR_SRV.resrec.name, ZoneServiceUpdate, ServiceRegistrationGotZoneData, srs);
					}
				else
					{
					if (srs->NATinfo.clientContext && (NATChanged || !NowNeedNATMAP))
						{
						mDNS_StopNATOperation_internal(m, &srs->NATinfo);
						srs->NATinfo.clientContext = mDNSNULL;
						}
					if (NATChanged && NowNeedNATMAP && srs->RR_SRV.AutoTarget == Target_AutoHostAndNATMAP)
						{ srs->state = regState_NATMap; StartSRVNatMap(m, srs); }
					else SendServiceRegistration(m, srs);
					}
				}
			return;

		case regState_Registered:
			// target or nat changed.  deregister service.  upon completion, we'll look for a new target
			debugf("UpdateSRV: SRV record changed for service %##s - deregistering (will re-register with new SRV)",  srs->RR_SRV.resrec.name->c);
			for (e = srs->Extras; e; e = e->next) e->r.state = regState_ExtraQueued;	// extra will be re-registed if the service is re-registered
			srs->SRVChanged = mDNStrue;
			SendServiceDeregistration(m, srs);
			return;

		default: LogMsg("UpdateSRV: Unknown state %d for %##s", srs->state, srs->RR_SRV.resrec.name->c);
		}
	}

// Called with lock held
mDNSlocal void UpdateSRVRecords(mDNS *m)
	{
	debugf("UpdateSRVRecords%s", m->SleepState ? " (ignored due to SleepState)" : "");
	if (m->SleepState) return;

	if (CurrentServiceRecordSet)
		LogMsg("UpdateSRVRecords ERROR CurrentServiceRecordSet already set");
	CurrentServiceRecordSet = m->ServiceRegistrations;
	
	while (CurrentServiceRecordSet)
		{
		ServiceRecordSet *s = CurrentServiceRecordSet;
		CurrentServiceRecordSet = CurrentServiceRecordSet->uDNS_next;
		UpdateSRV(m, s);
		}

	mDNS_DropLockBeforeCallback();		// mDNS_SetFQDN expects to be called without the lock held, so we emulate that here
	mDNS_SetFQDN(m);
	mDNS_ReclaimLockAfterCallback();
	}

// Forward reference: AdvertiseHostname references HostnameCallback, and HostnameCallback calls AdvertiseHostname
mDNSlocal void HostnameCallback(mDNS *const m, AuthRecord *const rr, mStatus result);

// Called in normal client context (lock not held)
mDNSlocal void hostnameGetPublicAddressCallback(mDNS *m, NATTraversalInfo *n)
	{
	HostnameInfo *h = (HostnameInfo *)n->clientContext;

	if (!h) { LogMsg("RegisterHostnameRecord: registration cancelled"); return; }

	if (!n->Result)
		{
		if (mDNSIPv4AddressIsZero(n->ExternalAddress) || mDNSv4AddrIsRFC1918(&n->ExternalAddress)) return;
		
		if (h->arv4.resrec.RecordType)
			{
			if (mDNSSameIPv4Address(h->arv4.resrec.rdata->u.ipv4, n->ExternalAddress)) return;	// If address unchanged, do nothing
			LogInfo("Updating hostname %##s IPv4 from %.4a to %.4a (NAT gateway's external address)",
				h->arv4.resrec.name->c, &h->arv4.resrec.rdata->u.ipv4, &n->ExternalAddress);
			mDNS_Deregister(m, &h->arv4);	// mStatus_MemFree callback will re-register with new address
			}
		else
			{
			LogInfo("Advertising hostname %##s IPv4 %.4a (NAT gateway's external address)", h->arv4.resrec.name->c, &n->ExternalAddress);
			h->arv4.resrec.RecordType = kDNSRecordTypeKnownUnique;
			h->arv4.resrec.rdata->u.ipv4 = n->ExternalAddress;
			mDNS_Register(m, &h->arv4);
			}
		}
	}

// register record or begin NAT traversal
mDNSlocal void AdvertiseHostname(mDNS *m, HostnameInfo *h)
	{
	if (!mDNSIPv4AddressIsZero(m->AdvertisedV4.ip.v4) && h->arv4.resrec.RecordType == kDNSRecordTypeUnregistered)
		{
		mDNS_SetupResourceRecord(&h->arv4, mDNSNULL, mDNSInterface_Any, kDNSType_A, kHostNameTTL, kDNSRecordTypeUnregistered, HostnameCallback, h);
		AssignDomainName(&h->arv4.namestorage, &h->fqdn);
		h->arv4.resrec.rdata->u.ipv4 = m->AdvertisedV4.ip.v4;
		h->arv4.state = regState_Unregistered;
		if (mDNSv4AddrIsRFC1918(&m->AdvertisedV4.ip.v4))
			{
			// If we already have a NAT query active, stop it and restart it to make sure we get another callback
			if (h->natinfo.clientContext) mDNS_StopNATOperation_internal(m, &h->natinfo);
			h->natinfo.Protocol         = 0;
			h->natinfo.IntPort          = zeroIPPort;
			h->natinfo.RequestedPort    = zeroIPPort;
			h->natinfo.NATLease         = 0;
			h->natinfo.clientCallback   = hostnameGetPublicAddressCallback;
			h->natinfo.clientContext    = h;
			mDNS_StartNATOperation_internal(m, &h->natinfo);
			}
		else
			{
			LogInfo("Advertising hostname %##s IPv4 %.4a", h->arv4.resrec.name->c, &m->AdvertisedV4.ip.v4);
			h->arv4.resrec.RecordType = kDNSRecordTypeKnownUnique;
			mDNS_Register_internal(m, &h->arv4);
			}
		}

	if (!mDNSIPv6AddressIsZero(m->AdvertisedV6.ip.v6) && h->arv6.resrec.RecordType == kDNSRecordTypeUnregistered)
		{
		mDNS_SetupResourceRecord(&h->arv6, mDNSNULL, mDNSInterface_Any, kDNSType_AAAA, kHostNameTTL, kDNSRecordTypeKnownUnique, HostnameCallback, h);
		AssignDomainName(&h->arv6.namestorage, &h->fqdn);
		h->arv6.resrec.rdata->u.ipv6 = m->AdvertisedV6.ip.v6;
		h->arv6.state = regState_Unregistered;
		LogInfo("Advertising hostname %##s IPv6 %.16a", h->arv6.resrec.name->c, &m->AdvertisedV6.ip.v6);
		mDNS_Register_internal(m, &h->arv6);
		}
	}

mDNSlocal void HostnameCallback(mDNS *const m, AuthRecord *const rr, mStatus result)
	{
	HostnameInfo *hi = (HostnameInfo *)rr->RecordContext;

	if (result == mStatus_MemFree)
		{
		if (hi)
			{
			// If we're still in the Hostnames list, update to new address
			HostnameInfo *i;
			LogInfo("HostnameCallback: Got mStatus_MemFree for %p %p %s", hi, rr, ARDisplayString(m, rr));
			for (i = m->Hostnames; i; i = i->next)
				if (rr == &i->arv4 || rr == &i->arv6)
					{ mDNS_Lock(m); AdvertiseHostname(m, i); mDNS_Unlock(m); return; }

			// Else, we're not still in the Hostnames list, so free the memory
			if (hi->arv4.resrec.RecordType == kDNSRecordTypeUnregistered &&
				hi->arv6.resrec.RecordType == kDNSRecordTypeUnregistered)
				{
				if (hi->natinfo.clientContext) mDNS_StopNATOperation_internal(m, &hi->natinfo);
				hi->natinfo.clientContext = mDNSNULL;
				mDNSPlatformMemFree(hi);	// free hi when both v4 and v6 AuthRecs deallocated
				}
			}
		return;
		}

	if (result)
		{
		// don't unlink or free - we can retry when we get a new address/router
		if (rr->resrec.rrtype == kDNSType_A)
			LogMsg("HostnameCallback: Error %d for registration of %##s IP %.4a", result, rr->resrec.name->c, &rr->resrec.rdata->u.ipv4);
		else
			LogMsg("HostnameCallback: Error %d for registration of %##s IP %.16a", result, rr->resrec.name->c, &rr->resrec.rdata->u.ipv6);
		if (!hi) { mDNSPlatformMemFree(rr); return; }
		if (rr->state != regState_Unregistered) LogMsg("Error: HostnameCallback invoked with error code for record not in regState_Unregistered!");

		if (hi->arv4.state == regState_Unregistered &&
			hi->arv6.state == regState_Unregistered)
			{
			// only deliver status if both v4 and v6 fail
			rr->RecordContext = (void *)hi->StatusContext;
			if (hi->StatusCallback)
				hi->StatusCallback(m, rr, result); // client may NOT make API calls here
			rr->RecordContext = (void *)hi;
			}
		return;
		}

	// register any pending services that require a target
	mDNS_Lock(m);
	UpdateSRVRecords(m);
	mDNS_Unlock(m);

	// Deliver success to client
	if (!hi) { LogMsg("HostnameCallback invoked with orphaned address record"); return; }
	if (rr->resrec.rrtype == kDNSType_A)
		LogInfo("Registered hostname %##s IP %.4a", rr->resrec.name->c, &rr->resrec.rdata->u.ipv4);
	else
		LogInfo("Registered hostname %##s IP %.16a", rr->resrec.name->c, &rr->resrec.rdata->u.ipv6);

	rr->RecordContext = (void *)hi->StatusContext;
	if (hi->StatusCallback)
		hi->StatusCallback(m, rr, result); // client may NOT make API calls here
	rr->RecordContext = (void *)hi;
	}

mDNSlocal void FoundStaticHostname(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
	{
	const domainname *pktname = &answer->rdata->u.name;
	domainname *storedname = &m->StaticHostname;
	HostnameInfo *h = m->Hostnames;

	(void)question;

	debugf("FoundStaticHostname: %##s -> %##s (%s)", question->qname.c, answer->rdata->u.name.c, AddRecord ? "added" : "removed");
	if (AddRecord && !SameDomainName(pktname, storedname))
		{
		AssignDomainName(storedname, pktname);
		while (h)
			{
			if (h->arv4.state == regState_FetchingZoneData || h->arv4.state == regState_Pending || h->arv4.state == regState_NATMap ||
				h->arv6.state == regState_FetchingZoneData || h->arv6.state == regState_Pending)
				{
				// if we're in the process of registering a dynamic hostname, delay SRV update so we don't have to reregister services if the dynamic name succeeds
				m->NextSRVUpdate = NonZeroTime(m->timenow + 5 * mDNSPlatformOneSecond);
				return;
				}
			h = h->next;
			}
		mDNS_Lock(m);
		UpdateSRVRecords(m);
		mDNS_Unlock(m);
		}
	else if (!AddRecord && SameDomainName(pktname, storedname))
		{
		mDNS_Lock(m);
		storedname->c[0] = 0;
		UpdateSRVRecords(m);
		mDNS_Unlock(m);
		}
	}

// Called with lock held
mDNSlocal void GetStaticHostname(mDNS *m)
	{
	char buf[MAX_REVERSE_MAPPING_NAME_V4];
	DNSQuestion *q = &m->ReverseMap;
	mDNSu8 *ip = m->AdvertisedV4.ip.v4.b;
	mStatus err;

	if (m->ReverseMap.ThisQInterval != -1) return; // already running
	if (mDNSIPv4AddressIsZero(m->AdvertisedV4.ip.v4)) return;

	mDNSPlatformMemZero(q, sizeof(*q));
	// Note: This is reverse order compared to a normal dotted-decimal IP address, so we can't use our customary "%.4a" format code
	mDNS_snprintf(buf, sizeof(buf), "%d.%d.%d.%d.in-addr.arpa.", ip[3], ip[2], ip[1], ip[0]);
	if (!MakeDomainNameFromDNSNameString(&q->qname, buf)) { LogMsg("Error: GetStaticHostname - bad name %s", buf); return; }

	q->InterfaceID      = mDNSInterface_Any;
	q->Target           = zeroAddr;
	q->qtype            = kDNSType_PTR;
	q->qclass           = kDNSClass_IN;
	q->LongLived        = mDNSfalse;
	q->ExpectUnique     = mDNSfalse;
	q->ForceMCast       = mDNSfalse;
	q->ReturnIntermed   = mDNStrue;
	q->QuestionCallback = FoundStaticHostname;
	q->QuestionContext  = mDNSNULL;

	LogInfo("GetStaticHostname: %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
	err = mDNS_StartQuery_internal(m, q);
	if (err) LogMsg("Error: GetStaticHostname - StartQuery returned error %d", err);
	}

mDNSexport void mDNS_AddDynDNSHostName(mDNS *m, const domainname *fqdn, mDNSRecordCallback *StatusCallback, const void *StatusContext)
   {
	HostnameInfo **ptr = &m->Hostnames;

	LogInfo("mDNS_AddDynDNSHostName %##s", fqdn);

	while (*ptr && !SameDomainName(fqdn, &(*ptr)->fqdn)) ptr = &(*ptr)->next;
	if (*ptr) { LogMsg("DynDNSHostName %##s already in list", fqdn->c); return; }

	// allocate and format new address record
	*ptr = mDNSPlatformMemAllocate(sizeof(**ptr));
	if (!*ptr) { LogMsg("ERROR: mDNS_AddDynDNSHostName - malloc"); return; }

	mDNSPlatformMemZero(*ptr, sizeof(**ptr));
	AssignDomainName(&(*ptr)->fqdn, fqdn);
	(*ptr)->arv4.state     = regState_Unregistered;
	(*ptr)->arv6.state     = regState_Unregistered;
	(*ptr)->StatusCallback = StatusCallback;
	(*ptr)->StatusContext  = StatusContext;

	AdvertiseHostname(m, *ptr);
	}

mDNSexport void mDNS_RemoveDynDNSHostName(mDNS *m, const domainname *fqdn)
	{
	HostnameInfo **ptr = &m->Hostnames;

	LogInfo("mDNS_RemoveDynDNSHostName %##s", fqdn);

	while (*ptr && !SameDomainName(fqdn, &(*ptr)->fqdn)) ptr = &(*ptr)->next;
	if (!*ptr) LogMsg("mDNS_RemoveDynDNSHostName: no such domainname %##s", fqdn->c);
	else
		{
		HostnameInfo *hi = *ptr;
		// We do it this way because, if we have no active v6 record, the "mDNS_Deregister_internal(m, &hi->arv4);"
		// below could free the memory, and we have to make sure we don't touch hi fields after that.
		mDNSBool f4 = hi->arv4.resrec.RecordType != kDNSRecordTypeUnregistered && hi->arv4.state != regState_Unregistered;
		mDNSBool f6 = hi->arv6.resrec.RecordType != kDNSRecordTypeUnregistered && hi->arv6.state != regState_Unregistered;
		if (f4) LogInfo("mDNS_RemoveDynDNSHostName removing v4 %##s", fqdn);
		if (f6) LogInfo("mDNS_RemoveDynDNSHostName removing v6 %##s", fqdn);
		*ptr = (*ptr)->next; // unlink
		if (f4) mDNS_Deregister_internal(m, &hi->arv4, mDNS_Dereg_normal);
		if (f6) mDNS_Deregister_internal(m, &hi->arv6, mDNS_Dereg_normal);
		// When both deregistrations complete we'll free the memory in the mStatus_MemFree callback
		}
	UpdateSRVRecords(m);
	}

// Currently called without holding the lock
// Maybe we should change that?
mDNSexport void mDNS_SetPrimaryInterfaceInfo(mDNS *m, const mDNSAddr *v4addr, const mDNSAddr *v6addr, const mDNSAddr *router)
	{
	mDNSBool v4Changed, v6Changed, RouterChanged;

	if (m->mDNS_busy != m->mDNS_reentrancy)
		LogMsg("mDNS_SetPrimaryInterfaceInfo: mDNS_busy (%ld) != mDNS_reentrancy (%ld)", m->mDNS_busy, m->mDNS_reentrancy);

	if (v4addr && v4addr->type != mDNSAddrType_IPv4) { LogMsg("mDNS_SetPrimaryInterfaceInfo v4 address - incorrect type.  Discarding. %#a", v4addr); return; }
	if (v6addr && v6addr->type != mDNSAddrType_IPv6) { LogMsg("mDNS_SetPrimaryInterfaceInfo v6 address - incorrect type.  Discarding. %#a", v6addr); return; }
	if (router && router->type != mDNSAddrType_IPv4) { LogMsg("mDNS_SetPrimaryInterfaceInfo passed non-v4 router.  Discarding. %#a",        router); return; }

	mDNS_Lock(m);

	if (v4addr && !mDNSv4AddressIsLinkLocal(&v4addr->ip.v4)) v6addr = mDNSNULL;

	v4Changed     = !mDNSSameIPv4Address(m->AdvertisedV4.ip.v4, v4addr ? v4addr->ip.v4 : zerov4Addr);
	v6Changed     = !mDNSSameIPv6Address(m->AdvertisedV6.ip.v6, v6addr ? v6addr->ip.v6 : zerov6Addr);
	RouterChanged = !mDNSSameIPv4Address(m->Router.ip.v4,       router ? router->ip.v4 : zerov4Addr);

	if (v4addr && (v4Changed || RouterChanged))
		debugf("mDNS_SetPrimaryInterfaceInfo: address changed from %#a to %#a", &m->AdvertisedV4, v4addr);

	if (v4addr) m->AdvertisedV4 = *v4addr; else m->AdvertisedV4.ip.v4 = zerov4Addr;
	if (v6addr) m->AdvertisedV6 = *v6addr; else m->AdvertisedV6.ip.v6 = zerov6Addr;
	if (router) m->Router       = *router; else m->Router      .ip.v4 = zerov4Addr;
	// setting router to zero indicates that nat mappings must be reestablished when router is reset

	if (v4Changed || RouterChanged || v6Changed)
		{
		HostnameInfo *i;
		LogInfo("mDNS_SetPrimaryInterfaceInfo: %s%s%s%#a %#a %#a",
			v4Changed     ? "v4Changed "     : "",
			RouterChanged ? "RouterChanged " : "",
			v6Changed     ? "v6Changed "     : "", v4addr, v6addr, router);

		for (i = m->Hostnames; i; i = i->next)
			{
			LogInfo("mDNS_SetPrimaryInterfaceInfo updating host name registrations for %##s", i->fqdn.c);
	
			if (i->arv4.resrec.RecordType > kDNSRecordTypeDeregistering &&
				!mDNSSameIPv4Address(i->arv4.resrec.rdata->u.ipv4, m->AdvertisedV4.ip.v4))
				{
				LogInfo("mDNS_SetPrimaryInterfaceInfo deregistering %s", ARDisplayString(m, &i->arv4));
				mDNS_Deregister_internal(m, &i->arv4, mDNS_Dereg_normal);
				}
	
			if (i->arv6.resrec.RecordType > kDNSRecordTypeDeregistering &&
				!mDNSSameIPv6Address(i->arv6.resrec.rdata->u.ipv6, m->AdvertisedV6.ip.v6))
				{
				LogInfo("mDNS_SetPrimaryInterfaceInfo deregistering %s", ARDisplayString(m, &i->arv6));
				mDNS_Deregister_internal(m, &i->arv6, mDNS_Dereg_normal);
				}

			// AdvertiseHostname will only register new address records.
			// For records still in the process of deregistering it will ignore them, and let the mStatus_MemFree callback handle them.
			AdvertiseHostname(m, i);
			}

		if (v4Changed || RouterChanged)
			{
			// If we have a non-zero IPv4 address, we should try immediately to see if we have a NAT gateway
			// If we have no IPv4 address, we don't want to be in quite such a hurry to report failures to our clients
			// <rdar://problem/6935929> Sleeping server sometimes briefly disappears over Back to My Mac after it wakes up
			m->ExternalAddress      = zerov4Addr;
			m->retryIntervalGetAddr = NATMAP_INIT_RETRY;
			m->retryGetAddr         = m->timenow + (v4addr ? 0 : mDNSPlatformOneSecond * 5);
			m->NextScheduledNATOp   = m->timenow;
			m->LastNATMapResultCode = NATErr_None;
#ifdef _LEGACY_NAT_TRAVERSAL_
			LNT_ClearState(m);
#endif // _LEGACY_NAT_TRAVERSAL_
			LogInfo("mDNS_SetPrimaryInterfaceInfo:%s%s: retryGetAddr in %d %d",
				v4Changed     ? " v4Changed"     : "",
				RouterChanged ? " RouterChanged" : "",
				m->retryGetAddr - m->timenow, m->timenow);
			}

		if (m->ReverseMap.ThisQInterval != -1) mDNS_StopQuery_internal(m, &m->ReverseMap);
		m->StaticHostname.c[0] = 0;
		
		UpdateSRVRecords(m); // Will call GetStaticHostname if needed
		
#if APPLE_OSX_mDNSResponder
		if (RouterChanged)	uuid_generate(m->asl_uuid);
		UpdateAutoTunnelDomainStatuses(m);
#endif
		}

	mDNS_Unlock(m);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Incoming Message Processing
#endif

mDNSlocal mStatus ParseTSIGError(mDNS *const m, const DNSMessage *const msg, const mDNSu8 *const end, const domainname *const displayname)
	{
	const mDNSu8 *ptr;
	mStatus err = mStatus_NoError;
	int i;

	ptr = LocateAdditionals(msg, end);
	if (!ptr) goto finish;

	for (i = 0; i < msg->h.numAdditionals; i++)
		{
		ptr = GetLargeResourceRecord(m, msg, ptr, end, 0, kDNSRecordTypePacketAdd, &m->rec);
		if (!ptr) goto finish;
		if (m->rec.r.resrec.rrtype == kDNSType_TSIG)
			{
			mDNSu32 macsize;
			mDNSu8 *rd = m->rec.r.resrec.rdata->u.data;
			mDNSu8 *rdend = rd + m->rec.r.resrec.rdlength;
			int alglen = DomainNameLengthLimit(&m->rec.r.resrec.rdata->u.name, rdend);
			if (alglen > MAX_DOMAIN_NAME) goto finish;
			rd += alglen;                                       // algorithm name
			if (rd + 6 > rdend) goto finish;
			rd += 6;                                            // 48-bit timestamp
			if (rd + sizeof(mDNSOpaque16) > rdend) goto finish;
			rd += sizeof(mDNSOpaque16);                         // fudge
			if (rd + sizeof(mDNSOpaque16) > rdend) goto finish;
			macsize = mDNSVal16(*(mDNSOpaque16 *)rd);
			rd += sizeof(mDNSOpaque16);                         // MAC size
			if (rd + macsize > rdend) goto finish;
			rd += macsize;
			if (rd + sizeof(mDNSOpaque16) > rdend) goto finish;
			rd += sizeof(mDNSOpaque16);                         // orig id
			if (rd + sizeof(mDNSOpaque16) > rdend) goto finish;
			err = mDNSVal16(*(mDNSOpaque16 *)rd);               // error code

			if      (err == TSIG_ErrBadSig)  { LogMsg("%##s: bad signature", displayname->c);              err = mStatus_BadSig;     }
			else if (err == TSIG_ErrBadKey)  { LogMsg("%##s: bad key", displayname->c);                    err = mStatus_BadKey;     }
			else if (err == TSIG_ErrBadTime) { LogMsg("%##s: bad time", displayname->c);                   err = mStatus_BadTime;    }
			else if (err)                    { LogMsg("%##s: unknown tsig error %d", displayname->c, err); err = mStatus_UnknownErr; }
			goto finish;
			}
		m->rec.r.resrec.RecordType = 0;		// Clear RecordType to show we're not still using it
		}

	finish:
	m->rec.r.resrec.RecordType = 0;		// Clear RecordType to show we're not still using it
	return err;
	}

mDNSlocal mStatus checkUpdateResult(mDNS *const m, const domainname *const displayname, const mDNSu8 rcode, const DNSMessage *const msg, const mDNSu8 *const end)
	{
	(void)msg;	// currently unused, needed for TSIG errors
	if (!rcode) return mStatus_NoError;
	else if (rcode == kDNSFlag1_RC_YXDomain)
		{
		debugf("name in use: %##s", displayname->c);
		return mStatus_NameConflict;
		}
	else if (rcode == kDNSFlag1_RC_Refused)
		{
		LogMsg("Update %##s refused", displayname->c);
		return mStatus_Refused;
		}
	else if (rcode == kDNSFlag1_RC_NXRRSet)
		{
		LogMsg("Reregister refused (NXRRSET): %##s", displayname->c);
		return mStatus_NoSuchRecord;
		}
	else if (rcode == kDNSFlag1_RC_NotAuth)
		{
		// TSIG errors should come with FormErr as per RFC 2845, but BIND 9 sends them with NotAuth so we look here too
		mStatus tsigerr = ParseTSIGError(m, msg, end, displayname);
		if (!tsigerr)
			{
			LogMsg("Permission denied (NOAUTH): %##s", displayname->c);
			return mStatus_UnknownErr;
			}
		else return tsigerr;
		}
	else if (rcode == kDNSFlag1_RC_FormErr)
		{
		mStatus tsigerr = ParseTSIGError(m, msg, end, displayname);
		if (!tsigerr)
			{
			LogMsg("Format Error: %##s", displayname->c);
			return mStatus_UnknownErr;
			}
		else return tsigerr;
		}
	else
		{
		LogMsg("Update %##s failed with rcode %d", displayname->c, rcode);
		return mStatus_UnknownErr;
		}
	}

// Called with lock held
mDNSlocal void SendRecordRegistration(mDNS *const m, AuthRecord *rr)
	{
	mDNSu8 *ptr = m->omsg.data;
	mDNSu8 *end = (mDNSu8 *)&m->omsg + sizeof(DNSMessage);
	mStatus err = mStatus_UnknownErr;

	if (m->mDNS_busy != m->mDNS_reentrancy+1)
		LogMsg("SendRecordRegistration: Lock not held! mDNS_busy (%ld) mDNS_reentrancy (%ld)", m->mDNS_busy, m->mDNS_reentrancy);

	if (mDNSIPv4AddressIsZero(rr->UpdateServer.ip.v4))	// Don't know our UpdateServer yet
		{
		rr->LastAPTime = m->timenow;
		if (rr->ThisAPInterval < mDNSPlatformOneSecond * 5)
			rr->ThisAPInterval = mDNSPlatformOneSecond * 5;
		return;
		}

	rr->RequireGoodbye = mDNStrue;
	rr->updateid = mDNS_NewMessageID(m);
	InitializeDNSMessage(&m->omsg.h, rr->updateid, UpdateReqFlags);

	// set zone
	ptr = putZone(&m->omsg, ptr, end, rr->zone, mDNSOpaque16fromIntVal(rr->resrec.rrclass));
	if (!ptr) { err = mStatus_UnknownErr; goto exit; }

	if (rr->state == regState_UpdatePending)
		{
		// delete old RData
		SetNewRData(&rr->resrec, rr->OrigRData, rr->OrigRDLen);
		if (!(ptr = putDeletionRecord(&m->omsg, ptr, &rr->resrec))) { err = mStatus_UnknownErr; goto exit; } // delete old rdata

		// add new RData
		SetNewRData(&rr->resrec, rr->InFlightRData, rr->InFlightRDLen);
		if (!(ptr = PutResourceRecordTTLJumbo(&m->omsg, ptr, &m->omsg.h.mDNS_numUpdates, &rr->resrec, rr->resrec.rroriginalttl))) { err = mStatus_UnknownErr; goto exit; }
		}

	else
		{
		if (rr->resrec.RecordType == kDNSRecordTypeKnownUnique)
			{
			// KnownUnique: Delete any previous value
			ptr = putDeleteRRSet(&m->omsg, ptr, rr->resrec.name, rr->resrec.rrtype);
			if (!ptr) { err = mStatus_UnknownErr; goto exit; }
			}

		else if (rr->resrec.RecordType != kDNSRecordTypeShared)
			{
			// For now don't do this, until we have the logic for intelligent grouping of individual recors into logical service record sets
			//ptr = putPrereqNameNotInUse(rr->resrec.name, &m->omsg, ptr, end);
			if (!ptr) { err = mStatus_UnknownErr; goto exit; }
			}

		ptr = PutResourceRecordTTLJumbo(&m->omsg, ptr, &m->omsg.h.mDNS_numUpdates, &rr->resrec, rr->resrec.rroriginalttl);
		if (!ptr) { err = mStatus_UnknownErr; goto exit; }
		}

	if (rr->uselease)
		{
		ptr = putUpdateLease(&m->omsg, ptr, DEFAULT_UPDATE_LEASE); if (!ptr) { err = mStatus_UnknownErr; goto exit; }
		}

	if (rr->Private)
		{
		LogInfo("SendRecordRegistration TCP %p %s", rr->tcp, ARDisplayString(m, rr));
		if (rr->tcp) LogInfo("SendRecordRegistration: Disposing existing TCP connection for %s", ARDisplayString(m, rr));
		if (rr->tcp) DisposeTCPConn(rr->tcp);
		rr->tcp = MakeTCPConn(m, &m->omsg, ptr, kTCPSocketFlags_UseTLS, &rr->UpdateServer, rr->UpdatePort, mDNSNULL, mDNSNULL, rr);
		if (!rr->tcp) rr->ThisAPInterval = mDNSPlatformOneSecond * 5; // If failed to make TCP connection, try again in ten seconds (5*2)
		else if (rr->ThisAPInterval < mDNSPlatformOneSecond * 30) rr->ThisAPInterval = mDNSPlatformOneSecond * 30;
		}
	else
		{
		err = mDNSSendDNSMessage(m, &m->omsg, ptr, mDNSInterface_Any, mDNSNULL, &rr->UpdateServer, rr->UpdatePort, mDNSNULL, GetAuthInfoForName_internal(m, rr->resrec.name));
		if (err) debugf("ERROR: SendRecordRegistration - mDNSSendDNSMessage - %d", err);
		}

	SetRecordRetry(m, rr, err);

	if (rr->state != regState_Refresh && rr->state != regState_DeregDeferred && rr->state != regState_UpdatePending)
		rr->state = regState_Pending;

	return;

exit:
	LogMsg("SendRecordRegistration: Error formatting message for %s", ARDisplayString(m, rr));
	}

// Called with lock held
mDNSlocal void hndlServiceUpdateReply(mDNS *const m, ServiceRecordSet *srs,  mStatus err)
	{
	mDNSBool InvokeCallback = mDNSfalse;
	ExtraResourceRecord **e = &srs->Extras;
	AuthRecord *txt = &srs->RR_TXT;

	if (m->mDNS_busy != m->mDNS_reentrancy+1)
		LogMsg("hndlServiceUpdateReply: Lock not held! mDNS_busy (%ld) mDNS_reentrancy (%ld)", m->mDNS_busy, m->mDNS_reentrancy);

	debugf("hndlServiceUpdateReply: err %d state %d %##s", err, srs->state, srs->RR_SRV.resrec.name->c);

	SetRecordRetry(m, &srs->RR_SRV, mStatus_NoError);

	switch (srs->state)
		{
		case regState_Pending:
			if (err == mStatus_NameConflict && !srs->TestForSelfConflict)
				{
				srs->TestForSelfConflict = mDNStrue;
				debugf("checking for self-conflict of service %##s", srs->RR_SRV.resrec.name->c);
				SendServiceRegistration(m, srs);
				return;
				}
			else if (srs->TestForSelfConflict)
				{
				srs->TestForSelfConflict = mDNSfalse;
				if (err == mStatus_NoSuchRecord) err = mStatus_NameConflict;	// NoSuchRecord implies that our prereq was not met, so we actually have a name conflict
				if (!err) srs->state = regState_Registered;
				InvokeCallback = mDNStrue;
				break;
				}
			else if (srs->srs_uselease && err == mStatus_UnknownErr && mDNSSameIPPort(srs->SRSUpdatePort, UnicastDNSPort))
				{
				LogMsg("Re-trying update of service %##s without lease option", srs->RR_SRV.resrec.name->c);
				srs->srs_uselease = mDNSfalse;
				SendServiceRegistration(m, srs);
				return;
				}
			else
				{
				//!!!KRS make sure all structs will still get cleaned up when client calls DeregisterService with this state
				if (err) LogMsg("Error %d for registration of service %##s", err, srs->RR_SRV.resrec.name->c);
				else srs->state = regState_Registered;
				InvokeCallback = mDNStrue;
				break;
				}
		case regState_Refresh:
			if (err)
				{
				LogMsg("Error %d for refresh of service %##s", err, srs->RR_SRV.resrec.name->c);
				InvokeCallback = mDNStrue;
				}
			else srs->state = regState_Registered;
			break;
		case regState_DeregPending:
			if (err) LogMsg("Error %d for deregistration of service %##s", err, srs->RR_SRV.resrec.name->c);
			if (srs->SRVChanged)
				{
				srs->state = regState_NoTarget;	// NoTarget will allow us to pick up new target OR nat traversal state
				break;
				}
			err = mStatus_MemFree;
			InvokeCallback = mDNStrue;
			if (srs->NATinfo.clientContext)
				{
				// deletion completed
				mDNS_StopNATOperation_internal(m, &srs->NATinfo);
				srs->NATinfo.clientContext = mDNSNULL;
				}
			srs->state = regState_Unregistered;
			break;
		case regState_DeregDeferred:
			if (err)
				{
				debugf("Error %d received prior to deferred deregistration of %##s", err, srs->RR_SRV.resrec.name->c);
				err = mStatus_MemFree;
				InvokeCallback = mDNStrue;
				srs->state = regState_Unregistered;
				break;
				}
			else
				{
				debugf("Performing deferred deregistration of %##s", srs->RR_SRV.resrec.name->c);
				srs->state = regState_Registered;
				SendServiceDeregistration(m, srs);
				return;
				}
		case regState_UpdatePending:
			if (err)
				{
				LogMsg("hndlServiceUpdateReply: error updating TXT record for service %##s", srs->RR_SRV.resrec.name->c);
				InvokeCallback = mDNStrue;
				}
			else
				{
				srs->state = regState_Registered;
				// deallocate old RData
				if (txt->UpdateCallback) txt->UpdateCallback(m, txt, txt->OrigRData);
				SetNewRData(&txt->resrec, txt->InFlightRData, txt->InFlightRDLen);
				txt->OrigRData = mDNSNULL;
				txt->InFlightRData = mDNSNULL;
				}
			break;
		case regState_NoTarget:
			// This state is used when using SendServiceDeregistration() when going to sleep -- no further action required
			return;
		case regState_FetchingZoneData:
		case regState_Registered:
		case regState_Unregistered:
		case regState_NATMap:
		case regState_ExtraQueued:
		case regState_NATError:
			LogMsg("hndlServiceUpdateReply called for service %##s in unexpected state %d with error %d.  Unlinking.",
				   srs->RR_SRV.resrec.name->c, srs->state, err);
			err = mStatus_UnknownErr;
		default: LogMsg("hndlServiceUpdateReply: Unknown state %d for %##s", srs->state, srs->RR_SRV.resrec.name->c);
		}

	if ((srs->SRVChanged || srs->SRVUpdateDeferred) && (srs->state == regState_NoTarget || srs->state == regState_Registered))
		{
		debugf("hndlServiceUpdateReply: SRVChanged %d SRVUpdateDeferred %d state %d", srs->SRVChanged, srs->SRVUpdateDeferred, srs->state);
		if (InvokeCallback)
			{
			srs->ClientCallbackDeferred = mDNStrue;
			srs->DeferredStatus = err;
			}
		srs->SRVChanged = srs->SRVUpdateDeferred = mDNSfalse;
		UpdateSRV(m, srs);
		return;
		}

	while (*e)
		{
		if ((*e)->r.state == regState_ExtraQueued)
			{
			if (srs->state == regState_Registered && !err)
				{
				// extra resource record queued for this service - copy zone srs and register
				(*e)->r.zone = &srs->zone;
				(*e)->r.UpdateServer    = srs->SRSUpdateServer;
				(*e)->r.UpdatePort  = srs->SRSUpdatePort;
				(*e)->r.uselease = srs->srs_uselease;
				SendRecordRegistration(m, &(*e)->r);
				e = &(*e)->next;
				}
			else if (err && (*e)->r.state != regState_Unregistered)
				{
				// unlink extra from list
				(*e)->r.state = regState_Unregistered;
				*e = (*e)->next;
				}
			else e = &(*e)->next;
			}
		else e = &(*e)->next;
		}

	if (srs->state == regState_Unregistered)
		{
		if (err != mStatus_MemFree)
			LogMsg("hndlServiceUpdateReply ERROR! state == regState_Unregistered but err != mStatus_MemFree. Permanently abandoning service registration %##s",
				srs->RR_SRV.resrec.name->c);
		unlinkSRS(m, srs);
		}
	else if (txt->QueuedRData && srs->state == regState_Registered)
		{
		if (InvokeCallback)
			{
			// if we were supposed to give a client callback, we'll do it after we update the primary txt record
			srs->ClientCallbackDeferred = mDNStrue;
			srs->DeferredStatus = err;
			}
		srs->state = regState_UpdatePending;
		txt->InFlightRData = txt->QueuedRData;
		txt->InFlightRDLen = txt->QueuedRDLen;
		txt->OrigRData = txt->resrec.rdata;
		txt->OrigRDLen = txt->resrec.rdlength;
		txt->QueuedRData = mDNSNULL;
		SendServiceRegistration(m, srs);
		return;
		}

	mDNS_DropLockBeforeCallback();
	if (InvokeCallback)
		srs->ServiceCallback(m, srs, err);
	else if (srs->ClientCallbackDeferred)
		{
		srs->ClientCallbackDeferred = mDNSfalse;
		srs->ServiceCallback(m, srs, srs->DeferredStatus);
		}
	mDNS_ReclaimLockAfterCallback();
	// CAUTION: MUST NOT do anything more with rr after calling rr->Callback(), because the client's callback function
	// is allowed to do anything, including starting/stopping queries, registering/deregistering records, etc.
	}

// Called with lock held
mDNSlocal void hndlRecordUpdateReply(mDNS *m, AuthRecord *rr, mStatus err)
	{
	mDNSBool InvokeCallback = mDNStrue;

	if (m->mDNS_busy != m->mDNS_reentrancy+1)
		LogMsg("hndlRecordUpdateReply: Lock not held! mDNS_busy (%ld) mDNS_reentrancy (%ld)", m->mDNS_busy, m->mDNS_reentrancy);

	LogInfo("hndlRecordUpdateReply: err %d state %d %s", err, rr->state, ARDisplayString(m, rr));

	if (m->SleepState) return;		// If we just sent a deregister on going to sleep, no further action required

	SetRecordRetry(m, rr, mStatus_NoError);

	if (rr->state == regState_UpdatePending)
		{
		if (err) LogMsg("Update record failed for %##s (err %d)", rr->resrec.name->c, err);
		rr->state = regState_Registered;
		// deallocate old RData
		if (rr->UpdateCallback) rr->UpdateCallback(m, rr, rr->OrigRData);
		SetNewRData(&rr->resrec, rr->InFlightRData, rr->InFlightRDLen);
		rr->OrigRData = mDNSNULL;
		rr->InFlightRData = mDNSNULL;
		}

	if (rr->state == regState_DeregPending)
		{
		debugf("Received reply for deregister record %##s type %d", rr->resrec.name->c, rr->resrec.rrtype);
		if (err) LogMsg("ERROR: Deregistration of record %##s type %d failed with error %d",
						rr->resrec.name->c, rr->resrec.rrtype, err);
		err = mStatus_MemFree;
		rr->state = regState_Unregistered;
		}

	if (rr->state == regState_DeregDeferred)
		{
		if (err)
			{
			LogMsg("Cancelling deferred deregistration record %##s type %d due to registration error %d",
				   rr->resrec.name->c, rr->resrec.rrtype, err);
			rr->state = regState_Unregistered;
			}
		debugf("Calling deferred deregistration of record %##s type %d",  rr->resrec.name->c, rr->resrec.rrtype);
		rr->state = regState_Registered;
		mDNS_Deregister_internal(m, rr, mDNS_Dereg_normal);
		return;
		}

	if (rr->state == regState_Pending || rr->state == regState_Refresh)
		{
		if (!err)
			{
			if (rr->state == regState_Refresh) InvokeCallback = mDNSfalse;
			rr->state = regState_Registered;
			}
		else
			{
			if (rr->uselease && err == mStatus_UnknownErr && mDNSSameIPPort(rr->UpdatePort, UnicastDNSPort))
				{
				LogMsg("Re-trying update of record %##s without lease option", rr->resrec.name->c);
				rr->uselease = mDNSfalse;
				SendRecordRegistration(m, rr);
				return;
				}
			LogMsg("hndlRecordUpdateReply: Registration of record %##s type %d failed with error %d", rr->resrec.name->c, rr->resrec.rrtype, err);
			return;
			}
		}

	if (rr->state == regState_Unregistered)		// Should never happen
		{
		LogMsg("hndlRecordUpdateReply rr->state == regState_Unregistered %s", ARDisplayString(m, rr));
		return;
		}

	if (rr->QueuedRData && rr->state == regState_Registered)
		{
		rr->state = regState_UpdatePending;
		rr->InFlightRData = rr->QueuedRData;
		rr->InFlightRDLen = rr->QueuedRDLen;
		rr->OrigRData = rr->resrec.rdata;
		rr->OrigRDLen = rr->resrec.rdlength;
		rr->QueuedRData = mDNSNULL;
		SendRecordRegistration(m, rr);
		return;
		}

	if (InvokeCallback && rr->RecordCallback)
		{
		mDNS_DropLockBeforeCallback();
		rr->RecordCallback(m, rr, err);
		mDNS_ReclaimLockAfterCallback();
		}
	// CAUTION: MUST NOT do anything more with rr after calling rr->Callback(), because the client's callback function
	// is allowed to do anything, including starting/stopping queries, registering/deregistering records, etc.
	}

mDNSexport void uDNS_ReceiveNATPMPPacket(mDNS *m, const mDNSInterfaceID InterfaceID, mDNSu8 *pkt, mDNSu16 len)
	{
	NATTraversalInfo *ptr;
	NATAddrReply     *AddrReply    = (NATAddrReply    *)pkt;
	NATPortMapReply  *PortMapReply = (NATPortMapReply *)pkt;
	mDNSu32 nat_elapsed, our_elapsed;

	// Minimum packet is vers (1) opcode (1) err (2) upseconds (4) = 8 bytes
	if (!AddrReply->err && len < 8) { LogMsg("NAT Traversal message too short (%d bytes)", len); return; }
	if (AddrReply->vers != NATMAP_VERS) { LogMsg("Received NAT Traversal response with version %d (expected %d)", pkt[0], NATMAP_VERS); return; }

	// Read multi-byte numeric values (fields are identical in a NATPortMapReply)
	AddrReply->err       = (mDNSu16) (                                                (mDNSu16)pkt[2] << 8 | pkt[3]);
	AddrReply->upseconds = (mDNSs32) ((mDNSs32)pkt[4] << 24 | (mDNSs32)pkt[5] << 16 | (mDNSs32)pkt[6] << 8 | pkt[7]);

	nat_elapsed = AddrReply->upseconds - m->LastNATupseconds;
	our_elapsed = (m->timenow - m->LastNATReplyLocalTime) / mDNSPlatformOneSecond;
	debugf("uDNS_ReceiveNATPMPPacket %X upseconds %u nat_elapsed %d our_elapsed %d", AddrReply->opcode, AddrReply->upseconds, nat_elapsed, our_elapsed);

	// We compute a conservative estimate of how much the NAT gateways's clock should have advanced
	// 1. We subtract 12.5% from our own measured elapsed time, to allow for NAT gateways that have an inacurate clock that runs slowly
	// 2. We add a two-second safety margin to allow for rounding errors:
	//    -- e.g. if NAT gateway sends a packet at t=2.00 seconds, then one at t=7.99, that's virtually 6 seconds,
	//       but based on the values in the packet (2,7) the apparent difference is only 5 seconds
	//    -- similarly, if we're slow handling packets and/or we have coarse clock granularity, we could over-estimate the true interval
	//       (e.g. t=1.99 seconds rounded to 1, and t=8.01 rounded to 8, gives an apparent difference of 7 seconds)
	if (AddrReply->upseconds < m->LastNATupseconds || nat_elapsed + 2 < our_elapsed - our_elapsed/8)
		{ LogMsg("NAT gateway %#a rebooted", &m->Router); RecreateNATMappings(m); }

	m->LastNATupseconds      = AddrReply->upseconds;
	m->LastNATReplyLocalTime = m->timenow;
#ifdef _LEGACY_NAT_TRAVERSAL_
	LNT_ClearState(m);
#endif // _LEGACY_NAT_TRAVERSAL_

	if (AddrReply->opcode == NATOp_AddrResponse)
		{
#if APPLE_OSX_mDNSResponder
		static char msgbuf[16];
		mDNS_snprintf(msgbuf, sizeof(msgbuf), "%d", AddrReply->err);
		mDNSASLLog((uuid_t *)&m->asl_uuid, "natt.natpmp.AddressRequest", AddrReply->err ? "failure" : "success", msgbuf, "");
#endif
		if (!AddrReply->err && len < sizeof(NATAddrReply)) { LogMsg("NAT Traversal AddrResponse message too short (%d bytes)", len); return; }
		natTraversalHandleAddressReply(m, AddrReply->err, AddrReply->ExtAddr);
		}
	else if (AddrReply->opcode == NATOp_MapUDPResponse || AddrReply->opcode == NATOp_MapTCPResponse)
		{
		mDNSu8 Protocol = AddrReply->opcode & 0x7F;
#if APPLE_OSX_mDNSResponder
		static char msgbuf[16];
		mDNS_snprintf(msgbuf, sizeof(msgbuf), "%s - %d", AddrReply->opcode == NATOp_MapUDPResponse ? "UDP" : "TCP", PortMapReply->err);
		mDNSASLLog((uuid_t *)&m->asl_uuid, "natt.natpmp.PortMapRequest", PortMapReply->err ? "failure" : "success", msgbuf, "");
#endif
		if (!PortMapReply->err)
			{
			if (len < sizeof(NATPortMapReply)) { LogMsg("NAT Traversal PortMapReply message too short (%d bytes)", len); return; }
			PortMapReply->NATRep_lease = (mDNSu32) ((mDNSu32)pkt[12] << 24 | (mDNSu32)pkt[13] << 16 | (mDNSu32)pkt[14] << 8 | pkt[15]);
			}

		// Since some NAT-PMP server implementations don't return the requested internal port in
		// the reply, we can't associate this reply with a particular NATTraversalInfo structure.
		// We globally keep track of the most recent error code for mappings.
		m->LastNATMapResultCode = PortMapReply->err;
		
		for (ptr = m->NATTraversals; ptr; ptr=ptr->next)
			if (ptr->Protocol == Protocol && mDNSSameIPPort(ptr->IntPort, PortMapReply->intport))
				natTraversalHandlePortMapReply(m, ptr, InterfaceID, PortMapReply->err, PortMapReply->extport, PortMapReply->NATRep_lease);
		}
	else { LogMsg("Received NAT Traversal response with version unknown opcode 0x%X", AddrReply->opcode); return; }

	// Don't need an SSDP socket if we get a NAT-PMP packet
	if (m->SSDPSocket) { debugf("uDNS_ReceiveNATPMPPacket destroying SSDPSocket %p", &m->SSDPSocket); mDNSPlatformUDPClose(m->SSDPSocket); m->SSDPSocket = mDNSNULL; }
	}

// <rdar://problem/3925163> Shorten DNS-SD queries to avoid NAT bugs
// <rdar://problem/4288449> Add check to avoid crashing NAT gateways that have buggy DNS relay code
//
// We know of bugs in home NAT gateways that cause them to crash if they receive certain DNS queries.
// The DNS queries that make them crash are perfectly legal DNS queries, but even if they weren't,
// the gateway shouldn't crash -- in today's world of viruses and network attacks, software has to
// be written assuming that a malicious attacker could send them any packet, properly-formed or not.
// Still, we don't want to be crashing people's home gateways, so we go out of our way to avoid
// the queries that crash them.
//
// Some examples:
//
// 1. Any query where the name ends in ".in-addr.arpa." and the text before this is 32 or more bytes.
//    The query type does not need to be PTR -- the gateway will crash for any query type.
//    e.g. "ping long-name-crashes-the-buggy-router.in-addr.arpa" will crash one of these.
//
// 2. Any query that results in a large response with the TC bit set.
//
// 3. Any PTR query that doesn't begin with four decimal numbers.
//    These gateways appear to assume that the only possible PTR query is a reverse-mapping query
//    (e.g. "1.0.168.192.in-addr.arpa") and if they ever get a PTR query where the first four
//    labels are not all decimal numbers in the range 0-255, they handle that by crashing.
//    These gateways also ignore the remainder of the name following the four decimal numbers
//    -- whether or not it actually says in-addr.arpa, they just make up an answer anyway.
//
// The challenge therefore is to craft a query that will discern whether the DNS server
// is one of these buggy ones, without crashing it. Furthermore we don't want our test
// queries making it all the way to the root name servers, putting extra load on those
// name servers and giving Apple a bad reputation. To this end we send this query:
//     dig -t ptr 1.0.0.127.dnsbugtest.1.0.0.127.in-addr.arpa.
//
// The text preceding the ".in-addr.arpa." is under 32 bytes, so it won't cause crash (1).
// It will not yield a large response with the TC bit set, so it won't cause crash (2).
// It starts with four decimal numbers, so it won't cause crash (3).
// The name falls within the "1.0.0.127.in-addr.arpa." domain, the reverse-mapping name for the local
// loopback address, and therefore the query will black-hole at the first properly-configured DNS server
// it reaches, making it highly unlikely that this query will make it all the way to the root.
//
// Finally, the correct response to this query is NXDOMAIN or a similar error, but the
// gateways that ignore the remainder of the name following the four decimal numbers
// give themselves away by actually returning a result for this nonsense query.

mDNSlocal const domainname *DNSRelayTestQuestion = (const domainname*)
	"\x1" "1" "\x1" "0" "\x1" "0" "\x3" "127" "\xa" "dnsbugtest"
	"\x1" "1" "\x1" "0" "\x1" "0" "\x3" "127" "\x7" "in-addr" "\x4" "arpa";

// See comments above for DNSRelayTestQuestion
// If this is the kind of query that has the risk of crashing buggy DNS servers, we do a test question first
mDNSlocal mDNSBool NoTestQuery(DNSQuestion *q)
	{
	int i;
	mDNSu8 *p = q->qname.c;
	if (q->AuthInfo) return(mDNStrue);		// Don't need a test query for private queries sent directly to authoritative server over TLS/TCP
	if (q->qtype != kDNSType_PTR) return(mDNStrue);		// Don't need a test query for any non-PTR queries
	for (i=0; i<4; i++)		// If qname does not begin with num.num.num.num, can't skip the test query
		{
		if (p[0] < 1 || p[0] > 3) return(mDNSfalse);
		if (              p[1] < '0' || p[1] > '9' ) return(mDNSfalse);
		if (p[0] >= 2 && (p[2] < '0' || p[2] > '9')) return(mDNSfalse);
		if (p[0] >= 3 && (p[3] < '0' || p[3] > '9')) return(mDNSfalse);
		p += 1 + p[0];
		}
	// If remainder of qname is ".in-addr.arpa.", this is a vanilla reverse-mapping query and
	// we can safely do it without needing a test query first, otherwise we need the test query.
	return(SameDomainName((domainname*)p, (const domainname*)"\x7" "in-addr" "\x4" "arpa"));
	}

// Returns mDNStrue if response was handled
mDNSlocal mDNSBool uDNS_ReceiveTestQuestionResponse(mDNS *const m, DNSMessage *const msg, const mDNSu8 *const end,
	const mDNSAddr *const srcaddr, const mDNSIPPort srcport)
	{
	const mDNSu8 *ptr = msg->data;
	DNSQuestion pktq;
	DNSServer *s;
	mDNSu32 result = 0;

	// 1. Find out if this is an answer to one of our test questions
	if (msg->h.numQuestions != 1) return(mDNSfalse);
	ptr = getQuestion(msg, ptr, end, mDNSInterface_Any, &pktq);
	if (!ptr) return(mDNSfalse);
	if (pktq.qtype != kDNSType_PTR || pktq.qclass != kDNSClass_IN) return(mDNSfalse);
	if (!SameDomainName(&pktq.qname, DNSRelayTestQuestion)) return(mDNSfalse);

	// 2. If the DNS relay gave us a positive response, then it's got buggy firmware
	// else, if the DNS relay gave us an error or no-answer response, it passed our test
	if ((msg->h.flags.b[1] & kDNSFlag1_RC_Mask) == kDNSFlag1_RC_NoErr && msg->h.numAnswers > 0)
		result = DNSServer_Failed;
	else
		result = DNSServer_Passed;

	// 3. Find occurrences of this server in our list, and mark them appropriately
	for (s = m->DNSServers; s; s = s->next)
		{
		mDNSBool matchaddr = (s->teststate != result && mDNSSameAddress(srcaddr, &s->addr) && mDNSSameIPPort(srcport, s->port));
		mDNSBool matchid   = (s->teststate == DNSServer_Untested && mDNSSameOpaque16(msg->h.id, s->testid));
		if (matchaddr || matchid)
			{
			DNSQuestion *q;
			s->teststate = result;
			if (result == DNSServer_Passed)
				{
				LogInfo("DNS Server %#a:%d (%#a:%d) %d passed%s",
					&s->addr, mDNSVal16(s->port), srcaddr, mDNSVal16(srcport), mDNSVal16(s->testid),
					matchaddr ? "" : " NOTE: Reply did not come from address to which query was sent");
				}
			else
				{
				LogMsg("NOTE: Wide-Area Service Discovery disabled to avoid crashing defective DNS relay %#a:%d (%#a:%d) %d%s",
					&s->addr, mDNSVal16(s->port), srcaddr, mDNSVal16(srcport), mDNSVal16(s->testid),
					matchaddr ? "" : " NOTE: Reply did not come from address to which query was sent");
				}

			// If this server has just changed state from DNSServer_Untested to DNSServer_Passed, then retrigger any waiting questions.
			// We use the NoTestQuery() test so that we only retrigger questions that were actually blocked waiting for this test to complete.
			if (result == DNSServer_Passed)		// Unblock any questions that were waiting for this result
				for (q = m->Questions; q; q=q->next)
					if (q->qDNSServer == s && !NoTestQuery(q))
						{
						q->ThisQInterval = INIT_UCAST_POLL_INTERVAL / QuestionIntervalStep;
						q->unansweredQueries = 0;
						q->LastQTime = m->timenow - q->ThisQInterval;
						m->NextScheduledQuery = m->timenow;
						}
			}
		}

	return(mDNStrue); // Return mDNStrue to tell uDNS_ReceiveMsg it doesn't need to process this packet further
	}

// Called from mDNSCoreReceive with the lock held
mDNSexport void uDNS_ReceiveMsg(mDNS *const m, DNSMessage *const msg, const mDNSu8 *const end, const mDNSAddr *const srcaddr, const mDNSIPPort srcport)
	{
	DNSQuestion *qptr;
	mStatus err = mStatus_NoError;

	mDNSu8 StdR    = kDNSFlag0_QR_Response | kDNSFlag0_OP_StdQuery;
	mDNSu8 UpdateR = kDNSFlag0_QR_Response | kDNSFlag0_OP_Update;
	mDNSu8 QR_OP   = (mDNSu8)(msg->h.flags.b[0] & kDNSFlag0_QROP_Mask);
	mDNSu8 rcode   = (mDNSu8)(msg->h.flags.b[1] & kDNSFlag1_RC_Mask);

	(void)srcport; // Unused

	debugf("uDNS_ReceiveMsg from %#-15a with "
		"%2d Question%s %2d Answer%s %2d Authorit%s %2d Additional%s",
		srcaddr,
		msg->h.numQuestions,   msg->h.numQuestions   == 1 ? ", " : "s,",
		msg->h.numAnswers,     msg->h.numAnswers     == 1 ? ", " : "s,",
		msg->h.numAuthorities, msg->h.numAuthorities == 1 ? "y,  " : "ies,",
		msg->h.numAdditionals, msg->h.numAdditionals == 1 ? "" : "s");

	if (QR_OP == StdR)
		{
		//if (srcaddr && recvLLQResponse(m, msg, end, srcaddr, srcport)) return;
		if (uDNS_ReceiveTestQuestionResponse(m, msg, end, srcaddr, srcport)) return;
		for (qptr = m->Questions; qptr; qptr = qptr->next)
			if (msg->h.flags.b[0] & kDNSFlag0_TC && mDNSSameOpaque16(qptr->TargetQID, msg->h.id) && m->timenow - qptr->LastQTime < RESPONSE_WINDOW)
				{
				if (!srcaddr) LogMsg("uDNS_ReceiveMsg: TCP DNS response had TC bit set: ignoring");
				else if (qptr->tcp)
					{
					// There may be a race condition here, if the server decides to drop the connection just as we decide to reuse it
					// For now it should not be serious because our normal retry logic (as used to handle UDP packet loss)
					// should take care of it but later we may want to look at handling this case explicitly
					LogInfo("uDNS_ReceiveMsg: Using existing TCP connection for %##s (%s)", qptr->qname.c, DNSTypeName(qptr->qtype));
					mDNS_DropLockBeforeCallback();
					tcpCallback(qptr->tcp->sock, qptr->tcp, mDNStrue, mStatus_NoError);
					mDNS_ReclaimLockAfterCallback();
					}
				else qptr->tcp = MakeTCPConn(m, mDNSNULL, mDNSNULL, kTCPSocketFlags_Zero, srcaddr, srcport, qptr, mDNSNULL, mDNSNULL);
				}
		}

	if (QR_OP == UpdateR)
		{
		mDNSu32 lease = GetPktLease(m, msg, end);
		mDNSs32 expire = m->timenow + (mDNSs32)lease * mDNSPlatformOneSecond;

		//rcode = kDNSFlag1_RC_ServFail;	// Simulate server failure (rcode 2)

		if (CurrentServiceRecordSet)
			LogMsg("uDNS_ReceiveMsg ERROR CurrentServiceRecordSet already set");
		CurrentServiceRecordSet = m->ServiceRegistrations;

		while (CurrentServiceRecordSet)
			{
			ServiceRecordSet *sptr = CurrentServiceRecordSet;
			CurrentServiceRecordSet = CurrentServiceRecordSet->uDNS_next;

			if (mDNSSameOpaque16(sptr->id, msg->h.id))
				{
				err = checkUpdateResult(m, sptr->RR_SRV.resrec.name, rcode, msg, end);
				if (!err && sptr->srs_uselease && lease)
					if (sptr->RR_SRV.expire - expire >= 0 || sptr->state != regState_UpdatePending)
						sptr->RR_SRV.expire = expire;
				hndlServiceUpdateReply(m, sptr, err);
				CurrentServiceRecordSet = mDNSNULL;
				return;
				}
			}

		if (m->CurrentRecord)
			LogMsg("uDNS_ReceiveMsg ERROR m->CurrentRecord already set %s", ARDisplayString(m, m->CurrentRecord));
		m->CurrentRecord = m->ResourceRecords;
		while (m->CurrentRecord)
			{
			AuthRecord *rptr = m->CurrentRecord;
			m->CurrentRecord = m->CurrentRecord->next;
			if (AuthRecord_uDNS(rptr) && mDNSSameOpaque16(rptr->updateid, msg->h.id))
				{
				err = checkUpdateResult(m, rptr->resrec.name, rcode, msg, end);
				if (!err && rptr->uselease && lease)
					if (rptr->expire - expire >= 0 || rptr->state != regState_UpdatePending)
						rptr->expire = expire;
				hndlRecordUpdateReply(m, rptr, err);
				m->CurrentRecord = mDNSNULL;
				return;
				}
			}
		}
	debugf("Received unexpected response: ID %d matches no active records", mDNSVal16(msg->h.id));
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Query Routines
#endif

mDNSexport void sendLLQRefresh(mDNS *m, DNSQuestion *q)
	{
	mDNSu8 *end;
	LLQOptData llq;

	if (q->ReqLease)
		if ((q->state == LLQ_Established && q->ntries >= kLLQ_MAX_TRIES) || q->expire - m->timenow < 0)
			{
			LogMsg("Unable to refresh LLQ %##s (%s) - will retry in %d seconds", q->qname.c, DNSTypeName(q->qtype), LLQ_POLL_INTERVAL / mDNSPlatformOneSecond);
			StartLLQPolling(m,q);
			return;
			}

	llq.vers     = kLLQ_Vers;
	llq.llqOp    = kLLQOp_Refresh;
	llq.err      = q->tcp ? GetLLQEventPort(m, &q->servAddr) : LLQErr_NoError;	// If using TCP tell server what UDP port to send notifications to
	llq.id       = q->id;
	llq.llqlease = q->ReqLease;

	InitializeDNSMessage(&m->omsg.h, q->TargetQID, uQueryFlags);
	end = putLLQ(&m->omsg, m->omsg.data, q, &llq);
	if (!end) { LogMsg("sendLLQRefresh: putLLQ failed %##s (%s)", q->qname.c, DNSTypeName(q->qtype)); return; }

	// Note that we (conditionally) add HINFO and TSIG here, since the question might be going away,
	// so we may not be able to reference it (most importantly it's AuthInfo) when we actually send the message
	end = putHINFO(m, &m->omsg, end, q->AuthInfo);
	if (!end) { LogMsg("sendLLQRefresh: putHINFO failed %##s (%s)", q->qname.c, DNSTypeName(q->qtype)); return; }

	if (q->AuthInfo)
		{
		DNSDigest_SignMessageHostByteOrder(&m->omsg, &end, q->AuthInfo);
		if (!end) { LogMsg("sendLLQRefresh: DNSDigest_SignMessage failed %##s (%s)", q->qname.c, DNSTypeName(q->qtype)); return; }
		}

	if (q->AuthInfo && !q->tcp)
		{
		LogInfo("sendLLQRefresh setting up new TLS session %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
		q->tcp = MakeTCPConn(m, &m->omsg, end, kTCPSocketFlags_UseTLS, &q->servAddr, q->servPort, q, mDNSNULL, mDNSNULL);
		}
	else
		{
		mStatus err = mDNSSendDNSMessage(m, &m->omsg, end, mDNSInterface_Any, q->LocalSocket, &q->servAddr, q->servPort, q->tcp ? q->tcp->sock : mDNSNULL, mDNSNULL);
		if (err)
			{
			LogMsg("sendLLQRefresh: mDNSSendDNSMessage%s failed: %d", q->tcp ? " (TCP)" : "", err);
			if (q->tcp) { DisposeTCPConn(q->tcp); q->tcp = mDNSNULL; }
			}
		}

	q->ntries++;

	debugf("sendLLQRefresh ntries %d %##s (%s)", q->ntries, q->qname.c, DNSTypeName(q->qtype));

	q->LastQTime = m->timenow;
	SetNextQueryTime(m, q);
	}

mDNSexport void LLQGotZoneData(mDNS *const m, mStatus err, const ZoneData *zoneInfo)
	{
	DNSQuestion *q = (DNSQuestion *)zoneInfo->ZoneDataContext;

	mDNS_Lock(m);

	// If we get here it means that the GetZoneData operation has completed, and is is about to cancel
	// its question and free the ZoneData memory. We no longer need to hold onto our pointer (which
	// we use for cleaning up if our LLQ is cancelled *before* the GetZoneData operation has completes).
	q->nta      = mDNSNULL;
	q->servAddr = zeroAddr;
	q->servPort = zeroIPPort;

	if (!err && zoneInfo && !mDNSIPPortIsZero(zoneInfo->Port) && !mDNSAddressIsZero(&zoneInfo->Addr))
		{
		q->servAddr = zoneInfo->Addr;
		q->servPort = zoneInfo->Port;
		q->AuthInfo = zoneInfo->ZonePrivate ? GetAuthInfoForName_internal(m, &q->qname) : mDNSNULL;
		q->ntries = 0;
		debugf("LLQGotZoneData %#a:%d", &q->servAddr, mDNSVal16(q->servPort));
		startLLQHandshake(m, q);
		}
	else
		{
		StartLLQPolling(m,q);
		if (err == mStatus_NoSuchNameErr) 
			{
			// this actually failed, so mark it by setting address to all ones 
			q->servAddr.type = mDNSAddrType_IPv4; 
			q->servAddr.ip.v4 = onesIPv4Addr; 
			}
		}

	mDNS_Unlock(m);
	}

// Called in normal callback context (i.e. mDNS_busy and mDNS_reentrancy are both 1)
mDNSlocal void PrivateQueryGotZoneData(mDNS *const m, mStatus err, const ZoneData *zoneInfo)
	{
	DNSQuestion *q = (DNSQuestion *) zoneInfo->ZoneDataContext;

	LogInfo("PrivateQueryGotZoneData %##s (%s) err %d Zone %##s Private %d", q->qname.c, DNSTypeName(q->qtype), err, zoneInfo->ZoneName.c, zoneInfo->ZonePrivate);

	// If we get here it means that the GetZoneData operation has completed, and is is about to cancel
	// its question and free the ZoneData memory. We no longer need to hold onto our pointer (which
	// we use for cleaning up if our LLQ is cancelled *before* the GetZoneData operation has completes).
	q->nta = mDNSNULL;

	if (err || !zoneInfo || mDNSAddressIsZero(&zoneInfo->Addr) || mDNSIPPortIsZero(zoneInfo->Port))
		{
		LogInfo("ERROR: PrivateQueryGotZoneData %##s (%s) invoked with error code %d %p %#a:%d",
			q->qname.c, DNSTypeName(q->qtype), err, zoneInfo,
			zoneInfo ? &zoneInfo->Addr : mDNSNULL,
			zoneInfo ? mDNSVal16(zoneInfo->Port) : 0);
		return;
		}

	if (!zoneInfo->ZonePrivate)
		{
		debugf("Private port lookup failed -- retrying without TLS -- %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
		q->AuthInfo      = mDNSNULL;		// Clear AuthInfo so we try again non-private
		q->ThisQInterval = InitialQuestionInterval;
		q->LastQTime     = m->timenow - q->ThisQInterval;
		mDNS_Lock(m);
		SetNextQueryTime(m, q);
		mDNS_Unlock(m);
		return;
		// Next call to uDNS_CheckCurrentQuestion() will do this as a non-private query
		}

	if (!q->AuthInfo)
		{
		LogMsg("ERROR: PrivateQueryGotZoneData: cannot find credentials for q %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
		return;
		}

	q->TargetQID = mDNS_NewMessageID(m);
	if (q->tcp) DisposeTCPConn(q->tcp);
	q->tcp = MakeTCPConn(m, mDNSNULL, mDNSNULL, kTCPSocketFlags_UseTLS, &zoneInfo->Addr, zoneInfo->Port, q, mDNSNULL, mDNSNULL);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Dynamic Updates
#endif

// Called in normal callback context (i.e. mDNS_busy and mDNS_reentrancy are both 1)
mDNSexport void RecordRegistrationGotZoneData(mDNS *const m, mStatus err, const ZoneData *zoneData)
	{
	AuthRecord *newRR = (AuthRecord*)zoneData->ZoneDataContext;
	AuthRecord *ptr;
	int c1, c2;

	if (m->mDNS_busy != m->mDNS_reentrancy)
		LogMsg("RecordRegistrationGotZoneData: mDNS_busy (%ld) != mDNS_reentrancy (%ld)", m->mDNS_busy, m->mDNS_reentrancy);

	newRR->nta = mDNSNULL;

	// Start off assuming we're going to use a lease
	// If we get an error from the server, and the update port as given in the SRV record is 53, then we'll retry without the lease option
	newRR->uselease = mDNStrue;

	// make sure record is still in list (!!!)
	for (ptr = m->ResourceRecords; ptr; ptr = ptr->next) if (ptr == newRR) break;
	if (!ptr) { LogMsg("RecordRegistrationGotZoneData - RR no longer in list.  Discarding."); return; }

	// check error/result
	if (err)
		{
		if (err != mStatus_NoSuchNameErr) LogMsg("RecordRegistrationGotZoneData: error %d", err);
		return;
		}

	if (!zoneData) { LogMsg("ERROR: RecordRegistrationGotZoneData invoked with NULL result and no error"); return; }

	if (newRR->resrec.rrclass != zoneData->ZoneClass)
		{
		LogMsg("ERROR: New resource record's class (%d) does not match zone class (%d)", newRR->resrec.rrclass, zoneData->ZoneClass);
		return;
		}

	// Don't try to do updates to the root name server.
	// We might be tempted also to block updates to any single-label name server (e.g. com, edu, net, etc.) but some
	// organizations use their own private pseudo-TLD, like ".home", etc, and we don't want to block that.
	if (zoneData->ZoneName.c[0] == 0)
		{
		LogInfo("RecordRegistrationGotZoneData: No name server found claiming responsibility for \"%##s\"!", newRR->resrec.name->c);
		return;
		}

	// Store discovered zone data
	c1 = CountLabels(newRR->resrec.name);
	c2 = CountLabels(&zoneData->ZoneName);
	if (c2 > c1)
		{
		LogMsg("RecordRegistrationGotZoneData: Zone \"%##s\" is longer than \"%##s\"", zoneData->ZoneName.c, newRR->resrec.name->c);
		return;
		}
	newRR->zone = SkipLeadingLabels(newRR->resrec.name, c1-c2);
	if (!SameDomainName(newRR->zone, &zoneData->ZoneName))
		{
		LogMsg("RecordRegistrationGotZoneData: Zone \"%##s\" does not match \"%##s\" for \"%##s\"", newRR->zone->c, zoneData->ZoneName.c, newRR->resrec.name->c);
		return;
		}
	newRR->UpdateServer = zoneData->Addr;
	newRR->UpdatePort   = zoneData->Port;
	newRR->Private      = zoneData->ZonePrivate;
	debugf("RecordRegistrationGotZoneData: Set newRR->UpdateServer %##s %##s to %#a:%d",
		newRR->resrec.name->c, zoneData->ZoneName.c, &newRR->UpdateServer, mDNSVal16(newRR->UpdatePort));

	if (mDNSIPPortIsZero(zoneData->Port) || mDNSAddressIsZero(&zoneData->Addr))
		{
		LogInfo("RecordRegistrationGotZoneData: No _dns-update._udp service found for \"%##s\"!", newRR->resrec.name->c);
		return;
		}

	newRR->ThisAPInterval = 5 * mDNSPlatformOneSecond;		// After doubling, first retry will happen after ten seconds

	mDNS_Lock(m);	// SendRecordRegistration expects to be called with the lock held
	SendRecordRegistration(m, newRR);
	mDNS_Unlock(m);
	}

mDNSlocal void SendRecordDeregistration(mDNS *m, AuthRecord *rr)
	{
	mDNSu8 *ptr = m->omsg.data;
	mDNSu8 *end = (mDNSu8 *)&m->omsg + sizeof(DNSMessage);

	if (mDNSIPv4AddressIsZero(rr->UpdateServer.ip.v4))	// Don't know our UpdateServer yet
		{
		rr->LastAPTime = m->timenow;
		if (rr->ThisAPInterval < mDNSPlatformOneSecond * 5)
			rr->ThisAPInterval = mDNSPlatformOneSecond * 5;
		return;
		}

	InitializeDNSMessage(&m->omsg.h, rr->updateid, UpdateReqFlags);

	ptr = putZone(&m->omsg, ptr, end, rr->zone, mDNSOpaque16fromIntVal(rr->resrec.rrclass));
	if (ptr) ptr = putDeletionRecord(&m->omsg, ptr, &rr->resrec);
	if (!ptr)
		{
		LogMsg("SendRecordDeregistration Error: could not contruct deregistration packet for %s", ARDisplayString(m, rr));
		if (rr->state == regState_DeregPending) CompleteDeregistration(m, rr);
		}
	else
		{
		rr->expire = 0;		// Indicate that we have no active registration any more
		if (rr->Private)
			{
			LogInfo("SendRecordDeregistration TCP %p %s", rr->tcp, ARDisplayString(m, rr));
			if (rr->tcp) LogInfo("SendRecordDeregistration: Disposing existing TCP connection for %s", ARDisplayString(m, rr));
			if (rr->tcp) DisposeTCPConn(rr->tcp);
			rr->tcp = MakeTCPConn(m, &m->omsg, ptr, kTCPSocketFlags_UseTLS, &rr->UpdateServer, rr->UpdatePort, mDNSNULL, mDNSNULL, rr);
			if (!rr->tcp) rr->ThisAPInterval = mDNSPlatformOneSecond * 5; // If failed to make TCP connection, try again in ten seconds (5*2)
			else if (rr->ThisAPInterval < mDNSPlatformOneSecond * 30) rr->ThisAPInterval = mDNSPlatformOneSecond * 30;
			SetRecordRetry(m, rr, mStatus_NoError);
			}
		else
			{
			mStatus err = mDNSSendDNSMessage(m, &m->omsg, ptr, mDNSInterface_Any, mDNSNULL, &rr->UpdateServer, rr->UpdatePort, mDNSNULL, GetAuthInfoForName_internal(m, rr->resrec.name));
			if (err) debugf("ERROR: SendRecordDeregistration - mDNSSendDNSMessage - %d", err);
			if (rr->state == regState_DeregPending) CompleteDeregistration(m, rr);		// Don't touch rr after this
			}
		}
	}

mDNSexport mStatus uDNS_DeregisterRecord(mDNS *const m, AuthRecord *const rr)
	{
	switch (rr->state)
		{
		case regState_NATMap:        LogMsg("regState_NATMap        %##s type %s", rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype)); return mStatus_NoError;
		case regState_ExtraQueued: rr->state = regState_Unregistered; break;
		case regState_Refresh:
		case regState_Pending:
		case regState_UpdatePending:
		case regState_FetchingZoneData:
		case regState_Registered: break;
		case regState_DeregPending: break;
		case regState_DeregDeferred: LogMsg("regState_DeregDeferred %##s type %s", rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype)); return mStatus_NoError;
		case regState_Unregistered:  LogMsg("regState_Unregistered  %##s type %s", rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype)); return mStatus_NoError;
		case regState_NATError:      LogMsg("regState_NATError      %##s type %s", rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype)); return mStatus_NoError;
		case regState_NoTarget:      LogMsg("regState_NoTarget      %##s type %s", rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype)); return mStatus_NoError;
		default: LogMsg("uDNS_DeregisterRecord: State %d for %##s type %s", rr->state, rr->resrec.name->c, DNSTypeName(rr->resrec.rrtype)); return mStatus_NoError;
		}

	if (rr->state != regState_Unregistered) { rr->state = regState_DeregPending; SendRecordDeregistration(m, rr); }
	return mStatus_NoError;
	}

// Called with lock held
mDNSexport mStatus uDNS_DeregisterService(mDNS *const m, ServiceRecordSet *srs)
	{
	char *errmsg = "Unknown State";

	if (m->mDNS_busy != m->mDNS_reentrancy+1)
		LogMsg("uDNS_DeregisterService: Lock not held! mDNS_busy (%ld) mDNS_reentrancy (%ld)", m->mDNS_busy, m->mDNS_reentrancy);

	// don't re-register with a new target following deregistration
	srs->SRVChanged = srs->SRVUpdateDeferred = mDNSfalse;

	if (srs->srs_nta) { CancelGetZoneData(m, srs->srs_nta); srs->srs_nta = mDNSNULL; }

	if (srs->NATinfo.clientContext)
		{
		mDNS_StopNATOperation_internal(m, &srs->NATinfo);
		srs->NATinfo.clientContext = mDNSNULL;
		}

	switch (srs->state)
		{
		case regState_Unregistered:
			debugf("uDNS_DeregisterService - service %##s not registered", srs->RR_SRV.resrec.name->c);
			return mStatus_BadReferenceErr;
		case regState_DeregPending:
		case regState_DeregDeferred:
			debugf("Double deregistration of service %##s", srs->RR_SRV.resrec.name->c);
			return mStatus_NoError;
		case regState_NATError:	// not registered
		case regState_NATMap:	// not registered
		case regState_NoTarget:	// not registered
			unlinkSRS(m, srs);
			srs->state = regState_Unregistered;
			mDNS_DropLockBeforeCallback();
			srs->ServiceCallback(m, srs, mStatus_MemFree);
			mDNS_ReclaimLockAfterCallback();
			return mStatus_NoError;
		case regState_Pending:
		case regState_Refresh:
		case regState_UpdatePending:
		case regState_FetchingZoneData:
		case regState_Registered:
			srs->state = regState_DeregPending;
			SendServiceDeregistration(m, srs);
			return mStatus_NoError;
		case regState_ExtraQueued: // only for record registrations
			errmsg = "bad state (regState_ExtraQueued)";
			goto error;
		default: LogMsg("uDNS_DeregisterService: Unknown state %d for %##s", srs->state, srs->RR_SRV.resrec.name->c);
		}

	error:
	LogMsg("Error, uDNS_DeregisterService: %s", errmsg);
	return mStatus_BadReferenceErr;
	}

mDNSexport mStatus uDNS_UpdateRecord(mDNS *m, AuthRecord *rr)
	{
	ServiceRecordSet *parent = mDNSNULL;
	AuthRecord *rptr;
	regState_t *stateptr = mDNSNULL;

	// find the record in registered service list
	for (parent = m->ServiceRegistrations; parent; parent = parent->uDNS_next)
		if (&parent->RR_TXT == rr) { stateptr = &parent->state; break; }

	if (!parent)
		{
		// record not part of a service - check individual record registrations
		for (rptr = m->ResourceRecords; rptr; rptr = rptr->next)
			if (rptr == rr) { stateptr = &rr->state; break; }
		if (!rptr) goto unreg_error;
		}

	switch(*stateptr)
		{
		case regState_DeregPending:
		case regState_DeregDeferred:
		case regState_Unregistered:
			// not actively registered
			goto unreg_error;

		case regState_FetchingZoneData:
		case regState_NATMap:
		case regState_ExtraQueued:
		case regState_NoTarget:
			// change rdata directly since it hasn't been sent yet
			if (rr->UpdateCallback) rr->UpdateCallback(m, rr, rr->resrec.rdata);
			SetNewRData(&rr->resrec, rr->NewRData, rr->newrdlength);
			rr->NewRData = mDNSNULL;
			return mStatus_NoError;

		case regState_Pending:
		case regState_Refresh:
		case regState_UpdatePending:
			// registration in-flight. queue rdata and return
			if (rr->QueuedRData && rr->UpdateCallback)
				// if unsent rdata is already queued, free it before we replace it
				rr->UpdateCallback(m, rr, rr->QueuedRData);
			rr->QueuedRData = rr->NewRData;
			rr->QueuedRDLen = rr->newrdlength;
			rr->NewRData = mDNSNULL;
			return mStatus_NoError;

		case regState_Registered:
			rr->OrigRData = rr->resrec.rdata;
			rr->OrigRDLen = rr->resrec.rdlength;
			rr->InFlightRData = rr->NewRData;
			rr->InFlightRDLen = rr->newrdlength;
			rr->NewRData = mDNSNULL;
			*stateptr = regState_UpdatePending;
			if (parent) SendServiceRegistration(m, parent);
			else SendRecordRegistration(m, rr);
			return mStatus_NoError;

		case regState_NATError:
			LogMsg("ERROR: uDNS_UpdateRecord called for record %##s with bad state regState_NATError", rr->resrec.name->c);
			return mStatus_UnknownErr;	// states for service records only

		default: LogMsg("uDNS_UpdateRecord: Unknown state %d for %##s", *stateptr, rr->resrec.name->c);
		}

	unreg_error:
	LogMsg("Requested update of record %##s type %d, part of service not currently registered",
		   rr->resrec.name->c, rr->resrec.rrtype);
	return mStatus_Invalid;
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Periodic Execution Routines
#endif

// The question to be checked is not passed in as an explicit parameter;
// instead it is implicit that the question to be checked is m->CurrentQuestion.
mDNSexport void uDNS_CheckCurrentQuestion(mDNS *const m)
	{
	DNSQuestion *q = m->CurrentQuestion;
	mDNSs32 sendtime = q->LastQTime + q->ThisQInterval;
	// Don't allow sendtime to be earlier than SuppressStdPort53Queries
	if (!q->LongLived && m->SuppressStdPort53Queries && sendtime - m->SuppressStdPort53Queries < 0)
		sendtime = m->SuppressStdPort53Queries;
	if (m->timenow - sendtime < 0) return;

	if (q->LongLived)
		{
		switch (q->state)
			{
			case LLQ_InitialRequest:   startLLQHandshake(m, q); break;
			case LLQ_SecondaryRequest: sendChallengeResponse(m, q, mDNSNULL); break;
			case LLQ_Established:      sendLLQRefresh(m, q); break;
			case LLQ_Poll:             break;	// Do nothing (handled below)
			}
		}

	// We repeat the check above (rather than just making this the "else" case) because startLLQHandshake can change q->state to LLQ_Poll
	if (!(q->LongLived && q->state != LLQ_Poll))
		{
		if (q->unansweredQueries >= MAX_UCAST_UNANSWERED_QUERIES)
			{
			DNSServer *orig = q->qDNSServer;
			if (orig) LogInfo("Sent %d unanswered queries for %##s (%s) to %#a:%d (%##s)", q->unansweredQueries, q->qname.c, DNSTypeName(q->qtype), &orig->addr, mDNSVal16(orig->port), orig->domain.c);

			PenalizeDNSServer(m, q, mDNStrue);
			}

		if (q->qDNSServer && q->qDNSServer->teststate != DNSServer_Disabled)
			{
			mDNSu8 *end = m->omsg.data;
			mStatus err = mStatus_NoError;
			mDNSBool private = mDNSfalse;

			InitializeDNSMessage(&m->omsg.h, q->TargetQID, uQueryFlags);

			if (q->qDNSServer->teststate != DNSServer_Untested || NoTestQuery(q))
				{
				end = putQuestion(&m->omsg, m->omsg.data, m->omsg.data + AbsoluteMaxDNSMessageData, &q->qname, q->qtype, q->qclass);
				private = (q->AuthInfo && q->AuthInfo->AutoTunnel);
				}
			else if (m->timenow - q->qDNSServer->lasttest >= INIT_UCAST_POLL_INTERVAL)	// Make sure at least three seconds has elapsed since last test query
				{
				LogInfo("Sending DNS test query to %#a:%d", &q->qDNSServer->addr, mDNSVal16(q->qDNSServer->port));
				q->ThisQInterval = INIT_UCAST_POLL_INTERVAL / QuestionIntervalStep;
				q->qDNSServer->lasttest = m->timenow;
				end = putQuestion(&m->omsg, m->omsg.data, m->omsg.data + AbsoluteMaxDNSMessageData, DNSRelayTestQuestion, kDNSType_PTR, kDNSClass_IN);
				q->qDNSServer->testid = m->omsg.h.id;
				}

			if (end > m->omsg.data && (q->qDNSServer->teststate != DNSServer_Failed || NoTestQuery(q)))
				{
				//LogMsg("uDNS_CheckCurrentQuestion %p %d %p %##s (%s)", q, sendtime - m->timenow, private, q->qname.c, DNSTypeName(q->qtype));
				if (private)
					{
					if (q->nta) CancelGetZoneData(m, q->nta);
					q->nta = StartGetZoneData(m, &q->qname, q->LongLived ? ZoneServiceLLQ : ZoneServiceQuery, PrivateQueryGotZoneData, q);
					if (q->state == LLQ_Poll) q->ThisQInterval = (LLQ_POLL_INTERVAL + mDNSRandom(LLQ_POLL_INTERVAL/10)) / QuestionIntervalStep;
					}
				else
					{
					if (!q->LocalSocket) q->LocalSocket = mDNSPlatformUDPSocket(m, zeroIPPort);
					if (!q->LocalSocket) err = mStatus_NoMemoryErr;	// If failed to make socket (should be very rare), we'll try again next time
					else err = mDNSSendDNSMessage(m, &m->omsg, end, q->qDNSServer->interface, q->LocalSocket, &q->qDNSServer->addr, q->qDNSServer->port, mDNSNULL, mDNSNULL);
					m->SuppressStdPort53Queries = NonZeroTime(m->timenow + (mDNSPlatformOneSecond+99)/100);
					}
				}

			if (err) debugf("ERROR: uDNS_idle - mDNSSendDNSMessage - %d", err); // surpress syslog messages if we have no network
			else
				{
				q->ThisQInterval = q->ThisQInterval * QuestionIntervalStep;	// Only increase interval if send succeeded
				q->unansweredQueries++;
				if (q->ThisQInterval > MAX_UCAST_POLL_INTERVAL)
					q->ThisQInterval = MAX_UCAST_POLL_INTERVAL;
				if (private && q->state != LLQ_Poll)
					{
					// We don't want to retransmit too soon. Hence, we always schedule our first
					// retransmisson at 3 seconds rather than one second
					if (q->ThisQInterval < (3 * mDNSPlatformOneSecond))
						q->ThisQInterval = q->ThisQInterval * QuestionIntervalStep;
					if (q->ThisQInterval > LLQ_POLL_INTERVAL)
						q->ThisQInterval = LLQ_POLL_INTERVAL;
					LogInfo("uDNS_CheckCurrentQuestion: private non polling question for %##s (%s) will be retried in %d ms", q->qname.c, DNSTypeName(q->qtype), q->ThisQInterval);
					}
				debugf("Increased ThisQInterval to %d for %##s (%s)", q->ThisQInterval, q->qname.c, DNSTypeName(q->qtype));
				}
			q->LastQTime = m->timenow;
			SetNextQueryTime(m, q);
			}
		else
			{
			// If we have no server for this query, or the only server is a disabled one, then we deliver
			// a transient failure indication to the client. This is important for things like iPhone
			// where we want to return timely feedback to the user when no network is available.
			// After calling MakeNegativeCacheRecord() we store the resulting record in the
			// cache so that it will be visible to other clients asking the same question.
			// (When we have a group of identical questions, only the active representative of the group gets
			// passed to uDNS_CheckCurrentQuestion -- we only want one set of query packets hitting the wire --
			// but we want *all* of the questions to get answer callbacks.)

			CacheRecord *rr;
			const mDNSu32 slot = HashSlot(&q->qname);
			CacheGroup *const cg = CacheGroupForName(m, slot, q->qnamehash, &q->qname);
			if (cg)
				for (rr = cg->members; rr; rr=rr->next)
					if (SameNameRecordAnswersQuestion(&rr->resrec, q)) mDNS_PurgeCacheResourceRecord(m, rr);

			if (!q->qDNSServer) LogInfo("uDNS_CheckCurrentQuestion no DNS server for %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
			else LogMsg("uDNS_CheckCurrentQuestion DNS server %#a:%d for %##s is disabled", &q->qDNSServer->addr, mDNSVal16(q->qDNSServer->port), q->qname.c);

			MakeNegativeCacheRecord(m, &m->rec.r, &q->qname, q->qnamehash, q->qtype, q->qclass, 60, mDNSInterface_Any);
			// Inactivate this question until the next change of DNS servers (do this before AnswerCurrentQuestionWithResourceRecord)
			q->ThisQInterval = 0;
			q->unansweredQueries = 0;
			CreateNewCacheEntry(m, slot, cg);
			m->rec.r.resrec.RecordType = 0;		// Clear RecordType to show we're not still using it
			// MUST NOT touch m->CurrentQuestion (or q) after this -- client callback could have deleted it
			}
		}
	}

mDNSlocal void CheckNATMappings(mDNS *m)
	{
	mStatus err = mStatus_NoError;
	mDNSBool rfc1918 = mDNSv4AddrIsRFC1918(&m->AdvertisedV4.ip.v4);
	mDNSBool HaveRoutable = !rfc1918 && !mDNSIPv4AddressIsZero(m->AdvertisedV4.ip.v4);
	m->NextScheduledNATOp = m->timenow + 0x3FFFFFFF;

	if (HaveRoutable) m->ExternalAddress = m->AdvertisedV4.ip.v4;

	if (m->NATTraversals && rfc1918)			// Do we need to open NAT-PMP socket to receive multicast announcements from router?
		{
		if (m->NATMcastRecvskt == mDNSNULL)		// If we are behind a NAT and the socket hasn't been opened yet, open it
			{
			// we need to log a message if we can't get our socket, but only the first time (after success)
			static mDNSBool needLog = mDNStrue;
			m->NATMcastRecvskt = mDNSPlatformUDPSocket(m, NATPMPAnnouncementPort);
			if (!m->NATMcastRecvskt)
				{
				if (needLog)
					{
					LogMsg("CheckNATMappings: Failed to allocate port 5350 UDP multicast socket for NAT-PMP announcements");
					needLog = mDNSfalse;
					}
				}
			else
				needLog = mDNStrue;				
			}
		}
	else										// else, we don't want to listen for announcements, so close them if they're open
		{
		if (m->NATMcastRecvskt) { mDNSPlatformUDPClose(m->NATMcastRecvskt); m->NATMcastRecvskt = mDNSNULL; }
		if (m->SSDPSocket)      { debugf("CheckNATMappings destroying SSDPSocket %p", &m->SSDPSocket); mDNSPlatformUDPClose(m->SSDPSocket); m->SSDPSocket = mDNSNULL; }
		}

	if (m->NATTraversals)
		{
		if (m->timenow - m->retryGetAddr >= 0)
			{
			err = uDNS_SendNATMsg(m, mDNSNULL);		// Will also do UPnP discovery for us, if necessary
			if (!err)
				{
				if      (m->retryIntervalGetAddr < NATMAP_INIT_RETRY)             m->retryIntervalGetAddr = NATMAP_INIT_RETRY;
				else if (m->retryIntervalGetAddr < NATMAP_MAX_RETRY_INTERVAL / 2) m->retryIntervalGetAddr *= 2;
				else                                                              m->retryIntervalGetAddr = NATMAP_MAX_RETRY_INTERVAL;
				}
			LogInfo("CheckNATMappings retryGetAddr sent address request err %d interval %d", err, m->retryIntervalGetAddr);

			// Always update m->retryGetAddr, even if we fail to send the packet. Otherwise in cases where we can't send the packet
			// (like when we have no active interfaces) we'll spin in an infinite loop repeatedly failing to send the packet
			m->retryGetAddr = m->timenow + m->retryIntervalGetAddr;
			}
		// Even when we didn't send the GetAddr packet, still need to make sure NextScheduledNATOp is set correctly
		if (m->NextScheduledNATOp - m->retryGetAddr > 0)
			m->NextScheduledNATOp = m->retryGetAddr;
		}

	if (m->CurrentNATTraversal) LogMsg("WARNING m->CurrentNATTraversal already in use");
	m->CurrentNATTraversal = m->NATTraversals;

	while (m->CurrentNATTraversal)
		{
		NATTraversalInfo *cur = m->CurrentNATTraversal;
		m->CurrentNATTraversal = m->CurrentNATTraversal->next;

		if (HaveRoutable)		// If not RFC 1918 address, our own address and port are effectively our external address and port
			{
			cur->ExpiryTime = 0;
			cur->NewResult  = mStatus_NoError;
			}
		else if (cur->Protocol)		// Check if it's time to send port mapping packets
			{
			if (m->timenow - cur->retryPortMap >= 0)						// Time to do something with this mapping
				{
				if (cur->ExpiryTime && cur->ExpiryTime - m->timenow < 0)	// Mapping has expired
					{
					cur->ExpiryTime    = 0;
					cur->retryInterval = NATMAP_INIT_RETRY;
					}

				//LogMsg("uDNS_SendNATMsg");
				err = uDNS_SendNATMsg(m, cur);

				if (cur->ExpiryTime)						// If have active mapping then set next renewal time halfway to expiry
					NATSetNextRenewalTime(m, cur);
				else										// else no mapping; use exponential backoff sequence
					{
					if      (cur->retryInterval < NATMAP_INIT_RETRY            ) cur->retryInterval = NATMAP_INIT_RETRY;
					else if (cur->retryInterval < NATMAP_MAX_RETRY_INTERVAL / 2) cur->retryInterval *= 2;
					else                                                         cur->retryInterval = NATMAP_MAX_RETRY_INTERVAL;
					cur->retryPortMap = m->timenow + cur->retryInterval;
					}
				}

			if (m->NextScheduledNATOp - cur->retryPortMap > 0)
				m->NextScheduledNATOp = cur->retryPortMap;
			}

		// Notify the client if necessary. We invoke the callback if:
		// (1) we have an ExternalAddress, or we've tried and failed a couple of times to discover it
		// and (2) the client doesn't want a mapping, or the client won't need a mapping, or the client has a successful mapping, or we've tried and failed a couple of times
		// and (3) we have new data to give the client that's changed since the last callback
		if (!mDNSIPv4AddressIsZero(m->ExternalAddress) || m->retryIntervalGetAddr > NATMAP_INIT_RETRY * 8)
			{
			const mStatus EffectiveResult = cur->NewResult ? cur->NewResult : mDNSv4AddrIsRFC1918(&m->ExternalAddress) ? mStatus_DoubleNAT : mStatus_NoError;
			const mDNSIPPort ExternalPort = HaveRoutable ? cur->IntPort :
				!mDNSIPv4AddressIsZero(m->ExternalAddress) && cur->ExpiryTime ? cur->RequestedPort : zeroIPPort;
			if (!cur->Protocol || HaveRoutable || cur->ExpiryTime || cur->retryInterval > NATMAP_INIT_RETRY * 8)
				if (!mDNSSameIPv4Address(cur->ExternalAddress, m->ExternalAddress) ||
					!mDNSSameIPPort     (cur->ExternalPort,       ExternalPort)    ||
					cur->Result != EffectiveResult)
					{
					//LogMsg("NAT callback %d %d %d", cur->Protocol, cur->ExpiryTime, cur->retryInterval);
					if (cur->Protocol && mDNSIPPortIsZero(ExternalPort) && !mDNSIPv4AddressIsZero(m->Router.ip.v4))
						{
						if (!EffectiveResult)
							LogInfo("Failed to obtain NAT port mapping %p from router %#a external address %.4a internal port %5d interval %d error %d",
								cur, &m->Router, &m->ExternalAddress, mDNSVal16(cur->IntPort), cur->retryInterval, EffectiveResult);
						else
							LogMsg("Failed to obtain NAT port mapping %p from router %#a external address %.4a internal port %5d interval %d error %d",
								cur, &m->Router, &m->ExternalAddress, mDNSVal16(cur->IntPort), cur->retryInterval, EffectiveResult);
						}

					cur->ExternalAddress = m->ExternalAddress;
					cur->ExternalPort    = ExternalPort;
					cur->Lifetime        = cur->ExpiryTime && !mDNSIPPortIsZero(ExternalPort) ?
						(cur->ExpiryTime - m->timenow + mDNSPlatformOneSecond/2) / mDNSPlatformOneSecond : 0;
					cur->Result          = EffectiveResult;
					mDNS_DropLockBeforeCallback();		// Allow client to legally make mDNS API calls from the callback
					if (cur->clientCallback)
						cur->clientCallback(m, cur);
					mDNS_ReclaimLockAfterCallback();	// Decrement mDNS_reentrancy to block mDNS API calls again
					// MUST NOT touch cur after invoking the callback
					}
			}
		}
	}

mDNSlocal mDNSs32 CheckRecordRegistrations(mDNS *m)
	{
	AuthRecord *rr;
	mDNSs32 nextevent = m->timenow + 0x3FFFFFFF;

	for (rr = m->ResourceRecords; rr; rr = rr->next)
		{
		if (rr->state == regState_FetchingZoneData ||
			rr->state == regState_Pending || rr->state == regState_DeregPending || rr->state == regState_UpdatePending ||
			rr->state == regState_DeregDeferred || rr->state == regState_Refresh || rr->state == regState_Registered)
			{
			if (rr->LastAPTime + rr->ThisAPInterval - m->timenow < 0)
				{
				if (rr->tcp) { DisposeTCPConn(rr->tcp); rr->tcp = mDNSNULL; }
				if (rr->state == regState_FetchingZoneData)
					{
					if (rr->nta) CancelGetZoneData(m, rr->nta);
					rr->nta = StartGetZoneData(m, rr->resrec.name, ZoneServiceUpdate, RecordRegistrationGotZoneData, rr);
					SetRecordRetry(m, rr, mStatus_NoError);
					}
				else if (rr->state == regState_DeregPending) SendRecordDeregistration(m, rr);
				else SendRecordRegistration(m, rr);
				}
			if (nextevent - (rr->LastAPTime + rr->ThisAPInterval) > 0)
				nextevent = (rr->LastAPTime + rr->ThisAPInterval);
			}
		}
	return nextevent;
	}

mDNSlocal mDNSs32 CheckServiceRegistrations(mDNS *m)
	{
	mDNSs32 nextevent = m->timenow + 0x3FFFFFFF;

	if (CurrentServiceRecordSet)
		LogMsg("CheckServiceRegistrations ERROR CurrentServiceRecordSet already set");
	CurrentServiceRecordSet = m->ServiceRegistrations;

	// Note: ServiceRegistrations list is in the order they were created; important for in-order event delivery
	while (CurrentServiceRecordSet)
		{
		ServiceRecordSet *srs = CurrentServiceRecordSet;
		CurrentServiceRecordSet = CurrentServiceRecordSet->uDNS_next;
		if (srs->state == regState_FetchingZoneData ||
			srs->state == regState_Pending || srs->state == regState_DeregPending  || srs->state == regState_DeregDeferred ||
			srs->state == regState_Refresh || srs->state == regState_UpdatePending || srs->state == regState_Registered)
			{
			if (srs->RR_SRV.LastAPTime + srs->RR_SRV.ThisAPInterval - m->timenow <= 0)
				{
				if (srs->tcp) { DisposeTCPConn(srs->tcp); srs->tcp = mDNSNULL; }
				if (srs->state == regState_FetchingZoneData)
					{
					if (srs->srs_nta) CancelGetZoneData(m, srs->srs_nta);
					srs->srs_nta = StartGetZoneData(m, srs->RR_SRV.resrec.name, ZoneServiceUpdate, ServiceRegistrationGotZoneData, srs);
					SetRecordRetry(m, &srs->RR_SRV, mStatus_NoError);
					}
				else if (srs->state == regState_DeregPending) SendServiceDeregistration(m, srs);
				else SendServiceRegistration(m, srs);
				}
			if (nextevent - (srs->RR_SRV.LastAPTime + srs->RR_SRV.ThisAPInterval) > 0)
				nextevent = (srs->RR_SRV.LastAPTime + srs->RR_SRV.ThisAPInterval);
			}
		}
	return nextevent;
	}

// This function is called early on in mDNS_Execute before any uDNS questions are
// dispatched so that if there are some good servers, the uDNS questions can now
// use it
mDNSexport void ResetDNSServerPenalties(mDNS *m)
	{
	DNSServer *d;
	for (d = m->DNSServers; d; d=d->next)
		{
		if (d->penaltyTime != 0)
			{
			if (d->penaltyTime - m->timenow <= 0)
				{
				LogInfo("ResetDNSServerPenalties: DNS server %#a:%d out of penalty box", &d->addr, mDNSVal16(d->port));
				d->penaltyTime = 0;
				}
			}
		}
	}

mDNSlocal mDNSs32 CheckDNSServerPenalties(mDNS *m)
	{
	mDNSs32 nextevent = m->timenow + 0x3FFFFFFF;
	DNSServer *d;
	for (d = m->DNSServers; d; d=d->next)
		{
		if (d->penaltyTime != 0)
			{
			if ((nextevent - d->penaltyTime) > 0)
				nextevent = d->penaltyTime;
			}
		}
	return nextevent;
	}

mDNSexport void uDNS_Execute(mDNS *const m)
	{
	mDNSs32 nexte;

	m->NextuDNSEvent = m->timenow + 0x3FFFFFFF;

	if (m->NextSRVUpdate && m->NextSRVUpdate - m->timenow < 0)
		{ m->NextSRVUpdate = 0; UpdateSRVRecords(m); }

	CheckNATMappings(m);

	if (m->SuppressStdPort53Queries && m->timenow - m->SuppressStdPort53Queries >= 0)
		m->SuppressStdPort53Queries = 0; // If suppression time has passed, clear it

	nexte = CheckRecordRegistrations(m);
	if (nexte - m->NextuDNSEvent < 0) m->NextuDNSEvent = nexte;

	nexte = CheckServiceRegistrations(m);
	if (nexte - m->NextuDNSEvent < 0) m->NextuDNSEvent = nexte;

	nexte = CheckDNSServerPenalties(m);
	if (nexte - m->NextuDNSEvent < 0) m->NextuDNSEvent = nexte;
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Startup, Shutdown, and Sleep
#endif

// simplest sleep logic - rather than having sleep states that must be dealt with explicitly in all parts of
// the code, we simply send a deregistration, and put the service in Refresh state, with a timeout far enough
// in the future that we'll sleep (or the sleep will be cancelled) before it is retransmitted. Then to wake,
// we just move up the timers.

mDNSexport void SleepRecordRegistrations(mDNS *m)
	{
	AuthRecord *rr;
	for (rr = m->ResourceRecords; rr; rr=rr->next)
		if (AuthRecord_uDNS(rr))
			if (rr->state == regState_Registered ||
				rr->state == regState_Refresh)
				{
				SendRecordDeregistration(m, rr);
				rr->state = regState_Refresh;
				rr->LastAPTime = m->timenow;
				rr->ThisAPInterval = 300 * mDNSPlatformOneSecond;
				}
	}

mDNSexport void SleepServiceRegistrations(mDNS *m)
	{
	ServiceRecordSet *srs = m->ServiceRegistrations;
	while (srs)
		{
		LogInfo("SleepServiceRegistrations: state %d %s", srs->state, ARDisplayString(m, &srs->RR_SRV));
		if (srs->srs_nta) { CancelGetZoneData(m, srs->srs_nta); srs->srs_nta = mDNSNULL; }

		if (srs->NATinfo.clientContext)
			{
			mDNS_StopNATOperation_internal(m, &srs->NATinfo);
			srs->NATinfo.clientContext = mDNSNULL;
			}

		if (srs->state == regState_UpdatePending)
			{
			// act as if the update succeeded, since we're about to delete the name anyway
			AuthRecord *txt = &srs->RR_TXT;
			srs->state = regState_Registered;
			// deallocate old RData
			if (txt->UpdateCallback) txt->UpdateCallback(m, txt, txt->OrigRData);
			SetNewRData(&txt->resrec, txt->InFlightRData, txt->InFlightRDLen);
			txt->OrigRData = mDNSNULL;
			txt->InFlightRData = mDNSNULL;
			}

		if (srs->state == regState_Registered || srs->state == regState_Refresh)
			SendServiceDeregistration(m, srs);

		srs->state = regState_NoTarget;	// when we wake, we'll re-register (and optionally nat-map) once our address record completes
		srs->RR_SRV.resrec.rdata->u.srv.target.c[0] = 0;
		srs->SRSUpdateServer = zeroAddr;		// This will cause UpdateSRV to do a new StartGetZoneData
		srs->RR_SRV.ThisAPInterval = 5 * mDNSPlatformOneSecond;		// After doubling, first retry will happen after ten seconds

		srs = srs->uDNS_next;
		}
	}

mDNSexport void mDNS_AddSearchDomain(const domainname *const domain)
	{
	SearchListElem **p;

	// Check to see if we already have this domain in our list
	for (p = &SearchList; *p; p = &(*p)->next)
		if (SameDomainName(&(*p)->domain, domain))
			{
			// If domain is already in list, and marked for deletion, change it to "leave alone"
			if ((*p)->flag == -1) (*p)->flag = 0;
			LogInfo("mDNS_AddSearchDomain already in list %##s", domain->c);
			return;
			}

	// if domain not in list, add to list, mark as add (1)
	*p = mDNSPlatformMemAllocate(sizeof(SearchListElem));
	if (!*p) { LogMsg("ERROR: mDNS_AddSearchDomain - malloc"); return; }
	mDNSPlatformMemZero(*p, sizeof(SearchListElem));
	AssignDomainName(&(*p)->domain, domain);
	(*p)->flag = 1;	// add
	(*p)->next = mDNSNULL;
	LogInfo("mDNS_AddSearchDomain created new %##s", domain->c);
	}

mDNSlocal void FreeARElemCallback(mDNS *const m, AuthRecord *const rr, mStatus result)
	{
	(void)m;	// unused
	if (result == mStatus_MemFree) mDNSPlatformMemFree(rr->RecordContext);
	}

mDNSlocal void FoundDomain(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
	{
	SearchListElem *slElem = question->QuestionContext;
	mStatus err;
	const char *name;

	if (answer->rrtype != kDNSType_PTR) return;
	if (answer->RecordType == kDNSRecordTypePacketNegative) return;
	if (answer->InterfaceID == mDNSInterface_LocalOnly) return;

	if      (question == &slElem->BrowseQ)          name = mDNS_DomainTypeNames[mDNS_DomainTypeBrowse];
	else if (question == &slElem->DefBrowseQ)       name = mDNS_DomainTypeNames[mDNS_DomainTypeBrowseDefault];
	else if (question == &slElem->AutomaticBrowseQ) name = mDNS_DomainTypeNames[mDNS_DomainTypeBrowseAutomatic];
	else if (question == &slElem->RegisterQ)        name = mDNS_DomainTypeNames[mDNS_DomainTypeRegistration];
	else if (question == &slElem->DefRegisterQ)     name = mDNS_DomainTypeNames[mDNS_DomainTypeRegistrationDefault];
	else { LogMsg("FoundDomain - unknown question"); return; }

	LogInfo("FoundDomain: %p %s %s Q %##s A %s", answer->InterfaceID, AddRecord ? "Add" : "Rmv", name, question->qname.c, RRDisplayString(m, answer));

	if (AddRecord)
		{
		ARListElem *arElem = mDNSPlatformMemAllocate(sizeof(ARListElem));
		if (!arElem) { LogMsg("ERROR: FoundDomain out of memory"); return; }
		mDNS_SetupResourceRecord(&arElem->ar, mDNSNULL, mDNSInterface_LocalOnly, kDNSType_PTR, 7200, kDNSRecordTypeShared, FreeARElemCallback, arElem);
		MakeDomainNameFromDNSNameString(&arElem->ar.namestorage, name);
		AppendDNSNameString            (&arElem->ar.namestorage, "local");
		AssignDomainName(&arElem->ar.resrec.rdata->u.name, &answer->rdata->u.name);
		LogInfo("FoundDomain: Registering %s", ARDisplayString(m, &arElem->ar));
		err = mDNS_Register(m, &arElem->ar);
		if (err) { LogMsg("ERROR: FoundDomain - mDNS_Register returned %d", err); mDNSPlatformMemFree(arElem); return; }
		arElem->next = slElem->AuthRecs;
		slElem->AuthRecs = arElem;
		}
	else
		{
		ARListElem **ptr = &slElem->AuthRecs;
		while (*ptr)
			{
			if (SameDomainName(&(*ptr)->ar.resrec.rdata->u.name, &answer->rdata->u.name))
				{
				ARListElem *dereg = *ptr;
				*ptr = (*ptr)->next;
				LogInfo("FoundDomain: Deregistering %s", ARDisplayString(m, &dereg->ar));
				err = mDNS_Deregister(m, &dereg->ar);
				if (err) LogMsg("ERROR: FoundDomain - mDNS_Deregister returned %d", err);
				// Memory will be freed in the FreeARElemCallback
				}
			else
				ptr = &(*ptr)->next;
			}
		}
	}

#if APPLE_OSX_mDNSResponder && MACOSX_MDNS_MALLOC_DEBUGGING
mDNSexport void udns_validatelists(void *const v)
	{
	mDNS *const m = v;

	ServiceRecordSet *s;
	for (s = m->ServiceRegistrations; s; s=s->uDNS_next)
		if (s->uDNS_next == (ServiceRecordSet*)~0)
			LogMemCorruption("m->ServiceRegistrations: %p is garbage (%lX)", s, s->uDNS_next);

	NATTraversalInfo *n;
	for (n = m->NATTraversals; n; n=n->next)
		if (n->next == (NATTraversalInfo *)~0 || n->clientCallback == (NATTraversalClientCallback)~0)
			LogMemCorruption("m->NATTraversals: %p is garbage", n);

	DNSServer *d;
	for (d = m->DNSServers; d; d=d->next)
		if (d->next == (DNSServer *)~0 || d->teststate > DNSServer_Disabled)
			LogMemCorruption("m->DNSServers: %p is garbage (%d)", d, d->teststate);

	DomainAuthInfo *info;
	for (info = m->AuthInfoList; info; info = info->next)
		if (info->next == (DomainAuthInfo *)~0 || info->AutoTunnel == (mDNSBool)~0)
			LogMemCorruption("m->AuthInfoList: %p is garbage (%X)", info, info->AutoTunnel);

	HostnameInfo *hi;
	for (hi = m->Hostnames; hi; hi = hi->next)
		if (hi->next == (HostnameInfo *)~0 || hi->StatusCallback == (mDNSRecordCallback*)~0)
			LogMemCorruption("m->Hostnames: %p is garbage", n);

	SearchListElem *ptr;
	for (ptr = SearchList; ptr; ptr = ptr->next)
		if (ptr->next == (SearchListElem *)~0 || ptr->AuthRecs == (void*)~0)
			LogMemCorruption("SearchList: %p is garbage (%X)", ptr, ptr->AuthRecs);
	}
#endif

// This should probably move to the UDS daemon -- the concept of legacy clients and automatic registration / automatic browsing
// is really a UDS API issue, not something intrinsic to uDNS

mDNSexport mStatus uDNS_RegisterSearchDomains(mDNS *const m)
	{
	SearchListElem **p = &SearchList, *ptr;
	mStatus err;

	// step 1: mark each element for removal (-1)
	for (ptr = SearchList; ptr; ptr = ptr->next) ptr->flag = -1;

	// Client has requested domain enumeration or automatic browse -- time to make sure we have the search domains from the platform layer
	mDNS_Lock(m);
	m->RegisterSearchDomains = mDNStrue;
	mDNSPlatformSetDNSConfig(m, mDNSfalse, m->RegisterSearchDomains, mDNSNULL, mDNSNULL, mDNSNULL);
	mDNS_Unlock(m);

	// delete elems marked for removal, do queries for elems marked add
	while (*p)
		{
		ptr = *p;
		LogInfo("RegisterSearchDomains %d %p %##s", ptr->flag, ptr->AuthRecs, ptr->domain.c);
		if (ptr->flag == -1)	// remove
			{
			ARListElem *arList = ptr->AuthRecs;
			ptr->AuthRecs = mDNSNULL;
			*p = ptr->next;

			// If the user has "local" in their DNS searchlist, we ignore that for the purposes of domain enumeration queries
			if (!SameDomainName(&ptr->domain, &localdomain))
				{
				mDNS_StopGetDomains(m, &ptr->BrowseQ);
				mDNS_StopGetDomains(m, &ptr->RegisterQ);
				mDNS_StopGetDomains(m, &ptr->DefBrowseQ);
				mDNS_StopGetDomains(m, &ptr->DefRegisterQ);
				mDNS_StopGetDomains(m, &ptr->AutomaticBrowseQ);
				}
			mDNSPlatformMemFree(ptr);

	        // deregister records generated from answers to the query
			while (arList)
				{
				ARListElem *dereg = arList;
				arList = arList->next;
				debugf("Deregistering PTR %##s -> %##s", dereg->ar.resrec.name->c, dereg->ar.resrec.rdata->u.name.c);
				err = mDNS_Deregister(m, &dereg->ar);
				if (err) LogMsg("ERROR: RegisterSearchDomains mDNS_Deregister returned %d", err);
				// Memory will be freed in the FreeARElemCallback
				}
			continue;
			}

		if (ptr->flag == 1)	// add
			{
			// If the user has "local" in their DNS searchlist, we ignore that for the purposes of domain enumeration queries
			if (!SameDomainName(&ptr->domain, &localdomain))
				{
				mStatus err1, err2, err3, err4, err5;
				err1 = mDNS_GetDomains(m, &ptr->BrowseQ,          mDNS_DomainTypeBrowse,              &ptr->domain, mDNSInterface_Any, FoundDomain, ptr);
				err2 = mDNS_GetDomains(m, &ptr->DefBrowseQ,       mDNS_DomainTypeBrowseDefault,       &ptr->domain, mDNSInterface_Any, FoundDomain, ptr);
				err3 = mDNS_GetDomains(m, &ptr->RegisterQ,        mDNS_DomainTypeRegistration,        &ptr->domain, mDNSInterface_Any, FoundDomain, ptr);
				err4 = mDNS_GetDomains(m, &ptr->DefRegisterQ,     mDNS_DomainTypeRegistrationDefault, &ptr->domain, mDNSInterface_Any, FoundDomain, ptr);
				err5 = mDNS_GetDomains(m, &ptr->AutomaticBrowseQ, mDNS_DomainTypeBrowseAutomatic,     &ptr->domain, mDNSInterface_Any, FoundDomain, ptr);
				if (err1 || err2 || err3 || err4 || err5)
					LogMsg("GetDomains for domain %##s returned error(s):\n"
						   "%d (mDNS_DomainTypeBrowse)\n"
						   "%d (mDNS_DomainTypeBrowseDefault)\n"
						   "%d (mDNS_DomainTypeRegistration)\n"
						   "%d (mDNS_DomainTypeRegistrationDefault)"
						   "%d (mDNS_DomainTypeBrowseAutomatic)\n",
						   ptr->domain.c, err1, err2, err3, err4, err5);
				}
			ptr->flag = 0;
			}

		if (ptr->flag) { LogMsg("RegisterSearchDomains - unknown flag %d. Skipping.", ptr->flag); }

		p = &ptr->next;
		}

	return mStatus_NoError;
	}

// Construction of Default Browse domain list (i.e. when clients pass NULL) is as follows:
// 1) query for b._dns-sd._udp.local on LocalOnly interface
//    (.local manually generated via explicit callback)
// 2) for each search domain (from prefs pane), query for b._dns-sd._udp.<searchdomain>.
// 3) for each result from (2), register LocalOnly PTR record b._dns-sd._udp.local. -> <result>
// 4) result above should generate a callback from question in (1).  result added to global list
// 5) global list delivered to client via GetSearchDomainList()
// 6) client calls to enumerate domains now go over LocalOnly interface
//    (!!!KRS may add outgoing interface in addition)

struct CompileTimeAssertionChecks_uDNS
	{
	// Check our structures are reasonable sizes. Including overly-large buffers, or embedding
	// other overly-large structures instead of having a pointer to them, can inadvertently
	// cause structure sizes (and therefore memory usage) to balloon unreasonably.
	char sizecheck_tcpInfo_t     [(sizeof(tcpInfo_t)      <=  9056) ? 1 : -1];
	char sizecheck_SearchListElem[(sizeof(SearchListElem) <=  3920) ? 1 : -1];
	};
