/*
 * ipforward value get function for solaris.
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

#include "memory.h"

int
ipforward ()
{
  int fd, ret;
  int ipforwarding = 0;
  char forward[] = "ip_forwarding";
  char *buf;
  struct strioctl si;

  buf = (char *) XMALLOC (MTYPE_TMP, sizeof forward + 1);
  strcpy (buf, forward);

  fd = open ("/dev/ip", O_RDWR);
  if (fd < 0) {
    free (buf);
    /* need logging here */
    /* "I can't get ipforwarding value because can't open /dev/ip" */
    return -1;
  }

  si.ic_cmd = ND_GET;
  si.ic_timout = 0;
  si.ic_len = strlen (buf) + 1;
  si.ic_dp = (caddr_t) buf;

  ret = ioctl (fd, I_STR, &si);
  close (fd);

  if (ret < 0) {
    free (buf);
    /* need logging here */
    /* can't get ipforwarding value : ioctl failed */
    return -1;
  }

  ipforwarding = atoi (buf);
  free (buf);
  return ipforwarding;
}

int
ipforward_on ()
{
  return 0;
}

int
ipforward_off ()
{
  return 0;
}
