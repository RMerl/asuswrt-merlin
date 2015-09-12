/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  USB device layer				File: usbd.c
    *  
    *  This module deals with devices (things connected to USB buses)
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
#include "cfe_timer.h"
#endif

#include "lib_malloc.h"
#include "lib_queue.h"
#include "usbchap9.h"
#include "usbd.h"

/*  *********************************************************************
    *  Globals
    ********************************************************************* */

int usb_noisy = 0;


/*  *********************************************************************
    *  usb_create_pipe(dev,epaddr,mps,flags)
    *  
    *  Create a pipe, causing the corresponding endpoint to
    *  be created in the host controller driver.  Pipes form the
    *  basic "handle" for unidirectional communications with a 
    *  USB device.
    *  
    *  Input parameters: 
    *  	   dev - device we're talking about
    *  	   epaddr - endpoint address open, usually from the endpoint
    *  	             descriptor
    *  	   mps - maximum packet size understood by the device
    *  	   flags - flags for this pipe (UP_xxx flags)
    *  	   
    *  Return value:
    *  	   <0 if error
    *  	   0 if ok
    ********************************************************************* */

int usb_create_pipe(usbdev_t *dev,int epaddr,int mps,int flags)
{
    usbpipe_t *pipe;
    int pipeidx;
    
    pipeidx = USB_EPADDR_TO_IDX(epaddr);

    if (dev->ud_pipes[pipeidx] != NULL) {
	printf("Trying to create a pipe that was already created!\n");
	return 0;
	}
    
    pipe = KMALLOC(sizeof(usbpipe_t),0);

    if (!pipe) return -1;

    pipe->up_flags = flags;
    pipe->up_num = pipeidx;
    pipe->up_mps = mps;
    pipe->up_dev = dev;
    if (dev->ud_flags & UD_FLAG_LOWSPEED) flags |= UP_TYPE_LOWSPEED;
    pipe->up_hwendpoint = UBEPTCREATE(dev->ud_bus,
				      dev->ud_address,
				      USB_ENDPOINT_ADDRESS(epaddr),
				      mps,
				      flags);

    dev->ud_pipes[pipeidx] = pipe;

    return 0;
}

/*  *********************************************************************
    *  usb_open_pipe(dev,epdesc)
    *  
    *  Open a pipe given an endpoint descriptor - this is the
    *  normal way pipes get open, since you've just selected a 
    *  configuration and have the descriptors handy with all
    *  the information you need.
    *  
    *  Input parameters: 
    *  	   dev - device we're talking to
    *  	   epdesc - endpoint descriptor
    *  	   
    *  Return value:
    *  	   <0 if error
    *  	   else endpoint/pipe number (from descriptor)
    ********************************************************************* */

int usb_open_pipe(usbdev_t *dev,usb_endpoint_descr_t *epdesc)
{
    int res;
    int flags = 0;

    if (USB_ENDPOINT_DIR_IN(epdesc->bEndpointAddress)) flags |= UP_TYPE_IN;
    else flags |= UP_TYPE_OUT;

    switch (epdesc->bmAttributes & USB_ENDPOINT_TYPE_MASK) {
	case USB_ENDPOINT_TYPE_CONTROL:
	    flags |= UP_TYPE_CONTROL;
	    break;
	case USB_ENDPOINT_TYPE_ISOCHRONOUS:
	    flags |= UP_TYPE_ISOC;
	    break;
	case USB_ENDPOINT_TYPE_BULK:
	    flags |= UP_TYPE_BULK;
	    break;	
	case USB_ENDPOINT_TYPE_INTERRUPT:
	    flags |= UP_TYPE_INTR;
	    break;
	}

    res = usb_create_pipe(dev,
			  epdesc->bEndpointAddress,
			  GETUSBFIELD(epdesc,wMaxPacketSize),
			  flags);

    if (res < 0) return res;

    return epdesc->bEndpointAddress;
}


/*  *********************************************************************
    *  usb_destroy_pipe(dev,epaddr)
    *  
    *  Close(destroy) an open pipe and remove endpoint descriptor
    *  
    *  Input parameters: 
    *  	   dev - device we're talking to
    *  	   epaddr - pipe to close
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void usb_destroy_pipe(usbdev_t *dev,int epaddr)
{
    usbpipe_t *pipe;
    int pipeidx;

    pipeidx = USB_EPADDR_TO_IDX(epaddr);

    pipe = dev->ud_pipes[pipeidx];
    if (!pipe) return;

    if (dev->ud_pipes[pipeidx]) {
	UBEPTDELETE(dev->ud_bus,
		    dev->ud_pipes[pipeidx]->up_hwendpoint);
	}

    KFREE(dev->ud_pipes[pipeidx]);
    dev->ud_pipes[pipeidx] = NULL;
}

/*  *********************************************************************
    *  usb_destroy_device(dev)
    *  
    *  Delete an entire USB device, closing its pipes and freeing
    *  the device data structure
    *  
    *  Input parameters: 
    *  	   dev - device to destroy
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void usb_destroy_device(usbdev_t *dev)
{
    int idx;

    for (idx = 0; idx < UD_MAX_PIPES; idx++) {
	if (dev->ud_pipes[idx]) {
	    UBEPTDELETE(dev->ud_bus,
			dev->ud_pipes[idx]->up_hwendpoint);
	    KFREE(dev->ud_pipes[idx]);
	    }
	}

    dev->ud_bus->ub_devices[dev->ud_address] = NULL;

    KFREE(dev);
}


/*  *********************************************************************
    *  usb_create_device(bus,lowspeed)
    *  
    *  Create a new USB device.  This device will be set to 
    *  communicate on address zero (default address) and will be 
    *  ready for basic stuff so we can figure out what it is.
    *  The control pipe will be open, so you can start requesting
    *  descriptors right away.
    *  
    *  Input parameters: 
    *  	   bus - bus to create device on
    *  	   lowspeed - true if it's a lowspeed device (the hubs tell
    *  	     us these things)
    *  	   
    *  Return value:
    *  	   usb device structure, or NULL
    ********************************************************************* */

usbdev_t *usb_create_device(usbbus_t *bus,int lowspeed)
{
    usbdev_t *dev;
    int pipeflags;

    /*
     * Create the device structure.
     */

    dev = KMALLOC(sizeof(usbdev_t),0);
    memset(dev,0,sizeof(usbdev_t));

    dev->ud_bus = bus;
    dev->ud_address = 0;		/* default address */
    dev->ud_parent = NULL;
    dev->ud_flags = 0;

    /*
     * Adjust things based on the target device speed
     */

    pipeflags = UP_TYPE_CONTROL;
    if (lowspeed) {
	pipeflags |= UP_TYPE_LOWSPEED;
	dev->ud_flags |= UD_FLAG_LOWSPEED;
	}

    /*
     * Create the control pipe.
     */

    usb_create_pipe(dev,0,
		    USB_CONTROL_ENDPOINT_MIN_SIZE,
		    pipeflags);

    return dev;
}

/*  *********************************************************************
    *  usb_make_request(dev,epaddr,buf,len,flags)
    *  
    *  Create a template request structure with basic fields
    *  ready to go.  A shorthand routine.
    *  
    *  Input parameters: 
    *  	   dev- device we're talking to
    *  	   epaddr - endpoint address, from usb_open_pipe()
    *  	   buf,length - user buffer and buffer length
    *  	   flags - transfer direction, etc. (UR_xxx flags)
    *  	   
    *  Return value:
    *  	   usbreq_t pointer, or NULL
    ********************************************************************* */

usbreq_t *usb_make_request(usbdev_t *dev,int epaddr,uint8_t *buf,int length,int flags)
{
    usbreq_t *ur;
    usbpipe_t *pipe;
    int pipeidx;

    pipeidx = USB_EPADDR_TO_IDX(epaddr);

    pipe = dev->ud_pipes[pipeidx];

    if (pipe == NULL) return NULL;

    ur = KMALLOC(sizeof(usbreq_t),0);
    memset(ur,0,sizeof(usbreq_t));

    ur->ur_dev = dev;
    ur->ur_pipe = pipe;
    ur->ur_buffer = buf;
    ur->ur_length = length;
    ur->ur_flags = flags;
    ur->ur_callback = NULL;

    return ur;
    
}

/*  *********************************************************************
    *  usb_poll(bus)
    *  
    *  Handle device-driver polling - simply vectors to host controller
    *  driver.
    *  
    *  Input parameters: 
    *  	   bus - bus structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void usb_poll(usbbus_t *bus)
{
    UBINTR(bus);
}

/*  *********************************************************************
    *  usb_daemon(bus)
    *  
    *  Polls for topology changes and initiates a bus scan if
    *  necessary.
    *  
    *  Input parameters: 
    *  	   bus - bus to  watch
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void usb_daemon(usbbus_t *bus)
{
    /*
     * Just see if someone flagged a need for a scan here
     * and start the bus scan if necessary.
     *
     * The actual scanning is a hub function, starting at the
     * root hub, so the code for that is over there.
     */

    if (bus->ub_flags & UB_FLG_NEEDSCAN) {
	bus->ub_flags &= ~UB_FLG_NEEDSCAN;
	usb_scan(bus);
	}
}

/*  *********************************************************************
    *  usb_cancel_request(ur)
    *  
    *  Cancel a pending usb transfer request.
    *  
    *  Input parameters: 
    *  	   ur - request to cancel
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error (could not find request)
    ********************************************************************* */

int usb_cancel_request(usbreq_t *ur)
{
    printf("usb_cancel_request is not implemented.\n");
    return 0;
}

/*  *********************************************************************
    *  usb_free_request(ur)
    *  
    *  Return a transfer request to the free pool.
    *  
    *  Input parameters: 
    *  	   ur - request to return
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void usb_free_request(usbreq_t *ur)
{
    if (ur->ur_inprogress) {
	printf("Yow!  Tried to free a request that was in progress!\n");
	return;
	}
    KFREE(ur);
}

/*  *********************************************************************
    *  usb_delay_ms(bus,ms)
    *  
    *  Wait a while, calling the polling routine as we go.
    *  
    *  Input parameters: 
    *  	   bus - bus we're talking to 
    *  	   ms - how long to wait
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */


void usb_delay_ms(usbbus_t *bus,int ms)
{
#ifdef _CFE_
    cfe_sleep(1+((ms*CFE_HZ)/1000));
#else
    mydelay(ms);
#endif
}

/*  *********************************************************************
    *  usb_queue_request(ur)
    *  
    *  Call the transfer handler in the host controller driver to
    *  set up a transfer descriptor
    *  
    *  Input parameters: 
    *  	   ur - request to queue
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error
    ********************************************************************* */

int usb_queue_request(usbreq_t *ur)
{
    int res;

    ur->ur_inprogress = 1;
    ur->ur_xferred = 0;
    res = UBXFER(ur->ur_dev->ud_bus,
		 ur->ur_pipe->up_hwendpoint,
		 ur);
    return res;
}

/*  *********************************************************************
    *  usb_wait_request(ur)
    *  
    *  Wait until a request completes, calling the polling routine
    *  as we wait.
    *  
    *  Input parameters: 
    *  	   ur - request to wait for
    *  	   
    *  Return value:
    *  	   request status
    ********************************************************************* */

int usb_wait_request(usbreq_t *ur)
{
    while ((volatile int) (ur->ur_inprogress)) {
	usb_poll(ur->ur_dev->ud_bus);
	}

    return ur->ur_status;
}

/*  *********************************************************************
    *  usb_sync_request(ur)
    *  
    *  Synchronous request - call usb_queue and then usb_wait
    *  
    *  Input parameters: 
    *  	   ur - request to submit
    *  	   
    *  Return value:
    *  	   status of request
    ********************************************************************* */

int usb_sync_request(usbreq_t *ur)
{
    usb_queue_request(ur);
    return usb_wait_request(ur);
}

/*  *********************************************************************
    *  usb_simple_request(dev,reqtype,bRequest,wValue,wIndex)
    *  
    *  Handle a simple USB control pipe request.  These are OUT
    *  requests with no data phase.
    *  
    *  Input parameters: 
    *  	   dev - device we're talking to
    *  	   reqtype - request type (bmRequestType) for descriptor
    *  	   wValue - wValue for descriptor
    *  	   wIndex - wIndex for descriptor
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error
    ********************************************************************* */

int usb_simple_request(usbdev_t *dev,uint8_t reqtype,int bRequest,int wValue,int wIndex)
{
    return usb_std_request(dev,reqtype,bRequest,wValue,wIndex,NULL,0);
    
}


/*  *********************************************************************
    *  usb_set_configuration(dev,config)
    *  
    *  Set the current configuration for a USB device.
    * 
    *  Input parameters: 
    *  	   dev - device we're talking to
    *  	   config - bConfigValue for the device
    *  	   
    *  Return value:
    *  	   request status
    ********************************************************************* */

int usb_set_configuration(usbdev_t *dev,int config)
{
    int res;

    res = usb_simple_request(dev,0x00,USB_REQUEST_SET_CONFIGURATION,config,0);

    return res;
}


/*  *********************************************************************
    *  usb_new_address(bus)
    *  
    *  Return the next available address for the specified bus
    *  
    *  Input parameters: 
    *  	   bus - bus to assign an address for
    *  	   
    *  Return value:
    *  	   new address, <0 if error
    ********************************************************************* */

int usb_new_address(usbbus_t *bus)
{
    int idx;

    for (idx = 1; idx < USB_MAX_DEVICES; idx++) {
	if (bus->ub_devices[idx] == NULL) return idx;
	}

    return -1;
}

/*  *********************************************************************
    *  usb_set_address(dev,address)
    *  
    *  Set the address of a device.  This also puts the device
    *  in the master device table for the bus and reconfigures the
    *  address of the control pipe.
    *  
    *  Input parameters: 
    *  	   dev - device we're talking to
    *  	   address - new address (1..127)
    *  	   
    *  Return value:
    *  	   request status
    ********************************************************************* */

int usb_set_address(usbdev_t *dev,int address)
{
    int res;
    int idx;
    usbpipe_t *pipe;

    res = usb_simple_request(dev,0x00,USB_REQUEST_SET_ADDRESS,address,0);

    if (res == 0) {
	dev->ud_bus->ub_devices[address] = dev;
	dev->ud_address = address;
	for (idx = 0; idx < UD_MAX_PIPES; idx++) {
	    pipe = dev->ud_pipes[idx];
	    if (pipe && pipe->up_hwendpoint) {
		UBEPTSETADDR(dev->ud_bus,pipe->up_hwendpoint,address);
		}
	    }
	}

    return res;
}

/*  *********************************************************************
    *  usb_set_ep0mps(dev,mps)
    *  
    *  Set the maximum packet size of endpoint zero (mucks with the
    *  endpoint in the host controller)
    *  
    *  Input parameters: 
    *  	   dev - device we're talking to
    *  	   mps - max packet size for endpoint zero
    *  	   
    *  Return value:
    *  	   request status
    ********************************************************************* */

int usb_set_ep0mps(usbdev_t *dev,int mps)
{
    usbpipe_t *pipe;

    pipe = dev->ud_pipes[0];
    if (pipe && pipe->up_hwendpoint) {
	UBEPTSETMPS(dev->ud_bus,pipe->up_hwendpoint,mps);
	}
    if (pipe) {
	pipe->up_mps = mps;
	}

    return 0;
}

/*  *********************************************************************
    *  usb_clear_stall(dev,epaddr)
    *  
    *  Clear a stall condition on the specified pipe
    *  
    *  Input parameters: 
    *  	   dev - device we're talking to
    *  	   epaddr - endpoint address
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error
    ********************************************************************* */
int usb_clear_stall(usbdev_t *dev,int epaddr)
{
    uint8_t *requestbuf;
    usb_device_request_t *req;
    usbreq_t *ur;
    int res;
    int pipeidx;

    /*
     * Clear the stall in the hardware.
     */

    pipeidx = USB_EPADDR_TO_IDX(epaddr);

    UBEPTCLEARTOGGLE(dev->ud_bus,dev->ud_pipes[pipeidx]->up_hwendpoint);

    /*
     * Do the "clear stall" request.   Note that we should do this
     * without calling usb_simple_request, since usb_simple_request
     * may itself stall.
     */

    requestbuf = KMALLOC(32,0);

    req = (usb_device_request_t *) requestbuf;

    req->bmRequestType = 0x02;
    req->bRequest = USB_REQUEST_CLEAR_FEATURE;
    PUTUSBFIELD(req,wValue,0);		/* ENDPOINT_HALT */
    PUTUSBFIELD(req,wIndex,epaddr);
    PUTUSBFIELD(req,wLength,0);

    ur = usb_make_request(dev,0,requestbuf,
			  sizeof(usb_device_request_t),
			  UR_FLAG_SETUP);
    res = usb_sync_request(ur);
    usb_free_request(ur);
    ur = usb_make_request(dev,0,requestbuf,0,UR_FLAG_STATUS_IN);
    res = usb_sync_request(ur);
    usb_free_request(ur);

    KFREE(requestbuf);

    return 0;
}



/*  *********************************************************************
    *  usb_std_request(dev,bmRequestType,bRequest,wValue,
    *                   wIndex,buffer,length)
    *  
    *  Do a standard control request on the control pipe,
    *  with the appropriate setup, data, and status phases.
    *  
    *  Input parameters: 
    *  	   dev - dev we're talking to
    *  	   bmRequestType,bRequest,wValue,wIndex - fields for the
    *  	            USB request structure
    *  	   buffer - user buffer
    *  	   length - length of user buffer
    *  	   
    *  Return value:
    *  	   number of bytes transferred
    ********************************************************************* */

int usb_std_request(usbdev_t *dev,uint8_t bmRequestType,
			   uint8_t bRequest,uint16_t wValue,
			   uint16_t wIndex,uint8_t *buffer,int length)
{
    usbpipe_t *pipe = dev->ud_pipes[0];
    usbreq_t *ur;
    int res;
    usb_device_request_t *req;
    uint8_t *databuf = NULL;

    req = KMALLOC(32,0);

    if ((buffer != NULL) && (length !=0)) {
	databuf = KMALLOC(length,0);
	if (!(bmRequestType & USBREQ_DIR_IN)) {
	    memcpy(databuf,buffer,length);
	    }
	else {
	    memset(databuf,0,length);
	    }
	}

    req->bmRequestType = bmRequestType;
    req->bRequest = bRequest;
    PUTUSBFIELD(req,wValue,wValue);
    PUTUSBFIELD(req,wIndex,wIndex);
    PUTUSBFIELD(req,wLength,length);

    ur = usb_make_request(dev,0,(uint8_t *)req,sizeof(usb_device_request_t),UR_FLAG_SETUP);
    res = usb_sync_request(ur);
    usb_free_request(ur);

    if (length != 0) {
	if (bmRequestType & USBREQ_DIR_IN) {
	    ur = usb_make_request(dev,0,databuf,length,UR_FLAG_IN);
	    }
	else {
	    ur = usb_make_request(dev,0,databuf,length,UR_FLAG_OUT);
	    }

	res = usb_sync_request(ur);

	if (res == 4) {		/* STALL */
	    usb_clear_stall(dev,pipe->up_num);
	    usb_free_request(ur);
	    if (databuf) KFREE(databuf);
	    KFREE(req);
	    return 0;
	    }

	length = ur->ur_xferred;
	usb_free_request(ur);
	}

    if ((length != 0) && (databuf != NULL) && (bmRequestType & USBREQ_DIR_IN)) {
	memcpy(buffer,databuf,length);
	}

    if (bmRequestType & USBREQ_DIR_IN) {
	ur = usb_make_request(dev,0,(uint8_t *)req,0,UR_FLAG_STATUS_OUT);
	}
    else {
	ur = usb_make_request(dev,0,(uint8_t *)req,0,UR_FLAG_STATUS_IN);
	}

    res = usb_sync_request(ur);
    usb_free_request(ur);

    if (res == 4) {		/* STALL */
	usb_clear_stall(dev,pipe->up_num);
	if (databuf) KFREE(databuf);
	KFREE(req);
	return 0;
	}

    if (databuf) KFREE(databuf);
    KFREE(req);

    return length;
}




/*  *********************************************************************
    *  usb_get_descriptor(dev,reqtype,dsctype,dscidx,respbuf,buflen)
    *  
    *  Request a descriptor from the device.
    *  
    *  Input parameters: 
    *  	   dev - device we're talking to
    *  	   reqtype - bmRequestType field for descriptor we want
    *  	   dsctype - descriptor type we want
    *  	   dscidx - index of descriptor we want (often zero)
    *  	   respbuf - response buffer
    *  	   buflen - length of response buffer
    *  	   
    *  Return value:
    *  	   number of bytes transferred 
    ********************************************************************* */

int usb_get_descriptor(usbdev_t *dev,uint8_t reqtype,int dsctype,int dscidx,
		       uint8_t *respbuf,int buflen)
{
    return usb_std_request(dev,
			   reqtype,USB_REQUEST_GET_DESCRIPTOR,
			   USB_DESCRIPTOR_TYPEINDEX(dsctype,dscidx),
			   0,
			   respbuf,buflen);
}

/*  *********************************************************************
    *  usb_get_string(dev,id,buf,maxlen)
    *  
    *  Request a string from the device, converting it from
    *  unicode to ascii (brutally).
    *  
    *  Input parameters: 
    *  	   dev - device we're talking to
    *  	   id - string ID
    *  	   buf - buffer to receive string (null terminated)
    *  	   maxlen - length of buffer
    *  	   
    *  Return value:
    *  	   number of characters in returned string
    ********************************************************************* */

int usb_get_string(usbdev_t *dev,int id,char *buf,int maxlen)
{
    int amtcopy;
    uint8_t *respbuf;
    int idx;
    usb_string_descr_t *sdscr;

    respbuf = KMALLOC(maxlen*2+2,0);
    sdscr = (usb_string_descr_t *) respbuf;

    /*
     * First time just get the header of the descriptor so we can
     * get the string length
     */

    amtcopy = usb_get_descriptor(dev,USBREQ_DIR_IN,USB_STRING_DESCRIPTOR_TYPE,id,
				 respbuf,2);

    /*
     * now do it again to get the whole string.
     */

    if (maxlen > sdscr->bLength) maxlen = sdscr->bLength;

    amtcopy = usb_get_descriptor(dev,USBREQ_DIR_IN,USB_STRING_DESCRIPTOR_TYPE,id,
				 respbuf,maxlen);

    *buf = '\0';
    amtcopy = sdscr->bLength - 2;
    if (amtcopy <= 0) return amtcopy;

    for (idx = 0; idx < amtcopy; idx+=2) {
	*buf++ = sdscr->bString[idx];
	}

    *buf = '\0';

    KFREE(respbuf);

    return amtcopy;
}



/*  *********************************************************************
    *  usb_get_device_descriptor(dev,dscr,smallflg)
    *  
    *  Request the device descriptor for the device. This is often
    *  the first descriptor requested, so it needs to be done in
    *  stages so we can find out how big the control pipe is.
    *  
    *  Input parameters: 
    *  	   dev - device we're talking to
    *  	   dscr - pointer to buffer to receive descriptor 
    *  	   smallflg - TRUE to request just 8 bytes.
    *  	   
    *  Return value:
    *  	   number of bytes copied
    ********************************************************************* */

int usb_get_device_descriptor(usbdev_t *dev,usb_device_descr_t *dscr,int smallflg)
{
    int res;
    uint8_t *respbuf;
    int amtcopy;

    /*
     * Smallflg truncates the request 8 bytes.  We need to do this for
     * the very first transaction to a USB device in order to determine
     * the size of its control pipe.  Bad things will happen if you
     * try to retrieve more data than the control pipe will hold.
     *
     * So, be conservative at first and get the first 8 bytes of the
     * descriptor.   Byte 7 is bMaxPacketSize0, the size of the control
     * pipe.  Then you can go back and submit a bigger request for
     * everything else.
     */

    amtcopy = smallflg ? USB_CONTROL_ENDPOINT_MIN_SIZE : sizeof(usb_device_descr_t);

    respbuf = KMALLOC(64,0);
    res = usb_get_descriptor(dev,USBREQ_DIR_IN,USB_DEVICE_DESCRIPTOR_TYPE,0,respbuf,amtcopy);
    memcpy(dscr,respbuf,amtcopy);
    KFREE(respbuf);
    return res;

}

/*  *********************************************************************
    *  usb_get_config_descriptor(dev,dscr,idx,maxlen)
    *  
    *  Request the configuration descriptor from the device.
    *  
    *  Input parameters: 
    *  	   dev - device we're talking to
    *  	   dscr - descriptor buffer (receives data from device)
    *  	   idx - index of config we want (usually zero)
    *  	   maxlen - total size of buffer to receive descriptor
    *  	   
    *  Return value:
    *  	   number of bytes copied
    ********************************************************************* */

int usb_get_config_descriptor(usbdev_t *dev,usb_config_descr_t *dscr,int idx,int maxlen)
{
    int res;
    uint8_t *respbuf;

    respbuf = KMALLOC(maxlen,0);
    res = usb_get_descriptor(dev,USBREQ_DIR_IN,
			     USB_CONFIGURATION_DESCRIPTOR_TYPE,idx,
			     respbuf,maxlen);
    memcpy(dscr,respbuf,maxlen);
    KFREE(respbuf);
    return res;

}



/*  *********************************************************************
    *  usb_get_device_status(dev,status)
    *  
    *  Request status from the device (status descriptor)
    *  
    *  Input parameters: 
    *  	   dev - device we're talking to
    *  	   status - receives device_status structure
    *  	   
    *  Return value:
    *  	   number of bytes returned
    ********************************************************************* */

int usb_get_device_status(usbdev_t *dev,usb_device_status_t *status)
{
    return usb_std_request(dev,
			   USBREQ_DIR_IN,
			   0,
			   0,
			   0,
			   (uint8_t *) status,
			   sizeof(usb_device_status_t));
}


/*  *********************************************************************
    *  usb_complete_request(ur,status)
    *  
    *  Called when a usb request completes - pass status to
    *  caller and call the callback if there is one.
    *  
    *  Input parameters: 
    *  	   ur - usbreq_t to complete
    *  	   status - completion status
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void usb_complete_request(usbreq_t *ur,int status)
{
    ur->ur_status = status;
    ur->ur_inprogress = 0;
    if (ur->ur_callback) (*(ur->ur_callback))(ur);
}


/*  *********************************************************************
    *  usb_initroot(bus)
    *  
    *  Initialize the root hub for the bus - we need to do this
    *  each time a bus is configured.
    *  
    *  Input parameters: 
    *  	   bus - bus to initialize
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void usb_initroot(usbbus_t *bus)
{
    usbdev_t *dev;
    usb_driver_t *drv;
    int addr;
    int res;
    uint8_t *buf;
    int len;
    usb_config_descr_t cfgdescr;

    /*
     * Create a device for the root hub.
     */

    dev = usb_create_device(bus,0);
    bus->ub_roothub = dev;

    /*
     * Get the device descriptor.  Make sure it's a hub.
     */

    res = usb_get_device_descriptor(dev,&(dev->ud_devdescr),TRUE);

    if (dev->ud_devdescr.bDeviceClass != USB_DEVICE_CLASS_HUB) {
	printf("Error! Root device is not a hub!\n");
	return;
	}

    /*
     * Set up the max packet size for the control endpoint,
     * then get the rest of the descriptor.
     */
    
    usb_set_ep0mps(dev,dev->ud_devdescr.bMaxPacketSize0);
    res = usb_get_device_descriptor(dev,&(dev->ud_devdescr),FALSE);

    /*
     * Obtain a new address and set the address of the
     * root hub to this address.
     */

    addr = usb_new_address(dev->ud_bus);
    res = usb_set_address(dev,addr);

    /*
     * Get the configuration descriptor and all the
     * associated interface and endpoint descriptors.
     */

    res = usb_get_config_descriptor(dev,&cfgdescr,0,
				    sizeof(usb_config_descr_t));
    if (res != sizeof(usb_config_descr_t)) {
	printf("[a]usb_get_config_descriptor returns %d\n",res);
	}

    len = GETUSBFIELD(&cfgdescr,wTotalLength);
    buf = KMALLOC(len,0);

    res = usb_get_config_descriptor(dev,(usb_config_descr_t *)buf,0,len);
    if (res != len) {
	printf("[b]usb_get_config_descriptor returns %d\n",res);
	}

    dev->ud_cfgdescr = (usb_config_descr_t *) buf;

    /*
     * Select the configuration.  Not really needed for our poor
     * imitation root hub, but it's the right thing to do.
     */

    usb_set_configuration(dev,cfgdescr.bConfigurationValue);

    /*
     * Find the driver for this.  It had better be the hub
     * driver.
     */

    drv = usb_find_driver(dev);

    /*
     * Call the attach method.
     */

    (*(drv->udrv_attach))(dev,drv);

    /*
     * Hub should now be operational.
     */

}


/*  *********************************************************************
    *  usb_find_cfg_descr(dev,dtype,idx)
    *  
    *  Find a configuration descriptor - we retrieved all the config
    *  descriptors during discovery, this lets us dig out the one
    *  we want.
    *  
    *  Input parameters: 
    *  	   dev - device we are talking to
    *  	   dtype - descriptor type to find
    *  	   idx - index of descriptor if there's more than one
    *  	   
    *  Return value:
    *  	   pointer to descriptor or NULL if not found
    ********************************************************************* */

void *usb_find_cfg_descr(usbdev_t *dev,int dtype,int idx)
{
    uint8_t *endptr;
    uint8_t *ptr;
    usb_config_descr_t  *cfgdscr;

    if (dev->ud_cfgdescr == NULL) return NULL;

    ptr = (uint8_t *) dev->ud_cfgdescr;
    endptr = ptr + GETUSBFIELD((dev->ud_cfgdescr),wTotalLength);

    while (ptr < endptr) {

	cfgdscr = (usb_config_descr_t *) ptr;

	if (cfgdscr->bDescriptorType == dtype) {
	    if (idx == 0) return (void *) ptr;
	    else idx--;
	    }

	ptr += cfgdscr->bLength;

	}

    return NULL;
}
