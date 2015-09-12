/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Board device initialization		File: lausanne_devs.c
    *  
    *  This is the "C" part of the board support package.  The
    *  routines to create and initialize the console, wire up 
    *  device drivers, and do other customization live here.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  Lausanne edits:  Jeffrey Cheng (chengj@broadcom.com)
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
#include "sb1250_scd.h"

#include "bsp_config.h"

#include "lausanne.h"
#include "dev_ide.h"

/*  *********************************************************************
    *  Devices we're importing
    ********************************************************************* */

extern cfe_driver_t promice_uart;		/* promice serial port */
extern cfe_driver_t sb1250_uart;		/* SB1250 serial ports */
extern cfe_driver_t sb1250_ether;		/* SB1250 MACs */
extern cfe_driver_t sb1250_24lc128eeprom;	/* Microchip EEPROM */
extern cfe_driver_t lausanne_flashdrv;	        /* Intel-style flash */
#if CFG_PCI
extern void pci_add_devices(int init);          /* driver collection du jour */
#endif

extern void ui_init_cpu1cmds(void);
extern void ui_init_swarmcmds(void);
extern int ui_init_corecmds(void);
extern int ui_init_soccmds(void);
extern int ui_init_testcmds(void);
extern int ui_init_cpldcmds(void);
extern int ui_init_tempsensorcmds(void);
extern int ui_init_toyclockcmds(void);
extern int ui_init_memtestcmds(void);
extern int ui_init_resetcmds(void);
extern int ui_init_ethertestcmds(void);
extern int ui_init_flashtestcmds(void);
extern int ui_init_phycmds(void);
extern int ui_init_spdcmds(void);

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
#define BOOTROM			(16*1024*1024)
/*  *********************************************************************
    *  SysConfig switch settings and related parameters
    ********************************************************************* */

int lausanne_board_rev;
int lausanne_config_switch;

#define SWARM_PROMICE_CONSOLE	0x00000001

const unsigned int lausanne_startflags[16] = {
    0,						/* 0 : UART console, no PCI */
    SWARM_PROMICE_CONSOLE,			/* 1 : PromICE console, no PCI */
    CFE_INIT_PCI,				/* 2 : UART console, PCI */
    CFE_INIT_PCI | SWARM_PROMICE_CONSOLE,	/* 3 : PromICE console, PCI */
    0,						/* 4 : unused  */
    0,						/* 5 : unused */
    CFE_INIT_PCI | CFE_LDT_SLAVE,		/* 6 : 2, plus LDT slave mode */
    CFE_INIT_SAFE,				/* 7 : UART console, no pci, safe mode */
    0,						/* 8 : unused */
    0,						/* 9 : unused */
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
    cfe_add_device(&sb1250_uart,A_DUART,0,0);
    cfe_add_device(&promice_uart,PROMICE_BASE,PROMICE_WORDSIZE,0);

    /*
     * Read the config switch and decide how we are going to set up
     * the console.  This is actually board revision dependent.
     *
     * Note that the human-readable board revision is (lausanne_board_rev+1).
     */
    lausanne_board_rev = G_SYS_CONFIG(syscfg) & 0x3;
    lausanne_config_switch = (G_SYS_CONFIG(syscfg) >> 2) & 0x0f;

    cfe_startflags = lausanne_startflags[lausanne_config_switch];

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

    /* 
     * NVRAM (environment variables)
     */
    cfe_add_device(&sb1250_24lc128eeprom,BIGEEPROM_SMBUS_CHAN,BIGEEPROM_SMBUS_DEV,0);
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
     * Print out the board version number.
     */
    printf("%s board revision %d\n", CFG_BOARDNAME, lausanne_board_rev + 1);

    /*
     * UART channel B
     */

    cfe_add_device(&sb1250_uart,A_DUART,1,0);

    /*  
     * Boot ROM 
     */
    cfe_add_device(&lausanne_flashdrv,BOOTROM_PHYS, REAL_BOOTROM_SIZE ,NULL);
    cfe_add_device(&lausanne_flashdrv,ALT_BOOTROM_PHYS, BOOTROM ,NULL);
#ifdef _FUNCSIM_
    cfe_add_device(&lausanne_flashdrv,0x11000000,64*1024*1024,NULL);
#endif

    /* 
     * MACs - must init after environment, since the hw address is stored there 
     */
    cfe_add_device(&sb1250_ether,A_MAC_BASE_0,0,env_getenv("ETH0_HWADDR"));
    cfe_add_device(&sb1250_ether,A_MAC_BASE_1,1,env_getenv("ETH1_HWADDR"));

#if CFG_PCI
    pci_add_devices(cfe_startflags & CFE_INIT_PCI);
#endif

     /*
      *  Clock
      */
     /*No clock on this board, SDA0 and SCL0 pull high*/
     

    /*
     * Set variable that contains CPU speed, spit out config register
     */

    sb1250_show_cpu_type();

    syscfg = SBREADCSR(A_SCD_SYSTEM_CFG);
    printf("Config switch: %d\n", lausanne_config_switch);
    if (G_SYS_PLL_DIV(syscfg) == 0) {
	printf("PLL_DIV of zero found, assuming 6 (300MHz)\n");
	}

    /*
     * Reset the MAC address for MAC 2, since it's hooked
     * to the video codec.  This prevents the OS from
     * probing it.
     */
    SBWRITECSR(A_MAC_REGISTER(2,R_MAC_ETHERNET_ADDR),0);
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
    /*
     * Reset the MAC address for MAC 2, since it's hooked
     * to the video codec.  This prevents the OS from
     * probing it.
     */
    SBWRITECSR(A_MAC_REGISTER(2,R_MAC_ETHERNET_ADDR),0);
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
    ui_init_cpldcmds();
    ui_init_tempsensorcmds();
    ui_init_memtestcmds();
    ui_init_resetcmds();
    ui_init_ethertestcmds();
    ui_init_flashtestcmds();
    ui_init_phycmds();
    ui_init_spdcmds();
}
