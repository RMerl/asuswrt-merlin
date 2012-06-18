/*
 * Prefix related functions.
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>

#include "prefix.h"
#include "vty.h"
#include "sockunion.h"
#include "memory.h"
#include "log.h"

/* Maskbit. */
static u_char maskbit[] = {0x00, 0x80, 0xc0, 0xe0, 0xf0,
			         0xf8, 0xfc, 0xfe, 0xff};

/* Number of bits in prefix type. */
#ifndef PNBBY
#define PNBBY 8
#endif /* PNBBY */

#define MASKBIT(offset)  ((0xff << (PNBBY - (offset))) & 0xff)

/* Address Famiy Identifier to Address Family converter. */
int
afi2family (int afi)
{
  if (afi == AFI_IP)
    return AF_INET;
#ifdef HAVE_IPV6
  else if (afi == AFI_IP6)
    return AF_INET6;
#endif /* HAVE_IPV6 */
  return 0;
}

int
family2afi (int family)
{
  if (family == AF_INET)
    return AFI_IP;
#ifdef HAVE_IPV6
  else if (family == AF_INET6)
    return AFI_IP6;
#endif /* HAVE_IPV6 */
  return 0;
}

/* If n includes p prefix then return 1 else return 0. */
int
prefix_match (struct prefix *n, struct prefix *p)
{
  int offset;
  int shift;

  /* Set both prefix's head pointer. */
  u_char *np = (u_char *)&n->u.prefix;
  u_char *pp = (u_char *)&p->u.prefix;

  /* If n's prefix is longer than p's one return 0. */
  if (n->prefixlen > p->prefixlen)
    return 0;

  offset = n->prefixlen / PNBBY;
  shift =  n->prefixlen % PNBBY;

  if (shift)
    if (maskbit[shift] & (np[offset] ^ pp[offset]))
      return 0;
  
  while (offset--)
    if (np[offset] != pp[offset])
      return 0;
  return 1;
}

/* Copy prefix from src to dest. */
void
prefix_copy (struct prefix *dest, struct prefix *src)
{
  dest->family = src->family;
  dest->prefixlen = src->prefixlen;

  if (src->family == AF_INET)
    dest->u.prefix4 = src->u.prefix4;
#ifdef HAVE_IPV6
  else if (src->family == AF_INET6)
    dest->u.prefix6 = src->u.prefix6;
#endif /* HAVE_IPV6 */
  else if (src->family == AF_UNSPEC)
    {
      dest->u.lp.id = src->u.lp.id;
      dest->u.lp.adv_router = src->u.lp.adv_router;
    }
  else
    {
      zlog (NULL, LOG_INFO, "prefix_copy(): Unknown address family %d",
	      src->family);
      assert (0);
    }
}

/* If both prefix structure is same then return 1 else return 0. */
int
prefix_same (struct prefix *p1, struct prefix *p2)
{
  if (p1->family == p2->family && p1->prefixlen == p2->prefixlen)
    {
      if (p1->family == AF_INET)
	if (IPV4_ADDR_SAME (&p1->u.prefix, &p2->u.prefix))
	  return 1;
#ifdef HAVE_IPV6
      if (p1->family == AF_INET6 )
	if (IPV6_ADDR_SAME (&p1->u.prefix, &p2->u.prefix))
	  return 1;
#endif /* HAVE_IPV6 */
    }
  return 0;
}

/* When both prefix structure is not same, but will be same after
   applying mask, return 0. otherwise, return 1 */
int
prefix_cmp (struct prefix *p1, struct prefix *p2)
{
  int offset;
  int shift;

  /* Set both prefix's head pointer. */
  u_char *pp1 = (u_char *)&p1->u.prefix;
  u_char *pp2 = (u_char *)&p2->u.prefix;

  if (p1->family != p2->family || p1->prefixlen != p2->prefixlen)
    return 1;

  offset = p1->prefixlen / 8;
  shift = p1->prefixlen % 8;

  if (shift)
    if (maskbit[shift] & (pp1[offset] ^ pp2[offset]))
      return 1;

  while (offset--)
    if (pp1[offset] != pp2[offset])
      return 1;

  return 0;
}

/* Return prefix family type string. */
char *
prefix_family_str (struct prefix *p)
{
  if (p->family == AF_INET)
    return "inet";
#ifdef HAVE_IPV6
  if (p->family == AF_INET6)
    return "inet6";
#endif /* HAVE_IPV6 */
  return "unspec";
}

/* Allocate new prefix_ipv4 structure. */
struct prefix_ipv4 *
prefix_ipv4_new ()
{
  struct prefix_ipv4 *p;

  p = XCALLOC (MTYPE_PREFIX_IPV4, sizeof *p);
  p->family = AF_INET;
  return p;
}

/* Free prefix_ipv4 structure. */
void
prefix_ipv4_free (struct prefix_ipv4 *p)
{
  XFREE (MTYPE_PREFIX_IPV4, p);
}

/* When string format is invalid return 0. */
int
str2prefix_ipv4 (char *str, struct prefix_ipv4 *p)
{
  int ret;
  int plen;
  char *pnt;
  char *cp;

  /* Find slash inside string. */
  pnt = strchr (str, '/');

  /* String doesn't contail slash. */
  if (pnt == NULL) 
    {
      /* Convert string to prefix. */
      ret = inet_aton (str, &p->prefix);
      if (ret == 0)
	return 0;

      /* If address doesn't contain slash we assume it host address. */
      p->family = AF_INET;
      p->prefixlen = IPV4_MAX_BITLEN;

      return ret;
    }
  else
    {
      cp = XMALLOC (MTYPE_TMP, (pnt - str) + 1);
      strncpy (cp, str, pnt - str);
      *(cp + (pnt - str)) = '\0';
      ret = inet_aton (cp, &p->prefix);
      XFREE (MTYPE_TMP, cp);

      /* Get prefix length. */
      plen = (u_char) atoi (++pnt);
      if (plen > 32)
	return 0;

      p->family = AF_INET;
      p->prefixlen = plen;
    }

  return ret;
}

/* Convert masklen into IP address's netmask. */
void
masklen2ip (int masklen, struct in_addr *netmask)
{
  u_char *pnt;
  int bit;
  int offset;

  memset (netmask, 0, sizeof (struct in_addr));
  pnt = (unsigned char *) netmask;

  offset = masklen / 8;
  bit = masklen % 8;
  
  while (offset--)
    *pnt++ = 0xff;

  if (bit)
    *pnt = maskbit[bit];
}

/* Convert IP address's netmask into integer. We assume netmask is
   sequential one. Argument netmask should be network byte order. */
u_char
ip_masklen (struct in_addr netmask)
{
  u_char len;
  u_char *pnt;
  u_char *end;
  u_char val;

  len = 0;
  pnt = (u_char *) &netmask;
  end = pnt + 4;

  while ((*pnt == 0xff) && pnt < end)
    {
      len+= 8;
      pnt++;
    } 

  if (pnt < end)
    {
      val = *pnt;
      while (val)
	{
	  len++;
	  val <<= 1;
	}
    }
  return len;
}

/* Apply mask to IPv4 prefix. */
void
apply_mask_ipv4 (struct prefix_ipv4 *p)
{
  u_char *pnt;
  int index;
  int offset;

  index = p->prefixlen / 8;

  if (index < 4)
    {
      pnt = (u_char *) &p->prefix;
      offset = p->prefixlen % 8;

      pnt[index] &= maskbit[offset];
      index++;

      while (index < 4)
	pnt[index++] = 0;
    }
}

/* If prefix is 0.0.0.0/0 then return 1 else return 0. */
int
prefix_ipv4_any (struct prefix_ipv4 *p)
{
  return (p->prefix.s_addr == 0 && p->prefixlen == 0);
}

#ifdef HAVE_IPV6

/* Allocate a new ip version 6 route */
struct prefix_ipv6 *
prefix_ipv6_new ()
{
  struct prefix_ipv6 *p;

  p = XCALLOC (MTYPE_PREFIX_IPV6, sizeof (struct prefix_ipv6));
  p->family = AF_INET6;
  return p;
}

/* Free prefix for IPv6. */
void
prefix_ipv6_free (struct prefix_ipv6 *p)
{
  XFREE (MTYPE_PREFIX_IPV6, p);
}

/* If given string is valid return pin6 else return NULL */
int
str2prefix_ipv6 (char *str, struct prefix_ipv6 *p)
{
  char *pnt;
  char *cp;
  int ret;

  pnt = strchr (str, '/');

  /* If string doesn't contain `/' treat it as host route. */
  if (pnt == NULL) 
    {
      ret = inet_pton (AF_INET6, str, &p->prefix);
      if (ret != 1)
	return 0;
      p->prefixlen = IPV6_MAX_BITLEN;
    }
  else 
    {
      int plen;

      cp = XMALLOC (0, (pnt - str) + 1);
      strncpy (cp, str, pnt - str);
      *(cp + (pnt - str)) = '\0';
      ret = inet_pton (AF_INET6, cp, &p->prefix);
      free (cp);
      if (ret != 1)
	return 0;
      plen = (u_char) atoi (++pnt);
      if (plen > 128)
	return 0;
      p->prefixlen = plen;
    }
  p->family = AF_INET6;

  return ret;
}

/* Convert struct in6_addr netmask into integer. */
int
ip6_masklen (struct in6_addr netmask)
{
  int len = 0;
  unsigned char val;
  unsigned char *pnt;
  
  pnt = (unsigned char *) & netmask;

  while ((*pnt == 0xff) && len < 128) 
    {
      len += 8;
      pnt++;
    } 
  
  if (len < 128) 
    {
      val = *pnt;
      while (val) 
	{
	  len++;
	  val <<= 1;
	}
    }
  return len;
}

void
masklen2ip6 (int masklen, struct in6_addr *netmask)
{
  unsigned char *pnt;
  int bit;
  int offset;

  memset (netmask, 0, sizeof (struct in6_addr));
  pnt = (unsigned char *) netmask;

  offset = masklen / 8;
  bit = masklen % 8;

  while (offset--)
    *pnt++ = 0xff;

  if (bit)
    *pnt = maskbit[bit];
}

void
apply_mask_ipv6 (struct prefix_ipv6 *p)
{
  u_char *pnt;
  int index;
  int offset;

  index = p->prefixlen / 8;

  if (index < 16)
    {
      pnt = (u_char *) &p->prefix;
      offset = p->prefixlen % 8;

      pnt[index] &= maskbit[offset];
      index++;

      while (index < 16)
	pnt[index++] = 0;
    }
}

void
str2in6_addr (char *str, struct in6_addr *addr)
{
  int i;
  unsigned int x;

  /* %x must point to unsinged int */
  for (i = 0; i < 16; i++)
    {
      sscanf (str + (i * 2), "%02x", &x);
      addr->s6_addr[i] = x & 0xff;
    }
}
#endif /* HAVE_IPV6 */

void
apply_mask (struct prefix *p)
{
  switch (p->family)
    {
      case AF_INET:
        apply_mask_ipv4 ((struct prefix_ipv4 *)p);
        break;
#ifdef HAVE_IPV6
      case AF_INET6:
        apply_mask_ipv6 ((struct prefix_ipv6 *)p);
        break;
#endif /* HAVE_IPV6 */
      default:
        break;
    }
  return;
}

/* Utility function of convert between struct prefix <=> union sockunion */
struct prefix *
sockunion2prefix (union sockunion *dest,
		  union sockunion *mask)
{
  if (dest->sa.sa_family == AF_INET)
    {
      struct prefix_ipv4 *p;

      p = prefix_ipv4_new ();
      p->family = AF_INET;
      p->prefix = dest->sin.sin_addr;
      p->prefixlen = ip_masklen (mask->sin.sin_addr);
      return (struct prefix *) p;
    }
#ifdef HAVE_IPV6
  if (dest->sa.sa_family == AF_INET6)
    {
      struct prefix_ipv6 *p;

      p = prefix_ipv6_new ();
      p->family = AF_INET6;
      p->prefixlen = ip6_masklen (mask->sin6.sin6_addr);
      memcpy (&p->prefix, &dest->sin6.sin6_addr, sizeof (struct in6_addr));
      return (struct prefix *) p;
    }
#endif /* HAVE_IPV6 */
  return NULL;
}

/* Utility function of convert between struct prefix <=> union sockunion */
struct prefix *
sockunion2hostprefix (union sockunion *su)
{
  if (su->sa.sa_family == AF_INET)
    {
      struct prefix_ipv4 *p;

      p = prefix_ipv4_new ();
      p->family = AF_INET;
      p->prefix = su->sin.sin_addr;
      p->prefixlen = IPV4_MAX_BITLEN;
      return (struct prefix *) p;
    }
#ifdef HAVE_IPV6
  if (su->sa.sa_family == AF_INET6)
    {
      struct prefix_ipv6 *p;

      p = prefix_ipv6_new ();
      p->family = AF_INET6;
      p->prefixlen = IPV6_MAX_BITLEN;
      memcpy (&p->prefix, &su->sin6.sin6_addr, sizeof (struct in6_addr));
      return (struct prefix *) p;
    }
#endif /* HAVE_IPV6 */
  return NULL;
}

int
prefix_blen (struct prefix *p)
{
  switch (p->family) 
    {
    case AF_INET:
      return IPV4_MAX_BYTELEN;
      break;
#ifdef HAVE_IPV6
    case AF_INET6:
      return IPV6_MAX_BYTELEN;
      break;
#endif /* HAVE_IPV6 */
    }
  return 0;
}

/* Generic function for conversion string to struct prefix. */
int
str2prefix (char *str, struct prefix *p)
{
  int ret;

  /* First we try to convert string to struct prefix_ipv4. */
  ret = str2prefix_ipv4 (str, (struct prefix_ipv4 *) p);
  if (ret)
    return ret;

#ifdef HAVE_IPV6
  /* Next we try to convert string to struct prefix_ipv6. */
  ret = str2prefix_ipv6 (str, (struct prefix_ipv6 *) p);
  if (ret)
    return ret;
#endif /* HAVE_IPV6 */

  return 0;
}

int
prefix2str (struct prefix *p, char *str, int size)
{
  char buf[BUFSIZ];

  inet_ntop (p->family, &p->u.prefix, buf, BUFSIZ);
  snprintf (str, size, "%s/%d", buf, p->prefixlen);
  return 0;
}

struct prefix *
prefix_new ()
{
  struct prefix *p;

  p = XCALLOC (MTYPE_PREFIX, sizeof *p);
  return p;
}

/* Free prefix structure. */
void
prefix_free (struct prefix *p)
{
  XFREE (MTYPE_PREFIX, p);
}

/* Utility function.  Check the string only contains digit
   character. */
int
all_digit (char *str)
{
  for (; *str != '\0'; str++)
    if (!isdigit ((int) *str))
      return 0;
  return 1;
}

/* Utility function to convert ipv4 prefixes to Classful prefixes */
void apply_classful_mask_ipv4 (struct prefix_ipv4 *p)
{

  u_int32_t destination;
  
  destination = ntohl (p->prefix.s_addr);
  
  if (p->prefixlen == 32);
  /* do nothing for host routes */
  else if (IN_CLASSC (destination)) 
    {
      p->prefixlen=24;
      apply_mask_ipv4(p);
    }
  else if (IN_CLASSB(destination)) 
    {
      p->prefixlen=16;
      apply_mask_ipv4(p);
    }
  else 
    {
      p->prefixlen=8;
      apply_mask_ipv4(p);
    }
}

/* Utility function to convert ipv4 netmask to prefixes 
   ex.) "1.1.0.0" "255.255.0.0" => "1.1.0.0/16"
   ex.) "1.0.0.0" NULL => "1.0.0.0/8"                   */
int
netmask_str2prefix_str (char *net_str, char *mask_str, char *prefix_str)
{
  struct in_addr network;
  struct in_addr mask;
  u_char prefixlen;
  u_int32_t destination;
  int ret;

  ret = inet_aton (net_str, &network);
  if (! ret)
    return 0;

  if (mask_str)
    {
      ret = inet_aton (mask_str, &mask);
      if (! ret)
        return 0;

      prefixlen = ip_masklen (mask);
    }
  else 
    {
      destination = ntohl (network.s_addr);

      if (network.s_addr == 0)
	prefixlen = 0;
      else if (IN_CLASSC (destination))
	prefixlen = 24;
      else if (IN_CLASSB (destination))
	prefixlen = 16;
      else if (IN_CLASSA (destination))
	prefixlen = 8;
      else
	return 0;
    }

  sprintf (prefix_str, "%s/%d", net_str, prefixlen);

  return 1;
}

