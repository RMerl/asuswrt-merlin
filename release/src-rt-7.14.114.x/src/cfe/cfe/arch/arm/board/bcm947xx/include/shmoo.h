/*
** Copyright 2000, 2001  Broadcom Corporation
** All Rights Reserved
**
** No portions of this material may be reproduced in any form 
** without the written permission of:
**
**   Broadcom Corporation
**   5300 California Avenue
**   Irvine, California 92617
**
** All information contained in this document is Broadcom 
** Corporation company private proprietary, and trade secret.
**
** ----------------------------------------------------------
**
** 
**
**  $Id:: platform_shmoo_priv.h 1758 2012-08-17 15:14:03Z gennady $:
**  $Rev::file =  : Global SVN Revision = 1780                    $:
**
*/

/* platform_shmoo_priv.h */

#ifndef PLATFORM_SHMOO_PRIV_H__
#define PLATFORM_SHMOO_PRIV_H__

/* SHMOO defines
 * 
 * Each word lane is made up of two byte lanes
 */
#define BL_PER_WL     2

#define DEFAULT_BIT_COUNT 16

#define USE_MSA   0
#define USE_MIPS  0


#define uart_out UART_OUT

/*
 * Previously it was BLOCK_SIZE and there was a conflict with the defines
 * in the linux kernel header file
 */
#define SHMOO_BLOCK_SIZE 64 /* * sizeof(uint32_t) = block size (256 bytes) */
#define NUM_BLOCKS 4	/* = # blocks */
#define NUM_DATA (SHMOO_BLOCK_SIZE*NUM_BLOCKS)   /* = number of uint32_t data */

#define MEM_SIZE_BYTES   1024                 /* memory test block size in bytes */
#define MEM_SIZE_INTS    (MEM_SIZE_BYTES/4)   /* memory test block size in ints */


#define FULL_DEBUG_PRINT	0
#define SHMOO_DEBUG_TIME	0

/* I/O */
/* UART */
typedef union pack328_ {
	unsigned pack32;
	char pack8[4];
} pack328_t;


/* Memory controller */

/* DIS client */

/* Generic DRAM read/write macros and functions
 */
/* Read from DRAM address */
#define SHMOO_DRAM_READ_32(ADDR)	(*((volatile uint32_t*)((ADDR) & (~0x3))))

/* Write to DRAM address */
#define SHMOO_DRAM_WRITE_32(ADDR, DATA)	(*((volatile uint32_t*)((ADDR) & (~0x3)))) = (DATA)


/*
 * Start and end DRAM addresses
 */
#define SHMOO_DRAM_START_ADDR		NOT_DEFINED
#define SHMOO_DRAM_END_ADDR		NOT_DEFINED


/*
 * Generic registers access
 */

/*
 * PHY_REG_OFFSET_PER_WL is the offset in address for the two identical registers in difference WL
 * For BLAST, this difference is 0x200
 * This is for support a loop to do the same procedures for different WL
 */
#define PHY_REG_OFFSET_PER_WL 0x200
#define PHY_REG_OFFSET_PER_BL 0x0A0


/* Read from the register */
#define SHMOO_REG_READ(ADDR)		(*((volatile uint32_t*)(ADDR)))

/* Write to the register */
#define SHMOO_REG_WRITE(ADDR, DATA)	(*((volatile uint32_t*)(ADDR))) = (DATA)


/*
 * Define macros for working with bit fields
 */
#if defined(GET_FIELD) || defined(SET_FIELD)
#undef BRCM_ALIGN
#undef BRCM_BITS
#undef BRCM_MASK
#undef BRCM_SHIFT
#undef GET_FIELD
#undef SET_FIELD
#endif /* if defined(GET_FIELD) || defined(SET_FIELD) */

#define BRCM_ALIGN(c, r, f)   c##_##r##_##f##_ALIGN
#define BRCM_BITS(c, r, f)    c##_##r##_##f##_BITS
#define BRCM_MASK(c, r, f)    c##_##r##_##f##_MASK
#define BRCM_SHIFT(c, r, f)   c##_##r##_##f##_SHIFT

#define GET_FIELD(m, c, r, f) \
	((((m) & BRCM_MASK(c, r, f)) >> BRCM_SHIFT(c, r, f)))

#define SET_FIELD(m, c, r, f, d) \
	((m) = (((m) & ~BRCM_MASK(c, r, f)) | \
	(((d) << BRCM_SHIFT(c, r, f)) & BRCM_MASK(c, r, f))))

/* ovr_force = ovr_en = 1, ovr_step = v */
#define SET_OVR_STEP(v) (0x30000 | ((v) & 0x3F))

/*
 * Define aliases for the reads and writes
 *
 * GLOBAL_REG_RBUS_START should be defined in the platform specific files
 */
#define tb_r(addr)           (*(volatile unsigned *)(addr))
#define tb_w(addr, value)    *(volatile unsigned *)(addr) = (value)
#define RDREG(a)             tb_r(GLOBAL_REG_RBUS_START + a)
#define WRREG(a, d)          tb_w(GLOBAL_REG_RBUS_START + a, d)


/* Common defines */
#define RD_EN_BYTE_SEL_CHANGE 1

#define WORD_STEP_COUNT    2
#define BYTE_STEP_COUNT    4
#define BYTE_BIT_BASE      2
#define RDEN_FIFO_COUNT    2
#define RDEN_BYTE_STEPS    4
#define RDEN_WORD_STEPS    2

#define WR_50PER_CALIB	1
#define WR_33PER_CALIB	0


/*
 * BITS per Word Lane.
 */
#define BITS_PER_WL	  16
#define BITS_PER_BL       8
#define WRDQ_MASK         0xFF
#define WRDM_STEPS        4
#define WRDQ_VALS         16

#define VDL_STEPS         64
#define VDL_END           VDL_STEPS
#define PASS_BIT          0
#define FAIL_BIT          1
#define FAIL_TO_LOOK_FOR  2

#define MAX_BIT_TO_CHECK   6
#define FAIL_BIT_TO_CHECK  1

#define NO_WL_PRINT  2
#define PAGE 4

#define WR_VDL_FIXED_DLY  8
#define WR_VDL_TOTAL_CNT  3
#define WR_VDL_MAJOR_CNT  2
#define K_WIN_SIZE        4

#define ADDRC_BIT_COUNT  1

/* This specifies the size of Memory to use for DIS-0 during shmoo */
#define DIS_SIZE_ADDR          0x3E80

/* Timeout magic number,
 * may be redefined in the platfrom_*.h files
 */
#define TIMEOUT_VAL             9000

#define CR 0xd
#define LF 0xa
#define ZERO 0x30
#define ONE  0x31
#define SPACE 0x20

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(A) (sizeof(A) / sizeof(A[0]))
#endif

#define NL()	UART_OUT(CR); UART_OUT(LF); /* need {} when used after an if statement */

/* Private function declarations */
void UART_OUT(unsigned int val);
void plot_name(pack328_t name, int wl);
void plot_dec_number(unsigned int val);
void plot_hex_number(unsigned int val);


/* #define WR_VDL_45deg		1 */
#define WR_VDL_22p5deg		1
/* #define WR_VDL_67p5deg	1 */
/* #define WR_VDL_90deg		1 */


/* Set this define to 1 to use TMT method to generate the data to read/write
 * to memory during shmoo
 *
 * Set this define to 0 to use 0xAAAAAAAA and 0x55555555 patterns for the data
 */
#define SHMOO_USE_TMT		1

/* Set this define to 1 in order to use memory copy function
 * (useful for Northstar platform)
 */
#define SHMOO_USE_MEMCPY    1


#define SHMOO_REL_YEAR   2012
#define SHMOO_REL_MONTH  6
#define SHMOO_REL_DAY    1
/*
 * The date code is packed as YYYYMMDD. ie, 20110618
 */
#define SHMOO_REL_DATE ((SHMOO_REL_YEAR<<16) | (SHMOO_REL_MONTH << 8) | SHMOO_REL_DAY)


#define SHMOO_VER_MAJ     0x1
/* these 2 moved to makefile */
#define SHMOO_VER_MIN     0x1
#define SHMOO_VER_MIN2    0x3

#define SHMOO_VER_MIN_LONG ((SHMOO_VER_MIN<<4)|SHMOO_VER_MIN2)

#ifdef USE_16BIT
#define PHY_DATA_WIDTH   0x16
#warning CAPABILITY WILL SHOW 16b
#else
#define PHY_DATA_WIDTH   0x32
#endif
#define PHY_DW_POS       24
#define PHY_WL_COUNT     0x02
#define PHY_WL_POS       16

#define SHMOO_VJ_POS     8

#define SHMOO_VER_MAJOR   (SHMOO_VER_MAJ+0x30)
#define SHMOO_VER_MINOR   (SHMOO_VER_MIN+0x30)
#define SHMOO_VER_MINOR2  (SHMOO_VER_MIN2+0x30)
#define BTOA(A)           (A+0x30)

#define SHMOO_VERSION ((PHY_DATA_WIDTH << PHY_DW_POS) | \
	(PHY_WL_COUNT << PHY_WL_POS) | \
	(SHMOO_VER_MAJ << SHMOO_VJ_POS) | \
	SHMOO_VER_MIN_LONG)


#ifdef IS_ENG_VER
#define ENG_VER_MARKER  UART_OUT(SHMOO_IS_ENG); UART_OUT(SHMOO_ENG_VER);
#else
#define ENG_VER_MARKER
#endif

/* Shmoo VERSION Macro
 * REVISION STRING
 */
#if defined USE_MSA && defined USE_16BIT

#define DISP_SHMOO_VER \
    NL(); \
    packname.pack32 = 0x4F4D4853; /* SHMO */ \
    plot_four_chars(packname); \
    packname.pack32 = 0x4556204F; /* O VE  */ \
    plot_four_chars(packname); \
    uart_out('R'); \
    uart_out(' '); \
    uart_out('M'); \
    uart_out('S'); \
    uart_out('A'); \
    uart_out(' '); \
    uart_out('1'); \
    uart_out('6'); \
    uart_out('b'); \
    uart_out(' '); \
    uart_out(BTOA(SHMOO_VER_MAJ)); \
    uart_out('.'); \
    uart_out(BTOA(SHMOO_VER_MIN)); \
    uart_out(BTOA(SHMOO_VER_MIN2)); \
    ENG_VER_MARKER \
    NL()

#else

#define DISP_SHMOO_VER \
    NL(); \
    packname.pack32 = 0x4F4D4853; /* SHMO */ \
    plot_four_chars(packname); \
    packname.pack32 = 0x4556204F; /* O VE */ \
    plot_four_chars(packname); \
    uart_out('R'); \
    uart_out(' '); \
    uart_out(BTOA(SHMOO_VER_MAJ)); \
    uart_out('.'); \
    uart_out(BTOA(SHMOO_VER_MIN)); \
    uart_out(BTOA(SHMOO_VER_MIN2)); \
    ENG_VER_MARKER \
    NL()

#endif /* USE_MSA &&  USE_16BIT */

void SHMOO_FLUSH_DATA_TO_DRAM(ddr40_addr_t Address, unsigned int bytes);
void SHMOO_INVALIDATE_DATA_FROM_DRAM(ddr40_addr_t Address, unsigned int bytes);

int shmoo_adjust_wr_dly(ddr40_addr_t phy_reg_base, ddr40_addr_t memc_reg_base);
int shmoo_adjust_addx_ctrl_delay(ddr40_addr_t phy_reg_base, ddr40_addr_t memc_reg_base,
	int shift_amount);

unsigned int all_shmoo_rd_data_dly_FIFO(ddr40_addr_t phy_reg_base,
	ddr40_addr_t memc_reg_base, unsigned int option, ddr40_addr_t mem_test_base);

unsigned int all_shmoo_RE_byte_FIFO(ddr40_addr_t phy_reg_base,
	ddr40_addr_t memc_reg_base, unsigned int option, ddr40_addr_t mem_test_base);

unsigned int generic_shmoo_calib_rd_dqs(ddr40_addr_t phy_reg_base,
	ddr40_addr_t memc_reg_base, unsigned int option, ddr40_addr_t mem_test_base);

unsigned int generic_shmoo_calib_wr_dq(ddr40_addr_t phy_reg_base,
	ddr40_addr_t memc_reg_base, unsigned int option, ddr40_addr_t mem_test_base);

unsigned int generic_shmoo_calib_wr_dm(ddr40_addr_t phy_reg_base,
	ddr40_addr_t memc_reg_base, unsigned int option, ddr40_addr_t mem_test_base);

unsigned int generic_shmoo_calib_addr(ddr40_addr_t phy_reg_base,
	ddr40_addr_t memc_reg_base, unsigned int option, ddr40_addr_t mem_test_base);

unsigned int mtest_verify(ddr40_addr_t memc_reg_base, ddr40_addr_t addr,
	unsigned int * data_expected, unsigned long mt_mode, unsigned long wrd_ln);
void InitMemory(ddr40_addr_t const Address, unsigned long const nSizeDword,
	unsigned long const Seed);
void InitMemoryV2(ddr40_addr_t Address, unsigned long const nLengthInDwords);
void InitMemoryV3(ddr40_addr_t Address, unsigned long const nLengthInDwords, int const j);
void CleanMemory(ddr40_addr_t const Address, unsigned long const nLengthInDwords,
	unsigned int const nMask);
unsigned int CheckMemory(ddr40_addr_t const Address, unsigned long const nSizeDword,
	unsigned int const Seed, unsigned int const wrd_ln);
unsigned int CheckMemoryV2(ddr40_addr_t Address, unsigned long const nLengthDwords,
	unsigned int wrd_ln);
unsigned int CheckMemoryV3(ddr40_addr_t Address, unsigned long const nLengthDwords,
	unsigned int wrd_ln, int const j);
unsigned int verify_mem_block_mips(ddr40_addr_t memc_reg_base, ddr40_addr_t addr,
	unsigned int size, unsigned int mt_mode, int wl);
int rewrite_dram_mode_regs(void *sih);
int timer_delay(unsigned int delay);
void timeout_ns(unsigned int ticks);

void CheckTmt(void);

#endif /* PLATFORM_SHMOO_PRIV_H__ */
