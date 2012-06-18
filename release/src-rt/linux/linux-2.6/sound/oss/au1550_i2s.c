/*
 *      au1550_i2s.c  --  Sound driver for Alchemy Au1550 MIPS
 *			Internet Edge Processor.
 *
 * Copyright 2004 Embedded Edge, LLC
 *	dan@embeddededge.com
 * Copyright 2005 Matt Porter <mporter@kernel.crashing.org>
 *
 * Mostly copied from the au1550_psc.c driver and some from the
 * PowerMac dbdma driver.
 * We assume the processor can do memory coherent DMA.
 *
 * WM8731 mixer support, codec framework, cleanup, and 2.6 port
 * Matt Porter <mporter@kernel.crashing.org>
 *
 * The SMBus (I2C) is required for the control of the
 * appears at I2C address 0x36 (I2C binary 0011011).  The Pb1550
 * uses the Wolfson WM8731 codec, which is controlled over the I2C.
 * It's connected to a 12MHz clock, so we can only reliably support
 * 96KHz, 48KHz, 32KHz, and 8KHz data rates.  Variable rate audio is
 * unsupported, we currently force it to 48KHz.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/module.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/hardirq.h>
#include <linux/sound.h>
#include <linux/slab.h>
#include <linux/soundcard.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/pci.h>
#include <linux/bitops.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/hardirq.h>

#include <asm/mach-au1x00/au1000.h>
#include <asm/mach-au1x00/au1xxx_psc.h>
#include <asm/mach-au1x00/au1xxx_dbdma.h>
#include <asm/mach-pb1x00/pb1550.h>

#undef OSS_DOCUMENTED_MIXER_SEMANTICS

#define AU1550_MODULE_NAME "Au1550 I2S Audio"
#define PFX AU1550_MODULE_NAME

/* Define this if you want to try running at the 44.1 KHz rate.
 * It's just a little off, I think it's actually 44117 or something.
 * I did this for debugging, since many programs, including this
 * driver, will try to upsample from 44.1 to 48 KHz.
 * Seems to work well, we'll just leave it this way.
 */
#define TRY_441KHz

#ifdef TRY_441KHz
#define SAMP_RATE	44100
#else
#define SAMP_RATE	48000
#endif

/* The number of DBDMA ring descriptors to allocate.  No sense making
 * this too large....if you can't keep up with a few you aren't likely
 * to be able to with lots of them, either.
 */
#define NUM_DBDMA_DESCRIPTORS 4

#define pr_error(format, arg...) printk(KERN_ERR PFX ": " format "\n" , ## arg)

static void
au1550_delay(int msec)
{
	unsigned long   tmo;
	signed long     tmo2;

	if (in_interrupt())
		return;

	tmo = jiffies + (msec * HZ) / 1000;
	for (;;) {
		tmo2 = tmo - jiffies;
		if (tmo2 <= 0)
			break;
		schedule_timeout(tmo2);
	}
}

/*
 * Codec framework. If somebody supports another codec, they
 * should hopefully be able to define another struct i2s_codec
 * definition, and #ifdef the support for it and the WM8731 so
 * they can be selected via a CONFIG option. For now, we just
 * hardcode WM8731_CODEC.
 */
#define i2s_supported_mixer(CODEC,FOO) ((FOO >= 0) && \
                                    (FOO < SOUND_MIXER_NRDEVICES) && \
                                    (CODEC)->supported_mixers & (1<<FOO) )

struct i2s_codec {
	int			modcnt;
	int			supported_mixers;
	int			stereo_mixers;
	int			record_sources;
	unsigned int		mixer_state[SOUND_MIXER_NRDEVICES];
	void			*data;
	int			(*set_mixer) (struct i2s_codec *codec, unsigned int oss_mixer, unsigned int val);
	void			(*init_codec) (struct i2s_codec *codec);
};

#define WM8731_CODEC
#ifdef WM8731_CODEC
/*
 * WM8731 codec support
 */
#define WM8731_SUPPORTED_MASK (WM8731_STEREO_MASK|WM8731_RECORD_MASK)
#define WM8731_STEREO_MASK (SOUND_MASK_VOLUME|SOUND_MASK_LINE)
#define WM8731_RECORD_MASK (SOUND_MASK_MIC|SOUND_MASK_LINE)

static struct codec_data {
	u16			audio_path;
} wm8731_data;

static void
wm8731_wrcodec(u8 ctlreg, u8 val)
{
	int	rcnt;
	extern int pb1550_wm_codec_write(u8 addr, u8 reg, u8 val);

	/* The codec is a write only device, with a 16-bit control/data
	 * word.  Although it is written as two bytes on the I2C, the
	 * format is actually 7 bits of register and 9 bits of data.
	 * The ls bit of the first byte is the ms bit of the data.
	 */
	rcnt = 0;
	while ((pb1550_wm_codec_write((0x36 >> 1), ctlreg, val) != 1)
							&& (rcnt < 50)) {
		rcnt++;
	}
}

static int
wm8731_set_mixer(struct i2s_codec *codec, unsigned int oss_mixer, unsigned int val)
{
	unsigned int lvol, rvol;
	struct codec_data *cdata = (struct codec_data *)codec->data;

	switch (oss_mixer) {
		case SOUND_MIXER_VOLUME:
			/* normalize OSS range to fit codec volume control */
			lvol = ((((val & 0x7f00) >> 8) * 0x60) / 0x64) + 0x1f;
			rvol = (((val & 0x7f) * 0x60) / 0x64) + 0x1f;
			lvol |= 0x80;
			rvol |= 0x80;
			wm8731_wrcodec(0x04, lvol);
			au1550_delay(10);
			wm8731_wrcodec(0x06, rvol);
			au1550_delay(10);
			codec->mixer_state[oss_mixer] = val;
			break;
		case SOUND_MIXER_LINE:
			/* normalize OSS range to fit codec line control */
			lvol = ((((val & 0x7f00) >> 8) * 0x1f) / 0x64);
			rvol = (((val & 0x7f) * 0x1f) / 0x64);
			if (!(val & 0x1f00))
				lvol |= 0x80;
			else
				lvol &= ~0x80;
			if (!(val & 0x001f))
				rvol |= 0x80;
			else
				rvol &= ~0x80;
			wm8731_wrcodec(0x00, lvol);
			au1550_delay(10);
			wm8731_wrcodec(0x02, rvol);
			au1550_delay(10);
			codec->mixer_state[oss_mixer] = val;
			break;
		case SOUND_MIXER_MIC:
			if (!val)
				cdata->audio_path |= 0x02;
			else {
				if (val >= 0x32)
					cdata->audio_path |= 0x01;
				else
					cdata->audio_path &= ~0x01;
				cdata->audio_path &= ~0x02;
			}
			wm8731_wrcodec(0x08, cdata->audio_path);
			au1550_delay(10);
			codec->mixer_state[oss_mixer] = val;
			break;
		case SOUND_MIXER_RECSRC:
			if (val & SOUND_MASK_LINE)
				cdata->audio_path &= ~0x04;
			else
				cdata->audio_path |= 0x04;
			wm8731_wrcodec(0x08, cdata->audio_path);
			au1550_delay(10);
			codec->mixer_state[oss_mixer] = val;
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

void
wm8731_init_codec(struct i2s_codec *codec)
{
	struct codec_data *cdata = (struct codec_data *)codec->data;

	wm8731_wrcodec(0x1e, 0x00);	/* Reset */
	au1550_delay(200);
	wm8731_wrcodec(0x0c, 0x00);	/* Power up everything */
	au1550_delay(10);
	wm8731_wrcodec(0x12, 0x00);	/* Deactivate codec */
	au1550_delay(10);
	cdata->audio_path = 0x10;
	/* Select DAC outputs to line out */
	wm8731_wrcodec(0x08, cdata->audio_path);
	au1550_delay(10);
	wm8731_wrcodec(0x0a, 0x00);	/* Disable output mute */
	au1550_delay(10);
	wm8731_wrcodec(0x0e, 0x02);	/* Set slave, 16-bit, I2S modes */
	au1550_delay(10);
	wm8731_wrcodec(0x10, 0x01);	/* 12MHz (USB), 250fs */
	au1550_delay(10);
	wm8731_wrcodec(0x12, 0x01);	/* Activate codec */
	au1550_delay(10);

	codec->set_mixer(codec, SOUND_MIXER_VOLUME, 0x5050);
	codec->set_mixer(codec, SOUND_MIXER_LINE, 0x0000);
	codec->set_mixer(codec, SOUND_MIXER_MIC, 0x00);
	codec->mixer_state[SOUND_MIXER_RECSRC] = SOUND_MIXER_LINE;
}

static struct i2s_codec au1550_i2s_codec = {
	.supported_mixers	= WM8731_SUPPORTED_MASK,
	.stereo_mixers		= WM8731_STEREO_MASK,
	.record_sources		= WM8731_RECORD_MASK,
	.init_codec		= &wm8731_init_codec,
	.set_mixer		= &wm8731_set_mixer,
	.data			= &wm8731_data,
};
#endif /* WM8731_CODEC */

static struct au1550_state {
	/* soundcore stuff */
	int             dev_audio;
	int		dev_mixer;

	spinlock_t		lock;
	struct semaphore	open_sem;
	struct semaphore	sem;
	mode_t			open_mode;
	wait_queue_head_t	open_wait;
	volatile psc_i2s_t	*psc_addr;
	struct i2s_codec	*codec;

	struct dmabuf {
		u32		dmanr;
		unsigned        sample_rate;
		unsigned	src_factor;
		unsigned        sample_size;
		int             num_channels;
		int		dma_bytes_per_sample;
		int		user_bytes_per_sample;
		int		cnt_factor;

		void		*rawbuf;
		unsigned        buforder;
		unsigned	numfrag;
		unsigned        fragshift;
		void		*nextIn;
		void		*nextOut;
		int		count;
		unsigned        total_bytes;
		unsigned        error;
		wait_queue_head_t wait;

		/* redundant, but makes calculations easier */
		unsigned	fragsize;
		unsigned	dma_fragsize;
		unsigned	dmasize;
		unsigned	dma_qcount;

		/* OSS stuff */
		unsigned        mapped:1;
		unsigned        ready:1;
		unsigned        stopped:1;
		unsigned        ossfragshift;
		int             ossmaxfrags;
		unsigned        subdivision;

		/* Mixer stuff */
		int		dev_mixer;
	} dma_dac, dma_adc;
} au1550_state;

static unsigned
ld2(unsigned int x)
{
	unsigned        r = 0;

	if (x >= 0x10000) {
		x >>= 16;
		r += 16;
	}
	if (x >= 0x100) {
		x >>= 8;
		r += 8;
	}
	if (x >= 0x10) {
		x >>= 4;
		r += 4;
	}
	if (x >= 4) {
		x >>= 2;
		r += 2;
	}
	if (x >= 2)
		r++;
	return r;
}

/* stop the ADC before calling */
static void
set_adc_rate(struct au1550_state *s, unsigned rate)
{
	struct dmabuf  *adc = &s->dma_adc;

	/* calc SRC factor */
	adc->src_factor = (((SAMP_RATE*2) / rate) + 1) >> 1;
	adc->sample_rate = SAMP_RATE / adc->src_factor;
	return;

	adc->src_factor = 1;
}

/* stop the DAC before calling */
static void
set_dac_rate(struct au1550_state *s, unsigned rate)
{
	struct dmabuf  *dac = &s->dma_dac;

	/* calc SRC factor */
	dac->src_factor = (((SAMP_RATE*2) / rate) + 1) >> 1;
	dac->sample_rate = SAMP_RATE / dac->src_factor;
	return;

	dac->src_factor = 1;
}

static void
stop_dac(struct au1550_state *s)
{
	struct dmabuf  *db = &s->dma_dac;
	unsigned long   flags;
	uint	stat;
	volatile psc_i2s_t *ip;

	if (db->stopped)
		return;

	ip = s->psc_addr;
	spin_lock_irqsave(&s->lock, flags);

	ip->psc_i2spcr = PSC_I2SPCR_TP;
	au_sync();

	/* Wait for Transmit Busy to show disabled.
	*/
	do {
		stat = ip->psc_i2sstat;
		au_sync();
	} while ((stat & PSC_I2SSTAT_TB) != 0);

	au1xxx_dbdma_reset(db->dmanr);

	db->stopped = 1;

	spin_unlock_irqrestore(&s->lock, flags);
}

static void
stop_adc(struct au1550_state *s)
{
	struct dmabuf  *db = &s->dma_adc;
	unsigned long   flags;
	uint	stat;
	volatile psc_i2s_t *ip;

	if (db->stopped)
		return;

	ip = s->psc_addr;
	spin_lock_irqsave(&s->lock, flags);

	ip->psc_i2spcr = PSC_I2SPCR_RP;
	au_sync();

	/* Wait for Receive Busy to show disabled.
	*/
	do {
		stat = ip->psc_i2sstat;
		au_sync();
	} while ((stat & PSC_I2SSTAT_RB) != 0);

	au1xxx_dbdma_reset(db->dmanr);

	db->stopped = 1;

	spin_unlock_irqrestore(&s->lock, flags);
}

static void
set_xmit_slots(int num_channels)
{
	/* This is here just as a place holder.  The WM8731 only
	 * supports two fixed channels.
	 */
}

static void
set_recv_slots(int num_channels)
{
	/* This is here just as a place holder.  The WM8731 only
	 * supports two fixed channels.
	 */
}

static void
start_dac(struct au1550_state *s)
{
	struct dmabuf  *db = &s->dma_dac;
	unsigned long   flags;
	volatile psc_i2s_t *ip;

	if (!db->stopped)
		return;

	spin_lock_irqsave(&s->lock, flags);

	ip = s->psc_addr;
	set_xmit_slots(db->num_channels);
	ip->psc_i2spcr = PSC_I2SPCR_TC;
	au_sync();
	ip->psc_i2spcr = PSC_I2SPCR_TS;
	au_sync();

	au1xxx_dbdma_start(db->dmanr);

	db->stopped = 0;

	spin_unlock_irqrestore(&s->lock, flags);
}

static void
start_adc(struct au1550_state *s)
{
	struct dmabuf  *db = &s->dma_adc;
	int	i;
	volatile psc_i2s_t *ip;

	if (!db->stopped)
		return;

	/* Put two buffers on the ring to get things started.
	*/
	for (i=0; i<2; i++) {
		au1xxx_dbdma_put_dest(db->dmanr, db->nextIn, db->dma_fragsize);

		db->nextIn += db->dma_fragsize;
		if (db->nextIn >= db->rawbuf + db->dmasize)
			db->nextIn -= db->dmasize;
	}

	ip = s->psc_addr;
	set_recv_slots(db->num_channels);
	au1xxx_dbdma_start(db->dmanr);
	ip->psc_i2spcr = PSC_I2SPCR_RC;
	au_sync();
	ip->psc_i2spcr = PSC_I2SPCR_RS;
	au_sync();

	db->stopped = 0;
}

static int
prog_dmabuf(struct au1550_state *s, struct dmabuf *db)
{
	unsigned user_bytes_per_sec;
	unsigned        bufs;
	unsigned        rate = db->sample_rate;

	if (!db->rawbuf) {
		db->ready = db->mapped = 0;
		db->buforder = 5;	/* 32 * PAGE_SIZE */
		db->rawbuf = kmalloc((PAGE_SIZE << db->buforder), GFP_KERNEL);
		if (!db->rawbuf)
			return -ENOMEM;
	}

	db->cnt_factor = 1;
	if (db->sample_size == 8)
		db->cnt_factor *= 2;
	if (db->num_channels == 1)
		db->cnt_factor *= 2;
	db->cnt_factor *= db->src_factor;

	db->count = 0;
	db->dma_qcount = 0;
	db->nextIn = db->nextOut = db->rawbuf;

	db->user_bytes_per_sample = (db->sample_size>>3) * db->num_channels;
	db->dma_bytes_per_sample = 2 * ((db->num_channels == 1) ?
					2 : db->num_channels);

	user_bytes_per_sec = rate * db->user_bytes_per_sample;
	bufs = PAGE_SIZE << db->buforder;
	if (db->ossfragshift) {
		if ((1000 << db->ossfragshift) < user_bytes_per_sec)
			db->fragshift = ld2(user_bytes_per_sec/1000);
		else
			db->fragshift = db->ossfragshift;
	} else {
		db->fragshift = ld2(user_bytes_per_sec / 100 /
				    (db->subdivision ? db->subdivision : 1));
		if (db->fragshift < 3)
			db->fragshift = 3;
	}

	db->fragsize = 1 << db->fragshift;
	db->dma_fragsize = db->fragsize * db->cnt_factor;
	db->numfrag = bufs / db->dma_fragsize;

	while (db->numfrag < 4 && db->fragshift > 3) {
		db->fragshift--;
		db->fragsize = 1 << db->fragshift;
		db->dma_fragsize = db->fragsize * db->cnt_factor;
		db->numfrag = bufs / db->dma_fragsize;
	}

	if (db->ossmaxfrags >= 4 && db->ossmaxfrags < db->numfrag)
		db->numfrag = db->ossmaxfrags;

	db->dmasize = db->dma_fragsize * db->numfrag;
	memset(db->rawbuf, 0, bufs);

#ifdef AU1000_VERBOSE_DEBUG
	dbg("rate=%d, samplesize=%d, channels=%d",
	    rate, db->sample_size, db->num_channels);
	dbg("fragsize=%d, cnt_factor=%d, dma_fragsize=%d",
	    db->fragsize, db->cnt_factor, db->dma_fragsize);
	dbg("numfrag=%d, dmasize=%d", db->numfrag, db->dmasize);
#endif

	db->ready = 1;
	return 0;
}

static int
prog_dmabuf_adc(struct au1550_state *s)
{
	stop_adc(s);
	return prog_dmabuf(s, &s->dma_adc);

}

static int
prog_dmabuf_dac(struct au1550_state *s)
{
	stop_dac(s);
	return prog_dmabuf(s, &s->dma_dac);
}


/* hold spinlock for the following */
static void
dac_dma_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct au1550_state *s = (struct au1550_state *) dev_id;
	struct dmabuf  *db = &s->dma_dac;
	u32	i2s_stat;
	volatile psc_i2s_t *ip;

	ip = s->psc_addr;
	i2s_stat = ip->psc_i2sstat;
#ifdef AU1000_VERBOSE_DEBUG
	if (i2s_stat & (PSC_I2SSTAT_TF | PSC_I2SSTAT_TR | PSC_I2SSTAT_TF))
		dbg("I2S status = 0x%08x", i2s_stat);
#endif
	db->dma_qcount--;

	if (db->count >= db->fragsize) {
		if (au1xxx_dbdma_put_source(db->dmanr, db->nextOut,
							db->fragsize) == 0) {
			pr_error("qcount < 2 and no ring room!");
		}
		db->nextOut += db->fragsize;
		if (db->nextOut >= db->rawbuf + db->dmasize)
			db->nextOut -= db->dmasize;
		db->count -= db->fragsize;
		db->total_bytes += db->dma_fragsize;
		db->dma_qcount++;
	}

	/* wake up anybody listening */
	if (waitqueue_active(&db->wait))
		wake_up(&db->wait);
}


static void
adc_dma_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct	au1550_state *s = (struct au1550_state *)dev_id;
	struct	dmabuf  *dp = &s->dma_adc;
	u32	obytes;
	char	*obuf;

	/* Pull the buffer from the dma queue.
	*/
	au1xxx_dbdma_get_dest(dp->dmanr, (void *)(&obuf), &obytes);

	if ((dp->count + obytes) > dp->dmasize) {
		/* Overrun. Stop ADC and log the error
		*/
		stop_adc(s);
		dp->error++;
		pr_error("adc overrun");
		return;
	}

	/* Put a new empty buffer on the destination DMA.
	*/
	au1xxx_dbdma_put_dest(dp->dmanr, dp->nextIn, dp->dma_fragsize);

	dp->nextIn += dp->dma_fragsize;
	if (dp->nextIn >= dp->rawbuf + dp->dmasize)
		dp->nextIn -= dp->dmasize;

	dp->count += obytes;
	dp->total_bytes += obytes;

	/* wake up anybody listening
	*/
	if (waitqueue_active(&dp->wait))
		wake_up(&dp->wait);

}

static loff_t
au1550_llseek(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}

static int
au1550_open_mixdev(struct inode *inode, struct file *file)
{
	file->private_data = &au1550_state;
	return 0;
}

static int
au1550_release_mixdev(struct inode *inode, struct file *file)
{
	return 0;
}

#define I2S_CODEC	"Wolfson WM8731"

static int
au1550_ioctl_mixdev(struct inode *inode, struct file *file,
			       unsigned int cmd, unsigned long arg)
{
	struct au1550_state *s = (struct au1550_state *)file->private_data;
	struct i2s_codec *codec = s->codec;
	int i, val = 0;

	if (cmd == SOUND_MIXER_INFO) {
		mixer_info info;
		memset(&info, 0, sizeof(info));
		strlcpy(info.id, I2S_CODEC, sizeof(info.id));
		strlcpy(info.name, I2S_CODEC, sizeof(info.name));
		info.modify_counter = codec->modcnt;
		if (copy_to_user((void __user *)arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}
	if (cmd == SOUND_OLD_MIXER_INFO) {
		_old_mixer_info info;
		memset(&info, 0, sizeof(info));
		strlcpy(info.id, I2S_CODEC, sizeof(info.id));
		strlcpy(info.name, I2S_CODEC, sizeof(info.name));
		if (copy_to_user((void __user *)arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}

	if (_IOC_TYPE(cmd) != 'M' || _SIOC_SIZE(cmd) != sizeof(int))
		return -EINVAL;

	if (cmd == OSS_GETVERSION)
		return put_user(SOUND_VERSION, (int __user *)arg);

	if (_SIOC_DIR(cmd) == _SIOC_READ) {
		switch (_IOC_NR(cmd)) {
		case SOUND_MIXER_RECSRC: /* give them the current record source */
			val = codec->mixer_state[SOUND_MIXER_RECSRC];
			break;

		case SOUND_MIXER_DEVMASK: /* give them the supported mixers */
			val = codec->supported_mixers;
			break;

		case SOUND_MIXER_RECMASK: /* Arg contains a bit for each supported recording source */
			val = codec->record_sources;
			break;

		case SOUND_MIXER_STEREODEVS: /* Mixer channels supporting stereo */
			val = codec->stereo_mixers;
			break;

		case SOUND_MIXER_CAPS:
			val = SOUND_CAP_EXCL_INPUT;
			break;

		default: /* read a specific mixer */
			i = _IOC_NR(cmd);

			if (!i2s_supported_mixer(codec, i))
				return -EINVAL;

			val = codec->mixer_state[i];
 			break;
		}
		return put_user(val, (int __user *)arg);
	}

	if (_SIOC_DIR(cmd) == (_SIOC_WRITE|_SIOC_READ)) {
		codec->modcnt++;
		if (get_user(val, (int __user *)arg))
			return -EFAULT;

		switch (_IOC_NR(cmd)) {
		case SOUND_MIXER_RECSRC: /* Arg contains a bit for each recording source */
			if (!val) return 0;
			if (!(val &= codec->record_sources)) return -EINVAL;

			codec->set_mixer(codec, SOUND_MIXER_RECSRC, val);

			return 0;
		default: /* write a specific mixer */
			i = _IOC_NR(cmd);

			if (!i2s_supported_mixer(codec, i))
				return -EINVAL;

			codec->set_mixer(codec, i, val);

			return 0;
		}
	}
	return -EINVAL;
}

static struct file_operations au1550_mixer_fops = {
	owner:THIS_MODULE,
	llseek:au1550_llseek,
	ioctl:au1550_ioctl_mixdev,
	open:au1550_open_mixdev,
	release:au1550_release_mixdev,
};

static int
drain_dac(struct au1550_state *s, int nonblock)
{
	unsigned long   flags;
	int             count, tmo;

	if (s->dma_dac.mapped || !s->dma_dac.ready || s->dma_dac.stopped)
		return 0;

	for (;;) {
		spin_lock_irqsave(&s->lock, flags);
		count = s->dma_dac.count;
		spin_unlock_irqrestore(&s->lock, flags);
		if (count <= 0)
			break;
		if (signal_pending(current))
			break;
		if (nonblock)
			return -EBUSY;
		tmo = 1000 * count / SAMP_RATE;
		tmo /= s->dma_dac.dma_bytes_per_sample;
		au1550_delay(tmo);
	}
	if (signal_pending(current))
		return -ERESTARTSYS;
	return 0;
}

static inline u8 S16_TO_U8(s16 ch)
{
	return (u8) (ch >> 8) + 0x80;
}
static inline s16 U8_TO_S16(u8 ch)
{
	return (s16) (ch - 0x80) << 8;
}

/*
 * Translates user samples to dma buffer suitable for audio DAC data:
 *     If mono, copy left channel to right channel in dma buffer.
 *     If 8 bit samples, cvt to 16-bit before writing to dma buffer.
 *     If interpolating (no VRA), duplicate every audio frame src_factor times.
 */
static int
translate_from_user(struct dmabuf *db, char* dmabuf, char* userbuf,
							       int dmacount)
{
	int             sample, i;
	int             interp_bytes_per_sample;
	int             num_samples;
	int             mono = (db->num_channels == 1);
	char            usersample[12];
	s16             ch, dmasample[6];

	if (db->sample_size == 16 && !mono && db->src_factor == 1) {
		/* no translation necessary, just copy
		*/
		if (copy_from_user(dmabuf, userbuf, dmacount))
			return -EFAULT;
		return dmacount;
	}

	interp_bytes_per_sample = db->dma_bytes_per_sample * db->src_factor;
	num_samples = dmacount / interp_bytes_per_sample;

	for (sample = 0; sample < num_samples; sample++) {
		if (copy_from_user(usersample, userbuf,
				   db->user_bytes_per_sample)) {
			return -EFAULT;
		}

		for (i = 0; i < db->num_channels; i++) {
			if (db->sample_size == 8)
				ch = U8_TO_S16(usersample[i]);
			else
				ch = *((s16 *) (&usersample[i * 2]));
			dmasample[i] = ch;
			if (mono)
				dmasample[i + 1] = ch;	/* right channel */
		}

		/* duplicate every audio frame src_factor times
		*/
		for (i = 0; i < db->src_factor; i++)
			memcpy(dmabuf, dmasample, db->dma_bytes_per_sample);

		userbuf += db->user_bytes_per_sample;
		dmabuf += interp_bytes_per_sample;
	}

	return num_samples * interp_bytes_per_sample;
}

/*
 * Translates audio ADC samples to user buffer:
 *     If mono, send only left channel to user buffer.
 *     If 8 bit samples, cvt from 16 to 8 bit before writing to user buffer.
 *     If decimating (no VRA), skip over src_factor audio frames.
 */
static int
translate_to_user(struct dmabuf *db, char* userbuf, char* dmabuf,
							     int dmacount)
{
	int             sample, i;
	int             interp_bytes_per_sample;
	int             num_samples;
	int             mono = (db->num_channels == 1);
	char            usersample[12];

	if (db->sample_size == 16 && !mono && db->src_factor == 1) {
		/* no translation necessary, just copy
		*/
		if (copy_to_user(userbuf, dmabuf, dmacount))
			return -EFAULT;
		return dmacount;
	}

	interp_bytes_per_sample = db->dma_bytes_per_sample * db->src_factor;
	num_samples = dmacount / interp_bytes_per_sample;

	for (sample = 0; sample < num_samples; sample++) {
		for (i = 0; i < db->num_channels; i++) {
			if (db->sample_size == 8)
				usersample[i] =
					S16_TO_U8(*((s16 *) (&dmabuf[i * 2])));
			else
				*((s16 *) (&usersample[i * 2])) =
					*((s16 *) (&dmabuf[i * 2]));
		}

		if (copy_to_user(userbuf, usersample,
				 db->user_bytes_per_sample)) {
			return -EFAULT;
		}

		userbuf += db->user_bytes_per_sample;
		dmabuf += interp_bytes_per_sample;
	}

	return num_samples * interp_bytes_per_sample;
}

/*
 * Copy audio data to/from user buffer from/to dma buffer, taking care
 * that we wrap when reading/writing the dma buffer. Returns actual byte
 * count written to or read from the dma buffer.
 */
static int
copy_dmabuf_user(struct dmabuf *db, char* userbuf, int count, int to_user)
{
	char           *bufptr = to_user ? db->nextOut : db->nextIn;
	char           *bufend = db->rawbuf + db->dmasize;
	int             cnt, ret;

	if (bufptr + count > bufend) {
		int             partial = (int) (bufend - bufptr);
		if (to_user) {
			if ((cnt = translate_to_user(db, userbuf,
						     bufptr, partial)) < 0)
				return cnt;
			ret = cnt;
			if ((cnt = translate_to_user(db, userbuf + partial,
						     db->rawbuf,
						     count - partial)) < 0)
				return cnt;
			ret += cnt;
		} else {
			if ((cnt = translate_from_user(db, bufptr, userbuf,
						       partial)) < 0)
				return cnt;
			ret = cnt;
			if ((cnt = translate_from_user(db, db->rawbuf,
						       userbuf + partial,
						       count - partial)) < 0)
				return cnt;
			ret += cnt;
		}
	} else {
		if (to_user)
			ret = translate_to_user(db, userbuf, bufptr, count);
		else
			ret = translate_from_user(db, bufptr, userbuf, count);
	}

	return ret;
}

static ssize_t
au1550_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	struct au1550_state *s = (struct au1550_state *)file->private_data;
	struct dmabuf  *db = &s->dma_adc;
	DECLARE_WAITQUEUE(wait, current);
	ssize_t         ret;
	unsigned long   flags;
	int             cnt, usercnt, avail;

	if (db->mapped)
		return -ENXIO;
	if (!access_ok(VERIFY_WRITE, buffer, count))
		return -EFAULT;
	ret = 0;

	count *= db->cnt_factor;

	down(&s->sem);
	add_wait_queue(&db->wait, &wait);

	while (count > 0) {
		/* wait for samples in ADC dma buffer
		*/
		do {
			if (db->stopped)
				start_adc(s);
			spin_lock_irqsave(&s->lock, flags);
			avail = db->count;
			if (avail <= 0)
				__set_current_state(TASK_INTERRUPTIBLE);
			spin_unlock_irqrestore(&s->lock, flags);
			if (avail <= 0) {
				if (file->f_flags & O_NONBLOCK) {
					if (!ret)
						ret = -EAGAIN;
					goto out;
				}
				up(&s->sem);
				schedule();
				if (signal_pending(current)) {
					if (!ret)
						ret = -ERESTARTSYS;
					goto out2;
				}
				down(&s->sem);
			}
		} while (avail <= 0);

		/* copy from nextOut to user
		*/
		if ((cnt = copy_dmabuf_user(db, buffer,
					    count > avail ?
					    avail : count, 1)) < 0) {
			if (!ret)
				ret = -EFAULT;
			goto out;
		}

		spin_lock_irqsave(&s->lock, flags);
		db->count -= cnt;
		db->nextOut += cnt;
		if (db->nextOut >= db->rawbuf + db->dmasize)
			db->nextOut -= db->dmasize;
		spin_unlock_irqrestore(&s->lock, flags);

		count -= cnt;
		usercnt = cnt / db->cnt_factor;
		buffer += usercnt;
		ret += usercnt;
	}			/* while (count > 0) */

out:
	up(&s->sem);
out2:
	remove_wait_queue(&db->wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

static ssize_t
au1550_write(struct file *file, const char *buffer, size_t count, loff_t * ppos)
{
	struct au1550_state *s = (struct au1550_state *)file->private_data;
	struct dmabuf  *db = &s->dma_dac;
	DECLARE_WAITQUEUE(wait, current);
	ssize_t         ret = 0;
	unsigned long   flags;
	int             cnt, usercnt, avail;

#ifdef AU1000_VERBOSE_DEBUG
	dbg("write: count=%d", count);
#endif

	if (db->mapped)
		return -ENXIO;
	if (!access_ok(VERIFY_READ, buffer, count))
		return -EFAULT;

	count *= db->cnt_factor;

	down(&s->sem);
	add_wait_queue(&db->wait, &wait);

	while (count > 0) {
		/* wait for space in playback buffer
		*/
		do {
			spin_lock_irqsave(&s->lock, flags);
			avail = (int) db->dmasize - db->count;
			if (avail <= 0)
				__set_current_state(TASK_INTERRUPTIBLE);
			spin_unlock_irqrestore(&s->lock, flags);
			if (avail <= 0) {
				if (file->f_flags & O_NONBLOCK) {
					if (!ret)
						ret = -EAGAIN;
					goto out;
				}
				up(&s->sem);
				schedule();
				if (signal_pending(current)) {
					if (!ret)
						ret = -ERESTARTSYS;
					goto out2;
				}
				down(&s->sem);
			}
		} while (avail <= 0);

		/* copy from user to nextIn
		*/
		if ((cnt = copy_dmabuf_user(db, (char *) buffer,
					    count > avail ?
					    avail : count, 0)) < 0) {
			if (!ret)
				ret = -EFAULT;
			goto out;
		}

		spin_lock_irqsave(&s->lock, flags);
		db->count += cnt;
		db->nextIn += cnt;
		if (db->nextIn >= db->rawbuf + db->dmasize)
			db->nextIn -= db->dmasize;

		/* If the data is available, we want to keep two buffers
		 * on the dma queue.  If the queue count reaches zero,
		 * we know the dma has stopped.
		 */
		while ((db->dma_qcount < 2) && (db->count >= db->fragsize)) {
			if (au1xxx_dbdma_put_source(db->dmanr, db->nextOut,
							db->fragsize) == 0) {
				pr_error("qcount < 2 and no ring room!");
			}
			db->nextOut += db->fragsize;
			if (db->nextOut >= db->rawbuf + db->dmasize)
				db->nextOut -= db->dmasize;
			db->count -= db->fragsize;
			db->total_bytes += db->dma_fragsize;
			if (db->dma_qcount == 0)
				start_dac(s);
			db->dma_qcount++;
		}
		spin_unlock_irqrestore(&s->lock, flags);

		count -= cnt;
		usercnt = cnt / db->cnt_factor;
		buffer += usercnt;
		ret += usercnt;
	}			/* while (count > 0) */

out:
	up(&s->sem);
out2:
	remove_wait_queue(&db->wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}


/* No kernel lock - we have our own spinlock */
static unsigned int
au1550_poll(struct file *file, struct poll_table_struct *wait)
{
	struct au1550_state *s = (struct au1550_state *)file->private_data;
	unsigned long   flags;
	unsigned int    mask = 0;

	if (file->f_mode & FMODE_WRITE) {
		if (!s->dma_dac.ready)
			return 0;
		poll_wait(file, &s->dma_dac.wait, wait);
	}
	if (file->f_mode & FMODE_READ) {
		if (!s->dma_adc.ready)
			return 0;
		poll_wait(file, &s->dma_adc.wait, wait);
	}

	spin_lock_irqsave(&s->lock, flags);

	if (file->f_mode & FMODE_READ) {
		if (s->dma_adc.count >= (signed)s->dma_adc.dma_fragsize)
			mask |= POLLIN | POLLRDNORM;
	}
	if (file->f_mode & FMODE_WRITE) {
		if (s->dma_dac.mapped) {
			if (s->dma_dac.count >=
			    (signed)s->dma_dac.dma_fragsize)
				mask |= POLLOUT | POLLWRNORM;
		} else {
			if ((signed) s->dma_dac.dmasize >=
			    s->dma_dac.count + (signed)s->dma_dac.dma_fragsize)
				mask |= POLLOUT | POLLWRNORM;
		}
	}
	spin_unlock_irqrestore(&s->lock, flags);
	return mask;
}

static int
au1550_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct au1550_state *s = (struct au1550_state *)file->private_data;
	struct dmabuf  *db;
	unsigned long   size;
	int ret = 0;

	lock_kernel();
	down(&s->sem);
	if (vma->vm_flags & VM_WRITE)
		db = &s->dma_dac;
	else if (vma->vm_flags & VM_READ)
		db = &s->dma_adc;
	else {
		ret = -EINVAL;
		goto out;
	}
	if (vma->vm_pgoff != 0) {
		ret = -EINVAL;
		goto out;
	}
	size = vma->vm_end - vma->vm_start;
	if (size > (PAGE_SIZE << db->buforder)) {
		ret = -EINVAL;
		goto out;
	}
	if (remap_pfn_range(vma, vma->vm_start,
			     page_to_pfn(virt_to_page(db->rawbuf)),
			     size, vma->vm_page_prot)) {
		ret = -EAGAIN;
		goto out;
	}
	vma->vm_flags &= ~VM_IO;
	db->mapped = 1;
out:
	up(&s->sem);
	unlock_kernel();
	return ret;
}


#ifdef AU1000_VERBOSE_DEBUG
static struct ioctl_str_t {
	unsigned int    cmd;
	const char     *str;
} ioctl_str[] = {
	{SNDCTL_DSP_RESET, "SNDCTL_DSP_RESET"},
	{SNDCTL_DSP_SYNC, "SNDCTL_DSP_SYNC"},
	{SNDCTL_DSP_SPEED, "SNDCTL_DSP_SPEED"},
	{SNDCTL_DSP_STEREO, "SNDCTL_DSP_STEREO"},
	{SNDCTL_DSP_GETBLKSIZE, "SNDCTL_DSP_GETBLKSIZE"},
	{SNDCTL_DSP_SAMPLESIZE, "SNDCTL_DSP_SAMPLESIZE"},
	{SNDCTL_DSP_CHANNELS, "SNDCTL_DSP_CHANNELS"},
	{SOUND_PCM_WRITE_CHANNELS, "SOUND_PCM_WRITE_CHANNELS"},
	{SOUND_PCM_WRITE_FILTER, "SOUND_PCM_WRITE_FILTER"},
	{SNDCTL_DSP_POST, "SNDCTL_DSP_POST"},
	{SNDCTL_DSP_SUBDIVIDE, "SNDCTL_DSP_SUBDIVIDE"},
	{SNDCTL_DSP_SETFRAGMENT, "SNDCTL_DSP_SETFRAGMENT"},
	{SNDCTL_DSP_GETFMTS, "SNDCTL_DSP_GETFMTS"},
	{SNDCTL_DSP_SETFMT, "SNDCTL_DSP_SETFMT"},
	{SNDCTL_DSP_GETOSPACE, "SNDCTL_DSP_GETOSPACE"},
	{SNDCTL_DSP_GETISPACE, "SNDCTL_DSP_GETISPACE"},
	{SNDCTL_DSP_NONBLOCK, "SNDCTL_DSP_NONBLOCK"},
	{SNDCTL_DSP_GETCAPS, "SNDCTL_DSP_GETCAPS"},
	{SNDCTL_DSP_GETTRIGGER, "SNDCTL_DSP_GETTRIGGER"},
	{SNDCTL_DSP_SETTRIGGER, "SNDCTL_DSP_SETTRIGGER"},
	{SNDCTL_DSP_GETIPTR, "SNDCTL_DSP_GETIPTR"},
	{SNDCTL_DSP_GETOPTR, "SNDCTL_DSP_GETOPTR"},
	{SNDCTL_DSP_MAPINBUF, "SNDCTL_DSP_MAPINBUF"},
	{SNDCTL_DSP_MAPOUTBUF, "SNDCTL_DSP_MAPOUTBUF"},
	{SNDCTL_DSP_SETSYNCRO, "SNDCTL_DSP_SETSYNCRO"},
	{SNDCTL_DSP_SETDUPLEX, "SNDCTL_DSP_SETDUPLEX"},
	{SNDCTL_DSP_GETODELAY, "SNDCTL_DSP_GETODELAY"},
	{SNDCTL_DSP_GETCHANNELMASK, "SNDCTL_DSP_GETCHANNELMASK"},
	{SNDCTL_DSP_BIND_CHANNEL, "SNDCTL_DSP_BIND_CHANNEL"},
	{OSS_GETVERSION, "OSS_GETVERSION"},
	{SOUND_PCM_READ_RATE, "SOUND_PCM_READ_RATE"},
	{SOUND_PCM_READ_CHANNELS, "SOUND_PCM_READ_CHANNELS"},
	{SOUND_PCM_READ_BITS, "SOUND_PCM_READ_BITS"},
	{SOUND_PCM_READ_FILTER, "SOUND_PCM_READ_FILTER"}
};
#endif

static int
dma_count_done(struct dmabuf *db)
{
	if (db->stopped)
		return 0;

	return db->dma_fragsize - au1xxx_get_dma_residue(db->dmanr);
}


static int
au1550_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
							unsigned long arg)
{
	struct au1550_state *s = (struct au1550_state *)file->private_data;
	unsigned long   flags;
	audio_buf_info  abinfo;
	count_info      cinfo;
	int             count;
	int             val, mapped, ret, diff;

	mapped = ((file->f_mode & FMODE_WRITE) && s->dma_dac.mapped) ||
		((file->f_mode & FMODE_READ) && s->dma_adc.mapped);

#ifdef AU1000_VERBOSE_DEBUG
	for (count=0; count<sizeof(ioctl_str)/sizeof(ioctl_str[0]); count++) {
		if (ioctl_str[count].cmd == cmd)
			break;
	}
	if (count < sizeof(ioctl_str) / sizeof(ioctl_str[0]))
		dbg("ioctl %s, arg=0x%lx", ioctl_str[count].str, arg);
	else
		dbg("ioctl 0x%x unknown, arg=0x%lx", cmd, arg);
#endif

	switch (cmd) {
	case OSS_GETVERSION:
		return put_user(SOUND_VERSION, (int *) arg);

	case SNDCTL_DSP_SYNC:
		if (file->f_mode & FMODE_WRITE)
			return drain_dac(s, file->f_flags & O_NONBLOCK);
		return 0;

	case SNDCTL_DSP_SETDUPLEX:
		return 0;

	case SNDCTL_DSP_GETCAPS:
		return put_user(DSP_CAP_DUPLEX | DSP_CAP_REALTIME |
				DSP_CAP_TRIGGER | DSP_CAP_MMAP, (int *)arg);

	case SNDCTL_DSP_RESET:
		if (file->f_mode & FMODE_WRITE) {
			stop_dac(s);
			synchronize_irq();
			s->dma_dac.count = s->dma_dac.total_bytes = 0;
			s->dma_dac.nextIn = s->dma_dac.nextOut =
				s->dma_dac.rawbuf;
		}
		if (file->f_mode & FMODE_READ) {
			stop_adc(s);
			synchronize_irq();
			s->dma_adc.count = s->dma_adc.total_bytes = 0;
			s->dma_adc.nextIn = s->dma_adc.nextOut =
				s->dma_adc.rawbuf;
		}
		return 0;

	case SNDCTL_DSP_SPEED:
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (val >= 0) {
			if (file->f_mode & FMODE_READ) {
				stop_adc(s);
				set_adc_rate(s, val);
			}
			if (file->f_mode & FMODE_WRITE) {
				stop_dac(s);
				set_dac_rate(s, val);
			}
			if (s->open_mode & FMODE_READ)
				if ((ret = prog_dmabuf_adc(s)))
					return ret;
			if (s->open_mode & FMODE_WRITE)
				if ((ret = prog_dmabuf_dac(s)))
					return ret;
		}
		return put_user((file->f_mode & FMODE_READ) ?
				s->dma_adc.sample_rate :
				s->dma_dac.sample_rate,
				(int *)arg);

	case SNDCTL_DSP_STEREO:
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (file->f_mode & FMODE_READ) {
			stop_adc(s);
			s->dma_adc.num_channels = val ? 2 : 1;
			if ((ret = prog_dmabuf_adc(s)))
				return ret;
		}
		if (file->f_mode & FMODE_WRITE) {
			stop_dac(s);
			s->dma_dac.num_channels = val ? 2 : 1;
			if ((ret = prog_dmabuf_dac(s)))
				return ret;
		}
		return 0;

	case SNDCTL_DSP_CHANNELS:
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (val != 0) {
			if (file->f_mode & FMODE_READ) {
				if (val < 0 || val > 2)
					return -EINVAL;
				stop_adc(s);
				s->dma_adc.num_channels = val;
				if ((ret = prog_dmabuf_adc(s)))
					return ret;
			}
			if (file->f_mode & FMODE_WRITE) {
				switch (val) {
				case 1:
				case 2:
					break;
				default:
					return -EINVAL;
				}

				stop_dac(s);
				s->dma_dac.num_channels = val;
				if ((ret = prog_dmabuf_dac(s)))
					return ret;
			}
		}
		return put_user(val, (int *) arg);

	case SNDCTL_DSP_GETFMTS:	/* Returns a mask */
		return put_user(AFMT_S16_LE | AFMT_U8, (int *) arg);

	case SNDCTL_DSP_SETFMT:	/* Selects ONE fmt */
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (val != AFMT_QUERY) {
			if (file->f_mode & FMODE_READ) {
				stop_adc(s);
				if (val == AFMT_S16_LE)
					s->dma_adc.sample_size = 16;
				else {
					val = AFMT_U8;
					s->dma_adc.sample_size = 8;
				}
				if ((ret = prog_dmabuf_adc(s)))
					return ret;
			}
			if (file->f_mode & FMODE_WRITE) {
				stop_dac(s);
				if (val == AFMT_S16_LE)
					s->dma_dac.sample_size = 16;
				else {
					val = AFMT_U8;
					s->dma_dac.sample_size = 8;
				}
				if ((ret = prog_dmabuf_dac(s)))
					return ret;
			}
		} else {
			if (file->f_mode & FMODE_READ)
				val = (s->dma_adc.sample_size == 16) ?
					AFMT_S16_LE : AFMT_U8;
			else
				val = (s->dma_dac.sample_size == 16) ?
					AFMT_S16_LE : AFMT_U8;
		}
		return put_user(val, (int *) arg);

	case SNDCTL_DSP_POST:
		return 0;

	case SNDCTL_DSP_GETTRIGGER:
		val = 0;
		spin_lock_irqsave(&s->lock, flags);
		if (file->f_mode & FMODE_READ && !s->dma_adc.stopped)
			val |= PCM_ENABLE_INPUT;
		if (file->f_mode & FMODE_WRITE && !s->dma_dac.stopped)
			val |= PCM_ENABLE_OUTPUT;
		spin_unlock_irqrestore(&s->lock, flags);
		return put_user(val, (int *) arg);

	case SNDCTL_DSP_SETTRIGGER:
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (file->f_mode & FMODE_READ) {
			if (val & PCM_ENABLE_INPUT)
				start_adc(s);
			else
				stop_adc(s);
		}
		if (file->f_mode & FMODE_WRITE) {
			if (val & PCM_ENABLE_OUTPUT)
				start_dac(s);
			else
				stop_dac(s);
		}
		return 0;

	case SNDCTL_DSP_GETOSPACE:
		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
		abinfo.fragsize = s->dma_dac.fragsize;
		spin_lock_irqsave(&s->lock, flags);
		count = s->dma_dac.count;
		count -= dma_count_done(&s->dma_dac);
		spin_unlock_irqrestore(&s->lock, flags);
		if (count < 0)
			count = 0;
		abinfo.bytes = (s->dma_dac.dmasize - count) /
			s->dma_dac.cnt_factor;
		abinfo.fragstotal = s->dma_dac.numfrag;
		abinfo.fragments = abinfo.bytes >> s->dma_dac.fragshift;
#ifdef AU1000_VERBOSE_DEBUG
		dbg("bytes=%d, fragments=%d", abinfo.bytes, abinfo.fragments);
#endif
		return copy_to_user((void *) arg, &abinfo,
				    sizeof(abinfo)) ? -EFAULT : 0;

	case SNDCTL_DSP_GETISPACE:
		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;
		abinfo.fragsize = s->dma_adc.fragsize;
		spin_lock_irqsave(&s->lock, flags);
		count = s->dma_adc.count;
		count += dma_count_done(&s->dma_adc);
		spin_unlock_irqrestore(&s->lock, flags);
		if (count < 0)
			count = 0;
		abinfo.bytes = count / s->dma_adc.cnt_factor;
		abinfo.fragstotal = s->dma_adc.numfrag;
		abinfo.fragments = abinfo.bytes >> s->dma_adc.fragshift;
		return copy_to_user((void *) arg, &abinfo,
				    sizeof(abinfo)) ? -EFAULT : 0;

	case SNDCTL_DSP_NONBLOCK:
		file->f_flags |= O_NONBLOCK;
		return 0;

	case SNDCTL_DSP_GETODELAY:
		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
		spin_lock_irqsave(&s->lock, flags);
		count = s->dma_dac.count;
		count -= dma_count_done(&s->dma_dac);
		spin_unlock_irqrestore(&s->lock, flags);
		if (count < 0)
			count = 0;
		count /= s->dma_dac.cnt_factor;
		return put_user(count, (int *) arg);

	case SNDCTL_DSP_GETIPTR:
		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;
		spin_lock_irqsave(&s->lock, flags);
		cinfo.bytes = s->dma_adc.total_bytes;
		count = s->dma_adc.count;
		if (!s->dma_adc.stopped) {
			diff = dma_count_done(&s->dma_adc);
			count += diff;
			cinfo.bytes += diff;
			cinfo.ptr =  virt_to_phys(s->dma_adc.nextIn) + diff -
				virt_to_phys(s->dma_adc.rawbuf);
		} else
			cinfo.ptr = virt_to_phys(s->dma_adc.nextIn) -
				virt_to_phys(s->dma_adc.rawbuf);
		if (s->dma_adc.mapped)
			s->dma_adc.count &= (s->dma_adc.dma_fragsize-1);
		spin_unlock_irqrestore(&s->lock, flags);
		if (count < 0)
			count = 0;
		cinfo.blocks = count >> s->dma_adc.fragshift;
		return copy_to_user((void *) arg, &cinfo, sizeof(cinfo));

	case SNDCTL_DSP_GETOPTR:
		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;
		spin_lock_irqsave(&s->lock, flags);
		cinfo.bytes = s->dma_dac.total_bytes;
		count = s->dma_dac.count;
		if (!s->dma_dac.stopped) {
			diff = dma_count_done(&s->dma_dac);
			count -= diff;
			cinfo.bytes += diff;
			cinfo.ptr = virt_to_phys(s->dma_dac.nextOut) + diff -
				virt_to_phys(s->dma_dac.rawbuf);
		} else
			cinfo.ptr = virt_to_phys(s->dma_dac.nextOut) -
				virt_to_phys(s->dma_dac.rawbuf);
		if (s->dma_dac.mapped)
			s->dma_dac.count &= (s->dma_dac.dma_fragsize-1);
		spin_unlock_irqrestore(&s->lock, flags);
		if (count < 0)
			count = 0;
		cinfo.blocks = count >> s->dma_dac.fragshift;
		return copy_to_user((void *) arg, &cinfo, sizeof(cinfo));

	case SNDCTL_DSP_GETBLKSIZE:
		if (file->f_mode & FMODE_WRITE)
			return put_user(s->dma_dac.fragsize, (int *) arg);
		else
			return put_user(s->dma_adc.fragsize, (int *) arg);

	case SNDCTL_DSP_SETFRAGMENT:
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (file->f_mode & FMODE_READ) {
			stop_adc(s);
			s->dma_adc.ossfragshift = val & 0xffff;
			s->dma_adc.ossmaxfrags = (val >> 16) & 0xffff;
			if (s->dma_adc.ossfragshift < 4)
				s->dma_adc.ossfragshift = 4;
			if (s->dma_adc.ossfragshift > 15)
				s->dma_adc.ossfragshift = 15;
			if (s->dma_adc.ossmaxfrags < 4)
				s->dma_adc.ossmaxfrags = 4;
			if ((ret = prog_dmabuf_adc(s)))
				return ret;
		}
		if (file->f_mode & FMODE_WRITE) {
			stop_dac(s);
			s->dma_dac.ossfragshift = val & 0xffff;
			s->dma_dac.ossmaxfrags = (val >> 16) & 0xffff;
			if (s->dma_dac.ossfragshift < 4)
				s->dma_dac.ossfragshift = 4;
			if (s->dma_dac.ossfragshift > 15)
				s->dma_dac.ossfragshift = 15;
			if (s->dma_dac.ossmaxfrags < 4)
				s->dma_dac.ossmaxfrags = 4;
			if ((ret = prog_dmabuf_dac(s)))
				return ret;
		}
		return 0;

	case SNDCTL_DSP_SUBDIVIDE:
		if ((file->f_mode & FMODE_READ && s->dma_adc.subdivision) ||
		    (file->f_mode & FMODE_WRITE && s->dma_dac.subdivision))
			return -EINVAL;
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (val != 1 && val != 2 && val != 4)
			return -EINVAL;
		if (file->f_mode & FMODE_READ) {
			stop_adc(s);
			s->dma_adc.subdivision = val;
			if ((ret = prog_dmabuf_adc(s)))
				return ret;
		}
		if (file->f_mode & FMODE_WRITE) {
			stop_dac(s);
			s->dma_dac.subdivision = val;
			if ((ret = prog_dmabuf_dac(s)))
				return ret;
		}
		return 0;

	case SOUND_PCM_READ_RATE:
		return put_user((file->f_mode & FMODE_READ) ?
				s->dma_adc.sample_rate :
				s->dma_dac.sample_rate,
				(int *)arg);

	case SOUND_PCM_READ_CHANNELS:
		if (file->f_mode & FMODE_READ)
			return put_user(s->dma_adc.num_channels, (int *)arg);
		else
			return put_user(s->dma_dac.num_channels, (int *)arg);

	case SOUND_PCM_READ_BITS:
		if (file->f_mode & FMODE_READ)
			return put_user(s->dma_adc.sample_size, (int *)arg);
		else
			return put_user(s->dma_dac.sample_size, (int *)arg);

	case SOUND_PCM_WRITE_FILTER:
	case SNDCTL_DSP_SETSYNCRO:
	case SOUND_PCM_READ_FILTER:
		return -EINVAL;
	}

	return 0;
}


static int
au1550_open(struct inode *inode, struct file *file)
{
	int             minor = MINOR(inode->i_rdev);
	DECLARE_WAITQUEUE(wait, current);
	struct au1550_state *s = &au1550_state;
	int             ret;

#ifdef AU1000_VERBOSE_DEBUG
	if (file->f_flags & O_NONBLOCK)
		dbg(__FUNCTION__ ": non-blocking");
	else
		dbg(__FUNCTION__ ": blocking");
#endif

	file->private_data = s;
	/* wait for device to become free */
	down(&s->open_sem);
	while (s->open_mode & file->f_mode) {
		if (file->f_flags & O_NONBLOCK) {
			up(&s->open_sem);
			return -EBUSY;
		}
		add_wait_queue(&s->open_wait, &wait);
		__set_current_state(TASK_INTERRUPTIBLE);
		up(&s->open_sem);
		schedule();
		remove_wait_queue(&s->open_wait, &wait);
		set_current_state(TASK_RUNNING);
		if (signal_pending(current))
			return -ERESTARTSYS;
		down(&s->open_sem);
	}

	stop_dac(s);
	stop_adc(s);

	if (file->f_mode & FMODE_READ) {
		s->dma_adc.ossfragshift = s->dma_adc.ossmaxfrags =
			s->dma_adc.subdivision = s->dma_adc.total_bytes = 0;
		s->dma_adc.num_channels = 1;
		s->dma_adc.sample_size = 8;
		set_adc_rate(s, 8000);
		if ((minor & 0xf) == SND_DEV_DSP16)
			s->dma_adc.sample_size = 16;
	}

	if (file->f_mode & FMODE_WRITE) {
		s->dma_dac.ossfragshift = s->dma_dac.ossmaxfrags =
			s->dma_dac.subdivision = s->dma_dac.total_bytes = 0;
		s->dma_dac.num_channels = 1;
		s->dma_dac.sample_size = 8;
		set_dac_rate(s, 8000);
		if ((minor & 0xf) == SND_DEV_DSP16)
			s->dma_dac.sample_size = 16;
	}

	if (file->f_mode & FMODE_READ) {
		if ((ret = prog_dmabuf_adc(s)))
			return ret;
	}
	if (file->f_mode & FMODE_WRITE) {
		if ((ret = prog_dmabuf_dac(s)))
			return ret;
	}

	s->open_mode |= file->f_mode & (FMODE_READ | FMODE_WRITE);
	up(&s->open_sem);
	init_MUTEX(&s->sem);
	return 0;
}

static int
au1550_release(struct inode *inode, struct file *file)
{
	struct au1550_state *s = (struct au1550_state *)file->private_data;

	lock_kernel();

	if (file->f_mode & FMODE_WRITE) {
		unlock_kernel();
		drain_dac(s, file->f_flags & O_NONBLOCK);
		lock_kernel();
	}

	down(&s->open_sem);
	if (file->f_mode & FMODE_WRITE) {
		stop_dac(s);
		kfree(s->dma_dac.rawbuf);
		s->dma_dac.rawbuf = NULL;
	}
	if (file->f_mode & FMODE_READ) {
		stop_adc(s);
		kfree(s->dma_adc.rawbuf);
		s->dma_adc.rawbuf = NULL;
	}
	s->open_mode &= ((~file->f_mode) & (FMODE_READ|FMODE_WRITE));
	up(&s->open_sem);
	wake_up(&s->open_wait);
	unlock_kernel();
	return 0;
}

static struct file_operations au1550_audio_fops = {
	owner:		THIS_MODULE,
	llseek:		au1550_llseek,
	read:		au1550_read,
	write:		au1550_write,
	poll:		au1550_poll,
	ioctl:		au1550_ioctl,
	mmap:		au1550_mmap,
	open:		au1550_open,
	release:	au1550_release,
};

/* Set up an internal clock for the PSC3.  This will then get
 * driven out of the Au1550 as the master.
 */
static void
intclk_setup(void)
{
	uint clk, rate;

	/* Wire up Freq4 as a clock for the PSC3.
	 * We know SMBus uses Freq3.
	 * By making changes to this rate, plus the word strobe
	 * size, we can make fine adjustments to the actual data rate.
	 */
	rate = get_au1x00_speed();
#ifdef TRY_441KHz
	rate /= (11 * 1000000);
#else
	rate /= (12 * 1000000);
#endif

	/* The FRDIV in the frequency control is (FRDIV + 1) * 2
	*/
	rate /=2;
	rate--;
	clk = au_readl(SYS_FREQCTRL1);
	au_sync();
	clk &= ~(SYS_FC_FRDIV4_MASK | SYS_FC_FS4);;
	clk |= (rate << SYS_FC_FRDIV4_BIT);
	clk |= SYS_FC_FE4;
	au_writel(clk, SYS_FREQCTRL1);
	au_sync();

	/* Set up the clock source routing to get Freq4 to PSC3_intclk.
	*/
	clk = au_readl(SYS_CLKSRC);
	au_sync();
	clk &= ~0x01f00000;
	clk |= (6 << 22);
	au_writel(clk, SYS_CLKSRC);
	au_sync();
}

static int __devinit
au1550_probe(void)
{
	struct au1550_state *s = &au1550_state;
	int val;
	volatile psc_i2s_t *ip;
#ifdef AU1550_DEBUG
	char proc_str[80];
#endif

	memset(s, 0, sizeof(struct au1550_state));

	init_waitqueue_head(&s->dma_adc.wait);
	init_waitqueue_head(&s->dma_dac.wait);
	init_waitqueue_head(&s->open_wait);
	init_MUTEX(&s->open_sem);
	spin_lock_init(&s->lock);

	s->codec = &au1550_i2s_codec;
	s->psc_addr = (volatile psc_i2s_t *)I2S_PSC_BASE;
	ip = s->psc_addr;

	if (!request_region(CPHYSADDR(ip),
			    0x30, AU1550_MODULE_NAME)) {
		pr_error("I2S Audio ports in use");
	}

	/* Allocate the DMA Channels
	*/
	if ((s->dma_dac.dmanr = au1xxx_dbdma_chan_alloc(DBDMA_MEM_CHAN,
	    DBDMA_I2S_TX_CHAN, dac_dma_interrupt, (void *)s)) == 0) {
		pr_error("Can't get DAC DMA");
		goto err_dma1;
	}
	au1xxx_dbdma_set_devwidth(s->dma_dac.dmanr, 16);
	if (au1xxx_dbdma_ring_alloc(s->dma_dac.dmanr,
					NUM_DBDMA_DESCRIPTORS) == 0) {
		pr_error("Can't get DAC DMA descriptors");
		goto err_dma1;
	}

	if ((s->dma_adc.dmanr = au1xxx_dbdma_chan_alloc(DBDMA_I2S_RX_CHAN,
	    DBDMA_MEM_CHAN, adc_dma_interrupt, (void *)s)) == 0) {
		pr_error("Can't get ADC DMA");
		goto err_dma2;
	}
	au1xxx_dbdma_set_devwidth(s->dma_adc.dmanr, 16);
	if (au1xxx_dbdma_ring_alloc(s->dma_adc.dmanr,
					NUM_DBDMA_DESCRIPTORS) == 0) {
		pr_error("Can't get ADC DMA descriptors");
		goto err_dma2;
	}

	pr_info("DAC: DMA%d, ADC: DMA%d", DBDMA_I2S_TX_CHAN, DBDMA_I2S_RX_CHAN);

	/* register devices */

	if ((s->dev_audio = register_sound_dsp(&au1550_audio_fops, -1)) < 0)
		goto err_dev1;

	if ((s->dev_mixer = register_sound_mixer(&au1550_mixer_fops, -1)) < 0)
		goto err_dev2;

#ifdef AU1550_DEBUG
	/* intialize the debug proc device */
	s->ps = create_proc_read_entry(AU1000_MODULE_NAME, 0, NULL,
				       proc_au1550_dump, NULL);
#endif /* AU1550_DEBUG */

	intclk_setup();

	/* The GPIO for the appropriate PSC was configured by the
	 * board specific start up.
	 *
	 * configure PSC for I2S Audio
	 */
	ip->psc_ctrl = PSC_CTRL_DISABLE;	/* Disable PSC */
	au_sync();
	ip->psc_sel = (PSC_SEL_CLK_INTCLK | PSC_SEL_PS_I2SMODE);
	au_sync();

	/* Enable PSC
	*/
	ip->psc_ctrl = PSC_CTRL_ENABLE;
	au_sync();

	/* Wait for PSC ready.
	*/
	do {
		val = ip->psc_i2sstat;
		au_sync();
	} while ((val & PSC_I2SSTAT_SR) == 0);

	/* Configure I2S controller.
	 * Deep FIFO, 16-bit sample, DMA, make sure DMA matches fifo size.
	 * Actual I2S mode (first bit delayed by one clock).
	 * Master mode (We provide the clock from the PSC).
	 */
	val = PSC_I2SCFG_SET_LEN(16);
#ifdef TRY_441KHz
	/* This really should be 250, but it appears that all of the
	 * PLLs, dividers and so on in the chain shift it.  That's the
	 * problem with sourceing the clock instead of letting the very
	 * stable codec provide it.  But, the PSC doesn't appear to want
	 * to work in slave mode, so this is what we get.  It's  not
	 * studio quality timing, but it's good enough for listening
	 * to mp3s.
	 */
	val |= PSC_I2SCFG_SET_WS(252);
#else
	val |= PSC_I2SCFG_SET_WS(250);
#endif
	val |= PSC_I2SCFG_RT_FIFO8 | PSC_I2SCFG_TT_FIFO8 | \
					PSC_I2SCFG_BI | PSC_I2SCFG_XM;

	ip->psc_i2scfg = val;
	au_sync();
	val |= PSC_I2SCFG_DE_ENABLE;
	ip->psc_i2scfg = val;
	au_sync();

	/* Wait for Device ready.
	*/
	do {
		val = ip->psc_i2sstat;
		au_sync();
	} while ((val & PSC_I2SSTAT_DR) == 0);

	val = ip->psc_i2scfg;
	au_sync();

	s->codec->init_codec(s->codec);

	return 0;

 err_dev2:
	unregister_sound_dsp(s->dev_audio);
 err_dev1:
	au1xxx_dbdma_chan_free(s->dma_adc.dmanr);
 err_dma2:
	au1xxx_dbdma_chan_free(s->dma_dac.dmanr);
 err_dma1:
	release_region(CPHYSADDR(I2S_PSC_BASE), 0x30);

	return -1;
}

static void __devinit
au1550_remove(void)
{
	struct au1550_state *s = &au1550_state;

	if (!s)
		return;
#ifdef AU1550_DEBUG
	if (s->ps)
		remove_proc_entry(AU1000_MODULE_NAME, NULL);
#endif /* AU1000_DEBUG */
	synchronize_irq();
	au1xxx_dbdma_chan_free(s->dma_adc.dmanr);
	au1xxx_dbdma_chan_free(s->dma_dac.dmanr);
	release_region(CPHYSADDR(I2S_PSC_BASE), 0x30);
	unregister_sound_dsp(s->dev_audio);
	unregister_sound_mixer(s->dev_mixer);
}

static int __init
init_au1550(void)
{
	return au1550_probe();
}

static void __exit
cleanup_au1550(void)
{
	au1550_remove();
}

module_init(init_au1550);
module_exit(cleanup_au1550);

MODULE_AUTHOR("Advanced Micro Devices (AMD), dan@embeddededge.com");
MODULE_DESCRIPTION("Au1550 I2S Audio Driver");
