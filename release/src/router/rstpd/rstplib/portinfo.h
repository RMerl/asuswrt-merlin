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

/* The Port Information State Machine : 17.21 */
 
#ifndef _STP_INFOR_H__
#define _STP_INFOR_H__

void
STP_info_enter_state (STATE_MACH_T* s);

Bool
STP_info_check_conditions (STATE_MACH_T* s);

void
STP_info_rx_bpdu (PORT_T* this, struct stp_bpdu_t* bpdu, size_t len);

char*
STP_info_get_state_name (int state);

#endif /* _STP_INFOR_H__ */
