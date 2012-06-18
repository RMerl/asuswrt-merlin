/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2003-2006 Apple Computer, Inc. All rights reserved.
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

$Log: uds_daemon.c,v $
Revision 1.463  2009/07/10 22:25:47  cheshire
Updated syslog messages for debugging unresponsive clients:
Will now log a warning message about an unresponsive client every ten seconds,
and then after 60 messagess (10 minutes) will terminate connection to that client.

Revision 1.462  2009/07/09 22:43:31  cheshire
Improved log messages for debugging unresponsive clients

Revision 1.461  2009/06/19 23:15:07  cheshire
<rdar://problem/6990066> Library: crash at handle_resolve_response + 183
Made resolve_result_callback code more defensive and improved LogOperation messages

Revision 1.460  2009/05/26 21:31:07  herscher
Fix compile errors on Windows

Revision 1.459  2009/04/30 20:07:51  mcguire
<rdar://problem/6822674> Support multiple UDSs from launchd

Revision 1.458  2009/04/25 00:59:06  mcguire
Change a few stray LogInfo to LogOperation

Revision 1.457  2009/04/22 01:19:57  jessic2
<rdar://problem/6814585> Daemon: mDNSResponder is logging garbage for error codes because it's using %ld for int 32

Revision 1.456  2009/04/21 01:56:34  jessic2
<rdar://problem/6803941> BTMM: Back out change for preventing other local users from sending packets to your BTMM machines

Revision 1.455  2009/04/20 19:19:57  cheshire
<rdar://problem/6803941> BTMM: If multiple local users are logged in to same BTMM account, all but one fail
Don't need "empty info->u.browser.browsers list" debugging message, now that we expect this to be
a case that can legitimately happen.

Revision 1.454  2009/04/18 20:56:43  jessic2
<rdar://problem/6803941> BTMM: If multiple local users are logged in to same BTMM account, all but one fail

Revision 1.453  2009/04/11 00:20:29  jessic2
<rdar://problem/4426780> Daemon: Should be able to turn on LogOperation dynamically

Revision 1.452  2009/04/07 01:17:42  jessic2
<rdar://problem/6747917> BTMM: Multiple accounts lets me see others' remote services & send packets to others' remote hosts

Revision 1.451  2009/04/02 22:34:26  jessic2
<rdar://problem/6305347> Race condition: If fd has already been closed, SO_NOSIGPIPE returns errno 22 (Invalid argument)

Revision 1.450  2009/04/01 21:11:28  herscher
<rdar://problem/5925472> Current Bonjour code does not compile on Windows. Workaround use of recvmsg.

Revision 1.449  2009/03/17 19:44:25  cheshire
<rdar://problem/6688927> Don't let negative unicast answers block Multicast DNS responses

Revision 1.448  2009/03/17 04:53:40  cheshire
<rdar://problem/6688927> Don't let negative unicast answers block Multicast DNS responses

Revision 1.447  2009/03/17 04:41:32  cheshire
Moved LogOperation message to after check for "if (answer->RecordType == kDNSRecordTypePacketNegative)"

Revision 1.446  2009/03/04 01:47:35  cheshire
Include m->ProxyRecords in SIGINFO output

Revision 1.445  2009/03/03 23:04:44  cheshire
For clarity, renamed "MAC" field to "HMAC" (Host MAC, as opposed to Interface MAC)

Revision 1.444  2009/03/03 22:51:55  cheshire
<rdar://problem/6504236> Sleep Proxy: Waking on same network but different interface will cause conflicts

Revision 1.443  2009/02/27 02:28:41  cheshire
Need to declare "const AuthRecord *ar;"

Revision 1.442  2009/02/27 00:58:17  cheshire
Improved detail of SIGINFO logging for m->DuplicateRecords

Revision 1.441  2009/02/24 22:18:59  cheshire
Include interface name for interface-specific AuthRecords

Revision 1.440  2009/02/21 01:38:08  cheshire
Added report of m->SleepState value in SIGINFO output

Revision 1.439  2009/02/18 23:38:44  cheshire
<rdar://problem/6600780> Could not write data to client 13 - aborting connection
Eliminated unnecessary "request_state *request" field from the reply_state structure.

Revision 1.438  2009/02/18 23:23:14  cheshire
Cleaned up debugging log messages

Revision 1.437  2009/02/17 23:29:05  cheshire
Throttle logging to a slower rate when running on SnowLeopard

Revision 1.436  2009/02/13 06:28:02  cheshire
Converted LogOperation messages to LogInfo

Revision 1.435  2009/02/12 20:57:26  cheshire
Renamed 'LogAllOperation' switch to 'LogClientOperations'; added new 'LogSleepProxyActions' switch

Revision 1.434  2009/02/12 20:28:31  cheshire
Added some missing "const" declarations

Revision 1.433  2009/02/10 01:44:39  cheshire
<rdar://problem/6553729> DNSServiceUpdateRecord fails with kDNSServiceErr_BadReference for otherwise valid reference

Revision 1.432  2009/02/10 01:38:56  cheshire
Move regservice_termination_callback() earlier in file in preparation for subsequent work

Revision 1.431  2009/02/07 01:48:55  cheshire
In SIGINFO output include sequence number for proxied records

Revision 1.430  2009/01/31 21:58:05  cheshire
<rdar://problem/4786302> Implement logic to determine when to send dot-local lookups via Unicast
Only want to do unicast dot-local lookups for address queries and conventional (RFC 2782) SRV queries

Revision 1.429  2009/01/31 00:45:26  cheshire
<rdar://problem/4786302> Implement logic to determine when to send dot-local lookups via Unicast
Further refinements

Revision 1.428  2009/01/30 19:52:31  cheshire
Eliminated unnecessary duplicated "dnssd_sock_t sd" fields in service_instance and reply_state structures

Revision 1.427  2009/01/24 01:48:43  cheshire
<rdar://problem/4786302> Implement logic to determine when to send dot-local lookups via Unicast

Revision 1.426  2009/01/16 21:07:08  cheshire
In SIGINFO "Duplicate Records" list, show expiry time for Sleep Proxy records

Revision 1.425  2009/01/16 20:53:16  cheshire
Include information about Sleep Proxy records in SIGINFO output

Revision 1.424  2009/01/12 22:43:50  cheshire
Fixed "unused variable" warning when SO_NOSIGPIPE is not defined

Revision 1.423  2009/01/10 22:54:42  mkrochma
<rdar://problem/5797544> Fixes from Igor Seleznev to get mdnsd working on Linux

Revision 1.422  2009/01/10 01:52:48  cheshire
Include DuplicateRecords and LocalOnlyQuestions in SIGINFO output

Revision 1.421  2008/12/17 05:05:26  cheshire
Fixed alignment of NAT mapping syslog messages

Revision 1.420  2008/12/12 00:52:05  cheshire
mDNSPlatformSetBPF is now called mDNSPlatformReceiveBPF_fd

Revision 1.419  2008/12/10 02:11:44  cheshire
ARMv5 compiler doesn't like uncommented stuff after #endif

Revision 1.418  2008/12/09 05:12:53  cheshire
Updated debugging messages

Revision 1.417  2008/12/04 03:38:12  cheshire
Miscellaneous defensive coding changes and improvements to debugging log messages

Revision 1.416  2008/12/02 22:02:12  cheshire
<rdar://problem/6320621> Adding domains after TXT record updates registers stale TXT record data

Revision 1.415  2008/11/26 20:35:59  cheshire
Changed some "LogOperation" debugging messages to "debugf"

Revision 1.414  2008/11/26 00:02:25  cheshire
Improved SIGINFO output to list AutoBrowseDomains and AutoRegistrationDomains

Revision 1.413  2008/11/25 04:48:58  cheshire
Added logging to show whether Sleep Proxy Service is active

Revision 1.412  2008/11/24 23:05:43  cheshire
Additional checking in uds_validatelists()

Revision 1.411  2008/11/05 21:41:39  cheshire
Updated LogOperation message

Revision 1.410  2008/11/04 20:06:20  cheshire
<rdar://problem/6186231> Change MAX_DOMAIN_NAME to 256

Revision 1.409  2008/10/31 23:44:22  cheshire
Fixed compile error in Posix build

Revision 1.408  2008/10/29 21:32:33  cheshire
Align "DNSServiceEnumerateDomains ... RESULT" log messages

Revision 1.407  2008/10/27 07:34:36  cheshire
Additional sanity checks for debugging

Revision 1.406  2008/10/23 23:55:56  cheshire
Fixed some missing "const" declarations

Revision 1.405  2008/10/23 23:21:31  cheshire
Moved definition of dnssd_strerror() to be with the definition of dnssd_errno, in dnssd_ipc.h

Revision 1.404  2008/10/23 23:06:17  cheshire
Removed () from dnssd_errno macro definition -- it's not a function and doesn't need any arguments

Revision 1.403  2008/10/23 22:33:25  cheshire
Changed "NOTE:" to "Note:" so that BBEdit 9 stops putting those comment lines into the funtion popup menu

Revision 1.402  2008/10/22 19:47:59  cheshire
Instead of SameRData(), use equivalent IdenticalSameNameRecord() macro

Revision 1.401  2008/10/22 17:20:40  cheshire
Don't give up if setsockopt SO_NOSIGPIPE fails

Revision 1.400  2008/10/21 01:06:57  cheshire
Pass BPF fd to mDNSMacOSX.c using mDNSPlatformSetBPF() instead of just writing it into a shared global variable

Revision 1.399  2008/10/20 22:06:42  cheshire
Updated debugging log messages

Revision 1.398  2008/10/03 18:25:17  cheshire
Instead of calling "m->MainCallback" function pointer directly, call mDNSCore routine "mDNS_ConfigChanged(m);"

Revision 1.397  2008/10/02 22:26:21  cheshire
Moved declaration of BPF_fd from uds_daemon.c to mDNSMacOSX.c, where it really belongs

Revision 1.396  2008/09/30 01:04:55  cheshire
Made BPF code a bit more defensive, to ignore subsequent BPF fds if we get passed more than one

Revision 1.395  2008/09/27 01:28:43  cheshire
Added code to receive and store BPF fd when passed via a send_bpf message

Revision 1.394  2008/09/23 04:12:40  cheshire
<rdar://problem/6238774> Remove "local" from the end of _services._dns-sd._udp PTR records
Added a special-case to massage these new records for Bonjour Browser's benefit

Revision 1.393  2008/09/23 03:01:58  cheshire
Added operation logging of domain enumeration results

Revision 1.392  2008/09/18 22:30:06  cheshire
<rdar://problem/6230679> device-info record not removed when last service deregisters

Revision 1.391  2008/09/18 22:05:44  cheshire
Fixed "DNSServiceRegister ... ADDED" message to have escaping consistent with
the other DNSServiceRegister operation messages

Revision 1.390  2008/09/16 21:06:56  cheshire
Improved syslog output to show if q->LongLived flag is set for multicast questions

Revision 1.389  2008/07/25 22:34:11  mcguire
fix sizecheck issues for 64bit

Revision 1.388  2008/07/01 01:40:02  mcguire
<rdar://problem/5823010> 64-bit fixes

Revision 1.387  2008/02/26 21:24:13  cheshire
Fixed spelling mistake in comment

Revision 1.386  2008/02/26 20:23:15  cheshire
Updated comments

Revision 1.385  2008/02/19 21:50:52  cheshire
Shortened some overly-long lines

Revision 1.384  2007/12/22 01:38:05  cheshire
Improve display of "Auth Records" SIGINFO output

Revision 1.383  2007/12/07 00:45:58  cheshire
<rdar://problem/5526800> BTMM: Need to deregister records and services on shutdown/sleep

Revision 1.382  2007/11/30 20:11:48  cheshire
Fixed compile warning: declaration of 'remove' shadows a global declaration

Revision 1.381  2007/11/28 22:02:52  cheshire
Remove pointless "if (!domain)" check (domain is an array on the stack, so its address can never be null)

Revision 1.380  2007/11/28 18:38:41  cheshire
Fixed typo in log message: "DNSServiceResolver" -> "DNSServiceResolve"

Revision 1.379  2007/11/01 19:32:14  cheshire
Added "DEBUG_64BIT_SCM_RIGHTS" debugging code

Revision 1.378  2007/10/31 19:21:40  cheshire
Don't show Expire time for records and services that aren't currently registered

Revision 1.377  2007/10/30 23:48:20  cheshire
Improved SIGINFO listing of question state

Revision 1.376  2007/10/30 20:43:54  cheshire
Fixed compiler warning when LogClientOperations is turned off

Revision 1.375  2007/10/26 22:51:38  cheshire
Improved SIGINFO output to show timers for AuthRecords and ServiceRegistrations

Revision 1.374  2007/10/25 22:45:02  cheshire
Tidied up code for DNSServiceRegister callback status messages

Revision 1.373  2007/10/25 21:28:43  cheshire
Add ServiceRegistrations to SIGINFO output

Revision 1.372  2007/10/25 21:21:45  cheshire
<rdar://problem/5496734> BTMM: Need to retry registrations after failures
Don't unlink_and_free_service_instance at the first error

Revision 1.371  2007/10/18 23:34:40  cheshire
<rdar://problem/5532821> Need "considerable burden on the network" warning in uds_daemon.c

Revision 1.370  2007/10/17 18:44:23  cheshire
<rdar://problem/5539930> Goodbye packets not being sent for services on shutdown

Revision 1.369  2007/10/16 17:18:27  cheshire
Fixed Posix compile errors

Revision 1.368  2007/10/16 16:58:58  cheshire
Improved debugging error messages in read_msg()

Revision 1.367  2007/10/15 22:55:14  cheshire
Make read_msg return "void" (since request_callback just ignores the redundant return value anyway)

Revision 1.366  2007/10/10 00:48:54  cheshire
<rdar://problem/5526379> Daemon spins in an infinite loop when it doesn't get the control message it's expecting

Revision 1.365  2007/10/06 03:25:23  cheshire
<rdar://problem/5525267> MacBuddy exits abnormally when clicking "Continue" in AppleConnect pane

Revision 1.364  2007/10/06 03:20:16  cheshire
Improved LogOperation debugging messages

Revision 1.363  2007/10/05 23:24:52  cheshire
Improved LogOperation messages about separate error return socket

Revision 1.362  2007/10/05 22:11:58  cheshire
Improved "send_msg ERROR" debugging message

Revision 1.361  2007/10/04 20:45:18  cheshire
<rdar://problem/5518381> Race condition in kDNSServiceFlagsShareConnection-mode call handling

Revision 1.360  2007/10/01 23:24:46  cheshire
SIGINFO output was mislabeling mDNSInterface_Any queries as unicast queries

Revision 1.359  2007/09/30 00:09:27  cheshire
<rdar://problem/5492315> Pass socket fd via SCM_RIGHTS sendmsg instead of using named UDS in the filesystem

Revision 1.358  2007/09/29 20:08:06  cheshire
Fixed typo in comment

Revision 1.357  2007/09/27 22:10:04  cheshire
Add LogOperation line for DNSServiceRegisterRecord callbacks

Revision 1.356  2007/09/26 21:29:30  cheshire
Improved question list SIGINFO output

Revision 1.355  2007/09/26 01:54:34  mcguire
Debugging: In SIGINFO output, show ClientTunnel query interval, which is how we determine whether a query is still active

Revision 1.354  2007/09/26 01:26:31  cheshire
<rdar://problem/5501567> BTMM: mDNSResponder crashes in free_service_instance enabling/disabling BTMM
Need to call SendServiceRemovalNotification *before* backpointer is cleared

Revision 1.353  2007/09/25 20:46:33  cheshire
Include DNSServiceRegisterRecord operations in SIGINFO output

Revision 1.352  2007/09/25 20:23:40  cheshire
<rdar://problem/5501567> BTMM: mDNSResponder crashes in free_service_instance enabling/disabling BTMM
Need to clear si->request backpointer before calling mDNS_DeregisterService(&mDNSStorage, &si->srs);

Revision 1.351  2007/09/25 18:20:34  cheshire
Changed name of "free_service_instance" to more accurate "unlink_and_free_service_instance"

Revision 1.350  2007/09/24 23:54:52  mcguire
Additional list checking in uds_validatelists()

Revision 1.349  2007/09/24 06:01:00  cheshire
Debugging: In SIGINFO output, show NAT Traversal time values in seconds rather than platform ticks

Revision 1.348  2007/09/24 05:02:41  cheshire
Debugging: In SIGINFO output, indicate explicitly when a given section is empty

Revision 1.347  2007/09/21 02:04:33  cheshire
<rdar://problem/5440831> BTMM: mDNSResponder crashes in free_service_instance enabling/disabling BTMM

Revision 1.346  2007/09/19 22:47:25  cheshire
<rdar://problem/5490182> Memory corruption freeing a "no such service" service record

Revision 1.345  2007/09/19 20:32:29  cheshire
<rdar://problem/5482322> BTMM: Don't advertise SMB with BTMM because it doesn't support IPv6

Revision 1.344  2007/09/19 19:27:50  cheshire
<rdar://problem/5492182> Improved diagnostics when daemon can't connect to error return path socket

Revision 1.343  2007/09/18 21:42:30  cheshire
To reduce programming mistakes, renamed ExtPort to RequestedPort

Revision 1.342  2007/09/14 22:38:20  cheshire
Additional list checking in uds_validatelists()

Revision 1.341  2007/09/13 00:16:43  cheshire
<rdar://problem/5468706> Miscellaneous NAT Traversal improvements

Revision 1.340  2007/09/12 23:03:08  cheshire
<rdar://problem/5476978> DNSServiceNATPortMappingCreate callback not giving correct interface index

Revision 1.339  2007/09/12 19:22:21  cheshire
Variable renaming in preparation for upcoming fixes e.g. priv/pub renamed to intport/extport
Made NAT Traversal packet handlers take typed data instead of anonymous "mDNSu8 *" byte pointers

Revision 1.338  2007/09/12 01:22:13  cheshire
Improve validatelists() checking to detect when 'next' pointer gets smashed to ~0

Revision 1.337  2007/09/07 23:05:04  cheshire
Add display of client_context field in handle_cancel_request() LogOperation message
While loop was checking client_context.u32[2] instead of client_context.u32[1]

Revision 1.336  2007/09/07 20:56:03  cheshire
Renamed uint32_t field in client_context_t from "ptr64" to more accurate name "u32"

Revision 1.335  2007/09/05 22:25:01  vazquez
<rdar://problem/5400521> update_record mDNSResponder leak

Revision 1.334  2007/09/05 20:43:57  cheshire
Added LogOperation message showing fd of socket listening for incoming Unix Domain Socket client requests

Revision 1.333  2007/08/28 23:32:35  cheshire
Added LogOperation messages for DNSServiceNATPortMappingCreate() operations

Revision 1.332  2007/08/27 22:59:31  cheshire
Show reg_index in DNSServiceRegisterRecord/DNSServiceRemoveRecord messages

Revision 1.331  2007/08/27 20:29:57  cheshire
Added SIGINFO listing of TunnelClients

Revision 1.330  2007/08/24 23:46:50  cheshire
Added debugging messages and SIGINFO listing of DomainAuthInfo records

Revision 1.329  2007/08/18 01:02:04  mcguire
<rdar://problem/5415593> No Bonjour services are getting registered at boot

Revision 1.328  2007/08/15 20:18:28  vazquez
<rdar://problem/5400521> update_record mDNSResponder leak
Make sure we free all ExtraResourceRecords

Revision 1.327  2007/08/08 22:34:59  mcguire
<rdar://problem/5197869> Security: Run mDNSResponder as user id mdnsresponder instead of root

Revision 1.326  2007/08/01 16:09:14  cheshire
Removed unused NATTraversalInfo substructure from AuthRecord; reduced structure sizecheck values accordingly

Revision 1.325  2007/07/31 21:29:41  cheshire
<rdar://problem/5372207> System Default registration domain(s) not listed in Domain Enumeration ("dns-sd -E")

Revision 1.324  2007/07/31 01:56:21  cheshire
Corrected function name in log message

Revision 1.323  2007/07/27 23:57:23  cheshire
Added compile-time structure size checks

Revision 1.322  2007/07/27 19:37:19  cheshire
Moved AutomaticBrowseDomainQ into main mDNS object

Revision 1.321  2007/07/27 19:30:41  cheshire
Changed mDNSQuestionCallback parameter from mDNSBool to QC_result,
to properly reflect tri-state nature of the possible responses

Revision 1.320  2007/07/27 00:48:27  cheshire
<rdar://problem/4700198> BTMM: Services should only get registered in .Mac domain of current user
<rdar://problem/4731180> BTMM: Only browse in the current user's .Mac domain by default

Revision 1.319  2007/07/24 17:23:33  cheshire
<rdar://problem/5357133> Add list validation checks for debugging

Revision 1.318  2007/07/23 23:09:51  cheshire
<rdar://problem/5351997> Reject oversized client requests

Revision 1.317  2007/07/23 22:24:47  cheshire
<rdar://problem/5352299> Make mDNSResponder more defensive against malicious local clients
Additional refinements

Revision 1.316  2007/07/23 22:12:53  cheshire
<rdar://problem/5352299> Make mDNSResponder more defensive against malicious local clients

Revision 1.315  2007/07/21 01:36:13  cheshire
Need to also add ".local" as automatic browsing domain

Revision 1.314  2007/07/20 20:12:37  cheshire
Rename "mDNS_DomainTypeBrowseLegacy" as "mDNS_DomainTypeBrowseAutomatic"

Revision 1.313  2007/07/20 00:54:21  cheshire
<rdar://problem/4641118> Need separate SCPreferences for per-user .Mac settings

Revision 1.312  2007/07/11 03:06:43  cheshire
<rdar://problem/5303807> Register IPv6-only hostname and don't create port mappings for AutoTunnel services

Revision 1.311  2007/07/06 21:19:18  cheshire
Add list of NAT traversals to SIGINFO output

Revision 1.310  2007/07/03 19:56:50  cheshire
Add LogOperation message for DNSServiceSetDefaultDomainForUser

Revision 1.309  2007/06/29 23:12:49  vazquez
<rdar://problem/5294103> Stop using generate_final_fatal_reply_with_garbage

Revision 1.308  2007/06/29 00:10:07  vazquez
<rdar://problem/5301908> Clean up NAT state machine (necessary for 6 other fixes)

Revision 1.307  2007/05/25 00:25:44  cheshire
<rdar://problem/5227737> Need to enhance putRData to output all current known types

Revision 1.306  2007/05/24 22:31:35  vazquez
Bug #: 4272956
Reviewed by: Stuart Cheshire
<rdar://problem/4272956> WWDC API: Return ADD/REMOVE events in registration callback

Revision 1.305  2007/05/23 18:59:22  cheshire
Remove unnecessary IPC_FLAGS_REUSE_SOCKET

Revision 1.304  2007/05/22 01:07:42  cheshire
<rdar://problem/3563675> API: Need a way to get version/feature information

Revision 1.303  2007/05/22 00:32:58  cheshire
Make a send_all() subroutine -- will be helpful for implementing DNSServiceGetProperty(DaemonVersion)

Revision 1.302  2007/05/21 18:54:54  cheshire
Add "Cancel" LogOperation message when we get a cancel_request command over the UDS

Revision 1.301  2007/05/18 23:55:22  cheshire
<rdar://problem/4454655> Allow multiple register/browse/resolve operations to share single Unix Domain Socket

Revision 1.300  2007/05/18 21:27:11  cheshire
Rename connected_registration_termination to connection_termination

Revision 1.299  2007/05/18 21:24:34  cheshire
Rename rstate to request

Revision 1.298  2007/05/18 21:22:35  cheshire
Convert uint16_t etc. to their locally-defined equivalents, like the rest of the core code

Revision 1.297  2007/05/18 20:33:11  cheshire
Avoid declaring lots of uninitialized variables in read_rr_from_ipc_msg

Revision 1.296  2007/05/18 19:04:19  cheshire
Rename msgdata to msgptr (may be modified); rename (currently unused) bufsize to msgend

Revision 1.295  2007/05/18 17:57:13  cheshire
Reorder functions in file to arrange them in logical groups; added "#pragma mark" headers for each group

Revision 1.294  2007/05/17 20:58:22  cheshire
<rdar://problem/4647145> DNSServiceQueryRecord should return useful information with NXDOMAIN

Revision 1.293  2007/05/17 19:46:20  cheshire
Routine name deliver_async_error() is misleading. What it actually does is write a message header
(containing an error code) followed by 256 bytes of garbage zeroes onto a client connection,
thereby trashing it and making it useless for any subsequent communication. It's destructive,
and not very useful. Changing name to generate_final_fatal_reply_with_garbage().

Revision 1.292  2007/05/16 01:06:52  cheshire
<rdar://problem/4471320> Improve reliability of kDNSServiceFlagsMoreComing flag on multiprocessor machines

Revision 1.291  2007/05/15 21:57:16  cheshire
<rdar://problem/4608220> Use dnssd_SocketValid(x) macro instead of just
assuming that all negative values (or zero!) are invalid socket numbers

Revision 1.290  2007/05/10 23:30:57  cheshire
<rdar://problem/4084490> Only one browse gets remove events when disabling browse domain

Revision 1.289  2007/05/02 22:18:08  cheshire
Renamed NATTraversalInfo_struct context to NATTraversalContext

Revision 1.288  2007/04/30 21:33:39  cheshire
Fix crash when a callback unregisters a service while the UpdateSRVRecords() loop
is iterating through the m->ServiceRegistrations list

Revision 1.287  2007/04/27 19:03:22  cheshire
Check q->LongLived not q->llq to tell if a query is LongLived

Revision 1.286  2007/04/26 16:00:01  cheshire
Show interface number in DNSServiceBrowse RESULT output

Revision 1.285  2007/04/22 19:03:39  cheshire
Minor code tidying

Revision 1.284  2007/04/22 06:02:03  cheshire
<rdar://problem/4615977> Query should immediately return failure when no server

Revision 1.283  2007/04/21 21:47:47  cheshire
<rdar://problem/4376383> Daemon: Add watchdog timer

Revision 1.282  2007/04/20 21:17:24  cheshire
For naming consistency, kDNSRecordTypeNegative should be kDNSRecordTypePacketNegative

Revision 1.281  2007/04/19 23:25:20  cheshire
Added debugging message

Revision 1.280  2007/04/17 19:21:29  cheshire
<rdar://problem/5140339> Domain discovery not working over VPN

Revision 1.279  2007/04/16 21:53:49  cheshire
Improve display of negative cache entries

Revision 1.278  2007/04/16 20:49:40  cheshire
Fix compile errors for mDNSPosix build

Revision 1.277  2007/04/05 22:55:36  cheshire
<rdar://problem/5077076> Records are ending up in Lighthouse without expiry information

Revision 1.276  2007/04/05 19:20:13  cheshire
Non-blocking mode not being set correctly -- was clobbering other flags

Revision 1.275  2007/04/04 21:21:25  cheshire
<rdar://problem/4546810> Fix crash: In regservice_callback service_instance was being referenced after being freed

Revision 1.274  2007/04/04 01:30:42  cheshire
<rdar://problem/5075200> DNSServiceAddRecord is failing to advertise NULL record
Add SIGINFO output lising our advertised Authoritative Records

Revision 1.273  2007/04/04 00:03:27  cheshire
<rdar://problem/5089862> DNSServiceQueryRecord is returning kDNSServiceErr_NoSuchRecord for empty rdata

Revision 1.272  2007/04/03 20:10:32  cheshire
Show ADD/RMV in DNSServiceQueryRecord log message instead of just "RESULT"

Revision 1.271  2007/04/03 19:22:32  cheshire
Use mDNSSameIPv4Address (and similar) instead of accessing internal fields directly

Revision 1.270  2007/03/30 21:55:30  cheshire
Added comments

Revision 1.269  2007/03/29 01:31:44  cheshire
Faulty logic was incorrectly suppressing some NAT port mapping callbacks

Revision 1.268  2007/03/29 00:13:58  cheshire
Remove unnecessary fields from service_instance structure: autoname, autorename, allowremotequery, name

Revision 1.267  2007/03/28 20:59:27  cheshire
<rdar://problem/4743285> Remove inappropriate use of IsPrivateV4Addr()

Revision 1.266  2007/03/28 15:56:37  cheshire
<rdar://problem/5085774> Add listing of NAT port mapping and GetAddrInfo requests in SIGINFO output

Revision 1.265  2007/03/27 22:52:07  cheshire
Fix crash in udsserver_automatic_browse_domain_changed

Revision 1.264  2007/03/27 00:49:40  cheshire
Should use mallocL, not plain malloc

Revision 1.263  2007/03/27 00:45:01  cheshire
Removed unnecessary "void *termination_context" pointer

Revision 1.262  2007/03/27 00:40:43  cheshire
Eliminate resolve_termination_t as a separately-allocated structure, and make it part of the request_state union

Revision 1.261  2007/03/27 00:29:00  cheshire
Eliminate queryrecord_request data as a separately-allocated structure, and make it part of the request_state union

Revision 1.260  2007/03/27 00:18:42  cheshire
Eliminate enum_termination_t and domain_enum_t as separately-allocated structures,
and make them part of the request_state union

Revision 1.259  2007/03/26 23:48:16  cheshire
<rdar://problem/4848295> Advertise model information via Bonjour
Refinements to reduce unnecessary transmissions of the DeviceInfo TXT record

Revision 1.258  2007/03/24 00:40:04  cheshire
Minor code cleanup

Revision 1.257  2007/03/24 00:23:12  cheshire
Eliminate port_mapping_info_t as a separately-allocated structure, and make it part of the request_state union

Revision 1.256  2007/03/24 00:07:18  cheshire
Eliminate addrinfo_info_t as a separately-allocated structure, and make it part of the request_state union

Revision 1.255  2007/03/23 23:56:14  cheshire
Move list of record registrations into the request_state union

Revision 1.254  2007/03/23 23:48:56  cheshire
Eliminate service_info as a separately-allocated structure, and make it part of the request_state union

Revision 1.253  2007/03/23 23:04:29  cheshire
Eliminate browser_info_t as a separately-allocated structure, and make it part of request_state

Revision 1.252  2007/03/23 22:59:58  cheshire
<rdar://problem/4848295> Advertise model information via Bonjour
Use kStandardTTL, not kHostNameTTL

Revision 1.251  2007/03/23 22:44:07  cheshire
Instead of calling AbortUnlinkAndFree() haphazardly all over the place, make the handle* routines
return an error code, and then request_callback() does all necessary cleanup in one place.

Revision 1.250  2007/03/22 20:30:07  cheshire
Remove pointless "if (request->ts != t_complete) ..." checks

Revision 1.249  2007/03/22 20:13:27  cheshire
Delete unused client_context field

Revision 1.248  2007/03/22 20:03:37  cheshire
Rename variables for clarity: instead of using variable rs for both request_state
and reply_state, use req for request_state and rep for reply_state

Revision 1.247  2007/03/22 19:31:42  cheshire
<rdar://problem/4848295> Advertise model information via Bonjour
Add missing "model=" at start of DeviceInfo data

Revision 1.246  2007/03/22 18:31:48  cheshire
Put dst parameter first in mDNSPlatformStrCopy/mDNSPlatformMemCopy, like conventional Posix strcpy/memcpy

Revision 1.245  2007/03/22 00:49:20  cheshire
<rdar://problem/4848295> Advertise model information via Bonjour

Revision 1.244  2007/03/21 21:01:48  cheshire
<rdar://problem/4789793> Leak on error path in regrecord_callback, uds_daemon.c

Revision 1.243  2007/03/21 19:01:57  cheshire
<rdar://problem/5078494> IPC code not 64-bit-savvy: assumes long=32bits, and short=16bits

Revision 1.242  2007/03/21 18:51:21  cheshire
<rdar://problem/4549320> Code in uds_daemon.c passes function name instead of type name to mallocL/freeL

Revision 1.241  2007/03/20 00:04:50  cheshire
<rdar://problem/4837929> Should allow "udp" or "tcp" for protocol command-line arg
Fix LogOperation("DNSServiceNATPortMappingCreate(...)") message to actually show client arguments

Revision 1.240  2007/03/16 23:25:35  cheshire
<rdar://problem/5067001> NAT-PMP: Parameter validation not working correctly

Revision 1.239  2007/03/10 02:29:36  cheshire
Added comment about port_mapping_create_reply()

Revision 1.238  2007/03/07 00:26:48  cheshire
<rdar://problem/4426754> DNSServiceRemoveRecord log message should include record type

Revision 1.237  2007/02/28 01:44:29  cheshire
<rdar://problem/5027863> Byte order bugs in uDNS.c, uds_daemon.c, dnssd_clientstub.c

Revision 1.236  2007/02/14 01:58:19  cheshire
<rdar://problem/4995831> Don't delete Unix Domain Socket on exit if we didn't create it on startup

Revision 1.235  2007/02/08 21:12:28  cheshire
<rdar://problem/4386497> Stop reading /etc/mDNSResponder.conf on every sleep/wake

Revision 1.234  2007/02/06 19:06:49  cheshire
<rdar://problem/3956518> Need to go native with launchd

Revision 1.233  2007/01/10 20:49:37  cheshire
Remove unnecessary setting of q->Private fields

Revision 1.232  2007/01/09 00:03:23  cheshire
Call udsserver_handle_configchange() once at the end of udsserver_init()
to set up the automatic registration and browsing domains.

Revision 1.231  2007/01/06 02:50:19  cheshire
<rdar://problem/4632919> Instead of copying SRV and TXT record data, just store pointers to cache entities

Revision 1.230  2007/01/06 01:00:35  cheshire
Improved SIGINFO output

Revision 1.229  2007/01/05 08:30:56  cheshire
Trim excessive "$Log" checkin history from before 2006
(checkin history still available via "cvs log ..." of course)

Revision 1.228  2007/01/05 08:09:05  cheshire
Reorder code into functional sections, with "#pragma mark" headers

Revision 1.227  2007/01/05 07:04:24  cheshire
Minor code tidying

Revision 1.226  2007/01/05 05:44:35  cheshire
Move automatic browse/registration management from uDNS.c to mDNSShared/uds_daemon.c,
so that mDNSPosix embedded clients will compile again

Revision 1.225  2007/01/04 23:11:15  cheshire
<rdar://problem/4720673> uDNS: Need to start caching unicast records
When an automatic browsing domain is removed, generate appropriate "remove" events for legacy queries

Revision 1.224  2007/01/04 20:57:49  cheshire
Rename ReturnCNAME to ReturnIntermed (for ReturnIntermediates)

Revision 1.223  2006/12/21 01:25:49  cheshire
Tidy up SIGINFO state log

Revision 1.222  2006/12/21 00:15:22  cheshire
Get rid of gmDNS macro; fixed a crash in udsserver_info()

Revision 1.221  2006/12/20 04:07:38  cheshire
Remove uDNS_info substructure from AuthRecord_struct

Revision 1.220  2006/12/19 22:49:25  cheshire
Remove uDNS_info substructure from ServiceRecordSet_struct

Revision 1.219  2006/12/14 03:02:38  cheshire
<rdar://problem/4838433> Tools: dns-sd -G 0 only returns IPv6 when you have a routable IPv6 address

Revision 1.218  2006/11/18 05:01:33  cheshire
Preliminary support for unifying the uDNS and mDNS code,
including caching of uDNS answers

Revision 1.217  2006/11/15 19:27:53  mkrochma
<rdar://problem/4838433> Tools: dns-sd -G 0 only returns IPv6 when you have a routable IPv6 address

Revision 1.216  2006/11/10 00:54:16  cheshire
<rdar://problem/4816598> Changing case of Computer Name doesn't work

Revision 1.215  2006/10/27 01:30:23  cheshire
Need explicitly to set ReturnIntermed = mDNSfalse

Revision 1.214  2006/10/20 05:37:23  herscher
Display question list information in udsserver_info()

Revision 1.213  2006/10/05 03:54:31  herscher
Remove embedded uDNS_info struct from DNSQuestion_struct

Revision 1.212  2006/09/30 01:22:35  cheshire
Put back UTF-8 curly quotes in log messages

Revision 1.211  2006/09/27 00:44:55  herscher
<rdar://problem/4249761> API: Need DNSServiceGetAddrInfo()

Revision 1.210  2006/09/26 01:52:41  herscher
<rdar://problem/4245016> NAT Port Mapping API (for both NAT-PMP and UPnP Gateway Protocol)

Revision 1.209  2006/09/21 21:34:09  cheshire
<rdar://problem/4100000> Allow empty string name when using kDNSServiceFlagsNoAutoRename

Revision 1.208  2006/09/21 21:28:24  cheshire
Code cleanup to make it consistent with daemon.c: change rename_on_memfree to renameonmemfree

Revision 1.207  2006/09/15 21:20:16  cheshire
Remove uDNS_info substructure from mDNS_struct

Revision 1.206  2006/08/14 23:24:56  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.205  2006/07/20 22:07:30  mkrochma
<rdar://problem/4633196> Wide-area browsing is currently broken in TOT
More fixes for uninitialized variables

Revision 1.204  2006/07/15 02:01:33  cheshire
<rdar://problem/4472014> Add Private DNS client functionality to mDNSResponder
Fix broken "empty string" browsing

Revision 1.203  2006/07/07 01:09:13  cheshire
<rdar://problem/4472013> Add Private DNS server functionality to dnsextd
Only use mallocL/freeL debugging routines when building mDNSResponder, not dnsextd

Revision 1.202  2006/07/05 22:00:10  cheshire
Wide-area cleanup: Rename mDNSPlatformGetRegDomainList() to uDNS_GetDefaultRegDomainList()

Revision 1.201  2006/06/29 03:02:47  cheshire
<rdar://problem/4607042> mDNSResponder NXDOMAIN and CNAME support

Revision 1.200  2006/06/28 08:56:26  cheshire
Added "_op" to the end of the operation code enum values,
to differentiate them from the routines with the same names

Revision 1.199  2006/06/28 08:53:39  cheshire
Added (commented out) debugging messages

Revision 1.198  2006/06/27 20:16:07  cheshire
Fix code layout

Revision 1.197  2006/05/18 01:32:35  cheshire
<rdar://problem/4472706> iChat: Lost connection with Bonjour
(mDNSResponder insufficiently defensive against malformed browsing PTR responses)

Revision 1.196  2006/05/05 07:07:13  cheshire
<rdar://problem/4538206> mDNSResponder fails when UDS reads deliver partial data

Revision 1.195  2006/04/25 20:56:28  mkrochma
Added comment about previous checkin

Revision 1.194  2006/04/25 18:29:36  mkrochma
Workaround for warning: unused variable 'status' when building mDNSPosix

Revision 1.193  2006/03/19 17:14:38  cheshire
<rdar://problem/4483117> Need faster purging of stale records
read_rr_from_ipc_msg was not setting namehash and rdatahash

Revision 1.192  2006/03/18 20:58:32  cheshire
Misplaced curly brace

Revision 1.191  2006/03/10 22:19:43  cheshire
Update debugging message in resolve_result_callback() to indicate whether event is ADD or RMV

Revision 1.190  2006/03/10 21:56:12  cheshire
<rdar://problem/4111464> After record update, old record sometimes remains in cache
When service TXT and SRV record both change, clients with active resolve calls get *two* callbacks, one
when the TXT data changes, and then immediately afterwards a second callback with the new port number
This change suppresses the first unneccessary (and confusing) callback

Revision 1.189  2006/01/06 00:56:31  cheshire
<rdar://problem/4400573> Should remove PID file on exit

*/

#if defined(_WIN32)
#include <process.h>
#define usleep(X) Sleep(((X)+999)/1000)
#else
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include "mDNSEmbeddedAPI.h"
#include "DNSCommon.h"
#include "uDNS.h"
#include "uds_daemon.h"

// Apple-specific functionality, not required for other platforms
#if APPLE_OSX_mDNSResponder
#include <sys/ucred.h>
#ifndef PID_FILE
#define PID_FILE ""
#endif
#endif

// User IDs 0-500 are system-wide processes, not actual users in the usual sense
// User IDs for real user accounts start at 501 and count up from there
#define SystemUID(X) ((X) <= 500)

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Types and Data Structures
#endif

typedef enum
	{
	t_uninitialized,
	t_morecoming,
	t_complete,
	t_error,
	t_terminated
	} transfer_state;

typedef struct request_state request_state;

typedef void (*req_termination_fn)(request_state *request);

typedef struct registered_record_entry
	{
	struct registered_record_entry *next;
	mDNSu32 key;
	AuthRecord *rr;				// Pointer to variable-sized AuthRecord
	client_context_t regrec_client_context;
	request_state *request;
	} registered_record_entry;

// A single registered service: ServiceRecordSet + bookkeeping
// Note that we duplicate some fields from parent service_info object
// to facilitate cleanup, when instances and parent may be deallocated at different times.
typedef struct service_instance
	{
	struct service_instance *next;
	request_state *request;
	AuthRecord *subtypes;
	mDNSBool renameonmemfree;  		// Set on config change when we deregister original name
    mDNSBool clientnotified;		// Has client been notified of successful registration yet?
	mDNSBool default_local;			// is this the "local." from an empty-string registration?
	domainname domain;
	ServiceRecordSet srs;			// note -- variable-sized object -- must be last field in struct
	} service_instance;

// for multi-domain default browsing
typedef struct browser_t
	{
	struct browser_t *next;
	domainname domain;
	DNSQuestion q;
	} browser_t;

struct request_state
	{
	request_state *next;
	request_state *primary;			// If this operation is on a shared socket, pointer to primary
									// request_state for the original DNSServiceConnect() operation
	dnssd_sock_t sd;
	dnssd_sock_t errsd;
	mDNSu32 uid;

	// Note: On a shared connection these fields in the primary structure, including hdr, are re-used
	// for each new request. This is because, until we've read the ipc_msg_hdr to find out what the
	// operation is, we don't know if we're going to need to allocate a new request_state or not.
	transfer_state ts;
	mDNSu32        hdr_bytes;		// bytes of header already read
	ipc_msg_hdr    hdr;
	mDNSu32        data_bytes;		// bytes of message data already read
	char          *msgbuf;			// pointer to data storage to pass to free()
	const char    *msgptr;			// pointer to data to be read from (may be modified)
	char          *msgend;			// pointer to byte after last byte of message

	// reply, termination, error, and client context info
	int no_reply;					// don't send asynchronous replies to client
	mDNSs32 time_blocked;			// record time of a blocked client
	int unresponsiveness_reports;
	struct reply_state *replies;	// corresponding (active) reply list
	req_termination_fn terminate;

	union
		{
		registered_record_entry *reg_recs;  // list of registrations for a connection-oriented request
		struct
			{
			mDNSInterfaceID interface_id;
			mDNSBool default_domain;
			mDNSBool ForceMCast;
			domainname regtype;
			browser_t *browsers;
			} browser;
		struct
			{
			mDNSInterfaceID InterfaceID;
			mDNSu16 txtlen;
			void *txtdata;
			mDNSIPPort port;
			domainlabel name;
			char type_as_string[MAX_ESCAPED_DOMAIN_NAME];
			domainname type;
			mDNSBool default_domain;
			domainname host;
			mDNSBool autoname;				// Set if this name is tied to the Computer Name
			mDNSBool autorename;			// Set if this client wants us to automatically rename on conflict
			mDNSBool allowremotequery;		// Respond to unicast queries from outside the local link?
			int num_subtypes;
			service_instance *instances;
			} servicereg;
		struct
			{
			mDNSInterfaceID      interface_id;
			mDNSu32              flags;
			mDNSu32              protocol;
			DNSQuestion          q4;
			DNSQuestion          q6;
			} addrinfo;
		struct
			{
			mDNSIPPort           ReqExt;	// External port we originally requested, for logging purposes
			NATTraversalInfo     NATinfo;
			} pm;
		struct
			{
#if 0
			DNSServiceFlags flags;
#endif
			DNSQuestion q_all;
			DNSQuestion q_default;
			} enumeration;
		struct
			{
			DNSQuestion q;
			DNSQuestion q2;
			} queryrecord;
		struct
			{
			DNSQuestion qtxt;
			DNSQuestion qsrv;
			const ResourceRecord *txt;
			const ResourceRecord *srv;
			mDNSs32 ReportTime;
			} resolve;
		} u;
	};

// struct physically sits between ipc message header and call-specific fields in the message buffer
typedef struct
	{
	DNSServiceFlags flags;			// Note: This field is in NETWORK byte order
	mDNSu32 ifi;					// Note: This field is in NETWORK byte order
	DNSServiceErrorType error;		// Note: This field is in NETWORK byte order
	} reply_hdr;

typedef struct reply_state
	{
	struct reply_state *next;		// If there are multiple unsent replies
	mDNSu32 totallen;
	mDNSu32 nwriten;
	ipc_msg_hdr mhdr[1];
	reply_hdr rhdr[1];
	} reply_state;

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Globals
#endif

// globals
mDNSexport mDNS mDNSStorage;
mDNSexport const char ProgramName[] = "mDNSResponder";

static dnssd_sock_t listenfd = dnssd_InvalidSocket;
static request_state *all_requests = NULL;

static DNameListElem *SCPrefBrowseDomains;			// List of automatic browsing domains read from SCPreferences for "empty string" browsing
static ARListElem    *LocalDomainEnumRecords;		// List of locally-generated PTR records to augment those we learn from the network
mDNSexport DNameListElem *AutoBrowseDomains;		// List created from those local-only PTR records plus records we get from the network

mDNSexport DNameListElem *AutoRegistrationDomains;	// Domains where we automatically register for empty-string registrations

#define MSG_PAD_BYTES 5		// pad message buffer (read from client) with n zero'd bytes to guarantee
							// n get_string() calls w/o buffer overrun
// initialization, setup/teardown functions

// If a platform specifies its own PID file name, we use that
#ifndef PID_FILE
#define PID_FILE "/var/run/mDNSResponder.pid"
#endif

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - General Utility Functions
#endif

mDNSlocal void FatalError(char *errmsg)
	{
	LogMsg("%s: %s", errmsg, dnssd_strerror(dnssd_errno));
	*(long*)0 = 0;	// On OS X abort() doesn't generate a crash log, but writing to zero does
	abort();		// On platforms where writing to zero doesn't generate an exception, abort instead
	}

mDNSlocal mDNSu32 dnssd_htonl(mDNSu32 l)
	{
	mDNSu32 ret;
	char *data = (char*) &ret;
	put_uint32(l, &data);
	return ret;
	}

// hack to search-replace perror's to LogMsg's
mDNSlocal void my_perror(char *errmsg)
	{
	LogMsg("%s: %d (%s)", errmsg, dnssd_errno, dnssd_strerror(dnssd_errno));
	}

mDNSlocal void abort_request(request_state *req)
	{
	if (req->terminate == (req_termination_fn)~0)
		{ LogMsg("abort_request: ERROR: Attempt to abort operation %p with req->terminate %p", req, req->terminate); return; }
	
	// First stop whatever mDNSCore operation we were doing
	if (req->terminate) req->terminate(req);

	if (!dnssd_SocketValid(req->sd))
		{ LogMsg("abort_request: ERROR: Attempt to abort operation %p with invalid fd %d",     req, req->sd);        return; }
	
	// Now, if this request_state is not subordinate to some other primary, close file descriptor and discard replies
	if (!req->primary)
		{
		if (req->errsd != req->sd) LogOperation("%3d: Removing FD and closing errsd %d", req->sd, req->errsd);
		else                       LogOperation("%3d: Removing FD", req->sd);
		udsSupportRemoveFDFromEventLoop(req->sd);		// Note: This also closes file descriptor req->sd for us
		if (req->errsd != req->sd) { dnssd_close(req->errsd); req->errsd = req->sd; }

		while (req->replies)	// free pending replies
			{
			reply_state *ptr = req->replies;
			req->replies = req->replies->next;
			freeL("reply_state (abort)", ptr);
			}
		}

	// Set req->sd to something invalid, so that udsserver_idle knows to unlink and free this structure
#if APPLE_OSX_mDNSResponder && MACOSX_MDNS_MALLOC_DEBUGGING
	// Don't use dnssd_InvalidSocket (-1) because that's the sentinel value MACOSX_MDNS_MALLOC_DEBUGGING uses
	// for detecting when the memory for an object is inadvertently freed while the object is still on some list
	req->sd = req->errsd = -2;
#else
	req->sd = req->errsd = dnssd_InvalidSocket;
#endif
	// We also set req->terminate to a bogus value so we know if abort_request() gets called again for this request
	req->terminate = (req_termination_fn)~0;
	}

mDNSlocal void AbortUnlinkAndFree(request_state *req)
	{
	request_state **p = &all_requests;
	abort_request(req);
	while (*p && *p != req) p=&(*p)->next;
	if (*p) { *p = req->next; freeL("request_state/AbortUnlinkAndFree", req); }
	else LogMsg("AbortUnlinkAndFree: ERROR: Attempt to abort operation %p not in list", req);
	}

mDNSlocal reply_state *create_reply(const reply_op_t op, const size_t datalen, request_state *const request)
	{
	reply_state *reply;

	if ((unsigned)datalen < sizeof(reply_hdr))
		{
		LogMsg("ERROR: create_reply - data length less than length of required fields");
		return NULL;
		}

	reply = mallocL("reply_state", sizeof(reply_state) + datalen - sizeof(reply_hdr));
	if (!reply) FatalError("ERROR: malloc");
	
	reply->next     = mDNSNULL;
	reply->totallen = datalen + sizeof(ipc_msg_hdr);
	reply->nwriten  = 0;

	reply->mhdr->version        = VERSION;
	reply->mhdr->datalen        = datalen;
	reply->mhdr->ipc_flags      = 0;
	reply->mhdr->op             = op;
	reply->mhdr->client_context = request->hdr.client_context;
	reply->mhdr->reg_index      = 0;

	return reply;
	}

// Append a reply to the list in a request object
// If our request is sharing a connection, then we append our reply_state onto the primary's list
mDNSlocal void append_reply(request_state *req, reply_state *rep)
	{
	request_state *r = req->primary ? req->primary : req;
	reply_state **ptr = &r->replies;
	while (*ptr) ptr = &(*ptr)->next;
	*ptr = rep;
	rep->next = NULL;
	}

// Generates a response message giving name, type, domain, plus interface index,
// suitable for a browse result or service registration result.
// On successful completion rep is set to point to a malloc'd reply_state struct
mDNSlocal mStatus GenerateNTDResponse(const domainname *const servicename, const mDNSInterfaceID id,
	request_state *const request, reply_state **const rep, reply_op_t op, DNSServiceFlags flags, mStatus err)
	{
	domainlabel name;
	domainname type, dom;
	*rep = NULL;
	if (!DeconstructServiceName(servicename, &name, &type, &dom))
		return kDNSServiceErr_Invalid;
	else
		{
		char namestr[MAX_DOMAIN_LABEL+1];
		char typestr[MAX_ESCAPED_DOMAIN_NAME];
		char domstr [MAX_ESCAPED_DOMAIN_NAME];
		int len;
		char *data;

		ConvertDomainLabelToCString_unescaped(&name, namestr);
		ConvertDomainNameToCString(&type, typestr);
		ConvertDomainNameToCString(&dom, domstr);

		// Calculate reply data length
		len = sizeof(DNSServiceFlags);
		len += sizeof(mDNSu32);  // if index
		len += sizeof(DNSServiceErrorType);
		len += (int) (strlen(namestr) + 1);
		len += (int) (strlen(typestr) + 1);
		len += (int) (strlen(domstr) + 1);

		// Build reply header
		*rep = create_reply(op, len, request);
		(*rep)->rhdr->flags = dnssd_htonl(flags);
		(*rep)->rhdr->ifi   = dnssd_htonl(mDNSPlatformInterfaceIndexfromInterfaceID(&mDNSStorage, id));
		(*rep)->rhdr->error = dnssd_htonl(err);

		// Build reply body
		data = (char *)&(*rep)->rhdr[1];
		put_string(namestr, &data);
		put_string(typestr, &data);
		put_string(domstr, &data);

		return mStatus_NoError;
		}
	}

// Special support to enable the DNSServiceBrowse call made by Bonjour Browser
// Remove after Bonjour Browser is updated to use DNSServiceQueryRecord instead of DNSServiceBrowse
mDNSlocal void GenerateBonjourBrowserResponse(const domainname *const servicename, const mDNSInterfaceID id,
	request_state *const request, reply_state **const rep, reply_op_t op, DNSServiceFlags flags, mStatus err)
	{
	char namestr[MAX_DOMAIN_LABEL+1];
	char typestr[MAX_ESCAPED_DOMAIN_NAME];
	static const char domstr[] = ".";
	int len;
	char *data;

	*rep = NULL;

	// 1. Put first label in namestr
	ConvertDomainLabelToCString_unescaped((const domainlabel *)servicename, namestr);

	// 2. Put second label and "local" into typestr
	mDNS_snprintf(typestr, sizeof(typestr), "%#s.local.", SecondLabel(servicename));

	// Calculate reply data length
	len = sizeof(DNSServiceFlags);
	len += sizeof(mDNSu32);  // if index
	len += sizeof(DNSServiceErrorType);
	len += (int) (strlen(namestr) + 1);
	len += (int) (strlen(typestr) + 1);
	len += (int) (strlen(domstr) + 1);

	// Build reply header
	*rep = create_reply(op, len, request);
	(*rep)->rhdr->flags = dnssd_htonl(flags);
	(*rep)->rhdr->ifi   = dnssd_htonl(mDNSPlatformInterfaceIndexfromInterfaceID(&mDNSStorage, id));
	(*rep)->rhdr->error = dnssd_htonl(err);

	// Build reply body
	data = (char *)&(*rep)->rhdr[1];
	put_string(namestr, &data);
	put_string(typestr, &data);
	put_string(domstr, &data);
	}

// Returns a resource record (allocated w/ malloc) containing the data found in an IPC message
// Data must be in the following format: flags, interfaceIndex, name, rrtype, rrclass, rdlen, rdata, (optional) ttl
// (ttl only extracted/set if ttl argument is non-zero). Returns NULL for a bad-parameter error
mDNSlocal AuthRecord *read_rr_from_ipc_msg(request_state *request, int GetTTL, int validate_flags)
	{
	DNSServiceFlags flags  = get_flags(&request->msgptr, request->msgend);
	mDNSu32 interfaceIndex = get_uint32(&request->msgptr, request->msgend);
	char name[256];
	int         str_err = get_string(&request->msgptr, request->msgend, name, sizeof(name));
	mDNSu16     type    = get_uint16(&request->msgptr, request->msgend);
	mDNSu16     class   = get_uint16(&request->msgptr, request->msgend);
	mDNSu16     rdlen   = get_uint16(&request->msgptr, request->msgend);
	const char *rdata   = get_rdata (&request->msgptr, request->msgend, rdlen);
	mDNSu32 ttl   = GetTTL ? get_uint32(&request->msgptr, request->msgend) : 0;
	int storage_size = rdlen > sizeof(RDataBody) ? rdlen : sizeof(RDataBody);
	AuthRecord *rr;

	if (str_err) { LogMsg("ERROR: read_rr_from_ipc_msg - get_string"); return NULL; }

	if (!request->msgptr) { LogMsg("Error reading Resource Record from client"); return NULL; }

	if (validate_flags &&
		!((flags & kDNSServiceFlagsShared) == kDNSServiceFlagsShared) &&
		!((flags & kDNSServiceFlagsUnique) == kDNSServiceFlagsUnique))
		{
		LogMsg("ERROR: Bad resource record flags (must be kDNSServiceFlagsShared or kDNSServiceFlagsUnique)");
		return NULL;
		}

	rr = mallocL("AuthRecord/read_rr_from_ipc_msg", sizeof(AuthRecord) - sizeof(RDataBody) + storage_size);
	if (!rr) FatalError("ERROR: malloc");
	mDNS_SetupResourceRecord(rr, mDNSNULL, mDNSPlatformInterfaceIDfromInterfaceIndex(&mDNSStorage, interfaceIndex),
		type, 0, (mDNSu8) ((flags & kDNSServiceFlagsShared) ? kDNSRecordTypeShared : kDNSRecordTypeUnique), mDNSNULL, mDNSNULL);

	if (!MakeDomainNameFromDNSNameString(&rr->namestorage, name))
		{
		LogMsg("ERROR: bad name: %s", name);
		freeL("AuthRecord/read_rr_from_ipc_msg", rr);
		return NULL;
		}

	if (flags & kDNSServiceFlagsAllowRemoteQuery) rr->AllowRemoteQuery = mDNStrue;
	rr->resrec.rrclass = class;
	rr->resrec.rdlength = rdlen;
	rr->resrec.rdata->MaxRDLength = rdlen;
	mDNSPlatformMemCopy(rr->resrec.rdata->u.data, rdata, rdlen);
	if (GetTTL) rr->resrec.rroriginalttl = ttl;
	rr->resrec.namehash = DomainNameHashValue(rr->resrec.name);
	SetNewRData(&rr->resrec, mDNSNULL, 0);	// Sets rr->rdatahash for us
	return rr;
	}

mDNSlocal int build_domainname_from_strings(domainname *srv, char *name, char *regtype, char *domain)
	{
	domainlabel n;
	domainname d, t;

	if (!MakeDomainLabelFromLiteralString(&n, name)) return -1;
	if (!MakeDomainNameFromDNSNameString(&t, regtype)) return -1;
	if (!MakeDomainNameFromDNSNameString(&d, domain)) return -1;
	if (!ConstructServiceName(srv, &n, &t, &d)) return -1;
	return 0;
	}

mDNSlocal void send_all(dnssd_sock_t s, const char *ptr, int len)
	{
	int n = send(s, ptr, len, 0);
	// On a freshly-created Unix Domain Socket, the kernel should *never* fail to buffer a small write for us
	// (four bytes for a typical error code return, 12 bytes for DNSServiceGetProperty(DaemonVersion)).
	// If it does fail, we don't attempt to handle this failure, but we do log it so we know something is wrong.
	if (n < len)
		LogMsg("ERROR: send_all(%d) wrote %d of %d errno %d (%s)",
			s, n, len, dnssd_errno, dnssd_strerror(dnssd_errno));
	}

#if 0
mDNSlocal mDNSBool AuthorizedDomain(const request_state * const request, const domainname * const d, const DNameListElem * const doms)
{
	const 		DNameListElem 	*delem = mDNSNULL;
	int 		bestDelta 	= -1; 					// the delta of the best match, lower is better
	int 		dLabels 	= 0;
	mDNSBool	allow 		= mDNSfalse;
	
	if (SystemUID(request->uid)) return mDNStrue;
	
	dLabels = CountLabels(d);
	for (delem = doms; delem; delem = delem->next)
		{
		if (delem->uid)
			{
			int	delemLabels = CountLabels(&delem->name);
			int delta 		= dLabels - delemLabels;
			if ((bestDelta == -1 || delta <= bestDelta) && SameDomainName(&delem->name, SkipLeadingLabels(d, delta)))
				{
				bestDelta = delta;
				allow = (allow || (delem->uid == request->uid));
				}
			}
		}
	
	return bestDelta == -1 ? mDNStrue : allow;
}
#endif

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - DNSServiceRegister
#endif

mDNSexport void FreeExtraRR(mDNS *const m, AuthRecord *const rr, mStatus result)
	{
	ExtraResourceRecord *extra = (ExtraResourceRecord *)rr->RecordContext;
	(void)m;  // Unused

	if (result != mStatus_MemFree) { LogMsg("Error: FreeExtraRR invoked with unexpected error %d", result); return; }

	LogInfo("     FreeExtraRR %s", RRDisplayString(m, &rr->resrec));

	if (rr->resrec.rdata != &rr->rdatastorage)
		freeL("Extra RData", rr->resrec.rdata);
	freeL("ExtraResourceRecord/FreeExtraRR", extra);
	}

mDNSlocal void unlink_and_free_service_instance(service_instance *srv)
	{
	ExtraResourceRecord *e = srv->srs.Extras, *tmp;

	// clear pointers from parent struct
	if (srv->request)
		{
		service_instance **p = &srv->request->u.servicereg.instances;
		while (*p)
			{
			if (*p == srv) { *p = (*p)->next; break; }
			p = &(*p)->next;
			}
		}

	while (e)
		{
		e->r.RecordContext = e;
		tmp = e;
		e = e->next;
		FreeExtraRR(&mDNSStorage, &tmp->r, mStatus_MemFree);
		}

	if (srv->srs.RR_TXT.resrec.rdata != &srv->srs.RR_TXT.rdatastorage)
		freeL("TXT RData", srv->srs.RR_TXT.resrec.rdata);

	if (srv->subtypes) { freeL("ServiceSubTypes", srv->subtypes); srv->subtypes = NULL; }
	freeL("service_instance", srv);
	}

// Count how many other service records we have locally with the same name, but different rdata.
// For auto-named services, we can have at most one per machine -- if we allowed two auto-named services of
// the same type on the same machine, we'd get into an infinite autoimmune-response loop of continuous renaming.
mDNSexport int CountPeerRegistrations(mDNS *const m, ServiceRecordSet *const srs)
	{
	int count = 0;
	ResourceRecord *r = &srs->RR_SRV.resrec;
	AuthRecord *rr;
	ServiceRecordSet *s;

	for (rr = m->ResourceRecords; rr; rr=rr->next)
		if (rr->resrec.rrtype == kDNSType_SRV && SameDomainName(rr->resrec.name, r->name) && !IdenticalSameNameRecord(&rr->resrec, r))
			count++;

	for (s = m->ServiceRegistrations; s; s = s->uDNS_next)
		if (s->state != regState_Unregistered && SameDomainName(s->RR_SRV.resrec.name, r->name) && !IdenticalSameNameRecord(&s->RR_SRV.resrec, r))
			count++;

	verbosedebugf("%d peer registrations for %##s", count, r->name->c);
	return(count);
	}

mDNSexport int CountExistingRegistrations(domainname *srv, mDNSIPPort port)
	{
	int count = 0;
	AuthRecord *rr;
	for (rr = mDNSStorage.ResourceRecords; rr; rr=rr->next)
		if (rr->resrec.rrtype == kDNSType_SRV &&
			mDNSSameIPPort(rr->resrec.rdata->u.srv.port, port) &&
			SameDomainName(rr->resrec.name, srv))
			count++;
	return(count);
	}

mDNSlocal void SendServiceRemovalNotification(ServiceRecordSet *const srs)
	{
	reply_state *rep;
	service_instance *instance = srs->ServiceContext;
	if (GenerateNTDResponse(srs->RR_SRV.resrec.name, srs->RR_SRV.resrec.InterfaceID, instance->request, &rep, reg_service_reply_op, 0, mStatus_NoError) != mStatus_NoError)
		LogMsg("%3d: SendServiceRemovalNotification: %##s is not valid DNS-SD SRV name", instance->request->sd, srs->RR_SRV.resrec.name->c);
	else { append_reply(instance->request, rep); instance->clientnotified = mDNSfalse; }
	}

// service registration callback performs three duties - frees memory for deregistered services,
// handles name conflicts, and delivers completed registration information to the client
mDNSlocal void regservice_callback(mDNS *const m, ServiceRecordSet *const srs, mStatus result)
	{
	mStatus err;
	mDNSBool SuppressError = mDNSfalse;
	service_instance *instance = srs->ServiceContext;
	reply_state         *rep;
	char *fmt = "";
	if (mDNS_LoggingEnabled)
		fmt = (result == mStatus_NoError)      ? "%3d: DNSServiceRegister(%##s, %u) REGISTERED"    :
			  (result == mStatus_MemFree)      ? "%3d: DNSServiceRegister(%##s, %u) DEREGISTERED"  :
			  (result == mStatus_NameConflict) ? "%3d: DNSServiceRegister(%##s, %u) NAME CONFLICT" :
			                                     "%3d: DNSServiceRegister(%##s, %u) %s %d";
	(void)m; // Unused
	if (!srs)      { LogMsg("regservice_callback: srs is NULL %d",                 result); return; }
	if (!instance) { LogMsg("regservice_callback: srs->ServiceContext is NULL %d", result); return; }

	// don't send errors up to client for wide-area, empty-string registrations
	if (instance->request &&
		instance->request->u.servicereg.default_domain &&
		!instance->default_local)
		SuppressError = mDNStrue;

	LogOperation(fmt, instance->request ? instance->request->sd : -99,
		srs->RR_SRV.resrec.name->c, mDNSVal16(srs->RR_SRV.resrec.rdata->u.srv.port), SuppressError ? "suppressed error" : "CALLBACK", result);

	if (!instance->request && result != mStatus_MemFree) { LogMsg("regservice_callback: instance->request is NULL %d", result); return; }

	if (result == mStatus_NoError)
		{
		if (instance->request->u.servicereg.allowremotequery)
			{
			ExtraResourceRecord *e;
			srs->RR_ADV.AllowRemoteQuery = mDNStrue;
			srs->RR_PTR.AllowRemoteQuery = mDNStrue;
			srs->RR_SRV.AllowRemoteQuery = mDNStrue;
			srs->RR_TXT.AllowRemoteQuery = mDNStrue;
			for (e = instance->srs.Extras; e; e = e->next) e->r.AllowRemoteQuery = mDNStrue;
			}

		if (GenerateNTDResponse(srs->RR_SRV.resrec.name, srs->RR_SRV.resrec.InterfaceID, instance->request, &rep, reg_service_reply_op, kDNSServiceFlagsAdd, result) != mStatus_NoError)
			LogMsg("%3d: regservice_callback: %##s is not valid DNS-SD SRV name", instance->request->sd, srs->RR_SRV.resrec.name->c);
		else { append_reply(instance->request, rep); instance->clientnotified = mDNStrue; }

		if (instance->request->u.servicereg.autoname && CountPeerRegistrations(m, srs) == 0)
			RecordUpdatedNiceLabel(m, 0);	// Successfully got new name, tell user immediately
		}
	else if (result == mStatus_MemFree)
		{
		if (instance->request && instance->renameonmemfree)
			{
			instance->renameonmemfree = 0;
			err = mDNS_RenameAndReregisterService(m, srs, &instance->request->u.servicereg.name);
			if (err) LogMsg("ERROR: regservice_callback - RenameAndReregisterService returned %d", err);
			// error should never happen - safest to log and continue
			}
		else
			unlink_and_free_service_instance(instance);
		}
	else if (result == mStatus_NameConflict)
		{
		if (instance->request->u.servicereg.autorename)
			{
			if (instance->request->u.servicereg.autoname && CountPeerRegistrations(m, srs) == 0)
				{
				// On conflict for an autoname service, rename and reregister *all* autoname services
				IncrementLabelSuffix(&m->nicelabel, mDNStrue);
				mDNS_ConfigChanged(m);	// Will call back into udsserver_handle_configchange()
				}
			else	// On conflict for a non-autoname service, rename and reregister just that one service
				{
				if (instance->clientnotified) SendServiceRemovalNotification(srs);
				mDNS_RenameAndReregisterService(m, srs, mDNSNULL);
				}
			}
		else
			{
			if (!SuppressError) 
				{
				if (GenerateNTDResponse(srs->RR_SRV.resrec.name, srs->RR_SRV.resrec.InterfaceID, instance->request, &rep, reg_service_reply_op, kDNSServiceFlagsAdd, result) != mStatus_NoError)
					LogMsg("%3d: regservice_callback: %##s is not valid DNS-SD SRV name", instance->request->sd, srs->RR_SRV.resrec.name->c);
				else { append_reply(instance->request, rep); instance->clientnotified = mDNStrue; }
				}
			unlink_and_free_service_instance(instance);
			}
		}
	else
		{
		if (!SuppressError) 
			{
			if (GenerateNTDResponse(srs->RR_SRV.resrec.name, srs->RR_SRV.resrec.InterfaceID, instance->request, &rep, reg_service_reply_op, kDNSServiceFlagsAdd, result) != mStatus_NoError)
				LogMsg("%3d: regservice_callback: %##s is not valid DNS-SD SRV name", instance->request->sd, srs->RR_SRV.resrec.name->c);
			else { append_reply(instance->request, rep); instance->clientnotified = mDNStrue; }
			}
		}
	}

mDNSlocal void regrecord_callback(mDNS *const m, AuthRecord *rr, mStatus result)
	{
	(void)m; // Unused
	if (!rr->RecordContext)		// parent struct already freed by termination callback
		{
		if (result == mStatus_NoError)
			LogMsg("Error: regrecord_callback: successful registration of orphaned record");
		else
			{
			if (result != mStatus_MemFree) LogMsg("regrecord_callback: error %d received after parent termination", result);
			freeL("AuthRecord/regrecord_callback", rr);
			}
		}
	else
		{
		registered_record_entry *re = rr->RecordContext;
		request_state *request = re->request;
		int len = sizeof(DNSServiceFlags) + sizeof(mDNSu32) + sizeof(DNSServiceErrorType);
		reply_state *reply = create_reply(reg_record_reply_op, len, request);
		reply->mhdr->client_context = re->regrec_client_context;
		reply->rhdr->flags = dnssd_htonl(0);
		reply->rhdr->ifi   = dnssd_htonl(mDNSPlatformInterfaceIndexfromInterfaceID(m, rr->resrec.InterfaceID));
		reply->rhdr->error = dnssd_htonl(result);

		LogOperation("%3d: DNSServiceRegisterRecord(%u) result %d", request->sd, request->hdr.reg_index, result);
		if (result)
			{
			// unlink from list, free memory
			registered_record_entry **ptr = &request->u.reg_recs;
			while (*ptr && (*ptr) != re) ptr = &(*ptr)->next;
			if (!*ptr) { LogMsg("regrecord_callback - record not in list!"); return; }
			*ptr = (*ptr)->next;
			freeL("registered_record_entry AuthRecord regrecord_callback", re->rr);
			freeL("registered_record_entry regrecord_callback", re);
			}
		append_reply(request, reply);
		}
	}

mDNSlocal void connection_termination(request_state *request)
	{
	request_state **req = &all_requests;
	while (*req)
		{
		if ((*req)->primary == request)
			{
			// Since we're already doing a list traversal, we unlink the request directly instead of using AbortUnlinkAndFree()
			request_state *tmp = *req;
			if (tmp->primary == tmp) LogMsg("connection_termination ERROR (*req)->primary == *req for %p %d",                  tmp, tmp->sd);
			if (tmp->replies)        LogMsg("connection_termination ERROR How can subordinate req %p %d have replies queued?", tmp, tmp->sd);
			abort_request(tmp);
			*req = tmp->next;
			freeL("request_state/connection_termination", tmp);
			}
		else
			req = &(*req)->next;
		}

	while (request->u.reg_recs)
		{
		registered_record_entry *ptr = request->u.reg_recs;
		request->u.reg_recs = request->u.reg_recs->next;
		ptr->rr->RecordContext = NULL;
		mDNS_Deregister(&mDNSStorage, ptr->rr);		// Will free ptr->rr for us
		freeL("registered_record_entry/connection_termination", ptr);
		}
	}

mDNSlocal void handle_cancel_request(request_state *request)
	{
	request_state **req = &all_requests;
	LogOperation("%3d: Cancel %08X %08X", request->sd, request->hdr.client_context.u32[1], request->hdr.client_context.u32[0]);
	while (*req)
		{
		if ((*req)->primary == request &&
			(*req)->hdr.client_context.u32[0] == request->hdr.client_context.u32[0] &&
			(*req)->hdr.client_context.u32[1] == request->hdr.client_context.u32[1])
			{
			// Since we're already doing a list traversal, we unlink the request directly instead of using AbortUnlinkAndFree()
			request_state *tmp = *req;
			abort_request(tmp);
			*req = tmp->next;
			freeL("request_state/handle_cancel_request", tmp);
			}
		else
			req = &(*req)->next;
		}
	}

mDNSlocal mStatus handle_regrecord_request(request_state *request)
	{
	mStatus err = mStatus_BadParamErr;
	AuthRecord *rr = read_rr_from_ipc_msg(request, 1, 1);
	if (rr)
		{
		// allocate registration entry, link into list
		registered_record_entry *re = mallocL("registered_record_entry", sizeof(registered_record_entry));
		if (!re) FatalError("ERROR: malloc");
		re->key = request->hdr.reg_index;
		re->rr = rr;
		re->request = request;
		re->regrec_client_context = request->hdr.client_context;
		rr->RecordContext = re;
		rr->RecordCallback = regrecord_callback;
		re->next = request->u.reg_recs;
		request->u.reg_recs = re;
	
#if 0
		if (!AuthorizedDomain(request, rr->resrec.name, AutoRegistrationDomains))	return (mStatus_NoError);
#endif
		if (rr->resrec.rroriginalttl == 0)
			rr->resrec.rroriginalttl = DefaultTTLforRRType(rr->resrec.rrtype);
	
		LogOperation("%3d: DNSServiceRegisterRecord(%u %s)", request->sd, request->hdr.reg_index, RRDisplayString(&mDNSStorage, &rr->resrec));
		err = mDNS_Register(&mDNSStorage, rr);
		}
	return(err);
	}

mDNSlocal void UpdateDeviceInfoRecord(mDNS *const m);

mDNSlocal void regservice_termination_callback(request_state *request)
	{
	if (!request) { LogMsg("regservice_termination_callback context is NULL"); return; }
	while (request->u.servicereg.instances)
		{
		service_instance *p = request->u.servicereg.instances;
		request->u.servicereg.instances = request->u.servicereg.instances->next;
		// only safe to free memory if registration is not valid, i.e. deregister fails (which invalidates p)
		LogOperation("%3d: DNSServiceRegister(%##s, %u) STOP",
			request->sd, p->srs.RR_SRV.resrec.name->c, mDNSVal16(p->srs.RR_SRV.resrec.rdata->u.srv.port));

		// Clear backpointer *before* calling mDNS_DeregisterService/unlink_and_free_service_instance
		// We don't need unlink_and_free_service_instance to cut its element from the list, because we're already advancing
		// request->u.servicereg.instances as we work our way through the list, implicitly cutting one element at a time
		// We can't clear p->request *after* the calling mDNS_DeregisterService/unlink_and_free_service_instance
		// because by then we might have already freed p
		p->request = NULL;
		if (mDNS_DeregisterService(&mDNSStorage, &p->srs)) unlink_and_free_service_instance(p);
		// Don't touch service_instance *p after this -- it's likely to have been freed already
		}
	if (request->u.servicereg.txtdata)
		{ freeL("service_info txtdata", request->u.servicereg.txtdata); request->u.servicereg.txtdata = NULL; }
	if (request->u.servicereg.autoname)
		{
		// Clear autoname before calling UpdateDeviceInfoRecord() so it doesn't mistakenly include this in its count of active autoname registrations
		request->u.servicereg.autoname = mDNSfalse;
		UpdateDeviceInfoRecord(&mDNSStorage);
		}
	}

mDNSlocal request_state *LocateSubordinateRequest(request_state *request)
	{
	request_state *req;
	for (req = all_requests; req; req = req->next)
		if (req->primary == request &&
			req->hdr.client_context.u32[0] == request->hdr.client_context.u32[0] &&
			req->hdr.client_context.u32[1] == request->hdr.client_context.u32[1]) return(req);
	return(request);
	}

mDNSlocal mStatus add_record_to_service(request_state *request, service_instance *instance, mDNSu16 rrtype, mDNSu16 rdlen, const char *rdata, mDNSu32 ttl)
	{
	ServiceRecordSet *srs = &instance->srs;
	mStatus result;
	int size = rdlen > sizeof(RDataBody) ? rdlen : sizeof(RDataBody);
	ExtraResourceRecord *extra = mallocL("ExtraResourceRecord", sizeof(*extra) - sizeof(RDataBody) + size);
	if (!extra) { my_perror("ERROR: malloc"); return mStatus_NoMemoryErr; }

	mDNSPlatformMemZero(extra, sizeof(ExtraResourceRecord));  // OK if oversized rdata not zero'd
	extra->r.resrec.rrtype = rrtype;
	extra->r.rdatastorage.MaxRDLength = (mDNSu16) size;
	extra->r.resrec.rdlength = rdlen;
	mDNSPlatformMemCopy(&extra->r.rdatastorage.u.data, rdata, rdlen);

	result = mDNS_AddRecordToService(&mDNSStorage, srs, extra, &extra->r.rdatastorage, ttl);
	if (result) { freeL("ExtraResourceRecord/add_record_to_service", extra); return result; }

	extra->ClientID = request->hdr.reg_index;
	return result;
	}

mDNSlocal mStatus handle_add_request(request_state *request)
	{
	service_instance *i;
	mStatus result = mStatus_UnknownErr;
	DNSServiceFlags flags  = get_flags (&request->msgptr, request->msgend);
	mDNSu16         rrtype = get_uint16(&request->msgptr, request->msgend);
	mDNSu16         rdlen  = get_uint16(&request->msgptr, request->msgend);
	const char     *rdata  = get_rdata (&request->msgptr, request->msgend, rdlen);
	mDNSu32         ttl    = get_uint32(&request->msgptr, request->msgend);
	if (!ttl) ttl = DefaultTTLforRRType(rrtype);
	(void)flags; // Unused

	if (!request->msgptr) { LogMsg("%3d: DNSServiceAddRecord(unreadable parameters)", request->sd); return(mStatus_BadParamErr); }

	// If this is a shared connection, check if the operation actually applies to a subordinate request_state object
	if (request->terminate == connection_termination) request = LocateSubordinateRequest(request);

	if (request->terminate != regservice_termination_callback)
		{ LogMsg("%3d: DNSServiceAddRecord(not a registered service ref)", request->sd); return(mStatus_BadParamErr); }

	LogOperation("%3d: DNSServiceAddRecord(%##s, %s, %d)", request->sd,
		(request->u.servicereg.instances) ? request->u.servicereg.instances->srs.RR_SRV.resrec.name->c : NULL, DNSTypeName(rrtype), rdlen);

	for (i = request->u.servicereg.instances; i; i = i->next)
		{
		result = add_record_to_service(request, i, rrtype, rdlen, rdata, ttl);
		if (result && i->default_local) break;
		else result = mStatus_NoError;  // suppress non-local default errors
		}

	return(result);
	}

mDNSlocal void update_callback(mDNS *const m, AuthRecord *const rr, RData *oldrd)
	{
	(void)m; // Unused
	if (oldrd != &rr->rdatastorage) freeL("RData/update_callback", oldrd);
	}

mDNSlocal mStatus update_record(AuthRecord *rr, mDNSu16 rdlen, const char *rdata, mDNSu32 ttl)
	{
	int rdsize;
	RData *newrd;
	mStatus result;

	if (rdlen > sizeof(RDataBody)) rdsize = rdlen;
	else rdsize = sizeof(RDataBody);
	newrd = mallocL("RData/update_record", sizeof(RData) - sizeof(RDataBody) + rdsize);
	if (!newrd) FatalError("ERROR: malloc");
	newrd->MaxRDLength = (mDNSu16) rdsize;
	mDNSPlatformMemCopy(&newrd->u, rdata, rdlen);

	// BIND named (name daemon) doesn't allow TXT records with zero-length rdata. This is strictly speaking correct,
	// since RFC 1035 specifies a TXT record as "One or more <character-string>s", not "Zero or more <character-string>s".
	// Since some legacy apps try to create zero-length TXT records, we'll silently correct it here.
	if (rr->resrec.rrtype == kDNSType_TXT && rdlen == 0) { rdlen = 1; newrd->u.txt.c[0] = 0; }

	result = mDNS_Update(&mDNSStorage, rr, ttl, rdlen, newrd, update_callback);
	if (result) { LogMsg("ERROR: mDNS_Update - %d", result); freeL("RData/update_record", newrd); }
	return result;
	}

mDNSlocal mStatus handle_update_request(request_state *request)
	{
	const ipc_msg_hdr *const hdr = &request->hdr;
	mStatus result = mStatus_BadReferenceErr;
	service_instance *i;
	AuthRecord *rr = NULL;

	// get the message data
	DNSServiceFlags flags = get_flags (&request->msgptr, request->msgend);	// flags unused
	mDNSu16         rdlen = get_uint16(&request->msgptr, request->msgend);
	const char     *rdata = get_rdata (&request->msgptr, request->msgend, rdlen);
	mDNSu32         ttl   = get_uint32(&request->msgptr, request->msgend);
	(void)flags; // Unused

	if (!request->msgptr) { LogMsg("%3d: DNSServiceUpdateRecord(unreadable parameters)", request->sd); return(mStatus_BadParamErr); }

	// If this is a shared connection, check if the operation actually applies to a subordinate request_state object
	if (request->terminate == connection_termination) request = LocateSubordinateRequest(request);

	if (request->terminate == connection_termination)
		{
		// update an individually registered record
		registered_record_entry *reptr;
		for (reptr = request->u.reg_recs; reptr; reptr = reptr->next)
			{
			if (reptr->key == hdr->reg_index)
				{
				result = update_record(reptr->rr, rdlen, rdata, ttl);
				goto end;
				}
			}
		result = mStatus_BadReferenceErr;
		goto end;
		}

	if (request->terminate != regservice_termination_callback)
		{ LogMsg("%3d: DNSServiceUpdateRecord(not a registered service ref)", request->sd); return(mStatus_BadParamErr); }

	// update the saved off TXT data for the service
	if (hdr->reg_index == TXT_RECORD_INDEX)
		{
		if (request->u.servicereg.txtdata)
			{ freeL("service_info txtdata", request->u.servicereg.txtdata); request->u.servicereg.txtdata = NULL; }
		if (rdlen > 0)
			{
			request->u.servicereg.txtdata = mallocL("service_info txtdata", rdlen);
			if (!request->u.servicereg.txtdata) FatalError("ERROR: handle_update_request - malloc");
			mDNSPlatformMemCopy(request->u.servicereg.txtdata, rdata, rdlen);
			}
		request->u.servicereg.txtlen = rdlen;
		}

	// update a record from a service record set
	for (i = request->u.servicereg.instances; i; i = i->next)
		{
		if (hdr->reg_index == TXT_RECORD_INDEX) rr = &i->srs.RR_TXT;
		else
			{
			ExtraResourceRecord *e;
			for (e = i->srs.Extras; e; e = e->next)
				if (e->ClientID == hdr->reg_index) { rr = &e->r; break; }
			}

		if (!rr) { result = mStatus_BadReferenceErr; goto end; }
		result = update_record(rr, rdlen, rdata, ttl);
		if (result && i->default_local) goto end;
		else result = mStatus_NoError;  // suppress non-local default errors
		}

end:
	if (request->terminate == regservice_termination_callback)
		LogOperation("%3d: DNSServiceUpdateRecord(%##s, %s)", request->sd,
			(request->u.servicereg.instances) ? request->u.servicereg.instances->srs.RR_SRV.resrec.name->c : NULL,
			rr ? DNSTypeName(rr->resrec.rrtype) : "<NONE>");

	return(result);
	}

// remove a resource record registered via DNSServiceRegisterRecord()
mDNSlocal mStatus remove_record(request_state *request)
	{
	mStatus err = mStatus_UnknownErr;
	registered_record_entry *e, **ptr = &request->u.reg_recs;

	while (*ptr && (*ptr)->key != request->hdr.reg_index) ptr = &(*ptr)->next;
	if (!*ptr) { LogMsg("%3d: DNSServiceRemoveRecord(%u) not found", request->sd, request->hdr.reg_index); return mStatus_BadReferenceErr; }
	e = *ptr;
	*ptr = e->next; // unlink

	LogOperation("%3d: DNSServiceRemoveRecord(%u %s)", request->sd, request->hdr.reg_index, RRDisplayString(&mDNSStorage, &e->rr->resrec));
	e->rr->RecordContext = NULL;
	err = mDNS_Deregister(&mDNSStorage, e->rr);
	if (err)
		{
		LogMsg("ERROR: remove_record, mDNS_Deregister: %d", err);
		freeL("registered_record_entry AuthRecord remove_record", e->rr);
		}
	freeL("registered_record_entry remove_record", e);
	return err;
	}

mDNSlocal mStatus remove_extra(const request_state *const request, service_instance *const serv, mDNSu16 *const rrtype)
	{
	mStatus err = mStatus_BadReferenceErr;
	ExtraResourceRecord *ptr;

	for (ptr = serv->srs.Extras; ptr; ptr = ptr->next)		
		{
		if (ptr->ClientID == request->hdr.reg_index) // found match
			{
			*rrtype = ptr->r.resrec.rrtype;
			return mDNS_RemoveRecordFromService(&mDNSStorage, &serv->srs, ptr, FreeExtraRR, ptr);
			}
		}
	return err;
	}

mDNSlocal mStatus handle_removerecord_request(request_state *request)
	{
	mStatus err = mStatus_BadReferenceErr;
	get_flags(&request->msgptr, request->msgend);	// flags unused

	if (!request->msgptr) { LogMsg("%3d: DNSServiceRemoveRecord(unreadable parameters)", request->sd); return(mStatus_BadParamErr); }

	// If this is a shared connection, check if the operation actually applies to a subordinate request_state object
	if (request->terminate == connection_termination) request = LocateSubordinateRequest(request);

	if (request->terminate == connection_termination)
		err = remove_record(request);  // remove individually registered record
	else if (request->terminate != regservice_termination_callback)
		{ LogMsg("%3d: DNSServiceRemoveRecord(not a registered service ref)", request->sd); return(mStatus_BadParamErr); }
	else
		{
		service_instance *i;
		mDNSu16 rrtype = 0;
		LogOperation("%3d: DNSServiceRemoveRecord(%##s, %s)", request->sd,
			(request->u.servicereg.instances) ? request->u.servicereg.instances->srs.RR_SRV.resrec.name->c : NULL,
			rrtype ? DNSTypeName(rrtype) : "<NONE>");
		for (i = request->u.servicereg.instances; i; i = i->next)
			{
			err = remove_extra(request, i, &rrtype);
			if (err && i->default_local) break;
			else err = mStatus_NoError;  // suppress non-local default errors
			}
		}

	return(err);
	}

// If there's a comma followed by another character,
// FindFirstSubType overwrites the comma with a nul and returns the pointer to the next character.
// Otherwise, it returns a pointer to the final nul at the end of the string
mDNSlocal char *FindFirstSubType(char *p)
	{
	while (*p)
		{
		if (p[0] == '\\' && p[1]) p += 2;
		else if (p[0] == ',' && p[1]) { *p++ = 0; return(p); }
		else p++;
		}
	return(p);
	}

// If there's a comma followed by another character,
// FindNextSubType overwrites the comma with a nul and returns the pointer to the next character.
// If it finds an illegal unescaped dot in the subtype name, it returns mDNSNULL
// Otherwise, it returns a pointer to the final nul at the end of the string
mDNSlocal char *FindNextSubType(char *p)
	{
	while (*p)
		{
		if (p[0] == '\\' && p[1])		// If escape character
			p += 2;						// ignore following character
		else if (p[0] == ',')			// If we found a comma
			{
			if (p[1]) *p++ = 0;
			return(p);
			}
		else if (p[0] == '.')
			return(mDNSNULL);
		else p++;
		}
	return(p);
	}

// Returns -1 if illegal subtype found
mDNSexport mDNSs32 ChopSubTypes(char *regtype)
	{
	mDNSs32 NumSubTypes = 0;
	char *stp = FindFirstSubType(regtype);
	while (stp && *stp)					// If we found a comma...
		{
		if (*stp == ',') return(-1);
		NumSubTypes++;
		stp = FindNextSubType(stp);
		}
	if (!stp) return(-1);
	return(NumSubTypes);
	}

mDNSexport AuthRecord *AllocateSubTypes(mDNSs32 NumSubTypes, char *p)
	{
	AuthRecord *st = mDNSNULL;
	if (NumSubTypes)
		{
		mDNSs32 i;
		st = mallocL("ServiceSubTypes", NumSubTypes * sizeof(AuthRecord));
		if (!st) return(mDNSNULL);
		for (i = 0; i < NumSubTypes; i++)
			{
			mDNS_SetupResourceRecord(&st[i], mDNSNULL, mDNSInterface_Any, kDNSQType_ANY, kStandardTTL, 0, mDNSNULL, mDNSNULL);
			while (*p) p++;
			p++;
			if (!MakeDomainNameFromDNSNameString(&st[i].namestorage, p))
				{ freeL("ServiceSubTypes", st); return(mDNSNULL); }
			}
		}
	return(st);
	}

mDNSlocal mStatus register_service_instance(request_state *request, const domainname *domain)
	{
	service_instance **ptr, *instance;
	int instance_size;
	mStatus result;

	for (ptr = &request->u.servicereg.instances; *ptr; ptr = &(*ptr)->next)
		{
		if (SameDomainName(&(*ptr)->domain, domain))
			{
			LogMsg("register_service_instance: domain %##s already registered for %#s.%##s",
				domain->c, &request->u.servicereg.name, &request->u.servicereg.type);
			return mStatus_AlreadyRegistered;
			}
		}

	// Special-case hack: We don't advertise SMB service in AutoTunnel domains, because AutoTunnel
	// services have to support IPv6, and our SMB server does not
	// <rdar://problem/5482322> BTMM: Don't advertise SMB with BTMM because it doesn't support IPv6
	if (SameDomainName(&request->u.servicereg.type, (const domainname *) "\x4" "_smb" "\x4" "_tcp"))
		{
		DomainAuthInfo *AuthInfo = GetAuthInfoForName(&mDNSStorage, domain);
		if (AuthInfo && AuthInfo->AutoTunnel) return(kDNSServiceErr_Unsupported);
		}

	instance_size = sizeof(*instance);
	if (request->u.servicereg.txtlen > sizeof(RDataBody)) instance_size += (request->u.servicereg.txtlen - sizeof(RDataBody));
	instance = mallocL("service_instance", instance_size);
	if (!instance) { my_perror("ERROR: malloc"); return mStatus_NoMemoryErr; }

	instance->next            = mDNSNULL;
	instance->request         = request;
	instance->subtypes        = AllocateSubTypes(request->u.servicereg.num_subtypes, request->u.servicereg.type_as_string);
	instance->renameonmemfree = 0;
	instance->clientnotified  = mDNSfalse;
	instance->default_local   = (request->u.servicereg.default_domain && SameDomainName(domain, &localdomain));
	AssignDomainName(&instance->domain, domain);

	if (request->u.servicereg.num_subtypes && !instance->subtypes)
		{ unlink_and_free_service_instance(instance); instance = NULL; FatalError("ERROR: malloc"); }

	result = mDNS_RegisterService(&mDNSStorage, &instance->srs,
		&request->u.servicereg.name, &request->u.servicereg.type, domain,
		request->u.servicereg.host.c[0] ? &request->u.servicereg.host : NULL,
		request->u.servicereg.port,
		request->u.servicereg.txtdata, request->u.servicereg.txtlen,
		instance->subtypes, request->u.servicereg.num_subtypes,
		request->u.servicereg.InterfaceID, regservice_callback, instance);

	if (!result)
		{
		*ptr = instance;		// Append this to the end of our request->u.servicereg.instances list
		LogOperation("%3d: DNSServiceRegister(%##s, %u) ADDED",
			instance->request->sd, instance->srs.RR_SRV.resrec.name->c, mDNSVal16(request->u.servicereg.port));
		}
	else
		{
		LogMsg("register_service_instance %#s.%##s%##s error %d",
			&request->u.servicereg.name, &request->u.servicereg.type, domain->c, result);
		unlink_and_free_service_instance(instance);
		}

	return result;
	}

mDNSlocal void udsserver_default_reg_domain_changed(const DNameListElem *const d, const mDNSBool add)
	{
	request_state *request;

#if APPLE_OSX_mDNSResponder
	machserver_automatic_registration_domain_changed(&d->name, add);
#endif // APPLE_OSX_mDNSResponder

	LogMsg("%s registration domain %##s", add ? "Adding" : "Removing", d->name.c);
	for (request = all_requests; request; request = request->next)
		{
		if (request->terminate != regservice_termination_callback) continue;
		if (!request->u.servicereg.default_domain) continue;
		if (!d->uid || SystemUID(request->uid) || request->uid == d->uid)
			{
			service_instance **ptr = &request->u.servicereg.instances;
			while (*ptr && !SameDomainName(&(*ptr)->domain, &d->name)) ptr = &(*ptr)->next;
			if (add)
				{
				// If we don't already have this domain in our list for this registration, add it now
				if (!*ptr) register_service_instance(request, &d->name);
				else debugf("udsserver_default_reg_domain_changed %##s already in list, not re-adding", &d->name);
				}
			else
				{
				// Normally we should not fail to find the specified instance
				// One case where this can happen is if a uDNS update fails for some reason,
				// and regservice_callback then calls unlink_and_free_service_instance and disposes of that instance.
				if (!*ptr)
					LogMsg("udsserver_default_reg_domain_changed domain %##s not found for service %#s type %s",
						&d->name, request->u.servicereg.name.c, request->u.servicereg.type_as_string);
				else
					{
					DNameListElem *p;
					for (p = AutoRegistrationDomains; p; p=p->next)
						if (!p->uid || SystemUID(request->uid) || request->uid == p->uid)
							if (SameDomainName(&d->name, &p->name)) break;
					if (p) debugf("udsserver_default_reg_domain_changed %##s still in list, not removing", &d->name);
					else
						{
						mStatus err;
						service_instance *si = *ptr;
						*ptr = si->next;
						if (si->clientnotified) SendServiceRemovalNotification(&si->srs); // Do this *before* clearing si->request backpointer
						// Now that we've cut this service_instance from the list, we MUST clear the si->request backpointer.
						// Otherwise what can happen is this: While our mDNS_DeregisterService is in the
						// process of completing asynchronously, the client cancels the entire operation, so
						// regservice_termination_callback then runs through the whole list deregistering each
						// instance, clearing the backpointers, and then disposing the parent request_state object.
						// However, because this service_instance isn't in the list any more, regservice_termination_callback
						// has no way to find it and clear its backpointer, and then when our mDNS_DeregisterService finally
						// completes later with a mStatus_MemFree message, it calls unlink_and_free_service_instance() with
						// a service_instance with a stale si->request backpointer pointing to memory that's already been freed.
						si->request = NULL;
						err = mDNS_DeregisterService(&mDNSStorage, &si->srs);
						if (err) { LogMsg("udsserver_default_reg_domain_changed err %d", err); unlink_and_free_service_instance(si); }
						}
					}
				}
			}
		}
	}

mDNSlocal mStatus handle_regservice_request(request_state *request)
	{
	char name[256];	// Lots of spare space for extra-long names that we'll auto-truncate down to 63 bytes
	char domain[MAX_ESCAPED_DOMAIN_NAME], host[MAX_ESCAPED_DOMAIN_NAME];
	char type_as_string[MAX_ESCAPED_DOMAIN_NAME];
	domainname d, srv;
	mStatus err;

	DNSServiceFlags flags = get_flags(&request->msgptr, request->msgend);
	mDNSu32 interfaceIndex = get_uint32(&request->msgptr, request->msgend);
	mDNSInterfaceID InterfaceID = mDNSPlatformInterfaceIDfromInterfaceIndex(&mDNSStorage, interfaceIndex);
	if (interfaceIndex && !InterfaceID)
		{ LogMsg("ERROR: handle_regservice_request - Couldn't find interfaceIndex %d", interfaceIndex); return(mStatus_BadParamErr); }

	if (get_string(&request->msgptr, request->msgend, name, sizeof(name)) < 0 ||
		get_string(&request->msgptr, request->msgend, type_as_string, MAX_ESCAPED_DOMAIN_NAME) < 0 ||
		get_string(&request->msgptr, request->msgend, domain, MAX_ESCAPED_DOMAIN_NAME) < 0 ||
		get_string(&request->msgptr, request->msgend, host, MAX_ESCAPED_DOMAIN_NAME) < 0)
		{ LogMsg("ERROR: handle_regservice_request - Couldn't read name/regtype/domain"); return(mStatus_BadParamErr); }

	request->u.servicereg.InterfaceID = InterfaceID;
	request->u.servicereg.instances = NULL;
	request->u.servicereg.txtlen  = 0;
	request->u.servicereg.txtdata = NULL;
	mDNSPlatformStrCopy(request->u.servicereg.type_as_string, type_as_string);

	if (request->msgptr + 2 > request->msgend) request->msgptr = NULL;
	else
		{
		request->u.servicereg.port.b[0] = *request->msgptr++;
		request->u.servicereg.port.b[1] = *request->msgptr++;
		}

	request->u.servicereg.txtlen = get_uint16(&request->msgptr, request->msgend);
	if (request->u.servicereg.txtlen)
		{
		request->u.servicereg.txtdata = mallocL("service_info txtdata", request->u.servicereg.txtlen);
		if (!request->u.servicereg.txtdata) FatalError("ERROR: handle_regservice_request - malloc");
		mDNSPlatformMemCopy(request->u.servicereg.txtdata, get_rdata(&request->msgptr, request->msgend, request->u.servicereg.txtlen), request->u.servicereg.txtlen);
		}

	if (!request->msgptr) { LogMsg("%3d: DNSServiceRegister(unreadable parameters)", request->sd); return(mStatus_BadParamErr); }

	// Check for sub-types after the service type
	request->u.servicereg.num_subtypes = ChopSubTypes(request->u.servicereg.type_as_string);	// Note: Modifies regtype string to remove trailing subtypes
	if (request->u.servicereg.num_subtypes < 0)
		{ LogMsg("ERROR: handle_regservice_request - ChopSubTypes failed %s", request->u.servicereg.type_as_string); return(mStatus_BadParamErr); }

	// Don't try to construct "domainname t" until *after* ChopSubTypes has worked its magic
	if (!*request->u.servicereg.type_as_string || !MakeDomainNameFromDNSNameString(&request->u.servicereg.type, request->u.servicereg.type_as_string))
		{ LogMsg("ERROR: handle_regservice_request - type_as_string bad %s", request->u.servicereg.type_as_string); return(mStatus_BadParamErr); }

	if (!name[0])
		{
		request->u.servicereg.name = mDNSStorage.nicelabel;
		request->u.servicereg.autoname = mDNStrue;
		}
	else
		{
		// If the client is allowing AutoRename, then truncate name to legal length before converting it to a DomainLabel
		if ((flags & kDNSServiceFlagsNoAutoRename) == 0)
			{
			int newlen = TruncateUTF8ToLength((mDNSu8*)name, mDNSPlatformStrLen(name), MAX_DOMAIN_LABEL);
			name[newlen] = 0;
			}
		if (!MakeDomainLabelFromLiteralString(&request->u.servicereg.name, name))
			{ LogMsg("ERROR: handle_regservice_request - name bad %s", name); return(mStatus_BadParamErr); }
		request->u.servicereg.autoname = mDNSfalse;
		}

	if (*domain)
		{
		request->u.servicereg.default_domain = mDNSfalse;
		if (!MakeDomainNameFromDNSNameString(&d, domain))
			{ LogMsg("ERROR: handle_regservice_request - domain bad %s", domain); return(mStatus_BadParamErr); }
		}
	else
		{
		request->u.servicereg.default_domain = mDNStrue;
		MakeDomainNameFromDNSNameString(&d, "local.");
		}

	if (!ConstructServiceName(&srv, &request->u.servicereg.name, &request->u.servicereg.type, &d))
		{
		LogMsg("ERROR: handle_regservice_request - Couldn't ConstructServiceName from, “%#s” “%##s” “%##s”",
			request->u.servicereg.name.c, request->u.servicereg.type.c, d.c); return(mStatus_BadParamErr);
		}

	if (!MakeDomainNameFromDNSNameString(&request->u.servicereg.host, host))
		{ LogMsg("ERROR: handle_regservice_request - host bad %s", host); return(mStatus_BadParamErr); }
	request->u.servicereg.autorename       = (flags & kDNSServiceFlagsNoAutoRename    ) == 0;
	request->u.servicereg.allowremotequery = (flags & kDNSServiceFlagsAllowRemoteQuery) != 0;

	// Some clients use mDNS for lightweight copy protection, registering a pseudo-service with
	// a port number of zero. When two instances of the protected client are allowed to run on one
	// machine, we don't want to see misleading "Bogus client" messages in syslog and the console.
	if (!mDNSIPPortIsZero(request->u.servicereg.port))
		{
		int count = CountExistingRegistrations(&srv, request->u.servicereg.port);
		if (count)
			LogMsg("Client application registered %d identical instances of service %##s port %u.",
				count+1, srv.c, mDNSVal16(request->u.servicereg.port));
		}

	LogOperation("%3d: DNSServiceRegister(\"%s\", \"%s\", \"%s\", \"%s\", %u) START",
		request->sd, name, request->u.servicereg.type_as_string, domain, host, mDNSVal16(request->u.servicereg.port));

	// We need to unconditionally set request->terminate, because even if we didn't successfully
	// start any registrations right now, subsequent configuration changes may cause successful
	// registrations to be added, and we'll need to cancel them before freeing this memory.
	// We also need to set request->terminate first, before adding additional service instances,
	// because the uds_validatelists uses the request->terminate function pointer to determine
	// what kind of request this is, and therefore what kind of list validation is required.
	request->terminate = regservice_termination_callback;

	err = register_service_instance(request, &d);

#if 0
	err = AuthorizedDomain(request, &d, AutoRegistrationDomains) ? register_service_instance(request, &d) : mStatus_NoError;
#endif
	if (!err)
		{
		if (request->u.servicereg.autoname) UpdateDeviceInfoRecord(&mDNSStorage);

		if (!*domain)
			{
			DNameListElem *ptr;
			// Note that we don't report errors for non-local, non-explicit domains
			for (ptr = AutoRegistrationDomains; ptr; ptr = ptr->next)
				if (!ptr->uid || SystemUID(request->uid) || request->uid == ptr->uid)
					register_service_instance(request, &ptr->name);
			}
		}

	return(err);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - DNSServiceBrowse
#endif

mDNSlocal void FoundInstance(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
	{
	const DNSServiceFlags flags = AddRecord ? kDNSServiceFlagsAdd : 0;
	request_state *req = question->QuestionContext;
	reply_state *rep;
	(void)m; // Unused

	if (answer->rrtype != kDNSType_PTR)
		{ LogMsg("%3d: FoundInstance: Should not be called with rrtype %d (not a PTR record)", req->sd, answer->rrtype); return; }

	if (GenerateNTDResponse(&answer->rdata->u.name, answer->InterfaceID, req, &rep, browse_reply_op, flags, mStatus_NoError) != mStatus_NoError)
		{
		if (SameDomainName(&req->u.browser.regtype, (const domainname*)"\x09_services\x07_dns-sd\x04_udp"))
			{
			// Special support to enable the DNSServiceBrowse call made by Bonjour Browser
			// Remove after Bonjour Browser is updated to use DNSServiceQueryRecord instead of DNSServiceBrowse
			GenerateBonjourBrowserResponse(&answer->rdata->u.name, answer->InterfaceID, req, &rep, browse_reply_op, flags, mStatus_NoError);
			goto bonjourbrowserhack;
			}

		LogMsg("%3d: FoundInstance: %##s PTR %##s received from network is not valid DNS-SD service pointer",
			req->sd, answer->name->c, answer->rdata->u.name.c);
		return;
		}

bonjourbrowserhack:

	LogOperation("%3d: DNSServiceBrowse(%##s, %s) RESULT %s %d: %s",
		req->sd, question->qname.c, DNSTypeName(question->qtype), AddRecord ? "Add" : "Rmv",
		mDNSPlatformInterfaceIndexfromInterfaceID(m, answer->InterfaceID), RRDisplayString(m, answer));

	append_reply(req, rep);
	}

mDNSlocal mStatus add_domain_to_browser(request_state *info, const domainname *d)
	{
	browser_t *b, *p;
	mStatus err;

	for (p = info->u.browser.browsers; p; p = p->next)
		{
		if (SameDomainName(&p->domain, d))
			{ debugf("add_domain_to_browser %##s already in list", d->c); return mStatus_AlreadyRegistered; }
		}

	b = mallocL("browser_t", sizeof(*b));
	if (!b) return mStatus_NoMemoryErr;
	AssignDomainName(&b->domain, d);
	err = mDNS_StartBrowse(&mDNSStorage, &b->q,
		&info->u.browser.regtype, d, info->u.browser.interface_id, info->u.browser.ForceMCast, FoundInstance, info);
	if (err)
		{
		LogMsg("mDNS_StartBrowse returned %d for type %##s domain %##s", err, info->u.browser.regtype.c, d->c);
		freeL("browser_t/add_domain_to_browser", b);
		}
	else
		{
		b->next = info->u.browser.browsers;
		info->u.browser.browsers = b;
		LogOperation("%3d: DNSServiceBrowse(%##s) START", info->sd, b->q.qname.c);
		}
	return err;
	}

mDNSlocal void browse_termination_callback(request_state *info)
	{
	while (info->u.browser.browsers)
		{
		browser_t *ptr = info->u.browser.browsers;
		info->u.browser.browsers = ptr->next;
		LogOperation("%3d: DNSServiceBrowse(%##s) STOP", info->sd, ptr->q.qname.c);
		mDNS_StopBrowse(&mDNSStorage, &ptr->q);  // no need to error-check result
		freeL("browser_t/browse_termination_callback", ptr);
		}
	}

mDNSlocal void udsserver_automatic_browse_domain_changed(const DNameListElem *const d, const mDNSBool add)
	{
	request_state *request;
	debugf("udsserver_automatic_browse_domain_changed: %s default browse domain %##s", add ? "Adding" : "Removing", d->name.c);

#if APPLE_OSX_mDNSResponder
	machserver_automatic_browse_domain_changed(&d->name, add);
#endif // APPLE_OSX_mDNSResponder

	for (request = all_requests; request; request = request->next)
		{
		if (request->terminate != browse_termination_callback) continue;	// Not a browse operation
		if (!request->u.browser.default_domain) continue;					// Not an auto-browse operation
		if (!d->uid || SystemUID(request->uid) || request->uid == d->uid)
			{
			browser_t **ptr = &request->u.browser.browsers;
			while (*ptr && !SameDomainName(&(*ptr)->domain, &d->name)) ptr = &(*ptr)->next;
			if (add)
				{
				// If we don't already have this domain in our list for this browse operation, add it now
				if (!*ptr) add_domain_to_browser(request, &d->name);
				else debugf("udsserver_automatic_browse_domain_changed %##s already in list, not re-adding", &d->name);
				}
			else
				{
				if (!*ptr) LogMsg("udsserver_automatic_browse_domain_changed ERROR %##s not found", &d->name);
				else
					{
					DNameListElem *p;
					for (p = AutoBrowseDomains; p; p=p->next)
						if (!p->uid || SystemUID(request->uid) || request->uid == p->uid)
							if (SameDomainName(&d->name, &p->name)) break;
					if (p) debugf("udsserver_automatic_browse_domain_changed %##s still in list, not removing", &d->name);
					else
						{
						browser_t *rem = *ptr;
						*ptr = (*ptr)->next;
						mDNS_StopQueryWithRemoves(&mDNSStorage, &rem->q);
						freeL("browser_t/udsserver_automatic_browse_domain_changed", rem);
						}
					}
				}
			}
		}
	}

mDNSlocal void FreeARElemCallback(mDNS *const m, AuthRecord *const rr, mStatus result)
	{
	(void)m;  // unused
	if (result == mStatus_MemFree)
		{
		// On shutdown, mDNS_Close automatically deregisters all records
		// Since in this case no one has called DeregisterLocalOnlyDomainEnumPTR to cut the record
		// from the LocalDomainEnumRecords list, we do this here before we free the memory.
		ARListElem **ptr = &LocalDomainEnumRecords;
		while (*ptr && &(*ptr)->ar != rr) ptr = &(*ptr)->next;
		if (*ptr) *ptr = (*ptr)->next;
		mDNSPlatformMemFree(rr->RecordContext);
		}
	}

mDNSlocal void RegisterLocalOnlyDomainEnumPTR(mDNS *m, const domainname *d, int type)
	{
	// allocate/register legacy and non-legacy _browse PTR record
	mStatus err;
	ARListElem *ptr = mDNSPlatformMemAllocate(sizeof(*ptr));

	debugf("Incrementing %s refcount for %##s",
		(type == mDNS_DomainTypeBrowse         ) ? "browse domain   " :
		(type == mDNS_DomainTypeRegistration   ) ? "registration dom" :
		(type == mDNS_DomainTypeBrowseAutomatic) ? "automatic browse" : "?", d->c);

	mDNS_SetupResourceRecord(&ptr->ar, mDNSNULL, mDNSInterface_LocalOnly, kDNSType_PTR, 7200, kDNSRecordTypeShared, FreeARElemCallback, ptr);
	MakeDomainNameFromDNSNameString(&ptr->ar.namestorage, mDNS_DomainTypeNames[type]);
	AppendDNSNameString            (&ptr->ar.namestorage, "local");
	AssignDomainName(&ptr->ar.resrec.rdata->u.name, d);
	err = mDNS_Register(m, &ptr->ar);
	if (err)
		{
		LogMsg("SetSCPrefsBrowseDomain: mDNS_Register returned error %d", err);
		mDNSPlatformMemFree(ptr);
		}
	else
		{
		ptr->next = LocalDomainEnumRecords;
		LocalDomainEnumRecords = ptr;
		}
	}

mDNSlocal void DeregisterLocalOnlyDomainEnumPTR(mDNS *m, const domainname *d, int type)
	{
	ARListElem **ptr = &LocalDomainEnumRecords;
	domainname lhs; // left-hand side of PTR, for comparison

	debugf("Decrementing %s refcount for %##s",
		(type == mDNS_DomainTypeBrowse         ) ? "browse domain   " :
		(type == mDNS_DomainTypeRegistration   ) ? "registration dom" :
		(type == mDNS_DomainTypeBrowseAutomatic) ? "automatic browse" : "?", d->c);

	MakeDomainNameFromDNSNameString(&lhs, mDNS_DomainTypeNames[type]);
	AppendDNSNameString            (&lhs, "local");

	while (*ptr)
		{
		if (SameDomainName(&(*ptr)->ar.resrec.rdata->u.name, d) && SameDomainName((*ptr)->ar.resrec.name, &lhs))
			{
			ARListElem *rem = *ptr;
			*ptr = (*ptr)->next;
			mDNS_Deregister(m, &rem->ar);
			return;
			}
		else ptr = &(*ptr)->next;
		}
	}

mDNSlocal void AddAutoBrowseDomain(const mDNSu32 uid, const domainname *const name)
	{
	DNameListElem *new = mDNSPlatformMemAllocate(sizeof(DNameListElem));
	if (!new) { LogMsg("ERROR: malloc"); return; }
	AssignDomainName(&new->name, name);
	new->uid = uid;
	new->next = AutoBrowseDomains;
	AutoBrowseDomains = new;
	udsserver_automatic_browse_domain_changed(new, mDNStrue);
	}

mDNSlocal void RmvAutoBrowseDomain(const mDNSu32 uid, const domainname *const name)
	{
	DNameListElem **p = &AutoBrowseDomains;
	while (*p && (!SameDomainName(&(*p)->name, name) || (*p)->uid != uid)) p = &(*p)->next;
	if (!*p) LogMsg("RmvAutoBrowseDomain: Got remove event for domain %##s not in list", name->c);
	else
		{
		DNameListElem *ptr = *p;
		*p = ptr->next;
		udsserver_automatic_browse_domain_changed(ptr, mDNSfalse);
		mDNSPlatformMemFree(ptr);
		}
	}

mDNSlocal void SetPrefsBrowseDomains(mDNS *m, DNameListElem *browseDomains, mDNSBool add)
	{
	DNameListElem *d;
	for (d = browseDomains; d; d = d->next)
		{
		if (add)
			{
			RegisterLocalOnlyDomainEnumPTR(m, &d->name, mDNS_DomainTypeBrowse);
			AddAutoBrowseDomain(d->uid, &d->name);
			}
		else
			{
			DeregisterLocalOnlyDomainEnumPTR(m, &d->name, mDNS_DomainTypeBrowse);
			RmvAutoBrowseDomain(d->uid, &d->name);
			}
		}
	}

mDNSlocal void UpdateDeviceInfoRecord(mDNS *const m)
	{
	int num_autoname = 0;
	request_state *req;
	for (req = all_requests; req; req = req->next)
		if (req->terminate == regservice_termination_callback && req->u.servicereg.autoname)
			num_autoname++;

	// If DeviceInfo record is currently registered, see if we need to deregister it
	if (m->DeviceInfo.resrec.RecordType != kDNSRecordTypeUnregistered)
		if (num_autoname == 0 || !SameDomainLabelCS(m->DeviceInfo.resrec.name->c, m->nicelabel.c))
			{
			LogOperation("UpdateDeviceInfoRecord Deregister %##s", m->DeviceInfo.resrec.name);
			mDNS_Deregister(m, &m->DeviceInfo);
			}

	// If DeviceInfo record is not currently registered, see if we need to register it
	if (m->DeviceInfo.resrec.RecordType == kDNSRecordTypeUnregistered)
		if (num_autoname > 0)
			{
			mDNSu8 len = m->HIHardware.c[0] < 255 - 6 ? m->HIHardware.c[0] : 255 - 6;
			mDNS_SetupResourceRecord(&m->DeviceInfo, mDNSNULL, mDNSNULL, kDNSType_TXT, kStandardTTL, kDNSRecordTypeAdvisory, mDNSNULL, mDNSNULL);
			ConstructServiceName(&m->DeviceInfo.namestorage, &m->nicelabel, &DeviceInfoName, &localdomain);
			mDNSPlatformMemCopy(m->DeviceInfo.resrec.rdata->u.data + 1, "model=", 6);
			mDNSPlatformMemCopy(m->DeviceInfo.resrec.rdata->u.data + 7, m->HIHardware.c + 1, len);
			m->DeviceInfo.resrec.rdata->u.data[0] = 6 + len;	// "model=" plus the device string
			m->DeviceInfo.resrec.rdlength         = 7 + len;	// One extra for the length byte at the start of the string
			LogOperation("UpdateDeviceInfoRecord   Register %##s", m->DeviceInfo.resrec.name);
			mDNS_Register(m, &m->DeviceInfo);
			}
	}

mDNSexport void udsserver_handle_configchange(mDNS *const m)
	{
	request_state *req;
	service_instance *ptr;
	DNameListElem *RegDomains = NULL;
	DNameListElem *BrowseDomains = NULL;
	DNameListElem *p;

	UpdateDeviceInfoRecord(m);

	// For autoname services, see if the default service name has changed, necessitating an automatic update
	for (req = all_requests; req; req = req->next)
		if (req->terminate == regservice_termination_callback)
			if (req->u.servicereg.autoname && !SameDomainLabelCS(req->u.servicereg.name.c, m->nicelabel.c))
				{
				req->u.servicereg.name = m->nicelabel;
				for (ptr = req->u.servicereg.instances; ptr; ptr = ptr->next)
					{
					ptr->renameonmemfree = 1;
					if (ptr->clientnotified) SendServiceRemovalNotification(&ptr->srs);
					if (mDNS_DeregisterService(m, &ptr->srs)) // If service was deregistered already
						regservice_callback(m, &ptr->srs, mStatus_MemFree); // we can re-register immediately
					}
				}

	// Let the platform layer get the current DNS information
	mDNS_Lock(m);
	mDNSPlatformSetDNSConfig(m, mDNSfalse, mDNSfalse, mDNSNULL, &RegDomains, &BrowseDomains);
	mDNS_Unlock(m);

	// Any automatic registration domains are also implicitly automatic browsing domains
	if (RegDomains) SetPrefsBrowseDomains(m, RegDomains, mDNStrue);								// Add the new list first
	if (AutoRegistrationDomains) SetPrefsBrowseDomains(m, AutoRegistrationDomains, mDNSfalse);	// Then clear the old list

	// Add any new domains not already in our AutoRegistrationDomains list
	for (p=RegDomains; p; p=p->next)
		{
		DNameListElem **pp = &AutoRegistrationDomains;
		while (*pp && ((*pp)->uid != p->uid || !SameDomainName(&(*pp)->name, &p->name))) pp = &(*pp)->next;
		if (!*pp)		// If not found in our existing list, this is a new default registration domain
			{
			RegisterLocalOnlyDomainEnumPTR(m, &p->name, mDNS_DomainTypeRegistration);
			udsserver_default_reg_domain_changed(p, mDNStrue);
			}
		else			// else found same domainname in both old and new lists, so no change, just delete old copy
			{
			DNameListElem *del = *pp;
			*pp = (*pp)->next;
			mDNSPlatformMemFree(del);
			}
		}

	// Delete any domains in our old AutoRegistrationDomains list that are now gone
	while (AutoRegistrationDomains)
		{
		DNameListElem *del = AutoRegistrationDomains;
		AutoRegistrationDomains = AutoRegistrationDomains->next;		// Cut record from list FIRST,
		DeregisterLocalOnlyDomainEnumPTR(m, &del->name, mDNS_DomainTypeRegistration);
		udsserver_default_reg_domain_changed(del, mDNSfalse);			// before calling udsserver_default_reg_domain_changed()
		mDNSPlatformMemFree(del);
		}

	// Now we have our new updated automatic registration domain list
	AutoRegistrationDomains = RegDomains;

	// Add new browse domains to internal list
	if (BrowseDomains) SetPrefsBrowseDomains(m, BrowseDomains, mDNStrue);

	// Remove old browse domains from internal list
	if (SCPrefBrowseDomains)
		{
		SetPrefsBrowseDomains(m, SCPrefBrowseDomains, mDNSfalse);
		while (SCPrefBrowseDomains)
			{
			DNameListElem *fptr = SCPrefBrowseDomains;
			SCPrefBrowseDomains = SCPrefBrowseDomains->next;
			mDNSPlatformMemFree(fptr);
			}
		}

	// Replace the old browse domains array with the new array
	SCPrefBrowseDomains = BrowseDomains;
	}

mDNSlocal void AutomaticBrowseDomainChange(mDNS *const m, DNSQuestion *q, const ResourceRecord *const answer, QC_result AddRecord)
	{
	(void)m; // unused;
	(void)q; // unused

	LogOperation("AutomaticBrowseDomainChange: %s automatic browse domain %##s",
		AddRecord ? "Adding" : "Removing", answer->rdata->u.name.c);

	if (AddRecord) AddAutoBrowseDomain(0, &answer->rdata->u.name);
	else           RmvAutoBrowseDomain(0, &answer->rdata->u.name);
	}

mDNSlocal mStatus handle_browse_request(request_state *request)
	{
	char regtype[MAX_ESCAPED_DOMAIN_NAME], domain[MAX_ESCAPED_DOMAIN_NAME];
	domainname typedn, d, temp;
	mDNSs32 NumSubTypes;
	mStatus err = mStatus_NoError;

	DNSServiceFlags flags = get_flags(&request->msgptr, request->msgend);
	mDNSu32 interfaceIndex = get_uint32(&request->msgptr, request->msgend);
	mDNSInterfaceID InterfaceID = mDNSPlatformInterfaceIDfromInterfaceIndex(&mDNSStorage, interfaceIndex);
	if (interfaceIndex && !InterfaceID) return(mStatus_BadParamErr);

	if (get_string(&request->msgptr, request->msgend, regtype, MAX_ESCAPED_DOMAIN_NAME) < 0 ||
		get_string(&request->msgptr, request->msgend, domain, MAX_ESCAPED_DOMAIN_NAME) < 0) return(mStatus_BadParamErr);

	if (!request->msgptr) { LogMsg("%3d: DNSServiceBrowse(unreadable parameters)", request->sd); return(mStatus_BadParamErr); }

	if (domain[0] == '\0') uDNS_RegisterSearchDomains(&mDNSStorage);

	typedn.c[0] = 0;
	NumSubTypes = ChopSubTypes(regtype);	// Note: Modifies regtype string to remove trailing subtypes
	if (NumSubTypes < 0 || NumSubTypes > 1) return(mStatus_BadParamErr);
	if (NumSubTypes == 1 && !AppendDNSNameString(&typedn, regtype + strlen(regtype) + 1)) return(mStatus_BadParamErr);

	if (!regtype[0] || !AppendDNSNameString(&typedn, regtype)) return(mStatus_BadParamErr);

	if (!MakeDomainNameFromDNSNameString(&temp, regtype)) return(mStatus_BadParamErr);
	// For over-long service types, we only allow domain "local"
	if (temp.c[0] > 15 && domain[0] == 0) mDNSPlatformStrCopy(domain, "local.");

	// Set up browser info
	request->u.browser.ForceMCast = (flags & kDNSServiceFlagsForceMulticast) != 0;
	request->u.browser.interface_id = InterfaceID;
	AssignDomainName(&request->u.browser.regtype, &typedn);
	request->u.browser.default_domain = !domain[0];
	request->u.browser.browsers = NULL;

	LogOperation("%3d: DNSServiceBrowse(\"%##s\", \"%s\") START", request->sd, request->u.browser.regtype.c, domain);

	// We need to unconditionally set request->terminate, because even if we didn't successfully
	// start any browses right now, subsequent configuration changes may cause successful
	// browses to be added, and we'll need to cancel them before freeing this memory.
	request->terminate = browse_termination_callback;

	if (domain[0])
		{
		if (!MakeDomainNameFromDNSNameString(&d, domain)) return(mStatus_BadParamErr);
		err = add_domain_to_browser(request, &d);
#if 0
		err = AuthorizedDomain(request, &d, AutoBrowseDomains) ? add_domain_to_browser(request, &d) : mStatus_NoError;
#endif
		}
	else
		{
		DNameListElem *sdom;
		for (sdom = AutoBrowseDomains; sdom; sdom = sdom->next)
			if (!sdom->uid || SystemUID(request->uid) || request->uid == sdom->uid)
				{
				err = add_domain_to_browser(request, &sdom->name);
				if (err)
					{
					if (SameDomainName(&sdom->name, &localdomain)) break;
					else err = mStatus_NoError;  // suppress errors for non-local "default" domains
					}
				}
		}

	return(err);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - DNSServiceResolve
#endif

mDNSlocal void resolve_result_callback(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
	{
	size_t len = 0;
	char fullname[MAX_ESCAPED_DOMAIN_NAME], target[MAX_ESCAPED_DOMAIN_NAME];
	char *data;
	reply_state *rep;
	request_state *req = question->QuestionContext;
	(void)m; // Unused

	LogOperation("%3d: DNSServiceResolve(%##s) %s %s", req->sd, question->qname.c, AddRecord ? "ADD" : "RMV", RRDisplayString(m, answer));

	if (!AddRecord)
		{
		if (req->u.resolve.srv == answer) req->u.resolve.srv = mDNSNULL;
		if (req->u.resolve.txt == answer) req->u.resolve.txt = mDNSNULL;
		return;
		}

	if (answer->rrtype == kDNSType_SRV) req->u.resolve.srv = answer;
	if (answer->rrtype == kDNSType_TXT) req->u.resolve.txt = answer;

	if (!req->u.resolve.txt || !req->u.resolve.srv) return;		// only deliver result to client if we have both answers

	ConvertDomainNameToCString(answer->name, fullname);
	ConvertDomainNameToCString(&req->u.resolve.srv->rdata->u.srv.target, target);

	// calculate reply length
	len += sizeof(DNSServiceFlags);
	len += sizeof(mDNSu32);  // interface index
	len += sizeof(DNSServiceErrorType);
	len += strlen(fullname) + 1;
	len += strlen(target) + 1;
	len += 2 * sizeof(mDNSu16);  // port, txtLen
	len += req->u.resolve.txt->rdlength;

	// allocate/init reply header
	rep = create_reply(resolve_reply_op, len, req);
	rep->rhdr->flags = dnssd_htonl(0);
	rep->rhdr->ifi   = dnssd_htonl(mDNSPlatformInterfaceIndexfromInterfaceID(m, answer->InterfaceID));
	rep->rhdr->error = dnssd_htonl(kDNSServiceErr_NoError);

	data = (char *)&rep->rhdr[1];

	// write reply data to message
	put_string(fullname, &data);
	put_string(target, &data);
	*data++ =  req->u.resolve.srv->rdata->u.srv.port.b[0];
	*data++ =  req->u.resolve.srv->rdata->u.srv.port.b[1];
	put_uint16(req->u.resolve.txt->rdlength, &data);
	put_rdata (req->u.resolve.txt->rdlength, req->u.resolve.txt->rdata->u.data, &data);

	LogOperation("%3d: DNSServiceResolve(%s) RESULT   %s:%d", req->sd, fullname, target, mDNSVal16(req->u.resolve.srv->rdata->u.srv.port));
	append_reply(req, rep);
	}

mDNSlocal void resolve_termination_callback(request_state *request)
	{
	LogOperation("%3d: DNSServiceResolve(%##s) STOP", request->sd, request->u.resolve.qtxt.qname.c);
	mDNS_StopQuery(&mDNSStorage, &request->u.resolve.qtxt);
	mDNS_StopQuery(&mDNSStorage, &request->u.resolve.qsrv);
	}

mDNSlocal mStatus handle_resolve_request(request_state *request)
	{
	char name[256], regtype[MAX_ESCAPED_DOMAIN_NAME], domain[MAX_ESCAPED_DOMAIN_NAME];
	domainname fqdn;
	mStatus err;

	// extract the data from the message
	DNSServiceFlags flags = get_flags(&request->msgptr, request->msgend);
	mDNSu32 interfaceIndex = get_uint32(&request->msgptr, request->msgend);
	mDNSInterfaceID InterfaceID = mDNSPlatformInterfaceIDfromInterfaceIndex(&mDNSStorage, interfaceIndex);
	if (interfaceIndex && !InterfaceID)
		{ LogMsg("ERROR: handle_resolve_request bad interfaceIndex %d", interfaceIndex); return(mStatus_BadParamErr); }

	if (get_string(&request->msgptr, request->msgend, name, 256) < 0 ||
		get_string(&request->msgptr, request->msgend, regtype, MAX_ESCAPED_DOMAIN_NAME) < 0 ||
		get_string(&request->msgptr, request->msgend, domain, MAX_ESCAPED_DOMAIN_NAME) < 0)
		{ LogMsg("ERROR: handle_resolve_request - Couldn't read name/regtype/domain"); return(mStatus_BadParamErr); }

	if (!request->msgptr) { LogMsg("%3d: DNSServiceResolve(unreadable parameters)", request->sd); return(mStatus_BadParamErr); }

	if (build_domainname_from_strings(&fqdn, name, regtype, domain) < 0)
		{ LogMsg("ERROR: handle_resolve_request bad “%s” “%s” “%s”", name, regtype, domain); return(mStatus_BadParamErr); }

	mDNSPlatformMemZero(&request->u.resolve, sizeof(request->u.resolve));

	// format questions
	request->u.resolve.qsrv.InterfaceID      = InterfaceID;
	request->u.resolve.qsrv.Target           = zeroAddr;
	AssignDomainName(&request->u.resolve.qsrv.qname, &fqdn);
	request->u.resolve.qsrv.qtype            = kDNSType_SRV;
	request->u.resolve.qsrv.qclass           = kDNSClass_IN;
	request->u.resolve.qsrv.LongLived        = (flags & kDNSServiceFlagsLongLivedQuery     ) != 0;
	request->u.resolve.qsrv.ExpectUnique     = mDNStrue;
	request->u.resolve.qsrv.ForceMCast       = (flags & kDNSServiceFlagsForceMulticast     ) != 0;
	request->u.resolve.qsrv.ReturnIntermed   = (flags & kDNSServiceFlagsReturnIntermediates) != 0;
	request->u.resolve.qsrv.QuestionCallback = resolve_result_callback;
	request->u.resolve.qsrv.QuestionContext  = request;

	request->u.resolve.qtxt.InterfaceID      = InterfaceID;
	request->u.resolve.qtxt.Target           = zeroAddr;
	AssignDomainName(&request->u.resolve.qtxt.qname, &fqdn);
	request->u.resolve.qtxt.qtype            = kDNSType_TXT;
	request->u.resolve.qtxt.qclass           = kDNSClass_IN;
	request->u.resolve.qtxt.LongLived        = (flags & kDNSServiceFlagsLongLivedQuery     ) != 0;
	request->u.resolve.qtxt.ExpectUnique     = mDNStrue;
	request->u.resolve.qtxt.ForceMCast       = (flags & kDNSServiceFlagsForceMulticast     ) != 0;
	request->u.resolve.qtxt.ReturnIntermed   = (flags & kDNSServiceFlagsReturnIntermediates) != 0;
	request->u.resolve.qtxt.QuestionCallback = resolve_result_callback;
	request->u.resolve.qtxt.QuestionContext  = request;

	request->u.resolve.ReportTime            = NonZeroTime(mDNS_TimeNow(&mDNSStorage) + 130 * mDNSPlatformOneSecond);

#if 0
	if (!AuthorizedDomain(request, &fqdn, AutoBrowseDomains))	return(mStatus_NoError);
#endif

	// ask the questions
	LogOperation("%3d: DNSServiceResolve(%##s) START", request->sd, request->u.resolve.qsrv.qname.c);
	err = mDNS_StartQuery(&mDNSStorage, &request->u.resolve.qsrv);
	if (!err)
		{
		err = mDNS_StartQuery(&mDNSStorage, &request->u.resolve.qtxt);
		if (err) mDNS_StopQuery(&mDNSStorage, &request->u.resolve.qsrv);
		else request->terminate = resolve_termination_callback;
		}

	return(err);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - DNSServiceQueryRecord
#endif

// mDNS operation functions. Each operation has 3 associated functions - a request handler that parses
// the client's request and makes the appropriate mDNSCore call, a result handler (passed as a callback
// to the mDNSCore routine) that sends results back to the client, and a termination routine that aborts
// the mDNSCore operation if the client dies or closes its socket.

mDNSlocal void queryrecord_result_callback(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
	{
	char name[MAX_ESCAPED_DOMAIN_NAME];
	request_state *req = question->QuestionContext;
	reply_state *rep;
	char *data;
	size_t len;
	DNSServiceErrorType error = kDNSServiceErr_NoError;
	(void)m; // Unused

#if APPLE_OSX_mDNSResponder
	if (question == &req->u.queryrecord.q2)
		{
		mDNS_StopQuery(&mDNSStorage, question);
		// If we got a non-negative answer for our "local SOA" test query, start an additional parallel unicast query
		if (answer->RecordType == kDNSRecordTypePacketNegative ||
			(question->qtype == req->u.queryrecord.q.qtype && SameDomainName(&question->qname, &req->u.queryrecord.q.qname)))
			question->QuestionCallback = mDNSNULL;
		else
			{
			*question              = req->u.queryrecord.q;
			question->InterfaceID  = mDNSInterface_Unicast;
			question->ExpectUnique = mDNStrue;
			mStatus err = mDNS_StartQuery(&mDNSStorage, question);
			if (!err) LogOperation("%3d: DNSServiceQueryRecord(%##s, %s) unicast", req->sd, question->qname.c, DNSTypeName(question->qtype));
			else LogMsg("%3d: ERROR: queryrecord_result_callback %##s %s mDNS_StartQuery: %d", req->sd, question->qname.c, DNSTypeName(question->qtype), (int)err);
			}
		return;
		}
#endif // APPLE_OSX_mDNSResponder

	if (answer->RecordType == kDNSRecordTypePacketNegative)
		{
		// When we're doing parallel unicast and multicast queries for dot-local names (for supporting Microsoft
		// Active Directory sites) we need to ignore negative unicast answers. Otherwise we'll generate negative
		// answers for just about every single multicast name we ever look up, since the Microsoft Active Directory
		// server is going to assert that pretty much every single multicast name doesn't exist.
		if (!answer->InterfaceID && IsLocalDomain(answer->name)) return;
		error = kDNSServiceErr_NoSuchRecord;
		AddRecord = mDNStrue;
		}

	ConvertDomainNameToCString(answer->name, name);

	LogOperation("%3d: %s(%##s, %s) %s %s", req->sd,
		req->hdr.op == query_request ? "DNSServiceQueryRecord" : "DNSServiceGetAddrInfo",
		question->qname.c, DNSTypeName(question->qtype), AddRecord ? "ADD" : "RMV", RRDisplayString(m, answer));

	len = sizeof(DNSServiceFlags);	// calculate reply data length
	len += sizeof(mDNSu32);		// interface index
	len += sizeof(DNSServiceErrorType);
	len += strlen(name) + 1;
	len += 3 * sizeof(mDNSu16);	// type, class, rdlen
	len += answer->rdlength;
	len += sizeof(mDNSu32);		// TTL

	rep = create_reply(req->hdr.op == query_request ? query_reply_op : addrinfo_reply_op, len, req);

	rep->rhdr->flags = dnssd_htonl(AddRecord ? kDNSServiceFlagsAdd : 0);
	rep->rhdr->ifi   = dnssd_htonl(mDNSPlatformInterfaceIndexfromInterfaceID(m, answer->InterfaceID));
	rep->rhdr->error = dnssd_htonl(error);

	data = (char *)&rep->rhdr[1];

	put_string(name,             &data);
	put_uint16(answer->rrtype,   &data);
	put_uint16(answer->rrclass,  &data);
	put_uint16(answer->rdlength, &data);
	// We need to use putRData here instead of the crude put_rdata function, because the crude put_rdata
	// function just does a blind memory copy without regard to structures that may have holes in them.
	if (answer->rdlength)
		if (!putRData(mDNSNULL, (mDNSu8 *)data, (mDNSu8 *)rep->rhdr + len, answer))
			LogMsg("queryrecord_result_callback putRData failed %d", (mDNSu8 *)rep->rhdr + len - (mDNSu8 *)data);
	data += answer->rdlength;
	put_uint32(AddRecord ? answer->rroriginalttl : 0, &data);

	append_reply(req, rep);
	}

mDNSlocal void queryrecord_termination_callback(request_state *request)
	{
	LogOperation("%3d: DNSServiceQueryRecord(%##s, %s) STOP",
		request->sd, request->u.queryrecord.q.qname.c, DNSTypeName(request->u.queryrecord.q.qtype));
	mDNS_StopQuery(&mDNSStorage, &request->u.queryrecord.q);  // no need to error check
	if (request->u.queryrecord.q2.QuestionCallback) mDNS_StopQuery(&mDNSStorage, &request->u.queryrecord.q2);
	}

mDNSlocal mStatus handle_queryrecord_request(request_state *request)
	{
	DNSQuestion *const q = &request->u.queryrecord.q;
	char name[256];
	mDNSu16 rrtype, rrclass;
	mStatus err;

	DNSServiceFlags flags = get_flags(&request->msgptr, request->msgend);
	mDNSu32 interfaceIndex = get_uint32(&request->msgptr, request->msgend);
	mDNSInterfaceID InterfaceID = mDNSPlatformInterfaceIDfromInterfaceIndex(&mDNSStorage, interfaceIndex);
	if (interfaceIndex && !InterfaceID) return(mStatus_BadParamErr);

	if (get_string(&request->msgptr, request->msgend, name, 256) < 0) return(mStatus_BadParamErr);
	rrtype  = get_uint16(&request->msgptr, request->msgend);
	rrclass = get_uint16(&request->msgptr, request->msgend);

	if (!request->msgptr)
		{ LogMsg("%3d: DNSServiceQueryRecord(unreadable parameters)", request->sd); return(mStatus_BadParamErr); }

	mDNSPlatformMemZero(&request->u.queryrecord, sizeof(request->u.queryrecord));

	q->InterfaceID      = InterfaceID;
	q->Target           = zeroAddr;
	if (!MakeDomainNameFromDNSNameString(&q->qname, name)) 			return(mStatus_BadParamErr);
#if 0
	if (!AuthorizedDomain(request, &q->qname, AutoBrowseDomains))	return (mStatus_NoError);
#endif
	q->qtype            = rrtype;
	q->qclass           = rrclass;
	q->LongLived        = (flags & kDNSServiceFlagsLongLivedQuery     ) != 0;
	q->ExpectUnique     = mDNSfalse;
	q->ForceMCast       = (flags & kDNSServiceFlagsForceMulticast     ) != 0;
	q->ReturnIntermed   = (flags & kDNSServiceFlagsReturnIntermediates) != 0;
	q->QuestionCallback = queryrecord_result_callback;
	q->QuestionContext  = request;

	LogOperation("%3d: DNSServiceQueryRecord(%##s, %s, %X) START", request->sd, q->qname.c, DNSTypeName(q->qtype), flags);
	err = mDNS_StartQuery(&mDNSStorage, q);
	if (err) LogMsg("%3d: ERROR: DNSServiceQueryRecord %##s %s mDNS_StartQuery: %d", request->sd, q->qname.c, DNSTypeName(q->qtype), (int)err);
	else request->terminate = queryrecord_termination_callback;

#if APPLE_OSX_mDNSResponder
	// Workaround for networks using Microsoft Active Directory using "local" as a private internal top-level domain
	extern domainname ActiveDirectoryPrimaryDomain;
	#define VALID_MSAD_SRV_TRANSPORT(T) (SameDomainLabel((T)->c, (const mDNSu8 *)"\x4_tcp") || SameDomainLabel((T)->c, (const mDNSu8 *)"\x4_udp"))
	#define VALID_MSAD_SRV(Q) ((Q)->qtype == kDNSType_SRV && VALID_MSAD_SRV_TRANSPORT(SecondLabel(&(Q)->qname)))

	if (!q->ForceMCast && SameDomainLabel(LastLabel(&q->qname), (const mDNSu8 *)&localdomain))
		if (q->qtype == kDNSType_A || q->qtype == kDNSType_AAAA || VALID_MSAD_SRV(q))
			{
			int labels = CountLabels(&q->qname);
			DNSQuestion *const q2 = &request->u.queryrecord.q2;
			*q2              = *q;
			q2->InterfaceID  = mDNSInterface_Unicast;
			q2->ExpectUnique = mDNStrue;
	
			// For names of the form "<one-or-more-labels>.bar.local." we always do a second unicast query in parallel.
			// For names of the form "<one-label>.local." it's less clear whether we should do a unicast query.
			// If the name being queried is exactly the same as the name in the DHCP "domain" option (e.g. the DHCP
			// "domain" is my-small-company.local, and the user types "my-small-company.local" into their web browser)
			// then that's a hint that it's worth doing a unicast query. Otherwise, we first check to see if the
			// site's DNS server claims there's an SOA record for "local", and if so, that's also a hint that queries
			// for names in the "local" domain will be safely answered privately before they hit the root name servers.
			if (labels == 2 && !SameDomainName(&q->qname, &ActiveDirectoryPrimaryDomain))
				{
				AssignDomainName(&q2->qname, &localdomain);
				q2->qtype          = kDNSType_SOA;
				q2->LongLived      = mDNSfalse;
				q2->ForceMCast     = mDNSfalse;
				q2->ReturnIntermed = mDNStrue;
				}
			err = mDNS_StartQuery(&mDNSStorage, q2);
			if (!err) LogOperation("%3d: DNSServiceQueryRecord(%##s, %s) unicast", request->sd, q2->qname.c, DNSTypeName(q2->qtype));
			else LogMsg("%3d: ERROR: DNSServiceQueryRecord %##s %s mDNS_StartQuery: %d", request->sd, q2->qname.c, DNSTypeName(q2->qtype), (int)err);
			}
#endif // APPLE_OSX_mDNSResponder

	return(err);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - DNSServiceEnumerateDomains
#endif

mDNSlocal reply_state *format_enumeration_reply(request_state *request,
	const char *domain, DNSServiceFlags flags, mDNSu32 ifi, DNSServiceErrorType err)
	{
	size_t len;
	reply_state *reply;
	char *data;

	len = sizeof(DNSServiceFlags);
	len += sizeof(mDNSu32);
	len += sizeof(DNSServiceErrorType);
	len += strlen(domain) + 1;

	reply = create_reply(enumeration_reply_op, len, request);
	reply->rhdr->flags = dnssd_htonl(flags);
	reply->rhdr->ifi   = dnssd_htonl(ifi);
	reply->rhdr->error = dnssd_htonl(err);
	data = (char *)&reply->rhdr[1];
	put_string(domain, &data);
	return reply;
	}

mDNSlocal void enum_termination_callback(request_state *request)
	{
	mDNS_StopGetDomains(&mDNSStorage, &request->u.enumeration.q_all);
	mDNS_StopGetDomains(&mDNSStorage, &request->u.enumeration.q_default);
	}

mDNSlocal void enum_result_callback(mDNS *const m,
	DNSQuestion *const question, const ResourceRecord *const answer, QC_result AddRecord)
	{
	char domain[MAX_ESCAPED_DOMAIN_NAME];
	request_state *request = question->QuestionContext;
	DNSServiceFlags flags = 0;
	reply_state *reply;
	(void)m; // Unused

	if (answer->rrtype != kDNSType_PTR) return;

#if 0
	if (!AuthorizedDomain(request, &answer->rdata->u.name, request->u.enumeration.flags ? AutoRegistrationDomains : AutoBrowseDomains)) return;
#endif

	// We only return add/remove events for the browse and registration lists
	// For the default browse and registration answers, we only give an "ADD" event
	if (question == &request->u.enumeration.q_default && !AddRecord) return;

	if (AddRecord)
		{
		flags |= kDNSServiceFlagsAdd;
		if (question == &request->u.enumeration.q_default) flags |= kDNSServiceFlagsDefault;
		}

	ConvertDomainNameToCString(&answer->rdata->u.name, domain);
	// Note that we do NOT propagate specific interface indexes to the client - for example, a domain we learn from
	// a machine's system preferences may be discovered on the LocalOnly interface, but should be browsed on the
	// network, so we just pass kDNSServiceInterfaceIndexAny
	reply = format_enumeration_reply(request, domain, flags, kDNSServiceInterfaceIndexAny, kDNSServiceErr_NoError);
	if (!reply) { LogMsg("ERROR: enum_result_callback, format_enumeration_reply"); return; }

	LogOperation("%3d: DNSServiceEnumerateDomains(%#2s) RESULT %s: %s", request->sd, question->qname.c, AddRecord ? "Add" : "Rmv", domain);

	append_reply(request, reply);
	}

mDNSlocal mStatus handle_enum_request(request_state *request)
	{
	mStatus err;
	DNSServiceFlags flags = get_flags(&request->msgptr, request->msgend);
	DNSServiceFlags reg = flags & kDNSServiceFlagsRegistrationDomains;
	mDNS_DomainType t_all     = reg ? mDNS_DomainTypeRegistration        : mDNS_DomainTypeBrowse;
	mDNS_DomainType t_default = reg ? mDNS_DomainTypeRegistrationDefault : mDNS_DomainTypeBrowseDefault;
	mDNSu32 interfaceIndex = get_uint32(&request->msgptr, request->msgend);
	mDNSInterfaceID InterfaceID = mDNSPlatformInterfaceIDfromInterfaceIndex(&mDNSStorage, interfaceIndex);
	if (interfaceIndex && !InterfaceID) return(mStatus_BadParamErr);

	if (!request->msgptr)
		{ LogMsg("%3d: DNSServiceEnumerateDomains(unreadable parameters)", request->sd); return(mStatus_BadParamErr); }

	// allocate context structures
	uDNS_RegisterSearchDomains(&mDNSStorage);

#if 0
	// mark which kind of enumeration we're doing so we can (de)authorize certain domains
	request->u.enumeration.flags = reg;
#endif

	// enumeration requires multiple questions, so we must link all the context pointers so that
	// necessary context can be reached from the callbacks
	request->u.enumeration.q_all    .QuestionContext = request;
	request->u.enumeration.q_default.QuestionContext = request;
	
	// if the caller hasn't specified an explicit interface, we use local-only to get the system-wide list.
	if (!InterfaceID) InterfaceID = mDNSInterface_LocalOnly;

	// make the calls
	LogOperation("%3d: DNSServiceEnumerateDomains(%X=%s)", request->sd, flags,
		(flags & kDNSServiceFlagsBrowseDomains      ) ? "kDNSServiceFlagsBrowseDomains" :
		(flags & kDNSServiceFlagsRegistrationDomains) ? "kDNSServiceFlagsRegistrationDomains" : "<<Unknown>>");
	err = mDNS_GetDomains(&mDNSStorage, &request->u.enumeration.q_all, t_all, NULL, InterfaceID, enum_result_callback, request);
	if (!err)
		{
		err = mDNS_GetDomains(&mDNSStorage, &request->u.enumeration.q_default, t_default, NULL, InterfaceID, enum_result_callback, request);
		if (err) mDNS_StopGetDomains(&mDNSStorage, &request->u.enumeration.q_all);
		else request->terminate = enum_termination_callback;
		}

	return(err);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - DNSServiceReconfirmRecord & Misc
#endif

mDNSlocal mStatus handle_reconfirm_request(request_state *request)
	{
	mStatus status = mStatus_BadParamErr;
	AuthRecord *rr = read_rr_from_ipc_msg(request, 0, 0);
	if (rr)
		{
		status = mDNS_ReconfirmByValue(&mDNSStorage, &rr->resrec);
		LogOperation(
			(status == mStatus_NoError) ?
			"%3d: DNSServiceReconfirmRecord(%s) interface %d initiated" :
			"%3d: DNSServiceReconfirmRecord(%s) interface %d failed: %d",
			request->sd, RRDisplayString(&mDNSStorage, &rr->resrec),
			mDNSPlatformInterfaceIndexfromInterfaceID(&mDNSStorage, rr->resrec.InterfaceID), status);
		freeL("AuthRecord/handle_reconfirm_request", rr);
		}
	return(status);
	}

mDNSlocal mStatus handle_setdomain_request(request_state *request)
	{
	char domainstr[MAX_ESCAPED_DOMAIN_NAME];
	domainname domain;
	DNSServiceFlags flags = get_flags(&request->msgptr, request->msgend);
	(void)flags; // Unused
	if (get_string(&request->msgptr, request->msgend, domainstr, MAX_ESCAPED_DOMAIN_NAME) < 0 ||
		!MakeDomainNameFromDNSNameString(&domain, domainstr))
		{ LogMsg("%3d: DNSServiceSetDefaultDomainForUser(unreadable parameters)", request->sd); return(mStatus_BadParamErr); }

	LogOperation("%3d: DNSServiceSetDefaultDomainForUser(%##s)", request->sd, domain.c);
	return(mStatus_NoError);
	}

typedef packedstruct
	{
	mStatus err;
	mDNSu32 len;
	mDNSu32 vers;
	} DaemonVersionReply;

mDNSlocal void handle_getproperty_request(request_state *request)
	{
	const mStatus BadParamErr = dnssd_htonl(mStatus_BadParamErr);
	char prop[256];
	if (get_string(&request->msgptr, request->msgend, prop, sizeof(prop)) >= 0)
		{
		LogOperation("%3d: DNSServiceGetProperty(%s)", request->sd, prop);
		if (!strcmp(prop, kDNSServiceProperty_DaemonVersion))
			{
			DaemonVersionReply x = { 0, dnssd_htonl(4), dnssd_htonl(_DNS_SD_H) };
			send_all(request->sd, (const char *)&x, sizeof(x));
			return;
			}
		}

	// If we didn't recogize the requested property name, return BadParamErr
	send_all(request->sd, (const char *)&BadParamErr, sizeof(BadParamErr));
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - DNSServiceNATPortMappingCreate
#endif

#define DNSServiceProtocol(X) ((X) == NATOp_AddrRequest ? 0 : (X) == NATOp_MapUDP ? kDNSServiceProtocol_UDP : kDNSServiceProtocol_TCP)

mDNSlocal void port_mapping_termination_callback(request_state *request)
	{
	LogOperation("%3d: DNSServiceNATPortMappingCreate(%X, %u, %u, %d) STOP", request->sd,
		DNSServiceProtocol(request->u.pm.NATinfo.Protocol),
		mDNSVal16(request->u.pm.NATinfo.IntPort), mDNSVal16(request->u.pm.ReqExt), request->u.pm.NATinfo.NATLease);
	mDNS_StopNATOperation(&mDNSStorage, &request->u.pm.NATinfo);
	}

// Called via function pointer when we get a NAT-PMP address request or port mapping response
mDNSlocal void port_mapping_create_request_callback(mDNS *m, NATTraversalInfo *n)
	{
	request_state *request = (request_state *)n->clientContext;
	reply_state *rep;
	int replyLen;
	char *data;

	if (!request) { LogMsg("port_mapping_create_request_callback called with unknown request_state object"); return; }

	// calculate reply data length
	replyLen = sizeof(DNSServiceFlags);
	replyLen += 3 * sizeof(mDNSu32);  // if index + addr + ttl
	replyLen += sizeof(DNSServiceErrorType);
	replyLen += 2 * sizeof(mDNSu16);  // Internal Port + External Port
	replyLen += sizeof(mDNSu8);       // protocol

	rep = create_reply(port_mapping_reply_op, replyLen, request);

	rep->rhdr->flags = dnssd_htonl(0);
	rep->rhdr->ifi   = dnssd_htonl(mDNSPlatformInterfaceIndexfromInterfaceID(m, n->InterfaceID));
	rep->rhdr->error = dnssd_htonl(n->Result);

	data = (char *)&rep->rhdr[1];

	*data++ = request->u.pm.NATinfo.ExternalAddress.b[0];
	*data++ = request->u.pm.NATinfo.ExternalAddress.b[1];
	*data++ = request->u.pm.NATinfo.ExternalAddress.b[2];
	*data++ = request->u.pm.NATinfo.ExternalAddress.b[3];
	*data++ = DNSServiceProtocol(request->u.pm.NATinfo.Protocol);
	*data++ = request->u.pm.NATinfo.IntPort.b[0];
	*data++ = request->u.pm.NATinfo.IntPort.b[1];
	*data++ = request->u.pm.NATinfo.ExternalPort.b[0];
	*data++ = request->u.pm.NATinfo.ExternalPort.b[1];
	put_uint32(request->u.pm.NATinfo.Lifetime, &data);

	LogOperation("%3d: DNSServiceNATPortMappingCreate(%X, %u, %u, %d) RESULT %.4a:%u TTL %u", request->sd,
		DNSServiceProtocol(request->u.pm.NATinfo.Protocol),
		mDNSVal16(request->u.pm.NATinfo.IntPort), mDNSVal16(request->u.pm.ReqExt), request->u.pm.NATinfo.NATLease,
		&request->u.pm.NATinfo.ExternalAddress, mDNSVal16(request->u.pm.NATinfo.ExternalPort), request->u.pm.NATinfo.Lifetime);

	append_reply(request, rep);
	}

mDNSlocal mStatus handle_port_mapping_request(request_state *request)
	{
	mDNSu32 ttl = 0;
	mStatus err = mStatus_NoError;

	DNSServiceFlags flags          = get_flags(&request->msgptr, request->msgend);
	mDNSu32         interfaceIndex = get_uint32(&request->msgptr, request->msgend);
	mDNSInterfaceID InterfaceID    = mDNSPlatformInterfaceIDfromInterfaceIndex(&mDNSStorage, interfaceIndex);
	mDNSu8          protocol       = get_uint32(&request->msgptr, request->msgend);
	(void)flags; // Unused
	if (interfaceIndex && !InterfaceID) return(mStatus_BadParamErr);
	if (request->msgptr + 8 > request->msgend) request->msgptr = NULL;
	else
		{
		request->u.pm.NATinfo.IntPort.b[0] = *request->msgptr++;
		request->u.pm.NATinfo.IntPort.b[1] = *request->msgptr++;
		request->u.pm.ReqExt.b[0]          = *request->msgptr++;
		request->u.pm.ReqExt.b[1]          = *request->msgptr++;
		ttl = get_uint32(&request->msgptr, request->msgend);
		}

	if (!request->msgptr)
		{ LogMsg("%3d: DNSServiceNATPortMappingCreate(unreadable parameters)", request->sd); return(mStatus_BadParamErr); }

	if (protocol == 0)	// If protocol == 0 (i.e. just request public address) then IntPort, ExtPort, ttl must be zero too
		{
		if (!mDNSIPPortIsZero(request->u.pm.NATinfo.IntPort) || !mDNSIPPortIsZero(request->u.pm.ReqExt) || ttl) return(mStatus_BadParamErr);
		}
	else
		{
		if (mDNSIPPortIsZero(request->u.pm.NATinfo.IntPort)) return(mStatus_BadParamErr);
		if (!(protocol & (kDNSServiceProtocol_UDP | kDNSServiceProtocol_TCP))) return(mStatus_BadParamErr);
		}

	request->u.pm.NATinfo.Protocol       = !protocol ? NATOp_AddrRequest : (protocol == kDNSServiceProtocol_UDP) ? NATOp_MapUDP : NATOp_MapTCP;
	//       u.pm.NATinfo.IntPort        = already set above
	request->u.pm.NATinfo.RequestedPort  = request->u.pm.ReqExt;
	request->u.pm.NATinfo.NATLease       = ttl;
	request->u.pm.NATinfo.clientCallback = port_mapping_create_request_callback;
	request->u.pm.NATinfo.clientContext  = request;

	LogOperation("%3d: DNSServiceNATPortMappingCreate(%X, %u, %u, %d) START", request->sd,
		protocol, mDNSVal16(request->u.pm.NATinfo.IntPort), mDNSVal16(request->u.pm.ReqExt), request->u.pm.NATinfo.NATLease);
	err = mDNS_StartNATOperation(&mDNSStorage, &request->u.pm.NATinfo);
	if (err) LogMsg("ERROR: mDNS_StartNATOperation: %d", (int)err);
	else request->terminate = port_mapping_termination_callback;

	return(err);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - DNSServiceGetAddrInfo
#endif

mDNSlocal void addrinfo_termination_callback(request_state *request)
	{
	if (request->u.addrinfo.q4.QuestionContext)
		{
		mDNS_StopQuery(&mDNSStorage, &request->u.addrinfo.q4);
		request->u.addrinfo.q4.QuestionContext = mDNSNULL;
		}

	if (request->u.addrinfo.q6.QuestionContext)
		{
		mDNS_StopQuery(&mDNSStorage, &request->u.addrinfo.q6);
		request->u.addrinfo.q6.QuestionContext = mDNSNULL;
		}
	}

mDNSlocal mStatus handle_addrinfo_request(request_state *request)
	{
	char hostname[256];
	domainname d;
	mStatus err = 0;

	DNSServiceFlags flags = get_flags(&request->msgptr, request->msgend);
	mDNSu32 interfaceIndex = get_uint32(&request->msgptr, request->msgend);

	mDNSPlatformMemZero(&request->u.addrinfo, sizeof(request->u.addrinfo));
	request->u.addrinfo.interface_id = mDNSPlatformInterfaceIDfromInterfaceIndex(&mDNSStorage, interfaceIndex);
	request->u.addrinfo.flags        = flags;
	request->u.addrinfo.protocol     = get_uint32(&request->msgptr, request->msgend);

	if (interfaceIndex && !request->u.addrinfo.interface_id) return(mStatus_BadParamErr);
	if (request->u.addrinfo.protocol > (kDNSServiceProtocol_IPv4|kDNSServiceProtocol_IPv6)) return(mStatus_BadParamErr);

	if (get_string(&request->msgptr, request->msgend, hostname, 256) < 0) return(mStatus_BadParamErr);

	if (!request->msgptr) { LogMsg("%3d: DNSServiceGetAddrInfo(unreadable parameters)", request->sd); return(mStatus_BadParamErr); }

	if (!MakeDomainNameFromDNSNameString(&d, hostname))
		{ LogMsg("ERROR: handle_addrinfo_request: bad hostname: %s", hostname); return(mStatus_BadParamErr); }

#if 0
	if (!AuthorizedDomain(request, &d, AutoBrowseDomains))	return (mStatus_NoError);
#endif

	if (!request->u.addrinfo.protocol)
		{
		NetworkInterfaceInfo *i;
		if (IsLocalDomain(&d))
			{
			for (i = mDNSStorage.HostInterfaces; i; i = i->next)
				{
				if      ((i->ip.type == mDNSAddrType_IPv4) && !mDNSIPv4AddressIsZero(i->ip.ip.v4)) request->u.addrinfo.protocol |= kDNSServiceProtocol_IPv4;
				else if ((i->ip.type == mDNSAddrType_IPv6) && !mDNSIPv6AddressIsZero(i->ip.ip.v6)) request->u.addrinfo.protocol |= kDNSServiceProtocol_IPv6;
				}
			}
		else
			{
			for (i = mDNSStorage.HostInterfaces; i; i = i->next)
				{
				if      ((i->ip.type == mDNSAddrType_IPv4) && !mDNSv4AddressIsLinkLocal(&i->ip.ip.v4)) request->u.addrinfo.protocol |= kDNSServiceProtocol_IPv4;
				else if ((i->ip.type == mDNSAddrType_IPv6) && !mDNSv4AddressIsLinkLocal(&i->ip.ip.v6)) request->u.addrinfo.protocol |= kDNSServiceProtocol_IPv6;
				}
			}
		}

	if (request->u.addrinfo.protocol & kDNSServiceProtocol_IPv4)
		{
		request->u.addrinfo.q4.InterfaceID      = request->u.addrinfo.interface_id;
		request->u.addrinfo.q4.Target           = zeroAddr;
		request->u.addrinfo.q4.qname            = d;
		request->u.addrinfo.q4.qtype            = kDNSServiceType_A;
		request->u.addrinfo.q4.qclass           = kDNSServiceClass_IN;
		request->u.addrinfo.q4.LongLived        = (flags & kDNSServiceFlagsLongLivedQuery     ) != 0;
		request->u.addrinfo.q4.ExpectUnique     = mDNSfalse;
		request->u.addrinfo.q4.ForceMCast       = (flags & kDNSServiceFlagsForceMulticast     ) != 0;
		request->u.addrinfo.q4.ReturnIntermed   = (flags & kDNSServiceFlagsReturnIntermediates) != 0;
		request->u.addrinfo.q4.QuestionCallback = queryrecord_result_callback;
		request->u.addrinfo.q4.QuestionContext  = request;

		err = mDNS_StartQuery(&mDNSStorage, &request->u.addrinfo.q4);
		if (err != mStatus_NoError)
			{
			LogMsg("ERROR: mDNS_StartQuery: %d", (int)err);
			request->u.addrinfo.q4.QuestionContext = mDNSNULL;
			}
		}

	if (!err && (request->u.addrinfo.protocol & kDNSServiceProtocol_IPv6))
		{
		request->u.addrinfo.q6.InterfaceID      = request->u.addrinfo.interface_id;
		request->u.addrinfo.q6.Target           = zeroAddr;
		request->u.addrinfo.q6.qname            = d;
		request->u.addrinfo.q6.qtype            = kDNSServiceType_AAAA;
		request->u.addrinfo.q6.qclass           = kDNSServiceClass_IN;
		request->u.addrinfo.q6.LongLived        = (flags & kDNSServiceFlagsLongLivedQuery     ) != 0;
		request->u.addrinfo.q6.ExpectUnique     = mDNSfalse;
		request->u.addrinfo.q6.ForceMCast       = (flags & kDNSServiceFlagsForceMulticast     ) != 0;
		request->u.addrinfo.q6.ReturnIntermed   = (flags & kDNSServiceFlagsReturnIntermediates) != 0;
		request->u.addrinfo.q6.QuestionCallback = queryrecord_result_callback;
		request->u.addrinfo.q6.QuestionContext  = request;

		err = mDNS_StartQuery(&mDNSStorage, &request->u.addrinfo.q6);
		if (err != mStatus_NoError)
			{
			LogMsg("ERROR: mDNS_StartQuery: %d", (int)err);
			request->u.addrinfo.q6.QuestionContext = mDNSNULL;
			if (request->u.addrinfo.protocol & kDNSServiceProtocol_IPv4)	// If we started a query for IPv4,
				addrinfo_termination_callback(request);						// we need to cancel it
			}
		}

	LogOperation("%3d: DNSServiceGetAddrInfo(%##s) START", request->sd, d.c);

	if (!err) request->terminate = addrinfo_termination_callback;

	return(err);
	}

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Main Request Handler etc.
#endif

mDNSlocal request_state *NewRequest(void)
	{
	request_state **p = &all_requests;
	while (*p) p=&(*p)->next;
	*p = mallocL("request_state", sizeof(request_state));
	if (!*p) FatalError("ERROR: malloc");
	mDNSPlatformMemZero(*p, sizeof(request_state));
	return(*p);
	}

// read_msg may be called any time when the transfer state (req->ts) is t_morecoming.
// if there is no data on the socket, the socket will be closed and t_terminated will be returned
mDNSlocal void read_msg(request_state *req)
	{
	if (req->ts == t_terminated || req->ts == t_error)
		{ LogMsg("%3d: ERROR: read_msg called with transfer state terminated or error", req->sd); req->ts = t_error; return; }

	if (req->ts == t_complete)	// this must be death or something is wrong
		{
		char buf[4];	// dummy for death notification
		int nread = recv(req->sd, buf, 4, 0);
		if (!nread) { req->ts = t_terminated; return; }
		if (nread < 0) goto rerror;
		LogMsg("%3d: ERROR: read data from a completed request", req->sd);
		req->ts = t_error;
		return;
		}

	if (req->ts != t_morecoming)
		{ LogMsg("%3d: ERROR: read_msg called with invalid transfer state (%d)", req->sd, req->ts); req->ts = t_error; return; }

	if (req->hdr_bytes < sizeof(ipc_msg_hdr))
		{
		mDNSu32 nleft = sizeof(ipc_msg_hdr) - req->hdr_bytes;
		int nread = recv(req->sd, (char *)&req->hdr + req->hdr_bytes, nleft, 0);
		if (nread == 0) { req->ts = t_terminated; return; }
		if (nread < 0) goto rerror;
		req->hdr_bytes += nread;
		if (req->hdr_bytes > sizeof(ipc_msg_hdr))
			{ LogMsg("%3d: ERROR: read_msg - read too many header bytes", req->sd); req->ts = t_error; return; }

		// only read data if header is complete
		if (req->hdr_bytes == sizeof(ipc_msg_hdr))
			{
			ConvertHeaderBytes(&req->hdr);
			if (req->hdr.version != VERSION)
				{ LogMsg("%3d: ERROR: client version 0x%08X daemon version 0x%08X", req->sd, req->hdr.version, VERSION); req->ts = t_error; return; }

			// Largest conceivable single request is a DNSServiceRegisterRecord() or DNSServiceAddRecord()
			// with 64kB of rdata. Adding 1009 byte for a maximal domain name, plus a safety margin
			// for other overhead, this means any message above 70kB is definitely bogus.
			if (req->hdr.datalen > 70000)
				{ LogMsg("%3d: ERROR: read_msg - hdr.datalen %lu (%X) > 70000", req->sd, req->hdr.datalen, req->hdr.datalen); req->ts = t_error; return; }
			req->msgbuf = mallocL("request_state msgbuf", req->hdr.datalen + MSG_PAD_BYTES);
			if (!req->msgbuf) { my_perror("ERROR: malloc"); req->ts = t_error; return; }
			req->msgptr = req->msgbuf;
			req->msgend = req->msgbuf + req->hdr.datalen;
			mDNSPlatformMemZero(req->msgbuf, req->hdr.datalen + MSG_PAD_BYTES);
			}
		}

	// If our header is complete, but we're still needing more body data, then try to read it now
	// Note: For cancel_request req->hdr.datalen == 0, but there's no error return socket for cancel_request
	// Any time we need to get the error return socket we know we'll have at least one data byte
	// (even if only the one-byte empty C string placeholder for the old ctrl_path parameter)
	if (req->hdr_bytes == sizeof(ipc_msg_hdr) && req->data_bytes < req->hdr.datalen)
		{
		mDNSu32 nleft = req->hdr.datalen - req->data_bytes;
		int nread;
#if !defined(_WIN32)
		struct iovec vec = { req->msgbuf + req->data_bytes, nleft };	// Tell recvmsg where we want the bytes put
		struct msghdr msg;
		struct cmsghdr *cmsg;
		char cbuf[CMSG_SPACE(sizeof(dnssd_sock_t))];
		msg.msg_name       = 0;
		msg.msg_namelen    = 0;
		msg.msg_iov        = &vec;
		msg.msg_iovlen     = 1;
		msg.msg_control    = cbuf;
		msg.msg_controllen = sizeof(cbuf);
		msg.msg_flags      = 0;
		nread = recvmsg(req->sd, &msg, 0);
#else
		nread = recv(req->sd, (char *)req->msgbuf + req->data_bytes, nleft, 0);
#endif
		if (nread == 0) { req->ts = t_terminated; return; }
		if (nread < 0) goto rerror;
		req->data_bytes += nread;
		if (req->data_bytes > req->hdr.datalen)
			{ LogMsg("%3d: ERROR: read_msg - read too many data bytes", req->sd); req->ts = t_error; return; }
#if !defined(_WIN32)
		cmsg = CMSG_FIRSTHDR(&msg);
#if DEBUG_64BIT_SCM_RIGHTS
		LogMsg("%3d: Expecting %d %d %d %d", req->sd, sizeof(cbuf),       sizeof(cbuf),   SOL_SOCKET,       SCM_RIGHTS);
		LogMsg("%3d: Got       %d %d %d %d", req->sd, msg.msg_controllen, cmsg->cmsg_len, cmsg->cmsg_level, cmsg->cmsg_type);
#endif // DEBUG_64BIT_SCM_RIGHTS
		if (msg.msg_controllen == sizeof(cbuf) &&
			cmsg->cmsg_len     == sizeof(cbuf) &&
			cmsg->cmsg_level   == SOL_SOCKET   &&
			cmsg->cmsg_type    == SCM_RIGHTS)
			{
#if APPLE_OSX_mDNSResponder
			// Strictly speaking BPF_fd belongs solely in the platform support layer, but because
			// of privilege separation on Mac OS X we need to get BPF_fd from mDNSResponderHelper,
			// and it's convenient to repurpose the existing fd-passing code here for that task
			if (req->hdr.op == send_bpf)
				{
				dnssd_sock_t x = *(dnssd_sock_t *)CMSG_DATA(cmsg);
				LogOperation("%3d: Got BPF %d", req->sd, x);
				mDNSPlatformReceiveBPF_fd(&mDNSStorage, x);
				}
			else
#endif // APPLE_OSX_mDNSResponder
				req->errsd = *(dnssd_sock_t *)CMSG_DATA(cmsg);
#if DEBUG_64BIT_SCM_RIGHTS
			LogMsg("%3d: read req->errsd %d", req->sd, req->errsd);
#endif // DEBUG_64BIT_SCM_RIGHTS
			if (req->data_bytes < req->hdr.datalen)
				{
				LogMsg("%3d: Client sent error socket %d via SCM_RIGHTS with req->data_bytes %d < req->hdr.datalen %d",
					req->sd, req->errsd, req->data_bytes, req->hdr.datalen);
				req->ts = t_error;
				return;
				}
			}
#endif
		}

	// If our header and data are both complete, see if we need to make our separate error return socket
	if (req->hdr_bytes == sizeof(ipc_msg_hdr) && req->data_bytes == req->hdr.datalen)
		{
		if (req->terminate && req->hdr.op != cancel_request)
			{
			dnssd_sockaddr_t cliaddr;
#if defined(USE_TCP_LOOPBACK)
			mDNSOpaque16 port;
			int opt = 1;
			port.b[0] = req->msgptr[0];
			port.b[1] = req->msgptr[1];
			req->msgptr += 2;
			cliaddr.sin_family      = AF_INET;
			cliaddr.sin_port        = port.NotAnInteger;
			cliaddr.sin_addr.s_addr = inet_addr(MDNS_TCP_SERVERADDR);
#else
			char ctrl_path[MAX_CTLPATH];
			get_string(&req->msgptr, req->msgend, ctrl_path, MAX_CTLPATH);	// path is first element in message buffer
			mDNSPlatformMemZero(&cliaddr, sizeof(cliaddr));
			cliaddr.sun_family = AF_LOCAL;
			mDNSPlatformStrCopy(cliaddr.sun_path, ctrl_path);
			// If the error return path UDS name is empty string, that tells us
			// that this is a new version of the library that's going to pass us
			// the error return path socket via sendmsg/recvmsg
			if (ctrl_path[0] == 0)
				{
				if (req->errsd == req->sd)
					{ LogMsg("%3d: read_msg: ERROR failed to get errsd via SCM_RIGHTS", req->sd); req->ts = t_error; return; }
				goto got_errfd;
				}
#endif
	
			req->errsd = socket(AF_DNSSD, SOCK_STREAM, 0);
			if (!dnssd_SocketValid(req->errsd)) { my_perror("ERROR: socket"); req->ts = t_error; return; }

			if (connect(req->errsd, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0)
				{
#if !defined(USE_TCP_LOOPBACK)
				struct stat sb;
				LogMsg("%3d: read_msg: Couldn't connect to error return path socket “%s” errno %d (%s)",
					req->sd, cliaddr.sun_path, dnssd_errno, dnssd_strerror(dnssd_errno));
				if (stat(cliaddr.sun_path, &sb) < 0)
					LogMsg("%3d: read_msg: stat failed “%s” errno %d (%s)", req->sd, cliaddr.sun_path, dnssd_errno, dnssd_strerror(dnssd_errno));
				else
					LogMsg("%3d: read_msg: file “%s” mode %o (octal) uid %d gid %d", req->sd, cliaddr.sun_path, sb.st_mode, sb.st_uid, sb.st_gid);
#endif
				req->ts = t_error;
				return;
				}
	
got_errfd:
			LogOperation("%3d: Error socket %d created %08X %08X", req->sd, req->errsd, req->hdr.client_context.u32[1], req->hdr.client_context.u32[0]);
#if defined(_WIN32)
			if (ioctlsocket(req->errsd, FIONBIO, &opt) != 0)
#else
			if (fcntl(req->errsd, F_SETFL, fcntl(req->errsd, F_GETFL, 0) | O_NONBLOCK) != 0)
#endif
				{
				LogMsg("%3d: ERROR: could not set control socket to non-blocking mode errno %d (%s)",
					req->sd, dnssd_errno, dnssd_strerror(dnssd_errno));
				req->ts = t_error;
				return;
				}
			}
		
		req->ts = t_complete;
		}

	return;

rerror:
	if (dnssd_errno == dnssd_EWOULDBLOCK || dnssd_errno == dnssd_EINTR) return;
	LogMsg("%3d: ERROR: read_msg errno %d (%s)", req->sd, dnssd_errno, dnssd_strerror(dnssd_errno));
	req->ts = t_error;
	}

#define RecordOrientedOp(X) \
	((X) == reg_record_request || (X) == add_record_request || (X) == update_record_request || (X) == remove_record_request)

// The lightweight operations are the ones that don't need a dedicated request_state structure allocated for them
#define LightweightOp(X) (RecordOrientedOp(X) || (X) == cancel_request)

mDNSlocal void request_callback(int fd, short filter, void *info)
	{
	mStatus err = 0;
	request_state *req = info;
#if defined(_WIN32)
	u_long opt = 1;
#endif
	mDNSs32 min_size = sizeof(DNSServiceFlags);
	(void)fd; // Unused
	(void)filter; // Unused

	read_msg(req);
	if (req->ts == t_morecoming) return;
	if (req->ts == t_terminated || req->ts == t_error) { AbortUnlinkAndFree(req); return; }
	if (req->ts != t_complete) { LogMsg("req->ts %d != t_complete", req->ts); AbortUnlinkAndFree(req); return; }

	if (req->hdr.version != VERSION)
		{
		LogMsg("ERROR: client version %d incompatible with daemon version %d", req->hdr.version, VERSION);
		AbortUnlinkAndFree(req);
		return;
		}

	switch(req->hdr.op)            //          Interface       + other data
		{
		case connection_request:       min_size = 0;                                                                           break;
		case reg_service_request:      min_size += sizeof(mDNSu32) + 4 /* name, type, domain, host */ + 4 /* port, textlen */; break;
		case add_record_request:       min_size +=                   4 /* type, rdlen */              + 4 /* ttl */;           break;
		case update_record_request:    min_size +=                   2 /* rdlen */                    + 4 /* ttl */;           break;
		case remove_record_request:                                                                                            break;
		case browse_request:           min_size += sizeof(mDNSu32) + 2 /* type, domain */;                                     break;
		case resolve_request:          min_size += sizeof(mDNSu32) + 3 /* type, type, domain */;                               break;
		case query_request:            min_size += sizeof(mDNSu32) + 1 /* name */                     + 4 /* type, class*/;    break;
		case enumeration_request:      min_size += sizeof(mDNSu32);                                                            break;
		case reg_record_request:       min_size += sizeof(mDNSu32) + 1 /* name */ + 6 /* type, class, rdlen */ + 4 /* ttl */;  break;
		case reconfirm_record_request: min_size += sizeof(mDNSu32) + 1 /* name */ + 6 /* type, class, rdlen */;                break;
		case setdomain_request:        min_size +=                   1 /* domain */;                                           break;
		case getproperty_request:      min_size = 2;                                                                           break;
		case port_mapping_request:     min_size += sizeof(mDNSu32) + 4 /* udp/tcp */ + 4 /* int/ext port */    + 4 /* ttl */;  break;
		case addrinfo_request:         min_size += sizeof(mDNSu32) + 4 /* v4/v6 */   + 1 /* hostname */;                       break;
		case send_bpf:                 // Same as cancel_request below
		case cancel_request:           min_size = 0;                                                                           break;
		default: LogMsg("ERROR: validate_message - unsupported req type: %d", req->hdr.op); min_size = -1;                     break;
		}

	if ((mDNSs32)req->data_bytes < min_size)
		{ LogMsg("Invalid message %d bytes; min for %d is %d", req->data_bytes, req->hdr.op, min_size); AbortUnlinkAndFree(req); return; }

	if (LightweightOp(req->hdr.op) && !req->terminate)
		{ LogMsg("Reg/Add/Update/Remove %d require existing connection", req->hdr.op);                  AbortUnlinkAndFree(req); return; }

	// check if client wants silent operation
	if (req->hdr.ipc_flags & IPC_FLAGS_NOREPLY) req->no_reply = 1;

	// If req->terminate is already set, this means this operation is sharing an existing connection
	if (req->terminate && !LightweightOp(req->hdr.op))
		{
		request_state *newreq = NewRequest();
		newreq->primary = req;
		newreq->sd      = req->sd;
		newreq->errsd   = req->errsd;
		newreq->uid     = req->uid;
		newreq->hdr     = req->hdr;
		newreq->msgbuf  = req->msgbuf;
		newreq->msgptr  = req->msgptr;
		newreq->msgend  = req->msgend;
		req = newreq;
		}

	// If we're shutting down, don't allow new client requests
	// We do allow "cancel" and "getproperty" during shutdown
	if (mDNSStorage.ShutdownTime && req->hdr.op != cancel_request && req->hdr.op != getproperty_request)
		{
		err = mStatus_ServiceNotRunning;
		}
	else switch(req->hdr.op)
		{
		// These are all operations that have their own first-class request_state object
		case connection_request:           LogOperation("%3d: DNSServiceCreateConnection START", req->sd);
			                               req->terminate = connection_termination; break;
		case resolve_request:              err = handle_resolve_request     (req);  break;
		case query_request:                err = handle_queryrecord_request (req);  break;
		case browse_request:               err = handle_browse_request      (req);  break;
		case reg_service_request:          err = handle_regservice_request  (req);  break;
		case enumeration_request:          err = handle_enum_request        (req);  break;
		case reconfirm_record_request:     err = handle_reconfirm_request   (req);  break;
		case setdomain_request:            err = handle_setdomain_request   (req);  break;
		case getproperty_request:                handle_getproperty_request (req);  break;
		case port_mapping_request:         err = handle_port_mapping_request(req);  break;
		case addrinfo_request:             err = handle_addrinfo_request    (req);  break;
		case send_bpf:                     /* Do nothing for send_bpf */            break;

		// These are all operations that work with an existing request_state object
		case reg_record_request:           err = handle_regrecord_request   (req);  break;
		case add_record_request:           err = handle_add_request         (req);  break;
		case update_record_request:        err = handle_update_request      (req);  break;
		case remove_record_request:        err = handle_removerecord_request(req);  break;
		case cancel_request:                     handle_cancel_request      (req);  break;
		default: LogMsg("%3d: ERROR: Unsupported UDS req: %d", req->sd, req->hdr.op);
		}

	// req->msgbuf may be NULL, e.g. for connection_request or remove_record_request
	if (req->msgbuf) freeL("request_state msgbuf", req->msgbuf);

	// There's no return data for a cancel request (DNSServiceRefDeallocate returns no result)
	// For a DNSServiceGetProperty call, the handler already generated the response, so no need to do it again here
	if (req->hdr.op != cancel_request && req->hdr.op != getproperty_request && req->hdr.op != send_bpf)
		{
		const mStatus err_netorder = dnssd_htonl(err);
		send_all(req->errsd, (const char *)&err_netorder, sizeof(err_netorder));
		if (req->errsd != req->sd)
			{
			LogOperation("%3d: Error socket %d closed  %08X %08X (%d)",
				req->sd, req->errsd, req->hdr.client_context.u32[1], req->hdr.client_context.u32[0], err);
			dnssd_close(req->errsd);
			req->errsd = req->sd;
			// Also need to reset the parent's errsd, if this is a subordinate operation
			if (req->primary) req->primary->errsd = req->primary->sd;
			}
		}

	// Reset ready to accept the next req on this pipe
	if (req->primary) req = req->primary;
	req->ts         = t_morecoming;
	req->hdr_bytes  = 0;
	req->data_bytes = 0;
	req->msgbuf     = mDNSNULL;
	req->msgptr     = mDNSNULL;
	req->msgend     = 0;
	}

mDNSlocal void connect_callback(int fd, short filter, void *info)
	{
	dnssd_sockaddr_t cliaddr;
	dnssd_socklen_t len = (dnssd_socklen_t) sizeof(cliaddr);
	dnssd_sock_t sd = accept(fd, (struct sockaddr*) &cliaddr, &len);
#if defined(SO_NOSIGPIPE) || defined(_WIN32)
	const unsigned long optval = 1;
#endif

	(void)filter; // Unused
	(void)info; // Unused

	if (!dnssd_SocketValid(sd))
		{
		if (dnssd_errno != dnssd_EWOULDBLOCK) my_perror("ERROR: accept");
		return;
		}

#ifdef SO_NOSIGPIPE
	// Some environments (e.g. OS X) support turning off SIGPIPE for a socket
	if (setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) < 0)
		LogMsg("%3d: WARNING: setsockopt - SO_NOSIGPIPE %d (%s)", sd, dnssd_errno, dnssd_strerror(dnssd_errno));
#endif

#if defined(_WIN32)
	if (ioctlsocket(sd, FIONBIO, &optval) != 0)
#else
	if (fcntl(sd, F_SETFL, fcntl(sd, F_GETFL, 0) | O_NONBLOCK) != 0)
#endif
		{
		my_perror("ERROR: fcntl(sd, F_SETFL, O_NONBLOCK) - aborting client");
		dnssd_close(sd);
		return;
		}
	else
		{
		request_state *request = NewRequest();
		request->ts    = t_morecoming;
		request->sd    = sd;
		request->errsd = sd;
#if APPLE_OSX_mDNSResponder
		struct xucred x;
		socklen_t xucredlen = sizeof(x);
		if (getsockopt(sd, 0, LOCAL_PEERCRED, &x, &xucredlen) >= 0 && x.cr_version == XUCRED_VERSION) request->uid = x.cr_uid;
		else my_perror("ERROR: getsockopt, LOCAL_PEERCRED");
		debugf("LOCAL_PEERCRED %d %u %u %d", xucredlen, x.cr_version, x.cr_uid, x.cr_ngroups);
#endif // APPLE_OSX_mDNSResponder
		LogOperation("%3d: Adding FD for uid %u", request->sd, request->uid);
		udsSupportAddFDToEventLoop(sd, request_callback, request);
		}
	}

mDNSlocal mDNSBool uds_socket_setup(dnssd_sock_t skt)
	{
#if defined(SO_NP_EXTENSIONS)
	struct		so_np_extensions sonpx;
	socklen_t 	optlen = sizeof(struct so_np_extensions);
	sonpx.npx_flags = SONPX_SETOPTSHUT;
	sonpx.npx_mask  = SONPX_SETOPTSHUT;
	if (setsockopt(skt, SOL_SOCKET, SO_NP_EXTENSIONS, &sonpx, optlen) < 0)
		my_perror("WARNING: could not set sockopt - SO_NP_EXTENSIONS");
#endif
#if defined(_WIN32)
	// SEH: do we even need to do this on windows?
	// This socket will be given to WSAEventSelect which will automatically set it to non-blocking
	u_long opt = 1;
	if (ioctlsocket(skt, FIONBIO, &opt) != 0)
#else
	if (fcntl(skt, F_SETFL, fcntl(skt, F_GETFL, 0) | O_NONBLOCK) != 0)
#endif
		{
		my_perror("ERROR: could not set listen socket to non-blocking mode");
		return mDNSfalse;
		}

	if (listen(skt, LISTENQ) != 0)
		{
		my_perror("ERROR: could not listen on listen socket");
		return mDNSfalse;
		}

	if (mStatus_NoError != udsSupportAddFDToEventLoop(skt, connect_callback, (void *) NULL))
		{
		my_perror("ERROR: could not add listen socket to event loop");
		return mDNSfalse;
		}
	else LogOperation("%3d: Listening for incoming Unix Domain Socket client requests", skt);
	
	return mDNStrue;
	}

mDNSexport int udsserver_init(dnssd_sock_t skts[], mDNSu32 count)
	{
	dnssd_sockaddr_t laddr;
	int ret;
	mDNSu32 i = 0;
#if defined(_WIN32)
	u_long opt = 1;
#endif

	LogInfo("udsserver_init");

	// If a particular platform wants to opt out of having a PID file, define PID_FILE to be ""
	if (PID_FILE[0])
		{
		FILE *fp = fopen(PID_FILE, "w");
		if (fp != NULL)
			{
			fprintf(fp, "%d\n", getpid());
			fclose(fp);
			}
		}

	if (skts)
		{
		for (i = 0; i < count; i++)
			if (dnssd_SocketValid(skts[i]) && !uds_socket_setup(skts[i]))
				goto error;
		}
	else
		{
		listenfd = socket(AF_DNSSD, SOCK_STREAM, 0);
		if (!dnssd_SocketValid(listenfd))
			{
			my_perror("ERROR: socket(AF_DNSSD, SOCK_STREAM, 0); failed");
			goto error;
			}

		mDNSPlatformMemZero(&laddr, sizeof(laddr));

		#if defined(USE_TCP_LOOPBACK)
			{
			laddr.sin_family = AF_INET;
			laddr.sin_port = htons(MDNS_TCP_SERVERPORT);
			laddr.sin_addr.s_addr = inet_addr(MDNS_TCP_SERVERADDR);
			ret = bind(listenfd, (struct sockaddr *) &laddr, sizeof(laddr));
			if (ret < 0)
				{
				my_perror("ERROR: bind(listenfd, (struct sockaddr *) &laddr, sizeof(laddr)); failed");
				goto error;
				}
			}
		#else
			{
			mode_t mask = umask(0);
			unlink(MDNS_UDS_SERVERPATH);  // OK if this fails
			laddr.sun_family = AF_LOCAL;
			#ifndef NOT_HAVE_SA_LEN
			// According to Stevens (section 3.2), there is no portable way to
			// determine whether sa_len is defined on a particular platform.
			laddr.sun_len = sizeof(struct sockaddr_un);
			#endif
			mDNSPlatformStrCopy(laddr.sun_path, MDNS_UDS_SERVERPATH);
			ret = bind(listenfd, (struct sockaddr *) &laddr, sizeof(laddr));
			umask(mask);
			if (ret < 0)
				{
				my_perror("ERROR: bind(listenfd, (struct sockaddr *) &laddr, sizeof(laddr)); failed");
				goto error;
				}
			}
		#endif
		
		if (!uds_socket_setup(listenfd)) goto error;
		}

#if !defined(PLATFORM_NO_RLIMIT)
	{
	// Set maximum number of open file descriptors
	#define MIN_OPENFILES 10240
	struct rlimit maxfds, newfds;

	// Due to bugs in OS X (<rdar://problem/2941095>, <rdar://problem/3342704>, <rdar://problem/3839173>)
	// you have to get and set rlimits once before getrlimit will return sensible values
	if (getrlimit(RLIMIT_NOFILE, &maxfds) < 0) { my_perror("ERROR: Unable to get file descriptor limit"); return 0; }
	if (setrlimit(RLIMIT_NOFILE, &maxfds) < 0) my_perror("ERROR: Unable to set maximum file descriptor limit");

	if (getrlimit(RLIMIT_NOFILE, &maxfds) < 0) { my_perror("ERROR: Unable to get file descriptor limit"); return 0; }
	newfds.rlim_max = (maxfds.rlim_max > MIN_OPENFILES) ? maxfds.rlim_max : MIN_OPENFILES;
	newfds.rlim_cur = (maxfds.rlim_cur > MIN_OPENFILES) ? maxfds.rlim_cur : MIN_OPENFILES;
	if (newfds.rlim_max != maxfds.rlim_max || newfds.rlim_cur != maxfds.rlim_cur)
		if (setrlimit(RLIMIT_NOFILE, &newfds) < 0) my_perror("ERROR: Unable to set maximum file descriptor limit");

	if (getrlimit(RLIMIT_NOFILE, &maxfds) < 0) { my_perror("ERROR: Unable to get file descriptor limit"); return 0; }
	debugf("maxfds.rlim_max %d", (long)maxfds.rlim_max);
	debugf("maxfds.rlim_cur %d", (long)maxfds.rlim_cur);
	}
#endif

	// We start a "LocalOnly" query looking for Automatic Browse Domain records.
	// When Domain Enumeration in uDNS.c finds an "lb" record from the network, it creates a
	// "LocalOnly" record, which results in our AutomaticBrowseDomainChange callback being invoked
	mDNS_GetDomains(&mDNSStorage, &mDNSStorage.AutomaticBrowseDomainQ, mDNS_DomainTypeBrowseAutomatic,
		mDNSNULL, mDNSInterface_LocalOnly, AutomaticBrowseDomainChange, mDNSNULL);

	// Add "local" as recommended registration domain ("dns-sd -E"), recommended browsing domain ("dns-sd -F"), and automatic browsing domain
	RegisterLocalOnlyDomainEnumPTR(&mDNSStorage, &localdomain, mDNS_DomainTypeRegistration);
	RegisterLocalOnlyDomainEnumPTR(&mDNSStorage, &localdomain, mDNS_DomainTypeBrowse);
	AddAutoBrowseDomain(0, &localdomain);

	udsserver_handle_configchange(&mDNSStorage);
	return 0;

error:

	my_perror("ERROR: udsserver_init");
	return -1;
	}

mDNSexport int udsserver_exit(void)
	{
	// If the launching environment created no listening socket,
	// that means we created it ourselves, so we should clean it up on exit
	if (dnssd_SocketValid(listenfd))
		{
		dnssd_close(listenfd);
#if !defined(USE_TCP_LOOPBACK)
		// Currently, we're unable to remove /var/run/mdnsd because we've changed to userid "nobody"
		// to give up unnecessary privilege, but we need to be root to remove this Unix Domain Socket.
		// It would be nice if we could find a solution to this problem
		if (unlink(MDNS_UDS_SERVERPATH))
			debugf("Unable to remove %s", MDNS_UDS_SERVERPATH);
#endif
		}

	if (PID_FILE[0]) unlink(PID_FILE);

	return 0;
	}

mDNSlocal void LogClientInfo(mDNS *const m, request_state *req)
	{
	if (!req->terminate)
		LogMsgNoIdent("%3d: No operation yet on this socket", req->sd);
	else if (req->terminate == connection_termination)
		{
		registered_record_entry *p;
		LogMsgNoIdent("%3d: DNSServiceCreateConnection", req->sd);
		for (p = req->u.reg_recs; p; p=p->next)
			LogMsgNoIdent(" ->  DNSServiceRegisterRecord %3d %s", p->key, ARDisplayString(m, p->rr));
		}
	else if (req->terminate == regservice_termination_callback)
		{
		service_instance *ptr;
		for (ptr = req->u.servicereg.instances; ptr; ptr = ptr->next)
			LogMsgNoIdent("%3d: DNSServiceRegister         %##s %u/%u",
				req->sd, ptr->srs.RR_SRV.resrec.name->c, mDNSVal16(req->u.servicereg.port), SRS_PORT(&ptr->srs));
		}
	else if (req->terminate == browse_termination_callback)
		{
		browser_t *blist;
		for (blist = req->u.browser.browsers; blist; blist = blist->next)
			LogMsgNoIdent("%3d: DNSServiceBrowse           %##s", req->sd, blist->q.qname.c);
		}
	else if (req->terminate == resolve_termination_callback)
		LogMsgNoIdent("%3d: DNSServiceResolve          %##s", req->sd, req->u.resolve.qsrv.qname.c);
	else if (req->terminate == queryrecord_termination_callback)
		LogMsgNoIdent("%3d: DNSServiceQueryRecord      %##s (%s)", req->sd, req->u.queryrecord.q.qname.c, DNSTypeName(req->u.queryrecord.q.qtype));
	else if (req->terminate == enum_termination_callback)
		LogMsgNoIdent("%3d: DNSServiceEnumerateDomains %##s", req->sd, req->u.enumeration.q_all.qname.c);
	else if (req->terminate == port_mapping_termination_callback)
		LogMsgNoIdent("%3d: DNSServiceNATPortMapping   %.4a %s%s Int %d Req %d Ext %d Req TTL %d Granted TTL %d",
			req->sd,
			&req->u.pm.NATinfo.ExternalAddress,
			req->u.pm.NATinfo.Protocol & NATOp_MapTCP ? "TCP" : "   ",
			req->u.pm.NATinfo.Protocol & NATOp_MapUDP ? "UDP" : "   ",
			mDNSVal16(req->u.pm.NATinfo.IntPort),
			mDNSVal16(req->u.pm.ReqExt),
			mDNSVal16(req->u.pm.NATinfo.ExternalPort),
			req->u.pm.NATinfo.NATLease,
			req->u.pm.NATinfo.Lifetime);
	else if (req->terminate == addrinfo_termination_callback)
		LogMsgNoIdent("%3d: DNSServiceGetAddrInfo      %s%s %##s", req->sd,
			req->u.addrinfo.protocol & kDNSServiceProtocol_IPv4 ? "v4" : "  ",
			req->u.addrinfo.protocol & kDNSServiceProtocol_IPv6 ? "v6" : "  ",
			req->u.addrinfo.q4.qname.c);
	else
		LogMsgNoIdent("%3d: Unrecognized operation %p", req->sd, req->terminate);
	}

mDNSlocal void LogAuthRecords(mDNS *const m, const mDNSs32 now, AuthRecord *ResourceRecords, int *proxy)
	{
	if (!ResourceRecords) LogMsgNoIdent("<None>");
	else
		{
		const AuthRecord *ar;
		mDNSEthAddr owner = zeroEthAddr;
		LogMsgNoIdent("    Int    Next  Expire   State");
		for (ar = ResourceRecords; ar; ar=ar->next)
			{
			char *ifname = InterfaceNameForID(m, ar->resrec.InterfaceID);
			if (ar->WakeUp.HMAC.l[0]) (*proxy)++;
			if (!mDNSSameEthAddress(&owner, &ar->WakeUp.HMAC))
				{
				owner = ar->WakeUp.HMAC;
				if (ar->WakeUp.password.l[0])
					LogMsgNoIdent("Proxying for H-MAC %.6a I-MAC %.6a Password %.6a seq %d", &ar->WakeUp.HMAC, &ar->WakeUp.IMAC, &ar->WakeUp.password, ar->WakeUp.seq);
				else if (!mDNSSameEthAddress(&ar->WakeUp.HMAC, &ar->WakeUp.IMAC))
					LogMsgNoIdent("Proxying for H-MAC %.6a I-MAC %.6a seq %d",               &ar->WakeUp.HMAC, &ar->WakeUp.IMAC,                       ar->WakeUp.seq);
				else
					LogMsgNoIdent("Proxying for %.6a seq %d",                                &ar->WakeUp.HMAC,                                         ar->WakeUp.seq);
				}
			if (AuthRecord_uDNS(ar))
				LogMsgNoIdent("%7d %7d %7d %7d %s",
					ar->ThisAPInterval / mDNSPlatformOneSecond,
					(ar->LastAPTime + ar->ThisAPInterval - now) / mDNSPlatformOneSecond,
					ar->expire ? (ar->expire - now) / mDNSPlatformOneSecond : 0,
					ar->state, ARDisplayString(m, ar));
			else if (ar->resrec.InterfaceID != mDNSInterface_LocalOnly)
				LogMsgNoIdent("%7d %7d %7d %7s %s",
					ar->ThisAPInterval / mDNSPlatformOneSecond,
					ar->AnnounceCount ? (ar->LastAPTime + ar->ThisAPInterval - now) / mDNSPlatformOneSecond : 0,
					ar->TimeExpire    ? (ar->TimeExpire                      - now) / mDNSPlatformOneSecond : 0,
					ifname ? ifname : "ALL",
					ARDisplayString(m, ar));
			else
				LogMsgNoIdent("                             LO %s", ARDisplayString(m, ar));
			usleep((m->KnownBugs & mDNS_KnownBug_LossySyslog) ? 3333 : 1000);
			}
		}
	}

mDNSexport void udsserver_info(mDNS *const m)
	{
	const mDNSs32 now = mDNS_TimeNow(m);
	mDNSu32 CacheUsed = 0, CacheActive = 0, slot;
	int ProxyA = 0, ProxyD = 0;
	const CacheGroup *cg;
	const CacheRecord *cr;
	const DNSQuestion *q;
	const DNameListElem *d;

	LogMsgNoIdent("Timenow 0x%08lX (%d)", (mDNSu32)now, now);
	LogMsgNoIdent("------------ Cache -------------");

	LogMsgNoIdent("Slt Q     TTL if     U Type rdlen");
	for (slot = 0; slot < CACHE_HASH_SLOTS; slot++)
		for (cg = m->rrcache_hash[slot]; cg; cg=cg->next)
			{
			CacheUsed++;	// Count one cache entity for the CacheGroup object
			for (cr = cg->members; cr; cr=cr->next)
				{
				mDNSs32 remain = cr->resrec.rroriginalttl - (now - cr->TimeRcvd) / mDNSPlatformOneSecond;
				char *ifname = InterfaceNameForID(m, cr->resrec.InterfaceID);
				CacheUsed++;
				if (cr->CRActiveQuestion) CacheActive++;
				LogMsgNoIdent("%3d %s%8ld %-7s%s %-6s%s",
					slot,
					cr->CRActiveQuestion ? "*" : " ",
					remain,
					ifname ? ifname : "-U-",
					(cr->resrec.RecordType == kDNSRecordTypePacketNegative)  ? "-" :
					(cr->resrec.RecordType & kDNSRecordTypePacketUniqueMask) ? " " : "+",
					DNSTypeName(cr->resrec.rrtype),
					CRDisplayString(m, cr));
				usleep((m->KnownBugs & mDNS_KnownBug_LossySyslog) ? 3333 : 1000);
				}
			}

	if (m->rrcache_totalused != CacheUsed)
		LogMsgNoIdent("Cache use mismatch: rrcache_totalused is %lu, true count %lu", m->rrcache_totalused, CacheUsed);
	if (m->rrcache_active != CacheActive)
		LogMsgNoIdent("Cache use mismatch: rrcache_active is %lu, true count %lu", m->rrcache_active, CacheActive);
	LogMsgNoIdent("Cache currently contains %lu entities; %lu referenced by active questions", CacheUsed, CacheActive);

	LogMsgNoIdent("--------- Auth Records ---------");
	LogAuthRecords(m, now, m->ResourceRecords, &ProxyA);

	LogMsgNoIdent("------ Duplicate Records -------");
	LogAuthRecords(m, now, m->DuplicateRecords, &ProxyD);

	LogMsgNoIdent("----- ServiceRegistrations -----");
	if (!m->ServiceRegistrations) LogMsgNoIdent("<None>");
	else
		{
		ServiceRecordSet *s;
		LogMsgNoIdent("    Int    Next  Expire   State");
		for (s = m->ServiceRegistrations; s; s = s->uDNS_next)
			LogMsgNoIdent("%7d %7d %7d %7d %s",
				s->RR_SRV.ThisAPInterval / mDNSPlatformOneSecond,
				(s->RR_SRV.LastAPTime + s->RR_SRV.ThisAPInterval - now) / mDNSPlatformOneSecond,
				s->RR_SRV.expire ? (s->RR_SRV.expire - now) / mDNSPlatformOneSecond : 0,
				s->state, ARDisplayString(m, &s->RR_SRV));
		}

	LogMsgNoIdent("---------- Questions -----------");
	if (!m->Questions) LogMsgNoIdent("<None>");
	else
		{
		CacheUsed = 0;
		CacheActive = 0;
		LogMsgNoIdent("   Int  Next if     T  NumAns Type  Name");
		for (q = m->Questions; q; q=q->next)
			{
			mDNSs32 i = q->ThisQInterval / mDNSPlatformOneSecond;
			mDNSs32 n = (q->LastQTime + q->ThisQInterval - now) / mDNSPlatformOneSecond;
			char *ifname = InterfaceNameForID(m, q->InterfaceID);
			CacheUsed++;
			if (q->ThisQInterval) CacheActive++;
			LogMsgNoIdent("%6d%6d %-7s%s%s %5d  %-6s%##s%s",
				i, n,
				ifname ? ifname : mDNSOpaque16IsZero(q->TargetQID) ? "" : "-U-",
				mDNSOpaque16IsZero(q->TargetQID) ? (q->LongLived ? "l" : " ") : (q->LongLived ? "L" : "O"),
				q->AuthInfo    ? "P" : " ",
				q->CurrentAnswers,
				DNSTypeName(q->qtype), q->qname.c, q->DuplicateOf ? " (dup)" : "");
			usleep((m->KnownBugs & mDNS_KnownBug_LossySyslog) ? 3333 : 1000);
			}
		LogMsgNoIdent("%lu question%s; %lu active", CacheUsed, CacheUsed > 1 ? "s" : "", CacheActive);
		}

	LogMsgNoIdent("----- Local-Only Questions -----");
	if (!m->LocalOnlyQuestions) LogMsgNoIdent("<None>");
	else for (q = m->LocalOnlyQuestions; q; q=q->next)
		LogMsgNoIdent("                       %5d  %-6s%##s%s",
			q->CurrentAnswers, DNSTypeName(q->qtype), q->qname.c, q->DuplicateOf ? " (dup)" : "");

	LogMsgNoIdent("---- Active Client Requests ----");
	if (!all_requests) LogMsgNoIdent("<None>");
	else
		{
		request_state *req;
		for (req = all_requests; req; req=req->next)
			LogClientInfo(m, req);
		usleep((m->KnownBugs & mDNS_KnownBug_LossySyslog) ? 3333 : 1000);
		}

	LogMsgNoIdent("-------- NAT Traversals --------");
	if (!m->NATTraversals) LogMsgNoIdent("<None>");
	else
		{
		NATTraversalInfo *nat;
		for (nat = m->NATTraversals; nat; nat=nat->next)
			{
			if (nat->Protocol)
				LogMsgNoIdent("%p %s Int %5d Ext %5d Err %d Retry %5d Interval %5d Expire %5d",
					nat, nat->Protocol == NATOp_MapTCP ? "TCP" : "UDP",
					mDNSVal16(nat->IntPort), mDNSVal16(nat->ExternalPort), nat->Result,
					nat->retryPortMap ? (nat->retryPortMap - now) / mDNSPlatformOneSecond : 0,
					nat->retryInterval / mDNSPlatformOneSecond,
					nat->ExpiryTime ? (nat->ExpiryTime - now) / mDNSPlatformOneSecond : 0);
			else
				LogMsgNoIdent("%p Address Request               Retry %5d Interval %5d", nat,
					(m->retryGetAddr - now) / mDNSPlatformOneSecond,
					m->retryIntervalGetAddr / mDNSPlatformOneSecond);
			usleep((m->KnownBugs & mDNS_KnownBug_LossySyslog) ? 3333 : 1000);
			}
		}

	LogMsgNoIdent("--------- AuthInfoList ---------");
	if (!m->AuthInfoList) LogMsgNoIdent("<None>");
	else
		{
		DomainAuthInfo *a;
		for (a = m->AuthInfoList; a; a = a->next)
			LogMsgNoIdent("%##s %##s%s", a->domain.c, a->keyname.c, a->AutoTunnel ? " AutoTunnel" : "");
		}

	#if APPLE_OSX_mDNSResponder
	LogMsgNoIdent("--------- TunnelClients --------");
	if (!m->TunnelClients) LogMsgNoIdent("<None>");
	else
		{
		ClientTunnel *c;
		for (c = m->TunnelClients; c; c = c->next)
			LogMsgNoIdent("%##s local %.16a %.4a remote %.16a %.4a %5d interval %d",
				c->dstname.c, &c->loc_inner, &c->loc_outer, &c->rmt_inner, &c->rmt_outer, mDNSVal16(c->rmt_outer_port), c->q.ThisQInterval);
		}
	#endif // APPLE_OSX_mDNSResponder

	LogMsgNoIdent("---------- Misc State ----------");

	LogMsgNoIdent("PrimaryMAC:   %.6a", &m->PrimaryMAC);

	LogMsgNoIdent("m->SleepState %d (%s) seq %d",
		m->SleepState,
		m->SleepState == SleepState_Awake        ? "Awake"        :
		m->SleepState == SleepState_Transferring ? "Transferring" : 
		m->SleepState == SleepState_Sleeping     ? "Sleeping"     : "?",
		m->SleepSeqNum);

	if (!m->SPSSocket) LogMsgNoIdent("Not offering Sleep Proxy Service");
	else LogMsgNoIdent("Offering Sleep Proxy Service: %#s", m->SPSRecords.RR_SRV.resrec.name->c);

	if (m->ProxyRecords == ProxyA + ProxyD) LogMsgNoIdent("ProxyRecords: %d + %d = %d", ProxyA, ProxyD, ProxyA + ProxyD);
	else LogMsgNoIdent("ProxyRecords: MISMATCH %d + %d = %d ≠ %d", ProxyA, ProxyD, ProxyA + ProxyD, m->ProxyRecords);

	LogMsgNoIdent("------ Auto Browse Domains -----");
	if (!AutoBrowseDomains) LogMsgNoIdent("<None>");
	else for (d=AutoBrowseDomains; d; d=d->next) LogMsgNoIdent("%##s", d->name.c);

	LogMsgNoIdent("--- Auto Registration Domains --");
	if (!AutoRegistrationDomains) LogMsgNoIdent("<None>");
	else for (d=AutoRegistrationDomains; d; d=d->next) LogMsgNoIdent("%##s", d->name.c);
	}

#if APPLE_OSX_mDNSResponder && MACOSX_MDNS_MALLOC_DEBUGGING
mDNSexport void uds_validatelists(void)
	{
	const request_state *req, *p;
	for (req = all_requests; req; req=req->next)
		{
		if (req->next == (request_state *)~0 || (req->sd < 0 && req->sd != -2))
			LogMemCorruption("UDS request list: %p is garbage (%d)", req, req->sd);

		if (req->primary == req)
			LogMemCorruption("UDS request list: req->primary should not point to self %p/%d", req, req->sd);

		if (req->primary && req->replies)
			LogMemCorruption("UDS request list: Subordinate request %p/%d/%p should not have replies (%p)",
				req, req->sd, req->primary && req->replies);

		p = req->primary;
		if ((long)p & 3)
			LogMemCorruption("UDS request list: req %p primary %p is misaligned (%d)", req, p, req->sd);
		else if (p && (p->next == (request_state *)~0 || (p->sd < 0 && p->sd != -2)))
			LogMemCorruption("UDS request list: req %p primary %p is garbage (%d)", req, p, p->sd);

		reply_state *rep;
		for (rep = req->replies; rep; rep=rep->next)
		  if (rep->next == (reply_state *)~0)
			LogMemCorruption("UDS req->replies: %p is garbage", rep);

		if (req->terminate == connection_termination)
			{
			registered_record_entry *r;
			for (r = req->u.reg_recs; r; r=r->next)
				if (r->next == (registered_record_entry *)~0)
					LogMemCorruption("UDS req->u.reg_recs: %p is garbage", r);
			}
		else if (req->terminate == regservice_termination_callback)
			{
			service_instance *s;
			for (s = req->u.servicereg.instances; s; s=s->next)
				if (s->next == (service_instance *)~0)
					LogMemCorruption("UDS req->u.servicereg.instances: %p is garbage", s);
			}
		else if (req->terminate == browse_termination_callback)
			{
			browser_t *b;
			for (b = req->u.browser.browsers; b; b=b->next)
				if (b->next == (browser_t *)~0)
					LogMemCorruption("UDS req->u.browser.browsers: %p is garbage", b);
			}
		}

	DNameListElem *d;
	for (d = SCPrefBrowseDomains; d; d=d->next)
		if (d->next == (DNameListElem *)~0 || d->name.c[0] > 63)
			LogMemCorruption("SCPrefBrowseDomains: %p is garbage (%d)", d, d->name.c[0]);

	ARListElem *b;
	for (b = LocalDomainEnumRecords; b; b=b->next)
		if (b->next == (ARListElem *)~0 || b->ar.resrec.name->c[0] > 63)
			LogMemCorruption("LocalDomainEnumRecords: %p is garbage (%d)", b, b->ar.resrec.name->c[0]);

	for (d = AutoBrowseDomains; d; d=d->next)
		if (d->next == (DNameListElem *)~0 || d->name.c[0] > 63)
			LogMemCorruption("AutoBrowseDomains: %p is garbage (%d)", d, d->name.c[0]);

	for (d = AutoRegistrationDomains; d; d=d->next)
		if (d->next == (DNameListElem *)~0 || d->name.c[0] > 63)
			LogMemCorruption("AutoRegistrationDomains: %p is garbage (%d)", d, d->name.c[0]);
	}
#endif // APPLE_OSX_mDNSResponder && MACOSX_MDNS_MALLOC_DEBUGGING

mDNSlocal int send_msg(request_state *const req)
	{
	reply_state *const rep = req->replies;		// Send the first waiting reply
	ssize_t nwriten;
	if (req->no_reply) return(t_complete);

	ConvertHeaderBytes(rep->mhdr);
	nwriten = send(req->sd, (char *)&rep->mhdr + rep->nwriten, rep->totallen - rep->nwriten, 0);
	ConvertHeaderBytes(rep->mhdr);

	if (nwriten < 0)
		{
		if (dnssd_errno == dnssd_EINTR || dnssd_errno == dnssd_EWOULDBLOCK) nwriten = 0;
		else
			{
#if !defined(PLATFORM_NO_EPIPE)
			if (dnssd_errno == EPIPE)
				return(req->ts = t_terminated);
			else
#endif
				{
				LogMsg("send_msg ERROR: failed to write %d of %d bytes to fd %d errno %d (%s)",
					rep->totallen - rep->nwriten, rep->totallen, req->sd, dnssd_errno, dnssd_strerror(dnssd_errno));
				return(t_error);
				}
			}
		}
	rep->nwriten += nwriten;
	return (rep->nwriten == rep->totallen) ? t_complete : t_morecoming;
	}

mDNSexport mDNSs32 udsserver_idle(mDNSs32 nextevent)
	{
	mDNSs32 now = mDNS_TimeNow(&mDNSStorage);
	request_state **req = &all_requests;

	while (*req)
		{
		request_state *const r = *req;

		if (r->terminate == resolve_termination_callback)
			if (r->u.resolve.ReportTime && now - r->u.resolve.ReportTime >= 0)
				{
				r->u.resolve.ReportTime = 0;
				LogMsgNoIdent("Client application bug: DNSServiceResolve(%##s) active for over two minutes. "
					"This places considerable burden on the network.", r->u.resolve.qsrv.qname.c);
				}

		// Note: Only primary req's have reply lists, not subordinate req's.
		while (r->replies)		// Send queued replies
			{
			transfer_state result;
			if (r->replies->next) r->replies->rhdr->flags |= dnssd_htonl(kDNSServiceFlagsMoreComing);
			result = send_msg(r);	// Returns t_morecoming if buffer full because client is not reading
			if (result == t_complete)
				{
				reply_state *fptr = r->replies;
				r->replies = r->replies->next;
				freeL("reply_state/udsserver_idle", fptr);
				r->time_blocked = 0; // reset failure counter after successful send
				r->unresponsiveness_reports = 0;
				continue;
				}
			else if (result == t_terminated || result == t_error)
				{
				LogMsg("%3d: Could not write data to client because of error - aborting connection", r->sd);
				LogClientInfo(&mDNSStorage, r);
				abort_request(r);
				}
			break;
			}

		if (r->replies)		// If we failed to send everything, check our time_blocked timer
			{
			if (nextevent - now > mDNSPlatformOneSecond) nextevent = now + mDNSPlatformOneSecond;

			if (mDNSStorage.SleepState != SleepState_Awake) r->time_blocked = 0;
			else if (!r->time_blocked) r->time_blocked = NonZeroTime(now);
			else if (now - r->time_blocked >= 10 * mDNSPlatformOneSecond * (r->unresponsiveness_reports+1))
				{
				int num = 0;
				struct reply_state *x = r->replies;
				while (x) { num++; x=x->next; }
				LogMsg("%3d: Could not write data to client after %ld seconds, %d repl%s waiting",
					r->sd, (now - r->time_blocked) / mDNSPlatformOneSecond, num, num == 1 ? "y" : "ies");
				if (++r->unresponsiveness_reports >= 60)
					{
					LogMsg("%3d: Client unresponsive; aborting connection", r->sd);
					LogClientInfo(&mDNSStorage, r);
					abort_request(r);
					}
				}
			}

		if (!dnssd_SocketValid(r->sd)) // If this request is finished, unlink it from the list and free the memory
			{
			// Since we're already doing a list traversal, we unlink the request directly instead of using AbortUnlinkAndFree()
			*req = r->next;
			freeL("request_state/udsserver_idle", r);
			}
		else
			req = &r->next;
		}
	return nextevent;
	}

struct CompileTimeAssertionChecks_uds_daemon
	{
	// Check our structures are reasonable sizes. Including overly-large buffers, or embedding
	// other overly-large structures instead of having a pointer to them, can inadvertently
	// cause structure sizes (and therefore memory usage) to balloon unreasonably.
	char sizecheck_request_state          [(sizeof(request_state)           <= 2000) ? 1 : -1];
	char sizecheck_registered_record_entry[(sizeof(registered_record_entry) <=   40) ? 1 : -1];
	char sizecheck_service_instance       [(sizeof(service_instance)        <= 6552) ? 1 : -1];
	char sizecheck_browser_t              [(sizeof(browser_t)               <=  992) ? 1 : -1];
	char sizecheck_reply_hdr              [(sizeof(reply_hdr)               <=   12) ? 1 : -1];
	char sizecheck_reply_state            [(sizeof(reply_state)             <=   64) ? 1 : -1];
	};
