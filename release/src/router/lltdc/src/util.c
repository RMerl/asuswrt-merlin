/*
 * LICENSE NOTICE.
 *
 * Use of the Microsoft Windows Rally Development Kit is covered under
 * the Microsoft Windows Rally Development Kit License Agreement,
 * which is provided within the Microsoft Windows Rally Development
 * Kit or at http://www.microsoft.com/whdc/rally/rallykit.mspx. If you
 * want a license from Microsoft to use the software in the Microsoft
 * Windows Rally Development Kit, you must (1) complete the designated
 * "licensee" information in the Windows Rally Development Kit License
 * Agreement, and (2) sign and return the Agreement AS IS to Microsoft
 * at the address provided in the Agreement.
 */

/*
 * Copyright (c) Microsoft Corporation 2005.  All rights reserved.
 * This software is provided with NO WARRANTY.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <syslog.h>

#include "globals.h"

static bool_t use_syslog;

void
cpy_hton64(void* destination, void* source)
{
#ifdef __LITTLE_ENDIAN
    uint8_t*	src = ((uint8_t*) source) + 7;
    uint8_t*	dst = (uint8_t*) destination;

    *dst++ = *src--;
    *dst++ = *src--;
    *dst++ = *src--;
    *dst++ = *src--;
    *dst++ = *src--;
    *dst++ = *src--;
    *dst++ = *src--;
    *dst++ = *src--;
#else   // BIG-ENDIAN below
    uint8_t*	src = (uint8_t*) source;
    uint8_t*	dst = (uint8_t*) destination;

    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
#endif
}


void
die(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    if (use_syslog)
    {
	vsyslog(LOG_ERR, msg, ap);
    }
    else
    {
	fprintf(stderr, "%s: ERROR: ", g_Progname);
	vfprintf(stderr, msg, ap);
    }
    va_end(ap);

    exit(1);
}


void
warn(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    if (use_syslog)
    {
	vsyslog(LOG_WARNING, msg, ap);
    }
    else
    {
	fprintf(stderr, "%s: warning: ", g_Progname);
	vfprintf(stderr, msg, ap);
    }
    va_end(ap);
}


void
dbgprintf(const char *msg, ...)
{
#ifdef __DEBUG__
    va_list ap;

    va_start(ap, msg);
    if (use_syslog)
	vsyslog(LOG_INFO, msg, ap);
    else
	vfprintf(stdout, msg, ap);
    va_end(ap);
#endif
}



void *
xmalloc(size_t nbytes)
{
    void *p = malloc(nbytes);
    if (!p)
	die("xmalloc: allocation of " FMT_SIZET " bytes failed", nbytes);
    else
	return p;
}


void
xfree(void *p)
{
    if (p)
	free(p);
}

void
util_use_syslog(void)
{
    if (!use_syslog)
    {
	openlog(g_Progname, LOG_PID | LOG_CONS, LOG_DAEMON);
	use_syslog = TRUE;
    }
}

/* Pick a random number uniformly distributed in interval [0, upperlimit] (inclusive) */
/* This implementation promotes to double internally to avoid overflow
 * issues.  If you are in an environment without floating point (eg
 * embedded or kernel) then you probably know enough about your rand()
 * implementation to avoid overflow. */
uint32_t
random_uniform(uint32_t upperlimit)
{
    return (int)(((double)rand()) / ((double)RAND_MAX) * ((double)upperlimit));
}



/* Convert the ASCII string starting at "src" to UCS-2 (little-endian) and
 * copy it into "dst", using no more than "dst_bytes".  If truncation is required,
 * ensure "dst" is safely terminated by a double-\0. */
void
util_copy_ascii_to_ucs2(ucs2char_t *dst, size_t dst_bytes, const char *src)
{
    uint8_t *d = (uint8_t*)dst;

    while (dst_bytes > 3 && *src != '\0')
    {
	/* UCS-2 includes ASCII as first 256 chars from 0x0000 to 0x00ff,
	 * so conversion is quite easy */
	*d++ = *src++;	/* low-byte is ascii value */
	*d++ = 0;	/* hi-byte is 0 */
	dst_bytes -= 2;
    }

    /* null-terminate dst */
    *d++ = 0;
    *d++ = 0;
}
