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

/* Generic (abstract) state machine : 17.13, 17.14 */
 
#include "base.h"
#include "statmch.h"

#if STP_DBG
#  include "stpm.h"
#endif

STATE_MACH_T *
STP_state_mach_create (void (*concreteEnterState) (STATE_MACH_T*),
                       Bool (*concreteCheckCondition) (STATE_MACH_T*),
                       char *(*concreteGetStatName) (int),
                       void *owner, char *name)
{
  STATE_MACH_T *this;

  STP_MALLOC(this, STATE_MACH_T, "state machine");
 
  this->State = BEGIN;
  this->name = (char*) strdup (name);
  this->changeState = False;
#if STP_DBG
  this->debug = False;
  this->ignoreHop2State = BEGIN;
#endif
  this->concreteEnterState = concreteEnterState;
  this->concreteCheckCondition = concreteCheckCondition;
  this->concreteGetStatName = concreteGetStatName;
  this->owner.owner = owner;

  return this;
}
                              
void
STP_state_mach_delete (STATE_MACH_T *this)
{
  free (this->name);
  STP_FREE(this, "state machine");
}

Bool
STP_check_condition (STATE_MACH_T* this)
{
  Bool bret;

  bret = (*(this->concreteCheckCondition)) (this);
  if (bret) {
    this->changeState = True;
  }
  
  return bret;
}
        
Bool
STP_change_state (STATE_MACH_T* this)
{
  register int number_of_loops;

  for (number_of_loops = 0; ; number_of_loops++) {
    if (! this->changeState) return number_of_loops;
    (*(this->concreteEnterState)) (this);
    this->changeState = False;
    STP_check_condition (this);
  }

  return number_of_loops;
}

Bool
STP_hop_2_state (STATE_MACH_T* this, unsigned int new_state)
{
#ifdef STP_DBG
  switch (this->debug) {
    case 0: break;
    case 1:
      if (new_state == this->State || new_state == this->ignoreHop2State) break;
      stp_trace ("%-8s(%s-%s): %s=>%s",
        this->name,
        *this->owner.port->owner->name ? this->owner.port->owner->name : "Glbl",
        this->owner.port->port_name,
        (*(this->concreteGetStatName)) (this->State),
        (*(this->concreteGetStatName)) (new_state));
      break;
    case 2:
      if (new_state == this->State) break;
      stp_trace ("%s(%s): %s=>%s", 
        this->name,
        *this->owner.stpm->name ? this->owner.stpm->name : "Glbl",
        (*(this->concreteGetStatName)) (this->State),
        (*(this->concreteGetStatName)) (new_state));
      break;
  }
#endif

  this->State = new_state;
  this->changeState = True;
  return True;
}

