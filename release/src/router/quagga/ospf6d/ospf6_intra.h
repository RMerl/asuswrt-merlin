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

/* Debug option */
extern unsigned char conf_debug_ospf6_brouter;
extern u_int32_t conf_debug_ospf6_brouter_specific_router_id;
extern u_int32_t conf_debug_ospf6_brouter_specific_area_id;
#define OSPF6_DEBUG_BROUTER_SUMMARY         0x01
#define OSPF6_DEBUG_BROUTER_SPECIFIC_ROUTER 0x02
#define OSPF6_DEBUG_BROUTER_SPECIFIC_AREA   0x04
#define OSPF6_DEBUG_BROUTER_ON() \
  (conf_debug_ospf6_brouter |= OSPF6_DEBUG_BROUTER_SUMMARY)
#define OSPF6_DEBUG_BROUTER_OFF() \
  (conf_debug_ospf6_brouter &= ~OSPF6_DEBUG_BROUTER_SUMMARY)
#define IS_OSPF6_DEBUG_BROUTER \
  (conf_debug_ospf6_brouter & OSPF6_DEBUG_BROUTER_SUMMARY)

#define OSPF6_DEBUG_BROUTER_SPECIFIC_ROUTER_ON(router_id)             \
  do {                                                                \
    conf_debug_ospf6_brouter_specific_router_id = (router_id);        \
    conf_debug_ospf6_brouter |= OSPF6_DEBUG_BROUTER_SPECIFIC_ROUTER;  \
  } while (0)
#define OSPF6_DEBUG_BROUTER_SPECIFIC_ROUTER_OFF()                     \
  do {                                                                \
    conf_debug_ospf6_brouter_specific_router_id = 0;                  \
    conf_debug_ospf6_brouter &= ~OSPF6_DEBUG_BROUTER_SPECIFIC_ROUTER; \
  } while (0)
#define IS_OSPF6_DEBUG_BROUTER_SPECIFIC_ROUTER                        \
  (conf_debug_ospf6_brouter & OSPF6_DEBUG_BROUTER_SPECIFIC_ROUTER)
#define IS_OSPF6_DEBUG_BROUTER_SPECIFIC_ROUTER_ID(router_id)          \
  (IS_OSPF6_DEBUG_BROUTER_SPECIFIC_ROUTER &&                          \
   conf_debug_ospf6_brouter_specific_router_id == (router_id))

#define OSPF6_DEBUG_BROUTER_SPECIFIC_AREA_ON(area_id)                 \
  do {                                                                \
    conf_debug_ospf6_brouter_specific_area_id = (area_id);            \
    conf_debug_ospf6_brouter |= OSPF6_DEBUG_BROUTER_SPECIFIC_AREA;    \
  } while (0)
#define OSPF6_DEBUG_BROUTER_SPECIFIC_AREA_OFF()                       \
  do {                                                                \
    conf_debug_ospf6_brouter_specific_area_id = 0;                    \
    conf_debug_ospf6_brouter &= ~OSPF6_DEBUG_BROUTER_SPECIFIC_AREA;   \
  } while (0)
#define IS_OSPF6_DEBUG_BROUTER_SPECIFIC_AREA                          \
  (conf_debug_ospf6_brouter & OSPF6_DEBUG_BROUTER_SPECIFIC_AREA)
#define IS_OSPF6_DEBUG_BROUTER_SPECIFIC_AREA_ID(area_id)              \
  (IS_OSPF6_DEBUG_BROUTER_SPECIFIC_AREA &&                            \
   conf_debug_ospf6_brouter_specific_area_id == (area_id))

/* Router-LSA */
#define OSPF6_ROUTER_LSA_MIN_SIZE              4U
struct ospf6_router_lsa
{
  u_char bits;
  u_char options[3];
  /* followed by ospf6_router_lsdesc(s) */
};

/* Link State Description in Router-LSA */
#define OSPF6_ROUTER_LSDESC_FIX_SIZE          16U
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

enum stub_router_mode
  {
    OSPF6_NOT_STUB_ROUTER,
    OSPF6_IS_STUB_ROUTER,
    OSPF6_IS_STUB_ROUTER_V6,
  };

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
#define OSPF6_NETWORK_LSA_MIN_SIZE             4U
struct ospf6_network_lsa
{
  u_char reserved;
  u_char options[3];
  /* followed by ospf6_netowrk_lsd(s) */
};

/* Link State Description in Router-LSA */
#define OSPF6_NETWORK_LSDESC_FIX_SIZE          4U
struct ospf6_network_lsdesc
{
  u_int32_t router_id;
};
#define NETWORK_LSDESC_GET_NBR_ROUTERID(x)                  \
  (((struct ospf6_network_lsdesc *)(x))->router_id)

/* Link-LSA */
#define OSPF6_LINK_LSA_MIN_SIZE               24U /* w/o 1st IPv6 prefix */
struct ospf6_link_lsa
{
  u_char          priority;
  u_char          options[3];
  struct in6_addr linklocal_addr;
  u_int32_t       prefix_num;
  /* followed by ospf6 prefix(es) */
};

/* Intra-Area-Prefix-LSA */
#define OSPF6_INTRA_PREFIX_LSA_MIN_SIZE       12U /* w/o 1st IPv6 prefix */
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
    if (! (oa)->thread_router_lsa \
        && CHECK_FLAG((oa)->flag, OSPF6_AREA_ENABLE)) \
      (oa)->thread_router_lsa = \
        thread_add_event (master, ospf6_router_lsa_originate, oa, 0); \
  } while (0)
#define OSPF6_NETWORK_LSA_SCHEDULE(oi) \
  do { \
    if (! (oi)->thread_network_lsa \
        && ! CHECK_FLAG((oi)->flag, OSPF6_INTERFACE_DISABLE)) \
      (oi)->thread_network_lsa = \
        thread_add_event (master, ospf6_network_lsa_originate, oi, 0); \
  } while (0)
#define OSPF6_LINK_LSA_SCHEDULE(oi) \
  do { \
    if (! (oi)->thread_link_lsa \
        && ! CHECK_FLAG((oi)->flag, OSPF6_INTERFACE_DISABLE)) \
      (oi)->thread_link_lsa = \
        thread_add_event (master, ospf6_link_lsa_originate, oi, 0); \
  } while (0)
#define OSPF6_INTRA_PREFIX_LSA_SCHEDULE_STUB(oa) \
  do { \
    if (! (oa)->thread_intra_prefix_lsa \
        && CHECK_FLAG((oa)->flag, OSPF6_AREA_ENABLE)) \
      (oa)->thread_intra_prefix_lsa = \
        thread_add_event (master, ospf6_intra_prefix_lsa_originate_stub, \
                          oa, 0); \
  } while (0)
#define OSPF6_INTRA_PREFIX_LSA_SCHEDULE_TRANSIT(oi) \
  do { \
    if (! (oi)->thread_intra_prefix_lsa \
        && ! CHECK_FLAG((oi)->flag, OSPF6_INTERFACE_DISABLE)) \
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
extern char *ospf6_router_lsdesc_lookup (u_char type, u_int32_t interface_id,
                                         u_int32_t neighbor_interface_id,
                                         u_int32_t neighbor_router_id,
                                         struct ospf6_lsa *lsa);
extern char *ospf6_network_lsdesc_lookup (u_int32_t router_id,
                                          struct ospf6_lsa *lsa);

extern int ospf6_router_is_stub_router (struct ospf6_lsa *lsa);
extern int ospf6_router_lsa_originate (struct thread *);
extern int ospf6_network_lsa_originate (struct thread *);
extern int ospf6_link_lsa_originate (struct thread *);
extern int ospf6_intra_prefix_lsa_originate_transit (struct thread *);
extern int ospf6_intra_prefix_lsa_originate_stub (struct thread *);
extern void ospf6_intra_prefix_lsa_add (struct ospf6_lsa *lsa);
extern void ospf6_intra_prefix_lsa_remove (struct ospf6_lsa *lsa);

extern void ospf6_intra_route_calculation (struct ospf6_area *oa);
extern void ospf6_intra_brouter_calculation (struct ospf6_area *oa);

extern void ospf6_intra_init (void);

extern int config_write_ospf6_debug_brouter (struct vty *vty);
extern void install_element_ospf6_debug_brouter (void);

#endif /* OSPF6_LSA_H */

