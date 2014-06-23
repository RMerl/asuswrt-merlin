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

/* Note: this state mashine distinkts from described in P802.1t Clause 18. */
/* I am ready to discuss it                                                */
 
#include "base.h"
#include "stpm.h"

#define STATES {        \
  CHOOSE(DISABLED),         \
  CHOOSE(DETECTED),     \
  CHOOSE(DELEAYED),     \
  CHOOSE(RESOLVED),     \
}

#define GET_STATE_NAME STP_edge_get_state_name
#include "choose.h"

#define DEFAULT_LINK_DELAY  3

void
STP_edge_enter_state (STATE_MACH_T *s)
{
  register PORT_T *port = s->owner.port;

  switch (s->State) {
    case BEGIN:
      break;
    case DISABLED:
      port->operEdge = port->adminEdge;
      port->wasInitBpdu = False;
      port->lnkWhile = 0;
      port->portEnabled = False;
      break;
    case DETECTED:
      port->portEnabled = True;
      port->lnkWhile = port->LinkDelay;
      port->operEdge = False;
      break;
    case DELEAYED:
      break;
    case RESOLVED:
      if (! port->wasInitBpdu) {
          port->operEdge = port->adminEdge;
      }
      break;
  }
}

Bool
STP_edge_check_conditions (STATE_MACH_T *s)
{
  register PORT_T *port = s->owner.port;

  switch (s->State) {
    case BEGIN:
      return STP_hop_2_state (s, DISABLED);
    case DISABLED:
      if (port->adminEnable) {
        return STP_hop_2_state (s, DETECTED);
      }
      break;
    case DETECTED:
      return STP_hop_2_state (s, DELEAYED);
    case DELEAYED:
      if (port->wasInitBpdu) {
#ifdef STP_DBG
        if (s->debug)
            stp_trace ("port %s 'edge' resolved by BPDU", port->port_name);
#endif        
        return STP_hop_2_state (s, RESOLVED);
      }

      if (! port->lnkWhile)  {
#ifdef STP_DBG
        if (s->debug)
          stp_trace ("port %s 'edge' resolved by timer", port->port_name);
#endif        
        return STP_hop_2_state (s, RESOLVED);
      }

      if (! port->adminEnable) {
        return STP_hop_2_state (s, DISABLED);
      }
      break;
    case RESOLVED:
      if (! port->adminEnable) {
        return STP_hop_2_state (s, DISABLED);
      }
      break;
  }
  return False;
}

