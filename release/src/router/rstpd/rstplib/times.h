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

/* "Times" API : bridgeTime, rootTimes, portTimes, designatedTimes, msgTimes */

#ifndef _RSTP_TIMES_H__
#define _RSTP_TIMES_H__

typedef struct timevalues_t {
  unsigned short MessageAge;
  unsigned short MaxAge;
  unsigned short ForwardDelay;
  unsigned short HelloTime;
} TIMEVALUES_T;

int
STP_compare_times (IN TIMEVALUES_T* t1, IN TIMEVALUES_T* t2);

void
STP_get_times (IN BPDU_BODY_T* b, OUT TIMEVALUES_T* v);

void
STP_set_times (IN TIMEVALUES_T* v, OUT BPDU_BODY_T* b);

void
STP_copy_times (OUT TIMEVALUES_T* t, IN TIMEVALUES_T* f);

#endif /* _RSTP_TIMES_H__ */



