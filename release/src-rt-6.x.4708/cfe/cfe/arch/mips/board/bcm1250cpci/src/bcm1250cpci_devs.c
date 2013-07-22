/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Board device initialization		File: bcm1250cpci_devs.c
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
#include "sb1250_smbus.h"
#include "dev_ide.h"
#include "dev_newflash.h"

#include "bsp_config.h"

#include "bcm1250cpci.h"


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
extern cfe_driver_t x1241_clock;		/* Xicor SMBus RTC */
extern cfe_driver_t m41t81_clock;		/* M41T81 SMBus RTC */

extern cfe_driver_t sb1250_pcihost;             /* driver for host downloads */
#if CFG_PCI
extern cfe_driver_t pciidedrv;			/* PCI IDE controller */
extern cfe_driver_t dc21143drv;     		/* Tulip Ethernet */
#if CFG_DOWNLOAD
extern cfe_driver_t bcm1250drv;     		/* BCM1250 as a device */
#endif
#endif

extern int ui_init_cpu1cmds(void);
extern int ui_init_bcm1250cpcicmds(void);
extern int ui_init_usbcmds(void);
extern int ui_init_corecmds(void);
extern int ui_init_soccmds(void);
extern int ui_init_testcmds(void);
extern int ui_init_toyclockcmds(void);
extern int ui_init_tempsensorcmds(void);
extern int ui_init_memtestcmds(void);
extern int ui_init_resetcmds(void);
extern int ui_init_phycmds(void);

extern int cfe_device_download(int boot, char *options);

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
    *  SysConfig switch settings and related parameters
    ********************************************************************* */

int bcm1250cpci_board_rev;
int bcm1250cpci_config_switch;

#define SWARM_PROMICE_CONSOLE	0x00000001
#define DEVICE_DOWNLOAD         0x00000002
#define DEVICE_EXECUTE          0x00000004
#define DEVICE_REBOOT           (DEVICE_DOWNLOAD|DEVICE_EXECUTE)

const unsigned int bcm1250cpci_startflags[16] = {
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


unsigned int cpu_revision;

#define SB1250_PASS1 (V_SYS_PART(K_SYS_PART_SB1250) | V_SYS_REVISION(K_SYS_REVISION_PASS1))
#define SB1250_PASS2 (V_SYS_PART(K_SYS_PART_SB1250) | V_SYS_REVISION(K_SYS_REVISION_PASS2))

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

    /* Console */
    cfe_add_device(&sb1250_uart,A_DUART,0,0);				/* BCM 1250 UART A */
    cfe_add_device(&promice_uart,PROMICE_BASE,PROMICE_WORDSIZE,0);	/* External PROMICE UART */

    /*
     * Read the config switch and decide how we are going to set up
     * the console.  This is actually board revision dependent.
     *
     * Note that the human-readable board revision is (swarm_board_rev+1).
     */

    bcm1250cpci_board_rev = G_SYS_CONFIG(syscfg) & 0x3;

    bcm1250cpci_config_switch = (G_SYS_CONFIG(syscfg) >> 2) & 0x0F;

    cfe_startflags = bcm1250cpci_startflags[bcm1250cpci_config_switch];

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
    cfe_cpu_speed = 500000;					/* wire func sim at 500KHz */
#else
    cfe_cpu_speed = 50000000 * plldiv;		/* use PLL divisor */
#endif

    /* 
     * NVRAM (environment variables
	 In the first version of the board, The Xicor X1240 was suppose to be the NVRAM.
	 Due to the part being obsoleted, It was decided to give up boot capability from the
	 EEPROM to allow for a consistent NVRAM storage area
     */

    cfe_add_device(&sb1250_24lc128eeprom,M24LC128_0_SMBUS_CHAN,M24LC128_0_SMBUS_DEV,0);
    cfe_set_envdevice("eeprom0");			/* Connect NVRAM subsystem to EEPROM */

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
    printf("%s board revision %d\n", CFG_BOARDNAME, bcm1250cpci_board_rev + 1);

    /*
     * UART channel B
     */

    cfe_add_device(&sb1250_uart,A_DUART,1,0);

    /* 
     * Boot ROM 
     */

    cfe_add_device(&newflashdrv,
		   BOOTROM_PHYS,
		   (BOOTROM_SIZE*K64) | FLASH_FLG_BUS8 | FLASH_FLG_DEV16,
		   NULL);
    cfe_add_device(&newflashdrv,
		   ALT_BOOTROM_PHYS,
		   (ALT_BOOTROM_SIZE*K64) | FLASH_FLG_BUS8 | FLASH_FLG_DEV16,
		   NULL);
#ifdef _FUNCSIM_
    cfe_add_device(&newflashdrv,0x11000000,64*1024*1024,NULL);
#endif


	/* 
	Add Second EEPROM if not board rev 0
	NOTE: Board rev 0 has the Xicor X1251 clock chip
	NOTE: Board rev 1 has a second 24lc128 chip
	*/
    switch (bcm1250cpci_board_rev) {
	case 0:
    cfe_add_device(&sb1250_x1240eeprom,X1240_SMBUS_CHAN,X1240_SMBUS_DEV,0);
	break;

	default:
    cfe_add_device(&sb1250_24lc128eeprom,M24LC128_1_SMBUS_CHAN,M24LC128_1_SMBUS_DEV,0);
	break;
	
	}

    /* 
     * MACs - must init after environment, since the hw address is stored there 
     */
    cfe_add_device(&sb1250_ether,A_MAC_BASE_0,0,env_getenv("ETH0_HWADDR"));
    cfe_add_device(&sb1250_ether,A_MAC_BASE_1,1,env_getenv("ETH1_HWADDR"));
    cfe_add_device(&sb1250_ether,A_MAC_BASE_2,2,env_getenv("ETH2_HWADDR"));

    /* 
     * IDE disks and PCMCIA
     */
    cfe_add_device(&idedrv,IDE_PHYS+(0x1F0<<5),
     		   IDE_PROBE_MASTER_TYPE(IDE_DEVTYPE_DISK) |
     		   IDE_PROBE_SLAVE_TYPE(IDE_DEVTYPE_NOPROBE),
     	 	   NULL);

    /*
     * Enable PCMCIA support
     * NOTE : PCMCIA support is not completely working on REV 0 of the board.  There are some
     * Crossed signals with board detect, voltage detect.
     */
    switch (bcm1250cpci_board_rev) {
	case 0:
	    break;

	default:
	    cfe_add_device(&pcmciadrv,PCMCIA_PHYS,0,NULL);
	    break;
	}

#if CFG_PCI
    /*
     * Add some sample PCI devices
     */
     if (cfe_startflags & CFE_INIT_PCI) {
	 cfe_add_device(&pciidedrv,0,IDE_PROBE_MASTER_TYPE(IDE_DEVTYPE_DISK),NULL);
	 cfe_add_device(&dc21143drv,0,0,env_getenv("TULIP0_HWADDR"));

#if CFG_DOWNLOAD
	 /* Access to bcm1250 in PCI device mode */
	 cfe_add_device(&bcm1250drv,0,0,NULL);
     printf("Adding PCI download driver\n");
#endif
	 }
#endif

     /*
      * Clock
      * NOTE : On rev 0 of the board, the clock supplied is the X1241 and does not work on some boards
      * NOTE : On rev 1 of the board, the clock supplied is the ST and does not work on some boards
      */
    switch (bcm1250cpci_board_rev) {
	case 0:
    cfe_add_device(&x1241_clock,X1240_SMBUS_CHAN,X1240_SMBUS_DEV,0);
	break;

	default:
	cfe_add_device(&m41t81_clock,M41T81_SMBUS_CHAN,M41T81_SMBUS_DEV,0);
	break;
	
	}

    /*
     * Host download interface.
     */
//    cfe_add_device(&sb1250_pcihost,0,0,NULL);


    /*
     * Set variable that contains CPU speed, spit out config register
     */


    printf("Config switch: %d\n", bcm1250cpci_config_switch);

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
	/* Nothing to be done */
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

    ui_init_cpu1cmds();       		/* Add CPU1 command for running VxWorks */
    ui_init_bcm1250cpcicmds();		/* Add bcm1250 specific cmds */ 
    ui_init_usbcmds();          	/* Add USB commands */
    ui_init_corecmds();			/* Add bcm1250 core commands */
    ui_init_soccmds();			/* Add system on chip commands */
    ui_init_testcmds();			/* Add test commands */
    ui_init_toyclockcmds();		/* Add clock commands */
    ui_init_tempsensorcmds();		/* Add temp sensor commands */
    ui_init_memtestcmds();		/* Add memory test commands */
    ui_init_resetcmds();		/* Add reset commands */
    ui_init_phycmds();			/* Add phy commands commands */

	/* If download mode is selected, then setup for download receive */
    if (cfe_startflags & DEVICE_DOWNLOAD) {
	cfe_device_download((cfe_startflags & DEVICE_EXECUTE), "");        
	}

}
