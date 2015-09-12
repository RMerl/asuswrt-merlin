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

#ifndef OSPF6_FLOOD_H
#define OSPF6_FLOOD_H

/* Debug option */
extern unsigned char conf_debug_ospf6_flooding;
#define OSPF6_DEBUG_FLOODING_ON() \
  (conf_debug_ospf6_flooding = 1)
#define OSPF6_DEBUG_FLOODING_OFF() \
  (conf_debug_ospf6_flooding = 0)
#define IS_OSPF6_DEBUG_FLOODING \
  (conf_debug_ospf6_flooding)

/* Function Prototypes */
extern struct ospf6_lsdb *ospf6_get_scoped_lsdb (struct ospf6_lsa *lsa);
extern struct ospf6_lsdb *ospf6_get_scoped_lsdb_self (struct ospf6_lsa *lsa);

/* origination & purging */
extern void ospf6_lsa_originate (struct ospf6_lsa *lsa);
extern void ospf6_lsa_originate_process (struct ospf6_lsa *lsa,
                                         struct ospf6 *process);
extern void ospf6_lsa_originate_area (struct ospf6_lsa *lsa,
                                     struct ospf6_area *oa);
extern void ospf6_lsa_originate_interface (struct ospf6_lsa *lsa,
                                           struct ospf6_interface *oi);
extern void ospf6_lsa_purge (struct ospf6_lsa *lsa);

/* access method to retrans_count */
extern void ospf6_increment_retrans_count (struct ospf6_lsa *lsa);
extern void ospf6_decrement_retrans_count (struct ospf6_lsa *lsa);

/* flooding & clear flooding */
extern void ospf6_flood_clear (struct ospf6_lsa *lsa);
extern void ospf6_flood (struct ospf6_neighbor *from, struct ospf6_lsa *lsa);

/* receive & install */
extern void ospf6_receive_lsa (struct ospf6_neighbor *from,
                               struct ospf6_lsa_header *header);
extern void ospf6_install_lsa (struct ospf6_lsa *lsa);

extern int config_write_ospf6_debug_flood (struct vty *vty);
extern void install_element_ospf6_debug_flood (void);

#endif /* OSPF6_FLOOD_H */


