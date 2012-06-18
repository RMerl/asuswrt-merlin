/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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

$Log: safe_vproc.c,v $
Revision 1.3  2009/02/14 00:09:53  cheshire
Only log "Compiled without vproc_transaction support" when running on a system
where we expect that to be available -- if running on a system that doesn't even
have vproc_transaction, then it doesn't matter that the code was compiled without it.

Revision 1.2  2009/02/09 21:16:17  mcguire
<rdar://problem/5858533> Adopt vproc_transaction API in mDNSResponder
additional cleanup: don't alloc memory since we currently only expect to have one transaction

Revision 1.1  2009/02/06 03:06:49  mcguire
<rdar://problem/5858533> Adopt vproc_transaction API in mDNSResponder

 */

#include <stdlib.h>
#include <assert.h>
#include <vproc.h>
#include "safe_vproc.h"
#include "mDNSDebug.h"

#ifdef VPROC_HAS_TRANSACTIONS

static vproc_transaction_t transaction = NULL;

void safe_vproc_transaction_begin(void)
	{
	if (vproc_transaction_begin)
		{
		if (transaction) { LogMsg("safe_vproc_transaction_begin: Already have a transaction"); }
		else transaction = vproc_transaction_begin(NULL);
		}
	}
	
void safe_vproc_transaction_end(void)
	{
	if (vproc_transaction_end)
		{
		if (transaction) { vproc_transaction_end(NULL, transaction); transaction = NULL; }
		else LogMsg("safe_vproc_transaction_end: No current transaction");
		}
	}

#else

#include <stdio.h>
#include <CoreFoundation/CFString.h>

CF_EXPORT CFDictionaryRef _CFCopySystemVersionDictionary(void);
CF_EXPORT const CFStringRef _kCFSystemVersionBuildVersionKey;
#define OSXVers_10_6_SnowLeopard 10

void safe_vproc_transaction_begin(void)
	{
	static int majorversion = 0;
	if (!majorversion)
		{
		CFDictionaryRef vers = _CFCopySystemVersionDictionary();
		if (vers)
			{
			char buildver[256];
			CFStringRef cfbuildver = CFDictionaryGetValue(vers, _kCFSystemVersionBuildVersionKey);
			if (cfbuildver && CFStringGetCString(cfbuildver, buildver, sizeof(buildver), kCFStringEncodingUTF8))
				sscanf(buildver, "%d", &majorversion);
			CFRelease(vers);
			}
		if (!majorversion || majorversion >= OSXVers_10_6_SnowLeopard)
			LogMsg("Compiled without vproc_transaction support");
		}
	}

void safe_vproc_transaction_end(void) { }

#endif
