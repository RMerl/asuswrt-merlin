#include "sbmips.h"
#include "bsp_config.h"

#include "6345_cpu.h"
#include "6345_common.h"
#include "board.h"

/*
#************************************************************************
#*     InitEbi: Initializes the EBI for proper chip-select operation    *
#*                                                                      *
#*      SYNTAX: void  InitEbi(void);                                    *
#*     RETURNS:                                                         *
#*                                                                      *
#*     Configuration                                                    *
#*       CS[0] - Flash        @ 1fc0_0000 BASE_ADDR must already be set *
#*                                      because we have boot from flash *
#*       CS[1] - Unused                                                 *
#*       CS[2] - ITeX ADSL    @ PHYS_ITEX_BASE                          *
#*       CS[3] - Unused                                                 *
#*       CS[4] - BCM42xx      @ PHYS_BCM42xx_BASE                       *
#*       CS[5] - Unused                                                 *
#*       CS[6] - Unused                                                 *
#*       CS[7] - Unused                                                 *
#*                                                                      *
#************************************************************************
*/

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
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

LEAF(board_earlyinit)

        .set    noreorder
        mtc0    zero,C0_SR
        li      v0,1<<22				# seanl: Why? SR_BEV
        mtc0    v0,C0_SR                # state unknown on reset
        mtc0    zero,C0_CAUSE           # clear software interrupts
        nop                             # paranoia
        .set    reorder

        /**--------------------------------------------------------------**/
        /** platform specific code                                       **/
        /**--------------------------------------------------------------**/

        .set    noreorder
        /**----- Disable Caches -----------------------------------------**/
        mfc0    t1, BCM6345_CONFIG
        nop
        and     t1, t1, ~(CP0_CFG_ICSHEN|CP0_CFG_DCSHEN)
        mtc0    t1, BCM6345_CONFIG      # disable I&D Caches
        .set    reorder

        /**----- Initialize EBI -----------------------------------------**/
        li      t1,EBIC_BASE            
        li      t2,PHYS_FLASH_BASE|EBI_SIZE_8M
        sw      t2,CS0BASE(t1)        # CS[0] Base
        li      t2,THREEWT|EBI_WORD_WIDE|EBI_ENABLE
        sw      t2,CS0CNTL(t1)        # CS[0] Control

        /*
        # Supposedly, this enables the master WR_POST for the EBI, as well as
        # setting something else.  However, with A3 silicon, it seems that we
        # need to set the individual WR_POST bits for each CS#CNTL in order for
        # it to work.  It isn't clear whether or not the master enable is needed
        # for A3.
        #
        # We believe that this used to work for the A2, but perhaps that change
        # didn't get rolled in to A3?
        */
1:
        li      t2,0x00000400 
        sw      t2,EBICONFIG(t1)
        
		/**--- End of InitEbi -------------------------------------------**/    

        /**----- Initialize MIPS Clock ----------------------------------**/
		li              t0, INTC_BASE           # The address of the PLL register
        lw              t1, FMSEL(t0)
        and             t1, ~FMDIV_MASK
        sw              t1, FMSEL(t0)           # Set Mips Clock
        nop

END(board_earlyinit)


/*  *********************************************************************
    *  BOARD_DRAMINFO
    *  
    *  Return the address of the DRAM information table
    *  
    *  Input parameters: 
    *  	   nothing
    *  	    
    *  Return value:
    *  	   v0 - DRAM info table, return 0 to use default table
    ********************************************************************* */


LEAF(board_draminfo)

		j	ra

END(board_draminfo)


/*  *********************************************************************
    *  BOARD_DRAMINIT
    *  
    *  This routine should activate memory.
    *  
    *  Input parameters: 
    *  	   a0 - points to configuration table returned by board_draminfo
    *           or 0 to use an automatic table
    *  	   
    *  Return value:
    *  	   v0 - total memory installed
    *  	   
    *  Registers used:
    *  	   can use all registers.
    ********************************************************************* */

LEAF(board_draminit)

/*
#************************************************************************
#* RC32364
#*     InitSdram: Initializes the Sdram for proper operation            *
#*                                                                      *
#*      SYNTAX: void  InitSdram(void);                                  *
#*     RETURNS:                                                         *
#*  Note:  We must be running UnCached before executing this routine    *
#*         for delay purposes                                           *
#************************************************************************
*/
InitSdram:
        .set    noreorder

		/* check memory config type 64MB, 32MB, 16MB and default to 8MB */
        li      t1,SDRAM_BASE          
		li		t0,BOARD_SDRAM_TYPE_ADDRESS
		lw		t0,0(t0)
		
		/* setting for 64MB, 2 chip */        
 		li      t3,SDR_ENABLE|SDR_MRS_CMD|SDR_64MEG	|SDR_128MEG			/* 0x0000003C */
        li      t2,PHYS_DRAM_BASE|DRAM64MBSPC							/* 64mg */
		li		v0,64
		li		t4,MEMORY_64MB_2_CHIP
		beq		t0,t4,1f
		nop
		
		/* setting for 32MB, 1 chip */        
 		li      t3,SDR_ENABLE|SDR_MRS_CMD|SDR_128MEG					/* 0x0000002C */
        li      t2,PHYS_DRAM_BASE|DRAM32MBSPC							/* 32mg */
		li		v0,32
		li		t4,MEMORY_32MB_1_CHIP
		beq		t0,t4,1f
		nop

		/* setting for 16MB, 1 chip */        
 		li      t3,SDR_ENABLE|SDR_MRS_CMD|SDR_128MEG|SDR_9BIT_COL		/* 0x0000082C */
        li      t2,PHYS_DRAM_BASE|DRAM16MBSPC							/* 16mg */
		li		v0,16
		li		t4,MEMORY_16MB_1_CHIP
		beq		t0,t4,1f
		nop
      
        /* Default SDRAM settings for BCM6345 board (8MB, 1 chip) */
        li      t3,SDR_ENABLE|SDR_MRS_CMD|SDR_64MEG						/* 0x0000001c */
        li      t2,PHYS_DRAM_BASE|DRAM8MBSPC							/* 8mg */
        li		v0,8
			
1:
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
    *  	   a0 - LED value (8 bits per character, 4 characters)
    *  	   
    *  Return value:
    *  	   nothing
    *  
    *  Registers used:
    *  	   t0,t1,t2,t3
    ********************************************************************* */


LEAF(board_setleds)

		j	ra

END(board_setleds)
