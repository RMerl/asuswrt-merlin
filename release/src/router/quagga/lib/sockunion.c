/* Socket union related function.
 * Copyright (c) 1997, 98 Kunihiro Ishiguro
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

#include "prefix.h"
#include "vty.h"
#include "sockunion.h"
#include "memory.h"
#include "str.h"
#include "log.h"

#ifndef HAVE_INET_ATON
int
inet_aton (const char *cp, struct in_addr *inaddr)
{
  int dots = 0;
  register u_long addr = 0;
  register u_long val = 0, base = 10;

  do
    {
      register char c = *cp;

      switch (c)
	{
	case '0': case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9':
	  val = (val * base) + (c - '0');
	  break;
	case '.':
	  if (++dots > 3)
	    return 0;
	case '\0':
	  if (val > 255)
	    return 0;
	  addr = addr << 8 | val;
	  val = 0;
	  break;
	default:
	  return 0;
	}
    } while (*cp++) ;

  if (dots < 3)
    addr <<= 8 * (3 - dots);
  if (inaddr)
    inaddr->s_addr = htonl (addr);
  return 1;
}
#endif /* ! HAVE_INET_ATON */


#ifndef HAVE_INET_PTON
int
inet_pton (int family, const char *strptr, void *addrptr)
{
  if (family == AF_INET)
    {
      struct in_addr in_val;

      if (inet_aton (strptr, &in_val))
	{
	  memcpy (addrptr, &in_val, sizeof (struct in_addr));
	  return 1;
	}
      return 0;
    }
  errno = EAFNOSUPPORT;
  return -1;
}
#endif /* ! HAVE_INET_PTON */

#ifndef HAVE_INET_NTOP
const char *
inet_ntop (int family, const void *addrptr, char *strptr, size_t len)
{
  unsigned char *p = (unsigned char *) addrptr;

  if (family == AF_INET) 
    {
      char temp[INET_ADDRSTRLEN];

      snprintf(temp, sizeof(temp), "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);

      if (strlen(temp) >= len) 
	{
	  errno = ENOSPC;
	  return NULL;
	}
      strcpy(strptr, temp);
      return strptr;
    }

  errno = EAFNOSUPPORT;
  return NULL;
}
#endif /* ! HAVE_INET_NTOP */

const char *
inet_sutop (union sockunion *su, char *str)
{
  switch (su->sa.sa_family)
    {
    case AF_INET:
      inet_ntop (AF_INET, &su->sin.sin_addr, str, INET_ADDRSTRLEN);
      break;
#ifdef HAVE_IPV6
    case AF_INET6:
      inet_ntop (AF_INET6, &su->sin6.sin6_addr, str, INET6_ADDRSTRLEN);
      break;
#endif /* HAVE_IPV6 */
    }
  return str;
}

int
str2sockunion (const char *str, union sockunion *su)
{
  int ret;

  memset (su, 0, sizeof (union sockunion));

  ret = inet_pton (AF_INET, str, &su->sin.sin_addr);
  if (ret > 0)			/* Valid IPv4 address format. */
    {
      su->sin.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
      su->sin.sin_len = sizeof(struct sockaddr_in);
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */
      return 0;
    }
#ifdef HAVE_IPV6
  ret = inet_pton (AF_INET6, str, &su->sin6.sin6_addr);
  if (ret > 0)			/* Valid IPv6 address format. */
    {
      su->sin6.sin6_family = AF_INET6;
#ifdef SIN6_LEN
      su->sin6.sin6_len = sizeof(struct sockaddr_in6);
#endif /* SIN6_LEN */
      return 0;
    }
#endif /* HAVE_IPV6 */
  return -1;
}

const char *
sockunion2str (union sockunion *su, char *buf, size_t len)
{
  if  (su->sa.sa_family == AF_INET)
    return inet_ntop (AF_INET, &su->sin.sin_addr, buf, len);
#ifdef HAVE_IPV6
  else if (su->sa.sa_family == AF_INET6)
    return inet_ntop (AF_INET6, &su->sin6.sin6_addr, buf, len);
#endif /* HAVE_IPV6 */
  return NULL;
}

union sockunion *
sockunion_str2su (const char *str)
{
  union sockunion *su = XCALLOC (MTYPE_SOCKUNION, sizeof (union sockunion));
  
  if (!str2sockunion (str, su))
    return su;
  
  XFREE (MTYPE_SOCKUNION, su);
  return NULL;
}

/* Convert IPv4 compatible IPv6 address to IPv4 address. */
static void
sockunion_normalise_mapped (union sockunion *su)
{
  struct sockaddr_in sin;
  
#ifdef HAVE_IPV6
  if (su->sa.sa_family == AF_INET6 
      && IN6_IS_ADDR_V4MAPPED (&su->sin6.sin6_addr))
    {
      memset (&sin, 0, sizeof (struct sockaddr_in));
      sin.sin_family = AF_INET;
      sin.sin_port = su->sin6.sin6_port;
      memcpy (&sin.sin_addr, ((char *)&su->sin6.sin6_addr) + 12, 4);
      memcpy (su, &sin, sizeof (struct sockaddr_in));
    }
#endif /* HAVE_IPV6 */
}

/* Return socket of sockunion. */
int
sockunion_socket (union sockunion *su)
{
  int sock;

  sock = socket (su->sa.sa_family, SOCK_STREAM, 0);
  if (sock < 0)
    {
      zlog (NULL, LOG_WARNING, "Can't make socket : %s", safe_strerror (errno));
      return -1;
    }

  return sock;
}

/* Return accepted new socket file descriptor. */
int
sockunion_accept (int sock, union sockunion *su)
{
  socklen_t len;
  int client_sock;

  len = sizeof (union sockunion);
  client_sock = accept (sock, (struct sockaddr *) su, &len);
  
  sockunion_normalise_mapped (su);
  return client_sock;
}

/* Return sizeof union sockunion.  */
static int
sockunion_sizeof (union sockunion *su)
{
  int ret;

  ret = 0;
  switch (su->sa.sa_family)
    {
    case AF_INET:
      ret = sizeof (struct sockaddr_in);
      break;
#ifdef HAVE_IPV6
    case AF_INET6:
      ret = sizeof (struct sockaddr_in6);
      break;
#endif /* AF_INET6 */
    }
  return ret;
}

/* return sockunion structure : this function should be revised. */
static const char *
sockunion_log (union sockunion *su, char *buf, size_t len)
{
  switch (su->sa.sa_family) 
    {
    case AF_INET:
      return inet_ntop(AF_INET, &su->sin.sin_addr, buf, len);

#ifdef HAVE_IPV6
    case AF_INET6:
      return inet_ntop(AF_INET6, &(su->sin6.sin6_addr), buf, len);
      break;
#endif /* HAVE_IPV6 */

    default:
      snprintf (buf, len, "af_unknown %d ", su->sa.sa_family);
      return buf;
    }
}

/* sockunion_connect returns
   -1 : error occured
   0 : connect success
   1 : connect is in progress */
enum connect_result
sockunion_connect (int fd, union sockunion *peersu, unsigned short port,
		   unsigned int ifindex)
{
  int ret;
  int val;
  union sockunion su;

  memcpy (&su, peersu, sizeof (union sockunion));

  switch (su.sa.sa_family)
    {
    case AF_INET:
      su.sin.sin_port = port;
      break;
#ifdef HAVE_IPV6
    case AF_INET6:
      su.sin6.sin6_port  = port;
#ifdef KAME
      if (IN6_IS_ADDR_LINKLOCAL(&su.sin6.sin6_addr) && ifindex)
	{
#ifdef HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID
	  /* su.sin6.sin6_scope_id = ifindex; */
#endif /* HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID */
	  SET_IN6_LINKLOCAL_IFINDEX (su.sin6.sin6_addr, ifindex);
	}
#endif /* KAME */
      break;
#endif /* HAVE_IPV6 */
    }      

  /* Make socket non-block. */
  val = fcntl (fd, F_GETFL, 0);
  fcntl (fd, F_SETFL, val|O_NONBLOCK);

  /* Call connect function. */
  ret = connect (fd, (struct sockaddr *) &su, sockunion_sizeof (&su));

  /* Immediate success */
  if (ret == 0)
    {
      fcntl (fd, F_SETFL, val);
      return connect_success;
    }

  /* If connect is in progress then return 1 else it's real error. */
  if (ret < 0)
    {
      if (errno != EINPROGRESS)
	{
	  char str[SU_ADDRSTRLEN];
	  zlog_info ("can't connect to %s fd %d : %s",
		     sockunion_log (&su, str, sizeof str),
		     fd, safe_strerror (errno));
	  return connect_error;
	}
    }

  fcntl (fd, F_SETFL, val);

  return connect_in_progress;
}

/* Make socket from sockunion union. */
int
sockunion_stream_socket (union sockunion *su)
{
  int sock;

  if (su->sa.sa_family == 0)
    su->sa.sa_family = AF_INET_UNION;

  sock = socket (su->sa.sa_family, SOCK_STREAM, 0);

  if (sock < 0)
    zlog (NULL, LOG_WARNING, "can't make socket sockunion_stream_socket");

  return sock;
}

/* Bind socket to specified address. */
int
sockunion_bind (int sock, union sockunion *su, unsigned short port, 
		union sockunion *su_addr)
{
  int size = 0;
  int ret;

  if (su->sa.sa_family == AF_INET)
    {
      size = sizeof (struct sockaddr_in);
      su->sin.sin_port = htons (port);
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
      su->sin.sin_len = size;
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */
      if (su_addr == NULL)
	sockunion2ip (su) = htonl (INADDR_ANY);
    }
#ifdef HAVE_IPV6
  else if (su->sa.sa_family == AF_INET6)
    {
      size = sizeof (struct sockaddr_in6);
      su->sin6.sin6_port = htons (port);
#ifdef SIN6_LEN
      su->sin6.sin6_len = size;
#endif /* SIN6_LEN */
      if (su_addr == NULL)
	{
#ifdef LINUX_IPV6
	  memset (&su->sin6.sin6_addr, 0, sizeof (struct in6_addr));
#else
	  su->sin6.sin6_addr = in6addr_any;
#endif /* LINUX_IPV6 */
	}
    }
#endif /* HAVE_IPV6 */
  

  ret = bind (sock, (struct sockaddr *)su, size);
  if (ret < 0)
    zlog (NULL, LOG_WARNING, "can't bind socket : %s", safe_strerror (errno));

  return ret;
}

int
sockopt_reuseaddr (int sock)
{
  int ret;
  int on = 1;

  ret = setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, 
		    (void *) &on, sizeof (on));
  if (ret < 0)
    {
      zlog (NULL, LOG_WARNING, "can't set sockopt SO_REUSEADDR to socket %d", sock);
      return -1;
    }
  return 0;
}

#ifdef SO_REUSEPORT
int
sockopt_reuseport (int sock)
{
  int ret;
  int on = 1;

  ret = setsockopt (sock, SOL_SOCKET, SO_REUSEPORT, 
		    (void *) &on, sizeof (on));
  if (ret < 0)
    {
      zlog (NULL, LOG_WARNING, "can't set sockopt SO_REUSEPORT to socket %d", sock);
      return -1;
    }
  return 0;
}
#else
int
sockopt_reuseport (int sock)
{
  return 0;
}
#endif /* 0 */

int
sockopt_ttl (int family, int sock, int ttl)
{
  int ret;

#ifdef IP_TTL
  if (family == AF_INET)
    {
      ret = setsockopt (sock, IPPROTO_IP, IP_TTL, 
			(void *) &ttl, sizeof (int));
      if (ret < 0)
	{
	  zlog (NULL, LOG_WARNING, "can't set sockopt IP_TTL %d to socket %d", ttl, sock);
	  return -1;
	}
      return 0;
    }
#endif /* IP_TTL */
#ifdef HAVE_IPV6
  if (family == AF_INET6)
    {
      ret = setsockopt (sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, 
			(void *) &ttl, sizeof (int));
      if (ret < 0)
	{
	  zlog (NULL, LOG_WARNING, "can't set sockopt IPV6_UNICAST_HOPS %d to socket %d",
		    ttl, sock);
	  return -1;
	}
      return 0;
    }
#endif /* HAVE_IPV6 */
  return 0;
}

int
sockopt_cork (int sock, int onoff)
{
#ifdef TCP_CORK
  return setsockopt (sock, IPPROTO_TCP, TCP_CORK, &onoff, sizeof(onoff));
#else
  return 0;
#endif
}

int
sockopt_minttl (int family, int sock, int minttl)
{
#ifdef IP_MINTTL
  if (family == AF_INET)
    {
      int ret = setsockopt (sock, IPPROTO_IP, IP_MINTTL, &minttl, sizeof(minttl));
      if (ret < 0)
	  zlog (NULL, LOG_WARNING,
		"can't set sockopt IP_MINTTL to %d on socket %d: %s",
		minttl, sock, safe_strerror (errno));
      return ret;
    }
#endif /* IP_MINTTL */
#ifdef IPV6_MINHOPCNT
  if (family == AF_INET6)
    {
      int ret = setsockopt (sock, IPPROTO_IPV6, IPV6_MINHOPCNT, &minttl, sizeof(minttl));
      if (ret < 0)
	  zlog (NULL, LOG_WARNING,
		"can't set sockopt IPV6_MINHOPCNT to %d on socket %d: %s",
		minttl, sock, safe_strerror (errno));
      return ret;
    }
#endif

  errno = EOPNOTSUPP;
  return -1;
}

int
sockopt_v6only (int family, int sock)
{
  int ret, on = 1;

#ifdef HAVE_IPV6
#ifdef IPV6_V6ONLY
  if (family == AF_INET6)
    {
      ret = setsockopt (sock, IPPROTO_IPV6, IPV6_V6ONLY,
			(void *) &on, sizeof (int));
      if (ret < 0)
	{
	  zlog (NULL, LOG_WARNING, "can't set sockopt IPV6_V6ONLY "
		    "to socket %d", sock);
	  return -1;
	}
      return 0;
    }
#endif /* IPV6_V6ONLY */
#endif /* HAVE_IPV6 */
  return 0;
}

/* If same family and same prefix return 1. */
int
sockunion_same (union sockunion *su1, union sockunion *su2)
{
  int ret = 0;

  if (su1->sa.sa_family != su2->sa.sa_family)
    return 0;

  switch (su1->sa.sa_family)
    {
    case AF_INET:
      ret = memcmp (&su1->sin.sin_addr, &su2->sin.sin_addr,
		    sizeof (struct in_addr));
      break;
#ifdef HAVE_IPV6
    case AF_INET6:
      ret = memcmp (&su1->sin6.sin6_addr, &su2->sin6.sin6_addr,
		    sizeof (struct in6_addr));
      break;
#endif /* HAVE_IPV6 */
    }
  if (ret == 0)
    return 1;
  else
    return 0;
}

/* After TCP connection is established.  Get local address and port. */
union sockunion *
sockunion_getsockname (int fd)
{
  int ret;
  socklen_t len;
  union
  {
    struct sockaddr sa;
    struct sockaddr_in sin;
#ifdef HAVE_IPV6
    struct sockaddr_in6 sin6;
#endif /* HAVE_IPV6 */
    char tmp_buffer[128];
  } name;
  union sockunion *su;

  memset (&name, 0, sizeof name);
  len = sizeof name;

  ret = getsockname (fd, (struct sockaddr *)&name, &len);
  if (ret < 0)
    {
      zlog_warn ("Can't get local address and port by getsockname: %s",
		 safe_strerror (errno));
      return NULL;
    }

  if (name.sa.sa_family == AF_INET)
    {
      su = XCALLOC (MTYPE_SOCKUNION, sizeof (union sockunion));
      memcpy (su, &name, sizeof (struct sockaddr_in));
      return su;
    }
#ifdef HAVE_IPV6
  if (name.sa.sa_family == AF_INET6)
    {
      su = XCALLOC (MTYPE_SOCKUNION, sizeof (union sockunion));
      memcpy (su, &name, sizeof (struct sockaddr_in6));
      sockunion_normalise_mapped (su);
      return su;
    }
#endif /* HAVE_IPV6 */
  return NULL;
}

/* After TCP connection is established.  Get remote address and port. */
union sockunion *
sockunion_getpeername (int fd)
{
  int ret;
  socklen_t len;
  union
  {
    struct sockaddr sa;
    struct sockaddr_in sin;
#ifdef HAVE_IPV6
    struct sockaddr_in6 sin6;
#endif /* HAVE_IPV6 */
    char tmp_buffer[128];
  } name;
  union sockunion *su;

  memset (&name, 0, sizeof name);
  len = sizeof name;
  ret = getpeername (fd, (struct sockaddr *)&name, &len);
  if (ret < 0)
    {
      zlog (NULL, LOG_WARNING, "Can't get remote address and port: %s",
	    safe_strerror (errno));
      return NULL;
    }

  if (name.sa.sa_family == AF_INET)
    {
      su = XCALLOC (MTYPE_SOCKUNION, sizeof (union sockunion));
      memcpy (su, &name, sizeof (struct sockaddr_in));
      return su;
    }
#ifdef HAVE_IPV6
  if (name.sa.sa_family == AF_INET6)
    {
      su = XCALLOC (MTYPE_SOCKUNION, sizeof (union sockunion));
      memcpy (su, &name, sizeof (struct sockaddr_in6));
      sockunion_normalise_mapped (su);
      return su;
    }
#endif /* HAVE_IPV6 */
  return NULL;
}

/* Print sockunion structure */
static void __attribute__ ((unused))
sockunion_print (union sockunion *su)
{
  if (su == NULL)
    return;

  switch (su->sa.sa_family) 
    {
    case AF_INET:
      printf ("%s\n", inet_ntoa (su->sin.sin_addr));
      break;
#ifdef HAVE_IPV6
    case AF_INET6:
      {
	char buf [SU_ADDRSTRLEN];

	printf ("%s\n", inet_ntop (AF_INET6, &(su->sin6.sin6_addr),
				 buf, sizeof (buf)));
      }
      break;
#endif /* HAVE_IPV6 */

#ifdef AF_LINK
    case AF_LINK:
      {
	struct sockaddr_dl *sdl;

	sdl = (struct sockaddr_dl *)&(su->sa);
	printf ("link#%d\n", sdl->sdl_index);
      }
      break;
#endif /* AF_LINK */
    default:
      printf ("af_unknown %d\n", su->sa.sa_family);
      break;
    }
}

#ifdef HAVE_IPV6
static int
in6addr_cmp (struct in6_addr *addr1, struct in6_addr *addr2)
{
  unsigned int i;
  u_char *p1, *p2;

  p1 = (u_char *)addr1;
  p2 = (u_char *)addr2;

  for (i = 0; i < sizeof (struct in6_addr); i++)
    {
      if (p1[i] > p2[i])
	return 1;
      else if (p1[i] < p2[i])
	return -1;
    }
  return 0;
}
#endif /* HAVE_IPV6 */

int
sockunion_cmp (union sockunion *su1, union sockunion *su2)
{
  if (su1->sa.sa_family > su2->sa.sa_family)
    return 1;
  if (su1->sa.sa_family < su2->sa.sa_family)
    return -1;

  if (su1->sa.sa_family == AF_INET)
    {
      if (ntohl (sockunion2ip (su1)) == ntohl (sockunion2ip (su2)))
	return 0;
      if (ntohl (sockunion2ip (su1)) > ntohl (sockunion2ip (su2)))
	return 1;
      else
	return -1;
    }
#ifdef HAVE_IPV6
  if (su1->sa.sa_family == AF_INET6)
    return in6addr_cmp (&su1->sin6.sin6_addr, &su2->sin6.sin6_addr);
#endif /* HAVE_IPV6 */
  return 0;
}

/* Duplicate sockunion. */
union sockunion *
sockunion_dup (union sockunion *su)
{
  union sockunion *dup = XCALLOC (MTYPE_SOCKUNION, sizeof (union sockunion));
  memcpy (dup, su, sizeof (union sockunion));
  return dup;
}

void
sockunion_free (union sockunion *su)
{
  XFREE (MTYPE_SOCKUNION, su);
}
