/* Copyright (C) 2002 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <errno.h>

#ifdef _LIBC
# include <libintl.h>
#else
# ifndef _
#  define _(s)			(s)
# endif
# define __socket(d, t, p)	socket ((d), (t), (p))
# define __close(f)		close ((f))
#endif

static int
svc_socket (u_long number, int type, int protocol, int reuse)
{
  struct sockaddr_in addr;
  socklen_t len = sizeof (struct sockaddr_in);
#ifdef __UCLIBC_HAS_REENTRANT_RPC__
  char rpcdata [1024], servdata [1024];
  struct rpcent rpcbuf, *rpcp;
  struct servent servbuf, *servp = NULL;
#else
  struct rpcent *rpcp;
  struct servent *servp;
#endif
  int sock, ret;
  const char *proto = protocol == IPPROTO_TCP ? "tcp" : "udp";

  if ((sock = __socket (AF_INET, type, protocol)) < 0)
    {
      perror (_("svc_socket: socket creation problem"));
      return sock;
    }

  if (reuse)
    {
      ret = 1;
      ret = setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &ret,
			sizeof (ret));
      if (ret < 0)
	{
	  perror (_("svc_socket: socket reuse problem"));
	  return ret;
	}
    }

  memset (&addr, 0, sizeof (addr));
  addr.sin_family = AF_INET;

#ifdef __UCLIBC_HAS_REENTRANT_RPC__
  ret = getrpcbynumber_r (number, &rpcbuf, rpcdata, sizeof rpcdata,
			  &rpcp);
#else
  ret = ((rpcp = getrpcbynumber(number)) == NULL);
#endif
  if (ret == 0 && rpcp != NULL)
    {
      /* First try name.  */
#ifdef __UCLIBC_HAS_REENTRANT_RPC__
      ret = getservbyname_r (rpcp->r_name, proto, &servbuf, servdata,
			     sizeof servdata, &servp);
#else
      ret = ((servp = getservbyname(rpcp->r_name, proto)) == NULL);
#endif
      if ((ret != 0 || servp == NULL) && rpcp->r_aliases)
	{
	  const char **a;

	  /* Then we try aliases.  */
	  for (a = (const char **) rpcp->r_aliases; *a != NULL; a++) 
	    {
#ifdef __UCLIBC_HAS_REENTRANT_RPC__
	      ret = getservbyname_r (*a, proto, &servbuf, servdata,
				     sizeof servdata, &servp);
#else
	      ret = ((servp = getservbyname(*a, proto)) == NULL);
#endif
	      if (ret == 0 && servp != NULL)
		break;
	    }
	}
    }

  if (ret == 0 && servp != NULL)
    {
      addr.sin_port = servp->s_port;
      if (bind (sock, (struct sockaddr *) &addr, len) < 0)
	{
	  perror (_("svc_socket: bind problem"));
	  (void) __close (sock);
	  sock = -1;
	}
    }
  else
    {
	  addr.sin_port = 0;
	  if (bind (sock, (struct sockaddr *) &addr, len) < 0)
	    {
	      perror (_("svc_socket: bind problem"));
	      (void) __close (sock);
	      sock = -1;
	    }
    }

  if (sock >= 0)
    {
	    /* This socket might be shared among multiple processes
	     * if mountd is run multi-threaded.  So it is safest to
	     * make it non-blocking, else all threads might wake
	     * one will get the data, and the others will block
	     * indefinitely.
	     * In all cases, transaction on this socket are atomic
	     * (accept for TCP, packet-read and packet-write for UDP)
	     * so O_NONBLOCK will not confuse unprepared code causing
	     * it to corrupt messages.
	     * It generally safest to have O_NONBLOCK when doing an accept
	     * as if we get a RST after the SYN and before accept runs,
	     * we can block despite being told there was an acceptable
	     * connection.
	     */
	int flags;
	if ((flags = fcntl(sock, F_GETFL)) < 0)
	  {
	      perror (_("svc_socket: can't get socket flags"));
	      (void) __close (sock);
	      sock = -1;
	  }
	else if (fcntl(sock, F_SETFL, flags|O_NONBLOCK) < 0)
	  {
	      perror (_("svc_socket: can't set socket flags"));
	      (void) __close (sock);
	      sock = -1;
	  }
    }

  return sock;
}

/*
 * Create and bind a TCP socket based on program number
 */
int
svctcp_socket (u_long number, int reuse)
{
  return svc_socket (number, SOCK_STREAM, IPPROTO_TCP, reuse);
}

/*
 * Create and bind a UDP socket based on program number
 */
int
svcudp_socket (u_long number, int reuse)
{
  return svc_socket (number, SOCK_DGRAM, IPPROTO_UDP, 0);
}

#ifdef TEST
static int
check (u_long number, u_short port, int protocol, int reuse)
{
  int socket;
  int result;
  struct sockaddr_in addr;
  socklen_t len = sizeof (struct sockaddr_in);

  if (protocol == IPPROTO_TCP)
    socket = svctcp_socket (number, reuse);
  else
    socket = svcudp_socket (number, reuse);

  if (socket < 0)
    return 1;

  result = getsockname (socket, (struct sockaddr *) &addr, &len);
  if (result == 0)
    {
      if (port != 0 && ntohs (addr.sin_port) != port)
	printf ("Program: %ld, expect port: %d, got: %d\n",
		number, port, ntohs (addr.sin_port)); 
      else
	printf ("Program: %ld, port: %d\n",
		number, ntohs (addr.sin_port)); 
    }

  close (socket);
  return result;
}

int
main (void)
{
  int result = 0;

  result += check (100001, 0, IPPROTO_TCP, 0);
  result += check (100001, 0, IPPROTO_UDP, 0);
  result += check (100003, 2049, IPPROTO_TCP, 1);
  result += check (100003, 2049, IPPROTO_UDP, 1);

  return result;
}
#endif
