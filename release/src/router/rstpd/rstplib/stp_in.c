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

/* This file contains API from an operation system to the RSTP library */

#include "base.h"
#include "stpm.h"
#include "stp_in.h"
#include "stp_to.h"

int max_port = 1024;

#define INCR100(nev) { nev++; if (nev > 99) nev = 0;}

RSTP_EVENT_T tev = RSTP_EVENT_LAST_DUMMY;
int			 nev = 0;

void *
stp_in_stpm_create (int vlan_id, char* name, BITMAP_T* port_bmp, int* err_code)
{
  int port_index;
  register STPM_T* this;

  /* stp_trace ("stp_in_stpm_create(%s)", name); */
  this = stpapi_stpm_find (vlan_id);
  if (this) { /* it had just been created :( */
    *err_code = STP_Nothing_To_Do;
    return this;
  }

  this = STP_stpm_create (vlan_id, name);
  if (! this) { /* can't create stpm :( */
    *err_code = STP_Cannot_Create_Instance_For_Vlan;
    return NULL;
  }

  for (port_index = 1; port_index <= max_port; port_index++) {
    if (BitmapGetBit(port_bmp, (port_index - 1))) {
      if (! STP_port_create (this, port_index)) {
        /* can't add port :( */
        stp_trace ("can't create port %d", (int) port_index);
        STP_stpm_delete (this);
        *err_code =STP_Cannot_Create_Instance_For_Port;
        return NULL;
      }
    }
  }

  *err_code = STP_OK;
  return this;
}

int
_stp_in_stpm_enable (int vlan_id, char* name,
                    BITMAP_T* port_bmp,
                    UID_STP_MODE_T admin_state)
{
  register STPM_T* this;
  Bool created_here = False;
  int rc, err_code;

  /* stp_trace ("_stp_in_stpm_enable(%s)", name); */
  this = stpapi_stpm_find (vlan_id);
  
  if (STP_DISABLED != admin_state) {
    if (! vlan_id) { /* STP_IN_stop_all (); */
        register STPM_T* stpm;

        for (stpm = STP_stpm_get_the_list (); stpm; stpm = stpm->next) {
          if (STP_DISABLED != stpm->admin_state) {
            STP_OUT_set_hardware_mode (stpm->vlan_id, STP_DISABLED);
            STP_stpm_enable (stpm, STP_DISABLED);
          }
        }
    }
  }

  if (! this) { /* it had not yet been created */
    if (STP_ENABLED == admin_state) {/* try to create it */
      stp_trace ("implicit create to vlan '%s'", name);
      this = stp_in_stpm_create (vlan_id, name, port_bmp, &err_code);
      if (! this) {
        stp_trace ("implicit create to vlan '%s' failed", name);
        return STP_Imlicite_Instance_Create_Failed;
      }
      created_here = True;
    } else {/* try to disable nothing ? */
      return 0;
    }
  }

  if (this->admin_state == admin_state) { /* nothing to do :) */
    return 0;
  }

  rc = STP_stpm_enable (this, admin_state);
  if (! rc) {
    STP_OUT_set_hardware_mode (vlan_id, admin_state);
  }

  if (rc && created_here) {
    STP_stpm_delete (this);
  }
    
  return rc;
}


STPM_T *
stpapi_stpm_find (int vlan_id)
{
  register STPM_T* this;

  for (this = STP_stpm_get_the_list (); this; this = this->next)
    if (vlan_id == this->vlan_id)
      return this;

  return NULL;
}

static PORT_T *
_stpapi_port_find (STPM_T* this, int port_index)
{
  register PORT_T* port;

  for (port = this->ports; port; port = port->next)
    if (port_index == port->port_index) {
      return port;
    }

  return NULL;
}


static void
_conv_br_id_2_uid (IN BRIDGE_ID* f, OUT UID_BRIDGE_ID_T* t)
{
  memcpy (t, f, sizeof (UID_BRIDGE_ID_T));
}

static int
_check_stpm_config (IN UID_STP_CFG_T* uid_cfg)
{
  if (uid_cfg->bridge_priority < MIN_BR_PRIO) {
    stp_trace ("%d bridge_priority small", (int) uid_cfg->bridge_priority);
    return STP_Small_Bridge_Priority;
  }

  if (uid_cfg->bridge_priority > MAX_BR_PRIO) {
    stp_trace ("%d bridge_priority large", (int) uid_cfg->bridge_priority);
    return STP_Large_Bridge_Priority;
  }

  if (uid_cfg->bridge_priority & ~MASK_BR_PRIO) {
    stp_trace ("%d bridge_priority must be a multiple of 4096", (int) uid_cfg->bridge_priority);
    return STP_Bridge_Priority_Not_A_Multiple_Of_4096;
  }

  if (uid_cfg->hello_time < MIN_BR_HELLOT) {
    stp_trace ("%d hello_time small", (int) uid_cfg->hello_time);
    return STP_Small_Hello_Time;
  }

  if (uid_cfg->hello_time > MAX_BR_HELLOT) {
    stp_trace ("%d hello_time large", (int) uid_cfg->hello_time);
    return STP_Large_Hello_Time;
  }

  if (uid_cfg->max_age < MIN_BR_MAXAGE) {
    stp_trace ("%d max_age small", (int) uid_cfg->max_age);
    return STP_Small_Max_Age;
  }

  if (uid_cfg->max_age > MAX_BR_MAXAGE) {
    stp_trace ("%d max_age large", (int) uid_cfg->max_age);
    return STP_Large_Max_Age;
  }

  if (uid_cfg->forward_delay < MIN_BR_FWDELAY) {
    stp_trace ("%d forward_delay small", (int) uid_cfg->forward_delay);
    return STP_Small_Forward_Delay;
  }

  if (uid_cfg->forward_delay > MAX_BR_FWDELAY) {
    stp_trace ("%d forward_delay large", (int) uid_cfg->forward_delay);
    return STP_Large_Forward_Delay;
  }

  if (2 * (uid_cfg->forward_delay - 1) < uid_cfg->max_age) {
    return STP_Forward_Delay_And_Max_Age_Are_Inconsistent;
  }

  if (uid_cfg->max_age < 2 * (uid_cfg->hello_time + 1)) {
    return STP_Hello_Time_And_Max_Age_Are_Inconsistent;
  }

  return 0;
}

static void
_stp_in_enable_port_on_stpm (STPM_T* stpm, int port_index, Bool enable)
{
  register PORT_T* port;

  port = _stpapi_port_find (stpm, port_index);
  if (! port) return; 
  if (port->portEnabled == enable) {/* nothing to do :) */
    return;
  }

  port->uptime = 0;
  if (enable) { /* clear port statistics */ 
    port->rx_cfg_bpdu_cnt =
    port->rx_rstp_bpdu_cnt =
    port->rx_tcn_bpdu_cnt = 0;
  }  

#ifdef STP_DBG
  if (port->edge->debug) {
    stp_trace ("Port %s became '%s' adminEdge=%c",
        port->port_name, enable ? "enable" : "disable",
        port->adminEdge ? 'Y' : 'N');
  }
#endif

  port->adminEnable = enable;
  STP_port_init (port, stpm, False);

  port->reselect = True;
  port->selected = False;  
}

void 
STP_IN_init (int max_port_index)
{
  max_port = max_port_index;
  RSTP_INIT_CRITICAL_PATH_PROTECTIO;
}

int
STP_IN_stpm_get_cfg (IN int vlan_id, OUT UID_STP_CFG_T* uid_cfg)
{
  register STPM_T* this;

  uid_cfg->field_mask = 0;
  
  RSTP_CRITICAL_PATH_START;  
  this = stpapi_stpm_find (vlan_id);

  if (!this) { /* it had not yet been created :( */
    RSTP_CRITICAL_PATH_END;
    return STP_Vlan_Had_Not_Yet_Been_Created;
  }

  if (this->admin_state != STP_DISABLED) {
    uid_cfg->field_mask |= BR_CFG_STATE;
  }
  uid_cfg->stp_enabled = this->admin_state;

  if (this->ForceVersion != 2) {
    uid_cfg->field_mask |= BR_CFG_FORCE_VER;
  }
  uid_cfg->force_version = this->ForceVersion;

  if (this->BrId.prio != DEF_BR_PRIO) {
    uid_cfg->field_mask |= BR_CFG_PRIO;
  }
  uid_cfg->bridge_priority = this->BrId.prio;

  if (this->BrTimes.MaxAge != DEF_BR_MAXAGE) {
    uid_cfg->field_mask |= BR_CFG_AGE;
  }
  uid_cfg->max_age = this->BrTimes.MaxAge;

  if (this->BrTimes.HelloTime != DEF_BR_HELLOT) {
    uid_cfg->field_mask |= BR_CFG_HELLO;
  }
  uid_cfg->hello_time = this->BrTimes.HelloTime;

  if (this->BrTimes.ForwardDelay != DEF_BR_FWDELAY) {
    uid_cfg->field_mask |= BR_CFG_DELAY;
  }
  uid_cfg->forward_delay = this->BrTimes.ForwardDelay;
  
  uid_cfg->hold_time = TxHoldCount;

  RSTP_CRITICAL_PATH_END;
  return 0;
}
    
int
STP_IN_port_get_cfg (int vlan_id, int port_index, UID_STP_PORT_CFG_T* uid_cfg)
{
  register STPM_T* this;
  register PORT_T* port;
  
  RSTP_CRITICAL_PATH_START;
  this = stpapi_stpm_find (vlan_id);
    
  if (!this) { /* it had not yet been created :( */
    RSTP_CRITICAL_PATH_END;
    return STP_Vlan_Had_Not_Yet_Been_Created;
  }

  port = _stpapi_port_find (this, port_index);
  if (! port) {/* port is absent in the stpm :( */
    RSTP_CRITICAL_PATH_END;
    return STP_Port_Is_Absent_In_The_Vlan;
  }

  uid_cfg->field_mask = 0;

  uid_cfg->port_priority = port->port_id >> 8;
  if (uid_cfg->port_priority != DEF_PORT_PRIO)
    uid_cfg->field_mask |= PT_CFG_PRIO;

  uid_cfg->admin_port_path_cost = port->adminPCost;
  if (uid_cfg->admin_port_path_cost != ADMIN_PORT_PATH_COST_AUTO)
    uid_cfg->field_mask |= PT_CFG_COST;

  uid_cfg->admin_point2point = port->adminPointToPointMac;
  if (uid_cfg->admin_point2point != DEF_P2P)
    uid_cfg->field_mask |= PT_CFG_P2P;

  uid_cfg->admin_edge = port->adminEdge;
  if (uid_cfg->admin_edge != DEF_ADMIN_EDGE)
    uid_cfg->field_mask |= PT_CFG_EDGE;
    
  RSTP_CRITICAL_PATH_END;
  return 0;
}

int
STP_IN_port_get_state (IN int vlan_id, INOUT UID_STP_PORT_STATE_T* entry)
{
  register STPM_T* this;
  register PORT_T* port;

  RSTP_CRITICAL_PATH_START;
  this = stpapi_stpm_find (vlan_id);

  if (!this) { /* it had not yet been created :( */
    RSTP_CRITICAL_PATH_END;
    return STP_Vlan_Had_Not_Yet_Been_Created;
  }

  port = _stpapi_port_find (this, entry->port_no);
  if (! port) {/* port is absent in the stpm :( */
    RSTP_CRITICAL_PATH_END;
    return STP_Port_Is_Absent_In_The_Vlan;
  }

  entry->port_id = port->port_id;
  if (DisabledPort == port->role) {
    entry->state = UID_PORT_DISABLED;
  } else if (! port->forward && ! port->learn) {
    entry->state = UID_PORT_DISCARDING;
  } else if (! port->forward && port->learn) {
    entry->state = UID_PORT_LEARNING;
  } else {
    entry->state = UID_PORT_FORWARDING;
  }

  entry->uptime = port->uptime;
  entry->path_cost = port->operPCost;
  _conv_br_id_2_uid (&port->portPrio.root_bridge, &entry->designated_root);
  entry->designated_cost = port->portPrio.root_path_cost;
  _conv_br_id_2_uid (&port->portPrio.design_bridge, &entry->designated_bridge);
  entry->designated_port = port->portPrio.design_port;

  switch (port->role) {
    case DisabledPort:   entry->role = ' '; break;
    case AlternatePort:  entry->role = 'A'; break;
    case BackupPort:     entry->role = 'B'; break;
    case RootPort:       entry->role = 'R'; break;
    case DesignatedPort: entry->role = 'D'; break;
    case NonStpPort:     entry->role = '-'; break;
    default:             entry->role = '?'; break;
  }

  if (DisabledPort == port->role || NonStpPort == port->role) {
    memset (&entry->designated_root, 0, sizeof (UID_BRIDGE_ID_T));
    memset (&entry->designated_bridge, 0, sizeof (UID_BRIDGE_ID_T));
    entry->designated_cost = 0;
    entry->designated_port = port->port_id;
  }

  if (DisabledPort == port->role) {
    entry->oper_point2point = (P2P_FORCE_FALSE == port->adminPointToPointMac) ? 0 : 1;
    entry->oper_edge = port->adminEdge;
    entry->oper_stp_neigb = 0;
  } else {
    entry->oper_point2point = port->operPointToPointMac ? 1 : 0;
    entry->oper_edge = port->operEdge                   ? 1 : 0;
    entry->oper_stp_neigb = port->sendRSTP              ? 0 : 1;
  }
  entry->oper_port_path_cost = port->operPCost;

  entry->rx_cfg_bpdu_cnt = port->rx_cfg_bpdu_cnt;
  entry->rx_rstp_bpdu_cnt = port->rx_rstp_bpdu_cnt;
  entry->rx_tcn_bpdu_cnt = port->rx_tcn_bpdu_cnt;

  entry->fdWhile =       port->fdWhile;      /* 17.15.1 */
  entry->helloWhen =     port->helloWhen;    /* 17.15.2 */
  entry->mdelayWhile =   port->mdelayWhile;  /* 17.15.3 */
  entry->rbWhile =       port->rbWhile;      /* 17.15.4 */
  entry->rcvdInfoWhile = port->rcvdInfoWhile;/* 17.15.5 */
  entry->rrWhile =       port->rrWhile;      /* 17.15.6 */
  entry->tcWhile =       port->tcWhile;      /* 17.15.7 */
  entry->txCount =       port->txCount;      /* 17.18.40 */
  entry->lnkWhile =      port->lnkWhile;

  entry->rcvdInfoWhile = port->rcvdInfoWhile;
  entry->top_change_ack = port->tcAck;
  entry->tc = port->tc;
  
  RSTP_CRITICAL_PATH_END;
  return 0; 
}

int
STP_IN_stpm_get_state (IN int vlan_id, OUT UID_STP_STATE_T* entry)
{
  register STPM_T* this;

  RSTP_CRITICAL_PATH_START;
  this = stpapi_stpm_find (vlan_id);

  if (!this) { /* it had not yet been created :( */
    RSTP_CRITICAL_PATH_END;
    return STP_Vlan_Had_Not_Yet_Been_Created;
  }

  strncpy (entry->vlan_name, this->name, NAME_LEN);
  entry->vlan_id = this->vlan_id;
  _conv_br_id_2_uid (&this->rootPrio.root_bridge, &entry->designated_root);
  entry->root_path_cost = this->rootPrio.root_path_cost;
  entry->root_port = this->rootPortId;
  entry->max_age =       this->rootTimes.MaxAge;
  entry->forward_delay = this->rootTimes.ForwardDelay;
  entry->hello_time =    this->rootTimes.HelloTime;

  _conv_br_id_2_uid (&this->BrId, &entry->bridge_id);

  entry->stp_enabled = this->admin_state;

  entry->timeSince_Topo_Change = this->timeSince_Topo_Change;
  entry->Topo_Change_Count = this->Topo_Change_Count;
  entry->Topo_Change = this->Topo_Change;
  
  RSTP_CRITICAL_PATH_END;
  return 0;
}

int
STP_IN_stpm_get_name_by_vlan_id (int vlan_id, char* name, size_t buffsize)
{
  register STPM_T* stpm;
  int iret = -1;

  RSTP_CRITICAL_PATH_START;
  for (stpm = STP_stpm_get_the_list (); stpm; stpm = stpm->next) {
    if (vlan_id ==  stpm->vlan_id) {
      if (stpm->name)
        strncpy (name, stpm->name, buffsize);
      else
        memset (name, 0, buffsize);
      iret = 0;
      break;
    }
  }
  RSTP_CRITICAL_PATH_END;
  return iret;
}

int /* call it, when link Up/Down */
STP_IN_enable_port (int port_index, Bool enable)
{
  register STPM_T* stpm;

  RSTP_CRITICAL_PATH_START;
  tev = enable ? RSTP_PORT_EN_T : RSTP_PORT_DIS_T; INCR100(nev);
  if (! enable) {
#ifdef STP_DBG
    stp_trace("%s (p%02d, all, %s, '%s')",
        "clearFDB", (int) port_index, "this port", "disable port");
#endif
    STP_OUT_flush_lt (port_index, 0, LT_FLASH_ONLY_THE_PORT, "disable port");
  }

  for (stpm = STP_stpm_get_the_list (); stpm; stpm = stpm->next) {
    if (STP_ENABLED != stpm->admin_state) continue;
    
    _stp_in_enable_port_on_stpm (stpm, port_index, enable);
  	/* STP_stpm_update (stpm);*/
  }

  RSTP_CRITICAL_PATH_END;
  return 0;
}

int /* call it, when port speed has been changed, speed in Kb/s  */
STP_IN_changed_port_speed (int port_index, long speed)
{
  register STPM_T* stpm;
  register PORT_T* port;

  RSTP_CRITICAL_PATH_START;
  tev = RSTP_PORT_SPEED_T; INCR100(nev);
  for (stpm = STP_stpm_get_the_list (); stpm; stpm = stpm->next) {
    if (STP_ENABLED != stpm->admin_state) continue;
    
    port = _stpapi_port_find (stpm, port_index);
    if (! port) continue; 
    port->operSpeed = speed;
#ifdef STP_DBG
    if (port->pcost->debug) {
      stp_trace ("changed operSpeed=%lu", port->operSpeed);
    }
#endif

    port->reselect = True;
    port->selected = False;
  }
  RSTP_CRITICAL_PATH_END;
  return 0;
}

int /* call it, when port duplex mode has been changed  */
STP_IN_changed_port_duplex (int port_index)
{
  register STPM_T* stpm;
  register PORT_T* port;

  RSTP_CRITICAL_PATH_START;
  tev = RSTP_PORT_DPLEX_T; INCR100(nev);
  for (stpm = STP_stpm_get_the_list (); stpm; stpm = stpm->next) {
    if (STP_ENABLED != stpm->admin_state) continue;
    
    port = _stpapi_port_find (stpm, port_index);
    if (! port) continue; 
#ifdef STP_DBG
    if (port->p2p->debug) {
      stp_trace ("STP_IN_changed_port_duplex(%s)", port->port_name);
    }
#endif
    port->p2p_recompute = True;
    port->reselect = True;
    port->selected = False;
  }
  RSTP_CRITICAL_PATH_END;
  return 0;
}

int
STP_IN_check_bpdu_header (BPDU_T* bpdu, size_t len)
{
  unsigned short len8023;

  len8023 = ntohs (*(unsigned short*) bpdu->eth.len8023);
  if (len8023 > 1500) {/* big len8023 format :( */
    return STP_Big_len8023_Format;
  }

  if (len8023 < MIN_BPDU) { /* small len8023 format :( */
    return STP_Small_len8023_Format;
  }

  if (len8023 + 14 > len) { /* len8023 format gt len :( */
    return STP_len8023_Format_Gt_Len;
  }

  if (bpdu->eth.dsap != BPDU_L_SAP                 ||
      bpdu->eth.ssap != BPDU_L_SAP                 ||
      bpdu->eth.llc != LLC_UI) {
    /* this is not a proper 802.3 pkt! :( */
    return STP_Not_Proper_802_3_Packet;
  }

  if (bpdu->hdr.protocol[0] || bpdu->hdr.protocol[1]) {
    return STP_Invalid_Protocol;
  }

#if 0
  if (bpdu->hdr.version != BPDU_VERSION_ID) {
    return STP_Invalid_Version;  
  }
#endif
  /* see also 9.3.4: think & TBD :( */
  return 0;
}

#ifdef STP_DBG
int dbg_rstp_deny = 0;
#endif


int
STP_IN_rx_bpdu (int vlan_id, int port_index, BPDU_T* bpdu, size_t len)
{
  register PORT_T* port;
  register STPM_T* this;
  int              iret;

#ifdef STP_DBG
  if (1 == dbg_rstp_deny) {
    return 0;
  }
#endif

  RSTP_CRITICAL_PATH_START;
  tev = RSTP_PORT_RX_T; INCR100(nev);
  this = stpapi_stpm_find (vlan_id);
  if (! this) { /*  the stpm had not yet been created :( */
    RSTP_CRITICAL_PATH_END;
    return STP_Vlan_Had_Not_Yet_Been_Created;
  }

  if (STP_DISABLED == this->admin_state) {/* the stpm had not yet been enabled :( */
    RSTP_CRITICAL_PATH_END;
    return STP_Had_Not_Yet_Been_Enabled_On_The_Vlan;
  }

  port = _stpapi_port_find (this, port_index);
  if (! port) {/* port is absent in the stpm :( */
    stp_trace ("RX bpdu vlan_id=%d port=%d port is absent in the stpm :(", (int) vlan_id, (int) port_index);
    RSTP_CRITICAL_PATH_END;
    return STP_Port_Is_Absent_In_The_Vlan;
  }

#ifdef STP_DBG
  if (port->skip_rx > 0) {
    if (1 == port->skip_rx)
      stp_trace ("port %s stop rx skipping",
                 port->port_name);
    else
      stp_trace ("port %s skip rx %d",
                 port->port_name, port->skip_rx);
    port->skip_rx--;
    RSTP_CRITICAL_PATH_END;
    return STP_Nothing_To_Do;
  }
#endif

  if (port->operEdge && ! port->lnkWhile && port->portEnabled) {
#ifdef STP_DBG
	  if (port->topoch->debug) {
    	stp_trace ("port %s tc=TRUE by operEdge", port->port_name);
	  }
#endif
    port->tc = True; /* IEEE 802.1y, 17.30 */
  }

  if (! port->portEnabled) {/* port link change indication will come later :( */
    _stp_in_enable_port_on_stpm (this, port->port_index, True);
  }
  
#ifdef STP_DBG
  if (port->edge->debug && port->operEdge) {
    stp_trace ("port %s not operEdge !", port->port_name);
  }
#endif

  port->operEdge = False;
  port->wasInitBpdu = True;
  
  iret = STP_port_rx_bpdu (port, bpdu, len);
  STP_stpm_update (this);
  RSTP_CRITICAL_PATH_END;

  return iret;
}

int
STP_IN_one_second (void)
{
  register STPM_T* stpm;
  register int     dbg_cnt = 0;

  RSTP_CRITICAL_PATH_START;
  tev = RSTP_PORT_TIME_T; INCR100(nev);
  for (stpm = STP_stpm_get_the_list (); stpm; stpm = stpm->next) {
    if (STP_ENABLED == stpm->admin_state) {
      /* stp_trace ("STP_IN_one_second vlan_id=%d", (int) stpm->vlan_id); */
      STP_stpm_one_second (stpm);
      dbg_cnt++;
    }
  }
  
  RSTP_CRITICAL_PATH_END;

  return dbg_cnt;
}

int
STP_IN_stpm_set_cfg (IN int vlan_id,
                     IN BITMAP_T* port_bmp,
                     IN UID_STP_CFG_T* uid_cfg)
{
  int rc = 0, prev_prio, err_code;
  Bool created_here, enabled_here;
  register STPM_T* this;
  UID_STP_CFG_T old;
           
  /* stp_trace ("STP_IN_stpm_set_cfg"); */
  if (0 != STP_IN_stpm_get_cfg (vlan_id, &old)) {
    STP_OUT_get_init_stpm_cfg (vlan_id, &old);
  }
  
  RSTP_CRITICAL_PATH_START;
  tev = RSTP_PORT_MNGR_T; INCR100(nev);
  if (BR_CFG_PRIO & uid_cfg->field_mask) {
    old.bridge_priority = uid_cfg->bridge_priority;
  }

  if (BR_CFG_AGE & uid_cfg->field_mask) {
    old.max_age = uid_cfg->max_age;
  }

  if (BR_CFG_HELLO & uid_cfg->field_mask) {
    old.hello_time = uid_cfg->hello_time;
  }

  if (BR_CFG_DELAY & uid_cfg->field_mask) {
    old.forward_delay = uid_cfg->forward_delay;
  }

  if (BR_CFG_FORCE_VER & uid_cfg->field_mask) {
    old.force_version = uid_cfg->force_version;
  }

  rc = _check_stpm_config (&old);
  if (0 != rc) {
    stp_trace ("_check_stpm_config failed %d", (int) rc);
    RSTP_CRITICAL_PATH_END;
    return rc;
  }

  if ((BR_CFG_STATE & uid_cfg->field_mask) &&
      (STP_DISABLED == uid_cfg->stp_enabled)) {
    rc = _stp_in_stpm_enable (vlan_id, uid_cfg->vlan_name, port_bmp, STP_DISABLED);
    if (0 != rc) {
      stp_trace ("can't disable rc=%d", (int) rc);
      RSTP_CRITICAL_PATH_END;
      return rc;
    }
    uid_cfg->field_mask &= ! BR_CFG_STATE;
    if (! uid_cfg->field_mask)  {
      RSTP_CRITICAL_PATH_END;
      return 0;
    }
  }

  /* get current state */
  this = stpapi_stpm_find (vlan_id);
  created_here = False;
  enabled_here = False;
  if (! this) { /* it had not yet been created */
    this = stp_in_stpm_create (vlan_id, uid_cfg->vlan_name, port_bmp, &err_code);/*STP_IN_stpm_set_cfg*/
    if (! this) {
      RSTP_CRITICAL_PATH_END;
      return err_code;
    }
  }

  prev_prio = this->BrId.prio;
  this->BrId.prio = old.bridge_priority;
  if (STP_ENABLED == this->admin_state) {
    if (0 != STP_stpm_check_bridge_priority (this)) {
      this->BrId.prio = prev_prio;
      stp_trace ("%s", "STP_stpm_check_bridge_priority failed");
      RSTP_CRITICAL_PATH_END;
      return STP_Invalid_Bridge_Priority;
    }
  }

  this->BrTimes.MaxAge = old.max_age;
  this->BrTimes.HelloTime = old.hello_time;
  this->BrTimes.ForwardDelay = old.forward_delay;
  this->ForceVersion = (PROTOCOL_VERSION_T) old.force_version;

  if ((BR_CFG_STATE & uid_cfg->field_mask) &&
      STP_DISABLED != uid_cfg->stp_enabled &&
      STP_DISABLED == this->admin_state) {
    rc = _stp_in_stpm_enable (vlan_id, uid_cfg->vlan_name, port_bmp, uid_cfg->stp_enabled);
    if (0 != rc) {
      stp_trace ("%s", "cannot enable");
      if (created_here) {
        STP_stpm_delete (this);
      }
      RSTP_CRITICAL_PATH_END;
      return rc;
    }
    enabled_here = True;
  }

  if (! enabled_here && STP_DISABLED != this->admin_state) {
    STP_stpm_update_after_bridge_management (this);
  }
  RSTP_CRITICAL_PATH_END;
  return 0;
}

#ifdef ORIG
int
STP_IN_set_port_cfg (IN int vlan_id, IN UID_STP_PORT_CFG_T* uid_cfg)
#else
int
STP_IN_set_port_cfg (int vlan_id, int port_index, UID_STP_PORT_CFG_T* uid_cfg)
#endif
{
  register STPM_T* this;
  register PORT_T* port;
  register int     port_no;

  RSTP_CRITICAL_PATH_START;
  tev = RSTP_PORT_MNGR_T; INCR100(nev);
  this = stpapi_stpm_find (vlan_id);
  if (! this) { /* it had not yet been created :( */
    RSTP_CRITICAL_PATH_END;
    Print ("RSTP instance with tag %d hasn't been created\n", (int) vlan_id);
    return STP_Vlan_Had_Not_Yet_Been_Created;
  }

#ifdef ORIG
  for (port_no = 1; port_no <= max_port; port_no++) {
    if (! BitmapGetBit(&uid_cfg->port_bmp, port_no - 1)) continue;
#else
  port_no = port_index;
  {
#endif
  
    port = _stpapi_port_find (this, port_no);
    if (! port) {/* port is absent in the stpm :( */
#ifdef ORIG
      continue;
#else
      return STP_Port_Is_Absent_In_The_Vlan;
#endif
    }

    if (PT_CFG_MCHECK & uid_cfg->field_mask) {
      if (this->ForceVersion >= NORMAL_RSTP)
        port->mcheck = True;
    }

    if (PT_CFG_COST & uid_cfg->field_mask) {
      if (uid_cfg->admin_port_path_cost > MAX_PORT_PCOST)
        return STP_Large_Port_PCost;
      port->adminPCost = uid_cfg->admin_port_path_cost;
    }
  
    if (PT_CFG_PRIO & uid_cfg->field_mask) {
      if (uid_cfg->port_priority < MIN_PORT_PRIO)
        return STP_Small_Port_Priority;
      if (uid_cfg->port_priority > MAX_PORT_PRIO)
        return STP_Large_Port_Priority;
      if (uid_cfg->port_priority & ~MASK_PORT_PRIO)
        return STP_Port_Priority_Not_A_Multiple_Of_16;
      port->port_id = (uid_cfg->port_priority << 8) + port_no;
    }
  
    if (PT_CFG_P2P & uid_cfg->field_mask) {
      port->adminPointToPointMac = uid_cfg->admin_point2point;
      port->p2p_recompute = True;
    }
  
    if (PT_CFG_EDGE & uid_cfg->field_mask) {
      port->adminEdge = uid_cfg->admin_edge;
      port->operEdge = port->adminEdge;
#ifdef STP_DBG
      if (port->edge->debug) {
        stp_trace ("port %s is operEdge=%c in STP_IN_set_port_cfg",
            port->port_name,
            port->operEdge ? 'Y' : 'n');
      }
#endif
    }

    if (PT_CFG_NON_STP & uid_cfg->field_mask) {
#ifdef STP_DBG
      if (port->roletrns->debug && port->admin_non_stp != uid_cfg->admin_non_stp) {
        stp_trace ("port %s is adminNonStp=%c in STP_IN_set_port_cfg",
            port->port_name,
            uid_cfg->admin_non_stp ? 'Y' : 'n');
      }
#endif
      port->admin_non_stp = uid_cfg->admin_non_stp;
    }

#ifdef STP_DBG
    if (PT_CFG_DBG_SKIP_RX & uid_cfg->field_mask) {
      port->skip_rx = uid_cfg->skip_rx;
    }

    if (PT_CFG_DBG_SKIP_TX & uid_cfg->field_mask) {
      port->skip_tx = uid_cfg->skip_tx;
    }

#endif

    port->reselect = True;
    port->selected = False;
  }
  
  STP_stpm_update (this);
  
  RSTP_CRITICAL_PATH_END;

  return 0;
}

#ifdef STP_DBG
int
STP_IN_dbg_set_port_trace (char* mach_name, int enadis,
                           int vlan_id, BITMAP_T* ports,
                           int is_print_err)
{
  register STPM_T* this;
  register PORT_T* port;
  register int     port_no;

  RSTP_CRITICAL_PATH_START;
  this = stpapi_stpm_find (vlan_id);
  if (! this) { /* it had not yet been created :( */
    RSTP_CRITICAL_PATH_END;
    if (is_print_err) {
        Print ("RSTP instance with tag %d hasn't been created\n", (int) vlan_id);
    }
    return STP_Vlan_Had_Not_Yet_Been_Created;
  }

  for (port_no = 1; port_no <= max_port; port_no++) {
    if (! BitmapGetBit(ports, port_no - 1)) continue;
  
    port = _stpapi_port_find (this, port_no);
    if (! port) {/* port is absent in the stpm :( */
      continue;
    }
    STP_port_trace_state_machine (port, mach_name, enadis, vlan_id);
  }
  
  RSTP_CRITICAL_PATH_END;

  return 0;
}

#endif

const char*
STP_IN_get_error_explanation (int rstp_err_no)
{
#define CHOOSE(a) #a
static char* rstp_error_names[] = RSTP_ERRORS;
#undef CHOOSE
  if (rstp_err_no < STP_OK) {
    return "Too small error code :(";
  }
  if (rstp_err_no >= STP_LAST_DUMMY) {
    return "Too big error code :(";
  }
  
  return rstp_error_names[rstp_err_no];
}

/*---------------- Dynamic port create / delete ------------------*/

int STP_IN_port_create(int vlan_id, int port_index)
{
  register STPM_T* this;

  this = stpapi_stpm_find (vlan_id);

  if (! this) { /* can't create stpm :( */
    return STP_Vlan_Had_Not_Yet_Been_Created;
  }

  PORT_T *port = STP_port_create (this, port_index);
  if (! port) {
    /* can't add port :( */
    stp_trace ("can't create port %d", (int) port_index);
    return STP_Cannot_Create_Instance_For_Port;
  }
  STP_port_init(port, this, True);

  STP_compute_bridge_id(this);
  STP_stpm_update_after_bridge_management (this);
  STP_stpm_update (this);
  return 0;
}

int STP_IN_port_delete(int vlan_id, int port_index)
{
  register STPM_T* this;
  PORT_T *port;

  this = stpapi_stpm_find (vlan_id);

  if (! this) { /* can't find stpm :( */
    return STP_Vlan_Had_Not_Yet_Been_Created;
  }

  port = _stpapi_port_find (this, port_index);
  if (! port) {
    return STP_Port_Is_Absent_In_The_Vlan;
  }
  
  STP_port_delete (port);

  STP_compute_bridge_id(this);
  STP_stpm_update_after_bridge_management (this);
  STP_stpm_update (this);
  return 0;
}


/*--- For multiple STP instances - non multithread use ---*/

struct stp_instance
{
  STPM_T *bridges;
#ifdef STP_DBG
  int dbg_rstp_deny;
#endif
  int max_port; /* Remove this */
  int nev;
  RSTP_EVENT_T tev;
};

struct stp_instance *STP_IN_instance_create(void)
{
  struct stp_instance *p;
  p = malloc(sizeof(*p));
  if (!p) return p;
  p->bridges = NULL;
#ifdef STP_DBG  
  p->dbg_rstp_deny = 0;
#endif
  p->max_port = 1024;
  p->tev = RSTP_EVENT_LAST_DUMMY;
  p->nev = 0;
  return p;
}

void STP_IN_instance_begin(struct stp_instance *p)
{
  bridges = p->bridges;
#ifdef STP_DBG  
  dbg_rstp_deny = p->dbg_rstp_deny;
#endif
  max_port = p->max_port;
  tev = p->tev;
  nev = p->nev;
}

void STP_IN_instance_end(struct stp_instance *p)
{
  p->bridges = bridges;
#ifdef STP_DBG  
  p->dbg_rstp_deny = dbg_rstp_deny;
#endif
  p->max_port = max_port;
  p->tev = tev;
  p->nev = nev;
}

void STP_IN_instance_delete(struct stp_instance *p)
{
  STP_IN_instance_begin(p);
  STP_IN_delete_all();
  STP_IN_instance_end(p);
  free(p);
}

  
  
