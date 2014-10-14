/*
 *  sata_via.c - VIA Serial ATA controllers
 *
 *  Maintained by:  Jeff Garzik <jgarzik@pobox.com>
 * 		   Please ALWAYS copy linux-ide@vger.kernel.org
 *		   on emails.
 *
 *  Copyright 2003-2004 Red Hat, Inc.  All rights reserved.
 *  Copyright 2003-2004 Jeff Garzik
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *  libata documentation is available via 'make {ps|pdf}docs',
 *  as Documentation/DocBook/libata.*
 *
 *  Hardware documentation available under NDA.
 *
 *
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>

#define DRV_NAME	"sata_via"
#define DRV_VERSION	"2.6"

/*
 * vt8251 is different from other sata controllers of VIA.  It has two
 * channels, each channel has both Master and Slave slot.
 */
enum board_ids_enum {
	vt6420,
	vt6421,
	vt8251,
};

enum {
	SATA_CHAN_ENAB		= 0x40, /* SATA channel enable */
	SATA_INT_GATE		= 0x41, /* SATA interrupt gating */
	SATA_NATIVE_MODE	= 0x42, /* Native mode enable */
	PATA_UDMA_TIMING	= 0xB3, /* PATA timing for DMA/ cable detect */
	PATA_PIO_TIMING		= 0xAB, /* PATA timing register */

	PORT0			= (1 << 1),
	PORT1			= (1 << 0),
	ALL_PORTS		= PORT0 | PORT1,

	NATIVE_MODE_ALL		= (1 << 7) | (1 << 6) | (1 << 5) | (1 << 4),

	SATA_EXT_PHY		= (1 << 6), /* 0==use PATA, 1==ext phy */
};

static int svia_init_one(struct pci_dev *pdev, const struct pci_device_id *ent);
static int svia_scr_read(struct ata_link *link, unsigned int sc_reg, u32 *val);
static int svia_scr_write(struct ata_link *link, unsigned int sc_reg, u32 val);
static int vt8251_scr_read(struct ata_link *link, unsigned int scr, u32 *val);
static int vt8251_scr_write(struct ata_link *link, unsigned int scr, u32 val);
static void svia_tf_load(struct ata_port *ap, const struct ata_taskfile *tf);
static void svia_noop_freeze(struct ata_port *ap);
static int vt6420_prereset(struct ata_link *link, unsigned long deadline);
static void vt6420_bmdma_start(struct ata_queued_cmd *qc);
static int vt6421_pata_cable_detect(struct ata_port *ap);
static void vt6421_set_pio_mode(struct ata_port *ap, struct ata_device *adev);
static void vt6421_set_dma_mode(struct ata_port *ap, struct ata_device *adev);

static const struct pci_device_id svia_pci_tbl[] = {
	{ PCI_VDEVICE(VIA, 0x5337), vt6420 },
	{ PCI_VDEVICE(VIA, 0x0591), vt6420 }, /* 2 sata chnls (Master) */
	{ PCI_VDEVICE(VIA, 0x3149), vt6420 }, /* 2 sata chnls (Master) */
	{ PCI_VDEVICE(VIA, 0x3249), vt6421 }, /* 2 sata chnls, 1 pata chnl */
	{ PCI_VDEVICE(VIA, 0x5372), vt6420 },
	{ PCI_VDEVICE(VIA, 0x7372), vt6420 },
	{ PCI_VDEVICE(VIA, 0x5287), vt8251 }, /* 2 sata chnls (Master/Slave) */
	{ PCI_VDEVICE(VIA, 0x9000), vt8251 },

	{ }	/* terminate list */
};

static struct pci_driver svia_pci_driver = {
	.name			= DRV_NAME,
	.id_table		= svia_pci_tbl,
	.probe			= svia_init_one,
#ifdef CONFIG_PM
	.suspend		= ata_pci_device_suspend,
	.resume			= ata_pci_device_resume,
#endif
	.remove			= ata_pci_remove_one,
};

static struct scsi_host_template svia_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};

static struct ata_port_operations svia_base_ops = {
	.inherits		= &ata_bmdma_port_ops,
	.sff_tf_load		= svia_tf_load,
};

static struct ata_port_operations vt6420_sata_ops = {
	.inherits		= &svia_base_ops,
	.freeze			= svia_noop_freeze,
	.prereset		= vt6420_prereset,
	.bmdma_start		= vt6420_bmdma_start,
};

static struct ata_port_operations vt6421_pata_ops = {
	.inherits		= &svia_base_ops,
	.cable_detect		= vt6421_pata_cable_detect,
	.set_piomode		= vt6421_set_pio_mode,
	.set_dmamode		= vt6421_set_dma_mode,
};

static struct ata_port_operations vt6421_sata_ops = {
	.inherits		= &svia_base_ops,
	.scr_read		= svia_scr_read,
	.scr_write		= svia_scr_write,
};

static struct ata_port_operations vt8251_ops = {
	.inherits		= &svia_base_ops,
	.hardreset		= sata_std_hardreset,
	.scr_read		= vt8251_scr_read,
	.scr_write		= vt8251_scr_write,
};

static const struct ata_port_info vt6420_port_info = {
	.flags		= ATA_FLAG_SATA,
	.pio_mask	= ATA_PIO4,
	.mwdma_mask	= ATA_MWDMA2,
	.udma_mask	= ATA_UDMA6,
	.port_ops	= &vt6420_sata_ops,
};

static struct ata_port_info vt6421_sport_info = {
	.flags		= ATA_FLAG_SATA,
	.pio_mask	= ATA_PIO4,
	.mwdma_mask	= ATA_MWDMA2,
	.udma_mask	= ATA_UDMA6,
	.port_ops	= &vt6421_sata_ops,
};

static struct ata_port_info vt6421_pport_info = {
	.flags		= ATA_FLAG_SLAVE_POSS,
	.pio_mask	= ATA_PIO4,
	/* No MWDMA */
	.udma_mask	= ATA_UDMA6,
	.port_ops	= &vt6421_pata_ops,
};

static struct ata_port_info vt8251_port_info = {
	.flags		= ATA_FLAG_SATA | ATA_FLAG_SLAVE_POSS,
	.pio_mask	= ATA_PIO4,
	.mwdma_mask	= ATA_MWDMA2,
	.udma_mask	= ATA_UDMA6,
	.port_ops	= &vt8251_ops,
};

MODULE_AUTHOR("Jeff Garzik");
MODULE_DESCRIPTION("SCSI low-level driver for VIA SATA controllers");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, svia_pci_tbl);
MODULE_VERSION(DRV_VERSION);

static int svia_scr_read(struct ata_link *link, unsigned int sc_reg, u32 *val)
{
	if (sc_reg > SCR_CONTROL)
		return -EINVAL;
	*val = ioread32(link->ap->ioaddr.scr_addr + (4 * sc_reg));
	return 0;
}

static int svia_scr_write(struct ata_link *link, unsigned int sc_reg, u32 val)
{
	if (sc_reg > SCR_CONTROL)
		return -EINVAL;
	iowrite32(val, link->ap->ioaddr.scr_addr + (4 * sc_reg));
	return 0;
}

static int vt8251_scr_read(struct ata_link *link, unsigned int scr, u32 *val)
{
	static const u8 ipm_tbl[] = { 1, 2, 6, 0 };
	struct pci_dev *pdev = to_pci_dev(link->ap->host->dev);
	int slot = 2 * link->ap->port_no + link->pmp;
	u32 v = 0;
	u8 raw;

	switch (scr) {
	case SCR_STATUS:
		pci_read_config_byte(pdev, 0xA0 + slot, &raw);

		/* read the DET field, bit0 and 1 of the config byte */
		v |= raw & 0x03;

		/* read the SPD field, bit4 of the configure byte */
		if (raw & (1 << 4))
			v |= 0x02 << 4;
		else
			v |= 0x01 << 4;

		/* read the IPM field, bit2 and 3 of the config byte */
		v |= ipm_tbl[(raw >> 2) & 0x3];
		break;

	case SCR_ERROR:
		/* devices other than 5287 uses 0xA8 as base */
		WARN_ON(pdev->device != 0x5287);
		pci_read_config_dword(pdev, 0xB0 + slot * 4, &v);
		break;

	case SCR_CONTROL:
		pci_read_config_byte(pdev, 0xA4 + slot, &raw);

		/* read the DET field, bit0 and bit1 */
		v |= ((raw & 0x02) << 1) | (raw & 0x01);

		/* read the IPM field, bit2 and bit3 */
		v |= ((raw >> 2) & 0x03) << 8;
		break;

	default:
		return -EINVAL;
	}

	*val = v;
	return 0;
}

static int vt8251_scr_write(struct ata_link *link, unsigned int scr, u32 val)
{
	struct pci_dev *pdev = to_pci_dev(link->ap->host->dev);
	int slot = 2 * link->ap->port_no + link->pmp;
	u32 v = 0;

	switch (scr) {
	case SCR_ERROR:
		/* devices other than 5287 uses 0xA8 as base */
		WARN_ON(pdev->device != 0x5287);
		pci_write_config_dword(pdev, 0xB0 + slot * 4, val);
		return 0;

	case SCR_CONTROL:
		/* set the DET field */
		v |= ((val & 0x4) >> 1) | (val & 0x1);

		/* set the IPM field */
		v |= ((val >> 8) & 0x3) << 2;

		pci_write_config_byte(pdev, 0xA4 + slot, v);
		return 0;

	default:
		return -EINVAL;
	}
}

/**
 *	svia_tf_load - send taskfile registers to host controller
 *	@ap: Port to which output is sent
 *	@tf: ATA taskfile register set
 *
 *	Outputs ATA taskfile to standard ATA host controller.
 *
 *	This is to fix the internal bug of via chipsets, which will
 *	reset the device register after changing the IEN bit on ctl
 *	register.
 */
static void svia_tf_load(struct ata_port *ap, const struct ata_taskfile *tf)
{
	struct ata_taskfile ttf;

	if (tf->ctl != ap->last_ctl)  {
		ttf = *tf;
		ttf.flags |= ATA_TFLAG_DEVICE;
		tf = &ttf;
	}
	ata_sff_tf_load(ap, tf);
}

static void svia_noop_freeze(struct ata_port *ap)
{
	/* Some VIA controllers choke if ATA_NIEN is manipulated in
	 * certain way.  Leave it alone and just clear pending IRQ.
	 */
	ap->ops->sff_check_status(ap);
	ata_bmdma_irq_clear(ap);
}

/**
 *	vt6420_prereset - prereset for vt6420
 *	@link: target ATA link
 *	@deadline: deadline jiffies for the operation
 *
 *	SCR registers on vt6420 are pieces of shit and may hang the
 *	whole machine completely if accessed with the wrong timing.
 *	To avoid such catastrophe, vt6420 doesn't provide generic SCR
 *	access operations, but uses SStatus and SControl only during
 *	boot probing in controlled way.
 *
 *	As the old (pre EH update) probing code is proven to work, we
 *	strictly follow the access pattern.
 *
 *	LOCKING:
 *	Kernel thread context (may sleep)
 *
 *	RETURNS:
 *	0 on success, -errno otherwise.
 */
static int vt6420_prereset(struct ata_link *link, unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	struct ata_eh_context *ehc = &ap->link.eh_context;
	unsigned long timeout = jiffies + (HZ * 5);
	u32 sstatus, scontrol;
	int online;

	/* don't do any SCR stuff if we're not loading */
	if (!(ap->pflags & ATA_PFLAG_LOADING))
		goto skip_scr;

	/* Resume phy.  This is the old SATA resume sequence */
	svia_scr_write(link, SCR_CONTROL, 0x300);
	svia_scr_read(link, SCR_CONTROL, &scontrol); /* flush */

	/* wait for phy to become ready, if necessary */
	do {
		ata_msleep(link->ap, 200);
		svia_scr_read(link, SCR_STATUS, &sstatus);
		if ((sstatus & 0xf) != 1)
			break;
	} while (time_before(jiffies, timeout));

	/* open code sata_print_link_status() */
	svia_scr_read(link, SCR_STATUS, &sstatus);
	svia_scr_read(link, SCR_CONTROL, &scontrol);

	online = (sstatus & 0xf) == 0x3;

	ata_port_printk(ap, KERN_INFO,
			"SATA link %s 1.5 Gbps (SStatus %X SControl %X)\n",
			online ? "up" : "down", sstatus, scontrol);

	/* SStatus is read one more time */
	svia_scr_read(link, SCR_STATUS, &sstatus);

	if (!online) {
		/* tell EH to bail */
		ehc->i.action &= ~ATA_EH_RESET;
		return 0;
	}

 skip_scr:
	/* wait for !BSY */
	ata_sff_wait_ready(link, deadline);

	return 0;
}

static void vt6420_bmdma_start(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	if ((qc->tf.command == ATA_CMD_PACKET) &&
	    (qc->scsicmd->sc_data_direction == DMA_TO_DEVICE)) {
		/* Prevents corruption on some ATAPI burners */
		ata_sff_pause(ap);
	}
	ata_bmdma_start(qc);
}

static int vt6421_pata_cable_detect(struct ata_port *ap)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u8 tmp;

	pci_read_config_byte(pdev, PATA_UDMA_TIMING, &tmp);
	if (tmp & 0x10)
		return ATA_CBL_PATA40;
	return ATA_CBL_PATA80;
}

static void vt6421_set_pio_mode(struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	static const u8 pio_bits[] = { 0xA8, 0x65, 0x65, 0x31, 0x20 };
	pci_write_config_byte(pdev, PATA_PIO_TIMING - adev->devno,
			      pio_bits[adev->pio_mode - XFER_PIO_0]);
}

static void vt6421_set_dma_mode(struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	static const u8 udma_bits[] = { 0xEE, 0xE8, 0xE6, 0xE4, 0xE2, 0xE1, 0xE0, 0xE0 };
	pci_write_config_byte(pdev, PATA_UDMA_TIMING - adev->devno,
			      udma_bits[adev->dma_mode - XFER_UDMA_0]);
}

static const unsigned int svia_bar_sizes[] = {
	8, 4, 8, 4, 16, 256
};

static const unsigned int vt6421_bar_sizes[] = {
	16, 16, 16, 16, 32, 128
};

static void __iomem *svia_scr_addr(void __iomem *addr, unsigned int port)
{
	return addr + (port * 128);
}

static void __iomem *vt6421_scr_addr(void __iomem *addr, unsigned int port)
{
	return addr + (port * 64);
}

static void vt6421_init_addrs(struct ata_port *ap)
{
	void __iomem * const * iomap = ap->host->iomap;
	void __iomem *reg_addr = iomap[ap->port_no];
	void __iomem *bmdma_addr = iomap[4] + (ap->port_no * 8);
	struct ata_ioports *ioaddr = &ap->ioaddr;

	ioaddr->cmd_addr = reg_addr;
	ioaddr->altstatus_addr =
	ioaddr->ctl_addr = (void __iomem *)
		((unsigned long)(reg_addr + 8) | ATA_PCI_CTL_OFS);
	ioaddr->bmdma_addr = bmdma_addr;
	ioaddr->scr_addr = vt6421_scr_addr(iomap[5], ap->port_no);

	ata_sff_std_ports(ioaddr);

	ata_port_pbar_desc(ap, ap->port_no, -1, "port");
	ata_port_pbar_desc(ap, 4, ap->port_no * 8, "bmdma");
}

static int vt6420_prepare_host(struct pci_dev *pdev, struct ata_host **r_host)
{
	const struct ata_port_info *ppi[] = { &vt6420_port_info, NULL };
	struct ata_host *host;
	int rc;

	rc = ata_pci_bmdma_prepare_host(pdev, ppi, &host);
	if (rc)
		return rc;
	*r_host = host;

	rc = pcim_iomap_regions(pdev, 1 << 5, DRV_NAME);
	if (rc) {
		dev_printk(KERN_ERR, &pdev->dev, "failed to iomap PCI BAR 5\n");
		return rc;
	}

	host->ports[0]->ioaddr.scr_addr = svia_scr_addr(host->iomap[5], 0);
	host->ports[1]->ioaddr.scr_addr = svia_scr_addr(host->iomap[5], 1);

	return 0;
}

static int vt6421_prepare_host(struct pci_dev *pdev, struct ata_host **r_host)
{
	const struct ata_port_info *ppi[] =
		{ &vt6421_sport_info, &vt6421_sport_info, &vt6421_pport_info };
	struct ata_host *host;
	int i, rc;

	*r_host = host = ata_host_alloc_pinfo(&pdev->dev, ppi, ARRAY_SIZE(ppi));
	if (!host) {
		dev_printk(KERN_ERR, &pdev->dev, "failed to allocate host\n");
		return -ENOMEM;
	}

	rc = pcim_iomap_regions(pdev, 0x3f, DRV_NAME);
	if (rc) {
		dev_printk(KERN_ERR, &pdev->dev, "failed to request/iomap "
			   "PCI BARs (errno=%d)\n", rc);
		return rc;
	}
	host->iomap = pcim_iomap_table(pdev);

	for (i = 0; i < host->n_ports; i++)
		vt6421_init_addrs(host->ports[i]);

	rc = pci_set_dma_mask(pdev, ATA_DMA_MASK);
	if (rc)
		return rc;
	rc = pci_set_consistent_dma_mask(pdev, ATA_DMA_MASK);
	if (rc)
		return rc;

	return 0;
}

static int vt8251_prepare_host(struct pci_dev *pdev, struct ata_host **r_host)
{
	const struct ata_port_info *ppi[] = { &vt8251_port_info, NULL };
	struct ata_host *host;
	int i, rc;

	rc = ata_pci_bmdma_prepare_host(pdev, ppi, &host);
	if (rc)
		return rc;
	*r_host = host;

	rc = pcim_iomap_regions(pdev, 1 << 5, DRV_NAME);
	if (rc) {
		dev_printk(KERN_ERR, &pdev->dev, "failed to iomap PCI BAR 5\n");
		return rc;
	}

	/* 8251 hosts four sata ports as M/S of the two channels */
	for (i = 0; i < host->n_ports; i++)
		ata_slave_link_init(host->ports[i]);

	return 0;
}

static void svia_configure(struct pci_dev *pdev, int board_id)
{
	u8 tmp8;

	pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, &tmp8);
	dev_printk(KERN_INFO, &pdev->dev, "routed to hard irq line %d\n",
	       (int) (tmp8 & 0xf0) == 0xf0 ? 0 : tmp8 & 0x0f);

	/* make sure SATA channels are enabled */
	pci_read_config_byte(pdev, SATA_CHAN_ENAB, &tmp8);
	if ((tmp8 & ALL_PORTS) != ALL_PORTS) {
		dev_printk(KERN_DEBUG, &pdev->dev,
			   "enabling SATA channels (0x%x)\n",
			   (int) tmp8);
		tmp8 |= ALL_PORTS;
		pci_write_config_byte(pdev, SATA_CHAN_ENAB, tmp8);
	}

	/* make sure interrupts for each channel sent to us */
	pci_read_config_byte(pdev, SATA_INT_GATE, &tmp8);
	if ((tmp8 & ALL_PORTS) != ALL_PORTS) {
		dev_printk(KERN_DEBUG, &pdev->dev,
			   "enabling SATA channel interrupts (0x%x)\n",
			   (int) tmp8);
		tmp8 |= ALL_PORTS;
		pci_write_config_byte(pdev, SATA_INT_GATE, tmp8);
	}

	/* make sure native mode is enabled */
	pci_read_config_byte(pdev, SATA_NATIVE_MODE, &tmp8);
	if ((tmp8 & NATIVE_MODE_ALL) != NATIVE_MODE_ALL) {
		dev_printk(KERN_DEBUG, &pdev->dev,
			   "enabling SATA channel native mode (0x%x)\n",
			   (int) tmp8);
		tmp8 |= NATIVE_MODE_ALL;
		pci_write_config_byte(pdev, SATA_NATIVE_MODE, tmp8);
	}

	/*
	 * vt6420/1 has problems talking to some drives.  The following
	 * is the fix from Joseph Chan <JosephChan@via.com.tw>.
	 *
	 * When host issues HOLD, device may send up to 20DW of data
	 * before acknowledging it with HOLDA and the host should be
	 * able to buffer them in FIFO.  Unfortunately, some WD drives
	 * send up to 40DW before acknowledging HOLD and, in the
	 * default configuration, this ends up overflowing vt6421's
	 * FIFO, making the controller abort the transaction with
	 * R_ERR.
	 *
	 * Rx52[2] is the internal 128DW FIFO Flow control watermark
	 * adjusting mechanism enable bit and the default value 0
	 * means host will issue HOLD to device when the left FIFO
	 * size goes below 32DW.  Setting it to 1 makes the watermark
	 * 64DW.
	 *
	 * https://bugzilla.kernel.org/show_bug.cgi?id=15173
	 * http://article.gmane.org/gmane.linux.ide/46352
	 * http://thread.gmane.org/gmane.linux.kernel/1062139
	 */
	if (board_id == vt6420 || board_id == vt6421) {
		pci_read_config_byte(pdev, 0x52, &tmp8);
		tmp8 |= 1 << 2;
		pci_write_config_byte(pdev, 0x52, tmp8);
	}
}

static int svia_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	static int printed_version;
	unsigned int i;
	int rc;
	struct ata_host *host = NULL;
	int board_id = (int) ent->driver_data;
	const unsigned *bar_sizes;

	if (!printed_version++)
		dev_printk(KERN_DEBUG, &pdev->dev, "version " DRV_VERSION "\n");

	rc = pcim_enable_device(pdev);
	if (rc)
		return rc;

	if (board_id == vt6421)
		bar_sizes = &vt6421_bar_sizes[0];
	else
		bar_sizes = &svia_bar_sizes[0];

	for (i = 0; i < ARRAY_SIZE(svia_bar_sizes); i++)
		if ((pci_resource_start(pdev, i) == 0) ||
		    (pci_resource_len(pdev, i) < bar_sizes[i])) {
			dev_printk(KERN_ERR, &pdev->dev,
				"invalid PCI BAR %u (sz 0x%llx, val 0x%llx)\n",
				i,
				(unsigned long long)pci_resource_start(pdev, i),
				(unsigned long long)pci_resource_len(pdev, i));
			return -ENODEV;
		}

	switch (board_id) {
	case vt6420:
		rc = vt6420_prepare_host(pdev, &host);
		break;
	case vt6421:
		rc = vt6421_prepare_host(pdev, &host);
		break;
	case vt8251:
		rc = vt8251_prepare_host(pdev, &host);
		break;
	default:
		rc = -EINVAL;
	}
	if (rc)
		return rc;

	svia_configure(pdev, board_id);

	pci_set_master(pdev);
	return ata_host_activate(host, pdev->irq, ata_bmdma_interrupt,
				 IRQF_SHARED, &svia_sht);
}

static int __init svia_init(void)
{
	return pci_register_driver(&svia_pci_driver);
}

static void __exit svia_exit(void)
{
	pci_unregister_driver(&svia_pci_driver);
}

module_init(svia_init);
module_exit(svia_exit);
