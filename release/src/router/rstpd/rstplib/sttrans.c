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

/* Port State Transition state machine : 17.24 */
    
#include "base.h"
#include "stpm.h"
#include "stp_to.h"

#define STATES { \
  CHOOSE(DISCARDING),   \
  CHOOSE(LEARNING), \
  CHOOSE(FORWARDING),   \
}

#define GET_STATE_NAME STP_sttrans_get_state_name
#include "choose.h"


#ifdef STRONGLY_SPEC_802_1W
static Bool
disableLearning (STATE_MACH_T *this)
{
  register PORT_T *port = this->owner.port;

  return STP_OUT_set_learning (port->port_index, port->owner->vlan_id, False);
}

static Bool
enableLearning (STATE_MACH_T *this)
{
  register PORT_T *port = this->owner.port;

  return STP_OUT_set_learning (port->port_index, port->owner->vlan_id, True);
}

static Bool
disableForwarding (STATE_MACH_T *this)
{
  register PORT_T *port = this->owner.port;

  return STP_OUT_set_forwarding (port->port_index, port->owner->vlan_id, False);
}

static Bool
enableForwarding (STATE_MACH_T *this)
{
  register PORT_T *port = this->owner.port;

  return STP_OUT_set_forwarding (port->port_index, port->owner->vlan_id, True);
}
#endif

void
STP_sttrans_enter_state (STATE_MACH_T *this)
{
  register PORT_T    *port = this->owner.port;

  switch (this->State) {
    case BEGIN:
    case DISCARDING:
      port->learning = False;
      port->forwarding = False;
#ifdef STRONGLY_SPEC_802_1W
      disableLearning (this);
      disableForwarding (this);
#else
      STP_OUT_set_port_state (port->port_index, port->owner->vlan_id, UID_PORT_DISCARDING);
#endif
      break;
    case LEARNING:
      port->learning = True;
#ifdef STRONGLY_SPEC_802_1W
      enableLearning (this);
#else
      STP_OUT_set_port_state (port->port_index, port->owner->vlan_id, UID_PORT_LEARNING);
#endif
      break;
    case FORWARDING:
      port->tc = !port->operEdge;
      port->forwarding = True;
#ifdef STRONGLY_SPEC_802_1W
      enableForwarding (this);
#else
      STP_OUT_set_port_state (port->port_index, port->owner->vlan_id, UID_PORT_FORWARDING);
#endif
      break;
  }

}

Bool
STP_sttrans_check_conditions (STATE_MACH_T *this)
{
  register PORT_T       *port = this->owner.port;

  if (BEGIN == this->State) {
    return STP_hop_2_state (this, DISCARDING);
  }

  switch (this->State) {
    case DISCARDING:
      if (port->learn) {
        return STP_hop_2_state (this, LEARNING);
      }
      break;
    case LEARNING:
      if (port->forward) {
        return STP_hop_2_state (this, FORWARDING);
      }
      if (!port->learn) {
        return STP_hop_2_state (this, DISCARDING);
      }
      break;
    case FORWARDING:
      if (!port->forward) {
        return STP_hop_2_state (this, DISCARDING);
      }
      break;
  }

  return False;
}

