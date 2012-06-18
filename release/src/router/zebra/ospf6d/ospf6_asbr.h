/*
 * Copyright (C) 2001 Yasuhiro Ohara
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

#ifndef OSPF6_ASBR_H
#define OSPF6_ASBR_H

/* Debug option */
extern unsigned char conf_debug_ospf6_asbr;
#define OSPF6_DEBUG_ASBR_ON() \
  (conf_debug_ospf6_asbr = 1)
#define OSPF6_DEBUG_ASBR_OFF() \
  (conf_debug_ospf6_asbr = 0)
#define IS_OSPF6_DEBUG_ASBR \
  (conf_debug_ospf6_asbr)

struct ospf6_external_info
{
  /* External route type */
  int type;

  /* Originating Link State ID */
  u_int32_t id;

  struct in6_addr forwarding;
  /* u_int32_t tag; */
};

/* AS-External-LSA */
struct ospf6_as_external_lsa
{
  u_int32_t bits_metric;

  struct ospf6_prefix prefix;
  /* followed by none or one forwarding address */
  /* followed by none or one external route tag */
  /* followed by none or one referenced LS-ID */
};

#define OSPF6_ASBR_BIT_T  ntohl (0x01000000)
#define OSPF6_ASBR_BIT_F  ntohl (0x02000000)
#define OSPF6_ASBR_BIT_E  ntohl (0x04000000)

#define OSPF6_ASBR_METRIC(E) (ntohl ((E)->bits_metric & htonl (0x00ffffff)))
#define OSPF6_ASBR_METRIC_SET(E,C) \
  { (E)->bits_metric &= htonl (0xff000000); \
    (E)->bits_metric |= htonl (0x00ffffff) & htonl (C); }

void ospf6_asbr_lsa_add (struct ospf6_lsa *lsa);
void ospf6_asbr_lsa_remove (struct ospf6_lsa *lsa);
void ospf6_asbr_lsentry_add (struct ospf6_route *asbr_entry);
void ospf6_asbr_lsentry_remove (struct ospf6_route *asbr_entry);

int ospf6_asbr_is_asbr (struct ospf6 *o);
void
ospf6_asbr_redistribute_add (int type, int ifindex, struct prefix *prefix,
                             u_int nexthop_num, struct in6_addr *nexthop);
void
ospf6_asbr_redistribute_remove (int type, int ifindex, struct prefix *prefix);

int ospf6_redistribute_config_write (struct vty *vty);

void ospf6_asbr_init ();

int config_write_ospf6_debug_asbr (struct vty *vty);
void install_element_ospf6_debug_asbr ();

#endif /* OSPF6_ASBR_H */

