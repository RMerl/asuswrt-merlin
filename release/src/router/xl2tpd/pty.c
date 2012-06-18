/*
 * Layer Two Tunnelling Protocol Daemon
 * Copyright (C) 1998 Adtran, Inc.
 * Copyright (C) 2002 Jeff McAdams
 *
 * Mark Spencer
 *
 * This software is distributed under the terms
 * of the GPL, which you should have received
 * along with this source.
 *
 * Pseudo-pty allocation routines...  Concepts and code borrowed
 * from pty-redir by Magosanyi Arpad.
 *
 */

#define _ISOC99_SOURCE
#define _XOPEN_SOURCE
#define _BSD_SOURCE
#define _XOPEN_SOURCE_EXTENDED

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include "l2tp.h"



#ifdef SOLARIS
#define PTY00 "/dev/ptyXX"
#define PTY10 "pqrstuvwxyz"
#define PTY01 "0123456789abcdef"
#endif

#ifdef LINUX
#define PTY00 "/dev/ptyXX"
#define PTY10 "pqrstuvwxyzabcde"
#define PTY01 "0123456789abcdef"
#endif

#ifdef FREEBSD
#define PTY00 "/dev/ptyXX"
#define PTY10 "p"
#define PTY01 "0123456789abcdefghijklmnopqrstuv"
#endif

#ifndef OPENBSD
int getPtyMaster_pty (char *tty10, char *tty01)
{
    char *p10;
    char *p01;
    static char dev[] = PTY00;
    int fd;

    for (p10 = PTY10; *p10; p10++)
    {
        dev[8] = *p10;
        for (p01 = PTY01; *p01; p01++)
        {
            dev[9] = *p01;
            fd = open (dev, O_RDWR | O_NONBLOCK);
            if (fd >= 0)
            {
                *tty10 = *p10;
                *tty01 = *p01;
                return fd;
            }
        }
    }
    l2tp_log (LOG_CRIT, "%s: No more free pseudo-tty's\n", __FUNCTION__);
    return -1;
}

int getPtyMaster_ptmx(char *ttybuf, int ttybuflen)
{
    int fd;
    char *tty;

    fd = open("/dev/ptmx", O_RDWR);
    if (fd == -1)
    {
	l2tp_log (LOG_WARNING, "%s: unable to open /dev/ptmx to allocate pty\n",
		  __FUNCTION__);
	return -EINVAL;
    }

    /* change the onwership */
    if (grantpt(fd))
    {
	l2tp_log (LOG_WARNING, "%s: unable to grantpt() on pty\n",
		  __FUNCTION__);
	close(fd);
	return -EINVAL;
    }

    if (unlockpt(fd))
    {
	l2tp_log (LOG_WARNING, "%s: unable to unlockpt() on pty\n",
		  __FUNCTION__);
	close(fd);
	return -EINVAL;
    }

    tty = ptsname(fd);
    if (tty == NULL)
    {
	l2tp_log (LOG_WARNING, "%s: unable to obtain name of slave tty\n",
		  __FUNCTION__);
	close(fd);
	return -EINVAL;
    }
    ttybuf[0]='\0';
    strncat(ttybuf, tty, ttybuflen);

    return fd;
}
#endif
#ifdef OPENBSD
int getPtyMaster_ptm(char *ttybuf, int ttybuflen)
{
    int amaster, aslave;
    char *tty = (char*) malloc(64);

    if((openpty(&amaster, &aslave, tty, NULL, NULL)) == -1)
    {
	l2tp_log (LOG_WARNING, "%s: openpty() returned %s\n",
		__FUNCTION__, strerror(errno));
	free(tty);
	return -EINVAL;
    }

    ttybuf[0] = '\0';
    strncat(ttybuf, tty, ttybuflen);

    free(tty);

    return amaster;
}
#endif /* OPENBSD */

int getPtyMaster(char *ttybuf, int ttybuflen)
{
    int fd;
#ifndef OPENBSD
    fd = getPtyMaster_ptmx(ttybuf, ttybuflen);
    char a, b;

    if(fd >= 0) {
	return fd;
    }

    l2tp_log (LOG_WARNING, "%s: failed to use pts -- using legacy ptys\n", __FUNCTION__);
    fd = getPtyMaster_pty(&a,&b);
    
    if(fd >= 0) {
	snprintf(ttybuf, ttybuflen, "/dev/tty%c%c", a, b);
	return fd;
    }
#endif
#ifdef OPENBSD

    fd = getPtyMaster_ptm(ttybuf, ttybuflen);
    if(fd >= 0) {
        return fd;
    }
#endif /* OPENBSD */

    return -EINVAL;
}
