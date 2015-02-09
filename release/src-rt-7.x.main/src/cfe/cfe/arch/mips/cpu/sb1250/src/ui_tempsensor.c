/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Temperature sensor commands:		File: ui_tempsensor.c
    *  
    *  Temperature sensor commands
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
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_console.h"
#include "cfe_timer.h"

#include "cfe_error.h"

#include "ui_command.h"
#include "cfe.h"

#include "bsp_config.h"

#include "sbmips.h"
#include "sb1250_regs.h"
#include "sb1250_smbus.h"


/*  *********************************************************************
    *  Configuration
    ********************************************************************* */

#define _MAX6654_	/* Support Maxim 6654 temperature chip w/parasitic mode */

/*  *********************************************************************
    *  prototypes
    ********************************************************************* */

int ui_init_tempsensorcmds(void);

#if (defined(TEMPSENSOR_SMBUS_DEV) && defined(TEMPSENSOR_SMBUS_CHAN))
static int ui_cmd_showtemp(ui_cmdline_t *cmd,int argc,char *argv[]);
static void temp_timer_proc(void *);
#endif

/*  *********************************************************************
    *  Data
    ********************************************************************* */

#if (defined(TEMPSENSOR_SMBUS_DEV) && defined(TEMPSENSOR_SMBUS_CHAN))
static int64_t temp_timer = 0;
static int temp_prev_local = 0;
static int temp_prev_remote = 0;
#endif

/*  *********************************************************************
    *  ui_init_swarmcmds()
    *  
    *  Add SWARM-specific commands to the command table
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */


int ui_init_tempsensorcmds(void)
{

#if (defined(TEMPSENSOR_SMBUS_DEV) && defined(TEMPSENSOR_SMBUS_CHAN))
    cmd_addcmd("show temp",
	       ui_cmd_showtemp,
	       NULL,
	       "Display CPU temperature",
	       "show temp",
	       "-continuous;Poll for temperature changes|"
	       "-stop;Stop polling for temperature changes");

    cfe_bg_add(temp_timer_proc,NULL);
#endif

    return 0;
}



#if (defined(TEMPSENSOR_SMBUS_DEV) && defined(TEMPSENSOR_SMBUS_CHAN))
/*  *********************************************************************
    *  temp_smbus_init(chan)
    *  
    *  Initialize the specified SMBus channel for the temp sensor
    *  
    *  Input parameters: 
    *  	   chan - channel # (0 or 1)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void temp_smbus_init(int chan)
{
    uintptr_t reg;

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_FREQ));

    SBWRITECSR(reg,K_SMB_FREQ_100KHZ);			/* 400Khz clock */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_CONTROL));

    SBWRITECSR(reg,0);			/* not in direct mode, no interrupts, will poll */
    
}

/*  *********************************************************************
    *  temp_smbus_waitready(chan)
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
static int temp_smbus_waitready(int chan)
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
    *  temp_smbus_read(chan,slaveaddr,devaddr)
    *  
    *  Read a byte from the temperature sensor chip
    *  
    *  Input parameters: 
    *  	   chan - SMBus channel
    *  	   slaveaddr -  SMBus slave address
    *  	   devaddr - byte with in the sensor device to read
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else -1
    ********************************************************************* */

static int temp_smbus_read(int chan,int slaveaddr,int devaddr)
{
    uintptr_t reg;
    int err;

    /*
     * Make sure the bus is idle (probably should
     * ignore error here)
     */

    if (temp_smbus_waitready(chan) < 0) return -1;

    /*
     * Write the device address to the controller. There are two
     * parts, the high part goes in the "CMD" field, and the 
     * low part is the data field.
     */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_CMD));
    SBWRITECSR(reg,devaddr);

    /*
     * Read the data byte
     */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_START));
    SBWRITECSR(reg,V_SMB_TT(K_SMB_TT_CMD_RD1BYTE) | slaveaddr);

    err = temp_smbus_waitready(chan);
    if (err < 0) return err;

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_DATA));
    err = SBREADCSR(reg);

    return (err & 0xFF);
}

#ifdef _MAX6654_
/*  *********************************************************************
    *  temp_smbus_write(chan,slaveaddr,devaddr,data)
    *  
    *  write a byte to the temperature sensor chip
    *  
    *  Input parameters: 
    *  	   chan - SMBus channel
    *  	   slaveaddr -  SMBus slave address
    *  	   devaddr - byte with in the sensor device to read
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else -1
    ********************************************************************* */

static int temp_smbus_write(int chan,int slaveaddr,int devaddr,int data)
{
    uintptr_t reg;
    int err;

    /*
     * Make sure the bus is idle (probably should
     * ignore error here)
     */

    if (temp_smbus_waitready(chan) < 0) return -1;

    /*
     * Write the device address to the controller. There are two
     * parts, the high part goes in the "CMD" field, and the 
     * low part is the data field.
     */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_CMD));
    SBWRITECSR(reg,devaddr);

    /*
     * Write the data byte
     */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_DATA));
    SBWRITECSR(reg,data);

    /*
     * Do the write command.
     */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_START));
    SBWRITECSR(reg,V_SMB_TT(K_SMB_TT_WR2BYTE) | slaveaddr);

    err = temp_smbus_waitready(chan);

    return err;
}
#endif


/*  *********************************************************************
    *  temp_showtemp(noisy)
    *  
    *  Display the temperature.  If 'noisy' is true, display it 
    *  regardless of whether it has changed, otherwise only display
    *  when it has changed.
    *  
    *  Input parameters: 
    *  	   noisy - display whether or not changed
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static int temp_showtemp(int noisy)
{
    int local,remote,status;
    char statstr[50];

    local  = temp_smbus_read(TEMPSENSOR_SMBUS_CHAN,TEMPSENSOR_SMBUS_DEV,0);
    remote = temp_smbus_read(TEMPSENSOR_SMBUS_CHAN,TEMPSENSOR_SMBUS_DEV,1);
    status = temp_smbus_read(TEMPSENSOR_SMBUS_CHAN,TEMPSENSOR_SMBUS_DEV,2);

    if ((local < 0) || (remote < 0) || (status < 0)) {
	if (noisy) printf("Temperature sensor device did not respond\n");
	return -1;
	}

    if (noisy || (local != temp_prev_local) || (remote != temp_prev_remote)) {
	statstr[0] = 0;
	if (status & 0x80) strcat(statstr,"Busy ");
	if (status & 0x40) strcat(statstr,"HiTempLcl ");
	if (status & 0x20) strcat(statstr,"LoTempLcl ");
	if (status & 0x10) strcat(statstr,"HiTempRem ");
	if (status & 0x08) strcat(statstr,"LoTempRem ");
	if (status & 0x04) strcat(statstr,"Fault ");

	if (noisy || !(status & 0x80)) {	
	    /* don't display if busy, always display if noisy */
	    console_log("Temperature:  CPU: %dC  Board: %dC  Status:%02X [ %s]",
		    remote,local,status,statstr);
	    }
	}

    temp_prev_local = local;
    temp_prev_remote = remote;

    return 0;
}




/*  *********************************************************************
    *  ui_cmd_showtemp(cmd,argc,argv)
    *  
    *  Show temperature 
    *  
    *  Input parameters: 
    *  	   cmd - command structure
    *  	   argc,argv - parameters
    *  	   
    *  Return value:
    *  	   -1 if error occured.  Does not return otherwise
    ********************************************************************* */

static int ui_cmd_showtemp(ui_cmdline_t *cmd,int argc,char *argv[])
{

    temp_smbus_init(TEMPSENSOR_SMBUS_CHAN);

#ifdef _MAX6654_
    do {
	int dev,rev;
	static int didinit = 0;

	if (!didinit) {
	    didinit = 1;
	    dev = temp_smbus_read(TEMPSENSOR_SMBUS_CHAN,TEMPSENSOR_SMBUS_DEV,0xFE);
	    rev = temp_smbus_read(TEMPSENSOR_SMBUS_CHAN,TEMPSENSOR_SMBUS_DEV,0xFF);
	    printf("Temperature Sensor Device ID %02X rev %02X\n",dev,rev);

	    if (dev == 0x4D) {		/* MAX6654 */
		printf("Switching MAX6654 to parasitic mode\n");
		/* Switch to 1hz conversion rate (1 seconds per conversion) */
		temp_smbus_write(TEMPSENSOR_SMBUS_CHAN,TEMPSENSOR_SMBUS_DEV,0x0A,0x04);
		/* Switch to parasitic mode */
		temp_smbus_write(TEMPSENSOR_SMBUS_CHAN,TEMPSENSOR_SMBUS_DEV,9,0x10);
		}
	    }
       } while (0);
#endif    

    if (temp_showtemp(1) < 0) {
	TIMER_CLEAR(temp_timer);
	return -1;
	}

    if (cmd_sw_isset(cmd,"-continuous")) {
	TIMER_SET(temp_timer,2*CFE_HZ);
	}
    if (cmd_sw_isset(cmd,"-stop")) {
	TIMER_CLEAR(temp_timer);
	}

    return 0;
}

/*  *********************************************************************
    *  temp_timer_proc()
    *  
    *  So we can be fancy and log temperature changes as they happen.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void temp_timer_proc(void *arg)
{
    if (!TIMER_RUNNING(temp_timer)) return;

    if (TIMER_EXPIRED(temp_timer)) {
	temp_showtemp(0);
	TIMER_SET(temp_timer,2*CFE_HZ);
	}
}
#endif
