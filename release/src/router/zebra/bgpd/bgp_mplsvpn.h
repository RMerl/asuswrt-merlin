/* MPLS-VPN
   Copyright (C) 2000 Kunihiro Ishiguro <kunihiro@zebra.org>

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

#define RD_TYPE_AS      0
#define RD_TYPE_IP      1

#define RD_ADDRSTRLEN  28

struct rd_as
{
  u_int16_t type;
  as_t as;
  u_int32_t val;
};

struct rd_ip
{
  u_int16_t type;
  struct in_addr ip;
  u_int16_t val;
};

void bgp_mplsvpn_init ();
int bgp_nlri_parse_vpnv4 (struct peer *, struct attr *, struct bgp_nlri *);
u_int32_t decode_label (u_char *);
int str2prefix_rd (u_char *, struct prefix_rd *);
int str2tag (u_char *, u_char *);
char *prefix_rd2str (struct prefix_rd *, char *, size_t);
