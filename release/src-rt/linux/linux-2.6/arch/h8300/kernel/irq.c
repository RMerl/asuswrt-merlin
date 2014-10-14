/*
 * linux/arch/h8300/kernel/irq.c
 *
 * Copyright 2007 Yoshinori Sato <ysato@users.sourceforge.jp>
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/random.h>
#include <linux/bootmem.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#include <asm/system.h>
#include <asm/traps.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <asm/errno.h>

/*#define DEBUG*/

extern unsigned long *interrupt_redirect_table;
extern const int h8300_saved_vectors[];
extern const h8300_vector h8300_trap_table[];
int h8300_enable_irq_pin(unsigned int irq);
void h8300_disable_irq_pin(unsigned int irq);

#define CPU_VECTOR ((unsigned long *)0x000000)
#define ADDR_MASK (0xffffff)

static inline int is_ext_irq(unsigned int irq)
{
	return (irq >= EXT_IRQ0 && irq <= (EXT_IRQ0 + EXT_IRQS));
}

static void h8300_enable_irq(struct irq_data *data)
{
	if (is_ext_irq(data->irq))
		IER_REGS |= 1 << (data->irq - EXT_IRQ0);
}

static void h8300_disable_irq(struct irq_data *data)
{
	if (is_ext_irq(data->irq))
		IER_REGS &= ~(1 << (data->irq - EXT_IRQ0));
}

static unsigned int h8300_startup_irq(struct irq_data *data)
{
	if (is_ext_irq(data->irq))
		return h8300_enable_irq_pin(data->irq);
	else
		return 0;
}

static void h8300_shutdown_irq(struct irq_data *data)
{
	if (is_ext_irq(data->irq))
		h8300_disable_irq_pin(data->irq);
}

/*
 * h8300 interrupt controller implementation
 */
struct irq_chip h8300irq_chip = {
	.name		= "H8300-INTC",
	.irq_startup	= h8300_startup_irq,
	.irq_shutdown	= h8300_shutdown_irq,
	.irq_enable	= h8300_enable_irq,
	.irq_disable	= h8300_disable_irq,
};

#if defined(CONFIG_RAMKERNEL)
static unsigned long __init *get_vector_address(void)
{
	unsigned long *rom_vector = CPU_VECTOR;
	unsigned long base,tmp;
	int vec_no;

	base = rom_vector[EXT_IRQ0] & ADDR_MASK;

	/* check romvector format */
	for (vec_no = EXT_IRQ1; vec_no <= EXT_IRQ0+EXT_IRQS; vec_no++) {
		if ((base+(vec_no - EXT_IRQ0)*4) != (rom_vector[vec_no] & ADDR_MASK))
			return NULL;
	}

	/* ramvector base address */
	base -= EXT_IRQ0*4;

	/* writerble check */
	tmp = ~(*(volatile unsigned long *)base);
	(*(volatile unsigned long *)base) = tmp;
	if ((*(volatile unsigned long *)base) != tmp)
		return NULL;
	return (unsigned long *)base;
}

static void __init setup_vector(void)
{
	int i;
	unsigned long *ramvec,*ramvec_p;
	const h8300_vector *trap_entry;
	const int *saved_vector;

	ramvec = get_vector_address();
	if (ramvec == NULL)
		panic("interrupt vector serup failed.");
	else
		printk(KERN_INFO "virtual vector at 0x%08lx\n",(unsigned long)ramvec);

	/* create redirect table */
	ramvec_p = ramvec;
	trap_entry = h8300_trap_table;
	saved_vector = h8300_saved_vectors;
	for ( i = 0; i < NR_IRQS; i++) {
		if (i == *saved_vector) {
			ramvec_p++;
			saved_vector++;
		} else {
			if ( i < NR_TRAPS ) {
				if (*trap_entry)
					*ramvec_p = VECTOR(*trap_entry);
				ramvec_p++;
				trap_entry++;
			} else
				*ramvec_p++ = REDIRECT(interrupt_entry);
		}
	}
	interrupt_redirect_table = ramvec;
#ifdef DEBUG
	ramvec_p = ramvec;
	for (i = 0; i < NR_IRQS; i++) {
		if ((i % 8) == 0)
			printk(KERN_DEBUG "\n%p: ",ramvec_p);
		printk(KERN_DEBUG "%p ",*ramvec_p);
		ramvec_p++;
	}
	printk(KERN_DEBUG "\n");
#endif
}
#else
#define setup_vector() do { } while(0)
#endif

void __init init_IRQ(void)
{
	int c;

	setup_vector();

	for (c = 0; c < NR_IRQS; c++)
		irq_set_chip_and_handler(c, &h8300irq_chip, handle_simple_irq);
}

asmlinkage void do_IRQ(int irq)
{
	irq_enter();
	generic_handle_irq(irq);
	irq_exit();
}
