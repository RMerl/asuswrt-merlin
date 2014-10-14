/*
 * isppreview.c
 *
 * TI OMAP3 ISP driver - Preview module
 *
 * Copyright (C) 2010 Nokia Corporation
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Contacts: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *	     Sakari Ailus <sakari.ailus@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/device.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#include "isp.h"
#include "ispreg.h"
#include "isppreview.h"

/* Default values in Office Fluorescent Light for RGBtoRGB Blending */
static struct omap3isp_prev_rgbtorgb flr_rgb2rgb = {
	{	/* RGB-RGB Matrix */
		{0x01E2, 0x0F30, 0x0FEE},
		{0x0F9B, 0x01AC, 0x0FB9},
		{0x0FE0, 0x0EC0, 0x0260}
	},	/* RGB Offset */
	{0x0000, 0x0000, 0x0000}
};

/* Default values in Office Fluorescent Light for RGB to YUV Conversion*/
static struct omap3isp_prev_csc flr_prev_csc = {
	{	/* CSC Coef Matrix */
		{66, 129, 25},
		{-38, -75, 112},
		{112, -94 , -18}
	},	/* CSC Offset */
	{0x0, 0x0, 0x0}
};

/* Default values in Office Fluorescent Light for CFA Gradient*/
#define FLR_CFA_GRADTHRS_HORZ	0x28
#define FLR_CFA_GRADTHRS_VERT	0x28

/* Default values in Office Fluorescent Light for Chroma Suppression*/
#define FLR_CSUP_GAIN		0x0D
#define FLR_CSUP_THRES		0xEB

/* Default values in Office Fluorescent Light for Noise Filter*/
#define FLR_NF_STRGTH		0x03

/* Default values for White Balance */
#define FLR_WBAL_DGAIN		0x100
#define FLR_WBAL_COEF		0x20

/* Default values in Office Fluorescent Light for Black Adjustment*/
#define FLR_BLKADJ_BLUE		0x0
#define FLR_BLKADJ_GREEN	0x0
#define FLR_BLKADJ_RED		0x0

#define DEF_DETECT_CORRECT_VAL	0xe

#define PREV_MIN_WIDTH		64
#define PREV_MIN_HEIGHT		8
#define PREV_MAX_HEIGHT		16384

/*
 * Coeficient Tables for the submodules in Preview.
 * Array is initialised with the values from.the tables text file.
 */

/*
 * CFA Filter Coefficient Table
 *
 */
static u32 cfa_coef_table[] = {
#include "cfa_coef_table.h"
};

/*
 * Default Gamma Correction Table - All components
 */
static u32 gamma_table[] = {
#include "gamma_table.h"
};

/*
 * Noise Filter Threshold table
 */
static u32 noise_filter_table[] = {
#include "noise_filter_table.h"
};

/*
 * Luminance Enhancement Table
 */
static u32 luma_enhance_table[] = {
#include "luma_enhance_table.h"
};

/*
 * preview_enable_invalaw - Enable/Disable Inverse A-Law module in Preview.
 * @enable: 1 - Reverse the A-Law done in CCDC.
 */
static void
preview_enable_invalaw(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_WIDTH | ISPPRV_PCR_INVALAW);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_WIDTH | ISPPRV_PCR_INVALAW);
}

/*
 * preview_enable_drkframe_capture - Enable/Disable of the darkframe capture.
 * @prev -
 * @enable: 1 - Enable, 0 - Disable
 *
 * NOTE: PRV_WSDR_ADDR and PRV_WADD_OFFSET must be set also
 * The process is applied for each captured frame.
 */
static void
preview_enable_drkframe_capture(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_DRKFCAP);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_DRKFCAP);
}

/*
 * preview_enable_drkframe - Enable/Disable of the darkframe subtract.
 * @enable: 1 - Acquires memory bandwidth since the pixels in each frame is
 *          subtracted with the pixels in the current frame.
 *
 * The process is applied for each captured frame.
 */
static void
preview_enable_drkframe(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_DRKFEN);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_DRKFEN);
}

/*
 * preview_config_drkf_shadcomp - Configures shift value in shading comp.
 * @scomp_shtval: 3bit value of shift used in shading compensation.
 */
static void
preview_config_drkf_shadcomp(struct isp_prev_device *prev,
			     const void *scomp_shtval)
{
	struct isp_device *isp = to_isp_device(prev);
	const u32 *shtval = scomp_shtval;

	isp_reg_clr_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			ISPPRV_PCR_SCOMP_SFT_MASK,
			*shtval << ISPPRV_PCR_SCOMP_SFT_SHIFT);
}

/*
 * preview_enable_hmed - Enables/Disables of the Horizontal Median Filter.
 * @enable: 1 - Enables Horizontal Median Filter.
 */
static void
preview_enable_hmed(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_HMEDEN);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_HMEDEN);
}

/*
 * preview_config_hmed - Configures the Horizontal Median Filter.
 * @prev_hmed: Structure containing the odd and even distance between the
 *             pixels in the image along with the filter threshold.
 */
static void
preview_config_hmed(struct isp_prev_device *prev, const void *prev_hmed)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_hmed *hmed = prev_hmed;

	isp_reg_writel(isp, (hmed->odddist == 1 ? 0 : ISPPRV_HMED_ODDDIST) |
		       (hmed->evendist == 1 ? 0 : ISPPRV_HMED_EVENDIST) |
		       (hmed->thres << ISPPRV_HMED_THRESHOLD_SHIFT),
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_HMED);
}

/*
 * preview_config_noisefilter - Configures the Noise Filter.
 * @prev_nf: Structure containing the noisefilter table, strength to be used
 *           for the noise filter and the defect correction enable flag.
 */
static void
preview_config_noisefilter(struct isp_prev_device *prev, const void *prev_nf)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_nf *nf = prev_nf;
	unsigned int i;

	isp_reg_writel(isp, nf->spread, OMAP3_ISP_IOMEM_PREV, ISPPRV_NF);
	isp_reg_writel(isp, ISPPRV_NF_TABLE_ADDR,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_ADDR);
	for (i = 0; i < OMAP3ISP_PREV_NF_TBL_SIZE; i++) {
		isp_reg_writel(isp, nf->table[i],
			       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_DATA);
	}
}

/*
 * preview_config_dcor - Configures the defect correction
 * @prev_dcor: Structure containing the defect correct thresholds
 */
static void
preview_config_dcor(struct isp_prev_device *prev, const void *prev_dcor)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_dcor *dcor = prev_dcor;

	isp_reg_writel(isp, dcor->detect_correct[0],
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_CDC_THR0);
	isp_reg_writel(isp, dcor->detect_correct[1],
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_CDC_THR1);
	isp_reg_writel(isp, dcor->detect_correct[2],
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_CDC_THR2);
	isp_reg_writel(isp, dcor->detect_correct[3],
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_CDC_THR3);
	isp_reg_clr_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			ISPPRV_PCR_DCCOUP,
			dcor->couplet_mode_en ? ISPPRV_PCR_DCCOUP : 0);
}

/*
 * preview_config_cfa - Configures the CFA Interpolation parameters.
 * @prev_cfa: Structure containing the CFA interpolation table, CFA format
 *            in the image, vertical and horizontal gradient threshold.
 */
static void
preview_config_cfa(struct isp_prev_device *prev, const void *prev_cfa)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_cfa *cfa = prev_cfa;
	unsigned int i;

	isp_reg_clr_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			ISPPRV_PCR_CFAFMT_MASK,
			cfa->format << ISPPRV_PCR_CFAFMT_SHIFT);

	isp_reg_writel(isp,
		(cfa->gradthrs_vert << ISPPRV_CFA_GRADTH_VER_SHIFT) |
		(cfa->gradthrs_horz << ISPPRV_CFA_GRADTH_HOR_SHIFT),
		OMAP3_ISP_IOMEM_PREV, ISPPRV_CFA);

	isp_reg_writel(isp, ISPPRV_CFA_TABLE_ADDR,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_ADDR);

	for (i = 0; i < OMAP3ISP_PREV_CFA_TBL_SIZE; i++) {
		isp_reg_writel(isp, cfa->table[i],
			       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_DATA);
	}
}

/*
 * preview_config_gammacorrn - Configures the Gamma Correction table values
 * @gtable: Structure containing the table for red, blue, green gamma table.
 */
static void
preview_config_gammacorrn(struct isp_prev_device *prev, const void *gtable)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_gtables *gt = gtable;
	unsigned int i;

	isp_reg_writel(isp, ISPPRV_REDGAMMA_TABLE_ADDR,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_ADDR);
	for (i = 0; i < OMAP3ISP_PREV_GAMMA_TBL_SIZE; i++)
		isp_reg_writel(isp, gt->red[i], OMAP3_ISP_IOMEM_PREV,
			       ISPPRV_SET_TBL_DATA);

	isp_reg_writel(isp, ISPPRV_GREENGAMMA_TABLE_ADDR,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_ADDR);
	for (i = 0; i < OMAP3ISP_PREV_GAMMA_TBL_SIZE; i++)
		isp_reg_writel(isp, gt->green[i], OMAP3_ISP_IOMEM_PREV,
			       ISPPRV_SET_TBL_DATA);

	isp_reg_writel(isp, ISPPRV_BLUEGAMMA_TABLE_ADDR,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_ADDR);
	for (i = 0; i < OMAP3ISP_PREV_GAMMA_TBL_SIZE; i++)
		isp_reg_writel(isp, gt->blue[i], OMAP3_ISP_IOMEM_PREV,
			       ISPPRV_SET_TBL_DATA);
}

/*
 * preview_config_luma_enhancement - Sets the Luminance Enhancement table.
 * @ytable: Structure containing the table for Luminance Enhancement table.
 */
static void
preview_config_luma_enhancement(struct isp_prev_device *prev,
				const void *ytable)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_luma *yt = ytable;
	unsigned int i;

	isp_reg_writel(isp, ISPPRV_YENH_TABLE_ADDR,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_ADDR);
	for (i = 0; i < OMAP3ISP_PREV_YENH_TBL_SIZE; i++) {
		isp_reg_writel(isp, yt->table[i],
			       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_DATA);
	}
}

/*
 * preview_config_chroma_suppression - Configures the Chroma Suppression.
 * @csup: Structure containing the threshold value for suppression
 *        and the hypass filter enable flag.
 */
static void
preview_config_chroma_suppression(struct isp_prev_device *prev,
				  const void *csup)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_csup *cs = csup;

	isp_reg_writel(isp,
		       cs->gain | (cs->thres << ISPPRV_CSUP_THRES_SHIFT) |
		       (cs->hypf_en << ISPPRV_CSUP_HPYF_SHIFT),
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_CSUP);
}

/*
 * preview_enable_noisefilter - Enables/Disables the Noise Filter.
 * @enable: 1 - Enables the Noise Filter.
 */
static void
preview_enable_noisefilter(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_NFEN);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_NFEN);
}

/*
 * preview_enable_dcor - Enables/Disables the defect correction.
 * @enable: 1 - Enables the defect correction.
 */
static void
preview_enable_dcor(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_DCOREN);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_DCOREN);
}

/*
 * preview_enable_cfa - Enable/Disable the CFA Interpolation.
 * @enable: 1 - Enables the CFA.
 */
static void
preview_enable_cfa(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_CFAEN);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_CFAEN);
}

/*
 * preview_enable_gammabypass - Enables/Disables the GammaByPass
 * @enable: 1 - Bypasses Gamma - 10bit input is cropped to 8MSB.
 *          0 - Goes through Gamma Correction. input and output is 10bit.
 */
static void
preview_enable_gammabypass(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_GAMMA_BYPASS);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_GAMMA_BYPASS);
}

/*
 * preview_enable_luma_enhancement - Enables/Disables Luminance Enhancement
 * @enable: 1 - Enable the Luminance Enhancement.
 */
static void
preview_enable_luma_enhancement(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_YNENHEN);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_YNENHEN);
}

/*
 * preview_enable_chroma_suppression - Enables/Disables Chrominance Suppr.
 * @enable: 1 - Enable the Chrominance Suppression.
 */
static void
preview_enable_chroma_suppression(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_SUPEN);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_SUPEN);
}

/*
 * preview_config_whitebalance - Configures the White Balance parameters.
 * @prev_wbal: Structure containing the digital gain and white balance
 *             coefficient.
 *
 * Coefficient matrix always with default values.
 */
static void
preview_config_whitebalance(struct isp_prev_device *prev, const void *prev_wbal)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_wbal *wbal = prev_wbal;
	u32 val;

	isp_reg_writel(isp, wbal->dgain, OMAP3_ISP_IOMEM_PREV, ISPPRV_WB_DGAIN);

	val = wbal->coef0 << ISPPRV_WBGAIN_COEF0_SHIFT;
	val |= wbal->coef1 << ISPPRV_WBGAIN_COEF1_SHIFT;
	val |= wbal->coef2 << ISPPRV_WBGAIN_COEF2_SHIFT;
	val |= wbal->coef3 << ISPPRV_WBGAIN_COEF3_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_WBGAIN);

	isp_reg_writel(isp,
		       ISPPRV_WBSEL_COEF0 << ISPPRV_WBSEL_N0_0_SHIFT |
		       ISPPRV_WBSEL_COEF1 << ISPPRV_WBSEL_N0_1_SHIFT |
		       ISPPRV_WBSEL_COEF0 << ISPPRV_WBSEL_N0_2_SHIFT |
		       ISPPRV_WBSEL_COEF1 << ISPPRV_WBSEL_N0_3_SHIFT |
		       ISPPRV_WBSEL_COEF2 << ISPPRV_WBSEL_N1_0_SHIFT |
		       ISPPRV_WBSEL_COEF3 << ISPPRV_WBSEL_N1_1_SHIFT |
		       ISPPRV_WBSEL_COEF2 << ISPPRV_WBSEL_N1_2_SHIFT |
		       ISPPRV_WBSEL_COEF3 << ISPPRV_WBSEL_N1_3_SHIFT |
		       ISPPRV_WBSEL_COEF0 << ISPPRV_WBSEL_N2_0_SHIFT |
		       ISPPRV_WBSEL_COEF1 << ISPPRV_WBSEL_N2_1_SHIFT |
		       ISPPRV_WBSEL_COEF0 << ISPPRV_WBSEL_N2_2_SHIFT |
		       ISPPRV_WBSEL_COEF1 << ISPPRV_WBSEL_N2_3_SHIFT |
		       ISPPRV_WBSEL_COEF2 << ISPPRV_WBSEL_N3_0_SHIFT |
		       ISPPRV_WBSEL_COEF3 << ISPPRV_WBSEL_N3_1_SHIFT |
		       ISPPRV_WBSEL_COEF2 << ISPPRV_WBSEL_N3_2_SHIFT |
		       ISPPRV_WBSEL_COEF3 << ISPPRV_WBSEL_N3_3_SHIFT,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_WBSEL);
}

/*
 * preview_config_blkadj - Configures the Black Adjustment parameters.
 * @prev_blkadj: Structure containing the black adjustment towards red, green,
 *               blue.
 */
static void
preview_config_blkadj(struct isp_prev_device *prev, const void *prev_blkadj)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_blkadj *blkadj = prev_blkadj;

	isp_reg_writel(isp, (blkadj->blue << ISPPRV_BLKADJOFF_B_SHIFT) |
		       (blkadj->green << ISPPRV_BLKADJOFF_G_SHIFT) |
		       (blkadj->red << ISPPRV_BLKADJOFF_R_SHIFT),
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_BLKADJOFF);
}

/*
 * preview_config_rgb_blending - Configures the RGB-RGB Blending matrix.
 * @rgb2rgb: Structure containing the rgb to rgb blending matrix and the rgb
 *           offset.
 */
static void
preview_config_rgb_blending(struct isp_prev_device *prev, const void *rgb2rgb)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_rgbtorgb *rgbrgb = rgb2rgb;
	u32 val;

	val = (rgbrgb->matrix[0][0] & 0xfff) << ISPPRV_RGB_MAT1_MTX_RR_SHIFT;
	val |= (rgbrgb->matrix[0][1] & 0xfff) << ISPPRV_RGB_MAT1_MTX_GR_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_RGB_MAT1);

	val = (rgbrgb->matrix[0][2] & 0xfff) << ISPPRV_RGB_MAT2_MTX_BR_SHIFT;
	val |= (rgbrgb->matrix[1][0] & 0xfff) << ISPPRV_RGB_MAT2_MTX_RG_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_RGB_MAT2);

	val = (rgbrgb->matrix[1][1] & 0xfff) << ISPPRV_RGB_MAT3_MTX_GG_SHIFT;
	val |= (rgbrgb->matrix[1][2] & 0xfff) << ISPPRV_RGB_MAT3_MTX_BG_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_RGB_MAT3);

	val = (rgbrgb->matrix[2][0] & 0xfff) << ISPPRV_RGB_MAT4_MTX_RB_SHIFT;
	val |= (rgbrgb->matrix[2][1] & 0xfff) << ISPPRV_RGB_MAT4_MTX_GB_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_RGB_MAT4);

	val = (rgbrgb->matrix[2][2] & 0xfff) << ISPPRV_RGB_MAT5_MTX_BB_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_RGB_MAT5);

	val = (rgbrgb->offset[0] & 0x3ff) << ISPPRV_RGB_OFF1_MTX_OFFR_SHIFT;
	val |= (rgbrgb->offset[1] & 0x3ff) << ISPPRV_RGB_OFF1_MTX_OFFG_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_RGB_OFF1);

	val = (rgbrgb->offset[2] & 0x3ff) << ISPPRV_RGB_OFF2_MTX_OFFB_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_RGB_OFF2);
}

/*
 * Configures the RGB-YCbYCr conversion matrix
 * @prev_csc: Structure containing the RGB to YCbYCr matrix and the
 *            YCbCr offset.
 */
static void
preview_config_rgb_to_ycbcr(struct isp_prev_device *prev, const void *prev_csc)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_csc *csc = prev_csc;
	u32 val;

	val = (csc->matrix[0][0] & 0x3ff) << ISPPRV_CSC0_RY_SHIFT;
	val |= (csc->matrix[0][1] & 0x3ff) << ISPPRV_CSC0_GY_SHIFT;
	val |= (csc->matrix[0][2] & 0x3ff) << ISPPRV_CSC0_BY_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_CSC0);

	val = (csc->matrix[1][0] & 0x3ff) << ISPPRV_CSC1_RCB_SHIFT;
	val |= (csc->matrix[1][1] & 0x3ff) << ISPPRV_CSC1_GCB_SHIFT;
	val |= (csc->matrix[1][2] & 0x3ff) << ISPPRV_CSC1_BCB_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_CSC1);

	val = (csc->matrix[2][0] & 0x3ff) << ISPPRV_CSC2_RCR_SHIFT;
	val |= (csc->matrix[2][1] & 0x3ff) << ISPPRV_CSC2_GCR_SHIFT;
	val |= (csc->matrix[2][2] & 0x3ff) << ISPPRV_CSC2_BCR_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_CSC2);

	val = (csc->offset[0] & 0xff) << ISPPRV_CSC_OFFSET_Y_SHIFT;
	val |= (csc->offset[1] & 0xff) << ISPPRV_CSC_OFFSET_CB_SHIFT;
	val |= (csc->offset[2] & 0xff) << ISPPRV_CSC_OFFSET_CR_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_CSC_OFFSET);
}

/*
 * preview_update_contrast - Updates the contrast.
 * @contrast: Pointer to hold the current programmed contrast value.
 *
 * Value should be programmed before enabling the module.
 */
static void
preview_update_contrast(struct isp_prev_device *prev, u8 contrast)
{
	struct prev_params *params = &prev->params;

	if (params->contrast != (contrast * ISPPRV_CONTRAST_UNITS)) {
		params->contrast = contrast * ISPPRV_CONTRAST_UNITS;
		prev->update |= PREV_CONTRAST;
	}
}

/*
 * preview_config_contrast - Configures the Contrast.
 * @params: Contrast value (u8 pointer, U8Q0 format).
 *
 * Value should be programmed before enabling the module.
 */
static void
preview_config_contrast(struct isp_prev_device *prev, const void *params)
{
	struct isp_device *isp = to_isp_device(prev);

	isp_reg_clr_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_CNT_BRT,
			0xff << ISPPRV_CNT_BRT_CNT_SHIFT,
			*(u8 *)params << ISPPRV_CNT_BRT_CNT_SHIFT);
}

/*
 * preview_update_brightness - Updates the brightness in preview module.
 * @brightness: Pointer to hold the current programmed brightness value.
 *
 */
static void
preview_update_brightness(struct isp_prev_device *prev, u8 brightness)
{
	struct prev_params *params = &prev->params;

	if (params->brightness != (brightness * ISPPRV_BRIGHT_UNITS)) {
		params->brightness = brightness * ISPPRV_BRIGHT_UNITS;
		prev->update |= PREV_BRIGHTNESS;
	}
}

/*
 * preview_config_brightness - Configures the brightness.
 * @params: Brightness value (u8 pointer, U8Q0 format).
 */
static void
preview_config_brightness(struct isp_prev_device *prev, const void *params)
{
	struct isp_device *isp = to_isp_device(prev);

	isp_reg_clr_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_CNT_BRT,
			0xff << ISPPRV_CNT_BRT_BRT_SHIFT,
			*(u8 *)params << ISPPRV_CNT_BRT_BRT_SHIFT);
}

/*
 * preview_config_yc_range - Configures the max and min Y and C values.
 * @yclimit: Structure containing the range of Y and C values.
 */
static void
preview_config_yc_range(struct isp_prev_device *prev, const void *yclimit)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_yclimit *yc = yclimit;

	isp_reg_writel(isp,
		       yc->maxC << ISPPRV_SETUP_YC_MAXC_SHIFT |
		       yc->maxY << ISPPRV_SETUP_YC_MAXY_SHIFT |
		       yc->minC << ISPPRV_SETUP_YC_MINC_SHIFT |
		       yc->minY << ISPPRV_SETUP_YC_MINY_SHIFT,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_SETUP_YC);
}

/* preview parameters update structure */
struct preview_update {
	int cfg_bit;
	int feature_bit;
	void (*config)(struct isp_prev_device *, const void *);
	void (*enable)(struct isp_prev_device *, u8);
};

static struct preview_update update_attrs[] = {
	{OMAP3ISP_PREV_LUMAENH, PREV_LUMA_ENHANCE,
		preview_config_luma_enhancement,
		preview_enable_luma_enhancement},
	{OMAP3ISP_PREV_INVALAW, PREV_INVERSE_ALAW,
		NULL,
		preview_enable_invalaw},
	{OMAP3ISP_PREV_HRZ_MED, PREV_HORZ_MEDIAN_FILTER,
		preview_config_hmed,
		preview_enable_hmed},
	{OMAP3ISP_PREV_CFA, PREV_CFA,
		preview_config_cfa,
		preview_enable_cfa},
	{OMAP3ISP_PREV_CHROMA_SUPP, PREV_CHROMA_SUPPRESS,
		preview_config_chroma_suppression,
		preview_enable_chroma_suppression},
	{OMAP3ISP_PREV_WB, PREV_WB,
		preview_config_whitebalance,
		NULL},
	{OMAP3ISP_PREV_BLKADJ, PREV_BLKADJ,
		preview_config_blkadj,
		NULL},
	{OMAP3ISP_PREV_RGB2RGB, PREV_RGB2RGB,
		preview_config_rgb_blending,
		NULL},
	{OMAP3ISP_PREV_COLOR_CONV, PREV_COLOR_CONV,
		preview_config_rgb_to_ycbcr,
		NULL},
	{OMAP3ISP_PREV_YC_LIMIT, PREV_YCLIMITS,
		preview_config_yc_range,
		NULL},
	{OMAP3ISP_PREV_DEFECT_COR, PREV_DEFECT_COR,
		preview_config_dcor,
		preview_enable_dcor},
	{OMAP3ISP_PREV_GAMMABYPASS, PREV_GAMMA_BYPASS,
		NULL,
		preview_enable_gammabypass},
	{OMAP3ISP_PREV_DRK_FRM_CAPTURE, PREV_DARK_FRAME_CAPTURE,
		NULL,
		preview_enable_drkframe_capture},
	{OMAP3ISP_PREV_DRK_FRM_SUBTRACT, PREV_DARK_FRAME_SUBTRACT,
		NULL,
		preview_enable_drkframe},
	{OMAP3ISP_PREV_LENS_SHADING, PREV_LENS_SHADING,
		preview_config_drkf_shadcomp,
		preview_enable_drkframe},
	{OMAP3ISP_PREV_NF, PREV_NOISE_FILTER,
		preview_config_noisefilter,
		preview_enable_noisefilter},
	{OMAP3ISP_PREV_GAMMA, PREV_GAMMA,
		preview_config_gammacorrn,
		NULL},
	{-1, PREV_CONTRAST,
		preview_config_contrast,
		NULL},
	{-1, PREV_BRIGHTNESS,
		preview_config_brightness,
		NULL},
};

/*
 * __preview_get_ptrs - helper function which return pointers to members
 *                         of params and config structures.
 * @params - pointer to preview_params structure.
 * @param - return pointer to appropriate structure field.
 * @configs - pointer to update config structure.
 * @config - return pointer to appropriate structure field.
 * @bit - for which feature to return pointers.
 * Return size of corresponding prev_params member
 */
static u32
__preview_get_ptrs(struct prev_params *params, void **param,
		   struct omap3isp_prev_update_config *configs,
		   void __user **config, u32 bit)
{
#define CHKARG(cfgs, cfg, field)				\
	if (cfgs && cfg) {					\
		*(cfg) = (cfgs)->field;				\
	}

	switch (bit) {
	case PREV_HORZ_MEDIAN_FILTER:
		*param = &params->hmed;
		CHKARG(configs, config, hmed)
		return sizeof(params->hmed);
	case PREV_NOISE_FILTER:
		*param = &params->nf;
		CHKARG(configs, config, nf)
		return sizeof(params->nf);
		break;
	case PREV_CFA:
		*param = &params->cfa;
		CHKARG(configs, config, cfa)
		return sizeof(params->cfa);
	case PREV_LUMA_ENHANCE:
		*param = &params->luma;
		CHKARG(configs, config, luma)
		return sizeof(params->luma);
	case PREV_CHROMA_SUPPRESS:
		*param = &params->csup;
		CHKARG(configs, config, csup)
		return sizeof(params->csup);
	case PREV_DEFECT_COR:
		*param = &params->dcor;
		CHKARG(configs, config, dcor)
		return sizeof(params->dcor);
	case PREV_BLKADJ:
		*param = &params->blk_adj;
		CHKARG(configs, config, blkadj)
		return sizeof(params->blk_adj);
	case PREV_YCLIMITS:
		*param = &params->yclimit;
		CHKARG(configs, config, yclimit)
		return sizeof(params->yclimit);
	case PREV_RGB2RGB:
		*param = &params->rgb2rgb;
		CHKARG(configs, config, rgb2rgb)
		return sizeof(params->rgb2rgb);
	case PREV_COLOR_CONV:
		*param = &params->rgb2ycbcr;
		CHKARG(configs, config, csc)
		return sizeof(params->rgb2ycbcr);
	case PREV_WB:
		*param = &params->wbal;
		CHKARG(configs, config, wbal)
		return sizeof(params->wbal);
	case PREV_GAMMA:
		*param = &params->gamma;
		CHKARG(configs, config, gamma)
		return sizeof(params->gamma);
	case PREV_CONTRAST:
		*param = &params->contrast;
		return 0;
	case PREV_BRIGHTNESS:
		*param = &params->brightness;
		return 0;
	default:
		*param = NULL;
		*config = NULL;
		break;
	}
	return 0;
}

/*
 * preview_config - Copy and update local structure with userspace preview
 *                  configuration.
 * @prev: ISP preview engine
 * @cfg: Configuration
 *
 * Return zero if success or -EFAULT if the configuration can't be copied from
 * userspace.
 */
static int preview_config(struct isp_prev_device *prev,
			  struct omap3isp_prev_update_config *cfg)
{
	struct prev_params *params;
	struct preview_update *attr;
	int i, bit, rval = 0;

	params = &prev->params;

	if (prev->state != ISP_PIPELINE_STREAM_STOPPED) {
		unsigned long flags;

		spin_lock_irqsave(&prev->lock, flags);
		prev->shadow_update = 1;
		spin_unlock_irqrestore(&prev->lock, flags);
	}

	for (i = 0; i < ARRAY_SIZE(update_attrs); i++) {
		attr = &update_attrs[i];
		bit = 0;

		if (!(cfg->update & attr->cfg_bit))
			continue;

		bit = cfg->flag & attr->cfg_bit;
		if (bit) {
			void *to = NULL, __user *from = NULL;
			unsigned long sz = 0;

			sz = __preview_get_ptrs(params, &to, cfg, &from,
						   bit);
			if (to && from && sz) {
				if (copy_from_user(to, from, sz)) {
					rval = -EFAULT;
					break;
				}
			}
			params->features |= attr->feature_bit;
		} else {
			params->features &= ~attr->feature_bit;
		}

		prev->update |= attr->feature_bit;
	}

	prev->shadow_update = 0;
	return rval;
}

/*
 * preview_setup_hw - Setup preview registers and/or internal memory
 * @prev: pointer to preview private structure
 * Note: can be called from interrupt context
 * Return none
 */
static void preview_setup_hw(struct isp_prev_device *prev)
{
	struct prev_params *params = &prev->params;
	struct preview_update *attr;
	int i, bit;
	void *param_ptr;

	for (i = 0; i < ARRAY_SIZE(update_attrs); i++) {
		attr = &update_attrs[i];

		if (!(prev->update & attr->feature_bit))
			continue;
		bit = params->features & attr->feature_bit;
		if (bit) {
			if (attr->config) {
				__preview_get_ptrs(params, &param_ptr, NULL,
						      NULL, bit);
				attr->config(prev, param_ptr);
			}
			if (attr->enable)
				attr->enable(prev, 1);
		} else
			if (attr->enable)
				attr->enable(prev, 0);

		prev->update &= ~attr->feature_bit;
	}
}

/*
 * preview_config_ycpos - Configure byte layout of YUV image.
 * @mode: Indicates the required byte layout.
 */
static void
preview_config_ycpos(struct isp_prev_device *prev,
		     enum v4l2_mbus_pixelcode pixelcode)
{
	struct isp_device *isp = to_isp_device(prev);
	enum preview_ycpos_mode mode;

	switch (pixelcode) {
	case V4L2_MBUS_FMT_YUYV8_1X16:
		mode = YCPOS_CrYCbY;
		break;
	case V4L2_MBUS_FMT_UYVY8_1X16:
		mode = YCPOS_YCrYCb;
		break;
	default:
		return;
	}

	isp_reg_clr_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			ISPPRV_PCR_YCPOS_CrYCbY,
			mode << ISPPRV_PCR_YCPOS_SHIFT);
}

/*
 * preview_config_averager - Enable / disable / configure averager
 * @average: Average value to be configured.
 */
static void preview_config_averager(struct isp_prev_device *prev, u8 average)
{
	struct isp_device *isp = to_isp_device(prev);
	int reg = 0;

	if (prev->params.cfa.format == OMAP3ISP_CFAFMT_BAYER)
		reg = ISPPRV_AVE_EVENDIST_2 << ISPPRV_AVE_EVENDIST_SHIFT |
		      ISPPRV_AVE_ODDDIST_2 << ISPPRV_AVE_ODDDIST_SHIFT |
		      average;
	else if (prev->params.cfa.format == OMAP3ISP_CFAFMT_RGBFOVEON)
		reg = ISPPRV_AVE_EVENDIST_3 << ISPPRV_AVE_EVENDIST_SHIFT |
		      ISPPRV_AVE_ODDDIST_3 << ISPPRV_AVE_ODDDIST_SHIFT |
		      average;
	isp_reg_writel(isp, reg, OMAP3_ISP_IOMEM_PREV, ISPPRV_AVE);
}

/*
 * preview_config_input_size - Configure the input frame size
 *
 * The preview engine crops several rows and columns internally depending on
 * which processing blocks are enabled. The driver assumes all those blocks are
 * enabled when reporting source pad formats to userspace. If this assumption is
 * not true, rows and columns must be manually cropped at the preview engine
 * input to avoid overflows at the end of lines and frames.
 */
static void preview_config_input_size(struct isp_prev_device *prev)
{
	struct isp_device *isp = to_isp_device(prev);
	struct prev_params *params = &prev->params;
	struct v4l2_mbus_framefmt *format = &prev->formats[PREV_PAD_SINK];
	unsigned int sph = 0;
	unsigned int eph = format->width - 1;
	unsigned int slv = 0;
	unsigned int elv = format->height - 1;

	if (prev->input == PREVIEW_INPUT_CCDC) {
		sph += 2;
		eph -= 2;
	}

	/*
	 * Median filter	4 pixels
	 * Noise filter		4 pixels, 4 lines
	 * or faulty pixels correction
	 * CFA filter		4 pixels, 4 lines in Bayer mode
	 *				  2 lines in other modes
	 * Color suppression	2 pixels
	 * or luma enhancement
	 * -------------------------------------------------------------
	 * Maximum total	14 pixels, 8 lines
	 */

	if (!(params->features & PREV_CFA)) {
		sph += 2;
		eph -= 2;
		slv += 2;
		elv -= 2;
	}
	if (!(params->features & (PREV_DEFECT_COR | PREV_NOISE_FILTER))) {
		sph += 2;
		eph -= 2;
		slv += 2;
		elv -= 2;
	}
	if (!(params->features & PREV_HORZ_MEDIAN_FILTER)) {
		sph += 2;
		eph -= 2;
	}
	if (!(params->features & (PREV_CHROMA_SUPPRESS | PREV_LUMA_ENHANCE)))
		sph += 2;

	isp_reg_writel(isp, (sph << ISPPRV_HORZ_INFO_SPH_SHIFT) | eph,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_HORZ_INFO);
	isp_reg_writel(isp, (slv << ISPPRV_VERT_INFO_SLV_SHIFT) | elv,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_VERT_INFO);
}

/*
 * preview_config_inlineoffset - Configures the Read address line offset.
 * @prev: Preview module
 * @offset: Line offset
 *
 * According to the TRM, the line offset must be aligned on a 32 bytes boundary.
 * However, a hardware bug requires the memory start address to be aligned on a
 * 64 bytes boundary, so the offset probably should be aligned on 64 bytes as
 * well.
 */
static void
preview_config_inlineoffset(struct isp_prev_device *prev, u32 offset)
{
	struct isp_device *isp = to_isp_device(prev);

	isp_reg_writel(isp, offset & 0xffff, OMAP3_ISP_IOMEM_PREV,
		       ISPPRV_RADR_OFFSET);
}

/*
 * preview_set_inaddr - Sets memory address of input frame.
 * @addr: 32bit memory address aligned on 32byte boundary.
 *
 * Configures the memory address from which the input frame is to be read.
 */
static void preview_set_inaddr(struct isp_prev_device *prev, u32 addr)
{
	struct isp_device *isp = to_isp_device(prev);

	isp_reg_writel(isp, addr, OMAP3_ISP_IOMEM_PREV, ISPPRV_RSDR_ADDR);
}

/*
 * preview_config_outlineoffset - Configures the Write address line offset.
 * @offset: Line Offset for the preview output.
 *
 * The offset must be a multiple of 32 bytes.
 */
static void preview_config_outlineoffset(struct isp_prev_device *prev,
				    u32 offset)
{
	struct isp_device *isp = to_isp_device(prev);

	isp_reg_writel(isp, offset & 0xffff, OMAP3_ISP_IOMEM_PREV,
		       ISPPRV_WADD_OFFSET);
}

/*
 * preview_set_outaddr - Sets the memory address to store output frame
 * @addr: 32bit memory address aligned on 32byte boundary.
 *
 * Configures the memory address to which the output frame is written.
 */
static void preview_set_outaddr(struct isp_prev_device *prev, u32 addr)
{
	struct isp_device *isp = to_isp_device(prev);

	isp_reg_writel(isp, addr, OMAP3_ISP_IOMEM_PREV, ISPPRV_WSDR_ADDR);
}

static void preview_adjust_bandwidth(struct isp_prev_device *prev)
{
	struct isp_pipeline *pipe = to_isp_pipeline(&prev->subdev.entity);
	struct isp_device *isp = to_isp_device(prev);
	const struct v4l2_mbus_framefmt *ifmt = &prev->formats[PREV_PAD_SINK];
	unsigned long l3_ick = pipe->l3_ick;
	struct v4l2_fract *timeperframe;
	unsigned int cycles_per_frame;
	unsigned int requests_per_frame;
	unsigned int cycles_per_request;
	unsigned int minimum;
	unsigned int maximum;
	unsigned int value;

	if (prev->input != PREVIEW_INPUT_MEMORY) {
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_SBL, ISPSBL_SDR_REQ_EXP,
			    ISPSBL_SDR_REQ_PRV_EXP_MASK);
		return;
	}

	/* Compute the minimum number of cycles per request, based on the
	 * pipeline maximum data rate. This is an absolute lower bound if we
	 * don't want SBL overflows, so round the value up.
	 */
	cycles_per_request = div_u64((u64)l3_ick / 2 * 256 + pipe->max_rate - 1,
				     pipe->max_rate);
	minimum = DIV_ROUND_UP(cycles_per_request, 32);

	/* Compute the maximum number of cycles per request, based on the
	 * requested frame rate. This is a soft upper bound to achieve a frame
	 * rate equal or higher than the requested value, so round the value
	 * down.
	 */
	timeperframe = &pipe->max_timeperframe;

	requests_per_frame = DIV_ROUND_UP(ifmt->width * 2, 256) * ifmt->height;
	cycles_per_frame = div_u64((u64)l3_ick * timeperframe->numerator,
				   timeperframe->denominator);
	cycles_per_request = cycles_per_frame / requests_per_frame;

	maximum = cycles_per_request / 32;

	value = max(minimum, maximum);

	dev_dbg(isp->dev, "%s: cycles per request = %u\n", __func__, value);
	isp_reg_clr_set(isp, OMAP3_ISP_IOMEM_SBL, ISPSBL_SDR_REQ_EXP,
			ISPSBL_SDR_REQ_PRV_EXP_MASK,
			value << ISPSBL_SDR_REQ_PRV_EXP_SHIFT);
}

/*
 * omap3isp_preview_busy - Gets busy state of preview module.
 */
int omap3isp_preview_busy(struct isp_prev_device *prev)
{
	struct isp_device *isp = to_isp_device(prev);

	return isp_reg_readl(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR)
		& ISPPRV_PCR_BUSY;
}

/*
 * omap3isp_preview_restore_context - Restores the values of preview registers
 */
void omap3isp_preview_restore_context(struct isp_device *isp)
{
	isp->isp_prev.update = PREV_FEATURES_END - 1;
	preview_setup_hw(&isp->isp_prev);
}

/*
 * preview_print_status - Dump preview module registers to the kernel log
 */
#define PREV_PRINT_REGISTER(isp, name)\
	dev_dbg(isp->dev, "###PRV " #name "=0x%08x\n", \
		isp_reg_readl(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_##name))

static void preview_print_status(struct isp_prev_device *prev)
{
	struct isp_device *isp = to_isp_device(prev);

	dev_dbg(isp->dev, "-------------Preview Register dump----------\n");

	PREV_PRINT_REGISTER(isp, PCR);
	PREV_PRINT_REGISTER(isp, HORZ_INFO);
	PREV_PRINT_REGISTER(isp, VERT_INFO);
	PREV_PRINT_REGISTER(isp, RSDR_ADDR);
	PREV_PRINT_REGISTER(isp, RADR_OFFSET);
	PREV_PRINT_REGISTER(isp, DSDR_ADDR);
	PREV_PRINT_REGISTER(isp, DRKF_OFFSET);
	PREV_PRINT_REGISTER(isp, WSDR_ADDR);
	PREV_PRINT_REGISTER(isp, WADD_OFFSET);
	PREV_PRINT_REGISTER(isp, AVE);
	PREV_PRINT_REGISTER(isp, HMED);
	PREV_PRINT_REGISTER(isp, NF);
	PREV_PRINT_REGISTER(isp, WB_DGAIN);
	PREV_PRINT_REGISTER(isp, WBGAIN);
	PREV_PRINT_REGISTER(isp, WBSEL);
	PREV_PRINT_REGISTER(isp, CFA);
	PREV_PRINT_REGISTER(isp, BLKADJOFF);
	PREV_PRINT_REGISTER(isp, RGB_MAT1);
	PREV_PRINT_REGISTER(isp, RGB_MAT2);
	PREV_PRINT_REGISTER(isp, RGB_MAT3);
	PREV_PRINT_REGISTER(isp, RGB_MAT4);
	PREV_PRINT_REGISTER(isp, RGB_MAT5);
	PREV_PRINT_REGISTER(isp, RGB_OFF1);
	PREV_PRINT_REGISTER(isp, RGB_OFF2);
	PREV_PRINT_REGISTER(isp, CSC0);
	PREV_PRINT_REGISTER(isp, CSC1);
	PREV_PRINT_REGISTER(isp, CSC2);
	PREV_PRINT_REGISTER(isp, CSC_OFFSET);
	PREV_PRINT_REGISTER(isp, CNT_BRT);
	PREV_PRINT_REGISTER(isp, CSUP);
	PREV_PRINT_REGISTER(isp, SETUP_YC);
	PREV_PRINT_REGISTER(isp, SET_TBL_ADDR);
	PREV_PRINT_REGISTER(isp, CDC_THR0);
	PREV_PRINT_REGISTER(isp, CDC_THR1);
	PREV_PRINT_REGISTER(isp, CDC_THR2);
	PREV_PRINT_REGISTER(isp, CDC_THR3);

	dev_dbg(isp->dev, "--------------------------------------------\n");
}

/*
 * preview_init_params - init image processing parameters.
 * @prev: pointer to previewer private structure
 * return none
 */
static void preview_init_params(struct isp_prev_device *prev)
{
	struct prev_params *params = &prev->params;
	int i = 0;

	/* Init values */
	params->contrast = ISPPRV_CONTRAST_DEF * ISPPRV_CONTRAST_UNITS;
	params->brightness = ISPPRV_BRIGHT_DEF * ISPPRV_BRIGHT_UNITS;
	params->average = NO_AVE;
	params->cfa.format = OMAP3ISP_CFAFMT_BAYER;
	memcpy(params->cfa.table, cfa_coef_table,
	       sizeof(params->cfa.table));
	params->cfa.gradthrs_horz = FLR_CFA_GRADTHRS_HORZ;
	params->cfa.gradthrs_vert = FLR_CFA_GRADTHRS_VERT;
	params->csup.gain = FLR_CSUP_GAIN;
	params->csup.thres = FLR_CSUP_THRES;
	params->csup.hypf_en = 0;
	memcpy(params->luma.table, luma_enhance_table,
	       sizeof(params->luma.table));
	params->nf.spread = FLR_NF_STRGTH;
	memcpy(params->nf.table, noise_filter_table, sizeof(params->nf.table));
	params->dcor.couplet_mode_en = 1;
	for (i = 0; i < OMAP3ISP_PREV_DETECT_CORRECT_CHANNELS; i++)
		params->dcor.detect_correct[i] = DEF_DETECT_CORRECT_VAL;
	memcpy(params->gamma.blue, gamma_table, sizeof(params->gamma.blue));
	memcpy(params->gamma.green, gamma_table, sizeof(params->gamma.green));
	memcpy(params->gamma.red, gamma_table, sizeof(params->gamma.red));
	params->wbal.dgain = FLR_WBAL_DGAIN;
	params->wbal.coef0 = FLR_WBAL_COEF;
	params->wbal.coef1 = FLR_WBAL_COEF;
	params->wbal.coef2 = FLR_WBAL_COEF;
	params->wbal.coef3 = FLR_WBAL_COEF;
	params->blk_adj.red = FLR_BLKADJ_RED;
	params->blk_adj.green = FLR_BLKADJ_GREEN;
	params->blk_adj.blue = FLR_BLKADJ_BLUE;
	params->rgb2rgb = flr_rgb2rgb;
	params->rgb2ycbcr = flr_prev_csc;
	params->yclimit.minC = ISPPRV_YC_MIN;
	params->yclimit.maxC = ISPPRV_YC_MAX;
	params->yclimit.minY = ISPPRV_YC_MIN;
	params->yclimit.maxY = ISPPRV_YC_MAX;

	params->features = PREV_CFA | PREV_DEFECT_COR | PREV_NOISE_FILTER
			 | PREV_GAMMA | PREV_BLKADJ | PREV_YCLIMITS
			 | PREV_RGB2RGB | PREV_COLOR_CONV | PREV_WB
			 | PREV_BRIGHTNESS | PREV_CONTRAST;

	prev->update = PREV_FEATURES_END - 1;
}

/*
 * preview_max_out_width - Handle previewer hardware ouput limitations
 * @isp_revision : ISP revision
 * returns maximum width output for current isp revision
 */
static unsigned int preview_max_out_width(struct isp_prev_device *prev)
{
	struct isp_device *isp = to_isp_device(prev);

	switch (isp->revision) {
	case ISP_REVISION_1_0:
		return ISPPRV_MAXOUTPUT_WIDTH;

	case ISP_REVISION_2_0:
	default:
		return ISPPRV_MAXOUTPUT_WIDTH_ES2;

	case ISP_REVISION_15_0:
		return ISPPRV_MAXOUTPUT_WIDTH_3630;
	}
}

static void preview_configure(struct isp_prev_device *prev)
{
	struct isp_device *isp = to_isp_device(prev);
	struct v4l2_mbus_framefmt *format;
	unsigned int max_out_width;
	unsigned int format_avg;

	preview_setup_hw(prev);

	if (prev->output & PREVIEW_OUTPUT_MEMORY)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_SDRPORT);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_SDRPORT);

	if (prev->output & PREVIEW_OUTPUT_RESIZER)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_RSZPORT);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_RSZPORT);

	/* PREV_PAD_SINK */
	format = &prev->formats[PREV_PAD_SINK];

	preview_adjust_bandwidth(prev);

	preview_config_input_size(prev);

	if (prev->input == PREVIEW_INPUT_CCDC)
		preview_config_inlineoffset(prev, 0);
	else
		preview_config_inlineoffset(prev,
				ALIGN(format->width, 0x20) * 2);

	/* PREV_PAD_SOURCE */
	format = &prev->formats[PREV_PAD_SOURCE];

	if (prev->output & PREVIEW_OUTPUT_MEMORY)
		preview_config_outlineoffset(prev,
				ALIGN(format->width, 0x10) * 2);

	max_out_width = preview_max_out_width(prev);

	format_avg = fls(DIV_ROUND_UP(format->width, max_out_width) - 1);
	preview_config_averager(prev, format_avg);
	preview_config_ycpos(prev, format->code);
}

/* -----------------------------------------------------------------------------
 * Interrupt handling
 */

static void preview_enable_oneshot(struct isp_prev_device *prev)
{
	struct isp_device *isp = to_isp_device(prev);

	/* The PCR.SOURCE bit is automatically reset to 0 when the PCR.ENABLE
	 * bit is set. As the preview engine is used in single-shot mode, we
	 * need to set PCR.SOURCE before enabling the preview engine.
	 */
	if (prev->input == PREVIEW_INPUT_MEMORY)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_SOURCE);

	isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
		    ISPPRV_PCR_EN | ISPPRV_PCR_ONESHOT);
}

void omap3isp_preview_isr_frame_sync(struct isp_prev_device *prev)
{
	/*
	 * If ISP_VIDEO_DMAQUEUE_QUEUED is set, DMA queue had an underrun
	 * condition, the module was paused and now we have a buffer queued
	 * on the output again. Restart the pipeline if running in continuous
	 * mode.
	 */
	if (prev->state == ISP_PIPELINE_STREAM_CONTINUOUS &&
	    prev->video_out.dmaqueue_flags & ISP_VIDEO_DMAQUEUE_QUEUED) {
		preview_enable_oneshot(prev);
		isp_video_dmaqueue_flags_clr(&prev->video_out);
	}
}

static void preview_isr_buffer(struct isp_prev_device *prev)
{
	struct isp_pipeline *pipe = to_isp_pipeline(&prev->subdev.entity);
	struct isp_buffer *buffer;
	int restart = 0;

	if (prev->input == PREVIEW_INPUT_MEMORY) {
		buffer = omap3isp_video_buffer_next(&prev->video_in,
						    prev->error);
		if (buffer != NULL)
			preview_set_inaddr(prev, buffer->isp_addr);
		pipe->state |= ISP_PIPELINE_IDLE_INPUT;
	}

	if (prev->output & PREVIEW_OUTPUT_MEMORY) {
		buffer = omap3isp_video_buffer_next(&prev->video_out,
						    prev->error);
		if (buffer != NULL) {
			preview_set_outaddr(prev, buffer->isp_addr);
			restart = 1;
		}
		pipe->state |= ISP_PIPELINE_IDLE_OUTPUT;
	}

	switch (prev->state) {
	case ISP_PIPELINE_STREAM_SINGLESHOT:
		if (isp_pipeline_ready(pipe))
			omap3isp_pipeline_set_stream(pipe,
						ISP_PIPELINE_STREAM_SINGLESHOT);
		break;

	case ISP_PIPELINE_STREAM_CONTINUOUS:
		/* If an underrun occurs, the video queue operation handler will
		 * restart the preview engine. Otherwise restart it immediately.
		 */
		if (restart)
			preview_enable_oneshot(prev);
		break;

	case ISP_PIPELINE_STREAM_STOPPED:
	default:
		return;
	}

	prev->error = 0;
}

/*
 * omap3isp_preview_isr - ISP preview engine interrupt handler
 *
 * Manage the preview engine video buffers and configure shadowed registers.
 */
void omap3isp_preview_isr(struct isp_prev_device *prev)
{
	unsigned long flags;

	if (omap3isp_module_sync_is_stopping(&prev->wait, &prev->stopping))
		return;

	spin_lock_irqsave(&prev->lock, flags);
	if (prev->shadow_update)
		goto done;

	preview_setup_hw(prev);
	preview_config_input_size(prev);

done:
	spin_unlock_irqrestore(&prev->lock, flags);

	if (prev->input == PREVIEW_INPUT_MEMORY ||
	    prev->output & PREVIEW_OUTPUT_MEMORY)
		preview_isr_buffer(prev);
	else if (prev->state == ISP_PIPELINE_STREAM_CONTINUOUS)
		preview_enable_oneshot(prev);
}

/* -----------------------------------------------------------------------------
 * ISP video operations
 */

static int preview_video_queue(struct isp_video *video,
			       struct isp_buffer *buffer)
{
	struct isp_prev_device *prev = &video->isp->isp_prev;

	if (video->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		preview_set_inaddr(prev, buffer->isp_addr);

	if (video->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		preview_set_outaddr(prev, buffer->isp_addr);

	return 0;
}

static const struct isp_video_operations preview_video_ops = {
	.queue = preview_video_queue,
};

/* -----------------------------------------------------------------------------
 * V4L2 subdev operations
 */

/*
 * preview_s_ctrl - Handle set control subdev method
 * @ctrl: pointer to v4l2 control structure
 */
static int preview_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct isp_prev_device *prev =
		container_of(ctrl->handler, struct isp_prev_device, ctrls);

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		preview_update_brightness(prev, ctrl->val);
		break;
	case V4L2_CID_CONTRAST:
		preview_update_contrast(prev, ctrl->val);
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops preview_ctrl_ops = {
	.s_ctrl = preview_s_ctrl,
};

/*
 * preview_ioctl - Handle preview module private ioctl's
 * @prev: pointer to preview context structure
 * @cmd: configuration command
 * @arg: configuration argument
 * return -EINVAL or zero on success
 */
static long preview_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct isp_prev_device *prev = v4l2_get_subdevdata(sd);

	switch (cmd) {
	case VIDIOC_OMAP3ISP_PRV_CFG:
		return preview_config(prev, arg);

	default:
		return -ENOIOCTLCMD;
	}
}

/*
 * preview_set_stream - Enable/Disable streaming on preview subdev
 * @sd    : pointer to v4l2 subdev structure
 * @enable: 1 == Enable, 0 == Disable
 * return -EINVAL or zero on success
 */
static int preview_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct isp_prev_device *prev = v4l2_get_subdevdata(sd);
	struct isp_video *video_out = &prev->video_out;
	struct isp_device *isp = to_isp_device(prev);
	struct device *dev = to_device(prev);
	unsigned long flags;

	if (prev->state == ISP_PIPELINE_STREAM_STOPPED) {
		if (enable == ISP_PIPELINE_STREAM_STOPPED)
			return 0;

		omap3isp_subclk_enable(isp, OMAP3_ISP_SUBCLK_PREVIEW);
		preview_configure(prev);
		atomic_set(&prev->stopping, 0);
		prev->error = 0;
		preview_print_status(prev);
	}

	switch (enable) {
	case ISP_PIPELINE_STREAM_CONTINUOUS:
		if (prev->output & PREVIEW_OUTPUT_MEMORY)
			omap3isp_sbl_enable(isp, OMAP3_ISP_SBL_PREVIEW_WRITE);

		if (video_out->dmaqueue_flags & ISP_VIDEO_DMAQUEUE_QUEUED ||
		    !(prev->output & PREVIEW_OUTPUT_MEMORY))
			preview_enable_oneshot(prev);

		isp_video_dmaqueue_flags_clr(video_out);
		break;

	case ISP_PIPELINE_STREAM_SINGLESHOT:
		if (prev->input == PREVIEW_INPUT_MEMORY)
			omap3isp_sbl_enable(isp, OMAP3_ISP_SBL_PREVIEW_READ);
		if (prev->output & PREVIEW_OUTPUT_MEMORY)
			omap3isp_sbl_enable(isp, OMAP3_ISP_SBL_PREVIEW_WRITE);

		preview_enable_oneshot(prev);
		break;

	case ISP_PIPELINE_STREAM_STOPPED:
		if (omap3isp_module_sync_idle(&sd->entity, &prev->wait,
					      &prev->stopping))
			dev_dbg(dev, "%s: stop timeout.\n", sd->name);
		spin_lock_irqsave(&prev->lock, flags);
		omap3isp_sbl_disable(isp, OMAP3_ISP_SBL_PREVIEW_READ);
		omap3isp_sbl_disable(isp, OMAP3_ISP_SBL_PREVIEW_WRITE);
		omap3isp_subclk_disable(isp, OMAP3_ISP_SUBCLK_PREVIEW);
		spin_unlock_irqrestore(&prev->lock, flags);
		isp_video_dmaqueue_flags_clr(video_out);
		break;
	}

	prev->state = enable;
	return 0;
}

static struct v4l2_mbus_framefmt *
__preview_get_format(struct isp_prev_device *prev, struct v4l2_subdev_fh *fh,
		     unsigned int pad, enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(fh, pad);
	else
		return &prev->formats[pad];
}

/* previewer format descriptions */
static const unsigned int preview_input_fmts[] = {
	V4L2_MBUS_FMT_SGRBG10_1X10,
	V4L2_MBUS_FMT_SRGGB10_1X10,
	V4L2_MBUS_FMT_SBGGR10_1X10,
	V4L2_MBUS_FMT_SGBRG10_1X10,
};

static const unsigned int preview_output_fmts[] = {
	V4L2_MBUS_FMT_UYVY8_1X16,
	V4L2_MBUS_FMT_YUYV8_1X16,
};

/*
 * preview_try_format - Handle try format by pad subdev method
 * @prev: ISP preview device
 * @fh : V4L2 subdev file handle
 * @pad: pad num
 * @fmt: pointer to v4l2 format structure
 */
static void preview_try_format(struct isp_prev_device *prev,
			       struct v4l2_subdev_fh *fh, unsigned int pad,
			       struct v4l2_mbus_framefmt *fmt,
			       enum v4l2_subdev_format_whence which)
{
	struct v4l2_mbus_framefmt *format;
	unsigned int max_out_width;
	enum v4l2_mbus_pixelcode pixelcode;
	unsigned int i;

	max_out_width = preview_max_out_width(prev);

	switch (pad) {
	case PREV_PAD_SINK:
		/* When reading data from the CCDC, the input size has already
		 * been mangled by the CCDC output pad so it can be accepted
		 * as-is.
		 *
		 * When reading data from memory, clamp the requested width and
		 * height. The TRM doesn't specify a minimum input height, make
		 * sure we got enough lines to enable the noise filter and color
		 * filter array interpolation.
		 */
		if (prev->input == PREVIEW_INPUT_MEMORY) {
			fmt->width = clamp_t(u32, fmt->width, PREV_MIN_WIDTH,
					     max_out_width * 8);
			fmt->height = clamp_t(u32, fmt->height, PREV_MIN_HEIGHT,
					      PREV_MAX_HEIGHT);
		}

		fmt->colorspace = V4L2_COLORSPACE_SRGB;

		for (i = 0; i < ARRAY_SIZE(preview_input_fmts); i++) {
			if (fmt->code == preview_input_fmts[i])
				break;
		}

		/* If not found, use SGRBG10 as default */
		if (i >= ARRAY_SIZE(preview_input_fmts))
			fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;
		break;

	case PREV_PAD_SOURCE:
		pixelcode = fmt->code;
		format = __preview_get_format(prev, fh, PREV_PAD_SINK, which);
		memcpy(fmt, format, sizeof(*fmt));

		/* The preview module output size is configurable through the
		 * input interface (horizontal and vertical cropping) and the
		 * averager (horizontal scaling by 1/1, 1/2, 1/4 or 1/8). In
		 * spite of this, hardcode the output size to the biggest
		 * possible value for simplicity reasons.
		 */
		switch (pixelcode) {
		case V4L2_MBUS_FMT_YUYV8_1X16:
		case V4L2_MBUS_FMT_UYVY8_1X16:
			fmt->code = pixelcode;
			break;

		default:
			fmt->code = V4L2_MBUS_FMT_YUYV8_1X16;
			break;
		}

		/* The TRM states (12.1.4.7.1.2) that 2 pixels must be cropped
		 * from the left and right sides when the input source is the
		 * CCDC. This seems not to be needed in practice, investigation
		 * is required.
		 */
		if (prev->input == PREVIEW_INPUT_CCDC)
			fmt->width -= 4;

		/* The preview module can output a maximum of 3312 pixels
		 * horizontally due to fixed memory-line sizes. Compute the
		 * horizontal averaging factor accordingly. Note that the limit
		 * applies to the noise filter and CFA interpolation blocks, so
		 * it doesn't take cropping by further blocks into account.
		 *
		 * ES 1.0 hardware revision is limited to 1280 pixels
		 * horizontally.
		 */
		fmt->width >>= fls(DIV_ROUND_UP(fmt->width, max_out_width) - 1);

		/* Assume that all blocks are enabled and crop pixels and lines
		 * accordingly. See preview_config_input_size() for more
		 * information.
		 */
		fmt->width -= 14;
		fmt->height -= 8;

		fmt->colorspace = V4L2_COLORSPACE_JPEG;
		break;
	}

	fmt->field = V4L2_FIELD_NONE;
}

/*
 * preview_enum_mbus_code - Handle pixel format enumeration
 * @sd     : pointer to v4l2 subdev structure
 * @fh     : V4L2 subdev file handle
 * @code   : pointer to v4l2_subdev_mbus_code_enum structure
 * return -EINVAL or zero on success
 */
static int preview_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	switch (code->pad) {
	case PREV_PAD_SINK:
		if (code->index >= ARRAY_SIZE(preview_input_fmts))
			return -EINVAL;

		code->code = preview_input_fmts[code->index];
		break;
	case PREV_PAD_SOURCE:
		if (code->index >= ARRAY_SIZE(preview_output_fmts))
			return -EINVAL;

		code->code = preview_output_fmts[code->index];
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int preview_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_fh *fh,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct isp_prev_device *prev = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt format;

	if (fse->index != 0)
		return -EINVAL;

	format.code = fse->code;
	format.width = 1;
	format.height = 1;
	preview_try_format(prev, fh, fse->pad, &format, V4L2_SUBDEV_FORMAT_TRY);
	fse->min_width = format.width;
	fse->min_height = format.height;

	if (format.code != fse->code)
		return -EINVAL;

	format.code = fse->code;
	format.width = -1;
	format.height = -1;
	preview_try_format(prev, fh, fse->pad, &format, V4L2_SUBDEV_FORMAT_TRY);
	fse->max_width = format.width;
	fse->max_height = format.height;

	return 0;
}

/*
 * preview_get_format - Handle get format by pads subdev method
 * @sd : pointer to v4l2 subdev structure
 * @fh : V4L2 subdev file handle
 * @fmt: pointer to v4l2 subdev format structure
 * return -EINVAL or zero on success
 */
static int preview_get_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			      struct v4l2_subdev_format *fmt)
{
	struct isp_prev_device *prev = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *format;

	format = __preview_get_format(prev, fh, fmt->pad, fmt->which);
	if (format == NULL)
		return -EINVAL;

	fmt->format = *format;
	return 0;
}

/*
 * preview_set_format - Handle set format by pads subdev method
 * @sd : pointer to v4l2 subdev structure
 * @fh : V4L2 subdev file handle
 * @fmt: pointer to v4l2 subdev format structure
 * return -EINVAL or zero on success
 */
static int preview_set_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			      struct v4l2_subdev_format *fmt)
{
	struct isp_prev_device *prev = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *format;

	format = __preview_get_format(prev, fh, fmt->pad, fmt->which);
	if (format == NULL)
		return -EINVAL;

	preview_try_format(prev, fh, fmt->pad, &fmt->format, fmt->which);
	*format = fmt->format;

	/* Propagate the format from sink to source */
	if (fmt->pad == PREV_PAD_SINK) {
		format = __preview_get_format(prev, fh, PREV_PAD_SOURCE,
					      fmt->which);
		*format = fmt->format;
		preview_try_format(prev, fh, PREV_PAD_SOURCE, format,
				   fmt->which);
	}

	return 0;
}

/*
 * preview_init_formats - Initialize formats on all pads
 * @sd: ISP preview V4L2 subdevice
 * @fh: V4L2 subdev file handle
 *
 * Initialize all pad formats with default values. If fh is not NULL, try
 * formats are initialized on the file handle. Otherwise active formats are
 * initialized on the device.
 */
static int preview_init_formats(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct v4l2_subdev_format format;

	memset(&format, 0, sizeof(format));
	format.pad = PREV_PAD_SINK;
	format.which = fh ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	format.format.code = V4L2_MBUS_FMT_SGRBG10_1X10;
	format.format.width = 4096;
	format.format.height = 4096;
	preview_set_format(sd, fh, &format);

	return 0;
}

/* subdev core operations */
static const struct v4l2_subdev_core_ops preview_v4l2_core_ops = {
	.ioctl = preview_ioctl,
};

/* subdev video operations */
static const struct v4l2_subdev_video_ops preview_v4l2_video_ops = {
	.s_stream = preview_set_stream,
};

/* subdev pad operations */
static const struct v4l2_subdev_pad_ops preview_v4l2_pad_ops = {
	.enum_mbus_code = preview_enum_mbus_code,
	.enum_frame_size = preview_enum_frame_size,
	.get_fmt = preview_get_format,
	.set_fmt = preview_set_format,
};

/* subdev operations */
static const struct v4l2_subdev_ops preview_v4l2_ops = {
	.core = &preview_v4l2_core_ops,
	.video = &preview_v4l2_video_ops,
	.pad = &preview_v4l2_pad_ops,
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops preview_v4l2_internal_ops = {
	.open = preview_init_formats,
};

/* -----------------------------------------------------------------------------
 * Media entity operations
 */

/*
 * preview_link_setup - Setup previewer connections.
 * @entity : Pointer to media entity structure
 * @local  : Pointer to local pad array
 * @remote : Pointer to remote pad array
 * @flags  : Link flags
 * return -EINVAL or zero on success
 */
static int preview_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct isp_prev_device *prev = v4l2_get_subdevdata(sd);

	switch (local->index | media_entity_type(remote->entity)) {
	case PREV_PAD_SINK | MEDIA_ENT_T_DEVNODE:
		/* read from memory */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (prev->input == PREVIEW_INPUT_CCDC)
				return -EBUSY;
			prev->input = PREVIEW_INPUT_MEMORY;
		} else {
			if (prev->input == PREVIEW_INPUT_MEMORY)
				prev->input = PREVIEW_INPUT_NONE;
		}
		break;

	case PREV_PAD_SINK | MEDIA_ENT_T_V4L2_SUBDEV:
		/* read from ccdc */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (prev->input == PREVIEW_INPUT_MEMORY)
				return -EBUSY;
			prev->input = PREVIEW_INPUT_CCDC;
		} else {
			if (prev->input == PREVIEW_INPUT_CCDC)
				prev->input = PREVIEW_INPUT_NONE;
		}
		break;

	/*
	 * The ISP core doesn't support pipelines with multiple video outputs.
	 * Revisit this when it will be implemented, and return -EBUSY for now.
	 */

	case PREV_PAD_SOURCE | MEDIA_ENT_T_DEVNODE:
		/* write to memory */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (prev->output & ~PREVIEW_OUTPUT_MEMORY)
				return -EBUSY;
			prev->output |= PREVIEW_OUTPUT_MEMORY;
		} else {
			prev->output &= ~PREVIEW_OUTPUT_MEMORY;
		}
		break;

	case PREV_PAD_SOURCE | MEDIA_ENT_T_V4L2_SUBDEV:
		/* write to resizer */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (prev->output & ~PREVIEW_OUTPUT_RESIZER)
				return -EBUSY;
			prev->output |= PREVIEW_OUTPUT_RESIZER;
		} else {
			prev->output &= ~PREVIEW_OUTPUT_RESIZER;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/* media operations */
static const struct media_entity_operations preview_media_ops = {
	.link_setup = preview_link_setup,
};

/*
 * review_init_entities - Initialize subdev and media entity.
 * @prev : Pointer to preview structure
 * return -ENOMEM or zero on success
 */
static int preview_init_entities(struct isp_prev_device *prev)
{
	struct v4l2_subdev *sd = &prev->subdev;
	struct media_pad *pads = prev->pads;
	struct media_entity *me = &sd->entity;
	int ret;

	prev->input = PREVIEW_INPUT_NONE;

	v4l2_subdev_init(sd, &preview_v4l2_ops);
	sd->internal_ops = &preview_v4l2_internal_ops;
	strlcpy(sd->name, "OMAP3 ISP preview", sizeof(sd->name));
	sd->grp_id = 1 << 16;	/* group ID for isp subdevs */
	v4l2_set_subdevdata(sd, prev);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	v4l2_ctrl_handler_init(&prev->ctrls, 2);
	v4l2_ctrl_new_std(&prev->ctrls, &preview_ctrl_ops, V4L2_CID_BRIGHTNESS,
			  ISPPRV_BRIGHT_LOW, ISPPRV_BRIGHT_HIGH,
			  ISPPRV_BRIGHT_STEP, ISPPRV_BRIGHT_DEF);
	v4l2_ctrl_new_std(&prev->ctrls, &preview_ctrl_ops, V4L2_CID_CONTRAST,
			  ISPPRV_CONTRAST_LOW, ISPPRV_CONTRAST_HIGH,
			  ISPPRV_CONTRAST_STEP, ISPPRV_CONTRAST_DEF);
	v4l2_ctrl_handler_setup(&prev->ctrls);
	sd->ctrl_handler = &prev->ctrls;

	pads[PREV_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	pads[PREV_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;

	me->ops = &preview_media_ops;
	ret = media_entity_init(me, PREV_PADS_NUM, pads, 0);
	if (ret < 0)
		return ret;

	preview_init_formats(sd, NULL);

	/* According to the OMAP34xx TRM, video buffers need to be aligned on a
	 * 32 bytes boundary. However, an undocumented hardware bug requires a
	 * 64 bytes boundary at the preview engine input.
	 */
	prev->video_in.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	prev->video_in.ops = &preview_video_ops;
	prev->video_in.isp = to_isp_device(prev);
	prev->video_in.capture_mem = PAGE_ALIGN(4096 * 4096) * 2 * 3;
	prev->video_in.bpl_alignment = 64;
	prev->video_out.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	prev->video_out.ops = &preview_video_ops;
	prev->video_out.isp = to_isp_device(prev);
	prev->video_out.capture_mem = PAGE_ALIGN(4096 * 4096) * 2 * 3;
	prev->video_out.bpl_alignment = 32;

	ret = omap3isp_video_init(&prev->video_in, "preview");
	if (ret < 0)
		return ret;

	ret = omap3isp_video_init(&prev->video_out, "preview");
	if (ret < 0)
		return ret;

	/* Connect the video nodes to the previewer subdev. */
	ret = media_entity_create_link(&prev->video_in.video.entity, 0,
			&prev->subdev.entity, PREV_PAD_SINK, 0);
	if (ret < 0)
		return ret;

	ret = media_entity_create_link(&prev->subdev.entity, PREV_PAD_SOURCE,
			&prev->video_out.video.entity, 0, 0);
	if (ret < 0)
		return ret;

	return 0;
}

void omap3isp_preview_unregister_entities(struct isp_prev_device *prev)
{
	media_entity_cleanup(&prev->subdev.entity);

	v4l2_device_unregister_subdev(&prev->subdev);
	v4l2_ctrl_handler_free(&prev->ctrls);
	omap3isp_video_unregister(&prev->video_in);
	omap3isp_video_unregister(&prev->video_out);
}

int omap3isp_preview_register_entities(struct isp_prev_device *prev,
	struct v4l2_device *vdev)
{
	int ret;

	/* Register the subdev and video nodes. */
	ret = v4l2_device_register_subdev(vdev, &prev->subdev);
	if (ret < 0)
		goto error;

	ret = omap3isp_video_register(&prev->video_in, vdev);
	if (ret < 0)
		goto error;

	ret = omap3isp_video_register(&prev->video_out, vdev);
	if (ret < 0)
		goto error;

	return 0;

error:
	omap3isp_preview_unregister_entities(prev);
	return ret;
}

/* -----------------------------------------------------------------------------
 * ISP previewer initialisation and cleanup
 */

void omap3isp_preview_cleanup(struct isp_device *isp)
{
}

/*
 * isp_preview_init - Previewer initialization.
 * @dev : Pointer to ISP device
 * return -ENOMEM or zero on success
 */
int omap3isp_preview_init(struct isp_device *isp)
{
	struct isp_prev_device *prev = &isp->isp_prev;
	int ret;

	spin_lock_init(&prev->lock);
	init_waitqueue_head(&prev->wait);
	preview_init_params(prev);

	ret = preview_init_entities(prev);
	if (ret < 0)
		goto out;

out:
	if (ret)
		omap3isp_preview_cleanup(isp);

	return ret;
}
