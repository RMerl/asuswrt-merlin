/************************************************************************ 
 * RSTP library - Rapid Spanning Tree (802.1t, 802.1w) 
 * Copyright (C) 2001-2003 Optical Access 
 * Author: Alex Rozin 
 * 
 * This file is part of RSTP library. 
 * 
 * RSTP library is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU Lesser General Public License as published by the 
 * Free Software Foundation; version 2.1 
 * 
 * RSTP library is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser 
 * General Public License for more details. 
 * 
 * You should have received a copy of the GNU Lesser General Public License 
 * along with RSTP library; see the file COPYING.  If not, write to the Free 
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
 * 02111-1307, USA. 
 **********************************************************************/

/* BPDU formats: 9.1 - 9.3, 17.28 */
 
#ifndef _STP_BPDU_H__
#define _STP_BPDU_H__

#define MIN_BPDU                7
#define BPDU_L_SAP              0x42
#define LLC_UI                  0x03
#define BPDU_PROTOCOL_ID        0x0000
#define BPDU_VERSION_ID         0x00
#define BPDU_VERSION_RAPID_ID   0x02

#define BPDU_TOPO_CHANGE_TYPE   0x80
#define BPDU_CONFIG_TYPE        0x00
#define BPDU_RSTP               0x02

#define TOLPLOGY_CHANGE_BIT     0x01
#define PROPOSAL_BIT            0x02
#define PORT_ROLE_OFFS          2   /* 0x04 & 0x08 */
#define PORT_ROLE_MASK          (0x03 << PORT_ROLE_OFFS)
#define LEARN_BIT               0x10
#define FORWARD_BIT             0x20
#define AGREEMENT_BIT           0x40
#define TOLPLOGY_CHANGE_ACK_BIT 0x80

#define RSTP_PORT_ROLE_UNKN     0x00
#define RSTP_PORT_ROLE_ALTBACK  0x01
#define RSTP_PORT_ROLE_ROOT     0x02
#define RSTP_PORT_ROLE_DESGN    0x03

typedef struct mac_header_t {
  unsigned char dst_mac[6];
  unsigned char src_mac[6];
} MAC_HEADER_T;

typedef struct eth_header_t {
  unsigned char len8023[2];
  unsigned char dsap;
  unsigned char ssap;
  unsigned char llc;
} ETH_HEADER_T;

typedef struct bpdu_header_t {
  unsigned char protocol[2];
  unsigned char version;
  unsigned char bpdu_type;
} BPDU_HEADER_T;

typedef struct bpdu_body_t {
  unsigned char flags;
  unsigned char root_id[8];
  unsigned char root_path_cost[4];
  unsigned char bridge_id[8];
  unsigned char port_id[2];
  unsigned char message_age[2];
  unsigned char max_age[2];
  unsigned char hello_time[2];
  unsigned char forward_delay[2];
} BPDU_BODY_T;

typedef struct stp_bpdu_t {
  ETH_HEADER_T  eth;
  BPDU_HEADER_T hdr;
  BPDU_BODY_T   body;
  unsigned char ver_1_len[2];
} BPDU_T;

#endif /* _STP_BPDU_H__ */

