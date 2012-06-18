/*
 * $Id: pathnames.h,v 1.1 2004/11/14 07:26:26 paulus Exp $
 *
 * Copyright (C) 1995,1996 Lars Fenneberg
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

#ifndef PATHNAMES_H
#define PATHNAMES_H

#define _PATH_DEV_URANDOM	"/dev/urandom"		/* Linux only */
#define _PATH_ETC_ISSUE		"/etc/issue"

/* normally defined in the Makefile */
#ifndef _PATH_ETC_RADIUSCLIENT_CONF
#define _PATH_ETC_RADIUSCLIENT_CONF       "/etc/radiusclient.conf"
#endif

#endif /* PATHNAMES_H */
