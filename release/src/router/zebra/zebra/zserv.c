/* Zebra daemon server routine.
 * Copyright (C) 1997, 98, 99 Kunihiro Ishiguro
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA.  
 */

#include <zebra.h>

#include "prefix.h"
#include "command.h"
#include "if.h"
#include "thread.h"
#include "stream.h"
#include "memory.h"
#include "table.h"
#include "rib.h"
#include "network.h"
#include "sockunion.h"
#include "log.h"
#include "zclient.h"

#include "zebra/zserv.h"
#include "zebra/redistribute.h"
#include "zebra/debug.h"
#include "zebra/ipforward.h"

/* Event list of zebra. */
enum event { ZEBRA_SERV, ZEBRA_READ, ZEBRA_WRITE };

/* Zebra client list. */
list client_list;

/* Default rtm_table for all clients */
int rtm_table_default = 0;

void zebra_event (enum event event, int sock, struct zserv *client);

extern struct thread_master *master;

/* For logging of zebra meesages. */
char *zebra_command_str [] =
{
  "NULL",
  "ZEBRA_INTERFACE_ADD",
  "ZEBRA_INTERFACE_DELETE",
  "ZEBRA_INTERFACE_ADDRESS_ADD",
  "ZEBRA_INTERFACE_ADDRESS_DELETE",
  "ZEBRA_INTERFACE_UP",
  "ZEBRA_INTERFACE_DOWN",
  "ZEBRA_IPV4_ROUTE_ADD",
  "ZEBRA_IPV4_ROUTE_DELETE",
  "ZEBRA_IPV6_ROUTE_ADD",
  "ZEBRA_IPV6_ROUTE_DELETE",
  "ZEBRA_REDISTRIBUTE_ADD",
  "ZEBRA_REDISTRIBUTE_DELETE",
  "ZEBRA_REDISTRIBUTE_DEFAULT_ADD",
  "ZEBRA_REDISTRIBUTE_DEFAULT_DELETE",
  "ZEBRA_IPV4_NEXTHOP_LOOKUP",
  "ZEBRA_IPV6_NEXTHOP_LOOKUP",
  "ZEBRA_IPV4_IMPORT_LOOKUP",
  "ZEBRA_IPV6_IMPORT_LOOKUP"
};

struct zebra_message_queue
{
  struct nsm_message_queue *next;
  struct nsm_message_queue *prev;

  u_char *buf;
  u_int16_t length;
  u_int16_t written;
};

struct thread *t_write;
struct fifo message_queue;

int
zebra_server_dequeue (struct thread *t)
{
  int sock;
  int nbytes;
  struct zebra_message_queue *queue;

  sock = THREAD_FD (t);
  t_write = NULL;

  queue = (struct zebra_message_queue *) FIFO_HEAD (&message_queue);
  if (queue)
    {
      nbytes = write (sock, queue->buf + queue->written,
		      queue->length - queue->written);

      if (nbytes <= 0)
        {
          if (errno != EAGAIN)
	    return -1;
        }
      else if (nbytes != (queue->length - queue->written))
	{
	  queue->written += nbytes;
	}
      else
        {
          FIFO_DEL (queue);
          XFREE (MTYPE_TMP, queue->buf);
          XFREE (MTYPE_TMP, queue);
        }
    }

  if (FIFO_TOP (&message_queue))
    THREAD_WRITE_ON (master, t_write, zebra_server_dequeue, NULL, sock);

  return 0;
}

/* Enqueu message.  */
void
zebra_server_enqueue (int sock, u_char *buf, unsigned long length,
		      unsigned long written)
{
  struct zebra_message_queue *queue;

  queue = XCALLOC (MTYPE_TMP, sizeof (struct zebra_message_queue));
  queue->buf = XMALLOC (MTYPE_TMP, length);
  memcpy (queue->buf, buf, length);
  queue->length = length;
  queue->written = written;

  FIFO_ADD (&message_queue, queue);

  THREAD_WRITE_ON (master, t_write, zebra_server_dequeue, NULL, sock);
}

int
zebra_server_send_message (int sock, u_char *buf, unsigned long length)
{
  int nbytes;

  if (FIFO_TOP (&message_queue))
    {
      zebra_server_enqueue (sock, buf, length, 0);
      return 0;
    }

  /* Send message.  */
  nbytes = write (sock, buf, length);

  if (nbytes <= 0)
    {
      if (errno == EAGAIN)
        zebra_server_enqueue (sock, buf, length, 0);
      else
	return -1;
    }
  else if (nbytes != length)
    zebra_server_enqueue (sock, buf, length, nbytes);

  return 0;
}

/* Interface is added. Send ZEBRA_INTERFACE_ADD to client. */
int
zsend_interface_add (struct zserv *client, struct interface *ifp)
{
  struct stream *s;

  /* Check this client need interface information. */
  if (! client->ifinfo)
    return -1;

  s = client->obuf;
  stream_reset (s);

  /* Place holder for size. */
  stream_putw (s, 0);

  /* Message type. */
  stream_putc (s, ZEBRA_INTERFACE_ADD);

  /* Interface information. */
  stream_put (s, ifp->name, INTERFACE_NAMSIZ);
  stream_putl (s, ifp->ifindex);
  stream_putl (s, ifp->flags);
  stream_putl (s, ifp->metric);
  stream_putl (s, ifp->mtu);
  stream_putl (s, ifp->bandwidth);
#ifdef HAVE_SOCKADDR_DL
  stream_put (s, &ifp->sdl, sizeof (ifp->sdl));
#else
  stream_putl (s, ifp->hw_addr_len);
  if (ifp->hw_addr_len)
    stream_put (s, ifp->hw_addr, ifp->hw_addr_len);
#endif /* HAVE_SOCKADDR_DL */

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

/* Interface deletion from zebra daemon. */
int
zsend_interface_delete (struct zserv *client, struct interface *ifp)
{
  struct stream *s;

  /* Check this client need interface information. */
  if (! client->ifinfo)
    return -1;

  s = client->obuf;
  stream_reset (s);

  /* Packet length placeholder. */
  stream_putw (s, 0);

  /* Interface information. */
  stream_putc (s, ZEBRA_INTERFACE_DELETE);
  stream_put (s, ifp->name, INTERFACE_NAMSIZ);
  stream_putl (s, ifp->ifindex);
  stream_putl (s, ifp->flags);
  stream_putl (s, ifp->metric);
  stream_putl (s, ifp->mtu);
  stream_putl (s, ifp->bandwidth);

  /* Write packet length. */
  stream_putw_at (s, 0, stream_get_endp (s));

  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

/* Interface address is added. Send ZEBRA_INTERFACE_ADDRESS_ADD to the
   client. */
int
zsend_interface_address_add (struct zserv *client, struct interface *ifp, 
			     struct connected *ifc)
{
  int blen;
  struct stream *s;
  struct prefix *p;

  /* Check this client need interface information. */
  if (! client->ifinfo)
    return -1;

  s = client->obuf;
  stream_reset (s);

  /* Place holder for size. */
  stream_putw (s, 0);

  stream_putc (s, ZEBRA_INTERFACE_ADDRESS_ADD);
  stream_putl (s, ifp->ifindex);

  /* Interface address flag. */
  stream_putc (s, ifc->flags);

  /* Prefix information. */
  p = ifc->address;
  stream_putc (s, p->family);
  blen = prefix_blen (p);
  stream_put (s, &p->u.prefix, blen);
  stream_putc (s, p->prefixlen);

  /* Destination. */
  p = ifc->destination;
  if (p)
    stream_put (s, &p->u.prefix, blen);
  else
    stream_put (s, NULL, blen);

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

/* Interface address is deleted. Send ZEBRA_INTERFACE_ADDRESS_DELETE
   to the client. */
int
zsend_interface_address_delete (struct zserv *client, struct interface *ifp,
				struct connected *ifc)
{
  int blen;
  struct stream *s;
  struct prefix *p;

  /* Check this client need interface information. */
  if (! client->ifinfo)
    return -1;

  s = client->obuf;
  stream_reset (s);

  /* Place holder for size. */
  stream_putw (s, 0);

  stream_putc (s, ZEBRA_INTERFACE_ADDRESS_DELETE);
  stream_putl (s, ifp->ifindex);

  /* Interface address flag. */
  stream_putc (s, ifc->flags);

  /* Prefix information. */
  p = ifc->address;
  stream_putc (s, p->family);
  blen = prefix_blen (p);
  stream_put (s, &p->u.prefix, blen);

  p = ifc->destination;
  if (p)
    stream_put (s, &p->u.prefix, blen);
  else
    stream_put (s, NULL, blen);

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

int
zsend_interface_up (struct zserv *client, struct interface *ifp)
{
  struct stream *s;

  /* Check this client need interface information. */
  if (! client->ifinfo)
    return -1;

  s = client->obuf;
  stream_reset (s);

  /* Place holder for size. */
  stream_putw (s, 0);

  /* Zebra command. */
  stream_putc (s, ZEBRA_INTERFACE_UP);

  /* Interface information. */
  stream_put (s, ifp->name, INTERFACE_NAMSIZ);
  stream_putl (s, ifp->ifindex);
  stream_putl (s, ifp->flags);
  stream_putl (s, ifp->metric);
  stream_putl (s, ifp->mtu);
  stream_putl (s, ifp->bandwidth);

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

int
zsend_interface_down (struct zserv *client, struct interface *ifp)
{
  struct stream *s;

  /* Check this client need interface information. */
  if (! client->ifinfo)
    return -1;

  s = client->obuf;
  stream_reset (s);

  /* Place holder for size. */
  stream_putw (s, 0);

  /* Zebra command. */
  stream_putc (s, ZEBRA_INTERFACE_DOWN);

  /* Interface information. */
  stream_put (s, ifp->name, INTERFACE_NAMSIZ);
  stream_putl (s, ifp->ifindex);
  stream_putl (s, ifp->flags);
  stream_putl (s, ifp->metric);
  stream_putl (s, ifp->mtu);
  stream_putl (s, ifp->bandwidth);

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

int
zsend_ipv4_add_multipath (struct zserv *client, struct prefix *p, 
			  struct rib *rib)
{
  int psize;
  struct stream *s;
  struct nexthop *nexthop;
  struct in_addr empty;

  empty.s_addr = 0;
  s = client->obuf;
  stream_reset (s);

  /* Place holder for size. */
  stream_putw (s, 0);

  /* Put command, type and nexthop. */
  stream_putc (s, ZEBRA_IPV4_ROUTE_ADD);
  stream_putc (s, rib->type);
  stream_putc (s, rib->flags);
  stream_putc (s, ZAPI_MESSAGE_NEXTHOP | ZAPI_MESSAGE_IFINDEX | ZAPI_MESSAGE_METRIC);

  /* Prefix. */
  psize = PSIZE (p->prefixlen);
  stream_putc (s, p->prefixlen);
  stream_write (s, (u_char *)&p->u.prefix, psize);

  /* Nexthop */
  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
    {
      if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
	{
	  stream_putc (s, 1);

	  if (nexthop->type == NEXTHOP_TYPE_IPV4
	      || nexthop->type == NEXTHOP_TYPE_IPV4_IFINDEX)
	    stream_put_in_addr (s, &nexthop->gate.ipv4);
	  else
	    stream_put_in_addr (s, &empty);

	  /* Interface index. */
	  stream_putc (s, 1);
	  stream_putl (s, nexthop->ifindex);

	  break;
	}
    }

  /* Metric */
  stream_putl (s, rib->metric);

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

int
zsend_ipv4_delete_multipath (struct zserv *client, struct prefix *p,
			     struct rib *rib)
{
  int psize;
  struct stream *s;
  struct nexthop *nexthop;
  struct in_addr empty;

  empty.s_addr = 0;

  s = client->obuf;
  stream_reset (s);

  /* Place holder for size. */
  stream_putw (s, 0);

  /* Put command, type and nexthop. */
  stream_putc (s, ZEBRA_IPV4_ROUTE_DELETE);
  stream_putc (s, rib->type);
  stream_putc (s, rib->flags);
  stream_putc (s, ZAPI_MESSAGE_NEXTHOP|ZAPI_MESSAGE_IFINDEX);

  /* Prefix. */
  psize = PSIZE (p->prefixlen);
  stream_putc (s, p->prefixlen);
  stream_write (s, (u_char *)&p->u.prefix, psize);

  /* Nexthop */
  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
    {
      if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
	{
	  stream_putc (s, 1);

	  if (nexthop->type == NEXTHOP_TYPE_IPV4)
	    stream_put_in_addr (s, &nexthop->gate.ipv4);
	  else
	    stream_put_in_addr (s, &empty);

	  /* Interface index. */
	  stream_putc (s, 1);
	  stream_putl (s, nexthop->ifindex);

	  break;
	}
    }

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

int
zsend_ipv4_add (struct zserv *client, int type, int flags,
		struct prefix_ipv4 *p, struct in_addr *nexthop,
		unsigned int ifindex)
{
  int psize;
  struct stream *s;

  s = client->obuf;
  stream_reset (s);

  /* Place holder for size. */
  stream_putw (s, 0);

  /* Put command, type and nexthop. */
  stream_putc (s, ZEBRA_IPV4_ROUTE_ADD);
  stream_putc (s, type);
  stream_putc (s, flags);
  stream_putc (s, ZAPI_MESSAGE_NEXTHOP|ZAPI_MESSAGE_IFINDEX);

  /* Prefix. */
  psize = PSIZE (p->prefixlen);
  stream_putc (s, p->prefixlen);
  stream_write (s, (u_char *)&p->prefix, psize);

  /* Nexthop */
  stream_putc (s, 1);
  stream_put_in_addr (s, nexthop);

  /* Interface index. */
  stream_putc (s, 1);
  stream_putl (s, ifindex);

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

int
zsend_ipv4_delete (struct zserv *client, int type, int flags,
		   struct prefix_ipv4 *p, struct in_addr *nexthop,
		   unsigned int ifindex)
{
  int psize;
  struct stream *s;

  s = client->obuf;
  stream_reset (s);

  /* Place holder for size. */
  stream_putw (s, 0);

  /* Put command, type and nexthop. */
  stream_putc (s, ZEBRA_IPV4_ROUTE_DELETE);
  stream_putc (s, type);
  stream_putc (s, flags);
  stream_putc (s, ZAPI_MESSAGE_NEXTHOP|ZAPI_MESSAGE_IFINDEX);

  /* Prefix. */
  psize = PSIZE (p->prefixlen);
  stream_putc (s, p->prefixlen);
  stream_write (s, (u_char *)&p->prefix, psize);

  /* Nexthop */
  stream_putc (s, 1);
  stream_put_in_addr (s, nexthop);

  /* Interface index. */
  stream_putc (s, 1);
  stream_putl (s, ifindex);

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

#ifdef HAVE_IPV6
int
zsend_ipv6_add (struct zserv *client, int type, int flags,
		struct prefix_ipv6 *p, struct in6_addr *nexthop,
		unsigned int ifindex)
{
  int psize;
  struct stream *s;

  s = client->obuf;
  stream_reset (s);

  /* Place holder for size. */
  stream_putw (s, 0);

  /* Put command, type and nexthop. */
  stream_putc (s, ZEBRA_IPV6_ROUTE_ADD);
  stream_putc (s, type);
  stream_putc (s, flags);
  stream_putc (s, ZAPI_MESSAGE_NEXTHOP|ZAPI_MESSAGE_IFINDEX);

  /* Prefix. */
  psize = PSIZE (p->prefixlen);
  stream_putc (s, p->prefixlen);
  stream_write (s, (u_char *)&p->prefix, psize);

  /* Nexthop */
  stream_putc (s, 1);
  stream_write (s, (u_char *)nexthop, 16);

  /* Interface index. */
  stream_putc (s, 1);
  stream_putl (s, ifindex);

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

int
zsend_ipv6_add_multipath (struct zserv *client, struct prefix *p,
			  struct rib *rib)
{
  int psize;
  struct stream *s;
  struct nexthop *nexthop;
  struct in6_addr empty;

  memset (&empty, 0, sizeof (struct in6_addr));
  s = client->obuf;
  stream_reset (s);

  /* Place holder for size. */
  stream_putw (s, 0);

  /* Put command, type and nexthop. */
  stream_putc (s, ZEBRA_IPV6_ROUTE_ADD);
  stream_putc (s, rib->type);
  stream_putc (s, rib->flags);
  stream_putc (s, ZAPI_MESSAGE_NEXTHOP | ZAPI_MESSAGE_IFINDEX | ZAPI_MESSAGE_METRIC);

  /* Prefix. */
  psize = PSIZE (p->prefixlen);
  stream_putc (s, p->prefixlen);
  stream_write (s, (u_char *) &p->u.prefix, psize);

  /* Nexthop */
  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
    {
      if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
	{
	  stream_putc (s, 1);

	  if (nexthop->type == NEXTHOP_TYPE_IPV6)
	    stream_write (s, (u_char *) &nexthop->gate.ipv6, 16);
	  else
	    stream_write (s, (u_char *) &empty, 16);

	  /* Interface index. */
	  stream_putc (s, 1);
	  stream_putl (s, nexthop->ifindex);

	  break;
	}
    }

  /* Metric */
  stream_putl (s, rib->metric);

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

int
zsend_ipv6_delete (struct zserv *client, int type, int flags,
		   struct prefix_ipv6 *p, struct in6_addr *nexthop,
		   unsigned int ifindex)
{
  int psize;
  struct stream *s;

  s = client->obuf;
  stream_reset (s);

  /* Place holder for size. */
  stream_putw (s, 0);

  /* Put command, type and nexthop. */
  stream_putc (s, ZEBRA_IPV6_ROUTE_DELETE);
  stream_putc (s, type);
  stream_putc (s, flags);
  stream_putc (s, ZAPI_MESSAGE_NEXTHOP|ZAPI_MESSAGE_IFINDEX);

  /* Prefix. */
  psize = PSIZE (p->prefixlen);
  stream_putc (s, p->prefixlen);
  stream_write (s, (u_char *)&p->prefix, psize);

  /* Nexthop */
  stream_putc (s, 1);
  stream_write (s, (u_char *)nexthop, 16);

  /* Interface index. */
  stream_putc (s, 1);
  stream_putl (s, ifindex);

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

int
zsend_ipv6_delete_multipath (struct zserv *client, struct prefix *p,
			     struct rib *rib)
{
  int psize;
  struct stream *s;
  struct nexthop *nexthop;
  struct in6_addr empty;

  memset (&empty, 0, sizeof (struct in6_addr));
  s = client->obuf;
  stream_reset (s);

  /* Place holder for size. */
  stream_putw (s, 0);

  /* Put command, type and nexthop. */
  stream_putc (s, ZEBRA_IPV6_ROUTE_DELETE);
  stream_putc (s, rib->type);
  stream_putc (s, rib->flags);
  stream_putc (s, ZAPI_MESSAGE_NEXTHOP|ZAPI_MESSAGE_IFINDEX);

  /* Prefix. */
  psize = PSIZE (p->prefixlen);
  stream_putc (s, p->prefixlen);
  stream_write (s, (u_char *)&p->u.prefix, psize);

  /* Nexthop */
  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
    {
      if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
	{
	  stream_putc (s, 1);

	  if (nexthop->type == NEXTHOP_TYPE_IPV6)
	    stream_write (s, (u_char *) &nexthop->gate.ipv6, 16);
	  else
	    stream_write (s, (u_char *) &empty, 16);

	  /* Interface index. */
	  stream_putc (s, 1);
	  stream_putl (s, nexthop->ifindex);

	  break;
	}
    }

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

int
zsend_ipv6_nexthop_lookup (struct zserv *client, struct in6_addr *addr)
{
  struct stream *s;
  struct rib *rib;
  unsigned long nump;
  u_char num;
  struct nexthop *nexthop;

  /* Lookup nexthop. */
  rib = rib_match_ipv6 (addr);

  /* Get output stream. */
  s = client->obuf;
  stream_reset (s);

  /* Fill in result. */
  stream_putw (s, 0);
  stream_putc (s, ZEBRA_IPV6_NEXTHOP_LOOKUP);
  stream_put (s, &addr, 16);

  if (rib)
    {
      stream_putl (s, rib->metric);
      num = 0;
      nump = s->putp;
      stream_putc (s, 0);
      for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
	if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
	  {
	    stream_putc (s, nexthop->type);
	    switch (nexthop->type)
	      {
	      case ZEBRA_NEXTHOP_IPV6:
		stream_put (s, &nexthop->gate.ipv6, 16);
		break;
	      case ZEBRA_NEXTHOP_IPV6_IFINDEX:
	      case ZEBRA_NEXTHOP_IPV6_IFNAME:
		stream_put (s, &nexthop->gate.ipv6, 16);
		stream_putl (s, nexthop->ifindex);
		break;
	      case ZEBRA_NEXTHOP_IFINDEX:
	      case ZEBRA_NEXTHOP_IFNAME:
		stream_putl (s, nexthop->ifindex);
		break;
	      }
	    num++;
	  }
      stream_putc_at (s, nump, num);
    }
  else
    {
      stream_putl (s, 0);
      stream_putc (s, 0);
    }

  stream_putw_at (s, 0, stream_get_endp (s));
  
  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}
#endif /* HAVE_IPV6 */

int
zsend_ipv4_nexthop_lookup (struct zserv *client, struct in_addr addr)
{
  struct stream *s;
  struct rib *rib;
  unsigned long nump;
  u_char num;
  struct nexthop *nexthop;

  /* Lookup nexthop. */
  rib = rib_match_ipv4 (addr);

  /* Get output stream. */
  s = client->obuf;
  stream_reset (s);

  /* Fill in result. */
  stream_putw (s, 0);
  stream_putc (s, ZEBRA_IPV4_NEXTHOP_LOOKUP);
  stream_put_in_addr (s, &addr);

  if (rib)
    {
      stream_putl (s, rib->metric);
      num = 0;
      nump = s->putp;
      stream_putc (s, 0);
      for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
	if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
	  {
	    stream_putc (s, nexthop->type);
	    switch (nexthop->type)
	      {
	      case ZEBRA_NEXTHOP_IPV4:
		stream_put_in_addr (s, &nexthop->gate.ipv4);
		break;
	      case ZEBRA_NEXTHOP_IFINDEX:
	      case ZEBRA_NEXTHOP_IFNAME:
		stream_putl (s, nexthop->ifindex);
		break;
	      }
	    num++;
	  }
      stream_putc_at (s, nump, num);
    }
  else
    {
      stream_putl (s, 0);
      stream_putc (s, 0);
    }

  stream_putw_at (s, 0, stream_get_endp (s));
  
  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

int
zsend_ipv4_import_lookup (struct zserv *client, struct prefix_ipv4 *p)
{
  struct stream *s;
  struct rib *rib;
  unsigned long nump;
  u_char num;
  struct nexthop *nexthop;

  /* Lookup nexthop. */
  rib = rib_lookup_ipv4 (p);

  /* Get output stream. */
  s = client->obuf;
  stream_reset (s);

  /* Fill in result. */
  stream_putw (s, 0);
  stream_putc (s, ZEBRA_IPV4_IMPORT_LOOKUP);
  stream_put_in_addr (s, &p->prefix);

  if (rib)
    {
      stream_putl (s, rib->metric);
      num = 0;
      nump = s->putp;
      stream_putc (s, 0);
      for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
	if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
	  {
	    stream_putc (s, nexthop->type);
	    switch (nexthop->type)
	      {
	      case ZEBRA_NEXTHOP_IPV4:
		stream_put_in_addr (s, &nexthop->gate.ipv4);
		break;
	      case ZEBRA_NEXTHOP_IFINDEX:
	      case ZEBRA_NEXTHOP_IFNAME:
		stream_putl (s, nexthop->ifindex);
		break;
	      }
	    num++;
	  }
      stream_putc_at (s, nump, num);
    }
  else
    {
      stream_putl (s, 0);
      stream_putc (s, 0);
    }

  stream_putw_at (s, 0, stream_get_endp (s));
  
  zebra_server_send_message (client->sock, s->data, stream_get_endp (s));

  return 0;
}

/* Register zebra server interface information.  Send current all
   interface and address information. */
void
zread_interface_add (struct zserv *client, u_short length)
{
  listnode ifnode;
  listnode cnode;
  struct interface *ifp;
  struct connected *c;

  /* Interface information is needed. */
  client->ifinfo = 1;

  for (ifnode = listhead (iflist); ifnode; ifnode = nextnode (ifnode))
    {
      ifp = getdata (ifnode);

      /* Skip pseudo interface. */
      if (! CHECK_FLAG (ifp->status, ZEBRA_INTERFACE_ACTIVE))
	continue;

      zsend_interface_add (client, ifp);

      for (cnode = listhead (ifp->connected); cnode; nextnode (cnode))
	{
	  c = getdata (cnode);
	  if (CHECK_FLAG (c->conf, ZEBRA_IFC_REAL))
	    zsend_interface_address_add (client, ifp, c);
	}
    }
}

/* Unregister zebra server interface information. */
void
zread_interface_delete (struct zserv *client, u_short length)
{
  client->ifinfo = 0;
}

/* This function support multiple nexthop. */
void
zread_ipv4_add (struct zserv *client, u_short length)
{
  int i;
  struct rib *rib;
  struct prefix_ipv4 p;
  u_char message;
  struct in_addr nexthop;
  u_char nexthop_num;
  u_char nexthop_type;
  struct stream *s;
  unsigned int ifindex;
  u_char ifname_len;

  /* Get input stream.  */
  s = client->ibuf;

  /* Allocate new rib. */
  rib = XMALLOC (MTYPE_RIB, sizeof (struct rib));
  memset (rib, 0, sizeof (struct rib));

  /* Type, flags, message. */
  rib->type = stream_getc (s);
  rib->flags = stream_getc (s);
  message = stream_getc (s);
  rib->uptime = time (NULL);

  /* IPv4 prefix. */
  memset (&p, 0, sizeof (struct prefix_ipv4));
  p.family = AF_INET;
  p.prefixlen = stream_getc (s);
  stream_get (&p.prefix, s, PSIZE (p.prefixlen));

  /* Nexthop parse. */
  if (CHECK_FLAG (message, ZAPI_MESSAGE_NEXTHOP))
    {
      nexthop_num = stream_getc (s);

      for (i = 0; i < nexthop_num; i++)
	{
	  nexthop_type = stream_getc (s);

	  switch (nexthop_type)
	    {
	    case ZEBRA_NEXTHOP_IFINDEX:
	      ifindex = stream_getl (s);
	      nexthop_ifindex_add (rib, ifindex);
	      break;
	    case ZEBRA_NEXTHOP_IFNAME:
	      ifname_len = stream_getc (s);
	      stream_forward (s, ifname_len);
	      break;
	    case ZEBRA_NEXTHOP_IPV4:
	      nexthop.s_addr = stream_get_ipv4 (s);
	      nexthop_ipv4_add (rib, &nexthop);
	      break;
	    case ZEBRA_NEXTHOP_IPV6:
	      stream_forward (s, IPV6_MAX_BYTELEN);
	      break;
	    case ZEBRA_NEXTHOP_BLACKHOLE:
	      nexthop_blackhole_add (rib);
	      break;
	    }
	}
    }

  /* Distance. */
  if (CHECK_FLAG (message, ZAPI_MESSAGE_DISTANCE))
    rib->distance = stream_getc (s);

  /* Metric. */
  if (CHECK_FLAG (message, ZAPI_MESSAGE_METRIC))
    rib->metric = stream_getl (s);
    
  rib_add_ipv4_multipath (&p, rib);
}

/* Zebra server IPv4 prefix delete function. */
void
zread_ipv4_delete (struct zserv *client, u_short length)
{
  int i;
  struct stream *s;
  struct zapi_ipv4 api;
  struct in_addr nexthop;
  unsigned long ifindex;
  struct prefix_ipv4 p;
  u_char nexthop_num;
  u_char nexthop_type;
  u_char ifname_len;
  
  s = client->ibuf;
  ifindex = 0;
  nexthop.s_addr = 0;

  /* Type, flags, message. */
  api.type = stream_getc (s);
  api.flags = stream_getc (s);
  api.message = stream_getc (s);

  /* IPv4 prefix. */
  memset (&p, 0, sizeof (struct prefix_ipv4));
  p.family = AF_INET;
  p.prefixlen = stream_getc (s);
  stream_get (&p.prefix, s, PSIZE (p.prefixlen));

  /* Nexthop, ifindex, distance, metric. */
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_NEXTHOP))
    {
      nexthop_num = stream_getc (s);

      for (i = 0; i < nexthop_num; i++)
	{
	  nexthop_type = stream_getc (s);

	  switch (nexthop_type)
	    {
	    case ZEBRA_NEXTHOP_IFINDEX:
	      ifindex = stream_getl (s);
	      break;
	    case ZEBRA_NEXTHOP_IFNAME:
	      ifname_len = stream_getc (s);
	      stream_forward (s, ifname_len);
	      break;
	    case ZEBRA_NEXTHOP_IPV4:
	      nexthop.s_addr = stream_get_ipv4 (s);
	      break;
	    case ZEBRA_NEXTHOP_IPV6:
	      stream_forward (s, IPV6_MAX_BYTELEN);
	      break;
	    }
	}
    }

  /* Distance. */
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_DISTANCE))
    api.distance = stream_getc (s);
  else
    api.distance = 0;

  /* Metric. */
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_METRIC))
    api.metric = stream_getl (s);
  else
    api.metric = 0;
    
  rib_delete_ipv4 (api.type, api.flags, &p, &nexthop, ifindex,
		   client->rtm_table);
}

/* Nexthop lookup for IPv4. */
void
zread_ipv4_nexthop_lookup (struct zserv *client, u_short length)
{
  struct in_addr addr;

  addr.s_addr = stream_get_ipv4 (client->ibuf);
  zsend_ipv4_nexthop_lookup (client, addr);
}

/* Nexthop lookup for IPv4. */
void
zread_ipv4_import_lookup (struct zserv *client, u_short length)
{
  struct prefix_ipv4 p;

  p.family = AF_INET;
  p.prefixlen = stream_getc (client->ibuf);
  p.prefix.s_addr = stream_get_ipv4 (client->ibuf);

  zsend_ipv4_import_lookup (client, &p);
}

#ifdef HAVE_IPV6
/* Zebra server IPv6 prefix add function. */
void
zread_ipv6_add (struct zserv *client, u_short length)
{
  int i;
  struct stream *s;
  struct zapi_ipv6 api;
  struct in6_addr nexthop;
  unsigned long ifindex;
  struct prefix_ipv6 p;
  
  s = client->ibuf;
  ifindex = 0;
  memset (&nexthop, 0, sizeof (struct in6_addr));

  /* Type, flags, message. */
  api.type = stream_getc (s);
  api.flags = stream_getc (s);
  api.message = stream_getc (s);

  /* IPv4 prefix. */
  memset (&p, 0, sizeof (struct prefix_ipv6));
  p.family = AF_INET6;
  p.prefixlen = stream_getc (s);
  stream_get (&p.prefix, s, PSIZE (p.prefixlen));

  /* Nexthop, ifindex, distance, metric. */
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_NEXTHOP))
    {
      u_char nexthop_type;

      api.nexthop_num = stream_getc (s);
      for (i = 0; i < api.nexthop_num; i++)
	{
	  nexthop_type = stream_getc (s);

	  switch (nexthop_type)
	    {
	    case ZEBRA_NEXTHOP_IPV6:
	      stream_get (&nexthop, s, 16);
	      break;
	    case ZEBRA_NEXTHOP_IFINDEX:
	      ifindex = stream_getl (s);
	      break;
	    }
	}
    }

  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_DISTANCE))
    api.distance = stream_getc (s);
  else
    api.distance = 0;

  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_METRIC))
    api.metric = stream_getl (s);
  else
    api.metric = 0;
    
  if (IN6_IS_ADDR_UNSPECIFIED (&nexthop))
    rib_add_ipv6 (api.type, api.flags, &p, NULL, ifindex, 0);
  else
    rib_add_ipv6 (api.type, api.flags, &p, &nexthop, ifindex, 0);
}

/* Zebra server IPv6 prefix delete function. */
void
zread_ipv6_delete (struct zserv *client, u_short length)
{
  int i;
  struct stream *s;
  struct zapi_ipv6 api;
  struct in6_addr nexthop;
  unsigned long ifindex;
  struct prefix_ipv6 p;
  
  s = client->ibuf;
  ifindex = 0;
  memset (&nexthop, 0, sizeof (struct in6_addr));

  /* Type, flags, message. */
  api.type = stream_getc (s);
  api.flags = stream_getc (s);
  api.message = stream_getc (s);

  /* IPv4 prefix. */
  memset (&p, 0, sizeof (struct prefix_ipv6));
  p.family = AF_INET6;
  p.prefixlen = stream_getc (s);
  stream_get (&p.prefix, s, PSIZE (p.prefixlen));

  /* Nexthop, ifindex, distance, metric. */
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_NEXTHOP))
    {
      u_char nexthop_type;

      api.nexthop_num = stream_getc (s);
      for (i = 0; i < api.nexthop_num; i++)
	{
	  nexthop_type = stream_getc (s);

	  switch (nexthop_type)
	    {
	    case ZEBRA_NEXTHOP_IPV6:
	      stream_get (&nexthop, s, 16);
	      break;
	    case ZEBRA_NEXTHOP_IFINDEX:
	      ifindex = stream_getl (s);
	      break;
	    }
	}
    }

  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_DISTANCE))
    api.distance = stream_getc (s);
  else
    api.distance = 0;
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_METRIC))
    api.metric = stream_getl (s);
  else
    api.metric = 0;
    
  if (IN6_IS_ADDR_UNSPECIFIED (&nexthop))
    rib_delete_ipv6 (api.type, api.flags, &p, NULL, ifindex, 0);
  else
    rib_delete_ipv6 (api.type, api.flags, &p, &nexthop, ifindex, 0);
}

void
zebra_read_ipv6 (int command, struct zserv *client, u_short length)
{
  u_char type;
  u_char flags;
  struct in6_addr nexthop, *gate;
  u_char *lim;
  u_char *pnt;
  unsigned int ifindex;

  pnt = stream_pnt (client->ibuf);
  lim = pnt + length;

  type = stream_getc (client->ibuf);
  flags = stream_getc (client->ibuf);
  stream_get (&nexthop, client->ibuf, sizeof (struct in6_addr));
  
  while (stream_pnt (client->ibuf) < lim)
    {
      int size;
      struct prefix_ipv6 p;
      
      ifindex = stream_getl (client->ibuf);

      memset (&p, 0, sizeof (struct prefix_ipv6));
      p.family = AF_INET6;
      p.prefixlen = stream_getc (client->ibuf);
      size = PSIZE(p.prefixlen);
      stream_get (&p.prefix, client->ibuf, size);

      if (IN6_IS_ADDR_UNSPECIFIED (&nexthop))
        gate = NULL;
      else
        gate = &nexthop;

      if (command == ZEBRA_IPV6_ROUTE_ADD)
	rib_add_ipv6 (type, flags, &p, gate, ifindex, 0);
      else
	rib_delete_ipv6 (type, flags, &p, gate, ifindex, 0);
    }
}

void
zread_ipv6_nexthop_lookup (struct zserv *client, u_short length)
{
  struct in6_addr addr;
  char buf[BUFSIZ];

  stream_get (&addr, client->ibuf, 16);
  printf ("DEBUG %s\n", inet_ntop (AF_INET6, &addr, buf, BUFSIZ));

  zsend_ipv6_nexthop_lookup (client, &addr);
}
#endif /* HAVE_IPV6 */

/* Close zebra client. */
void
zebra_client_close (struct zserv *client)
{
  /* Close file descriptor. */
  if (client->sock)
    {
      close (client->sock);
      client->sock = -1;
    }

  /* Free stream buffers. */
  if (client->ibuf)
    stream_free (client->ibuf);
  if (client->obuf)
    stream_free (client->obuf);

  /* Release threads. */
  if (client->t_read)
    thread_cancel (client->t_read);
  if (client->t_write)
    thread_cancel (client->t_write);

  /* Free client structure. */
  listnode_delete (client_list, client);
  XFREE (0, client);
}

/* Make new client. */
void
zebra_client_create (int sock)
{
  struct zserv *client;

  client = XCALLOC (0, sizeof (struct zserv));

  /* Make client input/output buffer. */
  client->sock = sock;
  client->ibuf = stream_new (ZEBRA_MAX_PACKET_SIZ);
  client->obuf = stream_new (ZEBRA_MAX_PACKET_SIZ);

  /* Set table number. */
  client->rtm_table = rtm_table_default;

  /* Add this client to linked list. */
  listnode_add (client_list, client);
  
  /* Make new read thread. */
  zebra_event (ZEBRA_READ, sock, client);
}

/* Handler of zebra service request. */
int
zebra_client_read (struct thread *thread)
{
  int sock;
  struct zserv *client;
  int nbyte;
  u_short length;
  u_char command;

  /* Get thread data.  Reset reading thread because I'm running. */
  sock = THREAD_FD (thread);
  client = THREAD_ARG (thread);
  client->t_read = NULL;

  /* Read length and command. */
  nbyte = stream_read (client->ibuf, sock, 3);
  if (nbyte <= 0) 
    {
      if (IS_ZEBRA_DEBUG_EVENT)
	zlog_info ("connection closed socket [%d]", sock);
      zebra_client_close (client);
      return -1;
    }
  length = stream_getw (client->ibuf);
  command = stream_getc (client->ibuf);

  if (length < 3) 
    {
      if (IS_ZEBRA_DEBUG_EVENT)
	zlog_info ("length %d is less than 3 ", length);
      zebra_client_close (client);
      return -1;
    }

  length -= 3;

  /* Read rest of data. */
  if (length)
    {
      nbyte = stream_read (client->ibuf, sock, length);
      if (nbyte <= 0) 
	{
	  if (IS_ZEBRA_DEBUG_EVENT)
	    zlog_info ("connection closed [%d] when reading zebra data", sock);
	  zebra_client_close (client);
	  return -1;
	}
    }

  /* Debug packet information. */
  if (IS_ZEBRA_DEBUG_EVENT)
    zlog_info ("zebra message comes from socket [%d]", sock);

  if (IS_ZEBRA_DEBUG_PACKET && IS_ZEBRA_DEBUG_RECV)
    zlog_info ("zebra message received [%s] %d", 
	       zebra_command_str[command], length);

  switch (command) 
    {
    case ZEBRA_INTERFACE_ADD:
      zread_interface_add (client, length);
      break;
    case ZEBRA_INTERFACE_DELETE:
      zread_interface_delete (client, length);
      break;
    case ZEBRA_IPV4_ROUTE_ADD:
      zread_ipv4_add (client, length);
      break;
    case ZEBRA_IPV4_ROUTE_DELETE:
      zread_ipv4_delete (client, length);
      break;
#ifdef HAVE_IPV6
    case ZEBRA_IPV6_ROUTE_ADD:
      zread_ipv6_add (client, length);
      break;
    case ZEBRA_IPV6_ROUTE_DELETE:
      zread_ipv6_delete (client, length);
      break;
#endif /* HAVE_IPV6 */
    case ZEBRA_REDISTRIBUTE_ADD:
      zebra_redistribute_add (command, client, length);
      break;
    case ZEBRA_REDISTRIBUTE_DELETE:
      zebra_redistribute_delete (command, client, length);
      break;
    case ZEBRA_REDISTRIBUTE_DEFAULT_ADD:
      zebra_redistribute_default_add (command, client, length);
      break;
    case ZEBRA_REDISTRIBUTE_DEFAULT_DELETE:
      zebra_redistribute_default_delete (command, client, length);
      break;
    case ZEBRA_IPV4_NEXTHOP_LOOKUP:
      zread_ipv4_nexthop_lookup (client, length);
      break;
#ifdef HAVE_IPV6
    case ZEBRA_IPV6_NEXTHOP_LOOKUP:
      zread_ipv6_nexthop_lookup (client, length);
      break;
#endif /* HAVE_IPV6 */
    case ZEBRA_IPV4_IMPORT_LOOKUP:
      zread_ipv4_import_lookup (client, length);
      break;
    default:
      zlog_info ("Zebra received unknown command %d", command);
      break;
    }

  stream_reset (client->ibuf);
  zebra_event (ZEBRA_READ, sock, client);

  return 0;
}

/* Write output buffer to the socket. */
void
zebra_write (struct thread *thread)
{
  int sock;
  struct zserv *client;

  /* Thread treatment. */
  sock = THREAD_FD (thread);
  client = THREAD_ARG (thread);
  client->t_write = NULL;

  stream_flush (client->obuf, sock);
}

/* Accept code of zebra server socket. */
int
zebra_accept (struct thread *thread)
{
  int val;
  int accept_sock;
  int client_sock;
  struct sockaddr_in client;
  socklen_t len;

  accept_sock = THREAD_FD (thread);

  len = sizeof (struct sockaddr_in);
  client_sock = accept (accept_sock, (struct sockaddr *) &client, &len);

  if (client_sock < 0)
    {
      zlog_warn ("Can't accept zebra socket: %s", strerror (errno));
      return -1;
    }

  /* Make client socket non-blocking.  */

  val = fcntl (client_sock, F_GETFL, 0);
  fcntl (client_sock, F_SETFL, (val | O_NONBLOCK));

  /* Create new zebra client. */
  zebra_client_create (client_sock);

  /* Register myself. */
  zebra_event (ZEBRA_SERV, accept_sock, NULL);

  return 0;
}

/* Make zebra's server socket. */
void
zebra_serv ()
{
  int ret;
  int accept_sock;
  struct sockaddr_in addr;

  accept_sock = socket (AF_INET, SOCK_STREAM, 0);

  if (accept_sock < 0) 
    {
      zlog_warn ("Can't bind to socket: %s", strerror (errno));
      zlog_warn ("zebra can't provice full functionality due to above error");
      return;
    }

  memset (&addr, 0, sizeof (struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons (ZEBRA_PORT);
#ifdef HAVE_SIN_LEN
  addr.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */
  addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

  sockopt_reuseaddr (accept_sock);
  sockopt_reuseport (accept_sock);

  ret  = bind (accept_sock, (struct sockaddr *)&addr, 
	       sizeof (struct sockaddr_in));
  if (ret < 0)
    {
      zlog_warn ("Can't bind to socket: %s", strerror (errno));
      zlog_warn ("zebra can't provice full functionality due to above error");
      close (accept_sock);      /* Avoid sd leak. */
      return;
    }

  ret = listen (accept_sock, 1);
  if (ret < 0)
    {
      zlog_warn ("Can't listen to socket: %s", strerror (errno));
      zlog_warn ("zebra can't provice full functionality due to above error");
      close (accept_sock);	/* Avoid sd leak. */
      return;
    }

  zebra_event (ZEBRA_SERV, accept_sock, NULL);
}

/* For sockaddr_un. */
#include <sys/un.h>

/* zebra server UNIX domain socket. */
void
zebra_serv_un (char *path)
{
  int ret;
  int sock, len;
  struct sockaddr_un serv;
  mode_t old_mask;

  /* First of all, unlink existing socket */
  unlink (path);

  /* Set umask */
  old_mask = umask (0077);

  /* Make UNIX domain socket. */
  sock = socket (AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0)
    {
      perror ("sock");
      return;
    }

  /* Make server socket. */
  memset (&serv, 0, sizeof (struct sockaddr_un));
  serv.sun_family = AF_UNIX;
  strncpy (serv.sun_path, path, strlen (path));
#ifdef HAVE_SUN_LEN
  len = serv.sun_len = SUN_LEN(&serv);
#else
  len = sizeof (serv.sun_family) + strlen (serv.sun_path);
#endif /* HAVE_SUN_LEN */

  ret = bind (sock, (struct sockaddr *) &serv, len);
  if (ret < 0)
    {
      perror ("bind");
      close (sock);
      return;
    }

  ret = listen (sock, 5);
  if (ret < 0)
    {
      perror ("listen");
      close (sock);
      return;
    }

  umask (old_mask);

  zebra_event (ZEBRA_SERV, sock, NULL);
}

/* Zebra's event management function. */
extern struct thread_master *master;

void
zebra_event (enum event event, int sock, struct zserv *client)
{
  switch (event)
    {
    case ZEBRA_SERV:
      thread_add_read (master, zebra_accept, client, sock);
      break;
    case ZEBRA_READ:
      client->t_read = 
	thread_add_read (master, zebra_client_read, client, sock);
      break;
    case ZEBRA_WRITE:
      /**/
      break;
    }
}

/* Display default rtm_table for all clients. */
DEFUN (show_table,
       show_table_cmd,
       "show table",
       SHOW_STR
       "default routing table to use for all clients\n")
{
  vty_out (vty, "table %d%s", rtm_table_default,
	   VTY_NEWLINE);
  return CMD_SUCCESS;
}

DEFUN (config_table, 
       config_table_cmd,
       "table TABLENO",
       "Configure target kernel routing table\n"
       "TABLE integer\n")
{
  rtm_table_default = strtol (argv[0], (char**)0, 10);
  return CMD_SUCCESS;
}

DEFUN (no_ip_forwarding,
       no_ip_forwarding_cmd,
       "no ip forwarding",
       NO_STR
       IP_STR
       "Turn off IP forwarding")
{
  int ret;

  ret = ipforward ();

  if (ret == 0)
    {
      vty_out (vty, "IP forwarding is already off%s", VTY_NEWLINE); 
      return CMD_ERR_NOTHING_TODO;
    }

  ret = ipforward_off ();
  if (ret != 0)
    {
      vty_out (vty, "Can't turn off IP forwarding%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

/* This command is for debugging purpose. */
DEFUN (show_zebra_client,
       show_zebra_client_cmd,
       "show zebra client",
       SHOW_STR
       "Zebra information"
       "Client information")
{
  listnode node;
  struct zserv *client;

  for (node = listhead (client_list); node; nextnode (node))
    {
      client = getdata (node);
      vty_out (vty, "Client fd %d%s", client->sock, VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

/* Table configuration write function. */
int
config_write_table (struct vty *vty)
{
  if (rtm_table_default)
    vty_out (vty, "table %d%s", rtm_table_default,
	     VTY_NEWLINE);
  return 0;
}

/* table node for routing tables. */
struct cmd_node table_node =
{
  TABLE_NODE,
  "",				/* This node has no interface. */
  1
};

/* Only display ip forwarding is enabled or not. */
DEFUN (show_ip_forwarding,
       show_ip_forwarding_cmd,
       "show ip forwarding",
       SHOW_STR
       IP_STR
       "IP forwarding status\n")
{
  int ret;

  ret = ipforward ();

  if (ret == 0)
    vty_out (vty, "IP forwarding is off%s", VTY_NEWLINE);
  else
    vty_out (vty, "IP forwarding is on%s", VTY_NEWLINE);
  return CMD_SUCCESS;
}

#ifdef HAVE_IPV6
/* Only display ipv6 forwarding is enabled or not. */
DEFUN (show_ipv6_forwarding,
       show_ipv6_forwarding_cmd,
       "show ipv6 forwarding",
       SHOW_STR
       "IPv6 information\n"
       "Forwarding status\n")
{
  int ret;

  ret = ipforward_ipv6 ();

  switch (ret)
    {
    case -1:
      vty_out (vty, "ipv6 forwarding is unknown%s", VTY_NEWLINE);
      break;
    case 0:
      vty_out (vty, "ipv6 forwarding is %s%s", "off", VTY_NEWLINE);
      break;
    case 1:
      vty_out (vty, "ipv6 forwarding is %s%s", "on", VTY_NEWLINE);
      break;
    default:
      vty_out (vty, "ipv6 forwarding is %s%s", "off", VTY_NEWLINE);
      break;
    }
  return CMD_SUCCESS;
}

DEFUN (no_ipv6_forwarding,
       no_ipv6_forwarding_cmd,
       "no ipv6 forwarding",
       NO_STR
       IP_STR
       "Doesn't forward IPv6 protocol packet")
{
  int ret;

  ret = ipforward_ipv6_off ();
  if (ret != 0)
    {
      vty_out (vty, "Can't turn off IPv6 forwarding%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

#endif /* HAVE_IPV6 */

/* IPForwarding configuration write function. */
int
config_write_forwarding (struct vty *vty)
{
  if (! ipforward ())
    vty_out (vty, "no ip forwarding%s", VTY_NEWLINE);
#ifdef HAVE_IPV6
  if (! ipforward_ipv6 ())
    vty_out (vty, "no ipv6 forwarding%s", VTY_NEWLINE);
#endif /* HAVE_IPV6 */
  vty_out (vty, "!%s", VTY_NEWLINE);
  return 0;
}

/* table node for routing tables. */
struct cmd_node forwarding_node =
{
  FORWARDING_NODE,
  "",				/* This node has no interface. */
  1
};


/* Initialisation of zebra and installation of commands. */
void
zebra_init ()
{
  /* Client list init. */
  client_list = list_new ();

  /* Forwarding is on by default. */
  ipforward_on ();
#ifdef HAVE_IPV6
  ipforward_ipv6_on ();
#endif /* HAVE_IPV6 */

  /* Make zebra server socket. */
#ifdef HAVE_TCP_ZEBRA
  zebra_serv ();
#else
  zebra_serv_un (ZEBRA_SERV_PATH);
#endif /* HAVE_TCP_ZEBRA */

  /* Install configuration write function. */
  install_node (&table_node, config_write_table);
  install_node (&forwarding_node, config_write_forwarding);

  install_element (VIEW_NODE, &show_ip_forwarding_cmd);
  install_element (ENABLE_NODE, &show_ip_forwarding_cmd);
  install_element (CONFIG_NODE, &no_ip_forwarding_cmd);
  install_element (ENABLE_NODE, &show_zebra_client_cmd);

#ifdef HAVE_NETLINK
  install_element (VIEW_NODE, &show_table_cmd);
  install_element (ENABLE_NODE, &show_table_cmd);
  install_element (CONFIG_NODE, &config_table_cmd);
#endif /* HAVE_NETLINK */

#ifdef HAVE_IPV6
  install_element (VIEW_NODE, &show_ipv6_forwarding_cmd);
  install_element (ENABLE_NODE, &show_ipv6_forwarding_cmd);
  install_element (CONFIG_NODE, &no_ipv6_forwarding_cmd);
#endif /* HAVE_IPV6 */

  FIFO_INIT(&message_queue);
  t_write = NULL;
}
