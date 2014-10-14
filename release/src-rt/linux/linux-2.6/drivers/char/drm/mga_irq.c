/* mga_irq.c -- IRQ handling for radeon -*- linux-c -*-
 *
 * Copyright (C) The Weather Channel, Inc.  2002.  All Rights Reserved.
 *
 * The Weather Channel (TM) funded Tungsten Graphics to develop the
 * initial release of the Radeon 8500 driver under the XFree86 license.
 * This notice must be preserved.
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Keith Whitwell <keith@tungstengraphics.com>
 *    Eric Anholt <anholt@FreeBSD.org>
 */

#include "drmP.h"
#include "drm.h"
#include "mga_drm.h"
#include "mga_drv.h"

irqreturn_t mga_driver_irq_handler(DRM_IRQ_ARGS)
{
	drm_device_t *dev = (drm_device_t *) arg;
	drm_mga_private_t *dev_priv = (drm_mga_private_t *) dev->dev_private;
	int status;
	int handled = 0;

	status = MGA_READ(MGA_STATUS);

	/* VBLANK interrupt */
	if (status & MGA_VLINEPEN) {
		MGA_WRITE(MGA_ICLEAR, MGA_VLINEICLR);
		atomic_inc(&dev->vbl_received);
		DRM_WAKEUP(&dev->vbl_queue);
		drm_vbl_send_signals(dev);
		handled = 1;
	}

	/* SOFTRAP interrupt */
	if (status & MGA_SOFTRAPEN) {
		const u32 prim_start = MGA_READ(MGA_PRIMADDRESS);
		const u32 prim_end = MGA_READ(MGA_PRIMEND);

		MGA_WRITE(MGA_ICLEAR, MGA_SOFTRAPICLR);

		/* In addition to clearing the interrupt-pending bit, we
		 * have to write to MGA_PRIMEND to re-start the DMA operation.
		 */
		if ((prim_start & ~0x03) != (prim_end & ~0x03)) {
			MGA_WRITE(MGA_PRIMEND, prim_end);
		}

		atomic_inc(&dev_priv->last_fence_retired);
		DRM_WAKEUP(&dev_priv->fence_queue);
		handled = 1;
	}

	if (handled) {
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

int mga_driver_vblank_wait(drm_device_t * dev, unsigned int *sequence)
{
	unsigned int cur_vblank;
	int ret = 0;

	/* Assume that the user has missed the current sequence number
	 * by about a day rather than she wants to wait for years
	 * using vertical blanks...
	 */
	DRM_WAIT_ON(ret, dev->vbl_queue, 3 * DRM_HZ,
		    (((cur_vblank = atomic_read(&dev->vbl_received))
		      - *sequence) <= (1 << 23)));

	*sequence = cur_vblank;

	return ret;
}

int mga_driver_fence_wait(drm_device_t * dev, unsigned int *sequence)
{
	drm_mga_private_t *dev_priv = (drm_mga_private_t *) dev->dev_private;
	unsigned int cur_fence;
	int ret = 0;

	/* Assume that the user has missed the current sequence number
	 * by about a day rather than she wants to wait for years
	 * using fences.
	 */
	DRM_WAIT_ON(ret, dev_priv->fence_queue, 3 * DRM_HZ,
		    (((cur_fence = atomic_read(&dev_priv->last_fence_retired))
		      - *sequence) <= (1 << 23)));

	*sequence = cur_fence;

	return ret;
}

void mga_driver_irq_preinstall(drm_device_t * dev)
{
	drm_mga_private_t *dev_priv = (drm_mga_private_t *) dev->dev_private;

	/* Disable *all* interrupts */
	MGA_WRITE(MGA_IEN, 0);
	/* Clear bits if they're already high */
	MGA_WRITE(MGA_ICLEAR, ~0);
}

void mga_driver_irq_postinstall(drm_device_t * dev)
{
	drm_mga_private_t *dev_priv = (drm_mga_private_t *) dev->dev_private;

	DRM_INIT_WAITQUEUE(&dev_priv->fence_queue);

	/* Turn on vertical blank interrupt and soft trap interrupt. */
	MGA_WRITE(MGA_IEN, MGA_VLINEIEN | MGA_SOFTRAPEN);
}

void mga_driver_irq_uninstall(drm_device_t * dev)
{
	drm_mga_private_t *dev_priv = (drm_mga_private_t *) dev->dev_private;
	if (!dev_priv)
		return;

	/* Disable *all* interrupts */
	MGA_WRITE(MGA_IEN, 0);

	dev->irq_enabled = 0;
}
