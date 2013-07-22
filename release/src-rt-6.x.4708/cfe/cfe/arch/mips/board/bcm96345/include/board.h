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

/* GPIO Definitions */
#define GPIO_BOARD_ID_1             0x0020
#define GPIO_BOARD_ID_2             0x0040
#define GPIO_BOARD_ID_3             0x0080
#define GPIO_LED_DSL_LINK           0x0100
#define GPIO_LED_PPP_CONNECTION     0x0200
#define GPIO_GP_LED                 0x0400
#define GPIO_RJ11_INNER_PAIR        0x0800
#define GPIO_RJ11_OUTER_PAIR        0x1000
#define GPIO_PRESS_AND_HOLD_RESET   0x2000
#define GPIO_DYING_GASP             0x4000

/* Identify BCM96345 board type by checking GPIO bits.
 * GPIO bit 7 6 5    Board type
 *          0 0 0    Undefined
 *          0 0 1    Undefined
 *          0 1 0    Undefined
 *          0 1 1    USB
 *          1 0 0    R 1.0
 *          1 0 1    I
 *          1 1 0    SV
 *          1 1 1    R 0.0
 */
#define BOARD_ID_BCM9634X_MASK  (GPIO_BOARD_ID_1|GPIO_BOARD_ID_2|GPIO_BOARD_ID_3) 
#define BOARD_ID_BCM96345SV     (GPIO_BOARD_ID_2|GPIO_BOARD_ID_3)
#define BOARD_ID_BCM96345R00    (GPIO_BOARD_ID_1|GPIO_BOARD_ID_2|GPIO_BOARD_ID_3)
#define BOARD_ID_BCM96345I      (GPIO_BOARD_ID_1|GPIO_BOARD_ID_3)
#define BOARD_ID_BCM96345R10    (GPIO_BOARD_ID_3)
#define BOARD_ID_BCM96345USB    (GPIO_BOARD_ID_1|GPIO_BOARD_ID_2)
#define BOARD_ID_BCM96345GW     (GPIO_BOARD_ID_2)

/*****************************************************************************/
/*                    Physical Memory Map                                    */
/*****************************************************************************/

#define PHYS_DRAM_BASE           0x00000000     /* Dynamic RAM Base */
#define PHYS_FLASH_BASE          0x1FC00000     /* Flash Memory         */

/*****************************************************************************/
/* Note that the addresses above are physical addresses and that programs    */
/* have to use converted addresses defined below:                            */
/*****************************************************************************/
#define DRAM_BASE           (0x80000000 | PHYS_DRAM_BASE)   /* cached DRAM */
#define DRAM_BASE_NOCACHE   (0xA0000000 | PHYS_DRAM_BASE)   /* uncached DRAM */
#define FLASH_BASE          (0xA0000000 | PHYS_FLASH_BASE)  /* uncached Flash  */

/*****************************************************************************/
/*  Select the PLL value to get the desired CPU clock frequency.             */
/*                                                                           */
/*                                                                           */
/*****************************************************************************/
#define FPERIPH            50000000

    
// 6345 board sdram type definition and flash location
#define MEMORY_8MB_1_CHIP               0
#define MEMORY_16MB_1_CHIP              1
#define MEMORY_32MB_1_CHIP              2
#define MEMORY_64MB_2_CHIP              3

// board memory type offset
#define SDRAM_TYPE_ADDRESS_OFFSET       16
#define BOARD_SDRAM_TYPE_ADDRESS        FLASH_BASE + SDRAM_TYPE_ADDRESS_OFFSET
#define BOARD_SDRAM_TYPE                *(unsigned long *)(FLASH_BASE + SDRAM_TYPE_ADDRESS_OFFSET)

#if __cplusplus
}
#endif

#endif /* _BOARD_H */
