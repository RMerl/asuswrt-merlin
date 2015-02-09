/*
 * NAND Flash Controller Device Driver
 * Copyright (c) 2009 - 2010, Intel Corporation and its suppliers.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <linux/mtd/nand.h>

#define DEVICE_RESET				0x0
#define     DEVICE_RESET__BANK0				0x0001
#define     DEVICE_RESET__BANK1				0x0002
#define     DEVICE_RESET__BANK2				0x0004
#define     DEVICE_RESET__BANK3				0x0008

#define TRANSFER_SPARE_REG			0x10
#define     TRANSFER_SPARE_REG__FLAG			0x0001

#define LOAD_WAIT_CNT				0x20
#define     LOAD_WAIT_CNT__VALUE			0xffff

#define PROGRAM_WAIT_CNT			0x30
#define     PROGRAM_WAIT_CNT__VALUE			0xffff

#define ERASE_WAIT_CNT				0x40
#define     ERASE_WAIT_CNT__VALUE			0xffff

#define INT_MON_CYCCNT				0x50
#define     INT_MON_CYCCNT__VALUE			0xffff

#define RB_PIN_ENABLED				0x60
#define     RB_PIN_ENABLED__BANK0			0x0001
#define     RB_PIN_ENABLED__BANK1			0x0002
#define     RB_PIN_ENABLED__BANK2			0x0004
#define     RB_PIN_ENABLED__BANK3			0x0008

#define MULTIPLANE_OPERATION			0x70
#define     MULTIPLANE_OPERATION__FLAG			0x0001

#define MULTIPLANE_READ_ENABLE			0x80
#define     MULTIPLANE_READ_ENABLE__FLAG		0x0001

#define COPYBACK_DISABLE			0x90
#define     COPYBACK_DISABLE__FLAG			0x0001

#define CACHE_WRITE_ENABLE			0xa0
#define     CACHE_WRITE_ENABLE__FLAG			0x0001

#define CACHE_READ_ENABLE			0xb0
#define     CACHE_READ_ENABLE__FLAG			0x0001

#define PREFETCH_MODE				0xc0
#define     PREFETCH_MODE__PREFETCH_EN			0x0001
#define     PREFETCH_MODE__PREFETCH_BURST_LENGTH	0xfff0

#define CHIP_ENABLE_DONT_CARE			0xd0
#define     CHIP_EN_DONT_CARE__FLAG			0x01

#define ECC_ENABLE				0xe0
#define     ECC_ENABLE__FLAG				0x0001

#define GLOBAL_INT_ENABLE			0xf0
#define     GLOBAL_INT_EN_FLAG				0x01

#define WE_2_RE					0x100
#define     WE_2_RE__VALUE				0x003f

#define ADDR_2_DATA				0x110
#define     ADDR_2_DATA__VALUE				0x003f

#define RE_2_WE					0x120
#define     RE_2_WE__VALUE				0x003f

#define ACC_CLKS				0x130
#define     ACC_CLKS__VALUE				0x000f

#define NUMBER_OF_PLANES			0x140
#define     NUMBER_OF_PLANES__VALUE			0x0007

#define PAGES_PER_BLOCK				0x150
#define     PAGES_PER_BLOCK__VALUE			0xffff

#define DEVICE_WIDTH				0x160
#define     DEVICE_WIDTH__VALUE				0x0003

#define DEVICE_MAIN_AREA_SIZE			0x170
#define     DEVICE_MAIN_AREA_SIZE__VALUE		0xffff

#define DEVICE_SPARE_AREA_SIZE			0x180
#define     DEVICE_SPARE_AREA_SIZE__VALUE		0xffff

#define TWO_ROW_ADDR_CYCLES			0x190
#define     TWO_ROW_ADDR_CYCLES__FLAG			0x0001

#define MULTIPLANE_ADDR_RESTRICT		0x1a0
#define     MULTIPLANE_ADDR_RESTRICT__FLAG		0x0001

#define ECC_CORRECTION				0x1b0
#define     ECC_CORRECTION__VALUE			0x001f

#define READ_MODE				0x1c0
#define     READ_MODE__VALUE				0x000f

#define WRITE_MODE				0x1d0
#define     WRITE_MODE__VALUE				0x000f

#define COPYBACK_MODE				0x1e0
#define     COPYBACK_MODE__VALUE			0x000f

#define RDWR_EN_LO_CNT				0x1f0
#define     RDWR_EN_LO_CNT__VALUE			0x001f

#define RDWR_EN_HI_CNT				0x200
#define     RDWR_EN_HI_CNT__VALUE			0x001f

#define MAX_RD_DELAY				0x210
#define     MAX_RD_DELAY__VALUE				0x000f

#define CS_SETUP_CNT				0x220
#define     CS_SETUP_CNT__VALUE				0x001f

#define SPARE_AREA_SKIP_BYTES			0x230
#define     SPARE_AREA_SKIP_BYTES__VALUE		0x003f

#define SPARE_AREA_MARKER			0x240
#define     SPARE_AREA_MARKER__VALUE			0xffff

#define DEVICES_CONNECTED			0x250
#define     DEVICES_CONNECTED__VALUE			0x0007

#define DIE_MASK				0x260
#define     DIE_MASK__VALUE				0x00ff

#define FIRST_BLOCK_OF_NEXT_PLANE		0x270
#define     FIRST_BLOCK_OF_NEXT_PLANE__VALUE		0xffff

#define WRITE_PROTECT				0x280
#define     WRITE_PROTECT__FLAG				0x0001

#define RE_2_RE					0x290
#define     RE_2_RE__VALUE				0x003f

#define MANUFACTURER_ID				0x300
#define     MANUFACTURER_ID__VALUE			0x00ff

#define DEVICE_ID				0x310
#define     DEVICE_ID__VALUE				0x00ff

#define DEVICE_PARAM_0				0x320
#define     DEVICE_PARAM_0__VALUE			0x00ff

#define DEVICE_PARAM_1				0x330
#define     DEVICE_PARAM_1__VALUE			0x00ff

#define DEVICE_PARAM_2				0x340
#define     DEVICE_PARAM_2__VALUE			0x00ff

#define LOGICAL_PAGE_DATA_SIZE			0x350
#define     LOGICAL_PAGE_DATA_SIZE__VALUE		0xffff

#define LOGICAL_PAGE_SPARE_SIZE			0x360
#define     LOGICAL_PAGE_SPARE_SIZE__VALUE		0xffff

#define REVISION				0x370
#define     REVISION__VALUE				0xffff

#define ONFI_DEVICE_FEATURES			0x380
#define     ONFI_DEVICE_FEATURES__VALUE			0x003f

#define ONFI_OPTIONAL_COMMANDS			0x390
#define     ONFI_OPTIONAL_COMMANDS__VALUE		0x003f

#define ONFI_TIMING_MODE			0x3a0
#define     ONFI_TIMING_MODE__VALUE			0x003f

#define ONFI_PGM_CACHE_TIMING_MODE		0x3b0
#define     ONFI_PGM_CACHE_TIMING_MODE__VALUE		0x003f

#define ONFI_DEVICE_NO_OF_LUNS			0x3c0
#define     ONFI_DEVICE_NO_OF_LUNS__NO_OF_LUNS		0x00ff
#define     ONFI_DEVICE_NO_OF_LUNS__ONFI_DEVICE		0x0100

#define ONFI_DEVICE_NO_OF_BLOCKS_PER_LUN_L	0x3d0
#define     ONFI_DEVICE_NO_OF_BLOCKS_PER_LUN_L__VALUE	0xffff

#define ONFI_DEVICE_NO_OF_BLOCKS_PER_LUN_U	0x3e0
#define     ONFI_DEVICE_NO_OF_BLOCKS_PER_LUN_U__VALUE	0xffff

#define FEATURES					0x3f0
#define     FEATURES__N_BANKS				0x0003
#define     FEATURES__ECC_MAX_ERR			0x003c
#define     FEATURES__DMA				0x0040
#define     FEATURES__CMD_DMA				0x0080
#define     FEATURES__PARTITION				0x0100
#define     FEATURES__XDMA_SIDEBAND			0x0200
#define     FEATURES__GPREG				0x0400
#define     FEATURES__INDEX_ADDR			0x0800

#define TRANSFER_MODE				0x400
#define     TRANSFER_MODE__VALUE			0x0003

#define INTR_STATUS0				0x410
#define     INTR_STATUS0__ECC_TRANSACTION_DONE		0x0001
#define     INTR_STATUS0__ECC_ERR			0x0002
#define     INTR_STATUS0__DMA_CMD_COMP			0x0004
#define     INTR_STATUS0__TIME_OUT			0x0008
#define     INTR_STATUS0__PROGRAM_FAIL			0x0010
#define     INTR_STATUS0__ERASE_FAIL			0x0020
#define     INTR_STATUS0__LOAD_COMP			0x0040
#define     INTR_STATUS0__PROGRAM_COMP			0x0080
#define     INTR_STATUS0__ERASE_COMP			0x0100
#define     INTR_STATUS0__PIPE_CPYBCK_CMD_COMP		0x0200
#define     INTR_STATUS0__LOCKED_BLK			0x0400
#define     INTR_STATUS0__UNSUP_CMD			0x0800
#define     INTR_STATUS0__INT_ACT			0x1000
#define     INTR_STATUS0__RST_COMP			0x2000
#define     INTR_STATUS0__PIPE_CMD_ERR			0x4000
#define     INTR_STATUS0__PAGE_XFER_INC			0x8000

#define INTR_EN0					0x420
#define     INTR_EN0__ECC_TRANSACTION_DONE		0x0001
#define     INTR_EN0__ECC_ERR				0x0002
#define     INTR_EN0__DMA_CMD_COMP			0x0004
#define     INTR_EN0__TIME_OUT				0x0008
#define     INTR_EN0__PROGRAM_FAIL			0x0010
#define     INTR_EN0__ERASE_FAIL			0x0020
#define     INTR_EN0__LOAD_COMP				0x0040
#define     INTR_EN0__PROGRAM_COMP			0x0080
#define     INTR_EN0__ERASE_COMP			0x0100
#define     INTR_EN0__PIPE_CPYBCK_CMD_COMP		0x0200
#define     INTR_EN0__LOCKED_BLK			0x0400
#define     INTR_EN0__UNSUP_CMD				0x0800
#define     INTR_EN0__INT_ACT				0x1000
#define     INTR_EN0__RST_COMP				0x2000
#define     INTR_EN0__PIPE_CMD_ERR			0x4000
#define     INTR_EN0__PAGE_XFER_INC			0x8000

#define PAGE_CNT0				0x430
#define     PAGE_CNT0__VALUE				0x00ff

#define ERR_PAGE_ADDR0				0x440
#define     ERR_PAGE_ADDR0__VALUE			0xffff

#define ERR_BLOCK_ADDR0				0x450
#define     ERR_BLOCK_ADDR0__VALUE			0xffff

#define INTR_STATUS1				0x460
#define     INTR_STATUS1__ECC_TRANSACTION_DONE		0x0001
#define     INTR_STATUS1__ECC_ERR			0x0002
#define     INTR_STATUS1__DMA_CMD_COMP			0x0004
#define     INTR_STATUS1__TIME_OUT			0x0008
#define     INTR_STATUS1__PROGRAM_FAIL			0x0010
#define     INTR_STATUS1__ERASE_FAIL			0x0020
#define     INTR_STATUS1__LOAD_COMP			0x0040
#define     INTR_STATUS1__PROGRAM_COMP			0x0080
#define     INTR_STATUS1__ERASE_COMP			0x0100
#define     INTR_STATUS1__PIPE_CPYBCK_CMD_COMP		0x0200
#define     INTR_STATUS1__LOCKED_BLK			0x0400
#define     INTR_STATUS1__UNSUP_CMD			0x0800
#define     INTR_STATUS1__INT_ACT			0x1000
#define     INTR_STATUS1__RST_COMP			0x2000
#define     INTR_STATUS1__PIPE_CMD_ERR			0x4000
#define     INTR_STATUS1__PAGE_XFER_INC			0x8000

#define INTR_EN1					0x470
#define     INTR_EN1__ECC_TRANSACTION_DONE		0x0001
#define     INTR_EN1__ECC_ERR				0x0002
#define     INTR_EN1__DMA_CMD_COMP			0x0004
#define     INTR_EN1__TIME_OUT				0x0008
#define     INTR_EN1__PROGRAM_FAIL			0x0010
#define     INTR_EN1__ERASE_FAIL			0x0020
#define     INTR_EN1__LOAD_COMP				0x0040
#define     INTR_EN1__PROGRAM_COMP			0x0080
#define     INTR_EN1__ERASE_COMP			0x0100
#define     INTR_EN1__PIPE_CPYBCK_CMD_COMP		0x0200
#define     INTR_EN1__LOCKED_BLK			0x0400
#define     INTR_EN1__UNSUP_CMD				0x0800
#define     INTR_EN1__INT_ACT				0x1000
#define     INTR_EN1__RST_COMP				0x2000
#define     INTR_EN1__PIPE_CMD_ERR			0x4000
#define     INTR_EN1__PAGE_XFER_INC			0x8000

#define PAGE_CNT1				0x480
#define     PAGE_CNT1__VALUE				0x00ff

#define ERR_PAGE_ADDR1				0x490
#define     ERR_PAGE_ADDR1__VALUE			0xffff

#define ERR_BLOCK_ADDR1				0x4a0
#define     ERR_BLOCK_ADDR1__VALUE			0xffff

#define INTR_STATUS2				0x4b0
#define     INTR_STATUS2__ECC_TRANSACTION_DONE		0x0001
#define     INTR_STATUS2__ECC_ERR			0x0002
#define     INTR_STATUS2__DMA_CMD_COMP			0x0004
#define     INTR_STATUS2__TIME_OUT			0x0008
#define     INTR_STATUS2__PROGRAM_FAIL			0x0010
#define     INTR_STATUS2__ERASE_FAIL			0x0020
#define     INTR_STATUS2__LOAD_COMP			0x0040
#define     INTR_STATUS2__PROGRAM_COMP			0x0080
#define     INTR_STATUS2__ERASE_COMP			0x0100
#define     INTR_STATUS2__PIPE_CPYBCK_CMD_COMP		0x0200
#define     INTR_STATUS2__LOCKED_BLK			0x0400
#define     INTR_STATUS2__UNSUP_CMD			0x0800
#define     INTR_STATUS2__INT_ACT			0x1000
#define     INTR_STATUS2__RST_COMP			0x2000
#define     INTR_STATUS2__PIPE_CMD_ERR			0x4000
#define     INTR_STATUS2__PAGE_XFER_INC			0x8000

#define INTR_EN2					0x4c0
#define     INTR_EN2__ECC_TRANSACTION_DONE		0x0001
#define     INTR_EN2__ECC_ERR				0x0002
#define     INTR_EN2__DMA_CMD_COMP			0x0004
#define     INTR_EN2__TIME_OUT				0x0008
#define     INTR_EN2__PROGRAM_FAIL			0x0010
#define     INTR_EN2__ERASE_FAIL			0x0020
#define     INTR_EN2__LOAD_COMP				0x0040
#define     INTR_EN2__PROGRAM_COMP			0x0080
#define     INTR_EN2__ERASE_COMP			0x0100
#define     INTR_EN2__PIPE_CPYBCK_CMD_COMP		0x0200
#define     INTR_EN2__LOCKED_BLK			0x0400
#define     INTR_EN2__UNSUP_CMD				0x0800
#define     INTR_EN2__INT_ACT				0x1000
#define     INTR_EN2__RST_COMP				0x2000
#define     INTR_EN2__PIPE_CMD_ERR			0x4000
#define     INTR_EN2__PAGE_XFER_INC			0x8000

#define PAGE_CNT2				0x4d0
#define     PAGE_CNT2__VALUE				0x00ff

#define ERR_PAGE_ADDR2				0x4e0
#define     ERR_PAGE_ADDR2__VALUE			0xffff

#define ERR_BLOCK_ADDR2				0x4f0
#define     ERR_BLOCK_ADDR2__VALUE			0xffff

#define INTR_STATUS3				0x500
#define     INTR_STATUS3__ECC_TRANSACTION_DONE		0x0001
#define     INTR_STATUS3__ECC_ERR			0x0002
#define     INTR_STATUS3__DMA_CMD_COMP			0x0004
#define     INTR_STATUS3__TIME_OUT			0x0008
#define     INTR_STATUS3__PROGRAM_FAIL			0x0010
#define     INTR_STATUS3__ERASE_FAIL			0x0020
#define     INTR_STATUS3__LOAD_COMP			0x0040
#define     INTR_STATUS3__PROGRAM_COMP			0x0080
#define     INTR_STATUS3__ERASE_COMP			0x0100
#define     INTR_STATUS3__PIPE_CPYBCK_CMD_COMP		0x0200
#define     INTR_STATUS3__LOCKED_BLK			0x0400
#define     INTR_STATUS3__UNSUP_CMD			0x0800
#define     INTR_STATUS3__INT_ACT			0x1000
#define     INTR_STATUS3__RST_COMP			0x2000
#define     INTR_STATUS3__PIPE_CMD_ERR			0x4000
#define     INTR_STATUS3__PAGE_XFER_INC			0x8000

#define INTR_EN3					0x510
#define     INTR_EN3__ECC_TRANSACTION_DONE		0x0001
#define     INTR_EN3__ECC_ERR				0x0002
#define     INTR_EN3__DMA_CMD_COMP			0x0004
#define     INTR_EN3__TIME_OUT				0x0008
#define     INTR_EN3__PROGRAM_FAIL			0x0010
#define     INTR_EN3__ERASE_FAIL			0x0020
#define     INTR_EN3__LOAD_COMP				0x0040
#define     INTR_EN3__PROGRAM_COMP			0x0080
#define     INTR_EN3__ERASE_COMP			0x0100
#define     INTR_EN3__PIPE_CPYBCK_CMD_COMP		0x0200
#define     INTR_EN3__LOCKED_BLK			0x0400
#define     INTR_EN3__UNSUP_CMD				0x0800
#define     INTR_EN3__INT_ACT				0x1000
#define     INTR_EN3__RST_COMP				0x2000
#define     INTR_EN3__PIPE_CMD_ERR			0x4000
#define     INTR_EN3__PAGE_XFER_INC			0x8000

#define PAGE_CNT3				0x520
#define     PAGE_CNT3__VALUE				0x00ff

#define ERR_PAGE_ADDR3				0x530
#define     ERR_PAGE_ADDR3__VALUE			0xffff

#define ERR_BLOCK_ADDR3				0x540
#define     ERR_BLOCK_ADDR3__VALUE			0xffff

#define DATA_INTR				0x550
#define     DATA_INTR__WRITE_SPACE_AV			0x0001
#define     DATA_INTR__READ_DATA_AV			0x0002

#define DATA_INTR_EN				0x560
#define     DATA_INTR_EN__WRITE_SPACE_AV		0x0001
#define     DATA_INTR_EN__READ_DATA_AV			0x0002

#define GPREG_0					0x570
#define     GPREG_0__VALUE				0xffff

#define GPREG_1					0x580
#define     GPREG_1__VALUE				0xffff

#define GPREG_2					0x590
#define     GPREG_2__VALUE				0xffff

#define GPREG_3					0x5a0
#define     GPREG_3__VALUE				0xffff

#define ECC_THRESHOLD				0x600
#define     ECC_THRESHOLD__VALUE			0x03ff

#define ECC_ERROR_BLOCK_ADDRESS			0x610
#define     ECC_ERROR_BLOCK_ADDRESS__VALUE		0xffff

#define ECC_ERROR_PAGE_ADDRESS			0x620
#define     ECC_ERROR_PAGE_ADDRESS__VALUE		0x0fff
#define     ECC_ERROR_PAGE_ADDRESS__BANK		0xf000

#define ECC_ERROR_ADDRESS			0x630
#define     ECC_ERROR_ADDRESS__OFFSET			0x0fff
#define     ECC_ERROR_ADDRESS__SECTOR_NR		0xf000

#define ERR_CORRECTION_INFO			0x640
#define     ERR_CORRECTION_INFO__BYTEMASK		0x00ff
#define     ERR_CORRECTION_INFO__DEVICE_NR		0x0f00
#define     ERR_CORRECTION_INFO__ERROR_TYPE		0x4000
#define     ERR_CORRECTION_INFO__LAST_ERR_INFO		0x8000

#define DMA_ENABLE				0x700
#define     DMA_ENABLE__FLAG				0x0001

#define IGNORE_ECC_DONE				0x710
#define     IGNORE_ECC_DONE__FLAG			0x0001

#define DMA_INTR				0x720
#define     DMA_INTR__TARGET_ERROR			0x0001
#define     DMA_INTR__DESC_COMP_CHANNEL0		0x0002
#define     DMA_INTR__DESC_COMP_CHANNEL1		0x0004
#define     DMA_INTR__DESC_COMP_CHANNEL2		0x0008
#define     DMA_INTR__DESC_COMP_CHANNEL3		0x0010
#define     DMA_INTR__MEMCOPY_DESC_COMP		0x0020

#define DMA_INTR_EN				0x730
#define     DMA_INTR_EN__TARGET_ERROR			0x0001
#define     DMA_INTR_EN__DESC_COMP_CHANNEL0		0x0002
#define     DMA_INTR_EN__DESC_COMP_CHANNEL1		0x0004
#define     DMA_INTR_EN__DESC_COMP_CHANNEL2		0x0008
#define     DMA_INTR_EN__DESC_COMP_CHANNEL3		0x0010
#define     DMA_INTR_EN__MEMCOPY_DESC_COMP		0x0020

#define TARGET_ERR_ADDR_LO			0x740
#define     TARGET_ERR_ADDR_LO__VALUE			0xffff

#define TARGET_ERR_ADDR_HI			0x750
#define     TARGET_ERR_ADDR_HI__VALUE			0xffff

#define CHNL_ACTIVE				0x760
#define     CHNL_ACTIVE__CHANNEL0			0x0001
#define     CHNL_ACTIVE__CHANNEL1			0x0002
#define     CHNL_ACTIVE__CHANNEL2			0x0004
#define     CHNL_ACTIVE__CHANNEL3			0x0008

#define ACTIVE_SRC_ID				0x800
#define     ACTIVE_SRC_ID__VALUE			0x00ff

#define PTN_INTR					0x810
#define     PTN_INTR__CONFIG_ERROR			0x0001
#define     PTN_INTR__ACCESS_ERROR_BANK0		0x0002
#define     PTN_INTR__ACCESS_ERROR_BANK1		0x0004
#define     PTN_INTR__ACCESS_ERROR_BANK2		0x0008
#define     PTN_INTR__ACCESS_ERROR_BANK3		0x0010
#define     PTN_INTR__REG_ACCESS_ERROR			0x0020

#define PTN_INTR_EN				0x820
#define     PTN_INTR_EN__CONFIG_ERROR			0x0001
#define     PTN_INTR_EN__ACCESS_ERROR_BANK0		0x0002
#define     PTN_INTR_EN__ACCESS_ERROR_BANK1		0x0004
#define     PTN_INTR_EN__ACCESS_ERROR_BANK2		0x0008
#define     PTN_INTR_EN__ACCESS_ERROR_BANK3		0x0010
#define     PTN_INTR_EN__REG_ACCESS_ERROR		0x0020

#define PERM_SRC_ID_0				0x830
#define     PERM_SRC_ID_0__SRCID			0x00ff
#define     PERM_SRC_ID_0__DIRECT_ACCESS_ACTIVE		0x0800
#define     PERM_SRC_ID_0__WRITE_ACTIVE			0x2000
#define     PERM_SRC_ID_0__READ_ACTIVE			0x4000
#define     PERM_SRC_ID_0__PARTITION_VALID		0x8000

#define MIN_BLK_ADDR_0				0x840
#define     MIN_BLK_ADDR_0__VALUE			0xffff

#define MAX_BLK_ADDR_0				0x850
#define     MAX_BLK_ADDR_0__VALUE			0xffff

#define MIN_MAX_BANK_0				0x860
#define     MIN_MAX_BANK_0__MIN_VALUE			0x0003
#define     MIN_MAX_BANK_0__MAX_VALUE			0x000c

#define PERM_SRC_ID_1				0x870
#define     PERM_SRC_ID_1__SRCID			0x00ff
#define     PERM_SRC_ID_1__DIRECT_ACCESS_ACTIVE		0x0800
#define     PERM_SRC_ID_1__WRITE_ACTIVE			0x2000
#define     PERM_SRC_ID_1__READ_ACTIVE			0x4000
#define     PERM_SRC_ID_1__PARTITION_VALID		0x8000

#define MIN_BLK_ADDR_1				0x880
#define     MIN_BLK_ADDR_1__VALUE			0xffff

#define MAX_BLK_ADDR_1				0x890
#define     MAX_BLK_ADDR_1__VALUE			0xffff

#define MIN_MAX_BANK_1				0x8a0
#define     MIN_MAX_BANK_1__MIN_VALUE			0x0003
#define     MIN_MAX_BANK_1__MAX_VALUE			0x000c

#define PERM_SRC_ID_2				0x8b0
#define     PERM_SRC_ID_2__SRCID			0x00ff
#define     PERM_SRC_ID_2__DIRECT_ACCESS_ACTIVE		0x0800
#define     PERM_SRC_ID_2__WRITE_ACTIVE			0x2000
#define     PERM_SRC_ID_2__READ_ACTIVE			0x4000
#define     PERM_SRC_ID_2__PARTITION_VALID		0x8000

#define MIN_BLK_ADDR_2				0x8c0
#define     MIN_BLK_ADDR_2__VALUE			0xffff

#define MAX_BLK_ADDR_2				0x8d0
#define     MAX_BLK_ADDR_2__VALUE			0xffff

#define MIN_MAX_BANK_2				0x8e0
#define     MIN_MAX_BANK_2__MIN_VALUE			0x0003
#define     MIN_MAX_BANK_2__MAX_VALUE			0x000c

#define PERM_SRC_ID_3				0x8f0
#define     PERM_SRC_ID_3__SRCID			0x00ff
#define     PERM_SRC_ID_3__DIRECT_ACCESS_ACTIVE		0x0800
#define     PERM_SRC_ID_3__WRITE_ACTIVE			0x2000
#define     PERM_SRC_ID_3__READ_ACTIVE			0x4000
#define     PERM_SRC_ID_3__PARTITION_VALID		0x8000

#define MIN_BLK_ADDR_3				0x900
#define     MIN_BLK_ADDR_3__VALUE			0xffff

#define MAX_BLK_ADDR_3				0x910
#define     MAX_BLK_ADDR_3__VALUE			0xffff

#define MIN_MAX_BANK_3				0x920
#define     MIN_MAX_BANK_3__MIN_VALUE			0x0003
#define     MIN_MAX_BANK_3__MAX_VALUE			0x000c

#define PERM_SRC_ID_4				0x930
#define     PERM_SRC_ID_4__SRCID			0x00ff
#define     PERM_SRC_ID_4__DIRECT_ACCESS_ACTIVE		0x0800
#define     PERM_SRC_ID_4__WRITE_ACTIVE			0x2000
#define     PERM_SRC_ID_4__READ_ACTIVE			0x4000
#define     PERM_SRC_ID_4__PARTITION_VALID		0x8000

#define MIN_BLK_ADDR_4				0x940
#define     MIN_BLK_ADDR_4__VALUE			0xffff

#define MAX_BLK_ADDR_4				0x950
#define     MAX_BLK_ADDR_4__VALUE			0xffff

#define MIN_MAX_BANK_4				0x960
#define     MIN_MAX_BANK_4__MIN_VALUE			0x0003
#define     MIN_MAX_BANK_4__MAX_VALUE			0x000c

#define PERM_SRC_ID_5				0x970
#define     PERM_SRC_ID_5__SRCID			0x00ff
#define     PERM_SRC_ID_5__DIRECT_ACCESS_ACTIVE		0x0800
#define     PERM_SRC_ID_5__WRITE_ACTIVE			0x2000
#define     PERM_SRC_ID_5__READ_ACTIVE			0x4000
#define     PERM_SRC_ID_5__PARTITION_VALID		0x8000

#define MIN_BLK_ADDR_5				0x980
#define     MIN_BLK_ADDR_5__VALUE			0xffff

#define MAX_BLK_ADDR_5				0x990
#define     MAX_BLK_ADDR_5__VALUE			0xffff

#define MIN_MAX_BANK_5				0x9a0
#define     MIN_MAX_BANK_5__MIN_VALUE			0x0003
#define     MIN_MAX_BANK_5__MAX_VALUE			0x000c

#define PERM_SRC_ID_6				0x9b0
#define     PERM_SRC_ID_6__SRCID			0x00ff
#define     PERM_SRC_ID_6__DIRECT_ACCESS_ACTIVE		0x0800
#define     PERM_SRC_ID_6__WRITE_ACTIVE			0x2000
#define     PERM_SRC_ID_6__READ_ACTIVE			0x4000
#define     PERM_SRC_ID_6__PARTITION_VALID		0x8000

#define MIN_BLK_ADDR_6				0x9c0
#define     MIN_BLK_ADDR_6__VALUE			0xffff

#define MAX_BLK_ADDR_6				0x9d0
#define     MAX_BLK_ADDR_6__VALUE			0xffff

#define MIN_MAX_BANK_6				0x9e0
#define     MIN_MAX_BANK_6__MIN_VALUE			0x0003
#define     MIN_MAX_BANK_6__MAX_VALUE			0x000c

#define PERM_SRC_ID_7				0x9f0
#define     PERM_SRC_ID_7__SRCID			0x00ff
#define     PERM_SRC_ID_7__DIRECT_ACCESS_ACTIVE		0x0800
#define     PERM_SRC_ID_7__WRITE_ACTIVE			0x2000
#define     PERM_SRC_ID_7__READ_ACTIVE			0x4000
#define     PERM_SRC_ID_7__PARTITION_VALID		0x8000

#define MIN_BLK_ADDR_7				0xa00
#define     MIN_BLK_ADDR_7__VALUE			0xffff

#define MAX_BLK_ADDR_7				0xa10
#define     MAX_BLK_ADDR_7__VALUE			0xffff

#define MIN_MAX_BANK_7				0xa20
#define     MIN_MAX_BANK_7__MIN_VALUE			0x0003
#define     MIN_MAX_BANK_7__MAX_VALUE			0x000c

/* ffsdefs.h */
#define CLEAR 0                 /*use this to clear a field instead of "fail"*/
#define SET   1                 /*use this to set a field instead of "pass"*/
#define FAIL 1                  /*failed flag*/
#define PASS 0                  /*success flag*/
#define ERR -1                  /*error flag*/

/* lld.h */
#define GOOD_BLOCK 0
#define DEFECTIVE_BLOCK 1
#define READ_ERROR 2

#define CLK_X  5
#define CLK_MULTI 4

/* spectraswconfig.h */
#define CMD_DMA 0

#define SPECTRA_PARTITION_ID    0
/**** Block Table and Reserved Block Parameters *****/
#define SPECTRA_START_BLOCK     3
#define NUM_FREE_BLOCKS_GATE    30

/* KBV - Updated to LNW scratch register address */
#define SCRATCH_REG_ADDR    CONFIG_MTD_NAND_DENALI_SCRATCH_REG_ADDR
#define SCRATCH_REG_SIZE    64

#define GLOB_HWCTL_DEFAULT_BLKS    2048

#define SUPPORT_15BITECC        1
#define SUPPORT_8BITECC         1

#define CUSTOM_CONF_PARAMS      0

#define ONFI_BLOOM_TIME         1
#define MODE5_WORKAROUND        0

/* lld_nand.h */
/*
 * NAND Flash Controller Device Driver
 * Copyright (c) 2009, Intel Corporation and its suppliers.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef _LLD_NAND_
#define _LLD_NAND_

#define MODE_00    0x00000000
#define MODE_01    0x04000000
#define MODE_10    0x08000000
#define MODE_11    0x0C000000


#define DATA_TRANSFER_MODE              0
#define PROTECTION_PER_BLOCK            1
#define LOAD_WAIT_COUNT                 2
#define PROGRAM_WAIT_COUNT              3
#define ERASE_WAIT_COUNT                4
#define INT_MONITOR_CYCLE_COUNT         5
#define READ_BUSY_PIN_ENABLED           6
#define MULTIPLANE_OPERATION_SUPPORT    7
#define PRE_FETCH_MODE                  8
#define CE_DONT_CARE_SUPPORT            9
#define COPYBACK_SUPPORT                10
#define CACHE_WRITE_SUPPORT             11
#define CACHE_READ_SUPPORT              12
#define NUM_PAGES_IN_BLOCK              13
#define ECC_ENABLE_SELECT               14
#define WRITE_ENABLE_2_READ_ENABLE      15
#define ADDRESS_2_DATA                  16
#define READ_ENABLE_2_WRITE_ENABLE      17
#define TWO_ROW_ADDRESS_CYCLES          18
#define MULTIPLANE_ADDRESS_RESTRICT     19
#define ACC_CLOCKS                      20
#define READ_WRITE_ENABLE_LOW_COUNT     21
#define READ_WRITE_ENABLE_HIGH_COUNT    22

#define ECC_SECTOR_SIZE     512
#define LLD_MAX_FLASH_BANKS     4

#define DENALI_BUF_SIZE		(NAND_MAX_PAGESIZE + NAND_MAX_OOBSIZE)

struct nand_buf {
	int head;
	int tail;
	uint8_t buf[DENALI_BUF_SIZE];
	dma_addr_t dma_buf;
};

#define INTEL_CE4100	1
#define INTEL_MRST	2

struct denali_nand_info {
	struct mtd_info mtd;
	struct nand_chip nand;
	int flash_bank; /* currently selected chip */
	int status;
	int platform;
	struct nand_buf buf;
	struct pci_dev *dev;
	int total_used_banks;
	uint32_t block;  /* stored for future use */
	uint16_t page;
	void __iomem *flash_reg;  /* Mapped io reg base address */
	void __iomem *flash_mem;  /* Mapped io reg base address */

	/* elements used by ISR */
	struct completion complete;
	spinlock_t irq_lock;
	uint32_t irq_status;
	int irq_debug_array[32];
	int idx;

	uint32_t devnum;	/* represent how many nands connected */
	uint32_t fwblks; /* represent how many blocks FW used */
	uint32_t totalblks;
	uint32_t blksperchip;
	uint32_t bbtskipbytes;
};

#endif /*_LLD_NAND_*/
