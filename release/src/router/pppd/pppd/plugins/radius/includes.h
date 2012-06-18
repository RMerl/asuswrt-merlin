/*
 * $Id: includes.h,v 1.1 2004/11/14 07:26:26 paulus Exp $
 *
 * Copyright (C) 1997 Lars Fenneberg
 *
 * Copyright 1992 Livingston Enterprises, Inc.
 *
 * Copyright 1992,1993, 1994,1995 The Regents of the University of Michigan
 * and Merit Network, Inc. All Rights Reserved
 *
 * See the file COPYRIGHT for the respective terms and conditions.
 * If the file is missing contact me at lf@elemental.net
 * and I'll send you a copy.
 *
 */

#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <syslog.h>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX        1024
#endif

#ifndef UCHAR_MAX
# define UCHAR_MAX       255
#endif

#include <pwd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <time.h>

#include "magic.h"

/* rlib/lock.c */
int do_lock_exclusive(int);
int do_unlock(int);
