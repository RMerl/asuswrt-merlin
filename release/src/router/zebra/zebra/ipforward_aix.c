/*
 * ipforward value get function for aix.
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

int
ipforward ()
{
  int fd, ret;
  int af = AF_INET;
  char netopt[] = "ipforwarding";
  struct optreq oq;

  fd = socket(af, SOCK_DGRAM, 0);
  if (fd < 0) {
    /* need logging here */
    return -1;
  }

  strcpy (oq.name, netopt);
  oq.getnext = 0;

  ret = ioctl (fd, SIOCGNETOPT, (caddr_t)&oq);
  close(fd);

  if (ret < 0) {
    /* need logging here */
    return -1;
  }

  ret = atoi (oq.data);
  return ret;
}

int
ipforward_on ()
{
  ;
}

int
ipforward_off ()
{
  ;
}
