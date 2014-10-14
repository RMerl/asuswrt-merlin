/*
 * Board setup routines for the Emerson KSI8560
 *
 * Author: Alexandr Smirnov <asmirnov@ru.mvista.com>
 *
 * Based on mpc85xx_ads.c maintained by Kumar Gala
 *
 * 2008 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 *
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/of_platform.h>

#include <asm/system.h>
#include <asm/time.h>
#include <asm/machdep.h>
#include <asm/pci-bridge.h>
#include <asm/mpic.h>
#include <mm/mmu_decl.h>
#include <asm/udbg.h>
#include <asm/prom.h>

#include <sysdev/fsl_soc.h>
#include <sysdev/fsl_pci.h>

#include <asm/cpm2.h>
#include <sysdev/cpm2_pic.h>


#define KSI8560_CPLD_HVR		0x04 /* Hardware Version Register */
#define KSI8560_CPLD_PVR		0x08 /* PLD Version Register */
#define KSI8560_CPLD_RCR1		0x30 /* Reset Command Register 1 */

#define KSI8560_CPLD_RCR1_CPUHR		0x80 /* CPU Hard Reset */

static void __iomem *cpld_base = NULL;

static void machine_restart(char *cmd)
{
	if (cpld_base)
		out_8(cpld_base + KSI8560_CPLD_RCR1, KSI8560_CPLD_RCR1_CPUHR);
	else
		printk(KERN_ERR "Can't find CPLD base, hang forever\n");

	for (;;);
}

static void cpm2_cascade(unsigned int irq, struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	int cascade_irq;

	while ((cascade_irq = cpm2_get_irq()) >= 0)
		generic_handle_irq(cascade_irq);

	chip->irq_eoi(&desc->irq_data);
}

static void __init ksi8560_pic_init(void)
{
	struct mpic *mpic;
	struct resource r;
	struct device_node *np;
#ifdef CONFIG_CPM2
	int irq;
#endif

	np = of_find_node_by_type(NULL, "open-pic");

	if (np == NULL) {
		printk(KERN_ERR "Could not find open-pic node\n");
		return;
	}

	if (of_address_to_resource(np, 0, &r)) {
		printk(KERN_ERR "Could not map mpic register space\n");
		of_node_put(np);
		return;
	}

	mpic = mpic_alloc(np, r.start,
			MPIC_PRIMARY | MPIC_WANTS_RESET | MPIC_BIG_ENDIAN,
			0, 256, " OpenPIC  ");
	BUG_ON(mpic == NULL);
	of_node_put(np);

	mpic_init(mpic);

#ifdef CONFIG_CPM2
	/* Setup CPM2 PIC */
	np = of_find_compatible_node(NULL, NULL, "fsl,cpm2-pic");
	if (np == NULL) {
		printk(KERN_ERR "PIC init: can not find fsl,cpm2-pic node\n");
		return;
	}
	irq = irq_of_parse_and_map(np, 0);

	cpm2_pic_init(np);
	of_node_put(np);
	irq_set_chained_handler(irq, cpm2_cascade);
#endif
}

#ifdef CONFIG_CPM2
/*
 * Setup I/O ports
 */
struct cpm_pin {
	int port, pin, flags;
};

static struct cpm_pin __initdata ksi8560_pins[] = {
	/* SCC1 */
	{3, 29, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{3, 30, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},
	{3, 31, CPM_PIN_INPUT | CPM_PIN_PRIMARY},

	/* SCC2 */
	{3, 26, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{3, 27, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{3, 28, CPM_PIN_INPUT | CPM_PIN_PRIMARY},

	/* FCC1 */
	{0, 14, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 15, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 16, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 17, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 18, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 19, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 20, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 21, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 26, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{0, 27, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{0, 28, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},
	{0, 29, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},
	{0, 30, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{0, 31, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{2, 23, CPM_PIN_INPUT | CPM_PIN_PRIMARY}, /* CLK9 */
	{2, 22, CPM_PIN_INPUT | CPM_PIN_PRIMARY}, /* CLK10 */

};

static void __init init_ioports(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ksi8560_pins); i++) {
		struct cpm_pin *pin = &ksi8560_pins[i];
		cpm2_set_pin(pin->port, pin->pin, pin->flags);
	}

	cpm2_clk_setup(CPM_CLK_SCC1, CPM_BRG1, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_SCC1, CPM_BRG1, CPM_CLK_TX);
	cpm2_clk_setup(CPM_CLK_SCC2, CPM_BRG2, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_SCC2, CPM_BRG2, CPM_CLK_TX);
	cpm2_clk_setup(CPM_CLK_FCC1, CPM_CLK9, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_FCC1, CPM_CLK10, CPM_CLK_TX);
}
#endif

/*
 * Setup the architecture
 */
static void __init ksi8560_setup_arch(void)
{
	struct device_node *cpld;

	cpld = of_find_compatible_node(NULL, NULL, "emerson,KSI8560-cpld");
	if (cpld)
		cpld_base = of_iomap(cpld, 0);
	else
		printk(KERN_ERR "Can't find CPLD in device tree\n");

	if (ppc_md.progress)
		ppc_md.progress("ksi8560_setup_arch()", 0);

#ifdef CONFIG_CPM2
	cpm2_reset();
	init_ioports();
#endif
}

static void ksi8560_show_cpuinfo(struct seq_file *m)
{
	uint pvid, svid, phid1;

	pvid = mfspr(SPRN_PVR);
	svid = mfspr(SPRN_SVR);

	seq_printf(m, "Vendor\t\t: Emerson Network Power\n");
	seq_printf(m, "Board\t\t: KSI8560\n");

	if (cpld_base) {
		seq_printf(m, "Hardware rev\t: %d\n",
					in_8(cpld_base + KSI8560_CPLD_HVR));
		seq_printf(m, "CPLD rev\t: %d\n",
					in_8(cpld_base + KSI8560_CPLD_PVR));
	} else
		seq_printf(m, "Unknown Hardware and CPLD revs\n");

	seq_printf(m, "PVR\t\t: 0x%x\n", pvid);
	seq_printf(m, "SVR\t\t: 0x%x\n", svid);

	/* Display cpu Pll setting */
	phid1 = mfspr(SPRN_HID1);
	seq_printf(m, "PLL setting\t: 0x%x\n", ((phid1 >> 24) & 0x3f));
}

static struct of_device_id __initdata of_bus_ids[] = {
	{ .type = "soc", },
	{ .type = "simple-bus", },
	{ .name = "cpm", },
	{ .name = "localbus", },
	{ .compatible = "gianfar", },
	{},
};

static int __init declare_of_platform_devices(void)
{
	of_platform_bus_probe(NULL, of_bus_ids, NULL);

	return 0;
}
machine_device_initcall(ksi8560, declare_of_platform_devices);

/*
 * Called very early, device-tree isn't unflattened
 */
static int __init ksi8560_probe(void)
{
	unsigned long root = of_get_flat_dt_root();

	return of_flat_dt_is_compatible(root, "emerson,KSI8560");
}

define_machine(ksi8560) {
	.name			= "KSI8560",
	.probe			= ksi8560_probe,
	.setup_arch		= ksi8560_setup_arch,
	.init_IRQ		= ksi8560_pic_init,
	.show_cpuinfo		= ksi8560_show_cpuinfo,
	.get_irq		= mpic_get_irq,
	.restart		= machine_restart,
	.calibrate_decr		= generic_calibrate_decr,
};
