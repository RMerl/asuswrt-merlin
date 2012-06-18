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

$Log: mDNSMacOSX.c,v $
Revision 1.691  2009/07/30 20:28:15  mkrochma
<rdar://problem/7100784> Sleep Proxy: Structure changes to data passed to userclient

Revision 1.690  2009/07/15 22:34:25  cheshire
<rdar://problem/6613674> Sleep Proxy: Add support for using sleep proxy in local network interface hardware
Fixes to make the code still compile with old headers and libraries (pre 10.5) that don't include IOConnectCallStructMethod

Revision 1.689  2009/07/15 22:09:19  cheshire
<rdar://problem/6613674> Sleep Proxy: Add support for using sleep proxy in local network interface hardware
Removed unnecessary sleep(1) and syslog message

Revision 1.688  2009/07/11 01:58:17  cheshire
<rdar://problem/6613674> Sleep Proxy: Add support for using sleep proxy in local network interface hardware
Added ActivateLocalProxy routine for transferring mDNS records to local proxy

Revision 1.687  2009/06/30 21:16:09  cheshire
<rdar://problem/7020041> Plugging and unplugging the power cable shouldn't cause a network change event
Additional fix: Only start and stop NetWake browses for active interfaces that are currently registered with mDNSCore

Revision 1.686  2009/06/25 23:36:56  cheshire
To facilitate testing, added command-line switch "-OfferSleepProxyService"
to re-enable the previously-supported mode of operation where we offer
sleep proxy service on desktop Macs that are set to never sleep.

Revision 1.685  2009/06/25 23:15:12  cheshire
Don't try to use private header file "IOPowerSourcesPrivate.h"
(it prevents external developers from being able to compile the code)

Revision 1.684  2009/06/24 22:14:22  cheshire
<rdar://problem/6911445> Plugging and unplugging the power cable shouldn't cause a network change event

Revision 1.683  2009/06/08 22:31:03  cheshire
Fixed typo in comment: Portability > 35 means nominal weight < 3kg

Revision 1.682  2009/05/19 23:30:31  cheshire
Suppressed some unnecessary debugging messages; added AppleTV to list of recognized hardware

Revision 1.681  2009/05/12 23:23:15  cheshire
Removed unnecessary "mDNSPlatformTCPConnect - connect failed ... Error 50 (Network is down)" debugging message

Revision 1.680  2009/05/05 01:32:50  jessic2
<rdar://problem/6830541> regservice_callback: instance->request is NULL 0 -- Clean up spurious logs resulting from fixing this bug.

Revision 1.679  2009/05/01 23:48:46  jessic2
<rdar://problem/6830541> regservice_callback: instance->request is NULL 0

Revision 1.678  2009/04/24 23:32:28  cheshire
To facilitate testing, put back code to be a sleep proxy when set to never sleep, compiled out by compile-time switch

Revision 1.677  2009/04/24 20:50:16  mcguire
<rdar://problem/6791775> 4 second delay in DNS response

Revision 1.676  2009/04/24 02:17:58  mcguire
<rdar://problem/5264124> uDNS: Not always respecting preference order of DNS servers

Revision 1.675  2009/04/23 18:51:28  mcguire
<rdar://problem/6729406> uDNS: PPP doesn't automatically reconnect on wake from sleep (no name resolver)

Revision 1.674  2009/04/23 00:58:01  jessic2
<rdar://problem/6802117> uDNS: DNS stops working after configd crashes

Revision 1.673  2009/04/22 01:19:57  jessic2
<rdar://problem/6814585> Daemon: mDNSResponder is logging garbage for error codes because it's using %ld for int 32

Revision 1.672  2009/04/21 16:34:47  mcguire
<rdar://problem/6810663> null deref in mDNSPlatformSetDNSConfig

Revision 1.671  2009/04/15 01:14:07  mcguire
<rdar://problem/6791775> 4 second delay in DNS response

Revision 1.670  2009/04/15 01:10:39  jessic2
<rdar://problem/6466541> BTMM: Add support for setting kDNSServiceErr_NoSuchRecord in DynamicStore

Revision 1.669  2009/04/11 02:02:34  mcguire
<rdar://problem/6780046> crash in doSSLHandshake

Revision 1.668  2009/04/11 00:20:08  jessic2
<rdar://problem/4426780> Daemon: Should be able to turn on LogOperation dynamically

Revision 1.667  2009/04/09 20:01:00  cheshire
<rdar://problem/6767122> IOPMCopyActivePMPreferences not available on Apple TV
At Rory's suggestion, removed unnecessary "Could not get Wake On LAN value" log message

Revision 1.666  2009/04/07 21:57:53  cheshire
<rdar://problem/6767122> IOPMCopyActivePMPreferences not available on Apple TV
Put previous code back

Revision 1.665  2009/04/03 21:48:44  mcguire
Back out checkin 1.664 (<rdar://problem/6755199> MessageTracer: prepend domain to make signature field unique)

Revision 1.663  2009/04/02 22:21:16  mcguire
<rdar://problem/6577409> Adopt IOPM APIs

Revision 1.662  2009/04/02 01:08:15  mcguire
<rdar://problem/6735635> Don't be a sleep proxy when set to sleep never

Revision 1.661  2009/04/01 17:50:14  mcguire
cleanup mDNSRandom

Revision 1.660  2009/04/01 01:13:10  mcguire
<rdar://problem/6744276> Sleep Proxy: Detect lid closed

Revision 1.659  2009/03/30 21:11:07  jessic2
<rdar://problem/6728725> Need to do some polish work on MessageTracer logging

Revision 1.658  2009/03/30 20:07:28  mcguire
<rdar://problem/6736133> BTMM: SSLHandshake threads are leaking Mach ports

Revision 1.657  2009/03/27 17:27:13  cheshire
<rdar://problem/6724859> Need to support querying IPv6 DNS servers

Revision 1.656  2009/03/26 05:02:48  mcguire
fix build error in dnsextd

Revision 1.655  2009/03/26 03:59:00  jessic2
Changes for <rdar://problem/6492552&6492593&6492609&6492613&6492628&6492640&6492699>

Revision 1.654  2009/03/20 20:53:26  cheshire
Added test code for testing with MacBook Air, using a USB dongle that doesn't actually support Wake-On-LAN

Revision 1.653  2009/03/20 20:52:22  cheshire
<rdar://problem/6703952> Support CFUserNotificationDisplayNotice in mDNSResponderHelper

Revision 1.652  2009/03/19 23:44:47  mcguire
<rdar://problem/6699216> Properly handle EADDRINUSE

Revision 1.651  2009/03/17 19:15:24  mcguire
<rdar://problem/6655415> SSLHandshake deadlock issues

Revision 1.650  2009/03/17 01:24:22  cheshire
Updated to new Sleep Proxy metric ranges: 100000-999999; 1000000 means "do not use"

Revision 1.649  2009/03/15 01:30:29  mcguire
fix log message

Revision 1.648  2009/03/15 01:16:08  mcguire
<rdar://problem/6655415> SSLHandshake deadlock issues

Revision 1.647  2009/03/14 01:42:56  mcguire
<rdar://problem/5457116> BTMM: Fix issues with multiple .Mac accounts on the same machine

Revision 1.646  2009/03/13 01:36:24  mcguire
<rdar://problem/6657640> Reachability fixes on DNS config change

Revision 1.645  2009/03/10 23:48:33  cheshire
<rdar://problem/6665739> Task scheduling failure when Sleep Proxy Server is active

Revision 1.644  2009/03/10 04:17:09  cheshire
Check for NULL answer in UpdateSPSStatus()

Revision 1.643  2009/03/10 01:15:55  cheshire
Sleep Proxies with invalid names (score 10000) need to be ignored

Revision 1.642  2009/03/08 04:46:51  mkrochma
Change Keychain LogMsg to LogInfo

Revision 1.641  2009/03/05 23:53:34  cheshire
Removed spurious "SnowLeopardPowerChanged: wake ERROR" syslog message

Revision 1.640  2009/03/05 21:57:13  cheshire
Don't close BPF fd until we have no more records we're proxying for on that interface

Revision 1.639  2009/03/04 01:45:01  cheshire
HW_MODEL information should be LogSPS, not LogMsg

Revision 1.638  2009/03/03 22:51:54  cheshire
<rdar://problem/6504236> Sleep Proxy: Waking on same network but different interface will cause conflicts

Revision 1.637  2009/02/26 22:58:47  cheshire
<rdar://problem/6616335> Crash in mDNSResponder at mDNSResponder • CloseBPF + 75
Fixed race condition between the kqueue thread and the CFRunLoop thread.

Revision 1.636  2009/02/21 01:38:39  cheshire
Added comment: mDNSCoreMachineSleep(m, false); // Will set m->SleepState = SleepState_Awake;

Revision 1.635  2009/02/17 23:29:03  cheshire
Throttle logging to a slower rate when running on SnowLeopard

Revision 1.634  2009/02/14 00:29:17  mcguire
fixed typo

Revision 1.633  2009/02/14 00:07:11  cheshire
Need to set up m->SystemWakeOnLANEnabled before calling UpdateInterfaceList(m, utc);

Revision 1.632  2009/02/13 19:40:07  cheshire
Improved alignment of LogSPS messages

Revision 1.631  2009/02/13 18:16:05  cheshire
Fixed some compile warnings

Revision 1.630  2009/02/13 06:32:43  cheshire
Converted LogOperation messages to LogInfo or LogSPS

Revision 1.629  2009/02/12 20:57:26  cheshire
Renamed 'LogAllOperation' switch to 'LogClientOperations'; added new 'LogSleepProxyActions' switch

Revision 1.628  2009/02/11 02:34:45  cheshire
m->p->SystemWakeForNetworkAccessEnabled renamed to m->SystemWakeOnLANEnabled

Revision 1.627  2009/02/10 00:19:17  cheshire
<rdar://problem/6107426> Sleep Proxy: Adopt SIOCGIFWAKEFLAGS ioctl to determine interface WOMP-ability

Revision 1.626  2009/02/10 00:15:38  cheshire
<rdar://problem/6551529> Sleep Proxy: "Unknown DNS packet type 8849" logs

Revision 1.625  2009/02/09 21:24:25  cheshire
Set correct bit in ifr.ifr_wake_flags (was coincidentally working because IF_WAKE_ON_MAGIC_PACKET happens to have the value 1)

Revision 1.624  2009/02/09 21:11:43  cheshire
Need to acknowledge kIOPMSystemPowerStateCapabilityCPU message

Revision 1.623  2009/02/09 06:20:42  cheshire
Upon receiving system power change notification, make sure our m->p->SystemWakeForNetworkAccessEnabled value
correctly reflects the current system setting

Revision 1.622  2009/02/07 02:57:32  cheshire
<rdar://problem/6084043> Sleep Proxy: Need to adopt IOPMConnection

Revision 1.621  2009/02/06 03:18:12  mcguire
<rdar://problem/6534643> BTMM: State not cleaned up on SIGTERM w/o reboot

Revision 1.620  2009/02/02 22:14:11  cheshire
Instead of repeatedly checking the Dynamic Store, use m->p->SystemWakeForNetworkAccessEnabled variable

Revision 1.619  2009/01/24 02:11:58  cheshire
Handle case where config->resolver[0]->nameserver[0] is NULL

Revision 1.618  2009/01/24 01:55:51  cheshire
Handle case where config->resolver[0]->domain is NULL

Revision 1.617  2009/01/24 01:48:42  cheshire
<rdar://problem/4786302> Implement logic to determine when to send dot-local lookups via Unicast

Revision 1.616  2009/01/24 00:28:43  cheshire
Updated comments

Revision 1.615  2009/01/22 02:14:26  cheshire
<rdar://problem/6515626> Sleep Proxy: Set correct target MAC address, instead of all zeroes

Revision 1.614  2009/01/21 03:43:57  mcguire
<rdar://problem/6511765> BTMM: Add support for setting kDNSServiceErr_NATPortMappingDisabled in DynamicStore

Revision 1.613  2009/01/20 02:38:41  mcguire
fix previous checkin comment

Revision 1.612  2009/01/20 02:35:15  mcguire
<rdar://problem/6508974> don't update BTMM & SleepProxyServers status at shutdown time

Revision 1.611  2009/01/17 04:15:40  cheshire
Updated "did sleep(5)" debugging message

Revision 1.610  2009/01/16 20:37:30  cheshire
Fixed incorrect value of EOPNOTSUPP

Revision 1.609  2009/01/16 03:08:13  cheshire
Use kernel event notifications to track KEV_DL_WAKEFLAGS_CHANGED
(indicates when SIOCGIFWAKEFLAGS changes for an interface, e.g. when AirPort
switches from a base-station that's WakeOnLAN-capable to one that isn't)

Revision 1.608  2009/01/16 01:27:03  cheshire
Initial work to adopt SIOCGIFWAKEFLAGS ioctl to determine whether an interface is WakeOnLAN-capable

Revision 1.607  2009/01/15 22:24:01  cheshire
Get rid of unnecessary ifa_name field in NetworkInterfaceInfoOSX (it just duplicates the content of ifinfo.ifname)
This also eliminates an unnecessary malloc, memory copy, and free

Revision 1.606  2009/01/15 00:22:49  mcguire
<rdar://problem/6437092> NAT-PMP: mDNSResponder needs to listen on 224.0.0.1:5350/UDP with REUSEPORT

Revision 1.605  2009/01/14 01:38:43  mcguire
<rdar://problem/6492710> Write out DynamicStore per-interface SleepProxyServer info

Revision 1.604  2009/01/13 05:31:34  mkrochma
<rdar://problem/6491367> Replace bzero, bcopy with mDNSPlatformMemZero, mDNSPlatformMemCopy, memset, memcpy

Revision 1.603  2009/01/12 22:26:13  mkrochma
Change DynamicStore location from BonjourSleepProxy/DiscoveredServers to SleepProxyServers

Revision 1.602  2008/12/19 20:23:34  mcguire
<rdar://problem/6459269> Lots of duplicate log messages about failure to bind to NAT-PMP Announcement port

Revision 1.601  2008/12/15 19:51:56  mcguire
<rdar://problem/6443067> Retry UDP socket creation only when randomizing ports

Revision 1.600  2008/12/12 21:30:14  cheshire
Additional defensive coding -- make sure InterfaceID is found in our list before using it

Revision 1.599  2008/12/12 04:36:26  cheshire
Make sure we don't overflow our BPF filter buffer
Only add addresses for records where the InterfaceID matches

Revision 1.598  2008/12/12 00:57:51  cheshire
Updated BPF filter generation to explicitly match addresses we're proxying for,
rather than just matching any unknown IP address

Revision 1.597  2008/12/10 20:37:05  cheshire
Don't mark interfaces like PPP as being WakeonLAN-capable

Revision 1.596  2008/12/10 19:34:30  cheshire
Use symbolic OS version names instead of literal integers

Revision 1.595  2008/12/10 02:25:31  cheshire
Minor fixes to use of LogClientOperations symbol

Revision 1.594  2008/12/10 02:11:45  cheshire
ARMv5 compiler doesn't like uncommented stuff after #endif

Revision 1.593  2008/12/09 23:08:55  mcguire
<rdar://problem/6430877> should use IP_BOUND_IF
additional cleanup

Revision 1.592  2008/12/09 19:58:44  mcguire
<rdar://problem/6430877> should use IP_BOUND_IF

Revision 1.591  2008/12/09 15:39:05  cheshire
Workaround for bug on Leopard and earlier where Ethernet drivers report wrong link state immediately after wake from sleep

Revision 1.590  2008/12/09 05:21:54  cheshire
Should key sleep/wake handling off kIOMessageSystemWillPowerOn message -- the kIOMessageSystemHasPoweredOn
message is delayed by some seemingly-random amount in the range 0-15 seconds.

Revision 1.589  2008/12/05 02:35:25  mcguire
<rdar://problem/6107390> Write to the DynamicStore when a Sleep Proxy server is available on the network

Revision 1.588  2008/12/04 21:08:52  mcguire
<rdar://problem/6116863> mDNS: Provide mechanism to disable Multicast advertisements

Revision 1.587  2008/12/04 02:17:47  cheshire
Additional sleep/wake debugging messages

Revision 1.586  2008/11/26 20:34:55  cheshire
Changed some "LogOperation" debugging messages to "debugf"

Revision 1.585  2008/11/25 20:53:35  cheshire
Updated portability metrics; added Xserve and PowerBook to list

Revision 1.584  2008/11/25 05:07:16  cheshire
<rdar://problem/6374328> Advertise Sleep Proxy metrics in service name

Revision 1.583  2008/11/20 01:42:31  cheshire
For consistency with other parts of the code, changed code to only check
that the first 4 bytes of MAC address are zero, not the whole 6 bytes.

Revision 1.582  2008/11/14 22:59:09  cheshire
When on a network with more than one subnet overlayed on a single physical link, don't make local ARP
entries for hosts that are on our physical link but not on our logical subnet -- it confuses the kernel

Revision 1.581  2008/11/14 21:01:26  cheshire
Log a warning if we fail to get a MAC address for an interface

Revision 1.580  2008/11/14 02:16:15  cheshire
Clean up NetworkChanged handling

Revision 1.579  2008/11/12 23:15:37  cheshire
Updated log messages

Revision 1.578  2008/11/06 23:41:57  cheshire
Refinement: Only need to create local ARP entry when sending ARP packet to broadcast address or to ourselves

Revision 1.577  2008/11/06 01:15:47  mcguire
Fix compile error that occurs when LogOperation is disabled

Revision 1.576  2008/11/05 21:55:21  cheshire
Fixed mistake in BPF filter generation

Revision 1.575  2008/11/04 23:54:09  cheshire
Added routine mDNSSetARP(), used to replace an SPS client's entry in our ARP cache with
a dummy one, so that IP traffic to the SPS client initiated by the SPS machine can be
captured by our BPF filters, and used as a trigger to wake the sleeping machine.

Revision 1.574  2008/11/04 00:27:58  cheshire
Corrected some timing anomalies in sleep/wake causing spurious name self-conflicts

Revision 1.573  2008/11/03 01:12:42  mkrochma
Fix compile error that occurs when LogOperation is disabled

Revision 1.572  2008/10/31 23:36:13  cheshire
One wakeup clear any previous power requests

Revision 1.571  2008/10/31 23:05:30  cheshire
Move logic to decide when to at as Sleep Proxy Server from daemon.c to mDNSMacOSX.c

Revision 1.570  2008/10/30 01:08:19  cheshire
After waking for network maintenance operations go back to sleep again

Revision 1.569  2008/10/29 21:39:43  cheshire
Updated syslog messages; close BPF socket on read error

Revision 1.568  2008/10/28 20:37:28  cheshire
Changed code to create its own CFSocketCreateWithNative directly, instead of
relying on udsSupportAddFDToEventLoop/udsSupportRemoveFDFromEventLoop

Revision 1.567  2008/10/27 22:31:37  cheshire
Can't just close BPF_fd using "close(i->BPF_fd);" -- need to call
"udsSupportRemoveFDFromEventLoop(i->BPF_fd);" to remove it from our event source list

Revision 1.566  2008/10/23 22:33:23  cheshire
Changed "NOTE:" to "Note:" so that BBEdit 9 stops putting those comment lines into the funtion popup menu

Revision 1.565  2008/10/22 23:23:55  cheshire
Moved definition of OSXVers from daemon.c into mDNSMacOSX.c

Revision 1.564  2008/10/22 22:08:46  cheshire
Take IP header length into account when determining how many bytes to return from BPF filter

Revision 1.563  2008/10/22 20:59:28  cheshire
BPF filter needs to capture a few more bytes so that we can examine TCP header fields

Revision 1.562  2008/10/22 19:48:29  cheshire
Improved syslog messages

Revision 1.561  2008/10/22 17:18:57  cheshire
Need to open and close BPF fds when turning Sleep Proxy Server on and off

Revision 1.560  2008/10/22 01:09:36  cheshire
Fixed build warning when not using LogClientOperations

Revision 1.559  2008/10/21 01:05:30  cheshire
Added code to receive raw packets using Berkeley Packet Filter (BPF)

Revision 1.558  2008/10/16 22:42:06  cheshire
Removed debugging messages

Revision 1.557  2008/10/09 22:33:14  cheshire
Fill in ifinfo.MAC field when fetching interface list

Revision 1.556  2008/10/09 21:15:23  cheshire
In mDNSPlatformUDPSocket(), need to create an IPv6 socket as well as IPv4

Revision 1.555  2008/10/09 19:05:57  cheshire
No longer want to inhibit all networking when going to sleep

Revision 1.554  2008/10/08 18:36:51  mkrochma
<rdar://problem/4371323> Supress Couldn't read user-specified Computer Name logs

Revision 1.553  2008/10/04 00:47:12  cheshire
If NetWake setting changes for an interface, treat it as a whole new interface (like BSSID changing)

Revision 1.552  2008/10/03 23:32:15  cheshire
Added definition of mDNSPlatformSendRawPacket

Revision 1.551  2008/10/03 18:25:16  cheshire
Instead of calling "m->MainCallback" function pointer directly, call mDNSCore routine "mDNS_ConfigChanged(m);"

Revision 1.550  2008/10/03 00:51:58  cheshire
Removed spurious "else" case that got left in by mistake

Revision 1.549  2008/10/03 00:50:13  cheshire
Minor code rearrangement; don't set up interface list until *after* we've started watching for network changes

Revision 1.548  2008/10/03 00:26:25  cheshire
Export DictionaryIsEnabled() so it's callable from other files

Revision 1.547  2008/10/02 23:50:07  mcguire
<rdar://problem/6136442> shutdown time issues
improve log messages when SCDynamicStoreCreate() fails

Revision 1.546  2008/10/02 22:26:21  cheshire
Moved declaration of BPF_fd from uds_daemon.c to mDNSMacOSX.c, where it really belongs

Revision 1.545  2008/10/01 22:01:40  cheshire
On Allan Nathanson's advice, add "State:/IOKit/PowerManagement/CurrentSettings"
to keys array instead of patterns array, for efficiency

Revision 1.544  2008/10/01 21:35:35  cheshire
Monitor "State:/IOKit/PowerManagement/CurrentSettings" to track state of "Wake for network access" setting

Revision 1.543  2008/09/26 23:05:56  mkrochma
Improve log messages by using good old UTF-8

Revision 1.542  2008/09/26 19:49:51  cheshire
Improved "failed to send packet" debugging message

Revision 1.541  2008/09/25 21:02:05  cheshire
<rdar://problem/6245044> Stop using separate m->ServiceRegistrations list
In TunnelServers(), need to check main m->ResourceRecords list to see
if we have a non-zero number of advertised AutoTunnel services

Revision 1.540  2008/07/30 00:55:56  mcguire
<rdar://problem/3988320> Should use randomized source ports and transaction IDs to avoid DNS cache poisoning
Additional fixes so that we know when a socket has been closed while in a loop reading from it

Revision 1.539  2008/07/24 20:23:04  cheshire
<rdar://problem/3988320> Should use randomized source ports and transaction IDs to avoid DNS cache poisoning

Revision 1.538  2008/06/02 05:39:39  mkrochma
<rdar://problem/5932760> Don't set TOS bits anymore

Revision 1.537  2008/05/01 18:30:54  mkrochma
<rdar://problem/5895642> Make mDNSResponder and mDNSResponderHelper shutdown at regular time

Revision 1.536  2008/03/25 01:27:30  mcguire
<rdar://problem/5810718> Status sometimes wrong when link goes down

Revision 1.535  2008/03/14 22:52:51  mcguire
<rdar://problem/5321824> write status to the DS
Ignore duplicate queries, which don't get established (since they're duplicates)

Revision 1.534  2008/03/12 22:58:15  mcguire
<rdar://problem/5321824> write status to the DS
Fixes for NO_SECURITYFRAMEWORK

Revision 1.533  2008/03/07 00:48:54  mcguire
<rdar://problem/5321824> write status to the DS
cleanup strings

Revision 1.532  2008/03/06 23:44:39  mcguire
<rdar://problem/5321824> write status to the DS
cleanup function names & log messages
add external port numbers to dictionary
add defensive code in case CF*Create fails
don't output NAT statuses if zero

Revision 1.531  2008/03/06 21:27:47  cheshire
<rdar://problem/5500969> BTMM: Need ability to identify version of mDNSResponder client
To save network bandwidth, removed unnecessary redundant information from HINFO record

Revision 1.530  2008/03/06 03:15:48  mcguire
<rdar://problem/5321824> write status to the DS
use mStatus_* instead of kDNSServiceErr_*

Revision 1.529  2008/03/06 02:48:35  mcguire
<rdar://problem/5321824> write status to the DS

Revision 1.528  2008/02/29 01:33:57  mcguire
<rdar://problem/5611801> BTMM: Services stay registered after previously successful NAT Port mapping fails

Revision 1.527  2008/02/28 03:25:26  mcguire
<rdar://problem/5535772> config cleanup on shutdown/reboot

Revision 1.526  2008/02/26 21:43:54  cheshire
Renamed 'clockdivisor' to 'mDNSPlatformClockDivisor' (LogTimeStamps code needs to be able to access it)

Revision 1.525  2008/02/20 00:53:20  cheshire
<rdar://problem/5492035> getifaddrs is returning invalid netmask family for fw0 and vmnet
Removed overly alarming syslog message

Revision 1.524  2008/01/31 22:25:10  jgraessley
<rdar://problem/5715434> using default Macintosh-0016CBF62EFD.local
Use sysctlbyname to get hardware type for the default name.

Revision 1.523  2008/01/15 01:32:56  jgraessley
Bug #: 5595309
Reviewed by: Stuart Cheshire
Additional change to print warning message up to 1000 times to make it more visible

Revision 1.522  2008/01/15 01:14:02  mcguire
<rdar://problem/5674390> mDNSPlatformSendUDP should allow unicast queries on specific interfaces
removed check and log message, as they are no longer relevant

Revision 1.521  2007/12/14 00:58:28  cheshire
<rdar://problem/5526800> BTMM: Need to deregister records and services on shutdown/sleep
Additional fixes: When going to sleep, mDNSResponder needs to postpone sleep
until TLS/TCP deregistrations have completed (up to five seconds maximum)

Revision 1.520  2007/12/10 23:01:01  cheshire
Remove some unnecessary log messages

Revision 1.519  2007/12/06 00:22:27  mcguire
<rdar://problem/5604567> BTMM: Doesn't work with Linksys WAG300N 1.01.06 (sending from 1026/udp)

Revision 1.518  2007/12/05 01:52:30  cheshire
<rdar://problem/5624763> BTMM: getaddrinfo_async_start returns EAI_NONAME when resolving BTMM hostname
Delay returning IPv4 address ("A") results for autotunnel names until after we've set up the tunnel (or tried to)

Revision 1.517  2007/12/03 18:37:26  cheshire
Moved mDNSPlatformWriteLogMsg & mDNSPlatformWriteDebugMsg
from mDNSMacOSX.c to PlatformCommon.c, so that Posix build can use them

Revision 1.516  2007/12/01 01:21:27  jgraessley
<rdar://problem/5623140> mDNSResponder unicast DNS improvements

Revision 1.515  2007/12/01 00:40:00  cheshire
Add mDNSPlatformWriteLogMsg & mDNSPlatformWriteDebugMsg abstractions, to facilitate EFI conversion

Revision 1.514  2007/12/01 00:38:32  cheshire
Fixed compile warning: declaration of 'index' shadows a global declaration

Revision 1.513  2007/11/27 00:08:49  jgraessley
<rdar://problem/5613538> Interface-specific resolvers not setup correctly

Revision 1.512  2007/11/16 22:09:26  cheshire
Added missing type information in mDNSPlatformTCPCloseConnection debugging log message

Revision 1.511  2007/11/14 23:06:13  cheshire
<rdar://problem/5585972> IP_ADD_MEMBERSHIP fails for previously-connected removable interfaces

Revision 1.510  2007/11/14 22:29:19  cheshire
Updated comments and debugging log messages

Revision 1.509  2007/11/14 01:07:53  cheshire
Updated comments

Revision 1.508  2007/11/02 21:59:37  cheshire
Added comment about locking

Revision 1.507  2007/11/02 20:18:13  cheshire
<rdar://problem/5575583> BTMM: Work around keychain notification bug <rdar://problem/5124399>

Revision 1.506  2007/10/30 20:46:45  cheshire
<rdar://problem/5496734> BTMM: Need to retry registrations after failures

Revision 1.505  2007/10/29 23:55:10  cheshire
<rdar://problem/5526791> BTMM: Changing Local Hostname doesn't update Back to My Mac registered records
Don't need to manually fake another AutoTunnelNATCallback if it has not yet received its first callback
(and indeed should not, since the result fields will not yet be set up correctly in this case)

Revision 1.504  2007/10/26 00:50:37  cheshire
<rdar://problem/5526791> BTMM: Changing Local Hostname doesn't update Back to My Mac registered records

Revision 1.503  2007/10/25 23:11:42  cheshire
Ignore IPv6 ULA addresses configured on lo0 loopback interface

Revision 1.502  2007/10/22 20:07:07  cheshire
Moved mDNSPlatformSourceAddrForDest from mDNSMacOSX.c to PlatformCommon.c so
Posix build can share the code (better than just pasting it into mDNSPosix.c)

Revision 1.501  2007/10/22 19:40:30  cheshire
<rdar://problem/5519458> BTMM: Machines don't appear in the sidebar on wake from sleep
Made subroutine mDNSPlatformSourceAddrForDest(mDNSAddr *const src, const mDNSAddr *const dst)

Revision 1.500  2007/10/17 22:49:55  cheshire
<rdar://problem/5519458> BTMM: Machines don't appear in the sidebar on wake from sleep

Revision 1.499  2007/10/17 19:47:54  cheshire
Improved debugging messages

Revision 1.498  2007/10/17 18:42:06  cheshire
Export SetDomainSecrets so its callable from other files

Revision 1.497  2007/10/16 17:03:07  cheshire
<rdar://problem/3557903> Performance: Core code will not work on platforms with small stacks
Cut SetDomainSecrets stack from 3792 to 1760 bytes

Revision 1.496  2007/10/04 20:33:05  mcguire
<rdar://problem/5518845> BTMM: Racoon configuration removed when network changes

Revision 1.495  2007/10/02 05:03:38  cheshire
Fix bogus indentation in mDNSPlatformDynDNSHostNameStatusChanged

Revision 1.494  2007/09/29 20:40:19  cheshire
<rdar://problem/5513378> Crash in ReissueBlockedQuestions

Revision 1.493  2007/09/29 03:16:45  cheshire
<rdar://problem/5513168> BTMM: mDNSResponder memory corruption in GetAuthInfoForName_internal
When AutoTunnel information changes, wait for record deregistrations to complete before registering new data

Revision 1.492  2007/09/28 23:58:35  mcguire
<rdar://problem/5505280> BTMM: v6 address and security policies being setup too soon
Fix locking issue.

Revision 1.491  2007/09/27 23:28:53  mcguire
<rdar://problem/5508042> BTMM: Anonymous racoon configuration not always cleaned up correctly

Revision 1.490  2007/09/26 23:01:21  mcguire
<rdar://problem/5505280> BTMM: v6 address and security policies being setup too soon

Revision 1.489  2007/09/26 22:58:16  mcguire
<rdar://problem/5505092> BTMM: Client tunnels being created to ::0 via 0.0.0.0

Revision 1.488  2007/09/26 00:32:45  cheshire
Rearrange struct TCPSocket_struct so "TCPSocketFlags flags" comes first (needed for debug logging)

Revision 1.487  2007/09/21 17:07:41  mcguire
<rdar://problem/5487354> BTMM: Need to modify IPSec tunnel setup files when shared secret changes (server-role)

Revision 1.486  2007/09/19 23:17:38  cheshire
<rdar://problem/5482131> BTMM: Crash when switching .Mac accounts

Revision 1.485  2007/09/19 21:44:29  cheshire
Improved "mDNSKeychainGetSecrets failed" error message

Revision 1.484  2007/09/18 21:44:55  cheshire
<rdar://problem/5469006> Crash in GetAuthInfoForName_internal
Code was using n->ExtPort (now n->RequestedPort) when it should have been using n->ExternalPort

Revision 1.483  2007/09/17 22:19:39  mcguire
<rdar://problem/5482519> BTMM: Tunnel is getting configured too much which causes long delays
No need to configure a tunnel again if all the parameters are the same -- just remove the older duplicate tunnel from the list.

Revision 1.482  2007/09/14 21:16:03  cheshire
<rdar://problem/5413170> mDNSResponder using 100% CPU spinning in tlsReadSock

Revision 1.481  2007/09/14 21:14:56  mcguire
<rdar://problem/5481318> BTMM: Need to modify IPSec tunnel setup files when shared secret changes

Revision 1.480  2007/09/13 00:16:42  cheshire
<rdar://problem/5468706> Miscellaneous NAT Traversal improvements

Revision 1.479  2007/09/12 19:22:20  cheshire
Variable renaming in preparation for upcoming fixes e.g. priv/pub renamed to intport/extport
Made NAT Traversal packet handlers take typed data instead of anonymous "mDNSu8 *" byte pointers

Revision 1.478  2007/09/07 22:21:45  vazquez
<rdar://problem/5460830> BTMM: Connection stops working after connecting VPN

Revision 1.477  2007/09/07 21:22:30  cheshire
<rdar://problem/5460210> BTMM: SetupSocket 5351 failed; Can't allocate UDP multicast socket spew on wake from sleep with internet sharing on
Don't log failures binding to port 5351

Revision 1.476  2007/09/06 20:38:08  cheshire
<rdar://problem/5439021> Only call SetDomainSecrets() for Keychain changes that are relevant to mDNSResponder

Revision 1.475  2007/09/05 02:24:28  cheshire
<rdar://problem/5457287> mDNSResponder taking up 100% CPU in ReissueBlockedQuestions
In ReissueBlockedQuestions, only restart questions marked NoAnswer_Suspended, not those marked NoAnswer_Fail

Revision 1.474  2007/09/04 22:32:58  mcguire
<rdar://problem/5453633> BTMM: BTMM overwrites /etc/racoon/remote/anonymous.conf

Revision 1.473  2007/08/31 19:53:15  cheshire
<rdar://problem/5431151> BTMM: IPv6 address lookup should not succeed if autotunnel cannot be setup
If AutoTunnel setup fails, the code now generates a fake NXDomain error saying that the requested AAAA record does not exist

Revision 1.472  2007/08/31 18:49:49  vazquez
<rdar://problem/5393719> BTMM: Need to properly deregister when stopping BTMM

Revision 1.471  2007/08/31 02:05:46  cheshire
Need to hold mDNS_Lock when calling mDNS_AddDynDNSHostName

Revision 1.470  2007/08/30 22:50:04  mcguire
<rdar://problem/5430628> BTMM: Tunneled services are registered when autotunnel can't be setup

Revision 1.469  2007/08/30 19:40:51  cheshire
Added syslog messages to report various initialization failures

Revision 1.468  2007/08/30 00:12:20  cheshire
Check error codes and log failures during AutoTunnel setup

Revision 1.467  2007/08/28 00:33:04  jgraessley
<rdar://problem/5423932> Selective compilation options

Revision 1.466  2007/08/24 23:25:55  cheshire
Debugging messages to help track down duplicate items being read from system keychain

Revision 1.465  2007/08/24 00:39:12  cheshire
Added comment explaining why we set info->AutoTunnelService.resrec.RecordType to kDNSRecordTypeUnregistered

Revision 1.464  2007/08/24 00:15:21  cheshire
Renamed GetAuthInfoForName() to GetAuthInfoForName_internal() to make it clear that it may only be called with the lock held

Revision 1.463  2007/08/23 21:02:35  cheshire
SecKeychainSetPreferenceDomain() call should be in platform-support layer, not daemon.c

Revision 1.462  2007/08/18 01:02:03  mcguire
<rdar://problem/5415593> No Bonjour services are getting registered at boot

Revision 1.461  2007/08/10 22:25:57  mkrochma
<rdar://problem/5396302> mDNSResponder continually complains about slow UDP packet reception -- about 400 msecs

Revision 1.460  2007/08/08 22:34:59  mcguire
<rdar://problem/5197869> Security: Run mDNSResponder as user id mdnsresponder instead of root

Revision 1.459  2007/08/08 21:07:48  vazquez
<rdar://problem/5244687> BTMM: Need to advertise model information via wide-area bonjour

Revision 1.458  2007/08/03 02:18:41  mcguire
<rdar://problem/5381687> BTMM: Use port numbers in IPsec policies & configuration files

Revision 1.457  2007/08/02 16:48:45  mcguire
<rdar://problem/5329526> BTMM: Don't try to create tunnel back to same machine

Revision 1.456  2007/08/02 03:28:30  vazquez
Make ExternalAddress and err unused to fix build warnings

Revision 1.455  2007/08/01 03:09:22  cheshire
<rdar://problem/5344587> BTMM: Create NAT port mapping for autotunnel port

Revision 1.454  2007/07/31 23:08:34  mcguire
<rdar://problem/5329542> BTMM: Make AutoTunnel mode work with multihoming

Revision 1.453  2007/07/31 19:13:58  mkrochma
No longer need to include "btmm" in hostname to avoid name conflicts

Revision 1.452  2007/07/27 19:30:41  cheshire
Changed mDNSQuestionCallback parameter from mDNSBool to QC_result,
to properly reflect tri-state nature of the possible responses

Revision 1.451  2007/07/25 22:25:45  cheshire
<rdar://problem/5360853> BTMM: Code not cleaning up old racoon files

Revision 1.450  2007/07/25 21:19:10  cheshire
<rdar://problem/5359507> Fails to build with NO_SECURITYFRAMEWORK: 'IsTunnelModeDomain' defined but not used

Revision 1.449  2007/07/25 01:36:09  mcguire
<rdar://problem/5345290> BTMM: Replace popen() `setkey` calls to setup/teardown ipsec policies

Revision 1.448  2007/07/24 21:30:09  cheshire
Added "AutoTunnel server listening for connections..." diagnostic message

Revision 1.447  2007/07/24 20:24:18  cheshire
Only remove AutoTunnel address if we have created it.
Otherwise, we get "errno 49 (Can't assign requested address)" errors on exit.

Revision 1.446  2007/07/24 03:00:09  cheshire
SetDomainSecrets() should call SetupLocalAutoTunnelInterface_internal(), not SetupLocalAutoTunnelInterface()

Revision 1.445  2007/07/23 20:26:26  cheshire
<rdar://problem/4641118> Need separate SCPreferences for per-user .Mac settings
Move code that reads "Setup:/Network/BackToMyMac" preferences outside the check
for existence of "Setup:/Network/DynamicDNS" settings

Revision 1.444  2007/07/21 00:54:49  cheshire
<rdar://problem/5344576> Delay IPv6 address callback until AutoTunnel route and policy is configured

Revision 1.443  2007/07/20 23:23:11  cheshire
Rename out-of-date name "atq" (was AutoTunnelQuery) to simpler "tun"

Revision 1.442  2007/07/20 20:23:24  cheshire
<rdar://problem/4641118> Need separate SCPreferences for per-user .Mac settings
Fixed errors reading the Setup:/Network/BackToMyMac preferences

Revision 1.441  2007/07/20 16:46:45  mcguire
<rdar://problem/5345233> BTMM: Replace system() `route` calls to setup/teardown routes

Revision 1.440  2007/07/20 16:22:07  mcguire
<rdar://problem/5344584> BTMM: Replace system() `ifconfig` calls to setup/teardown IPv6 address

Revision 1.439  2007/07/20 01:14:56  cheshire
<rdar://problem/4641118> Need separate SCPreferences for per-user .Mac settings
Cleaned up log messages

Revision 1.438  2007/07/20 00:54:21  cheshire
<rdar://problem/4641118> Need separate SCPreferences for per-user .Mac settings

Revision 1.437  2007/07/19 22:01:27  cheshire
Added "#pragma mark" sections headings to divide code into related function groups

Revision 1.436  2007/07/18 03:25:25  cheshire
<rdar://problem/5304766> Register IPSec tunnel with IPv4-only hostname and create NAT port mappings
Bring up server-side tunnel on demand, when necessary

Revision 1.435  2007/07/18 01:05:08  cheshire
<rdar://problem/5303834> Automatically configure IPSec policy when resolving services
Add list of client tunnels so we can automatically reconfigure when local address changes

Revision 1.434  2007/07/16 20:16:00  vazquez
<rdar://problem/3867231> LegacyNATTraversal: Need complete rewrite
Remove unnecessary LNT init code

Revision 1.433  2007/07/14 00:36:07  cheshire
Remove temporary IPv4LL tunneling mode now that IPv6-over-IPv4 is working

Revision 1.432  2007/07/12 23:55:11  cheshire
<rdar://problem/5303834> Automatically configure IPSec policy when resolving services
Don't need two separate DNSQuestion structures when looking up tunnel endpoint

Revision 1.431  2007/07/12 23:34:48  cheshire
Removed 'LogOperation' message to reduce verbosity in syslog

Revision 1.430  2007/07/12 22:16:46  cheshire
Improved "could not convert shared secret from base64" log message so it doesn't reveal key data in syslog

Revision 1.429  2007/07/12 02:51:28  cheshire
<rdar://problem/5303834> Automatically configure IPSec policy when resolving services

Revision 1.428  2007/07/11 23:17:31  cheshire
<rdar://problem/5304766> Register IPSec tunnel with IPv4-only hostname and create NAT port mappings
Improve log message to indicate if we're starting or restarting racoon

Revision 1.427  2007/07/11 22:50:30  cheshire
<rdar://problem/5304766> Register IPSec tunnel with IPv4-only hostname and create NAT port mappings
Write /etc/racoon/remote/anonymous.conf configuration file and start up /usr/sbin/racoon

Revision 1.426  2007/07/11 20:40:49  cheshire
<rdar://problem/5304766> Register IPSec tunnel with IPv4-only hostname and create NAT port mappings
In mDNSPlatformGetPrimaryInterface(), prefer routable IPv4 address to IPv4LL

Revision 1.425  2007/07/11 19:24:19  cheshire
<rdar://problem/5303807> Register IPv6-only hostname and don't create port mappings for services
Configure internal AutoTunnel address
(For temporary testing we're faking up an IPv4LL address instead of IPv6 ULA, and we're
assigning it with "system(commandstring);" which probably isn't the most efficient way to do it)

Revision 1.424  2007/07/11 19:00:27  cheshire
Only need to set up m->AutoTunnelHostAddr first time through UpdateInterfaceList()

Revision 1.423  2007/07/11 03:00:59  cheshire
<rdar://problem/5303807> Register IPv6-only hostname and don't create port mappings for AutoTunnel services
Add AutoTunnel parameter to mDNS_SetSecretForDomain; Generate IPv6 ULA address for tunnel endpoint

Revision 1.422  2007/07/10 01:21:20  cheshire
Added (commented out) line for displaying key data for debugging

Revision 1.421  2007/06/25 20:58:11  cheshire
<rdar://problem/5234463> Write the Multicast DNS domains to the DynamicStore
Additional refinement: Add mDNS domain list new new DynamicStore entity "State:/Network/MulticastDNS"

Revision 1.420  2007/06/22 21:52:14  cheshire
<rdar://problem/5234463> Write the Multicast DNS domains to the DynamicStore

Revision 1.419  2007/06/22 21:32:00  cheshire
<rdar://problem/5239020> Use SecKeychainCopyDefault instead of SecKeychainOpen

Revision 1.418  2007/06/21 16:37:43  jgraessley
Bug #: 5280520
Reviewed by: Stuart Cheshire
Additional changes to get this compiling on the embedded platform.

Revision 1.417  2007/06/20 01:44:00  cheshire
More information in "Network Configuration Change" message

Revision 1.416  2007/06/20 01:10:12  cheshire
<rdar://problem/5280520> Sync iPhone changes into main mDNSResponder code

Revision 1.415  2007/06/15 19:23:38  cheshire
<rdar://problem/5254053> mDNSResponder renames my host without asking
Improve log messages, to distinguish user-initiated renames from automatic (name conflict) renames

Revision 1.414  2007/05/17 22:00:59  cheshire
<rdar://problem/5210966> Lower network change delay from two seconds to one second

Revision 1.413  2007/05/16 16:43:27  cheshire
Only log "bind" failures for our shared mDNS port and for binding to zero
-- other attempts to bind to a particular port may legitimately fail

Revision 1.412  2007/05/15 21:49:21  cheshire
Get rid of "#pragma unused"

Revision 1.411  2007/05/14 23:54:55  cheshire
Instead of sprintf, use safer length-limited mDNS_snprintf

Revision 1.410  2007/05/12 01:05:00  cheshire
Updated debugging messages

Revision 1.409  2007/05/10 22:39:48  cheshire
<rdar://problem/4118503> Share single socket instead of creating separate socket for each active interface
Only define CountMaskBits for builds with debugging messages

Revision 1.408  2007/05/10 22:19:00  cheshire
<rdar://problem/4118503> Share single socket instead of creating separate socket for each active interface
Don't deliver multicast packets for which we can't find an associated InterfaceID

Revision 1.407  2007/05/10 21:40:28  cheshire
Don't log unnecessary "Address already in use" errors when joining multicast groups

Revision 1.406  2007/05/08 00:56:17  cheshire
<rdar://problem/4118503> Share single socket instead of creating separate socket for each active interface

Revision 1.405  2007/05/04 20:21:39  cheshire
Improve "connect failed" error message

Revision 1.404  2007/05/02 19:41:53  cheshire
No need to alarm people with "Connection reset by peer" syslog message

Revision 1.403  2007/04/28 01:31:59  cheshire
Improve debugging support for catching memory corruption problems

Revision 1.402  2007/04/26 22:54:57  cheshire
Debugging messages to help track down <rdar://problem/5164206> mDNSResponder takes 50%+ CPU

Revision 1.401  2007/04/26 00:35:16  cheshire
<rdar://problem/5140339> uDNS: Domain discovery not working over VPN
Fixes to make sure results update correctly when connectivity changes (e.g. a DNS server
inside the firewall may give answers where a public one gives none, and vice versa.)

Revision 1.400  2007/04/24 21:50:27  cheshire
Debugging: Show list of changedKeys in NetworkChanged callback

Revision 1.399  2007/04/23 22:28:47  cheshire
Allan Nathanson informs us we should only be looking at the search list for resolver[0], not all of them

Revision 1.398  2007/04/23 04:57:00  cheshire
Log messages for debugging <rdar://problem/4570952> IPv6 multicast not working properly

Revision 1.397  2007/04/22 06:02:03  cheshire
<rdar://problem/4615977> Query should immediately return failure when no server

Revision 1.396  2007/04/21 21:47:47  cheshire
<rdar://problem/4376383> Daemon: Add watchdog timer

Revision 1.395  2007/04/18 20:58:34  cheshire
<rdar://problem/5140339> Domain discovery not working over VPN
Needed different code to handle the case where there's only a single search domain

Revision 1.394  2007/04/17 23:05:50  cheshire
<rdar://problem/3957358> Shouldn't send domain queries when we have 169.254 or loopback address

Revision 1.393  2007/04/17 19:21:29  cheshire
<rdar://problem/5140339> Domain discovery not working over VPN

Revision 1.392  2007/04/17 17:15:09  cheshire
Change NO_CFUSERNOTIFICATION code so it still logs to syslog

Revision 1.391  2007/04/07 01:01:48  cheshire
<rdar://problem/5095167> mDNSResponder periodically blocks in SSLRead

Revision 1.390  2007/04/06 18:45:02  cheshire
Fix SetupActiveInterfaces() -- accidentally changed SetupSocket parameter

Revision 1.389  2007/04/05 21:39:49  cheshire
Debugging messages to help diagnose <rdar://problem/5095167> mDNSResponder periodically blocks in SSLRead

Revision 1.388  2007/04/05 21:09:52  cheshire
Condense sprawling code

Revision 1.387  2007/04/05 20:40:37  cheshire
Remove unused mDNSPlatformTCPGetFlags()

Revision 1.386  2007/04/05 19:50:56  cheshire
Fixed memory leak: GetCertChain() was not releasing cert returned by SecIdentityCopyCertificate()

Revision 1.385  2007/04/03 19:39:19  cheshire
Fixed intel byte order bug in mDNSPlatformSetDNSServers()

Revision 1.384  2007/03/31 01:10:53  cheshire
Add debugging

Revision 1.383  2007/03/31 00:13:48  cheshire
Remove LogMsg

Revision 1.382  2007/03/28 21:01:29  cheshire
<rdar://problem/4743285> Remove inappropriate use of IsPrivateV4Addr()

Revision 1.381  2007/03/28 15:56:37  cheshire
<rdar://problem/5085774> Add listing of NAT port mapping and GetAddrInfo requests in SIGINFO output

Revision 1.380  2007/03/26 22:54:46  cheshire
Fix compile error

Revision 1.379  2007/03/22 18:31:48  cheshire
Put dst parameter first in mDNSPlatformStrCopy/mDNSPlatformMemCopy, like conventional Posix strcpy/memcpy

Revision 1.378  2007/03/22 00:49:20  cheshire
<rdar://problem/4848295> Advertise model information via Bonjour

Revision 1.377  2007/03/21 00:30:05  cheshire
<rdar://problem/4789455> Multiple errors in DNameList-related code

Revision 1.376  2007/03/20 17:07:15  cheshire
Rename "struct uDNS_TCPSocket_struct" to "TCPSocket", "struct uDNS_UDPSocket_struct" to "UDPSocket"

Revision 1.375  2007/03/20 00:50:57  cheshire
<rdar://problem/4530644> Remove logic to disable IPv6 discovery on interfaces which have a routable IPv4 address

Revision 1.374  2007/03/06 23:29:50  cheshire
<rdar://problem/4331696> Need to call IONotificationPortDestroy on shutdown

Revision 1.373  2007/02/28 01:51:20  cheshire
Added comment about reverse-order IP address

Revision 1.372  2007/02/28 01:06:48  cheshire
Use %#a format code instead of %d.%d.%d.%d

Revision 1.371  2007/02/08 21:12:28  cheshire
<rdar://problem/4386497> Stop reading /etc/mDNSResponder.conf on every sleep/wake

Revision 1.370  2007/01/16 22:59:58  cheshire
Error code ioErr is from wrong conceptual namespace; use errSSLClosedAbort instead

Revision 1.369  2007/01/10 02:09:32  cheshire
Better LogOperation record of keys read from System Keychain

Revision 1.368  2007/01/10 01:25:31  cheshire
Use symbol kDNSServiceCompPrivateDNS instead of fixed string "State:/Network/PrivateDNS"

Revision 1.367  2007/01/10 01:22:01  cheshire
Make sure c1, c2, c3 are initialized

Revision 1.366  2007/01/09 22:37:20  cheshire
Provide ten-second grace period for deleted keys, to give mDNSResponder
time to delete host name before it gives up access to the required key.

Revision 1.365  2007/01/09 21:09:20  cheshire
Need locking in KeychainChanged()

Revision 1.364  2007/01/09 20:17:04  cheshire
mDNSPlatformGetDNSConfig() needs to initialize fields even when no "Setup:/Network/DynamicDNS" entity exists

Revision 1.363  2007/01/09 02:41:18  cheshire
uDNS_SetupDNSConfig() shouldn't be called from mDNSMacOSX.c (platform support layer);
moved it to mDNS_Init() in mDNS.c (core code)

Revision 1.362  2007/01/08 23:54:01  cheshire
Made mDNSPlatformGetDNSConfig() more selective -- only reads prefs for non-null parameters

Revision 1.361  2007/01/05 08:30:48  cheshire
Trim excessive "$Log" checkin history from before 2006
(checkin history still available via "cvs log ..." of course)

Revision 1.360  2007/01/04 00:12:24  cheshire
<rdar://problem/4742742> Read *all* DNS keys from keychain,
 not just key for the system-wide default registration domain

Revision 1.359  2006/12/22 21:14:37  cheshire
Added comment explaining why we allow both "ddns" and "sndd" as valid item types
The Keychain APIs on Intel appear to store the four-character item type backwards (at least some of the time)

Revision 1.358  2006/12/22 20:59:50  cheshire
<rdar://problem/4742742> Read *all* DNS keys from keychain,
 not just key for the system-wide default registration domain

Revision 1.357  2006/12/21 00:09:45  cheshire
Use mDNSPlatformMemZero instead of bzero

Revision 1.356  2006/12/20 23:15:53  mkrochma
Fix the private domain list code so that it actually works

Revision 1.355  2006/12/20 23:04:36  mkrochma
Fix crash when adding private domain list to Dynamic Store

Revision 1.354  2006/12/19 22:43:55  cheshire
Fix compiler warnings

Revision 1.353  2006/12/14 22:08:29  cheshire
Fixed memory leak: need to call SecKeychainItemFreeAttributesAndData()
to release data allocated by SecKeychainItemCopyAttributesAndData()

Revision 1.352  2006/12/14 02:33:26  cheshire
<rdar://problem/4841422> uDNS: Wide-area registrations sometimes fail

Revision 1.351  2006/11/28 21:37:51  mkrochma
Tweak where the private DNS data is written

Revision 1.350  2006/11/28 07:55:02  herscher
<rdar://problem/4742743> dnsextd has a slow memory leak

Revision 1.349  2006/11/28 07:45:58  herscher
<rdar://problem/4787010> Daemon: Need to write list of private domain names to the DynamicStore

Revision 1.348  2006/11/16 21:47:20  mkrochma
<rdar://problem/4841422> uDNS: Wide-area registrations sometimes fail

Revision 1.347  2006/11/10 00:54:16  cheshire
<rdar://problem/4816598> Changing case of Computer Name doesn't work

Revision 1.346  2006/10/31 02:34:58  cheshire
<rdar://problem/4692130> Stop creating HINFO records

Revision 1.345  2006/09/21 20:04:38  mkrochma
Accidently changed function name while checking in previous fix

Revision 1.344  2006/09/21 19:04:13  mkrochma
<rdar://problem/4733803> uDNS: Update keychain format of DNS key to include prefix

Revision 1.343  2006/09/15 21:20:16  cheshire
Remove uDNS_info substructure from mDNS_struct

Revision 1.342  2006/08/16 00:31:50  mkrochma
<rdar://problem/4386944> Get rid of NotAnInteger references

Revision 1.341  2006/08/14 23:24:40  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.340  2006/07/29 19:11:13  mkrochma
Change GetUserSpecifiedDDNSConfig LogMsg to debugf

Revision 1.339  2006/07/27 03:24:35  cheshire
<rdar://problem/4049048> Convert mDNSResponder to use kqueue
Further refinement: Declare KQueueEntry parameter "const"

Revision 1.338  2006/07/27 02:59:25  cheshire
<rdar://problem/4049048> Convert mDNSResponder to use kqueue
Further refinements: CFRunLoop thread needs to explicitly wake the kqueue thread
after releasing BigMutex, in case actions it took have resulted in new work for the
kqueue thread (e.g. NetworkChanged events may result in the kqueue thread having to
add new active interfaces to its list, and consequently schedule queries to be sent).

Revision 1.337  2006/07/22 06:11:37  cheshire
<rdar://problem/4049048> Convert mDNSResponder to use kqueue

Revision 1.336  2006/07/15 02:01:32  cheshire
<rdar://problem/4472014> Add Private DNS client functionality to mDNSResponder
Fix broken "empty string" browsing

Revision 1.335  2006/07/14 05:25:11  cheshire
<rdar://problem/4472014> Add Private DNS client functionality to mDNSResponder
Fixed crash in mDNSPlatformGetDNSConfig() reading BrowseDomains array

Revision 1.334  2006/07/05 23:42:00  cheshire
<rdar://problem/4472014> Add Private DNS client functionality to mDNSResponder

Revision 1.333  2006/06/29 05:33:30  cheshire
<rdar://problem/4607043> mDNSResponder conditional compilation options

Revision 1.332  2006/06/28 09:10:36  cheshire
Extra debugging messages

Revision 1.331  2006/06/21 22:29:42  cheshire
Make _CFCopySystemVersionDictionary() call more defensive on systems that have no build information set

Revision 1.330  2006/06/20 23:06:00  cheshire
Fix some keychain API type mismatches (was mDNSu32 instead of UInt32)

Revision 1.329  2006/06/08 23:22:33  cheshire
Comment changes

Revision 1.328  2006/03/19 03:27:49  cheshire
<rdar://problem/4118624> Suppress "interface flapping" logic for loopback

Revision 1.327  2006/03/19 02:00:09  cheshire
<rdar://problem/4073825> Improve logic for delaying packets after repeated interface transitions

Revision 1.326  2006/03/08 22:42:23  cheshire
Fix spelling mistake: LocalReverseMapomain -> LocalReverseMapDomain

Revision 1.325  2006/01/10 00:39:17  cheshire
Add comments explaining how IPv6 link-local addresses sometimes have an embedded scope_id

Revision 1.324  2006/01/09 19:28:59  cheshire
<rdar://problem/4403128> Cap number of "sendto failed" messages we allow mDNSResponder to log

Revision 1.323  2006/01/05 21:45:27  cheshire
<rdar://problem/4400118> Fix uninitialized structure member in IPv6 code

Revision 1.322  2006/01/05 21:41:50  cheshire
<rdar://problem/4108164> Reword "mach_absolute_time went backwards" dialog

Revision 1.321  2006/01/05 21:35:06  cheshire
Add (commented out) trigger value for testing "mach_absolute_time went backwards" notice

*/

// ***************************************************************************
// mDNSMacOSX.c:
// Supporting routines to run mDNS on a CFRunLoop platform
// ***************************************************************************

// For debugging, set LIST_ALL_INTERFACES to 1 to display all found interfaces,
// including ones that mDNSResponder chooses not to use.
#define LIST_ALL_INTERFACES 0

// For enabling AAAA records over IPv4. Setting this to 0 sends only
// A records over IPv4 and AAAA over IPv6. Setting this to 1 sends both
// AAAA and A records over both IPv4 and IPv6.
#define AAAA_OVER_V4 1

// In Mac OS X 10.4 and earlier, to reduce traffic, we would send and receive using IPv6 only on interfaces that had no routable
// IPv4 address. Having a routable IPv4 address assigned is a reasonable indicator of being on a large configured network,
// which means there's a good chance that most or all the other devices on that network should also have IPv4.
// By doing this we lost the ability to talk to true IPv6-only devices on that link, but we cut the packet rate in half.
// At that time, reducing the packet rate was more important than v6-only devices on a large configured network,
// so were willing to make that sacrifice.
// In Mac OS X 10.5, in 2007, two things have changed:
// 1. IPv6-only devices are starting to become more common, so we can't ignore them.
// 2. Other efficiency improvements in the code mean that crude hacks like this should no longer be necessary.

#define USE_V6_ONLY_WHEN_NO_ROUTABLE_V4 0

#include "mDNSEmbeddedAPI.h"		// Defines the interface provided to the client layer above
#include "DNSCommon.h"
#include "uDNS.h"
#include "mDNSMacOSX.h"				// Defines the specific types needed to run mDNS on this platform
#include "dns_sd.h"					// For mDNSInterface_LocalOnly etc.
#include "PlatformCommon.h"

#include <stdio.h>
#include <stdarg.h>                 // For va_list support
#include <stdlib.h>                 // For arc4random
#include <net/if.h>
#include <net/if_types.h>			// For IFT_ETHER
#include <net/if_dl.h>
#include <net/bpf.h>				// For BIOCSETIF etc.
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/event.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>                   // platform support for UTC time
#include <arpa/inet.h>              // for inet_aton
#include <pthread.h>

#include <netinet/in.h>             // For IP_RECVTTL
#ifndef IP_RECVTTL
#define IP_RECVTTL 24               // bool; receive reception TTL w/dgram
#endif

#include <netinet/in_systm.h>       // For n_long, required by <netinet/ip.h> below
#include <netinet/ip.h>             // For IPTOS_LOWDELAY etc.
#include <netinet6/in6_var.h>       // For IN6_IFF_NOTREADY etc.
#include <netinet6/nd6.h>           // For ND6_INFINITE_LIFETIME etc.

#if TARGET_OS_EMBEDDED
#define NO_SECURITYFRAMEWORK 1
#define NO_CFUSERNOTIFICATION 1
#endif

#ifndef NO_SECURITYFRAMEWORK
#include <Security/SecureTransport.h>
#include <Security/Security.h>
#endif /* NO_SECURITYFRAMEWORK */

#include <DebugServices.h>
#include "dnsinfo.h"

// Code contributed by Dave Heller:
// Define RUN_ON_PUMA_WITHOUT_IFADDRS to compile code that will
// work on Mac OS X 10.1, which does not have the getifaddrs call.
#define RUN_ON_PUMA_WITHOUT_IFADDRS 0
#if RUN_ON_PUMA_WITHOUT_IFADDRS
#include "mDNSMacOSXPuma.c"
#else
#include <ifaddrs.h>
#endif

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>

#if USE_IOPMCOPYACTIVEPMPREFERENCES
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>
#endif

#include <mach/mach_error.h>
#include <mach/mach_port.h>
#include <mach/mach_time.h>
#include "helper.h"

#include <asl.h>

#define kInterfaceSpecificOption "interface="

// ***************************************************************************
// Globals

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Globals
#endif

// By default we don't offer sleep proxy service
// If OfferSleepProxyService is set non-zero (typically via command-line switch),
// then we'll offer sleep proxy service on desktop Macs that are set to never sleep.
// We currently do not offer sleep proxy service on laptops, or on machines that are set to go to sleep.
mDNSexport int OfferSleepProxyService = 0;

mDNSexport int OSXVers;
mDNSexport int KQueueFD;

#ifndef NO_SECURITYFRAMEWORK
static CFArrayRef ServerCerts;
#endif /* NO_SECURITYFRAMEWORK */

static CFStringRef NetworkChangedKey_IPv4;
static CFStringRef NetworkChangedKey_IPv6;
static CFStringRef NetworkChangedKey_Hostnames;
static CFStringRef NetworkChangedKey_Computername;
static CFStringRef NetworkChangedKey_DNS;
static CFStringRef NetworkChangedKey_DynamicDNS  = CFSTR("Setup:/Network/DynamicDNS");
static CFStringRef NetworkChangedKey_BackToMyMac = CFSTR("Setup:/Network/BackToMyMac");

static char  HINFO_HWstring_buffer[32];
static char *HINFO_HWstring = "Device";
static int   HINFO_HWstring_prefixlen = 6;

mDNSexport int WatchDogReportingThreshold = 250;

#if APPLE_OSX_mDNSResponder
static mDNSu8 SPMetricPortability   = 99;
static mDNSu8 SPMetricMarginalPower = 99;
static mDNSu8 SPMetricTotalPower    = 99;
mDNSexport domainname ActiveDirectoryPrimaryDomain;
mDNSexport int        ActiveDirectoryPrimaryDomainLabelCount;
mDNSexport mDNSAddr   ActiveDirectoryPrimaryDomainServer;
#endif // APPLE_OSX_mDNSResponder

// ***************************************************************************
// Functions

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Utility Functions
#endif

// We only attempt to send and receive multicast packets on interfaces that are
// (a) flagged as multicast-capable
// (b) *not* flagged as point-to-point (e.g. modem)
// Typically point-to-point interfaces are modems (including mobile-phone pseudo-modems), and we don't want
// to run up the user's bill sending multicast traffic over a link where there's only a single device at the
// other end, and that device (e.g. a modem bank) is probably not answering Multicast DNS queries anyway.
#define MulticastInterface(i) (((i)->ifa_flags & IFF_MULTICAST) && !((i)->ifa_flags & IFF_POINTOPOINT))

mDNSexport void NotifyOfElusiveBug(const char *title, const char *msg)	// Both strings are UTF-8 text
	{
	static int notifyCount = 0;
	if (notifyCount) return;

	// If we display our alert early in the boot process, then it vanishes once the desktop appears.
	// To avoid this, we don't try to display alerts in the first three minutes after boot.
	if ((mDNSu32)(mDNSPlatformRawTime()) < (mDNSu32)(mDNSPlatformOneSecond * 180)) return;

	// Unless ForceAlerts is defined, we only show these bug report alerts on machines that have a 17.x.x.x address
	#if !ForceAlerts
		{
		// Determine if we're at Apple (17.*.*.*)
		extern mDNS mDNSStorage;
		NetworkInterfaceInfoOSX *i;
		for (i = mDNSStorage.p->InterfaceList; i; i = i->next)
			if (i->ifinfo.ip.type == mDNSAddrType_IPv4 && i->ifinfo.ip.ip.v4.b[0] == 17)
				break;
		if (!i) return;	// If not at Apple, don't show the alert
		}
	#endif

	LogMsg("%s", title);
	LogMsg("%s", msg);
	// Display a notification to the user
	notifyCount++;

#ifndef NO_CFUSERNOTIFICATION
	mDNSNotify(title, msg);
#endif /* NO_CFUSERNOTIFICATION */
	}

mDNSlocal struct ifaddrs *myGetIfAddrs(int refresh)
	{
	static struct ifaddrs *ifa = NULL;

	if (refresh && ifa)
		{
		freeifaddrs(ifa);
		ifa = NULL;
		}

	if (ifa == NULL) getifaddrs(&ifa);

	return ifa;
	}

// To match *either* a v4 or v6 instance of this interface name, pass AF_UNSPEC for type
mDNSlocal NetworkInterfaceInfoOSX *SearchForInterfaceByName(mDNS *const m, const char *ifname, int type)
	{
	NetworkInterfaceInfoOSX *i;
	for (i = m->p->InterfaceList; i; i = i->next)
		if (i->Exists && !strcmp(i->ifinfo.ifname, ifname) &&
			((type == AF_UNSPEC                                         ) ||
			 (type == AF_INET  && i->ifinfo.ip.type == mDNSAddrType_IPv4) ||
			 (type == AF_INET6 && i->ifinfo.ip.type == mDNSAddrType_IPv6))) return(i);
	return(NULL);
	}

mDNSlocal int myIfIndexToName(u_short ifindex, char *name)
	{
	struct ifaddrs *ifa;
	for (ifa = myGetIfAddrs(0); ifa; ifa = ifa->ifa_next)
		if (ifa->ifa_addr->sa_family == AF_LINK)
			if (((struct sockaddr_dl*)ifa->ifa_addr)->sdl_index == ifindex)
				{ strlcpy(name, ifa->ifa_name, IF_NAMESIZE); return 0; }
	return -1;
	}

mDNSexport NetworkInterfaceInfoOSX *IfindexToInterfaceInfoOSX(const mDNS *const m, mDNSInterfaceID ifindex)
{
	mDNSu32 scope_id = (mDNSu32)(uintptr_t)ifindex;
	NetworkInterfaceInfoOSX *i;

	// Don't get tricked by inactive interfaces
	for (i = m->p->InterfaceList; i; i = i->next)
		if (i->Registered && i->scope_id == scope_id) return(i);

	return mDNSNULL;
}

mDNSexport mDNSInterfaceID mDNSPlatformInterfaceIDfromInterfaceIndex(mDNS *const m, mDNSu32 ifindex)
	{
	if (ifindex == kDNSServiceInterfaceIndexLocalOnly) return(mDNSInterface_LocalOnly);
	if (ifindex == kDNSServiceInterfaceIndexAny      ) return(mDNSNULL);

	NetworkInterfaceInfoOSX* ifi = IfindexToInterfaceInfoOSX(m, (mDNSInterfaceID)(uintptr_t)ifindex);
	if (!ifi)
		{
		// Not found. Make sure our interface list is up to date, then try again.
		LogInfo("mDNSPlatformInterfaceIDfromInterfaceIndex: InterfaceID for interface index %d not found; Updating interface list", ifindex);
		mDNSMacOSXNetworkChanged(m);
		ifi = IfindexToInterfaceInfoOSX(m, (mDNSInterfaceID)(uintptr_t)ifindex);
		}

	if (!ifi) return(mDNSNULL);	
	
	return(ifi->ifinfo.InterfaceID);
	}


mDNSexport mDNSu32 mDNSPlatformInterfaceIndexfromInterfaceID(mDNS *const m, mDNSInterfaceID id)
	{
	NetworkInterfaceInfoOSX *i;
	if (id == mDNSInterface_LocalOnly) return(kDNSServiceInterfaceIndexLocalOnly);
	if (id == mDNSInterface_Any      ) return(0);

	mDNSu32 scope_id = (mDNSu32)(uintptr_t)id;

	// Don't use i->Registered here, because we DO want to find inactive interfaces, which have no Registered set
	for (i = m->p->InterfaceList; i; i = i->next)
		if (i->scope_id == scope_id) return(i->scope_id);

	// Not found. Make sure our interface list is up to date, then try again.
	LogInfo("Interface index for InterfaceID %p not found; Updating interface list", id);
	mDNSMacOSXNetworkChanged(m);
	for (i = m->p->InterfaceList; i; i = i->next)
		if (i->scope_id == scope_id) return(i->scope_id);

	return(0);
	}

#if APPLE_OSX_mDNSResponder
mDNSexport void mDNSASLLog(uuid_t *uuid, const char *subdomain, const char *result, const char *signature, const char *fmt, ...)
	{
	if (OSXVers < OSXVers_10_6_SnowLeopard)		return;

	static char		buffer[512];
	aslmsg 			asl_msg = asl_new(ASL_TYPE_MSG);

	if (!asl_msg)	{ LogMsg("mDNSASLLog: asl_new failed"); return; }
	if (uuid)
		{
		char		uuidStr[37];
		uuid_unparse(*uuid, uuidStr);
		asl_set		(asl_msg, "com.apple.message.uuid", uuidStr);
		}

	static char 	domainBase[] = "com.apple.mDNSResponder.%s";
	mDNS_snprintf	(buffer, sizeof(buffer), domainBase, subdomain);
	asl_set			(asl_msg, "com.apple.message.domain", buffer);

	if (result)		asl_set(asl_msg, "com.apple.message.result", result);
	if (signature)	asl_set(asl_msg, "com.apple.message.signature", signature);

	va_list ptr;
	va_start(ptr,fmt);
	mDNS_vsnprintf(buffer, sizeof(buffer), fmt, ptr);
	va_end(ptr);

	int	old_filter = asl_set_filter(NULL,ASL_FILTER_MASK_UPTO(ASL_LEVEL_DEBUG));
	asl_log(NULL, asl_msg, ASL_LEVEL_DEBUG, "%s", buffer);
	asl_set_filter(NULL, old_filter);
	asl_free(asl_msg);
	}
#endif // APPLE_OSX_mDNSResponder

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - UDP & TCP send & receive
#endif

mDNSlocal mDNSBool AddrRequiresPPPConnection(const struct sockaddr *addr)
	{
	mDNSBool result = mDNSfalse;
	SCNetworkConnectionFlags flags;
	SCNetworkReachabilityRef ReachRef = NULL;

	ReachRef = SCNetworkReachabilityCreateWithAddress(kCFAllocatorDefault, addr);
	if (!ReachRef) { LogMsg("ERROR: RequiresConnection - SCNetworkReachabilityCreateWithAddress"); goto end; }
	if (!SCNetworkReachabilityGetFlags(ReachRef, &flags)) { LogMsg("ERROR: AddrRequiresPPPConnection - SCNetworkReachabilityGetFlags"); goto end; }
	result = flags & kSCNetworkFlagsConnectionRequired;

	end:
	if (ReachRef) CFRelease(ReachRef);
	return result;
	}

// Note: If InterfaceID is NULL, it means, "send this packet through our anonymous unicast socket"
// Note: If InterfaceID is non-NULL it means, "send this packet through our port 5353 socket on the specified interface"
// OR send via our primary v4 unicast socket
// UPDATE: The UDPSocket *src parameter now allows the caller to specify the source socket
mDNSexport mStatus mDNSPlatformSendUDP(const mDNS *const m, const void *const msg, const mDNSu8 *const end,
	mDNSInterfaceID InterfaceID, UDPSocket *src, const mDNSAddr *dst, mDNSIPPort dstPort)
	{
	NetworkInterfaceInfoOSX *info = mDNSNULL;
	struct sockaddr_storage to;
	int s = -1, err;
	mStatus result = mStatus_NoError;

	if (InterfaceID)
		{
		info = IfindexToInterfaceInfoOSX(m, InterfaceID);
		if (info == NULL)
			{
			LogMsg("mDNSPlatformSendUDP: Invalid interface index %p", InterfaceID);
			return mStatus_BadParamErr;
			}
		}

	char *ifa_name = InterfaceID ? info->ifinfo.ifname : "unicast";

	if (dst->type == mDNSAddrType_IPv4)
		{
		struct sockaddr_in *sin_to = (struct sockaddr_in*)&to;
		sin_to->sin_len            = sizeof(*sin_to);
		sin_to->sin_family         = AF_INET;
		sin_to->sin_port           = dstPort.NotAnInteger;
		sin_to->sin_addr.s_addr    = dst->ip.v4.NotAnInteger;
		s = (src ? src->ss : m->p->permanentsockets).sktv4;

		if (info)	// Specify outgoing interface
			{
			if (!mDNSAddrIsDNSMulticast(dst))
				{
				#ifdef IP_BOUND_IF
					if (info->scope_id == 0)
						LogInfo("IP_BOUND_IF socket option not set -- info %p (%s) scope_id is zero", info, ifa_name);
					else
						setsockopt(s, IPPROTO_IP, IP_BOUND_IF, &info->scope_id, sizeof(info->scope_id));
				#else
					{
					static int displayed = 0;
					if (displayed < 1000)
						{
						displayed++;
						LogInfo("IP_BOUND_IF socket option not defined -- cannot specify interface for unicast packets");
						}
					}
				#endif
				}
			else
				{
				err = setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &info->ifa_v4addr, sizeof(info->ifa_v4addr));
				if (err < 0 && !m->p->NetworkChanged)
					LogMsg("setsockopt - IP_MULTICAST_IF error %.4a %d errno %d (%s)", &info->ifa_v4addr, err, errno, strerror(errno));
				}
			}
		}
	else if (dst->type == mDNSAddrType_IPv6)
		{
		struct sockaddr_in6 *sin6_to = (struct sockaddr_in6*)&to;
		sin6_to->sin6_len            = sizeof(*sin6_to);
		sin6_to->sin6_family         = AF_INET6;
		sin6_to->sin6_port           = dstPort.NotAnInteger;
		sin6_to->sin6_flowinfo       = 0;
		sin6_to->sin6_addr           = *(struct in6_addr*)&dst->ip.v6;
		sin6_to->sin6_scope_id       = info ? info->scope_id : 0;
		s = (src ? src->ss : m->p->permanentsockets).sktv6;
		if (info && mDNSAddrIsDNSMulticast(dst))	// Specify outgoing interface
			{
			err = setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_IF, &info->scope_id, sizeof(info->scope_id));
			if (err < 0) LogMsg("setsockopt - IPV6_MULTICAST_IF error %d errno %d (%s)", err, errno, strerror(errno));
			}
		}
	else
		{
		LogMsg("mDNSPlatformSendUDP: dst is not an IPv4 or IPv6 address!");
#if ForceAlerts
		*(long*)0 = 0;
#endif
		return mStatus_BadParamErr;
		}

	if (s >= 0)
		verbosedebugf("mDNSPlatformSendUDP: sending on InterfaceID %p %5s/%ld to %#a:%d skt %d",
			InterfaceID, ifa_name, dst->type, dst, mDNSVal16(dstPort), s);
	else
		verbosedebugf("mDNSPlatformSendUDP: NOT sending on InterfaceID %p %5s/%ld (socket of this type not available)",
			InterfaceID, ifa_name, dst->type, dst, mDNSVal16(dstPort));

	// Note: When sending, mDNSCore may often ask us to send both a v4 multicast packet and then a v6 multicast packet
	// If we don't have the corresponding type of socket available, then return mStatus_Invalid
	if (s < 0) return(mStatus_Invalid);

	err = sendto(s, msg, (UInt8*)end - (UInt8*)msg, 0, (struct sockaddr *)&to, to.ss_len);
	if (err < 0)
		{
		static int MessageCount = 0;
		// Don't report EHOSTDOWN (i.e. ARP failure), ENETDOWN, or no route to host for unicast destinations
		if (!mDNSAddressIsAllDNSLinkGroup(dst))
			if (errno == EHOSTDOWN || errno == ENETDOWN || errno == EHOSTUNREACH || errno == ENETUNREACH) return(mStatus_TransientErr);
		// Don't report EHOSTUNREACH in the first three minutes after boot
		// This is because mDNSResponder intentionally starts up early in the boot process (See <rdar://problem/3409090>)
		// but this means that sometimes it starts before configd has finished setting up the multicast routing entries.
		if (errno == EHOSTUNREACH && (mDNSu32)(mDNSPlatformRawTime()) < (mDNSu32)(mDNSPlatformOneSecond * 180)) return(mStatus_TransientErr);
		// Don't report EADDRNOTAVAIL ("Can't assign requested address") if we're in the middle of a network configuration change
		if (errno == EADDRNOTAVAIL && m->p->NetworkChanged) return(mStatus_TransientErr);
		if (MessageCount < 1000)
			{
			MessageCount++;
			if (errno == EHOSTUNREACH || errno == EADDRNOTAVAIL || errno == ENETDOWN)
				LogInfo("mDNSPlatformSendUDP sendto(%d) failed to send packet on InterfaceID %p %5s/%d to %#a:%d skt %d error %d errno %d (%s) %lu",
					s, InterfaceID, ifa_name, dst->type, dst, mDNSVal16(dstPort), s, err, errno, strerror(errno), (mDNSu32)(m->timenow));
			else
				LogMsg("mDNSPlatformSendUDP sendto(%d) failed to send packet on InterfaceID %p %5s/%d to %#a:%d skt %d error %d errno %d (%s) %lu",
					s, InterfaceID, ifa_name, dst->type, dst, mDNSVal16(dstPort), s, err, errno, strerror(errno), (mDNSu32)(m->timenow));
			}
		result = mStatus_UnknownErr;
		}

#ifdef IP_BOUND_IF
	if (dst->type == mDNSAddrType_IPv4 && info && !mDNSAddrIsDNSMulticast(dst))
		{
		static const mDNSu32 ifindex = 0;
		setsockopt(s, IPPROTO_IP, IP_BOUND_IF, &ifindex, sizeof(ifindex));
		}
#endif

	return(result);
	}

mDNSlocal ssize_t myrecvfrom(const int s, void *const buffer, const size_t max,
	struct sockaddr *const from, size_t *const fromlen, mDNSAddr *dstaddr, char ifname[IF_NAMESIZE], mDNSu8 *ttl)
	{
	static unsigned int numLogMessages = 0;
	struct iovec databuffers = { (char *)buffer, max };
	struct msghdr   msg;
	ssize_t         n;
	struct cmsghdr *cmPtr;
	char            ancillary[1024];

	*ttl = 255;  // If kernel fails to provide TTL data (e.g. Jaguar doesn't) then assume the TTL was 255 as it should be

	// Set up the message
	msg.msg_name       = (caddr_t)from;
	msg.msg_namelen    = *fromlen;
	msg.msg_iov        = &databuffers;
	msg.msg_iovlen     = 1;
	msg.msg_control    = (caddr_t)&ancillary;
	msg.msg_controllen = sizeof(ancillary);
	msg.msg_flags      = 0;

	// Receive the data
	n = recvmsg(s, &msg, 0);
	if (n<0)
		{
		if (errno != EWOULDBLOCK && numLogMessages++ < 100) LogMsg("mDNSMacOSX.c: recvmsg(%d) returned error %d errno %d", s, n, errno);
		return(-1);
		}
	if (msg.msg_controllen < (int)sizeof(struct cmsghdr))
		{
		if (numLogMessages++ < 100) LogMsg("mDNSMacOSX.c: recvmsg(%d) returned %d msg.msg_controllen %d < sizeof(struct cmsghdr) %lu",
			s, n, msg.msg_controllen, sizeof(struct cmsghdr));
		return(-1);
		}
	if (msg.msg_flags & MSG_CTRUNC)
		{
		if (numLogMessages++ < 100) LogMsg("mDNSMacOSX.c: recvmsg(%d) msg.msg_flags & MSG_CTRUNC", s);
		return(-1);
		}

	*fromlen = msg.msg_namelen;

	// Parse each option out of the ancillary data.
	for (cmPtr = CMSG_FIRSTHDR(&msg); cmPtr; cmPtr = CMSG_NXTHDR(&msg, cmPtr))
		{
		// debugf("myrecvfrom cmsg_level %d cmsg_type %d", cmPtr->cmsg_level, cmPtr->cmsg_type);
		if (cmPtr->cmsg_level == IPPROTO_IP && cmPtr->cmsg_type == IP_RECVDSTADDR)
			{
			dstaddr->type = mDNSAddrType_IPv4;
			dstaddr->ip.v4 = *(mDNSv4Addr*)CMSG_DATA(cmPtr);
			//LogMsg("mDNSMacOSX.c: recvmsg IP_RECVDSTADDR %.4a", &dstaddr->ip.v4);
			}
		if (cmPtr->cmsg_level == IPPROTO_IP && cmPtr->cmsg_type == IP_RECVIF)
			{
			struct sockaddr_dl *sdl = (struct sockaddr_dl *)CMSG_DATA(cmPtr);
			if (sdl->sdl_nlen < IF_NAMESIZE)
				{
				mDNSPlatformMemCopy(ifname, sdl->sdl_data, sdl->sdl_nlen);
				ifname[sdl->sdl_nlen] = 0;
				// debugf("IP_RECVIF sdl_index %d, sdl_data %s len %d", sdl->sdl_index, ifname, sdl->sdl_nlen);
				}
			}
		if (cmPtr->cmsg_level == IPPROTO_IP && cmPtr->cmsg_type == IP_RECVTTL)
			*ttl = *(u_char*)CMSG_DATA(cmPtr);
		if (cmPtr->cmsg_level == IPPROTO_IPV6 && cmPtr->cmsg_type == IPV6_PKTINFO)
			{
			struct in6_pktinfo *ip6_info = (struct in6_pktinfo*)CMSG_DATA(cmPtr);
			dstaddr->type = mDNSAddrType_IPv6;
			dstaddr->ip.v6 = *(mDNSv6Addr*)&ip6_info->ipi6_addr;
			myIfIndexToName(ip6_info->ipi6_ifindex, ifname);
			}
		if (cmPtr->cmsg_level == IPPROTO_IPV6 && cmPtr->cmsg_type == IPV6_HOPLIMIT)
			*ttl = *(int*)CMSG_DATA(cmPtr);
		}

	return(n);
	}

mDNSlocal void myKQSocketCallBack(int s1, short filter, void *context)
	{
	KQSocketSet *const ss = (KQSocketSet *)context;
	mDNS *const m = ss->m;
	int err = 0, count = 0, closed = 0;

	if (filter != EVFILT_READ)
		LogMsg("myKQSocketCallBack: Why is filter %d not EVFILT_READ (%d)?", filter, EVFILT_READ);

	if (s1 != ss->sktv4 && s1 != ss->sktv6)
		{
		LogMsg("myKQSocketCallBack: native socket %d", s1);
		LogMsg("myKQSocketCallBack: sktv4 %d", ss->sktv4);
		LogMsg("myKQSocketCallBack: sktv6 %d", ss->sktv6);
 		}

	while (!closed)
		{
		mDNSAddr senderAddr, destAddr;
		mDNSIPPort senderPort;
		struct sockaddr_storage from;
		size_t fromlen = sizeof(from);
		char packetifname[IF_NAMESIZE] = "";
		mDNSu8 ttl;
		err = myrecvfrom(s1, &m->imsg, sizeof(m->imsg), (struct sockaddr *)&from, &fromlen, &destAddr, packetifname, &ttl);
		if (err < 0) break;

		count++;
		if (from.ss_family == AF_INET)
			{
			struct sockaddr_in *s = (struct sockaddr_in*)&from;
			senderAddr.type = mDNSAddrType_IPv4;
			senderAddr.ip.v4.NotAnInteger = s->sin_addr.s_addr;
			senderPort.NotAnInteger = s->sin_port;
			//LogInfo("myKQSocketCallBack received IPv4 packet from %#-15a to %#-15a on skt %d %s", &senderAddr, &destAddr, s1, packetifname);
			}
		else if (from.ss_family == AF_INET6)
			{
			struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)&from;
			senderAddr.type = mDNSAddrType_IPv6;
			senderAddr.ip.v6 = *(mDNSv6Addr*)&sin6->sin6_addr;
			senderPort.NotAnInteger = sin6->sin6_port;
			//LogInfo("myKQSocketCallBack received IPv6 packet from %#-15a to %#-15a on skt %d %s", &senderAddr, &destAddr, s1, packetifname);
			}
		else
			{
			LogMsg("myKQSocketCallBack from is unknown address family %d", from.ss_family);
			return;
			}

		// Note: When handling multiple packets in a batch, MUST reset InterfaceID before handling each packet
		mDNSInterfaceID InterfaceID = mDNSNULL;
		//NetworkInterfaceInfo *intf = m->HostInterfaces;
		//while (intf && strcmp(intf->ifname, packetifname)) intf = intf->next;

		NetworkInterfaceInfoOSX *intf = m->p->InterfaceList;
		while (intf && strcmp(intf->ifinfo.ifname, packetifname)) intf = intf->next;

		// When going to sleep we deregister all our interfaces, but if the machine
		// takes a few seconds to sleep we may continue to receive multicasts
		// during that time, which would confuse mDNSCoreReceive, because as far
		// as it's concerned, we should have no active interfaces any more.
		// Hence we ignore multicasts for which we can find no matching InterfaceID.
		if (intf) InterfaceID = intf->ifinfo.InterfaceID;
		else if (mDNSAddrIsDNSMulticast(&destAddr)) continue;

//		LogMsg("myKQSocketCallBack got packet from %#a to %#a on interface %#a/%s",
//			&senderAddr, &destAddr, &ss->info->ifinfo.ip, ss->info->ifinfo.ifname);

		// mDNSCoreReceive may close the socket we're reading from.  We must break out of our
		// loop when that happens, or we may try to read from an invalid FD.  We do this by
		// setting the closeFlag pointer in the socketset, so CloseSocketSet can inform us
		// if it closes the socketset.
		ss->closeFlag = &closed;

		mDNSCoreReceive(m, &m->imsg, (unsigned char*)&m->imsg + err, &senderAddr, senderPort, &destAddr, ss->port, InterfaceID);

		// if we didn't close, we can safely dereference the socketset, and should to
		// reset the closeFlag, since it points to something on the stack
		if (!closed) ss->closeFlag = mDNSNULL;
		}

	if (err < 0 && (errno != EWOULDBLOCK || count == 0))
		{
		// Something is busted here.
		// kqueue says there is a packet, but myrecvfrom says there is not.
		// Try calling select() to get another opinion.
		// Find out about other socket parameter that can help understand why select() says the socket is ready for read
		// All of this is racy, as data may have arrived after the call to select()
		static unsigned int numLogMessages = 0;
		int save_errno = errno;
		int so_error = -1;
		int so_nread = -1;
		int fionread = -1;
		socklen_t solen = sizeof(int);
		fd_set readfds;
		struct timeval timeout;
		int selectresult;
		FD_ZERO(&readfds);
		FD_SET(s1, &readfds);
		timeout.tv_sec  = 0;
		timeout.tv_usec = 0;
		selectresult = select(s1+1, &readfds, NULL, NULL, &timeout);
		if (getsockopt(s1, SOL_SOCKET, SO_ERROR, &so_error, &solen) == -1)
			LogMsg("myKQSocketCallBack getsockopt(SO_ERROR) error %d", errno);
		if (getsockopt(s1, SOL_SOCKET, SO_NREAD, &so_nread, &solen) == -1)
			LogMsg("myKQSocketCallBack getsockopt(SO_NREAD) error %d", errno);
		if (ioctl(s1, FIONREAD, &fionread) == -1)
			LogMsg("myKQSocketCallBack ioctl(FIONREAD) error %d", errno);
		if (numLogMessages++ < 100)
			LogMsg("myKQSocketCallBack recvfrom skt %d error %d errno %d (%s) select %d (%spackets waiting) so_error %d so_nread %d fionread %d count %d",
				s1, err, save_errno, strerror(save_errno), selectresult, FD_ISSET(s1, &readfds) ? "" : "*NO* ", so_error, so_nread, fionread, count);
		if (numLogMessages > 5)
			NotifyOfElusiveBug("Flaw in Kernel (select/recvfrom mismatch)",
				"Congratulations, you've reproduced an elusive bug.\r"
				"Please contact the current assignee of <rdar://problem/3375328>.\r"
				"Alternatively, you can send email to radar-3387020@group.apple.com. (Note number is different.)\r"
				"If possible, please leave your machine undisturbed so that someone can come to investigate the problem.");

		sleep(1);		// After logging this error, rate limit so we don't flood syslog
		}
	}

// TCP socket support

typedef enum
	{
	handshake_required,
	handshake_in_progress,
	handshake_completed,
	handshake_to_be_closed
	} handshakeStatus;
	
struct TCPSocket_struct
	{
	TCPSocketFlags flags;		// MUST BE FIRST FIELD -- mDNSCore expects every TCPSocket_struct to begin with TCPSocketFlags flags
	TCPConnectionCallback callback;
	int fd;
	KQueueEntry kqEntry;
#ifndef NO_SECURITYFRAMEWORK
	SSLContextRef tlsContext;
	pthread_t handshake_thread;
#endif /* NO_SECURITYFRAMEWORK */
	void *context;
	mDNSBool setup;
	mDNSBool connected;
	handshakeStatus handshake;
	mDNS *m; // So we can call KQueueLock from the SSLHandshake thread
	mStatus err;
	};

mDNSlocal void doTcpSocketCallback(TCPSocket *sock)
	{
	mDNSBool c = !sock->connected;
	sock->connected = mDNStrue;
	sock->callback(sock, sock->context, c, sock->err);
	// Note: the callback may call CloseConnection here, which frees the context structure!
	}

#ifndef NO_SECURITYFRAMEWORK

mDNSlocal OSStatus tlsWriteSock(SSLConnectionRef connection, const void *data, size_t *dataLength)
	{
	int ret = send(((TCPSocket *)connection)->fd, data, *dataLength, 0);
	if (ret >= 0 && (size_t)ret < *dataLength) { *dataLength = ret; return(errSSLWouldBlock); }
	if (ret >= 0)                              { *dataLength = ret; return(noErr); }
	*dataLength = 0;
	if (errno == EAGAIN                      ) return(errSSLWouldBlock);
	if (errno == ENOENT                      ) return(errSSLClosedGraceful);
	if (errno == EPIPE || errno == ECONNRESET) return(errSSLClosedAbort);
	LogMsg("ERROR: tlsWriteSock: %d error %d (%s)\n", ((TCPSocket *)connection)->fd, errno, strerror(errno));
	return(errSSLClosedAbort);
	}

mDNSlocal OSStatus tlsReadSock(SSLConnectionRef connection, void *data, size_t *dataLength)
	{
	int ret = recv(((TCPSocket *)connection)->fd, data, *dataLength, 0);
	if (ret > 0 && (size_t)ret < *dataLength) { *dataLength = ret; return(errSSLWouldBlock); }
	if (ret > 0)                              { *dataLength = ret; return(noErr); }
	*dataLength = 0;
	if (ret == 0 || errno == ENOENT    ) return(errSSLClosedGraceful);
	if (            errno == EAGAIN    ) return(errSSLWouldBlock);
	if (            errno == ECONNRESET) return(errSSLClosedAbort);
	LogMsg("ERROR: tlsSockRead: error %d (%s)\n", errno, strerror(errno));
	return(errSSLClosedAbort);
	}

mDNSlocal OSStatus tlsSetupSock(TCPSocket *sock, mDNSBool server)
	{
	mStatus err = SSLNewContext(server, &sock->tlsContext);
	if (err) { LogMsg("ERROR: tlsSetupSock: SSLNewContext failed with error code: %d", err); return(err); }

	err = SSLSetIOFuncs(sock->tlsContext, tlsReadSock, tlsWriteSock);
	if (err) { LogMsg("ERROR: tlsSetupSock: SSLSetIOFuncs failed with error code: %d", err); return(err); }

	err = SSLSetConnection(sock->tlsContext, (SSLConnectionRef) sock);
	if (err) { LogMsg("ERROR: tlsSetupSock: SSLSetConnection failed with error code: %d", err); return(err); }

	return(err);
	}

mDNSlocal void *doSSLHandshake(void *ctx)
	{
	// Warning: Touching sock without the kqueue lock!
	// We're protected because sock->handshake == handshake_in_progress
	TCPSocket *sock = (TCPSocket*)ctx;
	mDNS * const m = sock->m; // Get m now, as we may free sock if marked to be closed while we're waiting on SSLHandshake
	mStatus err = SSLHandshake(sock->tlsContext);
	
	KQueueLock(m);
	debugf("doSSLHandshake %p: got lock", sock); // Log *after* we get the lock

	if (sock->handshake == handshake_to_be_closed)
		{
		LogInfo("SSLHandshake completed after close");
		mDNSPlatformTCPCloseConnection(sock);
		}
	else
		{
		if (sock->fd != -1) KQueueSet(sock->fd, EV_ADD, EVFILT_READ, &sock->kqEntry);
		else LogMsg("doSSLHandshake: sock->fd is -1");

		if (err == errSSLWouldBlock)
			sock->handshake = handshake_required;
		else
			{
			if (err)
				{
				LogMsg("SSLHandshake failed: %d%s", err, err == errSSLPeerInternalError ? " (server busy)" : "");
				SSLDisposeContext(sock->tlsContext);
				sock->tlsContext = NULL;
				}
			
			sock->err = err ? mStatus_ConnFailed : 0;
			sock->handshake = handshake_completed;
			
			debugf("doSSLHandshake: %p calling doTcpSocketCallback", sock);
			doTcpSocketCallback(sock);
			}
		}
	
	debugf("SSLHandshake %p: dropping lock", sock);
	KQueueUnlock(m, "doSSLHandshake");
	return NULL;
	}

mDNSlocal mStatus spawnSSLHandshake(TCPSocket* sock)
	{
	debugf("spawnSSLHandshake %p: entry", sock);
	if (sock->handshake != handshake_required) LogMsg("spawnSSLHandshake: handshake status not required: %d", sock->handshake);
	sock->handshake = handshake_in_progress;
	KQueueSet(sock->fd, EV_DELETE, EVFILT_READ, &sock->kqEntry);
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	mStatus err = pthread_create(&sock->handshake_thread, &attr, doSSLHandshake, sock);
	pthread_attr_destroy(&attr);
	if (err)
		{
		LogMsg("Could not start SSLHandshake thread: (%d) %s", err, strerror(err));
		sock->handshake = handshake_completed;
		sock->err = err;
		KQueueSet(sock->fd, EV_ADD, EVFILT_READ, &sock->kqEntry);
		}
	debugf("spawnSSLHandshake %p: done", sock);
	return err;
	}

mDNSlocal mDNSBool IsTunnelModeDomain(const domainname *d)
	{
	static const domainname *mmc = (const domainname*) "\x7" "members" "\x3" "mac" "\x3" "com";
	const domainname *d1 = mDNSNULL;	// TLD
	const domainname *d2 = mDNSNULL;	// SLD
	const domainname *d3 = mDNSNULL;
	while (d->c[0]) { d3 = d2; d2 = d1; d1 = d; d = (const domainname*)(d->c + 1 + d->c[0]); }
	return(d3 && SameDomainName(d3, mmc));
	}

#endif /* NO_SECURITYFRAMEWORK */

mDNSlocal void tcpKQSocketCallback(__unused int fd, short filter, void *context)
	{
	TCPSocket *sock = context;
	sock->err = mStatus_NoError;

	//if (filter == EVFILT_READ ) LogMsg("myKQSocketCallBack: tcpKQSocketCallback %d is EVFILT_READ", filter);
	//if (filter == EVFILT_WRITE) LogMsg("myKQSocketCallBack: tcpKQSocketCallback %d is EVFILT_WRITE", filter);
	// EV_ONESHOT doesn't seem to work, so we add the filter with EV_ADD, and explicitly delete it here with EV_DELETE
	if (filter == EVFILT_WRITE) KQueueSet(sock->fd, EV_DELETE, EVFILT_WRITE, &sock->kqEntry);

	if (sock->flags & kTCPSocketFlags_UseTLS)
		{
#ifndef NO_SECURITYFRAMEWORK
		if (!sock->setup) { sock->setup = mDNStrue; tlsSetupSock(sock, mDNSfalse); }
		
		if (sock->handshake == handshake_required) { if (spawnSSLHandshake(sock) == 0) return; }
		else if (sock->handshake == handshake_in_progress || sock->handshake == handshake_to_be_closed) return;
		else if (sock->handshake != handshake_completed)
			{
			if (!sock->err) sock->err = mStatus_UnknownErr;
			LogMsg("tcpKQSocketCallback called with unexpected SSLHandshake status: %d", sock->handshake);
			}
#else
		sock->err = mStatus_UnsupportedErr;
#endif /* NO_SECURITYFRAMEWORK */
		}
	
	doTcpSocketCallback(sock);
	}

mDNSexport int KQueueSet(int fd, u_short flags, short filter, const KQueueEntry *const entryRef)
	{
	struct kevent new_event;
	EV_SET(&new_event, fd, filter, flags, 0, 0, (void*)entryRef);
	return (kevent(KQueueFD, &new_event, 1, NULL, 0, NULL) < 0) ? errno : 0;
	}

mDNSexport void KQueueLock(mDNS *const m)
	{
	pthread_mutex_lock(&m->p->BigMutex);
	m->p->BigMutexStartTime = mDNSPlatformRawTime();
	}

mDNSexport void KQueueUnlock(mDNS *const m, const char const *task)
	{
	mDNSs32 end = mDNSPlatformRawTime();
	(void)task;
	if (end - m->p->BigMutexStartTime >= WatchDogReportingThreshold)
		LogInfo("WARNING: %s took %dms to complete", task, end - m->p->BigMutexStartTime);

	pthread_mutex_unlock(&m->p->BigMutex);

	char wake = 1;
	if (send(m->p->WakeKQueueLoopFD, &wake, sizeof(wake), 0) == -1)
		LogMsg("ERROR: KQueueWake: send failed with error code: %d (%s)", errno, strerror(errno));
	}

mDNSexport TCPSocket *mDNSPlatformTCPSocket(mDNS *const m, TCPSocketFlags flags, mDNSIPPort *port)
	{
	(void) m;

	TCPSocket *sock = mallocL("TCPSocket/mDNSPlatformTCPSocket", sizeof(TCPSocket));
	if (!sock) { LogMsg("mDNSPlatformTCPSocket: memory allocation failure"); return(mDNSNULL); }

	mDNSPlatformMemZero(sock, sizeof(TCPSocket));
	sock->callback          = mDNSNULL;
	sock->fd                = socket(AF_INET, SOCK_STREAM, 0);
	sock->kqEntry.KQcallback= tcpKQSocketCallback;
	sock->kqEntry.KQcontext = sock;
	sock->kqEntry.KQtask    = "mDNSPlatformTCPSocket";
	sock->flags             = flags;
	sock->context           = mDNSNULL;
	sock->setup             = mDNSfalse;
	sock->connected         = mDNSfalse;
	sock->handshake         = handshake_required;
	sock->m                 = m;
	sock->err               = mStatus_NoError;
	
	if (sock->fd == -1)
		{
		LogMsg("mDNSPlatformTCPSocket: socket error %d errno %d (%s)", sock->fd, errno, strerror(errno));
		freeL("TCPSocket/mDNSPlatformTCPSocket", sock);
		return(mDNSNULL);
		}

	// Bind it
	struct sockaddr_in addr;
	mDNSPlatformMemZero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = port->NotAnInteger;
	if (bind(sock->fd, (struct sockaddr*) &addr, sizeof(addr)) < 0)
		{ LogMsg("ERROR: bind %s", strerror(errno)); goto error; }

	// Receive interface identifiers
	const int on = 1;  // "on" for setsockopt
	if (setsockopt(sock->fd, IPPROTO_IP, IP_RECVIF, &on, sizeof(on)) < 0)
		{ LogMsg("setsockopt IP_RECVIF - %s", strerror(errno)); goto error; }

	mDNSPlatformMemZero(&addr, sizeof(addr));
	socklen_t len = sizeof(addr);
	if (getsockname(sock->fd, (struct sockaddr*) &addr, &len) < 0)
		{ LogMsg("getsockname - %s", strerror(errno)); goto error; }

	port->NotAnInteger = addr.sin_port;
	return sock;

error:
	close(sock->fd);
	freeL("TCPSocket/mDNSPlatformTCPSocket", sock);
	return(mDNSNULL);
	}

mDNSexport mStatus mDNSPlatformTCPConnect(TCPSocket *sock, const mDNSAddr *dst, mDNSOpaque16 dstport, mDNSInterfaceID InterfaceID,
                                          TCPConnectionCallback callback, void *context)
	{
	struct sockaddr_in saddr;
	mStatus err = mStatus_NoError;

	sock->callback          = callback;
	sock->context           = context;
	sock->setup             = mDNSfalse;
	sock->connected         = mDNSfalse;
	sock->handshake         = handshake_required;
	sock->err               = mStatus_NoError;

	(void) InterfaceID;	//!!!KRS use this if non-zero!!!

	if (dst->type != mDNSAddrType_IPv4)
		{
		LogMsg("ERROR: mDNSPlatformTCPConnect - attempt to connect to an IPv6 address: opperation not supported");
		return mStatus_UnknownErr;
		}

	mDNSPlatformMemZero(&saddr, sizeof(saddr));
	saddr.sin_family      = AF_INET;
	saddr.sin_port        = dstport.NotAnInteger;
	saddr.sin_len         = sizeof(saddr);
	saddr.sin_addr.s_addr = dst->ip.v4.NotAnInteger;

	sock->kqEntry.KQcallback = tcpKQSocketCallback;
	sock->kqEntry.KQcontext  = sock;
	sock->kqEntry.KQtask     = "Outgoing TCP";

	// Watch for connect complete (write is ready)
	// EV_ONESHOT doesn't seem to work, so we add the filter with EV_ADD, and explicitly delete it in tcpKQSocketCallback using EV_DELETE
	if (KQueueSet(sock->fd, EV_ADD /* | EV_ONESHOT */, EVFILT_WRITE, &sock->kqEntry))
		{
		LogMsg("ERROR: mDNSPlatformTCPConnect - KQueueSet failed");
		close(sock->fd);
		return errno;
		}

	// Watch for incoming data
	if (KQueueSet(sock->fd, EV_ADD, EVFILT_READ, &sock->kqEntry))
		{
		LogMsg("ERROR: mDNSPlatformTCPConnect - KQueueSet failed");
		close(sock->fd); // Closing the descriptor removes all filters from the kqueue
		return errno;
 		}

	if (fcntl(sock->fd, F_SETFL, fcntl(sock->fd, F_GETFL, 0) | O_NONBLOCK) < 0) // set non-blocking
		{
		LogMsg("ERROR: setsockopt O_NONBLOCK - %s", strerror(errno));
		return mStatus_UnknownErr;
		}

	// initiate connection wth peer
	if (connect(sock->fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
		{
		if (errno == EINPROGRESS) return mStatus_ConnPending;
		if (errno == EHOSTUNREACH || errno == EADDRNOTAVAIL || errno == ENETDOWN)
			LogInfo("ERROR: mDNSPlatformTCPConnect - connect failed: socket %d: Error %d (%s)", sock->fd, errno, strerror(errno));
		else
			LogMsg("ERROR: mDNSPlatformTCPConnect - connect failed: socket %d: Error %d (%s)", sock->fd, errno, strerror(errno));
		close(sock->fd);
		return mStatus_ConnFailed;
		}

	LogMsg("NOTE: mDNSPlatformTCPConnect completed synchronously");
	// kQueue should notify us, but this LogMsg is to help track down if it doesn't
	return err;
	}

// Why doesn't mDNSPlatformTCPAccept actually call accept() ?
mDNSexport TCPSocket *mDNSPlatformTCPAccept(TCPSocketFlags flags, int fd)
	{
	mStatus err = mStatus_NoError;

	TCPSocket *sock = mallocL("TCPSocket/mDNSPlatformTCPAccept", sizeof(TCPSocket));
	if (!sock) return(mDNSNULL);

	mDNSPlatformMemZero(sock, sizeof(*sock));
	sock->fd = fd;
	sock->flags = flags;

	if (flags & kTCPSocketFlags_UseTLS)
		{
#ifndef NO_SECURITYFRAMEWORK
		if (!ServerCerts) { LogMsg("ERROR: mDNSPlatformTCPAccept: unable to find TLS certificates"); err = mStatus_UnknownErr; goto exit; }

		err = tlsSetupSock(sock, mDNStrue);
		if (err) { LogMsg("ERROR: mDNSPlatformTCPAccept: tlsSetupSock failed with error code: %d", err); goto exit; }

		err = SSLSetCertificate(sock->tlsContext, ServerCerts);
		if (err) { LogMsg("ERROR: mDNSPlatformTCPAccept: SSLSetCertificate failed with error code: %d", err); goto exit; }
#else
		err = mStatus_UnsupportedErr;
#endif /* NO_SECURITYFRAMEWORK */
		}
#ifndef NO_SECURITYFRAMEWORK
exit:
#endif

	if (err) { freeL("TCPSocket/mDNSPlatformTCPAccept", sock); return(mDNSNULL); }
	return(sock);
	}

mDNSexport void mDNSPlatformTCPCloseConnection(TCPSocket *sock)
	{
	if (sock)
		{	
#ifndef NO_SECURITYFRAMEWORK
		if (sock->tlsContext)
			{
			if (sock->handshake == handshake_in_progress) // SSLHandshake thread using this sock (esp. tlsContext)
				{
				LogInfo("mDNSPlatformTCPCloseConnection: called while handshake in progress");
				sock->handshake = handshake_to_be_closed;
				}
			if (sock->handshake == handshake_to_be_closed)
				return;

			SSLClose(sock->tlsContext);
			SSLDisposeContext(sock->tlsContext);
			sock->tlsContext = NULL;
			}
#endif /* NO_SECURITYFRAMEWORK */
		if (sock->fd != -1)
			{
			shutdown(sock->fd, 2);
			close(sock->fd);
			sock->fd = -1;
			}

		freeL("TCPSocket/mDNSPlatformTCPCloseConnection", sock);
		}
	}

mDNSexport long mDNSPlatformReadTCP(TCPSocket *sock, void *buf, unsigned long buflen, mDNSBool *closed)
	{
	long nread = 0;
	*closed = mDNSfalse;

	if (sock->flags & kTCPSocketFlags_UseTLS)
		{
#ifndef NO_SECURITYFRAMEWORK
		if (sock->handshake == handshake_required) { LogMsg("mDNSPlatformReadTCP called while handshake required"); return 0; }
		else if (sock->handshake == handshake_in_progress) return 0;
		else if (sock->handshake != handshake_completed) LogMsg("mDNSPlatformReadTCP called with unexpected SSLHandshake status: %d", sock->handshake);

		//LogMsg("Starting SSLRead %d %X", sock->fd, fcntl(sock->fd, F_GETFL, 0));
		mStatus err = SSLRead(sock->tlsContext, buf, buflen, (size_t*)&nread);
		//LogMsg("SSLRead returned %d (%d) nread %d buflen %d", err, errSSLWouldBlock, nread, buflen);
		if (err == errSSLClosedGraceful) { nread = 0; *closed = mDNStrue; }
		else if (err && err != errSSLWouldBlock)
			{ LogMsg("ERROR: mDNSPlatformReadTCP - SSLRead: %d", err); nread = -1; *closed = mDNStrue; }
#else
		nread = -1;
		*closed = mDNStrue;
#endif /* NO_SECURITYFRAMEWORK */
		}
	else
		{
		static int CLOSEDcount = 0;
		static int EAGAINcount = 0;
		nread = recv(sock->fd, buf, buflen, 0);

		if (nread > 0) { CLOSEDcount = 0; EAGAINcount = 0; } // On success, clear our error counters
		else if (nread == 0)
			{
			*closed = mDNStrue;
			if ((++CLOSEDcount % 1000) == 0) { LogMsg("ERROR: mDNSPlatformReadTCP - recv %d got CLOSED %d times", sock->fd, CLOSEDcount); sleep(1); }
			}
		// else nread is negative -- see what kind of error we got
		else if (errno == ECONNRESET) { nread = 0; *closed = mDNStrue; }
		else if (errno != EAGAIN) { LogMsg("ERROR: mDNSPlatformReadTCP - recv: %d (%s)", errno, strerror(errno)); nread = -1; }
		else // errno is EAGAIN (EWOULDBLOCK) -- no data available
			{
			nread = 0;
			if ((++EAGAINcount % 1000) == 0) { LogMsg("ERROR: mDNSPlatformReadTCP - recv %d got EAGAIN %d times", sock->fd, EAGAINcount); sleep(1); }
			}
		}

	return nread;
	}

mDNSexport long mDNSPlatformWriteTCP(TCPSocket *sock, const char *msg, unsigned long len)
	{
	int nsent;

	if (sock->flags & kTCPSocketFlags_UseTLS)
		{
#ifndef NO_SECURITYFRAMEWORK
		size_t	processed;
		if (sock->handshake == handshake_required) { LogMsg("mDNSPlatformWriteTCP called while handshake required"); return 0; }
		if (sock->handshake == handshake_in_progress) return 0;
		else if (sock->handshake != handshake_completed) LogMsg("mDNSPlatformWriteTCP called with unexpected SSLHandshake status: %d", sock->handshake);
		
		mStatus	err = SSLWrite(sock->tlsContext, msg, len, &processed);

		if (!err) nsent = (int) processed;
		else if (err == errSSLWouldBlock) nsent = 0;
		else { LogMsg("ERROR: mDNSPlatformWriteTCP - SSLWrite returned %d", err); nsent = -1; }
#else
		nsent = -1;
#endif /* NO_SECURITYFRAMEWORK */
		}
	else
		{
		nsent = send(sock->fd, msg, len, 0);
		if (nsent < 0)
			{
			if (errno == EAGAIN) nsent = 0;
			else { LogMsg("ERROR: mDNSPlatformWriteTCP - send %s", strerror(errno)); nsent = -1; }
			}
		}

	return nsent;
	}

mDNSexport int mDNSPlatformTCPGetFD(TCPSocket *sock)
	{
	return sock->fd;
	}

// If mDNSIPPort port is non-zero, then it's a multicast socket on the specified interface
// If mDNSIPPort port is zero, then it's a randomly assigned port number, used for sending unicast queries
mDNSlocal mStatus SetupSocket(KQSocketSet *cp, const mDNSIPPort port, u_short sa_family, mDNSIPPort *const outport)
	{
	int         *s        = (sa_family == AF_INET) ? &cp->sktv4 : &cp->sktv6;
	KQueueEntry	*k        = (sa_family == AF_INET) ? &cp->kqsv4 : &cp->kqsv6;
	const int on = 1;
	const int twofivefive = 255;
	mStatus err = mStatus_NoError;
	char *errstr = mDNSNULL;
	
	cp->closeFlag = mDNSNULL;

	int skt = socket(sa_family, SOCK_DGRAM, IPPROTO_UDP);
	if (skt < 3) { if (errno != EAFNOSUPPORT) LogMsg("SetupSocket: socket error %d errno %d (%s)", skt, errno, strerror(errno)); return(skt); }

	// ... with a shared UDP port, if it's for multicast receiving
	if (mDNSSameIPPort(port, MulticastDNSPort) || mDNSSameIPPort(port, NATPMPAnnouncementPort)) err = setsockopt(skt, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
	if (err < 0) { errstr = "setsockopt - SO_REUSEPORT"; goto fail; }

	if (sa_family == AF_INET)
		{
		// We want to receive destination addresses
		err = setsockopt(skt, IPPROTO_IP, IP_RECVDSTADDR, &on, sizeof(on));
		if (err < 0) { errstr = "setsockopt - IP_RECVDSTADDR"; goto fail; }

		// We want to receive interface identifiers
		err = setsockopt(skt, IPPROTO_IP, IP_RECVIF, &on, sizeof(on));
		if (err < 0) { errstr = "setsockopt - IP_RECVIF"; goto fail; }

		// We want to receive packet TTL value so we can check it
		err = setsockopt(skt, IPPROTO_IP, IP_RECVTTL, &on, sizeof(on));
		// We ignore errors here -- we already know Jaguar doesn't support this, but we can get by without it

		// Send unicast packets with TTL 255
		err = setsockopt(skt, IPPROTO_IP, IP_TTL, &twofivefive, sizeof(twofivefive));
		if (err < 0) { errstr = "setsockopt - IP_TTL"; goto fail; }

		// And multicast packets with TTL 255 too
		err = setsockopt(skt, IPPROTO_IP, IP_MULTICAST_TTL, &twofivefive, sizeof(twofivefive));
		if (err < 0) { errstr = "setsockopt - IP_MULTICAST_TTL"; goto fail; }

		// And start listening for packets
		struct sockaddr_in listening_sockaddr;
		listening_sockaddr.sin_family      = AF_INET;
		listening_sockaddr.sin_port        = port.NotAnInteger;		// Pass in opaque ID without any byte swapping
		listening_sockaddr.sin_addr.s_addr = mDNSSameIPPort(port, NATPMPAnnouncementPort) ? AllSystemsMcast.NotAnInteger : 0;
		err = bind(skt, (struct sockaddr *) &listening_sockaddr, sizeof(listening_sockaddr));
		if (err) { errstr = "bind"; goto fail; }
		if (outport) outport->NotAnInteger = listening_sockaddr.sin_port;
		}
	else if (sa_family == AF_INET6)
		{
		// NAT-PMP Announcements make no sense on IPv6, so bail early w/o error
		if (mDNSSameIPPort(port, NATPMPAnnouncementPort)) { if (outport) *outport = zeroIPPort; return mStatus_NoError; }
		
		// We want to receive destination addresses and receive interface identifiers
		err = setsockopt(skt, IPPROTO_IPV6, IPV6_PKTINFO, &on, sizeof(on));
		if (err < 0) { errstr = "setsockopt - IPV6_PKTINFO"; goto fail; }

		// We want to receive packet hop count value so we can check it
		err = setsockopt(skt, IPPROTO_IPV6, IPV6_HOPLIMIT, &on, sizeof(on));
		if (err < 0) { errstr = "setsockopt - IPV6_HOPLIMIT"; goto fail; }

		// We want to receive only IPv6 packets. Without this option we get IPv4 packets too,
		// with mapped addresses of the form 0:0:0:0:0:FFFF:xxxx:xxxx, where xxxx:xxxx is the IPv4 address
		err = setsockopt(skt, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
		if (err < 0) { errstr = "setsockopt - IPV6_V6ONLY"; goto fail; }

		// Send unicast packets with TTL 255
		err = setsockopt(skt, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &twofivefive, sizeof(twofivefive));
		if (err < 0) { errstr = "setsockopt - IPV6_UNICAST_HOPS"; goto fail; }

		// And multicast packets with TTL 255 too
		err = setsockopt(skt, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &twofivefive, sizeof(twofivefive));
		if (err < 0) { errstr = "setsockopt - IPV6_MULTICAST_HOPS"; goto fail; }

		// Want to receive our own packets
		err = setsockopt(skt, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &on, sizeof(on));
		if (err < 0) { errstr = "setsockopt - IPV6_MULTICAST_LOOP"; goto fail; }

		// And start listening for packets
		struct sockaddr_in6 listening_sockaddr6;
		mDNSPlatformMemZero(&listening_sockaddr6, sizeof(listening_sockaddr6));
		listening_sockaddr6.sin6_len         = sizeof(listening_sockaddr6);
		listening_sockaddr6.sin6_family      = AF_INET6;
		listening_sockaddr6.sin6_port        = port.NotAnInteger;		// Pass in opaque ID without any byte swapping
		listening_sockaddr6.sin6_flowinfo    = 0;
		listening_sockaddr6.sin6_addr        = in6addr_any; // Want to receive multicasts AND unicasts on this socket
		listening_sockaddr6.sin6_scope_id    = 0;
		err = bind(skt, (struct sockaddr *) &listening_sockaddr6, sizeof(listening_sockaddr6));
		if (err) { errstr = "bind"; goto fail; }
		if (outport) outport->NotAnInteger = listening_sockaddr6.sin6_port;
		}

	fcntl(skt, F_SETFL, fcntl(skt, F_GETFL, 0) | O_NONBLOCK); // set non-blocking
	fcntl(skt, F_SETFD, 1); // set close-on-exec
	*s = skt;
	k->KQcallback = myKQSocketCallBack;
	k->KQcontext  = cp;
	k->KQtask     = "UDP packet reception";
	KQueueSet(*s, EV_ADD, EVFILT_READ, k);

	return(err);

	fail:
	// For "bind" failures, only write log messages for our shared mDNS port, or for binding to zero
	if (strcmp(errstr, "bind") || mDNSSameIPPort(port, MulticastDNSPort) || mDNSIPPortIsZero(port))
		LogMsg("%s skt %d port %d error %d errno %d (%s)", errstr, skt, mDNSVal16(port), err, errno, strerror(errno));

	// If we got a "bind" failure of EADDRINUSE, inform the caller as it might need to try another random port
	if (!strcmp(errstr, "bind") && errno == EADDRINUSE)
		{
		err = EADDRINUSE;
		if (mDNSSameIPPort(port, MulticastDNSPort))
			NotifyOfElusiveBug("Setsockopt SO_REUSEPORT failed",
				"Congratulations, you've reproduced an elusive bug.\r"
				"Please contact the current assignee of <rdar://problem/3814904>.\r"
				"Alternatively, you can send email to radar-3387020@group.apple.com. (Note number is different.)\r"
				"If possible, please leave your machine undisturbed so that someone can come to investigate the problem.");
		}

	close(skt);
	return(err);
	}

mDNSexport UDPSocket *mDNSPlatformUDPSocket(mDNS *const m, const mDNSIPPort requestedport)
	{
	mStatus err;
	mDNSIPPort port = requestedport;
	mDNSBool randomizePort = mDNSIPPortIsZero(requestedport);
	int i = 10000; // Try at most 10000 times to get a unique random port
	UDPSocket *p = mallocL("UDPSocket", sizeof(UDPSocket));
	if (!p) { LogMsg("mDNSPlatformUDPSocket: memory exhausted"); return(mDNSNULL); }
	mDNSPlatformMemZero(p, sizeof(UDPSocket));
	p->ss.port  = zeroIPPort;
	p->ss.m     = m;
	p->ss.sktv4 = -1;
	p->ss.sktv6 = -1;

	do
		{
		// The kernel doesn't do cryptographically strong random port allocation, so we do it ourselves here
		if (randomizePort) port = mDNSOpaque16fromIntVal(0xC000 + mDNSRandom(0x3FFF));
		err = SetupSocket(&p->ss, port, AF_INET, &p->ss.port);
		if (!err)
			{
			err = SetupSocket(&p->ss, port, AF_INET6, &p->ss.port);
			if (err) { close(p->ss.sktv4); p->ss.sktv4 = -1; }
			}
		i--;
		} while (err == EADDRINUSE && randomizePort && i);

	if (err)
		{
		// In customer builds we don't want to log failures with port 5351, because this is a known issue
		// of failing to bind to this port when Internet Sharing has already bound to it
		// We also don't want to log about port 5350, due to a known bug when some other
		// process is bound to it.
		if (mDNSSameIPPort(requestedport, NATPMPPort) || mDNSSameIPPort(requestedport, NATPMPAnnouncementPort))
			LogInfo("mDNSPlatformUDPSocket: SetupSocket %d failed error %d errno %d (%s)", mDNSVal16(requestedport), err, errno, strerror(errno));
		else LogMsg("mDNSPlatformUDPSocket: SetupSocket %d failed error %d errno %d (%s)", mDNSVal16(requestedport), err, errno, strerror(errno));
		freeL("UDPSocket", p);
		return(mDNSNULL);
		}
	return(p);
	}

mDNSlocal void CloseSocketSet(KQSocketSet *ss)
	{
	if (ss->sktv4 != -1)
		{
		close(ss->sktv4);
		ss->sktv4 = -1;
		}
	if (ss->sktv6 != -1)
		{
		close(ss->sktv6);
		ss->sktv6 = -1;
		}
	if (ss->closeFlag) *ss->closeFlag = 1;
	}

mDNSexport void mDNSPlatformUDPClose(UDPSocket *sock)
	{
	CloseSocketSet(&sock->ss);
	freeL("UDPSocket", sock);
	}

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - BPF Raw packet sending/receiving
#endif

#if APPLE_OSX_mDNSResponder

mDNSexport void mDNSPlatformSendRawPacket(const void *const msg, const mDNSu8 *const end, mDNSInterfaceID InterfaceID)
	{
	if (!InterfaceID) { LogMsg("mDNSPlatformSendRawPacket: No InterfaceID specified"); return; }
	NetworkInterfaceInfoOSX *info;

	extern mDNS mDNSStorage;
	info = IfindexToInterfaceInfoOSX(&mDNSStorage, InterfaceID);
	if (info == NULL)
		{
		LogMsg("mDNSPlatformSendUDP: Invalid interface index %p", InterfaceID);
		return;
		}
	if (info->BPF_fd < 0)
		LogMsg("mDNSPlatformSendRawPacket: %s BPF_fd %d not ready", info->ifinfo.ifname, info->BPF_fd);
	else
		{
		//LogMsg("mDNSPlatformSendRawPacket %d bytes on %s", end - (mDNSu8 *)msg, info->ifinfo.ifname);
		if (write(info->BPF_fd, msg, end - (mDNSu8 *)msg) < 0)
			LogMsg("mDNSPlatformSendRawPacket: BPF write(%d) failed %d (%s)", info->BPF_fd, errno, strerror(errno));
		}
	}

mDNSexport void mDNSPlatformSetLocalARP(const mDNSv4Addr *const tpa, const mDNSEthAddr *const tha, mDNSInterfaceID InterfaceID)
	{
	if (!InterfaceID) { LogMsg("mDNSPlatformSetLocalARP: No InterfaceID specified"); return; }
	NetworkInterfaceInfoOSX *info;
	extern mDNS mDNSStorage;
	info = IfindexToInterfaceInfoOSX(&mDNSStorage, InterfaceID);
	if (info == NULL)
		{
		LogMsg("mDNSPlatformSendUDP: Invalid interface index %p", InterfaceID);
		return;
		}
	// Manually inject an entry into our local ARP cache.
	// (We can't do this by sending an ARP broadcast, because the kernel only pays attention to incoming ARP packets, not outgoing.)
	mDNSBool makearp = mDNSv4AddressIsLinkLocal(tpa);
	if (!makearp)
		{
		NetworkInterfaceInfoOSX *i;
		for (i = info->m->p->InterfaceList; i; i = i->next)
			if (i->Exists && i->ifinfo.InterfaceID == InterfaceID && i->ifinfo.ip.type == mDNSAddrType_IPv4)
				if (((i->ifinfo.ip.ip.v4.NotAnInteger ^ tpa->NotAnInteger) & i->ifinfo.mask.ip.v4.NotAnInteger) == 0)
					makearp = mDNStrue;
		}
	if (!makearp)
		LogInfo("Don't need ARP entry for %s %.4a %.6a",            info->ifinfo.ifname, tpa, tha);
	else
		{
		int result = mDNSSetARP(info->scope_id, tpa->b, tha->b);
		if (result) LogMsg("Set local ARP entry for %s %.4a %.6a failed: %d", info->ifinfo.ifname, tpa, tha, result);
		else debugf       ("Set local ARP entry for %s %.4a %.6a",            info->ifinfo.ifname, tpa, tha);
		}
	}

mDNSlocal void CloseBPF(NetworkInterfaceInfoOSX *const i)
	{
	LogSPS("%s closing BPF fd %d", i->ifinfo.ifname, i->BPF_fd);

	// Note: MUST NOT close() the underlying native BSD sockets.
	// CFSocketInvalidate() will do that for us, in its own good time, which may not necessarily be immediately, because
	// it first has to unhook the sockets from its select() call on its other thread, before it can safely close them.
	CFRunLoopRemoveSource(i->m->p->CFRunLoop, i->BPF_rls, kCFRunLoopDefaultMode);
	CFRelease(i->BPF_rls);
	CFSocketInvalidate(i->BPF_cfs);
	CFRelease(i->BPF_cfs);
	i->BPF_fd = -1;
	}

mDNSlocal void bpf_callback(const CFSocketRef cfs, const CFSocketCallBackType CallBackType, const CFDataRef address, const void *const data, void *const context)
	{
	(void)cfs;
	(void)CallBackType;
	(void)address;
	(void)data;

	NetworkInterfaceInfoOSX *const info = (NetworkInterfaceInfoOSX *)context;
	KQueueLock(info->m);

	// Now we've got the lock, make sure the kqueue thread didn't close the fd out from under us (will not be a problem once the OS X
	// kernel has a mechanism for dispatching all events to a single thread, but for now we have to guard against this race condition).
	if (info->BPF_fd < 0) goto exit;

	ssize_t n = read(info->BPF_fd, &info->m->imsg, info->BPF_len);
	const mDNSu8 *ptr = (const mDNSu8 *)&info->m->imsg;
	const mDNSu8 *end = (const mDNSu8 *)&info->m->imsg + n;
	debugf("%3d: bpf_callback got %d bytes on %s", info->BPF_fd, n, info->ifinfo.ifname);

	if (n<0)
		{
		LogMsg("Closing %s BPF fd %d due to error %d (%s)", info->ifinfo.ifname, info->BPF_fd, errno, strerror(errno));
		CloseBPF(info);
		goto exit;
		}

	while (ptr < end)
		{
		const struct bpf_hdr *bh = (const struct bpf_hdr *)ptr;
		debugf("%3d: bpf_callback bh_caplen %4d bh_datalen %4d remaining %4d", info->BPF_fd, bh->bh_caplen, bh->bh_datalen, end - (ptr + BPF_WORDALIGN(bh->bh_hdrlen + bh->bh_caplen)));
		mDNSCoreReceiveRawPacket(info->m, ptr + bh->bh_hdrlen, ptr + bh->bh_hdrlen + bh->bh_caplen, info->ifinfo.InterfaceID);
		ptr += BPF_WORDALIGN(bh->bh_hdrlen + bh->bh_caplen);
		}
exit:
	KQueueUnlock(info->m, "bpf_callback");
	}

#define BPF_SetOffset(from, cond, to) (from)->cond = (to) - 1 - (from)

mDNSlocal int CountProxyTargets(mDNS *const m, NetworkInterfaceInfoOSX *x, int *p4, int *p6)
	{
	int numv4 = 0, numv6 = 0;
	AuthRecord *rr;

	for (rr = m->ResourceRecords; rr; rr=rr->next)
		if (rr->resrec.InterfaceID == x->ifinfo.InterfaceID && rr->AddressProxy.type == mDNSAddrType_IPv4)
			{
			if (p4) LogSPS("CountProxyTargets: fd %d %-7s IP%2d %.4a", x->BPF_fd, x->ifinfo.ifname, numv4, &rr->AddressProxy.ip.v4);
			numv4++;
			}

	for (rr = m->ResourceRecords; rr; rr=rr->next)
		if (rr->resrec.InterfaceID == x->ifinfo.InterfaceID && rr->AddressProxy.type == mDNSAddrType_IPv6)
			{
			if (p6) LogSPS("CountProxyTargets: fd %d %-7s IP%2d %.16a", x->BPF_fd, x->ifinfo.ifname, numv6, &rr->AddressProxy.ip.v6);
			numv6++;
			}

	if (p4) *p4 = numv4;
	if (p6) *p6 = numv6;
	return(numv4 + numv6);
	}

mDNSexport void mDNSPlatformUpdateProxyList(mDNS *const m, const mDNSInterfaceID InterfaceID)
	{
	NetworkInterfaceInfoOSX *x;

	//NOTE: We can't use IfIndexToInterfaceInfoOSX because that looks for Registered also.
	for (x = m->p->InterfaceList; x; x = x->next) if (x->ifinfo.InterfaceID == InterfaceID) break;

	if (!x) { LogMsg("mDNSPlatformUpdateProxyList: ERROR InterfaceID %p not found", InterfaceID); return; }

	#define MAX_BPF_ADDRS 250
	int numv4 = 0, numv6 = 0;

	if (CountProxyTargets(m, x, &numv4, &numv6) > MAX_BPF_ADDRS)
		{
		LogMsg("mDNSPlatformUpdateProxyList: ERROR Too many address proxy records v4 %d v6 %d", numv4, numv6);
		if (numv4 > MAX_BPF_ADDRS) numv4 = MAX_BPF_ADDRS;
		numv6 = MAX_BPF_ADDRS - numv4;
		}

	LogSPS("mDNSPlatformUpdateProxyList: fd %d %-7s MAC  %.6a %d v4 %d v6", x->BPF_fd, x->ifinfo.ifname, &x->ifinfo.MAC, numv4, numv6);

	// Caution: This is a static structure, so we need to be careful that any modifications we make to it
	// are done in such a way that they work correctly when mDNSPlatformUpdateProxyList is called multiple times
	static struct bpf_insn filter[17 + MAX_BPF_ADDRS] =
		{
		BPF_STMT(BPF_LD  + BPF_H   + BPF_ABS, 12),				// 0 Read Ethertype (bytes 12,13)

		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 0x0806, 0, 1),		// 1 If Ethertype == ARP goto next, else 3
		BPF_STMT(BPF_RET + BPF_K,             42),				// 2 Return 42-byte ARP

		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 0x0800, 4, 0),		// 3 If Ethertype == IPv4 goto 8 (IPv4 address list check) else next

		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 0x86DD, 0, 9),		// 4 If Ethertype == IPv6 goto next, else exit
		BPF_STMT(BPF_LD  + BPF_H   + BPF_ABS, 20),				// 5 Read Protocol and Hop Limit (bytes 20,21)
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 0x3AFF, 0, 9),		// 6 If (Prot,TTL) == (3A,FF) goto next, else IPv6 address list check
		BPF_STMT(BPF_RET + BPF_K,             78),				// 7 Return 78-byte ND

		// Is IPv4 packet; check if it's addressed to any IPv4 address we're proxying for
		BPF_STMT(BPF_LD  + BPF_W   + BPF_ABS, 30),				// 8 Read IPv4 Dst (bytes 30,31,32,33)
		};

	struct bpf_insn *pc   = &filter[9];
	struct bpf_insn *chk6 = pc   + numv4 + 1;	// numv4 address checks, plus a "return 0"
	struct bpf_insn *fail = chk6 + 1 + numv6;	// Get v6 Dst LSW, plus numv6 address checks
	struct bpf_insn *ret4 = fail + 1;
	struct bpf_insn *ret6 = ret4 + 4;

	static const struct bpf_insn rf  = BPF_STMT(BPF_RET + BPF_K, 0);				// No match: Return nothing

	static const struct bpf_insn g6  = BPF_STMT(BPF_LD  + BPF_W   + BPF_ABS, 50);	// Read IPv6 Dst LSW (bytes 50,51,52,53)

	static const struct bpf_insn r4a = BPF_STMT(BPF_LDX + BPF_B   + BPF_MSH, 14);	// Get IP Header length (normally 20)
	static const struct bpf_insn r4b = BPF_STMT(BPF_LD  + BPF_IMM,           54);	// A = 54 (14-byte Ethernet plus 20-byte TCP + 20 bytes spare)
	static const struct bpf_insn r4c = BPF_STMT(BPF_ALU + BPF_ADD + BPF_X,    0);	// A += IP Header length
	static const struct bpf_insn r4d = BPF_STMT(BPF_RET + BPF_A, 0);				// Success: Return Ethernet + IP + TCP + 20 bytes spare (normally 74)

	static const struct bpf_insn r6a = BPF_STMT(BPF_RET + BPF_K, 94);				// Success: Return Eth + IPv6 + TCP + 20 bytes spare

	BPF_SetOffset(&filter[4], jf, fail);	// If Ethertype not ARP, IPv4, or IPv6, fail
	BPF_SetOffset(&filter[6], jf, chk6);	// If IPv6 but not ICMPv6, go to IPv6 address list check

	// BPF Byte-Order Note
	// The BPF API designers apparently thought that programmers would not be smart enough to use htons
	// and htonl correctly to convert numeric values to network byte order on little-endian machines,
	// so instead they chose to make the API implicitly byte-swap *ALL* values, even literal byte strings
	// that shouldn't be byte-swapped, like ASCII text, Ethernet addresses, IP addresses, etc.
	// As a result, if we put Ethernet addresses and IP addresses in the right byte order, the BPF API
	// will byte-swap and make them backwards, and then our filter won't work. So, we have to arrange
	// that on little-endian machines we deliberately put addresses in memory with the bytes backwards,
	// so that when the BPF API goes through and swaps them all, they end up back as they should be.
	// In summary, if we byte-swap all the non-numeric fields that shouldn't be swapped, and we *don't*
	// swap any of the numeric values that *should* be byte-swapped, then the filter will work correctly.

	// IPSEC capture size notes:
	//  8 bytes UDP header
	//  4 bytes Non-ESP Marker
	// 28 bytes IKE Header
	// --
	// 40 Total. Capturing TCP Header + 20 gets us enough bytes to receive the IKE Header in a UDP-encapsulated IKE packet.

	AuthRecord *rr;
	for (rr = m->ResourceRecords; rr; rr=rr->next)
		if (rr->resrec.InterfaceID == InterfaceID && rr->AddressProxy.type == mDNSAddrType_IPv4)
			{
			mDNSv4Addr a = rr->AddressProxy.ip.v4;
			pc->code = BPF_JMP + BPF_JEQ + BPF_K;
			BPF_SetOffset(pc, jt, ret4);
			pc->jf   = 0;
			pc->k    = (bpf_u_int32)a.b[0] << 24 | (bpf_u_int32)a.b[1] << 16 | (bpf_u_int32)a.b[2] << 8 | (bpf_u_int32)a.b[3];
			pc++;
			}
	*pc++ = rf;

	if (pc != chk6) LogMsg("mDNSPlatformUpdateProxyList: pc %p != chk6 %p", pc, chk6);
	*pc++ = g6;	// chk6 points here

	for (rr = m->ResourceRecords; rr; rr=rr->next)
		if (rr->resrec.InterfaceID == InterfaceID && rr->AddressProxy.type == mDNSAddrType_IPv6)
			{
			mDNSv6Addr a = rr->AddressProxy.ip.v6;
			pc->code = BPF_JMP + BPF_JEQ + BPF_K;
			BPF_SetOffset(pc, jt, ret6);
			pc->jf   = 0;
			pc->k    = (bpf_u_int32)a.b[12] << 24 | (bpf_u_int32)a.b[13] << 16 | (bpf_u_int32)a.b[14] << 8 | (bpf_u_int32)a.b[15];
			pc++;
			}

	if (pc != fail) LogMsg("mDNSPlatformUpdateProxyList: pc %p != fail %p", pc, fail);
	*pc++ = rf;	// fail points here

	if (pc != ret4) LogMsg("mDNSPlatformUpdateProxyList: pc %p != ret4 %p", pc, ret4);
	*pc++ = r4a;	// ret4 points here
	*pc++ = r4b;
	*pc++ = r4c;
	*pc++ = r4d;

	if (pc != ret6) LogMsg("mDNSPlatformUpdateProxyList: pc %p != ret6 %p", pc, ret6);
	*pc++ = r6a;	// ret6 points here

	struct bpf_program prog = { pc - filter, filter };

#if 0
	// For debugging BPF filter program
	unsigned int q;
	for (q=0; q<prog.bf_len; q++)
		LogSPS("mDNSPlatformUpdateProxyList: %2d { 0x%02x, %d, %d, 0x%08x },", q, prog.bf_insns[q].code, prog.bf_insns[q].jt, prog.bf_insns[q].jf, prog.bf_insns[q].k);
#endif

	if (!numv4 && !numv6)
		{
		LogSPS("mDNSPlatformUpdateProxyList: No need for filter");
		if (m->timenow == 0) LogMsg("mDNSPlatformUpdateProxyList: m->timenow == 0");
		// Schedule check to see if we can close this BPF_fd now
		if (!m->p->NetworkChanged) m->p->NetworkChanged = NonZeroTime(m->timenow + mDNSPlatformOneSecond * 2);
		// prog.bf_len = 0; This seems to panic the kernel
		}

	if (ioctl(x->BPF_fd, BIOCSETF, &prog) < 0) LogMsg("mDNSPlatformUpdateProxyList: BIOCSETF(%d) failed %d (%s)", prog.bf_len, errno, strerror(errno));
	else LogSPS("mDNSPlatformUpdateProxyList: BIOCSETF(%d) successful", prog.bf_len);
	}

mDNSexport void mDNSPlatformReceiveBPF_fd(mDNS *const m, int fd)
	{
	mDNS_Lock(m);
	
	NetworkInterfaceInfoOSX *i;
	for (i = m->p->InterfaceList; i; i = i->next) if (i->BPF_fd == -2) break;
	if (!i) { LogSPS("mDNSPlatformReceiveBPF_fd: No Interfaces awaiting BPF fd %d; closing", fd); close(fd); }
	else
		{
		LogSPS("%s using   BPF fd %d", i->ifinfo.ifname, fd);
	
		struct bpf_version v;
		if (ioctl(fd, BIOCVERSION, &v) < 0)
			LogMsg("mDNSPlatformReceiveBPF_fd: %d %s BIOCVERSION failed %d (%s)", fd, i->ifinfo.ifname, errno, strerror(errno));
		else if (BPF_MAJOR_VERSION != v.bv_major || BPF_MINOR_VERSION != v.bv_minor)
			LogMsg("mDNSPlatformReceiveBPF_fd: %d %s BIOCVERSION header %d.%d kernel %d.%d",
				fd, i->ifinfo.ifname, BPF_MAJOR_VERSION, BPF_MINOR_VERSION, v.bv_major, v.bv_minor);
	
		if (ioctl(fd, BIOCGBLEN, &i->BPF_len) < 0)
			LogMsg("mDNSPlatformReceiveBPF_fd: %d %s BIOCGBLEN failed %d (%s)", fd, i->ifinfo.ifname, errno, strerror(errno));
	
		if (i->BPF_len > sizeof(m->imsg))
			{
			i->BPF_len = sizeof(m->imsg);
			if (ioctl(fd, BIOCSBLEN, &i->BPF_len) < 0)
				LogMsg("mDNSPlatformReceiveBPF_fd: %d %s BIOCSBLEN failed %d (%s)", fd, i->ifinfo.ifname, errno, strerror(errno));
			else LogSPS("mDNSPlatformReceiveBPF_fd: %d %s BIOCSBLEN %d", i->BPF_len);
			}
	
		static const u_int opt_immediate = 1;
		if (ioctl(fd, BIOCIMMEDIATE, &opt_immediate) < 0)
			LogMsg("mDNSPlatformReceiveBPF_fd: %d %s BIOCIMMEDIATE failed %d (%s)", fd, i->ifinfo.ifname, errno, strerror(errno));
	
		struct ifreq ifr;
		mDNSPlatformMemZero(&ifr, sizeof(ifr));
		strlcpy(ifr.ifr_name, i->ifinfo.ifname, sizeof(ifr.ifr_name));
		if (ioctl(fd, BIOCSETIF, &ifr) < 0)
			{ LogMsg("mDNSPlatformReceiveBPF_fd: %d %s BIOCSETIF failed %d (%s)", fd, i->ifinfo.ifname, errno, strerror(errno)); i->BPF_fd = -3; }
		else
			{
			CFSocketContext myCFSocketContext = { 0, i, NULL, NULL, NULL };
			i->BPF_fd  = fd;
			i->BPF_cfs = CFSocketCreateWithNative(kCFAllocatorDefault, fd, kCFSocketReadCallBack, bpf_callback, &myCFSocketContext);
			i->BPF_rls = CFSocketCreateRunLoopSource(kCFAllocatorDefault, i->BPF_cfs, 0);
			CFRunLoopAddSource(i->m->p->CFRunLoop, i->BPF_rls, kCFRunLoopDefaultMode);
			mDNSPlatformUpdateProxyList(m, i->ifinfo.InterfaceID);
			}
		}

	mDNS_Unlock(m);
	}

#endif // APPLE_OSX_mDNSResponder

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Key Management
#endif

#ifndef NO_SECURITYFRAMEWORK
mDNSlocal CFArrayRef GetCertChain(SecIdentityRef identity)
	{
	CFMutableArrayRef certChain = NULL;
	if (!identity) { LogMsg("getCertChain: identity is NULL"); return(NULL); }
	SecCertificateRef cert;
	OSStatus err = SecIdentityCopyCertificate(identity, &cert);
	if (err || !cert) LogMsg("getCertChain: SecIdentityCopyCertificate() returned %d", (int) err);
	else
		{
		SecPolicySearchRef searchRef;
		err = SecPolicySearchCreate(CSSM_CERT_X_509v3, &CSSMOID_APPLE_X509_BASIC, NULL, &searchRef);
		if (err || !searchRef) LogMsg("getCertChain: SecPolicySearchCreate() returned %d", (int) err);
		else
			{
			SecPolicyRef policy;
			err = SecPolicySearchCopyNext(searchRef, &policy);
			if (err || !policy) LogMsg("getCertChain: SecPolicySearchCopyNext() returned %d", (int) err);
			else
				{
				CFArrayRef wrappedCert = CFArrayCreate(NULL, (const void**) &cert, 1, &kCFTypeArrayCallBacks);
				if (!wrappedCert) LogMsg("getCertChain: wrappedCert is NULL");
				else
					{
					SecTrustRef trust;
					err = SecTrustCreateWithCertificates(wrappedCert, policy, &trust);
					if (err || !trust) LogMsg("getCertChain: SecTrustCreateWithCertificates() returned %d", (int) err);
					else
						{
						err = SecTrustEvaluate(trust, NULL);
						if (err) LogMsg("getCertChain: SecTrustEvaluate() returned %d", (int) err);
						else
							{
							CFArrayRef rawCertChain;
							CSSM_TP_APPLE_EVIDENCE_INFO *statusChain = NULL;
							err = SecTrustGetResult(trust, NULL, &rawCertChain, &statusChain);
							if (err || !rawCertChain || !statusChain) LogMsg("getCertChain: SecTrustGetResult() returned %d", (int) err);
							else
								{
								certChain = CFArrayCreateMutableCopy(NULL, 0, rawCertChain);
								if (!certChain) LogMsg("getCertChain: certChain is NULL");
								else
									{
									// Replace the SecCertificateRef at certChain[0] with a SecIdentityRef per documentation for SSLSetCertificate:
									// <http://devworld.apple.com/documentation/Security/Reference/secureTransportRef/index.html>
									CFArraySetValueAtIndex(certChain, 0, identity);
									// Remove root from cert chain, but keep any and all intermediate certificates that have been signed by the root certificate
									if (CFArrayGetCount(certChain) > 1) CFArrayRemoveValueAtIndex(certChain, CFArrayGetCount(certChain) - 1);
									}
								CFRelease(rawCertChain);
								// Do not free statusChain:
								// <http://developer.apple.com/documentation/Security/Reference/certifkeytrustservices/Reference/reference.html> says:
								// certChain: Call the CFRelease function to release this object when you are finished with it.
								// statusChain: Do not attempt to free this pointer; it remains valid until the trust management object is released...
								}
							}
						CFRelease(trust);
						}
					CFRelease(wrappedCert);
					}
				CFRelease(policy);
				}
			CFRelease(searchRef);
			}
		CFRelease(cert);
		}
	return certChain;
	}
#endif /* NO_SECURITYFRAMEWORK */

mDNSexport mStatus mDNSPlatformTLSSetupCerts(void)
	{
#ifdef NO_SECURITYFRAMEWORK
	return mStatus_UnsupportedErr;
#else
	SecIdentityRef			identity = nil;
	SecIdentitySearchRef	srchRef = nil;
	OSStatus				err;

	// search for "any" identity matching specified key use
	// In this app, we expect there to be exactly one
	err = SecIdentitySearchCreate(NULL, CSSM_KEYUSE_DECRYPT, &srchRef);
	if (err) { LogMsg("ERROR: mDNSPlatformTLSSetupCerts: SecIdentitySearchCreate returned %d", (int) err); return err; }

	err = SecIdentitySearchCopyNext(srchRef, &identity);
	if (err) { LogMsg("ERROR: mDNSPlatformTLSSetupCerts: SecIdentitySearchCopyNext returned %d", (int) err); return err; }

	if (CFGetTypeID(identity) != SecIdentityGetTypeID())
		{ LogMsg("ERROR: mDNSPlatformTLSSetupCerts: SecIdentitySearchCopyNext CFTypeID failure"); return mStatus_UnknownErr; }

	// Found one. Call getCertChain to create the correct certificate chain.
	ServerCerts = GetCertChain(identity);
	if (ServerCerts == nil) { LogMsg("ERROR: mDNSPlatformTLSSetupCerts: getCertChain error"); return mStatus_UnknownErr; }

	return mStatus_NoError;
#endif /* NO_SECURITYFRAMEWORK */
	}

mDNSexport  void  mDNSPlatformTLSTearDownCerts(void)
	{
#ifndef NO_SECURITYFRAMEWORK
	if (ServerCerts) { CFRelease(ServerCerts); ServerCerts = NULL; }
#endif /* NO_SECURITYFRAMEWORK */
	}

// This gets the text of the field currently labelled "Computer Name" in the Sharing Prefs Control Panel
mDNSlocal void GetUserSpecifiedFriendlyComputerName(domainlabel *const namelabel)
	{
	CFStringEncoding encoding = kCFStringEncodingUTF8;
	CFStringRef cfs = SCDynamicStoreCopyComputerName(NULL, &encoding);
	if (cfs)
		{
		CFStringGetPascalString(cfs, namelabel->c, sizeof(*namelabel), kCFStringEncodingUTF8);
		CFRelease(cfs);
		}
	}

// This gets the text of the field currently labelled "Local Hostname" in the Sharing Prefs Control Panel
mDNSlocal void GetUserSpecifiedLocalHostName(domainlabel *const namelabel)
	{
	CFStringRef cfs = SCDynamicStoreCopyLocalHostName(NULL);
	if (cfs)
		{
		CFStringGetPascalString(cfs, namelabel->c, sizeof(*namelabel), kCFStringEncodingUTF8);
		CFRelease(cfs);
		}
	}

mDNSexport mDNSBool DictionaryIsEnabled(CFDictionaryRef dict)
	{
	mDNSs32 val;
	CFNumberRef state = CFDictionaryGetValue(dict, CFSTR("Enabled"));
	if (!state) return mDNSfalse;
	if (!CFNumberGetValue(state, kCFNumberSInt32Type, &val))
		{ LogMsg("ERROR: DictionaryIsEnabled - CFNumberGetValue"); return mDNSfalse; }
	return val ? mDNStrue : mDNSfalse;
	}

mDNSlocal mStatus SetupAddr(mDNSAddr *ip, const struct sockaddr *const sa)
	{
	if (!sa) { LogMsg("SetupAddr ERROR: NULL sockaddr"); return(mStatus_Invalid); }

	if (sa->sa_family == AF_INET)
		{
		struct sockaddr_in *ifa_addr = (struct sockaddr_in *)sa;
		ip->type = mDNSAddrType_IPv4;
		ip->ip.v4.NotAnInteger = ifa_addr->sin_addr.s_addr;
		return(mStatus_NoError);
		}

	if (sa->sa_family == AF_INET6)
		{
		struct sockaddr_in6 *ifa_addr = (struct sockaddr_in6 *)sa;
		// Inside the BSD kernel they use a hack where they stuff the sin6->sin6_scope_id
		// value into the second word of the IPv6 link-local address, so they can just
		// pass around IPv6 address structures instead of full sockaddr_in6 structures.
		// Those hacked IPv6 addresses aren't supposed to escape the kernel in that form, but they do.
		// To work around this we always whack the second word of any IPv6 link-local address back to zero.
		if (IN6_IS_ADDR_LINKLOCAL(&ifa_addr->sin6_addr)) ifa_addr->sin6_addr.__u6_addr.__u6_addr16[1] = 0;
		ip->type = mDNSAddrType_IPv6;
		ip->ip.v6 = *(mDNSv6Addr*)&ifa_addr->sin6_addr;
		return(mStatus_NoError);
		}

	LogMsg("SetupAddr invalid sa_family %d", sa->sa_family);
	return(mStatus_Invalid);
	}

mDNSlocal mDNSEthAddr GetBSSID(char *ifa_name)
	{
	mDNSEthAddr eth = zeroEthAddr;
	SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("mDNSResponder:GetBSSID"), NULL, NULL);
	if (!store)
		LogMsg("GetBSSID: SCDynamicStoreCreate failed: %s", SCErrorString(SCError()));
	else
		{
		CFStringRef entityname = CFStringCreateWithFormat(NULL, NULL, CFSTR("State:/Network/Interface/%s/AirPort"), ifa_name);
		if (entityname)
			{
			CFDictionaryRef dict = SCDynamicStoreCopyValue(store, entityname);
			if (dict)
				{
				CFRange range = { 0, 6 };		// Offset, length
				CFDataRef data = CFDictionaryGetValue(dict, CFSTR("BSSID"));
				if (data && CFDataGetLength(data) == 6) CFDataGetBytes(data, range, eth.b);
				CFRelease(dict);
				}
			CFRelease(entityname);
			}
		CFRelease(store);
		}
	return(eth);
	}

mDNSlocal int GetMAC(mDNSEthAddr *eth, u_short ifindex)
	{
	struct ifaddrs *ifa;
	for (ifa = myGetIfAddrs(0); ifa; ifa = ifa->ifa_next)
		if (ifa->ifa_addr->sa_family == AF_LINK)
			{
			const struct sockaddr_dl *const sdl = (const struct sockaddr_dl *)ifa->ifa_addr;
			if (sdl->sdl_index == ifindex)
				{ mDNSPlatformMemCopy(eth->b, sdl->sdl_data + sdl->sdl_nlen, 6); return 0; }
			}
	*eth = zeroEthAddr;
	return -1;
	}

#ifndef SIOCGIFWAKEFLAGS
#define SIOCGIFWAKEFLAGS _IOWR('i', 136, struct ifreq) /* get interface wake property flags */
#endif

#ifndef IF_WAKE_ON_MAGIC_PACKET
#define IF_WAKE_ON_MAGIC_PACKET 0x01
#endif

#ifndef ifr_wake_flags
#define ifr_wake_flags ifr_ifru.ifru_intval
#endif

mDNSlocal mDNSBool NetWakeInterface(NetworkInterfaceInfoOSX *i)
	{
	if (!MulticastInterface(i)     ) return(mDNSfalse);	// We only use Sleep Proxy Service on multicast-capable interfaces
	if (i->ifa_flags & IFF_LOOPBACK) return(mDNSfalse);	// except loopback

	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) { LogMsg("NetWakeInterface %s socket failed %s errno %d (%s)", i->ifinfo.ifname, errno, strerror(errno)); return(mDNSfalse); }

	struct ifreq ifr;
	strlcpy(ifr.ifr_name, i->ifinfo.ifname, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFWAKEFLAGS, &ifr) < 0)
		{
		// For some strange reason, in /usr/include/sys/errno.h, EOPNOTSUPP is defined to be
		// 102 when compiling kernel code, and 45 when compiling user-level code. Since this
		// error code is being returned from the kernel, we need to use the kernel version.
		#define KERNEL_EOPNOTSUPP 102
		if (errno != KERNEL_EOPNOTSUPP)	// "Operation not supported on socket", the expected result on Leopard and earlier
			LogMsg("NetWakeInterface SIOCGIFWAKEFLAGS %s errno %d (%s)", i->ifinfo.ifname, errno, strerror(errno));
		// If on Leopard or earlier, we get EOPNOTSUPP, so in that case
		// we enable WOL if this interface is not AirPort and "Wake for Network access" is turned on.
		ifr.ifr_wake_flags = (errno == KERNEL_EOPNOTSUPP && !(i)->BSSID.l[0] && i->m->SystemWakeOnLANEnabled) ? IF_WAKE_ON_MAGIC_PACKET : 0;
		}
#if ASSUME_SNOWLEOPARD_INCORRECTLY_REPORTS_AIRPORT_INCAPABLE_OF_WAKE_ON_LAN
	else
		{
		// Call succeeded.
		// However, on SnowLeopard, it currently indicates incorrectly that AirPort is incapable of Wake-on-LAN.
		// Therefore, for AirPort interfaces, we just track the system-wide Wake-on-LAN setting.
		if ((i)->BSSID.l[0]) ifr.ifr_wake_flags = i->m->SystemWakeOnLANEnabled ? IF_WAKE_ON_MAGIC_PACKET : 0;
		}
#endif

	close(s);

	// ifr.ifr_wake_flags = IF_WAKE_ON_MAGIC_PACKET;	// For testing with MacBook Air, using a USB dongle that doesn't actually support Wake-On-LAN

	LogSPS("%-6s %#-14a %s WOMP", i->ifinfo.ifname, &i->ifinfo.ip, (ifr.ifr_wake_flags & IF_WAKE_ON_MAGIC_PACKET) ? "supports" : "no");

	return((ifr.ifr_wake_flags & IF_WAKE_ON_MAGIC_PACKET) != 0);
	}

// Returns pointer to newly created NetworkInterfaceInfoOSX object, or
// pointer to already-existing NetworkInterfaceInfoOSX object found in list, or
// may return NULL if out of memory (unlikely) or parameters are invalid for some reason
// (e.g. sa_family not AF_INET or AF_INET6)
mDNSlocal NetworkInterfaceInfoOSX *AddInterfaceToList(mDNS *const m, struct ifaddrs *ifa, mDNSs32 utc)
	{
	mDNSu32 scope_id  = if_nametoindex(ifa->ifa_name);
	mDNSEthAddr bssid = GetBSSID(ifa->ifa_name);

	mDNSAddr ip, mask;
	if (SetupAddr(&ip,   ifa->ifa_addr   ) != mStatus_NoError) return(NULL);
	if (SetupAddr(&mask, ifa->ifa_netmask) != mStatus_NoError) return(NULL);

	NetworkInterfaceInfoOSX **p;
	for (p = &m->p->InterfaceList; *p; p = &(*p)->next)
		if (scope_id == (*p)->scope_id &&
			mDNSSameAddress(&ip, &(*p)->ifinfo.ip) &&
			mDNSSameEthAddress(&bssid, &(*p)->BSSID))
			{
			debugf("AddInterfaceToList: Found existing interface %lu %.6a with address %#a at %p", scope_id, &bssid, &ip, *p);
			(*p)->Exists = mDNStrue;
			// If interface was not in getifaddrs list last time we looked, but it is now, update 'AppearanceTime' for this record
			if ((*p)->LastSeen != utc) (*p)->AppearanceTime = utc;

			// If Wake-on-LAN capability of this interface has changed (e.g. because power cable on laptop has been disconnected)
			// we may need to start or stop or sleep proxy browse operation
			const mDNSBool NetWake = NetWakeInterface(*p);
			if ((*p)->ifinfo.NetWake != NetWake)
				{
				(*p)->ifinfo.NetWake = NetWake;
				// If this interface is already registered with mDNSCore, then we need to start or stop its NetWake browse on-the-fly.
				// If this interface is not already registered (i.e. it's a dormant interface we had in our list
				// from when we previously saw it) then we mustn't do that, because mDNSCore doesn't know about it yet.
				// In this case, the mDNS_RegisterInterface() call will take care of starting the NetWake browse if necessary.
				if ((*p)->Registered)
					{
					mDNS_Lock(m);
					if (NetWake) mDNS_ActivateNetWake_internal  (m, &(*p)->ifinfo);
					else         mDNS_DeactivateNetWake_internal(m, &(*p)->ifinfo);
					mDNS_Unlock(m);
					}
				}

			return(*p);
			}

	NetworkInterfaceInfoOSX *i = (NetworkInterfaceInfoOSX *)mallocL("NetworkInterfaceInfoOSX", sizeof(*i));
	debugf("AddInterfaceToList: Making   new   interface %lu %.6a with address %#a at %p", scope_id, &bssid, &ip, i);
	if (!i) return(mDNSNULL);
	mDNSPlatformMemZero(i, sizeof(NetworkInterfaceInfoOSX));
	i->ifinfo.InterfaceID = (mDNSInterfaceID)(uintptr_t)scope_id;
	i->ifinfo.ip          = ip;
	i->ifinfo.mask        = mask;
	strlcpy(i->ifinfo.ifname, ifa->ifa_name, sizeof(i->ifinfo.ifname));
	i->ifinfo.ifname[sizeof(i->ifinfo.ifname)-1] = 0;
	// We can be configured to disable multicast advertisement, but we want to to support
	// local-only services, which need a loopback address record.
	i->ifinfo.Advertise   = m->DivertMulticastAdvertisements ? ((ifa->ifa_flags & IFF_LOOPBACK) ? mDNStrue : mDNSfalse) : m->AdvertiseLocalAddresses;
	i->ifinfo.McastTxRx   = mDNSfalse; // For now; will be set up later at the end of UpdateInterfaceList

	i->next            = mDNSNULL;
	i->m               = m;
	i->Exists          = mDNStrue;
	i->Flashing        = mDNSfalse;
	i->Occulting       = mDNSfalse;
	i->AppearanceTime  = utc;		// Brand new interface; AppearanceTime is now
	i->LastSeen        = utc;
	i->ifa_flags       = ifa->ifa_flags;
	i->scope_id        = scope_id;
	i->BSSID           = bssid;
	i->sa_family       = ifa->ifa_addr->sa_family;
	i->BPF_fd          = -1;
	i->BPF_len         = 0;
	i->Registered	   = mDNSNULL;

	// Do this AFTER i->BSSID has been set up
	i->ifinfo.NetWake  = NetWakeInterface(i);
	GetMAC(&i->ifinfo.MAC, scope_id);
	if (i->ifinfo.NetWake && !i->ifinfo.MAC.l[0])
		LogMsg("AddInterfaceToList: Bad MAC address %.6a for %d %s %#a", &i->ifinfo.MAC, scope_id, i->ifinfo.ifname, &ip);

	*p = i;
	return(i);
	}

#if USE_V6_ONLY_WHEN_NO_ROUTABLE_V4
mDNSlocal NetworkInterfaceInfoOSX *FindRoutableIPv4(mDNS *const m, mDNSu32 scope_id)
	{
	NetworkInterfaceInfoOSX *i;
	for (i = m->p->InterfaceList; i; i = i->next)
		if (i->Exists && i->scope_id == scope_id && i->ifinfo.ip.type == mDNSAddrType_IPv4)
			if (!mDNSv4AddressIsLinkLocal(&i->ifinfo.ip.ip.v4))
				return(i);
	return(mDNSNULL);
	}
#endif

#if APPLE_OSX_mDNSResponder

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - AutoTunnel
#endif

#define kRacoonPort 4500

static DomainAuthInfo* AnonymousRacoonConfig = mDNSNULL;

#ifndef NO_SECURITYFRAMEWORK

static CFMutableDictionaryRef domainStatusDict = NULL;

// MUST be called with lock held
mDNSlocal void RemoveAutoTunnelDomainStatus(const mDNS *const m, const DomainAuthInfo *const info)
	{
	char buffer[1024];
	CFStringRef domain;

	LogInfo("RemoveAutoTunnelDomainStatus: %##s", info->domain.c);

	if (!domainStatusDict) { LogMsg("RemoveAutoTunnelDomainStatus: No domainStatusDict"); return; }
	
	buffer[mDNS_snprintf(buffer, sizeof(buffer), "%##s", info->domain.c) - 1] = 0;
	domain = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
	if (!domain) { LogMsg("RemoveAutoTunnelDomainStatus: Could not create CFString domain"); return; }
	
	if (CFDictionaryContainsKey(domainStatusDict, domain))
		{
		CFDictionaryRemoveValue(domainStatusDict, domain);
		if (!m->ShutdownTime) mDNSDynamicStoreSetConfig(kmDNSBackToMyMacConfig, mDNSNULL, domainStatusDict);
		}
	CFRelease(domain);
	}

#endif // ndef NO_SECURITYFRAMEWORK

mDNSlocal mStatus CheckQuestionForStatus(const DNSQuestion *const q)
	{
	if (q->LongLived)
		{
		if (q->nta || (q->servAddr.type == mDNSAddrType_IPv4 && mDNSIPv4AddressIsOnes(q->servAddr.ip.v4)))
			return mStatus_NoSuchRecord;
		else if (q->state == LLQ_Poll)
			return mStatus_PollingMode;
		else if (q->state != LLQ_Established && !q->DuplicateOf)
			return mStatus_TransientErr;
		}
	
	return mStatus_NoError;
}

// MUST be called with lock held
mDNSlocal void UpdateAutoTunnelDomainStatus(const mDNS *const m, const DomainAuthInfo *const info)
	{
#ifdef NO_SECURITYFRAMEWORK
	(void)m;
	(void)info;
#else
	const NATTraversalInfo *const llq = m->LLQNAT.clientContext ? &m->LLQNAT : mDNSNULL;
	const NATTraversalInfo *const tun = info->AutoTunnelNAT.clientContext ? &info->AutoTunnelNAT : mDNSNULL;
	char buffer[1024];
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFStringRef domain = NULL;
	CFStringRef tmp = NULL;
	CFNumberRef num = NULL;
	mStatus status = mStatus_NoError;
	
	if (!domainStatusDict)
		{
		domainStatusDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (!domainStatusDict) { LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFDictionary domainStatusDict"); return; }
		}
	
	if (!dict) { LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFDictionary dict"); return; }

	buffer[mDNS_snprintf(buffer, sizeof(buffer), "%##s", info->domain.c) - 1] = 0;
	domain = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
	if (!domain) { LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFString domain"); return; }

	mDNS_snprintf(buffer, sizeof(buffer), "%#a", &m->Router);
	tmp = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
	if (!tmp)
		LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFString RouterAddress");
	else
		{
		CFDictionarySetValue(dict, CFSTR("RouterAddress"), tmp);
		CFRelease(tmp);
		}
	
	mDNS_snprintf(buffer, sizeof(buffer), "%.4a", &m->ExternalAddress);
	tmp = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
	if (!tmp)
		LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFString ExternalAddress");
	else
		{
		CFDictionarySetValue(dict, CFSTR("ExternalAddress"), tmp);
		CFRelease(tmp);
		}
	
	if (llq)
		{
		mDNSu32 port = mDNSVal16(llq->ExternalPort);
		
		num = CFNumberCreate(NULL, kCFNumberSInt32Type, &port);
		if (!num)
			LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFNumber LLQExternalPort");
		else
			{
			CFDictionarySetValue(dict, CFSTR("LLQExternalPort"), num);
			CFRelease(num);
			}

		if (llq->Result)
			{
			num = CFNumberCreate(NULL, kCFNumberSInt32Type, &llq->Result);
			if (!num)
				LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFNumber LLQNPMStatus");
			else
				{
				CFDictionarySetValue(dict, CFSTR("LLQNPMStatus"), num);
				CFRelease(num);
				}
			}
		}
	
	if (tun)
		{
		mDNSu32 port = mDNSVal16(tun->ExternalPort);
		
		num = CFNumberCreate(NULL, kCFNumberSInt32Type, &port);
		if (!num)
			LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFNumber AutoTunnelExternalPort");
		else
			{
			CFDictionarySetValue(dict, CFSTR("AutoTunnelExternalPort"), num);
			CFRelease(num);
			}

		if (tun->Result)
			{
			num = CFNumberCreate(NULL, kCFNumberSInt32Type, &tun->Result);
			if (!num)
				LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFNumber AutoTunnelNPMStatus");
			else
				{
				CFDictionarySetValue(dict, CFSTR("AutoTunnelNPMStatus"), num);
				CFRelease(num);
				}
			}
		}
	if (tun || llq)
		{
		mDNSu32 code = m->LastNATMapResultCode;
		
		num = CFNumberCreate(NULL, kCFNumberSInt32Type, &code);
		if (!num)
			LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFNumber LastNATMapResultCode");
		else
			{
			CFDictionarySetValue(dict, CFSTR("LastNATMapResultCode"), num);
			CFRelease(num);
			}
		}
	
	if (!llq && !tun)
		{
		status = mStatus_NotInitializedErr;
		mDNS_snprintf(buffer, sizeof(buffer), "Neither LLQ nor AutoTunnel NAT port mapping is currently active");
		}
	else if ((llq && llq->Result == mStatus_DoubleNAT) || (tun && tun->Result == mStatus_DoubleNAT))
		{
		status = mStatus_DoubleNAT;
		mDNS_snprintf(buffer, sizeof(buffer), "Double NAT: Router is reporting an external address");
		}
	else if ((llq && llq->Result == mStatus_NATPortMappingDisabled) || (tun && tun->Result == mStatus_NATPortMappingDisabled) ||
	         (m->LastNATMapResultCode == NATErr_Refused && ((llq && !llq->Result && mDNSIPPortIsZero(llq->ExternalPort)) || (tun && !tun->Result && mDNSIPPortIsZero(tun->ExternalPort)))))
		{
		status = mStatus_NATPortMappingDisabled;
		mDNS_snprintf(buffer, sizeof(buffer), "NAT-PMP is disabled on the router");
		}
	else if ((llq && llq->Result) || (tun && tun->Result))
		{
		status = mStatus_NATTraversal;
		mDNS_snprintf(buffer, sizeof(buffer), "Error obtaining NAT port mapping from router");
		}
	else if (m->Router.type == mDNSAddrType_None)
		{
		status = mStatus_NoRouter;
		mDNS_snprintf(buffer, sizeof(buffer), "No network connection - none");
		}
	else if (m->Router.type == mDNSAddrType_IPv4 && mDNSIPv4AddressIsZero(m->Router.ip.v4))
		{
		status = mStatus_NoRouter;
		mDNS_snprintf(buffer, sizeof(buffer), "No network connection - v4 zero");
		}
	else if ((llq && mDNSIPPortIsZero(llq->ExternalPort)) || (tun && mDNSIPPortIsZero(tun->ExternalPort)))
		{
		status = mStatus_NATTraversal;
		mDNS_snprintf(buffer, sizeof(buffer), "Unable to obtain NAT port mapping from router");
		}
	else
		{
		DNSQuestion* q, *worst_q = mDNSNULL;
		for (q = m->Questions; q; q=q->next)
			if (q->AuthInfo == info)
				{
				mStatus newStatus = CheckQuestionForStatus(q);
				if 		(newStatus == mStatus_NoSuchRecord) { status = newStatus; worst_q = q; break; }
				else if (newStatus == mStatus_PollingMode)  { status = newStatus; worst_q = q; }
				else if (newStatus == mStatus_TransientErr && status == mStatus_NoError) { status = newStatus; worst_q = q; }
				}
		
		if      (status == mStatus_NoError)      mDNS_snprintf(buffer, sizeof(buffer), "Success");
		else if (status == mStatus_NoSuchRecord) mDNS_snprintf(buffer, sizeof(buffer), "GetZoneData %s: %##s", worst_q->nta ? "not yet complete" : "failed", worst_q->qname.c);
		else if (status == mStatus_PollingMode)  mDNS_snprintf(buffer, sizeof(buffer), "Query polling %##s", worst_q->qname.c);
		else if (status == mStatus_TransientErr) mDNS_snprintf(buffer, sizeof(buffer), "Query not yet established %##s", worst_q->qname.c);
		}
	
	num = CFNumberCreate(NULL, kCFNumberSInt32Type, &status);
	if (!num)
		LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFNumber StatusCode");
	else
		{
		CFDictionarySetValue(dict, CFSTR("StatusCode"), num);
		CFRelease(num);
		}
		
	tmp = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
	if (!tmp)
		LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFString StatusMessage");
	else
		{
		CFDictionarySetValue(dict, CFSTR("StatusMessage"), tmp);
		CFRelease(tmp);
		}

	if (!CFDictionaryContainsKey(domainStatusDict, domain) ||
	    !CFEqual(dict, (CFMutableDictionaryRef)CFDictionaryGetValue(domainStatusDict, domain)))
	    {
		CFDictionarySetValue(domainStatusDict, domain, dict);
		if (!m->ShutdownTime) 
			{
			static char statusBuf[16];
			mDNS_snprintf(statusBuf, sizeof(statusBuf), "%d", (int)status);
			mDNSASLLog((uuid_t *)&m->asl_uuid, "autotunnel.domainstatus", status ? "failure" : "success", statusBuf, "");
			mDNSDynamicStoreSetConfig(kmDNSBackToMyMacConfig, mDNSNULL, domainStatusDict);
			}
		}
		
	CFRelease(domain);
	CFRelease(dict);

	debugf("UpdateAutoTunnelDomainStatus: %s", buffer);
#endif // def NO_SECURITYFRAMEWORK
	}

// MUST be called with lock held
mDNSexport void UpdateAutoTunnelDomainStatuses(const mDNS *const m)
	{
#ifdef NO_SECURITYFRAMEWORK
	(void)m;
#else
	DomainAuthInfo* info;
	for (info = m->AuthInfoList; info; info = info->next)
		if (info->AutoTunnel && !info->deltime)
			UpdateAutoTunnelDomainStatus(m, info);
#endif // def NO_SECURITYFRAMEWORK
	}
	
// MUST be called with lock held
mDNSlocal mDNSBool TunnelServers(mDNS *const m)
	{
	ServiceRecordSet *p;
	for (p = m->ServiceRegistrations; p; p = p->uDNS_next)
		{
		DomainAuthInfo *AuthInfo = GetAuthInfoForName_internal(m, p->RR_SRV.resrec.name);
		if (AuthInfo && AuthInfo->AutoTunnel && !AuthInfo->deltime) return(mDNStrue);
		}

	AuthRecord *r;
	for (r = m->ResourceRecords; r; r = r->next)
		if (r->resrec.rrtype == kDNSType_SRV)
			{
			DomainAuthInfo *AuthInfo = GetAuthInfoForName_internal(m, r->resrec.name);
			if (AuthInfo && AuthInfo->AutoTunnel && !AuthInfo->deltime) return(mDNStrue);
			}

	return(mDNSfalse);
	}

// MUST be called with lock held
mDNSlocal mDNSBool TunnelClients(mDNS *const m)
	{
	ClientTunnel *p;
	for (p = m->TunnelClients; p; p = p->next)
		if (p->q.ThisQInterval < 0)
			return(mDNStrue);
	return(mDNSfalse);
	}

mDNSlocal void RegisterAutoTunnelRecords(mDNS *m, DomainAuthInfo *info)
	{
	if (info->AutoTunnelNAT.clientContext && !info->AutoTunnelNAT.Result && !mDNSIPPortIsZero(info->AutoTunnelNAT.ExternalPort) && AutoTunnelUnregistered(info))
		{
		mStatus err;
		LogInfo("RegisterAutoTunnelRecords %##s (%#s)", info->domain.c, m->hostlabel.c);

		// 1. Set up our address record for the internal tunnel address
		// (User-visible user-friendly host name, used as target in AutoTunnel SRV records)
		info->AutoTunnelHostRecord.namestorage.c[0] = 0;
		AppendDomainLabel(&info->AutoTunnelHostRecord.namestorage, &m->hostlabel);
		AppendDomainName (&info->AutoTunnelHostRecord.namestorage, &info->domain);
		info->AutoTunnelHostRecord.resrec.rdata->u.ipv6 = m->AutoTunnelHostAddr;
		info->AutoTunnelHostRecord.resrec.RecordType = kDNSRecordTypeKnownUnique;
		err = mDNS_Register(m, &info->AutoTunnelHostRecord);
		if (err) LogMsg("RegisterAutoTunnelRecords error %d registering AutoTunnelHostRecord %##s", err, info->AutoTunnelHostRecord.namestorage.c);

		// 2. Set up device info record
		ConstructServiceName(&info->AutoTunnelDeviceInfo.namestorage, &m->nicelabel, &DeviceInfoName, &info->domain);
		mDNSu8 len = m->HIHardware.c[0] < 255 - 6 ? m->HIHardware.c[0] : 255 - 6;
		mDNSPlatformMemCopy(info->AutoTunnelDeviceInfo.resrec.rdata->u.data + 1, "model=", 6);
		mDNSPlatformMemCopy(info->AutoTunnelDeviceInfo.resrec.rdata->u.data + 7, m->HIHardware.c + 1, len);
		info->AutoTunnelDeviceInfo.resrec.rdata->u.data[0] = 6 + len;	// "model=" plus the device string
		info->AutoTunnelDeviceInfo.resrec.rdlength         = 7 + len;	// One extra for the length byte at the start of the string
		info->AutoTunnelDeviceInfo.resrec.RecordType = kDNSRecordTypeKnownUnique;
		err = mDNS_Register(m, &info->AutoTunnelDeviceInfo);
		if (err) LogMsg("RegisterAutoTunnelRecords error %d registering AutoTunnelDeviceInfo %##s", err, info->AutoTunnelDeviceInfo.namestorage.c);

		// 3. Set up our address record for the external tunnel address
		// (Constructed name, not generally user-visible, used as target in IKE tunnel's SRV record)
		info->AutoTunnelTarget.namestorage.c[0] = 0;
		AppendDomainLabel(&info->AutoTunnelTarget.namestorage, &m->AutoTunnelLabel);
		AppendDomainName (&info->AutoTunnelTarget.namestorage, &info->domain);
		info->AutoTunnelTarget.resrec.RecordType = kDNSRecordTypeKnownUnique;

		mDNS_Lock(m);
		mDNS_AddDynDNSHostName(m, &info->AutoTunnelTarget.namestorage, mDNSNULL, info);
		mDNS_Unlock(m);

		// 4. Set up IKE tunnel's SRV record: "AutoTunnelHostRecord SRV 0 0 port AutoTunnelTarget"
		AssignDomainName (&info->AutoTunnelService.namestorage, (const domainname*) "\x0B" "_autotunnel" "\x04" "_udp");
		AppendDomainLabel(&info->AutoTunnelService.namestorage, &m->hostlabel);
		AppendDomainName (&info->AutoTunnelService.namestorage, &info->domain);
		info->AutoTunnelService.resrec.rdata->u.srv.priority = 0;
		info->AutoTunnelService.resrec.rdata->u.srv.weight   = 0;
		info->AutoTunnelService.resrec.rdata->u.srv.port     = info->AutoTunnelNAT.ExternalPort;
		AssignDomainName(&info->AutoTunnelService.resrec.rdata->u.srv.target, &info->AutoTunnelTarget.namestorage);
		info->AutoTunnelService.resrec.RecordType = kDNSRecordTypeKnownUnique;
		err = mDNS_Register(m, &info->AutoTunnelService);
		if (err) LogMsg("RegisterAutoTunnelRecords error %d registering AutoTunnelService %##s", err, info->AutoTunnelService.namestorage.c);

		LogInfo("AutoTunnel server listening for connections on %##s[%.4a]:%d:%##s[%.16a]",
			info->AutoTunnelTarget.namestorage.c,     &m->AdvertisedV4.ip.v4, mDNSVal16(info->AutoTunnelNAT.IntPort),
			info->AutoTunnelHostRecord.namestorage.c, &m->AutoTunnelHostAddr);
		}
	}

mDNSlocal void DeregisterAutoTunnelRecords(mDNS *m, DomainAuthInfo *info)
	{
	LogInfo("DeregisterAutoTunnelRecords %##s", info->domain.c);
	if (info->AutoTunnelService.resrec.RecordType > kDNSRecordTypeDeregistering)
		{
		mStatus err = mDNS_Deregister(m, &info->AutoTunnelService);
		if (err)
			{
			info->AutoTunnelService.resrec.RecordType = kDNSRecordTypeUnregistered;
			LogMsg("DeregisterAutoTunnelRecords error %d deregistering AutoTunnelService %##s", err, info->AutoTunnelService.namestorage.c);
			}

		mDNS_Lock(m);
		mDNS_RemoveDynDNSHostName(m, &info->AutoTunnelTarget.namestorage);
		mDNS_Unlock(m);
		}

	if (info->AutoTunnelHostRecord.resrec.RecordType > kDNSRecordTypeDeregistering)
		{
		mStatus err = mDNS_Deregister(m, &info->AutoTunnelHostRecord);
		if (err)
			{
			info->AutoTunnelHostRecord.resrec.RecordType = kDNSRecordTypeUnregistered;
			LogMsg("DeregisterAutoTunnelRecords error %d deregistering AutoTunnelHostRecord %##s", err, info->AutoTunnelHostRecord.namestorage.c);
			}
		}

	if (info->AutoTunnelDeviceInfo.resrec.RecordType > kDNSRecordTypeDeregistering)
		{
		mStatus err = mDNS_Deregister(m, &info->AutoTunnelDeviceInfo);
		if (err)
			{
			info->AutoTunnelDeviceInfo.resrec.RecordType = kDNSRecordTypeUnregistered;
			LogMsg("DeregisterAutoTunnelRecords error %d deregistering AutoTunnelDeviceInfo %##s", err, info->AutoTunnelDeviceInfo.namestorage.c);
			}
		}
	}

mDNSlocal void AutoTunnelRecordCallback(mDNS *const m, AuthRecord *const rr, mStatus result)
	{
	DomainAuthInfo *info = (DomainAuthInfo *)rr->RecordContext;
	if (result == mStatus_MemFree)
		{
		LogInfo("AutoTunnelRecordCallback MemFree %s", ARDisplayString(m, rr));
		// Reset the host record namestorage to force high-level PTR/SRV/TXT to deregister
		if (rr == &info->AutoTunnelHostRecord)
			{
			rr->namestorage.c[0] = 0;
			m->NextSRVUpdate = NonZeroTime(m->timenow);
			}
		RegisterAutoTunnelRecords(m,info);
		}
	}

// Determine whether we need racoon to accept incoming connections
mDNSlocal void UpdateConfigureServer(mDNS *m)
	{
	DomainAuthInfo *info;
	
	for (info = m->AuthInfoList; info; info = info->next)
		if (info->AutoTunnel && !info->deltime && !mDNSIPPortIsZero(info->AutoTunnelNAT.ExternalPort))
			break;
	
	if (info != AnonymousRacoonConfig)
		{
		AnonymousRacoonConfig = info;
		// Create or revert configuration file, and start (or SIGHUP) Racoon
		(void)mDNSConfigureServer(AnonymousRacoonConfig ? kmDNSUp : kmDNSDown, AnonymousRacoonConfig ? &AnonymousRacoonConfig->domain : mDNSNULL);
		}
	}
		
mDNSlocal void AutoTunnelNATCallback(mDNS *m, NATTraversalInfo *n)
	{
	DomainAuthInfo *info = (DomainAuthInfo *)n->clientContext;
	LogInfo("AutoTunnelNATCallback Result %d %.4a Internal %d External %d %#s.%##s",
		n->Result, &n->ExternalAddress, mDNSVal16(n->IntPort), mDNSVal16(n->ExternalPort), m->hostlabel.c, info->domain.c);

	m->NextSRVUpdate = NonZeroTime(m->timenow);
	DeregisterAutoTunnelRecords(m,info);
	RegisterAutoTunnelRecords(m,info);
	
	UpdateConfigureServer(m);

	UpdateAutoTunnelDomainStatus(m, (DomainAuthInfo *)n->clientContext);
	}

mDNSlocal void AbortDeregistration(mDNS *const m, AuthRecord *rr)
	{
	if (rr->resrec.RecordType == kDNSRecordTypeDeregistering)
		{
		LogInfo("Aborting deregistration of %s", ARDisplayString(m, rr));
		CompleteDeregistration(m, rr);
		}
	else if (rr->resrec.RecordType != kDNSRecordTypeUnregistered)
		LogMsg("AbortDeregistration ERROR RecordType %02X for %s", ARDisplayString(m, rr));
	}

// Before SetupLocalAutoTunnelInterface_internal is called,
// m->AutoTunnelHostAddr.b[0] must be non-zero, and there must be at least one TunnelClient or TunnelServer
// Must be called with the lock held
mDNSexport void SetupLocalAutoTunnelInterface_internal(mDNS *const m)
	{
	LogInfo("SetupLocalAutoTunnelInterface");

	// 1. Configure the local IPv6 address
	if (!m->AutoTunnelHostAddrActive)
		{
		m->AutoTunnelHostAddrActive = mDNStrue;
		LogInfo("Setting up AutoTunnel address %.16a", &m->AutoTunnelHostAddr);
		(void)mDNSAutoTunnelInterfaceUpDown(kmDNSUp, m->AutoTunnelHostAddr.b);
		}

	// 2. If we have at least one server (pending) listening, publish our records
	if (TunnelServers(m))
		{
		DomainAuthInfo *info;
		for (info = m->AuthInfoList; info; info = info->next)
			{
			if (info->AutoTunnel && !info->deltime && !info->AutoTunnelNAT.clientContext)
				{
				// If we just resurrected a DomainAuthInfo that is still deregistering, we need to abort the deregistration process before re-using the AuthRecord memory
				AbortDeregistration(m, &info->AutoTunnelHostRecord);
				AbortDeregistration(m, &info->AutoTunnelDeviceInfo);
				AbortDeregistration(m, &info->AutoTunnelService);

				mDNS_SetupResourceRecord(&info->AutoTunnelHostRecord, mDNSNULL, mDNSInterface_Any, kDNSType_AAAA, kHostNameTTL, kDNSRecordTypeUnregistered, AutoTunnelRecordCallback, info);
				mDNS_SetupResourceRecord(&info->AutoTunnelDeviceInfo, mDNSNULL, mDNSInterface_Any, kDNSType_TXT,  kStandardTTL, kDNSRecordTypeUnregistered, AutoTunnelRecordCallback, info);
				mDNS_SetupResourceRecord(&info->AutoTunnelTarget,     mDNSNULL, mDNSInterface_Any, kDNSType_A,    kHostNameTTL, kDNSRecordTypeUnregistered, AutoTunnelRecordCallback, info);
				mDNS_SetupResourceRecord(&info->AutoTunnelService,    mDNSNULL, mDNSInterface_Any, kDNSType_SRV,  kHostNameTTL, kDNSRecordTypeUnregistered, AutoTunnelRecordCallback, info);

				// Try to get a NAT port mapping for the AutoTunnelService
				info->AutoTunnelNAT.clientCallback   = AutoTunnelNATCallback;
				info->AutoTunnelNAT.clientContext    = info;
				info->AutoTunnelNAT.Protocol         = NATOp_MapUDP;
				info->AutoTunnelNAT.IntPort          = mDNSOpaque16fromIntVal(kRacoonPort);
				info->AutoTunnelNAT.RequestedPort    = mDNSOpaque16fromIntVal(kRacoonPort);
				info->AutoTunnelNAT.NATLease         = 0;
				mStatus err = mDNS_StartNATOperation_internal(m, &info->AutoTunnelNAT);
				if (err) LogMsg("SetupLocalAutoTunnelInterface_internal error %d starting NAT mapping", err);
				}
			}
		}
	}

mDNSlocal mStatus AutoTunnelSetKeys(ClientTunnel *tun, mDNSBool AddNew)
	{
	return(mDNSAutoTunnelSetKeys(AddNew ? kmDNSAutoTunnelSetKeysReplace : kmDNSAutoTunnelSetKeysDelete, tun->loc_inner.b, tun->loc_outer.b, kRacoonPort, tun->rmt_inner.b, tun->rmt_outer.b, mDNSVal16(tun->rmt_outer_port), SkipLeadingLabels(&tun->dstname, 1)));
	}

// If the EUI-64 part of the IPv6 ULA matches, then that means the two addresses point to the same machine
#define mDNSSameClientTunnel(A,B) ((A)->l[2] == (B)->l[2] && (A)->l[3] == (B)->l[3])

mDNSlocal void ReissueBlockedQuestionWithType(mDNS *const m, domainname *d, mDNSBool success, mDNSu16 qtype)
	{
	DNSQuestion *q = m->Questions;
	while (q)
		{
		if (q->NoAnswer == NoAnswer_Suspended && q->qtype == qtype && q->AuthInfo && q->AuthInfo->AutoTunnel && SameDomainName(&q->qname, d))
			{
			LogInfo("Restart %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
			mDNSQuestionCallback *tmp = q->QuestionCallback;
			q->QuestionCallback = AutoTunnelCallback;	// Set QuestionCallback to suppress another call back to AddNewClientTunnel
			mDNS_StopQuery(m, q);
			mDNS_StartQuery(m, q);
			q->QuestionCallback = tmp;					// Restore QuestionCallback back to the real value
			if (!success) q->NoAnswer = NoAnswer_Fail;
			// When we call mDNS_StopQuery, it's possible for other subordinate questions like the GetZoneData query to be cancelled too.
			// In general we have to assume that the question list might have changed in arbitrary ways.
			// This code is itself called from a question callback, so the m->CurrentQuestion mechanism is
			// already in use. The safest solution is just to go back to the start of the list and start again.
			// In principle this sounds like an n^2 algorithm, but in practice we almost always activate
			// just one suspended question, so it's really a 2n algorithm.
			q = m->Questions;
			}
		else
			q = q->next;
		}
	}

mDNSlocal void ReissueBlockedQuestions(mDNS *const m, domainname *d, mDNSBool success)
	{
	// 1. We deliberately restart AAAA queries before A queries, because in the common case where a BTTM host has
	//    a v6 address but no v4 address, we prefer the caller to get the positive AAAA response before the A NXDOMAIN.
	// 2. In the case of AAAA queries, if our tunnel setup failed, then we return a deliberate failure indication to the caller --
	//    even if the name does have a valid AAAA record, we don't want clients trying to connect to it without a properly encrypted tunnel.
	// 3. For A queries we never fabricate failures -- if a BTTM service is really using raw IPv4, then it doesn't need the IPv6 tunnel.
	ReissueBlockedQuestionWithType(m, d, success, kDNSType_AAAA);
	ReissueBlockedQuestionWithType(m, d, mDNStrue, kDNSType_A);
	}

mDNSlocal void UnlinkAndReissueBlockedQuestions(mDNS *const m, ClientTunnel *tun, mDNSBool success)
	{
	ClientTunnel **p = &m->TunnelClients;
	while (*p != tun && *p) p = &(*p)->next;
	if (*p) *p = tun->next;
	ReissueBlockedQuestions(m, &tun->dstname, success);
	LogInfo("UnlinkAndReissueBlockedQuestions: Disposing ClientTunnel %p", tun);
	freeL("ClientTunnel", tun);
	}

mDNSexport void AutoTunnelCallback(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
	{
	ClientTunnel *tun = (ClientTunnel *)question->QuestionContext;
	LogInfo("AutoTunnelCallback tun %p AddRecord %d rdlength %d qtype %d", tun, AddRecord, answer->rdlength, question->qtype);

	if (!AddRecord) return;
	mDNS_StopQuery(m, question);

	if (!answer->rdlength)
		{
		LogInfo("AutoTunnelCallback NXDOMAIN %##s (%s)", question->qname.c, DNSTypeName(question->qtype));
		static char msgbuf[16];
		mDNS_snprintf(msgbuf, sizeof(msgbuf), "%s lookup", DNSTypeName(question->qtype));
		mDNSASLLog((uuid_t *)&m->asl_uuid, "autotunnel.config", "failure", msgbuf, "");
		UnlinkAndReissueBlockedQuestions(m, tun, mDNSfalse);
		return;
		}

	if (question->qtype == kDNSType_AAAA)
		{
		if (mDNSSameIPv6Address(answer->rdata->u.ipv6, m->AutoTunnelHostAddr))
			{
			LogInfo("AutoTunnelCallback: suppressing tunnel to self %.16a", &answer->rdata->u.ipv6);
			UnlinkAndReissueBlockedQuestions(m, tun, mDNStrue);
			return;
			}

		tun->rmt_inner = answer->rdata->u.ipv6;
		LogInfo("AutoTunnelCallback: dst host %.16a", &tun->rmt_inner);
		AssignDomainName(&question->qname, (const domainname*) "\x0B" "_autotunnel" "\x04" "_udp");
		AppendDomainName(&question->qname, &tun->dstname);
		question->qtype = kDNSType_SRV;
		mDNS_StartQuery(m, &tun->q);
		}
	else if (question->qtype == kDNSType_SRV)
		{
		LogInfo("AutoTunnelCallback: SRV target name %##s", answer->rdata->u.srv.target.c);
		AssignDomainName(&tun->q.qname, &answer->rdata->u.srv.target);
		tun->rmt_outer_port = answer->rdata->u.srv.port;
		question->qtype = kDNSType_A;
		mDNS_StartQuery(m, &tun->q);
		}
	else if (question->qtype == kDNSType_A)
		{
		ClientTunnel *old = mDNSNULL;
		LogInfo("AutoTunnelCallback: SRV target addr %.4a", &answer->rdata->u.ipv4);
		question->ThisQInterval = -1;		// So we know this tunnel setup has completed
		tun->rmt_outer = answer->rdata->u.ipv4;
		tun->loc_inner = m->AutoTunnelHostAddr;
		mDNSAddr tmpDst = { mDNSAddrType_IPv4, {{{0}}} };
		tmpDst.ip.v4 = tun->rmt_outer;
		mDNSAddr tmpSrc = zeroAddr;
		mDNSPlatformSourceAddrForDest(&tmpSrc, &tmpDst);
		if (tmpSrc.type == mDNSAddrType_IPv4) tun->loc_outer = tmpSrc.ip.v4;
		else tun->loc_outer = m->AdvertisedV4.ip.v4;

		ClientTunnel **p = &tun->next;
		mDNSBool needSetKeys = mDNStrue;
		while (*p)
			{
			if (!mDNSSameClientTunnel(&(*p)->rmt_inner, &tun->rmt_inner)) p = &(*p)->next;
			else
				{
				LogInfo("Found existing AutoTunnel for %##s %.16a", tun->dstname.c, &tun->rmt_inner);
				old = *p;
				*p = old->next;
				if (old->q.ThisQInterval >= 0) mDNS_StopQuery(m, &old->q);
				else if (!mDNSSameIPv6Address(old->loc_inner, tun->loc_inner) ||
						 !mDNSSameIPv4Address(old->loc_outer, tun->loc_outer) ||
						 !mDNSSameIPv6Address(old->rmt_inner, tun->rmt_inner) ||
						 !mDNSSameIPv4Address(old->rmt_outer, tun->rmt_outer) ||
						 !mDNSSameIPPort(old->rmt_outer_port, tun->rmt_outer_port))
					{
					LogInfo("Deleting existing AutoTunnel for %##s %.16a", tun->dstname.c, &tun->rmt_inner);
					AutoTunnelSetKeys(old, mDNSfalse);
					}
				else needSetKeys = mDNSfalse;

				LogInfo("AutoTunnelCallback: Disposing ClientTunnel %p", tun);
				freeL("ClientTunnel", old);
				}
			}

		if (needSetKeys) LogInfo("New AutoTunnel for %##s %.16a", tun->dstname.c, &tun->rmt_inner);

		if (m->AutoTunnelHostAddr.b[0]) { mDNS_Lock(m); SetupLocalAutoTunnelInterface_internal(m); mDNS_Unlock(m); };

		mStatus result = needSetKeys ? AutoTunnelSetKeys(tun, mDNStrue) : mStatus_NoError;
		static char msgbuf[32];
		mDNS_snprintf(msgbuf, sizeof(msgbuf), "Tunnel setup - %d", result);
		mDNSASLLog((uuid_t *)&m->asl_uuid, "autotunnel.config", result ? "failure" : "success", msgbuf, "");
		// Kick off any questions that were held pending this tunnel setup
		ReissueBlockedQuestions(m, &tun->dstname, (result == mStatus_NoError) ? mDNStrue : mDNSfalse);
		}
	else
		LogMsg("AutoTunnelCallback: Unknown question %p", question);
	}

// Must be called with the lock held
mDNSexport void AddNewClientTunnel(mDNS *const m, DNSQuestion *const q)
	{
	ClientTunnel *p = mallocL("ClientTunnel", sizeof(ClientTunnel));
	if (!p) return;
	AssignDomainName(&p->dstname, &q->qname);
	p->MarkedForDeletion = mDNSfalse;
	p->loc_inner      = zerov6Addr;
	p->loc_outer      = zerov4Addr;
	p->rmt_inner      = zerov6Addr;
	p->rmt_outer      = zerov4Addr;
	p->rmt_outer_port = zeroIPPort;
	p->next = m->TunnelClients;
	m->TunnelClients = p;		// We intentionally build list in reverse order

	p->q.InterfaceID      = mDNSInterface_Any;
	p->q.Target           = zeroAddr;
	AssignDomainName(&p->q.qname, &q->qname);
	p->q.qtype            = kDNSType_AAAA;
	p->q.qclass           = kDNSClass_IN;
	p->q.LongLived        = mDNSfalse;
	p->q.ExpectUnique     = mDNStrue;
	p->q.ForceMCast       = mDNSfalse;
	p->q.ReturnIntermed   = mDNStrue;
	p->q.QuestionCallback = AutoTunnelCallback;
	p->q.QuestionContext  = p;

	LogInfo("AddNewClientTunnel start tun %p %##s (%s)%s", p, &q->qname.c, DNSTypeName(q->qtype), q->LongLived ? " LongLived" : "");
	mDNS_StartQuery_internal(m, &p->q);
	}

#endif // APPLE_OSX_mDNSResponder

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Power State & Configuration Change Management
#endif

mDNSlocal mStatus UpdateInterfaceList(mDNS *const m, mDNSs32 utc)
	{
	mDNSBool foundav4           = mDNSfalse;
	mDNSBool foundav6           = mDNSfalse;
	struct ifaddrs *ifa         = myGetIfAddrs(1);
	struct ifaddrs *v4Loopback  = NULL;
	struct ifaddrs *v6Loopback  = NULL;
	char defaultname[64];
#ifndef NO_IPV6
	int InfoSocket              = socket(AF_INET6, SOCK_DGRAM, 0);
	if (InfoSocket < 3 && errno != EAFNOSUPPORT) LogMsg("UpdateInterfaceList: InfoSocket error %d errno %d (%s)", InfoSocket, errno, strerror(errno));
#endif
	if (m->SleepState == SleepState_Sleeping) ifa = NULL;

	while (ifa)
		{
#if LIST_ALL_INTERFACES
		if (ifa->ifa_addr->sa_family == AF_APPLETALK)
			LogMsg("UpdateInterfaceList: %5s(%d) Flags %04X Family %2d is AF_APPLETALK",
				ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family);
		else if (ifa->ifa_addr->sa_family == AF_LINK)
			LogMsg("UpdateInterfaceList: %5s(%d) Flags %04X Family %2d is AF_LINK",
				ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family);
		else if (ifa->ifa_addr->sa_family != AF_INET && ifa->ifa_addr->sa_family != AF_INET6)
			LogMsg("UpdateInterfaceList: %5s(%d) Flags %04X Family %2d not AF_INET (2) or AF_INET6 (30)",
				ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family);
		if (!(ifa->ifa_flags & IFF_UP))
			LogMsg("UpdateInterfaceList: %5s(%d) Flags %04X Family %2d Interface not IFF_UP",
				ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family);
		if (!(ifa->ifa_flags & IFF_MULTICAST))
			LogMsg("UpdateInterfaceList: %5s(%d) Flags %04X Family %2d Interface not IFF_MULTICAST",
				ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family);
		if (ifa->ifa_flags & IFF_POINTOPOINT)
			LogMsg("UpdateInterfaceList: %5s(%d) Flags %04X Family %2d Interface IFF_POINTOPOINT",
				ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family);
		if (ifa->ifa_flags & IFF_LOOPBACK)
			LogMsg("UpdateInterfaceList: %5s(%d) Flags %04X Family %2d Interface IFF_LOOPBACK",
				ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family);
#endif

		if (ifa->ifa_addr->sa_family == AF_LINK)
			{
			struct sockaddr_dl *sdl = (struct sockaddr_dl *)ifa->ifa_addr;
			if (sdl->sdl_type == IFT_ETHER && sdl->sdl_alen == sizeof(m->PrimaryMAC) && mDNSSameEthAddress(&m->PrimaryMAC, &zeroEthAddr))
				mDNSPlatformMemCopy(m->PrimaryMAC.b, sdl->sdl_data + sdl->sdl_nlen, 6);
			}

		if (ifa->ifa_flags & IFF_UP && ifa->ifa_addr)
			if (ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6)
				{
				if (!ifa->ifa_netmask)
					{
					mDNSAddr ip;
					SetupAddr(&ip, ifa->ifa_addr);
					LogMsg("getifaddrs: ifa_netmask is NULL for %5s(%d) Flags %04X Family %2d %#a",
						ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family, &ip);
					}
				// Apparently it's normal for the sa_family of an ifa_netmask to sometimes be zero, so we don't complain about that
				// <rdar://problem/5492035> getifaddrs is returning invalid netmask family for fw0 and vmnet
				else if (ifa->ifa_netmask->sa_family != ifa->ifa_addr->sa_family && ifa->ifa_netmask->sa_family != 0)
					{
					mDNSAddr ip;
					SetupAddr(&ip, ifa->ifa_addr);
					LogMsg("getifaddrs ifa_netmask for %5s(%d) Flags %04X Family %2d %#a has different family: %d",
						ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family, &ip, ifa->ifa_netmask->sa_family);
					}
					// Currently we use a few internal ones like mDNSInterfaceID_LocalOnly etc. that are negative values (0, -1, -2).
				else if ((int)if_nametoindex(ifa->ifa_name) <= 0)
					{
					LogMsg("UpdateInterfaceList: if_nametoindex returned zero/negative value for %5s(%d)", ifa->ifa_name, if_nametoindex(ifa->ifa_name));
					}
				else
					{
					// Make sure ifa_netmask->sa_family is set correctly
					// <rdar://problem/5492035> getifaddrs is returning invalid netmask family for fw0 and vmnet
					ifa->ifa_netmask->sa_family = ifa->ifa_addr->sa_family;
					int ifru_flags6 = 0;
#ifndef NO_IPV6
					struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
					if (ifa->ifa_addr->sa_family == AF_INET6 && InfoSocket >= 0)
						{
						struct in6_ifreq ifr6;
						mDNSPlatformMemZero((char *)&ifr6, sizeof(ifr6));
						strlcpy(ifr6.ifr_name, ifa->ifa_name, sizeof(ifr6.ifr_name));
						ifr6.ifr_addr = *sin6;
						if (ioctl(InfoSocket, SIOCGIFAFLAG_IN6, &ifr6) != -1)
							ifru_flags6 = ifr6.ifr_ifru.ifru_flags6;
						verbosedebugf("%s %.16a %04X %04X", ifa->ifa_name, &sin6->sin6_addr, ifa->ifa_flags, ifru_flags6);
						}
#endif
					if (!(ifru_flags6 & (IN6_IFF_NOTREADY | IN6_IFF_DETACHED | IN6_IFF_DEPRECATED | IN6_IFF_TEMPORARY)))
						{
						if (ifa->ifa_flags & IFF_LOOPBACK)
							{
							if (ifa->ifa_addr->sa_family == AF_INET)     v4Loopback = ifa;
#ifndef NO_IPV6
							else if (sin6->sin6_addr.s6_addr[0] != 0xFD) v6Loopback = ifa;
#endif
							}
						else
							{
							NetworkInterfaceInfoOSX *i = AddInterfaceToList(m, ifa, utc);
							if (i && MulticastInterface(i) && i->ifinfo.Advertise)
								{
								if (ifa->ifa_addr->sa_family == AF_INET) foundav4 = mDNStrue;
								else                                     foundav6 = mDNStrue;
								}
							}
						}
					}
				}
		ifa = ifa->ifa_next;
		}

	// For efficiency, we don't register a loopback interface when other interfaces of that family are available and advertising
	if (!foundav4 && v4Loopback) AddInterfaceToList(m, v4Loopback, utc);
	if (!foundav6 && v6Loopback) AddInterfaceToList(m, v6Loopback, utc);

	// Now the list is complete, set the McastTxRx setting for each interface.
	NetworkInterfaceInfoOSX *i;
	for (i = m->p->InterfaceList; i; i = i->next)
		if (i->Exists)
			{
			mDNSBool txrx = MulticastInterface(i);
#if USE_V6_ONLY_WHEN_NO_ROUTABLE_V4
			txrx = txrx && ((i->ifinfo.ip.type == mDNSAddrType_IPv4) || !FindRoutableIPv4(m, i->scope_id));
#endif
			if (i->ifinfo.McastTxRx != txrx)
				{
				i->ifinfo.McastTxRx = txrx;
				i->Exists = 2; // State change; need to deregister and reregister this interface
				}
			}

#ifndef NO_IPV6
	if (InfoSocket >= 0) close(InfoSocket);
#endif

	// If we haven't set up AutoTunnelHostAddr yet, do it now
	if (!mDNSSameEthAddress(&m->PrimaryMAC, &zeroEthAddr) && m->AutoTunnelHostAddr.b[0] == 0)
		{
		m->AutoTunnelHostAddr.b[0x0] = 0xFD;		// Required prefix for "locally assigned" ULA (See RFC 4193)
		m->AutoTunnelHostAddr.b[0x1] = mDNSRandom(255);
		m->AutoTunnelHostAddr.b[0x2] = mDNSRandom(255);
		m->AutoTunnelHostAddr.b[0x3] = mDNSRandom(255);
		m->AutoTunnelHostAddr.b[0x4] = mDNSRandom(255);
		m->AutoTunnelHostAddr.b[0x5] = mDNSRandom(255);
		m->AutoTunnelHostAddr.b[0x6] = mDNSRandom(255);
		m->AutoTunnelHostAddr.b[0x7] = mDNSRandom(255);
		m->AutoTunnelHostAddr.b[0x8] = m->PrimaryMAC.b[0] ^ 0x02;	// See RFC 3513, Appendix A for explanation
		m->AutoTunnelHostAddr.b[0x9] = m->PrimaryMAC.b[1];
		m->AutoTunnelHostAddr.b[0xA] = m->PrimaryMAC.b[2];
		m->AutoTunnelHostAddr.b[0xB] = 0xFF;
		m->AutoTunnelHostAddr.b[0xC] = 0xFE;
		m->AutoTunnelHostAddr.b[0xD] = m->PrimaryMAC.b[3];
		m->AutoTunnelHostAddr.b[0xE] = m->PrimaryMAC.b[4];
		m->AutoTunnelHostAddr.b[0xF] = m->PrimaryMAC.b[5];
		m->AutoTunnelLabel.c[0] = mDNS_snprintf((char*)m->AutoTunnelLabel.c+1, 254, "AutoTunnel-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X",
			m->AutoTunnelHostAddr.b[0x8], m->AutoTunnelHostAddr.b[0x9], m->AutoTunnelHostAddr.b[0xA], m->AutoTunnelHostAddr.b[0xB],
			m->AutoTunnelHostAddr.b[0xC], m->AutoTunnelHostAddr.b[0xD], m->AutoTunnelHostAddr.b[0xE], m->AutoTunnelHostAddr.b[0xF]);
		LogInfo("m->AutoTunnelLabel %#s", m->AutoTunnelLabel.c);
		}
	
	mDNS_snprintf(defaultname, sizeof(defaultname), "%.*s-%02X%02X%02X%02X%02X%02X", HINFO_HWstring_prefixlen, HINFO_HWstring,
		m->PrimaryMAC.b[0], m->PrimaryMAC.b[1], m->PrimaryMAC.b[2], m->PrimaryMAC.b[3], m->PrimaryMAC.b[4], m->PrimaryMAC.b[5]);

	// Set up the nice label
	domainlabel nicelabel;
	nicelabel.c[0] = 0;
	GetUserSpecifiedFriendlyComputerName(&nicelabel);
	if (nicelabel.c[0] == 0)
		{
		debugf("Couldn’t read user-specified Computer Name; using default “%s” instead", defaultname);
		MakeDomainLabelFromLiteralString(&nicelabel, defaultname);
		}

	// Set up the RFC 1034-compliant label
	domainlabel hostlabel;
	hostlabel.c[0] = 0;
	GetUserSpecifiedLocalHostName(&hostlabel);
	if (hostlabel.c[0] == 0)
		{
		debugf("Couldn’t read user-specified Local Hostname; using default “%s.local” instead", defaultname);
		MakeDomainLabelFromLiteralString(&hostlabel, defaultname);
		}

	mDNSBool namechange = mDNSfalse;

	// We use a case-sensitive comparison here because even though changing the capitalization
	// of the name alone is not significant to DNS, it's still a change from the user's point of view
	if (SameDomainLabelCS(m->p->usernicelabel.c, nicelabel.c))
		debugf("Usernicelabel (%#s) unchanged since last time; not changing m->nicelabel (%#s)", m->p->usernicelabel.c, m->nicelabel.c);
	else
		{
		if (m->p->usernicelabel.c[0])	// Don't show message first time through, when we first read name from prefs on boot
			LogMsg("User updated Computer Name from “%#s” to “%#s”", m->p->usernicelabel.c, nicelabel.c);
		m->p->usernicelabel = m->nicelabel = nicelabel;
		namechange = mDNStrue;
		}

	if (SameDomainLabelCS(m->p->userhostlabel.c, hostlabel.c))
		debugf("Userhostlabel (%#s) unchanged since last time; not changing m->hostlabel (%#s)", m->p->userhostlabel.c, m->hostlabel.c);
	else
		{
		if (m->p->userhostlabel.c[0])	// Don't show message first time through, when we first read name from prefs on boot
			LogMsg("User updated Local Hostname from “%#s” to “%#s”", m->p->userhostlabel.c, hostlabel.c);
		m->p->userhostlabel = m->hostlabel = hostlabel;
		mDNS_SetFQDN(m);
		namechange = mDNStrue;
		}

#if APPLE_OSX_mDNSResponder
	if (namechange)		// If either name has changed, we need to tickle our AutoTunnel state machine to update its registered records
		{
		DomainAuthInfo *info;
		for (info = m->AuthInfoList; info; info = info->next)
			if (info->AutoTunnelNAT.clientContext && !mDNSIPv4AddressIsOnes(info->AutoTunnelNAT.ExternalAddress))
				AutoTunnelNATCallback(m, &info->AutoTunnelNAT);
		}
#endif // APPLE_OSX_mDNSResponder

	return(mStatus_NoError);
	}

// Returns number of leading one-bits in mask: 0-32 for IPv4, 0-128 for IPv6
// Returns -1 if all the one-bits are not contiguous
mDNSlocal int CountMaskBits(mDNSAddr *mask)
	{
	int i = 0, bits = 0;
	int bytes = mask->type == mDNSAddrType_IPv4 ? 4 : mask->type == mDNSAddrType_IPv6 ? 16 : 0;
	while (i < bytes)
		{
		mDNSu8 b = mask->ip.v6.b[i++];
		while (b & 0x80) { bits++; b <<= 1; }
		if (b) return(-1);
		}
	while (i < bytes) if (mask->ip.v6.b[i++]) return(-1);
	return(bits);
	}

// returns count of non-link local V4 addresses registered
mDNSlocal int SetupActiveInterfaces(mDNS *const m, mDNSs32 utc)
	{
	NetworkInterfaceInfoOSX *i;
	int count = 0;
	for (i = m->p->InterfaceList; i; i = i->next)
		if (i->Exists)
			{
			NetworkInterfaceInfo *const n = &i->ifinfo;
			NetworkInterfaceInfoOSX *primary = SearchForInterfaceByName(m, i->ifinfo.ifname, AAAA_OVER_V4 ? AF_UNSPEC : i->sa_family);
			if (!primary) LogMsg("SetupActiveInterfaces ERROR! SearchForInterfaceByName didn't find %s", i->ifinfo.ifname);

			if (i->Registered && i->Registered != primary)	// Sanity check
				{
				LogMsg("SetupActiveInterfaces ERROR! n->Registered %p != primary %p", i->Registered, primary);
				i->Registered = mDNSNULL;
				}

			if (!i->Registered)
				{
				// Note: If i->Registered is set, that means we've called mDNS_RegisterInterface() for this interface,
				// so we need to make sure we call mDNS_DeregisterInterface() before disposing it.
				// If i->Registered is NOT set, then we haven't registered it and we should not try to deregister it
				//

				i->Registered = primary;

				// If i->LastSeen == utc, then this is a brand-new interface, just created, or an interface that never went away.
				// If i->LastSeen != utc, then this is an old interface, previously seen, that went away for (utc - i->LastSeen) seconds.
				// If the interface is an old one that went away and came back in less than a minute, then we're in a flapping scenario.
				i->Occulting = !(i->ifa_flags & IFF_LOOPBACK) && (utc - i->LastSeen > 0 && utc - i->LastSeen < 60);

				mDNS_RegisterInterface(m, n, i->Flashing && i->Occulting);

				if (!mDNSAddressIsLinkLocal(&n->ip)) count++;
				LogInfo("SetupActiveInterfaces:   Registered    %5s(%lu) %.6a InterfaceID %p(%p), primary %p, %#a/%d%s%s%s",
					i->ifinfo.ifname, i->scope_id, &i->BSSID, i->ifinfo.InterfaceID, i, primary, &n->ip, CountMaskBits(&n->mask),
					i->Flashing        ? " (Flashing)"  : "",
					i->Occulting       ? " (Occulting)" : "",
					n->InterfaceActive ? " (Primary)"   : "");

				if (!n->McastTxRx)
					debugf("SetupActiveInterfaces:   No Tx/Rx on   %5s(%lu) %.6a InterfaceID %p %#a", i->ifinfo.ifname, i->scope_id, &i->BSSID, i->ifinfo.InterfaceID, &n->ip);
				else
					{
					if (i->sa_family == AF_INET)
						{
						struct ip_mreq imr;
						primary->ifa_v4addr.s_addr = n->ip.ip.v4.NotAnInteger;
						imr.imr_multiaddr.s_addr = AllDNSLinkGroup_v4.ip.v4.NotAnInteger;
						imr.imr_interface        = primary->ifa_v4addr;
	
						// If this is our *first* IPv4 instance for this interface name, we need to do a IP_DROP_MEMBERSHIP first,
						// before trying to join the group, to clear out stale kernel state which may be lingering.
						// In particular, this happens with removable network interfaces like USB Ethernet adapters -- the kernel has stale state
						// from the last time the USB Ethernet adapter was connected, and part of the kernel thinks we've already joined the group
						// on that interface (so we get EADDRINUSE when we try to join again) but a different part of the kernel thinks we haven't
						// joined the group (so we receive no multicasts). Doing an IP_DROP_MEMBERSHIP before joining seems to flush the stale state.
						// Also, trying to make the code leave the group when the adapter is removed doesn't work either,
						// because by the time we get the configuration change notification, the interface is already gone,
						// so attempts to unsubscribe fail with EADDRNOTAVAIL (errno 49 "Can't assign requested address").
						// <rdar://problem/5585972> IP_ADD_MEMBERSHIP fails for previously-connected removable interfaces
						if (SearchForInterfaceByName(m, i->ifinfo.ifname, AF_INET) == i)
							{
							LogInfo("SetupActiveInterfaces: %5s(%lu) Doing precautionary IP_DROP_MEMBERSHIP for %.4a on %.4a", i->ifinfo.ifname, i->scope_id, &imr.imr_multiaddr, &imr.imr_interface);
							mStatus err = setsockopt(m->p->permanentsockets.sktv4, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr, sizeof(imr));
							if (err < 0 && (errno != EADDRNOTAVAIL))
								LogMsg("setsockopt - IP_DROP_MEMBERSHIP error %d errno %d (%s)", err, errno, strerror(errno));
							}
	
						LogInfo("SetupActiveInterfaces: %5s(%lu) joining IPv4 mcast group %.4a on %.4a", i->ifinfo.ifname, i->scope_id, &imr.imr_multiaddr, &imr.imr_interface);
						mStatus err = setsockopt(m->p->permanentsockets.sktv4, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(imr));
						// Joining same group twice can give "Address already in use" error -- no need to report that
						if (err < 0 && (errno != EADDRINUSE))
							LogMsg("setsockopt - IP_ADD_MEMBERSHIP error %d errno %d (%s) group %.4a on %.4a", err, errno, strerror(errno), &imr.imr_multiaddr, &imr.imr_interface);
						}
#ifndef NO_IPV6
					if (i->sa_family == AF_INET6)
						{
						struct ipv6_mreq i6mr;
						i6mr.ipv6mr_interface = primary->scope_id;
						i6mr.ipv6mr_multiaddr = *(struct in6_addr*)&AllDNSLinkGroup_v6.ip.v6;
	
						if (SearchForInterfaceByName(m, i->ifinfo.ifname, AF_INET6) == i)
							{
							LogInfo("SetupActiveInterfaces: %5s(%lu) Doing precautionary IPV6_LEAVE_GROUP for %.16a on %u", i->ifinfo.ifname, i->scope_id, &i6mr.ipv6mr_multiaddr, i6mr.ipv6mr_interface);
							mStatus err = setsockopt(m->p->permanentsockets.sktv6, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &i6mr, sizeof(i6mr));
							if (err < 0 && (errno != EADDRNOTAVAIL))
								LogMsg("setsockopt - IPV6_LEAVE_GROUP error %d errno %d (%s) group %.16a on %u", err, errno, strerror(errno), &i6mr.ipv6mr_multiaddr, i6mr.ipv6mr_interface);
							}
	
						LogInfo("SetupActiveInterfaces: %5s(%lu) joining IPv6 mcast group %.16a on %u", i->ifinfo.ifname, i->scope_id, &i6mr.ipv6mr_multiaddr, i6mr.ipv6mr_interface);
						mStatus err = setsockopt(m->p->permanentsockets.sktv6, IPPROTO_IPV6, IPV6_JOIN_GROUP, &i6mr, sizeof(i6mr));
						// Joining same group twice can give "Address already in use" error -- no need to report that
						if (err < 0 && (errno != EADDRINUSE))
							LogMsg("setsockopt - IPV6_JOIN_GROUP error %d errno %d (%s) group %.16a on %u", err, errno, strerror(errno), &i6mr.ipv6mr_multiaddr, i6mr.ipv6mr_interface);
						}
#endif
					}
				}
			}

	return count;
	}

mDNSlocal void MarkAllInterfacesInactive(mDNS *const m, mDNSs32 utc)
	{
	NetworkInterfaceInfoOSX *i;
	for (i = m->p->InterfaceList; i; i = i->next)
		{
		if (i->Exists) i->LastSeen = utc;
		i->Exists = mDNSfalse;
		}
	}

// returns count of non-link local V4 addresses deregistered
mDNSlocal int ClearInactiveInterfaces(mDNS *const m, mDNSs32 utc)
	{
	// First pass:
	// If an interface is going away, then deregister this from the mDNSCore.
	// We also have to deregister it if the primary interface that it's using for its InterfaceID is going away.
	// We have to do this because mDNSCore will use that InterfaceID when sending packets, and if the memory
	// it refers to has gone away we'll crash.
	NetworkInterfaceInfoOSX *i;
	int count = 0;
	for (i = m->p->InterfaceList; i; i = i->next)
		{
		// If this interface is no longer active, or its InterfaceID is changing, deregister it
		NetworkInterfaceInfoOSX *primary = SearchForInterfaceByName(m, i->ifinfo.ifname, AAAA_OVER_V4 ? AF_UNSPEC : i->sa_family);
		if (i->Registered)
			if (i->Exists == 0 || i->Exists == 2 || i->Registered != primary)
				{
				i->Flashing = !(i->ifa_flags & IFF_LOOPBACK) && (utc - i->AppearanceTime < 60);
				LogInfo("ClearInactiveInterfaces: Deregistering %5s(%lu) %.6a InterfaceID %p(%p), primary %p, %#a/%d%s%s%s",
					i->ifinfo.ifname, i->scope_id, &i->BSSID, i->ifinfo.InterfaceID, i, primary,
					&i->ifinfo.ip, CountMaskBits(&i->ifinfo.mask),
					i->Flashing               ? " (Flashing)"  : "",
					i->Occulting              ? " (Occulting)" : "",
					i->ifinfo.InterfaceActive ? " (Primary)"   : "");
				mDNS_DeregisterInterface(m, &i->ifinfo, i->Flashing && i->Occulting);
				if (!mDNSAddressIsLinkLocal(&i->ifinfo.ip)) count++;
				i->Registered = mDNSNULL;
				// Note: If i->Registered is set, that means we've called mDNS_RegisterInterface() for this interface,
				// so we need to make sure we call mDNS_DeregisterInterface() before disposing it.
				// If i->Registered is NOT set, then it's not registered and we should not call mDNS_DeregisterInterface() on it.

				// Caution: If we ever decide to add code here to leave the multicast group, we need to make sure that this
				// is the LAST representative of this physical interface, or we'll unsubscribe from the group prematurely.
				}
		}

	// Second pass:
	// Now that everything that's going to deregister has done so, we can clean up and free the memory
	NetworkInterfaceInfoOSX **p = &m->p->InterfaceList;
	while (*p)
		{
		i = *p;
		// If no longer active, delete interface from list and free memory
		if (!i->Exists)
			{
			if (i->LastSeen == utc) i->LastSeen = utc - 1;
			mDNSBool delete = (NumCacheRecordsForInterfaceID(m, i->ifinfo.InterfaceID) == 0) && (utc - i->LastSeen >= 60);
			LogInfo("ClearInactiveInterfaces: %-13s %5s(%lu) %.6a InterfaceID %p(%p) %#a/%d Age %d%s", delete ? "Deleting" : "Holding",
				i->ifinfo.ifname, i->scope_id, &i->BSSID, i->ifinfo.InterfaceID, i,
				&i->ifinfo.ip, CountMaskBits(&i->ifinfo.mask), utc - i->LastSeen,
				i->ifinfo.InterfaceActive ? " (Primary)" : "");
#if APPLE_OSX_mDNSResponder
			if (i->BPF_fd >= 0) CloseBPF(i);
#endif // APPLE_OSX_mDNSResponder
			if (delete)
				{
				*p = i->next;
				freeL("NetworkInterfaceInfoOSX", i);
				continue;	// After deleting this object, don't want to do the "p = &i->next;" thing at the end of the loop
				}
			}
		p = &i->next;
		}
	return count;
	}

mDNSlocal void AppendDNameListElem(DNameListElem ***List, mDNSu32 uid, domainname *name)
	{
	DNameListElem *dnle = (DNameListElem*) mallocL("DNameListElem/AppendDNameListElem", sizeof(DNameListElem));
	if (!dnle) LogMsg("ERROR: AppendDNameListElem: memory exhausted");
	else
		{
		dnle->next = mDNSNULL;
		dnle->uid  = uid;
		AssignDomainName(&dnle->name, name);
		**List = dnle;
		*List = &dnle->next;
		}
	}

mDNSlocal int compare_dns_configs(const void *aa, const void *bb)
	{
	dns_resolver_t *a = *(dns_resolver_t**)aa;
	dns_resolver_t *b = *(dns_resolver_t**)bb;
	
	return (a->search_order < b->search_order) ? -1 : (a->search_order == b->search_order) ? 0 : 1;
	}

mDNSexport void mDNSPlatformSetDNSConfig(mDNS *const m, mDNSBool setservers, mDNSBool setsearch, domainname *const fqdn, DNameListElem **RegDomains, DNameListElem **BrowseDomains)
	{
	int i;
	char buf[MAX_ESCAPED_DOMAIN_NAME];	// Max legal C-string name, including terminating NUL
	domainname d;

	// Need to set these here because we need to do this even if SCDynamicStoreCreate() or SCDynamicStoreCopyValue() below don't succeed
	if (fqdn)          fqdn->c[0]      = 0;
	if (RegDomains   ) *RegDomains     = NULL;
	if (BrowseDomains) *BrowseDomains  = NULL;

	LogInfo("mDNSPlatformSetDNSConfig:%s%s%s%s%s",
	        setservers    ? " setservers"    : "",
	        setsearch     ? " setsearch"     : "",
	        fqdn          ? " fqdn"          : "",
	        RegDomains    ? " RegDomains"    : "",
	        BrowseDomains ? " BrowseDomains" : "");

	// Add the inferred address-based configuration discovery domains
	// (should really be in core code I think, not platform-specific)
	if (setsearch)
		{
		struct ifaddrs *ifa = mDNSNULL;
		struct sockaddr_in saddr;
		mDNSPlatformMemZero(&saddr, sizeof(saddr));
		saddr.sin_len = sizeof(saddr);
		saddr.sin_family = AF_INET;
		saddr.sin_port = 0;
		saddr.sin_addr.s_addr = *(in_addr_t *)&m->Router.ip.v4;

		// Don't add any reverse-IP search domains if doing the WAB bootstrap queries would cause dial-on-demand connection initiation
		if (!AddrRequiresPPPConnection((struct sockaddr *)&saddr)) ifa =  myGetIfAddrs(1);
		
		while (ifa)
			{
			mDNSAddr a, n;
			if (ifa->ifa_addr->sa_family == AF_INET	&&
				ifa->ifa_netmask                    &&
				!(ifa->ifa_flags & IFF_LOOPBACK)	&&
				!SetupAddr(&a, ifa->ifa_addr)		&&
				!mDNSv4AddressIsLinkLocal(&a.ip.v4)  )
				{
				// Apparently it's normal for the sa_family of an ifa_netmask to sometimes be incorrect, so we explicitly fix it here before calling SetupAddr
				// <rdar://problem/5492035> getifaddrs is returning invalid netmask family for fw0 and vmnet
				ifa->ifa_netmask->sa_family = ifa->ifa_addr->sa_family;		// Make sure ifa_netmask->sa_family is set correctly
				SetupAddr(&n, ifa->ifa_netmask);
				// Note: This is reverse order compared to a normal dotted-decimal IP address, so we can't use our customary "%.4a" format code
				mDNS_snprintf(buf, sizeof(buf), "%d.%d.%d.%d.in-addr.arpa.", a.ip.v4.b[3] & n.ip.v4.b[3],
																			 a.ip.v4.b[2] & n.ip.v4.b[2],
																			 a.ip.v4.b[1] & n.ip.v4.b[1],
																			 a.ip.v4.b[0] & n.ip.v4.b[0]);
				mDNS_AddSearchDomain_CString(buf);
				}
			ifa = ifa->ifa_next;
			}
		}

#ifndef MDNS_NO_DNSINFO
	if (setservers || setsearch)
		{
		dns_config_t *config = dns_configuration_copy();
		if (!config)
			{
			// When running on 10.3 (build 7xxx) and earlier, we don't expect dns_configuration_copy() to succeed
			// On 10.4, calls to dns_configuration_copy() early in the boot process often fail.
			// Apparently this is expected behaviour -- "not a bug".
			// Accordingly, we suppress syslog messages for the first three minutes after boot.
			// If we are still getting failures after three minutes, then we log them.
			if (OSXVers > OSXVers_10_3_Panther && (mDNSu32)mDNSPlatformRawTime() > (mDNSu32)(mDNSPlatformOneSecond * 180))
				LogMsg("mDNSPlatformSetDNSConfig: Error: dns_configuration_copy returned NULL");
			}
		else
			{
			LogInfo("mDNSPlatformSetDNSConfig: config->n_resolver = %d", config->n_resolver);
			if (setsearch && config->n_resolver)
				{
				// Due to the vagaries of Apple's SystemConfiguration and dnsinfo.h APIs, if there are no search domains
				// listed, then you're supposed to interpret the "domain" field as also being the search domain, but if
				// there *are* search domains listed, then you're supposed to ignore the "domain" field completely and
				// instead use the search domain list as the sole authority for what domains to search and in what order
				// (and the domain from the "domain" field will also appear somewhere in that list).
				// Also, all search domains get added to the search list for resolver[0], so the domains and/or
				// search lists for other resolvers in the list need to be ignored.
				if (config->resolver[0]->n_search == 0) mDNS_AddSearchDomain_CString(config->resolver[0]->domain);
				else for (i = 0; i < config->resolver[0]->n_search; i++) mDNS_AddSearchDomain_CString(config->resolver[0]->search[i]);
				}

#if APPLE_OSX_mDNSResponder
				// Record the so-called "primary" domain, which we use as a hint to tell if the user is on a network set up
				// by someone using Microsoft Active Directory using "local" as a private internal top-level domain
				if (config->n_resolver && config->resolver[0]->domain && config->resolver[0]->n_nameserver && config->resolver[0]->nameserver[0])
					MakeDomainNameFromDNSNameString(&ActiveDirectoryPrimaryDomain, config->resolver[0]->domain);
				else ActiveDirectoryPrimaryDomain.c[0] = 0;
				//MakeDomainNameFromDNSNameString(&ActiveDirectoryPrimaryDomain, "test.local");
				ActiveDirectoryPrimaryDomainLabelCount = CountLabels(&ActiveDirectoryPrimaryDomain);
				if (config->n_resolver && config->resolver[0]->n_nameserver && SameDomainName(SkipLeadingLabels(&ActiveDirectoryPrimaryDomain, ActiveDirectoryPrimaryDomainLabelCount - 1), &localdomain))
					SetupAddr(&ActiveDirectoryPrimaryDomainServer, config->resolver[0]->nameserver[0]);
				else
					{
					AssignDomainName(&ActiveDirectoryPrimaryDomain, (const domainname *)"");
					ActiveDirectoryPrimaryDomainLabelCount = 0;
					ActiveDirectoryPrimaryDomainServer = zeroAddr;
					}
#endif

			if (setservers)
				{
				// For the "default" resolver ("resolver #1") the "domain" value is bogus and we need to ignore it.
				// e.g. the default resolver's "domain" value might say "apple.com", which indicates that this resolver
				// is only for names that fall under "apple.com", but that's not correct. Actually the default resolver is
				// for all names not covered by a more specific resolver (i.e. its domain should be ".", the root domain).
				if (config->n_resolver && config->resolver[0]->domain)
					config->resolver[0]->domain[0] = 0; // don't stop pointing at the memory, just change the first byte
				
				qsort(config->resolver, config->n_resolver, sizeof(dns_resolver_t*), compare_dns_configs);
				
				for (i = 0; i < config->n_resolver; i++)
					{
					int n;
					dns_resolver_t *r = config->resolver[i];
					mDNSInterfaceID interface = mDNSInterface_Any;
					int disabled = 0;
					
					// On Tiger, dnsinfo entries for mDNS domains have port 5353, the mDNS port.  Ignore them.
					// Note: Unlike the BSD Sockets APIs (where TCP and UDP port numbers are universally in network byte order)
					// in Apple's "dnsinfo.h" API the port number is declared to be a "uint16_t in host byte order"
					// We also don't need to do any more work if there are no nameserver addresses
					if (r->port == 5353 || r->n_nameserver == 0) continue;
					
					if (!r->domain || !*r->domain) d.c[0] = 0;
					else if (!MakeDomainNameFromDNSNameString(&d, r->domain)) { LogMsg("mDNSPlatformSetDNSConfig: bad domain %s", r->domain); continue; }

					// DNS server option parsing
					if (r->options != NULL)
						{
						char *nextOption = r->options;
						char *currentOption = NULL;
						while ((currentOption = strsep(&nextOption, " ")) != NULL && currentOption[0] != 0)
							{
							// The option may be in the form of interface=xxx where xxx is an interface name.
							if (strncmp(currentOption, kInterfaceSpecificOption, sizeof(kInterfaceSpecificOption) - 1) == 0)
								{
								NetworkInterfaceInfoOSX *ni;
								char	ifname[IF_NAMESIZE+1];
								mDNSu32	ifindex = 0;
								// If something goes wrong finding the interface, create the server entry anyhow but mark it as disabled.
								// This allows us to block these special queries from going out on the wire.
								strlcpy(ifname, currentOption + sizeof(kInterfaceSpecificOption)-1, sizeof(ifname));
								ifindex = if_nametoindex(ifname);
								if (ifindex == 0) { disabled = 1; LogMsg("RegisterSplitDNS: interfaceSpecific - interface %s not found", ifname); continue; }
								LogInfo("%s: Interface-specific entry: %s on %s (%d)", __FUNCTION__, r->domain, ifname, ifindex);
								// Find the interface, can't use mDNSPlatformInterfaceIDFromInterfaceIndex
								// because that will call mDNSMacOSXNetworkChanged if the interface doesn't exist
								for (ni = m->p->InterfaceList; ni; ni = ni->next)
									if (ni->ifinfo.InterfaceID && ni->scope_id == ifindex) break;
								if (ni != NULL) interface = ni->ifinfo.InterfaceID;
								if (interface == mDNSNULL) { disabled = 1; LogMsg("RegisterSplitDNS: interfaceSpecific - index %d (%s) not found", ifindex, ifname); continue; }
								}
							}
						}

					for (n = 0; n < r->n_nameserver; n++)
						if (r->nameserver[n]->sa_family == AF_INET || r->nameserver[n]->sa_family == AF_INET6)
							{
							mDNSAddr saddr;
							// mDNSAddr saddr = { mDNSAddrType_IPv4, { { { 192, 168, 1, 1 } } } }; // for testing
							if (SetupAddr(&saddr, r->nameserver[n])) LogMsg("RegisterSplitDNS: bad IP address");
							else
								{
								DNSServer *s = mDNS_AddDNSServer(m, &d, interface, &saddr, r->port ? mDNSOpaque16fromIntVal(r->port) : UnicastDNSPort);
								if (s)
									{
									if (disabled) s->teststate = DNSServer_Disabled;
									LogInfo("Added dns server %#a:%d for domain %##s from slot %d,%d", &s->addr, mDNSVal16(s->port), d.c, i, n);
									}
								}
							}
						
					}
				}

			dns_configuration_free(config);
			setservers = mDNSfalse;  // Done these now -- no need to fetch the same data from SCDynamicStore
			setsearch  = mDNSfalse;
			}
		}
#endif // MDNS_NO_DNSINFO

	SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("mDNSResponder:mDNSPlatformSetDNSConfig"), NULL, NULL);
	if (!store)
		LogMsg("mDNSPlatformSetDNSConfig: SCDynamicStoreCreate failed: %s", SCErrorString(SCError()));
	else
		{
		CFDictionaryRef ddnsdict = SCDynamicStoreCopyValue(store, NetworkChangedKey_DynamicDNS);
		if (ddnsdict)
			{
			if (fqdn)
				{
				CFArrayRef fqdnArray = CFDictionaryGetValue(ddnsdict, CFSTR("HostNames"));
				if (fqdnArray && CFArrayGetCount(fqdnArray) > 0)
					{
					// for now, we only look at the first array element.  if we ever support multiple configurations, we will walk the list
					CFDictionaryRef fqdnDict = CFArrayGetValueAtIndex(fqdnArray, 0);
					if (fqdnDict && DictionaryIsEnabled(fqdnDict))
						{
						CFStringRef name = CFDictionaryGetValue(fqdnDict, CFSTR("Domain"));
						if (name)
							{
							if (!CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8) ||
								!MakeDomainNameFromDNSNameString(fqdn, buf) || !fqdn->c[0])
								LogMsg("GetUserSpecifiedDDNSConfig SCDynamicStore bad DDNS host name: %s", buf[0] ? buf : "(unknown)");
							else debugf("GetUserSpecifiedDDNSConfig SCDynamicStore DDNS host name: %s", buf);
							}
						}
					}
				}

			if (RegDomains)
				{
				CFArrayRef regArray = CFDictionaryGetValue(ddnsdict, CFSTR("RegistrationDomains"));
				if (regArray && CFArrayGetCount(regArray) > 0)
					{
					CFDictionaryRef regDict = CFArrayGetValueAtIndex(regArray, 0);
					if (regDict && DictionaryIsEnabled(regDict))
						{
						CFStringRef name = CFDictionaryGetValue(regDict, CFSTR("Domain"));
						if (name)
							{
							if (!CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8) ||
								!MakeDomainNameFromDNSNameString(&d, buf) || !d.c[0])
								LogMsg("GetUserSpecifiedDDNSConfig SCDynamicStore bad DDNS registration domain: %s", buf[0] ? buf : "(unknown)");
							else
								{
								debugf("GetUserSpecifiedDDNSConfig SCDynamicStore DDNS registration domain: %s", buf);
								AppendDNameListElem(&RegDomains, 0, &d);
								}
							}
						}
					}
				}

			if (BrowseDomains)
				{
				CFArrayRef browseArray = CFDictionaryGetValue(ddnsdict, CFSTR("BrowseDomains"));
				if (browseArray)
					{
					for (i = 0; i < CFArrayGetCount(browseArray); i++)
						{
						CFDictionaryRef browseDict = CFArrayGetValueAtIndex(browseArray, i);
						if (browseDict && DictionaryIsEnabled(browseDict))
							{
							CFStringRef name = CFDictionaryGetValue(browseDict, CFSTR("Domain"));
							if (name)
								{
								if (!CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8) ||
									!MakeDomainNameFromDNSNameString(&d, buf) || !d.c[0])
									LogMsg("GetUserSpecifiedDDNSConfig SCDynamicStore bad DDNS browsing domain: %s", buf[0] ? buf : "(unknown)");
								else
									{
									debugf("GetUserSpecifiedDDNSConfig SCDynamicStore DDNS browsing domain: %s", buf);
									AppendDNameListElem(&BrowseDomains, 0, &d);
									}
								}
							}
						}
					}
				}
			CFRelease(ddnsdict);
			}

		if (RegDomains)
			{
			CFDictionaryRef btmm = SCDynamicStoreCopyValue(store, NetworkChangedKey_BackToMyMac);
			if (btmm)
				{
				CFIndex size = CFDictionaryGetCount(btmm);
				const void *key[size];
				const void *val[size];
				CFDictionaryGetKeysAndValues(btmm, key, val);
				for (i = 0; i < size; i++)
					{
					LogInfo("BackToMyMac %d", i);
					if (!CFStringGetCString(key[i], buf, sizeof(buf), kCFStringEncodingUTF8))
						LogMsg("Can't read BackToMyMac %d key %s", i, buf);
					else
						{
						mDNSu32 uid = atoi(buf);
						if (!CFStringGetCString(val[i], buf, sizeof(buf), kCFStringEncodingUTF8))
							LogMsg("Can't read BackToMyMac %d val %s", i, buf);
						else if (MakeDomainNameFromDNSNameString(&d, buf) && d.c[0])
							{
							LogInfo("BackToMyMac %d %d %##s", i, uid, d.c);
							AppendDNameListElem(&RegDomains, uid, &d);
							}
						}
					}
				CFRelease(btmm);
				}
			}

		if (setservers || setsearch)
			{
			CFDictionaryRef dict = SCDynamicStoreCopyValue(store, NetworkChangedKey_DNS);
			if (dict)
				{
				if (setservers)
					{
					CFArrayRef values = CFDictionaryGetValue(dict, kSCPropNetDNSServerAddresses);
					if (values)
						{
						LogInfo("DNS Server Address values: %d", CFArrayGetCount(values));
						for (i = 0; i < CFArrayGetCount(values); i++)
							{
							CFStringRef s = CFArrayGetValueAtIndex(values, i);
							mDNSAddr addr = { mDNSAddrType_IPv4, { { { 0 } } } };
							if (s && CFStringGetCString(s, buf, 256, kCFStringEncodingUTF8) &&
								inet_aton(buf, (struct in_addr *) &addr.ip.v4))
								{
								LogInfo("Adding DNS server from dict: %s", buf);
								mDNS_AddDNSServer(m, mDNSNULL, mDNSInterface_Any, &addr, UnicastDNSPort);
								}
							}
						}
					else LogInfo("No DNS Server Address values");
					}
				if (setsearch)
					{
					// Add the manual and/or DHCP-dicovered search domains
					CFArrayRef searchDomains = CFDictionaryGetValue(dict, kSCPropNetDNSSearchDomains);
					if (searchDomains)
						{
						for (i = 0; i < CFArrayGetCount(searchDomains); i++)
							{
							CFStringRef s = CFArrayGetValueAtIndex(searchDomains, i);
							if (s && CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8))
								mDNS_AddSearchDomain_CString(buf);
							}
						}
					else	// No kSCPropNetDNSSearchDomains, so use kSCPropNetDNSDomainName
						{
						// Due to the vagaries of Apple's SystemConfiguration and dnsinfo.h APIs, if there are no search domains
						// listed, then you're supposed to interpret the "domain" field as also being the search domain, but if
						// there *are* search domains listed, then you're supposed to ignore the "domain" field completely and
						// instead use the search domain list as the sole authority for what domains to search and in what order
						// (and the domain from the "domain" field will also appear somewhere in that list).
						CFStringRef string = CFDictionaryGetValue(dict, kSCPropNetDNSDomainName);
						if (string && CFStringGetCString(string, buf, sizeof(buf), kCFStringEncodingUTF8))
							mDNS_AddSearchDomain_CString(buf);
						}
					}
				CFRelease(dict);
				}
			}
		CFRelease(store);
		}
	}

mDNSexport mStatus mDNSPlatformGetPrimaryInterface(mDNS *const m, mDNSAddr *v4, mDNSAddr *v6, mDNSAddr *r)
	{
	char				buf[256];
	mStatus				err		= 0;
	(void)m; // Unused

	SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("mDNSResponder:mDNSPlatformGetPrimaryInterface"), NULL, NULL);
	if (!store)
		LogMsg("mDNSPlatformGetPrimaryInterface: SCDynamicStoreCreate failed: %s", SCErrorString(SCError()));
	else
		{
		CFDictionaryRef dict = SCDynamicStoreCopyValue(store, NetworkChangedKey_IPv4);
		if (dict)
			{
			r->type  = mDNSAddrType_IPv4;
			r->ip.v4 = zerov4Addr;
			CFStringRef string = CFDictionaryGetValue(dict, kSCPropNetIPv4Router);
			if (string)
				{
				if (!CFStringGetCString(string, buf, 256, kCFStringEncodingUTF8))
					LogMsg("Could not convert router to CString");
				else
					{
					struct sockaddr_in saddr;
					saddr.sin_len = sizeof(saddr);
					saddr.sin_family = AF_INET;
					saddr.sin_port = 0;
					inet_aton(buf, &saddr.sin_addr);
		
					*(in_addr_t *)&r->ip.v4 = saddr.sin_addr.s_addr;
					}
				}
		
			string = CFDictionaryGetValue(dict, kSCDynamicStorePropNetPrimaryInterface);
			if (string)
				{
				mDNSBool HavePrimaryGlobalv6 = mDNSfalse;  // does the primary interface have a global v6 address?
				struct ifaddrs *ifa = myGetIfAddrs(1);
		
				*v4 = *v6 = zeroAddr;
		
				if (!CFStringGetCString(string, buf, 256, kCFStringEncodingUTF8)) { LogMsg("Could not convert router to CString"); goto exit; }
		
				// find primary interface in list
				while (ifa && (mDNSIPv4AddressIsZero(v4->ip.v4) || mDNSv4AddressIsLinkLocal(&v4->ip.v4) || !HavePrimaryGlobalv6))
					{
					mDNSAddr tmp6 = zeroAddr;
					if (!strcmp(buf, ifa->ifa_name))
						{
						if (ifa->ifa_addr->sa_family == AF_INET)
							{
							if (mDNSIPv4AddressIsZero(v4->ip.v4) || mDNSv4AddressIsLinkLocal(&v4->ip.v4)) SetupAddr(v4, ifa->ifa_addr);
							}
						else if (ifa->ifa_addr->sa_family == AF_INET6)
							{
							SetupAddr(&tmp6, ifa->ifa_addr);
							if (tmp6.ip.v6.b[0] >> 5 == 1)   // global prefix: 001
								{ HavePrimaryGlobalv6 = mDNStrue; *v6 = tmp6; }
							}
						}
					else
						{
						// We'll take a V6 address from the non-primary interface if the primary interface doesn't have a global V6 address
						if (!HavePrimaryGlobalv6 && ifa->ifa_addr->sa_family == AF_INET6 && !v6->ip.v6.b[0])
							{
							SetupAddr(&tmp6, ifa->ifa_addr);
							if (tmp6.ip.v6.b[0] >> 5 == 1) *v6 = tmp6;
							}
						}
					ifa = ifa->ifa_next;
					}
		
				// Note that while we advertise v6, we still require v4 (possibly NAT'd, but not link-local) because we must use
				// V4 to communicate w/ our DNS server
				}
		
			exit:
			CFRelease(dict);
			}
		CFRelease(store);
		}
	return err;
	}

mDNSexport void mDNSPlatformDynDNSHostNameStatusChanged(const domainname *const dname, const mStatus status)
	{
	LogInfo("mDNSPlatformDynDNSHostNameStatusChanged %d %##s", status, dname->c);
	char uname[MAX_ESCAPED_DOMAIN_NAME];	// Max legal C-string name, including terminating NUL
	ConvertDomainNameToCString(dname, uname);

	char *p = uname;
	while (*p)
		{
		*p = tolower(*p);
		if (!(*(p+1)) && *p == '.') *p = 0; // if last character, strip trailing dot
		p++;
		}

	// We need to make a CFDictionary called "State:/Network/DynamicDNS" containing (at present) a single entity.
	// That single entity is a CFDictionary with name "HostNames".
	// The "HostNames" CFDictionary contains a set of name/value pairs, where the each name is the FQDN
	// in question, and the corresponding value is a CFDictionary giving the state for that FQDN.
	// (At present we only support a single FQDN, so this dictionary holds just a single name/value pair.)
	// The CFDictionary for each FQDN holds (at present) a single name/value pair,
	// where the name is "Status" and the value is a CFNumber giving an errror code (with zero meaning success).

	const CFStringRef StateKeys [1] = { CFSTR("HostNames") };
	const CFStringRef HostKeys  [1] = { CFStringCreateWithCString(NULL, uname, kCFStringEncodingUTF8) };
	const CFStringRef StatusKeys[1] = { CFSTR("Status") };
	if (!HostKeys[0]) LogMsg("SetDDNSNameStatus: CFStringCreateWithCString(%s) failed", uname);
	else
		{
		const CFNumberRef StatusVals[1] = { CFNumberCreate(NULL, kCFNumberSInt32Type, &status) };
		if (!StatusVals[0]) LogMsg("SetDDNSNameStatus: CFNumberCreate(%d) failed", status);
		else
			{
			const CFDictionaryRef HostVals[1] = { CFDictionaryCreate(NULL, (void*)StatusKeys, (void*)StatusVals, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks) };
			if (HostVals[0])
				{
				const CFDictionaryRef StateVals[1] = { CFDictionaryCreate(NULL, (void*)HostKeys, (void*)HostVals, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks) };
				if (StateVals[0])
					{
					CFDictionaryRef StateDict = CFDictionaryCreate(NULL, (void*)StateKeys, (void*)StateVals, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
					if (StateDict)
						{
						mDNSDynamicStoreSetConfig(kmDNSDynamicConfig, mDNSNULL, StateDict);
						CFRelease(StateDict);
						}
					CFRelease(StateVals[0]);
					}
				CFRelease(HostVals[0]);
				}
			CFRelease(StatusVals[0]);
			}
		CFRelease(HostKeys[0]);
		}
	}

// MUST be called holding the lock -- this routine calls SetupLocalAutoTunnelInterface_internal()
mDNSexport void SetDomainSecrets(mDNS *m)
	{
#ifdef NO_SECURITYFRAMEWORK
	(void)m;
	LogMsg("Note: SetDomainSecrets: no keychain support");
#else
	mDNSBool	haveAutoTunnels = mDNSfalse;

	LogInfo("SetDomainSecrets");

	// Rather than immediately deleting all keys now, we mark them for deletion in ten seconds.
	// In the case where the user simultaneously removes their DDNS host name and the key
	// for it, this gives mDNSResponder ten seconds to gracefully delete the name from the
	// server before it loses access to the necessary key. Otherwise, we'd leave orphaned
	// address records behind that we no longer have permission to delete.
	DomainAuthInfo *ptr;
	for (ptr = m->AuthInfoList; ptr; ptr = ptr->next)
		ptr->deltime = NonZeroTime(m->timenow + mDNSPlatformOneSecond*10);

#if APPLE_OSX_mDNSResponder
	{
	// Mark all TunnelClients for deletion
	ClientTunnel *client;
	for (client = m->TunnelClients; client; client = client->next)
		{
		LogInfo("SetDomainSecrets: tunnel to %##s marked for deletion", client->dstname.c);
		client->MarkedForDeletion = mDNStrue;
		}
	}
#endif // APPLE_OSX_mDNSResponder

	// String Array used to write list of private domains to Dynamic Store
	CFMutableArrayRef sa = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (!sa) { LogMsg("SetDomainSecrets: CFArrayCreateMutable failed"); return; }
	CFIndex i;
	CFDataRef data = NULL;
	const int itemsPerEntry = 3; // domain name, key name, key value
	CFArrayRef secrets = NULL;
	int err = mDNSKeychainGetSecrets(&secrets);
	if (err || !secrets)
		LogMsg("SetDomainSecrets: mDNSKeychainGetSecrets failed error %d CFArrayRef %p", err, secrets);
	else
		{
		CFIndex ArrayCount = CFArrayGetCount(secrets);
		// Iterate through the secrets
		for (i = 0; i < ArrayCount; ++i)
			{
			int j;
			CFArrayRef entry = CFArrayGetValueAtIndex(secrets, i);
			if (CFArrayGetTypeID() != CFGetTypeID(entry) || itemsPerEntry != CFArrayGetCount(entry))
				{ LogMsg("SetDomainSecrets: malformed entry"); continue; }
			for (j = 0; j < CFArrayGetCount(entry); ++j)
				if (CFDataGetTypeID() != CFGetTypeID(CFArrayGetValueAtIndex(entry, j)))
					{ LogMsg("SetDomainSecrets: malformed entry item"); continue; }

			// The names have already been vetted by the helper, but checking them again here helps humans and automated tools verify correctness

			// Get DNS domain this key is for
			char stringbuf[MAX_ESCAPED_DOMAIN_NAME];	// Max legal domainname as C-string, including terminating NUL
			data = CFArrayGetValueAtIndex(entry, 0);
			if (CFDataGetLength(data) >= (int)sizeof(stringbuf))
				{ LogMsg("SetDomainSecrets: Bad kSecServiceItemAttr length %d", CFDataGetLength(data)); continue; }
			CFDataGetBytes(data, CFRangeMake(0, CFDataGetLength(data)), (UInt8 *)stringbuf);
			stringbuf[CFDataGetLength(data)] = '\0';

			domainname domain;
			if (!MakeDomainNameFromDNSNameString(&domain, stringbuf)) { LogMsg("SetDomainSecrets: bad key domain %s", stringbuf); continue; }

			// Get key name
			data = CFArrayGetValueAtIndex(entry, 1);
			if (CFDataGetLength(data) >= (int)sizeof(stringbuf))
				{ LogMsg("SetDomainSecrets: Bad kSecAccountItemAttr length %d", CFDataGetLength(data)); continue; }
			CFDataGetBytes(data, CFRangeMake(0,CFDataGetLength(data)), (UInt8 *)stringbuf);
			stringbuf[CFDataGetLength(data)] = '\0';

			domainname keyname;
			if (!MakeDomainNameFromDNSNameString(&keyname, stringbuf)) { LogMsg("SetDomainSecrets: bad key name %s", stringbuf); continue; }

			// Get key data
			data = CFArrayGetValueAtIndex(entry, 2);
			if (CFDataGetLength(data) >= (int)sizeof(stringbuf))
				{ LogMsg("SetDomainSecrets: Shared secret too long: %d", CFDataGetLength(data)); continue; }
			CFDataGetBytes(data, CFRangeMake(0, CFDataGetLength(data)), (UInt8 *)stringbuf);
			stringbuf[CFDataGetLength(data)] = '\0';	// mDNS_SetSecretForDomain requires NULL-terminated C string for key

			DomainAuthInfo *FoundInList;
			for (FoundInList = m->AuthInfoList; FoundInList; FoundInList = FoundInList->next)
				if (SameDomainName(&FoundInList->domain, &domain)) break;

#if APPLE_OSX_mDNSResponder
			if (FoundInList)
				{
				// If any client tunnel destination is in this domain, set deletion flag to false
				ClientTunnel *client;
				for (client = m->TunnelClients; client; client = client->next)
					if (FoundInList == GetAuthInfoForName_internal(m, &client->dstname))
					{
					LogInfo("SetDomainSecrets: tunnel to %##s no longer marked for deletion", client->dstname.c);
					client->MarkedForDeletion = mDNSfalse;
					}
				}

#endif // APPLE_OSX_mDNSResponder

			// Uncomment the line below to view the keys as they're read out of the system keychain
			// DO NOT SHIP CODE THIS WAY OR YOU'LL LEAK SECRET DATA INTO A PUBLICLY READABLE FILE!
			//LogInfo("SetDomainSecrets: %##s %##s %s", &domain.c, &keyname.c, stringbuf);

			// If didn't find desired domain in the list, make a new entry
			ptr = FoundInList;
			if (FoundInList && FoundInList->AutoTunnel && haveAutoTunnels == mDNSfalse) haveAutoTunnels = mDNStrue;
			if (!FoundInList)
				{
				ptr = (DomainAuthInfo*)mallocL("DomainAuthInfo", sizeof(*ptr));
				if (!ptr) { LogMsg("SetDomainSecrets: No memory"); continue; }
				}

			LogInfo("SetDomainSecrets: %d of %d %##s", i, ArrayCount, &domain);
			if (mDNS_SetSecretForDomain(m, ptr, &domain, &keyname, stringbuf, IsTunnelModeDomain(&domain)) == mStatus_BadParamErr)
				{
				if (!FoundInList) mDNSPlatformMemFree(ptr);		// If we made a new DomainAuthInfo here, and it turned out bad, dispose it immediately
				continue;
				}

#if APPLE_OSX_mDNSResponder
			if (ptr->AutoTunnel) UpdateAutoTunnelDomainStatus(m, ptr);
#endif // APPLE_OSX_mDNSResponder

			ConvertDomainNameToCString(&domain, stringbuf);
			CFStringRef cfs = CFStringCreateWithCString(NULL, stringbuf, kCFStringEncodingUTF8);
			if (cfs) { CFArrayAppendValue(sa, cfs); CFRelease(cfs); }
			}
		CFRelease(secrets);
		}
	mDNSDynamicStoreSetConfig(kmDNSPrivateConfig, mDNSNULL, sa);
	CFRelease(sa);

#if APPLE_OSX_mDNSResponder
		{
		// clean up ClientTunnels
		ClientTunnel **pp = &m->TunnelClients;
		while (*pp)
			{
			if ((*pp)->MarkedForDeletion)
				{
				ClientTunnel *cur = *pp;
				LogInfo("SetDomainSecrets: removing client %p %##s from list", cur, cur->dstname.c);
				if (cur->q.ThisQInterval >= 0) mDNS_StopQuery(m, &cur->q);
				AutoTunnelSetKeys(cur, mDNSfalse);
				*pp = cur->next;
				freeL("ClientTunnel", cur);
				}
			else
				pp = &(*pp)->next;
			}

		DomainAuthInfo *info = m->AuthInfoList;
		while (info)
			{
			if (info->AutoTunnel && info->deltime)
				{
				if (info->AutoTunnelNAT.clientContext)
					{
					// stop the NAT operation
					mDNS_StopNATOperation_internal(m, &info->AutoTunnelNAT);
					if (info->AutoTunnelNAT.clientCallback)
						{
						// Reset port and let the AutoTunnelNATCallback handle cleanup
						info->AutoTunnelNAT.ExternalAddress = m->ExternalAddress;
						info->AutoTunnelNAT.ExternalPort    = zeroIPPort;
						info->AutoTunnelNAT.Lifetime        = 0;
						info->AutoTunnelNAT.Result          = mStatus_NoError;
						mDNS_DropLockBeforeCallback(); // Allow client to legally make mDNS API calls from the callback
						info->AutoTunnelNAT.clientCallback(m, &info->AutoTunnelNAT);
						mDNS_ReclaimLockAfterCallback(); // Decrement mDNS_reentrancy to block mDNS API calls again
						}
					info->AutoTunnelNAT.clientContext = mDNSNULL;
					}
				RemoveAutoTunnelDomainStatus(m, info);
				}
			info = info->next;
			}

		if (!haveAutoTunnels && !m->TunnelClients && m->AutoTunnelHostAddrActive)
			{
			// remove interface if no autotunnel servers and no more client tunnels
			LogInfo("SetDomainSecrets: Bringing tunnel interface DOWN");
			m->AutoTunnelHostAddrActive = mDNSfalse;
			(void)mDNSAutoTunnelInterfaceUpDown(kmDNSDown, m->AutoTunnelHostAddr.b);
			mDNSPlatformMemZero(m->AutoTunnelHostAddr.b, sizeof(m->AutoTunnelHostAddr.b));
			}

		UpdateConfigureServer(m);
		
		if (m->AutoTunnelHostAddr.b[0])
			if (TunnelClients(m) || TunnelServers(m))
				SetupLocalAutoTunnelInterface_internal(m);
		}
#endif // APPLE_OSX_mDNSResponder

#endif /* NO_SECURITYFRAMEWORK */
	}

mDNSlocal void SetLocalDomains(void)
	{
	CFMutableArrayRef sa = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (!sa) { LogMsg("SetLocalDomains: CFArrayCreateMutable failed"); return; }

	CFArrayAppendValue(sa, CFSTR("local"));
	CFArrayAppendValue(sa, CFSTR("254.169.in-addr.arpa"));
	CFArrayAppendValue(sa, CFSTR("8.e.f.ip6.arpa"));
	CFArrayAppendValue(sa, CFSTR("9.e.f.ip6.arpa"));
	CFArrayAppendValue(sa, CFSTR("a.e.f.ip6.arpa"));
	CFArrayAppendValue(sa, CFSTR("b.e.f.ip6.arpa"));

	mDNSDynamicStoreSetConfig(kmDNSMulticastConfig, mDNSNULL, sa);
	CFRelease(sa);
	}

mDNSlocal void GetCurrentPMSetting(const CFStringRef name, mDNSs32 *val)
	{
#if USE_IOPMCOPYACTIVEPMPREFERENCES
	CFTypeRef blob = NULL;
	CFStringRef str = NULL;
	CFDictionaryRef odict = NULL;
	CFDictionaryRef idict = NULL;
	CFNumberRef number = NULL;

	blob = IOPSCopyPowerSourcesInfo();
	if (!blob) { LogMsg("GetCurrentPMSetting: IOPSCopyPowerSourcesInfo failed!"); goto end; }

	odict = IOPMCopyActivePMPreferences();
	if (!odict) { LogMsg("GetCurrentPMSetting: IOPMCopyActivePMPreferences failed!"); goto end; }

	str = IOPSGetProvidingPowerSourceType(blob);
	if (!str) { LogMsg("GetCurrentPMSetting: IOPSGetProvidingPowerSourceType failed!"); goto end; }

	idict = CFDictionaryGetValue(odict, str);
	if (!idict)
		{
		char buf[256];
		if (!CFStringGetCString(str, buf, sizeof(buf), kCFStringEncodingUTF8)) buf[0] = 0;
		LogMsg("GetCurrentPMSetting: CFDictionaryGetValue (%s) failed!", buf);
		goto end;
		}
	
	number = CFDictionaryGetValue(idict, name);
	if (!number || CFGetTypeID(number) != CFNumberGetTypeID() || !CFNumberGetValue(number, kCFNumberSInt32Type, val))
		*val = 0;
end:
	if (blob) CFRelease(blob);
	if (odict) CFRelease(odict);

#else

	SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("mDNSResponder:GetCurrentPMSetting"), NULL, NULL);
	if (!store) LogMsg("GetCurrentPMSetting: SCDynamicStoreCreate failed: %s", SCErrorString(SCError()));
	else
		{
		CFDictionaryRef dict = SCDynamicStoreCopyValue(store, CFSTR("State:/IOKit/PowerManagement/CurrentSettings"));
		if (!dict) LogSPS("GetCurrentPMSetting: Could not get IOPM CurrentSettings dict");
		else
			{
			CFNumberRef number = CFDictionaryGetValue(dict, name);
			if (!number || CFGetTypeID(number) != CFNumberGetTypeID() || !CFNumberGetValue(number, kCFNumberSInt32Type, val))
				*val = 0;
			CFRelease(dict);
			}
		CFRelease(store);
		}
	
#endif
	}

#if APPLE_OSX_mDNSResponder

static CFMutableDictionaryRef spsStatusDict = NULL;
static const CFStringRef kMetricRef = CFSTR("Metric");

mDNSlocal void SPSStatusPutNumber(CFMutableDictionaryRef dict, const mDNSu8* const ptr, CFStringRef key)
	{
	mDNSu8 tmp = (ptr[0] - '0') * 10 + ptr[1] - '0';
	CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt8Type, &tmp);
	if (!num)
		LogMsg("SPSStatusPutNumber: Could not create CFNumber");
	else
		{
		CFDictionarySetValue(dict, key, num);
		CFRelease(num);
		}
	}

mDNSlocal CFMutableDictionaryRef SPSCreateDict(const mDNSu8* const ptr)
	{
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!dict) { LogMsg("SPSCreateDict: Could not create CFDictionary dict"); return dict; }
	
	char buffer[1024];
	buffer[mDNS_snprintf(buffer, sizeof(buffer), "%##s", ptr) - 1] = 0;
	CFStringRef spsname = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
	if (!spsname) { LogMsg("SPSCreateDict: Could not create CFString spsname full"); CFRelease(dict); return NULL; }
	CFDictionarySetValue(dict, CFSTR("FullName"), spsname);
	CFRelease(spsname);

	if (ptr[0] >=  2) SPSStatusPutNumber(dict, ptr + 1, CFSTR("Type"));
	if (ptr[0] >=  5) SPSStatusPutNumber(dict, ptr + 4, CFSTR("Portability"));
	if (ptr[0] >=  8) SPSStatusPutNumber(dict, ptr + 7, CFSTR("MarginalPower"));
	if (ptr[0] >= 11) SPSStatusPutNumber(dict, ptr +10, CFSTR("TotalPower"));

	mDNSu32 tmp = SPSMetric(ptr);
	CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt32Type, &tmp);
	if (!num)
		LogMsg("SPSCreateDict: Could not create CFNumber");
	else
		{
		CFDictionarySetValue(dict, kMetricRef, num);
		CFRelease(num);
		}

	if (ptr[0] >= 12)
		{
		memcpy(buffer, ptr + 13, ptr[0] - 12);
		buffer[ptr[0] - 12] = 0;
		spsname = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
		if (!spsname) { LogMsg("SPSCreateDict: Could not create CFString spsname"); CFRelease(dict); return NULL; }
		else
			{
			CFDictionarySetValue(dict, CFSTR("PrettyName"), spsname);
			CFRelease(spsname);
			}
		}
	
	return dict;
	}
	
mDNSlocal CFComparisonResult CompareSPSEntries(const void *val1, const void *val2, void *context)
	{
	(void)context;
	return CFNumberCompare((CFNumberRef)CFDictionaryGetValue((CFDictionaryRef)val1, kMetricRef),
						   (CFNumberRef)CFDictionaryGetValue((CFDictionaryRef)val2, kMetricRef),
						   NULL);
	}

mDNSlocal void UpdateSPSStatus(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
	{
	(void)m;
	NetworkInterfaceInfo* info = (NetworkInterfaceInfo*)question->QuestionContext;
	debugf("UpdateSPSStatus: %s %##s %s %s", info->ifname, question->qname.c, AddRecord ? "Add" : "Rmv", answer ? RRDisplayString(m, answer) : "<null>");

	if (answer && SPSMetric(answer->rdata->u.name.c) > 999999) return;	// Ignore instances with invalid names

	if (!spsStatusDict)
		{
		spsStatusDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (!spsStatusDict) { LogMsg("UpdateSPSStatus: Could not create CFDictionary spsStatusDict"); return; }
		}
	
	CFStringRef ifname = CFStringCreateWithCString(NULL, info->ifname, kCFStringEncodingUTF8);
	if (!ifname) { LogMsg("UpdateSPSStatus: Could not create CFString ifname"); return; }
	
	CFMutableArrayRef array = NULL;
	
	if (!CFDictionaryGetValueIfPresent(spsStatusDict, ifname, (const void**) &array))
		{
		array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		if (!array) { LogMsg("UpdateSPSStatus: Could not create CFMutableArray"); CFRelease(ifname); return; }
		CFDictionarySetValue(spsStatusDict, ifname, array);
		CFRelease(array); // let go of our reference, now that the dict has one
		}
	else
		if (!array) { LogMsg("UpdateSPSStatus: Could not get CFMutableArray for %s", info->ifname); CFRelease(ifname); return; }
	
	if (!answer) // special call that means the question has been stopped (because the interface is going away)
		CFArrayRemoveAllValues(array);
	else
		{
		CFMutableDictionaryRef dict = SPSCreateDict(answer->rdata->u.name.c);
		if (!dict) { CFRelease(ifname); return; }
		
		if (AddRecord)
			{
			if (!CFArrayContainsValue(array, CFRangeMake(0, CFArrayGetCount(array)), dict))
				{
				int i=0;
				for (i=0; i<CFArrayGetCount(array); i++)
					if (CompareSPSEntries(CFArrayGetValueAtIndex(array, i), dict, NULL) != kCFCompareLessThan)
						break;
				CFArrayInsertValueAtIndex(array, i, dict);
				}
			else LogMsg("UpdateSPSStatus: %s array already contains %##s", info->ifname, answer->rdata->u.name.c);
			}
		else
			{
			CFIndex i = CFArrayGetFirstIndexOfValue(array, CFRangeMake(0, CFArrayGetCount(array)), dict);
			if (i != -1) CFArrayRemoveValueAtIndex(array, i);
			else LogMsg("UpdateSPSStatus: %s array does not contain %##s", info->ifname, answer->rdata->u.name.c);
			}
			
		CFRelease(dict);
		}

	if (!m->ShutdownTime) mDNSDynamicStoreSetConfig(kmDNSSleepProxyServersState, info->ifname, array);
	
	CFRelease(ifname);
	}

mDNSlocal mDNSs32 GetSystemSleepTimerSetting(void)
	{
	mDNSs32 val = -1;
	SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("mDNSResponder:GetSystemSleepTimerSetting"), NULL, NULL);
	if (!store)
		LogMsg("GetSystemSleepTimerSetting: SCDynamicStoreCreate failed: %s", SCErrorString(SCError()));
	else
		{
		CFDictionaryRef dict = SCDynamicStoreCopyValue(store, CFSTR("State:/IOKit/PowerManagement/CurrentSettings"));
		if (dict)
			{
			CFNumberRef number = CFDictionaryGetValue(dict, CFSTR("System Sleep Timer"));
			if (number) CFNumberGetValue(number, kCFNumberSInt32Type, &val);
			CFRelease(dict);
			}
		CFRelease(store);
		}
	return val;
	}

mDNSlocal void SetSPS(mDNS *const m)
	{
	SCPreferencesSynchronize(m->p->SCPrefs);
	CFDictionaryRef dict = SCPreferencesGetValue(m->p->SCPrefs, CFSTR("NAT"));
	mDNSBool natenabled = (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID()) && DictionaryIsEnabled(dict));
	mDNSu8 sps = natenabled ? 50 : (OfferSleepProxyService && GetSystemSleepTimerSetting() == 0) ? OfferSleepProxyService : 0;

	// For devices that are not running NAT, but are set to never sleep, we may choose to act
	// as a Sleep Proxy, but only for non-portable Macs (Portability > 35 means nominal weight < 3kg)
	if (sps > 50 && SPMetricPortability > 35) sps = 0;

	// If we decide to let laptops act as Sleep Proxy, we should do it only when running on AC power, not on battery

	// For devices that are unable to sleep at all to save power (e.g. the current Apple TV hardware)
	// it makes sense for them to offer low-priority Sleep Proxy service on the network.
	// We rate such a device as metric 70 ("Incidentally Available Hardware")
	if (SPMetricMarginalPower == 10 && (!sps || sps > 70)) sps = 70;

	mDNSCoreBeSleepProxyServer(m, sps, SPMetricPortability, SPMetricMarginalPower, SPMetricTotalPower);
	}

mDNSlocal void InternetSharingChanged(SCPreferencesRef prefs, SCPreferencesNotification notificationType, void *context)
	{
	(void)prefs;             // Parameter not used
	(void)notificationType;  // Parameter not used
	mDNS *const m = (mDNS *const)context;
	KQueueLock(m);
	mDNS_Lock(m);

	// Tell platform layer to open or close its BPF fds
	if (!m->p->NetworkChanged ||
		m->p->NetworkChanged - NonZeroTime(m->timenow + mDNSPlatformOneSecond * 2) < 0)
		{
		m->p->NetworkChanged = NonZeroTime(m->timenow + mDNSPlatformOneSecond * 2);
		LogInfo("InternetSharingChanged: Set NetworkChanged to %d (%d)", m->p->NetworkChanged - m->timenow, m->p->NetworkChanged);
		}

	mDNS_Unlock(m);
	KQueueUnlock(m, "InternetSharingChanged");
	}

mDNSlocal mStatus WatchForInternetSharingChanges(mDNS *const m)
	{
	SCPreferencesRef SCPrefs = SCPreferencesCreate(NULL, CFSTR("mDNSResponder:WatchForInternetSharingChanges"), CFSTR("com.apple.nat.plist"));
	if (!SCPrefs) { LogMsg("SCPreferencesCreate failed: %s", SCErrorString(SCError())); return(mStatus_NoMemoryErr); }

	SCPreferencesContext context = { 0, m, NULL, NULL, NULL };
	if (!SCPreferencesSetCallback(SCPrefs, InternetSharingChanged, &context))
		{ LogMsg("SCPreferencesSetCallback failed: %s", SCErrorString(SCError())); CFRelease(SCPrefs); return(mStatus_NoMemoryErr); }

	if (!SCPreferencesScheduleWithRunLoop(SCPrefs, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode))
		{ LogMsg("SCPreferencesScheduleWithRunLoop failed: %s", SCErrorString(SCError())); CFRelease(SCPrefs); return(mStatus_NoMemoryErr); }

	m->p->SCPrefs = SCPrefs;
	return(mStatus_NoError);
	}

// The definitions below should eventually come from some externally-supplied header file.
// However, since these definitions can't really be changed without breaking binary compatibility,
// they should never change, so in practice it should not be a big problem to have them defined here.

#define mDNS_IOREG_KEY               "mDNS_KEY"
#define mDNS_IOREG_VALUE             "2009-07-30"
#define mDNS_USER_CLIENT_CREATE_TYPE 'mDNS'

enum
	{							// commands from the daemon to the driver
    cmd_mDNSOffloadRR = 21,		// give the mdns update buffer to the driver
	};

typedef union { void *ptr; mDNSOpaque64 sixtyfourbits; } FatPtr;

typedef struct
	{                                   // cmd_mDNSOffloadRR structure
    uint32_t  command;                // set to OffloadRR
    uint32_t  rrBufferSize;           // number of bytes of RR records
    uint32_t  numUDPPorts;            // number of SRV UDP ports
    uint32_t  numTCPPorts;            // number of SRV TCP ports 
    uint32_t  numRRRecords;           // number of RR records
    uint32_t  compression;            // rrRecords - compression is base for compressed strings
    FatPtr    rrRecords;              // address of array of pointers to the rr records
    FatPtr    udpPorts;               // address of udp port list (SRV)
    FatPtr    tcpPorts;               // address of tcp port list (SRV)
	} mDNSOffloadCmd;

#include <IOKit/IOKitLib.h>
#include <dns_util.h>

mDNSlocal mDNSu16 GetPortArray(mDNS *const m, int trans, mDNSIPPort *portarray)
	{
	const domainlabel *const tp = (trans == mDNSTransport_UDP) ? (const domainlabel *)"\x4_udp" : (const domainlabel *)"\x4_tcp";
	int count = 0;
	AuthRecord *rr;
	for (rr = m->ResourceRecords; rr; rr=rr->next)
		if (rr->resrec.rrtype == kDNSType_SRV && SameDomainLabel(ThirdLabel(rr->resrec.name)->c, tp->c))
			{
			if (portarray) portarray[count] = rr->resrec.rdata->u.srv.port;
			count++;
			}

	// If Back to My Mac is on, also wake for packets to the IPSEC UDP port (4500)
	if (trans == mDNSTransport_UDP && TunnelServers(m))	
		{
		LogSPS("GetPortArray Back to My Mac at %d", count);
		if (portarray) portarray[count] = IPSECPort;
		count++;
		}
	return(count);
	}

#define TfrRecordToNIC(RR) \
	(((RR)->resrec.InterfaceID && (RR)->resrec.InterfaceID != mDNSInterface_LocalOnly) || \
	(!(RR)->resrec.InterfaceID && ((RR)->ForceMCast || IsLocalDomain((RR)->resrec.name))))

mDNSlocal mDNSu32 CountProxyRecords(mDNS *const m, uint32_t *const numbytes)
	{
	*numbytes = 0;
	int count = 0;
	AuthRecord *rr;
	for (rr = m->ResourceRecords; rr; rr=rr->next)
		if (rr->resrec.RecordType > kDNSRecordTypeDeregistering)
			if (TfrRecordToNIC(rr))
				{
				*numbytes += DomainNameLength(rr->resrec.name) + 10 + rr->resrec.rdestimate;
				LogSPS("CountProxyRecords: %3d size %5d total %5d %s",
					count, DomainNameLength(rr->resrec.name) + 10 + rr->resrec.rdestimate, *numbytes, ARDisplayString(m,rr));
				count++;
				}
	return(count);
	}

mDNSlocal void GetProxyRecords(mDNS *const m, DNSMessage *const msg, uint32_t *const numbytes, FatPtr *const records)
	{
	mDNSu8 *p = msg->data;
	const mDNSu8 *const limit = p + *numbytes;
	InitializeDNSMessage(&msg->h, zeroID, zeroID);

	int count = 0;
	AuthRecord *rr;
	for (rr = m->ResourceRecords; rr; rr=rr->next)
		if (rr->resrec.RecordType > kDNSRecordTypeDeregistering)
			if (TfrRecordToNIC(rr))
				{
				records[count].sixtyfourbits = zeroOpaque64;
				records[count].ptr = p;
				if (rr->resrec.RecordType & kDNSRecordTypeUniqueMask)
					rr->resrec.rrclass |= kDNSClass_UniqueRRSet;	// Temporarily set the 'unique' bit so PutResourceRecord will set it
				p = PutResourceRecordTTLWithLimit(msg, p, &msg->h.mDNS_numUpdates, &rr->resrec, rr->resrec.rroriginalttl, limit);
				rr->resrec.rrclass &= ~kDNSClass_UniqueRRSet;		// Make sure to clear 'unique' bit back to normal state
				LogSPS("GetProxyRecords: %3d start %p end %p size %5d total %5d %s",
					count, records[count].ptr, p, p - (mDNSu8 *)records[count].ptr, p - msg->data, ARDisplayString(m,rr));
				count++;
				}
	*numbytes = p - msg->data;
	}

// If compiling with old headers and libraries (pre 10.5) that don't include IOConnectCallStructMethod
// then we declare a dummy version here so that the code at least compiles
#ifndef AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER
static kern_return_t
IOConnectCallStructMethod(
	mach_port_t	 connection,		// In
	uint32_t	 selector,			// In
	const void	*inputStruct,		// In
	size_t		 inputStructCnt,	// In
	void		*outputStruct,		// Out
	size_t		*outputStructCnt)	// In/Out
	{
	(void)connection;
	(void)selector;
	(void)inputStruct;
	(void)inputStructCnt;
	(void)outputStruct;
	(void)outputStructCnt;
	LogMsg("Compiled without IOConnectCallStructMethod");
	return(KERN_FAILURE);
	}
#endif

mDNSexport mStatus ActivateLocalProxy(mDNS *const m, char *ifname)
	{
	mStatus result = mStatus_UnknownErr;
	io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, IOBSDNameMatching(kIOMasterPortDefault, 0, ifname));
	if (!service) { LogMsg("ActivateLocalProxy: No service for interface %s", ifname); return(mStatus_UnknownErr); }

	io_name_t n1, n2;
	IOObjectGetClass(service, n1);
	io_object_t parent;
	kern_return_t kr = IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent);
	if (kr != KERN_SUCCESS) LogMsg("ActivateLocalProxy: IORegistryEntryGetParentEntry for %s/%s failed %d", ifname, n1, kr);
	else
		{
		IOObjectGetClass(parent, n2);
		LogSPS("ActivateLocalProxy: Interface %s service %s parent %s", ifname, n1, n2);
		const CFTypeRef ref = IORegistryEntryCreateCFProperty(parent, CFSTR(mDNS_IOREG_KEY), kCFAllocatorDefault, mDNSNULL);
		if (!ref) LogSPS("ActivateLocalProxy: No mDNS_IOREG_KEY for interface %s/%s/%s", ifname, n1, n2);
		else
			{
			if (CFGetTypeID(ref) != CFStringGetTypeID() || !CFEqual(ref, CFSTR(mDNS_IOREG_VALUE)))
				LogMsg("ActivateLocalProxy: mDNS_IOREG_KEY for interface %s/%s/%s value %s != %s",
					ifname, n1, n2, CFStringGetCStringPtr(ref, mDNSNULL), mDNS_IOREG_VALUE);
			else
				{
				io_connect_t conObj;
				kr = IOServiceOpen(parent, mach_task_self(), mDNS_USER_CLIENT_CREATE_TYPE, &conObj);
				if (kr != KERN_SUCCESS) LogMsg("ActivateLocalProxy: IOServiceOpen for %s/%s/%s failed %d", ifname, n1, n2, kr);
				else
					{
					mDNSOffloadCmd cmd;
					mDNSPlatformMemZero(&cmd, sizeof(cmd)); // When compiling 32-bit, make sure top 32 bits of 64-bit pointers get initialized to zero
					cmd.command       = cmd_mDNSOffloadRR;
					cmd.numUDPPorts   = GetPortArray(m, mDNSTransport_UDP, mDNSNULL);
					cmd.numTCPPorts   = GetPortArray(m, mDNSTransport_TCP, mDNSNULL);
					cmd.numRRRecords  = CountProxyRecords(m, &cmd.rrBufferSize);
					cmd.compression   = sizeof(DNSMessageHeader);

					DNSMessage *msg = (DNSMessage *)mallocL("mDNSOffloadCmd msg", sizeof(DNSMessageHeader) + cmd.rrBufferSize);
					cmd.rrRecords.ptr = mallocL("mDNSOffloadCmd rrRecords", cmd.numRRRecords * sizeof(FatPtr));
					cmd.udpPorts .ptr = mallocL("mDNSOffloadCmd udpPorts",  cmd.numUDPPorts  * sizeof(mDNSIPPort));
					cmd.tcpPorts .ptr = mallocL("mDNSOffloadCmd tcpPorts",  cmd.numTCPPorts  * sizeof(mDNSIPPort));

					LogSPS("ActivateLocalProxy: msg %p %d RR %p %d, UDP %p %d, TCP %p %d",
						msg, cmd.rrBufferSize,
						cmd.rrRecords.ptr, cmd.numRRRecords,
						cmd.udpPorts .ptr, cmd.numUDPPorts,
						cmd.tcpPorts .ptr, cmd.numTCPPorts);

					if (!msg || !cmd.rrRecords.ptr || !cmd.udpPorts.ptr || !cmd.tcpPorts.ptr)
						LogMsg("ActivateLocalProxy: Failed to allocate memory: msg %p %d RR %p %d, UDP %p %d, TCP %p %d",
						msg, cmd.rrBufferSize,
						cmd.rrRecords.ptr, cmd.numRRRecords,
						cmd.udpPorts .ptr, cmd.numUDPPorts,
						cmd.tcpPorts .ptr, cmd.numTCPPorts);
					else
						{
						GetProxyRecords(m, msg, &cmd.rrBufferSize, cmd.rrRecords.ptr);
						GetPortArray(m, mDNSTransport_UDP, cmd.udpPorts.ptr);
						GetPortArray(m, mDNSTransport_TCP, cmd.tcpPorts.ptr);
						char outputData[2];
						size_t outputDataSize = sizeof(outputData);
						kr = IOConnectCallStructMethod(conObj, 0, &cmd, sizeof(cmd), outputData, &outputDataSize);
						LogSPS("ActivateLocalProxy: IOConnectCallStructMethod for %s/%s/%s %d", ifname, n1, n2, kr);
						if (kr == KERN_SUCCESS) result = mStatus_NoError;
						}

				 	if (cmd.tcpPorts. ptr) freeL("mDNSOffloadCmd udpPorts",  cmd.tcpPorts .ptr);
					if (cmd.udpPorts. ptr) freeL("mDNSOffloadCmd tcpPorts",  cmd.udpPorts .ptr);
					if (cmd.rrRecords.ptr) freeL("mDNSOffloadCmd rrRecords", cmd.rrRecords.ptr);
					if (msg)               freeL("mDNSOffloadCmd msg",       msg);
					IOServiceClose(conObj);
					}
				}
			CFRelease(ref);
			}
		IOObjectRelease(parent);
		}
	IOObjectRelease(service);
	return result;
	}

#endif // APPLE_OSX_mDNSResponder

static io_service_t g_rootdomain = MACH_PORT_NULL;

mDNSlocal mDNSBool SystemWakeForNetworkAccess(void)
	{
	mDNSs32 val = 0;
	CFBooleanRef clamshellStop = NULL;
	mDNSBool retnow = mDNSfalse;

	GetCurrentPMSetting(CFSTR("Wake On LAN"), &val);
	LogSPS("SystemWakeForNetworkAccess: Wake On LAN: %d", val);
	if (!val) return mDNSfalse;
	
	if (!g_rootdomain) g_rootdomain = IORegistryEntryFromPath(MACH_PORT_NULL, kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");
	if (!g_rootdomain) { LogMsg("SystemWakeForNetworkAccess: IORegistryEntryFromPath failed; assuming no clamshell so can WOMP"); return mDNStrue; }
	
	clamshellStop = (CFBooleanRef)IORegistryEntryCreateCFProperty(g_rootdomain, CFSTR(kAppleClamshellStateKey), kCFAllocatorDefault, 0);
	if (!clamshellStop) { LogSPS("SystemWakeForNetworkAccess: kAppleClamshellStateKey does not exist; assuming no clamshell so can WOMP"); return mDNStrue; }
	retnow = clamshellStop == kCFBooleanFalse;
	CFRelease(clamshellStop);
	if (retnow) { LogSPS("SystemWakeForNetworkAccess: kAppleClamshellStateKey is false; clamshell is open so can WOMP"); return mDNStrue; }
	
	clamshellStop = (CFBooleanRef)IORegistryEntryCreateCFProperty(g_rootdomain, CFSTR(kAppleClamshellCausesSleepKey), kCFAllocatorDefault, 0);
	if (!clamshellStop) { LogSPS("SystemWakeForNetworkAccess: kAppleClamshellCausesSleepKey does not exist; assuming no clamshell so can WOMP"); return mDNStrue; }
	retnow = clamshellStop == kCFBooleanFalse;
	CFRelease(clamshellStop);	
	if (retnow) { LogSPS("SystemWakeForNetworkAccess: kAppleClamshellCausesSleepKey is false; clamshell is closed but can WOMP"); return mDNStrue; }
	
	LogSPS("SystemWakeForNetworkAccess: clamshell is closed and can't WOMP");
	return mDNSfalse;
	}

mDNSexport void mDNSMacOSXNetworkChanged(mDNS *const m)
	{
	LogInfo("***   Network Configuration Change   ***  (%d)%s",
		m->p->NetworkChanged ? mDNS_TimeNow(m) - m->p->NetworkChanged : 0,
		m->p->NetworkChanged ? "" : " (no scheduled configuration change)");
	m->p->NetworkChanged = 0;		// If we received a network change event and deferred processing, we're now dealing with it
	mDNSs32 utc = mDNSPlatformUTC();
	m->SystemWakeOnLANEnabled = SystemWakeForNetworkAccess();
	MarkAllInterfacesInactive(m, utc);
	UpdateInterfaceList(m, utc);
	ClearInactiveInterfaces(m, utc);
	SetupActiveInterfaces(m, utc);

#if APPLE_OSX_mDNSResponder

	if (m->AutoTunnelHostAddr.b[0])
		{
		mDNS_Lock(m);
		if (TunnelClients(m) || TunnelServers(m))
			SetupLocalAutoTunnelInterface_internal(m);
		mDNS_Unlock(m);
		}
	
	// Scan to find client tunnels whose questions have completed,
	// but whose local inner/outer addresses have changed since the tunnel was set up
	ClientTunnel *p;
	for (p = m->TunnelClients; p; p = p->next)
		if (p->q.ThisQInterval < 0)
			{
			mDNSAddr tmpSrc = zeroAddr;
			mDNSAddr tmpDst = { mDNSAddrType_IPv4, {{{0}}} };
			tmpDst.ip.v4 = p->rmt_outer;
			mDNSPlatformSourceAddrForDest(&tmpSrc, &tmpDst);
			if (!mDNSSameIPv6Address(p->loc_inner, m->AutoTunnelHostAddr) ||
				!mDNSSameIPv4Address(p->loc_outer, tmpSrc.ip.v4))
				{
				AutoTunnelSetKeys(p, mDNSfalse);
				p->loc_inner = m->AutoTunnelHostAddr;
				p->loc_outer = tmpSrc.ip.v4;
				AutoTunnelSetKeys(p, mDNStrue);
				}
			}

	SetSPS(m);

	NetworkInterfaceInfoOSX *i;
	for (i = m->p->InterfaceList; i; i = i->next)
		{
		if (!m->SPSSocket)		// Not being Sleep Proxy Server; close any open BPF fds
			{
			if (i->BPF_fd >= 0 && CountProxyTargets(m, i, mDNSNULL, mDNSNULL) == 0) CloseBPF(i);
			}
		else								// else, we're Sleep Proxy Server; open BPF fds
			{
			if (i->Exists && i->Registered == i && !(i->ifa_flags & IFF_LOOPBACK) && i->BPF_fd == -1)
				{ LogSPS("%s requesting BPF", i->ifinfo.ifname); i->BPF_fd = -2; mDNSRequestBPF(); }
			}
		}

#endif // APPLE_OSX_mDNSResponder

	uDNS_SetupDNSConfig(m);
	mDNS_ConfigChanged(m);
	}

// Called with KQueueLock & mDNS lock
mDNSlocal void SetNetworkChanged(mDNS *const m, mDNSs32 delay)
	{
	if (!m->p->NetworkChanged || m->p->NetworkChanged - NonZeroTime(m->timenow + delay) < 0)
		{
		m->p->NetworkChanged = NonZeroTime(m->timenow + delay);
		LogInfo("SetNetworkChanged: setting network changed to %d (%d)", delay, m->p->NetworkChanged);
		}
	}


mDNSlocal void NetworkChanged(SCDynamicStoreRef store, CFArrayRef changedKeys, void *context)
	{
	(void)store;        // Parameter not used
	mDNS *const m = (mDNS *const)context;
	KQueueLock(m);
	mDNS_Lock(m);

	mDNSs32 delay = mDNSPlatformOneSecond * 2;				// Start off assuming a two-second delay

	int c = CFArrayGetCount(changedKeys);					// Count changes
	CFRange range = { 0, c };
	int c1 = (CFArrayContainsValue(changedKeys, range, NetworkChangedKey_Hostnames   ) != 0);
	int c2 = (CFArrayContainsValue(changedKeys, range, NetworkChangedKey_Computername) != 0);
	int c3 = (CFArrayContainsValue(changedKeys, range, NetworkChangedKey_DynamicDNS  ) != 0);
	int c4 = (CFArrayContainsValue(changedKeys, range, NetworkChangedKey_DNS         ) != 0);
	if (c && c - c1 - c2 - c3 - c4 == 0) delay = mDNSPlatformOneSecond/10;	// If these were the only changes, shorten delay
	
	if (mDNS_LoggingEnabled)
		{
		int i;
		for (i=0; i<c; i++)
			{
			char buf[256];
			if (!CFStringGetCString(CFArrayGetValueAtIndex(changedKeys, i), buf, sizeof(buf), kCFStringEncodingUTF8)) buf[0] = 0;
			LogInfo("***   NetworkChanged SC key: %s", buf);
			}
		LogInfo("***   NetworkChanged   *** %d change%s %s%s%s%sdelay %d",
			c, c>1?"s":"",
			c1 ? "(Local Hostname) " : "",
			c2 ? "(Computer Name) "  : "",
			c3 ? "(DynamicDNS) "     : "",
			c4 ? "(DNS) "            : "",
			delay);
		}

	SetNetworkChanged(m, delay);
	
	// KeyChain frequently fails to notify clients of change events. To work around this
	// we set a timer and periodically poll to detect if any changes have occurred.
	// Without this Back To My Mac just does't work for a large number of users.
	// See <rdar://problem/5124399> Not getting Keychain Changed events when enabling BTMM
	if (c3 || CFArrayContainsValue(changedKeys, range, NetworkChangedKey_BackToMyMac))
		{
		LogInfo("***   NetworkChanged   *** starting KeyChainBugTimer");
		m->p->KeyChainBugTimer    = NonZeroTime(m->timenow + delay);
		m->p->KeyChainBugInterval = mDNSPlatformOneSecond;
		}

	mDNS_Unlock(m);

	// If DNS settings changed, immediately force a reconfig (esp. cache flush)
	if (c4) mDNSMacOSXNetworkChanged(m);

	KQueueUnlock(m, "NetworkChanged");
	}

mDNSlocal mStatus WatchForNetworkChanges(mDNS *const m)
	{
	mStatus err = -1;
	SCDynamicStoreContext context = { 0, m, NULL, NULL, NULL };
	SCDynamicStoreRef     store    = SCDynamicStoreCreate(NULL, CFSTR("mDNSResponder:WatchForNetworkChanges"), NetworkChanged, &context);
	CFMutableArrayRef     keys     = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFStringRef           pattern1 = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv4);
	CFStringRef           pattern2 = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv6);
	CFMutableArrayRef     patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (!store) { LogMsg("SCDynamicStoreCreate failed: %s", SCErrorString(SCError())); goto error; }
	if (!keys || !pattern1 || !pattern2 || !patterns) goto error;

	CFArrayAppendValue(keys, NetworkChangedKey_IPv4);
	CFArrayAppendValue(keys, NetworkChangedKey_IPv6);
	CFArrayAppendValue(keys, NetworkChangedKey_Hostnames);
	CFArrayAppendValue(keys, NetworkChangedKey_Computername);
	CFArrayAppendValue(keys, NetworkChangedKey_DNS);
	CFArrayAppendValue(keys, NetworkChangedKey_DynamicDNS);
	CFArrayAppendValue(keys, NetworkChangedKey_BackToMyMac);
	CFArrayAppendValue(keys, CFSTR("State:/IOKit/PowerManagement/CurrentSettings")); // should remove as part of <rdar://problem/6751656>
	CFArrayAppendValue(patterns, pattern1);
	CFArrayAppendValue(patterns, pattern2);
	CFArrayAppendValue(patterns, CFSTR("State:/Network/Interface/[^/]+/AirPort"));
	if (!SCDynamicStoreSetNotificationKeys(store, keys, patterns))
		{ LogMsg("SCDynamicStoreSetNotificationKeys failed: %s", SCErrorString(SCError())); goto error; }

	m->p->StoreRLS = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
	if (!m->p->StoreRLS) { LogMsg("SCDynamicStoreCreateRunLoopSource failed: %s", SCErrorString(SCError())); goto error; }

	CFRunLoopAddSource(CFRunLoopGetCurrent(), m->p->StoreRLS, kCFRunLoopDefaultMode);
	m->p->Store = store;
	err = 0;
	goto exit;

	error:
	if (store)    CFRelease(store);

	exit:
	if (patterns) CFRelease(patterns);
	if (pattern2) CFRelease(pattern2);
	if (pattern1) CFRelease(pattern1);
	if (keys)     CFRelease(keys);

	return(err);
	}

#if 0 // <rdar://problem/6751656>
mDNSlocal void PMChanged(void *context)
	{
	mDNS *const m = (mDNS *const)context;

	KQueueLock(m);
	mDNS_Lock(m);
	
	LogSPS("PMChanged");
	
	SetNetworkChanged(m, mDNSPlatformOneSecond * 2);
	
	mDNS_Unlock(m);
	KQueueUnlock(m, "PMChanged");
	}

mDNSlocal mStatus WatchForPMChanges(mDNS *const m)
	{
	m->p->PMRLS = IOPMPrefsNotificationCreateRunLoopSource(PMChanged, m);
	if (!m->p->PMRLS) { LogMsg("IOPMPrefsNotificationCreateRunLoopSource failed!"); return mStatus_UnknownErr; }

	CFRunLoopAddSource(CFRunLoopGetCurrent(), m->p->PMRLS, kCFRunLoopDefaultMode);

	return mStatus_NoError;
	}
#endif

#ifndef KEV_DL_WAKEFLAGS_CHANGED
#define KEV_DL_WAKEFLAGS_CHANGED 17
#endif

mDNSlocal void SysEventCallBack(int s1, short __unused filter, void *context)
	{
	mDNS *const m = (mDNS *const)context;

	mDNS_Lock(m);

	struct { struct kern_event_msg k; char extra[256]; } msg;
	int bytes = recv(s1, &msg, sizeof(msg), 0);
	if (bytes < 0)
		LogMsg("SysEventCallBack: recv error %d errno %d (%s)", bytes, errno, strerror(errno));
	else
		{
		LogInfo("SysEventCallBack got %d bytes size %d %X %s %X %s %X %s id %d code %d %s",
			bytes, msg.k.total_size,
			msg.k.vendor_code , msg.k.vendor_code  == KEV_VENDOR_APPLE  ? "KEV_VENDOR_APPLE"  : "?",
			msg.k.kev_class   , msg.k.kev_class    == KEV_NETWORK_CLASS ? "KEV_NETWORK_CLASS" : "?",
			msg.k.kev_subclass, msg.k.kev_subclass == KEV_DL_SUBCLASS   ? "KEV_DL_SUBCLASS"   : "?",
			msg.k.id, msg.k.event_code,
			msg.k.event_code == KEV_DL_SIFFLAGS             ? "KEV_DL_SIFFLAGS"             :
			msg.k.event_code == KEV_DL_SIFMETRICS           ? "KEV_DL_SIFMETRICS"           :
			msg.k.event_code == KEV_DL_SIFMTU               ? "KEV_DL_SIFMTU"               :
			msg.k.event_code == KEV_DL_SIFPHYS              ? "KEV_DL_SIFPHYS"              :
			msg.k.event_code == KEV_DL_SIFMEDIA             ? "KEV_DL_SIFMEDIA"             :
			msg.k.event_code == KEV_DL_SIFGENERIC           ? "KEV_DL_SIFGENERIC"           :
			msg.k.event_code == KEV_DL_ADDMULTI             ? "KEV_DL_ADDMULTI"             :
			msg.k.event_code == KEV_DL_DELMULTI             ? "KEV_DL_DELMULTI"             :
			msg.k.event_code == KEV_DL_IF_ATTACHED          ? "KEV_DL_IF_ATTACHED"          :
			msg.k.event_code == KEV_DL_IF_DETACHING         ? "KEV_DL_IF_DETACHING"         :
			msg.k.event_code == KEV_DL_IF_DETACHED          ? "KEV_DL_IF_DETACHED"          :
			msg.k.event_code == KEV_DL_LINK_OFF             ? "KEV_DL_LINK_OFF"             :
			msg.k.event_code == KEV_DL_LINK_ON              ? "KEV_DL_LINK_ON"              :
			msg.k.event_code == KEV_DL_PROTO_ATTACHED       ? "KEV_DL_PROTO_ATTACHED"       :
			msg.k.event_code == KEV_DL_PROTO_DETACHED       ? "KEV_DL_PROTO_DETACHED"       :
			msg.k.event_code == KEV_DL_LINK_ADDRESS_CHANGED ? "KEV_DL_LINK_ADDRESS_CHANGED" :
			msg.k.event_code == KEV_DL_WAKEFLAGS_CHANGED    ? "KEV_DL_WAKEFLAGS_CHANGED"    : "?");

		if (msg.k.event_code == KEV_DL_WAKEFLAGS_CHANGED) SetNetworkChanged(m, mDNSPlatformOneSecond * 2);
		}

	mDNS_Unlock(m);
	}

mDNSlocal mStatus WatchForSysEvents(mDNS *const m)
	{
	m->p->SysEventNotifier = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT);
	if (m->p->SysEventNotifier < 0)
		{ LogMsg("WatchForNetworkChanges: socket failed %s errno %d (%s)", errno, strerror(errno)); return(mStatus_NoMemoryErr); }

	struct kev_request kev_req = { KEV_VENDOR_APPLE, KEV_NETWORK_CLASS, KEV_DL_SUBCLASS };
	int err = ioctl(m->p->SysEventNotifier, SIOCSKEVFILT, &kev_req);
	if (err < 0)
		{
		LogMsg("WatchForNetworkChanges: SIOCSKEVFILT failed %s errno %d (%s)", errno, strerror(errno));
		close(m->p->SysEventNotifier);
		m->p->SysEventNotifier = -1;
		return(mStatus_UnknownErr);
		}

	m->p->SysEventKQueue.KQcallback = SysEventCallBack;
	m->p->SysEventKQueue.KQcontext  = m;
	m->p->SysEventKQueue.KQtask     = "System Event Notifier";
	KQueueSet(m->p->SysEventNotifier, EV_ADD, EVFILT_READ, &m->p->SysEventKQueue);

	return(mStatus_NoError);
	}

#ifndef NO_SECURITYFRAMEWORK
mDNSlocal OSStatus KeychainChanged(SecKeychainEvent keychainEvent, SecKeychainCallbackInfo *info, void *context)
	{
	LogInfo("***   Keychain Changed   ***");
	mDNS *const m = (mDNS *const)context;
	SecKeychainRef skc;
	OSStatus err = SecKeychainCopyDefault(&skc);
	if (!err)
		{
		if (info->keychain == skc)
			{
			// For delete events, attempt to verify what item was deleted fail because the item is already gone, so we just assume they may be relevant
			mDNSBool relevant = (keychainEvent == kSecDeleteEvent);
			if (!relevant)
				{
				UInt32 tags[3] = { kSecTypeItemAttr, kSecServiceItemAttr, kSecAccountItemAttr };
				SecKeychainAttributeInfo attrInfo = { 3, tags, NULL };	// Count, array of tags, array of formats
				SecKeychainAttributeList *a = NULL;
				err = SecKeychainItemCopyAttributesAndData(info->item, &attrInfo, NULL, &a, NULL, NULL);
				if (!err)
					{
					relevant = ((a->attr[0].length == 4 && (!strncasecmp(a->attr[0].data, "ddns", 4) || !strncasecmp(a->attr[0].data, "sndd", 4))) ||
								(a->attr[1].length >= 4 && (!strncasecmp(a->attr[1].data, "dns:", 4))));
					SecKeychainItemFreeAttributesAndData(a, NULL);
					}
				}
			if (relevant)
				{
				LogInfo("***   Keychain Changed   *** KeychainEvent=%d %s",
					keychainEvent,
					keychainEvent == kSecAddEvent    ? "kSecAddEvent"    :
					keychainEvent == kSecDeleteEvent ? "kSecDeleteEvent" :
					keychainEvent == kSecUpdateEvent ? "kSecUpdateEvent" : "<Unknown>");
				// We're running on the CFRunLoop (Mach port) thread, not the kqueue thread, so we need to grab the KQueueLock before proceeding
				KQueueLock(m);
				mDNS_Lock(m);
				SetDomainSecrets(m);
				mDNS_Unlock(m);
				KQueueUnlock(m, "KeychainChanged");
				}
			}
		CFRelease(skc);
		}

	return 0;
	}
#endif

#ifndef NO_IOPOWER
mDNSlocal void PowerOn(mDNS *const m)
	{
	mDNSCoreMachineSleep(m, false);		// Will set m->SleepState = SleepState_Awake;
	if (m->p->WakeAtUTC)
		{
		long utc = mDNSPlatformUTC();
		mDNSPowerRequest(-1,-1);		// Need to explicitly clear any previous power requests -- they're not cleared automatically on wake
		if      (m->p->WakeAtUTC - utc > 30) LogInfo("PowerChanged PowerOn %d seconds early, assuming not maintenance wake", m->p->WakeAtUTC - utc);
		else if (utc - m->p->WakeAtUTC > 30) LogInfo("PowerChanged PowerOn %d seconds late, assuming not maintenance wake", utc - m->p->WakeAtUTC);
		else
			{
			mDNSs32 i = 20;
			//int result = mDNSPowerRequest(0, i);
			//if (result == kIOReturnNotReady) do i += (i<20) ? 1 : ((i+3)/4); while (mDNSPowerRequest(0, i) == kIOReturnNotReady);
			LogMsg("PowerChanged: Waking for network maintenance operations %d seconds early; re-sleeping in %d seconds", m->p->WakeAtUTC - utc, i);
			m->p->RequestReSleep = mDNS_TimeNow(m) + i * mDNSPlatformOneSecond;
			}
		}
	}

mDNSlocal void PowerChanged(void *refcon, io_service_t service, natural_t messageType, void *messageArgument)
	{
	mDNS *const m = (mDNS *const)refcon;
	KQueueLock(m);
	(void)service;    // Parameter not used
	debugf("PowerChanged %X %lX", messageType, messageArgument);

	// Make sure our m->SystemWakeOnLANEnabled value correctly reflects the current system setting
	m->SystemWakeOnLANEnabled = SystemWakeForNetworkAccess();

	switch(messageType)
		{
		case kIOMessageCanSystemPowerOff:		LogInfo("PowerChanged kIOMessageCanSystemPowerOff     (no action)");	break;	// E0000240
		case kIOMessageSystemWillPowerOff:		LogInfo("PowerChanged kIOMessageSystemWillPowerOff");							// E0000250
												mDNSCoreMachineSleep(m, true);
												if (m->SleepState == SleepState_Sleeping) mDNSMacOSXNetworkChanged(m);
												break;
		case kIOMessageSystemWillNotPowerOff:	LogInfo("PowerChanged kIOMessageSystemWillNotPowerOff (no action)");	break;	// E0000260
		case kIOMessageCanSystemSleep:			LogInfo("PowerChanged kIOMessageCanSystemSleep        (no action)");	break;	// E0000270
		case kIOMessageSystemWillSleep:			LogInfo("PowerChanged kIOMessageSystemWillSleep");								// E0000280
												mDNSCoreMachineSleep(m, true);
												break;
		case kIOMessageSystemWillNotSleep:		LogInfo("PowerChanged kIOMessageSystemWillNotSleep    (no action)");	break;	// E0000290
		case kIOMessageSystemHasPoweredOn:		LogInfo("PowerChanged kIOMessageSystemHasPoweredOn");							// E0000300
												// If still sleeping (didn't get 'WillPowerOn' message for some reason?) wake now
												if (m->SleepState)
													{
													LogMsg("PowerChanged kIOMessageSystemHasPoweredOn: ERROR m->SleepState %d", m->SleepState);
													PowerOn(m);
													}
												// Just to be safe, schedule a mDNSMacOSXNetworkChanged(), in case we never received
												// the System Configuration Framework "network changed" event that we expect
												// to receive some time shortly after the kIOMessageSystemWillPowerOn message
												mDNS_Lock(m);
												if (!m->p->NetworkChanged ||
													m->p->NetworkChanged - NonZeroTime(m->timenow + mDNSPlatformOneSecond * 2) < 0)
													m->p->NetworkChanged = NonZeroTime(m->timenow + mDNSPlatformOneSecond * 2);
												mDNS_Unlock(m);
											
												break;
		case kIOMessageSystemWillRestart:		LogInfo("PowerChanged kIOMessageSystemWillRestart     (no action)");	break;	// E0000310
		case kIOMessageSystemWillPowerOn:		LogInfo("PowerChanged kIOMessageSystemWillPowerOn");							// E0000320

												// On Leopard and earlier, on wake from sleep, instead of reporting link state down, Apple
												// Ethernet drivers report "hardware incapable of detecting link state", which the kernel
												// interprets as "should assume we have networking", which results in the first 4-5 seconds
												// of packets vanishing into a black hole. To work around this, on wake from sleep,
												// we block for five seconds to let Ethernet come up, and then resume normal operation.
												if (OSXVers < OSXVers_10_6_SnowLeopard)
													{
													sleep(5);
													LogMsg("Running on Mac OS X version 10.%d earlier than 10.6; "
														"PowerChanged did sleep(5) to wait for Ethernet hardware", OSXVers - OSXVers_Base);
													}

												// Make sure our interface list is cleared to the empty state, then tell mDNSCore to wake
												if (m->SleepState != SleepState_Sleeping)
													{
													LogMsg("kIOMessageSystemWillPowerOn: ERROR m->SleepState %d", m->SleepState);
													m->SleepState = SleepState_Sleeping;
													mDNSMacOSXNetworkChanged(m);
													}
												PowerOn(m);
												break;
		default:								LogInfo("PowerChanged unknown message %X", messageType);				break;
		}

	if (messageType == kIOMessageSystemWillSleep) m->p->SleepCookie = (long)messageArgument;
	else IOAllowPowerChange(m->p->PowerConnection, (long)messageArgument);

	KQueueUnlock(m, "PowerChanged Sleep/Wake");
	}

#ifdef kIOPMAcknowledgmentOptionSystemCapabilityRequirements
mDNSlocal void SnowLeopardPowerChanged(void *refcon, IOPMConnection connection, IOPMConnectionMessageToken token, IOPMSystemPowerStateCapabilities eventDescriptor)
	{
	mDNS *const m = (mDNS *const)refcon;
	KQueueLock(m);
	LogSPS("SnowLeopardPowerChanged %X %X %X%s%s%s%s%s",
		connection, token, eventDescriptor,
		eventDescriptor & kIOPMSystemPowerStateCapabilityCPU     ? " CPU"     : "",
		eventDescriptor & kIOPMSystemPowerStateCapabilityVideo   ? " Video"   : "",
		eventDescriptor & kIOPMSystemPowerStateCapabilityAudio   ? " Audio"   : "",
		eventDescriptor & kIOPMSystemPowerStateCapabilityNetwork ? " Network" : "",
		eventDescriptor & kIOPMSystemPowerStateCapabilityDisk    ? " Disk"    : "");

	// Make sure our m->SystemWakeOnLANEnabled value correctly reflects the current system setting
	m->SystemWakeOnLANEnabled = SystemWakeForNetworkAccess();

	if (eventDescriptor & kIOPMSystemPowerStateCapabilityCPU)
		{
		// CPU Waking. Note: Can get this message repeatedly, as other subsystems power up or down.
		if (m->SleepState == SleepState_Sleeping) PowerOn(m);
		IOPMConnectionAcknowledgeEvent(connection, token);
		}
	else
		{
		// CPU sleeping. Should not get this repeatedly -- once we're told that the CPU is halting
		// we should hear nothing more until we're told that the CPU has started executing again.
		if (m->SleepState) LogMsg("SnowLeopardPowerChanged: Sleep Error %X m->SleepState %d", eventDescriptor, m->SleepState);
		//sleep(5);
		//mDNSMacOSXNetworkChanged(m);
		mDNSCoreMachineSleep(m, true);
		//if (m->SleepState == SleepState_Sleeping) mDNSMacOSXNetworkChanged(m);
		m->p->SleepCookie = token;
		}

	KQueueUnlock(m, "SnowLeopardPowerChanged Sleep/Wake");
	}
#endif

#endif /* NO_IOPOWER */

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Initialization & Teardown
#endif

CF_EXPORT CFDictionaryRef _CFCopySystemVersionDictionary(void);
CF_EXPORT const CFStringRef _kCFSystemVersionProductNameKey;
CF_EXPORT const CFStringRef _kCFSystemVersionProductVersionKey;
CF_EXPORT const CFStringRef _kCFSystemVersionBuildVersionKey;

// Major version  6 is 10.2.x (Jaguar)
// Major version  7 is 10.3.x (Panther)
// Major version  8 is 10.4.x (Tiger)
// Major version  9 is 10.5.x (Leopard)
// Major version 10 is 10.6.x (SnowLeopard)
mDNSexport int mDNSMacOSXSystemBuildNumber(char *HINFO_SWstring)
	{
	int major = 0, minor = 0;
	char letter = 0, prodname[256]="<Unknown>", prodvers[256]="<Unknown>", buildver[256]="<Unknown>";
	CFDictionaryRef vers = _CFCopySystemVersionDictionary();
	if (vers)
		{
		CFStringRef cfprodname = CFDictionaryGetValue(vers, _kCFSystemVersionProductNameKey);
		CFStringRef cfprodvers = CFDictionaryGetValue(vers, _kCFSystemVersionProductVersionKey);
		CFStringRef cfbuildver = CFDictionaryGetValue(vers, _kCFSystemVersionBuildVersionKey);
		if (cfprodname) CFStringGetCString(cfprodname, prodname, sizeof(prodname), kCFStringEncodingUTF8);
		if (cfprodvers) CFStringGetCString(cfprodvers, prodvers, sizeof(prodvers), kCFStringEncodingUTF8);
		if (cfbuildver && CFStringGetCString(cfbuildver, buildver, sizeof(buildver), kCFStringEncodingUTF8))
			sscanf(buildver, "%d%c%d", &major, &letter, &minor);
		CFRelease(vers);
		}
	if (!major) { major=8; LogMsg("Note: No Major Build Version number found; assuming 8"); }
	if (HINFO_SWstring) mDNS_snprintf(HINFO_SWstring, 256, "%s %s (%s), %s", prodname, prodvers, buildver, STRINGIFY(mDNSResponderVersion));
	return(major);
	}

// Test to see if we're the first client running on UDP port 5353, by trying to bind to 5353 without using SO_REUSEPORT.
// If we fail, someone else got here first. That's not a big problem; we can share the port for multicast responses --
// we just need to be aware that we shouldn't expect to successfully receive unicast UDP responses.
mDNSlocal mDNSBool mDNSPlatformInit_CanReceiveUnicast(void)
	{
	int err = -1;
	int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 3)
		LogMsg("mDNSPlatformInit_CanReceiveUnicast: socket error %d errno %d (%s)", s, errno, strerror(errno));
	else
		{
		struct sockaddr_in s5353;
		s5353.sin_family      = AF_INET;
		s5353.sin_port        = MulticastDNSPort.NotAnInteger;
		s5353.sin_addr.s_addr = 0;
		err = bind(s, (struct sockaddr *)&s5353, sizeof(s5353));
		close(s);
		}

	if (err) LogMsg("No unicast UDP responses");
	else     debugf("Unicast UDP responses okay");
	return(err == 0);
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

mDNSlocal mStatus mDNSPlatformInit_setup(mDNS *const m)
	{
	mStatus err;
	m->p->CFRunLoop = CFRunLoopGetCurrent();

	char HINFO_SWstring[256] = "";
	OSXVers = mDNSMacOSXSystemBuildNumber(HINFO_SWstring);

	// In 10.4, mDNSResponder is launched very early in the boot process, while other subsystems are still in the process of starting up.
	// If we can't read the user's preferences, then we sleep a bit and try again, for up to five seconds before we give up.
	int i;
	for (i=0; i<100; i++)
		{
		domainlabel testlabel;
		testlabel.c[0] = 0;
		GetUserSpecifiedLocalHostName(&testlabel);
		if (testlabel.c[0]) break;
		usleep(50000);
		}

	m->hostlabel.c[0]        = 0;

	int    get_model[2] = { CTL_HW, HW_MODEL };
	size_t len_model = sizeof(HINFO_HWstring_buffer);
	// Names that contain no commas are prototype model names, so we ignore those
	if (sysctl(get_model, 2, HINFO_HWstring_buffer, &len_model, NULL, 0) == 0 && strchr(HINFO_HWstring_buffer, ','))
		HINFO_HWstring = HINFO_HWstring_buffer;
	HINFO_HWstring_prefixlen = strcspn(HINFO_HWstring, "0123456789");

	if (OSXVers <  OSXVers_10_3_Panther     ) m->KnownBugs |= mDNS_KnownBug_PhantomInterfaces;
	if (OSXVers >= OSXVers_10_6_SnowLeopard ) m->KnownBugs |= mDNS_KnownBug_LossySyslog;
	if (mDNSPlatformInit_CanReceiveUnicast()) m->CanReceiveUnicastOn5353 = mDNStrue;

	mDNSu32 hlen = mDNSPlatformStrLen(HINFO_HWstring);
	mDNSu32 slen = mDNSPlatformStrLen(HINFO_SWstring);
	if (hlen + slen < 254)
		{
		m->HIHardware.c[0] = hlen;
		m->HISoftware.c[0] = slen;
		mDNSPlatformMemCopy(&m->HIHardware.c[1], HINFO_HWstring, hlen);
		mDNSPlatformMemCopy(&m->HISoftware.c[1], HINFO_SWstring, slen);
		}

 	m->p->permanentsockets.port  = MulticastDNSPort;
 	m->p->permanentsockets.m     = m;
	m->p->permanentsockets.sktv4 = m->p->permanentsockets.sktv6 = -1;
	m->p->permanentsockets.kqsv4.KQcallback = m->p->permanentsockets.kqsv6.KQcallback = myKQSocketCallBack;
	m->p->permanentsockets.kqsv4.KQcontext  = m->p->permanentsockets.kqsv6.KQcontext  = &m->p->permanentsockets;
	m->p->permanentsockets.kqsv4.KQtask     = m->p->permanentsockets.kqsv6.KQtask     = "UDP packet reception";

	err = SetupSocket(&m->p->permanentsockets, MulticastDNSPort, AF_INET, mDNSNULL);
#ifndef NO_IPV6
	err = SetupSocket(&m->p->permanentsockets, MulticastDNSPort, AF_INET6, mDNSNULL);
#endif

	struct sockaddr_in s4;
	socklen_t n4 = sizeof(s4);
	if (getsockname(m->p->permanentsockets.sktv4, (struct sockaddr *)&s4, &n4) < 0) LogMsg("getsockname v4 error %d (%s)", errno, strerror(errno));
	else m->UnicastPort4.NotAnInteger = s4.sin_port;
#ifndef NO_IPV6
	if (m->p->permanentsockets.sktv6 >= 0)
		{
		struct sockaddr_in6 s6;
		socklen_t n6 = sizeof(s6);
		if (getsockname(m->p->permanentsockets.sktv6, (struct sockaddr *)&s6, &n6) < 0) LogMsg("getsockname v6 error %d (%s)", errno, strerror(errno));
		else m->UnicastPort6.NotAnInteger = s6.sin6_port;
		}
#endif

	m->p->InterfaceList      = mDNSNULL;
	m->p->userhostlabel.c[0] = 0;
	m->p->usernicelabel.c[0] = 0;
	m->p->NotifyUser         = 0;
	m->p->KeyChainBugTimer   = 0;
	m->p->WakeAtUTC          = 0;
	m->p->RequestReSleep     = 0;
	
#if APPLE_OSX_mDNSResponder
	uuid_generate(m->asl_uuid);
#endif

	m->AutoTunnelHostAddr.b[0] = 0;		// Zero out AutoTunnelHostAddr so UpdateInterfaceList() know it has to set it up

	NetworkChangedKey_IPv4         = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4);
	NetworkChangedKey_IPv6         = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv6);
	NetworkChangedKey_Hostnames    = SCDynamicStoreKeyCreateHostNames(NULL);
	NetworkChangedKey_Computername = SCDynamicStoreKeyCreateComputerName(NULL);
	NetworkChangedKey_DNS          = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetDNS);
	if (!NetworkChangedKey_IPv4 || !NetworkChangedKey_IPv6 || !NetworkChangedKey_Hostnames || !NetworkChangedKey_Computername || !NetworkChangedKey_DNS)
		{ LogMsg("SCDynamicStore string setup failed"); return(mStatus_NoMemoryErr); }

	err = WatchForNetworkChanges(m);
	if (err) { LogMsg("mDNSPlatformInit_setup: WatchForNetworkChanges failed %d", err); return(err); }

#if 0 // <rdar://problem/6751656>
	err = WatchForPMChanges(m);
	if (err) { LogMsg("mDNSPlatformInit_setup: WatchForPMChanges failed %d", err); return(err); }
#endif

	err = WatchForSysEvents(m);
	if (err) { LogMsg("mDNSPlatformInit_setup: WatchForSysEvents failed %d", err); return(err); }

	mDNSs32 utc = mDNSPlatformUTC();
	m->SystemWakeOnLANEnabled = SystemWakeForNetworkAccess();
	UpdateInterfaceList(m, utc);
	SetupActiveInterfaces(m, utc);

	// Explicitly ensure that our Keychain operations utilize the system domain.
#ifndef NO_SECURITYFRAMEWORK
	SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
#endif

	mDNS_Lock(m);
	SetDomainSecrets(m);
	SetLocalDomains();
	mDNS_Unlock(m);

#ifndef NO_SECURITYFRAMEWORK
	err = SecKeychainAddCallback(KeychainChanged, kSecAddEventMask|kSecDeleteEventMask|kSecUpdateEventMask, m);
	if (err) { LogMsg("mDNSPlatformInit_setup: SecKeychainAddCallback failed %d", err); return(err); }
#endif

#ifndef NO_IOPOWER

#ifndef kIOPMAcknowledgmentOptionSystemCapabilityRequirements
	LogMsg("Note: Compiled without SnowLeopard Fine-Grained Power Management support");
#else
	IOPMConnection c;
	IOReturn iopmerr = IOPMConnectionCreate(CFSTR("mDNSResponder"), kIOPMSystemPowerStateCapabilityCPU, &c);
	if (iopmerr) LogMsg("IOPMConnectionCreate failed %d", iopmerr);
	else
		{
		iopmerr = IOPMConnectionSetNotification(c, m, SnowLeopardPowerChanged);
		if (iopmerr) LogMsg("IOPMConnectionSetNotification failed %d", iopmerr);
		else
			{
			iopmerr = IOPMConnectionScheduleWithRunLoop(c, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
			if (iopmerr) LogMsg("IOPMConnectionScheduleWithRunLoop failed %d", iopmerr);
			}
		}
	m->p->IOPMConnection = iopmerr ? mDNSNULL : c;
	if (iopmerr) // If IOPMConnectionCreate unavailable or failed, proceed with old-style power notification code below
#endif // kIOPMAcknowledgmentOptionSystemCapabilityRequirements
		{
		m->p->PowerConnection = IORegisterForSystemPower(m, &m->p->PowerPortRef, PowerChanged, &m->p->PowerNotifier);
		if (!m->p->PowerConnection) { LogMsg("mDNSPlatformInit_setup: IORegisterForSystemPower failed"); return(-1); }
		else CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(m->p->PowerPortRef), kCFRunLoopDefaultMode);
		}
#endif /* NO_IOPOWER */

#if APPLE_OSX_mDNSResponder
	// Note: We use SPMetricPortability > 35 to indicate a laptop of some kind
	// SPMetricPortability <= 35 means nominally a non-portable machine (i.e. Mac mini or better)
	// An Apple TV does not actually weigh 3kg, but we assign it a 'nominal' mass of 3kg to indicate that it's treated as being relatively less portable than a laptop
	if      (!strncasecmp(HINFO_HWstring, "Xserve",   6)) { SPMetricPortability = 25 /* 30kg */; SPMetricMarginalPower = 84 /* 250W */; SPMetricTotalPower = 85 /* 300W */; }
	else if (!strncasecmp(HINFO_HWstring, "RackMac",  7)) { SPMetricPortability = 25 /* 30kg */; SPMetricMarginalPower = 84 /* 250W */; SPMetricTotalPower = 85 /* 300W */; }
	else if (!strncasecmp(HINFO_HWstring, "MacPro",   6)) { SPMetricPortability = 27 /* 20kg */; SPMetricMarginalPower = 84 /* 250W */; SPMetricTotalPower = 85 /* 300W */; }
	else if (!strncasecmp(HINFO_HWstring, "PowerMac", 8)) { SPMetricPortability = 27 /* 20kg */; SPMetricMarginalPower = 82 /* 160W */; SPMetricTotalPower = 83 /* 200W */; }
	else if (!strncasecmp(HINFO_HWstring, "iMac",     4)) { SPMetricPortability = 30 /* 10kg */; SPMetricMarginalPower = 77 /*  50W */; SPMetricTotalPower = 78 /*  60W */; }
	else if (!strncasecmp(HINFO_HWstring, "Macmini",  7)) { SPMetricPortability = 33 /*  5kg */; SPMetricMarginalPower = 73 /*  20W */; SPMetricTotalPower = 74 /*  25W */; }
	else if (!strncasecmp(HINFO_HWstring, "AppleTV",  7)) { SPMetricPortability = 35 /*  3kg */; SPMetricMarginalPower = 10 /*   0W */; SPMetricTotalPower = 73 /*  20W */; }
	else if (!strncasecmp(HINFO_HWstring, "MacBook",  7)) { SPMetricPortability = 37 /*  2kg */; SPMetricMarginalPower = 71 /*  13W */; SPMetricTotalPower = 72 /*  15W */; }
	else if (!strncasecmp(HINFO_HWstring, "PowerBook",9)) { SPMetricPortability = 37 /*  2kg */; SPMetricMarginalPower = 71 /*  13W */; SPMetricTotalPower = 72 /*  15W */; }
	LogSPS("HW_MODEL: %.*s (%s) Portability %d Marginal Power %d Total Power %d",
		HINFO_HWstring_prefixlen, HINFO_HWstring, HINFO_HWstring, SPMetricPortability, SPMetricMarginalPower, SPMetricTotalPower);

	err = WatchForInternetSharingChanges(m);
	if (err) { LogMsg("WatchForInternetSharingChanges failed %d", err); return(err); }
#endif // APPLE_OSX_mDNSResponder

	return(mStatus_NoError);
	}

mDNSexport mStatus mDNSPlatformInit(mDNS *const m)
	{
#if MDNS_NO_DNSINFO
	LogMsg("Note: Compiled without Apple-specific Split-DNS support");
#endif

	// Adding interfaces will use this flag, so set it now.
	m->DivertMulticastAdvertisements = !m->AdvertiseLocalAddresses;
	
#if APPLE_OSX_mDNSResponder
	m->SPSBrowseCallback = UpdateSPSStatus;
#endif

	mStatus result = mDNSPlatformInit_setup(m);

	// We don't do asynchronous initialization on OS X, so by the time we get here the setup will already
	// have succeeded or failed -- so if it succeeded, we should just call mDNSCoreInitComplete() immediately
	if (result == mStatus_NoError) mDNSCoreInitComplete(m, mStatus_NoError);
	return(result);
	}

mDNSexport void mDNSPlatformClose(mDNS *const m)
	{
	if (m->p->PowerConnection)
		{
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(m->p->PowerPortRef), kCFRunLoopDefaultMode);
#ifndef NO_IOPOWER
		// According to <http://developer.apple.com/qa/qa2004/qa1340.html>, a single call
		// to IORegisterForSystemPower creates *three* objects that need to be disposed individually:
		IODeregisterForSystemPower(&m->p->PowerNotifier);
		IOServiceClose            ( m->p->PowerConnection);
		IONotificationPortDestroy ( m->p->PowerPortRef);
#endif /* NO_IOPOWER */
		m->p->PowerConnection = 0;
		}

	if (m->p->Store)
		{
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), m->p->StoreRLS, kCFRunLoopDefaultMode);
		CFRunLoopSourceInvalidate(m->p->StoreRLS);
		CFRelease(m->p->StoreRLS);
		CFRelease(m->p->Store);
		m->p->Store    = NULL;
		m->p->StoreRLS = NULL;
		}
	
	if (m->p->PMRLS)
		{
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), m->p->PMRLS, kCFRunLoopDefaultMode);
		CFRunLoopSourceInvalidate(m->p->PMRLS);
		CFRelease(m->p->PMRLS);
		m->p->PMRLS = NULL;
		}

	if (m->p->SysEventNotifier >= 0) { close(m->p->SysEventNotifier); m->p->SysEventNotifier = -1; }

	mDNSs32 utc = mDNSPlatformUTC();
	MarkAllInterfacesInactive(m, utc);
	ClearInactiveInterfaces(m, utc);
	CloseSocketSet(&m->p->permanentsockets);

#if APPLE_OSX_mDNSResponder
	// clean up tunnels
	while (m->TunnelClients)
		{
		ClientTunnel *cur = m->TunnelClients;
		LogInfo("mDNSPlatformClose: removing client tunnel %p %##s from list", cur, cur->dstname.c);
		if (cur->q.ThisQInterval >= 0) mDNS_StopQuery(m, &cur->q);
		AutoTunnelSetKeys(cur, mDNSfalse);
		m->TunnelClients = cur->next;
		freeL("ClientTunnel", cur);
		}

	if (AnonymousRacoonConfig)
		{
		AnonymousRacoonConfig = mDNSNULL;
		LogInfo("mDNSPlatformClose: Deconfiguring autotunnel");
		(void)mDNSConfigureServer(kmDNSDown, mDNSNULL);
		}

	if (m->AutoTunnelHostAddrActive && m->AutoTunnelHostAddr.b[0])
		{
		m->AutoTunnelHostAddrActive = mDNSfalse;
		LogInfo("mDNSPlatformClose: Removing AutoTunnel address %.16a", &m->AutoTunnelHostAddr);
		(void)mDNSAutoTunnelInterfaceUpDown(kmDNSDown, m->AutoTunnelHostAddr.b);
		}
#endif // APPLE_OSX_mDNSResponder
	}

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - General Platform Support Layer functions
#endif

mDNSexport mDNSu32 mDNSPlatformRandomNumber(void)
	{
	return(arc4random());
	}

mDNSexport mDNSs32 mDNSPlatformOneSecond = 1000;
mDNSexport mDNSu32 mDNSPlatformClockDivisor = 0;

mDNSexport mStatus mDNSPlatformTimeInit(void)
	{
	// Notes: Typical values for mach_timebase_info:
	// tbi.numer = 1000 million
	// tbi.denom =   33 million
	// These are set such that (mach_absolute_time() * numer/denom) gives us nanoseconds;
	//          numer  / denom = nanoseconds per hardware clock tick (e.g. 30);
	//          denom  / numer = hardware clock ticks per nanosecond (e.g. 0.033)
	// (denom*1000000) / numer = hardware clock ticks per millisecond (e.g. 33333)
	// So: mach_absolute_time() / ((denom*1000000)/numer) = milliseconds
	//
	// Arithmetic notes:
	// tbi.denom is at least 1, and not more than 2^32-1.
	// Therefore (tbi.denom * 1000000) is at least one million, but cannot overflow a uint64_t.
	// tbi.denom is at least 1, and not more than 2^32-1.
	// Therefore clockdivisor should end up being a number roughly in the range 10^3 - 10^9.
	// If clockdivisor is less than 10^3 then that means that the native clock frequency is less than 1MHz,
	// which is unlikely on any current or future Macintosh.
	// If clockdivisor is greater than 10^9 then that means the native clock frequency is greater than 1000GHz.
	// When we ship Macs with clock frequencies above 1000GHz, we may have to update this code.
	struct mach_timebase_info tbi;
	kern_return_t result = mach_timebase_info(&tbi);
	if (result == KERN_SUCCESS) mDNSPlatformClockDivisor = ((uint64_t)tbi.denom * 1000000) / tbi.numer;
	return(result);
	}

mDNSexport mDNSs32 mDNSPlatformRawTime(void)
	{
	if (mDNSPlatformClockDivisor == 0) { LogMsg("mDNSPlatformRawTime called before mDNSPlatformTimeInit"); return(0); }

	static uint64_t last_mach_absolute_time = 0;
	//static uint64_t last_mach_absolute_time = 0x8000000000000000LL;	// Use this value for testing the alert display
	uint64_t this_mach_absolute_time = mach_absolute_time();
	if ((int64_t)this_mach_absolute_time - (int64_t)last_mach_absolute_time < 0)
		{
		LogMsg("mDNSPlatformRawTime: last_mach_absolute_time %08X%08X", last_mach_absolute_time);
		LogMsg("mDNSPlatformRawTime: this_mach_absolute_time %08X%08X", this_mach_absolute_time);
		// Update last_mach_absolute_time *before* calling NotifyOfElusiveBug()
		last_mach_absolute_time = this_mach_absolute_time;
		// Only show "mach_absolute_time went backwards" notice on 10.4 (build 8xyyy) or later.
		// (This bug happens all the time on 10.3, and we know that's not going to be fixed.)
		if (OSXVers >= OSXVers_10_4_Tiger)
			NotifyOfElusiveBug("mach_absolute_time went backwards!",
				"This error occurs from time to time, often on newly released hardware, "
				"and usually the exact cause is different in each instance.\r\r"
				"Please file a new Radar bug report with the title “mach_absolute_time went backwards” "
				"and assign it to Radar Component “Kernel” Version “X”.");
		}
	last_mach_absolute_time = this_mach_absolute_time;

	return((mDNSs32)(this_mach_absolute_time / mDNSPlatformClockDivisor));
	}

mDNSexport mDNSs32 mDNSPlatformUTC(void)
	{
	return time(NULL);
	}

// Locking is a no-op here, because we're single-threaded with a CFRunLoop, so we can never interrupt ourselves
mDNSexport void     mDNSPlatformLock   (const mDNS *const m) { (void)m; }
mDNSexport void     mDNSPlatformUnlock (const mDNS *const m) { (void)m; }
mDNSexport void     mDNSPlatformStrCopy(      void *dst, const void *src)              { strcpy((char *)dst, (char *)src); }
mDNSexport mDNSu32  mDNSPlatformStrLen (                 const void *src)              { return(strlen((char*)src)); }
mDNSexport void     mDNSPlatformMemCopy(      void *dst, const void *src, mDNSu32 len) { memcpy(dst, src, len); }
mDNSexport mDNSBool mDNSPlatformMemSame(const void *dst, const void *src, mDNSu32 len) { return(memcmp(dst, src, len) == 0); }
mDNSexport void     mDNSPlatformMemZero(      void *dst,                  mDNSu32 len) { memset(dst, 0, len); }
#if !(APPLE_OSX_mDNSResponder && MACOSX_MDNS_MALLOC_DEBUGGING)
mDNSexport void *   mDNSPlatformMemAllocate(mDNSu32 len) { return(mallocL("mDNSPlatformMemAllocate", len)); }
#endif
mDNSexport void     mDNSPlatformMemFree    (void *mem)   { freeL("mDNSPlatformMemFree", mem); }
