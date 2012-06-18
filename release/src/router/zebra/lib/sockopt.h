/* Router advertisement
 * Copyright (C) 1999 Kunihiro Ishiguro
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

#ifndef _ZEBRA_SOCKOPT_H
#define _ZEBRA_SOCKOPT_H

#ifdef HAVE_IPV6
int setsockopt_ipv6_pktinfo (int, int);
int setsockopt_ipv6_checksum (int, int);
int setsockopt_ipv6_multicast_hops (int, int);
int setsockopt_ipv6_unicast_hops (int, int);
int setsockopt_ipv6_hoplimit (int, int);
int setsockopt_ipv6_multicast_loop (int, int);
#endif /* HAVE_IPV6 */

int setsockopt_multicast_ipv4(int sock, 
			     int optname, 
			     struct in_addr if_addr,
			     unsigned int mcast_addr,
			     unsigned int ifindex);


#endif /*_ZEBRA_SOCKOPT_H */
