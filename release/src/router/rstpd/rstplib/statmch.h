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

/* Generic (abstract state machine) state machine : 17.13, 17.14 */
 
#ifndef _STP_STATER_H__
#define _STP_STATER_H__

#define BEGIN  9999 /* distinct from any valid state */

typedef struct state_mach_t {
  struct state_mach_t* next;

  char*         name; /* for debugging */
#ifdef STP_DBG
  char          debug; /* 0- no dbg, 1 - port, 2 - stpm */
  unsigned int  ignoreHop2State;
#endif

  Bool          changeState;
  unsigned int  State;

  void          (* concreteEnterState) (struct state_mach_t * );
  Bool          (* concreteCheckCondition) (struct state_mach_t * );
  char*         (* concreteGetStatName) (int);
  union {
    struct stpm_t* stpm;
    struct port_t* port;
    void         * owner;
  } owner;

} STATE_MACH_T;

#define STP_STATE_MACH_IN_LIST(WHAT)                              \
  {                                                               \
    STATE_MACH_T* abstr;                                          \
                                                                  \
    abstr = STP_state_mach_create (STP_##WHAT##_enter_state,      \
                                  STP_##WHAT##_check_conditions,  \
                                  STP_##WHAT##_get_state_name,    \
                                  this,                           \
                                  #WHAT);                         \
    abstr->next = this->machines;                                 \
    this->machines = abstr;                                       \
    this->WHAT = abstr;                       \
  }


STATE_MACH_T *
STP_state_mach_create (void (* concreteEnterState) (STATE_MACH_T*),
                       Bool (* concreteCheckCondition) (STATE_MACH_T*),
                       char * (* concreteGetStatName) (int),
                       void* owner, char* name);
                     
void
STP_state_mach_delete (STATE_MACH_T* this);

Bool
STP_check_condition (STATE_MACH_T* this);

Bool
STP_change_state (STATE_MACH_T* this);

Bool
STP_hop_2_state (STATE_MACH_T* this, unsigned int new_state);

#endif /* _STP_STATER_H__ */

