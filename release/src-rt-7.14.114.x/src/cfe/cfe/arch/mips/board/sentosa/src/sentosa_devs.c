/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Board device initialization		File: sentosa_devs.c
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
#include "sb1250_scd.h"
#include "sb1250_smbus.h"

#include "bsp_config.h"

#include "sentosa.h"

#include "dev_newflash.h"

/*  *********************************************************************
    *  Devices we're importing
    ********************************************************************* */

extern cfe_driver_t promice_uart;		/* promice serial port */
extern cfe_driver_t sb1250_uart;		/* SB1250 serial ports */
extern cfe_driver_t sb1250_ether;		/* SB1250 MACs */
extern cfe_driver_t sb1250_x1240eeprom;		/* Xicor SMBus NVRAM */
extern cfe_driver_t newflashdrv;		/* AMD-style flash */
extern cfe_driver_t sb1250_24lc128eeprom;	/* Microchip EEPROM */
extern cfe_driver_t x1241_clock;		/* Xicor SMBus RTC */
extern cfe_driver_t m41t81_clock;		/* M41T81 SMBus RTC */

#if CFG_PCI
extern void pci_add_devices(int init);          /* driver collection du jour */
#endif
extern cfe_driver_t sb1250_pcihost;             /* driver for host downloads */

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

int sentosa_board_rev;
int sentosa_config_switch;

#define SWARM_PROMICE_CONSOLE	0x00000001
#define DEVICE_DOWNLOAD         0x00000002
#define DEVICE_EXECUTE          0x00000004
#define DEVICE_REBOOT           (DEVICE_DOWNLOAD|DEVICE_EXECUTE)

const unsigned int sentosa_startflags[16] = {
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
     *
     * Note that the human-readable board revision is (sentosa_board_rev+1).
     */
    sentosa_board_rev = G_SYS_CONFIG(syscfg) & 0x3;
    sentosa_config_switch = (G_SYS_CONFIG(syscfg) >> 2) & 0x0F;

    cfe_startflags = sentosa_startflags[sentosa_config_switch];

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

    switch (sentosa_board_rev) {
	case SENTOSA_REV_1:
	    /*
	     * Rev1 Sentosas always have Xicor X1241 clock chips
	     * and do not have two 24LC128 EEPROMs.  So, we use
	     * the X1241 for the environment settings.
	     */
	    cfe_add_device(&sb1250_x1240eeprom,X1240_SMBUS_CHAN,X1240_SMBUS_DEV,0);
	    cfe_add_device(&x1241_clock,X1240_SMBUS_CHAN,X1240_SMBUS_DEV,0);
	    break;
	default:
	case SENTOSA_REV_2:
	case SENTOSA_REV_3:
	    /*
	     * Some early rev2's have X1240s but most newer ones will have
	     * ST Micro M41T81 clock chips.  We'll need to probe for
	     * the X1241 to know if we've got one.
	     *
	     * All Rev2's have two 24LC128s so we'll use that for the 
	     * environment variables.
	     */

	    /* 
	     * Add the NVRAM device on SMBus 1 first.  This will be our environment
	     */
	     
	    cfe_add_device(&sb1250_24lc128eeprom,BIGEEPROM1_SMBUS_CHAN,BIGEEPROM1_SMBUS_DEV,0);

	}

    cfe_set_envdevice("eeprom0");	/* Connect NVRAM subsystem to EEPROM */
}


/*  *********************************************************************
    *  board_smbus_waitready(chan)
    *  
    *  Wait until the SMBus channel is ready.  We simply poll
    *  the busy bit until it clears.
    *  
    *  Input parameters: 
    *  	   chan - channel (0 or 1)
    *
    *  Return value:
    *      nothing
    ********************************************************************* */
static int board_smbus_waitready(int chan)
{
    uintptr_t reg;
    uint64_t status;

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_STATUS));

    for (;;) {
	status = SBREADCSR(reg);
	if (status & M_SMB_BUSY) continue;
	break;
	}

    if (status & M_SMB_ERROR) {
	
	SBWRITECSR(reg,(status & M_SMB_ERROR));
	return -1;
	}
    return 0;
}

/*  *********************************************************************
    *  board_probe_x1241(chan,dev)
    *  
    *  Probe for a Xicor X1241 at the specified device and channel.
    *  
    *  Actually, we just probe for anything at this address, and
    *  assume it's a Xicor.
    *  
    *  Input parameters: 
    *  	   chan - SMBus channel
    *  	   dev - device ID
    *  	   
    *  Return value:
    *  	   TRUE - X1241 is present
    *  	   FALSE - not present
    ********************************************************************* */

static int board_probe_x1241(int chan,int slaveaddr)
{
    uintptr_t reg;
    int devaddr = 0;
    int err;

    /*
     * Initialize the SMBus channel
     */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_FREQ));

    SBWRITECSR(reg,K_SMB_FREQ_400KHZ);

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_CONTROL));

    SBWRITECSR(reg,0);			/* not in direct mode, no interrupts, will poll */

    /*
     * Make sure the bus is idle (probably should
     * ignore error here)
     */

    if (board_smbus_waitready(chan) < 0) return FALSE;

    /*
     * Write the device address to the controller. There are two
     * parts, the high part goes in the "CMD" field, and the 
     * low part is the data field.
     */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_CMD));
    SBWRITECSR(reg,((devaddr >> 8) & 0x7));

    /*
     * Write the data to the controller
     */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_DATA));
    SBWRITECSR(reg,((devaddr & 0xFF) & 0xFF));

    /*
     * Start the command
     */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_START));
    SBWRITECSR(reg,V_SMB_TT(K_SMB_TT_WR2BYTE) | slaveaddr);

    /*
     * Wait till done
     */

    err = board_smbus_waitready(chan);
    if (err < 0) return FALSE;
    return TRUE;
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
    printf("%s board revision %d\n", CFG_BOARDNAME, sentosa_board_rev + 1);

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

    /* 
     * MACs - must init after environment, since the hw address is stored there 
     */
    cfe_add_device(&sb1250_ether,A_MAC_BASE_0,0,env_getenv("ETH0_HWADDR"));
    cfe_add_device(&sb1250_ether,A_MAC_BASE_1,1,env_getenv("ETH1_HWADDR"));

#if CFG_PCI
    pci_add_devices(cfe_startflags & CFE_INIT_PCI);
#endif

    /*
     * Real-time clock 
     */

    switch (sentosa_board_rev) {
	case SENTOSA_REV_1:
	    /*
	     * Rev1 Sentosas always have Xicor X1241 clock chips.
	     */
	    cfe_add_device(&x1241_clock,X1240_SMBUS_CHAN,X1240_SMBUS_DEV,0);
	    break;
	default:
	case SENTOSA_REV_2:
	case SENTOSA_REV_3:
	    /*
	     * Some early rev2's have X1240s but most newer ones will have
	     * ST Micro M41T81 clock chips.  We'll need to probe for
	     * the X1241 to know if we've got one.
	     */

	    if (board_probe_x1241(X1240_SMBUS_CHAN,X1240_SMBUS_DEV)) {
		cfe_add_device(&x1241_clock,X1240_SMBUS_CHAN,X1240_SMBUS_DEV,0);
		/* Might as well configure the X1240's EEPROM while we're here. */
		cfe_add_device(&sb1250_x1240eeprom,X1240_SMBUS_CHAN,X1240_SMBUS_DEV,0);
		}	
	    else {
		/* Add ST Micro clock driver here */
		cfe_add_device(&m41t81_clock,M41T81_SMBUS_CHAN,M41T81_SMBUS_DEV,0);
		}

	}

    /*
     * Host download interface.
     */
    cfe_add_device(&sb1250_pcihost,0,0,NULL);


    /*
     * Set variable that contains CPU speed, spit out config register
     */

    printf("Config switch: %d\n", sentosa_config_switch);

    sb1250_show_cpu_type();

    /*
     * Reset the MAC address for MAC 2, since it's not hooked
     * to anything.
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
     * Reset the MAC address for MAC 2, since it's not hooked
     * to anything.
     */
    SBWRITECSR(A_MAC_REGISTER(2,R_MAC_ETHERNET_ADDR),0);
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

#define WRITECSR(csr,val) *((volatile uint64_t *) (csr)) = (val)
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
    ui_init_toyclockcmds();
    ui_init_memtestcmds();
    ui_init_phycmds();
    ui_init_ethertestcmds();
    ui_init_flashtestcmds();

    cfe_bg_add(board_blinkylight,NULL);
    TIMER_SET(blinky_timer,CFE_HZ);

    if (cfe_startflags & DEVICE_DOWNLOAD) {
	cfe_device_download((cfe_startflags & DEVICE_EXECUTE), "");        
	}
}
