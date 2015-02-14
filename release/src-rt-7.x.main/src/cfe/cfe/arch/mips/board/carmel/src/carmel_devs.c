/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Board device initialization		File: carmel_devs.c
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
#include "sb1250_pci.h"
#include "sb1250_ldt.h"
#include "sb1250_scd.h"
#include "sb1250_smbus.h"
#include "sb1250_draminit.h"

#include "bsp_config.h"

#include "carmel.h"
#include "carmel_env.h"

#include "dev_ide.h"
#include "dev_newflash.h"

#include "cfe_loader.h"
#include "cfe_autoboot.h"

#include "ui_command.h"


static int program_spd_eeprom(void);

/*  *********************************************************************
    *  Devices we're importing
    ********************************************************************* */

extern cfe_driver_t sb1250_uart;		/* SB1250 serial ports */
extern cfe_driver_t ns16550_uart;		/* NS16550 UART */
extern cfe_driver_t sb1250_ether;		/* SB1250 MACs */
extern cfe_driver_t newflashdrv;		/* flash */
extern cfe_driver_t idedrv;			/* IDE disk */

extern cfe_driver_t sb1250_24lc128eeprom;	/* Microchip EEPROM */
extern cfe_driver_t sb1250_at24c02eeprom;	/* Atmel SPD EEPROM */

/*  *********************************************************************
    *  Commands we're importing
    ********************************************************************* */

extern int ui_init_carmelcmds(void);
extern int ui_init_corecmds(void);
extern int ui_init_soccmds(void);
extern int ui_init_testcmds(void);
extern int ui_init_resetcmds(void);
extern int ui_init_phycmds(void);
extern int ui_init_memtestcmds(void);
extern int ui_init_ethertestcmds(void);
extern int ui_init_flashtestcmds(void);
extern int ui_init_disktestcmds(void);
extern int ui_init_spdcmds(void);
extern int ui_init_uarttestcmds(void);
extern int ui_init_xpgmcmds(void);

/*  To use the 7-seg display  */
extern void board_setdisplay(unsigned int val);

/*  *********************************************************************
    *  Some other stuff we use
    ********************************************************************* */

extern void sb1250_show_cpu_type(void);
int carmel_startup(void);

/*  *********************************************************************
    *  Some board-specific parameters
    ********************************************************************* */


/*  *********************************************************************
    *  SysConfig switch settings and related parameters
    ********************************************************************* */

int board_rev;
int config_switch;

#define UART_CONSOLE		0x00000000
#define IDE_SUPPORT		0x00000001
#define IDE_BOOT		0x00000020
#define AUTOSTART		0x00000002
#define NO_AUTOSTART		0x00000004
#define PROGRAM_SPD		0x00000008
#define NO_READ_IDEEPROM	0x00000010
#define AUTOBOOT		0x00000040

const unsigned int board_startflags[16] = {
    UART_CONSOLE | AUTOSTART,				/* 0 : UART console */
    UART_CONSOLE | AUTOSTART | IDE_SUPPORT,		/* 1 : IDE support */						
    UART_CONSOLE | AUTOSTART | IDE_SUPPORT | IDE_BOOT,  /* 2 : IDE support */						
    UART_CONSOLE | AUTOSTART | AUTOBOOT,		/* 3 : AUTOBOOT */
    0,							/* 4 : unused */
    0,							/* 5 : unused */
    0,							/* 6 : unused */
    0,							/* 7 : unused */
    0,							/* 8 : unused */
    0,                             			/* 9 : unused */
    0,							/* 10 : unused */
    0,							/* 11 : unused */
    0,							/* 12 : unused */
    0,							/* 13 : unused */
    PROGRAM_SPD | UART_CONSOLE,				/* 14 : UART, program SPD EEPROM */
    NO_AUTOSTART | NO_READ_IDEEPROM | CFE_INIT_SAFE,	/* 15 : Safe mode */
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
    cfe_add_device(&sb1250_uart,A_DUART,0,0);

    /*
     * Read the config switch and decide how we are going to set up
     * the console.  This is actually board revision dependent.
     */
    board_rev = G_SYS_CONFIG(syscfg) & 0x3;
    config_switch = (G_SYS_CONFIG(syscfg) >> 2) & 0x0F;

    cfe_startflags = board_startflags[config_switch];
   
    cfe_set_console("uart0");
     
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

    cfe_add_device(&sb1250_24lc128eeprom,ENVEEPROM_SMBUS_CHAN,ENVEEPROM_SMBUS_DEV,0);
    cfe_set_envdevice("eeprom0");	/* Connect NVRAM to 24lc128 */

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
    *  board_probe_ideeprom(chan,dev)
    *  
    *  Probe for an ID EEPROM at the specified device and channel.
    *  
    *  Actually, we just probe for anything at this address, and
    *  assume it's an ID EEPROM
    *  
    *  Input parameters: 
    *  	   chan - SMBus channel
    *  	   dev - device ID
    *  	   
    *  Return value:
    *  	   TRUE - device is present
    *  	   FALSE - not present
    ********************************************************************* */

static int board_probe_ideeprom(int chan,int slaveaddr)
{
    uintptr_t reg;
    int devaddr = 0;
    int err;

    /*
     * Initialize the SMBus channel
     */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_FREQ));

    SBWRITECSR(reg,K_SMB_FREQ_100KHZ);

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

    /*
     * Read the data byte
     */

    SBWRITECSR(reg,V_SMB_TT(K_SMB_TT_RD1BYTE) | slaveaddr);

    err = board_smbus_waitready(chan);
    if (err < 0) return err;

    /*
     * Probably don't need to read the data, but we'll do it anyway.
     */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_DATA));
    err = SBREADCSR(reg);

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
    newflash_probe_t fprobe;

    /*
     * Print out the board version number.
     */
    printf("%s board revision %d\n", CFG_BOARDNAME,board_rev + 1);

    /* 
     * Boot ROM (16MB), using "new" flash driver.  Partition the flash.
     *
     * Partitions are as follows:
     *
     *	1MB   - "boot"   CFE
     *  1MB   - "fpga"   FPGA images
     *  14MB  - "os"     Linux boot area
     */

    memset(&fprobe,0,sizeof(fprobe));
    fprobe.flash_phys = BOOTROM_PHYS;
    fprobe.flash_size = BOOTROM_SIZE*K64;
    fprobe.flash_flags =  FLASH_FLG_BUS8 | FLASH_FLG_DEV16;
    fprobe.flash_nparts = 3;
    fprobe.flash_parts[0].fp_size = 1*1024*1024;
    fprobe.flash_parts[0].fp_name = "boot";
    fprobe.flash_parts[1].fp_size = 1*1024*1024;
    fprobe.flash_parts[1].fp_name = "fpga";
    fprobe.flash_parts[2].fp_size = 0;
    fprobe.flash_parts[2].fp_name = "os";

    cfe_add_device(&newflashdrv,0,0,&fprobe);

    /*
     * SPD EEPROM on the Carmel.  This is used to store the type of memory
     * soldered onto the board. [eeprom1]
     */

    cfe_add_device(&sb1250_at24c02eeprom,SPDEEPROM_SMBUS_CHAN,SPDEEPROM_SMBUS_DEV,0);

    /*
     * EEPROM on the monterey. This is used to identify the host that
     * the Carmel is plugged into. [eeprom2]
     */

    if (board_probe_ideeprom(IDEEPROM_SMBUS_CHAN_ALT,IDEEPROM_SMBUS_DEV)) {
	printf("ID EEPROM found at SMBus channel %d device 0x%02x\n",
	       IDEEPROM_SMBUS_CHAN_ALT,IDEEPROM_SMBUS_DEV);
	cfe_add_device(&sb1250_24lc128eeprom,IDEEPROM_SMBUS_CHAN_ALT,IDEEPROM_SMBUS_DEV,0);
	carmel_envdev = "eeprom2";
	}
    else if (board_probe_ideeprom(IDEEPROM_SMBUS_CHAN,IDEEPROM_SMBUS_DEV)) {
	printf("ID EEPROM found at SMBus channel %d device 0x%02x\n",
	       IDEEPROM_SMBUS_CHAN,IDEEPROM_SMBUS_DEV);
	cfe_add_device(&sb1250_24lc128eeprom,IDEEPROM_SMBUS_CHAN,IDEEPROM_SMBUS_DEV,0);
	carmel_envdev = "eeprom2";
	}
    else {
	printf("ID EEPROM not found.\n");
	cfe_startflags |= NO_READ_IDEEPROM;
	}

    if (!(cfe_startflags & NO_READ_IDEEPROM)) {
	carmel_loadenv();
	}

    /* 
     * MACs - must init after environment, since the hw address is stored there 
     */
    cfe_add_device(&sb1250_ether,A_MAC_BASE_0,0,env_getenv("ETH0_HWADDR"));

    /*
     * Quad UART
     */

    cfe_add_device(&ns16550_uart,UART_PHYS+0,0,NULL);	/* Quad UART channel 0 */
    cfe_add_device(&ns16550_uart,UART_PHYS+8,0,NULL);	/* Quad UART channel 1 */
    cfe_add_device(&ns16550_uart,UART_PHYS+16,0,NULL);	/* Quad UART channel 2 */
    cfe_add_device(&ns16550_uart,UART_PHYS+24,0,NULL);	/* Quad UART channel 3 */

    /*
     * Set variable that contains CPU speed, spit out config register
     */

    printf("Config switch: %d\n", config_switch);

    /* 
     * IDE disks
     */

    if (board_startflags[config_switch] & IDE_SUPPORT) {
	cfe_add_device(&idedrv,IDE_PHYS+(0x1F0<<5),
		       IDE_PROBE_MASTER_TYPE(IDE_DEVTYPE_DISK) |
		       IDE_PROBE_SLAVE_TYPE(IDE_DEVTYPE_NOPROBE),
		       NULL);
	}


    sb1250_show_cpu_type();

    /*
     * Reset the MAC address for MAC 1, since it's not hooked
     * to anything.
     */
    SBWRITECSR(A_MAC_REGISTER(1,R_MAC_ETHERNET_ADDR),0);
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
     * Reset the MAC address for MAC 1, since it's not hooked
     * to anything.
     */
    SBWRITECSR(A_MAC_REGISTER(1,R_MAC_ETHERNET_ADDR),0);
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
     * 128MB (STAKTEK: 256MB) on MC 1 (JEDEC SDRAM) 
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

#if !defined(_CARMEL_STAKTEK_)
    spd[JEDEC_SPD_SIDES] = 0x01;
#else
    spd[JEDEC_SPD_SIDES] = 0x02;
#endif

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

    fh = cfe_open("eeprom1");
    if (fh <= 0) {
	xprintf("Could not open device: %s\n",cfe_errortext(fh));
	xprintf("SPD EEPROM IS NOT PROGRAMMED\n");
	return fh;
	}

    res = cfe_writeblk(fh,0,spd,JEDEC_SPD_MAX);
    if (res != JEDEC_SPD_MAX) {
	xprintf("Could not write to device: %s\n",cfe_errortext(fh));
	xprintf("SPD EEPROM IS NOT PROGRAMMED\n");
	cfe_close(fh);
	return fh;
	}

    xprintf("SPD EEPROM programmed at SMBus chan: %d addr: 0x%x\n\n",SPDEEPROM_SMBUS_CHAN,
	    SPDEEPROM_SMBUS_DEV);

    cfe_close(fh);

    return 0;
}	

/*  *********************************************************************
    *  set_display_standalone(arg)
    *  set_display_connected(arg)
    *  
    *  Set 'C F E' on the 7-seg display.
    *  Set rotating pattern.
    *  
    *  Input parameters: 
    *  	   arg - not used
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static int64_t timer;
static int blinky_state = 0;

#define CFE_BLINKIE_TIMER (CFE_HZ/2)
static uint8_t cfe_blinkies[4] = {0x8C,0x1C,0x0C,0xFF};

#define BLINKIE_TIMER (CFE_HZ/4)
static uint8_t blinkies[3] = {
    ~( (1<<1) | (1<<4)),
    ~( (1<<2) | (1<<5)),
    ~( (1<<3) | (1<<6))};


#define SETDISPLAY(x) board_setdisplay(x)
static void set_display_standalone(void *arg)
{

    if (TIMER_EXPIRED(timer)) {
	SETDISPLAY(cfe_blinkies[blinky_state]);
	TIMER_SET(timer,CFE_BLINKIE_TIMER);
	blinky_state++;
	if (blinky_state >= 4) blinky_state = 0;
	}

}

static void set_display_connected(void *arg)
{

    if (TIMER_EXPIRED(timer)) {
	SETDISPLAY(blinkies[blinky_state]);
	TIMER_SET(timer,BLINKIE_TIMER);
	blinky_state++;
	if (blinky_state >= 3) blinky_state = 0;
	}

}

/*  *********************************************************************
    *  board_setup_autoboot()
    *  
    *  Set up autoboot methods.  This routine sets up the list of 
    *  places to find a boot program.
    *  
    *  Input parameters: 
    *  	   nothing
    *  
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void board_setup_autoboot(void)
{
    /*
     * Uncomment to try IDE.  We leave this disabled for now due
     * to the long timeout if you don't have an IDE disk connected.
     */

    if (board_startflags[config_switch] & IDE_BOOT) {
	cfe_add_autoboot(CFE_AUTOBOOT_DISK,0,"ide0.0","raw","raw",NULL);
	}

    /*
     * If you had partitioned your flash, you could boot from it like this:
     */

    /* cfe_add_autoboot(CFE_AUTOBOOT_RAW,0,"flash0.os","elf","raw",NULL); */

    /*
     * Now try running a script (file containing CFE commands) from
     * the TFTP server.   Your DHCP server must set option 130
     * to contain the name of the script.  Option 130 gets stored
     * in "BOOT_SCRIPT" when a DHCP reply is received.
     */

    cfe_add_autoboot(CFE_AUTOBOOT_NETWORK,LOADFLG_BATCH,
		     "eth0","raw","tftp","$BOOT_SERVER:$BOOT_SCRIPT");

    /*
     * Finally, try loading whatever the DHCP server says is the boot
     * program.  Do this as an ELF file, and failing that, try a
     * raw binary.
     */

    cfe_add_autoboot(CFE_AUTOBOOT_NETWORK,0,"eth0","elf","tftp",NULL);
    cfe_add_autoboot(CFE_AUTOBOOT_NETWORK,0,"eth0","raw","tftp",NULL);


}

/*  *********************************************************************
    *  carmel_startup()
    *  
    *  Do stuff we do when Carmels start up
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0 if ok
    ********************************************************************* */
int carmel_startup(void)
{
    char *fpgaload;
    int res = 0;

    fpgaload = carmel_getenv("M_FPGALOAD");

    if (fpgaload && (strcmp(fpgaload,"YES") == 0)) {
	res = ui_docommand("xload");
	if (res != 0) printf("Failed to load FPGA on main board.\n");
	else printf("FPGA loaded successfully.\n");
	}

    return res;
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
    int fh;

    ui_init_carmelcmds();
    ui_init_corecmds();
    ui_init_soccmds();
    ui_init_testcmds();
    ui_init_resetcmds();
    ui_init_memtestcmds();
    ui_init_phycmds();
    ui_init_ethertestcmds();	 
    ui_init_flashtestcmds();
    ui_init_disktestcmds();
    ui_init_spdcmds();
    ui_init_uarttestcmds();
    ui_init_xpgmcmds();

    /*
     * If Carmel is connected, display rotating pattern.  If standalone, display CFE
     */
    fh = cfe_open("eeprom2");
    if (fh <= 0) {
	cfe_bg_add(set_display_standalone,NULL);
	}
    else {
	cfe_bg_add(set_display_connected,NULL);
	cfe_close(fh);
	}	
    TIMER_SET(timer,CFE_HZ);

    /*
     * Program SPD EEPROM
     */
    if (cfe_startflags & PROGRAM_SPD) {
	program_spd_eeprom();
	}

    /*
     * Do autoboot stuff
     */

    board_setup_autoboot();

    /*
     * Load the Xilinx
     */

    if (!(cfe_startflags & NO_AUTOSTART)) {
	carmel_startup();

	if(cfe_startflags & AUTOBOOT) {
	    cfe_autoboot(0,0);
	    }
	}

}
