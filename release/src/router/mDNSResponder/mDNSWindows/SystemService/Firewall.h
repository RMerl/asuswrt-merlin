/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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
    
$Log: Firewall.h,v $
Revision 1.3  2009/04/24 04:55:26  herscher
<rdar://problem/3496833> Advertise SMB file sharing via Bonjour

Revision 1.2  2006/08/14 23:26:07  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.1  2004/09/13 07:32:31  shersche
Wrapper for Windows Firewall API code




*/



#ifndef _Firewall_h

#define _Firewall_h





#include "CommonServices.h"

#include "DebugServices.h"





#if defined(__cplusplus)

extern "C"

{

#endif





OSStatus

mDNSAddToFirewall

		(

		LPWSTR	executable,

		LPWSTR	name

		);


BOOL
mDNSIsFileAndPrintSharingEnabled();





#if defined(__cplusplus)

}

#endif





#endif

