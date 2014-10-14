/*
 * include/asm-ppc/gt64260_defs.h
 *
 * Register definitions for the Marvell/Galileo GT64260 host bridge.
 *
 * Author: Mark A. Greer <mgreer@mvista.com>
 *
 * 2001 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */
#ifndef __ASMPPC_GT64260_DEFS_H
#define __ASMPPC_GT64260_DEFS_H

/*
 * Define a macro to represent the supported version of the 64260.
 */
#define	GT64260			0x01
#define	GT64260A		0x10

/*
 *****************************************************************************
 *
 *	CPU Interface Registers
 *
 *****************************************************************************
 */

/* CPU physical address of 64260's registers */
#define GT64260_INTERNAL_SPACE_DECODE			0x0068
#define GT64260_INTERNAL_SPACE_SIZE			0x10000
#define GT64260_INTERNAL_SPACE_DEFAULT_ADDR		0x14000000

/* CPU Memory Controller Window Registers (4 windows) */
#define	GT64260_CPU_SCS_DECODE_WINDOWS			4

#define	GT64260_CPU_SCS_DECODE_0_BOT			0x0008
#define	GT64260_CPU_SCS_DECODE_0_TOP			0x0010
#define	GT64260_CPU_SCS_DECODE_1_BOT			0x0208
#define	GT64260_CPU_SCS_DECODE_1_TOP			0x0210
#define	GT64260_CPU_SCS_DECODE_2_BOT			0x0018
#define	GT64260_CPU_SCS_DECODE_2_TOP			0x0020
#define	GT64260_CPU_SCS_DECODE_3_BOT			0x0218
#define	GT64260_CPU_SCS_DECODE_3_TOP			0x0220

/* CPU Device Controller Window Registers (4 windows) */
#define	GT64260_CPU_CS_DECODE_WINDOWS			4

#define	GT64260_CPU_CS_DECODE_0_BOT			0x0028
#define	GT64260_CPU_CS_DECODE_0_TOP			0x0030
#define	GT64260_CPU_CS_DECODE_1_BOT			0x0228
#define	GT64260_CPU_CS_DECODE_1_TOP			0x0230
#define	GT64260_CPU_CS_DECODE_2_BOT			0x0248
#define	GT64260_CPU_CS_DECODE_2_TOP			0x0250
#define	GT64260_CPU_CS_DECODE_3_BOT			0x0038
#define	GT64260_CPU_CS_DECODE_3_TOP			0x0040

#define	GT64260_CPU_BOOT_CS_DECODE_0_BOT		0x0238
#define	GT64260_CPU_BOOT_CS_DECODE_0_TOP		0x0240

/* CPU Windows to PCI space (2 PCI buses each w/ 1 I/O & 4 MEM windows) */
#define	GT64260_PCI_BUSES				2
#define	GT64260_PCI_IO_WINDOWS_PER_BUS			1
#define	GT64260_PCI_MEM_WINDOWS_PER_BUS			4

#define	GT64260_CPU_PCI_SWAP_BYTE			0x00000000
#define	GT64260_CPU_PCI_SWAP_NONE			0x01000000
#define	GT64260_CPU_PCI_SWAP_BYTE_WORD			0x02000000
#define	GT64260_CPU_PCI_SWAP_WORD			0x03000000
#define	GT64260_CPU_PCI_SWAP_MASK			0x07000000

#define	GT64260_CPU_PCI_MEM_REQ64			(1<<27)

#define	GT64260_CPU_PCI_0_IO_DECODE_BOT			0x0048
#define	GT64260_CPU_PCI_0_IO_DECODE_TOP			0x0050
#define	GT64260_CPU_PCI_0_MEM_0_DECODE_BOT		0x0058
#define	GT64260_CPU_PCI_0_MEM_0_DECODE_TOP		0x0060
#define	GT64260_CPU_PCI_0_MEM_1_DECODE_BOT		0x0080
#define	GT64260_CPU_PCI_0_MEM_1_DECODE_TOP		0x0088
#define	GT64260_CPU_PCI_0_MEM_2_DECODE_BOT		0x0258
#define	GT64260_CPU_PCI_0_MEM_2_DECODE_TOP		0x0260
#define	GT64260_CPU_PCI_0_MEM_3_DECODE_BOT		0x0280
#define	GT64260_CPU_PCI_0_MEM_3_DECODE_TOP		0x0288

#define	GT64260_CPU_PCI_0_IO_REMAP			0x00f0
#define	GT64260_CPU_PCI_0_MEM_0_REMAP_LO		0x00f8
#define	GT64260_CPU_PCI_0_MEM_0_REMAP_HI		0x0320
#define	GT64260_CPU_PCI_0_MEM_1_REMAP_LO		0x0100
#define	GT64260_CPU_PCI_0_MEM_1_REMAP_HI		0x0328
#define	GT64260_CPU_PCI_0_MEM_2_REMAP_LO		0x02f8
#define	GT64260_CPU_PCI_0_MEM_2_REMAP_HI		0x0330
#define	GT64260_CPU_PCI_0_MEM_3_REMAP_LO		0x0300
#define	GT64260_CPU_PCI_0_MEM_3_REMAP_HI		0x0338

#define	GT64260_CPU_PCI_1_IO_DECODE_BOT			0x0090
#define	GT64260_CPU_PCI_1_IO_DECODE_TOP			0x0098
#define	GT64260_CPU_PCI_1_MEM_0_DECODE_BOT		0x00a0
#define	GT64260_CPU_PCI_1_MEM_0_DECODE_TOP		0x00a8
#define	GT64260_CPU_PCI_1_MEM_1_DECODE_BOT		0x00b0
#define	GT64260_CPU_PCI_1_MEM_1_DECODE_TOP		0x00b8
#define	GT64260_CPU_PCI_1_MEM_2_DECODE_BOT		0x02a0
#define	GT64260_CPU_PCI_1_MEM_2_DECODE_TOP		0x02a8
#define	GT64260_CPU_PCI_1_MEM_3_DECODE_BOT		0x02b0
#define	GT64260_CPU_PCI_1_MEM_3_DECODE_TOP		0x02b8

#define	GT64260_CPU_PCI_1_IO_REMAP			0x0108
#define	GT64260_CPU_PCI_1_MEM_0_REMAP_LO		0x0110
#define	GT64260_CPU_PCI_1_MEM_0_REMAP_HI		0x0340
#define	GT64260_CPU_PCI_1_MEM_1_REMAP_LO		0x0118
#define	GT64260_CPU_PCI_1_MEM_1_REMAP_HI		0x0348
#define	GT64260_CPU_PCI_1_MEM_2_REMAP_LO		0x0310
#define	GT64260_CPU_PCI_1_MEM_2_REMAP_HI		0x0350
#define	GT64260_CPU_PCI_1_MEM_3_REMAP_LO		0x0318
#define	GT64260_CPU_PCI_1_MEM_3_REMAP_HI		0x0358

/* CPU Control Registers */
#define GT64260_CPU_CONFIG				0x0000
#define GT64260_CPU_MODE				0x0120
#define GT64260_CPU_MASTER_CNTL				0x0160
#define GT64260_CPU_XBAR_CNTL_LO			0x0150
#define GT64260_CPU_XBAR_CNTL_HI			0x0158
#define GT64260_CPU_XBAR_TO				0x0168
#define GT64260_CPU_RR_XBAR_CNTL_LO			0x0170
#define GT64260_CPU_RR_XBAR_CNTL_HI			0x0178

/* CPU Sync Barrier Registers */
#define GT64260_CPU_SYNC_BARRIER_PCI_0			0x00c0
#define GT64260_CPU_SYNC_BARRIER_PCI_1			0x00c8

/* CPU Access Protection Registers */
#define	GT64260_CPU_PROT_WINDOWS			8

#define	GT64260_CPU_PROT_ACCPROTECT			(1<<16)
#define	GT64260_CPU_PROT_WRPROTECT			(1<<17)
#define	GT64260_CPU_PROT_CACHEPROTECT			(1<<18)

#define GT64260_CPU_PROT_BASE_0				0x0180
#define GT64260_CPU_PROT_TOP_0				0x0188
#define GT64260_CPU_PROT_BASE_1				0x0190
#define GT64260_CPU_PROT_TOP_1				0x0198
#define GT64260_CPU_PROT_BASE_2				0x01a0
#define GT64260_CPU_PROT_TOP_2				0x01a8
#define GT64260_CPU_PROT_BASE_3				0x01b0
#define GT64260_CPU_PROT_TOP_3				0x01b8
#define GT64260_CPU_PROT_BASE_4				0x01c0
#define GT64260_CPU_PROT_TOP_4				0x01c8
#define GT64260_CPU_PROT_BASE_5				0x01d0
#define GT64260_CPU_PROT_TOP_5				0x01d8
#define GT64260_CPU_PROT_BASE_6				0x01e0
#define GT64260_CPU_PROT_TOP_6				0x01e8
#define GT64260_CPU_PROT_BASE_7				0x01f0
#define GT64260_CPU_PROT_TOP_7				0x01f8

/* CPU Snoop Control Registers */
#define	GT64260_CPU_SNOOP_WINDOWS			4

#define	GT64260_CPU_SNOOP_NONE				0x00000000
#define	GT64260_CPU_SNOOP_WT				0x00010000
#define	GT64260_CPU_SNOOP_WB				0x00020000
#define	GT64260_CPU_SNOOP_MASK				0x00030000
#define	GT64260_CPU_SNOOP_ALL_BITS			GT64260_CPU_SNOOP_MASK

#define GT64260_CPU_SNOOP_BASE_0			0x0380
#define GT64260_CPU_SNOOP_TOP_0				0x0388
#define GT64260_CPU_SNOOP_BASE_1			0x0390
#define GT64260_CPU_SNOOP_TOP_1				0x0398
#define GT64260_CPU_SNOOP_BASE_2			0x03a0
#define GT64260_CPU_SNOOP_TOP_2				0x03a8
#define GT64260_CPU_SNOOP_BASE_3			0x03b0
#define GT64260_CPU_SNOOP_TOP_3				0x03b8

/* CPU Error Report Registers */
#define GT64260_CPU_ERR_ADDR_LO				0x0070
#define GT64260_CPU_ERR_ADDR_HI				0x0078
#define GT64260_CPU_ERR_DATA_LO				0x0128
#define GT64260_CPU_ERR_DATA_HI				0x0130
#define GT64260_CPU_ERR_PARITY				0x0138
#define GT64260_CPU_ERR_CAUSE				0x0140
#define GT64260_CPU_ERR_MASK				0x0148


/*
 *****************************************************************************
 *
 *	SDRAM Cotnroller Registers
 *
 *****************************************************************************
 */

/* SDRAM Config Registers */
#define	GT64260_SDRAM_CONFIG				0x0448
#define	GT64260_SDRAM_OPERATION_MODE			0x0474
#define	GT64260_SDRAM_ADDR_CNTL				0x047c
#define	GT64260_SDRAM_TIMING_PARAMS			0x04b4
#define	GT64260_SDRAM_UMA_CNTL				0x04a4
#define	GT64260_SDRAM_XBAR_CNTL_LO			0x04a8
#define	GT64260_SDRAM_XBAR_CNTL_HI			0x04ac
#define	GT64260_SDRAM_XBAR_CNTL_TO			0x04b0

/* SDRAM Banks Parameters Registers */
#define	GT64260_SDRAM_BANK_PARAMS_0			0x044c
#define	GT64260_SDRAM_BANK_PARAMS_1			0x0450
#define	GT64260_SDRAM_BANK_PARAMS_2			0x0454
#define	GT64260_SDRAM_BANK_PARAMS_3			0x0458

/* SDRAM Error Report Registers */
#define	GT64260_SDRAM_ERR_DATA_LO			0x0484
#define	GT64260_SDRAM_ERR_DATA_HI			0x0480
#define	GT64260_SDRAM_ERR_ADDR				0x0490
#define	GT64260_SDRAM_ERR_ECC_RCVD			0x0488
#define	GT64260_SDRAM_ERR_ECC_CALC			0x048c
#define	GT64260_SDRAM_ERR_ECC_CNTL			0x0494
#define	GT64260_SDRAM_ERR_ECC_ERR_CNT			0x0498


/*
 *****************************************************************************
 *
 *	Device/BOOT Cotnroller Registers
 *
 *****************************************************************************
 */

/* Device Control Registers */
#define	GT64260_DEV_BANK_PARAMS_0			0x045c
#define	GT64260_DEV_BANK_PARAMS_1			0x0460
#define	GT64260_DEV_BANK_PARAMS_2			0x0464
#define	GT64260_DEV_BANK_PARAMS_3			0x0468
#define	GT64260_DEV_BOOT_PARAMS				0x046c
#define	GT64260_DEV_IF_CNTL				0x04c0
#define	GT64260_DEV_IF_XBAR_CNTL_LO			0x04c8
#define	GT64260_DEV_IF_XBAR_CNTL_HI			0x04cc
#define	GT64260_DEV_IF_XBAR_CNTL_TO			0x04c4

/* Device Interrupt Registers */
#define	GT64260_DEV_INTR_CAUSE				0x04d0
#define	GT64260_DEV_INTR_MASK				0x04d4
#define	GT64260_DEV_INTR_ERR_ADDR			0x04d8


/*
 *****************************************************************************
 *
 *	PCI Bridge Interface Registers
 *
 *****************************************************************************
 */

/* PCI Configuration Access Registers */
#define	GT64260_PCI_0_CONFIG_ADDR			0x0cf8
#define	GT64260_PCI_0_CONFIG_DATA			0x0cfc
#define	GT64260_PCI_0_IACK				0x0c34

#define	GT64260_PCI_1_CONFIG_ADDR			0x0c78
#define	GT64260_PCI_1_CONFIG_DATA			0x0c7c
#define	GT64260_PCI_1_IACK				0x0cb4

/* PCI Control Registers */
#define	GT64260_PCI_0_CMD				0x0c00
#define	GT64260_PCI_0_MODE				0x0d00
#define	GT64260_PCI_0_TO_RETRY				0x0c04
#define	GT64260_PCI_0_RD_BUF_DISCARD_TIMER		0x0d04
#define	GT64260_PCI_0_MSI_TRIGGER_TIMER			0x0c38
#define	GT64260_PCI_0_ARBITER_CNTL			0x1d00
#define	GT64260_PCI_0_XBAR_CNTL_LO			0x1d08
#define	GT64260_PCI_0_XBAR_CNTL_HI			0x1d0c
#define	GT64260_PCI_0_XBAR_CNTL_TO			0x1d04
#define	GT64260_PCI_0_RD_RESP_XBAR_CNTL_LO		0x1d18
#define	GT64260_PCI_0_RD_RESP_XBAR_CNTL_HI		0x1d1c
#define	GT64260_PCI_0_SYNC_BARRIER			0x1d10
#define	GT64260_PCI_0_P2P_CONFIG			0x1d14
#define	GT64260_PCI_0_P2P_SWAP_CNTL			0x1d54

#define	GT64260_PCI_1_CMD				0x0c80
#define	GT64260_PCI_1_MODE				0x0d80
#define	GT64260_PCI_1_TO_RETRY				0x0c84
#define	GT64260_PCI_1_RD_BUF_DISCARD_TIMER		0x0d84
#define	GT64260_PCI_1_MSI_TRIGGER_TIMER			0x0cb8
#define	GT64260_PCI_1_ARBITER_CNTL			0x1d80
#define	GT64260_PCI_1_XBAR_CNTL_LO			0x1d88
#define	GT64260_PCI_1_XBAR_CNTL_HI			0x1d8c
#define	GT64260_PCI_1_XBAR_CNTL_TO			0x1d84
#define	GT64260_PCI_1_RD_RESP_XBAR_CNTL_LO		0x1d98
#define	GT64260_PCI_1_RD_RESP_XBAR_CNTL_HI		0x1d9c
#define	GT64260_PCI_1_SYNC_BARRIER			0x1d90
#define	GT64260_PCI_1_P2P_CONFIG			0x1d94
#define	GT64260_PCI_1_P2P_SWAP_CNTL			0x1dd4

/* PCI Access Control Regions Registers */
#define	GT64260_PCI_ACC_CNTL_WINDOWS			8

#define	GT64260_PCI_ACC_CNTL_PREFETCHEN			(1<<12)
#define	GT64260_PCI_ACC_CNTL_DREADEN			(1<<13)
#define	GT64260_PCI_ACC_CNTL_RDPREFETCH			(1<<16)
#define	GT64260_PCI_ACC_CNTL_RDLINEPREFETCH		(1<<17)
#define	GT64260_PCI_ACC_CNTL_RDMULPREFETCH		(1<<18)
#define	GT64260_PCI_ACC_CNTL_MBURST_4_WORDS		0x00000000
#define	GT64260_PCI_ACC_CNTL_MBURST_8_WORDS		0x00100000
#define	GT64260_PCI_ACC_CNTL_MBURST_16_WORDS		0x00200000
#define	GT64260_PCI_ACC_CNTL_MBURST_MASK		0x00300000
#define	GT64260_PCI_ACC_CNTL_SWAP_BYTE			0x00000000
#define	GT64260_PCI_ACC_CNTL_SWAP_NONE			0x01000000
#define	GT64260_PCI_ACC_CNTL_SWAP_BYTE_WORD		0x02000000
#define	GT64260_PCI_ACC_CNTL_SWAP_WORD			0x03000000
#define	GT64260_PCI_ACC_CNTL_SWAP_MASK			0x03000000
#define	GT64260_PCI_ACC_CNTL_ACCPROT			(1<<28)
#define	GT64260_PCI_ACC_CNTL_WRPROT			(1<<29)

#define	GT64260_PCI_ACC_CNTL_ALL_BITS	(GT64260_PCI_ACC_CNTL_PREFETCHEN |    \
					 GT64260_PCI_ACC_CNTL_DREADEN |       \
					 GT64260_PCI_ACC_CNTL_RDPREFETCH |    \
					 GT64260_PCI_ACC_CNTL_RDLINEPREFETCH |\
					 GT64260_PCI_ACC_CNTL_RDMULPREFETCH | \
					 GT64260_PCI_ACC_CNTL_MBURST_MASK |   \
					 GT64260_PCI_ACC_CNTL_SWAP_MASK |     \
					 GT64260_PCI_ACC_CNTL_ACCPROT|        \
					 GT64260_PCI_ACC_CNTL_WRPROT)

#define	GT64260_PCI_0_ACC_CNTL_0_BASE_LO		0x1e00
#define	GT64260_PCI_0_ACC_CNTL_0_BASE_HI		0x1e04
#define	GT64260_PCI_0_ACC_CNTL_0_TOP			0x1e08
#define	GT64260_PCI_0_ACC_CNTL_1_BASE_LO		0x1e10
#define	GT64260_PCI_0_ACC_CNTL_1_BASE_HI		0x1e14
#define	GT64260_PCI_0_ACC_CNTL_1_TOP			0x1e18
#define	GT64260_PCI_0_ACC_CNTL_2_BASE_LO		0x1e20
#define	GT64260_PCI_0_ACC_CNTL_2_BASE_HI		0x1e24
#define	GT64260_PCI_0_ACC_CNTL_2_TOP			0x1e28
#define	GT64260_PCI_0_ACC_CNTL_3_BASE_LO		0x1e30
#define	GT64260_PCI_0_ACC_CNTL_3_BASE_HI		0x1e34
#define	GT64260_PCI_0_ACC_CNTL_3_TOP			0x1e38
#define	GT64260_PCI_0_ACC_CNTL_4_BASE_LO		0x1e40
#define	GT64260_PCI_0_ACC_CNTL_4_BASE_HI		0x1e44
#define	GT64260_PCI_0_ACC_CNTL_4_TOP			0x1e48
#define	GT64260_PCI_0_ACC_CNTL_5_BASE_LO		0x1e50
#define	GT64260_PCI_0_ACC_CNTL_5_BASE_HI		0x1e54
#define	GT64260_PCI_0_ACC_CNTL_5_TOP			0x1e58
#define	GT64260_PCI_0_ACC_CNTL_6_BASE_LO		0x1e60
#define	GT64260_PCI_0_ACC_CNTL_6_BASE_HI		0x1e64
#define	GT64260_PCI_0_ACC_CNTL_6_TOP			0x1e68
#define	GT64260_PCI_0_ACC_CNTL_7_BASE_LO		0x1e70
#define	GT64260_PCI_0_ACC_CNTL_7_BASE_HI		0x1e74
#define	GT64260_PCI_0_ACC_CNTL_7_TOP			0x1e78

#define	GT64260_PCI_1_ACC_CNTL_0_BASE_LO		0x1e80
#define	GT64260_PCI_1_ACC_CNTL_0_BASE_HI		0x1e84
#define	GT64260_PCI_1_ACC_CNTL_0_TOP			0x1e88
#define	GT64260_PCI_1_ACC_CNTL_1_BASE_LO		0x1e90
#define	GT64260_PCI_1_ACC_CNTL_1_BASE_HI		0x1e94
#define	GT64260_PCI_1_ACC_CNTL_1_TOP			0x1e98
#define	GT64260_PCI_1_ACC_CNTL_2_BASE_LO		0x1ea0
#define	GT64260_PCI_1_ACC_CNTL_2_BASE_HI		0x1ea4
#define	GT64260_PCI_1_ACC_CNTL_2_TOP			0x1ea8
#define	GT64260_PCI_1_ACC_CNTL_3_BASE_LO		0x1eb0
#define	GT64260_PCI_1_ACC_CNTL_3_BASE_HI		0x1eb4
#define	GT64260_PCI_1_ACC_CNTL_3_TOP			0x1eb8
#define	GT64260_PCI_1_ACC_CNTL_4_BASE_LO		0x1ec0
#define	GT64260_PCI_1_ACC_CNTL_4_BASE_HI		0x1ec4
#define	GT64260_PCI_1_ACC_CNTL_4_TOP			0x1ec8
#define	GT64260_PCI_1_ACC_CNTL_5_BASE_LO		0x1ed0
#define	GT64260_PCI_1_ACC_CNTL_5_BASE_HI		0x1ed4
#define	GT64260_PCI_1_ACC_CNTL_5_TOP			0x1ed8
#define	GT64260_PCI_1_ACC_CNTL_6_BASE_LO		0x1ee0
#define	GT64260_PCI_1_ACC_CNTL_6_BASE_HI		0x1ee4
#define	GT64260_PCI_1_ACC_CNTL_6_TOP			0x1ee8
#define	GT64260_PCI_1_ACC_CNTL_7_BASE_LO		0x1ef0
#define	GT64260_PCI_1_ACC_CNTL_7_BASE_HI		0x1ef4
#define	GT64260_PCI_1_ACC_CNTL_7_TOP			0x1ef8

/* PCI Snoop Control Registers */
#define	GT64260_PCI_SNOOP_WINDOWS			4

#define	GT64260_PCI_SNOOP_NONE				0x00000000
#define	GT64260_PCI_SNOOP_WT				0x00001000
#define	GT64260_PCI_SNOOP_WB				0x00002000

#define	GT64260_PCI_0_SNOOP_0_BASE_LO			0x1f00
#define	GT64260_PCI_0_SNOOP_0_BASE_HI			0x1f04
#define	GT64260_PCI_0_SNOOP_0_TOP			0x1f08
#define	GT64260_PCI_0_SNOOP_1_BASE_LO			0x1f10
#define	GT64260_PCI_0_SNOOP_1_BASE_HI			0x1f14
#define	GT64260_PCI_0_SNOOP_1_TOP			0x1f18
#define	GT64260_PCI_0_SNOOP_2_BASE_LO			0x1f20
#define	GT64260_PCI_0_SNOOP_2_BASE_HI			0x1f24
#define	GT64260_PCI_0_SNOOP_2_TOP			0x1f28
#define	GT64260_PCI_0_SNOOP_3_BASE_LO			0x1f30
#define	GT64260_PCI_0_SNOOP_3_BASE_HI			0x1f34
#define	GT64260_PCI_0_SNOOP_3_TOP			0x1f38

#define	GT64260_PCI_1_SNOOP_0_BASE_LO			0x1f80
#define	GT64260_PCI_1_SNOOP_0_BASE_HI			0x1f84
#define	GT64260_PCI_1_SNOOP_0_TOP			0x1f88
#define	GT64260_PCI_1_SNOOP_1_BASE_LO			0x1f90
#define	GT64260_PCI_1_SNOOP_1_BASE_HI			0x1f94
#define	GT64260_PCI_1_SNOOP_1_TOP			0x1f98
#define	GT64260_PCI_1_SNOOP_2_BASE_LO			0x1fa0
#define	GT64260_PCI_1_SNOOP_2_BASE_HI			0x1fa4
#define	GT64260_PCI_1_SNOOP_2_TOP			0x1fa8
#define	GT64260_PCI_1_SNOOP_3_BASE_LO			0x1fb0
#define	GT64260_PCI_1_SNOOP_3_BASE_HI			0x1fb4
#define	GT64260_PCI_1_SNOOP_3_TOP			0x1fb8

/* PCI Error Report Registers */
#define GT64260_PCI_0_ERR_SERR_MASK			0x0c28
#define GT64260_PCI_0_ERR_ADDR_LO			0x1d40
#define GT64260_PCI_0_ERR_ADDR_HI			0x1d44
#define GT64260_PCI_0_ERR_DATA_LO			0x1d48
#define GT64260_PCI_0_ERR_DATA_HI			0x1d4c
#define GT64260_PCI_0_ERR_CMD				0x1d50
#define GT64260_PCI_0_ERR_CAUSE				0x1d58
#define GT64260_PCI_0_ERR_MASK				0x1d5c

#define GT64260_PCI_1_ERR_SERR_MASK			0x0ca8
#define GT64260_PCI_1_ERR_ADDR_LO			0x1dc0
#define GT64260_PCI_1_ERR_ADDR_HI			0x1dc4
#define GT64260_PCI_1_ERR_DATA_LO			0x1dc8
#define GT64260_PCI_1_ERR_DATA_HI			0x1dcc
#define GT64260_PCI_1_ERR_CMD				0x1dd0
#define GT64260_PCI_1_ERR_CAUSE				0x1dd8
#define GT64260_PCI_1_ERR_MASK				0x1ddc

/* PCI Slave Address Decoding Registers */
#define	GT64260_PCI_SCS_WINDOWS				4
#define	GT64260_PCI_CS_WINDOWS				4
#define	GT64260_PCI_BOOT_WINDOWS			1
#define	GT64260_PCI_P2P_MEM_WINDOWS			2
#define	GT64260_PCI_P2P_IO_WINDOWS			1
#define	GT64260_PCI_DAC_SCS_WINDOWS			4
#define	GT64260_PCI_DAC_CS_WINDOWS			4
#define	GT64260_PCI_DAC_BOOT_WINDOWS			1
#define	GT64260_PCI_DAC_P2P_MEM_WINDOWS			2

#define	GT64260_PCI_0_SLAVE_SCS_0_SIZE			0x0c08
#define	GT64260_PCI_0_SLAVE_SCS_1_SIZE			0x0d08
#define	GT64260_PCI_0_SLAVE_SCS_2_SIZE			0x0c0c
#define	GT64260_PCI_0_SLAVE_SCS_3_SIZE			0x0d0c
#define	GT64260_PCI_0_SLAVE_CS_0_SIZE			0x0c10
#define	GT64260_PCI_0_SLAVE_CS_1_SIZE			0x0d10
#define	GT64260_PCI_0_SLAVE_CS_2_SIZE			0x0d18
#define	GT64260_PCI_0_SLAVE_CS_3_SIZE			0x0c14
#define	GT64260_PCI_0_SLAVE_BOOT_SIZE			0x0d14
#define	GT64260_PCI_0_SLAVE_P2P_MEM_0_SIZE		0x0d1c
#define	GT64260_PCI_0_SLAVE_P2P_MEM_1_SIZE		0x0d20
#define	GT64260_PCI_0_SLAVE_P2P_IO_SIZE			0x0d24
#define	GT64260_PCI_0_SLAVE_CPU_SIZE			0x0d28

#define	GT64260_PCI_0_SLAVE_DAC_SCS_0_SIZE		0x0e00
#define	GT64260_PCI_0_SLAVE_DAC_SCS_1_SIZE		0x0e04
#define	GT64260_PCI_0_SLAVE_DAC_SCS_2_SIZE		0x0e08
#define	GT64260_PCI_0_SLAVE_DAC_SCS_3_SIZE		0x0e0c
#define	GT64260_PCI_0_SLAVE_DAC_CS_0_SIZE		0x0e10
#define	GT64260_PCI_0_SLAVE_DAC_CS_1_SIZE		0x0e14
#define	GT64260_PCI_0_SLAVE_DAC_CS_2_SIZE		0x0e18
#define	GT64260_PCI_0_SLAVE_DAC_CS_3_SIZE		0x0e1c
#define	GT64260_PCI_0_SLAVE_DAC_BOOT_SIZE		0x0e20
#define	GT64260_PCI_0_SLAVE_DAC_P2P_MEM_0_SIZE		0x0e24
#define	GT64260_PCI_0_SLAVE_DAC_P2P_MEM_1_SIZE		0x0e28
#define	GT64260_PCI_0_SLAVE_DAC_CPU_SIZE		0x0e2c

#define	GT64260_PCI_0_SLAVE_EXP_ROM_SIZE		0x0d2c

#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_SCS_0		(1<<0)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_SCS_1		(1<<1)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_SCS_2		(1<<2)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_SCS_3		(1<<3)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_CS_0		(1<<4)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_CS_1		(1<<5)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_CS_2		(1<<6)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_CS_3		(1<<7)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_BOOT		(1<<8)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_REG_MEM	(1<<9)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_REG_IO	(1<<10)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_P2P_MEM_0	(1<<11)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_P2P_MEM_1	(1<<12)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_P2P_IO	(1<<13)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_CPU		(1<<14)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_DAC_SCS_0	(1<<15)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_DAC_SCS_1	(1<<16)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_DAC_SCS_2	(1<<17)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_DAC_SCS_3	(1<<18)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_DAC_CS_0	(1<<19)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_DAC_CS_1	(1<<20)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_DAC_CS_2	(1<<21)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_DAC_CS_3	(1<<22)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_DAC_BOOT	(1<<23)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_DAC_P2P_MEM_0	(1<<24)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_DAC_P2P_MEM_1	(1<<25)
#define	GT64260_PCI_SLAVE_BAR_REG_ENABLES_DAC_CPU	(1<<26)

#define	GT64260_PCI_0_SLAVE_BAR_REG_ENABLES		0x0c3c
#define	GT64260_PCI_0_SLAVE_SCS_0_REMAP			0x0c48
#define	GT64260_PCI_0_SLAVE_SCS_1_REMAP			0x0d48
#define	GT64260_PCI_0_SLAVE_SCS_2_REMAP			0x0c4c
#define	GT64260_PCI_0_SLAVE_SCS_3_REMAP			0x0d4c
#define	GT64260_PCI_0_SLAVE_CS_0_REMAP			0x0c50
#define	GT64260_PCI_0_SLAVE_CS_1_REMAP			0x0d50
#define	GT64260_PCI_0_SLAVE_CS_2_REMAP			0x0d58
#define	GT64260_PCI_0_SLAVE_CS_3_REMAP			0x0c54
#define	GT64260_PCI_0_SLAVE_BOOT_REMAP			0x0d54
#define	GT64260_PCI_0_SLAVE_P2P_MEM_0_REMAP_LO		0x0d5c
#define	GT64260_PCI_0_SLAVE_P2P_MEM_0_REMAP_HI		0x0d60
#define	GT64260_PCI_0_SLAVE_P2P_MEM_1_REMAP_LO		0x0d64
#define	GT64260_PCI_0_SLAVE_P2P_MEM_1_REMAP_HI		0x0d68
#define	GT64260_PCI_0_SLAVE_P2P_IO_REMAP		0x0d6c
#define	GT64260_PCI_0_SLAVE_CPU_REMAP			0x0d70

#define	GT64260_PCI_0_SLAVE_DAC_SCS_0_REMAP		0x0f00
#define	GT64260_PCI_0_SLAVE_DAC_SCS_1_REMAP		0x0f04
#define	GT64260_PCI_0_SLAVE_DAC_SCS_2_REMAP		0x0f08
#define	GT64260_PCI_0_SLAVE_DAC_SCS_3_REMAP		0x0f0c
#define	GT64260_PCI_0_SLAVE_DAC_CS_0_REMAP		0x0f10
#define	GT64260_PCI_0_SLAVE_DAC_CS_1_REMAP		0x0f14
#define	GT64260_PCI_0_SLAVE_DAC_CS_2_REMAP		0x0f18
#define	GT64260_PCI_0_SLAVE_DAC_CS_3_REMAP		0x0f1c
#define	GT64260_PCI_0_SLAVE_DAC_BOOT_REMAP		0x0f20
#define	GT64260_PCI_0_SLAVE_DAC_P2P_MEM_0_REMAP_LO	0x0f24
#define	GT64260_PCI_0_SLAVE_DAC_P2P_MEM_0_REMAP_HI	0x0f28
#define	GT64260_PCI_0_SLAVE_DAC_P2P_MEM_1_REMAP_LO	0x0f2c
#define	GT64260_PCI_0_SLAVE_DAC_P2P_MEM_1_REMAP_HI	0x0f30
#define	GT64260_PCI_0_SLAVE_DAC_CPU_REMAP		0x0f34

#define	GT64260_PCI_0_SLAVE_EXP_ROM_REMAP		0x0f38
#define	GT64260_PCI_0_SLAVE_PCI_DECODE_CNTL		0x0d3c

#define	GT64260_PCI_1_SLAVE_SCS_0_SIZE			0x0c88
#define	GT64260_PCI_1_SLAVE_SCS_1_SIZE			0x0d88
#define	GT64260_PCI_1_SLAVE_SCS_2_SIZE			0x0c8c
#define	GT64260_PCI_1_SLAVE_SCS_3_SIZE			0x0d8c
#define	GT64260_PCI_1_SLAVE_CS_0_SIZE			0x0c90
#define	GT64260_PCI_1_SLAVE_CS_1_SIZE			0x0d90
#define	GT64260_PCI_1_SLAVE_CS_2_SIZE			0x0d98
#define	GT64260_PCI_1_SLAVE_CS_3_SIZE			0x0c94
#define	GT64260_PCI_1_SLAVE_BOOT_SIZE			0x0d94
#define	GT64260_PCI_1_SLAVE_P2P_MEM_0_SIZE		0x0d9c
#define	GT64260_PCI_1_SLAVE_P2P_MEM_1_SIZE		0x0da0
#define	GT64260_PCI_1_SLAVE_P2P_IO_SIZE			0x0da4
#define	GT64260_PCI_1_SLAVE_CPU_SIZE			0x0da8

#define	GT64260_PCI_1_SLAVE_DAC_SCS_0_SIZE		0x0e80
#define	GT64260_PCI_1_SLAVE_DAC_SCS_1_SIZE		0x0e84
#define	GT64260_PCI_1_SLAVE_DAC_SCS_2_SIZE		0x0e88
#define	GT64260_PCI_1_SLAVE_DAC_SCS_3_SIZE		0x0e8c
#define	GT64260_PCI_1_SLAVE_DAC_CS_0_SIZE		0x0e90
#define	GT64260_PCI_1_SLAVE_DAC_CS_1_SIZE		0x0e94
#define	GT64260_PCI_1_SLAVE_DAC_CS_2_SIZE		0x0e98
#define	GT64260_PCI_1_SLAVE_DAC_CS_3_SIZE		0x0e9c
#define	GT64260_PCI_1_SLAVE_DAC_BOOT_SIZE		0x0ea0
#define	GT64260_PCI_1_SLAVE_DAC_P2P_MEM_0_SIZE		0x0ea4
#define	GT64260_PCI_1_SLAVE_DAC_P2P_MEM_1_SIZE		0x0ea8
#define	GT64260_PCI_1_SLAVE_DAC_CPU_SIZE		0x0eac

#define	GT64260_PCI_1_SLAVE_EXP_ROM_SIZE		0x0dac

#define	GT64260_PCI_1_SLAVE_BAR_REG_ENABLES		0x0cbc
#define	GT64260_PCI_1_SLAVE_SCS_0_REMAP			0x0cc8
#define	GT64260_PCI_1_SLAVE_SCS_1_REMAP			0x0dc8
#define	GT64260_PCI_1_SLAVE_SCS_2_REMAP			0x0ccc
#define	GT64260_PCI_1_SLAVE_SCS_3_REMAP			0x0dcc
#define	GT64260_PCI_1_SLAVE_CS_0_REMAP			0x0cd0
#define	GT64260_PCI_1_SLAVE_CS_1_REMAP			0x0dd0
#define	GT64260_PCI_1_SLAVE_CS_2_REMAP			0x0dd8
#define	GT64260_PCI_1_SLAVE_CS_3_REMAP			0x0cd4
#define	GT64260_PCI_1_SLAVE_BOOT_REMAP			0x0dd4
#define	GT64260_PCI_1_SLAVE_P2P_MEM_0_REMAP_LO		0x0ddc
#define	GT64260_PCI_1_SLAVE_P2P_MEM_0_REMAP_HI		0x0de0
#define	GT64260_PCI_1_SLAVE_P2P_MEM_1_REMAP_LO		0x0de4
#define	GT64260_PCI_1_SLAVE_P2P_MEM_1_REMAP_HI		0x0de8
#define	GT64260_PCI_1_SLAVE_P2P_IO_REMAP		0x0dec
#define	GT64260_PCI_1_SLAVE_CPU_REMAP			0x0df0

#define	GT64260_PCI_1_SLAVE_DAC_SCS_0_REMAP		0x0f80
#define	GT64260_PCI_1_SLAVE_DAC_SCS_1_REMAP		0x0f84
#define	GT64260_PCI_1_SLAVE_DAC_SCS_2_REMAP		0x0f88
#define	GT64260_PCI_1_SLAVE_DAC_SCS_3_REMAP		0x0f8c
#define	GT64260_PCI_1_SLAVE_DAC_CS_0_REMAP		0x0f90
#define	GT64260_PCI_1_SLAVE_DAC_CS_1_REMAP		0x0f94
#define	GT64260_PCI_1_SLAVE_DAC_CS_2_REMAP		0x0f98
#define	GT64260_PCI_1_SLAVE_DAC_CS_3_REMAP		0x0f9c
#define	GT64260_PCI_1_SLAVE_DAC_BOOT_REMAP		0x0fa0
#define	GT64260_PCI_1_SLAVE_DAC_P2P_MEM_0_REMAP_LO	0x0fa4
#define	GT64260_PCI_1_SLAVE_DAC_P2P_MEM_0_REMAP_HI	0x0fa8
#define	GT64260_PCI_1_SLAVE_DAC_P2P_MEM_1_REMAP_LO	0x0fac
#define	GT64260_PCI_1_SLAVE_DAC_P2P_MEM_1_REMAP_HI	0x0fb0
#define	GT64260_PCI_1_SLAVE_DAC_CPU_REMAP		0x0fb4

#define	GT64260_PCI_1_SLAVE_EXP_ROM_REMAP		0x0fb8
#define	GT64260_PCI_1_SLAVE_PCI_DECODE_CNTL		0x0dbc


/*
 *****************************************************************************
 *
 *	I2O Controller Interface Registers
 *
 *****************************************************************************
 */

/* FIXME: fill in */



/*
 *****************************************************************************
 *
 *	DMA Controller Interface Registers
 *
 *****************************************************************************
 */

/* FIXME: fill in */


/*
 *****************************************************************************
 *
 *	Timer/Counter Interface Registers
 *
 *****************************************************************************
 */

/* FIXME: fill in */


/*
 *****************************************************************************
 *
 *	Communications Controller (Enet, Serial, etc.) Interface Registers
 *
 *****************************************************************************
 */

#define	GT64260_ENET_0_CNTL_LO				0xf200
#define	GT64260_ENET_0_CNTL_HI				0xf204
#define	GT64260_ENET_0_RX_BUF_PCI_ADDR_HI		0xf208
#define	GT64260_ENET_0_TX_BUF_PCI_ADDR_HI		0xf20c
#define	GT64260_ENET_0_RX_DESC_ADDR_HI			0xf210
#define	GT64260_ENET_0_TX_DESC_ADDR_HI			0xf214
#define	GT64260_ENET_0_HASH_TAB_PCI_ADDR_HI		0xf218
#define	GT64260_ENET_1_CNTL_LO				0xf220
#define	GT64260_ENET_1_CNTL_HI				0xf224
#define	GT64260_ENET_1_RX_BUF_PCI_ADDR_HI		0xf228
#define	GT64260_ENET_1_TX_BUF_PCI_ADDR_HI		0xf22c
#define	GT64260_ENET_1_RX_DESC_ADDR_HI			0xf230
#define	GT64260_ENET_1_TX_DESC_ADDR_HI			0xf234
#define	GT64260_ENET_1_HASH_TAB_PCI_ADDR_HI		0xf238
#define	GT64260_ENET_2_CNTL_LO				0xf240
#define	GT64260_ENET_2_CNTL_HI				0xf244
#define	GT64260_ENET_2_RX_BUF_PCI_ADDR_HI		0xf248
#define	GT64260_ENET_2_TX_BUF_PCI_ADDR_HI		0xf24c
#define	GT64260_ENET_2_RX_DESC_ADDR_HI			0xf250
#define	GT64260_ENET_2_TX_DESC_ADDR_HI			0xf254
#define	GT64260_ENET_2_HASH_TAB_PCI_ADDR_HI		0xf258

#define	GT64260_MPSC_0_CNTL_LO				0xf280
#define	GT64260_MPSC_0_CNTL_HI				0xf284
#define	GT64260_MPSC_0_RX_BUF_PCI_ADDR_HI		0xf288
#define	GT64260_MPSC_0_TX_BUF_PCI_ADDR_HI		0xf28c
#define	GT64260_MPSC_0_RX_DESC_ADDR_HI			0xf290
#define	GT64260_MPSC_0_TX_DESC_ADDR_HI			0xf294
#define	GT64260_MPSC_1_CNTL_LO				0xf2c0
#define	GT64260_MPSC_1_CNTL_HI				0xf2c4
#define	GT64260_MPSC_1_RX_BUF_PCI_ADDR_HI		0xf2c8
#define	GT64260_MPSC_1_TX_BUF_PCI_ADDR_HI		0xf2cc
#define	GT64260_MPSC_1_RX_DESC_ADDR_HI			0xf2d0
#define	GT64260_MPSC_1_TX_DESC_ADDR_HI			0xf2d4

#define	GT64260_SER_INIT_PCI_ADDR_HI			0xf320
#define	GT64260_SER_INIT_LAST_DATA			0xf324
#define	GT64260_SER_INIT_CONTROL			0xf328
#define	GT64260_SER_INIT_STATUS				0xf32c

#define	GT64260_COMM_ARBITER_CNTL			0xf300
#define	GT64260_COMM_CONFIG				0xb40c
#define	GT64260_COMM_XBAR_TO				0xf304
#define	GT64260_COMM_INTR_CAUSE				0xf310
#define	GT64260_COMM_INTR_MASK				0xf314
#define	GT64260_COMM_ERR_ADDR				0xf318


/*
 *****************************************************************************
 *
 *	Fast Ethernet Controller Interface Registers
 *
 *****************************************************************************
 */

#define	GT64260_ENET_PHY_ADDR				0x2000
#define	GT64260_ENET_ESMIR				0x2010

#define	GT64260_ENET_E0PCR				0x2400
#define	GT64260_ENET_E0PCXR				0x2408
#define	GT64260_ENET_E0PCMR				0x2410
#define	GT64260_ENET_E0PSR				0x2418
#define	GT64260_ENET_E0SPR				0x2420
#define	GT64260_ENET_E0HTPR				0x2428
#define	GT64260_ENET_E0FCSAL				0x2430
#define	GT64260_ENET_E0FCSAH				0x2438
#define	GT64260_ENET_E0SDCR				0x2440
#define	GT64260_ENET_E0SDCMR				0x2448
#define	GT64260_ENET_E0ICR				0x2450
#define	GT64260_ENET_E0IMR				0x2458
#define	GT64260_ENET_E0FRDP0				0x2480
#define	GT64260_ENET_E0FRDP1				0x2484
#define	GT64260_ENET_E0FRDP2				0x2488
#define	GT64260_ENET_E0FRDP3				0x248c
#define	GT64260_ENET_E0CRDP0				0x24a0
#define	GT64260_ENET_E0CRDP1				0x24a4
#define	GT64260_ENET_E0CRDP2				0x24a8
#define	GT64260_ENET_E0CRDP3				0x24ac
#define	GT64260_ENET_E0CTDP0				0x24e0
#define	GT64260_ENET_E0CTDP1				0x24e4
#define	GT64260_ENET_0_DSCP2P0L				0x2460
#define	GT64260_ENET_0_DSCP2P0H				0x2464
#define	GT64260_ENET_0_DSCP2P1L				0x2468
#define	GT64260_ENET_0_DSCP2P1H				0x246c
#define	GT64260_ENET_0_VPT2P				0x2470
#define	GT64260_ENET_0_MIB_CTRS				0x2500

#define	GT64260_ENET_E1PCR				0x2800
#define	GT64260_ENET_E1PCXR				0x2808
#define	GT64260_ENET_E1PCMR				0x2810
#define	GT64260_ENET_E1PSR				0x2818
#define	GT64260_ENET_E1SPR				0x2820
#define	GT64260_ENET_E1HTPR				0x2828
#define	GT64260_ENET_E1FCSAL				0x2830
#define	GT64260_ENET_E1FCSAH				0x2838
#define	GT64260_ENET_E1SDCR				0x2840
#define	GT64260_ENET_E1SDCMR				0x2848
#define	GT64260_ENET_E1ICR				0x2850
#define	GT64260_ENET_E1IMR				0x2858
#define	GT64260_ENET_E1FRDP0				0x2880
#define	GT64260_ENET_E1FRDP1				0x2884
#define	GT64260_ENET_E1FRDP2				0x2888
#define	GT64260_ENET_E1FRDP3				0x288c
#define	GT64260_ENET_E1CRDP0				0x28a0
#define	GT64260_ENET_E1CRDP1				0x28a4
#define	GT64260_ENET_E1CRDP2				0x28a8
#define	GT64260_ENET_E1CRDP3				0x28ac
#define	GT64260_ENET_E1CTDP0				0x28e0
#define	GT64260_ENET_E1CTDP1				0x28e4
#define	GT64260_ENET_1_DSCP2P0L				0x2860
#define	GT64260_ENET_1_DSCP2P0H				0x2864
#define	GT64260_ENET_1_DSCP2P1L				0x2868
#define	GT64260_ENET_1_DSCP2P1H				0x286c
#define	GT64260_ENET_1_VPT2P				0x2870
#define	GT64260_ENET_1_MIB_CTRS				0x2900

#define	GT64260_ENET_E2PCR				0x2c00
#define	GT64260_ENET_E2PCXR				0x2c08
#define	GT64260_ENET_E2PCMR				0x2c10
#define	GT64260_ENET_E2PSR				0x2c18
#define	GT64260_ENET_E2SPR				0x2c20
#define	GT64260_ENET_E2HTPR				0x2c28
#define	GT64260_ENET_E2FCSAL				0x2c30
#define	GT64260_ENET_E2FCSAH				0x2c38
#define	GT64260_ENET_E2SDCR				0x2c40
#define	GT64260_ENET_E2SDCMR				0x2c48
#define	GT64260_ENET_E2ICR				0x2c50
#define	GT64260_ENET_E2IMR				0x2c58
#define	GT64260_ENET_E2FRDP0				0x2c80
#define	GT64260_ENET_E2FRDP1				0x2c84
#define	GT64260_ENET_E2FRDP2				0x2c88
#define	GT64260_ENET_E2FRDP3				0x2c8c
#define	GT64260_ENET_E2CRDP0				0x2ca0
#define	GT64260_ENET_E2CRDP1				0x2ca4
#define	GT64260_ENET_E2CRDP2				0x2ca8
#define	GT64260_ENET_E2CRDP3				0x2cac
#define	GT64260_ENET_E2CTDP0				0x2ce0
#define	GT64260_ENET_E2CTDP1				0x2ce4
#define	GT64260_ENET_2_DSCP2P0L				0x2c60
#define	GT64260_ENET_2_DSCP2P0H				0x2c64
#define	GT64260_ENET_2_DSCP2P1L				0x2c68
#define	GT64260_ENET_2_DSCP2P1H				0x2c6c
#define	GT64260_ENET_2_VPT2P				0x2c70
#define	GT64260_ENET_2_MIB_CTRS				0x2d00


/*
 *****************************************************************************
 *
 *	Multi-Protocol Serial Controller Interface Registers
 *
 *****************************************************************************
 */

/* Signal Routing */
#define	GT64260_MPSC_MRR				0xb400
#define	GT64260_MPSC_RCRR				0xb404
#define	GT64260_MPSC_TCRR				0xb408

/* Main Configuratino Registers */
#define	GT64260_MPSC_0_MMCRL				0x8000
#define	GT64260_MPSC_0_MMCRH				0x8004
#define	GT64260_MPSC_0_MPCR				0x8008
#define	GT64260_MPSC_0_CHR_1				0x800c
#define	GT64260_MPSC_0_CHR_2				0x8010
#define	GT64260_MPSC_0_CHR_3				0x8014
#define	GT64260_MPSC_0_CHR_4				0x8018
#define	GT64260_MPSC_0_CHR_5				0x801c
#define	GT64260_MPSC_0_CHR_6				0x8020
#define	GT64260_MPSC_0_CHR_7				0x8024
#define	GT64260_MPSC_0_CHR_8				0x8028
#define	GT64260_MPSC_0_CHR_9				0x802c
#define	GT64260_MPSC_0_CHR_10				0x8030
#define	GT64260_MPSC_0_CHR_11				0x8034

#define	GT64260_MPSC_1_MMCRL				0x9000
#define	GT64260_MPSC_1_MMCRH				0x9004
#define	GT64260_MPSC_1_MPCR				0x9008
#define	GT64260_MPSC_1_CHR_1				0x900c
#define	GT64260_MPSC_1_CHR_2				0x9010
#define	GT64260_MPSC_1_CHR_3				0x9014
#define	GT64260_MPSC_1_CHR_4				0x9018
#define	GT64260_MPSC_1_CHR_5				0x901c
#define	GT64260_MPSC_1_CHR_6				0x9020
#define	GT64260_MPSC_1_CHR_7				0x9024
#define	GT64260_MPSC_1_CHR_8				0x9028
#define	GT64260_MPSC_1_CHR_9				0x902c
#define	GT64260_MPSC_1_CHR_10				0x9030
#define	GT64260_MPSC_1_CHR_11				0x9034

#define	GT64260_MPSC_0_INTR_CAUSE			0xb804
#define	GT64260_MPSC_0_INTR_MASK			0xb884
#define	GT64260_MPSC_1_INTR_CAUSE			0xb80c
#define	GT64260_MPSC_1_INTR_MASK			0xb88c

#define	GT64260_MPSC_UART_CR_TEV			(1<<1)
#define	GT64260_MPSC_UART_CR_TA				(1<<7)
#define	GT64260_MPSC_UART_CR_TTCS			(1<<9)
#define	GT64260_MPSC_UART_CR_REV			(1<<17)
#define	GT64260_MPSC_UART_CR_RA				(1<<23)
#define	GT64260_MPSC_UART_CR_CRD			(1<<25)
#define	GT64260_MPSC_UART_CR_EH				(1<<31)

#define	GT64260_MPSC_UART_ESR_CTS			(1<<0)
#define	GT64260_MPSC_UART_ESR_CD			(1<<1)
#define	GT64260_MPSC_UART_ESR_TIDLE			(1<<3)
#define	GT64260_MPSC_UART_ESR_RHS			(1<<5)
#define	GT64260_MPSC_UART_ESR_RLS			(1<<7)
#define	GT64260_MPSC_UART_ESR_RLIDL			(1<<11)


/*
 *****************************************************************************
 *
 *	Serial DMA Controller Interface Registers
 *
 *****************************************************************************
 */

#define	GT64260_SDMA_0_SDC				0x4000
#define	GT64260_SDMA_0_SDCM				0x4008
#define	GT64260_SDMA_0_RX_DESC				0x4800
#define	GT64260_SDMA_0_RX_BUF_PTR			0x4808
#define	GT64260_SDMA_0_SCRDP				0x4810
#define	GT64260_SDMA_0_TX_DESC				0x4c00
#define	GT64260_SDMA_0_SCTDP				0x4c10
#define	GT64260_SDMA_0_SFTDP				0x4c14

#define	GT64260_SDMA_1_SDC				0x6000
#define	GT64260_SDMA_1_SDCM				0x6008
#define	GT64260_SDMA_1_RX_DESC				0x6800
#define GT64260_SDMA_1_RX_BUF_PTR                       0x6808
#define	GT64260_SDMA_1_SCRDP				0x6810
#define	GT64260_SDMA_1_TX_DESC				0x6c00
#define	GT64260_SDMA_1_SCTDP				0x6c10
#define	GT64260_SDMA_1_SFTDP				0x6c14

#define	GT64260_SDMA_INTR_CAUSE				0xb800
#define	GT64260_SDMA_INTR_MASK				0xb880

#define	GT64260_SDMA_DESC_CMDSTAT_PE			(1<<0)
#define	GT64260_SDMA_DESC_CMDSTAT_CDL			(1<<1)
#define	GT64260_SDMA_DESC_CMDSTAT_FR			(1<<3)
#define	GT64260_SDMA_DESC_CMDSTAT_OR			(1<<6)
#define	GT64260_SDMA_DESC_CMDSTAT_BR			(1<<9)
#define	GT64260_SDMA_DESC_CMDSTAT_MI			(1<<10)
#define	GT64260_SDMA_DESC_CMDSTAT_A			(1<<11)
#define	GT64260_SDMA_DESC_CMDSTAT_AM			(1<<12)
#define	GT64260_SDMA_DESC_CMDSTAT_CT			(1<<13)
#define	GT64260_SDMA_DESC_CMDSTAT_C			(1<<14)
#define	GT64260_SDMA_DESC_CMDSTAT_ES			(1<<15)
#define	GT64260_SDMA_DESC_CMDSTAT_L			(1<<16)
#define	GT64260_SDMA_DESC_CMDSTAT_F			(1<<17)
#define	GT64260_SDMA_DESC_CMDSTAT_P			(1<<18)
#define	GT64260_SDMA_DESC_CMDSTAT_EI			(1<<23)
#define	GT64260_SDMA_DESC_CMDSTAT_O			(1<<31)

#define	GT64260_SDMA_SDC_RFT				(1<<0)
#define	GT64260_SDMA_SDC_SFM				(1<<1)
#define	GT64260_SDMA_SDC_BLMR				(1<<6)
#define	GT64260_SDMA_SDC_BLMT				(1<<7)
#define	GT64260_SDMA_SDC_POVR				(1<<8)
#define	GT64260_SDMA_SDC_RIFB				(1<<9)

#define	GT64260_SDMA_SDCM_ERD				(1<<7)
#define	GT64260_SDMA_SDCM_AR				(1<<15)
#define	GT64260_SDMA_SDCM_STD				(1<<16)
#define	GT64260_SDMA_SDCM_TXD				(1<<23)
#define	GT64260_SDMA_SDCM_AT				(1<<31)

#define	GT64260_SDMA_0_CAUSE_RXBUF			(1<<0)
#define	GT64260_SDMA_0_CAUSE_RXERR			(1<<1)
#define	GT64260_SDMA_0_CAUSE_TXBUF			(1<<2)
#define	GT64260_SDMA_0_CAUSE_TXEND			(1<<3)
#define	GT64260_SDMA_1_CAUSE_RXBUF			(1<<8)
#define	GT64260_SDMA_1_CAUSE_RXERR			(1<<9)
#define	GT64260_SDMA_1_CAUSE_TXBUF			(1<<10)
#define	GT64260_SDMA_1_CAUSE_TXEND			(1<<11)


/*
 *****************************************************************************
 *
 *	Baud Rate Generator Interface Registers
 *
 *****************************************************************************
 */

#define	GT64260_BRG_0_BCR				0xb200
#define	GT64260_BRG_0_BTR				0xb204
#define	GT64260_BRG_1_BCR				0xb208
#define	GT64260_BRG_1_BTR				0xb20c
#define	GT64260_BRG_2_BCR				0xb210
#define	GT64260_BRG_2_BTR				0xb214

#define	GT64260_BRG_INTR_CAUSE				0xb834
#define	GT64260_BRG_INTR_MASK				0xb8b4


/*
 *****************************************************************************
 *
 *	Watchdog Timer Interface Registers
 *
 *****************************************************************************
 */

#define	GT64260_WDT_WDC					0xb410
#define	GT64260_WDT_WDV					0xb414


/*
 *****************************************************************************
 *
 *	 General Purpose Pins Controller Interface Registers
 *
 *****************************************************************************
 */

#define	GT64260_GPP_IO_CNTL				0xf100
#define	GT64260_GPP_LEVEL_CNTL				0xf110
#define	GT64260_GPP_VALUE				0xf104
#define	GT64260_GPP_INTR_CAUSE				0xf108
#define	GT64260_GPP_INTR_MASK				0xf10c


/*
 *****************************************************************************
 *
 *	Multi-Purpose Pins Controller Interface Registers
 *
 *****************************************************************************
 */

#define	GT64260_MPP_CNTL_0				0xf000
#define	GT64260_MPP_CNTL_1				0xf004
#define	GT64260_MPP_CNTL_2				0xf008
#define	GT64260_MPP_CNTL_3				0xf00c
#define	GT64260_MPP_SERIAL_PORTS_MULTIPLEX		0xf010


/*
 *****************************************************************************
 *
 *	I2C Controller Interface Registers
 *
 *****************************************************************************
 */

/* FIXME: fill in */


/*
 *****************************************************************************
 *
 *	Interrupt Controller Interface Registers
 *
 *****************************************************************************
 */

#define	GT64260_IC_MAIN_CAUSE_LO			0x0c18
#define	GT64260_IC_MAIN_CAUSE_HI			0x0c68
#define	GT64260_IC_CPU_INTR_MASK_LO			0x0c1c
#define	GT64260_IC_CPU_INTR_MASK_HI			0x0c6c
#define	GT64260_IC_CPU_SELECT_CAUSE			0x0c70
#define	GT64260_IC_PCI_0_INTR_MASK_LO			0x0c24
#define	GT64260_IC_PCI_0_INTR_MASK_HI			0x0c64
#define	GT64260_IC_PCI_0_SELECT_CAUSE			0x0c74
#define	GT64260_IC_PCI_1_INTR_MASK_LO			0x0ca4
#define	GT64260_IC_PCI_1_INTR_MASK_HI			0x0ce4
#define	GT64260_IC_PCI_1_SELECT_CAUSE			0x0cf4
#define	GT64260_IC_CPU_INT_0_MASK			0x0e60
#define	GT64260_IC_CPU_INT_1_MASK			0x0e64
#define	GT64260_IC_CPU_INT_2_MASK			0x0e68
#define	GT64260_IC_CPU_INT_3_MASK			0x0e6c


#endif /* __ASMPPC_GT64260_DEFS_H */
