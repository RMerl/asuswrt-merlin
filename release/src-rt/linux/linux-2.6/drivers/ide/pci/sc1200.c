/*
 * linux/drivers/ide/pci/sc1200.c		Version 0.94	Mar 10 2007
 *
 * Copyright (C) 2000-2002		Mark Lord <mlord@pobox.com>
 * Copyright (C)      2007		Bartlomiej Zolnierkiewicz
 *
 * May be copied or modified under the terms of the GNU General Public License
 *
 * Development of this chipset driver was funded
 * by the nice folks at National Semiconductor.
 *
 * Documentation:
 *	Available from National Semiconductor
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/ide.h>
#include <linux/pm.h>
#include <asm/io.h>
#include <asm/irq.h>

#define SC1200_REV_A	0x00
#define SC1200_REV_B1	0x01
#define SC1200_REV_B3	0x02
#define SC1200_REV_C1	0x03
#define SC1200_REV_D1	0x04

#define PCI_CLK_33	0x00
#define PCI_CLK_48	0x01
#define PCI_CLK_66	0x02
#define PCI_CLK_33A	0x03

static unsigned short sc1200_get_pci_clock (void)
{
	unsigned char chip_id, silicon_revision;
	unsigned int pci_clock;
	/*
	 * Check the silicon revision, as not all versions of the chip
	 * have the register with the fast PCI bus timings.
	 */
	chip_id = inb (0x903c);
	silicon_revision = inb (0x903d);

	// Read the fast pci clock frequency
	if (chip_id == 0x04 && silicon_revision < SC1200_REV_B1) {
		pci_clock = PCI_CLK_33;
	} else {
		// check clock generator configuration (cfcc)
		// the clock is in bits 8 and 9 of this word

		pci_clock = inw (0x901e);
		pci_clock >>= 8;
		pci_clock &= 0x03;
		if (pci_clock == PCI_CLK_33A)
			pci_clock = PCI_CLK_33;
	}
	return pci_clock;
}

extern char *ide_xfer_verbose (byte xfer_rate);

/*
 * Set a new transfer mode at the drive
 */
static int sc1200_set_xfer_mode (ide_drive_t *drive, byte mode)
{
	printk("%s: sc1200_set_xfer_mode(%s)\n", drive->name, ide_xfer_verbose(mode));
	return ide_config_drive_speed(drive, mode);
}

/*
 * Here are the standard PIO mode 0-4 timings for each "format".
 * Format-0 uses fast data reg timings, with slower command reg timings.
 * Format-1 uses fast timings for all registers, but won't work with all drives.
 */
static const unsigned int sc1200_pio_timings[4][5] =
	{{0x00009172, 0x00012171, 0x00020080, 0x00032010, 0x00040010},	// format0  33Mhz
	 {0xd1329172, 0x71212171, 0x30200080, 0x20102010, 0x00100010},	// format1, 33Mhz
	 {0xfaa3f4f3, 0xc23232b2, 0x513101c1, 0x31213121, 0x10211021},	// format1, 48Mhz
	 {0xfff4fff4, 0xf35353d3, 0x814102f1, 0x42314231, 0x11311131}};	// format1, 66Mhz

/*
 * After chip reset, the PIO timings are set to 0x00009172, which is not valid.
 */
//#define SC1200_BAD_PIO(timings) (((timings)&~0x80000000)==0x00009172)

static void sc1200_tunepio(ide_drive_t *drive, u8 pio)
{
	ide_hwif_t *hwif = drive->hwif;
	struct pci_dev *pdev = hwif->pci_dev;
	unsigned int basereg = hwif->channel ? 0x50 : 0x40, format = 0;

	pci_read_config_dword(pdev, basereg + 4, &format);
	format = (format >> 31) & 1;
	if (format)
		format += sc1200_get_pci_clock();
	pci_write_config_dword(pdev, basereg + ((drive->dn & 1) << 3),
			       sc1200_pio_timings[format][pio]);
}

/*
 *	The SC1200 specifies that two drives sharing a cable cannot mix
 *	UDMA/MDMA.  It has to be one or the other, for the pair, though
 *	different timings can still be chosen for each drive.  We could
 *	set the appropriate timing bits on the fly, but that might be
 *	a bit confusing.  So, for now we statically handle this requirement
 *	by looking at our mate drive to see what it is capable of, before
 *	choosing a mode for our own drive.
 */
static u8 sc1200_udma_filter(ide_drive_t *drive)
{
	ide_hwif_t *hwif = drive->hwif;
	ide_drive_t *mate = &hwif->drives[(drive->dn & 1) ^ 1];
	struct hd_driveid *mateid = mate->id;
	u8 mask = hwif->ultra_mask;

	if (mate->present == 0)
		goto out;

	if ((mateid->capability & 1) && __ide_dma_bad_drive(mate) == 0) {
		if ((mateid->field_valid & 4) && (mateid->dma_ultra & 7))
			goto out;
		if ((mateid->field_valid & 2) && (mateid->dma_mword & 7))
			mask = 0;
	}
out:
	return mask;
}

static int sc1200_tune_chipset(ide_drive_t *drive, u8 mode)
{
	ide_hwif_t		*hwif = HWIF(drive);
	int			unit = drive->select.b.unit;
	unsigned int		reg, timings;
	unsigned short		pci_clock;
	unsigned int		basereg = hwif->channel ? 0x50 : 0x40;

	mode = ide_rate_filter(drive, mode);

	/*
	 * Tell the drive to switch to the new mode; abort on failure.
	 */
	if (sc1200_set_xfer_mode(drive, mode)) {
		printk("SC1200: set xfer mode failure\n");
		return 1;	/* failure */
	}

	switch (mode) {
	case XFER_PIO_4:
	case XFER_PIO_3:
	case XFER_PIO_2:
	case XFER_PIO_1:
	case XFER_PIO_0:
		sc1200_tunepio(drive, mode - XFER_PIO_0);
		return 0;
	}

	pci_clock = sc1200_get_pci_clock();

	/*
	 * Now tune the chipset to match the drive:
	 *
	 * Note that each DMA mode has several timings associated with it.
	 * The correct timing depends on the fast PCI clock freq.
	 */
	timings = 0;
	switch (mode) {
		case XFER_UDMA_0:
			switch (pci_clock) {
				case PCI_CLK_33:	timings = 0x00921250;	break;
				case PCI_CLK_48:	timings = 0x00932470;	break;
				case PCI_CLK_66:	timings = 0x009436a1;	break;
			}
			break;
		case XFER_UDMA_1:
			switch (pci_clock) {
				case PCI_CLK_33:	timings = 0x00911140;	break;
				case PCI_CLK_48:	timings = 0x00922260;	break;
				case PCI_CLK_66:	timings = 0x00933481;	break;
			}
			break;
		case XFER_UDMA_2:
			switch (pci_clock) {
				case PCI_CLK_33:	timings = 0x00911030;	break;
				case PCI_CLK_48:	timings = 0x00922140;	break;
				case PCI_CLK_66:	timings = 0x00923261;	break;
			}
			break;
		case XFER_MW_DMA_0:
			switch (pci_clock) {
				case PCI_CLK_33:	timings = 0x00077771;	break;
				case PCI_CLK_48:	timings = 0x000bbbb2;	break;
				case PCI_CLK_66:	timings = 0x000ffff3;	break;
			}
			break;
		case XFER_MW_DMA_1:
			switch (pci_clock) {
				case PCI_CLK_33:	timings = 0x00012121;	break;
				case PCI_CLK_48:	timings = 0x00024241;	break;
				case PCI_CLK_66:	timings = 0x00035352;	break;
			}
			break;
		case XFER_MW_DMA_2:
			switch (pci_clock) {
				case PCI_CLK_33:	timings = 0x00002020;	break;
				case PCI_CLK_48:	timings = 0x00013131;	break;
				case PCI_CLK_66:	timings = 0x00015151;	break;
			}
			break;
		default:
			BUG();
			break;
	}

	if (unit == 0) {			/* are we configuring drive0? */
		pci_read_config_dword(hwif->pci_dev, basereg+4, &reg);
		timings |= reg & 0x80000000;	/* preserve PIO format bit */
		pci_write_config_dword(hwif->pci_dev, basereg+4, timings);
	} else {
		pci_write_config_dword(hwif->pci_dev, basereg+12, timings);
	}

	return 0;	/* success */
}

/*
 * sc1200_config_dma() handles selection/setting of DMA/UDMA modes
 * for both the chipset and drive.
 */
static int sc1200_config_dma (ide_drive_t *drive)
{
	if (ide_tune_dma(drive))
		return 0;

	return 1;
}


/*  Replacement for the standard ide_dma_end action in
 *  dma_proc.
 *
 *  returns 1 on error, 0 otherwise
 */
static int sc1200_ide_dma_end (ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	unsigned long dma_base = hwif->dma_base;
	byte dma_stat;

	dma_stat = inb(dma_base+2);		/* get DMA status */

	if (!(dma_stat & 4))
		printk(" ide_dma_end dma_stat=%0x err=%x newerr=%x\n",
		  dma_stat, ((dma_stat&7)!=4), ((dma_stat&2)==2));

	outb(dma_stat|0x1b, dma_base+2);	/* clear the INTR & ERROR bits */
	outb(inb(dma_base)&~1, dma_base);	/* !! DO THIS HERE !! stop DMA */

	drive->waiting_for_dma = 0;
	ide_destroy_dmatable(drive);		/* purge DMA mappings */

	return (dma_stat & 7) != 4;		/* verify good DMA status */
}

/*
 * sc1200_tuneproc() handles selection/setting of PIO modes
 * for both the chipset and drive.
 *
 * All existing BIOSs for this chipset guarantee that all drives
 * will have valid default PIO timings set up before we get here.
 */
static void sc1200_tuneproc (ide_drive_t *drive, byte pio)	/* mode=255 means "autotune" */
{
	ide_hwif_t	*hwif = HWIF(drive);
	int		mode = -1;

	/*
	 * bad abuse of ->tuneproc interface
	 */
	switch (pio) {
		case 200: mode = XFER_UDMA_0;	break;
		case 201: mode = XFER_UDMA_1;	break;
		case 202: mode = XFER_UDMA_2;	break;
		case 100: mode = XFER_MW_DMA_0;	break;
		case 101: mode = XFER_MW_DMA_1;	break;
		case 102: mode = XFER_MW_DMA_2;	break;
	}
	if (mode != -1) {
		printk("SC1200: %s: changing (U)DMA mode\n", drive->name);
		hwif->dma_off_quietly(drive);
		if (sc1200_tune_chipset(drive, mode) == 0)
			hwif->dma_host_on(drive);
		return;
	}

	pio = ide_get_best_pio_mode(drive, pio, 4, NULL);
	printk("SC1200: %s: setting PIO mode%d\n", drive->name, pio);

	if (sc1200_set_xfer_mode(drive, XFER_PIO_0 + pio) == 0)
		sc1200_tunepio(drive, pio);
}

#ifdef CONFIG_PM
static ide_hwif_t *lookup_pci_dev (ide_hwif_t *prev, struct pci_dev *dev)
{
	int	h;

	for (h = 0; h < MAX_HWIFS; h++) {
		ide_hwif_t *hwif = &ide_hwifs[h];
		if (prev) {
			if (hwif == prev)
				prev = NULL;	// found previous, now look for next match
		} else {
			if (hwif && hwif->pci_dev == dev)
				return hwif;	// found next match
		}
	}
	return NULL;	// not found
}

typedef struct sc1200_saved_state_s {
	__u32		regs[4];
} sc1200_saved_state_t;


static int sc1200_suspend (struct pci_dev *dev, pm_message_t state)
{
	ide_hwif_t		*hwif = NULL;

	printk("SC1200: suspend(%u)\n", state.event);

	if (state.event == PM_EVENT_ON) {
		// we only save state when going from full power to less

		//
		// Loop over all interfaces that are part of this PCI device:
		//
		while ((hwif = lookup_pci_dev(hwif, dev)) != NULL) {
			sc1200_saved_state_t	*ss;
			unsigned int		basereg, r;
			//
			// allocate a permanent save area, if not already allocated
			//
			ss = (sc1200_saved_state_t *)hwif->config_data;
			if (ss == NULL) {
				ss = kmalloc(sizeof(sc1200_saved_state_t), GFP_KERNEL);
				if (ss == NULL)
					return -ENOMEM;
				hwif->config_data = (unsigned long)ss;
			}
			ss = (sc1200_saved_state_t *)hwif->config_data;
			//
			// Save timing registers:  this may be unnecessary if 
			// BIOS also does it
			//
			basereg = hwif->channel ? 0x50 : 0x40;
			for (r = 0; r < 4; ++r) {
				pci_read_config_dword (hwif->pci_dev, basereg + (r<<2), &ss->regs[r]);
			}
		}
	}

	/* You don't need to iterate over disks -- sysfs should have done that for you already */ 

	pci_disable_device(dev);
	pci_set_power_state(dev, pci_choose_state(dev, state));
	dev->current_state = state.event;
	return 0;
}

static int sc1200_resume (struct pci_dev *dev)
{
	ide_hwif_t	*hwif = NULL;

	pci_set_power_state(dev, PCI_D0);	// bring chip back from sleep state
	dev->current_state = PM_EVENT_ON;
	pci_enable_device(dev);
	//
	// loop over all interfaces that are part of this pci device:
	//
	while ((hwif = lookup_pci_dev(hwif, dev)) != NULL) {
		unsigned int		basereg, r, d, format;
		sc1200_saved_state_t	*ss = (sc1200_saved_state_t *)hwif->config_data;

		//
		// Restore timing registers:  this may be unnecessary if BIOS also does it
		//
		basereg = hwif->channel ? 0x50 : 0x40;
		if (ss != NULL) {
			for (r = 0; r < 4; ++r) {
				pci_write_config_dword(hwif->pci_dev, basereg + (r<<2), ss->regs[r]);
			}
		}
		//
		// Re-program drive PIO modes
		//
		pci_read_config_dword(hwif->pci_dev, basereg+4, &format);
		format = (format >> 31) & 1;
		if (format)
			format += sc1200_get_pci_clock();
		for (d = 0; d < 2; ++d) {
			ide_drive_t *drive = &(hwif->drives[d]);
			if (drive->present) {
				unsigned int pio, timings;
				pci_read_config_dword(hwif->pci_dev, basereg+(drive->select.b.unit << 3), &timings);
				for (pio = 0; pio <= 4; ++pio) {
					if (sc1200_pio_timings[format][pio] == timings)
						break;
				}
				if (pio > 4)
					pio = 255; /* autotune */
				(void)sc1200_tuneproc(drive, pio);
			}
		}
		//
		// Re-program drive DMA modes
		//
		for (d = 0; d < MAX_DRIVES; ++d) {
			ide_drive_t *drive = &(hwif->drives[d]);
			if (drive->present && !__ide_dma_bad_drive(drive)) {
				int enable_dma = drive->using_dma;
				hwif->dma_off_quietly(drive);
				if (sc1200_config_dma(drive))
					enable_dma = 0;
				if (enable_dma)
					hwif->dma_host_on(drive);
			}
		}
	}
	return 0;
}
#endif

/*
 * This gets invoked by the IDE driver once for each channel,
 * and performs channel-specific pre-initialization before drive probing.
 */
static void __devinit init_hwif_sc1200 (ide_hwif_t *hwif)
{
	if (hwif->mate)
		hwif->serialized = hwif->mate->serialized = 1;
	hwif->autodma = 0;
	if (hwif->dma_base) {
		hwif->udma_filter = sc1200_udma_filter;
		hwif->ide_dma_check = &sc1200_config_dma;
		hwif->ide_dma_end   = &sc1200_ide_dma_end;
        	if (!noautodma)
                	hwif->autodma = 1;
		hwif->tuneproc = &sc1200_tuneproc;
		hwif->speedproc = &sc1200_tune_chipset;
	}
        hwif->atapi_dma = 1;
        hwif->ultra_mask = 0x07;
        hwif->mwdma_mask = 0x07;

        hwif->drives[0].autodma = hwif->autodma;
        hwif->drives[1].autodma = hwif->autodma;
}

static ide_pci_device_t sc1200_chipset __devinitdata = {
	.name		= "SC1200",
	.init_hwif	= init_hwif_sc1200,
	.channels	= 2,
	.autodma	= AUTODMA,
	.bootable	= ON_BOARD,
};

static int __devinit sc1200_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	return ide_setup_pci_device(dev, &sc1200_chipset);
}

static struct pci_device_id sc1200_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_NS, PCI_DEVICE_ID_NS_SCx200_IDE), 0},
	{ 0, },
};
MODULE_DEVICE_TABLE(pci, sc1200_pci_tbl);

static struct pci_driver driver = {
	.name		= "SC1200_IDE",
	.id_table	= sc1200_pci_tbl,
	.probe		= sc1200_init_one,
#ifdef CONFIG_PM
	.suspend	= sc1200_suspend,
	.resume		= sc1200_resume,
#endif
};

static int __init sc1200_ide_init(void)
{
	return ide_pci_register_driver(&driver);
}

module_init(sc1200_ide_init);

MODULE_AUTHOR("Mark Lord");
MODULE_DESCRIPTION("PCI driver module for NS SC1200 IDE");
MODULE_LICENSE("GPL");
