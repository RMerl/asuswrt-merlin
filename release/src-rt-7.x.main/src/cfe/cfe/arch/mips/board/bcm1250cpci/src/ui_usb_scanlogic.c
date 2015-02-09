/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  BCM1250CPCI USB commands			    File: ui_usb_scanlogic.c
    *  
    *  USB test utilities
    *  
    *  Author:  Paul Burrell
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
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_timer.h"
#include "cfe_error.h"

#include "ui_command.h"
#include "cfe.h"

#include "bsp_config.h"

#include "sl11h.h"
#include "sbmips.h"
#include "sb1250_regs.h"
#include "sb1250_smbus.h"

#include "bcm1250cpci.h"



/*  *********************************************************************
    *  Constants
    ********************************************************************* */

/*  *********************************************************************
    *  Configuration
    ********************************************************************* */

/*  *********************************************************************
    *  prototypes
    ********************************************************************* */

int ui_init_usbcmds(void);
static int ui_cmd_usbread(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_usbwrite(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_usbreset(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_usbmemtest(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_usbfinddevice(ui_cmdline_t *cmd,int argc,char *argv[]);


/*  *********************************************************************
    *  Data
    ********************************************************************* */


/*  *********************************************************************
    *  ui_init_usbcmds()
    *  
    *  Add USB commands to the command table
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */


int ui_init_usbcmds(void)
{
    cmd_addcmd("usb read",
	       ui_cmd_usbread,
	       NULL,
	       "Read register from SL11H USB chip",
	       "usb read <reg>",
	       "");

    cmd_addcmd("usb write",
	       ui_cmd_usbwrite,
	       NULL,
	       "Write register from SL11H USB chip",
	       "usb write <reg> <data>",
	       "");

    cmd_addcmd("usb reset",
	       ui_cmd_usbreset,
	       NULL,
	       "Resets the SL11H USB chip",
	       "usb reset",
	       "");

    cmd_addcmd("usb memtest",
	       ui_cmd_usbmemtest,
	       NULL,
	       "Tests all of the onboard memory on the SL11H USB chip",
	       "usb memtest",
	       "");

    cmd_addcmd("usb finddevice",
	       ui_cmd_usbfinddevice,
	       NULL,
	       "Scans for peripherals.",
	       "usb find device",
	       "");

    return 0;
}


/*  *********************************************************************
    *  User interface commands
    ********************************************************************* */

static int ui_cmd_usbread(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *line;
    char *p;
    int address;
    
    if ((line = cmd_getarg(cmd,0)) == NULL) {
    	return ui_showusage(cmd);
	}

    /* parse and check command-line args */
    address = -1;
    
    p = gettoken(&line);
    if (p) address  = xtoi(p);
    
    if ((address < 0) || (address > 0xfF)) {
        printf("Invalid Address Range on USB read");
        return(-1);
    }
    
    printf("USB read at address %02X = %02X\n",address,SL11Read(address));
    
    return 0;
}

static int ui_cmd_usbwrite(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *line;
//    char *p;
    int address,data;

    if (argc != 2) {
	    return ui_showusage(cmd);
	}

    if ((line = cmd_getarg(cmd,0)) == NULL) {
	    return ui_showusage(cmd);
	}


    /* parse and check command-line args */
    address = -1;
    data    = -1;

	address = xtoi(argv[0]);
	data 	= xtoi(argv[1]);

//    p = gettoken(&line);
//    if (p) address  = xtoi(p);
//	printf("%s\n",p);
//    p = gettoken(&line);
//    if (p) data     = xtoi(p);
//	printf("%s\n",p);

	printf ("%X,%X\n",address,data);
    
    if ((address < 0) || (address > 0xfF)) {
        printf("Invalid Address Range on USB write\n");
        return(-1);
    }
    
    if ((data < 0) || (data > 0xfF)) {
        printf("Invalid DATA Range on USB write\n");
        return(-1);
    }
        
    SL11Write(address,data);
    
    printf("Data written\n");
    
    return(0);
    
}


static int ui_cmd_usbreset(ui_cmdline_t *cmd,int argc,char *argv[])
{

    printf("Reseting USB device\n");
    USBReset();
    printf("USB reset complete\n");
    
    return 0;

}

static int ui_cmd_usbmemtest(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int errors;

    printf("Performing memory test...\n");
    errors = SL11HMemTest();
    printf("Error Count = %d\n",errors);
    printf("Memory Test complete.\n");
    
    if (errors) {
        return(-1);
    } else {
        return(0);
        
    }

}

static int ui_cmd_usbfinddevice(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int status;

    status = uFindUsbDev(0x0);
    
    return(status);
        

}
