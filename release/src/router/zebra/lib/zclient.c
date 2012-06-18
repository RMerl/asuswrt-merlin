/* Zebra's client library.
 * Copyright (C) 1999 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 */

#include <zebra.h>

#include "prefix.h"
#include "stream.h"
#include "network.h"
#include "if.h"
#include "log.h"
#include "thread.h"
#include "zclient.h"
#include "memory.h"
#include "table.h"

#include "zebra/rib.h"
#include "zebra/zserv.h"

/* Zebra client events. */
enum event {ZCLIENT_SCHEDULE, ZCLIENT_READ, ZCLIENT_CONNECT};

/* Prototype for event manager. */
static void zclient_event (enum event, struct zclient *);

/* This file local debug flag. */
int zclient_debug = 0;

/* Allocate zclient structure. */
struct zclient *
zclient_new ()
{
  struct zclient *zclient;
  zclient = XMALLOC (MTYPE_ZCLIENT, sizeof (struct zclient));
  memset (zclient, 0, sizeof (struct zclient));

  zclient->ibuf = stream_new (ZEBRA_MAX_PACKET_SIZ);
  zclient->obuf = stream_new (ZEBRA_MAX_PACKET_SIZ);

  return zclient;
}

/* Free zclient structure. */
void
zclient_free (struct zclient *zclient)
{
  XFREE (MTYPE_ZCLIENT, zclient);
}

/* Initialize zebra client.  Argument redist_default is unwanted
   redistribute route type. */
void
zclient_init (struct zclient *zclient, int redist_default)
{
  int i;
  
  /* Enable zebra client connection by default. */
  zclient->enable = 1;

  /* Set -1 to the default socket value. */
  zclient->sock = -1;

  /* Clear redistribution flags. */
  for (i = 0; i < ZEBRA_ROUTE_MAX; i++)
    zclient->redist[i] = 0;

  /* Set unwanted redistribute route.  bgpd does not need BGP route
     redistribution. */
  zclient->redist_default = redist_default;
  zclient->redist[redist_default] = 1;

  /* Set default-information redistribute to zero. */
  zclient->default_information = 0;

  /* Schedule first zclient connection. */
  if (zclient_debug)
    zlog_info ("zclient start scheduled");

  zclient_event (ZCLIENT_SCHEDULE, zclient);
}

/* Stop zebra client services. */
void
zclient_stop (struct zclient *zclient)
{
  if (zclient_debug)
    zlog_info ("zclient stopped");

  /* Stop threads. */
  if (zclient->t_read)
    {
      thread_cancel (zclient->t_read);
      zclient->t_read = NULL;
   }
  if (zclient->t_connect)
    {
      thread_cancel (zclient->t_connect);
      zclient->t_connect = NULL;
    }

  /* Close socket. */
  if (zclient->sock >= 0)
    {
      close (zclient->sock);
      zclient->sock = -1;
    }
  zclient->fail = 0;
}

void
zclient_reset (struct zclient *zclient)
{
  zclient_stop (zclient);
  zclient_init (zclient, zclient->redist_default);
}

/* Make socket to zebra daemon. Return zebra socket. */
int
zclient_socket ()
{
  int sock;
  int ret;
  struct sockaddr_in serv;

  /* We should think about IPv6 connection. */
  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    return -1;
  
  /* Make server socket. */ 
  memset (&serv, 0, sizeof (struct sockaddr_in));
  serv.sin_family = AF_INET;
  serv.sin_port = htons (ZEBRA_PORT);
#ifdef HAVE_SIN_LEN
  serv.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */
  serv.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

  /* Connect to zebra. */
  ret = connect (sock, (struct sockaddr *) &serv, sizeof (serv));
  if (ret < 0)
    {
      close (sock);
      return -1;
    }
  return sock;
}

/* For sockaddr_un. */
#include <sys/un.h>

int
zclient_socket_un (char *path)
{
  int ret;
  int sock, len;
  struct sockaddr_un addr;

  sock = socket (AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0)
    return -1;
  
  /* Make server socket. */ 
  memset (&addr, 0, sizeof (struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strncpy (addr.sun_path, path, strlen (path));
#ifdef HAVE_SUN_LEN
  len = addr.sun_len = SUN_LEN(&addr);
#else
  len = sizeof (addr.sun_family) + strlen (addr.sun_path);
#endif /* HAVE_SUN_LEN */

  ret = connect (sock, (struct sockaddr *) &addr, len);
  if (ret < 0)
    {
      close (sock);
      return -1;
    }
  return sock;
}

/* Send simple Zebra message. */
int
zebra_message_send (struct zclient *zclient, int command)
{
  struct stream *s;

  /* Get zclient output buffer. */
  s = zclient->obuf;
  stream_reset (s);

  /* Send very simple command only Zebra message. */
  stream_putw (s, 3);
  stream_putc (s, command);

  return writen (zclient->sock, s->data, 3);
}

/* Make connection to zebra daemon. */
int
zclient_start (struct zclient *zclient)
{
  int i;

  if (zclient_debug)
    zlog_info ("zclient_start is called");

  /* zclient is disabled. */
  if (! zclient->enable)
    return 0;

  /* If already connected to the zebra. */
  if (zclient->sock >= 0)
    return 0;

  /* Check connect thread. */
  if (zclient->t_connect)
    return 0;

  /* Make socket. */
#ifdef HAVE_TCP_ZEBRA
  zclient->sock = zclient_socket ();
#else
  zclient->sock = zclient_socket_un (ZEBRA_SERV_PATH);
#endif /* HAVE_TCP_ZEBRA */
  if (zclient->sock < 0)
    {
      if (zclient_debug)
	zlog_info ("zclient connection fail");
      zclient->fail++;
      zclient_event (ZCLIENT_CONNECT, zclient);
      return -1;
    }

  /* Clear fail count. */
  zclient->fail = 0;
  if (zclient_debug)
    zlog_info ("zclient connect success with socket [%d]", zclient->sock);
      
  /* Create read thread. */
  zclient_event (ZCLIENT_READ, zclient);

  /* We need interface information. */
  zebra_message_send (zclient, ZEBRA_INTERFACE_ADD);

  /* Flush all redistribute request. */
  for (i = 0; i < ZEBRA_ROUTE_MAX; i++)
    if (i != zclient->redist_default && zclient->redist[i])
      zebra_redistribute_send (ZEBRA_REDISTRIBUTE_ADD, zclient->sock, i);

  /* If default information is needed. */
  if (zclient->default_information)
    zebra_message_send (zclient, ZEBRA_REDISTRIBUTE_DEFAULT_ADD);

  return 0;
}

/* This function is a wrapper function for calling zclient_start from
   timer or event thread. */
int
zclient_connect (struct thread *t)
{
  struct zclient *zclient;

  zclient = THREAD_ARG (t);
  zclient->t_connect = NULL;

  if (zclient_debug)
    zlog_info ("zclient_connect is called");

  return zclient_start (zclient);
}

int
zapi_ipv4_add (struct zclient *zclient, struct prefix_ipv4 *p,
	       struct zapi_ipv4 *api)
{
  int i;
  int psize;
  struct stream *s;

  /* Reset stream. */
  s = zclient->obuf;
  stream_reset (s);

  /* Length place holder. */
  stream_putw (s, 0);

  /* Put command, type and nexthop. */
  stream_putc (s, ZEBRA_IPV4_ROUTE_ADD);
  stream_putc (s, api->type);
  stream_putc (s, api->flags);
  stream_putc (s, api->message);
  
  /* Put prefix information. */
  psize = PSIZE (p->prefixlen);
  stream_putc (s, p->prefixlen);
  stream_write (s, (u_char *)&p->prefix, psize);

  /* Nexthop, ifindex, distance and metric information. */
  if (CHECK_FLAG (api->message, ZAPI_MESSAGE_NEXTHOP))
    {
      if (CHECK_FLAG (api->flags, ZEBRA_FLAG_BLACKHOLE))
	{
	  stream_putc (s, 1);
	  stream_putc (s, ZEBRA_NEXTHOP_BLACKHOLE);
	}
      else
	stream_putc (s, api->nexthop_num + api->ifindex_num);

      for (i = 0; i < api->nexthop_num; i++)
	{
	  stream_putc (s, ZEBRA_NEXTHOP_IPV4);
	  stream_put_in_addr (s, api->nexthop[i]);
	}
      for (i = 0; i < api->ifindex_num; i++)
	{
	  stream_putc (s, ZEBRA_NEXTHOP_IFINDEX);
	  stream_putl (s, api->ifindex[i]);
	}
    }

  if (CHECK_FLAG (api->message, ZAPI_MESSAGE_DISTANCE))
    stream_putc (s, api->distance);
  if (CHECK_FLAG (api->message, ZAPI_MESSAGE_METRIC))
    stream_putl (s, api->metric);

  /* Put length at the first point of the stream. */
  stream_putw_at (s, 0, stream_get_endp (s));

  return writen (zclient->sock, s->data, stream_get_endp (s));
}

int
zapi_ipv4_delete (struct zclient *zclient, struct prefix_ipv4 *p,
		  struct zapi_ipv4 *api)
{
  int i;
  int psize;
  struct stream *s;

  /* Reset stream. */
  s = zclient->obuf;
  stream_reset (s);

  /* Length place holder. */
  stream_putw (s, 0);

  /* Put command, type and nexthop. */
  stream_putc (s, ZEBRA_IPV4_ROUTE_DELETE);
  stream_putc (s, api->type);
  stream_putc (s, api->flags);
  stream_putc (s, api->message);
  
  /* Put prefix information. */
  psize = PSIZE (p->prefixlen);
  stream_putc (s, p->prefixlen);
  stream_write (s, (u_char *)&p->prefix, psize);

  /* Nexthop, ifindex, distance and metric information. */
  if (CHECK_FLAG (api->message, ZAPI_MESSAGE_NEXTHOP))
    {
      if (CHECK_FLAG (api->flags, ZEBRA_FLAG_BLACKHOLE))
	{
	  stream_putc (s, 1);
	  stream_putc (s, ZEBRA_NEXTHOP_BLACKHOLE);
	}
      else
	stream_putc (s, api->nexthop_num + api->ifindex_num);

      for (i = 0; i < api->nexthop_num; i++)
	{
	  stream_putc (s, ZEBRA_NEXTHOP_IPV4);
	  stream_put_in_addr (s, api->nexthop[i]);
	}
      for (i = 0; i < api->ifindex_num; i++)
	{
	  stream_putc (s, ZEBRA_NEXTHOP_IFINDEX);
	  stream_putl (s, api->ifindex[i]);
	}
    }

  if (CHECK_FLAG (api->message, ZAPI_MESSAGE_DISTANCE))
    stream_putc (s, api->distance);
  if (CHECK_FLAG (api->message, ZAPI_MESSAGE_METRIC))
    stream_putl (s, api->metric);

  /* Put length at the first point of the stream. */
  stream_putw_at (s, 0, stream_get_endp (s));

  return writen (zclient->sock, s->data, stream_get_endp (s));
}

#ifdef HAVE_IPV6
int
zapi_ipv6_add (struct zclient *zclient, struct prefix_ipv6 *p,
	       struct zapi_ipv6 *api)
{
  int i;
  int psize;
  struct stream *s;

  /* Reset stream. */
  s = zclient->obuf;
  stream_reset (s);

  /* Length place holder. */
  stream_putw (s, 0);

  /* Put command, type and nexthop. */
  stream_putc (s, ZEBRA_IPV6_ROUTE_ADD);
  stream_putc (s, api->type);
  stream_putc (s, api->flags);
  stream_putc (s, api->message);
  
  /* Put prefix information. */
  psize = PSIZE (p->prefixlen);
  stream_putc (s, p->prefixlen);
  stream_write (s, (u_char *)&p->prefix, psize);

  /* Nexthop, ifindex, distance and metric information. */
  if (CHECK_FLAG (api->message, ZAPI_MESSAGE_NEXTHOP))
    {
      stream_putc (s, api->nexthop_num + api->ifindex_num);

      for (i = 0; i < api->nexthop_num; i++)
	{
	  stream_putc (s, ZEBRA_NEXTHOP_IPV6);
	  stream_write (s, (u_char *)api->nexthop[i], 16);
	}
      for (i = 0; i < api->ifindex_num; i++)
	{
	  stream_putc (s, ZEBRA_NEXTHOP_IFINDEX);
	  stream_putl (s, api->ifindex[i]);
	}
    }

  if (CHECK_FLAG (api->message, ZAPI_MESSAGE_DISTANCE))
    stream_putc (s, api->distance);
  if (CHECK_FLAG (api->message, ZAPI_MESSAGE_METRIC))
    stream_putl (s, api->metric);

  /* Put length at the first point of the stream. */
  stream_putw_at (s, 0, stream_get_endp (s));

  return writen (zclient->sock, s->data, stream_get_endp (s));
}

int
zapi_ipv6_delete (struct zclient *zclient, struct prefix_ipv6 *p,
		  struct zapi_ipv6 *api)
{
  int i;
  int psize;
  struct stream *s;

  /* Reset stream. */
  s = zclient->obuf;
  stream_reset (s);

  /* Length place holder. */
  stream_putw (s, 0);

  /* Put command, type and nexthop. */
  stream_putc (s, ZEBRA_IPV6_ROUTE_DELETE);
  stream_putc (s, api->type);
  stream_putc (s, api->flags);
  stream_putc (s, api->message);
  
  /* Put prefix information. */
  psize = PSIZE (p->prefixlen);
  stream_putc (s, p->prefixlen);
  stream_write (s, (u_char *)&p->prefix, psize);

  /* Nexthop, ifindex, distance and metric information. */
  if (CHECK_FLAG (api->message, ZAPI_MESSAGE_NEXTHOP))
    {
      stream_putc (s, api->nexthop_num + api->ifindex_num);

      for (i = 0; i < api->nexthop_num; i++)
	{
	  stream_putc (s, ZEBRA_NEXTHOP_IPV6);
	  stream_write (s, (u_char *)api->nexthop[i], 16);
	}
      for (i = 0; i < api->ifindex_num; i++)
	{
	  stream_putc (s, ZEBRA_NEXTHOP_IFINDEX);
	  stream_putl (s, api->ifindex[i]);
	}
    }

  if (CHECK_FLAG (api->message, ZAPI_MESSAGE_DISTANCE))
    stream_putc (s, api->distance);
  if (CHECK_FLAG (api->message, ZAPI_MESSAGE_METRIC))
    stream_putl (s, api->metric);

  /* Put length at the first point of the stream. */
  stream_putw_at (s, 0, stream_get_endp (s));

  return writen (zclient->sock, s->data, stream_get_endp (s));
}

#endif /* HAVE_IPV6 */

int
zebra_redistribute_send (int command, int sock, int type)
{
  int ret;
  struct stream *s;

  s = stream_new (ZEBRA_MAX_PACKET_SIZ);

  /* Total length of the messages. */
  stream_putw (s, 4);
  
  stream_putc (s, command);
  stream_putc (s, type);

  ret = writen (sock, s->data, 4);

  stream_free (s);

  return ret;
}

/* Interface addition from zebra daemon. */
struct interface *
zebra_interface_add_read (struct stream *s)
{
  struct interface *ifp;
  u_char ifname_tmp[INTERFACE_NAMSIZ];

  /* Read interface name. */
  stream_get (ifname_tmp, s, INTERFACE_NAMSIZ);

  /* Lookup this by interface name. */
  ifp = if_lookup_by_name (ifname_tmp);

  /* If such interface does not exist, make new one. */
  if (! ifp)
    {
      ifp = if_create ();
      strncpy (ifp->name, ifname_tmp, IFNAMSIZ);
    }

  /* Read interface's index. */
  ifp->ifindex = stream_getl (s);

  /* Read interface's value. */
  ifp->flags = stream_getl (s);
  ifp->metric = stream_getl (s);
  ifp->mtu = stream_getl (s);
  ifp->bandwidth = stream_getl (s);
#ifdef HAVE_SOCKADDR_DL
  stream_get (&ifp->sdl, s, sizeof (ifp->sdl));
#else
  ifp->hw_addr_len = stream_getl (s);
  if (ifp->hw_addr_len)
    stream_get (ifp->hw_addr, s, ifp->hw_addr_len);
#endif /* HAVE_SOCKADDR_DL */
  
  return ifp;
}

/* Read interface up/down msg from zebra daemon. */
struct interface *
zebra_interface_state_read (struct stream *s)
{
  struct interface *ifp;
  u_char ifname_tmp[INTERFACE_NAMSIZ];

  /* Read interface name. */
  stream_get (ifname_tmp, s, INTERFACE_NAMSIZ);

  /* Lookup this by interface index. */
  ifp = if_lookup_by_name (ifname_tmp);

  /* If such interface does not exist, indicate an error */
  if (! ifp)
     return NULL;

  /* Read interface's index. */
  ifp->ifindex = stream_getl (s);

  /* Read interface's value. */
  ifp->flags = stream_getl (s);
  ifp->metric = stream_getl (s);
  ifp->mtu = stream_getl (s);
  ifp->bandwidth = stream_getl (s);

  return ifp;
}

struct connected *
zebra_interface_address_add_read (struct stream *s)
{
  unsigned int ifindex;
  struct interface *ifp;
  struct connected *ifc;
  struct prefix *p;
  int family;
  int plen;

  /* Get interface index. */
  ifindex = stream_getl (s);

  /* Lookup index. */
  ifp = if_lookup_by_index (ifindex);
  if (ifp == NULL)
    {
      zlog_warn ("zebra_interface_address_add_read: Can't find interface by ifindex: %d ", ifindex);
      return NULL;
    }

  /* Allocate new connected address. */
  ifc = connected_new ();
  ifc->ifp = ifp;

  /* Fetch flag. */
  ifc->flags = stream_getc (s);

  /* Fetch interface address. */
  p = prefix_new ();
  family = p->family = stream_getc (s);

  plen = prefix_blen (p);
  stream_get (&p->u.prefix, s, plen);
  p->prefixlen = stream_getc (s);
  ifc->address = p;

  /* Fetch destination address. */
  p = prefix_new ();
  stream_get (&p->u.prefix, s, plen);
  p->family = family;

  ifc->destination = p;

  p = ifc->address;

  /* Add connected address to the interface. */
  listnode_add (ifp->connected, ifc);

  return ifc;
}

struct connected *
zebra_interface_address_delete_read (struct stream *s)
{
  unsigned int ifindex;
  struct interface *ifp;
  struct connected *ifc;
  struct prefix p;
  struct prefix d;
  int family;
  int len;
  u_char flags;

  /* Get interface index. */
  ifindex = stream_getl (s);

  /* Lookup index. */
  ifp = if_lookup_by_index (ifindex);
  if (ifp == NULL)
    {
      zlog_warn ("zebra_interface_address_delete_read: Can't find interface by ifindex: %d ", ifindex);
      return NULL;
    }

  /* Fetch flag. */
  flags = stream_getc (s);

  /* Fetch interface address. */
  family = p.family = stream_getc (s);

  len = prefix_blen (&p);
  stream_get (&p.u.prefix, s, len);
  p.prefixlen = stream_getc (s);

  /* Fetch destination address. */
  stream_get (&d.u.prefix, s, len);
  d.family = family;

  ifc = connected_delete_by_prefix (ifp, &p);

  return ifc;
}

/* Zebra client message read function. */
int
zclient_read (struct thread *thread)
{
  int ret;
  int nbytes;
  int sock;
  zebra_size_t length;
  zebra_command_t command;
  struct zclient *zclient;

  /* Get socket to zebra. */
  sock = THREAD_FD (thread);
  zclient = THREAD_ARG (thread);
  zclient->t_read = NULL;

  /* Clear input buffer. */
  stream_reset (zclient->ibuf);

  /* Read zebra header. */
  nbytes = stream_read (zclient->ibuf, sock, ZEBRA_HEADER_SIZE);

  /* zebra socket is closed. */
  if (nbytes == 0) 
    {
      if (zclient_debug)
	zlog_info ("zclient connection closed socket [%d].", sock);
      zclient->fail++;
      zclient_stop (zclient);
      zclient_event (ZCLIENT_CONNECT, zclient);
      return -1;
    }

  /* zebra read error. */
  if (nbytes < 0 || nbytes != ZEBRA_HEADER_SIZE)
    {
      if (zclient_debug)
	zlog_info ("Can't read all packet (length %d).", nbytes);
      zclient->fail++;
      zclient_stop (zclient);
      zclient_event (ZCLIENT_CONNECT, zclient);
      return -1;
    }

  /* Fetch length and command. */
  length = stream_getw (zclient->ibuf);
  command = stream_getc (zclient->ibuf);

  /* Length check. */
  if (length >= zclient->ibuf->size)
    {
      stream_free (zclient->ibuf);
      zclient->ibuf = stream_new (length + 1);
    }
  length -= ZEBRA_HEADER_SIZE;

  /* Read rest of zebra packet. */
  nbytes = stream_read (zclient->ibuf, sock, length);
 if (nbytes != length)
   {
     if (zclient_debug)
       zlog_info ("zclient connection closed socket [%d].", sock);
     zclient->fail++;
     zclient_stop (zclient);
     zclient_event (ZCLIENT_CONNECT, zclient);
     return -1;
   }

  switch (command)
    {
    case ZEBRA_INTERFACE_ADD:
      if (zclient->interface_add)
	ret = (*zclient->interface_add) (command, zclient, length);
      break;
    case ZEBRA_INTERFACE_DELETE:
      if (zclient->interface_delete)
	ret = (*zclient->interface_delete) (command, zclient, length);
      break;
    case ZEBRA_INTERFACE_ADDRESS_ADD:
      if (zclient->interface_address_add)
	ret = (*zclient->interface_address_add) (command, zclient, length);
      break;
    case ZEBRA_INTERFACE_ADDRESS_DELETE:
      if (zclient->interface_address_delete)
	ret = (*zclient->interface_address_delete) (command, zclient, length);
      break;
    case ZEBRA_INTERFACE_UP:
      if (zclient->interface_up)
	ret = (*zclient->interface_up) (command, zclient, length);
      break;
    case ZEBRA_INTERFACE_DOWN:
      if (zclient->interface_down)
	ret = (*zclient->interface_down) (command, zclient, length);
      break;
    case ZEBRA_IPV4_ROUTE_ADD:
      if (zclient->ipv4_route_add)
	ret = (*zclient->ipv4_route_add) (command, zclient, length);
      break;
    case ZEBRA_IPV4_ROUTE_DELETE:
      if (zclient->ipv4_route_delete)
	ret = (*zclient->ipv4_route_delete) (command, zclient, length);
      break;
    case ZEBRA_IPV6_ROUTE_ADD:
      if (zclient->ipv6_route_add)
	ret = (*zclient->ipv6_route_add) (command, zclient, length);
      break;
    case ZEBRA_IPV6_ROUTE_DELETE:
      if (zclient->ipv6_route_delete)
	ret = (*zclient->ipv6_route_delete) (command, zclient, length);
      break;
    default:
      break;
    }

  /* Register read thread. */
  zclient_event (ZCLIENT_READ, zclient);

  return 0;
}

void
zclient_redistribute_set (struct zclient *zclient, int type)
{
  if (zclient->redist[type])
    return;

  zclient->redist[type] = 1;

  if (zclient->sock > 0)
    zebra_redistribute_send (ZEBRA_REDISTRIBUTE_ADD, zclient->sock, type);
}

void
zclient_redistribute_unset (struct zclient *zclient, int type)
{
  if (! zclient->redist[type])
    return;

  zclient->redist[type] = 0;

  if (zclient->sock > 0)
    zebra_redistribute_send (ZEBRA_REDISTRIBUTE_DELETE, zclient->sock, type);
}

void
zclient_redistribute_default_set (struct zclient *zclient)
{
  if (zclient->default_information)
    return;

  zclient->default_information = 1;

  if (zclient->sock > 0)
    zebra_message_send (zclient, ZEBRA_REDISTRIBUTE_DEFAULT_ADD);
}

void
zclient_redistribute_default_unset (struct zclient *zclient)
{
  if (! zclient->default_information)
    return;

  zclient->default_information = 0;

  if (zclient->sock > 0)
    zebra_message_send (zclient, ZEBRA_REDISTRIBUTE_DEFAULT_DELETE);
}

extern struct thread_master *master;

static void
zclient_event (enum event event, struct zclient *zclient)
{
  switch (event)
    {
    case ZCLIENT_SCHEDULE:
      if (! zclient->t_connect)
	zclient->t_connect =
	  thread_add_event (master, zclient_connect, zclient, 0);
      break;
    case ZCLIENT_CONNECT:
      if (zclient->fail >= 10)
	return;
      if (zclient_debug)
	zlog_info ("zclient connect schedule interval is %d", 
		   zclient->fail < 3 ? 10 : 60);
      if (! zclient->t_connect)
	zclient->t_connect = 
	  thread_add_timer (master, zclient_connect, zclient,
			    zclient->fail < 3 ? 10 : 60);
      break;
    case ZCLIENT_READ:
      zclient->t_read = 
	thread_add_read (master, zclient_read, zclient, zclient->sock);
      break;
    }
}
