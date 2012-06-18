/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

$Log: Responder.c,v $
Revision 1.3  2006/08/14 23:24:29  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.2  2004/05/20 18:38:31  cheshire
Fix build broken by removal of 'kDNSServiceFlagsAutoRename' from dns_sd.h

Revision 1.1  2004/03/12 21:30:25  cheshire
Build a System-Context Shared Library from mDNSCore, for the benefit of developers
like Muse Research who want to be able to use mDNS/DNS-SD from GPL-licensed code.

 */

#include <stdio.h>						// For printf()
#include <string.h>						// For strcpy()

#include <Events.h>						// For WaitNextEvent()

#include <OpenTransport.h>
#include <OpenTptInternet.h>

#include <SIOUX.h>						// For SIOUXHandleOneEvent()

#include "dns_sd.h"

typedef union { UInt8 b[2]; UInt16 NotAnInteger; } mDNSOpaque16;
static UInt16 mDNSVal16(mDNSOpaque16 x) { return((UInt16)(x.b[0]<<8 | x.b[1])); }
static mDNSOpaque16 mDNSOpaque16fromIntVal(UInt16 v)
	{ mDNSOpaque16 x; x.b[0] = (UInt8)(v >> 8); x.b[1] = (UInt8)(v & 0xFF); return(x); }

typedef struct RegisteredService_struct RegisteredService;
struct RegisteredService_struct
	{
	RegisteredService *next;
	DNSServiceRef sdRef;
	Boolean gotresult;
	DNSServiceErrorType errorCode;
	char namestr[64];
	char typestr[kDNSServiceMaxDomainName];
	char domstr [kDNSServiceMaxDomainName];
	};

static RegisteredService p1, p2, afp, http, njp;
static RegisteredService *services = NULL, **nextservice = &services;

static void RegCallback(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode,
						const char *name, const char *regtype, const char *domain, void *context)
    {
    RegisteredService *rs = (RegisteredService *)context;
    (void)sdRef;	// Unused
    (void)flags;	// Unused
    rs->gotresult = true;
    rs->errorCode = errorCode;
    strcpy(rs->namestr, name);
    strcpy(rs->typestr, regtype);
    strcpy(rs->domstr,  domain);
    }

static DNSServiceErrorType RegisterService(RegisteredService *rs, mDNSOpaque16 OpaquePort,
	const char name[], const char type[], const char domain[], const char txtinfo[])
	{
	DNSServiceErrorType err;
	unsigned char txtbuffer[257];
	strncpy((char*)txtbuffer+1, txtinfo, 255);
	txtbuffer[256] = 0;
	txtbuffer[0] = (unsigned char)strlen((char*)txtbuffer);
	rs->gotresult = 0;
	rs->errorCode = kDNSServiceErr_NoError;
	err = DNSServiceRegister(&rs->sdRef, /* kDNSServiceFlagsAutoRename*/ 0, 0,
		name, type, domain, NULL, OpaquePort.NotAnInteger, (unsigned short)(1+txtbuffer[0]), txtbuffer, RegCallback, rs);
	if (err)
		printf("RegisterService(%s %s %s) failed %d\n", name, type, domain, err);
	else
		{ *nextservice = rs; nextservice = &rs->next; }
	return(err);
	}

// RegisterFakeServiceForTesting() simulates the effect of services being registered on
// dynamically-allocated port numbers. No real service exists on that port -- this is just for testing.
static DNSServiceErrorType RegisterFakeServiceForTesting(RegisteredService *rs,
	const char name[], const char type[], const char domain[], const char txtinfo[])
	{
	static UInt16 NextPort = 0xF000;
	return RegisterService(rs, mDNSOpaque16fromIntVal(NextPort++), name, type, domain, txtinfo);
	}

// CreateProxyRegistrationForRealService() checks to see if the given port is currently
// in use, and if so, advertises the specified service as present on that port.
// This is useful for advertising existing real services (Personal Web Sharing, Personal
// File Sharing, etc.) that currently don't register with mDNS Service Discovery themselves.
static DNSServiceErrorType CreateProxyRegistrationForRealService(RegisteredService *rs,
	const char *servicetype, UInt16 PortAsNumber, const char txtinfo[])
	{
	mDNSOpaque16 OpaquePort = mDNSOpaque16fromIntVal(PortAsNumber);
	InetAddress ia;
	TBind bindReq;
	OSStatus err;
	TEndpointInfo endpointinfo;
	EndpointRef ep = OTOpenEndpoint(OTCreateConfiguration(kTCPName), 0, &endpointinfo, &err);
	if (!ep || err) { printf("OTOpenEndpoint (CreateProxyRegistrationForRealService) failed %d", err); return(err); }

	ia.fAddressType = AF_INET;
	ia.fPort        = OpaquePort.NotAnInteger;
	ia.fHost        = 0;
	bindReq.addr.maxlen = sizeof(ia);
	bindReq.addr.len    = sizeof(ia);
	bindReq.addr.buf    = (UInt8*)&ia;
	bindReq.qlen        = 0;
	err = OTBind(ep, &bindReq, NULL);

	if (err == kOTBadAddressErr)
		err = RegisterService(rs, OpaquePort, "", servicetype, "local.", txtinfo);
	else if (err)
		printf("OTBind failed %d", err);

	OTCloseProvider(ep);
	return(err);
	}

// YieldSomeTime() just cooperatively yields some time to other processes running on classic Mac OS
static Boolean YieldSomeTime(UInt32 milliseconds)
	{
	extern Boolean SIOUXQuitting;
	EventRecord e;
	WaitNextEvent(everyEvent, &e, milliseconds / 17, NULL);
	SIOUXHandleOneEvent(&e);
	return(SIOUXQuitting);
	}

int main()
	{
	OSStatus err;
	RegisteredService *s;
	
	SIOUXSettings.asktosaveonclose = false;
	SIOUXSettings.userwindowtitle = "\pMulticast DNS Responder";

	printf("Multicast DNS Responder\n\n");
	printf("This software reports errors using MacsBug breaks,\n");
	printf("so if you don't have MacsBug installed your Mac may crash.\n\n");
	printf("******************************************************************************\n\n");

	err = InitOpenTransport();
	if (err) { printf("InitOpenTransport failed %d", err); return(err); }

	printf("Advertising Services...\n");

#define SRSET 0
#if SRSET==0
	RegisterFakeServiceForTesting(&p1, "Web Server One", "_http._tcp.", "local.", "path=/index.html");
	RegisterFakeServiceForTesting(&p2, "Web Server Two", "_http._tcp.", "local.", "path=/path.html");
#elif SRSET==1
	RegisterFakeServiceForTesting(&p1, "Epson Stylus 900N", "_printer._tcp.", "local.", "rn=lpq1");
	RegisterFakeServiceForTesting(&p2, "HP LaserJet",       "_printer._tcp.", "local.", "rn=lpq2");
#else
	RegisterFakeServiceForTesting(&p1, "My Printer",        "_printer._tcp.", "local.", "rn=lpq3");
	RegisterFakeServiceForTesting(&p2, "My Other Printer",  "_printer._tcp.", "local.", "lrn=pq4");
#endif

	// If AFP Server is running, register a record for it
	CreateProxyRegistrationForRealService(&afp, "_afpovertcp._tcp.", 548, "");

	// If Web Server is running, register a record for it
	CreateProxyRegistrationForRealService(&http, "_http._tcp.",       80, "path=/index.html");

	while (!YieldSomeTime(35))
		for (s = services; s; s = s->next)
			if (s->gotresult)
				{
				printf("%s %s %s registered\n", s->namestr, s->typestr, s->domstr);
				s->gotresult = false;
				}
	
	for (s = services; s; s = s->next)
		if (s->sdRef) DNSServiceRefDeallocate(s->sdRef);

	CloseOpenTransport();
	return(0);
	}
