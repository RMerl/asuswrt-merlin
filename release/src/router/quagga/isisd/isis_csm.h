/*
 * IS-IS Rout(e)ing protocol - isis_csm.h
 *                             IS-IS circuit state machine
 *
 * Copyright (C) 2001,2002   Sampo Saaristo
 *                           Tampere University of Technology      
 *                           Institute of Communications Engineering
 *
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
#ifndef _ZEBRA_ISIS_CSM_H
#define _ZEBRA_ISIS_CSM_H

/*
 * Circuit states
 */
#define C_STATE_NA   0
#define C_STATE_INIT 1		/* Connected to interface */
#define C_STATE_CONF 2		/* Configured for ISIS    */
#define C_STATE_UP   3		/* CONN | CONF            */

/*
 * Circuit events
 */
#define ISIS_ENABLE    1
#define IF_UP_FROM_Z   2
#define ISIS_DISABLE   3
#define IF_DOWN_FROM_Z 4

struct isis_circuit *isis_csm_state_change (int event,
					    struct isis_circuit *circuit,
					    void *arg);

#endif /* _ZEBRA_ISIS_CSM_H */
