/* $Id: errno.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include "test.h"
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/ctype.h>
#include <pj/compat/socket.h>
#include <pj/string.h>

#if INCLUDE_ERRNO_TEST

#define THIS_FILE   "errno"

#if defined(PJ_WIN32) && PJ_WIN32 != 0
#   include <windows.h>
#endif

#if defined(PJ_HAS_ERRNO_H) && PJ_HAS_ERRNO_H != 0
#   include <errno.h>
#endif

static void trim_newlines(char *s)
{
    while (*s) {
        if (*s == '\r' || *s == '\n')
            *s = ' ';
        ++s;
    }
}

int my_strncasecmp(const char *s1, const char *s2, int max_len)
{
    while (*s1 && *s2 && max_len > 0) {
        if (pj_tolower(*s1) != pj_tolower(*s2))
            return -1;
        ++s1;
        ++s2;
        --max_len;
    }
    return 0;
}

const char *my_stristr(const char *whole, const char *part)
{
    int part_len = strlen(part);
    while (*whole) {
        if (my_strncasecmp(whole, part, part_len) == 0)
            return whole;
        ++whole;
    }
    return NULL;
}

int errno_test(void)
{
    enum { CUT = 6 };
    pj_status_t rc = 0;
    char errbuf[256];

    PJ_LOG(3,(THIS_FILE, "...errno test: check the msg carefully"));

    PJ_UNUSED_ARG(rc);
    
    /* 
     * Windows platform error. 
     */
#   ifdef ERROR_INVALID_DATA
    rc = PJ_STATUS_FROM_OS(ERROR_INVALID_DATA);
    pj_set_os_error(rc);

    /* Whole */
    pj_strerror(rc, errbuf, sizeof(errbuf));
    trim_newlines(errbuf);
    PJ_LOG(3,(THIS_FILE, "...msg for rc=ERROR_INVALID_DATA: '%s'", errbuf));
    if (my_stristr(errbuf, "invalid") == NULL) {
        PJ_LOG(3, (THIS_FILE, 
                   "...error: expecting \"invalid\" string in the msg"));
#ifndef PJ_WIN32_WINCE
        return -20;
#endif
    }

    /* Cut version. */
    pj_strerror(rc, errbuf, CUT);
    PJ_LOG(3,(THIS_FILE, "...msg for rc=ERROR_INVALID_DATA (cut): '%s'", errbuf));
#   endif

    /*
     * Unix errors
     */
#   if defined(EINVAL) && !defined(PJ_SYMBIAN)
    rc = PJ_STATUS_FROM_OS(EINVAL);
    pj_set_os_error(rc);

    /* Whole */
    pj_strerror(rc, errbuf, sizeof(errbuf));
    trim_newlines(errbuf);
    PJ_LOG(3,(THIS_FILE, "...msg for rc=EINVAL: '%s'", errbuf));
    if (my_stristr(errbuf, "invalid") == NULL) {
        PJ_LOG(3, (THIS_FILE, 
                   "...error: expecting \"invalid\" string in the msg"));
        return -30;
    }

    /* Cut */
    pj_strerror(rc, errbuf, CUT);
    PJ_LOG(3,(THIS_FILE, "...msg for rc=EINVAL (cut): '%s'", errbuf));
#   endif

    /* 
     * Windows WSA errors
     */
#   ifdef WSAEINVAL
    rc = PJ_STATUS_FROM_OS(WSAEINVAL);
    pj_set_os_error(rc);

    /* Whole */
    pj_strerror(rc, errbuf, sizeof(errbuf));
    trim_newlines(errbuf);
    PJ_LOG(3,(THIS_FILE, "...msg for rc=WSAEINVAL: '%s'", errbuf));
    if (my_stristr(errbuf, "invalid") == NULL) {
        PJ_LOG(3, (THIS_FILE, 
                   "...error: expecting \"invalid\" string in the msg"));
        return -40;
    }

    /* Cut */
    pj_strerror(rc, errbuf, CUT);
    PJ_LOG(3,(THIS_FILE, "...msg for rc=WSAEINVAL (cut): '%s'", errbuf));
#   endif

    pj_strerror(PJ_EBUG, errbuf, sizeof(errbuf));
    PJ_LOG(3,(THIS_FILE, "...msg for rc=PJ_EBUG: '%s'", errbuf));
    if (my_stristr(errbuf, "BUG") == NULL) {
        PJ_LOG(3, (THIS_FILE, 
                   "...error: expecting \"BUG\" string in the msg"));
        return -20;
    }

    pj_strerror(PJ_EBUG, errbuf, CUT);
    PJ_LOG(3,(THIS_FILE, "...msg for rc=PJ_EBUG, cut at %d chars: '%s'", 
              CUT, errbuf));

    /* Perror */
    pj_perror(3, THIS_FILE, PJ_SUCCESS, "...testing %s", "pj_perror");
    PJ_PERROR(3,(THIS_FILE, PJ_SUCCESS, "...testing %s", "PJ_PERROR"));

    return 0;
}


#endif	/* INCLUDE_ERRNO_TEST */


