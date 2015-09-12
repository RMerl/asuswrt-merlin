/*
 * IS-IS Rout(e)ing protocol - isis_network.h   
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


#ifndef _ZEBRA_ISIS_NETWORK_H
#define _ZEBRA_ISIS_NETWORK_H

extern u_char ALL_L1_ISYSTEMS[];
extern u_char ALL_L2_ISYSTEMS[];

int isis_sock_init (struct isis_circuit *circuit);

int isis_recv_pdu_bcast (struct isis_circuit *circuit, u_char * ssnpa);
int isis_recv_pdu_p2p (struct isis_circuit *circuit, u_char * ssnpa);
int isis_send_pdu_bcast (struct isis_circuit *circuit, int level);
int isis_send_pdu_p2p (struct isis_circuit *circuit, int level);

#endif /* _ZEBRA_ISIS_NETWORK_H */
