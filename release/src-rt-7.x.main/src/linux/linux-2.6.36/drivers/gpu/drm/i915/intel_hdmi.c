/*
 * Copyright 2006 Dave Airlie <airlied@linux.ie>
 * Copyright © 2006-2009 Intel Corporation
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
 *	Jesse Barnes <jesse.barnes@intel.com>
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

struct intel_hdmi {
	struct intel_encoder base;
	u32 sdvox_reg;
	bool has_hdmi_sink;
};

static struct intel_hdmi *enc_to_intel_hdmi(struct drm_encoder *encoder)
{
	return container_of(enc_to_intel_encoder(encoder), struct intel_hdmi, base);
}

static void intel_hdmi_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_crtc *crtc = encoder->crtc;
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
	struct intel_hdmi *intel_hdmi = enc_to_intel_hdmi(encoder);
	u32 sdvox;

	sdvox = SDVO_ENCODING_HDMI | SDVO_BORDER_ENABLE;
	if (adjusted_mode->flags & DRM_MODE_FLAG_PVSYNC)
		sdvox |= SDVO_VSYNC_ACTIVE_HIGH;
	if (adjusted_mode->flags & DRM_MODE_FLAG_PHSYNC)
		sdvox |= SDVO_HSYNC_ACTIVE_HIGH;

	if (intel_hdmi->has_hdmi_sink) {
		sdvox |= SDVO_AUDIO_ENABLE;
		if (HAS_PCH_CPT(dev))
			sdvox |= HDMI_MODE_SELECT;
	}

	if (intel_crtc->pipe == 1) {
		if (HAS_PCH_CPT(dev))
			sdvox |= PORT_TRANS_B_SEL_CPT;
		else
			sdvox |= SDVO_PIPE_B_SELECT;
	}

	I915_WRITE(intel_hdmi->sdvox_reg, sdvox);
	POSTING_READ(intel_hdmi->sdvox_reg);
}

static void intel_hdmi_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_hdmi *intel_hdmi = enc_to_intel_hdmi(encoder);
	u32 temp;

	temp = I915_READ(intel_hdmi->sdvox_reg);

	if (HAS_PCH_SPLIT(dev)) {
		I915_WRITE(intel_hdmi->sdvox_reg, temp & ~SDVO_ENABLE);
		POSTING_READ(intel_hdmi->sdvox_reg);
	}

	if (mode != DRM_MODE_DPMS_ON) {
		temp &= ~SDVO_ENABLE;
	} else {
		temp |= SDVO_ENABLE;
	}

	I915_WRITE(intel_hdmi->sdvox_reg, temp);
	POSTING_READ(intel_hdmi->sdvox_reg);

	if (HAS_PCH_SPLIT(dev)) {
		I915_WRITE(intel_hdmi->sdvox_reg, temp);
		POSTING_READ(intel_hdmi->sdvox_reg);
	}
}

static int intel_hdmi_mode_valid(struct drm_connector *connector,
				 struct drm_display_mode *mode)
{
	if (mode->clock > 165000)
		return MODE_CLOCK_HIGH;
	if (mode->clock < 20000)
		return MODE_CLOCK_HIGH;

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		return MODE_NO_DBLESCAN;

	return MODE_OK;
}

static bool intel_hdmi_mode_fixup(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	return true;
}

static enum drm_connector_status
intel_hdmi_detect(struct drm_connector *connector, bool force)
{
	struct drm_encoder *encoder = intel_attached_encoder(connector);
	struct intel_hdmi *intel_hdmi = enc_to_intel_hdmi(encoder);
	struct edid *edid = NULL;
	enum drm_connector_status status = connector_status_disconnected;

	intel_hdmi->has_hdmi_sink = false;
	edid = drm_get_edid(connector, intel_hdmi->base.ddc_bus);

	if (edid) {
		if (edid->input & DRM_EDID_INPUT_DIGITAL) {
			status = connector_status_connected;
			intel_hdmi->has_hdmi_sink = drm_detect_hdmi_monitor(edid);
		}
		connector->display_info.raw_edid = NULL;
		kfree(edid);
	}

	return status;
}

static int intel_hdmi_get_modes(struct drm_connector *connector)
{
	struct drm_encoder *encoder = intel_attached_encoder(connector);
	struct intel_hdmi *intel_hdmi = enc_to_intel_hdmi(encoder);

	/* We should parse the EDID data and find out if it's an HDMI sink so
	 * we can send audio to it.
	 */

	return intel_ddc_get_modes(connector, intel_hdmi->base.ddc_bus);
}

static void intel_hdmi_destroy(struct drm_connector *connector)
{
	drm_sysfs_connector_remove(connector);
	drm_connector_cleanup(connector);
	kfree(connector);
}

static const struct drm_encoder_helper_funcs intel_hdmi_helper_funcs = {
	.dpms = intel_hdmi_dpms,
	.mode_fixup = intel_hdmi_mode_fixup,
	.prepare = intel_encoder_prepare,
	.mode_set = intel_hdmi_mode_set,
	.commit = intel_encoder_commit,
};

static const struct drm_connector_funcs intel_hdmi_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.detect = intel_hdmi_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = intel_hdmi_destroy,
};

static const struct drm_connector_helper_funcs intel_hdmi_connector_helper_funcs = {
	.get_modes = intel_hdmi_get_modes,
	.mode_valid = intel_hdmi_mode_valid,
	.best_encoder = intel_attached_encoder,
};

static const struct drm_encoder_funcs intel_hdmi_enc_funcs = {
	.destroy = intel_encoder_destroy,
};

void intel_hdmi_init(struct drm_device *dev, int sdvox_reg)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_connector *connector;
	struct intel_encoder *intel_encoder;
	struct intel_connector *intel_connector;
	struct intel_hdmi *intel_hdmi;

	intel_hdmi = kzalloc(sizeof(struct intel_hdmi), GFP_KERNEL);
	if (!intel_hdmi)
		return;

	intel_connector = kzalloc(sizeof(struct intel_connector), GFP_KERNEL);
	if (!intel_connector) {
		kfree(intel_hdmi);
		return;
	}

	intel_encoder = &intel_hdmi->base;
	connector = &intel_connector->base;
	drm_connector_init(dev, connector, &intel_hdmi_connector_funcs,
			   DRM_MODE_CONNECTOR_HDMIA);
	drm_connector_helper_add(connector, &intel_hdmi_connector_helper_funcs);

	intel_encoder->type = INTEL_OUTPUT_HDMI;

	connector->polled = DRM_CONNECTOR_POLL_HPD;
	connector->interlace_allowed = 0;
	connector->doublescan_allowed = 0;
	intel_encoder->crtc_mask = (1 << 0) | (1 << 1);

	/* Set up the DDC bus. */
	if (sdvox_reg == SDVOB) {
		intel_encoder->clone_mask = (1 << INTEL_HDMIB_CLONE_BIT);
		intel_encoder->ddc_bus = intel_i2c_create(dev, GPIOE, "HDMIB");
		dev_priv->hotplug_supported_mask |= HDMIB_HOTPLUG_INT_STATUS;
	} else if (sdvox_reg == SDVOC) {
		intel_encoder->clone_mask = (1 << INTEL_HDMIC_CLONE_BIT);
		intel_encoder->ddc_bus = intel_i2c_create(dev, GPIOD, "HDMIC");
		dev_priv->hotplug_supported_mask |= HDMIC_HOTPLUG_INT_STATUS;
	} else if (sdvox_reg == HDMIB) {
		intel_encoder->clone_mask = (1 << INTEL_HDMID_CLONE_BIT);
		intel_encoder->ddc_bus = intel_i2c_create(dev, PCH_GPIOE,
								"HDMIB");
		dev_priv->hotplug_supported_mask |= HDMIB_HOTPLUG_INT_STATUS;
	} else if (sdvox_reg == HDMIC) {
		intel_encoder->clone_mask = (1 << INTEL_HDMIE_CLONE_BIT);
		intel_encoder->ddc_bus = intel_i2c_create(dev, PCH_GPIOD,
								"HDMIC");
		dev_priv->hotplug_supported_mask |= HDMIC_HOTPLUG_INT_STATUS;
	} else if (sdvox_reg == HDMID) {
		intel_encoder->clone_mask = (1 << INTEL_HDMIF_CLONE_BIT);
		intel_encoder->ddc_bus = intel_i2c_create(dev, PCH_GPIOF,
								"HDMID");
		dev_priv->hotplug_supported_mask |= HDMID_HOTPLUG_INT_STATUS;
	}
	if (!intel_encoder->ddc_bus)
		goto err_connector;

	intel_hdmi->sdvox_reg = sdvox_reg;

	drm_encoder_init(dev, &intel_encoder->enc, &intel_hdmi_enc_funcs,
			 DRM_MODE_ENCODER_TMDS);
	drm_encoder_helper_add(&intel_encoder->enc, &intel_hdmi_helper_funcs);

	drm_mode_connector_attach_encoder(&intel_connector->base,
					  &intel_encoder->enc);
	drm_sysfs_connector_add(connector);

	/* For G4X desktop chip, PEG_BAND_GAP_DATA 3:0 must first be written
	 * 0xd.  Failure to do so will result in spurious interrupts being
	 * generated on the port when a cable is not attached.
	 */
	if (IS_G4X(dev) && !IS_GM45(dev)) {
		u32 temp = I915_READ(PEG_BAND_GAP_DATA);
		I915_WRITE(PEG_BAND_GAP_DATA, (temp & ~0xf) | 0xd);
	}

	return;

err_connector:
	drm_connector_cleanup(connector);
	kfree(intel_hdmi);
	kfree(intel_connector);

	return;
}
