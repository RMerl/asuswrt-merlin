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

/* External management communication API definitions */

#ifndef _UID_STP_H__
#define _UID_STP_H__

#define NAME_LEN    20

typedef enum {
  STP_DISABLED,
  STP_ENABLED,
} UID_STP_MODE_T;

typedef struct {
  unsigned short  prio;
  unsigned char   addr[6];
} UID_BRIDGE_ID_T;

typedef struct {
  char      vlan_name[NAME_LEN]; /* name of the VLAN, key of the bridge */
  char      action; /* 1-create, 0- delete */
} UID_STP_BR_CTRL_T;

#define BR_CFG_STATE        (1L << 0)
#define BR_CFG_PRIO         (1L << 1)
#define BR_CFG_AGE          (1L << 2)
#define BR_CFG_HELLO        (1L << 3)
#define BR_CFG_DELAY        (1L << 4)
#define BR_CFG_FORCE_VER    (1L << 5)
#define BR_CFG_AGE_MODE     (1L << 6)
#define BR_CFG_AGE_TIME     (1L << 7)
#define BR_CFG_HOLD_TIME    (1L << 8)
#define BR_CFG_ALL BR_CFG_STATE     | \
                   BR_CFG_PRIO      | \
                   BR_CFG_AGE       | \
                   BR_CFG_HELLO     | \
                   BR_CFG_DELAY     | \
                   BR_CFG_FORCE_VER | \
                   BR_CFG_AGE_MODE  | \
                   BR_CFG_AGE_TIME  | \
                   BR_CFG_HOLD_TIME

typedef struct {
  /* service data */
  unsigned long     field_mask; /* which fields to change */
  UID_STP_MODE_T    stp_enabled;
  char              vlan_name[NAME_LEN]; /* name of the VLAN, key of the bridge */

  /* protocol data */
  int           bridge_priority;
  int           max_age;
  int           hello_time;
  int           forward_delay;
  int           force_version;
  int           hold_time;
} UID_STP_CFG_T;

typedef struct {
  /* service data */
  char              vlan_name[NAME_LEN]; /* name of the VLAN, key of the bridge */
  unsigned long     vlan_id;
  UID_STP_MODE_T    stp_enabled;

  /* protocol data */
  UID_BRIDGE_ID_T   designated_root;
  unsigned long     root_path_cost;

  unsigned long     timeSince_Topo_Change; /* 14.8.1.1.3.b: TBD */
  unsigned long     Topo_Change_Count;     /* 14.8.1.1.3.c: TBD */
  unsigned char     Topo_Change;           /* 14.8.1.1.3.d: TBD */

  unsigned short    root_port;
  int               max_age;
  int               hello_time;
  int               forward_delay;
  UID_BRIDGE_ID_T   bridge_id;
} UID_STP_STATE_T;

typedef enum {
  UID_PORT_DISABLED = 0,
  UID_PORT_DISCARDING,
  UID_PORT_LEARNING,
  UID_PORT_FORWARDING,
  UID_PORT_NON_STP
} RSTP_PORT_STATE;

typedef unsigned short  UID_PORT_ID;

typedef enum {
  P2P_FORCE_TRUE,
  P2P_FORCE_FALSE,
  P2P_AUTO,
} ADMIN_P2P_T;

#ifdef __BITMAP_H

#define PT_CFG_STATE    (1L << 0)
#define PT_CFG_COST     (1L << 1)
#define PT_CFG_PRIO     (1L << 2)
#define PT_CFG_P2P      (1L << 3)
#define PT_CFG_EDGE     (1L << 4)
#define PT_CFG_MCHECK   (1L << 5)
#define PT_CFG_NON_STP  (1L << 6)
#ifdef STP_DBG
#define PT_CFG_DBG_SKIP_RX (1L << 16)
#define PT_CFG_DBG_SKIP_TX (1L << 17)
#endif

#define PT_CFG_ALL PT_CFG_STATE  | \
                   PT_CFG_COST   | \
                   PT_CFG_PRIO   | \
                   PT_CFG_P2P    | \
                   PT_CFG_EDGE   | \
                   PT_CFG_MCHECK | \
                   PT_CFG_NON_STP                  

#define ADMIN_PORT_PATH_COST_AUTO   0

typedef struct {
  /* service data */
  unsigned long field_mask; /* which fields to change */
  BITMAP_T      port_bmp;   
  char          vlan_name[NAME_LEN]; /* name of the VLAN, key of the bridge */

  /* protocol data */
  int           port_priority;
  unsigned long admin_port_path_cost; /* ADMIN_PORT_PATH_COST_AUTO - auto sence */
  ADMIN_P2P_T   admin_point2point;
  unsigned char admin_edge;
  unsigned char admin_non_stp; /* 1- doesn't participate in STP, 1 - regular */
#ifdef STP_DBG
  unsigned int	skip_rx;
  unsigned int	skip_tx;
#endif

} UID_STP_PORT_CFG_T;
#endif

typedef struct {
  /* service data */
  char              vlan_name[NAME_LEN]; /* name of the VLAN, key of the bridge */
  unsigned int      port_no; /* key of the entry */

  /* protocol data */
  UID_PORT_ID       port_id;
  RSTP_PORT_STATE   state;
  unsigned long     path_cost;

  UID_BRIDGE_ID_T   designated_root;
  unsigned long     designated_cost;
  UID_BRIDGE_ID_T   designated_bridge;
  UID_PORT_ID       designated_port;

#if 0
  int               infoIs;
  unsigned short    handshake_flags;
#endif

  unsigned long     rx_cfg_bpdu_cnt;
  unsigned long     rx_rstp_bpdu_cnt;
  unsigned long     rx_tcn_bpdu_cnt;
  int               fdWhile;      /* 17.15.1 */
  int               helloWhen;    /* 17.15.2 */
  int               mdelayWhile;  /* 17.15.3 */
  int               rbWhile;      /* 17.15.4 */
  int               rcvdInfoWhile;/* 17.15.5 */
  int               rrWhile;      /* 17.15.6 */
  int               tcWhile;      /* 17.15.7 */
  int               txCount;      /* 17.18.40 */
  int               lnkWhile;

  unsigned long     uptime;       /* 14.8.2.1.3.a */
  unsigned long     oper_port_path_cost;
  unsigned char     role;
  unsigned char     oper_point2point;
  unsigned char     oper_edge;
  unsigned char     oper_stp_neigb;
  unsigned char     top_change_ack;
  unsigned char     tc;
} UID_STP_PORT_STATE_T;

#endif

