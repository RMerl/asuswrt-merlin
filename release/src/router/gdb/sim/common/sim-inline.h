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


#ifndef SIM_INLINE_H
#define SIM_INLINE_H


/* INLINE CODE SELECTION:

   GCC -O3 attempts to inline any function or procedure in scope.  The
   options below facilitate finer grained control over what is and
   what is not inlined.  In particular, it allows the selection of
   modules for inlining.  Doing this allows the compiler to both
   eliminate the overhead of function calls and (as a consequence)
   also eliminate further dead code.

   On a CISC (x86) I've found that I can achieve an order of magintude
   speed improvement (x3-x5).  In the case of RISC (sparc) while the
   performance gain isn't as great it is still significant.

   Each module is controled by the macro <module>_INLINE which can
   have the values described below

       0 (ZERO)

         Do not inline any thing for the given module

   The following bit fields values can be combined:

      H_REVEALS_MODULE:
      C_REVEALS_MODULE:

         Include the C file for the module into the file being
         compiled.  The actual inlining is controlled separatly.

	 While of no apparent benefit, this makes it possible for the
	 included module, when compiled, to inline its calls to what
	 would otherwize be external functions.

	 {C_,H_} Determines where the module is inlined.  A
	 H_REVEALS_MODULE will be included everywhere.

      INLINE_GLOBALS:

         Make external functions within the module `inline'.  Thus if
         the module is included into a file being compiled, calls to
	 the included modules funtions can be eliminated.  INLINE_MODULE
	 implies REVEAL_MODULE.

      INLINE_LOCALS:

         Make internal (static) functions within the module `inline'.


   CODING STYLE:

   The inline ability is enabled by specifying every data and function
   declaration and definition using one of the following methods:


       GLOBAL INLINE FUNCTIONS:

          Such functions are small and used heavily.  Inlining them
          will eliminate an unnecessary function call overhead.

	  .h: INLINE_OURPKG (void) ourpkg_func
	      (int x,
	       int y);

	  .c: INLINE_OURPKG (void)
	      ourpkg_func (int x,
	                   int y)
	      {
	        ...
	      }


       GLOBAL INLINE VARIABLES:

          This doesn't make much sense.


       GLOBAL NON-INLINE (EXTERN) FUNCTIONS AND VARIABLES:

          These include functions with varargs parameters.  It can
          also include large rarely used functions that contribute
          little when inlined.

	  .h: extern int ourpkg_print
	      (char *fmt, ...);
	      extern int a_global_variable;

	  .c: #if EXTERN_OURPKG_P
	      int
	      ourpkg_print (char *fmt,
	                    ...)
              {
	         ...
	      }
	      #endif
	      #if EXTERN_OURPKG_P
	      int a_global_variable = 1;
	      #endif


       LOCAL (STATIC) FUNCTIONS:

          These can either be marked inline or just static static vis:

	  .h: STATIC_INLINE_OURPKG (int) ourpkg_staticf (void);
	  .c: STATIC_INLINE_OURPKG (int)
	      ourpkg_staticf (void)
	      {
	        ..
	      }

	  .h: STATIC_OURPKG (int) ourpkg_staticf (void);
	  .c: STATIC_OURPKG (int)
	      ourpkg_staticf (void)
	      {
	        ..
	      }


       All .h files:


          All modules must wrap their .h code in the following:

	  #ifndef OURPKG_H
	  #define OURPKG_H
	  ... code proper ...
	  #endif

          In addition, modules that want to allow global inlining must
          include the lines (below) at the end of the .h file. (FIXME:
          Shouldn't be needed).

          #if H_REVEALS_MODULE_P (OURPKG_INLINE)
          #include "ourpkg.c"
          #endif


       All .c files:

          All modules must wrap their .c code in the following

	  #ifndef OURPKG_C
	  #define OURPKG_C
	  ... code proper ...
	  #endif


   NOW IT WORKS:

      0:

      Since no inlining is defined. All macro's get standard defaults
      (extern, static, ...).
      


      H_REVEALS_MODULE (alt includes our):


      altprog.c defines ALTPROG_C and then includes sim-inline.h.

      In sim-inline.h the expression `` H_REVEALS_MODULE_P
      (OURPROG_INLINE) && !  defined (OURPROG_C) && REVEAL_MODULE_P
      (OURPROG_INLINE) '' is TRUE so it defines *_OURPROG as static
      and EXTERN_OURPROG_P as FALSE.

      altprog.c includes ourprog.h.

      In ourprog.h the expression ``H_REVEALS_MODULE_P
      (OURPROG_INLINE)'' is TRUE so it includes ourprog.c.

      Consequently, all the code in ourprog.c is visible and static in
      the file altprog.c



      H_REVEALS_MODULE (our includes our):


      ourprog.c defines OURPROG_C and then includes sim-inline.h.

      In sim-inline.h the term `` ! defined (OURPROG_C) '' is FALSE so
      it defines *_OURPROG as non-static and EXTERN_OURPROG_P as TRUE.

      ourprog.c includes ourprog.h.

      In ourprog.h the expression ``H_REVEALS_MODULE_P
      (OURPROG_INLINE)'' is true so it includes ourprog.c.

      In ourprog.c (second include) the expression defined (OURPROG_C)
      and so the body is not re-included.

      Consequently, ourprog.o will contain a non-static copy of all
      the exported symbols.



      C_REVEALS_MODULE (alt includes our):


      altprog.c defines ALTPROG_C and then includes sim-inline.c

      sim-inline.c defines C_INLINE_C and then includes sim-inline.h
      
      In sim-inline.h the expression `` defined (SIM_INLINE) && !
      defined (OURPROG_C) && REVEAL_MODULE_P (OURPROG_INLINE) '' is
      true so it defines *_OURPROG as static and EXTERN_OURPROG_P as
      FALSE.

      In sim-inline.c the expression ``C_REVEALS_MODULE_P
      (OURPROG_INLINE)'' is true so it includes ourprog.c.

      Consequently, all the code in ourprog.c is visible and static in
      the file altprog.c.



      C_REVEALS_MODULE (our includes our):


      ourprog.c defines OURPROG_C and then includes sim-inline.c

      sim-inline.c defines C_INLINE_C and then includes sim-inline.h
      
      In sim-inline.h the term `` !  defined (OURPROG_C) '' is FALSE
      so it defines *_OURPROG as non-static and EXTERN_OURPROG_P as
      TRUE.

      Consequently, ourprog.o will contain a non-static copy of all
      the exported symbols.



   REALITY CHECK:

   This is not for the faint hearted.  I've seen GCC get up to 500mb
   trying to compile what this can create. */

#define H_REVEALS_MODULE		1
#define C_REVEALS_MODULE		2
#define INLINE_GLOBALS			4
#define INLINE_LOCALS			8

#define REGPARM_MODULE                 32

#define ALL_H_INLINE (H_REVEALS_MODULE | INLINE_GLOBALS | INLINE_LOCALS)
#define ALL_C_INLINE (C_REVEALS_MODULE | INLINE_GLOBALS | INLINE_LOCALS)


/* Default macro to simplify control several of key the inlines */

#ifndef DEFAULT_INLINE
#define	DEFAULT_INLINE			INLINE_LOCALS
#endif

#define REVEAL_MODULE_P(X) (X & (H_REVEALS_MODULE | C_REVEALS_MODULE))
#define H_REVEALS_MODULE_P(X) ((X & H_REVEALS_MODULE))
#define C_REVEALS_MODULE_P(X) ((X & C_REVEALS_MODULE))


#ifndef HAVE_INLINE
#ifdef __GNUC__
#define HAVE_INLINE
#endif
#endif


/* Your compilers inline prefix */

#ifndef INLINE
#if defined (__GNUC__) && defined (__OPTIMIZE__)
#define INLINE __inline__
#else
#define INLINE /*inline*/
#endif
#endif

/* ??? Temporary, pending decision to always use extern inline and do a vast
   cleanup of inline support.  */
#ifndef INLINE2
#if defined (__GNUC__)
#define INLINE2 __inline__
#else
#define INLINE2 /*inline*/
#endif
#endif


/* Your compiler's static inline prefix */

#ifndef STATIC_INLINE
#define STATIC_INLINE static INLINE
#endif


/* Your compiler's extern inline prefix */

#ifndef EXTERN_INLINE
#define EXTERN_INLINE extern INLINE2
#endif


/* Your compiler's no-return reserved word */

#ifndef NORETURN
#define NORETURN
#endif



/* Your compilers's unused reserved word */

#if !defined (UNUSED)
#if (!defined (__GNUC__) \
     || (__GNUC__ < 2) \
     || (__GNUC__ == 2 && __GNUC_MINOR__ < 7))
#define UNUSED
#else
#define UNUSED __attribute__((__unused__))
#endif
#endif




/* Your compilers nonstandard function call mechanism prefix */

#if !defined REGPARM
#if defined (__GNUC__) && (defined (__i386__) || defined (__i486__) || defined (__i586__) || defined (__i686__))
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



/* *****
   sim-bits and sim-endian are treated differently from the rest
   of the modules below.  Their default value is ALL_H_INLINE.
   The rest are ALL_C_INLINE.  Don't blink, you'll miss it!
   *****
   */

/* sim-bits */

#if !defined (SIM_BITS_INLINE) && (DEFAULT_INLINE)
# define SIM_BITS_INLINE (ALL_H_INLINE)
#endif

#if (SIM_BITS_INLINE & REGPARM_MODULE)
# define REGPARM_SIM_BITS REGPARM
#else
# define REGPARM_SIM_BITS
#endif

#if ((H_REVEALS_MODULE_P (SIM_BITS_INLINE) || defined (SIM_INLINE_C)) \
     && !defined (SIM_BITS_C) \
     && (REVEAL_MODULE_P (SIM_BITS_INLINE)))
# if (SIM_BITS_INLINE & INLINE_GLOBALS)
#  define INLINE_SIM_BITS(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_SIM_BITS_P 0
# else
#  define INLINE_SIM_BITS(TYPE) static TYPE UNUSED REGPARM_SIM_BITS
#  define EXTERN_SIM_BITS_P 0
# endif
#else
# define INLINE_SIM_BITS(TYPE) TYPE REGPARM_SIM_BITS
# define EXTERN_SIM_BITS_P 1
#endif

#if (SIM_BITS_INLINE & INLINE_LOCALS)
# define STATIC_INLINE_SIM_BITS(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_SIM_BITS(TYPE) static TYPE REGPARM_SIM_BITS
#endif

#define STATIC_SIM_BITS(TYPE) static TYPE



/* sim-core */

#if !defined (SIM_CORE_INLINE) && (DEFAULT_INLINE)
# define SIM_CORE_INLINE ALL_C_INLINE
#endif

#if (SIM_CORE_INLINE & REGPARM_MODULE)
# define REGPARM_SIM_CORE REGPARM
#else
# define REGPARM_SIM_CORE
#endif

#if ((H_REVEALS_MODULE_P (SIM_CORE_INLINE) || defined (SIM_INLINE_C)) \
     && !defined (SIM_CORE_C) \
     && (REVEAL_MODULE_P (SIM_CORE_INLINE)))
# if (SIM_CORE_INLINE & INLINE_GLOBALS)
#  define INLINE_SIM_CORE(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_SIM_CORE_P 0
#else
#  define INLINE_SIM_CORE(TYPE) static TYPE UNUSED REGPARM_SIM_CORE
#  define EXTERN_SIM_CORE_P 0
#endif
#else
# define INLINE_SIM_CORE(TYPE) TYPE REGPARM_SIM_CORE
# define EXTERN_SIM_CORE_P 1
#endif

#if (SIM_CORE_INLINE & INLINE_LOCALS)
# define STATIC_INLINE_SIM_CORE(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_SIM_CORE(TYPE) static TYPE REGPARM_SIM_CORE
#endif

#define STATIC_SIM_CORE(TYPE) static TYPE



/* sim-endian */

#if !defined (SIM_ENDIAN_INLINE) && (DEFAULT_INLINE)
# define SIM_ENDIAN_INLINE ALL_H_INLINE
#endif

#if (SIM_ENDIAN_INLINE & REGPARM_MODULE)
# define REGPARM_SIM_ENDIAN REGPARM
#else
# define REGPARM_SIM_ENDIAN
#endif

#if ((H_REVEALS_MODULE_P (SIM_ENDIAN_INLINE) || defined (SIM_INLINE_C)) \
     && !defined (SIM_ENDIAN_C) \
     && (REVEAL_MODULE_P (SIM_ENDIAN_INLINE)))
# if (SIM_ENDIAN_INLINE & INLINE_GLOBALS)
#  define INLINE_SIM_ENDIAN(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_SIM_ENDIAN_P 0
# else
#  define INLINE_SIM_ENDIAN(TYPE) static TYPE UNUSED REGPARM_SIM_ENDIAN
#  define EXTERN_SIM_ENDIAN_P 0
# endif
#else
# define INLINE_SIM_ENDIAN(TYPE) TYPE REGPARM_SIM_ENDIAN
# define EXTERN_SIM_ENDIAN_P 1
#endif

#if (SIM_ENDIAN_INLINE & INLINE_LOCALS)
# define STATIC_INLINE_SIM_ENDIAN(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_SIM_ENDIAN(TYPE) static TYPE REGPARM_SIM_ENDIAN
#endif

#define STATIC_SIM_ENDIAN(TYPE) static TYPE



/* sim-events */

#if !defined (SIM_EVENTS_INLINE) && (DEFAULT_INLINE)
# define SIM_EVENTS_INLINE ALL_C_INLINE
#endif

#if (SIM_EVENTS_INLINE & REGPARM_MODULE)
# define REGPARM_SIM_EVENTS REGPARM
#else
# define REGPARM_SIM_EVENTS
#endif

#if ((H_REVEALS_MODULE_P (SIM_EVENTS_INLINE) || defined (SIM_INLINE_C)) \
     && !defined (SIM_EVENTS_C) \
     && (REVEAL_MODULE_P (SIM_EVENTS_INLINE)))
# if (SIM_EVENTS_INLINE & INLINE_GLOBALS)
#  define INLINE_SIM_EVENTS(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_SIM_EVENTS_P 0
# else
#  define INLINE_SIM_EVENTS(TYPE) static TYPE UNUSED REGPARM_SIM_EVENTS
#  define EXTERN_SIM_EVENTS_P 0
# endif
#else
# define INLINE_SIM_EVENTS(TYPE) TYPE REGPARM_SIM_EVENTS
# define EXTERN_SIM_EVENTS_P 1
#endif

#if (SIM_EVENTS_INLINE & INLINE_LOCALS)
# define STATIC_INLINE_SIM_EVENTS(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_SIM_EVENTS(TYPE) static TYPE REGPARM_SIM_EVENTS
#endif

#define STATIC_SIM_EVENTS(TYPE) static TYPE



/* sim-fpu */

#if !defined (SIM_FPU_INLINE) && (DEFAULT_INLINE)
# define SIM_FPU_INLINE ALL_C_INLINE
#endif

#if (SIM_FPU_INLINE & REGPARM_MODULE)
# define REGPARM_SIM_FPU REGPARM
#else
# define REGPARM_SIM_FPU
#endif

#if ((H_REVEALS_MODULE_P (SIM_FPU_INLINE) || defined (SIM_INLINE_C)) \
     && !defined (SIM_FPU_C) \
     && (REVEAL_MODULE_P (SIM_FPU_INLINE)))
# if (SIM_FPU_INLINE & INLINE_GLOBALS)
#  define INLINE_SIM_FPU(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_SIM_FPU_P 0
# else
#  define INLINE_SIM_FPU(TYPE) static TYPE UNUSED REGPARM_SIM_FPU
#  define EXTERN_SIM_FPU_P 0
# endif
#else
# define INLINE_SIM_FPU(TYPE) TYPE REGPARM_SIM_FPU
# define EXTERN_SIM_FPU_P 1
#endif

#if (SIM_FPU_INLINE & INLINE_LOCALS)
# define STATIC_INLINE_SIM_FPU(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_SIM_FPU(TYPE) static TYPE REGPARM_SIM_FPU
#endif

#define STATIC_SIM_FPU(TYPE) static TYPE



/* sim-types */

#if (SIM_TYPES_INLINE & REGPARM_MODULE)
# define REGPARM_SIM_TYPES REGPARM
#else
# define REGPARM_SIM_TYPES
#endif

#if ((H_REVEALS_MODULE_P (SIM_TYPES_INLINE) || defined (SIM_INLINE_C)) \
     && !defined (SIM_TYPES_C) \
     && (REVEAL_MODULE_P (SIM_TYPES_INLINE)))
# if (SIM_TYPES_INLINE & INLINE_GLOBALS)
#  define INLINE_SIM_TYPES(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_SIM_TYPES_P 0
# else
#  define INLINE_SIM_TYPES(TYPE) static TYPE UNUSED REGPARM_SIM_TYPES
#  define EXTERN_SIM_TYPES_P 0
# endif
#else
# define INLINE_SIM_TYPES(TYPE) TYPE REGPARM_SIM_TYPES
# define EXTERN_SIM_TYPES_P 1
#endif

#if (SIM_TYPES_INLINE & INLINE_LOCALS)
# define STATIC_INLINE_SIM_TYPES(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_SIM_TYPES(TYPE) static TYPE REGPARM_SIM_TYPES
#endif

#define STATIC_SIM_TYPES(TYPE) static TYPE



/* sim_main */

#if !defined (SIM_MAIN_INLINE) && (DEFAULT_INLINE)
# define SIM_MAIN_INLINE (ALL_C_INLINE)
#endif

#if (SIM_MAIN_INLINE & REGPARM_MODULE)
# define REGPARM_SIM_MAIN REGPARM
#else
# define REGPARM_SIM_MAIN
#endif

#if ((H_REVEALS_MODULE_P (SIM_MAIN_INLINE) || defined (SIM_INLINE_C)) \
     && !defined (SIM_MAIN_C) \
     && (REVEAL_MODULE_P (SIM_MAIN_INLINE)))
# if (SIM_MAIN_INLINE & INLINE_GLOBALS)
#  define INLINE_SIM_MAIN(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_SIM_MAIN_P 0
# else
#  define INLINE_SIM_MAIN(TYPE) static TYPE UNUSED REGPARM_SIM_MAIN
#  define EXTERN_SIM_MAIN_P 0
# endif
#else
# define INLINE_SIM_MAIN(TYPE) TYPE REGPARM_SIM_MAIN
# define EXTERN_SIM_MAIN_P 1
#endif

#if (SIM_MAIN_INLINE & INLINE_LOCALS)
# define STATIC_INLINE_SIM_MAIN(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_SIM_MAIN(TYPE) static TYPE REGPARM_SIM_MAIN
#endif

#define STATIC_SIM_MAIN(TYPE) static TYPE

/* engine */

#if (ENGINE_INLINE & REGPARM_MODULE)
# define REGPARM_ENGINE REGPARM
#else
# define REGPARM_ENGINE
#endif

#if ((H_REVEALS_MODULE_P (ENGINE_INLINE) || defined (SIM_INLINE_C)) \
     && !defined (ENGINE_C) \
     && (REVEAL_MODULE_P (ENGINE_INLINE)))
# if (ENGINE_INLINE & INLINE_GLOBALS)
#  define INLINE_ENGINE(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_ENGINE_P 0
# else
#  define INLINE_ENGINE(TYPE) static TYPE UNUSED REGPARM_ENGINE
#  define EXTERN_ENGINE_P 0
# endif
#else
# define INLINE_ENGINE(TYPE) TYPE REGPARM_ENGINE
# define EXTERN_ENGINE_P 1
#endif

#if (ENGINE_INLINE & INLINE_LOCALS)
# define STATIC_INLINE_ENGINE(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_ENGINE(TYPE) static TYPE REGPARM_ENGINE
#endif

#define STATIC_ENGINE(TYPE) static TYPE



/* icache */

#if (ICACHE_INLINE & REGPARM_MODULE)
# define REGPARM_ICACHE REGPARM
#else
# define REGPARM_ICACHE
#endif

#if ((H_REVEALS_MODULE_P (ICACHE_INLINE) || defined (SIM_INLINE_C)) \
     && !defined (ICACHE_C) \
     && (REVEAL_MODULE_P (ICACHE_INLINE)))
# if (ICACHE_INLINE & INLINE_GLOBALS)
#  define INLINE_ICACHE(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_ICACHE_P 0
#else
#  define INLINE_ICACHE(TYPE) static TYPE UNUSED REGPARM_ICACHE
#  define EXTERN_ICACHE_P 0
#endif
#else
# define INLINE_ICACHE(TYPE) TYPE REGPARM_ICACHE
# define EXTERN_ICACHE_P 1
#endif

#if (ICACHE_INLINE & INLINE_LOCALS)
# define STATIC_INLINE_ICACHE(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_ICACHE(TYPE) static TYPE REGPARM_ICACHE
#endif

#define STATIC_ICACHE(TYPE) static TYPE



/* idecode */

#if (IDECODE_INLINE & REGPARM_MODULE)
# define REGPARM_IDECODE REGPARM
#else
# define REGPARM_IDECODE
#endif

#if ((H_REVEALS_MODULE_P (IDECODE_INLINE) || defined (SIM_INLINE_C)) \
     && !defined (IDECODE_C) \
     && (REVEAL_MODULE_P (IDECODE_INLINE)))
# if (IDECODE_INLINE & INLINE_GLOBALS)
#  define INLINE_IDECODE(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_IDECODE_P 0
#else
#  define INLINE_IDECODE(TYPE) static TYPE UNUSED REGPARM_IDECODE
#  define EXTERN_IDECODE_P 0
#endif
#else
# define INLINE_IDECODE(TYPE) TYPE REGPARM_IDECODE
# define EXTERN_IDECODE_P 1
#endif

#if (IDECODE_INLINE & INLINE_LOCALS)
# define STATIC_INLINE_IDECODE(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_IDECODE(TYPE) static TYPE REGPARM_IDECODE
#endif

#define STATIC_IDECODE(TYPE) static TYPE



/* semantics */

#if (SEMANTICS_INLINE & REGPARM_MODULE)
# define REGPARM_SEMANTICS REGPARM
#else
# define REGPARM_SEMANTICS
#endif

#if ((H_REVEALS_MODULE_P (SEMANTICS_INLINE) || defined (SIM_INLINE_C)) \
     && !defined (SEMANTICS_C) \
     && (REVEAL_MODULE_P (SEMANTICS_INLINE)))
# if (SEMANTICS_INLINE & INLINE_GLOBALS)
#  define INLINE_SEMANTICS(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_SEMANTICS_P 0
#else
#  define INLINE_SEMANTICS(TYPE) static TYPE UNUSED REGPARM_SEMANTICS
#  define EXTERN_SEMANTICS_P 0
#endif
#else
# define INLINE_SEMANTICS(TYPE) TYPE REGPARM_SEMANTICS
# define EXTERN_SEMANTICS_P 1
#endif

#if EXTERN_SEMANTICS_P
# define EXTERN_SEMANTICS(TYPE) TYPE REGPARM_SEMANTICS
#else
# define EXTERN_SEMANTICS(TYPE) static TYPE UNUSED REGPARM_SEMANTICS
#endif

#if (SEMANTICS_INLINE & INLINE_LOCALS)
# define STATIC_INLINE_SEMANTICS(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_SEMANTICS(TYPE) static TYPE REGPARM_SEMANTICS
#endif

#define STATIC_SEMANTICS(TYPE) static TYPE



/* support */

#if !defined (SUPPORT_INLINE) && (DEFAULT_INLINE)
# define SUPPORT_INLINE ALL_C_INLINE
#endif

#if (SUPPORT_INLINE & REGPARM_MODULE)
# define REGPARM_SUPPORT REGPARM
#else
# define REGPARM_SUPPORT
#endif

#if ((H_REVEALS_MODULE_P (SUPPORT_INLINE) || defined (SIM_INLINE_C)) \
     && !defined (SUPPORT_C) \
     && (REVEAL_MODULE_P (SUPPORT_INLINE)))
# if (SUPPORT_INLINE & INLINE_GLOBALS)
#  define INLINE_SUPPORT(TYPE) static INLINE TYPE UNUSED
#  define EXTERN_SUPPORT_P 0
#else
#  define INLINE_SUPPORT(TYPE) static TYPE UNUSED REGPARM_SUPPORT
#  define EXTERN_SUPPORT_P 0
#endif
#else
# define INLINE_SUPPORT(TYPE) TYPE REGPARM_SUPPORT
# define EXTERN_SUPPORT_P 1
#endif

#if (SUPPORT_INLINE & INLINE_LOCALS)
# define STATIC_INLINE_SUPPORT(TYPE) static INLINE TYPE
#else
# define STATIC_INLINE_SUPPORT(TYPE) static TYPE REGPARM_SUPPORT
#endif

#define STATIC_SUPPORT(TYPE) static TYPE



#endif
