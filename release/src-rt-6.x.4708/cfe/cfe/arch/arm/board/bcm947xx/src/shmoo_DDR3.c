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
**  $Id:: shmoo_DDR3.c 1440 2012-07-10 17:02:43Z gennady          $:
**  $Rev::file =  : Global SVN Revision = 1780                    $:
**
*/

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>

#include <shmoo_public.h>
#include <shmoo.h>
/*
 * The order of the include files is important:
 * platform should be include after the shmoo.h and/or shmoo_public
 */
#include <platform.h>
#include <tinymt32.h>
#include <ddr40_variant.h>

static void *sih_saved;

/* Need to use the same handle for several calls to tinymt32_init() 
 * in order to get the same sequence of the data
 */ 
static tinymt32_t tinymt32_rand;
tinymt32_t *TINYMT32_RAND_PTR;

void NL(void)
{
	UART_OUT('\n');
}

void plot_name(pack328_t name, int wl)
{
	unsigned int i = 0;

	UART_OUT('\n');
	for (/* Trick */; i < ARRAY_SIZE(name.pack8); i++) {
		UART_OUT(name.pack8[i]);
	}
	if (wl != NO_WL_PRINT) {
		UART_OUT('W');
		UART_OUT((wl == 0)?'0':'1');
		UART_OUT(' ');
	}
}

static void plot_four_chars(pack328_t name)
{
#ifdef BIGEND
	UART_OUT(name.pack8[3]);
	UART_OUT(name.pack8[2]);
	UART_OUT(name.pack8[1]);
	UART_OUT(name.pack8[0]);
#else
	UART_OUT(name.pack8[0]);
	UART_OUT(name.pack8[1]);
	UART_OUT(name.pack8[2]);
	UART_OUT(name.pack8[3]);
#endif
}

unsigned int verify_mem_block_mips(ddr40_addr_t memc_reg_base, ddr40_addr_t addr,
	unsigned int size, unsigned int mt_mode, int wl)
{
	uint32_t result = 0;
	uint32_t const nLengthInDwords = 128;	/* addr */

	/* To make compiler happy - start */
	memc_reg_base++;
	size++;
	mt_mode++;
	/* To make compiler happy - end */
	InitMemory(addr, nLengthInDwords, 0x31426926);
	result = CheckMemory(addr, nLengthInDwords, 0x31426926, wl);
	return result;
}

unsigned int mtest_verify(ddr40_addr_t memc_reg_base, ddr40_addr_t addr,
	unsigned int *data_expected, unsigned long mt_mode, unsigned long wrd_ln)
{
	unsigned int nRet = 0;
	uint32_t const nLengthInDwords = 128;	/* addr */

	/* To make compiler happy - start */
	memc_reg_base++;
	data_expected++;
	mt_mode++;
	/* To make compiler happy - end */
	InitMemory(addr, nLengthInDwords, 0x31426926);
	nRet = CheckMemory(addr, nLengthInDwords, 0x31426926, wrd_ln);
	return nRet;
}


#if SHMOO_USE_MEMCPY

#define DATA_TRANSFER_LENGTH_BYTES      (128 * 4)
#define DATA_TRANSFER_LENGTH_DWORDS     ((DATA_TRANSFER_LENGTH_BYTES) / 4)

void InitMemory(ddr40_addr_t Address, unsigned long const nLengthInDwords, unsigned long const Seed)
{
	uint32_t i;
	uint32_t aData[DATA_TRANSFER_LENGTH_DWORDS + 1];

	Address &= ~0x3;

	/* Initialize the generator
	 * Handle to the function should be the same in order to get the same
	 * reproducible pseudo-random values.
	 */
	tinymt32_init((tinymt32_t *)TINYMT32_RAND_PTR, Seed);

	/* fill the buffer with random numbers here */
	for (i = 0; i < DATA_TRANSFER_LENGTH_DWORDS; i++) {
		aData[i] = tinymt32_generate_uint32((tinymt32_t *)TINYMT32_RAND_PTR);
	}

	memcpy((void *)Address, aData, DATA_TRANSFER_LENGTH_BYTES);

	SHMOO_FLUSH_DATA_TO_DRAM(Address, DATA_TRANSFER_LENGTH_BYTES);
	SHMOO_INVALIDATE_DATA_FROM_DRAM(Address, DATA_TRANSFER_LENGTH_BYTES);
}

unsigned int CheckMemory(ddr40_addr_t Address, unsigned long const nLengthDwords,
	unsigned int const Seed, unsigned int wrd_ln)
{
	uint32_t i;
	uint32_t aData[DATA_TRANSFER_LENGTH_DWORDS + 1];
	uint32_t result_dst = 0;

	Address &= ~0x3;

	/* Initialize the generator
	 * Handle to the function should be the same in order to get the same
	 * reproducible pseudo-random values.
	 */
	tinymt32_init((tinymt32_t *)TINYMT32_RAND_PTR, Seed);

	memcpy(aData, (void *)Address, DATA_TRANSFER_LENGTH_BYTES);
	/* read the destination buffer and compare it to the random numbers
	 * that were written to the source buffer
	 */
	for (i = 0; i < DATA_TRANSFER_LENGTH_DWORDS; i++) {
		uint32_t const DstValue = aData[i];
		uint32_t const Value = tinymt32_generate_uint32((tinymt32_t *)TINYMT32_RAND_PTR);
		uint32_t const xor_result = DstValue ^ Value;

		result_dst |= xor_result;
	}

	/* Select correct word lane */
#if defined(DDR40_WIDTH_IS_32)
	/* For 32-bit PHY */
	if (wrd_ln != 0) {
		result_dst >>= 16;
	}
	result_dst &= 0xFFFF;
#else
	/* For 16-bit PHY */
	result_dst = (result_dst & 0xFFFF) | ((result_dst >> 16) & 0xFFFF);
#endif

	SHMOO_INVALIDATE_DATA_FROM_DRAM(Address, DATA_TRANSFER_LENGTH_BYTES);

	return result_dst;
}
#else /* SHMOO_USE_MEMCPY */
void InitMemory(ddr40_addr_t Address, unsigned long const nLengthInDwords, unsigned long const Seed)
{
	uint32_t i;

	Address &= ~0x3;
	/* Initialize the generator
	 * Handle to the function should be the same in order to get the same
	 * reproducible pseudo-random values.
	 */
	tinymt32_init((tinymt32_t *)TINYMT32_RAND_PTR, Seed);

	/* fill the buffer with random numbers here */
	for (i = 0; i < nLengthInDwords; i++, Address += sizeof(uint32_t)) {
		uint32_t const Value = tinymt32_generate_uint32((tinymt32_t *)TINYMT32_RAND_PTR);
		SHMOO_DRAM_WRITE_32(Address, Value);
	}
	SHMOO_FLUSH_DATA_TO_DRAM(Address, nLengthInDwords * 4);
}

unsigned int CheckMemory(ddr40_addr_t Address, unsigned long const nLengthDwords,
	unsigned int const Seed, unsigned int wrd_ln)
{
	uint32_t i;
	uint32_t result_dst = 0;
	ddr40_addr_t AddressCopy = Address;

	Address &= ~0x3;
	/* Initialize the generator
	 * Handle to the function should be the same in order to get the same
	 * reproducible pseudo-random values.
	 */
	tinymt32_init((tinymt32_t *)TINYMT32_RAND_PTR, Seed);

	/* read the destination buffer and compare it to the random numbers
	 * that were written to the source buffer
	 */
	for (i = 0; i < nLengthDwords; i++, Address += sizeof(uint32_t)) {
		uint32_t const DstValue = SHMOO_DRAM_READ_32(Address);
		uint32_t const Value = tinymt32_generate_uint32((tinymt32_t *)TINYMT32_RAND_PTR);
		uint32_t const xor_result = DstValue ^ Value;
		result_dst |= xor_result;
	}

	/* Select correct word lane */
#if defined(DDR40_WIDTH_IS_32)
	/* For 32-bit PHY */
	if (wrd_ln != 0) {
		result_dst >>= 16;
	}
	result_dst &= 0xFFFF;
#else
	/* For 16-bit PHY */
	result_dst = (result_dst & 0xFFFF) | ((result_dst >> 16) & 0xFFFF);
#endif

	SHMOO_INVALIDATE_DATA_FROM_DRAM(AddressCopy, nLengthDwords * 4);

	return result_dst;
}
#endif  /* SHMOO_USE_MEMCPY */

/* ---- Private Functions ------------------------------------------------ */
/* int timer_delay(unsigned int delay);
 *
 * ****************************
 *
 * timeout_ns
 *
 * a very simple and rudimentary way to generate some delay.
 * not very accurate.
 *
 * input :   ticks = some non-zero value
 * return:   none
 *
 * void timeout_ns(unsigned int ticks);
 */
void timeout_ns(unsigned int ticks)
{
	volatile unsigned int count = ticks;
	while (count-- > 0) /* nothing */;
}

int timer_delay(unsigned int const delay)
{
	int temp = 0;
	unsigned int k;

	for (k = 0; k < delay; k++) {
		int i;
		for (i = 0; i < 100; i++) {
			temp *= i;
		}
	}
	return temp;
}

void DramMemTest(ddr40_addr_t mem_test_base)
{
	unsigned int n;

	InitMemory(mem_test_base, 128, 0x31426926);
	n = CheckMemory(mem_test_base, 128, 0x31426926, 0);
	printf("Lane 0: n = 0x%X\n", n);
	n = CheckMemory(mem_test_base, 128, 0x31426926, 1);
	printf("Lane 1: n = 0x%X\n", n);
}


#if (SHMOO_BUILD == 1)

/* Pattern for the analyzing on the scope */
void InitMemoryV2(unsigned long Address, unsigned long const nLengthInDwords)
{
	uint32_t i;
	uint32_t Value = 0xAAAAAAAAul;

	Address &= ~0x3;
	for (i = 0; i < nLengthInDwords; i++, Address += sizeof(uint32_t)) {
		SHMOO_DRAM_WRITE_32(Address, Value);
		Value = ~Value;
	}
}

/* Bitflip pattern */
void InitMemoryV3(unsigned long Address, unsigned long const nLengthInDwords, int const j)
{
	uint32_t i;
	uint32_t pattern = 0x80000000ul >> j;

	Address &= ~0x3;
	pattern = ~pattern;
	for (i = 0; i < nLengthInDwords; i++, Address += sizeof(uint32_t)) {
		uint32_t Value = (i % 2) == 0 ? pattern:~pattern;
		SHMOO_DRAM_WRITE_32(Address, Value);
		Value = ~Value;
	}
}

void CleanMemory(unsigned long Address, unsigned long const nLengthInDwords,
	unsigned int const nMask)
{
	uint32_t i;

	Address &= ~0x3;

	for (i = 0; i < nLengthInDwords; i++, Address += sizeof(uint32_t)) {
		uint32_t const Value = (nMask & 0xFFFF0000u) + i;
		SHMOO_DRAM_WRITE_32(Address, Value);
	}
}

unsigned int CheckMemoryV2(unsigned long Address, unsigned long const nLengthDwords,
	unsigned int wrd_ln)
{
	uint32_t i;
	uint32_t result_dst = 0;
	uint32_t Value = 0xAAAAAAAAul;

	Address &= ~0x3;
	/* read the destination buffer and compare it to the pattern */
	for (i = 0; i < nLengthDwords; i++, Address += sizeof(uint32_t)) {
		uint32_t const DstValue = SHMOO_DRAM_READ_32(Address);
		uint32_t const xor_result = DstValue ^ Value;
		Value = ~Value;
		result_dst |= xor_result;
	}
	/* Select correct word lane */
	if (wrd_ln != 0) {
		result_dst >>= 16;
	}
	result_dst &= 0xFFFF;
	return result_dst;
}

/* For bitflip test */
unsigned int CheckMemoryV3(unsigned long Address, unsigned long const nLengthDwords,
	unsigned int wrd_ln, int const j)
{
	uint32_t i;
	uint32_t result_dst = 0;
	uint32_t pattern = 0x80000000ul >> j;

	Address &= ~0x3;
	pattern = ~pattern;
	/* read the destination buffer and compare it to the random numbers
	 * that were written to the source buffer
	 */
	for (i = 0; i < nLengthDwords; i++, Address += sizeof(uint32_t)) {
		uint32_t Value = (i % 2) == 0 ? pattern:~pattern;
		uint32_t const DstValue = SHMOO_DRAM_READ_32(Address);
		uint32_t const xor_result = DstValue ^ Value;
		result_dst |= xor_result;
	}
	return result_dst;
}

static inline void generic_calib_steps(ddr40_addr_t phy_reg_base);
static inline void generic_calib_steps(ddr40_addr_t phy_reg_base)
{
	/* calib steps */
	WRREG(phy_reg_base + DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIBRATE, 0x0);
	WRREG(phy_reg_base + DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIBRATE,
		DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIBRATE_calib_steps_MASK); /* calib_steps = 1 */
	while (true) {
		uint32_t const regval = RDREG(phy_reg_base
			+ DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIB_STATUS);
		if ((regval & DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIB_STATUS_calib_idle_MASK)
			== DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIB_STATUS_calib_idle_MASK) {
			break;
		}
	}

	NL();
	UART_OUT('S');
	plot_hex_number(tb_r(phy_reg_base + DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIB_STATUS +
		GLOBAL_REG_RBUS_START));
	NL();
}

static inline void generic_shmoo_cleanup(unsigned long phy_reg_base, unsigned int wl_in_use);
static inline void generic_shmoo_cleanup(unsigned long phy_reg_base, unsigned int wl_in_use)
{
	unsigned wl;

	for (wl = 0; wl < wl_in_use; wl++) {
		WRREG((phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_CLEAR +
			wl * PHY_REG_OFFSET_PER_WL), 0x1);
	}
}

/* ---- Public Functions ------------------------------------------------- */
/* Inputs:
 *   unsigned int phy_reg_base     base offset for PHY
 *   unsigned int memc_reg_base    base offset for MEMC
 *   unsigned int option           shmoo runtime options 
 *                                   [29:28]    : Address Shmoo Type
 *                                               0=normal.  <== DEFAULT 
 *                                               1=low-stress
 *                                               2=med-stress
 *                                               3=high-stress (takes about 40 secs)
 *                                   [27:24] : addx/ctrl initial VDL shift:
 *                                               0 = none   <== DEFAULT
 *                                               90deg * N/16
 *                                   [23:16] : log2(MEM_SIZE)-1.  I.E. 256MB => 27
 *                                   [15:08] : width  32 or 16  
 *                                   [07:00] : run time shmoo options
 *                                               5 = run Addr Calib shmoo
 * 
 *   unsigned int mem_test_base)   memory area for WR/RD testing
 */
int do_shmoo(void *sih,	ddr40_addr_t phy_reg_base, ddr40_addr_t memc_reg_base,
	unsigned int option, ddr40_addr_t mem_test_base)
{
	unsigned int nRet = 0;
	int shift_amount, stat;
	pack328_t packname;

	sih_saved = sih;
	TINYMT32_RAND_PTR = &tinymt32_rand;
	/* display SHMOO version */
	DISP_SHMOO_VER;

	packname.pack32 = 0x44494B50; /* PKID */
	plot_name(packname, NO_WL_PRINT);
	/* plot_hex_number(SHMOO_VERSION); */
	plot_hex_number(SHMOO_REL_DATE);

	/* plot info for this run */
	plot_hex_number(phy_reg_base);
	plot_hex_number(memc_reg_base);
	plot_hex_number(option);
	plot_hex_number(mem_test_base);
	NL();

	/* do WR chan calib step calibration */
	generic_calib_steps(phy_reg_base);

	/* do A/C VDL shift */ 
	shift_amount = (option >> 24) & 0xF;
	stat = shmoo_adjust_addx_ctrl_delay(phy_reg_base, memc_reg_base, shift_amount);
	nRet |= all_shmoo_rd_data_dly_FIFO(phy_reg_base, memc_reg_base, option, mem_test_base);
	nRet |= all_shmoo_RE_byte_FIFO(phy_reg_base, memc_reg_base, option, mem_test_base);
	nRet |= generic_shmoo_calib_rd_dqs(phy_reg_base, memc_reg_base, option, mem_test_base);
	nRet |= generic_shmoo_calib_wr_dq(phy_reg_base, memc_reg_base, option, mem_test_base);
	nRet |= generic_shmoo_calib_wr_dm(phy_reg_base, memc_reg_base, option, mem_test_base);
	nRet |= generic_shmoo_calib_addr(phy_reg_base, memc_reg_base, option, mem_test_base);
	generic_shmoo_cleanup(phy_reg_base, (((option >> 8) & 0xFF) / BITS_PER_WL));

	return nRet;
}

static unsigned one_shmoo_RE_FIFO(unsigned int  *word_new_step,
	unsigned int *byte_new_step, unsigned int option, ddr40_addr_t phy_reg_base,
	ddr40_addr_t memc_reg_base, ddr40_addr_t mem_test_base);

static unsigned one_shmoo_RE_FIFO(unsigned int *word_new_step,
	unsigned int *byte_new_step, unsigned int option, ddr40_addr_t phy_reg_base,
	ddr40_addr_t memc_reg_base, ddr40_addr_t mem_test_base)
{
	unsigned int  retval = SHMOO_NO_ERROR;
	unsigned int  data;
	unsigned int  byte_setting;
	unsigned int  word_setting;
	unsigned int  passing;
	unsigned int  fifo_result[RDEN_FIFO_COUNT] = {0, 0};
	unsigned int  wlx_start[RDEN_WORD_STEPS] = {0, 0};
	unsigned int  wlx_blx_pass[RDEN_BYTE_STEPS] = {0, 0, 0, 0};
	unsigned int  wlx_blx_start[RDEN_BYTE_STEPS] = {0, 0, 0, 0};
	unsigned int  common;
	unsigned int  ticks_to_add;
#if (SHMOO_PLATFORM == SHMOO_PLATFORM_S2) || (SHMOO_PLATFORM == SHMOO_PLATFORM_S2_A)
	static unsigned int data_expected[64 * 4];
#else
	int const size = 64;
	int const pages = PAGE;
	unsigned int data_expected[size * pages];
#endif
	unsigned int wl;
	unsigned long memsys_reg_offset;

	ticks_to_add = (GET_FIELD(tb_r(phy_reg_base +
		DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIB_STATUS + GLOBAL_REG_RBUS_START),
		DDR40_CORE_PHY_CONTROL_REGS, VDL_CALIB_STATUS, calib_total)) / 4;

	for (wl = 0; wl < (((option >> 8) & 0xFF) / BITS_PER_WL); wl++) {
		memsys_reg_offset = phy_reg_base + GLOBAL_REG_RBUS_START
			+ wl * PHY_REG_OFFSET_PER_WL;

		/* START: PREPARATION */
		/* set RD_EN Byte VDL's to 0 */
		word_setting = 0;
		data = 0;
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN, ovr_en, 1);
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN, ovr_force, 1);
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN, ovr_step,
			word_setting);
		tb_w(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE_RD_EN + memsys_reg_offset, data);

		/* set RD_EN Bit VDL's to 0 */
		byte_setting = 0;
		data = 0;
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN,
			ovr_en, 1);
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN,
			ovr_force, 1);
#if RD_EN_BYTE_SEL_CHANGE
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN,
			byte_sel, 1);
#endif
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN,
			ovr_step, byte_setting);
		tb_w(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN + memsys_reg_offset,
			data);
		tb_w(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN + memsys_reg_offset,
			data);
		mtest_verify(memc_reg_base, mem_test_base, data_expected, mem_test_base, wl);

		/* Clear the FIFO status for this WL */
		tb_w(DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_CLEAR + memsys_reg_offset, 1);
		/* END: PREPARATION */

		/* START: PER WL */
		/* start word lane 0 */ 
		passing = 0;

		while (passing == 0) {
			mtest_verify(memc_reg_base, mem_test_base, data_expected,
				mem_test_base, wl);

			/* read and save status0 and status1 from READ_FIFO_STATUS for this WL */
			fifo_result[0] = GET_FIELD(
				tb_r(DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_STATUS
				+ memsys_reg_offset),
				DDR40_CORE_PHY_WORD_LANE_0, READ_FIFO_STATUS, status0);
			fifo_result[1] = GET_FIELD(
				tb_r(DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_STATUS
				+ memsys_reg_offset),
				DDR40_CORE_PHY_WORD_LANE_0, READ_FIFO_STATUS, status1);
			/* Clear the FIFO status for this WL */
			tb_w(DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_CLEAR + memsys_reg_offset,
				DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_CLEAR_clear_MASK);
			timeout_ns(TIMEOUT_VAL);
			/* check which byte lane has problem (0, 1) or (2, 3) */
			if (fifo_result[0] == 0) {
				if (wlx_blx_pass[wl * RDEN_WORD_STEPS] == 0) {
					wlx_blx_pass[wl * RDEN_WORD_STEPS] = 1;
				} else {
					passing = 1;
				}
			}
			if (fifo_result[1] == 0) {
				if (wlx_blx_pass[wl * RDEN_WORD_STEPS + 1] == 0) {
					wlx_blx_pass[wl * RDEN_WORD_STEPS + 1] = 1;
				} else {
					passing = 1;
				}
			}
			/* not passing yet, re-iterate */
			if (passing == 0) {
				if (word_setting < 63) {
					word_setting = word_setting + 1;
					data = 0;
					SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
						VDL_OVRIDE_BYTE_RD_EN, ovr_en, 1);
					SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
						VDL_OVRIDE_BYTE_RD_EN, ovr_force, 1);
					SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
						VDL_OVRIDE_BYTE_RD_EN, ovr_step, word_setting);
					tb_w(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE_RD_EN
						+ memsys_reg_offset, data);
				} else {
					byte_setting = byte_setting + 1;
					data = 0;
					SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
						VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_en, 1);
					SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
						VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_force, 1);
#if RD_EN_BYTE_SEL_CHANGE
					SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
						VDL_OVRIDE_BYTE0_BIT_RD_EN, byte_sel, 1);
#endif
					SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
						VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_step, byte_setting);
					tb_w(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN
						+ memsys_reg_offset, data);
					tb_w(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN
						+ memsys_reg_offset, data);
					if (byte_setting > 64) {
						return ERR_RDEN_BYTE_TO;
					}
				}
			}
		} /* end while */

		wlx_start[wl] = GET_FIELD(tb_r(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE_RD_EN
			+ memsys_reg_offset), DDR40_CORE_PHY_WORD_LANE_0,
			VDL_OVRIDE_BYTE_RD_EN, ovr_step);
		if (wlx_blx_pass[wl * RDEN_WORD_STEPS] == 1) {
			wlx_blx_start[wl * RDEN_WORD_STEPS] = GET_FIELD(
				tb_r(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN
				+ memsys_reg_offset),
				DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_step);
			passing = 0;
			while (passing == 0) {
				if (passing == 0) {
					byte_setting = byte_setting + 1;
					if (byte_setting > 64) {
						return ERR_RDEN_BYTE1_TO;
					}
				}
				data = 0;
				SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
					VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_en, 1);
				SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
					VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_force, 1);
#if RD_EN_BYTE_SEL_CHANGE
				SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
					VDL_OVRIDE_BYTE0_BIT_RD_EN, byte_sel, 1);
#endif
				SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
					VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_step, byte_setting);

				tb_w(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN
					+ memsys_reg_offset, data);
				mtest_verify(memc_reg_base, mem_test_base, data_expected, 0, wl);

				/* Do we need to read status of another fifo? */
				fifo_result[0] = GET_FIELD(
					tb_r(DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_STATUS
					+ memsys_reg_offset),
					DDR40_CORE_PHY_WORD_LANE_0, READ_FIFO_STATUS, status0);
				fifo_result[1] = GET_FIELD(
					tb_r(DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_STATUS
					+ memsys_reg_offset),
					DDR40_CORE_PHY_WORD_LANE_0, READ_FIFO_STATUS, status1);
				/* Clear the FIFO status for this WL */
				tb_w(DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_CLEAR
					+ memsys_reg_offset,
					DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_CLEAR_clear_MASK);

				timeout_ns(TIMEOUT_VAL);

				if (fifo_result[1] == 0) {
					passing = 1;
				}
			} /* end while */

			/* (1, or 3) */
			wlx_blx_start[wl * RDEN_WORD_STEPS + 1] = GET_FIELD(
				tb_r(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN
				+ memsys_reg_offset),
				DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE1_BIT_RD_EN, ovr_step);
		}
		if (wlx_blx_pass[wl * RDEN_WORD_STEPS + 1] == 1) {
			wlx_blx_start[wl * RDEN_WORD_STEPS + 1] = GET_FIELD(
				tb_r(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN
				+ memsys_reg_offset),
				DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE1_BIT_RD_EN, ovr_step);
			passing = 0;
			while (passing == 0) {
				if (passing == 0) {
					byte_setting = byte_setting + 1;
					if (byte_setting > 64) {
						return ERR_RDEN_BYTE0_TO;
					}
				}
				data = 0;
				SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
					VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_en, 1);
				SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
					VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_force, 1);
#if RD_EN_BYTE_SEL_CHANGE
				SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
					VDL_OVRIDE_BYTE0_BIT_RD_EN, byte_sel, 1);
#endif
				SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
					VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_step, byte_setting);
				tb_w(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN
					+ memsys_reg_offset, data);

				mtest_verify(memc_reg_base, mem_test_base, data_expected, 0, wl);

				fifo_result[0] = GET_FIELD(
					tb_r(DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_STATUS
					+ memsys_reg_offset),
					DDR40_CORE_PHY_WORD_LANE_0, READ_FIFO_STATUS, status0);
				/* Do we need to read status of another fifo? */
				fifo_result[1] = GET_FIELD(
					tb_r(DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_STATUS
					+ memsys_reg_offset),
					DDR40_CORE_PHY_WORD_LANE_0, READ_FIFO_STATUS, status1);
				/* Clear the FIFO status */
				tb_w(DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_CLEAR + memsys_reg_offset,
					DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_CLEAR_clear_MASK);

				timeout_ns(TIMEOUT_VAL);

				if (fifo_result[0] == 0) {
					passing = 1;
				}
			} /* end while */

			wlx_blx_start[wl * RDEN_WORD_STEPS] = GET_FIELD(
				tb_r(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN
				+ memsys_reg_offset),
				DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_step);
		}

		word_new_step[wl] = wlx_start[wl] + ticks_to_add;

		if (word_new_step[wl] > 63) {
			common = word_new_step[wl] - 63;
			word_new_step[wl] = 63;
			byte_new_step[wl * RDEN_WORD_STEPS] =
				wlx_blx_start[wl * RDEN_WORD_STEPS] + common;
			byte_new_step[wl * RDEN_WORD_STEPS + 1] =
				wlx_blx_start[wl * RDEN_WORD_STEPS + 1] + common;
		} else {
			byte_new_step[wl * RDEN_WORD_STEPS] =
				wlx_blx_start[wl * RDEN_WORD_STEPS];
			byte_new_step[wl * RDEN_WORD_STEPS + 1] =
				wlx_blx_start[wl * RDEN_WORD_STEPS + 1];
		}
		if (byte_new_step[wl * RDEN_WORD_STEPS] > 63) {
			byte_new_step[wl * RDEN_WORD_STEPS] = 63;
		}
		if (byte_new_step[wl * RDEN_WORD_STEPS + 1] > 63) {
			byte_new_step[wl * RDEN_WORD_STEPS + 1] = 63;
		}
		/* end word lane 0 */
		/* END do per WL */   
	}

	return retval;
}

/* Inputs:
 *   unsigned int phy_reg_base     base offset for PHY
 *   unsigned int memc_reg_base    base offset for MEMC
 *   unsigned int option           shmoo runtime options 
 *                                   [23:16] : log2(MEM_SIZE)-1.  I.E. 256MB => 27
 *                                   [15:08] : width  32 or 16  
 *                                   [07:00] : run time shmoo options
 *                                            0 = run RD delay shmoo
 *                                            1 = run RD Enable  shmoo 
 *                                            2 = run RD DQS shmoo
 *                                            3 = run WR DQ shmoo
 *                                            4 = run WR DM delay shmoo
 *                                            5 = run Addr Calib shmoo
 * 
 *   unsigned int mem_test_base)   memory area for WR/RD testing
 */

unsigned int all_shmoo_RE_byte_FIFO(unsigned long phy_reg_base, unsigned long memc_reg_base,
	unsigned int option, unsigned long mem_test_base)
{
	unsigned int  retval = SHMOO_NO_ERROR;
	unsigned int  data;
	unsigned int  word_new_step[WORD_STEP_COUNT];
	unsigned int  byte_new_step[BYTE_STEP_COUNT];
	unsigned long  memsys_reg_offset = 0;
	unsigned int  wl;
	pack328_t packname;

#ifdef ESMT_SUPPORT
	/*check RT-AC68U boot up with 38 3 */
	/* New DDR3, RE VDL >=43 will boot up panic */
	unsigned int ddr3_revdlw_ovr=38;
	unsigned int ddr3_revdlb_ovr=0;
#endif

	/* To make compiler happy - start */
	unsigned long mem_test_base_ = mem_test_base;

	mem_test_base_++;
	/* To make compiler happy - end */

	packname.pack32 = 0x4e454452; /* RDEN */

	one_shmoo_RE_FIFO(word_new_step, byte_new_step, option, phy_reg_base, memc_reg_base,
		mem_test_base);

#ifdef ESMT_SUPPORT
	if(ddr3_revdlw_ovr) {
		word_new_step[0] = ddr3_revdlw_ovr;
		word_new_step[1] = ddr3_revdlw_ovr;
	}
	if(ddr3_revdlb_ovr) {
		byte_new_step[0] = ddr3_revdlb_ovr;
		byte_new_step[1] = ddr3_revdlb_ovr;
	}
#endif

	for (wl = 0; wl < (((option >> 8) & 0xFF) / BITS_PER_WL); wl ++) {
		memsys_reg_offset = wl * PHY_REG_OFFSET_PER_WL + phy_reg_base
			+ GLOBAL_REG_RBUS_START;
		/* write back BYTE_RD_EN values: 2 new values, one per WL */
		data = 0;
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN, ovr_en, 1);
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN, ovr_force, 1);
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN, ovr_step,
			word_new_step[wl]);
		plot_name(packname, wl);
		plot_dec_number(word_new_step[wl]);	/* NAME & step */
#ifdef ESMT_SUPPORT
		UART_OUT(SPACE);
		plot_dec_number(byte_new_step[wl]);	/* NAME & step */
#endif
		tb_w(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE_RD_EN + memsys_reg_offset, data);

		/* write back BIT_RD_EN values: 4 new values, 2 per WL (0,1) (2,3) */
		data = 0;
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN,
			ovr_en, 1);
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN,
			ovr_force, 1);
#if RD_EN_BYTE_SEL_CHANGE
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN,
			byte_sel, 1);
#endif
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_step,
			byte_new_step[wl * BYTE_BIT_BASE]);
		tb_w(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN + memsys_reg_offset,
			data);
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN,
			ovr_step, byte_new_step[wl * BYTE_BIT_BASE + 1]);
		tb_w(DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN + memsys_reg_offset,
			data);
	}
	NL();

	return retval;
}

/** plot_result()
 * 
 *   CKD:
 *   result is an array of 64 - each bit represents PASS(0)/FAIL(1) of a targeted shmoo item
 *   The organization of result can be looked at as a verticalized bit map (instead of horizontal)
 *   Since there are two WL, so only the LS 16 bits are used to store PASS/FAIL result.
 *   If all 64 result INT are stack in a column, the shmoo plot matches the 1 & 0 of the data
 *   if they rotate by -90 degrees
 *   The STEP information - depends on if this is a BYTE plot, or a BIT plot.
 *   For a BYTE plot, the X is the same for the 8 bits of either LS or MS byte
 *   For a BIT plot, the X is driven individually - and therefore required an array size of 16
 *   (instead of 2)
 *   marker2:  if non-zero, and in range of 0-63 it will plot a special marker 'S' to all bits
 *
 * 
 *           0 1 ..  ..   62  64         
 *        0
 *        1
 *        
 *       30
 *       31               62  64
 *           ^                ^
 *          result[0]         result[63]
 */  
static void plot_result(unsigned int *result, unsigned int *step, unsigned int bit_count,
	int marker2)
{
	unsigned int i;
	unsigned int bit;
	char str[65];

	NL();
	NL();
	UART_OUT(SPACE);
	UART_OUT(SPACE);
	UART_OUT(SPACE);
	UART_OUT(SPACE);
	for (i = 0; i < 64; i++)
		UART_OUT('0' + (i/10));
	NL();
	UART_OUT(SPACE);
	UART_OUT(SPACE);
	UART_OUT(SPACE);
	UART_OUT(SPACE);
	for (i = 0; i < 64; i++)
		UART_OUT('0' + (i%10));
	NL();
	for (bit = 0; bit < bit_count; bit++) {
		if (bit <= 9) {
			UART_OUT(SPACE);
			UART_OUT(ZERO);
			UART_OUT(bit + ZERO);
		} else {
			UART_OUT(SPACE);
			UART_OUT(ONE);
			UART_OUT(ZERO + (bit-10));
		}
		UART_OUT(SPACE);
		for (i = 0; i < 64; i++) {
			if (i == step[bit])
				str[i] = 'X';
			else if (i == (unsigned int)marker2)
				str[i] = 'S';
			else
				str[i] = (result[i] & (1 << bit))? '-' : '+';
			UART_OUT(str[i]);
		}
		NL();
	}
	NL();
}

static unsigned int calib_wr_dq(unsigned int *result, int *init_step, int *new_step,
	unsigned long phy_reg_base);
static unsigned int calib_wr_dq(unsigned int *result, int *init_step, int *new_step,
	unsigned long phy_reg_base)
{
	/* Find the first pass, then find the first fail */
	int bit, vdl;
	unsigned int marker;
	int pass_found, pass_vdl = 0;
	int fail_vdl, fail_width = 0, fail_so_far;
	int ticks_per_clock, adjustment;
	int all_passed = FALSE;
	int data;
	unsigned int retval = SHMOO_NO_ERROR;
	int pass_cnt, fail_cnt, last_pass_pos = 0;
	unsigned int last_stat, now_stat, going_up, going_down, win_size, win_size_last = 255;
	int up, down;
	int *init_step_ = init_step;

	init_step_++;
	/*
	 * 110831 CKD:
	 * To calculate the adjustment, we need these:
	 * 1. The vdl position of the last pass just before the fail window starts
	 * 2. The width of the fail window
	 *
	 * A fail window is established when 3 fail in a row is detected
	 *
	 * The resultant plot for each bit can be one of the followings:
	 * case 1 normal:   ++++++++++++++++++++------------++++++++
	 * The fail window starts after the pass window.
	 * case 2 normal:   ------+++++++++++++++++++++---------++++
	 * The fail window is not the first block of fails but the one after the pass window
	 * case 3 all pass: ++++++++++++++++++++++++++++++++++++++++
	 * There is no fail window           
	 * JIRA-14 case 3.1 delayed all pass, with 0 to 3 fail endings:
	 * -----+++++++++++++++++++++++--  (2 trailing fails)
	 * -----++++++++++++++++++++++++-  (1 trailing fail)
	 * -----+++++++++++++++++++++++++  (no trailing fail)  
	 * case 4 all fail: ----------------------------------------
	 * There is no pass window
	 *
	 *
	 * For cases 1 and 2, find the first fail after at least one pass
	 * Then mark the last pass vdl position just before this first fail
	 * Then determine the width of the fail window - we need at least 3 fails in a row
	 * The calibration calculation can be done with this vdl position and the width of 
	 * the fail window
	 *
	 * For case 3 - it is a all pass, the calculation is based on the last valid vdl 
	 * position.  Note that at least 3 fail in a row must be present, otherwise the sequence 
	 * is considered pass. This can happen with a slow chip
	 *
	 * For case 4 - something bad has happened - no amount of shmoo can help
	 * 
	 * Special handling:
	 * There must be 3 fail in a row to establish a fail window.
	 * For instance:    ----++++++++++++-++--+++--------+++++++++
	 * The two fail cases sandwiched between pass do not count as a fail window
	 * The valid fail window starts further down when 3 and more fail show up
	 *
	 */

	/* Iterate for Each BIT in WL (one WL typically  being 16bits) */
	for (bit = 0, marker = 1, all_passed = FALSE; bit < BITS_PER_WL;
		bit++, marker = marker << 1)
	{
		pass_cnt = 0, fail_cnt = 0, last_stat = 0xff;
		going_up = 0xff;
		going_down = 0xff;
		up = 0, down = -1;
		win_size_last = 0, win_size = 0;

		/* scan VDL looking for passing window */
		for (vdl = 0, pass_found = FALSE, fail_so_far = 0; vdl < VDL_STEPS; vdl++) {
			now_stat = (result[vdl] & marker);
			last_stat = (vdl == 0) ? 0xFF : (result[vdl - 1] & marker);
			/* mark fail to pass transition */
			if ((last_stat != PASS_BIT) && (now_stat == PASS_BIT) && (pass_cnt == 0)) {
				pass_cnt ++;
				last_pass_pos = vdl;
			}
			/* detect passing edge */
			if ((last_stat != PASS_BIT) && (now_stat == PASS_BIT)) {
				up = vdl;
			}
			/* detect falling edge */
			if ((last_stat == PASS_BIT) && (now_stat != PASS_BIT)) {
				down = vdl;
			}

			if (down >= up) {
				win_size = down-up;
			}

			if ((win_size > K_WIN_SIZE) && (win_size >= win_size_last)) {
				going_up = up;
				going_down = down;
				win_size_last = win_size;
				up = 0; down = -1; win_size = 0;
			}

			if (now_stat == PASS_BIT) {
				pass_found = TRUE;
				pass_vdl = vdl;
				fail_so_far = 0;
			}
			if (now_stat != PASS_BIT) {
				if (pass_found && (win_size_last > K_WIN_SIZE)) {
					if (fail_so_far++ >= FAIL_TO_LOOK_FOR) {
						break;
					}
				}
			}
		} /* for */

		/* Find Failing Window size (comes right after passing window) */
		if ((vdl == VDL_END) && (win_size > K_WIN_SIZE)) {
			vdl = VDL_STEPS - 1;
			fail_vdl = VDL_STEPS;
			fail_width = 0;
			all_passed = TRUE;
		} else {
			all_passed = FALSE;
			vdl = vdl - FAIL_TO_LOOK_FOR; /* just to the last pass */
			fail_vdl = vdl;
			/* next, find the width of the failed window
			 * continue with the current vdl
			 * fail criteria is N fail in a row (N = 3)
			 */
			for (fail_width = 0; vdl < VDL_STEPS; vdl++, fail_width++) {
				if ((result[vdl] & marker) == PASS_BIT) {
					break;
				}
			}
		}

		/* This is the case of all fail */
		if (fail_width == VDL_STEPS) {
			fail_vdl = VDL_STEPS;
			fail_width = 0;
		}
		if (all_passed == FALSE) {
			/* get ticks_per_clock from PHY CR VDL_CALIB_STATUS reg */
			data = RDREG(phy_reg_base + DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIB_STATUS);
			ticks_per_clock = GET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS,
				VDL_CALIB_STATUS, calib_total);
			/* half_ticks size is ticks_per_clock / 2, so we do a shift right by 1 */
			adjustment = (ticks_per_clock >> 1) - fail_width;
			/* now either take 2/3 or 1/2 of the adjustment */
#if WR_33PER_CALIB
			/* use repeat subtraction for divide
			 * to avoid issue with floating point
			 */
			dsr = 3;
			val = adjustment;
			adjustment = 0;
			while (val >= dsr) {
				adjustment ++;
				val -= dsr;
			}
			adjustment = adjustment << 1; /* x2 now */
			/* The net result is 2/3 of adjustment */
#endif
#if WR_50PER_CALIB
			adjustment = ((ticks_per_clock >> 1) - fail_width) >> 1; /* divide by 2 */
			/* The net result is 1/2 of adjustment */
#endif
			/* if there is no fails leading up to pass .. */
			if (going_up == 0xff) {
				fail_vdl -= 1;
				/* we need the position of the last pass just
				 * before the fail window
				 */
				new_step[bit] = fail_vdl - adjustment;
			} else {
				/* center to the window */
				new_step[bit] = (going_up + going_down) / 2;
			}

			/* JIRA-14 range check */
			if (new_step[bit] > VDL_STEPS) {
				new_step[bit] = VDL_STEPS - 1;
			}
			if (new_step[bit] < 0) {
				new_step[bit] = 0;
			}
			/* JIRA-14 error checking - make sure the VDL we choose is not
			 * a failing one
			 * if so, pick the fail-pass transition
			 */
			if ((result[new_step[bit]] & marker) != PASS_BIT) {
				new_step[bit] = last_pass_pos;
			}
		}
		/* case 3: all pass - take half of vdl from pass point to the end
		 * as the adjustment
		 */
		else {
			new_step [bit] = (fail_vdl + last_pass_pos) / 2; /* JIRA-14 */
		}
	}

	return retval;
}


/* Make a shmoo with the write dq vdl values */
static unsigned int shmoo_wr_dq(int wl, unsigned int *result, unsigned long phy_reg_base,
	unsigned long memc_reg_base, unsigned long mtest_addr, unsigned int mt_mode);

static unsigned int shmoo_wr_dq(int wl, unsigned int *result, unsigned long phy_reg_base,
	unsigned long memc_reg_base, unsigned long mtest_addr, unsigned int mt_mode)
{
	unsigned retval = SHMOO_NO_ERROR;
	unsigned data, i, bl;
	unsigned long addr;
	unsigned setting;
	/* Do we need m? */
	int m;
#if (SHMOO_PLATFORM == SHMOO_PLATFORM_S2) || (SHMOO_PLATFORM == SHMOO_PLATFORM_S2_A)
	static unsigned data_expected[64 * 4];
#else
	int const size = 64;
	int const pages = PAGE;
	unsigned data_expected[size * pages];
#endif
#if USE_MIPS
	unsigned val[WR_DQS_DATA_SIZE];

	mtest_fill(val, WR_DQS_DATA_SIZE);
#endif

	for (setting = 0; setting < 64; setting++) { /* for each step */
		/* write setting to PHY WL{0,1} VDL_OVRIDE_BYTE{0,1}_BIT{0...7}_W reg */
		data = SET_OVR_STEP(setting);
		for (bl = 0; bl < BL_PER_WL; bl++) {
			addr = wl * PHY_REG_OFFSET_PER_WL + PHY_REG_OFFSET_PER_BL * bl +
				phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT0_W;
			for (i = 0; i < 8; i++) {
				WRREG(addr + (i * 4), data);
			}
		}

		timeout_ns(TIMEOUT_VAL);
		m = setting & 0x7;
		result[setting] = mtest_verify(memc_reg_base, mtest_addr, data_expected,
			mt_mode, wl);
	}

	return retval;
}

unsigned int generic_shmoo_calib_wr_dq(unsigned long phy_reg_base, unsigned long memc_reg_base,
	unsigned int option, unsigned long mem_test_base)
{
	unsigned int retval = SHMOO_NO_ERROR;
	unsigned int wl;
	int i, bl, n;
	unsigned long a;
	unsigned data = 0, en, step;
	unsigned result[64];
	int init_step[16];
	int new_step[16];
	unsigned wr_calib_total;
	pack328_t packname;

	packname.pack32 = 0x51445257; /* WRDQ */

	for (wl = 0; wl < (((option >> 8) & 0xFF) / BITS_PER_WL); wl++) {
		/* get wr_calib_total from PHY CR VDL_WR_CHAN_CALIB_STATUS reg */ 
		data = RDREG(phy_reg_base + DDR40_CORE_PHY_CONTROL_REGS_VDL_WR_CHAN_CALIB_STATUS);
		wr_calib_total = GET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS,
			VDL_WR_CHAN_CALIB_STATUS, wr_chan_calib_total) >> 4;
		/* set init_step from PHY WL{0,1} VDL_OVRIDE_BYTE{0,1}_BIT{0...7}_W reg */
		n = 0;
		for (bl = 0; bl < BL_PER_WL; bl++) {
			a = wl * PHY_REG_OFFSET_PER_WL + PHY_REG_OFFSET_PER_BL * bl +
				phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT0_W;

			for (i = 0; i < 8; i++) {
				data = RDREG(a + (i * 4));
				en = GET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
					VDL_OVRIDE_BYTE0_BIT0_W, ovr_en);
				step = GET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
					VDL_OVRIDE_BYTE0_BIT0_W, ovr_step);
				init_step[n] = (en)? step : wr_calib_total;
				n++;
			}
		}

		retval |= shmoo_wr_dq(wl, result, phy_reg_base, memc_reg_base, mem_test_base, 0);
		/* printf("generic_shmoo_calib_wr_dq: adjust last result entry\n"); */
		result[63] = 0xFFFF;
		retval |= calib_wr_dq(result, init_step, new_step, phy_reg_base);

		/* set new_step to PHY WL{0,1} OVRIDE_BYTE{0,1}_BIT{0...7}_W regs */    
		n = 0;
		for (bl = 0; bl < BL_PER_WL; bl++) {
			a = wl * PHY_REG_OFFSET_PER_WL + PHY_REG_OFFSET_PER_BL * bl +
				phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT0_W;

			for (i = 0; i < 8; i++) {
				data = SET_OVR_STEP(new_step[n]);
				WRREG(a + (i * 4), data);
				n++;
			}
		}

		plot_name(packname, wl);
		plot_result(result, (unsigned int *)new_step, DEFAULT_BIT_COUNT, -1); /* RESULT */
	}

	return retval;
}

/* Make a shmoo with the read dqs vdl values */
static unsigned generic_shmoo_rd_dqs(int wl, unsigned int *result, int start_step,
	unsigned long phy_reg_base, unsigned long memc_reg_base, unsigned long mtest_addr,
	unsigned int mt_mode);
static unsigned generic_shmoo_rd_dqs(int wl, unsigned int * result, int start_step,
	unsigned long phy_reg_base, unsigned long memc_reg_base, unsigned long mtest_addr,
	unsigned int mt_mode)
{
	unsigned int retval = SHMOO_NO_ERROR;
	int bl, bit;
	unsigned int data;
	unsigned dq_setting;
	unsigned int rd_en_setting[2];
	unsigned dqs_setting;
	int all_fail;
#if (SHMOO_PLATFORM == SHMOO_PLATFORM_S2) || (SHMOO_PLATFORM == SHMOO_PLATFORM_S2_A)
	static unsigned data_expected[NUM_DATA];
#else
	unsigned data_expected[NUM_DATA];
#endif
	unsigned long byte_bit_rd_en_addr;
	unsigned long byte_bit_rp_addr;
	unsigned long byte_bit_rn_addr;
	/* Do we need loop_count ? */
	unsigned loop_count = 0;
	/* To make compiler happy - start */
	int wl_ = wl;
	unsigned *result_ = result;
	int start_step_ = start_step;

	wl_++;
	result_++;
	start_step_++;
	/* To make compiler happy - end */

	/* get rd_en_setting from PHY WL{0,1} VDL_OVRIDE_BYTE{0,1}_BIT_RD_EN reg */
	for (bl = 0; bl < BL_PER_WL; bl++) {
		byte_bit_rd_en_addr = (wl * PHY_REG_OFFSET_PER_WL) + phy_reg_base +
			DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN +
			PHY_REG_OFFSET_PER_BL * bl;
		data = RDREG(byte_bit_rd_en_addr);
		rd_en_setting[bl] = GET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0,
			VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_step);
	}

#if USE_MIPS
	mtest_fill_write(data_expected, NUM_DATA, mtest_addr, mt_mode);
#endif

	dq_setting = 0;
	data = SET_OVR_STEP(dq_setting);
	for (bl = 0; bl < BL_PER_WL; bl++) {
		byte_bit_rp_addr = (wl * PHY_REG_OFFSET_PER_WL) + phy_reg_base +
			DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT0_R_P +
			PHY_REG_OFFSET_PER_BL * bl;
		byte_bit_rn_addr = (wl * PHY_REG_OFFSET_PER_WL) + phy_reg_base +
			DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT0_R_N +
			PHY_REG_OFFSET_PER_BL * bl;
		for (bit = 0; bit < 8; bit++) {
			WRREG(byte_bit_rp_addr + (bit * 8), data);
			WRREG(byte_bit_rn_addr + (bit * 8), data);
		}
	}

	/* get dqs_setting from PHY CR VDL_DQ_CALIB_STATUS reg */
	data = RDREG(phy_reg_base +
		DDR40_CORE_PHY_CONTROL_REGS_VDL_DQ_CALIB_STATUS); /* DQCS_PCR */
	dqs_setting = GET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS, VDL_DQ_CALIB_STATUS,
		dqs_calib_total) >> 4;

	all_fail = 0;
	while (all_fail == 0) {
		timeout_ns(TIMEOUT_VAL);
		result[dq_setting] = mtest_verify(memc_reg_base, mtest_addr, data_expected,
			mt_mode, wl);
		if (result[dq_setting] == 0xFFFF) {
			all_fail = 1;
		}
		else {
			dqs_setting = dqs_setting + 1;
			/* 110811 change per 230 result */
			if (dqs_setting >= 63) {
				all_fail = 1;
			}
			/* write dqs_setting to PHY WL{0,1} VDL_OVRIDE_BYTE{0,1}_R_{P,N} */
			data = SET_OVR_STEP(dqs_setting);
			data |= (1 << 8); /* byte_sel = 1; */
			/* byte0 RP */
			WRREG((wl * PHY_REG_OFFSET_PER_WL + phy_reg_base +
				DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_R_P), data);
			/* byte0 RN */
			WRREG((wl * PHY_REG_OFFSET_PER_WL + phy_reg_base +
				DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_R_P + 4), data);
			/* byte1 RP JIRA-11 */
			WRREG((wl * PHY_REG_OFFSET_PER_WL + phy_reg_base +
				DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_R_P), data);
			/* byte1 RN JIRA-11 */
			WRREG((wl * PHY_REG_OFFSET_PER_WL + phy_reg_base +
				DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_R_P + 4), data);
			for (bl = 0; bl < BL_PER_WL; bl++) { /* for each byte */
				rd_en_setting[bl] = rd_en_setting[bl] + 1;
				/* 1108011 change per 230 */
				if (rd_en_setting[bl] >= 63) {
					all_fail = 1;
				}
				/* write rd_en_setting to PHY WL{0,1}
				 * VDL_OVRIDE_BYTE{0,1}_BIT_RD_EN reg
				 */
				data = SET_OVR_STEP(rd_en_setting[bl]);
				/* Start TODD 200911 need byte_sel */
				data |= (1 << 8); /* byte_sel = 1; */
				/* Finish TODD 200911 need byte_sel */
				WRREG(wl * PHY_REG_OFFSET_PER_WL + phy_reg_base +
					DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN +
					PHY_REG_OFFSET_PER_BL * bl, data);
			}
		}
		loop_count++;
	}

	return retval;
}

/* Calibrate the read dqs vdl values */
static unsigned calib_rd_dqs(unsigned int *result, int *init_step, int *new_step);

static unsigned calib_rd_dqs(unsigned int *result, int *init_step, int *new_step)
{
	unsigned retval = SHMOO_NO_ERROR;
	int i, inx;
	int bit_result_sum = 0;
	int bit_total_failed;
	int bit;
	int pass_bit_window[16];
	int start;
	int finish;
	int pass_start[16];
	int fail_start[16];
	/* To make compiler happy - start */
	int *init_step_ = init_step;

	init_step_++;
	/* To make compiler happy - end */

	/* determine pass window by finding start of passes and end of passes
	 * splitting the visible pass window
	 */
	for (bit = 0; bit < 16; ++bit) { /* for each bit */
		pass_bit_window[bit] = 0;
		i = 0;
		start = 1;
		finish = 1;
		/* cycle thru bit value until 1st pass is hit */
		while (start == 1) {
			for (inx = 0, bit_result_sum = 0; inx < MAX_BIT_TO_CHECK; inx++) {
				/* check the next 6 bit results (a fail is a 1) expect
				 * all results to be 0 for a pass
				 */
				bit_result_sum += result[i + inx] & (1 << bit);
			}
			/* bit_result_sum = 0 if all bit results are pass,
			 * otherwise it is non-zero
			 */
			if (bit_result_sum == 0) {
				pass_start[bit] = i;
				start = 0;
			}
			i = i + 1;
			if (i >= 63) {
				pass_start[bit] = 64;
				i++;
				break;
			}
		}
		if (i > 63) {
			fail_start[bit] = 64;
			finish = 0;
		}

		while (finish == 1) {
			/* accumulate the failed result, expected all result bits to fail */
			for (inx = 0, bit_total_failed = 1; inx < FAIL_BIT_TO_CHECK; inx ++) {
				if ((result[i + inx] & (1 << bit)) == 0) {
					/* if any one of the result is a pass,
					 * then we are not there yet
					 */
					bit_total_failed = 0;
					break;
				}
			}
			/* if bit_total_failed is still true, the all results have failed */
			if (bit_total_failed || (i == 63)) {
				fail_start[bit] = i;
				finish = 0;
			}

			i = i + 1;
			if (i >= 63) {
				fail_start[bit] = 64;
				break;
			}
		}
		pass_bit_window[bit] = fail_start[bit] - pass_start[bit];
	}

	for (bit = 0; bit < 16; ++bit) {
		new_step[bit] = pass_bit_window[bit] >> 1;
		new_step[bit] += pass_start[bit];
		if (new_step[bit] < 0) {
			new_step[bit] = 0;
		}
		if (new_step[bit] > 63) {
			new_step[bit] = 63;
		}
	}

	return retval;
}

/* Return :  0 = pass
 *           non-zero = failed
 */
static int wr_dqs_setting(int wl, unsigned *result, int *init_step, int *new_step,
	unsigned long phy_reg_base, unsigned long memc_reg_base, unsigned long mtest_addr,
	int start_point, int which);
static int wr_dqs_setting(int wl, unsigned *result, int *init_step, int *new_step,
	unsigned long phy_reg_base, unsigned long memc_reg_base, unsigned long mtest_addr,
	int start_point, int which)
{
	int dq_setting, bit, bl, n, i;
	unsigned int data;
	unsigned long byte_bit_rp_addr;
	unsigned long byte_bit_rn_addr;
	int start, delta;
	int status = 0;
	/* To make compiler happy - start */
	int start_point_ = start_point;

	start_point_++;
	/* To make compiler happy - end */

	/* single up sweep */
	for (dq_setting = 0; dq_setting < 64; dq_setting++) { /* for each setting */
		/* write dq_setting to PHY WL{0,1} VDL_OVRIDE_BYTE{0,1}_BIT{0,...,7}_R_{P,N} */
		data = SET_OVR_STEP(dq_setting);
		for (bl = 0; bl < BL_PER_WL; bl++) { /* for each byte */
			byte_bit_rp_addr = (wl * PHY_REG_OFFSET_PER_WL) + phy_reg_base +
				DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT0_R_P +
				PHY_REG_OFFSET_PER_BL * bl;
			byte_bit_rn_addr = (wl * PHY_REG_OFFSET_PER_WL) + phy_reg_base +
				DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT0_R_N +
				PHY_REG_OFFSET_PER_BL * bl;
			for (bit = 0; bit < 8; bit++) {
				if (which & 1) {
					WRREG(byte_bit_rp_addr + (bit * 8), data);
				}
				if (which & 2) {
					WRREG(byte_bit_rn_addr + (bit * 8), data);
				}
			}
		}
		timeout_ns(TIMEOUT_VAL);
		result[dq_setting] = mtest_verify(memc_reg_base, mtest_addr, 0, 0, wl);
	}

	/* write new_step to PHY WL{0,1} VDL_BYTE{0,1}_BIT{0...7}_R_{P,N} regs
	 *
	 * P + N
	 */
	if (which == 3) {
		start = 0;
		delta = 1;
	}
	/* P */
	else if (which == 1) {
		start = 0;
		delta = 2;
	}
	/* N */
	else {
		start = 1;
		delta = 2;
	}

	calib_rd_dqs(result, init_step, new_step);
	n = 0;
	for (bl = 0; bl < BL_PER_WL; bl++) {
		byte_bit_rp_addr = wl * PHY_REG_OFFSET_PER_WL + phy_reg_base +
			DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT0_R_P +
			PHY_REG_OFFSET_PER_BL * bl;
		for (i = start; i < 16; i += delta) { /* 8 R_Ps and 8 R_Ns */
			/* check for erroneous value: min or max */
			if ((new_step[n] == 0) || (new_step[n] >= 63)) {
				printf("ERR_BAD_VDL\n");
				return ERR_BAD_VDL;
			}
			data = SET_OVR_STEP(new_step[n]);
			WRREG(byte_bit_rp_addr + (i * 4), data);
			if (which == 3) {
				if (i & 0x1) {
					n++;
				}
			}
			else {
				n++;
			}
		}
	}

	return status;
}


unsigned generic_shmoo_calib_rd_dqs(unsigned long phy_reg_base,
                                     unsigned long memc_reg_base,
                                     unsigned int option,
                                     unsigned long mem_test_base)
{
	unsigned retval = SHMOO_NO_ERROR;
	unsigned int data;
	unsigned result[64];
	int init_step[16];
	int new_step[16];
	int start_step;
	unsigned int dq_calib_total;
	unsigned int wl;
	pack328_t packname;

	for (wl = 0; wl < (((option >> 8) & 0xFF) / BITS_PER_WL); wl++) {
	/* get dq_calib_total from PHY CR VDL_DQ_CALIB_STATUS reg */
		data = RDREG(phy_reg_base + DDR40_CORE_PHY_CONTROL_REGS_VDL_DQ_CALIB_STATUS);
		dq_calib_total = GET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS, VDL_DQ_CALIB_STATUS,
			dq_calib_total) >> 4;
		start_step = 0;
		/* DQS Calibration */
		retval |= generic_shmoo_rd_dqs(wl, result, start_step, phy_reg_base,
			memc_reg_base, mem_test_base, 0);
		retval |= wr_dqs_setting(wl, result, init_step, new_step, phy_reg_base,
			memc_reg_base, mem_test_base, start_step, 3 /* P + N */);
		packname.pack32 = 0x53514452; /* RDQS */
		plot_name(packname, wl);
		plot_result(result, (unsigned int *)new_step, DEFAULT_BIT_COUNT, -1); /* RESULT */ 
		/* DQS POS EDGE Calibration */
		retval |= wr_dqs_setting(wl, result, init_step, new_step, phy_reg_base,
			memc_reg_base, mem_test_base, start_step, 1 /* P */ );
		packname.pack32 = 0x50; /* P */
		plot_name(packname, wl);
		plot_result(result, (unsigned int*)new_step, DEFAULT_BIT_COUNT, -1); /* RESULT */ 
		/* DQS NEG EDGE Calibration */
		retval |= wr_dqs_setting(wl, result, init_step, new_step, phy_reg_base,
			memc_reg_base, mem_test_base, start_step, 2 /* N */); 
		packname.pack32 = 0x4E; /* N */
		plot_name(packname, wl);
		plot_result(result, (unsigned int*)new_step, DEFAULT_BIT_COUNT, -1);
	}

	return retval;
}

/* at_size is the MS bit to test (27 for 256MB, 28 for 512 MB) */
unsigned run_addr_test(unsigned long memc_reg_base, unsigned long at_start, unsigned *data,
	unsigned int mt_mode, unsigned int at_size, unsigned int wrd_ln, unsigned int high_stress);
unsigned run_addr_test(unsigned long memc_reg_base, unsigned long at_start, unsigned *data,
	unsigned int mt_mode, unsigned int at_size, unsigned int wrd_ln, unsigned int high_stress)
{
	unsigned long a;
	unsigned int size = (unsigned int)at_size /* TRICK */, result = 0, i;

	if (high_stress == 3)
		size = 16 * 1024  * 1024;
	else if (high_stress == 2)
		size =  8 * 1024  * 1024;
	else if (high_stress == 1)
		size =  1 * 1024  * 1024;
	else
		size = 2*1024;
	a = (0xa6a000 - 32) | at_start;
	for (i = 0; i < 20; i++) {
		result = verify_mem_block_mips(memc_reg_base, a, size, mt_mode, wrd_ln);
		if (result) {
			break;
		}
		a += size;
	}
	/* re-write MRS */
	rewrite_dram_mode_regs(sih_saved);

	return result;
}

/* results[64] :  a 1 means failed., 0 pass. */
unsigned generic_shmoo_calib_addr(unsigned long phy_reg_base, unsigned long memc_reg_base,
	unsigned int option, unsigned long mem_test_base)
{
	int i;
	int wl;
	unsigned calib_vdl_step;
	unsigned new_vdl_step;
	unsigned start_vdl_val;
	unsigned pass_start, pass_start_0 = 0, pass_start_1 = 0;
	unsigned pass_end,   pass_end_0 = 0,   pass_end_1 = 64;
	unsigned result[64];
#if SHMOO_PLATFORM == SHMOO_PLATFORM_S2 || (SHMOO_PLATFORM == SHMOO_PLATFORM_S2_A)
	/* Remove array from the stack */
	static unsigned data[MEM_SIZE_INTS + 8];
#else
	unsigned data[MEM_SIZE_INTS + 8];
#endif
	unsigned retval = SHMOO_NO_ERROR;
	pack328_t packname;
	unsigned int wl_count;
	unsigned int high_stress, run_addx_shmoo;
	unsigned long memsys_reg_offset;
	unsigned long memsys_reg_offset1;
	unsigned int initial_vdl, ac_shift_detect, ac_shifted_vdl;

	/* get current VDL overrided setting */
	ac_shifted_vdl = RDREG(phy_reg_base + DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL);
	ac_shifted_vdl &= DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL_ovr_step_MASK;
	ac_shift_detect = RDREG(phy_reg_base + DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL) &
		DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL_ovr_en_MASK;

	high_stress = (option >> 28) & 3;
	run_addx_shmoo = (option & DO_CALIB_ADDR);


	/* Determine VDL calibrated steps */
	calib_vdl_step = RDREG(phy_reg_base + DDR40_CORE_PHY_CONTROL_REGS_VDL_WR_CHAN_CALIB_STATUS);
	calib_vdl_step = calib_vdl_step &
		DDR40_CORE_PHY_CONTROL_REGS_VDL_WR_CHAN_CALIB_STATUS_wr_chan_calib_total_MASK;
	calib_vdl_step = calib_vdl_step >>
		(DDR40_CORE_PHY_CONTROL_REGS_VDL_WR_CHAN_CALIB_STATUS_wr_chan_calib_total_SHIFT +
		4);

	/* if A/C VDL override was detected, use that as the initial VDL */
	if (ac_shift_detect) {
		initial_vdl = ac_shifted_vdl;
	}
	else {
		initial_vdl = calib_vdl_step;
	}

	pass_start = initial_vdl;
	pass_end = initial_vdl;
	new_vdl_step = initial_vdl;

	/* added to fix a hang at code copy that looks like a mode reg issue -
	 * these are in the Toronto address shmoo code - Todd
	 */
	if (run_addx_shmoo) {
		packname.pack32 = 0x52444441; /* ADDR */

		wl_count = (((option >> 8) & 0xFF) / BITS_PER_WL); /* K_PHY_CFG__WC_COUNT */
		memsys_reg_offset = phy_reg_base +  GLOBAL_REG_RBUS_START;
		memsys_reg_offset1 = PHY_REG_OFFSET_PER_WL + memsys_reg_offset;

		/* iterate over Word Lane (1 or 2) */
		for (wl = 0; (unsigned int)wl < wl_count; wl++) {
			start_vdl_val = initial_vdl;
			/* DOWN */
			for (i = start_vdl_val - 1; i >= 0; i--) {
				/* Clear the FIFO status for this WL */
				tb_w(DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_CLEAR +
					memsys_reg_offset, 1);
				if (wl) {
					tb_w(DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_CLEAR +
						memsys_reg_offset1, 0x1);
				}
				/* ovr_en = 1, ovr_step = * */
				WRREG(phy_reg_base +
					DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL,
					(0x10000 | i));
				/* Log2(DDR SIZE) -1  (i.e. 256MB => 27) */
				if (!run_addr_test(memc_reg_base, mem_test_base, data, wl_count,
					(option >> 16)& 0xFF, wl, high_stress))
				{
					pass_start = i;
				}
				else {
					break;
				}
			}
			/* restore starting point */
			WRREG(phy_reg_base + DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL,
				(0x10000 | start_vdl_val));
			/* rewrite DRAM's MRS */
			retval |= rewrite_dram_mode_regs(sih_saved);
			/* UP
			 * start from known good VDL. Then find the end of passing.
			 */
			for (i = start_vdl_val; i < 64; i++) {
				WRREG(phy_reg_base + DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL,
					(0x10000 | i)); /* ovr_en = 1, ovr_step = * */
				/* Clear the FIFO status for this WL */
				tb_w(DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_CLEAR +
					memsys_reg_offset, 1);
				if (wl) {
					tb_w(DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_CLEAR +
						memsys_reg_offset1, 0x1);
				}
				/* Log2(DDR SIZE) -1  (i.e. 256MB => 27) */
				if (!run_addr_test(memc_reg_base, mem_test_base, data, wl_count,
					(option >> 16) & 0xFF, wl, high_stress))
				{
					pass_end = i;
				}
				else {
					break;
				}
			}

			/* restore initial value */
			WRREG(phy_reg_base + DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL,
				(0x10000 | start_vdl_val));
			retval |= rewrite_dram_mode_regs(sih_saved);
			if (wl == 0) {
				pass_start_0 = pass_start;
				pass_end_0 = pass_end;
			}
			else {
				pass_start_1 = pass_start;
				pass_end_1 = pass_end;
			}
		}
		/* do "union" of (pass_start0,pass_end0)  and (pass_start1,pass_end1) */
		pass_start = (pass_start_0 > pass_start_1)? pass_start_0 : pass_start_1;
		/* For a PHY with one WL, pass_end_1 is set to 64, so pass_end_0 will be used */
		pass_end = (pass_end_0 < pass_end_1)? pass_end_0 : pass_end_1;

		if (pass_end > pass_start) {
			new_vdl_step = pass_start + ((pass_end - pass_start) >> 1);
			WRREG(phy_reg_base + DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL,
				(0x10000 | new_vdl_step)); /* ovr_en = 1, ovr_step = * */
		}
		else {
			new_vdl_step = 0;
			WRREG(phy_reg_base + DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL, 0);
		}
		retval |= rewrite_dram_mode_regs(sih_saved);
	}
	else {
		packname.pack32 = 0x72646461; /* addr (lower case) */
	}

	for (i = 0; i < 64; i++) {
		result[i] = 1;
		if (pass_end >= pass_start) {
			if (((unsigned int)i >= pass_start) && ((unsigned int)i <= pass_end)) {
				result[i] = 0;
			}
		}
	}
	plot_name(packname, NO_WL_PRINT);
	plot_result(result, &new_vdl_step, ADDRC_BIT_COUNT, initial_vdl); /* RESULT */

	/* Determine width of pass window. Pass window must be contiguous */
	if (retval != 0) {
		return ERR_ADDR_SHMOO_FAILED;
	}
	else if (new_vdl_step == 0) {
		return ERR_ADDR_SHMOO_FAILED;
	}

	return SHMOO_NO_ERROR;
}


/*
 * FUNCTION: one_shmoo_rd_data_dly_FIFO
 *           It is critical to keep the setting in both word lane the same
 *           Otherwise test_mem_read will fail
 */ 
static inline unsigned one_shmoo_rd_data_dly_FIFO(unsigned int *new_step,
	ddr40_addr_t phy_reg_base, ddr40_addr_t memc_reg_base, unsigned int wl_count,
	ddr40_addr_t mem_test_base);
static inline unsigned one_shmoo_rd_data_dly_FIFO(unsigned int *new_step,
	ddr40_addr_t phy_reg_base, ddr40_addr_t memc_reg_base, unsigned int wl_count,
	ddr40_addr_t  mem_test_base)
{
	unsigned int  retval = SHMOO_NO_ERROR;
	unsigned int  data = 0;
	unsigned int  setting = 1;
	unsigned int  passing = 0;
	unsigned int  wl0_bl0_pass = 0;
	unsigned int  wl0_bl1_pass = 0;
	unsigned int  wl1_bl0_pass = 0;
	unsigned int  wl1_bl1_pass = 0;
	unsigned int  RD_EN_word_setting = 0;
	unsigned int  RD_EN_byte_setting = 0;
	unsigned int  original_RD_EN_setting[4];
	unsigned int  original_RD_EN_BYTE_setting[2];
	unsigned int  fifo_result[4] = {0, 0, 0, 0};
#if (SHMOO_PLATFORM == SHMOO_PLATFORM_S2) || (SHMOO_PLATFORM == SHMOO_PLATFORM_S2_A)
	static unsigned int  data_expected[64 * 4];
#else
	int const size = 64;
	int const pages = PAGE;
	unsigned int  data_expected[size * pages];
#endif
	unsigned int nTest_0, nTest_1;
#if PHY_VER_D
	unsigned int  regval1;
#endif

	/* set initial condition
	 *
	 * initialize with 0 for the wl_count > 1. Can be moved to the else clause,
	 * see the following statement:
	 * if (wl_count > 1) {
	 */
	original_RD_EN_setting[2] = 0;
	original_RD_EN_setting[3] = 0;
	original_RD_EN_BYTE_setting[1] = 0;

#if PHY_VER_D
	/* disable dqs always on */
	regval1 = tb_r(phy_reg_base + DDR40_PHY_WORD_LANE_DRIVE_PAD_CTL_OFFSET +
		GLOBAL_REG_RBUS_START + memsys_reg_offset);
	SET_FIELD(regval1, DDR40_PHY_WORD_LANE, DRIVE_PAD_CTL, DQS_ALWAYS_ON, 0x0);
	tb_w(phy_reg_base + DDR40_PHY_WORD_LANE_DRIVE_PAD_CTL_OFFSET + GLOBAL_REG_RBUS_START +
		memsys_reg_offset, regval1);
	tb_w(phy_reg_base + DDR40_PHY_WORD_LANE_DRIVE_PAD_CTL_OFFSET + PHY_REG_OFFSET_PER_WL +
		GLOBAL_REG_RBUS_START + memsys_reg_offset, regval1);
#endif

	/*
	 * We can remove this code if using declaration with initialization
	 */
	wl0_bl0_pass = 0;
	wl0_bl1_pass = 0;
	wl1_bl0_pass = 0;
	wl1_bl1_pass = 0;

	original_RD_EN_BYTE_setting[0] = tb_r(phy_reg_base +
		DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE_RD_EN + GLOBAL_REG_RBUS_START);
	original_RD_EN_setting[0] = tb_r(phy_reg_base +
		DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN + GLOBAL_REG_RBUS_START);
	original_RD_EN_setting[1] = tb_r(phy_reg_base +
		DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN + GLOBAL_REG_RBUS_START);
	if (wl_count > 1) {
		original_RD_EN_BYTE_setting[1] = tb_r(phy_reg_base +
			DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE_RD_EN +
			PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START);
		original_RD_EN_setting[2] = tb_r(phy_reg_base +
			DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN +
			PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START);
		original_RD_EN_setting[3] = tb_r(phy_reg_base +
			DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN +
			PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START);
	}

	/*
	 * We can remove this code if using declaration with initialization
	 */
	RD_EN_word_setting = 0x0;
	RD_EN_byte_setting = 0x0;

	data = 0x0;
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN, ovr_en, 1);
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN, ovr_force, 1);
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN, ovr_step,
		RD_EN_word_setting);

	tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE_RD_EN +
		GLOBAL_REG_RBUS_START, data);
	if (wl_count > 1) {
		tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE_RD_EN +
			PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START, data);
	}

	data = 0x0;
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_en, 1);
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_force, 1);
#if RD_EN_BYTE_SEL_CHANGE
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN, byte_sel, 1);
#endif
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_step,
		RD_EN_byte_setting);
	tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN +
		GLOBAL_REG_RBUS_START, data);
	tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN +
		GLOBAL_REG_RBUS_START, data);
	if (wl_count > 1) {
		tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN +
			PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START, data);
		tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN +
			PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START, data);
	}


	/*
	 * We can remove this code if using declaration with initialization
	 */
	passing = 0x0;
	setting = 0x1;

	while (passing == 0x0) {
		data = 0x0;
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, READ_DATA_DLY, rd_data_dly, setting);
		tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_READ_DATA_DLY +
			GLOBAL_REG_RBUS_START, data);
		if (wl_count > 1) {
			tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_READ_DATA_DLY +
				PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START, data);
		}
		timeout_ns(TIMEOUT_VAL);
		nTest_0 = mtest_verify(memc_reg_base, mem_test_base, data_expected, 0, 0);
		fifo_result[0] = GET_FIELD(tb_r(phy_reg_base +
			DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_STATUS + GLOBAL_REG_RBUS_START),
			DDR40_CORE_PHY_WORD_LANE_0, READ_FIFO_STATUS, status0);
		fifo_result[1] = GET_FIELD(tb_r(phy_reg_base +
			DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_STATUS +
			GLOBAL_REG_RBUS_START),
			DDR40_CORE_PHY_WORD_LANE_0, READ_FIFO_STATUS, status1);
		if (wl_count  > 1) {
			nTest_1 = mtest_verify(memc_reg_base, mem_test_base, data_expected, 0, 1);
			fifo_result[2] = GET_FIELD(tb_r(phy_reg_base +
				DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_STATUS +
				PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START),
				DDR40_CORE_PHY_WORD_LANE_0, READ_FIFO_STATUS, status0);
			fifo_result[3] = GET_FIELD(tb_r(phy_reg_base +
				DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_STATUS +
				PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START),
				DDR40_CORE_PHY_WORD_LANE_0, READ_FIFO_STATUS, status1);
		}
		/* Clear the FIFO status */
		tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_CLEAR +
			GLOBAL_REG_RBUS_START, 0x1);
		if (wl_count > 1) {
			tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_READ_FIFO_CLEAR +
				PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START, 0x1);
		}
		/* When a byte passes - save that read data delay setting
		 *
		 * Can we use array of 4 elements instead of 4 independent wlx_blx_pass variables ?
		 */
		if (fifo_result[0] == 0x0) {
			/* printf("fifo_result[0] == 0 and nTest_0 = 0x%X\n", nTest_0); */
			if (wl0_bl0_pass == 0x0) {
				wl0_bl0_pass = setting;
			}
			else {
			}
		}
		else {
		}

		if (fifo_result[1] == 0x0) {
			/* printf("fifo_result[1] == 0 and nTest_0 = 0x%X\n", nTest_0); */
			if (wl0_bl1_pass == 0x0) {
				wl0_bl1_pass = setting;
			}
			else {
			}
		}
		else {
		}

		if (fifo_result[2] == 0x0) {
			/* printf("fifo_result[2] == 0 and nTest_1 = 0x%X\n", nTest_1); */
			if (wl1_bl0_pass == 0x0) {
				wl1_bl0_pass = setting;
			}
			else {
			}
		}
		else {
		}

		if (fifo_result[3] == 0x0) {
			/* printf("fifo_result[3] == 0 and nTest_1 = 0x%X\n", nTest_1); */
			if (wl1_bl1_pass == 0x0) {
				wl1_bl1_pass = setting;
			}
			else {
			}
		}
		else {
		}

		/* increment thru RD_EN VDL's by 7 */
		if (RD_EN_word_setting < 57) {
			RD_EN_word_setting = RD_EN_word_setting + 7;
		}
		else {
			RD_EN_byte_setting = RD_EN_byte_setting + 7;
		}

		if (RD_EN_byte_setting > 63) {
			setting = setting + 1;
			RD_EN_word_setting = 0;
			RD_EN_byte_setting = 0;
		}
		else {
		}

		if ((wl0_bl0_pass > 0x0) && (wl0_bl1_pass > 0x0) && (wl1_bl0_pass > 0x0) &&
			(wl1_bl1_pass > 0x0)) {
			passing = 1;
		}
		else {
		}

		/* exit loop with setting of 7 if any byte does not pass */
		if (setting > 7) {
			passing = 1;
			setting = 7;
			if (wl0_bl0_pass == 0x0) {
				wl0_bl0_pass = setting;
			}
			else {
			}
			if (wl0_bl1_pass == 0x0) {
				wl0_bl1_pass = setting;
			}
			else {
			}
			if (wl1_bl0_pass == 0x0) {
				wl1_bl0_pass = setting;
			}
			else {
			}
			if (wl1_bl1_pass == 0x0) {
				wl1_bl1_pass = setting;
			}
			else {
			}
		}
		else {
		}

		data = 0x0;
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN, ovr_en, 1);
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN, ovr_force, 1);
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN,
			ovr_step, RD_EN_word_setting);

		tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE_RD_EN +
			GLOBAL_REG_RBUS_START, data);
		if (wl_count  > 1) {
			tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE_RD_EN +
				PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START, data);
		}

		data = 0x0;
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN,
			ovr_en, 1);
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN,
			ovr_force, 1);
#if RD_EN_BYTE_SEL_CHANGE
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN,
			byte_sel, 1);
#endif
		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN,
			ovr_step,
			RD_EN_byte_setting);

		tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN +
			GLOBAL_REG_RBUS_START, data);
		tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN +
			GLOBAL_REG_RBUS_START, data);
		if (wl_count  > 1) {
			tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN +
				PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START, data);
			tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN +
				PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START, data);
		}
	}

	setting = wl0_bl0_pass;

	if (wl0_bl1_pass > setting) {
		setting = wl0_bl1_pass;
	}
	else {
	}
	if (wl1_bl0_pass > setting) {
		setting = wl1_bl0_pass;
	}
	else {
	}
	if (wl1_bl1_pass > setting) {
		setting = wl1_bl1_pass;
	}
	else {
	}

	setting = setting + 1;

	if (setting > 7) {
		setting = 7;
	}
	else {
	}

	new_step[0] = setting;
	new_step[1] = setting;

	tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE_RD_EN +
		GLOBAL_REG_RBUS_START, original_RD_EN_BYTE_setting[0]);
	if (wl_count  > 1) {
		tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE_RD_EN +
			PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START,
			original_RD_EN_BYTE_setting[1]);
	}

	tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN +
		GLOBAL_REG_RBUS_START, original_RD_EN_setting[0]);
	tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN +
		GLOBAL_REG_RBUS_START, original_RD_EN_setting[1]);
	if (wl_count > 1) {
		tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN +
			PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START, original_RD_EN_setting[2]);
		tb_w(phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN +
			PHY_REG_OFFSET_PER_WL + GLOBAL_REG_RBUS_START, original_RD_EN_setting[3]);
	}

	return retval;
}

unsigned all_shmoo_rd_data_dly_FIFO(ddr40_addr_t phy_reg_base, ddr40_addr_t memc_reg_base,
	unsigned int option, ddr40_addr_t mem_test_base)
{
	unsigned int retval = SHMOO_NO_ERROR;
	unsigned int new_step[2];
	unsigned int wc_count = ((option >> 8) & 0xFF) / BITS_PER_WL;
	unsigned int wl;

	NL();

	retval |= one_shmoo_rd_data_dly_FIFO(new_step, phy_reg_base, memc_reg_base,
		wc_count, mem_test_base);
	for (wl = 0; wl < wc_count; wl ++) {
		pack328_t packname;
		unsigned long memsys_reg_offset =  wl * PHY_REG_OFFSET_PER_WL + phy_reg_base +
			GLOBAL_REG_RBUS_START;
		unsigned int data = 0x0;

		if (new_step[wl] < 7) {
			new_step[wl]++;
		}

		SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, READ_DATA_DLY, rd_data_dly,
			new_step[wl]);
		packname.pack32 = 0x594C4452; /* RDLY */
		plot_name(packname, wl);
		plot_dec_number(new_step[wl]); /* NAME & step */
		tb_w((DDR40_CORE_PHY_WORD_LANE_0_READ_DATA_DLY + memsys_reg_offset), data);
	}
	NL();

	return retval;
}


unsigned int generic_shmoo_calib_wr_dm(unsigned long phy_reg_base, unsigned long memc_reg_base,
	unsigned int option, unsigned long mem_test_base)
{
	unsigned retval = SHMOO_NO_ERROR;
	unsigned int wl;
	int bl;
	int bit;
	unsigned long val;
	unsigned new_step[WRDM_STEPS];
	unsigned wrdq_val[WRDQ_VALS];
	unsigned wrdq_sum[WRDM_STEPS] = {0, 0, 0, 0};
	unsigned step_inx, data;
	pack328_t packname;
	unsigned long memc_reg_base_ = memc_reg_base;
	unsigned long mem_test_base_ = mem_test_base;

	memc_reg_base_++;
	mem_test_base_++;

	/* 
	 * 110818 CKD
	 * New strategy regarding the limitation of MIPS to force MEMC to set mask
	 * Take the values generated by WR_DQ and calculate the average to be used as the new_step
	 */ 

	packname.pack32 = 0x4D445257; /* WRDM */

	for (wl = 0; wl < (((option >> 8) & 0xFF) / BITS_PER_WL); wl ++) {
		for (bl = 0; bl < BL_PER_WL; bl++) {
			step_inx = (wl * 2) + (bl * 1);
			/* step_inx generated as 0, 1, 2, 3 */

			val = wl * PHY_REG_OFFSET_PER_WL + PHY_REG_OFFSET_PER_BL * bl +
				phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT0_W;
			/*  for each WL, collect the WR_DQ value */          
			for (bit = 0; bit < BITS_PER_BL; bit ++) {
				wrdq_val[bit] = (RDREG(val + (bit * 4))) &
				DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT0_W_ovr_step_MASK;
				wrdq_sum[step_inx] += wrdq_val[bit];
			}
			/* Take the average of the sum */
			new_step[step_inx] = wrdq_sum[step_inx] / BITS_PER_BL;
			/*  Write it back for this byte lane immediately */
			data = SET_OVR_STEP(new_step[step_inx]);
			val = wl * PHY_REG_OFFSET_PER_WL + PHY_REG_OFFSET_PER_BL * bl +
				phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_DM_W;

			plot_name(packname, wl); /* WRDM WL x */ 
			plot_dec_number(new_step[step_inx]);

			WRREG((wl * PHY_REG_OFFSET_PER_WL + PHY_REG_OFFSET_PER_BL * bl +
				phy_reg_base + DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_DM_W),
				data);
		}
		NL();
	}
	NL();

	return retval;
}

/* Adjust ADDR_CTL VDL setting by programmable amount.
 *
 * input:
 *    shift_amount  0= no shift (does nothing)
 *                  1= 22.5 deg
 *                  2= 45   deg
 *                  3= 67.5 deg
 * return:
 *   -1   PLL not locked
 *   -2   byte mode - not supported yet
 *   -3   shift too much.  
 *    0   OK
 */
int shmoo_adjust_addx_ctrl_delay(unsigned long phy_reg_base, unsigned long memc_reg_base,
	int shift_amount)
{
	unsigned int regdata = (unsigned int)memc_reg_base /* TRICK */, steps, incr;
	unsigned int calib_bit_offset, wr_chan_calib_total;
	unsigned int bitdata, bytedata, II;
	regdata = 0;
	II = 0;

	regdata = tb_r(phy_reg_base + GLOBAL_REG_RBUS_START +
		DDR40_CORE_PHY_CONTROL_REGS_VDL_WR_CHAN_CALIB_STATUS);

	calib_bit_offset = GET_FIELD(regdata, DDR40_CORE_PHY_CONTROL_REGS,
		VDL_WR_CHAN_CALIB_STATUS, wr_chan_calib_bit_offset);
	wr_chan_calib_total = GET_FIELD(regdata, DDR40_CORE_PHY_CONTROL_REGS,
		VDL_WR_CHAN_CALIB_STATUS, wr_chan_calib_total);

	plot_hex_number(regdata);
	NL();

	/* return if nothing to do */
	if (shift_amount == 0) {
		return 0;
	}
	else if (shift_amount > 16) {
		return ERR_AC_VDL_SHIFT_TOO_MUCH;
	}

	/* BYTE MODE */
	if (DDR40_CORE_PHY_CONTROL_REGS_VDL_WR_CHAN_CALIB_STATUS_wr_chan_calib_byte_sel_MASK
		& regdata) {
		return ERR_AC_VDL_SHIFT_BYTE_MODE_NOT_SUPPORTED;
	}
	/* BIT MODE */
	else {
		/* WR_VDL_FIXED_DLY (about 8, comes from the VDL fixed delay.) */
		steps = (WR_VDL_FIXED_DLY + (wr_chan_calib_total >> 4));
		/*  add multiple of steps/16 */
		incr = (steps * 1024 / 16) * shift_amount;
		incr = incr / 1024;

		/* plot the changes: */  
		UART_OUT('+');
		plot_dec_number(shift_amount);

		if ((incr + steps) > 63) {
			bytedata  = DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BYTE_CTL_ovr_en_MASK |
				((((incr + steps) >> 1) - WR_VDL_FIXED_DLY) &
				DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BYTE_CTL_ovr_step_MASK);
			/* Set VDL_OVRIDE_BYTE_CTL.byte_sel = 0;
			 * Set VDL_OVRIDE_BYTE_CTL.ovr_step = (((incr + steps) >> 1) -
			 * WR_VDL_FIXED_DLY);
			 * This -8 covers the additional fixed delays.
			 */
			bitdata = DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL_ovr_en_MASK |
				DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL_byte_sel_MASK;
			/* Set VDL_OVRIDE_BIT_CTL.byte_sel = 1;          
			 * Set VDL_OVRIDE_BIT_CTL.ovr_step = 0
			 */
		}
		else {
			bytedata  = DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BYTE_CTL_ovr_en_MASK;
			/* Set VDL_OVRIDE_BYTE_CTL.byte_sel = 0;
			 * Set VDL_OVRIDE_BYTE_CTL.ovr_step = 0;
			 */
			bitdata = DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL_ovr_en_MASK |
				((incr + steps) &
				DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL_ovr_step_MASK);
			/* Set VDL_OVRIDE_BIT_CTL.byte_sel = 0;
			 * Set VDL_OVRIDE_BIT_CTL.ovr_step = (incr + steps);
			 */
		}

		/* update reg */
		tb_w(phy_reg_base + GLOBAL_REG_RBUS_START +
			DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BYTE_CTL, bytedata);
		tb_w(phy_reg_base + GLOBAL_REG_RBUS_START +
			DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL, bitdata);
	}

	/* re issue MRS programming */
	rewrite_dram_mode_regs(sih_saved);
	return 0;
}

#endif /* (SHMOO_BUILD == 1) */
