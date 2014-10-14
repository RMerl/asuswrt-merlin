/*
 * E3C EC100 demodulator driver
 *
 * Copyright (C) 2009 Antti Palosaari <crope@iki.fi>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "dvb_frontend.h"
#include "ec100_priv.h"
#include "ec100.h"

int ec100_debug;
module_param_named(debug, ec100_debug, int, 0644);
MODULE_PARM_DESC(debug, "Turn on/off frontend debugging (default:off).");

struct ec100_state {
	struct i2c_adapter *i2c;
	struct dvb_frontend frontend;
	struct ec100_config config;

	u16 ber;
};

/* write single register */
static int ec100_write_reg(struct ec100_state *state, u8 reg, u8 val)
{
	u8 buf[2] = {reg, val};
	struct i2c_msg msg = {
		.addr = state->config.demod_address,
		.flags = 0,
		.len = 2,
		.buf = buf};

	if (i2c_transfer(state->i2c, &msg, 1) != 1) {
		warn("I2C write failed reg:%02x", reg);
		return -EREMOTEIO;
	}
	return 0;
}

/* read single register */
static int ec100_read_reg(struct ec100_state *state, u8 reg, u8 *val)
{
	struct i2c_msg msg[2] = {
		{
			.addr = state->config.demod_address,
			.flags = 0,
			.len = 1,
			.buf = &reg
		}, {
			.addr = state->config.demod_address,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = val
		}
	};

	if (i2c_transfer(state->i2c, msg, 2) != 2) {
		warn("I2C read failed reg:%02x", reg);
		return -EREMOTEIO;
	}
	return 0;
}

static int ec100_set_frontend(struct dvb_frontend *fe,
	struct dvb_frontend_parameters *params)
{
	struct ec100_state *state = fe->demodulator_priv;
	int ret;
	u8 tmp, tmp2;

	deb_info("%s: freq:%d bw:%d\n", __func__, params->frequency,
		params->u.ofdm.bandwidth);

	/* program tuner */
	if (fe->ops.tuner_ops.set_params)
		fe->ops.tuner_ops.set_params(fe, params);

	ret = ec100_write_reg(state, 0x04, 0x06);
	if (ret)
		goto error;
	ret = ec100_write_reg(state, 0x67, 0x58);
	if (ret)
		goto error;
	ret = ec100_write_reg(state, 0x05, 0x18);
	if (ret)
		goto error;

	/* reg/bw |   6  |   7  |   8
	   -------+------+------+------
	   A 0x1b | 0xa1 | 0xe7 | 0x2c
	   A 0x1c | 0x55 | 0x63 | 0x72
	   -------+------+------+------
	   B 0x1b | 0xb7 | 0x00 | 0x49
	   B 0x1c | 0x55 | 0x64 | 0x72 */

	switch (params->u.ofdm.bandwidth) {
	case BANDWIDTH_6_MHZ:
		tmp = 0xb7;
		tmp2 = 0x55;
		break;
	case BANDWIDTH_7_MHZ:
		tmp = 0x00;
		tmp2 = 0x64;
		break;
	case BANDWIDTH_8_MHZ:
	default:
		tmp = 0x49;
		tmp2 = 0x72;
	}

	ret = ec100_write_reg(state, 0x1b, tmp);
	if (ret)
		goto error;
	ret = ec100_write_reg(state, 0x1c, tmp2);
	if (ret)
		goto error;

	ret = ec100_write_reg(state, 0x0c, 0xbb); /* if freq */
	if (ret)
		goto error;
	ret = ec100_write_reg(state, 0x0d, 0x31); /* if freq */
	if (ret)
		goto error;

	ret = ec100_write_reg(state, 0x08, 0x24);
	if (ret)
		goto error;

	ret = ec100_write_reg(state, 0x00, 0x00); /* go */
	if (ret)
		goto error;
	ret = ec100_write_reg(state, 0x00, 0x20); /* go */
	if (ret)
		goto error;

	return ret;
error:
	deb_info("%s: failed:%d\n", __func__, ret);
	return ret;
}

static int ec100_get_tune_settings(struct dvb_frontend *fe,
	struct dvb_frontend_tune_settings *fesettings)
{
	fesettings->min_delay_ms = 300;
	fesettings->step_size = 0;
	fesettings->max_drift = 0;

	return 0;
}

static int ec100_read_status(struct dvb_frontend *fe, fe_status_t *status)
{
	struct ec100_state *state = fe->demodulator_priv;
	int ret;
	u8 tmp;
	*status = 0;

	ret = ec100_read_reg(state, 0x42, &tmp);
	if (ret)
		goto error;

	if (tmp & 0x80) {
		/* bit7 set - have lock */
		*status |= FE_HAS_SIGNAL | FE_HAS_CARRIER | FE_HAS_VITERBI |
			FE_HAS_SYNC | FE_HAS_LOCK;
	} else {
		ret = ec100_read_reg(state, 0x01, &tmp);
		if (ret)
			goto error;

		if (tmp & 0x10) {
			/* bit4 set - have signal */
			*status |= FE_HAS_SIGNAL;
			if (!(tmp & 0x01)) {
				/* bit0 clear - have ~valid signal */
				*status |= FE_HAS_CARRIER |  FE_HAS_VITERBI;
			}
		}
	}

	return ret;
error:
	deb_info("%s: failed:%d\n", __func__, ret);
	return ret;
}

static int ec100_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	struct ec100_state *state = fe->demodulator_priv;
	int ret;
	u8 tmp, tmp2;
	u16 ber2;

	*ber = 0;

	ret = ec100_read_reg(state, 0x65, &tmp);
	if (ret)
		goto error;
	ret = ec100_read_reg(state, 0x66, &tmp2);
	if (ret)
		goto error;

	ber2 = (tmp2 << 8) | tmp;

	/* if counter overflow or clear */
	if (ber2 < state->ber)
		*ber = ber2;
	else
		*ber = ber2 - state->ber;

	state->ber = ber2;

	return ret;
error:
	deb_info("%s: failed:%d\n", __func__, ret);
	return ret;
}

static int ec100_read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	struct ec100_state *state = fe->demodulator_priv;
	int ret;
	u8 tmp;

	ret = ec100_read_reg(state, 0x24, &tmp);
	if (ret) {
		*strength = 0;
		goto error;
	}

	*strength = ((tmp << 8) | tmp);

	return ret;
error:
	deb_info("%s: failed:%d\n", __func__, ret);
	return ret;
}

static int ec100_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	*snr = 0;
	return 0;
}

static int ec100_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}

static void ec100_release(struct dvb_frontend *fe)
{
	struct ec100_state *state = fe->demodulator_priv;
	kfree(state);
}

static struct dvb_frontend_ops ec100_ops;

struct dvb_frontend *ec100_attach(const struct ec100_config *config,
	struct i2c_adapter *i2c)
{
	int ret;
	struct ec100_state *state = NULL;
	u8 tmp;

	/* allocate memory for the internal state */
	state = kzalloc(sizeof(struct ec100_state), GFP_KERNEL);
	if (state == NULL)
		goto error;

	/* setup the state */
	state->i2c = i2c;
	memcpy(&state->config, config, sizeof(struct ec100_config));

	/* check if the demod is there */
	ret = ec100_read_reg(state, 0x33, &tmp);
	if (ret || tmp != 0x0b)
		goto error;

	/* create dvb_frontend */
	memcpy(&state->frontend.ops, &ec100_ops,
		sizeof(struct dvb_frontend_ops));
	state->frontend.demodulator_priv = state;

	return &state->frontend;
error:
	kfree(state);
	return NULL;
}
EXPORT_SYMBOL(ec100_attach);

static struct dvb_frontend_ops ec100_ops = {
	.info = {
		.name = "E3C EC100 DVB-T",
		.type = FE_OFDM,
		.caps =
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 |
			FE_CAN_QAM_64 | FE_CAN_QAM_AUTO |
			FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO |
			FE_CAN_HIERARCHY_AUTO |
			FE_CAN_MUTE_TS
	},

	.release = ec100_release,
	.set_frontend = ec100_set_frontend,
	.get_tune_settings = ec100_get_tune_settings,
	.read_status = ec100_read_status,
	.read_ber = ec100_read_ber,
	.read_signal_strength = ec100_read_signal_strength,
	.read_snr = ec100_read_snr,
	.read_ucblocks = ec100_read_ucblocks,
};

MODULE_AUTHOR("Antti Palosaari <crope@iki.fi>");
MODULE_DESCRIPTION("E3C EC100 DVB-T demodulator driver");
MODULE_LICENSE("GPL");
