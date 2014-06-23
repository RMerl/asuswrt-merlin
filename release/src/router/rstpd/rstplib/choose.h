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

#ifndef _STP_CHOOSE_H__
#define _STP_CHOOSE_H__

/* State machines states & debug tools. Sorry, if these are no readable enogth :( */

#define CHOOSE(a) a
typedef enum STATES THE_STATE_T;
#undef CHOOSE

char * GET_STATE_NAME (int state)
{
#define CHOOSE(a) #a
static char    *state_names[] = STATES;
#undef CHOOSE

  if (BEGIN == state) return "Begin";
  return state_names[state];
}

#endif /* _STP_CHOOSE_H__ */
