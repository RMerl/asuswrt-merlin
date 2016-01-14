/*
 * IS-IS Rout(e)ing protocol - isis_dr.h
 *                             IS-IS designated router related routines   
 *
 * Copyright (C) 2001,2002   Sampo Saaristo
 *                           Tampere University of Technology      
 *                           Institute of Communications Engineering
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public Licenseas published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 * more details.

 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _ZEBRA_ISIS_DR_H
#define _ZEBRA_ISIS_DR_H

int isis_run_dr_l1 (struct thread *thread);
int isis_run_dr_l2 (struct thread *thread);
int isis_dr_elect (struct isis_circuit *circuit, int level);
int isis_dr_resign (struct isis_circuit *circuit, int level);
int isis_dr_commence (struct isis_circuit *circuit, int level);
const char *isis_disflag2string (int disflag);

enum isis_dis_state
{
  ISIS_IS_NOT_DIS,
  ISIS_IS_DIS,
  ISIS_WAS_DIS,
  ISIS_UNKNOWN_DIS
};

#endif /* _ZEBRA_ISIS_DR_H */
