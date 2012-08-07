/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Board device initialization		File: p5064_devs.c
    *  
    *  This is the "C" part of the board support package.  The
    *  routines to create and initialize the console, wire up 
    *  device drivers, and do other customization live here.
    *
    *  This init module is for the Algorithmics P5064 eval board.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */

#include "lib_types.h"
#include "lib_queue.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_console.h"
#include "env_subr.h"
#include "cfe.h"

#include "bsp_config.h"

#include "sbd.h"

#include "i82371sb.h"
#include "pcivar.h"
#include "pcireg.h"

#include "dev_ide.h"
#include "dev_flash.h"

/*  *********************************************************************
    *  Configuration options
    ********************************************************************* */


/*  *********************************************************************
    *  Devices we're importing
    ********************************************************************* */

extern cfe_driver_t ns16550_uart;		/* 16550 serial port */
extern cfe_driver_t promice_uart;		/* promice serial port */
extern cfe_driver_t piixidedrv;
extern cfe_driver_t pciidedrv;
extern cfe_driver_t pcmcia_pcic_drv;
extern cfe_driver_t piixatapidrv;
extern cfe_driver_t flashdrv;

#if CFG_VGACONSOLE
extern cfe_driver_t pcconsole;
static int vgainit_ok = 0;
extern int vga_biosinit(void);
extern int vga_probe(void);
#endif

/*  *********************************************************************
    *  Some board-specific parameters
    ********************************************************************* */

/*
 * Note!  Configure the PROMICE for burst mode zero (one byte per
 * access) and use the special region reserved for writing the flash
 * ROMs.  See the diagram on page 22 of the manual.  The address
 * 0x1F900000 is an alias for 0x1FC00000 except you can *only* do 
 * byte reads/writes
 */

#define PROMICE_BASE	(0x1F900000+511*1024)	/* 511K into the ROM */
#define PROMICE_WORDSIZE 1

extern int ui_init_p5064cmds(void);

/*  *********************************************************************
    *  board_console_init()
    *  
    *  Add the console device and set it to be the primary
    *  console.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void board_console_init(void)
{
#if CFG_PCI
    cfe_startflags = CFE_INIT_PCI;
#else
    cfe_startflags = 0;
#endif

    /* Console */
    cfe_add_device(&ns16550_uart,UART0_BASE,0,0);
    cfe_add_device(&promice_uart,PROMICE_BASE,PROMICE_WORDSIZE,0);
#if CFG_VGACONSOLE
    cfe_add_device(&pcconsole,0,0,0);
    cfe_set_console(CFE_BUFFER_CONSOLE);
#else
    cfe_set_console("uart0");
#endif
}

/*  *********************************************************************
    *  p5064_enable_ide()
    *  
    *  Enable the IDE port on the P5064 board.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void p5064_enable_ide(void)
{
#if CFG_PCI
    pcitag_t tag;

    if (pci_find_device(0x8086,0x7010,0,&tag) == 0) {
	pci_conf_write16(tag,0x40,0x8000);
	}
#endif
}

/*  *********************************************************************
    *  board_device_init()
    *  
    *  Initialize and add other devices.  Add everything you need
    *  for bootstrap here, like disk drives, flash memory, UARTs,
    *  network controllers, etc.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void board_device_init(void)
{
    flash_probe_t flash;

    /* Flash */
    flash.flash_phys = 0x1FC00000;
    flash.flash_prog_phys = 0x1F800000;
    flash.flash_flags = FLASH_FLG_NVRAM | FLASH_FLG_16BIT;
    flash.flash_size = 2*1024*1024;
    cfe_add_device(&flashdrv,0,0,&flash);


    /* Flash */
    flash.flash_phys = 0x1FE00000;
    flash.flash_prog_phys = 0x1FA00000;
    flash.flash_size = 2*1024*1024;
    flash.flash_flags = FLASH_FLG_NVRAM | FLASH_FLG_16BIT;
    cfe_add_device(&flashdrv,0,0,&flash);


    p5064_enable_ide();

    cfe_add_device(&piixidedrv,
		   ISAPORT_BASE(0x1F0),
		   IDE_PROBE_MASTER_TYPE(IDE_DEVTYPE_DISK) |
		   IDE_PROBE_SLAVE_TYPE(IDE_DEVTYPE_NOPROBE),
		   NULL);
#if CFG_PCI
    cfe_add_device(&pciidedrv,
		   0,
		   IDE_PROBE_MASTER_TYPE(IDE_DEVTYPE_AUTO) |
		   IDE_PROBE_SLAVE_TYPE(IDE_DEVTYPE_NOPROBE),
		   NULL);
#endif

    cfe_add_device(&pcmcia_pcic_drv,
		   0x100D0000,
		   IDE_PROBE_MASTER_TYPE(IDE_DEVTYPE_DISK) |
		   IDE_PROBE_SLAVE_TYPE(IDE_DEVTYPE_NOPROBE),
		   NULL);
#if CFG_VGACONSOLE
    cfe_ledstr("vgai");
    vgainit_ok = vga_biosinit();
    if (vgainit_ok == 0) cfe_set_console("pcconsole0");
    else cfe_set_console("uart0");
#endif
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
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
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
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void board_final_init(void)
{
    ui_init_p5064cmds();
}
