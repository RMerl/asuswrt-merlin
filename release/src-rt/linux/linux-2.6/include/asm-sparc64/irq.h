/* $Id: irq.h,v 1.21 2002/01/23 11:27:36 davem Exp $
 * irq.h: IRQ registers on the 64-bit Sparc.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1998 Jakub Jelinek (jj@ultra.linux.cz)
 */

#ifndef _SPARC64_IRQ_H
#define _SPARC64_IRQ_H

#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <asm/pil.h>
#include <asm/ptrace.h>

/* IMAP/ICLR register defines */
#define IMAP_VALID		0x80000000	/* IRQ Enabled		*/
#define IMAP_TID_UPA		0x7c000000	/* UPA TargetID		*/
#define IMAP_TID_JBUS		0x7c000000	/* JBUS TargetID	*/
#define IMAP_TID_SHIFT		26
#define IMAP_AID_SAFARI		0x7c000000	/* Safari AgentID	*/
#define IMAP_AID_SHIFT		26
#define IMAP_NID_SAFARI		0x03e00000	/* Safari NodeID	*/
#define IMAP_NID_SHIFT		21
#define IMAP_IGN		0x000007c0	/* IRQ Group Number	*/
#define IMAP_INO		0x0000003f	/* IRQ Number		*/
#define IMAP_INR		0x000007ff	/* Full interrupt number*/

#define ICLR_IDLE		0x00000000	/* Idle state		*/
#define ICLR_TRANSMIT		0x00000001	/* Transmit state	*/
#define ICLR_PENDING		0x00000003	/* Pending state	*/

/* The largest number of unique interrupt sources we support.
 * If this needs to ever be larger than 255, you need to change
 * the type of ino_bucket->virt_irq as appropriate.
 *
 * ino_bucket->virt_irq allocation is made during {sun4v_,}build_irq().
 */
#define NR_IRQS    255

extern void irq_install_pre_handler(int virt_irq,
				    void (*func)(unsigned int, void *, void *),
				    void *arg1, void *arg2);
#define irq_canonicalize(irq)	(irq)
extern unsigned int build_irq(int inofixup, unsigned long iclr, unsigned long imap);
extern unsigned int sun4v_build_irq(u32 devhandle, unsigned int devino);
extern unsigned int sun4v_build_virq(u32 devhandle, unsigned int devino);
extern unsigned int sun4v_build_msi(u32 devhandle, unsigned int *virt_irq_p,
				    unsigned int msi_devino_start,
				    unsigned int msi_devino_end);
extern void sun4v_destroy_msi(unsigned int virt_irq);
extern unsigned int sbus_build_irq(void *sbus, unsigned int ino);

static __inline__ void set_softint(unsigned long bits)
{
	__asm__ __volatile__("wr	%0, 0x0, %%set_softint"
			     : /* No outputs */
			     : "r" (bits));
}

static __inline__ void clear_softint(unsigned long bits)
{
	__asm__ __volatile__("wr	%0, 0x0, %%clear_softint"
			     : /* No outputs */
			     : "r" (bits));
}

static __inline__ unsigned long get_softint(void)
{
	unsigned long retval;

	__asm__ __volatile__("rd	%%softint, %0"
			     : "=r" (retval));
	return retval;
}

#endif
