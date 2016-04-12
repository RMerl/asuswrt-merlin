/* Copyright (c) 2010-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_STATUS_H
#define TOR_STATUS_H

#include "testsupport.h"

int log_heartbeat(time_t now);

#ifdef STATUS_PRIVATE
STATIC int count_circuits(void);
STATIC char *secs_to_uptime(long secs);
STATIC char *bytes_to_usage(uint64_t bytes);
#endif

#endif

