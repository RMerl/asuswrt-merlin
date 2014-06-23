/*****************************************************************************
  Copyright (c) 2006 EMC Corporation.

  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Authors: Srinivas Aji <Aji_Srinivas@emc.com>

******************************************************************************/

#ifndef CTL_FUNCTIONS_H
#define CTL_FUNCTIONS_H

#include <bitmap.h>
#include <uid_stp.h>

int CTL_enable_bridge_rstp(int br_index, int enable);

int CTL_get_bridge_state(int br_index,
			 UID_STP_CFG_T * cfg, UID_STP_STATE_T * state);

int CTL_set_bridge_config(int br_index, UID_STP_CFG_T * cfg);

int CTL_get_port_state(int br_index, int port_index,
		       UID_STP_PORT_CFG_T * cfg, UID_STP_PORT_STATE_T * state);

int CTL_set_port_config(int br_index, int port_index, UID_STP_PORT_CFG_T * cfg);

int CTL_set_debug_level(int level);

#define CTL_ERRORS \
 CHOOSE(Err_Interface_not_a_bridge), \
 CHOOSE(Err_Bridge_RSTP_not_enabled), \
 CHOOSE(Err_Bridge_is_down), \
 CHOOSE(Err_Port_does_not_belong_to_bridge), \

#define CHOOSE(a) a

enum Errors {
	Err_Dummy_Start = 1000,
	CTL_ERRORS Err_Dummy_End
};

#undef CHOOSE

const char *CTL_error_explanation(int err);

#endif
