/*
 * IS-IS Rout(e)ing protocol - topology/spgrid.h
                               Routines for manipulation of SSN and SRM flags
 * Copyright (C) 2001 Sampo Saaristo, Ofer Wald
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

/*
 * Based on:
 * SPLIB Copyright C 1994 by Cherkassky, Goldberg, and Radzik
 *
 */
#ifndef _ZEBRA_ISIS_TOPOLOGY_SPGRID_H
#define _ZEBRA_ISIS_TOPOLOGY_SPGRID_H

struct arc {
  long from_node;
  long to_node;
  long distance;
};

int           gen_spgrid_topology (struct vty *vty, struct list *topology);
int           spgrid_check_params (struct vty *vty, int argc, const char **argv);


#endif /* _ZEBRA_ISIS_TOPOLOGY_SPGRID_H */






