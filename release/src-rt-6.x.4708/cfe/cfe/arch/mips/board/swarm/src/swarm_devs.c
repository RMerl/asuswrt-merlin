/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Board device initialization		File: swarm_devs.c
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
#include "sb1250_wid.h"

#include "bsp_config.h"

#include "swarm.h"
#include "dev_ide.h"

#include "dev_newflash.h"

#include "cfe_loader.h"
#include "cfe_autoboot.h"

/*  *********************************************************************
    *  Devices we're importing
    ********************************************************************* */

extern cfe_driver_t m41t81_clock;		/* M41T81 SMBus RTC */
extern cfe_driver_t x1241_clock;		/* Xicor SMBus RTC */
extern cfe_driver_t promice_uart;		/* promice serial port */
extern cfe_driver_t sb1250_uart;		/* SB1250 serial ports */
extern cfe_driver_t sb1250_ether;		/* SB1250 MACs */
extern cfe_driver_t sb1250_x1240eeprom;		/* Xicor SMBus NVRAM */
extern cfe_driver_t sb1250_24lc128eeprom;	/* Microchip EEPROM */
extern cfe_driver_t newflashdrv;		/* flash */
extern cfe_driver_t idedrv;			/* IDE disk */
extern cfe_driver_t pcmciadrv;			/* PCMCIA card */
#if CFG_PCI
extern void pci_add_devices(int init);          /* driver collection du jour */
#endif
#if CFG_TCP
extern cfe_driver_t tcpconsole;			/* TCP/IP console */
#endif
#if CFG_VGACONSOLE
extern cfe_driver_t pcconsole2;			/* VGA Console */
static int vgainit_ok = FALSE;
extern int vga_biosinit(void);
#endif
#if CFG_USB
extern int usb_init(void);
extern cfe_driver_t usb_disk;			/* USB mass-storage */
extern cfe_driver_t usb_uart;			/* USB serial ports */
#endif

/*  *********************************************************************
    *  UI command initialization
    ********************************************************************* */

extern void ui_init_cpu1cmds(void);
extern void ui_init_swarmcmds(void);
extern int ui_init_corecmds(void);
extern int ui_init_soccmds(void);
extern int ui_init_testcmds(void);
extern int ui_init_tempsensorcmds(void);
extern int ui_init_toyclockcmds(void);
extern int ui_init_memtestcmds(void);
extern int ui_init_resetcmds(void);
extern int ui_init_phycmds(void);
extern int ui_init_spdcmds(void);
extern int ui_init_disktestcmds(void);
extern int ui_init_ethertestcmds(void);
extern int ui_init_flashtestcmds(void);

#if CFG_USB
extern int ui_init_usbcmds(void);
#endif

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

#define REAL_BOOTROM_SIZE	(2*1024*1024)	/* region is 4MB, but rom is 2MB */

/*  *********************************************************************
    *  SysConfig switch settings and related parameters
    ********************************************************************* */

int swarm_board_rev;
int swarm_config_switch;

#define SWARM_PROMICE_CONSOLE	0x00000001
#define SWARM_VGA_CONSOLE	0x00000002

const unsigned int swarm_startflags[16] = {
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
    CFE_INIT_PCI | SWARM_VGA_CONSOLE,		/* 10 : PCI, VGA Console */
    0,						/* 11 : unused */
    0,						/* 12 : unused */
    0,						/* 13 : unused */
    0,						/* 14 : unused */
    0						/* 15 : unused */
};


unsigned int cpu_revision;


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

#if CFG_VGACONSOLE
    /* VGA console, for the truly adventurous! */
    cfe_add_device(&pcconsole2,0,0,0);
#endif

    /*
     * Read the config switch and decide how we are going to set up
     * the console.  This is actually board revision dependent.
     *
     * Note that the human-readable board revision is (swarm_board_rev+1).
     */
    swarm_board_rev = G_SYS_CONFIG(syscfg) & 0x3;
    switch (swarm_board_rev) {
    case 0:
	/*
	 * Board revision 0.  The config switch sets bits 3..5 of the
	 * SYS_CONFIG field of the syscfg register.
	 */
        swarm_config_switch = (G_SYS_CONFIG(syscfg) >> 3) & 0x07;
	break;

    default:
	/*
	 * In later board revisions, the config switch sets bits 2..5.
	 */
        swarm_config_switch = (G_SYS_CONFIG(syscfg) >> 2) & 0x0f;
	break;
    }

    cfe_startflags = swarm_startflags[swarm_config_switch];

    /*
     * Choose which console we're going to use.  Our three choices
     * are the PromICE AI2 port, the "VGA" console, and
     * the UART.
     *
     * The UART is the "normal" console, of course.
     */

    if (cfe_startflags & SWARM_PROMICE_CONSOLE) {
	cfe_set_console("promice0");
	}
#if CFG_VGACONSOLE 
    /*
     * If VGA console, "buffer" the console messages until the
     * PCI is ready.
     */
    else if (cfe_startflags & SWARM_VGA_CONSOLE) {
	cfe_set_console(CFE_BUFFER_CONSOLE);
	}
#endif
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
     * NVRAM (environment variables - These depend on the SWARM
     * board version since SWARM rev3 (and later) do not have Xicor
     * clock chips.  Instead, a second 24lc128 is on SMBus 1.
     */

    switch (swarm_board_rev) {
	case SWARM_REV_1:
	case SWARM_REV_2:
	    cfe_add_device(&sb1250_x1240eeprom,X1240_SMBUS_CHAN,X1240_SMBUS_DEV,0);
	    break;
	default:
	case SWARM_REV_3:
	    cfe_add_device(&sb1250_24lc128eeprom,BIGEEPROM_SMBUS_CHAN_1,BIGEEPROM_SMBUS_DEV_1,0);
	    
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
    printf("%s board revision %d\n", CFG_BOARDNAME, swarm_board_rev + 1);

    /*
     * UART channel B
     */

    cfe_add_device(&sb1250_uart,A_DUART,1,0);

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

#ifdef _FUNCSIM_
    /*
     * As a hack, we instantiate another flash at 0x1100_0000 (the PCMCIA area)
     * when running in the functional simulator.  We can preload an image into
     * that region for faster booting.  (see build/broadcom/sim/runcfe)
     */
    cfe_add_device(&newflashdrv,0x11000000,64*1024*1024,NULL);
#endif

    /*
     * This is the 24LC128 on SMBus0.  CFE doesn't use it for anything,
     * but you can load data into it and then boot from it by changing a jumper.
     */

    cfe_add_device(&sb1250_24lc128eeprom,BIGEEPROM_SMBUS_CHAN,BIGEEPROM_SMBUS_DEV,0);

    /* 
     * MACs - must init after environment, since the hw address is stored there 
     */

    cfe_add_device(&sb1250_ether,A_MAC_BASE_0,0,env_getenv("ETH0_HWADDR"));
    cfe_add_device(&sb1250_ether,A_MAC_BASE_1,1,env_getenv("ETH1_HWADDR"));

    /* 
     * IDE disks
     */

    cfe_add_device(&idedrv,IDE_PHYS+(0x1F0<<5),
     		   IDE_PROBE_MASTER_TYPE(IDE_DEVTYPE_DISK) |
     		   IDE_PROBE_SLAVE_TYPE(IDE_DEVTYPE_NOPROBE),
     	 	   NULL);

    /*
     * PCMCIA support
     */

    cfe_add_device(&pcmciadrv,PCMCIA_PHYS,0,NULL);

#if CFG_PCI
    pci_add_devices(cfe_startflags & CFE_INIT_PCI);
#endif

     /*
      * Clock - depends on the hardware version number, since the older
      * boards have Xicor clocks and the newer boards have ST Micro parts.
      */

     switch (swarm_board_rev) {
	 case SWARM_REV_1:
	     cfe_add_device(&x1241_clock,X1240_SMBUS_CHAN,X1240_SMBUS_DEV,0);
	     break;

	 default:
	 case SWARM_REV_2:
	 case SWARM_REV_3:
	     if (board_probe_x1241(X1240_SMBUS_CHAN,X1240_SMBUS_DEV)) {
		 cfe_add_device(&x1241_clock,X1240_SMBUS_CHAN,X1240_SMBUS_DEV,0);
		 }	
	     else {
		 /* Add ST Micro clock driver here */
		 cfe_add_device(&m41t81_clock,M41T81_SMBUS_CHAN,M41T81_SMBUS_DEV,0);
		 }
	 }

#if CFG_VGACONSOLE
    /*
     * VGA Console - try to init a VGA console, fall back to
     * UART if not there.
     */

    if (cfe_startflags & SWARM_VGA_CONSOLE) {
	cfe_ledstr("vgai");
	vgainit_ok = vga_biosinit();
	if (vgainit_ok == 0) cfe_set_console("pcconsole0");
	else cfe_set_console("uart0");
	}
#endif


    /*
     * Set variable that contains CPU speed, spit out config register
     */

    printf("Config switch: %d\n", swarm_config_switch);

    sb1250_show_cpu_type();

    /*
     * Reset the MAC address for MAC 2, since it's hooked
     * to the video codec.  This prevents the OS from
     * probing it.
     */
    SBWRITECSR(A_MAC_REGISTER(2,R_MAC_ETHERNET_ADDR),0);

    /*
     * Some misc devices go here, mostly for playing.
     */

#if CFG_TCP
    cfe_add_device(&tcpconsole,0,0,0);
#endif

#if CFG_USB
    usb_init();
    cfe_add_device(&usb_disk,0,0,0);
    cfe_add_device(&usb_uart,0,0,0);
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
    /*
     * Reset the MAC address for MAC 2, since it's hooked
     * to the video codec.  This prevents the OS from
     * probing it.
     */
    SBWRITECSR(A_MAC_REGISTER(2,R_MAC_ETHERNET_ADDR),0);
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
     * First try PCMCIA.  Try to load an elf file called 'autoboot'
     * from the root directory of the FAT filesystem on the PCMCIA card.
     */

    cfe_add_autoboot(CFE_AUTOBOOT_DISK,0,"pcmcia0","elf","fat","autoboot");

    /*
     * Uncomment to try IDE next.  We leave this disabled for now due
     * to the long timeout if you don't have an IDE disk connected.
     */

    /* cfe_add_autoboot(CFE_AUTOBOOT_DISK,0,"ide0.0","raw","raw",NULL); */

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
    ui_init_toyclockcmds();
    ui_init_tempsensorcmds();
    ui_init_memtestcmds();
    ui_init_resetcmds();
    ui_init_phycmds();
    ui_init_spdcmds();
#if CFG_USB
    ui_init_usbcmds();
#endif
    ui_init_disktestcmds();
    ui_init_ethertestcmds();
    ui_init_flashtestcmds();

    board_setup_autoboot();

}
