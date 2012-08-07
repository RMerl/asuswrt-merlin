/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  IRQ related definitions			File: cfe_irq.h
    *  
    *  This module describes CFE's interface for dispatching
    *  to driver-supplied service routines.  Dispatch can be based
    *  either on asynchronous interrupt events or on synchrnous
    *  polling of the interrupt status registers from the idle loop.
    *
    *  The interface attempts to accomodate the concept of interrupt
    *  mapping as is common on high-integration parts with standard
    *  cores.  The details are motivated by the bcm1250/MIPS
    *  architecture, where the mapping abstraction is somewhat violated
    *  by CP0 interrupts that do not go through the mapper.
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

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define NR_IRQS  64

#define CFE_IRQ_FLAGS_SHARED  0x1

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

/*  *********************************************************************
    *  Functions
    ********************************************************************* */

void cfe_irq_init(void);


/* Functions that use interrupt mapping, i.e., the irq argument is the
   interrupt number at the input to the mapper. */

void cfe_mask_irq(int cpu, unsigned int irq);
void cfe_unmask_irq(int cpu, unsigned int irq);

void cfe_enable_irq(unsigned int irq);
void cfe_disable_irq(unsigned int irq);

int cfe_request_irq(unsigned int irq, 
		    void (*handler)(void *), void *arg,
		    unsigned long irqflags, int device);
void cfe_free_irq(unsigned int irq, int device);

/* pseudo-interrupts, by polling request lines and invoking handlers */

void cfe_irq_poll(void *);


/* Functions that bypass interrupt mapping, i.e., the ip argument
   is the interrupt number at the output of the mapper and/or the
   input to the CPU. */

typedef void (* ip_handler_t)(int ip);

void cfe_irq_setvector(int ip, ip_handler_t handler);

/* enable/disable interrupts at the CPU level. */

void cfe_irq_enable(int mask);
int cfe_irq_disable(void);
