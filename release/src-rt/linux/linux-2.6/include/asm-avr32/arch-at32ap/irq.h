#ifndef __ASM_AVR32_ARCH_IRQ_H
#define __ASM_AVR32_ARCH_IRQ_H

#define EIM_IRQ_BASE	NR_INTERNAL_IRQS
#define NR_EIM_IRQS	32

#define AT32_EXTINT(n)	(EIM_IRQ_BASE + (n))

#define GPIO_IRQ_BASE	(EIM_IRQ_BASE + NR_EIM_IRQS)
#define NR_GPIO_IRQS	(5 * 32)

#define NR_IRQS		(GPIO_IRQ_BASE + NR_GPIO_IRQS)

#endif /* __ASM_AVR32_ARCH_IRQ_H */
