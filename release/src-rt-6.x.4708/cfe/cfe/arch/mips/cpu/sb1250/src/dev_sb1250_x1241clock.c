/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Xicor RTC  driver		File: dev_sb1250_x1241clock.c
    *  
    *  This module contains a CFE driver for a Xicor X1241 SMBus
    *  real-time-clock.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com) and Binh Vo
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
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "cfe_timer.h"

#include "sb1250_defs.h"
#include "sb1250_regs.h"
#include "sb1250_smbus.h"


/*  *********************************************************************
    *  Constants
    ********************************************************************* */

/*
 * Register bits
 */

#define X1241REG_SR_BAT	0x80		/* currently on battery power */
#define X1241REG_SR_RWEL 0x04		/* r/w latch is enabled, can write RTC */
#define X1241REG_SR_WEL 0x02		/* r/w latch is unlocked, can enable r/w now */
#define X1241REG_SR_RTCF 0x01		/* clock failed */
#define X1241REG_BL_BP2 0x80		/* block protect 2 */
#define X1241REG_BL_BP1 0x40		/* block protect 1 */
#define X1241REG_BL_BP0 0x20		/* block protect 0 */
#define X1241REG_BL_WD1	0x10
#define X1241REG_BL_WD0	0x08
#define X1241REG_HR_MIL 0x80		/* military time format */

/*
 * Register numbers
 */

#define X1241REG_BL	0x10		/* block protect bits */
#define X1241REG_INT	0x11		/*  */
#define X1241REG_SC	0x30		/* Seconds */
#define X1241REG_MN	0x31		/* Minutes */
#define X1241REG_HR	0x32		/* Hours */
#define X1241REG_DT	0x33		/* Day of month */
#define X1241REG_MO	0x34		/* Month */
#define X1241REG_YR	0x35		/* Year */
#define X1241REG_DW	0x36		/* Day of Week */
#define X1241REG_Y2K	0x37		/* Year 2K */
#define X1241REG_SR	0x3F		/* Status register */

#define X1241_CCR_ADDRESS	0x6F
#define X1241_ARRAY_ADDRESS	0x57

#define X1241_EEPROM_SIZE 2048

#define BCD(x) (((x) % 10) + (((x) / 10) << 4))
#define SET_TIME	0x00
#define SET_DATE	0x01

/*  *********************************************************************
    *  Forward declarations
    ********************************************************************* */

static void x1241_clock_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr);

static int x1241_clock_open(cfe_devctx_t *ctx);
static int x1241_clock_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int x1241_clock_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int x1241_clock_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int x1241_clock_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int x1241_clock_close(cfe_devctx_t *ctx);


/*  *********************************************************************
    *  Device dispatch
    ********************************************************************* */

const static cfe_devdisp_t x1241_clock_dispatch = {
  x1241_clock_open,
  x1241_clock_read,
  x1241_clock_inpstat,
  x1241_clock_write,
  x1241_clock_ioctl,
  x1241_clock_close,
  NULL,
  NULL
};

const cfe_driver_t x1241_clock = {
  "Xicor X1241 RTC",
  "clock",
  CFE_DEV_CLOCK,
  &x1241_clock_dispatch,
  x1241_clock_probe
};
  

/*  *********************************************************************
    *  Structures
    ********************************************************************* */
typedef struct x1241_clock_s {
  int smbus_channel;
} x1241_clock_t;
  
/*  *********************************************************************
    *  time_smbus_init(chan)
    *  
    *  Initialize the specified SMBus channel for the temp sensor
    *  
    *  Input parameters: 
    *  	   chan - channel # (0 or 1)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void time_smbus_init(int chan)
{
    uintptr_t reg;

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_FREQ));

    SBWRITECSR(reg,K_SMB_FREQ_100KHZ);			/* 100KHz clock */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_CONTROL));

    SBWRITECSR(reg,0);			/* not in direct mode, no interrupts, will poll */
    
}

/*  *********************************************************************
    *  time_waitready(chan)
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
static int time_waitready(int chan)
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
    *  time_readrtc(chan,slaveaddr,devaddr)
    *  
    *  Read a byte from the chip.  The 'slaveaddr' parameter determines
    *  whether we're reading from the RTC section or the EEPROM section.
    *  
    *  Input parameters: 
    *  	   chan - SMBus channel
    *  	   slaveaddr -  SMBus slave address
    *  	   devaddr - byte with in the X1240 device to read
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else -1
    ********************************************************************* */

static int time_readrtc(int chan,int slaveaddr,int devaddr)
{
    uintptr_t reg;
    int err;

    /*
     * Make sure the bus is idle (probably should
     * ignore error here)
     */

    if (time_waitready(chan) < 0) return -1;

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

    err = time_waitready(chan);
    if (err < 0) return err;

    /*
     * Read the data byte
     */

    SBWRITECSR(reg,V_SMB_TT(K_SMB_TT_RD1BYTE) | slaveaddr);

    err = time_waitready(chan);
    if (err < 0) return err;

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_DATA));
    err = SBREADCSR(reg);

    return (err & 0xFF);
}

/*  *********************************************************************
    *  time_writertc(chan,slaveaddr,devaddr,b)
    *  
    *  write a byte from the chip.  The 'slaveaddr' parameter determines
    *  whethe we're writing to the RTC section or the EEPROM section.
    *  
    *  Input parameters: 
    *  	   chan - SMBus channel
    *  	   slaveaddr -  SMBus slave address
    *  	   devaddr - byte with in the X1240 device to read
    *      b - byte to write
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else -1
    ********************************************************************* */

static int time_writertc(int chan,int slaveaddr,int devaddr,int b)
{
    uintptr_t reg;
    int err;
    int64_t timer;

    /*
     * Make sure the bus is idle (probably should
     * ignore error here)
     */

    if (time_waitready(chan) < 0) return -1;

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
    SBWRITECSR(reg,((devaddr & 0xFF) | ((b & 0xFF) << 8)));

    /*
     * Start the command.  Keep pounding on the device until it
     * submits or the timer expires, whichever comes first.  The
     * datasheet says writes can take up to 10ms, so we'll give it 500.
     */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_START));
    SBWRITECSR(reg,V_SMB_TT(K_SMB_TT_WR3BYTE) | slaveaddr);

    /*
     * Wait till the SMBus interface is done
     */	

    err = time_waitready(chan);
    if (err < 0) return err;

    /*
     * Pound on the device with a current address read
     * to poll for the write complete
     */

    TIMER_SET(timer,50);
    err = -1; 

    while (!TIMER_EXPIRED(timer)) {
	POLL();

	SBWRITECSR(reg,V_SMB_TT(K_SMB_TT_RD1BYTE) | slaveaddr);

	err = time_waitready(chan);
	if (err == 0) break;
	}

    return err;
}


/*  *********************************************************************
    *  x1241_clock_probe(drv,a,b,ptr)
    *  
    *  Probe routine for this driver.  This routine creates the 
    *  local device context and attaches it to the driver list
    *  within CFE.
    *  
    *  Input parameters: 
    *  	   drv - driver handle
    *  	   a,b - probe hints (longs)
    *  	   ptr - probe hint (pointer)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void x1241_clock_probe(cfe_driver_t *drv,
				     unsigned long probe_a, unsigned long probe_b, 
				     void *probe_ptr)
{
    x1241_clock_t *softc;
    char descr[80];

    softc = (x1241_clock_t *) KMALLOC(sizeof(x1241_clock_t),0);

    /*
     * Probe_a is the SMBus channel number
     * Probe_b is unused.
     * Probe_ptr is unused.
     */

    softc->smbus_channel = (int)probe_a;

    xsprintf(descr,"%s on SMBus channel %d",
	     drv->drv_description,probe_a);
    cfe_attach(drv,softc,NULL,descr);
}



/*  *********************************************************************
    *  x1241_clock_open(ctx)
    *  
    *  Open this device.  For the X1241, we do a quick test 
    *  read to be sure the device is out there.
    *  
    *  Input parameters: 
    *  	   ctx - device context (can obtain our softc here)
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int x1241_clock_open(cfe_devctx_t *ctx)
{
    x1241_clock_t *softc = ctx->dev_softc;
    int b;
    int64_t timer;
    int chan = softc->smbus_channel;

    /*
     * Try to read byte 0 from the device.  If it does not
     * respond, fail the open.  We may need to do this for
     * up to 300ms in case the X1241 is busy wiggling its
     * RESET line.
     */

    time_smbus_init(chan);

    TIMER_SET(timer,300);
    while (!TIMER_EXPIRED(timer)) {
	POLL();
	b = time_readrtc(chan,X1241_CCR_ADDRESS,0);
	if (b >= 0) break;		/* read is ok */
	}

    /*
     * See if the watchdog is enabled.  If it is, turn it off.
     */

    b = time_readrtc(chan,X1241_CCR_ADDRESS,X1241REG_BL);

    if (b != (X1241REG_BL_WD1 | X1241REG_BL_WD0)) {

	time_writertc(chan,
		      X1241_CCR_ADDRESS,
		      X1241REG_SR,
		      X1241REG_SR_WEL);

	time_writertc(chan,
		      X1241_CCR_ADDRESS,
		      X1241REG_SR,
		      X1241REG_SR_WEL | X1241REG_SR_RWEL);

	time_writertc(chan,
		      X1241_CCR_ADDRESS,
		      X1241REG_BL,
		      (X1241REG_BL_WD1 | X1241REG_BL_WD0));

	time_writertc(chan,
		      X1241_CCR_ADDRESS,
		      X1241REG_SR,
		      0);
	}

    return (b < 0) ? -1 : 0;
}

/*  *********************************************************************
    *  x1241_clock_read(ctx,buffer)
    *  
    *  Read time/date from the RTC. Read a total of 8 bytes in this format:
    *  hour-minute-second-month-day-year1-year2
    *  
    *  Input parameters: 
    *  	   ctx - device context (can obtain our softc here)
    *  	   buffer - buffer descriptor (target buffer, length, offset)
    *  	   
    *  Return value:
    *  	   number of bytes read
    *  	   -1 if an error occured
    ********************************************************************* */

static int x1241_clock_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{

    x1241_clock_t *softc = ctx->dev_softc;
    uint8_t byte;
    unsigned char *bptr;    
    int chan = softc->smbus_channel;

    bptr = buffer->buf_ptr;

    byte = (uint8_t)  time_readrtc(chan,X1241_CCR_ADDRESS,X1241REG_HR);
    byte &= ~X1241REG_HR_MIL;
    *bptr++ = (unsigned char) byte;
    
    byte = (uint8_t) time_readrtc(chan,X1241_CCR_ADDRESS,X1241REG_MN);
    *bptr++ = (unsigned char) byte;
    
    byte = (uint8_t) time_readrtc(chan,X1241_CCR_ADDRESS,X1241REG_SC);
    *bptr++ = (unsigned char) byte;

    byte =  (uint8_t) time_readrtc(chan,X1241_CCR_ADDRESS,X1241REG_MO);
    *bptr++ = (unsigned char) byte;

    byte = (uint8_t) time_readrtc(chan,X1241_CCR_ADDRESS,X1241REG_DT);
    *bptr++ = (unsigned char) byte;

    byte =  (uint8_t) time_readrtc(chan,X1241_CCR_ADDRESS,X1241REG_YR);
    *bptr++ = (unsigned char) byte;

    byte = (uint8_t) time_readrtc(chan,X1241_CCR_ADDRESS,X1241REG_Y2K);
    *bptr = (unsigned char) byte;

    byte = (uint8_t) time_readrtc(chan,X1241_CCR_ADDRESS,X1241REG_SR);
  
    /*Last byte is status of battery*/
    if (byte & X1241REG_SR_RTCF) 
    {
      printf("Warning: Battery has failed.  Clock setting is not accurate.\n");
    }
   
    buffer->buf_retlen = 8;
    return 0;
}

/*  *********************************************************************
    *  x1241_clock_write(ctx,buffer)
    *  
    *  Write time/date to the RTC. Write in this format:
    *  hour-minute-second-month-day-year1-year2-(time/date flag)
    *  
    *  Input parameters: 
    *  	   ctx - device context (can obtain our softc here)
    *  	   buffer - buffer descriptor (target buffer, length, offset)
    *  	   
    *  Return value:
    *  	   number of bytes written
    *  	   -1 if an error occured
    ********************************************************************* */

static int x1241_clock_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    x1241_clock_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    uint8_t hr,min,sec;
    uint8_t mo,day,yr,y2k;
    uint8_t timeDateFlag;
    int chan = softc->smbus_channel;    

    bptr = buffer->buf_ptr;
    timeDateFlag = *(bptr + 7);
 
    /* unlock writes to the CCR */
    time_writertc(chan,X1241_CCR_ADDRESS,X1241REG_SR,X1241REG_SR_WEL);
    time_writertc(chan,X1241_CCR_ADDRESS,X1241REG_SR,
		  X1241REG_SR_WEL | X1241REG_SR_RWEL);

    /* write time or date */
    if(timeDateFlag == SET_TIME) {

	hr = (uint8_t) *bptr;
	time_writertc(chan,X1241_CCR_ADDRESS,X1241REG_HR, BCD(hr) | X1241REG_HR_MIL);

	min = (uint8_t) *(bptr+1);
	time_writertc(chan,X1241_CCR_ADDRESS,X1241REG_MN, BCD(min) );

	sec = (uint8_t) *(bptr+2);
	time_writertc(chan,X1241_CCR_ADDRESS,X1241REG_SC, BCD(sec) );

	buffer->buf_retlen = 3;
	} 
    else if(timeDateFlag == SET_DATE) {

	mo = (uint8_t) *(bptr+3);
	time_writertc(chan,X1241_CCR_ADDRESS,X1241REG_MO, BCD(mo) );

	day = (uint8_t) *(bptr+4);
	time_writertc(chan,X1241_CCR_ADDRESS,X1241REG_DT, BCD(day) );

	yr = (uint8_t) *(bptr+5);
	time_writertc(chan,X1241_CCR_ADDRESS,X1241REG_YR, BCD(yr) );

	y2k = (uint8_t) *(bptr+6);
	time_writertc(chan,X1241_CCR_ADDRESS,X1241REG_Y2K, BCD(y2k) );

	buffer->buf_retlen = 4;
	}
    else {
	return -1;
	}
	
    /* lock writes again */
    time_writertc(chan,X1241_CCR_ADDRESS,X1241REG_SR,0);

    return 0;
}

/*  *********************************************************************
    *  x1241_clock_inpstat(ctx,inpstat)
    *  
    *  Test input (read) status for the device
    *  
    *  Input parameters: 
    *  	   ctx - device context (can obtain our softc here)
    *  	   inpstat - input status descriptor to receive value
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   -1 if an error occured
    ********************************************************************* */

static int x1241_clock_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
    inpstat->inp_status = 1;

    return 0;
}

/*  *********************************************************************
    *  x1241_clock_ioctl(ctx,buffer)
    *  
    *  Perform miscellaneous I/O control operations on the device.
    *  
    *  Input parameters: 
    *  	   ctx - device context (can obtain our softc here)
    *  	   buffer - buffer descriptor (target buffer, length, offset)
    *  	   
    *  Return value:
    *  	   number of bytes read
    *  	   -1 if an error occured
    ********************************************************************* */

static int x1241_clock_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
  return 0;
}

/*  *********************************************************************
    *  x1241_clock_close(ctx,buffer)
    *  
    *  Close the device.
    *  
    *  Input parameters: 
    *  	   ctx - device context (can obtain our softc here)
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   -1 if an error occured
    ********************************************************************* */

static int x1241_clock_close(cfe_devctx_t *ctx)
{
    return 0;
}
