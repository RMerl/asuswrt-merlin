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

/* Path Cost monitoring state machine */
 
#include "base.h"
#include "stpm.h"
#include "stp_to.h" /* for STP_OUT_get_port_oper_speed */

#define STATES {        \
  CHOOSE(AUTO),         \
  CHOOSE(FORSE),        \
  CHOOSE(STABLE),       \
}

#define GET_STATE_NAME STP_pcost_get_state_name
#include "choose.h"

static long
computeAutoPCost (STATE_MACH_T *this)
{
    long lret;
    register PORT_T*  port = this->owner.port;

    if (port->usedSpeed        < 10L) {         /* < 10Mb/s */
        lret = 20000000;
    } else if (port->usedSpeed <= 10L) {        /* 10 Mb/s  */
        lret = 2000000;        
    } else if (port->usedSpeed <= 100L) {       /* 100 Mb/s */
        lret = 200000;     
    } else if (port->usedSpeed <= 1000L) {      /* 1 Gb/s */
        lret = 20000;      
    } else if (port->usedSpeed <= 10000L) {     /* 10 Gb/s */
        lret = 2000;       
    } else if (port->usedSpeed <= 100000L) {    /* 100 Gb/s */
        lret = 200;        
    } else if (port->usedSpeed <= 1000000L) {   /* 1 GTb/s */
        lret = 20;     
    } else if (port->usedSpeed <= 10000000L) {  /* 10 Tb/s */
        lret = 2;      
    } else   /* ??? */                        { /* > Tb/s */
        lret = 1;       
    }
#ifdef STP_DBG
    if (port->pcost->debug) {
      stp_trace ("usedSpeed=%lu lret=%ld", port->usedSpeed, lret);
    }
#endif

    return lret;
}

static void
updPortPathCost (PORT_T *port)
{
  port->reselect = True;
  port->selected = False;
}

void
STP_pcost_enter_state (STATE_MACH_T *this)
{
  register PORT_T*  port = this->owner.port;

  switch (this->State) {
    case BEGIN:
      break;
    case AUTO:
      port->operSpeed = STP_OUT_get_port_oper_speed (port->port_index);
#ifdef STP_DBG
      if (port->pcost->debug) {
        stp_trace ("AUTO:operSpeed=%lu", port->operSpeed);
      }
#endif
      port->usedSpeed = port->operSpeed;
      port->operPCost = computeAutoPCost (this);
      break;
    case FORSE:
      port->operPCost = port->adminPCost;
      port->usedSpeed = -1;
      break;
    case STABLE:
      updPortPathCost (port);
      break;
  }
}

Bool
STP_pcost_check_conditions (STATE_MACH_T* this)
{
  register PORT_T*  port = this->owner.port;

  switch (this->State) {
    case BEGIN:
      return STP_hop_2_state (this, AUTO);
    case AUTO:
      return STP_hop_2_state (this, STABLE);
    case FORSE:
      return STP_hop_2_state (this, STABLE);
    case STABLE:
      if (ADMIN_PORT_PATH_COST_AUTO == port->adminPCost && 
          port->operSpeed != port->usedSpeed) {
          return STP_hop_2_state (this, AUTO);
      }

      if (ADMIN_PORT_PATH_COST_AUTO != port->adminPCost &&
          port->operPCost != port->adminPCost) {
          return STP_hop_2_state (this, FORSE);
      }
      break;
  }
  return False;
}

