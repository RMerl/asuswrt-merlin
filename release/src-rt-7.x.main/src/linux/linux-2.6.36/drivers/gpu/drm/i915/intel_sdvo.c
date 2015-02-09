/*
 * Copyright 2006 Dave Airlie <airlied@linux.ie>
 * Copyright © 2006-2007 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Eric Anholt <eric@anholt.net>
 */
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"
#include "drm_edid.h"
#include "intel_drv.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "intel_sdvo_regs.h"

#define SDVO_TMDS_MASK (SDVO_OUTPUT_TMDS0 | SDVO_OUTPUT_TMDS1)
#define SDVO_RGB_MASK  (SDVO_OUTPUT_RGB0 | SDVO_OUTPUT_RGB1)
#define SDVO_LVDS_MASK (SDVO_OUTPUT_LVDS0 | SDVO_OUTPUT_LVDS1)
#define SDVO_TV_MASK   (SDVO_OUTPUT_CVBS0 | SDVO_OUTPUT_SVID0)

#define SDVO_OUTPUT_MASK (SDVO_TMDS_MASK | SDVO_RGB_MASK | SDVO_LVDS_MASK |\
                         SDVO_TV_MASK)

#define IS_TV(c)	(c->output_flag & SDVO_TV_MASK)
#define IS_LVDS(c)	(c->output_flag & SDVO_LVDS_MASK)
#define IS_TV_OR_LVDS(c) (c->output_flag & (SDVO_TV_MASK | SDVO_LVDS_MASK))


static const char *tv_format_names[] = {
	"NTSC_M"   , "NTSC_J"  , "NTSC_443",
	"PAL_B"    , "PAL_D"   , "PAL_G"   ,
	"PAL_H"    , "PAL_I"   , "PAL_M"   ,
	"PAL_N"    , "PAL_NC"  , "PAL_60"  ,
	"SECAM_B"  , "SECAM_D" , "SECAM_G" ,
	"SECAM_K"  , "SECAM_K1", "SECAM_L" ,
	"SECAM_60"
};

#define TV_FORMAT_NUM  (sizeof(tv_format_names) / sizeof(*tv_format_names))

struct intel_sdvo {
	struct intel_encoder base;

	u8 slave_addr;

	/* Register for the SDVO device: SDVOB or SDVOC */
	int sdvo_reg;

	/* Active outputs controlled by this SDVO output */
	uint16_t controlled_output;

	/*
	 * Capabilities of the SDVO device returned by
	 * i830_sdvo_get_capabilities()
	 */
	struct intel_sdvo_caps caps;

	/* Pixel clock limitations reported by the SDVO device, in kHz */
	int pixel_clock_min, pixel_clock_max;

	/*
	* For multiple function SDVO device,
	* this is for current attached outputs.
	*/
	uint16_t attached_output;

	/**
	 * This is set if we're going to treat the device as TV-out.
	 *
	 * While we have these nice friendly flags for output types that ought
	 * to decide this for us, the S-Video output on our HDMI+S-Video card
	 * shows up as RGB1 (VGA).
	 */
	bool is_tv;

	/* This is for current tv format name */
	int tv_format_index;

	/**
	 * This is set if we treat the device as HDMI, instead of DVI.
	 */
	bool is_hdmi;

	/**
	 * This is set if we detect output of sdvo device as LVDS.
	 */
	bool is_lvds;

	/**
	 * This is sdvo flags for input timing.
	 */
	uint8_t sdvo_flags;

	/**
	 * This is sdvo fixed pannel mode pointer
	 */
	struct drm_display_mode *sdvo_lvds_fixed_mode;

	/*
	 * supported encoding mode, used to determine whether HDMI is
	 * supported
	 */
	struct intel_sdvo_encode encode;

	/* DDC bus used by this SDVO encoder */
	uint8_t ddc_bus;

	/* Mac mini hack -- use the same DDC as the analog connector */
	struct i2c_adapter *analog_ddc_bus;

};

struct intel_sdvo_connector {
	struct intel_connector base;

	/* Mark the type of connector */
	uint16_t output_flag;

	/* This contains all current supported TV format */
	u8 tv_format_supported[TV_FORMAT_NUM];
	int   format_supported_num;
	struct drm_property *tv_format;

	/* add the property for the SDVO-TV */
	struct drm_property *left;
	struct drm_property *right;
	struct drm_property *top;
	struct drm_property *bottom;
	struct drm_property *hpos;
	struct drm_property *vpos;
	struct drm_property *contrast;
	struct drm_property *saturation;
	struct drm_property *hue;
	struct drm_property *sharpness;
	struct drm_property *flicker_filter;
	struct drm_property *flicker_filter_adaptive;
	struct drm_property *flicker_filter_2d;
	struct drm_property *tv_chroma_filter;
	struct drm_property *tv_luma_filter;
	struct drm_property *dot_crawl;

	/* add the property for the SDVO-TV/LVDS */
	struct drm_property *brightness;

	/* Add variable to record current setting for the above property */
	u32	left_margin, right_margin, top_margin, bottom_margin;

	/* this is to get the range of margin.*/
	u32	max_hscan,  max_vscan;
	u32	max_hpos, cur_hpos;
	u32	max_vpos, cur_vpos;
	u32	cur_brightness, max_brightness;
	u32	cur_contrast,	max_contrast;
	u32	cur_saturation, max_saturation;
	u32	cur_hue,	max_hue;
	u32	cur_sharpness,	max_sharpness;
	u32	cur_flicker_filter,		max_flicker_filter;
	u32	cur_flicker_filter_adaptive,	max_flicker_filter_adaptive;
	u32	cur_flicker_filter_2d,		max_flicker_filter_2d;
	u32	cur_tv_chroma_filter,	max_tv_chroma_filter;
	u32	cur_tv_luma_filter,	max_tv_luma_filter;
	u32	cur_dot_crawl,	max_dot_crawl;
};

static struct intel_sdvo *enc_to_intel_sdvo(struct drm_encoder *encoder)
{
	return container_of(enc_to_intel_encoder(encoder), struct intel_sdvo, base);
}

static struct intel_sdvo_connector *to_intel_sdvo_connector(struct drm_connector *connector)
{
	return container_of(to_intel_connector(connector), struct intel_sdvo_connector, base);
}

static bool
intel_sdvo_output_setup(struct intel_sdvo *intel_sdvo, uint16_t flags);
static bool
intel_sdvo_tv_create_property(struct intel_sdvo *intel_sdvo,
			      struct intel_sdvo_connector *intel_sdvo_connector,
			      int type);
static bool
intel_sdvo_create_enhance_property(struct intel_sdvo *intel_sdvo,
				   struct intel_sdvo_connector *intel_sdvo_connector);

static void intel_sdvo_write_sdvox(struct intel_sdvo *intel_sdvo, u32 val)
{
	struct drm_device *dev = intel_sdvo->base.enc.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 bval = val, cval = val;
	int i;

	if (intel_sdvo->sdvo_reg == PCH_SDVOB) {
		I915_WRITE(intel_sdvo->sdvo_reg, val);
		I915_READ(intel_sdvo->sdvo_reg);
		return;
	}

	if (intel_sdvo->sdvo_reg == SDVOB) {
		cval = I915_READ(SDVOC);
	} else {
		bval = I915_READ(SDVOB);
	}
	/*
	 * Write the registers twice for luck. Sometimes,
	 * writing them only once doesn't appear to 'stick'.
	 * The BIOS does this too. Yay, magic
	 */
	for (i = 0; i < 2; i++)
	{
		I915_WRITE(SDVOB, bval);
		I915_READ(SDVOB);
		I915_WRITE(SDVOC, cval);
		I915_READ(SDVOC);
	}
}

static bool intel_sdvo_read_byte(struct intel_sdvo *intel_sdvo, u8 addr, u8 *ch)
{
	u8 out_buf[2] = { addr, 0 };
	u8 buf[2];
	struct i2c_msg msgs[] = {
		{
			.addr = intel_sdvo->slave_addr >> 1,
			.flags = 0,
			.len = 1,
			.buf = out_buf,
		},
		{
			.addr = intel_sdvo->slave_addr >> 1,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = buf,
		}
	};
	int ret;

	if ((ret = i2c_transfer(intel_sdvo->base.i2c_bus, msgs, 2)) == 2)
	{
		*ch = buf[0];
		return true;
	}

	DRM_DEBUG_KMS("i2c transfer returned %d\n", ret);
	return false;
}

static bool intel_sdvo_write_byte(struct intel_sdvo *intel_sdvo, int addr, u8 ch)
{
	u8 out_buf[2] = { addr, ch };
	struct i2c_msg msgs[] = {
		{
			.addr = intel_sdvo->slave_addr >> 1,
			.flags = 0,
			.len = 2,
			.buf = out_buf,
		}
	};

	return i2c_transfer(intel_sdvo->base.i2c_bus, msgs, 1) == 1;
}

#define SDVO_CMD_NAME_ENTRY(cmd) {cmd, #cmd}
/** Mapping of command numbers to names, for debug output */
static const struct _sdvo_cmd_name {
	u8 cmd;
	const char *name;
} sdvo_cmd_names[] = {
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_RESET),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_DEVICE_CAPS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_FIRMWARE_REV),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_TRAINED_INPUTS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_ACTIVE_OUTPUTS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_ACTIVE_OUTPUTS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_IN_OUT_MAP),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_IN_OUT_MAP),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_ATTACHED_DISPLAYS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_HOT_PLUG_SUPPORT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_ACTIVE_HOT_PLUG),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_ACTIVE_HOT_PLUG),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_INTERRUPT_EVENT_SOURCE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_TARGET_INPUT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_TARGET_OUTPUT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_INPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_INPUT_TIMINGS_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_INPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_INPUT_TIMINGS_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_INPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_OUTPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_OUTPUT_TIMINGS_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_OUTPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_OUTPUT_TIMINGS_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_CREATE_PREFERRED_INPUT_TIMING),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_INPUT_PIXEL_CLOCK_RANGE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_OUTPUT_PIXEL_CLOCK_RANGE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_SUPPORTED_CLOCK_RATE_MULTS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_CLOCK_RATE_MULT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_CLOCK_RATE_MULT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_SUPPORTED_TV_FORMATS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_TV_FORMAT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_TV_FORMAT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_SUPPORTED_POWER_STATES),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_POWER_STATE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_ENCODER_POWER_STATE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_DISPLAY_POWER_STATE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_CONTROL_BUS_SWITCH),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_SDTV_RESOLUTION_SUPPORT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_SCALED_HDTV_RESOLUTION_SUPPORT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_SUPPORTED_ENHANCEMENTS),

    /* Add the op code for SDVO enhancements */
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_MAX_HPOS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_HPOS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_HPOS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_MAX_VPOS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_VPOS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_VPOS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_MAX_SATURATION),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_SATURATION),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_SATURATION),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_MAX_HUE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_HUE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_HUE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_MAX_CONTRAST),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_CONTRAST),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_CONTRAST),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_MAX_BRIGHTNESS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_BRIGHTNESS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_BRIGHTNESS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_MAX_OVERSCAN_H),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_OVERSCAN_H),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_OVERSCAN_H),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_MAX_OVERSCAN_V),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_OVERSCAN_V),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_OVERSCAN_V),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_MAX_FLICKER_FILTER),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_FLICKER_FILTER),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_FLICKER_FILTER),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_MAX_FLICKER_FILTER_ADAPTIVE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_FLICKER_FILTER_ADAPTIVE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_FLICKER_FILTER_ADAPTIVE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_MAX_FLICKER_FILTER_2D),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_FLICKER_FILTER_2D),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_FLICKER_FILTER_2D),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_MAX_SHARPNESS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_SHARPNESS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_SHARPNESS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_DOT_CRAWL),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_DOT_CRAWL),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_MAX_TV_CHROMA_FILTER),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_TV_CHROMA_FILTER),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_TV_CHROMA_FILTER),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_MAX_TV_LUMA_FILTER),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_TV_LUMA_FILTER),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_TV_LUMA_FILTER),

    /* HDMI op code */
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_SUPP_ENCODE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_ENCODE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_ENCODE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_PIXEL_REPLI),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_PIXEL_REPLI),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_COLORIMETRY_CAP),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_COLORIMETRY),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_COLORIMETRY),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_AUDIO_ENCRYPT_PREFER),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_AUDIO_STAT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_AUDIO_STAT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_HBUF_INDEX),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_HBUF_INDEX),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_HBUF_INFO),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_HBUF_AV_SPLIT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_HBUF_AV_SPLIT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_HBUF_TXRATE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_HBUF_TXRATE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_HBUF_DATA),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_HBUF_DATA),
};

#define IS_SDVOB(reg)	(reg == SDVOB || reg == PCH_SDVOB)
#define SDVO_NAME(svdo) (IS_SDVOB((svdo)->sdvo_reg) ? "SDVOB" : "SDVOC")

static void intel_sdvo_debug_write(struct intel_sdvo *intel_sdvo, u8 cmd,
				   const void *args, int args_len)
{
	int i;

	DRM_DEBUG_KMS("%s: W: %02X ",
				SDVO_NAME(intel_sdvo), cmd);
	for (i = 0; i < args_len; i++)
		DRM_LOG_KMS("%02X ", ((u8 *)args)[i]);
	for (; i < 8; i++)
		DRM_LOG_KMS("   ");
	for (i = 0; i < ARRAY_SIZE(sdvo_cmd_names); i++) {
		if (cmd == sdvo_cmd_names[i].cmd) {
			DRM_LOG_KMS("(%s)", sdvo_cmd_names[i].name);
			break;
		}
	}
	if (i == ARRAY_SIZE(sdvo_cmd_names))
		DRM_LOG_KMS("(%02X)", cmd);
	DRM_LOG_KMS("\n");
}

static bool intel_sdvo_write_cmd(struct intel_sdvo *intel_sdvo, u8 cmd,
				 const void *args, int args_len)
{
	int i;

	intel_sdvo_debug_write(intel_sdvo, cmd, args, args_len);

	for (i = 0; i < args_len; i++) {
		if (!intel_sdvo_write_byte(intel_sdvo, SDVO_I2C_ARG_0 - i,
					   ((u8*)args)[i]))
			return false;
	}

	return intel_sdvo_write_byte(intel_sdvo, SDVO_I2C_OPCODE, cmd);
}

static const char *cmd_status_names[] = {
	"Power on",
	"Success",
	"Not supported",
	"Invalid arg",
	"Pending",
	"Target not specified",
	"Scaling not supported"
};

static void intel_sdvo_debug_response(struct intel_sdvo *intel_sdvo,
				      void *response, int response_len,
				      u8 status)
{
	int i;

	DRM_DEBUG_KMS("%s: R: ", SDVO_NAME(intel_sdvo));
	for (i = 0; i < response_len; i++)
		DRM_LOG_KMS("%02X ", ((u8 *)response)[i]);
	for (; i < 8; i++)
		DRM_LOG_KMS("   ");
	if (status <= SDVO_CMD_STATUS_SCALING_NOT_SUPP)
		DRM_LOG_KMS("(%s)", cmd_status_names[status]);
	else
		DRM_LOG_KMS("(??? %d)", status);
	DRM_LOG_KMS("\n");
}

static bool intel_sdvo_read_response(struct intel_sdvo *intel_sdvo,
				     void *response, int response_len)
{
	int i;
	u8 status;
	u8 retry = 50;

	while (retry--) {
		/* Read the command response */
		for (i = 0; i < response_len; i++) {
			if (!intel_sdvo_read_byte(intel_sdvo,
						  SDVO_I2C_RETURN_0 + i,
						  &((u8 *)response)[i]))
				return false;
		}

		/* read the return status */
		if (!intel_sdvo_read_byte(intel_sdvo, SDVO_I2C_CMD_STATUS,
					  &status))
			return false;

		intel_sdvo_debug_response(intel_sdvo, response, response_len,
					  status);
		if (status != SDVO_CMD_STATUS_PENDING)
			break;

		mdelay(50);
	}

	return status == SDVO_CMD_STATUS_SUCCESS;
}

static int intel_sdvo_get_pixel_multiplier(struct drm_display_mode *mode)
{
	if (mode->clock >= 100000)
		return 1;
	else if (mode->clock >= 50000)
		return 2;
	else
		return 4;
}

/**
 * Try to read the response after issuie the DDC switch command. But it
 * is noted that we must do the action of reading response and issuing DDC
 * switch command in one I2C transaction. Otherwise when we try to start
 * another I2C transaction after issuing the DDC bus switch, it will be
 * switched to the internal SDVO register.
 */
static void intel_sdvo_set_control_bus_switch(struct intel_sdvo *intel_sdvo,
					      u8 target)
{
	u8 out_buf[2], cmd_buf[2], ret_value[2], ret;
	struct i2c_msg msgs[] = {
		{
			.addr = intel_sdvo->slave_addr >> 1,
			.flags = 0,
			.len = 2,
			.buf = out_buf,
		},
		/* the following two are to read the response */
		{
			.addr = intel_sdvo->slave_addr >> 1,
			.flags = 0,
			.len = 1,
			.buf = cmd_buf,
		},
		{
			.addr = intel_sdvo->slave_addr >> 1,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = ret_value,
		},
	};

	intel_sdvo_debug_write(intel_sdvo, SDVO_CMD_SET_CONTROL_BUS_SWITCH,
					&target, 1);
	/* write the DDC switch command argument */
	intel_sdvo_write_byte(intel_sdvo, SDVO_I2C_ARG_0, target);

	out_buf[0] = SDVO_I2C_OPCODE;
	out_buf[1] = SDVO_CMD_SET_CONTROL_BUS_SWITCH;
	cmd_buf[0] = SDVO_I2C_CMD_STATUS;
	cmd_buf[1] = 0;
	ret_value[0] = 0;
	ret_value[1] = 0;

	ret = i2c_transfer(intel_sdvo->base.i2c_bus, msgs, 3);
	if (ret != 3) {
		/* failure in I2C transfer */
		DRM_DEBUG_KMS("I2c transfer returned %d\n", ret);
		return;
	}
	if (ret_value[0] != SDVO_CMD_STATUS_SUCCESS) {
		DRM_DEBUG_KMS("DDC switch command returns response %d\n",
					ret_value[0]);
		return;
	}
	return;
}

static bool intel_sdvo_set_value(struct intel_sdvo *intel_sdvo, u8 cmd, const void *data, int len)
{
	if (!intel_sdvo_write_cmd(intel_sdvo, cmd, data, len))
		return false;

	return intel_sdvo_read_response(intel_sdvo, NULL, 0);
}

static bool
intel_sdvo_get_value(struct intel_sdvo *intel_sdvo, u8 cmd, void *value, int len)
{
	if (!intel_sdvo_write_cmd(intel_sdvo, cmd, NULL, 0))
		return false;

	return intel_sdvo_read_response(intel_sdvo, value, len);
}

static bool intel_sdvo_set_target_input(struct intel_sdvo *intel_sdvo)
{
	struct intel_sdvo_set_target_input_args targets = {0};
	return intel_sdvo_set_value(intel_sdvo,
				    SDVO_CMD_SET_TARGET_INPUT,
				    &targets, sizeof(targets));
}

/**
 * Return whether each input is trained.
 *
 * This function is making an assumption about the layout of the response,
 * which should be checked against the docs.
 */
static bool intel_sdvo_get_trained_inputs(struct intel_sdvo *intel_sdvo, bool *input_1, bool *input_2)
{
	struct intel_sdvo_get_trained_inputs_response response;

	if (!intel_sdvo_get_value(intel_sdvo, SDVO_CMD_GET_TRAINED_INPUTS,
				  &response, sizeof(response)))
		return false;

	*input_1 = response.input0_trained;
	*input_2 = response.input1_trained;
	return true;
}

static bool intel_sdvo_set_active_outputs(struct intel_sdvo *intel_sdvo,
					  u16 outputs)
{
	return intel_sdvo_set_value(intel_sdvo,
				    SDVO_CMD_SET_ACTIVE_OUTPUTS,
				    &outputs, sizeof(outputs));
}

static bool intel_sdvo_set_encoder_power_state(struct intel_sdvo *intel_sdvo,
					       int mode)
{
	u8 state = SDVO_ENCODER_STATE_ON;

	switch (mode) {
	case DRM_MODE_DPMS_ON:
		state = SDVO_ENCODER_STATE_ON;
		break;
	case DRM_MODE_DPMS_STANDBY:
		state = SDVO_ENCODER_STATE_STANDBY;
		break;
	case DRM_MODE_DPMS_SUSPEND:
		state = SDVO_ENCODER_STATE_SUSPEND;
		break;
	case DRM_MODE_DPMS_OFF:
		state = SDVO_ENCODER_STATE_OFF;
		break;
	}

	return intel_sdvo_set_value(intel_sdvo,
				    SDVO_CMD_SET_ENCODER_POWER_STATE, &state, sizeof(state));
}

static bool intel_sdvo_get_input_pixel_clock_range(struct intel_sdvo *intel_sdvo,
						   int *clock_min,
						   int *clock_max)
{
	struct intel_sdvo_pixel_clock_range clocks;

	if (!intel_sdvo_get_value(intel_sdvo,
				  SDVO_CMD_GET_INPUT_PIXEL_CLOCK_RANGE,
				  &clocks, sizeof(clocks)))
		return false;

	/* Convert the values from units of 10 kHz to kHz. */
	*clock_min = clocks.min * 10;
	*clock_max = clocks.max * 10;
	return true;
}

static bool intel_sdvo_set_target_output(struct intel_sdvo *intel_sdvo,
					 u16 outputs)
{
	return intel_sdvo_set_value(intel_sdvo,
				    SDVO_CMD_SET_TARGET_OUTPUT,
				    &outputs, sizeof(outputs));
}

static bool intel_sdvo_set_timing(struct intel_sdvo *intel_sdvo, u8 cmd,
				  struct intel_sdvo_dtd *dtd)
{
	return intel_sdvo_set_value(intel_sdvo, cmd, &dtd->part1, sizeof(dtd->part1)) &&
		intel_sdvo_set_value(intel_sdvo, cmd + 1, &dtd->part2, sizeof(dtd->part2));
}

static bool intel_sdvo_set_input_timing(struct intel_sdvo *intel_sdvo,
					 struct intel_sdvo_dtd *dtd)
{
	return intel_sdvo_set_timing(intel_sdvo,
				     SDVO_CMD_SET_INPUT_TIMINGS_PART1, dtd);
}

static bool intel_sdvo_set_output_timing(struct intel_sdvo *intel_sdvo,
					 struct intel_sdvo_dtd *dtd)
{
	return intel_sdvo_set_timing(intel_sdvo,
				     SDVO_CMD_SET_OUTPUT_TIMINGS_PART1, dtd);
}

static bool
intel_sdvo_create_preferred_input_timing(struct intel_sdvo *intel_sdvo,
					 uint16_t clock,
					 uint16_t width,
					 uint16_t height)
{
	struct intel_sdvo_preferred_input_timing_args args;

	memset(&args, 0, sizeof(args));
	args.clock = clock;
	args.width = width;
	args.height = height;
	args.interlace = 0;

	if (intel_sdvo->is_lvds &&
	   (intel_sdvo->sdvo_lvds_fixed_mode->hdisplay != width ||
	    intel_sdvo->sdvo_lvds_fixed_mode->vdisplay != height))
		args.scaled = 1;

	return intel_sdvo_set_value(intel_sdvo,
				    SDVO_CMD_CREATE_PREFERRED_INPUT_TIMING,
				    &args, sizeof(args));
}

static bool intel_sdvo_get_preferred_input_timing(struct intel_sdvo *intel_sdvo,
						  struct intel_sdvo_dtd *dtd)
{
	return intel_sdvo_get_value(intel_sdvo, SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART1,
				    &dtd->part1, sizeof(dtd->part1)) &&
		intel_sdvo_get_value(intel_sdvo, SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART2,
				     &dtd->part2, sizeof(dtd->part2));
}

static bool intel_sdvo_set_clock_rate_mult(struct intel_sdvo *intel_sdvo, u8 val)
{
	return intel_sdvo_set_value(intel_sdvo, SDVO_CMD_SET_CLOCK_RATE_MULT, &val, 1);
}

static void intel_sdvo_get_dtd_from_mode(struct intel_sdvo_dtd *dtd,
					 const struct drm_display_mode *mode)
{
	uint16_t width, height;
	uint16_t h_blank_len, h_sync_len, v_blank_len, v_sync_len;
	uint16_t h_sync_offset, v_sync_offset;

	width = mode->crtc_hdisplay;
	height = mode->crtc_vdisplay;

	/* do some mode translations */
	h_blank_len = mode->crtc_hblank_end - mode->crtc_hblank_start;
	h_sync_len = mode->crtc_hsync_end - mode->crtc_hsync_start;

	v_blank_len = mode->crtc_vblank_end - mode->crtc_vblank_start;
	v_sync_len = mode->crtc_vsync_end - mode->crtc_vsync_start;

	h_sync_offset = mode->crtc_hsync_start - mode->crtc_hblank_start;
	v_sync_offset = mode->crtc_vsync_start - mode->crtc_vblank_start;

	dtd->part1.clock = mode->clock / 10;
	dtd->part1.h_active = width & 0xff;
	dtd->part1.h_blank = h_blank_len & 0xff;
	dtd->part1.h_high = (((width >> 8) & 0xf) << 4) |
		((h_blank_len >> 8) & 0xf);
	dtd->part1.v_active = height & 0xff;
	dtd->part1.v_blank = v_blank_len & 0xff;
	dtd->part1.v_high = (((height >> 8) & 0xf) << 4) |
		((v_blank_len >> 8) & 0xf);

	dtd->part2.h_sync_off = h_sync_offset & 0xff;
	dtd->part2.h_sync_width = h_sync_len & 0xff;
	dtd->part2.v_sync_off_width = (v_sync_offset & 0xf) << 4 |
		(v_sync_len & 0xf);
	dtd->part2.sync_off_width_high = ((h_sync_offset & 0x300) >> 2) |
		((h_sync_len & 0x300) >> 4) | ((v_sync_offset & 0x30) >> 2) |
		((v_sync_len & 0x30) >> 4);

	dtd->part2.dtd_flags = 0x18;
	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		dtd->part2.dtd_flags |= 0x2;
	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		dtd->part2.dtd_flags |= 0x4;

	dtd->part2.sdvo_flags = 0;
	dtd->part2.v_sync_off_high = v_sync_offset & 0xc0;
	dtd->part2.reserved = 0;
}

static void intel_sdvo_get_mode_from_dtd(struct drm_display_mode * mode,
					 const struct intel_sdvo_dtd *dtd)
{
	mode->hdisplay = dtd->part1.h_active;
	mode->hdisplay += ((dtd->part1.h_high >> 4) & 0x0f) << 8;
	mode->hsync_start = mode->hdisplay + dtd->part2.h_sync_off;
	mode->hsync_start += (dtd->part2.sync_off_width_high & 0xc0) << 2;
	mode->hsync_end = mode->hsync_start + dtd->part2.h_sync_width;
	mode->hsync_end += (dtd->part2.sync_off_width_high & 0x30) << 4;
	mode->htotal = mode->hdisplay + dtd->part1.h_blank;
	mode->htotal += (dtd->part1.h_high & 0xf) << 8;

	mode->vdisplay = dtd->part1.v_active;
	mode->vdisplay += ((dtd->part1.v_high >> 4) & 0x0f) << 8;
	mode->vsync_start = mode->vdisplay;
	mode->vsync_start += (dtd->part2.v_sync_off_width >> 4) & 0xf;
	mode->vsync_start += (dtd->part2.sync_off_width_high & 0x0c) << 2;
	mode->vsync_start += dtd->part2.v_sync_off_high & 0xc0;
	mode->vsync_end = mode->vsync_start +
		(dtd->part2.v_sync_off_width & 0xf);
	mode->vsync_end += (dtd->part2.sync_off_width_high & 0x3) << 4;
	mode->vtotal = mode->vdisplay + dtd->part1.v_blank;
	mode->vtotal += (dtd->part1.v_high & 0xf) << 8;

	mode->clock = dtd->part1.clock * 10;

	mode->flags &= ~(DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC);
	if (dtd->part2.dtd_flags & 0x2)
		mode->flags |= DRM_MODE_FLAG_PHSYNC;
	if (dtd->part2.dtd_flags & 0x4)
		mode->flags |= DRM_MODE_FLAG_PVSYNC;
}

static bool intel_sdvo_get_supp_encode(struct intel_sdvo *intel_sdvo,
				       struct intel_sdvo_encode *encode)
{
	if (intel_sdvo_get_value(intel_sdvo,
				  SDVO_CMD_GET_SUPP_ENCODE,
				  encode, sizeof(*encode)))
		return true;

	/* non-support means DVI */
	memset(encode, 0, sizeof(*encode));
	return false;
}

static bool intel_sdvo_set_encode(struct intel_sdvo *intel_sdvo,
				  uint8_t mode)
{
	return intel_sdvo_set_value(intel_sdvo, SDVO_CMD_SET_ENCODE, &mode, 1);
}

static bool intel_sdvo_set_colorimetry(struct intel_sdvo *intel_sdvo,
				       uint8_t mode)
{
	return intel_sdvo_set_value(intel_sdvo, SDVO_CMD_SET_COLORIMETRY, &mode, 1);
}


static bool intel_sdvo_set_hdmi_buf(struct intel_sdvo *intel_sdvo,
				    int index,
				    uint8_t *data, int8_t size, uint8_t tx_rate)
{
    uint8_t set_buf_index[2];

    set_buf_index[0] = index;
    set_buf_index[1] = 0;

    if (!intel_sdvo_write_cmd(intel_sdvo, SDVO_CMD_SET_HBUF_INDEX,
			      set_buf_index, 2))
	    return false;

    for (; size > 0; size -= 8) {
	if (!intel_sdvo_write_cmd(intel_sdvo, SDVO_CMD_SET_HBUF_DATA, data, 8))
		return false;

	data += 8;
    }

    return intel_sdvo_write_cmd(intel_sdvo, SDVO_CMD_SET_HBUF_TXRATE, &tx_rate, 1);
}

static uint8_t intel_sdvo_calc_hbuf_csum(uint8_t *data, uint8_t size)
{
	uint8_t csum = 0;
	int i;

	for (i = 0; i < size; i++)
		csum += data[i];

	return 0x100 - csum;
}

#define DIP_TYPE_AVI	0x82
#define DIP_VERSION_AVI	0x2
#define DIP_LEN_AVI	13

struct dip_infoframe {
	uint8_t type;
	uint8_t version;
	uint8_t len;
	uint8_t checksum;
	union {
		struct {
			/* Packet Byte #1 */
			uint8_t S:2;
			uint8_t B:2;
			uint8_t A:1;
			uint8_t Y:2;
			uint8_t rsvd1:1;
			/* Packet Byte #2 */
			uint8_t R:4;
			uint8_t M:2;
			uint8_t C:2;
			/* Packet Byte #3 */
			uint8_t SC:2;
			uint8_t Q:2;
			uint8_t EC:3;
			uint8_t ITC:1;
			/* Packet Byte #4 */
			uint8_t VIC:7;
			uint8_t rsvd2:1;
			/* Packet Byte #5 */
			uint8_t PR:4;
			uint8_t rsvd3:4;
			/* Packet Byte #6~13 */
			uint16_t top_bar_end;
			uint16_t bottom_bar_start;
			uint16_t left_bar_end;
			uint16_t right_bar_start;
		} avi;
		struct {
			/* Packet Byte #1 */
			uint8_t channel_count:3;
			uint8_t rsvd1:1;
			uint8_t coding_type:4;
			/* Packet Byte #2 */
			uint8_t sample_size:2; /* SS0, SS1 */
			uint8_t sample_frequency:3;
			uint8_t rsvd2:3;
			/* Packet Byte #3 */
			uint8_t coding_type_private:5;
			uint8_t rsvd3:3;
			/* Packet Byte #4 */
			uint8_t channel_allocation;
			/* Packet Byte #5 */
			uint8_t rsvd4:3;
			uint8_t level_shift:4;
			uint8_t downmix_inhibit:1;
		} audio;
		uint8_t payload[28];
	} __attribute__ ((packed)) u;
} __attribute__((packed));

static bool intel_sdvo_set_avi_infoframe(struct intel_sdvo *intel_sdvo,
					 struct drm_display_mode * mode)
{
	struct dip_infoframe avi_if = {
		.type = DIP_TYPE_AVI,
		.version = DIP_VERSION_AVI,
		.len = DIP_LEN_AVI,
	};

	avi_if.checksum = intel_sdvo_calc_hbuf_csum((uint8_t *)&avi_if,
						    4 + avi_if.len);
	return intel_sdvo_set_hdmi_buf(intel_sdvo, 1, (uint8_t *)&avi_if,
				       4 + avi_if.len,
				       SDVO_HBUF_TX_VSYNC);
}

static bool intel_sdvo_set_tv_format(struct intel_sdvo *intel_sdvo)
{
	struct intel_sdvo_tv_format format;
	uint32_t format_map;

	format_map = 1 << intel_sdvo->tv_format_index;
	memset(&format, 0, sizeof(format));
	memcpy(&format, &format_map, min(sizeof(format), sizeof(format_map)));

	BUILD_BUG_ON(sizeof(format) != 6);
	return intel_sdvo_set_value(intel_sdvo,
				    SDVO_CMD_SET_TV_FORMAT,
				    &format, sizeof(format));
}

static bool
intel_sdvo_set_output_timings_from_mode(struct intel_sdvo *intel_sdvo,
					struct drm_display_mode *mode)
{
	struct intel_sdvo_dtd output_dtd;

	if (!intel_sdvo_set_target_output(intel_sdvo,
					  intel_sdvo->attached_output))
		return false;

	intel_sdvo_get_dtd_from_mode(&output_dtd, mode);
	if (!intel_sdvo_set_output_timing(intel_sdvo, &output_dtd))
		return false;

	return true;
}

static bool
intel_sdvo_set_input_timings_for_mode(struct intel_sdvo *intel_sdvo,
					struct drm_display_mode *mode,
					struct drm_display_mode *adjusted_mode)
{
	struct intel_sdvo_dtd input_dtd;

	/* Reset the input timing to the screen. Assume always input 0. */
	if (!intel_sdvo_set_target_input(intel_sdvo))
		return false;

	if (!intel_sdvo_create_preferred_input_timing(intel_sdvo,
						      mode->clock / 10,
						      mode->hdisplay,
						      mode->vdisplay))
		return false;

	if (!intel_sdvo_get_preferred_input_timing(intel_sdvo,
						   &input_dtd))
		return false;

	intel_sdvo_get_mode_from_dtd(adjusted_mode, &input_dtd);
	intel_sdvo->sdvo_flags = input_dtd.part2.sdvo_flags;

	drm_mode_set_crtcinfo(adjusted_mode, 0);
	mode->clock = adjusted_mode->clock;
	return true;
}

static bool intel_sdvo_mode_fixup(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	struct intel_sdvo *intel_sdvo = enc_to_intel_sdvo(encoder);

	/* We need to construct preferred input timings based on our
	 * output timings.  To do that, we have to set the output
	 * timings, even though this isn't really the right place in
	 * the sequence to do it. Oh well.
	 */
	if (intel_sdvo->is_tv) {
		if (!intel_sdvo_set_output_timings_from_mode(intel_sdvo, mode))
			return false;

		(void) intel_sdvo_set_input_timings_for_mode(intel_sdvo,
							     mode,
							     adjusted_mode);
	} else if (intel_sdvo->is_lvds) {
		drm_mode_set_crtcinfo(intel_sdvo->sdvo_lvds_fixed_mode, 0);

		if (!intel_sdvo_set_output_timings_from_mode(intel_sdvo,
							    intel_sdvo->sdvo_lvds_fixed_mode))
			return false;

		(void) intel_sdvo_set_input_timings_for_mode(intel_sdvo,
							     mode,
							     adjusted_mode);
	}

	/* Make the CRTC code factor in the SDVO pixel multiplier.  The
	 * SDVO device will be told of the multiplier during mode_set.
	 */
	adjusted_mode->clock *= intel_sdvo_get_pixel_multiplier(mode);

	return true;
}

static void intel_sdvo_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_crtc *crtc = encoder->crtc;
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
	struct intel_sdvo *intel_sdvo = enc_to_intel_sdvo(encoder);
	u32 sdvox = 0;
	int sdvo_pixel_multiply, rate;
	struct intel_sdvo_in_out_map in_out;
	struct intel_sdvo_dtd input_dtd;

	if (!mode)
		return;

	/* First, set the input mapping for the first input to our controlled
	 * output. This is only correct if we're a single-input device, in
	 * which case the first input is the output from the appropriate SDVO
	 * channel on the motherboard.  In a two-input device, the first input
	 * will be SDVOB and the second SDVOC.
	 */
	in_out.in0 = intel_sdvo->attached_output;
	in_out.in1 = 0;

	intel_sdvo_set_value(intel_sdvo,
			     SDVO_CMD_SET_IN_OUT_MAP,
			     &in_out, sizeof(in_out));

	if (intel_sdvo->is_hdmi) {
		if (!intel_sdvo_set_avi_infoframe(intel_sdvo, mode))
			return;

		sdvox |= SDVO_AUDIO_ENABLE;
	}

	/* We have tried to get input timing in mode_fixup, and filled into
	   adjusted_mode */
	intel_sdvo_get_dtd_from_mode(&input_dtd, adjusted_mode);
	if (intel_sdvo->is_tv || intel_sdvo->is_lvds)
		input_dtd.part2.sdvo_flags = intel_sdvo->sdvo_flags;

	/* If it's a TV, we already set the output timing in mode_fixup.
	 * Otherwise, the output timing is equal to the input timing.
	 */
	if (!intel_sdvo->is_tv && !intel_sdvo->is_lvds) {
		/* Set the output timing to the screen */
		if (!intel_sdvo_set_target_output(intel_sdvo,
						  intel_sdvo->attached_output))
			return;

		(void) intel_sdvo_set_output_timing(intel_sdvo, &input_dtd);
	}

	/* Set the input timing to the screen. Assume always input 0. */
	if (!intel_sdvo_set_target_input(intel_sdvo))
		return;

	if (intel_sdvo->is_tv) {
		if (!intel_sdvo_set_tv_format(intel_sdvo))
			return;
	}

	/* We would like to use intel_sdvo_create_preferred_input_timing() to
	 * provide the device with a timing it can support, if it supports that
	 * feature.  However, presumably we would need to adjust the CRTC to
	 * output the preferred timing, and we don't support that currently.
	 */
	(void) intel_sdvo_set_input_timing(intel_sdvo, &input_dtd);

	sdvo_pixel_multiply = intel_sdvo_get_pixel_multiplier(mode);
	switch (sdvo_pixel_multiply) {
	case 1: rate = SDVO_CLOCK_RATE_MULT_1X; break;
	case 2: rate = SDVO_CLOCK_RATE_MULT_2X; break;
	case 4: rate = SDVO_CLOCK_RATE_MULT_4X; break;
	}
	if (!intel_sdvo_set_clock_rate_mult(intel_sdvo, rate))
		return;

	/* Set the SDVO control regs. */
	if (IS_I965G(dev)) {
		sdvox |= SDVO_BORDER_ENABLE;
		if (adjusted_mode->flags & DRM_MODE_FLAG_PVSYNC)
			sdvox |= SDVO_VSYNC_ACTIVE_HIGH;
		if (adjusted_mode->flags & DRM_MODE_FLAG_PHSYNC)
			sdvox |= SDVO_HSYNC_ACTIVE_HIGH;
	} else {
		sdvox |= I915_READ(intel_sdvo->sdvo_reg);
		switch (intel_sdvo->sdvo_reg) {
		case SDVOB:
			sdvox &= SDVOB_PRESERVE_MASK;
			break;
		case SDVOC:
			sdvox &= SDVOC_PRESERVE_MASK;
			break;
		}
		sdvox |= (9 << 19) | SDVO_BORDER_ENABLE;
	}
	if (intel_crtc->pipe == 1)
		sdvox |= SDVO_PIPE_B_SELECT;

	if (IS_I965G(dev)) {
		/* done in crtc_mode_set as the dpll_md reg must be written early */
	} else if (IS_I945G(dev) || IS_I945GM(dev) || IS_G33(dev)) {
		/* done in crtc_mode_set as it lives inside the dpll register */
	} else {
		sdvox |= (sdvo_pixel_multiply - 1) << SDVO_PORT_MULTIPLY_SHIFT;
	}

	if (intel_sdvo->sdvo_flags & SDVO_NEED_TO_STALL)
		sdvox |= SDVO_STALL_SELECT;
	intel_sdvo_write_sdvox(intel_sdvo, sdvox);
}

static void intel_sdvo_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_sdvo *intel_sdvo = enc_to_intel_sdvo(encoder);
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->crtc);
	u32 temp;

	if (mode != DRM_MODE_DPMS_ON) {
		intel_sdvo_set_active_outputs(intel_sdvo, 0);
		if (0)
			intel_sdvo_set_encoder_power_state(intel_sdvo, mode);

		if (mode == DRM_MODE_DPMS_OFF) {
			temp = I915_READ(intel_sdvo->sdvo_reg);
			if ((temp & SDVO_ENABLE) != 0) {
				intel_sdvo_write_sdvox(intel_sdvo, temp & ~SDVO_ENABLE);
			}
		}
	} else {
		bool input1, input2;
		int i;
		u8 status;

		temp = I915_READ(intel_sdvo->sdvo_reg);
		if ((temp & SDVO_ENABLE) == 0)
			intel_sdvo_write_sdvox(intel_sdvo, temp | SDVO_ENABLE);
		for (i = 0; i < 2; i++)
			intel_wait_for_vblank(dev, intel_crtc->pipe);

		status = intel_sdvo_get_trained_inputs(intel_sdvo, &input1, &input2);
		/* Warn if the device reported failure to sync.
		 * A lot of SDVO devices fail to notify of sync, but it's
		 * a given it the status is a success, we succeeded.
		 */
		if (status == SDVO_CMD_STATUS_SUCCESS && !input1) {
			DRM_DEBUG_KMS("First %s output reported failure to "
					"sync\n", SDVO_NAME(intel_sdvo));
		}

		if (0)
			intel_sdvo_set_encoder_power_state(intel_sdvo, mode);
		intel_sdvo_set_active_outputs(intel_sdvo, intel_sdvo->attached_output);
	}
	return;
}

static int intel_sdvo_mode_valid(struct drm_connector *connector,
				 struct drm_display_mode *mode)
{
	struct drm_encoder *encoder = intel_attached_encoder(connector);
	struct intel_sdvo *intel_sdvo = enc_to_intel_sdvo(encoder);

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		return MODE_NO_DBLESCAN;

	if (intel_sdvo->pixel_clock_min > mode->clock)
		return MODE_CLOCK_LOW;

	if (intel_sdvo->pixel_clock_max < mode->clock)
		return MODE_CLOCK_HIGH;

	if (intel_sdvo->is_lvds) {
		if (mode->hdisplay > intel_sdvo->sdvo_lvds_fixed_mode->hdisplay)
			return MODE_PANEL;

		if (mode->vdisplay > intel_sdvo->sdvo_lvds_fixed_mode->vdisplay)
			return MODE_PANEL;
	}

	return MODE_OK;
}

static bool intel_sdvo_get_capabilities(struct intel_sdvo *intel_sdvo, struct intel_sdvo_caps *caps)
{
	return intel_sdvo_get_value(intel_sdvo, SDVO_CMD_GET_DEVICE_CAPS, caps, sizeof(*caps));
}

/* No use! */

static bool
intel_sdvo_multifunc_encoder(struct intel_sdvo *intel_sdvo)
{
	int caps = 0;

	if (intel_sdvo->caps.output_flags &
		(SDVO_OUTPUT_TMDS0 | SDVO_OUTPUT_TMDS1))
		caps++;
	if (intel_sdvo->caps.output_flags &
		(SDVO_OUTPUT_RGB0 | SDVO_OUTPUT_RGB1))
		caps++;
	if (intel_sdvo->caps.output_flags &
		(SDVO_OUTPUT_SVID0 | SDVO_OUTPUT_SVID1))
		caps++;
	if (intel_sdvo->caps.output_flags &
		(SDVO_OUTPUT_CVBS0 | SDVO_OUTPUT_CVBS1))
		caps++;
	if (intel_sdvo->caps.output_flags &
		(SDVO_OUTPUT_YPRPB0 | SDVO_OUTPUT_YPRPB1))
		caps++;

	if (intel_sdvo->caps.output_flags &
		(SDVO_OUTPUT_SCART0 | SDVO_OUTPUT_SCART1))
		caps++;

	if (intel_sdvo->caps.output_flags &
		(SDVO_OUTPUT_LVDS0 | SDVO_OUTPUT_LVDS1))
		caps++;

	return (caps > 1);
}

static struct drm_connector *
intel_find_analog_connector(struct drm_device *dev)
{
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct intel_sdvo *intel_sdvo;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		intel_sdvo = enc_to_intel_sdvo(encoder);
		if (intel_sdvo->base.type == INTEL_OUTPUT_ANALOG) {
			list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
				if (encoder == intel_attached_encoder(connector))
					return connector;
			}
		}
	}
	return NULL;
}

static int
intel_analog_is_connected(struct drm_device *dev)
{
	struct drm_connector *analog_connector;

	analog_connector = intel_find_analog_connector(dev);
	if (!analog_connector)
		return false;

	if (analog_connector->funcs->detect(analog_connector, false) ==
			connector_status_disconnected)
		return false;

	return true;
}

enum drm_connector_status
intel_sdvo_hdmi_sink_detect(struct drm_connector *connector)
{
	struct drm_encoder *encoder = intel_attached_encoder(connector);
	struct intel_sdvo *intel_sdvo = enc_to_intel_sdvo(encoder);
	struct intel_sdvo_connector *intel_sdvo_connector = to_intel_sdvo_connector(connector);
	enum drm_connector_status status = connector_status_connected;
	struct edid *edid = NULL;

	edid = drm_get_edid(connector, intel_sdvo->base.ddc_bus);

	/* This is only applied to SDVO cards with multiple outputs */
	if (edid == NULL && intel_sdvo_multifunc_encoder(intel_sdvo)) {
		uint8_t saved_ddc, temp_ddc;
		saved_ddc = intel_sdvo->ddc_bus;
		temp_ddc = intel_sdvo->ddc_bus >> 1;
		/*
		 * Don't use the 1 as the argument of DDC bus switch to get
		 * the EDID. It is used for SDVO SPD ROM.
		 */
		while(temp_ddc > 1) {
			intel_sdvo->ddc_bus = temp_ddc;
			edid = drm_get_edid(connector, intel_sdvo->base.ddc_bus);
			if (edid) {
				/*
				 * When we can get the EDID, maybe it is the
				 * correct DDC bus. Update it.
				 */
				intel_sdvo->ddc_bus = temp_ddc;
				break;
			}
			temp_ddc >>= 1;
		}
		if (edid == NULL)
			intel_sdvo->ddc_bus = saved_ddc;
	}
	/* when there is no edid and no monitor is connected with VGA
	 * port, try to use the CRT ddc to read the EDID for DVI-connector
	 */
	if (edid == NULL && intel_sdvo->analog_ddc_bus &&
	    !intel_analog_is_connected(connector->dev))
		edid = drm_get_edid(connector, intel_sdvo->analog_ddc_bus);

	if (edid != NULL) {
		bool is_digital = !!(edid->input & DRM_EDID_INPUT_DIGITAL);
		bool need_digital = !!(intel_sdvo_connector->output_flag & SDVO_TMDS_MASK);

		/* DDC bus is shared, match EDID to connector type */
		if (is_digital && need_digital)
			intel_sdvo->is_hdmi = drm_detect_hdmi_monitor(edid);
		else if (is_digital != need_digital)
			status = connector_status_disconnected;

		connector->display_info.raw_edid = NULL;
	} else
		status = connector_status_disconnected;
	
	kfree(edid);

	return status;
}

static enum drm_connector_status
intel_sdvo_detect(struct drm_connector *connector, bool force)
{
	uint16_t response;
	struct drm_encoder *encoder = intel_attached_encoder(connector);
	struct intel_sdvo *intel_sdvo = enc_to_intel_sdvo(encoder);
	struct intel_sdvo_connector *intel_sdvo_connector = to_intel_sdvo_connector(connector);
	enum drm_connector_status ret;

	if (!intel_sdvo_write_cmd(intel_sdvo,
			     SDVO_CMD_GET_ATTACHED_DISPLAYS, NULL, 0))
		return connector_status_unknown;

	/* add 30ms delay when the output type might be TV */
	if (intel_sdvo->caps.output_flags &
	    (SDVO_OUTPUT_SVID0 | SDVO_OUTPUT_CVBS0))
		mdelay(30);

	if (!intel_sdvo_read_response(intel_sdvo, &response, 2))
		return connector_status_unknown;

	DRM_DEBUG_KMS("SDVO response %d %d\n", response & 0xff, response >> 8);

	if (response == 0)
		return connector_status_disconnected;

	intel_sdvo->attached_output = response;

	if ((intel_sdvo_connector->output_flag & response) == 0)
		ret = connector_status_disconnected;
	else if (response & SDVO_TMDS_MASK)
		ret = intel_sdvo_hdmi_sink_detect(connector);
	else
		ret = connector_status_connected;

	/* May update encoder flag for like clock for SDVO TV, etc.*/
	if (ret == connector_status_connected) {
		intel_sdvo->is_tv = false;
		intel_sdvo->is_lvds = false;
		intel_sdvo->base.needs_tv_clock = false;

		if (response & SDVO_TV_MASK) {
			intel_sdvo->is_tv = true;
			intel_sdvo->base.needs_tv_clock = true;
		}
		if (response & SDVO_LVDS_MASK)
			intel_sdvo->is_lvds = intel_sdvo->sdvo_lvds_fixed_mode != NULL;
	}

	return ret;
}

static void intel_sdvo_get_ddc_modes(struct drm_connector *connector)
{
	struct drm_encoder *encoder = intel_attached_encoder(connector);
	struct intel_sdvo *intel_sdvo = enc_to_intel_sdvo(encoder);
	int num_modes;

	/* set the bus switch and get the modes */
	num_modes = intel_ddc_get_modes(connector, intel_sdvo->base.ddc_bus);

	/*
	 * Mac mini hack.  On this device, the DVI-I connector shares one DDC
	 * link between analog and digital outputs. So, if the regular SDVO
	 * DDC fails, check to see if the analog output is disconnected, in
	 * which case we'll look there for the digital DDC data.
	 */
	if (num_modes == 0 &&
	    intel_sdvo->analog_ddc_bus &&
	    !intel_analog_is_connected(connector->dev)) {
		/* Switch to the analog ddc bus and try that
		 */
		(void) intel_ddc_get_modes(connector, intel_sdvo->analog_ddc_bus);
	}
}

struct drm_display_mode sdvo_tv_modes[] = {
	{ DRM_MODE("320x200", DRM_MODE_TYPE_DRIVER, 5815, 320, 321, 384,
		   416, 0, 200, 201, 232, 233, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("320x240", DRM_MODE_TYPE_DRIVER, 6814, 320, 321, 384,
		   416, 0, 240, 241, 272, 273, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("400x300", DRM_MODE_TYPE_DRIVER, 9910, 400, 401, 464,
		   496, 0, 300, 301, 332, 333, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("640x350", DRM_MODE_TYPE_DRIVER, 16913, 640, 641, 704,
		   736, 0, 350, 351, 382, 383, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("640x400", DRM_MODE_TYPE_DRIVER, 19121, 640, 641, 704,
		   736, 0, 400, 401, 432, 433, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 22654, 640, 641, 704,
		   736, 0, 480, 481, 512, 513, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("704x480", DRM_MODE_TYPE_DRIVER, 24624, 704, 705, 768,
		   800, 0, 480, 481, 512, 513, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("704x576", DRM_MODE_TYPE_DRIVER, 29232, 704, 705, 768,
		   800, 0, 576, 577, 608, 609, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("720x350", DRM_MODE_TYPE_DRIVER, 18751, 720, 721, 784,
		   816, 0, 350, 351, 382, 383, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("720x400", DRM_MODE_TYPE_DRIVER, 21199, 720, 721, 784,
		   816, 0, 400, 401, 432, 433, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 25116, 720, 721, 784,
		   816, 0, 480, 481, 512, 513, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("720x540", DRM_MODE_TYPE_DRIVER, 28054, 720, 721, 784,
		   816, 0, 540, 541, 572, 573, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 29816, 720, 721, 784,
		   816, 0, 576, 577, 608, 609, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("768x576", DRM_MODE_TYPE_DRIVER, 31570, 768, 769, 832,
		   864, 0, 576, 577, 608, 609, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("800x600", DRM_MODE_TYPE_DRIVER, 34030, 800, 801, 864,
		   896, 0, 600, 601, 632, 633, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("832x624", DRM_MODE_TYPE_DRIVER, 36581, 832, 833, 896,
		   928, 0, 624, 625, 656, 657, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("920x766", DRM_MODE_TYPE_DRIVER, 48707, 920, 921, 984,
		   1016, 0, 766, 767, 798, 799, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("1024x768", DRM_MODE_TYPE_DRIVER, 53827, 1024, 1025, 1088,
		   1120, 0, 768, 769, 800, 801, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	{ DRM_MODE("1280x1024", DRM_MODE_TYPE_DRIVER, 87265, 1280, 1281, 1344,
		   1376, 0, 1024, 1025, 1056, 1057, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
};

static void intel_sdvo_get_tv_modes(struct drm_connector *connector)
{
	struct drm_encoder *encoder = intel_attached_encoder(connector);
	struct intel_sdvo *intel_sdvo = enc_to_intel_sdvo(encoder);
	struct intel_sdvo_sdtv_resolution_request tv_res;
	uint32_t reply = 0, format_map = 0;
	int i;

	/* Read the list of supported input resolutions for the selected TV
	 * format.
	 */
	format_map = 1 << intel_sdvo->tv_format_index;
	memcpy(&tv_res, &format_map,
	       min(sizeof(format_map), sizeof(struct intel_sdvo_sdtv_resolution_request)));

	if (!intel_sdvo_set_target_output(intel_sdvo, intel_sdvo->attached_output))
		return;

	BUILD_BUG_ON(sizeof(tv_res) != 3);
	if (!intel_sdvo_write_cmd(intel_sdvo, SDVO_CMD_GET_SDTV_RESOLUTION_SUPPORT,
				  &tv_res, sizeof(tv_res)))
		return;
	if (!intel_sdvo_read_response(intel_sdvo, &reply, 3))
		return;

	for (i = 0; i < ARRAY_SIZE(sdvo_tv_modes); i++)
		if (reply & (1 << i)) {
			struct drm_display_mode *nmode;
			nmode = drm_mode_duplicate(connector->dev,
						   &sdvo_tv_modes[i]);
			if (nmode)
				drm_mode_probed_add(connector, nmode);
		}
}

static void intel_sdvo_get_lvds_modes(struct drm_connector *connector)
{
	struct drm_encoder *encoder = intel_attached_encoder(connector);
	struct intel_sdvo *intel_sdvo = enc_to_intel_sdvo(encoder);
	struct drm_i915_private *dev_priv = connector->dev->dev_private;
	struct drm_display_mode *newmode;

	/*
	 * Attempt to get the mode list from DDC.
	 * Assume that the preferred modes are
	 * arranged in priority order.
	 */
	intel_ddc_get_modes(connector, intel_sdvo->base.ddc_bus);
	if (list_empty(&connector->probed_modes) == false)
		goto end;

	/* Fetch modes from VBT */
	if (dev_priv->sdvo_lvds_vbt_mode != NULL) {
		newmode = drm_mode_duplicate(connector->dev,
					     dev_priv->sdvo_lvds_vbt_mode);
		if (newmode != NULL) {
			/* Guarantee the mode is preferred */
			newmode->type = (DRM_MODE_TYPE_PREFERRED |
					 DRM_MODE_TYPE_DRIVER);
			drm_mode_probed_add(connector, newmode);
		}
	}

end:
	list_for_each_entry(newmode, &connector->probed_modes, head) {
		if (newmode->type & DRM_MODE_TYPE_PREFERRED) {
			intel_sdvo->sdvo_lvds_fixed_mode =
				drm_mode_duplicate(connector->dev, newmode);
			intel_sdvo->is_lvds = true;
			break;
		}
	}

}

static int intel_sdvo_get_modes(struct drm_connector *connector)
{
	struct intel_sdvo_connector *intel_sdvo_connector = to_intel_sdvo_connector(connector);

	if (IS_TV(intel_sdvo_connector))
		intel_sdvo_get_tv_modes(connector);
	else if (IS_LVDS(intel_sdvo_connector))
		intel_sdvo_get_lvds_modes(connector);
	else
		intel_sdvo_get_ddc_modes(connector);

	return !list_empty(&connector->probed_modes);
}

static void
intel_sdvo_destroy_enhance_property(struct drm_connector *connector)
{
	struct intel_sdvo_connector *intel_sdvo_connector = to_intel_sdvo_connector(connector);
	struct drm_device *dev = connector->dev;

	if (intel_sdvo_connector->left)
		drm_property_destroy(dev, intel_sdvo_connector->left);
	if (intel_sdvo_connector->right)
		drm_property_destroy(dev, intel_sdvo_connector->right);
	if (intel_sdvo_connector->top)
		drm_property_destroy(dev, intel_sdvo_connector->top);
	if (intel_sdvo_connector->bottom)
		drm_property_destroy(dev, intel_sdvo_connector->bottom);
	if (intel_sdvo_connector->hpos)
		drm_property_destroy(dev, intel_sdvo_connector->hpos);
	if (intel_sdvo_connector->vpos)
		drm_property_destroy(dev, intel_sdvo_connector->vpos);
	if (intel_sdvo_connector->saturation)
		drm_property_destroy(dev, intel_sdvo_connector->saturation);
	if (intel_sdvo_connector->contrast)
		drm_property_destroy(dev, intel_sdvo_connector->contrast);
	if (intel_sdvo_connector->hue)
		drm_property_destroy(dev, intel_sdvo_connector->hue);
	if (intel_sdvo_connector->sharpness)
		drm_property_destroy(dev, intel_sdvo_connector->sharpness);
	if (intel_sdvo_connector->flicker_filter)
		drm_property_destroy(dev, intel_sdvo_connector->flicker_filter);
	if (intel_sdvo_connector->flicker_filter_2d)
		drm_property_destroy(dev, intel_sdvo_connector->flicker_filter_2d);
	if (intel_sdvo_connector->flicker_filter_adaptive)
		drm_property_destroy(dev, intel_sdvo_connector->flicker_filter_adaptive);
	if (intel_sdvo_connector->tv_luma_filter)
		drm_property_destroy(dev, intel_sdvo_connector->tv_luma_filter);
	if (intel_sdvo_connector->tv_chroma_filter)
		drm_property_destroy(dev, intel_sdvo_connector->tv_chroma_filter);
	if (intel_sdvo_connector->dot_crawl)
		drm_property_destroy(dev, intel_sdvo_connector->dot_crawl);
	if (intel_sdvo_connector->brightness)
		drm_property_destroy(dev, intel_sdvo_connector->brightness);
}

static void intel_sdvo_destroy(struct drm_connector *connector)
{
	struct intel_sdvo_connector *intel_sdvo_connector = to_intel_sdvo_connector(connector);

	if (intel_sdvo_connector->tv_format)
		drm_property_destroy(connector->dev,
				     intel_sdvo_connector->tv_format);

	intel_sdvo_destroy_enhance_property(connector);
	drm_sysfs_connector_remove(connector);
	drm_connector_cleanup(connector);
	kfree(connector);
}

static int
intel_sdvo_set_property(struct drm_connector *connector,
			struct drm_property *property,
			uint64_t val)
{
	struct drm_encoder *encoder = intel_attached_encoder(connector);
	struct intel_sdvo *intel_sdvo = enc_to_intel_sdvo(encoder);
	struct intel_sdvo_connector *intel_sdvo_connector = to_intel_sdvo_connector(connector);
	uint16_t temp_value;
	uint8_t cmd;
	int ret;

	ret = drm_connector_property_set_value(connector, property, val);
	if (ret)
		return ret;

#define CHECK_PROPERTY(name, NAME) \
	if (intel_sdvo_connector->name == property) { \
		if (intel_sdvo_connector->cur_##name == temp_value) return 0; \
		if (intel_sdvo_connector->max_##name < temp_value) return -EINVAL; \
		cmd = SDVO_CMD_SET_##NAME; \
		intel_sdvo_connector->cur_##name = temp_value; \
		goto set_value; \
	}

	if (property == intel_sdvo_connector->tv_format) {
		if (val >= TV_FORMAT_NUM)
			return -EINVAL;

		if (intel_sdvo->tv_format_index ==
		    intel_sdvo_connector->tv_format_supported[val])
			return 0;

		intel_sdvo->tv_format_index = intel_sdvo_connector->tv_format_supported[val];
		goto done;
	} else if (IS_TV_OR_LVDS(intel_sdvo_connector)) {
		temp_value = val;
		if (intel_sdvo_connector->left == property) {
			drm_connector_property_set_value(connector,
							 intel_sdvo_connector->right, val);
			if (intel_sdvo_connector->left_margin == temp_value)
				return 0;

			intel_sdvo_connector->left_margin = temp_value;
			intel_sdvo_connector->right_margin = temp_value;
			temp_value = intel_sdvo_connector->max_hscan -
				intel_sdvo_connector->left_margin;
			cmd = SDVO_CMD_SET_OVERSCAN_H;
			goto set_value;
		} else if (intel_sdvo_connector->right == property) {
			drm_connector_property_set_value(connector,
							 intel_sdvo_connector->left, val);
			if (intel_sdvo_connector->right_margin == temp_value)
				return 0;

			intel_sdvo_connector->left_margin = temp_value;
			intel_sdvo_connector->right_margin = temp_value;
			temp_value = intel_sdvo_connector->max_hscan -
				intel_sdvo_connector->left_margin;
			cmd = SDVO_CMD_SET_OVERSCAN_H;
			goto set_value;
		} else if (intel_sdvo_connector->top == property) {
			drm_connector_property_set_value(connector,
							 intel_sdvo_connector->bottom, val);
			if (intel_sdvo_connector->top_margin == temp_value)
				return 0;

			intel_sdvo_connector->top_margin = temp_value;
			intel_sdvo_connector->bottom_margin = temp_value;
			temp_value = intel_sdvo_connector->max_vscan -
				intel_sdvo_connector->top_margin;
			cmd = SDVO_CMD_SET_OVERSCAN_V;
			goto set_value;
		} else if (intel_sdvo_connector->bottom == property) {
			drm_connector_property_set_value(connector,
							 intel_sdvo_connector->top, val);
			if (intel_sdvo_connector->bottom_margin == temp_value)
				return 0;

			intel_sdvo_connector->top_margin = temp_value;
			intel_sdvo_connector->bottom_margin = temp_value;
			temp_value = intel_sdvo_connector->max_vscan -
				intel_sdvo_connector->top_margin;
			cmd = SDVO_CMD_SET_OVERSCAN_V;
			goto set_value;
		}
		CHECK_PROPERTY(hpos, HPOS)
		CHECK_PROPERTY(vpos, VPOS)
		CHECK_PROPERTY(saturation, SATURATION)
		CHECK_PROPERTY(contrast, CONTRAST)
		CHECK_PROPERTY(hue, HUE)
		CHECK_PROPERTY(brightness, BRIGHTNESS)
		CHECK_PROPERTY(sharpness, SHARPNESS)
		CHECK_PROPERTY(flicker_filter, FLICKER_FILTER)
		CHECK_PROPERTY(flicker_filter_2d, FLICKER_FILTER_2D)
		CHECK_PROPERTY(flicker_filter_adaptive, FLICKER_FILTER_ADAPTIVE)
		CHECK_PROPERTY(tv_chroma_filter, TV_CHROMA_FILTER)
		CHECK_PROPERTY(tv_luma_filter, TV_LUMA_FILTER)
		CHECK_PROPERTY(dot_crawl, DOT_CRAWL)
	}

	return -EINVAL; /* unknown property */

set_value:
	if (!intel_sdvo_set_value(intel_sdvo, cmd, &temp_value, 2))
		return -EIO;


done:
	if (encoder->crtc) {
		struct drm_crtc *crtc = encoder->crtc;

		drm_crtc_helper_set_mode(crtc, &crtc->mode, crtc->x,
					 crtc->y, crtc->fb);
	}

	return 0;
#undef CHECK_PROPERTY
}

static const struct drm_encoder_helper_funcs intel_sdvo_helper_funcs = {
	.dpms = intel_sdvo_dpms,
	.mode_fixup = intel_sdvo_mode_fixup,
	.prepare = intel_encoder_prepare,
	.mode_set = intel_sdvo_mode_set,
	.commit = intel_encoder_commit,
};

static const struct drm_connector_funcs intel_sdvo_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.detect = intel_sdvo_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.set_property = intel_sdvo_set_property,
	.destroy = intel_sdvo_destroy,
};

static const struct drm_connector_helper_funcs intel_sdvo_connector_helper_funcs = {
	.get_modes = intel_sdvo_get_modes,
	.mode_valid = intel_sdvo_mode_valid,
	.best_encoder = intel_attached_encoder,
};

static void intel_sdvo_enc_destroy(struct drm_encoder *encoder)
{
	struct intel_sdvo *intel_sdvo = enc_to_intel_sdvo(encoder);

	if (intel_sdvo->analog_ddc_bus)
		intel_i2c_destroy(intel_sdvo->analog_ddc_bus);

	if (intel_sdvo->sdvo_lvds_fixed_mode != NULL)
		drm_mode_destroy(encoder->dev,
				 intel_sdvo->sdvo_lvds_fixed_mode);

	intel_encoder_destroy(encoder);
}

static const struct drm_encoder_funcs intel_sdvo_enc_funcs = {
	.destroy = intel_sdvo_enc_destroy,
};

static void
intel_sdvo_guess_ddc_bus(struct intel_sdvo *sdvo)
{
	uint16_t mask = 0;
	unsigned int num_bits;

	/* Make a mask of outputs less than or equal to our own priority in the
	 * list.
	 */
	switch (sdvo->controlled_output) {
	case SDVO_OUTPUT_LVDS1:
		mask |= SDVO_OUTPUT_LVDS1;
	case SDVO_OUTPUT_LVDS0:
		mask |= SDVO_OUTPUT_LVDS0;
	case SDVO_OUTPUT_TMDS1:
		mask |= SDVO_OUTPUT_TMDS1;
	case SDVO_OUTPUT_TMDS0:
		mask |= SDVO_OUTPUT_TMDS0;
	case SDVO_OUTPUT_RGB1:
		mask |= SDVO_OUTPUT_RGB1;
	case SDVO_OUTPUT_RGB0:
		mask |= SDVO_OUTPUT_RGB0;
		break;
	}

	/* Count bits to find what number we are in the priority list. */
	mask &= sdvo->caps.output_flags;
	num_bits = hweight16(mask);
	/* If more than 3 outputs, default to DDC bus 3 for now. */
	if (num_bits > 3)
		num_bits = 3;

	/* Corresponds to SDVO_CONTROL_BUS_DDCx */
	sdvo->ddc_bus = 1 << num_bits;
}

/**
 * Choose the appropriate DDC bus for control bus switch command for this
 * SDVO output based on the controlled output.
 *
 * DDC bus number assignment is in a priority order of RGB outputs, then TMDS
 * outputs, then LVDS outputs.
 */
static void
intel_sdvo_select_ddc_bus(struct drm_i915_private *dev_priv,
			  struct intel_sdvo *sdvo, u32 reg)
{
	struct sdvo_device_mapping *mapping;

	if (IS_SDVOB(reg))
		mapping = &(dev_priv->sdvo_mappings[0]);
	else
		mapping = &(dev_priv->sdvo_mappings[1]);

	if (mapping->initialized)
		sdvo->ddc_bus = 1 << ((mapping->ddc_pin & 0xf0) >> 4);
	else
		intel_sdvo_guess_ddc_bus(sdvo);
}

static bool
intel_sdvo_get_digital_encoding_mode(struct intel_sdvo *intel_sdvo, int device)
{
	return intel_sdvo_set_target_output(intel_sdvo,
					    device == 0 ? SDVO_OUTPUT_TMDS0 : SDVO_OUTPUT_TMDS1) &&
		intel_sdvo_get_value(intel_sdvo, SDVO_CMD_GET_ENCODE,
				     &intel_sdvo->is_hdmi, 1);
}

static struct intel_sdvo *
intel_sdvo_chan_to_intel_sdvo(struct intel_i2c_chan *chan)
{
	struct drm_device *dev = chan->drm_dev;
	struct drm_encoder *encoder;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		struct intel_sdvo *intel_sdvo = enc_to_intel_sdvo(encoder);
		if (intel_sdvo->base.ddc_bus == &chan->adapter)
			return intel_sdvo;
	}

	return NULL;
}

static int intel_sdvo_master_xfer(struct i2c_adapter *i2c_adap,
				  struct i2c_msg msgs[], int num)
{
	struct intel_sdvo *intel_sdvo;
	struct i2c_algo_bit_data *algo_data;
	const struct i2c_algorithm *algo;

	algo_data = (struct i2c_algo_bit_data *)i2c_adap->algo_data;
	intel_sdvo =
		intel_sdvo_chan_to_intel_sdvo((struct intel_i2c_chan *)
					      (algo_data->data));
	if (intel_sdvo == NULL)
		return -EINVAL;

	algo = intel_sdvo->base.i2c_bus->algo;

	intel_sdvo_set_control_bus_switch(intel_sdvo, intel_sdvo->ddc_bus);
	return algo->master_xfer(i2c_adap, msgs, num);
}

static struct i2c_algorithm intel_sdvo_i2c_bit_algo = {
	.master_xfer	= intel_sdvo_master_xfer,
};

static u8
intel_sdvo_get_slave_addr(struct drm_device *dev, int sdvo_reg)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct sdvo_device_mapping *my_mapping, *other_mapping;

	if (IS_SDVOB(sdvo_reg)) {
		my_mapping = &dev_priv->sdvo_mappings[0];
		other_mapping = &dev_priv->sdvo_mappings[1];
	} else {
		my_mapping = &dev_priv->sdvo_mappings[1];
		other_mapping = &dev_priv->sdvo_mappings[0];
	}

	/* If the BIOS described our SDVO device, take advantage of it. */
	if (my_mapping->slave_addr)
		return my_mapping->slave_addr;

	/* If the BIOS only described a different SDVO device, use the
	 * address that it isn't using.
	 */
	if (other_mapping->slave_addr) {
		if (other_mapping->slave_addr == 0x70)
			return 0x72;
		else
			return 0x70;
	}

	/* No SDVO device info is found for another DVO port,
	 * so use mapping assumption we had before BIOS parsing.
	 */
	if (IS_SDVOB(sdvo_reg))
		return 0x70;
	else
		return 0x72;
}

static void
intel_sdvo_connector_init(struct drm_encoder *encoder,
			  struct drm_connector *connector)
{
	drm_connector_init(encoder->dev, connector, &intel_sdvo_connector_funcs,
			   connector->connector_type);

	drm_connector_helper_add(connector, &intel_sdvo_connector_helper_funcs);

	connector->interlace_allowed = 0;
	connector->doublescan_allowed = 0;
	connector->display_info.subpixel_order = SubPixelHorizontalRGB;

	drm_mode_connector_attach_encoder(connector, encoder);
	drm_sysfs_connector_add(connector);
}

static bool
intel_sdvo_dvi_init(struct intel_sdvo *intel_sdvo, int device)
{
	struct drm_encoder *encoder = &intel_sdvo->base.enc;
	struct drm_connector *connector;
	struct intel_connector *intel_connector;
	struct intel_sdvo_connector *intel_sdvo_connector;

	intel_sdvo_connector = kzalloc(sizeof(struct intel_sdvo_connector), GFP_KERNEL);
	if (!intel_sdvo_connector)
		return false;

	if (device == 0) {
		intel_sdvo->controlled_output |= SDVO_OUTPUT_TMDS0;
		intel_sdvo_connector->output_flag = SDVO_OUTPUT_TMDS0;
	} else if (device == 1) {
		intel_sdvo->controlled_output |= SDVO_OUTPUT_TMDS1;
		intel_sdvo_connector->output_flag = SDVO_OUTPUT_TMDS1;
	}

	intel_connector = &intel_sdvo_connector->base;
	connector = &intel_connector->base;
	connector->polled = DRM_CONNECTOR_POLL_CONNECT | DRM_CONNECTOR_POLL_DISCONNECT;
	encoder->encoder_type = DRM_MODE_ENCODER_TMDS;
	connector->connector_type = DRM_MODE_CONNECTOR_DVID;

	if (intel_sdvo_get_supp_encode(intel_sdvo, &intel_sdvo->encode)
		&& intel_sdvo_get_digital_encoding_mode(intel_sdvo, device)
		&& intel_sdvo->is_hdmi) {
		/* enable hdmi encoding mode if supported */
		intel_sdvo_set_encode(intel_sdvo, SDVO_ENCODE_HDMI);
		intel_sdvo_set_colorimetry(intel_sdvo,
					   SDVO_COLORIMETRY_RGB256);
		connector->connector_type = DRM_MODE_CONNECTOR_HDMIA;
	}
	intel_sdvo->base.clone_mask = ((1 << INTEL_SDVO_NON_TV_CLONE_BIT) |
				       (1 << INTEL_ANALOG_CLONE_BIT));

	intel_sdvo_connector_init(encoder, connector);

	return true;
}

static bool
intel_sdvo_tv_init(struct intel_sdvo *intel_sdvo, int type)
{
        struct drm_encoder *encoder = &intel_sdvo->base.enc;
        struct drm_connector *connector;
        struct intel_connector *intel_connector;
        struct intel_sdvo_connector *intel_sdvo_connector;

	intel_sdvo_connector = kzalloc(sizeof(struct intel_sdvo_connector), GFP_KERNEL);
	if (!intel_sdvo_connector)
		return false;

	intel_connector = &intel_sdvo_connector->base;
        connector = &intel_connector->base;
        encoder->encoder_type = DRM_MODE_ENCODER_TVDAC;
        connector->connector_type = DRM_MODE_CONNECTOR_SVIDEO;

        intel_sdvo->controlled_output |= type;
        intel_sdvo_connector->output_flag = type;

        intel_sdvo->is_tv = true;
        intel_sdvo->base.needs_tv_clock = true;
        intel_sdvo->base.clone_mask = 1 << INTEL_SDVO_TV_CLONE_BIT;

        intel_sdvo_connector_init(encoder, connector);

        if (!intel_sdvo_tv_create_property(intel_sdvo, intel_sdvo_connector, type))
		goto err;

        if (!intel_sdvo_create_enhance_property(intel_sdvo, intel_sdvo_connector))
		goto err;

        return true;

err:
	intel_sdvo_destroy(connector);
	return false;
}

static bool
intel_sdvo_analog_init(struct intel_sdvo *intel_sdvo, int device)
{
        struct drm_encoder *encoder = &intel_sdvo->base.enc;
        struct drm_connector *connector;
        struct intel_connector *intel_connector;
        struct intel_sdvo_connector *intel_sdvo_connector;

	intel_sdvo_connector = kzalloc(sizeof(struct intel_sdvo_connector), GFP_KERNEL);
	if (!intel_sdvo_connector)
		return false;

	intel_connector = &intel_sdvo_connector->base;
        connector = &intel_connector->base;
	connector->polled = DRM_CONNECTOR_POLL_CONNECT;
        encoder->encoder_type = DRM_MODE_ENCODER_DAC;
        connector->connector_type = DRM_MODE_CONNECTOR_VGA;

        if (device == 0) {
                intel_sdvo->controlled_output |= SDVO_OUTPUT_RGB0;
                intel_sdvo_connector->output_flag = SDVO_OUTPUT_RGB0;
        } else if (device == 1) {
                intel_sdvo->controlled_output |= SDVO_OUTPUT_RGB1;
                intel_sdvo_connector->output_flag = SDVO_OUTPUT_RGB1;
        }

        intel_sdvo->base.clone_mask = ((1 << INTEL_SDVO_NON_TV_CLONE_BIT) |
				       (1 << INTEL_ANALOG_CLONE_BIT));

        intel_sdvo_connector_init(encoder, connector);
        return true;
}

static bool
intel_sdvo_lvds_init(struct intel_sdvo *intel_sdvo, int device)
{
        struct drm_encoder *encoder = &intel_sdvo->base.enc;
        struct drm_connector *connector;
        struct intel_connector *intel_connector;
        struct intel_sdvo_connector *intel_sdvo_connector;

	intel_sdvo_connector = kzalloc(sizeof(struct intel_sdvo_connector), GFP_KERNEL);
	if (!intel_sdvo_connector)
		return false;

	intel_connector = &intel_sdvo_connector->base;
	connector = &intel_connector->base;
        encoder->encoder_type = DRM_MODE_ENCODER_LVDS;
        connector->connector_type = DRM_MODE_CONNECTOR_LVDS;

        if (device == 0) {
                intel_sdvo->controlled_output |= SDVO_OUTPUT_LVDS0;
                intel_sdvo_connector->output_flag = SDVO_OUTPUT_LVDS0;
        } else if (device == 1) {
                intel_sdvo->controlled_output |= SDVO_OUTPUT_LVDS1;
                intel_sdvo_connector->output_flag = SDVO_OUTPUT_LVDS1;
        }

        intel_sdvo->base.clone_mask = ((1 << INTEL_ANALOG_CLONE_BIT) |
				       (1 << INTEL_SDVO_LVDS_CLONE_BIT));

        intel_sdvo_connector_init(encoder, connector);
        if (!intel_sdvo_create_enhance_property(intel_sdvo, intel_sdvo_connector))
		goto err;

	return true;

err:
	intel_sdvo_destroy(connector);
	return false;
}

static bool
intel_sdvo_output_setup(struct intel_sdvo *intel_sdvo, uint16_t flags)
{
	intel_sdvo->is_tv = false;
	intel_sdvo->base.needs_tv_clock = false;
	intel_sdvo->is_lvds = false;

	/* SDVO requires XXX1 function may not exist unless it has XXX0 function.*/

	if (flags & SDVO_OUTPUT_TMDS0)
		if (!intel_sdvo_dvi_init(intel_sdvo, 0))
			return false;

	if ((flags & SDVO_TMDS_MASK) == SDVO_TMDS_MASK)
		if (!intel_sdvo_dvi_init(intel_sdvo, 1))
			return false;

	/* TV has no XXX1 function block */
	if (flags & SDVO_OUTPUT_SVID0)
		if (!intel_sdvo_tv_init(intel_sdvo, SDVO_OUTPUT_SVID0))
			return false;

	if (flags & SDVO_OUTPUT_CVBS0)
		if (!intel_sdvo_tv_init(intel_sdvo, SDVO_OUTPUT_CVBS0))
			return false;

	if (flags & SDVO_OUTPUT_RGB0)
		if (!intel_sdvo_analog_init(intel_sdvo, 0))
			return false;

	if ((flags & SDVO_RGB_MASK) == SDVO_RGB_MASK)
		if (!intel_sdvo_analog_init(intel_sdvo, 1))
			return false;

	if (flags & SDVO_OUTPUT_LVDS0)
		if (!intel_sdvo_lvds_init(intel_sdvo, 0))
			return false;

	if ((flags & SDVO_LVDS_MASK) == SDVO_LVDS_MASK)
		if (!intel_sdvo_lvds_init(intel_sdvo, 1))
			return false;

	if ((flags & SDVO_OUTPUT_MASK) == 0) {
		unsigned char bytes[2];

		intel_sdvo->controlled_output = 0;
		memcpy(bytes, &intel_sdvo->caps.output_flags, 2);
		DRM_DEBUG_KMS("%s: Unknown SDVO output type (0x%02x%02x)\n",
			      SDVO_NAME(intel_sdvo),
			      bytes[0], bytes[1]);
		return false;
	}
	intel_sdvo->base.crtc_mask = (1 << 0) | (1 << 1);

	return true;
}

static bool intel_sdvo_tv_create_property(struct intel_sdvo *intel_sdvo,
					  struct intel_sdvo_connector *intel_sdvo_connector,
					  int type)
{
	struct drm_device *dev = intel_sdvo->base.enc.dev;
	struct intel_sdvo_tv_format format;
	uint32_t format_map, i;

	if (!intel_sdvo_set_target_output(intel_sdvo, type))
		return false;

	if (!intel_sdvo_get_value(intel_sdvo,
				  SDVO_CMD_GET_SUPPORTED_TV_FORMATS,
				  &format, sizeof(format)))
		return false;

	memcpy(&format_map, &format, min(sizeof(format_map), sizeof(format)));

	if (format_map == 0)
		return false;

	intel_sdvo_connector->format_supported_num = 0;
	for (i = 0 ; i < TV_FORMAT_NUM; i++)
		if (format_map & (1 << i))
			intel_sdvo_connector->tv_format_supported[intel_sdvo_connector->format_supported_num++] = i;


	intel_sdvo_connector->tv_format =
			drm_property_create(dev, DRM_MODE_PROP_ENUM,
					    "mode", intel_sdvo_connector->format_supported_num);
	if (!intel_sdvo_connector->tv_format)
		return false;

	for (i = 0; i < intel_sdvo_connector->format_supported_num; i++)
		drm_property_add_enum(
				intel_sdvo_connector->tv_format, i,
				i, tv_format_names[intel_sdvo_connector->tv_format_supported[i]]);

	intel_sdvo->tv_format_index = intel_sdvo_connector->tv_format_supported[0];
	drm_connector_attach_property(&intel_sdvo_connector->base.base,
				      intel_sdvo_connector->tv_format, 0);
	return true;

}

#define ENHANCEMENT(name, NAME) do { \
	if (enhancements.name) { \
		if (!intel_sdvo_get_value(intel_sdvo, SDVO_CMD_GET_MAX_##NAME, &data_value, 4) || \
		    !intel_sdvo_get_value(intel_sdvo, SDVO_CMD_GET_##NAME, &response, 2)) \
			return false; \
		intel_sdvo_connector->max_##name = data_value[0]; \
		intel_sdvo_connector->cur_##name = response; \
		intel_sdvo_connector->name = \
			drm_property_create(dev, DRM_MODE_PROP_RANGE, #name, 2); \
		if (!intel_sdvo_connector->name) return false; \
		intel_sdvo_connector->name->values[0] = 0; \
		intel_sdvo_connector->name->values[1] = data_value[0]; \
		drm_connector_attach_property(connector, \
					      intel_sdvo_connector->name, \
					      intel_sdvo_connector->cur_##name); \
		DRM_DEBUG_KMS(#name ": max %d, default %d, current %d\n", \
			      data_value[0], data_value[1], response); \
	} \
} while(0)

static bool
intel_sdvo_create_enhance_property_tv(struct intel_sdvo *intel_sdvo,
				      struct intel_sdvo_connector *intel_sdvo_connector,
				      struct intel_sdvo_enhancements_reply enhancements)
{
	struct drm_device *dev = intel_sdvo->base.enc.dev;
	struct drm_connector *connector = &intel_sdvo_connector->base.base;
	uint16_t response, data_value[2];

	/* when horizontal overscan is supported, Add the left/right  property */
	if (enhancements.overscan_h) {
		if (!intel_sdvo_get_value(intel_sdvo,
					  SDVO_CMD_GET_MAX_OVERSCAN_H,
					  &data_value, 4))
			return false;

		if (!intel_sdvo_get_value(intel_sdvo,
					  SDVO_CMD_GET_OVERSCAN_H,
					  &response, 2))
			return false;

		intel_sdvo_connector->max_hscan = data_value[0];
		intel_sdvo_connector->left_margin = data_value[0] - response;
		intel_sdvo_connector->right_margin = intel_sdvo_connector->left_margin;
		intel_sdvo_connector->left =
			drm_property_create(dev, DRM_MODE_PROP_RANGE,
					    "left_margin", 2);
		if (!intel_sdvo_connector->left)
			return false;

		intel_sdvo_connector->left->values[0] = 0;
		intel_sdvo_connector->left->values[1] = data_value[0];
		drm_connector_attach_property(connector,
					      intel_sdvo_connector->left,
					      intel_sdvo_connector->left_margin);

		intel_sdvo_connector->right =
			drm_property_create(dev, DRM_MODE_PROP_RANGE,
					    "right_margin", 2);
		if (!intel_sdvo_connector->right)
			return false;

		intel_sdvo_connector->right->values[0] = 0;
		intel_sdvo_connector->right->values[1] = data_value[0];
		drm_connector_attach_property(connector,
					      intel_sdvo_connector->right,
					      intel_sdvo_connector->right_margin);
		DRM_DEBUG_KMS("h_overscan: max %d, "
			      "default %d, current %d\n",
			      data_value[0], data_value[1], response);
	}

	if (enhancements.overscan_v) {
		if (!intel_sdvo_get_value(intel_sdvo,
					  SDVO_CMD_GET_MAX_OVERSCAN_V,
					  &data_value, 4))
			return false;

		if (!intel_sdvo_get_value(intel_sdvo,
					  SDVO_CMD_GET_OVERSCAN_V,
					  &response, 2))
			return false;

		intel_sdvo_connector->max_vscan = data_value[0];
		intel_sdvo_connector->top_margin = data_value[0] - response;
		intel_sdvo_connector->bottom_margin = intel_sdvo_connector->top_margin;
		intel_sdvo_connector->top =
			drm_property_create(dev, DRM_MODE_PROP_RANGE,
					    "top_margin", 2);
		if (!intel_sdvo_connector->top)
			return false;

		intel_sdvo_connector->top->values[0] = 0;
		intel_sdvo_connector->top->values[1] = data_value[0];
		drm_connector_attach_property(connector,
					      intel_sdvo_connector->top,
					      intel_sdvo_connector->top_margin);

		intel_sdvo_connector->bottom =
			drm_property_create(dev, DRM_MODE_PROP_RANGE,
					    "bottom_margin", 2);
		if (!intel_sdvo_connector->bottom)
			return false;

		intel_sdvo_connector->bottom->values[0] = 0;
		intel_sdvo_connector->bottom->values[1] = data_value[0];
		drm_connector_attach_property(connector,
					      intel_sdvo_connector->bottom,
					      intel_sdvo_connector->bottom_margin);
		DRM_DEBUG_KMS("v_overscan: max %d, "
			      "default %d, current %d\n",
			      data_value[0], data_value[1], response);
	}

	ENHANCEMENT(hpos, HPOS);
	ENHANCEMENT(vpos, VPOS);
	ENHANCEMENT(saturation, SATURATION);
	ENHANCEMENT(contrast, CONTRAST);
	ENHANCEMENT(hue, HUE);
	ENHANCEMENT(sharpness, SHARPNESS);
	ENHANCEMENT(brightness, BRIGHTNESS);
	ENHANCEMENT(flicker_filter, FLICKER_FILTER);
	ENHANCEMENT(flicker_filter_adaptive, FLICKER_FILTER_ADAPTIVE);
	ENHANCEMENT(flicker_filter_2d, FLICKER_FILTER_2D);
	ENHANCEMENT(tv_chroma_filter, TV_CHROMA_FILTER);
	ENHANCEMENT(tv_luma_filter, TV_LUMA_FILTER);

	if (enhancements.dot_crawl) {
		if (!intel_sdvo_get_value(intel_sdvo, SDVO_CMD_GET_DOT_CRAWL, &response, 2))
			return false;

		intel_sdvo_connector->max_dot_crawl = 1;
		intel_sdvo_connector->cur_dot_crawl = response & 0x1;
		intel_sdvo_connector->dot_crawl =
			drm_property_create(dev, DRM_MODE_PROP_RANGE, "dot_crawl", 2);
		if (!intel_sdvo_connector->dot_crawl)
			return false;

		intel_sdvo_connector->dot_crawl->values[0] = 0;
		intel_sdvo_connector->dot_crawl->values[1] = 1;
		drm_connector_attach_property(connector,
					      intel_sdvo_connector->dot_crawl,
					      intel_sdvo_connector->cur_dot_crawl);
		DRM_DEBUG_KMS("dot crawl: current %d\n", response);
	}

	return true;
}

static bool
intel_sdvo_create_enhance_property_lvds(struct intel_sdvo *intel_sdvo,
					struct intel_sdvo_connector *intel_sdvo_connector,
					struct intel_sdvo_enhancements_reply enhancements)
{
	struct drm_device *dev = intel_sdvo->base.enc.dev;
	struct drm_connector *connector = &intel_sdvo_connector->base.base;
	uint16_t response, data_value[2];

	ENHANCEMENT(brightness, BRIGHTNESS);

	return true;
}
#undef ENHANCEMENT

static bool intel_sdvo_create_enhance_property(struct intel_sdvo *intel_sdvo,
					       struct intel_sdvo_connector *intel_sdvo_connector)
{
	union {
		struct intel_sdvo_enhancements_reply reply;
		uint16_t response;
	} enhancements;

	enhancements.response = 0;
	intel_sdvo_get_value(intel_sdvo,
			     SDVO_CMD_GET_SUPPORTED_ENHANCEMENTS,
			     &enhancements, sizeof(enhancements));
	if (enhancements.response == 0) {
		DRM_DEBUG_KMS("No enhancement is supported\n");
		return true;
	}

	if (IS_TV(intel_sdvo_connector))
		return intel_sdvo_create_enhance_property_tv(intel_sdvo, intel_sdvo_connector, enhancements.reply);
	else if(IS_LVDS(intel_sdvo_connector))
		return intel_sdvo_create_enhance_property_lvds(intel_sdvo, intel_sdvo_connector, enhancements.reply);
	else
		return true;

}

bool intel_sdvo_init(struct drm_device *dev, int sdvo_reg)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_encoder *intel_encoder;
	struct intel_sdvo *intel_sdvo;
	u8 ch[0x40];
	int i;
	u32 i2c_reg, ddc_reg, analog_ddc_reg;

	intel_sdvo = kzalloc(sizeof(struct intel_sdvo), GFP_KERNEL);
	if (!intel_sdvo)
		return false;

	intel_sdvo->sdvo_reg = sdvo_reg;

	intel_encoder = &intel_sdvo->base;
	intel_encoder->type = INTEL_OUTPUT_SDVO;

	if (HAS_PCH_SPLIT(dev)) {
		i2c_reg = PCH_GPIOE;
		ddc_reg = PCH_GPIOE;
		analog_ddc_reg = PCH_GPIOA;
	} else {
		i2c_reg = GPIOE;
		ddc_reg = GPIOE;
		analog_ddc_reg = GPIOA;
	}

	/* setup the DDC bus. */
	if (IS_SDVOB(sdvo_reg))
		intel_encoder->i2c_bus = intel_i2c_create(dev, i2c_reg, "SDVOCTRL_E for SDVOB");
	else
		intel_encoder->i2c_bus = intel_i2c_create(dev, i2c_reg, "SDVOCTRL_E for SDVOC");

	if (!intel_encoder->i2c_bus)
		goto err_inteloutput;

	intel_sdvo->slave_addr = intel_sdvo_get_slave_addr(dev, sdvo_reg);

	/* Save the bit-banging i2c functionality for use by the DDC wrapper */
	intel_sdvo_i2c_bit_algo.functionality = intel_encoder->i2c_bus->algo->functionality;

	/* Read the regs to test if we can talk to the device */
	for (i = 0; i < 0x40; i++) {
		if (!intel_sdvo_read_byte(intel_sdvo, i, &ch[i])) {
			DRM_DEBUG_KMS("No SDVO device found on SDVO%c\n",
				      IS_SDVOB(sdvo_reg) ? 'B' : 'C');
			goto err_i2c;
		}
	}

	/* setup the DDC bus. */
	if (IS_SDVOB(sdvo_reg)) {
		intel_encoder->ddc_bus = intel_i2c_create(dev, ddc_reg, "SDVOB DDC BUS");
		intel_sdvo->analog_ddc_bus = intel_i2c_create(dev, analog_ddc_reg,
						"SDVOB/VGA DDC BUS");
		dev_priv->hotplug_supported_mask |= SDVOB_HOTPLUG_INT_STATUS;
	} else {
		intel_encoder->ddc_bus = intel_i2c_create(dev, ddc_reg, "SDVOC DDC BUS");
		intel_sdvo->analog_ddc_bus = intel_i2c_create(dev, analog_ddc_reg,
						"SDVOC/VGA DDC BUS");
		dev_priv->hotplug_supported_mask |= SDVOC_HOTPLUG_INT_STATUS;
	}
	if (intel_encoder->ddc_bus == NULL || intel_sdvo->analog_ddc_bus == NULL)
		goto err_i2c;

	/* Wrap with our custom algo which switches to DDC mode */
	intel_encoder->ddc_bus->algo = &intel_sdvo_i2c_bit_algo;

	/* encoder type will be decided later */
	drm_encoder_init(dev, &intel_encoder->enc, &intel_sdvo_enc_funcs, 0);
	drm_encoder_helper_add(&intel_encoder->enc, &intel_sdvo_helper_funcs);

	/* In default case sdvo lvds is false */
	if (!intel_sdvo_get_capabilities(intel_sdvo, &intel_sdvo->caps))
		goto err_enc;

	if (intel_sdvo_output_setup(intel_sdvo,
				    intel_sdvo->caps.output_flags) != true) {
		DRM_DEBUG_KMS("SDVO output failed to setup on SDVO%c\n",
			      IS_SDVOB(sdvo_reg) ? 'B' : 'C');
		goto err_enc;
	}

	intel_sdvo_select_ddc_bus(dev_priv, intel_sdvo, sdvo_reg);

	/* Set the input timing to the screen. Assume always input 0. */
	if (!intel_sdvo_set_target_input(intel_sdvo))
		goto err_enc;

	if (!intel_sdvo_get_input_pixel_clock_range(intel_sdvo,
						    &intel_sdvo->pixel_clock_min,
						    &intel_sdvo->pixel_clock_max))
		goto err_enc;

	DRM_DEBUG_KMS("%s device VID/DID: %02X:%02X.%02X, "
			"clock range %dMHz - %dMHz, "
			"input 1: %c, input 2: %c, "
			"output 1: %c, output 2: %c\n",
			SDVO_NAME(intel_sdvo),
			intel_sdvo->caps.vendor_id, intel_sdvo->caps.device_id,
			intel_sdvo->caps.device_rev_id,
			intel_sdvo->pixel_clock_min / 1000,
			intel_sdvo->pixel_clock_max / 1000,
			(intel_sdvo->caps.sdvo_inputs_mask & 0x1) ? 'Y' : 'N',
			(intel_sdvo->caps.sdvo_inputs_mask & 0x2) ? 'Y' : 'N',
			/* check currently supported outputs */
			intel_sdvo->caps.output_flags &
			(SDVO_OUTPUT_TMDS0 | SDVO_OUTPUT_RGB0) ? 'Y' : 'N',
			intel_sdvo->caps.output_flags &
			(SDVO_OUTPUT_TMDS1 | SDVO_OUTPUT_RGB1) ? 'Y' : 'N');
	return true;

err_enc:
	drm_encoder_cleanup(&intel_encoder->enc);
err_i2c:
	if (intel_sdvo->analog_ddc_bus != NULL)
		intel_i2c_destroy(intel_sdvo->analog_ddc_bus);
	if (intel_encoder->ddc_bus != NULL)
		intel_i2c_destroy(intel_encoder->ddc_bus);
	if (intel_encoder->i2c_bus != NULL)
		intel_i2c_destroy(intel_encoder->i2c_bus);
err_inteloutput:
	kfree(intel_sdvo);

	return false;
}
