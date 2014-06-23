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

#include "base.h"
#include "stpm.h"

/* The Port Information State Machine : 17.21 */

#define STATES { \
  CHOOSE(DISABLED), \
  CHOOSE(ENABLED),  \
  CHOOSE(AGED),     \
  CHOOSE(UPDATE),   \
  CHOOSE(CURRENT),  \
  CHOOSE(RECEIVE),  \
  CHOOSE(SUPERIOR), \
  CHOOSE(REPEAT),   \
  CHOOSE(AGREEMENT),    \
}

#define GET_STATE_NAME STP_info_get_state_name
#include "choose.h"

#if 0 /* for debug */
void
_stp_dump (char* title, unsigned char* buff, int len)
{
  register int iii;

  printf ("\n%s:", title);
  for (iii = 0; iii < len; iii++) {
    if (! (iii % 24)) Print ("\n%6d:", iii);
    if (! (iii % 8)) Print (" ");
    Print ("%02lx", (unsigned long) buff[iii]);
  }
  Print ("\n");
}
#endif

static RCVD_MSG_T
rcvBpdu (STATE_MACH_T* this)
{/* 17.19.8 */
  int   bridcmp;
  register PORT_T* port = this->owner.port;

  if (port->msgBpduType == BPDU_TOPO_CHANGE_TYPE) {
#ifdef STP_DBG
    if (this->debug) {
        stp_trace ("%s", "OtherMsg:BPDU_TOPO_CHANGE_TYPE");
    }
#endif
    return OtherMsg;
  }

  port->msgPortRole = RSTP_PORT_ROLE_UNKN;

  if (BPDU_RSTP == port->msgBpduType) {
    port->msgPortRole = (port->msgFlags & PORT_ROLE_MASK) >> PORT_ROLE_OFFS;
#ifndef ORIG
    if (RSTP_PORT_ROLE_UNKN == port->msgPortRole) {
      port->msgBpduVersion = FORCE_STP_COMPAT;
      port->msgBpduType = BPDU_CONFIG_TYPE;
    }
#endif
  }

  if (RSTP_PORT_ROLE_DESGN == port->msgPortRole ||
      BPDU_CONFIG_TYPE == port->msgBpduType) {
    bridcmp = STP_VECT_compare_vector (&port->msgPrio, &port->portPrio);

    if (bridcmp < 0 ||
        (! STP_VECT_compare_bridge_id (&port->msgPrio.design_bridge,
                                       &port->portPrio.design_bridge) &&
         port->msgPrio.design_port == port->portPrio.design_port      &&
         STP_compare_times (&port->msgTimes, &port->portTimes))) {
#ifdef STP_DBG
         if (this->debug) {
           stp_trace ("SuperiorDesignateMsg:bridcmp=%d", (int) bridcmp);
         }
#endif
      return SuperiorDesignateMsg;
    }
  }

  if (BPDU_CONFIG_TYPE == port->msgBpduType ||
      RSTP_PORT_ROLE_DESGN == port->msgPortRole) {
    if (! STP_VECT_compare_vector (&port->msgPrio,
                                   &port->portPrio) &&
        ! STP_compare_times (&port->msgTimes, &port->portTimes)) {
#ifdef STP_DBG
        if (this->debug) {
          stp_trace ("%s", "RepeatedDesignateMsg");
        }
#endif
        return RepeatedDesignateMsg;
    }
  }

  if (RSTP_PORT_ROLE_ROOT == port->msgPortRole                    &&
      port->operPointToPointMac                                   &&
      ! STP_VECT_compare_bridge_id (&port->msgPrio.root_bridge,
                                    &port->portPrio.root_bridge) &&
      port->msgPrio.root_path_cost == port->portPrio.root_path_cost &&
      ! STP_VECT_compare_bridge_id (&port->msgPrio.design_bridge,
                                    &port->portPrio.design_bridge) &&
      port->msgPrio.design_port == port->portPrio.design_port &&
      AGREEMENT_BIT & port->msgFlags) {
#ifdef STP_DBG
    if (this->debug) {
      stp_trace ("%s", "ConfirmedRootMsg");
    }
#endif
    return ConfirmedRootMsg;
  }
  
#ifdef STP_DBG
    if (this->debug) {
      stp_trace ("%s", "OtherMsg");
    }
#endif
  return OtherMsg;
}

static Bool
recordProposed (STATE_MACH_T* this, char* reason)
{/* 17.19.9 */
  register PORT_T* port = this->owner.port;

  if (RSTP_PORT_ROLE_DESGN == port->msgPortRole &&
      (PROPOSAL_BIT & port->msgFlags)           &&
      port->operPointToPointMac) {
    return True;
  }
  return False;
}

static Bool
setTcFlags (STATE_MACH_T* this)
{/* 17.19.13 */
  register PORT_T* port = this->owner.port;

  if (BPDU_TOPO_CHANGE_TYPE == port->msgBpduType) {
#ifdef STP_DBG
      if (this->debug) {
        stp_trace ("port %s rx rcvdTcn", port->port_name);
      }
#endif
    port->rcvdTcn = True;
  } else {
    if (TOLPLOGY_CHANGE_BIT & port->msgFlags) {
#ifdef STP_DBG
      if (this->debug) {
        stp_trace ("(%s-%s) rx rcvdTc 0X%lx",
            port->owner->name, port->port_name,
            (unsigned long) port->msgFlags);
      }
#endif
      port->rcvdTc = True;
    }
    if (TOLPLOGY_CHANGE_ACK_BIT & port->msgFlags) {
#ifdef STP_DBG
      if (this->debug) {
        stp_trace ("port %s rx rcvdTcAck 0X%lx",
            port->port_name,
            (unsigned long) port->msgFlags);
      }
#endif
      port->rcvdTcAck = True;
    }
  }

  return True;
}

static Bool
updtBPDUVersion (STATE_MACH_T* this)
{/* 17.19.18 */
  register PORT_T* port = this->owner.port;

  if (BPDU_TOPO_CHANGE_TYPE == port->msgBpduType) {
    port->rcvdSTP = True;
  }

  if (port->msgBpduVersion < 2) {
    port->rcvdSTP = True;
  }
  
  if (BPDU_RSTP == port->msgBpduType) {
    /* port->port->owner->ForceVersion >= NORMAL_RSTP
       we have checked in STP_info_rx_bpdu */
    port->rcvdRSTP = True;
  }

  return True;
}

static Bool
updtRcvdInfoWhile (STATE_MACH_T* this)
{/* 17.19.19 */
  register int eff_age, dm, dt;
  register int hello3;
  register PORT_T* port = this->owner.port;
  
  eff_age = ( + port->portTimes.MaxAge) / 16;
  if (eff_age < 1) eff_age = 1;
  eff_age += port->portTimes.MessageAge;

  if (eff_age <= port->portTimes.MaxAge) {
    hello3 = 3 *  port->portTimes.HelloTime;
    dm = port->portTimes.MaxAge - eff_age;
    if (dm > hello3)
      dt = hello3;
    else
      dt = dm;
    port->rcvdInfoWhile = dt;
/****
    stp_trace ("ma=%d eff_age=%d dm=%d dt=%d p=%s",
               (int) port->portTimes.MessageAge,
               (int) eff_age, (int) dm, (int) dt, port->port_name);
****/
  } else {
    port->rcvdInfoWhile = 0;
/****/
#ifdef STP_DBG
    /*if (this->debug) */
    {
      stp_trace ("port %s: MaxAge=%d MessageAge=%d HelloTime=%d rcvdInfoWhile=null !",
            port->port_name,
                (int) port->portTimes.MaxAge,
                (int) port->portTimes.MessageAge,
                (int) port->portTimes.HelloTime);
    }
#endif
/****/
  }

  return True;
}


void
STP_info_rx_bpdu (PORT_T* port, struct stp_bpdu_t* bpdu, size_t len)
{  
#if 0
  _stp_dump ("\nall BPDU", ((unsigned char*) bpdu) - 12, len + 12);
  _stp_dump ("ETH_HEADER", (unsigned char*) &bpdu->eth, 5);
  _stp_dump ("BPDU_HEADER", (unsigned char*) &bpdu->hdr, 4);
  printf ("protocol=%02x%02x version=%02x bpdu_type=%02x\n",
     bpdu->hdr.protocol[0], bpdu->hdr.protocol[1],
     bpdu->hdr.version, bpdu->hdr.bpdu_type);

  _stp_dump ("\nBPDU_BODY", (unsigned char*) &bpdu->body, sizeof (BPDU_BODY_T) + 2);
  printf ("flags=%02x\n", bpdu->body.flags);
  _stp_dump ("root_id", bpdu->body.root_id, 8);
  _stp_dump ("root_path_cost", bpdu->body.root_path_cost, 4);
  _stp_dump ("bridge_id", bpdu->body.bridge_id, 8);
  _stp_dump ("port_id", bpdu->body.port_id, 2);
  _stp_dump ("message_age", bpdu->body.message_age, 2);
  _stp_dump ("max_age", bpdu->body.max_age, 2);
  _stp_dump ("hello_time", bpdu->body.hello_time, 2);
  _stp_dump ("forward_delay", bpdu->body.forward_delay, 2);
  _stp_dump ("ver_1_len", bpdu->ver_1_len, 2);
#endif
  
  /* check bpdu type */
  switch (bpdu->hdr.bpdu_type) {
    case BPDU_CONFIG_TYPE:
      port->rx_cfg_bpdu_cnt++;
#if 0 /* def STP_DBG */
      if (port->info->debug) 
        stp_trace ("CfgBpdu on port %s", port->port_name);
#endif
      if (port->admin_non_stp) return;
      port->rcvdBpdu = True;
      break;
    case BPDU_TOPO_CHANGE_TYPE:
      port->rx_tcn_bpdu_cnt++;
#if 0 /* def STP_DBG */
      if (port->info->debug)
        stp_trace ("TcnBpdu on port %s", port->port_name);
#endif
      if (port->admin_non_stp) return;
      port->rcvdBpdu = True;
      port->msgBpduVersion = bpdu->hdr.version;
      port->msgBpduType = bpdu->hdr.bpdu_type;
      return;
    default:
      stp_trace ("RX undef bpdu type=%d", (int) bpdu->hdr.bpdu_type);
      return;
    case BPDU_RSTP:
      port->rx_rstp_bpdu_cnt++;
      if (port->admin_non_stp) return;
      if (port->owner->ForceVersion >= NORMAL_RSTP) {
        port->rcvdBpdu = True;
      } else {          
        return;
      }
#if 0 /* def STP_DBG */
      if (port->info->debug)
        stp_trace ("BPDU_RSTP on port %s", port->port_name);
#endif
      break;
  }

  port->msgBpduVersion = bpdu->hdr.version;
  port->msgBpduType =    bpdu->hdr.bpdu_type;
  port->msgFlags =       bpdu->body.flags;

  /* 17.18.11 */
  STP_VECT_get_vector (&bpdu->body, &port->msgPrio);
  port->msgPrio.bridge_port = port->port_id;

  /* 17.18.12 */
  STP_get_times (&bpdu->body, &port->msgTimes);

  /* 17.18.25, 17.18.26 : see setTcFlags() */
}

void STP_info_enter_state (STATE_MACH_T* this)
{
  register PORT_T* port = this->owner.port;

  switch (this->State) {
    case BEGIN:
      port->rcvdMsg = OtherMsg;
      port->msgBpduType = -1;
      port->msgPortRole = RSTP_PORT_ROLE_UNKN;
      port->msgFlags = 0;

      /* clear port statistics */
      port->rx_cfg_bpdu_cnt =
      port->rx_rstp_bpdu_cnt =
      port->rx_tcn_bpdu_cnt = 0;
      
    case DISABLED:
      port->rcvdBpdu = port->rcvdRSTP = port->rcvdSTP = False;
      port->updtInfo = port->proposing = False; /* In DISABLED */
      port->agreed = port->proposed = False;
      port->rcvdInfoWhile = 0;
      port->infoIs = Disabled;
      port->reselect = True;
      port->selected = False;
      break;
    case ENABLED: /* IEEE 802.1y, 17.21, Z.14 */
      STP_VECT_copy (&port->portPrio, &port->designPrio);
      STP_copy_times (&port->portTimes, &port->designTimes);
      break;
    case AGED:
      port->infoIs = Aged;
      port->reselect = True;
      port->selected = False;
      break;
    case UPDATE:
      STP_VECT_copy (&port->portPrio, &port->designPrio);
      STP_copy_times (&port->portTimes, &port->designTimes);
      port->updtInfo = False;
      port->agreed = port->synced = False; /* In UPDATE */
      port->proposed = port->proposing = False; /* in UPDATE */
      port->infoIs = Mine;
      port->newInfo = True;
#ifdef STP_DBG
      if (this->debug) {
        STP_VECT_br_id_print ("updated: portPrio.design_bridge",
                            &port->portPrio.design_bridge, True);
      }
#endif
      break;
    case CURRENT:
      break;
    case RECEIVE:
      port->rcvdMsg = rcvBpdu (this);
      updtBPDUVersion (this);
      setTcFlags (this);
      port->rcvdBpdu = False;
      break;
    case SUPERIOR:
      STP_VECT_copy (&port->portPrio, &port->msgPrio);
      STP_copy_times (&port->portTimes, &port->msgTimes);
      updtRcvdInfoWhile (this);
#if 1 /* due 802.1y, Z.7 */
      port->agreed = False; /* deleted due 802.y in SUPERIOR */
      port->synced = False; /* due 802.y deleted in SUPERIOR */
#endif
      port->proposing = False; /* in SUPERIOR */
      port->proposed = recordProposed (this, "SUPERIOR");
      port->infoIs = Received;
      port->reselect = True;
      port->selected = False;
#ifdef STP_DBG
      if (this->debug) {
        STP_VECT_br_id_print ("stored: portPrio.design_bridge",
                            &port->portPrio.design_bridge, True);
        stp_trace ("proposed=%d on port %s",
                   (int) port->proposed, port->port_name);
      }
#endif
      break;
    case REPEAT:
      port->proposed = recordProposed (this, "REPEAT");
      updtRcvdInfoWhile (this);
      break;
  case AGREEMENT:
#ifdef STP_DBG
      if (port->roletrns->debug) {
        stp_trace ("(%s-%s) rx AGREEMENT flag !",
            port->owner->name, port->port_name);
      }
#endif
      
      port->agreed = True;
      port->proposing = False; /* In AGREEMENT */
      break;
  }

}

Bool STP_info_check_conditions (STATE_MACH_T* this)
{
  register PORT_T* port = this->owner.port;

  if ((! port->portEnabled && port->infoIs != Disabled) || BEGIN == this->State) {
    return STP_hop_2_state (this, DISABLED);
  }

  switch (this->State) {
    case DISABLED:
      if (port->updtInfo) {
        return STP_hop_2_state (this, DISABLED);
      }
      if (port->portEnabled && port->selected) {
        return STP_hop_2_state (this, ENABLED);
      }
      if (port->rcvdBpdu) {
        return STP_hop_2_state (this, DISABLED);
      }
      break; 
    case ENABLED: /* IEEE 802.1y, 17.21, Z.14 */
      return STP_hop_2_state (this, AGED);
      break; 
    case AGED:
      if (port->selected && port->updtInfo) {
        return STP_hop_2_state (this, UPDATE);
      }
      break;
    case UPDATE:
      return STP_hop_2_state (this, CURRENT);
      break;
    case CURRENT:
      if (port->selected && port->updtInfo) {
        return STP_hop_2_state (this, UPDATE);
      }

      if (Received == port->infoIs       &&
          ! port->rcvdInfoWhile &&
          ! port->updtInfo               &&
          ! port->rcvdBpdu) {
        return STP_hop_2_state (this, AGED);
      }
      if (port->rcvdBpdu && !port->updtInfo) {
        return STP_hop_2_state (this, RECEIVE);
      }
      break;
    case RECEIVE:
      switch (port->rcvdMsg) {
        case SuperiorDesignateMsg:
          return STP_hop_2_state (this, SUPERIOR);
        case RepeatedDesignateMsg:
          return STP_hop_2_state (this, REPEAT);
        case ConfirmedRootMsg:
          return STP_hop_2_state (this, AGREEMENT);
        default:
          return STP_hop_2_state (this, CURRENT);
      }
      break;
    case SUPERIOR:
      return STP_hop_2_state (this, CURRENT);
      break;
    case REPEAT:
      return STP_hop_2_state (this, CURRENT);
      break;
    case AGREEMENT:
      return STP_hop_2_state (this, CURRENT);
      break;
  }

  return False;
}


