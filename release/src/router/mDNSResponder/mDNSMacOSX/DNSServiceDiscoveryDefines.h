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

$Log: DNSServiceDiscoveryDefines.h,v $
Revision 1.8  2006/10/27 00:35:36  cheshire
DNS_SERVICE_DISCOVERY_SERVER is now com.apple.mDNSResponder, not DNSServiceDiscoveryServer

Revision 1.7  2006/08/14 23:24:39  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.6  2004/09/20 21:45:27  ksekar
Mach IPC cleanup

Revision 1.5  2003/08/12 19:56:25  cheshire
Update to APSL 2.0

 */

#ifndef __DNS_SERVICE_DISCOVERY_DEFINES_H
#define __DNS_SERVICE_DISCOVERY_DEFINES_H

#include <mach/mach_types.h>

#define DNS_SERVICE_DISCOVERY_SERVER "com.apple.mDNSResponder"

typedef char    DNSCString[1024];
typedef char    sockaddr_t[128];

typedef const char * record_data_t;
typedef struct { char bytes[4]; } IPPort;

#endif	/* __DNS_SERVICE_DISCOVERY_DEFINES_H */
