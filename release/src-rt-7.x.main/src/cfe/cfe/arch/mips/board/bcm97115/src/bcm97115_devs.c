/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  Board device initialization		File: bcm97115_devs.c
    *
    *  This is the "C" part of the board support package.  The
    *  routines to create and initialize the console, wire up
    *  device drivers, and do other customization live here.
    *
    *  Author:  Matt Carlson (mcarlson@broadcom.com)
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
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe.h"

#include "bcm97115.h"


/*  *********************************************************************
    *  Devices we're importing
    ********************************************************************* */

extern cfe_driver_t bcm97115_uart;
extern cfe_driver_t bcm4413drv;

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
	cfe_add_device( &bcm97115_uart, UARTA_BASE, 0, 0 );

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
	/* Modify some gpios so HD is released from reset. */
	*((volatile unsigned char *)0xfffe0057) &= 0xfd;
	*((volatile unsigned char *)0xfffe0053) |= 0x02;

	*((volatile unsigned short *)0xfffe0900) = 0x07FF;

	/* Set TM_TOP mux for LED */
	*(volatile unsigned char *)(0xfffe8008) = 0x00;
	*(volatile unsigned char *)(0xfffe009d) = 0x00;
	*(volatile unsigned char *)(0xfffe0092) = 0x00;
	*(volatile unsigned char *)(0xfffe0093) = 0x55;
	*(volatile unsigned char *)(0xfffe0094) = 0x01;
	*(volatile unsigned char *)(0xfffe0095) = 0xaa;
	*(volatile unsigned char *)(0xfffe009a) = 0x80;

	/* Add the ethernet driver. */
	cfe_add_device( &bcm4413drv, 0, 0, 0 );
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
}
