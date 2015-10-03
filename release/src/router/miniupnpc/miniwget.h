/* $Id: miniwget.h,v 1.9 2014/06/10 09:44:55 nanard Exp $ */
/* Project : miniupnp
 * Author : Thomas Bernard
 * Copyright (c) 2005-2012 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution.
 * */
#ifndef MINIWGET_H_INCLUDED
#define MINIWGET_H_INCLUDED

#include "declspec.h"
#include "miniupnpc.h"

#ifdef __cplusplus
extern "C" {
#endif

MINIUPNP_LIBSPEC void * getHTTPResponse(int s, int * size, struct UPNPDevInfo *devinfo);

MINIUPNP_LIBSPEC void * miniwget(const char *, int *, unsigned int, struct UPNPDevInfo *devinfo);

MINIUPNP_LIBSPEC void * miniwget_getaddr(const char *, int *, char *, int, unsigned int, struct UPNPDevInfo *devinfo, char *dut_addr);

int parseURL(const char *, char *, unsigned short *, char * *, unsigned int *);

#ifdef __cplusplus
}
#endif

#endif

