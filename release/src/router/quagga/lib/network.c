/*
 * Network library.
 * Copyright (C) 1997 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>
#include "log.h"
#include "network.h"

/* Read nbytes from fd and store into ptr. */
int
readn (int fd, u_char *ptr, int nbytes)
{
  int nleft;
  int nread;

  nleft = nbytes;

  while (nleft > 0) 
    {
      nread = read (fd, ptr, nleft);

      if (nread < 0) 
	return (nread);
      else
	if (nread == 0) 
	  break;

      nleft -= nread;
      ptr += nread;
    }

  return nbytes - nleft;
}  

/* Write nbytes from ptr to fd. */
int
writen(int fd, const u_char *ptr, int nbytes)
{
  int nleft;
  int nwritten;

  nleft = nbytes;

  while (nleft > 0) 
    {
      nwritten = write(fd, ptr, nleft);
      
      if (nwritten <= 0) 
	return (nwritten);

      nleft -= nwritten;
      ptr += nwritten;
    }
  return nbytes - nleft;
}

int
set_nonblocking(int fd)
{
  int flags;

  /* According to the Single UNIX Spec, the return value for F_GETFL should
     never be negative. */
  if ((flags = fcntl(fd, F_GETFL)) < 0)
    {
      zlog_warn("fcntl(F_GETFL) failed for fd %d: %s",
      		fd, safe_strerror(errno));
      return -1;
    }
  if (fcntl(fd, F_SETFL, (flags | O_NONBLOCK)) < 0)
    {
      zlog_warn("fcntl failed setting fd %d non-blocking: %s",
      		fd, safe_strerror(errno));
      return -1;
    }
  return 0;
}
