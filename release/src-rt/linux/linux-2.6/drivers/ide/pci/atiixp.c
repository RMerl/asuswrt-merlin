/*
 *  linux/drivers/ide/pci/atiixp.c	Version 0.01-bart2	Feb. 26, 2004
 *
 *  Copyright (C) 2003 ATI Inc. <hyu@ati.com>
 *  Copyright (C) 2004 Bartlomiej Zolnierkiewicz
 *
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/hdreg.h>
#include <linux/ide.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/io.h>

#define ATIIXP_IDE_PIO_TIMING		0x40
#define ATIIXP_IDE_MDMA_TIMING		0x44
#define ATIIXP_IDE_PIO_CONTROL		0x48
#define ATIIXP_IDE_PIO_MODE		0x4a
#define ATIIXP_IDE_UDMA_CONTROL		0x54
#define ATIIXP_IDE_UDMA_MODE		0x56

typedef struct {
	u8 command_width;
	u8 recover_width;
} atiixp_ide_timing;

static atiixp_ide_timing pio_timing[] = {
	{ 0x05, 0x0d },
	{ 0x04, 0x07 },
	{ 0x03, 0x04 },
	{ 0x02, 0x02 },
	{ 0x02, 0x00 },
};

static atiixp_ide_timing mdma_timing[] = {
	{ 0x07, 0x07 },
	{ 0x02, 0x01 },
	{ 0x02, 0x00 },
};

static int save_mdma_mode[4];

static DEFINE_SPINLOCK(atiixp_lock);

/**
 *	atiixp_dma_2_pio		-	return the PIO mode matching DMA
 *	@xfer_rate: transfer speed
 *
 *	Returns the nearest equivalent PIO timing for the PIO or DMA
 *	mode requested by the controller.
 */

static u8 atiixp_dma_2_pio(u8 xfer_rate) {
	switch(xfer_rate) {
		case XFER_UDMA_6:
		case XFER_UDMA_5:
		case XFER_UDMA_4:
		case XFER_UDMA_3:
		case XFER_UDMA_2:
		case XFER_UDMA_1:
		case XFER_UDMA_0:
		case XFER_MW_DMA_2:
		case XFER_PIO_4:
			return 4;
		case XFER_MW_DMA_1:
		case XFER_PIO_3:
			return 3;
		case XFER_SW_DMA_2:
		case XFER_PIO_2:
			return 2;
		case XFER_MW_DMA_0:
		case XFER_SW_DMA_1:
		case XFER_SW_DMA_0:
		case XFER_PIO_1:
		case XFER_PIO_0:
		case XFER_PIO_SLOW:
		default:
			return 0;
	}
}

static void atiixp_dma_host_on(ide_drive_t *drive)
{
	struct pci_dev *dev = drive->hwif->pci_dev;
	unsigned long flags;
	u16 tmp16;

	spin_lock_irqsave(&atiixp_lock, flags);

	pci_read_config_word(dev, ATIIXP_IDE_UDMA_CONTROL, &tmp16);
	if (save_mdma_mode[drive->dn])
		tmp16 &= ~(1 << drive->dn);
	else
		tmp16 |= (1 << drive->dn);
	pci_write_config_word(dev, ATIIXP_IDE_UDMA_CONTROL, tmp16);

	spin_unlock_irqrestore(&atiixp_lock, flags);

	ide_dma_host_on(drive);
}

static void atiixp_dma_host_off(ide_drive_t *drive)
{
	struct pci_dev *dev = drive->hwif->pci_dev;
	unsigned long flags;
	u16 tmp16;

	spin_lock_irqsave(&atiixp_lock, flags);

	pci_read_config_word(dev, ATIIXP_IDE_UDMA_CONTROL, &tmp16);
	tmp16 &= ~(1 << drive->dn);
	pci_write_config_word(dev, ATIIXP_IDE_UDMA_CONTROL, tmp16);

	spin_unlock_irqrestore(&atiixp_lock, flags);

	ide_dma_host_off(drive);
}

/**
 *	atiixp_tune_drive		-	tune a drive attached to a ATIIXP
 *	@drive: drive to tune
 *	@pio: desired PIO mode
 *
 *	Set the interface PIO mode.
 */

static void atiixp_tuneproc(ide_drive_t *drive, u8 pio)
{
	struct pci_dev *dev = drive->hwif->pci_dev;
	unsigned long flags;
	int timing_shift = (drive->dn & 2) ? 16 : 0 + (drive->dn & 1) ? 0 : 8;
	u32 pio_timing_data;
	u16 pio_mode_data;

	spin_lock_irqsave(&atiixp_lock, flags);

	pci_read_config_word(dev, ATIIXP_IDE_PIO_MODE, &pio_mode_data);
	pio_mode_data &= ~(0x07 << (drive->dn * 4));
	pio_mode_data |= (pio << (drive->dn * 4));
	pci_write_config_word(dev, ATIIXP_IDE_PIO_MODE, pio_mode_data);

	pci_read_config_dword(dev, ATIIXP_IDE_PIO_TIMING, &pio_timing_data);
	pio_timing_data &= ~(0xff << timing_shift);
	pio_timing_data |= (pio_timing[pio].recover_width << timing_shift) |
		 (pio_timing[pio].command_width << (timing_shift + 4));
	pci_write_config_dword(dev, ATIIXP_IDE_PIO_TIMING, pio_timing_data);

	spin_unlock_irqrestore(&atiixp_lock, flags);
}

/**
 *	atiixp_tune_chipset	-	tune a ATIIXP interface
 *	@drive: IDE drive to tune
 *	@xferspeed: speed to configure
 *
 *	Set a ATIIXP interface channel to the desired speeds. This involves
 *	requires the right timing data into the ATIIXP configuration space
 *	then setting the drive parameters appropriately
 */

static int atiixp_speedproc(ide_drive_t *drive, u8 xferspeed)
{
	struct pci_dev *dev = drive->hwif->pci_dev;
	unsigned long flags;
	int timing_shift = (drive->dn & 2) ? 16 : 0 + (drive->dn & 1) ? 0 : 8;
	u32 tmp32;
	u16 tmp16;
	u8 speed, pio;

	speed = ide_rate_filter(drive, xferspeed);

	spin_lock_irqsave(&atiixp_lock, flags);

	save_mdma_mode[drive->dn] = 0;
	if (speed >= XFER_UDMA_0) {
		pci_read_config_word(dev, ATIIXP_IDE_UDMA_MODE, &tmp16);
		tmp16 &= ~(0x07 << (drive->dn * 4));
		tmp16 |= ((speed & 0x07) << (drive->dn * 4));
		pci_write_config_word(dev, ATIIXP_IDE_UDMA_MODE, tmp16);
	} else {
		if ((speed >= XFER_MW_DMA_0) && (speed <= XFER_MW_DMA_2)) {
			save_mdma_mode[drive->dn] = speed;
			pci_read_config_dword(dev, ATIIXP_IDE_MDMA_TIMING, &tmp32);
			tmp32 &= ~(0xff << timing_shift);
			tmp32 |= (mdma_timing[speed & 0x03].recover_width << timing_shift) |
				(mdma_timing[speed & 0x03].command_width << (timing_shift + 4));
			pci_write_config_dword(dev, ATIIXP_IDE_MDMA_TIMING, tmp32);
		}
	}

	spin_unlock_irqrestore(&atiixp_lock, flags);

	if (speed >= XFER_SW_DMA_0)
		pio = atiixp_dma_2_pio(speed);
	else
		pio = speed - XFER_PIO_0;

	atiixp_tuneproc(drive, pio);

	return ide_config_drive_speed(drive, speed);
}

/**
 *	atiixp_dma_check	-	set up an IDE device
 *	@drive: IDE drive to configure
 *
 *	Set up the ATIIXP interface for the best available speed on this
 *	interface, preferring DMA to PIO.
 */

static int atiixp_dma_check(ide_drive_t *drive)
{
	u8 tspeed, speed;

	drive->init_speed = 0;

	if (ide_tune_dma(drive))
		return 0;

	if (ide_use_fast_pio(drive)) {
		tspeed = ide_get_best_pio_mode(drive, 255, 5, NULL);
		speed = atiixp_dma_2_pio(XFER_PIO_0 + tspeed) + XFER_PIO_0;
		atiixp_speedproc(drive, speed);
	}

	return -1;
}

/**
 *	init_hwif_atiixp		-	fill in the hwif for the ATIIXP
 *	@hwif: IDE interface
 *
 *	Set up the ide_hwif_t for the ATIIXP interface according to the
 *	capabilities of the hardware.
 */

static void __devinit init_hwif_atiixp(ide_hwif_t *hwif)
{
	u8 udma_mode = 0;
	u8 ch = hwif->channel;
	struct pci_dev *pdev = hwif->pci_dev;

	if (!hwif->irq)
		hwif->irq = ch ? 15 : 14;

	hwif->autodma = 0;
	hwif->tuneproc = &atiixp_tuneproc;
	hwif->speedproc = &atiixp_speedproc;
	hwif->drives[0].autotune = 1;
	hwif->drives[1].autotune = 1;

	if (!hwif->dma_base)
		return;

	hwif->atapi_dma = 1;
	hwif->ultra_mask = 0x3f;
	hwif->mwdma_mask = 0x06;
	hwif->swdma_mask = 0x04;

	pci_read_config_byte(pdev, ATIIXP_IDE_UDMA_MODE + ch, &udma_mode);
	if ((udma_mode & 0x07) >= 0x04 || (udma_mode & 0x70) >= 0x40)
		hwif->udma_four = 1;
	else
		hwif->udma_four = 0;

	hwif->dma_host_on = &atiixp_dma_host_on;
	hwif->dma_host_off = &atiixp_dma_host_off;
	hwif->ide_dma_check = &atiixp_dma_check;
	if (!noautodma)
		hwif->autodma = 1;

	hwif->drives[1].autodma = hwif->autodma;
	hwif->drives[0].autodma = hwif->autodma;
}


static ide_pci_device_t atiixp_pci_info[] __devinitdata = {
	{	/* 0 */
		.name		= "ATIIXP",
		.init_hwif	= init_hwif_atiixp,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x48,0x01,0x00}, {0x48,0x08,0x00}},
		.bootable	= ON_BOARD,
	},{	/* 1 */
		.name		= "SB600_PATA",
		.init_hwif	= init_hwif_atiixp,
		.channels	= 1,
		.autodma	= AUTODMA,
		.enablebits	= {{0x48,0x01,0x00}, {0x00,0x00,0x00}},
 		.bootable	= ON_BOARD,
 	},
};

/**
 *	atiixp_init_one	-	called when a ATIIXP is found
 *	@dev: the atiixp device
 *	@id: the matching pci id
 *
 *	Called when the PCI registration layer (or the IDE initialization)
 *	finds a device matching our IDE device tables.
 */

static int __devinit atiixp_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	return ide_setup_pci_device(dev, &atiixp_pci_info[id->driver_data]);
}

static struct pci_device_id atiixp_pci_tbl[] = {
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_IXP200_IDE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_IXP300_IDE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_IXP400_IDE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_IXP600_IDE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1},
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_IXP700_IDE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1},
	{ 0, },
};
MODULE_DEVICE_TABLE(pci, atiixp_pci_tbl);

static struct pci_driver driver = {
	.name		= "ATIIXP_IDE",
	.id_table	= atiixp_pci_tbl,
	.probe		= atiixp_init_one,
};

static int __init atiixp_ide_init(void)
{
	return ide_pci_register_driver(&driver);
}

module_init(atiixp_ide_init);

MODULE_AUTHOR("HUI YU");
MODULE_DESCRIPTION("PCI driver module for ATI IXP IDE");
MODULE_LICENSE("GPL");
