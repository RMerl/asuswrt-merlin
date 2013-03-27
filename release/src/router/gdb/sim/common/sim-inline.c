/* The common simulator framework for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney and Red Hat.

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


#ifndef SIM_INLINE_C
#define SIM_INLINE_C

#undef SIM_INLINE_P
#define SIM_INLINE_P 1

#include "sim-inline.h"
#include "sim-main.h"


#if C_REVEALS_MODULE_P (SIM_BITS_INLINE)
#include "sim-bits.c"
#endif


#if C_REVEALS_MODULE_P (SIM_CORE_INLINE)
#include "sim-core.c"
#endif


#if C_REVEALS_MODULE_P (SIM_ENDIAN_INLINE)
#include "sim-endian.c"
#endif


#if C_REVEALS_MODULE_P (SIM_EVENTS_INLINE)
#include "sim-events.c"
#endif


#if C_REVEALS_MODULE_P (SIM_FPU_INLINE)
#include "sim-fpu.c"
#endif


#if C_REVEALS_MODULE_P (SIM_TYPES_INLINE)
#include "sim-types.c"
#endif


#if C_REVEALS_MODULE_P (SIM_MAIN_INLINE)
#include "sim-main.c"
#endif


#if C_REVEALS_MODULE_P (ENGINE_INLINE)
/* #include "engine.c" - handled by generator */
#endif


#if C_REVEALS_MODULE_P (ICACHE_INLINE)
/* #include "icache.c" - handled by generator */
#endif


#if C_REVEALS_MODULE_P (IDECODE_INLINE)
/* #include "idecode.c" - handled by generator */
#endif


#if C_REVEALS_MODULE_P (SEMANTICS_INLINE)
/* #include "semantics.c" - handled by generator */
#endif


#if C_REVEALS_MODULE_P (SUPPORT_INLINE)
/* #include "support.c" - handled by generator */
#endif


#undef SIM_INLINE_P
#define SIM_INLINE_P 0

#endif
