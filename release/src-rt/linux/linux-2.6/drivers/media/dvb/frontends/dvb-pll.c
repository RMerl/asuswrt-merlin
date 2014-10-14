/*
 * descriptions + helper functions for simple dvb plls.
 *
 * (c) 2004 Gerd Knorr <kraxel@bytesex.org> [SuSE Labs]
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/dvb/frontend.h>
#include <asm/types.h>

#include "dvb-pll.h"

struct dvb_pll_priv {
	/* pll number */
	int nr;

	/* i2c details */
	int pll_i2c_address;
	struct i2c_adapter *i2c;

	/* the PLL descriptor */
	struct dvb_pll_desc *pll_desc;

	/* cached frequency/bandwidth */
	u32 frequency;
	u32 bandwidth;
};

#define DVB_PLL_MAX 64

static unsigned int dvb_pll_devcount;

static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "enable verbose debug messages");

static unsigned int id[DVB_PLL_MAX] =
	{ [ 0 ... (DVB_PLL_MAX-1) ] = DVB_PLL_UNDEFINED };
module_param_array(id, int, NULL, 0644);
MODULE_PARM_DESC(id, "force pll id to use (DEBUG ONLY)");

/* ----------------------------------------------------------- */

struct dvb_pll_desc {
	char *name;
	u32  min;
	u32  max;
	u32  iffreq;
	void (*set)(struct dvb_frontend *fe, u8 *buf,
		    const struct dvb_frontend_parameters *params);
	u8   *initdata;
	u8   *initdata2;
	u8   *sleepdata;
	int  count;
	struct {
		u32 limit;
		u32 stepsize;
		u8  config;
		u8  cb;
	} entries[12];
};

/* ----------------------------------------------------------- */
/* descriptions                                                */

static struct dvb_pll_desc dvb_pll_thomson_dtt7579 = {
	.name  = "Thomson dtt7579",
	.min   = 177000000,
	.max   = 858000000,
	.iffreq= 36166667,
	.sleepdata = (u8[]){ 2, 0xb4, 0x03 },
	.count = 4,
	.entries = {
		{  443250000, 166667, 0xb4, 0x02 },
		{  542000000, 166667, 0xb4, 0x08 },
		{  771000000, 166667, 0xbc, 0x08 },
		{  999999999, 166667, 0xf4, 0x08 },
	},
};

static void thomson_dtt759x_bw(struct dvb_frontend *fe, u8 *buf,
			       const struct dvb_frontend_parameters *params)
{
	if (BANDWIDTH_7_MHZ == params->u.ofdm.bandwidth)
		buf[3] |= 0x10;
}

static struct dvb_pll_desc dvb_pll_thomson_dtt759x = {
	.name  = "Thomson dtt759x",
	.min   = 177000000,
	.max   = 896000000,
	.set   = thomson_dtt759x_bw,
	.iffreq= 36166667,
	.sleepdata = (u8[]){ 2, 0x84, 0x03 },
	.count = 5,
	.entries = {
		{  264000000, 166667, 0xb4, 0x02 },
		{  470000000, 166667, 0xbc, 0x02 },
		{  735000000, 166667, 0xbc, 0x08 },
		{  835000000, 166667, 0xf4, 0x08 },
		{  999999999, 166667, 0xfc, 0x08 },
	},
};

static struct dvb_pll_desc dvb_pll_lg_z201 = {
	.name  = "LG z201",
	.min   = 174000000,
	.max   = 862000000,
	.iffreq= 36166667,
	.sleepdata = (u8[]){ 2, 0xbc, 0x03 },
	.count = 5,
	.entries = {
		{  157500000, 166667, 0xbc, 0x01 },
		{  443250000, 166667, 0xbc, 0x02 },
		{  542000000, 166667, 0xbc, 0x04 },
		{  830000000, 166667, 0xf4, 0x04 },
		{  999999999, 166667, 0xfc, 0x04 },
	},
};

static struct dvb_pll_desc dvb_pll_unknown_1 = {
	.name  = "unknown 1", /* used by dntv live dvb-t */
	.min   = 174000000,
	.max   = 862000000,
	.iffreq= 36166667,
	.count = 9,
	.entries = {
		{  150000000, 166667, 0xb4, 0x01 },
		{  173000000, 166667, 0xbc, 0x01 },
		{  250000000, 166667, 0xb4, 0x02 },
		{  400000000, 166667, 0xbc, 0x02 },
		{  420000000, 166667, 0xf4, 0x02 },
		{  470000000, 166667, 0xfc, 0x02 },
		{  600000000, 166667, 0xbc, 0x08 },
		{  730000000, 166667, 0xf4, 0x08 },
		{  999999999, 166667, 0xfc, 0x08 },
	},
};

/* Infineon TUA6010XS
 * used in Thomson Cable Tuner
 */
static struct dvb_pll_desc dvb_pll_tua6010xs = {
	.name  = "Infineon TUA6010XS",
	.min   =  44250000,
	.max   = 858000000,
	.iffreq= 36125000,
	.count = 3,
	.entries = {
		{  115750000, 62500, 0x8e, 0x03 },
		{  403250000, 62500, 0x8e, 0x06 },
		{  999999999, 62500, 0x8e, 0x85 },
	},
};

/* Panasonic env57h1xd5 (some Philips PLL ?) */
static struct dvb_pll_desc dvb_pll_env57h1xd5 = {
	.name  = "Panasonic ENV57H1XD5",
	.min   =  44250000,
	.max   = 858000000,
	.iffreq= 36125000,
	.count = 4,
	.entries = {
		{  153000000, 166667, 0xc2, 0x41 },
		{  470000000, 166667, 0xc2, 0x42 },
		{  526000000, 166667, 0xc2, 0x84 },
		{  999999999, 166667, 0xc2, 0xa4 },
	},
};

/* Philips TDA6650/TDA6651
 * used in Panasonic ENV77H11D5
 */
static void tda665x_bw(struct dvb_frontend *fe, u8 *buf,
		       const struct dvb_frontend_parameters *params)
{
	if (params->u.ofdm.bandwidth == BANDWIDTH_8_MHZ)
		buf[3] |= 0x08;
}

static struct dvb_pll_desc dvb_pll_tda665x = {
	.name  = "Philips TDA6650/TDA6651",
	.min   =  44250000,
	.max   = 858000000,
	.set   = tda665x_bw,
	.iffreq= 36166667,
	.initdata = (u8[]){ 4, 0x0b, 0xf5, 0x85, 0xab },
	.count = 12,
	.entries = {
		{   93834000, 166667, 0xca, 0x61 /* 011 0 0 0  01 */ },
		{  123834000, 166667, 0xca, 0xa1 /* 101 0 0 0  01 */ },
		{  161000000, 166667, 0xca, 0xa1 /* 101 0 0 0  01 */ },
		{  163834000, 166667, 0xca, 0xc2 /* 110 0 0 0  10 */ },
		{  253834000, 166667, 0xca, 0x62 /* 011 0 0 0  10 */ },
		{  383834000, 166667, 0xca, 0xa2 /* 101 0 0 0  10 */ },
		{  443834000, 166667, 0xca, 0xc2 /* 110 0 0 0  10 */ },
		{  444000000, 166667, 0xca, 0xc4 /* 110 0 0 1  00 */ },
		{  583834000, 166667, 0xca, 0x64 /* 011 0 0 1  00 */ },
		{  793834000, 166667, 0xca, 0xa4 /* 101 0 0 1  00 */ },
		{  444834000, 166667, 0xca, 0xc4 /* 110 0 0 1  00 */ },
		{  861000000, 166667, 0xca, 0xe4 /* 111 0 0 1  00 */ },
	}
};

/* Infineon TUA6034
 * used in LG TDTP E102P
 */
static void tua6034_bw(struct dvb_frontend *fe, u8 *buf,
		       const struct dvb_frontend_parameters *params)
{
	if (BANDWIDTH_7_MHZ != params->u.ofdm.bandwidth)
		buf[3] |= 0x08;
}

static struct dvb_pll_desc dvb_pll_tua6034 = {
	.name  = "Infineon TUA6034",
	.min   =  44250000,
	.max   = 858000000,
	.iffreq= 36166667,
	.count = 3,
	.set   = tua6034_bw,
	.entries = {
		{  174500000, 62500, 0xce, 0x01 },
		{  230000000, 62500, 0xce, 0x02 },
		{  999999999, 62500, 0xce, 0x04 },
	},
};

/* ALPS TDED4
 * used in Nebula-Cards and USB boxes
 */
static void tded4_bw(struct dvb_frontend *fe, u8 *buf,
		     const struct dvb_frontend_parameters *params)
{
	if (params->u.ofdm.bandwidth == BANDWIDTH_8_MHZ)
		buf[3] |= 0x04;
}

static struct dvb_pll_desc dvb_pll_tded4 = {
	.name = "ALPS TDED4",
	.min = 47000000,
	.max = 863000000,
	.iffreq= 36166667,
	.set   = tded4_bw,
	.count = 4,
	.entries = {
		{ 153000000, 166667, 0x85, 0x01 },
		{ 470000000, 166667, 0x85, 0x02 },
		{ 823000000, 166667, 0x85, 0x08 },
		{ 999999999, 166667, 0x85, 0x88 },
	}
};

/* ALPS TDHU2
 * used in AverTVHD MCE A180
 */
static struct dvb_pll_desc dvb_pll_tdhu2 = {
	.name = "ALPS TDHU2",
	.min = 54000000,
	.max = 864000000,
	.iffreq= 44000000,
	.count = 4,
	.entries = {
		{ 162000000, 62500, 0x85, 0x01 },
		{ 426000000, 62500, 0x85, 0x02 },
		{ 782000000, 62500, 0x85, 0x08 },
		{ 999999999, 62500, 0x85, 0x88 },
	}
};

/* Samsung TBMV30111IN / TBMV30712IN1
 * used in Air2PC ATSC - 2nd generation (nxt2002)
 */
static struct dvb_pll_desc dvb_pll_samsung_tbmv = {
	.name = "Samsung TBMV30111IN / TBMV30712IN1",
	.min = 54000000,
	.max = 860000000,
	.iffreq= 44000000,
	.count = 6,
	.entries = {
		{ 172000000, 166667, 0xb4, 0x01 },
		{ 214000000, 166667, 0xb4, 0x02 },
		{ 467000000, 166667, 0xbc, 0x02 },
		{ 721000000, 166667, 0xbc, 0x08 },
		{ 841000000, 166667, 0xf4, 0x08 },
		{ 999999999, 166667, 0xfc, 0x02 },
	}
};

/*
 * Philips SD1878 Tuner.
 */
static struct dvb_pll_desc dvb_pll_philips_sd1878_tda8261 = {
	.name  = "Philips SD1878",
	.min   =  950000,
	.max   = 2150000,
	.iffreq= 249, /* zero-IF, offset 249 is to round up */
	.count = 4,
	.entries = {
		{ 1250000, 500, 0xc4, 0x00},
		{ 1450000, 500, 0xc4, 0x40},
		{ 2050000, 500, 0xc4, 0x80},
		{ 2150000, 500, 0xc4, 0xc0},
	},
};

static void opera1_bw(struct dvb_frontend *fe, u8 *buf,
		      const struct dvb_frontend_parameters *params)
{
	struct dvb_pll_priv *priv = fe->tuner_priv;
	u32 b_w  = (params->u.qpsk.symbol_rate * 27) / 32000;
	struct i2c_msg msg = {
		.addr = priv->pll_i2c_address,
		.flags = 0,
		.buf = buf,
		.len = 4
	};
	int result;
	u8 lpf;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);

	result = i2c_transfer(priv->i2c, &msg, 1);
	if (result != 1)
		printk(KERN_ERR "%s: i2c_transfer failed:%d",
			__func__, result);

	if (b_w <= 10000)
		lpf = 0xc;
	else if (b_w <= 12000)
		lpf = 0x2;
	else if (b_w <= 14000)
		lpf = 0xa;
	else if (b_w <= 16000)
		lpf = 0x6;
	else if (b_w <= 18000)
		lpf = 0xe;
	else if (b_w <= 20000)
		lpf = 0x1;
	else if (b_w <= 22000)
		lpf = 0x9;
	else if (b_w <= 24000)
		lpf = 0x5;
	else if (b_w <= 26000)
		lpf = 0xd;
	else if (b_w <= 28000)
		lpf = 0x3;
		else
		lpf = 0xb;
	buf[2] ^= 0x1c; /* Flip bits 3-5 */
	/* Set lpf */
	buf[2] |= ((lpf >> 2) & 0x3) << 3;
	buf[3] |= (lpf & 0x3) << 2;

	return;
}

static struct dvb_pll_desc dvb_pll_opera1 = {
	.name  = "Opera Tuner",
	.min   =  900000,
	.max   = 2250000,
	.initdata = (u8[]){ 4, 0x08, 0xe5, 0xe1, 0x00 },
	.initdata2 = (u8[]){ 4, 0x08, 0xe5, 0xe5, 0x00 },
	.iffreq= 0,
	.set   = opera1_bw,
	.count = 8,
	.entries = {
		{ 1064000, 500, 0xf9, 0xc2 },
		{ 1169000, 500, 0xf9, 0xe2 },
		{ 1299000, 500, 0xf9, 0x20 },
		{ 1444000, 500, 0xf9, 0x40 },
		{ 1606000, 500, 0xf9, 0x60 },
		{ 1777000, 500, 0xf9, 0x80 },
		{ 1941000, 500, 0xf9, 0xa0 },
		{ 2250000, 500, 0xf9, 0xc0 },
	}
};

static void samsung_dtos403ih102a_set(struct dvb_frontend *fe, u8 *buf,
		       const struct dvb_frontend_parameters *params)
{
	struct dvb_pll_priv *priv = fe->tuner_priv;
	struct i2c_msg msg = {
		.addr = priv->pll_i2c_address,
		.flags = 0,
		.buf = buf,
		.len = 4
	};
	int result;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);

	result = i2c_transfer(priv->i2c, &msg, 1);
	if (result != 1)
		printk(KERN_ERR "%s: i2c_transfer failed:%d",
			__func__, result);

	buf[2] = 0x9e;
	buf[3] = 0x90;

	return;
}

/* unknown pll used in Samsung DTOS403IH102A DVB-C tuner */
static struct dvb_pll_desc dvb_pll_samsung_dtos403ih102a = {
	.name   = "Samsung DTOS403IH102A",
	.min    =  44250000,
	.max    = 858000000,
	.iffreq =  36125000,
	.count  = 8,
	.set    = samsung_dtos403ih102a_set,
	.entries = {
		{ 135000000, 62500, 0xbe, 0x01 },
		{ 177000000, 62500, 0xf6, 0x01 },
		{ 370000000, 62500, 0xbe, 0x02 },
		{ 450000000, 62500, 0xf6, 0x02 },
		{ 466000000, 62500, 0xfe, 0x02 },
		{ 538000000, 62500, 0xbe, 0x08 },
		{ 826000000, 62500, 0xf6, 0x08 },
		{ 999999999, 62500, 0xfe, 0x08 },
	}
};

/* Samsung TDTC9251DH0 DVB-T NIM, as used on AirStar 2 */
static struct dvb_pll_desc dvb_pll_samsung_tdtc9251dh0 = {
	.name	= "Samsung TDTC9251DH0",
	.min	=  48000000,
	.max	= 863000000,
	.iffreq	=  36166667,
	.count	= 3,
	.entries = {
		{ 157500000, 166667, 0xcc, 0x09 },
		{ 443000000, 166667, 0xcc, 0x0a },
		{ 863000000, 166667, 0xcc, 0x08 },
	}
};

/* Samsung TBDU18132 DVB-S NIM with TSA5059 PLL, used in SkyStar2 DVB-S 2.3 */
static struct dvb_pll_desc dvb_pll_samsung_tbdu18132 = {
	.name = "Samsung TBDU18132",
	.min	=  950000,
	.max	= 2150000, /* guesses */
	.iffreq = 0,
	.count = 2,
	.entries = {
		{ 1550000, 125, 0x84, 0x82 },
		{ 4095937, 125, 0x84, 0x80 },
	}
	/* TSA5059 PLL has a 17 bit divisor rather than the 15 bits supported
	 * by this driver.  The two extra bits are 0x60 in the third byte.  15
	 * bits is enough for over 4 GHz, which is enough to cover the range
	 * of this tuner.  We could use the additional divisor bits by adding
	 * more entries, e.g.
	 { 0x0ffff * 125 + 125/2, 125, 0x84 | 0x20, },
	 { 0x17fff * 125 + 125/2, 125, 0x84 | 0x40, },
	 { 0x1ffff * 125 + 125/2, 125, 0x84 | 0x60, }, */
};

/* Samsung TBMU24112 DVB-S NIM with SL1935 zero-IF tuner */
static struct dvb_pll_desc dvb_pll_samsung_tbmu24112 = {
	.name = "Samsung TBMU24112",
	.min	=  950000,
	.max	= 2150000, /* guesses */
	.iffreq = 0,
	.count = 2,
	.entries = {
		{ 1500000, 125, 0x84, 0x18 },
		{ 9999999, 125, 0x84, 0x08 },
	}
};

/* Alps TDEE4 DVB-C NIM, used on Cablestar 2 */
/* byte 4 : 1  *   *   AGD R3  R2  R1  R0
 * byte 5 : C1 *   RE  RTS BS4 BS3 BS2 BS1
 * AGD = 1, R3 R2 R1 R0 = 0 1 0 1 => byte 4 = 1**10101 = 0x95
 * Range(MHz)  C1 *  RE RTS BS4 BS3 BS2 BS1  Byte 5
 *  47 - 153   0  *  0   0   0   0   0   1   0x01
 * 153 - 430   0  *  0   0   0   0   1   0   0x02
 * 430 - 822   0  *  0   0   1   0   0   0   0x08
 * 822 - 862   1  *  0   0   1   0   0   0   0x88 */
static struct dvb_pll_desc dvb_pll_alps_tdee4 = {
	.name = "ALPS TDEE4",
	.min	=  47000000,
	.max	= 862000000,
	.iffreq	=  36125000,
	.count = 4,
	.entries = {
		{ 153000000, 62500, 0x95, 0x01 },
		{ 430000000, 62500, 0x95, 0x02 },
		{ 822000000, 62500, 0x95, 0x08 },
		{ 999999999, 62500, 0x95, 0x88 },
	}
};

/* ----------------------------------------------------------- */

static struct dvb_pll_desc *pll_list[] = {
	[DVB_PLL_UNDEFINED]              = NULL,
	[DVB_PLL_THOMSON_DTT7579]        = &dvb_pll_thomson_dtt7579,
	[DVB_PLL_THOMSON_DTT759X]        = &dvb_pll_thomson_dtt759x,
	[DVB_PLL_LG_Z201]                = &dvb_pll_lg_z201,
	[DVB_PLL_UNKNOWN_1]              = &dvb_pll_unknown_1,
	[DVB_PLL_TUA6010XS]              = &dvb_pll_tua6010xs,
	[DVB_PLL_ENV57H1XD5]             = &dvb_pll_env57h1xd5,
	[DVB_PLL_TUA6034]                = &dvb_pll_tua6034,
	[DVB_PLL_TDA665X]                = &dvb_pll_tda665x,
	[DVB_PLL_TDED4]                  = &dvb_pll_tded4,
	[DVB_PLL_TDEE4]                  = &dvb_pll_alps_tdee4,
	[DVB_PLL_TDHU2]                  = &dvb_pll_tdhu2,
	[DVB_PLL_SAMSUNG_TBMV]           = &dvb_pll_samsung_tbmv,
	[DVB_PLL_PHILIPS_SD1878_TDA8261] = &dvb_pll_philips_sd1878_tda8261,
	[DVB_PLL_OPERA1]                 = &dvb_pll_opera1,
	[DVB_PLL_SAMSUNG_DTOS403IH102A]  = &dvb_pll_samsung_dtos403ih102a,
	[DVB_PLL_SAMSUNG_TDTC9251DH0]    = &dvb_pll_samsung_tdtc9251dh0,
	[DVB_PLL_SAMSUNG_TBDU18132]	 = &dvb_pll_samsung_tbdu18132,
	[DVB_PLL_SAMSUNG_TBMU24112]      = &dvb_pll_samsung_tbmu24112,
};

/* ----------------------------------------------------------- */
/* code                                                        */

static int dvb_pll_configure(struct dvb_frontend *fe, u8 *buf,
			     const struct dvb_frontend_parameters *params)
{
	struct dvb_pll_priv *priv = fe->tuner_priv;
	struct dvb_pll_desc *desc = priv->pll_desc;
	u32 div;
	int i;

	if (params->frequency != 0 && (params->frequency < desc->min ||
				       params->frequency > desc->max))
		return -EINVAL;

	for (i = 0; i < desc->count; i++) {
		if (params->frequency > desc->entries[i].limit)
			continue;
		break;
	}

	if (debug)
		printk("pll: %s: freq=%d | i=%d/%d\n", desc->name,
		       params->frequency, i, desc->count);
	if (i == desc->count)
		return -EINVAL;

	div = (params->frequency + desc->iffreq +
	       desc->entries[i].stepsize/2) / desc->entries[i].stepsize;
	buf[0] = div >> 8;
	buf[1] = div & 0xff;
	buf[2] = desc->entries[i].config;
	buf[3] = desc->entries[i].cb;

	if (desc->set)
		desc->set(fe, buf, params);

	if (debug)
		printk("pll: %s: div=%d | buf=0x%02x,0x%02x,0x%02x,0x%02x\n",
		       desc->name, div, buf[0], buf[1], buf[2], buf[3]);

	// calculate the frequency we set it to
	return (div * desc->entries[i].stepsize) - desc->iffreq;
}

static int dvb_pll_release(struct dvb_frontend *fe)
{
	kfree(fe->tuner_priv);
	fe->tuner_priv = NULL;
	return 0;
}

static int dvb_pll_sleep(struct dvb_frontend *fe)
{
	struct dvb_pll_priv *priv = fe->tuner_priv;

	if (priv->i2c == NULL)
		return -EINVAL;

	if (priv->pll_desc->sleepdata) {
		struct i2c_msg msg = { .flags = 0,
			.addr = priv->pll_i2c_address,
			.buf = priv->pll_desc->sleepdata + 1,
			.len = priv->pll_desc->sleepdata[0] };

		int result;

		if (fe->ops.i2c_gate_ctrl)
			fe->ops.i2c_gate_ctrl(fe, 1);
		if ((result = i2c_transfer(priv->i2c, &msg, 1)) != 1) {
			return result;
		}
		return 0;
	}
	/* Shouldn't be called when initdata is NULL, maybe BUG()? */
	return -EINVAL;
}

static int dvb_pll_set_params(struct dvb_frontend *fe,
			      struct dvb_frontend_parameters *params)
{
	struct dvb_pll_priv *priv = fe->tuner_priv;
	u8 buf[4];
	struct i2c_msg msg =
		{ .addr = priv->pll_i2c_address, .flags = 0,
		  .buf = buf, .len = sizeof(buf) };
	int result;
	u32 frequency = 0;

	if (priv->i2c == NULL)
		return -EINVAL;

	if ((result = dvb_pll_configure(fe, buf, params)) < 0)
		return result;
	else
		frequency = result;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
	if ((result = i2c_transfer(priv->i2c, &msg, 1)) != 1) {
		return result;
	}

	priv->frequency = frequency;
	priv->bandwidth = (fe->ops.info.type == FE_OFDM) ? params->u.ofdm.bandwidth : 0;

	return 0;
}

static int dvb_pll_calc_regs(struct dvb_frontend *fe,
			     struct dvb_frontend_parameters *params,
			     u8 *buf, int buf_len)
{
	struct dvb_pll_priv *priv = fe->tuner_priv;
	int result;
	u32 frequency = 0;

	if (buf_len < 5)
		return -EINVAL;

	if ((result = dvb_pll_configure(fe, buf+1, params)) < 0)
		return result;
	else
		frequency = result;

	buf[0] = priv->pll_i2c_address;

	priv->frequency = frequency;
	priv->bandwidth = (fe->ops.info.type == FE_OFDM) ? params->u.ofdm.bandwidth : 0;

	return 5;
}

static int dvb_pll_get_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	struct dvb_pll_priv *priv = fe->tuner_priv;
	*frequency = priv->frequency;
	return 0;
}

static int dvb_pll_get_bandwidth(struct dvb_frontend *fe, u32 *bandwidth)
{
	struct dvb_pll_priv *priv = fe->tuner_priv;
	*bandwidth = priv->bandwidth;
	return 0;
}

static int dvb_pll_init(struct dvb_frontend *fe)
{
	struct dvb_pll_priv *priv = fe->tuner_priv;

	if (priv->i2c == NULL)
		return -EINVAL;

	if (priv->pll_desc->initdata) {
		struct i2c_msg msg = { .flags = 0,
			.addr = priv->pll_i2c_address,
			.buf = priv->pll_desc->initdata + 1,
			.len = priv->pll_desc->initdata[0] };

		int result;
		if (fe->ops.i2c_gate_ctrl)
			fe->ops.i2c_gate_ctrl(fe, 1);
		result = i2c_transfer(priv->i2c, &msg, 1);
		if (result != 1)
			return result;
		if (priv->pll_desc->initdata2) {
			msg.buf = priv->pll_desc->initdata2 + 1;
			msg.len = priv->pll_desc->initdata2[0];
			if (fe->ops.i2c_gate_ctrl)
				fe->ops.i2c_gate_ctrl(fe, 1);
			result = i2c_transfer(priv->i2c, &msg, 1);
			if (result != 1)
				return result;
		}
		return 0;
	}
	/* Shouldn't be called when initdata is NULL, maybe BUG()? */
	return -EINVAL;
}

static struct dvb_tuner_ops dvb_pll_tuner_ops = {
	.release = dvb_pll_release,
	.sleep = dvb_pll_sleep,
	.init = dvb_pll_init,
	.set_params = dvb_pll_set_params,
	.calc_regs = dvb_pll_calc_regs,
	.get_frequency = dvb_pll_get_frequency,
	.get_bandwidth = dvb_pll_get_bandwidth,
};

struct dvb_frontend *dvb_pll_attach(struct dvb_frontend *fe, int pll_addr,
				    struct i2c_adapter *i2c,
				    unsigned int pll_desc_id)
{
	u8 b1 [] = { 0 };
	struct i2c_msg msg = { .addr = pll_addr, .flags = I2C_M_RD,
			       .buf = b1, .len = 1 };
	struct dvb_pll_priv *priv = NULL;
	int ret;
	struct dvb_pll_desc *desc;

	if ((id[dvb_pll_devcount] > DVB_PLL_UNDEFINED) &&
	    (id[dvb_pll_devcount] < ARRAY_SIZE(pll_list)))
		pll_desc_id = id[dvb_pll_devcount];

	BUG_ON(pll_desc_id < 1 || pll_desc_id >= ARRAY_SIZE(pll_list));

	desc = pll_list[pll_desc_id];

	if (i2c != NULL) {
		if (fe->ops.i2c_gate_ctrl)
			fe->ops.i2c_gate_ctrl(fe, 1);

		ret = i2c_transfer (i2c, &msg, 1);
		if (ret != 1)
			return NULL;
		if (fe->ops.i2c_gate_ctrl)
			     fe->ops.i2c_gate_ctrl(fe, 0);
	}

	priv = kzalloc(sizeof(struct dvb_pll_priv), GFP_KERNEL);
	if (priv == NULL)
		return NULL;

	priv->pll_i2c_address = pll_addr;
	priv->i2c = i2c;
	priv->pll_desc = desc;
	priv->nr = dvb_pll_devcount++;

	memcpy(&fe->ops.tuner_ops, &dvb_pll_tuner_ops,
	       sizeof(struct dvb_tuner_ops));

	strncpy(fe->ops.tuner_ops.info.name, desc->name,
		sizeof(fe->ops.tuner_ops.info.name));
	fe->ops.tuner_ops.info.frequency_min = desc->min;
	fe->ops.tuner_ops.info.frequency_max = desc->max;
	if (!desc->initdata)
		fe->ops.tuner_ops.init = NULL;
	if (!desc->sleepdata)
		fe->ops.tuner_ops.sleep = NULL;

	fe->tuner_priv = priv;

	if ((debug) || (id[priv->nr] == pll_desc_id)) {
		printk("dvb-pll[%d]", priv->nr);
		if (i2c != NULL)
			printk(" %d-%04x", i2c_adapter_id(i2c), pll_addr);
		printk(": id# %d (%s) attached, %s\n", pll_desc_id, desc->name,
		       id[priv->nr] == pll_desc_id ?
				"insmod option" : "autodetected");
	}

	return fe;
}
EXPORT_SYMBOL(dvb_pll_attach);

MODULE_DESCRIPTION("dvb pll library");
MODULE_AUTHOR("Gerd Knorr");
MODULE_LICENSE("GPL");
