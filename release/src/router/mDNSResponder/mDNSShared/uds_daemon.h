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

 	File:		uds_daemon.h

 	Contains:	Interfaces necessary to talk to uds_daemon.c.

 	Version:	1.0

    Change History (most recent first):

$Log: uds_daemon.h,v $
Revision 1.27  2009/04/30 20:07:51  mcguire
<rdar://problem/6822674> Support multiple UDSs from launchd

Revision 1.26  2008/10/02 22:26:21  cheshire
Moved declaration of BPF_fd from uds_daemon.c to mDNSMacOSX.c, where it really belongs

Revision 1.25  2008/09/27 01:08:25  cheshire
Added external declaration of "dnssd_sock_t BPF_fd"

Revision 1.24  2007/09/19 20:25:17  cheshire
Deleted outdated comment

Revision 1.23  2007/07/24 17:23:02  cheshire
Rename DefRegList as AutoRegistrationDomains

Revision 1.22  2007/07/11 02:58:04  cheshire
<rdar://problem/5303807> Register IPv6-only hostname and don't create port mappings for AutoTunnel services

Revision 1.21  2007/04/21 21:47:47  cheshire
<rdar://problem/4376383> Daemon: Add watchdog timer

Revision 1.20  2007/02/14 01:58:19  cheshire
<rdar://problem/4995831> Don't delete Unix Domain Socket on exit if we didn't create it on startup

Revision 1.19  2007/02/07 19:32:00  cheshire
<rdar://problem/4980353> All mDNSResponder components should contain version strings in SCCS-compatible format

Revision 1.18  2007/02/06 19:06:49  cheshire
<rdar://problem/3956518> Need to go native with launchd

Revision 1.17  2007/01/05 05:46:07  cheshire
Add mDNS *const m parameter to udsserver_handle_configchange()

Revision 1.16  2007/01/04 23:11:15  cheshire
<rdar://problem/4720673> uDNS: Need to start caching unicast records
When an automatic browsing domain is removed, generate appropriate "remove" events for legacy queries

Revision 1.15  2006/08/14 23:24:57  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.14  2005/01/27 17:48:39  cheshire
Added comment about CFSocketInvalidate closing the underlying socket

Revision 1.13  2004/12/10 05:27:26  cheshire
<rdar://problem/3909147> Guard against multiple autoname services of the same type on the same machine

Revision 1.12  2004/12/10 04:28:28  cheshire
<rdar://problem/3914406> User not notified of name changes for services using new UDS API

Revision 1.11  2004/12/06 21:15:23  ksekar
<rdar://problem/3884386> mDNSResponder crashed in CheckServiceRegistrations

Revision 1.10  2004/10/26 04:31:44  cheshire
Rename CountSubTypes() as ChopSubTypes()

Revision 1.9  2004/09/30 00:25:00  ksekar
<rdar://problem/3695802> Dynamically update default registration domains on config change

Revision 1.8  2004/09/21 21:05:11  cheshire
Move duplicate code out of mDNSMacOSX/daemon.c and mDNSPosix/PosixDaemon.c,
into mDNSShared/uds_daemon.c

Revision 1.7  2004/09/17 01:08:55  cheshire
Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
  The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
  declared in that file are ONLY appropriate to single-address-space embedded applications.
  For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.

Revision 1.6  2004/08/11 01:58:49  cheshire
Remove "mDNS *globalInstance" parameter from udsserver_init()

Revision 1.5  2004/06/18 04:44:58  rpantos
Use platform layer for socket types

Revision 1.4  2004/06/12 00:51:58  cheshire
Changes for Windows compatibility

Revision 1.3  2004/01/25 00:03:21  cheshire
Change to use mDNSVal16() instead of private PORT_AS_NUM() macro

Revision 1.2  2004/01/24 08:46:26  bradley
Added InterfaceID<->Index platform interfaces since they are now used by all platforms for the DNS-SD APIs.

Revision 1.1  2003/12/08 21:11:42  rpantos;
Changes necessary to support mDNSResponder on Linux.

*/

#include "mDNSEmbeddedAPI.h"
#include "dnssd_ipc.h"

/* Client interface: */

#define SRS_PORT(S) mDNSVal16((S)->RR_SRV.resrec.rdata->u.srv.port)

extern int udsserver_init(dnssd_sock_t skts[], mDNSu32 count);
extern mDNSs32 udsserver_idle(mDNSs32 nextevent);
extern void udsserver_info(mDNS *const m);	// print out info about current state
extern void udsserver_handle_configchange(mDNS *const m);
extern int udsserver_exit(void);	// should be called prior to app exit

/* Routines that uds_daemon expects to link against: */

typedef	void (*udsEventCallback)(int fd, short filter, void *context);
extern mStatus udsSupportAddFDToEventLoop(dnssd_sock_t fd, udsEventCallback callback, void *context);
extern mStatus udsSupportRemoveFDFromEventLoop(dnssd_sock_t fd); // Note: This also CLOSES the file descriptor as well

extern void RecordUpdatedNiceLabel(mDNS *const m, mDNSs32 delay);

// Globals and functions defined in uds_daemon.c and also shared with the old "daemon.c" on OS X

extern mDNS mDNSStorage;
extern DNameListElem *AutoRegistrationDomains;
extern DNameListElem *AutoBrowseDomains;

extern mDNSs32 ChopSubTypes(char *regtype);
extern AuthRecord *AllocateSubTypes(mDNSs32 NumSubTypes, char *p);
extern int CountExistingRegistrations(domainname *srv, mDNSIPPort port);
extern void FreeExtraRR(mDNS *const m, AuthRecord *const rr, mStatus result);
extern int CountPeerRegistrations(mDNS *const m, ServiceRecordSet *const srs);

#if APPLE_OSX_mDNSResponder
extern void machserver_automatic_browse_domain_changed(const domainname *d, mDNSBool add);
extern void machserver_automatic_registration_domain_changed(const domainname *d, mDNSBool add);
#endif

extern const char mDNSResponderVersionString_SCCS[];
#define mDNSResponderVersionString (mDNSResponderVersionString_SCCS+5)
