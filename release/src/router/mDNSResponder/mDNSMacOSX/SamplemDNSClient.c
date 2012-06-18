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
 *
 * Formatting notes:
 * This code follows the "Whitesmiths style" C indentation rules. Plenty of discussion
 * on C indentation can be found on the web, such as <http://www.kafejo.com/komp/1tbs.htm>,
 * but for the sake of brevity here I will say just this: Curly braces are not syntactially
 * part of an "if" statement; they are the beginning and ending markers of a compound statement;
 * therefore common sense dictates that if they are part of a compound statement then they
 * should be indented to the same level as everything else in that compound statement.
 * Indenting curly braces at the same level as the "if" implies that curly braces are
 * part of the "if", which is false. (This is as misleading as people who write "char* x,y;"
 * thinking that variables x and y are both of type "char*" -- and anyone who doesn't
 * understand why variable y is not of type "char*" just proves the point that poor code
 * layout leads people to unfortunate misunderstandings about how the C language really works.)

	Change History (most recent first):

$Log: SamplemDNSClient.c,v $
Revision 1.56  2008/10/22 02:59:58  mkrochma
<rdar://problem/6309616> Fix errors compiling mDNS tool caused by BIND8 removal

Revision 1.55  2008/09/15 23:52:30  cheshire
<rdar://problem/6218902> mDNSResponder-177 fails to compile on Linux with .desc pseudo-op
Made __crashreporter_info__ symbol conditional, so we only use it for OS X build

Revision 1.54  2007/11/30 23:39:55  cheshire
Fixed compile warning: declaration of 'client' shadows a global declaration

Revision 1.53  2007/09/18 19:09:02  cheshire
<rdar://problem/5489549> mDNSResponderHelper (and other binaries) missing SCCS version strings

Revision 1.52  2007/03/06 22:45:52  cheshire

<rdar://problem/4138615> argv buffer overflow issues

Revision 1.51  2007/02/13 18:56:45  cheshire
<rdar://problem/4993485> Mach mDNS tool inconsistent with UDS-based dns-sd tool
(missing domain should mean "system default(s)", not "local")

Revision 1.50  2007/01/05 08:30:47  cheshire
Trim excessive "$Log" checkin history from before 2006
(checkin history still available via "cvs log ..." of course)

Revision 1.49  2007/01/04 21:54:49  cheshire
Fix compile warnings related to the deprecated Mach-based API

Revision 1.48  2006/08/14 23:24:39  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.47  2006/01/10 02:29:22  cheshire
<rdar://problem/4403861> Cosmetic IPv6 address display problem in mDNS test tool

*/

#include <libc.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <CoreFoundation/CoreFoundation.h>

// We already know this tool is using the old deprecated API (that's its purpose)
// Since we compile with all warnings treated as errors, we have to turn off the warnings here or the project won't compile
#include <AvailabilityMacros.h>
#undef AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER_BUT_DEPRECATED
#define AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER_BUT_DEPRECATED
#undef AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_3
#define AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_3

#include <DNSServiceDiscovery/DNSServiceDiscovery.h>

//*************************************************************************************************************
// Globals

typedef union { unsigned char b[2]; unsigned short NotAnInteger; } Opaque16;

static char operation;
static dns_service_discovery_ref client = NULL;
static int num_printed;
static char addtest = 0;
static DNSRecordReference record;
static char myhinfo9[11] = "\003Mac\006OS 9.2";
static char myhinfoX[ 9] = "\003Mac\004OS X";
static char updatetest[3] = "\002AA";
static char bigNULL[4096];

//*************************************************************************************************************
// Supporting Utility Functions
//
// This code takes care of:
// 1. Extracting the mach_port_t from the dns_service_discovery_ref
// 2. Making a CFMachPortRef from it
// 3. Making a CFRunLoopSourceRef from that
// 4. Adding that source to the current RunLoop
// 5. and passing the resulting messages back to DNSServiceDiscovery_handleReply() for processing
//
// Code that's not based around a CFRunLoop will need its own mechanism to receive Mach messages
// from the mDNSResponder daemon and pass them to the DNSServiceDiscovery_handleReply() routine.
// (There is no way to automate this, because it varies depending on the application's existing
// event handling model.)

static void MyHandleMachMessage(CFMachPortRef port, void *msg, CFIndex size, void *info)
	{
	(void)port;	// Unused
	(void)size;	// Unused
	(void)info;	// Unused
	DNSServiceDiscovery_handleReply(msg);
	}

static int AddDNSServiceClientToRunLoop(dns_service_discovery_ref c)
	{
	mach_port_t port = DNSServiceDiscoveryMachPort(c);
	if (!port)
		return(-1);
	else
		{
		CFMachPortContext  context    = { 0, 0, NULL, NULL, NULL };
		Boolean            shouldFreeInfo;
		CFMachPortRef      cfMachPort = CFMachPortCreateWithPort(kCFAllocatorDefault, port, MyHandleMachMessage, &context, &shouldFreeInfo);
		CFRunLoopSourceRef rls        = CFMachPortCreateRunLoopSource(NULL, cfMachPort, 0);
		CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
		CFRelease(rls);
		return(0);
		}
	}

//*************************************************************************************************************
// Sample callback functions for each of the operation types

static void printtimestamp(void)
	{
	struct timeval tv;
	struct tm tm;
	gettimeofday(&tv, NULL);
	localtime_r((time_t*)&tv.tv_sec, &tm);
	printf("%2d:%02d:%02d.%03d  ", tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec/1000);
	}

#define DomainMsg(X) ((X) == DNSServiceDomainEnumerationReplyAddDomain        ? "Added"     :          \
					  (X) == DNSServiceDomainEnumerationReplyAddDomainDefault ? "(Default)" :          \
					  (X) == DNSServiceDomainEnumerationReplyRemoveDomain     ? "Removed"   : "Unknown")

static void regdom_reply(DNSServiceDomainEnumerationReplyResultType resultType, const char *replyDomain,
	DNSServiceDiscoveryReplyFlags flags, void *context)
	{
	(void)context; // Unused
	printtimestamp();
	printf("Recommended Registration Domain %s %s", replyDomain, DomainMsg(resultType));
	if (flags) printf(" Flags: %X", flags);
	printf("\n");
	}

static void browsedom_reply(DNSServiceDomainEnumerationReplyResultType resultType, const char *replyDomain,
	DNSServiceDiscoveryReplyFlags flags, void *context)
	{
	(void)context; // Unused
	printtimestamp();
	printf("Recommended Browsing Domain %s %s", replyDomain, DomainMsg(resultType));
	if (flags) printf(" Flags: %X", flags);
	printf("\n");
	}

static void browse_reply(DNSServiceBrowserReplyResultType resultType,
	const char *replyName, const char *replyType, const char *replyDomain, DNSServiceDiscoveryReplyFlags flags, void *context)
	{
	char *op = (resultType == DNSServiceBrowserReplyAddInstance) ? "Add" : "Rmv";
	(void)context; // Unused
	if (num_printed++ == 0) printf("Timestamp     A/R Flags %-24s %-24s %s\n", "Domain", "Service Type", "Instance Name");
	printtimestamp();
	printf("%s%6X %-24s %-24s %s\n", op, flags, replyDomain, replyType, replyName);
	}

static void resolve_reply(struct sockaddr *interface, struct sockaddr *address, const char *txtRecord, DNSServiceDiscoveryReplyFlags flags, void *context)
	{
	(void)interface; // Unused
	(void)context; // Unused
	if (address->sa_family != AF_INET && address->sa_family != AF_INET6)
		printf("Unknown address family %d\n", address->sa_family);
	else
		{
		const char *src = txtRecord;
		printtimestamp();

		if (address->sa_family == AF_INET)
			{
			struct sockaddr_in *ip = (struct sockaddr_in *)address;
			union { uint32_t l; u_char b[4]; } addr = { ip->sin_addr.s_addr };
			union { uint16_t s; u_char b[2]; } port = { ip->sin_port };
			uint16_t PortAsNumber = ((uint16_t)port.b[0]) << 8 | port.b[1];
			char ipstring[16];
			sprintf(ipstring, "%d.%d.%d.%d", addr.b[0], addr.b[1], addr.b[2], addr.b[3]);
			printf("Service can be reached at   %-15s:%u", ipstring, PortAsNumber);
			}
		else if (address->sa_family == AF_INET6)
			{
			struct sockaddr_in6 *ip6 = (struct sockaddr_in6 *)address;
			u_int8_t *b = ip6->sin6_addr.__u6_addr.__u6_addr8;
			union { uint16_t s; u_char b[2]; } port = { ip6->sin6_port };
			uint16_t PortAsNumber = ((uint16_t)port.b[0]) << 8 | port.b[1];
			char ipstring[40];
			char ifname[IF_NAMESIZE + 1] = "";
			sprintf(ipstring, "%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X",
				b[0x0], b[0x1], b[0x2], b[0x3], b[0x4], b[0x5], b[0x6], b[0x7],
				b[0x8], b[0x9], b[0xA], b[0xB], b[0xC], b[0xD], b[0xE], b[0xF]);
			if (ip6->sin6_scope_id) { ifname[0] = '%';  if_indextoname(ip6->sin6_scope_id, &ifname[1]); }
			printf("%s%s:%u", ipstring, ifname, PortAsNumber);
			}
		if (flags) printf(" Flags: %X", flags);
		if (*src)
			{
			char txtInfo[64];								// Display at most first 64 characters of TXT record
			char *dst = txtInfo;
			const char *const lim = &txtInfo[sizeof(txtInfo)];
			while (*src && dst < lim-1)
				{
				if (*src == '\\') *dst++ = '\\';			// '\' displays as "\\"
				if (*src >= ' ') *dst++ = *src++;			// Display normal characters as-is
				else
					{
					*dst++ = '\\';							// Display a backslash
					if (*src ==    1) *dst++ = ' ';			// String boundary displayed as "\ "
					else									// Other chararacters displayed as "\0xHH"
						{
						static const char hexchars[16] = "0123456789ABCDEF";
						*dst++ = '0';
						*dst++ = 'x';
						*dst++ = hexchars[*src >> 4];
						*dst++ = hexchars[*src & 0xF];
						}
					src++;
					}
				}
			*dst++ = 0;
			printf(" TXT %s", txtInfo);
			}
		printf("\n");
		}
	}

static void myCFRunLoopTimerCallBack(CFRunLoopTimerRef timer, void *info)
	{
	(void)timer;	// Parameter not used
	(void)info;		// Parameter not used
	
	switch (operation)
		{
		case 'A':
			{
			switch (addtest)
				{
				case 0: printf("Adding Test HINFO record\n");
						record = DNSServiceRegistrationAddRecord(client, ns_t_hinfo, sizeof(myhinfo9), &myhinfo9[0], 120);
						addtest = 1;
						break;
				case 1: printf("Updating Test HINFO record\n");
						DNSServiceRegistrationUpdateRecord(client, record, sizeof(myhinfoX), &myhinfoX[0], 120);
						addtest = 2;
						break;
				case 2: printf("Removing Test HINFO record\n");
						DNSServiceRegistrationRemoveRecord(client, record);
						addtest = 0;
						break;
				}
			}
			break;

		case 'U':
			{
			if (updatetest[1] != 'Z') updatetest[1]++;
			else                      updatetest[1] = 'A';
			updatetest[0] = 3 - updatetest[0];
			updatetest[2] = updatetest[1];
			printf("Updating Test TXT record to %c\n", updatetest[1]);
			DNSServiceRegistrationUpdateRecord(client, 0, 1+updatetest[0], &updatetest[0], 120);
			}
			break;

		case 'N':
			{
			printf("Adding big NULL record\n");
			DNSServiceRegistrationAddRecord(client, ns_t_null, sizeof(bigNULL), &bigNULL[0], 120);
			CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
			}
			break;
		}
	}

static void reg_reply(DNSServiceRegistrationReplyErrorType errorCode, void *context)
	{
	(void)context; // Unused
	printf("Got a reply from the server: ");
	switch (errorCode)
		{
		case kDNSServiceDiscoveryNoError:      printf("Name now registered and active\n"); break;
		case kDNSServiceDiscoveryNameConflict: printf("Name in use, please choose another\n"); exit(-1);
		default:                               printf("Error %d\n", errorCode); return;
		}

	if (operation == 'A' || operation == 'U' || operation == 'N')
		{
		CFRunLoopTimerContext myCFRunLoopTimerContext = { 0, 0, NULL, NULL, NULL };
		CFRunLoopTimerRef timer = CFRunLoopTimerCreate(kCFAllocatorDefault,
			CFAbsoluteTimeGetCurrent() + 5.0, 5.0, 0, 1,	// Next fire time, periodic interval, flags, and order
								myCFRunLoopTimerCallBack, &myCFRunLoopTimerContext);
		CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
		}
	}

//*************************************************************************************************************
// The main test function

int main(int argc, char **argv)
	{
	const char *progname = strrchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0];
	char *d;
	setlinebuf(stdout);				// Want to see lines as they appear, not block buffered

	if (argc < 2) goto Fail;		// Minimum command line is the command name and one argument
	operation = getopt(argc, (char * const *)argv, "EFBLRAUNTMI");
	if (operation == -1) goto Fail;

	switch (operation)
		{
		case 'E':	printf("Looking for recommended registration domains:\n");
					client = DNSServiceDomainEnumerationCreate(1, regdom_reply, nil);
					break;

		case 'F':	printf("Looking for recommended browsing domains:\n");
					client = DNSServiceDomainEnumerationCreate(0, browsedom_reply, nil);
					break;

		case 'B':	if (argc < optind+1) goto Fail;
					d = (argc < optind+2) ? "" : argv[optind+1];	// Missing domain argument is the same as empty string i.e. use system default(s)
					if (d[0] == '.' && d[1] == 0) d[0] = 0;	// We allow '.' on the command line as a synonym for empty string
					printf("Browsing for %s%s\n", argv[optind+0], d);
					client = DNSServiceBrowserCreate(argv[optind+0], d, browse_reply, nil);
					break;

		case 'L':	if (argc < optind+2) goto Fail;
					d = (argc < optind+3) ? "" : argv[optind+2];
					if (d[0] == '.' && d[1] == 0) d = "local";   // We allow '.' on the command line as a synonym for "local"
					printf("Lookup %s.%s%s\n", argv[optind+0], argv[optind+1], d);
					client = DNSServiceResolverResolve(argv[optind+0], argv[optind+1], d, resolve_reply, nil);
					break;

		case 'R':	if (argc < optind+4) goto Fail;
					{
					char *nam = argv[optind+0];
					char *typ = argv[optind+1];
					char *dom = argv[optind+2];
					uint16_t PortAsNumber = atoi(argv[optind+3]);
					Opaque16 registerPort = { { PortAsNumber >> 8, PortAsNumber & 0xFF } };
					char txt[2048];
					char *ptr = txt;
					int i;

					if (nam[0] == '.' && nam[1] == 0) nam[0] = 0;	// We allow '.' on the command line as a synonym for empty string
					if (dom[0] == '.' && dom[1] == 0) dom[0] = 0;	// We allow '.' on the command line as a synonym for empty string

					// Copy all the TXT strings into one C string separated by ASCII-1 delimiters
					for (i = optind+4; i < argc; i++)
						{
						int len = strlen(argv[i]);
						if (len > 255 || ptr + len + 1 >= txt + sizeof(txt)) break;
						strcpy(ptr, argv[i]);
						ptr += len;
						*ptr++ = 1;
						}
					if (ptr > txt) ptr--;
					*ptr = 0;
					
					printf("Registering Service %s.%s%s port %s %s\n", nam, typ, dom, argv[optind+3], txt);
					client = DNSServiceRegistrationCreate(nam, typ, dom, registerPort.NotAnInteger, txt, reg_reply, nil);
					break;
					}

		case 'A':
		case 'U':
		case 'N':	{
					Opaque16 registerPort = { { 0x12, 0x34 } };
					static const char TXT[] = "First String\001Second String\001Third String";
					printf("Registering Service Test._testupdate._tcp.local.\n");
					client = DNSServiceRegistrationCreate("Test", "_testupdate._tcp.", "", registerPort.NotAnInteger, TXT, reg_reply, nil);
					break;
					}

		case 'T':	{
					Opaque16 registerPort = { { 0x23, 0x45 } };
					char TXT[1000];
					unsigned int i;
					for (i=0; i<sizeof(TXT)-1; i++)
						if ((i & 0x1F) == 0x1F) TXT[i] = 1; else TXT[i] = 'A' + (i >> 5);
					TXT[i] = 0;
					printf("Registering Service Test._testlargetxt._tcp.local.\n");
					client = DNSServiceRegistrationCreate("Test", "_testlargetxt._tcp.", "", registerPort.NotAnInteger, TXT, reg_reply, nil);
					break;
					}

		case 'M':	{
					pid_t pid = getpid();
					Opaque16 registerPort = { { pid >> 8, pid & 0xFF } };
					static const char TXT1[] = "First String\001Second String\001Third String";
					static const char TXT2[] = "\x0D" "Fourth String" "\x0C" "Fifth String" "\x0C" "Sixth String";
					printf("Registering Service Test._testdualtxt._tcp.local.\n");
					client = DNSServiceRegistrationCreate("", "_testdualtxt._tcp.", "", registerPort.NotAnInteger, TXT1, reg_reply, nil);
					// use "sizeof(TXT2)-1" because we don't wan't the C compiler's null byte on the end of the string
					record = DNSServiceRegistrationAddRecord(client, ns_t_txt, sizeof(TXT2)-1, TXT2, 120);
					break;
					}

		case 'I':	{
					pid_t pid = getpid();
					Opaque16 registerPort = { { pid >> 8, pid & 0xFF } };
					static const char TXT[] = "\x09" "Test Data";
					printf("Registering Service Test._testtxt._tcp.local.\n");
					client = DNSServiceRegistrationCreate("", "_testtxt._tcp.", "", registerPort.NotAnInteger, "", reg_reply, nil);
					if (client) DNSServiceRegistrationUpdateRecord(client, 0, 1+TXT[0], &TXT[0], 120);
					break;
					}

		default: goto Exit;
		}

	if (!client) { fprintf(stderr, "DNSService call failed\n"); return (-1); }
	if (AddDNSServiceClientToRunLoop(client) != 0) { fprintf(stderr, "AddDNSServiceClientToRunLoop failed\n"); return (-1); }
	printf("Talking to DNS SD Daemon at Mach port %d\n", DNSServiceDiscoveryMachPort(client));
	CFRunLoopRun();
	
	// Be sure to deallocate the dns_service_discovery_ref when you're finished
	// Note: What other cleanup has to be done here?
	// We should probably invalidate, remove and release our CFRunLoopSourceRef?
	DNSServiceDiscoveryDeallocate(client);
	
Exit:
	return 0;

Fail:
	fprintf(stderr, "%s -E                  (Enumerate recommended registration domains)\n", progname);
	fprintf(stderr, "%s -F                      (Enumerate recommended browsing domains)\n", progname);
	fprintf(stderr, "%s -B        <Type> <Domain>        (Browse for services instances)\n", progname);
	fprintf(stderr, "%s -L <Name> <Type> <Domain>           (Look up a service instance)\n", progname);
	fprintf(stderr, "%s -R <Name> <Type> <Domain> <Port> [<TXT>...] (Register a service)\n", progname);
	fprintf(stderr, "%s -A                      (Test Adding/Updating/Deleting a record)\n", progname);
	fprintf(stderr, "%s -U                                  (Test updating a TXT record)\n", progname);
	fprintf(stderr, "%s -N                             (Test adding a large NULL record)\n", progname);
	fprintf(stderr, "%s -T                            (Test creating a large TXT record)\n", progname);
	fprintf(stderr, "%s -M      (Test creating a registration with multiple TXT records)\n", progname);
	fprintf(stderr, "%s -I   (Test registering and then immediately updating TXT record)\n", progname);
	return 0;
	}

// Note: The C preprocessor stringify operator ('#') makes a string from its argument, without macro expansion
// e.g. If "version" is #define'd to be "4", then STRINGIFY_AWE(version) will return the string "version", not "4"
// To expand "version" to its value before making the string, use STRINGIFY(version) instead
#define STRINGIFY_ARGUMENT_WITHOUT_EXPANSION(s) #s
#define STRINGIFY(s) STRINGIFY_ARGUMENT_WITHOUT_EXPANSION(s)

// NOT static -- otherwise the compiler may optimize it out
// The "@(#) " pattern is a special prefix the "what" command looks for
const char VersionString_SCCS[] = "@(#) mDNS " STRINGIFY(mDNSResponderVersion) " (" __DATE__ " " __TIME__ ")";

#if _BUILDING_XCODE_PROJECT_
// If the process crashes, then this string will be magically included in the automatically-generated crash log
const char *__crashreporter_info__ = VersionString_SCCS + 5;
asm(".desc ___crashreporter_info__, 0x10");
#endif
