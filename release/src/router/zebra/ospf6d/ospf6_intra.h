/*
 * Copyright (C) 2003 Yasuhiro Ohara
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

#ifndef OSPF6_INTRA_H
#define OSPF6_INTRA_H

/* Router-LSA */
struct ospf6_router_lsa
{
  u_char bits;
  u_char options[3];
  /* followed by ospf6_router_lsdesc(s) */
};

/* Link State Description in Router-LSA */
struct ospf6_router_lsdesc
{
  u_char    type;
  u_char    reserved;
  u_int16_t metric;                /* output cost */
  u_int32_t interface_id;
  u_int32_t neighbor_interface_id;
  u_int32_t neighbor_router_id;
};

#define OSPF6_ROUTER_LSDESC_POINTTOPOINT       1
#define OSPF6_ROUTER_LSDESC_TRANSIT_NETWORK    2
#define OSPF6_ROUTER_LSDESC_STUB_NETWORK       3
#define OSPF6_ROUTER_LSDESC_VIRTUAL_LINK       4

#define ROUTER_LSDESC_IS_TYPE(t,x)                         \
  ((((struct ospf6_router_lsdesc *)(x))->type ==           \
   OSPF6_ROUTER_LSDESC_ ## t) ? 1 : 0)
#define ROUTER_LSDESC_GET_METRIC(x)                        \
  (ntohs (((struct ospf6_router_lsdesc *)(x))->metric))
#define ROUTER_LSDESC_GET_IFID(x)                          \
  (ntohl (((struct ospf6_router_lsdesc *)(x))->interface_id))
#define ROUTER_LSDESC_GET_NBR_IFID(x)                      \
  (ntohl (((struct ospf6_router_lsdesc *)(x))->neighbor_interface_id))
#define ROUTER_LSDESC_GET_NBR_ROUTERID(x)                  \
  (((struct ospf6_router_lsdesc *)(x))->neighbor_router_id)

/* Network-LSA */
struct ospf6_network_lsa
{
  u_char reserved;
  u_char options[3];
  /* followed by ospf6_netowrk_lsd(s) */
};

/* Link State Description in Router-LSA */
struct ospf6_network_lsdesc
{
  u_int32_t router_id;
};
#define NETWORK_LSDESC_GET_NBR_ROUTERID(x)                  \
  (((struct ospf6_network_lsdesc *)(x))->router_id)

/* Link-LSA */
struct ospf6_link_lsa
{
  u_char          priority;
  u_char          options[3];
  struct in6_addr linklocal_addr;
  u_int32_t       prefix_num;
  /* followed by ospf6 prefix(es) */
};

/* Intra-Area-Prefix-LSA */
struct ospf6_intra_prefix_lsa
{
  u_int16_t prefix_num;
  u_int16_t ref_type;
  u_int32_t ref_id;
  u_int32_t ref_adv_router;
  /* followed by ospf6 prefix(es) */
};


#define OSPF6_ROUTER_LSA_SCHEDULE(oa) \
  do { \
    if (! (oa)->thread_router_lsa) \
      (oa)->thread_router_lsa = \
        thread_add_event (master, ospf6_router_lsa_originate, oa, 0); \
  } while (0)
#define OSPF6_NETWORK_LSA_SCHEDULE(oi) \
  do { \
    if (! (oi)->thread_network_lsa) \
      (oi)->thread_network_lsa = \
        thread_add_event (master, ospf6_network_lsa_originate, oi, 0); \
  } while (0)
#define OSPF6_LINK_LSA_SCHEDULE(oi) \
  do { \
    if (! (oi)->thread_link_lsa) \
      (oi)->thread_link_lsa = \
        thread_add_event (master, ospf6_link_lsa_originate, oi, 0); \
  } while (0)
#define OSPF6_INTRA_PREFIX_LSA_SCHEDULE_STUB(oa) \
  do { \
    if (! (oa)->thread_intra_prefix_lsa) \
      (oa)->thread_intra_prefix_lsa = \
        thread_add_event (master, ospf6_intra_prefix_lsa_originate_stub, \
                          oa, 0); \
  } while (0)
#define OSPF6_INTRA_PREFIX_LSA_SCHEDULE_TRANSIT(oi) \
  do { \
    if (! (oi)->thread_intra_prefix_lsa) \
      (oi)->thread_intra_prefix_lsa = \
        thread_add_event (master, ospf6_intra_prefix_lsa_originate_transit, \
                          oi, 0); \
  } while (0)

#define OSPF6_NETWORK_LSA_EXECUTE(oi) \
  do { \
    THREAD_OFF ((oi)->thread_network_lsa); \
    thread_execute (master, ospf6_network_lsa_originate, oi, 0); \
  } while (0)
#define OSPF6_INTRA_PREFIX_LSA_EXECUTE_TRANSIT(oi) \
  do { \
    THREAD_OFF ((oi)->thread_intra_prefix_lsa); \
    thread_execute (master, ospf6_intra_prefix_lsa_originate_transit, oi, 0); \
  } while (0)


/* Function Prototypes */
char *ospf6_router_lsdesc_lookup (u_char type, u_int32_t interface_id,
                            u_int32_t neighbor_interface_id,
                            u_int32_t neighbor_router_id,
                            struct ospf6_lsa *lsa);
char *ospf6_network_lsdesc_lookup (u_int32_t router_id,
                                   struct ospf6_lsa *lsa);

int ospf6_router_lsa_originate (struct thread *);
int ospf6_network_lsa_originate (struct thread *);
int ospf6_link_lsa_originate (struct thread *);
int ospf6_intra_prefix_lsa_originate_transit (struct thread *);
int ospf6_intra_prefix_lsa_originate_stub (struct thread *);
void ospf6_intra_prefix_lsa_add (struct ospf6_lsa *lsa);
void ospf6_intra_prefix_lsa_remove (struct ospf6_lsa *lsa);

void ospf6_intra_route_calculation (struct ospf6_area *oa);
void ospf6_intra_brouter_calculation (struct ospf6_area *oa);

void ospf6_intra_init ();

#endif /* OSPF6_LSA_H */

