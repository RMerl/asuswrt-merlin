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
#include "stp_in.h" /* for bridge defaults */
#include "stp_to.h"


int
STP_IN_stpm_create (int vlan_id, char* name, BITMAP_T* port_bmp)
{
  register STPM_T*  this;
  int               err_code;
  UID_STP_CFG_T		init_cfg;
		   
  stp_trace ("STP_IN_stpm_create(%s)", name);

  init_cfg.field_mask = BR_CFG_ALL;
  STP_OUT_get_init_stpm_cfg (vlan_id, &init_cfg);
  init_cfg.field_mask = 0;

  RSTP_CRITICAL_PATH_START;  
  this = stp_in_stpm_create (vlan_id, name, port_bmp, &err_code);
  if (this) {
    this->BrId.prio = init_cfg.bridge_priority;
    this->BrTimes.MaxAge = init_cfg.max_age;
    this->BrTimes.HelloTime = init_cfg.hello_time;
    this->BrTimes.ForwardDelay = init_cfg.forward_delay;
    this->ForceVersion = (PROTOCOL_VERSION_T) init_cfg.force_version;
  }
#ifdef ORIG
#else
  if (this->admin_state != STP_ENABLED)
    err_code = STP_stpm_enable(this, STP_ENABLED);
#endif

  RSTP_CRITICAL_PATH_END;
  return err_code;  
}

int
STP_IN_stpm_delete (int vlan_id)
{
  register STPM_T* this;
  int iret = 0;

  RSTP_CRITICAL_PATH_START;  
  this = stpapi_stpm_find (vlan_id);

  if (! this) { /* it had not yet been created :( */
    iret = STP_Vlan_Had_Not_Yet_Been_Created;
  } else {

    if (STP_ENABLED == this->admin_state) {
      if (0 != STP_stpm_enable (this, STP_DISABLED)) {/* can't disable :( */
        iret = STP_Another_Error;
      } else
        STP_OUT_set_hardware_mode (vlan_id, STP_DISABLED);
    }

    if (0 == iret) {
      STP_stpm_delete (this);   
    }
  }
  RSTP_CRITICAL_PATH_END;
  return iret;
}

int
STP_IN_stpm_get_vlan_id_by_name (char* name, int* vlan_id)
{
  register STPM_T* stpm;
  int iret = STP_Cannot_Find_Vlan;

  RSTP_CRITICAL_PATH_START;  
  for (stpm = STP_stpm_get_the_list (); stpm; stpm = stpm->next) {
    if (stpm->name && ! strcmp (stpm->name, name)) {
      *vlan_id = stpm->vlan_id;
      iret = 0;
      break;
    }
  }
  RSTP_CRITICAL_PATH_END;

  return iret;
}
    

Bool
STP_IN_get_is_stpm_enabled (int vlan_id)
{
  STPM_T* this;
  Bool iret = False;

  RSTP_CRITICAL_PATH_START;  
  this = stpapi_stpm_find (vlan_id);
  
  if (this) { 
    if (this->admin_state == STP_ENABLED) {
      iret = True;
    }
  } else {
    ;   /* it had not yet been created :( */
  }
  
  RSTP_CRITICAL_PATH_END;
  return iret;
}

int
STP_IN_stop_all (void)
{
  register STPM_T* stpm;

  RSTP_CRITICAL_PATH_START;
  
  for (stpm = STP_stpm_get_the_list (); stpm; stpm = stpm->next) {
    if (STP_DISABLED != stpm->admin_state) {
      STP_OUT_set_hardware_mode (stpm->vlan_id, STP_DISABLED);
      STP_stpm_enable (stpm, STP_DISABLED);
    }
  }

  RSTP_CRITICAL_PATH_END;
  return 0;
} 

int
STP_IN_delete_all (void)
{
  register STPM_T* stpm, *next;

  RSTP_CRITICAL_PATH_START;
  for (stpm = STP_stpm_get_the_list (); stpm; stpm = next) {
    next = stpm->next;
    STP_stpm_enable (stpm, STP_DISABLED);
    STP_stpm_delete (stpm);
  }

  RSTP_CRITICAL_PATH_END;
  return 0;
}

