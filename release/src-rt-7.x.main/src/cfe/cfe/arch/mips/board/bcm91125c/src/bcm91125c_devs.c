/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Board device initialization		File: bcm91125c_devs.c
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
#include "lib_string.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_timer.h"
#include "env_subr.h"
#include "cfe.h"

#include "sb1250_defs.h"
#include "sb1250_regs.h"
#include "sb1250_scd.h"
#include "sb1250_smbus.h"

#include "bsp_config.h"

#include "bcm91125c.h"

#include "dev_newflash.h"

/*  *********************************************************************
    *  Devices we're importing
    ********************************************************************* */

extern cfe_driver_t promice_uart;		/* promice serial port */
extern cfe_driver_t sb1250_uart;		/* SB1250 serial ports */
extern cfe_driver_t sb1250_ether;		/* SB1250 MACs */
extern cfe_driver_t sb1250_24lc128eeprom;	/* Microchip EEPROM */
extern cfe_driver_t m41t81_clock;		/* M41T81 SMBus RTC */

extern cfe_driver_t newflashdrv;		/* AMD-style flash */

#if CFG_PCI
extern void pci_add_devices(int init);          /* driver collection du jour */
#endif

extern int ui_init_bcm91125ccmds(void);
extern int ui_init_corecmds(void);
extern int ui_init_soccmds(void);
extern int ui_init_testcmds(void);
extern int ui_init_tempsensorcmds(void);
extern int ui_init_toyclockcmds(void);
extern int ui_init_memtestcmds(void);
extern int ui_init_resetcmds(void);

/*  *********************************************************************
    *  Other externs
    ********************************************************************* */

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

/*  *********************************************************************
    *  SysConfig switch settings and related parameters
    ********************************************************************* */

int board_rev;
int config_switch;

#define UART_CONSOLE	0x00000000	/* Pseudo flag */
#define PROMICE_CONSOLE	0x00000001

const unsigned int board_startflags[16] = {
    UART_CONSOLE,				/* 0 : UART console, no PCI */
    PROMICE_CONSOLE,				/* 1 : PromICE console, no PCI */
    CFE_INIT_PCI | UART_CONSOLE,			/* 2 : UART console, PCI */
    CFE_INIT_PCI | PROMICE_CONSOLE,		/* 3 : PromICE console, PCI*/
    0,						/* 4: unused */		
    0,						/* 5: unused */
    CFE_INIT_PCI | UART_CONSOLE | CFE_LDT_SLAVE, /* 6: UART console, PCI, HT slave mode */ 
    0,						/* 7: unused */
    0,						/* 8: unused */
    0,						/* 9: unused */
    0,						/* 10: unused */
    0,						/* 11: unused */
    0,						/* 12: unused */
    0,						/* 13: unused */
    PROMICE_CONSOLE | CFE_INIT_SAFE,		/* 14: PromICE console, no PCI, safe mode */
    UART_CONSOLE | CFE_INIT_SAFE,		/* 15: UART console, no PCI, safe mode */	
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

    /*
     * Read the config switch and decide how we are going to set up
     * the console.  This is actually board revision dependent.
     */
    board_rev = G_SYS_CONFIG(syscfg) & 0x3;
    config_switch = (G_SYS_CONFIG(syscfg) >> 2) & 0x0f;
    cfe_startflags = board_startflags[config_switch];
#if defined(_BCM91125C_DIAG_CFG_)
    cfe_startflags = PROMICE_CONSOLE;
#endif

    /* Initialize console */
    cfe_add_device(&sb1250_uart,A_DUART,0,0);
    cfe_add_device(&promice_uart,PROMICE_BASE,PROMICE_WORDSIZE,0);

    if (cfe_startflags & PROMICE_CONSOLE) {
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
     * NVRAM
     */
    cfe_add_device(&sb1250_24lc128eeprom,BIGEEPROM0_SMBUS_CHAN,BIGEEPROM0_SMBUS_DEV,0);
    cfe_add_device(&sb1250_24lc128eeprom,BIGEEPROM1_SMBUS_CHAN,BIGEEPROM1_SMBUS_DEV,0);
    cfe_set_envdevice("eeprom1");	/* Connect NVRAM to 2nd 24lc128 */

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
    newflash_probe_t fprobe;

    /*
     * Print out the board version number.
     */
    printf("%s board revision %d\n", CFG_BOARDNAME,board_rev+1);

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

    /*
     * Big Flash - partitioned into four 4MB chunks.
     */

    memset(&fprobe,0,sizeof(fprobe));
    fprobe.flash_phys = BIG_FLASH_PHYS;
    fprobe.flash_size = BIG_FLASH_SIZE*K64;
    fprobe.flash_flags =  FLASH_FLG_BUS8 | FLASH_FLG_DEV16;
    fprobe.flash_nparts = 4;
    fprobe.flash_parts[0].fp_size = 4*1024*1024;
    fprobe.flash_parts[0].fp_name = NULL;
    fprobe.flash_parts[1].fp_size = 4*1024*1024;
    fprobe.flash_parts[1].fp_name = NULL;
    fprobe.flash_parts[2].fp_size = 4*1024*1024;
    fprobe.flash_parts[2].fp_name = NULL;
    fprobe.flash_parts[3].fp_size = 4*1024*1024;
    fprobe.flash_parts[3].fp_name = NULL;

    cfe_add_device(&newflashdrv,0,0,&fprobe);

    /*
     * TOY clock
     */
    cfe_add_device(&m41t81_clock,M41T81_SMBUS_CHAN,M41T81_SMBUS_DEV,0);
       
    /* 
     * MACs - must init after environment, since the hw address is stored there 
     */
    cfe_add_device(&sb1250_ether,A_MAC_BASE_0,0,env_getenv("ETH0_HWADDR"));
    cfe_add_device(&sb1250_ether,A_MAC_BASE_1,1,env_getenv("ETH1_HWADDR"));

#if CFG_PCI
    pci_add_devices(cfe_startflags & CFE_INIT_PCI);
#endif

    /*
     * Set variable that contains CPU speed, spit out config register
     */
    printf("Config switch: %d\n", config_switch);

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

    /* Nothing to do. */
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
    ui_init_bcm91125ccmds();
    ui_init_corecmds();
    ui_init_soccmds();
    ui_init_testcmds();
    ui_init_toyclockcmds();
    ui_init_tempsensorcmds();
    ui_init_memtestcmds();
    ui_init_resetcmds();
}
