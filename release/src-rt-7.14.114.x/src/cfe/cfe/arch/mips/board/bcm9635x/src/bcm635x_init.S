#include "sbmips.h"
#include "bsp_config.h"
#include "board.h"

#define GPIO_BASE           0xfffe0400
#define GPIODIR             0x06
#define GPIOLED             0x09
#define GPIOIO              0x0a

#define CS0BASE             0x00
#define CS0CNTL             0x04
#define CS2BASE             0x10
#define CS2CNTL             0x14
#define CS4BASE             0x20
#define CS4CNTL             0x24

#define EBICONFIG           0x40

#define UART_BASE           0xFFFE0300
#define INTC_BASE           0xfffe0000

#define UART_CONTROL        0x01
#define UART_CONFIG         0x02
#define UART_RXTIMEOUT      0x03
#define UART_BAUD           0x04
#define UART_FIFOCFG        0x0a
#define UART_INTMASK        0x10
#define UART_INTSTAT        0x12
#define UART_DATA           0x17

#define BRGEN               0x80
#define TXEN                0x40
#define RXEN                0x20
#define TXPARITYEVEN        0x04
#define RXPARITYEVEN        0x01
#define BITS8SYM            0x30
#define ONESTOP             0x07
#define TX4                 0x40
#define RX4                 0x04

#define UART0CONTROL        0x01
#define UART0CONFIG         0x02
#define UART0RXTIMEOUT      0x03
#define UART0BAUD           0x04
#define UART0FIFOCFG        0x0a
#define UART0INTMASK        0x10
#define BLKEN               06
#define UART_CLK_EN         0x0008

#define TXFIFOEMT           0x0020

#define SDR_INIT_CTL        0x00
#define SDRAM_BASE          0xfffe2300
#define SDR_9BIT_COL        (1<<11)
#define SDR_32BIT           (1<<10)
#define SDR_64x32           (3<<4)
#define SDR_128MEG          (2<<4)
#define SDR_ENABLE          (1<<3)
#define SDR_MRS_CMD         (1<<2)
#define SDR_PRE_CMD         (1<<1)
#define SDR_CBR_CMD         (1<<0)
#define SDR_CFG_REG         0x04
#define SDR_FULL_PG         0x00
#define SDR_FAST_MEM        (1<<2)
#define SDR_REF_CTL         0x08
#define SDR_REF_EN          (1<<15)
#define SDR_MEM_BASE        0x0c
#define DRAM16MBSPC         0x00000002
#define DRAM32MBSPC         0x00000003
#define DRAM64MBSPC         0x00000004

#define EBIC_BASE           0xfffe2000
#define EBI_SIZE_8K         0
#define EBI_SIZE_64K        3
#define EBI_SIZE_8M         10
#define EBI_ENABLE          0x00000001
#define ONEWT               0x00000002
#define THREEWT             0x00000006
#define EBI_WORD_WIDE       0x00000010
#define EBI_TS_TA_MODE      0x00000080
#define EBI_TS_SEL          0x00000100
#define EBI_FIFO            0x00000200

#define TIMR_BASE           0xfffe0200
#define TIMER_MASK(t0)      2
#define TIMER_INTS(t0)      3
#define TIMER_CTL0(t0)      4
#define TIMER_CTL1(t0)      8
#define TIMER_CTL2(t0)      12

/* r2 = BCM96352 board type which is determined by checking GPIO bits.
 * BCM96352SV - GPIO bit 6 = 1, GPIO bit 5 = 1 (GPIOIO = 0x60)
 * BCM96352R  - GPIO bit 6 = 0, GPIO bit 5 = 1 (GPIOIO = 0x20)
 * BCM96350   - GPIO bit 6 = 1, GPIO bit 5 = 0 (GPIOIO = 0x40)
 */
#define BT_BCM9635X_MASK    0x60
#define BT_BCM96352SV       0x60
#define BT_BCM96352R        0x20
#define BT_BCM96350         0x40
#define board_type(r1, r2) \
        .set    noreorder; \
        li      r1, GPIO_BASE; \
        lhu     r2, GPIODIR(r1); \
        andi    r2, 0xff9f; \
        sh      r2, GPIODIR(r1); \
        lhu     r2, GPIOIO(r1); \
        andi    r2, BT_BCM9635X_MASK; \
        .set    reorder


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
    *   nothing
    *
    *  Return value:
    *   nothing
    ********************************************************************* */

LEAF(board_earlyinit)

        /* Initialize the EBI bus. */
        .set    noreorder
        li      t1,EBIC_BASE            
        li      t2,PHYS_FLASH_BASE|EBI_SIZE_8M
        sw      t2,CS0BASE(t1)        # CS[0] Base
        li      t2,THREEWT|EBI_WORD_WIDE|EBI_ENABLE
        sw      t2,CS0CNTL(t1)        # CS[0] Control

        li      t2,PHYS_ITEX_BASE|EBI_SIZE_64K
        sw      t2,CS2BASE(t1)        # CS[2] Base
        li      t2,ONEWT|EBI_WORD_WIDE|EBI_ENABLE|EBI_FIFO
        sw      t2,CS2CNTL(t1)        # CS[2] Control

        /* Skip HPNA EBI initialization if BCM96350 */
        board_type(t0, t2)            /* t2 = board type */
        .set    noreorder

        li      t0, BT_BCM96350
        beq     t2, t0, 1f
        nop

        li      t2,PHYS_BCM42xx_BASE|EBI_SIZE_8K
        sw      t2,CS4BASE(t1)        # CS[4] Base
        li      t2,EBI_WORD_WIDE|EBI_TS_TA_MODE|EBI_TS_SEL|EBI_ENABLE
        sw      t2,CS4CNTL(t1)        # CS[4] Control

        /*
         * Supposedly, this enables the master WR_POST for the EBI, as well as
         * setting something else.  However, with A3 silicon, it seems that we
         * need to set the individual WR_POST bits for each CS#CNTL in order for
         * it to work.  It isn't clear whether or not the master enable is needed
         * for A3.
         *
         * We believe that this used to work for the A2, but perhaps that change
         *  didn't get rolled in to A3?
         */
1:
        li      t2,0x00000400 
        sw      t2,EBICONFIG(t1)

        j       ra
        nop
        .set    reorder

        /* Turn off all timers.  Should this be done here? */
        .set    noreorder
        li      t0,TIMR_BASE
        li      v0,0x00
        sb      v0,TIMER_MASK(t0)
        li      v0,0x07
        sb      v0,TIMER_INTS(t0)

        sw      zero,TIMER_CTL0(t0)
        sw      zero,TIMER_CTL1(t0)
        sw      zero,TIMER_CTL2(t0)
        j       ra
        .set    reorder

        j       ra

END(board_earlyinit)


/*  *********************************************************************
    *  BOARD_DRAMINFO
    *  
    *  Return the address of the DRAM information table
    *  
    *  Input parameters: 
    *   nothing
    *
    *  Return value:
    *   v0 - DRAM info table, return 0 to use default table
    ********************************************************************* */


LEAF(board_draminfo)

        move    v0,zero
        j       ra

END(board_draminfo)


/*  *********************************************************************
    *  BOARD_DRAMINIT
    *  
    *  This routine should activate memory.
    *  
    *  Input parameters: 
    *   a0 - points to configuration table returned by board_draminfo
    *           or 0 to use an automatic table
    *
    *  Return value:
    *   v0 - total memory installed
    *
    *  Registers used:
    *   can use all registers.
    ********************************************************************* */

LEAF(board_draminit)

        .set    noreorder

		/* check memory config type 64MG, 32MG, 16MG and default to 8MG */
        li      t1,SDRAM_BASE          
	li		t0,BOARD_SDRAM_TYPE_ADDRESS
	lw		t0,0(t0)
		
        /* SDRAM settings for BCM6352SV board (64MB) */
		li		v0, 64
        li      t3,SDR_ENABLE|SDR_MRS_CMD|SDR_64x32|SDR_9BIT_COL
        li      t2,PHYS_DRAM_BASE|DRAM64MBSPC
		li		t4,MEMORY_635X_64MB_2_CHIP
        beq     t0, t4, 1f				/* jump if board is 64MB */
        nop

        /* SDRAM settings for BCM6352R board (32MB) */
		li		v0, 32
        li      t3,SDR_ENABLE|SDR_MRS_CMD|SDR_128MEG|SDR_32BIT|SDR_9BIT_COL
        li      t2,PHYS_DRAM_BASE|DRAM32MBSPC
		li		t4,MEMORY_635X_32MB_2_CHIP
        beq     t0, t4, 1f				/* jump if board is 64MB */
        nop

        /* SDRAM settings for BCM6350 board (16MB) */
		li		v0, 16
        li      t3,SDR_ENABLE|SDR_MRS_CMD|SDR_128MEG|SDR_32BIT
        li      t2,PHYS_DRAM_BASE|DRAM16MBSPC

1:

        /* Bypass SDRAM initialization if SDRAM is already enabled. */
        li      t1,SDRAM_BASE          
        lw      t4,SDR_REF_CTL(t1)
        li      t5,SDR_REF_EN|0x40
        beq     t4, t5, 2f
        nop

        sw      t2,SDR_MEM_BASE(t1)   # Set Memory Base address and size
        li      t2,SDR_FULL_PG|SDR_FAST_MEM
        sw      t2,SDR_CFG_REG(t1)    # Full page burst, slow memory
        li      t2,SDR_ENABLE|SDR_PRE_CMD
        sw      t2,SDR_INIT_CTL(t1)   # Pre-charge powerup sequence
        li      t2,SDR_ENABLE|SDR_CBR_CMD
        sw      t2,SDR_INIT_CTL(t1)   # 8 or more auto-refresh cmds
        sw      t2,SDR_INIT_CTL(t1)
        sw      t2,SDR_INIT_CTL(t1)
        sw      t2,SDR_INIT_CTL(t1)
        sw      t2,SDR_INIT_CTL(t1)
        sw      t2,SDR_INIT_CTL(t1)
        sw      t2,SDR_INIT_CTL(t1)
        sw      t2,SDR_INIT_CTL(t1)
        sw      t3,SDR_INIT_CTL(t1)   # Mode Register set, 
        li      t2,SDR_REF_EN|0x40     
        sw      t2,SDR_REF_CTL(t1)    # Refresh period 0x40, Enable

2:
        j       ra
        nop
        .set    reorder


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
    *   a0 - LED value (8 bits per character, 4 characters)
    *
    *  Return value:
    *   nothing
    *  
    *  Registers used:
    *   t0,t1,t2,t3
    ********************************************************************* */


LEAF(board_setleds)

        j       ra

END(board_setleds)
