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
#include "privs.h"
#include "network.h"
#include "buffer.h"

#include "zebra/zserv.h"
#include "zebra/router-id.h"
#include "zebra/redistribute.h"
#include "zebra/debug.h"
#include "zebra/ipforward.h"

/* Event list of zebra. */
enum event { ZEBRA_SERV, ZEBRA_READ, ZEBRA_WRITE };

extern struct zebra_t zebrad;

static void zebra_event (enum event event, int sock, struct zserv *client);

extern struct zebra_privs_t zserv_privs;

static void zebra_client_close (struct zserv *client);

static int
zserv_delayed_close(struct thread *thread)
{
  struct zserv *client = THREAD_ARG(thread);

  client->t_suicide = NULL;
  zebra_client_close(client);
  return 0;
}

/* When client connects, it sends hello message
 * with promise to send zebra routes of specific type.
 * Zebra stores a socket fd of the client into
 * this array. And use it to clean up routes that
 * client didn't remove for some reasons after closing
 * connection.
 */
static int route_type_oaths[ZEBRA_ROUTE_MAX];

static int
zserv_flush_data(struct thread *thread)
{
  struct zserv *client = THREAD_ARG(thread);

  client->t_write = NULL;
  if (client->t_suicide)
    {
      zebra_client_close(client);
      return -1;
    }
  switch (buffer_flush_available(client->wb, client->sock))
    {
    case BUFFER_ERROR:
      zlog_warn("%s: buffer_flush_available failed on zserv client fd %d, "
      		"closing", __func__, client->sock);
      zebra_client_close(client);
      break;
    case BUFFER_PENDING:
      client->t_write = thread_add_write(zebrad.master, zserv_flush_data,
      					 client, client->sock);
      break;
    case BUFFER_EMPTY:
      break;
    }
  return 0;
}

static int
zebra_server_send_message(struct zserv *client)
{
  if (client->t_suicide)
    return -1;
  switch (buffer_write(client->wb, client->sock, STREAM_DATA(client->obuf),
		       stream_get_endp(client->obuf)))
    {
    case BUFFER_ERROR:
      zlog_warn("%s: buffer_write failed to zserv client fd %d, closing",
      		 __func__, client->sock);
      /* Schedule a delayed close since many of the functions that call this
         one do not check the return code.  They do not allow for the
	 possibility that an I/O error may have caused the client to be
	 deleted. */
      client->t_suicide = thread_add_event(zebrad.master, zserv_delayed_close,
					   client, 0);
      return -1;
    case BUFFER_EMPTY:
      THREAD_OFF(client->t_write);
      break;
    case BUFFER_PENDING:
      THREAD_WRITE_ON(zebrad.master, client->t_write,
		      zserv_flush_data, client, client->sock);
      break;
    }
  return 0;
}

static void
zserv_create_header (struct stream *s, uint16_t cmd)
{
  /* length placeholder, caller can update */
  stream_putw (s, ZEBRA_HEADER_SIZE);
  stream_putc (s, ZEBRA_HEADER_MARKER);
  stream_putc (s, ZSERV_VERSION);
  stream_putw (s, cmd);
}

static void
zserv_encode_interface (struct stream *s, struct interface *ifp)
{
  /* Interface information. */
  stream_put (s, ifp->name, INTERFACE_NAMSIZ);
  stream_putl (s, ifp->ifindex);
  stream_putc (s, ifp->status);
  stream_putq (s, ifp->flags);
  stream_putl (s, ifp->metric);
  stream_putl (s, ifp->mtu);
  stream_putl (s, ifp->mtu6);
  stream_putl (s, ifp->bandwidth);
#ifdef HAVE_STRUCT_SOCKADDR_DL
  stream_put (s, &ifp->sdl, sizeof (ifp->sdl_storage));
#else
  stream_putl (s, ifp->hw_addr_len);
  if (ifp->hw_addr_len)
    stream_put (s, ifp->hw_addr, ifp->hw_addr_len);
#endif /* HAVE_STRUCT_SOCKADDR_DL */

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));
}

/* Interface is added. Send ZEBRA_INTERFACE_ADD to client. */
/*
 * This function is called in the following situations:
 * - in response to a 3-byte ZEBRA_INTERFACE_ADD request
 *   from the client.
 * - at startup, when zebra figures out the available interfaces
 * - when an interface is added (where support for
 *   RTM_IFANNOUNCE or AF_NETLINK sockets is available), or when
 *   an interface is marked IFF_UP (i.e., an RTM_IFINFO message is
 *   received)
 */
int
zsend_interface_add (struct zserv *client, struct interface *ifp)
{
  struct stream *s;

  /* Check this client need interface information. */
  if (! client->ifinfo)
    return 0;

  s = client->obuf;
  stream_reset (s);

  zserv_create_header (s, ZEBRA_INTERFACE_ADD);
  zserv_encode_interface (s, ifp);

  return zebra_server_send_message(client);
}

/* Interface deletion from zebra daemon. */
int
zsend_interface_delete (struct zserv *client, struct interface *ifp)
{
  struct stream *s;

  /* Check this client need interface information. */
  if (! client->ifinfo)
    return 0;

  s = client->obuf;
  stream_reset (s);

  zserv_create_header (s, ZEBRA_INTERFACE_DELETE);
  zserv_encode_interface (s, ifp);

  return zebra_server_send_message (client);
}

/* Interface address is added/deleted. Send ZEBRA_INTERFACE_ADDRESS_ADD or
 * ZEBRA_INTERFACE_ADDRESS_DELETE to the client. 
 *
 * A ZEBRA_INTERFACE_ADDRESS_ADD is sent in the following situations:
 * - in response to a 3-byte ZEBRA_INTERFACE_ADD request
 *   from the client, after the ZEBRA_INTERFACE_ADD has been
 *   sent from zebra to the client
 * - redistribute new address info to all clients in the following situations
 *    - at startup, when zebra figures out the available interfaces
 *    - when an interface is added (where support for
 *      RTM_IFANNOUNCE or AF_NETLINK sockets is available), or when
 *      an interface is marked IFF_UP (i.e., an RTM_IFINFO message is
 *      received)
 *    - for the vty commands "ip address A.B.C.D/M [<secondary>|<label LINE>]"
 *      and "no bandwidth <1-10000000>", "ipv6 address X:X::X:X/M"
 *    - when an RTM_NEWADDR message is received from the kernel,
 * 
 * The call tree that triggers ZEBRA_INTERFACE_ADDRESS_DELETE: 
 *
 *                   zsend_interface_address(DELETE)
 *                           ^                         
 *                           |                        
 *          zebra_interface_address_delete_update    
 *             ^                        ^      ^
 *             |                        |      if_delete_update
 *             |                        |
 *         ip_address_uninstall        connected_delete_ipv4
 *         [ipv6_addresss_uninstall]   [connected_delete_ipv6]
 *             ^                        ^
 *             |                        |
 *             |                  RTM_NEWADDR on routing/netlink socket
 *             |
 *         vty commands:
 *     "no ip address A.B.C.D/M [label LINE]"
 *     "no ip address A.B.C.D/M secondary"
 *     ["no ipv6 address X:X::X:X/M"]
 *
 */
int
zsend_interface_address (int cmd, struct zserv *client, 
                         struct interface *ifp, struct connected *ifc)
{
  int blen;
  struct stream *s;
  struct prefix *p;

  /* Check this client need interface information. */
  if (! client->ifinfo)
    return 0;

  s = client->obuf;
  stream_reset (s);
  
  zserv_create_header (s, cmd);
  stream_putl (s, ifp->ifindex);

  /* Interface address flag. */
  stream_putc (s, ifc->flags);

  /* Prefix information. */
  p = ifc->address;
  stream_putc (s, p->family);
  blen = prefix_blen (p);
  stream_put (s, &p->u.prefix, blen);

  /* 
   * XXX gnu version does not send prefixlen for ZEBRA_INTERFACE_ADDRESS_DELETE
   * but zebra_interface_address_delete_read() in the gnu version 
   * expects to find it
   */
  stream_putc (s, p->prefixlen);

  /* Destination. */
  p = ifc->destination;
  if (p)
    stream_put (s, &p->u.prefix, blen);
  else
    stream_put (s, NULL, blen);

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  return zebra_server_send_message(client);
}

/*
 * The cmd passed to zsend_interface_update  may be ZEBRA_INTERFACE_UP or
 * ZEBRA_INTERFACE_DOWN.
 *
 * The ZEBRA_INTERFACE_UP message is sent from the zebra server to
 * the clients in one of 2 situations:
 *   - an if_up is detected e.g., as a result of an RTM_IFINFO message
 *   - a vty command modifying the bandwidth of an interface is received.
 * The ZEBRA_INTERFACE_DOWN message is sent when an if_down is detected.
 */
int
zsend_interface_update (int cmd, struct zserv *client, struct interface *ifp)
{
  struct stream *s;

  /* Check this client need interface information. */
  if (! client->ifinfo)
    return 0;

  s = client->obuf;
  stream_reset (s);

  zserv_create_header (s, cmd);
  zserv_encode_interface (s, ifp);

  return zebra_server_send_message(client);
}

/*
 * The zebra server sends the clients  a ZEBRA_IPV4_ROUTE_ADD or a
 * ZEBRA_IPV6_ROUTE_ADD via zsend_route_multipath in the following
 * situations:
 * - when the client starts up, and requests default information
 *   by sending a ZEBRA_REDISTRIBUTE_DEFAULT_ADD to the zebra server, in the
 * - case of rip, ripngd, ospfd and ospf6d, when the client sends a
 *   ZEBRA_REDISTRIBUTE_ADD as a result of the "redistribute" vty cmd,
 * - when the zebra server redistributes routes after it updates its rib
 *
 * The zebra server sends clients a ZEBRA_IPV4_ROUTE_DELETE or a
 * ZEBRA_IPV6_ROUTE_DELETE via zsend_route_multipath when:
 * - a "ip route"  or "ipv6 route" vty command is issued, a prefix is
 * - deleted from zebra's rib, and this info
 *   has to be redistributed to the clients 
 * 
 * XXX The ZEBRA_IPV*_ROUTE_ADD message is also sent by the client to the
 * zebra server when the client wants to tell the zebra server to add a
 * route to the kernel (zapi_ipv4_add etc. ).  Since it's essentially the
 * same message being sent back and forth, this function and
 * zapi_ipv{4,6}_{add, delete} should be re-written to avoid code
 * duplication.
 */
int
zsend_route_multipath (int cmd, struct zserv *client, struct prefix *p,
                       struct rib *rib)
{
  int psize;
  struct stream *s;
  struct nexthop *nexthop;
  unsigned long nhnummark = 0, messmark = 0;
  int nhnum = 0;
  u_char zapi_flags = 0;
  
  s = client->obuf;
  stream_reset (s);
  
  zserv_create_header (s, cmd);
  
  /* Put type and nexthop. */
  stream_putc (s, rib->type);
  stream_putc (s, rib->flags);
  
  /* marker for message flags field */
  messmark = stream_get_endp (s);
  stream_putc (s, 0);

  /* Prefix. */
  psize = PSIZE (p->prefixlen);
  stream_putc (s, p->prefixlen);
  stream_write (s, (u_char *) & p->u.prefix, psize);

  /* 
   * XXX The message format sent by zebra below does not match the format
   * of the corresponding message expected by the zebra server
   * itself (e.g., see zread_ipv4_add). The nexthop_num is not set correctly,
   * (is there a bug on the client side if more than one segment is sent?)
   * nexthop ZEBRA_NEXTHOP_IPV4 is never set, ZEBRA_NEXTHOP_IFINDEX 
   * is hard-coded.
   */
  /* Nexthop */
  
  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
    {
      if (CHECK_FLAG(nexthop->flags, NEXTHOP_FLAG_FIB)
          || nexthop_has_fib_child(nexthop))
        {
          SET_FLAG (zapi_flags, ZAPI_MESSAGE_NEXTHOP);
          SET_FLAG (zapi_flags, ZAPI_MESSAGE_IFINDEX);
          
          if (nhnummark == 0)
            {
              nhnummark = stream_get_endp (s);
              stream_putc (s, 1); /* placeholder */
            }
          
          nhnum++;

          switch(nexthop->type) 
            {
              case NEXTHOP_TYPE_IPV4:
              case NEXTHOP_TYPE_IPV4_IFINDEX:
                stream_put_in_addr (s, &nexthop->gate.ipv4);
                break;
#ifdef HAVE_IPV6
              case NEXTHOP_TYPE_IPV6:
              case NEXTHOP_TYPE_IPV6_IFINDEX:
              case NEXTHOP_TYPE_IPV6_IFNAME:
                stream_write (s, (u_char *) &nexthop->gate.ipv6, 16);
                break;
#endif
              default:
                if (cmd == ZEBRA_IPV4_ROUTE_ADD 
                    || cmd == ZEBRA_IPV4_ROUTE_DELETE)
                  {
                    struct in_addr empty;
                    memset (&empty, 0, sizeof (struct in_addr));
                    stream_write (s, (u_char *) &empty, IPV4_MAX_BYTELEN);
                  }
                else
                  {
                    struct in6_addr empty;
                    memset (&empty, 0, sizeof (struct in6_addr));
                    stream_write (s, (u_char *) &empty, IPV6_MAX_BYTELEN);
                  }
              }

          /* Interface index. */
          stream_putc (s, 1);
          stream_putl (s, nexthop->ifindex);

          break;
        }
    }

  /* Metric */
  if (cmd == ZEBRA_IPV4_ROUTE_ADD || cmd == ZEBRA_IPV6_ROUTE_ADD)
    {
      SET_FLAG (zapi_flags, ZAPI_MESSAGE_DISTANCE);
      stream_putc (s, rib->distance);
      SET_FLAG (zapi_flags, ZAPI_MESSAGE_METRIC);
      stream_putl (s, rib->metric);
    }
  
  /* write real message flags value */
  stream_putc_at (s, messmark, zapi_flags);
  
  /* Write next-hop number */
  if (nhnummark)
    stream_putc_at (s, nhnummark, nhnum);
  
  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  return zebra_server_send_message(client);
}

#ifdef HAVE_IPV6
static int
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
  zserv_create_header (s, ZEBRA_IPV6_NEXTHOP_LOOKUP);
  stream_put (s, &addr, 16);

  if (rib)
    {
      stream_putl (s, rib->metric);
      num = 0;
      nump = stream_get_endp(s);
      stream_putc (s, 0);
      /* Only non-recursive routes are elegible to resolve nexthop we
       * are looking up. Therefore, we will just iterate over the top
       * chain of nexthops. */
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
	      default:
                /* do nothing */
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
  
  return zebra_server_send_message(client);
}
#endif /* HAVE_IPV6 */

static int
zsend_ipv4_nexthop_lookup (struct zserv *client, struct in_addr addr)
{
  struct stream *s;
  struct rib *rib;
  unsigned long nump;
  u_char num;
  struct nexthop *nexthop;

  /* Lookup nexthop - eBGP excluded */
  rib = rib_match_ipv4_safi (addr, SAFI_UNICAST, 1, NULL);

  /* Get output stream. */
  s = client->obuf;
  stream_reset (s);

  /* Fill in result. */
  zserv_create_header (s, ZEBRA_IPV4_NEXTHOP_LOOKUP);
  stream_put_in_addr (s, &addr);

  if (rib)
    {
      if (IS_ZEBRA_DEBUG_PACKET && IS_ZEBRA_DEBUG_RECV)
        zlog_debug("%s: Matching rib entry found.", __func__);
      stream_putl (s, rib->metric);
      num = 0;
      nump = stream_get_endp(s);
      stream_putc (s, 0);
      /* Only non-recursive routes are elegible to resolve the nexthop we
       * are looking up. Therefore, we will just iterate over the top
       * chain of nexthops. */
      for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
	if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
	  {
	    stream_putc (s, nexthop->type);
	    switch (nexthop->type)
	      {
	      case ZEBRA_NEXTHOP_IPV4:
		stream_put_in_addr (s, &nexthop->gate.ipv4);
		break;
	      case ZEBRA_NEXTHOP_IPV4_IFINDEX:
		stream_put_in_addr (s, &nexthop->gate.ipv4);
		stream_putl (s, nexthop->ifindex);
		break;
	      case ZEBRA_NEXTHOP_IFINDEX:
	      case ZEBRA_NEXTHOP_IFNAME:
		stream_putl (s, nexthop->ifindex);
		break;
	      default:
                /* do nothing */
		break;
	      }
	    num++;
	  }
      stream_putc_at (s, nump, num);
    }
  else
    {
      if (IS_ZEBRA_DEBUG_PACKET && IS_ZEBRA_DEBUG_RECV)
        zlog_debug("%s: No matching rib entry found.", __func__);
      stream_putl (s, 0);
      stream_putc (s, 0);
    }

  stream_putw_at (s, 0, stream_get_endp (s));
  
  return zebra_server_send_message(client);
}

/*
  Modified version of zsend_ipv4_nexthop_lookup():
  Query unicast rib if nexthop is not found on mrib.
  Returns both route metric and protocol distance.
*/
static int
zsend_ipv4_nexthop_lookup_mrib (struct zserv *client, struct in_addr addr,
				struct rib *rib)
{
  struct stream *s;
  unsigned long nump;
  u_char num;
  struct nexthop *nexthop;

  /* Get output stream. */
  s = client->obuf;
  stream_reset (s);

  /* Fill in result. */
  zserv_create_header (s, ZEBRA_IPV4_NEXTHOP_LOOKUP_MRIB);
  stream_put_in_addr (s, &addr);

  if (rib)
    {
      stream_putc (s, rib->distance);
      stream_putl (s, rib->metric);
      num = 0;
      nump = stream_get_endp(s); /* remember position for nexthop_num */
      stream_putc (s, 0);        /* reserve room for nexthop_num */
      /* Only non-recursive routes are elegible to resolve the nexthop we
       * are looking up. Therefore, we will just iterate over the top
       * chain of nexthops. */
      for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
	if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
	  {
	    stream_putc (s, nexthop->type);
	    switch (nexthop->type)
	      {
	      case ZEBRA_NEXTHOP_IPV4:
		stream_put_in_addr (s, &nexthop->gate.ipv4);
		break;
	      case ZEBRA_NEXTHOP_IPV4_IFINDEX:
		stream_put_in_addr (s, &nexthop->gate.ipv4);
		stream_putl (s, nexthop->ifindex);
		break;
	      case ZEBRA_NEXTHOP_IFINDEX:
	      case ZEBRA_NEXTHOP_IFNAME:
		stream_putl (s, nexthop->ifindex);
		break;
	      default:
		/* do nothing */
		break;
	      }
	    num++;
	  }
    
      stream_putc_at (s, nump, num); /* store nexthop_num */
    }
  else
    {
      stream_putc (s, 0); /* distance */
      stream_putl (s, 0); /* metric */
      stream_putc (s, 0); /* nexthop_num */
    }

  stream_putw_at (s, 0, stream_get_endp (s));
  
  return zebra_server_send_message(client);
}

static int
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
  zserv_create_header (s, ZEBRA_IPV4_IMPORT_LOOKUP);
  stream_put_in_addr (s, &p->prefix);

  if (rib)
    {
      stream_putl (s, rib->metric);
      num = 0;
      nump = stream_get_endp(s);
      stream_putc (s, 0);
      for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
	if (CHECK_FLAG(nexthop->flags, NEXTHOP_FLAG_FIB)
            || nexthop_has_fib_child(nexthop))
	  {
	    stream_putc (s, nexthop->type);
	    switch (nexthop->type)
	      {
	      case ZEBRA_NEXTHOP_IPV4:
		stream_put_in_addr (s, &nexthop->gate.ipv4);
		break;
	      case ZEBRA_NEXTHOP_IPV4_IFINDEX:
		stream_put_in_addr (s, &nexthop->gate.ipv4);
		stream_putl (s, nexthop->ifindex);
		break;
	      case ZEBRA_NEXTHOP_IFINDEX:
	      case ZEBRA_NEXTHOP_IFNAME:
		stream_putl (s, nexthop->ifindex);
		break;
	      default:
                /* do nothing */
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
  
  return zebra_server_send_message(client);
}

/* Router-id is updated. Send ZEBRA_ROUTER_ID_ADD to client. */
int
zsend_router_id_update (struct zserv *client, struct prefix *p)
{
  struct stream *s;
  int blen;

  /* Check this client need interface information. */
  if (!client->ridinfo)
    return 0;

  s = client->obuf;
  stream_reset (s);

  /* Message type. */
  zserv_create_header (s, ZEBRA_ROUTER_ID_UPDATE);

  /* Prefix information. */
  stream_putc (s, p->family);
  blen = prefix_blen (p);
  stream_put (s, &p->u.prefix, blen);
  stream_putc (s, p->prefixlen);

  /* Write packet size. */
  stream_putw_at (s, 0, stream_get_endp (s));

  return zebra_server_send_message(client);
}

/* Register zebra server interface information.  Send current all
   interface and address information. */
static int
zread_interface_add (struct zserv *client, u_short length)
{
  struct listnode *ifnode, *ifnnode;
  struct listnode *cnode, *cnnode;
  struct interface *ifp;
  struct connected *c;

  /* Interface information is needed. */
  client->ifinfo = 1;

  for (ALL_LIST_ELEMENTS (iflist, ifnode, ifnnode, ifp))
    {
      /* Skip pseudo interface. */
      if (! CHECK_FLAG (ifp->status, ZEBRA_INTERFACE_ACTIVE))
	continue;

      if (zsend_interface_add (client, ifp) < 0)
        return -1;

      for (ALL_LIST_ELEMENTS (ifp->connected, cnode, cnnode, c))
	{
	  if (CHECK_FLAG (c->conf, ZEBRA_IFC_REAL) &&
	      (zsend_interface_address (ZEBRA_INTERFACE_ADDRESS_ADD, client, 
				        ifp, c) < 0))
	    return -1;
	}
    }
  return 0;
}

/* Unregister zebra server interface information. */
static int
zread_interface_delete (struct zserv *client, u_short length)
{
  client->ifinfo = 0;
  return 0;
}

/* This function support multiple nexthop. */
/* 
 * Parse the ZEBRA_IPV4_ROUTE_ADD sent from client. Update rib and
 * add kernel route. 
 */
static int
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
  safi_t safi;	


  /* Get input stream.  */
  s = client->ibuf;

  /* Allocate new rib. */
  rib = XCALLOC (MTYPE_RIB, sizeof (struct rib));
  
  /* Type, flags, message. */
  rib->type = stream_getc (s);
  rib->flags = stream_getc (s);
  message = stream_getc (s); 
  safi = stream_getw (s);
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
	      stream_forward_getp (s, ifname_len);
	      break;
	    case ZEBRA_NEXTHOP_IPV4:
	      nexthop.s_addr = stream_get_ipv4 (s);
	      nexthop_ipv4_add (rib, &nexthop, NULL);
	      break;
	    case ZEBRA_NEXTHOP_IPV4_IFINDEX:
	      nexthop.s_addr = stream_get_ipv4 (s);
	      ifindex = stream_getl (s);
	      nexthop_ipv4_ifindex_add (rib, &nexthop, NULL, ifindex);
	      break;
	    case ZEBRA_NEXTHOP_IPV6:
	      stream_forward_getp (s, IPV6_MAX_BYTELEN);
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
    
  /* Table */
  rib->table=zebrad.rtm_table_default;
  rib_add_ipv4_multipath (&p, rib, safi);
  return 0;
}

/* Zebra server IPv4 prefix delete function. */
static int
zread_ipv4_delete (struct zserv *client, u_short length)
{
  int i;
  struct stream *s;
  struct zapi_ipv4 api;
  struct in_addr nexthop, *nexthop_p;
  unsigned long ifindex;
  struct prefix_ipv4 p;
  u_char nexthop_num;
  u_char nexthop_type;
  u_char ifname_len;
  
  s = client->ibuf;
  ifindex = 0;
  nexthop.s_addr = 0;
  nexthop_p = NULL;

  /* Type, flags, message. */
  api.type = stream_getc (s);
  api.flags = stream_getc (s);
  api.message = stream_getc (s);
  api.safi = stream_getw (s);

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
	      stream_forward_getp (s, ifname_len);
	      break;
	    case ZEBRA_NEXTHOP_IPV4:
	      nexthop.s_addr = stream_get_ipv4 (s);
	      nexthop_p = &nexthop;
	      break;
	    case ZEBRA_NEXTHOP_IPV4_IFINDEX:
	      nexthop.s_addr = stream_get_ipv4 (s);
	      nexthop_p = &nexthop;
	      ifindex = stream_getl (s);
	      break;
	    case ZEBRA_NEXTHOP_IPV6:
	      stream_forward_getp (s, IPV6_MAX_BYTELEN);
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
    
  rib_delete_ipv4 (api.type, api.flags, &p, nexthop_p, ifindex,
		   client->rtm_table, api.safi);
  return 0;
}

/* Nexthop lookup for IPv4. */
static int
zread_ipv4_nexthop_lookup (struct zserv *client, u_short length)
{
  struct in_addr addr;
  char buf[BUFSIZ];

  addr.s_addr = stream_get_ipv4 (client->ibuf);
  if (IS_ZEBRA_DEBUG_PACKET && IS_ZEBRA_DEBUG_RECV)
    zlog_debug("%s: looking up %s", __func__,
               inet_ntop (AF_INET, &addr, buf, BUFSIZ));
  return zsend_ipv4_nexthop_lookup (client, addr);
}

/* MRIB Nexthop lookup for IPv4. */
static int
zread_ipv4_nexthop_lookup_mrib (struct zserv *client, u_short length)
{
  struct in_addr addr;
  struct rib *rib;

  addr.s_addr = stream_get_ipv4 (client->ibuf);
  rib = rib_match_ipv4_multicast (addr, NULL);
  return zsend_ipv4_nexthop_lookup_mrib (client, addr, rib);
}

/* Nexthop lookup for IPv4. */
static int
zread_ipv4_import_lookup (struct zserv *client, u_short length)
{
  struct prefix_ipv4 p;

  p.family = AF_INET;
  p.prefixlen = stream_getc (client->ibuf);
  p.prefix.s_addr = stream_get_ipv4 (client->ibuf);

  return zsend_ipv4_import_lookup (client, &p);
}

#ifdef HAVE_IPV6
/* Zebra server IPv6 prefix add function. */
static int
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
  api.safi = stream_getw (s);

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
    rib_add_ipv6 (api.type, api.flags, &p, NULL, ifindex, zebrad.rtm_table_default, api.metric,
		  api.distance, api.safi);
  else
    rib_add_ipv6 (api.type, api.flags, &p, &nexthop, ifindex, zebrad.rtm_table_default, api.metric,
		  api.distance, api.safi);
  return 0;
}

/* Zebra server IPv6 prefix delete function. */
static int
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
  api.safi = stream_getw (s);

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
    rib_delete_ipv6 (api.type, api.flags, &p, NULL, ifindex, client->rtm_table, api.safi);
  else
    rib_delete_ipv6 (api.type, api.flags, &p, &nexthop, ifindex, client->rtm_table, api.safi);
  return 0;
}

static int
zread_ipv6_nexthop_lookup (struct zserv *client, u_short length)
{
  struct in6_addr addr;
  char buf[BUFSIZ];

  stream_get (&addr, client->ibuf, 16);
  if (IS_ZEBRA_DEBUG_PACKET && IS_ZEBRA_DEBUG_RECV)
    zlog_debug("%s: looking up %s", __func__,
               inet_ntop (AF_INET6, &addr, buf, BUFSIZ));

  return zsend_ipv6_nexthop_lookup (client, &addr);
}
#endif /* HAVE_IPV6 */

/* Register zebra server router-id information.  Send current router-id */
static int
zread_router_id_add (struct zserv *client, u_short length)
{
  struct prefix p;

  /* Router-id information is needed. */
  client->ridinfo = 1;

  router_id_get (&p);

  return zsend_router_id_update (client,&p);
}

/* Unregister zebra server router-id information. */
static int
zread_router_id_delete (struct zserv *client, u_short length)
{
  client->ridinfo = 0;
  return 0;
}

/* Tie up route-type and client->sock */
static void
zread_hello (struct zserv *client)
{
  /* type of protocol (lib/zebra.h) */
  u_char proto;
  proto = stream_getc (client->ibuf);

  /* accept only dynamic routing protocols */
  if ((proto < ZEBRA_ROUTE_MAX)
  &&  (proto > ZEBRA_ROUTE_STATIC))
    {
      zlog_notice ("client %d says hello and bids fair to announce only %s routes",
                    client->sock, zebra_route_string(proto));

      /* if route-type was binded by other client */
      if (route_type_oaths[proto])
        zlog_warn ("sender of %s routes changed %c->%c",
                    zebra_route_string(proto), route_type_oaths[proto],
                    client->sock);

      route_type_oaths[proto] = client->sock;
    }
}

/* If client sent routes of specific type, zebra removes it
 * and returns number of deleted routes.
 */
static void
zebra_score_rib (int client_sock)
{
  int i;

  for (i = ZEBRA_ROUTE_RIP; i < ZEBRA_ROUTE_MAX; i++)
    if (client_sock == route_type_oaths[i])
      {
        zlog_notice ("client %d disconnected. %lu %s routes removed from the rib",
                      client_sock, rib_score_proto (i), zebra_route_string (i));
        route_type_oaths[i] = 0;
        break;
      }
}

/* Close zebra client. */
static void
zebra_client_close (struct zserv *client)
{
  /* Close file descriptor. */
  if (client->sock)
    {
      close (client->sock);
      zebra_score_rib (client->sock);
      client->sock = -1;
    }

  /* Free stream buffers. */
  if (client->ibuf)
    stream_free (client->ibuf);
  if (client->obuf)
    stream_free (client->obuf);
  if (client->wb)
    buffer_free(client->wb);

  /* Release threads. */
  if (client->t_read)
    thread_cancel (client->t_read);
  if (client->t_write)
    thread_cancel (client->t_write);
  if (client->t_suicide)
    thread_cancel (client->t_suicide);

  /* Free client structure. */
  listnode_delete (zebrad.client_list, client);
  XFREE (0, client);
}

/* Make new client. */
static void
zebra_client_create (int sock)
{
  struct zserv *client;

  client = XCALLOC (0, sizeof (struct zserv));

  /* Make client input/output buffer. */
  client->sock = sock;
  client->ibuf = stream_new (ZEBRA_MAX_PACKET_SIZ);
  client->obuf = stream_new (ZEBRA_MAX_PACKET_SIZ);
  client->wb = buffer_new(0);

  /* Set table number. */
  client->rtm_table = zebrad.rtm_table_default;

  /* Add this client to linked list. */
  listnode_add (zebrad.client_list, client);
  
  /* Make new read thread. */
  zebra_event (ZEBRA_READ, sock, client);
}

/* Handler of zebra service request. */
static int
zebra_client_read (struct thread *thread)
{
  int sock;
  struct zserv *client;
  size_t already;
  uint16_t length, command;
  uint8_t marker, version;

  /* Get thread data.  Reset reading thread because I'm running. */
  sock = THREAD_FD (thread);
  client = THREAD_ARG (thread);
  client->t_read = NULL;

  if (client->t_suicide)
    {
      zebra_client_close(client);
      return -1;
    }

  /* Read length and command (if we don't have it already). */
  if ((already = stream_get_endp(client->ibuf)) < ZEBRA_HEADER_SIZE)
    {
      ssize_t nbyte;
      if (((nbyte = stream_read_try (client->ibuf, sock,
				     ZEBRA_HEADER_SIZE-already)) == 0) ||
	  (nbyte == -1))
	{
	  if (IS_ZEBRA_DEBUG_EVENT)
	    zlog_debug ("connection closed socket [%d]", sock);
	  zebra_client_close (client);
	  return -1;
	}
      if (nbyte != (ssize_t)(ZEBRA_HEADER_SIZE-already))
	{
	  /* Try again later. */
	  zebra_event (ZEBRA_READ, sock, client);
	  return 0;
	}
      already = ZEBRA_HEADER_SIZE;
    }

  /* Reset to read from the beginning of the incoming packet. */
  stream_set_getp(client->ibuf, 0);

  /* Fetch header values */
  length = stream_getw (client->ibuf);
  marker = stream_getc (client->ibuf);
  version = stream_getc (client->ibuf);
  command = stream_getw (client->ibuf);

  if (marker != ZEBRA_HEADER_MARKER || version != ZSERV_VERSION)
    {
      zlog_err("%s: socket %d version mismatch, marker %d, version %d",
               __func__, sock, marker, version);
      zebra_client_close (client);
      return -1;
    }
  if (length < ZEBRA_HEADER_SIZE) 
    {
      zlog_warn("%s: socket %d message length %u is less than header size %d",
	        __func__, sock, length, ZEBRA_HEADER_SIZE);
      zebra_client_close (client);
      return -1;
    }
  if (length > STREAM_SIZE(client->ibuf))
    {
      zlog_warn("%s: socket %d message length %u exceeds buffer size %lu",
	        __func__, sock, length, (u_long)STREAM_SIZE(client->ibuf));
      zebra_client_close (client);
      return -1;
    }

  /* Read rest of data. */
  if (already < length)
    {
      ssize_t nbyte;
      if (((nbyte = stream_read_try (client->ibuf, sock,
				     length-already)) == 0) ||
	  (nbyte == -1))
	{
	  if (IS_ZEBRA_DEBUG_EVENT)
	    zlog_debug ("connection closed [%d] when reading zebra data", sock);
	  zebra_client_close (client);
	  return -1;
	}
      if (nbyte != (ssize_t)(length-already))
        {
	  /* Try again later. */
	  zebra_event (ZEBRA_READ, sock, client);
	  return 0;
	}
    }

  length -= ZEBRA_HEADER_SIZE;

  /* Debug packet information. */
  if (IS_ZEBRA_DEBUG_EVENT)
    zlog_debug ("zebra message comes from socket [%d]", sock);

  if (IS_ZEBRA_DEBUG_PACKET && IS_ZEBRA_DEBUG_RECV)
    zlog_debug ("zebra message received [%s] %d", 
	       zserv_command_string (command), length);

  switch (command) 
    {
    case ZEBRA_ROUTER_ID_ADD:
      zread_router_id_add (client, length);
      break;
    case ZEBRA_ROUTER_ID_DELETE:
      zread_router_id_delete (client, length);
      break;
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
    case ZEBRA_IPV4_NEXTHOP_LOOKUP_MRIB:
      zread_ipv4_nexthop_lookup_mrib (client, length);
      break;
#ifdef HAVE_IPV6
    case ZEBRA_IPV6_NEXTHOP_LOOKUP:
      zread_ipv6_nexthop_lookup (client, length);
      break;
#endif /* HAVE_IPV6 */
    case ZEBRA_IPV4_IMPORT_LOOKUP:
      zread_ipv4_import_lookup (client, length);
      break;
    case ZEBRA_HELLO:
      zread_hello (client);
      break;
    default:
      zlog_info ("Zebra received unknown command %d", command);
      break;
    }

  if (client->t_suicide)
    {
      /* No need to wait for thread callback, just kill immediately. */
      zebra_client_close(client);
      return -1;
    }

  stream_reset (client->ibuf);
  zebra_event (ZEBRA_READ, sock, client);
  return 0;
}


/* Accept code of zebra server socket. */
static int
zebra_accept (struct thread *thread)
{
  int accept_sock;
  int client_sock;
  struct sockaddr_in client;
  socklen_t len;

  accept_sock = THREAD_FD (thread);

  /* Reregister myself. */
  zebra_event (ZEBRA_SERV, accept_sock, NULL);

  len = sizeof (struct sockaddr_in);
  client_sock = accept (accept_sock, (struct sockaddr *) &client, &len);

  if (client_sock < 0)
    {
      zlog_warn ("Can't accept zebra socket: %s", safe_strerror (errno));
      return -1;
    }

  /* Make client socket non-blocking.  */
  set_nonblocking(client_sock);
  
  /* Create new zebra client. */
  zebra_client_create (client_sock);

  return 0;
}

#ifdef HAVE_TCP_ZEBRA
/* Make zebra's server socket. */
static void
zebra_serv ()
{
  int ret;
  int accept_sock;
  struct sockaddr_in addr;

  accept_sock = socket (AF_INET, SOCK_STREAM, 0);

  if (accept_sock < 0) 
    {
      zlog_warn ("Can't create zserv stream socket: %s", 
                 safe_strerror (errno));
      zlog_warn ("zebra can't provice full functionality due to above error");
      return;
    }

  memset (&route_type_oaths, 0, sizeof (route_type_oaths));
  memset (&addr, 0, sizeof (struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons (ZEBRA_PORT);
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  addr.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */
  addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

  sockopt_reuseaddr (accept_sock);
  sockopt_reuseport (accept_sock);

  if ( zserv_privs.change(ZPRIVS_RAISE) )
    zlog (NULL, LOG_ERR, "Can't raise privileges");
    
  ret  = bind (accept_sock, (struct sockaddr *)&addr, 
	       sizeof (struct sockaddr_in));
  if (ret < 0)
    {
      zlog_warn ("Can't bind to stream socket: %s", 
                 safe_strerror (errno));
      zlog_warn ("zebra can't provice full functionality due to above error");
      close (accept_sock);      /* Avoid sd leak. */
      return;
    }
    
  if ( zserv_privs.change(ZPRIVS_LOWER) )
    zlog (NULL, LOG_ERR, "Can't lower privileges");

  ret = listen (accept_sock, 1);
  if (ret < 0)
    {
      zlog_warn ("Can't listen to stream socket: %s", 
                 safe_strerror (errno));
      zlog_warn ("zebra can't provice full functionality due to above error");
      close (accept_sock);	/* Avoid sd leak. */
      return;
    }

  zebra_event (ZEBRA_SERV, accept_sock, NULL);
}
#endif /* HAVE_TCP_ZEBRA */

/* For sockaddr_un. */
#include <sys/un.h>

/* zebra server UNIX domain socket. */
static void
zebra_serv_un (const char *path)
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
      zlog_warn ("Can't create zserv unix socket: %s", 
                 safe_strerror (errno));
      zlog_warn ("zebra can't provide full functionality due to above error");
      return;
    }

  memset (&route_type_oaths, 0, sizeof (route_type_oaths));

  /* Make server socket. */
  memset (&serv, 0, sizeof (struct sockaddr_un));
  serv.sun_family = AF_UNIX;
  strncpy (serv.sun_path, path, strlen (path));
#ifdef HAVE_STRUCT_SOCKADDR_UN_SUN_LEN
  len = serv.sun_len = SUN_LEN(&serv);
#else
  len = sizeof (serv.sun_family) + strlen (serv.sun_path);
#endif /* HAVE_STRUCT_SOCKADDR_UN_SUN_LEN */

  ret = bind (sock, (struct sockaddr *) &serv, len);
  if (ret < 0)
    {
      zlog_warn ("Can't bind to unix socket %s: %s", 
                 path, safe_strerror (errno));
      zlog_warn ("zebra can't provide full functionality due to above error");
      close (sock);
      return;
    }

  ret = listen (sock, 5);
  if (ret < 0)
    {
      zlog_warn ("Can't listen to unix socket %s: %s", 
                 path, safe_strerror (errno));
      zlog_warn ("zebra can't provide full functionality due to above error");
      close (sock);
      return;
    }

  umask (old_mask);

  zebra_event (ZEBRA_SERV, sock, NULL);
}


static void
zebra_event (enum event event, int sock, struct zserv *client)
{
  switch (event)
    {
    case ZEBRA_SERV:
      thread_add_read (zebrad.master, zebra_accept, client, sock);
      break;
    case ZEBRA_READ:
      client->t_read = 
	thread_add_read (zebrad.master, zebra_client_read, client, sock);
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
  vty_out (vty, "table %d%s", zebrad.rtm_table_default,
	   VTY_NEWLINE);
  return CMD_SUCCESS;
}

DEFUN (config_table, 
       config_table_cmd,
       "table TABLENO",
       "Configure target kernel routing table\n"
       "TABLE integer\n")
{
  zebrad.rtm_table_default = strtol (argv[0], (char**)0, 10);
  return CMD_SUCCESS;
}

DEFUN (ip_forwarding,
       ip_forwarding_cmd,
       "ip forwarding",
       IP_STR
       "Turn on IP forwarding")
{
  int ret;

  ret = ipforward ();
  if (ret == 0)
    ret = ipforward_on ();

  if (ret == 0)
    {
      vty_out (vty, "Can't turn on IP forwarding%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

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
  if (ret != 0)
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
  struct listnode *node;
  struct zserv *client;

  for (ALL_LIST_ELEMENTS_RO (zebrad.client_list, node, client))
    vty_out (vty, "Client fd %d%s", client->sock, VTY_NEWLINE);
  
  return CMD_SUCCESS;
}

/* Table configuration write function. */
static int
config_write_table (struct vty *vty)
{
  if (zebrad.rtm_table_default)
    vty_out (vty, "table %d%s", zebrad.rtm_table_default,
	     VTY_NEWLINE);
  return 0;
}

/* table node for routing tables. */
static struct cmd_node table_node =
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

DEFUN (ipv6_forwarding,
       ipv6_forwarding_cmd,
       "ipv6 forwarding",
       IPV6_STR
       "Turn on IPv6 forwarding")
{
  int ret;

  ret = ipforward_ipv6 ();
  if (ret == 0)
    ret = ipforward_ipv6_on ();

  if (ret == 0)
    {
      vty_out (vty, "Can't turn on IPv6 forwarding%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

DEFUN (no_ipv6_forwarding,
       no_ipv6_forwarding_cmd,
       "no ipv6 forwarding",
       NO_STR
       IPV6_STR
       "Turn off IPv6 forwarding")
{
  int ret;

  ret = ipforward_ipv6 ();
  if (ret != 0)
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
static int
config_write_forwarding (struct vty *vty)
{
  /* FIXME: Find better place for that. */
  router_id_write (vty);

  if (ipforward ())
    vty_out (vty, "ip forwarding%s", VTY_NEWLINE);
#ifdef HAVE_IPV6
  if (ipforward_ipv6 ())
    vty_out (vty, "ipv6 forwarding%s", VTY_NEWLINE);
#endif /* HAVE_IPV6 */
  vty_out (vty, "!%s", VTY_NEWLINE);
  return 0;
}

/* table node for routing tables. */
static struct cmd_node forwarding_node =
{
  FORWARDING_NODE,
  "",				/* This node has no interface. */
  1
};


/* Initialisation of zebra and installation of commands. */
void
zebra_init (void)
{
  /* Client list init. */
  zebrad.client_list = list_new ();

  /* Install configuration write function. */
  install_node (&table_node, config_write_table);
  install_node (&forwarding_node, config_write_forwarding);

  install_element (VIEW_NODE, &show_ip_forwarding_cmd);
  install_element (ENABLE_NODE, &show_ip_forwarding_cmd);
  install_element (CONFIG_NODE, &ip_forwarding_cmd);
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
  install_element (CONFIG_NODE, &ipv6_forwarding_cmd);
  install_element (CONFIG_NODE, &no_ipv6_forwarding_cmd);
#endif /* HAVE_IPV6 */

  /* Route-map */
  zebra_route_map_init ();
}

/* Make zebra server socket, wiping any existing one (see bug #403). */
void
zebra_zserv_socket_init (char *path)
{
#ifdef HAVE_TCP_ZEBRA
  zebra_serv ();
#else
  zebra_serv_un (path ? path : ZEBRA_SERV_PATH);
#endif /* HAVE_TCP_ZEBRA */
}
