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

/* Point To Point MAC mode selection machine : 6.4.3, 6.5.1 */
 
#include "base.h"
#include "stpm.h"
#include "stp_to.h" /* for STP_OUT_get_duplex */

#define STATES { \
  CHOOSE(INIT),     \
  CHOOSE(RECOMPUTE),    \
  CHOOSE(STABLE),    \
}

#define GET_STATE_NAME STP_p2p_get_state_name
#include "choose.h"

static Bool
computeP2P (PORT_T *port)
{
    switch (port->adminPointToPointMac) {
      case P2P_FORCE_TRUE:
        return True;
      case P2P_FORCE_FALSE:
        return False;
      default:
      case P2P_AUTO:
        return STP_OUT_get_duplex (port->port_index);
    }
}

void
STP_p2p_enter_state (STATE_MACH_T* s)
{
  register PORT_T* port = s->owner.port;

  switch (s->State) {
    case BEGIN:
    case INIT:
      port->p2p_recompute = True;
      break;
    case RECOMPUTE:
      port->operPointToPointMac = computeP2P (port);
      port->p2p_recompute = False;
      break;
    case STABLE:
      break;
  }
}

Bool
STP_p2p_check_conditions (STATE_MACH_T* s)
{
  register PORT_T* port = s->owner.port;

  switch (s->State) {
    case BEGIN:
    case INIT:
      return STP_hop_2_state (s, STABLE);
    case RECOMPUTE:
      return STP_hop_2_state (s, STABLE);
    case STABLE:
      if (port->p2p_recompute) {
        return STP_hop_2_state (s, RECOMPUTE);
      }
      break;
  }
  return False;
}

