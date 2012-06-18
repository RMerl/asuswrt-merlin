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

/* Read nbytes from fd and store into ptr. */
int
readn (int fd, char *ptr, int nbytes)
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
writen(int fd, char *ptr, int nbytes)
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
