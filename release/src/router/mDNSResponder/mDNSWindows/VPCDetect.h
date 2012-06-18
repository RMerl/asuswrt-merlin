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
    
$Log: VPCDetect.h,v $
Revision 1.3  2006/08/14 23:25:20  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.2  2006/02/26 19:31:05  herscher
<rdar://problem/4455038> Bonjour For Windows takes 90 seconds to start. This was caused by a bad interaction between the VirtualPC check, and the removal of the WMI dependency.  The problem was fixed by: 1) checking to see if WMI is running before trying to talk to it.  2) Retrying the VirtualPC check every 10 seconds upon failure, stopping after 10 unsuccessful tries.

Revision 1.1  2005/11/27 20:21:16  herscher
<rdar://problem/4210580> Workaround Virtual PC bug that incorrectly modifies incoming mDNS packets

*/

#pragma once

#include <windows.h>
#include <mDNSEmbeddedAPI.h>


#if defined(__cplusplus)
extern "C"
{
#endif


extern mStatus
IsVPCRunning( BOOL * inVirtualPC );


#if defined(__cplusplus)
}
#endif
