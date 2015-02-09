/*
 *  Driver for Microtune MT2060 "Single chip dual conversion broadband tuner"
 *
 *  Copyright (c) 2006 Olivier DANET <odanet@caramail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.=
 */

/* In that file, frequencies are expressed in kiloHertz to avoid 32 bits overflows */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/dvb/frontend.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#include "dvb_frontend.h"

#include "mt2060.h"
#include "mt2060_priv.h"

static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Turn on/off debugging (default:off).");

#define dprintk(args...) do { if (debug) {printk(KERN_DEBUG "MT2060: " args); printk("\n"); }} while (0)

// Reads a single register
static int mt2060_readreg(struct mt2060_priv *priv, u8 reg, u8 *val)
{
	struct i2c_msg msg[2] = {
		{ .addr = priv->cfg->i2c_address, .flags = 0,        .buf = &reg, .len = 1 },
		{ .addr = priv->cfg->i2c_address, .flags = I2C_M_RD, .buf = val,  .len = 1 },
	};

	if (i2c_transfer(priv->i2c, msg, 2) != 2) {
		printk(KERN_WARNING "mt2060 I2C read failed\n");
		return -EREMOTEIO;
	}
	return 0;
}

// Writes a single register
static int mt2060_writereg(struct mt2060_priv *priv, u8 reg, u8 val)
{
	u8 buf[2] = { reg, val };
	struct i2c_msg msg = {
		.addr = priv->cfg->i2c_address, .flags = 0, .buf = buf, .len = 2
	};

	if (i2c_transfer(priv->i2c, &msg, 1) != 1) {
		printk(KERN_WARNING "mt2060 I2C write failed\n");
		return -EREMOTEIO;
	}
	return 0;
}

// Writes a set of consecutive registers
static int mt2060_writeregs(struct mt2060_priv *priv,u8 *buf, u8 len)
{
	struct i2c_msg msg = {
		.addr = priv->cfg->i2c_address, .flags = 0, .buf = buf, .len = len
	};
	if (i2c_transfer(priv->i2c, &msg, 1) != 1) {
		printk(KERN_WARNING "mt2060 I2C write failed (len=%i)\n",(int)len);
		return -EREMOTEIO;
	}
	return 0;
}

// Initialisation sequences
// LNABAND=3, NUM1=0x3C, DIV1=0x74, NUM2=0x1080, DIV2=0x49
static u8 mt2060_config1[] = {
	REG_LO1C1,
	0x3F,	0x74,	0x00,	0x08,	0x93
};

// FMCG=2, GP2=0, GP1=0
static u8 mt2060_config2[] = {
	REG_MISC_CTRL,
	0x20,	0x1E,	0x30,	0xff,	0x80,	0xff,	0x00,	0x2c,	0x42
};

//  VGAG=3, V1CSE=1

#ifdef  MT2060_SPURCHECK
/* The function below calculates the frequency offset between the output frequency if2
 and the closer cross modulation subcarrier between lo1 and lo2 up to the tenth harmonic */
static int mt2060_spurcalc(u32 lo1,u32 lo2,u32 if2)
{
	int I,J;
	int dia,diamin,diff;
	diamin=1000000;
	for (I = 1; I < 10; I++) {
		J = ((2*I*lo1)/lo2+1)/2;
		diff = I*(int)lo1-J*(int)lo2;
		if (diff < 0) diff=-diff;
		dia = (diff-(int)if2);
		if (dia < 0) dia=-dia;
		if (diamin > dia) diamin=dia;
	}
	return diamin;
}

#define BANDWIDTH 4000 // kHz

/* Calculates the frequency offset to add to avoid spurs. Returns 0 if no offset is needed */
static int mt2060_spurcheck(u32 lo1,u32 lo2,u32 if2)
{
	u32 Spur,Sp1,Sp2;
	int I,J;
	I=0;
	J=1000;

	Spur=mt2060_spurcalc(lo1,lo2,if2);
	if (Spur < BANDWIDTH) {
		/* Potential spurs detected */
		dprintk("Spurs before : f_lo1: %d  f_lo2: %d  (kHz)",
			(int)lo1,(int)lo2);
		I=1000;
		Sp1 = mt2060_spurcalc(lo1+I,lo2+I,if2);
		Sp2 = mt2060_spurcalc(lo1-I,lo2-I,if2);

		if (Sp1 < Sp2) {
			J=-J; I=-I; Spur=Sp2;
		} else
			Spur=Sp1;

		while (Spur < BANDWIDTH) {
			I += J;
			Spur = mt2060_spurcalc(lo1+I,lo2+I,if2);
		}
		dprintk("Spurs after  : f_lo1: %d  f_lo2: %d  (kHz)",
			(int)(lo1+I),(int)(lo2+I));
	}
	return I;
}
#endif

#define IF2  36150       // IF2 frequency = 36.150 MHz
#define FREF 16000       // Quartz oscillator 16 MHz

static int mt2060_set_params(struct dvb_frontend *fe, struct dvb_frontend_parameters *params)
{
	struct mt2060_priv *priv;
	int ret=0;
	int i=0;
	u32 freq;
	u8  lnaband;
	u32 f_lo1,f_lo2;
	u32 div1,num1,div2,num2;
	u8  b[8];
	u32 if1;

	priv = fe->tuner_priv;

	if1 = priv->if1_freq;
	b[0] = REG_LO1B1;
	b[1] = 0xFF;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1); /* open i2c_gate */

	mt2060_writeregs(priv,b,2);

	freq = params->frequency / 1000; // Hz -> kHz
	priv->bandwidth = (fe->ops.info.type == FE_OFDM) ? params->u.ofdm.bandwidth : 0;

	f_lo1 = freq + if1 * 1000;
	f_lo1 = (f_lo1 / 250) * 250;
	f_lo2 = f_lo1 - freq - IF2;
	// From the Comtech datasheet, the step used is 50kHz. The tuner chip could be more precise
	f_lo2 = ((f_lo2 + 25) / 50) * 50;
	priv->frequency =  (f_lo1 - f_lo2 - IF2) * 1000,

#ifdef MT2060_SPURCHECK
	// LO-related spurs detection and correction
	num1   = mt2060_spurcheck(f_lo1,f_lo2,IF2);
	f_lo1 += num1;
	f_lo2 += num1;
#endif
	//Frequency LO1 = 16MHz * (DIV1 + NUM1/64 )
	num1 = f_lo1 / (FREF / 64);
	div1 = num1 / 64;
	num1 &= 0x3f;

	// Frequency LO2 = 16MHz * (DIV2 + NUM2/8192 )
	num2 = f_lo2 * 64 / (FREF / 128);
	div2 = num2 / 8192;
	num2 &= 0x1fff;

	if (freq <=  95000) lnaband = 0xB0; else
	if (freq <= 180000) lnaband = 0xA0; else
	if (freq <= 260000) lnaband = 0x90; else
	if (freq <= 335000) lnaband = 0x80; else
	if (freq <= 425000) lnaband = 0x70; else
	if (freq <= 480000) lnaband = 0x60; else
	if (freq <= 570000) lnaband = 0x50; else
	if (freq <= 645000) lnaband = 0x40; else
	if (freq <= 730000) lnaband = 0x30; else
	if (freq <= 810000) lnaband = 0x20; else lnaband = 0x10;

	b[0] = REG_LO1C1;
	b[1] = lnaband | ((num1 >>2) & 0x0F);
	b[2] = div1;
	b[3] = (num2 & 0x0F)  | ((num1 & 3) << 4);
	b[4] = num2 >> 4;
	b[5] = ((num2 >>12) & 1) | (div2 << 1);

	dprintk("IF1: %dMHz",(int)if1);
	dprintk("PLL freq=%dkHz  f_lo1=%dkHz  f_lo2=%dkHz",(int)freq,(int)f_lo1,(int)f_lo2);
	dprintk("PLL div1=%d  num1=%d  div2=%d  num2=%d",(int)div1,(int)num1,(int)div2,(int)num2);
	dprintk("PLL [1..5]: %2x %2x %2x %2x %2x",(int)b[1],(int)b[2],(int)b[3],(int)b[4],(int)b[5]);

	mt2060_writeregs(priv,b,6);

	//Waits for pll lock or timeout
	i = 0;
	do {
		mt2060_readreg(priv,REG_LO_STATUS,b);
		if ((b[0] & 0x88)==0x88)
			break;
		msleep(4);
		i++;
	} while (i<10);

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0); /* close i2c_gate */

	return ret;
}

static void mt2060_calibrate(struct mt2060_priv *priv)
{
	u8 b = 0;
	int i = 0;

	if (mt2060_writeregs(priv,mt2060_config1,sizeof(mt2060_config1)))
		return;
	if (mt2060_writeregs(priv,mt2060_config2,sizeof(mt2060_config2)))
		return;

	/* initialize the clock output */
	mt2060_writereg(priv, REG_VGAG, (priv->cfg->clock_out << 6) | 0x30);

	do {
		b |= (1 << 6); // FM1SS;
		mt2060_writereg(priv, REG_LO2C1,b);
		msleep(20);

		if (i == 0) {
			b |= (1 << 7); // FM1CA;
			mt2060_writereg(priv, REG_LO2C1,b);
			b &= ~(1 << 7); // FM1CA;
			msleep(20);
		}

		b &= ~(1 << 6); // FM1SS
		mt2060_writereg(priv, REG_LO2C1,b);

		msleep(20);
		i++;
	} while (i < 9);

	i = 0;
	while (i++ < 10 && mt2060_readreg(priv, REG_MISC_STAT, &b) == 0 && (b & (1 << 6)) == 0)
		msleep(20);

	if (i <= 10) {
		mt2060_readreg(priv, REG_FM_FREQ, &priv->fmfreq); // now find out, what is fmreq used for :)
		dprintk("calibration was successful: %d", (int)priv->fmfreq);
	} else
		dprintk("FMCAL timed out");
}

static int mt2060_get_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	struct mt2060_priv *priv = fe->tuner_priv;
	*frequency = priv->frequency;
	return 0;
}

static int mt2060_get_bandwidth(struct dvb_frontend *fe, u32 *bandwidth)
{
	struct mt2060_priv *priv = fe->tuner_priv;
	*bandwidth = priv->bandwidth;
	return 0;
}

static int mt2060_init(struct dvb_frontend *fe)
{
	struct mt2060_priv *priv = fe->tuner_priv;
	int ret;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1); /* open i2c_gate */

	ret = mt2060_writereg(priv, REG_VGAG,
			      (priv->cfg->clock_out << 6) | 0x33);

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0); /* close i2c_gate */

	return ret;
}

static int mt2060_sleep(struct dvb_frontend *fe)
{
	struct mt2060_priv *priv = fe->tuner_priv;
	int ret;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1); /* open i2c_gate */

	ret = mt2060_writereg(priv, REG_VGAG,
			      (priv->cfg->clock_out << 6) | 0x30);

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0); /* close i2c_gate */

	return ret;
}

static int mt2060_release(struct dvb_frontend *fe)
{
	kfree(fe->tuner_priv);
	fe->tuner_priv = NULL;
	return 0;
}

static const struct dvb_tuner_ops mt2060_tuner_ops = {
	.info = {
		.name           = "Microtune MT2060",
		.frequency_min  =  48000000,
		.frequency_max  = 860000000,
		.frequency_step =     50000,
	},

	.release       = mt2060_release,

	.init          = mt2060_init,
	.sleep         = mt2060_sleep,

	.set_params    = mt2060_set_params,
	.get_frequency = mt2060_get_frequency,
	.get_bandwidth = mt2060_get_bandwidth
};

/* This functions tries to identify a MT2060 tuner by reading the PART/REV register. This is hasty. */
struct dvb_frontend * mt2060_attach(struct dvb_frontend *fe, struct i2c_adapter *i2c, struct mt2060_config *cfg, u16 if1)
{
	struct mt2060_priv *priv = NULL;
	u8 id = 0;

	priv = kzalloc(sizeof(struct mt2060_priv), GFP_KERNEL);
	if (priv == NULL)
		return NULL;

	priv->cfg      = cfg;
	priv->i2c      = i2c;
	priv->if1_freq = if1;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1); /* open i2c_gate */

	if (mt2060_readreg(priv,REG_PART_REV,&id) != 0) {
		kfree(priv);
		return NULL;
	}

	if (id != PART_REV) {
		kfree(priv);
		return NULL;
	}
	printk(KERN_INFO "MT2060: successfully identified (IF1 = %d)\n", if1);
	memcpy(&fe->ops.tuner_ops, &mt2060_tuner_ops, sizeof(struct dvb_tuner_ops));

	fe->tuner_priv = priv;

	mt2060_calibrate(priv);

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0); /* close i2c_gate */

	return fe;
}
EXPORT_SYMBOL(mt2060_attach);

MODULE_AUTHOR("Olivier DANET");
MODULE_DESCRIPTION("Microtune MT2060 silicon tuner driver");
MODULE_LICENSE("GPL");
