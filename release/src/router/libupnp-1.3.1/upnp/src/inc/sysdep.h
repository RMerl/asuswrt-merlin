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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include "ithread.h"

/* change to point to where MD5 .h's live */
/* get MD5 sample implementation from RFC 1321 */
#include "global.h"
#include "md5.h"

/* set the following to the number of 100ns ticks of the actual
   resolution of
   your system's clock */
#define UUIDS_PER_TICK 1024

/* Set the following to a call to acquire a system wide global lock
 */
extern ithread_mutex_t gUUIDMutex;

#define UUIDLock()      ithread_mutex_lock(&gUUIDMutex)
#define UUIDUnlock()    ithread_mutex_unlock(&gUUIDMutex)

typedef unsigned long   unsigned32;
typedef unsigned short  unsigned16;
typedef unsigned char   unsigned8;
typedef unsigned char   byte;

/* Set this to what your compiler uses for 64 bit data type */
#define unsigned64_t unsigned long long
#define I64(C) C##LL

typedef unsigned64_t uuid_time_t;
typedef struct {
    char nodeID[6];
} uuid_node_t;

void get_ieee_node_identifier(uuid_node_t *node);
void get_system_time(uuid_time_t *uuid_time);
void get_random_info(char seed[16]);

