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

/* STP PORT instance : 17.18, 17.15 */
 
#include "base.h"
#include "stpm.h"
#include "stp_in.h"

/* #include "rolesel.h" */
#include "portinfo.h"
#include "roletrns.h"
#include "sttrans.h"
#include "topoch.h"
#include "migrate.h"
#include "transmit.h"
#include "p2p.h"
#include "pcost.h"
#include "edge.h"

#include "stp_to.h" /* for STP_OUT_get_port_name & STP_OUT_get_port_link_status */

PORT_T *
STP_port_create (STPM_T* stpm, int port_index)
{
  PORT_T*        this;
  UID_STP_PORT_CFG_T port_cfg;
  register int   iii;
  unsigned short port_prio;

  /* check, if the port has just been added */
  for (this = stpm->ports; this; this = this->next) {
    if (this->port_index == port_index) {
      return NULL;
    }
  }

  STP_NEW_IN_LIST(this, PORT_T, stpm->ports, "port create");

  this->owner = stpm;
  this->machines = NULL;
  this->port_index = port_index;
  this->port_name = strdup (STP_OUT_get_port_name (port_index));
  this->uptime = 0;

  STP_OUT_get_init_port_cfg (stpm->vlan_id, port_index, &port_cfg);
  port_prio =                  port_cfg.port_priority;
  this->admin_non_stp =        port_cfg.admin_non_stp;
  this->adminEdge =            port_cfg.admin_edge;
  this->adminPCost =           port_cfg.admin_port_path_cost;
  this->adminPointToPointMac = port_cfg.admin_point2point;
  
  this->LinkDelay = DEF_LINK_DELAY;
  this->port_id = (port_prio << 8) + port_index;

  iii = 0;
  this->timers[iii++] = &this->fdWhile;
  this->timers[iii++] = &this->helloWhen;
  this->timers[iii++] = &this->mdelayWhile;
  this->timers[iii++] = &this->rbWhile;
  this->timers[iii++] = &this->rcvdInfoWhile;
  this->timers[iii++] = &this->rrWhile;
  this->timers[iii++] = &this->tcWhile;
  this->timers[iii++] = &this->txCount;
  this->timers[iii++] = &this->lnkWhile;

  /* create and bind port state machines */
  STP_STATE_MACH_IN_LIST(topoch);

  STP_STATE_MACH_IN_LIST(migrate);

  STP_STATE_MACH_IN_LIST(p2p);

  STP_STATE_MACH_IN_LIST(edge);
                  
  STP_STATE_MACH_IN_LIST(pcost)

  STP_STATE_MACH_IN_LIST(info);
                  
  STP_STATE_MACH_IN_LIST(roletrns);

  STP_STATE_MACH_IN_LIST(sttrans);

  STP_STATE_MACH_IN_LIST(transmit);
                  
#ifdef STP_DBG

#if 0
  this->roletrns->ignoreHop2State = 14; /* DESIGNATED_PORT; */
  this->info->ignoreHop2State =      3; /* CURRENT */
  this->transmit->ignoreHop2State =  3; /* IDLE */
  this->edge->ignoreHop2State =      0; /* DISABLED; */
#endif

#if 0
  this->info->debug = 1;
  this->pcost->debug = 1;
  this->p2p->debug = 1;
  this->edge->debug = 1;
  this->migrate->debug = 1;
  this->sttrans->debug = 1;
  this->topoch->debug = 1;
  this->roletrns->debug = 1;
#endif
  this->sttrans->debug = 1;

#endif
  return this;
}

void 
STP_port_init (PORT_T* this, STPM_T* stpm, Bool check_link)
{
  if (check_link) {
    this->adminEnable = STP_OUT_get_port_link_status (this->port_index);
    STP_VECT_create (&this->designPrio,
                   &stpm->BrId,
                   0,
                   &stpm->BrId,
                   this->port_id,
                   this->port_id);
    STP_copy_times (&this->designTimes, &stpm->rootTimes);
    this->fdWhile = 0;
  }

  /* reset timers */
  this->helloWhen =
  this->mdelayWhile =
  this->rbWhile =
  this->rcvdInfoWhile =
  this->rrWhile =
  this->tcWhile =
  this->txCount = 0;

  this->msgPortRole = RSTP_PORT_ROLE_UNKN;
  this->selectedRole = DisabledPort;
  this->sendRSTP = True;
  this->operSpeed = STP_OUT_get_port_oper_speed (this->port_index);
  this->p2p_recompute = True;
}

void
STP_port_delete (PORT_T* this)
{
  STPM_T*                   stpm;
  register PORT_T*          prev;
  register PORT_T*          tmp;
  register STATE_MACH_T*    stater;
  register void*            pv;

  stpm = this->owner;

  free (this->port_name);
  for (stater = this->machines; stater; ) {
    pv = (void*) stater->next;
    STP_state_mach_delete (stater);
    stater = (STATE_MACH_T*) pv;
  }
                 
  prev = NULL;
  for (tmp = stpm->ports; tmp; tmp = tmp->next) {
    if (tmp->port_index == this->port_index) {
      if (prev) {
        prev->next = this->next;
      } else {
        stpm->ports = this->next;
      }
      STP_FREE(this, "stp instance");
      break;
    }
    prev = tmp;
  }
}

int
STP_port_rx_bpdu (PORT_T* this, BPDU_T* bpdu, size_t len)
{
  STP_info_rx_bpdu (this, bpdu, len);

  return 0;
}

#ifdef STP_DBG
int STP_port_trace_state_machine (PORT_T* this, char* mach_name, int enadis, int vlan_id)
{
    register struct state_mach_t* stater;

    for (stater = this->machines; stater; stater = stater->next) {
        if (! strcmp (mach_name, "all") || ! strcmp (mach_name, stater->name)) {
            /* if (stater->debug != enadis) */
            {
                stp_trace ("port %s on %s trace %-8s (was %s) now %s",
                    this->port_name, this->owner->name,
                    stater->name,
                    stater->debug ? " enabled" :"disabled",
                    enadis        ? " enabled" :"disabled");
            }
            stater->debug = enadis;
        }
    }

    return 0;
}

void STP_port_trace_flags (char* title, PORT_T* this)
{
#if 0 /* it may be opened for more deep debugging */
    unsigned long flag = 0L;
    
    if (this->reRoot)   flag |= 0x000001L;
    if (this->sync)     flag |= 0x000002L;
    if (this->synced)   flag |= 0x000004L;

    if (this->proposed)  flag |= 0x000010L;
    if (this->proposing) flag |= 0x000020L;
    if (this->agreed)    flag |= 0x000040L;
    if (this->updtInfo)  flag |= 0x000080L;

    if (this->operEdge)   flag |= 0x000100L;
    stp_trace ("         %-12s: flags=0X%04lx port=%s", title, flag, this->port_name);
#endif
}

#endif
