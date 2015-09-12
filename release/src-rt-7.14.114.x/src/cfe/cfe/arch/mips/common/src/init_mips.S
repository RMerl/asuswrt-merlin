/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  CPU init module				File: init_mips.S
    *
    *  This module contains the vectors and lowest-level CPU startup
    *  functions for CFE.
    *
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *
    *********************************************************************
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *
    *  This software is furnished under license and may be used and
    *  copied only in accordance with the following terms and
    *  conditions.  Subject to these conditions, you may download,
    *  copy, install, use, modify and distribute modified or unmodified
    *  copies of this software in source and/or binary form.  No title
    *  or ownership is transferred hereby.
    *
    *  1) Any source code used, modified or distributed must reproduce
    *     and retain this copyright notice and list of conditions
    *     as they appear in the source file.
    *
    *  2) No right is granted to use any trade name, trademark, or
    *     logo of Broadcom Corporation.  The "Broadcom Corporation"
    *     name may not be used to endorse or promote products derived
    *     from this software without the prior written permission of
    *     Broadcom Corporation.
    *
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */


#include "sbmips.h"
#include "exception.h"

#include "bsp_config.h"
#include "cpu_config.h"

#ifdef _CFE_
#include "cfe_devfuncs.h"
#else

#if CFG_BIENDIAN && defined(__MIPSEB)
#define CFE_EPTSEAL_REV 0x31454643
#endif
#define CFE_EPTSEAL 0x43464531

#define cfe_command_restart 0
#endif

#if CFG_VAPI		/* haul in SB1250-specfic stuff only for VAPI */
#include "sb1250_defs.h"
#include "sb1250_regs.h"
#include "sb1250_scd.h"
#endif

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#include "mipsmacros.h"


/*  *********************************************************************
    *  SETLEDS(a,b,c,d)
    *  SETLEDS1(a,b,c,d)
    *
    *  Sets the on-board LED display (if present).  Two variants
    *  of this routine are provided.  If you're running KSEG1,
    *  call the SETLEDS1 variant, else call SETLEDS.
    *
    *  Input parameters:
    *  	   a,b,c,d - four ASCII characters (literal constants)
    *
    *  Return value:
    *  	   a0,k1,ra trashed
    ********************************************************************* */

#define SETLEDS(a,b,c,d)                     \
	li     a0,(((a)<<24)|((b)<<16)|((c)<<8)|(d)) ;    \
	CALLINIT_KSEG0(init_table,R_INIT_SETLEDS)

#define SETLEDS1(a,b,c,d)                     \
	li     a0,(((a)<<24)|((b)<<16)|((c)<<8)|(d)) ;    \
	CALLINIT_KSEG1(init_table,R_INIT_SETLEDS)

/*  *********************************************************************
    *  Other constants
    ********************************************************************* */

/*
 * This is the size of the stack, rounded to KByte boundaries.
 */

#ifndef CFG_STACK_SIZE
#error "CFG_STACK_SIZE not defined"
#else
#define STACK_SIZE	((CFG_STACK_SIZE+1023) & ~1023)
#endif

#ifdef __MIPSEB
#define TEXTSECTION	0x2e746578		# ".tex", big-endian
#else
#define TEXTSECTION	0x7865742e		# ".tex", little-endian
#endif

/*
 * Duplicates from cfe_iocb.h -- warning!
 */

#define CFE_CACHE_FLUSH_D	1
#define CFE_CACHE_INVAL_I	2
#define CFE_CACHE_INVAL_D	4
#define CFE_CACHE_INVAL_L2	8
#define CFE_CACHE_FLUSH_L2	16
#define CFE_CACHE_INVAL_RANGE	32
#define CFE_CACHE_FLUSH_RANGE	64


/*
 * To make life easier reading this code, define "KSEGBASE"
 * to either K0BASE or K1BASE depending on whether we're running
 * uncached.
 */

#if CFG_RUNFROMKSEG0
#define KSEGBASE	K0BASE
#else
#define KSEGBASE	K1BASE
#endif


/*  *********************************************************************
    *  Names of registers used in this module
    ********************************************************************* */

#define RELOCOFFSET	s8			/* $30 (fp) */
#define TEXTOFFSET      t9			/* $25 (t9) */
#define MEMTOP		t8			/* $24 (t8) */
#define TEXTBASE	s7			/* $23 (s7) */

		.sdata

#include "initdata.h"		/* declare variables we use here */

#if CFG_MULTI_CPUS
		.globl	cfe_spinlock
cfe_spinlock:	.word	0
#endif

		.extern	_ftext
		.extern	_etext
		.extern	_fdata
		.extern	_edata
		.extern	_fbss
		.extern	_end

/*  *********************************************************************
    *  uninitialized data
    ********************************************************************* */

		.bss

		.comm	__junk,4

/*  *********************************************************************
    *  Exception Vectors
    ********************************************************************* */

		.text

		.set noreorder

/*
 * If we're building a bi-endian version, this is the base
 * address that we can expect to find the little-endian version
 * of the firmware.
 *
 * Warning: If you change this, you must also change
 * the linker script (arch/mips/common/src/cfe_rom_reloc_cached_biendian.lds)
 * and the mkflashimage program (hosttools/mkflashimage.c)
 */

#define BIENDIAN_LE_BASE	0xBFD00000

/*
 * Declare the actual vectors.  This expands to code that
 * must be at the very beginning of the text segment.
 */

DECLARE_VECTOR(0x0000,vec_reset,cpu_reset)
DECLARE_VECTOR(0x0200,vec_tlbfill,cpu_tlbfill)
DECLARE_XVECTOR(0x0280,vec_xtlbfill,cpu_xtlbfill,XTYPE_XTLBFILL)
DECLARE_VECTOR(0x0300,vec_cacheerr,cpu_cacheerr)
DECLARE_XVECTOR(0x0380,vec_exception,cpu_exception,XTYPE_EXCEPTION)
DECLARE_XVECTOR(0x0400,vec_interrupt,cpu_interrupt,XTYPE_INTERRUPT)
DECLARE_XVECTOR(0x0480,vec_ejtag,cpu_ejtag,XTYPE_EJTAG)


/*
 * New location of CFE seal.  Will eventually phase out the seal at
 * offset 0x508
 */
		.org	0x4E0
cfe_seal:	.word	CFE_EPTSEAL
		.word	CFE_EPTSEAL

#if CFG_BIENDIAN && defined(__MIPSEB)
		.org    0x4E8
cfe_seal_rev:	.word	CFE_EPTSEAL_REV
		.word	CFE_EPTSEAL_REV
#endif

		.set reorder

/*  *********************************************************************
    *  CFE Entry Point (used by OS boot loaders and such)
    ********************************************************************* */

		.set  noreorder

DECLARE_VECTOR(0x0500,vec_apientry,cpu_apientry)
#if !CFG_BIENDIAN
		.org	0x508
		.word   CFE_EPTSEAL
		.word	CFE_EPTSEAL
#endif

/*  *********************************************************************
    *  Verification APIs (if present)   [SB1250-specific]
    ********************************************************************* */

#if CFG_VAPI
#if CFG_EMBEDDED_PIC
#error "CFG_VAPI is not compatible with relocatable code"
#endif
#include "vapi.h"
/*
 * Vector should be 16 bytes long
 */
#define VAPI_VECTOR(l,x) \
		.extern x ; \
		.org (l & 0xFFFF) ; \
		j       x ; \
		nop ;  \
		.word   VAPI_EPTSEAL ; \
		.word   VAPI_EPTSEAL

VAPI_VECTOR(VAPI_FUNC_EXIT,vapi_exit)
VAPI_VECTOR(VAPI_FUNC_DUMPGPRS,vapi_dumpgprs)
VAPI_VECTOR(VAPI_FUNC_SETLOG,vapi_setlog)
VAPI_VECTOR(VAPI_FUNC_LOGVALUE,vapi_logsingle)
VAPI_VECTOR(VAPI_FUNC_LOGDATA,vapi_logdata)
VAPI_VECTOR(VAPI_FUNC_LOGTRACE,vapi_logtrace)
VAPI_VECTOR(VAPI_FUNC_LOGSOC,vapi_savesoc)
VAPI_VECTOR(VAPI_FUNC_LOGGPRS,vapi_loggprs)
VAPI_VECTOR(VAPI_FUNC_DUMPSTRING,vapi_puts)
VAPI_VECTOR(VAPI_FUNC_SETLEDS,vapi_setleds)
VAPI_VECTOR(VAPI_FUNC_LOGFPRS,vapi_logfprs)
#endif


		.set   reorder

/*  *********************************************************************
    *  Some offsets depend on our current configuration
    ********************************************************************* */

#if CFG_EMBEDDED_PIC
#define RUNTIME_RELOC_START	__runtime_reloc_start
#define RUNTIME_RELOC_STOP	__runtime_reloc_stop
#else
#define RUNTIME_RELOC_START	0
#define RUNTIME_RELOC_STOP	0
#endif


/*  *********************************************************************
    *  Segment Table.
    *
    *  Addresses of data segments and of certain routines we're going
    *  to call from KSEG1.  These are here mostly for the embedded
    *  PIC case, since we can't count on the 'la' instruction to
    *  do the expected thing (the assembler expands it into a macro
    *  for doing GP-relative stuff, and the code is NOT GP-relative.
    *  So, we (relocatably) get the offset of this table and then
    *  index within it.
    *
    *  Pointer values in this segment will be relative to KSEG0 for
    *  cached versions of CFE, so we need to OR in K1BASE in the
    *  case of calling to a uncached address.
    *
    *  The LOADREL macro handles most of the nastiness here.
    ********************************************************************* */


#include "segtable.h"

#if CFG_VAPI
		.org	0x600			# move past exception vectors
#else
		.org	0x580			# move past exception vectors
#endif

#if CFG_EMBEDDED_NVRAM
		.org	0x1000
		.globl	embedded_nvram
embedded_nvram:	.fill	0x400,4,~(0x48534c46)
	.long   0x4c5a4d41              # LZMA NVRAM Supported

#endif

		.globl segment_table
segment_table:
		_LONG_	_etext			# [  0] End of text (R_SEG_ETEXT)
		_LONG_	_fdata			# [  1] Beginning of data (R_SEG_FDATA)
		_LONG_	_edata			# [  2] End of data (R_SEG_EDATA)
		_LONG_	_end			# [  3] End of BSS (R_SEG_END)
		_LONG_	_ftext			# [  4] Beginning of text (R_SEG_FTEXT)
		_LONG_	_fbss			# [  5] Beginning of BSS (R_SEG_FBSS)
		_LONG_	_gp			# [  6] Global Pointer (R_SEG_GP)
		_LONG_  RUNTIME_RELOC_START	# [  7] Beginning of reloc entries
		_LONG_  RUNTIME_RELOC_STOP	# [  8] End of reloc entries
		_LONG_	cpu_apientry		# [  9] R_SEG_APIENTRY

/*  *********************************************************************
    *  Init Table.
    *
    *  This is like segment_table except it contains pointers to
    *  routines used during initialization.  It serves both as a
    *  table for doing PIC stuff and also to separate out
    *  machine-specific init routines.
    *
    *  The CALLINIT_xxx macros are used to call routines in this table.
    ********************************************************************* */


		.globl  init_table
init_table:
		_LONG_  board_earlyinit         # [  0] R_INIT_EARLYINIT
		_LONG_  board_setleds           # [  1] R_INIT_SETLEDS
		_LONG_  board_draminfo		# [  2] R_INIT_DRAMINFO
		_LONG_	CPUCFG_CPUINIT		# [  3] R_INIT_CPUINIT
		_LONG_  CPUCFG_ALTCPU_START1	# [  4] R_INIT_ALTCPU_START1
		_LONG_  CPUCFG_ALTCPU_START2	# [  5] R_INIT_ALTCPU_START2
		_LONG_  CPUCFG_ALTCPU_RESET     # [  6] R_INIT_ALTCPU_RESET
		_LONG_  CPUCFG_CPURESTART	# [  7] R_INIT_CPURESTART
		_LONG_  CPUCFG_DRAMINIT		# [  8] R_INIT_DRAMINIT
		_LONG_  CPUCFG_CACHEOPS		# [  9] R_INIT_CACHEOPS
		_LONG_  CPUCFG_TLBHANDLER       # [ 10] R_INIT_TLBHANDLER
		_LONG_	cfe_main		# [ 11] R_INIT_CMDSTART
		_LONG_	cfe_command_restart	# [ 12] R_INIT_CMDRESTART
		_LONG_  cfe_doxreq		# [ 13] R_INIT_DOXREQ


#if !CFG_MINIMAL_SIZE
		.globl  diag_table
diag_table:	_LONG_  CPUCFG_DIAG_TEST1	# [ 0 ] R_DIAG_TEST1
		_LONG_  CPUCFG_DIAG_TEST2	# [ 1 ] R_DIAG_TEST2
#endif

/*  *********************************************************************
    *  CPU Startup Code
    ********************************************************************* */

cpu_reset:

	/*
	 * Start with GP as zero.  Nobody should touch
	 * this or set it to any other value until we're ready
	 * to use it.  This is used to tell when we should start
	 * using relocated references in the init table,
	 * so beware!  (see CALLINIT_RELOC in mipsmacros.h)
	 */

		move	gp,zero			# start with no GP.

#if CFG_VAPI
	/*
	 * VAPI works by using the SCD to reset just the core.
	 * Look for a special signature in the mailbox register
	 * on CPU0 - if present, jump to the start of the diag.
	 * Of course, you need a real 12500 to do this.
	 */

		li	k0,PHYS_TO_K1(A_IMR_REGISTER(0,R_IMR_MAILBOX_CPU))
		ld	k0,0(k0)
		dli	k1,VAPI_MAGIC_NUMBER_MC
		beq	k0,k1,vapi_runmc
		dli	k1,VAPI_MAGIC_NUMBER_UNC
		beq	k0,k1,vapi_rununc
		dli	k1,VAPI_MAGIC_NUMBER
		bne	k0,k1,vapi_skip

	/*
	 * The only CP0 init we do is to set K0 to cacheable
	 */

		mfc0	k0,C0_CONFIG		# get current CONFIG register
		srl	k0,k0,3			# strip out K0 bits
		sll	k0,k0,3			# k0 bits now zero
		or	k0,k0,K_CFG_K0COH_COHERENT # K0 is cacheable.
		mtc0	k0,C0_CONFIG

	/*
	 * Set any required defeature bits (for VAPI diagnostics only)
	 * they get cleared by the soft reset.
	 */

		jal	sb1250_reset_defeature	/* in sb1250_l1cache.S */

	/*
	 * Jump to the diagnostic.  Two variants, one for cached
	 * and one for uncached.
	 */

		li	k0,VAPI_DIAG_ENTRY
		j	k0

vapi_rununc:	li	k0,VAPI_DIAG_ENTRY_UNC
		j	k0

vapi_runmc:	li	k0,VAPI_DIAG_ENTRY_MC
		j	k0

vapi_skip:
#endif

	/*
	 * Test the CAUSE and STATUS registers for why we
	 * are here.  Cold reset, Warm reset, and NMI all
	 * use this vector.
	 */



	/*
	 * Test to see if we're on the secondary CPU.  If so,
	 * go do the initialization for that CPU.
	 */

#if CFG_MULTI_CPUS
		CALLINIT_KSEG1(init_table,R_INIT_ALTCPU_RESET)
		/* does not return if on CPU1 */
#endif

#------------------------------------------------------------------------------

	/*
	 * Do low-level board initialization.  This is our first
	 * chance to customize the startup sequence.
	 */

		CALLINIT_KSEG1(init_table,R_INIT_EARLYINIT)

		SETLEDS1('H','E','L','O')

		CALLINIT_KSEG1(init_table,R_INIT_CPUINIT)

	/*
	 * Run some diagnostics
	 */

#if !CFG_MINIMAL_SIZE
		SETLEDS1('T','S','T','1')

		CALLINIT_KSEG1(diag_table,R_DIAG_TEST1)
#endif


#------------------------------------------------------------------------------
#if CFG_MULTI_CPUS
	/*
	 * Spin up secondary CPU core(s)
	 */

		CALLINIT_KSEG1(init_table,R_INIT_ALTCPU_START1)
#endif

	/*
	 * Now, switch from KSEG1 to KSEG0
	 */


#if CFG_RUNFROMKSEG0
		bal	cpu_kseg0_switch
#endif

#------------------------------------------------------------------------------
	/*
	 * Now running on cpu0 in K0SEG.
	 */

#if CFG_INIT_DRAM
		SETLEDS('D','R','A','M')

		CALLINIT_KSEG0(init_table,R_INIT_DRAMINFO)

		move   a0,v0		# pass these params
		CALLINIT_KSEG0(init_table,R_INIT_DRAMINIT)
		srl	k0,v0,10	# board_draminit returns memsize in bytes
#else
		li	k0,CFG_DRAM_SIZE
#endif

#ifdef CFE_CAPMEM
		li	k0,CFE_CAPMEM
#endif

		/* Check if we are already in RAM */
		bal	1f
		nop
1:		li	t0,0x1fffffff
		and	t0,t0,ra
		li	t1,0x1fc00000
		blt	t0,t1,zbss
		nop

		SETLEDS('C','O','P','Y')

#if !CFG_EMBEDDED_PIC
#if !CFG_XIP
		/* Copy self to RAM */
		LOADREL(a0,_ftext)
		la	a1,_ftext
#else
		/* Copy data only to RAM */
		LOADREL(a0,_etext)
		la	a1,_fdata
#endif
		la	a2,_edata
		sub	a2,a2,a1
1:		LR	t0,0(a0)
		SR	t0,0(a1)
		add	a0,4
		add	a1,4
		sub	a2,4
		bnez	a2,1b
		nop

zbss:
		/* Zero BSS */
		SETLEDS('Z','B','S','S')

		la	a0,_fbss
		la	a1,_end
		sub	a1,a1,a0

1:		SR	zero,0(a0)
		add	a0,4
		sub	a1,4
		bnez	a1,1b
		nop

		SETLEDS(' ','D','$','F')

		/* Flush the D cache */
		CALLINIT_KSEG0(cacheops_table,_TBLIDX(0))

		SETLEDS(' ','I','$','I')

		/* and invalidate the I cache */
		CALLINIT_KSEG0(cacheops_table,_TBLIDX(1))

		SETLEDS(' ','J','S','R')

		/* Jump to self in RAM  */
		la	a0,have_ram
		jr	a0
		nop

		.globl  cacheops_table
cacheops_table:	_LONG_  bcmcore_l1cache_flush_d	# [ 0 ] DCACHE_FULSH
		_LONG_  bcmcore_l1cache_inval_i	# [ 1 ] ICACHE_INVAL
#endif

#------------------------------------------------------------------------------

#if CFG_BOOTRAM
		b      have_ram			# No RAM is ok if using emulator RAM
#endif

		bne    k0,zero,have_ram

		SETLEDS('R','A','M','X')	# die here if no ram

die1:		b      die1

have_ram:

	 /*
	  * If this is the 64-bit version, turn on the KX bit
	  * to allow 64-bit accesses.
	  */

#ifdef __long64
		mfc0	t0,C0_SR
		or	t0,t0,M_SR_KX
		mtc0	t0,C0_SR
#endif

#------------------------------------------------------------------------------
	/*
	 * K0 contains the RAM size (and therefore the top of RAM
	 * offset).  Start there, and subtract the amount of memory
	 * we expect to use.  If we have more than 256MB of
	 * physical memory, work backwards from the 256MB
	 * boundary.
	 */

__CalcMemTop:   li	MEMTOP,0x40000		# 256MB boundary
		bgt	k0,MEMTOP,1f		# use 256MB if k0 is greater
		move	MEMTOP,k0		# otherwise keep top
1:		sll	MEMTOP,10		# make into byte amount


#if CFG_EMBEDDED_PIC
	/*
	 * Calculate the data relocation amount.  Store in FP
	 * for now.  We'll call this register RELOCOFFSET.
	 *
	 * Also calculate a similar offset for relocating code
	 * if we're doing that.  Call this TEXTOFFSET (t9).
	 */

		LOADREL(a0,segment_table)	# we'll need this.

		LR	t1,R_SEG_ETEXT(a0)
		LR	t0,R_SEG_FTEXT(a0)
		sub	t1,t1,t0		# T1 = text size
		sub	t3,MEMTOP,t1		# reserve room for code
		li	t2,~31			# round down to cache-line boundary
		and	t3,t2
		move	TEXTBASE,t3		# TEXTBASE is current mem top
		sub	TEXTBASE,64		# Reserve top 64 bytes

/*
 * If you're debugging the relocation stuff, you can uncomment this
 * line to force CFE to be relocated to a specific address.  Sure
 * makes life easier when setting breakpoints!
 *
 *		li	TEXTBASE,0x3C00000
 */

		li	t2,KSEGBASE
		add	TEXTBASE,t2		# offset is in K0SEG.
		sub	TEXTOFFSET,TEXTBASE,t0	# TEXTOFFSET is distance to move

__RelocOffset:

		li	t0,((CFG_HEAP_SIZE*1024)+STACK_SIZE) # t0 = size of heap + stack
		LR	t1,R_SEG_END(a0)
		LR	t2,R_SEG_FDATA(a0)
		sub	t1,t2			# t1 = data + bss
		add	t0,t1			# t0 = total
		li	t1,31			# round to 32-byte boundary
		add	t0,t1
		not	t1
		and	t0,t1			# t0 = total size rounded up

		sub	t1,TEXTBASE,t0		# t1 = TEXTBASE - total size

	/*
	 * t1 now contains the place where we would like to put our
	 * data segment, BSS, and heap.  Calculate the difference between that
	 * and where our data segment currently resides.
	 */

		LR	t0,R_SEG_FDATA(a0)		# beginning of data
		subu	RELOCOFFSET,t1,t0	# offset = distance to move segment

		li	t0,31			# round *down* to a cache line
		not	t0
		and	RELOCOFFSET,t0
#else /* */
		li	RELOCOFFSET,0		# not relocating, no offset
		li	TEXTOFFSET,0
#endif

	/*
	 * DRAM is now running, and we're alive in cacheable memory
	 * on cpu0 in K0SEG.  Set up GP.
	 */

		LOADREL(a0,segment_table)
		LR	gp,R_SEG_GP(a0)
		add	gp,RELOCOFFSET

#if CFG_EMBEDDED_PIC
#------------------------------------------------------------------------------
	/*
	 * Zero BSS
	 */

		SETLEDS('Z','B','S','S')

		LOADREL(a0,segment_table)
__ZeroBss:

		LR	v0,R_SEG_FBSS(a0)
		LR	v1,R_SEG_END(a0)
		ADD	v0,RELOCOFFSET		# Relocate to actual data segment
		ADD	v1,RELOCOFFSET

1:		SR	zero,0(v0)		# Zero one cacheline at a time
		SR	zero,(REGSIZE*1)(v0)
		SR	zero,(REGSIZE*2)(v0)
		SR	zero,(REGSIZE*3)(v0)
		add	v0,REGSIZE*4
		blt	v0,v1,1b

#------------------------------------------------------------------------------
	/*
	 * Copy code
	 */

		SETLEDS('C','O','D','E')

		LOADREL(a0,segment_table)
__CopyCode:

		move	t1,TEXTBASE		# destination address

		LR	t2,R_SEG_FTEXT(a0)		# Source address
		LR	t3,R_SEG_ETEXT(a0)

1:		LR	t4,0(t2)	# read one cache line
		LR	t5,(REGSIZE*1)(t2)
		LR	t6,(REGSIZE*2)(t2)
		LR	t7,(REGSIZE*3)(t2)
		SR	t4,0(t1)	# write one cache line
		SR	t5,(REGSIZE*1)(t1)
		SR	t6,(REGSIZE*2)(t1)
		SR	t7,(REGSIZE*3)(t1)
		add	t1,REGSIZE*4
		add	t2,REGSIZE*4
		bltu	t2,t3,1b
#endif

#------------------------------------------------------------------------------
	/*
	 * Copy initialized data
	 */

#if (CFG_BOOTRAM == 0)

		SETLEDS('D','A','T','A')

		LOADREL(a0,segment_table)

__CopyData:
		LR	t1,R_SEG_ETEXT(a0)
		li	t0,15
		add	t1,t0
		not	t0
		and	t1,t0		# t1 = _etext rounded up to 16-byte boundary

		LR	t2,R_SEG_FDATA(a0)
		LR	t3,R_SEG_EDATA(a0)
		ADD	t2,RELOCOFFSET	# Relocate to actual data segment
		ADD	t3,RELOCOFFSET

1:		LR	t4,0(t1)	# read one cache line
		LR	t5,(REGSIZE*1)(t1)
		LR	t6,(REGSIZE*2)(t1)
		LR	t7,(REGSIZE*3)(t1)
		SR	t4,0(t2)	# write one cache line
		SR	t5,(REGSIZE*1)(t2)
		SR	t6,(REGSIZE*2)(t2)
		SR	t7,(REGSIZE*3)(t2)
		add	t1,(REGSIZE*4)
		add	t2,(REGSIZE*4)
		bltu	t2,t3,1b

#endif

#------------------------------------------------------------------------------

#if CFG_EMBEDDED_PIC

	/*
	 * Walk through relocation table and do the data segment
	 * fixups.  Each entry in this table is in the following
	 * format:
	 *
	 *    <offset> <segment-name>
	 *    4 bytes   8 bytes
	 *
	 * The 'offset' represents the distance into the segment where
	 * a fixup needs to be applied, and the 'segment-name'
	 * is the name of the section where the offset is located.
	 * The basic idea is that you would expect that code and
	 * data are moved by different amounts, so places where
	 * the data segment references the text segment you need
	 * apply the offset that the text segment was moved, not data.
	 */

__RelocAll:
		SETLEDS('R','E','L','O')

		LOADREL(a0,segment_table)

		LR	a1,R_SEG_FDATA(a0)		# beginning of data segment
		add	a1,RELOCOFFSET		# relocate it.

		LR	v0,R_SEG_RELOCSTART(a0)	# relocs start here
		LR	v1,R_SEG_RELOCEND(a0)	# and end here
		li	t2,TEXTSECTION		# marker for text sections

	#
	# The bottom bit in the offset will be set if we want to
	# handle a MIPS_64 relocation.   Of course, this bit is not really
	# part of the relocation offset.
	#
		li	t4,1			# Make the mask that we
		not	t4			# need to mask off bottom bit

reloclp:	lw	t0,4(v0)		# Get section name
		beq	t0,t2,textreloc		# skip if for text section

# - - - - -  - - - - -  - - - - -  - - - - -  - - - - -  - - - - -  - - - - -  - - - - -

		lw	t0,0(v0)		# T0 = relocation offset
		and	t1,t0,1			# test MIPS_64 reloc bit (t1=0 for 32-bit)
		and	t0,t4			# clear MIPS_64 reloc bit
		add	t0,a1			# Add offset to start of data segment

		beq	t1,zero,reloc32		# go if doing a 32-bit reloc

reloc64:	ld	t1,(t0)			# Get word from data segment
		add	t1,RELOCOFFSET		# Add relocation offset
		sd	t1,(t0)			# Put it back
		b	skipreloc		# next...

reloc32:	lw	t1,(t0)			# Get word from data segment
		add	t1,RELOCOFFSET		# Add relocation offset
		sw	t1,(t0)			# Put it back
		b	skipreloc

# - - - - -  - - - - -  - - - - -  - - - - -  - - - - -  - - - - -  - - - - -  - - - - -

textreloc:
		lw	t0,0(v0)		# T0 = relocation offset
		and	t1,t0,1			# test MIPS_64 reloc bit (t1=0 for 32-bit)
		and	t0,t4			# clear MIPS_64 reloc bit
		add	t0,a1			# Add offset to start of data segment

		beq	t1,zero,treloc32	# go if doing a 32-bit reloc

treloc64:	ld	t1,(t0)			# Get word from data segment
		add	t1,TEXTOFFSET		# Add relocation offset
		sd	t1,(t0)			# Put it back
		b	skipreloc		# next...

treloc32:	lw	t1,(t0)			# Get word from data segment
		add	t1,TEXTOFFSET		# Add relocation offset
		sw	t1,(t0)			# Put it back
		b	skipreloc

skipreloc:	add	v0,12			# Go to next relocation entry
		blt	v0,v1,reloclp
#endif


#------------------------------------------------------------------------------

#if CFG_EMBEDDED_PIC
	/*
	 * Flush the cache, then switch to relocated code
	 * We need to flush the cache since we just moved the code and
	 * it may still live in our L1 DCache.  We also need to
	 * flush L2, since there are some rare times we run
	 * uncached from DRAM, like when we start/stop a CPU.
	 *
	 * In the case of running completely uncached, don't flush the
	 * cache.  It should not have any dirty lines in it, but you
	 * never know...
	 */

__GoRelo:

#if CFG_RUNFROMKSEG0
		SETLEDS('L','1','2','F')

		li	a0,CFE_CACHE_FLUSH_D | CFE_CACHE_FLUSH_L2
		CALLINIT_KSEG0(init_table,R_INIT_CACHEOPS)
		li	a0,CFE_CACHE_INVAL_I
		CALLINIT_KSEG0(init_table,R_INIT_CACHEOPS)
#endif /* CFG_RUNFROMKSEG0 */

	 	LOADREL(t0,gorelo)		# get offset of next instr
		add	t0,TEXTOFFSET		# add in our relocation
		j	t0			# and go there
gorelo:		nop

#endif
	/*
	 * Remember total amount of memory.  This is *still* in k0
	 * after all this time.  Hopefully.
	 */

__MemVars:
		SR	k0,mem_totalsize
		SR	RELOCOFFSET,mem_datareloc

		move	v0,zero

		LOADREL(a0,segment_table)	# trashed by l2 cache flush
#if CFG_EMBEDDED_PIC
		LR	v0,R_SEG_FDATA(a0)
		ADD	v0,RELOCOFFSET
#else
		LR	v0,R_SEG_FTEXT(a0)
		ADD	v0,TEXTOFFSET
#endif
		LR	v1,R_SEG_END(a0)
		ADD	v1,RELOCOFFSET

		SR	v0,mem_bottomofmem
		SR	v1,mem_heapstart

#if CFG_EMBEDDED_PIC
		move	t0,MEMTOP		# relocated code means top of memory
		add	t0,KSEGBASE		# is *after* code.
		SR	t0,mem_topofmem

//		SR	gp,-8(t0)		# Store handle at top of memory
//		SR	zero,-16(t0)		# Zero out the other handles
//		SR	zero,-24(t0)		# in our special place.
//		SR	zero,-32(t0)		#
//		SR	zero,-48(t0)		#
//		SR	zero,-64(t0)		#
#else
		add	v1,(CFG_HEAP_SIZE*1024)	# Otherwise
		add	v1,STACK_SIZE
		SR	v1,mem_topofmem
#endif

		SR	TEXTOFFSET,mem_textreloc

		/* At this point it's safe to use the CALLINIT_RELOC macro */


		LR	t1,R_SEG_FTEXT(a0)
		LR	t0,R_SEG_ETEXT(a0)
		sub	t0,t0,t1
		SR	t0,mem_textsize
		add	t1,TEXTOFFSET
		SR	t1,mem_textbase


#------------------------------------------------------------------------------

#if CFG_MULTI_CPUS
	/*
	 * Let secondary CPU(s) run their idle loops.  Set the
	 * mailbox register to our relocation factor so we can read
	 * it out of the mailbox register and relocate GP properly.
	 */

		move	a0,RELOCOFFSET
		CALLINIT_RELOC(init_table,R_INIT_ALTCPU_START2)
#endif

#ifdef _SB1250_PASS1_WORKAROUNDS_
	/*
	 * Okay, it's safe now to be coherent.
	 * Flush the D cache to invalidate all the lines we have,
	 * then change the config register back.
	 */
		li	a0,CFE_CACHE_FLUSH_D
		CALLINIT_RELOC(init_table,R_INIT_CACHEOPS)
		SETCCAMODE(v0,K_CFG_K0COH_COHERENT) /* cacheable coherent */
#endif

	/*
	 * Stash away some config register stuff
	 */

		mfc0	v0,C0_PRID
		SR	v0,cpu_prid


#------------------------------------------------------------------------------

	/*
	 * Set up the "C" stack and jump to the main routine.
	 */

		SETLEDS('M','A','I','N')

		LR	sp,mem_heapstart
		ADD	sp,((CFG_HEAP_SIZE*1024)+STACK_SIZE - 8)
		li	a0,0			# call as "cfe_main(0,0)"
		li	a1,0

		CALLINIT_RELOC(init_table,R_INIT_CMDSTART)  # should not return


	/*
	 * Terminate the simulator.
	 */

crash_sim:      li $2,1
		li $4,0
		syscall	0xCA
		b	cpu_reset



#ifdef _CFE_
/*  *********************************************************************
    *  CFE_WARMSTART
    *
    *  Restart the command interpreter
    *
    *  Input parameters:
    *      A0 - command status
    *  	   nothing (GP has already been set up for us)
    *
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(cfe_warmstart)

		SR	a0,0(sp)		# store on old stack
		LOADREL(v0,init_table)
		LR	v0,R_INIT_CPURESTART(v0)
#if CFG_EMBEDDED_PIC
		LR	t0,mem_textreloc	# relocate table entry
		ADD	v0,v0,t0
#endif

		jal	v0			# had better not trash GP or K1

	 /*
	  * If this is the 64-bit version, turn on the KX bit
	  * to allow 64-bit accesses.  Do after calling the cpu
	  * init routine, but before touching the stack, which
	  * *could* be a 64-bit address.
	  */

#ifdef __long64
		mfc0	t0,C0_SR
		or	t0,t0,M_SR_KX
		mtc0	t0,C0_SR
		HAZARD
#endif

		LR	a0,0(sp)

		LR	sp,mem_heapstart
		ADD	sp,((CFG_HEAP_SIZE*1024)+STACK_SIZE - 8)

	/*
	 * If someone called the API to do a warm start, clear the
	 * spin lock, since the call will never return.
	 */

#if CFG_MULTI_CPUS
		SPIN_UNLOCK(cfe_spinlock,t0)
#endif

		CALLINIT_RELOC(init_table,R_INIT_CMDRESTART)  # should not return

END(cfe_warmstart)
#endif

/*  *********************************************************************
    *  CFE_FLUSHCACHE
    *
    *  Perform certain cache operations
    *
    *  Input parameters:
    *  	   a0 - flags (CFE_CACHE_xxx flags, or zero for a default)
    *      a1,a2 - start/end of range for "range invalidate" operations
    *      (not used otherwise)
    *
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(_cfe_flushcache)

		sub	sp,56
		SR	ra,0(sp)
		SR	a0,8(sp)
		SR	s0,16(sp)
		SR	v1,24(sp)
		SR	s1,32(sp)
		SR	s2,40(sp)
		SR	s3,48(sp)
		SR	s4,56(sp)


		CALLINIT_RELOC(init_table,R_INIT_CACHEOPS)

		LR	s4,56(sp)
		LR	s3,48(sp)
		LR	s2,40(sp)
		LR	s1,32(sp)
		LR	v1,24(sp)
		LR	s0,16(sp)
		LR	a0,8(sp)
		LR	ra,0(sp)
		add	sp,56
		j	ra

END(_cfe_flushcache)


/*  *********************************************************************
    *  CFE_LAUNCH
    *
    *  Start the user program.  The program is passed a handle
    *  that must be passed back when calling the firmware.
    *
    *  Parameters passed to the called program are as follows:
    *
    *      a0 - CFE handle
    *      a1 - entry vector
    *      a2 - reserved, will be 0
    *      a3 - entrypoint signature.
    *
    *  Input parameters:
    *  	   a0 - entry vector
    *
    *  Return value:
    *  	   does not return
    ********************************************************************* */

LEAF(cfe_launch)

		sub	sp,8
		SR	a0,0(sp)


	/*
	 * Mask all interrupts.
	 */
		mfc0	v0,C0_SR		# Get current interrupt flag
		li	v1,M_SR_IE		# master interrupt control
		not	v1			# disable interrupts
		and	v0,v1			# SR now has IE=0
		mtc0	v0,C0_SR		# put back into CP0


	/*
	 * Flush the D-Cache, since the program we loaded is "data".
	 * Invalidate the I-Cache, so that addresses in the program
	 * region will miss and need to be filled from the data we
	 * just flushed above.
	 */

		li	a0,CFE_CACHE_FLUSH_D|CFE_CACHE_INVAL_I
		CALLINIT_RELOC(init_table,R_INIT_CACHEOPS)


	/*
	 * Set things up for launching the program.  Pass the
	 * handle in A0 - apps need to remember that and pass it
	 * back.
	 */

		j	RunProgram

END(cfe_launch)

	/*
	 * This is a nice place to set a breakpoint.
	 */
LEAF(RunProgram)

		LOADREL(a2,segment_table)
		LR	a2,R_SEG_APIENTRY(a2) # A2 = code entry

#if CFG_EMBEDDED_PIC
		LR	t1,mem_textreloc	# relocate table entry
		ADD	a2,a2,t1
#endif
		move	t0,a0		#
		move	a1,zero		# A1 = 0
		move	a0,gp		# A0 = handle
		li	a3,CFE_EPTSEAL  # A3 = entrypoint signature
		LR	t0,0(sp)	# entry point

	/* Check if the MERGE_FRAG function should execute or not,
	 * according to the information within structure cfe_frag_info_t.
	 * _frag_info_sz is the size of structure cfe_frag_info_t.
	 * Structure cfe_frag_info_t is stored right before MERGE_FRAG function, and
	 * MERGE_FRAG function is stored right before entry point.
	 * Note: {a0-a3,t0} must be preserved
	 */
		la	t3, _frag_merge_func_len
		LR	t2, 0(t3)
		la	t4, _frag_info_sz
		LR	t3, 0(t4)
		add	t2, t2, t3
		sub	t1, t0, t2
		LR	t2, 0(t1)
	        beqz	t2, do_run

		j	t2		# Go to _merge_func
		nop
do_run:
		j	t0		# go for it.
END(RunProgram)


.globl _frag_merge_func
_frag_merge_func:
        .word _merge_func

.globl _frag_merge_func_len
_frag_merge_func_len:
        .word _merge_func_end - _merge_func

       .globl _frag_info_sz
_frag_info_sz:
	.word   0

/* This merge_func will relocate at address which is stored at MERGE_FRAG.
 * It will reference the value which is stored at MERGE_FRAG.
 * There are the merge_func pointer, des_addr, src_addr and length.
 * t0 is the entry point, a0-a3 are the parameters for the image.
 * t1 is the address of MERGE_FRAG
 */
LEAF(_merge_func)
		LR	t2,4(t1)
		LR	t3,8(t1)
		LR	t4,12(t1)
1:
		LR	t5,0(t3)
		SR	t5,0(t2)
		add	t3,4
		add	t2,4
		sub	t4,4
		bgez	t4,1b

		j	t0                     # jump to the entry point
_merge_func_end:
END(_merge_func)


/*  *********************************************************************
    *  CFE_LEDS
    *
    *  Set the on-board LEDs.
    *
    *  Input parameters:
    *  	   a0 - LEDs
    *
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(cfe_leds)

#if CFG_EMBEDDED_PIC
		move	t1,ra			# LOADREL trashes ra!
		LOADREL(t0,init_table)
		move	ra,t1
		LR	t0,R_INIT_SETLEDS(t0)
		jr	t0
#else
		j	board_setleds		# jump to BSP routine
#endif

END(cfe_leds)

/*  *********************************************************************
    *  TLB Fill Exeption Handler
    ********************************************************************* */

cpu_tlbfill:
		move	k0,ra			# Save, we're about to trash
		LOADREL(k1,init_table)		# Load offset of init table
		LR	k1,R_INIT_TLBHANDLER(k1) # Get entry from table
		move	ra,k0			# restore trashed ra
		j	k1			# Dispatch to handler

/*  *********************************************************************
    *  XTLB Fill Exception Handler
    ********************************************************************* */

cpu_xtlbfill:
		j	_exc_entry

/*  *********************************************************************
    *  Cache Error Exception Handler
    ********************************************************************* */

cpu_cacheerr:

#if defined(_CSWARM_) || defined(_SWARM_) || defined(_BCM91120C_) || defined(_PTSWARM_)
#define LED_CHAR0	(32+8*3)
#define LED_CHAR1	(32+8*2)
#define LED_CHAR2	(32+8*1)
#define LED_CHAR3	(32+8*0)
#if defined(_PTSWARM_)
		li    k0,0xBB0A0000	     /* address of LEDs */
#else
		li    k0,0xB00A0000	     /* address of LEDs */
#endif
		li    k1,'C'
		sb    k1,LED_CHAR0(k0)
		li    k1,'e'
		sb    k1,LED_CHAR1(k0)
		li    k1,'r'
		sb    k1,LED_CHAR2(k0)
		li    k1,'2'
		sb    k1,LED_CHAR3(k0)

		SETLEDS1('C','e','r','2')
#endif

cpu_cache_death:	b	cpu_cache_death



/*  *********************************************************************
    *  General Exception Handler
    ********************************************************************* */

cpu_exception:
		j	_exc_entry


/*  *********************************************************************
    *  General Interrupt Handler
    ********************************************************************* */

cpu_interrupt:
		j	_exc_entry


/*  *********************************************************************
    *  EJTAG Debug Exception Handler
    ********************************************************************* */

cpu_ejtag:
		.set push
		.set mips64
		deret
		.set pop
		j	cpu_reset

/*  *********************************************************************
    *  cpu_apientry(handle,iocb)
    *
    *  API entry point for external apps.
    *
    *  Input parameters:
    *  	   a0 - firmware handle (used to determine the location of
    *  	        our relocated data)
    *  	   a1 - pointer to IOCB to execute
    *
    *  Return value:
    *  	   v0 - return code, 0 if ok
    ********************************************************************* */

#define _regidx(x)    ((x)*8)

#define CAE_SRSAVE     _regidx(0)
#define CAE_GPSAVE     _regidx(1)
#define CAE_RASAVE     _regidx(2)
#define CAE_S0SAVE     _regidx(3)
#define CAE_S1SAVE     _regidx(4)
#define CAE_S2SAVE     _regidx(5)
#define CAE_S3SAVE     _regidx(6)
#define CAE_S4SAVE     _regidx(7)
#define CAE_S5SAVE     _regidx(8)
#define CAE_S6SAVE     _regidx(9)
#define CAE_S7SAVE     _regidx(10)
#define CAE_K0SAVE     _regidx(11)
#define CAE_K1SAVE     _regidx(12)

#define CAE_STKSIZE    _regidx(13)

LEAF(cpu_apientry)

		sub	sp,CAE_STKSIZE		# Make room for our stuff

		mfc0	v0,C0_SR		# Get current interrupt flag
		SR	v0,CAE_SRSAVE(sp)	# save on stack
		li	t0,M_SR_IE		# master interrupt control
		not	t0			# disable interrupts
		and	v0,t0			# SR now has IE=0
#ifdef __long64
		or	v0,M_SR_KX
#endif
		mtc0	v0,C0_SR		# put back into CP0
		HAZARD

		SR	gp,CAE_GPSAVE(sp)	# save GP
		SR	ra,CAE_RASAVE(sp)	# and old RA

		SR	s0,CAE_S0SAVE(sp)
		SR	s1,CAE_S1SAVE(sp)
		SR	s2,CAE_S2SAVE(sp)
		SR	s3,CAE_S3SAVE(sp)
		SR	s4,CAE_S4SAVE(sp)
		SR	s5,CAE_S5SAVE(sp)
		SR	s6,CAE_S6SAVE(sp)
		SR	s7,CAE_S7SAVE(sp)
		SR	k0,CAE_K0SAVE(sp)
		SR	k1,CAE_K1SAVE(sp)

		move	gp,a0			# set up new GP
		move	a0,a1			# A0 points at IOCB

#if CFG_RUNFROMKSEG0
		bal	cpu_kseg0_switch	# switch to kseg0 if not already there
#endif

#if CFG_MULTI_CPUS
		SPIN_LOCK(cfe_spinlock,t0,t1)
#endif

		CALLINIT_RELOC(init_table,R_INIT_DOXREQ)

#if CFG_MULTI_CPUS
		SPIN_UNLOCK(cfe_spinlock,t0)
#endif

		#
		# Restore the saved registers.
		#

		LR	k1,CAE_K1SAVE(sp)
		LR	k0,CAE_K0SAVE(sp)
		LR	s7,CAE_S7SAVE(sp)
		LR	s6,CAE_S6SAVE(sp)
		LR	s5,CAE_S5SAVE(sp)
		LR	s4,CAE_S4SAVE(sp)
		LR	s3,CAE_S3SAVE(sp)
		LR	s2,CAE_S2SAVE(sp)
		LR	s1,CAE_S1SAVE(sp)
		LR	s0,CAE_S0SAVE(sp)

		LR	ra,CAE_RASAVE(sp)	# unwind the stack
		LR	gp,CAE_GPSAVE(sp)

		LR	t0,CAE_SRSAVE(sp)	# old interrupt mask

		add	sp,CAE_STKSIZE		# restore old stack pointer

		mtc0	t0,C0_SR		# restore interrupts
		HAZARD
		j	ra
		nop

END(cpu_apientry)


/*  *********************************************************************
    *  CPU_KSEG0_SWITCH
    *
    *  Hack the return address so we will come back in KSEG0
    *
    *  Input parameters:
    *  	   nothing
    *
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(cpu_kseg0_switch)

		and	ra,(K0SIZE-1)
		or	ra,K0BASE
		jr	ra

END(cpu_kseg0_switch)




/*  *********************************************************************
    *  _GETSTATUS()
    *
    *  Read the STATUS register into v0
    *
    *  Input parameters:
    *  	   nothing
    *
    *  Return value:
    *  	   v0 - Status register
    ********************************************************************* */

LEAF(_getstatus)

		mfc0	v0,C0_SR
		j	ra
END(_getstatus)

/*  *********************************************************************
    *  _SETSTATUS()
    *
    *  Set the STATUS register to the value in a0
    *
    *  Input parameters:
    *  	   nothing
    *
    *  Return value:
    *  	   v0 - Status register
    ********************************************************************* */

LEAF(_setstatus)

		mtc0	a0,C0_SR
		j	ra
END(_setstatus)


/*  *********************************************************************
    *  _GETCAUSE()
    *
    *  Read the CAUSE register into v0
    *
    *  Input parameters:
    *  	   nothing
    *
    *  Return value:
    *  	   v0 - Cause register
    ********************************************************************* */

LEAF(_getcause)

		mfc0	v0,C0_CAUSE
		j	ra
END(_getcause)


/*  *********************************************************************
    *  _GETTICKS()
    *
    *  Read the COUNT register into v0
    *
    *  Input parameters:
    *  	   nothing
    *
    *  Return value:
    *  	   v0 - count register
    ********************************************************************* */

LEAF(_getticks)

		mfc0	v0,C0_COUNT
		j	ra
END(_getticks)


/*  *********************************************************************
    *  _SETALARM(ticks)
    *
    *  Set the C0_Compare register from a0
    *
    *  Input parameters:
    *  	   a0 - compare register
    *
    *  Return value:
    *  	   none
    ********************************************************************* */

LEAF(_setalarm)

		mtc0	a0,C0_COMPARE
		j	ra
END(_setalarm)


/*  *********************************************************************
    *  _SETCONTEXT()
    *
    *  Set the CONTEXT register.
    *
    *  Input parameters:
    *  	   a0 - context
    *
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(_setcontext)

		mtc0	a0,C0_CTEXT
		j	ra
END(_setcontext)

/*  *********************************************************************
    *  _GETSEGTBL()
    *
    *  Return the address of the segment table.  We use this
    *  to display the startup messages.
    *
    *  You can't just address the table from C because it lives
    *  in the text segment.
    *
    *  Input parameters:
    *  	   nothing
    *
    *  Return value:
    *      address of table
    ********************************************************************* */


LEAF(_getsegtbl)
		move	t0,ra
		LOADREL(v0,segment_table)
		move	ra,t0
		j	ra
END(_getsegtbl)


/*  *********************************************************************
    *  _wbflush()
    *
    *  Flush the write buffer.  This is probably not necessary
    *  on SiByte CPUs, but we have it for completeness.
    *
    *  Input parameters:
    *  	   nothing
    *
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(_wbflush)

		sync			/* drain the buffers */
		la	t0,__junk	/* do an uncached read to force it out */
		or	t0,K1BASE
		lw	zero,0(t0)
		j	ra

END(_wbflush)


/*  *********************************************************************
    *  End
    ********************************************************************* */
