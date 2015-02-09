/*
 * drivers/mtd/nand/cs553x_nand.c
 *
 * (C) 2005, 2006 Red Hat Inc.
 *
 * Author: David Woodhouse <dwmw2@infradead.org>
 *	   Tom Sylla <tom.sylla@amd.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This is a device driver for the NAND flash controller found on
 *   the AMD CS5535/CS5536 companion chipsets for the Geode processor.
 *   mtd-id for command line partitioning is cs553x_nand_cs[0-3]
 *   where 0-3 reflects the chip select for NAND.
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

#include <asm/msr.h>
#include <asm/io.h>

#define NR_CS553X_CONTROLLERS	4

#define MSR_DIVIL_GLD_CAP	0x51400000	/* DIVIL capabilitiies */
#define CAP_CS5535		0x2df000ULL
#define CAP_CS5536		0x5df500ULL

/* NAND Timing MSRs */
#define MSR_NANDF_DATA		0x5140001b	/* NAND Flash Data Timing MSR */
#define MSR_NANDF_CTL		0x5140001c	/* NAND Flash Control Timing */
#define MSR_NANDF_RSVD		0x5140001d	/* Reserved */

/* NAND BAR MSRs */
#define MSR_DIVIL_LBAR_FLSH0	0x51400010	/* Flash Chip Select 0 */
#define MSR_DIVIL_LBAR_FLSH1	0x51400011	/* Flash Chip Select 1 */
#define MSR_DIVIL_LBAR_FLSH2	0x51400012	/* Flash Chip Select 2 */
#define MSR_DIVIL_LBAR_FLSH3	0x51400013	/* Flash Chip Select 3 */
	/* Each made up of... */
#define FLSH_LBAR_EN		(1ULL<<32)
#define FLSH_NOR_NAND		(1ULL<<33)	/* 1 for NAND */
#define FLSH_MEM_IO		(1ULL<<34)	/* 1 for MMIO */
	/* I/O BARs have BASE_ADDR in bits 15:4, IO_MASK in 47:36 */
	/* MMIO BARs have BASE_ADDR in bits 31:12, MEM_MASK in 63:44 */

/* Pin function selection MSR (IDE vs. flash on the IDE pins) */
#define MSR_DIVIL_BALL_OPTS	0x51400015
#define PIN_OPT_IDE		(1<<0)	/* 0 for flash, 1 for IDE */

/* Registers within the NAND flash controller BAR -- memory mapped */
#define MM_NAND_DATA		0x00	/* 0 to 0x7ff, in fact */
#define MM_NAND_CTL		0x800	/* Any even address 0x800-0x80e */
#define MM_NAND_IO		0x801	/* Any odd address 0x801-0x80f */
#define MM_NAND_STS		0x810
#define MM_NAND_ECC_LSB		0x811
#define MM_NAND_ECC_MSB		0x812
#define MM_NAND_ECC_COL		0x813
#define MM_NAND_LAC		0x814
#define MM_NAND_ECC_CTL		0x815

/* Registers within the NAND flash controller BAR -- I/O mapped */
#define IO_NAND_DATA		0x00	/* 0 to 3, in fact */
#define IO_NAND_CTL		0x04
#define IO_NAND_IO		0x05
#define IO_NAND_STS		0x06
#define IO_NAND_ECC_CTL		0x08
#define IO_NAND_ECC_LSB		0x09
#define IO_NAND_ECC_MSB		0x0a
#define IO_NAND_ECC_COL		0x0b
#define IO_NAND_LAC		0x0c

#define CS_NAND_CTL_DIST_EN	(1<<4)	/* Enable NAND Distract interrupt */
#define CS_NAND_CTL_RDY_INT_MASK	(1<<3)	/* Enable RDY/BUSY# interrupt */
#define CS_NAND_CTL_ALE		(1<<2)
#define CS_NAND_CTL_CLE		(1<<1)
#define CS_NAND_CTL_CE		(1<<0)	/* Keep low; 1 to reset */

#define CS_NAND_STS_FLASH_RDY	(1<<3)
#define CS_NAND_CTLR_BUSY	(1<<2)
#define CS_NAND_CMD_COMP	(1<<1)
#define CS_NAND_DIST_ST		(1<<0)

#define CS_NAND_ECC_PARITY	(1<<2)
#define CS_NAND_ECC_CLRECC	(1<<1)
#define CS_NAND_ECC_ENECC	(1<<0)

static void cs553x_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	struct nand_chip *this = mtd->priv;

	while (unlikely(len > 0x800)) {
		memcpy_fromio(buf, this->IO_ADDR_R, 0x800);
		buf += 0x800;
		len -= 0x800;
	}
	memcpy_fromio(buf, this->IO_ADDR_R, len);
}

static void cs553x_write_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	struct nand_chip *this = mtd->priv;

	while (unlikely(len > 0x800)) {
		memcpy_toio(this->IO_ADDR_R, buf, 0x800);
		buf += 0x800;
		len -= 0x800;
	}
	memcpy_toio(this->IO_ADDR_R, buf, len);
}

static unsigned char cs553x_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;
	return readb(this->IO_ADDR_R);
}

static void cs553x_write_byte(struct mtd_info *mtd, u_char byte)
{
	struct nand_chip *this = mtd->priv;
	int i = 100000;

	while (i && readb(this->IO_ADDR_R + MM_NAND_STS) & CS_NAND_CTLR_BUSY) {
		udelay(1);
		i--;
	}
	writeb(byte, this->IO_ADDR_W + 0x801);
}

static void cs553x_hwcontrol(struct mtd_info *mtd, int cmd,
			     unsigned int ctrl)
{
	struct nand_chip *this = mtd->priv;
	void __iomem *mmio_base = this->IO_ADDR_R;
	if (ctrl & NAND_CTRL_CHANGE) {
		unsigned char ctl = (ctrl & ~NAND_CTRL_CHANGE ) ^ 0x01;
		writeb(ctl, mmio_base + MM_NAND_CTL);
	}
	if (cmd != NAND_CMD_NONE)
		cs553x_write_byte(mtd, cmd);
}

static int cs553x_device_ready(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;
	void __iomem *mmio_base = this->IO_ADDR_R;
	unsigned char foo = readb(mmio_base + MM_NAND_STS);

	return (foo & CS_NAND_STS_FLASH_RDY) && !(foo & CS_NAND_CTLR_BUSY);
}

static void cs_enable_hwecc(struct mtd_info *mtd, int mode)
{
	struct nand_chip *this = mtd->priv;
	void __iomem *mmio_base = this->IO_ADDR_R;

	writeb(0x07, mmio_base + MM_NAND_ECC_CTL);
}

static int cs_calculate_ecc(struct mtd_info *mtd, const u_char *dat, u_char *ecc_code)
{
	uint32_t ecc;
	struct nand_chip *this = mtd->priv;
	void __iomem *mmio_base = this->IO_ADDR_R;

	ecc = readl(mmio_base + MM_NAND_STS);

	ecc_code[1] = ecc >> 8;
	ecc_code[0] = ecc >> 16;
	ecc_code[2] = ecc >> 24;
	return 0;
}

static struct mtd_info *cs553x_mtd[4];

static int __init cs553x_init_one(int cs, int mmio, unsigned long adr)
{
	int err = 0;
	struct nand_chip *this;
	struct mtd_info *new_mtd;

	printk(KERN_NOTICE "Probing CS553x NAND controller CS#%d at %sIO 0x%08lx\n", cs, mmio?"MM":"P", adr);

	if (!mmio) {
		printk(KERN_NOTICE "PIO mode not yet implemented for CS553X NAND controller\n");
		return -ENXIO;
	}

	/* Allocate memory for MTD device structure and private data */
	new_mtd = kmalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip), GFP_KERNEL);
	if (!new_mtd) {
		printk(KERN_WARNING "Unable to allocate CS553X NAND MTD device structure.\n");
		err = -ENOMEM;
		goto out;
	}

	/* Get pointer to private data */
	this = (struct nand_chip *)(&new_mtd[1]);

	/* Initialize structures */
	memset(new_mtd, 0, sizeof(struct mtd_info));
	memset(this, 0, sizeof(struct nand_chip));

	/* Link the private data with the MTD structure */
	new_mtd->priv = this;
	new_mtd->owner = THIS_MODULE;

	/* map physical address */
	this->IO_ADDR_R = this->IO_ADDR_W = ioremap(adr, 4096);
	if (!this->IO_ADDR_R) {
		printk(KERN_WARNING "ioremap cs553x NAND @0x%08lx failed\n", adr);
		err = -EIO;
		goto out_mtd;
	}

	this->cmd_ctrl = cs553x_hwcontrol;
	this->dev_ready = cs553x_device_ready;
	this->read_byte = cs553x_read_byte;
	this->read_buf = cs553x_read_buf;
	this->write_buf = cs553x_write_buf;

	this->chip_delay = 0;

	this->ecc.mode = NAND_ECC_HW;
	this->ecc.size = 256;
	this->ecc.bytes = 3;
	this->ecc.hwctl  = cs_enable_hwecc;
	this->ecc.calculate = cs_calculate_ecc;
	this->ecc.correct  = nand_correct_data;

	/* Enable the following for a flash based bad block table */
	this->options = NAND_USE_FLASH_BBT | NAND_NO_AUTOINCR;

	/* Scan to find existance of the device */
	if (nand_scan(new_mtd, 1)) {
		err = -ENXIO;
		goto out_ior;
	}

	new_mtd->name = kasprintf(GFP_KERNEL, "cs553x_nand_cs%d", cs);

	cs553x_mtd[cs] = new_mtd;
	goto out;

out_ior:
	iounmap(this->IO_ADDR_R);
out_mtd:
	kfree(new_mtd);
out:
	return err;
}

static int is_geode(void)
{
	/* These are the CPUs which will have a CS553[56] companion chip */
	if (boot_cpu_data.x86_vendor == X86_VENDOR_AMD &&
	    boot_cpu_data.x86 == 5 &&
	    boot_cpu_data.x86_model == 10)
		return 1; /* Geode LX */

	if ((boot_cpu_data.x86_vendor == X86_VENDOR_NSC ||
	     boot_cpu_data.x86_vendor == X86_VENDOR_CYRIX) &&
	    boot_cpu_data.x86 == 5 &&
	    boot_cpu_data.x86_model == 5)
		return 1; /* Geode GX (née GX2) */

	return 0;
}


#ifdef CONFIG_MTD_PARTITIONS
static const char *part_probes[] = { "cmdlinepart", NULL };
#endif


static int __init cs553x_init(void)
{
	int err = -ENXIO;
	int i;
	uint64_t val;

#ifdef CONFIG_MTD_PARTITIONS
	int mtd_parts_nb = 0;
	struct mtd_partition *mtd_parts = NULL;
#endif

	/* If the CPU isn't a Geode GX or LX, abort */
	if (!is_geode())
		return -ENXIO;

	/* If it doesn't have the CS553[56], abort */
	rdmsrl(MSR_DIVIL_GLD_CAP, val);
	val &= ~0xFFULL;
	if (val != CAP_CS5535 && val != CAP_CS5536)
		return -ENXIO;

	/* If it doesn't have the NAND controller enabled, abort */
	rdmsrl(MSR_DIVIL_BALL_OPTS, val);
	if (val & PIN_OPT_IDE) {
		printk(KERN_INFO "CS553x NAND controller: Flash I/O not enabled in MSR_DIVIL_BALL_OPTS.\n");
		return -ENXIO;
	}

	for (i = 0; i < NR_CS553X_CONTROLLERS; i++) {
		rdmsrl(MSR_DIVIL_LBAR_FLSH0 + i, val);

		if ((val & (FLSH_LBAR_EN|FLSH_NOR_NAND)) == (FLSH_LBAR_EN|FLSH_NOR_NAND))
			err = cs553x_init_one(i, !!(val & FLSH_MEM_IO), val & 0xFFFFFFFF);
	}

	/* Register all devices together here. This means we can easily hack it to
	   do mtdconcat etc. if we want to. */
	for (i = 0; i < NR_CS553X_CONTROLLERS; i++) {
		if (cs553x_mtd[i]) {

			/* If any devices registered, return success. Else the last error. */
#ifdef CONFIG_MTD_PARTITIONS
			mtd_parts_nb = parse_mtd_partitions(cs553x_mtd[i], part_probes, &mtd_parts, 0);
			if (mtd_parts_nb > 0) {
				printk(KERN_NOTICE "Using command line partition definition\n");
				add_mtd_partitions(cs553x_mtd[i], mtd_parts, mtd_parts_nb);
			} else {
				add_mtd_device(cs553x_mtd[i]);
			}
#else
			add_mtd_device(cs553x_mtd[i]);
#endif
			err = 0;
		}
	}

	return err;
}

module_init(cs553x_init);

static void __exit cs553x_cleanup(void)
{
	int i;

	for (i = 0; i < NR_CS553X_CONTROLLERS; i++) {
		struct mtd_info *mtd = cs553x_mtd[i];
		struct nand_chip *this;
		void __iomem *mmio_base;

		if (!mtd)
			continue;

		this = cs553x_mtd[i]->priv;
		mmio_base = this->IO_ADDR_R;

		/* Release resources, unregister device */
		nand_release(cs553x_mtd[i]);
		kfree(cs553x_mtd[i]->name);
		cs553x_mtd[i] = NULL;

		/* unmap physical address */
		iounmap(mmio_base);

		/* Free the MTD device structure */
		kfree(mtd);
	}
}

module_exit(cs553x_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("NAND controller driver for AMD CS5535/CS5536 companion chip");
