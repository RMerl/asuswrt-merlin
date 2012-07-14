/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Board device initialization		File: bigsur_devs.c
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

#include "bcm1400_defs.h"
#include "bcm1400_regs.h"
#include "bcm1400_scd.h"
#ifdef NYI
#include "sb1250_smbus.h"
#endif

#include "bsp_config.h"

#include "bigsur.h"

#include "dev_newflash.h"

/*  *********************************************************************
    *  Devices we're importing
    ********************************************************************* */

extern cfe_driver_t promice_uart;		/* promice serial port */
extern cfe_driver_t bcm1400_uart;		/* BCM1400 serial ports */
extern cfe_driver_t sb1250_ether;		/* SB1250 MAC */
extern cfe_driver_t newflashdrv;		/* AMD-style flash */
#ifdef NYI
extern cfe_driver_t sb1250_24lc128eeprom;	/* Microchip EEPROM */
extern cfe_driver_t m41t81_clock;		/* M41T81 SMBus RTC */
#endif

#if CFG_PCI
extern void pci_add_devices(int init);
#endif

/*  *********************************************************************
    *  Commands we're importing
    ********************************************************************* */

extern void ui_init_cpu1cmds(void);
extern void ui_init_swarmcmds(void);
extern int ui_init_corecmds(void);
extern int ui_init_soccmds(void);
extern int ui_init_testcmds(void);
extern int ui_init_resetcmds(void);
extern int ui_init_phycmds(void);
extern int ui_init_tempsensorcmds(void);
extern int ui_init_toyclockcmds(void);
extern int ui_init_memtestcmds(void);
extern int ui_init_ethertestcmds(void);
extern int ui_init_flashtestcmds(void);
extern int ui_init_testcmds2(void);
extern int ui_init_tracecmds(void);

/*  *********************************************************************
    *  Some other stuff we use
    ********************************************************************* */

extern void sb1250_show_cpu_type(void);
extern int cfe_device_download(int boot, char *options);

/*  *********************************************************************
    *  Some board-specific parameters
    ********************************************************************* */

/*
 * Note!  Configure the PROMICE for burst mode zero (one byte per
 * access).
 */

#define PROMICE_BASE	(0x1FDFFC00)
#define PROMICE_WORDSIZE 1

#define REAL_BOOTROM_SIZE	(2*1024*1024)	/* region is 4MB, but rom is 2MB */

/*  *********************************************************************
    *  SysConfig switch settings and related parameters
    ********************************************************************* */

int bigsur_board_rev;
int bigsur_config_switch;

#define SWARM_PROMICE_CONSOLE	0x00000001
#define DEVICE_DOWNLOAD         0x00000002
#define DEVICE_EXECUTE          0x00000004
#define DEVICE_REBOOT           (DEVICE_DOWNLOAD|DEVICE_EXECUTE)

const unsigned int bigsur_startflags[16] = {
    0,						/* 0 : UART console, no PCI */
    SWARM_PROMICE_CONSOLE,			/* 1 : PromICE console, no PCI */
    CFE_INIT_PCI,				/* 2 : UART console, PCI */
    CFE_INIT_PCI | SWARM_PROMICE_CONSOLE,	/* 3 : PromICE console, PCI */
    0,						/* 4 : unused  */
    0,						/* 5 : unused */
    CFE_INIT_PCI | CFE_LDT_SLAVE,		/* 6 : 2, plus LDT slave mode */
    CFE_INIT_SAFE,				/* 7 : UART console, no pci, safe mode */
    CFE_INIT_PCI | DEVICE_DOWNLOAD,		/* 8 : UART console, PCI, download */
    CFE_INIT_PCI | DEVICE_REBOOT,		/* 9 : UART console, PCI, reboot */
    0,						/* 10 : unused */
    0,						/* 11 : unused */
    0,						/* 12 : unused */
    0,						/* 13 : unused */
    0,						/* 14 : unused */
    0,						/* 15 : unused */
};


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
    cfe_add_device(&bcm1400_uart,A_DUART(0),0,0);
    cfe_add_device(&promice_uart,PROMICE_BASE,PROMICE_WORDSIZE,0);

    /*
     * Read the config switch and decide how we are going to set up
     * the console.  This is actually board revision dependent.
     *
     * Note that the human-readable board revision is (bigsur_board_rev+1).
     */
    bigsur_board_rev = G_SYS_CONFIG(syscfg) & 0x3;
    bigsur_config_switch = (G_SYS_CONFIG(syscfg) >> 2) & 0x0F;

    cfe_startflags = bigsur_startflags[bigsur_config_switch];

    if (cfe_startflags & SWARM_PROMICE_CONSOLE) {
	cfe_set_console("promice0");
	}
    else {
	cfe_set_console("uart0");
	}

    /*
     * Set variable that contains CPU speed, spit out config register
     */

    plldiv = G_SYS_PLL_DIV(syscfg);
    if (plldiv == 0) {
	plldiv = 6;
	}

#ifdef _FUNCSIM_
    cfe_cpu_speed = 500000;			/* wire func sim at 500KHz */
#else
    cfe_cpu_speed = 50000000 * plldiv;		/* use PLL divisor */
#endif

#if !CFG_MINIMAL_SIZE
#ifdef NYI
    /* 
     * NVRAM (environment variables)
     */

    switch (bigsur_board_rev) {
	case BIGSUR_REV_1:
	default:
	    /* 
	     * Add the NVRAM device on SMBus 1 first.  This will be our environment
	     */
	     
	    cfe_add_device(&sb1250_24lc128eeprom,BIGEEPROM1_SMBUS_CHAN,BIGEEPROM1_SMBUS_DEV,0);

	}

    cfe_set_envdevice("eeprom0");	/* Connect NVRAM subsystem to EEPROM */
#endif
#endif /* CFG_MINIMAL_SIZE */
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
    /*
     * Print out the board version number.
     */
    printf("%s board revision %d\n", CFG_BOARDNAME, bigsur_board_rev + 1);

#ifndef _FUNCSIM_
    /* 
     * Boot ROM, using "new" flash driver
     */

    cfe_add_device(&newflashdrv,
		   BOOTROM_PHYS,
		   REAL_BOOTROM_SIZE | FLASH_FLG_BUS8 | FLASH_FLG_DEV16,
		   NULL);
    cfe_add_device(&newflashdrv,
		   ALT_BOOTROM_PHYS,
		   REAL_BOOTROM_SIZE | FLASH_FLG_BUS8 | FLASH_FLG_DEV16,
		   NULL);
#endif /*_FUNCSIM_*/

#if !CFG_MINIMAL_SIZE
    /* 
     * MAC - must init after environment, since the hw address is stored there 
     */
    cfe_add_device(&sb1250_ether,A_MAC_BASE_0,0,env_getenv("ETH0_HWADDR"));

#if CFG_PCI
    pci_add_devices(cfe_startflags & CFE_INIT_PCI);
#endif

#ifdef NYI
    /*
     * Real-time clock 
     */

    switch (bigsur_board_rev) {
	case BIGSUR_REV_1:
	default:
	  /* Add ST Micro clock driver here */
	  cfe_add_device(&m41t81_clock,M41T81_SMBUS_CHAN,M41T81_SMBUS_DEV,0);
	}
#endif
#endif

    /*
     * Set variable that contains CPU speed, spit out config register
     */

    printf("Config switch: %d\n", bigsur_config_switch);

#ifdef NYI
    sb1250_show_cpu_type();
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
#if !CFG_MINIMAL_SIZE
    ui_init_cpu1cmds();
    ui_init_swarmcmds();
#ifdef NYI
    ui_init_corecmds();
#endif
    ui_init_soccmds();
#ifdef NYI
    ui_init_testcmds();
    ui_init_testcmds2();
#endif
    ui_init_resetcmds();
#ifdef NYI
    ui_init_tempsensorcmds();
    ui_init_toyclockcmds();
    ui_init_memtestcmds();
    ui_init_tracecmds();
#endif
    ui_init_phycmds();
    ui_init_ethertestcmds();
    ui_init_flashtestcmds();

#endif
}
