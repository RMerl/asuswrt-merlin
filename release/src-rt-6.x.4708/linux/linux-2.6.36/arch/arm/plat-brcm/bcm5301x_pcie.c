/* Modified by Broadcom Corp. Portions Copyright (c) Broadcom Corp, 2012. */
/*
 * Northstar PCI-Express driver
 * Only supports Root-Complex (RC) mode
 *
 * Notes:
 * PCI Domains are being used to identify the PCIe port 1:1.
 *
 * Only MEM access is supported, PAX does not support IO.
 *
 * TODO:
 *	MSI interrupts,
 *	DRAM > 128 MBytes (e.g. DMA zones)
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>

#include <mach/memory.h>
#include <mach/io_map.h>

#include <typedefs.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <siutils.h>
#include <hndcpu.h>
#include <hndpci.h>
#include <pcicfg.h>
#include <bcmdevs.h>
#include <bcmnvram.h>

/* Global SB handle */
extern si_t *bcm947xx_sih;
extern spinlock_t bcm947xx_sih_lock;

/* Convenience */
#define sih bcm947xx_sih
#define sih_lock bcm947xx_sih_lock

/*
 * Register offset definitions
 */
#define	SOC_PCIE_CONTROL	0x000	/* a.k.a. CLK_CONTROL reg */
#define	SOC_PCIE_PM_STATUS	0x008
#define	SOC_PCIE_PM_CONTROL	0x00c	/* in EP mode only ! */
#define SOC_PCIE_RC_AXI_CONFIG	0x100
#define	SOC_PCIE_EXT_CFG_ADDR	0x120
#define	SOC_PCIE_EXT_CFG_DATA	0x124
#define	SOC_PCIE_CFG_ADDR	0x1f8
#define	SOC_PCIE_CFG_DATA	0x1fc

#define	SOC_PCIE_SYS_RC_INTX_EN		0x330
#define	SOC_PCIE_SYS_RC_INTX_CSR	0x334
#define	SOC_PCIE_SYS_HOST_INTR_EN	0x344
#define	SOC_PCIE_SYS_HOST_INTR_CSR	0x348

#define	SOC_PCIE_HDR_OFF	0x400	/* 256 bytes per function */

#define	SOC_PCIE_IMAP0_0123_REGS_TYPE	0xcd0

/* 32-bit 4KB in-bound mapping windows for Function 0..3, n=0..7 */
#define	SOC_PCIE_SYS_IMAP0(f, n)	(0xc00+((f)<<5)+((n)<<2))
/* 64-bit in-bound mapping windows for func 0..3 */
#define	SOC_PCIE_SYS_IMAP1(f)		(0xc80+((f)<<3))
#define	SOC_PCIE_SYS_IMAP2(f)		(0xcc0+((f)<<3))
/* 64-bit in-bound address range n=0..2 */
#define	SOC_PCIE_SYS_IARR(n)		(0xd00+((n)<<3))
/* 64-bit out-bound address filter n=0..2 */
#define	SOC_PCIE_SYS_OARR(n)		(0xd20+((n)<<3))
/* 64-bit out-bound mapping windows n=0..2 */
#define	SOC_PCIE_SYS_OMAP(n)		(0xd40+((n)<<3))

#ifdef	__nonexistent_regs_
#define	SOC_PCIE_MDIO_CONTROL	0x128
#define	SOC_PCIE_MDIO_RD_DATA	0x12c
#define	SOC_PCIE_MDIO_WR_DATA	0x130
#define	SOC_PCIE_CLK_STAT	0x1e0
#endif

extern int _memsize;

#define PCI_MAX_BUS		4
#define PLX_PRIM_SEC_BUS_NUM		(0x00000201 | (PCI_MAX_BUS << 16))

static uint pcie_coreid, pcie_corerev;

#ifdef	CONFIG_PCI

/*
 * Forward declarations
 */
static int soc_pci_setup(int nr, struct pci_sys_data *sys);
static struct pci_bus *soc_pci_scan_bus(int nr, struct pci_sys_data *sys);
static int soc_pcie_map_irq(struct pci_dev *dev, u8 slot, u8 pin);
static int soc_pci_read_config(struct pci_bus *bus, unsigned int devfn,
                                   int where, int size, u32 *val);
static int soc_pci_write_config(struct pci_bus *bus, unsigned int devfn,
                                    int where, int size, u32 val);

#ifndef	CONFIG_PCI_DOMAINS
#error	CONFIG_PCI_DOMAINS is required
#endif

static int
sbpci_read_config_reg(struct pci_bus *bus, unsigned int devfn, int where,
                      int size, u32 *value)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&sih_lock, flags);
	ret = hndpci_read_config(sih, bus->number, PCI_SLOT(devfn),
	                        PCI_FUNC(devfn), where, value, size);
	spin_unlock_irqrestore(&sih_lock, flags);
	return ret ? PCIBIOS_DEVICE_NOT_FOUND : PCIBIOS_SUCCESSFUL;
}

static int
sbpci_write_config_reg(struct pci_bus *bus, unsigned int devfn, int where,
                       int size, u32 value)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&sih_lock, flags);
	ret = hndpci_write_config(sih, bus->number, PCI_SLOT(devfn),
	                         PCI_FUNC(devfn), where, &value, size);
	spin_unlock_irqrestore(&sih_lock, flags);
	return ret ? PCIBIOS_DEVICE_NOT_FOUND : PCIBIOS_SUCCESSFUL;
}

static struct pci_ops pcibios_ops = {
	sbpci_read_config_reg,
	sbpci_write_config_reg
};

/*
 * PCIe host controller registers
 * one entry per port
 */
static struct resource soc_pcie_regs[3] = {
	{
	.name = "pcie0",
	.start = 0x18012000,
	.end   = 0x18012fff,
	.flags = IORESOURCE_MEM,
	},
	{
	.name = "pcie1",
	.start = 0x18013000,
	.end   = 0x18013fff,
	.flags = IORESOURCE_MEM,
	},
	{
	.name = "pcie2",
	.start = 0x18014000,
	.end   = 0x18014fff,
	.flags = IORESOURCE_MEM,
	},
};

static struct resource soc_pcie_owin[3] = {
	{
	.name = "PCIe Outbound Window, Port 0",
	.start = 0x08000000,
	.end =   0x08000000 + SZ_128M - 1,
	.flags = IORESOURCE_MEM,
	},
	{
	.name = "PCIe Outbound Window, Port 1",
	.start = 0x40000000,
	.end =   0x40000000 + SZ_128M - 1,
	.flags = IORESOURCE_MEM,
	},
	{
	.name = "PCIe Outbound Window, Port 2",
	.start = 0x48000000,
	.end =   0x48000000 + SZ_128M - 1,
	.flags = IORESOURCE_MEM,
	},
};

/*
 * Per port control structure
 */
static struct soc_pcie_port {
	struct resource *regs_res;
	struct resource *owin_res;
	void * __iomem reg_base;
	unsigned short irqs[6];
	struct hw_pci hw_pci;

	bool enable;
	bool link;
	bool isswitch;
	bool port1active;
	bool port2active;
} soc_pcie_ports[4] = {
	{
	.irqs = {0, 0, 0, 0, 0, 0},
	.hw_pci = {
		.domain 	= 0,
		.swizzle	= NULL,
		.nr_controllers = 1,
		.map_irq 	= NULL,
		},
	.enable = 1,
	.isswitch = 0,
	.port1active = 0,
	.port2active = 0,
	},
	{
	.regs_res = & soc_pcie_regs[0],
	.owin_res = & soc_pcie_owin[0],
	.irqs = {158, 159, 160, 161, 162, 163},
	.hw_pci = {
		.domain 	= 1,
		.swizzle 	= pci_std_swizzle,
		.nr_controllers = 1,
		.setup 		= soc_pci_setup,
		.scan 		= soc_pci_scan_bus,
		.map_irq 	= soc_pcie_map_irq,
		},
	.enable = 1,
	.isswitch = 0,
	.port1active = 0,
	.port2active = 0,
	},
	{
	.regs_res = & soc_pcie_regs[1],
	.owin_res = & soc_pcie_owin[1],
	.irqs = {164, 165, 166, 167, 168, 169},
	.hw_pci = {
		.domain 	= 2,
		.swizzle 	= pci_std_swizzle,
		.nr_controllers = 1,
		.setup 		= soc_pci_setup,
		.scan 		= soc_pci_scan_bus,
		.map_irq 	= soc_pcie_map_irq,
		},
	.enable = 1,
	.isswitch = 0,
	.port1active = 0,
	.port2active = 0,
	},
	{
	.regs_res = & soc_pcie_regs[2],
	.owin_res = & soc_pcie_owin[2],
	.irqs = {170, 171, 172, 173, 174, 175},
	.hw_pci = {
		.domain 	= 3,
		.swizzle 	= pci_std_swizzle,
		.nr_controllers = 1,
		.setup 		= soc_pci_setup,
		.scan 		= soc_pci_scan_bus,
		.map_irq 	= soc_pcie_map_irq,
		},
	.enable = 1,
	.isswitch = 0,
	.port1active = 0,
	.port2active = 0,
	}
	};

/*
 * Methods for accessing configuration registers
 */
static struct pci_ops soc_pcie_ops = {
	.read = soc_pci_read_config,
	.write = soc_pci_write_config,
};

/*
 * Per hnd si bus devices irq map
 */
typedef struct si_bus_irq_map {
	unsigned short device;
	unsigned short unit;
	unsigned short max_unit;
	unsigned short irq;
} si_bus_irq_map_t;

si_bus_irq_map_t si_bus_irq_map[] = {
	{BCM47XX_GMAC_ID, 0, 4, 179}	/* 179, 180, 181, 182 */,
	{BCM47XX_USB20H_ID, 0, 1, 111}	/* 111 EHCI. */,
	{BCM47XX_USB20H_ID, 0, 1, 111}	/* 111 OHCI. */,
	{BCM47XX_USB30H_ID, 0, 5, 112}	/* 112, 113, 114, 115, 116. XHCI */,
	{BCM47XX_SDIO3H_ID, 0, 1, 177}  /* 177 SDIO3 */
};
#define SI_BUS_IRQ_MAP_SIZE (sizeof(si_bus_irq_map) / sizeof(si_bus_irq_map_t))

static int si_bus_map_irq(struct pci_dev *pdev)
{
	int i, irq = 0;

	for (i = 0; i < SI_BUS_IRQ_MAP_SIZE; i++) {
		if (pdev->device == si_bus_irq_map[i].device &&
		    si_bus_irq_map[i].unit < si_bus_irq_map[i].max_unit) {
			irq = si_bus_irq_map[i].irq + si_bus_irq_map[i].unit;
			si_bus_irq_map[i].unit++;
			break;
		}
	}

	return irq;
}

static struct soc_pcie_port *soc_pcie_sysdata2port(struct pci_sys_data *sysdata)
{
	unsigned port;

	port = sysdata->domain;
	BUG_ON(port >= ARRAY_SIZE(soc_pcie_ports));
	return & soc_pcie_ports[port];
}

static struct soc_pcie_port *soc_pcie_pdev2port(struct pci_dev *pdev)
{
	return soc_pcie_sysdata2port(pdev->sysdata);
}

static struct soc_pcie_port *soc_pcie_bus2port(struct pci_bus *bus)
{
	return soc_pcie_sysdata2port(bus->sysdata);
}

static struct pci_bus *soc_pci_scan_bus(int nr, struct pci_sys_data *sys)
{
	return pci_scan_bus(sys->busnr, &soc_pcie_ops, sys);
}

static int soc_pcie_map_irq(struct pci_dev *pdev, u8 slot, u8 pin)
{
	struct soc_pcie_port *port = soc_pcie_pdev2port(pdev);
	int irq;

	irq = port->irqs[5];	/* All INTx share int src 5, last per port */

	pr_debug("PCIe map irq: %04d:%02x:%02x.%02x slot %d, pin %d, irq: %d\n",
		pci_domain_nr(pdev->bus),
		pdev->bus->number,
		PCI_SLOT(pdev->devfn),
		PCI_FUNC(pdev->devfn),
		slot, pin, irq);

	return irq;
}

static void __iomem *soc_pci_cfg_base(struct pci_bus *bus, unsigned int devfn, int where)
{
	struct soc_pcie_port *port = soc_pcie_bus2port(bus);
	int busno = bus->number;
	int slot = PCI_SLOT(devfn);
	int fn = PCI_FUNC(devfn);
	void __iomem *base;
	int offset;
	int type;
	u32 addr_reg;

	base = port->reg_base;

	/* If there is no link, just show the PCI bridge. */
	if (!port->link && (busno > 0 || slot > 0))
		return NULL;

	if (busno == 0) {
		if (slot >= 1)
			return NULL;
		type = slot;
		__raw_writel(where & 0xffc, base + SOC_PCIE_EXT_CFG_ADDR);
		offset = SOC_PCIE_EXT_CFG_DATA;
	} else {
		/* WAR for function num > 1 */
		if (fn > 1)
			return NULL;
		type = 1;
		addr_reg = (busno & 0xff) << 20 |
			(slot << 15) |
			(fn << 12) |
			(where & 0xffc) |
			(type & 0x3);

		__raw_writel(addr_reg, base + SOC_PCIE_CFG_ADDR);
		offset = SOC_PCIE_CFG_DATA;
	}

	return base + offset;
}

static void plx_pcie_switch_init(struct pci_bus *bus, unsigned int devfn)
{
	struct soc_pcie_port *port = soc_pcie_bus2port(bus);
	u32 dRead = 0;
	u16 bm = 0;
	int bus_inc = port->hw_pci.domain - 1;

	soc_pci_read_config(bus, devfn, 0x100, 4, &dRead);
	printk("PCIE: Doing PLX switch Init...Test Read = %08x\n", (unsigned int)dRead);

	/* Debug control register. */
	soc_pci_read_config(bus, devfn, 0x1dc, 4, &dRead);
	dRead &= ~(1<<22);

	soc_pci_write_config(bus, devfn, 0x1dc, 4, dRead);

	/* Set GPIO enables. */
	soc_pci_read_config(bus, devfn, 0x62c, 4, &dRead);

	printk("PCIE: Doing PLX switch Init...GPIO Read = %08x\n", (unsigned int)dRead);

	dRead &= ~((1 << 0) | (1 << 1) | (1 << 3));
	dRead |= ((1 << 4) | (1 << 5) | (1 << 7));

	soc_pci_write_config(bus, devfn, 0x62c, 4, dRead);

	mdelay(50);
	dRead |= ((1<<0)|(1<<1));
	soc_pci_write_config(bus, devfn, 0x62c, 4, dRead);

	soc_pci_read_config(bus, devfn, 0x4, 2, &bm);
#if NS_PCI_DEBUG
	printk("bus master: %08x\n", bm);
#endif
	bm |= 0x06;
	soc_pci_write_config(bus, devfn, 0x4, 2, bm);
	bm = 0;
#if NS_PCI_DEBUG
	soc_pci_read_config(bus, devfn, 0x4, 2, &bm);
	printk("bus master after: %08x\n", bm);
	bm = 0;
#endif
	/* Bus 1 if the upstream port of the switch.
	 * Bus 2 has the two downstream ports, one on each device number.
	 */
	if (bus->number == (bus_inc + 1)) {
		soc_pci_write_config(bus, devfn, 0x18, 4, PLX_PRIM_SEC_BUS_NUM);

		/* TODO: We need to scan all outgoing windows,
		 * to look for a base limit pair for this register.
		 */
		/* MEM_BASE, MEM_LIM require 1MB alignment */
		BUG_ON((port->owin_res->start >> 16) & 0xf);
		soc_pci_write_config(bus, devfn, PCI_MEMORY_BASE, 2,
			port->owin_res->start >> 16);
		BUG_ON(((port->owin_res->start + SZ_32M) >> 16) & 0xf);
		soc_pci_write_config(bus, devfn, PCI_MEMORY_LIMIT, 2,
			(port->owin_res->start + SZ_32M) >> 16);
	} else if (bus->number == (bus_inc + 2)) {
		/* TODO: I need to fix these hard coded addresses. */
		if (devfn == 0x8) {
			soc_pci_write_config(bus, devfn, 0x18, 4,
				(0x00000000 | ((bus->number + 1) << 16) |
				((bus->number + 1) << 8) | bus->number));
			BUG_ON((port->owin_res->start + SZ_48M >> 16) & 0xf);
			soc_pci_write_config(bus, devfn, PCI_MEMORY_BASE, 2,
				port->owin_res->start + SZ_48M >> 16);
			BUG_ON(((port->owin_res->start + SZ_48M + SZ_32M) >> 16) & 0xf);
			soc_pci_write_config(bus, devfn, PCI_MEMORY_LIMIT, 2,
				(port->owin_res->start + SZ_48M + SZ_32M) >> 16);
			soc_pci_read_config(bus, devfn, 0x7A, 2, &bm);
			if (bm & PCI_EXP_LNKSTA_DLLLA)
				port->port1active = 1;
			printk("bm = %04x\n devfn = = %08x, bus = %08x\n", bm, devfn, bus->number);
		} else if (devfn == 0x10) {
			soc_pci_write_config(bus, devfn, 0x18, 4,
				(0x00000000 | ((bus->number + 2) << 16) |
				((bus->number + 2) << 8) | bus->number));
			BUG_ON((port->owin_res->start + (SZ_48M * 2) >> 16) & 0xf);
			soc_pci_write_config(bus, devfn, PCI_MEMORY_BASE, 2,
				port->owin_res->start  + (SZ_48M * 2) >> 16);
			BUG_ON(((port->owin_res->start + (SZ_48M * 2) + SZ_32M) >> 16) & 0xf);
			soc_pci_write_config(bus, devfn, PCI_MEMORY_LIMIT, 2,
				(port->owin_res->start + (SZ_48M * 2) + SZ_32M) >> 16);
			soc_pci_read_config(bus, devfn, 0x7A, 2, &bm);
			if (bm & PCI_EXP_LNKSTA_DLLLA)
				port->port2active = 1;
			printk("bm = %04x\n devfn = = %08x, bus = %08x\n", bm, devfn, bus->number);
		}
	}
}

static int soc_pci_read_config(struct pci_bus *bus, unsigned int devfn,
	int where, int size, u32 *val)
{
	void __iomem *base;
	u32 data_reg;
	struct soc_pcie_port *port = soc_pcie_bus2port(bus);
	int bus_inc = port->hw_pci.domain - 1;

	if ((bus->number > (bus_inc + 4))) {
		*val = ~0UL;
		return PCIBIOS_SUCCESSFUL;
	}

	if (port->isswitch == 1) {
		if (bus->number == (bus_inc + 2)) {
			if (!((devfn == 0x8) || (devfn == 0x10))) {
				*val = ~0UL;
				return PCIBIOS_SUCCESSFUL;
			}
		} else if ((bus->number == (bus_inc + 3)) || (bus->number == (bus_inc + 4))) {
			if (devfn != 0) {
				*val = ~0UL;
				return PCIBIOS_SUCCESSFUL;
			} else if ((bus->number == (bus_inc + 3)) && (port->port1active == 0)) {
				*val = ~0UL;
				return PCIBIOS_SUCCESSFUL;
			} else if ((bus->number == (bus_inc + 4)) && (port->port2active == 0)) {
				*val = ~0UL;
				return PCIBIOS_SUCCESSFUL;
			}
		}
	}

	base = soc_pci_cfg_base(bus, devfn, where);
	if (base == NULL) {
		*val = ~0UL;
		return PCIBIOS_SUCCESSFUL;
	}

	data_reg = __raw_readl(base);

	/* NS: CLASS field is R/O, and set to wrong 0x200 value */
	if (bus->number == 0 && devfn == 0) {
		if ((where & 0xffc) == PCI_CLASS_REVISION) {
		/*
		 * RC's class is 0x0280, but Linux PCI driver needs 0x604
		 * for a PCIe bridge. So we must fixup the class code
		 * to 0x604 here.
		 */
			data_reg &= 0xff;
			data_reg |= 0x604 << 16;
		}
	}

	if ((bus->number == (bus_inc + 1)) && (port->isswitch == 0) &&
		(where == 0) && (((data_reg >> 16) & 0x0000FFFF) == 0x00008603)) {
		plx_pcie_switch_init(bus, devfn);
		port->isswitch = 1;
	}
	if ((bus->number == (bus_inc + 2)) && (port->isswitch == 1) &&
		(where == 0) && (((data_reg >> 16) & 0x0000FFFF) == 0x00008603))
		plx_pcie_switch_init(bus, devfn);

	/* HEADER_TYPE=00 indicates the port in EP mode */

	if (size == 4) {
		*val = data_reg;
	} else if (size < 4) {
		u32 mask = (1 << (size * 8)) - 1;
		int shift = (where % 4) * 8;
		*val = (data_reg >> shift) & mask;
	}

	return PCIBIOS_SUCCESSFUL;
}

static int soc_pci_write_config(struct pci_bus *bus, unsigned int devfn,
                                    int where, int size, u32 val)
{
	void __iomem *base;
	u32 data_reg;
	struct soc_pcie_port *port = soc_pcie_bus2port(bus);
	int bus_inc = port->hw_pci.domain - 1;

	if (bus->number > (bus_inc + 4))
		return PCIBIOS_SUCCESSFUL;

	if ((bus->number == (bus_inc + 2)) && (port->isswitch == 1)) {
		if (!((devfn == 0x8) || (devfn == 0x10)))
			return PCIBIOS_SUCCESSFUL;
	}
	else if ((bus->number == (bus_inc + 3)) && (port->isswitch == 1)) {
		if (devfn != 0)
			return PCIBIOS_SUCCESSFUL;
	}
	else if ((bus->number == (bus_inc + 4)) && (port->isswitch == 1)) {
		if (devfn != 0)
			return PCIBIOS_SUCCESSFUL;
	}

	base = soc_pci_cfg_base(bus, devfn, where);
	if (base == NULL) {
		return PCIBIOS_SUCCESSFUL;
	}

	if (size < 4) {
		u32 mask = (1 << (size * 8)) - 1;
		int shift = (where % 4) * 8;
		data_reg = __raw_readl(base);
		data_reg &= ~(mask << shift);
		data_reg |= (val & mask) << shift;
	} else {
		data_reg = val;
	}

	__raw_writel(data_reg, base);

	return PCIBIOS_SUCCESSFUL;
}

static int soc_pci_setup(int nr, struct pci_sys_data *sys)
{
	struct soc_pcie_port *port = soc_pcie_sysdata2port(sys);

	BUG_ON(request_resource(&iomem_resource, port->owin_res));

	sys->resource[0] = port->owin_res;
	sys->private_data = port;
	return 1;
}

/*
 * Check link status, return 0 if link is up in RC mode,
 * otherwise return non-zero
 */
static int __init noinline soc_pcie_check_link(struct soc_pcie_port *port, uint32 allow_gen2)
{
	u32 devfn = 0;
	u16 pos, tmp16;
	u8 nlw, tmp8;
	u32 tmp32;

	struct pci_sys_data sd = {
		.domain = port->hw_pci.domain,
	};
	struct pci_bus bus = {
		.number = 0,
		.ops = &soc_pcie_ops,
		.sysdata = &sd,
	};

	if (!port->enable)
		return -EINVAL;


	pci_bus_read_config_dword(&bus, devfn, 0xdc, &tmp32);
	tmp32 &= ~0xf;
	if (allow_gen2)
		tmp32 |= 2;
	else {
		/* force PCIE GEN1 */
		tmp32 |= 1;
	}
	pci_bus_write_config_dword(&bus, devfn, 0xdc, tmp32);

	/* See if the port is in EP mode, indicated by header type 00 */
	pci_bus_read_config_byte(&bus, devfn, PCI_HEADER_TYPE, &tmp8);
	if (tmp8 != PCI_HEADER_TYPE_BRIDGE) {
		pr_info("PCIe port %d in End-Point mode - ignored\n",
			port->hw_pci.domain);
		return -ENODEV;
	}

	/* NS PAX only changes NLW field when card is present */
	pos = pci_bus_find_capability(&bus, devfn, PCI_CAP_ID_EXP);
	pci_bus_read_config_word(&bus, devfn, pos + PCI_EXP_LNKSTA, &tmp16);

#ifdef	DEBUG
	pr_debug("PCIE%d: LINKSTA reg %#x val %#x\n", port->hw_pci.domain,
		pos+PCI_EXP_LNKSTA, tmp16);
#endif

	nlw = (tmp16 & PCI_EXP_LNKSTA_NLW) >> PCI_EXP_LNKSTA_NLW_SHIFT;
	port->link = tmp16 & PCI_EXP_LNKSTA_DLLLA;

	if (nlw != 0)
		port->link = 1;

#ifdef	DEBUG
	for (; pos < 0x100; pos += 2) {
		pci_bus_read_config_word(&bus, devfn, pos, &tmp16);
		if (tmp16)
			pr_debug("reg[%#x]=%#x, ", pos, tmp16);
	}
#endif
	pr_info("PCIE%d link=%d\n", port->hw_pci.domain,  port->link);

	return ((port->link)? 0: -ENOSYS);
}

/*
 * Initializte the PCIe controller
 */
static void __init soc_pcie_hw_init(struct soc_pcie_port *port)
{
	u32 devfn = 0;
	u32 tmp32;
	struct pci_sys_data sd = {
		.domain = port->hw_pci.domain,
	};
	struct pci_bus bus = {
		.number = 0,
		.ops = &soc_pcie_ops,
		.sysdata = &sd,
	};

	/* Change MPS and MRRS to 512 */
	pci_bus_read_config_word(&bus, devfn, 0x4d4, &tmp32);
	tmp32 &= ~7;
	tmp32 |= 2;
	pci_bus_write_config_word(&bus, devfn, 0x4d4, tmp32);

	pci_bus_read_config_dword(&bus, devfn, 0xb4, &tmp32);
	tmp32 &= ~((7 << 12) | (7 << 5));
	tmp32 |= (2 << 12) | (2 << 5);
	pci_bus_write_config_dword(&bus, devfn, 0xb4, tmp32);

	/* Turn-on Root-Complex (RC) mode, from reset defailt of EP */

	/* The mode is set by straps, can be overwritten via DMU
	   register <cru_straps_control> bit 5, "1" means RC
	 */

	/* Send a downstream reset */
	__raw_writel(0x3, port->reg_base + SOC_PCIE_CONTROL);
	udelay(250);
	__raw_writel(0x1, port->reg_base + SOC_PCIE_CONTROL);
	mdelay(250);

	/* TBD: take care of PM, check we're on */
}

/*
 * Setup the address translation
 */
static void __init soc_pcie_map_init(struct soc_pcie_port *port)
{
	unsigned size, i;
	u32 addr;

	/*
	 * NOTE:
	 * All PCI-to-CPU address mapping are 1:1 for simplicity
	 */

	/* Outbound address translation setup */
	size = resource_size(port->owin_res);
	addr = port->owin_res->start;
	BUG_ON(!addr);
	BUG_ON(addr & ((1 << 25) - 1));	/* 64MB alignment */

	for (i = 0; i < 3; i++) {
		const unsigned win_size = SZ_64M;
		/* 64-bit LE regs, write low word, high is 0 at reset */
		__raw_writel(addr, port->reg_base + SOC_PCIE_SYS_OMAP(i));
		__raw_writel(addr|0x1, port->reg_base + SOC_PCIE_SYS_OARR(i));
		addr += win_size;
		if (size >= win_size)
			size -= win_size;
		if (size == 0)
			break;
	}
	WARN_ON(size > 0);

	if (pcie_coreid == NS_PCIEG2_CORE_ID && pcie_corerev == NS_PCIEG2_CORE_REV_B0) {
		/* Enable FUNC0_IMAP0_0/1/2/3 from RO to RW for NS-B0 */
		__raw_writel(0x1, port->reg_base + SOC_PCIE_IMAP0_0123_REGS_TYPE);
		/* 4KB memory page pointing to CCB for NS-B0 */
		addr = (0x18001 << 12) | 0x1;
		__raw_writel(addr, port->reg_base + SOC_PCIE_SYS_IMAP0(0, 1));
	}

	/* 
	 * Inbound address translation setup
	 * Northstar only maps up to 128 MiB inbound, DRAM could be up to 1 GiB.
	 *
	 * For now allow access to entire DRAM, assuming it is less than 128MiB,
	 * otherwise DMA bouncing mechanism may be required.
	 * Also consider DMA mask to limit DMA physical address
	 */
	size = min(_memsize, SZ_128M);
	addr = PHYS_OFFSET;

	size >>= 20;	/* In MB */
	size &= 0xff;	/* Size is an 8-bit field */

	WARN_ON(size == 0);
	/* 64-bit LE regs, write low word, high is 0 at reset */
	__raw_writel(addr | 0x1,
		port->reg_base + SOC_PCIE_SYS_IMAP1(0));
	__raw_writel(addr | size,
		port->reg_base + SOC_PCIE_SYS_IARR(1));

	if (_memsize <= SZ_128M) {
		return;
	}
	
#ifdef CONFIG_SPARSEMEM
	addr = PHYS_OFFSET2;

	if (pcie_coreid == NS_PCIEG2_CORE_ID && pcie_corerev == NS_PCIEG2_CORE_REV_B0) {
		/* Means 1GB for NS-B0 IARR_2 */
		size = 1;
	} else {
		size = min(_memsize - SZ_128M, SZ_128M);
		size >>= 20;	/* In MB */
		size &= 0xff;	/* Size is an 8-bit field */
	}

	__raw_writel(addr | 0x1,
		port->reg_base + SOC_PCIE_SYS_IMAP2(0));
	__raw_writel(addr | size,
		port->reg_base + SOC_PCIE_SYS_IARR(2));
#endif
}

/*
 * Setup PCIE Host bridge
 */
static void __init noinline soc_pcie_bridge_init(struct soc_pcie_port *port)
{
	u32 devfn = 0;
	u8 tmp8;
	u16 tmp16;

	/* Fake <bus> object */
	struct pci_sys_data sd = {
		.domain = port->hw_pci.domain,
	};
	struct pci_bus bus = {
		.number = 0,
		.ops = &soc_pcie_ops,
		.sysdata = &sd,
	};


	pci_bus_write_config_byte(&bus, devfn, PCI_PRIMARY_BUS, 0);
	pci_bus_write_config_byte(&bus, devfn, PCI_SECONDARY_BUS, 1);
	pci_bus_write_config_byte(&bus, devfn, PCI_SUBORDINATE_BUS, 4);

	pci_bus_read_config_byte(&bus, devfn, PCI_PRIMARY_BUS, &tmp8);
	pci_bus_read_config_byte(&bus, devfn, PCI_SECONDARY_BUS, &tmp8);
	pci_bus_read_config_byte(&bus, devfn, PCI_SUBORDINATE_BUS, &tmp8);

	/* MEM_BASE, MEM_LIM require 1MB alignment */
	BUG_ON((port->owin_res->start >> 16) & 0xf);
#ifdef	DEBUG
	pr_debug("%s: membase %#x memlimit %#x\n", __FUNCTION__,
		port->owin_res->start, port->owin_res->end+1);
#endif
	pci_bus_write_config_word(&bus, devfn, PCI_MEMORY_BASE,
		port->owin_res->start >> 16);
	BUG_ON(((port->owin_res->end+1) >> 16) & 0xf);
	pci_bus_write_config_word(&bus, devfn, PCI_MEMORY_LIMIT,
		(port->owin_res->end+1) >> 16);

	/* These registers are not supported on the NS */
	pci_bus_write_config_word(&bus, devfn, PCI_IO_BASE_UPPER16, 0);
	pci_bus_write_config_word(&bus, devfn, PCI_IO_LIMIT_UPPER16, 0);

	/* Force class to that of a Bridge */
	pci_bus_write_config_word(&bus, devfn, PCI_CLASS_DEVICE, PCI_CLASS_BRIDGE_PCI);

	pci_bus_read_config_word(&bus, devfn, PCI_CLASS_DEVICE, &tmp16);
	pci_bus_read_config_word(&bus, devfn, PCI_MEMORY_BASE, &tmp16);
	pci_bus_read_config_word(&bus, devfn, PCI_MEMORY_LIMIT, &tmp16);
}


int
pcibios_enable_resources(struct pci_dev *dev)
{
	u16 cmd, old_cmd;
	int idx;
	struct resource *r;

	/* External PCI only */
	if (dev->bus->number == 0)
		return 0;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	old_cmd = cmd;
	for (idx = 0; idx < 6; idx++) {
		r = &dev->resource[idx];
		if (r->flags & IORESOURCE_IO)
			cmd |= PCI_COMMAND_IO;
		if (r->flags & IORESOURCE_MEM)
			cmd |= PCI_COMMAND_MEMORY;
	}
	if (dev->resource[PCI_ROM_RESOURCE].start)
		cmd |= PCI_COMMAND_MEMORY;
	if (cmd != old_cmd) {
		printk("PCI: Enabling device %s (%04x -> %04x)\n", pci_name(dev), old_cmd, cmd);
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	return 0;
}

static void
bcm5301x_usb_power_on(int coreid)
{
	int enable_usb;

	if (coreid == NS_USB20_CORE_ID) {
		enable_usb = getgpiopin(NULL, "usbport1", GPIO_PIN_NOTDEFINED);
		if (enable_usb != GPIO_PIN_NOTDEFINED) {
			int enable_usb_mask = 1 << enable_usb;

			si_gpioout(sih, enable_usb_mask, enable_usb_mask, GPIO_DRV_PRIORITY);
			si_gpioouten(sih, enable_usb_mask, enable_usb_mask, GPIO_DRV_PRIORITY);
		}

		enable_usb = getgpiopin(NULL, "usbport2", GPIO_PIN_NOTDEFINED);
		if (enable_usb != GPIO_PIN_NOTDEFINED) {
			int enable_usb_mask = 1 << enable_usb;

			si_gpioout(sih, enable_usb_mask, enable_usb_mask, GPIO_DRV_PRIORITY);
			si_gpioouten(sih, enable_usb_mask, enable_usb_mask, GPIO_DRV_PRIORITY);
		}
	}
	else if (coreid == NS_USB30_CORE_ID) {
		enable_usb = getgpiopin(NULL, "usbport2", GPIO_PIN_NOTDEFINED);
		if (enable_usb != GPIO_PIN_NOTDEFINED) {
			int enable_usb_mask = 1 << enable_usb;

			si_gpioout(sih, enable_usb_mask, enable_usb_mask, GPIO_DRV_PRIORITY);
			si_gpioouten(sih, enable_usb_mask, enable_usb_mask, GPIO_DRV_PRIORITY);
		}
	}
}

static void
bcm5301x_usb20_phy_init(void)
{
	uint32 *genpll_base;
	uint32 val, ndiv, pdiv, ch2_mdiv, ch2_freq;
	uint32 usb_pll_pdiv, usb_pll_ndiv;

	/* Check Chip ID */
	if (!BCM4707_CHIP(CHIPID(sih->chip))) {
		return;
	}

	/* reg map for genpll base address */
	genpll_base = (uint32 *)REG_MAP(0x1800C140, 4096);

	/* get divider integer from the cru_genpll_control5 */
	val = readl(genpll_base + 0x5);
	ndiv = (val >> 20) & 0x3ff;
	if (ndiv == 0)
		ndiv = 1 << 10;

	/* get pdiv and ch2_mdiv from the cru_genpll_control6 */
	val = readl(genpll_base + 0x6);
	pdiv = (val >> 24) & 0x7;
	pdiv = (pdiv == 0) ? (1 << 3) : pdiv;

	ch2_mdiv = val & 0xff;
	ch2_mdiv = (ch2_mdiv == 0) ? (1 << 8) : ch2_mdiv;

	/* calculate ch2_freq based on 25MHz reference clock */
	ch2_freq = (25000000 / (pdiv * ch2_mdiv)) * ndiv;

	/* get usb_pll_pdiv from the cru_usb2_control */
	val = readl(genpll_base + 0x9);
	usb_pll_pdiv = (val >> 12) & 0x7;
	usb_pll_pdiv = (usb_pll_pdiv == 0) ? (1 << 3) : usb_pll_pdiv;

	/* calculate usb_pll_ndiv based on a solid 1920MHz that is for USB2 phy */
	usb_pll_ndiv = (1920000000 * usb_pll_pdiv) / ch2_freq;

	/* unlock in cru_clkset_key */
	writel(0x0000ea68, genpll_base + 0x10);

	/* set usb_pll_ndiv to cru_usb2_control */
	val &= ~(0x3ff << 2);
	val |= (usb_pll_ndiv << 2);
	writel(val, genpll_base + 0x9);

	/* lock in cru_clkset_key */
	writel(0x00000000, genpll_base + 0x10);

	/* reg unmap */
	REG_UNMAP((void *)genpll_base);
}

static void
bcm5301x_usb30_phy_init(void)
{
	uint32 ccb_mii_base;
	uint32 dmu_base;
	uint32 *ccb_mii_mng_ctrl_addr;
	uint32 *ccb_mii_mng_cmd_data_addr;
	uint32 *cru_rst_addr;
	uint32 cru_straps_ctrl;
	uint32 usb3_idm_idm_base;
	uint32 *usb3_idm_idm_reset_ctrl_addr;

	/* Check Chip ID */
	if (!BCM4707_CHIP(CHIPID(sih->chip)))
		return;

	dmu_base = (uint32)REG_MAP(0x1800c000, 4096);
	/* Check strapping of PCIE/USB3 SEL */
	cru_straps_ctrl = readl((uint32 *)(dmu_base + 0x2a0));
	if ((cru_straps_ctrl & 0x10) == 0)
		goto out;
	cru_rst_addr = (uint32 *)(dmu_base +  0x184);

	/* Reg map */
	ccb_mii_base = (uint32)REG_MAP(0x18003000, 4096);
	ccb_mii_mng_ctrl_addr = (uint32 *)(ccb_mii_base + 0x0);
	ccb_mii_mng_cmd_data_addr = (uint32 *)(ccb_mii_base + 0x4);
	usb3_idm_idm_base = (uint32)REG_MAP(0x18105000, 4096);
	usb3_idm_idm_reset_ctrl_addr = (uint32 *)(usb3_idm_idm_base + 0x800);

	/* Perform USB3 system soft reset */
	writel(0x00000001, usb3_idm_idm_reset_ctrl_addr);

	/* Enable MDIO. Setting MDCDIV as 26  */
	writel(0x0000009a, ccb_mii_mng_ctrl_addr);
	OSL_DELAY(2);

	if ((CHIPID(sih->chip) == BCM4707_CHIP_ID &&
	    (CHIPREV(sih->chiprev) == 4 || CHIPREV(sih->chiprev) == 6)) ||
	    (CHIPID(sih->chip) == BCM47094_CHIP_ID)) {
		/* PLL30 block */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x587e8000, ccb_mii_mng_cmd_data_addr);

		/* Clear ana_pllSeqStart */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x58061000, ccb_mii_mng_cmd_data_addr);

		/* CMOS Divider ratio to 25 */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x582a6400, ccb_mii_mng_cmd_data_addr);

		/* Asserting PLL Reset */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x582ec000, ccb_mii_mng_cmd_data_addr);

		/* Deasserting PLL Reset */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x582e8000, ccb_mii_mng_cmd_data_addr);

		/* Deasserting USB3 system reset */
		writel(0x00000000, usb3_idm_idm_reset_ctrl_addr);

		/* Set ana_pllSeqStart */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x58069000, ccb_mii_mng_cmd_data_addr);

		/* RXPMD block */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x587e8020, ccb_mii_mng_cmd_data_addr);

		/* CDR int loop locking BW to 1 */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x58120049, ccb_mii_mng_cmd_data_addr);

		/* CDR int loop acquisition BW to 1 */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x580e0049, ccb_mii_mng_cmd_data_addr);

		/* CDR prop loop BW to 1 */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x580a005c, ccb_mii_mng_cmd_data_addr);

		/* Waiting MII Mgt interface idle */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
	}
	/* NS-Ax */
	else if (CHIPID(sih->chip) == BCM4707_CHIP_ID) {
		/* PLL30 block */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x587e8000, ccb_mii_mng_cmd_data_addr);

		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x582a6400, ccb_mii_mng_cmd_data_addr);

		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x587e80e0, ccb_mii_mng_cmd_data_addr);

		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x580a009c, ccb_mii_mng_cmd_data_addr);

		/* Enable SSC */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x587e8040, ccb_mii_mng_cmd_data_addr);

		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x580a21d3, ccb_mii_mng_cmd_data_addr);

		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x58061003, ccb_mii_mng_cmd_data_addr);

		/* Waiting MII Mgt interface idle */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);

		/* Deasserting USB3 system reset */
		writel(0x00000000, usb3_idm_idm_reset_ctrl_addr);
	}
	else if (CHIPID(sih->chip) == BCM53018_CHIP_ID) {
		/* USB3 PLL Block */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x587e8000, ccb_mii_mng_cmd_data_addr);

		/* Assert Ana_Pllseq start */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x58061000, ccb_mii_mng_cmd_data_addr);

		/* Assert CML Divider ratio to 26 */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x582a6400, ccb_mii_mng_cmd_data_addr);

		/* Asserting PLL Reset */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x582ec000, ccb_mii_mng_cmd_data_addr);

		/* Deaaserting PLL Reset */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x582e8000, ccb_mii_mng_cmd_data_addr);

		/* Waiting MII Mgt interface idle */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);

		/* Deasserting USB3 system reset */
		writel(0x00000000, usb3_idm_idm_reset_ctrl_addr);

		/* PLL frequency monitor enable */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x58069000, ccb_mii_mng_cmd_data_addr);

		/* PIPE Block */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x587e8060, ccb_mii_mng_cmd_data_addr);

		/* CMPMAX & CMPMINTH setting */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x580af30d, ccb_mii_mng_cmd_data_addr);

		/* DEGLITCH MIN & MAX setting */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x580e6302, ccb_mii_mng_cmd_data_addr);

		/* TXPMD block */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x587e8040, ccb_mii_mng_cmd_data_addr);

		/* Enabling SSC */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
		writel(0x58061003, ccb_mii_mng_cmd_data_addr);

		/* Waiting MII Mgt interface idle */
		SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);
	}

	/* Reg unmap */
	REG_UNMAP((void *)ccb_mii_base);
	REG_UNMAP((void *)usb3_idm_idm_base);

out:
	REG_UNMAP((void *)dmu_base);
}

 static void
bcm5301x_usb_and_3rd_pcie_phy_reset(void)
{
	static bool phy_reset = FALSE;
	uint32 dmu_base;
	uint32 *cru_rst_addr;
	uint32 val;

	/* only reset once */
	if (phy_reset)
		return;

	/* NS-Bx and NS47094
	* Chiprev 4 for NS-B0 and chiprev 6 for NS-B1
	*/
	if ((CHIPID(sih->chip) == BCM4707_CHIP_ID &&
		(CHIPREV(sih->chiprev) == 4 || CHIPREV(sih->chiprev) == 6)) ||
		(CHIPID(sih->chip) == BCM47094_CHIP_ID)) {
			/* reset PHY */
			dmu_base = (uint32)REG_MAP(0x1800c000, 4096);
			cru_rst_addr = (uint32 *)(dmu_base +  0x184);
			val = readl(cru_rst_addr);
			val &= ~(0x3 << 3);
			writel(val, cru_rst_addr);
			OSL_DELAY(100);
			val |= (0x3 << 3);
			writel(val, cru_rst_addr);
			REG_UNMAP((void *)dmu_base);
			phy_reset = TRUE;
	}
}

static void
bcm5301x_usb_phy_init(int coreid)
{
	bcm5301x_usb_and_3rd_pcie_phy_reset();

	if (coreid == NS_USB20_CORE_ID) {
		bcm5301x_usb20_phy_init();
	}
	else if (coreid == NS_USB30_CORE_ID) {
		bcm5301x_usb30_phy_init();
	}
}

static void
bcm5301x_usb_hc_init(struct pci_dev *dev, int coreid)
{
	uint32 start, len;

	if (!BCM4707_CHIP(CHIPID(sih->chip)))
		return;

	if (coreid == NS_USB20_CORE_ID) {
		uint32 ehci_base;
		uint32 *insnreg01, *insnreg03;

		start = pci_resource_start(dev, 0);
		len = pci_resource_len(dev, 0);
		if (!len)
			return;

		/* Delay after PHY initialized to ensure HC is ready to be configured */
		mdelay(1);

		ehci_base = (uint32)REG_MAP(start, len);
		insnreg01 = (uint32 *)(ehci_base + 0x94);
		insnreg03 = (uint32 *)(ehci_base + 0x9C);
		/* Set packet buffer OUT threshold */
		writel(((readl(insnreg01) & 0xFFFF) | (0x80 << 16)), insnreg01);
		/* Enabling break memory transfer */
		writel((readl(insnreg03) | 0x1), insnreg03);
		REG_UNMAP((void *)ehci_base);
	}
}

static void
bcm5301x_sdio_init(void)
{
	uint32 sdio3_idm_idm_base;
	uint32 *sdio3_idm_idm_reset_ctrl_addr;
	uint32 dmu_base;
	uint32 *cru_gpio_control0_addr;
	uint32 val32;
	uint32 mask;

	/* Check Chip ID */
	if (!BCM4707_CHIP(CHIPID(sih->chip)) ||
		(sih->chippkg != BCM4709_PKG_ID))
		return;

	dmu_base = (uint32)REG_MAP(0x1800c000, 4096);
	cru_gpio_control0_addr = (uint32 *)(dmu_base + 0x1c0);
	mask = ((1 << 19) |     /* SDIO_CARD_PWR_CTL */
		(1 << 20));     /* SDIO_EN_1P8 */
	val32 = readl(cru_gpio_control0_addr);
	val32 &= ~mask;
	writel(val32, cru_gpio_control0_addr);
	REG_UNMAP((void *)dmu_base);

	sdio3_idm_idm_base = (uint32)REG_MAP(0x18116000, 4096);
	sdio3_idm_idm_reset_ctrl_addr = (uint32 *)(sdio3_idm_idm_base + 0x800);
	/* Perform SDIO3 system soft reset */
	writel(0x00000001, sdio3_idm_idm_reset_ctrl_addr);
	OSL_DELAY(1);
	/* Deasserting SDIO3 system reset */
	writel(0x00000000, sdio3_idm_idm_reset_ctrl_addr);
	REG_UNMAP((void *)sdio3_idm_idm_base);
}

int
pcibios_enable_device(struct pci_dev *dev, int mask)
{
	ulong flags;
	uint coreidx, coreid;
	void *regs;
	int rc = -1;

	/* External PCI device enable */
	if (dev->bus->number != 0)
		return pcibios_enable_resources(dev);

	/* These cores come out of reset enabled */
	if (dev->device == NS_IHOST_CORE_ID ||
	    dev->device == CC_CORE_ID)
		return 0;

	spin_lock_irqsave(&sih_lock, flags);

	regs = si_setcoreidx(sih, PCI_SLOT(dev->devfn));
	coreidx = si_coreidx(sih);
	coreid = si_coreid(sih);

	if (!regs) {
		printk(KERN_ERR "WARNING! PCIBIOS_DEVICE_NOT_FOUND\n");
		goto out;
	}

	/* OHCI/EHCI only initialize one time */
	if (coreid == NS_USB20_CORE_ID && si_iscoreup(sih)) {
		rc = 0;
		goto out;
	}

	si_core_reset(sih, 0, 0);

	if (coreid == NS_USB20_CORE_ID || coreid == NS_USB30_CORE_ID) {
		/* Set gpio HIGH to turn on USB VBUS power */
		bcm5301x_usb_power_on(coreid);

		/* USB PHY init */
		bcm5301x_usb_phy_init(coreid);

		/* USB HC init */
		bcm5301x_usb_hc_init(dev, coreid);
	}
	else if (coreid == NS_SDIO3_CORE_ID){
		bcm5301x_sdio_init();
	}

	rc = 0;
out:
	si_setcoreidx(sih, coreidx);
	spin_unlock_irqrestore(&sih_lock, flags);

	return rc;
}

bool __devinit
plat_fixup_bus(struct pci_bus *b)
{
	struct list_head *ln;
	struct pci_dev *d;
	u8 irq;

	printk("PCI: Fixing up bus %d\n", b->number);

	/* Fix up SB */
	if (((struct pci_sys_data *)b->sysdata)->domain == 0) {
		for (ln = b->devices.next; ln != &b->devices; ln = ln->next) {
			d = pci_dev_b(ln);
			/* Fix up interrupt lines */
			pci_read_config_byte(d, PCI_INTERRUPT_LINE, &irq);
			d->irq = si_bus_map_irq(d);
			pci_write_config_byte(d, PCI_INTERRUPT_LINE, d->irq);
		}
		return TRUE;
	}

	return FALSE;
}

static int __init allow_gen2_rc(struct soc_pcie_port *port)
{
	uint32 vendorid, devid, chipid, chiprev;
	uint32 val, bar, base;
	int allow = 1;
	char *p;

	/* Force GEN1 if specified in NVRAM */
	if ((p = nvram_get("forcegen1rc")) != NULL && simple_strtoul(p, NULL, 0) == 1) {
		printk(KERN_NOTICE "Force PCIE RC to GEN1 only\n");
		return 0;
	}

	/* Read PCI vendor/device ID's */
	__raw_writel(0x0, port->reg_base + SOC_PCIE_CFG_ADDR);
	val = __raw_readl(port->reg_base + SOC_PCIE_CFG_DATA);
	vendorid = val & 0xffff;
	devid = val >> 16;
	if (vendorid == VENDOR_BROADCOM &&
	    (devid == BCM4360_CHIP_ID || devid == BCM4360_D11AC_ID ||
	     devid == BCM4360_D11AC2G_ID || devid == BCM4360_D11AC5G_ID ||
	     devid == BCM4352_D11AC_ID || devid == BCM4352_D11AC2G_ID ||
	     devid == BCM4352_D11AC5G_ID)) {
		/* Config BAR0 */
		bar = port->owin_res->start;
		__raw_writel(0x10, port->reg_base + SOC_PCIE_CFG_ADDR);
		__raw_writel(bar, port->reg_base + SOC_PCIE_CFG_DATA);
		/* Config BAR0 window to access chipc */
		__raw_writel(0x80, port->reg_base + SOC_PCIE_CFG_ADDR);
		__raw_writel(SI_ENUM_BASE, port->reg_base + SOC_PCIE_CFG_DATA);

		/* Enable memory resource */
		__raw_writel(0x4, port->reg_base + SOC_PCIE_CFG_ADDR);
		val = __raw_readl(port->reg_base + SOC_PCIE_CFG_DATA);
		val |= PCI_COMMAND_MEMORY;
		__raw_writel(val, port->reg_base + SOC_PCIE_CFG_DATA);
		/* Enable memory and bus master */
		__raw_writel(0x6, port->reg_base + SOC_PCIE_HDR_OFF + 4);

		/* Read CHIP ID */
		base = (uint32)ioremap(bar, 0x1000);
		val = __raw_readl(base);
		iounmap((void *)base);
		chipid = val & 0xffff;
		chiprev = (val >> 16) & 0xf;
		if ((chipid == BCM4360_CHIP_ID ||
		     chipid == BCM43460_CHIP_ID ||
		     chipid == BCM4352_CHIP_ID) && (chiprev < 3))
			allow = 0;
	}
	return (allow);
}

static void __init
bcm5301x_3rd_pcie_init(void)
{
	uint32 cru_straps_ctrl;
	uint32 ccb_mii_base;
	uint32 dmu_base;
	uint32 *ccb_mii_mng_ctrl_addr;
	uint32 *ccb_mii_mng_cmd_data_addr;

	/* Check Chip ID */
	if (!BCM4707_CHIP(CHIPID(sih->chip)) ||
	    (sih->chippkg != BCM4708_PKG_ID && sih->chippkg != BCM4709_PKG_ID))
		return;

	/* Reg map */
	dmu_base = (uint32)REG_MAP(0x1800c000, 4096);

	/* Check strapping of PCIE/USB3 SEL */
	cru_straps_ctrl = readl((uint32 *)(dmu_base + 0x2a0));
	/* PCIE mode is not selected */
	if (cru_straps_ctrl & 0x10)
		goto out;

	bcm5301x_usb_and_3rd_pcie_phy_reset();

	/* Reg map */
	ccb_mii_base = (uint32)REG_MAP(0x18003000, 4096);
	ccb_mii_mng_ctrl_addr = (uint32 *)(ccb_mii_base + 0x0);
	ccb_mii_mng_cmd_data_addr = (uint32 *)(ccb_mii_base + 0x4);

	/* MDIO setting. set MDC-> MDCDIV is 7'd8 */
	writel(0x00000088, ccb_mii_mng_ctrl_addr);
	SPINWAIT(((readl(ccb_mii_mng_ctrl_addr) >> 8 & 1) == 1), 1000);
	/* PCIE PLL block register (base 0x8000) */
	writel(0x57fe8000, ccb_mii_mng_cmd_data_addr);
	SPINWAIT(((readl(ccb_mii_mng_ctrl_addr) >> 8 & 1) == 1), 1000);
	/* Check PCIE PLL lock status */
	writel(0x67c60000, ccb_mii_mng_cmd_data_addr);
	SPINWAIT(((readl(ccb_mii_mng_ctrl_addr) >> 8 & 1) == 1), 1000);

	/* Reg unmap */
	REG_UNMAP((void *)ccb_mii_base);
out:
	REG_UNMAP((void *)dmu_base);
}

static void __init
bcm5301x_pcie_phy_init(void)
{
	uint32 ccb_mii_base;
	uint32 *ccb_mii_mng_ctrl_addr;
	uint32 *ccb_mii_mng_cmd_data_addr;
	uint32 dmu_base, cru_straps_ctrl;
	uint32 blkaddr = 0x863, regaddr;
	uint32 sb = 1, op_w = 1, pa[3] = {0x0, 0x1, 0xf}, blkra = 0x1f, ta = 2;
	uint32 i, val;

	/* Check Chip ID */
	if (!BCM4707_CHIP(CHIPID(sih->chip)))
		return;

	/* Reg map */
	dmu_base = (uint32)REG_MAP(0x1800c000, 4096);
	ccb_mii_base = (uint32)REG_MAP(0x18003000, 4096);
	ccb_mii_mng_ctrl_addr = (uint32 *)ccb_mii_base;
	ccb_mii_mng_cmd_data_addr = (uint32 *)(ccb_mii_base + 0x4);

	/* Set MDC/MDIO for Internal phy */
	SPINWAIT(((readl(ccb_mii_mng_ctrl_addr) >> 8 & 1) == 1), 1000);
	writel(0x0000009a, ccb_mii_mng_ctrl_addr);

	/* To improve PCIE phy jitter */
	for (i = 0; i < (ARRAY_SIZE(soc_pcie_ports) - 1); i++) {
		if (i == 2) {
			cru_straps_ctrl = readl((uint32 *)(dmu_base + 0x2a0));

			/* 3rd PCIE is not selected */
			if (cru_straps_ctrl & 0x10)
				break;
		}

		/* Change blkaddr */
		SPINWAIT(((readl(ccb_mii_mng_ctrl_addr) >> 8 & 1) == 1), 1000);
		val = (sb << 30) | (op_w << 28) | (pa[i] << 23) | (blkra << 18) |
			(ta << 16) | (blkaddr << 4);
		writel(val, ccb_mii_mng_cmd_data_addr);

		/* Write 0x0190 to 0x13 regaddr */
		SPINWAIT(((readl(ccb_mii_mng_ctrl_addr) >> 8 & 1) == 1), 1000);
		regaddr = 0x13;
		val = (sb << 30) | (op_w << 28) | (pa[i] << 23) | (regaddr << 18) |
			(ta << 16) | 0x0190;
		writel(val, ccb_mii_mng_cmd_data_addr);

		/* Write 0x0191 to 0x19 regaddr */
		SPINWAIT(((readl(ccb_mii_mng_ctrl_addr) >> 8 & 1) == 1), 1000);
		regaddr = 0x19;
		val = (sb << 30) | (op_w << 28) | (pa[i] << 23) | (regaddr << 18) |
			(ta << 16) | 0x0191;
		writel(val, ccb_mii_mng_cmd_data_addr);
	}

	/* Waiting MII Mgt interface idle */
	SPINWAIT((((readl(ccb_mii_mng_ctrl_addr) >> 8) & 1) == 1), 1000);

	/* Reg unmap */
	REG_UNMAP((void *)dmu_base);
	REG_UNMAP((void *)ccb_mii_base);
}

static int __init soc_pcie_init(void)
{
	unsigned int i;
	int allow_gen2, linkfail;
	uint origidx;
	unsigned long flags;


	hndpci_init(sih);

	spin_lock_irqsave(&sih_lock, flags);

	/* Save current core index */
	origidx = si_coreidx(sih);
	/* Get pcie coreid and corerev */
	si_setcore(sih, NS_PCIEG2_CORE_ID, 0);

	pcie_coreid = si_coreid(sih);
	pcie_corerev = si_corerev(sih);

	/* Restore core index */
	si_setcoreidx(sih, origidx);

	spin_unlock_irqrestore(&sih_lock, flags);

	/* For NS-B0, overwrite the start and end values for PCIE port 1 and port 2 */
	if (pcie_coreid == NS_PCIEG2_CORE_ID && pcie_corerev == NS_PCIEG2_CORE_REV_B0) {
		soc_pcie_owin[1].start = 0x20000000;
		soc_pcie_owin[1].end = 0x20000000 + SZ_128M - 1;

		soc_pcie_owin[2].start = 0x28000000;
		soc_pcie_owin[2].end = 0x28000000 + SZ_128M - 1;
	}

	/* Scan the SB bus */
	printk(KERN_INFO "PCI: scanning bus %x\n", 0);
	pci_scan_bus(0, &pcibios_ops, &soc_pcie_ports[0].hw_pci);

	bcm5301x_3rd_pcie_init();

	bcm5301x_pcie_phy_init();

	for (i = 1; i < ARRAY_SIZE(soc_pcie_ports); i++) {
		struct soc_pcie_port *port = &soc_pcie_ports[i];

		/* Check if this port needs to be enabled */
		if (!port->enable)
			continue;

		/* Setup PCIe controller registers */
		BUG_ON(request_resource(&iomem_resource, port->regs_res));
		port->reg_base =
			ioremap(port->regs_res->start,
			resource_size(port->regs_res));
		BUG_ON(IS_ERR_OR_NULL(port->reg_base));

		for (allow_gen2 = 0; allow_gen2 <= 1; allow_gen2++) {
			soc_pcie_hw_init(port);
			soc_pcie_map_init(port);

			/*
			 * Skip inactive ports -
			 * will need to change this for hot-plugging
			 */
			linkfail = soc_pcie_check_link(port, allow_gen2);
			if (linkfail)
				break;

			soc_pcie_bridge_init(port);

			if (allow_gen2 == 0) {
				if (allow_gen2_rc(port) == 0)
					break;
				pr_info("PCIE%d switching to GEN2\n", port->hw_pci.domain);
			}
		}

		if (linkfail)
			continue;

		/* Announce this port to ARM/PCI common code */
		pci_common_init(&port->hw_pci);

		/* Setup virtual-wire interrupts */
		__raw_writel(0xf, port->reg_base + SOC_PCIE_SYS_RC_INTX_EN);

		/* Enable memory and bus master */
		__raw_writel(0x6, port->reg_base + SOC_PCIE_HDR_OFF + 4);
	}

	return 0;
}

device_initcall(soc_pcie_init);

#endif	/* CONFIG_PCI */
