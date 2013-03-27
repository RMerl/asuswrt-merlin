/*  This file is part of the program psim.

    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


/* branch macro's:

   The macro's below implement the semantics of the PowerPC jump
   instructions. */


/* If so required, update the Link Register with the next sequential
   instruction address */

#define UPDATE_LK \
do { \
  if (update_LK) { \
    ppc_ia target = cia + 4; \
    ppc_spr new_address = (ppc_spr)IEA_MASKED(ppc_is_64bit(processor), \
			                      target); \
    LR = new_address; \
  } \
  ITRACE(trace_branch, \
	 ("UPDATE_LK - update_LK=%d lr=0x%x cia=0x%x\n", \
	  update_LK, LR, cia); \
} while (0)


/* take the branch - absolute or relative - possibly updating the link
   register */

#define BRANCH(ADDRESS) \
do { \
  UPDATE_LK; \
  if (update_AA) { \
    ppc_ia target = (ppc_ia)(ADDRESS); \
    nia = (ppc_ia)IEA_MASKED(ppc_is_64bit(processor), target); \
  } \
  else { \
    ppc_ia target = cia + ADDRESS; \
    nia = (ppc_ia)IEA_MASKED(ppc_is_64bit(processor), target); \
  } \
  PTRACE(trace_branch, \
	 ("BRANCH - update_AA=%d update_LK=%d nia=0x%x cia=0x%x\n", \
	  update_AA, update_LK, nia, cia); \
} while (0)
