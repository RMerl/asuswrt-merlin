/*
 * Northstar SoC main platform file.
 *
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/serial_8250.h>
#include <linux/proc_fs.h>
#include <linux/spi/spi.h>
#include <linux/irq.h>
#include <linux/spinlock.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/mach/map.h>
#include <asm/clkdev.h>
#include <asm/uaccess.h>
#include <mach/clkdev.h>
#include <mach/io_map.h>
#include <plat/bsp.h>
#include <plat/mpcore.h>
#include <plat/plat-bcm5301x.h>

#include <typedefs.h>
#include <sbchipc.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdefs.h>
#include <bcmdevs.h>

#ifdef	CONFIG_SMP
#include <asm/spinlock.h>
#endif

#define SOC_CHIPCOMON_A_REG_VA(offset)		(SOC_CHIPCOMON_A_BASE_VA + (offset))
#define SOC_CHIPCOMON_A_INTSTATUS_VA		SOC_CHIPCOMON_A_REG_VA(CC_INTSTATUS)
#define SOC_CHIPCOMON_A_INTMASK_VA		SOC_CHIPCOMON_A_REG_VA(CC_INTMASK)
#define SOC_CHIPCOMON_A_GPIOINPUT_VA		SOC_CHIPCOMON_A_REG_VA(CC_GPIOIN)
#define SOC_CHIPCOMON_A_GPIOINTPOLARITY_VA	SOC_CHIPCOMON_A_REG_VA(CC_GPIOPOL)
#define SOC_CHIPCOMON_A_GPIOINTMASK_VA		SOC_CHIPCOMON_A_REG_VA(CC_GPIOINTM)
#define SOC_CHIPCOMON_A_GPIOEVENT_VA		SOC_CHIPCOMON_A_REG_VA(CC_GPIOEVENT)
#define SOC_CHIPCOMON_A_GPIOEVENTINTMASK_VA	SOC_CHIPCOMON_A_REG_VA(CC_GPIOEVENTMASK)
#define SOC_CHIPCOMON_A_GPIOEVENTINTPOLARITY_VA	SOC_CHIPCOMON_A_REG_VA(CC_GPIOEVENTPOL)
#define SOC_CHIPCOMON_A_CORECAP			SOC_CHIPCOMON_A_REG_VA(CC_CAPABILITIES)
#define SOC_CHIPCOMON_A_CORECTRL		SOC_CHIPCOMON_A_REG_VA(0x08)
#define SOC_CHIPCOMON_A_CLKDIV			SOC_CHIPCOMON_A_REG_VA(CC_CLKDIV)

#define SOC_CHIPCOMON_A_INTMASK_UART		(1 << 6)
#define SOC_CHIPCOMON_A_INTMASK_GPIO		(1 << 0)

#define SOC_CHIPCOMON_A_CORECAP_UARTCLKSEL_M	0x3
#define SOC_CHIPCOMON_A_CORECAP_UARTCLKSEL_S	3

#define SOC_CHIPCOMON_A_CORECTRL_UARTCLKOVR_M	0x1
#define SOC_CHIPCOMON_A_CORECTRL_UARTCLKOVR_S	0

#define SOC_CHIPCOMON_A_CLKDIV_UARTCLKDIV_M	0xFF
#define SOC_CHIPCOMON_A_CLKDIV_UARTCLKDIV_S	0

#define SOC_APBX_IDM_IDM_IO_CONTROL_DIRECT	(SOC_IDM_BASE_VA + 0x21408)

#define SOC_APBX_IDM_IDM_IO_CONTROL_DIRECT_UARTCLKSEL_M	0x1
#define SOC_APBX_IDM_IDM_IO_CONTROL_DIRECT_UARTCLKSEL_S	17

#define PLAT_SM_CLR(val, field)			((val) & ~(field##_M << field##_S))
#define PLAT_SM_SET(val, field, field_val)	((val) | (((field_val) & field##_M) << field##_S))
#define PLAT_SM_GET(val, field)			(((val) >> field##_S) & field##_M)
#define PLAT_SM_ASSIGN(val, field, field_val)	PLAT_SM_SET(PLAT_SM_CLR(val, field), field, field_val)

static struct clk * _soc_refclk = NULL;

#ifdef CONFIG_PLAT_UART_CLOCKS

#if CONFIG_PLAT_CCA_UART0_DIVIDER <= 0xFFFF
# define CONFIG_PLAT_CCA_UART0_DIVIDER_FLAG	UPF_SPD_CUST
#else
# define CONFIG_PLAT_CCA_UART0_DIVIDER_FLAG	0
#endif

#if CONFIG_PLAT_CCA_UART1_DIVIDER <= 0xFFFF
# define CONFIG_PLAT_CCA_UART1_DIVIDER_FLAG	UPF_SPD_CUST
#else
# define CONFIG_PLAT_CCA_UART1_DIVIDER_FLAG	0
#endif

#if CONFIG_PLAT_CCB_UART0_DIVIDER <= 0xFFFF
# define CONFIG_PLAT_CCB_UART0_DIVIDER_FLAG	UPF_SPD_CUST
#else
# define CONFIG_PLAT_CCB_UART0_DIVIDER_FLAG	0
#endif

#else

#define CONFIG_PLAT_CCA_UART0_DIVIDER		0
#define CONFIG_PLAT_CCA_UART1_DIVIDER		0
#define CONFIG_PLAT_CCB_UART0_DIVIDER		0

#define CONFIG_PLAT_CCA_UART0_DIVIDER_FLAG	0
#define CONFIG_PLAT_CCA_UART1_DIVIDER_FLAG	0
#define CONFIG_PLAT_CCB_UART0_DIVIDER_FLAG	0

#endif /* PLAT_UART_CLOCKS */

/* Global SB handle */
extern si_t *bcm947xx_sih;
#define sih bcm947xx_sih

extern int _chipid;

static struct plat_serial8250_port uart_ports[] = {
	{
	.type       = PORT_16550,
	.flags      = UPF_FIXED_TYPE | UPF_SHARE_IRQ | CONFIG_PLAT_CCA_UART0_DIVIDER_FLAG,
	.regshift   = 0,
	.iotype     = UPIO_MEM,
	.mapbase    = (resource_size_t)(PLAT_UART1_PA),
	.membase    = (void __iomem *) PLAT_UART1_VA,
	.irq        = IRQ_CCA_UART,
	.custom_divisor = CONFIG_PLAT_CCA_UART0_DIVIDER,
	},
	{
	.type       = PORT_16550,
	.flags      = UPF_FIXED_TYPE | UPF_SHARE_IRQ | CONFIG_PLAT_CCA_UART1_DIVIDER_FLAG,
	.regshift   = 0,
	.iotype     = UPIO_MEM,
	.mapbase    = (resource_size_t)(PLAT_UART2_PA),
	.membase    = (void __iomem *) PLAT_UART2_VA,
	.irq        = IRQ_CCA_UART,
	.custom_divisor = CONFIG_PLAT_CCA_UART1_DIVIDER,
	},
#ifdef CONFIG_PLAT_MUX_CONSOLE_CCB
	{
	.type       = PORT_16550,
	.flags      = UPF_FIXED_TYPE | CONFIG_PLAT_CCB_UART0_DIVIDER_FLAG,
	.regshift   = 2,	/* Chipcommon B regs are 32-bit aligned */
	.iotype     = UPIO_MEM,
	.mapbase    = (resource_size_t)(PLAT_UART0_PA),
	.membase    = (void __iomem *) PLAT_UART0_VA,
	.irq        = IRQ_CCB_UART0,
	.custom_divisor = CONFIG_PLAT_CCB_UART0_DIVIDER,
	},
#endif /* CONFIG_PLAT_MUX_CONSOLE_CCB */
	{ .flags = 0, },	/* List terminatoir */
};

static struct platform_device platform_serial_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,	/* <linux/serial_8250.h> */
	.dev = {
		.platform_data = uart_ports,
	},
};

static struct platform_device platform_spi_master_device = {
	.name = "bcm5301x-spi-master",
	.id = 1, /* Bus number */
	.dev = {
		.platform_data = NULL, /* Passed to driver */
	},
};

/*
 * Map fix-mapped I/O that is needed before full MMU operation
 */
void __init soc_map_io(struct clk * refclk)
{
	struct map_desc desc[2];

	/*
	* Map Chipcommon A & B to a fixed virtual location
	* as these contain all the system UARTs, which are
	* needed for low-level debugging,
	* it have already been mapped from mach_desc earlier
	* but that is undone from page_init()
	*/

	desc[0].virtual = IO_BASE_VA;
	desc[0].pfn = __phys_to_pfn(IO_BASE_PA);
	if (BCM53573_CHIP(_chipid)) {
		desc[0].length = SZ_2M;
	} else {
		desc[0].length = SZ_16M;	/* CCA+CCB: 0x18000000-0x18ffffff */
	}

	desc[0].type = MT_DEVICE;

	iotable_init(desc, 1);

	mpcore_map_io();
	/* Save refclk for later use */
	_soc_refclk = refclk;
}

static int soc_abort_handler(unsigned long addr, unsigned int fsr,
                                      struct pt_regs *regs)
{
	/*
	 * These happen for no good reason
	 * possibly left over from CFE
	 */
	printk( KERN_WARNING 
		"External imprecise Data abort at "
		"addr=%#lx, fsr=%#x ignored.\n", 
		addr, fsr );

	/* Returning non-zero causes fault display and panic */
        return 0;
}

static void __init soc_aborts_enable(void)
{
	u32 x;

	/* Install our hook */
        hook_fault_code(16 + 6, soc_abort_handler, SIGBUS, 0,
                        "imprecise external abort");

	/* Enable external aborts - clear "A" bit in CPSR */

	/* Read CPSR */
	asm( "mrs	%0,cpsr": "=&r" (x) : : );

	x &= ~ PSR_A_BIT ;

	/* Update CPSR, affect bits 8-15 */
	asm( "msr	cpsr_x,%0; nop; nop": : "r" (x) : "cc" );

}

#ifdef CONFIG_PLAT_CCA_GPIO_IRQ

#include <asm/gpio.h>

struct soc_cca_gpio_irq_data {
	unsigned int type;
};

static void soc_cca_gpio_handle(void)
{
	uint32 mask = readl(SOC_CHIPCOMON_A_GPIOINTMASK_VA);
	uint32 polarity = readl(SOC_CHIPCOMON_A_GPIOINTPOLARITY_VA);
	uint32 input = readl(SOC_CHIPCOMON_A_GPIOINPUT_VA);
	uint32 event = readl(SOC_CHIPCOMON_A_GPIOEVENT_VA);
	uint32 eventmask = readl(SOC_CHIPCOMON_A_GPIOEVENTINTMASK_VA);
	int gpio;

	for (gpio = 0; gpio < IRQ_CCA_GPIO_N; ++gpio) {
		uint32 gpio_bit = (1 << gpio);
		uint32 level_triggered = ((input ^ polarity) & mask & gpio_bit);
		uint32 edge_triggered = (event & eventmask & gpio_bit);

		if (level_triggered || edge_triggered) {
			generic_handle_irq(IRQ_CCA_GPIO(gpio));
		}
	}
}

static void soc_cca_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	uint32 status = readl(SOC_CHIPCOMON_A_INTSTATUS_VA);

	desc->chip->mask(irq);
	desc->chip->ack(irq);

	if (status & SOC_CHIPCOMON_A_INTMASK_UART) {
		generic_handle_irq(IRQ_CCA_UART);
	}

	if (status & SOC_CHIPCOMON_A_INTMASK_GPIO) {
		soc_cca_gpio_handle();
	}

	desc->chip->unmask(irq);
}

static void soc_cca_gpio_irq_update_type(unsigned int irq)
{
	int gpio = irq_to_gpio(irq);
	struct soc_cca_gpio_irq_data *pdata;
	uint32 gpio_bit;
	unsigned int type;

	if (gpio < 0) {
		return;
	}

	pdata = get_irq_data(irq);
	type = pdata->type;
	gpio_bit = (1 << gpio);

	if (type & IRQ_TYPE_LEVEL_LOW) {
		writel(readl(SOC_CHIPCOMON_A_GPIOINTPOLARITY_VA) | gpio_bit,
			SOC_CHIPCOMON_A_GPIOINTPOLARITY_VA);
	} else if (type & IRQ_TYPE_LEVEL_HIGH) {
		writel(readl(SOC_CHIPCOMON_A_GPIOINTPOLARITY_VA) & ~gpio_bit,
			SOC_CHIPCOMON_A_GPIOINTPOLARITY_VA);
	} else if (type & IRQ_TYPE_EDGE_FALLING) {
		writel(readl(SOC_CHIPCOMON_A_GPIOEVENTINTPOLARITY_VA) | gpio_bit,
			SOC_CHIPCOMON_A_GPIOEVENTINTPOLARITY_VA);
	} else if (type & IRQ_TYPE_EDGE_RISING) {
		writel(readl(SOC_CHIPCOMON_A_GPIOEVENTINTPOLARITY_VA) & ~gpio_bit,
			SOC_CHIPCOMON_A_GPIOEVENTINTPOLARITY_VA);
	}
}

static void soc_cca_gpio_irq_ack(unsigned int irq)
{
	int gpio = irq_to_gpio(irq);

	if (gpio < 0) {
		return;
	}

	writel((1 << gpio), SOC_CHIPCOMON_A_GPIOEVENT_VA);
}

static void soc_cca_gpio_irq_maskunmask(unsigned int irq, bool mask)
{
	int gpio = irq_to_gpio(irq);
	struct soc_cca_gpio_irq_data *data;
	uint32 mask_addr;
	uint32 gpio_bit;
	uint32 val;

	if (gpio < 0) {
		return;
	}

	data = get_irq_data(irq);
	gpio_bit = (1 << gpio);

	if (data->type & IRQ_TYPE_EDGE_BOTH) {
		mask_addr = SOC_CHIPCOMON_A_GPIOEVENTINTMASK_VA;
	} else {
		mask_addr = SOC_CHIPCOMON_A_GPIOINTMASK_VA;
	}

	val = readl(mask_addr);
	if (mask) {
		val &= ~gpio_bit;
	} else {
		val |= gpio_bit;
	}
	writel(val, mask_addr);
}

static void soc_cca_gpio_irq_mask(unsigned int irq)
{
	soc_cca_gpio_irq_maskunmask(irq, true);
}

static void soc_cca_gpio_irq_unmask(unsigned int irq)
{
	soc_cca_gpio_irq_maskunmask(irq, false);
}

static void soc_cca_gpio_irq_enable(unsigned int irq)
{
	soc_cca_gpio_irq_update_type(irq);
	soc_cca_gpio_irq_ack(irq);
	soc_cca_gpio_irq_unmask(irq);
}

static void soc_cca_gpio_irq_disable(unsigned int irq)
{
	soc_cca_gpio_irq_mask(irq);
}

static int soc_cca_gpio_irq_set_type(unsigned int irq, unsigned int type)
{
	struct soc_cca_gpio_irq_data *pdata;

	if (irq_to_gpio(irq) < 0) {
		return -EINVAL;
	}

	if ((type & IRQ_TYPE_SENSE_MASK) == 0) {
		return -EINVAL;
	}

	pdata = get_irq_data(irq);
	pdata->type = type & IRQ_TYPE_SENSE_MASK;

	return 0;
}

static struct irq_chip cca_gpio_irq_chip = {
	.name		= "CCA_GPIO",
	.ack		= soc_cca_gpio_irq_ack,
	.mask		= soc_cca_gpio_irq_mask,
	.unmask		= soc_cca_gpio_irq_unmask,
	.enable		= soc_cca_gpio_irq_enable,
	.disable	= soc_cca_gpio_irq_disable,
	.set_type	= soc_cca_gpio_irq_set_type,
};

static void __init soc_cca_irq_enable(void)
{
	static struct soc_cca_gpio_irq_data irq_data[IRQ_CCA_GPIO_N];
	int irq;

	for (irq = IRQ_CCA_FIRST; irq <= IRQ_CCA_LAST; ++irq) {
		if (irq == IRQ_CCA_UART) {
			set_irq_chip(irq, NULL);
		} else {
			set_irq_chip(irq, &cca_gpio_irq_chip);
			set_irq_data(irq, &irq_data[irq_to_gpio(irq)]);
		}
		set_irq_handler(irq, handle_level_irq);
		set_irq_flags(irq, IRQF_VALID);
	}

	set_irq_chained_handler(IRQ_CCA, soc_cca_irq_handler);

	printk(KERN_INFO"%d IRQ chained: UART, GPIOs\n", IRQ_CCA);
}

#else

static void __init soc_cca_irq_enable(void)
{
}

#endif /* CONFIG_PLAT_CCA_GPIO_IRQ */

/*
 * This SoC relies on MPCORE GIC interrupt controller
 */
void __init soc_init_irq( void )
{
	mpcore_init_gic();
	soc_aborts_enable();
	soc_cca_irq_enable();
}

/*
 * Do timer clock (using ILP clock) calibration for BCM53573
 */
static u32 __init soc_timerclk_calibration(void)
{
	void * __iomem reg_base = (void *)SOC_PMU_BASE_VA;
	u32 val1, val2, val_sum = 0, val_num = 0, loop_num = 0;
	u32 timer_clk;

	/* Enable XtalCntrEanble bit, bit[31] of PMU_XtalFreqRatio register */
	writel(0x80000000, reg_base + PMU_XTALFREQ_RATIO_OFF);
	val1 = readl(reg_base + PMU_XTALFREQ_RATIO_OFF) & 0x1fff;

	/*
	 * Get some valid values of the field AlpPer4Ilp of the above register, and
	 * average it as timer clock.
	 */
	while (val_num < 20) {
		/* Check next valid value */
		val2 = readl(reg_base + PMU_XTALFREQ_RATIO_OFF) & 0x1fff;
		if (val1 == val2) {
			if (++loop_num > 5000) {
				val_sum += val2;
				val_num++;
				break;
			}
			continue;
		}
		val1 = val2;
		val_sum += val1;
		val_num++;
		loop_num = 0;
	}

	/* Disable XtalCntrEanble bit, bit[31] of PMU_XtalFreqRatio register */
	writel(0x0, reg_base + PMU_XTALFREQ_RATIO_OFF);

	val_sum /= val_num;
	timer_clk = (si_alp_clock(sih) * 4) / val_sum;

	return timer_clk;
}

/*
 * Initialize SoC timers
 */
void __init soc_init_timer(void)
{
	unsigned long periphclk_freq;
	struct clk * clk;
	if (BCM53573_CHIP(sih->chip)) {
		soc_pmu_clk_init(_soc_refclk);
		periphclk_freq = (unsigned long)soc_timerclk_calibration();
	} else {
		/* Clocks need to be setup early */
		soc_dmu_init(_soc_refclk);
		soc_cru_init(_soc_refclk);

		/* get mpcore PERIPHCLK from clock modules */
		clk = clk_get_sys(NULL, "periph_clk");
		BUG_ON(IS_ERR_OR_NULL(clk));
		periphclk_freq = clk_get_rate(clk);
	}

	BUG_ON(!periphclk_freq);

	/* Fire up the global MPCORE timer */
	mpcore_init_timer(periphclk_freq);

}

static void __init soc_config_cca_uart_clock(void)
{
	if (BCM53573_CHIP(sih->chip)) {
		return;
	}

#if defined(CONFIG_PLAT_CCA_UART_CLK_DEFAULT)
	/* Do nothing. Use what already set. */
#elif defined(CONFIG_PLAT_CCA_UART_CLK_INTERNAL_OVERRIDE)
	writel(PLAT_SM_SET(readl(SOC_CHIPCOMON_A_CORECTRL), SOC_CHIPCOMON_A_CORECTRL_UARTCLKOVR, 1),
		SOC_CHIPCOMON_A_CORECTRL);
#elif defined(CONFIG_PLAT_CCA_UART_CLK_INTERNAL_DIVIDER)
	writel(PLAT_SM_ASSIGN(readl(SOC_CHIPCOMON_A_CLKDIV), SOC_CHIPCOMON_A_CLKDIV_UARTCLKDIV, CONFIG_PLAT_CCA_UART_CLK_INTERNAL_DIVIDER_VAL),
		SOC_CHIPCOMON_A_CLKDIV);
	writel(PLAT_SM_CLR(readl(SOC_CHIPCOMON_A_CORECTRL), SOC_CHIPCOMON_A_CORECTRL_UARTCLKOVR),
		SOC_CHIPCOMON_A_CORECTRL);
	writel(PLAT_SM_SET(readl(SOC_APBX_IDM_IDM_IO_CONTROL_DIRECT), SOC_APBX_IDM_IDM_IO_CONTROL_DIRECT_UARTCLKSEL, 1),
		SOC_APBX_IDM_IDM_IO_CONTROL_DIRECT);
#elif defined(CONFIG_PLAT_CCA_UART_CLK_EXTERNAL)
	writel(PLAT_SM_CLR(readl(SOC_CHIPCOMON_A_CORECTRL), SOC_CHIPCOMON_A_CORECTRL_UARTCLKOVR),
		SOC_CHIPCOMON_A_CORECTRL);
	writel(PLAT_SM_CLR(readl(SOC_APBX_IDM_IDM_IO_CONTROL_DIRECT), SOC_APBX_IDM_IDM_IO_CONTROL_DIRECT_UARTCLKSEL),
		SOC_APBX_IDM_IDM_IO_CONTROL_DIRECT);
#endif
}

static void __init soc_get_uart_clk_rate(u32 *clk_rate_cca, u32 *clk_rate_ccb)
{
	struct clk *clk_ext = _soc_refclk;
	struct clk *clk_int = NULL;
	u32 clk_rate_int;
	u32 UARTClkSel = PLAT_SM_GET(readl(SOC_CHIPCOMON_A_CORECAP), SOC_CHIPCOMON_A_CORECAP_UARTCLKSEL);
	u32 UARTClkOvr = PLAT_SM_GET(readl(SOC_CHIPCOMON_A_CORECTRL), SOC_CHIPCOMON_A_CORECTRL_UARTCLKOVR);
	u32 UARTClkDiv = PLAT_SM_GET(readl(SOC_CHIPCOMON_A_CLKDIV), SOC_CHIPCOMON_A_CLKDIV_UARTCLKDIV);

	BUG_ON(!clk_ext);
	BUG_ON(UARTClkSel > 1);
	if (BCM53573_CHIP(sih->chip)) {
		clk_rate_int = si_alp_clock(sih);
	} else {
		clk_int = clk_get_sys(NULL, "iproc_slow_clk");
		BUG_ON(!clk_int);
		clk_rate_int = clk_get_rate(clk_int);
	}

	if (UARTClkDiv == 0) {
		UARTClkDiv = 0x100;
	}

	if (UARTClkOvr) {
		/* internal reference clock source */
		*clk_rate_cca = clk_rate_int;
	} else if (UARTClkSel) {
		/* internal reference clock source with applied divider */
		*clk_rate_cca = clk_rate_int / UARTClkDiv;
	} else {
		/* external reference clock source */
		*clk_rate_cca = clk_get_rate(clk_ext);
	}

	printk(KERN_INFO "CCA UART Clock Config: Sel=%d Ovr=%d Div=%d\n",
		UARTClkSel, UARTClkOvr, UARTClkDiv);

	if (BCM53573_CHIP(sih->chip)) {
		/* Don't care about CCB for BCM53573 */
		*clk_rate_ccb = 0;

		printk(KERN_INFO "CCA UART Clock rate %uHz\n", *clk_rate_cca);
	} else {
		*clk_rate_ccb = clk_rate_int;

		printk(KERN_INFO "CCA UART Clock rate %uHz CCB UART Clock rate %uHz\n",
		*clk_rate_cca, *clk_rate_ccb);
	}
}

static void __init soc_fixup_uart_ports(void)
{
	u32 clk_rate_cca, clk_rate_ccb;
	unsigned i;

	soc_config_cca_uart_clock();

	soc_get_uart_clk_rate(&clk_rate_cca, &clk_rate_ccb);

	for (i = 0; i < ARRAY_SIZE(uart_ports); i++) {
		/* Last empty entry */
		if (uart_ports[i].flags == 0)
			break;

		if (uart_ports[i].irq == 0)
			uart_ports[i].flags |= UPF_AUTO_IRQ;

		/* UART clock rate */
		uart_ports[i].uartclk =
		(uart_ports[i].irq == IRQ_CCA_UART || uart_ports[i].irq == IRQ_CC_UART) ?
		clk_rate_cca : clk_rate_ccb;
	}
}

/*
 * Install all other SoC device drivers
 * that are not automatically discoverable.
 */
void __init soc_add_devices(void)
{
	u32 i;

	if (BCM53573_CHIP(sih->chip)) {
		/* BCM53573 only has one uart port */
		for (i = 1; i < ARRAY_SIZE(uart_ports); i++) {
			memset(&uart_ports[i], 0x0, sizeof(uart_ports[i]));
		}
		/* BCM53573 uses different interrupt ID number */
		uart_ports[0].irq = IRQ_CC_UART;
	}

	/* Fixup UART port structure */
	soc_fixup_uart_ports();

	/* Install SoC devices in the system: uarts */
	if (platform_device_register(&platform_serial_device) != 0) {
		printk(KERN_WARNING "Fail to register serial device\n");
	}

	/* Install SoC devices in the system: SPI master */
	if (platform_device_register(&platform_spi_master_device) != 0) {
		printk(KERN_WARNING "Fail to register SPI master device\n");
	}

	/* Enable UART interrupt in ChipcommonA */
	i = readl(SOC_CHIPCOMON_A_BASE_VA + 0x24);
	i |= SOC_CHIPCOMON_A_INTMASK_UART;
	writel(i, SOC_CHIPCOMON_A_BASE_VA + 0x24);

#ifdef CONFIG_PLAT_CCA_GPIO_IRQ
	/* Enable GPIO interrupts in ChipcommonA */
	i = readl(SOC_CHIPCOMON_A_INTMASK_VA);
	i |= SOC_CHIPCOMON_A_INTMASK_GPIO;
	writel(i, SOC_CHIPCOMON_A_INTMASK_VA);
#endif /* CONFIG_PLAT_CCA_GPIO_IRQ */
}

void plat_wake_secondary_cpu( unsigned cpu, void (* _sec_entry_va)(void) )
{
	void __iomem * rombase = NULL;
	phys_addr_t lut_pa;
	u32 offset, mask;
	u32 val ;

	mask = ( 1UL << PAGE_SHIFT) -1 ;

	lut_pa = SOC_ROM_BASE_PA & ~mask ;
	offset = SOC_ROM_BASE_PA &  mask ;
	offset += SOC_ROM_LUT_OFF;
	
	rombase = ioremap( lut_pa, PAGE_SIZE );
	if( rombase == NULL )
		return;
	val = virt_to_phys( _sec_entry_va );

	writel( val, rombase + offset );

	smp_wmb();	/* probably not needed - io regs are not cached */

#ifdef	CONFIG_SMP
	dsb_sev();	/* Exit WFI */
#endif
	mb();

	iounmap( rombase );
}

#ifdef CONFIG_CACHE_L310
/*
 * SoC initialization that need to be done early,
 * e.g. L2 cache, clock, I/O pin mux, power management
 */
static int  __init bcm5301_pl310_init(void)
{
	void __iomem *l2cache_base;
	u32 auxctl_val, auxctl_msk;
	extern void __init l310_init(void __iomem *, u32, u32, int);
	/* Return immediately since no L2 cache for BCM53573 */
	if (BCM53573_CHIP(sih->chip)) {
		outer_cache.inv_range = NULL;
		outer_cache.clean_range = NULL;
		outer_cache.flush_range = NULL;
#ifdef CONFIG_OUTER_CACHE_SYNC
		outer_cache.sync = NULL;
#endif
		return 0;
	}
	/* Default AUXCTL modifiers */
	auxctl_val = 0UL;
	auxctl_msk = ~0UL;

	/* Customize AUXCTL */
	auxctl_val |= 0 << 0 ;	/* Full line of zero Enable - requires A9 cfg */
	auxctl_val |= 1 << 25;	/* Cache replacement policy - round robin */
	auxctl_val |= 1 << 29;	/* Instruction prefetch enable */
	auxctl_val |= 1 << 28;	/* Data prefetch enable */
	auxctl_val |= 1 << 30;	/* Early BRESP enable */

	if (ACP_WAR_ENAB() || arch_is_coherent())
		auxctl_val |= 1 << 11; /* Store buffer device limitation enable */

	l2cache_base = ioremap(L2CC_BASE_PA, SZ_4K);

	/* Configure using default aux control value */
	if (l2cache_base != NULL)
		l310_init(l2cache_base, auxctl_val, auxctl_msk, 32);

	return 0;
}
early_initcall( bcm5301_pl310_init );
#endif
