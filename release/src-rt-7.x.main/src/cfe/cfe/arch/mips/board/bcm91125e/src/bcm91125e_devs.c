/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Board device initialization		File: bcm91125e_devs.c
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

#include "lib_malloc.h"
#include "lib_string.h"

#include "lib_printf.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_timer.h"
#include "env_subr.h"
#include "cfe.h"
#include "cfe_devfuncs.h"

#include "sb1250_defs.h"
#include "sb1250_regs.h"
#include "sb1250_scd.h"
#include "sb1250_smbus.h"
#include "sb1250_draminit.h"
#include "sb1250_mac.h"

#include "bsp_config.h"

#include "bcm91125e.h"

#include "dev_newflash.h"

static int program_spd_eeprom(void);

/*  *********************************************************************
    *  Devices we're importing
    ********************************************************************* */

extern cfe_driver_t promice_uart;		/* promice serial port */
extern cfe_driver_t sb1250_uart;		/* SB1250 serial ports */
extern cfe_driver_t sb1250_ether;		/* SB1250 MACs */

extern cfe_driver_t newflashdrv;		/* AMD-style flash */
extern cfe_driver_t flashdrv;			/* AMD-style flash */

extern cfe_driver_t sb1250_24lc128eeprom;	/* Microchip EEPROM */
extern cfe_driver_t m41t81_clock;		/* M41T81 SMBus RTC */
extern cfe_driver_t sb1250_at24c02eeprom;	/* Atmel SPD EEPROM */

#if CFG_PCI
extern void pci_add_devices(int init);          /* driver collection du jour */
#endif
extern cfe_driver_t sb1250_pcihost;             /* driver for host downloads */

/*  *********************************************************************
    *  Commands we're importing
    ********************************************************************* */

extern int ui_init_bcm91125ecmds(void);
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
extern int ui_init_disktestcmds(void);
extern int ui_init_spdcmds(void);

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

int board_rev;
int config_switch;

#define UART_CONSOLE		0x00000000
#define PROMICE_CONSOLE		0x00000001
#define DEVICE_DOWNLOAD         0x00000002
#define DEVICE_EXECUTE          0x00000004
#define DEVICE_REBOOT           (DEVICE_DOWNLOAD|DEVICE_EXECUTE)
#define PROGRAM_SPD		0x00000008

const unsigned int board_startflags[16] = {
    UART_CONSOLE,					/* 0 : UART console, no PCI */
    PROMICE_CONSOLE,					/* 1 : PromICE console, no PCI */
    CFE_INIT_PCI | UART_CONSOLE,			/* 2 : UART console, PCI */
    CFE_INIT_PCI | PROMICE_CONSOLE,			/* 3 : PromICE console, PCI */
    0,							/* 4 : unused  */
    0,							/* 5 : unused */
    CFE_INIT_PCI | CFE_LDT_SLAVE | UART_CONSOLE, 	/* 6 : UART, PCI, HT slave mode */
    CFE_INIT_SAFE | UART_CONSOLE,			/* 7 : UART console, no pci, safe mode */
    CFE_INIT_PCI | DEVICE_DOWNLOAD | UART_CONSOLE,	/* 8 : UART console, PCI, download */
    CFE_INIT_PCI | DEVICE_REBOOT | UART_CONSOLE,	/* 9 : UART console, PCI, reboot */
    0,							/* 10 : unused */
    0,							/* 11 : unused */
    0,							/* 12 : unused */
    0,							/* 13 : unused */
    PROGRAM_SPD | UART_CONSOLE,				/* 14 : UART, program SPD EEPROM */
    0,							/* 15 : unused */
};

static int64_t blinky_timer;			/* for blinky */

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
    cfe_add_device(&sb1250_uart,A_DUART,0,0);
    cfe_add_device(&promice_uart,PROMICE_BASE,PROMICE_WORDSIZE,0);

    /*
     * Read the config switch and decide how we are going to set up
     * the console.  This is actually board revision dependent.
     */
    board_rev = G_SYS_CONFIG(syscfg) & 0x3;
    config_switch = (G_SYS_CONFIG(syscfg) >> 2) & 0x0F;

    cfe_startflags = board_startflags[config_switch];

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
     * NVRAM (environment variables)
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
#define WRITECSR(csr,val) *((volatile uint64_t *) (csr)) = (val)
#define READCSR(csr) *((volatile uint64_t *) (csr))
void board_device_init(void)
{
    
    uint64_t mac_mdio_1_genc;

    /*
     * Print out the board version number.
     */
    printf("%s board revision %d\n", CFG_BOARDNAME,board_rev + 1);

    /* 
     * Boot ROM, using "new" flash driver
     */
    cfe_add_device(&newflashdrv,
		   BOOTROM_PHYS,
		   (BOOTROM_SIZE*K64) | FLASH_FLG_BUS8 | FLASH_FLG_DEV16,
		   NULL);
    cfe_add_device(&newflashdrv,
		   ALT_BOOTROM_PHYS,
		   (BOOTROM_SIZE*K64) | FLASH_FLG_BUS8 | FLASH_FLG_DEV16,
		   NULL);

    /*
     * SPD EEPROM. This is used to store the type of memory soldered 
     * onto the board. [eeprom2]
     */

    cfe_add_device(&sb1250_at24c02eeprom,SPDEEPROM_SMBUS_CHAN,SPDEEPROM_SMBUS_DEV,0);

    /* 
     * MACs - must init after environment, since the hw address is stored there.
     *
     */
    cfe_add_device(&sb1250_ether,A_MAC_BASE_0,0,env_getenv("ETH0_HWADDR"));
    cfe_add_device(&sb1250_ether,A_MAC_BASE_1,1,env_getenv("ETH1_HWADDR"));

    /*
     * MAC 1 on BCM91125E boards is mux'ed between the PHY and the mezz connector.
     * The genc bit on the mac_mdio_1 register determine the route.  High = PHY
     * Low = Mezz.  
     * 
     * Default to PHY
     */
    mac_mdio_1_genc = READCSR(PHYS_TO_K1(A_MAC_BASE_1+R_MAC_MDIO)) | M_MAC_GENC;
    WRITECSR(PHYS_TO_K1(A_MAC_BASE_1+R_MAC_MDIO),mac_mdio_1_genc);
 

#if CFG_PCI
    pci_add_devices(cfe_startflags & CFE_INIT_PCI);
#endif

    /*
     * Real-time clock 
     */
    cfe_add_device(&m41t81_clock,M41T81_SMBUS_CHAN,M41T81_SMBUS_DEV,0);

    /*
     * Host download interface.
     */
    cfe_add_device(&sb1250_pcihost,0,0,NULL);

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
    /*Nothing to do. */
}


/*  *********************************************************************
    *  board_blinkylight(arg)
    *  
    *  Blink the LED once per second
    *  
    *  Input parameters: 
    *  	   arg - not used
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void board_blinkylight(void *arg)
{
    static int light = 0;
    intptr_t reg;

    if (TIMER_EXPIRED(blinky_timer)) {
	light = !light;
	reg = light ? A_GPIO_PIN_SET : A_GPIO_PIN_CLR;
	WRITECSR(PHYS_TO_K1(reg),M_GPIO_DEBUG_LED);
	TIMER_SET(blinky_timer,CFE_HZ);
	}
}

/*  *********************************************************************
    *  program_spd_eeprom()
    *  
    *  Program the Atmel eeprom with SPD definitions
    *  
    *  Input parameters: 
    *      none
    *      
    *  Return value:
    *  	   0 - success
    *	  else - failure
    ********************************************************************* */
 
static int program_spd_eeprom(void)
{
    unsigned char spd[JEDEC_SPD_MAX];
    int fh;
    int res;

    memset(spd,0,JEDEC_SPD_MAX);

    /* 
     * 128MB on MC 1 (JEDEC SDRAM) 
     * Samsung K4H561638B - 16Mx16 chips  
     *
     * Minimum tMEMCLK: 8.0ns (125Mhz max freq) 
     *
     * CS0 Geometry: 13 rows, 9 columns, 2 bankbits
     *
     * 64khz refresh, CAS Latency 2.5
     * Timing (ns):   tCK=7.50 tRAS=45 tRP=20.00 tRRD=15.0 tRCD=20.0 tRFC=auto tRC=auto
     *
     * Clock Config: Default
     */

    spd[JEDEC_SPD_MEMTYPE] = JEDEC_MEMTYPE_DDRSDRAM2;
    spd[JEDEC_SPD_ROWS] = 0x0D;
    spd[JEDEC_SPD_COLS] = 0x09;
    spd[JEDEC_SPD_BANKS] = 0x04;
    spd[JEDEC_SPD_SIDES] = 0x01;
    spd[JEDEC_SPD_WIDTH] = 0x48;

    spd[JEDEC_SPD_tCK25] = 0x75;
    spd[JEDEC_SPD_tCK20] = 0x00;
    spd[JEDEC_SPD_tCK10] = 0x00;
    spd[JEDEC_SPD_RFSH] = 0x82;
    spd[JEDEC_SPD_CASLATENCIES] = 0x08;
    spd[JEDEC_SPD_ATTRIBUTES] = 0x00;
    spd[JEDEC_SPD_tRAS] = 0x2D;
    spd[JEDEC_SPD_tRP] = 0x50;
    spd[JEDEC_SPD_tRRD] = 0x3C;
    spd[JEDEC_SPD_tRCD] = 0x50;
    spd[JEDEC_SPD_tRFC] = 0x00;
    spd[JEDEC_SPD_tRC] = 0x00;

    fh = cfe_open("eeprom2");
    if (fh <= 0) {
	xprintf("Could not open device: %s\n",cfe_errortext(fh));
	xprintf("SPD EEPROM IS NOT PROGRAMMED\n");
	return fh;
	}

    res = cfe_writeblk(fh,0,spd,JEDEC_SPD_MAX);
    if (res != JEDEC_SPD_MAX) {
	xprintf("Could not write to device: %s\n",cfe_errortext(fh));
	xprintf("SPD EEPROM IS NOT PROGRAMMED\n");
	return fh;
	}

    xprintf("SPD EEPROM programmed at SMBus chan: %d addr: 0x%x\n\n",SPDEEPROM_SMBUS_CHAN,
	    SPDEEPROM_SMBUS_DEV);

    cfe_close(fh);

    return 0;
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
    ui_init_bcm91125ecmds();
    ui_init_corecmds();
    ui_init_soccmds();
    ui_init_testcmds();
    ui_init_resetcmds();
    ui_init_tempsensorcmds();
    ui_init_toyclockcmds();
    ui_init_memtestcmds();
    ui_init_phycmds();
    ui_init_ethertestcmds();	 
    ui_init_flashtestcmds();
    ui_init_disktestcmds();
    ui_init_spdcmds();

    cfe_bg_add(board_blinkylight,NULL);
    TIMER_SET(blinky_timer,CFE_HZ);

    if (cfe_startflags & DEVICE_DOWNLOAD) {
	cfe_device_download((cfe_startflags & DEVICE_EXECUTE), "");        
	}

    /*
     * Program SPD EEPROM
     */
    if (cfe_startflags & PROGRAM_SPD) {
	program_spd_eeprom();
	}
}
