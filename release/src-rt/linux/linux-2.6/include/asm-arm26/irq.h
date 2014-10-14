#ifndef __ASM_ARM_IRQ_H
#define __ASM_ARM_IRQ_H

#include <asm/sysirq.h>

#ifndef NR_IRQS
#define NR_IRQS	128
#endif


/* JMA 18.05.02 Copied off arch/arm/irq.h */
#ifndef irq_canonicalize
#define irq_canonicalize(i)     (i)
#endif


/*
 * Use this value to indicate lack of interrupt
 * capability
 */
#ifndef NO_IRQ
#define NO_IRQ	((unsigned int)(-1))
#endif

struct irqaction;

#define disable_irq_nosync(i) disable_irq(i)

extern void disable_irq(unsigned int);
extern void enable_irq(unsigned int);

#define __IRQT_FALEDGE	(1 << 0)
#define __IRQT_RISEDGE	(1 << 1)
#define __IRQT_LOWLVL	(1 << 2)
#define __IRQT_HIGHLVL	(1 << 3)

#define IRQT_NOEDGE	(0)
#define IRQT_RISING	(__IRQT_RISEDGE)
#define IRQT_FALLING	(__IRQT_FALEDGE)
#define IRQT_BOTHEDGE	(__IRQT_RISEDGE|__IRQT_FALEDGE)
#define IRQT_LOW	(__IRQT_LOWLVL)
#define IRQT_HIGH	(__IRQT_HIGHLVL)
#define IRQT_PROBE	(1 << 4)

int set_irq_type(unsigned int irq, unsigned int type);

#endif

