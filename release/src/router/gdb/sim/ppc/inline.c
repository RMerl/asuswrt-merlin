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


#ifndef _INLINE_C_
#define _INLINE_C_

#include "config.h"
#include "ppc-config.h"

#include "inline.h"

#if (BITS_INLINE & INCLUDE_MODULE)
#include "bits.c"
#endif

#if (SIM_ENDIAN_INLINE & INCLUDE_MODULE)
#include "sim-endian.c"
#endif

#if (ICACHE_INLINE & INCLUDE_MODULE)
#include "icache.c"
#endif

#if (CORE_INLINE & INCLUDE_MODULE)
#include "corefile.c"
#endif

#if (VM_INLINE & INCLUDE_MODULE)
#include "vm.c"
#endif

#if (EVENTS_INLINE & INCLUDE_MODULE)
#include "events.c"
#endif

#if (MODEL_INLINE & INCLUDE_MODULE)
#include "model.c"
#endif

#if (OPTIONS_INLINE & INCLUDE_MODULE)
#include "options.c"
#endif

#if (MON_INLINE & INCLUDE_MODULE)
#include "mon.c"
#endif

#if (REGISTERS_INLINE & INCLUDE_MODULE)
#include "registers.c"
#endif

#if (INTERRUPTS_INLINE & INCLUDE_MODULE)
#include "interrupts.c"
#endif

#if (DEVICE_INLINE & INCLUDE_MODULE)
#include "device.c"
#endif

#if (TREE_INLINE & INCLUDE_MODULE)
#include "tree.c"
#endif

#if (SPREG_INLINE & INCLUDE_MODULE)
#include "spreg.c"
#endif

#if (SEMANTICS_INLINE & INCLUDE_MODULE)
#include "semantics.c"
#endif

#if (IDECODE_INLINE & INCLUDE_MODULE)
#include "idecode.c"
#endif

#if (OS_EMUL_INLINE & INCLUDE_MODULE)
#include "os_emul.c"
#endif

#endif
