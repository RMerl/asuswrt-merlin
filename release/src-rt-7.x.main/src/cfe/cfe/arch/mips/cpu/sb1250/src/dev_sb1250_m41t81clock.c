/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  M41T81 RTC  driver		File: dev_sb1250_m41t81clock.c
    *  
    *  This module contains a CFE driver for a M41T81 SMBus
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

#define M41T81REG_SC_ST		0x80		/* stop bit */
#define M41T81REG_HR_CB	        0x40		/* century bit */
#define M41T81REG_HR_CEB 	0x80		/* century enable bit */
#define M41T81REG_CTL_S		0x20		/* sign bit */
#define M41T81REG_CTL_FT 	0x40		/* frequency test bit */
#define M41T81REG_CTL_OUT 	0x80		/* output level */
#define M41T81REG_WD_RB0 	0x01		/* watchdog resolution bit 0 */
#define M41T81REG_WD_RB1 	0x02		/* watchdog resolution bit 1 */
#define M41T81REG_WD_BMB0 	0x04		/* watchdog multiplier bit 0 */
#define M41T81REG_WD_BMB1 	0x08		/* watchdog multiplier bit 1 */
#define M41T81REG_WD_BMB2 	0x10		/* watchdog multiplier bit 2 */
#define M41T81REG_WD_BMB3 	0x20		/* watchdog multiplier bit 3 */
#define M41T81REG_WD_BMB4 	0x40		/* watchdog multiplier bit 4 */
#define M41T81REG_AMO_ABE	0x20		/* alarm in "battery back-up mode" enable bit */
#define M41T81REG_AMO_SQWE	0x40		/* square wave enable */
#define M41T81REG_AMO_AFE 	0x80		/* alarm flag enable flag */
#define M41T81REG_ADT_RPT5 	0x40		/* alarm repeat mode bit 5 */
#define M41T81REG_ADT_RPT4 	0x80		/* alarm repeat mode bit 4 */
#define M41T81REG_AHR_RPT3 	0x80		/* alarm repeat mode bit 3 */
#define M41T81REG_AHR_HT	0x40		/* halt update bit */
#define M41T81REG_AMN_RPT2 	0x80		/* alarm repeat mode bit 2 */
#define M41T81REG_ASC_RPT1 	0x80		/* alarm repeat mode bit 1 */
#define M41T81REG_FLG_AF 	0x40		/* alarm flag (read only) */
#define M41T81REG_FLG_WDF 	0x80		/* watchdog flag (read only) */
#define M41T81REG_SQW_RS0 	0x10		/* sqw frequency bit 0 */
#define M41T81REG_SQW_RS1 	0x20		/* sqw frequency bit 1 */
#define M41T81REG_SQW_RS2 	0x40		/* sqw frequency bit 2 */
#define M41T81REG_SQW_RS3 	0x80		/* sqw frequency bit 3 */


/*
 * Register numbers
 */

#define M41T81REG_TSC	0x00		/* tenths/hundredths of second */
#define M41T81REG_SC	0x01		/* seconds */
#define M41T81REG_MN	0x02		/* minute */
#define M41T81REG_HR	0x03		/* hour/century */
#define M41T81REG_DY	0x04		/* day of week */
#define M41T81REG_DT	0x05		/* date of month */
#define M41T81REG_MO	0x06		/* month */
#define M41T81REG_YR	0x07		/* year */
#define M41T81REG_CTL	0x08		/* control */
#define M41T81REG_WD	0x09		/* watchdog */
#define M41T81REG_AMO	0x0A		/* alarm: month */
#define M41T81REG_ADT	0x0B		/* alarm: date */
#define M41T81REG_AHR	0x0C		/* alarm: hour */
#define M41T81REG_AMN	0x0D		/* alarm: minute */
#define M41T81REG_ASC	0x0E		/* alarm: second */
#define M41T81REG_FLG	0x0F		/* flags */
#define M41T81REG_SQW	0x13		/* square wave register */

#define M41T81_CCR_ADDRESS	0x68

#define BCD(x) (((x) % 10) + (((x) / 10) << 4))
#define SET_TIME	0x00
#define SET_DATE	0x01

/*  *********************************************************************
    *  Forward declarations
    ********************************************************************* */

static void m41t81_clock_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr);

static int m41t81_clock_open(cfe_devctx_t *ctx);
static int m41t81_clock_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int m41t81_clock_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int m41t81_clock_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int m41t81_clock_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int m41t81_clock_close(cfe_devctx_t *ctx);


/*  *********************************************************************
    *  Device dispatch
    ********************************************************************* */

const static cfe_devdisp_t m41t81_clock_dispatch = {
  m41t81_clock_open,
  m41t81_clock_read,
  m41t81_clock_inpstat,
  m41t81_clock_write,
  m41t81_clock_ioctl,
  m41t81_clock_close,
  NULL,
  NULL
};

const cfe_driver_t m41t81_clock = {
  "M41T81 RTC",
  "clock",
  CFE_DEV_CLOCK,
  &m41t81_clock_dispatch,
  m41t81_clock_probe
};
  

/*  *********************************************************************
    *  Structures
    ********************************************************************* */
typedef struct m41t81_clock_s {
  int smbus_channel;
} m41t81_clock_t;
  
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

    SBWRITECSR(reg,K_SMB_FREQ_400KHZ);

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
    *  Read a byte from the chip.
    *
    *  Input parameters: 
    *  	   chan - SMBus channel
    *  	   slaveaddr -  SMBus slave address
    *  	   devaddr - byte with in the M41T81 device to read
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

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_CMD));
    SBWRITECSR(reg,((devaddr & 0xFF) & 0xFF));

    /*
     * Start the command
     */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_START));
    SBWRITECSR(reg,V_SMB_TT(K_SMB_TT_WR1BYTE) | slaveaddr);

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
    *  write a byte from the chip.
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
    SBWRITECSR(reg,((devaddr & 0xFF) & 0xFF));

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_DATA));
    SBWRITECSR(reg,((b & 0xFF) & 0xFF));

    /*
     * Start the command.  Keep pounding on the device until it
     * submits or the timer expires, whichever comes first.  The
     * datasheet says writes can take up to 10ms, so we'll give it 500.
     */

    reg = PHYS_TO_K1(A_SMB_REGISTER(chan,R_SMB_START));
    SBWRITECSR(reg,V_SMB_TT(K_SMB_TT_WR2BYTE) | slaveaddr);

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
    *  m41t81_clock_probe(drv,a,b,ptr)
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

static void m41t81_clock_probe(cfe_driver_t *drv,
				     unsigned long probe_a, unsigned long probe_b, 
				     void *probe_ptr)
{
    m41t81_clock_t *softc;
    char descr[80];
    uint8_t byte;

    softc = (m41t81_clock_t *) KMALLOC(sizeof(m41t81_clock_t),0);

    /*
     * Probe_a is the SMBus channel number
     * Probe_b is unused.
     * Probe_ptr is unused.
     */

    softc->smbus_channel = (int)probe_a;

    xsprintf(descr,"%s on SMBus channel %d",
	     drv->drv_description,probe_a);
    cfe_attach(drv,softc,NULL,descr);


    /*
     * Reset HT bit to "0" to update registers with current time.
     */    
    byte = time_readrtc(softc->smbus_channel,M41T81_CCR_ADDRESS,M41T81REG_AHR);
    byte &= ~M41T81REG_AHR_HT;
    time_writertc(softc->smbus_channel,M41T81_CCR_ADDRESS,M41T81REG_AHR, byte);

   
}

/*  *********************************************************************
    *  m41t81_clock_open(ctx)
    *  
    *  Open this device.  For the M41T81, we do a quick test 
    *  read to be sure the device is out there.
    *  
    *  Input parameters: 
    *  	   ctx - device context (can obtain our softc here)
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int m41t81_clock_open(cfe_devctx_t *ctx)
{
    m41t81_clock_t *softc = ctx->dev_softc;
    int b = -1;
    int64_t timer;
    int chan = softc->smbus_channel;
    
    /*
     * Try to read from the device.  If it does not
     * respond, fail the open.  We may need to do this for
     * up to 300ms.
     */

    time_smbus_init(chan);

    TIMER_SET(timer,300);
    while (!TIMER_EXPIRED(timer)) {
	POLL();
	b = time_readrtc(chan,M41T81_CCR_ADDRESS,0);
	if (b >= 0) break;		/* read is ok */
	}
 
    return (b < 0) ? -1 : 0;
}

/*  *********************************************************************
    *  m41t81_clock_read(ctx,buffer)
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

static int m41t81_clock_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{

    m41t81_clock_t *softc = ctx->dev_softc;
    uint8_t byte;
    unsigned char *bptr;    
    int chan = softc->smbus_channel;

    bptr = buffer->buf_ptr;

    byte = (uint8_t)  time_readrtc(chan,M41T81_CCR_ADDRESS,M41T81REG_HR);
    byte &= ~(M41T81REG_HR_CB | M41T81REG_HR_CEB);
    *bptr++ = (unsigned char) byte;
    
    byte = (uint8_t) time_readrtc(chan,M41T81_CCR_ADDRESS,M41T81REG_MN);
    *bptr++ = (unsigned char) byte;
    
    byte = (uint8_t) time_readrtc(chan,M41T81_CCR_ADDRESS,M41T81REG_SC);
    byte &= ~(M41T81REG_SC_ST);
    *bptr++ = (unsigned char) byte;

    byte =  (uint8_t) time_readrtc(chan,M41T81_CCR_ADDRESS,M41T81REG_MO);
    *bptr++ = (unsigned char) byte;

    byte = (uint8_t) time_readrtc(chan,M41T81_CCR_ADDRESS,M41T81REG_DT);
    *bptr++ = (unsigned char) byte;

    byte =  (uint8_t) time_readrtc(chan,M41T81_CCR_ADDRESS,M41T81REG_YR);
    *bptr++ = (unsigned char) byte;

    byte = (uint8_t) time_readrtc(chan,M41T81_CCR_ADDRESS,M41T81REG_HR);
    byte &= M41T81REG_HR_CB;
    if (byte) {
	*bptr = 0x20;			/*Year 20xx*/
	}
    else {
	*bptr = 0x19;			/*Year 19xx*/
	}
   
    buffer->buf_retlen = 8;
    return 0;
}

/*  *********************************************************************
    *  m41t81_clock_write(ctx,buffer)
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

static int m41t81_clock_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    m41t81_clock_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    uint8_t hr,min,sec;
    uint8_t mo,day,yr,y2k;
    uint8_t timeDateFlag;
    uint8_t temp;
    int chan = softc->smbus_channel;    
    bptr = buffer->buf_ptr;
    timeDateFlag = *(bptr + 7);
 
    /* write time or date */
    if(timeDateFlag == SET_TIME) {

	temp = time_readrtc(chan,M41T81_CCR_ADDRESS,M41T81REG_HR);
	temp &= (M41T81REG_HR_CB | M41T81REG_HR_CEB);
	hr = (uint8_t) *bptr;
	hr = BCD(hr);
	hr |= temp;
	time_writertc(chan,M41T81_CCR_ADDRESS,M41T81REG_HR, hr);

	min = (uint8_t) *(bptr+1);
	time_writertc(chan,M41T81_CCR_ADDRESS,M41T81REG_MN, BCD(min) );

	sec = (uint8_t) *(bptr+2);
	sec &= ~M41T81REG_SC_ST;
	time_writertc(chan,M41T81_CCR_ADDRESS,M41T81REG_SC, BCD(sec) );

	buffer->buf_retlen = 3;
	} 
    else if(timeDateFlag == SET_DATE) {

	mo = (uint8_t) *(bptr+3);
	time_writertc(chan,M41T81_CCR_ADDRESS,M41T81REG_MO, BCD(mo) );

	day = (uint8_t) *(bptr+4);
	time_writertc(chan,M41T81_CCR_ADDRESS,M41T81REG_DT, BCD(day) );

	yr = (uint8_t) *(bptr+5);
	time_writertc(chan,M41T81_CCR_ADDRESS,M41T81REG_YR, BCD(yr) );

	y2k = (uint8_t) *(bptr+6);
	/*M41T81 does not have a century byte, so we don't need to write y2k
	 *But we should flip the century bit (CB) to "1" for year 20xx and to "0"
	 *for year 19xx.
	 */
	temp = (uint8_t) time_readrtc(chan,M41T81_CCR_ADDRESS,M41T81REG_HR);	
	if (y2k == 0x19) {
	    temp &= ~M41T81REG_HR_CB;
	    }
	else if (y2k == 0x20) {
	    temp |= M41T81REG_HR_CB;
	    }
	else {
	    return -1;
	    }	
	time_writertc(chan,M41T81_CCR_ADDRESS,M41T81REG_HR, temp );
    
	buffer->buf_retlen = 3;
	}
    else {
	return -1;
	}

    return 0;
}

/*  *********************************************************************
    *  m41t81_clock_inpstat(ctx,inpstat)
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

static int m41t81_clock_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
    inpstat->inp_status = 1;

    return 0;
}

/*  *********************************************************************
    *  m41t81_clock_ioctl(ctx,buffer)
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

static int m41t81_clock_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
  return 0;
}

/*  *********************************************************************
    *  m41t81_clock_close(ctx,buffer)
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

static int m41t81_clock_close(cfe_devctx_t *ctx)
{
    return 0;
}
