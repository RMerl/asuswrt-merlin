/* BGP network related fucntions
   Copyright (C) 1999 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <zebra.h>

#include "thread.h"
#include "sockunion.h"
#include "memory.h"
#include "log.h"
#include "if.h"
#include "prefix.h"
#include "command.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_fsm.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_debug.h"
#include "bgpd/bgp_network.h"

#ifdef HAVE_TCP_SIGNATURE
#include "bgpd/bgp_tcpsig.h"
#endif /* HAVE_TCP_SIGNATURE */


/* Accept bgp connection. */
static int
bgp_accept (struct thread *thread)
{
  int bgp_sock;
  int accept_sock;
  union sockunion su;
  struct peer *peer;
  struct peer *peer1;
  struct bgp *bgp;
  char buf[SU_ADDRSTRLEN];

  /* Regiser accept thread. */
  accept_sock = THREAD_FD (thread);
  bgp = THREAD_ARG (thread);

  if (accept_sock < 0)
    {
      zlog_err ("accept_sock is nevative value %d", accept_sock);
      return -1;
    }
  thread_add_read (master, bgp_accept, bgp, accept_sock);

  /* Accept client connection. */
  bgp_sock = sockunion_accept (accept_sock, &su);
  if (bgp_sock < 0)
    {
      zlog_err ("[Error] BGP socket accept failed (%s)", strerror (errno));
      return -1;
    }

  if (BGP_DEBUG (events, EVENTS))
    zlog_info ("[Event] BGP connection from host %s", inet_sutop (&su, buf));
  
  /* Check remote IP address */
  peer1 = peer_lookup (bgp, &su);
  if (! peer1 || peer1->status == Idle)
    {
      if (BGP_DEBUG (events, EVENTS))
	{
	  if (! peer1)
	    zlog_info ("[Event] BGP connection IP address %s is not configured",
		       inet_sutop (&su, buf));
	  else
	    zlog_info ("[Event] BGP connection IP address %s is Idle state",
		       inet_sutop (&su, buf));
	}
      close (bgp_sock);
      return -1;
    }

  /* In case of peer is EBGP, we should set TTL for this connection.  */
  if (peer_sort (peer1) == BGP_PEER_EBGP)
    sockopt_ttl (peer1->su.sa.sa_family, bgp_sock, peer1->ttl);

  if (! bgp)
    bgp = peer1->bgp;

  /* Make dummy peer until read Open packet. */
  if (BGP_DEBUG (events, EVENTS))
    zlog_info ("[Event] Make dummy peer structure until read Open packet");

  {
    char buf[SU_ADDRSTRLEN + 1];

    peer = peer_create_accept (bgp);
    SET_FLAG (peer->sflags, PEER_STATUS_ACCEPT_PEER);
    peer->su = su;
    peer->fd = bgp_sock;
    peer->status = Active;
    peer->local_id = peer1->local_id;

    /* Make peer's address string. */
    sockunion2str (&su, buf, SU_ADDRSTRLEN);
    peer->host = strdup (buf);
  }

  BGP_EVENT_ADD (peer, TCP_connection_open);

  return 0;
}

/* BGP socket bind. */
int
bgp_bind (struct peer *peer)
{
#ifdef SO_BINDTODEVICE
  int ret;
  struct ifreq ifreq;

  if (! peer->ifname)
    return 0;

  strncpy ((char *)&ifreq.ifr_name, peer->ifname, sizeof (ifreq.ifr_name));

  ret = setsockopt (peer->fd, SOL_SOCKET, SO_BINDTODEVICE, 
		    &ifreq, sizeof (ifreq));
  if (ret < 0)
    {
      zlog (peer->log, LOG_INFO, "bind to interface %s failed", peer->ifname);
      return ret;
    }
#endif /* SO_BINDTODEVICE */
  return 0;
}

int
bgp_bind_address (int sock, struct in_addr *addr)
{
  int ret;
  struct sockaddr_in local;

  memset (&local, 0, sizeof (struct sockaddr_in));
  local.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
  local.sin_len = sizeof(struct sockaddr_in);
#endif /* HAVE_SIN_LEN */
  memcpy (&local.sin_addr, addr, sizeof (struct in_addr));

  ret = bind (sock, (struct sockaddr *)&local, sizeof (struct sockaddr_in));
  if (ret < 0)
    ;
  return 0;
}

struct in_addr *
bgp_update_address (struct interface *ifp)
{
  struct prefix_ipv4 *p;
  struct connected *connected;
  listnode node;

  for (node = listhead (ifp->connected); node; nextnode (node))
    {
      connected = getdata (node);

      p = (struct prefix_ipv4 *) connected->address;

      if (p->family == AF_INET)
	return &p->prefix;
    }
  return NULL;
}

/* Update source selection.  */
void
bgp_update_source (struct peer *peer)
{
  struct interface *ifp;
  struct in_addr *addr;

  /* Source is specified with interface name.  */
  if (peer->update_if)
    {
      ifp = if_lookup_by_name (peer->update_if);
      if (! ifp)
	return;

      addr = bgp_update_address (ifp);
      if (! addr)
	return;

      bgp_bind_address (peer->fd, addr);
    }

  /* Source is specified with IP address.  */
  if (peer->update_source)
    sockunion_bind (peer->fd, peer->update_source, 0, peer->update_source);
}

/* BGP try to connect to the peer.  */
int
bgp_connect (struct peer *peer)
{
  unsigned int ifindex = 0;

  /* Make socket for the peer. */
  peer->fd = sockunion_socket (&peer->su);
  if (peer->fd < 0)
    return -1;

  /* If we can get socket for the peer, adjest TTL and make connection. */
  if (peer_sort (peer) == BGP_PEER_EBGP)
    sockopt_ttl (peer->su.sa.sa_family, peer->fd, peer->ttl);

  sockopt_reuseaddr (peer->fd);
  sockopt_reuseport (peer->fd);

  /* Bind socket. */
  bgp_bind (peer);

  /* Update source bind. */
  bgp_update_source (peer);

#ifdef HAVE_IPV6
  if (peer->ifname)
    ifindex = if_nametoindex (peer->ifname);
#endif /* HAVE_IPV6 */

  if (BGP_DEBUG (events, EVENTS))
    plog_info (peer->log, "%s [Event] Connect start to %s fd %d",
	       peer->host, peer->host, peer->fd);

#ifdef HAVE_TCP_SIGNATURE
  if (CHECK_FLAG (peer->flags, PEER_FLAG_PASSWORD))
    bgp_tcpsig_set (peer->fd, peer);
#endif /* HAVE_TCP_SIGNATURE */

  /* Connect to the remote peer. */
  return sockunion_connect (peer->fd, &peer->su, htons (peer->port), ifindex);
}

/* After TCP connection is established.  Get local address and port. */
void
bgp_getsockname (struct peer *peer)
{
  if (peer->su_local)
    {
      XFREE (MTYPE_TMP, peer->su_local);
      peer->su_local = NULL;
    }

  if (peer->su_remote)
    {
      XFREE (MTYPE_TMP, peer->su_remote);
      peer->su_remote = NULL;
    }

  peer->su_local = sockunion_getsockname (peer->fd);
  peer->su_remote = sockunion_getpeername (peer->fd);

  bgp_nexthop_set (peer->su_local, peer->su_remote, &peer->nexthop, peer);
}

/* IPv6 supported version of BGP server socket setup.  */
#if defined (HAVE_IPV6) && ! defined (NRL)
int
bgp_socket (struct bgp *bgp, unsigned short port)
{
  int ret;
  struct addrinfo req;
  struct addrinfo *ainfo;
  struct addrinfo *ainfo_save;
  int sock = 0;
  char port_str[BUFSIZ];

  memset (&req, 0, sizeof (struct addrinfo));

  req.ai_flags = AI_PASSIVE;
  req.ai_family = AF_UNSPEC;
  req.ai_socktype = SOCK_STREAM;
  sprintf (port_str, "%d", port);
  port_str[sizeof (port_str) - 1] = '\0';

  ret = getaddrinfo (NULL, port_str, &req, &ainfo);
  if (ret != 0)
    {
      zlog_err ("getaddrinfo: %s", gai_strerror (ret));
      return -1;
    }

  ainfo_save = ainfo;

  do
    {
      if (ainfo->ai_family != AF_INET && ainfo->ai_family != AF_INET6)
	continue;
     
      sock = socket (ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
      if (sock < 0)
	{
	  zlog_err ("socket: %s", strerror (errno));
	  continue;
	}

      sockopt_reuseaddr (sock);
      sockopt_reuseport (sock);

      ret = bind (sock, ainfo->ai_addr, ainfo->ai_addrlen);
      if (ret < 0)
	{
	  zlog_err ("bind: %s", strerror (errno));
	  close (sock);
	  continue;
	}
      ret = listen (sock, 3);
      if (ret < 0) 
	{
	  zlog_err ("listen: %s", strerror (errno));
	  close (sock);
	  continue;
	}

#ifdef HAVE_TCP_SIGNATURE
#ifdef HAVE_LINUX_TCP_SIGNATURE
      bm->sock = sock;
#endif /* HAVE_LINUX_TCP_SIGNATURE */
#ifdef HAVE_OPENBSD_TCP_SIGNATURE
      bgp_tcpsig_set (sock, 0);
      bm->sock = -1;
#endif /* HAVE_OPENBSD_TCP_SIGNATURE */
#endif /* HAVE_TCP_SIGNATURE */

      thread_add_read (master, bgp_accept, bgp, sock);
    }
  while ((ainfo = ainfo->ai_next) != NULL);

  freeaddrinfo (ainfo_save);

  return sock;
}
#else
/* Traditional IPv4 only version.  */
int
bgp_socket (struct bgp *bgp, unsigned short port)
{
  int sock;
  int socklen;
  struct sockaddr_in sin;
  int ret;

  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    {
      zlog_err ("socket: %s", strerror (errno));
      return sock;
    }

  sockopt_reuseaddr (sock);
  sockopt_reuseport (sock);

  memset (&sin, 0, sizeof (struct sockaddr_in));

  sin.sin_family = AF_INET;
  sin.sin_port = htons (port);
  socklen = sizeof (struct sockaddr_in);
#ifdef HAVE_SIN_LEN
  sin.sin_len = socklen;
#endif /* HAVE_SIN_LEN */

  ret = bind (sock, (struct sockaddr *) &sin, socklen);
  if (ret < 0)
    {
      zlog_err ("bind: %s", strerror (errno));
      close (sock);
      return ret;
    }
  ret = listen (sock, 3);
  if (ret < 0) 
    {
      zlog_err ("listen: %s", strerror (errno));
      close (sock);
      return ret;
    }
#ifdef HAVE_TCP_SIGNATURE
#ifdef HAVE_LINUX_TCP_SIGNATURE
  bm->sock = sock;
#endif /* HAVE_LINUX_TCP_SIGNATURE */
#ifdef HAVE_OPENBSD_TCP_SIGNATURE
  bgp_tcpsig_set (sock, 0);
  bm->sock = -1;
#endif /* HAVE_OPENBSD_TCP_SIGNATURE */
#endif /* HAVE_TCP_SIGNATURE */

  thread_add_read (bm->master, bgp_accept, bgp, sock);

  return sock;
}
#endif /* HAVE_IPV6 && !NRL */
