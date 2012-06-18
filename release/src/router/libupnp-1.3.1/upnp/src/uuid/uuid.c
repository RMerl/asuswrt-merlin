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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <netinet/in.h>
#include "sysdep.h"
#include "uuid.h"

/*
   various forward declarations 
 */
static int read_state( unsigned16 * clockseq,
                       uuid_time_t * timestamp,
                       uuid_node_t * node );
static void write_state( unsigned16 clockseq,
                         uuid_time_t timestamp,
                         uuid_node_t node );
static void format_uuid_v1( uuid_upnp * uid,
                            unsigned16 clockseq,
                            uuid_time_t timestamp,
                            uuid_node_t node );
static void format_uuid_v3( uuid_upnp * uid,
                            unsigned char hash[16] );
static void get_current_time( uuid_time_t * timestamp );
static unsigned16 true_random( void );

/*-----------------------------------------------------------------------------*/
/*
   uuid_create -- generator a UUID 
 */
int
uuid_create( uuid_upnp * uid )
{
    uuid_time_t timestamp,
      last_time;
    unsigned16 clockseq;
    uuid_node_t node;
    uuid_node_t last_node;
    int f;

    /*
       acquire system wide lock so we're alone 
     */
    UUIDLock(  );

    /*
       get current time 
     */
    get_current_time( &timestamp );

    /*
       get node ID 
     */
    get_ieee_node_identifier( &node );

    /*
       get saved state from NV storage 
     */
    f = read_state( &clockseq, &last_time, &last_node );

    /*
       if no NV state, or if clock went backwards, or node ID changed
       (e.g., net card swap) change clockseq 
     */
    if( !f || memcmp( &node, &last_node, sizeof( uuid_node_t ) ) )
        clockseq = true_random(  );
    else if( timestamp < last_time )
        clockseq++;

    /*
       stuff fields into the UUID 
     */
    format_uuid_v1( uid, clockseq, timestamp, node );

    /*
       save the state for next time 
     */
    write_state( clockseq, timestamp, node );

    UUIDUnlock(  );
    return ( 1 );
};

/*-----------------------------------------------------------------------------*/
void
uuid_unpack( uuid_upnp * u,
             char *out )
{

    sprintf( out,
             "%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
             ( unsigned int )u->time_low, u->time_mid,
             u->time_hi_and_version, u->clock_seq_hi_and_reserved,
             u->clock_seq_low, u->node[0], u->node[1], u->node[2],
             u->node[3], u->node[4], u->node[5] );

    *( out + 36 ) = '\0';

};

/*-----------------------------------------------------------------------------*/
/*
   format_uuid_v1 -- make a UUID from the timestamp, clockseq,
   and node ID 
 */
void
format_uuid_v1( uuid_upnp * uid,
                unsigned16 clock_seq,
                uuid_time_t timestamp,
                uuid_node_t node )
{
    /*
       Construct a version 1 uuid with the information we've gathered
       * plus a few constants. 
     */
    uid->time_low = ( unsigned long )( timestamp & 0xFFFFFFFF );
    uid->time_mid = ( unsigned short )( ( timestamp >> 32 ) & 0xFFFF );
    uid->time_hi_and_version = ( unsigned short )( ( timestamp >> 48 ) &
                                                   0x0FFF );
    uid->time_hi_and_version |= ( 1 << 12 );
    uid->clock_seq_low = clock_seq & 0xFF;
    uid->clock_seq_hi_and_reserved = ( clock_seq & 0x3F00 ) >> 8;
    uid->clock_seq_hi_and_reserved |= 0x80;
    memcpy( &uid->node, &node, sizeof uid->node );
};

/*-----------------------------------------------------------------------------*/
/*
   data type for UUID generator persistent state 
 */
typedef struct {
    uuid_time_t ts;             /* saved timestamp */
    uuid_node_t node;           /* saved node ID */
    unsigned16 cs;              /* saved clock sequence */
} uuid_state;

static uuid_state st;
static int stateInited = 0;

/*-----------------------------------------------------------------------------*/
/*
   read_state -- read UUID generator state from non-volatile store 
 */
int
read_state( unsigned16 * clockseq,
            uuid_time_t * timestamp,
            uuid_node_t * node )
{

    if( !stateInited ) {
        return 0;
    };

    *clockseq = st.cs;
    *timestamp = st.ts;
    *node = st.node;
    return 1;
};

/*-----------------------------------------------------------------------------*/
/*
   write_state -- save UUID generator state back to non-volatile
   storage 
 */
void
write_state( unsigned16 clockseq,
             uuid_time_t timestamp,
             uuid_node_t node )
{
    static uuid_time_t next_save;

    if( !stateInited ) {
        next_save = timestamp;
        stateInited = 1;
    };

    /*
       always save state to volatile shared state 
     */
    st.cs = clockseq;
    st.ts = timestamp;
    st.node = node;
    if( timestamp >= next_save ) {
        /*
           schedule next save for 10 seconds from now 
         */
        next_save = timestamp + ( 10 * 10 * 1000 * 1000 );
    };
};

/*-----------------------------------------------------------------------------*/
/*
   get-current_time -- get time as 60 bit 100ns ticks since whenever.
   Compensate for the fact that real clock resolution is
   less than 100ns. 
 */
void
get_current_time( uuid_time_t * timestamp )
{
    uuid_time_t time_now;
    static uuid_time_t time_last;
    static unsigned16 uuids_this_tick;
    static int inited = 0;

    if( !inited ) {
        get_system_time( &time_now );
        uuids_this_tick = UUIDS_PER_TICK;
        inited = 1;
    };

    while( 1 ) {
        get_system_time( &time_now );

        /*
           if clock reading changed since last UUID generated... 
         */
        if( time_last != time_now ) {
            /*
               reset count of uuids gen'd with this clock reading 
             */
            uuids_this_tick = 0;
            break;
        };

        if( uuids_this_tick < UUIDS_PER_TICK ) {
            uuids_this_tick++;
            break;
        };
        /*
           going too fast for our clock; spin 
         */
    };

    /*
       add the count of uuids to low order bits of the clock reading 
     */
    *timestamp = time_now + uuids_this_tick;
    time_last = *timestamp;
};

/*-----------------------------------------------------------------------------*/
/*
   true_random -- generate a crypto-quality random number.
   This sample doesn't do that. 
 */
static unsigned16
true_random( void )
{
    static int inited = 0;
    uuid_time_t time_now;

    if( !inited ) {
        get_system_time( &time_now );
        time_now = time_now / UUIDS_PER_TICK;
        srand( ( unsigned int )( ( ( time_now >> 32 ) ^ time_now ) &
                                 0xffffffff ) );
        inited = 1;
    };

    return ( rand(  ) );
}

/*-----------------------------------------------------------------------------*/
/*
   uuid_create_from_name -- create a UUID using a "name" from a "name space" 
 */
void
uuid_create_from_name( uuid_upnp * uid, /* resulting UUID */

                       uuid_upnp nsid,  /* UUID to serve as context, so identical
                                           names from different name spaces generate
                                           different UUIDs */

                       void *name,  /* the name from which to generate a UUID */

                       int namelen  /* the length of the name */
     )
{
    MD5_CTX c;
    unsigned char hash[16];
    uuid_upnp net_nsid;         /* context UUID in network byte order */

    /*
       put name space ID in network byte order so it hashes the same
       no matter what endian machine we're on 
     */
    net_nsid = nsid;
    htonl( net_nsid.time_low );
    htons( net_nsid.time_mid );
    htons( net_nsid.time_hi_and_version );

    MD5Init( &c );
    MD5Update( &c, &net_nsid, sizeof( uuid_upnp ) );
    MD5Update( &c, name, namelen );
    MD5Final( hash, &c );

    /*
       the hash is in network byte order at this point 
     */
    format_uuid_v3( uid, hash );
};

/*-----------------------------------------------------------------------------*/
/*
   format_uuid_v3 -- make a UUID from a (pseudo)random 128 bit number
 */
void
format_uuid_v3( uuid_upnp * uid,
                unsigned char hash[16] )
{
    /*
       Construct a version 3 uuid with the (pseudo-)random number
       * plus a few constants. 
     */

    memcpy( uid, hash, sizeof( uuid_upnp ) );

    /*
       convert UUID to local byte order 
     */
    ntohl( uid->time_low );
    ntohs( uid->time_mid );
    ntohs( uid->time_hi_and_version );

    /*
       put in the variant and version bits 
     */
    uid->time_hi_and_version &= 0x0FFF;
    uid->time_hi_and_version |= ( 3 << 12 );
    uid->clock_seq_hi_and_reserved &= 0x3F;
    uid->clock_seq_hi_and_reserved |= 0x80;
};

/*-----------------------------------------------------------------------------*/
/*
   uuid_compare --  Compare two UUID's "lexically" and return
   -1   u1 is lexically before u2
   0   u1 is equal to u2
   1   u1 is lexically after u2

   Note:   lexical ordering is not temporal ordering!
 */
int
uuid_compare( uuid_upnp * u1,
              uuid_upnp * u2 )
{
    int i;

#define CHECK(f1, f2) if (f1 != f2) return f1 < f2 ? -1 : 1;
    CHECK( u1->time_low, u2->time_low );
    CHECK( u1->time_mid, u2->time_mid );
    CHECK( u1->time_hi_and_version, u2->time_hi_and_version );
    CHECK( u1->clock_seq_hi_and_reserved, u2->clock_seq_hi_and_reserved );
    CHECK( u1->clock_seq_low, u2->clock_seq_low )
        for( i = 0; i < 6; i++ ) {
        if( u1->node[i] < u2->node[i] )
            return -1;
        if( u1->node[i] > u2->node[i] )
            return 1;
    }

    return 0;
};
