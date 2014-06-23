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

/* STP PORT instance : 17.18, 17.15 */
 
#ifndef _STP_PORT_H__
#define _STP_PORT_H__

#include "statmch.h"

#define TIMERS_NUMBER   9
typedef unsigned int    PORT_TIMER_T;

typedef enum {
  Mine,
  Aged,
  Received,
  Disabled
} INFO_IS_T;

typedef enum {
  SuperiorDesignateMsg,
  RepeatedDesignateMsg,
  ConfirmedRootMsg,
  OtherMsg
} RCVD_MSG_T;

typedef enum {
  DisabledPort = 0,
  AlternatePort,
  BackupPort,
  RootPort,
  DesignatedPort,
  NonStpPort
} PORT_ROLE_T;

typedef struct port_t {
  struct port_t*     next;

  /* per Port state machines */
  STATE_MACH_T*     info;      /* 17.21 */
  STATE_MACH_T*     roletrns;  /* 17.23 */
  STATE_MACH_T*     sttrans;   /* 17.24 */
  STATE_MACH_T*     topoch;    /* 17.25 */
  STATE_MACH_T*     migrate;   /* 17.26 */
  STATE_MACH_T*     transmit;  /* 17.26 */
  STATE_MACH_T*     p2p;       /* 6.4.3, 6.5.1 */
  STATE_MACH_T*     edge;      /*  */
  STATE_MACH_T*     pcost;     /*  */

  STATE_MACH_T*     machines; /* list of machines */

  struct stpm_t*    owner; /* Bridge, that this port belongs to */
  
  /* per port Timers */
  PORT_TIMER_T      fdWhile;      /* 17.15.1 */
  PORT_TIMER_T      helloWhen;    /* 17.15.2 */
  PORT_TIMER_T      mdelayWhile;  /* 17.15.3 */
  PORT_TIMER_T      rbWhile;      /* 17.15.4 */
  PORT_TIMER_T      rcvdInfoWhile;/* 17.15.5 */
  PORT_TIMER_T      rrWhile;      /* 17.15.6 */
  PORT_TIMER_T      tcWhile;      /* 17.15.7 */
  PORT_TIMER_T      txCount;      /* 17.18.40 */
  PORT_TIMER_T      lnkWhile;

  PORT_TIMER_T*     timers[TIMERS_NUMBER]; /*list of timers */

  Bool              agreed;        /* 17.18.1 */
  PRIO_VECTOR_T     designPrio;    /* 17.18.2 */
  TIMEVALUES_T      designTimes;   /* 17.18.3 */
  Bool              forward;       /* 17.18.4 */
  Bool              forwarding;    /* 17.18.5 */
  INFO_IS_T         infoIs;        /* 17.18.6 */
  Bool              initPm;        /* 17.18.7  */
  Bool              learn;         /* 17.18.8 */
  Bool              learning;      /* 17.18.9 */
  Bool              mcheck;        /* 17.18.10 */
  PRIO_VECTOR_T     msgPrio;       /* 17.18.11 */
  TIMEVALUES_T      msgTimes;      /* 17.18.12 */
  Bool              newInfo;       /* 17.18.13 */
  Bool              operEdge;      /* 17.18.14 */
  Bool              adminEdge;     /* 17.18.14 */
  Bool              portEnabled;   /* 17.18.15 */
  PORT_ID           port_id;       /* 17.18.16 */
  PRIO_VECTOR_T     portPrio;      /* 17.18.17 */
  TIMEVALUES_T      portTimes;     /* 17.18.18 */
  Bool              proposed;      /* 17.18.19 */
  Bool              proposing;     /* 17.18.20 */
  Bool              rcvdBpdu;      /* 17.18.21 */
  RCVD_MSG_T        rcvdMsg;       /* 17.18.22 */
  Bool              rcvdRSTP;      /* 17/18.23 */
  Bool              rcvdSTP;       /* 17.18.24 */
  Bool              rcvdTc;        /* 17.18.25 */
  Bool              rcvdTcAck;     /* 17.18.26 */
  Bool              rcvdTcn;       /* 17.18.27 */
  Bool              reRoot;        /* 17.18.28 */
  Bool              reselect;      /* 17.18.29 */
  PORT_ROLE_T       role;          /* 17.18.30 */
  Bool              selected;      /* 17.18.31 */
  PORT_ROLE_T       selectedRole;  /* 17.18.32 */
  Bool              sendRSTP;      /* 17.18.33 */
  Bool              sync;          /* 17.18.34 */
  Bool              synced;        /* 17.18.35 */
  Bool              tc;            /* 17.18.36 */
  Bool              tcAck;         /* 17.18.37 */
  Bool              tcProp;        /* 17.18.38 */

  Bool              updtInfo;      /* 17.18.41 */

  /* message information */
  unsigned char     msgBpduVersion;
  unsigned char     msgBpduType;
  unsigned char     msgPortRole;
  unsigned char     msgFlags;

  unsigned long     adminPCost; /* may be ADMIN_PORT_PATH_COST_AUTO */
  unsigned long     operPCost;
  unsigned long     operSpeed;
  unsigned long     usedSpeed;
  int               LinkDelay;   /* TBD: LinkDelay may be managed ? */
  Bool              adminEnable; /* 'has LINK' */
  Bool              wasInitBpdu;  
  Bool              admin_non_stp;

  Bool              p2p_recompute;
  Bool              operPointToPointMac;
  ADMIN_P2P_T       adminPointToPointMac;

  /* statistics */
  unsigned long     rx_cfg_bpdu_cnt;
  unsigned long     rx_rstp_bpdu_cnt;
  unsigned long     rx_tcn_bpdu_cnt;

  unsigned long     uptime;       /* 14.8.2.1.3.a */

  int               port_index;
  char*             port_name;

#ifdef STP_DBG
  unsigned int	    skip_rx;
  unsigned int	    skip_tx;
#endif
} PORT_T;

PORT_T*
STP_port_create (struct stpm_t* stpm, int port_index);

void
STP_port_delete (PORT_T* this);

int
STP_port_rx_bpdu (PORT_T* this, BPDU_T* bpdu, size_t len);

void
STP_port_init (PORT_T* this, struct stpm_t* stpm, Bool check_link);

#ifdef STP_DBG
int
STP_port_trace_state_machine (PORT_T* this, char* mach_name, int enadis, int vlan_id);

void
STP_port_trace_flags (char* title, PORT_T* this);
#endif

#endif /*  _STP_PORT_H__ */

