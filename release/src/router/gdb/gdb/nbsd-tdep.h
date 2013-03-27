/* Common target-dependent definitions for NetBSD systems.
   Copyright (C) 2002, 2007 Free Software Foundation, Inc.
   Contributed by Wasabi Systems, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef NBSD_TDEP_H
#define NBSD_TDEP_H

struct link_map_offsets *nbsd_ilp32_solib_svr4_fetch_link_map_offsets (void);
struct link_map_offsets *nbsd_lp64_solib_svr4_fetch_link_map_offsets (void);

int nbsd_pc_in_sigtramp (CORE_ADDR, char *);

#endif /* NBSD_TDEP_H */
