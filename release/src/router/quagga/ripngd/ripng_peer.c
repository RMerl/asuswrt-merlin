/* RIPng peer support
 * Copyright (C) 2000 Kunihiro Ishiguro <kunihiro@zebra.org>
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

/* RIPng support added by Vincent Jardin <vincent.jardin@6wind.com>
 * Copyright (C) 2002 6WIND
 */

#include <zebra.h>

#include "if.h"
#include "prefix.h"
#include "command.h"
#include "linklist.h"
#include "thread.h"
#include "memory.h"

#include "ripngd/ripngd.h"
#include "ripngd/ripng_nexthop.h"


/* Linked list of RIPng peer. */
struct list *peer_list;

static struct ripng_peer *
ripng_peer_new (void)
{
  return XCALLOC (MTYPE_RIPNG_PEER, sizeof (struct ripng_peer));
}

static void
ripng_peer_free (struct ripng_peer *peer)
{
  XFREE (MTYPE_RIPNG_PEER, peer);
}

struct ripng_peer *
ripng_peer_lookup (struct in6_addr *addr)
{
  struct ripng_peer *peer;
  struct listnode *node, *nnode;

  for (ALL_LIST_ELEMENTS (peer_list, node, nnode, peer))
    {
      if (IPV6_ADDR_SAME (&peer->addr, addr))
	return peer;
    }
  return NULL;
}

struct ripng_peer *
ripng_peer_lookup_next (struct in6_addr *addr)
{
  struct ripng_peer *peer;
  struct listnode *node, *nnode;

  for (ALL_LIST_ELEMENTS (peer_list, node, nnode, peer))
    {
      if (addr6_cmp(&peer->addr, addr) > 0) 
	return peer;
    }
  return NULL;
}

/* RIPng peer is timeout.
 * Garbage collector.
 **/
static int
ripng_peer_timeout (struct thread *t)
{
  struct ripng_peer *peer;

  peer = THREAD_ARG (t);
  listnode_delete (peer_list, peer);
  ripng_peer_free (peer);

  return 0;
}

/* Get RIPng peer.  At the same time update timeout thread. */
static struct ripng_peer *
ripng_peer_get (struct in6_addr *addr)
{
  struct ripng_peer *peer;

  peer = ripng_peer_lookup (addr);

  if (peer)
    {
      if (peer->t_timeout)
	thread_cancel (peer->t_timeout);
    }
  else
    {
      peer = ripng_peer_new ();
      peer->addr = *addr; /* XXX */
      listnode_add_sort (peer_list, peer);
    }

  /* Update timeout thread. */
  peer->t_timeout = thread_add_timer (master, ripng_peer_timeout, peer,
				      RIPNG_PEER_TIMER_DEFAULT);

  /* Last update time set. */
  time (&peer->uptime);
  
  return peer;
}

void
ripng_peer_update (struct sockaddr_in6 *from, u_char version)
{
  struct ripng_peer *peer;
  peer = ripng_peer_get (&from->sin6_addr);
  peer->version = version;
}

void
ripng_peer_bad_route (struct sockaddr_in6 *from)
{
  struct ripng_peer *peer;
  peer = ripng_peer_get (&from->sin6_addr);
  peer->recv_badroutes++;
}

void
ripng_peer_bad_packet (struct sockaddr_in6 *from)
{
  struct ripng_peer *peer;
  peer = ripng_peer_get (&from->sin6_addr);
  peer->recv_badpackets++;
}

/* Display peer uptime. */
static char *
ripng_peer_uptime (struct ripng_peer *peer, char *buf, size_t len)
{
  time_t uptime;
  struct tm *tm;

  /* If there is no connection has been done before print `never'. */
  if (peer->uptime == 0)
    {
      snprintf (buf, len, "never   ");
      return buf;
    }

  /* Get current time. */
  uptime = time (NULL);
  uptime -= peer->uptime;
  tm = gmtime (&uptime);

  /* Making formatted timer strings. */
#define ONE_DAY_SECOND 60*60*24
#define ONE_WEEK_SECOND 60*60*24*7

  if (uptime < ONE_DAY_SECOND)
    snprintf (buf, len, "%02d:%02d:%02d", 
	      tm->tm_hour, tm->tm_min, tm->tm_sec);
  else if (uptime < ONE_WEEK_SECOND)
    snprintf (buf, len, "%dd%02dh%02dm", 
	      tm->tm_yday, tm->tm_hour, tm->tm_min);
  else
    snprintf (buf, len, "%02dw%dd%02dh", 
	      tm->tm_yday/7, tm->tm_yday - ((tm->tm_yday/7) * 7), tm->tm_hour);
  return buf;
}

void
ripng_peer_display (struct vty *vty)
{
  struct ripng_peer *peer;
  struct listnode *node, *nnode;
#define RIPNG_UPTIME_LEN 25
  char timebuf[RIPNG_UPTIME_LEN];

  for (ALL_LIST_ELEMENTS (peer_list, node, nnode, peer))
    {
      vty_out (vty, "    %s %s%14s %10d %10d %10d      %s%s", inet6_ntoa (peer->addr),
               VTY_NEWLINE, " ",
	       peer->recv_badpackets, peer->recv_badroutes,
	       ZEBRA_RIPNG_DISTANCE_DEFAULT,
	       ripng_peer_uptime (peer, timebuf, RIPNG_UPTIME_LEN),
	       VTY_NEWLINE);
    }
}

static int
ripng_peer_list_cmp (struct ripng_peer *p1, struct ripng_peer *p2)
{
  return addr6_cmp(&p1->addr, &p2->addr) > 0;
}

void
ripng_peer_init ()
{
  peer_list = list_new ();
  peer_list->cmp = (int (*)(void *, void *)) ripng_peer_list_cmp;
}
