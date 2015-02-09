/* radeon_state.c -- State support for Radeon -*- linux-c -*- */
/*
 * Copyright 2000 VA Linux Systems, Inc., Fremont, California.
 * All Rights Reserved.
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
 *    Gareth Hughes <gareth@valinux.com>
 *    Kevin E. Martin <martin@valinux.com>
 */

#include "drmP.h"
#include "drm.h"
#include "drm_buffer.h"
#include "drm_sarea.h"
#include "radeon_drm.h"
#include "radeon_drv.h"

/* ================================================================
 * Helper functions for client state checking and fixup
 */

static __inline__ int radeon_check_and_fixup_offset(drm_radeon_private_t *
						    dev_priv,
						    struct drm_file * file_priv,
						    u32 *offset)
{
	u64 off = *offset;
	u32 fb_end = dev_priv->fb_location + dev_priv->fb_size - 1;
	struct drm_radeon_driver_file_fields *radeon_priv;

	/* Hrm ... the story of the offset ... So this function converts
	 * the various ideas of what userland clients might have for an
	 * offset in the card address space into an offset into the card
	 * address space :) So with a sane client, it should just keep
	 * the value intact and just do some boundary checking. However,
	 * not all clients are sane. Some older clients pass us 0 based
	 * offsets relative to the start of the framebuffer and some may
	 * assume the AGP aperture it appended to the framebuffer, so we
	 * try to detect those cases and fix them up.
	 *
	 * Note: It might be a good idea here to make sure the offset lands
	 * in some "allowed" area to protect things like the PCIE GART...
	 */

	/* First, the best case, the offset already lands in either the
	 * framebuffer or the GART mapped space
	 */
	if (radeon_check_offset(dev_priv, off))
		return 0;

	/* Ok, that didn't happen... now check if we have a zero based
	 * offset that fits in the framebuffer + gart space, apply the
	 * magic offset we get from SETPARAM or calculated from fb_location
	 */
	if (off < (dev_priv->fb_size + dev_priv->gart_size)) {
		radeon_priv = file_priv->driver_priv;
		off += radeon_priv->radeon_fb_delta;
	}

	/* Finally, assume we aimed at a GART offset if beyond the fb */
	if (off > fb_end)
		off = off - fb_end - 1 + dev_priv->gart_vm_start;

	/* Now recheck and fail if out of bounds */
	if (radeon_check_offset(dev_priv, off)) {
		DRM_DEBUG("offset fixed up to 0x%x\n", (unsigned int)off);
		*offset = off;
		return 0;
	}
	return -EINVAL;
}

static __inline__ int radeon_check_and_fixup_packets(drm_radeon_private_t *
						     dev_priv,
						     struct drm_file *file_priv,
						     int id, struct drm_buffer *buf)
{
	u32 *data;
	switch (id) {

	case RADEON_EMIT_PP_MISC:
		data = drm_buffer_pointer_to_dword(buf,
			(RADEON_RB3D_DEPTHOFFSET - RADEON_PP_MISC) / 4);

		if (radeon_check_and_fixup_offset(dev_priv, file_priv, data)) {
			DRM_ERROR("Invalid depth buffer offset\n");
			return -EINVAL;
		}
		dev_priv->have_z_offset = 1;
		break;

	case RADEON_EMIT_PP_CNTL:
		data = drm_buffer_pointer_to_dword(buf,
			(RADEON_RB3D_COLOROFFSET - RADEON_PP_CNTL) / 4);

		if (radeon_check_and_fixup_offset(dev_priv, file_priv, data)) {
			DRM_ERROR("Invalid colour buffer offset\n");
			return -EINVAL;
		}
		break;

	case R200_EMIT_PP_TXOFFSET_0:
	case R200_EMIT_PP_TXOFFSET_1:
	case R200_EMIT_PP_TXOFFSET_2:
	case R200_EMIT_PP_TXOFFSET_3:
	case R200_EMIT_PP_TXOFFSET_4:
	case R200_EMIT_PP_TXOFFSET_5:
		data = drm_buffer_pointer_to_dword(buf, 0);
		if (radeon_check_and_fixup_offset(dev_priv, file_priv, data)) {
			DRM_ERROR("Invalid R200 texture offset\n");
			return -EINVAL;
		}
		break;

	case RADEON_EMIT_PP_TXFILTER_0:
	case RADEON_EMIT_PP_TXFILTER_1:
	case RADEON_EMIT_PP_TXFILTER_2:
		data = drm_buffer_pointer_to_dword(buf,
			(RADEON_PP_TXOFFSET_0 - RADEON_PP_TXFILTER_0) / 4);
		if (radeon_check_and_fixup_offset(dev_priv, file_priv, data)) {
			DRM_ERROR("Invalid R100 texture offset\n");
			return -EINVAL;
		}
		break;

	case R200_EMIT_PP_CUBIC_OFFSETS_0:
	case R200_EMIT_PP_CUBIC_OFFSETS_1:
	case R200_EMIT_PP_CUBIC_OFFSETS_2:
	case R200_EMIT_PP_CUBIC_OFFSETS_3:
	case R200_EMIT_PP_CUBIC_OFFSETS_4:
	case R200_EMIT_PP_CUBIC_OFFSETS_5:{
			int i;
			for (i = 0; i < 5; i++) {
				data = drm_buffer_pointer_to_dword(buf, i);
				if (radeon_check_and_fixup_offset(dev_priv,
								  file_priv,
								  data)) {
					DRM_ERROR
					    ("Invalid R200 cubic texture offset\n");
					return -EINVAL;
				}
			}
			break;
		}

	case RADEON_EMIT_PP_CUBIC_OFFSETS_T0:
	case RADEON_EMIT_PP_CUBIC_OFFSETS_T1:
	case RADEON_EMIT_PP_CUBIC_OFFSETS_T2:{
			int i;
			for (i = 0; i < 5; i++) {
				data = drm_buffer_pointer_to_dword(buf, i);
				if (radeon_check_and_fixup_offset(dev_priv,
								  file_priv,
								  data)) {
					DRM_ERROR
					    ("Invalid R100 cubic texture offset\n");
					return -EINVAL;
				}
			}
		}
		break;

	case R200_EMIT_VAP_CTL:{
			RING_LOCALS;
			BEGIN_RING(2);
			OUT_RING_REG(RADEON_SE_TCL_STATE_FLUSH, 0);
			ADVANCE_RING();
		}
		break;

	case RADEON_EMIT_RB3D_COLORPITCH:
	case RADEON_EMIT_RE_LINE_PATTERN:
	case RADEON_EMIT_SE_LINE_WIDTH:
	case RADEON_EMIT_PP_LUM_MATRIX:
	case RADEON_EMIT_PP_ROT_MATRIX_0:
	case RADEON_EMIT_RB3D_STENCILREFMASK:
	case RADEON_EMIT_SE_VPORT_XSCALE:
	case RADEON_EMIT_SE_CNTL:
	case RADEON_EMIT_SE_CNTL_STATUS:
	case RADEON_EMIT_RE_MISC:
	case RADEON_EMIT_PP_BORDER_COLOR_0:
	case RADEON_EMIT_PP_BORDER_COLOR_1:
	case RADEON_EMIT_PP_BORDER_COLOR_2:
	case RADEON_EMIT_SE_ZBIAS_FACTOR:
	case RADEON_EMIT_SE_TCL_OUTPUT_VTX_FMT:
	case RADEON_EMIT_SE_TCL_MATERIAL_EMMISSIVE_RED:
	case R200_EMIT_PP_TXCBLEND_0:
	case R200_EMIT_PP_TXCBLEND_1:
	case R200_EMIT_PP_TXCBLEND_2:
	case R200_EMIT_PP_TXCBLEND_3:
	case R200_EMIT_PP_TXCBLEND_4:
	case R200_EMIT_PP_TXCBLEND_5:
	case R200_EMIT_PP_TXCBLEND_6:
	case R200_EMIT_PP_TXCBLEND_7:
	case R200_EMIT_TCL_LIGHT_MODEL_CTL_0:
	case R200_EMIT_TFACTOR_0:
	case R200_EMIT_VTX_FMT_0:
	case R200_EMIT_MATRIX_SELECT_0:
	case R200_EMIT_TEX_PROC_CTL_2:
	case R200_EMIT_TCL_UCP_VERT_BLEND_CTL:
	case R200_EMIT_PP_TXFILTER_0:
	case R200_EMIT_PP_TXFILTER_1:
	case R200_EMIT_PP_TXFILTER_2:
	case R200_EMIT_PP_TXFILTER_3:
	case R200_EMIT_PP_TXFILTER_4:
	case R200_EMIT_PP_TXFILTER_5:
	case R200_EMIT_VTE_CNTL:
	case R200_EMIT_OUTPUT_VTX_COMP_SEL:
	case R200_EMIT_PP_TAM_DEBUG3:
	case R200_EMIT_PP_CNTL_X:
	case R200_EMIT_RB3D_DEPTHXY_OFFSET:
	case R200_EMIT_RE_AUX_SCISSOR_CNTL:
	case R200_EMIT_RE_SCISSOR_TL_0:
	case R200_EMIT_RE_SCISSOR_TL_1:
	case R200_EMIT_RE_SCISSOR_TL_2:
	case R200_EMIT_SE_VAP_CNTL_STATUS:
	case R200_EMIT_SE_VTX_STATE_CNTL:
	case R200_EMIT_RE_POINTSIZE:
	case R200_EMIT_TCL_INPUT_VTX_VECTOR_ADDR_0:
	case R200_EMIT_PP_CUBIC_FACES_0:
	case R200_EMIT_PP_CUBIC_FACES_1:
	case R200_EMIT_PP_CUBIC_FACES_2:
	case R200_EMIT_PP_CUBIC_FACES_3:
	case R200_EMIT_PP_CUBIC_FACES_4:
	case R200_EMIT_PP_CUBIC_FACES_5:
	case RADEON_EMIT_PP_TEX_SIZE_0:
	case RADEON_EMIT_PP_TEX_SIZE_1:
	case RADEON_EMIT_PP_TEX_SIZE_2:
	case R200_EMIT_RB3D_BLENDCOLOR:
	case R200_EMIT_TCL_POINT_SPRITE_CNTL:
	case RADEON_EMIT_PP_CUBIC_FACES_0:
	case RADEON_EMIT_PP_CUBIC_FACES_1:
	case RADEON_EMIT_PP_CUBIC_FACES_2:
	case R200_EMIT_PP_TRI_PERF_CNTL:
	case R200_EMIT_PP_AFS_0:
	case R200_EMIT_PP_AFS_1:
	case R200_EMIT_ATF_TFACTOR:
	case R200_EMIT_PP_TXCTLALL_0:
	case R200_EMIT_PP_TXCTLALL_1:
	case R200_EMIT_PP_TXCTLALL_2:
	case R200_EMIT_PP_TXCTLALL_3:
	case R200_EMIT_PP_TXCTLALL_4:
	case R200_EMIT_PP_TXCTLALL_5:
	case R200_EMIT_VAP_PVS_CNTL:
		/* These packets don't contain memory offsets */
		break;

	default:
		DRM_ERROR("Unknown state packet ID %d\n", id);
		return -EINVAL;
	}

	return 0;
}

static __inline__ int radeon_check_and_fixup_packet3(drm_radeon_private_t *
						     dev_priv,
						     struct drm_file *file_priv,
						     drm_radeon_kcmd_buffer_t *
						     cmdbuf,
						     unsigned int *cmdsz)
{
	u32 *cmd = drm_buffer_pointer_to_dword(cmdbuf->buffer, 0);
	u32 offset, narrays;
	int count, i, k;

	count = ((*cmd & RADEON_CP_PACKET_COUNT_MASK) >> 16);
	*cmdsz = 2 + count;

	if ((*cmd & 0xc0000000) != RADEON_CP_PACKET3) {
		DRM_ERROR("Not a type 3 packet\n");
		return -EINVAL;
	}

	if (4 * *cmdsz > drm_buffer_unprocessed(cmdbuf->buffer)) {
		DRM_ERROR("Packet size larger than size of data provided\n");
		return -EINVAL;
	}

	switch (*cmd & 0xff00) {

	case RADEON_3D_DRAW_IMMD:
	case RADEON_3D_DRAW_VBUF:
	case RADEON_3D_DRAW_INDX:
	case RADEON_WAIT_FOR_IDLE:
	case RADEON_CP_NOP:
	case RADEON_3D_CLEAR_ZMASK:
/*	case RADEON_CP_NEXT_CHAR:
	case RADEON_CP_PLY_NEXTSCAN:
	case RADEON_CP_SET_SCISSORS: */ /* probably safe but will never need them? */
		/* these packets are safe */
		break;

	case RADEON_CP_3D_DRAW_IMMD_2:
	case RADEON_CP_3D_DRAW_VBUF_2:
	case RADEON_CP_3D_DRAW_INDX_2:
	case RADEON_3D_CLEAR_HIZ:
		/* safe but r200 only */
		if (dev_priv->microcode_version != UCODE_R200) {
			DRM_ERROR("Invalid 3d packet for r100-class chip\n");
			return -EINVAL;
		}
		break;

	case RADEON_3D_LOAD_VBPNTR:

		if (count > 18) { /* 12 arrays max */
			DRM_ERROR("Too large payload in 3D_LOAD_VBPNTR (count=%d)\n",
				  count);
			return -EINVAL;
		}

		/* carefully check packet contents */
		cmd = drm_buffer_pointer_to_dword(cmdbuf->buffer, 1);

		narrays = *cmd & ~0xc000;
		k = 0;
		i = 2;
		while ((k < narrays) && (i < (count + 2))) {
			i++;		/* skip attribute field */
			cmd = drm_buffer_pointer_to_dword(cmdbuf->buffer, i);
			if (radeon_check_and_fixup_offset(dev_priv, file_priv,
							  cmd)) {
				DRM_ERROR
				    ("Invalid offset (k=%d i=%d) in 3D_LOAD_VBPNTR packet.\n",
				     k, i);
				return -EINVAL;
			}
			k++;
			i++;
			if (k == narrays)
				break;
			/* have one more to process, they come in pairs */
			cmd = drm_buffer_pointer_to_dword(cmdbuf->buffer, i);

			if (radeon_check_and_fixup_offset(dev_priv,
							  file_priv, cmd))
			{
				DRM_ERROR
				    ("Invalid offset (k=%d i=%d) in 3D_LOAD_VBPNTR packet.\n",
				     k, i);
				return -EINVAL;
			}
			k++;
			i++;
		}
		/* do the counts match what we expect ? */
		if ((k != narrays) || (i != (count + 2))) {
			DRM_ERROR
			    ("Malformed 3D_LOAD_VBPNTR packet (k=%d i=%d narrays=%d count+1=%d).\n",
			      k, i, narrays, count + 1);
			return -EINVAL;
		}
		break;

	case RADEON_3D_RNDR_GEN_INDX_PRIM:
		if (dev_priv->microcode_version != UCODE_R100) {
			DRM_ERROR("Invalid 3d packet for r200-class chip\n");
			return -EINVAL;
		}

		cmd = drm_buffer_pointer_to_dword(cmdbuf->buffer, 1);
		if (radeon_check_and_fixup_offset(dev_priv, file_priv, cmd)) {
				DRM_ERROR("Invalid rndr_gen_indx offset\n");
				return -EINVAL;
		}
		break;

	case RADEON_CP_INDX_BUFFER:
		if (dev_priv->microcode_version != UCODE_R200) {
			DRM_ERROR("Invalid 3d packet for r100-class chip\n");
			return -EINVAL;
		}

		cmd = drm_buffer_pointer_to_dword(cmdbuf->buffer, 1);
		if ((*cmd & 0x8000ffff) != 0x80000810) {
			DRM_ERROR("Invalid indx_buffer reg address %08X\n", *cmd);
			return -EINVAL;
		}
		cmd = drm_buffer_pointer_to_dword(cmdbuf->buffer, 2);
		if (radeon_check_and_fixup_offset(dev_priv, file_priv, cmd)) {
			DRM_ERROR("Invalid indx_buffer offset is %08X\n", *cmd);
			return -EINVAL;
		}
		break;

	case RADEON_CNTL_HOSTDATA_BLT:
	case RADEON_CNTL_PAINT_MULTI:
	case RADEON_CNTL_BITBLT_MULTI:
		/* MSB of opcode: next DWORD GUI_CNTL */
		cmd = drm_buffer_pointer_to_dword(cmdbuf->buffer, 1);
		if (*cmd & (RADEON_GMC_SRC_PITCH_OFFSET_CNTL
			      | RADEON_GMC_DST_PITCH_OFFSET_CNTL)) {
			u32 *cmd2 = drm_buffer_pointer_to_dword(cmdbuf->buffer, 2);
			offset = *cmd2 << 10;
			if (radeon_check_and_fixup_offset
			    (dev_priv, file_priv, &offset)) {
				DRM_ERROR("Invalid first packet offset\n");
				return -EINVAL;
			}
			*cmd2 = (*cmd2 & 0xffc00000) | offset >> 10;
		}

		if ((*cmd & RADEON_GMC_SRC_PITCH_OFFSET_CNTL) &&
		    (*cmd & RADEON_GMC_DST_PITCH_OFFSET_CNTL)) {
			u32 *cmd3 = drm_buffer_pointer_to_dword(cmdbuf->buffer, 3);
			offset = *cmd3 << 10;
			if (radeon_check_and_fixup_offset
			    (dev_priv, file_priv, &offset)) {
				DRM_ERROR("Invalid second packet offset\n");
				return -EINVAL;
			}
			*cmd3 = (*cmd3 & 0xffc00000) | offset >> 10;
		}
		break;

	default:
		DRM_ERROR("Invalid packet type %x\n", *cmd & 0xff00);
		return -EINVAL;
	}

	return 0;
}

/* ================================================================
 * CP hardware state programming functions
 */

static __inline__ void radeon_emit_clip_rect(drm_radeon_private_t * dev_priv,
					     struct drm_clip_rect * box)
{
	RING_LOCALS;

	DRM_DEBUG("   box:  x1=%d y1=%d  x2=%d y2=%d\n",
		  box->x1, box->y1, box->x2, box->y2);

	BEGIN_RING(4);
	OUT_RING(CP_PACKET0(RADEON_RE_TOP_LEFT, 0));
	OUT_RING((box->y1 << 16) | box->x1);
	OUT_RING(CP_PACKET0(RADEON_RE_WIDTH_HEIGHT, 0));
	OUT_RING(((box->y2 - 1) << 16) | (box->x2 - 1));
	ADVANCE_RING();
}

/* Emit 1.1 state
 */
static int radeon_emit_state(drm_radeon_private_t * dev_priv,
			     struct drm_file *file_priv,
			     drm_radeon_context_regs_t * ctx,
			     drm_radeon_texture_regs_t * tex,
			     unsigned int dirty)
{
	RING_LOCALS;
	DRM_DEBUG("dirty=0x%08x\n", dirty);

	if (dirty & RADEON_UPLOAD_CONTEXT) {
		if (radeon_check_and_fixup_offset(dev_priv, file_priv,
						  &ctx->rb3d_depthoffset)) {
			DRM_ERROR("Invalid depth buffer offset\n");
			return -EINVAL;
		}

		if (radeon_check_and_fixup_offset(dev_priv, file_priv,
						  &ctx->rb3d_coloroffset)) {
			DRM_ERROR("Invalid depth buffer offset\n");
			return -EINVAL;
		}

		BEGIN_RING(14);
		OUT_RING(CP_PACKET0(RADEON_PP_MISC, 6));
		OUT_RING(ctx->pp_misc);
		OUT_RING(ctx->pp_fog_color);
		OUT_RING(ctx->re_solid_color);
		OUT_RING(ctx->rb3d_blendcntl);
		OUT_RING(ctx->rb3d_depthoffset);
		OUT_RING(ctx->rb3d_depthpitch);
		OUT_RING(ctx->rb3d_zstencilcntl);
		OUT_RING(CP_PACKET0(RADEON_PP_CNTL, 2));
		OUT_RING(ctx->pp_cntl);
		OUT_RING(ctx->rb3d_cntl);
		OUT_RING(ctx->rb3d_coloroffset);
		OUT_RING(CP_PACKET0(RADEON_RB3D_COLORPITCH, 0));
		OUT_RING(ctx->rb3d_colorpitch);
		ADVANCE_RING();
	}

	if (dirty & RADEON_UPLOAD_VERTFMT) {
		BEGIN_RING(2);
		OUT_RING(CP_PACKET0(RADEON_SE_COORD_FMT, 0));
		OUT_RING(ctx->se_coord_fmt);
		ADVANCE_RING();
	}

	if (dirty & RADEON_UPLOAD_LINE) {
		BEGIN_RING(5);
		OUT_RING(CP_PACKET0(RADEON_RE_LINE_PATTERN, 1));
		OUT_RING(ctx->re_line_pattern);
		OUT_RING(ctx->re_line_state);
		OUT_RING(CP_PACKET0(RADEON_SE_LINE_WIDTH, 0));
		OUT_RING(ctx->se_line_width);
		ADVANCE_RING();
	}

	if (dirty & RADEON_UPLOAD_BUMPMAP) {
		BEGIN_RING(5);
		OUT_RING(CP_PACKET0(RADEON_PP_LUM_MATRIX, 0));
		OUT_RING(ctx->pp_lum_matrix);
		OUT_RING(CP_PACKET0(RADEON_PP_ROT_MATRIX_0, 1));
		OUT_RING(ctx->pp_rot_matrix_0);
		OUT_RING(ctx->pp_rot_matrix_1);
		ADVANCE_RING();
	}

	if (dirty & RADEON_UPLOAD_MASKS) {
		BEGIN_RING(4);
		OUT_RING(CP_PACKET0(RADEON_RB3D_STENCILREFMASK, 2));
		OUT_RING(ctx->rb3d_stencilrefmask);
		OUT_RING(ctx->rb3d_ropcntl);
		OUT_RING(ctx->rb3d_planemask);
		ADVANCE_RING();
	}

	if (dirty & RADEON_UPLOAD_VIEWPORT) {
		BEGIN_RING(7);
		OUT_RING(CP_PACKET0(RADEON_SE_VPORT_XSCALE, 5));
		OUT_RING(ctx->se_vport_xscale);
		OUT_RING(ctx->se_vport_xoffset);
		OUT_RING(ctx->se_vport_yscale);
		OUT_RING(ctx->se_vport_yoffset);
		OUT_RING(ctx->se_vport_zscale);
		OUT_RING(ctx->se_vport_zoffset);
		ADVANCE_RING();
	}

	if (dirty & RADEON_UPLOAD_SETUP) {
		BEGIN_RING(4);
		OUT_RING(CP_PACKET0(RADEON_SE_CNTL, 0));
		OUT_RING(ctx->se_cntl);
		OUT_RING(CP_PACKET0(RADEON_SE_CNTL_STATUS, 0));
		OUT_RING(ctx->se_cntl_status);
		ADVANCE_RING();
	}

	if (dirty & RADEON_UPLOAD_MISC) {
		BEGIN_RING(2);
		OUT_RING(CP_PACKET0(RADEON_RE_MISC, 0));
		OUT_RING(ctx->re_misc);
		ADVANCE_RING();
	}

	if (dirty & RADEON_UPLOAD_TEX0) {
		if (radeon_check_and_fixup_offset(dev_priv, file_priv,
						  &tex[0].pp_txoffset)) {
			DRM_ERROR("Invalid texture offset for unit 0\n");
			return -EINVAL;
		}

		BEGIN_RING(9);
		OUT_RING(CP_PACKET0(RADEON_PP_TXFILTER_0, 5));
		OUT_RING(tex[0].pp_txfilter);
		OUT_RING(tex[0].pp_txformat);
		OUT_RING(tex[0].pp_txoffset);
		OUT_RING(tex[0].pp_txcblend);
		OUT_RING(tex[0].pp_txablend);
		OUT_RING(tex[0].pp_tfactor);
		OUT_RING(CP_PACKET0(RADEON_PP_BORDER_COLOR_0, 0));
		OUT_RING(tex[0].pp_border_color);
		ADVANCE_RING();
	}

	if (dirty & RADEON_UPLOAD_TEX1) {
		if (radeon_check_and_fixup_offset(dev_priv, file_priv,
						  &tex[1].pp_txoffset)) {
			DRM_ERROR("Invalid texture offset for unit 1\n");
			return -EINVAL;
		}

		BEGIN_RING(9);
		OUT_RING(CP_PACKET0(RADEON_PP_TXFILTER_1, 5));
		OUT_RING(tex[1].pp_txfilter);
		OUT_RING(tex[1].pp_txformat);
		OUT_RING(tex[1].pp_txoffset);
		OUT_RING(tex[1].pp_txcblend);
		OUT_RING(tex[1].pp_txablend);
		OUT_RING(tex[1].pp_tfactor);
		OUT_RING(CP_PACKET0(RADEON_PP_BORDER_COLOR_1, 0));
		OUT_RING(tex[1].pp_border_color);
		ADVANCE_RING();
	}

	if (dirty & RADEON_UPLOAD_TEX2) {
		if (radeon_check_and_fixup_offset(dev_priv, file_priv,
						  &tex[2].pp_txoffset)) {
			DRM_ERROR("Invalid texture offset for unit 2\n");
			return -EINVAL;
		}

		BEGIN_RING(9);
		OUT_RING(CP_PACKET0(RADEON_PP_TXFILTER_2, 5));
		OUT_RING(tex[2].pp_txfilter);
		OUT_RING(tex[2].pp_txformat);
		OUT_RING(tex[2].pp_txoffset);
		OUT_RING(tex[2].pp_txcblend);
		OUT_RING(tex[2].pp_txablend);
		OUT_RING(tex[2].pp_tfactor);
		OUT_RING(CP_PACKET0(RADEON_PP_BORDER_COLOR_2, 0));
		OUT_RING(tex[2].pp_border_color);
		ADVANCE_RING();
	}

	return 0;
}

/* Emit 1.2 state
 */
static int radeon_emit_state2(drm_radeon_private_t * dev_priv,
			      struct drm_file *file_priv,
			      drm_radeon_state_t * state)
{
	RING_LOCALS;

	if (state->dirty & RADEON_UPLOAD_ZBIAS) {
		BEGIN_RING(3);
		OUT_RING(CP_PACKET0(RADEON_SE_ZBIAS_FACTOR, 1));
		OUT_RING(state->context2.se_zbias_factor);
		OUT_RING(state->context2.se_zbias_constant);
		ADVANCE_RING();
	}

	return radeon_emit_state(dev_priv, file_priv, &state->context,
				 state->tex, state->dirty);
}

/* New (1.3) state mechanism.  3 commands (packet, scalar, vector) in
 * 1.3 cmdbuffers allow all previous state to be updated as well as
 * the tcl scalar and vector areas.
 */
static struct {
	int start;
	int len;
	const char *name;
} packet[RADEON_MAX_STATE_PACKETS] = {
	{RADEON_PP_MISC, 7, "RADEON_PP_MISC"},
	{RADEON_PP_CNTL, 3, "RADEON_PP_CNTL"},
	{RADEON_RB3D_COLORPITCH, 1, "RADEON_RB3D_COLORPITCH"},
	{RADEON_RE_LINE_PATTERN, 2, "RADEON_RE_LINE_PATTERN"},
	{RADEON_SE_LINE_WIDTH, 1, "RADEON_SE_LINE_WIDTH"},
	{RADEON_PP_LUM_MATRIX, 1, "RADEON_PP_LUM_MATRIX"},
	{RADEON_PP_ROT_MATRIX_0, 2, "RADEON_PP_ROT_MATRIX_0"},
	{RADEON_RB3D_STENCILREFMASK, 3, "RADEON_RB3D_STENCILREFMASK"},
	{RADEON_SE_VPORT_XSCALE, 6, "RADEON_SE_VPORT_XSCALE"},
	{RADEON_SE_CNTL, 2, "RADEON_SE_CNTL"},
	{RADEON_SE_CNTL_STATUS, 1, "RADEON_SE_CNTL_STATUS"},
	{RADEON_RE_MISC, 1, "RADEON_RE_MISC"},
	{RADEON_PP_TXFILTER_0, 6, "RADEON_PP_TXFILTER_0"},
	{RADEON_PP_BORDER_COLOR_0, 1, "RADEON_PP_BORDER_COLOR_0"},
	{RADEON_PP_TXFILTER_1, 6, "RADEON_PP_TXFILTER_1"},
	{RADEON_PP_BORDER_COLOR_1, 1, "RADEON_PP_BORDER_COLOR_1"},
	{RADEON_PP_TXFILTER_2, 6, "RADEON_PP_TXFILTER_2"},
	{RADEON_PP_BORDER_COLOR_2, 1, "RADEON_PP_BORDER_COLOR_2"},
	{RADEON_SE_ZBIAS_FACTOR, 2, "RADEON_SE_ZBIAS_FACTOR"},
	{RADEON_SE_TCL_OUTPUT_VTX_FMT, 11, "RADEON_SE_TCL_OUTPUT_VTX_FMT"},
	{RADEON_SE_TCL_MATERIAL_EMMISSIVE_RED, 17,
		    "RADEON_SE_TCL_MATERIAL_EMMISSIVE_RED"},
	{R200_PP_TXCBLEND_0, 4, "R200_PP_TXCBLEND_0"},
	{R200_PP_TXCBLEND_1, 4, "R200_PP_TXCBLEND_1"},
	{R200_PP_TXCBLEND_2, 4, "R200_PP_TXCBLEND_2"},
	{R200_PP_TXCBLEND_3, 4, "R200_PP_TXCBLEND_3"},
	{R200_PP_TXCBLEND_4, 4, "R200_PP_TXCBLEND_4"},
	{R200_PP_TXCBLEND_5, 4, "R200_PP_TXCBLEND_5"},
	{R200_PP_TXCBLEND_6, 4, "R200_PP_TXCBLEND_6"},
	{R200_PP_TXCBLEND_7, 4, "R200_PP_TXCBLEND_7"},
	{R200_SE_TCL_LIGHT_MODEL_CTL_0, 6, "R200_SE_TCL_LIGHT_MODEL_CTL_0"},
	{R200_PP_TFACTOR_0, 6, "R200_PP_TFACTOR_0"},
	{R200_SE_VTX_FMT_0, 4, "R200_SE_VTX_FMT_0"},
	{R200_SE_VAP_CNTL, 1, "R200_SE_VAP_CNTL"},
	{R200_SE_TCL_MATRIX_SEL_0, 5, "R200_SE_TCL_MATRIX_SEL_0"},
	{R200_SE_TCL_TEX_PROC_CTL_2, 5, "R200_SE_TCL_TEX_PROC_CTL_2"},
	{R200_SE_TCL_UCP_VERT_BLEND_CTL, 1, "R200_SE_TCL_UCP_VERT_BLEND_CTL"},
	{R200_PP_TXFILTER_0, 6, "R200_PP_TXFILTER_0"},
	{R200_PP_TXFILTER_1, 6, "R200_PP_TXFILTER_1"},
	{R200_PP_TXFILTER_2, 6, "R200_PP_TXFILTER_2"},
	{R200_PP_TXFILTER_3, 6, "R200_PP_TXFILTER_3"},
	{R200_PP_TXFILTER_4, 6, "R200_PP_TXFILTER_4"},
	{R200_PP_TXFILTER_5, 6, "R200_PP_TXFILTER_5"},
	{R200_PP_TXOFFSET_0, 1, "R200_PP_TXOFFSET_0"},
	{R200_PP_TXOFFSET_1, 1, "R200_PP_TXOFFSET_1"},
	{R200_PP_TXOFFSET_2, 1, "R200_PP_TXOFFSET_2"},
	{R200_PP_TXOFFSET_3, 1, "R200_PP_TXOFFSET_3"},
	{R200_PP_TXOFFSET_4, 1, "R200_PP_TXOFFSET_4"},
	{R200_PP_TXOFFSET_5, 1, "R200_PP_TXOFFSET_5"},
	{R200_SE_VTE_CNTL, 1, "R200_SE_VTE_CNTL"},
	{R200_SE_TCL_OUTPUT_VTX_COMP_SEL, 1,
	 "R200_SE_TCL_OUTPUT_VTX_COMP_SEL"},
	{R200_PP_TAM_DEBUG3, 1, "R200_PP_TAM_DEBUG3"},
	{R200_PP_CNTL_X, 1, "R200_PP_CNTL_X"},
	{R200_RB3D_DEPTHXY_OFFSET, 1, "R200_RB3D_DEPTHXY_OFFSET"},
	{R200_RE_AUX_SCISSOR_CNTL, 1, "R200_RE_AUX_SCISSOR_CNTL"},
	{R200_RE_SCISSOR_TL_0, 2, "R200_RE_SCISSOR_TL_0"},
	{R200_RE_SCISSOR_TL_1, 2, "R200_RE_SCISSOR_TL_1"},
	{R200_RE_SCISSOR_TL_2, 2, "R200_RE_SCISSOR_TL_2"},
	{R200_SE_VAP_CNTL_STATUS, 1, "R200_SE_VAP_CNTL_STATUS"},
	{R200_SE_VTX_STATE_CNTL, 1, "R200_SE_VTX_STATE_CNTL"},
	{R200_RE_POINTSIZE, 1, "R200_RE_POINTSIZE"},
	{R200_SE_TCL_INPUT_VTX_VECTOR_ADDR_0, 4,
		    "R200_SE_TCL_INPUT_VTX_VECTOR_ADDR_0"},
	{R200_PP_CUBIC_FACES_0, 1, "R200_PP_CUBIC_FACES_0"},	/* 61 */
	{R200_PP_CUBIC_OFFSET_F1_0, 5, "R200_PP_CUBIC_OFFSET_F1_0"}, /* 62 */
	{R200_PP_CUBIC_FACES_1, 1, "R200_PP_CUBIC_FACES_1"},
	{R200_PP_CUBIC_OFFSET_F1_1, 5, "R200_PP_CUBIC_OFFSET_F1_1"},
	{R200_PP_CUBIC_FACES_2, 1, "R200_PP_CUBIC_FACES_2"},
	{R200_PP_CUBIC_OFFSET_F1_2, 5, "R200_PP_CUBIC_OFFSET_F1_2"},
	{R200_PP_CUBIC_FACES_3, 1, "R200_PP_CUBIC_FACES_3"},
	{R200_PP_CUBIC_OFFSET_F1_3, 5, "R200_PP_CUBIC_OFFSET_F1_3"},
	{R200_PP_CUBIC_FACES_4, 1, "R200_PP_CUBIC_FACES_4"},
	{R200_PP_CUBIC_OFFSET_F1_4, 5, "R200_PP_CUBIC_OFFSET_F1_4"},
	{R200_PP_CUBIC_FACES_5, 1, "R200_PP_CUBIC_FACES_5"},
	{R200_PP_CUBIC_OFFSET_F1_5, 5, "R200_PP_CUBIC_OFFSET_F1_5"},
	{RADEON_PP_TEX_SIZE_0, 2, "RADEON_PP_TEX_SIZE_0"},
	{RADEON_PP_TEX_SIZE_1, 2, "RADEON_PP_TEX_SIZE_1"},
	{RADEON_PP_TEX_SIZE_2, 2, "RADEON_PP_TEX_SIZE_2"},
	{R200_RB3D_BLENDCOLOR, 3, "R200_RB3D_BLENDCOLOR"},
	{R200_SE_TCL_POINT_SPRITE_CNTL, 1, "R200_SE_TCL_POINT_SPRITE_CNTL"},
	{RADEON_PP_CUBIC_FACES_0, 1, "RADEON_PP_CUBIC_FACES_0"},
	{RADEON_PP_CUBIC_OFFSET_T0_0, 5, "RADEON_PP_CUBIC_OFFSET_T0_0"},
	{RADEON_PP_CUBIC_FACES_1, 1, "RADEON_PP_CUBIC_FACES_1"},
	{RADEON_PP_CUBIC_OFFSET_T1_0, 5, "RADEON_PP_CUBIC_OFFSET_T1_0"},
	{RADEON_PP_CUBIC_FACES_2, 1, "RADEON_PP_CUBIC_FACES_2"},
	{RADEON_PP_CUBIC_OFFSET_T2_0, 5, "RADEON_PP_CUBIC_OFFSET_T2_0"},
	{R200_PP_TRI_PERF, 2, "R200_PP_TRI_PERF"},
	{R200_PP_AFS_0, 32, "R200_PP_AFS_0"},     /* 85 */
	{R200_PP_AFS_1, 32, "R200_PP_AFS_1"},
	{R200_PP_TFACTOR_0, 8, "R200_ATF_TFACTOR"},
	{R200_PP_TXFILTER_0, 8, "R200_PP_TXCTLALL_0"},
	{R200_PP_TXFILTER_1, 8, "R200_PP_TXCTLALL_1"},
	{R200_PP_TXFILTER_2, 8, "R200_PP_TXCTLALL_2"},
	{R200_PP_TXFILTER_3, 8, "R200_PP_TXCTLALL_3"},
	{R200_PP_TXFILTER_4, 8, "R200_PP_TXCTLALL_4"},
	{R200_PP_TXFILTER_5, 8, "R200_PP_TXCTLALL_5"},
	{R200_VAP_PVS_CNTL_1, 2, "R200_VAP_PVS_CNTL"},
};

/* ================================================================
 * Performance monitoring functions
 */

static void radeon_clear_box(drm_radeon_private_t * dev_priv,
			     struct drm_radeon_master_private *master_priv,
			     int x, int y, int w, int h, int r, int g, int b)
{
	u32 color;
	RING_LOCALS;

	x += master_priv->sarea_priv->boxes[0].x1;
	y += master_priv->sarea_priv->boxes[0].y1;

	switch (dev_priv->color_fmt) {
	case RADEON_COLOR_FORMAT_RGB565:
		color = (((r & 0xf8) << 8) |
			 ((g & 0xfc) << 3) | ((b & 0xf8) >> 3));
		break;
	case RADEON_COLOR_FORMAT_ARGB8888:
	default:
		color = (((0xff) << 24) | (r << 16) | (g << 8) | b);
		break;
	}

	BEGIN_RING(4);
	RADEON_WAIT_UNTIL_3D_IDLE();
	OUT_RING(CP_PACKET0(RADEON_DP_WRITE_MASK, 0));
	OUT_RING(0xffffffff);
	ADVANCE_RING();

	BEGIN_RING(6);

	OUT_RING(CP_PACKET3(RADEON_CNTL_PAINT_MULTI, 4));
	OUT_RING(RADEON_GMC_DST_PITCH_OFFSET_CNTL |
		 RADEON_GMC_BRUSH_SOLID_COLOR |
		 (dev_priv->color_fmt << 8) |
		 RADEON_GMC_SRC_DATATYPE_COLOR |
		 RADEON_ROP3_P | RADEON_GMC_CLR_CMP_CNTL_DIS);

	if (master_priv->sarea_priv->pfCurrentPage == 1) {
		OUT_RING(dev_priv->front_pitch_offset);
	} else {
		OUT_RING(dev_priv->back_pitch_offset);
	}

	OUT_RING(color);

	OUT_RING((x << 16) | y);
	OUT_RING((w << 16) | h);

	ADVANCE_RING();
}

static void radeon_cp_performance_boxes(drm_radeon_private_t *dev_priv, struct drm_radeon_master_private *master_priv)
{
	/* Collapse various things into a wait flag -- trying to
	 * guess if userspase slept -- better just to have them tell us.
	 */
	if (dev_priv->stats.last_frame_reads > 1 ||
	    dev_priv->stats.last_clear_reads > dev_priv->stats.clears) {
		dev_priv->stats.boxes |= RADEON_BOX_WAIT_IDLE;
	}

	if (dev_priv->stats.freelist_loops) {
		dev_priv->stats.boxes |= RADEON_BOX_WAIT_IDLE;
	}

	/* Purple box for page flipping
	 */
	if (dev_priv->stats.boxes & RADEON_BOX_FLIP)
		radeon_clear_box(dev_priv, master_priv, 4, 4, 8, 8, 255, 0, 255);

	/* Red box if we have to wait for idle at any point
	 */
	if (dev_priv->stats.boxes & RADEON_BOX_WAIT_IDLE)
		radeon_clear_box(dev_priv, master_priv, 16, 4, 8, 8, 255, 0, 0);

	/* Blue box: lost context?
	 */

	/* Yellow box for texture swaps
	 */
	if (dev_priv->stats.boxes & RADEON_BOX_TEXTURE_LOAD)
		radeon_clear_box(dev_priv, master_priv, 40, 4, 8, 8, 255, 255, 0);

	/* Green box if hardware never idles (as far as we can tell)
	 */
	if (!(dev_priv->stats.boxes & RADEON_BOX_DMA_IDLE))
		radeon_clear_box(dev_priv, master_priv, 64, 4, 8, 8, 0, 255, 0);

	/* Draw bars indicating number of buffers allocated
	 * (not a great measure, easily confused)
	 */
	if (dev_priv->stats.requested_bufs) {
		if (dev_priv->stats.requested_bufs > 100)
			dev_priv->stats.requested_bufs = 100;

		radeon_clear_box(dev_priv, master_priv, 4, 16,
				 dev_priv->stats.requested_bufs, 4,
				 196, 128, 128);
	}

	memset(&dev_priv->stats, 0, sizeof(dev_priv->stats));

}

/* ================================================================
 * CP command dispatch functions
 */

static void radeon_cp_dispatch_clear(struct drm_device * dev,
				     struct drm_master *master,
				     drm_radeon_clear_t * clear,
				     drm_radeon_clear_rect_t * depth_boxes)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = master->driver_priv;
	drm_radeon_sarea_t *sarea_priv = master_priv->sarea_priv;
	drm_radeon_depth_clear_t *depth_clear = &dev_priv->depth_clear;
	int nbox = sarea_priv->nbox;
	struct drm_clip_rect *pbox = sarea_priv->boxes;
	unsigned int flags = clear->flags;
	u32 rb3d_cntl = 0, rb3d_stencilrefmask = 0;
	int i;
	RING_LOCALS;
	DRM_DEBUG("flags = 0x%x\n", flags);

	dev_priv->stats.clears++;

	if (sarea_priv->pfCurrentPage == 1) {
		unsigned int tmp = flags;

		flags &= ~(RADEON_FRONT | RADEON_BACK);
		if (tmp & RADEON_FRONT)
			flags |= RADEON_BACK;
		if (tmp & RADEON_BACK)
			flags |= RADEON_FRONT;
	}
	if (flags & (RADEON_DEPTH|RADEON_STENCIL)) {
		if (!dev_priv->have_z_offset) {
			printk_once(KERN_ERR "radeon: illegal depth clear request. Buggy mesa detected - please update.\n");
			flags &= ~(RADEON_DEPTH | RADEON_STENCIL);
		}
	}

	if (flags & (RADEON_FRONT | RADEON_BACK)) {

		BEGIN_RING(4);

		/* Ensure the 3D stream is idle before doing a
		 * 2D fill to clear the front or back buffer.
		 */
		RADEON_WAIT_UNTIL_3D_IDLE();

		OUT_RING(CP_PACKET0(RADEON_DP_WRITE_MASK, 0));
		OUT_RING(clear->color_mask);

		ADVANCE_RING();

		/* Make sure we restore the 3D state next time.
		 */
		sarea_priv->ctx_owner = 0;

		for (i = 0; i < nbox; i++) {
			int x = pbox[i].x1;
			int y = pbox[i].y1;
			int w = pbox[i].x2 - x;
			int h = pbox[i].y2 - y;

			DRM_DEBUG("%d,%d-%d,%d flags 0x%x\n",
				  x, y, w, h, flags);

			if (flags & RADEON_FRONT) {
				BEGIN_RING(6);

				OUT_RING(CP_PACKET3
					 (RADEON_CNTL_PAINT_MULTI, 4));
				OUT_RING(RADEON_GMC_DST_PITCH_OFFSET_CNTL |
					 RADEON_GMC_BRUSH_SOLID_COLOR |
					 (dev_priv->
					  color_fmt << 8) |
					 RADEON_GMC_SRC_DATATYPE_COLOR |
					 RADEON_ROP3_P |
					 RADEON_GMC_CLR_CMP_CNTL_DIS);

				OUT_RING(dev_priv->front_pitch_offset);
				OUT_RING(clear->clear_color);

				OUT_RING((x << 16) | y);
				OUT_RING((w << 16) | h);

				ADVANCE_RING();
			}

			if (flags & RADEON_BACK) {
				BEGIN_RING(6);

				OUT_RING(CP_PACKET3
					 (RADEON_CNTL_PAINT_MULTI, 4));
				OUT_RING(RADEON_GMC_DST_PITCH_OFFSET_CNTL |
					 RADEON_GMC_BRUSH_SOLID_COLOR |
					 (dev_priv->
					  color_fmt << 8) |
					 RADEON_GMC_SRC_DATATYPE_COLOR |
					 RADEON_ROP3_P |
					 RADEON_GMC_CLR_CMP_CNTL_DIS);

				OUT_RING(dev_priv->back_pitch_offset);
				OUT_RING(clear->clear_color);

				OUT_RING((x << 16) | y);
				OUT_RING((w << 16) | h);

				ADVANCE_RING();
			}
		}
	}

	/* hyper z clear */
	/* no docs available, based on reverse engeneering by Stephane Marchesin */
	if ((flags & (RADEON_DEPTH | RADEON_STENCIL))
	    && (flags & RADEON_CLEAR_FASTZ)) {

		int i;
		int depthpixperline =
		    dev_priv->depth_fmt ==
		    RADEON_DEPTH_FORMAT_16BIT_INT_Z ? (dev_priv->depth_pitch /
						       2) : (dev_priv->
							     depth_pitch / 4);

		u32 clearmask;

		u32 tempRB3D_DEPTHCLEARVALUE = clear->clear_depth |
		    ((clear->depth_mask & 0xff) << 24);

		/* Make sure we restore the 3D state next time.
		 * we haven't touched any "normal" state - still need this?
		 */
		sarea_priv->ctx_owner = 0;

		if ((dev_priv->flags & RADEON_HAS_HIERZ)
		    && (flags & RADEON_USE_HIERZ)) {
			/* pattern seems to work for r100, though get slight
			   rendering errors with glxgears. If hierz is not enabled for r100,
			   only 4 bits which indicate clear (15,16,31,32, all zero) matter, the
			   other ones are ignored, and the same clear mask can be used. That's
			   very different behaviour than R200 which needs different clear mask
			   and different number of tiles to clear if hierz is enabled or not !?!
			 */
			clearmask = (0xff << 22) | (0xff << 6) | 0x003f003f;
		} else {
			/* clear mask : chooses the clearing pattern.
			   rv250: could be used to clear only parts of macrotiles
			   (but that would get really complicated...)?
			   bit 0 and 1 (either or both of them ?!?!) are used to
			   not clear tile (or maybe one of the bits indicates if the tile is
			   compressed or not), bit 2 and 3 to not clear tile 1,...,.
			   Pattern is as follows:
			   | 0,1 | 4,5 | 8,9 |12,13|16,17|20,21|24,25|28,29|
			   bits -------------------------------------------------
			   | 2,3 | 6,7 |10,11|14,15|18,19|22,23|26,27|30,31|
			   rv100: clearmask covers 2x8 4x1 tiles, but one clear still
			   covers 256 pixels ?!?
			 */
			clearmask = 0x0;
		}

		BEGIN_RING(8);
		RADEON_WAIT_UNTIL_2D_IDLE();
		OUT_RING_REG(RADEON_RB3D_DEPTHCLEARVALUE,
			     tempRB3D_DEPTHCLEARVALUE);
		/* what offset is this exactly ? */
		OUT_RING_REG(RADEON_RB3D_ZMASKOFFSET, 0);
		/* need ctlstat, otherwise get some strange black flickering */
		OUT_RING_REG(RADEON_RB3D_ZCACHE_CTLSTAT,
			     RADEON_RB3D_ZC_FLUSH_ALL);
		ADVANCE_RING();

		for (i = 0; i < nbox; i++) {
			int tileoffset, nrtilesx, nrtilesy, j;
			/* it looks like r200 needs rv-style clears, at least if hierz is not enabled? */
			if ((dev_priv->flags & RADEON_HAS_HIERZ)
			    && !(dev_priv->microcode_version == UCODE_R200)) {
				tileoffset =
				    ((pbox[i].y1 >> 3) * depthpixperline +
				     pbox[i].x1) >> 6;
				nrtilesx =
				    ((pbox[i].x2 & ~63) -
				     (pbox[i].x1 & ~63)) >> 4;
				nrtilesy =
				    (pbox[i].y2 >> 3) - (pbox[i].y1 >> 3);
				for (j = 0; j <= nrtilesy; j++) {
					BEGIN_RING(4);
					OUT_RING(CP_PACKET3
						 (RADEON_3D_CLEAR_ZMASK, 2));
					/* first tile */
					OUT_RING(tileoffset * 8);
					/* the number of tiles to clear */
					OUT_RING(nrtilesx + 4);
					/* clear mask : chooses the clearing pattern. */
					OUT_RING(clearmask);
					ADVANCE_RING();
					tileoffset += depthpixperline >> 6;
				}
			} else if (dev_priv->microcode_version == UCODE_R200) {
				/* works for rv250. */
				/* find first macro tile (8x2 4x4 z-pixels on rv250) */
				tileoffset =
				    ((pbox[i].y1 >> 3) * depthpixperline +
				     pbox[i].x1) >> 5;
				nrtilesx =
				    (pbox[i].x2 >> 5) - (pbox[i].x1 >> 5);
				nrtilesy =
				    (pbox[i].y2 >> 3) - (pbox[i].y1 >> 3);
				for (j = 0; j <= nrtilesy; j++) {
					BEGIN_RING(4);
					OUT_RING(CP_PACKET3
						 (RADEON_3D_CLEAR_ZMASK, 2));
					/* first tile */
					/* judging by the first tile offset needed, could possibly
					   directly address/clear 4x4 tiles instead of 8x2 * 4x4
					   macro tiles, though would still need clear mask for
					   right/bottom if truly 4x4 granularity is desired ? */
					OUT_RING(tileoffset * 16);
					/* the number of tiles to clear */
					OUT_RING(nrtilesx + 1);
					/* clear mask : chooses the clearing pattern. */
					OUT_RING(clearmask);
					ADVANCE_RING();
					tileoffset += depthpixperline >> 5;
				}
			} else {	/* rv 100 */
				/* rv100 might not need 64 pix alignment, who knows */
				/* offsets are, hmm, weird */
				tileoffset =
				    ((pbox[i].y1 >> 4) * depthpixperline +
				     pbox[i].x1) >> 6;
				nrtilesx =
				    ((pbox[i].x2 & ~63) -
				     (pbox[i].x1 & ~63)) >> 4;
				nrtilesy =
				    (pbox[i].y2 >> 4) - (pbox[i].y1 >> 4);
				for (j = 0; j <= nrtilesy; j++) {
					BEGIN_RING(4);
					OUT_RING(CP_PACKET3
						 (RADEON_3D_CLEAR_ZMASK, 2));
					OUT_RING(tileoffset * 128);
					/* the number of tiles to clear */
					OUT_RING(nrtilesx + 4);
					/* clear mask : chooses the clearing pattern. */
					OUT_RING(clearmask);
					ADVANCE_RING();
					tileoffset += depthpixperline >> 6;
				}
			}
		}

		/* TODO don't always clear all hi-level z tiles */
		if ((dev_priv->flags & RADEON_HAS_HIERZ)
		    && (dev_priv->microcode_version == UCODE_R200)
		    && (flags & RADEON_USE_HIERZ))
			/* r100 and cards without hierarchical z-buffer have no high-level z-buffer */
		{
			BEGIN_RING(4);
			OUT_RING(CP_PACKET3(RADEON_3D_CLEAR_HIZ, 2));
			OUT_RING(0x0);	/* First tile */
			OUT_RING(0x3cc0);
			OUT_RING((0xff << 22) | (0xff << 6) | 0x003f003f);
			ADVANCE_RING();
		}
	}

	/* We have to clear the depth and/or stencil buffers by
	 * rendering a quad into just those buffers.  Thus, we have to
	 * make sure the 3D engine is configured correctly.
	 */
	else if ((dev_priv->microcode_version == UCODE_R200) &&
		(flags & (RADEON_DEPTH | RADEON_STENCIL))) {

		int tempPP_CNTL;
		int tempRE_CNTL;
		int tempRB3D_CNTL;
		int tempRB3D_ZSTENCILCNTL;
		int tempRB3D_STENCILREFMASK;
		int tempRB3D_PLANEMASK;
		int tempSE_CNTL;
		int tempSE_VTE_CNTL;
		int tempSE_VTX_FMT_0;
		int tempSE_VTX_FMT_1;
		int tempSE_VAP_CNTL;
		int tempRE_AUX_SCISSOR_CNTL;

		tempPP_CNTL = 0;
		tempRE_CNTL = 0;

		tempRB3D_CNTL = depth_clear->rb3d_cntl;

		tempRB3D_ZSTENCILCNTL = depth_clear->rb3d_zstencilcntl;
		tempRB3D_STENCILREFMASK = 0x0;

		tempSE_CNTL = depth_clear->se_cntl;

		/* Disable TCL */

		tempSE_VAP_CNTL = (	/* SE_VAP_CNTL__FORCE_W_TO_ONE_MASK |  */
					  (0x9 <<
					   SE_VAP_CNTL__VF_MAX_VTX_NUM__SHIFT));

		tempRB3D_PLANEMASK = 0x0;

		tempRE_AUX_SCISSOR_CNTL = 0x0;

		tempSE_VTE_CNTL =
		    SE_VTE_CNTL__VTX_XY_FMT_MASK | SE_VTE_CNTL__VTX_Z_FMT_MASK;

		/* Vertex format (X, Y, Z, W) */
		tempSE_VTX_FMT_0 =
		    SE_VTX_FMT_0__VTX_Z0_PRESENT_MASK |
		    SE_VTX_FMT_0__VTX_W0_PRESENT_MASK;
		tempSE_VTX_FMT_1 = 0x0;

		/*
		 * Depth buffer specific enables
		 */
		if (flags & RADEON_DEPTH) {
			/* Enable depth buffer */
			tempRB3D_CNTL |= RADEON_Z_ENABLE;
		} else {
			/* Disable depth buffer */
			tempRB3D_CNTL &= ~RADEON_Z_ENABLE;
		}

		/*
		 * Stencil buffer specific enables
		 */
		if (flags & RADEON_STENCIL) {
			tempRB3D_CNTL |= RADEON_STENCIL_ENABLE;
			tempRB3D_STENCILREFMASK = clear->depth_mask;
		} else {
			tempRB3D_CNTL &= ~RADEON_STENCIL_ENABLE;
			tempRB3D_STENCILREFMASK = 0x00000000;
		}

		if (flags & RADEON_USE_COMP_ZBUF) {
			tempRB3D_ZSTENCILCNTL |= RADEON_Z_COMPRESSION_ENABLE |
			    RADEON_Z_DECOMPRESSION_ENABLE;
		}
		if (flags & RADEON_USE_HIERZ) {
			tempRB3D_ZSTENCILCNTL |= RADEON_Z_HIERARCHY_ENABLE;
		}

		BEGIN_RING(26);
		RADEON_WAIT_UNTIL_2D_IDLE();

		OUT_RING_REG(RADEON_PP_CNTL, tempPP_CNTL);
		OUT_RING_REG(R200_RE_CNTL, tempRE_CNTL);
		OUT_RING_REG(RADEON_RB3D_CNTL, tempRB3D_CNTL);
		OUT_RING_REG(RADEON_RB3D_ZSTENCILCNTL, tempRB3D_ZSTENCILCNTL);
		OUT_RING_REG(RADEON_RB3D_STENCILREFMASK,
			     tempRB3D_STENCILREFMASK);
		OUT_RING_REG(RADEON_RB3D_PLANEMASK, tempRB3D_PLANEMASK);
		OUT_RING_REG(RADEON_SE_CNTL, tempSE_CNTL);
		OUT_RING_REG(R200_SE_VTE_CNTL, tempSE_VTE_CNTL);
		OUT_RING_REG(R200_SE_VTX_FMT_0, tempSE_VTX_FMT_0);
		OUT_RING_REG(R200_SE_VTX_FMT_1, tempSE_VTX_FMT_1);
		OUT_RING_REG(R200_SE_VAP_CNTL, tempSE_VAP_CNTL);
		OUT_RING_REG(R200_RE_AUX_SCISSOR_CNTL, tempRE_AUX_SCISSOR_CNTL);
		ADVANCE_RING();

		/* Make sure we restore the 3D state next time.
		 */
		sarea_priv->ctx_owner = 0;

		for (i = 0; i < nbox; i++) {

			/* Funny that this should be required --
			 *  sets top-left?
			 */
			radeon_emit_clip_rect(dev_priv, &sarea_priv->boxes[i]);

			BEGIN_RING(14);
			OUT_RING(CP_PACKET3(R200_3D_DRAW_IMMD_2, 12));
			OUT_RING((RADEON_PRIM_TYPE_RECT_LIST |
				  RADEON_PRIM_WALK_RING |
				  (3 << RADEON_NUM_VERTICES_SHIFT)));
			OUT_RING(depth_boxes[i].ui[CLEAR_X1]);
			OUT_RING(depth_boxes[i].ui[CLEAR_Y1]);
			OUT_RING(depth_boxes[i].ui[CLEAR_DEPTH]);
			OUT_RING(0x3f800000);
			OUT_RING(depth_boxes[i].ui[CLEAR_X1]);
			OUT_RING(depth_boxes[i].ui[CLEAR_Y2]);
			OUT_RING(depth_boxes[i].ui[CLEAR_DEPTH]);
			OUT_RING(0x3f800000);
			OUT_RING(depth_boxes[i].ui[CLEAR_X2]);
			OUT_RING(depth_boxes[i].ui[CLEAR_Y2]);
			OUT_RING(depth_boxes[i].ui[CLEAR_DEPTH]);
			OUT_RING(0x3f800000);
			ADVANCE_RING();
		}
	} else if ((flags & (RADEON_DEPTH | RADEON_STENCIL))) {

		int tempRB3D_ZSTENCILCNTL = depth_clear->rb3d_zstencilcntl;

		rb3d_cntl = depth_clear->rb3d_cntl;

		if (flags & RADEON_DEPTH) {
			rb3d_cntl |= RADEON_Z_ENABLE;
		} else {
			rb3d_cntl &= ~RADEON_Z_ENABLE;
		}

		if (flags & RADEON_STENCIL) {
			rb3d_cntl |= RADEON_STENCIL_ENABLE;
			rb3d_stencilrefmask = clear->depth_mask;	/* misnamed field */
		} else {
			rb3d_cntl &= ~RADEON_STENCIL_ENABLE;
			rb3d_stencilrefmask = 0x00000000;
		}

		if (flags & RADEON_USE_COMP_ZBUF) {
			tempRB3D_ZSTENCILCNTL |= RADEON_Z_COMPRESSION_ENABLE |
			    RADEON_Z_DECOMPRESSION_ENABLE;
		}
		if (flags & RADEON_USE_HIERZ) {
			tempRB3D_ZSTENCILCNTL |= RADEON_Z_HIERARCHY_ENABLE;
		}

		BEGIN_RING(13);
		RADEON_WAIT_UNTIL_2D_IDLE();

		OUT_RING(CP_PACKET0(RADEON_PP_CNTL, 1));
		OUT_RING(0x00000000);
		OUT_RING(rb3d_cntl);

		OUT_RING_REG(RADEON_RB3D_ZSTENCILCNTL, tempRB3D_ZSTENCILCNTL);
		OUT_RING_REG(RADEON_RB3D_STENCILREFMASK, rb3d_stencilrefmask);
		OUT_RING_REG(RADEON_RB3D_PLANEMASK, 0x00000000);
		OUT_RING_REG(RADEON_SE_CNTL, depth_clear->se_cntl);
		ADVANCE_RING();

		/* Make sure we restore the 3D state next time.
		 */
		sarea_priv->ctx_owner = 0;

		for (i = 0; i < nbox; i++) {

			/* Funny that this should be required --
			 *  sets top-left?
			 */
			radeon_emit_clip_rect(dev_priv, &sarea_priv->boxes[i]);

			BEGIN_RING(15);

			OUT_RING(CP_PACKET3(RADEON_3D_DRAW_IMMD, 13));
			OUT_RING(RADEON_VTX_Z_PRESENT |
				 RADEON_VTX_PKCOLOR_PRESENT);
			OUT_RING((RADEON_PRIM_TYPE_RECT_LIST |
				  RADEON_PRIM_WALK_RING |
				  RADEON_MAOS_ENABLE |
				  RADEON_VTX_FMT_RADEON_MODE |
				  (3 << RADEON_NUM_VERTICES_SHIFT)));

			OUT_RING(depth_boxes[i].ui[CLEAR_X1]);
			OUT_RING(depth_boxes[i].ui[CLEAR_Y1]);
			OUT_RING(depth_boxes[i].ui[CLEAR_DEPTH]);
			OUT_RING(0x0);

			OUT_RING(depth_boxes[i].ui[CLEAR_X1]);
			OUT_RING(depth_boxes[i].ui[CLEAR_Y2]);
			OUT_RING(depth_boxes[i].ui[CLEAR_DEPTH]);
			OUT_RING(0x0);

			OUT_RING(depth_boxes[i].ui[CLEAR_X2]);
			OUT_RING(depth_boxes[i].ui[CLEAR_Y2]);
			OUT_RING(depth_boxes[i].ui[CLEAR_DEPTH]);
			OUT_RING(0x0);

			ADVANCE_RING();
		}
	}

	/* Increment the clear counter.  The client-side 3D driver must
	 * wait on this value before performing the clear ioctl.  We
	 * need this because the card's so damned fast...
	 */
	sarea_priv->last_clear++;

	BEGIN_RING(4);

	RADEON_CLEAR_AGE(sarea_priv->last_clear);
	RADEON_WAIT_UNTIL_IDLE();

	ADVANCE_RING();
}

static void radeon_cp_dispatch_swap(struct drm_device *dev, struct drm_master *master)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = master->driver_priv;
	drm_radeon_sarea_t *sarea_priv = master_priv->sarea_priv;
	int nbox = sarea_priv->nbox;
	struct drm_clip_rect *pbox = sarea_priv->boxes;
	int i;
	RING_LOCALS;
	DRM_DEBUG("\n");

	/* Do some trivial performance monitoring...
	 */
	if (dev_priv->do_boxes)
		radeon_cp_performance_boxes(dev_priv, master_priv);

	/* Wait for the 3D stream to idle before dispatching the bitblt.
	 * This will prevent data corruption between the two streams.
	 */
	BEGIN_RING(2);

	RADEON_WAIT_UNTIL_3D_IDLE();

	ADVANCE_RING();

	for (i = 0; i < nbox; i++) {
		int x = pbox[i].x1;
		int y = pbox[i].y1;
		int w = pbox[i].x2 - x;
		int h = pbox[i].y2 - y;

		DRM_DEBUG("%d,%d-%d,%d\n", x, y, w, h);

		BEGIN_RING(9);

		OUT_RING(CP_PACKET0(RADEON_DP_GUI_MASTER_CNTL, 0));
		OUT_RING(RADEON_GMC_SRC_PITCH_OFFSET_CNTL |
			 RADEON_GMC_DST_PITCH_OFFSET_CNTL |
			 RADEON_GMC_BRUSH_NONE |
			 (dev_priv->color_fmt << 8) |
			 RADEON_GMC_SRC_DATATYPE_COLOR |
			 RADEON_ROP3_S |
			 RADEON_DP_SRC_SOURCE_MEMORY |
			 RADEON_GMC_CLR_CMP_CNTL_DIS | RADEON_GMC_WR_MSK_DIS);

		/* Make this work even if front & back are flipped:
		 */
		OUT_RING(CP_PACKET0(RADEON_SRC_PITCH_OFFSET, 1));
		if (sarea_priv->pfCurrentPage == 0) {
			OUT_RING(dev_priv->back_pitch_offset);
			OUT_RING(dev_priv->front_pitch_offset);
		} else {
			OUT_RING(dev_priv->front_pitch_offset);
			OUT_RING(dev_priv->back_pitch_offset);
		}

		OUT_RING(CP_PACKET0(RADEON_SRC_X_Y, 2));
		OUT_RING((x << 16) | y);
		OUT_RING((x << 16) | y);
		OUT_RING((w << 16) | h);

		ADVANCE_RING();
	}

	/* Increment the frame counter.  The client-side 3D driver must
	 * throttle the framerate by waiting for this value before
	 * performing the swapbuffer ioctl.
	 */
	sarea_priv->last_frame++;

	BEGIN_RING(4);

	RADEON_FRAME_AGE(sarea_priv->last_frame);
	RADEON_WAIT_UNTIL_2D_IDLE();

	ADVANCE_RING();
}

void radeon_cp_dispatch_flip(struct drm_device *dev, struct drm_master *master)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = master->driver_priv;
	struct drm_sarea *sarea = (struct drm_sarea *)master_priv->sarea->handle;
	int offset = (master_priv->sarea_priv->pfCurrentPage == 1)
	    ? dev_priv->front_offset : dev_priv->back_offset;
	RING_LOCALS;
	DRM_DEBUG("pfCurrentPage=%d\n",
		  master_priv->sarea_priv->pfCurrentPage);

	/* Do some trivial performance monitoring...
	 */
	if (dev_priv->do_boxes) {
		dev_priv->stats.boxes |= RADEON_BOX_FLIP;
		radeon_cp_performance_boxes(dev_priv, master_priv);
	}

	/* Update the frame offsets for both CRTCs
	 */
	BEGIN_RING(6);

	RADEON_WAIT_UNTIL_3D_IDLE();
	OUT_RING_REG(RADEON_CRTC_OFFSET,
		     ((sarea->frame.y * dev_priv->front_pitch +
		       sarea->frame.x * (dev_priv->color_fmt - 2)) & ~7)
		     + offset);
	OUT_RING_REG(RADEON_CRTC2_OFFSET, master_priv->sarea_priv->crtc2_base
		     + offset);

	ADVANCE_RING();

	/* Increment the frame counter.  The client-side 3D driver must
	 * throttle the framerate by waiting for this value before
	 * performing the swapbuffer ioctl.
	 */
	master_priv->sarea_priv->last_frame++;
	master_priv->sarea_priv->pfCurrentPage =
		1 - master_priv->sarea_priv->pfCurrentPage;

	BEGIN_RING(2);

	RADEON_FRAME_AGE(master_priv->sarea_priv->last_frame);

	ADVANCE_RING();
}

static int bad_prim_vertex_nr(int primitive, int nr)
{
	switch (primitive & RADEON_PRIM_TYPE_MASK) {
	case RADEON_PRIM_TYPE_NONE:
	case RADEON_PRIM_TYPE_POINT:
		return nr < 1;
	case RADEON_PRIM_TYPE_LINE:
		return (nr & 1) || nr == 0;
	case RADEON_PRIM_TYPE_LINE_STRIP:
		return nr < 2;
	case RADEON_PRIM_TYPE_TRI_LIST:
	case RADEON_PRIM_TYPE_3VRT_POINT_LIST:
	case RADEON_PRIM_TYPE_3VRT_LINE_LIST:
	case RADEON_PRIM_TYPE_RECT_LIST:
		return nr % 3 || nr == 0;
	case RADEON_PRIM_TYPE_TRI_FAN:
	case RADEON_PRIM_TYPE_TRI_STRIP:
		return nr < 3;
	default:
		return 1;
	}
}

typedef struct {
	unsigned int start;
	unsigned int finish;
	unsigned int prim;
	unsigned int numverts;
	unsigned int offset;
	unsigned int vc_format;
} drm_radeon_tcl_prim_t;

static void radeon_cp_dispatch_vertex(struct drm_device * dev,
				      struct drm_file *file_priv,
				      struct drm_buf * buf,
				      drm_radeon_tcl_prim_t * prim)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = file_priv->master->driver_priv;
	drm_radeon_sarea_t *sarea_priv = master_priv->sarea_priv;
	int offset = dev_priv->gart_buffers_offset + buf->offset + prim->start;
	int numverts = (int)prim->numverts;
	int nbox = sarea_priv->nbox;
	int i = 0;
	RING_LOCALS;

	DRM_DEBUG("hwprim 0x%x vfmt 0x%x %d..%d %d verts\n",
		  prim->prim,
		  prim->vc_format, prim->start, prim->finish, prim->numverts);

	if (bad_prim_vertex_nr(prim->prim, prim->numverts)) {
		DRM_ERROR("bad prim %x numverts %d\n",
			  prim->prim, prim->numverts);
		return;
	}

	do {
		/* Emit the next cliprect */
		if (i < nbox) {
			radeon_emit_clip_rect(dev_priv, &sarea_priv->boxes[i]);
		}

		/* Emit the vertex buffer rendering commands */
		BEGIN_RING(5);

		OUT_RING(CP_PACKET3(RADEON_3D_RNDR_GEN_INDX_PRIM, 3));
		OUT_RING(offset);
		OUT_RING(numverts);
		OUT_RING(prim->vc_format);
		OUT_RING(prim->prim | RADEON_PRIM_WALK_LIST |
			 RADEON_COLOR_ORDER_RGBA |
			 RADEON_VTX_FMT_RADEON_MODE |
			 (numverts << RADEON_NUM_VERTICES_SHIFT));

		ADVANCE_RING();

		i++;
	} while (i < nbox);
}

void radeon_cp_discard_buffer(struct drm_device *dev, struct drm_master *master, struct drm_buf *buf)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = master->driver_priv;
	drm_radeon_buf_priv_t *buf_priv = buf->dev_private;
	RING_LOCALS;

	buf_priv->age = ++master_priv->sarea_priv->last_dispatch;

	/* Emit the vertex buffer age */
	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600) {
		BEGIN_RING(3);
		R600_DISPATCH_AGE(buf_priv->age);
		ADVANCE_RING();
	} else {
		BEGIN_RING(2);
		RADEON_DISPATCH_AGE(buf_priv->age);
		ADVANCE_RING();
	}

	buf->pending = 1;
	buf->used = 0;
}

static void radeon_cp_dispatch_indirect(struct drm_device * dev,
					struct drm_buf * buf, int start, int end)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;
	DRM_DEBUG("buf=%d s=0x%x e=0x%x\n", buf->idx, start, end);

	if (start != end) {
		int offset = (dev_priv->gart_buffers_offset
			      + buf->offset + start);
		int dwords = (end - start + 3) / sizeof(u32);

		/* Indirect buffer data must be an even number of
		 * dwords, so if we've been given an odd number we must
		 * pad the data with a Type-2 CP packet.
		 */
		if (dwords & 1) {
			u32 *data = (u32 *)
			    ((char *)dev->agp_buffer_map->handle
			     + buf->offset + start);
			data[dwords++] = RADEON_CP_PACKET2;
		}

		/* Fire off the indirect buffer */
		BEGIN_RING(3);

		OUT_RING(CP_PACKET0(RADEON_CP_IB_BASE, 1));
		OUT_RING(offset);
		OUT_RING(dwords);

		ADVANCE_RING();
	}
}

static void radeon_cp_dispatch_indices(struct drm_device *dev,
				       struct drm_master *master,
				       struct drm_buf * elt_buf,
				       drm_radeon_tcl_prim_t * prim)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = master->driver_priv;
	drm_radeon_sarea_t *sarea_priv = master_priv->sarea_priv;
	int offset = dev_priv->gart_buffers_offset + prim->offset;
	u32 *data;
	int dwords;
	int i = 0;
	int start = prim->start + RADEON_INDEX_PRIM_OFFSET;
	int count = (prim->finish - start) / sizeof(u16);
	int nbox = sarea_priv->nbox;

	DRM_DEBUG("hwprim 0x%x vfmt 0x%x %d..%d offset: %x nr %d\n",
		  prim->prim,
		  prim->vc_format,
		  prim->start, prim->finish, prim->offset, prim->numverts);

	if (bad_prim_vertex_nr(prim->prim, count)) {
		DRM_ERROR("bad prim %x count %d\n", prim->prim, count);
		return;
	}

	if (start >= prim->finish || (prim->start & 0x7)) {
		DRM_ERROR("buffer prim %d\n", prim->prim);
		return;
	}

	dwords = (prim->finish - prim->start + 3) / sizeof(u32);

	data = (u32 *) ((char *)dev->agp_buffer_map->handle +
			elt_buf->offset + prim->start);

	data[0] = CP_PACKET3(RADEON_3D_RNDR_GEN_INDX_PRIM, dwords - 2);
	data[1] = offset;
	data[2] = prim->numverts;
	data[3] = prim->vc_format;
	data[4] = (prim->prim |
		   RADEON_PRIM_WALK_IND |
		   RADEON_COLOR_ORDER_RGBA |
		   RADEON_VTX_FMT_RADEON_MODE |
		   (count << RADEON_NUM_VERTICES_SHIFT));

	do {
		if (i < nbox)
			radeon_emit_clip_rect(dev_priv, &sarea_priv->boxes[i]);

		radeon_cp_dispatch_indirect(dev, elt_buf,
					    prim->start, prim->finish);

		i++;
	} while (i < nbox);

}

#define RADEON_MAX_TEXTURE_SIZE RADEON_BUFFER_SIZE

static int radeon_cp_dispatch_texture(struct drm_device * dev,
				      struct drm_file *file_priv,
				      drm_radeon_texture_t * tex,
				      drm_radeon_tex_image_t * image)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_buf *buf;
	u32 format;
	u32 *buffer;
	const u8 __user *data;
	int size, dwords, tex_width, blit_width, spitch;
	u32 height;
	int i;
	u32 texpitch, microtile;
	u32 offset, byte_offset;
	RING_LOCALS;

	if (radeon_check_and_fixup_offset(dev_priv, file_priv, &tex->offset)) {
		DRM_ERROR("Invalid destination offset\n");
		return -EINVAL;
	}

	dev_priv->stats.boxes |= RADEON_BOX_TEXTURE_LOAD;

	/* Flush the pixel cache.  This ensures no pixel data gets mixed
	 * up with the texture data from the host data blit, otherwise
	 * part of the texture image may be corrupted.
	 */
	BEGIN_RING(4);
	RADEON_FLUSH_CACHE();
	RADEON_WAIT_UNTIL_IDLE();
	ADVANCE_RING();

	/* The compiler won't optimize away a division by a variable,
	 * even if the only legal values are powers of two.  Thus, we'll
	 * use a shift instead.
	 */
	switch (tex->format) {
	case RADEON_TXFORMAT_ARGB8888:
	case RADEON_TXFORMAT_RGBA8888:
		format = RADEON_COLOR_FORMAT_ARGB8888;
		tex_width = tex->width * 4;
		blit_width = image->width * 4;
		break;
	case RADEON_TXFORMAT_AI88:
	case RADEON_TXFORMAT_ARGB1555:
	case RADEON_TXFORMAT_RGB565:
	case RADEON_TXFORMAT_ARGB4444:
	case RADEON_TXFORMAT_VYUY422:
	case RADEON_TXFORMAT_YVYU422:
		format = RADEON_COLOR_FORMAT_RGB565;
		tex_width = tex->width * 2;
		blit_width = image->width * 2;
		break;
	case RADEON_TXFORMAT_I8:
	case RADEON_TXFORMAT_RGB332:
		format = RADEON_COLOR_FORMAT_CI8;
		tex_width = tex->width * 1;
		blit_width = image->width * 1;
		break;
	default:
		DRM_ERROR("invalid texture format %d\n", tex->format);
		return -EINVAL;
	}
	spitch = blit_width >> 6;
	if (spitch == 0 && image->height > 1)
		return -EINVAL;

	texpitch = tex->pitch;
	if ((texpitch << 22) & RADEON_DST_TILE_MICRO) {
		microtile = 1;
		if (tex_width < 64) {
			texpitch &= ~(RADEON_DST_TILE_MICRO >> 22);
			/* we got tiled coordinates, untile them */
			image->x *= 2;
		}
	} else
		microtile = 0;

	/* this might fail for zero-sized uploads - are those illegal? */
	if (!radeon_check_offset(dev_priv, tex->offset + image->height *
				blit_width - 1)) {
		DRM_ERROR("Invalid final destination offset\n");
		return -EINVAL;
	}

	DRM_DEBUG("tex=%dx%d blit=%d\n", tex_width, tex->height, blit_width);

	do {
		DRM_DEBUG("tex: ofs=0x%x p=%d f=%d x=%hd y=%hd w=%hd h=%hd\n",
			  tex->offset >> 10, tex->pitch, tex->format,
			  image->x, image->y, image->width, image->height);

		/* Make a copy of some parameters in case we have to
		 * update them for a multi-pass texture blit.
		 */
		height = image->height;
		data = (const u8 __user *)image->data;

		size = height * blit_width;

		if (size > RADEON_MAX_TEXTURE_SIZE) {
			height = RADEON_MAX_TEXTURE_SIZE / blit_width;
			size = height * blit_width;
		} else if (size < 4 && size > 0) {
			size = 4;
		} else if (size == 0) {
			return 0;
		}

		buf = radeon_freelist_get(dev);
		if (0 && !buf) {
			radeon_do_cp_idle(dev_priv);
			buf = radeon_freelist_get(dev);
		}
		if (!buf) {
			DRM_DEBUG("EAGAIN\n");
			if (DRM_COPY_TO_USER(tex->image, image, sizeof(*image)))
				return -EFAULT;
			return -EAGAIN;
		}

		/* Dispatch the indirect buffer.
		 */
		buffer =
		    (u32 *) ((char *)dev->agp_buffer_map->handle + buf->offset);
		dwords = size / 4;

#define RADEON_COPY_MT(_buf, _data, _width) \
	do { \
		if (DRM_COPY_FROM_USER(_buf, _data, (_width))) {\
			DRM_ERROR("EFAULT on pad, %d bytes\n", (_width)); \
			return -EFAULT; \
		} \
	} while(0)

		if (microtile) {
			/* texture micro tiling in use, minimum texture width is thus 16 bytes.
			   however, we cannot use blitter directly for texture width < 64 bytes,
			   since minimum tex pitch is 64 bytes and we need this to match
			   the texture width, otherwise the blitter will tile it wrong.
			   Thus, tiling manually in this case. Additionally, need to special
			   case tex height = 1, since our actual image will have height 2
			   and we need to ensure we don't read beyond the texture size
			   from user space. */
			if (tex->height == 1) {
				if (tex_width >= 64 || tex_width <= 16) {
					RADEON_COPY_MT(buffer, data,
						(int)(tex_width * sizeof(u32)));
				} else if (tex_width == 32) {
					RADEON_COPY_MT(buffer, data, 16);
					RADEON_COPY_MT(buffer + 8,
						       data + 16, 16);
				}
			} else if (tex_width >= 64 || tex_width == 16) {
				RADEON_COPY_MT(buffer, data,
					       (int)(dwords * sizeof(u32)));
			} else if (tex_width < 16) {
				for (i = 0; i < tex->height; i++) {
					RADEON_COPY_MT(buffer, data, tex_width);
					buffer += 4;
					data += tex_width;
				}
			} else if (tex_width == 32) {
				/* TODO: make sure this works when not fitting in one buffer
				   (i.e. 32bytes x 2048...) */
				for (i = 0; i < tex->height; i += 2) {
					RADEON_COPY_MT(buffer, data, 16);
					data += 16;
					RADEON_COPY_MT(buffer + 8, data, 16);
					data += 16;
					RADEON_COPY_MT(buffer + 4, data, 16);
					data += 16;
					RADEON_COPY_MT(buffer + 12, data, 16);
					data += 16;
					buffer += 16;
				}
			}
		} else {
			if (tex_width >= 32) {
				/* Texture image width is larger than the minimum, so we
				 * can upload it directly.
				 */
				RADEON_COPY_MT(buffer, data,
					       (int)(dwords * sizeof(u32)));
			} else {
				/* Texture image width is less than the minimum, so we
				 * need to pad out each image scanline to the minimum
				 * width.
				 */
				for (i = 0; i < tex->height; i++) {
					RADEON_COPY_MT(buffer, data, tex_width);
					buffer += 8;
					data += tex_width;
				}
			}
		}

#undef RADEON_COPY_MT
		byte_offset = (image->y & ~2047) * blit_width;
		buf->file_priv = file_priv;
		buf->used = size;
		offset = dev_priv->gart_buffers_offset + buf->offset;
		BEGIN_RING(9);
		OUT_RING(CP_PACKET3(RADEON_CNTL_BITBLT_MULTI, 5));
		OUT_RING(RADEON_GMC_SRC_PITCH_OFFSET_CNTL |
			 RADEON_GMC_DST_PITCH_OFFSET_CNTL |
			 RADEON_GMC_BRUSH_NONE |
			 (format << 8) |
			 RADEON_GMC_SRC_DATATYPE_COLOR |
			 RADEON_ROP3_S |
			 RADEON_DP_SRC_SOURCE_MEMORY |
			 RADEON_GMC_CLR_CMP_CNTL_DIS | RADEON_GMC_WR_MSK_DIS);
		OUT_RING((spitch << 22) | (offset >> 10));
		OUT_RING((texpitch << 22) | ((tex->offset >> 10) + (byte_offset >> 10)));
		OUT_RING(0);
		OUT_RING((image->x << 16) | (image->y % 2048));
		OUT_RING((image->width << 16) | height);
		RADEON_WAIT_UNTIL_2D_IDLE();
		ADVANCE_RING();
		COMMIT_RING();

		radeon_cp_discard_buffer(dev, file_priv->master, buf);

		/* Update the input parameters for next time */
		image->y += height;
		image->height -= height;
		image->data = (const u8 __user *)image->data + size;
	} while (image->height > 0);

	/* Flush the pixel cache after the blit completes.  This ensures
	 * the texture data is written out to memory before rendering
	 * continues.
	 */
	BEGIN_RING(4);
	RADEON_FLUSH_CACHE();
	RADEON_WAIT_UNTIL_2D_IDLE();
	ADVANCE_RING();
	COMMIT_RING();

	return 0;
}

static void radeon_cp_dispatch_stipple(struct drm_device * dev, u32 * stipple)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int i;
	RING_LOCALS;
	DRM_DEBUG("\n");

	BEGIN_RING(35);

	OUT_RING(CP_PACKET0(RADEON_RE_STIPPLE_ADDR, 0));
	OUT_RING(0x00000000);

	OUT_RING(CP_PACKET0_TABLE(RADEON_RE_STIPPLE_DATA, 31));
	for (i = 0; i < 32; i++) {
		OUT_RING(stipple[i]);
	}

	ADVANCE_RING();
}

static void radeon_apply_surface_regs(int surf_index,
				      drm_radeon_private_t *dev_priv)
{
	if (!dev_priv->mmio)
		return;

	radeon_do_cp_idle(dev_priv);

	RADEON_WRITE(RADEON_SURFACE0_INFO + 16 * surf_index,
		     dev_priv->surfaces[surf_index].flags);
	RADEON_WRITE(RADEON_SURFACE0_LOWER_BOUND + 16 * surf_index,
		     dev_priv->surfaces[surf_index].lower);
	RADEON_WRITE(RADEON_SURFACE0_UPPER_BOUND + 16 * surf_index,
		     dev_priv->surfaces[surf_index].upper);
}

/* Allocates a virtual surface
 * doesn't always allocate a real surface, will stretch an existing
 * surface when possible.
 *
 * Note that refcount can be at most 2, since during a free refcount=3
 * might mean we have to allocate a new surface which might not always
 * be available.
 * For example : we allocate three contiguous surfaces ABC. If B is
 * freed, we suddenly need two surfaces to store A and C, which might
 * not always be available.
 */
static int alloc_surface(drm_radeon_surface_alloc_t *new,
			 drm_radeon_private_t *dev_priv,
			 struct drm_file *file_priv)
{
	struct radeon_virt_surface *s;
	int i;
	int virt_surface_index;
	uint32_t new_upper, new_lower;

	new_lower = new->address;
	new_upper = new_lower + new->size - 1;

	/* sanity check */
	if ((new_lower >= new_upper) || (new->flags == 0) || (new->size == 0) ||
	    ((new_upper & RADEON_SURF_ADDRESS_FIXED_MASK) !=
	     RADEON_SURF_ADDRESS_FIXED_MASK)
	    || ((new_lower & RADEON_SURF_ADDRESS_FIXED_MASK) != 0))
		return -1;

	/* make sure there is no overlap with existing surfaces */
	for (i = 0; i < RADEON_MAX_SURFACES; i++) {
		if ((dev_priv->surfaces[i].refcount != 0) &&
		    (((new_lower >= dev_priv->surfaces[i].lower) &&
		      (new_lower < dev_priv->surfaces[i].upper)) ||
		     ((new_lower < dev_priv->surfaces[i].lower) &&
		      (new_upper > dev_priv->surfaces[i].lower)))) {
			return -1;
		}
	}

	/* find a virtual surface */
	for (i = 0; i < 2 * RADEON_MAX_SURFACES; i++)
		if (dev_priv->virt_surfaces[i].file_priv == NULL)
			break;
	if (i == 2 * RADEON_MAX_SURFACES) {
		return -1;
	}
	virt_surface_index = i;

	/* try to reuse an existing surface */
	for (i = 0; i < RADEON_MAX_SURFACES; i++) {
		/* extend before */
		if ((dev_priv->surfaces[i].refcount == 1) &&
		    (new->flags == dev_priv->surfaces[i].flags) &&
		    (new_upper + 1 == dev_priv->surfaces[i].lower)) {
			s = &(dev_priv->virt_surfaces[virt_surface_index]);
			s->surface_index = i;
			s->lower = new_lower;
			s->upper = new_upper;
			s->flags = new->flags;
			s->file_priv = file_priv;
			dev_priv->surfaces[i].refcount++;
			dev_priv->surfaces[i].lower = s->lower;
			radeon_apply_surface_regs(s->surface_index, dev_priv);
			return virt_surface_index;
		}

		/* extend after */
		if ((dev_priv->surfaces[i].refcount == 1) &&
		    (new->flags == dev_priv->surfaces[i].flags) &&
		    (new_lower == dev_priv->surfaces[i].upper + 1)) {
			s = &(dev_priv->virt_surfaces[virt_surface_index]);
			s->surface_index = i;
			s->lower = new_lower;
			s->upper = new_upper;
			s->flags = new->flags;
			s->file_priv = file_priv;
			dev_priv->surfaces[i].refcount++;
			dev_priv->surfaces[i].upper = s->upper;
			radeon_apply_surface_regs(s->surface_index, dev_priv);
			return virt_surface_index;
		}
	}

	/* okay, we need a new one */
	for (i = 0; i < RADEON_MAX_SURFACES; i++) {
		if (dev_priv->surfaces[i].refcount == 0) {
			s = &(dev_priv->virt_surfaces[virt_surface_index]);
			s->surface_index = i;
			s->lower = new_lower;
			s->upper = new_upper;
			s->flags = new->flags;
			s->file_priv = file_priv;
			dev_priv->surfaces[i].refcount = 1;
			dev_priv->surfaces[i].lower = s->lower;
			dev_priv->surfaces[i].upper = s->upper;
			dev_priv->surfaces[i].flags = s->flags;
			radeon_apply_surface_regs(s->surface_index, dev_priv);
			return virt_surface_index;
		}
	}

	/* we didn't find anything */
	return -1;
}

static int free_surface(struct drm_file *file_priv,
			drm_radeon_private_t * dev_priv,
			int lower)
{
	struct radeon_virt_surface *s;
	int i;
	/* find the virtual surface */
	for (i = 0; i < 2 * RADEON_MAX_SURFACES; i++) {
		s = &(dev_priv->virt_surfaces[i]);
		if (s->file_priv) {
			if ((lower == s->lower) && (file_priv == s->file_priv))
			{
				if (dev_priv->surfaces[s->surface_index].
				    lower == s->lower)
					dev_priv->surfaces[s->surface_index].
					    lower = s->upper;

				if (dev_priv->surfaces[s->surface_index].
				    upper == s->upper)
					dev_priv->surfaces[s->surface_index].
					    upper = s->lower;

				dev_priv->surfaces[s->surface_index].refcount--;
				if (dev_priv->surfaces[s->surface_index].
				    refcount == 0)
					dev_priv->surfaces[s->surface_index].
					    flags = 0;
				s->file_priv = NULL;
				radeon_apply_surface_regs(s->surface_index,
							  dev_priv);
				return 0;
			}
		}
	}
	return 1;
}

static void radeon_surfaces_release(struct drm_file *file_priv,
				    drm_radeon_private_t * dev_priv)
{
	int i;
	for (i = 0; i < 2 * RADEON_MAX_SURFACES; i++) {
		if (dev_priv->virt_surfaces[i].file_priv == file_priv)
			free_surface(file_priv, dev_priv,
				     dev_priv->virt_surfaces[i].lower);
	}
}

/* ================================================================
 * IOCTL functions
 */
static int radeon_surface_alloc(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_surface_alloc_t *alloc = data;

	if (alloc_surface(alloc, dev_priv, file_priv) == -1)
		return -EINVAL;
	else
		return 0;
}

static int radeon_surface_free(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_surface_free_t *memfree = data;

	if (free_surface(file_priv, dev_priv, memfree->address))
		return -EINVAL;
	else
		return 0;
}

static int radeon_cp_clear(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = file_priv->master->driver_priv;
	drm_radeon_sarea_t *sarea_priv = master_priv->sarea_priv;
	drm_radeon_clear_t *clear = data;
	drm_radeon_clear_rect_t depth_boxes[RADEON_NR_SAREA_CLIPRECTS];
	DRM_DEBUG("\n");

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	RING_SPACE_TEST_WITH_RETURN(dev_priv);

	if (sarea_priv->nbox > RADEON_NR_SAREA_CLIPRECTS)
		sarea_priv->nbox = RADEON_NR_SAREA_CLIPRECTS;

	if (DRM_COPY_FROM_USER(&depth_boxes, clear->depth_boxes,
			       sarea_priv->nbox * sizeof(depth_boxes[0])))
		return -EFAULT;

	radeon_cp_dispatch_clear(dev, file_priv->master, clear, depth_boxes);

	COMMIT_RING();
	return 0;
}

/* Not sure why this isn't set all the time:
 */
static int radeon_do_init_pageflip(struct drm_device *dev, struct drm_master *master)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = master->driver_priv;
	RING_LOCALS;

	DRM_DEBUG("\n");

	BEGIN_RING(6);
	RADEON_WAIT_UNTIL_3D_IDLE();
	OUT_RING(CP_PACKET0(RADEON_CRTC_OFFSET_CNTL, 0));
	OUT_RING(RADEON_READ(RADEON_CRTC_OFFSET_CNTL) |
		 RADEON_CRTC_OFFSET_FLIP_CNTL);
	OUT_RING(CP_PACKET0(RADEON_CRTC2_OFFSET_CNTL, 0));
	OUT_RING(RADEON_READ(RADEON_CRTC2_OFFSET_CNTL) |
		 RADEON_CRTC_OFFSET_FLIP_CNTL);
	ADVANCE_RING();

	dev_priv->page_flipping = 1;

	if (master_priv->sarea_priv->pfCurrentPage != 1)
		master_priv->sarea_priv->pfCurrentPage = 0;

	return 0;
}

/* Swapping and flipping are different operations, need different ioctls.
 * They can & should be intermixed to support multiple 3d windows.
 */
static int radeon_cp_flip(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG("\n");

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	RING_SPACE_TEST_WITH_RETURN(dev_priv);

	if (!dev_priv->page_flipping)
		radeon_do_init_pageflip(dev, file_priv->master);

	radeon_cp_dispatch_flip(dev, file_priv->master);

	COMMIT_RING();
	return 0;
}

static int radeon_cp_swap(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = file_priv->master->driver_priv;
	drm_radeon_sarea_t *sarea_priv = master_priv->sarea_priv;

	DRM_DEBUG("\n");

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	RING_SPACE_TEST_WITH_RETURN(dev_priv);

	if (sarea_priv->nbox > RADEON_NR_SAREA_CLIPRECTS)
		sarea_priv->nbox = RADEON_NR_SAREA_CLIPRECTS;

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600)
		r600_cp_dispatch_swap(dev, file_priv);
	else
		radeon_cp_dispatch_swap(dev, file_priv->master);
	sarea_priv->ctx_owner = 0;

	COMMIT_RING();
	return 0;
}

static int radeon_cp_vertex(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = file_priv->master->driver_priv;
	drm_radeon_sarea_t *sarea_priv;
	struct drm_device_dma *dma = dev->dma;
	struct drm_buf *buf;
	drm_radeon_vertex_t *vertex = data;
	drm_radeon_tcl_prim_t prim;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	sarea_priv = master_priv->sarea_priv;

	DRM_DEBUG("pid=%d index=%d count=%d discard=%d\n",
		  DRM_CURRENTPID, vertex->idx, vertex->count, vertex->discard);

	if (vertex->idx < 0 || vertex->idx >= dma->buf_count) {
		DRM_ERROR("buffer index %d (of %d max)\n",
			  vertex->idx, dma->buf_count - 1);
		return -EINVAL;
	}
	if (vertex->prim < 0 || vertex->prim > RADEON_PRIM_TYPE_3VRT_LINE_LIST) {
		DRM_ERROR("buffer prim %d\n", vertex->prim);
		return -EINVAL;
	}

	RING_SPACE_TEST_WITH_RETURN(dev_priv);
	VB_AGE_TEST_WITH_RETURN(dev_priv);

	buf = dma->buflist[vertex->idx];

	if (buf->file_priv != file_priv) {
		DRM_ERROR("process %d using buffer owned by %p\n",
			  DRM_CURRENTPID, buf->file_priv);
		return -EINVAL;
	}
	if (buf->pending) {
		DRM_ERROR("sending pending buffer %d\n", vertex->idx);
		return -EINVAL;
	}

	/* Build up a prim_t record:
	 */
	if (vertex->count) {
		buf->used = vertex->count;	/* not used? */

		if (sarea_priv->dirty & ~RADEON_UPLOAD_CLIPRECTS) {
			if (radeon_emit_state(dev_priv, file_priv,
					      &sarea_priv->context_state,
					      sarea_priv->tex_state,
					      sarea_priv->dirty)) {
				DRM_ERROR("radeon_emit_state failed\n");
				return -EINVAL;
			}

			sarea_priv->dirty &= ~(RADEON_UPLOAD_TEX0IMAGES |
					       RADEON_UPLOAD_TEX1IMAGES |
					       RADEON_UPLOAD_TEX2IMAGES |
					       RADEON_REQUIRE_QUIESCENCE);
		}

		prim.start = 0;
		prim.finish = vertex->count;	/* unused */
		prim.prim = vertex->prim;
		prim.numverts = vertex->count;
		prim.vc_format = sarea_priv->vc_format;

		radeon_cp_dispatch_vertex(dev, file_priv, buf, &prim);
	}

	if (vertex->discard) {
		radeon_cp_discard_buffer(dev, file_priv->master, buf);
	}

	COMMIT_RING();
	return 0;
}

static int radeon_cp_indices(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = file_priv->master->driver_priv;
	drm_radeon_sarea_t *sarea_priv;
	struct drm_device_dma *dma = dev->dma;
	struct drm_buf *buf;
	drm_radeon_indices_t *elts = data;
	drm_radeon_tcl_prim_t prim;
	int count;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	sarea_priv = master_priv->sarea_priv;

	DRM_DEBUG("pid=%d index=%d start=%d end=%d discard=%d\n",
		  DRM_CURRENTPID, elts->idx, elts->start, elts->end,
		  elts->discard);

	if (elts->idx < 0 || elts->idx >= dma->buf_count) {
		DRM_ERROR("buffer index %d (of %d max)\n",
			  elts->idx, dma->buf_count - 1);
		return -EINVAL;
	}
	if (elts->prim < 0 || elts->prim > RADEON_PRIM_TYPE_3VRT_LINE_LIST) {
		DRM_ERROR("buffer prim %d\n", elts->prim);
		return -EINVAL;
	}

	RING_SPACE_TEST_WITH_RETURN(dev_priv);
	VB_AGE_TEST_WITH_RETURN(dev_priv);

	buf = dma->buflist[elts->idx];

	if (buf->file_priv != file_priv) {
		DRM_ERROR("process %d using buffer owned by %p\n",
			  DRM_CURRENTPID, buf->file_priv);
		return -EINVAL;
	}
	if (buf->pending) {
		DRM_ERROR("sending pending buffer %d\n", elts->idx);
		return -EINVAL;
	}

	count = (elts->end - elts->start) / sizeof(u16);
	elts->start -= RADEON_INDEX_PRIM_OFFSET;

	if (elts->start & 0x7) {
		DRM_ERROR("misaligned buffer 0x%x\n", elts->start);
		return -EINVAL;
	}
	if (elts->start < buf->used) {
		DRM_ERROR("no header 0x%x - 0x%x\n", elts->start, buf->used);
		return -EINVAL;
	}

	buf->used = elts->end;

	if (sarea_priv->dirty & ~RADEON_UPLOAD_CLIPRECTS) {
		if (radeon_emit_state(dev_priv, file_priv,
				      &sarea_priv->context_state,
				      sarea_priv->tex_state,
				      sarea_priv->dirty)) {
			DRM_ERROR("radeon_emit_state failed\n");
			return -EINVAL;
		}

		sarea_priv->dirty &= ~(RADEON_UPLOAD_TEX0IMAGES |
				       RADEON_UPLOAD_TEX1IMAGES |
				       RADEON_UPLOAD_TEX2IMAGES |
				       RADEON_REQUIRE_QUIESCENCE);
	}

	/* Build up a prim_t record:
	 */
	prim.start = elts->start;
	prim.finish = elts->end;
	prim.prim = elts->prim;
	prim.offset = 0;	/* offset from start of dma buffers */
	prim.numverts = RADEON_MAX_VB_VERTS;	/* duh */
	prim.vc_format = sarea_priv->vc_format;

	radeon_cp_dispatch_indices(dev, file_priv->master, buf, &prim);
	if (elts->discard) {
		radeon_cp_discard_buffer(dev, file_priv->master, buf);
	}

	COMMIT_RING();
	return 0;
}

static int radeon_cp_texture(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_texture_t *tex = data;
	drm_radeon_tex_image_t image;
	int ret;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (tex->image == NULL) {
		DRM_ERROR("null texture image!\n");
		return -EINVAL;
	}

	if (DRM_COPY_FROM_USER(&image,
			       (drm_radeon_tex_image_t __user *) tex->image,
			       sizeof(image)))
		return -EFAULT;

	RING_SPACE_TEST_WITH_RETURN(dev_priv);
	VB_AGE_TEST_WITH_RETURN(dev_priv);

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600)
		ret = r600_cp_dispatch_texture(dev, file_priv, tex, &image);
	else
		ret = radeon_cp_dispatch_texture(dev, file_priv, tex, &image);

	return ret;
}

static int radeon_cp_stipple(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_stipple_t *stipple = data;
	u32 mask[32];

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (DRM_COPY_FROM_USER(&mask, stipple->mask, 32 * sizeof(u32)))
		return -EFAULT;

	RING_SPACE_TEST_WITH_RETURN(dev_priv);

	radeon_cp_dispatch_stipple(dev, mask);

	COMMIT_RING();
	return 0;
}

static int radeon_cp_indirect(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_device_dma *dma = dev->dma;
	struct drm_buf *buf;
	drm_radeon_indirect_t *indirect = data;
	RING_LOCALS;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	DRM_DEBUG("idx=%d s=%d e=%d d=%d\n",
		  indirect->idx, indirect->start, indirect->end,
		  indirect->discard);

	if (indirect->idx < 0 || indirect->idx >= dma->buf_count) {
		DRM_ERROR("buffer index %d (of %d max)\n",
			  indirect->idx, dma->buf_count - 1);
		return -EINVAL;
	}

	buf = dma->buflist[indirect->idx];

	if (buf->file_priv != file_priv) {
		DRM_ERROR("process %d using buffer owned by %p\n",
			  DRM_CURRENTPID, buf->file_priv);
		return -EINVAL;
	}
	if (buf->pending) {
		DRM_ERROR("sending pending buffer %d\n", indirect->idx);
		return -EINVAL;
	}

	if (indirect->start < buf->used) {
		DRM_ERROR("reusing indirect: start=0x%x actual=0x%x\n",
			  indirect->start, buf->used);
		return -EINVAL;
	}

	RING_SPACE_TEST_WITH_RETURN(dev_priv);
	VB_AGE_TEST_WITH_RETURN(dev_priv);

	buf->used = indirect->end;

	/* Dispatch the indirect buffer full of commands from the
	 * X server.  This is insecure and is thus only available to
	 * privileged clients.
	 */
	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600)
		r600_cp_dispatch_indirect(dev, buf, indirect->start, indirect->end);
	else {
		/* Wait for the 3D stream to idle before the indirect buffer
		 * containing 2D acceleration commands is processed.
		 */
		BEGIN_RING(2);
		RADEON_WAIT_UNTIL_3D_IDLE();
		ADVANCE_RING();
		radeon_cp_dispatch_indirect(dev, buf, indirect->start, indirect->end);
	}

	if (indirect->discard) {
		radeon_cp_discard_buffer(dev, file_priv->master, buf);
	}

	COMMIT_RING();
	return 0;
}

static int radeon_cp_vertex2(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = file_priv->master->driver_priv;
	drm_radeon_sarea_t *sarea_priv;
	struct drm_device_dma *dma = dev->dma;
	struct drm_buf *buf;
	drm_radeon_vertex2_t *vertex = data;
	int i;
	unsigned char laststate;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	sarea_priv = master_priv->sarea_priv;

	DRM_DEBUG("pid=%d index=%d discard=%d\n",
		  DRM_CURRENTPID, vertex->idx, vertex->discard);

	if (vertex->idx < 0 || vertex->idx >= dma->buf_count) {
		DRM_ERROR("buffer index %d (of %d max)\n",
			  vertex->idx, dma->buf_count - 1);
		return -EINVAL;
	}

	RING_SPACE_TEST_WITH_RETURN(dev_priv);
	VB_AGE_TEST_WITH_RETURN(dev_priv);

	buf = dma->buflist[vertex->idx];

	if (buf->file_priv != file_priv) {
		DRM_ERROR("process %d using buffer owned by %p\n",
			  DRM_CURRENTPID, buf->file_priv);
		return -EINVAL;
	}

	if (buf->pending) {
		DRM_ERROR("sending pending buffer %d\n", vertex->idx);
		return -EINVAL;
	}

	if (sarea_priv->nbox > RADEON_NR_SAREA_CLIPRECTS)
		return -EINVAL;

	for (laststate = 0xff, i = 0; i < vertex->nr_prims; i++) {
		drm_radeon_prim_t prim;
		drm_radeon_tcl_prim_t tclprim;

		if (DRM_COPY_FROM_USER(&prim, &vertex->prim[i], sizeof(prim)))
			return -EFAULT;

		if (prim.stateidx != laststate) {
			drm_radeon_state_t state;

			if (DRM_COPY_FROM_USER(&state,
					       &vertex->state[prim.stateidx],
					       sizeof(state)))
				return -EFAULT;

			if (radeon_emit_state2(dev_priv, file_priv, &state)) {
				DRM_ERROR("radeon_emit_state2 failed\n");
				return -EINVAL;
			}

			laststate = prim.stateidx;
		}

		tclprim.start = prim.start;
		tclprim.finish = prim.finish;
		tclprim.prim = prim.prim;
		tclprim.vc_format = prim.vc_format;

		if (prim.prim & RADEON_PRIM_WALK_IND) {
			tclprim.offset = prim.numverts * 64;
			tclprim.numverts = RADEON_MAX_VB_VERTS;	/* duh */

			radeon_cp_dispatch_indices(dev, file_priv->master, buf, &tclprim);
		} else {
			tclprim.numverts = prim.numverts;
			tclprim.offset = 0;	/* not used */

			radeon_cp_dispatch_vertex(dev, file_priv, buf, &tclprim);
		}

		if (sarea_priv->nbox == 1)
			sarea_priv->nbox = 0;
	}

	if (vertex->discard) {
		radeon_cp_discard_buffer(dev, file_priv->master, buf);
	}

	COMMIT_RING();
	return 0;
}

static int radeon_emit_packets(drm_radeon_private_t * dev_priv,
			       struct drm_file *file_priv,
			       drm_radeon_cmd_header_t header,
			       drm_radeon_kcmd_buffer_t *cmdbuf)
{
	int id = (int)header.packet.packet_id;
	int sz, reg;
	RING_LOCALS;

	if (id >= RADEON_MAX_STATE_PACKETS)
		return -EINVAL;

	sz = packet[id].len;
	reg = packet[id].start;

	if (sz * sizeof(u32) > drm_buffer_unprocessed(cmdbuf->buffer)) {
		DRM_ERROR("Packet size provided larger than data provided\n");
		return -EINVAL;
	}

	if (radeon_check_and_fixup_packets(dev_priv, file_priv, id,
				cmdbuf->buffer)) {
		DRM_ERROR("Packet verification failed\n");
		return -EINVAL;
	}

	BEGIN_RING(sz + 1);
	OUT_RING(CP_PACKET0(reg, (sz - 1)));
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, sz);
	ADVANCE_RING();

	return 0;
}

static __inline__ int radeon_emit_scalars(drm_radeon_private_t *dev_priv,
					  drm_radeon_cmd_header_t header,
					  drm_radeon_kcmd_buffer_t *cmdbuf)
{
	int sz = header.scalars.count;
	int start = header.scalars.offset;
	int stride = header.scalars.stride;
	RING_LOCALS;

	BEGIN_RING(3 + sz);
	OUT_RING(CP_PACKET0(RADEON_SE_TCL_SCALAR_INDX_REG, 0));
	OUT_RING(start | (stride << RADEON_SCAL_INDX_DWORD_STRIDE_SHIFT));
	OUT_RING(CP_PACKET0_TABLE(RADEON_SE_TCL_SCALAR_DATA_REG, sz - 1));
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, sz);
	ADVANCE_RING();
	return 0;
}

/* God this is ugly
 */
static __inline__ int radeon_emit_scalars2(drm_radeon_private_t *dev_priv,
					   drm_radeon_cmd_header_t header,
					   drm_radeon_kcmd_buffer_t *cmdbuf)
{
	int sz = header.scalars.count;
	int start = ((unsigned int)header.scalars.offset) + 0x100;
	int stride = header.scalars.stride;
	RING_LOCALS;

	BEGIN_RING(3 + sz);
	OUT_RING(CP_PACKET0(RADEON_SE_TCL_SCALAR_INDX_REG, 0));
	OUT_RING(start | (stride << RADEON_SCAL_INDX_DWORD_STRIDE_SHIFT));
	OUT_RING(CP_PACKET0_TABLE(RADEON_SE_TCL_SCALAR_DATA_REG, sz - 1));
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, sz);
	ADVANCE_RING();
	return 0;
}

static __inline__ int radeon_emit_vectors(drm_radeon_private_t *dev_priv,
					  drm_radeon_cmd_header_t header,
					  drm_radeon_kcmd_buffer_t *cmdbuf)
{
	int sz = header.vectors.count;
	int start = header.vectors.offset;
	int stride = header.vectors.stride;
	RING_LOCALS;

	BEGIN_RING(5 + sz);
	OUT_RING_REG(RADEON_SE_TCL_STATE_FLUSH, 0);
	OUT_RING(CP_PACKET0(RADEON_SE_TCL_VECTOR_INDX_REG, 0));
	OUT_RING(start | (stride << RADEON_VEC_INDX_OCTWORD_STRIDE_SHIFT));
	OUT_RING(CP_PACKET0_TABLE(RADEON_SE_TCL_VECTOR_DATA_REG, (sz - 1)));
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, sz);
	ADVANCE_RING();

	return 0;
}

static __inline__ int radeon_emit_veclinear(drm_radeon_private_t *dev_priv,
					  drm_radeon_cmd_header_t header,
					  drm_radeon_kcmd_buffer_t *cmdbuf)
{
	int sz = header.veclinear.count * 4;
	int start = header.veclinear.addr_lo | (header.veclinear.addr_hi << 8);
	RING_LOCALS;

        if (!sz)
                return 0;
	if (sz * 4 > drm_buffer_unprocessed(cmdbuf->buffer))
                return -EINVAL;

	BEGIN_RING(5 + sz);
	OUT_RING_REG(RADEON_SE_TCL_STATE_FLUSH, 0);
	OUT_RING(CP_PACKET0(RADEON_SE_TCL_VECTOR_INDX_REG, 0));
	OUT_RING(start | (1 << RADEON_VEC_INDX_OCTWORD_STRIDE_SHIFT));
	OUT_RING(CP_PACKET0_TABLE(RADEON_SE_TCL_VECTOR_DATA_REG, (sz - 1)));
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, sz);
	ADVANCE_RING();

	return 0;
}

static int radeon_emit_packet3(struct drm_device * dev,
			       struct drm_file *file_priv,
			       drm_radeon_kcmd_buffer_t *cmdbuf)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	unsigned int cmdsz;
	int ret;
	RING_LOCALS;

	DRM_DEBUG("\n");

	if ((ret = radeon_check_and_fixup_packet3(dev_priv, file_priv,
						  cmdbuf, &cmdsz))) {
		DRM_ERROR("Packet verification failed\n");
		return ret;
	}

	BEGIN_RING(cmdsz);
	OUT_RING_DRM_BUFFER(cmdbuf->buffer, cmdsz);
	ADVANCE_RING();

	return 0;
}

static int radeon_emit_packet3_cliprect(struct drm_device *dev,
					struct drm_file *file_priv,
					drm_radeon_kcmd_buffer_t *cmdbuf,
					int orig_nbox)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_clip_rect box;
	unsigned int cmdsz;
	int ret;
	struct drm_clip_rect __user *boxes = cmdbuf->boxes;
	int i = 0;
	RING_LOCALS;

	DRM_DEBUG("\n");

	if ((ret = radeon_check_and_fixup_packet3(dev_priv, file_priv,
						  cmdbuf, &cmdsz))) {
		DRM_ERROR("Packet verification failed\n");
		return ret;
	}

	if (!orig_nbox)
		goto out;

	do {
		if (i < cmdbuf->nbox) {
			if (DRM_COPY_FROM_USER(&box, &boxes[i], sizeof(box)))
				return -EFAULT;
			if (i) {
				BEGIN_RING(2);
				RADEON_WAIT_UNTIL_3D_IDLE();
				ADVANCE_RING();
			}
			radeon_emit_clip_rect(dev_priv, &box);
		}

		BEGIN_RING(cmdsz);
		OUT_RING_DRM_BUFFER(cmdbuf->buffer, cmdsz);
		ADVANCE_RING();

	} while (++i < cmdbuf->nbox);
	if (cmdbuf->nbox == 1)
		cmdbuf->nbox = 0;

	return 0;
      out:
	drm_buffer_advance(cmdbuf->buffer, cmdsz * 4);
	return 0;
}

static int radeon_emit_wait(struct drm_device * dev, int flags)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;

	DRM_DEBUG("%x\n", flags);
	switch (flags) {
	case RADEON_WAIT_2D:
		BEGIN_RING(2);
		RADEON_WAIT_UNTIL_2D_IDLE();
		ADVANCE_RING();
		break;
	case RADEON_WAIT_3D:
		BEGIN_RING(2);
		RADEON_WAIT_UNTIL_3D_IDLE();
		ADVANCE_RING();
		break;
	case RADEON_WAIT_2D | RADEON_WAIT_3D:
		BEGIN_RING(2);
		RADEON_WAIT_UNTIL_IDLE();
		ADVANCE_RING();
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int radeon_cp_cmdbuf(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_device_dma *dma = dev->dma;
	struct drm_buf *buf = NULL;
	drm_radeon_cmd_header_t stack_header;
	int idx;
	drm_radeon_kcmd_buffer_t *cmdbuf = data;
	int orig_nbox;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	RING_SPACE_TEST_WITH_RETURN(dev_priv);
	VB_AGE_TEST_WITH_RETURN(dev_priv);

	if (cmdbuf->bufsz > 64 * 1024 || cmdbuf->bufsz < 0) {
		return -EINVAL;
	}

	/* Allocate an in-kernel area and copy in the cmdbuf.  Do this to avoid
	 * races between checking values and using those values in other code,
	 * and simply to avoid a lot of function calls to copy in data.
	 */
	if (cmdbuf->bufsz != 0) {
		int rv;
		void __user *buffer = cmdbuf->buffer;
		rv = drm_buffer_alloc(&cmdbuf->buffer, cmdbuf->bufsz);
		if (rv)
			return rv;
		rv = drm_buffer_copy_from_user(cmdbuf->buffer, buffer,
						cmdbuf->bufsz);
		if (rv) {
			drm_buffer_free(cmdbuf->buffer);
			return rv;
		}
	} else
		goto done;

	orig_nbox = cmdbuf->nbox;

	if (dev_priv->microcode_version == UCODE_R300) {
		int temp;
		temp = r300_do_cp_cmdbuf(dev, file_priv, cmdbuf);

		drm_buffer_free(cmdbuf->buffer);

		return temp;
	}

	/* microcode_version != r300 */
	while (drm_buffer_unprocessed(cmdbuf->buffer) >= sizeof(stack_header)) {

		drm_radeon_cmd_header_t *header;
		header = drm_buffer_read_object(cmdbuf->buffer,
				sizeof(stack_header), &stack_header);

		switch (header->header.cmd_type) {
		case RADEON_CMD_PACKET:
			DRM_DEBUG("RADEON_CMD_PACKET\n");
			if (radeon_emit_packets
			    (dev_priv, file_priv, *header, cmdbuf)) {
				DRM_ERROR("radeon_emit_packets failed\n");
				goto err;
			}
			break;

		case RADEON_CMD_SCALARS:
			DRM_DEBUG("RADEON_CMD_SCALARS\n");
			if (radeon_emit_scalars(dev_priv, *header, cmdbuf)) {
				DRM_ERROR("radeon_emit_scalars failed\n");
				goto err;
			}
			break;

		case RADEON_CMD_VECTORS:
			DRM_DEBUG("RADEON_CMD_VECTORS\n");
			if (radeon_emit_vectors(dev_priv, *header, cmdbuf)) {
				DRM_ERROR("radeon_emit_vectors failed\n");
				goto err;
			}
			break;

		case RADEON_CMD_DMA_DISCARD:
			DRM_DEBUG("RADEON_CMD_DMA_DISCARD\n");
			idx = header->dma.buf_idx;
			if (idx < 0 || idx >= dma->buf_count) {
				DRM_ERROR("buffer index %d (of %d max)\n",
					  idx, dma->buf_count - 1);
				goto err;
			}

			buf = dma->buflist[idx];
			if (buf->file_priv != file_priv || buf->pending) {
				DRM_ERROR("bad buffer %p %p %d\n",
					  buf->file_priv, file_priv,
					  buf->pending);
				goto err;
			}

			radeon_cp_discard_buffer(dev, file_priv->master, buf);
			break;

		case RADEON_CMD_PACKET3:
			DRM_DEBUG("RADEON_CMD_PACKET3\n");
			if (radeon_emit_packet3(dev, file_priv, cmdbuf)) {
				DRM_ERROR("radeon_emit_packet3 failed\n");
				goto err;
			}
			break;

		case RADEON_CMD_PACKET3_CLIP:
			DRM_DEBUG("RADEON_CMD_PACKET3_CLIP\n");
			if (radeon_emit_packet3_cliprect
			    (dev, file_priv, cmdbuf, orig_nbox)) {
				DRM_ERROR("radeon_emit_packet3_clip failed\n");
				goto err;
			}
			break;

		case RADEON_CMD_SCALARS2:
			DRM_DEBUG("RADEON_CMD_SCALARS2\n");
			if (radeon_emit_scalars2(dev_priv, *header, cmdbuf)) {
				DRM_ERROR("radeon_emit_scalars2 failed\n");
				goto err;
			}
			break;

		case RADEON_CMD_WAIT:
			DRM_DEBUG("RADEON_CMD_WAIT\n");
			if (radeon_emit_wait(dev, header->wait.flags)) {
				DRM_ERROR("radeon_emit_wait failed\n");
				goto err;
			}
			break;
		case RADEON_CMD_VECLINEAR:
			DRM_DEBUG("RADEON_CMD_VECLINEAR\n");
			if (radeon_emit_veclinear(dev_priv, *header, cmdbuf)) {
				DRM_ERROR("radeon_emit_veclinear failed\n");
				goto err;
			}
			break;

		default:
			DRM_ERROR("bad cmd_type %d at byte %d\n",
				  header->header.cmd_type,
				  cmdbuf->buffer->iterator);
			goto err;
		}
	}

	drm_buffer_free(cmdbuf->buffer);

      done:
	DRM_DEBUG("DONE\n");
	COMMIT_RING();
	return 0;

      err:
	drm_buffer_free(cmdbuf->buffer);
	return -EINVAL;
}

static int radeon_cp_getparam(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_getparam_t *param = data;
	int value;

	DRM_DEBUG("pid=%d\n", DRM_CURRENTPID);

	switch (param->param) {
	case RADEON_PARAM_GART_BUFFER_OFFSET:
		value = dev_priv->gart_buffers_offset;
		break;
	case RADEON_PARAM_LAST_FRAME:
		dev_priv->stats.last_frame_reads++;
		value = GET_SCRATCH(dev_priv, 0);
		break;
	case RADEON_PARAM_LAST_DISPATCH:
		value = GET_SCRATCH(dev_priv, 1);
		break;
	case RADEON_PARAM_LAST_CLEAR:
		dev_priv->stats.last_clear_reads++;
		value = GET_SCRATCH(dev_priv, 2);
		break;
	case RADEON_PARAM_IRQ_NR:
		if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600)
			value = 0;
		else
			value = drm_dev_to_irq(dev);
		break;
	case RADEON_PARAM_GART_BASE:
		value = dev_priv->gart_vm_start;
		break;
	case RADEON_PARAM_REGISTER_HANDLE:
		value = dev_priv->mmio->offset;
		break;
	case RADEON_PARAM_STATUS_HANDLE:
		value = dev_priv->ring_rptr_offset;
		break;
#if BITS_PER_LONG == 32
		/*
		 * This ioctl() doesn't work on 64-bit platforms because hw_lock is a
		 * pointer which can't fit into an int-sized variable.  According to
		 * Michel Dänzer, the ioctl() is only used on embedded platforms, so
		 * not supporting it shouldn't be a problem.  If the same functionality
		 * is needed on 64-bit platforms, a new ioctl() would have to be added,
		 * so backwards-compatibility for the embedded platforms can be
		 * maintained.  --davidm 4-Feb-2004.
		 */
	case RADEON_PARAM_SAREA_HANDLE:
		/* The lock is the first dword in the sarea. */
		/* no users of this parameter */
		break;
#endif
	case RADEON_PARAM_GART_TEX_HANDLE:
		value = dev_priv->gart_textures_offset;
		break;
	case RADEON_PARAM_SCRATCH_OFFSET:
		if (!dev_priv->writeback_works)
			return -EINVAL;
		if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600)
			value = R600_SCRATCH_REG_OFFSET;
		else
			value = RADEON_SCRATCH_REG_OFFSET;
		break;
	case RADEON_PARAM_CARD_TYPE:
		if (dev_priv->flags & RADEON_IS_PCIE)
			value = RADEON_CARD_PCIE;
		else if (dev_priv->flags & RADEON_IS_AGP)
			value = RADEON_CARD_AGP;
		else
			value = RADEON_CARD_PCI;
		break;
	case RADEON_PARAM_VBLANK_CRTC:
		value = radeon_vblank_crtc_get(dev);
		break;
	case RADEON_PARAM_FB_LOCATION:
		value = radeon_read_fb_location(dev_priv);
		break;
	case RADEON_PARAM_NUM_GB_PIPES:
		value = dev_priv->num_gb_pipes;
		break;
	case RADEON_PARAM_NUM_Z_PIPES:
		value = dev_priv->num_z_pipes;
		break;
	default:
		DRM_DEBUG("Invalid parameter %d\n", param->param);
		return -EINVAL;
	}

	if (DRM_COPY_TO_USER(param->value, &value, sizeof(int))) {
		DRM_ERROR("copy_to_user\n");
		return -EFAULT;
	}

	return 0;
}

static int radeon_cp_setparam(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = file_priv->master->driver_priv;
	drm_radeon_setparam_t *sp = data;
	struct drm_radeon_driver_file_fields *radeon_priv;

	switch (sp->param) {
	case RADEON_SETPARAM_FB_LOCATION:
		radeon_priv = file_priv->driver_priv;
		radeon_priv->radeon_fb_delta = dev_priv->fb_location -
		    sp->value;
		break;
	case RADEON_SETPARAM_SWITCH_TILING:
		if (sp->value == 0) {
			DRM_DEBUG("color tiling disabled\n");
			dev_priv->front_pitch_offset &= ~RADEON_DST_TILE_MACRO;
			dev_priv->back_pitch_offset &= ~RADEON_DST_TILE_MACRO;
			if (master_priv->sarea_priv)
				master_priv->sarea_priv->tiling_enabled = 0;
		} else if (sp->value == 1) {
			DRM_DEBUG("color tiling enabled\n");
			dev_priv->front_pitch_offset |= RADEON_DST_TILE_MACRO;
			dev_priv->back_pitch_offset |= RADEON_DST_TILE_MACRO;
			if (master_priv->sarea_priv)
				master_priv->sarea_priv->tiling_enabled = 1;
		}
		break;
	case RADEON_SETPARAM_PCIGART_LOCATION:
		dev_priv->pcigart_offset = sp->value;
		dev_priv->pcigart_offset_set = 1;
		break;
	case RADEON_SETPARAM_NEW_MEMMAP:
		dev_priv->new_memmap = sp->value;
		break;
	case RADEON_SETPARAM_PCIGART_TABLE_SIZE:
		dev_priv->gart_info.table_size = sp->value;
		if (dev_priv->gart_info.table_size < RADEON_PCIGART_TABLE_SIZE)
			dev_priv->gart_info.table_size = RADEON_PCIGART_TABLE_SIZE;
		break;
	case RADEON_SETPARAM_VBLANK_CRTC:
		return radeon_vblank_crtc_set(dev, sp->value);
		break;
	default:
		DRM_DEBUG("Invalid parameter %d\n", sp->param);
		return -EINVAL;
	}

	return 0;
}

/* When a client dies:
 *    - Check for and clean up flipped page state
 *    - Free any alloced GART memory.
 *    - Free any alloced radeon surfaces.
 *
 * DRM infrastructure takes care of reclaiming dma buffers.
 */
void radeon_driver_preclose(struct drm_device *dev, struct drm_file *file_priv)
{
	if (dev->dev_private) {
		drm_radeon_private_t *dev_priv = dev->dev_private;
		dev_priv->page_flipping = 0;
		radeon_mem_release(file_priv, dev_priv->gart_heap);
		radeon_mem_release(file_priv, dev_priv->fb_heap);
		radeon_surfaces_release(file_priv, dev_priv);
	}
}

void radeon_driver_lastclose(struct drm_device *dev)
{
	radeon_surfaces_release(PCIGART_FILE_PRIV, dev->dev_private);
	radeon_do_release(dev);
}

int radeon_driver_open(struct drm_device *dev, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_driver_file_fields *radeon_priv;

	DRM_DEBUG("\n");
	radeon_priv = kmalloc(sizeof(*radeon_priv), GFP_KERNEL);

	if (!radeon_priv)
		return -ENOMEM;

	file_priv->driver_priv = radeon_priv;

	if (dev_priv)
		radeon_priv->radeon_fb_delta = dev_priv->fb_location;
	else
		radeon_priv->radeon_fb_delta = 0;
	return 0;
}

void radeon_driver_postclose(struct drm_device *dev, struct drm_file *file_priv)
{
	struct drm_radeon_driver_file_fields *radeon_priv =
	    file_priv->driver_priv;

	kfree(radeon_priv);
}

struct drm_ioctl_desc radeon_ioctls[] = {
	DRM_IOCTL_DEF_DRV(RADEON_CP_INIT, radeon_cp_init, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_CP_START, radeon_cp_start, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_CP_STOP, radeon_cp_stop, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_CP_RESET, radeon_cp_reset, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_CP_IDLE, radeon_cp_idle, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_CP_RESUME, radeon_cp_resume, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_RESET, radeon_engine_reset, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_FULLSCREEN, radeon_fullscreen, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_SWAP, radeon_cp_swap, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_CLEAR, radeon_cp_clear, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_VERTEX, radeon_cp_vertex, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_INDICES, radeon_cp_indices, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_TEXTURE, radeon_cp_texture, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_STIPPLE, radeon_cp_stipple, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_INDIRECT, radeon_cp_indirect, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_VERTEX2, radeon_cp_vertex2, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_CMDBUF, radeon_cp_cmdbuf, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_GETPARAM, radeon_cp_getparam, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_FLIP, radeon_cp_flip, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_ALLOC, radeon_mem_alloc, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_FREE, radeon_mem_free, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_INIT_HEAP, radeon_mem_init_heap, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(RADEON_IRQ_EMIT, radeon_irq_emit, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_IRQ_WAIT, radeon_irq_wait, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_SETPARAM, radeon_cp_setparam, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_SURF_ALLOC, radeon_surface_alloc, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_SURF_FREE, radeon_surface_free, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(RADEON_CS, r600_cs_legacy_ioctl, DRM_AUTH)
};

int radeon_max_ioctl = DRM_ARRAY_SIZE(radeon_ioctls);
