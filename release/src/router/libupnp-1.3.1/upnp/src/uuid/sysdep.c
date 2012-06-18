/*
 ** Copyright (c) 1990- 1993, 1996 Open Software Foundation, Inc.
 ** Copyright (c) 1989 by Hewlett-Packard Company, Palo Alto, Ca. &
 ** Digital Equipment Corporation, Maynard, Mass.
 ** Copyright (c) 1998 Microsoft.
 ** To anyone who acknowledges that this file is provided "AS IS"
 ** without any express or implied warranty: permission to use, copy,
 ** modify, and distribute this file for any purpose is hereby
 ** granted without fee, provided that the above copyright notices and
 ** this notice appears in all source code copies, and that none of
 ** the names of Open Software Foundation, Inc., Hewlett-Packard
 ** Company, or Digital Equipment Corporation be used in advertising
 ** or publicity pertaining to distribution of the software without
 ** specific, written prior permission.  Neither Open Software
 ** Foundation, Inc., Hewlett-Packard Company, Microsoft, nor Digital Equipment
 ** Corporation makes any representations about the suitability of
 ** this software for any purpose.
 */

#include "config.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "sysdep.h"

/*-----------------------------------------------------------------------------*/
/*
   system dependent call to get IEEE node ID.
   This sample implementation generates a random node ID
 */
void
get_ieee_node_identifier( uuid_node_t * node )
{
    char seed[16];
    static int inited = 0;
    static uuid_node_t saved_node;

    if( !inited ) {
        get_random_info( seed );
        seed[0] |= 0x80;
        memcpy( &saved_node, seed, sizeof( uuid_node_t ) );

        inited = 1;
    };

    *node = saved_node;
};

/*-----------------------------------------------------------------------------*/
/*
   system dependent call to get the current system time.
   Returned as 100ns ticks since Oct 15, 1582, but resolution may be
   less than 100ns.
 */

#ifdef _WINDOWS_

void
get_system_time( uuid_time_t * uuid_time )
{
    ULARGE_INTEGER time;

    GetSystemTimeAsFileTime( ( FILETIME * ) & time );

    /*
       NT keeps time in FILETIME format which is 100ns ticks since
       Jan 1, 1601.  UUIDs use time in 100ns ticks since Oct 15, 1582.
       The difference is 17 Days in Oct + 30 (Nov) + 31 (Dec)
       + 18 years and 5 leap days.
     */

    time.QuadPart += ( unsigned __int64 )( 1000 * 1000 * 10 )   // seconds
        * ( unsigned __int64 )( 60 * 60 * 24 )  // days
        * ( unsigned __int64 )( 17 + 30 + 31 + 365 * 18 + 5 );  // # of days

    *uuid_time = time.QuadPart;

};

/*-----------------------------------------------------------------------------*/
void
get_random_info( char seed[16] )
{
    MD5_CTX c;
    typedef struct {
        MEMORYSTATUS m;
        SYSTEM_INFO s;
        FILETIME t;
        LARGE_INTEGER pc;
        DWORD tc;
        DWORD l;
        char hostname[MAX_COMPUTERNAME_LENGTH + 1];
    } randomness;
    randomness r;

    MD5Init( &c );
    /*
       memory usage stats 
     */
    GlobalMemoryStatus( &r.m );
    /*
       random system stats 
     */
    GetSystemInfo( &r.s );
    /*
       100ns resolution (nominally) time of day 
     */
    GetSystemTimeAsFileTime( &r.t );
    /*
       high resolution performance counter 
     */
    QueryPerformanceCounter( &r.pc );
    /*
       milliseconds since last boot 
     */
    r.tc = GetTickCount(  );
    r.l = MAX_COMPUTERNAME_LENGTH + 1;

    GetComputerName( r.hostname, &r.l );
    MD5Update( &c, &r, sizeof( randomness ) );
    MD5Final( seed, &c );
};
#else

/*-----------------------------------------------------------------------------*/
void
get_system_time( uuid_time_t * uuid_time )
{
    struct timeval tp;

    gettimeofday( &tp, ( struct timezone * )0 );

    /*
       Offset between UUID formatted times and Unix formatted times.
       UUID UTC base time is October 15, 1582.
       Unix base time is January 1, 1970.
     */
    *uuid_time = ( tp.tv_sec * 10000000 ) + ( tp.tv_usec * 10 ) +
        I64( 0x01B21DD213814000 );
};

/*-----------------------------------------------------------------------------*/
void
get_random_info( char seed[16] )
{
    MD5_CTX c;
    typedef struct {
        struct sysinfo s;
        struct timeval t;
        char hostname[257];
    } randomness;
    randomness r;

    MD5Init( &c );

    gettimeofday( &r.t, ( struct timezone * )0 );
    gethostname( r.hostname, 256 );
    MD5Update( &c, &r, sizeof( randomness ) );
    MD5Final( seed, &c );
};

#endif
