#include <linux/init.h>
#include <asm/io.h>
#include <asm/time.h>

extern void qvga_init(void);
extern void qemu_reboot_setup(void);

#define QEMU_PORT_BASE 0xb4000000

const char *get_system_type(void)
{
	return "Qemu";
}

void __init plat_timer_setup(struct irqaction *irq)
{
	/* set the clock to 100 Hz */
	outb_p(0x34,0x43);		/* binary, mode 2, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */
	outb(LATCH >> 8 , 0x40);	/* MSB */
	setup_irq(0, irq);
}

void __init plat_mem_setup(void)
{
	set_io_port_base(QEMU_PORT_BASE);
#ifdef CONFIG_VT
	qvga_init();
#endif

	qemu_reboot_setup();
}
