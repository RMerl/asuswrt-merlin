/***************************************************************************
 *  Silicon Motion VoyagerGX framebuffer driver
 *
 * 	ported to 2.6 by Embedded Alley Solutions, Inc
 * 	Copyright (C) 2005 Embedded Alley Solutions, Inc
 *
 * 		based on
    copyright            : (C) 2001 by Szu-Tao Huang
    email                : johuang@siliconmotion.com
    Updated to SM501 by Eric.Devolder@amd.com and dan@embeddededge.com
    for the AMD Mirage Portable Tablet.  20 Oct 2003
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/uaccess.h>

static char __iomem *SMIRegs;	// point to virtual Memory Map IO starting address
static char __iomem *SMILFB;	// point to virtual video memory starting address

static struct fb_fix_screeninfo smifb_fix __devinitdata = {
	.id =		"smivgx",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.ywrapstep = 	0,
	.line_length	= 1024 * 2, /* (bbp * xres)/8 */
	.accel =	FB_ACCEL_NONE,
};

static struct fb_var_screeninfo smifb_var __devinitdata = {
	.xres           = 1024,
	.yres           = 768,
	.xres_virtual   = 1024,
	.yres_virtual   = 768,
	.bits_per_pixel = 16,
	.red            = { 11, 5, 0 },
	.green          = {  5, 6, 0 },
	.blue           = {  0, 5, 0 },
	.activate       = FB_ACTIVATE_NOW,
	.height         = -1,
	.width          = -1,
	.vmode          = FB_VMODE_NONINTERLACED,
};


static struct fb_info info;

#define smi_mmiowb(dat,reg)	writeb(dat, (SMIRegs + reg))
#define smi_mmioww(dat,reg)	writew(dat, (SMIRegs + reg))
#define smi_mmiowl(dat,reg)	writel(dat, (SMIRegs + reg))

#define smi_mmiorb(reg)	        readb(SMIRegs + reg)
#define smi_mmiorw(reg)	        readw(SMIRegs + reg)
#define smi_mmiorl(reg)	        readl(SMIRegs + reg)

/* Address space offsets for various control/status registers.
*/
#define MISC_CTRL			0x000004
#define GPIO_LO_CTRL			0x000008
#define GPIO_HI_CTRL			0x00000c
#define DRAM_CTRL			0x000010
#define CURRENT_POWER_GATE		0x000038
#define CURRENT_POWER_CLOCK		0x00003C
#define POWER_MODE1_GATE                0x000048
#define POWER_MODE1_CLOCK               0x00004C
#define POWER_MODE_CTRL			0x000054

#define GPIO_DATA_LO			0x010000
#define GPIO_DATA_HI			0x010004
#define GPIO_DATA_DIR_LO		0x010008
#define GPIO_DATA_DIR_HI		0x01000c
#define I2C_BYTE_COUNT			0x010040
#define I2C_CONTROL			0x010041
#define I2C_STATUS_RESET		0x010042
#define I2C_SLAVE_ADDRESS		0x010043
#define I2C_DATA			0x010044

#define DE_COLOR_COMPARE		0x100020
#define DE_COLOR_COMPARE_MASK		0x100024
#define DE_MASKS			0x100028
#define DE_WRAP				0x10004C

#define PANEL_DISPLAY_CTRL              0x080000
#define PANEL_PAN_CTRL                  0x080004
#define PANEL_COLOR_KEY                 0x080008
#define PANEL_FB_ADDRESS                0x08000C
#define PANEL_FB_WIDTH                  0x080010
#define PANEL_WINDOW_WIDTH              0x080014
#define PANEL_WINDOW_HEIGHT             0x080018
#define PANEL_PLANE_TL                  0x08001C
#define PANEL_PLANE_BR                  0x080020
#define PANEL_HORIZONTAL_TOTAL          0x080024
#define PANEL_HORIZONTAL_SYNC           0x080028
#define PANEL_VERTICAL_TOTAL            0x08002C
#define PANEL_VERTICAL_SYNC             0x080030
#define PANEL_CURRENT_LINE              0x080034
#define VIDEO_DISPLAY_CTRL		0x080040
#define VIDEO_DISPLAY_FB0		0x080044
#define VIDEO_DISPLAY_FBWIDTH		0x080048
#define VIDEO_DISPLAY_FB0LAST		0x08004C
#define VIDEO_DISPLAY_TL		0x080050
#define VIDEO_DISPLAY_BR		0x080054
#define VIDEO_SCALE			0x080058
#define VIDEO_INITIAL_SCALE		0x08005C
#define VIDEO_YUV_CONSTANTS		0x080060
#define VIDEO_DISPLAY_FB1		0x080064
#define VIDEO_DISPLAY_FB1LAST		0x080068
#define VIDEO_ALPHA_CTRL		0x080080
#define PANEL_HWC_ADDRESS		0x0800F0
#define CRT_DISPLAY_CTRL		0x080200
#define CRT_FB_ADDRESS			0x080204
#define CRT_FB_WIDTH			0x080208
#define CRT_HORIZONTAL_TOTAL		0x08020c
#define CRT_HORIZONTAL_SYNC		0x080210
#define CRT_VERTICAL_TOTAL		0x080214
#define CRT_VERTICAL_SYNC		0x080218
#define CRT_HWC_ADDRESS			0x080230
#define CRT_HWC_LOCATION		0x080234

#define ZV_CAPTURE_CTRL			0x090000
#define ZV_CAPTURE_CLIP			0x090004
#define ZV_CAPTURE_SIZE			0x090008
#define ZV_CAPTURE_BUF0			0x09000c
#define ZV_CAPTURE_BUF1			0x090010
#define ZV_CAPTURE_OFFSET		0x090014
#define ZV_FIFO_CTRL			0x090018

#define waitforvsync() udelay(400)

static int initdone = 0;
static int crt_out = 1;


static int
smi_setcolreg(unsigned regno, unsigned red, unsigned green,
	unsigned blue, unsigned transp,
	struct fb_info *info)
{
	if (regno > 255)
		return 1;

	((u32 *)(info->pseudo_palette))[regno] =
		    ((red & 0xf800) >> 0) |
		    ((green & 0xfc00) >> 5) |
		    ((blue & 0xf800) >> 11);

	return 0;
}

/* This function still needs lots of work to generically support
 * different output devices (CRT or LCD) and resolutions.
 * Currently hard-coded for 1024x768 LCD panel.
 */
static void smi_setmode(void)
{
	if (initdone)
		return;

	initdone = 1;

	/* Just blast in some control values based upon the chip
	 * documentation.  We use the internal memory, I don't know
	 * how to determine the amount available yet.
	 */
	smi_mmiowl(0x07F127C2, DRAM_CTRL);
	smi_mmiowl(0x02000020, PANEL_HWC_ADDRESS);
	smi_mmiowl(0x007FF800, PANEL_HWC_ADDRESS);
	smi_mmiowl(0x00021827, POWER_MODE1_GATE);
	smi_mmiowl(0x011A0A09, POWER_MODE1_CLOCK);
	smi_mmiowl(0x00000001, POWER_MODE_CTRL);
	smi_mmiowl(0x80000000, PANEL_FB_ADDRESS);
	smi_mmiowl(0x08000800, PANEL_FB_WIDTH);
	smi_mmiowl(0x04000000, PANEL_WINDOW_WIDTH);
	smi_mmiowl(0x03000000, PANEL_WINDOW_HEIGHT);
	smi_mmiowl(0x00000000, PANEL_PLANE_TL);
	smi_mmiowl(0x02FF03FF, PANEL_PLANE_BR);
	smi_mmiowl(0x05D003FF, PANEL_HORIZONTAL_TOTAL);
	smi_mmiowl(0x00C80424, PANEL_HORIZONTAL_SYNC);
	smi_mmiowl(0x032502FF, PANEL_VERTICAL_TOTAL);
	smi_mmiowl(0x00060302, PANEL_VERTICAL_SYNC);
	smi_mmiowl(0x00013905, PANEL_DISPLAY_CTRL);
	smi_mmiowl(0x01013105, PANEL_DISPLAY_CTRL);
	waitforvsync();
	smi_mmiowl(0x03013905, PANEL_DISPLAY_CTRL);
	waitforvsync();
	smi_mmiowl(0x07013905, PANEL_DISPLAY_CTRL);
	waitforvsync();
	smi_mmiowl(0x0F013905, PANEL_DISPLAY_CTRL);
	smi_mmiowl(0x0002187F, POWER_MODE1_GATE);
	smi_mmiowl(0x01011801, POWER_MODE1_CLOCK);
	smi_mmiowl(0x00000001, POWER_MODE_CTRL);

	smi_mmiowl(0x80000000, PANEL_FB_ADDRESS);
	smi_mmiowl(0x00000000, PANEL_PAN_CTRL);
	smi_mmiowl(0x00000000, PANEL_COLOR_KEY);

	if (crt_out) {
		/* Just sent the panel out to the CRT for now.
		*/
		smi_mmiowl(0x80000000, CRT_FB_ADDRESS);
		smi_mmiowl(0x08000800, CRT_FB_WIDTH);
		smi_mmiowl(0x05D003FF, CRT_HORIZONTAL_TOTAL);
		smi_mmiowl(0x00C80424, CRT_HORIZONTAL_SYNC);
		smi_mmiowl(0x032502FF, CRT_VERTICAL_TOTAL);
		smi_mmiowl(0x00060302, CRT_VERTICAL_SYNC);
		smi_mmiowl(0x007FF800, CRT_HWC_ADDRESS);
		smi_mmiowl(0x00010305, CRT_DISPLAY_CTRL);
		smi_mmiowl(0x00000001, MISC_CTRL);
	}
}

/*
 * Unmap in the memory mapped IO registers
 *
 */

static void __devinit smi_unmap_mmio(void)
{
	if (SMIRegs) {
		iounmap(SMIRegs);
		SMIRegs = NULL;
	}
}


/*
 * Unmap in the screen memory
 *
 */
static void __devinit smi_unmap_smem(void)
{
	if (SMILFB) {
		iounmap(SMILFB);
		SMILFB = NULL;
	}
}

static void vgxfb_setup(char *options)
{

	if (!options || !*options)
		return;

	/* The only thing I'm looking for right now is to disable a
	 * CRT output that mirrors the panel display.
	 */
	if (strcmp(options, "no_crt") == 0)
		crt_out = 0;

	return;
}

static struct fb_ops smifb_ops = {
	.owner =		THIS_MODULE,
	.fb_setcolreg =		smi_setcolreg,
	.fb_fillrect =		cfb_fillrect,
	.fb_copyarea =		cfb_copyarea,
	.fb_imageblit =		cfb_imageblit,
};

static int __devinit vgx_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int err;

	/* Enable the chip.
	*/
	err = pci_enable_device(dev);
	if (err)
		return err;


	/* Set up MMIO space.
	*/
	smifb_fix.mmio_start = pci_resource_start(dev,1);
	smifb_fix.mmio_len = 0x00200000;
	SMIRegs = ioremap(smifb_fix.mmio_start, smifb_fix.mmio_len);

	/* Set up framebuffer.  It's a 64M space, various amount of
	 * internal memory.  I don't know how to determine the real
	 * amount of memory (yet).
	 */
	smifb_fix.smem_start = pci_resource_start(dev,0);
	smifb_fix.smem_len = 0x00800000;
	SMILFB = ioremap(smifb_fix.smem_start, smifb_fix.smem_len);

	memset_io(SMILFB, 0, smifb_fix.smem_len);

	info.screen_base = SMILFB;
	info.fbops = &smifb_ops;
	info.fix = smifb_fix;

	info.flags = FBINFO_FLAG_DEFAULT;

	info.pseudo_palette = kmalloc(sizeof(u32) * 16, GFP_KERNEL);
	if (!info.pseudo_palette) {
		return -ENOMEM;
	}
	memset(info.pseudo_palette, 0, sizeof(u32) *16);

	fb_alloc_cmap(&info.cmap,256,0);

	smi_setmode();

	info.var = smifb_var;

	if (register_framebuffer(&info) < 0)
		goto failed;

	return 0;

failed:
	smi_unmap_smem();
	smi_unmap_mmio();

	return err;
}

static void __devexit vgx_pci_remove(struct pci_dev *dev)
{
	unregister_framebuffer(&info);
	smi_unmap_smem();
	smi_unmap_mmio();
}

static struct pci_device_id vgx_devices[] = {
	{PCI_VENDOR_ID_SILICON_MOTION, PCI_DEVICE_ID_SM501_VOYAGER_GX_REV_AA,
	 PCI_ANY_ID, PCI_ANY_ID},
	{PCI_VENDOR_ID_SILICON_MOTION, PCI_DEVICE_ID_SM501_VOYAGER_GX_REV_B,
	 PCI_ANY_ID, PCI_ANY_ID},
	{0}
};

MODULE_DEVICE_TABLE(pci, vgx_devices);

static struct pci_driver vgxfb_pci_driver = {
	.name	= "vgxfb",
	.id_table= vgx_devices,
	.probe	= vgx_pci_probe,
	.remove	= __devexit_p(vgx_pci_remove),
};

static int __init vgxfb_init(void)
{
	char *option = NULL;

	if (fb_get_options("vgxfb", &option))
		return -ENODEV;
	vgxfb_setup(option);

	printk("Silicon Motion Inc. VOYAGER Init complete.\n");
	return pci_module_init(&vgxfb_pci_driver);
}

static void __exit vgxfb_exit(void)
{
	pci_unregister_driver(&vgxfb_pci_driver);
}

module_init(vgxfb_init);
module_exit(vgxfb_exit);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("Framebuffer driver for SMI Voyager");
MODULE_LICENSE("GPL");
