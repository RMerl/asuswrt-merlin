/*
 * Copyright 1998,1999 The OpenLDAP Foundation, Redwood City, California, USA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted only
 * as authorized by the OpenLDAP Public License.  A copy of this
 * license is available at http://www.OpenLDAP.org/license.html or
 * in file LICENSE in the top-level directory of the distribution.
 */
#include <sys/types.h>
#ifndef _LUTIL_H
#define _LUTIL_H 1

/* base64.c */
int lutil_b64_ntop(unsigned char const *, size_t, char *, size_t);
int lutil_b64_pton(char const *, unsigned char *, size_t);
#endif

