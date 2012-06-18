/***********************************************************************
*
* cmd-control.c
*
* Simple command-line program for sending commands to L2TP daemon
*
* Copyright (C) 2002 by Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: cmd-control.c 3323 2011-09-21 18:45:48Z lly.dev $";
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/uio.h>


/**********************************************************************
* %FUNCTION: send_cmd
* %ARGUMENTS:
*  cmd -- command to send to server
* %RETURNS:
*  file descriptor for channel to server
* %DESCRIPTION:
*  Sends a command to the server
***********************************************************************/
static int
send_cmd(char const *cmd)
{
    struct sockaddr_un addr;
    int fd;
    struct iovec v[2];

    /* Do not send zero-length command */
    if (!*cmd) {
	errno = ESPIPE;
	return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, "/var/run/l2tpctrl", sizeof(addr.sun_path) - 1);

    fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (fd < 0) {
	perror("socket");
	return -1;
    }
    if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
	perror("connect");
	close(fd);
	return -1;
    }
    v[0].iov_base = (char *) cmd;
    v[0].iov_len = strlen(cmd);
    v[1].iov_base = "\n";
    v[1].iov_len = 1;
    writev(fd, v, 2);
    return fd;
}

int
main(int argc, char *argv[])
{
    int fd;
    int n;
    char buf[4096];

    if (argc != 2) {
	fprintf(stderr, "Usage: %s command\n", argv[0]);
	return 1;
    }

    fd = send_cmd(argv[1]);
    if (fd < 0) {
	return 1;
    }

    for(;;) {
	n = read(fd, buf, sizeof(buf));
	if (n < 0) {
	    perror("read");
	    printf("\n");
	    close(fd);
	    return 1;
	}
	if (n == 0) {
	    printf("\n");
	    close(fd);
	    return 0;
	}
	write(1, buf, n);
    }
}

