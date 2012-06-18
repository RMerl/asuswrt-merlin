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

$Log: ProxyResponder.c,v $
Revision 1.45  2008/11/04 19:46:01  cheshire
Updated comment about MAX_ESCAPED_DOMAIN_NAME size (should be 1009, not 1005)

Revision 1.44  2007/04/22 20:16:25  cheshire
Fix compiler errors (const parameter declarations)

Revision 1.43  2007/04/16 20:49:39  cheshire
Fix compile errors for mDNSPosix build

Revision 1.42  2007/03/06 22:45:53  cheshire

<rdar://problem/4138615> argv buffer overflow issues

Revision 1.41  2007/02/28 01:51:22  cheshire
Added comment about reverse-order IP address

Revision 1.40  2007/01/05 04:32:13  cheshire
Change "(domainname *)" cast to "(const domainname *)"

Revision 1.39  2006/08/14 23:24:46  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.38  2006/06/12 18:22:42  cheshire
<rdar://problem/4580067> mDNSResponder building warnings under Red Hat 64-bit (LP64) Linux

Revision 1.37  2006/02/23 23:38:43  cheshire
<rdar://problem/4427969> On FreeBSD 4 "arpa/inet.h" requires "netinet/in.h" be included first

Revision 1.36  2005/08/04 03:12:47  mkrochma
<rdar://problem/4199236> Register reverse PTR record using multicast

Revision 1.35  2004/12/16 20:17:11  cheshire
<rdar://problem/3324626> Cache memory management improvements

Revision 1.34  2004/12/01 04:27:28  cheshire
<rdar://problem/3872803> Darwin patches for Solaris and Suse
Don't use uint32_t, etc. -- they require stdint.h, which doesn't exist on FreeBSD 4.x, Solaris, etc.

Revision 1.33  2004/11/30 22:37:01  cheshire
Update copyright dates and add "Mode: C; tab-width: 4" headers

Revision 1.32  2004/10/26 03:59:41  cheshire
Update comments

Revision 1.31  2004/09/17 01:08:53  cheshire
Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
  The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
  declared in that file are ONLY appropriate to single-address-space embedded applications.
  For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.

Revision 1.30  2004/09/17 00:31:52  cheshire
For consistency with ipv6, renamed rdata field 'ip' to 'ipv4'

Revision 1.29  2004/09/16 01:58:22  cheshire
Fix compiler warnings

Revision 1.28  2004/06/25 00:26:27  rpantos
Changes to fix the Posix build on Solaris.

Revision 1.27  2004/03/12 08:03:14  cheshire
Update comments

Revision 1.26  2004/01/25 00:00:39  cheshire
Change to use mDNSOpaque16fromIntVal() instead of shifting and masking

Revision 1.25  2003/12/08 20:47:02  rpantos
Add support for mDNSResponder on Linux.

Revision 1.24  2003/11/14 21:27:09  cheshire
<rdar://problem/3484766>: Security: Crashing bug in mDNSResponder
Fix code that should use buffer size MAX_ESCAPED_DOMAIN_NAME (1009) instead of 256-byte buffers.

Revision 1.23  2003/10/30 19:39:28  cheshire
Fix warnings on certain compilers

Revision 1.22  2003/08/14 02:19:55  cheshire
<rdar://problem/3375491> Split generic ResourceRecord type into two separate types: AuthRecord and CacheRecord

Revision 1.21  2003/08/12 19:56:26  cheshire
Update to APSL 2.0

Revision 1.20  2003/07/23 00:00:04  cheshire
Add comments

Revision 1.19  2003/07/15 01:55:16  cheshire
<rdar://problem/3315777> Need to implement service registration with subtypes

Revision 1.18  2003/07/02 21:19:58  cheshire
<rdar://problem/3313413> Update copyright notices, etc., in source code comments

Revision 1.17  2003/05/26 03:21:29  cheshire
Tidy up address structure naming:
mDNSIPAddr         => mDNSv4Addr (for consistency with mDNSv6Addr)
mDNSAddr.addr.ipv4 => mDNSAddr.ip.v4
mDNSAddr.addr.ipv6 => mDNSAddr.ip.v6

Revision 1.16  2003/05/26 03:01:28  cheshire
<rdar://problem/3268904> sprintf/vsprintf-style functions are unsafe; use snprintf/vsnprintf instead

Revision 1.15  2003/05/06 00:00:50  cheshire
<rdar://problem/3248914> Rationalize naming of domainname manipulation functions

Revision 1.14  2003/04/25 01:45:57  cheshire
<rdar://problem/3240002> mDNS_RegisterNoSuchService needs to include a host name

Revision 1.13  2003/04/18 22:46:12  cheshire
Fix mistake in 1.8 -- INADDR_NONE is 0xFFFFFFFF, not 0

Revision 1.12  2003/04/16 02:11:07  cheshire
Fixed mDNS_RegisterNoSuchService non-existence function so that it works again

Revision 1.11  2003/03/31 22:49:35  cheshire
Add "$Log" header

 */

#include <stdio.h>				// For printf()
#include <stdlib.h>				// For exit() etc.
#include <string.h>				// For strlen() etc.
#include <unistd.h>				// For select()
#include <signal.h>				// For SIGINT, SIGTERM
#include <errno.h>				// For errno, EINTR
#include <netinet/in.h>			// For INADDR_NONE
#include <arpa/inet.h>			// For inet_addr()
#include <netdb.h>				// For gethostbyname()

#include "mDNSEmbeddedAPI.h"	// Defines the interface to the client layer above
#include "mDNSPosix.h"			// Defines the specific types needed to run mDNS on this platform
#include "ExampleClientApp.h"

// Compatibility workaround: Solaris 2.5 has no INADDR_NONE
#ifndef	INADDR_NONE
#define	INADDR_NONE	(mDNSu32)0xffffffff
#endif

//*************************************************************************************************************
// Globals
static mDNS mDNSStorage;       // mDNS core uses this to store its globals
static mDNS_PlatformSupport PlatformStorage;  // Stores this platform's globals
mDNSexport const char ProgramName[] = "mDNSProxyResponderPosix";

//*************************************************************************************************************
// Proxy Host Registration

typedef struct
	{
	mDNSv4Addr ip;
	domainlabel hostlabel;		// Conforms to standard DNS letter-digit-hyphen host name rules
	AuthRecord RR_A;		// 'A' (address) record for our ".local" name
	AuthRecord RR_PTR;		// PTR (reverse lookup) record
	} ProxyHost;

mDNSlocal void HostNameCallback(mDNS *const m, AuthRecord *const rr, mStatus result)
	{
	ProxyHost *f = (ProxyHost*)rr->RecordContext;
	if (result == mStatus_NoError)
		debugf("Host name successfully registered: %##s", rr->resrec.name->c);
	else
		{
		debugf("Host name conflict for %##s", rr->resrec.name->c);
		mDNS_Deregister(m, &f->RR_A);
		mDNS_Deregister(m, &f->RR_PTR);
		exit(-1);
		}
	}

mDNSlocal mStatus mDNS_RegisterProxyHost(mDNS *m, ProxyHost *p)
	{
	char buffer[32];
	
	mDNS_SetupResourceRecord(&p->RR_A,   mDNSNULL, mDNSInterface_Any, kDNSType_A,   60, kDNSRecordTypeUnique,      HostNameCallback, p);
	mDNS_SetupResourceRecord(&p->RR_PTR, mDNSNULL, mDNSInterface_Any, kDNSType_PTR, 60, kDNSRecordTypeKnownUnique, HostNameCallback, p);

	p->RR_A.namestorage.c[0] = 0;
	AppendDomainLabel(&p->RR_A.namestorage, &p->hostlabel);
	AppendLiteralLabelString(&p->RR_A.namestorage, "local");

	// Note: This is reverse order compared to a normal dotted-decimal IP address, so we can't use our customary "%.4a" format code
	mDNS_snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d.in-addr.arpa.", p->ip.b[3], p->ip.b[2], p->ip.b[1], p->ip.b[0]);
	MakeDomainNameFromDNSNameString(&p->RR_PTR.namestorage, buffer);
	p->RR_PTR.ForceMCast = mDNStrue; // This PTR points to our dot-local name, so don't ever try to write it into a uDNS server

	p->RR_A.  resrec.rdata->u.ipv4 = p->ip;
	AssignDomainName(&p->RR_PTR.resrec.rdata->u.name, p->RR_A.resrec.name);

	mDNS_Register(m, &p->RR_A);
	mDNS_Register(m, &p->RR_PTR);

	debugf("Made Proxy Host Records for %##s", p->RR_A.resrec.name->c);
	
	return(mStatus_NoError);
	}

//*************************************************************************************************************
// Service Registration

// This sample ServiceCallback just calls mDNS_RenameAndReregisterService to automatically pick a new
// unique name for the service. For a device such as a printer, this may be appropriate.
// For a device with a user interface, and a screen, and a keyboard, the appropriate
// response may be to prompt the user and ask them to choose a new name for the service.
mDNSlocal void ServiceCallback(mDNS *const m, ServiceRecordSet *const sr, mStatus result)
	{
	switch (result)
		{
		case mStatus_NoError:      debugf("Callback: %##s Name Registered",    sr->RR_SRV.resrec.name->c); break;
		case mStatus_NameConflict: debugf("Callback: %##s Name Conflict",      sr->RR_SRV.resrec.name->c); break;
		case mStatus_MemFree:      debugf("Callback: %##s Memory Free",        sr->RR_SRV.resrec.name->c); break;
		default:                   debugf("Callback: %##s Unknown Result %ld", sr->RR_SRV.resrec.name->c, result); break;
		}

	if (result == mStatus_NoError)
		{
		char buffer[MAX_ESCAPED_DOMAIN_NAME];
		ConvertDomainNameToCString(sr->RR_SRV.resrec.name, buffer);
		printf("Service %s now registered and active\n", buffer);
		}

	if (result == mStatus_NameConflict)
		{
		char buffer1[MAX_ESCAPED_DOMAIN_NAME], buffer2[MAX_ESCAPED_DOMAIN_NAME];
		ConvertDomainNameToCString(sr->RR_SRV.resrec.name, buffer1);
		mDNS_RenameAndReregisterService(m, sr, mDNSNULL);
		ConvertDomainNameToCString(sr->RR_SRV.resrec.name, buffer2);
		printf("Name Conflict! %s renamed as %s\n", buffer1, buffer2);
		}
	}

// RegisterService() is a simple wrapper function which takes C string
// parameters, converts them to domainname parameters, and calls mDNS_RegisterService()
mDNSlocal void RegisterService(mDNS *m, ServiceRecordSet *recordset,
	const char name[], const char type[], const char domain[],
	const domainname *host, mDNSu16 PortAsNumber, int argc, char **argv)
	{
	domainlabel n;
	domainname t, d;
	unsigned char txtbuffer[1024], *bptr = txtbuffer;
	char buffer[MAX_ESCAPED_DOMAIN_NAME];

	MakeDomainLabelFromLiteralString(&n, name);
	MakeDomainNameFromDNSNameString(&t, type);
	MakeDomainNameFromDNSNameString(&d, domain);
	while (argc)
		{
		int len = strlen(argv[0]);
		if (len > 255 || bptr + 1 + len >= txtbuffer + sizeof(txtbuffer)) break;
		printf("STR: %s\n", argv[0]);
		bptr[0] = len;
		strcpy((char*)(bptr+1), argv[0]);
		bptr += 1 + len;
		argc--;
		argv++;
		}
	
	mDNS_RegisterService(m, recordset,
		&n, &t, &d,					// Name, type, domain
		host, mDNSOpaque16fromIntVal(PortAsNumber),
		txtbuffer, bptr-txtbuffer,	// TXT data, length
		mDNSNULL, 0,				// Subtypes
		mDNSInterface_Any,			// Interface ID
		ServiceCallback, mDNSNULL);	// Callback and context

	ConvertDomainNameToCString(recordset->RR_SRV.resrec.name, buffer);
	printf("Made Service Records for %s\n", buffer);
	}

//*************************************************************************************************************
// Service non-existence assertion
// (claiming a service name without actually providing a service at that name, to prevent anyone else using that name)
// This is useful to avoid confusion between similar services
// e.g. A printer that implements IPP printing service using the name "My Printer", but doesn't implement LPR service,
// should also claim the LPR service name "My Printer" to stop a different printer offering LPR service under the same name,
// since it would be confusing to users to have two equivalent services with the same name.

mDNSlocal void NoSuchServiceCallback(mDNS *const m, AuthRecord *const rr, mStatus result)
	{
	const domainname *proxyhostname = (const domainname *)rr->RecordContext;
	switch (result)
		{
		case mStatus_NoError:      debugf("Callback: %##s Name Registered",    rr->resrec.name->c); break;
		case mStatus_NameConflict: debugf("Callback: %##s Name Conflict",      rr->resrec.name->c); break;
		case mStatus_MemFree:      debugf("Callback: %##s Memory Free",        rr->resrec.name->c); break;
		default:                   debugf("Callback: %##s Unknown Result %ld", rr->resrec.name->c, result); break;
		}

	if (result == mStatus_NoError)
		{
		char buffer[MAX_ESCAPED_DOMAIN_NAME];
		ConvertDomainNameToCString(rr->resrec.name, buffer);
		printf("Non-existence assertion %s now registered and active\n", buffer);
		}

	if (result == mStatus_NameConflict)
		{
		domainlabel n;
		domainname t, d;
		char buffer1[MAX_ESCAPED_DOMAIN_NAME], buffer2[MAX_ESCAPED_DOMAIN_NAME];
		ConvertDomainNameToCString(rr->resrec.name, buffer1);
		DeconstructServiceName(rr->resrec.name, &n, &t, &d);
		IncrementLabelSuffix(&n, mDNStrue);
		mDNS_RegisterNoSuchService(m, rr, &n, &t, &d, proxyhostname, mDNSInterface_Any, NoSuchServiceCallback, mDNSNULL);
		ConvertDomainNameToCString(rr->resrec.name, buffer2);
		printf("Name Conflict! %s renamed as %s\n", buffer1, buffer2);
		}
	}

mDNSlocal void RegisterNoSuchService(mDNS *m, AuthRecord *const rr, domainname *proxyhostname,
	const char name[], const char type[], const char domain[])
	{
	domainlabel n;
	domainname t, d;
	char buffer[MAX_ESCAPED_DOMAIN_NAME];
	MakeDomainLabelFromLiteralString(&n, name);
	MakeDomainNameFromDNSNameString(&t, type);
	MakeDomainNameFromDNSNameString(&d, domain);
	mDNS_RegisterNoSuchService(m, rr, &n, &t, &d, proxyhostname, mDNSInterface_Any, NoSuchServiceCallback, proxyhostname);
	ConvertDomainNameToCString(rr->resrec.name, buffer);
	printf("Made Non-existence Record for %s\n", buffer);
	}

//*************************************************************************************************************
// Main

mDNSexport int main(int argc, char **argv)
	{
	mStatus			status;
	sigset_t		signals;
	
	if (argc < 3) goto usage;
	
	status = mDNS_Init(&mDNSStorage, &PlatformStorage,
		mDNS_Init_NoCache, mDNS_Init_ZeroCacheSize,
		mDNS_Init_DontAdvertiseLocalAddresses,
		mDNS_Init_NoInitCallback, mDNS_Init_NoInitCallbackContext);
	if (status) { fprintf(stderr, "Daemon start: mDNS_Init failed %d\n", (int)status); return(status); }

	mDNSPosixListenForSignalInEventLoop(SIGINT);
	mDNSPosixListenForSignalInEventLoop(SIGTERM);

	if (!strcmp(argv[1], "-"))
		{
		domainname proxyhostname;
		AuthRecord proxyrecord;
		if (argc < 5) goto usage;
		proxyhostname.c[0] = 0;
		AppendLiteralLabelString(&proxyhostname, argv[2]);
		AppendLiteralLabelString(&proxyhostname, "local");
		RegisterNoSuchService(&mDNSStorage, &proxyrecord, &proxyhostname, argv[3], argv[4], "local.");
		}
	else
		{
		ProxyHost proxyhost;
		ServiceRecordSet proxyservice;

		proxyhost.ip.NotAnInteger = inet_addr(argv[1]);
		if (proxyhost.ip.NotAnInteger == INADDR_NONE)	// INADDR_NONE is 0xFFFFFFFF
			{
			struct hostent *h = gethostbyname(argv[1]);
			if (h) proxyhost.ip.NotAnInteger = *(long*)h->h_addr;
			}
		if (proxyhost.ip.NotAnInteger == INADDR_NONE)	// INADDR_NONE is 0xFFFFFFFF
			{
			fprintf(stderr, "%s is not valid host address\n", argv[1]);
			return(-1);
			}
	
		MakeDomainLabelFromLiteralString(&proxyhost.hostlabel, argv[2]);

		mDNS_RegisterProxyHost(&mDNSStorage, &proxyhost);

		if (argc >=6)
			RegisterService(&mDNSStorage, &proxyservice, argv[3], argv[4], "local.",
							proxyhost.RR_A.resrec.name, atoi(argv[5]), argc-6, &argv[6]);
		}

	do 
		{
		struct timeval	timeout = { 0x3FFFFFFF, 0 };	// wait until SIGINT or SIGTERM
		mDNSBool		gotSomething;
		mDNSPosixRunEventLoopOnce(&mDNSStorage, &timeout, &signals, &gotSomething);
		}
	while ( !( sigismember( &signals, SIGINT) || sigismember( &signals, SIGTERM)));

	mDNS_Close(&mDNSStorage);

	return(0);

usage:
	fprintf(stderr, "%s ip hostlabel [srvname srvtype port txt [txt ...]]\n", argv[0]);
	fprintf(stderr, "ip        Real IP address (or valid host name) of the host where the service actually resides\n");
	fprintf(stderr, "hostlabel First label of the dot-local host name to create for this host, e.g. \"foo\" for \"foo.local.\"\n");
	fprintf(stderr, "srvname   Descriptive name of service, e.g. \"Stuart's Ink Jet Printer\"\n");
	fprintf(stderr, "srvtype   IANA service type, e.g. \"_ipp._tcp\" or \"_ssh._tcp\", etc.\n");
	fprintf(stderr, "port      Port number where the service resides (1-65535)\n");
	fprintf(stderr, "txt       Additional name/value pairs specified in service definition, e.g. \"pdl=application/postscript\"\n");
	fprintf(stderr, "e.g. %s 169.254.12.34 thehost                                (just create a dot-local host name)\n", argv[0]);
	fprintf(stderr, "or   %s 169.254.12.34 thehost \"My Printer\" _printer._tcp. 515 rp=lpt1 pdl=application/postscript\n", argv[0]);
	fprintf(stderr, "or   %s -             thehost \"My Printer\" _printer._tcp.           (assertion of non-existence)\n", argv[0]);
	return(-1);
	}
