/*
 * Prefix structure.
 * Copyright (C) 1998 Kunihiro Ishiguro
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

#ifndef _ZEBRA_PREFIX_H
#define _ZEBRA_PREFIX_H

/* IPv4 and IPv6 unified prefix structure. */
struct prefix
{
  u_char family;
  u_char prefixlen;
  union 
  {
    u_char prefix;
    struct in_addr prefix4;
#ifdef HAVE_IPV6
    struct in6_addr prefix6;
#endif /* HAVE_IPV6 */
    struct 
    {
      struct in_addr id;
      struct in_addr adv_router;
    } lp;
    u_char val[8];
  } u __attribute__ ((aligned (8)));
};

/* IPv4 prefix structure. */
struct prefix_ipv4
{
  u_char family;
  u_char prefixlen;
  struct in_addr prefix __attribute__ ((aligned (8)));
};

/* IPv6 prefix structure. */
#ifdef HAVE_IPV6
struct prefix_ipv6
{
  u_char family;
  u_char prefixlen;
  struct in6_addr prefix __attribute__ ((aligned (8)));
};
#endif /* HAVE_IPV6 */

struct prefix_ls
{
  u_char family;
  u_char prefixlen;
  struct in_addr id __attribute__ ((aligned (8)));
  struct in_addr adv_router;
};

/* Prefix for routing distinguisher. */
struct prefix_rd
{
  u_char family;
  u_char prefixlen;
  u_char val[8] __attribute__ ((aligned (8)));
};

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif /* INET_ADDRSTRLEN */

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif /* INET6_ADDRSTRLEN */

#ifndef INET6_BUFSIZ
#define INET6_BUFSIZ 51
#endif /* INET6_BUFSIZ */

/* Max bit/byte length of IPv4 address. */
#define IPV4_MAX_BYTELEN    4
#define IPV4_MAX_BITLEN    32
#define IPV4_MAX_PREFIXLEN 32
#define IPV4_ADDR_CMP(D,S)   memcmp ((D), (S), IPV4_MAX_BYTELEN)
#define IPV4_ADDR_SAME(D,S)  (memcmp ((D), (S), IPV4_MAX_BYTELEN) == 0)
#define IPV4_ADDR_COPY(D,S)  memcpy ((D), (S), IPV4_MAX_BYTELEN)

#define IPV4_NET0(a)    ((((u_int32_t) (a)) & 0xff000000) == 0x00000000)
#define IPV4_NET127(a)  ((((u_int32_t) (a)) & 0xff000000) == 0x7f000000)

/* Max bit/byte length of IPv6 address. */
#define IPV6_MAX_BYTELEN    16
#define IPV6_MAX_BITLEN    128
#define IPV6_MAX_PREFIXLEN 128
#define IPV6_ADDR_CMP(D,S)   memcmp ((D), (S), IPV6_MAX_BYTELEN)
#define IPV6_ADDR_SAME(D,S)  (memcmp ((D), (S), IPV6_MAX_BYTELEN) == 0)
#define IPV6_ADDR_COPY(D,S)  memcpy ((D), (S), IPV6_MAX_BYTELEN)

/* Count prefix size from mask length */
#define PSIZE(a) (((a) + 7) / (8))

/* Prefix's family member. */
#define PREFIX_FAMILY(p)  ((p)->family)

/* Prototypes. */
int afi2family (int);
int family2afi (int);

int prefix2str (struct prefix *, char *, int);
int str2prefix (char *, struct prefix *);
struct prefix *prefix_new ();
void prefix_free (struct prefix *p);

struct prefix_ipv4 *prefix_ipv4_new ();
void prefix_ipv4_free ();
int str2prefix_ipv4 (char *, struct prefix_ipv4 *);
void apply_mask_ipv4 (struct prefix_ipv4 *);
int prefix_blen (struct prefix *);
u_char ip_masklen (struct in_addr);
int prefix_ipv4_any (struct prefix_ipv4 *);
void masklen2ip (int, struct in_addr *);
void apply_classful_mask_ipv4 (struct prefix_ipv4 *);

char *prefix_family_str (struct prefix *p);
struct prefix *sockunion2prefix ();
struct prefix *sockunion2hostprefix ();

#ifdef HAVE_IPV6
struct prefix_ipv6 *prefix_ipv6_new ();
void prefix_ipv6_free ();
struct prefix *str2routev6 (char *);
int str2prefix_ipv6 (char *str, struct prefix_ipv6 *p);
void apply_mask_ipv6 (struct prefix_ipv6 *p);
void str2in6_addr (char *str, struct in6_addr *addr);
void masklen2ip6 (int masklen, struct in6_addr *netmask);
int ip6_masklen (struct in6_addr netmask);
#endif /* HAVE_IPV6 */

void apply_mask (struct prefix *);
int prefix_match (struct prefix *n, struct prefix *p);
int prefix_same (struct prefix *, struct prefix *);
int prefix_cmp (struct prefix *, struct prefix *);
void prefix_copy (struct prefix *, struct prefix *);

int all_digit (char *);
int netmask_str2prefix_str (char *, char *, char *);

#endif /* _ZEBRA_PREFIX_H */
