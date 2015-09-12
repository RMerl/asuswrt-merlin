/*
 * BCM47XX ARM DDR2/DDR3 memory controlers.
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id$
 */

#ifndef	_AMEMC_H
#define	_AMEMC_H

#ifdef _LANGUAGE_ASSEMBLY

#if	defined(IL_BIGENDIAN) && defined(BCMHND74K)
/* Swapped defines for big-endian code in 74K based chips */

#else	/* !IL_BIGENDIAN || !BCMHND74K */

/*
 * DDR23PHY registers
 */
#define DDR23PHY_PLL_STATUS             0x010
#define DDR23PHY_PLL_CONFIG             0x014
#define DDR23PHY_PLL_PRE_DIVIDER        0x018
#define DDR23PHY_PLL_DIVIDER            0x01c
#define DDR23PHY_STATIC_VDL_OVERRIDE    0x030
#define DDR23PHY_ZQ_PVT_COMP_CTL        0x03c
#define DDR23PHY_BL3_VDL_CALIBRATE      0x104
#define DDR23PHY_BL3_VDL_STATUS         0x108
#define DDR23PHY_BL3_READ_CONTROL       0x130
#define DDR23PHY_BL3_WR_PREAMBLE_MODE   0x148
#define DDR23PHY_BL2_VDL_CALIBRATE      0x204
#define DDR23PHY_BL2_VDL_STATUS         0x208
#define DDR23PHY_BL2_READ_CONTROL       0x230
#define DDR23PHY_BL2_WR_PREAMBLE_MODE   0x248
#define DDR23PHY_BL1_VDL_CALIBRATE      0x304
#define DDR23PHY_BL1_VDL_STATUS         0x308
#define DDR23PHY_BL1_READ_CONTROL       0x330
#define DDR23PHY_BL1_WR_PREAMBLE_MODE   0x348
#define DDR23PHY_BL0_VDL_CALIBRATE      0x404
#define DDR23PHY_BL0_VDL_STATUS         0x408
#define DDR23PHY_BL0_READ_CONTROL       0x430
#define DDR23PHY_BL0_WR_PREAMBLE_MODE   0x448

/*
 * PL341 registers
 */
#define PL341_memc_status               0x000
#define PL341_memc_cmd                  0x004
#define PL341_direct_cmd                0x008
#define PL341_memory_cfg                0x00c
#define PL341_refresh_prd               0x010
#define PL341_cas_latency               0x014
#define PL341_write_latency             0x018
#define PL341_t_mrd                     0x01c
#define PL341_t_ras                     0x020
#define PL341_t_rc                      0x024
#define PL341_t_rcd                     0x028
#define PL341_t_rfc                     0x02c
#define PL341_t_rp                      0x030
#define PL341_t_rrd                     0x034
#define PL341_t_wr                      0x038
#define PL341_t_wtr                     0x03c
#define PL341_t_xp                      0x040
#define PL341_t_xsr                     0x044
#define PL341_t_esr                     0x048
#define PL341_memory_cfg2               0x04c
#define PL341_memory_cfg3               0x050
#define PL341_t_faw                     0x054
#define PL341_chip_0_cfg                0x200
#define PL341_user_config0              0x304

#endif	/* IL_BIGENDIAN && BCMHND74K */

#endif	/* _LANGUAGE_ASSEMBLY */

#define MEMC_BURST_LENGTH       (4)

#define AI_DDRPHY_BASE       (0x1800f000)

/* Default configuration from bsp_config.h of _BCM953003RSP_ */
/* (required) PLL clock */
#define CFG_DDR_PLL_CLOCK       (331250)	/* KHz */

/* (required) CAS Latency (NOTE: could be affected by PLL clock) */
#define CFG_DDR_CAS_LATENCY     5

/* (required) t_wr (picoseconds) */
#define CFG_DDR_T_WR            15000

/* (optional) Refresh period t_refi (picoseconds) */
#define CFG_DDR_REFRESH_PRD     7800000

/* (optional) t_rfc (picoseconds) */
#define CFG_DDR_T_RFC           105000

/*
 * Convenient macros
 */
#define MEMCYCLES(psec)   (((psec) * (CFG_DDR_PLL_CLOCK) + 999999999) / 1000000000)

/*
 * Convenient macros (minimum requirement and truncated decimal)
 */
#define MEMCYCLES_MIN(psec)   ((psec) * (CFG_DDR_PLL_CLOCK) / 1000000000)

/*
 * PLL clock configuration
 */
#define PLL_NDIV_INT_VAL    (16 * (CFG_DDR_PLL_CLOCK) / 100000)

/*
 * Values for PL341 Direct Command Register
 */
#define MCHIP_CMD_PRECHARGE_ALL         (0x0 << 18)
#define MCHIP_CMD_AUTO_REFRESH          (0x1 << 18)
#define MCHIP_CMD_MODE_REG              (0x2 << 18)
#define MCHIP_CMD_NOP                   (0x3 << 18)
#define MCHIP_MODEREG_SEL(x)            ((x) << 16)
#define MCHIP_MR_WRITE_RECOVERY(x)      (((x) - 1) << 9)
#define MCHIP_MR_DLL_RESET(x)           ((x) << 8)
#define MCHIP_MR_CAS_LATENCY(x)         ((x) << 4)
#if (MEMC_BURST_LENGTH == 4)
#define MCHIP_MR_BURST_LENGTH           (2)
#else
#define MCHIP_MR_BURST_LENGTH           (3)
#endif
#define MCHIP_EMR1_DLL_DISABLE(x)       ((x) << 0)
#define MCHIP_EMR1_RTT_ODT_DISABLED     (0)
#define MCHIP_EMR1_RTT_75_OHM           (1 << 2)
#define MCHIP_EMR1_RTT_150_OHM          (1 << 6)
#define MCHIP_EMR1_RTT_50_OHM           ((1 << 6) | (1 << 2))
#define MCHIP_EMR1_OCD_CALI_EXIT        (0x0 << 7)
#define MCHIP_EMR1_OCD_CALI_DEFAULT     (0x3 << 7)

/* PVT calibration */
#define PVT_MAX_RETRY       (120)
#define PVT_MATCHED_COUNT   (3)

#endif	/* _AMEMC_H */
