/*
 * Platform device setup for Marvell mv64360/mv64460 host bridges (Discovery)
 *
 * Author: Dale Farnsworth <dale@farnsworth.org>
 *
 * 2007 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/mv643xx.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>

#include <asm/prom.h>

/* These functions provide the necessary setup for the mv64x60 drivers. */

static struct of_device_id __initdata of_mv64x60_devices[] = {
	{ .compatible = "marvell,mv64306-devctrl", },
	{}
};

/*
 * Create MPSC platform devices
 */
static int __init mv64x60_mpsc_register_shared_pdev(struct device_node *np)
{
	struct platform_device *pdev;
	struct resource r[2];
	struct mpsc_shared_pdata pdata;
	const phandle *ph;
	struct device_node *mpscrouting, *mpscintr;
	int err;

	ph = of_get_property(np, "mpscrouting", NULL);
	mpscrouting = of_find_node_by_phandle(*ph);
	if (!mpscrouting)
		return -ENODEV;

	err = of_address_to_resource(mpscrouting, 0, &r[0]);
	of_node_put(mpscrouting);
	if (err)
		return err;

	ph = of_get_property(np, "mpscintr", NULL);
	mpscintr = of_find_node_by_phandle(*ph);
	if (!mpscintr)
		return -ENODEV;

	err = of_address_to_resource(mpscintr, 0, &r[1]);
	of_node_put(mpscintr);
	if (err)
		return err;

	memset(&pdata, 0, sizeof(pdata));

	pdev = platform_device_alloc(MPSC_SHARED_NAME, 0);
	if (!pdev)
		return -ENOMEM;

	err = platform_device_add_resources(pdev, r, 2);
	if (err)
		goto error;

	err = platform_device_add_data(pdev, &pdata, sizeof(pdata));
	if (err)
		goto error;

	err = platform_device_add(pdev);
	if (err)
		goto error;

	return 0;

error:
	platform_device_put(pdev);
	return err;
}


static int __init mv64x60_mpsc_device_setup(struct device_node *np, int id)
{
	struct resource r[5];
	struct mpsc_pdata pdata;
	struct platform_device *pdev;
	const unsigned int *prop;
	const phandle *ph;
	struct device_node *sdma, *brg;
	int err;
	int port_number;

	/* only register the shared platform device the first time through */
	if (id == 0 && (err = mv64x60_mpsc_register_shared_pdev(np)))
		return err;

	memset(r, 0, sizeof(r));

	err = of_address_to_resource(np, 0, &r[0]);
	if (err)
		return err;

	of_irq_to_resource(np, 0, &r[4]);

	ph = of_get_property(np, "sdma", NULL);
	sdma = of_find_node_by_phandle(*ph);
	if (!sdma)
		return -ENODEV;

	of_irq_to_resource(sdma, 0, &r[3]);
	err = of_address_to_resource(sdma, 0, &r[1]);
	of_node_put(sdma);
	if (err)
		return err;

	ph = of_get_property(np, "brg", NULL);
	brg = of_find_node_by_phandle(*ph);
	if (!brg)
		return -ENODEV;

	err = of_address_to_resource(brg, 0, &r[2]);
	of_node_put(brg);
	if (err)
		return err;

	prop = of_get_property(np, "cell-index", NULL);
	if (!prop)
		return -ENODEV;
	port_number = *(int *)prop;

	memset(&pdata, 0, sizeof(pdata));

	pdata.cache_mgmt = 1; /* All current revs need this set */

	pdata.max_idle = 40; /* default */
	prop = of_get_property(np, "max_idle", NULL);
	if (prop)
		pdata.max_idle = *prop;

	prop = of_get_property(brg, "current-speed", NULL);
	if (prop)
		pdata.default_baud = *prop;

	/* Default is 8 bits, no parity, no flow control */
	pdata.default_bits = 8;
	pdata.default_parity = 'n';
	pdata.default_flow = 'n';

	prop = of_get_property(np, "chr_1", NULL);
	if (prop)
		pdata.chr_1_val = *prop;

	prop = of_get_property(np, "chr_2", NULL);
	if (prop)
		pdata.chr_2_val = *prop;

	prop = of_get_property(np, "chr_10", NULL);
	if (prop)
		pdata.chr_10_val = *prop;

	prop = of_get_property(np, "mpcr", NULL);
	if (prop)
		pdata.mpcr_val = *prop;

	prop = of_get_property(brg, "bcr", NULL);
	if (prop)
		pdata.bcr_val = *prop;

	pdata.brg_can_tune = 1; /* All current revs need this set */

	prop = of_get_property(brg, "clock-src", NULL);
	if (prop)
		pdata.brg_clk_src = *prop;

	prop = of_get_property(brg, "clock-frequency", NULL);
	if (prop)
		pdata.brg_clk_freq = *prop;

	pdev = platform_device_alloc(MPSC_CTLR_NAME, port_number);
	if (!pdev)
		return -ENOMEM;
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	err = platform_device_add_resources(pdev, r, 5);
	if (err)
		goto error;

	err = platform_device_add_data(pdev, &pdata, sizeof(pdata));
	if (err)
		goto error;

	err = platform_device_add(pdev);
	if (err)
		goto error;

	return 0;

error:
	platform_device_put(pdev);
	return err;
}

/*
 * Create mv64x60_eth platform devices
 */
static struct platform_device * __init mv64x60_eth_register_shared_pdev(
						struct device_node *np, int id)
{
	struct platform_device *pdev;
	struct resource r[1];
	int err;

	err = of_address_to_resource(np, 0, &r[0]);
	if (err)
		return ERR_PTR(err);

	pdev = platform_device_register_simple(MV643XX_ETH_SHARED_NAME, id,
					       r, 1);
	return pdev;
}

static int __init mv64x60_eth_device_setup(struct device_node *np, int id,
					   struct platform_device *shared_pdev)
{
	struct resource r[1];
	struct mv643xx_eth_platform_data pdata;
	struct platform_device *pdev;
	struct device_node *phy;
	const u8 *mac_addr;
	const int *prop;
	const phandle *ph;
	int err;

	memset(r, 0, sizeof(r));
	of_irq_to_resource(np, 0, &r[0]);

	memset(&pdata, 0, sizeof(pdata));

	pdata.shared = shared_pdev;

	prop = of_get_property(np, "reg", NULL);
	if (!prop)
		return -ENODEV;
	pdata.port_number = *prop;

	mac_addr = of_get_mac_address(np);
	if (mac_addr)
		memcpy(pdata.mac_addr, mac_addr, 6);

	prop = of_get_property(np, "speed", NULL);
	if (prop)
		pdata.speed = *prop;

	prop = of_get_property(np, "tx_queue_size", NULL);
	if (prop)
		pdata.tx_queue_size = *prop;

	prop = of_get_property(np, "rx_queue_size", NULL);
	if (prop)
		pdata.rx_queue_size = *prop;

	prop = of_get_property(np, "tx_sram_addr", NULL);
	if (prop)
		pdata.tx_sram_addr = *prop;

	prop = of_get_property(np, "tx_sram_size", NULL);
	if (prop)
		pdata.tx_sram_size = *prop;

	prop = of_get_property(np, "rx_sram_addr", NULL);
	if (prop)
		pdata.rx_sram_addr = *prop;

	prop = of_get_property(np, "rx_sram_size", NULL);
	if (prop)
		pdata.rx_sram_size = *prop;

	ph = of_get_property(np, "phy", NULL);
	if (!ph)
		return -ENODEV;

	phy = of_find_node_by_phandle(*ph);
	if (phy == NULL)
		return -ENODEV;

	prop = of_get_property(phy, "reg", NULL);
	if (prop)
		pdata.phy_addr = MV643XX_ETH_PHY_ADDR(*prop);

	of_node_put(phy);

	pdev = platform_device_alloc(MV643XX_ETH_NAME, id);
	if (!pdev)
		return -ENOMEM;

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	err = platform_device_add_resources(pdev, r, 1);
	if (err)
		goto error;

	err = platform_device_add_data(pdev, &pdata, sizeof(pdata));
	if (err)
		goto error;

	err = platform_device_add(pdev);
	if (err)
		goto error;

	return 0;

error:
	platform_device_put(pdev);
	return err;
}

/*
 * Create mv64x60_i2c platform devices
 */
static int __init mv64x60_i2c_device_setup(struct device_node *np, int id)
{
	struct resource r[2];
	struct platform_device *pdev;
	struct mv64xxx_i2c_pdata pdata;
	const unsigned int *prop;
	int err;

	memset(r, 0, sizeof(r));

	err = of_address_to_resource(np, 0, &r[0]);
	if (err)
		return err;

	of_irq_to_resource(np, 0, &r[1]);

	memset(&pdata, 0, sizeof(pdata));

	pdata.freq_m = 8;	/* default */
	prop = of_get_property(np, "freq_m", NULL);
	if (prop)
		pdata.freq_m = *prop;

	pdata.freq_m = 3;	/* default */
	prop = of_get_property(np, "freq_n", NULL);
	if (prop)
		pdata.freq_n = *prop;

	pdata.timeout = 1000;				/* default: 1 second */

	pdev = platform_device_alloc(MV64XXX_I2C_CTLR_NAME, id);
	if (!pdev)
		return -ENOMEM;

	err = platform_device_add_resources(pdev, r, 2);
	if (err)
		goto error;

	err = platform_device_add_data(pdev, &pdata, sizeof(pdata));
	if (err)
		goto error;

	err = platform_device_add(pdev);
	if (err)
		goto error;

	return 0;

error:
	platform_device_put(pdev);
	return err;
}

/*
 * Create mv64x60_wdt platform devices
 */
static int __init mv64x60_wdt_device_setup(struct device_node *np, int id)
{
	struct resource r;
	struct platform_device *pdev;
	struct mv64x60_wdt_pdata pdata;
	const unsigned int *prop;
	int err;

	err = of_address_to_resource(np, 0, &r);
	if (err)
		return err;

	memset(&pdata, 0, sizeof(pdata));

	pdata.timeout = 10;			/* Default: 10 seconds */

	np = of_get_parent(np);
	if (!np)
		return -ENODEV;

	prop = of_get_property(np, "clock-frequency", NULL);
	of_node_put(np);
	if (!prop)
		return -ENODEV;
	pdata.bus_clk = *prop / 1000000; /* wdt driver wants freq in MHz */

	pdev = platform_device_alloc(MV64x60_WDT_NAME, id);
	if (!pdev)
		return -ENOMEM;

	err = platform_device_add_resources(pdev, &r, 1);
	if (err)
		goto error;

	err = platform_device_add_data(pdev, &pdata, sizeof(pdata));
	if (err)
		goto error;

	err = platform_device_add(pdev);
	if (err)
		goto error;

	return 0;

error:
	platform_device_put(pdev);
	return err;
}

static int __init mv64x60_device_setup(void)
{
	struct device_node *np, *np2;
	struct platform_device *pdev;
	int id, id2;
	int err;

	id = 0;
	for_each_compatible_node(np, "serial", "marvell,mv64360-mpsc") {
		err = mv64x60_mpsc_device_setup(np, id++);
		if (err)
			printk(KERN_ERR "Failed to initialize MV64x60 "
					"serial device %s: error %d.\n",
					np->full_name, err);
	}

	id = 0;
	id2 = 0;
	for_each_compatible_node(np, NULL, "marvell,mv64360-eth-group") {
		pdev = mv64x60_eth_register_shared_pdev(np, id++);
		if (IS_ERR(pdev)) {
			err = PTR_ERR(pdev);
			printk(KERN_ERR "Failed to initialize MV64x60 "
					"network block %s: error %d.\n",
					np->full_name, err);
			continue;
		}
		for_each_child_of_node(np, np2) {
			if (!of_device_is_compatible(np2,
					"marvell,mv64360-eth"))
				continue;
			err = mv64x60_eth_device_setup(np2, id2++, pdev);
			if (err)
				printk(KERN_ERR "Failed to initialize "
						"MV64x60 network device %s: "
						"error %d.\n",
						np2->full_name, err);
		}
	}

	id = 0;
	for_each_compatible_node(np, "i2c", "marvell,mv64360-i2c") {
		err = mv64x60_i2c_device_setup(np, id++);
		if (err)
			printk(KERN_ERR "Failed to initialize MV64x60 I2C "
					"bus %s: error %d.\n",
					np->full_name, err);
	}

	/* support up to one watchdog timer */
	np = of_find_compatible_node(np, NULL, "marvell,mv64360-wdt");
	if (np) {
		if ((err = mv64x60_wdt_device_setup(np, id)))
			printk(KERN_ERR "Failed to initialize MV64x60 "
					"Watchdog %s: error %d.\n",
					np->full_name, err);
		of_node_put(np);
	}

	/* Now add every node that is on the device bus */
	for_each_compatible_node(np, NULL, "marvell,mv64360")
		of_platform_bus_probe(np, of_mv64x60_devices, NULL);

	return 0;
}
arch_initcall(mv64x60_device_setup);

static int __init mv64x60_add_mpsc_console(void)
{
	struct device_node *np = NULL;
	const char *prop;

	prop = of_get_property(of_chosen, "linux,stdout-path", NULL);
	if (prop == NULL)
		goto not_mpsc;

	np = of_find_node_by_path(prop);
	if (!np)
		goto not_mpsc;

	if (!of_device_is_compatible(np, "marvell,mv64360-mpsc"))
		goto not_mpsc;

	prop = of_get_property(np, "cell-index", NULL);
	if (!prop)
		goto not_mpsc;

	add_preferred_console("ttyMM", *(int *)prop, NULL);

not_mpsc:
	return 0;
}
console_initcall(mv64x60_add_mpsc_console);
