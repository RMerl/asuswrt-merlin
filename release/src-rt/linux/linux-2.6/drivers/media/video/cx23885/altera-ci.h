/*
 * altera-ci.c
 *
 *  CI driver in conjunction with NetUp Dual DVB-T/C RF CI card
 *
 * Copyright (C) 2010 NetUP Inc.
 * Copyright (C) 2010 Igor M. Liplianin <liplianin@netup.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __ALTERA_CI_H
#define __ALTERA_CI_H

#define ALT_DATA	0x000000ff
#define ALT_TDI		0x00008000
#define ALT_TDO		0x00004000
#define ALT_TCK		0x00002000
#define ALT_RDY		0x00001000
#define ALT_RD		0x00000800
#define ALT_WR		0x00000400
#define ALT_AD_RG	0x00000200
#define ALT_CS		0x00000100

struct altera_ci_config {
	void *dev;/* main dev, for example cx23885_dev */
	void *adapter;/* for CI to connect to */
	struct dvb_demux *demux;/* for hardware PID filter to connect to */
	int (*fpga_rw) (void *dev, int ad_rg, int val, int rw);
};

#if defined(CONFIG_MEDIA_ALTERA_CI) || (defined(CONFIG_MEDIA_ALTERA_CI_MODULE) \
							&& defined(MODULE))

extern int altera_ci_init(struct altera_ci_config *config, int ci_nr);
extern void altera_ci_release(void *dev, int ci_nr);
extern int altera_ci_irq(void *dev);
extern int altera_ci_tuner_reset(void *dev, int ci_nr);

#else

static inline int altera_ci_init(struct altera_ci_config *config, int ci_nr)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return 0;
}

static inline void altera_ci_release(void *dev, int ci_nr)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
}

static inline int altera_ci_irq(void *dev)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return 0;
}

static inline int altera_ci_tuner_reset(void *dev, int ci_nr)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return 0;
}

#endif
#if 0
static inline int altera_hw_filt_init(struct altera_ci_config *config,
							int hw_filt_nr)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return 0;
}

static inline void altera_hw_filt_release(void *dev, int filt_nr)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
}

static inline int altera_pid_feed_control(void *dev, int filt_nr,
		struct dvb_demux_feed *dvbdmxfeed, int onoff)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return 0;
}

#endif /* CONFIG_MEDIA_ALTERA_CI */

#endif /* __ALTERA_CI_H */
