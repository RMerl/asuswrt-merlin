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
    
$Log: PosixCompat.c,v $
Revision 1.1  2009/07/09 21:40:32  herscher
<rdar://problem/3775717> SDK: Port mDNSNetMonitor to Windows. Add a small Posix compatibility layer to the mDNSWindows platform layer. This makes it possible to centralize the implementations to functions such as if_indextoname() and inet_pton() that are made in several projects in B4W.


*/

#include "PosixCompat.h"
#include <DebugServices.h>


typedef PCHAR (WINAPI * if_indextoname_funcptr_t)(ULONG index, PCHAR name);
typedef ULONG (WINAPI * if_nametoindex_funcptr_t)(PCSTR name);


unsigned
if_nametoindex( const char * ifname )
{
	HMODULE library;
	unsigned index = 0;

	check( ifname );

	// Try and load the IP helper library dll
	if ((library = LoadLibrary(TEXT("Iphlpapi")) ) != NULL )
	{
		if_nametoindex_funcptr_t if_nametoindex_funcptr;

		// On Vista and above there is a Posix like implementation of if_nametoindex
		if ((if_nametoindex_funcptr = (if_nametoindex_funcptr_t) GetProcAddress(library, "if_nametoindex")) != NULL )
		{
			index = if_nametoindex_funcptr(ifname);
		}

		FreeLibrary(library);
	}

	return index;
}


char*
if_indextoname( unsigned ifindex, char * ifname )
{
	HMODULE library;
	char * name = NULL;

	check( ifname );
	*ifname = '\0';

	// Try and load the IP helper library dll
	if ((library = LoadLibrary(TEXT("Iphlpapi")) ) != NULL )
	{
		if_indextoname_funcptr_t if_indextoname_funcptr;

		// On Vista and above there is a Posix like implementation of if_indextoname
		if ((if_indextoname_funcptr = (if_indextoname_funcptr_t) GetProcAddress(library, "if_indextoname")) != NULL )
		{
			name = if_indextoname_funcptr(ifindex, ifname);
		}

		FreeLibrary(library);
	}

	return name;
}


int
inet_pton( int family, const char * addr, void * dst )
{
	struct sockaddr_storage ss;
	int sslen = sizeof( ss );

	ZeroMemory( &ss, sizeof( ss ) );
	ss.ss_family = family;

	if ( WSAStringToAddressA( ( LPSTR ) addr, family, NULL, ( struct sockaddr* ) &ss, &sslen ) == 0 )
	{
		if ( family == AF_INET ) { memcpy( dst, &( ( struct sockaddr_in* ) &ss)->sin_addr, sizeof( IN_ADDR ) ); return 1; }
		else if ( family == AF_INET6 ) { memcpy( dst, &( ( struct sockaddr_in6* ) &ss)->sin6_addr, sizeof( IN6_ADDR ) ); return 1; }
		else return 0;
	}
    else return 0;
}


int
gettimeofday( struct timeval * tv, struct timezone * tz )
{
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#	define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#	define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif
 
	FILETIME ft;
	unsigned __int64 tmpres = 0;
 
	if ( tv != NULL )
	{
		GetSystemTimeAsFileTime(&ft);
 
		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;
 
		tmpres -= DELTA_EPOCH_IN_MICROSECS; 
		tmpres /= 10;  /*convert into microseconds*/
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}
 
	return 0;
}


extern struct tm*
localtime_r( const time_t * clock, struct tm * result )
{
	result = localtime( clock );
	return result;
}
