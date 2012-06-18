/*
 * OSPF network related functions.
 *   Copyright (C) 1999 Toshiaki Takada
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

#ifndef _ZEBRA_OSPF_NETWORK_H
#define _ZEBRA_OSPF_NETWORK_H

/* Prototypes. */
int ospf_if_add_allspfrouters (struct ospf *, struct prefix *, unsigned int);
int ospf_if_drop_allspfrouters (struct ospf *, struct prefix *, unsigned int);
int ospf_if_add_alldrouters (struct ospf *, struct prefix *, unsigned int);
int ospf_if_drop_alldrouters (struct ospf *, struct prefix *, unsigned int);
int ospf_if_ipmulticast (struct ospf *, struct prefix *, unsigned int);
int ospf_sock_init (void);

#endif /* _ZEBRA_OSPF_NETWORK_H */
