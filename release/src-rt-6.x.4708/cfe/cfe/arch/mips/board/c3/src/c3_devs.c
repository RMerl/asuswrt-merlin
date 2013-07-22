/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Board device initialization		File: c3_devs.c
    *  
    *  This is the "C" part of the board support package.  The
    *  routines to create and initialize the console, wire up 
    *  device drivers, and do other customization live here.
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


#include "sbmips.h"
#include "lib_types.h"
#include "lib_queue.h"
#include "lib_printf.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_timer.h"
#include "env_subr.h"
#include "cfe.h"

#include "sb1250_defs.h"
#include "sb1250_regs.h"
#include "sb1250_pci.h"
#include "sb1250_ldt.h"
#include "sb1250_scd.h"
#include "dev_ide.h"
#include "dev_newflash.h"

#include "bsp_config.h"

#include "c3.h"

/*  *********************************************************************
    *  Devices we're importing
    ********************************************************************* */

extern cfe_driver_t promice_uart;		/* promice serial port */
extern cfe_driver_t sb1250_uart;		/* SB1250 serial ports */
extern cfe_driver_t sb1250_x1240eeprom;		/* Xicor SMBus NVRAM */
extern cfe_driver_t newflashdrv;	       	/* AMD-style flash */
extern cfe_driver_t jtagconsole;		/* SB1250 JTAG unit */

#ifdef _C3H_
#if CFG_PCI
extern cfe_driver_t pciidedrv;			/* PCI IDE controller */
extern cfe_driver_t dc21143drv;                 /* Tulip Ethernet */
#if CFG_DOWNLOAD
extern cfe_driver_t bcm1250drv;                 /* BCM1250 as a device */
#endif
#endif
#else
extern cfe_driver_t sb1250_pcihost;             /* driver for host downloads */
#endif  /* _C3H_ */


extern void ui_init_cpu1cmds(void);
extern int ui_init_corecmds(void);
extern int ui_init_soccmds(void);
extern int ui_init_testcmds(void);
extern int ui_init_resetcmds(void);
extern int ui_init_tempsensorcmds(void);
extern int ui_init_toyclockcmds(void);
extern int ui_init_memtestcmds(void);

extern int ui_init_c3_testcmds(void);

/*  *********************************************************************
    *  Some board-specific parameters
    ********************************************************************* */

/*
 * Note!  Configure the PROMICE for burst mode zero (one byte per
 * access).
 */

#define PROMICE_BASE	(0x1FDFFC00)
#define PROMICE_WORDSIZE 1

#define X1240_SMBUS_CHANNEL	1
#define X1240_SMBUS_DEVICE	0x57

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
    uint64_t syscfg;
    int plldiv;

    syscfg = SBREADCSR(A_SCD_SYSTEM_CFG);

    /* Console */
    cfe_add_device(&promice_uart,PROMICE_BASE,PROMICE_WORDSIZE,0);
    cfe_add_device(&sb1250_uart,A_DUART,0,0);

#if CFG_PCI
    cfe_startflags = CFE_INIT_PCI;
#else
    cfe_startflags = 0;
#endif

#ifdef _C3_DIAG_CFG_  
    cfe_set_console("promice0"); 
#else
    cfe_set_console("uart0");
#endif 

    /* CPU Speed : read from PLL field of SYSCFG register */
    plldiv = G_SYS_PLL_DIV(syscfg);
    if (plldiv == 0) {
	plldiv = 6;
    }
#ifdef _FUNCSIM_
    cfe_cpu_speed = 500000;		/* wire func sim at 500KHz */
#else
    cfe_cpu_speed = 50000000 * plldiv;
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
    uint64_t syscfg;

    /* Boot ROM */
#ifndef _C3_DIAG_CFG_  
    cfe_add_device(&newflashdrv,
		   BOOTROM_PHYS,
		   (BOOTROM_SIZE*K64) | FLASH_FLG_BUS8 | FLASH_FLG_DEV16,
		   NULL);
    cfe_add_device(&newflashdrv,
		   ALT_BOOTROM_PHYS,
		   (ALT_BOOTROM_SIZE*K64) | FLASH_FLG_BUS8 | FLASH_FLG_DEV16,
		   NULL);
#endif

#ifdef _C3H_      /* Host Mode */
#if CFG_PCI
    /*
     * Add some sample PCI devices
     */
    cfe_add_device(&pciidedrv,0,IDE_PROBE_MASTER_TYPE(IDE_DEVTYPE_DISK),NULL);
    cfe_add_device(&dc21143drv,0,0,env_getenv("TULIP0_HWADDR"));

#if CFG_DOWNLOAD
    /* Access to bcm1250 in PCI device mode */
    cfe_add_device(&bcm1250drv,0,0,NULL);
#endif
#endif
#else       /* Device Mode */
    /*
     * Host download interface.
     */
    cfe_add_device(&sb1250_pcihost,0,0,NULL);
#endif /* _C3H_ */

    syscfg = SBREADCSR(A_SCD_SYSTEM_CFG);
    printf("SysCfg: %016llX [PLL_DIV: %d, IOB0_DIV: %s, IOB1_DIV: %s]\n",
	   syscfg,
	   (int)G_SYS_PLL_DIV(syscfg),
	   (syscfg & M_SYS_IOB0_DIV) ? "CPUCLK/3" : "CPUCLK/4",
	   (syscfg & M_SYS_IOB1_DIV) ? "CPUCLK/2" : "CPUCLK/3");
    if (G_SYS_PLL_DIV(syscfg) == 0) {
	printf("PLL_DIV of zero found, assuming 6 (300MHz)\n");
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
    ui_init_corecmds();
    ui_init_soccmds();
    ui_init_testcmds();
    ui_init_resetcmds();
    ui_init_tempsensorcmds();
    ui_init_toyclockcmds();
    ui_init_memtestcmds();

#ifndef _C3H_
    ui_init_c3_testcmds();
    {
    extern int cfe_device_download(int boot, char *options);
    /* cfe_device_download(1, ""); */
    }
#endif
}
