/* BGP open message handling
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

#ifndef _QUAGGA_BGP_OPEN_H
#define _QUAGGA_BGP_OPEN_H

/* Standard header for capability TLV */
struct capability_header
{
  u_char code;
  u_char length;
};

/* Generic MP capability data */
struct capability_mp_data
{
  afi_t afi;
  u_char reserved;
  safi_t safi;
};

#pragma pack(1)
struct capability_orf_entry 
{
  struct capability_mp_data mpc;
  u_char num;
  struct {
    u_char type;
    u_char mode;
  } orfs[];
} __attribute__ ((packed));
#pragma pack()

struct capability_as4
{
  uint32_t as4;
};

struct graceful_restart_af
{
  afi_t afi;
  safi_t safi;
  u_char flag;
};

struct capability_gr
{
  u_int16_t restart_flag_time;
  struct graceful_restart_af gr[];
};

/* Capability Code */
#define CAPABILITY_CODE_MP              1 /* Multiprotocol Extensions */
#define CAPABILITY_CODE_REFRESH         2 /* Route Refresh Capability */
#define CAPABILITY_CODE_ORF             3 /* Cooperative Route Filtering Capability */
#define CAPABILITY_CODE_RESTART        64 /* Graceful Restart Capability */
#define CAPABILITY_CODE_AS4            65 /* 4-octet AS number Capability */
#define CAPABILITY_CODE_DYNAMIC        66 /* Dynamic Capability */
#define CAPABILITY_CODE_REFRESH_OLD   128 /* Route Refresh Capability(cisco) */
#define CAPABILITY_CODE_ORF_OLD       130 /* Cooperative Route Filtering Capability(cisco) */

/* Capability Length */
#define CAPABILITY_CODE_MP_LEN          4
#define CAPABILITY_CODE_REFRESH_LEN     0
#define CAPABILITY_CODE_DYNAMIC_LEN     0
#define CAPABILITY_CODE_RESTART_LEN     2 /* Receiving only case */
#define CAPABILITY_CODE_AS4_LEN         4

/* Cooperative Route Filtering Capability.  */

/* ORF Type */
#define ORF_TYPE_PREFIX                64 
#define ORF_TYPE_PREFIX_OLD           128

/* ORF Mode */
#define ORF_MODE_RECEIVE                1 
#define ORF_MODE_SEND                   2 
#define ORF_MODE_BOTH                   3 

/* Capability Message Action.  */
#define CAPABILITY_ACTION_SET           0
#define CAPABILITY_ACTION_UNSET         1

/* Graceful Restart */
#define RESTART_R_BIT              0x8000
#define RESTART_F_BIT              0x80

extern int bgp_open_option_parse (struct peer *, u_char, int *);
extern void bgp_open_capability (struct stream *, struct peer *);
extern void bgp_capability_vty_out (struct vty *, struct peer *);
extern as_t peek_for_as4_capability (struct peer *, u_char);
extern int bgp_afi_safi_valid_indices (afi_t, safi_t *);

#endif /* _QUAGGA_BGP_OPEN_H */
