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

$Log: PlatformCommon.c,v $
Revision 1.21  2009/04/11 00:20:24  jessic2
<rdar://problem/4426780> Daemon: Should be able to turn on LogOperation dynamically

Revision 1.20  2008/10/09 22:26:05  cheshire
Save space by not showing high-resolution timestamp in LogMsgNoIdent() lines

Revision 1.19  2008/07/14 17:43:36  mkrochma
Fix previous check in so connect still gets called

Revision 1.18  2008/07/12 17:19:41  mkrochma
<rdar://problem/6068351> mDNSResponder PlatformCommon.c uses sin_len even on non-compliant platforms

Revision 1.17  2008/03/05 00:19:09  cheshire
Conditionalize LogTimeStamps so it's specific to APPLE_OSX, for now

Revision 1.16  2008/02/26 21:47:45  cheshire
Added cast to avoid compiler warning

Revision 1.15  2008/02/26 21:42:26  cheshire
Added 'LogTimeStamps' option, to show ms-granularity timestamps on every log message

Revision 1.14  2007/12/03 18:37:26  cheshire
Moved mDNSPlatformWriteLogMsg & mDNSPlatformWriteDebugMsg
from mDNSMacOSX.c to PlatformCommon.c, so that Posix build can use them

Revision 1.13  2007/10/22 20:07:07  cheshire
Moved mDNSPlatformSourceAddrForDest from mDNSMacOSX.c to PlatformCommon.c so
Posix build can share the code (better than just pasting it into mDNSPosix.c)

Revision 1.12  2007/10/16 17:19:53  cheshire
<rdar://problem/3557903> Performance: Core code will not work on platforms with small stacks
Cut ReadDDNSSettingsFromConfFile stack from 2112 to 1104 bytes

Revision 1.11  2007/07/31 23:08:34  mcguire
<rdar://problem/5329542> BTMM: Make AutoTunnel mode work with multihoming

Revision 1.10  2007/07/11 02:59:58  cheshire
<rdar://problem/5303807> Register IPv6-only hostname and don't create port mappings for AutoTunnel services
Add AutoTunnel parameter to mDNS_SetSecretForDomain

Revision 1.9  2007/01/09 22:37:44  cheshire
Remove unused ClearDomainSecrets() function

Revision 1.8  2006/12/22 20:59:51  cheshire
<rdar://problem/4742742> Read *all* DNS keys from keychain,
 not just key for the system-wide default registration domain

Revision 1.7  2006/08/14 23:24:56  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.6  2005/04/08 21:30:16  ksekar
<rdar://problem/4007457> Compiling problems with mDNSResponder-98 on Solaris/Sparc v9
Patch submitted by Bernd Kuhls

Revision 1.5  2005/02/01 19:33:30  ksekar
<rdar://problem/3985239> Keychain format too restrictive

Revision 1.4  2005/01/19 19:19:21  ksekar
<rdar://problem/3960191> Need a way to turn off domain discovery

Revision 1.3  2004/12/13 17:46:52  cheshire
Use sizeof(buf) instead of fixed constant 1024

Revision 1.2  2004/12/01 03:30:29  cheshire
<rdar://problem/3889346> Add Unicast DNS support to mDNSPosix

Revision 1.1  2004/12/01 01:51:35  cheshire
Move ReadDDNSSettingsFromConfFile() from mDNSMacOSX.c to PlatformCommon.c

 */

#include <stdio.h>				// Needed for fopen() etc.
#include <unistd.h>				// Needed for close()
#include <string.h>				// Needed for strlen() etc.
#include <errno.h>				// Needed for errno etc.
#include <sys/socket.h>			// Needed for socket() etc.
#include <netinet/in.h>			// Needed for sockaddr_in
#include <syslog.h>

#include "mDNSEmbeddedAPI.h"	// Defines the interface provided to the client layer above
#include "DNSCommon.h"
#include "PlatformCommon.h"

#ifdef NOT_HAVE_SOCKLEN_T
    typedef unsigned int socklen_t;
#endif

// Bind a UDP socket to find the source address to a destination
mDNSexport void mDNSPlatformSourceAddrForDest(mDNSAddr *const src, const mDNSAddr *const dst)
	{
	union { struct sockaddr s; struct sockaddr_in a4; struct sockaddr_in6 a6; } addr;
	socklen_t len = sizeof(addr);
	socklen_t inner_len = 0;
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	src->type = mDNSAddrType_None;
	if (sock == -1) return;
	if (dst->type == mDNSAddrType_IPv4)
		{
		inner_len = sizeof(addr.a4);
		#ifndef NOT_HAVE_SA_LEN
		addr.a4.sin_len         = inner_len;
		#endif
		addr.a4.sin_family      = AF_INET;
		addr.a4.sin_port        = 1;	// Not important, any port will do
		addr.a4.sin_addr.s_addr = dst->ip.v4.NotAnInteger;
		}
	else if (dst->type == mDNSAddrType_IPv6)
		{
		inner_len = sizeof(addr.a6);
		#ifndef NOT_HAVE_SA_LEN
		addr.a6.sin6_len      = inner_len;
		#endif
		addr.a6.sin6_family   = AF_INET6;
		addr.a6.sin6_flowinfo = 0;
		addr.a6.sin6_port     = 1;	// Not important, any port will do
		addr.a6.sin6_addr     = *(struct in6_addr*)&dst->ip.v6;
		addr.a6.sin6_scope_id = 0;
		}
	else return;

	if ((connect(sock, &addr.s, inner_len)) < 0)
		{ LogMsg("mDNSPlatformSourceAddrForDest: connect %#a failed errno %d (%s)", dst, errno, strerror(errno)); goto exit; }

	if ((getsockname(sock, &addr.s, &len)) < 0)
		{ LogMsg("mDNSPlatformSourceAddrForDest: getsockname failed errno %d (%s)", errno, strerror(errno)); goto exit; }

	src->type = dst->type;
	if (dst->type == mDNSAddrType_IPv4) src->ip.v4.NotAnInteger = addr.a4.sin_addr.s_addr;
	else                                src->ip.v6 = *(mDNSv6Addr*)&addr.a6.sin6_addr;
exit:
	close(sock);
	}

// dst must be at least MAX_ESCAPED_DOMAIN_NAME bytes, and option must be less than 32 bytes in length
mDNSlocal mDNSBool GetConfigOption(char *dst, const char *option, FILE *f)
	{
	char buf[32+1+MAX_ESCAPED_DOMAIN_NAME];	// Option name, one space, option value
	unsigned int len = strlen(option);
	if (len + 1 + MAX_ESCAPED_DOMAIN_NAME > sizeof(buf)-1) { LogMsg("GetConfigOption: option %s too long", option); return mDNSfalse; }
	fseek(f, 0, SEEK_SET);  // set position to beginning of stream
	while (fgets(buf, sizeof(buf), f))		// Read at most sizeof(buf)-1 bytes from file, and append '\0' C-string terminator
		{
		if (!strncmp(buf, option, len))
			{
			strncpy(dst, buf + len + 1, MAX_ESCAPED_DOMAIN_NAME-1);
			if (dst[MAX_ESCAPED_DOMAIN_NAME-1]) dst[MAX_ESCAPED_DOMAIN_NAME-1] = '\0';
			len = strlen(dst);
			if (len && dst[len-1] == '\n') dst[len-1] = '\0';  // chop newline
			return mDNStrue;
			}
		}
	debugf("Option %s not set", option);
	return mDNSfalse;
	}

mDNSexport void ReadDDNSSettingsFromConfFile(mDNS *const m, const char *const filename, domainname *const hostname, domainname *const domain, mDNSBool *DomainDiscoveryDisabled)
	{
	char buf[MAX_ESCAPED_DOMAIN_NAME] = "";
	mStatus err;
	FILE *f = fopen(filename, "r");

    if (hostname)                 hostname->c[0] = 0;
    if (domain)                   domain->c[0] = 0;
	if (DomainDiscoveryDisabled) *DomainDiscoveryDisabled = mDNSfalse;

	if (f)
		{
		if (DomainDiscoveryDisabled && GetConfigOption(buf, "DomainDiscoveryDisabled", f) && !strcasecmp(buf, "true")) *DomainDiscoveryDisabled = mDNStrue;
		if (hostname && GetConfigOption(buf, "hostname", f) && !MakeDomainNameFromDNSNameString(hostname, buf)) goto badf;
		if (domain && GetConfigOption(buf, "zone", f) && !MakeDomainNameFromDNSNameString(domain, buf)) goto badf;
		buf[0] = 0;
		GetConfigOption(buf, "secret-64", f);  // failure means no authentication
		fclose(f);
		f = NULL;
		}
	else
		{
		if (errno != ENOENT) LogMsg("ERROR: Config file exists, but cannot be opened.");
		return;
		}

	if (domain && domain->c[0] && buf[0])
		{
		DomainAuthInfo *info = (DomainAuthInfo*)mDNSPlatformMemAllocate(sizeof(*info));
		// for now we assume keyname = service reg domain and we use same key for service and hostname registration
		err = mDNS_SetSecretForDomain(m, info, domain, domain, buf, mDNSfalse);
		if (err) LogMsg("ERROR: mDNS_SetSecretForDomain returned %d for domain %##s", err, domain->c);
		}

	return;

	badf:
	LogMsg("ERROR: malformatted config file");
	if (f) fclose(f);
	}

#if MDNS_DEBUGMSGS
mDNSexport void mDNSPlatformWriteDebugMsg(const char *msg)
	{
	fprintf(stderr,"%s\n", msg);
	fflush(stderr);
	}
#endif

mDNSexport void mDNSPlatformWriteLogMsg(const char *ident, const char *buffer, mDNSLogLevel_t loglevel)
	{
#if APPLE_OSX_mDNSResponder && LogTimeStamps
	extern mDNS mDNSStorage;
	extern mDNSu32 mDNSPlatformClockDivisor;
	mDNSs32 t = mDNSStorage.timenow ? mDNSStorage.timenow : mDNSPlatformClockDivisor ? mDNS_TimeNow_NoLock(&mDNSStorage) : 0;
	int ms = ((t < 0) ? -t : t) % 1000;
#endif

	if (mDNS_DebugMode)	// In debug mode we write to stderr
		{
#if APPLE_OSX_mDNSResponder && LogTimeStamps
		if (ident && ident[0] && mDNSPlatformClockDivisor)
			fprintf(stderr,"%8d.%03d: %s\n", (int)(t/1000), ms, buffer);
		else
#endif
			fprintf(stderr,"%s\n", buffer);
		fflush(stderr);
		}
	else				// else, in production mode, we write to syslog
		{
		static int log_inited = 0;
		
		int syslog_level = LOG_ERR;
		switch (loglevel) 
			{
			case MDNS_LOG_MSG:       syslog_level = LOG_ERR;     break;
			case MDNS_LOG_OPERATION: syslog_level = LOG_WARNING; break;
			case MDNS_LOG_SPS:       syslog_level = LOG_NOTICE;  break;
			case MDNS_LOG_INFO:      syslog_level = LOG_INFO;    break;
			case MDNS_LOG_DEBUG:     syslog_level = LOG_DEBUG;   break;
			default:
			fprintf(stderr, "Unknown loglevel %d, assuming LOG_ERR\n", loglevel);
			fflush(stderr);
			}
		
		if (!log_inited) { openlog(ident, LOG_CONS, LOG_DAEMON); log_inited++; }

#if APPLE_OSX_mDNSResponder && LogTimeStamps
		if (ident && ident[0] && mDNSPlatformClockDivisor)
			syslog(syslog_level, "%8d.%03d: %s", (int)(t/1000), ms, buffer);
		else
#endif
			syslog(syslog_level, "%s", buffer);
		}
	}
