/*
 * Baboon Custom IC Management
 *
 * The Baboon custom IC controls the IDE, PCMCIA and media bay on the
 * PowerBook 190. It multiplexes multiple interrupt sources onto the
 * Nubus slot $C interrupt.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/traps.h>
#include <asm/bootinfo.h>
#include <asm/macintosh.h>
#include <asm/macints.h>
#include <asm/mac_baboon.h>

/* #define DEBUG_IRQS */

extern void mac_enable_irq(unsigned int);
extern void mac_disable_irq(unsigned int);

int baboon_present;
static volatile struct baboon *baboon;
static unsigned char baboon_disabled;


/*
 * Baboon initialization.
 */

void __init baboon_init(void)
{
	if (macintosh_config->ident != MAC_MODEL_PB190) {
		baboon = NULL;
		baboon_present = 0;
		return;
	}

	baboon = (struct baboon *) BABOON_BASE;
	baboon_present = 1;

	printk("Baboon detected at %p\n", baboon);
}

/*
 * Baboon interrupt handler. This works a lot like a VIA.
 */

static irqreturn_t baboon_irq(int irq, void *dev_id)
{
	int irq_bit, irq_num;
	unsigned char events;

#ifdef DEBUG_IRQS
	printk("baboon_irq: mb_control %02X mb_ifr %02X mb_status %02X\n",
		(uint) baboon->mb_control, (uint) baboon->mb_ifr,
		(uint) baboon->mb_status);
#endif

	if (!(events = baboon->mb_ifr & 0x07))
		return IRQ_NONE;

	irq_num = IRQ_BABOON_0;
	irq_bit = 1;
	do {
	        if (events & irq_bit) {
			baboon->mb_ifr &= ~irq_bit;
			m68k_handle_int(irq_num);
		}
		irq_bit <<= 1;
		irq_num++;
	} while(events >= irq_bit);
	return IRQ_HANDLED;
}

/*
 * Register the Baboon interrupt dispatcher on nubus slot $C.
 */

void __init baboon_register_interrupts(void)
{
	baboon_disabled = 0;
	if (request_irq(IRQ_NUBUS_C, baboon_irq, 0, "baboon", (void *)baboon))
		pr_err("Couldn't register baboon interrupt\n");
}

/*
 * The means for masking individual baboon interrupts remains a mystery, so
 * enable the umbrella interrupt only when no baboon interrupt is disabled.
 */

void baboon_irq_enable(int irq)
{
	int irq_idx = IRQ_IDX(irq);

#ifdef DEBUG_IRQUSE
	printk("baboon_irq_enable(%d)\n", irq);
#endif

	baboon_disabled &= ~(1 << irq_idx);
	if (!baboon_disabled)
		mac_enable_irq(IRQ_NUBUS_C);
}

void baboon_irq_disable(int irq)
{
	int irq_idx = IRQ_IDX(irq);

#ifdef DEBUG_IRQUSE
	printk("baboon_irq_disable(%d)\n", irq);
#endif

	baboon_disabled |= 1 << irq_idx;
	if (baboon_disabled)
		mac_disable_irq(IRQ_NUBUS_C);
}

void baboon_irq_clear(int irq)
{
	int irq_idx = IRQ_IDX(irq);

	baboon->mb_ifr &= ~(1 << irq_idx);
}

int baboon_irq_pending(int irq)
{
	int irq_idx = IRQ_IDX(irq);

	return baboon->mb_ifr & (1 << irq_idx);
}
