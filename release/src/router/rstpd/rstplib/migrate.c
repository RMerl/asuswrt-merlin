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

/* Port Protocol Migration state machine : 17.26 */
 
#include "base.h"
#include "stpm.h"

#define STATES { \
  CHOOSE(INIT),     \
  CHOOSE(SEND_RSTP),    \
  CHOOSE(SENDING_RSTP), \
  CHOOSE(SEND_STP), \
  CHOOSE(SENDING_STP),  \
}

#define GET_STATE_NAME STP_migrate_get_state_name
#include "choose.h"

#define MigrateTime 3 /* 17,16.4 */

void
STP_migrate_enter_state (STATE_MACH_T* this)
{
  register PORT_T*       port = this->owner.port;

  switch (this->State) {
    case BEGIN:
    case INIT:
      port->initPm = True;
      port->mcheck = False;
      break;
    case SEND_RSTP:
      port->mdelayWhile = MigrateTime;
      port->mcheck = port->initPm = False;
      port->sendRSTP = True;
      break;
    case SENDING_RSTP:
      port->rcvdRSTP = port->rcvdSTP = False;
      break;
    case SEND_STP:
      port->mdelayWhile = MigrateTime;
      port->sendRSTP = False;
      port->initPm = False;
      break;
    case SENDING_STP:
      port->rcvdRSTP = port->rcvdSTP = False;
      break;
  }
}

Bool
STP_migrate_check_conditions (STATE_MACH_T* this)
{
  register PORT_T*    port = this->owner.port;

  if ((!port->portEnabled && !port->initPm) || BEGIN == this->State)
    return STP_hop_2_state (this, INIT);

  switch (this->State) {
    case INIT:
      if (port->portEnabled) {
        return STP_hop_2_state (this, (port->owner->ForceVersion >= 2) ?
                                   SEND_RSTP : SEND_STP);
      }
      break;
    case SEND_RSTP:
      return STP_hop_2_state (this, SENDING_RSTP);
    case SENDING_RSTP:
      if (port->mcheck)
        return STP_hop_2_state (this, SEND_RSTP);
      if (port->mdelayWhile &&
          (port->rcvdSTP || port->rcvdRSTP)) {
        return STP_hop_2_state (this, SENDING_RSTP);
      }
        
      if (!port->mdelayWhile && port->rcvdSTP) {
        return STP_hop_2_state (this, SEND_STP);       
      }
        
      if (port->owner->ForceVersion < 2) {
        return STP_hop_2_state (this, SEND_STP);
      }
        
      break;
    case SEND_STP:
      return STP_hop_2_state (this, SENDING_STP);
    case SENDING_STP:
      if (port->mcheck)
        return STP_hop_2_state (this, SEND_RSTP);
      if (port->mdelayWhile &&
          (port->rcvdSTP || port->rcvdRSTP))
        return STP_hop_2_state (this, SENDING_STP);
      if (!port->mdelayWhile && port->rcvdRSTP)
        return STP_hop_2_state (this, SEND_RSTP);
      break;
  }
  return False;
}

