/*  This file is part of the program psim.

    Copyright 1994, 1995, 2002 Andrew Cagney <cagney@highland.com.au>

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


#ifndef _PSIM_CONFIG_H_
#define _PSIM_CONFIG_H_


/* endianness of the host/target:

   If the build process is aware (at compile time) of the endianness
   of the host/target it is able to eliminate slower generic endian
   handling code.

   Possible values are 0 (unknown), LITTLE_ENDIAN, BIG_ENDIAN */

#ifndef WITH_HOST_BYTE_ORDER
#define WITH_HOST_BYTE_ORDER		0 /*unknown*/
#endif

#ifndef WITH_TARGET_BYTE_ORDER
#define WITH_TARGET_BYTE_ORDER		0 /*unknown*/
#endif

extern int current_host_byte_order;
#define CURRENT_HOST_BYTE_ORDER (WITH_HOST_BYTE_ORDER \
				 ? WITH_HOST_BYTE_ORDER \
				 : current_host_byte_order)
extern int current_target_byte_order;
#define CURRENT_TARGET_BYTE_ORDER (WITH_TARGET_BYTE_ORDER \
				   ? WITH_TARGET_BYTE_ORDER \
				   : current_target_byte_order)


/* PowerPC XOR endian.

   In addition to the above, the simulator can support the PowerPC's
   horrible XOR endian mode.  This feature makes it possible to
   control the endian mode of a processor using the MSR. */

#ifndef WITH_XOR_ENDIAN
#define WITH_XOR_ENDIAN		8
#endif


/* Intel host BSWAP support:

   Whether to use bswap on the 486 and pentiums rather than the 386
   sequence that uses xchgb/rorl/xchgb */
#ifndef WITH_BSWAP
#define	WITH_BSWAP 0
#endif


/* SMP support:

   Sets a limit on the number of processors that can be simulated.  If
   WITH_SMP is set to zero (0), the simulator is restricted to
   suporting only on processor (and as a consequence leaves the SMP
   code out of the build process).

   The actual number of processors is taken from the device
   /options/smp@<nr-cpu> */

#ifndef WITH_SMP
#define WITH_SMP                        5
#endif
#if WITH_SMP
#define MAX_NR_PROCESSORS		WITH_SMP
#else
#define MAX_NR_PROCESSORS		1
#endif


/* Word size of host/target:

   Set these according to your host and target requirements.  At this
   point in time, I've only compiled (not run) for a 64bit and never
   built for a 64bit host.  This will always remain a compile time
   option */

#ifndef WITH_TARGET_WORD_BITSIZE
#define WITH_TARGET_WORD_BITSIZE        32 /* compiled only */
#endif

#ifndef WITH_HOST_WORD_BITSIZE
#define WITH_HOST_WORD_BITSIZE		32 /* 64bit ready? */
#endif


/* Program environment:

   Three environments are available - UEA (user), VEA (virtual) and
   OEA (perating).  The former two are environment that users would
   expect to see (VEA includes things like coherency and the time
   base) while OEA is what an operating system expects to see.  By
   setting these to specific values, the build process is able to
   eliminate non relevent environment code

   CURRENT_ENVIRONMENT specifies which of vea or oea is required for
   the current runtime. */

#define USER_ENVIRONMENT		1
#define VIRTUAL_ENVIRONMENT		2
#define OPERATING_ENVIRONMENT		3

#ifndef WITH_ENVIRONMENT
#define WITH_ENVIRONMENT		0
#endif

extern int current_environment;
#define CURRENT_ENVIRONMENT (WITH_ENVIRONMENT \
			     ? WITH_ENVIRONMENT \
			     : current_environment)


/* Optional VEA/OEA code: 

   The below, required for the OEA model may also be included in the
   VEA model however, as far as I can tell only make things
   slower... */


/* Events.  Devices modeling real H/W need to be able to efficiently
   schedule things to do at known times in the future.  The event
   queue implements this.  Unfortunatly this adds the need to check
   for any events once each full instruction cycle. */

#define WITH_EVENTS                     (WITH_ENVIRONMENT != USER_ENVIRONMENT)


/* Time base:

   The PowerPC architecture includes the addition of both a time base
   register and a decrement timer.  Like events adds to the overhead
   of of some instruction cycles. */

#ifndef WITH_TIME_BASE
#define WITH_TIME_BASE			(WITH_ENVIRONMENT != USER_ENVIRONMENT)
#endif


/* Callback/Default Memory.

   Core includes a builtin memory type (raw_memory) that is
   implemented using an array.  raw_memory does not require any
   additional functions etc.

   Callback memory is where the core calls a core device for the data
   it requires.

   Default memory is an extenstion of this where for addresses that do
   not map into either a callback or core memory range a default map
   can be used.

   The OEA model uses callback memory for devices and default memory
   for buses.

   The VEA model uses callback memory to capture `page faults'.

   While it may be possible to eliminate callback/default memory (and
   hence also eliminate an additional test per memory fetch) it
   probably is not worth the effort.

   BTW, while raw_memory could have been implemented as a callback,
   profiling has shown that there is a biger win (at least for the
   x86) in eliminating a function call for the most common
   (raw_memory) case. */

#define WITH_CALLBACK_MEMORY		1


/* Alignment:

   The PowerPC may or may not handle miss aligned transfers.  An
   implementation normally handles miss aligned transfers in big
   endian mode but generates an exception in little endian mode.

   This model.  Instead allows both little and big endian modes to
   either take exceptions or handle miss aligned transfers.

   If 0 is specified then for big-endian mode miss alligned accesses
   are permitted (NONSTRICT_ALIGNMENT) while in little-endian mode the
   processor will fault on them (STRICT_ALIGNMENT). */

#define NONSTRICT_ALIGNMENT    		1
#define STRICT_ALIGNMENT	       	2

#ifndef WITH_ALIGNMENT
#define WITH_ALIGNMENT     		0
#endif

extern int current_alignment;
#define CURRENT_ALIGNMENT (WITH_ALIGNMENT \
			   ? WITH_ALIGNMENT \
			   : current_alignment)


/* Floating point suport:

   Still under development. */

#define SOFT_FLOATING_POINT		1
#define HARD_FLOATING_POINT		2

#ifndef WITH_FLOATING_POINT
#define WITH_FLOATING_POINT		HARD_FLOATING_POINT
#endif
extern int current_floating_point;
#define CURRENT_FLOATING_POINT (WITH_FLOATING_POINT \
				? WITH_FLOATING_POINT \
				: current_floating_point)


/* Debugging:

   Control the inclusion of debugging code. */

/* Include the tracing code.  Disabling this eliminates all tracing
   code */

#ifndef WITH_TRACE
#define WITH_TRACE                      1
#endif

/* include code that checks assertions scattered through out the
   program */

#ifndef WITH_ASSERT
#define WITH_ASSERT			1
#endif

/* Whether to check instructions for reserved bits being set */

#ifndef WITH_RESERVED_BITS
#define WITH_RESERVED_BITS		1
#endif

/* include monitoring code */

#define MONITOR_INSTRUCTION_ISSUE	1
#define MONITOR_LOAD_STORE_UNIT		2
#ifndef WITH_MON
#define WITH_MON			(MONITOR_LOAD_STORE_UNIT \
					 | MONITOR_INSTRUCTION_ISSUE)
#endif

/* Current CPU model (models are in the generated models.h include file)  */
#ifndef WITH_MODEL
#define WITH_MODEL			0
#endif

#define CURRENT_MODEL (WITH_MODEL	\
		       ? WITH_MODEL	\
		       : current_model)

#ifndef WITH_DEFAULT_MODEL
#define WITH_DEFAULT_MODEL		DEFAULT_MODEL
#endif

#define MODEL_ISSUE_IGNORE		(-1)
#define MODEL_ISSUE_PROCESS		1

#ifndef WITH_MODEL_ISSUE
#define WITH_MODEL_ISSUE		0
#endif

extern int current_model_issue;
#define CURRENT_MODEL_ISSUE (WITH_MODEL_ISSUE	\
			     ? WITH_MODEL_ISSUE	\
			     : current_model_issue)

/* Whether or not input/output just uses stdio, or uses printf_filtered for
   output, and polling input for input.  */

#define DONT_USE_STDIO			2
#define DO_USE_STDIO			1

#ifndef WITH_STDIO
#define WITH_STDIO			0
#endif

extern int current_stdio;
#define CURRENT_STDIO (WITH_STDIO	\
		       ? WITH_STDIO     \
		       : current_stdio)



/* INLINE CODE SELECTION:

   GCC -O3 attempts to inline any function or procedure in scope.  The
   options below facilitate fine grained control over what is and what
   isn't made inline.  For instance it can control things down to a
   specific modules static routines.  Doing this allows the compiler
   to both eliminate the overhead of function calls and (as a
   consequence) also eliminate further dead code.

   On a CISC (x86) I've found that I can achieve an order of magintude
   speed improvement (x3-x5).  In the case of RISC (sparc) while the
   performance gain isn't as great it is still significant.

   Each module is controled by the macro <module>_INLINE which can
   have the values described below

       0  Do not inline any thing for the given module

   The following additional values are `bit fields' and can be
   combined.

      REVEAL_MODULE:

         Include the C file for the module into the file being compiled
         but do not make the functions within the module inline.

	 While of no apparent benefit, this makes it possible for the
	 included module, when compiled to inline its calls to what
	 would otherwize be external functions.

      INLINE_MODULE:

         Make external functions within the module `inline'.  Thus if
         the module is included into a file being compiled, calls to
	 its funtions can be eliminated. 2 implies 1.

      PSIM_INLINE_LOCALS:

         Make internal (static) functions within the module `inline'.

   The following abreviations are available:

      INCLUDE_MODULE == (REVEAL_MODULE | INLINE_MODULE)

      ALL_INLINE == (REVEAL_MODULE | INLINE_MODULE | PSIM_INLINE_LOCALS)

   In addition to this, modules have been put into two categories.

         Simple modules - eg sim-endian.h bits.h

	 Because these modules are small and simple and do not have
	 any complex interpendencies they are configured, if
	 <module>_INLINE is so enabled, to inline themselves in all
	 modules that include those files.

	 For the default build, this is a real win as all byte
	 conversion and bit manipulation functions are inlined.

	 Complex modules - the rest

	 These are all handled using the files inline.h and inline.c.
	 psim.c includes the above which in turn include any remaining
	 code.

   IMPLEMENTATION:

   The inline ability is enabled by prefixing every data / function
   declaration and definition with one of the following:


       INLINE_<module>

       Prefix to any global function that is a candidate for being
       inline.

       values - `', `static', `static INLINE'


       EXTERN_<module>
      
       Prefix to any global data structures for the module.  Global
       functions that are not to be inlined shall also be prefixed
       with this.

       values - `', `static', `static'


       STATIC_INLINE_<module>

       Prefix to any local (static) function that is a candidate for
       being made inline.

       values - `static', `static INLINE'


       static

       Prefix all local data structures.  Local functions that are not
       to be inlined shall also be prefixed with this.

       values - `static', `static'

       nb: will not work for modules that are being inlined for every
       use (white lie).


       extern
       #ifndef _INLINE_C_
       #endif
       
       Prefix to any declaration of a global object (function or
       variable) that should not be inlined and should have only one
       definition.  The #ifndef wrapper goes around the definition
       propper to ensure that only one copy is generated.

       nb: this will not work when a module is being inlined for every
       use.


       STATIC_<module>

       Replaced by either `static' or `EXTERN_MODULE'.


   REALITY CHECK:

   This is not for the faint hearted.  I've seen GCC get up to 500mb
   trying to compile what this can create.

   Some of the modules do not yet implement the WITH_INLINE_STATIC
   option.  Instead they use the macro STATIC_INLINE to control their
   local function.

   Because of the way that GCC parses __attribute__(), the macro's
   need to be adjacent to the function name rather than at the start
   of the line vis:

   	int STATIC_INLINE_MODULE f(void);
	void INLINE_MODULE *g(void);

   */

#define REVEAL_MODULE			1
#define INLINE_MODULE			2
#define INCLUDE_MODULE			(INLINE_MODULE | REVEAL_MODULE)
#define PSIM_INLINE_LOCALS			4
#define ALL_INLINE			7

/* Your compilers inline reserved word */

#ifndef INLINE
#if defined(__GNUC__) && defined(__OPTIMIZE__)
#define INLINE __inline__
#else
#define INLINE /*inline*/
#endif
#endif


/* Your compilers pass parameters in registers reserved word */

#ifndef WITH_REGPARM
#define WITH_REGPARM                   0
#endif

/* Your compilers use an alternative calling sequence reserved word */

#ifndef WITH_STDCALL
#define WITH_STDCALL                   0
#endif

#if !defined REGPARM
#if defined(__GNUC__) && (defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__))
#if (WITH_REGPARM && WITH_STDCALL)
#define REGPARM __attribute__((__regparm__(WITH_REGPARM),__stdcall__))
#else
#if (WITH_REGPARM && !WITH_STDCALL)
#define REGPARM __attribute__((__regparm__(WITH_REGPARM)))
#else
#if (!WITH_REGPARM && WITH_STDCALL)
#define REGPARM __attribute__((__stdcall__))
#endif
#endif
#endif
#endif
#endif

#if !defined REGPARM
#define REGPARM
#endif



/* Default prefix for static functions */

#ifndef STATIC_INLINE
#define STATIC_INLINE static INLINE
#endif

/* Default macro to simplify control several of key the inlines */

#ifndef DEFAULT_INLINE
#define	DEFAULT_INLINE			PSIM_INLINE_LOCALS
#endif

/* Code that converts between hosts and target byte order.  Used on
   every memory access (instruction and data).  See sim-endian.h for
   additional byte swapping configuration information.  This module
   can inline for all callers */

#ifndef SIM_ENDIAN_INLINE
#define SIM_ENDIAN_INLINE		(DEFAULT_INLINE ? ALL_INLINE : 0)
#endif

/* Low level bit manipulation routines. This module can inline for all
   callers */

#ifndef BITS_INLINE
#define BITS_INLINE			(DEFAULT_INLINE ? ALL_INLINE : 0)
#endif

/* Code that gives access to various CPU internals such as registers.
   Used every time an instruction is executed */

#ifndef CPU_INLINE
#define CPU_INLINE			(DEFAULT_INLINE ? ALL_INLINE : 0)
#endif

/* Code that translates between an effective and real address.  Used
   by every load or store. */

#ifndef VM_INLINE
#define VM_INLINE			DEFAULT_INLINE
#endif

/* Code that loads/stores data to/from the memory data structure.
   Used by every load or store */

#ifndef CORE_INLINE
#define CORE_INLINE			DEFAULT_INLINE
#endif

/* Code to check for and process any events scheduled in the future.
   Called once per instruction cycle */

#ifndef EVENTS_INLINE
#define EVENTS_INLINE			(DEFAULT_INLINE ? ALL_INLINE : 0)
#endif

/* Code monotoring the processors performance.  It counts events on
   every instruction cycle */

#ifndef MON_INLINE
#define MON_INLINE			(DEFAULT_INLINE ? ALL_INLINE : 0)
#endif

/* Code called on the rare occasions that an interrupt occures. */

#ifndef INTERRUPTS_INLINE
#define INTERRUPTS_INLINE		DEFAULT_INLINE
#endif

/* Code called on the rare occasion that either gdb or the device tree
   need to manipulate a register within a processor */

#ifndef REGISTERS_INLINE
#define REGISTERS_INLINE		DEFAULT_INLINE
#endif

/* Code called on the rare occasion that a processor is manipulating
   real hardware instead of RAM.

   Also, most of the functions in devices.c are always called through
   a jump table. */

#ifndef DEVICE_INLINE
#define DEVICE_INLINE			(DEFAULT_INLINE ? PSIM_INLINE_LOCALS : 0)
#endif

/* Code called used while the device tree is being built.

   Inlining this is of no benefit */

#ifndef TREE_INLINE
#define TREE_INLINE			(DEFAULT_INLINE ? PSIM_INLINE_LOCALS : 0)
#endif

/* Code called whenever information on a Special Purpose Register is
   required.  Called by the mflr/mtlr pseudo instructions */

#ifndef SPREG_INLINE
#define SPREG_INLINE			DEFAULT_INLINE
#endif

/* Functions modeling the semantics of each instruction.  Two cases to
   consider, firstly of idecode is implemented with a switch then this
   allows the idecode function to inline each semantic function
   (avoiding a call).  The second case is when idecode is using a
   table, even then while the semantic functions can't be inlined,
   setting it to one still enables each semantic function to inline
   anything they call (if that code is marked for being inlined).

   WARNING: you need lots (like 200mb of swap) of swap.  Setting this
   to 1 is useful when using a table as it enables the sematic code to
   inline all of their called functions */

#ifndef SEMANTICS_INLINE
#define SEMANTICS_INLINE		(DEFAULT_INLINE & ~INLINE_MODULE)
#endif

/* When using the instruction cache, code to decode an instruction and
   install it into the cache.  Normally called when ever there is a
   miss in the instruction cache. */

#ifndef ICACHE_INLINE
#define ICACHE_INLINE			(DEFAULT_INLINE & ~INLINE_MODULE)
#endif

/* General functions called by semantics functions but part of the
   instruction table.  Although called by the semantic functions the
   frequency of calls is low.  Consequently the need to inline this
   code is reduced. */

#ifndef SUPPORT_INLINE
#define SUPPORT_INLINE			PSIM_INLINE_LOCALS
#endif

/* Model specific code used in simulating functional units.  Note, it actaully
   pays NOT to inline the PowerPC model functions (at least on the x86).  This
   is because if it is inlined, each PowerPC instruction gets a separate copy
   of the code, which is not friendly to the cache.  */

#ifndef MODEL_INLINE
#define	MODEL_INLINE			(DEFAULT_INLINE & ~INLINE_MODULE)
#endif

/* Code to print out what options we were compiled with.  Because this
   is called at process startup, it doesn't have to be inlined, but
   if it isn't brought in and the model routines are inline, the model
   routines will be pulled in twice.  */

#ifndef OPTIONS_INLINE
#define OPTIONS_INLINE			MODEL_INLINE
#endif

/* idecode acts as the hub of the system, everything else is imported
   into this file */

#ifndef IDECOCE_INLINE
#define IDECODE_INLINE			PSIM_INLINE_LOCALS
#endif

/* psim, isn't actually inlined */

#ifndef PSIM_INLINE
#define PSIM_INLINE			PSIM_INLINE_LOCALS
#endif

/* Code to emulate os or rom compatibility.  This code is called via a
   table and hence there is little benefit in making it inline */

#ifndef OS_EMUL_INLINE
#define OS_EMUL_INLINE			0
#endif

#endif /* _PSIM_CONFIG_H */
