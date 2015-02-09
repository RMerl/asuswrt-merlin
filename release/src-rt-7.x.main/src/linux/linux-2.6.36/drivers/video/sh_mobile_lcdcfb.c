/*
 * SuperH Mobile LCDC Framebuffer
 *
 * Copyright (c) 2008 Magnus Damm
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/ioctl.h>
#include <linux/slab.h>
#include <video/sh_mobile_lcdc.h>
#include <asm/atomic.h>

#define PALETTE_NR 16
#define SIDE_B_OFFSET 0x1000
#define MIRROR_OFFSET 0x2000

/* shared registers */
#define _LDDCKR 0x410
#define _LDDCKSTPR 0x414
#define _LDINTR 0x468
#define _LDSR 0x46c
#define _LDCNT1R 0x470
#define _LDCNT2R 0x474
#define _LDRCNTR 0x478
#define _LDDDSR 0x47c
#define _LDDWD0R 0x800
#define _LDDRDR 0x840
#define _LDDWAR 0x900
#define _LDDRAR 0x904

/* shared registers and their order for context save/restore */
static int lcdc_shared_regs[] = {
	_LDDCKR,
	_LDDCKSTPR,
	_LDINTR,
	_LDDDSR,
	_LDCNT1R,
	_LDCNT2R,
};
#define NR_SHARED_REGS ARRAY_SIZE(lcdc_shared_regs)

/* per-channel registers */
enum { LDDCKPAT1R, LDDCKPAT2R, LDMT1R, LDMT2R, LDMT3R, LDDFR, LDSM1R,
       LDSM2R, LDSA1R, LDMLSR, LDHCNR, LDHSYNR, LDVLNR, LDVSYNR, LDPMR,
       LDHAJR,
       NR_CH_REGS };

static unsigned long lcdc_offs_mainlcd[NR_CH_REGS] = {
	[LDDCKPAT1R] = 0x400,
	[LDDCKPAT2R] = 0x404,
	[LDMT1R] = 0x418,
	[LDMT2R] = 0x41c,
	[LDMT3R] = 0x420,
	[LDDFR] = 0x424,
	[LDSM1R] = 0x428,
	[LDSM2R] = 0x42c,
	[LDSA1R] = 0x430,
	[LDMLSR] = 0x438,
	[LDHCNR] = 0x448,
	[LDHSYNR] = 0x44c,
	[LDVLNR] = 0x450,
	[LDVSYNR] = 0x454,
	[LDPMR] = 0x460,
	[LDHAJR] = 0x4a0,
};

static unsigned long lcdc_offs_sublcd[NR_CH_REGS] = {
	[LDDCKPAT1R] = 0x408,
	[LDDCKPAT2R] = 0x40c,
	[LDMT1R] = 0x600,
	[LDMT2R] = 0x604,
	[LDMT3R] = 0x608,
	[LDDFR] = 0x60c,
	[LDSM1R] = 0x610,
	[LDSM2R] = 0x614,
	[LDSA1R] = 0x618,
	[LDMLSR] = 0x620,
	[LDHCNR] = 0x624,
	[LDHSYNR] = 0x628,
	[LDVLNR] = 0x62c,
	[LDVSYNR] = 0x630,
	[LDPMR] = 0x63c,
};

#define START_LCDC	0x00000001
#define LCDC_RESET	0x00000100
#define DISPLAY_BEU	0x00000008
#define LCDC_ENABLE	0x00000001
#define LDINTR_FE	0x00000400
#define LDINTR_VSE	0x00000200
#define LDINTR_VEE	0x00000100
#define LDINTR_FS	0x00000004
#define LDINTR_VSS	0x00000002
#define LDINTR_VES	0x00000001
#define LDRCNTR_SRS	0x00020000
#define LDRCNTR_SRC	0x00010000
#define LDRCNTR_MRS	0x00000002
#define LDRCNTR_MRC	0x00000001
#define LDSR_MRS	0x00000100

struct sh_mobile_lcdc_priv;
struct sh_mobile_lcdc_chan {
	struct sh_mobile_lcdc_priv *lcdc;
	unsigned long *reg_offs;
	unsigned long ldmt1r_value;
	unsigned long enabled; /* ME and SE in LDCNT2R */
	struct sh_mobile_lcdc_chan_cfg cfg;
	u32 pseudo_palette[PALETTE_NR];
	unsigned long saved_ch_regs[NR_CH_REGS];
	struct fb_info *info;
	dma_addr_t dma_handle;
	struct fb_deferred_io defio;
	struct scatterlist *sglist;
	unsigned long frame_end;
	unsigned long pan_offset;
	wait_queue_head_t frame_end_wait;
	struct completion vsync_completion;
};

struct sh_mobile_lcdc_priv {
	void __iomem *base;
	int irq;
	atomic_t hw_usecnt;
	struct device *dev;
	struct clk *dot_clk;
	unsigned long lddckr;
	struct sh_mobile_lcdc_chan ch[2];
	struct notifier_block notifier;
	unsigned long saved_shared_regs[NR_SHARED_REGS];
	int started;
};

static bool banked(int reg_nr)
{
	switch (reg_nr) {
	case LDMT1R:
	case LDMT2R:
	case LDMT3R:
	case LDDFR:
	case LDSM1R:
	case LDSA1R:
	case LDMLSR:
	case LDHCNR:
	case LDHSYNR:
	case LDVLNR:
	case LDVSYNR:
		return true;
	}
	return false;
}

static void lcdc_write_chan(struct sh_mobile_lcdc_chan *chan,
			    int reg_nr, unsigned long data)
{
	iowrite32(data, chan->lcdc->base + chan->reg_offs[reg_nr]);
	if (banked(reg_nr))
		iowrite32(data, chan->lcdc->base + chan->reg_offs[reg_nr] +
			  SIDE_B_OFFSET);
}

static void lcdc_write_chan_mirror(struct sh_mobile_lcdc_chan *chan,
			    int reg_nr, unsigned long data)
{
	iowrite32(data, chan->lcdc->base + chan->reg_offs[reg_nr] +
		  MIRROR_OFFSET);
}

static unsigned long lcdc_read_chan(struct sh_mobile_lcdc_chan *chan,
				    int reg_nr)
{
	return ioread32(chan->lcdc->base + chan->reg_offs[reg_nr]);
}

static void lcdc_write(struct sh_mobile_lcdc_priv *priv,
		       unsigned long reg_offs, unsigned long data)
{
	iowrite32(data, priv->base + reg_offs);
}

static unsigned long lcdc_read(struct sh_mobile_lcdc_priv *priv,
			       unsigned long reg_offs)
{
	return ioread32(priv->base + reg_offs);
}

static void lcdc_wait_bit(struct sh_mobile_lcdc_priv *priv,
			  unsigned long reg_offs,
			  unsigned long mask, unsigned long until)
{
	while ((lcdc_read(priv, reg_offs) & mask) != until)
		cpu_relax();
}

static int lcdc_chan_is_sublcd(struct sh_mobile_lcdc_chan *chan)
{
	return chan->cfg.chan == LCDC_CHAN_SUBLCD;
}

static void lcdc_sys_write_index(void *handle, unsigned long data)
{
	struct sh_mobile_lcdc_chan *ch = handle;

	lcdc_write(ch->lcdc, _LDDWD0R, data | 0x10000000);
	lcdc_wait_bit(ch->lcdc, _LDSR, 2, 0);
	lcdc_write(ch->lcdc, _LDDWAR, 1 | (lcdc_chan_is_sublcd(ch) ? 2 : 0));
	lcdc_wait_bit(ch->lcdc, _LDSR, 2, 0);
}

static void lcdc_sys_write_data(void *handle, unsigned long data)
{
	struct sh_mobile_lcdc_chan *ch = handle;

	lcdc_write(ch->lcdc, _LDDWD0R, data | 0x11000000);
	lcdc_wait_bit(ch->lcdc, _LDSR, 2, 0);
	lcdc_write(ch->lcdc, _LDDWAR, 1 | (lcdc_chan_is_sublcd(ch) ? 2 : 0));
	lcdc_wait_bit(ch->lcdc, _LDSR, 2, 0);
}

static unsigned long lcdc_sys_read_data(void *handle)
{
	struct sh_mobile_lcdc_chan *ch = handle;

	lcdc_write(ch->lcdc, _LDDRDR, 0x01000000);
	lcdc_wait_bit(ch->lcdc, _LDSR, 2, 0);
	lcdc_write(ch->lcdc, _LDDRAR, 1 | (lcdc_chan_is_sublcd(ch) ? 2 : 0));
	udelay(1);
	lcdc_wait_bit(ch->lcdc, _LDSR, 2, 0);

	return lcdc_read(ch->lcdc, _LDDRDR) & 0x3ffff;
}

struct sh_mobile_lcdc_sys_bus_ops sh_mobile_lcdc_sys_bus_ops = {
	lcdc_sys_write_index,
	lcdc_sys_write_data,
	lcdc_sys_read_data,
};

static void sh_mobile_lcdc_clk_on(struct sh_mobile_lcdc_priv *priv)
{
	if (atomic_inc_and_test(&priv->hw_usecnt)) {
		pm_runtime_get_sync(priv->dev);
		if (priv->dot_clk)
			clk_enable(priv->dot_clk);
	}
}

static void sh_mobile_lcdc_clk_off(struct sh_mobile_lcdc_priv *priv)
{
	if (atomic_sub_return(1, &priv->hw_usecnt) == -1) {
		if (priv->dot_clk)
			clk_disable(priv->dot_clk);
		pm_runtime_put(priv->dev);
	}
}

static int sh_mobile_lcdc_sginit(struct fb_info *info,
				  struct list_head *pagelist)
{
	struct sh_mobile_lcdc_chan *ch = info->par;
	unsigned int nr_pages_max = info->fix.smem_len >> PAGE_SHIFT;
	struct page *page;
	int nr_pages = 0;

	sg_init_table(ch->sglist, nr_pages_max);

	list_for_each_entry(page, pagelist, lru)
		sg_set_page(&ch->sglist[nr_pages++], page, PAGE_SIZE, 0);

	return nr_pages;
}

static void sh_mobile_lcdc_deferred_io(struct fb_info *info,
				       struct list_head *pagelist)
{
	struct sh_mobile_lcdc_chan *ch = info->par;
	struct sh_mobile_lcdc_board_cfg	*bcfg = &ch->cfg.board_cfg;

	/* enable clocks before accessing hardware */
	sh_mobile_lcdc_clk_on(ch->lcdc);

	/*
	 * It's possible to get here without anything on the pagelist via
	 * sh_mobile_lcdc_deferred_io_touch() or via a userspace fsync()
	 * invocation. In the former case, the acceleration routines are
	 * stepped in to when using the framebuffer console causing the
	 * workqueue to be scheduled without any dirty pages on the list.
	 *
	 * Despite this, a panel update is still needed given that the
	 * acceleration routines have their own methods for writing in
	 * that still need to be updated.
	 *
	 * The fsync() and empty pagelist case could be optimized for,
	 * but we don't bother, as any application exhibiting such
	 * behaviour is fundamentally broken anyways.
	 */
	if (!list_empty(pagelist)) {
		unsigned int nr_pages = sh_mobile_lcdc_sginit(info, pagelist);

		/* trigger panel update */
		dma_map_sg(info->dev, ch->sglist, nr_pages, DMA_TO_DEVICE);
		if (bcfg->start_transfer)
			bcfg->start_transfer(bcfg->board_data, ch,
					     &sh_mobile_lcdc_sys_bus_ops);
		lcdc_write_chan(ch, LDSM2R, 1);
		dma_unmap_sg(info->dev, ch->sglist, nr_pages, DMA_TO_DEVICE);
	} else {
		if (bcfg->start_transfer)
			bcfg->start_transfer(bcfg->board_data, ch,
					     &sh_mobile_lcdc_sys_bus_ops);
		lcdc_write_chan(ch, LDSM2R, 1);
	}
}

static void sh_mobile_lcdc_deferred_io_touch(struct fb_info *info)
{
	struct fb_deferred_io *fbdefio = info->fbdefio;

	if (fbdefio)
		schedule_delayed_work(&info->deferred_work, fbdefio->delay);
}

static irqreturn_t sh_mobile_lcdc_irq(int irq, void *data)
{
	struct sh_mobile_lcdc_priv *priv = data;
	struct sh_mobile_lcdc_chan *ch;
	unsigned long tmp;
	unsigned long ldintr;
	int is_sub;
	int k;

	/* acknowledge interrupt */
	ldintr = tmp = lcdc_read(priv, _LDINTR);
	/*
	 * disable further VSYNC End IRQs, preserve all other enabled IRQs,
	 * write 0 to bits 0-6 to ack all triggered IRQs.
	 */
	tmp &= 0xffffff00 & ~LDINTR_VEE;
	lcdc_write(priv, _LDINTR, tmp);

	/* figure out if this interrupt is for main or sub lcd */
	is_sub = (lcdc_read(priv, _LDSR) & (1 << 10)) ? 1 : 0;

	/* wake up channel and disable clocks */
	for (k = 0; k < ARRAY_SIZE(priv->ch); k++) {
		ch = &priv->ch[k];

		if (!ch->enabled)
			continue;

		/* Frame Start */
		if (ldintr & LDINTR_FS) {
			if (is_sub == lcdc_chan_is_sublcd(ch)) {
				ch->frame_end = 1;
				wake_up(&ch->frame_end_wait);

				sh_mobile_lcdc_clk_off(priv);
			}
		}

		/* VSYNC End */
		if (ldintr & LDINTR_VES)
			complete(&ch->vsync_completion);
	}

	return IRQ_HANDLED;
}

static void sh_mobile_lcdc_start_stop(struct sh_mobile_lcdc_priv *priv,
				      int start)
{
	unsigned long tmp = lcdc_read(priv, _LDCNT2R);
	int k;

	/* start or stop the lcdc */
	if (start)
		lcdc_write(priv, _LDCNT2R, tmp | START_LCDC);
	else
		lcdc_write(priv, _LDCNT2R, tmp & ~START_LCDC);

	/* wait until power is applied/stopped on all channels */
	for (k = 0; k < ARRAY_SIZE(priv->ch); k++)
		if (lcdc_read(priv, _LDCNT2R) & priv->ch[k].enabled)
			while (1) {
				tmp = lcdc_read_chan(&priv->ch[k], LDPMR) & 3;
				if (start && tmp == 3)
					break;
				if (!start && tmp == 0)
					break;
				cpu_relax();
			}

	if (!start)
		lcdc_write(priv, _LDDCKSTPR, 1); /* stop dotclock */
}

static void sh_mobile_lcdc_geometry(struct sh_mobile_lcdc_chan *ch)
{
	struct fb_var_screeninfo *var = &ch->info->var;
	unsigned long h_total, hsync_pos;
	u32 tmp;

	tmp = ch->ldmt1r_value;
	tmp |= (var->sync & FB_SYNC_VERT_HIGH_ACT) ? 0 : 1 << 28;
	tmp |= (var->sync & FB_SYNC_HOR_HIGH_ACT) ? 0 : 1 << 27;
	tmp |= (ch->cfg.flags & LCDC_FLAGS_DWPOL) ? 1 << 26 : 0;
	tmp |= (ch->cfg.flags & LCDC_FLAGS_DIPOL) ? 1 << 25 : 0;
	tmp |= (ch->cfg.flags & LCDC_FLAGS_DAPOL) ? 1 << 24 : 0;
	tmp |= (ch->cfg.flags & LCDC_FLAGS_HSCNT) ? 1 << 17 : 0;
	tmp |= (ch->cfg.flags & LCDC_FLAGS_DWCNT) ? 1 << 16 : 0;
	lcdc_write_chan(ch, LDMT1R, tmp);

	/* setup SYS bus */
	lcdc_write_chan(ch, LDMT2R, ch->cfg.sys_bus_cfg.ldmt2r);
	lcdc_write_chan(ch, LDMT3R, ch->cfg.sys_bus_cfg.ldmt3r);

	/* horizontal configuration */
	h_total = var->xres + var->hsync_len +
		var->left_margin + var->right_margin;
	tmp = h_total / 8; /* HTCN */
	tmp |= (var->xres / 8) << 16; /* HDCN */
	lcdc_write_chan(ch, LDHCNR, tmp);

	hsync_pos = var->xres + var->right_margin;
	tmp = hsync_pos / 8; /* HSYNP */
	tmp |= (var->hsync_len / 8) << 16; /* HSYNW */
	lcdc_write_chan(ch, LDHSYNR, tmp);

	/* vertical configuration */
	tmp = var->yres + var->vsync_len +
		var->upper_margin + var->lower_margin; /* VTLN */
	tmp |= var->yres << 16; /* VDLN */
	lcdc_write_chan(ch, LDVLNR, tmp);

	tmp = var->yres + var->lower_margin; /* VSYNP */
	tmp |= var->vsync_len << 16; /* VSYNW */
	lcdc_write_chan(ch, LDVSYNR, tmp);

	/* Adjust horizontal synchronisation for HDMI */
	tmp = ((var->xres & 7) << 24) |
		((h_total & 7) << 16) |
		((var->hsync_len & 7) << 8) |
		hsync_pos;
	lcdc_write_chan(ch, LDHAJR, tmp);
}

static int sh_mobile_lcdc_start(struct sh_mobile_lcdc_priv *priv)
{
	struct sh_mobile_lcdc_chan *ch;
	struct fb_videomode *lcd_cfg;
	struct sh_mobile_lcdc_board_cfg	*board_cfg;
	unsigned long tmp;
	int k, m;
	int ret = 0;

	/* enable clocks before accessing the hardware */
	for (k = 0; k < ARRAY_SIZE(priv->ch); k++)
		if (priv->ch[k].enabled)
			sh_mobile_lcdc_clk_on(priv);

	/* reset */
	lcdc_write(priv, _LDCNT2R, lcdc_read(priv, _LDCNT2R) | LCDC_RESET);
	lcdc_wait_bit(priv, _LDCNT2R, LCDC_RESET, 0);

	/* enable LCDC channels */
	tmp = lcdc_read(priv, _LDCNT2R);
	tmp |= priv->ch[0].enabled;
	tmp |= priv->ch[1].enabled;
	lcdc_write(priv, _LDCNT2R, tmp);

	/* read data from external memory, avoid using the BEU for now */
	lcdc_write(priv, _LDCNT2R, lcdc_read(priv, _LDCNT2R) & ~DISPLAY_BEU);

	/* stop the lcdc first */
	sh_mobile_lcdc_start_stop(priv, 0);

	/* configure clocks */
	tmp = priv->lddckr;
	for (k = 0; k < ARRAY_SIZE(priv->ch); k++) {
		ch = &priv->ch[k];

		if (!priv->ch[k].enabled)
			continue;

		m = ch->cfg.clock_divider;
		if (!m)
			continue;

		if (m == 1)
			m = 1 << 6;
		tmp |= m << (lcdc_chan_is_sublcd(ch) ? 8 : 0);

		lcdc_write_chan(ch, LDDCKPAT1R, 0x00000000);
		lcdc_write_chan(ch, LDDCKPAT2R, (1 << (m/2)) - 1);
	}

	lcdc_write(priv, _LDDCKR, tmp);

	/* start dotclock again */
	lcdc_write(priv, _LDDCKSTPR, 0);
	lcdc_wait_bit(priv, _LDDCKSTPR, ~0, 0);

	/* interrupts are disabled to begin with */
	lcdc_write(priv, _LDINTR, 0);

	for (k = 0; k < ARRAY_SIZE(priv->ch); k++) {
		ch = &priv->ch[k];
		lcd_cfg = &ch->cfg.lcd_cfg;

		if (!ch->enabled)
			continue;

		sh_mobile_lcdc_geometry(ch);

		/* power supply */
		lcdc_write_chan(ch, LDPMR, 0);

		board_cfg = &ch->cfg.board_cfg;
		if (board_cfg->setup_sys)
			ret = board_cfg->setup_sys(board_cfg->board_data, ch,
						   &sh_mobile_lcdc_sys_bus_ops);
		if (ret)
			return ret;
	}

	/* word and long word swap */
	lcdc_write(priv, _LDDDSR, lcdc_read(priv, _LDDDSR) | 6);

	for (k = 0; k < ARRAY_SIZE(priv->ch); k++) {
		ch = &priv->ch[k];

		if (!priv->ch[k].enabled)
			continue;

		/* set bpp format in PKF[4:0] */
		tmp = lcdc_read_chan(ch, LDDFR);
		tmp &= ~(0x0001001f);
		tmp |= (ch->info->var.bits_per_pixel == 16) ? 3 : 0;
		lcdc_write_chan(ch, LDDFR, tmp);

		/* point out our frame buffer */
		lcdc_write_chan(ch, LDSA1R, ch->info->fix.smem_start);

		/* set line size */
		lcdc_write_chan(ch, LDMLSR, ch->info->fix.line_length);

		/* setup deferred io if SYS bus */
		tmp = ch->cfg.sys_bus_cfg.deferred_io_msec;
		if (ch->ldmt1r_value & (1 << 12) && tmp) {
			ch->defio.deferred_io = sh_mobile_lcdc_deferred_io;
			ch->defio.delay = msecs_to_jiffies(tmp);
			ch->info->fbdefio = &ch->defio;
			fb_deferred_io_init(ch->info);

			/* one-shot mode */
			lcdc_write_chan(ch, LDSM1R, 1);

			/* enable "Frame End Interrupt Enable" bit */
			lcdc_write(priv, _LDINTR, LDINTR_FE);

		} else {
			/* continuous read mode */
			lcdc_write_chan(ch, LDSM1R, 0);
		}
	}

	/* display output */
	lcdc_write(priv, _LDCNT1R, LCDC_ENABLE);

	/* start the lcdc */
	sh_mobile_lcdc_start_stop(priv, 1);
	priv->started = 1;

	/* tell the board code to enable the panel */
	for (k = 0; k < ARRAY_SIZE(priv->ch); k++) {
		ch = &priv->ch[k];
		if (!ch->enabled)
			continue;

		board_cfg = &ch->cfg.board_cfg;
		if (board_cfg->display_on)
			board_cfg->display_on(board_cfg->board_data, ch->info);
	}

	return 0;
}

static void sh_mobile_lcdc_stop(struct sh_mobile_lcdc_priv *priv)
{
	struct sh_mobile_lcdc_chan *ch;
	struct sh_mobile_lcdc_board_cfg	*board_cfg;
	int k;

	/* clean up deferred io and ask board code to disable panel */
	for (k = 0; k < ARRAY_SIZE(priv->ch); k++) {
		ch = &priv->ch[k];
		if (!ch->enabled)
			continue;

		/* deferred io mode:
		 * flush frame, and wait for frame end interrupt
		 * clean up deferred io and enable clock
		 */
		if (ch->info->fbdefio) {
			ch->frame_end = 0;
			schedule_delayed_work(&ch->info->deferred_work, 0);
			wait_event(ch->frame_end_wait, ch->frame_end);
			fb_deferred_io_cleanup(ch->info);
			ch->info->fbdefio = NULL;
			sh_mobile_lcdc_clk_on(priv);
		}

		board_cfg = &ch->cfg.board_cfg;
		if (board_cfg->display_off)
			board_cfg->display_off(board_cfg->board_data);
	}

	/* stop the lcdc */
	if (priv->started) {
		sh_mobile_lcdc_start_stop(priv, 0);
		priv->started = 0;
	}

	/* stop clocks */
	for (k = 0; k < ARRAY_SIZE(priv->ch); k++)
		if (priv->ch[k].enabled)
			sh_mobile_lcdc_clk_off(priv);
}

static int sh_mobile_lcdc_check_interface(struct sh_mobile_lcdc_chan *ch)
{
	int ifm, miftyp;

	switch (ch->cfg.interface_type) {
	case RGB8: ifm = 0; miftyp = 0; break;
	case RGB9: ifm = 0; miftyp = 4; break;
	case RGB12A: ifm = 0; miftyp = 5; break;
	case RGB12B: ifm = 0; miftyp = 6; break;
	case RGB16: ifm = 0; miftyp = 7; break;
	case RGB18: ifm = 0; miftyp = 10; break;
	case RGB24: ifm = 0; miftyp = 11; break;
	case SYS8A: ifm = 1; miftyp = 0; break;
	case SYS8B: ifm = 1; miftyp = 1; break;
	case SYS8C: ifm = 1; miftyp = 2; break;
	case SYS8D: ifm = 1; miftyp = 3; break;
	case SYS9: ifm = 1; miftyp = 4; break;
	case SYS12: ifm = 1; miftyp = 5; break;
	case SYS16A: ifm = 1; miftyp = 7; break;
	case SYS16B: ifm = 1; miftyp = 8; break;
	case SYS16C: ifm = 1; miftyp = 9; break;
	case SYS18: ifm = 1; miftyp = 10; break;
	case SYS24: ifm = 1; miftyp = 11; break;
	default: goto bad;
	}

	/* SUBLCD only supports SYS interface */
	if (lcdc_chan_is_sublcd(ch)) {
		if (ifm == 0)
			goto bad;
		else
			ifm = 0;
	}

	ch->ldmt1r_value = (ifm << 12) | miftyp;
	return 0;
 bad:
	return -EINVAL;
}

static int sh_mobile_lcdc_setup_clocks(struct platform_device *pdev,
				       int clock_source,
				       struct sh_mobile_lcdc_priv *priv)
{
	char *str;
	int icksel;

	switch (clock_source) {
	case LCDC_CLK_BUS: str = "bus_clk"; icksel = 0; break;
	case LCDC_CLK_PERIPHERAL: str = "peripheral_clk"; icksel = 1; break;
	case LCDC_CLK_EXTERNAL: str = NULL; icksel = 2; break;
	default:
		return -EINVAL;
	}

	priv->lddckr = icksel << 16;

	if (str) {
		priv->dot_clk = clk_get(&pdev->dev, str);
		if (IS_ERR(priv->dot_clk)) {
			dev_err(&pdev->dev, "cannot get dot clock %s\n", str);
			return PTR_ERR(priv->dot_clk);
		}
	}
	atomic_set(&priv->hw_usecnt, -1);

	/* Runtime PM support involves two step for this driver:
	 * 1) Enable Runtime PM
	 * 2) Force Runtime PM Resume since hardware is accessed from probe()
	 */
	priv->dev = &pdev->dev;
	pm_runtime_enable(priv->dev);
	pm_runtime_resume(priv->dev);
	return 0;
}

static int sh_mobile_lcdc_setcolreg(u_int regno,
				    u_int red, u_int green, u_int blue,
				    u_int transp, struct fb_info *info)
{
	u32 *palette = info->pseudo_palette;

	if (regno >= PALETTE_NR)
		return -EINVAL;

	/* only FB_VISUAL_TRUECOLOR supported */

	red >>= 16 - info->var.red.length;
	green >>= 16 - info->var.green.length;
	blue >>= 16 - info->var.blue.length;
	transp >>= 16 - info->var.transp.length;

	palette[regno] = (red << info->var.red.offset) |
	  (green << info->var.green.offset) |
	  (blue << info->var.blue.offset) |
	  (transp << info->var.transp.offset);

	return 0;
}

static struct fb_fix_screeninfo sh_mobile_lcdc_fix  = {
	.id =		"SH Mobile LCDC",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.accel =	FB_ACCEL_NONE,
	.xpanstep =	0,
	.ypanstep =	1,
	.ywrapstep =	0,
};

static void sh_mobile_lcdc_fillrect(struct fb_info *info,
				    const struct fb_fillrect *rect)
{
	sys_fillrect(info, rect);
	sh_mobile_lcdc_deferred_io_touch(info);
}

static void sh_mobile_lcdc_copyarea(struct fb_info *info,
				    const struct fb_copyarea *area)
{
	sys_copyarea(info, area);
	sh_mobile_lcdc_deferred_io_touch(info);
}

static void sh_mobile_lcdc_imageblit(struct fb_info *info,
				     const struct fb_image *image)
{
	sys_imageblit(info, image);
	sh_mobile_lcdc_deferred_io_touch(info);
}

static int sh_mobile_fb_pan_display(struct fb_var_screeninfo *var,
				     struct fb_info *info)
{
	struct sh_mobile_lcdc_chan *ch = info->par;
	struct sh_mobile_lcdc_priv *priv = ch->lcdc;
	unsigned long ldrcntr;
	unsigned long new_pan_offset;

	new_pan_offset = (var->yoffset * info->fix.line_length) +
		(var->xoffset * (info->var.bits_per_pixel / 8));

	if (new_pan_offset == ch->pan_offset)
		return 0;	/* No change, do nothing */

	ldrcntr = lcdc_read(priv, _LDRCNTR);

	/* Set the source address for the next refresh */
	lcdc_write_chan_mirror(ch, LDSA1R, ch->dma_handle + new_pan_offset);
	if (lcdc_chan_is_sublcd(ch))
		lcdc_write(ch->lcdc, _LDRCNTR, ldrcntr ^ LDRCNTR_SRS);
	else
		lcdc_write(ch->lcdc, _LDRCNTR, ldrcntr ^ LDRCNTR_MRS);

	ch->pan_offset = new_pan_offset;

	sh_mobile_lcdc_deferred_io_touch(info);

	return 0;
}

static int sh_mobile_wait_for_vsync(struct fb_info *info)
{
	struct sh_mobile_lcdc_chan *ch = info->par;
	unsigned long ldintr;
	int ret;

	/* Enable VSync End interrupt */
	ldintr = lcdc_read(ch->lcdc, _LDINTR);
	ldintr |= LDINTR_VEE;
	lcdc_write(ch->lcdc, _LDINTR, ldintr);

	ret = wait_for_completion_interruptible_timeout(&ch->vsync_completion,
							msecs_to_jiffies(100));
	if (!ret)
		return -ETIMEDOUT;

	return 0;
}

static int sh_mobile_ioctl(struct fb_info *info, unsigned int cmd,
		       unsigned long arg)
{
	int retval;

	switch (cmd) {
	case FBIO_WAITFORVSYNC:
		retval = sh_mobile_wait_for_vsync(info);
		break;

	default:
		retval = -ENOIOCTLCMD;
		break;
	}
	return retval;
}


static struct fb_ops sh_mobile_lcdc_ops = {
	.owner          = THIS_MODULE,
	.fb_setcolreg	= sh_mobile_lcdc_setcolreg,
	.fb_read        = fb_sys_read,
	.fb_write       = fb_sys_write,
	.fb_fillrect	= sh_mobile_lcdc_fillrect,
	.fb_copyarea	= sh_mobile_lcdc_copyarea,
	.fb_imageblit	= sh_mobile_lcdc_imageblit,
	.fb_pan_display = sh_mobile_fb_pan_display,
	.fb_ioctl       = sh_mobile_ioctl,
};

static int sh_mobile_lcdc_set_bpp(struct fb_var_screeninfo *var, int bpp)
{
	switch (bpp) {
	case 16: /* PKF[4:0] = 00011 - RGB 565 */
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;

	case 32: /* PKF[4:0] = 00000 - RGB 888
		  * sh7722 pdf says 00RRGGBB but reality is GGBB00RR
		  * this may be because LDDDSR has word swap enabled..
		  */
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 24;
		var->green.length = 8;
		var->blue.offset = 16;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	default:
		return -EINVAL;
	}
	var->bits_per_pixel = bpp;
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;
	return 0;
}

static int sh_mobile_lcdc_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	sh_mobile_lcdc_stop(platform_get_drvdata(pdev));
	return 0;
}

static int sh_mobile_lcdc_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return sh_mobile_lcdc_start(platform_get_drvdata(pdev));
}

static int sh_mobile_lcdc_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sh_mobile_lcdc_priv *p = platform_get_drvdata(pdev);
	struct sh_mobile_lcdc_chan *ch;
	int k, n;

	/* save per-channel registers */
	for (k = 0; k < ARRAY_SIZE(p->ch); k++) {
		ch = &p->ch[k];
		if (!ch->enabled)
			continue;
		for (n = 0; n < NR_CH_REGS; n++)
			ch->saved_ch_regs[n] = lcdc_read_chan(ch, n);
	}

	/* save shared registers */
	for (n = 0; n < NR_SHARED_REGS; n++)
		p->saved_shared_regs[n] = lcdc_read(p, lcdc_shared_regs[n]);

	/* turn off LCDC hardware */
	lcdc_write(p, _LDCNT1R, 0);
	return 0;
}

static int sh_mobile_lcdc_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sh_mobile_lcdc_priv *p = platform_get_drvdata(pdev);
	struct sh_mobile_lcdc_chan *ch;
	int k, n;

	/* restore per-channel registers */
	for (k = 0; k < ARRAY_SIZE(p->ch); k++) {
		ch = &p->ch[k];
		if (!ch->enabled)
			continue;
		for (n = 0; n < NR_CH_REGS; n++)
			lcdc_write_chan(ch, n, ch->saved_ch_regs[n]);
	}

	/* restore shared registers */
	for (n = 0; n < NR_SHARED_REGS; n++)
		lcdc_write(p, lcdc_shared_regs[n], p->saved_shared_regs[n]);

	return 0;
}

static const struct dev_pm_ops sh_mobile_lcdc_dev_pm_ops = {
	.suspend = sh_mobile_lcdc_suspend,
	.resume = sh_mobile_lcdc_resume,
	.runtime_suspend = sh_mobile_lcdc_runtime_suspend,
	.runtime_resume = sh_mobile_lcdc_runtime_resume,
};

static int sh_mobile_lcdc_notify(struct notifier_block *nb,
				 unsigned long action, void *data)
{
	struct fb_event *event = data;
	struct fb_info *info = event->info;
	struct sh_mobile_lcdc_chan *ch = info->par;
	struct sh_mobile_lcdc_board_cfg	*board_cfg = &ch->cfg.board_cfg;
	struct fb_var_screeninfo *var;

	if (&ch->lcdc->notifier != nb)
		return 0;

	dev_dbg(info->dev, "%s(): action = %lu, data = %p\n",
		__func__, action, event->data);

	switch(action) {
	case FB_EVENT_SUSPEND:
		if (board_cfg->display_off)
			board_cfg->display_off(board_cfg->board_data);
		pm_runtime_put(info->device);
		break;
	case FB_EVENT_RESUME:
		var = &info->var;

		/* HDMI must be enabled before LCDC configuration */
		if (board_cfg->display_on)
			board_cfg->display_on(board_cfg->board_data, ch->info);

		/* Check if the new display is not in our modelist */
		if (ch->info->modelist.next &&
		    !fb_match_mode(var, &ch->info->modelist)) {
			struct fb_videomode mode;
			int ret;

			/* Can we handle this display? */
			if (var->xres > ch->cfg.lcd_cfg.xres ||
			    var->yres > ch->cfg.lcd_cfg.yres)
				return -ENOMEM;

			/* Add to the modelist */
			fb_var_to_videomode(&mode, var);
			ret = fb_add_videomode(&mode, &ch->info->modelist);
			if (ret < 0)
				return ret;
		}

		pm_runtime_get_sync(info->device);

		sh_mobile_lcdc_geometry(ch);

		break;
	}

	return 0;
}

static int sh_mobile_lcdc_remove(struct platform_device *pdev);

static int __devinit sh_mobile_lcdc_probe(struct platform_device *pdev)
{
	struct fb_info *info;
	struct sh_mobile_lcdc_priv *priv;
	struct sh_mobile_lcdc_info *pdata;
	struct sh_mobile_lcdc_chan_cfg *cfg;
	struct resource *res;
	int error;
	void *buf;
	int i, j;

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "no platform data defined\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	i = platform_get_irq(pdev, 0);
	if (!res || i < 0) {
		dev_err(&pdev->dev, "cannot get platform resources\n");
		return -ENOENT;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "cannot allocate device data\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, priv);

	error = request_irq(i, sh_mobile_lcdc_irq, IRQF_DISABLED,
			    dev_name(&pdev->dev), priv);
	if (error) {
		dev_err(&pdev->dev, "unable to request irq\n");
		goto err1;
	}

	priv->irq = i;
	pdata = pdev->dev.platform_data;

	j = 0;
	for (i = 0; i < ARRAY_SIZE(pdata->ch); i++) {
		priv->ch[j].lcdc = priv;
		memcpy(&priv->ch[j].cfg, &pdata->ch[i], sizeof(pdata->ch[i]));

		error = sh_mobile_lcdc_check_interface(&priv->ch[j]);
		if (error) {
			dev_err(&pdev->dev, "unsupported interface type\n");
			goto err1;
		}
		init_waitqueue_head(&priv->ch[j].frame_end_wait);
		init_completion(&priv->ch[j].vsync_completion);
		priv->ch[j].pan_offset = 0;

		switch (pdata->ch[i].chan) {
		case LCDC_CHAN_MAINLCD:
			priv->ch[j].enabled = 1 << 1;
			priv->ch[j].reg_offs = lcdc_offs_mainlcd;
			j++;
			break;
		case LCDC_CHAN_SUBLCD:
			priv->ch[j].enabled = 1 << 2;
			priv->ch[j].reg_offs = lcdc_offs_sublcd;
			j++;
			break;
		}
	}

	if (!j) {
		dev_err(&pdev->dev, "no channels defined\n");
		error = -EINVAL;
		goto err1;
	}

	priv->base = ioremap_nocache(res->start, resource_size(res));
	if (!priv->base)
		goto err1;

	error = sh_mobile_lcdc_setup_clocks(pdev, pdata->clock_source, priv);
	if (error) {
		dev_err(&pdev->dev, "unable to setup clocks\n");
		goto err1;
	}

	for (i = 0; i < j; i++) {
		struct fb_var_screeninfo *var;
		struct fb_videomode *lcd_cfg;
		cfg = &priv->ch[i].cfg;

		priv->ch[i].info = framebuffer_alloc(0, &pdev->dev);
		if (!priv->ch[i].info) {
			dev_err(&pdev->dev, "unable to allocate fb_info\n");
			error = -ENOMEM;
			break;
		}

		info = priv->ch[i].info;
		var = &info->var;
		lcd_cfg = &cfg->lcd_cfg;
		info->fbops = &sh_mobile_lcdc_ops;
		var->xres = var->xres_virtual = lcd_cfg->xres;
		var->yres = lcd_cfg->yres;
		/* Default Y virtual resolution is 2x panel size */
		var->yres_virtual = var->yres * 2;
		var->width = cfg->lcd_size_cfg.width;
		var->height = cfg->lcd_size_cfg.height;
		var->activate = FB_ACTIVATE_NOW;
		var->left_margin = lcd_cfg->left_margin;
		var->right_margin = lcd_cfg->right_margin;
		var->upper_margin = lcd_cfg->upper_margin;
		var->lower_margin = lcd_cfg->lower_margin;
		var->hsync_len = lcd_cfg->hsync_len;
		var->vsync_len = lcd_cfg->vsync_len;
		var->sync = lcd_cfg->sync;
		var->pixclock = lcd_cfg->pixclock;

		error = sh_mobile_lcdc_set_bpp(var, cfg->bpp);
		if (error)
			break;

		info->fix = sh_mobile_lcdc_fix;
		info->fix.line_length = lcd_cfg->xres * (cfg->bpp / 8);
		info->fix.smem_len = info->fix.line_length *
			var->yres_virtual;

		buf = dma_alloc_coherent(&pdev->dev, info->fix.smem_len,
					 &priv->ch[i].dma_handle, GFP_KERNEL);
		if (!buf) {
			dev_err(&pdev->dev, "unable to allocate buffer\n");
			error = -ENOMEM;
			break;
		}

		info->pseudo_palette = &priv->ch[i].pseudo_palette;
		info->flags = FBINFO_FLAG_DEFAULT;

		error = fb_alloc_cmap(&info->cmap, PALETTE_NR, 0);
		if (error < 0) {
			dev_err(&pdev->dev, "unable to allocate cmap\n");
			dma_free_coherent(&pdev->dev, info->fix.smem_len,
					  buf, priv->ch[i].dma_handle);
			break;
		}

		memset(buf, 0, info->fix.smem_len);
		info->fix.smem_start = priv->ch[i].dma_handle;
		info->screen_base = buf;
		info->device = &pdev->dev;
		info->par = &priv->ch[i];
	}

	if (error)
		goto err1;

	error = sh_mobile_lcdc_start(priv);
	if (error) {
		dev_err(&pdev->dev, "unable to start hardware\n");
		goto err1;
	}

	for (i = 0; i < j; i++) {
		struct sh_mobile_lcdc_chan *ch = priv->ch + i;

		info = ch->info;

		if (info->fbdefio) {
			ch->sglist = vmalloc(sizeof(struct scatterlist) *
					info->fix.smem_len >> PAGE_SHIFT);
			if (!ch->sglist) {
				dev_err(&pdev->dev, "cannot allocate sglist\n");
				goto err1;
			}
		}

		error = register_framebuffer(info);
		if (error < 0)
			goto err1;

		dev_info(info->dev,
			 "registered %s/%s as %dx%d %dbpp.\n",
			 pdev->name,
			 (ch->cfg.chan == LCDC_CHAN_MAINLCD) ?
			 "mainlcd" : "sublcd",
			 (int) ch->cfg.lcd_cfg.xres,
			 (int) ch->cfg.lcd_cfg.yres,
			 ch->cfg.bpp);

		/* deferred io mode: disable clock to save power */
		if (info->fbdefio || info->state == FBINFO_STATE_SUSPENDED)
			sh_mobile_lcdc_clk_off(priv);
	}

	/* Failure ignored */
	priv->notifier.notifier_call = sh_mobile_lcdc_notify;
	fb_register_client(&priv->notifier);

	return 0;
err1:
	sh_mobile_lcdc_remove(pdev);

	return error;
}

static int sh_mobile_lcdc_remove(struct platform_device *pdev)
{
	struct sh_mobile_lcdc_priv *priv = platform_get_drvdata(pdev);
	struct fb_info *info;
	int i;

	fb_unregister_client(&priv->notifier);

	for (i = 0; i < ARRAY_SIZE(priv->ch); i++)
		if (priv->ch[i].info && priv->ch[i].info->dev)
			unregister_framebuffer(priv->ch[i].info);

	sh_mobile_lcdc_stop(priv);

	for (i = 0; i < ARRAY_SIZE(priv->ch); i++) {
		info = priv->ch[i].info;

		if (!info || !info->device)
			continue;

		if (priv->ch[i].sglist)
			vfree(priv->ch[i].sglist);

		dma_free_coherent(&pdev->dev, info->fix.smem_len,
				  info->screen_base, priv->ch[i].dma_handle);
		fb_dealloc_cmap(&info->cmap);
		framebuffer_release(info);
	}

	if (priv->dot_clk)
		clk_put(priv->dot_clk);

	if (priv->dev)
		pm_runtime_disable(priv->dev);

	if (priv->base)
		iounmap(priv->base);

	if (priv->irq)
		free_irq(priv->irq, priv);
	kfree(priv);
	return 0;
}

static struct platform_driver sh_mobile_lcdc_driver = {
	.driver		= {
		.name		= "sh_mobile_lcdc_fb",
		.owner		= THIS_MODULE,
		.pm		= &sh_mobile_lcdc_dev_pm_ops,
	},
	.probe		= sh_mobile_lcdc_probe,
	.remove		= sh_mobile_lcdc_remove,
};

static int __init sh_mobile_lcdc_init(void)
{
	return platform_driver_register(&sh_mobile_lcdc_driver);
}

static void __exit sh_mobile_lcdc_exit(void)
{
	platform_driver_unregister(&sh_mobile_lcdc_driver);
}

module_init(sh_mobile_lcdc_init);
module_exit(sh_mobile_lcdc_exit);

MODULE_DESCRIPTION("SuperH Mobile LCDC Framebuffer driver");
MODULE_AUTHOR("Magnus Damm <damm@opensource.se>");
MODULE_LICENSE("GPL v2");
