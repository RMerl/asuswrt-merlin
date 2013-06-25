/*
 * Nortstar NAND controller driver
 * for Linux NAND library and MTD interface
 *
 * (c) Broadcom, Inc. 2012 All Rights Reserved.
 *
 * This module interfaces the NAND controller and hardware ECC capabilities
 * tp the generic NAND chip support in the NAND library.
 *
 * Notes:
 *	This driver depends on generic NAND driver, but works at the
 *	page level for operations.
 *
 *	When a page is written, the ECC calculated also protects the OOB
 *	bytes not taken by ECC, and so the OOB must be combined with any
 *	OOB data that preceded the page-write operation in order for the
 *	ECC to be calculated correctly.
 *	Also, when the page is erased, but OOB data is not, HW ECC will
 *	indicate an error, because it checks OOB too, which calls for some
 *	help from the software in this driver.
 *
 * TBD:
 *	Block locking/unlocking support, OTP support
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/bug.h>
#include <linux/err.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#ifdef	CONFIG_MTD_PARTITIONS
#include <linux/mtd/partitions.h>
#endif

#define	NANDC_MAX_CHIPS		2	/* Only 2 CSn supported in NorthStar */

#define	DRV_NAME	"iproc-nand"
#define	DRV_VERSION	"0.1"
#define	DRV_DESC	"Northstar on-chip NAND Flash Controller driver"

/*
 * RESOURCES
 */
static struct resource nandc_regs[] = {
	{
	.name	= "nandc_regs", .flags	= IORESOURCE_MEM,
	.start	= 0x18028000, .end	= 0x18028FFF,
	},
};

static struct resource nandc_idm_regs[] = {
	{
	.name	= "nandc_idm_regs", .flags= IORESOURCE_MEM,
	.start	= 0x1811a000, .end	= 0x1811afff,	
	},
};

static struct resource nandc_irqs[] = {
	{
	.name	= "nandc_irqs", .flags	= IORESOURCE_IRQ,
	.start	= 96,		 .end	= 103,
	},
};


/*
 * Driver private control structure
 */
struct nandc_ctrl {
	struct mtd_info		mtd;
	struct nand_chip	nand;
#ifdef	CONFIG_MTD_PARTITIONS
	struct mtd_partition	*parts;
#endif
	struct device		*device;

	struct	completion	op_completion;

	void * __iomem 		reg_base;
	void * __iomem 		idm_base;
	int			irq_base;
	struct nand_ecclayout	ecclayout;
	int			cmd_ret;	/* saved error code */
	unsigned char		oob_index,
				id_byte_index,
				chip_num,
				last_cmd,
				ecc_level,
				sector_size_shift,
				sec_per_page_shift;
};


/*
 * IRQ numbers - offset from first irq in nandc_irq resource
 */
#define	NANDC_IRQ_RD_MISS		0
#define	NANDC_IRQ_ERASE_COMPLETE	1
#define	NANDC_IRQ_COPYBACK_COMPLETE	2
#define	NANDC_IRQ_PROGRAM_COMPLETE	3
#define	NANDC_IRQ_CONTROLLER_RDY	4
#define	NANDC_IRQ_RDBSY_RDY		5
#define	NANDC_IRQ_ECC_UNCORRECTABLE	6
#define	NANDC_IRQ_ECC_CORRECTABLE	7
#define	NANDC_IRQ_NUM			8

/*
 * REGISTERS
 *
 * Individual bit-fields aof registers are specificed here
 * for clarity, and the rest of the code will access each field
 * as if it was its own register.
 *
 * Following registers are off <reg_base>:
 */
#define	REG_BIT_FIELD(r,p,w)	((reg_bit_field_t){(r),(p),(w)})

#define	NANDC_8KB_PAGE_SUPPORT		REG_BIT_FIELD(0x0, 31, 1)
#define	NANDC_REV_MAJOR			REG_BIT_FIELD(0x0, 8, 8)
#define	NANDC_REV_MINOR			REG_BIT_FIELD(0x0, 0, 8)

#define	NANDC_CMD_START_OPCODE		REG_BIT_FIELD(0x4, 24, 5)

#define	NANDC_CMD_CS_SEL		REG_BIT_FIELD(0x8, 16, 3)
#define	NANDC_CMD_EXT_ADDR		REG_BIT_FIELD(0x8, 0, 16)

#define	NANDC_CMD_ADDRESS		REG_BIT_FIELD(0xc, 0, 32)
#define	NANDC_CMD_END_ADDRESS		REG_BIT_FIELD(0x10, 0, 32)

#define	NANDC_INT_STATUS		REG_BIT_FIELD(0x14, 0, 32)
#define	NANDC_INT_STAT_CTLR_RDY		REG_BIT_FIELD(0x14, 31, 1)
#define	NANDC_INT_STAT_FLASH_RDY	REG_BIT_FIELD(0x14, 30, 1)
#define	NANDC_INT_STAT_CACHE_VALID	REG_BIT_FIELD(0x14, 29, 1)
#define	NANDC_INT_STAT_SPARE_VALID	REG_BIT_FIELD(0x14, 28, 1)
#define	NANDC_INT_STAT_ERASED		REG_BIT_FIELD(0x14, 27, 1)
#define	NANDC_INT_STAT_PLANE_RDY	REG_BIT_FIELD(0x14, 26, 1)
#define	NANDC_INT_STAT_FLASH_STATUS	REG_BIT_FIELD(0x14, 0, 8)

#define	NANDC_CS_LOCK			REG_BIT_FIELD(0x18, 31, 1)
#define	NANDC_CS_AUTO_CONFIG		REG_BIT_FIELD(0x18, 30, 1)
#define	NANDC_CS_NAND_WP		REG_BIT_FIELD(0x18, 29, 1)
#define	NANDC_CS_BLK0_WP		REG_BIT_FIELD(0x18, 28, 1)
#define	NANDC_CS_SW_USING_CS(n)		REG_BIT_FIELD(0x18, 8+(n), 1)
#define	NANDC_CS_MAP_SEL_CS(n)		REG_BIT_FIELD(0x18, 0+(n), 1)

#define	NANDC_XOR_ADDR_BLK0_ONLY	REG_BIT_FIELD(0x1c, 31, 1)
#define	NANDC_XOR_ADDR_CS(n)		REG_BIT_FIELD(0x1c, 0+(n), 1)

#define	NANDC_LL_OP_RET_IDLE		REG_BIT_FIELD(0x20, 31, 1)
#define	NANDC_LL_OP_CLE			REG_BIT_FIELD(0x20, 19, 1)
#define	NANDC_LL_OP_ALE			REG_BIT_FIELD(0x20, 18, 1)
#define	NANDC_LL_OP_WE			REG_BIT_FIELD(0x20, 17, 1)
#define	NANDC_LL_OP_RE			REG_BIT_FIELD(0x20, 16, 1)
#define	NANDC_LL_OP_DATA		REG_BIT_FIELD(0x20, 0, 16)

#define	NANDC_MPLANE_ADDR_EXT		REG_BIT_FIELD(0x24, 0, 16)
#define	NANDC_MPLANE_ADDR		REG_BIT_FIELD(0x28, 0, 32)

#define	NANDC_ACC_CTRL_CS(n)		REG_BIT_FIELD(0x50+((n)<<4), 0, 32)
#define	NANDC_ACC_CTRL_RD_ECC(n)	REG_BIT_FIELD(0x50+((n)<<4), 31, 1)
#define	NANDC_ACC_CTRL_WR_ECC(n)	REG_BIT_FIELD(0x50+((n)<<4), 30, 1)
#define	NANDC_ACC_CTRL_CE_CARE(n)	REG_BIT_FIELD(0x50+((n)<<4), 29, 1)
#define	NANDC_ACC_CTRL_PGM_RDIN(n)	REG_BIT_FIELD(0x50+((n)<<4), 28, 1)
#define	NANDC_ACC_CTRL_ERA_ECC_ERR(n)	REG_BIT_FIELD(0x50+((n)<<4), 27, 1)
#define	NANDC_ACC_CTRL_PGM_PARTIAL(n)	REG_BIT_FIELD(0x50+((n)<<4), 26, 1)
#define	NANDC_ACC_CTRL_WR_PREEMPT(n)	REG_BIT_FIELD(0x50+((n)<<4), 25, 1)
#define	NANDC_ACC_CTRL_PG_HIT(n)	REG_BIT_FIELD(0x50+((n)<<4), 24, 1)
#define	NANDC_ACC_CTRL_PREFETCH(n)	REG_BIT_FIELD(0x50+((n)<<4), 23, 1)
#define	NANDC_ACC_CTRL_CACHE_MODE(n)	REG_BIT_FIELD(0x50+((n)<<4), 22, 1)
#define	NANDC_ACC_CTRL_CACHE_LASTPG(n)	REG_BIT_FIELD(0x50+((n)<<4), 21, 1)
#define	NANDC_ACC_CTRL_ECC_LEVEL(n)	REG_BIT_FIELD(0x50+((n)<<4), 16, 5)
#define	NANDC_ACC_CTRL_SECTOR_1K(n)	REG_BIT_FIELD(0x50+((n)<<4), 7, 1)
#define	NANDC_ACC_CTRL_SPARE_SIZE(n)	REG_BIT_FIELD(0x50+((n)<<4), 0, 7)

#define	NANDC_CONFIG_CS(n)		REG_BIT_FIELD(0x54+((n)<<4), 0, 32)
#define	NANDC_CONFIG_LOCK(n)		REG_BIT_FIELD(0x54+((n)<<4), 31, 1)
#define	NANDC_CONFIG_BLK_SIZE(n)	REG_BIT_FIELD(0x54+((n)<<4), 28, 3)
#define	NANDC_CONFIG_CHIP_SIZE(n)	REG_BIT_FIELD(0x54+((n)<<4), 24, 4)
#define	NANDC_CONFIG_CHIP_WIDTH(n)	REG_BIT_FIELD(0x54+((n)<<4), 23, 1)
#define	NANDC_CONFIG_PAGE_SIZE(n)	REG_BIT_FIELD(0x54+((n)<<4), 20, 2)
#define	NANDC_CONFIG_FUL_ADDR_BYTES(n)	REG_BIT_FIELD(0x54+((n)<<4), 16, 3)
#define	NANDC_CONFIG_COL_ADDR_BYTES(n)	REG_BIT_FIELD(0x54+((n)<<4), 12, 3)
#define	NANDC_CONFIG_BLK_ADDR_BYTES(n)	REG_BIT_FIELD(0x54+((n)<<4), 8, 3)

#define	NANDC_TIMING_1_CS(n)		REG_BIT_FIELD(0x58+((n)<<4), 0, 32)
#define	NANDC_TIMING_2_CS(n)		REG_BIT_FIELD(0x5c+((n)<<4), 0, 32)
	/* Individual bits for Timing registers - TBD */

#define	NANDC_CORR_STAT_THRESH_CS(n)	REG_BIT_FIELD(0xc0, 6*(n), 6)

#define	NANDC_BLK_WP_END_ADDR		REG_BIT_FIELD(0xc8, 0, 32)

#define	NANDC_MPLANE_ERASE_CYC2_OPCODE	REG_BIT_FIELD(0xcc, 24, 8)
#define	NANDC_MPLANE_READ_STAT_OPCODE	REG_BIT_FIELD(0xcc, 16, 8)
#define	NANDC_MPLANE_PROG_ODD_OPCODE	REG_BIT_FIELD(0xcc, 8, 8)
#define	NANDC_MPLANE_PROG_TRL_OPCODE	REG_BIT_FIELD(0xcc, 0, 8)

#define	NANDC_MPLANE_PGCACHE_TRL_OPCODE	REG_BIT_FIELD(0xd0, 24, 8)
#define	NANDC_MPLANE_READ_STAT2_OPCODE	REG_BIT_FIELD(0xd0, 16, 8)
#define	NANDC_MPLANE_READ_EVEN_OPCODE	REG_BIT_FIELD(0xd0, 8, 8)
#define	NANDC_MPLANE_READ_ODD__OPCODE	REG_BIT_FIELD(0xd0, 0, 8)

#define	NANDC_MPLANE_CTRL_ERASE_CYC2_EN	REG_BIT_FIELD(0xd4, 31, 1)
#define	NANDC_MPLANE_CTRL_RD_ADDR_SIZE	REG_BIT_FIELD(0xd4, 30, 1)
#define	NANDC_MPLANE_CTRL_RD_CYC_ADDR	REG_BIT_FIELD(0xd4, 29, 1)
#define	NANDC_MPLANE_CTRL_RD_COL_ADDR	REG_BIT_FIELD(0xd4, 28, 1)

#define	NANDC_UNCORR_ERR_COUNT		REG_BIT_FIELD(0xfc, 0, 32)

#define	NANDC_CORR_ERR_COUNT		REG_BIT_FIELD(0x100, 0, 32)

#define	NANDC_READ_CORR_BIT_COUNT	REG_BIT_FIELD(0x104, 0, 32)

#define	NANDC_BLOCK_LOCK_STATUS		REG_BIT_FIELD(0x108, 0, 8)

#define	NANDC_ECC_CORR_ADDR_CS		REG_BIT_FIELD(0x10c, 16, 3)
#define	NANDC_ECC_CORR_ADDR_EXT		REG_BIT_FIELD(0x10c, 0, 16)

#define	NANDC_ECC_CORR_ADDR		REG_BIT_FIELD(0x110, 0, 32)

#define	NANDC_ECC_UNC_ADDR_CS		REG_BIT_FIELD(0x114, 16, 3)
#define	NANDC_ECC_UNC_ADDR_EXT		REG_BIT_FIELD(0x114, 0, 16)

#define	NANDC_ECC_UNC_ADDR		REG_BIT_FIELD(0x118, 0, 32)

#define	NANDC_READ_ADDR_CS		REG_BIT_FIELD(0x11c, 16, 3)
#define	NANDC_READ_ADDR_EXT		REG_BIT_FIELD(0x11c, 0, 16)
#define	NANDC_READ_ADDR			REG_BIT_FIELD(0x120, 0, 32)

#define	NANDC_PROG_ADDR_CS		REG_BIT_FIELD(0x124, 16, 3)
#define	NANDC_PROG_ADDR_EXT		REG_BIT_FIELD(0x124, 0, 16)
#define	NANDC_PROG_ADDR			REG_BIT_FIELD(0x128, 0, 32)

#define	NANDC_CPYBK_ADDR_CS		REG_BIT_FIELD(0x12c, 16, 3)
#define	NANDC_CPYBK_ADDR_EXT		REG_BIT_FIELD(0x12c, 0, 16)
#define	NANDC_CPYBK_ADDR		REG_BIT_FIELD(0x130, 0, 32)

#define	NANDC_ERASE_ADDR_CS		REG_BIT_FIELD(0x134, 16, 3)
#define	NANDC_ERASE_ADDR_EXT		REG_BIT_FIELD(0x134, 0, 16)
#define	NANDC_ERASE_ADDR		REG_BIT_FIELD(0x138, 0, 32)

#define	NANDC_INV_READ_ADDR_CS		REG_BIT_FIELD(0x13c, 16, 3)
#define	NANDC_INV_READ_ADDR_EXT		REG_BIT_FIELD(0x13c, 0, 16)
#define	NANDC_INV_READ_ADDR		REG_BIT_FIELD(0x140, 0, 32)

#define	NANDC_INIT_STAT			REG_BIT_FIELD(0x144, 0, 32)
#define	NANDC_INIT_ONFI_DONE		REG_BIT_FIELD(0x144, 31, 1)
#define	NANDC_INIT_DEVID_DONE		REG_BIT_FIELD(0x144, 30, 1)
#define	NANDC_INIT_SUCCESS		REG_BIT_FIELD(0x144, 29, 1)
#define	NANDC_INIT_FAIL			REG_BIT_FIELD(0x144, 28, 1)
#define	NANDC_INIT_BLANK		REG_BIT_FIELD(0x144, 27, 1)
#define	NANDC_INIT_TIMEOUT		REG_BIT_FIELD(0x144, 26, 1)
#define	NANDC_INIT_UNC_ERROR		REG_BIT_FIELD(0x144, 25, 1)
#define	NANDC_INIT_CORR_ERROR		REG_BIT_FIELD(0x144, 24, 1)
#define	NANDC_INIT_PARAM_RDY		REG_BIT_FIELD(0x144, 23, 1)
#define	NANDC_INIT_AUTH_FAIL		REG_BIT_FIELD(0x144, 22, 1)

#define	NANDC_ONFI_STAT			REG_BIT_FIELD(0x148, 0, 32)
#define	NANDC_ONFI_DEBUG		REG_BIT_FIELD(0x148, 28, 4)
#define	NANDC_ONFI_PRESENT		REG_BIT_FIELD(0x148, 27, 1)
#define	NANDC_ONFI_BADID_PG2		REG_BIT_FIELD(0x148, 5, 1)
#define	NANDC_ONFI_BADID_PG1		REG_BIT_FIELD(0x148, 4, 1)
#define	NANDC_ONFI_BADID_PG0		REG_BIT_FIELD(0x148, 3, 1)
#define	NANDC_ONFI_BADCRC_PG2		REG_BIT_FIELD(0x148, 2, 1)
#define	NANDC_ONFI_BADCRC_PG1		REG_BIT_FIELD(0x148, 1, 1)
#define	NANDC_ONFI_BADCRC_PG0		REG_BIT_FIELD(0x148, 0, 1)

#define	NANDC_ONFI_DEBUG_DATA		REG_BIT_FIELD(0x14c, 0, 32)

#define	NANDC_SEMAPHORE			REG_BIT_FIELD(0x150, 0, 8)

#define	NANDC_DEVID_BYTE(b)		REG_BIT_FIELD(0x194+((b)&0x4), \
						24-(((b)&3)<<3), 8)

#define	NANDC_LL_RDDATA			REG_BIT_FIELD(0x19c, 0, 16)

#define	NANDC_INT_N_REG(n)		REG_BIT_FIELD(0xf00|((n)<<2), 0, 1)
#define	NANDC_INT_DIREC_READ_MISS	REG_BIT_FIELD(0xf00, 0, 1)
#define	NANDC_INT_ERASE_DONE		REG_BIT_FIELD(0xf04, 0, 1)
#define	NANDC_INT_CPYBK_DONE		REG_BIT_FIELD(0xf08, 0, 1)
#define	NANDC_INT_PROGRAM_DONE		REG_BIT_FIELD(0xf0c, 0, 1)
#define	NANDC_INT_CONTROLLER_RDY	REG_BIT_FIELD(0xf10, 0, 1)
#define	NANDC_INT_RDBSY_RDY		REG_BIT_FIELD(0xf14, 0, 1)
#define	NANDC_INT_ECC_UNCORRECTABLE	REG_BIT_FIELD(0xf18, 0, 1)
#define	NANDC_INT_ECC_CORRECTABLE	REG_BIT_FIELD(0xf1c, 0, 1)

/*
 * Following  registers are treated as contigous IO memory, offset is from
 * <reg_base>, and the data is in big-endian byte order
 */
#define	NANDC_SPARE_AREA_READ_OFF	0x200
#define	NANDC_SPARE_AREA_WRITE_OFF	0x280
#define	NANDC_CACHE_OFF			0x400
#define	NANDC_CACHE_SIZE		(128*4)

/*
 * Following are IDM (a.k.a. Slave Wrapper) registers are off <idm_base>:
 */
#define	IDMREG_BIT_FIELD(r,p,w)	((idm_reg_bit_field_t){(r),(p),(w)})

#define	NANDC_IDM_AXI_BIG_ENDIAN	IDMREG_BIT_FIELD(0x408, 28, 1)
#define	NANDC_IDM_APB_LITTLE_ENDIAN	IDMREG_BIT_FIELD(0x408, 24, 1)
#define	NANDC_IDM_TM			IDMREG_BIT_FIELD(0x408, 16, 5)
#define	NANDC_IDM_IRQ_CORRECABLE_EN	IDMREG_BIT_FIELD(0x408, 9, 1)
#define	NANDC_IDM_IRQ_UNCORRECABLE_EN	IDMREG_BIT_FIELD(0x408, 8, 1)
#define	NANDC_IDM_IRQ_RDYBSY_RDY_EN	IDMREG_BIT_FIELD(0x408, 7, 1)
#define	NANDC_IDM_IRQ_CONTROLLER_RDY_EN	IDMREG_BIT_FIELD(0x408, 6, 1)
#define	NANDC_IDM_IRQ_PRPOGRAM_COMP_EN	IDMREG_BIT_FIELD(0x408, 5, 1)
#define	NANDC_IDM_IRQ_COPYBK_COMP_EN	IDMREG_BIT_FIELD(0x408, 4, 1)
#define	NANDC_IDM_IRQ_ERASE_COMP_EN	IDMREG_BIT_FIELD(0x408, 3, 1)
#define	NANDC_IDM_IRQ_READ_MISS_EN	IDMREG_BIT_FIELD(0x408, 2, 1)
#define	NANDC_IDM_IRQ_N_EN(n)		IDMREG_BIT_FIELD(0x408, 2+(n), 1)

#define	NANDC_IDM_CLOCK_EN		IDMREG_BIT_FIELD(0x408, 0, 1)

#define	NANDC_IDM_IO_ECC_CORR		IDMREG_BIT_FIELD(0x500, 3, 1)
#define	NANDC_IDM_IO_ECC_UNCORR		IDMREG_BIT_FIELD(0x500, 2, 1)
#define	NANDC_IDM_IO_RDYBSY		IDMREG_BIT_FIELD(0x500, 1, 1)
#define	NANDC_IDM_IO_CTRL_RDY		IDMREG_BIT_FIELD(0x500, 0, 1)

#define	NANDC_IDM_RESET			IDMREG_BIT_FIELD(0x800, 0, 1)
	/* Remaining IDM registers do not seem to be useful, skipped */

/*
 * NAND Controller has its own command opcodes
 * different from opcodes sent to the actual flash chip
 */
#define	NANDC_CMD_OPCODE_NULL		0
#define	NANDC_CMD_OPCODE_PAGE_READ	1
#define	NANDC_CMD_OPCODE_SPARE_READ	2
#define	NANDC_CMD_OPCODE_STATUS_READ	3
#define	NANDC_CMD_OPCODE_PAGE_PROG	4
#define	NANDC_CMD_OPCODE_SPARE_PROG	5
#define	NANDC_CMD_OPCODE_DEVID_READ	7
#define	NANDC_CMD_OPCODE_BLOCK_ERASE	8
#define	NANDC_CMD_OPCODE_FLASH_RESET	9

/*
 * Forward declarations
 */
static void nandc_cmdfunc( struct mtd_info *mtd,
		unsigned command,
		int column,
		int page_addr);

/*
 * NAND Controller hardware ECC data size
 *
 * The following table contains the number of bytes needed for
 * each of the ECC levels, per "sector", which is either 512 or 1024 bytes.
 * The actual layout is as follows:
 * The entire spare area is equally divided into as many sections as there
 * are sectors per page, and the ECC data is located at the end of each
 * of these sections.
 * For example, given a 2K per page and 64 bytes spare device, configured for
 * sector size 1k and ECC level of 4, the spare area will be divided into 2
 * sections 32 bytes each, and the last 14 bytes of 32 in each section will
 * be filled with ECC data.
 * Note: the name of the algorythm and the number of error bits it can correct
 * is of no consequence to this driver, therefore omitted.
 */
static const struct nandc_ecc_size_s {
	unsigned char
		sector_size_shift,
		ecc_level,
		ecc_bytes_per_sec,
		reserved	;
} nandc_ecc_sizes [] = {
	{ 9,	0, 	0 },
	{ 10,	0, 	0 },
	{ 9,	1, 	2 },
	{ 10,	1, 	4 },
	{ 9,	2, 	4 },
	{ 10,	2, 	7 },
	{ 9,	3, 	6 },
	{ 10,	3, 	11 },
	{ 9,	4, 	7 },
	{ 10,	4, 	14 },
	{ 9,	5, 	9 },
	{ 10,	5, 	18 },
	{ 9,	6, 	11 },
	{ 10,	6, 	21 },
	{ 9,	7, 	13 },
	{ 10,	7, 	25 },
	{ 9,	8, 	14 },
	{ 10,	8, 	28 },

	{ 9,	9, 	16 },
	{ 9,	10, 	18 },
	{ 9,	11, 	20 },
	{ 9,	12, 	21 },

	{ 10,	9, 	32 },
	{ 10,	10, 	35 },
	{ 10,	11, 	39 },
	{ 10,	12, 	42 },
};

/*
 * INTERNAL - Populate the various fields that depend on how
 * the hardware ECC data is located in the spare area
 *
 * For this controiller, it is easier to fill-in these
 * structures at run time.
 *
 * The bad-block marker is assumed to occupy one byte
 * at chip->badblockpos, which must be in the first
 * sector of the spare area, namely it is either 
 * at offset 0 or 5.
 * Some chips use both for manufacturer's bad block
 * markers, but we ingore that issue here, and assume only
 * one byte is used as bad-block marker always.
 */
static int nandc_hw_ecc_layout( struct nandc_ctrl * ctrl )
{
	struct nand_ecclayout * layout ;
	unsigned i, j, k;
	unsigned ecc_per_sec, oob_per_sec ;
	unsigned bbm_pos = ctrl->nand.badblockpos;

	DEBUG(MTD_DEBUG_LEVEL1, "%s: ecc_level %d\n",
		__func__, ctrl->ecc_level);

	/* Caclculate spare area per sector size */
	oob_per_sec = ctrl->mtd.oobsize >> ctrl->sec_per_page_shift ;

	/* Try to calculate the amount of ECC bytes per sector with a formula */
	if( ctrl->sector_size_shift == 9 )
		ecc_per_sec = ((ctrl->ecc_level * 14) + 7) >> 3 ;
	else if( ctrl->sector_size_shift == 10 )
		ecc_per_sec = ((ctrl->ecc_level * 14) + 3) >> 2 ;
	else
		ecc_per_sec = oob_per_sec + 1 ;	/* cause an error if not in table */

	DEBUG(MTD_DEBUG_LEVEL1, "%s: calc eccbytes %d\n",
		__func__, ecc_per_sec );

	/* Now find out the answer according to the table */
	for(i = 0; i < ARRAY_SIZE(nandc_ecc_sizes); i++ ) {
		if( nandc_ecc_sizes[i].ecc_level == ctrl->ecc_level &&
		    nandc_ecc_sizes[i].sector_size_shift ==
				ctrl->sector_size_shift ) {
			DEBUG(MTD_DEBUG_LEVEL1, "%s: table eccbytes %d\n",
				__func__, 
				nandc_ecc_sizes[i].ecc_bytes_per_sec );
			break;
			}
		}

	/* Table match overrides formula */
	if( nandc_ecc_sizes[i].ecc_level == ctrl->ecc_level &&
	    nandc_ecc_sizes[i].sector_size_shift == ctrl->sector_size_shift )
		ecc_per_sec = nandc_ecc_sizes[i].ecc_bytes_per_sec ;

	/* Return an error if calculated ECC leaves no room for OOB */
	if( (ctrl->sec_per_page_shift != 0 && ecc_per_sec >= oob_per_sec) ||
	    (ctrl->sec_per_page_shift == 0 && ecc_per_sec >= (oob_per_sec-1))){
		DEBUG(MTD_DEBUG_LEVEL0, "%s: ERROR: ECC level %d too high, "
			"leaves no room for OOB data\n",
			__func__, ctrl->ecc_level );
		return -EINVAL ;
		}

	/* Fill in the needed fields */
	ctrl->nand.ecc.size = ctrl->mtd.writesize >> ctrl->sec_per_page_shift;
	ctrl->nand.ecc.bytes = ecc_per_sec ;
	ctrl->nand.ecc.steps = 1 << ctrl->sec_per_page_shift ;
	ctrl->nand.ecc.total = ecc_per_sec << ctrl->sec_per_page_shift ;

	/* Build an ecc layout data structure */
	layout = & ctrl->ecclayout ;
	memset( layout, 0, sizeof( * layout ));

	/* Total number of bytes used by HW ECC */
	layout->eccbytes = ecc_per_sec << ctrl->sec_per_page_shift ;

	/* Location for each of the HW ECC bytes */
	for(i = j = 0, k = 1 ;
		i < ARRAY_SIZE(layout->eccpos) && i < layout->eccbytes;
		i++, j++ ) {
		/* switch sector # */
		if( j == ecc_per_sec ) {
			j = 0;
			k ++;
			}
		/* save position of each HW-generated ECC byte */
		layout->eccpos[i] = (oob_per_sec * k) - ecc_per_sec + j;

		/* Check that HW ECC does not overlap bad-block marker */
		if( bbm_pos == layout->eccpos[i] ) {
			DEBUG(MTD_DEBUG_LEVEL0, "%s: ERROR: ECC level %d too high, "
			"HW ECC collides with bad-block marker position\n",
			__func__, ctrl->ecc_level );

			return -EINVAL;
			}
		}

	/* Location of all user-available OOB byte-ranges */
	for( i = 0; i < ARRAY_SIZE(layout->oobfree); i++ ) {
		if( i >= (1 << ctrl->sec_per_page_shift ))
			break ;
		layout->oobfree[i].offset = oob_per_sec * i ;
		layout->oobfree[i].length = oob_per_sec - ecc_per_sec ;


		/* Bad-block marker must be in the first sector spare area */
		BUG_ON( bbm_pos >=
			(layout->oobfree[i].offset+layout->oobfree[i].length));
		if( i != 0 )
			continue;

		/* Remove bad-block marker from available byte range */
		if( bbm_pos == layout->oobfree[i].offset ) {
			layout->oobfree[i].offset += 1 ;
			layout->oobfree[i].length -= 1 ;
			}
		else if( bbm_pos ==
		  (layout->oobfree[i].offset+layout->oobfree[i].length-1)){
			layout->oobfree[i].length -= 1 ;
			}
		else {
			layout->oobfree[i+1].offset = bbm_pos + 1 ;
			layout->oobfree[i+1].length =
				layout->oobfree[i].length - bbm_pos - 1;
			layout->oobfree[i].length = bbm_pos ;
			i ++ ;
			}
		}

	layout->oobavail = ((oob_per_sec - ecc_per_sec)
		<< ctrl->sec_per_page_shift) - 1 ;

	ctrl->mtd.oobavail = layout->oobavail ;
	ctrl->nand.ecclayout = layout ;
	ctrl->nand.ecc.layout = layout ;

	/* Output layout for debugging */
	printk("Spare area=%d eccbytes %d, ecc bytes located at:\n",
		ctrl->mtd.oobsize, layout->eccbytes );
	for(i = j = 0; i<ARRAY_SIZE(layout->eccpos)&&i<layout->eccbytes; i++ ) {
		printk(" %d", layout->eccpos[i]);
		}
	printk("\nAvailable %d bytes at (off,len):\n", layout->oobavail );
	for(i = 0; i < ARRAY_SIZE(layout->oobfree); i++ ) {
		printk("(%d,%d) ",
			layout->oobfree[i].offset,
			layout->oobfree[i].length );
		}
	printk("\n");

	return 0 ;
}

/*
 * Register bit-field manipulation routines
 * NOTE: These compile to just a few machine instructions in-line
 */

typedef	struct {unsigned reg, pos, width;} reg_bit_field_t ;

static unsigned inline _reg_read( struct nandc_ctrl *ctrl, reg_bit_field_t rbf )
{
	void * __iomem base = (void *) ctrl->reg_base;
	unsigned val ;

	val =  __raw_readl( base + rbf.reg);
	val >>= rbf.pos;
	val &= (1 << rbf.width)-1;

	return val;
}

static void inline _reg_write( struct nandc_ctrl *ctrl,  
	reg_bit_field_t rbf, unsigned newval )
{
	void * __iomem base = (void *) ctrl->reg_base;
	unsigned val, msk ;

	msk = (1 << rbf.width)-1;
	msk <<= rbf.pos;
	newval <<= rbf.pos;
	newval &= msk ;

	val =  __raw_readl( base + rbf.reg);
	val &= ~msk ;
	val |= newval ;
	__raw_writel( val, base + rbf.reg);
}


typedef	struct {unsigned reg, pos, width;} idm_reg_bit_field_t ;

static unsigned inline _idm_reg_read( struct nandc_ctrl *ctrl,
	idm_reg_bit_field_t rbf )
{
	void * __iomem base = (void *) ctrl->idm_base;
	unsigned val ;

	val =  __raw_readl( base + rbf.reg);
	val >>= rbf.pos;
	val &= (1 << rbf.width)-1;

	return val;
}

static void inline _idm_reg_write( struct nandc_ctrl *ctrl,  
	idm_reg_bit_field_t rbf, unsigned newval )
{
	void * __iomem base = (void *) ctrl->idm_base;
	unsigned val, msk ;

	msk = (1 << rbf.width)-1;
	msk <<= rbf.pos;
	newval <<= rbf.pos;
	newval &= msk ;

	val =  __raw_readl( base + rbf.reg);
	val &= ~msk ;
	val |= newval ;
	__raw_writel( val, base + rbf.reg);
}

/*
 * INTERNAL - print NAND chip options
 * 
 * Useful for debugging
 */
static void nandc_options_print( unsigned opts )
{
	unsigned bit;
	const char * n ;

	printk("Options: ");
	for(bit = 0; bit < 32; bit ++ ) {
		if( 0 == (opts & (1<<bit)))
			continue;
		switch(1<<bit){
			default:
				printk("OPT_%x",1<<bit);
				n = NULL;
				break;
			case NAND_NO_AUTOINCR:
				n = "NO_AUTOINCR"; break;
			case NAND_BUSWIDTH_16:
				n = "BUSWIDTH_16"; break;
			case NAND_NO_PADDING:
				n = "NO_PADDING"; break;
			case NAND_CACHEPRG:
				n = "CACHEPRG"; break;
			case NAND_COPYBACK:
				n = "COPYBACK"; break;
			case NAND_IS_AND:
				n = "IS_AND"; break;
			case NAND_4PAGE_ARRAY:
				n = "4PAGE_ARRAY"; break;
			case BBT_AUTO_REFRESH:
				n = "AUTO_REFRESH"; break;
			case NAND_NO_READRDY:
				n = "NO_READRDY"; break;
			case NAND_NO_SUBPAGE_WRITE:
				n = "NO_SUBPAGE_WRITE"; break;
			case NAND_BROKEN_XD:
				n = "BROKEN_XD"; break;
			case NAND_ROM:
				n = "ROM"; break;
			case NAND_USE_FLASH_BBT:
				n = "USE_FLASH_BBT"; break;
			case NAND_SKIP_BBTSCAN:
				n = "SKIP_BBTSCAN"; break;
			case NAND_OWN_BUFFERS:
				n = "OWN_BUFFERS"; break;
			case NAND_SCAN_SILENT_NODEV:
				n = "SCAN_SILENT_NODEV"; break;
			case NAND_CONTROLLER_ALLOC:
				n = "SCAN_SILENT_NODEV"; break;
			case NAND_BBT_SCAN2NDPAGE:
				n = "BBT_SCAN2NDPAGE"; break;
			case NAND_BBT_SCANBYTE1AND6:
				n = "BBT_SCANBYTE1AND6"; break;
			case NAND_BBT_SCANLASTPAGE:
				n = "BBT_SCANLASTPAGE"; break;
		}
		printk("%s,",n);
	}
	printk("\n");
}

/*
 * NAND Interface - dev_ready
 *
 * Return 1 iff device is ready, 0 otherwise
 */
static int nandc_dev_ready( struct mtd_info * mtd)
{
	struct nand_chip * chip = mtd->priv ;
	struct nandc_ctrl * ctrl = chip->priv ;
	int rdy;

	rdy = _idm_reg_read( ctrl, NANDC_IDM_IO_CTRL_RDY );
	DEBUG(MTD_DEBUG_LEVEL1, "%s: %d\n", __func__, rdy);

	return rdy ;
}

/*
 * Interrupt service routines
 */
static irqreturn_t nandc_isr(int irq, void *dev_id)
{
        struct nandc_ctrl *ctrl = dev_id;
	int irq_off;

	irq_off = irq - ctrl->irq_base ;
	BUG_ON( irq_off < 0 || irq_off >= NANDC_IRQ_NUM );

	DEBUG(MTD_DEBUG_LEVEL3, "%s:start irqoff=%d irq_reg=%x en=%d cmd=%#x\n",
		__func__, irq_off,
		_reg_read( ctrl, NANDC_INT_N_REG(irq_off)),
		_idm_reg_read(ctrl, NANDC_IDM_IRQ_N_EN(irq_off)),
		ctrl->last_cmd
		);

	if( ! _reg_read( ctrl, NANDC_INT_N_REG(irq_off)))
			return IRQ_NONE;

	/* Acknowledge interrupt */
	_reg_write( ctrl, NANDC_INT_N_REG(irq_off), 1 );

	/* Wake up task */
        complete(&ctrl->op_completion);

        return IRQ_HANDLED;
}

static int nandc_wait_interrupt( struct nandc_ctrl * ctrl,
	unsigned irq_off, unsigned timeout_usec )
{
	long timeout_jiffies ;
	int ret = 0 ;

	INIT_COMPLETION(ctrl->op_completion);

	/* Acknowledge interrupt */
	_reg_write( ctrl, NANDC_INT_N_REG(irq_off), 1 );

	/* Enable IRQ to wait on */
	_idm_reg_write(ctrl, NANDC_IDM_IRQ_N_EN(irq_off),1);

	timeout_jiffies = 1 + usecs_to_jiffies( timeout_usec );

	if( irq_off != NANDC_IRQ_CONTROLLER_RDY || 
		0 == _idm_reg_read( ctrl, NANDC_IDM_IO_CTRL_RDY)) {

		DEBUG(MTD_DEBUG_LEVEL3, "%s: wait start to=%ld\n", __func__,
			timeout_jiffies);

		timeout_jiffies = wait_for_completion_interruptible_timeout(
			&ctrl->op_completion, timeout_jiffies );

		DEBUG(MTD_DEBUG_LEVEL3, "%s: wait done to=%ld\n", __func__,
			timeout_jiffies);

		if( timeout_jiffies < 0 )
			ret =  timeout_jiffies;
		if( timeout_jiffies == 0 )
			ret = -ETIME;
		}

	/* Disable IRQ, we're done waiting */
	_idm_reg_write(ctrl, NANDC_IDM_IRQ_N_EN(irq_off),0);

	if( _idm_reg_read( ctrl, NANDC_IDM_IO_CTRL_RDY ) )
		ret = 0;

	if( ret < 0  )
		DEBUG(MTD_DEBUG_LEVEL0, "%s: to=%d, timeout!\n",
			__func__, timeout_usec);

	return ret ;
}

/*
 * INTERNAL - wait for command completion
 */
static int nandc_wait_cmd( struct nandc_ctrl * ctrl, unsigned timeout_usec )
{
	unsigned retries;

	if( _reg_read( ctrl, NANDC_INT_STAT_CTLR_RDY ))
		return 0;

	/* If the timeout is long, wait for interrupt */
	if( timeout_usec >= jiffies_to_usecs(1) >> 4 )
		return nandc_wait_interrupt(
			ctrl, NANDC_IRQ_CONTROLLER_RDY, timeout_usec );

	/* Wait for completion of the prior command */
	retries = (timeout_usec >> 3) + 1 ;

	while( retries -- &&
		0 == _reg_read( ctrl, NANDC_INT_STAT_CTLR_RDY )) {
		cpu_relax();
		udelay(6);
		}

	if(retries == 0 )
		{
		DEBUG(MTD_DEBUG_LEVEL0, "%s: to=%d, timeout!\n",
			__func__, timeout_usec);
		return -ETIME ;
		}
	return 0;
}


/*
 * NAND Interface - waitfunc
 */
static int nandc_waitfunc( struct mtd_info *mtd, struct nand_chip *chip )
{
	struct nandc_ctrl * ctrl = chip->priv ;
	unsigned to;
	int ret ;

	/* figure out timeout based on what command is on */
	switch( ctrl->last_cmd ) {
		default:
		case NAND_CMD_ERASE1:
		case NAND_CMD_ERASE2:	to = 1 << 16;	break;
		case NAND_CMD_STATUS:
		case NAND_CMD_RESET:	to = 256;	break;
		case NAND_CMD_READID:	to = 1024;	break;
		case NAND_CMD_READ1:
		case NAND_CMD_READ0:	to = 2048;	break;
		case NAND_CMD_PAGEPROG:	to = 4096;	break;
		case NAND_CMD_READOOB:	to = 512;	break;
		}
	
	/* deliver deferred error code if any */
	if( (ret = ctrl->cmd_ret) < 0 ) {
		ctrl->cmd_ret = 0;
		}
	else {
		ret = nandc_wait_cmd( ctrl, to) ;
		}

	/* Timeout */
	if( ret < 0 ) {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: timeout\n", __func__ );
		return NAND_STATUS_FAIL ;
		}

	ret = _reg_read(ctrl, NANDC_INT_STAT_FLASH_STATUS);

	DEBUG(MTD_DEBUG_LEVEL3, "%s: status=%#x\n", __func__, ret);

	return ret;
}

/*
 * NAND Interface - read_oob
 */
static int nandc_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
                int page, int sndcmd)
{
	struct nandc_ctrl * ctrl = chip->priv ;
	unsigned n = ctrl->chip_num ;
	void * __iomem ctrl_spare ;
	unsigned spare_per_sec, sector ;
	u64 nand_addr;

	DEBUG(MTD_DEBUG_LEVEL3, "%s: page=%#x\n", __func__, page);

	ctrl_spare = ctrl->reg_base + NANDC_SPARE_AREA_READ_OFF;

	/* Set the page address for the following commands */
	nand_addr = ((u64)page << chip->page_shift);
	_reg_write( ctrl, NANDC_CMD_EXT_ADDR, nand_addr >> 32 );

	spare_per_sec = mtd->oobsize >> ctrl->sec_per_page_shift;

	/* Disable ECC validation for spare area reads */
	_reg_write( ctrl, NANDC_ACC_CTRL_RD_ECC(n), 0 );

	/* Loop all sectors in page */
	for( sector = 0 ; sector < (1<<ctrl->sec_per_page_shift); sector ++ ) {
		unsigned col ;

		col = (sector << ctrl->sector_size_shift);

		/* Issue command to read partial page */
		_reg_write( ctrl, NANDC_CMD_ADDRESS, nand_addr + col);

		_reg_write(ctrl, NANDC_CMD_START_OPCODE,
				NANDC_CMD_OPCODE_SPARE_READ);

		/* Wait for the command to complete */
		if( nandc_wait_cmd(ctrl, (sector==0)? 10000: 100) )
			return -EIO;

		if( !_reg_read( ctrl, NANDC_INT_STAT_SPARE_VALID ) ) {
			DEBUG(MTD_DEBUG_LEVEL0, "%s: data not valid\n",	
				__func__);
			return -EIO;
			}

		/* Set controller to Little Endian mode for copying */
		_idm_reg_write( ctrl, NANDC_IDM_APB_LITTLE_ENDIAN, 1);

		memcpy( chip->oob_poi + sector * spare_per_sec,
			ctrl_spare,
			spare_per_sec );

		/* Return to Big Endian mode for commands etc */
		_idm_reg_write( ctrl, NANDC_IDM_APB_LITTLE_ENDIAN, 0);
		} /* for */

	return 0;
}

/*
 * NAND Interface - write_oob
 */
static int nandc_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
                int page )
{
	struct nandc_ctrl * ctrl = chip->priv ;
	unsigned n = ctrl->chip_num ;
	void * __iomem ctrl_spare ;
	unsigned spare_per_sec, sector, num_sec;
	u64 nand_addr;
	int to, status = 0;
	
	DEBUG(MTD_DEBUG_LEVEL3, "%s: page=%#x\n", __func__, page);

	ctrl_spare = ctrl->reg_base + NANDC_SPARE_AREA_WRITE_OFF;

	/* Disable ECC generation for spare area writes */
	_reg_write( ctrl, NANDC_ACC_CTRL_WR_ECC(n), 0 );

	spare_per_sec = mtd->oobsize >> ctrl->sec_per_page_shift;

	/* Set the page address for the following commands */
	nand_addr = ((u64)page << chip->page_shift);
	_reg_write( ctrl, NANDC_CMD_EXT_ADDR, nand_addr >> 32 );

	/* Must allow partial programming to change spare area only */
	_reg_write( ctrl, NANDC_ACC_CTRL_PGM_PARTIAL(n), 1 );

	num_sec = 1 << ctrl->sec_per_page_shift;
	/* Loop all sectors in page */
	for( sector = 0 ; sector < num_sec; sector ++ ) {
		unsigned col ;

		/* Spare area accessed by the data sector offset */
		col = (sector << ctrl->sector_size_shift);

		_reg_write( ctrl, NANDC_CMD_ADDRESS, nand_addr + col);

		/* Set controller to Little Endian mode for copying */
		_idm_reg_write( ctrl, NANDC_IDM_APB_LITTLE_ENDIAN, 1);

		memcpy( ctrl_spare,
			chip->oob_poi + sector * spare_per_sec,
			spare_per_sec );

		/* Return to Big Endian mode for commands etc */
		_idm_reg_write( ctrl, NANDC_IDM_APB_LITTLE_ENDIAN, 0);

		/* Push spare bytes into internal buffer, last goes to flash */
		_reg_write(ctrl, NANDC_CMD_START_OPCODE,
				NANDC_CMD_OPCODE_SPARE_PROG);

		if(sector == (num_sec-1))
			to = 1 << 16;
		else
			to = 1 << 10;

		if( nandc_wait_cmd(ctrl, to ) )
			return -EIO;
		} /* for */

	/* Restore partial programming inhibition */
	_reg_write( ctrl, NANDC_ACC_CTRL_PGM_PARTIAL(n), 0 );

	status = nandc_waitfunc(mtd, chip);
        return status & NAND_STATUS_FAIL ? -EIO : 0;
}

/*
 * INTERNAL - verify that a buffer is all erased
 */
static bool _nandc_buf_erased( const void * buf, unsigned len )
{
	unsigned i;
	const u32 * p = buf ;

	for(i=0; i < (len >> 2) ; i++ ) {
		if( p[i] != 0xffffffff )
			return false;
		}
	return true;
}

/*
 * INTERNAL - read a page, with or without ECC checking
 */
static int _nandc_read_page_do(struct mtd_info *mtd, struct nand_chip *chip,
                uint8_t *buf, int page, bool ecc)
{
	struct nandc_ctrl * ctrl = chip->priv ;
	unsigned n = ctrl->chip_num ;
	unsigned cache_reg_size = NANDC_CACHE_SIZE;
	void * __iomem ctrl_cache ;
	void * __iomem ctrl_spare ;
	unsigned data_bytes ;
	unsigned spare_per_sec ;
	unsigned sector, to = 1 << 16 ;
	u32 err_soft_reg, err_hard_reg;
	unsigned hard_err_count = 0;
	int ret;
	u64 nand_addr ;

	DEBUG(MTD_DEBUG_LEVEL3, "%s: page=%#x\n", __func__, page);

	ctrl_cache = ctrl->reg_base + NANDC_CACHE_OFF;
	ctrl_spare = ctrl->reg_base + NANDC_SPARE_AREA_READ_OFF;

	/* Reset  ECC error stats */
	err_hard_reg = _reg_read(ctrl, NANDC_UNCORR_ERR_COUNT);
	err_soft_reg = _reg_read(ctrl, NANDC_READ_CORR_BIT_COUNT);

	spare_per_sec = mtd->oobsize >> ctrl->sec_per_page_shift;

	/* Set the page address for the following commands */
	nand_addr = ((u64)page << chip->page_shift);
	_reg_write( ctrl, NANDC_CMD_EXT_ADDR, nand_addr >> 32 );

	/* Enable ECC validation for ecc page reads */
	_reg_write( ctrl, NANDC_ACC_CTRL_RD_ECC(n), ecc );

	/* Loop all sectors in page */
	for( sector = 0 ; sector < (1<<ctrl->sec_per_page_shift); sector ++ ) {
		data_bytes  = 0;

		/* Copy partial sectors sized by cache reg */
		while( data_bytes < (1<<ctrl->sector_size_shift)) {
			unsigned col ;

			col = data_bytes +
				(sector << ctrl->sector_size_shift);

			_reg_write( ctrl, NANDC_CMD_ADDRESS, nand_addr + col);

			/* Issue command to read partial page */
			_reg_write(ctrl, NANDC_CMD_START_OPCODE,
				NANDC_CMD_OPCODE_PAGE_READ);

			/* Wait for the command to complete */
			if((ret = nandc_wait_cmd(ctrl, to)) < 0 )
				return ret;
	    
			/* Set controller to Little Endian mode for copying */
			_idm_reg_write( ctrl, NANDC_IDM_APB_LITTLE_ENDIAN, 1);

			if(data_bytes == 0 ) {
				memcpy( 
					chip->oob_poi + sector * spare_per_sec,
					ctrl_spare,
					spare_per_sec );
				}

			memcpy(  buf+col, ctrl_cache, cache_reg_size);
			data_bytes += cache_reg_size ;

			/* Return to Big Endian mode for commands etc */
			_idm_reg_write( ctrl, NANDC_IDM_APB_LITTLE_ENDIAN, 0);

			/* Next iterations should go fast */
			to = 1 << 10;

			/* capture hard errors for each partial */
			if( err_hard_reg !=
				_reg_read(ctrl, NANDC_UNCORR_ERR_COUNT)){
			    int era = _reg_read(ctrl,NANDC_INT_STAT_ERASED);
			    if( (!era) &&
			       (!_nandc_buf_erased(buf+col, cache_reg_size)))
				hard_err_count ++ ;
			
			    err_hard_reg =
					_reg_read(ctrl, NANDC_UNCORR_ERR_COUNT);
			    }
			} /* while FlashCache buffer */
		} /* for sector */

	if( ! ecc )
		return 0;

	/* Report hard ECC errors */
	if( hard_err_count ) {
		mtd->ecc_stats.failed ++;
		}

	/* Get ECC soft error stats */
	mtd->ecc_stats.corrected += err_soft_reg - 
			_reg_read(ctrl, NANDC_READ_CORR_BIT_COUNT);

	DEBUG(MTD_DEBUG_LEVEL3, "%s: page=%#x err hard %d soft %d\n",
		__func__, page,
		mtd->ecc_stats.failed, mtd->ecc_stats.corrected);

	return 0;
}

/*
 * NAND Interface - read_page_ecc
 */
static int nandc_read_page_ecc(struct mtd_info *mtd, struct nand_chip *chip,
                uint8_t *buf, int page)
{
	return _nandc_read_page_do( mtd, chip, buf, page, true );
}

/*
 * NAND Interface - read_page_raw
 */
static int nandc_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
                uint8_t *buf, int page)
{
	return _nandc_read_page_do( mtd, chip, buf, page, true );
}

/*
 * INTERNAL - do page write, with or without ECC generation enabled
 */
static void _nandc_write_page_do( struct mtd_info *mtd, struct nand_chip *chip,
                const uint8_t *buf, bool ecc)
{
	struct nandc_ctrl * ctrl = chip->priv ;
	const unsigned cache_reg_size = NANDC_CACHE_SIZE;
	unsigned n = ctrl->chip_num ;
	void * __iomem ctrl_cache ;
	void * __iomem ctrl_spare ;
	unsigned spare_per_sec, sector, num_sec ;
	unsigned data_bytes, spare_bytes ;
	int i, ret, to ;
	uint8_t tmp_poi[ NAND_MAX_OOBSIZE ];
	u32  nand_addr;
	
	ctrl_cache = ctrl->reg_base + NANDC_CACHE_OFF;
	ctrl_spare = ctrl->reg_base + NANDC_SPARE_AREA_WRITE_OFF;

	/* Get start-of-page address */
	nand_addr = _reg_read( ctrl, NANDC_CMD_ADDRESS );

	DEBUG(MTD_DEBUG_LEVEL3, "%s: page=%#x\n", __func__,
		nand_addr >> chip->page_shift );

	BUG_ON( mtd->oobsize > sizeof(tmp_poi));

	/* Retreive pre-existing OOB values */
	memcpy( tmp_poi, chip->oob_poi, mtd->oobsize );
	ret = nandc_read_oob( mtd, chip, nand_addr >> chip->page_shift, 1 );
	if( (ctrl->cmd_ret = ret) < 0 )
		return ;

	/* Apply new OOB data bytes just like they would end up on the chip */
	for(i = 0; i < mtd->oobsize; i ++ )
		chip->oob_poi[i] &= tmp_poi[i];

	spare_per_sec = mtd->oobsize >> ctrl->sec_per_page_shift;

	/* Enable ECC generation for ecc page write, if requested */
	_reg_write( ctrl, NANDC_ACC_CTRL_WR_ECC(n), ecc );

	spare_bytes = 0;
	num_sec = 1 << ctrl->sec_per_page_shift ;

	/* Loop all sectors in page */
	for( sector = 0 ; sector < num_sec; sector ++ ) {

		data_bytes  = 0;

		/* Copy partial sectors sized by cache reg */
		while( data_bytes < (1<<ctrl->sector_size_shift)) {
			unsigned col ;

			col = data_bytes +
				(sector << ctrl->sector_size_shift);

			/* Set address of 512-byte sub-page */
			_reg_write( ctrl, NANDC_CMD_ADDRESS, nand_addr + col );

			/* Set controller to Little Endian mode for copying */
			_idm_reg_write( ctrl, NANDC_IDM_APB_LITTLE_ENDIAN, 1);

			/* Set spare area is written at each sector start */
			if(data_bytes == 0 ) {
				memcpy( ctrl_spare,
					chip->oob_poi + spare_bytes,
					spare_per_sec );
				spare_bytes += spare_per_sec ;
				}

			/* Copy sub-page data */
			memcpy(  ctrl_cache, buf+col, cache_reg_size);
			data_bytes += cache_reg_size ;

			/* Return to Big Endian mode for commands etc */
			_idm_reg_write( ctrl, NANDC_IDM_APB_LITTLE_ENDIAN, 0);

			/* Push data into internal cache */
			_reg_write(ctrl, NANDC_CMD_START_OPCODE,
				NANDC_CMD_OPCODE_PAGE_PROG);

			/* Wait for the command to complete */
			if( sector == (num_sec-1))
				to = 1 << 16;
			else
				to = 1 << 10;
			ret = nandc_wait_cmd(ctrl, to );
			if( (ctrl->cmd_ret = ret) < 0 )
				return ;	/* error deferred */
			} /* while */
		} /* for */
}

/*
 * NAND Interface = write_page_ecc
 */
static void nandc_write_page_ecc( struct mtd_info *mtd, struct nand_chip *chip,
                const uint8_t *buf)
{
	_nandc_write_page_do( mtd, chip, buf, true );
}

/*
 * NAND Interface = write_page_raw
 */
static void nandc_write_page_raw( struct mtd_info *mtd, struct nand_chip *chip,
                const uint8_t *buf)
{
	_nandc_write_page_do( mtd, chip, buf, false );
}

/*
 * MTD Interface - read_byte
 *
 * This function emulates simple controllers behavior
 * for just a few relevant commands
 */
static uint8_t nandc_read_byte( struct mtd_info *mtd )
{
	struct nand_chip * nand = mtd->priv ;
	struct nandc_ctrl * ctrl = nand->priv ;
	uint8_t b = ~0;

	switch( ctrl->last_cmd ) {
		case NAND_CMD_READID:
			if( ctrl->id_byte_index < 8) {
				b = _reg_read( ctrl, NANDC_DEVID_BYTE(
					ctrl->id_byte_index ));
				ctrl->id_byte_index ++;
				}
			break;
		case NAND_CMD_READOOB:
			if( ctrl->oob_index < mtd->oobsize ) {
				b = nand->oob_poi[ ctrl->oob_index ++ ] ;
				}
			break;
		case NAND_CMD_STATUS:
			b = _reg_read(ctrl, NANDC_INT_STAT_FLASH_STATUS);
			break;
		default:
			BUG_ON( 1);
		}
	DEBUG(MTD_DEBUG_LEVEL3, "%s: %#x\n", __func__, b );
	return b;
}

/*
 * MTD Interface - read_word
 *
 * Can not be tested without x16 chip, but the SoC does not support x16 i/f.
 */
static u16 nandc_read_word( struct mtd_info *mtd )
{
	u16 w = ~0;

	w = nandc_read_byte(mtd);
	barrier();
	w |= nandc_read_byte(mtd) << 8;

	DEBUG(MTD_DEBUG_LEVEL0, "%s: %#x\n", __func__, w );

	return w;
}

/*
 * MTD Interface - select a chip from an array
 */
static void nandc_select_chip( struct mtd_info *mtd, int chip)
{
	struct nand_chip * nand = mtd->priv ;
	struct nandc_ctrl * ctrl = nand->priv ;

	ctrl->chip_num = chip;
	_reg_write( ctrl, NANDC_CMD_CS_SEL, chip);
}

/*
 * NAND Interface - emulate low-level NAND commands
 *
 * Only a few low-level commands are really needed by generic NAND,
 * and they do not call for CMD_LL operations the controller can support.
 */
static void nandc_cmdfunc( struct mtd_info *mtd,
		unsigned command,
		int column,
		int page_addr)
{
	struct nand_chip * nand = mtd->priv ;
	struct nandc_ctrl * ctrl = nand->priv ;
	u64 nand_addr;
	unsigned to = 1;

	DEBUG(MTD_DEBUG_LEVEL3, "%s: cmd=%#x col=%#x pg=%#x\n", __func__,
		command, column, page_addr );

	ctrl->last_cmd = command ;

	/* Set address for some commands */
	switch( command ) {
		case NAND_CMD_ERASE1:
			column = 0;
			/*FALLTHROUGH*/
		case NAND_CMD_SEQIN:
		case NAND_CMD_READ0:
		case NAND_CMD_READ1:
			BUG_ON( column >= mtd->writesize );
			nand_addr = (u64) column |
				((u64)page_addr << nand->page_shift);
			_reg_write( ctrl, NANDC_CMD_EXT_ADDR, nand_addr >> 32 );
			_reg_write( ctrl, NANDC_CMD_ADDRESS, nand_addr );
			break;
		case NAND_CMD_ERASE2:
		case NAND_CMD_RESET:
		case NAND_CMD_READID:
		case NAND_CMD_READOOB:
		case NAND_CMD_PAGEPROG:
		default:
			/* Do nothing, address not used */
			break;
		}

	/* Issue appropriate command to controller */
	switch( command ) {
		case NAND_CMD_SEQIN:
			/* Only need to load command address, done */
			return ;

		case NAND_CMD_RESET:
			_reg_write(ctrl, NANDC_CMD_START_OPCODE,
					NANDC_CMD_OPCODE_FLASH_RESET);
			to = 1 << 8;
			break;

		case NAND_CMD_READID:
			_reg_write(ctrl, NANDC_CMD_START_OPCODE,
					NANDC_CMD_OPCODE_DEVID_READ);
			ctrl->id_byte_index = 0;
			to = 1 << 8;
			break;

		case NAND_CMD_READ0:
		case NAND_CMD_READ1:
			_reg_write(ctrl, NANDC_CMD_START_OPCODE,
					NANDC_CMD_OPCODE_PAGE_READ);
			to = 1 << 15;
			break;
		case NAND_CMD_STATUS:
			_reg_write(ctrl, NANDC_CMD_START_OPCODE,
					NANDC_CMD_OPCODE_STATUS_READ );
			to = 1 << 8;
			break;
		case NAND_CMD_ERASE1:
			return ;

		case NAND_CMD_ERASE2:
			_reg_write(ctrl, NANDC_CMD_START_OPCODE,
					NANDC_CMD_OPCODE_BLOCK_ERASE);
			to = 1 << 18;
			break;

		case NAND_CMD_PAGEPROG:
			/* Cmd already set from write_page */
			return;

		case NAND_CMD_READOOB:
			/* Emulate simple interface */
			nandc_read_oob( mtd, nand, page_addr, 1);
			ctrl->oob_index = 0;
			return;

		default:
			BUG_ON(1);

	} /* switch */

	/* Wait for command to complete */
	ctrl->cmd_ret = nandc_wait_cmd( ctrl, to) ;

}

/*
 *
 */
static int nandc_scan( struct mtd_info *mtd )
{
	struct nand_chip * nand = mtd->priv ;
	struct nandc_ctrl * ctrl = nand->priv ;
	bool sector_1k = false;
	unsigned chip_num = 0;
	int ecc_level = 0;
	int ret;

	ret = nand_scan_ident( mtd, NANDC_MAX_CHIPS, NULL);
	if( ret )
		return ret;

	DEBUG(MTD_DEBUG_LEVEL0, "%s: scan_ident ret=%d num_chips=%d\n",
		__func__, ret, nand->numchips );

#ifdef	__INTERNAL_DEBUG__
	/* For debug - change sector size, ecc level */
	_reg_write( ctrl, NANDC_ACC_CTRL_SECTOR_1K(0), 1);
	_reg_write( ctrl, NANDC_ACC_CTRL_ECC_LEVEL(0), 4);
#endif

	/* Get configuration from first chip */
	sector_1k = _reg_read( ctrl, NANDC_ACC_CTRL_SECTOR_1K(0) );
	ecc_level = _reg_read( ctrl, NANDC_ACC_CTRL_ECC_LEVEL(0) );
	mtd->writesize_shift = nand->page_shift ;

	ctrl->ecc_level = ecc_level ;
	ctrl->sector_size_shift = (sector_1k)? 10: 9;

	/* Configure spare area, tweak as needed */
	do {
		ctrl->sec_per_page_shift =
			mtd->writesize_shift - ctrl->sector_size_shift;

		/* will return -EINVAL if OOB space exhausted */
		ret = nandc_hw_ecc_layout(ctrl) ;

		/* First try to bump sector size to 1k, then decrease level */
		if( ret && nand->page_shift > 9 && ctrl->sector_size_shift < 10)
			ctrl->sector_size_shift = 10;
		else if( ret )
			ctrl->ecc_level -- ;

		} while( ret && ctrl->ecc_level > 0 );

	BUG_ON(ctrl->ecc_level == 0);

	if( (ctrl->sector_size_shift  > 9 ) != (sector_1k==1) ) {
		printk(KERN_WARNING "%s: sector size adjusted to 1k\n", DRV_NAME );
		sector_1k = 1;
		}

	if( ecc_level != ctrl->ecc_level ) {
		printk(KERN_WARNING "%s: ECC level adjusted from %u to %u\n",
			DRV_NAME, ecc_level, ctrl->ecc_level );
		ecc_level = ctrl->ecc_level ;
		}

	/* handle the hardware chip config registers */
	for( chip_num = 0; chip_num < nand->numchips; chip_num ++ ) {
		_reg_write( ctrl, NANDC_ACC_CTRL_SECTOR_1K(chip_num), 
			sector_1k);
		_reg_write( ctrl, NANDC_ACC_CTRL_ECC_LEVEL(chip_num), 
			ecc_level);

		/* Large pages: no partial page programming */
		if( mtd->writesize > 512 ) {
			_reg_write( ctrl,
				NANDC_ACC_CTRL_PGM_RDIN(chip_num), 0 );
			_reg_write( ctrl,
				NANDC_ACC_CTRL_PGM_PARTIAL(chip_num), 0 );
			}

		/* Do not raise ECC error when reading erased pages */
		/* This bit has only partial effect, driver needs to help */
		_reg_write( ctrl, NANDC_ACC_CTRL_ERA_ECC_ERR(chip_num), 0 );

		_reg_write( ctrl, NANDC_ACC_CTRL_PG_HIT(chip_num), 0 );
		_reg_write( ctrl, NANDC_ACC_CTRL_PREFETCH(chip_num), 0 );
		_reg_write( ctrl, NANDC_ACC_CTRL_CACHE_MODE(chip_num), 0 );
		_reg_write( ctrl, NANDC_ACC_CTRL_CACHE_LASTPG(chip_num), 0 );

		/* TBD: consolidate or at least verify the s/w and h/w geometries agree */
		}

	/* Allow writing on device */
	if( !(nand->options & NAND_ROM) ) {
		_reg_write( ctrl, NANDC_CS_NAND_WP, 0);
	}

	DEBUG(MTD_DEBUG_LEVEL0, "%s: layout.oobavail=%d\n", __func__,
		nand->ecc.layout->oobavail );

	ret = nand_scan_tail( mtd );

	DEBUG(MTD_DEBUG_LEVEL0, "%s: scan_tail ret=%d\n", __func__, ret );

	if( nand->badblockbits == 0 )
		nand->badblockbits = 8;
	BUG_ON( (1<<nand->page_shift) != mtd->writesize );

	/* Spit out some key chip parameters as detected by nand_base */
	DEBUG(MTD_DEBUG_LEVEL0, 
		"%s: erasesize=%d writesize=%d oobsize=%d "
		" page_shift=%d badblockpos=%d badblockbits=%d\n",
		__func__, mtd->erasesize, mtd->writesize, mtd->oobsize,
		nand->page_shift, nand->badblockpos, nand->badblockbits );

	nandc_options_print(nand->options);

	return ret ;
}

/*
 * Dummy function to make sure generic NAND does not call anything unexpected.
 */
static int nandc_dummy_func( struct mtd_info * mtd )
{
	BUG_ON(1);
}

/*
 * INTERNAL - main intiailization function
 */
static int nandc_ctrl_init( struct nandc_ctrl * ctrl )
{
	unsigned chip;
	struct nand_chip * nand ;
	struct mtd_info * mtd ;
	unsigned n = 0;

	/* Software variables init */
	nand = &ctrl->nand ;
	mtd = &ctrl->mtd ;

	init_completion( &ctrl->op_completion);

	spin_lock_init( &nand->hwcontrol.lock );
	init_waitqueue_head( &nand->hwcontrol.wq );

	mtd->priv = nand ;
	mtd->owner = THIS_MODULE ;
	mtd->name = DRV_NAME ;

	nand->priv = ctrl ;
	nand->controller = & nand->hwcontrol;

	nand->chip_delay = 5 ;	/* not used */
	nand->IO_ADDR_R = nand->IO_ADDR_W = (void *)~0L;

	if( _reg_read( ctrl, NANDC_CONFIG_CHIP_WIDTH(n) ))
		nand->options |= NAND_BUSWIDTH_16;
	nand->options |= NAND_SKIP_BBTSCAN ;	/* Dont need BBTs */

	nand->options |= NAND_NO_SUBPAGE_WRITE ; /* Subpages unsupported */

	nand->ecc.mode		= NAND_ECC_HW;

	nand->dev_ready		= nandc_dev_ready;
	nand->read_byte		= nandc_read_byte;
	nand->read_word		= nandc_read_word;
	nand->ecc.read_page_raw	= nandc_read_page_raw;
	nand->ecc.write_page_raw= nandc_write_page_raw;
	nand->ecc.read_page	= nandc_read_page_ecc;
	nand->ecc.write_page	= nandc_write_page_ecc;
	nand->ecc.read_oob	= nandc_read_oob;
	nand->ecc.write_oob	= nandc_write_oob;

	nand->select_chip 	= nandc_select_chip ;
	nand->cmdfunc 		= nandc_cmdfunc ;
	nand->waitfunc		= nandc_waitfunc ;
	nand->read_buf		= (void *) nandc_dummy_func ;
	nand->write_buf		= (void *) nandc_dummy_func ;
	nand->verify_buf	= (void *) nandc_dummy_func ;

	/* Set AUTO_CNFIG bit - try to auto-detect chips */
	_reg_write( ctrl, NANDC_CS_AUTO_CONFIG, 1);
	
	udelay(1000);

	/* Print out current chip config */
	for(chip = 0; chip < NANDC_MAX_CHIPS; chip ++ ) {
		printk("nandc[%d]: size=%#x block=%#x page=%#x ecc_level=%#x\n"
			,chip,
			_reg_read( ctrl, NANDC_CONFIG_CHIP_SIZE(chip)),
			_reg_read( ctrl, NANDC_CONFIG_BLK_SIZE(chip)),
			_reg_read( ctrl, NANDC_CONFIG_PAGE_SIZE(chip)),
			_reg_read( ctrl, NANDC_ACC_CTRL_ECC_LEVEL(chip))
			);
		}

	printk("%s: ready=%d\n", __FUNCTION__,
		_idm_reg_read(ctrl, NANDC_IDM_IO_CTRL_RDY) );

	if( nandc_scan( mtd ) )
		return -ENXIO ;

	return 0;
}

static int __init nandc_idm_init( struct nandc_ctrl * ctrl )
{
	int irq_off;
	unsigned retries = 0x1000;

	if( _idm_reg_read( ctrl, NANDC_IDM_RESET) )
		printk("%s: stuck in reset ?\n", __FUNCTION__ );

	_idm_reg_write( ctrl, NANDC_IDM_RESET, 1);
	if( !_idm_reg_read( ctrl, NANDC_IDM_RESET) ) {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: reset failed\n", __func__);
		return -1;
		}

	while( _idm_reg_read( ctrl, NANDC_IDM_RESET) ) {
		_idm_reg_write( ctrl, NANDC_IDM_RESET, 0);
		udelay(100);
		if( ! (retries--) ) {
			DEBUG(MTD_DEBUG_LEVEL0, 
				"%s: reset timeout\n", __func__);
			return -1;
			}
		}

	_idm_reg_write( ctrl, NANDC_IDM_CLOCK_EN, 1);
	_idm_reg_write( ctrl, NANDC_IDM_APB_LITTLE_ENDIAN, 0);
	udelay(10);

	printk("%s: NAND Controller rev %d.%d\n", __FUNCTION__,
		_reg_read(ctrl, NANDC_REV_MAJOR),
		_reg_read(ctrl, NANDC_REV_MINOR)
		);

	udelay(250);

	/* Disable all IRQs */
	for(irq_off = 0; irq_off < NANDC_IRQ_NUM; irq_off ++ )
		_idm_reg_write(ctrl, NANDC_IDM_IRQ_N_EN(irq_off),0);

	return 0;
}


static int __init nandc_regs_map( struct nandc_ctrl * ctrl )
{
	int res, irq ;

	res = request_resource( &iomem_resource, &nandc_regs[ 0 ] );
	if( res != 0) {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: reg resource failure\n", __func__);
		return res ;
		}

	res = request_resource( &iomem_resource, &nandc_idm_regs[ 0 ] );
	if( res != 0) {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: idm resource failure\n", __func__);
		return res ;
		}

	/* map controller registers in virtual space */
	ctrl->reg_base = ioremap(  nandc_regs[ 0 ].start,
			resource_size( &nandc_regs[ 0 ] ));

	if( IS_ERR_OR_NULL(ctrl->reg_base) ) {
		res = PTR_ERR(ctrl->reg_base);
		DEBUG(MTD_DEBUG_LEVEL0, "%s: error mapping regs\n", __func__);
		/* Release resources */
		release_resource( &nandc_regs[ 0 ] );
		return res;
	}

	/* map wrapper registers in virtual space */
	ctrl->idm_base = ioremap(  nandc_idm_regs[ 0 ].start,
			resource_size( &nandc_idm_regs[ 0 ] ));

	if( IS_ERR_OR_NULL(ctrl->idm_base) ) {
		/* Release resources */
		res = PTR_ERR(ctrl->idm_base);
		DEBUG(MTD_DEBUG_LEVEL0, "%s: error mapping wrapper\n", __func__);
		release_resource( &nandc_idm_regs[ 0 ] );
		iounmap( ctrl->reg_base );
		ctrl->reg_base = NULL;
		release_resource( &nandc_regs[ 0 ] );
		return res;
	}

	/* Acquire all interrupt lines */
	ctrl->irq_base = nandc_irqs[0].start ;
	for( irq = nandc_irqs[0].start ; irq <= nandc_irqs[0].end; irq ++ ) {
		res = request_irq( irq, nandc_isr, 0, "nandc", ctrl );
		if( res < 0 ) {
			DEBUG(MTD_DEBUG_LEVEL0, "%s: irq %d failure\n",
				__func__, irq);
		}
	}


	return 0;
}

#ifdef CONFIG_MTD_PARTITIONS
static const char *part_probes[] = { "cfenandpart", "cmdlinepart", NULL };
#endif

/*
 * Top-level init function
 */
static int __devinit nandc_probe(struct platform_device *pdev )
{
	static struct nandc_ctrl _nand_ctrl;
	struct nandc_ctrl * ctrl = & _nand_ctrl ;
	int res ;
	
	ctrl = pdev->dev.platform_data;

	platform_set_drvdata(pdev, ctrl);

	ctrl->device	= &pdev->dev ;

	res = nandc_regs_map( ctrl );
	if( res ) {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: regs_map failed\n", __func__);
		goto probe_error ;
		}

	res = nandc_idm_init( ctrl );
	if( res ) {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: idm_init failed\n", __func__);
		goto probe_error ;
		}

	res = nandc_ctrl_init( ctrl );
	if( res ) {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: ctrl_init failed\n", __func__);
		goto probe_error ;
		}

	ctrl->mtd.dev.parent = ctrl->device;

#ifdef CONFIG_MTD_PARTITIONS
	res = parse_mtd_partitions( &ctrl->mtd, part_probes, &ctrl->parts, 0 );

	if( res > 0 ) {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: registering MTD partitions\n",
			__func__);
		res = add_mtd_partitions( &ctrl->mtd, ctrl->parts, res );
		if( res < 0 ) {
			DEBUG(MTD_DEBUG_LEVEL0,
				"%s: failed to register partitions\n",
				__func__);
			goto probe_error ;
			}
		}
	else
#else
	{
	    DEBUG(MTD_DEBUG_LEVEL0, "%s: registering the entire device MTD\n",
			__func__);
	    if( (res = add_mtd_device( &ctrl->mtd ))) {
		DEBUG(MTD_DEBUG_LEVEL0, "%s: failed to register\n", __func__);
		goto probe_error ;
		}
	}
#endif

	return 0;
  probe_error:
	return res;
}

/* Single device present */
static struct nandc_ctrl _nand_ctrl;

/* driver structure for object model */
static struct platform_driver nandc_driver = {
	.probe		= nandc_probe,
	.driver		= {
		.name 	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
};

static struct platform_device platform_nand_devices = {
        .name = DRV_NAME,
	.id   = 0x5a5d,
        .dev = {
                .platform_data = &_nand_ctrl,
        },
};


static int nandc_init(void)
{
	int ret;
	printk(KERN_INFO "%s, Version %s (c) Broadcom Inc. 2012\n",
		DRV_DESC, DRV_VERSION );

	if( (ret =  platform_driver_register( &nandc_driver )))
		return ret;
	return 0;
}

static int nandc_dev_start(void)
{
	printk("%s:\n", __func__);
	return platform_device_register( &platform_nand_devices );
}

module_init(nandc_init);

/* Device init myst be delayed to let "mtd" module initialize before it does */
/* I wish there was a way to explicitly declare dependencies */
late_initcall( nandc_dev_start );

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRV_DESC);
MODULE_ALIAS("platform:" DRV_NAME);
