/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Main Module				File: usbmain.c
    *  
    *  Main module that invokes the top of the USB stack from CFE.
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


#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_string.h"
#include "lib_printf.h"
#include "lib_queue.h"
#include "lib_physio.h"

#include "cfe_timer.h"
#include "ui_command.h"

#if CFG_PCI
#include "pcireg.h"
#include "pcivar.h"
#endif

#include "usbchap9.h"
#include "usbd.h"

#include "bsp_config.h"


/*  *********************************************************************
    *  Externs
    ********************************************************************* */

extern usb_hcdrv_t ohci_driver;			/* OHCI Driver dispatch */

extern int ohcidebug;				/* OHCI debug control */
extern int usb_noisy;				/* USBD debug control */

int usb_init(void);				/* forward */
int ui_init_usbcmds(void);			/* forward */

/*  *********************************************************************
    *  Globals
    ********************************************************************* */

/*
 * We keep track of the pointers to USB buses in globals.
 * One entry in this array per USB bus (the Opti controller
 * on the SWARM has two functions, so it's two buses)
 */

#define USB_MAX_BUS 4
int usb_buscnt = 0;
usbbus_t *usb_buses[USB_MAX_BUS];


/*  *********************************************************************
    *  usb_cfe_timer(arg)
    *  
    *  This routine is called periodically by CFE's timer routines
    *  to give the USB subsystem some time.  Basically we scan
    *  for work to do to manage configuration updates, and handle
    *  interrupts from the USB controllers.
    *  
    *  Input parameters: 
    *  	   arg - value we passed when the timer was initialized
    *  	          (not used)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void usb_cfe_timer(void *arg)
{
    int idx;
    static int in_poll = 0;

    /*
     * We sometimes call the timer routines in here, which calls
     * the polling loop.  This code is not reentrant, so
     * prevent us from running the interrupt routine or
     * bus daemon while we are already in there.
     */

    if (in_poll) return;

    /*
     * Do not allow nested "interrupts."
     */

    in_poll = 1;

    for (idx = 0; idx < usb_buscnt; idx++) {
	if (usb_buses[idx]) {
	    usb_poll(usb_buses[idx]);
	    usb_daemon(usb_buses[idx]);
	    }
	}

    /*
     * Okay to call polling again.
     */

    in_poll = 0;
}


/*  *********************************************************************
    *  usb_init_one_ohci(addr)
    *  
    *  Initialize one USB controller.
    *  
    *  Input parameters: 
    *  	   addr - physical address of OHCI registers
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error
    ********************************************************************* */
static int usb_init_one_ohci(uint32_t addr)
{
    usbbus_t *bus;
    int res;

    bus = UBCREATE(&ohci_driver, addr);

    if (bus == NULL) {
	printf("USB: Could not create OHCI driver structure for controller at 0x%08X\n",addr);
	return -1;
	}

    bus->ub_num = usb_buscnt;

    res = UBSTART(bus);

    if (res != 0) {
	printf("USB: Could not init OHCI controller at 0x%08X\n",addr);
	UBSTOP(bus);
	return -1;
	}
    else {
	usb_buses[usb_buscnt++] = bus;
	usb_initroot(bus);
	}

    return 0;
}

#if CFG_PCI
/*  *********************************************************************
    *  usb_init_pci_ohci()
    *  
    *  Initialize all PCI-based OHCI controllers
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error
    ********************************************************************* */
static int usb_init_pci_ohci(void)
{
    int res;
    pcitag_t tag;
    uint32_t pciclass;
    physaddr_t bar;
    int idx;

    idx = 0;

    while (pci_find_class(PCI_CLASS_SERIALBUS,idx,&tag) == 0) {
	pciclass = pci_conf_read(tag,PCI_CLASS_REG);
	if ((PCI_SUBCLASS(pciclass) == PCI_SUBCLASS_SERIALBUS_USB) && 
	    (PCI_INTERFACE(pciclass) == 0x10)) {
	    bar = (physaddr_t) pci_conf_read(tag,PCI_MAPREG_START);
	    pci_tagprintf(tag,"OHCI USB controller found at %08X\n",(uint32_t) bar);

	    /* On the BCM1250, this sets the address to "match bits" mode, 
	       which eliminates the need for byte swaps of data to/from the registers. */
	    bar |= 0x20000000;

	    res = usb_init_one_ohci(bar);
	    if (res < 0) break;
	    }
	idx++;
	}

    return 0;
}
#endif



/*  *********************************************************************
    *  usb_init()
    *  
    *  Initialize the USB subsystem
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

int usb_init(void)
{
    static int initdone = 0;

    if (initdone) {
	printf("USB has already been initialized.\n");
	return -1;
	}

    printf("Initializing USB.\n");

    initdone = 1;

    usb_buscnt = 0;

#if CFG_PCI
    usb_init_pci_ohci();
#endif

#if CFG_USB_OHCI_BASE
    usb_init_one_ohci(CFG_USB_OHCI_BASE);
#endif
	    
    cfe_bg_add(usb_cfe_timer,NULL);

    return 0;
}



static int ui_cmd_usb_start(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int res = 0;

    if (cmd_sw_isset(cmd,"-o")) ohcidebug++;
    if (cmd_sw_isset(cmd,"-oo")) ohcidebug+=2;
    if (cmd_sw_isset(cmd,"-u")) usb_noisy++;
    if (cmd_sw_isset(cmd,"-uu")) usb_noisy+=2;

    if (usb_buscnt == 0) {
	res = usb_init();
	}
    
    return res;
}


static int ui_cmd_usb_show(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int busnum;
    int devnum;
    char *x;
    int idx;
    uint32_t arg;

    x = cmd_getarg(cmd,1);
    if (!x) devnum = 0;
    else devnum = atoi(x);

    x = cmd_getarg(cmd,0);
    if (!x) x = "*";
    busnum = atoi(x);

    if (busnum >= usb_buscnt) {
	printf("Invalid bus number,  %d USB Buses currently configured.\n",usb_buscnt);
	return -1;
	}

    arg = cmd_sw_isset(cmd,"-v") ? 0x100 : 0;
    arg |= (devnum & 0xFF);

    if (x[0] == '*') {
	for (idx = 0; idx < usb_buscnt; idx++) {
	    usbhub_dumpbus(usb_buses[idx],arg);
	    }
	}
    else {
	usbhub_dumpbus(usb_buses[busnum],arg);
	}
    
    return 0;

}

/*  *********************************************************************
    *  ui_init_usbcmds(void)
    *  
    *  Initialize the USB commands
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

int ui_init_usbcmds(void)
{
    cmd_addcmd("usb init",
	       ui_cmd_usb_start,
	       NULL,
	       "Initialize the USB controller.",
	       "usb init",
	       "-o;OHCI debug messages|"
	       "-oo;more OHCI debug messages|"
	       "-u;USBD debug messages|"
	       "-uu;more USBD debug messages");


    cmd_addcmd("show usb",
	       ui_cmd_usb_show,
	       NULL,
	       "Display devices connected to USB bus.",
	       "usb show [bus [device]]\n\n"
	       "Displays the configuration descriptors for devices connected to the USB\n"
	       "If you specify a bus, the entire bus is displayed.  If you specify the\n"
	       "device number as well, only the specified device is displayed\n",
	       "-v;Display descriptors from the devices");

    return 0;
}
