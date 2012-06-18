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
    
$Log: Prefix.h,v $
Revision 1.2  2006/08/14 23:26:07  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.1  2004/06/18 04:16:41  rpantos
Move up one level.

Revision 1.2  2004/04/30 02:40:23  bradley
Define DNS_SD_CLIENT_ENABLED=0 so DNSSD.c can be included without linking the client IPC code.

Revision 1.1  2004/01/30 02:58:39  bradley
mDNSResponder Windows Service. Provides global Bonjour support with an IPC interface.

*/

#ifndef __PREFIX__
#define __PREFIX__

#if( defined( _DEBUG ) )
	#define	DEBUG					1
	#define	MDNS_DEBUGMSGS			1
#else
	#define	DEBUG					0
#endif

#define	DNS_SD_CLIENT_ENABLED		0

#endif	// __PREFIX__
