/*
 *  ahci.c - AHCI SATA support
 *
 *  Maintained by:  Jeff Garzik <jgarzik@pobox.com>
 *    		    Please ALWAYS copy linux-ide@vger.kernel.org
 *		    on emails.
 *
 *  Copyright 2004-2005 Red Hat, Inc.
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
 * libata documentation is available via 'make {ps|pdf}docs',
 * as Documentation/DocBook/libata.*
 *
 * AHCI hardware documentation:
 * http://www.intel.com/technology/serialata/pdf/rev1_0.pdf
 * http://www.intel.com/technology/serialata/pdf/rev1_1.pdf
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/dmi.h>
#include <linux/gfp.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <linux/libata.h>
#include "ahci.h"

#define DRV_NAME	"ahci"
#define DRV_VERSION	"3.0"

enum {
	AHCI_PCI_BAR		= 5,
};

enum board_ids {
	/* board IDs by feature in alphabetical order */
	board_ahci,
	board_ahci_ign_iferr,
	board_ahci_nosntf,
	board_ahci_yes_fbs,

	/* board IDs for specific chipsets in alphabetical order */
	board_ahci_mcp65,
	board_ahci_mcp77,
	board_ahci_mcp89,
	board_ahci_mv,
	board_ahci_sb600,
	board_ahci_sb700,	/* for SB700 and SB800 */
	board_ahci_vt8251,

	/* aliases */
	board_ahci_mcp_linux	= board_ahci_mcp65,
	board_ahci_mcp67	= board_ahci_mcp65,
	board_ahci_mcp73	= board_ahci_mcp65,
	board_ahci_mcp79	= board_ahci_mcp77,
};

static int ahci_init_one(struct pci_dev *pdev, const struct pci_device_id *ent);
static int ahci_sb600_softreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline);
static int ahci_vt8251_hardreset(struct ata_link *link, unsigned int *class,
				 unsigned long deadline);
static int ahci_p5wdh_hardreset(struct ata_link *link, unsigned int *class,
				unsigned long deadline);
#ifdef CONFIG_PM
static int ahci_pci_device_suspend(struct pci_dev *pdev, pm_message_t mesg);
static int ahci_pci_device_resume(struct pci_dev *pdev);
#endif

static struct scsi_host_template ahci_sht = {
	AHCI_SHT("ahci"),
};

static struct ata_port_operations ahci_vt8251_ops = {
	.inherits		= &ahci_ops,
	.hardreset		= ahci_vt8251_hardreset,
};

static struct ata_port_operations ahci_p5wdh_ops = {
	.inherits		= &ahci_ops,
	.hardreset		= ahci_p5wdh_hardreset,
};

static struct ata_port_operations ahci_sb600_ops = {
	.inherits		= &ahci_ops,
	.softreset		= ahci_sb600_softreset,
	.pmp_softreset		= ahci_sb600_softreset,
};

#define AHCI_HFLAGS(flags)	.private_data	= (void *)(flags)

static const struct ata_port_info ahci_port_info[] = {
	/* by features */
	[board_ahci] =
	{
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_ign_iferr] =
	{
		AHCI_HFLAGS	(AHCI_HFLAG_IGN_IRQ_IF_ERR),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_nosntf] =
	{
		AHCI_HFLAGS	(AHCI_HFLAG_NO_SNTF),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_yes_fbs] =
	{
		AHCI_HFLAGS	(AHCI_HFLAG_YES_FBS),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	/* by chipsets */
	[board_ahci_mcp65] =
	{
		AHCI_HFLAGS	(AHCI_HFLAG_NO_FPDMA_AA | AHCI_HFLAG_NO_PMP |
				 AHCI_HFLAG_YES_NCQ),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_mcp77] =
	{
		AHCI_HFLAGS	(AHCI_HFLAG_NO_FPDMA_AA | AHCI_HFLAG_NO_PMP),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_mcp89] =
	{
		AHCI_HFLAGS	(AHCI_HFLAG_NO_FPDMA_AA),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_mv] =
	{
		AHCI_HFLAGS	(AHCI_HFLAG_NO_NCQ | AHCI_HFLAG_NO_MSI |
				 AHCI_HFLAG_MV_PATA | AHCI_HFLAG_NO_PMP),
		.flags		= ATA_FLAG_SATA | ATA_FLAG_NO_LEGACY |
				  ATA_FLAG_MMIO | ATA_FLAG_PIO_DMA,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_sb600] =
	{
		AHCI_HFLAGS	(AHCI_HFLAG_IGN_SERR_INTERNAL |
				 AHCI_HFLAG_NO_MSI | AHCI_HFLAG_SECT255 |
				 AHCI_HFLAG_32BIT_ONLY),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_sb600_ops,
	},
	[board_ahci_sb700] =	/* for SB700 and SB800 */
	{
		AHCI_HFLAGS	(AHCI_HFLAG_IGN_SERR_INTERNAL),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_sb600_ops,
	},
	[board_ahci_vt8251] =
	{
		AHCI_HFLAGS	(AHCI_HFLAG_NO_NCQ | AHCI_HFLAG_NO_PMP),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_vt8251_ops,
	},
};

static const struct pci_device_id ahci_pci_tbl[] = {
	/* Intel */
	{ PCI_VDEVICE(INTEL, 0x2652), board_ahci }, /* ICH6 */
	{ PCI_VDEVICE(INTEL, 0x2653), board_ahci }, /* ICH6M */
	{ PCI_VDEVICE(INTEL, 0x27c1), board_ahci }, /* ICH7 */
	{ PCI_VDEVICE(INTEL, 0x27c5), board_ahci }, /* ICH7M */
	{ PCI_VDEVICE(INTEL, 0x27c3), board_ahci }, /* ICH7R */
	{ PCI_VDEVICE(AL, 0x5288), board_ahci_ign_iferr }, /* ULi M5288 */
	{ PCI_VDEVICE(INTEL, 0x2681), board_ahci }, /* ESB2 */
	{ PCI_VDEVICE(INTEL, 0x2682), board_ahci }, /* ESB2 */
	{ PCI_VDEVICE(INTEL, 0x2683), board_ahci }, /* ESB2 */
	{ PCI_VDEVICE(INTEL, 0x27c6), board_ahci }, /* ICH7-M DH */
	{ PCI_VDEVICE(INTEL, 0x2821), board_ahci }, /* ICH8 */
	{ PCI_VDEVICE(INTEL, 0x2822), board_ahci_nosntf }, /* ICH8 */
	{ PCI_VDEVICE(INTEL, 0x2824), board_ahci }, /* ICH8 */
	{ PCI_VDEVICE(INTEL, 0x2829), board_ahci }, /* ICH8M */
	{ PCI_VDEVICE(INTEL, 0x282a), board_ahci }, /* ICH8M */
	{ PCI_VDEVICE(INTEL, 0x2922), board_ahci }, /* ICH9 */
	{ PCI_VDEVICE(INTEL, 0x2923), board_ahci }, /* ICH9 */
	{ PCI_VDEVICE(INTEL, 0x2924), board_ahci }, /* ICH9 */
	{ PCI_VDEVICE(INTEL, 0x2925), board_ahci }, /* ICH9 */
	{ PCI_VDEVICE(INTEL, 0x2927), board_ahci }, /* ICH9 */
	{ PCI_VDEVICE(INTEL, 0x2929), board_ahci }, /* ICH9M */
	{ PCI_VDEVICE(INTEL, 0x292a), board_ahci }, /* ICH9M */
	{ PCI_VDEVICE(INTEL, 0x292b), board_ahci }, /* ICH9M */
	{ PCI_VDEVICE(INTEL, 0x292c), board_ahci }, /* ICH9M */
	{ PCI_VDEVICE(INTEL, 0x292f), board_ahci }, /* ICH9M */
	{ PCI_VDEVICE(INTEL, 0x294d), board_ahci }, /* ICH9 */
	{ PCI_VDEVICE(INTEL, 0x294e), board_ahci }, /* ICH9M */
	{ PCI_VDEVICE(INTEL, 0x502a), board_ahci }, /* Tolapai */
	{ PCI_VDEVICE(INTEL, 0x502b), board_ahci }, /* Tolapai */
	{ PCI_VDEVICE(INTEL, 0x3a05), board_ahci }, /* ICH10 */
	{ PCI_VDEVICE(INTEL, 0x3a22), board_ahci }, /* ICH10 */
	{ PCI_VDEVICE(INTEL, 0x3a25), board_ahci }, /* ICH10 */
	{ PCI_VDEVICE(INTEL, 0x3b22), board_ahci }, /* PCH AHCI */
	{ PCI_VDEVICE(INTEL, 0x3b23), board_ahci }, /* PCH AHCI */
	{ PCI_VDEVICE(INTEL, 0x3b24), board_ahci }, /* PCH RAID */
	{ PCI_VDEVICE(INTEL, 0x3b25), board_ahci }, /* PCH RAID */
	{ PCI_VDEVICE(INTEL, 0x3b29), board_ahci }, /* PCH AHCI */
	{ PCI_VDEVICE(INTEL, 0x3b2b), board_ahci }, /* PCH RAID */
	{ PCI_VDEVICE(INTEL, 0x3b2c), board_ahci }, /* PCH RAID */
	{ PCI_VDEVICE(INTEL, 0x3b2f), board_ahci }, /* PCH AHCI */
	{ PCI_VDEVICE(INTEL, 0x1c02), board_ahci }, /* CPT AHCI */
	{ PCI_VDEVICE(INTEL, 0x1c03), board_ahci }, /* CPT AHCI */
	{ PCI_VDEVICE(INTEL, 0x1c04), board_ahci }, /* CPT RAID */
	{ PCI_VDEVICE(INTEL, 0x1c05), board_ahci }, /* CPT RAID */
	{ PCI_VDEVICE(INTEL, 0x1c06), board_ahci }, /* CPT RAID */
	{ PCI_VDEVICE(INTEL, 0x1c07), board_ahci }, /* CPT RAID */
	{ PCI_VDEVICE(INTEL, 0x1d02), board_ahci }, /* PBG AHCI */
	{ PCI_VDEVICE(INTEL, 0x1d04), board_ahci }, /* PBG RAID */
	{ PCI_VDEVICE(INTEL, 0x1d06), board_ahci }, /* PBG RAID */

	/* JMicron 360/1/3/5/6, match class to avoid IDE function */
	{ PCI_VENDOR_ID_JMICRON, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID,
	  PCI_CLASS_STORAGE_SATA_AHCI, 0xffffff, board_ahci_ign_iferr },

	/* ATI */
	{ PCI_VDEVICE(ATI, 0x4380), board_ahci_sb600 }, /* ATI SB600 */
	{ PCI_VDEVICE(ATI, 0x4390), board_ahci_sb700 }, /* ATI SB700/800 */
	{ PCI_VDEVICE(ATI, 0x4391), board_ahci_sb700 }, /* ATI SB700/800 */
	{ PCI_VDEVICE(ATI, 0x4392), board_ahci_sb700 }, /* ATI SB700/800 */
	{ PCI_VDEVICE(ATI, 0x4393), board_ahci_sb700 }, /* ATI SB700/800 */
	{ PCI_VDEVICE(ATI, 0x4394), board_ahci_sb700 }, /* ATI SB700/800 */
	{ PCI_VDEVICE(ATI, 0x4395), board_ahci_sb700 }, /* ATI SB700/800 */

	/* AMD */
	{ PCI_VDEVICE(AMD, 0x7800), board_ahci }, /* AMD Hudson-2 */
	/* AMD is using RAID class only for ahci controllers */
	{ PCI_VENDOR_ID_AMD, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID,
	  PCI_CLASS_STORAGE_RAID << 8, 0xffffff, board_ahci },

	/* VIA */
	{ PCI_VDEVICE(VIA, 0x3349), board_ahci_vt8251 }, /* VIA VT8251 */
	{ PCI_VDEVICE(VIA, 0x6287), board_ahci_vt8251 }, /* VIA VT8251 */

	/* NVIDIA */
	{ PCI_VDEVICE(NVIDIA, 0x044c), board_ahci_mcp65 },	/* MCP65 */
	{ PCI_VDEVICE(NVIDIA, 0x044d), board_ahci_mcp65 },	/* MCP65 */
	{ PCI_VDEVICE(NVIDIA, 0x044e), board_ahci_mcp65 },	/* MCP65 */
	{ PCI_VDEVICE(NVIDIA, 0x044f), board_ahci_mcp65 },	/* MCP65 */
	{ PCI_VDEVICE(NVIDIA, 0x045c), board_ahci_mcp65 },	/* MCP65 */
	{ PCI_VDEVICE(NVIDIA, 0x045d), board_ahci_mcp65 },	/* MCP65 */
	{ PCI_VDEVICE(NVIDIA, 0x045e), board_ahci_mcp65 },	/* MCP65 */
	{ PCI_VDEVICE(NVIDIA, 0x045f), board_ahci_mcp65 },	/* MCP65 */
	{ PCI_VDEVICE(NVIDIA, 0x0550), board_ahci_mcp67 },	/* MCP67 */
	{ PCI_VDEVICE(NVIDIA, 0x0551), board_ahci_mcp67 },	/* MCP67 */
	{ PCI_VDEVICE(NVIDIA, 0x0552), board_ahci_mcp67 },	/* MCP67 */
	{ PCI_VDEVICE(NVIDIA, 0x0553), board_ahci_mcp67 },	/* MCP67 */
	{ PCI_VDEVICE(NVIDIA, 0x0554), board_ahci_mcp67 },	/* MCP67 */
	{ PCI_VDEVICE(NVIDIA, 0x0555), board_ahci_mcp67 },	/* MCP67 */
	{ PCI_VDEVICE(NVIDIA, 0x0556), board_ahci_mcp67 },	/* MCP67 */
	{ PCI_VDEVICE(NVIDIA, 0x0557), board_ahci_mcp67 },	/* MCP67 */
	{ PCI_VDEVICE(NVIDIA, 0x0558), board_ahci_mcp67 },	/* MCP67 */
	{ PCI_VDEVICE(NVIDIA, 0x0559), board_ahci_mcp67 },	/* MCP67 */
	{ PCI_VDEVICE(NVIDIA, 0x055a), board_ahci_mcp67 },	/* MCP67 */
	{ PCI_VDEVICE(NVIDIA, 0x055b), board_ahci_mcp67 },	/* MCP67 */
	{ PCI_VDEVICE(NVIDIA, 0x0580), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x0581), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x0582), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x0583), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x0584), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x0585), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x0586), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x0587), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x0588), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x0589), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x058a), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x058b), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x058c), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x058d), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x058e), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x058f), board_ahci_mcp_linux },	/* Linux ID */
	{ PCI_VDEVICE(NVIDIA, 0x07f0), board_ahci_mcp73 },	/* MCP73 */
	{ PCI_VDEVICE(NVIDIA, 0x07f1), board_ahci_mcp73 },	/* MCP73 */
	{ PCI_VDEVICE(NVIDIA, 0x07f2), board_ahci_mcp73 },	/* MCP73 */
	{ PCI_VDEVICE(NVIDIA, 0x07f3), board_ahci_mcp73 },	/* MCP73 */
	{ PCI_VDEVICE(NVIDIA, 0x07f4), board_ahci_mcp73 },	/* MCP73 */
	{ PCI_VDEVICE(NVIDIA, 0x07f5), board_ahci_mcp73 },	/* MCP73 */
	{ PCI_VDEVICE(NVIDIA, 0x07f6), board_ahci_mcp73 },	/* MCP73 */
	{ PCI_VDEVICE(NVIDIA, 0x07f7), board_ahci_mcp73 },	/* MCP73 */
	{ PCI_VDEVICE(NVIDIA, 0x07f8), board_ahci_mcp73 },	/* MCP73 */
	{ PCI_VDEVICE(NVIDIA, 0x07f9), board_ahci_mcp73 },	/* MCP73 */
	{ PCI_VDEVICE(NVIDIA, 0x07fa), board_ahci_mcp73 },	/* MCP73 */
	{ PCI_VDEVICE(NVIDIA, 0x07fb), board_ahci_mcp73 },	/* MCP73 */
	{ PCI_VDEVICE(NVIDIA, 0x0ad0), board_ahci_mcp77 },	/* MCP77 */
	{ PCI_VDEVICE(NVIDIA, 0x0ad1), board_ahci_mcp77 },	/* MCP77 */
	{ PCI_VDEVICE(NVIDIA, 0x0ad2), board_ahci_mcp77 },	/* MCP77 */
	{ PCI_VDEVICE(NVIDIA, 0x0ad3), board_ahci_mcp77 },	/* MCP77 */
	{ PCI_VDEVICE(NVIDIA, 0x0ad4), board_ahci_mcp77 },	/* MCP77 */
	{ PCI_VDEVICE(NVIDIA, 0x0ad5), board_ahci_mcp77 },	/* MCP77 */
	{ PCI_VDEVICE(NVIDIA, 0x0ad6), board_ahci_mcp77 },	/* MCP77 */
	{ PCI_VDEVICE(NVIDIA, 0x0ad7), board_ahci_mcp77 },	/* MCP77 */
	{ PCI_VDEVICE(NVIDIA, 0x0ad8), board_ahci_mcp77 },	/* MCP77 */
	{ PCI_VDEVICE(NVIDIA, 0x0ad9), board_ahci_mcp77 },	/* MCP77 */
	{ PCI_VDEVICE(NVIDIA, 0x0ada), board_ahci_mcp77 },	/* MCP77 */
	{ PCI_VDEVICE(NVIDIA, 0x0adb), board_ahci_mcp77 },	/* MCP77 */
	{ PCI_VDEVICE(NVIDIA, 0x0ab4), board_ahci_mcp79 },	/* MCP79 */
	{ PCI_VDEVICE(NVIDIA, 0x0ab5), board_ahci_mcp79 },	/* MCP79 */
	{ PCI_VDEVICE(NVIDIA, 0x0ab6), board_ahci_mcp79 },	/* MCP79 */
	{ PCI_VDEVICE(NVIDIA, 0x0ab7), board_ahci_mcp79 },	/* MCP79 */
	{ PCI_VDEVICE(NVIDIA, 0x0ab8), board_ahci_mcp79 },	/* MCP79 */
	{ PCI_VDEVICE(NVIDIA, 0x0ab9), board_ahci_mcp79 },	/* MCP79 */
	{ PCI_VDEVICE(NVIDIA, 0x0aba), board_ahci_mcp79 },	/* MCP79 */
	{ PCI_VDEVICE(NVIDIA, 0x0abb), board_ahci_mcp79 },	/* MCP79 */
	{ PCI_VDEVICE(NVIDIA, 0x0abc), board_ahci_mcp79 },	/* MCP79 */
	{ PCI_VDEVICE(NVIDIA, 0x0abd), board_ahci_mcp79 },	/* MCP79 */
	{ PCI_VDEVICE(NVIDIA, 0x0abe), board_ahci_mcp79 },	/* MCP79 */
	{ PCI_VDEVICE(NVIDIA, 0x0abf), board_ahci_mcp79 },	/* MCP79 */
	{ PCI_VDEVICE(NVIDIA, 0x0d84), board_ahci_mcp89 },	/* MCP89 */
	{ PCI_VDEVICE(NVIDIA, 0x0d85), board_ahci_mcp89 },	/* MCP89 */
	{ PCI_VDEVICE(NVIDIA, 0x0d86), board_ahci_mcp89 },	/* MCP89 */
	{ PCI_VDEVICE(NVIDIA, 0x0d87), board_ahci_mcp89 },	/* MCP89 */
	{ PCI_VDEVICE(NVIDIA, 0x0d88), board_ahci_mcp89 },	/* MCP89 */
	{ PCI_VDEVICE(NVIDIA, 0x0d89), board_ahci_mcp89 },	/* MCP89 */
	{ PCI_VDEVICE(NVIDIA, 0x0d8a), board_ahci_mcp89 },	/* MCP89 */
	{ PCI_VDEVICE(NVIDIA, 0x0d8b), board_ahci_mcp89 },	/* MCP89 */
	{ PCI_VDEVICE(NVIDIA, 0x0d8c), board_ahci_mcp89 },	/* MCP89 */
	{ PCI_VDEVICE(NVIDIA, 0x0d8d), board_ahci_mcp89 },	/* MCP89 */
	{ PCI_VDEVICE(NVIDIA, 0x0d8e), board_ahci_mcp89 },	/* MCP89 */
	{ PCI_VDEVICE(NVIDIA, 0x0d8f), board_ahci_mcp89 },	/* MCP89 */

	/* SiS */
	{ PCI_VDEVICE(SI, 0x1184), board_ahci },		/* SiS 966 */
	{ PCI_VDEVICE(SI, 0x1185), board_ahci },		/* SiS 968 */
	{ PCI_VDEVICE(SI, 0x0186), board_ahci },		/* SiS 968 */

	/* Marvell */
	{ PCI_VDEVICE(MARVELL, 0x6145), board_ahci_mv },	/* 6145 */
	{ PCI_VDEVICE(MARVELL, 0x6121), board_ahci_mv },	/* 6121 */
	{ PCI_DEVICE(0x1b4b, 0x9123),
	  .driver_data = board_ahci_yes_fbs },			/* 88se9128 */

	/* Promise */
	{ PCI_VDEVICE(PROMISE, 0x3f20), board_ahci },	/* PDC42819 */

	/* ASMedia */
	{ PCI_VDEVICE(ASMEDIA, 0x0601), board_ahci },
	{ PCI_VDEVICE(ASMEDIA, 0x0602), board_ahci },
	{ PCI_VDEVICE(ASMEDIA, 0x0611), board_ahci },
	{ PCI_VDEVICE(ASMEDIA, 0x0612), board_ahci },

	/* Generic, PCI class code for AHCI */
	{ PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID,
	  PCI_CLASS_STORAGE_SATA_AHCI, 0xffffff, board_ahci },

	{ }	/* terminate list */
};


static struct pci_driver ahci_pci_driver = {
	.name			= DRV_NAME,
	.id_table		= ahci_pci_tbl,
	.probe			= ahci_init_one,
	.remove			= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend		= ahci_pci_device_suspend,
	.resume			= ahci_pci_device_resume,
#endif
};

#if defined(CONFIG_PATA_MARVELL) || defined(CONFIG_PATA_MARVELL_MODULE)
static int marvell_enable;
#else
static int marvell_enable = 1;
#endif
module_param(marvell_enable, int, 0644);
MODULE_PARM_DESC(marvell_enable, "Marvell SATA via AHCI (1 = enabled)");


static void ahci_pci_save_initial_config(struct pci_dev *pdev,
					 struct ahci_host_priv *hpriv)
{
	unsigned int force_port_map = 0;
	unsigned int mask_port_map = 0;

	if (pdev->vendor == PCI_VENDOR_ID_JMICRON && pdev->device == 0x2361) {
		dev_info(&pdev->dev, "JMB361 has only one port\n");
		force_port_map = 1;
	}

	/*
	 * Temporary Marvell 6145 hack: PATA port presence
	 * is asserted through the standard AHCI port
	 * presence register, as bit 4 (counting from 0)
	 */
	if (hpriv->flags & AHCI_HFLAG_MV_PATA) {
		if (pdev->device == 0x6121)
			mask_port_map = 0x3;
		else
			mask_port_map = 0xf;
		dev_info(&pdev->dev,
			  "Disabling your PATA port. Use the boot option 'ahci.marvell_enable=0' to avoid this.\n");
	}

	ahci_save_initial_config(&pdev->dev, hpriv, force_port_map,
				 mask_port_map);
}

static int ahci_pci_reset_controller(struct ata_host *host)
{
	struct pci_dev *pdev = to_pci_dev(host->dev);

	ahci_reset_controller(host);

	if (pdev->vendor == PCI_VENDOR_ID_INTEL) {
		struct ahci_host_priv *hpriv = host->private_data;
		u16 tmp16;

		/* configure PCS */
		pci_read_config_word(pdev, 0x92, &tmp16);
		if ((tmp16 & hpriv->port_map) != hpriv->port_map) {
			tmp16 |= hpriv->port_map;
			pci_write_config_word(pdev, 0x92, tmp16);
		}
	}

	return 0;
}

static void ahci_pci_init_controller(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;
	struct pci_dev *pdev = to_pci_dev(host->dev);
	void __iomem *port_mmio;
	u32 tmp;
	int mv;

	if (hpriv->flags & AHCI_HFLAG_MV_PATA) {
		if (pdev->device == 0x6121)
			mv = 2;
		else
			mv = 4;
		port_mmio = __ahci_port_base(host, mv);

		writel(0, port_mmio + PORT_IRQ_MASK);

		/* clear port IRQ */
		tmp = readl(port_mmio + PORT_IRQ_STAT);
		VPRINTK("PORT_IRQ_STAT 0x%x\n", tmp);
		if (tmp)
			writel(tmp, port_mmio + PORT_IRQ_STAT);
	}

	ahci_init_controller(host);
}

static int ahci_sb600_check_ready(struct ata_link *link)
{
	void __iomem *port_mmio = ahci_port_base(link->ap);
	u8 status = readl(port_mmio + PORT_TFDATA) & 0xFF;
	u32 irq_status = readl(port_mmio + PORT_IRQ_STAT);

	/*
	 * There is no need to check TFDATA if BAD PMP is found due to HW bug,
	 * which can save timeout delay.
	 */
	if (irq_status & PORT_IRQ_BAD_PMP)
		return -EIO;

	return ata_check_ready(status);
}

static int ahci_sb600_softreset(struct ata_link *link, unsigned int *class,
				unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	void __iomem *port_mmio = ahci_port_base(ap);
	int pmp = sata_srst_pmp(link);
	int rc;
	u32 irq_sts;

	DPRINTK("ENTER\n");

	rc = ahci_do_softreset(link, class, pmp, deadline,
			       ahci_sb600_check_ready);

	/*
	 * Soft reset fails on some ATI chips with IPMS set when PMP
	 * is enabled but SATA HDD/ODD is connected to SATA port,
	 * do soft reset again to port 0.
	 */
	if (rc == -EIO) {
		irq_sts = readl(port_mmio + PORT_IRQ_STAT);
		if (irq_sts & PORT_IRQ_BAD_PMP) {
			ata_link_printk(link, KERN_WARNING,
					"applying SB600 PMP SRST workaround "
					"and retrying\n");
			rc = ahci_do_softreset(link, class, 0, deadline,
					       ahci_check_ready);
		}
	}

	return rc;
}

static int ahci_vt8251_hardreset(struct ata_link *link, unsigned int *class,
				 unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	bool online;
	int rc;

	DPRINTK("ENTER\n");

	ahci_stop_engine(ap);

	rc = sata_link_hardreset(link, sata_ehc_deb_timing(&link->eh_context),
				 deadline, &online, NULL);

	ahci_start_engine(ap);

	DPRINTK("EXIT, rc=%d, class=%u\n", rc, *class);

	/* vt8251 doesn't clear BSY on signature FIS reception,
	 * request follow-up softreset.
	 */
	return online ? -EAGAIN : rc;
}

static int ahci_p5wdh_hardreset(struct ata_link *link, unsigned int *class,
				unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	u8 *d2h_fis = pp->rx_fis + RX_FIS_D2H_REG;
	struct ata_taskfile tf;
	bool online;
	int rc;

	ahci_stop_engine(ap);

	/* clear D2H reception area to properly wait for D2H FIS */
	ata_tf_init(link->device, &tf);
	tf.command = 0x80;
	ata_tf_to_fis(&tf, 0, 0, d2h_fis);

	rc = sata_link_hardreset(link, sata_ehc_deb_timing(&link->eh_context),
				 deadline, &online, NULL);

	ahci_start_engine(ap);

	if (online) {
		rc = ata_wait_after_reset(link, jiffies + 2 * HZ,
					  ahci_check_ready);
		if (rc)
			ahci_kick_engine(ap);
	}
	return rc;
}

#ifdef CONFIG_PM
static int ahci_pci_device_suspend(struct pci_dev *pdev, pm_message_t mesg)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = hpriv->mmio;
	u32 ctl;

	if (mesg.event & PM_EVENT_SUSPEND &&
	    hpriv->flags & AHCI_HFLAG_NO_SUSPEND) {
		dev_printk(KERN_ERR, &pdev->dev,
			   "BIOS update required for suspend/resume\n");
		return -EIO;
	}

	if (mesg.event & PM_EVENT_SLEEP) {
		/* AHCI spec rev1.1 section 8.3.3:
		 * Software must disable interrupts prior to requesting a
		 * transition of the HBA to D3 state.
		 */
		ctl = readl(mmio + HOST_CTL);
		ctl &= ~HOST_IRQ_EN;
		writel(ctl, mmio + HOST_CTL);
		readl(mmio + HOST_CTL); /* flush */
	}

	return ata_pci_device_suspend(pdev, mesg);
}

static int ahci_pci_device_resume(struct pci_dev *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	int rc;

	rc = ata_pci_device_do_resume(pdev);
	if (rc)
		return rc;

	if (pdev->dev.power.power_state.event == PM_EVENT_SUSPEND) {
		rc = ahci_pci_reset_controller(host);
		if (rc)
			return rc;

		ahci_pci_init_controller(host);
	}

	ata_host_resume(host);

	return 0;
}
#endif

static int ahci_configure_dma_masks(struct pci_dev *pdev, int using_dac)
{
	int rc;

	if (using_dac &&
	    !pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) {
		rc = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
		if (rc) {
			rc = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
			if (rc) {
				dev_printk(KERN_ERR, &pdev->dev,
					   "64-bit DMA enable failed\n");
				return rc;
			}
		}
	} else {
		rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (rc) {
			dev_printk(KERN_ERR, &pdev->dev,
				   "32-bit DMA enable failed\n");
			return rc;
		}
		rc = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		if (rc) {
			dev_printk(KERN_ERR, &pdev->dev,
				   "32-bit consistent DMA enable failed\n");
			return rc;
		}
	}
	return 0;
}

static void ahci_pci_print_info(struct ata_host *host)
{
	struct pci_dev *pdev = to_pci_dev(host->dev);
	u16 cc;
	const char *scc_s;

	pci_read_config_word(pdev, 0x0a, &cc);
	if (cc == PCI_CLASS_STORAGE_IDE)
		scc_s = "IDE";
	else if (cc == PCI_CLASS_STORAGE_SATA)
		scc_s = "SATA";
	else if (cc == PCI_CLASS_STORAGE_RAID)
		scc_s = "RAID";
	else
		scc_s = "unknown";

	ahci_print_info(host, scc_s);
}

/* On ASUS P5W DH Deluxe, the second port of PCI device 00:1f.2 is
 * hardwired to on-board SIMG 4726.  The chipset is ICH8 and doesn't
 * support PMP and the 4726 either directly exports the device
 * attached to the first downstream port or acts as a hardware storage
 * controller and emulate a single ATA device (can be RAID 0/1 or some
 * other configuration).
 *
 * When there's no device attached to the first downstream port of the
 * 4726, "Config Disk" appears, which is a pseudo ATA device to
 * configure the 4726.  However, ATA emulation of the device is very
 * lame.  It doesn't send signature D2H Reg FIS after the initial
 * hardreset, pukes on SRST w/ PMP==0 and has bunch of other issues.
 *
 * The following function works around the problem by always using
 * hardreset on the port and not depending on receiving signature FIS
 * afterward.  If signature FIS isn't received soon, ATA class is
 * assumed without follow-up softreset.
 */
static void ahci_p5wdh_workaround(struct ata_host *host)
{
	static struct dmi_system_id sysids[] = {
		{
			.ident = "P5W DH Deluxe",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR,
					  "ASUSTEK COMPUTER INC"),
				DMI_MATCH(DMI_PRODUCT_NAME, "P5W DH Deluxe"),
			},
		},
		{ }
	};
	struct pci_dev *pdev = to_pci_dev(host->dev);

	if (pdev->bus->number == 0 && pdev->devfn == PCI_DEVFN(0x1f, 2) &&
	    dmi_check_system(sysids)) {
		struct ata_port *ap = host->ports[1];

		dev_printk(KERN_INFO, &pdev->dev, "enabling ASUS P5W DH "
			   "Deluxe on-board SIMG4726 workaround\n");

		ap->ops = &ahci_p5wdh_ops;
		ap->link.flags |= ATA_LFLAG_NO_SRST | ATA_LFLAG_ASSUME_ATA;
	}
}

/* only some SB600 ahci controllers can do 64bit DMA */
static bool ahci_sb600_enable_64bit(struct pci_dev *pdev)
{
	static const struct dmi_system_id sysids[] = {
		/*
		 * The oldest version known to be broken is 0901 and
		 * working is 1501 which was released on 2007-10-26.
		 * Enable 64bit DMA on 1501 and anything newer.
		 *
		 * Please read bko#9412 for more info.
		 */
		{
			.ident = "ASUS M2A-VM",
			.matches = {
				DMI_MATCH(DMI_BOARD_VENDOR,
					  "ASUSTeK Computer INC."),
				DMI_MATCH(DMI_BOARD_NAME, "M2A-VM"),
			},
			.driver_data = "20071026",	/* yyyymmdd */
		},
		/*
		 * All BIOS versions for the MSI K9A2 Platinum (MS-7376)
		 * support 64bit DMA.
		 *
		 * BIOS versions earlier than 1.5 had the Manufacturer DMI
		 * fields as "MICRO-STAR INTERANTIONAL CO.,LTD".
		 * This spelling mistake was fixed in BIOS version 1.5, so
		 * 1.5 and later have the Manufacturer as
		 * "MICRO-STAR INTERNATIONAL CO.,LTD".
		 * So try to match on DMI_BOARD_VENDOR of "MICRO-STAR INTER".
		 *
		 * BIOS versions earlier than 1.9 had a Board Product Name
		 * DMI field of "MS-7376". This was changed to be
		 * "K9A2 Platinum (MS-7376)" in version 1.9, but we can still
		 * match on DMI_BOARD_NAME of "MS-7376".
		 */
		{
			.ident = "MSI K9A2 Platinum",
			.matches = {
				DMI_MATCH(DMI_BOARD_VENDOR,
					  "MICRO-STAR INTER"),
				DMI_MATCH(DMI_BOARD_NAME, "MS-7376"),
			},
		},
		{ }
	};
	const struct dmi_system_id *match;
	int year, month, date;
	char buf[9];

	match = dmi_first_match(sysids);
	if (pdev->bus->number != 0 || pdev->devfn != PCI_DEVFN(0x12, 0) ||
	    !match)
		return false;

	if (!match->driver_data)
		goto enable_64bit;

	dmi_get_date(DMI_BIOS_DATE, &year, &month, &date);
	snprintf(buf, sizeof(buf), "%04d%02d%02d", year, month, date);

	if (strcmp(buf, match->driver_data) >= 0)
		goto enable_64bit;
	else {
		dev_printk(KERN_WARNING, &pdev->dev, "%s: BIOS too old, "
			   "forcing 32bit DMA, update BIOS\n", match->ident);
		return false;
	}

enable_64bit:
	dev_printk(KERN_WARNING, &pdev->dev, "%s: enabling 64bit DMA\n",
		   match->ident);
	return true;
}

static bool ahci_broken_system_poweroff(struct pci_dev *pdev)
{
	static const struct dmi_system_id broken_systems[] = {
		{
			.ident = "HP Compaq nx6310",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
				DMI_MATCH(DMI_PRODUCT_NAME, "HP Compaq nx6310"),
			},
			/* PCI slot number of the controller */
			.driver_data = (void *)0x1FUL,
		},
		{
			.ident = "HP Compaq 6720s",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
				DMI_MATCH(DMI_PRODUCT_NAME, "HP Compaq 6720s"),
			},
			/* PCI slot number of the controller */
			.driver_data = (void *)0x1FUL,
		},

		{ }	/* terminate list */
	};
	const struct dmi_system_id *dmi = dmi_first_match(broken_systems);

	if (dmi) {
		unsigned long slot = (unsigned long)dmi->driver_data;
		/* apply the quirk only to on-board controllers */
		return slot == PCI_SLOT(pdev->devfn);
	}

	return false;
}

static bool ahci_broken_suspend(struct pci_dev *pdev)
{
	static const struct dmi_system_id sysids[] = {
		/*
		 * On HP dv[4-6] and HDX18 with earlier BIOSen, link
		 * to the harddisk doesn't become online after
		 * resuming from STR.  Warn and fail suspend.
		 *
		 * http://bugzilla.kernel.org/show_bug.cgi?id=12276
		 *
		 * Use dates instead of versions to match as HP is
		 * apparently recycling both product and version
		 * strings.
		 *
		 * http://bugzilla.kernel.org/show_bug.cgi?id=15462
		 */
		{
			.ident = "dv4",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
				DMI_MATCH(DMI_PRODUCT_NAME,
					  "HP Pavilion dv4 Notebook PC"),
			},
			.driver_data = "20090105",	/* F.30 */
		},
		{
			.ident = "dv5",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
				DMI_MATCH(DMI_PRODUCT_NAME,
					  "HP Pavilion dv5 Notebook PC"),
			},
			.driver_data = "20090506",	/* F.16 */
		},
		{
			.ident = "dv6",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
				DMI_MATCH(DMI_PRODUCT_NAME,
					  "HP Pavilion dv6 Notebook PC"),
			},
			.driver_data = "20090423",	/* F.21 */
		},
		{
			.ident = "HDX18",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
				DMI_MATCH(DMI_PRODUCT_NAME,
					  "HP HDX18 Notebook PC"),
			},
			.driver_data = "20090430",	/* F.23 */
		},
		/*
		 * Acer eMachines G725 has the same problem.  BIOS
		 * V1.03 is known to be broken.  V3.04 is known to
		 * work.  Inbetween, there are V1.06, V2.06 and V3.03
		 * that we don't have much idea about.  For now,
		 * blacklist anything older than V3.04.
		 *
		 * http://bugzilla.kernel.org/show_bug.cgi?id=15104
		 */
		{
			.ident = "G725",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "eMachines"),
				DMI_MATCH(DMI_PRODUCT_NAME, "eMachines G725"),
			},
			.driver_data = "20091216",	/* V3.04 */
		},
		{ }	/* terminate list */
	};
	const struct dmi_system_id *dmi = dmi_first_match(sysids);
	int year, month, date;
	char buf[9];

	if (!dmi || pdev->bus->number || pdev->devfn != PCI_DEVFN(0x1f, 2))
		return false;

	dmi_get_date(DMI_BIOS_DATE, &year, &month, &date);
	snprintf(buf, sizeof(buf), "%04d%02d%02d", year, month, date);

	return strcmp(buf, dmi->driver_data) < 0;
}

static bool ahci_broken_online(struct pci_dev *pdev)
{
#define ENCODE_BUSDEVFN(bus, slot, func)			\
	(void *)(unsigned long)(((bus) << 8) | PCI_DEVFN((slot), (func)))
	static const struct dmi_system_id sysids[] = {
		/*
		 * There are several gigabyte boards which use
		 * SIMG5723s configured as hardware RAID.  Certain
		 * 5723 firmware revisions shipped there keep the link
		 * online but fail to answer properly to SRST or
		 * IDENTIFY when no device is attached downstream
		 * causing libata to retry quite a few times leading
		 * to excessive detection delay.
		 *
		 * As these firmwares respond to the second reset try
		 * with invalid device signature, considering unknown
		 * sig as offline works around the problem acceptably.
		 */
		{
			.ident = "EP45-DQ6",
			.matches = {
				DMI_MATCH(DMI_BOARD_VENDOR,
					  "Gigabyte Technology Co., Ltd."),
				DMI_MATCH(DMI_BOARD_NAME, "EP45-DQ6"),
			},
			.driver_data = ENCODE_BUSDEVFN(0x0a, 0x00, 0),
		},
		{
			.ident = "EP45-DS5",
			.matches = {
				DMI_MATCH(DMI_BOARD_VENDOR,
					  "Gigabyte Technology Co., Ltd."),
				DMI_MATCH(DMI_BOARD_NAME, "EP45-DS5"),
			},
			.driver_data = ENCODE_BUSDEVFN(0x03, 0x00, 0),
		},
		{ }	/* terminate list */
	};
#undef ENCODE_BUSDEVFN
	const struct dmi_system_id *dmi = dmi_first_match(sysids);
	unsigned int val;

	if (!dmi)
		return false;

	val = (unsigned long)dmi->driver_data;

	return pdev->bus->number == (val >> 8) && pdev->devfn == (val & 0xff);
}

#ifdef CONFIG_ATA_ACPI
static void ahci_gtf_filter_workaround(struct ata_host *host)
{
	static const struct dmi_system_id sysids[] = {
		/*
		 * Aspire 3810T issues a bunch of SATA enable commands
		 * via _GTF including an invalid one and one which is
		 * rejected by the device.  Among the successful ones
		 * is FPDMA non-zero offset enable which when enabled
		 * only on the drive side leads to NCQ command
		 * failures.  Filter it out.
		 */
		{
			.ident = "Aspire 3810T",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
				DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 3810T"),
			},
			.driver_data = (void *)ATA_ACPI_FILTER_FPDMA_OFFSET,
		},
		{ }
	};
	const struct dmi_system_id *dmi = dmi_first_match(sysids);
	unsigned int filter;
	int i;

	if (!dmi)
		return;

	filter = (unsigned long)dmi->driver_data;
	dev_printk(KERN_INFO, host->dev,
		   "applying extra ACPI _GTF filter 0x%x for %s\n",
		   filter, dmi->ident);

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];
		struct ata_link *link;
		struct ata_device *dev;

		ata_for_each_link(link, ap, EDGE)
			ata_for_each_dev(dev, link, ALL)
				dev->gtf_filter |= filter;
	}
}
#else
static inline void ahci_gtf_filter_workaround(struct ata_host *host)
{}
#endif

static int ahci_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	static int printed_version;
	unsigned int board_id = ent->driver_data;
	struct ata_port_info pi = ahci_port_info[board_id];
	const struct ata_port_info *ppi[] = { &pi, NULL };
	struct device *dev = &pdev->dev;
	struct ahci_host_priv *hpriv;
	struct ata_host *host;
	int n_ports, i, rc;

	VPRINTK("ENTER\n");

	WARN_ON((int)ATA_MAX_QUEUE > AHCI_MAX_CMDS);

	if (!printed_version++)
		dev_printk(KERN_DEBUG, &pdev->dev, "version " DRV_VERSION "\n");

	/* The AHCI driver can only drive the SATA ports, the PATA driver
	   can drive them all so if both drivers are selected make sure
	   AHCI stays out of the way */
	if (pdev->vendor == PCI_VENDOR_ID_MARVELL && !marvell_enable)
		return -ENODEV;

	/*
	 * For some reason, MCP89 on MacBook 7,1 doesn't work with
	 * ahci, use ata_generic instead.
	 */
	if (pdev->vendor == PCI_VENDOR_ID_NVIDIA &&
	    pdev->device == PCI_DEVICE_ID_NVIDIA_NFORCE_MCP89_SATA &&
	    pdev->subsystem_vendor == PCI_VENDOR_ID_APPLE &&
	    pdev->subsystem_device == 0xcb89)
		return -ENODEV;

	/* Promise's PDC42819 is a SAS/SATA controller that has an AHCI mode.
	 * At the moment, we can only use the AHCI mode. Let the users know
	 * that for SAS drives they're out of luck.
	 */
	if (pdev->vendor == PCI_VENDOR_ID_PROMISE)
		dev_printk(KERN_INFO, &pdev->dev, "PDC42819 "
			   "can only drive SATA devices with this driver\n");

	/* acquire resources */
	rc = pcim_enable_device(pdev);
	if (rc)
		return rc;

	/* AHCI controllers often implement SFF compatible interface.
	 * Grab all PCI BARs just in case.
	 */
	rc = pcim_iomap_regions_request_all(pdev, 1 << AHCI_PCI_BAR, DRV_NAME);
	if (rc == -EBUSY)
		pcim_pin_device(pdev);
	if (rc)
		return rc;

	if (pdev->vendor == PCI_VENDOR_ID_INTEL &&
	    (pdev->device == 0x2652 || pdev->device == 0x2653)) {
		u8 map;

		/* ICH6s share the same PCI ID for both piix and ahci
		 * modes.  Enabling ahci mode while MAP indicates
		 * combined mode is a bad idea.  Yield to ata_piix.
		 */
		pci_read_config_byte(pdev, ICH_MAP, &map);
		if (map & 0x3) {
			dev_printk(KERN_INFO, &pdev->dev, "controller is in "
				   "combined mode, can't enable AHCI mode\n");
			return -ENODEV;
		}
	}

	hpriv = devm_kzalloc(dev, sizeof(*hpriv), GFP_KERNEL);
	if (!hpriv)
		return -ENOMEM;
	hpriv->flags |= (unsigned long)pi.private_data;

	/* MCP65 revision A1 and A2 can't do MSI */
	if (board_id == board_ahci_mcp65 &&
	    (pdev->revision == 0xa1 || pdev->revision == 0xa2))
		hpriv->flags |= AHCI_HFLAG_NO_MSI;

	if (board_id == board_ahci_sb700 && pdev->revision >= 0x40)
		hpriv->flags &= ~AHCI_HFLAG_IGN_SERR_INTERNAL;

	/* only some SB600s can do 64bit DMA */
	if (ahci_sb600_enable_64bit(pdev))
		hpriv->flags &= ~AHCI_HFLAG_32BIT_ONLY;

	if ((hpriv->flags & AHCI_HFLAG_NO_MSI) || pci_enable_msi(pdev))
		pci_intx(pdev, 1);

	hpriv->mmio = pcim_iomap_table(pdev)[AHCI_PCI_BAR];

	/* save initial config */
	ahci_pci_save_initial_config(pdev, hpriv);

	/* prepare host */
	if (hpriv->cap & HOST_CAP_NCQ) {
		pi.flags |= ATA_FLAG_NCQ;
		/*
		 * Auto-activate optimization is supposed to be
		 * supported on all AHCI controllers indicating NCQ
		 * capability, but it seems to be broken on some
		 * chipsets including NVIDIAs.
		 */
		if (!(hpriv->flags & AHCI_HFLAG_NO_FPDMA_AA))
			pi.flags |= ATA_FLAG_FPDMA_AA;
	}

	if (hpriv->cap & HOST_CAP_PMP)
		pi.flags |= ATA_FLAG_PMP;

	ahci_set_em_messages(hpriv, &pi);

	if (ahci_broken_system_poweroff(pdev)) {
		pi.flags |= ATA_FLAG_NO_POWEROFF_SPINDOWN;
		dev_info(&pdev->dev,
			"quirky BIOS, skipping spindown on poweroff\n");
	}

	if (ahci_broken_suspend(pdev)) {
		hpriv->flags |= AHCI_HFLAG_NO_SUSPEND;
		dev_printk(KERN_WARNING, &pdev->dev,
			   "BIOS update required for suspend/resume\n");
	}

	if (ahci_broken_online(pdev)) {
		hpriv->flags |= AHCI_HFLAG_SRST_TOUT_IS_OFFLINE;
		dev_info(&pdev->dev,
			 "online status unreliable, applying workaround\n");
	}

	/* CAP.NP sometimes indicate the index of the last enabled
	 * port, at other times, that of the last possible port, so
	 * determining the maximum port number requires looking at
	 * both CAP.NP and port_map.
	 */
	n_ports = max(ahci_nr_ports(hpriv->cap), fls(hpriv->port_map));

	host = ata_host_alloc_pinfo(&pdev->dev, ppi, n_ports);
	if (!host)
		return -ENOMEM;
	host->private_data = hpriv;

	if (!(hpriv->cap & HOST_CAP_SSS) || ahci_ignore_sss)
		host->flags |= ATA_HOST_PARALLEL_SCAN;
	else
		printk(KERN_INFO "ahci: SSS flag set, parallel bus scan disabled\n");

	if (pi.flags & ATA_FLAG_EM)
		ahci_reset_em(host);

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		ata_port_pbar_desc(ap, AHCI_PCI_BAR, -1, "abar");
		ata_port_pbar_desc(ap, AHCI_PCI_BAR,
				   0x100 + ap->port_no * 0x80, "port");

		/* set initial link pm policy */
		ap->pm_policy = NOT_AVAILABLE;

		/* set enclosure management message type */
		if (ap->flags & ATA_FLAG_EM)
			ap->em_message_type = hpriv->em_msg_type;


		/* disabled/not-implemented port */
		if (!(hpriv->port_map & (1 << i)))
			ap->ops = &ata_dummy_port_ops;
	}

	ahci_p5wdh_workaround(host);

	/* apply gtf filter quirk */
	ahci_gtf_filter_workaround(host);

	/* initialize adapter */
	rc = ahci_configure_dma_masks(pdev, hpriv->cap & HOST_CAP_64);
	if (rc)
		return rc;

	rc = ahci_pci_reset_controller(host);
	if (rc)
		return rc;

	ahci_pci_init_controller(host);
	ahci_pci_print_info(host);

	pci_set_master(pdev);
	return ata_host_activate(host, pdev->irq, ahci_interrupt, IRQF_SHARED,
				 &ahci_sht);
}

static int __init ahci_init(void)
{
	return pci_register_driver(&ahci_pci_driver);
}

static void __exit ahci_exit(void)
{
	pci_unregister_driver(&ahci_pci_driver);
}


MODULE_AUTHOR("Jeff Garzik");
MODULE_DESCRIPTION("AHCI SATA low-level driver");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, ahci_pci_tbl);
MODULE_VERSION(DRV_VERSION);

module_init(ahci_init);
module_exit(ahci_exit);
