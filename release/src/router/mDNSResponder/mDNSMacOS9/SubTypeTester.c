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

$Log: SubTypeTester.c,v $
Revision 1.7  2006/08/14 23:24:29  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.6  2004/12/16 20:49:35  cheshire
<rdar://problem/3324626> Cache memory management improvements

Revision 1.5  2004/09/17 01:08:50  cheshire
Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
  The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
  declared in that file are ONLY appropriate to single-address-space embedded applications.
  For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.

Revision 1.4  2004/09/16 00:24:49  cheshire
<rdar://problem/3803162> Fix unsafe use of mDNSPlatformTimeNow()

Revision 1.3  2004/08/13 23:25:01  cheshire
Now that we do both uDNS and mDNS, global replace "m->hostname" with
"m->MulticastHostname" for clarity

Revision 1.2  2004/08/04 22:11:30  cheshire
<rdar://problem/3588761> Current method of doing subtypes causes name collisions
Change to use "._sub." instead of ".s." to mark subtypes.

Revision 1.1  2004/06/11 00:03:28  cheshire
Add code for testing avail/busy subtypes


 */

#include <stdio.h>						// For printf()
#include <string.h>						// For strlen() etc.

#include <Events.h>						// For WaitNextEvent()
#include <SIOUX.h>						// For SIOUXHandleOneEvent()

#include "mDNSEmbeddedAPI.h"			// Defines the interface to the client layer above

#include "mDNSMacOS9.h"					// Defines the specific types needed to run mDNS on this platform

// These don't have to be globals, but their memory does need to remain valid for as
// long as the search is going on. They are declared as globals here for simplicity.
static mDNS m;
static mDNS_PlatformSupport p;
static ServiceRecordSet p1, p2;
static AuthRecord availRec1, availRec2;
static Boolean availRec2Active;

// This sample code just calls mDNS_RenameAndReregisterService to automatically pick a new
// unique name for the service. For a device such as a printer, this may be appropriate.
// For a device with a user interface, and a screen, and a keyboard, the appropriate
// response may be to prompt the user and ask them to choose a new name for the service.
mDNSlocal void Callback(mDNS *const m, ServiceRecordSet *const sr, mStatus result)
	{
	switch (result)
		{
		case mStatus_NoError:      debugf("Callback: %##s Name Registered",   sr->RR_SRV.resrec.name.c); break;
		case mStatus_NameConflict: debugf("Callback: %##s Name Conflict",     sr->RR_SRV.resrec.name.c); break;
		case mStatus_MemFree:      debugf("Callback: %##s Memory Free",       sr->RR_SRV.resrec.name.c); break;
		default:                   debugf("Callback: %##s Unknown Result %d", sr->RR_SRV.resrec.name.c, result); break;
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

	RegisterFakeServiceForTesting(m, &p1, "", "One", "_raop._tcp.", "local.");
	RegisterFakeServiceForTesting(m, &p2, "", "Two", "_raop._tcp.", "local.");

	return(kOTNoError);
	}

mDNSlocal void AvailCallback(mDNS *const m, AuthRecord *const rr, mStatus result)
	{
	Boolean *b = (Boolean *)rr->RecordContext;
	(void)m; // Unused
	// Signal that our record is now free for re-use
	if (result == mStatus_MemFree) *b = false;
	}

mDNSlocal OSStatus mDNSResponderSetAvail(mDNS *m, AuthRecord *rr, ServiceRecordSet *sr)
	{
	// 1. Initialize required fields of AuthRecord
	// 2. Set name of subtype PTR record to our special subtype name denoting "available" instances
	// 3. Set target of subtype PTR record to point to our SRV record (exactly the same as the main service PTR record)
	// 4. And register it
	mDNS_SetupResourceRecord(rr, mDNSNULL, mDNSInterface_Any, kDNSType_PTR, 2*3600, kDNSRecordTypeShared, AvailCallback, &availRec2Active);
	MakeDomainNameFromDNSNameString(rr->resrec.name, "a._sub._raop._tcp.local.");
	AssignDomainName(&rr->resrec.rdata->u.name, sr->RR_SRV.resrec.name);
	return(mDNS_Register(m, rr));
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
	mDNSs32 nextAvail, nextBusy;

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
			mDNSResponderSetAvail(&m, &availRec1, &p1);
			availRec2Active = false;
			nextAvail = mDNS_TimeNow(&m) + mDNSPlatformOneSecond * 10;
			nextBusy  = mDNS_TimeNow(&m) + mDNSPlatformOneSecond * 15;
			}

		if (DoneSetup)
			{
			// We check availRec2.RecordType because we don't want to re-register this record
			// if the previous mDNS_Deregister() has not yet completed
			if (mDNS_TimeNow(&m) - nextAvail > 0 && !availRec2Active)
				{
				printf("Setting Two now available\n");
				availRec2Active = true;
				mDNSResponderSetAvail(&m, &availRec2, &p2);
				nextAvail = nextBusy + mDNSPlatformOneSecond * 10;
				}
			else if (mDNS_TimeNow(&m) - nextBusy > 0)
				{
				printf("Setting Two now busy\n");
				mDNS_Deregister(&m, &availRec2);
				nextBusy = nextAvail + mDNSPlatformOneSecond * 5;
				}
			}
		}
	
	if (p1.RR_SRV.resrec.RecordType) mDNS_DeregisterService(&m, &p1);
	if (p2.RR_SRV.resrec.RecordType) mDNS_DeregisterService(&m, &p2);
	if (availRec1.resrec.RecordType) mDNS_Deregister(&m, &availRec1);
	if (availRec2Active)             mDNS_Deregister(&m, &availRec2);

	mDNS_Close(&m);
	
	return(0);
	}
