/*
 * (C) Copyright 2010 Quantenna Communications Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * Header file which describes Ruby platform.
 * Has to be used by both kernel and bootloader.
 */

#ifndef __RUBY_MEM_H
#define __RUBY_MEM_H

#include "ruby_config.h"

/* Platform memory */
/* SRAM */
#define RUBY_SRAM_UNIFIED_BEGIN			0x98000000
#define RUBY_SRAM_UNIFIED_NOCACHE_BEGIN		0xf8000000
#define RUBY_SRAM_FLIP_BEGIN			0x88000000
#define RUBY_SRAM_FLIP_NOCACHE_BEGIN		0x60000000
#define RUBY_SRAM_NOFLIP_BEGIN			0x80000000
#define RUBY_SRAM_NOFLIP_NOCACHE_BEGIN		0x60000000
#define RUBY_SRAM_BANK_SIZE			(64 * 1024)
#ifdef TOPAZ_PLATFORM
	#define RUBY_SRAM_SIZE			(8 * RUBY_SRAM_BANK_SIZE)
	#define RUBY_SRAM_BANK_SAFE_SIZE	RUBY_SRAM_BANK_SIZE
#else
	#define RUBY_SRAM_END_BANK_GUARD_SIZE	32
	#define RUBY_SRAM_SIZE			(4 * RUBY_SRAM_BANK_SIZE)
	#define RUBY_SRAM_BANK_SAFE_SIZE	(RUBY_SRAM_BANK_SIZE - RUBY_SRAM_END_BANK_GUARD_SIZE)
#endif
/* DDR */
#define RUBY_DRAM_UNIFIED_BEGIN			0x80000000
#define RUBY_DRAM_UNIFIED_NOCACHE_BEGIN		0xd0000000
#define RUBY_DRAM_FLIP_BEGIN			0x80000000
#define RUBY_DRAM_FLIP_NOCACHE_BEGIN		0x40000000
#define RUBY_DRAM_NOFLIP_BEGIN			0x0
#define RUBY_DRAM_NOFLIP_NOCACHE_BEGIN		0x40000000
#define RUBY_MAX_DRAM_SIZE			DDR_128MB
#define RUBY_MIN_DRAM_SIZE			DDR_64MB
/*****************************************************************************/
/* SPI memory mapped                                                         */
/*****************************************************************************/
#define RUBY_SPI_FLASH_ADDR     0x90000000
/* NEVTBD - put in real XYMEM values */
#define RUBY_DSP_XYMEM_BEGIN			0xD0000000
#define RUBY_DSP_XYMEM_END			0xDFFFFFFF
#if TOPAZ_MMAP_UNIFIED
	#define RUBY_SRAM_BEGIN			RUBY_SRAM_UNIFIED_BEGIN
	#define RUBY_SRAM_BUS_BEGIN		RUBY_SRAM_UNIFIED_BEGIN
	#define RUBY_SRAM_NOCACHE_BEGIN		RUBY_SRAM_UNIFIED_NOCACHE_BEGIN
	#define RUBY_DRAM_BEGIN			RUBY_DRAM_UNIFIED_BEGIN
	#define RUBY_DRAM_BUS_BEGIN		RUBY_DRAM_UNIFIED_BEGIN
	#define RUBY_DRAM_NOCACHE_BEGIN		RUBY_DRAM_UNIFIED_NOCACHE_BEGIN
#elif RUBY_MMAP_FLIP
	#define RUBY_SRAM_BEGIN			RUBY_SRAM_FLIP_BEGIN
	#define RUBY_SRAM_BUS_BEGIN		RUBY_SRAM_NOFLIP_BEGIN
	#define RUBY_SRAM_NOCACHE_BEGIN		RUBY_SRAM_FLIP_NOCACHE_BEGIN
	#define RUBY_DRAM_BEGIN			RUBY_DRAM_FLIP_BEGIN
	#define RUBY_DRAM_BUS_BEGIN		RUBY_DRAM_NOFLIP_BEGIN
	#define RUBY_DRAM_NOCACHE_BEGIN		RUBY_DRAM_FLIP_NOCACHE_BEGIN
#else
	#define RUBY_SRAM_BEGIN			RUBY_SRAM_NOFLIP_BEGIN
	#define RUBY_SRAM_BUS_BEGIN		RUBY_SRAM_NOFLIP_BEGIN
	#define RUBY_SRAM_NOCACHE_BEGIN		RUBY_SRAM_NOFLIP_NOCACHE_BEGIN
	#define RUBY_DRAM_BEGIN			RUBY_DRAM_NOFLIP_BEGIN
	#define RUBY_DRAM_BUS_BEGIN		RUBY_DRAM_NOFLIP_BEGIN
	#define RUBY_DRAM_NOCACHE_BEGIN		RUBY_DRAM_NOFLIP_NOCACHE_BEGIN
#endif

#define RUBY_KERNEL_LOAD_DRAM_BEGIN		(RUBY_DRAM_BEGIN + 0x3000000)

/* Hardware */
#define RUBY_HARDWARE_BEGIN	0xC0000000

#define	ROUNDUP(x, y)			((((x)+((y)-1))/(y))*(y))

/* DDR layout  */
#define CONFIG_ARC_NULL_BASE		0x00000000
#define CONFIG_ARC_NULL_SIZE		(64 * 1024)
#define CONFIG_ARC_NULL_END		(CONFIG_ARC_NULL_BASE + CONFIG_ARC_NULL_SIZE)
#define CONFIG_ARC_PCIE_RSVD_SIZE	(64 * 1024)
#define CONFIG_ARC_DSP_BASE		(CONFIG_ARC_NULL_END + CONFIG_ARC_PCIE_RSVD_SIZE)
#define CONFIG_ARC_DSP_SIZE		(768 * 1024)
#define CONFIG_ARC_DSP_END		(CONFIG_ARC_DSP_BASE + CONFIG_ARC_DSP_SIZE)
#define CONFIG_ARC_MUC_BASE		CONFIG_ARC_DSP_END
#ifdef TOPAZ_PLATFORM
#ifdef TOPAZ_128_NODE_MODE
#define CONFIG_ARC_MUC_SIZE		((2 * 1024 * 1024) + (368 * 1024))
#else
#define CONFIG_ARC_MUC_SIZE		((1 * 1024 * 1024) + (768 * 1024))
#endif
#else
#define CONFIG_ARC_MUC_SIZE             ((1 * 1024 * 1024) + (640 * 1024))
#endif
#define CONFIG_ARC_MUC_END		(CONFIG_ARC_MUC_BASE + CONFIG_ARC_MUC_SIZE)
#define CONFIG_ARC_MUC_MAPPED_BASE	CONFIG_ARC_MUC_BASE
#define CONFIG_ARC_MUC_MAPPED_SIZE	(RUBY_MAX_DRAM_SIZE - CONFIG_ARC_MUC_MAPPED_BASE)
#ifdef TOPAZ_PLATFORM
	#define CONFIG_ARC_AUC_BASE		CONFIG_ARC_MUC_END
	#define CONFIG_ARC_AUC_SIZE		(1024 * 1024 + 768 * 1024)
	#define CONFIG_ARC_AUC_END		(CONFIG_ARC_AUC_BASE + CONFIG_ARC_AUC_SIZE)

	#define TOPAZ_HBM_BUF_ALIGN		(1 * 1024)
	#define TOPAZ_HBM_BUF_EMAC_RX_SIZE	(4 * 1024)
	#define TOPAZ_HBM_BUF_WMAC_RX_SIZE	(17 * 1024)
#ifdef QTN_RC_ENABLE_HDP
	#define TOPAZ_HBM_BUF_EMAC_RX_COUNT_S	(14)
	#define TOPAZ_HBM_BUF_WMAC_RX_COUNT_S	(0)
#else
	#define TOPAZ_HBM_BUF_EMAC_RX_COUNT_S	(13)
	#define TOPAZ_HBM_BUF_WMAC_RX_COUNT_S	(11)
#endif
	#define TOPAZ_HBM_EMAC_TX_DONE_COUNT_S	(12)

	#define TOPAZ_HBM_BUF_EMAC_RX_POOL	0
	#define TOPAZ_HBM_BUF_WMAC_RX_POOL	1
	#define TOPAZ_HBM_AUC_FEEDBACK_POOL	2
	#define TOPAZ_HBM_EMAC_TX_DONE_POOL	3

	#define TOPAZ_HBM_BUF_META_SIZE		64		/* keep it 2^n */
	#define TOPAZ_HBM_POOL_GUARD_SIZE	(64 * 1024)

	#define TOPAZ_HBM_BUF_EMAC_RX_COUNT	(1 << TOPAZ_HBM_BUF_EMAC_RX_COUNT_S)
	#define TOPAZ_HBM_BUF_EMAC_RX_TOTAL	(TOPAZ_HBM_BUF_EMAC_RX_COUNT *	\
							TOPAZ_HBM_BUF_EMAC_RX_SIZE)
	#define TOPAZ_HBM_BUF_WMAC_RX_COUNT	(1 << TOPAZ_HBM_BUF_WMAC_RX_COUNT_S)
	#define TOPAZ_HBM_BUF_WMAC_RX_TOTAL	(TOPAZ_HBM_BUF_WMAC_RX_COUNT *	\
							TOPAZ_HBM_BUF_WMAC_RX_SIZE)
	#define TOPAZ_HBM_EMAC_TX_DONE_COUNT	(1 << TOPAZ_HBM_EMAC_TX_DONE_COUNT_S)

	#define TOPAZ_HBM_BUF_META_BASE		CONFIG_ARC_AUC_END

	#define TOPAZ_HBM_BUF_META_EMAC_RX_BASE		(TOPAZ_HBM_BUF_META_BASE + TOPAZ_HBM_BUF_META_SIZE)
	#define TOPAZ_HBM_BUF_META_EMAC_RX_BASE_VIRT	(RUBY_DRAM_BEGIN + TOPAZ_HBM_BUF_META_EMAC_RX_BASE)
	#define TOPAZ_HBM_BUF_META_EMAC_RX_TOTAL	(TOPAZ_HBM_BUF_EMAC_RX_COUNT * \
								TOPAZ_HBM_BUF_META_SIZE)
	#define TOPAZ_HBM_BUF_META_EMAC_RX_END		(TOPAZ_HBM_BUF_META_EMAC_RX_BASE + \
								TOPAZ_HBM_BUF_META_EMAC_RX_TOTAL)

	#define TOPAZ_HBM_BUF_META_WMAC_RX_BASE		(TOPAZ_HBM_BUF_META_EMAC_RX_END + TOPAZ_HBM_BUF_META_SIZE)
	#define TOPAZ_HBM_BUF_META_WMAC_RX_BASE_VIRT	(RUBY_DRAM_BEGIN + TOPAZ_HBM_BUF_META_WMAC_RX_BASE)
	#define TOPAZ_HBM_BUF_META_WMAC_RX_TOTAL	(TOPAZ_HBM_BUF_WMAC_RX_COUNT * \
								TOPAZ_HBM_BUF_META_SIZE)
	#define TOPAZ_HBM_BUF_META_WMAC_RX_END		(TOPAZ_HBM_BUF_META_WMAC_RX_BASE + \
								TOPAZ_HBM_BUF_META_WMAC_RX_TOTAL)

	#define TOPAZ_HBM_BUF_META_END		(TOPAZ_HBM_BUF_META_WMAC_RX_END + TOPAZ_HBM_BUF_META_SIZE)
	#define TOPAZ_HBM_BUF_META_TOTAL	(TOPAZ_HBM_BUF_META_END - TOPAZ_HBM_BUF_META_BASE)

	#define TOPAZ_HBM_BUF_BASE		ROUNDUP(TOPAZ_HBM_BUF_META_END, TOPAZ_HBM_BUF_ALIGN)

	#define TOPAZ_HBM_BUF_EMAC_RX_BASE	(TOPAZ_HBM_BUF_BASE + TOPAZ_HBM_POOL_GUARD_SIZE)
	#define TOPAZ_HBM_BUF_EMAC_RX_BASE_VIRT	(RUBY_DRAM_BEGIN + TOPAZ_HBM_BUF_EMAC_RX_BASE)
	#define TOPAZ_HBM_BUF_EMAC_RX_END	(TOPAZ_HBM_BUF_EMAC_RX_BASE +	\
							TOPAZ_HBM_BUF_EMAC_RX_TOTAL)

	#define TOPAZ_HBM_BUF_WMAC_RX_BASE	(TOPAZ_HBM_BUF_EMAC_RX_END + TOPAZ_HBM_POOL_GUARD_SIZE)
	#define TOPAZ_HBM_BUF_WMAC_RX_BASE_VIRT	(RUBY_DRAM_BEGIN + TOPAZ_HBM_BUF_WMAC_RX_BASE)
	#define TOPAZ_HBM_BUF_WMAC_RX_END	(TOPAZ_HBM_BUF_WMAC_RX_BASE +	\
							TOPAZ_HBM_BUF_WMAC_RX_TOTAL)

	#define TOPAZ_HBM_BUF_END		(TOPAZ_HBM_BUF_WMAC_RX_END + TOPAZ_HBM_POOL_GUARD_SIZE)

	#define TOPAZ_FWT_MCAST_ENTRIES		2048
	#define TOPAZ_FWT_MCAST_FF_ENTRIES	8	/* one per vap */
	#define TOPAZ_FWT_MCAST_IPMAP_ENT_SIZE	64	/* sizeof(struct topaz_fwt_sw_ipmap) */
	#define TOPAZ_FWT_MCAST_TQE_ENT_SIZE	20	/* sizeof(struct topaz_fwt_sw_mcast_entry) */
	#define TOPAZ_FWT_MCAST_IPMAP_SIZE	(TOPAZ_FWT_MCAST_ENTRIES * TOPAZ_FWT_MCAST_IPMAP_ENT_SIZE)
	#define TOPAZ_FWT_MCAST_TQE_SIZE	(TOPAZ_FWT_MCAST_ENTRIES * TOPAZ_FWT_MCAST_TQE_ENT_SIZE)
	#define TOPAZ_FWT_MCAST_TQE_FF_SIZE	(TOPAZ_FWT_MCAST_FF_ENTRIES * TOPAZ_FWT_MCAST_TQE_ENT_SIZE)

	#define TOPAZ_FWT_MCAST_IPMAP_BASE	TOPAZ_HBM_BUF_END
	#define TOPAZ_FWT_MCAST_IPMAP_END	(TOPAZ_FWT_MCAST_IPMAP_BASE + TOPAZ_FWT_MCAST_IPMAP_SIZE)
	#define TOPAZ_FWT_MCAST_TQE_BASE	TOPAZ_FWT_MCAST_IPMAP_END
	#define TOPAZ_FWT_MCAST_TQE_END		(TOPAZ_FWT_MCAST_TQE_BASE + TOPAZ_FWT_MCAST_TQE_SIZE)
	#define TOPAZ_FWT_MCAST_TQE_FF_BASE	TOPAZ_FWT_MCAST_TQE_END
	#define TOPAZ_FWT_MCAST_TQE_FF_END	(TOPAZ_FWT_MCAST_TQE_FF_BASE + TOPAZ_FWT_MCAST_TQE_FF_SIZE)
	#define TOPAZ_FWT_MCAST_END		TOPAZ_FWT_MCAST_TQE_FF_END

	/* Offset from DDR beginning, from which memory start to belong to Linux */
	#define CONFIG_ARC_KERNEL_MEM_BASE	TOPAZ_FWT_MCAST_END

	#if TOPAZ_HBM_BUF_EMAC_RX_BASE & (TOPAZ_HBM_BUF_ALIGN - 1)
		#error EMAC Buffer start not aligned
	#endif
	#if TOPAZ_HBM_BUF_WMAC_RX_BASE & (TOPAZ_HBM_BUF_ALIGN - 1)
		#error WMAC Buffer start not aligned
	#endif
#else
	/* Offset from DDR beginning, from which memory start to belong to Linux */
	#define CONFIG_ARC_KERNEL_MEM_BASE	CONFIG_ARC_MUC_END
#endif
#define CONFIG_ARC_UBOOT_RESERVED_SPACE	(8 * 1024)
#define CONFIG_ARC_KERNEL_PAGE_SIZE	(8 * 1024)
/* Linux kernel u-boot image start address, for uncompressed images */
#define CONFIG_ARC_KERNEL_BOOT_BASE	ROUNDUP(CONFIG_ARC_KERNEL_MEM_BASE, \
						CONFIG_ARC_KERNEL_PAGE_SIZE)
/* Linux kernel image start */
#define CONFIG_ARC_KERNEL_BASE		(CONFIG_ARC_KERNEL_BOOT_BASE + CONFIG_ARC_UBOOT_RESERVED_SPACE)
#define CONFIG_ARC_KERNEL_MAX_SIZE	(RUBY_MAX_DRAM_SIZE - CONFIG_ARC_KERNEL_MEM_BASE)
#define CONFIG_ARC_KERNEL_MIN_SIZE	(RUBY_MIN_DRAM_SIZE - CONFIG_ARC_KERNEL_MEM_BASE)

/* Config space */
#define CONFIG_ARC_CONF_SIZE		(8 * 1024)
#define CONFIG_ARC_CONF_BASE		(0x80000000 + CONFIG_ARC_CONF_SIZE)	/* Config area for Universal H/W ID */

/* SRAM layout */
#define RUBY_CRUMBS_SIZE			64	/* bytes at the very end of sram for crash tracing */
/*-*/
#ifdef TOPAZ_PLATFORM
/* dedicated SRAM space for HBM pointer pools */
# define TOPAZ_HBM_POOL_PTR_SIZE		4	/* sizeof(void *), 32 bit arch */
# define TOPAZ_HBM_POOL_EMAC_RX_START		0x00000000
# define TOPAZ_HBM_POOL_EMAC_RX_SIZE		(TOPAZ_HBM_BUF_EMAC_RX_COUNT * TOPAZ_HBM_POOL_PTR_SIZE)
# define TOPAZ_HBM_POOL_EMAC_RX_END		(TOPAZ_HBM_POOL_EMAC_RX_START + TOPAZ_HBM_POOL_EMAC_RX_SIZE)
# define TOPAZ_HBM_POOL_WMAC_RX_START		TOPAZ_HBM_POOL_EMAC_RX_END
# define TOPAZ_HBM_POOL_WMAC_RX_SIZE		(TOPAZ_HBM_BUF_WMAC_RX_COUNT * TOPAZ_HBM_POOL_PTR_SIZE)
# define TOPAZ_HBM_POOL_WMAC_RX_END		(TOPAZ_HBM_POOL_WMAC_RX_START + TOPAZ_HBM_POOL_WMAC_RX_SIZE)
# define TOPAZ_HBM_POOL_EMAC_TX_DONE_START	TOPAZ_HBM_POOL_WMAC_RX_END
# define TOPAZ_HBM_POOL_EMAC_TX_DONE_SIZE	(TOPAZ_HBM_EMAC_TX_DONE_COUNT * TOPAZ_HBM_POOL_PTR_SIZE)
# define TOPAZ_HBM_POOL_EMAC_TX_DONE_END	(TOPAZ_HBM_POOL_EMAC_TX_DONE_START + TOPAZ_HBM_POOL_EMAC_TX_DONE_SIZE)
# define TOPAZ_FWT_SW_START			TOPAZ_HBM_POOL_EMAC_TX_DONE_END
# define TOPAZ_FWT_SW_SIZE			(4096)
# define TOPAZ_FWT_SW_END			(TOPAZ_FWT_SW_START + TOPAZ_FWT_SW_SIZE)
# define CONFIG_ARC_KERNEL_SRAM_B1_BASE		ROUNDUP(TOPAZ_FWT_SW_END, CONFIG_ARC_KERNEL_PAGE_SIZE)
# define CONFIG_ARC_KERNEL_SRAM_B1_SIZE		(30 * 1024)
# define CONFIG_ARC_KERNEL_SRAM_B1_END		(CONFIG_ARC_KERNEL_SRAM_B1_BASE + CONFIG_ARC_KERNEL_SRAM_B1_SIZE)
# define CONFIG_ARC_KERNEL_SRAM_B2_BASE		CONFIG_ARC_KERNEL_SRAM_B1_END
# define CONFIG_ARC_KERNEL_SRAM_B2_END		(ROUNDUP(CONFIG_ARC_KERNEL_SRAM_B2_BASE, RUBY_SRAM_BANK_SIZE) -	\
						 ROUNDUP(TOPAZ_VNET_WR_STAGING_RESERVE, CONFIG_ARC_KERNEL_PAGE_SIZE))
# define CONFIG_ARC_KERNEL_SRAM_B2_SIZE		(CONFIG_ARC_KERNEL_SRAM_B2_END - CONFIG_ARC_KERNEL_SRAM_B2_BASE)
# define TOPAZ_VNET_WR_STAGING_1_START		CONFIG_ARC_KERNEL_SRAM_B2_END
# define TOPAZ_VNET_WR_STAGING_1_SIZE		TOPAZ_VNET_WR_STAGING_RESERVE
# define TOPAZ_VNET_WR_STAGING_1_END		(TOPAZ_VNET_WR_STAGING_1_START + TOPAZ_VNET_WR_STAGING_1_SIZE)
# define TOPAZ_VNET_WR_STAGING_2_START		ROUNDUP(TOPAZ_VNET_WR_STAGING_1_END, RUBY_SRAM_BANK_SIZE)
# define TOPAZ_VNET_WR_STAGING_2_SIZE		TOPAZ_VNET_WR_STAGING_RESERVE
# define TOPAZ_VNET_WR_STAGING_2_END		(TOPAZ_VNET_WR_STAGING_2_START + TOPAZ_VNET_WR_STAGING_2_SIZE)
# if TOPAZ_VNET_WR_STAGING_2_START != (2 * RUBY_SRAM_BANK_SIZE)
#  error SRAM linkmap error forming topaz sram wr staging
# endif
# define CONFIG_ARC_MUC_SRAM_B1_BASE		ROUNDUP(TOPAZ_VNET_WR_STAGING_2_END, CONFIG_ARC_KERNEL_PAGE_SIZE)
# define CONFIG_ARC_MUC_SRAM_B1_END		ROUNDUP(CONFIG_ARC_MUC_SRAM_B1_BASE + 1, RUBY_SRAM_BANK_SIZE)
# define CONFIG_ARC_MUC_SRAM_B1_SIZE		(CONFIG_ARC_MUC_SRAM_B1_END - CONFIG_ARC_MUC_SRAM_B1_BASE)
# define CONFIG_ARC_MUC_SRAM_B2_BASE		ROUNDUP(CONFIG_ARC_MUC_SRAM_B1_END, RUBY_SRAM_BANK_SIZE)
# define CONFIG_ARC_MUC_SRAM_B2_SIZE		(RUBY_SRAM_BANK_SAFE_SIZE - RUBY_CRUMBS_SIZE)
# define CONFIG_ARC_MUC_SRAM_B2_END		(CONFIG_ARC_MUC_SRAM_B2_BASE + CONFIG_ARC_MUC_SRAM_B2_SIZE)
# define CONFIG_ARC_AUC_SRAM_BASE		ROUNDUP(CONFIG_ARC_MUC_SRAM_B2_END, RUBY_SRAM_BANK_SIZE)
# define CONFIG_ARC_AUC_SRAM_SIZE		(4 * RUBY_SRAM_BANK_SIZE)
# define CONFIG_ARC_AUC_SRAM_END		(CONFIG_ARC_AUC_SRAM_BASE + CONFIG_ARC_AUC_SRAM_SIZE)
# define CONFIG_ARC_SRAM_END			CONFIG_ARC_AUC_SRAM_END
# if CONFIG_ARC_SRAM_END != RUBY_SRAM_SIZE
	#error SRAM linkmap error
# endif
#else	/* Ruby */
# define CONFIG_ARC_KERNEL_SRAM_B1_BASE		0x00000000
# define CONFIG_ARC_KERNEL_SRAM_B1_SIZE		RUBY_SRAM_BANK_SAFE_SIZE
# define CONFIG_ARC_KERNEL_SRAM_B1_END		(CONFIG_ARC_KERNEL_SRAM_B1_BASE + CONFIG_ARC_KERNEL_SRAM_B1_SIZE)
# define CONFIG_ARC_KERNEL_SRAM_B2_BASE		(CONFIG_ARC_KERNEL_SRAM_B1_BASE + RUBY_SRAM_BANK_SIZE)
# define CONFIG_ARC_KERNEL_SRAM_B2_SIZE		RUBY_SRAM_BANK_SAFE_SIZE
# define CONFIG_ARC_KERNEL_SRAM_B2_END		(CONFIG_ARC_KERNEL_SRAM_B2_BASE + CONFIG_ARC_KERNEL_SRAM_B2_SIZE)
# define CONFIG_ARC_MUC_SRAM_B1_BASE		ROUNDUP(CONFIG_ARC_KERNEL_SRAM_B2_END, RUBY_SRAM_BANK_SIZE)
# define CONFIG_ARC_MUC_SRAM_B1_SIZE		RUBY_SRAM_BANK_SAFE_SIZE
# define CONFIG_ARC_MUC_SRAM_B1_END		(CONFIG_ARC_MUC_SRAM_B1_BASE + CONFIG_ARC_MUC_SRAM_B1_SIZE)
# define CONFIG_ARC_MUC_SRAM_B2_BASE		ROUNDUP(CONFIG_ARC_MUC_SRAM_B1_END, RUBY_SRAM_BANK_SIZE)
# define CONFIG_ARC_MUC_SRAM_B2_SIZE		(RUBY_SRAM_BANK_SAFE_SIZE - RUBY_CRUMBS_SIZE)
# define CONFIG_ARC_MUC_SRAM_B2_END		(CONFIG_ARC_MUC_SRAM_B2_BASE + CONFIG_ARC_MUC_SRAM_B2_SIZE)
#endif

/* PCIe BDA area */
#define CONFIG_ARC_PCIE_BASE		(RUBY_DRAM_BEGIN + CONFIG_ARC_NULL_END)
#define CONFIG_ARC_PCIE_SIZE		(64 * 1024) /* minimal PCI BAR size */
#if ((CONFIG_ARC_PCIE_BASE & (64 * 1024 - 1)) != 0)
	#error "The reserverd region for PCIe BAR should 64k aligned!"
#endif

#if TOPAZ_RX_ACCELERATE
/* TODO FIXME - MuC crashed when copying data between SRAM and DDR */
#define CONFIG_ARC_MUC_STACK_INIT		(RUBY_SRAM_BEGIN + CONFIG_ARC_MUC_SRAM_B2_END - 2048)
#define CONFIG_ARC_MUC_STACK_SIZE		(4 * 1024) /* MuC stack, included in CONFIG_ARC_MUC_SRAM_SIZE */
#else
#define CONFIG_ARC_MUC_STACK_INIT		(RUBY_SRAM_BEGIN + CONFIG_ARC_MUC_SRAM_B2_END)
#define CONFIG_ARC_MUC_STACK_SIZE		(6 * 1024) /* MuC stack, included in CONFIG_ARC_MUC_SRAM_SIZE */
#endif
/*-*/
#define RUBY_CRUMBS_ADDR			(RUBY_SRAM_BEGIN + CONFIG_ARC_MUC_SRAM_B2_END)
/***************/

/* AuC tightly coupled memory specification */
#define TOPAZ_AUC_IMEM_ADDR		0xE5000000
#define TOPAZ_AUC_IMEM_SIZE		(32 * 1024)
#define TOPAZ_AUC_DMEM_ADDR		0xE5100000
#define TOPAZ_AUC_DMEM_SIZE		(16 * 1024)
/***************/

/*
 * Crumb structure, sits at the end of SRAM. Each core can use it to
 * store the last run function to detect bus hangs.
 */
#ifndef __ASSEMBLY__
struct ruby_crumbs_percore {
	unsigned long	blink;
	unsigned long	status32;
	unsigned long	sp;
};

struct ruby_crumbs_mem_section {
	unsigned long	start;
	unsigned long	end;
};

struct ruby_crumbs {
	struct ruby_crumbs_percore	lhost;
	struct ruby_crumbs_percore	muc;
	struct ruby_crumbs_percore	dsp;
	/*
	 * allow (somewhat) intelligent parsing of muc stacks by
	 * specifying the text section
	 */
	struct ruby_crumbs_mem_section	muc_dram;
	struct ruby_crumbs_mem_section	muc_sram;

	/*
	 * magic token; if set incorrectly we probably have
	 * random values after power-on
	 */
	unsigned long			magic;
};

#define	RUBY_CRUMBS_MAGIC	0x7c97be8f

#endif

/* Utility functions */
#ifndef __ASSEMBLY__

	#if defined(AUC_BUILD) || defined(RUBY_MINI)
		#define NO_RUBY_WEAK	1
	#else
		#define NO_RUBY_WEAK	0
	#endif

	#define RUBY_BAD_BUS_ADDR	((unsigned  long)0)
	#define RUBY_BAD_VIRT_ADDR	((void*)RUBY_BAD_BUS_ADDR)
	#define RUBY_ERROR_ADDR		((unsigned long)0xefefefef)

	#if defined(__CHECKER__)
		#define RUBY_INLINE			static inline __attribute__((always_inline))
		#define RUBY_WEAK(name)			RUBY_INLINE
		#define __sram_text
		#define __sram_data
	#elif defined(__GNUC__)
		/*GCC*/
		#if defined(CONFIG_ARCH_RUBY_NUMA) && defined(__KERNEL__) && defined(__linux__)
			/* Kernel is compiled with -mlong-calls option, so we can make calls between code fragments placed in different memories */
			#define __sram_text_sect_name	".sram.text"
			#define __sram_data_sect_name	".sram.data"
			#if defined(PROFILE_LINUX_EP) || defined(TOPAZ_PLATFORM)
			# define __sram_text
			# define __sram_data
			#else
			# define __sram_text		__attribute__ ((__section__ (__sram_text_sect_name)))
			# define __sram_data		__attribute__ ((__section__ (__sram_data_sect_name)))
			#endif
		#else
			#define __sram_text_sect_name	".text"
			#define __sram_data_sect_name	".data"
			#define __sram_text
			#define __sram_data
		#endif
		#define RUBY_INLINE			static inline __attribute__((always_inline))
		#if NO_RUBY_WEAK
			#define RUBY_WEAK(name)		RUBY_INLINE
		#else
			#define RUBY_WEAK(name)		__attribute__((weak))
		#endif
	#else
		/*MCC*/
		#define RUBY_INLINE			static _Inline
		#if NO_RUBY_WEAK
			#define RUBY_WEAK(name)		RUBY_INLINE
		#else
			#define RUBY_WEAK(name)		pragma Weak(name);
		#endif
		#pragma Offwarn(428)
	#endif

	#define ____in_mem_range(addr, start, size)	\
		(((addr) >= (start)) && ((addr) < (start) + (size)))

	#if defined(STATIC_CHECK) || defined(__CHECKER__)
		RUBY_INLINE int __in_mem_range(unsigned long addr, unsigned long start, unsigned long size)
		{
			return (((addr) >= (start)) && ((addr) < (start) + (size)));
		}
	#else
		#define __in_mem_range ____in_mem_range
	#endif

	RUBY_INLINE int is_valid_mem_addr(unsigned long addr)
	{
		if (__in_mem_range(addr, RUBY_SRAM_BEGIN, RUBY_SRAM_SIZE)) {
			return 1;
		} else if (__in_mem_range(addr, RUBY_DRAM_BEGIN, RUBY_MAX_DRAM_SIZE)) {
			return 1;
		}
		return 0;
	}

	#if RUBY_MMAP_FLIP
		RUBY_INLINE unsigned long virt_to_bus(const void *addr)
		{
			unsigned long ret = (unsigned long)addr;
			if (__in_mem_range(ret, RUBY_SRAM_FLIP_BEGIN, RUBY_SRAM_SIZE)) {
				ret = ret - RUBY_SRAM_FLIP_BEGIN + RUBY_SRAM_NOFLIP_BEGIN;
			} else if (__in_mem_range(ret, RUBY_DRAM_FLIP_BEGIN, RUBY_MAX_DRAM_SIZE)) {
				ret = ret - RUBY_DRAM_FLIP_BEGIN + RUBY_DRAM_NOFLIP_BEGIN;
			} else if (ret < RUBY_HARDWARE_BEGIN) {
				ret = RUBY_BAD_BUS_ADDR;
			}
			return ret;
		}
		RUBY_WEAK(bus_to_virt) void* bus_to_virt(unsigned long addr)
		{
			unsigned long ret = addr;
			if (__in_mem_range(ret, RUBY_SRAM_NOFLIP_BEGIN, RUBY_SRAM_SIZE)) {
				ret = ret - RUBY_SRAM_NOFLIP_BEGIN + RUBY_SRAM_FLIP_BEGIN;
			} else if (__in_mem_range(ret, RUBY_DRAM_NOFLIP_BEGIN, RUBY_MAX_DRAM_SIZE)) {
				ret = ret - RUBY_DRAM_NOFLIP_BEGIN + RUBY_DRAM_FLIP_BEGIN;
			} else if (ret < RUBY_HARDWARE_BEGIN) {
				ret = (unsigned long)RUBY_BAD_VIRT_ADDR;
			}
			return (void*)ret;
		}
	#else
		/* Map 1:1, (x) address must be upper then 0x8000_0000. */
		#define virt_to_bus(x) ((unsigned long)(x))
		#define bus_to_virt(x) ((void *)(x))
	#endif // #if RUBY_MMAP_FLIP

	#if TOPAZ_MMAP_UNIFIED
		RUBY_WEAK(virt_to_nocache) void* virt_to_nocache(const void *addr)
		{
			unsigned long ret = (unsigned long)addr;
			if (__in_mem_range(ret, RUBY_SRAM_BEGIN, RUBY_SRAM_SIZE)) {
				ret = ret - RUBY_SRAM_BEGIN + RUBY_SRAM_NOCACHE_BEGIN;
			} else if (__in_mem_range(ret, RUBY_DRAM_BEGIN, RUBY_MAX_DRAM_SIZE)) {
				ret = ret - RUBY_DRAM_BEGIN + RUBY_DRAM_NOCACHE_BEGIN;
			} else if (ret < RUBY_HARDWARE_BEGIN) {
				ret = (unsigned long)RUBY_BAD_VIRT_ADDR;
			}
			return (void*)ret;
		}
		RUBY_WEAK(nocache_to_virt) void* nocache_to_virt(const void *addr)
		{
			unsigned long ret = (unsigned long)addr;
			if (__in_mem_range(ret, RUBY_SRAM_NOCACHE_BEGIN, RUBY_SRAM_SIZE)) {
				ret = ret - RUBY_SRAM_NOCACHE_BEGIN + RUBY_SRAM_BEGIN;
			} else if (__in_mem_range(ret, RUBY_DRAM_NOCACHE_BEGIN, RUBY_MAX_DRAM_SIZE)) {
				ret = ret - RUBY_DRAM_NOCACHE_BEGIN + RUBY_DRAM_BEGIN;
			} else if (ret < RUBY_HARDWARE_BEGIN) {
				ret = (unsigned long)RUBY_BAD_VIRT_ADDR;
			}
			return (void*)ret;
		}
	#endif

	#if RUBY_MUC_TLB_ENABLE
		#if TOPAZ_MMAP_UNIFIED
			#define muc_to_nocache virt_to_nocache
			#define nocache_to_muc nocache_to_virt
		#else
			RUBY_WEAK(muc_to_nocache) void* muc_to_nocache(const void *addr)
			{
				unsigned long ret = (unsigned long)addr;
				if (__in_mem_range(ret, RUBY_SRAM_NOFLIP_BEGIN, RUBY_SRAM_SIZE)) {
					ret = ret - RUBY_SRAM_NOFLIP_BEGIN + RUBY_SRAM_NOFLIP_NOCACHE_BEGIN;
				} else if (__in_mem_range(ret, RUBY_DRAM_NOFLIP_BEGIN, RUBY_MAX_DRAM_SIZE)) {
					ret = ret - RUBY_DRAM_NOFLIP_BEGIN + RUBY_DRAM_NOFLIP_NOCACHE_BEGIN;
				} else if (ret < RUBY_HARDWARE_BEGIN) {
					ret = (unsigned long)RUBY_BAD_VIRT_ADDR;
				}
				return (void*)ret;
			}
			RUBY_WEAK(nocache_to_muc) void* nocache_to_muc(const void *addr)
			{
				unsigned long ret = (unsigned long)addr;
				if (__in_mem_range(ret, RUBY_SRAM_NOFLIP_NOCACHE_BEGIN, RUBY_SRAM_SIZE)) {
					ret = ret - RUBY_SRAM_NOFLIP_NOCACHE_BEGIN + RUBY_SRAM_NOFLIP_BEGIN;
				} else if (__in_mem_range(ret, RUBY_DRAM_NOFLIP_NOCACHE_BEGIN, RUBY_MAX_DRAM_SIZE)) {
					ret = ret - RUBY_DRAM_NOFLIP_NOCACHE_BEGIN + RUBY_DRAM_NOFLIP_BEGIN;
				} else if (ret < RUBY_HARDWARE_BEGIN) {
					ret = (unsigned long)RUBY_BAD_VIRT_ADDR;
				}
				return (void*)ret;
			}
		#endif
		#ifndef MUC_BUILD
			RUBY_INLINE unsigned long muc_to_lhost(unsigned long addr)
			{
				void *tmp = nocache_to_muc((void*)addr);
				if (tmp != RUBY_BAD_VIRT_ADDR) {
					addr = (unsigned long)tmp;
				}
				return (unsigned long)bus_to_virt(addr);
			}
		#endif // #ifndef MUC_BUILD
	#else
		#define muc_to_nocache(x) ((void*)(x))
		#define nocache_to_muc(x) ((void*)(x))
		#ifndef MUC_BUILD
			#define muc_to_lhost(x)   ((unsigned long)bus_to_virt((unsigned long)(x)))
		#endif // #ifndef MUC_BUILD
	#endif // #if RUBY_MUC_TLB_ENABLE

	#ifndef __GNUC__
		/*MCC*/
		#pragma Popwarn()
	#endif

#endif // #ifndef __ASSEMBLY__


/*
 * "Write memory barrier" instruction emulation.
 * Ruby platform has complex net of connected buses.
 * Write transactions are buffered.
 * qtn_wmb() guarantees that all issued earlier and pending writes
 * to system controller, to SRAM and to DDR are completed
 * before qtn_wmb() is finished.
 * For complete safety Linux's wmb() should be defined
 * through qtn_wmb(), but I afraid it would kill performance.
 */
#ifndef __ASSEMBLY__
	#define RUBY_SYS_CTL_SAFE_READ_REGISTER 0xE0000000
	#if defined(__GNUC__) && defined(__i386__)
		#define qtn_wmb()		do {} while(0)
		static inline unsigned long _qtn_addr_wmb(unsigned long *addr) { return *addr; }
		#define qtn_addr_wmb(addr)	_qtn_addr_wmb((unsigned long *)(addr))
		#define qtn_pipeline_drain()	do {} while(0)
	#elif defined(__GNUC__)
		/*GCC*/
		#if defined(__arc__)
			#define qtn_wmb() \
			({ \
				unsigned long temp; \
				__asm__ __volatile__ ( \
					"ld.di %0, [%1]\n\t" \
					"ld.di %0, [%2]\n\t" \
					"ld.di %0, [%3]\n\t" \
					"sync\n\t" \
					: "=r"(temp) \
					: "i"(RUBY_DRAM_BEGIN + CONFIG_ARC_KERNEL_MEM_BASE), "i"(RUBY_SRAM_BEGIN + CONFIG_ARC_KERNEL_SRAM_B1_BASE), "i"(RUBY_SYS_CTL_SAFE_READ_REGISTER) \
					: "memory"); \
			})
			#define qtn_addr_wmb(addr) \
			({ \
				unsigned long temp; \
				__asm__ __volatile__ ( \
					"ld.di %0, [%1]\n\t" \
					"sync\n\t" \
					: "=r"(temp) \
					: "r"(addr) \
					: "memory"); \
				temp; \
			})
			#define qtn_pipeline_drain() \
			({ \
				__asm__ __volatile__ ( \
					"sync\n\t" \
					: : : "memory"); \
			})
		#else
			#define qtn_wmb()
			#define qtn_addr_wmb(addr)	*((volatile uint32_t*)addr)
			#define qtn_pipeline_drain()
		#endif
	#else
		/*MCC*/
		#if _ARCVER >= 0x31/*ARC7*/
			#define _qtn_pipeline_drain() \
				sync
		#else
			#define _qtn_pipeline_drain() \
				nop_s; nop_s; nop_s
		#endif
		_Asm void qtn_wmb(void)
		{
			/*r12 is temporary register, so we can use it inside this function freely*/
			ld.di %r12, [RUBY_DRAM_BEGIN + CONFIG_ARC_MUC_BASE]
			ld.di %r12, [RUBY_SRAM_BEGIN + CONFIG_ARC_MUC_SRAM_B1_BASE]
			ld.di %r12, [RUBY_SYS_CTL_SAFE_READ_REGISTER]
			_qtn_pipeline_drain()
		}
		_Asm u_int32_t qtn_addr_wmb(unsigned long addr)
		{
			%reg addr;
			ld.di %r0, [addr]
			_qtn_pipeline_drain()
		}
		_Asm void qtn_pipeline_drain(void)
		{
			_qtn_pipeline_drain()
		}
	#endif
#endif

/*
 * Problem - writing to first half of cache way trash second half.
 * Idea is to lock second half.
 * Need make sure that invalidation does not unlock these lines (whole
 * cache invalidation unlocks), or need to re-lock lines back.
 * Also side effect - half of lines will be cached, half - not.
 * So may need to shuffle data to make hot data cacheable.
 */
#define TOPAZ_CACHE_WAR_OFFSET	2048
#ifndef __ASSEMBLY__
#ifdef __GNUC__
RUBY_INLINE void qtn_cache_topaz_war_dcache_lock(unsigned long aux_reg, unsigned long val)
{
	unsigned long addr;
	unsigned long way_iter;
	unsigned long line_iter;

	asm volatile (
		"	sr	%4, [%3]\n"
		"	mov	%0, 0xA0000000\n"
		"	mov	%1, 0\n"
		"1:	add	%0, %0, 2048\n"
		"	mov	%2, 0\n"
		"2:	sr	%0, [0x49]\n"
		"	add	%0, %0, 32\n"
		"	add	%2, %2, 1\n"
		"	cmp	%2, 64\n"
		"	bne	2b\n"
		"	add	%1, %1, 1\n"
		"	cmp	%1, 4\n"
		"	bne	1b\n"
		: "=r"(addr), "=r"(way_iter), "=r"(line_iter)
		: "r"(aux_reg), "r"(val)
	);
}
#else
_Inline _Asm  void qtn_cache_topaz_war_dcache_lock(unsigned long aux_reg, unsigned long val)
{
	% reg aux_reg, reg val;

	sr	val, [aux_reg]
	mov	%r0, 0xA0000000
	mov	%r1, 0
	1:	add	%r0, %r0, 2048
	mov	%r2, 0
	2:	sr	%r0, [0x49]
	add	%r0, %r0, 32
	add	%r2, %r2, 1
	cmp	%r2, 64
	bne	2b
	add	%r1, %r1, 1
	cmp	%r1, 4
	bne	1b
}
#endif // #ifdef __GNUC__
#endif // #ifndef __ASSEMBLY__

#endif // #ifndef __RUBY_MEM_H
