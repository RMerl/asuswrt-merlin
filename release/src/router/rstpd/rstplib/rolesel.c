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

/* Port Role Selection state machine : 17.22 */

#include "base.h"
#include "stpm.h"

#define STATES { \
  CHOOSE(INIT_BRIDGE),      \
  CHOOSE(ROLE_SELECTION),   \
}

#define GET_STATE_NAME STP_rolesel_get_state_name
#include "choose.h"

#ifdef STP_DBG
void stp_dbg_break_point (PORT_T * port, STPM_T* stpm)
{
}
#endif

static Bool
_is_backup_port (PORT_T* port, STPM_T* this)
{
  if (!STP_VECT_compare_bridge_id
      (&port->portPrio.design_bridge, &this->BrId)) {
#if 0 /* def STP_DBG */
    if (port->info->debug) {
      STP_VECT_br_id_print ("portPrio.design_bridge",
                            &port->portPrio.design_bridge, True);
      STP_VECT_br_id_print ("            this->BrId",
                            &this->BrId, True);
    }
    stp_dbg_break_point (port, this);
#endif
    return True;
  } else {
    return False;
  }
}

static void
setRoleSelected (char* reason, STPM_T* stpm, PORT_T* port,
                PORT_ROLE_T newRole)
{
  char* new_role_name;

  port->selectedRole = newRole;

  if (newRole == port->role)
    return;

  switch (newRole) {
    case DisabledPort:
      new_role_name = "Disabled";
      break;
    case AlternatePort:
      new_role_name = "Alternate";
      break;
    case BackupPort:
      new_role_name = "Backup";
      break;
    case RootPort:
      new_role_name = "Root";
      break;
    case DesignatedPort:
      new_role_name = "Designated";
      break;
    case NonStpPort:
      new_role_name = "NonStp";
      port->role = newRole;
      break;
    default:
      stp_trace ("%s-%s:port %s => Unknown (%d ?)",
                 reason, stpm->name, port->port_name, (int) newRole);
      return;
  }

#ifdef STP_DBG
  if (port->roletrns->debug)
    stp_trace ("%s(%s-%s) => %s",
               reason, stpm->name, port->port_name, new_role_name);
#endif
}

static void
updtRoleDisableBridge (STPM_T* this)
{               /* 17.10.20 */
  register PORT_T *port;

  for (port = this->ports; port; port = port->next) {
    port->selectedRole = DisabledPort;
  }
}

static void
clearReselectBridge (STPM_T* this)
{               /* 17.19.1 */
  register PORT_T *port;

  for (port = this->ports; port; port = port->next) {
    port->reselect = False;
  }
}

static void
updtRootPrio (STATE_MACH_T* this)
{
  PRIO_VECTOR_T rootPathPrio;   /* 17.4.2.2 */
  register PORT_T *port;
  register STPM_T *stpm;
  register unsigned int dm;

  stpm = this->owner.stpm;

  for (port = stpm->ports; port; port = port->next) {
    if (port->admin_non_stp) {
      continue;
    }

    if (Disabled == port->infoIs)
      continue;
    if (Aged == port->infoIs)
      continue;
    if (Mine == port->infoIs) {
#if 0 /* def STP_DBG */
      stp_dbg_break_point (port); /* for debugger break point */
#endif
      continue;
    }

    STP_VECT_copy (&rootPathPrio, &port->portPrio);
    rootPathPrio.root_path_cost += port->operPCost;

    if (STP_VECT_compare_vector (&rootPathPrio, &stpm->rootPrio) < 0) {
      STP_VECT_copy (&stpm->rootPrio, &rootPathPrio);
      STP_copy_times (&stpm->rootTimes, &port->portTimes);
      dm = (8 +  stpm->rootTimes.MaxAge) / 16;
      if (!dm)
        dm = 1;
      stpm->rootTimes.MessageAge += dm;
#ifdef STP_DBG
      if (port->roletrns->debug)
          stp_trace ("updtRootPrio: dm=%d rootTimes.MessageAge=%d on port %s",
                 (int) dm, (int) stpm->rootTimes.MessageAge,
                 port->port_name);
#endif
    }
  }
}

static void
updtRolesBridge (STATE_MACH_T* this)
{               /* 17.19.21 */
  register PORT_T* port;
  register STPM_T* stpm;
  PORT_ID old_root_port; /* for tracing of root port changing */

  stpm = this->owner.stpm;
  old_root_port = stpm->rootPortId;

  STP_VECT_create (&stpm->rootPrio, &stpm->BrId, 0, &stpm->BrId, 0, 0);
  STP_copy_times (&stpm->rootTimes, &stpm->BrTimes);
  stpm->rootPortId = 0;

  updtRootPrio (this);

  for (port = stpm->ports; port; port = port->next) {
    if (port->admin_non_stp) {
      continue;
    }
    STP_VECT_create (&port->designPrio,
             &stpm->rootPrio.root_bridge,
             stpm->rootPrio.root_path_cost,
             &stpm->BrId, port->port_id, port->port_id);
    STP_copy_times (&port->designTimes, &stpm->rootTimes);

#if 0
#ifdef STP_DBG
    if (port->roletrns->debug) {
      STP_VECT_br_id_print ("ch:designPrio.design_bridge",
                            &port->designPrio.design_bridge, True);
    }
#endif
#endif
  }

  stpm->rootPortId = stpm->rootPrio.bridge_port;

#ifdef STP_DBG
  if (old_root_port != stpm->rootPortId) {
    if (! stpm->rootPortId) {
      stp_trace ("\nbrige %s became root", stpm->name);
    } else {
      stp_trace ("\nbrige %s new root port: %s",
        stpm->name,
        STP_stpm_get_port_name_by_id (stpm, stpm->rootPortId));
    }
  }
#endif

  for (port = stpm->ports; port; port = port->next) {
    if (port->admin_non_stp) {
      setRoleSelected ("Non", stpm, port, NonStpPort);
      port->forward = port->learn = True;
      continue;
    }

    switch (port->infoIs) {
      case Disabled:
        setRoleSelected ("Dis", stpm, port, DisabledPort);
        break;
      case Aged:
        setRoleSelected ("Age", stpm, port, DesignatedPort);
        port->updtInfo = True;
        break;
      case Mine:
        setRoleSelected ("Mine", stpm, port, DesignatedPort);
        if (0 != STP_VECT_compare_vector (&port->portPrio,
                      &port->designPrio) ||
            0 != STP_compare_times (&port->portTimes,
                  &port->designTimes)) {
            port->updtInfo = True;
        }
        break;
      case Received:
        if (stpm->rootPortId == port->port_id) {
          setRoleSelected ("Rec", stpm, port, RootPort);
        } else if (STP_VECT_compare_vector (&port->designPrio, &port->portPrio) < 0) {
          /* Note: this important piece has been inserted after
           * discussion with Mick Sieman and reading 802.1y Z1 */
          setRoleSelected ("Rec", stpm, port, DesignatedPort);
          port->updtInfo = True;
          break;
        } else {
          if (_is_backup_port (port, stpm)) {
            setRoleSelected ("rec", stpm, port, BackupPort);
          } else {
            setRoleSelected ("rec", stpm, port, AlternatePort);
          }
        }
        port->updtInfo = False;
        break;
      default:
        stp_trace ("undef infoIs=%d", (int) port->infoIs);
        break;
    }
  }

}


static Bool
setSelectedBridge (STPM_T* this)
{
  register PORT_T* port;

  for (port = this->ports; port; port = port->next) {
    if (port->reselect) {
#ifdef STP_DBG
      stp_trace ("setSelectedBridge: TRUE=reselect on port %s", port->port_name);
#endif
      return False;
    }
  }

  for (port = this->ports; port; port = port->next) {
    port->selected = True;
  }

  return True;
}

void
STP_rolesel_enter_state (STATE_MACH_T* this)
{
  STPM_T* stpm;

  stpm = this->owner.stpm;

  switch (this->State) {
    case BEGIN:
    case INIT_BRIDGE:
      updtRoleDisableBridge (stpm);
      break;
    case ROLE_SELECTION:
      clearReselectBridge (stpm);
      updtRolesBridge (this);
      setSelectedBridge (stpm);
      break;
  }
}

Bool
STP_rolesel_check_conditions (STATE_MACH_T* s)
{
  STPM_T* stpm;
  register PORT_T* port;

  if (BEGIN == s->State) {
    STP_hop_2_state (s, INIT_BRIDGE);
  }

  switch (s->State) {
    case BEGIN:
      return STP_hop_2_state (s, INIT_BRIDGE);
    case INIT_BRIDGE:
      return STP_hop_2_state (s, ROLE_SELECTION);
    case ROLE_SELECTION:
      stpm = s->owner.stpm;
      for (port = stpm->ports; port; port = port->next) {
        if (port->reselect) {
          /* stp_trace ("reselect on port %s", port->port_name); */
          return STP_hop_2_state (s, ROLE_SELECTION);
        }
      }
      break;
  }

  return False;
}

void
STP_rolesel_update_stpm (STPM_T* this)
{
  register PORT_T* port;
  PRIO_VECTOR_T rootPathPrio;   /* 17.4.2.2 */

  stp_trace ("%s", "??? STP_rolesel_update_stpm ???");
  STP_VECT_create (&rootPathPrio, &this->BrId, 0, &this->BrId, 0, 0);

  if (!this->rootPortId ||
      STP_VECT_compare_vector (&rootPathPrio, &this->rootPrio) < 0) {
    STP_VECT_copy (&this->rootPrio, &rootPathPrio);
  }

  for (port = this->ports; port; port = port->next) {
    STP_VECT_create (&port->designPrio,
             &this->rootPrio.root_bridge,
             this->rootPrio.root_path_cost,
             &this->BrId, port->port_id, port->port_id);
    if (Received != port->infoIs || this->rootPortId == port->port_id) {
      STP_VECT_copy (&port->portPrio, &port->designPrio);
    }
    port->reselect = True;
    port->selected = False;
  }
}

