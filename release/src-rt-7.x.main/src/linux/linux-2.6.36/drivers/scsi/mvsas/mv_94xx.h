/*
 * Marvell 88SE94xx hardware specific head file
 *
 * Copyright 2007 Red Hat, Inc.
 * Copyright 2008 Marvell. <kewei@marvell.com>
 *
 * This file is licensed under GPLv2.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
*/

#ifndef _MVS94XX_REG_H_
#define _MVS94XX_REG_H_

#include <linux/types.h>

#define MAX_LINK_RATE		SAS_LINK_RATE_6_0_GBPS

enum hw_registers {
	MVS_GBL_CTL		= 0x04,  /* global control */
	MVS_GBL_INT_STAT	= 0x00,  /* global irq status */
	MVS_GBL_PI		= 0x0C,  /* ports implemented bitmask */

	MVS_PHY_CTL		= 0x40,  /* SOC PHY Control */
	MVS_PORTS_IMP		= 0x9C,  /* SOC Port Implemented */

	MVS_GBL_PORT_TYPE	= 0xa0,  /* port type */

	MVS_CTL			= 0x100, /* SAS/SATA port configuration */
	MVS_PCS			= 0x104, /* SAS/SATA port control/status */
	MVS_CMD_LIST_LO		= 0x108, /* cmd list addr */
	MVS_CMD_LIST_HI		= 0x10C,
	MVS_RX_FIS_LO		= 0x110, /* RX FIS list addr */
	MVS_RX_FIS_HI		= 0x114,
	MVS_STP_REG_SET_0	= 0x118, /* STP/SATA Register Set Enable */
	MVS_STP_REG_SET_1	= 0x11C,
	MVS_TX_CFG		= 0x120, /* TX configuration */
	MVS_TX_LO		= 0x124, /* TX (delivery) ring addr */
	MVS_TX_HI		= 0x128,

	MVS_TX_PROD_IDX		= 0x12C, /* TX producer pointer */
	MVS_TX_CONS_IDX		= 0x130, /* TX consumer pointer (RO) */
	MVS_RX_CFG		= 0x134, /* RX configuration */
	MVS_RX_LO		= 0x138, /* RX (completion) ring addr */
	MVS_RX_HI		= 0x13C,
	MVS_RX_CONS_IDX		= 0x140, /* RX consumer pointer (RO) */

	MVS_INT_COAL		= 0x148, /* Int coalescing config */
	MVS_INT_COAL_TMOUT	= 0x14C, /* Int coalescing timeout */
	MVS_INT_STAT		= 0x150, /* Central int status */
	MVS_INT_MASK		= 0x154, /* Central int enable */
	MVS_INT_STAT_SRS_0	= 0x158, /* SATA register set status */
	MVS_INT_MASK_SRS_0	= 0x15C,
	MVS_INT_STAT_SRS_1	= 0x160,
	MVS_INT_MASK_SRS_1	= 0x164,
	MVS_NON_NCQ_ERR_0	= 0x168, /* SRS Non-specific NCQ Error */
	MVS_NON_NCQ_ERR_1	= 0x16C,
	MVS_CMD_ADDR		= 0x170, /* Command register port (addr) */
	MVS_CMD_DATA		= 0x174, /* Command register port (data) */
	MVS_MEM_PARITY_ERR	= 0x178, /* Memory parity error */

					 /* ports 1-3 follow after this */
	MVS_P0_INT_STAT		= 0x180, /* port0 interrupt status */
	MVS_P0_INT_MASK		= 0x184, /* port0 interrupt mask */
					 /* ports 5-7 follow after this */
	MVS_P4_INT_STAT		= 0x1A0, /* Port4 interrupt status */
	MVS_P4_INT_MASK		= 0x1A4, /* Port4 interrupt enable mask */

					 /* ports 1-3 follow after this */
	MVS_P0_SER_CTLSTAT	= 0x1D0, /* port0 serial control/status */
					 /* ports 5-7 follow after this */
	MVS_P4_SER_CTLSTAT	= 0x1E0, /* port4 serial control/status */

					 /* ports 1-3 follow after this */
	MVS_P0_CFG_ADDR		= 0x200, /* port0 phy register address */
	MVS_P0_CFG_DATA		= 0x204, /* port0 phy register data */
					 /* ports 5-7 follow after this */
	MVS_P4_CFG_ADDR		= 0x220, /* Port4 config address */
	MVS_P4_CFG_DATA		= 0x224, /* Port4 config data */

					 /* phys 1-3 follow after this */
	MVS_P0_VSR_ADDR		= 0x250, /* phy0 VSR address */
	MVS_P0_VSR_DATA		= 0x254, /* phy0 VSR data */
					 /* phys 1-3 follow after this */
					 /* multiplexing */
	MVS_P4_VSR_ADDR 	= 0x250, /* phy4 VSR address */
	MVS_P4_VSR_DATA 	= 0x254, /* phy4 VSR data */
	MVS_PA_VSR_ADDR		= 0x290, /* All port VSR addr */
	MVS_PA_VSR_PORT		= 0x294, /* All port VSR data */
};

enum pci_cfg_registers {
	PCR_PHY_CTL		= 0x40,
	PCR_PHY_CTL2		= 0x90,
	PCR_DEV_CTRL		= 0x78,
	PCR_LINK_STAT		= 0x82,
};

/*  SAS/SATA Vendor Specific Port Registers */
enum sas_sata_vsp_regs {
	VSR_PHY_STAT		= 0x00 * 4, /* Phy Status */
	VSR_PHY_MODE1		= 0x01 * 4, /* phy tx */
	VSR_PHY_MODE2		= 0x02 * 4, /* tx scc */
	VSR_PHY_MODE3		= 0x03 * 4, /* pll */
	VSR_PHY_MODE4		= 0x04 * 4, /* VCO */
	VSR_PHY_MODE5		= 0x05 * 4, /* Rx */
	VSR_PHY_MODE6		= 0x06 * 4, /* CDR */
	VSR_PHY_MODE7		= 0x07 * 4, /* Impedance */
	VSR_PHY_MODE8		= 0x08 * 4, /* Voltage */
	VSR_PHY_MODE9		= 0x09 * 4, /* Test */
	VSR_PHY_MODE10		= 0x0A * 4, /* Power */
	VSR_PHY_MODE11		= 0x0B * 4, /* Phy Mode */
	VSR_PHY_VS0		= 0x0C * 4, /* Vednor Specific 0 */
	VSR_PHY_VS1		= 0x0D * 4, /* Vednor Specific 1 */
};

enum chip_register_bits {
	PHY_MIN_SPP_PHYS_LINK_RATE_MASK = (0x7 << 8),
	PHY_MAX_SPP_PHYS_LINK_RATE_MASK = (0x7 << 8),
	PHY_NEG_SPP_PHYS_LINK_RATE_MASK_OFFSET = (12),
	PHY_NEG_SPP_PHYS_LINK_RATE_MASK =
			(0x3 << PHY_NEG_SPP_PHYS_LINK_RATE_MASK_OFFSET),
};

enum pci_interrupt_cause {
	/*  MAIN_IRQ_CAUSE (R10200) Bits*/
	IRQ_COM_IN_I2O_IOP0            = (1 << 0),
	IRQ_COM_IN_I2O_IOP1            = (1 << 1),
	IRQ_COM_IN_I2O_IOP2            = (1 << 2),
	IRQ_COM_IN_I2O_IOP3            = (1 << 3),
	IRQ_COM_OUT_I2O_HOS0           = (1 << 4),
	IRQ_COM_OUT_I2O_HOS1           = (1 << 5),
	IRQ_COM_OUT_I2O_HOS2           = (1 << 6),
	IRQ_COM_OUT_I2O_HOS3           = (1 << 7),
	IRQ_PCIF_TO_CPU_DRBL0          = (1 << 8),
	IRQ_PCIF_TO_CPU_DRBL1          = (1 << 9),
	IRQ_PCIF_TO_CPU_DRBL2          = (1 << 10),
	IRQ_PCIF_TO_CPU_DRBL3          = (1 << 11),
	IRQ_PCIF_DRBL0                 = (1 << 12),
	IRQ_PCIF_DRBL1                 = (1 << 13),
	IRQ_PCIF_DRBL2                 = (1 << 14),
	IRQ_PCIF_DRBL3                 = (1 << 15),
	IRQ_XOR_A                      = (1 << 16),
	IRQ_XOR_B                      = (1 << 17),
	IRQ_SAS_A                      = (1 << 18),
	IRQ_SAS_B                      = (1 << 19),
	IRQ_CPU_CNTRL                  = (1 << 20),
	IRQ_GPIO                       = (1 << 21),
	IRQ_UART                       = (1 << 22),
	IRQ_SPI                        = (1 << 23),
	IRQ_I2C                        = (1 << 24),
	IRQ_SGPIO                      = (1 << 25),
	IRQ_COM_ERR                    = (1 << 29),
	IRQ_I2O_ERR                    = (1 << 30),
	IRQ_PCIE_ERR                   = (1 << 31),
};

#define MAX_SG_ENTRY		255

struct mvs_prd_imt {
	__le32			len:22;
	u8			_r_a:2;
	u8			misc_ctl:4;
	u8			inter_sel:4;
};

struct mvs_prd {
	/* 64-bit buffer address */
	__le64			addr;
	/* 22-bit length */
	struct mvs_prd_imt	im_len;
} __attribute__ ((packed));

#define SPI_CTRL_REG_94XX           	0xc800
#define SPI_ADDR_REG_94XX            	0xc804
#define SPI_WR_DATA_REG_94XX         0xc808
#define SPI_RD_DATA_REG_94XX         	0xc80c
#define SPI_CTRL_READ_94XX         	(1U << 2)
#define SPI_ADDR_VLD_94XX         	(1U << 1)
#define SPI_CTRL_SpiStart_94XX     	(1U << 0)

#define mv_ffc(x)   ffz(x)

static inline int
mv_ffc64(u64 v)
{
	int i;
	i = mv_ffc((u32)v);
	if (i >= 0)
		return i;
	i = mv_ffc((u32)(v>>32));

	if (i != 0)
		return 32 + i;

	return -1;
}

#define r_reg_set_enable(i) \
	(((i) > 31) ? mr32(MVS_STP_REG_SET_1) : \
	mr32(MVS_STP_REG_SET_0))

#define w_reg_set_enable(i, tmp) \
	(((i) > 31) ? mw32(MVS_STP_REG_SET_1, tmp) : \
	mw32(MVS_STP_REG_SET_0, tmp))

extern const struct mvs_dispatch mvs_94xx_dispatch;
#endif
