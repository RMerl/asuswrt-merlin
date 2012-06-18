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

$Log: uDNS.h,v $
Revision 1.32.2.1  2006/08/29 06:24:23  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.32  2005/07/29 19:46:10  ksekar
<rdar://problem/4191860> reduce polling period on failed LLQs to 15 minutes

Revision 1.31  2005/03/31 02:19:56  cheshire
<rdar://problem/4021486> Fix build warnings
Reviewed by: Scott Herscher

Revision 1.30  2005/03/04 03:00:03  ksekar
<rdar://problem/4026546> Retransmissions happen too early, causing registrations to conflict with themselves

Revision 1.29  2005/01/11 22:50:53  ksekar
Fixed constant naming (was using kLLQ_DefLease for update leases)

Revision 1.28  2004/12/22 00:13:49  ksekar
<rdar://problem/3873993> Change version, port, and polling interval for LLQ

Revision 1.27  2004/11/23 04:06:50  cheshire
Get rid of floating point constant -- in a small embedded device, bringing in all
the floating point libraries just to halve an integer value is a bit too heavyweight.

Revision 1.26  2004/11/22 17:49:15  ksekar
Changed INIT_REFRESH from fraction to decimal

Revision 1.25  2004/11/22 17:16:20  ksekar
<rdar://problem/3854298> Unicast services don't disappear when you disable all networking

Revision 1.24  2004/11/19 04:24:08  ksekar
<rdar://problem/3682609> Security: Enforce a "window" on one-shot wide-area queries

Revision 1.23  2004/11/18 18:04:21  ksekar
Add INIT_REFRESH constant

Revision 1.22  2004/11/15 20:09:24  ksekar
<rdar://problem/3719050> Wide Area support for Add/Remove record

Revision 1.21  2004/11/11 20:14:55  ksekar
<rdar://problem/3719574> Wide-Area registrations not deregistered on sleep

Revision 1.20  2004/10/16 00:16:59  cheshire
<rdar://problem/3770558> Replace IP TTL 255 check with local subnet source address check

Revision 1.19  2004/09/17 01:08:49  cheshire
Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
  The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
  declared in that file are ONLY appropriate to single-address-space embedded applications.
  For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.

Revision 1.18  2004/09/03 19:23:05  ksekar
<rdar://problem/3788460>: Need retransmission mechanism for wide-area service registrations

Revision 1.17  2004/09/01 03:59:29  ksekar
<rdar://problem/3783453>: Conditionally compile out uDNS code on Windows

Revision 1.16  2004/08/25 00:37:27  ksekar
<rdar://problem/3774635>: Cleanup DynDNS hostname registration code

Revision 1.15  2004/07/30 17:40:06  ksekar
<rdar://problem/3739115>: TXT Record updates not available for wide-area services

Revision 1.14  2004/07/29 19:27:15  ksekar
NATPMP Support - minor fixes and cleanup

Revision 1.13  2004/07/29 02:03:35  ksekar
Delete unused #define and structure field

Revision 1.12  2004/07/26 22:49:30  ksekar
<rdar://problem/3651409>: Feature #9516: Need support for NATPMP in client

Revision 1.11  2004/06/17 01:13:11  ksekar
<rdar://problem/3696616>: polling interval too short

Revision 1.10  2004/06/11 05:45:03  ksekar
<rdar://problem/3682397>: Change SRV names for LLQ/Update port lookups

Revision 1.9  2004/06/01 23:46:50  ksekar
<rdar://problem/3675149>: DynDNS: dynamically look up LLQ/Update ports

Revision 1.8  2004/05/28 23:42:37  ksekar
<rdar://problem/3258021>: Feature: DNS server->client notification on record changes (#7805)

Revision 1.7  2004/05/18 23:51:25  cheshire
Tidy up all checkin comments to use consistent "<rdar://problem/xxxxxxx>" format for bug numbers

Revision 1.6  2004/03/13 01:57:33  ksekar
<rdar://problem/3192546>: DynDNS: Dynamic update of service records

Revision 1.5  2004/02/21 08:56:58  bradley
Wrap prototypes with extern "C" for C++ builds.

Revision 1.4  2004/02/06 23:04:19  ksekar
Basic Dynamic Update support via mDNS_Register (dissabled via
UNICAST_REGISTRATION #define)

Revision 1.3  2004/01/24 03:38:27  cheshire
Fix minor syntactic error: Headers should use "extern" declarations, not "mDNSexport"

Revision 1.2  2004/01/23 23:23:15  ksekar
Added TCP support for truncated unicast messages.

Revision 1.1  2003/12/13 03:05:27  ksekar
<rdar://problem/3192548>: DynDNS: Unicast query of service records

 
 */

#ifndef __UDNS_H_
#define __UDNS_H_

#include "mDNSEmbeddedAPI.h"
#include "DNSCommon.h"

#ifdef	__cplusplus
	extern "C" {
#endif

#define RESTART_GOODBYE_DELAY    (6 * mDNSPlatformOneSecond) // delay after restarting LLQ before nuking previous known answers (avoids flutter if we restart before we have networking up)
#define MIN_UCAST_PERIODIC_EXEC  (5 * mDNSPlatformOneSecond) 	
#define INIT_UCAST_POLL_INTERVAL (3 * mDNSPlatformOneSecond) // this interval is used after send failures on network transitions
	                                                         // which typically heal quickly, so we start agressively and exponentially back off
#define MAX_UCAST_POLL_INTERVAL (60 * 60 * mDNSPlatformOneSecond)
#define LLQ_POLL_INTERVAL       (15 * 60 * mDNSPlatformOneSecond) // Polling interval for zones w/ an advertised LLQ port (ie not static zones) if LLQ fails due to NAT, etc.
#define RESPONSE_WINDOW (60 * mDNSPlatformOneSecond)         // require server responses within one minute of request
#define UPDATE_PORT_NAME "_dns-update._udp."
#define LLQ_PORT_NAME "_dns-llq._udp"
#define DEFAULT_UPDATE_LEASE 7200
	
// Entry points into unicast-specific routines

extern mStatus uDNS_StartQuery(mDNS *const m, DNSQuestion *const question);
extern mDNSBool uDNS_IsActiveQuery(DNSQuestion *const question, uDNS_GlobalInfo *u);  // returns true if OK to call StopQuery
extern mStatus uDNS_StopQuery(mDNS *const m, DNSQuestion *const question);
	
extern void uDNS_Init(mDNS *const m);
extern void uDNS_Sleep(mDNS *const m);
extern void uDNS_Wake(mDNS *const m);
#define uDNS_Close uDNS_Sleep
	
// uDNS_UpdateRecord
// following fields must be set, and the update validated, upon entry.
// rr->NewRData
// rr->newrdlength
// rr->UpdateCallback

extern mStatus uDNS_AddRecordToService(mDNS *const m, ServiceRecordSet *sr, ExtraResourceRecord *extra);
extern mStatus uDNS_UpdateRecord(mDNS *m, AuthRecord *rr);
	
extern mStatus uDNS_RegisterRecord(mDNS *const m, AuthRecord *const rr);
extern mStatus uDNS_DeregisterRecord(mDNS *const m, AuthRecord *const rr);

extern mStatus uDNS_RegisterService(mDNS *const m, ServiceRecordSet *srs);
extern mStatus uDNS_DeregisterService(mDNS *const m, ServiceRecordSet *srs);

// integer fields of msg header must be in HOST byte order before calling this routine
extern void uDNS_ReceiveMsg(mDNS *const m, DNSMessage *const msg, const mDNSu8 *const end,
	const mDNSAddr *const srcaddr, const mDNSIPPort srcport, const mDNSAddr *const dstaddr, 
	const mDNSIPPort dstport, const mDNSInterfaceID InterfaceID);

extern void uDNS_ReceiveNATMap(mDNS *m, mDNSu8 *pkt, mDNSu16 len);
	
// returns time of next scheduled event
extern void uDNS_Execute(mDNS *const m);

	
#ifdef	__cplusplus
	}
#endif

#endif // __UDNS_H_
