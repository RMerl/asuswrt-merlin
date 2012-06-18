/*
 * $Id: restart.c,v 1.1 2009-06-30 02:31:09 steven Exp $
 * Restart Library
 *
 * ** NOTICE **
 *
 * This code is written by (and is therefore copyright) Dr Kay Robbins 
 * (krobbins@cs.utsa.edu) and Dr. Steve Robbins (srobbins@cs.utsa.edu), 
 * and was released with unspecified licensing as part of their book 
 * _UNIX_Systems_Programming_ (Prentice Hall, ISBN: 0130424110).
 *
 * Dr. Steve Robbins was kind enough to allow me to re-license this 
 * software as GPL.  I would request that any bugs or problems with 
 * this code be brought to my attention (ron@pedde.com), and I will 
 * submit appropriate patches upstream, should the problem be with
 * the original code.
 *
 * ** NOTICE **
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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include "err.h"
#include "restart.h"
#define BLKSIZE PIPE_BUF
#define MILLION 1000000L
#define D_MILLION 1000000.0

/* Private functions */

static int gettimeout(struct timeval end,
		      struct timeval *timeoutp) {
    gettimeofday(timeoutp, NULL);
    timeoutp->tv_sec = end.tv_sec - timeoutp->tv_sec;
    timeoutp->tv_usec = end.tv_usec - timeoutp->tv_usec;
    if (timeoutp->tv_usec >= MILLION) {
	timeoutp->tv_sec++;
	timeoutp->tv_usec -= MILLION;
    }
    if (timeoutp->tv_usec < 0) {
	timeoutp->tv_sec--;
	timeoutp->tv_usec += MILLION;
    }
    if ((timeoutp->tv_sec < 0) ||
	((timeoutp->tv_sec == 0) && (timeoutp->tv_usec == 0))) {
	errno = ETIME;
	return -1;
    }
    return 0;
}

/* Restart versions of traditional functions */

int r_close(int fildes) {
    int retval;
    while (retval = close(fildes), retval == -1 && errno == EINTR) ;
    return retval;
}

int r_dup2(int fildes, int fildes2) {
    int retval;
    while (retval = dup2(fildes, fildes2), retval == -1 && errno == EINTR) ;
    return retval;
}


int r_open2(const char *path, int oflag) {
    int retval;
    while (retval = open(path, oflag), retval == -1 && errno == EINTR) ;
    return retval;
}

int r_open3(const char *path, int oflag, mode_t mode) {
    int retval;
    while (retval = open(path, oflag, mode), retval == -1 && errno == EINTR) ;
    return retval;
}

ssize_t r_read(int fd, void *buf, size_t size) {
    ssize_t retval;
    while (((retval = read(fd, buf, size)) == -1) && (errno==EINTR)) {};
    return retval;
}

pid_t r_wait(int *stat_loc) {
    pid_t retval;
    while (((retval = wait(stat_loc)) == -1) && (errno == EINTR)) ;
    return retval;
}

pid_t r_waitpid(pid_t pid, int *stat_loc, int options) {
    pid_t retval;
    while (((retval = waitpid(pid, stat_loc, options)) == -1) &&
           (errno == EINTR)) ;
    return retval;
}

ssize_t r_write(int fd, void *buf, size_t size) {
    char *bufp;
    size_t bytestowrite;
    ssize_t byteswritten;
    size_t totalbytes;

    for (bufp = buf, bytestowrite = size, totalbytes = 0;
	 bytestowrite > 0;
	 bufp += byteswritten, bytestowrite -= byteswritten) {
	byteswritten = write(fd, bufp, bytestowrite);
	if ((byteswritten) == -1 && (errno != EINTR))
	    return -1;
	if (byteswritten == -1)
	    byteswritten = 0;
	totalbytes += byteswritten;
    }
    return totalbytes;
}

/* Utility functions */

struct timeval add2currenttime(double seconds) {
    struct timeval newtime;

    gettimeofday(&newtime, NULL);
    newtime.tv_sec += (int)seconds;
    newtime.tv_usec += (int)((seconds - (int)seconds)*D_MILLION + 0.5);
    if (newtime.tv_usec >= MILLION) {
	newtime.tv_sec++;
	newtime.tv_usec -= MILLION;
    }
    return newtime;
}

int copyfile(int fromfd, int tofd) {
    int bytesread;
    int totalbytes = 0;

    while ((bytesread = readwrite(fromfd, tofd)) > 0)
	totalbytes += bytesread;
    return totalbytes;
}

ssize_t readblock(int fd, void *buf, size_t size) {
    char *bufp;
    ssize_t bytesread;
    size_t bytestoread;
    size_t totalbytes;
 
    for (bufp = buf, bytestoread = size, totalbytes = 0;
	 bytestoread > 0;
	 bufp += bytesread, bytestoread -= bytesread) {
	bytesread = read(fd, bufp, bytestoread);
	if ((bytesread == 0) && (totalbytes == 0))
	    return 0;
	if (bytesread == 0) {
	    errno = EINVAL;
	    return -1;
	}  
	if ((bytesread) == -1 && (errno != EINTR))
	    return -1;
	if (bytesread == -1)
	    bytesread = 0;
	totalbytes += bytesread;
    }
    return totalbytes;
}

int readline(int fd, char *buf, int nbytes) {
    int numread = 0;
    int returnval;

    while (numread < nbytes - 1) {
	returnval = read(fd, buf + numread, 1);
	if ((returnval == -1) && (errno == EINTR))
	    continue;
	if ((returnval == 0) && (numread == 0))
	    return 0;
	if (returnval == 0)
	    break;
	if (returnval == -1)
	    return -1;
	numread++;
	if (buf[numread-1] == '\n') {
	    buf[numread] = '\0';
	    return numread;
	}  
    }   
    errno = EINVAL;
    return -1;
}

int readlinetimed(int fd, char *buf, int nbytes, double seconds) {
    int numread = 0;
    int returnval;

    while (numread < nbytes - 1) {
	returnval = (int)readtimed(fd, buf + numread, 1, seconds);
	if ((returnval == -1) && (errno == EINTR))
	    continue;
	if ((returnval == 0) && (numread == 0))
	    return 0;
	if (returnval == 0)
	    break;
	if (returnval == -1)
	    return -1;
	numread++;
	if (buf[numread-1] == '\n') {
	    buf[numread] = '\0';
	    return numread;
	}  
    }   
    errno = EINVAL;
    return -1;
}

ssize_t readtimed(int fd, void *buf, size_t nbyte, double seconds) {
    struct timeval timedone;

    timedone = add2currenttime(seconds);
    if (waitfdtimed(fd, timedone) == -1)
	return (ssize_t)(-1);
    return r_read(fd, buf, nbyte);
}

int readwrite(int fromfd, int tofd) {
    char buf[BLKSIZE];
    int bytesread;

    if ((bytesread = r_read(fromfd, buf, BLKSIZE)) < 0)
	return -1;
    if (bytesread == 0)
	return 0;
    if (r_write(tofd, buf, bytesread) < 0)
	return -1;
    return bytesread;
}

int readwriteblock(int fromfd, int tofd, char *buf, int size) {
    int bytesread;

    bytesread = readblock(fromfd, buf, size);
    if (bytesread != size)         /* can only be 0 or -1 */
	return bytesread;
    return r_write(tofd, buf, size);
}

int waitfdtimed(int fd, struct timeval end) {
    fd_set readset;
    int retval;
    struct timeval timeout;
 
    if ((fd < 0) || (fd >= FD_SETSIZE)) {
	errno = EINVAL;
	return -1;
    }  
    FD_ZERO(&readset);
    FD_SET(fd, &readset);
    if (gettimeout(end, &timeout) == -1)
	return -1;
    while (((retval = select(fd+1, &readset, NULL, NULL, &timeout)) == -1)
           && (errno == EINTR)) {
	if (gettimeout(end, &timeout) == -1)
	    return -1;
	FD_ZERO(&readset);
	FD_SET(fd, &readset);
    }
    if (retval == 0) {
	errno = ETIME;
	return -1;
    }
    if (retval == -1)
	return -1;
    return 0;
}
