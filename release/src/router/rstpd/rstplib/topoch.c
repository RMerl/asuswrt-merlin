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

/* Topolgy Change state machine : 17.25 */
  
#include "base.h"
#include "stpm.h"
#include "stp_to.h" /* for STP_OUT_flush_lt */
  
#define STATES { \
  CHOOSE(INIT),             \
  CHOOSE(INACTIVE),         \
  CHOOSE(TCACTIVE),         \
  CHOOSE(DETECTED),         \
  CHOOSE(NOTIFIED_TC),          \
  CHOOSE(PROPAGATING),          \
  CHOOSE(ACKNOWLEDGED),         \
  CHOOSE(NOTIFIED_TCN),         \
}

#define GET_STATE_NAME STP_topoch_get_state_name
#include "choose.h"

/* We can flush learned fdb by port, so set this in stpm.c and topoch.c  */
/* This doesn't seem to solve the topology change problems. Don't use it yet */
//#define STRONGLY_SPEC_802_1W

#ifndef STRONGLY_SPEC_802_1W
/* 
 * In many kinds of hardware the function
 * STP_OUT_flush_lt is a) is very hard and b) cannot
 * delete learning emtries per port. The alternate
 * method may be used: we don't care operEdge flag here,
 * but clean learning table once for TopologyChange
 * for all ports, except the received port. I am ready to discuss :(
 * See below word STRONGLY_SPEC_802_1W
 */
#else
static Bool
flush (STATE_MACH_T *this, char* reason) /* 17.19.9 */
{
  register PORT_T* port = this->owner.port;
  Bool bret;

  if (port->operEdge) return True;
  if (this->debug) {
    stp_trace("%s (%s, %s, %s, '%s')",
        "flush", port->port_name, port->owner->name,
        "this port",
        reason);
  }

  bret = STP_OUT_flush_lt (port->port_index, port->owner->vlan_id,
                           LT_FLASH_ONLY_THE_PORT, reason);
  return bret;
}
#endif

static void
setTcPropBridge (STATE_MACH_T* this, char* reason) /* 17.19.14 */
{
  register PORT_T* port = this->owner.port;
  register PORT_T* tmp;

  for (tmp = port->owner->ports; tmp; tmp = tmp->next) {
    if (tmp->port_index != port->port_index)
      tmp->tcProp = True;
  }

#ifndef STRONGLY_SPEC_802_1W
#ifdef STP_DBG
  if (this->debug) {
    stp_trace("%s (%s, %s, %s, '%s')",
        "clearFDB", port->port_name, port->owner->name,
        "other ports", reason);
  }
#endif

  STP_OUT_flush_lt (port->port_index, port->owner->vlan_id,
                    LT_FLASH_ALL_PORTS_EXCLUDE_THIS, reason);
#endif
}

static unsigned int
newTcWhile (STATE_MACH_T* this) /* 17.19.7 */
{
  register PORT_T* port = this->owner.port;

  if (port->sendRSTP && port->operPointToPointMac) {
    return 2 * port->owner->rootTimes.HelloTime;
  }
#ifdef ORIG
  return port->owner->rootTimes.MaxAge;
#else
  return port->owner->rootTimes.MaxAge + port->owner->rootTimes.ForwardDelay;
#endif
}

void
STP_topoch_enter_state (STATE_MACH_T* this)
{
  register PORT_T*      port = this->owner.port;

  switch (this->State) {
    case BEGIN:
    case INIT:
#ifdef STRONGLY_SPEC_802_1W
      flush (this, "topoch INIT");
#endif
      port->tcWhile = 0;
      port->tc =
      port->tcProp =
      port->tcAck = False;
      break;
    case INACTIVE:
      port->rcvdTc =
      port->rcvdTcn =
      port->rcvdTcAck = port->tc = port->tcProp = False;
      break;
    case TCACTIVE:
      break;
    case DETECTED:
      port->tcWhile = newTcWhile (this);
#ifdef STP_DBG
  if (this->debug) 
    stp_trace("DETECTED: tcWhile=%d on port %s", 
        port->tcWhile, port->port_name);
#endif
      setTcPropBridge (this, "DETECTED");
      port->tc = False;  
      break;
    case NOTIFIED_TC:
      port->rcvdTcn = port->rcvdTc = False;
      if (port->role == DesignatedPort) {
        port->tcAck = True;
      }
      setTcPropBridge (this, "NOTIFIED_TC");
      break;
    case PROPAGATING:
      port->tcWhile = newTcWhile (this);
#ifdef STP_DBG
  if (this->debug) 
    stp_trace("PROPAGATING: tcWhile=%d on port %s", 
        port->tcWhile, port->port_name);
#endif
#ifdef STRONGLY_SPEC_802_1W
      flush (this, "topoch PROPAGATING");
#endif
      port->tcProp = False;
      break;
    case ACKNOWLEDGED:
      port->tcWhile = 0;
#ifdef STP_DBG
  if (this->debug) 
    stp_trace("ACKNOWLEDGED: tcWhile=%d on port %s", 
        port->tcWhile, port->port_name);
#endif
      port->rcvdTcAck = False;
      break;
    case NOTIFIED_TCN:
      port->tcWhile = newTcWhile (this);
#ifdef STP_DBG
  if (this->debug) 
    stp_trace("NOTIFIED_TCN: tcWhile=%d on port %s", 
        port->tcWhile, port->port_name);
#endif
      break;
  };
}

Bool
STP_topoch_check_conditions (STATE_MACH_T* this)
{
  register PORT_T*      port = this->owner.port;

  if (BEGIN == this->State) {
    return STP_hop_2_state (this, INIT);
  }

  switch (this->State) {
    case INIT:
      return STP_hop_2_state (this, INACTIVE);
    case INACTIVE:
      if (port->role == RootPort || port->role == DesignatedPort)
        return STP_hop_2_state (this, TCACTIVE);
      if (port->rcvdTc || port->rcvdTcn || port->rcvdTcAck ||
          port->tc || port->tcProp)
        return STP_hop_2_state (this, INACTIVE);
      break;
    case TCACTIVE:
      if (port->role != RootPort && (port->role != DesignatedPort))
        return STP_hop_2_state (this, INIT);
      if (port->tc)
        return STP_hop_2_state (this, DETECTED);
      if (port->rcvdTcn)
        return STP_hop_2_state (this, NOTIFIED_TCN);
      if (port->rcvdTc)
        return STP_hop_2_state (this, NOTIFIED_TC);
      if (port->tcProp && !port->operEdge)
        return STP_hop_2_state (this, PROPAGATING);
      if (port->rcvdTcAck)
        return STP_hop_2_state (this, ACKNOWLEDGED);
      break;
    case DETECTED:
      return STP_hop_2_state (this, TCACTIVE);
    case NOTIFIED_TC:
      return STP_hop_2_state (this, TCACTIVE);
    case PROPAGATING:
      return STP_hop_2_state (this, TCACTIVE);
    case ACKNOWLEDGED:
      return STP_hop_2_state (this, TCACTIVE);
    case NOTIFIED_TCN:
      return STP_hop_2_state (this, NOTIFIED_TC);
  };
  return False;
}

