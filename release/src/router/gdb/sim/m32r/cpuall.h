/* Simulator CPU header for m32r.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

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

#ifndef M32R_CPUALL_H
#define M32R_CPUALL_H

/* Include files for each cpu family.  */

#ifdef WANT_CPU_M32RBF
#include "eng.h"
#include "cgen-engine.h"
#include "cpu.h"
#include "decode.h"
#endif

#ifdef WANT_CPU_M32RXF
#include "engx.h"
#include "cgen-engine.h"
#include "cpux.h"
#include "decodex.h"
#endif

#ifdef WANT_CPU_M32R2F
#include "eng2.h"
#include "cgen-engine.h"
#include "cpu2.h"
#include "decode2.h"
#endif

extern const MACH m32r_mach;
extern const MACH m32rx_mach;
extern const MACH m32r2_mach;

#ifndef WANT_CPU
/* The ARGBUF struct.  */
struct argbuf {
  /* These are the baseclass definitions.  */
  IADDR addr;
  const IDESC *idesc;
  char trace_p;
  char profile_p;
  /* ??? Temporary hack for skip insns.  */
  char skip_count;
  char unused;
  /* cpu specific data follows */
};
#endif

#ifndef WANT_CPU
/* A cached insn.

   ??? SCACHE used to contain more than just argbuf.  We could delete the
   type entirely and always just use ARGBUF, but for future concerns and as
   a level of abstraction it is left in.  */

struct scache {
  struct argbuf argbuf;
};
#endif

#endif /* M32R_CPUALL_H */
