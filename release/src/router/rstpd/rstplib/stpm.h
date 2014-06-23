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

/* STP machine instance : bridge per VLAN: 17.17 */
/* The Clause 17.13 points: "NOTE:The operation of the Bridge as a whole can 
 * be represented by the interaction between Bridge Ports specified,
 * and by parameters of the Bridge stored in ‘Port 0’. This removes the
 * need for any ‘per Bridge’ specification elements, and helps ensure
 * the minimum dependencies between Bridge Ports. This in turn supports
 * the development of implementations that scale well with increasing
 * numbers of Bridge Ports. This shift of focus to ‘per Port operation’
 * for the RSTP is supported by underlying technical changes from the
 * Spanning Tree Algorithm and Protocol (Clause 8):"
 * Newetheless, it seems to me, the behaviour of of the bridge, its variables
 * and functions are so distinct from Port, that I decided to design Bridge
 * instance. I called this object 'stpm' from STP machine. I'd like to see
 * another procedural model, more corresponding to this note on Clause 17.13 */
  
#ifndef _STP_MACHINE_H__
#define _STP_MACHINE_H__

#include "port.h"
#include "rolesel.h"

#define TxHoldCount          3 /* 17.16.6, 17.28.2(Table 17-5) */

typedef enum {/* 17.12, 17.16.1 */
  FORCE_STP_COMPAT = 0,
  NORMAL_RSTP = 2
} PROTOCOL_VERSION_T;

typedef struct stpm_t {
  struct stpm_t*        next;

  struct port_t*        ports;

  /* The only "per bridge" state machine */
  STATE_MACH_T*         rolesel;   /* the Port Role Selection State machione: 17.22 */
  STATE_MACH_T*         machines;

  /* variables */
  PROTOCOL_VERSION_T    ForceVersion;   /* 17.12, 17.16.1 */
  BRIDGE_ID             BrId;           /* 17.17.2 */
  TIMEVALUES_T          BrTimes;        /* 17.17.4 */
  PORT_ID               rootPortId;     /* 17.17.5 */
  PRIO_VECTOR_T         rootPrio;       /* 17.17.6 */
  TIMEVALUES_T          rootTimes;      /* 17.17.7 */
  
  int                   vlan_id;        /* let's say: tag */
  char*                 name;           /* name of the VLAN, maily for debugging */
  UID_STP_MODE_T        admin_state;    /* STP_DISABLED or STP_ENABLED; type see in UiD */

  unsigned long         timeSince_Topo_Change; /* 14.8.1.1.3.b */
  unsigned long         Topo_Change_Count;     /* 14.8.1.1.3.c */
  unsigned char         Topo_Change;           /* 14.8.1.1.3.d */
} STPM_T;

/* Functions prototypes */

void
STP_stpm_one_second (STPM_T* param);

STPM_T*
STP_stpm_create (int vlan_id, char* name);

int
STP_stpm_enable (STPM_T* this, UID_STP_MODE_T admin_state);

void
STP_stpm_delete (STPM_T* this);

int
STP_stpm_start (STPM_T* this);

void
STP_stpm_stop (STPM_T* this);

int
STP_stpm_update (STPM_T* this);

BRIDGE_ID *
STP_compute_bridge_id (STPM_T* this);

STPM_T *
STP_stpm_get_the_list (void);

extern STPM_T *bridges;

void
STP_stpm_update_after_bridge_management (STPM_T* this);

int
STP_stpm_check_bridge_priority (STPM_T* this);

const char*
STP_stpm_get_port_name_by_id (STPM_T* this, PORT_ID port_id);
  
#endif /* _STP_MACHINE_H__ */
