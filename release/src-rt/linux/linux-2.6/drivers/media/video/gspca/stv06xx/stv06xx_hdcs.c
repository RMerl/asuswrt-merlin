/*
 * Copyright (c) 2001 Jean-Fredric Clere, Nikolas Zimmermann, Georg Acher
 *		      Mark Cave-Ayland, Carlo E Prelz, Dick Streefland
 * Copyright (c) 2002, 2003 Tuukka Toivonen
 * Copyright (c) 2008 Erik Andrén
 * Copyright (c) 2008 Chia-I Wu
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * P/N 861037:      Sensor HDCS1000        ASIC STV0600
 * P/N 861050-0010: Sensor HDCS1000        ASIC STV0600
 * P/N 861050-0020: Sensor Photobit PB100  ASIC STV0600-1 - QuickCam Express
 * P/N 861055:      Sensor ST VV6410       ASIC STV0610   - LEGO cam
 * P/N 861075-0040: Sensor HDCS1000        ASIC
 * P/N 961179-0700: Sensor ST VV6410       ASIC STV0602   - Dexxa WebCam USB
 * P/N 861040-0000: Sensor ST VV6410       ASIC STV0610   - QuickCam Web
 */

#include "stv06xx_hdcs.h"

static const struct ctrl hdcs1x00_ctrl[] = {
	{
		{
			.id		= V4L2_CID_EXPOSURE,
			.type		= V4L2_CTRL_TYPE_INTEGER,
			.name		= "exposure",
			.minimum	= 0x00,
			.maximum	= 0xff,
			.step		= 0x1,
			.default_value	= HDCS_DEFAULT_EXPOSURE,
			.flags		= V4L2_CTRL_FLAG_SLIDER
		},
		.set = hdcs_set_exposure,
		.get = hdcs_get_exposure
	}, {
		{
			.id		= V4L2_CID_GAIN,
			.type		= V4L2_CTRL_TYPE_INTEGER,
			.name		= "gain",
			.minimum	= 0x00,
			.maximum	= 0xff,
			.step		= 0x1,
			.default_value	= HDCS_DEFAULT_GAIN,
			.flags		= V4L2_CTRL_FLAG_SLIDER
		},
		.set = hdcs_set_gain,
		.get = hdcs_get_gain
	}
};

static struct v4l2_pix_format hdcs1x00_mode[] = {
	{
		HDCS_1X00_DEF_WIDTH,
		HDCS_1X00_DEF_HEIGHT,
		V4L2_PIX_FMT_SGRBG8,
		V4L2_FIELD_NONE,
		.sizeimage =
			HDCS_1X00_DEF_WIDTH * HDCS_1X00_DEF_HEIGHT,
		.bytesperline = HDCS_1X00_DEF_WIDTH,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1
	}
};

static const struct ctrl hdcs1020_ctrl[] = {
	{
		{
			.id		= V4L2_CID_EXPOSURE,
			.type		= V4L2_CTRL_TYPE_INTEGER,
			.name		= "exposure",
			.minimum	= 0x00,
			.maximum	= 0xffff,
			.step		= 0x1,
			.default_value	= HDCS_DEFAULT_EXPOSURE,
			.flags		= V4L2_CTRL_FLAG_SLIDER
		},
		.set = hdcs_set_exposure,
		.get = hdcs_get_exposure
	}, {
		{
			.id		= V4L2_CID_GAIN,
			.type		= V4L2_CTRL_TYPE_INTEGER,
			.name		= "gain",
			.minimum	= 0x00,
			.maximum	= 0xff,
			.step		= 0x1,
			.default_value	= HDCS_DEFAULT_GAIN,
			.flags		= V4L2_CTRL_FLAG_SLIDER
		},
		.set = hdcs_set_gain,
		.get = hdcs_get_gain
	}
};

static struct v4l2_pix_format hdcs1020_mode[] = {
	{
		HDCS_1020_DEF_WIDTH,
		HDCS_1020_DEF_HEIGHT,
		V4L2_PIX_FMT_SGRBG8,
		V4L2_FIELD_NONE,
		.sizeimage =
			HDCS_1020_DEF_WIDTH * HDCS_1020_DEF_HEIGHT,
		.bytesperline = HDCS_1020_DEF_WIDTH,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1
	}
};

enum hdcs_power_state {
	HDCS_STATE_SLEEP,
	HDCS_STATE_IDLE,
	HDCS_STATE_RUN
};

/* no lock? */
struct hdcs {
	enum hdcs_power_state state;
	int w, h;

	/* visible area of the sensor array */
	struct {
		int left, top;
		int width, height;
		int border;
	} array;

	struct {
		/* Column timing overhead */
		u8 cto;
		/* Column processing overhead */
		u8 cpo;
		/* Row sample period constant */
		u16 rs;
		/* Exposure reset duration */
		u16 er;
	} exp;

	int psmp;
	u8 exp_cache, gain_cache;
};

static int hdcs_reg_write_seq(struct sd *sd, u8 reg, u8 *vals, u8 len)
{
	u8 regs[I2C_MAX_BYTES * 2];
	int i;

	if (unlikely((len <= 0) || (len >= I2C_MAX_BYTES) ||
		     (reg + len > 0xff)))
		return -EINVAL;

	for (i = 0; i < len; i++) {
		regs[2 * i] = reg;
		regs[2 * i + 1] = vals[i];
		/* All addresses are shifted left one bit
		 * as bit 0 toggles r/w */
		reg += 2;
	}

	return stv06xx_write_sensor_bytes(sd, regs, len);
}

static int hdcs_set_state(struct sd *sd, enum hdcs_power_state state)
{
	struct hdcs *hdcs = sd->sensor_priv;
	u8 val;
	int ret;

	if (hdcs->state == state)
		return 0;

	/* we need to go idle before running or sleeping */
	if (hdcs->state != HDCS_STATE_IDLE) {
		ret = stv06xx_write_sensor(sd, HDCS_REG_CONTROL(sd), 0);
		if (ret)
			return ret;
	}

	hdcs->state = HDCS_STATE_IDLE;

	if (state == HDCS_STATE_IDLE)
		return 0;

	switch (state) {
	case HDCS_STATE_SLEEP:
		val = HDCS_SLEEP_MODE;
		break;

	case HDCS_STATE_RUN:
		val = HDCS_RUN_ENABLE;
		break;

	default:
		return -EINVAL;
	}

	ret = stv06xx_write_sensor(sd, HDCS_REG_CONTROL(sd), val);

	/* Update the state if the write succeeded */
	if (!ret)
		hdcs->state = state;

	return ret;
}

static int hdcs_reset(struct sd *sd)
{
	struct hdcs *hdcs = sd->sensor_priv;
	int err;

	err = stv06xx_write_sensor(sd, HDCS_REG_CONTROL(sd), 1);
	if (err < 0)
		return err;

	err = stv06xx_write_sensor(sd, HDCS_REG_CONTROL(sd), 0);
	if (err < 0)
		hdcs->state = HDCS_STATE_IDLE;

	return err;
}

static int hdcs_get_exposure(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct hdcs *hdcs = sd->sensor_priv;

	*val = hdcs->exp_cache;

	return 0;
}

static int hdcs_set_exposure(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct hdcs *hdcs = sd->sensor_priv;
	int rowexp, srowexp;
	int max_srowexp;
	/* Column time period */
	int ct;
	/* Column processing period */
	int cp;
	/* Row processing period */
	int rp;
	/* Minimum number of column timing periods
	   within the column processing period */
	int mnct;
	int cycles, err;
	u8 exp[14];

	val &= 0xff;
	hdcs->exp_cache = val;

	cycles = val * HDCS_CLK_FREQ_MHZ * 257;

	ct = hdcs->exp.cto + hdcs->psmp + (HDCS_ADC_START_SIG_DUR + 2);
	cp = hdcs->exp.cto + (hdcs->w * ct / 2);

	/* the cycles one row takes */
	rp = hdcs->exp.rs + cp;

	rowexp = cycles / rp;

	/* the remaining cycles */
	cycles -= rowexp * rp;

	/* calculate sub-row exposure */
	if (IS_1020(sd)) {
		/* see HDCS-1020 datasheet 3.5.6.4, p. 63 */
		srowexp = hdcs->w - (cycles + hdcs->exp.er + 13) / ct;

		mnct = (hdcs->exp.er + 12 + ct - 1) / ct;
		max_srowexp = hdcs->w - mnct;
	} else {
		/* see HDCS-1000 datasheet 3.4.5.5, p. 61 */
		srowexp = cp - hdcs->exp.er - 6 - cycles;

		mnct = (hdcs->exp.er + 5 + ct - 1) / ct;
		max_srowexp = cp - mnct * ct - 1;
	}

	if (srowexp < 0)
		srowexp = 0;
	else if (srowexp > max_srowexp)
		srowexp = max_srowexp;

	if (IS_1020(sd)) {
		exp[0] = HDCS20_CONTROL;
		exp[1] = 0x00;		/* Stop streaming */
		exp[2] = HDCS_ROWEXPL;
		exp[3] = rowexp & 0xff;
		exp[4] = HDCS_ROWEXPH;
		exp[5] = rowexp >> 8;
		exp[6] = HDCS20_SROWEXP;
		exp[7] = (srowexp >> 2) & 0xff;
		exp[8] = HDCS20_ERROR;
		exp[9] = 0x10;		/* Clear exposure error flag*/
		exp[10] = HDCS20_CONTROL;
		exp[11] = 0x04;		/* Restart streaming */
		err = stv06xx_write_sensor_bytes(sd, exp, 6);
	} else {
		exp[0] = HDCS00_CONTROL;
		exp[1] = 0x00;         /* Stop streaming */
		exp[2] = HDCS_ROWEXPL;
		exp[3] = rowexp & 0xff;
		exp[4] = HDCS_ROWEXPH;
		exp[5] = rowexp >> 8;
		exp[6] = HDCS00_SROWEXPL;
		exp[7] = srowexp & 0xff;
		exp[8] = HDCS00_SROWEXPH;
		exp[9] = srowexp >> 8;
		exp[10] = HDCS_STATUS;
		exp[11] = 0x10;         /* Clear exposure error flag*/
		exp[12] = HDCS00_CONTROL;
		exp[13] = 0x04;         /* Restart streaming */
		err = stv06xx_write_sensor_bytes(sd, exp, 7);
		if (err < 0)
			return err;
	}
	PDEBUG(D_V4L2, "Writing exposure %d, rowexp %d, srowexp %d",
	       val, rowexp, srowexp);
	return err;
}

static int hdcs_set_gains(struct sd *sd, u8 g)
{
	struct hdcs *hdcs = sd->sensor_priv;
	int err;
	u8 gains[4];

	hdcs->gain_cache = g;

	/* the voltage gain Av = (1 + 19 * val / 127) * (1 + bit7) */
	if (g > 127)
		g = 0x80 | (g / 2);

	gains[0] = g;
	gains[1] = g;
	gains[2] = g;
	gains[3] = g;

	err = hdcs_reg_write_seq(sd, HDCS_ERECPGA, gains, 4);
		return err;
}

static int hdcs_get_gain(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct hdcs *hdcs = sd->sensor_priv;

	*val = hdcs->gain_cache;

	return 0;
}

static int hdcs_set_gain(struct gspca_dev *gspca_dev, __s32 val)
{
	PDEBUG(D_V4L2, "Writing gain %d", val);
	return hdcs_set_gains((struct sd *) gspca_dev,
			       val & 0xff);
}

static int hdcs_set_size(struct sd *sd,
		unsigned int width, unsigned int height)
{
	struct hdcs *hdcs = sd->sensor_priv;
	u8 win[4];
	unsigned int x, y;
	int err;

	/* must be multiple of 4 */
	width = (width + 3) & ~0x3;
	height = (height + 3) & ~0x3;

	if (width > hdcs->array.width)
		width = hdcs->array.width;

	if (IS_1020(sd)) {
		/* the borders are also invalid */
		if (height + 2 * hdcs->array.border + HDCS_1020_BOTTOM_Y_SKIP
				  > hdcs->array.height)
			height = hdcs->array.height - 2 * hdcs->array.border -
				HDCS_1020_BOTTOM_Y_SKIP;

		y = (hdcs->array.height - HDCS_1020_BOTTOM_Y_SKIP - height) / 2
				+ hdcs->array.top;
	} else {
		if (height > hdcs->array.height)
			height = hdcs->array.height;

		y = hdcs->array.top + (hdcs->array.height - height) / 2;
	}

	x = hdcs->array.left + (hdcs->array.width - width) / 2;

	win[0] = y / 4;
	win[1] = x / 4;
	win[2] = (y + height) / 4 - 1;
	win[3] = (x + width) / 4 - 1;

	err = hdcs_reg_write_seq(sd, HDCS_FWROW, win, 4);
	if (err < 0)
		return err;

	/* Update the current width and height */
	hdcs->w = width;
	hdcs->h = height;
	return err;
}

static int hdcs_probe_1x00(struct sd *sd)
{
	struct hdcs *hdcs;
	u16 sensor;
	int ret;

	ret = stv06xx_read_sensor(sd, HDCS_IDENT, &sensor);
	if (ret < 0 || sensor != 0x08)
		return -ENODEV;

	info("HDCS-1000/1100 sensor detected");

	sd->gspca_dev.cam.cam_mode = hdcs1x00_mode;
	sd->gspca_dev.cam.nmodes = ARRAY_SIZE(hdcs1x00_mode);
	sd->desc.ctrls = hdcs1x00_ctrl;
	sd->desc.nctrls = ARRAY_SIZE(hdcs1x00_ctrl);

	hdcs = kmalloc(sizeof(struct hdcs), GFP_KERNEL);
	if (!hdcs)
		return -ENOMEM;

	hdcs->array.left = 8;
	hdcs->array.top = 8;
	hdcs->array.width = HDCS_1X00_DEF_WIDTH;
	hdcs->array.height = HDCS_1X00_DEF_HEIGHT;
	hdcs->array.border = 4;

	hdcs->exp.cto = 4;
	hdcs->exp.cpo = 2;
	hdcs->exp.rs = 186;
	hdcs->exp.er = 100;

	/*
	 * Frame rate on HDCS-1000 with STV600 depends on PSMP:
	 *  4 = doesn't work at all
	 *  5 = 7.8 fps,
	 *  6 = 6.9 fps,
	 *  8 = 6.3 fps,
	 * 10 = 5.5 fps,
	 * 15 = 4.4 fps,
	 * 31 = 2.8 fps
	 *
	 * Frame rate on HDCS-1000 with STV602 depends on PSMP:
	 * 15 = doesn't work at all
	 * 18 = doesn't work at all
	 * 19 = 7.3 fps
	 * 20 = 7.4 fps
	 * 21 = 7.4 fps
	 * 22 = 7.4 fps
	 * 24 = 6.3 fps
	 * 30 = 5.4 fps
	 */
	hdcs->psmp = (sd->bridge == BRIDGE_STV602) ? 20 : 5;

	sd->sensor_priv = hdcs;

	return 0;
}

static int hdcs_probe_1020(struct sd *sd)
{
	struct hdcs *hdcs;
	u16 sensor;
	int ret;

	ret = stv06xx_read_sensor(sd, HDCS_IDENT, &sensor);
	if (ret < 0 || sensor != 0x10)
		return -ENODEV;

	info("HDCS-1020 sensor detected");

	sd->gspca_dev.cam.cam_mode = hdcs1020_mode;
	sd->gspca_dev.cam.nmodes = ARRAY_SIZE(hdcs1020_mode);
	sd->desc.ctrls = hdcs1020_ctrl;
	sd->desc.nctrls = ARRAY_SIZE(hdcs1020_ctrl);

	hdcs = kmalloc(sizeof(struct hdcs), GFP_KERNEL);
	if (!hdcs)
		return -ENOMEM;

	/*
	 * From Andrey's test image: looks like HDCS-1020 upper-left
	 * visible pixel is at 24,8 (y maybe even smaller?) and lower-right
	 * visible pixel at 375,299 (x maybe even larger?)
	 */
	hdcs->array.left = 24;
	hdcs->array.top  = 4;
	hdcs->array.width = HDCS_1020_DEF_WIDTH;
	hdcs->array.height = 304;
	hdcs->array.border = 4;

	hdcs->psmp = 6;

	hdcs->exp.cto = 3;
	hdcs->exp.cpo = 3;
	hdcs->exp.rs = 155;
	hdcs->exp.er = 96;

	sd->sensor_priv = hdcs;

	return 0;
}

static int hdcs_start(struct sd *sd)
{
	PDEBUG(D_STREAM, "Starting stream");

	return hdcs_set_state(sd, HDCS_STATE_RUN);
}

static int hdcs_stop(struct sd *sd)
{
	PDEBUG(D_STREAM, "Halting stream");

	return hdcs_set_state(sd, HDCS_STATE_SLEEP);
}

static void hdcs_disconnect(struct sd *sd)
{
	PDEBUG(D_PROBE, "Disconnecting the sensor");
	kfree(sd->sensor_priv);
}

static int hdcs_init(struct sd *sd)
{
	struct hdcs *hdcs = sd->sensor_priv;
	int i, err = 0;

	/* Set the STV0602AA in STV0600 emulation mode */
	if (sd->bridge == BRIDGE_STV602)
		stv06xx_write_bridge(sd, STV_STV0600_EMULATION, 1);

	/* Execute the bridge init */
	for (i = 0; i < ARRAY_SIZE(stv_bridge_init) && !err; i++) {
		err = stv06xx_write_bridge(sd, stv_bridge_init[i][0],
					   stv_bridge_init[i][1]);
	}
	if (err < 0)
		return err;

	/* sensor soft reset */
	hdcs_reset(sd);

	/* Execute the sensor init */
	for (i = 0; i < ARRAY_SIZE(stv_sensor_init) && !err; i++) {
		err = stv06xx_write_sensor(sd, stv_sensor_init[i][0],
					     stv_sensor_init[i][1]);
	}
	if (err < 0)
		return err;

	/* Enable continuous frame capture, bit 2: stop when frame complete */
	err = stv06xx_write_sensor(sd, HDCS_REG_CONFIG(sd), BIT(3));
	if (err < 0)
		return err;

	/* Set PGA sample duration
	(was 0x7E for the STV602, but caused slow framerate with HDCS-1020) */
	if (IS_1020(sd))
		err = stv06xx_write_sensor(sd, HDCS_TCTRL,
				(HDCS_ADC_START_SIG_DUR << 6) | hdcs->psmp);
	else
		err = stv06xx_write_sensor(sd, HDCS_TCTRL,
				(HDCS_ADC_START_SIG_DUR << 5) | hdcs->psmp);
	if (err < 0)
		return err;

	err = hdcs_set_gains(sd, HDCS_DEFAULT_GAIN);
	if (err < 0)
		return err;

	err = hdcs_set_size(sd, hdcs->array.width, hdcs->array.height);
	if (err < 0)
		return err;

	err = hdcs_set_exposure(&sd->gspca_dev, HDCS_DEFAULT_EXPOSURE);
	return err;
}

static int hdcs_dump(struct sd *sd)
{
	u16 reg, val;

	info("Dumping sensor registers:");

	for (reg = HDCS_IDENT; reg <= HDCS_ROWEXPH; reg++) {
		stv06xx_read_sensor(sd, reg, &val);
		info("reg 0x%02x = 0x%02x", reg, val);
	}
	return 0;
}
