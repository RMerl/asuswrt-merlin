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


#ifndef _INLINE_H_
#define _INLINE_H_


#define STATIC(TYPE) static TYPE


/* sim_endian is always inlined */

#if !defined(_SIM_ENDIAN_C_) && (SIM_ENDIAN_INLINE & INCLUDE_MODULE)
# if (SIM_ENDIAN_INLINE & INLINE_MODULE)
#  define INLINE_PSIM_ENDIAN(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_PSIM_ENDIAN(TYPE) static TYPE UNUSED
# else
#  define INLINE_PSIM_ENDIAN(TYPE) static TYPE UNUSED
#  define EXTERN_PSIM_ENDIAN(TYPE) static TYPE UNUSED
# endif
#else
# define INLINE_PSIM_ENDIAN(TYPE) TYPE
# define EXTERN_PSIM_ENDIAN(TYPE) TYPE
#endif

#if (SIM_ENDIAN_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_PSIM_ENDIAN(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_PSIM_ENDIAN(TYPE) static TYPE
#endif


/* bits is always inlined */

#if !defined(_BITS_C_) && (BITS_INLINE & INCLUDE_MODULE)
# if (BITS_INLINE & INLINE_MODULE)
#  define INLINE_BITS(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_BITS(TYPE) static TYPE UNUSED
# else
#  define INLINE_BITS(TYPE) static TYPE UNUSED
#  define EXTERN_BITS(TYPE) static TYPE UNUSED
# endif
#else
# define INLINE_BITS(TYPE) TYPE
# define EXTERN_BITS(TYPE) TYPE
#endif

#if (BITS_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_BITS(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_BITS(TYPE) static TYPE
#endif


/* core is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_CORE_C_) && (CORE_INLINE & INCLUDE_MODULE)
# if (CORE_INLINE & INLINE_MODULE)
#  define INLINE_CORE(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_CORE(TYPE) static TYPE UNUSED
#else
#  define INLINE_CORE(TYPE) static TYPE UNUSED
#  define EXTERN_CORE(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_CORE(TYPE) TYPE
# define EXTERN_CORE(TYPE) TYPE
#endif

#if (CORE_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_CORE(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_CORE(TYPE) static TYPE
#endif


/* vm is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_VM_C_) && (VM_INLINE & INCLUDE_MODULE)
# if (VM_INLINE & INLINE_MODULE)
#  define INLINE_VM(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_VM(TYPE) static TYPE UNUSED
#else
#  define INLINE_VM(TYPE) static TYPE UNUSED
#  define EXTERN_VM(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_VM(TYPE) TYPE
# define EXTERN_VM(TYPE) TYPE
#endif

#if (VM_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_VM(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_VM(TYPE) static TYPE
#endif


/* cpu is always inlined */

#if !defined(_CPU_C_) && (CPU_INLINE & INCLUDE_MODULE)
# if (CPU_INLINE & INLINE_MODULE)
#  define INLINE_CPU(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_CPU(TYPE) static TYPE UNUSED
#else
#  define INLINE_CPU(TYPE) static TYPE UNUSED
#  define EXTERN_CPU(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_CPU(TYPE) TYPE
# define EXTERN_CPU(TYPE) TYPE
#endif

#if (CPU_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_CPU(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_CPU(TYPE) static TYPE
#endif


/* model is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_MODEL_C_) && (MODEL_INLINE & INCLUDE_MODULE)
# if (MODEL_INLINE & INLINE_MODULE)
#  define INLINE_MODEL(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_MODEL(TYPE) static TYPE UNUSED
#else
#  define INLINE_MODEL(TYPE) static TYPE UNUSED
#  define EXTERN_MODEL(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_MODEL(TYPE) TYPE
# define EXTERN_MODEL(TYPE) TYPE
#endif

#if (MODEL_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_MODEL(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_MODEL(TYPE) static TYPE
#endif


/* events is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_EVENTS_C_) && (EVENTS_INLINE & INCLUDE_MODULE)
# if (EVENTS_INLINE & INLINE_MODULE)
#  define INLINE_EVENTS(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_EVENTS(TYPE) static TYPE UNUSED
#else
#  define INLINE_EVENTS(TYPE) static TYPE UNUSED
#  define EXTERN_EVENTS(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_EVENTS(TYPE) TYPE
# define EXTERN_EVENTS(TYPE) TYPE
#endif

#if (EVENTS_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_EVENTS(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_EVENTS(TYPE) static TYPE
#endif


/* mon is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_MON_C_) && (MON_INLINE & INCLUDE_MODULE)
# if (MON_INLINE & INLINE_MODULE)
#  define INLINE_MON(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_MON(TYPE) static TYPE UNUSED
#else
#  define INLINE_MON(TYPE) static TYPE UNUSED
#  define EXTERN_MON(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_MON(TYPE) TYPE
# define EXTERN_MON(TYPE) TYPE
#endif

#if (MON_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_MON(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_MON(TYPE) static TYPE
#endif


/* registers is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_REGISTERS_C_) && (REGISTERS_INLINE & INCLUDE_MODULE)
# if (REGISTERS_INLINE & INLINE_MODULE)
#  define INLINE_REGISTERS(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_REGISTERS(TYPE) static TYPE UNUSED
#else
#  define INLINE_REGISTERS(TYPE) static TYPE UNUSED
#  define EXTERN_REGISTERS(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_REGISTERS(TYPE) TYPE
# define EXTERN_REGISTERS(TYPE) TYPE
#endif

#if (REGISTERS_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_REGISTERS(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_REGISTERS(TYPE) static TYPE
#endif


/* interrupts is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_INTERRUPTS_C_) && (INTERRUPTS_INLINE & INCLUDE_MODULE)
# if (INTERRUPTS_INLINE & INLINE_MODULE)
#  define INLINE_INTERRUPTS(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_INTERRUPTS(TYPE) static TYPE UNUSED
#else
#  define INLINE_INTERRUPTS(TYPE) static TYPE UNUSED
#  define EXTERN_INTERRUPTS(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_INTERRUPTS(TYPE) TYPE
# define EXTERN_INTERRUPTS(TYPE) TYPE
#endif

#if (INTERRUPTS_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_INTERRUPTS(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_INTERRUPTS(TYPE) static TYPE
#endif


/* device is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_DEVICE_C_) && (DEVICE_INLINE & INCLUDE_MODULE)
# if (DEVICE_INLINE & INLINE_MODULE)
#  define INLINE_DEVICE(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_DEVICE(TYPE) static TYPE UNUSED
#else
#  define INLINE_DEVICE(TYPE) static TYPE UNUSED
#  define EXTERN_DEVICE(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_DEVICE(TYPE) TYPE
# define EXTERN_DEVICE(TYPE) TYPE
#endif

#if (DEVICE_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_DEVICE(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_DEVICE(TYPE) static TYPE
#endif


/* tree is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_TREE_C_) && (TREE_INLINE & INCLUDE_MODULE)
# if (TREE_INLINE & INLINE_MODULE)
#  define INLINE_TREE(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_TREE(TYPE) static TYPE UNUSED
#else
#  define INLINE_TREE(TYPE) static TYPE UNUSED
#  define EXTERN_TREE(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_TREE(TYPE) TYPE
# define EXTERN_TREE(TYPE) TYPE
#endif

#if (TREE_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_TREE(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_TREE(TYPE) static TYPE
#endif


/* spreg is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_SPREG_C_) && (SPREG_INLINE & INCLUDE_MODULE)
# if (SPREG_INLINE & INLINE_MODULE)
#  define INLINE_SPREG(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_SPREG(TYPE) static TYPE UNUSED
#else
#  define INLINE_SPREG(TYPE) static TYPE UNUSED
#  define EXTERN_SPREG(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_SPREG(TYPE) TYPE
# define EXTERN_SPREG(TYPE) TYPE
#endif

#if (SPREG_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_SPREG(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_SPREG(TYPE) static TYPE
#endif


/* semantics is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_SEMANTICS_C_) && (SEMANTICS_INLINE & INCLUDE_MODULE)
# if (SEMANTICS_INLINE & INLINE_MODULE)
#  define PSIM_INLINE_SEMANTICS(TYPE) static INLINE TYPE UNUSED
#  define PSIM_EXTERN_SEMANTICS(TYPE) static TYPE UNUSED REGPARM
#else
#  define PSIM_INLINE_SEMANTICS(TYPE) static TYPE UNUSED REGPARM
#  define PSIM_EXTERN_SEMANTICS(TYPE) static TYPE UNUSED REGPARM
#endif
#else
# define PSIM_INLINE_SEMANTICS(TYPE) TYPE REGPARM
# define PSIM_EXTERN_SEMANTICS(TYPE) TYPE REGPARM
#endif

#if 0 /* this isn't used */
#if (SEMANTICS_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_SEMANTICS(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_SEMANTICS(TYPE) static TYPE REGPARM
#endif
#endif


/* idecode is actually not inlined */

#if defined(_INLINE_C_) && !defined(_IDECODE_C_) && (IDECODE_INLINE & INCLUDE_MODULE)
# if (IDECODE_INLINE & INLINE_MODULE)
#  define PSIM_INLINE_IDECODE(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_IDECODE(TYPE) static TYPE UNUSED REGPARM
#else
#  define PSIM_INLINE_IDECODE(TYPE) static TYPE UNUSED REGPARM
#  define EXTERN_IDECODE(TYPE) static TYPE UNUSED REGPARM
#endif
#else
# define PSIM_INLINE_IDECODE(TYPE) TYPE REGPARM
# define EXTERN_IDECODE(TYPE) TYPE REGPARM
#endif

#if 0 /* this isn't used */
#if (IDECODE_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_IDECODE(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_IDECODE(TYPE) static TYPE REGPARM
#endif
#endif


/* icache is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_ICACHE_C_) && (ICACHE_INLINE & INCLUDE_MODULE)
# if (ICACHE_INLINE & INLINE_MODULE)
#  define PSIM_INLINE_ICACHE(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_ICACHE(TYPE) static TYPE UNUSED REGPARM
#else
#  define PSIM_INLINE_ICACHE(TYPE) static TYPE UNUSED REGPARM
#  define EXTERN_ICACHE(TYPE) static TYPE UNUSED REGPARM
#endif
#else
# define PSIM_INLINE_ICACHE(TYPE) TYPE REGPARM
# define EXTERN_ICACHE(TYPE) TYPE REGPARM
#endif

#if 0 /* this isn't used */
#if (ICACHE_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_ICACHE(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_ICACHE(TYPE) static TYPE REGPARM
#endif
#endif


/* support is always inlined */

#if !defined(_SUPPORT_C_) && (SUPPORT_INLINE & INCLUDE_MODULE)
# if (SUPPORT_INLINE & INLINE_MODULE)
#  define PSIM_INLINE_SUPPORT(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_SUPPORT(TYPE) static TYPE UNUSED
#else
#  define PSIM_INLINE_SUPPORT(TYPE) static TYPE UNUSED
#  define EXTERN_SUPPORT(TYPE) static TYPE UNUSED
#endif
#else
# define PSIM_INLINE_SUPPORT(TYPE) TYPE
# define EXTERN_SUPPORT(TYPE) TYPE
#endif

#if 0 /* this isn't used */
#if (SUPPORT_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_SUPPORT(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_SUPPORT(TYPE) static TYPE
#endif
#endif


/* options is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_OPTIONS_C_) && (OPTIONS_INLINE & INCLUDE_MODULE)
# if (OPTIONS_INLINE & INLINE_MODULE)
#  define INLINE_OPTIONS(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_OPTIONS(TYPE) static TYPE UNUSED
#else
#  define INLINE_OPTIONS(TYPE) static TYPE UNUSED
#  define EXTERN_OPTIONS(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_OPTIONS(TYPE) TYPE
# define EXTERN_OPTIONS(TYPE) TYPE
#endif

#if (OPTIONS_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_OPTIONS(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_OPTIONS(TYPE) static TYPE
#endif


/* os_emul is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_OS_EMUL_C_) && (OS_EMUL_INLINE & INCLUDE_MODULE)
# if (OS_EMUL_INLINE & INLINE_MODULE)
#  define INLINE_OS_EMUL(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_OS_EMUL(TYPE) static TYPE UNUSED
#else
#  define INLINE_OS_EMUL(TYPE) static TYPE UNUSED
#  define EXTERN_OS_EMUL(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_OS_EMUL(TYPE) TYPE
# define EXTERN_OS_EMUL(TYPE) TYPE
#endif

#if (OS_EMUL_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_OS_EMUL(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_OS_EMUL(TYPE) static TYPE
#endif


/* psim is actually not inlined */

#if defined(_INLINE_C_) && !defined(_PSIM_C_) && (PSIM_INLINE & INCLUDE_MODULE)
# if (PSIM_INLINE & INLINE_MODULE)
#  define INLINE_PSIM(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_PSIM(TYPE) static TYPE UNUSED
#else
#  define INLINE_PSIM(TYPE) static TYPE UNUSED
#  define EXTERN_PSIM(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_PSIM(TYPE) TYPE
# define EXTERN_PSIM(TYPE) TYPE
#endif

#if (PSIM_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_PSIM(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_PSIM(TYPE) static TYPE
#endif


/* cap is inlined with inline.c */

#if defined(_INLINE_C_) && !defined(_CAP_C_) && (CAP_INLINE & INCLUDE_MODULE)
# if (CAP_INLINE & INLINE_MODULE)
#  define INLINE_CAP(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_CAP(TYPE) static TYPE UNUSED
#else
#  define INLINE_CAP(TYPE) static TYPE UNUSED
#  define EXTERN_CAP(TYPE) static TYPE UNUSED
#endif
#else
# define INLINE_CAP(TYPE) TYPE
# define EXTERN_CAP(TYPE) TYPE
#endif

#if (CAP_INLINE & PSIM_INLINE_LOCALS)
# define STATIC_INLINE_CAP(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_CAP(TYPE) static TYPE
#endif

#endif
