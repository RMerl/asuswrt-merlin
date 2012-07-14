/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  CPU init module				File: init_ram.S
    *
    *  This module contains the vectors and lowest-level CPU startup
    *  functions for CFE.
    * 
    *  This is very similar to "init_mips.S" but is used when
    *  you want to locate CFE in DRAM, loading it like an
    *  application program.
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

#include "cfe_devfuncs.h"

/*  *********************************************************************
    *  Check some stuff
    ********************************************************************* */

#if CFG_RELOC
#error "RAM version is not compatible with relocation."
#endif
#if !CFG_RUNFROMKSEG0 && !defined(JTAG_RAM_BOOT)
#error "RAM version should be run cached"
#endif

#if CFG_MULTI_CPUS
#error "Multiple CPUs not compatible with RAM version"
#endif


/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#include "mipsmacros.h"


/*  *********************************************************************
    *  SETLEDS(a,b,c,d)
    *  
    *  Sets the on-board LED display (if present). 
    *  
    *  Input parameters: 
    *  	   a,b,c,d - four ASCII characters (literal constants)
    *  	   
    *  Return value:
    *  	   a0,k1,ra trashed
    ********************************************************************* */


#define SETLEDS(a,b,c,d)                     \
       li     a0,(((a)<<24)|((b)<<16)|((c)<<8)|(d)) ;    \
       jal    board_setleds ;


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
#ifdef JTAG_RAM_BOOT
#define KSEGBASE        K1BASE   /* JTAG RAM version always uncached */
#else
#define KSEGBASE        K0BASE   /* RAM version always cached */
#endif /* JTAG_RAM_BOOT */

/*  *********************************************************************
    *  Names of registers used in this module
    ********************************************************************* */

		.sdata

#include "initdata.h"		/* declare variables we use here */

#if CFG_MULTI_CPUS
		.globl	cfe_spinlock
cfe_spinlock:	.word	0
#endif

		.extern	_fdata
		.extern	_edata
		.extern	_etext

/*  *********************************************************************
    *  uninitialized data
    ********************************************************************* */

		.bss

		.comm	__junk,4

		.text
	
		.set noreorder


/*  *********************************************************************
    *  CFE Entry Point (used by OS boot loaders and such)
    ********************************************************************* */

                .set  noreorder

                .globl vec_reset

vec_reset:	b      cpu_reset
		nop


vec_apientry:	b	cpu_apientry
		nop
		.word   CFE_EPTSEAL
		.word   CFE_EPTSEAL

		.set   reorder


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

		.globl segment_table
segment_table:
		_LONG_	_etext			# [  0] End of text (R_SEG_ETEXT)
		_LONG_	_fdata			# [  1] Beginning of data (R_SEG_FDATA)
		_LONG_	_edata			# [  2] End of data (R_SEG_EDATA)
		_LONG_	_end			# [  3] End of BSS (R_SEG_END)
		_LONG_	_ftext			# [  4] Beginning of text (R_SEG_FTEXT)
		_LONG_	_fbss			# [  5] Beginning of BSS (R_SEG_FBSS)
		_LONG_	_gp			# [  6] Global Pointer (R_SEG_GP)
		_LONG_  0			# [  7] Beginning of reloc entries
		_LONG_  0			# [  8] End of reloc entries
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

/*  *********************************************************************
    *  CPU Startup Code
    ********************************************************************* */


cpu_reset:

#------------------------------------------------------------------------------

	/*
	 * Do low-level board initialization.  This is our first
	 * chance to customize the startup sequence.
	 */

		CALLINIT_KSEG0(init_table,R_INIT_EARLYINIT)

		SETLEDS('H','E','L','O')

#------------------------------------------------------------------------------

	/*
	 * DRAM is now running, and we're alive in cacheable memory
	 * on cpu0 in K0SEG.  Set up GP.
	 */

		LOADREL(a0,segment_table)
		LR	gp,R_SEG_GP(a0)

#------------------------------------------------------------------------------
	/*
	 * Zero BSS
         */

		SETLEDS('Z','B','S','S')

		LOADREL(a0,segment_table)
__ZeroBss:

		LR	v0,R_SEG_FBSS(a0)
		LR	v1,R_SEG_END(a0)

1:		SR	zero,0(v0)		# Zero one cacheline at a time
		SR	zero,(REGSIZE*1)(v0)
		SR	zero,(REGSIZE*2)(v0)
		SR	zero,(REGSIZE*3)(v0)
		add	v0,REGSIZE*4
		blt	v0,v1,1b


#------------------------------------------------------------------------------

		li	k0,256			# memory size in megabytes


#ifdef __long64
		mfc0	t0,C0_SR
		or	t0,t0,M_SR_KX
		mtc0	t0,C0_SR
#endif

#------------------------------------------------------------------------------

	/*
         * Remember total amount of memory.  This is *still* in k0
	 * after all this time.  Hopefully.
	 */

__MemVars:
		SR	k0,mem_totalsize
		SR	zero,mem_datareloc

		move	v0,zero

		LOADREL(a0,segment_table)	# trashed by l2 cache flush
		LR	v0,R_SEG_FTEXT(a0)	# bottom = beginning of text
		LR	v1,R_SEG_END(a0)

		SR	v0,mem_bottomofmem
		SR	v1,mem_heapstart

		add	v1,(CFG_HEAP_SIZE*1024)	# Otherwise
		add	v1,STACK_SIZE
		SR	v1,mem_topofmem

		SR	zero,mem_textreloc


		LR	t1,R_SEG_FTEXT(a0)
		LR	t0,R_SEG_ETEXT(a0)
		sub	t0,t0,t1
		SR	t0,mem_textsize
		SR	t1,mem_textbase


#------------------------------------------------------------------------------

#if CFG_MULTI_CPUS
	/*
	 * Let secondary CPU(s) run their idle loops.  Set the 
	 * mailbox register to our relocation factor so we can read
	 * it out of the mailbox register and relocate GP properly.
	 */

		move	a0,zero
		CALLINIT_KSEG0(init_table,R_INIT_ALTCPU_START2)
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

		CALLINIT_KSEG0(init_table,R_INIT_CMDSTART)  # should not return


	/*
	 * Terminate the simulator.
	 */

crash_sim:      li $2,1
                li $4,0
                syscall	0xCA
		b	cpu_reset



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
		jal	v0			# had better not trash GP or K1
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

		CALLINIT_KSEG0(init_table,R_INIT_CMDRESTART)  # should not return

END(cfe_warmstart)

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
#ifndef JTAG_RAM_BOOT
		sub	sp,32
		SR	ra,0(sp)
		SR	a0,8(sp)
		SR	s0,16(sp)
		SR	v1,32(sp)


		CALLINIT_KSEG0(init_table,R_INIT_CACHEOPS)

		LR	v1,32(sp)
		LR	s0,16(sp)
		LR	a0,8(sp)
		LR	ra,0(sp)
		add	sp,32
		j	ra
#endif
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

#ifndef JTAG_RAM_BOOT
	/*
	 * Flush the D-Cache, since the program we loaded is "data".
	 * Invalidate the I-Cache, so that addresses in the program
	 * region will miss and need to be filled from the data we 
	 * just flushed above.
	 */
		li	a0,CFE_CACHE_FLUSH_D|CFE_CACHE_INVAL_I
		CALLINIT_KSEG0(init_table,R_INIT_CACHEOPS)
#endif
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
		move	t0,a0		# 
		move	a1,zero		# A1 = 0
		move	a0,gp		# A0 = handle
		li	a3,CFE_EPTSEAL  # A3 = entrypoint signature
		LR	t0,0(sp)	# entry point
		j	t0		# go for it.
END(RunProgram)




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

		j	board_setleds		# jump to BSP routine

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
		j	_exc_entry


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

#define CAE_STKSIZE    _regidx(11)

LEAF(cpu_apientry)

		sub	sp,CAE_STKSIZE		# Make room for our stuff

		mfc0	v0,C0_SR		# Get current interrupt flag
		SR	v0,CAE_SRSAVE(sp)	# save on stack
		li	t0,M_SR_IE		# master interrupt control
		not	t0			# disable interrupts
		and	v0,t0			# SR now has IE=0
		mtc0	v0,C0_SR		# put back into CP0

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

		move	gp,a0			# set up new GP
		move	a0,a1			# A0 points at IOCB


#if CFG_MULTI_CPUS
		SPIN_LOCK(cfe_spinlock,t0,t1)
#endif

		CALLINIT_KSEG0(init_table,R_INIT_DOXREQ)  # should not return

#if CFG_MULTI_CPUS
		SPIN_UNLOCK(cfe_spinlock,t0)
#endif

		#
		# Restore the saved registers.
		#

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
