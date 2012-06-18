/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 1997-2004 Apple Computer, Inc. All rights reserved.
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
    
$Log: PosixCompat.h,v $
Revision 1.1  2009/07/09 21:40:32  herscher
<rdar://problem/3775717> SDK: Port mDNSNetMonitor to Windows. Add a small Posix compatibility layer to the mDNSWindows platform layer. This makes it possible to centralize the implementations to functions such as if_indextoname() and inet_pton() that are made in several projects in B4W.


*/

#pragma once

#include "CommonServices.h"
#include <winsock2.h>
#include <time.h>


/* 
 * Posix process compatibility
 */
typedef int pid_t;
#if !defined( getpid )
#	define getpid _getpid
#endif


/* 
 * Posix networking compatibility
 */
extern unsigned
if_nametoindex( const char * ifname );


extern char*
if_indextoname( unsigned ifindex, char * ifname );


extern int
inet_pton( int family, const char * addr, void * dst );


/* 
 * Posix time compatibility
 */
extern int
gettimeofday( struct timeval * tv, struct timezone * tz );


extern struct tm*
localtime_r( const time_t * clock, struct tm * result );


/* 
 * Posix string compatibility
 */
#if !defined( strcasecmp )
#	define strcasecmp	_stricmp
#endif

#if !defined( snprintf )
#	define snprint		_snprintf
#endif

