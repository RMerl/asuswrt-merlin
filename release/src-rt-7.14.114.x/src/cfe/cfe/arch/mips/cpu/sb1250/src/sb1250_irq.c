/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Polled interrupt dispatch routines	File: sb1250_irq.c
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

/*    *********************************************************************
    *  This module provides an interface for associating service
    *  routines with SB-1250 system and LDT interrupt sources.  The
    *  various interrupt mapper registers are periodically polled
    *  and the requested service routine is invoked when a
    *  corresponding interrupt request is active and enabled.
    *
    *  The interface is loosely based on irq.c from Linux.
    *
    *  This is not a full-fledged interrupt handler.
    *
    *  If CFG_INTERRUPTS == 0, it operates synchronously with the
    *  main polling loop and is never invoked directly by the
    *  hardware exception handler.  If CFG_INTERRUPTS == 1, certain
    *  interrupt sources can be handled asynchronously as exceptions.
    *
    *  For now, only CPU0 polls interrupts, via its interrupt mapper.
    *
    ********************************************************************* */

#include "lib_types.h"
#include "lib_printf.h"
#include "lib_malloc.h"

#include "sbmips.h"
#include "sb1250_regs.h"
#include "sb1250_int.h"
extern void sb1250_update_sr(uint32_t clear, uint32_t set);

#include "exception.h"
#include "cfe_irq.h"
extern void sb1250_irq_install(void);
extern void sb1250_irq_arm(void);


/* First level dispatching (MIPS IP level). */

#define IP_LEVELS 8

/* Shared variables that must be protected in non-interrupt code. */
static ip_handler_t ip_handler[IP_LEVELS] = {NULL};

void
cfe_irq_setvector(int index, ip_handler_t handler)
{
    if (index >= 0 && index < IP_LEVELS) {
	uint32_t set, clear;

	ip_handler[index] = NULL;   /* disable: see demux */
	if (handler == NULL) {
	    clear = _MM_MAKEMASK1(S_SR_IMMASK + index);
	    set = 0;
	    }
	else {
	    clear = 0;
	    set = _MM_MAKEMASK1(S_SR_IMMASK + index);
	    }
	sb1250_update_sr(clear, set);
	ip_handler[index] = handler;
    }
}


/*
 * Dispatch function called from the exception handler for
 * asynchronous (non-polled) interrupts.
 * info is a pointer to the saved register block.
 *
 * At entry, interrupts will be masked.
 */

static void
sb1250_irq_demux(int code, uint64_t *info)
{
    uint32_t pending;

    pending = info[XCP0_CAUSE] & info[XCP0_SR];

    /* For now, we handle IP7 (internal timers) and IP2 (mapper) only */

    if (pending & M_CAUSE_IP7) {
	if (ip_handler[7] != NULL) {
	    (*(ip_handler[7]))(7);
	    }
	else {
	    /* mask off IP7, else we're caught here forever */
	    sb1250_update_sr(M_SR_IM7, 0);
	    }
	}

    if (pending & M_CAUSE_IP2) {
	if (ip_handler[2] != NULL) {
	    (*(ip_handler[2]))(2);
	    }
	else {
	    /* mask off IP2, else we're caught here forever */
	    sb1250_update_sr(M_SR_IM2, 0);
	    }
	}
}


/*
 * Initialize the MIPS level dispatch vector.
 * This function should be called with interrupts disabled.
 */

static void
sb1250_irq_vectorinit(void)
{
    int  i;

    for (i = 0; i < IP_LEVELS; i++) {
	ip_handler[i] = NULL;
	}
    _exc_setvector(XTYPE_INTERRUPT, (void *) sb1250_irq_demux);
    sb1250_irq_arm();
}


#define IMR_POINTER(cpu,reg) \
    ((volatile uint64_t *)(PHYS_TO_K1(A_IMR_REGISTER(cpu,reg))))

typedef struct irq_action_s irq_action_t;
struct irq_action_s {
    void (*handler)(void *arg);
    void *arg;
    unsigned long flags;
    int device;              /* to distinguish actions for shared interrupts */
    irq_action_t *next;
};

typedef struct irq_desc_s {
    irq_action_t *actions;
    int depth;
    int status;
} irq_desc_t;

/* status flags */
#define IRQ_DISABLED  0x0001

/* Shared variables that must be protected in non-interrupt code. */
static irq_desc_t irq_desc[NR_IRQS];

/*
 *  cfe_irq_init is called early in the boot sequence.  It is
 *  responsible for setting up the interrupt mapper and initializing
 *  the handler table that will be used for dispatching pending
 *  interrupts.  If hard interrupts are used, it then enables hardware
 *  interrupts (initially all masked).
 *
 *  This function should be called before interrupts are enabled.
 */

void
cfe_irq_init(void)
{
    int i;
    irq_desc_t *p;

    for (i = 0, p = irq_desc; i < NR_IRQS; i++, p++) {
        p->actions = NULL;
	p->depth = 0;
	p->status = IRQ_DISABLED;
	}
	
    /* initially, all interrupts are masked */
    *IMR_POINTER(0, R_IMR_INTERRUPT_MASK) = ~((uint64_t)0);
    *IMR_POINTER(1, R_IMR_INTERRUPT_MASK) = ~((uint64_t)0);

    *IMR_POINTER(0, R_IMR_INTERRUPT_DIAG) = 0;
    *IMR_POINTER(1, R_IMR_INTERRUPT_DIAG) = 0;

    for (i = 0; i < R_IMR_INTERRUPT_MAP_COUNT; i++) {
        *IMR_POINTER(0, R_IMR_INTERRUPT_MAP_BASE + 8*i) = 0;
        *IMR_POINTER(1, R_IMR_INTERRUPT_MAP_BASE + 8*i) = 0;
	}

    sb1250_irq_install();

    sb1250_irq_vectorinit();

#if CFG_INTERRUPTS
    cfe_irq_setvector(2, (ip_handler_t)cfe_irq_poll);
#endif
}


/* cfe_mask_irq() is called to mask an interrupt at the hw level */
void
cfe_mask_irq(int cpu, unsigned int irq)
{
    int sr;

    if (irq >= NR_IRQS)
	return;

    sr = cfe_irq_disable();
    *IMR_POINTER(cpu, R_IMR_INTERRUPT_MASK) |= ((uint64_t)1 << irq);
    __asm__ __volatile__ ("sync" : : : "memory");
    cfe_irq_enable(sr);
}


/* cfe_unmask_irq() is called to unmask an interrupt at the hw level */
void
cfe_unmask_irq(int cpu, unsigned int irq)
{
    int sr;

    if (irq >= NR_IRQS)
	return;

    sr = cfe_irq_disable();
    *IMR_POINTER(cpu, R_IMR_INTERRUPT_MASK) &=~ ((uint64_t)1 << irq);
    cfe_irq_enable(sr);
}


/* If depth is 0, unmask the interrupt. Increment depth. */
void
cfe_enable_irq(unsigned int irq)
{
    int sr;
    irq_desc_t *desc;

    if (irq >= NR_IRQS)
	return;

    desc = &irq_desc[irq];
    /* The following code must be atomic */
    sr = cfe_irq_disable();
    if (desc->depth == 0) {
	*IMR_POINTER(0, R_IMR_INTERRUPT_MASK) &=~ ((uint64_t)1 << irq);
	desc->status &=~ IRQ_DISABLED;
	}
    desc->depth++;
    cfe_irq_enable(sr);
}


/* Decrement depth. If depth is 0, mask the interrupt. */
void
cfe_disable_irq(unsigned int irq)
{
    int sr;
    irq_desc_t *desc;

    if (irq >= NR_IRQS)
	return;

    desc = &irq_desc[irq];
    /* The following code must be atomic */
    sr = cfe_irq_disable();
    desc->depth--;
    if (desc->depth == 0) {
	*IMR_POINTER(0, R_IMR_INTERRUPT_MASK) |= ((uint64_t)1 << irq);
	desc->status |= IRQ_DISABLED;
	__asm__ __volatile__ ("sync" : : : "memory");
	}
    cfe_irq_enable(sr);
}


/*
 *  cfe_request_irq() is called by drivers to request addition to the
 *  chain of handlers called for a given interrupt.  
 */
int
cfe_request_irq(unsigned int irq, 
		void (*handler)(void *), void *arg,
		unsigned long irqflags, int device)
{
    int sr;
    irq_action_t *action;
    irq_desc_t *desc;
    irq_action_t *p;

    if (irq >= NR_IRQS) {
        return -1;
	}
    if (handler == NULL) {
        return -1;
	}

    action = (irq_action_t *) KMALLOC(sizeof(irq_action_t), 0);
    if (action == NULL) {
        return -1;
	}

    action->handler = handler;
    action->arg = arg;
    action->flags = irqflags;
    action->device = device;
    action->next = NULL;

    desc = &irq_desc[irq];

    /* The following block of code has to be executed atomically */
    sr = cfe_irq_disable();
    p = desc->actions;
    if (p == NULL) {
        desc->actions = action;
        desc->depth = 0;
        desc->status |= IRQ_DISABLED;

	 /* always direct to IP[2] for now */
	*IMR_POINTER(0, R_IMR_INTERRUPT_MAP_BASE + 8*irq) = 0;
	__asm__ __volatile__ ("sync" : : : "memory");
	}
    else {
        /* Can't share interrupts unless both agree to */
        if ((p->flags & action->flags & CFE_IRQ_FLAGS_SHARED) == 0) {
	    cfe_enable_irq(irq);
	    KFREE(action);
	    xprintf("cfe_request_irq: conflicting unsharable interrupts.\n");
            return -1;
	    }
	while (p->next != NULL) {
	    p = p->next;
	    }
	p->next = action;
	}

    cfe_enable_irq(irq);
    cfe_irq_enable(sr);
    return 0;
}


/*
 *  free_irq() releases a handler set up by request_irq()
 */
void
cfe_free_irq(unsigned int irq, int device)
{
    int sr;
    irq_desc_t *desc;
    irq_action_t *p, *q;

    if (irq >= NR_IRQS)
        return;

    desc = &irq_desc[irq];

    /* The following block of code has to be executed atomically */
    sr = cfe_irq_disable();
    p = desc->actions;
    q = NULL;
    while (p != NULL) {
	if (p->device == device) {
	    cfe_disable_irq(irq);
	    if (q == NULL) {
		desc->actions = p->next;
		if (desc->actions == NULL)
		    desc->status |= IRQ_DISABLED;
		}
	    else
		q->next = p->next;
	    break;
	    }
	else {
	    q = p;
	    p = p->next;
	    }
	}
    cfe_irq_enable(sr);
    if (p != NULL)
	KFREE(p);
}


/* The interrupt polling code calls this routine to dispatch each
   pending, unmasked interrupt.

   For asynchronous interrupt processing, it is entered with
   interrupts disabled. */

void sb1250_dispatch_irq(unsigned int irq);
void
sb1250_dispatch_irq(unsigned int irq)
{
    irq_action_t *action;
    irq_desc_t *desc = &irq_desc[irq];

    for (action = desc->actions; action != NULL; action = action->next) {
	if (action->handler != NULL) {
	    (*action->handler)(action->arg);
	    }
	}
}
