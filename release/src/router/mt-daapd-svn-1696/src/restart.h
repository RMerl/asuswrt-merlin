/*
 * $Id: restart.h 779 2006-02-26 08:46:24Z rpedde $
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <fcntl.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <sys/types.h>

#ifndef ETIME
#define ETIME ETIMEDOUT
#endif

struct timeval add2currenttime(double seconds);
int copyfile(int fromfd, int tofd);
int r_fdprintf(int fd, char *fmt, ...);
int r_close(int fildes);
int r_dup2(int fildes, int fildes2);
int r_open2(const char *path, int oflag);
ssize_t r_read(int fd, void *buf, size_t size);
ssize_t r_write(int fd, void *buf, size_t size);
ssize_t readblock(int fd, void *buf, size_t size);
int readline(int fd, char *buf, int nbytes);
int readlinetimed(int fd, char *buf, int nbytes, double seconds);
ssize_t readtimed(int fd, void *buf, size_t nbyte, double seconds);
int readwrite(int fromfd, int tofd);
int readwriteblock(int fromfd, int tofd, char *buf, int size);
int waitfdtimed(int fd, struct timeval end);
