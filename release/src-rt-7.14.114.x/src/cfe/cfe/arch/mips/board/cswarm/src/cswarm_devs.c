/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Board device initialization		File: cswarm_devs.c
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
#include "sb1250_wid.h"

#include "bsp_config.h"

#include "cswarm.h"
#include "dev_ide.h"
#include "dev_newflash.h"

/*
 * This lets us override the WID by poking values into our PromICE 
 */
#ifdef _MAGICWID_
#undef A_SCD_SYSTEM_REVISION
#define A_SCD_SYSTEM_REVISION 0x1FC00010
#define SB1250_REFCLK (*((uint64_t *) 0xBFC00018))
#endif


/*  *********************************************************************
    *  Devices we're importing
    ********************************************************************* */

extern cfe_driver_t promice_uart;		/* promice serial port */
extern cfe_driver_t sb1250_uart;		/* SB1250 serial ports */
extern cfe_driver_t sb1250_ether;		/* SB1250 MACs */
extern cfe_driver_t sb1250_x1240eeprom;		/* Xicor SMBus NVRAM */
extern cfe_driver_t sb1250_24lc128eeprom;	/* Microchip EEPROM */
extern cfe_driver_t newflashdrv;		/* AMD-style flash */
extern cfe_driver_t idedrv;			/* IDE disk */
extern cfe_driver_t pcmciadrv;			/* PCMCIA card */
#if CFG_PCI
extern cfe_driver_t pciidedrv;			/* PCI IDE controller */
extern cfe_driver_t dc21143drv;                 /* Tulip Ethernet */
#endif

extern void ui_init_cpu1cmds(void);
extern void ui_init_swarmcmds(void);
extern int ui_init_corecmds(void);
extern int ui_init_soccmds(void);
extern int ui_init_testcmds(void);
extern int ui_init_resetcmds(void);
extern int ui_init_phycmds(void);
extern int ui_init_tempsensorcmds(void);
extern int ui_init_memtestcmds(void);
extern int ui_init_toyclockcmds(void);

#if CFG_VGACONSOLE
extern int vga_biosinit(void);
#endif

extern void sb1250_show_cpu_type(void);

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
    *  SysConfig switch settings
    ********************************************************************* */

#define CSWARM_PROMICE_CONSOLE	0x00000001

const unsigned int cswarm_startflags[8] = {
    0,						/* 0 : UART console, no PCI */
    CSWARM_PROMICE_CONSOLE,			/* 1 : PromICE console, no PCI */
    CFE_INIT_PCI,				/* 2 : UART console, PCI */
    CFE_INIT_PCI | CSWARM_PROMICE_CONSOLE,	/* 3 : PromICE console, PCI */
    0,						/* 4 : unused  */
    0,						/* 5 : unused */
    CFE_INIT_PCI | CFE_LDT_SLAVE,		/* 6 : 2, plus LDT slave mode */
    CFE_INIT_SAFE				/* 7 : UART console, no pci, safe mode */
};

static unsigned int cpu_pass;
static unsigned int cpu_revision;



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

    cpu_revision = (unsigned int) (SBREADCSR(A_SCD_SYSTEM_REVISION) & 
				   (M_SYS_PART | M_SYS_REVISION));
    cpu_pass = G_SYS_REVISION(cpu_revision);

    /* Console */
#if defined(_CSWARM_DIAG_CFG_) || defined(_CSWARM_DIAG3E_CFG_)
    cfe_add_device(&promice_uart,PROMICE_BASE,PROMICE_WORDSIZE,0);
    cfe_set_console("promice0");
#else
    cfe_add_device(&sb1250_uart,A_DUART,0,0);
    cfe_add_device(&promice_uart,PROMICE_BASE,PROMICE_WORDSIZE,0);

    /*
     * Read the config switch and decide how we are going to 
     * set up the console.  The config switch sets
     * bits 3..5 of the SYS_CONFIG field of the syscfg register.
     */

    cfe_startflags = cswarm_startflags[(G_SYS_CONFIG(syscfg)/8) & 0x07];

    if (cfe_startflags & CSWARM_PROMICE_CONSOLE) {
	cfe_set_console("promice0");
	}
    else {
	cfe_set_console("uart0");
	}
#endif

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

#if CFG_VGACONSOLE
    vga_biosinit();
#endif

    /* 
     * NVRAM (environment variables
     */
    cfe_add_device(&sb1250_x1240eeprom,X1240_SMBUS_CHAN,X1240_SMBUS_DEV,0);
    cfe_set_envdevice("eeprom0");	/* Connect NVRAM subsystem to EEPROM */
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

    /* 
     * Boot ROM 
     */
#if !defined(_CSWARM_DIAG_CFG_)
    cfe_add_device(&newflashdrv,
		   BOOTROM_PHYS,
		   (BOOTROM_SIZE*K64) | FLASH_FLG_BUS8 | FLASH_FLG_DEV16,
		   NULL);
    cfe_add_device(&newflashdrv,
		   ALT_BOOTROM_PHYS,
		   (ALT_BOOTROM_SIZE*K64) | FLASH_FLG_BUS8 | FLASH_FLG_DEV16,
		   NULL);
#endif

    cfe_add_device(&sb1250_24lc128eeprom,BIGEEPROM_SMBUS_CHAN,BIGEEPROM_SMBUS_DEV,0);

    /* 
     * MACs - must init after environment, since the hw address is stored there 
     */
    cfe_add_device(&sb1250_ether,A_MAC_BASE_0,0,env_getenv("ETH0_HWADDR"));
#ifndef _CSWARM_DIAG_CFG_
    cfe_add_device(&sb1250_ether,A_MAC_BASE_1,1,env_getenv("ETH1_HWADDR"));
    cfe_add_device(&sb1250_ether,A_MAC_BASE_2,2,env_getenv("ETH2_HWADDR"));
#endif

    /* 
     * IDE disks and PCMCIA
     */
#if !defined(_CSWARM_DIAG_CFG_) && !defined(_CSWARM_DIAG3E_CFG_)
    /*
     * Uncomment to enable IDE disks
     *
     * cfe_add_device(&idedrv,IDE_PHYS+(0x1F0<<5),
     *		   IDE_PROBE_MASTER_TYPE(IDE_DEVTYPE_DISK) |
     *		   IDE_PROBE_SLAVE_TYPE(IDE_DEVTYPE_NOPROBE),
     *	 	   NULL);
     */

    /*
     * Uncomment to enable PCMCIA support
     *
     *  cfe_add_device(&pcmciadrv,PCMCIA_PHYS,0,NULL);
     */
#endif

#if CFG_PCI
    /*
     * Add some sample PCI devices
     */
    if (cfe_startflags & CFE_INIT_PCI) {
	cfe_add_device(&pciidedrv,0,IDE_PROBE_MASTER_TYPE(IDE_DEVTYPE_DISK),NULL);
	cfe_add_device(&dc21143drv,0,0,env_getenv("TULIP0_HWADDR"));
	}
#endif

    /*
     * Set variable that contains CPU speed, spit out config register
     */

    syscfg = SBREADCSR(A_SCD_SYSTEM_CFG);
    printf("Config switch: %d\n",G_SYS_CONFIG(syscfg));

#ifdef _MAGICWID_
    printf("Reference Clock: %dMhz\n",(uint32_t)SB1250_REFCLK);
#endif

    sb1250_show_cpu_type();
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
    ui_init_cpu1cmds();
    ui_init_swarmcmds();
    ui_init_corecmds();
    ui_init_soccmds();
    ui_init_testcmds();
    ui_init_resetcmds();
    ui_init_tempsensorcmds();
    ui_init_memtestcmds();
    ui_init_toyclockcmds();
    ui_init_phycmds();
}
