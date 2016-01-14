/*
 * Kernel routing table read by sysctl function.
 * Copyright (C) 1997, 98 Kunihiro Ishiguro
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
#include "log.h"

#include "zebra/zserv.h"
#include "zebra/rt.h"
#include "zebra/kernel_socket.h"

/* Kernel routing table read up by sysctl function. */
void
route_read (void)
{
  caddr_t buf, end, ref;
  size_t bufsiz;
  struct rt_msghdr *rtm;
  
#define MIBSIZ 6
  int mib[MIBSIZ] = 
  {
    CTL_NET,
    PF_ROUTE,
    0,
    0,
    NET_RT_DUMP,
    0
  };
		      
  /* Get buffer size. */
  if (sysctl (mib, MIBSIZ, NULL, &bufsiz, NULL, 0) < 0) 
    {
      zlog_warn ("sysctl fail: %s", safe_strerror (errno));
      return;
    }

  /* Allocate buffer. */
  ref = buf = XMALLOC (MTYPE_TMP, bufsiz);
  
  /* Read routing table information by calling sysctl(). */
  if (sysctl (mib, MIBSIZ, buf, &bufsiz, NULL, 0) < 0) 
    {
      zlog_warn ("sysctl() fail by %s", safe_strerror (errno));
      return;
    }

  for (end = buf + bufsiz; buf < end; buf += rtm->rtm_msglen) 
    {
      rtm = (struct rt_msghdr *) buf;
      /* We must set RTF_DONE here, so rtm_read() doesn't ignore the message. */
      SET_FLAG (rtm->rtm_flags, RTF_DONE);
      rtm_read (rtm);
    }

  /* Free buffer. */
  XFREE (MTYPE_TMP, ref);

  return;
}
