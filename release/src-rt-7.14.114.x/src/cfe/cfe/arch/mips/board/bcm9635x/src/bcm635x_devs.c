/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  Board device initialization        File: bcm94710_devs.c
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
#include "6352_map.h"
#include "board.h"
#include "bcmTag.h"
#include "dev_bcm63xx_flash.h"
#include "bcm63xx_util.h"
#include "flash.h"

void keySysMipsSoftReset(void);
void udelay(long us);
void dumpaddr( unsigned char *pAddr, int nLen );
int sysGetBoardId( void );
PFILE_TAG kerSysImageTagGet(void);

/*  *********************************************************************
    *  Devices we're importing
    ********************************************************************* */

extern cfe_driver_t bcm63xx_uart;
extern cfe_driver_t bcm6352_enet;
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
    *       nothing
    *
    *  Return value:
    *       nothing
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
    *       nothing
    *
    *  Return value:
    *       nothing
    ********************************************************************* */

void board_device_init(void)
{
    PFILE_TAG pTag;

    kerSysFlashInit();

    /* Add the ethernet driver. */
    cfe_add_device( &bcm6352_enet, 0, 0, 0 );

    if ((pTag = kerSysImageTagGet()))
    {
        int flash_addr_kernel = atoi(pTag->kernelAddress) + TAG_LEN;
        int flash_len_kernel = atoi(pTag->kernelLen);
        if( flash_len_kernel > 0 )
        {
            printf( "Run from flash address=0x%8.8lx, length=%lu\n",
                (unsigned long) flash_addr_kernel,
                (unsigned long) flash_len_kernel );
            cfe_add_device(&flashdrv, flash_addr_kernel, flash_len_kernel, NULL);
        }
        else
            printf( "Kernel image not found on flash.\n" );
    }
    else
        printf("Failed to read image tag from flash\n");
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
    *       nothing
    *
    *  Return value:
    *       nothing
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
    *       nothing
    *
    *  Return value:
    *       nothing
    ********************************************************************* */

void board_final_init(void)
{
    bcm63xx_run();
}

/*  *********************************************************************
    * Miscellaneous Board Functions
    ********************************************************************* */

/***********************************************************************
 * Function Name: kerSysMipsSoftReset
 * Description  : Resets the board.
 * Returns      : None.
 ***********************************************************************/
void kerSysMipsSoftReset(void)
{
    INTC->control |= SOFT_RESET;
}

/***********************************************************************
 * Function Name: dumpaddr
 * Description  : Delay for microseconds by reading the BCM6352 CP0 count
 *                register.  The count register is updated every other tick.
 *                Therefore,
 *
 *                1 / (225 MHz / 2) ~= 9 ns per count register increment
 *                9 ns * 111 ~= 1 us
 *
 * Returns      : None.
 ***********************************************************************/

/* time_after(a,b) returns true if the time a is after time b.*/
#define time_after(a,b)     ((long)(b) - (long)(a) < 0)
extern long _getticks(void);

void udelay(long us)
{
    long t = _getticks() + (us * 111);
    while( time_after(t, _getticks()) )
        ;
}

/***********************************************************************
 * Function Name: dumpaddr
 * Description  : Display a hex dump of the specified address.
 * Returns      : None.
 ***********************************************************************/
void dumpaddr( unsigned char *pAddr, int nLen )
{
    static char szHexChars[] = "0123456789abcdef";
    char szLine[80];
    char *p = szLine;
    unsigned char ch, *q;
    int i, j;
    unsigned long ul;

    while( nLen > 0 )
    {
        sprintf( szLine, "%8.8lx: ", (unsigned long) pAddr );
        p = szLine + strlen(szLine);

        for(i = 0; i < 16 && nLen > 0; i += sizeof(long), nLen -= sizeof(long))
        {
            ul = *(unsigned long *) &pAddr[i];
            q = (unsigned char *) &ul;
            for( j = 0; j < sizeof(long); j++ )
            {
                *p++ = szHexChars[q[j] >> 4];
                *p++ = szHexChars[q[j] & 0x0f];
                *p++ = ' ';
            }
        }

        for( j = 0; j < 16 - i; j++ )
            *p++ = ' ', *p++ = ' ', *p++ = ' ';

        *p++ = ' ', *p++ = ' ', *p++ = ' ';

        for( j = 0; j < i; j++ )
        {
            ch = pAddr[j];
            *p++ = (ch > ' ' && ch < '~') ? ch : '.';
        }

        *p++ = '\0';
        printf( "%s\r\n", szLine );

        pAddr += i;
    }
    printf( "\r\n" );
} /* dumpaddr */


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
