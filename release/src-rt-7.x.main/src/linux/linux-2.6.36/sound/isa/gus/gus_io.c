/*
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *  I/O routines for GF1/InterWave synthesizer chips
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/delay.h>
#include <linux/time.h>
#include <sound/core.h>
#include <sound/gus.h>

void snd_gf1_delay(struct snd_gus_card * gus)
{
	int i;

	for (i = 0; i < 6; i++) {
		mb();
		inb(GUSP(gus, DRAM));
	}
}

/*
 *  =======================================================================
 */

/*
 *  ok.. stop of control registers (wave & ramp) need some special things..
 *       big UltraClick (tm) elimination...
 */

static inline void __snd_gf1_ctrl_stop(struct snd_gus_card * gus, unsigned char reg)
{
	unsigned char value;

	outb(reg | 0x80, gus->gf1.reg_regsel);
	mb();
	value = inb(gus->gf1.reg_data8);
	mb();
	outb(reg, gus->gf1.reg_regsel);
	mb();
	outb((value | 0x03) & ~(0x80 | 0x20), gus->gf1.reg_data8);
	mb();
}

static inline void __snd_gf1_write8(struct snd_gus_card * gus,
				    unsigned char reg,
				    unsigned char data)
{
	outb(reg, gus->gf1.reg_regsel);
	mb();
	outb(data, gus->gf1.reg_data8);
	mb();
}

static inline unsigned char __snd_gf1_look8(struct snd_gus_card * gus,
					    unsigned char reg)
{
	outb(reg, gus->gf1.reg_regsel);
	mb();
	return inb(gus->gf1.reg_data8);
}

static inline void __snd_gf1_write16(struct snd_gus_card * gus,
				     unsigned char reg, unsigned int data)
{
	outb(reg, gus->gf1.reg_regsel);
	mb();
	outw((unsigned short) data, gus->gf1.reg_data16);
	mb();
}

static inline unsigned short __snd_gf1_look16(struct snd_gus_card * gus,
					      unsigned char reg)
{
	outb(reg, gus->gf1.reg_regsel);
	mb();
	return inw(gus->gf1.reg_data16);
}

static inline void __snd_gf1_adlib_write(struct snd_gus_card * gus,
					 unsigned char reg, unsigned char data)
{
	outb(reg, gus->gf1.reg_timerctrl);
	inb(gus->gf1.reg_timerctrl);
	inb(gus->gf1.reg_timerctrl);
	outb(data, gus->gf1.reg_timerdata);
	inb(gus->gf1.reg_timerctrl);
	inb(gus->gf1.reg_timerctrl);
}

static inline void __snd_gf1_write_addr(struct snd_gus_card * gus, unsigned char reg,
                                        unsigned int addr, int w_16bit)
{
	if (gus->gf1.enh_mode) {
		if (w_16bit)
			addr = ((addr >> 1) & ~0x0000000f) | (addr & 0x0000000f);
		__snd_gf1_write8(gus, SNDRV_GF1_VB_UPPER_ADDRESS, (unsigned char) ((addr >> 26) & 0x03));
	} else if (w_16bit)
		addr = (addr & 0x00c0000f) | ((addr & 0x003ffff0) >> 1);
	__snd_gf1_write16(gus, reg, (unsigned short) (addr >> 11));
	__snd_gf1_write16(gus, reg + 1, (unsigned short) (addr << 5));
}

static inline unsigned int __snd_gf1_read_addr(struct snd_gus_card * gus,
					       unsigned char reg, short w_16bit)
{
	unsigned int res;

	res = ((unsigned int) __snd_gf1_look16(gus, reg | 0x80) << 11) & 0xfff800;
	res |= ((unsigned int) __snd_gf1_look16(gus, (reg + 1) | 0x80) >> 5) & 0x0007ff;
	if (gus->gf1.enh_mode) {
		res |= (unsigned int) __snd_gf1_look8(gus, SNDRV_GF1_VB_UPPER_ADDRESS | 0x80) << 26;
		if (w_16bit)
			res = ((res << 1) & 0xffffffe0) | (res & 0x0000000f);
	} else if (w_16bit)
		res = ((res & 0x001ffff0) << 1) | (res & 0x00c0000f);
	return res;
}


/*
 *  =======================================================================
 */

void snd_gf1_ctrl_stop(struct snd_gus_card * gus, unsigned char reg)
{
	__snd_gf1_ctrl_stop(gus, reg);
}

void snd_gf1_write8(struct snd_gus_card * gus,
		    unsigned char reg,
		    unsigned char data)
{
	__snd_gf1_write8(gus, reg, data);
}

unsigned char snd_gf1_look8(struct snd_gus_card * gus, unsigned char reg)
{
	return __snd_gf1_look8(gus, reg);
}

void snd_gf1_write16(struct snd_gus_card * gus,
		     unsigned char reg,
		     unsigned int data)
{
	__snd_gf1_write16(gus, reg, data);
}

unsigned short snd_gf1_look16(struct snd_gus_card * gus, unsigned char reg)
{
	return __snd_gf1_look16(gus, reg);
}

void snd_gf1_adlib_write(struct snd_gus_card * gus,
                         unsigned char reg,
                         unsigned char data)
{
	__snd_gf1_adlib_write(gus, reg, data);
}

void snd_gf1_write_addr(struct snd_gus_card * gus, unsigned char reg,
                        unsigned int addr, short w_16bit)
{
	__snd_gf1_write_addr(gus, reg, addr, w_16bit);
}

unsigned int snd_gf1_read_addr(struct snd_gus_card * gus,
                               unsigned char reg,
                               short w_16bit)
{
	return __snd_gf1_read_addr(gus, reg, w_16bit);
}

/*

 */

void snd_gf1_i_ctrl_stop(struct snd_gus_card * gus, unsigned char reg)
{
	unsigned long flags;

	spin_lock_irqsave(&gus->reg_lock, flags);
	__snd_gf1_ctrl_stop(gus, reg);
	spin_unlock_irqrestore(&gus->reg_lock, flags);
}

void snd_gf1_i_write8(struct snd_gus_card * gus,
		      unsigned char reg,
                      unsigned char data)
{
	unsigned long flags;

	spin_lock_irqsave(&gus->reg_lock, flags);
	__snd_gf1_write8(gus, reg, data);
	spin_unlock_irqrestore(&gus->reg_lock, flags);
}

unsigned char snd_gf1_i_look8(struct snd_gus_card * gus, unsigned char reg)
{
	unsigned long flags;
	unsigned char res;

	spin_lock_irqsave(&gus->reg_lock, flags);
	res = __snd_gf1_look8(gus, reg);
	spin_unlock_irqrestore(&gus->reg_lock, flags);
	return res;
}

void snd_gf1_i_write16(struct snd_gus_card * gus,
		       unsigned char reg,
		       unsigned int data)
{
	unsigned long flags;

	spin_lock_irqsave(&gus->reg_lock, flags);
	__snd_gf1_write16(gus, reg, data);
	spin_unlock_irqrestore(&gus->reg_lock, flags);
}

unsigned short snd_gf1_i_look16(struct snd_gus_card * gus, unsigned char reg)
{
	unsigned long flags;
	unsigned short res;

	spin_lock_irqsave(&gus->reg_lock, flags);
	res = __snd_gf1_look16(gus, reg);
	spin_unlock_irqrestore(&gus->reg_lock, flags);
	return res;
}


#ifdef CONFIG_SND_DEBUG
static unsigned int snd_gf1_i_read_addr(struct snd_gus_card * gus,
					unsigned char reg, short w_16bit)
{
	unsigned int res;
	unsigned long flags;

	spin_lock_irqsave(&gus->reg_lock, flags);
	res = __snd_gf1_read_addr(gus, reg, w_16bit);
	spin_unlock_irqrestore(&gus->reg_lock, flags);
	return res;
}
#endif

/*

 */

void snd_gf1_dram_addr(struct snd_gus_card * gus, unsigned int addr)
{
	outb(0x43, gus->gf1.reg_regsel);
	mb();
	outw((unsigned short) addr, gus->gf1.reg_data16);
	mb();
	outb(0x44, gus->gf1.reg_regsel);
	mb();
	outb((unsigned char) (addr >> 16), gus->gf1.reg_data8);
	mb();
}

void snd_gf1_poke(struct snd_gus_card * gus, unsigned int addr, unsigned char data)
{
	unsigned long flags;

	spin_lock_irqsave(&gus->reg_lock, flags);
	outb(SNDRV_GF1_GW_DRAM_IO_LOW, gus->gf1.reg_regsel);
	mb();
	outw((unsigned short) addr, gus->gf1.reg_data16);
	mb();
	outb(SNDRV_GF1_GB_DRAM_IO_HIGH, gus->gf1.reg_regsel);
	mb();
	outb((unsigned char) (addr >> 16), gus->gf1.reg_data8);
	mb();
	outb(data, gus->gf1.reg_dram);
	spin_unlock_irqrestore(&gus->reg_lock, flags);
}

unsigned char snd_gf1_peek(struct snd_gus_card * gus, unsigned int addr)
{
	unsigned long flags;
	unsigned char res;

	spin_lock_irqsave(&gus->reg_lock, flags);
	outb(SNDRV_GF1_GW_DRAM_IO_LOW, gus->gf1.reg_regsel);
	mb();
	outw((unsigned short) addr, gus->gf1.reg_data16);
	mb();
	outb(SNDRV_GF1_GB_DRAM_IO_HIGH, gus->gf1.reg_regsel);
	mb();
	outb((unsigned char) (addr >> 16), gus->gf1.reg_data8);
	mb();
	res = inb(gus->gf1.reg_dram);
	spin_unlock_irqrestore(&gus->reg_lock, flags);
	return res;
}


void snd_gf1_select_active_voices(struct snd_gus_card * gus)
{
	unsigned short voices;

	static unsigned short voices_tbl[32 - 14 + 1] =
	{
	    44100, 41160, 38587, 36317, 34300, 32494, 30870, 29400, 28063, 26843,
	    25725, 24696, 23746, 22866, 22050, 21289, 20580, 19916, 19293
	};

	voices = gus->gf1.active_voices;
	if (voices > 32)
		voices = 32;
	if (voices < 14)
		voices = 14;
	if (gus->gf1.enh_mode)
		voices = 32;
	gus->gf1.active_voices = voices;
	gus->gf1.playback_freq =
	    gus->gf1.enh_mode ? 44100 : voices_tbl[voices - 14];
	if (!gus->gf1.enh_mode) {
		snd_gf1_i_write8(gus, SNDRV_GF1_GB_ACTIVE_VOICES, 0xc0 | (voices - 1));
		udelay(100);
	}
}

#ifdef CONFIG_SND_DEBUG

void snd_gf1_print_voice_registers(struct snd_gus_card * gus)
{
	unsigned char mode;
	int voice, ctrl;

	voice = gus->gf1.active_voice;
	printk(KERN_INFO " -%i- GF1  voice ctrl, ramp ctrl  = 0x%x, 0x%x\n", voice, ctrl = snd_gf1_i_read8(gus, 0), snd_gf1_i_read8(gus, 0x0d));
	printk(KERN_INFO " -%i- GF1  frequency              = 0x%x\n", voice, snd_gf1_i_read16(gus, 1));
	printk(KERN_INFO " -%i- GF1  loop start, end        = 0x%x (0x%x), 0x%x (0x%x)\n", voice, snd_gf1_i_read_addr(gus, 2, ctrl & 4), snd_gf1_i_read_addr(gus, 2, (ctrl & 4) ^ 4), snd_gf1_i_read_addr(gus, 4, ctrl & 4), snd_gf1_i_read_addr(gus, 4, (ctrl & 4) ^ 4));
	printk(KERN_INFO " -%i- GF1  ramp start, end, rate  = 0x%x, 0x%x, 0x%x\n", voice, snd_gf1_i_read8(gus, 7), snd_gf1_i_read8(gus, 8), snd_gf1_i_read8(gus, 6));
	printk(KERN_INFO" -%i- GF1  volume                 = 0x%x\n", voice, snd_gf1_i_read16(gus, 9));
	printk(KERN_INFO " -%i- GF1  position               = 0x%x (0x%x)\n", voice, snd_gf1_i_read_addr(gus, 0x0a, ctrl & 4), snd_gf1_i_read_addr(gus, 0x0a, (ctrl & 4) ^ 4));
	if (gus->interwave && snd_gf1_i_read8(gus, 0x19) & 0x01) {	/* enhanced mode */
		mode = snd_gf1_i_read8(gus, 0x15);
		printk(KERN_INFO " -%i- GFA1 mode                   = 0x%x\n", voice, mode);
		if (mode & 0x01) {	/* Effect processor */
			printk(KERN_INFO " -%i- GFA1 effect address         = 0x%x\n", voice, snd_gf1_i_read_addr(gus, 0x11, ctrl & 4));
			printk(KERN_INFO " -%i- GFA1 effect volume          = 0x%x\n", voice, snd_gf1_i_read16(gus, 0x16));
			printk(KERN_INFO " -%i- GFA1 effect volume final    = 0x%x\n", voice, snd_gf1_i_read16(gus, 0x1d));
			printk(KERN_INFO " -%i- GFA1 effect acumulator      = 0x%x\n", voice, snd_gf1_i_read8(gus, 0x14));
		}
		if (mode & 0x20) {
			printk(KERN_INFO " -%i- GFA1 left offset            = 0x%x (%i)\n", voice, snd_gf1_i_read16(gus, 0x13), snd_gf1_i_read16(gus, 0x13) >> 4);
			printk(KERN_INFO " -%i- GFA1 left offset final      = 0x%x (%i)\n", voice, snd_gf1_i_read16(gus, 0x1c), snd_gf1_i_read16(gus, 0x1c) >> 4);
			printk(KERN_INFO " -%i- GFA1 right offset           = 0x%x (%i)\n", voice, snd_gf1_i_read16(gus, 0x0c), snd_gf1_i_read16(gus, 0x0c) >> 4);
			printk(KERN_INFO " -%i- GFA1 right offset final     = 0x%x (%i)\n", voice, snd_gf1_i_read16(gus, 0x1b), snd_gf1_i_read16(gus, 0x1b) >> 4);
		} else
			printk(KERN_INFO " -%i- GF1  pan                    = 0x%x\n", voice, snd_gf1_i_read8(gus, 0x0c));
	} else
		printk(KERN_INFO " -%i- GF1  pan                    = 0x%x\n", voice, snd_gf1_i_read8(gus, 0x0c));
}


#endif
