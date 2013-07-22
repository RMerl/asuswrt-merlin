/*
<:copyright-broadcom 
 
 Copyright (c) 2002 Broadcom Corporation 
 All Rights Reserved 
 No portions of this material may be reproduced in any form without the 
 written permission of: 
          Broadcom Corporation 
          16215 Alton Parkway 
          Irvine, California 92619 
 All information contained in this document is Broadcom Corporation 
 company private, proprietary, and trade secret. 
 
:>
*/
/***********************************************************************/
/*                                                                     */
/*   MODULE:  board.h                                                  */
/*   DATE:    97/02/18                                                 */
/*   PURPOSE: Board specific information.  This module should include  */
/*            all base device addresses and board specific macros.     */
/*                                                                     */
/***********************************************************************/
#ifndef _BOARD_H
#define _BOARD_H

#if __cplusplus
extern "C" {
#endif
/*****************************************************************************/
/*                    Misc board definitions                                 */
/*****************************************************************************/

/*****************************************************************************/
/*                    Physical Memory Map                                    */
/*****************************************************************************/

#define PHYS_DRAM_BASE           0x00000000     /* Dynamic RAM Base */
#define PHYS_FLASH_BASE          0x1FC00000     /* Flash Memory         */
#define PHYS_BCM42xx_BASE        0x1B000000     /* HPNA */
#define PHYS_ITEX_BASE           0x1CC00000     /* ITEX Card            */

/*****************************************************************************/
/* Note that the addresses above are physical addresses and that programs    */
/* have to use converted addresses defined below:                            */
/*****************************************************************************/
#define DRAM_BASE           (0x80000000 | PHYS_DRAM_BASE)   /* cached DRAM */
#define DRAM_BASE_NOCACHE   (0xA0000000 | PHYS_DRAM_BASE)   /* uncached DRAM */
#define FLASH_BASE          (0xA0000000 | PHYS_FLASH_BASE)  /* uncached Flash  */
#define BCM42xx_BASE        (0xA0000000 | PHYS_BCM42xx_BASE)
#define ITEX_BASE           (0xA0000000 | PHYS_ITEX_BASE)   /* uncached ITeX Card */

/*****************************************************************************/
/*  Select the PLL value to get the desired CPU clock frequency.             */
/*                                                                           */
/*  Use the following equation to calculate what value should be used for    */
/*  a given clock frequency:                                                 */
/*                                                                           */
/*  freq = (value + 1) * XTALFREQ/2 MHz                                      */
/*****************************************************************************/
#define CLK_FREQ_122MHz     8     /* Fastest operation supported. */
#define CLK_FREQ_108MHz     7
#define CLK_FREQ_95MHz      6
#define CLK_FREQ_81MHz      5
#define CLK_FREQ_68MHz      4
#define CLK_FREQ_54MHz      3
#define CLK_FREQ_50MHz      3
#define CLK_FREQ_41MHz      2
#define CLK_FREQ_27MHz      1
#define CLK_FREQ_14MHz      0

#define XTALFREQ            25000000

// 635X board sdram type definition and flash location
#define MEMORY_635X_16MB_1_CHIP         0
#define MEMORY_635X_32MB_2_CHIP         1
#define MEMORY_635X_64MB_2_CHIP         2
// board memory type offset
#define SDRAM_TYPE_ADDRESS_OFFSET       16
#define BOARD_SDRAM_TYPE_ADDRESS        FLASH_BASE + SDRAM_TYPE_ADDRESS_OFFSET
#define BOARD_SDRAM_TYPE                *(unsigned long *)(FLASH_BASE + SDRAM_TYPE_ADDRESS_OFFSET)

/*****************************************************************************/
/*          board ioctl calls for flash, led and some other utilities        */
/*****************************************************************************/

/* GPIO Definitions */
#define GPIO_BOARD_ID_1             0x0020
#define GPIO_BOARD_ID_2             0x0040

/* Identify BCM96352 board type.
 * BCM86352SV - GPIO bit 6 = 1, GPIO bit 5 = 1 (GPIOIO = 0x60)
 * BCM86352R  - GPIO bit 6 = 0, GPIO bit 5 = 1 (GPIOIO = 0x20)
 * BCM96350   - GPIO bit 6 = 1, GPIO bit 5 = 0 (GPIOIO = 0x40)
 */
#define BOARD_ID_BCM9635X_MASK      (GPIO_BOARD_ID_1 | GPIO_BOARD_ID_2)
#define BOARD_ID_BCM96352SV         (GPIO_BOARD_ID_1 | GPIO_BOARD_ID_2)
#define BOARD_ID_BCM96352R          (GPIO_BOARD_ID_1)
#define BOARD_ID_BCM96350           (GPIO_BOARD_ID_2)

/*****************************************************************************/
/*          External interrupt mapping                                       */
/*****************************************************************************/
#define INTERRUPT_ID_ITEX   INTERRUPT_ID_EXTERNAL_0
#define MSI_IRQ             INTERRUPT_ID_EXTERNAL_1

#if __cplusplus
}
#endif

#endif /* _BOARD_H */
