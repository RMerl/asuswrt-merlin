/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  USB Serial Port Driver			File: usbserial.c	
    *  
    *  This device can talk to a few of those usb->serial converters
    *  out there.
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


#ifndef _CFE_
#include <stdio.h>
#include <time.h>
#include <memory.h>
#include <stdint.h>
#include "usbhack.h"
#else
#include "lib_types.h"
#include "lib_string.h"
#include "lib_printf.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "cfe_console.h"
#include "bsp_config.h"
#endif

#include "lib_malloc.h"
#include "lib_queue.h"
#include "usbchap9.h"
#include "usbd.h"

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define USER_FIFOSIZE	256

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

typedef struct usbser_linedata_s {
    uint8_t dLineDataBaud0,dLineDataBaud1,dLineDataBaud2,dLineDataBaud3;
    uint8_t bLineDataStopBits;	/* 0=1, 1=1.5, 2=2 */
    uint8_t bLineDataParity;	/* 0=none, 1=odd, 2=even, 3=mark, 4=space */
    uint8_t bLineDataBits;	/* 5,6,7,8 */
} usbser_linedata_t;


/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define GETDWFIELD(s,f) ((uint32_t)((s)->f##0) | ((uint32_t)((s)->f##1) << 8) | \
                          ((uint32_t)((s)->f##2) << 16) | ((uint32_t)((s)->f##3) << 24))
#define PUTDWFIELD(s,f,v) (s)->f##0 = (v & 0xFF); \
                           (s)->f##1 = ((v)>>8 & 0xFF); \
                           (s)->f##2 = ((v)>>16 & 0xFF); \
                           (s)->f##3 = ((v)>>24 & 0xFF);



/*  *********************************************************************
    *  Forward Definitions
    ********************************************************************* */

static int usbserial_attach(usbdev_t *dev,usb_driver_t *drv);
static int usbserial_detach(usbdev_t *dev);

#ifdef _CFE_
static void usb_uart_probe(cfe_driver_t *drv,
			       unsigned long probe_a, unsigned long probe_b, 
			       void *probe_ptr);

static int usb_uart_open(cfe_devctx_t *ctx);
static int usb_uart_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int usb_uart_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int usb_uart_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int usb_uart_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int usb_uart_close(cfe_devctx_t *ctx);

const static cfe_devdisp_t usb_uart_dispatch = {
    usb_uart_open,
    usb_uart_read,
    usb_uart_inpstat,
    usb_uart_write,
    usb_uart_ioctl,
    usb_uart_close,	
    NULL,
    NULL
};

const cfe_driver_t usb_uart = {
    "USB UART",
    "uart",
    CFE_DEV_SERIAL,
    &usb_uart_dispatch,
    usb_uart_probe
};

typedef struct usb_uart_s {
    int uart_unit;
    int uart_speed;
    int uart_flowcontrol;
} usb_uart_t;

#define USBUART_MAXUNITS	4
static usbdev_t *usbuart_units[USBUART_MAXUNITS];
#endif


/*  *********************************************************************
    *  Structures
    ********************************************************************* */

typedef struct usbserial_softc_s {
    int user_inpipe;
    int user_outpipe;
    int user_outmps;
    int user_intpipe;
    uint8_t user_inbuf[USER_FIFOSIZE];
    int user_inbuf_in;
    int user_inbuf_out;
    uint8_t *user_devinbuf;
    int user_devinbufsize;
    int user_unit;
    uint8_t *user_intbuf;
    usbser_linedata_t user_linedata;
} usbserial_softc_t;

usb_driver_t usbserial_driver = {
    "USB Serial Port",
    usbserial_attach,
    usbserial_detach
};

usbdev_t *usbserial_dev = NULL;



/*  *********************************************************************
    *  usbserial_set_linedata(dev,linedata)
    *  
    *  Set line data to the device.
    *  
    *  Input parameters: 
    *  	   dev - USB device
    *  	   linedata - pointer to structure
    *  	   
    *  Return value:
    *  	   # of bytes returned 
    *  	   <0 if error
    ********************************************************************* */

static int usbserial_set_linedata(usbdev_t *dev,usbser_linedata_t *ldata)
{
    int res;

    /*
     * Send request to device.
     */

    res = usb_std_request(dev,0x21,0x20,0,0,(uint8_t *) ldata,sizeof(usbser_linedata_t));

    return res;
}


/*  *********************************************************************
    *  usbserial_tx_data(dev,buffer,len)
    *  
    *  Synchronously transmit data via the USB.
    *  
    *  Input parameters: 
    *  	   dev - device pointer
    *  	   buffer,len - data we want to send
    *  	   
    *  Return value:
    *  	   number of bytes sent.
    ********************************************************************* */

static int usbserial_tx_data(usbdev_t *dev,uint8_t *buffer,int len)
{
    uint8_t *bptr;
    usbreq_t *ur;
    usbserial_softc_t *softc = (dev->ud_private);
    int res;

    bptr = KMALLOC(len,0);

    memcpy(bptr,buffer,len);

    ur = usb_make_request(dev,softc->user_outpipe,bptr,len,UR_FLAG_OUT);
    res = usb_sync_request(ur);
    
//    printf("Data sent, status=%d, xferred=%d\n",res,ur->ur_xferred);

    res = ur->ur_xferred;

    usb_free_request(ur);

    KFREE(bptr);

    return res;
}

/*  *********************************************************************
    *  usbserial_int_callback(ur)
    *  
    *  Callback routine for the interrupt request, for devices
    *  that have an interrupt pipe.  We ignore this.
    *  
    *  Input parameters: 
    *  	   ur - usb request
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static int usbserial_int_callback(usbreq_t *ur)
{
//    int idx;

    /*
     * Check to see if the request was cancelled by someone
     * deleting our endpoint.
     */

    if (ur->ur_status == 0xFF) {
	usb_free_request(ur);
	return 0;
	}

//    printf("serial int msg: ");
//    for (idx = 0; idx < ur->ur_xferred; idx++) printf("%02X ",ur->ur_buffer[idx]);
//    printf("\n");

    usb_queue_request(ur);

    return 0;

}


/*  *********************************************************************
    *  usbserial_rx_callback(ur)
    *  
    *  Callback routine for the regular data pipe.
    *  
    *  Input parameters: 
    *  	   ur - usb request
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static int usbserial_rx_callback(usbreq_t *ur)
{
    int idx;
    int iptr;
    usbserial_softc_t *user = (ur->ur_dev->ud_private);

    /*
     * Check to see if the request was cancelled by someone
     * deleting our endpoint.
     */

    if (ur->ur_status == 0xFF) {
	usb_free_request(ur);
	return 0;
	}

    /*
     * Add characters to the receive fifo
     */

    for (idx = 0; idx < ur->ur_xferred; idx++) {
	iptr = (user->user_inbuf_in + 1) & (USER_FIFOSIZE-1);
	if (iptr == user->user_inbuf_out) break;	/* overflow */
	user->user_inbuf[user->user_inbuf_in] = ur->ur_buffer[idx];
	user->user_inbuf_in = iptr;
	}

    /*
     * Requeue the request
     */

    usb_queue_request(ur);

    return 0;

}


/*  *********************************************************************
    *  usbserial_attach(dev,drv)
    *  
    *  This routine is called when the bus scan stuff finds a mass-storage
    *  device.  We finish up the initialization by configuring the
    *  device and allocating our softc here.
    *  
    *  Input parameters: 
    *  	   dev - usb device, in the "addressed" state.
    *  	   drv - the driver table entry that matched
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

static int usbserial_attach(usbdev_t *dev,usb_driver_t *drv)
{
    usb_config_descr_t *cfgdscr = dev->ud_cfgdescr;
    usb_endpoint_descr_t *epdscr;
    usb_endpoint_descr_t *indscr = NULL;
    usb_endpoint_descr_t *outdscr = NULL;
    usb_endpoint_descr_t *intdscr = NULL;
    usb_interface_descr_t *ifdscr;
    usbser_linedata_t *ldata;
    usbserial_softc_t *softc;
    usbreq_t *ur;
    int idx;

    dev->ud_drv = drv;

    softc = KMALLOC(sizeof(usbserial_softc_t),0);
    memset(softc,0,sizeof(usbserial_softc_t));
    dev->ud_private = softc;

    ifdscr = usb_find_cfg_descr(dev,USB_INTERFACE_DESCRIPTOR_TYPE,0);
    if (ifdscr == NULL) {
	printf("Could not get interface descriptor\n");
	return -1;
	}

    for (idx = 0; idx < ifdscr->bNumEndpoints; idx++) {
	epdscr = usb_find_cfg_descr(dev,USB_ENDPOINT_DESCRIPTOR_TYPE,idx);

	if ((epdscr->bmAttributes & USB_ENDPOINT_TYPE_MASK) == 
	    USB_ENDPOINT_TYPE_INTERRUPT) {
	    intdscr = epdscr;
	    }
	else if (USB_ENDPOINT_DIR_OUT(epdscr->bEndpointAddress)) {
	    outdscr = epdscr;
	    }
	else {
	    indscr = epdscr;
	    }
	}


    if (!indscr || !outdscr) {
	printf("IN or OUT endpoint descriptors are missing\n");
	/*
	 * Could not get descriptors, something is very wrong.
	 * Leave device addressed but not configured.
	 */
	return 0;
	}

    /*
     * Choose the standard configuration.
     */

    usb_set_configuration(dev,cfgdscr->bConfigurationValue);

    /*
     * Open the pipes.
     */

    softc->user_inpipe     = usb_open_pipe(dev,indscr);
    softc->user_devinbufsize = GETUSBFIELD(indscr,wMaxPacketSize);
    softc->user_devinbuf = KMALLOC(softc->user_devinbufsize,0);
    softc->user_outpipe    = usb_open_pipe(dev,outdscr);
    softc->user_outmps = GETUSBFIELD(outdscr,wMaxPacketSize);
    if (intdscr) {
	softc->user_intpipe     = usb_open_pipe(dev,intdscr);
	}
    else {
	softc->user_intpipe = -1;
	}

    ur = usb_make_request(dev,softc->user_inpipe,softc->user_devinbuf,
			  softc->user_devinbufsize,
			  UR_FLAG_IN | UR_FLAG_SHORTOK);
    ur->ur_callback = usbserial_rx_callback;
    usb_queue_request(ur);


    if (softc->user_intpipe) {
	softc->user_intbuf = KMALLOC(32,0);
	ur = usb_make_request(dev,softc->user_intpipe,softc->user_intbuf,
			      GETUSBFIELD(intdscr,wMaxPacketSize),
			      UR_FLAG_IN | UR_FLAG_SHORTOK);
	ur->ur_callback = usbserial_int_callback;	
	usb_queue_request(ur);
	}

#ifdef _CFE_
    softc->user_unit = -1;
    for (idx = 0; idx < USBUART_MAXUNITS; idx++) {
	if (usbuart_units[idx] == NULL) {
	    softc->user_unit = idx;
	    usbuart_units[idx] = dev;
	    break;
	    }
	}

    console_log("USBSERIAL: Unit %d connected",softc->user_unit);
#endif

//    usbserial_song_and_dance(dev);

    ldata = &(softc->user_linedata);
    PUTDWFIELD(ldata,dLineDataBaud,115200);
    ldata->bLineDataStopBits = 0;
    ldata->bLineDataParity = 2;
    ldata->bLineDataBits = 8;

    usbserial_set_linedata(dev,ldata);
//    usbserial_get_linedata(dev,NULL);

    usbserial_dev = dev;

    return 0;
}

/*  *********************************************************************
    *  usbserial_detach(dev)
    *  
    *  This routine is called when the bus scanner notices that
    *  this device has been removed from the system.  We should
    *  do any cleanup that is required.  The pending requests
    *  will be cancelled automagically.
    *  
    *  Input parameters: 
    *  	   dev - usb device
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */

static int usbserial_detach(usbdev_t *dev)
{
    usbserial_softc_t *softc;

    softc = dev->ud_private;


#ifdef _CFE_
    console_log("USBSERIAL: USB unit %d disconnected",softc->user_unit);
    if (softc->user_unit >= 0) usbuart_units[softc->user_unit] = NULL;
#endif

    if (softc) {
	if (softc->user_devinbuf) KFREE(softc->user_devinbuf);
	if (softc->user_intbuf) KFREE(softc->user_intbuf);
	KFREE(softc);
	}

    return 0;
}
 


#ifdef _CFE_



static void usb_uart_probe(cfe_driver_t *drv,
			       unsigned long probe_a, unsigned long probe_b, 
			       void *probe_ptr)
{
    usb_uart_t *softc;
    char descr[80];

    softc = (usb_uart_t *) KMALLOC(sizeof(usb_uart_t),0);

    memset(softc,0,sizeof(usb_uart_t));

    softc->uart_unit = (int)probe_a;

    xsprintf(descr,"USB UART unit %d",(int)probe_a);

    cfe_attach(drv,softc,NULL,descr);
}


static int usb_uart_open(cfe_devctx_t *ctx)
{
//    usb_uart_t *softc = ctx->dev_softc;
//    int baudrate = CFG_SERIAL_BAUD_RATE;
//    usbdev_t *dev = usbuart_units[softc->uart_unit];


    return 0;
}

static int usb_uart_read(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
    usb_uart_t *softc = ctx->dev_softc;
    usbdev_t *dev = usbuart_units[softc->uart_unit];
    usbserial_softc_t *user = dev->ud_private;
    unsigned char *bptr;
    int blen;

    if (!dev) return 0;

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;

    while ((blen > 0) && (user->user_inbuf_out != user->user_inbuf_in)) {
	*bptr++ = user->user_inbuf[user->user_inbuf_out];
	user->user_inbuf_out = (user->user_inbuf_out + 1) & (USER_FIFOSIZE-1);
	blen--;
	}

    buffer->buf_retlen = buffer->buf_length - blen;
    return 0;
}

static int usb_uart_inpstat(cfe_devctx_t *ctx, iocb_inpstat_t *inpstat)
{
    usb_uart_t *softc = ctx->dev_softc;
    usbdev_t *dev = usbuart_units[softc->uart_unit];
    usbserial_softc_t *user = dev->ud_private;

    inpstat->inp_status = 0;

    if (!dev) return 0;

    inpstat->inp_status = (user->user_inbuf_in != user->user_inbuf_out);

    return 0;
}

static int usb_uart_write(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
    usb_uart_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    int blen;
    usbdev_t *dev = usbuart_units[softc->uart_unit];
    usbserial_softc_t *user = dev->ud_private;

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;

    if (!dev) {
	buffer->buf_retlen = blen;
	return 0;
	}

    if (blen > user->user_outmps) blen = user->user_outmps;

    usbserial_tx_data(dev,bptr,blen);

    buffer->buf_retlen = blen;
    return 0;
}

static int usb_uart_ioctl(cfe_devctx_t *ctx, iocb_buffer_t *buffer) 
{
    usb_uart_t *softc = ctx->dev_softc;
    usbdev_t *dev = usbuart_units[softc->uart_unit];
//    usbserial_softc_t *user = dev->ud_private;

    if (!dev) return -1;

    unsigned int *info = (unsigned int *) buffer->buf_ptr;

    switch ((int)buffer->buf_ioctlcmd) {
	case IOCTL_SERIAL_GETSPEED:
	    *info = softc->uart_speed;
	    break;
	case IOCTL_SERIAL_SETSPEED:
	    softc->uart_speed = *info;
	    /* NYI */
	    break;
	case IOCTL_SERIAL_GETFLOW:
	    *info = softc->uart_flowcontrol;
	    break;
	case IOCTL_SERIAL_SETFLOW:
	    softc->uart_flowcontrol = *info;
	    /* NYI */
	    break;
	default:
	    return -1;
	}

    return 0;
}

static int usb_uart_close(cfe_devctx_t *ctx)
{
//    usb_uart_t *softc = ctx->dev_softc;

    return 0;
}



#endif
