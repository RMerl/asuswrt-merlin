/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Code to handle x86 style IRQs plus some generic interrupt stuff.
 *
 * Copyright (C) 1992 Linus Torvalds
 * Copyright (C) 1994 - 2000 Ralf Baechle
 */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/kgdb.h>
#include <linux/ftrace.h>

#include <asm/atomic.h>
#include <asm/system.h>
#include <asm/uaccess.h>

#ifdef CONFIG_KGDB
int kgdb_early_setup;
#endif

static unsigned long irq_map[NR_IRQS / BITS_PER_LONG];

int allocate_irqno(void)
{
	int irq;

again:
	irq = find_first_zero_bit(irq_map, NR_IRQS);

	if (irq >= NR_IRQS)
		return -ENOSPC;

	if (test_and_set_bit(irq, irq_map))
		goto again;

	return irq;
}

/*
 * Allocate the 16 legacy interrupts for i8259 devices.  This happens early
 * in the kernel initialization so treating allocation failure as BUG() is
 * ok.
 */
void __init alloc_legacy_irqno(void)
{
	int i;

	for (i = 0; i <= 16; i++)
		BUG_ON(test_and_set_bit(i, irq_map));
}

void free_irqno(unsigned int irq)
{
	smp_mb__before_clear_bit();
	clear_bit(irq, irq_map);
	smp_mb__after_clear_bit();
}

/*
 * 'what should we do if we get a hw irq event on an illegal vector'.
 * each architecture has to answer this themselves.
 */
void ack_bad_irq(unsigned int irq)
{
	smtc_im_ack_irq(irq);
	printk("unexpected IRQ # %d\n", irq);
}

atomic_t irq_err_count;

int arch_show_interrupts(struct seq_file *p, int prec)
{
	seq_printf(p, "%*s: %10u\n", prec, "ERR", atomic_read(&irq_err_count));
	return 0;
}

asmlinkage void spurious_interrupt(void)
{
	atomic_inc(&irq_err_count);
}

void __init init_IRQ(void)
{
	int i;

#ifdef CONFIG_KGDB
	if (kgdb_early_setup)
		return;
#endif

	for (i = 0; i < NR_IRQS; i++)
		irq_set_noprobe(i);

	arch_init_irq();

#ifdef CONFIG_KGDB
	if (!kgdb_early_setup)
		kgdb_early_setup = 1;
#endif
}

#ifdef DEBUG_STACKOVERFLOW
static inline void check_stack_overflow(void)
{
	unsigned long sp;

	__asm__ __volatile__("move %0, $sp" : "=r" (sp));
	sp &= THREAD_MASK;

	/*
	 * Check for stack overflow: is there less than STACK_WARN free?
	 * STACK_WARN is defined as 1/8 of THREAD_SIZE by default.
	 */
	if (unlikely(sp < (sizeof(struct thread_info) + STACK_WARN))) {
		printk("do_IRQ: stack overflow: %ld\n",
		       sp - sizeof(struct thread_info));
		dump_stack();
	}
}
#else
static inline void check_stack_overflow(void) {}
#endif


/*
 * do_IRQ handles all normal device IRQ's (the special
 * SMP cross-CPU interrupts have their own specific
 * handlers).
 */
void __irq_entry do_IRQ(unsigned int irq)
{
	irq_enter();
	check_stack_overflow();
	if (!smtc_handle_on_other_cpu(irq))
		generic_handle_irq(irq);
	irq_exit();
}

#ifdef CONFIG_MIPS_MT_SMTC_IRQAFF
/*
 * To avoid inefficient and in some cases pathological re-checking of
 * IRQ affinity, we have this variant that skips the affinity check.
 */

void __irq_entry do_IRQ_no_affinity(unsigned int irq)
{
	irq_enter();
	smtc_im_backstop(irq);
	generic_handle_irq(irq);
	irq_exit();
}

#endif /* CONFIG_MIPS_MT_SMTC_IRQAFF */
