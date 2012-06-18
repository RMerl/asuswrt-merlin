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

$Log: Stdafx.h,v $
Revision 1.6  2009/03/30 20:17:57  herscher
<rdar://problem/5925472> Current Bonjour code does not compile on Windows
<rdar://problem/5187308> Move build train to Visual Studio 2005

Revision 1.5  2006/08/14 23:25:43  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.4  2005/10/19 19:50:35  herscher
Workaround a bug in the latest Microsoft Platform SDK when compiling C++ files that include (directly or indirectly) <WspiApi.h>

Revision 1.3  2005/02/05 02:40:59  cheshire
Convert newlines to Unix-style (ASCII 10)

Revision 1.2  2005/02/05 02:37:01  cheshire
Convert newlines to Unix-style (ASCII 10)

Revision 1.1  2004/06/26 04:01:22  shersche
Initial revision

 */
    
// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

#if !defined(_WSPIAPI_COUNTOF)
#	define _WSPIAPI_COUNTOF(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#endif

#using <mscorlib.dll>
#using <System.dll>

struct _DNSServiceRef_t {};
struct _DNSRecordRef_t {};

