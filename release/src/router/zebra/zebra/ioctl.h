/*
 * Common ioctl functions.
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

#ifndef _ZEBRA_IOCTL_H
#define _ZEBRA_IOCTL_H

/* Prototypes. */
void ifreq_set_name (struct ifreq *, struct interface *);
int if_ioctl (u_long, caddr_t);

int if_set_flags (struct interface *, unsigned long);
int if_unset_flags (struct interface *, unsigned long);
void if_get_flags (struct interface *);

int if_set_prefix (struct interface *, struct connected *);
int if_unset_prefix (struct interface *, struct connected *);

void if_get_metric (struct interface *);
void if_get_mtu (struct interface *);

#ifdef HAVE_IPV6
int if_prefix_add_ipv6 (struct interface *, struct connected *);
int if_prefix_delete_ipv6 (struct interface *, struct connected *);

#endif /* HAVE_IPV6 */

#endif /* _ZEBRA_IOCTL_H */
