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
#include "sockopt.h"
#include "memory.h"
#include "log.h"
#include "if.h"
#include "prefix.h"
#include "command.h"
#include "privs.h"
#include "linklist.h"
#include "network.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_fsm.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_debug.h"
#include "bgpd/bgp_network.h"

extern struct zebra_privs_t bgpd_privs;

/* BGP listening socket. */
struct bgp_listener
{
  int fd;
  union sockunion su;
  struct thread *thread;
};

/*
 * Set MD5 key for the socket, for the given IPv4 peer address.
 * If the password is NULL or zero-length, the option will be disabled.
 */
static int
bgp_md5_set_socket (int socket, union sockunion *su, const char *password)
{
  int ret = -1;
  int en = ENOSYS;
  
  assert (socket >= 0);
  
#if HAVE_DECL_TCP_MD5SIG  
  ret = sockopt_tcp_signature (socket, su, password);
  en  = errno;
#endif /* HAVE_TCP_MD5SIG */
  
  if (ret < 0)
    zlog (NULL, LOG_WARNING, "can't set TCP_MD5SIG option on socket %d: %s",
          socket, safe_strerror (en));

  return ret;
}

/* Helper for bgp_connect */
static int
bgp_md5_set_connect (int socket, union sockunion *su, const char *password)
{
  int ret = -1;

#if HAVE_DECL_TCP_MD5SIG  
  if ( bgpd_privs.change (ZPRIVS_RAISE) )
    {
      zlog_err ("%s: could not raise privs", __func__);
      return ret;
    }
  
  ret = bgp_md5_set_socket (socket, su, password);

  if (bgpd_privs.change (ZPRIVS_LOWER) )
    zlog_err ("%s: could not lower privs", __func__);
#endif /* HAVE_TCP_MD5SIG */
  
  return ret;
}

int
bgp_md5_set (struct peer *peer)
{
  struct listnode *node;
  int ret = 0;
  struct bgp_listener *listener;

  if ( bgpd_privs.change (ZPRIVS_RAISE) )
    {
      zlog_err ("%s: could not raise privs", __func__);
      return -1;
    }
  
  /* Just set the password on the listen socket(s). Outbound connections
   * are taken care of in bgp_connect() below.
   */
  for (ALL_LIST_ELEMENTS_RO(bm->listen_sockets, node, listener))
    if (listener->su.sa.sa_family == peer->su.sa.sa_family)
      {
	ret = bgp_md5_set_socket (listener->fd, &peer->su, peer->password);
	break;
      }

  if (bgpd_privs.change (ZPRIVS_LOWER) )
    zlog_err ("%s: could not lower privs", __func__);
  
  return ret;
}

/* Update BGP socket send buffer size */
static void
bgp_update_sock_send_buffer_size (int fd)
{
  int size = BGP_SOCKET_SNDBUF_SIZE;
  int optval;
  socklen_t optlen = sizeof(optval);

  if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &optval, &optlen) < 0)
    {
      zlog_err("getsockopt of SO_SNDBUF failed %s\n", safe_strerror(errno));
      return;
    }
  if (optval < size)
    {
      if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) < 0)
        {
          zlog_err("Couldn't increase send buffer: %s\n", safe_strerror(errno));
        }
    }
}

static void
bgp_set_socket_ttl (struct peer *peer, int bgp_sock)
{
  char buf[INET_ADDRSTRLEN];
  int ret;

  /* In case of peer is EBGP, we should set TTL for this connection.  */
  if (!peer->gtsm_hops && (peer_sort (peer) == BGP_PEER_EBGP))
    {
      ret = sockopt_ttl (peer->su.sa.sa_family, bgp_sock, peer->ttl);
      if (ret)
	{
	  zlog_err ("%s: Can't set TxTTL on peer (rtrid %s) socket, err = %d",
		    __func__,
		    inet_ntop (AF_INET, &peer->remote_id, buf, sizeof(buf)),
		    errno);
	}
    }
  else if (peer->gtsm_hops)
    {
      /* On Linux, setting minttl without setting ttl seems to mess with the
	 outgoing ttl. Therefore setting both.
      */
      ret = sockopt_ttl (peer->su.sa.sa_family, bgp_sock, MAXTTL);
      if (ret)
	{
	  zlog_err ("%s: Can't set TxTTL on peer (rtrid %s) socket, err = %d",
		    __func__,
		    inet_ntop (AF_INET, &peer->remote_id, buf, sizeof(buf)),
		    errno);
	}
      ret = sockopt_minttl (peer->su.sa.sa_family, bgp_sock,
			    MAXTTL + 1 - peer->gtsm_hops);
      if (ret)
	{
	  zlog_err ("%s: Can't set MinTTL on peer (rtrid %s) socket, err = %d",
		    __func__,
		    inet_ntop (AF_INET, &peer->remote_id, buf, sizeof(buf)),
		    errno);
	}
    }
}

/* Accept bgp connection. */
static int
bgp_accept (struct thread *thread)
{
  int bgp_sock;
  int accept_sock;
  union sockunion su;
  struct bgp_listener *listener = THREAD_ARG(thread);
  struct peer *peer;
  struct peer *peer1;
  char buf[SU_ADDRSTRLEN];

  /* Register accept thread. */
  accept_sock = THREAD_FD (thread);
  if (accept_sock < 0)
    {
      zlog_err ("accept_sock is nevative value %d", accept_sock);
      return -1;
    }
  listener->thread = thread_add_read (master, bgp_accept, listener, accept_sock);

  /* Accept client connection. */
  bgp_sock = sockunion_accept (accept_sock, &su);
  if (bgp_sock < 0)
    {
      zlog_err ("[Error] BGP socket accept failed (%s)", safe_strerror (errno));
      return -1;
    }
  set_nonblocking (bgp_sock);

  /* Set socket send buffer size */
  bgp_update_sock_send_buffer_size(bgp_sock);

  if (BGP_DEBUG (events, EVENTS))
    zlog_debug ("[Event] BGP connection from host %s", inet_sutop (&su, buf));
  
  /* Check remote IP address */
  peer1 = peer_lookup (NULL, &su);
  if (! peer1 || peer1->status == Idle)
    {
      if (BGP_DEBUG (events, EVENTS))
	{
	  if (! peer1)
	    zlog_debug ("[Event] BGP connection IP address %s is not configured",
		       inet_sutop (&su, buf));
	  else
	    zlog_debug ("[Event] BGP connection IP address %s is Idle state",
		       inet_sutop (&su, buf));
	}
      close (bgp_sock);
      return -1;
    }

  bgp_set_socket_ttl (peer1, bgp_sock);

  /* Make dummy peer until read Open packet. */
  if (BGP_DEBUG (events, EVENTS))
    zlog_debug ("[Event] Make dummy peer structure until read Open packet");

  {
    char buf[SU_ADDRSTRLEN];

    peer = peer_create_accept (peer1->bgp);
    SET_FLAG (peer->sflags, PEER_STATUS_ACCEPT_PEER);
    peer->su = su;
    peer->fd = bgp_sock;
    peer->status = Active;
    peer->local_id = peer1->local_id;
    peer->v_holdtime = peer1->v_holdtime;
    peer->v_keepalive = peer1->v_keepalive;

    /* Make peer's address string. */
    sockunion2str (&su, buf, SU_ADDRSTRLEN);
    peer->host = XSTRDUP (MTYPE_BGP_PEER_HOST, buf);
  }

  BGP_EVENT_ADD (peer, TCP_connection_open);

  return 0;
}

/* BGP socket bind. */
static int
bgp_bind (struct peer *peer)
{
#ifdef SO_BINDTODEVICE
  int ret;
  struct ifreq ifreq;

  if (! peer->ifname)
    return 0;

  strncpy ((char *)&ifreq.ifr_name, peer->ifname, sizeof (ifreq.ifr_name));

  if ( bgpd_privs.change (ZPRIVS_RAISE) )
  	zlog_err ("bgp_bind: could not raise privs");
  
  ret = setsockopt (peer->fd, SOL_SOCKET, SO_BINDTODEVICE, 
		    &ifreq, sizeof (ifreq));

  if (bgpd_privs.change (ZPRIVS_LOWER) )
    zlog_err ("bgp_bind: could not lower privs");

  if (ret < 0)
    {
      zlog (peer->log, LOG_INFO, "bind to interface %s failed", peer->ifname);
      return ret;
    }
#endif /* SO_BINDTODEVICE */
  return 0;
}

static int
bgp_update_address (struct interface *ifp, const union sockunion *dst,
		    union sockunion *addr)
{
  struct prefix *p, *sel, *d;
  struct connected *connected;
  struct listnode *node;
  int common;

  d = sockunion2hostprefix (dst);
  sel = NULL;
  common = -1;

  for (ALL_LIST_ELEMENTS_RO (ifp->connected, node, connected))
    {
      p = connected->address;
      if (p->family != d->family)
	continue;
      if (prefix_common_bits (p, d) > common)
	{
	  sel = p;
	  common = prefix_common_bits (sel, d);
	}
    }

  prefix_free (d);
  if (!sel)
    return 1;

  prefix2sockunion (sel, addr);
  return 0;
}

/* Update source selection.  */
static void
bgp_update_source (struct peer *peer)
{
  struct interface *ifp;
  union sockunion addr;

  /* Source is specified with interface name.  */
  if (peer->update_if)
    {
      ifp = if_lookup_by_name (peer->update_if);
      if (! ifp)
	return;

      if (bgp_update_address (ifp, &peer->su, &addr))
	return;

      sockunion_bind (peer->fd, &addr, 0, &addr);
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

  set_nonblocking (peer->fd);

  /* Set socket send buffer size */
  bgp_update_sock_send_buffer_size(peer->fd);

  bgp_set_socket_ttl (peer, peer->fd);

  sockopt_reuseaddr (peer->fd);
  sockopt_reuseport (peer->fd);
  
#ifdef IPTOS_PREC_INTERNETCONTROL
  if (bgpd_privs.change (ZPRIVS_RAISE))
    zlog_err ("%s: could not raise privs", __func__);
  if (sockunion_family (&peer->su) == AF_INET)
    setsockopt_ipv4_tos (peer->fd, IPTOS_PREC_INTERNETCONTROL);
# ifdef HAVE_IPV6
  else if (sockunion_family (&peer->su) == AF_INET6)
    setsockopt_ipv6_tclass (peer->fd, IPTOS_PREC_INTERNETCONTROL);
# endif
  if (bgpd_privs.change (ZPRIVS_LOWER))
    zlog_err ("%s: could not lower privs", __func__);
#endif

  if (peer->password)
    bgp_md5_set_connect (peer->fd, &peer->su, peer->password);

  /* Bind socket. */
  bgp_bind (peer);

  /* Update source bind. */
  bgp_update_source (peer);

#ifdef HAVE_IPV6
  if (peer->ifname)
    ifindex = if_nametoindex (peer->ifname);
#endif /* HAVE_IPV6 */

  if (BGP_DEBUG (events, EVENTS))
    plog_debug (peer->log, "%s [Event] Connect start to %s fd %d",
	       peer->host, peer->host, peer->fd);

  /* Connect to the remote peer. */
  return sockunion_connect (peer->fd, &peer->su, htons (peer->port), ifindex);
}

/* After TCP connection is established.  Get local address and port. */
void
bgp_getsockname (struct peer *peer)
{
  if (peer->su_local)
    {
      sockunion_free (peer->su_local);
      peer->su_local = NULL;
    }

  if (peer->su_remote)
    {
      sockunion_free (peer->su_remote);
      peer->su_remote = NULL;
    }

  peer->su_local = sockunion_getsockname (peer->fd);
  peer->su_remote = sockunion_getpeername (peer->fd);

  bgp_nexthop_set (peer->su_local, peer->su_remote, &peer->nexthop, peer);
}


static int
bgp_listener (int sock, struct sockaddr *sa, socklen_t salen)
{
  struct bgp_listener *listener;
  int ret, en;

  sockopt_reuseaddr (sock);
  sockopt_reuseport (sock);

  if (bgpd_privs.change (ZPRIVS_RAISE))
    zlog_err ("%s: could not raise privs", __func__);

#ifdef IPTOS_PREC_INTERNETCONTROL
  if (sa->sa_family == AF_INET)
    setsockopt_ipv4_tos (sock, IPTOS_PREC_INTERNETCONTROL);
#  ifdef HAVE_IPV6
  else if (sa->sa_family == AF_INET6)
    setsockopt_ipv6_tclass (sock, IPTOS_PREC_INTERNETCONTROL);
#  endif
#endif

  sockopt_v6only (sa->sa_family, sock);

  ret = bind (sock, sa, salen);
  en = errno;
  if (bgpd_privs.change (ZPRIVS_LOWER))
    zlog_err ("%s: could not lower privs", __func__);

  if (ret < 0)
    {
      zlog_err ("bind: %s", safe_strerror (en));
      return ret;
    }

  ret = listen (sock, 3);
  if (ret < 0)
    {
      zlog_err ("listen: %s", safe_strerror (errno));
      return ret;
    }

  listener = XMALLOC (MTYPE_BGP_LISTENER, sizeof(*listener));
  listener->fd = sock;
  memcpy(&listener->su, sa, salen);
  listener->thread = thread_add_read (master, bgp_accept, listener, sock);
  listnode_add (bm->listen_sockets, listener);

  return 0;
}

/* IPv6 supported version of BGP server socket setup.  */
#ifdef HAVE_IPV6
int
bgp_socket (unsigned short port, const char *address)
{
  struct addrinfo *ainfo;
  struct addrinfo *ainfo_save;
  static const struct addrinfo req = {
    .ai_family = AF_UNSPEC,
    .ai_flags = AI_PASSIVE,
    .ai_socktype = SOCK_STREAM,
  };
  int ret, count;
  char port_str[BUFSIZ];

  snprintf (port_str, sizeof(port_str), "%d", port);
  port_str[sizeof (port_str) - 1] = '\0';

  ret = getaddrinfo (address, port_str, &req, &ainfo_save);
  if (ret != 0)
    {
      zlog_err ("getaddrinfo: %s", gai_strerror (ret));
      return -1;
    }

  count = 0;
  for (ainfo = ainfo_save; ainfo; ainfo = ainfo->ai_next)
    {
      int sock;

      if (ainfo->ai_family != AF_INET && ainfo->ai_family != AF_INET6)
	continue;
     
      sock = socket (ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
      if (sock < 0)
	{
	  zlog_err ("socket: %s", safe_strerror (errno));
	  continue;
	}
	
      /* if we intend to implement ttl-security, this socket needs ttl=255 */
      sockopt_ttl (ainfo->ai_family, sock, MAXTTL);
      
      ret = bgp_listener (sock, ainfo->ai_addr, ainfo->ai_addrlen);
      if (ret == 0)
	++count;
      else
	close(sock);
    }
  freeaddrinfo (ainfo_save);
  if (count == 0)
    {
      zlog_err ("%s: no usable addresses", __func__);
      return -1;
    }

  return 0;
}
#else
/* Traditional IPv4 only version.  */
int
bgp_socket (unsigned short port, const char *address)
{
  int sock;
  int socklen;
  struct sockaddr_in sin;
  int ret, en;

  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    {
      zlog_err ("socket: %s", safe_strerror (errno));
      return sock;
    }

  /* if we intend to implement ttl-security, this socket needs ttl=255 */
  sockopt_ttl (AF_INET, sock, MAXTTL);

  memset (&sin, 0, sizeof (struct sockaddr_in));
  sin.sin_family = AF_INET;
  sin.sin_port = htons (port);
  socklen = sizeof (struct sockaddr_in);

  if (address && ((ret = inet_aton(address, &sin.sin_addr)) < 1))
    {
      zlog_err("bgp_socket: could not parse ip address %s: %s",
                address, safe_strerror (errno));
      return ret;
    }
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  sin.sin_len = socklen;
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */

  ret = bgp_listener (sock, (struct sockaddr *) &sin, socklen);
  if (ret < 0) 
    {
      close (sock);
      return ret;
    }
  return sock;
}
#endif /* HAVE_IPV6 */

void
bgp_close (void)
{
  struct listnode *node, *next;
  struct bgp_listener *listener;

  for (ALL_LIST_ELEMENTS (bm->listen_sockets, node, next, listener))
    {
      thread_cancel (listener->thread);
      close (listener->fd);
      listnode_delete (bm->listen_sockets, listener);
      XFREE (MTYPE_BGP_LISTENER, listener);
    }
}
