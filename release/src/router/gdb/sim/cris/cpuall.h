/* Simulator CPU header for cris.

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

#ifndef CRIS_CPUALL_H
#define CRIS_CPUALL_H

/* Include files for each cpu family.  */

#ifdef WANT_CPU_CRISV0F
#include "engv0.h"
#include "cgen-engine.h"
#include "cpuv0.h"
#include "decodev0.h"
#endif

#ifdef WANT_CPU_CRISV3F
#include "engv3.h"
#include "cgen-engine.h"
#include "cpuv3.h"
#include "decodev3.h"
#endif

#ifdef WANT_CPU_CRISV8F
#include "engv8.h"
#include "cgen-engine.h"
#include "cpuv8.h"
#include "decodev8.h"
#endif

#ifdef WANT_CPU_CRISV10F
#include "engv10.h"
#include "cgen-engine.h"
#include "cpuv10.h"
#include "decodev10.h"
#endif

#ifdef WANT_CPU_CRISV32F
#include "engv32.h"
#include "cgen-engine.h"
#include "cpuv32.h"
#include "decodev32.h"
#endif

extern const MACH crisv10_mach;
extern const MACH crisv32_mach;

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

#endif /* CRIS_CPUALL_H */
