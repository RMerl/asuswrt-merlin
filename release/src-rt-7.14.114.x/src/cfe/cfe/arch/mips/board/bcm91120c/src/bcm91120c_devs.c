/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Board device initialization		File: bcm91120c_devs.c
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
#include "sb1250_pci.h"
#include "sb1250_ldt.h"
#include "sb1250_scd.h"
#include "sb1250_smbus.h"

#include "bsp_config.h"

#include "bcm91120c.h"

#include "dev_newflash.h"

/*  *********************************************************************
    *  Devices we're importing
    ********************************************************************* */

extern cfe_driver_t promice_uart;		/* promice serial port */
extern cfe_driver_t sb1250_uart;		/* SB1250 serial ports */
extern cfe_driver_t sb1250_ether;		/* SB1250 MACs */
extern cfe_driver_t sb1250_x1240eeprom;		/* Xicor SMBus NVRAM */
extern cfe_driver_t sb1250_24lc128eeprom;	/* Microchip EEPROM */

extern cfe_driver_t newflashdrv;		/* AMD-style flash */

extern cfe_driver_t x1241_clock;		/* Xicor SMBus RTC */
extern cfe_driver_t m41t81_clock;		/* M41T81 SMBus RTC */
extern int ui_init_bcm91120ccmds(void);
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
    /*  0 */ UART_CONSOLE,
    /*  1 */ PROMICE_CONSOLE,
    /*  2 */ 0,
    /*  3 */ 0,
    /*  4 */ 0,
    /*  5 */ 0,
    /*  6 */ 0,
    /*  7 */ 0,
    /*  8 */ 0,
    /*  9 */ 0,
    /* 10 */ 0,
    /* 11 */ 0,
    /* 12 */ 0,
    /* 13 */ 0,
    /* 14 */ PROMICE_CONSOLE | CFE_INIT_SAFE,
    /* 15 */ UART_CONSOLE | CFE_INIT_SAFE,
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
#if defined(_BCM91120C_DIAG_CFG_)
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
     * Many boards set up the environment here.  We don't need
     * to for the CRhine because the only thing that needs the env
     * between here and board_device_init is the PCI config code.
     */

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
    uint64_t syscfg;
    newflash_probe_t fprobe;

    /*
     * Print out the board version number.
     */
    printf("%s board revision ", CFG_BOARDNAME);
    switch (board_rev) {
    case 0:
	printf("A (with 512MB DRAM)");
	break;
    case 1:
	printf("A (with 256MB DRAM)");
	break;
    default:
	printf("- UNKNOWN (%d)", board_rev);
	break;
    }
    printf("\n");

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
     * EEPROMs, NVRAMs (environment variables), and TOY clock
     * Probe for the X1241 - if not present, assume we have a STMicro clock.
     */

    if (board_probe_x1241(X1240_SMBUS_CHAN,X1240_SMBUS_DEV)) {
	cfe_add_device(&sb1250_x1240eeprom,X1240_SMBUS_CHAN,X1240_SMBUS_DEV,0);
	cfe_add_device(&sb1250_24lc128eeprom,BIGEEPROM0_SMBUS_CHAN,BIGEEPROM0_SMBUS_DEV,0);
	cfe_add_device(&sb1250_24lc128eeprom,BIGEEPROM1_SMBUS_CHAN,BIGEEPROM1_SMBUS_DEV,0);
	cfe_set_envdevice("eeprom0");	/* Connect NVRAM subsystem to X1240 */
	cfe_add_device(&x1241_clock,X1240_SMBUS_CHAN,X1240_SMBUS_DEV,0);
	}
    else {
	cfe_add_device(&sb1250_24lc128eeprom,BIGEEPROM0_SMBUS_CHAN,BIGEEPROM0_SMBUS_DEV,0);
	cfe_add_device(&sb1250_24lc128eeprom,BIGEEPROM1_SMBUS_CHAN,BIGEEPROM1_SMBUS_DEV,0);
	cfe_set_envdevice("eeprom1");	/* Connect NVRAM to 2nd 24lc128 */
	cfe_add_device(&m41t81_clock,M41T81_SMBUS_CHAN,M41T81_SMBUS_DEV,0);
	}


    /* 
     * MACs - must init after environment, since the hw address is stored there 
     */
    cfe_add_device(&sb1250_ether,A_MAC_BASE_0,0,env_getenv("ETH0_HWADDR"));
    cfe_add_device(&sb1250_ether,A_MAC_BASE_1,1,env_getenv("ETH1_HWADDR"));

    /*
     * Set variable that contains CPU speed, spit out config register
     */

    syscfg = SBREADCSR(A_SCD_SYSTEM_CFG);
    printf("SysCfg: %016llX [PLL_DIV: %d, IOB0_DIV: %s, IOB1_DIV: %s]\n",
	   syscfg,
	   (int)G_SYS_PLL_DIV(syscfg),
	   (syscfg & M_SYS_IOB0_DIV) ? "CPUCLK/3" : "CPUCLK/4",
	   (syscfg & M_SYS_IOB1_DIV) ? "CPUCLK/2" : "CPUCLK/3");
    printf("Config switch: %d\n", config_switch);
    if (G_SYS_PLL_DIV(syscfg) == 0) {
	printf("PLL_DIV of zero found, assuming 6 (300MHz)\n");
	}

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
    ui_init_bcm91120ccmds();
    ui_init_corecmds();
    ui_init_soccmds();
    ui_init_testcmds();
    ui_init_toyclockcmds();
    ui_init_tempsensorcmds();
    ui_init_memtestcmds();
    ui_init_resetcmds();
}
