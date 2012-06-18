/* RIP peer support
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

#include <zebra.h>

#include "if.h"
#include "prefix.h"
#include "command.h"
#include "linklist.h"
#include "thread.h"
#include "memory.h"

#include "ripd/ripd.h"

/* Linked list of RIP peer. */
struct list *peer_list;

struct rip_peer *
rip_peer_new ()
{
  struct rip_peer *new;

  new = XMALLOC (MTYPE_RIP_PEER, sizeof (struct rip_peer));
  memset (new, 0, sizeof (struct rip_peer));
  return new;
}

void
rip_peer_free (struct rip_peer *peer)
{
  XFREE (MTYPE_RIP_PEER, peer);
}

struct rip_peer *
rip_peer_lookup (struct in_addr *addr)
{
  struct rip_peer *peer;
  struct listnode *nn;

  LIST_LOOP (peer_list, peer, nn)
    {
      if (IPV4_ADDR_SAME (&peer->addr, addr))
	return peer;
    }
  return NULL;
}

struct rip_peer *
rip_peer_lookup_next (struct in_addr *addr)
{
  struct rip_peer *peer;
  struct listnode *nn;

  LIST_LOOP (peer_list, peer, nn)
    {
      if (htonl (peer->addr.s_addr) > htonl (addr->s_addr))
	return peer;
    }
  return NULL;
}

/* RIP peer is timeout. */
int
rip_peer_timeout (struct thread *t)
{
  struct rip_peer *peer;

  peer = THREAD_ARG (t);
  listnode_delete (peer_list, peer);
  rip_peer_free (peer);

  return 0;
}

/* Get RIP peer.  At the same time update timeout thread. */
struct rip_peer *
rip_peer_get (struct in_addr *addr)
{
  struct rip_peer *peer;

  peer = rip_peer_lookup (addr);

  if (peer)
    {
      if (peer->t_timeout)
	thread_cancel (peer->t_timeout);
    }
  else
    {
      peer = rip_peer_new ();
      peer->addr = *addr;
      listnode_add_sort (peer_list, peer);
    }

  /* Update timeout thread. */
  peer->t_timeout = thread_add_timer (master, rip_peer_timeout, peer,
				      RIP_PEER_TIMER_DEFAULT);

  /* Last update time set. */
  time (&peer->uptime);
  
  return peer;
}

void
rip_peer_update (struct sockaddr_in *from, u_char version)
{
  struct rip_peer *peer;
  peer = rip_peer_get (&from->sin_addr);
  peer->version = version;
}

void
rip_peer_bad_route (struct sockaddr_in *from)
{
  struct rip_peer *peer;
  peer = rip_peer_get (&from->sin_addr);
  peer->recv_badroutes++;
}

void
rip_peer_bad_packet (struct sockaddr_in *from)
{
  struct rip_peer *peer;
  peer = rip_peer_get (&from->sin_addr);
  peer->recv_badpackets++;
}

/* Display peer uptime. */
char *
rip_peer_uptime (struct rip_peer *peer, char *buf, size_t len)
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
rip_peer_display (struct vty *vty)
{
  struct rip_peer *peer;
  struct listnode *nn;
#define RIP_UPTIME_LEN 25
  char timebuf[RIP_UPTIME_LEN];

  LIST_LOOP (peer_list, peer, nn)
    {
      vty_out (vty, "    %-16s %9d %9d %9d   %s%s", inet_ntoa (peer->addr),
	       peer->recv_badpackets, peer->recv_badroutes,
	       ZEBRA_RIP_DISTANCE_DEFAULT,
	       rip_peer_uptime (peer, timebuf, RIP_UPTIME_LEN),
	       VTY_NEWLINE);
    }
}

int
rip_peer_list_cmp (struct rip_peer *p1, struct rip_peer *p2)
{
  return htonl (p1->addr.s_addr) > htonl (p2->addr.s_addr);
}

void
rip_peer_init ()
{
  peer_list = list_new ();
  peer_list->cmp = (int (*)(void *, void *)) rip_peer_list_cmp;
}
