/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Verification Test APIs			File: vapi.S
    *
    *  This module contains special low-level routines for use
    *  by verification programs.
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
#include "bsp_config.h"
#include "mipsmacros.h"

#if CFG_VAPI

#if CFG_EMBEDDED_PIC
#error "CFG_VAPI is not compatible with relocatable code"
#endif

#include "cfe_devfuncs.h"

#include "sb1250_defs.h"
#include "sb1250_regs.h"
#include "sb1250_scd.h"

#include "vapi.h"

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define CALLKSEG1(x) \
                la       t0,x ; \
		or	 t0,K1BASE ; \
		jal	 t0

#ifndef CFG_STACK_SIZE
#define STACK_SIZE	8192
#else
#define STACK_SIZE	((CFG_STACK_SIZE+1023) & ~1023)
#endif


/*Break compile if stack less than 4096.  Vapi_puts() and vapi_dumpgprs()
   uses high 1/4 of stack and needs between 512 and 1024 bytes */
#if  !(STACK_SIZE & ~4095)
#error "To use this module the stack must be at least 4096 bytes"
#endif

#define REGIDX(x) ((x)*8) 

#define SAVE_RA		  REGIDX(0)
#define	SAVE_GP		  REGIDX(1)
#define SAVE_AT		  REGIDX(2)
#define SAVE_T0		  REGIDX(3)
#define SAVE_T1		  REGIDX(4)
#define SAVE_T2		  REGIDX(5)
#define SAVE_T3		  REGIDX(6)
#define SAVE_A0		  REGIDX(7)
#define SAVE_A1		  REGIDX(8)
#define SAVE_A2		  REGIDX(9)

#define SAVE_SIZE	  REGIDX(10)

#define SAVETEMPS(x)				\
		.set    noat ;                  \
                la      k0,x ;			\
		sd	ra,SAVE_RA(k0) ;        \
		sd	gp,SAVE_GP(k0) ;        \
		sd	AT,SAVE_AT(k0) ;        \
		sd	t0,SAVE_T0(k0) ;        \
		sd	t1,SAVE_T1(k0) ;        \
		sd	t2,SAVE_T2(k0) ;        \
		sd	t3,SAVE_T3(k0) ;        \
		sd	a0,SAVE_A0(k0) ;        \
		sd	a1,SAVE_A1(k0) ;        \
		sd	a2,SAVE_A2(k0) ;        \
                .set    at ;                    \
		la	gp,_gp


#define RESTORETEMPS(x)				\
		.set    noat ;                  \
                la      k0,x ;			\
		ld	ra,SAVE_RA(k0) ;        \
		ld	gp,SAVE_GP(k0) ;        \
		ld	AT,SAVE_AT(k0) ;        \
		ld	t0,SAVE_T0(k0) ;        \
		ld	t1,SAVE_T1(k0) ;        \
		ld	t2,SAVE_T2(k0) ;        \
		ld	t3,SAVE_T3(k0) ;        \
		ld	a0,SAVE_A0(k0) ;        \
		ld	a1,SAVE_A1(k0) ;        \
		ld	a2,SAVE_A2(k0) ;        \
                .set    at 

#define RECPTR t3

#define CHECKPTR(label) \
		ld      RECPTR,vapi_logptr ; \
		ld	t0,vapi_logend ; \
		beq	RECPTR,zero,label ; \
		bge	RECPTR,t0,label 

#define SETRECTYPE(x,id) \
                ld      RECPTR,vapi_logptr ; \
                li      t2,(VAPI_CFESEAL | (x)) ; \
		mfc0	t0,C0_PRID ; \
		srl	t0,t0,25 ; \
		and	t0,t0,7 ; \
		sll	t0,t0,VAPI_PRNUM_SHIFT ; \
		or      t2,t2,t0 ; \
                dsll    t2,t2,32 ; \
                or      t2,id ; \
                sd      t2,VAPI_REC_SIGNATURE(RECPTR) ; \
		mfc0    t2,C0_COUNT ; \
		dsll	t2,t2,32 ; \
		sd      t2,VAPI_REC_SIZE(RECPTR) ; \
                sd      ra,VAPI_REC_RA(RECPTR)



#define SETRECLEN_CONST(len) \
		ld      t2,VAPI_REC_SIZE(RECPTR) ; \
		or	t2,len ; \
                sd      t2,VAPI_REC_SIZE(RECPTR)

#define SETRECLEN_REG(r) \
		ld      t2,VAPI_REC_SIZE(RECPTR) ; \
		or	t2,r ; \
                sd      t2,VAPI_REC_SIZE(RECPTR)


/*  *********************************************************************
    *  Data
    ********************************************************************* */

                .sdata

		.globl vapi_logstart
		.globl vapi_logend
		.globl vapi_logptr
		.globl vapi_status
		.globl vapi_logover

vapi_logstart:	.dword 0
vapi_logend:	.dword 0
vapi_logptr:	.dword 0
vapi_status:	.dword -1
vapi_logover:	.dword 0

		.extern	mem_heapstart

		.bss

		.comm	vapi_regsave,REGIDX(64)

		.text

		.globl	vapi_socregs
vapi_socregs:

#ifdef _P5064_
		.word 0, 0
#else
#include "sb1250_socregs.inc"
#endif

		.text

		.extern cfe_warmstart

		.set reorder


/*  *********************************************************************
    *  VAPI_KSEG0_SWITCH
    *  
    *  Hack the return address so we will come back in KSEG0
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(vapi_kseg0_switch)

		and	ra,(K0SIZE-1)
		or	ra,K0BASE
		jr	ra

END(vapi_kseg0_switch)

/*  *********************************************************************
    *  VAPI_EXIT(status)
    *  
    *  Return from diagnostic to firmware
    *  
    *  Input parameters: 
    *  	   a0 - exit status (0=ok, else error)
    *  	   
    *  Return value:
    *  	   does not return
    ********************************************************************* */


LEAF(vapi_exit)

		move	k1,a0


/*
 * Reinitialize the CPU and the caches
 */

		bal	vapi_kseg1_switch
		CALLKSEG1(sb1_cpu_init)
/*
 * Don't initialize the caches again.  Some diags
 * leave data in the caches and if we invalidate it
 * now we won't be able to see what happened.
 */
/*		CALLKSEG1(sb1250_l1cache_init) */
/*		CALLKSEG1(sb1250_l2cache_init) */

#ifdef __long64
/*
 * Set back to 64-bit mode.  Don't worry about the hazard
 * here, it'll be eons before we need to use the KX space.
 */
		mfc0	t0,C0_SR
		or	t0,t0,M_SR_KX
		mtc0	t0,C0_SR
#endif

		bal	vapi_kseg0_switch

		li	a0,0x42424242		# 'BBBB'
		jal	board_setleds

		move	a0,k1

		la	gp,_gp
		sd	a0,vapi_status
		LR	sp,mem_heapstart
		ADD	sp,((CFG_HEAP_SIZE*1024)+STACK_SIZE - 8)

/*
 * Create a log record for the EXIT status.
 */
		ld	t0,vapi_logptr
		beq	t0,zero,nolog

		SETRECTYPE(VAPI_FMT_EXIT,0)
		SETRECLEN_CONST(1)
		sd	a0,VAPI_REC_DATA(RECPTR)
		add	RECPTR,32
		sd	RECPTR,vapi_logptr
nolog:
		li	a0,0x45454545		# 'EEEE'
		jal	board_setleds

#if CFG_MULTI_CPUS

	/*
	 * Restart the other CPU if it was left in RESET.
	 */

		la	t2,PHYS_TO_K1(A_SCD_SYSTEM_CFG)
		ld	t0,0(t2)
		dli	t1,M_SYS_CPU_RESET_1	# Reset mask
		and	t0,t1			# Test if CPU is in reset
		beq	t0,zero,1f		# skip if not in reset

		li	a0,1			# Whack the CPU
		jal	altcpu_cmd_stop		# and put it back in idle
1:
#endif

		ld	a0,vapi_status
		j	cfe_warmstart

END(vapi_exit)



/*  *********************************************************************
    *  VAPI_DUMPGPRS()
    *  
    *  Dump the GPRs to the console
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    *  	   
    *  Registers used:
    *  	   k0 - scratch register for CFE
    ********************************************************************* */

LEAF(vapi_dumpgprs)

		.set	noat
		la	k0,vapi_regsave
		sd	$0,REGIDX(0)(k0)
		sd	$1,REGIDX(1)(k0)
		sd	$2,REGIDX(2)(k0)
		sd	$3,REGIDX(3)(k0)
		sd	$4,REGIDX(4)(k0)
		sd	$5,REGIDX(5)(k0)
		sd	$6,REGIDX(6)(k0)
		sd	$7,REGIDX(7)(k0)
		sd	$8,REGIDX(8)(k0)
		sd	$9,REGIDX(9)(k0)
		sd	$10,REGIDX(10)(k0)
		sd	$11,REGIDX(11)(k0)
		sd	$12,REGIDX(12)(k0)
		sd	$13,REGIDX(13)(k0)
		sd	$14,REGIDX(14)(k0)
		sd	$15,REGIDX(15)(k0)
		sd	$16,REGIDX(16)(k0)
		sd	$17,REGIDX(17)(k0)
		sd	$18,REGIDX(18)(k0)
		sd	$19,REGIDX(19)(k0)
		sd	$20,REGIDX(20)(k0)
		sd	$21,REGIDX(21)(k0)
		sd	$22,REGIDX(22)(k0)
		sd	$23,REGIDX(23)(k0)
		sd	$24,REGIDX(24)(k0)
		sd	$25,REGIDX(25)(k0)
		sd	$26,REGIDX(26)(k0)	/* k0 */
		sd	$27,REGIDX(27)(k0)
		sd	$28,REGIDX(28)(k0)
		sd	$29,REGIDX(29)(k0)
		sd	$30,REGIDX(30)(k0)
		sd	$31,REGIDX(31)(k0)
		.set    at

# Save some CP0 registers here.  
#define LSAVECP0(cp0,idx) \
                dmfc0    t0,cp0 ; \
                sd      t0,REGIDX(idx)(k0) 

		LSAVECP0(C0_INX,32)
		LSAVECP0(C0_RAND,33)
		LSAVECP0(C0_TLBLO0,34)
		LSAVECP0(C0_TLBLO1,35)
		LSAVECP0(C0_CTEXT,36)
		LSAVECP0(C0_PGMASK,37)
		LSAVECP0(C0_WIRED,38)
		LSAVECP0(C0_BADVADDR,39)
		LSAVECP0(C0_COUNT,40)
		LSAVECP0(C0_TLBHI,41)
		LSAVECP0(C0_COMPARE,42)
		LSAVECP0(C0_SR,43)
		LSAVECP0(C0_CAUSE,44)
		LSAVECP0(C0_EPC,45)
		LSAVECP0(C0_PRID,46)
		LSAVECP0(C0_CONFIG,47)
		LSAVECP0(C0_LLADDR,48)
		LSAVECP0(C0_WATCHLO,49)
		LSAVECP0(C0_WATCHHI,50)
		LSAVECP0(C0_XCTEXT,51)
		LSAVECP0(C0_ECC,52)
		LSAVECP0(C0_CACHEERR,53)
		LSAVECP0(C0_TAGLO,54)
		LSAVECP0(C0_TAGHI,55)
		LSAVECP0(C0_ERREPC,56)


		move	a0,k0			/* pass addr of regs */
		la	gp,_gp
		LR	sp,mem_heapstart
		/*use bottom 1/4 of stack so not to trash top of stack */
		ADD	sp,((CFG_HEAP_SIZE*1024)+(STACK_SIZE/4) - 8)
		jal	vapi_dodumpregs		/* dump registers in 'C' */

		.set	noat
		la	k0,vapi_regsave
		ld	$1,REGIDX(1)(k0)
		ld	$2,REGIDX(2)(k0)
		ld	$3,REGIDX(3)(k0)
		ld	$4,REGIDX(4)(k0)
		ld	$5,REGIDX(5)(k0)
		ld	$6,REGIDX(6)(k0)
		ld	$7,REGIDX(7)(k0)
		ld	$8,REGIDX(8)(k0)
		ld	$9,REGIDX(9)(k0)
		ld	$10,REGIDX(10)(k0)
		ld	$11,REGIDX(11)(k0)
		ld	$12,REGIDX(12)(k0)
		ld	$13,REGIDX(13)(k0)
		ld	$14,REGIDX(14)(k0)
		ld	$15,REGIDX(15)(k0)
		ld	$16,REGIDX(16)(k0)
		ld	$17,REGIDX(17)(k0)
		ld	$18,REGIDX(18)(k0)
		ld	$19,REGIDX(19)(k0)
		ld	$20,REGIDX(20)(k0)
		ld	$21,REGIDX(21)(k0)
		ld	$22,REGIDX(22)(k0)
		ld	$23,REGIDX(23)(k0)
		ld	$24,REGIDX(24)(k0)
		ld	$25,REGIDX(25)(k0)
		/*ld	$26,REGIDX(26)(k0)	don't restore k0 */
		ld	$27,REGIDX(27)(k0)
		ld	$28,REGIDX(28)(k0)
		ld	$29,REGIDX(29)(k0)
		ld	$30,REGIDX(30)(k0)
		ld	$31,REGIDX(31)(k0)
		.set    at

		j	ra

END(vapi_dumpgprs)


/*  *********************************************************************
    *  VAPI_SETLOG(start,end)
    *  
    *  Set the address of the log buffer.  This call is required
    *  before any data will be stored in the log.
    *  
    *  Input parameters: 
    *  	   a0 - start of log buffer, 64-bit aligned
    *  	   a1 - end of log buffer, 64-bit aligned
    *  	   
    *  Return value:
    *  	   nothing
    *  	   
    *  Registers used:
    *  	   k0 - scratch register for CFE
    ********************************************************************* */

LEAF(vapi_setlog)

		SAVETEMPS(vapi_regsave)

		sd	a0,vapi_logstart
		sd	a0,vapi_logptr
		sd	a1,vapi_logend
		sd	zero,vapi_logover

		RESTORETEMPS(vapi_regsave)

		j	ra

END(vapi_setlog)

/*  *********************************************************************
    *  VAPI_LOGTRACE(id)
    *  
    *  Store a the contents of the trace buffer to the log
    *  
    *  Input parameters: 
    *  	   a0 - low 32 bits are the ID code to store with the entry
    *  	        in the log.
    *  	   
    *  Return value:
    *  	   nothing
    *  	   
    *  Registers used:
    *  	   k0 - scratch register for CFE
    ********************************************************************* */

LEAF(vapi_logtrace)

		j	ra

END(vapi_logtrace)


/*  *********************************************************************
    *  VAPI_LOGSINGLE(id,value)
    *  
    *  Store a single value in the log.
    *  
    *  Input parameters: 
    *  	   a0 - low 32 bits are the ID code to store with the entry
    *  	        in the log.
    *  	   a1 - value to store in the log
    *  	   
    *  Return value:
    *  	   nothing
    *  	   
    *  Registers used:
    *  	   k0 - scratch register for CFE
    ********************************************************************* */

LEAF(vapi_logsingle)


		SAVETEMPS(vapi_regsave)

		CHECKPTR(99f)

		SETRECTYPE(VAPI_FMT_DATA,a0)
		SETRECLEN_CONST(1)

		sd	a1,VAPI_REC_DATA(RECPTR)

		add	RECPTR,32		# one record
		sd	RECPTR,vapi_logptr		

99:		RESTORETEMPS(vapi_regsave)

		j	ra

END(vapi_logsingle)

/*  *********************************************************************
    *  VAPI_LOGDATA(id,addr,cnt)
    *  
    *  Store multiple values in the log
    *  
    *  Input parameters: 
    *  	   a0 - low 32 bits are the ID code to store with the entry
    *  	        in the log.
    *  	   a1 - Address of values to store in the log
    *      a2 - number of 64-bit values to store in the log
    *  	   
    *  Return value:
    *  	   nothing
    *  	   
    *  Registers used:
    *  	   k0 - scratch register for CFE
    ********************************************************************* */

LEAF(vapi_logdata)

		SAVETEMPS(vapi_regsave)

		CHECKPTR(99f)

		SETRECTYPE(VAPI_FMT_BUFFER,a0)

		add	t1,RECPTR,VAPI_REC_DATA    # a1 = ptr to data ara

		sd	a1,0(t1)
		add	t1,8

		move	k0,a2			# counter for words

1:		beq	k0,zero,2f
		ld	t0,0(a1)
		sd	t0,0(t1)
		add	a1,8
		add	t1,8
		sub	k0,1
		b	1b

2:		add	k0,a2,1			 # total number of words
		SETRECLEN_REG(k0)
		sll	k0,k0,3			# number of words we wrote
		add	k0,24			# size of header
		add	RECPTR,k0
		sd	RECPTR,vapi_logptr

99:		RESTORETEMPS(vapi_regsave)

		j	ra

END(vapi_logdata)


/*  *********************************************************************
    *  VAPI_SAVESOC(id)
    *  
    *  Save the SOC registers in the log
    *  
    *  Input parameters: 
    *  	   a0 - low 32 bits are the ID code to store with the entry
    *  	        in the log
    *      a1 - bitmask of SOC agents to save
    *  	   
    *  Return value:
    *  	   nothing
    *  	   
    *  Registers used:
    *  	   k0 - scratch register for CFE
    ********************************************************************* */

LEAF(vapi_savesoc)

		SAVETEMPS(vapi_regsave)

		CHECKPTR(99f)

		li	t0,VAPI_CFESEAL | VAPI_FMT_SOC
		dsll	t0,t0,32
		or	t0,a0
		mfc0	t1,C0_PRID
		srl	t1,t1,25 
		and	t1,t1,7 
		sll	t1,t1,VAPI_PRNUM_SHIFT 
		or      t0,t0,t1 
		ld	t1,vapi_logptr

		sd	t0,VAPI_REC_SIGNATURE(t1)
		mfc0	t0,C0_COUNT
		dsll	t0,t0,32
		sd	t0,VAPI_REC_SIZE(t1)
		sd	ra,VAPI_REC_RA(t1)

		move	a2,zero			# Counts how many we write

		la	t2,vapi_socregs

1:		lw	t0,0(t2)		# get flags
		beq	t0,zero,2f
		and	t0,t0,a1		# test flags
		beq	t0,zero,3f		# skip if no flags set

		lw	t0,4(t2)		# get address of register

		sd	t0,VAPI_REC_DATA(t1)	# store address of register
		add	t1,8			# next destination addr
		add	a2,1			# count the words written

		or	t0,K1BASE		# Make K1seg
		ld	t0,0(t0)		# Read SOC register

		sd	t0,VAPI_REC_DATA(t1)	# Store in log
		add	t1,8			# next destination addr
		add	a2,1			# count the words written

3:		add	t2,8			# next reg from table

		b	1b

2:		ld	t0,vapi_logptr		# get original pointer
		ld	a1,VAPI_REC_SIZE(t0)	# Get C0_COUNT value
		or	a1,a2			# OR in the record size
		sd	a1,VAPI_REC_SIZE(t0)	# put the record size back

		add	t1,24			# Account for extra fields in record
		sd	t1,vapi_logptr		# Update the pointer

99:		RESTORETEMPS(vapi_regsave)

		j	ra

END(vapi_savesoc)

/*  *********************************************************************
    *  VAPI_LOGGPRS(id)
    *  
    *  Save the general purpose registers and certain CP0 values
    *  in the log.
    *  
    *  Input parameters: 
    *  	   a0 - low 32 bits are the ID code to store with the entry
    *  	        in the log
    *  	   
    *  Return value:
    *  	   nothing
    *  	   
    *  Registers used:
    *  	   k0 - scratch register for CFE
    ********************************************************************* */

#define REGLOG(x) (VAPI_REC_DATA+REGIDX(x))
#define MAXREGS 57
#define REGLOGMAX REGLOG(MAXREGS)

LEAF(vapi_loggprs)

		SAVETEMPS(vapi_regsave)
		CHECKPTR(99f)

		.set	noat
		ld	k0,vapi_logptr
		sd	$0,REGLOG(0)(k0)
		sd	$1,REGLOG(1)(k0)
		sd	$2,REGLOG(2)(k0)
		sd	$3,REGLOG(3)(k0)
		sd	$4,REGLOG(4)(k0)
		sd	$5,REGLOG(5)(k0)
		sd	$6,REGLOG(6)(k0)
		sd	$7,REGLOG(7)(k0)
		sd	$8,REGLOG(8)(k0)
		sd	$9,REGLOG(9)(k0)
		sd	$10,REGLOG(10)(k0)
		sd	$11,REGLOG(11)(k0)
		sd	$12,REGLOG(12)(k0)
		sd	$13,REGLOG(13)(k0)
		sd	$14,REGLOG(14)(k0)
		sd	$15,REGLOG(15)(k0)
		sd	$16,REGLOG(16)(k0)
		sd	$17,REGLOG(17)(k0)
		sd	$18,REGLOG(18)(k0)
		sd	$19,REGLOG(19)(k0)
		sd	$20,REGLOG(20)(k0)
		sd	$21,REGLOG(21)(k0)
		sd	$22,REGLOG(22)(k0)
		sd	$23,REGLOG(23)(k0)
		sd	$24,REGLOG(24)(k0)
		sd	$25,REGLOG(25)(k0)
		sd	$26,REGLOG(26)(k0)
		sd	$27,REGLOG(27)(k0)
		sd	$28,REGLOG(28)(k0)
		sd	$29,REGLOG(29)(k0)
		sd	$30,REGLOG(30)(k0)
		sd	$31,REGLOG(31)(k0)
		.set	at


# Save some CP0 registers here.  
#define SAVECP0(cp0,idx) \
                dmfc0    t0,cp0 ; \
                sd      t0,REGLOG(idx)(k0) 

		SAVECP0(C0_INX,32)
		SAVECP0(C0_RAND,33)
		SAVECP0(C0_TLBLO0,34)
		SAVECP0(C0_TLBLO1,35)
		SAVECP0(C0_CTEXT,36)
		SAVECP0(C0_PGMASK,37)
		SAVECP0(C0_WIRED,38)
		SAVECP0(C0_BADVADDR,39)
		SAVECP0(C0_COUNT,40)
		SAVECP0(C0_TLBHI,41)
		SAVECP0(C0_COMPARE,42)
		SAVECP0(C0_SR,43)
		SAVECP0(C0_CAUSE,44)
		SAVECP0(C0_EPC,45)
		SAVECP0(C0_PRID,46)
		SAVECP0(C0_CONFIG,47)
		SAVECP0(C0_LLADDR,48)
		SAVECP0(C0_WATCHLO,49)
		SAVECP0(C0_WATCHHI,50)
		SAVECP0(C0_XCTEXT,51)
		SAVECP0(C0_ECC,52)
		SAVECP0(C0_CACHEERR,53)
		SAVECP0(C0_TAGLO,54)
		SAVECP0(C0_TAGHI,55)
		SAVECP0(C0_ERREPC,56)

		SETRECTYPE(VAPI_FMT_GPRS,a0)
		SETRECLEN_CONST(MAXREGS)
		add	RECPTR,REGLOGMAX
		sd	RECPTR,vapi_logptr

99:		RESTORETEMPS(vapi_regsave)

		j	ra			# go home
 
END(vapi_loggprs)


/*  *********************************************************************
    *  VAPI_LOGFPRS(id)
    *  
    *  Save the floating point unit's registers
    *  in the log.
    *  
    *  Input parameters: 
    *  	   a0 - low 32 bits are the ID code to store with the entry
    *  	        in the log
    *  	   
    *  Return value:
    *  	   nothing
    *  	   
    *  Registers used:
    *  	   k0 - scratch register for CFE
    ********************************************************************* */


#define SAVEFPR(cp1,idx) \
                dmfc1    t0,cp1 ; \
                sd      t0,FPREGLOG(idx)(k0) 
#define SAVECP1(cp1,idx) \
                cfc1    t0,cp1 ; \
                sd      t0,FPREGLOG(idx)(k0) 

#define FPREGLOG(x) (VAPI_REC_DATA+REGIDX(x))
#define FPMAXREGS 37
#define FPREGLOGMAX FPREGLOG(FPMAXREGS)

LEAF(vapi_logfprs)

		SAVETEMPS(vapi_regsave)
		CHECKPTR(99f)

		ld	k0,vapi_logptr
		SAVEFPR($0,0)
		SAVEFPR($1,1)
		SAVEFPR($2,2)
		SAVEFPR($3,3)
		SAVEFPR($4,4)
		SAVEFPR($5,5)
		SAVEFPR($6,6)
		SAVEFPR($7,7)
		SAVEFPR($8,8)
		SAVEFPR($9,9)
		SAVEFPR($10,10)
		SAVEFPR($11,11)
		SAVEFPR($12,12)
		SAVEFPR($13,13)
		SAVEFPR($14,14)
		SAVEFPR($15,15)
		SAVEFPR($16,16)
		SAVEFPR($17,17)
		SAVEFPR($18,18)
		SAVEFPR($19,19)
		SAVEFPR($20,20)
		SAVEFPR($21,21)
		SAVEFPR($22,22)
		SAVEFPR($23,23)
		SAVEFPR($24,24)
		SAVEFPR($25,25)
		SAVEFPR($26,26)
		SAVEFPR($27,27)
		SAVEFPR($28,28)
		SAVEFPR($29,29)
		SAVEFPR($30,30)
		SAVEFPR($31,31)

		SAVECP1($0,32)		/* FIR */
		SAVECP1($31,33)		/* Status */
		SAVECP1($25,34)		/* condition codes */
		SAVECP1($26,35)		/* Exceptions */
		SAVECP1($28,36)		/* enables */

		SETRECTYPE(VAPI_FMT_FPRS,a0)
		SETRECLEN_CONST(FPMAXREGS)
		add	RECPTR,FPREGLOGMAX
		sd	RECPTR,vapi_logptr

99:		RESTORETEMPS(vapi_regsave)

		j	ra			# go home
 
END(vapi_logfprs)

/*  *********************************************************************
    *  VAPI_PUTS(string)
    *  
    *  Display a string on the console
    *  
    *  Input parameters: 
    *  	   a0 - pointer to null-terminated string
    *  	   
    *  Return value:
    *  	   nothing
    *  	   
    *  Registers used:
    *  	   k0 - scratch register for CFE
    ********************************************************************* */

LEAF(vapi_puts)

		.set	noat
		la	k0,vapi_regsave
		sd	$0,REGIDX(0)(k0)
		sd	$1,REGIDX(1)(k0)
		sd	$2,REGIDX(2)(k0)
		sd	$3,REGIDX(3)(k0)
		sd	$4,REGIDX(4)(k0)
		sd	$5,REGIDX(5)(k0)
		sd	$6,REGIDX(6)(k0)
		sd	$7,REGIDX(7)(k0)
		sd	$8,REGIDX(8)(k0)
		sd	$9,REGIDX(9)(k0)
		sd	$10,REGIDX(10)(k0)
		sd	$11,REGIDX(11)(k0)
		sd	$12,REGIDX(12)(k0)
		sd	$13,REGIDX(13)(k0)
		sd	$14,REGIDX(14)(k0)
		sd	$15,REGIDX(15)(k0)
		sd	$16,REGIDX(16)(k0)
		sd	$17,REGIDX(17)(k0)
		sd	$18,REGIDX(18)(k0)
		sd	$19,REGIDX(19)(k0)
		sd	$20,REGIDX(20)(k0)
		sd	$21,REGIDX(21)(k0)
		sd	$22,REGIDX(22)(k0)
		sd	$23,REGIDX(23)(k0)
		sd	$24,REGIDX(24)(k0)
		sd	$25,REGIDX(25)(k0)
		sd	$26,REGIDX(26)(k0)	/* k0 */
		sd	$27,REGIDX(27)(k0)
		sd	$28,REGIDX(28)(k0)
		sd	$29,REGIDX(29)(k0)
		sd	$30,REGIDX(30)(k0)
		sd	$31,REGIDX(31)(k0)
		.set    at

		la	gp,_gp
		LR	sp,mem_heapstart
		/*use bottom 1/4 of stack so not to trash top of stack */
		ADD	sp,((CFG_HEAP_SIZE*1024)+(STACK_SIZE/4) - 8)
	            
		jal	vapi_doputs		/* dump registers in 'C' */
                          
		.set	noat
		la	k0,vapi_regsave
		ld	$1,REGIDX(1)(k0)
		ld	$2,REGIDX(2)(k0)
		ld	$3,REGIDX(3)(k0)
		ld	$4,REGIDX(4)(k0)
		ld	$5,REGIDX(5)(k0)
		ld	$6,REGIDX(6)(k0)
		ld	$7,REGIDX(7)(k0)
		ld	$8,REGIDX(8)(k0)
		ld	$9,REGIDX(9)(k0)
		ld	$10,REGIDX(10)(k0)
		ld	$11,REGIDX(11)(k0)
		ld	$12,REGIDX(12)(k0)
		ld	$13,REGIDX(13)(k0)
		ld	$14,REGIDX(14)(k0)
		ld	$15,REGIDX(15)(k0)
		ld	$16,REGIDX(16)(k0)
		ld	$17,REGIDX(17)(k0)
		ld	$18,REGIDX(18)(k0)
		ld	$19,REGIDX(19)(k0)
		ld	$20,REGIDX(20)(k0)
		ld	$21,REGIDX(21)(k0)
		ld	$22,REGIDX(22)(k0)
		ld	$23,REGIDX(23)(k0)
		ld	$24,REGIDX(24)(k0)
		ld	$25,REGIDX(25)(k0)
		/*ld	$26,REGIDX(26)(k0)	don't restore k0 */
		ld	$27,REGIDX(27)(k0)
		ld	$28,REGIDX(28)(k0)
		ld	$29,REGIDX(29)(k0)
		ld	$30,REGIDX(30)(k0)
		ld	$31,REGIDX(31)(k0)
		.set    at

		j	ra

END(vapi_puts)

/*  *********************************************************************
    *  VAPI_SETLEDS(leds)
    *  
    *  Set the onboard LEDS on the swarm board.
    *  
    *  Input parameters: 
    *  	   a0 - LED value, "ABCD" is 0x41424344
    *  	   
    *  Return value:
    *  	   nothing
    *  	   
    *  Registers used:
    *  	   k0 - scratch register for CFE
    ********************************************************************* */


LEAF(vapi_setleds)

		SAVETEMPS(vapi_regsave)

		jal	board_setleds

		RESTORETEMPS(vapi_regsave)

		j	ra

END(vapi_setleds)

/*  *********************************************************************
    *  VAPI_KSEG1_SWITCH
    *  
    *  Hack the return address so we will come back in KSEG1 (uncached)
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(vapi_kseg1_switch)

		and	ra,(K0SIZE-1)
		or	ra,K1BASE
		jr	ra

END(vapi_kseg1_switch)


/*  *********************************************************************
    *  VAPI_RUN()
    *  
    *  Jump to the diagnostic program, which must be loaded at the
    *  special address (typically 8002_0000).  First we flush the
    *  cache, then set magic #'s in the mailbox.   Finally, the core
    *  is reset.  On restart, we do minimal initialization and jump
    *  directly to the diagnostic.
    *  
    *  Input parameters: 
    *  	   a0 - nonzero to restart uncached.
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(vapi_run)

	/*
	 * Run uncached
	 */

		bal	vapi_kseg1_switch	# now running in KSEG1 

	/*
	 * Flush the caches
	 */

		move	s0,a0			# L2 flush trashes A0
		CALLKSEG1(sb1250_l1cache_flush_d)
		CALLKSEG1(sb1250_l1cache_inval_i)
		CALLKSEG1(sb1250_l2cache_flush)
		move	a0,s0

#ifdef _P5064_

       /* In the case of the P5064, just jump directly to the entry point */

		li	t0,VAPI_DIAG_ENTRY
		j	t0

#else

	/*
	 * Set the magic code in the mailbox.
	 */

		li	t0,-1
		la	t1,PHYS_TO_K1(A_IMR_REGISTER(0,R_IMR_MAILBOX_CLR_CPU))
		sd	t0,0(t1)

		dli	t0,VAPI_MAGIC_NUMBER
		beq	a0,0,1f
		dli	t0,VAPI_MAGIC_NUMBER_UNC
		beq	a0,1,1f
		dli	t0,VAPI_MAGIC_NUMBER_MC
1:		la	t1,PHYS_TO_K1(A_IMR_REGISTER(0,R_IMR_MAILBOX_SET_CPU))
		sd	t0,0(t1)

	/*
	 * Whack the reset line.
	 */
#if defined(_PTSWARM_)
		li	k0,PHYS_TO_K1(0x1B0A0000+32+8*3)
#else
		li	k0,PHYS_TO_K1(0x100A0000+32+8*3)
#endif
		li	k1,'!'

		li	t1,PHYS_TO_K1(A_SCD_SYSTEM_CFG)
		ld	t2,0(t1)
		dli	t0,M_SYS_CPU_RESET_0 | M_SYS_CPU_RESET_1
		or	t2,t2,t0
		bal	vapi_kseg0_switch
		.align 5
#if defined(_CSWARM_) || defined(_SWARM_) || defined(_PTSWARM_)
		sb	k1,0(k0)
#else
		nop
#endif
		sync			/* flush the write buffer */
		sd	t2,0(t1)
1:		b	1b

	/*
	 * And he never returned, no he never returned... and his fate
	 * is still unknown, he will ride forever 'neath the cycles of
	 * the SB1, he's the core that never returned!
	 */
#endif



END(vapi_run)


LEAF(vapi_flushtest)

		move	s1,ra

	/*
	 * Run uncached
	 */

		bal	vapi_kseg1_switch	# now running in KSEG1

	/*
	 * Flush the caches
	 */

		move	s0,a0			# L2 flush trashes A0
		CALLKSEG1(sb1250_l1cache_flush_d)
		CALLKSEG1(sb1250_l1cache_inval_i)
		CALLKSEG1(sb1250_l2cache_flush)
		move	a0,s0

	/*
	 * Back to cached
	 */

		bal	vapi_kseg0_switch	# now running in KSEG1

		move	ra,s1
		j	ra

END(vapi_flushtest)


#endif /* CFG_VAPI */

/*  *********************************************************************
    *  End
    ********************************************************************* */
