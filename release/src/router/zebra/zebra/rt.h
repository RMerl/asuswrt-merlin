/*
 * kernel routing table update prototype.
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

#ifndef _ZEBRA_RT_H
#define _ZEBRA_RT_H

int kernel_add_ipv4 (struct prefix *, struct rib *);
int kernel_delete_ipv4 (struct prefix *, struct rib *);
int kernel_add_route (struct prefix_ipv4 *, struct in_addr *, int, int);
int kernel_address_add_ipv4 (struct interface *, struct connected *);
int kernel_address_delete_ipv4 (struct interface *, struct connected *);

#ifdef HAVE_IPV6
int kernel_add_ipv6 (struct prefix *, struct rib *);
int kernel_delete_ipv6 (struct prefix *, struct rib *);
int kernel_delete_ipv6_old (struct prefix_ipv6 *dest, struct in6_addr *gate,
			    unsigned int index, int flags, int table);

#endif /* HAVE_IPV6 */

#endif /* _ZEBRA_RT_H */
