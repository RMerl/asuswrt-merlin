/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  DS17887-3 RTC  driver		File: dev_sb1250_ds17887clock.c
    *  
    *  This module contains a CFE driver for a DS17887-3 generic bus
    *  real-time-clock.
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
#include "lib_printf.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"

#include "lib_physio.h"


/*  *********************************************************************
    *  Constants
    ********************************************************************* */

/*
 * Register bits
 */

#define DS17887REGA_UIP		0x80		/* Update-in-progress */
#define DS17887REGA_DV2		0x40		/* Countdown chain */
#define DS17887REGA_DV1		0x20		/* Oscillator enable */
#define DS17887REGA_DV0		0x10		/* Bank Select */
#define DS17887REGA_RS3		0x08		/* Rate-selection bits */
#define DS17887REGA_RS2		0x04
#define DS17887REGA_RS1		0x02
#define DS17887REGA_RS0		0x01

#define DS17887REGB_SET		0x80		/* Set bit */
#define DS17887REGB_PIE		0x40		/* Periodic Interrupt Enable */
#define DS17887REGB_AIE		0x20		/* Alarm Interrupt Enable */
#define DS17887REGB_UIE		0x10		/* Update-ended Interrupt Enable */
#define DS17887REGB_SQWE	0x08		/* Square-wave Enable */
#define DS17887REGB_DM		0x04		/* Data Mode (binary) */
#define DS17887REGB_24		0x02		/* 24-hour mode control bit */
#define DS17887REGB_DSE		0x01		/* Daylight Savings Enable */

#define DS17887REGC_IRQF	0x80		/* Interrupt request flag */
#define DS17887REGC_PF 		0x40		/* Periodic interrupt flag */
#define DS17887REGC_AF		0x20		/* Alarm interrupt flag */
#define DS17887REGC_UF		0x10		/* Update ended interrupt flag */

#define DS17887REGD_VRT		0x80		/* Valid RAM and time */

/*
 * Register numbers
 */

#define DS17887REG_SC		0x00		/* seconds */
#define DS17887REG_SCA		0x01		/* seconds alarm */
#define DS17887REG_MN		0x02		/* minutes */
#define DS17887REG_MNA		0x03		/* minutes alarm */
#define DS17887REG_HR		0x04		/* hours */
#define DS17887REG_HRA		0x05		/* hours alarm */
#define DS17887REG_DW		0x06		/* day of week */
#define DS17887REG_DM		0x07		/* day of month */
#define DS17887REG_MO		0x08		/* month */
#define DS17887REG_YR		0x09		/* year */
#define DS17887REG_A		0x0A		/* register A */
#define DS17887REG_B		0x0B		/* register B */
#define DS17887REG_C		0x0C		/* register C */
#define DS17887REG_D		0x0D		/* register D */

#define DS17887REG_CE		0x48		/* century (bank 1 only) */

#define BCD(x) (((x) % 10) + (((x) / 10) << 4))
#define SET_TIME	0x00
#define SET_DATE	0x01

#define WRITECSR(p,v) phys_write8((p),(v))
#define READCSR(p) phys_read8((p))

/*  *********************************************************************
    *  Forward declarations
    ********************************************************************* */

static void ds17887_clock_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr);

static int ds17887_clock_open(cfe_devctx_t *ctx);
static int ds17887_clock_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int ds17887_clock_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int ds17887_clock_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int ds17887_clock_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int ds17887_clock_close(cfe_devctx_t *ctx);


/*  *********************************************************************
    *  Device dispatch
    ********************************************************************* */

const static cfe_devdisp_t ds17887_clock_dispatch = {
  ds17887_clock_open,
  ds17887_clock_read,
  ds17887_clock_inpstat,
  ds17887_clock_write,
  ds17887_clock_ioctl,
  ds17887_clock_close,
  NULL,
  NULL
};

const cfe_driver_t ds17887_clock = {
  "DS17887 RTC",
  "clock",
  CFE_DEV_CLOCK,
  &ds17887_clock_dispatch,
  ds17887_clock_probe
};
  

/*  *********************************************************************
    *  Structures
    ********************************************************************* */
typedef struct ds17887_clock_s {
    physaddr_t clock_base;
} ds17887_clock_t;
  
/*  *********************************************************************
    *  ds17887_clock_probe(drv,a,b,ptr)
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

static void ds17887_clock_probe(cfe_driver_t *drv,
				     unsigned long probe_a, unsigned long probe_b, 
				     void *probe_ptr)
{
    ds17887_clock_t *softc;
    char descr[80];

    softc = (ds17887_clock_t *) KMALLOC(sizeof(ds17887_clock_t),0);

    /*
     * Probe_a is the clock base address
     * Probe_b is unused.
     * Probe_ptr is unused.
     */

    softc->clock_base = probe_a;

    xsprintf(descr,"%s at 0x%X",
	     drv->drv_description,(uint32_t)probe_a);
    cfe_attach(drv,softc,NULL,descr);
   
}

/*  *********************************************************************
    *  ds17887_clock_open(ctx)
    *  
    *  Open this device.  For the DS17887, we do a quick test 
    *  read to be sure the device is out there.
    *  
    *  Input parameters: 
    *  	   ctx - device context (can obtain our softc here)
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int ds17887_clock_open(cfe_devctx_t *ctx)
{
    ds17887_clock_t *softc = ctx->dev_softc;
    uint8_t byte;
    physaddr_t clockbase;

    clockbase =  softc->clock_base;
    
    /* Make sure battery is still good and RTC valid */
    if ( !(READCSR(clockbase+DS17887REG_D) & DS17887REGD_VRT) ) {
	      printf("Warning: Battery has failed.  Clock setting is not accurate.\n");
	}

    /* Switch to bank 1.  Mainly for century byte */
    byte = (uint8_t) (READCSR(clockbase+DS17887REG_A) & 0xFF);
    WRITECSR(clockbase+DS17887REG_A,DS17887REGA_DV0 | DS17887REGA_DV1 | byte);

    /* Set data mode to BCD, 24-hour mode, and enable daylight savings */
    byte = (uint8_t) (READCSR(clockbase+DS17887REG_B) & 0xFF);
    byte &= (~DS17887REGB_DM & ~DS17887REGB_AIE);
    WRITECSR(clockbase+DS17887REG_B, DS17887REGB_24 | DS17887REGB_DSE | byte );
  
    return 0;
}

/*  *********************************************************************
    *  ds17887_clock_read(ctx,buffer)
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

static int ds17887_clock_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{

    ds17887_clock_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    physaddr_t clockbase;

    clockbase = softc->clock_base;

    bptr = buffer->buf_ptr;

    *bptr++ =  READCSR(clockbase+DS17887REG_HR);
    *bptr++ =  READCSR(clockbase+DS17887REG_MN);
    *bptr++ =  READCSR(clockbase+DS17887REG_SC);
    *bptr++ =  READCSR(clockbase+DS17887REG_MO);
    *bptr++ =  READCSR(clockbase+DS17887REG_DM);
    *bptr++ =  READCSR(clockbase+DS17887REG_YR);
    *bptr++ =  READCSR(clockbase+DS17887REG_CE);
    
    buffer->buf_retlen = 8;
    return 0;
}

/*  *********************************************************************
    *  ds17887_clock_write(ctx,buffer)
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

static int ds17887_clock_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    ds17887_clock_t *softc = ctx->dev_softc;
    uint8_t byte;
    unsigned char *bptr;
    uint8_t hr,min,sec;
    uint8_t mo,day,yr,y2k;
    uint8_t timeDateFlag;
    physaddr_t clockbase;

    clockbase = softc->clock_base;

    bptr = buffer->buf_ptr;

    /* Set SET bit */
    byte = (uint8_t) (READCSR(clockbase+DS17887REG_B) & 0xFF);
    WRITECSR(clockbase+DS17887REG_B,DS17887REGB_SET | byte);

    timeDateFlag = *(bptr + 7);

    /* write time or date */
    if(timeDateFlag == SET_TIME) {

	hr = (uint8_t) *bptr;
	WRITECSR(clockbase+DS17887REG_HR,BCD(hr));
	   
	min = (uint8_t) *(bptr+1);
	WRITECSR(clockbase+DS17887REG_MN,BCD(min));
	
	sec = (uint8_t) *(bptr+2);
	WRITECSR(clockbase+DS17887REG_SC,BCD(sec));

	buffer->buf_retlen = 3;
	} 
    else if(timeDateFlag == SET_DATE) {

	mo = (uint8_t) *(bptr+3);
	WRITECSR(clockbase+DS17887REG_MO,BCD(mo));

	day = (uint8_t) *(bptr+4);
	WRITECSR(clockbase+DS17887REG_DM,BCD(day));

	yr = (uint8_t) *(bptr+5);
	WRITECSR(clockbase+DS17887REG_YR,BCD(yr));

	y2k = (uint8_t) *(bptr+6);
 	WRITECSR(clockbase+DS17887REG_CE,y2k);
   
	buffer->buf_retlen = 4;
	}
    else {
	return -1;
	}

    /* clear SET bit */
    byte = (uint8_t) (READCSR(clockbase+DS17887REG_B) & 0xFF);
    WRITECSR(clockbase+DS17887REG_B,~DS17887REGB_SET &  byte);

  return 0;
}

/*  *********************************************************************
    *  ds17887_clock_inpstat(ctx,inpstat)
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

static int ds17887_clock_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
    inpstat->inp_status = 1;

    return 0;
}

/*  *********************************************************************
    *  ds17887_clock_ioctl(ctx,buffer)
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

static int ds17887_clock_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
  return 0;
}

/*  *********************************************************************
    *  ds17887_clock_close(ctx,buffer)
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

static int ds17887_clock_close(cfe_devctx_t *ctx)
{
    return 0;
}
