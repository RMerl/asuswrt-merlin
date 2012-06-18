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

$Log: Client.c,v $
Revision 1.21  2008/11/04 19:46:01  cheshire
Updated comment about MAX_ESCAPED_DOMAIN_NAME size (should be 1009, not 1005)

Revision 1.20  2007/07/27 19:30:41  cheshire
Changed mDNSQuestionCallback parameter from mDNSBool to QC_result,
to properly reflect tri-state nature of the possible responses

Revision 1.19  2007/04/16 20:49:39  cheshire
Fix compile errors for mDNSPosix build

Revision 1.18  2006/08/14 23:24:46  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.17  2006/06/12 18:22:42  cheshire
<rdar://problem/4580067> mDNSResponder building warnings under Red Hat 64-bit (LP64) Linux

Revision 1.16  2005/02/04 01:00:53  cheshire
Add '-d' command-line option to specify domain to browse

Revision 1.15  2004/12/16 20:17:11  cheshire
<rdar://problem/3324626> Cache memory management improvements

Revision 1.14  2004/11/30 22:37:00  cheshire
Update copyright dates and add "Mode: C; tab-width: 4" headers

Revision 1.13  2004/10/19 21:33:20  cheshire
<rdar://problem/3844991> Cannot resolve non-local registrations using the mach API
Added flag 'kDNSServiceFlagsForceMulticast'. Passing through an interface id for a unicast name
doesn't force multicast unless you set this flag to indicate explicitly that this is what you want

Revision 1.12  2004/09/17 01:08:53  cheshire
Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
  The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
  declared in that file are ONLY appropriate to single-address-space embedded applications.
  For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.

Revision 1.11  2003/11/17 20:14:32  cheshire
Typo: Wrote "domC" where it should have said "domainC"

Revision 1.10  2003/11/14 21:27:09  cheshire
<rdar://problem/3484766>: Security: Crashing bug in mDNSResponder
Fix code that should use buffer size MAX_ESCAPED_DOMAIN_NAME (1009) instead of 256-byte buffers.

Revision 1.9  2003/08/14 02:19:55  cheshire
<rdar://problem/3375491> Split generic ResourceRecord type into two separate types: AuthRecord and CacheRecord

Revision 1.8  2003/08/12 19:56:26  cheshire
Update to APSL 2.0

Revision 1.7  2003/07/02 21:19:58  cheshire
<rdar://problem/3313413> Update copyright notices, etc., in source code comments

Revision 1.6  2003/06/18 05:48:41  cheshire
Fix warnings

Revision 1.5  2003/05/06 00:00:50  cheshire
<rdar://problem/3248914> Rationalize naming of domainname manipulation functions

Revision 1.4  2002/12/23 22:13:31  jgraessl

Reviewed by: Stuart Cheshire
Initial IPv6 support for mDNSResponder.

Revision 1.3  2002/09/21 20:44:53  zarzycki
Added APSL info

Revision 1.2  2002/09/19 04:20:44  cheshire
Remove high-ascii characters that confuse some systems

Revision 1.1  2002/09/17 06:24:35  cheshire
First checkin

*/

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "mDNSEmbeddedAPI.h"// Defines the interface to the mDNS core code
#include "mDNSPosix.h"    // Defines the specific types needed to run mDNS on this platform
#include "ExampleClientApp.h"

// Globals
static mDNS mDNSStorage;       // mDNS core uses this to store its globals
static mDNS_PlatformSupport PlatformStorage;  // Stores this platform's globals
#define RR_CACHE_SIZE 500
static CacheEntity gRRCache[RR_CACHE_SIZE];

mDNSexport const char ProgramName[] = "mDNSClientPosix";

static const char *gProgramName = ProgramName;

static void BrowseCallback(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
    // A callback from the core mDNS code that indicates that we've received a 
    // response to our query.  Note that this code runs on the main thread 
    // (in fact, there is only one thread!), so we can safely printf the results.
{
    domainlabel name;
    domainname  type;
    domainname  domain;
	char nameC  [MAX_DOMAIN_LABEL+1];			// Unescaped name: up to 63 bytes plus C-string terminating NULL.
	char typeC  [MAX_ESCAPED_DOMAIN_NAME];
	char domainC[MAX_ESCAPED_DOMAIN_NAME];
    const char *state;

	(void)m;		// Unused
	(void)question;	// Unused

    assert(answer->rrtype == kDNSType_PTR);

    DeconstructServiceName(&answer->rdata->u.name, &name, &type, &domain);

    ConvertDomainLabelToCString_unescaped(&name, nameC);
    ConvertDomainNameToCString(&type, typeC);
    ConvertDomainNameToCString(&domain, domainC);

    // If the TTL has hit 0, the service is no longer available.
    if (!AddRecord) {
        state = "Lost ";
    } else {
        state = "Found";
    }
    fprintf(stderr, "*** %s name = '%s', type = '%s', domain = '%s'\n", state, nameC, typeC, domainC);
}

static mDNSBool CheckThatServiceTypeIsUsable(const char *serviceType, mDNSBool printExplanation)
    // Checks that serviceType is a reasonable service type 
    // label and, if it isn't and printExplanation is true, prints 
    // an explanation of why not.
{
    mDNSBool result;
    
    result = mDNStrue;
    if (result && strlen(serviceType) > 63) {
        if (printExplanation) {
            fprintf(stderr, 
                    "%s: Service type specified by -t is too long (must be 63 characters or less)\n", 
                    gProgramName);
        }
        result = mDNSfalse;
    }
    if (result && serviceType[0] == 0) {
        if (printExplanation) {
            fprintf(stderr, 
                    "%s: Service type specified by -t can't be empty\n", 
                    gProgramName);
        }
        result = mDNSfalse;
    }
    return result;
}

static const char kDefaultServiceType[] = "_afpovertcp._tcp";
static const char kDefaultDomain[]      = "local.";

static void PrintUsage()
{
    fprintf(stderr, 
            "Usage: %s [-v level] [-t type] [-d domain]\n",
            gProgramName);
    fprintf(stderr, "          -v verbose mode, level is a number from 0 to 2\n");
    fprintf(stderr, "             0 = no debugging info (default)\n");
    fprintf(stderr, "             1 = standard debugging info\n");
    fprintf(stderr, "             2 = intense debugging info\n");
    fprintf(stderr, "          -t uses 'type' as the service type (default is '%s')\n", kDefaultServiceType);
    fprintf(stderr, "          -d uses 'domain' as the domain to browse (default is '%s')\n", kDefaultDomain);
}

static const char *gServiceType      = kDefaultServiceType;
static const char *gServiceDomain    = kDefaultDomain;

static void ParseArguments(int argc, char **argv)
    // Parses our command line arguments into the global variables 
    // listed above.
{
    int ch;
    
    // Set gProgramName to the last path component of argv[0]
    
    gProgramName = strrchr(argv[0], '/');
    if (gProgramName == NULL) {
        gProgramName = argv[0];
    } else {
        gProgramName += 1;
    }
    
    // Parse command line options using getopt.
    
    do {
        ch = getopt(argc, argv, "v:t:d:");
        if (ch != -1) {
            switch (ch) {
                case 'v':
                    gMDNSPlatformPosixVerboseLevel = atoi(optarg);
                    if (gMDNSPlatformPosixVerboseLevel < 0 || gMDNSPlatformPosixVerboseLevel > 2) {
                        fprintf(stderr, 
                                "%s: Verbose mode must be in the range 0..2\n", 
                                gProgramName);
                        exit(1);
                    }
                    break;
                case 't':
                    gServiceType = optarg;
                    if ( ! CheckThatServiceTypeIsUsable(gServiceType, mDNStrue) ) {
                        exit(1);
                    }
                    break;
                case 'd':
                    gServiceDomain = optarg;
                    break;
                case '?':
                default:
                    PrintUsage();
                    exit(1);
                    break;
            }
        }
    } while (ch != -1);

    // Check for any left over command line arguments.
    
    if (optind != argc) {
        fprintf(stderr, "%s: Unexpected argument '%s'\n", gProgramName, argv[optind]);
        exit(1);
    }
}

int main(int argc, char **argv)
    // The program's main entry point.  The program does a trivial 
    // mDNS query, looking for all AFP servers in the local domain.
{
    int     result;
    mStatus     status;
    DNSQuestion question;
    domainname  type;
    domainname  domain;

    // Parse our command line arguments.  This won't come back if there's an error.
    ParseArguments(argc, argv);

    // Initialise the mDNS core.
	status = mDNS_Init(&mDNSStorage, &PlatformStorage,
    	gRRCache, RR_CACHE_SIZE,
    	mDNS_Init_DontAdvertiseLocalAddresses,
    	mDNS_Init_NoInitCallback, mDNS_Init_NoInitCallbackContext);
    if (status == mStatus_NoError) {
    
        // Construct and start the query.
        
        MakeDomainNameFromDNSNameString(&type, gServiceType);
        MakeDomainNameFromDNSNameString(&domain, gServiceDomain);

        status = mDNS_StartBrowse(&mDNSStorage, &question, &type, &domain, mDNSInterface_Any, mDNSfalse, BrowseCallback, NULL);
    
        // Run the platform main event loop until the user types ^C. 
        // The BrowseCallback routine is responsible for printing 
        // any results that we find.
        
        if (status == mStatus_NoError) {
            fprintf(stderr, "Hit ^C when you're bored waiting for responses.\n");
        	ExampleClientEventLoop(&mDNSStorage);
            mDNS_StopQuery(&mDNSStorage, &question);
			mDNS_Close(&mDNSStorage);
        }
    }
    
    if (status == mStatus_NoError) {
        result = 0;
    } else {
        result = 2;
    }
    if ( (result != 0) || (gMDNSPlatformPosixVerboseLevel > 0) ) {
        fprintf(stderr, "%s: Finished with status %d, result %d\n", gProgramName, (int)status, result);
    }

    return 0;
}
