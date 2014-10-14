/*
 *  linux/drivers/video/vgacon.c -- Low level VGA based console driver
 *
 *	Created 28 Sep 1997 by Geert Uytterhoeven
 *
 *	Rewritten by Martin Mares <mj@ucw.cz>, July 1998
 *
 *  This file is based on the old console.c, vga.c and vesa_blank.c drivers.
 *
 *	Copyright (C) 1991, 1992  Linus Torvalds
 *			    1995  Jay Estabrook
 *
 *	User definable mapping table and font loading by Eugene G. Crosser,
 *	<crosser@average.org>
 *
 *	Improved loadable font/UTF-8 support by H. Peter Anvin
 *	Feb-Sep 1995 <peter.anvin@linux.org>
 *
 *	Colour palette handling, by Simon Tatham
 *	17-Jun-95 <sgt20@cam.ac.uk>
 *
 *	if 512 char mode is already enabled don't re-enable it,
 *	because it causes screen to flicker, by Mitja Horvat
 *	5-May-96 <mitja.horvat@guest.arnes.si>
 *
 *	Use 2 outw instead of 4 outb_p to reduce erroneous text
 *	flashing on RHS of screen during heavy console scrolling .
 *	Oct 1996, Paul Gortmaker.
 *
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/kd.h>
#include <linux/slab.h>
#include <linux/vt_kern.h>
#include <linux/selection.h>
#include <linux/spinlock.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/screen_info.h>
#include <video/vga.h>
#include <asm/io.h>

static DEFINE_SPINLOCK(vga_lock);
static int cursor_size_lastfrom;
static int cursor_size_lastto;
static u32 vgacon_xres;
static u32 vgacon_yres;
static struct vgastate state;

#define BLANK 0x0020

#define CAN_LOAD_EGA_FONTS	/* undefine if the user must not do this */
#define CAN_LOAD_PALETTE	/* undefine if the user must not do this */

/* You really do _NOT_ want to define this, unless you have buggy
 * Trident VGA which will resize cursor when moving it between column
 * 15 & 16. If you define this and your VGA is OK, inverse bug will
 * appear.
 */
#undef TRIDENT_GLITCH
#define VGA_FONTWIDTH       8   /* VGA does not support fontwidths != 8 */
/*
 *  Interface used by the world
 */

static const char *vgacon_startup(void);
static void vgacon_init(struct vc_data *c, int init);
static void vgacon_deinit(struct vc_data *c);
static void vgacon_cursor(struct vc_data *c, int mode);
static int vgacon_switch(struct vc_data *c);
static int vgacon_blank(struct vc_data *c, int blank, int mode_switch);
static int vgacon_set_palette(struct vc_data *vc, unsigned char *table);
static int vgacon_scrolldelta(struct vc_data *c, int lines);
static int vgacon_set_origin(struct vc_data *c);
static void vgacon_save_screen(struct vc_data *c);
static int vgacon_scroll(struct vc_data *c, int t, int b, int dir,
			 int lines);
static void vgacon_invert_region(struct vc_data *c, u16 * p, int count);
static unsigned long vgacon_uni_pagedir[2];

/* Description of the hardware situation */
static int		vga_init_done		__read_mostly;
static unsigned long	vga_vram_base		__read_mostly;	/* Base of video memory */
static unsigned long	vga_vram_end		__read_mostly;	/* End of video memory */
static unsigned int	vga_vram_size		__read_mostly;	/* Size of video memory */
static u16		vga_video_port_reg	__read_mostly;	/* Video register select port */
static u16		vga_video_port_val	__read_mostly;	/* Video register value port */
static unsigned int	vga_video_num_columns;			/* Number of text columns */
static unsigned int	vga_video_num_lines;			/* Number of text lines */
static int		vga_can_do_color	__read_mostly;	/* Do we support colors? */
static unsigned int	vga_default_font_height __read_mostly;	/* Height of default screen font */
static unsigned char	vga_video_type		__read_mostly;	/* Card type */
static unsigned char	vga_hardscroll_enabled	__read_mostly;
static unsigned char	vga_hardscroll_user_enable __read_mostly = 1;
static unsigned char	vga_font_is_default = 1;
static int		vga_vesa_blanked;
static int 		vga_palette_blanked;
static int 		vga_is_gfx;
static int 		vga_512_chars;
static int 		vga_video_font_height;
static int 		vga_scan_lines		__read_mostly;
static unsigned int 	vga_rolled_over;

static int vgacon_text_mode_force;

bool vgacon_text_force(void)
{
	return vgacon_text_mode_force ? true : false;
}
EXPORT_SYMBOL(vgacon_text_force);

static int __init text_mode(char *str)
{
	vgacon_text_mode_force = 1;
	return 1;
}

/* force text mode - used by kernel modesetting */
__setup("nomodeset", text_mode);

static int __init no_scroll(char *str)
{
	/*
	 * Disabling scrollback is required for the Braillex ib80-piezo
	 * Braille reader made by F.H. Papenmeier (Germany).
	 * Use the "no-scroll" bootflag.
	 */
	vga_hardscroll_user_enable = vga_hardscroll_enabled = 0;
	return 1;
}

__setup("no-scroll", no_scroll);

/*
 * By replacing the four outb_p with two back to back outw, we can reduce
 * the window of opportunity to see text mislocated to the RHS of the
 * console during heavy scrolling activity. However there is the remote
 * possibility that some pre-dinosaur hardware won't like the back to back
 * I/O. Since the Xservers get away with it, we should be able to as well.
 */
static inline void write_vga(unsigned char reg, unsigned int val)
{
	unsigned int v1, v2;
	unsigned long flags;

	/*
	 * ddprintk might set the console position from interrupt
	 * handlers, thus the write has to be IRQ-atomic.
	 */
	spin_lock_irqsave(&vga_lock, flags);

#ifndef SLOW_VGA
	v1 = reg + (val & 0xff00);
	v2 = reg + 1 + ((val << 8) & 0xff00);
	outw(v1, vga_video_port_reg);
	outw(v2, vga_video_port_reg);
#else
	outb_p(reg, vga_video_port_reg);
	outb_p(val >> 8, vga_video_port_val);
	outb_p(reg + 1, vga_video_port_reg);
	outb_p(val & 0xff, vga_video_port_val);
#endif
	spin_unlock_irqrestore(&vga_lock, flags);
}

static inline void vga_set_mem_top(struct vc_data *c)
{
	write_vga(12, (c->vc_visible_origin - vga_vram_base) / 2);
}

#ifdef CONFIG_VGACON_SOFT_SCROLLBACK
/* software scrollback */
static void *vgacon_scrollback;
static int vgacon_scrollback_tail;
static int vgacon_scrollback_size;
static int vgacon_scrollback_rows;
static int vgacon_scrollback_cnt;
static int vgacon_scrollback_cur;
static int vgacon_scrollback_save;
static int vgacon_scrollback_restore;

static void vgacon_scrollback_init(int pitch)
{
	int rows = CONFIG_VGACON_SOFT_SCROLLBACK_SIZE * 1024/pitch;

	if (vgacon_scrollback) {
		vgacon_scrollback_cnt  = 0;
		vgacon_scrollback_tail = 0;
		vgacon_scrollback_cur  = 0;
		vgacon_scrollback_rows = rows - 1;
		vgacon_scrollback_size = rows * pitch;
	}
}

static void vgacon_scrollback_startup(void)
{
	vgacon_scrollback = kcalloc(CONFIG_VGACON_SOFT_SCROLLBACK_SIZE, 1024, GFP_NOWAIT);
	vgacon_scrollback_init(vga_video_num_columns * 2);
}

static void vgacon_scrollback_update(struct vc_data *c, int t, int count)
{
	void *p;

	if (!vgacon_scrollback_size || c->vc_num != fg_console)
		return;

	p = (void *) (c->vc_origin + t * c->vc_size_row);

	while (count--) {
		scr_memcpyw(vgacon_scrollback + vgacon_scrollback_tail,
			    p, c->vc_size_row);
		vgacon_scrollback_cnt++;
		p += c->vc_size_row;
		vgacon_scrollback_tail += c->vc_size_row;

		if (vgacon_scrollback_tail >= vgacon_scrollback_size)
			vgacon_scrollback_tail = 0;

		if (vgacon_scrollback_cnt > vgacon_scrollback_rows)
			vgacon_scrollback_cnt = vgacon_scrollback_rows;

		vgacon_scrollback_cur = vgacon_scrollback_cnt;
	}
}

static void vgacon_restore_screen(struct vc_data *c)
{
	vgacon_scrollback_save = 0;

	if (!vga_is_gfx && !vgacon_scrollback_restore) {
		scr_memcpyw((u16 *) c->vc_origin, (u16 *) c->vc_screenbuf,
			    c->vc_screenbuf_size > vga_vram_size ?
			    vga_vram_size : c->vc_screenbuf_size);
		vgacon_scrollback_restore = 1;
		vgacon_scrollback_cur = vgacon_scrollback_cnt;
	}
}

static int vgacon_scrolldelta(struct vc_data *c, int lines)
{
	int start, end, count, soff;

	if (!lines) {
		c->vc_visible_origin = c->vc_origin;
		vga_set_mem_top(c);
		return 1;
	}

	if (!vgacon_scrollback)
		return 1;

	if (!vgacon_scrollback_save) {
		vgacon_cursor(c, CM_ERASE);
		vgacon_save_screen(c);
		vgacon_scrollback_save = 1;
	}

	vgacon_scrollback_restore = 0;
	start = vgacon_scrollback_cur + lines;
	end = start + abs(lines);

	if (start < 0)
		start = 0;

	if (start > vgacon_scrollback_cnt)
		start = vgacon_scrollback_cnt;

	if (end < 0)
		end = 0;

	if (end > vgacon_scrollback_cnt)
		end = vgacon_scrollback_cnt;

	vgacon_scrollback_cur = start;
	count = end - start;
	soff = vgacon_scrollback_tail - ((vgacon_scrollback_cnt - end) *
					 c->vc_size_row);
	soff -= count * c->vc_size_row;

	if (soff < 0)
		soff += vgacon_scrollback_size;

	count = vgacon_scrollback_cnt - start;

	if (count > c->vc_rows)
		count = c->vc_rows;

	if (count) {
		int copysize;

		int diff = c->vc_rows - count;
		void *d = (void *) c->vc_origin;
		void *s = (void *) c->vc_screenbuf;

		count *= c->vc_size_row;
		/* how much memory to end of buffer left? */
		copysize = min(count, vgacon_scrollback_size - soff);
		scr_memcpyw(d, vgacon_scrollback + soff, copysize);
		d += copysize;
		count -= copysize;

		if (count) {
			scr_memcpyw(d, vgacon_scrollback, count);
			d += count;
		}

		if (diff)
			scr_memcpyw(d, s, diff * c->vc_size_row);
	} else
		vgacon_cursor(c, CM_MOVE);

	return 1;
}
#else
#define vgacon_scrollback_startup(...) do { } while (0)
#define vgacon_scrollback_init(...)    do { } while (0)
#define vgacon_scrollback_update(...)  do { } while (0)

static void vgacon_restore_screen(struct vc_data *c)
{
	if (c->vc_origin != c->vc_visible_origin)
		vgacon_scrolldelta(c, 0);
}

static int vgacon_scrolldelta(struct vc_data *c, int lines)
{
	if (!lines)		/* Turn scrollback off */
		c->vc_visible_origin = c->vc_origin;
	else {
		int margin = c->vc_size_row * 4;
		int ul, we, p, st;

		if (vga_rolled_over >
		    (c->vc_scr_end - vga_vram_base) + margin) {
			ul = c->vc_scr_end - vga_vram_base;
			we = vga_rolled_over + c->vc_size_row;
		} else {
			ul = 0;
			we = vga_vram_size;
		}
		p = (c->vc_visible_origin - vga_vram_base - ul + we) % we +
		    lines * c->vc_size_row;
		st = (c->vc_origin - vga_vram_base - ul + we) % we;
		if (st < 2 * margin)
			margin = 0;
		if (p < margin)
			p = 0;
		if (p > st - margin)
			p = st;
		c->vc_visible_origin = vga_vram_base + (p + ul) % we;
	}
	vga_set_mem_top(c);
	return 1;
}
#endif /* CONFIG_VGACON_SOFT_SCROLLBACK */

static const char *vgacon_startup(void)
{
	const char *display_desc = NULL;
	u16 saved1, saved2;
	volatile u16 *p;

	if (screen_info.orig_video_isVGA == VIDEO_TYPE_VLFB ||
	    screen_info.orig_video_isVGA == VIDEO_TYPE_EFI) {
	      no_vga:
#ifdef CONFIG_DUMMY_CONSOLE
		conswitchp = &dummy_con;
		return conswitchp->con_startup();
#else
		return NULL;
#endif
	}

	/* boot_params.screen_info initialized? */
	if ((screen_info.orig_video_mode  == 0) &&
	    (screen_info.orig_video_lines == 0) &&
	    (screen_info.orig_video_cols  == 0))
		goto no_vga;

	/* VGA16 modes are not handled by VGACON */
	if ((screen_info.orig_video_mode == 0x0D) ||	/* 320x200/4 */
	    (screen_info.orig_video_mode == 0x0E) ||	/* 640x200/4 */
	    (screen_info.orig_video_mode == 0x10) ||	/* 640x350/4 */
	    (screen_info.orig_video_mode == 0x12) ||	/* 640x480/4 */
	    (screen_info.orig_video_mode == 0x6A))	/* 800x600/4 (VESA) */
		goto no_vga;

	vga_video_num_lines = screen_info.orig_video_lines;
	vga_video_num_columns = screen_info.orig_video_cols;
	state.vgabase = NULL;

	if (screen_info.orig_video_mode == 7) {
		/* Monochrome display */
		vga_vram_base = 0xb0000;
		vga_video_port_reg = VGA_CRT_IM;
		vga_video_port_val = VGA_CRT_DM;
		if ((screen_info.orig_video_ega_bx & 0xff) != 0x10) {
			static struct resource ega_console_resource =
			    { .name = "ega", .start = 0x3B0, .end = 0x3BF };
			vga_video_type = VIDEO_TYPE_EGAM;
			vga_vram_size = 0x8000;
			display_desc = "EGA+";
			request_resource(&ioport_resource,
					 &ega_console_resource);
		} else {
			static struct resource mda1_console_resource =
			    { .name = "mda", .start = 0x3B0, .end = 0x3BB };
			static struct resource mda2_console_resource =
			    { .name = "mda", .start = 0x3BF, .end = 0x3BF };
			vga_video_type = VIDEO_TYPE_MDA;
			vga_vram_size = 0x2000;
			display_desc = "*MDA";
			request_resource(&ioport_resource,
					 &mda1_console_resource);
			request_resource(&ioport_resource,
					 &mda2_console_resource);
			vga_video_font_height = 14;
		}
	} else {
		/* If not, it is color. */
		vga_can_do_color = 1;
		vga_vram_base = 0xb8000;
		vga_video_port_reg = VGA_CRT_IC;
		vga_video_port_val = VGA_CRT_DC;
		if ((screen_info.orig_video_ega_bx & 0xff) != 0x10) {
			int i;

			vga_vram_size = 0x8000;

			if (!screen_info.orig_video_isVGA) {
				static struct resource ega_console_resource
				    = { .name = "ega", .start = 0x3C0, .end = 0x3DF };
				vga_video_type = VIDEO_TYPE_EGAC;
				display_desc = "EGA";
				request_resource(&ioport_resource,
						 &ega_console_resource);
			} else {
				static struct resource vga_console_resource
				    = { .name = "vga+", .start = 0x3C0, .end = 0x3DF };
				vga_video_type = VIDEO_TYPE_VGAC;
				display_desc = "VGA+";
				request_resource(&ioport_resource,
						 &vga_console_resource);

#ifdef VGA_CAN_DO_64KB
				/*
				 * get 64K rather than 32K of video RAM.
				 * This doesn't actually work on all "VGA"
				 * controllers (it seems like setting MM=01
				 * and COE=1 isn't necessarily a good idea)
				 */
				vga_vram_base = 0xa0000;
				vga_vram_size = 0x10000;
				outb_p(6, VGA_GFX_I);
				outb_p(6, VGA_GFX_D);
#endif
				/*
				 * Normalise the palette registers, to point
				 * the 16 screen colours to the first 16
				 * DAC entries.
				 */

				for (i = 0; i < 16; i++) {
					inb_p(VGA_IS1_RC);
					outb_p(i, VGA_ATT_W);
					outb_p(i, VGA_ATT_W);
				}
				outb_p(0x20, VGA_ATT_W);

				/*
				 * Now set the DAC registers back to their
				 * default values
				 */
				for (i = 0; i < 16; i++) {
					outb_p(color_table[i], VGA_PEL_IW);
					outb_p(default_red[i], VGA_PEL_D);
					outb_p(default_grn[i], VGA_PEL_D);
					outb_p(default_blu[i], VGA_PEL_D);
				}
			}
		} else {
			static struct resource cga_console_resource =
			    { .name = "cga", .start = 0x3D4, .end = 0x3D5 };
			vga_video_type = VIDEO_TYPE_CGA;
			vga_vram_size = 0x2000;
			display_desc = "*CGA";
			request_resource(&ioport_resource,
					 &cga_console_resource);
			vga_video_font_height = 8;
		}
	}

	vga_vram_base = VGA_MAP_MEM(vga_vram_base, vga_vram_size);
	vga_vram_end = vga_vram_base + vga_vram_size;

	/*
	 *      Find out if there is a graphics card present.
	 *      Are there smarter methods around?
	 */
	p = (volatile u16 *) vga_vram_base;
	saved1 = scr_readw(p);
	saved2 = scr_readw(p + 1);
	scr_writew(0xAA55, p);
	scr_writew(0x55AA, p + 1);
	if (scr_readw(p) != 0xAA55 || scr_readw(p + 1) != 0x55AA) {
		scr_writew(saved1, p);
		scr_writew(saved2, p + 1);
		goto no_vga;
	}
	scr_writew(0x55AA, p);
	scr_writew(0xAA55, p + 1);
	if (scr_readw(p) != 0x55AA || scr_readw(p + 1) != 0xAA55) {
		scr_writew(saved1, p);
		scr_writew(saved2, p + 1);
		goto no_vga;
	}
	scr_writew(saved1, p);
	scr_writew(saved2, p + 1);

	if (vga_video_type == VIDEO_TYPE_EGAC
	    || vga_video_type == VIDEO_TYPE_VGAC
	    || vga_video_type == VIDEO_TYPE_EGAM) {
		vga_hardscroll_enabled = vga_hardscroll_user_enable;
		vga_default_font_height = screen_info.orig_video_points;
		vga_video_font_height = screen_info.orig_video_points;
		/* This may be suboptimal but is a safe bet - go with it */
		vga_scan_lines =
		    vga_video_font_height * vga_video_num_lines;
	}

	vgacon_xres = screen_info.orig_video_cols * VGA_FONTWIDTH;
	vgacon_yres = vga_scan_lines;

	if (!vga_init_done) {
		vgacon_scrollback_startup();
		vga_init_done = 1;
	}

	return display_desc;
}

static void vgacon_init(struct vc_data *c, int init)
{
	unsigned long p;

	/*
	 * We cannot be loaded as a module, therefore init is always 1,
	 * but vgacon_init can be called more than once, and init will
	 * not be 1.
	 */
	c->vc_can_do_color = vga_can_do_color;

	/* set dimensions manually if init != 0 since vc_resize() will fail */
	if (init) {
		c->vc_cols = vga_video_num_columns;
		c->vc_rows = vga_video_num_lines;
	} else
		vc_resize(c, vga_video_num_columns, vga_video_num_lines);

	c->vc_scan_lines = vga_scan_lines;
	c->vc_font.height = vga_video_font_height;
	c->vc_complement_mask = 0x7700;
	if (vga_512_chars)
		c->vc_hi_font_mask = 0x0800;
	p = *c->vc_uni_pagedir_loc;
	if (c->vc_uni_pagedir_loc == &c->vc_uni_pagedir ||
	    !--c->vc_uni_pagedir_loc[1])
		con_free_unimap(c);
	c->vc_uni_pagedir_loc = vgacon_uni_pagedir;
	vgacon_uni_pagedir[1]++;
	if (!vgacon_uni_pagedir[0] && p)
		con_set_default_unimap(c);

	/* Only set the default if the user didn't deliberately override it */
	if (global_cursor_default == -1)
		global_cursor_default =
			!(screen_info.flags & VIDEO_FLAGS_NOCURSOR);
}

static void vgacon_deinit(struct vc_data *c)
{
	/* When closing the active console, reset video origin */
	if (CON_IS_VISIBLE(c)) {
		c->vc_visible_origin = vga_vram_base;
		vga_set_mem_top(c);
	}

	if (!--vgacon_uni_pagedir[1])
		con_free_unimap(c);
	c->vc_uni_pagedir_loc = &c->vc_uni_pagedir;
	con_set_default_unimap(c);
}

static u8 vgacon_build_attr(struct vc_data *c, u8 color, u8 intensity,
			    u8 blink, u8 underline, u8 reverse, u8 italic)
{
	u8 attr = color;

	if (vga_can_do_color) {
		if (italic)
			attr = (attr & 0xF0) | c->vc_itcolor;
		else if (underline)
			attr = (attr & 0xf0) | c->vc_ulcolor;
		else if (intensity == 0)
			attr = (attr & 0xf0) | c->vc_halfcolor;
	}
	if (reverse)
		attr =
		    ((attr) & 0x88) | ((((attr) >> 4) | ((attr) << 4)) &
				       0x77);
	if (blink)
		attr ^= 0x80;
	if (intensity == 2)
		attr ^= 0x08;
	if (!vga_can_do_color) {
		if (italic)
			attr = (attr & 0xF8) | 0x02;
		else if (underline)
			attr = (attr & 0xf8) | 0x01;
		else if (intensity == 0)
			attr = (attr & 0xf0) | 0x08;
	}
	return attr;
}

static void vgacon_invert_region(struct vc_data *c, u16 * p, int count)
{
	int col = vga_can_do_color;

	while (count--) {
		u16 a = scr_readw(p);
		if (col)
			a = ((a) & 0x88ff) | (((a) & 0x7000) >> 4) |
			    (((a) & 0x0700) << 4);
		else
			a ^= ((a & 0x0700) == 0x0100) ? 0x7000 : 0x7700;
		scr_writew(a, p++);
	}
}

static void vgacon_set_cursor_size(int xpos, int from, int to)
{
	unsigned long flags;
	int curs, cure;

#ifdef TRIDENT_GLITCH
	if (xpos < 16)
		from--, to--;
#endif

	if ((from == cursor_size_lastfrom) && (to == cursor_size_lastto))
		return;
	cursor_size_lastfrom = from;
	cursor_size_lastto = to;

	spin_lock_irqsave(&vga_lock, flags);
	if (vga_video_type >= VIDEO_TYPE_VGAC) {
		outb_p(VGA_CRTC_CURSOR_START, vga_video_port_reg);
		curs = inb_p(vga_video_port_val);
		outb_p(VGA_CRTC_CURSOR_END, vga_video_port_reg);
		cure = inb_p(vga_video_port_val);
	} else {
		curs = 0;
		cure = 0;
	}

	curs = (curs & 0xc0) | from;
	cure = (cure & 0xe0) | to;

	outb_p(VGA_CRTC_CURSOR_START, vga_video_port_reg);
	outb_p(curs, vga_video_port_val);
	outb_p(VGA_CRTC_CURSOR_END, vga_video_port_reg);
	outb_p(cure, vga_video_port_val);
	spin_unlock_irqrestore(&vga_lock, flags);
}

static void vgacon_cursor(struct vc_data *c, int mode)
{
	if (c->vc_mode != KD_TEXT)
		return;

	vgacon_restore_screen(c);

	switch (mode) {
	case CM_ERASE:
		write_vga(14, (c->vc_pos - vga_vram_base) / 2);
	        if (vga_video_type >= VIDEO_TYPE_VGAC)
			vgacon_set_cursor_size(c->vc_x, 31, 30);
		else
			vgacon_set_cursor_size(c->vc_x, 31, 31);
		break;

	case CM_MOVE:
	case CM_DRAW:
		write_vga(14, (c->vc_pos - vga_vram_base) / 2);
		switch (c->vc_cursor_type & 0x0f) {
		case CUR_UNDERLINE:
			vgacon_set_cursor_size(c->vc_x,
					       c->vc_font.height -
					       (c->vc_font.height <
						10 ? 2 : 3),
					       c->vc_font.height -
					       (c->vc_font.height <
						10 ? 1 : 2));
			break;
		case CUR_TWO_THIRDS:
			vgacon_set_cursor_size(c->vc_x,
					       c->vc_font.height / 3,
					       c->vc_font.height -
					       (c->vc_font.height <
						10 ? 1 : 2));
			break;
		case CUR_LOWER_THIRD:
			vgacon_set_cursor_size(c->vc_x,
					       (c->vc_font.height * 2) / 3,
					       c->vc_font.height -
					       (c->vc_font.height <
						10 ? 1 : 2));
			break;
		case CUR_LOWER_HALF:
			vgacon_set_cursor_size(c->vc_x,
					       c->vc_font.height / 2,
					       c->vc_font.height -
					       (c->vc_font.height <
						10 ? 1 : 2));
			break;
		case CUR_NONE:
			if (vga_video_type >= VIDEO_TYPE_VGAC)
				vgacon_set_cursor_size(c->vc_x, 31, 30);
			else
				vgacon_set_cursor_size(c->vc_x, 31, 31);
			break;
		default:
			vgacon_set_cursor_size(c->vc_x, 1,
					       c->vc_font.height);
			break;
		}
		break;
	}
}

static int vgacon_doresize(struct vc_data *c,
		unsigned int width, unsigned int height)
{
	unsigned long flags;
	unsigned int scanlines = height * c->vc_font.height;
	u8 scanlines_lo = 0, r7 = 0, vsync_end = 0, mode, max_scan;

	spin_lock_irqsave(&vga_lock, flags);

	vgacon_xres = width * VGA_FONTWIDTH;
	vgacon_yres = height * c->vc_font.height;
	if (vga_video_type >= VIDEO_TYPE_VGAC) {
		outb_p(VGA_CRTC_MAX_SCAN, vga_video_port_reg);
		max_scan = inb_p(vga_video_port_val);

		if (max_scan & 0x80)
			scanlines <<= 1;

		outb_p(VGA_CRTC_MODE, vga_video_port_reg);
		mode = inb_p(vga_video_port_val);

		if (mode & 0x04)
			scanlines >>= 1;

		scanlines -= 1;
		scanlines_lo = scanlines & 0xff;

		outb_p(VGA_CRTC_OVERFLOW, vga_video_port_reg);
		r7 = inb_p(vga_video_port_val) & ~0x42;

		if (scanlines & 0x100)
			r7 |= 0x02;
		if (scanlines & 0x200)
			r7 |= 0x40;

		/* deprotect registers */
		outb_p(VGA_CRTC_V_SYNC_END, vga_video_port_reg);
		vsync_end = inb_p(vga_video_port_val);
		outb_p(VGA_CRTC_V_SYNC_END, vga_video_port_reg);
		outb_p(vsync_end & ~0x80, vga_video_port_val);
	}

	outb_p(VGA_CRTC_H_DISP, vga_video_port_reg);
	outb_p(width - 1, vga_video_port_val);
	outb_p(VGA_CRTC_OFFSET, vga_video_port_reg);
	outb_p(width >> 1, vga_video_port_val);

	if (vga_video_type >= VIDEO_TYPE_VGAC) {
		outb_p(VGA_CRTC_V_DISP_END, vga_video_port_reg);
		outb_p(scanlines_lo, vga_video_port_val);
		outb_p(VGA_CRTC_OVERFLOW, vga_video_port_reg);
		outb_p(r7,vga_video_port_val);

		/* reprotect registers */
		outb_p(VGA_CRTC_V_SYNC_END, vga_video_port_reg);
		outb_p(vsync_end, vga_video_port_val);
	}

	spin_unlock_irqrestore(&vga_lock, flags);
	return 0;
}

static int vgacon_switch(struct vc_data *c)
{
	int x = c->vc_cols * VGA_FONTWIDTH;
	int y = c->vc_rows * c->vc_font.height;
	int rows = screen_info.orig_video_lines * vga_default_font_height/
		c->vc_font.height;
	/*
	 * We need to save screen size here as it's the only way
	 * we can spot the screen has been resized and we need to
	 * set size of freshly allocated screens ourselves.
	 */
	vga_video_num_columns = c->vc_cols;
	vga_video_num_lines = c->vc_rows;

	/* We can only copy out the size of the video buffer here,
	 * otherwise we get into VGA BIOS */

	if (!vga_is_gfx) {
		scr_memcpyw((u16 *) c->vc_origin, (u16 *) c->vc_screenbuf,
			    c->vc_screenbuf_size > vga_vram_size ?
				vga_vram_size : c->vc_screenbuf_size);

		if ((vgacon_xres != x || vgacon_yres != y) &&
		    (!(vga_video_num_columns % 2) &&
		     vga_video_num_columns <= screen_info.orig_video_cols &&
		     vga_video_num_lines <= rows))
			vgacon_doresize(c, c->vc_cols, c->vc_rows);
	}

	vgacon_scrollback_init(c->vc_size_row);
	return 0;		/* Redrawing not needed */
}

static void vga_set_palette(struct vc_data *vc, unsigned char *table)
{
	int i, j;

	vga_w(state.vgabase, VGA_PEL_MSK, 0xff);
	for (i = j = 0; i < 16; i++) {
		vga_w(state.vgabase, VGA_PEL_IW, table[i]);
		vga_w(state.vgabase, VGA_PEL_D, vc->vc_palette[j++] >> 2);
		vga_w(state.vgabase, VGA_PEL_D, vc->vc_palette[j++] >> 2);
		vga_w(state.vgabase, VGA_PEL_D, vc->vc_palette[j++] >> 2);
	}
}

static int vgacon_set_palette(struct vc_data *vc, unsigned char *table)
{
#ifdef CAN_LOAD_PALETTE
	if (vga_video_type != VIDEO_TYPE_VGAC || vga_palette_blanked
	    || !CON_IS_VISIBLE(vc))
		return -EINVAL;
	vga_set_palette(vc, table);
	return 0;
#else
	return -EINVAL;
#endif
}

/* structure holding original VGA register settings */
static struct {
	unsigned char SeqCtrlIndex;	/* Sequencer Index reg.   */
	unsigned char CrtCtrlIndex;	/* CRT-Contr. Index reg.  */
	unsigned char CrtMiscIO;	/* Miscellaneous register */
	unsigned char HorizontalTotal;	/* CRT-Controller:00h */
	unsigned char HorizDisplayEnd;	/* CRT-Controller:01h */
	unsigned char StartHorizRetrace;	/* CRT-Controller:04h */
	unsigned char EndHorizRetrace;	/* CRT-Controller:05h */
	unsigned char Overflow;	/* CRT-Controller:07h */
	unsigned char StartVertRetrace;	/* CRT-Controller:10h */
	unsigned char EndVertRetrace;	/* CRT-Controller:11h */
	unsigned char ModeControl;	/* CRT-Controller:17h */
	unsigned char ClockingMode;	/* Seq-Controller:01h */
} vga_state;

static void vga_vesa_blank(struct vgastate *state, int mode)
{
	/* save original values of VGA controller registers */
	if (!vga_vesa_blanked) {
		spin_lock_irq(&vga_lock);
		vga_state.SeqCtrlIndex = vga_r(state->vgabase, VGA_SEQ_I);
		vga_state.CrtCtrlIndex = inb_p(vga_video_port_reg);
		vga_state.CrtMiscIO = vga_r(state->vgabase, VGA_MIS_R);
		spin_unlock_irq(&vga_lock);

		outb_p(0x00, vga_video_port_reg);	/* HorizontalTotal */
		vga_state.HorizontalTotal = inb_p(vga_video_port_val);
		outb_p(0x01, vga_video_port_reg);	/* HorizDisplayEnd */
		vga_state.HorizDisplayEnd = inb_p(vga_video_port_val);
		outb_p(0x04, vga_video_port_reg);	/* StartHorizRetrace */
		vga_state.StartHorizRetrace = inb_p(vga_video_port_val);
		outb_p(0x05, vga_video_port_reg);	/* EndHorizRetrace */
		vga_state.EndHorizRetrace = inb_p(vga_video_port_val);
		outb_p(0x07, vga_video_port_reg);	/* Overflow */
		vga_state.Overflow = inb_p(vga_video_port_val);
		outb_p(0x10, vga_video_port_reg);	/* StartVertRetrace */
		vga_state.StartVertRetrace = inb_p(vga_video_port_val);
		outb_p(0x11, vga_video_port_reg);	/* EndVertRetrace */
		vga_state.EndVertRetrace = inb_p(vga_video_port_val);
		outb_p(0x17, vga_video_port_reg);	/* ModeControl */
		vga_state.ModeControl = inb_p(vga_video_port_val);
		vga_state.ClockingMode = vga_rseq(state->vgabase, VGA_SEQ_CLOCK_MODE);
	}

	/* assure that video is enabled */
	/* "0x20" is VIDEO_ENABLE_bit in register 01 of sequencer */
	spin_lock_irq(&vga_lock);
	vga_wseq(state->vgabase, VGA_SEQ_CLOCK_MODE, vga_state.ClockingMode | 0x20);

	/* test for vertical retrace in process.... */
	if ((vga_state.CrtMiscIO & 0x80) == 0x80)
		vga_w(state->vgabase, VGA_MIS_W, vga_state.CrtMiscIO & 0xEF);

	/*
	 * Set <End of vertical retrace> to minimum (0) and
	 * <Start of vertical Retrace> to maximum (incl. overflow)
	 * Result: turn off vertical sync (VSync) pulse.
	 */
	if (mode & VESA_VSYNC_SUSPEND) {
		outb_p(0x10, vga_video_port_reg);	/* StartVertRetrace */
		outb_p(0xff, vga_video_port_val);	/* maximum value */
		outb_p(0x11, vga_video_port_reg);	/* EndVertRetrace */
		outb_p(0x40, vga_video_port_val);	/* minimum (bits 0..3)  */
		outb_p(0x07, vga_video_port_reg);	/* Overflow */
		outb_p(vga_state.Overflow | 0x84, vga_video_port_val);	/* bits 9,10 of vert. retrace */
	}

	if (mode & VESA_HSYNC_SUSPEND) {
		/*
		 * Set <End of horizontal retrace> to minimum (0) and
		 *  <Start of horizontal Retrace> to maximum
		 * Result: turn off horizontal sync (HSync) pulse.
		 */
		outb_p(0x04, vga_video_port_reg);	/* StartHorizRetrace */
		outb_p(0xff, vga_video_port_val);	/* maximum */
		outb_p(0x05, vga_video_port_reg);	/* EndHorizRetrace */
		outb_p(0x00, vga_video_port_val);	/* minimum (0) */
	}

	/* restore both index registers */
	vga_w(state->vgabase, VGA_SEQ_I, vga_state.SeqCtrlIndex);
	outb_p(vga_state.CrtCtrlIndex, vga_video_port_reg);
	spin_unlock_irq(&vga_lock);
}

static void vga_vesa_unblank(struct vgastate *state)
{
	/* restore original values of VGA controller registers */
	spin_lock_irq(&vga_lock);
	vga_w(state->vgabase, VGA_MIS_W, vga_state.CrtMiscIO);

	outb_p(0x00, vga_video_port_reg);	/* HorizontalTotal */
	outb_p(vga_state.HorizontalTotal, vga_video_port_val);
	outb_p(0x01, vga_video_port_reg);	/* HorizDisplayEnd */
	outb_p(vga_state.HorizDisplayEnd, vga_video_port_val);
	outb_p(0x04, vga_video_port_reg);	/* StartHorizRetrace */
	outb_p(vga_state.StartHorizRetrace, vga_video_port_val);
	outb_p(0x05, vga_video_port_reg);	/* EndHorizRetrace */
	outb_p(vga_state.EndHorizRetrace, vga_video_port_val);
	outb_p(0x07, vga_video_port_reg);	/* Overflow */
	outb_p(vga_state.Overflow, vga_video_port_val);
	outb_p(0x10, vga_video_port_reg);	/* StartVertRetrace */
	outb_p(vga_state.StartVertRetrace, vga_video_port_val);
	outb_p(0x11, vga_video_port_reg);	/* EndVertRetrace */
	outb_p(vga_state.EndVertRetrace, vga_video_port_val);
	outb_p(0x17, vga_video_port_reg);	/* ModeControl */
	outb_p(vga_state.ModeControl, vga_video_port_val);
	/* ClockingMode */
	vga_wseq(state->vgabase, VGA_SEQ_CLOCK_MODE, vga_state.ClockingMode);

	/* restore index/control registers */
	vga_w(state->vgabase, VGA_SEQ_I, vga_state.SeqCtrlIndex);
	outb_p(vga_state.CrtCtrlIndex, vga_video_port_reg);
	spin_unlock_irq(&vga_lock);
}

static void vga_pal_blank(struct vgastate *state)
{
	int i;

	vga_w(state->vgabase, VGA_PEL_MSK, 0xff);
	for (i = 0; i < 16; i++) {
		vga_w(state->vgabase, VGA_PEL_IW, i);
		vga_w(state->vgabase, VGA_PEL_D, 0);
		vga_w(state->vgabase, VGA_PEL_D, 0);
		vga_w(state->vgabase, VGA_PEL_D, 0);
	}
}

static int vgacon_blank(struct vc_data *c, int blank, int mode_switch)
{
	switch (blank) {
	case 0:		/* Unblank */
		if (vga_vesa_blanked) {
			vga_vesa_unblank(&state);
			vga_vesa_blanked = 0;
		}
		if (vga_palette_blanked) {
			vga_set_palette(c, color_table);
			vga_palette_blanked = 0;
			return 0;
		}
		vga_is_gfx = 0;
		/* Tell console.c that it has to restore the screen itself */
		return 1;
	case 1:		/* Normal blanking */
	case -1:	/* Obsolete */
		if (!mode_switch && vga_video_type == VIDEO_TYPE_VGAC) {
			vga_pal_blank(&state);
			vga_palette_blanked = 1;
			return 0;
		}
		vgacon_set_origin(c);
		scr_memsetw((void *) vga_vram_base, BLANK,
			    c->vc_screenbuf_size);
		if (mode_switch)
			vga_is_gfx = 1;
		return 1;
	default:		/* VESA blanking */
		if (vga_video_type == VIDEO_TYPE_VGAC) {
			vga_vesa_blank(&state, blank - 1);
			vga_vesa_blanked = blank;
		}
		return 0;
	}
}

/*
 * PIO_FONT support.
 *
 * The font loading code goes back to the codepage package by
 * Joel Hoffman (joel@wam.umd.edu). (He reports that the original
 * reference is: "From: p. 307 of _Programmer's Guide to PC & PS/2
 * Video Systems_ by Richard Wilton. 1987.  Microsoft Press".)
 *
 * Change for certain monochrome monitors by Yury Shevchuck
 * (sizif@botik.yaroslavl.su).
 */

#ifdef CAN_LOAD_EGA_FONTS

#define colourmap 0xa0000
/* Pauline Middelink <middelin@polyware.iaf.nl> reports that we
   should use 0xA0000 for the bwmap as well.. */
#define blackwmap 0xa0000
#define cmapsz 8192

static int vgacon_do_font_op(struct vgastate *state,char *arg,int set,int ch512)
{
	unsigned short video_port_status = vga_video_port_reg + 6;
	int font_select = 0x00, beg, i;
	char *charmap;
	
	if (vga_video_type != VIDEO_TYPE_EGAM) {
		charmap = (char *) VGA_MAP_MEM(colourmap, 0);
		beg = 0x0e;
#ifdef VGA_CAN_DO_64KB
		if (vga_video_type == VIDEO_TYPE_VGAC)
			beg = 0x06;
#endif
	} else {
		charmap = (char *) VGA_MAP_MEM(blackwmap, 0);
		beg = 0x0a;
	}

#ifdef BROKEN_GRAPHICS_PROGRAMS
	/*
	 * All fonts are loaded in slot 0 (0:1 for 512 ch)
	 */

	if (!arg)
		return -EINVAL;	/* Return to default font not supported */

	vga_font_is_default = 0;
	font_select = ch512 ? 0x04 : 0x00;
#else
	/*
	 * The default font is kept in slot 0 and is never touched.
	 * A custom font is loaded in slot 2 (256 ch) or 2:3 (512 ch)
	 */

	if (set) {
		vga_font_is_default = !arg;
		if (!arg)
			ch512 = 0;	/* Default font is always 256 */
		font_select = arg ? (ch512 ? 0x0e : 0x0a) : 0x00;
	}

	if (!vga_font_is_default)
		charmap += 4 * cmapsz;
#endif

	spin_lock_irq(&vga_lock);
	/* First, the Sequencer */
	vga_wseq(state->vgabase, VGA_SEQ_RESET, 0x1);
	/* CPU writes only to map 2 */
	vga_wseq(state->vgabase, VGA_SEQ_PLANE_WRITE, 0x04);	
	/* Sequential addressing */
	vga_wseq(state->vgabase, VGA_SEQ_MEMORY_MODE, 0x07);	
	/* Clear synchronous reset */
	vga_wseq(state->vgabase, VGA_SEQ_RESET, 0x03);

	/* Now, the graphics controller, select map 2 */
	vga_wgfx(state->vgabase, VGA_GFX_PLANE_READ, 0x02);		
	/* disable odd-even addressing */
	vga_wgfx(state->vgabase, VGA_GFX_MODE, 0x00);
	/* map start at A000:0000 */
	vga_wgfx(state->vgabase, VGA_GFX_MISC, 0x00);
	spin_unlock_irq(&vga_lock);

	if (arg) {
		if (set)
			for (i = 0; i < cmapsz; i++)
				vga_writeb(arg[i], charmap + i);
		else
			for (i = 0; i < cmapsz; i++)
				arg[i] = vga_readb(charmap + i);

		/*
		 * In 512-character mode, the character map is not contiguous if
		 * we want to remain EGA compatible -- which we do
		 */

		if (ch512) {
			charmap += 2 * cmapsz;
			arg += cmapsz;
			if (set)
				for (i = 0; i < cmapsz; i++)
					vga_writeb(arg[i], charmap + i);
			else
				for (i = 0; i < cmapsz; i++)
					arg[i] = vga_readb(charmap + i);
		}
	}

	spin_lock_irq(&vga_lock);
	/* First, the sequencer, Synchronous reset */
	vga_wseq(state->vgabase, VGA_SEQ_RESET, 0x01);	
	/* CPU writes to maps 0 and 1 */
	vga_wseq(state->vgabase, VGA_SEQ_PLANE_WRITE, 0x03);
	/* odd-even addressing */
	vga_wseq(state->vgabase, VGA_SEQ_MEMORY_MODE, 0x03);
	/* Character Map Select */
	if (set)
		vga_wseq(state->vgabase, VGA_SEQ_CHARACTER_MAP, font_select);
	/* clear synchronous reset */
	vga_wseq(state->vgabase, VGA_SEQ_RESET, 0x03);

	/* Now, the graphics controller, select map 0 for CPU */
	vga_wgfx(state->vgabase, VGA_GFX_PLANE_READ, 0x00);
	/* enable even-odd addressing */
	vga_wgfx(state->vgabase, VGA_GFX_MODE, 0x10);
	/* map starts at b800:0 or b000:0 */
	vga_wgfx(state->vgabase, VGA_GFX_MISC, beg);

	/* if 512 char mode is already enabled don't re-enable it. */
	if ((set) && (ch512 != vga_512_chars)) {
		/* attribute controller */
		for (i = 0; i < MAX_NR_CONSOLES; i++) {
			struct vc_data *c = vc_cons[i].d;
			if (c && c->vc_sw == &vga_con)
				c->vc_hi_font_mask = ch512 ? 0x0800 : 0;
		}
		vga_512_chars = ch512;
		/* 256-char: enable intensity bit
		   512-char: disable intensity bit */
		inb_p(video_port_status);	/* clear address flip-flop */
		/* color plane enable register */
		vga_wattr(state->vgabase, VGA_ATC_PLANE_ENABLE, ch512 ? 0x07 : 0x0f);
		/* Wilton (1987) mentions the following; I don't know what
		   it means, but it works, and it appears necessary */
		inb_p(video_port_status);
		vga_wattr(state->vgabase, VGA_AR_ENABLE_DISPLAY, 0);	
	}
	spin_unlock_irq(&vga_lock);
	return 0;
}

/*
 * Adjust the screen to fit a font of a certain height
 */
static int vgacon_adjust_height(struct vc_data *vc, unsigned fontheight)
{
	unsigned char ovr, vde, fsr;
	int rows, maxscan, i;

	rows = vc->vc_scan_lines / fontheight;	/* Number of video rows we end up with */
	maxscan = rows * fontheight - 1;	/* Scan lines to actually display-1 */

	/* Reprogram the CRTC for the new font size
	   Note: the attempt to read the overflow register will fail
	   on an EGA, but using 0xff for the previous value appears to
	   be OK for EGA text modes in the range 257-512 scan lines, so I
	   guess we don't need to worry about it.

	   The same applies for the spill bits in the font size and cursor
	   registers; they are write-only on EGA, but it appears that they
	   are all don't care bits on EGA, so I guess it doesn't matter. */

	spin_lock_irq(&vga_lock);
	outb_p(0x07, vga_video_port_reg);	/* CRTC overflow register */
	ovr = inb_p(vga_video_port_val);
	outb_p(0x09, vga_video_port_reg);	/* Font size register */
	fsr = inb_p(vga_video_port_val);
	spin_unlock_irq(&vga_lock);

	vde = maxscan & 0xff;	/* Vertical display end reg */
	ovr = (ovr & 0xbd) +	/* Overflow register */
	    ((maxscan & 0x100) >> 7) + ((maxscan & 0x200) >> 3);
	fsr = (fsr & 0xe0) + (fontheight - 1);	/*  Font size register */

	spin_lock_irq(&vga_lock);
	outb_p(0x07, vga_video_port_reg);	/* CRTC overflow register */
	outb_p(ovr, vga_video_port_val);
	outb_p(0x09, vga_video_port_reg);	/* Font size */
	outb_p(fsr, vga_video_port_val);
	outb_p(0x12, vga_video_port_reg);	/* Vertical display limit */
	outb_p(vde, vga_video_port_val);
	spin_unlock_irq(&vga_lock);
	vga_video_font_height = fontheight;

	for (i = 0; i < MAX_NR_CONSOLES; i++) {
		struct vc_data *c = vc_cons[i].d;

		if (c && c->vc_sw == &vga_con) {
			if (CON_IS_VISIBLE(c)) {
			        /* void size to cause regs to be rewritten */
				cursor_size_lastfrom = 0;
				cursor_size_lastto = 0;
				c->vc_sw->con_cursor(c, CM_DRAW);
			}
			c->vc_font.height = fontheight;
			vc_resize(c, 0, rows);	/* Adjust console size */
		}
	}
	return 0;
}

static int vgacon_font_set(struct vc_data *c, struct console_font *font, unsigned flags)
{
	unsigned charcount = font->charcount;
	int rc;

	if (vga_video_type < VIDEO_TYPE_EGAM)
		return -EINVAL;

	if (font->width != VGA_FONTWIDTH ||
	    (charcount != 256 && charcount != 512))
		return -EINVAL;

	rc = vgacon_do_font_op(&state, font->data, 1, charcount == 512);
	if (rc)
		return rc;

	if (!(flags & KD_FONT_FLAG_DONT_RECALC))
		rc = vgacon_adjust_height(c, font->height);
	return rc;
}

static int vgacon_font_get(struct vc_data *c, struct console_font *font)
{
	if (vga_video_type < VIDEO_TYPE_EGAM)
		return -EINVAL;

	font->width = VGA_FONTWIDTH;
	font->height = c->vc_font.height;
	font->charcount = vga_512_chars ? 512 : 256;
	if (!font->data)
		return 0;
	return vgacon_do_font_op(&state, font->data, 0, vga_512_chars);
}

#else

#define vgacon_font_set NULL
#define vgacon_font_get NULL

#endif

static int vgacon_resize(struct vc_data *c, unsigned int width,
			 unsigned int height, unsigned int user)
{
	if (width % 2 || width > screen_info.orig_video_cols ||
	    height > (screen_info.orig_video_lines * vga_default_font_height)/
	    c->vc_font.height)
		/* let svgatextmode tinker with video timings and
		   return success */
		return (user) ? 0 : -EINVAL;

	if (CON_IS_VISIBLE(c) && !vga_is_gfx) /* who knows */
		vgacon_doresize(c, width, height);
	return 0;
}

static int vgacon_set_origin(struct vc_data *c)
{
	if (vga_is_gfx ||	/* We don't play origin tricks in graphic modes */
	    (console_blanked && !vga_palette_blanked))	/* Nor we write to blanked screens */
		return 0;
	c->vc_origin = c->vc_visible_origin = vga_vram_base;
	vga_set_mem_top(c);
	vga_rolled_over = 0;
	return 1;
}

static void vgacon_save_screen(struct vc_data *c)
{
	static int vga_bootup_console = 0;

	if (!vga_bootup_console) {
		/* This is a gross hack, but here is the only place we can
		 * set bootup console parameters without messing up generic
		 * console initialization routines.
		 */
		vga_bootup_console = 1;
		c->vc_x = screen_info.orig_x;
		c->vc_y = screen_info.orig_y;
	}

	/* We can't copy in more than the size of the video buffer,
	 * or we'll be copying in VGA BIOS */

	if (!vga_is_gfx)
		scr_memcpyw((u16 *) c->vc_screenbuf, (u16 *) c->vc_origin,
			    c->vc_screenbuf_size > vga_vram_size ? vga_vram_size : c->vc_screenbuf_size);
}

static int vgacon_scroll(struct vc_data *c, int t, int b, int dir,
			 int lines)
{
	unsigned long oldo;
	unsigned int delta;

	if (t || b != c->vc_rows || vga_is_gfx || c->vc_mode != KD_TEXT)
		return 0;

	if (!vga_hardscroll_enabled || lines >= c->vc_rows / 2)
		return 0;

	vgacon_restore_screen(c);
	oldo = c->vc_origin;
	delta = lines * c->vc_size_row;
	if (dir == SM_UP) {
		vgacon_scrollback_update(c, t, lines);
		if (c->vc_scr_end + delta >= vga_vram_end) {
			scr_memcpyw((u16 *) vga_vram_base,
				    (u16 *) (oldo + delta),
				    c->vc_screenbuf_size - delta);
			c->vc_origin = vga_vram_base;
			vga_rolled_over = oldo - vga_vram_base;
		} else
			c->vc_origin += delta;
		scr_memsetw((u16 *) (c->vc_origin + c->vc_screenbuf_size -
				     delta), c->vc_video_erase_char,
			    delta);
	} else {
		if (oldo - delta < vga_vram_base) {
			scr_memmovew((u16 *) (vga_vram_end -
					      c->vc_screenbuf_size +
					      delta), (u16 *) oldo,
				     c->vc_screenbuf_size - delta);
			c->vc_origin = vga_vram_end - c->vc_screenbuf_size;
			vga_rolled_over = 0;
		} else
			c->vc_origin -= delta;
		c->vc_scr_end = c->vc_origin + c->vc_screenbuf_size;
		scr_memsetw((u16 *) (c->vc_origin), c->vc_video_erase_char,
			    delta);
	}
	c->vc_scr_end = c->vc_origin + c->vc_screenbuf_size;
	c->vc_visible_origin = c->vc_origin;
	vga_set_mem_top(c);
	c->vc_pos = (c->vc_pos - oldo) + c->vc_origin;
	return 1;
}


/*
 *  The console `switch' structure for the VGA based console
 */

static int vgacon_dummy(struct vc_data *c)
{
	return 0;
}

#define DUMMY (void *) vgacon_dummy

const struct consw vga_con = {
	.owner = THIS_MODULE,
	.con_startup = vgacon_startup,
	.con_init = vgacon_init,
	.con_deinit = vgacon_deinit,
	.con_clear = DUMMY,
	.con_putc = DUMMY,
	.con_putcs = DUMMY,
	.con_cursor = vgacon_cursor,
	.con_scroll = vgacon_scroll,
	.con_bmove = DUMMY,
	.con_switch = vgacon_switch,
	.con_blank = vgacon_blank,
	.con_font_set = vgacon_font_set,
	.con_font_get = vgacon_font_get,
	.con_resize = vgacon_resize,
	.con_set_palette = vgacon_set_palette,
	.con_scrolldelta = vgacon_scrolldelta,
	.con_set_origin = vgacon_set_origin,
	.con_save_screen = vgacon_save_screen,
	.con_build_attr = vgacon_build_attr,
	.con_invert_region = vgacon_invert_region,
};

MODULE_LICENSE("GPL");
