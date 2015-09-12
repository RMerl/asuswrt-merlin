/* BGP packet management header.
   Copyright (C) 1999 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#ifndef _QUAGGA_BGP_PACKET_H
#define _QUAGGA_BGP_PACKET_H

#define BGP_NLRI_LENGTH       1U
#define BGP_TOTAL_ATTR_LEN    2U
#define BGP_UNFEASIBLE_LEN    2U
#define BGP_WRITE_PACKET_MAX 10U

/* When to refresh */
#define REFRESH_IMMEDIATE 1
#define REFRESH_DEFER     2 

/* ORF Common part flag */
#define ORF_COMMON_PART_ADD        0x00 
#define ORF_COMMON_PART_REMOVE     0x80 
#define ORF_COMMON_PART_REMOVE_ALL 0xC0 
#define ORF_COMMON_PART_PERMIT     0x00 
#define ORF_COMMON_PART_DENY       0x20 

/* Packet send and receive function prototypes. */
extern int bgp_read (struct thread *);
extern int bgp_write (struct thread *);

extern void bgp_keepalive_send (struct peer *);
extern void bgp_open_send (struct peer *);
extern void bgp_notify_send (struct peer *, u_int8_t, u_int8_t);
extern void bgp_notify_send_with_data (struct peer *, u_int8_t, u_int8_t, 
                                u_int8_t *, size_t);
extern void bgp_route_refresh_send (struct peer *, afi_t, safi_t, u_char, u_char, int);
extern void bgp_capability_send (struct peer *, afi_t, safi_t, int, int);
extern void bgp_default_update_send (struct peer *, struct attr *,
			      afi_t, safi_t, struct peer *);
extern void bgp_default_withdraw_send (struct peer *, afi_t, safi_t);

extern int bgp_capability_receive (struct peer *, bgp_size_t);

#endif /* _QUAGGA_BGP_PACKET_H */
