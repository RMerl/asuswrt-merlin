/*
 * Copyright (C) 2010 Bluecherry, LLC www.bluecherrydvr.com
 * Copyright (C) 2010 Ben Collins <bcollins@bluecherry.net>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __SOLO6010_REGISTERS_H
#define __SOLO6010_REGISTERS_H

#include "solo6010-offsets.h"

/* Global 6010 system configuration */
#define SOLO_SYS_CFG				0x0000
#define   SOLO_SYS_CFG_FOUT_EN			0x00000001
#define   SOLO_SYS_CFG_PLL_BYPASS		0x00000002
#define   SOLO_SYS_CFG_PLL_PWDN			0x00000004
#define   SOLO_SYS_CFG_OUTDIV(__n)		(((__n) & 0x003) << 3)
#define   SOLO_SYS_CFG_FEEDBACKDIV(__n)		(((__n) & 0x1ff) << 5)
#define   SOLO_SYS_CFG_INPUTDIV(__n)		(((__n) & 0x01f) << 14)
#define   SOLO_SYS_CFG_CLOCK_DIV		0x00080000
#define   SOLO_SYS_CFG_NCLK_DELAY(__n)		(((__n) & 0x003) << 24)
#define   SOLO_SYS_CFG_PCLK_DELAY(__n)		(((__n) & 0x00f) << 26)
#define   SOLO_SYS_CFG_SDRAM64BIT		0x40000000
#define   SOLO_SYS_CFG_RESET			0x80000000

#define	SOLO_DMA_CTRL				0x0004
#define	  SOLO_DMA_CTRL_REFRESH_CYCLE(n)	((n)<<8)
/* 0=16/32MB, 1=32/64MB, 2=64/128MB, 3=128/256MB */
#define	  SOLO_DMA_CTRL_SDRAM_SIZE(n)		((n)<<6)
#define	  SOLO_DMA_CTRL_SDRAM_CLK_INVERT	(1<<5)
#define	  SOLO_DMA_CTRL_STROBE_SELECT		(1<<4)
#define	  SOLO_DMA_CTRL_READ_DATA_SELECT	(1<<3)
#define	  SOLO_DMA_CTRL_READ_CLK_SELECT		(1<<2)
#define	  SOLO_DMA_CTRL_LATENCY(n)		((n)<<0)

#define SOLO_SYS_VCLK				0x000C
#define	  SOLO_VCLK_INVERT			(1<<22)
/* 0=sys_clk/4, 1=sys_clk/2, 2=clk_in/2 of system input */
#define	  SOLO_VCLK_SELECT(n)			((n)<<20)
#define	  SOLO_VCLK_VIN1415_DELAY(n)		((n)<<14)
#define	  SOLO_VCLK_VIN1213_DELAY(n)		((n)<<12)
#define	  SOLO_VCLK_VIN1011_DELAY(n)		((n)<<10)
#define	  SOLO_VCLK_VIN0809_DELAY(n)		((n)<<8)
#define	  SOLO_VCLK_VIN0607_DELAY(n)		((n)<<6)
#define	  SOLO_VCLK_VIN0405_DELAY(n)		((n)<<4)
#define	  SOLO_VCLK_VIN0203_DELAY(n)		((n)<<2)
#define	  SOLO_VCLK_VIN0001_DELAY(n)		((n)<<0)

#define SOLO_IRQ_STAT				0x0010
#define SOLO_IRQ_ENABLE				0x0014
#define	  SOLO_IRQ_P2M(n)			(1<<((n)+17))
#define	  SOLO_IRQ_GPIO				(1<<16)
#define	  SOLO_IRQ_VIDEO_LOSS			(1<<15)
#define	  SOLO_IRQ_VIDEO_IN			(1<<14)
#define	  SOLO_IRQ_MOTION			(1<<13)
#define	  SOLO_IRQ_ATA_CMD			(1<<12)
#define	  SOLO_IRQ_ATA_DIR			(1<<11)
#define	  SOLO_IRQ_PCI_ERR			(1<<10)
#define	  SOLO_IRQ_PS2_1			(1<<9)
#define	  SOLO_IRQ_PS2_0			(1<<8)
#define	  SOLO_IRQ_SPI				(1<<7)
#define	  SOLO_IRQ_IIC				(1<<6)
#define	  SOLO_IRQ_UART(n)			(1<<((n) + 4))
#define	  SOLO_IRQ_G723				(1<<3)
#define	  SOLO_IRQ_DECODER			(1<<1)
#define	  SOLO_IRQ_ENCODER			(1<<0)

#define SOLO_CHIP_OPTION			0x001C
#define   SOLO_CHIP_ID_MASK			0x00000007

#define SOLO_EEPROM_CTRL			0x0060
#define	  SOLO_EEPROM_ACCESS_EN			(1<<7)
#define	  SOLO_EEPROM_CS			(1<<3)
#define	  SOLO_EEPROM_CLK			(1<<2)
#define	  SOLO_EEPROM_DO			(1<<1)
#define	  SOLO_EEPROM_DI			(1<<0)
#define	  SOLO_EEPROM_ENABLE			(EEPROM_ACCESS_EN | EEPROM_CS)

#define SOLO_PCI_ERR				0x0070
#define   SOLO_PCI_ERR_FATAL			0x00000001
#define   SOLO_PCI_ERR_PARITY			0x00000002
#define   SOLO_PCI_ERR_TARGET			0x00000004
#define   SOLO_PCI_ERR_TIMEOUT			0x00000008
#define   SOLO_PCI_ERR_P2M			0x00000010
#define   SOLO_PCI_ERR_ATA			0x00000020
#define   SOLO_PCI_ERR_P2M_DESC			0x00000040
#define   SOLO_PCI_ERR_FSM0(__s)		(((__s) >> 16) & 0x0f)
#define   SOLO_PCI_ERR_FSM1(__s)		(((__s) >> 20) & 0x0f)
#define   SOLO_PCI_ERR_FSM2(__s)		(((__s) >> 24) & 0x1f)

#define SOLO_P2M_BASE				0x0080

#define SOLO_P2M_CONFIG(n)			(0x0080 + ((n)*0x20))
#define	  SOLO_P2M_DMA_INTERVAL(n)		((n)<<6)/* N*32 clocks */
#define	  SOLO_P2M_CSC_BYTE_REORDER		(1<<5)	/* BGR -> RGB */
/* 0:r=[14:10] g=[9:5] b=[4:0], 1:r=[15:11] g=[10:5] b=[4:0] */
#define	  SOLO_P2M_CSC_16BIT_565		(1<<4)
#define	  SOLO_P2M_UV_SWAP			(1<<3)
#define	  SOLO_P2M_PCI_MASTER_MODE		(1<<2)
#define	  SOLO_P2M_DESC_INTR_OPT		(1<<1)	/* 1:Empty, 0:Each */
#define	  SOLO_P2M_DESC_MODE			(1<<0)

#define SOLO_P2M_DES_ADR(n)			(0x0084 + ((n)*0x20))

#define SOLO_P2M_DESC_ID(n)			(0x0088 + ((n)*0x20))
#define	  SOLO_P2M_UPDATE_ID(n)			((n)<<0)

#define SOLO_P2M_STATUS(n)			(0x008C + ((n)*0x20))
#define	  SOLO_P2M_COMMAND_DONE			(1<<8)
#define	  SOLO_P2M_CURRENT_ID(stat)		(0xff & (stat))

#define SOLO_P2M_CONTROL(n)			(0x0090 + ((n)*0x20))
#define	  SOLO_P2M_PCI_INC(n)			((n)<<20)
#define	  SOLO_P2M_REPEAT(n)			((n)<<10)
/* 0:512, 1:256, 2:128, 3:64, 4:32, 5:128(2page) */
#define	  SOLO_P2M_BURST_SIZE(n)		((n)<<7)
#define	    SOLO_P2M_BURST_512			0
#define	    SOLO_P2M_BURST_256			1
#define	    SOLO_P2M_BURST_128			2
#define	    SOLO_P2M_BURST_64			3
#define	    SOLO_P2M_BURST_32			4
#define	  SOLO_P2M_CSC_16BIT			(1<<6)	/* 0:24bit, 1:16bit */
/* 0:Y[0]<-0(OFF), 1:Y[0]<-1(ON), 2:Y[0]<-G[0], 3:Y[0]<-Bit[15] */
#define	  SOLO_P2M_ALPHA_MODE(n)		((n)<<4)
#define	  SOLO_P2M_CSC_ON			(1<<3)
#define	  SOLO_P2M_INTERRUPT_REQ		(1<<2)
#define	  SOLO_P2M_WRITE			(1<<1)
#define	  SOLO_P2M_TRANS_ON			(1<<0)

#define SOLO_P2M_EXT_CFG(n)			(0x0094 + ((n)*0x20))
#define	  SOLO_P2M_EXT_INC(n)			((n)<<20)
#define	  SOLO_P2M_COPY_SIZE(n)			((n)<<0)

#define SOLO_P2M_TAR_ADR(n)			(0x0098 + ((n)*0x20))

#define SOLO_P2M_EXT_ADR(n)			(0x009C + ((n)*0x20))

#define SOLO_P2M_BUFFER(i)			(0x2000 + ((i)*4))

#define SOLO_VI_CH_SWITCH_0			0x0100
#define SOLO_VI_CH_SWITCH_1			0x0104
#define SOLO_VI_CH_SWITCH_2			0x0108

#define	SOLO_VI_CH_ENA				0x010C
#define	SOLO_VI_CH_FORMAT			0x0110
#define	  SOLO_VI_FD_SEL_MASK(n)		((n)<<16)
#define	  SOLO_VI_PROG_MASK(n)			((n)<<0)

#define SOLO_VI_FMT_CFG				0x0114
#define	  SOLO_VI_FMT_CHECK_VCOUNT		(1<<31)
#define	  SOLO_VI_FMT_CHECK_HCOUNT		(1<<30)
#define   SOLO_VI_FMT_TEST_SIGNAL		(1<<28)

#define	SOLO_VI_PAGE_SW				0x0118
#define	  SOLO_FI_INV_DISP_LIVE(n)		((n)<<8)
#define	  SOLO_FI_INV_DISP_OUT(n)		((n)<<7)
#define	  SOLO_DISP_SYNC_FI(n)			((n)<<6)
#define	  SOLO_PIP_PAGE_ADD(n)			((n)<<3)
#define	  SOLO_NORMAL_PAGE_ADD(n)		((n)<<0)

#define	SOLO_VI_ACT_I_P				0x011C
#define	SOLO_VI_ACT_I_S				0x0120
#define	SOLO_VI_ACT_P				0x0124
#define	  SOLO_VI_FI_INVERT			(1<<31)
#define	  SOLO_VI_H_START(n)			((n)<<21)
#define	  SOLO_VI_V_START(n)			((n)<<11)
#define	  SOLO_VI_V_STOP(n)			((n)<<0)

#define SOLO_VI_STATUS0				0x0128
#define   SOLO_VI_STATUS0_PAGE(__n)		((__n) & 0x07)
#define SOLO_VI_STATUS1				0x012C

#define DISP_PAGE(stat)				((stat) & 0x07)

#define SOLO_VI_PB_CONFIG			0x0130
#define	  SOLO_VI_PB_USER_MODE			(1<<1)
#define	  SOLO_VI_PB_PAL			(1<<0)
#define SOLO_VI_PB_RANGE_HV			0x0134
#define	  SOLO_VI_PB_HSIZE(h)			((h)<<12)
#define	  SOLO_VI_PB_VSIZE(v)			((v)<<0)
#define SOLO_VI_PB_ACT_H			0x0138
#define	  SOLO_VI_PB_HSTART(n)			((n)<<12)
#define	  SOLO_VI_PB_HSTOP(n)			((n)<<0)
#define SOLO_VI_PB_ACT_V			0x013C
#define	  SOLO_VI_PB_VSTART(n)			((n)<<12)
#define	  SOLO_VI_PB_VSTOP(n)			((n)<<0)

#define	SOLO_VI_MOSAIC(ch)			(0x0140 + ((ch)*4))
#define	  SOLO_VI_MOSAIC_SX(x)			((x)<<24)
#define	  SOLO_VI_MOSAIC_EX(x)			((x)<<16)
#define	  SOLO_VI_MOSAIC_SY(x)			((x)<<8)
#define	  SOLO_VI_MOSAIC_EY(x)			((x)<<0)

#define	SOLO_VI_WIN_CTRL0(ch)			(0x0180 + ((ch)*4))
#define	SOLO_VI_WIN_CTRL1(ch)			(0x01C0 + ((ch)*4))

#define	  SOLO_VI_WIN_CHANNEL(n)		((n)<<28)

#define	  SOLO_VI_WIN_PIP(n)			((n)<<27)
#define	  SOLO_VI_WIN_SCALE(n)			((n)<<24)

#define	  SOLO_VI_WIN_SX(x)			((x)<<12)
#define	  SOLO_VI_WIN_EX(x)			((x)<<0)

#define	  SOLO_VI_WIN_SY(x)			((x)<<12)
#define	  SOLO_VI_WIN_EY(x)			((x)<<0)

#define	SOLO_VI_WIN_ON(ch)			(0x0200 + ((ch)*4))

#define SOLO_VI_WIN_SW				0x0240
#define SOLO_VI_WIN_LIVE_AUTO_MUTE		0x0244

#define	SOLO_VI_MOT_ADR				0x0260
#define	  SOLO_VI_MOTION_EN(mask)		((mask)<<16)
#define	SOLO_VI_MOT_CTRL			0x0264
#define	  SOLO_VI_MOTION_FRAME_COUNT(n)		((n)<<24)
#define	  SOLO_VI_MOTION_SAMPLE_LENGTH(n)	((n)<<16)
#define	  SOLO_VI_MOTION_INTR_START_STOP	(1<<15)
#define	  SOLO_VI_MOTION_FREEZE_DATA		(1<<14)
#define	  SOLO_VI_MOTION_SAMPLE_COUNT(n)	((n)<<0)
#define SOLO_VI_MOT_CLEAR			0x0268
#define SOLO_VI_MOT_STATUS			0x026C
#define	  SOLO_VI_MOTION_CNT(n)			((n)<<0)
#define SOLO_VI_MOTION_BORDER			0x0270
#define SOLO_VI_MOTION_BAR			0x0274
#define	  SOLO_VI_MOTION_Y_SET			(1<<29)
#define	  SOLO_VI_MOTION_Y_ADD			(1<<28)
#define	  SOLO_VI_MOTION_CB_SET			(1<<27)
#define	  SOLO_VI_MOTION_CB_ADD			(1<<26)
#define	  SOLO_VI_MOTION_CR_SET			(1<<25)
#define	  SOLO_VI_MOTION_CR_ADD			(1<<24)
#define	  SOLO_VI_MOTION_Y_VALUE(v)		((v)<<16)
#define	  SOLO_VI_MOTION_CB_VALUE(v)		((v)<<8)
#define	  SOLO_VI_MOTION_CR_VALUE(v)		((v)<<0)

#define	SOLO_VO_FMT_ENC				0x0300
#define	  SOLO_VO_SCAN_MODE_PROGRESSIVE		(1<<31)
#define	  SOLO_VO_FMT_TYPE_PAL			(1<<30)
#define   SOLO_VO_FMT_TYPE_NTSC			0
#define	  SOLO_VO_USER_SET			(1<<29)

#define	  SOLO_VO_FI_CHANGE			(1<<20)
#define	  SOLO_VO_USER_COLOR_SET_VSYNC		(1<<19)
#define	  SOLO_VO_USER_COLOR_SET_HSYNC		(1<<18)
#define	  SOLO_VO_USER_COLOR_SET_NAV		(1<<17)
#define	  SOLO_VO_USER_COLOR_SET_NAH		(1<<16)
#define	  SOLO_VO_NA_COLOR_Y(Y)			((Y)<<8)
#define	  SOLO_VO_NA_COLOR_CB(CB)		(((CB)/16)<<4)
#define	  SOLO_VO_NA_COLOR_CR(CR)		(((CR)/16)<<0)

#define	SOLO_VO_ACT_H				0x0304
#define	  SOLO_VO_H_BLANK(n)			((n)<<22)
#define	  SOLO_VO_H_START(n)			((n)<<11)
#define	  SOLO_VO_H_STOP(n)			((n)<<0)

#define	SOLO_VO_ACT_V				0x0308
#define	  SOLO_VO_V_BLANK(n)			((n)<<22)
#define	  SOLO_VO_V_START(n)			((n)<<11)
#define	  SOLO_VO_V_STOP(n)			((n)<<0)

#define	SOLO_VO_RANGE_HV			0x030C
#define	  SOLO_VO_SYNC_INVERT			(1<<24)
#define	  SOLO_VO_HSYNC_INVERT			(1<<23)
#define	  SOLO_VO_VSYNC_INVERT			(1<<22)
#define	  SOLO_VO_H_LEN(n)			((n)<<11)
#define	  SOLO_VO_V_LEN(n)			((n)<<0)

#define	SOLO_VO_DISP_CTRL			0x0310
#define	  SOLO_VO_DISP_ON			(1<<31)
#define	  SOLO_VO_DISP_ERASE_COUNT(n)		((n&0xf)<<24)
#define	  SOLO_VO_DISP_DOUBLE_SCAN		(1<<22)
#define	  SOLO_VO_DISP_SINGLE_PAGE		(1<<21)
#define	  SOLO_VO_DISP_BASE(n)			(((n)>>16) & 0xffff)

#define SOLO_VO_DISP_ERASE			0x0314
#define	  SOLO_VO_DISP_ERASE_ON			(1<<0)

#define	SOLO_VO_ZOOM_CTRL			0x0318
#define	  SOLO_VO_ZOOM_VER_ON			(1<<24)
#define	  SOLO_VO_ZOOM_HOR_ON			(1<<23)
#define	  SOLO_VO_ZOOM_V_COMP			(1<<22)
#define	  SOLO_VO_ZOOM_SX(h)			(((h)/2)<<11)
#define	  SOLO_VO_ZOOM_SY(v)			(((v)/2)<<0)

#define SOLO_VO_FREEZE_CTRL			0x031C
#define	  SOLO_VO_FREEZE_ON			(1<<1)
#define	  SOLO_VO_FREEZE_INTERPOLATION		(1<<0)

#define	SOLO_VO_BKG_COLOR			0x0320
#define	  SOLO_BG_Y(y)				((y)<<16)
#define	  SOLO_BG_U(u)				((u)<<8)
#define	  SOLO_BG_V(v)				((v)<<0)

#define	SOLO_VO_DEINTERLACE			0x0324
#define	  SOLO_VO_DEINTERLACE_THRESHOLD(n)	((n)<<8)
#define	  SOLO_VO_DEINTERLACE_EDGE_VALUE(n)	((n)<<0)

#define SOLO_VO_BORDER_LINE_COLOR		0x0330
#define SOLO_VO_BORDER_FILL_COLOR		0x0334
#define SOLO_VO_BORDER_LINE_MASK		0x0338
#define SOLO_VO_BORDER_FILL_MASK		0x033c

#define SOLO_VO_BORDER_X(n)			(0x0340+((n)*4))
#define SOLO_VO_BORDER_Y(n)			(0x0354+((n)*4))

#define SOLO_VO_CELL_EXT_SET			0x0368
#define SOLO_VO_CELL_EXT_START			0x036c
#define SOLO_VO_CELL_EXT_STOP			0x0370

#define SOLO_VO_CELL_EXT_SET2			0x0374
#define SOLO_VO_CELL_EXT_START2			0x0378
#define SOLO_VO_CELL_EXT_STOP2			0x037c

#define SOLO_VO_RECTANGLE_CTRL(n)		(0x0368+((n)*12))
#define SOLO_VO_RECTANGLE_START(n)		(0x036c+((n)*12))
#define SOLO_VO_RECTANGLE_STOP(n)		(0x0370+((n)*12))

#define SOLO_VO_CURSOR_POS			(0x0380)
#define SOLO_VO_CURSOR_CLR			(0x0384)
#define SOLO_VO_CURSOR_CLR2			(0x0388)
#define SOLO_VO_CURSOR_MASK(id)			(0x0390+((id)*4))

#define SOLO_VO_EXPANSION(id)			(0x0250+((id)*4))

#define	SOLO_OSG_CONFIG				0x03E0
#define	  SOLO_VO_OSG_ON			(1<<31)
#define	  SOLO_VO_OSG_COLOR_MUTE		(1<<28)
#define	  SOLO_VO_OSG_ALPHA_RATE(n)		((n)<<22)
#define	  SOLO_VO_OSG_ALPHA_BG_RATE(n)		((n)<<16)
#define	  SOLO_VO_OSG_BASE(offset)		(((offset)>>16)&0xffff)

#define SOLO_OSG_ERASE				0x03E4
#define	  SOLO_OSG_ERASE_ON			(0x80)
#define	  SOLO_OSG_ERASE_OFF			(0x00)

#define SOLO_VO_OSG_BLINK			0x03E8
#define	  SOLO_VO_OSG_BLINK_ON			(1<<1)
#define	  SOLO_VO_OSG_BLINK_INTREVAL18		(1<<0)

#define SOLO_CAP_BASE				0x0400
#define	  SOLO_CAP_MAX_PAGE(n)			((n)<<16)
#define	  SOLO_CAP_BASE_ADDR(n)			((n)<<0)
#define SOLO_CAP_BTW				0x0404
#define	  SOLO_CAP_PROG_BANDWIDTH(n)		((n)<<8)
#define	  SOLO_CAP_MAX_BANDWIDTH(n)		((n)<<0)

#define SOLO_DIM_SCALE1				0x0408
#define SOLO_DIM_SCALE2				0x040C
#define SOLO_DIM_SCALE3				0x0410
#define SOLO_DIM_SCALE4				0x0414
#define SOLO_DIM_SCALE5				0x0418
#define	  SOLO_DIM_V_MB_NUM_FRAME(n)		((n)<<16)
#define	  SOLO_DIM_V_MB_NUM_FIELD(n)		((n)<<8)
#define	  SOLO_DIM_H_MB_NUM(n)			((n)<<0)

#define SOLO_DIM_PROG				0x041C
#define SOLO_CAP_STATUS				0x0420

#define SOLO_CAP_CH_SCALE(ch)			(0x0440+((ch)*4))
#define SOLO_CAP_CH_COMP_ENA_E(ch)		(0x0480+((ch)*4))
#define SOLO_CAP_CH_INTV(ch)			(0x04C0+((ch)*4))
#define SOLO_CAP_CH_INTV_E(ch)			(0x0500+((ch)*4))


#define SOLO_VE_CFG0				0x0610
#define	  SOLO_VE_TWO_PAGE_MODE			(1<<31)
#define	  SOLO_VE_INTR_CTRL(n)			((n)<<24)
#define	  SOLO_VE_BLOCK_SIZE(n)			((n)<<16)
#define	  SOLO_VE_BLOCK_BASE(n)			((n)<<0)

#define SOLO_VE_CFG1				0x0614
#define	  SOLO_VE_BYTE_ALIGN(n)			((n)<<24)
#define	  SOLO_VE_INSERT_INDEX			(1<<18)
#define	  SOLO_VE_MOTION_MODE(n)		((n)<<16)
#define	  SOLO_VE_MOTION_BASE(n)		((n)<<0)

#define SOLO_VE_WMRK_POLY			0x061C
#define SOLO_VE_VMRK_INIT_KEY			0x0620
#define SOLO_VE_WMRK_STRL			0x0624
#define SOLO_VE_ENCRYP_POLY			0x0628
#define SOLO_VE_ENCRYP_INIT			0x062C
#define SOLO_VE_ATTR				0x0630
#define	  SOLO_VE_LITTLE_ENDIAN			(1<<31)
#define	  SOLO_COMP_ATTR_RN			(1<<30)
#define	  SOLO_COMP_ATTR_FCODE(n)		((n)<<27)
#define	  SOLO_COMP_TIME_INC(n)			((n)<<25)
#define	  SOLO_COMP_TIME_WIDTH(n)		((n)<<21)
#define	  SOLO_DCT_INTERVAL(n)			((n)<<16)

#define SOLO_VE_STATE(n)			(0x0640+((n)*4))
struct videnc_status {
	union {
		u32 status0;
		struct {
			u32 mp4_enc_code_size:20, sad_motion:1, vid_motion:1,
			    vop_type:2, video_channel:5, source_field_idx:1,
			    interlace:1, progressive:1;
		} status0_st;
	};
	union {
		u32 status1;
		struct {
			u32 vsize:8, hsize:8, last_queue:4, foo1:8, scale:4;
		} status1_st;
	};
	union {
		u32 status4;
		struct {
			u32 jpeg_code_size:20, interval:10, foo1:2;
		} status4_st;
	};
	union {
		u32 status9;
		struct {
			u32 channel:5, foo1:27;
		} status9_st;
	};
	union {
		u32 status10;
		struct {
			u32 mp4_code_size:20, foo:12;
		} status10_st;
	};
	union {
		u32 status11;
		struct {
			u32 last_queue:8, foo1:24;
		} status11_st;
	};
};

#define SOLO_VE_JPEG_QP_TBL			0x0670
#define SOLO_VE_JPEG_QP_CH_L			0x0674
#define SOLO_VE_JPEG_QP_CH_H			0x0678
#define SOLO_VE_JPEG_CFG			0x067C
#define SOLO_VE_JPEG_CTRL			0x0680

#define SOLO_VE_OSD_CH				0x0690
#define SOLO_VE_OSD_BASE			0x0694
#define SOLO_VE_OSD_CLR				0x0698
#define SOLO_VE_OSD_OPT				0x069C

#define SOLO_VE_CH_INTL(ch)			(0x0700+((ch)*4))
#define SOLO_VE_CH_MOT(ch)			(0x0740+((ch)*4))
#define SOLO_VE_CH_QP(ch)			(0x0780+((ch)*4))
#define SOLO_VE_CH_QP_E(ch)			(0x07C0+((ch)*4))
#define SOLO_VE_CH_GOP(ch)			(0x0800+((ch)*4))
#define SOLO_VE_CH_GOP_E(ch)			(0x0840+((ch)*4))
#define SOLO_VE_CH_REF_BASE(ch)			(0x0880+((ch)*4))
#define SOLO_VE_CH_REF_BASE_E(ch)		(0x08C0+((ch)*4))

#define SOLO_VE_MPEG4_QUE(n)			(0x0A00+((n)*8))
#define SOLO_VE_JPEG_QUE(n)			(0x0A04+((n)*8))

#define SOLO_VD_CFG0				0x0900
#define	  SOLO_VD_CFG_NO_WRITE_NO_WINDOW	(1<<24)
#define	  SOLO_VD_CFG_BUSY_WIAT_CODE		(1<<23)
#define	  SOLO_VD_CFG_BUSY_WIAT_REF		(1<<22)
#define	  SOLO_VD_CFG_BUSY_WIAT_RES		(1<<21)
#define	  SOLO_VD_CFG_BUSY_WIAT_MS		(1<<20)
#define	  SOLO_VD_CFG_SINGLE_MODE		(1<<18)
#define	  SOLO_VD_CFG_SCAL_MANUAL		(1<<17)
#define	  SOLO_VD_CFG_USER_PAGE_CTRL		(1<<16)
#define	  SOLO_VD_CFG_LITTLE_ENDIAN		(1<<15)
#define	  SOLO_VD_CFG_START_FI			(1<<14)
#define	  SOLO_VD_CFG_ERR_LOCK			(1<<13)
#define	  SOLO_VD_CFG_ERR_INT_ENA		(1<<12)
#define	  SOLO_VD_CFG_TIME_WIDTH(n)		((n)<<8)
#define	  SOLO_VD_CFG_DCT_INTERVAL(n)		((n)<<0)

#define SOLO_VD_CFG1				0x0904

#define	SOLO_VD_DEINTERLACE			0x0908
#define	  SOLO_VD_DEINTERLACE_THRESHOLD(n)	((n)<<8)
#define	  SOLO_VD_DEINTERLACE_EDGE_VALUE(n)	((n)<<0)

#define SOLO_VD_CODE_ADR			0x090C

#define SOLO_VD_CTRL				0x0910
#define	  SOLO_VD_OPER_ON			(1<<31)
#define	  SOLO_VD_MAX_ITEM(n)			((n)<<0)

#define SOLO_VD_STATUS0				0x0920
#define	  SOLO_VD_STATUS0_INTR_ACK		(1<<22)
#define	  SOLO_VD_STATUS0_INTR_EMPTY		(1<<21)
#define	  SOLO_VD_STATUS0_INTR_ERR		(1<<20)

#define SOLO_VD_STATUS1				0x0924

#define SOLO_VD_IDX0				0x0930
#define	  SOLO_VD_IDX_INTERLACE			(1<<30)
#define	  SOLO_VD_IDX_CHANNEL(n)		((n)<<24)
#define	  SOLO_VD_IDX_SIZE(n)			((n)<<0)

#define SOLO_VD_IDX1				0x0934
#define	  SOLO_VD_IDX_SRC_SCALE(n)		((n)<<28)
#define	  SOLO_VD_IDX_WINDOW(n)			((n)<<24)
#define	  SOLO_VD_IDX_DEINTERLACE		(1<<16)
#define	  SOLO_VD_IDX_H_BLOCK(n)		((n)<<8)
#define	  SOLO_VD_IDX_V_BLOCK(n)		((n)<<0)

#define SOLO_VD_IDX2				0x0938
#define	  SOLO_VD_IDX_REF_BASE_SIDE		(1<<31)
#define	  SOLO_VD_IDX_REF_BASE(n)		(((n)>>16)&0xffff)

#define SOLO_VD_IDX3				0x093C
#define	  SOLO_VD_IDX_DISP_SCALE(n)		((n)<<28)
#define	  SOLO_VD_IDX_INTERLACE_WR		(1<<27)
#define	  SOLO_VD_IDX_INTERPOL			(1<<26)
#define	  SOLO_VD_IDX_HOR2X			(1<<25)
#define	  SOLO_VD_IDX_OFFSET_X(n)		((n)<<12)
#define	  SOLO_VD_IDX_OFFSET_Y(n)		((n)<<0)

#define SOLO_VD_IDX4				0x0940
#define	  SOLO_VD_IDX_DEC_WR_PAGE(n)		((n)<<8)
#define	  SOLO_VD_IDX_DISP_RD_PAGE(n)		((n)<<0)

#define SOLO_VD_WR_PAGE(n)			(0x03F0 + ((n) * 4))


#define SOLO_GPIO_CONFIG_0			0x0B00
#define SOLO_GPIO_CONFIG_1			0x0B04
#define SOLO_GPIO_DATA_OUT			0x0B08
#define SOLO_GPIO_DATA_IN			0x0B0C
#define SOLO_GPIO_INT_ACK_STA			0x0B10
#define SOLO_GPIO_INT_ENA			0x0B14
#define SOLO_GPIO_INT_CFG_0			0x0B18
#define SOLO_GPIO_INT_CFG_1			0x0B1C


#define SOLO_IIC_CFG				0x0B20
#define	  SOLO_IIC_ENABLE			(1<<8)
#define	  SOLO_IIC_PRESCALE(n)			((n)<<0)

#define SOLO_IIC_CTRL				0x0B24
#define	  SOLO_IIC_AUTO_CLEAR			(1<<20)
#define	  SOLO_IIC_STATE_RX_ACK			(1<<19)
#define	  SOLO_IIC_STATE_BUSY			(1<<18)
#define	  SOLO_IIC_STATE_SIG_ERR		(1<<17)
#define	  SOLO_IIC_STATE_TRNS			(1<<16)
#define	  SOLO_IIC_CH_SET(n)			((n)<<5)
#define	  SOLO_IIC_ACK_EN			(1<<4)
#define	  SOLO_IIC_START			(1<<3)
#define	  SOLO_IIC_STOP				(1<<2)
#define	  SOLO_IIC_READ				(1<<1)
#define	  SOLO_IIC_WRITE			(1<<0)

#define SOLO_IIC_TXD				0x0B28
#define SOLO_IIC_RXD				0x0B2C

/*
 *	UART REGISTER
 */
#define SOLO_UART_CONTROL(n)			(0x0BA0 + ((n)*0x20))
#define	  SOLO_UART_CLK_DIV(n)			((n)<<24)
#define	  SOLO_MODEM_CTRL_EN			(1<<20)
#define	  SOLO_PARITY_ERROR_DROP		(1<<18)
#define	  SOLO_IRQ_ERR_EN			(1<<17)
#define	  SOLO_IRQ_RX_EN			(1<<16)
#define	  SOLO_IRQ_TX_EN			(1<<15)
#define	  SOLO_RX_EN				(1<<14)
#define	  SOLO_TX_EN				(1<<13)
#define	  SOLO_UART_HALF_DUPLEX			(1<<12)
#define	  SOLO_UART_LOOPBACK			(1<<11)

#define	  SOLO_BAUDRATE_230400			((0<<9)|(0<<6))
#define	  SOLO_BAUDRATE_115200			((0<<9)|(1<<6))
#define	  SOLO_BAUDRATE_57600			((0<<9)|(2<<6))
#define	  SOLO_BAUDRATE_38400			((0<<9)|(3<<6))
#define	  SOLO_BAUDRATE_19200			((0<<9)|(4<<6))
#define	  SOLO_BAUDRATE_9600			((0<<9)|(5<<6))
#define	  SOLO_BAUDRATE_4800			((0<<9)|(6<<6))
#define	  SOLO_BAUDRATE_2400			((1<<9)|(6<<6))
#define	  SOLO_BAUDRATE_1200			((2<<9)|(6<<6))
#define	  SOLO_BAUDRATE_300			((3<<9)|(6<<6))

#define	  SOLO_UART_DATA_BIT_8			(3<<4)
#define	  SOLO_UART_DATA_BIT_7			(2<<4)
#define	  SOLO_UART_DATA_BIT_6			(1<<4)
#define	  SOLO_UART_DATA_BIT_5			(0<<4)

#define	  SOLO_UART_STOP_BIT_1			(0<<2)
#define	  SOLO_UART_STOP_BIT_2			(1<<2)

#define	  SOLO_UART_PARITY_NONE			(0<<0)
#define	  SOLO_UART_PARITY_EVEN			(2<<0)
#define	  SOLO_UART_PARITY_ODD			(3<<0)

#define SOLO_UART_STATUS(n)			(0x0BA4 + ((n)*0x20))
#define	  SOLO_UART_CTS				(1<<15)
#define	  SOLO_UART_RX_BUSY			(1<<14)
#define	  SOLO_UART_OVERRUN			(1<<13)
#define	  SOLO_UART_FRAME_ERR			(1<<12)
#define	  SOLO_UART_PARITY_ERR			(1<<11)
#define	  SOLO_UART_TX_BUSY			(1<<5)

#define	  SOLO_UART_RX_BUFF_CNT(stat)		(((stat)>>6) & 0x1f)
#define	  SOLO_UART_RX_BUFF_SIZE		8
#define	  SOLO_UART_TX_BUFF_CNT(stat)		(((stat)>>0) & 0x1f)
#define	  SOLO_UART_TX_BUFF_SIZE		8

#define SOLO_UART_TX_DATA(n)			(0x0BA8 + ((n)*0x20))
#define	  SOLO_UART_TX_DATA_PUSH		(1<<8)
#define SOLO_UART_RX_DATA(n)			(0x0BAC + ((n)*0x20))
#define	  SOLO_UART_RX_DATA_POP			(1<<8)

#define SOLO_TIMER_CLOCK_NUM			0x0be0
#define SOLO_TIMER_WATCHDOG			0x0be4
#define SOLO_TIMER_USEC				0x0be8
#define SOLO_TIMER_SEC				0x0bec

#define SOLO_AUDIO_CONTROL			0x0D00
#define	  SOLO_AUDIO_ENABLE			(1<<31)
#define	  SOLO_AUDIO_MASTER_MODE		(1<<30)
#define	  SOLO_AUDIO_I2S_MODE			(1<<29)
#define	  SOLO_AUDIO_I2S_LR_SWAP		(1<<27)
#define	  SOLO_AUDIO_I2S_8BIT			(1<<26)
#define	  SOLO_AUDIO_I2S_MULTI(n)		((n)<<24)
#define	  SOLO_AUDIO_MIX_9TO0			(1<<23)
#define	  SOLO_AUDIO_DEC_9TO0_VOL(n)		((n)<<20)
#define	  SOLO_AUDIO_MIX_19TO10			(1<<19)
#define	  SOLO_AUDIO_DEC_19TO10_VOL(n)		((n)<<16)
#define	  SOLO_AUDIO_MODE(n)			((n)<<0)
#define SOLO_AUDIO_SAMPLE			0x0D04
#define	  SOLO_AUDIO_EE_MODE_ON			(1<<30)
#define	  SOLO_AUDIO_EE_ENC_CH(ch)		((ch)<<25)
#define	  SOLO_AUDIO_BITRATE(n)			((n)<<16)
#define	  SOLO_AUDIO_CLK_DIV(n)			((n)<<0)
#define SOLO_AUDIO_FDMA_INTR			0x0D08
#define	  SOLO_AUDIO_FDMA_INTERVAL(n)		((n)<<19)
#define	  SOLO_AUDIO_INTR_ORDER(n)		((n)<<16)
#define	  SOLO_AUDIO_FDMA_BASE(n)		((n)<<0)
#define SOLO_AUDIO_EVOL_0			0x0D0C
#define SOLO_AUDIO_EVOL_1			0x0D10
#define	  SOLO_AUDIO_EVOL(ch, value)		((value)<<((ch)%10))
#define SOLO_AUDIO_STA				0x0D14


#define SOLO_WATCHDOG				0x0BE4
#define WATCHDOG_STAT(status)			(status<<8)
#define WATCHDOG_TIME(sec)			(sec&0xff)

#endif /* __SOLO6010_REGISTERS_H */
