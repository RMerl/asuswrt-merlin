/*
 * wm8775 - driver version 0.0.1
 *
 * Copyright (C) 2004 Ulf Eklund <ivtv at eklund.to>
 *
 * Based on saa7115 driver
 *
 * Copyright (C) 2005 Hans Verkuil <hverkuil@xs4all.nl>
 * - Cleanup
 * - V4L2 API update
 * - sound fixes
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <linux/i2c.h>
#include <linux/i2c-id.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-i2c-drv.h>

MODULE_DESCRIPTION("wm8775 driver");
MODULE_AUTHOR("Ulf Eklund, Hans Verkuil");
MODULE_LICENSE("GPL");



/* ----------------------------------------------------------------------- */

enum {
	R7 = 7, R11 = 11,
	R12, R13, R14, R15, R16, R17, R18, R19, R20, R21, R23 = 23,
	TOT_REGS
};

struct wm8775_state {
	struct v4l2_subdev sd;
	struct v4l2_ctrl_handler hdl;
	struct v4l2_ctrl *mute;
	u8 input;		/* Last selected input (0-0xf) */
};

static inline struct wm8775_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct wm8775_state, sd);
}

static inline struct v4l2_subdev *to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct wm8775_state, hdl)->sd;
}

static int wm8775_write(struct v4l2_subdev *sd, int reg, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i;

	if (reg < 0 || reg >= TOT_REGS) {
		v4l2_err(sd, "Invalid register R%d\n", reg);
		return -1;
	}

	for (i = 0; i < 3; i++)
		if (i2c_smbus_write_byte_data(client,
				(reg << 1) | (val >> 8), val & 0xff) == 0)
			return 0;
	v4l2_err(sd, "I2C: cannot write %03x to register R%d\n", val, reg);
	return -1;
}

static int wm8775_s_routing(struct v4l2_subdev *sd,
			    u32 input, u32 output, u32 config)
{
	struct wm8775_state *state = to_state(sd);

	/* There are 4 inputs and one output. Zero or more inputs
	   are multiplexed together to the output. Hence there are
	   16 combinations.
	   If only one input is active (the normal case) then the
	   input values 1, 2, 4 or 8 should be used. */
	if (input > 15) {
		v4l2_err(sd, "Invalid input %d.\n", input);
		return -EINVAL;
	}
	state->input = input;
	if (!v4l2_ctrl_g_ctrl(state->mute))
		return 0;
	wm8775_write(sd, R21, 0x0c0);
	wm8775_write(sd, R14, 0x1d4);
	wm8775_write(sd, R15, 0x1d4);
	wm8775_write(sd, R21, 0x100 + state->input);
	return 0;
}

static int wm8775_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = to_sd(ctrl);
	struct wm8775_state *state = to_state(sd);

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_MUTE:
		wm8775_write(sd, R21, 0x0c0);
		wm8775_write(sd, R14, 0x1d4);
		wm8775_write(sd, R15, 0x1d4);
		if (!ctrl->val)
			wm8775_write(sd, R21, 0x100 + state->input);
		return 0;
	}
	return -EINVAL;
}

static int wm8775_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_WM8775, 0);
}

static int wm8775_log_status(struct v4l2_subdev *sd)
{
	struct wm8775_state *state = to_state(sd);

	v4l2_info(sd, "Input: %d\n", state->input);
	v4l2_ctrl_handler_log_status(&state->hdl, sd->name);
	return 0;
}

static int wm8775_s_frequency(struct v4l2_subdev *sd, struct v4l2_frequency *freq)
{
	struct wm8775_state *state = to_state(sd);

	/* If I remove this, then it can happen that I have no
	   sound the first time I tune from static to a valid channel.
	   It's difficult to reproduce and is almost certainly related
	   to the zero cross detect circuit. */
	wm8775_write(sd, R21, 0x0c0);
	wm8775_write(sd, R14, 0x1d4);
	wm8775_write(sd, R15, 0x1d4);
	wm8775_write(sd, R21, 0x100 + state->input);
	return 0;
}

/* ----------------------------------------------------------------------- */

static const struct v4l2_ctrl_ops wm8775_ctrl_ops = {
	.s_ctrl = wm8775_s_ctrl,
};

static const struct v4l2_subdev_core_ops wm8775_core_ops = {
	.log_status = wm8775_log_status,
	.g_chip_ident = wm8775_g_chip_ident,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.queryctrl = v4l2_subdev_queryctrl,
	.querymenu = v4l2_subdev_querymenu,
};

static const struct v4l2_subdev_tuner_ops wm8775_tuner_ops = {
	.s_frequency = wm8775_s_frequency,
};

static const struct v4l2_subdev_audio_ops wm8775_audio_ops = {
	.s_routing = wm8775_s_routing,
};

static const struct v4l2_subdev_ops wm8775_ops = {
	.core = &wm8775_core_ops,
	.tuner = &wm8775_tuner_ops,
	.audio = &wm8775_audio_ops,
};

/* ----------------------------------------------------------------------- */

/* i2c implementation */

/*
 * Generic i2c probe
 * concerning the addresses: i2c wants 7 bit (without the r/w bit), so '>>1'
 */

static int wm8775_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct wm8775_state *state;
	struct v4l2_subdev *sd;

	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	v4l_info(client, "chip found @ 0x%02x (%s)\n",
			client->addr << 1, client->adapter->name);

	state = kzalloc(sizeof(struct wm8775_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;
	sd = &state->sd;
	v4l2_i2c_subdev_init(sd, client, &wm8775_ops);
	state->input = 2;

	v4l2_ctrl_handler_init(&state->hdl, 1);
	state->mute = v4l2_ctrl_new_std(&state->hdl, &wm8775_ctrl_ops,
			V4L2_CID_AUDIO_MUTE, 0, 1, 1, 0);
	sd->ctrl_handler = &state->hdl;
	if (state->hdl.error) {
		int err = state->hdl.error;

		v4l2_ctrl_handler_free(&state->hdl);
		kfree(state);
		return err;
	}

	/* Initialize wm8775 */

	/* RESET */
	wm8775_write(sd, R23, 0x000);
	/* Disable zero cross detect timeout */
	wm8775_write(sd, R7, 0x000);
	/* Left justified, 24-bit mode */
	wm8775_write(sd, R11, 0x021);
	/* Master mode, clock ratio 256fs */
	wm8775_write(sd, R12, 0x102);
	/* Powered up */
	wm8775_write(sd, R13, 0x000);
	/* ADC gain +2.5dB, enable zero cross */
	wm8775_write(sd, R14, 0x1d4);
	/* ADC gain +2.5dB, enable zero cross */
	wm8775_write(sd, R15, 0x1d4);
	/* ALC Stereo, ALC target level -1dB FS max gain +8dB */
	wm8775_write(sd, R16, 0x1bf);
	/* Enable gain control, use zero cross detection,
	   ALC hold time 42.6 ms */
	wm8775_write(sd, R17, 0x185);
	/* ALC gain ramp up delay 34 s, ALC gain ramp down delay 33 ms */
	wm8775_write(sd, R18, 0x0a2);
	/* Enable noise gate, threshold -72dBfs */
	wm8775_write(sd, R19, 0x005);
	/* Transient window 4ms, lower PGA gain limit -1dB */
	wm8775_write(sd, R20, 0x07a);
	/* LRBOTH = 1, use input 2. */
	wm8775_write(sd, R21, 0x102);
	return 0;
}

static int wm8775_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct wm8775_state *state = to_state(sd);

	v4l2_device_unregister_subdev(sd);
	v4l2_ctrl_handler_free(&state->hdl);
	kfree(state);
	return 0;
}

static const struct i2c_device_id wm8775_id[] = {
	{ "wm8775", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm8775_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = "wm8775",
	.probe = wm8775_probe,
	.remove = wm8775_remove,
	.id_table = wm8775_id,
};
