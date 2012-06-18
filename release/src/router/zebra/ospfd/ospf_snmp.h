/* OSPFv2 SNMP support
 * Copyright (C) 2000 IP Infusion Inc.
 *
 * Written by Kunihiro Ishiguro <kunihiro@zebra.org>
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

#ifndef _ZEBRA_OSPF_SNMP_H
#define _ZEBRA_OSPF_SNMP_H

void ospf_snmp_if_update (struct interface *);
void ospf_snmp_if_delete (struct interface *);

void ospf_snmp_vl_add (struct ospf_vl_data *);
void ospf_snmp_vl_delete (struct ospf_vl_data *);

#endif /* _ZEBRA_OSPF_SNMP_H */
