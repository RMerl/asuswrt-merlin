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

/* Port Role Transitions state machine : 17.24 */
 
#include "base.h"

#include "stpm.h"

#define STATES { \
   CHOOSE(INIT_PORT),       \
   CHOOSE(BLOCK_PORT),      \
   CHOOSE(BLOCKED_PORT),    \
   CHOOSE(BACKUP_PORT),     \
   CHOOSE(ROOT_PROPOSED),   \
   CHOOSE(ROOT_AGREED),     \
   CHOOSE(REROOT),      \
   CHOOSE(ROOT_PORT),       \
   CHOOSE(REROOTED),        \
   CHOOSE(ROOT_LEARN),      \
   CHOOSE(ROOT_FORWARD),    \
   CHOOSE(DESIGNATED_PROPOSE),  \
   CHOOSE(DESIGNATED_SYNCED),   \
   CHOOSE(DESIGNATED_RETIRED),  \
   CHOOSE(DESIGNATED_PORT), \
   CHOOSE(DESIGNATED_LISTEN),   \
   CHOOSE(DESIGNATED_LEARN),    \
   CHOOSE(DESIGNATED_FORWARD),  \
}

#define GET_STATE_NAME STP_roletrns_get_state_name
#include "choose.h"

static void
setSyncBridge (STATE_MACH_T *this)
{
  register PORT_T* port;

  for (port = this->owner.port->owner->ports; port; port = port->next) {
    port->sync = True; /* in ROOT_PROPOSED (setSyncBridge) */
  }
}

static void
setReRootBridge (STATE_MACH_T *this)
{
  register PORT_T* port;

  for (port = this->owner.port->owner->ports; port; port = port->next) {
    port->reRoot = True; /* In setReRootBridge */
  }
}

static Bool
compute_all_synced (PORT_T* this)
{
  register PORT_T* port;

  for (port = this->owner->ports; port; port = port->next) {
    if (port->port_index == this->port_index) continue;
    if (! port->synced) {
        return False;
    }
  }

  return True;
}

static Bool
compute_re_rooted (PORT_T* this)
{
  register PORT_T* port;

  for (port = this->owner->ports; port; port = port->next) {
    if (port->port_index == this->port_index) continue;
    if (port->rrWhile) {
      return False;
    }
  }
  return True;
}

void
STP_roletrns_enter_state (STATE_MACH_T* this)
{
  register PORT_T*           port = this->owner.port;
  register STPM_T*           stpm;

  stpm = port->owner;

  switch (this->State) {
    case BEGIN:
    case INIT_PORT:
#if 0 /* due 802.1y Z.4 */
      port->role = DisabledPort;
#else
      port->role = port->selectedRole = DisabledPort;
      port->reselect = True;
#endif
      port->synced = False; /* in INIT */
      port->sync = True; /* in INIT */
      port->reRoot = True; /* in INIT_PORT */
      port->rrWhile = stpm->rootTimes.ForwardDelay;
      port->fdWhile = stpm->rootTimes.ForwardDelay;
      port->rbWhile = 0; 
#ifdef STP_DBG
      if (this->debug)
        STP_port_trace_flags ("after init", port);
#endif
      break;
    case BLOCK_PORT:
      port->role = port->selectedRole;
      port->learn =
      port->forward = False;
      break;
    case BLOCKED_PORT:
      port->fdWhile = stpm->rootTimes.ForwardDelay;
      port->synced = True; /* In BLOCKED_PORT */
      port->rrWhile = 0;
      port->sync = port->reRoot = False; /* BLOCKED_PORT */
      break;
    case BACKUP_PORT:
      port->rbWhile = 2 * stpm->rootTimes.HelloTime;
      break;

    /* 17.23.2 */
    case ROOT_PROPOSED:
      setSyncBridge (this);
      port->proposed = False;
#ifdef STP_DBG
      if (this->debug) 
        STP_port_trace_flags ("ROOT_PROPOSED", port);
#endif
      break;
    case ROOT_AGREED:
      port->proposed = port->sync = False; /* in ROOT_AGREED */
      port->synced = True; /* In ROOT_AGREED */
      port->newInfo = True;
#ifdef STP_DBG
      if (this->debug)
        STP_port_trace_flags ("ROOT_AGREED", port);
#endif
      break;
    case REROOT:
      setReRootBridge (this);
#ifdef STP_DBG
      if (this->debug)
        STP_port_trace_flags ("REROOT", port);
#endif
      break;
    case ROOT_PORT:
      port->role = RootPort;
      port->rrWhile = stpm->rootTimes.ForwardDelay;
#ifdef STP_DBG
      if (this->debug)
        STP_port_trace_flags ("ROOT_PORT", port);
#endif
      break;
    case REROOTED:
      port->reRoot = False; /* In REROOTED */
#ifdef STP_DBG
      if (this->debug)
        STP_port_trace_flags ("REROOTED", port);
#endif
      break;
    case ROOT_LEARN:
      port->fdWhile = stpm->rootTimes.ForwardDelay;
      port->learn = True;
#ifdef STP_DBG
      if (this->debug)
        STP_port_trace_flags ("ROOT_LEARN", port);
#endif
      break;
    case ROOT_FORWARD:
      port->fdWhile = 0;
      port->forward = True;
#ifdef STP_DBG
      if (this->debug)
        STP_port_trace_flags ("ROOT_FORWARD", port);
#endif
      break;

    /* 17.23.3 */
    case DESIGNATED_PROPOSE:
      port->proposing = True; /* in DESIGNATED_PROPOSE */
      port->newInfo = True;
#ifdef STP_DBG
      if (this->debug)
        STP_port_trace_flags ("DESIGNATED_PROPOSE", port);
#endif
      break;
    case DESIGNATED_SYNCED:
      port->rrWhile = 0;
      port->synced = True; /* DESIGNATED_SYNCED */
      port->sync = False; /* DESIGNATED_SYNCED */
#ifdef STP_DBG
      if (this->debug)
        STP_port_trace_flags ("DESIGNATED_SYNCED", port);
#endif
      break;
    case DESIGNATED_RETIRED:
      port->reRoot = False; /* DESIGNATED_RETIRED */
#ifdef STP_DBG
      if (this->debug)
        STP_port_trace_flags ("DESIGNATED_RETIRED", port);
#endif
      break;
    case DESIGNATED_PORT:
      port->role = DesignatedPort;
#ifdef STP_DBG
      if (this->debug)
        STP_port_trace_flags ("DESIGNATED_PORT", port);
#endif
      break;
    case DESIGNATED_LISTEN:
      port->learn = port->forward = False;
      port->fdWhile = stpm->rootTimes.ForwardDelay;
#ifdef STP_DBG
      if (this->debug)
        STP_port_trace_flags ("DESIGNATED_LISTEN", port);
#endif
      break;
    case DESIGNATED_LEARN:
      port->learn = True;
      port->fdWhile = stpm->rootTimes.ForwardDelay;
#ifdef STP_DBG
      if (this->debug)
        STP_port_trace_flags ("DESIGNATED_LEARN", port);
#endif
      break;
    case DESIGNATED_FORWARD:
      port->forward = True;
      port->fdWhile = 0;
#ifdef STP_DBG
      if (this->debug)
        STP_port_trace_flags ("DESIGNATED_FORWARD", port);
#endif
      break;
  };
}
    
Bool
STP_roletrns_check_conditions (STATE_MACH_T* this)
{
  register PORT_T           *port = this->owner.port;
  register STPM_T           *stpm;
  Bool                      allSynced;
  Bool                      allReRooted;

  stpm = port->owner;

  if (BEGIN == this->State) {
    return STP_hop_2_state (this, INIT_PORT);
  }

  if (port->role != port->selectedRole &&
      port->selected &&
      ! port->updtInfo) {
    switch (port->selectedRole) {
      case DisabledPort:
      case AlternatePort:
      case BackupPort:
#if 0 /* def STP_DBG */
        if (this->debug) {
          stp_trace ("hop to BLOCK_PORT role=%d selectedRole=%d",
                                (int) port->role, (int) port->selectedRole);
        }
#endif
        return STP_hop_2_state (this, BLOCK_PORT);
      case RootPort:
        return STP_hop_2_state (this, ROOT_PORT);
      case DesignatedPort:
        return STP_hop_2_state (this, DESIGNATED_PORT);
      default:
        return False;
    }
  }

  switch (this->State) {
    /* 17.23.1 */
    case INIT_PORT:
      return STP_hop_2_state (this, BLOCK_PORT);
    case BLOCK_PORT:
      if (!port->selected || port->updtInfo) break;
      if (!port->learning && !port->forwarding) {
        return STP_hop_2_state (this, BLOCKED_PORT);
      }
      break;
    case BLOCKED_PORT:
      if (!port->selected || port->updtInfo) break;
      if (port->fdWhile != stpm->rootTimes.ForwardDelay ||
          port->sync                ||
          port->reRoot              ||
          !port->synced) {
        return STP_hop_2_state (this, BLOCKED_PORT);
      }
      if (port->rbWhile != 2 * stpm->rootTimes.HelloTime &&
          port->role == BackupPort) {
        return STP_hop_2_state (this, BACKUP_PORT);
      }
      break;
    case BACKUP_PORT:
      return STP_hop_2_state (this, BLOCKED_PORT);

    /* 17.23.2 */
    case ROOT_PROPOSED:
      return STP_hop_2_state (this, ROOT_PORT);
    case ROOT_AGREED:
      return STP_hop_2_state (this, ROOT_PORT);
    case REROOT:
      return STP_hop_2_state (this, ROOT_PORT);
    case ROOT_PORT:
      if (!port->selected || port->updtInfo) break;
      if (!port->forward && !port->reRoot) {
        return STP_hop_2_state (this, REROOT);
      }
      allSynced = compute_all_synced (port);
      if ((port->proposed && allSynced) ||
          (!port->synced && allSynced)) {
        return STP_hop_2_state (this, ROOT_AGREED);
      }
      if (port->proposed && !port->synced) {
        return STP_hop_2_state (this, ROOT_PROPOSED);
      }

      allReRooted = compute_re_rooted (port);
      if ((!port->fdWhile || 
           ((allReRooted && !port->rbWhile) && stpm->ForceVersion >=2)) &&
          port->learn && !port->forward) {
        return STP_hop_2_state (this, ROOT_FORWARD);
      }
      if ((!port->fdWhile || 
           ((allReRooted && !port->rbWhile) && stpm->ForceVersion >=2)) &&
          !port->learn) {
        return STP_hop_2_state (this, ROOT_LEARN);
      }

      if (port->reRoot && port->forward) {
        return STP_hop_2_state (this, REROOTED);
      }
      if (port->rrWhile != stpm->rootTimes.ForwardDelay) {
        return STP_hop_2_state (this, ROOT_PORT);
      }
      break;
    case REROOTED:
      return STP_hop_2_state (this, ROOT_PORT);
    case ROOT_LEARN:
      return STP_hop_2_state (this, ROOT_PORT);
    case ROOT_FORWARD:
      return STP_hop_2_state (this, ROOT_PORT);

    /* 17.23.3 */
    case DESIGNATED_PROPOSE:
      return STP_hop_2_state (this, DESIGNATED_PORT);
    case DESIGNATED_SYNCED:
      return STP_hop_2_state (this, DESIGNATED_PORT);
    case DESIGNATED_RETIRED:
      return STP_hop_2_state (this, DESIGNATED_PORT);
    case DESIGNATED_PORT:
      if (!port->selected || port->updtInfo) break;

      if (!port->forward && !port->agreed && !port->proposing && !port->operEdge) {
        return STP_hop_2_state (this, DESIGNATED_PROPOSE);
      }

      if (!port->rrWhile && port->reRoot) {
        return STP_hop_2_state (this, DESIGNATED_RETIRED);
      }
      
      if (!port->learning && !port->forwarding && !port->synced) {
        return STP_hop_2_state (this, DESIGNATED_SYNCED);
      }

      if (port->agreed && !port->synced) {
        return STP_hop_2_state (this, DESIGNATED_SYNCED);
      }
      if (port->operEdge && !port->synced) {
        return STP_hop_2_state (this, DESIGNATED_SYNCED);
      }
      if (port->sync && port->synced) {
        return STP_hop_2_state (this, DESIGNATED_SYNCED);
      }

      if ((!port->fdWhile || port->agreed || port->operEdge) &&
          (!port->rrWhile  || !port->reRoot) &&
          !port->sync &&
          (port->learn && !port->forward)) {
        return STP_hop_2_state (this, DESIGNATED_FORWARD);
      }
      if ((!port->fdWhile || port->agreed || port->operEdge) &&
          (!port->rrWhile  || !port->reRoot) &&
          !port->sync && !port->learn) {
        return STP_hop_2_state (this, DESIGNATED_LEARN);
      }
      if (((port->sync && !port->synced) ||
           (port->reRoot && port->rrWhile)) &&
          !port->operEdge && (port->learn || port->forward)) {
        return STP_hop_2_state (this, DESIGNATED_LISTEN);
      }
      break;
    case DESIGNATED_LISTEN:
      return STP_hop_2_state (this, DESIGNATED_PORT);
    case DESIGNATED_LEARN:
      return STP_hop_2_state (this, DESIGNATED_PORT);
    case DESIGNATED_FORWARD:
      return STP_hop_2_state (this, DESIGNATED_PORT);
  };

  return False;
}


