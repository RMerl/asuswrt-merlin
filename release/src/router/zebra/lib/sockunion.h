/*
 * Socket union header.
 * Copyright (c) 1997 Kunihiro Ishiguro
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

#ifndef _ZEBRA_SOCKUNION_H
#define _ZEBRA_SOCKUNION_H

#if 0
union sockunion {
  struct sockinet {
    u_char si_len;
    u_char si_family;
    u_short si_port;
  } su_si;
  struct sockaddr_in  su_sin;
  struct sockaddr_in6 su_sin6;
};
#define su_len                su_si.si_len
#define su_family     su_si.si_family
#define su_port               su_si.si_port
#endif /* 0 */

union sockunion 
{
  struct sockaddr sa;
  struct sockaddr_in sin;
#ifdef HAVE_IPV6
  struct sockaddr_in6 sin6;
#endif /* HAVE_IPV6 */
};

enum connect_result
{
  connect_error,
  connect_success,
  connect_in_progress
};

/* Default address family. */
#ifdef HAVE_IPV6
#define AF_INET_UNION AF_INET6
#else
#define AF_INET_UNION AF_INET
#endif

/* Sockunion address string length.  Same as INET6_ADDRSTRLEN. */
#define SU_ADDRSTRLEN 46

/* Macro to set link local index to the IPv6 address.  For KAME IPv6
   stack. */
#ifdef KAME
#define	IN6_LINKLOCAL_IFINDEX(a)  ((a).s6_addr[2] << 8 | (a).s6_addr[3])
#define SET_IN6_LINKLOCAL_IFINDEX(a, i) \
  do { \
    (a).s6_addr[2] = ((i) >> 8) & 0xff; \
    (a).s6_addr[3] = (i) & 0xff; \
  } while (0)
#else
#define	IN6_LINKLOCAL_IFINDEX(a)
#define SET_IN6_LINKLOCAL_IFINDEX(a, i)
#endif /* KAME */

/* shortcut macro to specify address field of struct sockaddr */
#define sock2ip(X)   (((struct sockaddr_in *)(X))->sin_addr.s_addr)
#ifdef HAVE_IPV6
#define sock2ip6(X)  (((struct sockaddr_in6 *)(X))->sin6_addr.s6_addr)
#endif /* HAVE_IPV6 */

#define sockunion_family(X)  (X)->sa.sa_family

/* Prototypes. */
int str2sockunion (char *, union sockunion *);
const char *sockunion2str (union sockunion *, char *, size_t);
int sockunion_cmp (union sockunion *, union sockunion *);
int sockunion_same (union sockunion *, union sockunion *);

char *sockunion_su2str (union sockunion *su);
union sockunion *sockunion_str2su (char *str);
struct in_addr sockunion_get_in_addr (union sockunion *su);
int sockunion_accept (int sock, union sockunion *);
int sockunion_stream_socket (union sockunion *);
int sockopt_reuseaddr (int);
int sockopt_reuseport (int);
int sockunion_bind (int sock, union sockunion *, unsigned short, union sockunion *);
int sockopt_ttl (int family, int sock, int ttl);
int sockunion_socket (union sockunion *su);
const char *inet_sutop (union sockunion *su, char *str);
enum connect_result
sockunion_connect (int fd, union sockunion *su, unsigned short port, unsigned int);
union sockunion *sockunion_getsockname (int);
union sockunion *sockunion_getpeername (int);
union sockunion *sockunion_dup (union sockunion *);
void sockunion_free (union sockunion *);

#ifndef HAVE_INET_NTOP
const char *
inet_ntop (int family, const void *addrptr, char *strptr, size_t len);
#endif /* HAVE_INET_NTOP */

#ifndef HAVE_INET_PTON
int
inet_pton (int family, const char *strptr, void *addrptr);
#endif /* HAVE_INET_PTON */

#ifndef HAVE_INET_ATON
int
inet_aton (const char *cp, struct in_addr *inaddr);
#endif

#endif /* _ZEBRA_SOCKUNION_H */
