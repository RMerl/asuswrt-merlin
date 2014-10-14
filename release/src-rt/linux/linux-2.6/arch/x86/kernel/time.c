/*
 *  Copyright (c) 1991,1992,1995  Linus Torvalds
 *  Copyright (c) 1994  Alan Modra
 *  Copyright (c) 1995  Markus Kuhn
 *  Copyright (c) 1996  Ingo Molnar
 *  Copyright (c) 1998  Andrea Arcangeli
 *  Copyright (c) 2002,2006  Vojtech Pavlik
 *  Copyright (c) 2003  Andi Kleen
 *
 */

#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/mca.h>

#include <asm/vsyscall.h>
#include <asm/x86_init.h>
#include <asm/i8259.h>
#include <asm/i8253.h>
#include <asm/timer.h>
#include <asm/hpet.h>
#include <asm/time.h>

#ifdef CONFIG_X86_64
volatile unsigned long __jiffies __section_jiffies = INITIAL_JIFFIES;
#endif

unsigned long profile_pc(struct pt_regs *regs)
{
	unsigned long pc = instruction_pointer(regs);

	if (!user_mode_vm(regs) && in_lock_functions(pc)) {
#ifdef CONFIG_FRAME_POINTER
		return *(unsigned long *)(regs->bp + sizeof(long));
#else
		unsigned long *sp =
			(unsigned long *)kernel_stack_pointer(regs);
		/*
		 * Return address is either directly at stack pointer
		 * or above a saved flags. Eflags has bits 22-31 zero,
		 * kernel addresses don't.
		 */
		if (sp[0] >> 22)
			return sp[0];
		if (sp[1] >> 22)
			return sp[1];
#endif
	}
	return pc;
}
EXPORT_SYMBOL(profile_pc);

/*
 * Default timer interrupt handler for PIT/HPET
 */
static irqreturn_t timer_interrupt(int irq, void *dev_id)
{
	/* Keep nmi watchdog up to date */
	inc_irq_stat(irq0_irqs);

	global_clock_event->event_handler(global_clock_event);

	/* MCA bus quirk: Acknowledge irq0 by setting bit 7 in port 0x61 */
	if (MCA_bus)
		outb_p(inb_p(0x61)| 0x80, 0x61);

	return IRQ_HANDLED;
}

static struct irqaction irq0  = {
	.handler = timer_interrupt,
	.flags = IRQF_DISABLED | IRQF_NOBALANCING | IRQF_IRQPOLL | IRQF_TIMER,
	.name = "timer"
};

void __init setup_default_timer_irq(void)
{
	setup_irq(0, &irq0);
}

/* Default timer init function */
void __init hpet_time_init(void)
{
	if (!hpet_enable())
		setup_pit_timer();
	setup_default_timer_irq();
}

static __init void x86_late_time_init(void)
{
	x86_init.timers.timer_init();
	tsc_init();
}

/*
 * Initialize TSC and delay the periodic timer init to
 * late x86_late_time_init() so ioremap works.
 */
void __init time_init(void)
{
	late_time_init = x86_late_time_init;
}
