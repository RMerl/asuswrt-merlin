/* Simulator support for frv.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996-2005 Free Software Foundation, Inc.

This file is part of the GNU simulators.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "sim-main.h"
#include "bfd.h"

const MACH *sim_machs[] =
{
#ifdef HAVE_CPU_FRVBF
  & frv_mach,
#endif
#ifdef HAVE_CPU_FRVBF
  & fr550_mach,
#endif
#ifdef HAVE_CPU_FRVBF
  & fr500_mach,
#endif
#ifdef HAVE_CPU_FRVBF
  & tomcat_mach,
#endif
#ifdef HAVE_CPU_FRVBF
  & fr400_mach,
#endif
#ifdef HAVE_CPU_FRVBF
  & fr450_mach,
#endif
#ifdef HAVE_CPU_FRVBF
  & simple_mach,
#endif
  0
};

