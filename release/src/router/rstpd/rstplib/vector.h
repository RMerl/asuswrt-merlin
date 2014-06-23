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

/* STP priority vectors API : 17.4.2 */
 
#ifndef _PRIO_VECTOR_H__
#define _PRIO_VECTOR_H__

typedef struct bridge_id
{
  unsigned short    prio;
  unsigned char     addr[6];
} BRIDGE_ID;

typedef unsigned short  PORT_ID;

typedef struct prio_vector_t {
  BRIDGE_ID root_bridge;
  unsigned long root_path_cost;
  BRIDGE_ID design_bridge;
  PORT_ID   design_port;
  PORT_ID   bridge_port;
} PRIO_VECTOR_T;

void 
STP_VECT_create (OUT PRIO_VECTOR_T* t,
                 IN BRIDGE_ID* root_br,
                 IN unsigned long root_path_cost,
                 IN BRIDGE_ID* design_bridge,
                 IN PORT_ID design_port,
                 IN PORT_ID bridge_port);
void
STP_VECT_copy (OUT PRIO_VECTOR_T* t, IN PRIO_VECTOR_T* f);

int
STP_VECT_compare_bridge_id (IN BRIDGE_ID* b1, IN BRIDGE_ID* b2);

int
STP_VECT_compare_vector (IN PRIO_VECTOR_T* v1, IN PRIO_VECTOR_T* v2);

void
STP_VECT_get_vector (IN BPDU_BODY_T* b, OUT PRIO_VECTOR_T* v);

void
STP_VECT_set_vector (IN PRIO_VECTOR_T* v, OUT BPDU_BODY_T* b);

#ifdef STP_DBG
void
STP_VECT_print (IN char* title, IN PRIO_VECTOR_T* v);

void
STP_VECT_br_id_print (IN char *title, IN BRIDGE_ID* br_id, IN Bool cr);

#endif

#endif /* _PRIO_VECTOR_H__ */


