/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  Board device initialization      File: bcm94710_devs.c
    *
    *  This is the "C" part of the board support package.  The
    *  routines to create and initialize the console, wire up
    *  device drivers, and do other customization live here.
    *
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *
    *********************************************************************
    *
    *  XX Copyright 2000,2001
    *  Broadcom Corporation. All rights reserved.
    *
    *  BROADCOM PROPRIETARY AND CONFIDENTIAL
    *
    *  This software is furnished under license and may be used and
    *  copied only in accordance with the license.
    ********************************************************************* */

#include "lib_types.h"
#include "lib_printf.h"
#include "cfe_timer.h"
#include "cfe.h"
#include "6345_map.h"
#include "board.h"
#include "bcmTag.h"
#include "dev_bcm63xx_flash.h"
#include "bcm63xx_util.h"
#include "cfiflash.h"

void checkForResetHold(void);
void kerSysMipsSoftReset(void);
int sysGetBoardId(void);
const char *get_system_type(void);
PFILE_TAG kerSysImageTagGet(void);

/*  *********************************************************************
    *  Devices we're importing
    ********************************************************************* */

extern cfe_driver_t bcm63xx_uart;
extern cfe_driver_t bcm6345_enet;
extern cfe_driver_t flashdrv;

/*  *********************************************************************
    *  Some board-specific parameters
    ********************************************************************* */


/*  *********************************************************************
    *  board_console_init()
    *
    *  Add the console device and set it to be the primary
    *  console.
    *
    *  Input parameters:
    *      nothing
    *
    *  Return value:
    *      nothing
    ********************************************************************* */

void board_console_init(void)
{
    /* Add the serial port driver. */
    cfe_add_device(&bcm63xx_uart,0xFFFE0300,0,0);

    cfe_set_console( "uart0" );
}


/*  *********************************************************************
    *  board_device_init()
    *
    *  Initialize and add other devices.  Add everything you need
    *  for bootstrap here, like disk drives, flash memory, UARTs,
    *  network controllers, etc.
    *
    *  Input parameters:
    *      nothing
    *
    *  Return value:
    *      nothing
    ********************************************************************* */

void board_device_init(void)
{
    PFILE_TAG pTag = NULL;

    /* Add the ethernet driver. */
    cfe_add_device( &bcm6345_enet, 0, 0, 0);

    kerSysFlashInit();

    if ((pTag = kerSysImageTagGet()))
    {
        int flash_addr_kernel = atoi(pTag->kernelAddress) + TAG_LEN;
        int flash_len_kernel = atoi(pTag->kernelLen);
        if( flash_len_kernel > 0 )
            cfe_add_device(&flashdrv, flash_addr_kernel, flash_len_kernel, NULL);
        else
            printf( "Kernel image not found on flash.\n" );
    }
    else
    {
        // Used to load an RTEMS image.
        cfe_add_device(&flashdrv, FLASH45_IMAGE_START_ADDR, 0x100000, NULL);
        printf("\r\n** Using flash start address 0x%8.8lx **\r\n\r\n",
            FLASH45_IMAGE_START_ADDR);
    }
}



/*  *********************************************************************
    *  board_device_reset()
    *
    *  Reset devices.  This call is done when the firmware is restarted,
    *  as might happen when an operating system exits, just before the
    *  "reset" command is applied to the installed devices.   You can
    *  do whatever board-specific things are here to keep the system
    *  stable, like stopping DMA sources, interrupts, etc.
    *
    *  Input parameters:
    *      nothing
    *
    *  Return value:
    *      nothing
    ********************************************************************* */

void board_device_reset(void)
{
}


/*  *********************************************************************
    *  board_final_init()
    *
    *  Do any final initialization, such as adding commands to the
    *  user interface.
    *
    *  If you don't want a user interface, put the startup code here.
    *  This routine is called just before CFE starts its user interface.
    *
    *  Input parameters:
    *      nothing
    *
    *  Return value:
    *      nothing
    ********************************************************************* */

void board_final_init(void)
{           
    checkForResetHold();
    bcm63xx_run();
}

/*  *********************************************************************
    * Miscellaneous Board Functions
    ********************************************************************* */

/*  *********************************************************************
    *  checkForResetHold()
    *
    *  Determines if the user is holding the reset button in a depressed
    *  state for 3 seconds.  If it is, default settings are restored.
    *
    *  Input parameters:
    *      nothing
    *
    *  Return value:
    *      nothing
    ********************************************************************* */

void checkForResetHold(void)
{
    const int nResetHoldDelay = 3;
    int i;

    for (i = 0; i < nResetHoldDelay; i++)
    {
        if ((GPIO->GPIOio & (unsigned short) GPIO_PRESS_AND_HOLD_RESET))
            break;
        cfe_sleep(CFE_HZ/2);
    }
    if (i == nResetHoldDelay)
    {
        printf("\n*** Restore to Factory Default Setting ***\n\n");
        kerSysErasePsi();
        flash_reset();
        setDefaultBootline();
    }

}


/*  *********************************************************************
    *  kerSysMipsSoftReset()
    *
    *  Resets the board.
    *
    *  Input parameters:
    *      nothing
    *
    *  Return value:
    *      nothing
    ********************************************************************* */
void kerSysMipsSoftReset(void)
{
    INTC->pll_control |= SOFT_RESET;    // soft reset mips
}

/*  *********************************************************************
    *  kerSysImageTagGet()
    *   Get the image tag
    *  Input parameters:
    *      none
    *  Return value:
    *      point to tag in flash -- Found
    *      NULL -- failed
    ********************************************************************* */
PFILE_TAG kerSysImageTagGet(void)
{
    int i;
    int totalBlks = flash_get_numsectors();
    UINT32 crc;
    unsigned char *sectAddr;
    PFILE_TAG pTag;

    // start from 2nd blk, assume 1st one is always CFE
    for (i = 2; i < totalBlks; i++)
    {
        sectAddr =  flash_get_memptr((byte) i);
        crc = CRC32_INIT_VALUE;
        crc = getCrc32(sectAddr, (UINT32)TAG_LEN-TOKEN_LEN, crc);      
        pTag = (PFILE_TAG) sectAddr;
        if (crc == (UINT32)(*(UINT32*)(pTag->tagValidationToken)))
            return pTag;
    }

    return (PFILE_TAG) NULL;
}

/*  *********************************************************************
    *  get_system_type()
    *
    *  Returns board id constant.
    *
    *  Input parameters:
    *      nothing
    *
    *  Return value:
    *      Board id constant.
    ********************************************************************* */

int sysGetBoardId( void )
{
    GPIO->GPIODir &= (unsigned short) ~BOARD_ID_BCM9634X_MASK;
    return( (int) (GPIO->GPIOio & BOARD_ID_BCM9634X_MASK) );
} /* sysGetBoardId */
