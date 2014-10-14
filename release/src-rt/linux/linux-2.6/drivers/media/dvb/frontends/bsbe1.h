/*
 * bsbe1.h - ALPS BSBE1 tuner support
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 *
 * the project's page is at http://www.linuxtv.org
 */

#ifndef BSBE1_H
#define BSBE1_H

static u8 alps_bsbe1_inittab[] = {
	0x01, 0x15,   /* XTAL = 4MHz, VCO = 352 MHz */
	0x02, 0x30,   /* MCLK = 88 MHz */
	0x03, 0x00,   /* ACR output 0 */
	0x04, 0x7d,   /* F22FR = 0x7d, F22 = f_VCO / 128 / 0x7d = 22 kHz */
	0x05, 0x05,   /* I2CT = 0, SCLT = 1, SDAT = 1 */
	0x06, 0x00,   /* DAC output 0 */
	0x08, 0x40,   /* DiSEqC off, LNB power on OP2/LOCK pin on */
	0x09, 0x00,   /* FIFO */
	0x0c, 0x51,   /* OP1/OP0 normal, val = 1 (LNB power on) */
	0x0d, 0x82,   /* DC offset compensation = on, beta_agc1 = 2 */
	0x0f, 0x92,   /* AGC1R */
	0x10, 0x34,   /* AGC2O */
	0x11, 0x84,   /* TLSR */
	0x12, 0xb9,   /* CFD */
	0x15, 0xc9,   /* lock detector threshold */
	0x28, 0x00,   /* out imp: normal, type: parallel, FEC mode: QPSK */
	0x33, 0xfc,   /* RS control */
	0x34, 0x93,   /* count viterbi bit errors per 2E18 bytes */
	0xff, 0xff
};


static int alps_bsbe1_set_symbol_rate(struct dvb_frontend* fe, u32 srate, u32 ratio)
{
	u8 aclk = 0;
	u8 bclk = 0;

	if (srate < 1500000) { aclk = 0xb7; bclk = 0x47; }
	else if (srate < 3000000) { aclk = 0xb7; bclk = 0x4b; }
	else if (srate < 7000000) { aclk = 0xb7; bclk = 0x4f; }
	else if (srate < 14000000) { aclk = 0xb7; bclk = 0x53; }
	else if (srate < 30000000) { aclk = 0xb6; bclk = 0x53; }
	else if (srate < 45000000) { aclk = 0xb4; bclk = 0x51; }

	stv0299_writereg(fe, 0x13, aclk);
	stv0299_writereg(fe, 0x14, bclk);
	stv0299_writereg(fe, 0x1f, (ratio >> 16) & 0xff);
	stv0299_writereg(fe, 0x20, (ratio >>  8) & 0xff);
	stv0299_writereg(fe, 0x21, (ratio      ) & 0xf0);

	return 0;
}

static int alps_bsbe1_tuner_set_params(struct dvb_frontend* fe, struct dvb_frontend_parameters *params)
{
	int ret;
	u8 data[4];
	u32 div;
	struct i2c_msg msg = { .addr = 0x61, .flags = 0, .buf = data, .len = sizeof(data) };
	struct i2c_adapter *i2c = fe->tuner_priv;

	if ((params->frequency < 950000) || (params->frequency > 2150000))
		return -EINVAL;

	div = params->frequency / 1000;
	data[0] = (div >> 8) & 0x7f;
	data[1] = div & 0xff;
	data[2] = 0x80 | ((div & 0x18000) >> 10) | 0x1;
	data[3] = 0xe0;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
	ret = i2c_transfer(i2c, &msg, 1);
	return (ret != 1) ? -EIO : 0;
}

static struct stv0299_config alps_bsbe1_config = {
	.demod_address = 0x68,
	.inittab = alps_bsbe1_inittab,
	.mclk = 88000000UL,
	.invert = 1,
	.skip_reinit = 0,
	.min_delay_ms = 100,
	.set_symbol_rate = alps_bsbe1_set_symbol_rate,
};

#endif
