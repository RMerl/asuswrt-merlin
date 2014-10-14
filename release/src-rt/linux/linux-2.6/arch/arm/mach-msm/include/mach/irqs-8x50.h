/* Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_MSM_IRQS_8XXX_H
#define __ASM_ARCH_MSM_IRQS_8XXX_H

/* MSM ACPU Interrupt Numbers */

#define INT_A9_M2A_0         0
#define INT_A9_M2A_1         1
#define INT_A9_M2A_2         2
#define INT_A9_M2A_3         3
#define INT_A9_M2A_4         4
#define INT_A9_M2A_5         5
#define INT_A9_M2A_6         6
#define INT_GP_TIMER_EXP     7
#define INT_DEBUG_TIMER_EXP  8
#define INT_SIRC_0           9
#define INT_SDC3_0           10
#define INT_SDC3_1           11
#define INT_SDC4_0           12
#define INT_SDC4_1           13
#define INT_AD6_EXT_VFR      14
#define INT_USB_OTG          15
#define INT_MDDI_PRI         16
#define INT_MDDI_EXT         17
#define INT_MDDI_CLIENT      18
#define INT_MDP              19
#define INT_GRAPHICS         20
#define INT_ADM_AARM         21
#define INT_ADSP_A11         22
#define INT_ADSP_A9_A11      23
#define INT_SDC1_0           24
#define INT_SDC1_1           25
#define INT_SDC2_0           26
#define INT_SDC2_1           27
#define INT_KEYSENSE         28
#define INT_TCHSCRN_SSBI     29
#define INT_TCHSCRN1         30
#define INT_TCHSCRN2         31

#define INT_TCSR_MPRPH_SC1   (32 + 0)
#define INT_USB_FS2          (32 + 1)
#define INT_PWB_I2C          (32 + 2)
#define INT_SOFTRESET        (32 + 3)
#define INT_NAND_WR_ER_DONE  (32 + 4)
#define INT_NAND_OP_DONE     (32 + 5)
#define INT_TCSR_MPRPH_SC2   (32 + 6)
#define INT_OP_PEN           (32 + 7)
#define INT_AD_HSSD          (32 + 8)
#define INT_ARM11_PM         (32 + 9)
#define INT_SDMA_NON_SECURE  (32 + 10)
#define INT_TSIF_IRQ         (32 + 11)
#define INT_UART1DM_IRQ      (32 + 12)
#define INT_UART1DM_RX       (32 + 13)
#define INT_SDMA_SECURE      (32 + 14)
#define INT_SI2S_SLAVE       (32 + 15)
#define INT_SC_I2CPU         (32 + 16)
#define INT_SC_DBG_RDTRFULL  (32 + 17)
#define INT_SC_DBG_WDTRFULL  (32 + 18)
#define INT_SCPLL_CTL_DONE   (32 + 19)
#define INT_UART2DM_IRQ      (32 + 20)
#define INT_UART2DM_RX       (32 + 21)
#define INT_VDC_MEC          (32 + 22)
#define INT_VDC_DB           (32 + 23)
#define INT_VDC_AXI          (32 + 24)
#define INT_VFE              (32 + 25)
#define INT_USB_HS           (32 + 26)
#define INT_AUDIO_OUT0       (32 + 27)
#define INT_AUDIO_OUT1       (32 + 28)
#define INT_CRYPTO           (32 + 29)
#define INT_AD6M_IDLE        (32 + 30)
#define INT_SIRC_1           (32 + 31)

#define NR_GPIO_IRQS 165
#define NR_MSM_IRQS 64
#define NR_BOARD_IRQS 64

#endif
