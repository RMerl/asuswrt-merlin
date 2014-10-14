/*
	STV6110(A) Silicon tuner driver

	Copyright (C) Manu Abraham <abraham.manu@gmail.com>

	Copyright (C) ST Microelectronics

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "dvb_frontend.h"

#include "stv6110x_reg.h"
#include "stv6110x.h"
#include "stv6110x_priv.h"

static unsigned int verbose;
module_param(verbose, int, 0644);
MODULE_PARM_DESC(verbose, "Set Verbosity level");

static int stv6110x_read_reg(struct stv6110x_state *stv6110x, u8 reg, u8 *data)
{
	int ret;
	const struct stv6110x_config *config = stv6110x->config;
	u8 b0[] = { reg };
	u8 b1[] = { 0 };
	struct i2c_msg msg[] = {
		{ .addr = config->addr, .flags = 0, 	   .buf = b0, .len = 1 },
		{ .addr = config->addr, .flags = I2C_M_RD, .buf = b1, .len = 1 }
	};

	ret = i2c_transfer(stv6110x->i2c, msg, 2);
	if (ret != 2) {
		dprintk(FE_ERROR, 1, "I/O Error");
		return -EREMOTEIO;
	}
	*data = b1[0];

	return 0;
}

static int stv6110x_write_regs(struct stv6110x_state *stv6110x, int start, u8 data[], int len)
{
	int ret;
	const struct stv6110x_config *config = stv6110x->config;
	u8 buf[len + 1];
	struct i2c_msg msg = {
		.addr = config->addr,
		.flags = 0,
		.buf = buf,
		.len = len + 1
	};

	if (start + len > 8)
		return -EINVAL;

	buf[0] = start;
	memcpy(&buf[1], data, len);

	ret = i2c_transfer(stv6110x->i2c, &msg, 1);
	if (ret != 1) {
		dprintk(FE_ERROR, 1, "I/O Error");
		return -EREMOTEIO;
	}

	return 0;
}

static int stv6110x_write_reg(struct stv6110x_state *stv6110x, u8 reg, u8 data)
{
	return stv6110x_write_regs(stv6110x, reg, &data, 1);
}

static int stv6110x_init(struct dvb_frontend *fe)
{
	struct stv6110x_state *stv6110x = fe->tuner_priv;
	int ret;

	ret = stv6110x_write_regs(stv6110x, 0, stv6110x->regs,
				  ARRAY_SIZE(stv6110x->regs));
	if (ret < 0) {
		dprintk(FE_ERROR, 1, "Initialization failed");
		return -1;
	}

	return 0;
}

static int stv6110x_set_frequency(struct dvb_frontend *fe, u32 frequency)
{
	struct stv6110x_state *stv6110x = fe->tuner_priv;
	u32 rDiv, divider;
	s32 pVal, pCalc, rDivOpt = 0, pCalcOpt = 1000;
	u8 i;

	STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL1], CTRL1_K, (REFCLOCK_MHz - 16));

	if (frequency <= 1023000) {
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_TNG1], TNG1_DIV4SEL, 1);
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_TNG1], TNG1_PRESC32_ON, 0);
		pVal = 40;
	} else if (frequency <= 1300000) {
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_TNG1], TNG1_DIV4SEL, 1);
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_TNG1], TNG1_PRESC32_ON, 1);
		pVal = 40;
	} else if (frequency <= 2046000) {
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_TNG1], TNG1_DIV4SEL, 0);
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_TNG1], TNG1_PRESC32_ON, 0);
		pVal = 20;
	} else {
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_TNG1], TNG1_DIV4SEL, 0);
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_TNG1], TNG1_PRESC32_ON, 1);
		pVal = 20;
	}

	for (rDiv = 0; rDiv <= 3; rDiv++) {
		pCalc = (REFCLOCK_kHz / 100) / R_DIV(rDiv);

		if ((abs((s32)(pCalc - pVal))) < (abs((s32)(pCalcOpt - pVal))))
			rDivOpt = rDiv;

		pCalcOpt = (REFCLOCK_kHz / 100) / R_DIV(rDivOpt);
	}

	divider = (frequency * R_DIV(rDivOpt) * pVal) / REFCLOCK_kHz;
	divider = (divider + 5) / 10;

	STV6110x_SETFIELD(stv6110x->regs[STV6110x_TNG1], TNG1_R_DIV, rDivOpt);
	STV6110x_SETFIELD(stv6110x->regs[STV6110x_TNG1], TNG1_N_DIV_11_8, MSB(divider));
	STV6110x_SETFIELD(stv6110x->regs[STV6110x_TNG0], TNG0_N_DIV_7_0, LSB(divider));

	/* VCO Auto calibration */
	STV6110x_SETFIELD(stv6110x->regs[STV6110x_STAT1], STAT1_CALVCO_STRT, 1);

	stv6110x_write_reg(stv6110x, STV6110x_CTRL1, stv6110x->regs[STV6110x_CTRL1]);
	stv6110x_write_reg(stv6110x, STV6110x_TNG1, stv6110x->regs[STV6110x_TNG1]);
	stv6110x_write_reg(stv6110x, STV6110x_TNG0, stv6110x->regs[STV6110x_TNG0]);
	stv6110x_write_reg(stv6110x, STV6110x_STAT1, stv6110x->regs[STV6110x_STAT1]);

	for (i = 0; i < TRIALS; i++) {
		stv6110x_read_reg(stv6110x, STV6110x_STAT1, &stv6110x->regs[STV6110x_STAT1]);
		if (!STV6110x_GETFIELD(STAT1_CALVCO_STRT, stv6110x->regs[STV6110x_STAT1]))
				break;
		msleep(1);
	}

	return 0;
}

static int stv6110x_get_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	struct stv6110x_state *stv6110x = fe->tuner_priv;

	stv6110x_read_reg(stv6110x, STV6110x_TNG1, &stv6110x->regs[STV6110x_TNG1]);
	stv6110x_read_reg(stv6110x, STV6110x_TNG0, &stv6110x->regs[STV6110x_TNG0]);

	*frequency = (MAKEWORD16(STV6110x_GETFIELD(TNG1_N_DIV_11_8, stv6110x->regs[STV6110x_TNG1]),
				 STV6110x_GETFIELD(TNG0_N_DIV_7_0, stv6110x->regs[STV6110x_TNG0]))) * REFCLOCK_kHz;

	*frequency /= (1 << (STV6110x_GETFIELD(TNG1_R_DIV, stv6110x->regs[STV6110x_TNG1]) +
			     STV6110x_GETFIELD(TNG1_DIV4SEL, stv6110x->regs[STV6110x_TNG1])));

	*frequency >>= 2;

	return 0;
}

static int stv6110x_set_bandwidth(struct dvb_frontend *fe, u32 bandwidth)
{
	struct stv6110x_state *stv6110x = fe->tuner_priv;
	u32 halfbw;
	u8 i;

	halfbw = bandwidth >> 1;

	if (halfbw > 36000000)
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL3], CTRL3_CF, 31); /* LPF */
	else if (halfbw < 5000000)
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL3], CTRL3_CF, 0); /* LPF */
	else
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL3], CTRL3_CF, ((halfbw / 1000000) - 5)); /* LPF */


	STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL3], CTRL3_RCCLK_OFF, 0x0); /* cal. clk activated */
	STV6110x_SETFIELD(stv6110x->regs[STV6110x_STAT1], STAT1_CALRC_STRT, 0x1); /* LPF auto cal */

	stv6110x_write_reg(stv6110x, STV6110x_CTRL3, stv6110x->regs[STV6110x_CTRL3]);
	stv6110x_write_reg(stv6110x, STV6110x_STAT1, stv6110x->regs[STV6110x_STAT1]);

	for (i = 0; i < TRIALS; i++) {
		stv6110x_read_reg(stv6110x, STV6110x_STAT1, &stv6110x->regs[STV6110x_STAT1]);
		if (!STV6110x_GETFIELD(STAT1_CALRC_STRT, stv6110x->regs[STV6110x_STAT1]))
			break;
		msleep(1);
	}
	STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL3], CTRL3_RCCLK_OFF, 0x1); /* cal. done */
	stv6110x_write_reg(stv6110x, STV6110x_CTRL3, stv6110x->regs[STV6110x_CTRL3]);

	return 0;
}

static int stv6110x_get_bandwidth(struct dvb_frontend *fe, u32 *bandwidth)
{
	struct stv6110x_state *stv6110x = fe->tuner_priv;

	stv6110x_read_reg(stv6110x, STV6110x_CTRL3, &stv6110x->regs[STV6110x_CTRL3]);
	*bandwidth = (STV6110x_GETFIELD(CTRL3_CF, stv6110x->regs[STV6110x_CTRL3]) + 5) * 2000000;

	return 0;
}

static int stv6110x_set_refclock(struct dvb_frontend *fe, u32 refclock)
{
	struct stv6110x_state *stv6110x = fe->tuner_priv;

	/* setup divider */
	switch (refclock) {
	default:
	case 1:
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL2], CTRL2_CO_DIV, 0);
		break;
	case 2:
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL2], CTRL2_CO_DIV, 1);
		break;
	case 4:
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL2], CTRL2_CO_DIV, 2);
		break;
	case 8:
	case 0:
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL2], CTRL2_CO_DIV, 3);
		break;
	}
	stv6110x_write_reg(stv6110x, STV6110x_CTRL2, stv6110x->regs[STV6110x_CTRL2]);

	return 0;
}

static int stv6110x_get_bbgain(struct dvb_frontend *fe, u32 *gain)
{
	struct stv6110x_state *stv6110x = fe->tuner_priv;

	stv6110x_read_reg(stv6110x, STV6110x_CTRL2, &stv6110x->regs[STV6110x_CTRL2]);
	*gain = 2 * STV6110x_GETFIELD(CTRL2_BBGAIN, stv6110x->regs[STV6110x_CTRL2]);

	return 0;
}

static int stv6110x_set_bbgain(struct dvb_frontend *fe, u32 gain)
{
	struct stv6110x_state *stv6110x = fe->tuner_priv;

	STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL2], CTRL2_BBGAIN, gain / 2);
	stv6110x_write_reg(stv6110x, STV6110x_CTRL2, stv6110x->regs[STV6110x_CTRL2]);

	return 0;
}

static int stv6110x_set_mode(struct dvb_frontend *fe, enum tuner_mode mode)
{
	struct stv6110x_state *stv6110x = fe->tuner_priv;
	int ret;

	switch (mode) {
	case TUNER_SLEEP:
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL1], CTRL1_SYN, 0);
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL1], CTRL1_RX, 0);
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL1], CTRL1_LPT, 0);
		break;

	case TUNER_WAKE:
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL1], CTRL1_SYN, 1);
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL1], CTRL1_RX, 1);
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL1], CTRL1_LPT, 1);
		break;
	}

	ret = stv6110x_write_reg(stv6110x, STV6110x_CTRL1, stv6110x->regs[STV6110x_CTRL1]);
	if (ret < 0) {
		dprintk(FE_ERROR, 1, "I/O Error");
		return -EIO;
	}

	return 0;
}

static int stv6110x_sleep(struct dvb_frontend *fe)
{
	if (fe->tuner_priv)
		return stv6110x_set_mode(fe, TUNER_SLEEP);

	return 0;
}

static int stv6110x_get_status(struct dvb_frontend *fe, u32 *status)
{
	struct stv6110x_state *stv6110x = fe->tuner_priv;

	stv6110x_read_reg(stv6110x, STV6110x_STAT1, &stv6110x->regs[STV6110x_STAT1]);

	if (STV6110x_GETFIELD(STAT1_LOCK, stv6110x->regs[STV6110x_STAT1]))
		*status = TUNER_PHASELOCKED;
	else
		*status = 0;

	return 0;
}


static int stv6110x_release(struct dvb_frontend *fe)
{
	struct stv6110x_state *stv6110x = fe->tuner_priv;

	fe->tuner_priv = NULL;
	kfree(stv6110x);

	return 0;
}

static struct dvb_tuner_ops stv6110x_ops = {
	.info = {
		.name		= "STV6110(A) Silicon Tuner",
		.frequency_min	=  950000,
		.frequency_max	= 2150000,
		.frequency_step	= 0,
	},
	.release		= stv6110x_release
};

static struct stv6110x_devctl stv6110x_ctl = {
	.tuner_init		= stv6110x_init,
	.tuner_sleep		= stv6110x_sleep,
	.tuner_set_mode		= stv6110x_set_mode,
	.tuner_set_frequency	= stv6110x_set_frequency,
	.tuner_get_frequency	= stv6110x_get_frequency,
	.tuner_set_bandwidth	= stv6110x_set_bandwidth,
	.tuner_get_bandwidth	= stv6110x_get_bandwidth,
	.tuner_set_bbgain	= stv6110x_set_bbgain,
	.tuner_get_bbgain	= stv6110x_get_bbgain,
	.tuner_set_refclk	= stv6110x_set_refclock,
	.tuner_get_status	= stv6110x_get_status,
};

struct stv6110x_devctl *stv6110x_attach(struct dvb_frontend *fe,
					const struct stv6110x_config *config,
					struct i2c_adapter *i2c)
{
	struct stv6110x_state *stv6110x;
	u8 default_regs[] = {0x07, 0x11, 0xdc, 0x85, 0x17, 0x01, 0xe6, 0x1e};

	stv6110x = kzalloc(sizeof (struct stv6110x_state), GFP_KERNEL);
	if (!stv6110x)
		return NULL;

	stv6110x->i2c		= i2c;
	stv6110x->config	= config;
	stv6110x->devctl	= &stv6110x_ctl;
	memcpy(stv6110x->regs, default_regs, 8);

	/* setup divider */
	switch (stv6110x->config->clk_div) {
	default:
	case 1:
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL2], CTRL2_CO_DIV, 0);
		break;
	case 2:
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL2], CTRL2_CO_DIV, 1);
		break;
	case 4:
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL2], CTRL2_CO_DIV, 2);
		break;
	case 8:
	case 0:
		STV6110x_SETFIELD(stv6110x->regs[STV6110x_CTRL2], CTRL2_CO_DIV, 3);
		break;
	}

	fe->tuner_priv		= stv6110x;
	fe->ops.tuner_ops	= stv6110x_ops;

	printk(KERN_INFO "%s: Attaching STV6110x\n", __func__);
	return stv6110x->devctl;
}
EXPORT_SYMBOL(stv6110x_attach);

MODULE_AUTHOR("Manu Abraham");
MODULE_DESCRIPTION("STV6110x Silicon tuner");
MODULE_LICENSE("GPL");
