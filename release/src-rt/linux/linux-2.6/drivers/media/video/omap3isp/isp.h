/*
 * isp.h
 *
 * TI OMAP3 ISP - Core
 *
 * Copyright (C) 2009-2010 Nokia Corporation
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

#ifndef OMAP3_ISP_CORE_H
#define OMAP3_ISP_CORE_H

#include <media/v4l2-device.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <plat/iommu.h>
#include <plat/iovmm.h>

#include "ispstat.h"
#include "ispccdc.h"
#include "ispreg.h"
#include "ispresizer.h"
#include "isppreview.h"
#include "ispcsiphy.h"
#include "ispcsi2.h"
#include "ispccp2.h"

#define IOMMU_FLAG (IOVMF_ENDIAN_LITTLE | IOVMF_ELSZ_8)

#define ISP_TOK_TERM		0xFFFFFFFF	/*
						 * terminating token for ISP
						 * modules reg list
						 */
#define to_isp_device(ptr_module)				\
	container_of(ptr_module, struct isp_device, isp_##ptr_module)
#define to_device(ptr_module)						\
	(to_isp_device(ptr_module)->dev)

enum isp_mem_resources {
	OMAP3_ISP_IOMEM_MAIN,
	OMAP3_ISP_IOMEM_CCP2,
	OMAP3_ISP_IOMEM_CCDC,
	OMAP3_ISP_IOMEM_HIST,
	OMAP3_ISP_IOMEM_H3A,
	OMAP3_ISP_IOMEM_PREV,
	OMAP3_ISP_IOMEM_RESZ,
	OMAP3_ISP_IOMEM_SBL,
	OMAP3_ISP_IOMEM_CSI2A_REGS1,
	OMAP3_ISP_IOMEM_CSIPHY2,
	OMAP3_ISP_IOMEM_CSI2A_REGS2,
	OMAP3_ISP_IOMEM_CSI2C_REGS1,
	OMAP3_ISP_IOMEM_CSIPHY1,
	OMAP3_ISP_IOMEM_CSI2C_REGS2,
	OMAP3_ISP_IOMEM_LAST
};

enum isp_sbl_resource {
	OMAP3_ISP_SBL_CSI1_READ		= 0x1,
	OMAP3_ISP_SBL_CSI1_WRITE	= 0x2,
	OMAP3_ISP_SBL_CSI2A_WRITE	= 0x4,
	OMAP3_ISP_SBL_CSI2C_WRITE	= 0x8,
	OMAP3_ISP_SBL_CCDC_LSC_READ	= 0x10,
	OMAP3_ISP_SBL_CCDC_WRITE	= 0x20,
	OMAP3_ISP_SBL_PREVIEW_READ	= 0x40,
	OMAP3_ISP_SBL_PREVIEW_WRITE	= 0x80,
	OMAP3_ISP_SBL_RESIZER_READ	= 0x100,
	OMAP3_ISP_SBL_RESIZER_WRITE	= 0x200,
};

enum isp_subclk_resource {
	OMAP3_ISP_SUBCLK_CCDC		= (1 << 0),
	OMAP3_ISP_SUBCLK_H3A		= (1 << 1),
	OMAP3_ISP_SUBCLK_HIST		= (1 << 2),
	OMAP3_ISP_SUBCLK_PREVIEW	= (1 << 3),
	OMAP3_ISP_SUBCLK_RESIZER	= (1 << 4),
};

enum isp_interface_type {
	ISP_INTERFACE_PARALLEL,
	ISP_INTERFACE_CSI2A_PHY2,
	ISP_INTERFACE_CCP2B_PHY1,
	ISP_INTERFACE_CCP2B_PHY2,
	ISP_INTERFACE_CSI2C_PHY1,
};

/* ISP: OMAP 34xx ES 1.0 */
#define ISP_REVISION_1_0		0x10
/* ISP2: OMAP 34xx ES 2.0, 2.1 and 3.0 */
#define ISP_REVISION_2_0		0x20
/* ISP2P: OMAP 36xx */
#define ISP_REVISION_15_0		0xF0

/*
 * struct isp_res_mapping - Map ISP io resources to ISP revision.
 * @isp_rev: ISP_REVISION_x_x
 * @map: bitmap for enum isp_mem_resources
 */
struct isp_res_mapping {
	u32 isp_rev;
	u32 map;
};

/*
 * struct isp_reg - Structure for ISP register values.
 * @reg: 32-bit Register address.
 * @val: 32-bit Register value.
 */
struct isp_reg {
	enum isp_mem_resources mmio_range;
	u32 reg;
	u32 val;
};

/**
 * struct isp_parallel_platform_data - Parallel interface platform data
 * @data_lane_shift: Data lane shifter
 *		0 - CAMEXT[13:0] -> CAM[13:0]
 *		1 - CAMEXT[13:2] -> CAM[11:0]
 *		2 - CAMEXT[13:4] -> CAM[9:0]
 *		3 - CAMEXT[13:6] -> CAM[7:0]
 * @clk_pol: Pixel clock polarity
 *		0 - Non Inverted, 1 - Inverted
 * @bridge: CCDC Bridge input control
 *		ISPCTRL_PAR_BRIDGE_DISABLE - Disable
 *		ISPCTRL_PAR_BRIDGE_LENDIAN - Little endian
 *		ISPCTRL_PAR_BRIDGE_BENDIAN - Big endian
 */
struct isp_parallel_platform_data {
	unsigned int data_lane_shift:2;
	unsigned int clk_pol:1;
	unsigned int bridge:4;
};

/**
 * struct isp_ccp2_platform_data - CCP2 interface platform data
 * @strobe_clk_pol: Strobe/clock polarity
 *		0 - Non Inverted, 1 - Inverted
 * @crc: Enable the cyclic redundancy check
 * @ccp2_mode: Enable CCP2 compatibility mode
 *		0 - MIPI-CSI1 mode, 1 - CCP2 mode
 * @phy_layer: Physical layer selection
 *		ISPCCP2_CTRL_PHY_SEL_CLOCK - Data/clock physical layer
 *		ISPCCP2_CTRL_PHY_SEL_STROBE - Data/strobe physical layer
 * @vpclk_div: Video port output clock control
 */
struct isp_ccp2_platform_data {
	unsigned int strobe_clk_pol:1;
	unsigned int crc:1;
	unsigned int ccp2_mode:1;
	unsigned int phy_layer:1;
	unsigned int vpclk_div:2;
};

/**
 * struct isp_csi2_platform_data - CSI2 interface platform data
 * @crc: Enable the cyclic redundancy check
 * @vpclk_div: Video port output clock control
 */
struct isp_csi2_platform_data {
	unsigned crc:1;
	unsigned vpclk_div:2;
};

struct isp_subdev_i2c_board_info {
	struct i2c_board_info *board_info;
	int i2c_adapter_id;
};

struct isp_v4l2_subdevs_group {
	struct isp_subdev_i2c_board_info *subdevs;
	enum isp_interface_type interface;
	union {
		struct isp_parallel_platform_data parallel;
		struct isp_ccp2_platform_data ccp2;
		struct isp_csi2_platform_data csi2;
	} bus; /* gcc < 4.6.0 chokes on anonymous union initializers */
};

struct isp_platform_data {
	struct isp_v4l2_subdevs_group *subdevs;
	void (*set_constraints)(struct isp_device *isp, bool enable);
};

struct isp_platform_callback {
	u32 (*set_xclk)(struct isp_device *isp, u32 xclk, u8 xclksel);
	int (*csiphy_config)(struct isp_csiphy *phy,
			     struct isp_csiphy_dphy_cfg *dphy,
			     struct isp_csiphy_lanes_cfg *lanes);
	void (*set_pixel_clock)(struct isp_device *isp, unsigned int pixelclk);
};

/*
 * struct isp_device - ISP device structure.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @revision: Stores current ISP module revision.
 * @irq_num: Currently used IRQ number.
 * @mmio_base: Array with kernel base addresses for ioremapped ISP register
 *             regions.
 * @mmio_base_phys: Array with physical L4 bus addresses for ISP register
 *                  regions.
 * @mmio_size: Array with ISP register regions size in bytes.
 * @raw_dmamask: Raw DMA mask
 * @stat_lock: Spinlock for handling statistics
 * @isp_mutex: Mutex for serializing requests to ISP.
 * @has_context: Context has been saved at least once and can be restored.
 * @ref_count: Reference count for handling multiple ISP requests.
 * @cam_ick: Pointer to camera interface clock structure.
 * @cam_mclk: Pointer to camera functional clock structure.
 * @dpll4_m5_ck: Pointer to DPLL4 M5 clock structure.
 * @csi2_fck: Pointer to camera CSI2 complexIO clock structure.
 * @l3_ick: Pointer to OMAP3 L3 bus interface clock.
 * @irq: Currently attached ISP ISR callbacks information structure.
 * @isp_af: Pointer to current settings for ISP AutoFocus SCM.
 * @isp_hist: Pointer to current settings for ISP Histogram SCM.
 * @isp_h3a: Pointer to current settings for ISP Auto Exposure and
 *           White Balance SCM.
 * @isp_res: Pointer to current settings for ISP Resizer.
 * @isp_prev: Pointer to current settings for ISP Preview.
 * @isp_ccdc: Pointer to current settings for ISP CCDC.
 * @iommu: Pointer to requested IOMMU instance for ISP.
 * @platform_cb: ISP driver callback function pointers for platform code
 *
 * This structure is used to store the OMAP ISP Information.
 */
struct isp_device {
	struct v4l2_device v4l2_dev;
	struct media_device media_dev;
	struct device *dev;
	u32 revision;

	/* platform HW resources */
	struct isp_platform_data *pdata;
	unsigned int irq_num;

	void __iomem *mmio_base[OMAP3_ISP_IOMEM_LAST];
	unsigned long mmio_base_phys[OMAP3_ISP_IOMEM_LAST];
	resource_size_t mmio_size[OMAP3_ISP_IOMEM_LAST];

	u64 raw_dmamask;

	/* ISP Obj */
	spinlock_t stat_lock;	/* common lock for statistic drivers */
	struct mutex isp_mutex;	/* For handling ref_count field */
	bool needs_reset;
	int has_context;
	int ref_count;
	unsigned int autoidle;
	u32 xclk_divisor[2];	/* Two clocks, a and b. */
#define ISP_CLK_CAM_ICK		0
#define ISP_CLK_CAM_MCLK	1
#define ISP_CLK_DPLL4_M5_CK	2
#define ISP_CLK_CSI2_FCK	3
#define ISP_CLK_L3_ICK		4
	struct clk *clock[5];

	/* ISP modules */
	struct ispstat isp_af;
	struct ispstat isp_aewb;
	struct ispstat isp_hist;
	struct isp_res_device isp_res;
	struct isp_prev_device isp_prev;
	struct isp_ccdc_device isp_ccdc;
	struct isp_csi2_device isp_csi2a;
	struct isp_csi2_device isp_csi2c;
	struct isp_ccp2_device isp_ccp2;
	struct isp_csiphy isp_csiphy1;
	struct isp_csiphy isp_csiphy2;

	unsigned int sbl_resources;
	unsigned int subclk_resources;

	struct iommu *iommu;

	struct isp_platform_callback platform_cb;
};

#define v4l2_dev_to_isp_device(dev) \
	container_of(dev, struct isp_device, v4l2_dev)

void omap3isp_hist_dma_done(struct isp_device *isp);

void omap3isp_flush(struct isp_device *isp);

int omap3isp_module_sync_idle(struct media_entity *me, wait_queue_head_t *wait,
			      atomic_t *stopping);

int omap3isp_module_sync_is_stopping(wait_queue_head_t *wait,
				     atomic_t *stopping);

int omap3isp_pipeline_set_stream(struct isp_pipeline *pipe,
				 enum isp_pipeline_stream_state state);
void omap3isp_configure_bridge(struct isp_device *isp,
			       enum ccdc_input_entity input,
			       const struct isp_parallel_platform_data *pdata,
			       unsigned int shift);

#define ISP_XCLK_NONE			0
#define ISP_XCLK_A			1
#define ISP_XCLK_B			2

struct isp_device *omap3isp_get(struct isp_device *isp);
void omap3isp_put(struct isp_device *isp);

void omap3isp_print_status(struct isp_device *isp);

void omap3isp_sbl_enable(struct isp_device *isp, enum isp_sbl_resource res);
void omap3isp_sbl_disable(struct isp_device *isp, enum isp_sbl_resource res);

void omap3isp_subclk_enable(struct isp_device *isp,
			    enum isp_subclk_resource res);
void omap3isp_subclk_disable(struct isp_device *isp,
			     enum isp_subclk_resource res);

int omap3isp_pipeline_pm_use(struct media_entity *entity, int use);

int omap3isp_register_entities(struct platform_device *pdev,
			       struct v4l2_device *v4l2_dev);
void omap3isp_unregister_entities(struct platform_device *pdev);

/*
 * isp_reg_readl - Read value of an OMAP3 ISP register
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @isp_mmio_range: Range to which the register offset refers to.
 * @reg_offset: Register offset to read from.
 *
 * Returns an unsigned 32 bit value with the required register contents.
 */
static inline
u32 isp_reg_readl(struct isp_device *isp, enum isp_mem_resources isp_mmio_range,
		  u32 reg_offset)
{
	return __raw_readl(isp->mmio_base[isp_mmio_range] + reg_offset);
}

/*
 * isp_reg_writel - Write value to an OMAP3 ISP register
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @reg_value: 32 bit value to write to the register.
 * @isp_mmio_range: Range to which the register offset refers to.
 * @reg_offset: Register offset to write into.
 */
static inline
void isp_reg_writel(struct isp_device *isp, u32 reg_value,
		    enum isp_mem_resources isp_mmio_range, u32 reg_offset)
{
	__raw_writel(reg_value, isp->mmio_base[isp_mmio_range] + reg_offset);
}

/*
 * isp_reg_and - Clear individual bits in an OMAP3 ISP register
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @mmio_range: Range to which the register offset refers to.
 * @reg: Register offset to work on.
 * @clr_bits: 32 bit value which would be cleared in the register.
 */
static inline
void isp_reg_clr(struct isp_device *isp, enum isp_mem_resources mmio_range,
		 u32 reg, u32 clr_bits)
{
	u32 v = isp_reg_readl(isp, mmio_range, reg);

	isp_reg_writel(isp, v & ~clr_bits, mmio_range, reg);
}

/*
 * isp_reg_set - Set individual bits in an OMAP3 ISP register
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @mmio_range: Range to which the register offset refers to.
 * @reg: Register offset to work on.
 * @set_bits: 32 bit value which would be set in the register.
 */
static inline
void isp_reg_set(struct isp_device *isp, enum isp_mem_resources mmio_range,
		 u32 reg, u32 set_bits)
{
	u32 v = isp_reg_readl(isp, mmio_range, reg);

	isp_reg_writel(isp, v | set_bits, mmio_range, reg);
}

/*
 * isp_reg_clr_set - Clear and set invidial bits in an OMAP3 ISP register
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @mmio_range: Range to which the register offset refers to.
 * @reg: Register offset to work on.
 * @clr_bits: 32 bit value which would be cleared in the register.
 * @set_bits: 32 bit value which would be set in the register.
 *
 * The clear operation is done first, and then the set operation.
 */
static inline
void isp_reg_clr_set(struct isp_device *isp, enum isp_mem_resources mmio_range,
		     u32 reg, u32 clr_bits, u32 set_bits)
{
	u32 v = isp_reg_readl(isp, mmio_range, reg);

	isp_reg_writel(isp, (v & ~clr_bits) | set_bits, mmio_range, reg);
}

static inline enum v4l2_buf_type
isp_pad_buffer_type(const struct v4l2_subdev *subdev, int pad)
{
	if (pad >= subdev->entity.num_pads)
		return 0;

	if (subdev->entity.pads[pad].flags & MEDIA_PAD_FL_SINK)
		return V4L2_BUF_TYPE_VIDEO_OUTPUT;
	else
		return V4L2_BUF_TYPE_VIDEO_CAPTURE;
}

#endif	/* OMAP3_ISP_CORE_H */
