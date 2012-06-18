/***********************************************************************
*
* pty.c
*
* Code for dealing with pseudo-tty's for running pppd.
*
* Copyright (C) 2002 Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: pty.c 3323 2011-09-21 18:45:48Z lly.dev $";

#include "../l2tp.h"
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#ifndef N_HDLC
#include <linux/termios.h>
#endif

/**********************************************************************
* %FUNCTION: pty_get
* %ARGUMENTS:
*  mfp -- pointer to master file descriptor
*  sfp -- pointer to slave file descriptor
* %RETURNS:
*  0 on success, -1 on failure
* %DESCRIPTION:
*  Opens a PTY and sets line discipline to N_HDLC for ppp.
*  Taken almost verbatim from Linux pppd code.
***********************************************************************/
int
pty_get(int *mfp, int *sfp)
{
    char pty_name[24];
    struct termios tios;
    int mfd, sfd;
    int disc = N_HDLC;

    mfd = -1;
    sfd = -1;

    mfd = open("/dev/ptmx", O_RDWR);
    if (mfd >= 0) {
	int ptn;
	if (ioctl(mfd, TIOCGPTN, &ptn) >= 0) {
	    snprintf(pty_name, sizeof(pty_name), "/dev/pts/%d", ptn);
	    ptn = 0;
	    if (ioctl(mfd, TIOCSPTLCK, &ptn) < 0) {
		/* warn("Couldn't unlock pty slave %s: %m", pty_name); */
	    }
	    if ((sfd = open(pty_name, O_RDWR | O_NOCTTY)) < 0) {
		/* warn("Couldn't open pty slave %s: %m", pty_name); */
	    }
	}
    }

    if (sfd < 0 || mfd < 0) {
	if (sfd >= 0) close(sfd);
	if (mfd >= 0) close(mfd);
	return -1;
    }

    *mfp = mfd;
    *sfp = sfd;
    if (tcgetattr(sfd, &tios) == 0) {
	tios.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
	tios.c_cflag |= CS8 | CREAD | CLOCAL;
	tios.c_iflag  = IGNPAR;
	tios.c_oflag  = 0;
	tios.c_lflag  = 0;
	tcsetattr(sfd, TCSAFLUSH, &tios);
    }
    if (ioctl(sfd, TIOCSETD, &disc) < 0) {
	l2tp_set_errmsg("Unable to set line discipline to N_HDLC");
	close(mfd);
	close(sfd);
	return -1;
    }
    disc = N_HDLC;
    if (ioctl(mfd, TIOCSETD, &disc) < 0) {
	l2tp_set_errmsg("Unable to set line discipline to N_HDLC");
	close(mfd);
	close(sfd);
	return -1;
    }
    return 0;
}

