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

    Change History (most recent first):

$Log: uDNS.c,v $
Revision 1.230.2.1  2006/08/29 06:24:23  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.230  2006/06/29 03:02:44  cheshire
<rdar://problem/4607042> mDNSResponder NXDOMAIN and CNAME support

Revision 1.229  2006/03/02 22:03:41  cheshire
<rdar://problem/4395331> Spurious warning "GetLargeResourceRecord: m->rec appears to be already in use"
Refinement: m->rec.r.resrec.RecordType needs to be cleared *every* time around for loop, not just once at the end

Revision 1.228  2006/02/26 00:54:42  cheshire
Fixes to avoid code generation warning/error on FreeBSD 7

Revision 1.227  2006/01/09 20:47:05  cheshire
<rdar://problem/4395331> Spurious warning "GetLargeResourceRecord: m->rec appears to be already in use"

Revision 1.226  2005/12/20 02:46:33  cheshire
<rdar://problem/4175520> mDNSPosix wide-area registration broken
Check too strict -- we can still do wide-area registration (without NAT-PMP)
without having to know our gateway address

Revision 1.225  2005/10/21 22:51:17  cheshire
<rdar://problem/4290265> Add check to avoid crashing NAT gateways that have buggy DNS relay code
Refinement: Shorten "check-for-broken-dns-relay" to just "dnsbugtest"
to avoid crashing NAT gateways that have a different DNS relay bug

Revision 1.224  2005/10/20 00:10:33  cheshire
<rdar://problem/4290265> Add check to avoid crashing NAT gateways that have buggy DNS relay code

Revision 1.223  2005/10/17 18:52:42  cheshire
<rdar://problem/4271183> mDNSResponder crashed in CheckRecordRegistrations
Move code to unregister the service's extra records from uDNS_DeregisterService() to unlinkSRS().

Revision 1.222  2005/10/05 23:04:10  cheshire
Add more information to unlinkAR and startLLQHandshakeCallback error messages

Revision 1.221  2005/10/05 17:27:48  herscher
<rdar://problem/4272516> Change 200ms delay to 10ms

Revision 1.220  2005/09/24 01:10:09  cheshire
Fix comment typos

Revision 1.219  2005/09/22 07:28:25  herscher
Double the delay to 200000 usec after sending out a DNS query

Revision 1.218  2005/09/13 01:06:14  herscher
<rdar://problem/4248878> Add 100ms delay in sendQuery.

Revision 1.217  2005/08/04 18:08:24  cheshire
Update comments

Revision 1.216  2005/07/29 23:05:22  ksekar
<rdar://problem/4137930> Hostname registration should register IPv6 AAAA record with DNS Update
Services should point to IPv6 address if IPv4 NAT mapping fails

Revision 1.215  2005/07/29 21:01:51  ksekar
<rdar://problem/4137930> Hostname registration should register IPv6 AAAA record with DNS Update
correction to original checkin - misplaced return in HostnameCallback and logic error determining v6 changes

Revision 1.214  2005/07/29 19:46:10  ksekar
<rdar://problem/4191860> reduce polling period on failed LLQs to 15 minutes

Revision 1.213  2005/07/29 18:04:22  ksekar
<rdar://problem/4137930> Hostname registration should register IPv6 AAAA record with DNS Update

Revision 1.212  2005/07/22 19:35:50  ksekar
<rdar://problem/4188821> SUTiger: LLQ event acknowledgments are not formated correctly

Revision 1.211  2005/07/21 18:51:04  ksekar
<rdar://problem/4103136> mDNSResponder times out when mapping ports after sleep

Revision 1.210  2005/07/21 18:47:31  ksekar
<rdar://problem/4137283> NAT-PMP refresh Requested Public Port should contain actual mapped port

Revision 1.209  2005/07/04 21:16:37  cheshire
Minor code tidying -- initialize variables where they are declared

Revision 1.208  2005/06/28 00:24:28  ksekar
<rdar://problem/4157823> memory smasher in conQueryCallback

Revision 1.207  2005/05/13 20:45:10  ksekar
<rdar://problem/4074400> Rapid wide-area txt record updates don't work

Revision 1.206  2005/03/31 02:19:55  cheshire
<rdar://problem/4021486> Fix build warnings
Reviewed by: Scott Herscher

Revision 1.205  2005/03/21 00:33:51  shersche
<rdar://problem/4021486> Fix build warnings on Win32 platform

Revision 1.204  2005/03/16 00:42:32  ksekar
<rdar://problem/4012279> Long-lived queries not working on Windows

Revision 1.203  2005/03/04 03:00:03  ksekar
<rdar://problem/4026546> Retransmissions happen too early, causing registrations to conflict with themselves

Revision 1.202  2005/03/01 19:29:17  ksekar
changed LogMsgs to debugfs

Revision 1.201  2005/02/26 03:04:13  cheshire
<rdar://problem/4017292> Should not indicate successful dynamic update if no network connection
Don't try to do updates to root name server. This ensures status dot turns red if user
enters a bad host name such as just "fred" instead of a properly fully-qualified name.

Revision 1.200  2005/02/25 17:47:45  ksekar
<rdar://problem/4021868> SendServiceRegistration fails on wake from sleep

Revision 1.199  2005/02/25 04:21:00  cheshire
<rdar://problem/4015377> mDNS -F returns the same domain multiple times with different casing

Revision 1.198  2005/02/25 02:35:22  cheshire
<rdar://problem/4017292> Should not indicate successful dynamic update if no network connection
If we get NXDomain error looking for the _dns-update._udp record,
update status from 1 (in progress) to mStatus_NoSuchNameErr (failed)

Revision 1.197  2005/02/24 21:56:59  ksekar
Change LogMsgs to debugfs

Revision 1.196  2005/02/24 21:52:28  ksekar
<rdar://problem/3922768> Remove "deferred deregistration" logic for hostnames

Revision 1.195  2005/02/22 17:53:08  ksekar
Changed successful NAT Traversals from LogMsg to LogOperation

Revision 1.194  2005/02/15 18:38:03  ksekar
<rdar://problem/3967876> change expected/redundant log messages to debugfs.

Revision 1.193  2005/02/15 01:17:48  ksekar
Fixed build failure.

Revision 1.192  2005/02/14 23:01:28  ksekar
Refinement to previous checkin - don't log bad LLQ opcode if we had to send the request more than once.

Revision 1.191  2005/02/14 18:26:51  ksekar
<rdar://problem/4005569> mDNSResponder complains about bad LLQ Opcode 2

Revision 1.190  2005/02/11 19:44:06  shersche
Remove extra semicolon at end of line

Revision 1.189  2005/02/10 21:07:02  ksekar
Don't goto error in ReceiveNATAddrResponse if we receive a malformatted response

Revision 1.188  2005/02/10 02:02:44  ksekar
Remove double semi-colon

Revision 1.187  2005/02/09 23:28:01  ksekar
<rdar://problem/3984374> NAT-PMP response callback should return a
boolean indicating if the packet matched the request

Revision 1.186  2005/02/04 21:56:29  ksekar
<rdar://problem/3984374> Simultaneous port map requests sometimes fail
- Refinement to previous checkin.

Revision 1.185  2005/02/03 23:48:22  ksekar
<rdar://problem/3984374> Simultaneous port map requests sometimes fail

Revision 1.184  2005/02/01 19:33:29  ksekar
<rdar://problem/3985239> Keychain format too restrictive

Revision 1.183  2005/01/27 22:57:55  cheshire
Fix compile errors on gcc4

Revision 1.182  2005/01/25 18:55:05  ksekar
Shortened log message

Revision 1.181  2005/01/25 02:17:32  cheshire
<rdar://problem/3971263> Don't use query ID zero in uDNS queries

Revision 1.180  2005/01/19 21:01:54  ksekar
<rdar://problem/3955355> uDNS needs to support subtype registration and browsing

Revision 1.179  2005/01/19 19:15:35  ksekar
Refinement to <rdar://problem/3954575> - Simplify mDNS_PurgeResultsForDomain logic and move into daemon layer

Revision 1.178  2005/01/17 23:47:58  cheshire
<rdar://problem/3904954> Wide-area services not found on little-endian

Revision 1.177  2005/01/17 23:41:26  cheshire
Fix compile errors

Revision 1.176  2005/01/17 21:03:04  cheshire
<rdar://problem/3904954> Wide-area services not found on little-endian

Revision 1.175  2005/01/15 00:56:41  ksekar
<rdar://problem/3954575> Unicast services don't disappear when logging
out of VPN

Revision 1.174  2005/01/14 18:44:28  ksekar
<rdar://problem/3954609> mDNSResponder is crashing when changing domains

Revision 1.173  2005/01/14 18:34:22  ksekar
<rdar://problem/3954571> Services registered outside of firewall don't succeed after location change

Revision 1.172  2005/01/11 22:50:52  ksekar
Fixed constant naming (was using kLLQ_DefLease for update leases)

Revision 1.171  2005/01/10 04:52:49  ksekar
Changed LogMsg to debugf

Revision 1.170  2005/01/08 00:50:05  ksekar
Fixed spelling mistake in log msg

Revision 1.169  2005/01/08 00:42:18  ksekar
<rdar://problem/3922758> Clean up syslog messages

Revision 1.168  2004/12/23 23:22:47  ksekar
<rdar://problem/3933606> Unicast known answers "name" pointers point to garbage stack memory

Revision 1.167  2004/12/22 22:25:47  ksekar
<rdar://problem/3734265> NATPMP: handle location changes

Revision 1.166  2004/12/22 00:04:12  ksekar
<rdar://problem/3930324> mDNSResponder crashing in ReceivePortMapReply

Revision 1.165  2004/12/18 03:14:22  cheshire
DblNAT -> DoubleNAT

Revision 1.164  2004/12/17 03:55:40  ksekar
Don't use -1 as special meaning for expiration timer (it is a valid
value, and is redundant with our state variables)

Revision 1.163  2004/12/17 03:51:53  ksekar
<rdar://problem/3920991> Don't update TXT record if service registration fails

Revision 1.162  2004/12/17 01:29:11  ksekar
<rdar://problem/3920598> Questions can go deaf on location changes

Revision 1.161  2004/12/16 20:42:02  cheshire
Fix compiler warnings

Revision 1.160  2004/12/16 20:13:00  cheshire
<rdar://problem/3324626> Cache memory management improvements

Revision 1.159  2004/12/15 02:11:22  ksekar
<rdar://problem/3917317> Don't check for Dynamic DNS hostname uniqueness

Revision 1.158  2004/12/15 02:04:28  ksekar
Refinement to previous checkin - we should still return NatTraversal error  when the port mapping fails

Revision 1.157  2004/12/15 01:39:21  ksekar
Refinement to previous checkin - we should still return NatTraversal error  when the port mapping fails

Revision 1.156  2004/12/15 01:18:57  ksekar
<rdar://problem/3825979> Call DeregisterService on nat port map failure

Revision 1.155  2004/12/14 21:21:20  ksekar
<rdar://problem/3825979> NAT-PMP: Update response format to contain "Seconds Since Boot"

Revision 1.154  2004/12/14 20:52:27  cheshire
Add question->qnamehash and cr->resrec.namehash to log message

Revision 1.153  2004/12/14 20:45:02  cheshire
Improved error logging in "unexpected answer" message

Revision 1.152  2004/12/14 03:02:10  ksekar
<rdar://problem/3919016> Rare race condition can cause crash

Revision 1.151  2004/12/13 21:45:08  ksekar
uDNS_DeregisterService should return NoError if called twice (to follow mDNS behavior expected by daemon layer)

Revision 1.150  2004/12/13 20:42:41  ksekar
Fixed LogMsg

Revision 1.149  2004/12/13 18:10:03  ksekar
Fixed LogMsg

Revision 1.148  2004/12/13 01:18:04  ksekar
Fixed unused variable warning for non-debug builds

Revision 1.147  2004/12/12 23:51:42  ksekar
<rdar://problem/3845683> Wide-area registrations should fallback to using DHCP hostname as target

Revision 1.146  2004/12/12 23:30:40  ksekar
<rdar://problem/3916987> Extra RRs not properly unlinked when parent service registration fails

Revision 1.145  2004/12/12 22:56:29  ksekar
<rdar://problem/3668508> Need to properly handle duplicate long-lived queries

Revision 1.144  2004/12/11 20:55:29  ksekar
<rdar://problem/3916479> Clean up registration state machines

Revision 1.143  2004/12/10 01:21:27  cheshire
<rdar://problem/3914089> Get rid of "LLQ Responses over TCP not currently supported" message

Revision 1.142  2004/12/08 02:03:31  ksekar
<rdar://problem/3865124> Looping on NAT Traversal error - check for
NULL RR on error

Revision 1.141  2004/12/07 01:39:28  cheshire
Don't fail if the same server is responsible for more than one domain
(e.g. the same DNS server may be responsible for both apple.com. and 17.in-addr.arpa.)

Revision 1.140  2004/12/06 21:15:22  ksekar
<rdar://problem/3884386> mDNSResponder crashed in CheckServiceRegistrations

Revision 1.139  2004/12/06 19:08:03  cheshire
Add clarifying comment -- CountLabels() excludes the final root label.

Revision 1.138  2004/12/06 01:45:54  ksekar
Correct wording in LogMsg

Revision 1.137  2004/12/03 20:40:35  ksekar
<rdar://problem/3865124> Looping on NAT Traversal error

Revision 1.136  2004/12/03 07:20:50  ksekar
<rdar://problem/3674208> Wide-Area: Registration of large TXT record fails

Revision 1.135  2004/12/03 05:18:33  ksekar
<rdar://problem/3810596> mDNSResponder needs to return more specific TSIG errors

Revision 1.134  2004/12/02 20:03:49  ksekar
<rdar://problem/3889647> Still publishes wide-area domains even after switching to a local subnet

Revision 1.133  2004/12/02 18:37:52  ksekar
<rdar://problem/3758233> Registering with port number zero should not create a port mapping

Revision 1.132  2004/12/01 20:57:19  ksekar
<rdar://problem/3873921> Wide Area Service Discovery must be split-DNS aware

Revision 1.131  2004/12/01 19:59:27  cheshire
<rdar://problem/3882643> Crash in mDNSPlatformTCPConnect
If a TCP response has the TC bit set, don't respond by just trying another TCP connection

Revision 1.130  2004/12/01 02:43:23  cheshire
Don't call StatusCallback if function pointer is null

Revision 1.129  2004/11/30 23:51:06  cheshire
Remove double semicolons

Revision 1.128  2004/11/25 01:48:30  ksekar
<rdar://problem/3878991> Logging into VPN does not trigger registration of address record

Revision 1.127  2004/11/25 01:41:36  ksekar
Changed unnecessary LogMsgs to debugfs

Revision 1.126  2004/11/23 23:54:17  ksekar
<rdar://problem/3890318> Wide-Area DNSServiceRegisterRecord() failures
can crash mDNSResponder

Revision 1.125  2004/11/23 04:16:48  cheshire
Removed receiveMsg() routine.

Revision 1.124  2004/11/23 04:06:51  cheshire
Get rid of floating point constant -- in a small embedded device, bringing in all
the floating point libraries just to halve an integer value is a bit too heavyweight.

Revision 1.123  2004/11/22 17:16:20  ksekar
<rdar://problem/3854298> Unicast services don't disappear when you disable all networking

Revision 1.122  2004/11/19 18:00:34  ksekar
<rdar://problem/3682646> Security: use random ID for one-shot unicast queries

Revision 1.121  2004/11/19 04:24:08  ksekar
<rdar://problem/3682609> Security: Enforce a "window" on one-shot wide-area queries

Revision 1.120  2004/11/19 02:32:43  ksekar
<rdar://problem/3682608> Wide-Area Security: Add LLQ-ID to events

Revision 1.119  2004/11/18 23:21:24  ksekar
<rdar://problem/3764544> LLQ Security: Need to verify src port/address for LLQ handshake

Revision 1.118  2004/11/18 22:58:37  ksekar
Removed old comment.

Revision 1.117  2004/11/18 18:04:21  ksekar
Restore checkins lost due to repository disk failure: Update comments & <rdar://problem/3880688>

Revision 1.xxx  2004/11/17 06:17:57  cheshire
Update comments to show correct SRV names: _dns-update._udp.<zone>. and _dns-llq._udp.<zone>.

Revision 1.xxx  2004/11/17 00:45:28  ksekar
<rdar://problem/3880688> Result of putUpdateLease not error-checked

Revision 1.116  2004/11/16 01:41:47  ksekar
Fixed typo in debugf

Revision 1.115  2004/11/15 20:09:24  ksekar
<rdar://problem/3719050> Wide Area support for Add/Remove record

Revision 1.114  2004/11/13 02:32:47  ksekar
<rdar://problem/3868216> LLQ mobility fragile on non-primary interface
- fixed incorrect state comparison in CheckQueries

Revision 1.113  2004/11/13 02:29:52  ksekar
<rdar://problem/3878386> LLQ refreshes not reliable

Revision 1.112  2004/11/11 20:45:14  ksekar
<rdar://problem/3876052> self-conflict test not compatible with some BIND servers

Revision 1.111  2004/11/11 20:14:55  ksekar
<rdar://problem/3719574> Wide-Area registrations not deregistered on sleep

Revision 1.110  2004/11/10 23:53:53  ksekar
Remove no longer relevant comment

Revision 1.109  2004/11/10 20:40:53  ksekar
<rdar://problem/3868216> LLQ mobility fragile on non-primary interface

Revision 1.108  2004/11/01 20:36:16  ksekar
<rdar://problem/3802395> mDNSResponder should not receive Keychain Notifications

Revision 1.107  2004/10/26 06:11:41  cheshire
Add improved logging to aid in diagnosis of <rdar://problem/3842714> mDNSResponder crashed

Revision 1.106  2004/10/26 03:52:03  cheshire
Update checkin comments

Revision 1.105  2004/10/26 01:15:06  cheshire
Use "#if 0" instead of commenting out code

Revision 1.104  2004/10/25 21:41:38  ksekar
<rdar://problem/3852958> wide-area name conflicts can cause crash

Revision 1.103  2004/10/25 19:30:52  ksekar
<rdar://problem/3827956> Simplify dynamic host name structures

Revision 1.102  2004/10/23 01:16:00  cheshire
<rdar://problem/3851677> uDNS operations not always reliable on multi-homed hosts

Revision 1.101  2004/10/22 20:52:07  ksekar
<rdar://problem/3799260> Create NAT port mappings for Long Lived Queries

Revision 1.100  2004/10/20 02:16:41  cheshire
Improve "could not confirm existence of NS record" error message
Don't call newRR->RecordCallback if it is NULL

Revision 1.99  2004/10/19 21:33:18  cheshire
<rdar://problem/3844991> Cannot resolve non-local registrations using the mach API
Added flag 'kDNSServiceFlagsForceMulticast'. Passing through an interface id for a unicast name
doesn't force multicast unless you set this flag to indicate explicitly that this is what you want

Revision 1.98  2004/10/16 00:16:59  cheshire
<rdar://problem/3770558> Replace IP TTL 255 check with local subnet source address check

Revision 1.97  2004/10/15 23:00:18  ksekar
<rdar://problem/3799242> Need to update LLQs on location changes

Revision 1.96  2004/10/12 23:30:44  ksekar
<rdar://problem/3609944> mDNSResponder needs to follow CNAME referrals

Revision 1.95  2004/10/12 03:15:09  ksekar
<rdar://problem/3835612> mDNS_StartQuery shouldn't return transient no-server error

Revision 1.94  2004/10/12 02:49:20  ksekar
<rdar://problem/3831228> Clean up LLQ sleep/wake, error handling

Revision 1.93  2004/10/08 04:17:25  ksekar
<rdar://problem/3831819> Don't use DNS extensions if the server does not advertise required SRV record

Revision 1.92  2004/10/08 03:54:35  ksekar
<rdar://problem/3831802> Refine unicast polling intervals

Revision 1.91  2004/09/30 17:45:34  ksekar
<rdar://problem/3821119> lots of log messages: mDNS_SetPrimaryIP: IP address unchanged

Revision 1.90  2004/09/25 00:22:13  ksekar
<rdar://problem/3815534> Crash in uDNS_RegisterService

Revision 1.89  2004/09/24 19:14:53  cheshire
Remove unused "extern mDNS mDNSStorage"

Revision 1.88  2004/09/23 20:48:15  ksekar
Clarify retransmission debugf messages.

Revision 1.87  2004/09/22 00:41:59  cheshire
Move tcp connection status codes into the legal range allocated for mDNS use

Revision 1.86  2004/09/21 23:40:11  ksekar
<rdar://problem/3810349> mDNSResponder to return errors on NAT traversal failure

Revision 1.85  2004/09/21 22:38:27  ksekar
<rdar://problem/3810286> PrimaryIP type uninitialized

Revision 1.84  2004/09/18 00:30:39  cheshire
<rdar://problem/3806643> Infinite loop in CheckServiceRegistrations

Revision 1.83  2004/09/17 00:31:51  cheshire
For consistency with ipv6, renamed rdata field 'ip' to 'ipv4'

Revision 1.82  2004/09/16 21:36:36  cheshire
<rdar://problem/3803162> Fix unsafe use of mDNSPlatformTimeNow()
Changes to add necessary locking calls around unicast DNS operations

Revision 1.81  2004/09/16 02:29:39  cheshire
Moved mDNS_Lock/mDNS_Unlock to DNSCommon.c; Added necessary locking around
uDNS_ReceiveMsg, uDNS_StartQuery, uDNS_UpdateRecord, uDNS_RegisterService

Revision 1.80  2004/09/16 01:58:21  cheshire
Fix compiler warnings

Revision 1.79  2004/09/16 00:24:48  cheshire
<rdar://problem/3803162> Fix unsafe use of mDNSPlatformTimeNow()

Revision 1.78  2004/09/15 01:16:57  ksekar
<rdar://problem/3797598> mDNSResponder printing too many messages

Revision 1.77  2004/09/14 23:27:47  cheshire
Fix compile errors

Revision 1.76  2004/09/14 22:22:00  ksekar
<rdar://problem/3800920> Legacy browses broken against some BIND versions

Revision 1.75  2004/09/03 19:23:05  ksekar
<rdar://problem/3788460>: Need retransmission mechanism for wide-area service registrations

Revision 1.74  2004/09/02 17:49:04  ksekar
<rdar://problem/3785135>: 8A246: mDNSResponder crash while logging on restart
Fixed incorrect conversions, changed %s to %##s for all domain names.

Revision 1.73  2004/09/02 01:39:40  cheshire
For better readability, follow consistent convention that QR bit comes first, followed by OP bits

Revision 1.72  2004/09/01 03:59:29  ksekar
<rdar://problem/3783453>: Conditionally compile out uDNS code on Windows

Revision 1.71  2004/08/27 17:51:53  ksekar
Replaced unnecessary LogMsg with debugf.

Revision 1.70  2004/08/25 00:37:27  ksekar
<rdar://problem/3774635>: Cleanup DynDNS hostname registration code

Revision 1.69  2004/08/18 17:35:41  ksekar
<rdar://problem/3651443>: Feature #9586: Need support for Legacy NAT gateways

Revision 1.68  2004/08/14 03:22:41  cheshire
<rdar://problem/3762579> Dynamic DNS UI <-> mDNSResponder glue
Add GetUserSpecifiedDDNSName() routine
Convert ServiceRegDomain to domainname instead of C string
Replace mDNS_GenerateFQDN/mDNS_GenerateGlobalFQDN with mDNS_SetFQDNs

Revision 1.67  2004/08/13 23:46:58  cheshire
"asyncronous" -> "asynchronous"

Revision 1.66  2004/08/13 23:37:02  cheshire
Now that we do both uDNS and mDNS, global replace "uDNS_info.hostname" with
"uDNS_info.UnicastHostname" for clarity

Revision 1.65  2004/08/13 23:12:32  cheshire
Don't use strcpy() and strlen() on "struct domainname" objects;
use AssignDomainName() and DomainNameLength() instead
(A "struct domainname" is a collection of packed pascal strings, not a C string.)

Revision 1.64  2004/08/13 23:01:05  cheshire
Use platform-independent mDNSNULL instead of NULL

Revision 1.63  2004/08/12 00:32:36  ksekar
<rdar://problem/3759567>: LLQ Refreshes never terminate if unanswered

Revision 1.62  2004/08/10 23:19:14  ksekar
<rdar://problem/3722542>: DNS Extension daemon for Wide Area Service Discovery
Moved routines/constants to allow extern access for garbage collection daemon

Revision 1.61  2004/07/30 17:40:06  ksekar
<rdar://problem/3739115>: TXT Record updates not available for wide-area services

Revision 1.60  2004/07/29 19:40:05  ksekar
NATPMP Support - minor fixes and cleanup

Revision 1.59  2004/07/29 19:27:15  ksekar
NATPMP Support - minor fixes and cleanup

Revision 1.58  2004/07/27 07:35:38  shersche
fix syntax error, variables declared in the middle of a block

Revision 1.57  2004/07/26 22:49:30  ksekar
<rdar://problem/3651409>: Feature #9516: Need support for NATPMP in client

Revision 1.56  2004/07/26 19:14:44  ksekar
<rdar://problem/3737814>: 8A210: mDNSResponder crashed in startLLQHandshakeCallback

Revision 1.55  2004/07/15 19:01:33  ksekar
<rdar://problem/3681029>: Check for incorrect time comparisons

Revision 1.54  2004/06/22 02:10:53  ksekar
<rdar://problem/3705433>: Lighthouse failure causes packet flood to DNS

Revision 1.53  2004/06/17 20:49:09  ksekar
<rdar://problem/3690436>: mDNSResponder crash while location cycling

Revision 1.52  2004/06/17 01:13:11  ksekar
<rdar://problem/3696616>: polling interval too short

Revision 1.51  2004/06/10 04:36:44  cheshire
Fix compiler warning

Revision 1.50  2004/06/10 00:55:13  ksekar
<rdar://problem/3686213>: crash on network reconnect

Revision 1.49  2004/06/10 00:10:50  ksekar
<rdar://problem/3686174>: Infinite Loop in uDNS_Execute()

Revision 1.48  2004/06/09 20:03:37  ksekar
<rdar://problem/3686163>: Incorrect copying of resource record in deregistration

Revision 1.47  2004/06/09 03:48:28  ksekar
<rdar://problem/3685226>: nameserver address fails with prod. Lighthouse server

Revision 1.46  2004/06/09 01:44:30  ksekar
<rdar://problem/3681378> reworked Cache Record copy code

Revision 1.45  2004/06/08 18:54:47  ksekar
<rdar://problem/3681378>: mDNSResponder leaks after exploring in Printer Setup Utility

Revision 1.44  2004/06/05 00:33:51  cheshire
<rdar://problem/3681029>: Check for incorrect time comparisons

Revision 1.43  2004/06/05 00:14:44  cheshire
Fix signed/unsigned and other compiler warnings

Revision 1.42  2004/06/04 22:36:16  ksekar
Properly set u->nextevent in uDNS_Execute

Revision 1.41  2004/06/04 08:58:29  ksekar
<rdar://problem/3668624>: Keychain integration for secure dynamic update

Revision 1.40  2004/06/03 03:09:58  ksekar
<rdar://problem/3668626>: Garbage Collection for Dynamic Updates

Revision 1.39  2004/06/01 23:46:50  ksekar
<rdar://problem/3675149>: DynDNS: dynamically look up LLQ/Update ports

Revision 1.38  2004/05/31 22:19:44  ksekar
<rdar://problem/3258021>: Feature: DNS server->client notification on
record changes (#7805) - revert to polling mode on setup errors

Revision 1.37  2004/05/28 23:42:37  ksekar
<rdar://problem/3258021>: Feature: DNS server->client notification on record changes (#7805)

Revision 1.36  2004/05/18 23:51:25  cheshire
Tidy up all checkin comments to use consistent "<rdar://problem/xxxxxxx>" format for bug numbers

Revision 1.35  2004/05/07 23:01:04  ksekar
Cleaned up list traversal in deriveGoodbyes - removed unnecessary
conditional assignment.

Revision 1.34  2004/05/05 18:26:12  ksekar
Periodically re-transmit questions if the send() fails.  Include
internal questions in retransmission.

Revision 1.33  2004/05/05 17:40:06  ksekar
Removed prerequisite from deregistration update - it does not work for
shared records, and is unnecessary until we have more sophisticated
name conflict management.

Revision 1.32  2004/05/05 17:32:18  ksekar
Prevent registration of loopback interface caused by removal of
Multicast flag in interface structure.

Revision 1.31  2004/05/05 17:05:02  ksekar
Use LargeCacheRecord structs when pulling records off packets

Revision 1.30  2004/04/16 21:33:27  ksekar
Fixed bug in processing GetZoneData responses that do not use BIND formatting.

Revision 1.29  2004/04/15 20:03:13  ksekar
Clarified log message when pulling bad resource records off packet.

Revision 1.28  2004/04/15 00:51:28  bradley
Minor tweaks for Windows and C++ builds. Added casts for signed/unsigned integers and 64-bit pointers.
Prefix some functions with mDNS to avoid conflicts. Disable benign warnings on Microsoft compilers.

Revision 1.27  2004/04/14 23:09:28  ksekar
Support for TSIG signed dynamic updates.

Revision 1.26  2004/04/14 19:36:05  ksekar
Fixed memory corruption error in deriveGoodbyes.

Revision 1.25  2004/04/14 04:07:11  ksekar
Fixed crash in IsActiveUnicastQuery().  Removed redundant checks in routine.

Revision 1.24  2004/04/08 09:41:40  bradley
Added const to AuthRecord in deadvertiseIfCallback to match callback typedef.

Revision 1.23  2004/03/24 00:29:45  ksekar
Make it safe to call StopQuery in a unicast question callback

Revision 1.22  2004/03/19 10:11:09  bradley
Added AuthRecord * cast from umalloc for C++ builds.

Revision 1.21  2004/03/15 02:03:45  bradley
Added const to params where needed to match prototypes. Changed SetNewRData calls to use 0 instead
of -1 for unused size to fix warning. Disable assignment within conditional warnings with Visual C++.

Revision 1.20  2004/03/13 02:07:26  ksekar
<rdar://problem/3192546>: DynDNS: Dynamic update of service records

Revision 1.19  2004/03/13 01:57:33  ksekar
<rdar://problem/3192546>: DynDNS: Dynamic update of service records

Revision 1.18  2004/02/21 08:34:15  bradley
Added casts from void * to specific type for C++ builds. Changed void * l-value cast
r-value cast to fix problems with VC++ builds. Removed empty switch to fix VC++ error.

Revision 1.17  2004/02/21 02:06:24  cheshire
Can't use anonymous unions -- they're non-standard and don't work on all compilers

Revision 1.16  2004/02/12 01:51:45  cheshire
Don't try to send uDNS queries unless we have at least one uDNS server available

Revision 1.15  2004/02/10 03:02:46  cheshire
Fix compiler warning

Revision 1.14  2004/02/06 23:04:19  ksekar
Basic Dynamic Update support via mDNS_Register (dissabled via
UNICAST_REGISTRATION #define)

Revision 1.13  2004/02/03 22:15:01  ksekar
Fixed nameToAddr error check: don't abort state machine on nxdomain error.

Revision 1.12  2004/02/03 19:47:36  ksekar
Added an asynchronous state machine mechanism to uDNS.c, including
calls to find the parent zone for a domain name.  Changes include code
in repository previously dissabled via "#if 0 incomplete".  Codepath
is currently unused, and will be called to create update records, etc.

Revision 1.11  2004/01/30 02:12:30  ksekar
Changed uDNS_ReceiveMsg() to correctly return void.

Revision 1.10  2004/01/29 02:59:17  ksekar
Unicast DNS: Changed from a resource record oriented question/response
matching to packet based matching.  New callback architecture allows
collections of records in a response to be processed differently
depending on the nature of the request, and allows the same structure
to be used for internal and client-driven queries with different processing needs.

Revision 1.9  2004/01/28 20:20:45  ksekar
Unified ActiveQueries and ActiveInternalQueries lists, using a flag to
demux them.  Check-in includes work-in-progress code, #ifdef'd out.

Revision 1.8  2004/01/28 02:30:07  ksekar
Added default Search Domains to unicast browsing, controlled via
Networking sharing prefs pane.  Stopped sending unicast messages on
every interface.  Fixed unicast resolving via mach-port API.

Revision 1.7  2004/01/27 20:15:22  cheshire
<rdar://problem/3541288>: Time to prune obsolete code for listening on port 53

Revision 1.6  2004/01/24 23:47:17  cheshire
Use mDNSOpaque16fromIntVal() instead of shifting and masking

Revision 1.5  2004/01/24 04:59:15  cheshire
Fixes so that Posix/Linux, OS9, Windows, and VxWorks targets build again

Revision 1.4  2004/01/24 04:19:26  cheshire
Restore overwritten checkin 1.2

Revision 1.3  2004/01/23 23:23:15  ksekar
Added TCP support for truncated unicast messages.

Revision 1.2  2004/01/22 03:48:41  cheshire
Make sure uDNS client doesn't accidentally use query ID zero

Revision 1.1  2003/12/13 03:05:27  ksekar
<rdar://problem/3192548>: DynDNS: Unicast query of service records

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

#define umalloc(x)         mDNSPlatformMemAllocate(x)       // short hands for common routines
#define ufree(x)           mDNSPlatformMemFree(x)
#define ubzero(x,y)        mDNSPlatformMemZero(x,y)
#define umemcpy(x, y, l)   mDNSPlatformMemCopy(y, x, l)  // uses memcpy(2) arg ordering

// Asynchronous operation types

typedef enum
	{
	zoneDataResult
	// other async. operation names go here
	} AsyncOpResultType;

typedef struct
	{
    domainname zoneName;
    mDNSAddr primaryAddr;
    mDNSu16 zoneClass;
    mDNSIPPort llqPort;
    mDNSIPPort updatePort;
	} zoneData_t;

// other async. result struct defs go here

typedef struct
	{
    AsyncOpResultType type;
    zoneData_t zoneData;
    // other async result structs go here
	} AsyncOpResult;

typedef void AsyncOpCallback(mStatus err, mDNS *const m, void *info, const AsyncOpResult *result);


// Private Function Prototypes
// Note:  In general, functions are ordered such that they do not require forward declarations.
// However, prototypes are used where cyclic call graphs exist (e.g. foo calls bar, and bar calls
// foo), or when they aid in the grouping or readability of code (e.g. state machine code that is easier
// read top-to-bottom.)

mDNSlocal mDNSBool FreeNATInfo(mDNS *m, NATTraversalInfo *n);
mDNSlocal void hndlTruncatedAnswer(DNSQuestion *question,  const mDNSAddr *src, mDNS *m);
mDNSlocal mStatus startGetZoneData(domainname *name, mDNS *m, mDNSBool findUpdatePort, mDNSBool findLLQPort, AsyncOpCallback callback, void *callbackInfo);
mDNSlocal mDNSBool recvLLQResponse(mDNS *m, DNSMessage *msg, const mDNSu8 *end, const mDNSAddr *srcaddr, mDNSIPPort srcport, const mDNSInterfaceID InterfaceID);
mDNSlocal void sendRecordRegistration(mDNS *const m, AuthRecord *rr);
mDNSlocal void SendServiceRegistration(mDNS *m, ServiceRecordSet *srs);
mDNSlocal void SendServiceDeregistration(mDNS *m, ServiceRecordSet *srs);
mDNSlocal void serviceRegistrationCallback(mStatus err, mDNS *const m, void *srsPtr, const AsyncOpResult *result);
mDNSlocal void SuspendLLQs(mDNS *m, mDNSBool DeregisterActive);
mDNSlocal void RestartQueries(mDNS *m);
mDNSlocal void startLLQHandshake(mDNS *m, LLQ_Info *info, mDNSBool defer);
mDNSlocal void llqResponseHndlr(mDNS * const m, DNSMessage *msg, const  mDNSu8 *end, DNSQuestion *question, void *context);

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Temporary workaround
#endif

// 17 Places in this file directly call mDNSPlatformTimeNow(), which is unsafe
// The platform function is now called mDNSPlatformRawTime(), and
// mDNSPlatformTimeNow() is defined here as a temporary workaround.
// This is a gross hack, and after this change has been tested for a while,
// all these calls should be replaced by simple references to m->timenow

mDNSlocal mDNSs32 mDNSPlatformTimeNow(mDNS *m)
	{
	if (m->mDNS_busy && m->timenow) return(m->timenow);
	LogMsg("ERROR: uDNS.c code executing without holding main mDNS lock");

	// To get a quick and easy stack trace to find out *how* this routine
	// is being called without holding main mDNS lock, uncomment the line below:
	// *(long*)0=0;
	
	return(mDNS_TimeNow(m));
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - General Utility Functions
#endif

// CountLabels() returns number of labels in name, excluding final root label
// (e.g. for "apple.com." CountLabels returns 2.)
mDNSlocal int CountLabels(const domainname *d)
	{
	int count = 0;
	const mDNSu8 *ptr;
	
	for (ptr = d->c; *ptr; ptr = ptr + ptr[0] + 1) count++;
	return count;
	}

mDNSlocal mDNSOpaque16 newMessageID(uDNS_GlobalInfo *u)
	{
	static mDNSBool randomized = mDNSfalse;

	if (!randomized) { u->NextMessageID = (mDNSu16)mDNSRandom(0xFFFF); randomized = mDNStrue; }
	if (u->NextMessageID == 0) u->NextMessageID++;
	return mDNSOpaque16fromIntVal(u->NextMessageID++);
	}

// unlink an AuthRecord from a linked list
mDNSlocal mStatus unlinkAR(AuthRecord **list, AuthRecord *const rr)
	{
	while (*list && *list != rr) list = &(*list)->next;
	if (*list) { *list = rr->next; rr->next = mDNSNULL; return(mStatus_NoError); }
	LogMsg("ERROR: unlinkAR - no such active record %##s", rr->resrec.name->c);
	return(mStatus_NoSuchRecord);
	}

mDNSlocal void unlinkSRS(mDNS *m, ServiceRecordSet *srs)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	ServiceRecordSet **p;
	NATTraversalInfo *n = u->NATTraversals;

	// verify that no NAT objects reference this service
	while (n)
		{
		if (n->reg.ServiceRegistration == srs)
			{
			NATTraversalInfo *tmp = n;
			n = n->next;
			LogMsg("ERROR: Unlinking service record set %##s still referenced by NAT traversal object!", srs->RR_SRV.resrec.name->c);
			FreeNATInfo(m, tmp);
			}
		else n = n->next;
		}
	
	for (p = &u->ServiceRegistrations; *p; p = &(*p)->next)
		if (*p == srs)
			{
			ExtraResourceRecord *e;
			*p = srs->next;
			srs->next = mDNSNULL;
			for (e=srs->Extras; e; e=e->next)
				if (unlinkAR(&u->RecordRegistrations, &e->r))
					LogMsg("unlinkSRS: extra record %##s not found", e->r.resrec.name->c);
			return;
			}
	LogMsg("ERROR: unlinkSRS - SRS not found in ServiceRegistrations list %##s", srs->RR_SRV.resrec.name->c);
	}

mDNSlocal void LinkActiveQuestion(uDNS_GlobalInfo *u, DNSQuestion *q)
	{
	if (uDNS_IsActiveQuery(q, u))
		{ LogMsg("LinkActiveQuestion - %##s (%d) already in list!", q->qname.c, q->qtype); return; }
	
	q->next = u->ActiveQueries;
	u->ActiveQueries = q;
	}

// set retry timestamp for record with exponential backoff
// (for service record sets, use RR_SRV as representative for time checks
mDNSlocal void SetRecordRetry(mDNS *const m, AuthRecord *rr, mStatus SendErr)
	{
	rr->LastAPTime = mDNSPlatformTimeNow(m);
	if (SendErr == mStatus_TransientErr || rr->ThisAPInterval < INIT_UCAST_POLL_INTERVAL)  rr->ThisAPInterval = INIT_UCAST_POLL_INTERVAL;
	else if (rr->ThisAPInterval*2 <= MAX_UCAST_POLL_INTERVAL)                              rr->ThisAPInterval *= 2;
	else if (rr->ThisAPInterval != MAX_UCAST_POLL_INTERVAL)                                rr->ThisAPInterval = MAX_UCAST_POLL_INTERVAL;
	}
	

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Name Server List Management
#endif

mDNSexport void mDNS_AddDNSServer(mDNS *const m, const mDNSAddr *addr, const domainname *d)
    {
    uDNS_GlobalInfo *u = &m->uDNS_info;
	DNSServer *s, **p = &u->Servers;
	
	mDNS_Lock(m);
	if (!d) d = (domainname *)"";

	while (*p)                 // Check if we already have this {server,domain} pair registered
		{
		if (mDNSSameAddress(&(*p)->addr, addr) && SameDomainName(&(*p)->domain, d))
			LogMsg("Note: DNS Server %#a for domain %##s registered more than once", addr, d->c);
		p=&(*p)->next;
		}

	// allocate, add to list
	s = umalloc(sizeof(*s));
	if (!s) { LogMsg("Error: mDNS_AddDNSServer - malloc"); goto end; }
	s->addr      = *addr;
	s->del       = mDNSfalse;
	s->teststate = DNSServer_Untested;
	AssignDomainName(&s->domain, d);
	s->next = mDNSNULL;
	*p = s;
	
	end:
	mDNS_Unlock(m);
    }

mDNSexport void mDNS_DeleteDNSServers(mDNS *const m)
    {
	DNSServer *s;
	mDNS_Lock(m);

	s = m->uDNS_info.Servers;
	m->uDNS_info.Servers = mDNSNULL;
	while (s)
		{
		DNSServer *tmp = s;
		s = s->next;
		ufree(tmp);
		}

	mDNS_Unlock(m);
    }

 // ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - authorization management
#endif

mDNSlocal uDNS_AuthInfo *GetAuthInfoForName(const uDNS_GlobalInfo *u, const domainname *name)
	{
	uDNS_AuthInfo *ptr;
	while (name->c[0])
		{
		for (ptr = u->AuthInfoList; ptr; ptr = ptr->next)
			if (SameDomainName(&ptr->zone, name)) return(ptr);
		name = (const domainname *)(name->c + 1 + name->c[0]);
		}
	return mDNSNULL;
	}

mDNSlocal void DeleteAuthInfoForZone(uDNS_GlobalInfo *u, const domainname *zone)
	{
	uDNS_AuthInfo *ptr, *prev = mDNSNULL;

	for (ptr = u->AuthInfoList; ptr; ptr = ptr->next)
		{
		if (SameDomainName(&ptr->zone, zone))
			{
			if (prev) prev->next = ptr->next;
			else u->AuthInfoList = ptr->next;
			ufree(ptr);
			return;
			}
		prev = ptr;
		}
	}

mDNSexport mStatus mDNS_SetSecretForZone(mDNS *m, const domainname *zone, const domainname *key, const char *sharedSecret)
	{
	uDNS_AuthInfo *info;
	mDNSu8 keybuf[1024];
	mDNSs32 keylen;
	uDNS_GlobalInfo *u = &m->uDNS_info;
	mStatus status = mStatus_NoError;

	mDNS_Lock(m);
	
	if (GetAuthInfoForName(u, zone)) DeleteAuthInfoForZone(u, zone);
	if (!key) goto exit;
	
	info = (uDNS_AuthInfo*)umalloc(sizeof(*info));
	if (!info) { LogMsg("ERROR: umalloc"); status = mStatus_NoMemoryErr; goto exit; }
   	ubzero(info, sizeof(*info));
	AssignDomainName(&info->zone, zone);
	AssignDomainName(&info->keyname, key);

	keylen = DNSDigest_Base64ToBin(sharedSecret, keybuf, 1024);
	if (keylen < 0)
		{
		LogMsg("ERROR: mDNS_SetSecretForZone - could not convert shared secret %s from base64", sharedSecret);
		ufree(info);
		status = mStatus_UnknownErr;
		goto exit;
		}
	DNSDigest_ConstructHMACKey(info, keybuf, (mDNSu32)keylen);

    // link into list
	info->next = m->uDNS_info.AuthInfoList;
	m->uDNS_info.AuthInfoList = info;
exit:
	mDNS_Unlock(m);
	return status;
	}
	
 // ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - NAT Traversal
#endif

mDNSlocal mDNSBool DomainContainsLabelString(const domainname *d, const char *str)
	{
	const domainlabel *l;
	domainlabel buf;

	if (!MakeDomainLabelFromLiteralString(&buf, str)) return mDNSfalse;

	for (l = (const domainlabel *)d; l->c[0]; l = (const domainlabel *)(l->c + l->c[0]+1))
		if (SameDomainLabel(l->c, buf.c)) return mDNStrue;
	return mDNSfalse;
	}

// allocate struct, link into global list, initialize
mDNSlocal NATTraversalInfo *AllocNATInfo(mDNS *const m, NATOp_t op, NATResponseHndlr callback)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	NATTraversalInfo *info = umalloc(sizeof(NATTraversalInfo));
	if (!info) { LogMsg("ERROR: malloc"); return mDNSNULL; }
	ubzero(info, sizeof(NATTraversalInfo));
	info->next = u->NATTraversals;
	u->NATTraversals = info;
	info->retry = mDNSPlatformTimeNow(m) + NATMAP_INIT_RETRY;
	info->op = op;
	info->state = NATState_Init;
	info->ReceiveResponse = callback;
	info->PublicPort.NotAnInteger = 0;
	info->Router = u->Router;
	return info;
	}

// unlink from list, deallocate
mDNSlocal mDNSBool FreeNATInfo(mDNS *m, NATTraversalInfo *n)
	{
	NATTraversalInfo *ptr, *prev = mDNSNULL;
	ServiceRecordSet *s = m->uDNS_info.ServiceRegistrations;

	// Verify that object is not referenced by any services
	while (s)
		{
		if (s->uDNS_info.NATinfo == n)
			{
			LogMsg("Error: Freeing NAT info object still referenced by Service Record Set %##s!", s->RR_SRV.resrec.name->c);
			s->uDNS_info.NATinfo = mDNSNULL;
			}
		s = s->next;
		}
	
	if (n == m->uDNS_info.LLQNatInfo) m->uDNS_info.LLQNatInfo = mDNSNULL;
	ptr = m->uDNS_info.NATTraversals;
	while (ptr)
		{
		if (ptr == n)
			{
			if (prev) prev->next = ptr->next;
			else m->uDNS_info.NATTraversals = ptr->next;
			ufree(n);
			return mDNStrue;
			}
		prev = ptr;
		ptr = ptr->next;
		}
	LogMsg("FreeNATInfo: NATTraversalInfo not found in list");
	return mDNSfalse;
	}

mDNSlocal void SendNATMsg(NATTraversalInfo *info, mDNS *m)
	{
	mStatus err;
	uDNS_GlobalInfo *u = &m->uDNS_info;

	if (info->state != NATState_Request && info->state != NATState_Refresh)
		{ LogMsg("SendNATMsg: Bad state %d", info->state); return; }

	if (u->Router.ip.v4.NotAnInteger)
		{
		// send msg if we have a router
		const mDNSu8 *end = (mDNSu8 *)&info->request;
		if (info->op == NATOp_AddrRequest) end += sizeof(NATAddrRequest);
		else end += sizeof(NATPortMapRequest);

		err = mDNSPlatformSendUDP(m, &info->request, end, 0, &u->Router, NATPMPPort);
		if (!err) (info->ntries++);  // don't increment attempt counter if the send failed
		}
	
	// set retry
	if (info->RetryInterval < NATMAP_INIT_RETRY) info->RetryInterval = NATMAP_INIT_RETRY;
	else if (info->RetryInterval * 2 > NATMAP_MAX_RETRY) info->RetryInterval = NATMAP_MAX_RETRY;
	else info->RetryInterval *= 2;
	info->retry = mDNSPlatformTimeNow(m) + info->RetryInterval;
	}

mDNSlocal mDNSBool ReceiveNATAddrResponse(NATTraversalInfo *n, mDNS *m, mDNSu8 *pkt, mDNSu16 len)
	{
	mStatus err = mStatus_NoError;
	AuthRecord *rr = mDNSNULL;
	NATAddrReply *response = (NATAddrReply *)pkt;
	mDNSAddr addr;

	if (n->state != NATState_Request)
		{
		LogMsg("ReceiveNATAddrResponse: bad state %d", n->state);
		return mDNSfalse;
		}
		
	rr = n->reg.RecordRegistration;
	if (!rr)
		{
		LogMsg("ReceiveNATAddrResponse: registration cancelled");
		return mDNSfalse;
		}

	addr.type = mDNSAddrType_IPv4;
	addr.ip.v4 = rr->resrec.rdata->u.ipv4;

	if (!pkt) // timeout
		{
#ifdef _LEGACY_NAT_TRAVERSAL_
		err = LNT_GetPublicIP(&addr.ip.v4);
		if (err) goto end;
		else n->state = NATState_Legacy;
#else
		debugf("ReceiveNATAddrResponse: timeout");
		err = mStatus_NATTraversal;
		goto end;
#endif // _LEGACY_NAT_TRAVERSAL_
		}
	else
		{
		if (len < sizeof(*response))
			{
			LogMsg("ReceiveNATAddrResponse: response too short (%d bytes)", len);
			return mDNSfalse;
			}
		if (response->vers != NATMAP_VERS)
			{
			LogMsg("ReceiveNATAddrResponse: received  version %d (expect version %d)", pkt[0], NATMAP_VERS);
			return mDNSfalse;
			}
		if (response->opcode != (NATOp_AddrRequest | NATMAP_RESPONSE_MASK))
			{
			LogMsg("ReceiveNATAddrResponse: bad response code %d", response->opcode);
			return mDNSfalse;
			}
		if (response->err.NotAnInteger)
			{ LogMsg("ReceiveAddrResponse: received error %d", mDNSVal16(response->err)); err = mStatus_NATTraversal; goto end; }

		addr.ip.v4 = response->PubAddr;
		n->state = NATState_Established;
		}
	
	if (IsPrivateV4Addr(&addr))
		{
		LogMsg("ReceiveNATAddrResponse: Double NAT");
		err = mStatus_DoubleNAT;
		goto end;
		}
	
	end:
	if (err)
		{
		FreeNATInfo(m, n);
		if (rr)
			{
			rr->uDNS_info.NATinfo = mDNSNULL;
			rr->uDNS_info.state = regState_Unregistered;    // note that rr is not yet in global list
			rr->RecordCallback(m, rr, mStatus_NATTraversal);
			// note - unsafe to touch rr after callback
			}
		return mDNStrue;
		}
	else LogOperation("Received public IP address %d.%d.%d.%d from NAT.", addr.ip.v4.b[0], addr.ip.v4.b[1], addr.ip.v4.b[2], addr.ip.v4.b[3]);
	rr->resrec.rdata->u.ipv4 = addr.ip.v4;  // replace rdata w/ public address
	uDNS_RegisterRecord(m, rr);
	return mDNStrue;
	}


mDNSlocal void StartGetPublicAddr(mDNS *m, AuthRecord *AddressRec)
	{
	NATAddrRequest *req;
	uDNS_GlobalInfo *u = &m->uDNS_info;
	
	NATTraversalInfo *info = AllocNATInfo(m, NATOp_AddrRequest, ReceiveNATAddrResponse);
	if (!info) { uDNS_RegisterRecord(m, AddressRec); return; }
	AddressRec->uDNS_info.NATinfo = info;
	info->reg.RecordRegistration = AddressRec;
	info->state = NATState_Request;
	
    // format message
	req = &info->request.AddrReq;
	req->vers = NATMAP_VERS;
	req->opcode = NATOp_AddrRequest;
	
	if (!u->Router.ip.v4.NotAnInteger)
		{
		debugf("No router.  Will retry NAT traversal in %ld ticks", NATMAP_INIT_RETRY);
		return;
		}
   
	SendNATMsg(info, m);
	}


mDNSlocal void RefreshNATMapping(NATTraversalInfo *n, mDNS *m)
	{
	n->state = NATState_Refresh;
	n->RetryInterval = NATMAP_INIT_RETRY;
	n->ntries = 0;
	SendNATMsg(n, m);
	}

mDNSlocal void LLQNatMapComplete(mDNS *m)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	LLQ_Info *llqInfo;
	NATTraversalInfo *n = u->LLQNatInfo;
	
	if (!n) { LogMsg("Error: LLQNatMapComplete called with NULL LLQNatInfo"); return; }
	if (n->state != NATState_Established && n->state != NATState_Legacy && n->state != NATState_Error)
		{ LogMsg("LLQNatMapComplete - bad nat state %d", n->state); return; }

	u->CurrentQuery = u->ActiveQueries;
	while (u->CurrentQuery)
		{
		DNSQuestion *q = u->CurrentQuery;
		u->CurrentQuery = u->CurrentQuery->next;
		llqInfo = q->uDNS_info.llq;
		if (q->LongLived && llqInfo->state == LLQ_NatMapWait)
			{
			if (n->state == NATState_Error)
				{
				llqInfo->NATMap = mDNSfalse;
				llqInfo->question->uDNS_info.responseCallback = llqResponseHndlr;
				llqInfo->state = LLQ_Poll;
				llqInfo->question->LastQTime = mDNSPlatformTimeNow(m) - (2 * INIT_UCAST_POLL_INTERVAL);  // trigger immediate poll
				llqInfo->question->ThisQInterval = INIT_UCAST_POLL_INTERVAL;
				}
			else { llqInfo->state = LLQ_GetZoneInfo; startLLQHandshake(m, llqInfo, mDNSfalse); }
			}
		}
	}

mDNSlocal mDNSBool ReceivePortMapReply(NATTraversalInfo *n, mDNS *m, mDNSu8 *pkt, mDNSu16 len)
	{
	ServiceRecordSet *srs = n->reg.ServiceRegistration;
	mDNSIPPort priv = srs ? srs->RR_SRV.resrec.rdata->u.srv.port : m->UnicastPort4;
	mDNSu32 lease;
	mDNSBool deletion = !n->request.PortReq.lease.NotAnInteger;
	NATPortMapReply *reply = (NATPortMapReply *)pkt;
	mDNSu8 *service = srs ? srs->RR_SRV.resrec.name->c : (mDNSu8 *)"\016LLQ event port";
 
	if (n->state != NATState_Request && n->state != NATState_Refresh)
		{ LogMsg("ReceivePortMapReply (%##s): bad state %d", service, n->state);  return mDNSfalse; }
		
	if (!pkt && !deletion) // timeout
		{
#ifdef _LEGACY_NAT_TRAVERSAL_
		mDNSIPPort pub;
		int ntries = 0;
		mStatus err;
		mDNSBool tcp = (srs && DomainContainsLabelString(srs->RR_PTR.resrec.name, "_tcp"));
	   
		pub = priv; // initially request priv == pub
		while (1)
			{
			err = LNT_MapPort(priv, pub, tcp);
			if (!err)
				{
				n->PublicPort = pub;
				n->state = NATState_Legacy;
				goto end;
				}
			else if (err != mStatus_AlreadyRegistered || ++ntries > LEGACY_NATMAP_MAX_TRIES)
				{
				n->state = NATState_Error;
				goto end;
				}
			else
				{
				// the mapping we want is taken - try a random port
				mDNSu16 RandPort = mDNSRandom(DYN_PORT_MAX - DYN_PORT_MIN) + DYN_PORT_MIN;
				pub = mDNSOpaque16fromIntVal(RandPort);
				}
			}
#else
		goto end;
#endif // _LEGACY_NAT_TRAVERSAL_
		}

	if (len < sizeof(*reply)) { LogMsg("ReceivePortMapReply: response too short (%d bytes)", len); return mDNSfalse; }
	if (reply->vers != NATMAP_VERS) { LogMsg("ReceivePortMapReply: received  version %d (expect version %d)", pkt[0], NATMAP_VERS);  return mDNSfalse; }
	if (reply->opcode != (n->op | NATMAP_RESPONSE_MASK)) { LogMsg("ReceivePortMapReply: bad response code %d", pkt[1]); return mDNSfalse; }
	if (reply->err.NotAnInteger) { LogMsg("ReceivePortMapReply: received error %d", mDNSVal16(reply->err));  return mDNSfalse; }
	if (priv.NotAnInteger != reply->priv.NotAnInteger) return mDNSfalse;  // packet does not match this request

	if (!srs && n != m->uDNS_info.LLQNatInfo)
		{
		LogMsg("ReceivePortMapReply: registration cancelled");  //!!!KRS change to debugf before checkin
		FreeNATInfo(m, n);
		return mDNStrue;
		}

	if (deletion) { n->state = NATState_Deleted; return mDNStrue; }
	
	lease = (mDNSu32)mDNSVal32(reply->lease);
	if (lease > 0x70000000UL / mDNSPlatformOneSecond) lease = 0x70000000UL / mDNSPlatformOneSecond;

	if (n->state == NATState_Refresh && reply->pub.NotAnInteger != n->PublicPort.NotAnInteger)
		LogMsg("ReceivePortMapReply: NAT refresh changed public port from %d to %d", mDNSVal16(n->PublicPort), mDNSVal16(reply->pub));
        // this should never happen
	    // !!!KRS to be defensive, use SRVChanged flag on service and deregister here
	
	n->PublicPort = reply->pub;
	if (reply->pub.NotAnInteger != n->request.PortReq.pub.NotAnInteger) n->request.PortReq.pub = reply->pub; // set message buffer for refreshes

	n->retry = mDNSPlatformTimeNow(m) + ((mDNSs32)lease * mDNSPlatformOneSecond / 2);  // retry half way to expiration

	if (n->state == NATState_Refresh) { n->state = NATState_Established; return mDNStrue; }
	n->state = NATState_Established;

	end:
	if (n->state != NATState_Established && n->state != NATState_Legacy)
		{
		LogMsg("NAT Port Mapping (%##s): timeout", service);
		if (pkt) LogMsg("!!! timeout with non-null packet");
		n->state = NATState_Error;
		if (srs)
			{
			uDNS_HostnameInfo *hi = m->uDNS_info.Hostnames;
			while (hi)
				{
				if (hi->arv6 && (hi->arv6->uDNS_info.state == regState_Registered || hi->arv6->uDNS_info.state == regState_Refresh)) break;
				else hi = hi->next;
				}

			if (hi)
				{
				debugf("Port map failed for service %##s - using IPv6 service target", service);
				srs->uDNS_info.NATinfo = mDNSNULL;
				FreeNATInfo(m, n);
				goto register_service;
				}
			else srs->uDNS_info.state = regState_NATError;
			}
		else LLQNatMapComplete(m);
		return mDNStrue;
		}
	else LogOperation("Mapped private port %d to public port %d", mDNSVal16(priv), mDNSVal16(n->PublicPort));

	if (!srs) { LLQNatMapComplete(m); return mDNStrue; }

	register_service:
	if (srs->uDNS_info.ns.ip.v4.NotAnInteger) SendServiceRegistration(m, srs);  // non-zero server address means we already have necessary zone data to send update
	else
		{
		srs->uDNS_info.state = regState_FetchingZoneData;
		startGetZoneData(srs->RR_SRV.resrec.name, m, mDNStrue, mDNSfalse, serviceRegistrationCallback, srs);
		}
	return mDNStrue;
	}

mDNSlocal void FormatPortMaprequest(NATTraversalInfo *info, mDNSIPPort port)
	{
	NATPortMapRequest *req = &info->request.PortReq;

	req->vers = NATMAP_VERS;
	req->opcode = info->op;
	req->unused.NotAnInteger = 0;
	req->priv = port;
	req->pub = port;
	req->lease = mDNSOpaque32fromIntVal(NATMAP_DEFAULT_LEASE);
	}

mDNSlocal void SendInitialPMapReq(mDNS *m, NATTraversalInfo *info)
	{
	if (!m->uDNS_info.Router.ip.v4.NotAnInteger)
		{
		debugf("No router.  Will retry NAT traversal in %ld seconds", NATMAP_INIT_RETRY);
		info->retry = mDNSPlatformTimeNow(m) + NATMAP_INIT_RETRY;
		info->RetryInterval = NATMAP_INIT_RETRY;
		return;
		}
    SendNATMsg(info, m);
	return;
	}

mDNSlocal void StartNATPortMap(mDNS *m, ServiceRecordSet *srs)
	{
	NATOp_t op;
	NATTraversalInfo *info;

   	if (DomainContainsLabelString(srs->RR_PTR.resrec.name, "_tcp")) op = NATOp_MapTCP;
	else if (DomainContainsLabelString(srs->RR_PTR.resrec.name, "_udp")) op = NATOp_MapUDP;
	else { LogMsg("StartNATPortMap: could not determine transport protocol of service %##s", srs->RR_SRV.resrec.name->c); goto error; }

	if (srs->uDNS_info.NATinfo) { LogMsg("Error: StartNATPortMap - NAT info already initialized!");  FreeNATInfo(m, srs->uDNS_info.NATinfo); }
	info = AllocNATInfo(m, op, ReceivePortMapReply);
	srs->uDNS_info.NATinfo = info;
	info->reg.ServiceRegistration = srs;
	info->state = NATState_Request;
	
	FormatPortMaprequest(info, srs->RR_SRV.resrec.rdata->u.srv.port);
	SendInitialPMapReq(m, info);
	return;
	
	error:
	startGetZoneData(srs->RR_SRV.resrec.name, m, mDNStrue, mDNSfalse, serviceRegistrationCallback, srs);
	}

mDNSlocal void DeleteNATPortMapping(mDNS *m, NATTraversalInfo *nat, ServiceRecordSet *srs)
	{
	if (nat->state == NATState_Established)  // let other edge-case states expire for simplicity
		{
		// zero lease
		nat->request.PortReq.lease.NotAnInteger = 0;
		nat->state = NATState_Request;
		SendNATMsg(nat, m);
		}
#ifdef _LEGACY_NAT_TRAVERSAL_
	else if (nat->state == NATState_Legacy)
		{
		mStatus err = mStatus_NoError;
		mDNSBool tcp = srs ? DomainContainsLabelString(srs->RR_PTR.resrec.name, "_tcp") : mDNSfalse;
		err = LNT_UnmapPort(nat->PublicPort, tcp);
		if (err) LogMsg("Legacy NAT Traversal - unmap request failed with error %ld", err);
		}
#else
	(void)srs; // unused
#endif // _LEGACY_NAT_TRAVERSAL_
	}

mDNSlocal void StartLLQNatMap(mDNS *m)
	{
	NATTraversalInfo *info = AllocNATInfo(m, NATOp_MapUDP, ReceivePortMapReply);
	uDNS_GlobalInfo *u = &m->uDNS_info;
	
	u->LLQNatInfo = info;

	info->reg.RecordRegistration = mDNSNULL;
	info->reg.ServiceRegistration = mDNSNULL;
    info->state = NATState_Request;
	FormatPortMaprequest(info, m->UnicastPort4);
    SendInitialPMapReq(m, info);
	return;
	}

// if LLQ NAT context unreferenced, delete the mapping
mDNSlocal void CheckForUnreferencedLLQMapping(mDNS *m)
	{
	NATTraversalInfo *nat = m->uDNS_info.LLQNatInfo;
	DNSQuestion *q;
	
	if (!nat) return;

	for (q = m->uDNS_info.ActiveQueries; q; q = q->next)
		if (q->LongLived && q->uDNS_info.llq->NATMap) return;

	//to avoid race condition if we need to recreate before this finishes, we do one-shot deregistration
	if (nat->state == NATState_Established || nat->state == NATState_Legacy)
		DeleteNATPortMapping(m, nat, mDNSNULL); // for simplicity we allow other states to expire
	FreeNATInfo(m, nat); // note: this clears the global LLQNatInfo pointer
	}

 // ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - host name and interface management
#endif

// if we ever want to refine support for multiple hostnames, we can add logic matching service names to a particular hostname
// for now, we grab the first registered DynDNS name, if any, or a static name we learned via a reverse-map query
mDNSlocal mDNSBool GetServiceTarget(uDNS_GlobalInfo *u, AuthRecord *srv, domainname *dst)
	{
	uDNS_HostnameInfo *hi = u->Hostnames;
	(void)srv;  // unused

	dst->c[0] = 0;
	while (hi)
		{
		if (hi->arv4 && (hi->arv4->uDNS_info.state == regState_Registered || hi->arv4->uDNS_info.state == regState_Refresh))
			{
			AssignDomainName(dst, hi->arv4->resrec.name);
			return mDNStrue;
			}
		if (hi->arv6 && (hi->arv6->uDNS_info.state == regState_Registered || hi->arv6->uDNS_info.state == regState_Refresh))
			{
			AssignDomainName(dst, hi->arv4->resrec.name);
			return mDNStrue;
			}
		hi = hi->next;
		}

	if (u->StaticHostname.c[0]) { AssignDomainName(dst, &u->StaticHostname); return mDNStrue; }
	return mDNSfalse;
	}

mDNSlocal void UpdateSRV(mDNS *m, ServiceRecordSet *srs)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	ExtraResourceRecord *e;

	// Target change if:
	// We have a target and were previously waiting for one, or
	// We had a target and no longer do, or
	// The target has changed

	domainname newtarget;
	domainname *curtarget = &srs->RR_SRV.resrec.rdata->u.srv.target;
	mDNSBool HaveTarget = GetServiceTarget(u, &srs->RR_SRV, &newtarget);
	mDNSBool TargetChanged = (HaveTarget && srs->uDNS_info.state == regState_NoTarget) || (curtarget->c[0] && !HaveTarget) || !SameDomainName(curtarget, &newtarget);
	mDNSBool HaveZoneData = srs->uDNS_info.ns.ip.v4.NotAnInteger ? mDNStrue : mDNSfalse;
	
	// Nat state change if:
	// We were behind a NAT, and now we are behind a new NAT, or
	// We're not behind a NAT but our port was previously mapped to a different public port
	// We were not behind a NAT and now we are
	
	NATTraversalInfo *nat = srs->uDNS_info.NATinfo;
	mDNSIPPort port = srs->RR_SRV.resrec.rdata->u.srv.port;
	mDNSBool NATChanged = mDNSfalse;
	mDNSBool NowBehindNAT = port.NotAnInteger && IsPrivateV4Addr(&u->AdvertisedV4);
	mDNSBool WereBehindNAT = nat != mDNSNULL;
	mDNSBool NATRouterChanged = nat && nat->Router.ip.v4.NotAnInteger != u->Router.ip.v4.NotAnInteger;
	mDNSBool PortWasMapped = nat && (nat->state == NATState_Established || nat->state == NATState_Legacy) && nat->PublicPort.NotAnInteger != port.NotAnInteger;
	
	if (WereBehindNAT && NowBehindNAT && NATRouterChanged) NATChanged = mDNStrue;
	else if (!NowBehindNAT && PortWasMapped)               NATChanged = mDNStrue;
	else if (!WereBehindNAT && NowBehindNAT)               NATChanged = mDNStrue;
	
	if (!TargetChanged && !NATChanged) return;

	debugf("UpdateSRV (%##s) HadZoneData=%d, TargetChanged=%d, HaveTarget=%d, NowBehindNAT=%d, WereBehindNAT=%d, NATRouterChanged=%d, PortWasMapped=%d",
		   srs->RR_SRV.resrec.name->c,  HaveZoneData, TargetChanged, HaveTarget, NowBehindNAT, WereBehindNAT, NATRouterChanged, PortWasMapped);
	
	switch(srs->uDNS_info.state)
		{
		case regState_FetchingZoneData:
		case regState_Cancelled:
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
			srs->uDNS_info.SRVUpdateDeferred = mDNStrue;
			return;
						
		case regState_NATError:
			if (!NATChanged) return;
			// if nat changed, register if we have a target (below)

		case regState_NoTarget:
			if (HaveTarget)
				{
				debugf("UpdateSRV: %s service %##s", HaveZoneData ? (NATChanged && NowBehindNAT ? "Starting Port Map for" : "Registering") : "Getting Zone Data for", srs->RR_SRV.resrec.name->c);
				if (!HaveZoneData)
					{
					srs->uDNS_info.state = regState_FetchingZoneData;
					startGetZoneData(srs->RR_SRV.resrec.name, m, mDNStrue, mDNSfalse, serviceRegistrationCallback, srs);
					}
				else
					{
					if (nat && (NATChanged || !NowBehindNAT)) { srs->uDNS_info.NATinfo = mDNSNULL; FreeNATInfo(m, nat); }
					if (NATChanged && NowBehindNAT) { srs->uDNS_info.state = regState_NATMap; StartNATPortMap(m, srs); }
					else SendServiceRegistration(m, srs);
					}
				}
			return;
			
		case regState_Registered:
			// target or nat changed.  deregister service.  upon completion, we'll look for a new target
			debugf("UpdateSRV: SRV record changed for service %##s - deregistering (will re-register with new SRV)",  srs->RR_SRV.resrec.name->c);
			for (e = srs->Extras; e; e = e->next) e->r.uDNS_info.state = regState_ExtraQueued;  // extra will be re-registed if the service is re-registered
			srs->uDNS_info.SRVChanged = mDNStrue;
			SendServiceDeregistration(m, srs);
			return;
		}
	}

mDNSlocal void UpdateSRVRecords(mDNS *m)
	{
	ServiceRecordSet *srs;

	for (srs = m->uDNS_info.ServiceRegistrations; srs; srs = srs->next) UpdateSRV(m, srs);
	}

mDNSlocal void HostnameCallback(mDNS *const m, AuthRecord *const rr, mStatus result)
	{
	uDNS_HostnameInfo *hi = (uDNS_HostnameInfo *)rr->RecordContext;
		
	if (result == mStatus_MemFree)
		{
		if (hi)
			{
			if      (hi->arv4 == rr)                hi->arv4 = mDNSNULL;
			else if (hi->arv4 == rr)                hi->arv6 = mDNSNULL;
			rr->RecordContext = mDNSNULL;
			if (!hi->arv4 && !hi->arv6) ufree(hi);  // free hi when both v4 and v6 AuthRecs deallocated
			}
		ufree(rr);
		return;
		}
	
	if (result)
		{
		// don't unlink or free - we can retry when we get a new address/router
		if (rr->resrec.rrtype == kDNSType_A)
			LogMsg("HostnameCallback: Error %ld for registration of %##s IP %.4a", result, rr->resrec.name->c, &rr->resrec.rdata->u.ipv4);
		else
			LogMsg("HostnameCallback: Error %ld for registration of %##s IP %.16a", result, rr->resrec.name->c, &rr->resrec.rdata->u.ipv6);
		if (!hi) { ufree(rr); return; }
		if (rr->uDNS_info.state != regState_Unregistered) LogMsg("Error: HostnameCallback invoked with error code for record not in regState_Unregistered!");

		if ((!hi->arv4 || hi->arv4->uDNS_info.state == regState_Unregistered) &&
			(!hi->arv6 || hi->arv6->uDNS_info.state == regState_Unregistered))
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
	UpdateSRVRecords(m);
	
	// Deliver success to client
	if (!hi) { LogMsg("HostnameCallback invoked with orphaned address record"); return; }
	if (rr->resrec.rrtype == kDNSType_A)
		LogMsg("Registered hostname %##s IP %.4a", rr->resrec.name->c, &rr->resrec.rdata->u.ipv4);
	else
		LogMsg("Registered hostname %##s IP %.16a", rr->resrec.name->c, &rr->resrec.rdata->u.ipv6);

	rr->RecordContext = (void *)hi->StatusContext;
	if (hi->StatusCallback)
		hi->StatusCallback(m, rr, result); // client may NOT make API calls here
	rr->RecordContext = (void *)hi;
	}

// register record or begin NAT traversal
mDNSlocal void AdvertiseHostname(mDNS *m, uDNS_HostnameInfo *h)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	
	if (u->AdvertisedV4.ip.v4.NotAnInteger && h->arv4->uDNS_info.state == regState_Unregistered)
		{
		if (IsPrivateV4Addr(&u->AdvertisedV4))
			StartGetPublicAddr(m, h->arv4);
		else
			{
			LogMsg("Advertising %##s IP %.4a", h->arv4->resrec.name->c, &u->AdvertisedV4.ip.v4);
			uDNS_RegisterRecord(m, h->arv4);
			}
		}
	if (u->AdvertisedV6.ip.v6.b[0] && h->arv6->uDNS_info.state == regState_Unregistered)
		{
		LogMsg("Advertising %##s IP %.16a", h->arv4->resrec.name->c, &u->AdvertisedV6.ip.v6);
		uDNS_RegisterRecord(m, h->arv6);
		}
	}

mDNSlocal void FoundStaticHostname(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, mDNSBool AddRecord)
	{
	const domainname *pktname = &answer->rdata->u.name;
	domainname *storedname = &m->uDNS_info.StaticHostname;
	uDNS_HostnameInfo *h = m->uDNS_info.Hostnames;

	(void)question;
	
	debugf("FoundStaticHostname: %##s -> %##s (%s)", question->qname.c, answer->rdata->u.name.c, AddRecord ? "added" : "removed");
	if (AddRecord && !SameDomainName(pktname, storedname))
		{
		AssignDomainName(storedname, pktname);
		while (h)
			{
			if ((h->arv4 && (h->arv4->uDNS_info.state == regState_FetchingZoneData || h->arv4->uDNS_info.state == regState_Pending || h->arv4->uDNS_info.state == regState_NATMap)) ||
			    (h->arv6 && (h->arv6->uDNS_info.state == regState_FetchingZoneData || h->arv6->uDNS_info.state == regState_Pending)))
				{
				// if we're in the process of registering a dynamic hostname, delay SRV update so we don't have to reregister services if the dynamic name succeeds
				m->uDNS_info.DelaySRVUpdate = mDNStrue;
				m->uDNS_info.NextSRVUpdate = mDNSPlatformTimeNow(m) + (5 * mDNSPlatformOneSecond);
				return;
				}
			h = h->next;
			}
		UpdateSRVRecords(m);
		}
	else if (!AddRecord && SameDomainName(pktname, storedname))
		{
		storedname->c[0] = 0;
		UpdateSRVRecords(m);
		}
	}

mDNSlocal void GetStaticHostname(mDNS *m)
	{
	char buf[MAX_ESCAPED_DOMAIN_NAME];
	DNSQuestion *q = &m->uDNS_info.ReverseMap;
	mDNSu8 *ip = m->uDNS_info.AdvertisedV4.ip.v4.b;
	mStatus err;
	
	if (m->uDNS_info.ReverseMapActive)
		{
		uDNS_StopQuery(m, q);
		m->uDNS_info.ReverseMapActive = mDNSfalse;
		}

	m->uDNS_info.StaticHostname.c[0] = 0;
	if (!m->uDNS_info.AdvertisedV4.ip.v4.NotAnInteger) return;
	ubzero(q, sizeof(*q));
	mDNS_snprintf(buf, MAX_ESCAPED_DOMAIN_NAME, "%d.%d.%d.%d.in-addr.arpa.", ip[3], ip[2], ip[1], ip[0]);
    if (!MakeDomainNameFromDNSNameString(&q->qname, buf)) { LogMsg("Error: GetStaticHostname - bad name %s", buf); return; }

	q->InterfaceID      = mDNSInterface_Any;
    q->Target           = zeroAddr;
    q->qtype            = kDNSType_PTR;
    q->qclass           = kDNSClass_IN;
    q->LongLived        = mDNSfalse;
    q->ExpectUnique     = mDNSfalse;
    q->ForceMCast       = mDNSfalse;
    q->QuestionCallback = FoundStaticHostname;
    q->QuestionContext  = mDNSNULL;

	err = uDNS_StartQuery(m, q);
	if (err) LogMsg("Error: GetStaticHostname - StartQuery returned error %d", err);
	else m->uDNS_info.ReverseMapActive = mDNStrue;
	}

mDNSlocal void AssignHostnameInfoAuthRecord(mDNS *m, uDNS_HostnameInfo *hi, int type)
	{
	AuthRecord **dst = (type == mDNSAddrType_IPv4 ? &hi->arv4 : &hi->arv6);
	AuthRecord *ar =  umalloc(sizeof(*ar));
	uDNS_GlobalInfo *u = &m->uDNS_info;
	
	if (type != mDNSAddrType_IPv4 && type != mDNSAddrType_IPv6) { LogMsg("ERROR: AssignHostnameInfoAuthRecord - bad type %d", type); return; }
	if (!ar) { LogMsg("ERROR: AssignHostnameInfoAuthRecord - malloc"); return; }
	
	mDNS_SetupResourceRecord(ar, mDNSNULL, 0, type == mDNSAddrType_IPv4 ? kDNSType_A : kDNSType_AAAA,  1, kDNSRecordTypeKnownUnique, HostnameCallback, hi);
	AssignDomainName(ar->resrec.name, &hi->fqdn);

	// only set RData if we have a valid IP
	if (type == mDNSAddrType_IPv4 && u->AdvertisedV4.ip.v4.NotAnInteger)
		{
		if (u->MappedV4.ip.v4.NotAnInteger) ar->resrec.rdata->u.ipv4 = u->MappedV4.ip.v4;
		else                                ar->resrec.rdata->u.ipv4 = u->AdvertisedV4.ip.v4;
		}
	else if (type == mDNSAddrType_IPv6 && u->AdvertisedV6.ip.v6.b[0])
		{
		ar->resrec.rdata->u.ipv6 = u->AdvertisedV6.ip.v6;
		}

	ar->uDNS_info.state = regState_Unregistered;

	if (*dst)
		{
		LogMsg("ERROR: AssignHostnameInfoAuthRecord - overwriting %s AuthRec", type == mDNSAddrType_IPv4 ? "IPv4" : "IPv6");
		unlinkAR(&u->RecordRegistrations, *dst);
		(*dst)->RecordContext = mDNSNULL;  // defensively clear backpointer to avoid doubly-referenced context
		}

	*dst = ar;
	}


// Deregister hostnames and register new names for each host domain with the current global
// values for the hostlabel and primary IP address
mDNSlocal void UpdateHostnameRegistrations(mDNS *m)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	uDNS_HostnameInfo *i;

	for (i = u->Hostnames; i; i = i->next)
		{
		if (i->arv4 && i->arv4->uDNS_info.state != regState_Unregistered &&
			i->arv4->resrec.rdata->u.ipv4.NotAnInteger != u->AdvertisedV4.ip.v4.NotAnInteger &&
			i->arv4->resrec.rdata->u.ipv4.NotAnInteger !=u->MappedV4.ip.v4.NotAnInteger)
			{
			uDNS_DeregisterRecord(m, i->arv4);
			i->arv4 = mDNSNULL;
			}
		if (i->arv6 && !mDNSPlatformMemSame(i->arv6->resrec.rdata->u.ipv6.b, u->AdvertisedV6.ip.v6.b, 16) && i->arv6->uDNS_info.state != regState_Unregistered)
			{
			uDNS_DeregisterRecord(m, i->arv6);
			i->arv6 = mDNSNULL;
			}
		
		if (!i->arv4 && u->AdvertisedV4.ip.v4.NotAnInteger)                    AssignHostnameInfoAuthRecord(m, i, mDNSAddrType_IPv4);
		else if (i->arv4 && i->arv4->uDNS_info.state == regState_Unregistered) i->arv4->resrec.rdata->u.ipv4 = u->AdvertisedV4.ip.v4;  // simply overwrite unregistered
		if (!i->arv6 && u->AdvertisedV6.ip.v6.b[0])                            AssignHostnameInfoAuthRecord(m, i, mDNSAddrType_IPv6);
		else if (i->arv6 &&i->arv6->uDNS_info.state == regState_Unregistered)  i->arv6->resrec.rdata->u.ipv6 = u->AdvertisedV6.ip.v6;

		AdvertiseHostname(m, i);
		}
	}

mDNSexport void mDNS_AddDynDNSHostName(mDNS *m, const domainname *fqdn, mDNSRecordCallback *StatusCallback, const void *StatusContext)
   {
	uDNS_GlobalInfo *u = &m->uDNS_info;
	uDNS_HostnameInfo *ptr, *new;

	mDNS_Lock(m);

	// check if domain already registered
	for (ptr = u->Hostnames; ptr; ptr = ptr->next)
		{
		if (SameDomainName(fqdn, &ptr->fqdn))
			{ LogMsg("Host Domain %##s already in list", fqdn->c); goto exit; }
		}

	// allocate and format new address record
	new = umalloc(sizeof(*new));
	if (!new) { LogMsg("ERROR: mDNS_AddDynDNSHostname - malloc"); goto exit; }
	ubzero(new, sizeof(*new));
    new->next = u->Hostnames;
	u->Hostnames = new;
	
	AssignDomainName(&new->fqdn, fqdn);
	new->StatusCallback = StatusCallback;
	new->StatusContext = StatusContext;

	if (u->AdvertisedV4.ip.v4.NotAnInteger) AssignHostnameInfoAuthRecord(m, new, mDNSAddrType_IPv4);
	else new->arv4 = mDNSNULL;
	if (u->AdvertisedV6.ip.v6.b[0])         AssignHostnameInfoAuthRecord(m, new, mDNSAddrType_IPv6);
	else new->arv6 = mDNSNULL;

	 if (u->AdvertisedV6.ip.v6.b[0] || u->AdvertisedV4.ip.v4.NotAnInteger) AdvertiseHostname(m, new);
	
exit:
	mDNS_Unlock(m);
	}

mDNSexport void mDNS_RemoveDynDNSHostName(mDNS *m, const domainname *fqdn)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	uDNS_HostnameInfo **ptr = &u->Hostnames;

	mDNS_Lock(m);

	while (*ptr && !SameDomainName(fqdn, &(*ptr)->fqdn)) ptr = &(*ptr)->next;
	if (!*ptr) LogMsg("mDNS_RemoveDynDNSHostName: no such domainname %##s", fqdn->c);
	else
		{
		uDNS_HostnameInfo *hi = *ptr;
		*ptr = (*ptr)->next; // unlink
		if (hi->arv4)
			{
			hi->arv4->RecordContext = mDNSNULL; // about to free wrapper struct
			if (hi->arv4->uDNS_info.state != regState_Unregistered) uDNS_DeregisterRecord(m, hi->arv4);
			else { ufree(hi->arv4); hi->arv4 = mDNSNULL; }
			}
		if (hi->arv6)
			{
			hi->arv6->RecordContext = mDNSNULL; // about to free wrapper struct
			if (hi->arv6->uDNS_info.state != regState_Unregistered) uDNS_DeregisterRecord(m, hi->arv6);
			else { ufree(hi->arv6); hi->arv6 = mDNSNULL; }
			}
		ufree(hi);
		}
	UpdateSRVRecords(m);
	mDNS_Unlock(m);
	}

mDNSexport void mDNS_SetPrimaryInterfaceInfo(mDNS *m, const mDNSAddr *v4addr, const mDNSAddr *v6addr, const mDNSAddr *router)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	mDNSBool v4Changed, v6Changed, RouterChanged;
   
	if (v4addr && v4addr->type != mDNSAddrType_IPv4) { LogMsg("mDNS_SetPrimaryInterfaceInfo V4 address - incorrect type.  Discarding."); return; }
	if (v6addr && v6addr->type != mDNSAddrType_IPv6) { LogMsg("mDNS_SetPrimaryInterfaceInfo V6 address - incorrect type.  Discarding."); return; }
	if (router && router->type != mDNSAddrType_IPv4) { LogMsg("mDNS_SetPrimaryInterfaceInfo passed non-V4 router.  Discarding."); return; }

	mDNS_Lock(m);

	v4Changed   = (v4addr ? v4addr->ip.v4.NotAnInteger : 0) != u->AdvertisedV4.ip.v4.NotAnInteger;
	v6Changed   = v6addr ? !mDNSPlatformMemSame(v6addr, &u->AdvertisedV6, sizeof(*v6addr)) : (u->AdvertisedV6.ip.v6.b[0] != 0);
	RouterChanged = (router ? router->ip.v4.NotAnInteger : 0) != u->Router.ip.v4.NotAnInteger;
	
#if MDNS_DEBUGMSGS
	if (v4addr && (v4Changed || RouterChanged))
		LogMsg("mDNS_SetPrimaryInterfaceInfo: address changed from %d.%d.%d.%d to %d.%d.%d.%d:%d",
			   u->AdvertisedV4.ip.v4.b[0], u->AdvertisedV4.ip.v4.b[1], u->AdvertisedV4.ip.v4.b[2], u->AdvertisedV4.ip.v4.b[3],
			   v4addr->ip.v4.b[0], v4addr->ip.v4.b[1], v4addr->ip.v4.b[2], v4addr->ip.v4.b[3]);
#endif // MDNS_DEBUGMSGS

	if ((v4Changed || RouterChanged) && u->MappedV4.ip.v4.NotAnInteger) u->MappedV4.ip.v4.NotAnInteger = 0;
	if (v4addr) u->AdvertisedV4 = *v4addr;  else u->AdvertisedV4.ip.v4.NotAnInteger = 0;
	if (v6addr) u->AdvertisedV6 = *v6addr;  else ubzero(u->AdvertisedV6.ip.v6.b, 16);
	if (router) u->Router       = *router;  else u->Router.ip.v4.NotAnInteger = 0;
	// setting router to zero indicates that nat mappings must be reestablished when router is reset
	
	if ((v4Changed || RouterChanged || v6Changed) && v4addr)
		{
		// don't update these unless we've got V4
		UpdateHostnameRegistrations(m);
		UpdateSRVRecords(m);
		GetStaticHostname(m);  // look up reverse map record to find any static hostnames for our IP address
		}
	
	mDNS_Unlock(m);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Incoming Message Processing
#endif

mDNSlocal mDNSBool kaListContainsAnswer(DNSQuestion *question, CacheRecord *rr)
	{
	CacheRecord *ptr;

	for (ptr = question->uDNS_info.knownAnswers; ptr; ptr = ptr->next)
		if (SameResourceRecord(&ptr->resrec, &rr->resrec)) return mDNStrue;

	return mDNSfalse;
	}


mDNSlocal void removeKnownAnswer(DNSQuestion *question, CacheRecord *rr)
	{
	CacheRecord *ptr, *prev = mDNSNULL;

	for (ptr = question->uDNS_info.knownAnswers; ptr; ptr = ptr->next)
		{
		if (SameResourceRecord(&ptr->resrec, &rr->resrec))
			{
			if (prev) prev->next = ptr->next;
			else question->uDNS_info.knownAnswers = ptr->next;
			ufree(ptr);
			return;
			}
		prev = ptr;
		}
	LogMsg("removeKnownAnswer() called for record not in KA list");
	}


mDNSlocal void addKnownAnswer(DNSQuestion *question, const CacheRecord *rr)
	{
	CacheRecord *newCR = mDNSNULL;
	mDNSu32 size;

	size = sizeof(CacheRecord) + rr->resrec.rdlength - InlineCacheRDSize;
	newCR = (CacheRecord *)umalloc(size);
	if (!newCR) { LogMsg("ERROR: addKnownAnswer - malloc"); return; }
	umemcpy(newCR, rr, size);
	newCR->resrec.rdata = (RData*)&newCR->rdatastorage;
	newCR->resrec.rdata->MaxRDLength = rr->resrec.rdlength;
	newCR->resrec.name = &question->qname;
	newCR->next = question->uDNS_info.knownAnswers;
	question->uDNS_info.knownAnswers = newCR;
	}

mDNSlocal void deriveGoodbyes(mDNS * const m, DNSMessage *msg, const  mDNSu8 *end, DNSQuestion *question)
	{
	const mDNSu8 *ptr;
	int i;
	CacheRecord *fptr, *ka, *cr, *answers = mDNSNULL, *prev = mDNSNULL;
	LargeCacheRecord *lcr;
	
	if (question != m->uDNS_info.CurrentQuery) { LogMsg("ERROR: deriveGoodbyes called without CurrentQuery set!"); return; }

	ptr = LocateAnswers(msg, end);
	if (!ptr) goto pkt_error;

	if (!msg->h.numAnswers)
		{
		// delete the whole KA list
		ka = question->uDNS_info.knownAnswers;
		while (ka)
			{
			debugf("deriving goodbye for %##s", ka->resrec.name->c);
			
			m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
			question->QuestionCallback(m, question, &ka->resrec, mDNSfalse);
			m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
			// CAUTION: Need to be careful after calling question->QuestionCallback(),
			// because the client's callback function is allowed to do anything,
			// including starting/stopping queries, registering/deregistering records, etc.
			if (question != m->uDNS_info.CurrentQuery)
				{
				debugf("deriveGoodbyes - question removed via callback.  returning.");
				return;
				}
			fptr = ka;
			ka = ka->next;
			ufree(fptr);
			}
		question->uDNS_info.knownAnswers = mDNSNULL;
		return;
		}
	
	// make a list of all the new answers
	for (i = 0; i < msg->h.numAnswers; i++)
		{
		lcr = (LargeCacheRecord *)umalloc(sizeof(LargeCacheRecord));
		if (!lcr) goto malloc_error;
		ubzero(lcr, sizeof(LargeCacheRecord));
		ptr = GetLargeResourceRecord(m, msg, ptr, end, 0, kDNSRecordTypePacketAns, lcr);
		if (!ptr) goto pkt_error;
		cr = &lcr->r;
		if (ResourceRecordAnswersQuestion(&cr->resrec, question))
			{
			cr->next = answers;
			answers = cr;
			}
		else ufree(cr);
		}
	
	// make sure every known answer is in the answer list
	ka = question->uDNS_info.knownAnswers;
	while (ka)
		{
		for (cr = answers; cr; cr = cr->next)
			{ if (SameResourceRecord(&ka->resrec, &cr->resrec)) break; }
		if (!cr)
			{
			// record is in KA list but not answer list - remove from KA list
			if (prev) prev->next = ka->next;
			else question->uDNS_info.knownAnswers = ka->next;
			debugf("deriving goodbye for %##s", ka->resrec.name->c);
			m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
			question->QuestionCallback(m, question, &ka->resrec, mDNSfalse);
			m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
			// CAUTION: Need to be careful after calling question->QuestionCallback(),
			// because the client's callback function is allowed to do anything,
			// including starting/stopping queries, registering/deregistering records, etc.
			if (question != m->uDNS_info.CurrentQuery)
				{
				debugf("deriveGoodbyes - question removed via callback.  returning.");
				return;
				}
			fptr = ka;
			ka = ka->next;
			ufree(fptr);
			}
		else
			{
			prev = ka;
			ka = ka->next;
			}
		}

	// free temp answers list
	cr = answers;
	while (cr) { fptr = cr; cr = cr->next; ufree(fptr); }

	return;
	
	pkt_error:
	LogMsg("ERROR: deriveGoodbyes - received malformed response to query for %##s (%d)",
		   question->qname.c, question->qtype);
	return;

	malloc_error:
	LogMsg("ERROR: Malloc");
	}

mDNSlocal void pktResponseHndlr(mDNS * const m, DNSMessage *msg, const  mDNSu8 *end, DNSQuestion *question, mDNSBool llq)
	{
	const mDNSu8 *ptr;
	int i;
	CacheRecord *cr = &m->rec.r;
	mDNSBool goodbye, inKAList;
	int	followedCNames = 0;
	static const int maxCNames = 5;
	LLQ_Info *llqInfo = question->uDNS_info.llq;
	domainname origname;
	origname.c[0] = 0;
	
	if (question != m->uDNS_info.CurrentQuery)
		{ LogMsg("ERROR: pktResponseHdnlr called without CurrentQuery ptr set!");  return; }

	if (question->uDNS_info.Answered == 0 && msg->h.numAnswers == 0 && !llq)
		{
		/* NXDOMAIN error or empty RR set - notify client */
		question->uDNS_info.Answered = mDNStrue;
		
		/* Create empty resource record */
		cr->resrec.RecordType = kDNSRecordTypeUnregistered;
		cr->resrec.InterfaceID = mDNSNULL;
		cr->resrec.name = &question->qname;
		cr->resrec.rrtype = question->qtype;
		cr->resrec.rrclass = question->qclass;
		cr->resrec.rroriginalttl = 1; /* What should we use for the TTL? TTL from SOA for domain? */
		cr->resrec.rdlength = 0;
		cr->resrec.rdestimate = 0;
		cr->resrec.namehash = 0;
		cr->resrec.namehash = 0;
		cr->resrec.rdata = (RData*)&cr->rdatastorage;
		cr->resrec.rdata->MaxRDLength = cr->resrec.rdlength;
		
		/* Pass empty answer to callback */
		m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
		question->QuestionCallback(m, question, &cr->resrec, 0);
		m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
		// CAUTION: Need to be careful after calling question->QuestionCallback(),
		// because the client's callback function is allowed to do anything,
		// including starting/stopping queries, registering/deregistering records, etc.
		m->rec.r.resrec.RecordType = 0; // Clear RecordType to show we're not still using it
		if (question != m->uDNS_info.CurrentQuery)
			{
			debugf("pktResponseHndlr - CurrentQuery changed by QuestionCallback - returning.");
			return;
			}
		}
	
	question->uDNS_info.Answered = mDNStrue;
	
	ptr = LocateAnswers(msg, end);
	if (!ptr) goto pkt_error;

	for (i = 0; i < msg->h.numAnswers; i++)
		{
		ptr = GetLargeResourceRecord(m, msg, ptr, end, 0, kDNSRecordTypePacketAns, &m->rec);
		if (!ptr) goto pkt_error;
		if (ResourceRecordAnswersQuestion(&cr->resrec, question))
			{
			goodbye = llq ? ((mDNSs32)cr->resrec.rroriginalttl == -1) : mDNSfalse;
			if (cr->resrec.rrtype == kDNSType_CNAME)
				{
				if (followedCNames > (maxCNames - 1)) LogMsg("Error: too many CNAME referals for question %##s", &origname);
				else
					{
					debugf("Following cname %##s -> %##s", question->qname.c, cr->resrec.rdata->u.name.c);
					if (question->ReturnCNAME)
						{
						m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
						question->QuestionCallback(m, question, &cr->resrec, !goodbye);
						m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
						// CAUTION: Need to be careful after calling question->QuestionCallback(),
						// because the client's callback function is allowed to do anything,
						// including starting/stopping queries, registering/deregistering records, etc.
						if (question != m->uDNS_info.CurrentQuery)
							{
							m->rec.r.resrec.RecordType = 0; // Clear RecordType to show we're not still using it
							debugf("pktResponseHndlr - CurrentQuery changed by QuestionCallback - returning.");
							return;
							}
						}
					AssignDomainName(&origname, &question->qname);
					AssignDomainName(&question->qname, &cr->resrec.rdata->u.name);
					question->qnamehash = DomainNameHashValue(&question->qname);
					followedCNames++;
					i = -1; // restart packet answer matching
					ptr = LocateAnswers(msg, end);
					m->rec.r.resrec.RecordType = 0; // Clear RecordType to show we're not still using it
					continue;
					}
				}
			
			inKAList = kaListContainsAnswer(question, cr);

			if ((goodbye && !inKAList) || (!goodbye && inKAList))
				{
				m->rec.r.resrec.RecordType = 0; // Clear RecordType to show we're not still using it
				continue;  // list up to date
				}
			if (!inKAList) addKnownAnswer(question, cr);
			if (goodbye) removeKnownAnswer(question, cr);
			m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
			question->QuestionCallback(m, question, &cr->resrec, !goodbye);
			m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
			if (question != m->uDNS_info.CurrentQuery)
				{
				m->rec.r.resrec.RecordType = 0; // Clear RecordType to show we're not still using it
				debugf("pktResponseHndlr - CurrentQuery changed by QuestionCallback - returning");
				return;
				}
			}

		m->rec.r.resrec.RecordType = 0; // Clear RecordType to show we're not still using it
		}

	if (!llq || llqInfo->state == LLQ_Poll || llqInfo->deriveRemovesOnResume)
		{
		deriveGoodbyes(m, msg, end,question);
		if (llq && llqInfo->deriveRemovesOnResume) llqInfo->deriveRemovesOnResume = mDNSfalse;
		}

	// Our interval may be set lower to recover from failures -- now that we have an answer, fully back off retry.
	// If the server advertised an LLQ-specific port number then that implies that this zone
	// *wants* to support LLQs, so if the setup fails (e.g. because we are behind a NAT)
	// then we use a slightly faster polling rate to give slightly better user experience.
	if (llq && llqInfo->state == LLQ_Poll && llqInfo->servPort.NotAnInteger) question->ThisQInterval = LLQ_POLL_INTERVAL;
	else if (question->ThisQInterval < MAX_UCAST_POLL_INTERVAL) question->ThisQInterval = MAX_UCAST_POLL_INTERVAL;
	return;

	pkt_error:
	LogMsg("ERROR: pktResponseHndlr - received malformed response to query for %##s (%d)",
		   question->qname.c, question->qtype);
	return;
	}

mDNSlocal void simpleResponseHndlr(mDNS * const m, DNSMessage *msg, const  mDNSu8 *end, DNSQuestion *question, void *context)
	{
	(void)context; // unused
	pktResponseHndlr(m, msg, end, question, mDNSfalse);
	}

mDNSlocal void llqResponseHndlr(mDNS * const m, DNSMessage *msg, const  mDNSu8 *end, DNSQuestion *question, void *context)
	{
	(void)context; // unused
	pktResponseHndlr(m, msg, end, question, mDNStrue);
	}

mDNSlocal mStatus ParseTSIGError(mDNS *m, const DNSMessage *msg, const mDNSu8 *end, const domainname *displayname)
	{
	LargeCacheRecord lcr;
	const mDNSu8 *ptr;
	mStatus err = mStatus_NoError;
	int i;
	
	ptr = LocateAdditionals(msg, end);
	if (!ptr) goto finish;
	
	for (i = 0; i < msg->h.numAdditionals; i++)
		{
		ptr = GetLargeResourceRecord(m, msg, ptr, end, 0, kDNSRecordTypePacketAdd, &lcr);
		if (!ptr) goto finish;
		if (lcr.r.resrec.rrtype == kDNSType_TSIG)
			{
			mDNSu32 macsize;
		    mDNSu8 *rd = lcr.r.resrec.rdata->u.data;
			mDNSu8 *rdend = rd + MaximumRDSize;
			int alglen = DomainNameLength(&lcr.r.resrec.rdata->u.name);
			
			if (rd +  alglen > rdend) goto finish;
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

			if (err ==  TSIG_ErrBadSig)      { LogMsg("%##s: bad signature", displayname->c);              err = mStatus_BadSig;     }
			else if (err == TSIG_ErrBadKey)  { LogMsg("%##s: bad key", displayname->c);                    err = mStatus_BadKey;     }
			else if (err == TSIG_ErrBadTime) { LogMsg("%##s: bad time", displayname->c);                   err = mStatus_BadTime;    }
			else if (err)                    { LogMsg("%##s: unknown tsig error %d", displayname->c, err); err = mStatus_UnknownErr; }
			goto finish;
			}
		}
			
	finish:
	return err;
	}

mDNSlocal mStatus checkUpdateResult(domainname *displayname, mDNSu8 rcode, mDNS *m, const DNSMessage *msg, const mDNSu8 *end)
	{
	(void)msg;  // currently unused, needed for TSIG errors
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
		// TSIG errors should come with FmtErr as per RFC 2845, but BIND 9 sends them with NotAuth so we look here too
		mStatus tsigerr = ParseTSIGError(m, msg, end, displayname);
		if (!tsigerr)
			{
			LogMsg("Permission denied (NOAUTH): %##s", displayname->c);
			return mStatus_UnknownErr;
			}
		else return tsigerr;
		}
	else if (rcode == kDNSFlag1_RC_FmtErr)
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

mDNSlocal void hndlServiceUpdateReply(mDNS * const m, ServiceRecordSet *srs,  mStatus err)
	{
	mDNSBool InvokeCallback = mDNSfalse;
	uDNS_RegInfo *info = &srs->uDNS_info;
	NATTraversalInfo *nat = srs->uDNS_info.NATinfo;
	ExtraResourceRecord **e = &srs->Extras;
	AuthRecord *txt = &srs->RR_TXT;
	uDNS_RegInfo *txtInfo = &txt->uDNS_info;
	switch (info->state)
		{
		case regState_Pending:
			if (err == mStatus_NameConflict && !info->TestForSelfConflict)
				{
				info->TestForSelfConflict = mDNStrue;
				debugf("checking for self-conflict of service %##s", srs->RR_SRV.resrec.name->c);
				SendServiceRegistration(m, srs);
				return;
				}
			else if (info->TestForSelfConflict)
				{
				info->TestForSelfConflict = mDNSfalse;
				if (err == mStatus_NoSuchRecord) err = mStatus_NameConflict;  // NoSuchRecord implies that our prereq was not met, so we actually have a name conflict
				if (err) info->state = regState_Unregistered;
				else info->state = regState_Registered;
				InvokeCallback = mDNStrue;
				break;
				}
			else if (err == mStatus_UnknownErr && info->lease)
				{
				LogMsg("Re-trying update of service %##s without lease option", srs->RR_SRV.resrec.name->c);
				info->lease = mDNSfalse;
				SendServiceRegistration(m, srs);
				return;
				}
			else
				{
				if (err) { LogMsg("Error %ld for registration of service %##s", err, srs->RR_SRV.resrec.name->c); info->state = regState_Unregistered; } //!!!KRS make sure all structs will still get cleaned up when client calls DeregisterService with this state
				else info->state = regState_Registered;
				InvokeCallback = mDNStrue;
				break;
				}
		case regState_Refresh:
			if (err)
				{
				LogMsg("Error %ld for refresh of service %##s", err, srs->RR_SRV.resrec.name->c);
				InvokeCallback = mDNStrue;
				info->state = regState_Unregistered;
				}
			else info->state = regState_Registered;
			break;
		case regState_DeregPending:
			if (err) LogMsg("Error %ld for deregistration of service %##s", err, srs->RR_SRV.resrec.name->c);
			if (info->SRVChanged)
				{
				info->state = regState_NoTarget;  // NoTarget will allow us to pick up new target OR nat traversal state
				break;
				}
			err = mStatus_MemFree;
			InvokeCallback = mDNStrue;
			if (nat)
				{
				if (nat->state == NATState_Deleted) { info->NATinfo = mDNSNULL; FreeNATInfo(m, nat); } // deletion copmleted
				else nat->reg.ServiceRegistration = mDNSNULL;  // allow mapping deletion to continue
				}
			info->state = regState_Unregistered;
			break;
		case regState_DeregDeferred:
			if (err)
				{
				debugf("Error %ld received prior to deferred derigstration of %##s", err, srs->RR_SRV.resrec.name->c);
				err = mStatus_MemFree;
				InvokeCallback = mDNStrue;
				info->state = regState_Unregistered;
				break;
				}
			else
				{
				debugf("Performing deferred deregistration of %##s", srs->RR_SRV.resrec.name->c);
				info->state = regState_Registered;
				SendServiceDeregistration(m, srs);
				return;
				}
		case regState_UpdatePending:
			if (err)
				{
				LogMsg("hndlServiceUpdateReply: error updating TXT record for service %##s", srs->RR_SRV.resrec.name->c);
				info->state = regState_Unregistered;
				InvokeCallback = mDNStrue;
				}
			else
				{
				info->state = regState_Registered;
				// deallocate old RData
				if (txtInfo->UpdateRDCallback) txtInfo->UpdateRDCallback(m, txt, txtInfo->OrigRData);
				SetNewRData(&txt->resrec, txtInfo->InFlightRData, txtInfo->InFlightRDLen);
				txtInfo->OrigRData = mDNSNULL;
				txtInfo->InFlightRData = mDNSNULL;
				}
			break;
		case regState_FetchingZoneData:
		case regState_Registered:
		case regState_Cancelled:
		case regState_Unregistered:
		case regState_NATMap:
		case regState_NoTarget:
		case regState_ExtraQueued:
		case regState_NATError:
			LogMsg("hndlServiceUpdateReply called for service %##s in unexpected state %d with error %ld.  Unlinking.",
				   srs->RR_SRV.resrec.name->c, info->state, err);
			err = mStatus_UnknownErr;
		}

	if ((info->SRVChanged || info->SRVUpdateDeferred) && (info->state == regState_NoTarget || info->state == regState_Registered))
		{
		if (InvokeCallback)
			{
			info->ClientCallbackDeferred = mDNStrue;
			info->DeferredStatus = err;
			}
		info->SRVChanged = mDNSfalse;
		UpdateSRV(m, srs);
		return;
		}

	while (*e)
		{
		uDNS_RegInfo *einfo = &(*e)->r.uDNS_info;
		if (einfo->state == regState_ExtraQueued)
			{
			if (info->state == regState_Registered && !err)
				{
				// extra resource record queued for this service - copy zone info and register
				AssignDomainName(&einfo->zone, &info->zone);
				einfo->ns = info->ns;
				einfo->port = info->port;
				einfo->lease = info->lease;
				sendRecordRegistration(m, &(*e)->r);
				e = &(*e)->next;
				}
			else if (err && einfo->state != regState_Unregistered)
				{
				// unlink extra from list
				einfo->state = regState_Unregistered;
				*e = (*e)->next;
				}
			else e = &(*e)->next;
			}
		else e = &(*e)->next;
		}

	srs->RR_SRV.ThisAPInterval = INIT_UCAST_POLL_INTERVAL - 1;  // reset retry delay for future refreshes, dereg, etc.
	if (info->state == regState_Unregistered) unlinkSRS(m, srs);
	else if (txtInfo->QueuedRData && info->state == regState_Registered)
		{
		if (InvokeCallback)
			{
			// if we were supposed to give a client callback, we'll do it after we update the primary txt record
			info->ClientCallbackDeferred = mDNStrue;
			info->DeferredStatus = err;
			}
		info->state = regState_UpdatePending;
		txtInfo->InFlightRData = txtInfo->QueuedRData;
		txtInfo->InFlightRDLen = txtInfo->QueuedRDLen;
		info->OrigRData = txt->resrec.rdata;
		info->OrigRDLen = txt->resrec.rdlength;
		txtInfo->QueuedRData = mDNSNULL;
		SendServiceRegistration(m, srs);
		return;
		}
	
	m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
	if (InvokeCallback) srs->ServiceCallback(m, srs, err);
	else if (info->ClientCallbackDeferred)
		{
		info->ClientCallbackDeferred = mDNSfalse;
		srs->ServiceCallback(m, srs, info->DeferredStatus);
		}
	m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
	// NOTE: do not touch structures after calling ServiceCallback
	}

mDNSlocal void hndlRecordUpdateReply(mDNS *m, AuthRecord *rr, mStatus err)
	{
	uDNS_RegInfo *info = &rr->uDNS_info;
	mDNSBool InvokeCallback = mDNStrue;
	
	if (info->state == regState_UpdatePending)
		{
		if (err)
			{
			LogMsg("Update record failed for %##s (err %d)", rr->resrec.name->c, err);
			info->state = regState_Unregistered;
			}
		else
			{
			debugf("Update record %##s - success", rr->resrec.name->c);
			info->state = regState_Registered;
			// deallocate old RData
			if (info->UpdateRDCallback) info->UpdateRDCallback(m, rr, info->OrigRData);
			SetNewRData(&rr->resrec, info->InFlightRData, info->InFlightRDLen);
			info->OrigRData = mDNSNULL;
			info->InFlightRData = mDNSNULL;
			}
		}

	if (info->state == regState_DeregPending)
		{
		debugf("Received reply for deregister record %##s type %d", rr->resrec.name->c, rr->resrec.rrtype);
		if (err) LogMsg("ERROR: Deregistration of record %##s type %d failed with error %ld",
						rr->resrec.name->c, rr->resrec.rrtype, err);
		err = mStatus_MemFree;
		info->state = regState_Unregistered;
		}

	if (info->state == regState_DeregDeferred)
		{
		if (err)
			{
			LogMsg("Cancelling deferred deregistration record %##s type %d due to registration error %ld",
				   rr->resrec.name->c, rr->resrec.rrtype, err);
			info->state = regState_Unregistered;
			}
		debugf("Calling deferred deregistration of record %##s type %d",  rr->resrec.name->c, rr->resrec.rrtype);
		info->state = regState_Registered;
		uDNS_DeregisterRecord(m, rr);
		return;
		}

	if (info->state == regState_Pending || info->state == regState_Refresh)
		{
		if (!err)
			{
			info->state = regState_Registered;
			if (info->state == regState_Refresh) InvokeCallback = mDNSfalse;
			}
		else
			{
			if (info->lease && err == mStatus_UnknownErr)
				{
				LogMsg("Re-trying update of record %##s without lease option", rr->resrec.name->c);
				info->lease = mDNSfalse;
				sendRecordRegistration(m, rr);
				return;
				}
			LogMsg("Registration of record %##s type %d failed with error %ld", rr->resrec.name->c, rr->resrec.rrtype, err);
			info->state = regState_Unregistered;
			}
		}
	
	if (info->state == regState_Unregistered) unlinkAR(&m->uDNS_info.RecordRegistrations, rr);
	else rr->ThisAPInterval = INIT_UCAST_POLL_INTERVAL - 1;  // reset retry delay for future refreshes, dereg, etc.

	if (info->QueuedRData && info->state == regState_Registered)
		{
		info->state = regState_UpdatePending;
		info->InFlightRData = info->QueuedRData;
		info->InFlightRDLen = info->QueuedRDLen;
		info->OrigRData = rr->resrec.rdata;
		info->OrigRDLen = rr->resrec.rdlength;
		info->QueuedRData = mDNSNULL;
		sendRecordRegistration(m, rr);
		return;
		}

	if (InvokeCallback)
		{
		m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
		if (rr->RecordCallback) rr->RecordCallback(m, rr, err);
		m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
		}
	}


mDNSlocal void SetUpdateExpiration(mDNS *m, DNSMessage *msg, const mDNSu8 *end, uDNS_RegInfo *info)
	{
	LargeCacheRecord lcr;
	const mDNSu8 *ptr;
	int i;
	mDNSu32 lease = 0;
	mDNSs32 expire;
	
	ptr = LocateAdditionals(msg, end);

	if (info->lease && (ptr = LocateAdditionals(msg, end)))
		{
		for (i = 0; i < msg->h.numAdditionals; i++)
			{
			ptr = GetLargeResourceRecord(m, msg, ptr, end, 0, kDNSRecordTypePacketAdd, &lcr);
			if (!ptr) break;
			if (lcr.r.resrec.rrtype == kDNSType_OPT)
				{
				if (lcr.r.resrec.rdlength < LEASE_OPT_RDLEN) continue;
				if (lcr.r.resrec.rdata->u.opt.opt != kDNSOpt_Lease) continue;
				lease = lcr.r.resrec.rdata->u.opt.OptData.lease;
				break;
				}
			}
		}
	
	if (lease > 0)
		{
		expire = (mDNSPlatformTimeNow(m) + (((mDNSs32)lease * mDNSPlatformOneSecond)) * 3/4);
		if (info->state == regState_UpdatePending)
            // if updating individual record, the service record set may expire sooner
			{ if (expire - info->expire < 0) info->expire = expire; }
		else info->expire = expire;
		}
	else info->lease = mDNSfalse;
	}

mDNSexport void uDNS_ReceiveNATMap(mDNS *m, mDNSu8 *pkt, mDNSu16 len)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	NATTraversalInfo *ptr = u->NATTraversals;
	NATOp_t op;
	
	// check length, version, opcode
	if (len < sizeof(NATPortMapReply) && len < sizeof(NATAddrReply)) { LogMsg("NAT Traversal message too short (%d bytes)", len); return; }
	if (pkt[0] != NATMAP_VERS) { LogMsg("Received NAT Traversal response with version %d (expect version %d)", pkt[0], NATMAP_VERS); return; }
	op = pkt[1];
	if (!(op & NATMAP_RESPONSE_MASK)) { LogMsg("Received NAT Traversal message that is not a response (opcode %d)", op); return; }

	while (ptr)
		{
		if ((ptr->state == NATState_Request || ptr->state == NATState_Refresh) && (ptr->op | NATMAP_RESPONSE_MASK) == op)
			if (ptr->ReceiveResponse(ptr, m, pkt, len)) break;  // note callback may invalidate ptr if it return value is non-zero
		ptr = ptr->next;
		}
	}

mDNSlocal const domainname *DNSRelayTestQuestion = (domainname*)
	"\x1" "1" "\x1" "0" "\x1" "0" "\x3" "127" "\xa" "dnsbugtest"
	"\x1" "1" "\x1" "0" "\x1" "0" "\x3" "127" "\x7" "in-addr" "\x4" "arpa";

// Returns mDNStrue if response was handled
mDNSlocal mDNSBool uDNS_ReceiveTestQuestionResponse(mDNS *const m, DNSMessage *const msg, const mDNSu8 *const end,
	const mDNSAddr *const srcaddr, const mDNSInterfaceID InterfaceID)
	{
	const mDNSu8 *ptr = msg->data;
	DNSQuestion q;
	DNSServer *s;
	mDNSu32 result = 0;
	mDNSBool found = mDNSfalse;

	// 1. Find out if this is an answer to one of our test questions
	if (msg->h.numQuestions != 1) return(mDNSfalse);
	ptr = getQuestion(msg, ptr, end, InterfaceID, &q);
	if (!ptr) return(mDNSfalse);
	if (q.qtype != kDNSType_PTR || q.qclass != kDNSClass_IN) return(mDNSfalse);
	if (!SameDomainName(&q.qname, DNSRelayTestQuestion)) return(mDNSfalse);

	// 2. If the DNS relay gave us a positive response, then it's got buggy firmware
	// else, if the DNS relay gave us an error or no-answer response, it passed our test
	if ((msg->h.flags.b[1] & kDNSFlag1_RC) == kDNSFlag1_RC_NoErr && msg->h.numAnswers > 0)
		result = DNSServer_Failed;
	else
		result = DNSServer_Passed;

	// 3. Find occurrences of this server in our list, and mark them appropriately
	for (s = m->uDNS_info.Servers; s; s = s->next)
		if (mDNSSameAddress(srcaddr, &s->addr) && s->teststate != result)
			{ s->teststate = result; found = mDNStrue; }

	// 4. Assuming we found the server in question in our list (don't want to risk being victim of a deliberate DOS attack here)
	// log a message to let the user know why Wide-Area Service Discovery isn't working
	if (found && result == DNSServer_Failed)
		LogMsg("NOTE: Wide-Area Service Discovery disabled to avoid crashing defective DNS relay %#a.", srcaddr);

	return(mDNStrue); // Return mDNStrue to tell uDNS_ReceiveMsg it doens't need to process this packet further
	}

mDNSexport void uDNS_ReceiveMsg(mDNS *const m, DNSMessage *const msg, const mDNSu8 *const end,
	const mDNSAddr *const srcaddr, const mDNSIPPort srcport, const mDNSAddr *const dstaddr,
	const mDNSIPPort dstport, const mDNSInterfaceID InterfaceID)
	{
	DNSQuestion *qptr;
	AuthRecord *rptr;
	ServiceRecordSet *sptr;
	mStatus err = mStatus_NoError;
	uDNS_GlobalInfo *u = &m->uDNS_info;
	
	mDNSu8 StdR    = kDNSFlag0_QR_Response | kDNSFlag0_OP_StdQuery;
	mDNSu8 UpdateR = kDNSFlag0_QR_Response | kDNSFlag0_OP_Update;
	mDNSu8 QR_OP   = (mDNSu8)(msg->h.flags.b[0] & kDNSFlag0_QROP_Mask);
	mDNSu8 rcode   = (mDNSu8)(msg->h.flags.b[1] & kDNSFlag1_RC);

	mDNSs32 timenow = mDNSPlatformTimeNow(m);
	
    // unused
	(void)dstaddr;
	(void)dstport;
	(void)InterfaceID;
	
	if (QR_OP == StdR)
		{
		// !!!KRS we should to a table lookup here to see if it answers an LLQ or a 1-shot
		// LLQ Responses over TCP not currently supported
		if (srcaddr && recvLLQResponse(m, msg, end, srcaddr, srcport, InterfaceID)) return;
	
		if (uDNS_ReceiveTestQuestionResponse(m, msg, end, srcaddr, InterfaceID)) return;
	
		for (qptr = u->ActiveQueries; qptr; qptr = qptr->next)
			{
			//!!!KRS we should have a hashtable, hashed on message id
			if (qptr->uDNS_info.id.NotAnInteger == msg->h.id.NotAnInteger)
				{
				if (timenow - (qptr->LastQTime + RESPONSE_WINDOW) > 0)
					{ debugf("uDNS_ReceiveMsg - response received after maximum allowed window.  Discarding"); return; }
				if (msg->h.flags.b[0] & kDNSFlag0_TC)
					{ hndlTruncatedAnswer(qptr, srcaddr, m); return; }
				else
					{
					u->CurrentQuery = qptr;
					qptr->uDNS_info.responseCallback(m, msg, end, qptr, qptr->uDNS_info.context);
					u->CurrentQuery = mDNSNULL;
					// Note: responseCallback can invalidate qptr
					return;
					}
				}
			}
		}
	if (QR_OP == UpdateR)
		{
		for (sptr = u->ServiceRegistrations; sptr; sptr = sptr->next)
			{
			if (sptr->uDNS_info.id.NotAnInteger == msg->h.id.NotAnInteger)
				{
				err = checkUpdateResult(sptr->RR_SRV.resrec.name, rcode, m, msg, end);
				if (!err) SetUpdateExpiration(m, msg, end, &sptr->uDNS_info);
				hndlServiceUpdateReply(m, sptr, err);
				return;
				}
			}
		for (rptr = u->RecordRegistrations; rptr; rptr = rptr->next)
			{
			if (rptr->uDNS_info.id.NotAnInteger == msg->h.id.NotAnInteger)
				{
				err = checkUpdateResult(rptr->resrec.name, rcode, m, msg, end);
				if (!err) SetUpdateExpiration(m, msg, end, &rptr->uDNS_info);
				hndlRecordUpdateReply(m, rptr, err);
				return;
				}
			}
		}
	debugf("Received unexpected response: ID %d matches no active records", mDNSVal16(msg->h.id));
	}

// lookup a DNS Server, matching by name in split-dns configurations.  Result stored in addr parameter if successful
mDNSlocal DNSServer *GetServerForName(uDNS_GlobalInfo *u, const domainname *name)
    {
	DNSServer *curmatch = mDNSNULL, *p = u->Servers;
	int i, curmatchlen = -1;
	int ncount = name ? CountLabels(name) : 0;

	while (p)
		{
		int scount = CountLabels(&p->domain);
		if (scount <= ncount && scount > curmatchlen)
			{
			// only inspect if server's domain is longer than current best match and shorter than the name itself
			const domainname *tail = name;
			for (i = 0; i < ncount - scount; i++)
				tail = (domainname *)(tail->c + 1 + tail->c[0]);  // find "tail" (scount labels) of name
			if (SameDomainName(tail, &p->domain)) { curmatch = p; curmatchlen = scount; }
			}
		p = p->next;
		}
	return(curmatch);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Query Routines
#endif

#define sameID(x,y) mDNSPlatformMemSame(x,y,8)

mDNSlocal void initializeQuery(DNSMessage *msg, DNSQuestion *question)
	{
	ubzero(msg, sizeof(msg));
    InitializeDNSMessage(&msg->h, question->uDNS_info.id, uQueryFlags);
	}

mDNSlocal mStatus constructQueryMsg(DNSMessage *msg, mDNSu8 **endPtr, DNSQuestion *const question)
	{
	initializeQuery(msg, question);

	*endPtr = putQuestion(msg, msg->data, msg->data + AbsoluteMaxDNSMessageData, &question->qname, question->qtype, question->qclass);
    if (!*endPtr)
        {
        LogMsg("ERROR: Unicast query out of space in packet");
        return mStatus_UnknownErr;
        }
	return mStatus_NoError;
	}

mDNSlocal mDNSu8 *putLLQ(DNSMessage *const msg, mDNSu8 *ptr, DNSQuestion *question, LLQOptData *data, mDNSBool includeQuestion)
	{
	AuthRecord rr;
	ResourceRecord *opt = &rr.resrec;
	rdataOpt *optRD;
	
	//!!!KRS when we implement multiple llqs per message, we'll need to memmove anything past the question section
	if (includeQuestion)
		{
		ptr = putQuestion(msg, ptr, msg->data + AbsoluteMaxDNSMessageData, &question->qname, question->qtype, question->qclass);
		if (!ptr) { LogMsg("ERROR: putLLQ - putQuestion"); return mDNSNULL; }
		}
	// locate OptRR if it exists, set pointer to end
	// !!!KRS implement me

	
	// format opt rr (fields not specified are zero-valued)
	ubzero(&rr, sizeof(AuthRecord));
	mDNS_SetupResourceRecord(&rr, mDNSNULL, mDNSInterface_Any, kDNSType_OPT, kStandardTTL, kDNSRecordTypeKnownUnique, mDNSNULL, mDNSNULL);
	opt->rdlength = LLQ_OPT_RDLEN;
	opt->rdestimate = LLQ_OPT_RDLEN;

	optRD = &rr.resrec.rdata->u.opt;
	optRD->opt = kDNSOpt_LLQ;
	optRD->optlen = LLQ_OPTLEN;
	umemcpy(&optRD->OptData.llq, data, sizeof(*data));
	ptr = PutResourceRecordTTLJumbo(msg, ptr, &msg->h.numAdditionals, opt, 0);
	if (!ptr) { LogMsg("ERROR: putLLQ - PutResourceRecordTTLJumbo"); return mDNSNULL; }

	return ptr;
	}

			  
mDNSlocal mDNSBool getLLQAtIndex(mDNS *m, DNSMessage *msg, const mDNSu8 *end, LLQOptData *llq, int index)
	{
	LargeCacheRecord lcr;
	int i;
	const mDNSu8 *ptr;
	
	ubzero(&lcr, sizeof(lcr));
	
	ptr = LocateAdditionals(msg, end);
	if (!ptr) return mDNSfalse;
	
	// find the last additional
	for (i = 0; i < msg->h.numAdditionals; i++)
//		{ ptr = GetLargeResourceRecord(m, msg, ptr, end, 0, kDNSRecordTypePacketAdd, &lcr); if (!ptr) return mDNSfalse; }
//!!!KRS workaround for LH server bug, which puts OPT as first additional
		{ ptr = GetLargeResourceRecord(m, msg, ptr, end, 0, kDNSRecordTypePacketAdd, &lcr); if (!ptr) return mDNSfalse; if (lcr.r.resrec.rrtype == kDNSType_OPT) break; }
	if (lcr.r.resrec.rrtype != kDNSType_OPT) return mDNSfalse;
	if (lcr.r.resrec.rdlength < (index + 1) * LLQ_OPT_RDLEN) return mDNSfalse;  // rdata too small
	umemcpy(llq, (mDNSu8 *)&lcr.r.resrec.rdata->u.opt.OptData.llq + (index * sizeof(*llq)), sizeof(*llq));
	return mDNStrue;
	}

mDNSlocal void recvRefreshReply(mDNS *m, DNSMessage *msg, const mDNSu8 *end, DNSQuestion *q)
	{
	LLQ_Info *qInfo;
	LLQOptData pktData;

	qInfo = q->uDNS_info.llq;
	if (!getLLQAtIndex(m, msg, end, &pktData, 0)) { LogMsg("ERROR recvRefreshReply - getLLQAtIndex"); return; }
	if (pktData.llqOp != kLLQOp_Refresh) return;
	if (!sameID(pktData.id, qInfo->id)) { LogMsg("recvRefreshReply - ID mismatch.  Discarding");  return; }
	if (pktData.err != LLQErr_NoError) { LogMsg("recvRefreshReply: received error %d from server", pktData.err); return; }

	qInfo->expire = mDNSPlatformTimeNow(m) + ((mDNSs32)pktData.lease * mDNSPlatformOneSecond);
	qInfo->retry = qInfo->expire - ((mDNSs32)pktData.lease * mDNSPlatformOneSecond/2);
 
	qInfo->origLease = pktData.lease;
	qInfo->state = LLQ_Established;
	}

mDNSlocal void sendLLQRefresh(mDNS *m, DNSQuestion *q, mDNSu32 lease)
	{
	DNSMessage msg;
	mDNSu8 *end;
	LLQOptData llq;
	LLQ_Info *info = q->uDNS_info.llq;
	mStatus err;
	mDNSs32 timenow;

	timenow = mDNSPlatformTimeNow(m);
	if ((info->state == LLQ_Refresh && info->ntries >= kLLQ_MAX_TRIES) ||
		info->expire - timenow < 0)
		{
		LogMsg("Unable to refresh LLQ %##s - will retry in %d minutes", q->qname.c, kLLQ_DEF_RETRY/60);
		info->state = LLQ_Retry;
		info->retry = mDNSPlatformTimeNow(m) + kLLQ_DEF_RETRY * mDNSPlatformOneSecond;
		info->deriveRemovesOnResume = mDNStrue;
		return;
		//!!!KRS handle this - periodically try to re-establish
		}

	llq.vers = kLLQ_Vers;
	llq.llqOp = kLLQOp_Refresh;
	llq.err = LLQErr_NoError;
	umemcpy(llq.id, info->id, 8);
	llq.lease = lease;

	initializeQuery(&msg, q);
	end = putLLQ(&msg, msg.data, q, &llq, mDNStrue);
	if (!end) { LogMsg("ERROR: sendLLQRefresh - putLLQ"); return; }
	
	err = mDNSSendDNSMessage(m, &msg, end, mDNSInterface_Any, &info->servAddr, info->servPort, -1, mDNSNULL);
	if (err) debugf("ERROR: sendLLQRefresh - mDNSSendDNSMessage returned %ld", err);

	if (info->state == LLQ_Established) info->ntries = 1;
	else info->ntries++;
	info->state = LLQ_Refresh;
	q->LastQTime = timenow;
	info->retry = (info->expire - q->LastQTime) / 2;
	}

mDNSlocal mDNSBool recvLLQEvent(mDNS *m, DNSQuestion *q, DNSMessage *msg, const mDNSu8 *end, const mDNSAddr *srcaddr, mDNSIPPort srcport, mDNSInterfaceID InterfaceID)
	{
	DNSMessage ack;
	mDNSu8 *ackEnd = ack.data;
	mStatus err;
	LLQOptData opt;
	
	(void)InterfaceID;  // unused

    // find Opt RR, verify correct ID
	if (!getLLQAtIndex(m, msg, end, &opt, 0))  { debugf("Pkt does not contain LLQ Opt");                                   return mDNSfalse; }
	if (!q->uDNS_info.llq) { LogMsg("Error: recvLLQEvent - question object does not contain LLQ metadata");                return mDNSfalse; }
	if (!sameID(opt.id, q->uDNS_info.llq->id)) {                                                                           return mDNSfalse; }
	if (opt.llqOp != kLLQOp_Event) { if (!q->uDNS_info.llq->ntries) LogMsg("recvLLQEvent - Bad LLQ Opcode %d", opt.llqOp); return mDNSfalse; }

    // invoke response handler
	m->uDNS_info.CurrentQuery = q;
	q->uDNS_info.responseCallback(m, msg, end, q, q->uDNS_info.context);
	if (m->uDNS_info.CurrentQuery != q) return mDNStrue;
	
    //  format and send ack
	InitializeDNSMessage(&ack.h, msg->h.id, ResponseFlags);
	ackEnd = putLLQ(&ack, ack.data, mDNSNULL, &opt, mDNSfalse);
	if (!ackEnd) { LogMsg("ERROR: recvLLQEvent - putLLQ");  return mDNSfalse; }
	err = mDNSSendDNSMessage(m, &ack, ackEnd, mDNSInterface_Any, srcaddr, srcport, -1, mDNSNULL);
	if (err) debugf("ERROR: recvLLQEvent - mDNSSendDNSMessage returned %ld", err);
	return mDNStrue;
	}



mDNSlocal void hndlChallengeResponseAck(mDNS *m, DNSMessage *pktMsg, const mDNSu8 *end, LLQOptData *llq, DNSQuestion *q)
	{
	LLQ_Info *info = q->uDNS_info.llq;
	
	if (llq->err) { LogMsg("hndlChallengeResponseAck - received error %d from server", llq->err); goto error; }
	if (!sameID(info->id, llq->id)) { LogMsg("hndlChallengeResponseAck - ID changed.  discarding"); return; } // this can happen rarely (on packet loss + reordering)
	info->expire = mDNSPlatformTimeNow(m) + ((mDNSs32)llq->lease * mDNSPlatformOneSecond);
	info->retry = info->expire - ((mDNSs32)llq->lease * mDNSPlatformOneSecond / 2);
 
	info->origLease = llq->lease;
	info->state = LLQ_Established;
	
	q->uDNS_info.responseCallback = llqResponseHndlr;
	llqResponseHndlr(m, pktMsg, end, q, mDNSNULL);
	return;

	error:
	info->state = LLQ_Error;
	}

mDNSlocal void sendChallengeResponse(mDNS *m, DNSQuestion *q, LLQOptData *llq)
	{
	LLQ_Info *info = q->uDNS_info.llq;
	DNSMessage response;
	mDNSu8 *responsePtr = response.data;
	mStatus err;
	LLQOptData llqBuf;
	mDNSs32 timenow = mDNSPlatformTimeNow(m);
	
	if (info->ntries++ == kLLQ_MAX_TRIES)
		{
		LogMsg("sendChallengeResponse: %d failed attempts for LLQ %##s. Will re-try in %d minutes",
			   kLLQ_MAX_TRIES, q->qname.c, kLLQ_DEF_RETRY / 60);
		info->state = LLQ_Retry;
		info->retry = timenow + (kLLQ_DEF_RETRY * mDNSPlatformOneSecond);
		// !!!KRS give a callback error in these cases?
		return;
		}
		
	if (!llq)
		{
		llq = &llqBuf;
		llq->vers    = kLLQ_Vers;
		llq->llqOp   = kLLQOp_Setup;
		llq->err     = LLQErr_NoError;
		umemcpy(llq->id, info->id, 8);
		llq->lease    = info->origLease;
		}

	q->LastQTime = timenow;
	info->retry = timenow  + (kLLQ_INIT_RESEND * info->ntries * mDNSPlatformOneSecond);
	
	if (constructQueryMsg(&response, &responsePtr, q)) goto error;
	responsePtr = putLLQ(&response, responsePtr, q, llq, mDNSfalse);
	if (!responsePtr) { LogMsg("ERROR: sendChallengeResponse - putLLQ"); goto error; }
	
	err = mDNSSendDNSMessage(m, &response, responsePtr, mDNSInterface_Any, &info->servAddr, info->servPort, -1, mDNSNULL);
	if (err) debugf("ERROR: sendChallengeResponse - mDNSSendDNSMessage returned %ld", err);
	// on error, we procede as normal and retry after the appropriate interval

	return;

	error:
	info->state = LLQ_Error;
	}



mDNSlocal void hndlRequestChallenge(mDNS *m, DNSMessage *pktMsg, const mDNSu8 *end, LLQOptData *llq, DNSQuestion *q)
	{
	LLQ_Info *info = q->uDNS_info.llq;
	mDNSs32 timenow = mDNSPlatformTimeNow(m);
	switch(llq->err)
		{
		case LLQErr_NoError: break;
		case LLQErr_ServFull:
			LogMsg("hndlRequestChallenge - received ServFull from server for LLQ %##s.  Retry in %lu sec", q->qname.c, llq->lease);
			info->retry = timenow + ((mDNSs32)llq->lease * mDNSPlatformOneSecond);
			info->state = LLQ_Retry;
			simpleResponseHndlr(m, pktMsg, end, q, mDNSNULL);  // get available answers
			info->deriveRemovesOnResume = mDNStrue;
		case LLQErr_Static:
			info->state = LLQ_Static;
			LogMsg("LLQ %##s: static", q->qname.c);
			simpleResponseHndlr(m, pktMsg, end, q, mDNSNULL);
			return;
		case LLQErr_FormErr:
			LogMsg("ERROR: hndlRequestChallenge - received FormErr from server for LLQ %##s", q->qname.c);
			goto error;
		case LLQErr_BadVers:
			LogMsg("ERROR: hndlRequestChallenge - received BadVers from server");
			goto error;
		case LLQErr_UnknownErr:
			LogMsg("ERROR: hndlRequestChallenge - received UnknownErr from server for LLQ %##s", q->qname.c);
			goto error;
		default:
			LogMsg("ERROR: hndlRequestChallenge - received invalid error %d for LLQ %##s", llq->err, q->qname.c);
			goto error;
		}

	if (info->origLease != llq->lease)
		debugf("hndlRequestChallenge: requested lease %lu, granted lease %lu", info->origLease, llq->lease);

	// cache expiration in case we go to sleep before finishing setup
	info->origLease = llq->lease;
	info->expire = timenow + ((mDNSs32)llq->lease * mDNSPlatformOneSecond);

	// update state
	info->state = LLQ_SecondaryRequest;
	umemcpy(info->id, llq->id, 8);
	info->ntries = 0; // first attempt to send response

	sendChallengeResponse(m, q, llq);
	return;


	error:
	info->state = LLQ_Error;
	}


// response handler for initial and secondary setup responses
mDNSlocal void recvSetupResponse(mDNS *m, DNSMessage *pktMsg, const mDNSu8 *end, DNSQuestion *q, void *clientContext)
	{
	DNSQuestion pktQuestion;
	LLQOptData llq;
	const mDNSu8 *ptr = pktMsg->data;
	LLQ_Info *info = q->uDNS_info.llq;
	mDNSu8 rcode = (mDNSu8)(pktMsg->h.flags.b[1] & kDNSFlag1_RC);

	(void)clientContext;  // unused
	
	if (rcode && rcode != kDNSFlag1_RC_NXDomain) goto poll;
	
	ptr = getQuestion(pktMsg, ptr, end, 0, &pktQuestion);
	if (!ptr) { LogMsg("ERROR: recvSetupResponse - getQuestion"); goto poll; }
	if (!SameDomainName(&q->qname, &pktQuestion.qname))
		{ LogMsg("ERROR: recvSetupResponse - mismatched question in response for llq setup %##s", q->qname.c);   goto poll; }

	if (!getLLQAtIndex(m, pktMsg, end, &llq, 0)) { debugf("recvSetupResponse - GetLLQAtIndex"); goto poll; }
	if (llq.llqOp != kLLQOp_Setup) { LogMsg("ERROR: recvSetupResponse - bad op %d", llq.llqOp); goto poll; }
	if (llq.vers != kLLQ_Vers) { LogMsg("ERROR: recvSetupResponse - bad vers %d", llq.vers);  goto poll; }

	if (info->state == LLQ_InitialRequest) { hndlRequestChallenge(m, pktMsg, end, &llq, q); return; }
	if (info->state == LLQ_SecondaryRequest) { hndlChallengeResponseAck(m, pktMsg, end, &llq, q); return; }
	LogMsg("recvSetupResponse - bad state %d", info->state);

	poll:
	info->state = LLQ_Poll;
	q->uDNS_info.responseCallback = llqResponseHndlr;
	info->question->LastQTime = mDNSPlatformTimeNow(m) - (2 * INIT_UCAST_POLL_INTERVAL);  // trigger immediate poll
	info->question->ThisQInterval = INIT_UCAST_POLL_INTERVAL;
	}

mDNSlocal void startLLQHandshake(mDNS *m, LLQ_Info *info, mDNSBool defer)
	{
	DNSMessage msg;
	mDNSu8 *end;
	LLQOptData llqData;
	DNSQuestion *q = info->question;
	mStatus err = mStatus_NoError;
	mDNSs32 timenow = mDNSPlatformTimeNow(m);
	uDNS_GlobalInfo *u = &m->uDNS_info;
	
	if (IsPrivateV4Addr(&u->AdvertisedV4))
		{
		if (!u->LLQNatInfo)
			{
			info->state = LLQ_NatMapWait;
			StartLLQNatMap(m);
			return;
			}
		if (u->LLQNatInfo->state == NATState_Error) goto poll;
		if (u->LLQNatInfo->state != NATState_Established && u->LLQNatInfo->state != NATState_Legacy)
			{ info->state = LLQ_NatMapWait; info->NATMap = mDNStrue; return; }
		info->NATMap = mDNStrue;  // this llq references the global llq nat mapping
		}
	
	if (info->ntries++ >= kLLQ_MAX_TRIES)
		{
		debugf("startLLQHandshake: %d failed attempts for LLQ %##s.  Polling.", kLLQ_MAX_TRIES, q->qname.c, kLLQ_DEF_RETRY / 60);
		goto poll;
		}
	
    // set llq rdata
	llqData.vers    = kLLQ_Vers;
	llqData.llqOp   = kLLQOp_Setup;
	llqData.err     = LLQErr_NoError;
	ubzero(llqData.id, 8);
	llqData.lease    = kLLQ_DefLease;

	initializeQuery(&msg, q);
	end = putLLQ(&msg, msg.data, q, &llqData, mDNStrue);
	if (!end)
		{
		LogMsg("ERROR: startLLQHandshake - putLLQ");
		info->state = LLQ_Error;
		return;
		}

	if (!defer) // if we are to defer, we simply set the retry timers so the request goes out in the future
		{
		err = mDNSSendDNSMessage(m, &msg, end, mDNSInterface_Any, &info->servAddr, info->servPort, -1, mDNSNULL);
		if (err) debugf("ERROR: startLLQHandshake - mDNSSendDNSMessage returned %ld", err);
		// on error, we procede as normal and retry after the appropriate interval
		}
	
	// update question/info state
	info->state = LLQ_InitialRequest;
	info->origLease = kLLQ_DefLease;
    info->retry = timenow + (kLLQ_INIT_RESEND * mDNSPlatformOneSecond);
	q->LastQTime = timenow;
	q->uDNS_info.responseCallback = recvSetupResponse;
	q->uDNS_info.internal = mDNStrue;
	return;

	poll:
	info->question->uDNS_info.responseCallback = llqResponseHndlr;
	info->state = LLQ_Poll;
	info->question->LastQTime = mDNSPlatformTimeNow(m) - (2 * INIT_UCAST_POLL_INTERVAL);  // trigger immediate poll
	info->question->ThisQInterval = INIT_UCAST_POLL_INTERVAL;
	}

// wrapper for startLLQHandshake, invoked by async op callback
mDNSlocal void startLLQHandshakeCallback(mStatus err, mDNS *const m, void *llqInfo, const AsyncOpResult *result)
	{
	LLQ_Info *info = (LLQ_Info *)llqInfo;
	const zoneData_t *zoneInfo = mDNSNULL;
	
    // check state first to make sure it is OK to touch question object
	if (info->state == LLQ_Cancelled)
		{
		// StopQuery was called while we were getting the zone info
		debugf("startLLQHandshake - LLQ Cancelled.");
		info->question = mDNSNULL;  // question may be deallocated
		ufree(info);
		return;
		}

	if (!info->question)
		{ LogMsg("ERROR: startLLQHandshakeCallback invoked with NULL question"); goto error; }

	if (info->state != LLQ_GetZoneInfo)
		{ LogMsg("ERROR: startLLQHandshake - bad state %d", info->state); goto error; }

	if (err)
		{ LogMsg("ERROR: startLLQHandshakeCallback %##s invoked with error code %ld", info->question->qname.c, err); goto poll; }

	if (!result)
		{ LogMsg("ERROR: startLLQHandshakeCallback invoked with NULL result and no error code"); goto error; }
	
	zoneInfo = &result->zoneData;

	if (!zoneInfo->llqPort.NotAnInteger)
		{ debugf("LLQ port lookup failed - reverting to polling"); info->servPort.NotAnInteger = 0; goto poll; }
	
    // cache necessary zone data
	info->servAddr = zoneInfo->primaryAddr;
	info->servPort = zoneInfo->llqPort;
    info->ntries = 0;

	if (info->state == LLQ_SuspendDeferred) info->state = LLQ_Suspended;
	else startLLQHandshake(m, info, mDNSfalse);
	return;

	poll:
	info->question->uDNS_info.responseCallback = llqResponseHndlr;
	info->state = LLQ_Poll;
	info->question->LastQTime = mDNSPlatformTimeNow(m) - (2 * INIT_UCAST_POLL_INTERVAL);  // trigger immediate poll
	info->question->ThisQInterval = INIT_UCAST_POLL_INTERVAL;
	return;

	error:
	info->state = LLQ_Error;
	}

mDNSlocal mStatus startLLQ(mDNS *m, DNSQuestion *question)
    {
	LLQ_Info *info;
	mStatus err = mStatus_NoError;
	
	// allocate / init info struct
    info = umalloc(sizeof(LLQ_Info));
    if (!info) { LogMsg("ERROR: startLLQ - malloc"); return mStatus_NoMemoryErr; }
	ubzero(info, sizeof(LLQ_Info));
    info->state = LLQ_GetZoneInfo;
	
	// link info/question
	info->question = question;
	question->uDNS_info.llq = info;

	question->uDNS_info.responseCallback = llqResponseHndlr;
	
	err = startGetZoneData(&question->qname, m, mDNSfalse, mDNStrue, startLLQHandshakeCallback, info);
    if (err)
		{
		LogMsg("ERROR: startLLQ - startGetZoneData returned %ld", err);
		info->question = mDNSNULL;
		ufree(info);
		question->uDNS_info.llq = mDNSNULL;
		return err;
		}

	LinkActiveQuestion(&m->uDNS_info, question);
	return err;
	}

mDNSlocal mDNSBool recvLLQResponse(mDNS *m, DNSMessage *msg, const mDNSu8 *end, const mDNSAddr *srcaddr, mDNSIPPort srcport, const mDNSInterfaceID InterfaceID)
	{
	DNSQuestion pktQ, *q;
	uDNS_GlobalInfo *u = &m->uDNS_info;
	const mDNSu8 *ptr = msg->data;
	LLQ_Info *llqInfo;

	if (!msg->h.numQuestions) return mDNSfalse;

	ptr = getQuestion(msg, ptr, end, 0, &pktQ);
	if (!ptr) return mDNSfalse;
	pktQ.uDNS_info.id = msg->h.id;
	
	q = u->ActiveQueries;
	while (q)
		{
		llqInfo = q->uDNS_info.llq;
		if (q->LongLived &&
			llqInfo &&
			q->qnamehash == pktQ.qnamehash &&
			q->qtype == pktQ.qtype &&
			SameDomainName(&q->qname, &pktQ.qname))
			{
			u->CurrentQuery = q;
			if (llqInfo->state == LLQ_Established || (llqInfo->state == LLQ_Refresh && msg->h.numAnswers))
				{ if (recvLLQEvent(m, q, msg, end, srcaddr, srcport, InterfaceID)) return mDNStrue; }
			else if (msg->h.id.NotAnInteger == q->uDNS_info.id.NotAnInteger)
				{
				if (llqInfo->state == LLQ_Refresh && msg->h.numAdditionals && !msg->h.numAnswers)
					{ recvRefreshReply(m, msg, end, q); return mDNStrue; }
				if (llqInfo->state < LLQ_Static)
					{
					if ((llqInfo->state != LLQ_InitialRequest && llqInfo->state != LLQ_SecondaryRequest) || mDNSSameAddress(srcaddr, &llqInfo->servAddr))
						{ q->uDNS_info.responseCallback(m, msg, end, q, q->uDNS_info.context); return mDNStrue; }
					}
				}
			}
		q = q->next;
		}
	return mDNSfalse;
	}

mDNSexport mDNSBool uDNS_IsActiveQuery(DNSQuestion *const question, uDNS_GlobalInfo *u)
    {
	DNSQuestion *q;

	for (q = u->ActiveQueries; q; q = q->next)
		{
		if (q == question)
			{
			if (!question->uDNS_info.id.NotAnInteger || question->InterfaceID == mDNSInterface_LocalOnly || IsLocalDomain(&question->qname))
				LogMsg("Warning: Question %##s in Active Unicast Query list with id %d, interfaceID %p",
					   question->qname.c, question->uDNS_info.id.NotAnInteger, question->InterfaceID);
			return mDNStrue;
			}
		}
	return mDNSfalse;
	}

// stopLLQ happens IN ADDITION to stopQuery
mDNSlocal void stopLLQ(mDNS *m, DNSQuestion *question)
	{
	LLQ_Info *info = question->uDNS_info.llq;
	(void)m;  // unused

	if (!question->LongLived) { LogMsg("ERROR: stopLLQ - LongLived flag not set"); return; }
	if (!info)                { LogMsg("ERROR: stopLLQ - llq info is NULL");       return; }

	switch (info->state)
		{
		case LLQ_UnInit:
			LogMsg("ERROR: stopLLQ - state LLQ_UnInit");
			//!!!KRS should we unlink info<->question here?
			return;
		case LLQ_GetZoneInfo:
		case LLQ_SuspendDeferred:
			info->question = mDNSNULL; // remove ref to question, as it may be freed when we get called back from async op
			info->state = LLQ_Cancelled;
			return;
		case LLQ_Established:
		case LLQ_Refresh:
			// refresh w/ lease 0
			sendLLQRefresh(m, question, 0);
			goto end;
		default:
			debugf("stopLLQ - silently discarding LLQ in state %d", info->state);
			goto end;
		}
	
	end:
	if (info->NATMap) info->NATMap = mDNSfalse;
	CheckForUnreferencedLLQMapping(m);
	info->question = mDNSNULL;
	ufree(info);
	question->uDNS_info.llq = mDNSNULL;
	question->LongLived = mDNSfalse;
	}

mDNSexport mStatus uDNS_StopQuery(mDNS *const m, DNSQuestion *const question)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	DNSQuestion *qptr, *prev = mDNSNULL;
	CacheRecord *ka;
	
	qptr = u->ActiveQueries;
	while (qptr)
        {
        if (qptr == question)
            {
			if (question->LongLived && question->uDNS_info.llq)
				stopLLQ(m, question);
			if (m->uDNS_info.CurrentQuery == question)
				m->uDNS_info.CurrentQuery = m->uDNS_info.CurrentQuery->next;
			while (question->uDNS_info.knownAnswers)
				{
				ka = question->uDNS_info.knownAnswers;
				question->uDNS_info.knownAnswers = question->uDNS_info.knownAnswers->next;
				ufree(ka);
				}
			if (prev) prev->next = question->next;
            else u->ActiveQueries = question->next;
			return mStatus_NoError;
            }
        prev = qptr;
		qptr = qptr->next;
        }
    LogMsg("uDNS_StopQuery: no such active query (%##s)", question->qname.c);
    return mStatus_UnknownErr;
    }

mDNSlocal mStatus startQuery(mDNS *const m, DNSQuestion *const question, mDNSBool internal)
    {
    uDNS_GlobalInfo *u = &m->uDNS_info;
    //!!!KRS we should check if the question is already in our activequestion list
	if (!ValidateDomainName(&question->qname))
		{
		LogMsg("Attempt to start query with invalid qname %##s (%s)", question->qname.c, DNSTypeName(question->qtype));
		return mStatus_Invalid;
		}
		
	question->next = mDNSNULL;
	question->qnamehash = DomainNameHashValue(&question->qname);    // to do quick domain name comparisons
    question->uDNS_info.id = newMessageID(u);
	question->uDNS_info.Answered = mDNSfalse;
	
	// break here if its and LLQ
	if (question->LongLived) return startLLQ(m, question);

	question->ThisQInterval = INIT_UCAST_POLL_INTERVAL / 2;
	question->LastQTime = mDNSPlatformTimeNow(m) - question->ThisQInterval;
    // store the question/id in active question list
	question->uDNS_info.internal = internal;
	LinkActiveQuestion(u, question);
	question->uDNS_info.knownAnswers = mDNSNULL;
	LogOperation("uDNS startQuery: %##s (%s)", question->qname.c, DNSTypeName(question->qtype));
	
	return mStatus_NoError;
	}
	
mDNSexport mStatus uDNS_StartQuery(mDNS *const m, DNSQuestion *const question)
    {
	ubzero(&question->uDNS_info, sizeof(uDNS_QuestionInfo));
	question->uDNS_info.responseCallback = simpleResponseHndlr;
	question->uDNS_info.context = mDNSNULL;
	//LogOperation("uDNS_StartQuery %##s (%s)", question->qname.c, DNSTypeName(question->qtype));
	return startQuery(m, question, 0);
    }

// explicitly set response handler
mDNSlocal mStatus startInternalQuery(DNSQuestion *q, mDNS *m, InternalResponseHndlr callback, void *hndlrContext)
    {
	ubzero(&q->uDNS_info, sizeof(uDNS_QuestionInfo));
    q->QuestionContext = hndlrContext;
    q->uDNS_info.responseCallback = callback;
	q->uDNS_info.context = hndlrContext;
    return startQuery(m, q, 1);
    }



// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Domain -> Name Server Conversion
#endif


/* startGetZoneData
 *
 * Asynchronously find the address of the nameserver for the enclosing zone for a given domain name,
 * i.e. the server to which update and LLQ requests will be sent for a given name.  Once the address is
 * derived, it will be passed to the callback, along with a context pointer.  If the zone cannot
 * be determined or if an error occurs, an all-zeros address will be passed and a message will be
 * written to the syslog.
 *
 * If the FindUpdatePort arg is set, the port on which the server accepts dynamic updates is determined
 * by querying for the _dns-update._udp.<zone>. SRV record.  Likewise, if the FindLLQPort arg is set,
 * the port on which the server accepts long lived queries is determined by querying for
 * _dns-llq._udp.<zone>. record.  If either of these queries fail, or flags are not specified,
 * the llqPort and updatePort fields in the result structure are set to zero.
 *
 *  Steps for deriving the zone name are as follows:
 *
 * Query for an SOA record for the required domain.  If we don't get an answer (or an SOA in the Authority
 * section), we strip the leading label from the name and repeat, until we get an answer.
 *
 * The name of the SOA record is our enclosing zone.  The mname field in the SOA rdata is the domain
 * name of the primary NS.
 *
 * We verify that there is an NS record with this zone for a name and the mname for its rdata.
 * (!!!KRS this seems redundant, but BIND does this, and it should normally be zero-overhead since
 * the NS query will get us address records in the additionals section, which we'd otherwise have to
 * explicitly query for.)
 *
 * We then query for the address record for this nameserver (if it is not in the addionals section of
 * the NS record response.)
 */
 

// state machine types and structs
//

// state machine states
typedef enum
    {
    init,
    lookupSOA,
	foundZone,
	lookupNS,
	foundNS,
	lookupA,
	foundA,
	lookupPort,
	foundPort,
	complete
    } ntaState;

// state machine actions
typedef enum
    {
    smContinue,  // continue immediately to next state
    smBreak,     // break until next packet/timeout
	smError      // terminal error - cleanup and abort
    } smAction;
 
typedef struct
    {
    domainname 	origName;            // name we originally try to convert
    domainname 	*curSOA;             // name we have an outstanding SOA query for
    ntaState  	state;               // determines what we do upon receiving a packet
    mDNS	    *m;
    domainname  zone;                // left-hand-side of SOA record
    mDNSu16     zoneClass;
    domainname  ns;                  // mname in SOA rdata, verified in confirmNS state
    mDNSv4Addr  addr;                // address of nameserver
    DNSQuestion question;            // storage for any active question
    DNSQuestion extraQuestion;       // additional storage
    mDNSBool    questionActive;      // if true, StopQuery() can be called on the question field
    mDNSBool    findUpdatePort;
    mDNSBool    findLLQPort;
    mDNSIPPort  updatePort;
    mDNSIPPort  llqPort;
    AsyncOpCallback *callback;       // caller specified function to be called upon completion
    void        *callbackInfo;
    } ntaContext;


// function prototypes (for routines that must be used as fn pointers prior to their definitions,
// and allows states to be read top-to-bottom in logical order)
mDNSlocal void getZoneData(mDNS *const m, DNSMessage *msg, const mDNSu8 *end, DNSQuestion *question, void *contextPtr);
mDNSlocal smAction hndlLookupSOA(DNSMessage *msg, const mDNSu8 *end, ntaContext *context);
mDNSlocal void processSOA(ntaContext *context, ResourceRecord *rr);
mDNSlocal smAction confirmNS(DNSMessage *msg, const mDNSu8 *end, ntaContext *context);
mDNSlocal smAction lookupNSAddr(DNSMessage *msg, const mDNSu8 *end, ntaContext *context);
mDNSlocal smAction hndlLookupPorts(DNSMessage *msg, const mDNSu8 *end, ntaContext *context);

// initialization
mDNSlocal mStatus startGetZoneData(domainname *name, mDNS *m, mDNSBool findUpdatePort, mDNSBool findLLQPort,
								   AsyncOpCallback callback, void *callbackInfo)
    {
    ntaContext *context = (ntaContext*)umalloc(sizeof(ntaContext));
    if (!context) { LogMsg("ERROR: startGetZoneData - umalloc failed");  return mStatus_NoMemoryErr; }
	ubzero(context, sizeof(ntaContext));
    AssignDomainName(&context->origName, name);
    context->state = init;
    context->m = m;
	context->callback = callback;
	context->callbackInfo = callbackInfo;
	context->findUpdatePort = findUpdatePort;
	context->findLLQPort = findLLQPort;
    getZoneData(m, mDNSNULL, mDNSNULL, mDNSNULL, context);
    return mStatus_NoError;
    }

// state machine entry routine
mDNSlocal void getZoneData(mDNS *const m, DNSMessage *msg, const mDNSu8 *end, DNSQuestion *question, void *contextPtr)
    {
	AsyncOpResult result;
	ntaContext *context = (ntaContext*)contextPtr;
	smAction action;

    // unused
	(void)m;
	(void)question;
	
	// stop any active question
	if (context->questionActive)
		{
		uDNS_StopQuery(context->m, &context->question);
		context->questionActive = mDNSfalse;
		}

	if (msg && msg->h.flags.b[2] >> 4 && msg->h.flags.b[2] >> 4 != kDNSFlag1_RC_NXDomain)
		{
		// rcode non-zero, non-nxdomain
		LogMsg("ERROR: getZoneData - received response w/ rcode %d", msg->h.flags.b[2] >> 4);
		goto error;
		}
 	
	switch (context->state)
        {
        case init:
        case lookupSOA:
            action = hndlLookupSOA(msg, end, context);
			if (action == smError) goto error;
			if (action == smBreak) return;
		case foundZone:
		case lookupNS:
			action = confirmNS(msg, end, context);
			if (action == smError) goto error;
			if (action == smBreak) return;
		case foundNS:
		case lookupA:
			action = lookupNSAddr(msg, end, context);
			if (action == smError) goto error;
			if (action == smBreak) return;
		case foundA:
			if (!context->findUpdatePort && !context->findLLQPort)
				{
				context->state = complete;
				break;
				}
		case lookupPort:
			action = hndlLookupPorts(msg, end, context);
			if (action == smError) goto error;
			if (action == smBreak) return;
			if (action == smContinue) context->state = complete;
		case foundPort:
		case complete: break;
		}
					  
	if (context->state != complete)
		{
		LogMsg("ERROR: getZoneData - exited state machine with state %d", context->state);
		goto error;
		}
	
	result.type = zoneDataResult;
	result.zoneData.primaryAddr.ip.v4 = context->addr;
	result.zoneData.primaryAddr.type = mDNSAddrType_IPv4;
	AssignDomainName(&result.zoneData.zoneName, &context->zone);
	result.zoneData.zoneClass = context->zoneClass;
	result.zoneData.llqPort    = context->findLLQPort    ? context->llqPort    : zeroIPPort;
	result.zoneData.updatePort = context->findUpdatePort ? context->updatePort : zeroIPPort;
	context->callback(mStatus_NoError, context->m, context->callbackInfo, &result);
	goto cleanup;
			
error:
	if (context && context->callback)
		context->callback(mStatus_UnknownErr, context->m, context->callbackInfo, mDNSNULL);
cleanup:
	if (context && context->questionActive)
		{
		uDNS_StopQuery(context->m, &context->question);
		context->questionActive = mDNSfalse;
		}
    if (context) ufree(context);
	}

mDNSlocal smAction hndlLookupSOA(DNSMessage *msg, const mDNSu8 *end, ntaContext *context)
    {
    mStatus err;
    LargeCacheRecord lcr;
	ResourceRecord *rr = &lcr.r.resrec;
	DNSQuestion *query = &context->question;
	const mDNSu8 *ptr;
	
    if (msg)
        {
        // if msg contains SOA record in answer or authority sections, update context/state and return
		int i;
		ptr = LocateAnswers(msg, end);
		for (i = 0; i < msg->h.numAnswers; i++)
			{
			ptr = GetLargeResourceRecord(context->m, msg, ptr, end, 0, kDNSRecordTypePacketAns, &lcr);
			if (!ptr) { LogMsg("ERROR: hndlLookupSOA, Answers - GetLargeResourceRecord returned NULL");  return smError; }
			if (rr->rrtype == kDNSType_SOA && SameDomainName(context->curSOA, rr->name))
				{
				processSOA(context, rr);
				return smContinue;
				}
			}
		ptr = LocateAuthorities(msg, end);
		// SOA not in answers, check in authority
		for (i = 0; i < msg->h.numAuthorities; i++)
			{
			ptr = GetLargeResourceRecord(context->m, msg, ptr, end, 0, kDNSRecordTypePacketAns, &lcr); ///!!!KRS using type PacketAns for auth
			if (!ptr) { LogMsg("ERROR: hndlLookupSOA, Authority - GetLargeResourceRecord returned NULL");  return smError; }
			if (rr->rrtype == kDNSType_SOA)
				{
				processSOA(context, rr);
				return smContinue;
				}
			}
		}

    if (context->state != init && !context->curSOA->c[0])
        {
        // we've gone down to the root and have not found an SOA
        LogMsg("ERROR: hndlLookupSOA - recursed to root label of %##s without finding SOA",
                context->origName.c);
		return smError;
        }

    ubzero(query, sizeof(DNSQuestion));
    // chop off leading label unless this is our first try
    if (context->state == init)  context->curSOA = &context->origName;
    else                         context->curSOA = (domainname *)(context->curSOA->c + context->curSOA->c[0]+1);
    
    context->state = lookupSOA;
    AssignDomainName(&query->qname, context->curSOA);
    query->qtype = kDNSType_SOA;
    query->qclass = kDNSClass_IN;
    err = startInternalQuery(query, context->m, getZoneData, context);
	context->questionActive = mDNStrue;
	if (err) LogMsg("hndlLookupSOA: startInternalQuery returned error %ld (breaking until next periodic retransmission)", err);

    return smBreak;     // break from state machine until we receive another packet
    }

mDNSlocal void processSOA(ntaContext *context, ResourceRecord *rr)
	{
	AssignDomainName(&context->zone, rr->name);
	context->zoneClass = rr->rrclass;
	AssignDomainName(&context->ns, &rr->rdata->u.soa.mname);
	context->state = foundZone;
	}


mDNSlocal smAction confirmNS(DNSMessage *msg, const mDNSu8 *end, ntaContext *context)
	{
	DNSQuestion *query = &context->question;
	mStatus err;
	LargeCacheRecord lcr;
	const ResourceRecord *const rr = &lcr.r.resrec;
	const mDNSu8 *ptr;
	int i;
		
	if (context->state == foundZone)
		{
		// we've just learned the zone.  confirm that an NS record exists
		AssignDomainName(&query->qname, &context->zone);
		query->qtype = kDNSType_NS;
		query->qclass = kDNSClass_IN;
		err = startInternalQuery(query, context->m, getZoneData, context);
		context->questionActive = mDNStrue;
		if (err) LogMsg("confirmNS: startInternalQuery returned error %ld (breaking until next periodic retransmission", err);
		context->state = lookupNS;
		return smBreak;  // break from SM until we receive another packet
		}
	else if (context->state == lookupNS)
		{
		ptr = LocateAnswers(msg, end);
		for (i = 0; i < msg->h.numAnswers; i++)
			{
			ptr = GetLargeResourceRecord(context->m, msg, ptr, end, 0, kDNSRecordTypePacketAns, &lcr);
			if (!ptr) { LogMsg("ERROR: confirmNS, Answers - GetLargeResourceRecord returned NULL");  return smError; }
			if (rr->rrtype == kDNSType_NS &&
				SameDomainName(&context->zone, rr->name) && SameDomainName(&context->ns, &rr->rdata->u.name))
				{
				context->state = foundNS;
				return smContinue;  // next routine will examine additionals section of A record
				}
			}
		debugf("ERROR: could not confirm existence of record %##s NS %##s", context->zone.c, context->ns.c);
		return smError;
		}
	else { LogMsg("ERROR: confirmNS - bad state %d", context->state); return smError; }
	}

mDNSlocal smAction queryNSAddr(ntaContext *context)
	{
	mStatus err;
	DNSQuestion *query = &context->question;
	
	AssignDomainName(&query->qname, &context->ns);
	query->qtype = kDNSType_A;
	query->qclass = kDNSClass_IN;
	err = startInternalQuery(query, context->m, getZoneData, context);
	context->questionActive = mDNStrue;
	if (err) LogMsg("confirmNS: startInternalQuery returned error %ld (breaking until next periodic retransmission)", err);
	context->state = lookupA;
	return smBreak;
	}

mDNSlocal smAction lookupNSAddr(DNSMessage *msg, const mDNSu8 *end, ntaContext *context)
	{
	const mDNSu8 *ptr;
	int i;
	LargeCacheRecord lcr;
	ResourceRecord *rr = &lcr.r.resrec;
	
	if (context->state == foundNS)
		{
		// we just found the NS record - look for the corresponding A record in the Additionals section
		if (!msg->h.numAdditionals) return queryNSAddr(context);
		ptr = LocateAdditionals(msg, end);
		if (!ptr)
			{
			LogMsg("ERROR: lookupNSAddr - LocateAdditionals returned NULL, expected %d additionals", msg->h.numAdditionals);
			return queryNSAddr(context);
			}
		else
			{
			for (i = 0; i < msg->h.numAdditionals; i++)
				{
				ptr = GetLargeResourceRecord(context->m, msg, ptr, end, 0, kDNSRecordTypePacketAns, &lcr);
				if (!ptr)
					{
					LogMsg("ERROR: lookupNSAddr, Additionals - GetLargeResourceRecord returned NULL");
					return queryNSAddr(context);
					}
				if (rr->rrtype == kDNSType_A && SameDomainName(&context->ns, rr->name))
					{
					context->addr = rr->rdata->u.ipv4;
					context->state = foundA;
					return smContinue;
					}
				}
			}
		// no A record in Additionals - query the server
		return queryNSAddr(context);
		}
	else if (context->state == lookupA)
		{
		ptr = LocateAnswers(msg, end);
		if (!ptr) { LogMsg("ERROR: lookupNSAddr: LocateAnswers returned NULL");  return smError; }
		for (i = 0; i < msg->h.numAnswers; i++)
			{
			ptr = GetLargeResourceRecord(context->m, msg, ptr, end, 0, kDNSRecordTypePacketAns, &lcr);
			if (!ptr) { LogMsg("ERROR: lookupNSAddr, Answers - GetLargeResourceRecord returned NULL"); break; }
			if (rr->rrtype == kDNSType_A && SameDomainName(&context->ns, rr->name))
				{
				context->addr = rr->rdata->u.ipv4;
				context->state = foundA;
				return smContinue;
				}
			}
		LogMsg("ERROR: lookupNSAddr: Address record not found in answer section");
		return smError;
		}
	else { LogMsg("ERROR: lookupNSAddr - bad state %d", context->state); return smError; }
	}
	
mDNSlocal smAction lookupDNSPort(DNSMessage *msg, const mDNSu8 *end, ntaContext *context, char *portName, mDNSIPPort *port)
	{
	int i;
	LargeCacheRecord lcr;
	const mDNSu8 *ptr;
	DNSQuestion *q;
	mStatus err;
	
	if (context->state == lookupPort)  // we've already issued the query
		{
		if (!msg) { LogMsg("ERROR: hndlLookupUpdatePort - NULL message"); return smError; }
		ptr = LocateAnswers(msg, end);
		for (i = 0; i < msg->h.numAnswers; i++)
			{
			ptr = GetLargeResourceRecord(context->m, msg, ptr, end, 0, kDNSRecordTypePacketAns, &lcr);
			if (!ptr) { LogMsg("ERROR: hndlLookupUpdatePort - GetLargeResourceRecord returned NULL");  return smError; }
			if (ResourceRecordAnswersQuestion(&lcr.r.resrec, &context->question))
				{
				*port = lcr.r.resrec.rdata->u.srv.port;
				context->state = foundPort;
				return smContinue;
				}
			}
		debugf("hndlLookupUpdatePort - no answer for type %s", portName);
		port->NotAnInteger = 0;
		context->state = foundPort;
		return smContinue;
		}

	// query the server for the update port for the zone
	context->state = lookupPort;
	q = &context->question;
	MakeDomainNameFromDNSNameString(&q->qname, portName);
	AppendDomainName(&q->qname, &context->zone);
    q->qtype = kDNSType_SRV;
    q->qclass = kDNSClass_IN;
    err = startInternalQuery(q, context->m, getZoneData, context);
	context->questionActive = mDNStrue;
    if (err) LogMsg("hndlLookupSOA: startInternalQuery returned error %ld (breaking until next periodic retransmission)", err);
    return smBreak;     // break from state machine until we receive another packet
	}

mDNSlocal smAction hndlLookupPorts(DNSMessage *msg, const mDNSu8 *end, ntaContext *context)
	{
	smAction action;
	
	if (context->findUpdatePort && !context->updatePort.NotAnInteger)
		{
		action = lookupDNSPort(msg, end, context, UPDATE_PORT_NAME, &context->updatePort);
		if (action != smContinue) return action;
		}
	if (context->findLLQPort && !context->llqPort.NotAnInteger)
		return lookupDNSPort(msg, end, context, LLQ_PORT_NAME, &context->llqPort);

	return smContinue;
	}


// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Truncation Handling
#endif

typedef struct
	{
    DNSQuestion  *question;
    DNSMessage *reply;
    mDNSu16  replylen;
    int nread;
    mDNS *m;
	} tcpInfo_t;

// issue queries over a conected socket
mDNSlocal void conQueryCallback(int sd, void *context, mDNSBool ConnectionEstablished)
	{
	mStatus err = 0;
	char msgbuf[356];  // 96 (hdr) + 256 (domain) + 4 (class/type)
	DNSMessage *msg;
	mDNSu8 *end;
	tcpInfo_t *info = (tcpInfo_t *)context;
	DNSQuestion *question = info->question;
	int n;
	mDNS *m = info->m;

	mDNS_Lock(m);
	
	if (ConnectionEstablished)
		{
		// connection is established - send the message
		msg = (DNSMessage *)&msgbuf;
		err = constructQueryMsg(msg, &end, question);
		if (err) { LogMsg("ERROR: conQueryCallback: constructQueryMsg - %ld", err);  goto error; }
		err = mDNSSendDNSMessage(m, msg, end, mDNSInterface_Any, &zeroAddr, zeroIPPort, sd, mDNSNULL);
		question->LastQTime = mDNSPlatformTimeNow(m);
		if (err) { debugf("ERROR: conQueryCallback: mDNSSendDNSMessage_tcp - %ld", err);  goto error; }
		}
	else
		{
		if (!info->nread)
			{
			// read msg len
			mDNSu8 lenbuf[2];
			n = mDNSPlatformReadTCP(sd, lenbuf, 2);
			if (n != 2)
				{
				LogMsg("ERROR:conQueryCallback - attempt to read message length failed (read returned %d)", n);
				goto error;
				}
			info->replylen = (mDNSu16)((mDNSu16)lenbuf[0] << 8 | lenbuf[1]);
			if (info->replylen < sizeof(DNSMessageHeader))
				{ LogMsg("ERROR: conQueryCallback - length too short (%d bytes)", info->replylen);  goto error; }
			info->reply = umalloc(info->replylen);
			if (!info->reply) { LogMsg("ERROR: conQueryCallback - malloc failed"); goto error; }
			}
		n = mDNSPlatformReadTCP(sd, ((char *)info->reply) + info->nread, info->replylen - info->nread);
		if (n < 0) { LogMsg("ERROR: conQueryCallback - read returned %d", n); goto error; }
		info->nread += n;
		if (info->nread == info->replylen)
			{
			// Finished reading message; convert the integer parts which are in IETF byte-order (MSB first, LSB second)
			DNSMessage *msg = info->reply;
			mDNSu8 *ptr = (mDNSu8 *)&msg->h.numQuestions;
			msg->h.numQuestions   = (mDNSu16)((mDNSu16)ptr[0] << 8 | ptr[1]);
			msg->h.numAnswers     = (mDNSu16)((mDNSu16)ptr[2] << 8 | ptr[3]);
			msg->h.numAuthorities = (mDNSu16)((mDNSu16)ptr[4] << 8 | ptr[5]);
			msg->h.numAdditionals = (mDNSu16)((mDNSu16)ptr[6] << 8 | ptr[7]);
			uDNS_ReceiveMsg(m, msg, (mDNSu8 *)msg + info->replylen, mDNSNULL, zeroIPPort, mDNSNULL, zeroIPPort, question->InterfaceID);
			mDNSPlatformTCPCloseConnection(sd);
			ufree(info->reply);
			ufree(info);
			}
		}

	mDNS_Unlock(m);
	return;

	error:
	mDNSPlatformTCPCloseConnection(sd);
	if (info->reply) ufree(info->reply);
	ufree(info);
	mDNS_Unlock(m);
	}

mDNSlocal void hndlTruncatedAnswer(DNSQuestion *question, const  mDNSAddr *src, mDNS *m)
	{
	mStatus connectionStatus;
	uDNS_QuestionInfo *info = &question->uDNS_info;
	int sd;
	tcpInfo_t *context;
	
	if (!src) { LogMsg("hndlTruncatedAnswer: TCP DNS response had TC bit set: ignoring"); return; }

	context = (tcpInfo_t *)umalloc(sizeof(tcpInfo_t));
	if (!context) { LogMsg("ERROR: hndlTruncatedAnswer - memallocate failed"); return; }
	ubzero(context, sizeof(tcpInfo_t));
	context->question = question;
	context->m = m;
	info->id = newMessageID(&m->uDNS_info);

	connectionStatus = mDNSPlatformTCPConnect(src, UnicastDNSPort, question->InterfaceID, conQueryCallback, context, &sd);
	if (connectionStatus == mStatus_ConnEstablished)  // manually invoke callback if connection completes
		{
		conQueryCallback(sd, context, mDNStrue);
		return;
		}
	if (connectionStatus == mStatus_ConnPending) return; // callback will be automatically invoked when connection completes
	LogMsg("hndlTruncatedAnswer: connection failed");
	uDNS_StopQuery(m, question);  //!!!KRS can we really call this here?
	}


// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Dynamic Updates
#endif

mDNSlocal void sendRecordRegistration(mDNS *const m, AuthRecord *rr)
	{
	DNSMessage msg;
	mDNSu8 *ptr = msg.data;
	mDNSu8 *end = (mDNSu8 *)&msg + sizeof(DNSMessage);
	uDNS_GlobalInfo *u = &m->uDNS_info;
	mDNSOpaque16 id;
	uDNS_RegInfo *regInfo = &rr->uDNS_info;
	mStatus err = mStatus_UnknownErr;

	id = newMessageID(u);
	InitializeDNSMessage(&msg.h, id, UpdateReqFlags);
	rr->uDNS_info.id = id;
	
    // set zone
	ptr = putZone(&msg, ptr, end, &regInfo->zone, mDNSOpaque16fromIntVal(rr->resrec.rrclass));
	if (!ptr) goto error;

	if (regInfo->state == regState_UpdatePending)
		{
		// delete old RData
		SetNewRData(&rr->resrec, regInfo->OrigRData, regInfo->OrigRDLen);
		if (!(ptr = putDeletionRecord(&msg, ptr, &rr->resrec))) goto error;  // delete old rdata
		
		// add new RData
		SetNewRData(&rr->resrec, regInfo->InFlightRData, regInfo->InFlightRDLen);
		if (!(ptr = PutResourceRecordTTLJumbo(&msg, ptr, &msg.h.mDNS_numUpdates, &rr->resrec, rr->resrec.rroriginalttl))) goto error;
		}

	else
		{
		if (rr->resrec.RecordType == kDNSRecordTypeKnownUnique)
			{
			// KnownUnique: Delete any previous value
			ptr = putDeleteRRSet(&msg, ptr, rr->resrec.name, rr->resrec.rrtype);
			if (!ptr) goto error;
			}
		
		else if (rr->resrec.RecordType != kDNSRecordTypeShared)
			{
			ptr = putPrereqNameNotInUse(rr->resrec.name, &msg, ptr, end);
			if (!ptr) goto error;
			}
		
		ptr = PutResourceRecordTTLJumbo(&msg, ptr, &msg.h.mDNS_numUpdates, &rr->resrec, rr->resrec.rroriginalttl);
		if (!ptr) goto error;
		}
	
	if (rr->uDNS_info.lease)
		{ ptr = putUpdateLease(&msg, ptr, DEFAULT_UPDATE_LEASE); if (!ptr) goto error; }

	err = mDNSSendDNSMessage(m, &msg, ptr, mDNSInterface_Any, &regInfo->ns, regInfo->port, -1, GetAuthInfoForName(u, rr->resrec.name));
	if (err) debugf("ERROR: sendRecordRegistration - mDNSSendDNSMessage - %ld", err);
   
	SetRecordRetry(m, rr, err);
	
	if (regInfo->state != regState_Refresh && regInfo->state != regState_DeregDeferred && regInfo->state != regState_UpdatePending)
		regInfo->state = regState_Pending;

	return;

error:
	LogMsg("sendRecordRegistration: Error formatting message");
	if (rr->uDNS_info.state != regState_Unregistered)
		{
		unlinkAR(&u->RecordRegistrations, rr);
		rr->uDNS_info.state = regState_Unregistered;
		}
	m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
	if (rr->RecordCallback) rr->RecordCallback(m, rr, err);
	m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
	// NOTE: not safe to touch any client structures here
	}

mDNSlocal void RecordRegistrationCallback(mStatus err, mDNS *const m, void *authPtr, const AsyncOpResult *result)
	{
	AuthRecord *newRR = (AuthRecord*)authPtr;
	const zoneData_t *zoneData = mDNSNULL;
	uDNS_GlobalInfo *u = &m->uDNS_info;
	AuthRecord *ptr;

	// make sure record is still in list
	for (ptr = u->RecordRegistrations; ptr; ptr = ptr->next)
		if (ptr == newRR) break;
	if (!ptr) { LogMsg("RecordRegistrationCallback - RR no longer in list.  Discarding."); return; }

	// check error/result
	if (err) { LogMsg("RecordRegistrationCallback: error %ld", err); goto error; }
	if (!result) { LogMsg("ERROR: RecordRegistrationCallback invoked with NULL result and no error"); goto error;  }
	else zoneData = &result->zoneData;

	if (newRR->uDNS_info.state == regState_Cancelled)
		{
		//!!!KRS we should send a memfree callback here!
		debugf("Registration of %##s type %d cancelled prior to update",
			   newRR->resrec.name->c, newRR->resrec.rrtype);
		newRR->uDNS_info.state = regState_Unregistered;
		unlinkAR(&u->RecordRegistrations, newRR);
		return;
		}
	
	if (result->type != zoneDataResult)
		{
		LogMsg("ERROR: buildUpdatePacket passed incorrect result type %d", result->type);
		goto error;
		}

	if (newRR->resrec.rrclass != zoneData->zoneClass)
		{
		LogMsg("ERROR: New resource record's class (%d) does not match zone class (%d)",
			   newRR->resrec.rrclass, zoneData->zoneClass);
		goto error;
		}
	
	// Don't try to do updates to the root name server.
	// We might be tempted also to block updates to any single-label name server (e.g. com, edu, net, etc.) but some
	// organizations use their own private pseudo-TLD, like ".home", etc, and we don't want to block that.
	if (zoneData->zoneName.c[0] == 0)
		{
		LogMsg("ERROR: Only name server claiming responsibility for \"%##s\" is \"%##s\"!",
			newRR->resrec.name->c, zoneData->zoneName.c);
		err = mStatus_NoSuchNameErr;
		goto error;
		}

	// cache zone data
	AssignDomainName(&newRR->uDNS_info.zone, &zoneData->zoneName);
	newRR->uDNS_info.ns = zoneData->primaryAddr;
	if (zoneData->updatePort.NotAnInteger) newRR->uDNS_info.port = zoneData->updatePort;
	else
		{
		debugf("Update port not advertised via SRV - guessing port 53, no lease option");
		newRR->uDNS_info.port = UnicastDNSPort;
		newRR->uDNS_info.lease = mDNSfalse;
		}

	sendRecordRegistration(m, newRR);
	return;
		
error:
	if (newRR->uDNS_info.state != regState_Unregistered)
		{
		unlinkAR(&u->RecordRegistrations, newRR);
		newRR->uDNS_info.state = regState_Unregistered;
		}
	m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
	if (newRR->RecordCallback)
		newRR->RecordCallback(m, newRR, err);
	m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
	// NOTE: not safe to touch any client structures here
	}

mDNSlocal void SendServiceRegistration(mDNS *m, ServiceRecordSet *srs)
	{
	DNSMessage msg;
	mDNSu8 *ptr = msg.data;
	mDNSu8 *end = (mDNSu8 *)&msg + sizeof(DNSMessage);
	uDNS_GlobalInfo *u = &m->uDNS_info;
	mDNSOpaque16 id;
	uDNS_RegInfo *rInfo = &srs->uDNS_info;
	mStatus err = mStatus_UnknownErr;
	mDNSIPPort privport;
	NATTraversalInfo *nat = srs->uDNS_info.NATinfo;
	mDNSBool mapped = mDNSfalse;
	domainname target;
	AuthRecord *srv = &srs->RR_SRV;
	mDNSu32 i;
	
	privport = zeroIPPort;
	
	if (!rInfo->ns.ip.v4.NotAnInteger) { LogMsg("SendServiceRegistration - NS not set!"); return; }

	id = newMessageID(u);
	InitializeDNSMessage(&msg.h, id, UpdateReqFlags);

	// setup resource records
	SetNewRData(&srs->RR_PTR.resrec, mDNSNULL, 0);
	SetNewRData(&srs->RR_TXT.resrec, mDNSNULL, 0);
	
	// replace port w/ NAT mapping if necessary
 	if (nat && nat->PublicPort.NotAnInteger &&
		(nat->state == NATState_Established || nat->state == NATState_Refresh || nat->state == NATState_Legacy))
		{
		privport = srv->resrec.rdata->u.srv.port;
		srv->resrec.rdata->u.srv.port = nat->PublicPort;
		mapped = mDNStrue;
		}
	
	// construct update packet
    // set zone
	ptr = putZone(&msg, ptr, end, &rInfo->zone, mDNSOpaque16fromIntVal(srv->resrec.rrclass));
	if (!ptr) goto error;
	
	if (srs->uDNS_info.TestForSelfConflict)
		{
		// update w/ prereq that SRV already exist to make sure previous registration was ours, and delete any stale TXT records
		if (!(ptr = PutResourceRecordTTLJumbo(&msg, ptr, &msg.h.mDNS_numPrereqs, &srs->RR_SRV.resrec, 0))) goto error;
		if (!(ptr = putDeleteRRSet(&msg, ptr, srs->RR_TXT.resrec.name, srs->RR_TXT.resrec.rrtype)))       goto error;
		}
	
	else if (srs->uDNS_info.state != regState_Refresh && srs->uDNS_info.state != regState_UpdatePending)
		{
		// use SRV name for prereq
		ptr = putPrereqNameNotInUse(srv->resrec.name, &msg, ptr, end);
		if (!ptr) goto error;
		}
	
	//!!!KRS  Need to do bounds checking and use TCP if it won't fit!!!
	if (!(ptr = PutResourceRecordTTLJumbo(&msg, ptr, &msg.h.mDNS_numUpdates, &srs->RR_PTR.resrec, srs->RR_PTR.resrec.rroriginalttl))) goto error;

	for (i = 0; i < srs->NumSubTypes; i++)
		if (!(ptr = PutResourceRecordTTLJumbo(&msg, ptr, &msg.h.mDNS_numUpdates, &srs->SubTypes[i].resrec, srs->SubTypes[i].resrec.rroriginalttl))) goto error;
	
	if (rInfo->state == regState_UpdatePending) // we're updating the txt record
		{
		AuthRecord *txt = &srs->RR_TXT;
		uDNS_RegInfo *txtInfo = &txt->uDNS_info;
		// delete old RData
		SetNewRData(&txt->resrec, txtInfo->OrigRData, txtInfo->OrigRDLen);
		if (!(ptr = putDeletionRecord(&msg, ptr, &srs->RR_TXT.resrec))) goto error;  // delete old rdata

		// add new RData
		SetNewRData(&txt->resrec, txtInfo->InFlightRData, txtInfo->InFlightRDLen);
		if (!(ptr = PutResourceRecordTTLJumbo(&msg, ptr, &msg.h.mDNS_numUpdates, &srs->RR_TXT.resrec, srs->RR_TXT.resrec.rroriginalttl))) goto error;
		}
	else
		if (!(ptr = PutResourceRecordTTLJumbo(&msg, ptr, &msg.h.mDNS_numUpdates, &srs->RR_TXT.resrec, srs->RR_TXT.resrec.rroriginalttl))) goto error;

	 if (!GetServiceTarget(u, srv, &target))
		{
		debugf("Couldn't get target for service %##s", srv->resrec.name->c);
		rInfo->state = regState_NoTarget;
		return;
		}

	 if (!SameDomainName(&target, &srv->resrec.rdata->u.srv.target))
		 {
		 AssignDomainName(&srv->resrec.rdata->u.srv.target, &target);
		 SetNewRData(&srv->resrec, mDNSNULL, 0);
		 }

	ptr = PutResourceRecordTTLJumbo(&msg, ptr, &msg.h.mDNS_numUpdates, &srv->resrec, srv->resrec.rroriginalttl);
	if (!ptr) goto error;

	if (srs->uDNS_info.lease)
		{ ptr = putUpdateLease(&msg, ptr, DEFAULT_UPDATE_LEASE); if (!ptr) goto error; }
	   
	err = mDNSSendDNSMessage(m, &msg, ptr, mDNSInterface_Any, &rInfo->ns, rInfo->port, -1, GetAuthInfoForName(u, srs->RR_SRV.resrec.name));
	if (err) debugf("ERROR: SendServiceRegistration - mDNSSendDNSMessage - %ld", err);

	if (rInfo->state != regState_Refresh && rInfo->state != regState_DeregDeferred && srs->uDNS_info.state != regState_UpdatePending)
		rInfo->state = regState_Pending;

	SetRecordRetry(m, &srs->RR_SRV, err);
	rInfo->id = id;
	if (mapped) srv->resrec.rdata->u.srv.port = privport;
	return;

error:
	LogMsg("SendServiceRegistration - Error formatting message");
	if (mapped) srv->resrec.rdata->u.srv.port = privport;
	unlinkSRS(m, srs);
	rInfo->state = regState_Unregistered;
	m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
	srs->ServiceCallback(m, srs, err);
	m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
	//!!!KRS will mem still be free'd on error?
	// NOTE: not safe to touch any client structures here
	}

mDNSlocal void serviceRegistrationCallback(mStatus err, mDNS *const m, void *srsPtr, const AsyncOpResult *result)
	{
	ServiceRecordSet *srs = (ServiceRecordSet *)srsPtr;
	const zoneData_t *zoneData = mDNSNULL;
	
	if (err) goto error;
	if (!result) { LogMsg("ERROR: serviceRegistrationCallback invoked with NULL result and no error");  goto error; }
	else zoneData = &result->zoneData;
	
	if (result->type != zoneDataResult)
		{
		LogMsg("ERROR: buildUpdatePacket passed incorrect result type %d", result->type);
		goto error;
		}

	if (srs->uDNS_info.state == regState_Cancelled)
		{
		// client cancelled registration while fetching zone data
		srs->uDNS_info.state = regState_Unregistered;
		unlinkSRS(m, srs);
		m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
		srs->ServiceCallback(m, srs, mStatus_MemFree);
		m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
		return;
		}
	
	if (srs->RR_SRV.resrec.rrclass != zoneData->zoneClass)
		{
		LogMsg("Service %##s - class does not match zone", srs->RR_SRV.resrec.name->c);
		goto error;
		}

	// cache zone data
	AssignDomainName(&srs->uDNS_info.zone, &zoneData->zoneName);
    srs->uDNS_info.ns.type = mDNSAddrType_IPv4;
	srs->uDNS_info.ns = zoneData->primaryAddr;
	if (zoneData->updatePort.NotAnInteger) srs->uDNS_info.port = zoneData->updatePort;
	else
		{
		debugf("Update port not advertised via SRV - guessing port 53, no lease option");
		srs->uDNS_info.port = UnicastDNSPort;
		srs->uDNS_info.lease = mDNSfalse;
		}

	if (srs->RR_SRV.resrec.rdata->u.srv.port.NotAnInteger && IsPrivateV4Addr(&m->uDNS_info.AdvertisedV4))
		{ srs->uDNS_info.state = regState_NATMap; StartNATPortMap(m, srs); }
	else SendServiceRegistration(m, srs);
	return;
		
error:
	unlinkSRS(m, srs);
	srs->uDNS_info.state = regState_Unregistered;
	m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
	srs->ServiceCallback(m, srs, err);
	m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
	// NOTE: not safe to touch any client structures here
	}

mDNSlocal mStatus SetupRecordRegistration(mDNS *m, AuthRecord *rr)
	{
	domainname *target = GetRRDomainNameTarget(&rr->resrec);
	AuthRecord *ptr = m->uDNS_info.RecordRegistrations;

	while (ptr && ptr != rr) ptr = ptr->next;
	if (ptr) { LogMsg("Error: SetupRecordRegistration - record %##s already in list!", rr->resrec.name->c); return mStatus_AlreadyRegistered; }
	
	if (rr->uDNS_info.state == regState_FetchingZoneData ||
		rr->uDNS_info.state == regState_Pending ||
		rr->uDNS_info.state ==  regState_Registered)
		{
		LogMsg("Requested double-registration of physical record %##s type %d",
			   rr->resrec.name->c, rr->resrec.rrtype);
		return mStatus_AlreadyRegistered;
		}
	
	rr->resrec.rdlength   = GetRDLength(&rr->resrec, mDNSfalse);
	rr->resrec.rdestimate = GetRDLength(&rr->resrec, mDNStrue);

	if (!ValidateDomainName(rr->resrec.name))
		{
		LogMsg("Attempt to register record with invalid name: %s", ARDisplayString(m, rr));
		return mStatus_Invalid;
		}

	// Don't do this until *after* we've set rr->resrec.rdlength
	if (!ValidateRData(rr->resrec.rrtype, rr->resrec.rdlength, rr->resrec.rdata))
		{
		LogMsg("Attempt to register record with invalid rdata: %s", ARDisplayString(m, rr));
		return mStatus_Invalid;
		}

	rr->resrec.namehash   = DomainNameHashValue(rr->resrec.name);
	rr->resrec.rdatahash  = target ? DomainNameHashValue(target) : RDataHashValue(rr->resrec.rdlength, &rr->resrec.rdata->u);

	rr->uDNS_info.state = regState_FetchingZoneData;
	rr->next = m->uDNS_info.RecordRegistrations;
	m->uDNS_info.RecordRegistrations = rr;
	rr->uDNS_info.lease = mDNStrue;

	return mStatus_NoError;
	}

mDNSexport mStatus uDNS_RegisterRecord(mDNS *const m, AuthRecord *const rr)
	{
	mStatus err = SetupRecordRegistration(m, rr);
	if (err) return err;
	else return startGetZoneData(rr->resrec.name, m, mDNStrue, mDNSfalse, RecordRegistrationCallback, rr);
	}

mDNSlocal void SendRecordDeregistration(mDNS *m, AuthRecord *rr)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	DNSMessage msg;
	mDNSu8 *ptr = msg.data;
	mDNSu8 *end = (mDNSu8 *)&msg + sizeof(DNSMessage);
	mStatus err;
	
	InitializeDNSMessage(&msg.h, rr->uDNS_info.id, UpdateReqFlags);
	
	ptr = putZone(&msg, ptr, end, &rr->uDNS_info.zone, mDNSOpaque16fromIntVal(rr->resrec.rrclass));
	if (!ptr) goto error;
	if (!(ptr = putDeletionRecord(&msg, ptr, &rr->resrec))) goto error;

	err = mDNSSendDNSMessage(m, &msg, ptr, mDNSInterface_Any, &rr->uDNS_info.ns, rr->uDNS_info.port, -1, GetAuthInfoForName(u, rr->resrec.name));
	if (err) debugf("ERROR: SendRecordDeregistration - mDNSSendDNSMessage - %ld", err);

	SetRecordRetry(m, rr, err);
	rr->uDNS_info.state = regState_DeregPending;
	return;

	error:
	LogMsg("Error: SendRecordDeregistration - could not contruct deregistration packet");
	unlinkAR(&u->RecordRegistrations, rr);
	rr->uDNS_info.state = regState_Unregistered;
	}



mDNSexport mStatus uDNS_DeregisterRecord(mDNS *const m, AuthRecord *const rr)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	NATTraversalInfo *n = rr->uDNS_info.NATinfo;
 
	switch (rr->uDNS_info.state)
		{
		case regState_NATMap:
            // we're in the middle of a NAT traversal operation
            rr->uDNS_info.NATinfo = mDNSNULL;
			if (!n) LogMsg("uDNS_DeregisterRecord: no NAT info context");
			else FreeNATInfo(m, n); // cause response to outstanding request to be ignored.
			                        // Note: normally here we're trying to determine our public address,
			                        //in which case there is not state to be torn down.  For simplicity,
			                        //we allow other operations to expire.
			rr->uDNS_info.state = regState_Unregistered;
			break;
		case regState_ExtraQueued:
			rr->uDNS_info.state = regState_Unregistered;
			break;
		case regState_FetchingZoneData:
			rr->uDNS_info.state = regState_Cancelled;
			return mStatus_NoError;
		case regState_Refresh:
		case regState_Pending:
		case regState_UpdatePending:
			rr->uDNS_info.state = regState_DeregDeferred;
			LogMsg("Deferring deregistration of record %##s until registration completes", rr->resrec.name->c);
			return mStatus_NoError;
		case regState_Registered:
		case regState_DeregPending:
			break;
		case regState_DeregDeferred:
		case regState_Cancelled:
			LogMsg("Double deregistration of record %##s type %d",
				   rr->resrec.name->c, rr->resrec.rrtype);
			return mStatus_UnknownErr;
		case regState_Unregistered:
			LogMsg("Requested deregistration of unregistered record %##s type %d",
				   rr->resrec.name->c, rr->resrec.rrtype);
			return mStatus_UnknownErr;
		case regState_NATError:
		case  regState_NoTarget:
			LogMsg("ERROR: uDNS_DeregisterRecord called for record %##s with bad state %s", rr->resrec.name->c, rr->uDNS_info.state == regState_NoTarget ? "regState_NoTarget" : "regState_NATError");
			return mStatus_UnknownErr;
		}

	if (rr->uDNS_info.state == regState_Unregistered)
		{
		// unlink and deliver memfree
		
		unlinkAR(&u->RecordRegistrations, rr);
		m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
		if (rr->RecordCallback) rr->RecordCallback(m, rr, mStatus_MemFree);
		m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
		return mStatus_NoError;
		}

	rr->uDNS_info.NATinfo = mDNSNULL;
	if (n) FreeNATInfo(m, n);
	
	SendRecordDeregistration(m, rr);
	return mStatus_NoError;
	}
	
mDNSexport mStatus uDNS_RegisterService(mDNS *const m, ServiceRecordSet *srs)
	{
	mDNSu32 i;
	domainname target;
	uDNS_RegInfo *info = &srs->uDNS_info;
	ServiceRecordSet **p = &m->uDNS_info.ServiceRegistrations;
	while (*p && *p != srs) p=&(*p)->next;
	if (*p) { LogMsg("uDNS_RegisterService: %p %##s already in list", srs, srs->RR_SRV.resrec.name->c); return(mStatus_AlreadyRegistered); }
	ubzero(info, sizeof(*info));
	*p = srs;
	srs->next = mDNSNULL;

	srs->RR_SRV.resrec.rroriginalttl = kWideAreaTTL;
	srs->RR_TXT.resrec.rroriginalttl = kWideAreaTTL;
	srs->RR_PTR.resrec.rroriginalttl = kWideAreaTTL;
	for (i = 0; i < srs->NumSubTypes;i++) srs->SubTypes[i].resrec.rroriginalttl = kWideAreaTTL;
	
	info->lease = mDNStrue;

	srs->RR_SRV.resrec.rdata->u.srv.target.c[0] = 0;
	if (!GetServiceTarget(&m->uDNS_info, &srs->RR_SRV, &target))
		{
		// defer registration until we've got a target
		debugf("uDNS_RegisterService - no target for %##s", srs->RR_SRV.resrec.name->c);
		info->state = regState_NoTarget;
		return mStatus_NoError;
		}
	
	info->state = regState_FetchingZoneData;
	return startGetZoneData(srs->RR_SRV.resrec.name, m, mDNStrue, mDNSfalse, serviceRegistrationCallback, srs);
	}

mDNSlocal void SendServiceDeregistration(mDNS *m, ServiceRecordSet *srs)
	{
	uDNS_RegInfo *info = &srs->uDNS_info;
	uDNS_GlobalInfo *u = &m->uDNS_info;
	DNSMessage msg;
	mDNSOpaque16 id;
	mDNSu8 *ptr = msg.data;
	mDNSu8 *end = (mDNSu8 *)&msg + sizeof(DNSMessage);
	mStatus err = mStatus_UnknownErr;
	mDNSu32 i;
	
	id = newMessageID(u);
	InitializeDNSMessage(&msg.h, id, UpdateReqFlags);
	
    // put zone
	ptr = putZone(&msg, ptr, end, &info->zone, mDNSOpaque16fromIntVal(srs->RR_SRV.resrec.rrclass));
	if (!ptr) { LogMsg("ERROR: SendServiceDeregistration - putZone"); goto error; }
		
	if (!(ptr = putDeleteAllRRSets(&msg, ptr, srs->RR_SRV.resrec.name))) goto error;  // this deletes SRV, TXT, and Extras
	if (!(ptr = putDeletionRecord(&msg, ptr, &srs->RR_PTR.resrec))) goto error;
	for (i = 0; i < srs->NumSubTypes; i++)
		if (!(ptr = putDeletionRecord(&msg, ptr, &srs->SubTypes[i].resrec))) goto error;

	
	err = mDNSSendDNSMessage(m, &msg, ptr, mDNSInterface_Any, &info->ns, info->port, -1, GetAuthInfoForName(u, srs->RR_SRV.resrec.name));
	if (err && err != mStatus_TransientErr) { debugf("ERROR: SendServiceDeregistration - mDNSSendDNSMessage - %ld", err); goto error; }

	SetRecordRetry(m, &srs->RR_SRV, err);
    info->id = id;
	info->state = regState_DeregPending;
 
	return;
	
	error:
	unlinkSRS(m, srs);
	info->state = regState_Unregistered;
	}

mDNSexport mStatus uDNS_DeregisterService(mDNS *const m, ServiceRecordSet *srs)
	{
	NATTraversalInfo *nat = srs->uDNS_info.NATinfo;
	char *errmsg = "Unknown State";
	
	// don't re-register with a new target following deregistration
	srs->uDNS_info.SRVChanged = srs->uDNS_info.SRVUpdateDeferred = mDNSfalse;

	if (nat)
		{
		if (nat->state == NATState_Established || nat->state == NATState_Refresh || nat->state == NATState_Legacy)
			DeleteNATPortMapping(m, nat, srs);
		nat->reg.ServiceRegistration = mDNSNULL;
		srs->uDNS_info.NATinfo = mDNSNULL;
		FreeNATInfo(m, nat);
		}
	
	switch (srs->uDNS_info.state)
		{
		case regState_Unregistered:
			debugf("uDNS_DeregisterService - service %##s not registered", srs->RR_SRV.resrec.name->c);
			return mStatus_BadReferenceErr;
		case regState_FetchingZoneData:
			// let the async op complete, then terminate
			srs->uDNS_info.state = regState_Cancelled;
			return mStatus_NoError;  // deliver memfree upon completion of async op
		case regState_Pending:
		case regState_Refresh:
		case regState_UpdatePending:
			// deregister following completion of in-flight operation
			srs->uDNS_info.state = regState_DeregDeferred;
			return mStatus_NoError;
		case regState_DeregPending:
		case regState_DeregDeferred:
		case regState_Cancelled:
			debugf("Double deregistration of service %##s", srs->RR_SRV.resrec.name->c);
			return mStatus_NoError;
		case regState_NATError:  // not registered
		case regState_NATMap:    // not registered
		case regState_NoTarget:  // not registered
			unlinkSRS(m, srs);
			srs->uDNS_info.state = regState_Unregistered;
			m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
			srs->ServiceCallback(m, srs, mStatus_MemFree);
			m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
			return mStatus_NoError;
		case regState_Registered:
			srs->uDNS_info.state = regState_DeregPending;
			SendServiceDeregistration(m, srs);
			return mStatus_NoError;
		case regState_ExtraQueued: // only for record registrations
			errmsg = "bad state (regState_ExtraQueued)";
			goto error;
		}

	error:
	LogMsg("Error, uDNS_DeregisterService: %s", errmsg);
	return mStatus_BadReferenceErr;
	}

mDNSexport mStatus uDNS_AddRecordToService(mDNS *const m, ServiceRecordSet *sr, ExtraResourceRecord *extra)
	{
	mStatus err = mStatus_UnknownErr;
	
	extra->r.resrec.RecordType = kDNSRecordTypeShared;  // don't want it to conflict with the service name
	extra->r.RecordCallback = mDNSNULL;  // don't generate callbacks for extra RRs
	
	if (sr->uDNS_info.state == regState_Registered || sr->uDNS_info.state == regState_Refresh)
		err = uDNS_RegisterRecord(m, &extra->r);
	else
		{
		err = SetupRecordRegistration(m, &extra->r);
		extra->r.uDNS_info.state = regState_ExtraQueued; // %%% Is it okay to overwrite the previous uDNS_info.state?
		}
	
	if (!err)
		{
		extra->next = sr->Extras;
		sr->Extras = extra;
		}
	return err;
	}
  
mDNSexport mStatus uDNS_UpdateRecord(mDNS *m, AuthRecord *rr)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	ServiceRecordSet *parent = mDNSNULL;
	AuthRecord *rptr;
	uDNS_RegInfo *info = &rr->uDNS_info;
	regState_t *stateptr = mDNSNULL;
	
	// find the record in registered service list
	for (parent = u->ServiceRegistrations; parent; parent = parent->next)
		if (&parent->RR_TXT == rr) { stateptr = &parent->uDNS_info.state; break; }

	if (!parent)
		{
		// record not part of a service - check individual record registrations
		for (rptr = u->RecordRegistrations; rptr; rptr = rptr->next)
			if (rptr == rr) { stateptr = &rr->uDNS_info.state; break; }
		if (!rptr) goto unreg_error;
		}
	
	switch(*stateptr)
		{
		case regState_DeregPending:
		case regState_DeregDeferred:
		case regState_Cancelled:
		case regState_Unregistered:
			// not actively registered
			goto unreg_error;
			
		case regState_FetchingZoneData:
		case regState_NATMap:
		case regState_ExtraQueued:
		case regState_NoTarget:
			// change rdata directly since it hasn't been sent yet
			if (info->UpdateRDCallback) info->UpdateRDCallback(m, rr, rr->resrec.rdata);
			SetNewRData(&rr->resrec, rr->NewRData, rr->newrdlength);
			rr->NewRData = mDNSNULL;
			return mStatus_NoError;
			
		case regState_Pending:
		case regState_Refresh:
		case regState_UpdatePending:
			// registration in-flight.  queue rdata and return
			if (info->QueuedRData && info->UpdateRDCallback)
				// if unsent rdata is already queued, free it before we replace it
				info->UpdateRDCallback(m, rr, info->QueuedRData);
			info->QueuedRData = rr->NewRData;
			info->QueuedRDLen = rr->newrdlength;
			rr->NewRData = mDNSNULL;
			return mStatus_NoError;
			
		case regState_Registered:
			info->OrigRData = rr->resrec.rdata;
			info->OrigRDLen = rr->resrec.rdlength;
			info->InFlightRData = rr->NewRData;
			info->InFlightRDLen = rr->newrdlength;
			rr->NewRData = mDNSNULL;
			*stateptr = regState_UpdatePending;
			if (parent)  SendServiceRegistration(m, parent);
			else sendRecordRegistration(m, rr);
			return mStatus_NoError;

		case regState_NATError:
			LogMsg("ERROR: uDNS_UpdateRecord called for record %##s with bad state regState_NATError", rr->resrec.name->c);
			return mStatus_UnknownErr;  // states for service records only
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


mDNSlocal mDNSs32 CheckNATMappings(mDNS *m, mDNSs32 timenow)
	{
	NATTraversalInfo *ptr = m->uDNS_info.NATTraversals;
	mDNSs32 nextevent = timenow + MIN_UCAST_PERIODIC_EXEC;
	
	while (ptr)
		{
		NATTraversalInfo *cur = ptr;
		ptr = ptr->next;
		if (cur->op != NATOp_AddrRequest || cur->state != NATState_Established)  // no refresh necessary for established Add requests
			{
			if (cur->retry - timenow < 0)
				{
				if (cur->state == NATState_Established) RefreshNATMapping(cur, m);
				else if (cur->state == NATState_Request || cur->state == NATState_Refresh)
					{
					if (cur->ntries >= NATMAP_MAX_TRIES) cur->ReceiveResponse(cur, m, mDNSNULL, 0); // may invalidate "cur"
					else SendNATMsg(cur, m);
					}
				}
			else if (cur->retry - nextevent < 0) nextevent = cur->retry;
			}
		}
	return nextevent;
	}

mDNSlocal mDNSs32 CheckQueries(mDNS *m, mDNSs32 timenow)
	{
	DNSQuestion *q;
	uDNS_GlobalInfo *u = &m->uDNS_info;
	LLQ_Info *llq;
	mDNSs32 sendtime;
	mDNSs32 nextevent = timenow + MIN_UCAST_PERIODIC_EXEC;
	DNSMessage msg;
	mStatus err = mStatus_NoError;
	mDNSu8 *end;
	uDNS_QuestionInfo *info;
	
	u->CurrentQuery = u->ActiveQueries;
	while (u->CurrentQuery)
		{
		q = u->CurrentQuery;
		info = &q->uDNS_info;
		llq = info->llq;
		
		if (!info->internal && ((!q->LongLived && !info->Answered) || (llq && llq->state < LLQ_Established)) &&
			info->RestartTime + RESTART_GOODBYE_DELAY - timenow < 0)
			{
			// if we've been spinning on restart setup, and we have known answers, give goodbyes (they may be re-added later)
			while (info->knownAnswers)
				{
				CacheRecord *cr = info->knownAnswers;
				info->knownAnswers = info->knownAnswers->next;
				
				m->mDNS_reentrancy++; // Increment to allow client to legally make mDNS API calls from the callback
				q->QuestionCallback(m, q, &cr->resrec, mDNSfalse);
				m->mDNS_reentrancy--; // Decrement to block mDNS API calls again
				ufree(cr);
				if (q != u->CurrentQuery) { debugf("CheckQueries - question removed via callback."); break; }
				}
			}
		if (q != u->CurrentQuery) continue;
		
		if (q->LongLived && llq->state != LLQ_Poll)
			{
			if (llq->state >= LLQ_InitialRequest && llq->state <= LLQ_Established)
				{
				if (llq->retry - timenow < 0)
					{
					// sanity check to avoid packet flood bugs
					if (!llq->retry)
						LogMsg("ERROR: retry timer not set for LLQ %##s in state %d", q->qname.c, llq->state);
					else if (llq->state == LLQ_Established || llq->state == LLQ_Refresh)
						sendLLQRefresh(m, q, llq->origLease);
					else if (llq->state == LLQ_InitialRequest)
						startLLQHandshake(m, llq, mDNSfalse);
					else if (llq->state == LLQ_SecondaryRequest)
						sendChallengeResponse(m, q, mDNSNULL);
					else if (llq->state == LLQ_Retry)
						{ llq->ntries = 0; startLLQHandshake(m, llq, mDNSfalse); }
					}
				else if (llq->retry - nextevent < 0) nextevent = llq->retry;
				}
			}
		else
			{
			sendtime = q->LastQTime + q->ThisQInterval;
			if (m->SuppressStdPort53Queries &&
				sendtime - m->SuppressStdPort53Queries < 0) // Don't allow sendtime to be earlier than SuppressStdPort53Queries
				sendtime = m->SuppressStdPort53Queries;
			if (sendtime - timenow < 0)
				{
				DNSServer *server = GetServerForName(&m->uDNS_info, &q->qname);
				if (server)
					{
					if (server->teststate == DNSServer_Untested)
						{
						InitializeDNSMessage(&msg.h, newMessageID(&m->uDNS_info), uQueryFlags);
						end = putQuestion(&msg, msg.data, msg.data + AbsoluteMaxDNSMessageData, DNSRelayTestQuestion, kDNSType_PTR, kDNSClass_IN);
						}
					else
						err = constructQueryMsg(&msg, &end, q);
					if (err)  LogMsg("Error: uDNS_Idle - constructQueryMsg.  Skipping question %##s", q->qname.c);
					else
						{
						if (server->teststate != DNSServer_Failed)
							err = mDNSSendDNSMessage(m, &msg, end, mDNSInterface_Any, &server->addr, UnicastDNSPort, -1, mDNSNULL);
						m->SuppressStdPort53Queries = NonZeroTime(m->timenow + (mDNSPlatformOneSecond+99)/100);
						q->LastQTime = timenow;
						if (err) debugf("ERROR: uDNS_idle - mDNSSendDNSMessage - %ld", err); // surpress syslog messages if we have no network
						else if (q->ThisQInterval < MAX_UCAST_POLL_INTERVAL) q->ThisQInterval = q->ThisQInterval * 2;  // don't increase interval if send failed
						}
					}
				}
			else if (sendtime - nextevent < 0) nextevent = sendtime;
			}
		u->CurrentQuery = u->CurrentQuery->next;
		}
	return nextevent;
	}

mDNSlocal mDNSs32 CheckRecordRegistrations(mDNS *m, mDNSs32 timenow)
	{
	AuthRecord *rr;
	uDNS_RegInfo *rInfo;
	uDNS_GlobalInfo *u = &m->uDNS_info;
 	mDNSs32 nextevent = timenow + MIN_UCAST_PERIODIC_EXEC;
	
	//!!!KRS list should be pre-sorted by expiration
	for (rr = u->RecordRegistrations; rr; rr = rr->next)
		{
		rInfo = &rr->uDNS_info;
		if (rInfo->state == regState_Pending || rInfo->state == regState_DeregPending || rInfo->state == regState_UpdatePending || rInfo->state == regState_DeregDeferred || rInfo->state == regState_Refresh)
			{
			if (rr->LastAPTime + rr->ThisAPInterval - timenow < 0)
				{
#if MDNS_DEBUGMSGS
				char *op = "(unknown operation)";
				if (rInfo->state == regState_Pending) op = "registration";
				else if (rInfo->state == regState_DeregPending) op = "deregistration";
				else if (rInfo->state == regState_Refresh) op = "refresh";
				debugf("Retransmit record %s %##s", op, rr->resrec.name->c);
#endif
				//LogMsg("Retransmit record %##s", rr->resrec.name->c);
				if      (rInfo->state == regState_DeregPending)   SendRecordDeregistration(m, rr);
				else                                              sendRecordRegistration(m, rr);
				}
			if (rr->LastAPTime + rr->ThisAPInterval - nextevent < 0) nextevent = rr->LastAPTime + rr->ThisAPInterval;
			}
		if (rInfo->lease && rInfo->state == regState_Registered)
		    {
		    if (rInfo->expire - timenow < 0)
		        {
		        debugf("refreshing record %##s", rr->resrec.name->c);
		        rInfo->state = regState_Refresh;
		        sendRecordRegistration(m, rr);
		        }
		    if (rInfo->expire - nextevent < 0) nextevent = rInfo->expire;
		    }
		}
	return nextevent;
	}

mDNSlocal mDNSs32 CheckServiceRegistrations(mDNS *m, mDNSs32 timenow)
	{
	ServiceRecordSet *s = m->uDNS_info.ServiceRegistrations;
	uDNS_RegInfo *rInfo;
	mDNSs32 nextevent = timenow + MIN_UCAST_PERIODIC_EXEC;
	
	// Note: ServiceRegistrations list is in the order they were created; important for in-order event delivery
	while (s)
		{
		ServiceRecordSet *srs = s;
		// NOTE: Must advance s here -- SendServiceDeregistration may delete the object we're looking at,
		// and then if we tried to do srs = srs->next at the end we'd be referencing a dead object
		s = s->next;
		
		rInfo = &srs->uDNS_info;
		if (rInfo->state == regState_Pending || rInfo->state == regState_DeregPending || rInfo->state == regState_DeregDeferred || rInfo->state == regState_Refresh  || rInfo->state == regState_UpdatePending)
			{
			if (srs->RR_SRV.LastAPTime + srs->RR_SRV.ThisAPInterval - timenow < 0)
				{
#if MDNS_DEBUGMSGS
				char *op = "unknown";
				if (rInfo->state == regState_Pending) op = "registration";
				else if (rInfo->state == regState_DeregPending) op = "deregistration";
				else if (rInfo->state == regState_Refresh) op = "refresh";
				else if (rInfo->state == regState_UpdatePending) op = "txt record update";
				debugf("Retransmit service %s %##s", op, srs->RR_SRV.resrec.name->c);
#endif
				if (rInfo->state == regState_DeregPending) { SendServiceDeregistration(m, srs); continue; }
				else                                         SendServiceRegistration  (m, srs);
				}
			if (nextevent - srs->RR_SRV.LastAPTime + srs->RR_SRV.ThisAPInterval > 0)
				nextevent = srs->RR_SRV.LastAPTime + srs->RR_SRV.ThisAPInterval;
			}

		if (rInfo->lease && rInfo->state == regState_Registered)
		    {
		    if (rInfo->expire - timenow < 0)
		        {
			    debugf("refreshing service %##s", srs->RR_SRV.resrec.name->c);
			    rInfo->state = regState_Refresh;
			    SendServiceRegistration(m, srs);
		        }
		    if (rInfo->expire - nextevent < 0) nextevent = rInfo->expire;
		    }
		}
	return nextevent;
	}

mDNSexport void uDNS_Execute(mDNS *const m)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	mDNSs32 nexte, timenow = mDNSPlatformTimeNow(m);

	u->nextevent = timenow + MIN_UCAST_PERIODIC_EXEC;

	if (u->DelaySRVUpdate && u->NextSRVUpdate - timenow < 0)
		{
		u->DelaySRVUpdate = mDNSfalse;
		UpdateSRVRecords(m);
		}
	
	nexte = CheckNATMappings(m, timenow);
	if (nexte - u->nextevent < 0) u->nextevent = nexte;

	if (m->SuppressStdPort53Queries && m->timenow - m->SuppressStdPort53Queries >= 0)
		m->SuppressStdPort53Queries = 0; // If suppression time has passed, clear it

	nexte = CheckQueries(m, timenow);
	if (nexte - u->nextevent < 0) u->nextevent = nexte;

	nexte = CheckRecordRegistrations(m, timenow);
	if (nexte - u->nextevent < 0) u->nextevent = nexte;

	nexte = CheckServiceRegistrations(m, timenow);
	if (nexte - u->nextevent < 0) u->nextevent = nexte;
	
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Startup, Shutdown, and Sleep
#endif

// DeregisterActive causes active LLQs to be removed from the server, e.g. before sleep.  Pass false
// following a location change, as the server will reject deletions from a source address different
// from the address on which the LLQ was created.

mDNSlocal void SuspendLLQs(mDNS *m, mDNSBool DeregisterActive)
	{
	DNSQuestion *q;
	LLQ_Info *llq;
	for (q = m->uDNS_info.ActiveQueries; q; q = q->next)
		{
		llq = q->uDNS_info.llq;
		if (q->LongLived && llq)
			{
			if (llq->state == LLQ_GetZoneInfo)
				{
				debugf("Marking %##s suspend-deferred", q->qname.c);
				llq->state = LLQ_SuspendDeferred;  // suspend once we're done getting zone info
				}
			else if (llq->state < LLQ_Suspended)
				{
				if (DeregisterActive && (llq->state == LLQ_Established || llq->state == LLQ_Refresh))
					{ debugf("Deleting LLQ %##s", q->qname.c); sendLLQRefresh(m, q, 0); }
				debugf("Marking %##s suspended", q->qname.c);
				llq->state = LLQ_Suspended;
				ubzero(llq->id, 8);
				}
			else if (llq->state == LLQ_Poll) { debugf("Marking %##s suspended-poll", q->qname.c); llq->state = LLQ_SuspendedPoll; }
			if (llq->NATMap) llq->NATMap = mDNSfalse;  // may not need nat mapping if we restart with new route
			}
		}
	CheckForUnreferencedLLQMapping(m);
	}

mDNSlocal void RestartQueries(mDNS *m)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;
	DNSQuestion *q;
	LLQ_Info *llqInfo;
	mDNSs32 timenow = mDNSPlatformTimeNow(m);
	
	u->CurrentQuery = u->ActiveQueries;
	while (u->CurrentQuery)
		{
		q = u->CurrentQuery;
		u->CurrentQuery = u->CurrentQuery->next;
		llqInfo = q->uDNS_info.llq;
		q->uDNS_info.RestartTime = timenow;
		q->uDNS_info.Answered = mDNSfalse;
		if (q->LongLived)
			{
			if (!llqInfo) { LogMsg("Error: RestartQueries - %##s long-lived with NULL info", q->qname.c); continue; }
			if (llqInfo->state == LLQ_Suspended || llqInfo->state == LLQ_NatMapWait)
				{
				llqInfo->ntries = -1;
				llqInfo->deriveRemovesOnResume = mDNStrue;
				startLLQHandshake(m, llqInfo, mDNStrue);  // we set defer to true since several events that may generate restarts often arrive in rapid succession, and this cuts unnecessary packets
				}
			else if (llqInfo->state == LLQ_SuspendDeferred)
				llqInfo->state = LLQ_GetZoneInfo; // we never finished getting zone data - proceed as usual
			else if (llqInfo->state == LLQ_SuspendedPoll)
				{
				// if we were polling, we may have had bad zone data due to firewall, etc. - refetch
				llqInfo->ntries = 0;
				llqInfo->deriveRemovesOnResume = mDNStrue;
				llqInfo->state = LLQ_GetZoneInfo;
				startGetZoneData(&q->qname, m, mDNSfalse, mDNStrue, startLLQHandshakeCallback, llqInfo);
				}
			}
		else { q->LastQTime = timenow; q->ThisQInterval = INIT_UCAST_POLL_INTERVAL; } // trigger poll in 1 second (to reduce packet rate when restarts come in rapid succession)
		}
	}

mDNSexport void mDNS_UpdateLLQs(mDNS *m)
	{
	uDNS_GlobalInfo *u = &m->uDNS_info;

	mDNS_Lock(m);
	if (u->LLQNatInfo)
		{
		DeleteNATPortMapping(m, u->LLQNatInfo, mDNSNULL);
		FreeNATInfo(m, u->LLQNatInfo);  // routine clears u->LLQNatInfo ptr
		}
	SuspendLLQs(m, mDNStrue);
	RestartQueries(m);
	mDNS_Unlock(m);
	}

// simplest sleep logic - rather than having sleep states that must be dealt with explicitly in all parts of
// the code, we simply send a deregistration, and put the service in Refresh state, with a timeout far enough
// in the future that we'll sleep (or the sleep will be cancelled) before it is retransmitted.  Then to wake,
// we just move up the timers.



mDNSlocal void SleepRecordRegistrations(mDNS *m)
	{
	DNSMessage msg;
	AuthRecord *rr = m->uDNS_info.RecordRegistrations;
	mDNSs32 timenow = mDNSPlatformTimeNow(m);

	while (rr)
		{
		if (rr->uDNS_info.state == regState_Registered ||
			rr->uDNS_info.state == regState_Refresh)
			{
			mDNSu8 *ptr = msg.data, *end = (mDNSu8 *)&msg + sizeof(DNSMessage);
			InitializeDNSMessage(&msg.h, newMessageID(&m->uDNS_info), UpdateReqFlags);
			
			// construct deletion update
			ptr = putZone(&msg, ptr, end, &rr->uDNS_info.zone, mDNSOpaque16fromIntVal(rr->resrec.rrclass));
			if (!ptr) { LogMsg("Error: SleepRecordRegistrations - could not put zone"); return; }
			ptr = putDeletionRecord(&msg, ptr, &rr->resrec);
			if (!ptr) {  LogMsg("Error: SleepRecordRegistrations - could not put deletion record"); return; }

			mDNSSendDNSMessage(m, &msg, ptr, mDNSInterface_Any, &rr->uDNS_info.ns, rr->uDNS_info.port, -1, GetAuthInfoForName(&m->uDNS_info, rr->resrec.name));
			rr->uDNS_info.state = regState_Refresh;
			rr->LastAPTime = timenow;
			rr->ThisAPInterval = 300 * mDNSPlatformOneSecond;
			}
		rr = rr->next;
		}
	}

mDNSlocal void WakeRecordRegistrations(mDNS *m)
	{
	mDNSs32 timenow = mDNSPlatformTimeNow(m);
	AuthRecord *rr = m->uDNS_info.RecordRegistrations;

	while (rr)
		{
		if (rr->uDNS_info.state == regState_Refresh)
			{
			// trigger slightly delayed refresh (we usually get this message before kernel is ready to send packets)
			rr->LastAPTime = timenow;
			rr->ThisAPInterval = INIT_UCAST_POLL_INTERVAL;
			}
		rr = rr->next;
		}
	}

mDNSlocal void SleepServiceRegistrations(mDNS *m)
	{
	ServiceRecordSet *srs = m->uDNS_info.ServiceRegistrations;
	while(srs)
		{
		uDNS_RegInfo *info = &srs->uDNS_info;
		NATTraversalInfo *nat = info->NATinfo;
		
		if (nat)
			{
			if (nat->state == NATState_Established || nat->state == NATState_Refresh || nat->state == NATState_Legacy)
				DeleteNATPortMapping(m, nat, srs);
			nat->reg.ServiceRegistration = mDNSNULL;
			srs->uDNS_info.NATinfo = mDNSNULL;
			FreeNATInfo(m, nat);
			}

		if (info->state == regState_UpdatePending)
			{
			// act as if the update succeeded, since we're about to delete the name anyway
			AuthRecord *txt = &srs->RR_TXT;
			uDNS_RegInfo *txtInfo = &txt->uDNS_info;
			info->state = regState_Registered;
			// deallocate old RData
			if (txtInfo->UpdateRDCallback) txtInfo->UpdateRDCallback(m, txt, txtInfo->OrigRData);
			SetNewRData(&txt->resrec, txtInfo->InFlightRData, txtInfo->InFlightRDLen);
			txtInfo->OrigRData = mDNSNULL;
			txtInfo->InFlightRData = mDNSNULL;
			}

		if (info->state == regState_Registered || info->state == regState_Refresh)
			{
			mDNSOpaque16 origid  = srs->uDNS_info.id;
			info->state = regState_DeregPending;  // state expected by SendDereg()
			SendServiceDeregistration(m, srs);
			info->id = origid;
			info->state = regState_NoTarget;  // when we wake, we'll re-register (and optionally nat-map) once our address record completes
			srs->RR_SRV.resrec.rdata->u.srv.target.c[0] = 0;
			}
		srs = srs->next;
		}
	}

mDNSlocal void WakeServiceRegistrations(mDNS *m)
	{
	mDNSs32 timenow = mDNSPlatformTimeNow(m);
	ServiceRecordSet *srs = m->uDNS_info.ServiceRegistrations;
	while(srs)
		{
		if (srs->uDNS_info.state == regState_Refresh)
			{
			// trigger slightly delayed refresh (we usually get this message before kernel is ready to send packets)
			srs->RR_SRV.LastAPTime = timenow;
			srs->RR_SRV.ThisAPInterval = INIT_UCAST_POLL_INTERVAL;
			}
		srs = srs->next;
		}
	}

mDNSexport void uDNS_Init(mDNS *const m)
	{
	mDNSPlatformMemZero(&m->uDNS_info, sizeof(uDNS_GlobalInfo));
	m->uDNS_info.nextevent = m->timenow_last + 0x78000000;
	}

mDNSexport void uDNS_Sleep(mDNS *const m)
	{
	SuspendLLQs(m, mDNStrue);
	SleepServiceRegistrations(m);
	SleepRecordRegistrations(m);
	}

mDNSexport void uDNS_Wake(mDNS *const m)
	{
	RestartQueries(m);
	WakeServiceRegistrations(m);
	WakeRecordRegistrations(m);
	}
