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

$Log: Searcher.c,v $
Revision 1.4  2006/12/19 22:43:54  cheshire
Fix compiler warnings

Revision 1.3  2006/08/14 23:24:29  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.2  2004/05/27 06:30:21  cheshire
Add code to test DNSServiceQueryRecord()

Revision 1.1  2004/03/12 21:30:25  cheshire
Build a System-Context Shared Library from mDNSCore, for the benefit of developers
like Muse Research who want to be able to use mDNS/DNS-SD from GPL-licensed code.

 */

#include <stdio.h>						// For printf()
#include <string.h>						// For strcpy()

#include <Events.h>						// For WaitNextEvent()
#include <CodeFragments.h>				// For SIOkUnresolvedCFragSymbolAddress

#include <SIOUX.h>						// For SIOUXHandleOneEvent()

#include <OpenTransport.h>
#include <OpenTptInternet.h>

#include "dns_sd.h"

#define ns_c_in 1
#define ns_t_a 1

typedef union { UInt8 b[2]; UInt16 NotAnInteger; } mDNSOpaque16;
static UInt16 mDNSVal16(mDNSOpaque16 x) { return((UInt16)(x.b[0]<<8 | x.b[1])); }
static mDNSOpaque16 mDNSOpaque16fromIntVal(UInt16 v)
	{ mDNSOpaque16 x; x.b[0] = (UInt8)(v >> 8); x.b[1] = (UInt8)(v & 0xFF); return(x); }

typedef struct
	{
	OTLIFO serviceinfolist;
	Boolean headerPrinted;
	Boolean lostRecords;
	} SearcherServices;

typedef struct
	{
	SearcherServices *services;
	char name[kDNSServiceMaxDomainName];
	char type[kDNSServiceMaxDomainName];
	char domn[kDNSServiceMaxDomainName];
	char host[kDNSServiceMaxDomainName];
	char text[kDNSServiceMaxDomainName];
	InetHost      address;
	mDNSOpaque16  notAnIntPort;
	DNSServiceRef sdRef;
	Boolean       add;
	Boolean       dom;
	OTLink        link;
	} linkedServiceInfo;

static SearcherServices services;

// PrintServiceInfo prints the service information to standard out
// A real application might want to do something else with the information
static void PrintServiceInfo(SearcherServices *services)
	{
	OTLink *link = OTReverseList(OTLIFOStealList(&services->serviceinfolist));
	
	while (link)
		{
		linkedServiceInfo *s = OTGetLinkObject(link, linkedServiceInfo, link);

		if (!services->headerPrinted)
			{
			printf("%-55s Type             Domain         Target Host     IP Address      Port Info\n", "Name");
			services->headerPrinted = true;
			}

		if (s->dom)
			{
			if (s->add) printf("%-55s available for browsing\n", s->domn);
			else        printf("%-55s no longer available for browsing\n", s->domn);
			}
		else
			{
			char ip[16];
			unsigned char *p = (unsigned char *)&s->address;
			sprintf(ip, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
			printf("%-55s %-16s %-14s ", s->name, s->type, s->domn);
			if (s->add) printf("%-15s %-15s %5d %s\n", s->host, ip, mDNSVal16(s->notAnIntPort), s->text);
			else        printf("Removed\n");
			}

		link = link->fNext;
		OTFreeMem(s);
		}
	}

static void FoundInstanceAddress(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode,
	const char *fullname, uint16_t rrtype, uint16_t rrclass, uint16_t rdlen, const void *rdata, uint32_t ttl, void *context)
	{
	linkedServiceInfo *info = (linkedServiceInfo *)context;
	SearcherServices *services = info->services;
	(void)sdRef;			// Unused
	(void)interfaceIndex;	// Unused
	(void)fullname;			// Unused
	(void)ttl;				// Unused
	if (errorCode == kDNSServiceErr_NoError)
		if (flags & kDNSServiceFlagsAdd)
			if (rrclass == ns_c_in && rrtype == ns_t_a && rdlen == sizeof(info->address))
				{
				memcpy(&info->address, rdata, sizeof(info->address));
				DNSServiceRefDeallocate(info->sdRef);
				OTLIFOEnqueue(&services->serviceinfolist, &info->link);
				}
	}

static void FoundInstanceInfo(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
	DNSServiceErrorType errorCode, const char *fullname, const char *hosttarget, uint16_t notAnIntPort,
	uint16_t txtLen, const unsigned char *txtRecord, void *context)
	{
	linkedServiceInfo *info = (linkedServiceInfo *)context;
	SearcherServices *services = info->services;
	(void)sdRef;			// Unused
	(void)flags;			// Unused
	(void)interfaceIndex;	// Unused
	(void)errorCode;		// Unused
	(void)fullname;			// Unused
	strcpy(info->host, hosttarget);
	if (txtLen == 0) info->text[0] = 0;
	else
		{
		strncpy(info->text, (char *)txtRecord+1, txtRecord[0]);
		info->text[txtRecord[0]] = 0;
		}
	info->notAnIntPort.NotAnInteger = notAnIntPort;
	DNSServiceRefDeallocate(info->sdRef);
	DNSServiceQueryRecord(&info->sdRef, 0, 0, info->host, ns_t_a, ns_c_in, FoundInstanceAddress, info);
	}

// When a new named instance of a service is found, FoundInstance() is called.
// In this sample code we turn around and immediately to a DNSServiceResolve() to resolve that service name
// to find its target host, port, and txtinfo, but a normal browing application would just display the name.
// Resolving every single thing you find can be quite hard on the network, so you shouldn't do this
// in a real application. Defer resolving until the client has picked which instance from the
// long list of services is the one they want to use, and then resolve only that one.
static void FoundInstance(DNSServiceRef client, DNSServiceFlags flags, uint32_t interface, DNSServiceErrorType errorCode,
    const char *replyName, const char *replyType, const char *replyDomain, void *context)
	{
#pragma unused(client, interface, errorCode)
	SearcherServices *services = (SearcherServices *)context;
	linkedServiceInfo *info;

	if (!services) { DebugStr("\pFoundInstance: services is NULL"); return; }
	
	info = (linkedServiceInfo *)OTAllocMem(sizeof(linkedServiceInfo));
	if (!info) { services->lostRecords = true; return; }
	
	info->services = services;
	strcpy(info->name, replyName);
	strcpy(info->type, replyType);
	strcpy(info->domn, replyDomain);
	info->text[0] = 0;
	info->add = (flags & kDNSServiceFlagsAdd) ? true : false;
	info->dom = false;
	
	if (!info->add)	// If TTL == 0 we're deleting a service,
		OTLIFOEnqueue(&services->serviceinfolist, &info->link);
	else								// else we're adding a new service
		DNSServiceResolve(&info->sdRef, 0, 0, info->name, info->type, info->domn, FoundInstanceInfo, info);
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
	void *tempmem;
	DNSServiceRef sdRef;
	DNSServiceErrorType dse;

	SIOUXSettings.asktosaveonclose = false;
	SIOUXSettings.userwindowtitle  = "\pMulticast DNS Searcher";
	SIOUXSettings.rows             = 40;
	SIOUXSettings.columns          = 160;

	printf("DNS-SD Search Client\n\n");
	printf("This software reports errors using MacsBug breaks,\n");
	printf("so if you don't have MacsBug installed your Mac may crash.\n\n");
	printf("******************************************************************************\n\n");

	if (DNSServiceBrowse == (void*)kUnresolvedCFragSymbolAddress)
		{
		printf("Before you can use mDNS/DNS-SD clients, you need to place the \n");
		printf("\"Multicast DNS & DNS-SD\" Extension in the Extensions Folder and restart\n");
		return(-1);
		}

	err = InitOpenTransport();
	if (err) { printf("InitOpenTransport failed %d", err); return(err); }

	// Make sure OT has a large enough memory pool for us to draw from at OTNotifier (interrupt) time
	tempmem = OTAllocMem(0x10000);
	if (tempmem) OTFreeMem(tempmem);
	else printf("**** Warning: OTAllocMem couldn't pre-allocate 64K for us.\n");

	services.serviceinfolist.fHead = NULL;
	services.headerPrinted         = false;
	services.lostRecords           = false;

	printf("Sending mDNS service lookup queries and waiting for responses...\n\n");
    dse = DNSServiceBrowse(&sdRef, 0, 0, "_http._tcp", "", FoundInstance, &services);
	if (dse == kDNSServiceErr_NoError)
		{
		while (!YieldSomeTime(35))
			{
			if (services.serviceinfolist.fHead)
				PrintServiceInfo(&services);

			if (services.lostRecords)
				{
				services.lostRecords = false;
				printf("**** Warning: Out of memory: Records have been missed.\n");
				}
			}
		}

	DNSServiceRefDeallocate(sdRef);
	CloseOpenTransport();
	return(0);
	}
