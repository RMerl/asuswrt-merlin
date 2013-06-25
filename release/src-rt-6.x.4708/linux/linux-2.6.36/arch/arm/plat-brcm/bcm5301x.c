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
#include <asm/hardware/cache-l2x0.h>
#include <asm/mach/map.h>
#include <asm/clkdev.h>
#include <asm/uaccess.h>
#include <mach/clkdev.h>
#include <mach/io_map.h>
#include <plat/bsp.h>
#include <plat/mpcore.h>
#include <plat/plat-bcm5301x.h>

#ifdef	CONFIG_SMP
#include <asm/spinlock.h>
#endif

static struct clk * _soc_refclk = NULL;

static struct plat_serial8250_port uart_ports[] = {
	{
	.type       = PORT_16550,
	.flags      = UPF_FIXED_TYPE | UPF_SHARE_IRQ,
	.regshift   = 0,
	.iotype     = UPIO_MEM,	
	.mapbase    = (resource_size_t)(PLAT_UART1_PA),
	.membase    = (void __iomem *) PLAT_UART1_VA,
	.irq        = IRQ_CCA,
	},
	{
	.type       = PORT_16550,
	.flags      = UPF_FIXED_TYPE | UPF_SHARE_IRQ,
	.regshift   = 0,
	.iotype     = UPIO_MEM,	
	.mapbase    = (resource_size_t)(PLAT_UART2_PA),
	.membase    = (void __iomem *) PLAT_UART2_VA,
	.irq        = IRQ_CCA,
	},
	{ .flags = 0, },	/* List terminatoir */
};

static struct platform_device platform_serial_devices = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,	/* <linux/serial_8250.h> */
	.dev = {
		.platform_data = uart_ports,
	},
};

/*
 * Map fix-mapped I/O that is needed before full MMU operation
 */
void __init soc_map_io( struct clk * refclk )
{
	struct map_desc desc[2] ;

	/*
	* Map Chipcommon A & B to a fixed virtual location
	* as these contain all the system UARTs, which are
	* needed for low-level debugging,
	* it have already been mapped from mach_desc earlier
	* but that is undone from page_init()
	*/

	desc[0].virtual = IO_BASE_VA;
	desc[0].pfn = __phys_to_pfn( IO_BASE_PA );
	desc[0].length = SZ_16M ;	/* CCA+CCB: 0x18000000-0x18ffffff */
	desc[0].type = MT_DEVICE ;

	iotable_init( desc, 1);

	mpcore_map_io( );
	/* Save refclk for later use */
	_soc_refclk = refclk ;
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

static void soc_aborts_enable(void)
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

/*
 * This SoC relies on MPCORE GIC interrupt controller
 */
void __init soc_init_irq( void )
{
	mpcore_init_gic();
	soc_aborts_enable();

}

/*
 * Initialize SoC timers
 */
void __init soc_init_timer( void )
{
	unsigned long periphclk_freq;
	struct clk * clk ;

	/* Clocks need to be setup early */
	soc_dmu_init( _soc_refclk );
	soc_cru_init( _soc_refclk );

	/* get mpcore PERIPHCLK from clock modules */
	clk = clk_get_sys( NULL, "periph_clk");
	BUG_ON( IS_ERR_OR_NULL (clk) );
	periphclk_freq = clk_get_rate( clk );
	BUG_ON( !periphclk_freq );

	/* Fire up the global MPCORE timer */
	mpcore_init_timer( periphclk_freq );

}


/*
 * Install all other SoC device drivers
 * that are not automatically discoverable.
 */
void __init soc_add_devices( void )
{
	u32 i, clk_rate = 0;
	u8 UARTClkSel, UARTClkOvr;
	u16 UARTClkDiv ;
	struct clk * clk = NULL ;

	i = readl( SOC_CHIPCOMON_A_BASE_VA + 0x04 );	
	/* UARTClkSel ChipcommonA_CoreCapabilities bit 4..3 */
	UARTClkSel = ( i >> 3 ) & 0x3 ;
	/* UARTClkOvr ChipcommonA_CoreCtrl bit 0 */
	UARTClkOvr = 1 &  readl( SOC_CHIPCOMON_A_BASE_VA + 0x08 );
	/* UARTClkDiv: ChipcommonA_ClkDiv bits 0..7 */
	UARTClkDiv =  0xff & readl( SOC_CHIPCOMON_A_BASE_VA + 0xa4 );
	if( UARTClkDiv == 0 ) UARTClkDiv = 0x100 ;

	if( UARTClkSel == 0 )
		{
		/* UARTClkSel = 0 -> external reference clock source */
		clk = _soc_refclk ;
		BUG_ON( !clk );
		clk_rate = clk_get_rate(clk);
		}
	else if( UARTClkSel == 1 )
		{
		/* UARTClkSel = 1 -> Internal clock optionally divided */
		clk = clk_get_sys( NULL, "iproc_slow_clk");
		BUG_ON( !clk );
		clk_rate = clk_get_rate(clk) ;
		if( ! UARTClkOvr )
			clk_rate /= UARTClkDiv;
		}

	printk( KERN_INFO "CCA UART Clock Config: Sel=%d Ovr=%d Div=%d\n",
		UARTClkSel, UARTClkOvr, UARTClkDiv );
	printk( KERN_INFO "CCA UART Clock rate %uHz\n", clk_rate );

	/* fixup UART port structure */
	for(i = 0; i < ARRAY_SIZE(uart_ports); i++ )
		{
		if( uart_ports[i].flags == 0 )
			break;
		if( uart_ports[i].irq == 0 )
			uart_ports[i].flags |= UPF_AUTO_IRQ;

		/* UART input clock source and rate */
		uart_ports[i].uartclk = clk_rate ;
		}

	/* Install SoC devices in the system: uarts */
	platform_device_register( & platform_serial_devices );

	/* Enable UART interrupt in ChipcommonA */
	i = readl( SOC_CHIPCOMON_A_BASE_VA + 0x24 );
	i |= 1 << 6;
	writel( i, SOC_CHIPCOMON_A_BASE_VA + 0x24 );

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
static int  __init bcm5301_pl310_init( void )
{
	void __iomem *l2cache_base;
	u32 auxctl_val, auxctl_msk ;
	extern void __init l310_init( void __iomem *, u32, u32, int );

	/* Default AUXCTL modifiers */
	auxctl_val = 0UL;
	auxctl_msk = ~0UL ;

	/* Customize AUXCTL */
	auxctl_val |= 0 << 0 ;	/* Full line of zero Enable - requires A9 cfg */
	auxctl_val |= 1 << 25;	/* Cache replacement policy - round robin */
	auxctl_val |= 1 << 29;	/* Instruction prefetch enable */
	auxctl_val |= 1 << 28;	/* Data prefetch enable */
	auxctl_val |= 1 << 30;	/* Early BRESP enable */
	
	l2cache_base = ioremap( L2CC_BASE_PA, SZ_4K );

	/* Configure using default aux control value */
	if( l2cache_base != NULL )
		l310_init( l2cache_base, auxctl_val, auxctl_msk, 32 );

	return 0;
}
early_initcall( bcm5301_pl310_init );
#endif
