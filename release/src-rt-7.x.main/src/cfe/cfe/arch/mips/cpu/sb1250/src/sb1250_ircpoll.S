/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  IRQ polling				File: sb1250_ircpoll.S
    *  
    *  This module contains code to poll the interrupt controller
    *  and invoke the servicing of pending, unmasked interrupts.
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

#include "mipsmacros.h"
#incldue "initdata.h"
#include "sbmips.h"
#include "sb1250_regs.h"
#include "sb1250_int.h"

/*
 * External interrupt conventions:
 *   i5 (IP7) is reserved for sources within the CPU
 *   i0 (IP2) is used for polling and always masked
 *   i1 (IP3) is used for armed interrupts that are registered (not yet)
 *   i2-i4 are available for specific devices.
 */
	
/* EOIs are issued when LDT interrupts are programmed as levels. */
	
#define A_LDT_EOI 0x90000000d8000000
#define D_EOI_MDT (0x7 << 2)
	
/*
 * Dispatch function address.  Access to this function pointer assumes
 * that gp has been initialized and any relocation has been done.
 */
	.sdata

	.globl	irq_dispatch
irq_dispatch:
	_LONG_	0

	.text	
	
/*  *********************************************************************
    * sb1250_irq_install()
    *	
    * Initialize dispatch pointer.  See CALLINIT_RELOC in mipsmacros.h
    * for the logic used here.  This function should be called after
    * any relocation is done and gp is initialized.
    *
    * This function should be called with interrupts disabled.
    *
    * Input parameters:	
    *	nothing
    *
    * Return value:	
    *	nothing					
    ********************************************************************* */
	
	.set    push
	.set	noreorder

	.extern	sb1250_dispatch_irq

dispatch_func:
	_LONG_	sb1250_dispatch_irq
	
LEAF(sb1250_irq_install)
	
	move	a0, ra                          /* LOADREL can clobber */
	LOADREL(v0,dispatch_func)
	move	ra, a0
	LR	v0, 0(v0)
#if CFG_EMBEDDED_PIC
	beq	gp, zero, 1f			/* is this ever ok? */
	LR	v1, mem_textreloc
	daddu	v0, v0, v1
#endif
1:	SR	v0, irq_dispatch

	jr	ra
	nop	

END(sb1250_irq_install)

	.set	pop

	
/*  *********************************************************************
    *  CFE_IRQ_POLL
    *  
    *  Scan the interrupt_source_status and interrupt_ldt registers.
    *  For those not masked by the interrupt_mask register, invoke
    *  cfe_dispatch_irq with the interrupt number as argument.
    *
    *  For LDT-signaled interrupts, also clear the pending vector.
    *  
    *  For asynchronous dispatch, called with interrupts disabled.
    *
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

	.set    push
	.set	noreorder
	.set	noat
	.set    mips64


        /* This version also shows one way of saving 64-bit registers
	in systems that don't necessarily preserve the high 32 bits
	across subroutine calls.  That is not an issue for CFE with
	current compilation options but is required for, e.g., 32-bit
	Linux and NetBSD systems. */
	
LEAF(cfe_irq_poll)

	daddiu	sp, -56		/* saved register space */
	sd	s0, 32(sp)
	sd	s1, 40(sp)
	sd	ra, 48(sp)

	mfc0	a0, C0_CAUSE
	andi	a0, a0, M_CAUSE_IP2
	beqz	a0, 4f
	
	la	v0, K1BASE + A_IMR_CPU0_BASE
	ld	a1, R_IMR_INTERRUPT_MASK(v0)    
        ld	s0, R_IMR_INTERRUPT_SOURCE_STATUS(v0)
	ld	s1, R_IMR_LDT_INTERRUPT(v0)
        nor	a1, a1, zero	/* Negate mask to turn it into an and mask */
	or	s0, s0, s1	/* Unified pending bits */
	and	s0, s0, a1	/* Bit vector of unmasked pending IRQs */
	and	s1, s1, a1      /* ... and unmasked LDT IRQs */
	beqz	s0, 4f		/* No interrupts.  Return */
	nop
	
	/* Clear the accumulated LDT (edge-triggered) interrupts */
	sd	s1, R_IMR_LDT_INTERRUPT_CLR(v0)

	/* The bit scan loop */
	dclz	s1, s0		/* Find index of the next interrupt */
3:
	li	a1, 1
	dsubu	a0, zero, s1
	daddiu	a0, a0, 63     
	dsllv	a1, a1, a0
	xor	s0, s0, a1	/* clear current bit */
	LR	v0, irq_dispatch
	jalr	ra, v0
	dsrl32	s1, s0, 0	/* Save upper 32 bits of unified vector */

	dsll32	s0, s0, 0	/* clear upper bits of s0            */
	dsll32	s1, s1, 0	/* get saved back in the right place */
	dsrl32	s0, s0, 0	/* realign s0                        */
	or	s0, s0, s1	/* restore the saved bits            */
	bnez	s0, 3b		/* More interrupts to service?       */
	dclz	s1, s0		/* unroll for branch delay slot */
4:                        
	ld	ra, 48(sp)	/* restore registers */
	ld	s1, 40(sp)
	ld	s0, 32(sp)
	
	jr	ra
	daddiu	sp, 56		/* saved register space */
		
END(cfe_irq_poll)
	
	.set	pop


/*  *********************************************************************
    * sb1250_irq_arm()
    *	
    * Set up CP0 Status and Cause per conventions above (not really -- yet)
    *
    * This function should be called with interrupts disabled.
    *
    * Input parameters:	
    *	nothing
    *
    * Return value:	
    *	nothing					
    ********************************************************************* */
	
	.set    push
	.set	noreorder

LEAF(sb1250_irq_arm)
	
	mfc0	t0,C0_CAUSE
	or	t0,t0,M_CAUSE_IV
	mtc0	t0,C0_CAUSE

	mfc0	t0,C0_SR
	li	t1,M_SR_IMMASK		/* Mask all interrupt levels */
	nor	t1,t1,zero
	and	t0,t0,t1
	or	t0,t0,M_SR_IE		/* but set IE */
	mtc0	t0,C0_SR
	
	ssnop
	ssnop
	ssnop
	ssnop

	jr	ra
	nop	

END(sb1250_irq_arm)

	.set	pop



	
	.set    push
	.set	noreorder

LEAF(cfe_irq_disable)
	
	mfc0	t0,C0_SR
	li	t1,M_SR_IMMASK|M_SR_IE
	li	t2,~(M_SR_IMMASK|M_SR_IE)
	and	v0,t0,t1                /* current mask bits */
	and	t0,t0,t2
	mtc0	t0,C0_SR                /* all enables cleared */
	
	ssnop
	ssnop
	ssnop
	ssnop

	jr	ra
	nop	

END(cfe_irq_disable)

	.set	pop

	
	
	.set    push
	.set	noreorder

LEAF(cfe_irq_enable)
	
	mfc0	t0,C0_SR
	li	t1,M_SR_IMMASK|M_SR_IE
	li	t2,~(M_SR_IMMASK|M_SR_IE)
	and	a0,a0,t1
	and	t0,t0,t2
	or	t0,t0,a0
	mtc0	t0,C0_SR
	
	ssnop
	ssnop
	ssnop
	ssnop

	jr	ra
	nop	

END(cfe_irq_enable)

	.set	pop


/*  *********************************************************************
    * sb1250_update_sr(clear,set)
    *	
    * Upate Status.IM according to masks
    *
    * Caller should disable interrupts if the effect is to be atomic.
    *
    * Input parameters:	
    *	a0         SR bits to be cleared
    *   a1         SR bits to be set	
    *
    * Return value:	
    *	none
    ********************************************************************* */
	
	.set    push
	.set	noreorder

LEAF(sb1250_update_sr)
	
	nor	a0,a0,zero
	mfc0	t0,C0_SR
	and	t0,t0,a0                /* current mask bits */
	or	t0,t0,a1
	mtc0	t0,C0_SR                /* all enables cleared */
	
	ssnop
	ssnop
	ssnop
	ssnop

	jr	ra
	nop	

END(sb1250_update_sr)

	.set	pop
