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

$Log: Mac\040OS\040Test\040Responder.c,v $
Revision 1.26  2008/11/04 19:43:35  cheshire
Updated comment about MAX_ESCAPED_DOMAIN_NAME size (should be 1009, not 1005)

Revision 1.25  2006/08/14 23:24:29  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.24  2004/12/16 20:49:34  cheshire
<rdar://problem/3324626> Cache memory management improvements

Revision 1.23  2004/09/17 01:08:50  cheshire
Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
  The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
  declared in that file are ONLY appropriate to single-address-space embedded applications.
  For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.

Revision 1.22  2004/08/13 23:25:01  cheshire
Now that we do both uDNS and mDNS, global replace "m->hostname" with
"m->MulticastHostname" for clarity

Revision 1.21  2004/03/12 21:30:25  cheshire
Build a System-Context Shared Library from mDNSCore, for the benefit of developers
like Muse Research who want to be able to use mDNS/DNS-SD from GPL-licensed code.

Revision 1.20  2004/02/09 23:23:32  cheshire
Advertise "IL 2\4th Floor.apple.com." as another test "browse domain"

Revision 1.19  2004/01/24 23:55:15  cheshire
Change to use mDNSOpaque16fromIntVal/mDNSVal16 instead of shifting and masking

Revision 1.18  2003/11/14 21:27:08  cheshire
<rdar://problem/3484766>: Security: Crashing bug in mDNSResponder
Fix code that should use buffer size MAX_ESCAPED_DOMAIN_NAME (1009) instead of 256-byte buffers.

Revision 1.17  2003/08/14 02:19:54  cheshire
<rdar://problem/3375491> Split generic ResourceRecord type into two separate types: AuthRecord and CacheRecord

Revision 1.16  2003/08/12 19:56:24  cheshire
Update to APSL 2.0

 */

#include <stdio.h>						// For printf()
#include <string.h>			// For strlen() etc.

#include <Events.h>						// For WaitNextEvent()
#include <SIOUX.h>						// For SIOUXHandleOneEvent()

#include "mDNSEmbeddedAPI.h"			// Defines the interface to the client layer above

#include "mDNSMacOS9.h"					// Defines the specific types needed to run mDNS on this platform

// These don't have to be globals, but their memory does need to remain valid for as
// long as the search is going on. They are declared as globals here for simplicity.
static mDNS m;
static mDNS_PlatformSupport p;
static ServiceRecordSet p1, p2, afp, http, njp;
static AuthRecord browsedomain1, browsedomain2;

// This sample code just calls mDNS_RenameAndReregisterService to automatically pick a new
// unique name for the service. For a device such as a printer, this may be appropriate.
// For a device with a user interface, and a screen, and a keyboard, the appropriate
// response may be to prompt the user and ask them to choose a new name for the service.
mDNSlocal void Callback(mDNS *const m, ServiceRecordSet *const sr, mStatus result)
	{
	switch (result)
		{
		case mStatus_NoError:      debugf("Callback: %##s Name Registered",   sr->RR_SRV.resrec.name->c); break;
		case mStatus_NameConflict: debugf("Callback: %##s Name Conflict",     sr->RR_SRV.resrec.name->c); break;
		case mStatus_MemFree:      debugf("Callback: %##s Memory Free",       sr->RR_SRV.resrec.name->c); break;
		default:                   debugf("Callback: %##s Unknown Result %d", sr->RR_SRV.resrec.name->c, result); break;
		}

	if (result == mStatus_NameConflict) mDNS_RenameAndReregisterService(m, sr, mDNSNULL);
	}

// RegisterService() is a simple wrapper function which takes C string
// parameters, converts them to domainname parameters, and calls mDNS_RegisterService()
mDNSlocal void RegisterService(mDNS *m, ServiceRecordSet *recordset,
	UInt16 PortAsNumber, const char txtinfo[],
	const domainlabel *const n, const char type[], const char domain[])
	{
	domainname t;
	domainname d;
	char buffer[MAX_ESCAPED_DOMAIN_NAME];
	UInt8 txtbuffer[512];

	MakeDomainNameFromDNSNameString(&t, type);
	MakeDomainNameFromDNSNameString(&d, domain);
	
	if (txtinfo)
		{
		strncpy((char*)txtbuffer+1, txtinfo, sizeof(txtbuffer)-1);
		txtbuffer[0] = (UInt8)strlen(txtinfo);
		}
	else
		txtbuffer[0] = 0;

	mDNS_RegisterService(m, recordset,
		n, &t, &d,									// Name, type, domain
		mDNSNULL, mDNSOpaque16fromIntVal(PortAsNumber),
		txtbuffer, (mDNSu16)(1+txtbuffer[0]),		// TXT data, length
		mDNSNULL, 0,								// Subtypes (none)
		mDNSInterface_Any,							// Interface ID
		Callback, mDNSNULL);						// Callback and context

	ConvertDomainNameToCString(recordset->RR_SRV.resrec.name, buffer);
	printf("Made Service Records for %s\n", buffer);
	}

// RegisterFakeServiceForTesting() simulates the effect of services being registered on
// dynamically-allocated port numbers. No real service exists on that port -- this is just for testing.
mDNSlocal void RegisterFakeServiceForTesting(mDNS *m, ServiceRecordSet *recordset, const char txtinfo[],
	const char name[], const char type[], const char domain[])
	{
	static UInt16 NextPort = 0xF000;
	domainlabel n;
	MakeDomainLabelFromLiteralString(&n, name);
	RegisterService(m, recordset, NextPort++, txtinfo, &n, type, domain);
	}

// CreateProxyRegistrationForRealService() checks to see if the given port is currently
// in use, and if so, advertises the specified service as present on that port.
// This is useful for advertising existing real services (Personal Web Sharing, Personal
// File Sharing, etc.) that currently don't register with mDNS Service Discovery themselves.
mDNSlocal OSStatus CreateProxyRegistrationForRealService(mDNS *m, UInt16 PortAsNumber, const char txtinfo[],
	const char *servicetype, ServiceRecordSet *recordset)
	{
	InetAddress ia;
	TBind bindReq;
	OSStatus err;
	TEndpointInfo endpointinfo;
	EndpointRef ep = OTOpenEndpoint(OTCreateConfiguration(kTCPName), 0, &endpointinfo, &err);
	if (!ep || err) { printf("OTOpenEndpoint (CreateProxyRegistrationForRealService) failed %d", err); return(err); }

	ia.fAddressType = AF_INET;
	ia.fPort        = mDNSOpaque16fromIntVal(PortAsNumber).NotAnInteger;
	ia.fHost        = 0;
	bindReq.addr.maxlen = sizeof(ia);
	bindReq.addr.len    = sizeof(ia);
	bindReq.addr.buf    = (UInt8*)&ia;
	bindReq.qlen        = 0;
	err = OTBind(ep, &bindReq, NULL);

	if (err == kOTBadAddressErr)
		RegisterService(m, recordset, PortAsNumber, txtinfo, &m->nicelabel, servicetype, "local.");
	else if (err)
		debugf("OTBind failed %d", err);

	OTCloseProvider(ep);
	return(noErr);
	}

// Done once on startup, and then again every time our address changes
mDNSlocal OSStatus mDNSResponderTestSetup(mDNS *m)
	{
	char buffer[MAX_ESCAPED_DOMAIN_NAME];
	mDNSv4Addr ip = m->HostInterfaces->ip.ip.v4;
	
	ConvertDomainNameToCString(&m->MulticastHostname, buffer);
	printf("Name %s\n", buffer);
	printf("IP   %d.%d.%d.%d\n", ip.b[0], ip.b[1], ip.b[2], ip.b[3]);

	printf("\n");
	printf("Registering Service Records\n");
	// Create example printer discovery records
	//static ServiceRecordSet p1, p2;

#define SRSET 0
#if SRSET==0
	RegisterFakeServiceForTesting(m, &p1, "path=/index.html", "Web Server One", "_http._tcp.", "local.");
	RegisterFakeServiceForTesting(m, &p2, "path=/path.html",  "Web Server Two", "_http._tcp.", "local.");
#elif SRSET==1
	RegisterFakeServiceForTesting(m, &p1, "rn=lpq1", "Epson Stylus 900N", "_printer._tcp.", "local.");
	RegisterFakeServiceForTesting(m, &p2, "rn=lpq2", "HP LaserJet",       "_printer._tcp.", "local.");
#else
	RegisterFakeServiceForTesting(m, &p1, "rn=lpq3", "My Printer",        "_printer._tcp.", "local.");
	RegisterFakeServiceForTesting(m, &p2, "lrn=pq4", "My Other Printer",  "_printer._tcp.", "local.");
#endif

	// If AFP Server is running, register a record for it
	CreateProxyRegistrationForRealService(m, 548, "", "_afpovertcp._tcp.", &afp);

	// If Web Server is running, register a record for it
	CreateProxyRegistrationForRealService(m, 80, "", "_http._tcp.", &http);

	// And pretend we always have an NJP server running on port 80 too
	//RegisterService(m, &njp, 80, "NJP/", &m->nicelabel, "_njp._tcp.", "local.");

	// Advertise that apple.com. is available for browsing
	mDNS_AdvertiseDomains(m, &browsedomain1, mDNS_DomainTypeBrowse, mDNSInterface_Any, "apple.com.");
	mDNS_AdvertiseDomains(m, &browsedomain2, mDNS_DomainTypeBrowse, mDNSInterface_Any, "IL 2\\4th Floor.apple.com.");

	return(kOTNoError);
	}

// YieldSomeTime() just cooperatively yields some time to other processes running on classic Mac OS
mDNSlocal Boolean YieldSomeTime(UInt32 milliseconds)
	{
	extern Boolean SIOUXQuitting;
	EventRecord e;
	WaitNextEvent(everyEvent, &e, milliseconds / 17, NULL);
	SIOUXHandleOneEvent(&e);
	return(SIOUXQuitting);
	}

int main()
	{
	mStatus err;
	Boolean DoneSetup = false;

	SIOUXSettings.asktosaveonclose = false;
	SIOUXSettings.userwindowtitle = "\pMulticast DNS Responder";

	printf("Multicast DNS Responder\n\n");
	printf("This software reports errors using MacsBug breaks,\n");
	printf("so if you don't have MacsBug installed your Mac may crash.\n\n");
	printf("******************************************************************************\n");

	err = InitOpenTransport();
	if (err) { debugf("InitOpenTransport failed %d", err); return(err); }

	err = mDNS_Init(&m, &p, mDNS_Init_NoCache, mDNS_Init_ZeroCacheSize,
		mDNS_Init_AdvertiseLocalAddresses, mDNS_Init_NoInitCallback, mDNS_Init_NoInitCallbackContext);
	if (err) return(err);

	while (!YieldSomeTime(35))
		{
#if MDNS_ONLYSYSTEMTASK
		// For debugging, use "#define MDNS_ONLYSYSTEMTASK 1" and call mDNSPlatformIdle() periodically.
		// For shipping code, don't define MDNS_ONLYSYSTEMTASK, and you don't need to call mDNSPlatformIdle()
		extern void mDNSPlatformIdle(mDNS *const m);
		mDNSPlatformIdle(&m);	// Only needed for debugging version
#endif
		if (m.mDNSPlatformStatus == mStatus_NoError && !DoneSetup)
			{
			DoneSetup = true;
			printf("\nListening for mDNS queries...\n");
			mDNSResponderTestSetup(&m);
			}
		}
	
	if (p1.RR_SRV.resrec.RecordType  ) mDNS_DeregisterService(&m, &p1);
	if (p2.RR_SRV.resrec.RecordType  ) mDNS_DeregisterService(&m, &p2);
	if (afp.RR_SRV.resrec.RecordType ) mDNS_DeregisterService(&m, &afp);
	if (http.RR_SRV.resrec.RecordType) mDNS_DeregisterService(&m, &http);
	if (njp.RR_SRV.resrec.RecordType ) mDNS_DeregisterService(&m, &njp);

	mDNS_Close(&m);
	
	return(0);
	}
