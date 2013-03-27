/* The common simulator framework for GDB, the GNU Debugger.

   Copyright 2002, 2004, 2007 Free Software Foundation, Inc.

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


#ifndef SIM_CONFIG_H
#define SIM_CONFIG_H


/* Host dependant:

   The CPP below defines information about the compilation host.  In
   particular it defines the macro's:

   	WITH_HOST_BYTE_ORDER	The byte order of the host. Could
				be any of LITTLE_ENDIAN, BIG_ENDIAN
				or 0 (unknown).  Those macro's also
				need to be defined.

 */


/* NetBSD:

   NetBSD is easy, everything you could ever want is in a header file
   (well almost :-) */

#if defined(__NetBSD__)
# include <machine/endian.h>
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER BYTE_ORDER
# endif
# if (BYTE_ORDER != WITH_HOST_BYTE_ORDER)
#  error "host endian incorrectly configured, check config.h"
# endif
#endif

/* Linux is similarly easy.  */

#if defined(__linux__)
# include <endian.h>
# if defined(__LITTLE_ENDIAN) && !defined(LITTLE_ENDIAN)
#  define LITTLE_ENDIAN __LITTLE_ENDIAN
# endif
# if defined(__BIG_ENDIAN) && !defined(BIG_ENDIAN)
#  define BIG_ENDIAN __BIG_ENDIAN
# endif
# if defined(__BYTE_ORDER) && !defined(BYTE_ORDER)
#  define BYTE_ORDER __BYTE_ORDER
# endif
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER BYTE_ORDER
# endif
# if (BYTE_ORDER != WITH_HOST_BYTE_ORDER)
#  error "host endian incorrectly configured, check config.h"
# endif
#endif

/* INSERT HERE - hosts that have available LITTLE_ENDIAN and
   BIG_ENDIAN macro's */


/* Some hosts don't define LITTLE_ENDIAN or BIG_ENDIAN, help them out */

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN 4321
#endif


/* SunOS on SPARC:

   Big endian last time I looked */

#if defined(sparc) || defined(__sparc__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER BIG_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != BIG_ENDIAN)
#  error "sun was big endian last time I looked ..."
# endif
#endif


/* Random x86

   Little endian last time I looked */

#if defined(i386) || defined(i486) || defined(i586) || defined (i686) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined (__i686__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER LITTLE_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != LITTLE_ENDIAN)
#  error "x86 was little endian last time I looked ..."
# endif
#endif

#if (defined (__i486__) || defined (__i586__) || defined (__i686__)) && defined(__GNUC__) && WITH_BSWAP
#undef  htonl
#undef  ntohl
#define htonl(IN) __extension__ ({ int _out; __asm__ ("bswap %0" : "=r" (_out) : "0" (IN)); _out; })
#define ntohl(IN) __extension__ ({ int _out; __asm__ ("bswap %0" : "=r" (_out) : "0" (IN)); _out; })
#endif

/* Power or PowerPC running AIX  */
#if defined(_POWER) && defined(_AIX)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER BIG_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != BIG_ENDIAN)
#  error "Power/PowerPC AIX was big endian last time I looked ..."
# endif
#endif

/* Solaris running PowerPC */
#if defined(__PPC) && defined(__sun__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER LITTLE_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != LITTLE_ENDIAN)
#  error "Solaris on PowerPCs was little endian last time I looked ..."
# endif
#endif

/* HP/PA */
#if defined(__hppa__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER BIG_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != BIG_ENDIAN)
#  error "HP/PA was big endian last time I looked ..."
# endif
#endif

/* Big endian MIPS */
#if defined(__MIPSEB__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER BIG_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != BIG_ENDIAN)
#  error "MIPSEB was big endian last time I looked ..."
# endif
#endif

/* Little endian MIPS */
#if defined(__MIPSEL__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER LITTLE_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != LITTLE_ENDIAN)
#  error "MIPSEL was little endian last time I looked ..."
# endif
#endif

/* Windows NT */
#if defined(__WIN32__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER LITTLE_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != LITTLE_ENDIAN)
#  error "Windows NT was little endian last time I looked ..."
# endif
#endif

/* Alpha running DEC unix */
#if defined(__osf__) && defined(__alpha__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER LITTLE_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != LITTLE_ENDIAN)
#  error "AXP running DEC unix was little endian last time I looked ..."
# endif
#endif


/* INSERT HERE - additional hosts that do not have LITTLE_ENDIAN and
   BIG_ENDIAN definitions available.  */

/* Until devices and tree properties are sorted out, tell sim-config.c
   not to call the tree_find_foo fns.  */
#define WITH_TREE_PROPERTIES 0


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

#ifndef WITH_DEFAULT_TARGET_BYTE_ORDER
#define WITH_DEFAULT_TARGET_BYTE_ORDER 0 /* fatal */
#endif

extern int current_host_byte_order;
#define CURRENT_HOST_BYTE_ORDER (WITH_HOST_BYTE_ORDER \
				 ? WITH_HOST_BYTE_ORDER \
				 : current_host_byte_order)
extern int current_target_byte_order;
#define CURRENT_TARGET_BYTE_ORDER (WITH_TARGET_BYTE_ORDER \
				   ? WITH_TARGET_BYTE_ORDER \
				   : current_target_byte_order)



/* XOR endian.

   In addition to the above, the simulator can support the horrible
   XOR endian mode (as found in the PowerPC and MIPS ISA).  See
   sim-core for more information.

   If WITH_XOR_ENDIAN is non-zero, it specifies the number of bytes
   potentially involved in the XOR munge. A typical value is 8. */

#ifndef WITH_XOR_ENDIAN
#define WITH_XOR_ENDIAN		0
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
   suporting only one processor (and as a consequence leaves the SMP
   code out of the build process).

   The actual number of processors is taken from the device
   /options/smp@<nr-cpu> */

#if defined (WITH_SMP) && (WITH_SMP > 0)
#define MAX_NR_PROCESSORS		WITH_SMP
#endif

#ifndef MAX_NR_PROCESSORS
#define MAX_NR_PROCESSORS		1
#endif


/* Size of target word, address and OpenFirmware Cell:

   The target word size is determined by the natural size of its
   reginsters.

   On most hosts, the address and cell are the same size as a target
   word.  */

#ifndef WITH_TARGET_WORD_BITSIZE
#define WITH_TARGET_WORD_BITSIZE        32
#endif

#ifndef WITH_TARGET_ADDRESS_BITSIZE
#define WITH_TARGET_ADDRESS_BITSIZE	WITH_TARGET_WORD_BITSIZE
#endif

#ifndef WITH_TARGET_CELL_BITSIZE
#define WITH_TARGET_CELL_BITSIZE	WITH_TARGET_WORD_BITSIZE
#endif

#ifndef WITH_TARGET_FLOATING_POINT_BITSIZE
#define WITH_TARGET_FLOATING_POINT_BITSIZE 64
#endif



/* Most significant bit of target:

   Set this according to your target's bit numbering convention.  For
   the PowerPC it is zero, for many other targets it is 31 or 63.

   For targets that can both have either 32 or 64 bit words and number
   MSB as 31, 63.  Define this to be (WITH_TARGET_WORD_BITSIZE - 1) */

#ifndef WITH_TARGET_WORD_MSB
#define WITH_TARGET_WORD_MSB            0
#endif



/* Program environment:

   Three environments are available - UEA (user), VEA (virtual) and
   OEA (perating).  The former two are environment that users would
   expect to see (VEA includes things like coherency and the time
   base) while OEA is what an operating system expects to see.  By
   setting these to specific values, the build process is able to
   eliminate non relevent environment code.

   STATE_ENVIRONMENT(sd) specifies which of vea or oea is required for
   the current runtime.

   ALL_ENVIRONMENT is used during configuration as a value for
   WITH_ENVIRONMENT to indicate the choice is runtime selectable.
   The default is then USER_ENVIRONMENT [since allowing the user to choose
   the default at configure time seems like featuritis and since people using
   OPERATING_ENVIRONMENT have more to worry about than selecting the
   default].
   ALL_ENVIRONMENT is also used to set STATE_ENVIRONMENT to the
   "uninitialized" state.  */

enum sim_environment {
  ALL_ENVIRONMENT,
  USER_ENVIRONMENT,
  VIRTUAL_ENVIRONMENT,
  OPERATING_ENVIRONMENT
};

/* If the simulator specified SIM_AC_OPTION_ENVIRONMENT, indicate so.  */
#ifdef WITH_ENVIRONMENT
#define SIM_HAVE_ENVIRONMENT
#endif

/* If the simulator doesn't specify SIM_AC_OPTION_ENVIRONMENT in its
   configure.in, the only supported environment is the user environment.  */
#ifndef WITH_ENVIRONMENT
#define WITH_ENVIRONMENT USER_ENVIRONMENT
#endif

#define DEFAULT_ENVIRONMENT (WITH_ENVIRONMENT != ALL_ENVIRONMENT \
			     ? WITH_ENVIRONMENT \
			     : USER_ENVIRONMENT)

/* To be prepended to simulator calls with absolute file paths and
   chdir:ed at startup.  */
extern char *simulator_sysroot;

/* Callback & Modulo Memory.

   Core includes a builtin memory type (raw_memory) that is
   implemented using an array.  raw_memory does not require any
   additional functions etc.

   Callback memory is where the core calls a core device for the data
   it requires.  Callback memory can be layered using priorities.

   Modulo memory is a variation on raw_memory where ADDRESS & (MODULO
   - 1) is used as the index into the memory array.

   The OEA model uses callback memory for devices.

   The VEA model uses callback memory to capture `page faults'.

   BTW, while raw_memory could have been implemented as a callback,
   profiling has shown that there is a biger win (at least for the
   x86) in eliminating a function call for the most common
   (raw_memory) case. */

#ifndef WITH_CALLBACK_MEMORY
#define WITH_CALLBACK_MEMORY		1
#endif

#ifndef WITH_MODULO_MEMORY
#define WITH_MODULO_MEMORY              0
#endif



/* Alignment:

   A processor architecture may or may not handle miss aligned
   transfers.

   As alternatives: both little and big endian modes take an exception
   (STRICT_ALIGNMENT); big and little endian models handle mis aligned
   transfers (NONSTRICT_ALIGNMENT); or the address is forced into
   alignment using a mask (FORCED_ALIGNMENT).

   Mixed alignment should be specified when the simulator needs to be
   able to change the alignment requirements on the fly (eg for
   bi-endian support). */

enum sim_alignments {
  MIXED_ALIGNMENT,
  NONSTRICT_ALIGNMENT,
  STRICT_ALIGNMENT,
  FORCED_ALIGNMENT,
};

extern enum sim_alignments current_alignment;

#if !defined (WITH_ALIGNMENT)
#define WITH_ALIGNMENT 0
#endif

#if !defined (WITH_DEFAULT_ALIGNMENT)
#define WITH_DEFAULT_ALIGNMENT 0 /* fatal */
#endif




#define CURRENT_ALIGNMENT (WITH_ALIGNMENT \
			   ? WITH_ALIGNMENT \
			   : current_alignment)



/* Floating point suport:

   Should the processor trap for all floating point instructions (as
   if the hardware wasn't implemented) or implement the floating point
   instructions directly. */

#if defined (WITH_FLOATING_POINT)

#define SOFT_FLOATING_POINT		1
#define HARD_FLOATING_POINT		2

extern int current_floating_point;
#define CURRENT_FLOATING_POINT (WITH_FLOATING_POINT \
				? WITH_FLOATING_POINT \
				: current_floating_point)

#endif



/* Engine module.

   Use the common start/stop/restart framework (sim-engine).
   Simulators using the other modules but not the engine should define
   WITH_ENGINE=0. */

#ifndef WITH_ENGINE
#define WITH_ENGINE			1
#endif



/* Debugging:

   Control the inclusion of debugging code.
   Debugging is only turned on in rare circumstances [say during development]
   and is not intended to be turned on otherwise.  */

#ifndef WITH_DEBUG
#define WITH_DEBUG			0
#endif

/* Include the tracing code.  Disabling this eliminates all tracing
   code */

#ifndef WITH_TRACE
#define WITH_TRACE                      (-1)
#endif

/* Include the profiling code.  Disabling this eliminates all profiling
   code.  */

#ifndef WITH_PROFILE
#define WITH_PROFILE			(-1)
#endif


/* include code that checks assertions scattered through out the
   program */

#ifndef WITH_ASSERT
#define WITH_ASSERT			1
#endif


/* Whether to check instructions for reserved bits being set */

/* #define WITH_RESERVED_BITS		1 */



/* include monitoring code */

#define MONITOR_INSTRUCTION_ISSUE	1
#define MONITOR_LOAD_STORE_UNIT		2
/* do not define WITH_MON by default */
#define DEFAULT_WITH_MON		(MONITOR_LOAD_STORE_UNIT \
					 | MONITOR_INSTRUCTION_ISSUE)


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



/* Specify that configured calls pass parameters in registers when the
   convention is that they are placed on the stack */

#ifndef WITH_REGPARM
#define WITH_REGPARM                   0
#endif

/* Specify that configured calls use an alternative calling mechanism */

#ifndef WITH_STDCALL
#define WITH_STDCALL                   0
#endif


/* Set the default state configuration, before parsing argv.  */

extern void sim_config_default (SIM_DESC sd);

/* Complete and verify the simulator configuration.  */

extern SIM_RC sim_config (SIM_DESC sd);

/* Print the simulator configuration.  */

extern void print_sim_config (SIM_DESC sd);


#endif
