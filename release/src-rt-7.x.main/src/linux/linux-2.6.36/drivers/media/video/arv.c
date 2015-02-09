/*
 * Colour AR M64278(VGA) driver for Video4Linux
 *
 * Copyright (C) 2003	Takeo Takahashi <takahashi.takeo@renesas.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Some code is taken from AR driver sample program for M3T-M32700UT.
 *
 * AR driver sample (M32R SDK):
 *     Copyright (c) 2003 RENESAS TECHNOROGY CORPORATION
 *     AND RENESAS SOLUTIONS CORPORATION
 *     All Rights Reserved.
 *
 * 2003-09-01:	Support w3cam by Takeo Takahashi
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <linux/mutex.h>

#include <asm/uaccess.h>
#include <asm/m32r.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/byteorder.h>

#define DEBUG(n, args...)
#define CHECK_LOST	0

/*
 * USE_INT is always 0, interrupt mode is not available
 * on linux due to lack of speed
 */
#define USE_INT		0	/* Don't modify */

#define VERSION	"0.04"

#define ar_inl(addr) 		inl((unsigned long)(addr))
#define ar_outl(val, addr)	outl((unsigned long)(val), (unsigned long)(addr))

extern struct cpuinfo_m32r	boot_cpu_data;

/*
 * CCD pixel size
 *	Note that M32700UT does not support CIF mode, but QVGA is
 *	supported by M32700UT hardware using VGA mode of AR LSI.
 *
 * 	Supported: VGA  (Normal mode, Interlace mode)
 *		   QVGA (Always Interlace mode of VGA)
 *
 */
#define AR_WIDTH_VGA		640
#define AR_HEIGHT_VGA		480
#define AR_WIDTH_QVGA		320
#define AR_HEIGHT_QVGA		240
#define MIN_AR_WIDTH		AR_WIDTH_QVGA
#define MIN_AR_HEIGHT		AR_HEIGHT_QVGA
#define MAX_AR_WIDTH		AR_WIDTH_VGA
#define MAX_AR_HEIGHT		AR_HEIGHT_VGA

/* bits & bytes per pixel */
#define AR_BITS_PER_PIXEL	16
#define AR_BYTES_PER_PIXEL	(AR_BITS_PER_PIXEL / 8)

/* line buffer size */
#define AR_LINE_BYTES_VGA	(AR_WIDTH_VGA * AR_BYTES_PER_PIXEL)
#define AR_LINE_BYTES_QVGA	(AR_WIDTH_QVGA * AR_BYTES_PER_PIXEL)
#define MAX_AR_LINE_BYTES	AR_LINE_BYTES_VGA

/* frame size & type */
#define AR_FRAME_BYTES_VGA \
	(AR_WIDTH_VGA * AR_HEIGHT_VGA * AR_BYTES_PER_PIXEL)
#define AR_FRAME_BYTES_QVGA \
	(AR_WIDTH_QVGA * AR_HEIGHT_QVGA * AR_BYTES_PER_PIXEL)
#define MAX_AR_FRAME_BYTES \
	(MAX_AR_WIDTH * MAX_AR_HEIGHT * AR_BYTES_PER_PIXEL)

#define AR_MAX_FRAME		15

/* capture size */
#define AR_SIZE_VGA		0
#define AR_SIZE_QVGA		1

/* capture mode */
#define AR_MODE_INTERLACE	0
#define AR_MODE_NORMAL		1

struct ar {
	struct v4l2_device v4l2_dev;
	struct video_device vdev;
	unsigned int start_capture;	/* duaring capture in INT. mode. */
#if USE_INT
	unsigned char *line_buff;	/* DMA line buffer */
#endif
	unsigned char *frame[MAX_AR_HEIGHT];	/* frame data */
	short size;			/* capture size */
	short mode;			/* capture mode */
	int width, height;
	int frame_bytes, line_bytes;
	wait_queue_head_t wait;
	struct mutex lock;
};

static struct ar ardev;

static int video_nr = -1;	/* video device number (first free) */
static unsigned char yuv[MAX_AR_FRAME_BYTES];

/* module parameters */
/* default frequency */
#define DEFAULT_FREQ	50	/* 50 or 75 (MHz) is available as BCLK */
static int freq = DEFAULT_FREQ;	/* BCLK: available 50 or 70 (MHz) */
static int vga;			/* default mode(0:QVGA mode, other:VGA mode) */
static int vga_interlace;	/* 0 is normal mode for, else interlace mode */
module_param(freq, int, 0);
module_param(vga, int, 0);
module_param(vga_interlace, int, 0);

static void wait_for_vsync(void)
{
	while (ar_inl(ARVCR0) & ARVCR0_VDS)	/* wait for VSYNC */
		cpu_relax();
	while (!(ar_inl(ARVCR0) & ARVCR0_VDS))	/* wait for VSYNC */
		cpu_relax();
}

static void wait_acknowledge(void)
{
	int i;

	for (i = 0; i < 1000; i++)
		cpu_relax();
	while (ar_inl(PLDI2CSTS) & PLDI2CSTS_NOACK)
		cpu_relax();
}

/*******************************************************************
 * I2C functions
 *******************************************************************/
static void iic(int n, unsigned long addr, unsigned long data1, unsigned long data2,
	 unsigned long data3)
{
	int i;

	/* Slave Address */
	ar_outl(addr, PLDI2CDATA);
	wait_for_vsync();

	/* Start */
	ar_outl(1, PLDI2CCND);
	wait_acknowledge();

	/* Transfer data 1 */
	ar_outl(data1, PLDI2CDATA);
	wait_for_vsync();
	ar_outl(PLDI2CSTEN_STEN, PLDI2CSTEN);
	wait_acknowledge();

	/* Transfer data 2 */
	ar_outl(data2, PLDI2CDATA);
	wait_for_vsync();
	ar_outl(PLDI2CSTEN_STEN, PLDI2CSTEN);
	wait_acknowledge();

	if (n == 3) {
		/* Transfer data 3 */
		ar_outl(data3, PLDI2CDATA);
		wait_for_vsync();
		ar_outl(PLDI2CSTEN_STEN, PLDI2CSTEN);
		wait_acknowledge();
	}

	/* Stop */
	for (i = 0; i < 100; i++)
		cpu_relax();
	ar_outl(2, PLDI2CCND);
	ar_outl(2, PLDI2CCND);

	while (ar_inl(PLDI2CSTS) & PLDI2CSTS_BB)
		cpu_relax();
}


static void init_iic(void)
{
	DEBUG(1, "init_iic:\n");

	/*
	 * ICU Setting (iic)
	 */
	/* I2C Setting */
	ar_outl(0x0, PLDI2CCR);      	/* I2CCR Disable                   */
	ar_outl(0x0300, PLDI2CMOD); 	/* I2CMOD ACK/8b-data/7b-addr/auto */
	ar_outl(0x1, PLDI2CACK);	/* I2CACK ACK                      */

	/* I2C CLK */
	/* 50MH-100k */
	if (freq == 75)
		ar_outl(369, PLDI2CFREQ);	/* BCLK = 75MHz */
	else if (freq == 50)
		ar_outl(244, PLDI2CFREQ);	/* BCLK = 50MHz */
	else
		ar_outl(244, PLDI2CFREQ);	/* default: BCLK = 50MHz */
	ar_outl(0x1, PLDI2CCR); 	/* I2CCR Enable */
}

/**************************************************************************
 *
 * Video4Linux Interface functions
 *
 **************************************************************************/

static inline void disable_dma(void)
{
	ar_outl(0x8000, M32R_DMAEN_PORTL);	/* disable DMA0 */
}

static inline void enable_dma(void)
{
	ar_outl(0x8080, M32R_DMAEN_PORTL);	/* enable DMA0 */
}

static inline void clear_dma_status(void)
{
	ar_outl(0x8000, M32R_DMAEDET_PORTL);	/* clear status */
}

static void wait_for_vertical_sync(struct ar *ar, int exp_line)
{
#if CHECK_LOST
	int tmout = 10000;
	int l;

	/*
	 * check HCOUNT because we cannot check vertical sync.
	 */
	for (; tmout >= 0; tmout--) {
		l = ar_inl(ARVHCOUNT);
		if (l == exp_line)
			break;
	}
	if (tmout < 0)
		v4l2_err(&ar->v4l2_dev, "lost %d -> %d\n", exp_line, l);
#else
	while (ar_inl(ARVHCOUNT) != exp_line)
		cpu_relax();
#endif
}

static ssize_t ar_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct ar *ar = video_drvdata(file);
	long ret = ar->frame_bytes;		/* return read bytes */
	unsigned long arvcr1 = 0;
	unsigned long flags;
	unsigned char *p;
	int h, w;
	unsigned char *py, *pu, *pv;
#if !USE_INT
	int l;
#endif

	DEBUG(1, "ar_read()\n");

	if (ar->size == AR_SIZE_QVGA)
		arvcr1 |= ARVCR1_QVGA;
	if (ar->mode == AR_MODE_NORMAL)
		arvcr1 |= ARVCR1_NORMAL;

	mutex_lock(&ar->lock);

#if USE_INT
	local_irq_save(flags);
	disable_dma();
	ar_outl(0xa1871300, M32R_DMA0CR0_PORTL);
	ar_outl(0x01000000, M32R_DMA0CR1_PORTL);

	/* set AR FIFO address as source(BSEL5) */
	ar_outl(ARDATA32, M32R_DMA0CSA_PORTL);
	ar_outl(ARDATA32, M32R_DMA0RSA_PORTL);
	ar_outl(ar->line_buff, M32R_DMA0CDA_PORTL);	/* destination addr. */
	ar_outl(ar->line_buff, M32R_DMA0RDA_PORTL); 	/* reload address */
	ar_outl(ar->line_bytes, M32R_DMA0CBCUT_PORTL); 	/* byte count (bytes) */
	ar_outl(ar->line_bytes, M32R_DMA0RBCUT_PORTL); 	/* reload count (bytes) */

	/*
	 * Okay, kick AR LSI to invoke an interrupt
	 */
	ar->start_capture = 0;
	ar_outl(arvcr1 | ARVCR1_HIEN, ARVCR1);
	local_irq_restore(flags);
	/* .... AR interrupts .... */
	interruptible_sleep_on(&ar->wait);
	if (signal_pending(current)) {
		printk(KERN_ERR "arv: interrupted while get frame data.\n");
		ret = -EINTR;
		goto out_up;
	}
#else	/* ! USE_INT */
	/* polling */
	ar_outl(arvcr1, ARVCR1);
	disable_dma();
	ar_outl(0x8000, M32R_DMAEDET_PORTL);
	ar_outl(0xa0861300, M32R_DMA0CR0_PORTL);
	ar_outl(0x01000000, M32R_DMA0CR1_PORTL);
	ar_outl(ARDATA32, M32R_DMA0CSA_PORTL);
	ar_outl(ARDATA32, M32R_DMA0RSA_PORTL);
	ar_outl(ar->line_bytes, M32R_DMA0CBCUT_PORTL);
	ar_outl(ar->line_bytes, M32R_DMA0RBCUT_PORTL);

	local_irq_save(flags);
	while (ar_inl(ARVHCOUNT) != 0)		/* wait for 0 */
		cpu_relax();
	if (ar->mode == AR_MODE_INTERLACE && ar->size == AR_SIZE_VGA) {
		for (h = 0; h < ar->height; h++) {
			wait_for_vertical_sync(ar, h);
			if (h < (AR_HEIGHT_VGA/2))
				l = h << 1;
			else
				l = (((h - (AR_HEIGHT_VGA/2)) << 1) + 1);
			ar_outl(virt_to_phys(ar->frame[l]), M32R_DMA0CDA_PORTL);
			enable_dma();
			while (!(ar_inl(M32R_DMAEDET_PORTL) & 0x8000))
				cpu_relax();
			disable_dma();
			clear_dma_status();
			ar_outl(0xa0861300, M32R_DMA0CR0_PORTL);
		}
	} else {
		for (h = 0; h < ar->height; h++) {
			wait_for_vertical_sync(ar, h);
			ar_outl(virt_to_phys(ar->frame[h]), M32R_DMA0CDA_PORTL);
			enable_dma();
			while (!(ar_inl(M32R_DMAEDET_PORTL) & 0x8000))
				cpu_relax();
			disable_dma();
			clear_dma_status();
			ar_outl(0xa0861300, M32R_DMA0CR0_PORTL);
		}
	}
	local_irq_restore(flags);
#endif	/* ! USE_INT */

	/*
	 * convert YUV422 to YUV422P
	 * 	+--------------------+
	 *	|  Y0,Y1,...	     |
	 *	|  ..............Yn  |
	 *	+--------------------+
	 *	|  U0,U1,........Un  |
	 *	+--------------------+
	 *	|  V0,V1,........Vn  |
	 *	+--------------------+
	 */
	py = yuv;
	pu = py + (ar->frame_bytes / 2);
	pv = pu + (ar->frame_bytes / 4);
	for (h = 0; h < ar->height; h++) {
		p = ar->frame[h];
		for (w = 0; w < ar->line_bytes; w += 4) {
			*py++ = *p++;
			*pu++ = *p++;
			*py++ = *p++;
			*pv++ = *p++;
		}
	}
	if (copy_to_user(buf, yuv, ar->frame_bytes)) {
		v4l2_err(&ar->v4l2_dev, "failed while copy_to_user yuv.\n");
		ret = -EFAULT;
		goto out_up;
	}
	DEBUG(1, "ret = %d\n", ret);
out_up:
	mutex_unlock(&ar->lock);
	return ret;
}

static int ar_querycap(struct file *file, void  *priv,
					struct v4l2_capability *vcap)
{
	struct ar *ar = video_drvdata(file);

	strlcpy(vcap->driver, ar->vdev.name, sizeof(vcap->driver));
	strlcpy(vcap->card, "Colour AR VGA", sizeof(vcap->card));
	strlcpy(vcap->bus_info, "Platform", sizeof(vcap->bus_info));
	vcap->version = KERNEL_VERSION(0, 0, 4);
	vcap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_READWRITE;
	return 0;
}

static int ar_enum_input(struct file *file, void *fh, struct v4l2_input *vin)
{
	if (vin->index > 0)
		return -EINVAL;
	strlcpy(vin->name, "Camera", sizeof(vin->name));
	vin->type = V4L2_INPUT_TYPE_CAMERA;
	vin->audioset = 0;
	vin->tuner = 0;
	vin->std = V4L2_STD_ALL;
	vin->status = 0;
	return 0;
}

static int ar_g_input(struct file *file, void *fh, unsigned int *inp)
{
	*inp = 0;
	return 0;
}

static int ar_s_input(struct file *file, void *fh, unsigned int inp)
{
	return inp ? -EINVAL : 0;
}

static int ar_g_fmt_vid_cap(struct file *file, void *fh, struct v4l2_format *fmt)
{
	struct ar *ar = video_drvdata(file);
	struct v4l2_pix_format *pix = &fmt->fmt.pix;

	pix->width = ar->width;
	pix->height = ar->height;
	pix->pixelformat = V4L2_PIX_FMT_YUV422P;
	pix->field = (ar->mode == AR_MODE_NORMAL) ? V4L2_FIELD_NONE : V4L2_FIELD_INTERLACED;
	pix->bytesperline = ar->width;
	pix->sizeimage = 2 * ar->width * ar->height;
	/* Just a guess */
	pix->colorspace = V4L2_COLORSPACE_SMPTE170M;
	return 0;
}

static int ar_try_fmt_vid_cap(struct file *file, void *fh, struct v4l2_format *fmt)
{
	struct ar *ar = video_drvdata(file);
	struct v4l2_pix_format *pix = &fmt->fmt.pix;

	if (pix->height <= AR_HEIGHT_QVGA || pix->width <= AR_WIDTH_QVGA) {
		pix->height = AR_HEIGHT_QVGA;
		pix->width = AR_WIDTH_QVGA;
		pix->field = V4L2_FIELD_INTERLACED;
	} else {
		pix->height = AR_HEIGHT_VGA;
		pix->width = AR_WIDTH_VGA;
		pix->field = vga_interlace ? V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;
	}
	pix->pixelformat = V4L2_PIX_FMT_YUV422P;
	pix->bytesperline = ar->width;
	pix->sizeimage = 2 * ar->width * ar->height;
	/* Just a guess */
	pix->colorspace = V4L2_COLORSPACE_SMPTE170M;
	return 0;
}

static int ar_s_fmt_vid_cap(struct file *file, void *fh, struct v4l2_format *fmt)
{
	struct ar *ar = video_drvdata(file);
	struct v4l2_pix_format *pix = &fmt->fmt.pix;
	int ret = ar_try_fmt_vid_cap(file, fh, fmt);

	if (ret)
		return ret;
	mutex_lock(&ar->lock);
	ar->width = pix->width;
	ar->height = pix->height;
	if (ar->width == AR_WIDTH_VGA) {
		ar->size = AR_SIZE_VGA;
		ar->frame_bytes = AR_FRAME_BYTES_VGA;
		ar->line_bytes = AR_LINE_BYTES_VGA;
		if (vga_interlace)
			ar->mode = AR_MODE_INTERLACE;
		else
			ar->mode = AR_MODE_NORMAL;
	} else {
		ar->size = AR_SIZE_QVGA;
		ar->frame_bytes = AR_FRAME_BYTES_QVGA;
		ar->line_bytes = AR_LINE_BYTES_QVGA;
		ar->mode = AR_MODE_INTERLACE;
	}
	/* Ok we figured out what to use from our wide choice */
	mutex_unlock(&ar->lock);
	return 0;
}

static int ar_enum_fmt_vid_cap(struct file *file, void *fh, struct v4l2_fmtdesc *fmt)
{
	static struct v4l2_fmtdesc formats[] = {
		{ 0, 0, 0,
		  "YUV 4:2:2 Planar", V4L2_PIX_FMT_YUV422P,
		  { 0, 0, 0, 0 }
		},
	};
	enum v4l2_buf_type type = fmt->type;

	if (fmt->index > 0)
		return -EINVAL;

	*fmt = formats[fmt->index];
	fmt->type = type;
	return 0;
}

#if USE_INT
/*
 * Interrupt handler
 */
static void ar_interrupt(int irq, void *dev)
{
	struct ar *ar = dev;
	unsigned int line_count;
	unsigned int line_number;
	unsigned int arvcr1;

	line_count = ar_inl(ARVHCOUNT);			/* line number */
	if (ar->mode == AR_MODE_INTERLACE && ar->size == AR_SIZE_VGA) {
		/* operations for interlace mode */
		if (line_count < (AR_HEIGHT_VGA / 2)) 	/* even line */
			line_number = (line_count << 1);
		else 					/* odd line */
			line_number =
			(((line_count - (AR_HEIGHT_VGA / 2)) << 1) + 1);
	} else {
		line_number = line_count;
	}

	if (line_number == 0) {
		/*
		 * It is an interrupt for line 0.
		 * we have to start capture.
		 */
		disable_dma();
		memcpy(ar->frame[0], ar->line_buff, ar->line_bytes);
		enable_dma();
		ar->start_capture = 1;			/* during capture */
		return;
	}

	if (ar->start_capture == 1 && line_number <= (ar->height - 1)) {
		disable_dma();
		memcpy(ar->frame[line_number], ar->line_buff, ar->line_bytes);

		/*
		 * if captured all line of a frame, disable AR interrupt
		 * and wake a process up.
		 */
		if (line_number == (ar->height - 1)) { 	/* end  of line */

			ar->start_capture = 0;

			/* disable AR interrupt request */
			arvcr1 = ar_inl(ARVCR1);
			arvcr1 &= ~ARVCR1_HIEN;		/* clear int. flag */
			ar_outl(arvcr1, ARVCR1);	/* disable */
			wake_up_interruptible(&ar->wait);
		} else {
			enable_dma();
		}
	}
}
#endif

/*
 * ar_initialize()
 * 	ar_initialize() is called by video_register_device() and
 *	initializes AR LSI and peripherals.
 *
 *	-1 is returned in all failures.
 *	0 is returned in success.
 *
 */
static int ar_initialize(struct ar *ar)
{
	unsigned long cr = 0;
	int i, found = 0;

	DEBUG(1, "ar_initialize:\n");

	/*
	 * initialize AR LSI
	 */
	ar_outl(0, ARVCR0);		/* assert reset of AR LSI */
	for (i = 0; i < 0x18; i++)	/* wait for over 10 cycles @ 27MHz */
		cpu_relax();
	ar_outl(ARVCR0_RST, ARVCR0);	/* negate reset of AR LSI (enable) */
	for (i = 0; i < 0x40d; i++)	/* wait for over 420 cycles @ 27MHz */
		cpu_relax();

	/* AR uses INT3 of CPU as interrupt pin. */
	ar_outl(ARINTSEL_INT3, ARINTSEL);

	if (ar->size == AR_SIZE_QVGA)
		cr |= ARVCR1_QVGA;
	if (ar->mode == AR_MODE_NORMAL)
		cr |= ARVCR1_NORMAL;
	ar_outl(cr, ARVCR1);

	/*
	 * Initialize IIC so that CPU can communicate with AR LSI,
	 * and send boot commands to AR LSI.
	 */
	init_iic();

	for (i = 0; i < 0x100000; i++) {	/* > 0xa1d10,  56ms */
		if ((ar_inl(ARVCR0) & ARVCR0_VDS)) {	/* VSYNC */
			found = 1;
			break;
		}
	}

	if (found == 0)
		return -ENODEV;

	v4l2_info(&ar->v4l2_dev, "Initializing ");

	iic(2, 0x78, 0x11, 0x01, 0x00);	/* start */
	iic(3, 0x78, 0x12, 0x00, 0x06);
	iic(3, 0x78, 0x12, 0x12, 0x30);
	iic(3, 0x78, 0x12, 0x15, 0x58);
	iic(3, 0x78, 0x12, 0x17, 0x30);
	printk(KERN_CONT ".");
	iic(3, 0x78, 0x12, 0x1a, 0x97);
	iic(3, 0x78, 0x12, 0x1b, 0xff);
	iic(3, 0x78, 0x12, 0x1c, 0xff);
	iic(3, 0x78, 0x12, 0x26, 0x10);
	iic(3, 0x78, 0x12, 0x27, 0x00);
	printk(KERN_CONT ".");
	iic(2, 0x78, 0x34, 0x02, 0x00);
	iic(2, 0x78, 0x7a, 0x10, 0x00);
	iic(2, 0x78, 0x80, 0x39, 0x00);
	iic(2, 0x78, 0x81, 0xe6, 0x00);
	iic(2, 0x78, 0x8d, 0x00, 0x00);
	printk(KERN_CONT ".");
	iic(2, 0x78, 0x8e, 0x0c, 0x00);
	iic(2, 0x78, 0x8f, 0x00, 0x00);
	iic(2, 0x78, 0x93, 0x01, 0x00);
	iic(2, 0x78, 0x94, 0xcd, 0x00);
	iic(2, 0x78, 0x95, 0x00, 0x00);
	printk(KERN_CONT ".");
	iic(2, 0x78, 0x96, 0xa0, 0x00);
	iic(2, 0x78, 0x97, 0x00, 0x00);
	iic(2, 0x78, 0x98, 0x60, 0x00);
	iic(2, 0x78, 0x99, 0x01, 0x00);
	iic(2, 0x78, 0x9a, 0x19, 0x00);
	printk(KERN_CONT ".");
	iic(2, 0x78, 0x9b, 0x02, 0x00);
	iic(2, 0x78, 0x9c, 0xe8, 0x00);
	iic(2, 0x78, 0x9d, 0x02, 0x00);
	iic(2, 0x78, 0x9e, 0x2e, 0x00);
	iic(2, 0x78, 0xb8, 0x78, 0x00);
	iic(2, 0x78, 0xba, 0x05, 0x00);
	printk(KERN_CONT ".");

	/* color correction */
	iic(3, 0x78, 0x49, 0x00, 0x95);	/* a		*/
	iic(3, 0x78, 0x49, 0x01, 0x96);	/* b		*/
	iic(3, 0x78, 0x49, 0x03, 0x85);	/* c		*/
	iic(3, 0x78, 0x49, 0x04, 0x97);	/* d		*/
	iic(3, 0x78, 0x49, 0x02, 0x7e);	/* e(Lo)	*/
	iic(3, 0x78, 0x49, 0x05, 0xa4);	/* f(Lo)	*/
	iic(3, 0x78, 0x49, 0x06, 0x04);	/* e(Hi)	*/
	iic(3, 0x78, 0x49, 0x07, 0x04);	/* e(Hi)	*/
	iic(2, 0x78, 0x48, 0x01, 0x00);	/* on=1 off=0	*/

	printk(KERN_CONT ".");
	iic(2, 0x78, 0x11, 0x00, 0x00);	/* end */
	printk(KERN_CONT " done\n");
	return 0;
}


/****************************************************************************
 *
 * Video4Linux Module functions
 *
 ****************************************************************************/

static const struct v4l2_file_operations ar_fops = {
	.owner		= THIS_MODULE,
	.read		= ar_read,
	.ioctl		= video_ioctl2,
};

static const struct v4l2_ioctl_ops ar_ioctl_ops = {
	.vidioc_querycap    		    = ar_querycap,
	.vidioc_g_input      		    = ar_g_input,
	.vidioc_s_input      		    = ar_s_input,
	.vidioc_enum_input   		    = ar_enum_input,
	.vidioc_enum_fmt_vid_cap 	    = ar_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap 		    = ar_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap  		    = ar_s_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap  	    = ar_try_fmt_vid_cap,
};

#define ALIGN4(x)	((((int)(x)) & 0x3) == 0)

static int __init ar_init(void)
{
	struct ar *ar;
	struct v4l2_device *v4l2_dev;
	int ret;
	int i;

	ar = &ardev;
	v4l2_dev = &ar->v4l2_dev;
	strlcpy(v4l2_dev->name, "arv", sizeof(v4l2_dev->name));
	v4l2_info(v4l2_dev, "Colour AR VGA driver %s\n", VERSION);

	ret = v4l2_device_register(NULL, v4l2_dev);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "Could not register v4l2_device\n");
		return ret;
	}
	ret = -EIO;

#if USE_INT
	/* allocate a DMA buffer for 1 line.  */
	ar->line_buff = kmalloc(MAX_AR_LINE_BYTES, GFP_KERNEL | GFP_DMA);
	if (ar->line_buff == NULL || !ALIGN4(ar->line_buff)) {
		v4l2_err(v4l2_dev, "buffer allocation failed for DMA.\n");
		ret = -ENOMEM;
		goto out_end;
	}
#endif
	/* allocate buffers for a frame */
	for (i = 0; i < MAX_AR_HEIGHT; i++) {
		ar->frame[i] = kmalloc(MAX_AR_LINE_BYTES, GFP_KERNEL);
		if (ar->frame[i] == NULL || !ALIGN4(ar->frame[i])) {
			v4l2_err(v4l2_dev, "buffer allocation failed for frame.\n");
			ret = -ENOMEM;
			goto out_line_buff;
		}
	}

	strlcpy(ar->vdev.name, "Colour AR VGA", sizeof(ar->vdev.name));
	ar->vdev.v4l2_dev = v4l2_dev;
	ar->vdev.fops = &ar_fops;
	ar->vdev.ioctl_ops = &ar_ioctl_ops;
	ar->vdev.release = video_device_release_empty;
	video_set_drvdata(&ar->vdev, ar);

	if (vga) {
		ar->width 	= AR_WIDTH_VGA;
		ar->height 	= AR_HEIGHT_VGA;
		ar->size 	= AR_SIZE_VGA;
		ar->frame_bytes = AR_FRAME_BYTES_VGA;
		ar->line_bytes	= AR_LINE_BYTES_VGA;
		if (vga_interlace)
			ar->mode = AR_MODE_INTERLACE;
		else
			ar->mode = AR_MODE_NORMAL;
	} else {
		ar->width 	= AR_WIDTH_QVGA;
		ar->height 	= AR_HEIGHT_QVGA;
		ar->size 	= AR_SIZE_QVGA;
		ar->frame_bytes = AR_FRAME_BYTES_QVGA;
		ar->line_bytes	= AR_LINE_BYTES_QVGA;
		ar->mode	= AR_MODE_INTERLACE;
	}
	mutex_init(&ar->lock);
	init_waitqueue_head(&ar->wait);

#if USE_INT
	if (request_irq(M32R_IRQ_INT3, ar_interrupt, 0, "arv", ar)) {
		v4l2_err("request_irq(%d) failed.\n", M32R_IRQ_INT3);
		ret = -EIO;
		goto out_irq;
	}
#endif

	if (ar_initialize(ar) != 0) {
		v4l2_err(v4l2_dev, "M64278 not found.\n");
		ret = -ENODEV;
		goto out_dev;
	}

	/*
	 * ok, we can initialize h/w according to parameters,
	 * so register video device as a frame grabber type.
	 * device is named "video[0-64]".
	 * video_register_device() initializes h/w using ar_initialize().
	 */
	if (video_register_device(&ar->vdev, VFL_TYPE_GRABBER, video_nr) != 0) {
		/* return -1, -ENFILE(full) or others */
		v4l2_err(v4l2_dev, "register video (Colour AR) failed.\n");
		ret = -ENODEV;
		goto out_dev;
	}

	v4l2_info(v4l2_dev, "%s: Found M64278 VGA (IRQ %d, Freq %dMHz).\n",
		video_device_node_name(&ar->vdev), M32R_IRQ_INT3, freq);

	return 0;

out_dev:
#if USE_INT
	free_irq(M32R_IRQ_INT3, ar);

out_irq:
#endif
	for (i = 0; i < MAX_AR_HEIGHT; i++)
		kfree(ar->frame[i]);

out_line_buff:
#if USE_INT
	kfree(ar->line_buff);

out_end:
#endif
	v4l2_device_unregister(&ar->v4l2_dev);
	return ret;
}


static int __init ar_init_module(void)
{
	freq = (boot_cpu_data.bus_clock / 1000000);
	printk(KERN_INFO "arv: Bus clock %d\n", freq);
	if (freq != 50 && freq != 75)
		freq = DEFAULT_FREQ;
	return ar_init();
}

static void __exit ar_cleanup_module(void)
{
	struct ar *ar;
	int i;

	ar = &ardev;
	video_unregister_device(&ar->vdev);
#if USE_INT
	free_irq(M32R_IRQ_INT3, ar);
#endif
	for (i = 0; i < MAX_AR_HEIGHT; i++)
		kfree(ar->frame[i]);
#if USE_INT
	kfree(ar->line_buff);
#endif
	v4l2_device_unregister(&ar->v4l2_dev);
}

module_init(ar_init_module);
module_exit(ar_cleanup_module);

MODULE_AUTHOR("Takeo Takahashi <takahashi.takeo@renesas.com>");
MODULE_DESCRIPTION("Colour AR M64278(VGA) for Video4Linux");
MODULE_LICENSE("GPL");
