/*
  * OMAP4XXX L3 Interconnect error handling driver
  *
  * Copyright (C) 2011 Texas Corporation
  *	Santosh Shilimkar <santosh.shilimkar@ti.com>
  *	Sricharan <r.sricharan@ti.com>
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  * USA
  */
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "omap_l3_noc.h"

/*
 * Interrupt Handler for L3 error detection.
 *	1) Identify the L3 clockdomain partition to which the error belongs to.
 *	2) Identify the slave where the error information is logged
 *	3) Print the logged information.
 *	4) Add dump stack to provide kernel trace.
 *
 * Two Types of errors :
 *	1) Custom errors in L3 :
 *		Target like DMM/FW/EMIF generates SRESP=ERR error
 *	2) Standard L3 error:
 *		- Unsupported CMD.
 *			L3 tries to access target while it is idle
 *		- OCP disconnect.
 *		- Address hole error:
 *			If DSS/ISS/FDIF/USBHOSTFS access a target where they
 *			do not have connectivity, the error is logged in
 *			their default target which is DMM2.
 *
 *	On High Secure devices, firewall errors are possible and those
 *	can be trapped as well. But the trapping is implemented as part
 *	secure software and hence need not be implemented here.
 */
static irqreturn_t l3_interrupt_handler(int irq, void *_l3)
{

	struct omap4_l3		*l3 = _l3;
	int inttype, i, j;
	int err_src = 0;
	u32 std_err_main_addr, std_err_main, err_reg;
	u32 base, slave_addr, clear;
	char *source_name;

	/* Get the Type of interrupt */
	if (irq == l3->app_irq)
		inttype = L3_APPLICATION_ERROR;
	else
		inttype = L3_DEBUG_ERROR;

	for (i = 0; i < L3_MODULES; i++) {
		/*
		 * Read the regerr register of the clock domain
		 * to determine the source
		 */
		base = (u32)l3->l3_base[i];
		err_reg =  readl(base + l3_flagmux[i] + (inttype << 3));

		/* Get the corresponding error and analyse */
		if (err_reg) {
			/* Identify the source from control status register */
			for (j = 0; !(err_reg & (1 << j)); j++)
									;

			err_src = j;
			/* Read the stderrlog_main_source from clk domain */
			std_err_main_addr = base + (*(l3_targ[i] + err_src));
			std_err_main =  readl(std_err_main_addr);

			switch ((std_err_main & CUSTOM_ERROR)) {
			case STANDARD_ERROR:
				source_name =
				l3_targ_stderrlog_main_name[i][err_src];

				slave_addr = std_err_main_addr +
						L3_SLAVE_ADDRESS_OFFSET;
				WARN(true, "L3 standard error: SOURCE:%s at address 0x%x\n",
					source_name, readl(slave_addr));
				/* clear the std error log*/
				clear = std_err_main | CLEAR_STDERR_LOG;
				writel(clear, std_err_main_addr);
				break;

			case CUSTOM_ERROR:
				source_name =
				l3_targ_stderrlog_main_name[i][err_src];

				WARN(true, "CUSTOM SRESP error with SOURCE:%s\n",
							source_name);
				/* clear the std error log*/
				clear = std_err_main | CLEAR_STDERR_LOG;
				writel(clear, std_err_main_addr);
				break;

			default:
				/* Nothing to be handled here as of now */
				break;
			}
		/* Error found so break the for loop */
		break;
		}
	}
	return IRQ_HANDLED;
}

static int __init omap4_l3_probe(struct platform_device *pdev)
{
	static struct omap4_l3		*l3;
	struct resource		*res;
	int			ret;
	int			irq;

	l3 = kzalloc(sizeof(*l3), GFP_KERNEL);
	if (!l3)
		ret = -ENOMEM;

	platform_set_drvdata(pdev, l3);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "couldn't find resource 0\n");
		ret = -ENODEV;
		goto err1;
	}

	l3->l3_base[0] = ioremap(res->start, resource_size(res));
	if (!(l3->l3_base[0])) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err2;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "couldn't find resource 1\n");
		ret = -ENODEV;
		goto err3;
	}

	l3->l3_base[1] = ioremap(res->start, resource_size(res));
	if (!(l3->l3_base[1])) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err4;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(&pdev->dev, "couldn't find resource 2\n");
		ret = -ENODEV;
		goto err5;
	}

	l3->l3_base[2] = ioremap(res->start, resource_size(res));
	if (!(l3->l3_base[2])) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err6;
	}

	/*
	 * Setup interrupt Handlers
	 */
	irq = platform_get_irq(pdev, 0);
	ret = request_irq(irq,
			l3_interrupt_handler,
			IRQF_DISABLED, "l3-dbg-irq", l3);
	if (ret) {
		pr_crit("L3: request_irq failed to register for 0x%x\n",
					 OMAP44XX_IRQ_L3_DBG);
		goto err7;
	}
	l3->debug_irq = irq;

	irq = platform_get_irq(pdev, 1);
	ret = request_irq(irq,
			l3_interrupt_handler,
			IRQF_DISABLED, "l3-app-irq", l3);
	if (ret) {
		pr_crit("L3: request_irq failed to register for 0x%x\n",
					 OMAP44XX_IRQ_L3_APP);
		goto err8;
	}
	l3->app_irq = irq;

	goto err0;
err8:
err7:
	iounmap(l3->l3_base[2]);
err6:
err5:
	iounmap(l3->l3_base[1]);
err4:
err3:
	iounmap(l3->l3_base[0]);
err2:
err1:
	kfree(l3);
err0:
	return ret;
}

static int __exit omap4_l3_remove(struct platform_device *pdev)
{
	struct omap4_l3         *l3 = platform_get_drvdata(pdev);

	free_irq(l3->app_irq, l3);
	free_irq(l3->debug_irq, l3);
	iounmap(l3->l3_base[0]);
	iounmap(l3->l3_base[1]);
	iounmap(l3->l3_base[2]);
	kfree(l3);

	return 0;
}

static struct platform_driver omap4_l3_driver = {
	.remove		= __exit_p(omap4_l3_remove),
	.driver		= {
	.name		= "omap_l3_noc",
	},
};

static int __init omap4_l3_init(void)
{
	return platform_driver_probe(&omap4_l3_driver, omap4_l3_probe);
}
postcore_initcall_sync(omap4_l3_init);

static void __exit omap4_l3_exit(void)
{
	platform_driver_unregister(&omap4_l3_driver);
}
module_exit(omap4_l3_exit);
