/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2005 by Ralf Baechle (ralf@linux-mips.org)
 *
 * This will eventually go into the qemu firmware.
 */
#include <linux/init.h>
#include <linux/screen_info.h>
#include <linux/tty.h>
#include <asm/io.h>
#include <video/vga.h>

/*
 * This will eventually be done by the firmware; right now Linux assumes to
 * run on the uninitialized hardware.
 */
#undef LOAD_VGA_FONT

static unsigned char sr[8] __initdata = {	/* Sequencer */
	0x03, 0x00, 0x03, 0x04, 0x02, 0x00, 0x00, 0x00
};

static unsigned char gr[16] __initdata= {	/* Graphics Controller */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0e, 0x00,
	0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static unsigned char ar[21] __initdata= {	/* Attribute Controller */
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x0c, 0x01, 0x07, 0x13, 0x00
};

static unsigned char cr[32] __initdata= {	/* CRT Controller */
	0x91, 0x4f, 0x4f, 0x95, 0x57, 0x4f, 0xc0, 0x1f,
	0x00, 0x4f, 0x0d, 0x0e, 0x02, 0x30, 0x09, 0xb0,
	0x90, 0x83, 0x8f, 0x28, 0x1f, 0x8f, 0xc1, 0xa3,
	0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static struct rgb {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} palette[16] __initdata= {
	[ 0] = {0x00, 0x00, 0x00},
	[ 1] = {0x00, 0x00, 0x2a},
	[ 2] = {0x00, 0x2a, 0x00},
	[ 3] = {0x00, 0x2a, 0x2a},
	[ 4] = {0x2a, 0x00, 0x00},
	[ 5] = {0x2a, 0x00, 0x2a},
	[ 6] = {0x2a, 0x15, 0x00},
	[ 7] = {0x2a, 0x2a, 0x2a},
	[ 8] = {0x15, 0x15, 0x15},
	[ 9] = {0x15, 0x15, 0x3f},
	[10] = {0x15, 0x3f, 0x15},
	[11] = {0x15, 0x3f, 0x3f},
	[12] = {0x3f, 0x15, 0x15},
	[13] = {0x3f, 0x15, 0x3f},
	[14] = {0x3f, 0x3f, 0x15},
	[15] = {0x3f, 0x3f, 0x3f}

};

void __init qvga_init_ibm(void)
{
	int i;

	for (i = 0; i < 8; i++) {	/* Sequencer registers */
		outb(i, 0x3c4);
		outb(sr[i], 0x3c5);
	}

	for (i = 0; i < 16; i++) {	/* Graphics Controller registers */
		outb(i, 0x3ce);
		outb(gr[i], 0x3cf);
	}

	for (i = 0; i < 21; i++) {	/* Attribute Controller registers */
		outb(i, 0x3c0);
		outb(ar[i], 0x3c1);
	}
	outb(0x20, 0x3c0);		/* enable bit in *index* register */

	for (i = 0; i < 32; i++) {	/* CRT Controller registers */
		outb(i, 0x3d4);
		outb(cr[i], 0x3d5);
	}

	for (i = 0; i < 16; i++) {	/* palette */
		outb(i, 0x3c8);
		outb(palette[i].r, 0x3c9);
		outb(palette[i].g, 0x3c9);
		outb(palette[i].b, 0x3c9);
	}

#if 1
	 for (i = 0; i < 0x20000; i += 2)
		*(volatile unsigned short *) (0xb00a0000 + i) = 0xaaaa;
#endif
}

#ifdef LOAD_VGA_FONT
#include "/home/ralf/src/qemu/qemu-mips/vgafont.h"

static void __init
qvga_load_font(unsigned char *def, unsigned int c)
{
	volatile void *w = (volatile void *) 0xb00a0000;

	vga_wseq(NULL, 0, 1);
	vga_wseq(NULL, 2, 4);
	vga_wseq(NULL, 4, 7);
	vga_wseq(NULL, 0, 3);
	vga_wgfx(NULL, 4, 2);
	vga_wgfx(NULL, 5, 0);
	vga_wgfx(NULL, 6, 0);

	memcpy(w, def, c);

	vga_wseq(NULL, 0, 1);
	vga_wseq(NULL, 2, 3);
	vga_wseq(NULL, 4, 3);
	vga_wseq(NULL, 0, 3);
	vga_wgfx(NULL, 4, 0);
	vga_wgfx(NULL, 5, 0x10);
	vga_wgfx(NULL, 6, 0xe);
}
#endif

void __init qvga_init(void)
{
	struct screen_info *si = &screen_info;
	unsigned int h;
	int i;

#ifdef LOAD_VGA_FONT
	qvga_load_font(vgafont16, 4096);
#endif

	vga_wgfx(NULL, 5, 0x10);	/* Set odd/even mode */
	vga_wgfx(NULL, 6, 0x0c);	/* map to offset 0xb8000, text mode */
	vga_wseq(NULL, 2, 3);		/* Planes 0 & 1 */
	vga_wseq(NULL, 3, 4);		/* Font offset */
	outb(1, VGA_MIS_W);		/* set msr to MSR_COLOR_EMULATION */
	vga_wcrt(NULL, 1, 79);		/* 80 columns */
	vga_wcrt(NULL, 9, 15);		/* 16 pixels per character */
	vga_wcrt(NULL, 0x0c, 0);	/* start address high 8 bit */
	vga_wcrt(NULL, 0x0d, 0);	/* start address low 8 bit */
	vga_wcrt(NULL, 0x13, 0x28);	/* line offset */
	vga_wcrt(NULL, 0x07, 0x1f);	/* line compare bit 8 */
	vga_wcrt(NULL, 0x09, 0x4f);	/* line compare bit 9 */
	vga_wcrt(NULL, 0x18, 0xff);	/* line compare low 8 bit */

	h = (25 * 16);
	vga_wcrt(NULL, 0x12, h);

	outb(7, 0x3d4);
	outb((inb(0x3d5) & ~0x42) | ((h >> 7) & 2) | ((h >> 3) & 0x40), 0x3d5);

	for (i = 0; i < 21; i++)	/* Attribute Controller */
		vga_wattr(NULL, i, ar[i]);
	outb(0x20, 0x3c0);		/* Set bit 5 in Attribute Controller */
					/* index ...  VGA is so stupid I want */
					/* to cry all day ... */
	outb(0, VGA_PEL_IW);
	for (i = 0; i < 16; i++) {	/* palette */
		outb(palette[i].r, VGA_PEL_D);
		outb(palette[i].g, VGA_PEL_D);
		outb(palette[i].b, VGA_PEL_D);
	}

	si->orig_x		= 0; 			/* Cursor x position */
	si->orig_y		= 0;			/* Cursor y position */
	si->orig_video_cols	= 80;			/* Columns */
	si->orig_video_lines	= 25;			/* Lines */
	si->orig_video_isVGA	= VIDEO_TYPE_VGAC;	/* Card type */
	si->orig_video_points	= 16;

#if 0
	for (i = 0; i < 80; i += 2)
		//*(volatile unsigned short *) (0xb00b8000 + i) = 0x0100 | 'A';
		scr_writew(0x0100 | 'A', (volatile unsigned short *) (0xb00b8000 + i));
	while (1);
#endif
}
