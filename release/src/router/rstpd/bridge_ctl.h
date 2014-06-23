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

#ifndef BRIDGE_CTL_H
#define BRIDGE_CTL_H

struct ifdata;

int init_bridge_ops(void);

void bridge_get_configuration(void);

int bridge_set_state(int ifindex, int state);

int bridge_notify(int br_index, int if_index, int newlink, unsigned flags);

void bridge_bpdu_rcv(int ifindex, const unsigned char *data, int len);

void bridge_one_second(void);

#endif
