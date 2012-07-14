#include "sbmips.h"
#include "bsp_config.h"
#include "bcm97115.h"

#ifdef SERIALLEDS
		#include "bcm97115_uart.h"
#endif


/*  *********************************************************************
    *  BOARD_EARLYINIT()
    *
    *  Initialize board registers.  This is the earliest 
    *  time the BSP gets control.  This routine cannot assume that
    *  memory is operational, and therefore all code in this routine
    *  must run from registers only.  The $ra register must not
    *  be modified, as it contains the return address.
    *
    *  This routine will be called from uncached space, before
    *  the caches are initialized.  If you want to make
    *  subroutine calls from here, you must use the CALLKSEG1 macro.
    *
    *  Among other things, this is where the GPIO registers get
    *  programmed to make on-board LEDs function, or other startup
    *  that has to be done before anything will work.
    *
    *  Input parameters:
    *      nothing
    *
    *  Return value:
    *      nothing
    ********************************************************************* */

LEAF(board_earlyinit)

		.set    noreorder

		li      t1,EBI_ADR_BASE

		/* Chip Select # 0 */
		li      t2,PHYS_ROM_BASE|EBI_SIZE_2M
		sw      t2,CS0BASE(t1)        //# CS[0] Base

		/* Chip Select # 1 */
		li      t2,PHYS_FLASH_BASE|EBI_SIZE_16M
		sw      t2,CS1BASE(t1)        //# CS[1] Base
		li      t2,SEVENWT|EBI_WORD_WIDE|EBI_ENABLE
		sw      t2,CS1CNTL(t1)        //# CS[1] Control

		/* Chip Select # 2 */
		li      t2,PHYS_FLASH2_BASE|EBI_SIZE_4M
		sw      t2,CS2BASE(t1)        //# CS[2] Base
		li      t2,SEVENWT|EBI_WORD_WIDE|EBI_ENABLE
		sw      t2,CS2CNTL(t1)        //# CS[2] Control

		/* Chip Select # 8 */
		li      t2,PHYS_BCM44XX_BASE|EBI_SIZE_8K
		sw      t2,CS8BASE(t1)        //# CS[8] Base
#ifdef __MIPSEB
		li      t2,EBI_WORD_WIDE|EBI_ENABLE|EBI_TS_TA_MODE|0x800
#else
		li      t2,EBI_WORD_WIDE|EBI_ENABLE|EBI_TS_TA_MODE|EBI_REV_END|0x800
#endif
		sw      t2,CS8CNTL(t1)        //# CS[8] Control

		li      t2,0x00000400
		sw      t2,EBICONFIG(t1)
		nop

	#
	# Turn off all timers.
	#

		li      t0,TIMR_ADR_BASE
		sb      zero,TIMER_MASK(t0)
		li      v0,0x0f
		sb      v0,TIMER_INTS(t0)

		sw      zero,TIMER_0_CTL(t0)
		sw      zero,TIMER_1_CTL(t0)
		sw      zero,TIMER_2_CTL(t0)
		sw      zero,TIMER_3_CTL(t0)


#ifdef SERIALLEDS

		li t3, XTALFREQ / 115200 / 16

		/* Ugh!  Debugging hack! */
		li      t1,   0xFFFE00B0
		andi    t3,   t3, 0xff
		sb      t3,   BAUDLO_OFFSET(t1)
		srl     t3,   8
		andi    t3,   t3, 0xff
		sb      t3,   BAUDHI_OFFSET(t1)

		sb      zero, RXSTAT_OFFSET(t1)
		sb      zero, TXSTAT_OFFSET(t1)

		li      t2,   TXEN | RXEN | BITM8
		sb      t2,   CTRL_OFFSET(t1)

#endif

		j       ra
		nop

		.set    reorder

END(board_earlyinit)


/*  *********************************************************************
    *  BOARD_DRAMINFO
    *
    *  Return the address of the DRAM information table
    *
    *  Input parameters:
    *      nothing
    *
    *  Return value:
    *      v0 - DRAM info table, return 0 to use default table
    ********************************************************************* */


LEAF(board_draminfo)

		move    v0,zero     # auto configure
		j       ra

END(board_draminfo)


/*  *********************************************************************
    *  BOARD_DRAMINIT
    *
    *  This routine should activate memory.
    *
    *  Input parameters:
    *      a0 - points to configuration table returned by board_draminfo
    *           or 0 to use an automatic table
    *
    *  Return value:
    *      v0 - total memory installed
    *
    *  Registers used:
    *      can use all registers.
    ********************************************************************* */

LEAF(board_draminit)

#ifndef DEBUG_ENV_ICE

/*
 * BCM711x Mips bootcode
 *
 * Notes:
 * BCM97115 DDRAM Configuration
 * 	DDRAM configured for 2 chips of 16Mbits x 16 (total 64 Mbytes):
 *		- CS0: 1 chip of 256 Mbits, 16 bit wide, a total of 32 Mbytes
 *		- CS1: 1 chip of 256 Mbits, 16 bit wide, a total of 32 Mbytes
 *		- 16 bit mode of operation for the DDR interface 
 * BCM97110 DDRAM Configuration
 *	DDRAM configured for 2 chips of 16Mbits x 16 (total 64 Mbytes):
 *		- CS0: 2 chips of 256 Mbits, 16-bit wide, a total of 64 Mbytes
 *		- CS1: No DDR device mounted on this chip select
 *		- 32 bit mode of operation for the DDR interface
 */
	
	/* 
	 *	Setup for 2 chips 16Mbits x 16 Samsung K4H561638B.
	 */
	li	v0,DDR_BASE_ADR_REG	/*  v0 contains DDR control reg. base.		*/

	li	v1,0x00008000
	sw	v1,0x1A0(v0)

	/* 
	 *	Program RB_MMC_DRAM_TIMING_REG	(0x00)
	 */
#if defined(BCM97115)
	li	v1,0x0000BA2C	/* Set RB_MMC_DRAM_TIMING_REG = 0x0000BA2C	*/
#else
	li	v1,0x00001A2C	/* Set RB_MMC_DRAM_TIMING_REG = 0x00001A2C	*/
#endif
	sw	v1,0x00(v0)		/* Set RB_MMC_DRAM_TIMING_REG		*/
			
	/* 
	 *	Program RB_MMC_MEC_DRAM_MODE_REG (0x01)
	 *	BCM97115 (default 64 Mbytes)
	 *		- ExtBusWidth		16-bit external DDR interface
	 *		- M256_CS1			256 Mbits device mounted on chip select 1
	 *		- M256_CS0			256 Mbits device mounted on chip select 0
	 *		- CAS Latency		2
	 *	BCM97110 (default 64 Mbytes)
	 *		- ExtBusWidth		32-bit external DDR interface
	 *		- M256_CS0			256 Mbits device mounted on chip select 0
	 *		- CAS Latency		2
         *              - Burst Length          8 for 16bit mode  & 4 for 32bit mode
	 */	
#if defined(BCM97115)
	li	v1,0x00007823	/* Setting for BCM7115						*/
						/* RB_MMC_MEC_DRAM_MODE_REG = 0x00007863	*/
#else
	li	v1,0x00001023	/* Setting for BCM7110						*/
						/* RB_MMC_MEC_DRAM_MODE_REG = 0x00001022	*/
#endif

	sw	v1,0x04(v0)		/* Set RB_MMC_MEMC_DRAM_MODE_REG	*/

	/* 
	 *	Program RB_MMC_MEMC_DRAM_MODE2_REG (0x2A)
	 *	BCM97115
	 *		- MEMSIZE			32 Mbytes mounted on CS0
	 *		- CS1_NM			there is DDR device mounted on the second chip select
	 *	BCM97110
	 *		- MEMSIZE			64 Mbytes mounted on CS0
	 *		- CS1_NM			there is no DDR device mounted on the second chip select
	 */
#if defined(BCM97115)
	li	v1,0x0000C6F4
#else
    li  v1,0x00006002
#endif

	sw	v1,0xA8(v0)		/* Set RB_MMC_MEMC_DRAM_MODE2_REG	*/
	
	/* 
	 *	Program RB_MMC_MMU_REG0 (0x06)
	 *			RB_MMC_MMU_REG1 (0x07)
	 *			RB_MMC_MMU_REG2 (0x08)
	 *			RB_MMC_MMU_REG3	(0x09)
	 *	BCM97115
	 *		- CS0				32 Mbytes mounted on CS0 (4M x 16 x 4) * 1 chip
	 *		- CS1				32 Mbytes mounted on CS1 (4M x 16 x 4) * 1 chip
	 *	BCM97110
	 *		- CS0				64 Mbytes mounted on CS0 (4M x 16 x 4) * 2 chips
	 *		- CS1				there is no DDR device mounted on the second chip select
	 */
#if defined(BCM97115)
	li	v1,0x00003210
	sw	v1,0x18(v0)		/* Set RB_MMC_MMU_REG0 = 0x00003210	*/
	li	v1,0x0000BA98
	sw	v1,0x1C(v0)		/* Set RB_MMC_MMU_REG1 = 0x0000BA98	*/
	li	v1,0x0000ffff
	sw	v1,0x20(v0)		/* Set RB_MMC_MMU_REG2 = 0x0000ffff	*/
	sw	v1,0x24(v0)		/* Set RB_MMC_MMU_REG3 = 0x0000ffff	*/
#else
	li	v1,0x00003210
	sw	v1,0x18(v0)		/* Set RB_MMC_MMU_REG0 = 0x00003210	*/
	li	v1,0x00007654
	sw	v1,0x1C(v0)		/* Set RB_MMC_MMU_REG1 = 0x00007654	*/
	li	v1,0x0000ffff
	sw	v1,0x20(v0)		/* Set RB_MMC_MMU_REG2 = 0x0000ffff	*/
	sw	v1,0x24(v0)		/* Set RB_MMC_MMU_REG3 = 0x0000ffff	*/
#endif

	/* 
	 *	Program RB_MMC_NMBX_TIMING_REG	(0x02)
	 */
	li	v1,0x0000172E
	sw	v1,0x08(v0)		/* Set RB_MMC_NMBX_TIMING_REG = 0x0000172E	*/
			
	/* 
	 *	Program RB_MMC_READ01_NCDL_CORR_REG	(0x0D)
	 *			RB_MMC_READ23_NCDL_CORR_REG	(0x2B)
	 */
	li	v1,0x00000101
	sw	v1,0x34(v0)		/* Set RB_MMC_READ01_NCDL_CORR_REG = 0x00007E7E	*/
	sw	v1,0xAC(v0)		/* Set RB_MMC_READ23_NCDL_CORR_REG = 0x00007E7E */	
			
	/* 
	 *	Program RB_MMC_RDWR_NCDL_CORR_REG	(0x0C)
	 */
#if defined(BCM97115)
	li	v1,0x00009D05	/* Set RB_MMC_RDWR_NCDL_CORR_REG = 0x00009D05	*/
#else
	li	v1,0x00000101	/* Set RB_MMC_RDWR_NCDL_CORR_REG = 0x00000101	*/
#endif

	sw	v1,0x30(v0)		/* Set RB_MMC_RDWR_NCDL_CORR_REG	*/

	/* 
	 *	Program RB_MMC_MSA_MODE_REG	(0x1B)
	 */
	sw      $0,0x6C(v0)	/* Set RB_MMC_MSA_MODE_REG: Normal mode	*/

	/*
	 * Program the client priority, timer value and 
	 * round robin membership etc.
	 */
	sw      $0,0x0C(v0)	/* Set RB_MMC_CLIENT_INDEX_REG		*/
	li      v1,0xA80
	sw      v1,0x10(v0)	/* Set RB_MMC_INDX_DATA_WR_REG		*/

	li      v1,0x1
	sw      v1,0x0C(v0)
	li      v1,0xFFF2
	sw      v1,0x10(v0)

	li      v1,0x2
	sw      v1,0x0C(v0)
	li      v1,0xFFF3
	sw      v1,0x10(v0)
	
	li      v1,0x3
	sw      v1,0x0C(v0)
	li      v1,0xFFF4
	sw      v1,0x10(v0)

	li      v1,0x4
	sw      v1,0x0C(v0)
	li      v1,0xC00F
	sw      v1,0x10(v0)
	
	li      v1,0x5
	sw      v1,0x0C(v0)
	li      v1,0xFFD6
	sw      v1,0x10(v0)
	
	li      v1,0x6
	sw      v1,0x0C(v0)
	li      v1,0xC010
	sw      v1,0x10(v0)
	
	li      v1,0x7
	sw      v1,0x0C(v0)
	li      v1,0x1985
	sw      v1,0x10(v0)
	
	li      v1,0x8
	sw      v1,0x0C(v0)
	li      v1,0x1984
	sw      v1,0x10(v0)
	
	li      v1,0x9
	sw      v1,0x0C(v0)
	li      v1,0x2A0A
	sw      v1,0x10(v0)
	
	li      v1,0xA
	sw      v1,0x0C(v0)
	li      v1,0xFFD7
	sw      v1,0x10(v0)
	
	li      v1,0xB
	sw      v1,0x0C(v0)
	li      v1,0xFC2
	sw      v1,0x10(v0)
	
	li      v1,0xC
	sw      v1,0x0C(v0)
	li      v1,0xBF8E
	sw      v1,0x10(v0)
	
	li      v1,0xD
	sw      v1,0x0C(v0)
	li      v1,0x2B0B
	sw      v1,0x10(v0)
	
	li      v1,0xE
	sw      v1,0x0C(v0)
	li      v1,0xF41
	sw      v1,0x10(v0)
	
	li      v1,0xF
	sw      v1,0x0C(v0)
	li      v1,0xFFD8
	sw      v1,0x10(v0)
	
	li      v1,0x10
	sw      v1,0x0C(v0)
	li      v1,0x2849
	sw      v1,0x10(v0)

	li      v1,0x11
	sw      v1,0x0C(v0)
	li      v1,0x3CAC
	sw      v1,0x10(v0)

	li      v1,0x12
	sw      v1,0x0C(v0)
	li      v1,0x650D
	sw      v1,0x10(v0)

	li      v1,0x13
	sw      v1,0x0C(v0)
	li      v1,0xFFD9
	sw      v1,0x10(v0)

	li      v1,0x14
	sw      v1,0x0C(v0)
	li      v1,0xFFDA
	sw      v1,0x10(v0)

	li      v1,0x15
	sw      v1,0x0C(v0)
	li      v1,0xFFDB
	sw      v1,0x10(v0)

	li      v1,0x16
	sw      v1,0x0C(v0)
	li      v1,0xFFDC
	sw      v1,0x10(v0)

	li      v1,0x17
	sw      v1,0x0C(v0)
	li      v1,0xFFDD
	sw      v1,0x10(v0)

	li      v1,0x18
	sw      v1,0x0C(v0)
	li      v1,0x1583
	sw      v1,0x10(v0)

	li      v1,0x19
	sw      v1,0x0C(v0)
	li      v1,0x31
	sw      v1,0x10(v0)

	li      v1,0x1A
	sw      v1,0x0C(v0)
	li      v1,0x1FC6
	sw      v1,0x10(v0)

	li      v1,0x1B
	sw      v1,0x0C(v0)
	li      v1,0x25C7
	sw      v1,0x10(v0)

	li      v1,0x1C
	sw      v1,0x0C(v0)
	li      v1,0xFFDE
	sw      v1,0x10(v0)

	li      v1,0x1D
	sw      v1,0x0C(v0)
	li      v1,0xFFDF
	sw      v1,0x10(v0)

	li      v1,0x1E
	sw      v1,0x0C(v0)
	li      v1,0xFFF5
	sw      v1,0x10(v0)
			
	li      v1,0x1F
	sw      v1,0x0C(v0)
	li      v1,0x4E8
	sw      v1,0x10(v0)
			
	li		v1,0x11F5
	sw		v1,0x28(v0)
	
	/* 
	 *	Program RB_SOFT_RESET1_WITHDRAW_REG	(0x69)
	 */
	li      v1,0x0080
	sw      v1,0x01A4(v0)

	nop

#endif

		li      v0, 64
		j       ra

END(board_draminit)


/*  *********************************************************************
    *  BOARD_SETLEDS(x)
    *
    *  Set LEDs for boot-time progress indication.  Not used if
    *  the board does not have progress LEDs.  This routine
    *  must not call any other routines, since it may be invoked
    *  either from KSEG0 or KSEG1 and it may be invoked 
    *  whether or not the icache is operational.
    *
    *  Input parameters:
    *      a0 - LED value (8 bits per character, 4 characters)
    *
    *  Return value:
    *      nothing
    *
    *  Registers used:
    *      t0,t1,t2,t3
    ********************************************************************* */


LEAF(board_setleds)

#ifdef SERIALLEDS

		li      t0, UARTA_BASE
		li      t3, 0x0

		li      t2, TXDRE
1:		lb      t1, TXSTAT_OFFSET(t0)
		and     t1, t2
		bne     t1, t2, 1b
		nop
		sb      a0, TXDATA_OFFSET(t0)

		rol     a0, 8
		addu    t3, t3, 1
		bne     t3, 4, 1b
		nop

		/* Now write \r\n */

		li      t3, '\r'
1:		lb      t1, TXSTAT_OFFSET(t0)
		and     t1, t2
		bne     t1, t2, 1b
		nop
		sb      t3, TXDATA_OFFSET(t0)

		li      t3, '\n'
1:		lb      t1, TXSTAT_OFFSET(t0)
		and     t1, t2
		bne     t1, t2, 1b
		nop
		sb      t3, TXDATA_OFFSET(t0)

#endif

		j       ra

END(board_setleds)
