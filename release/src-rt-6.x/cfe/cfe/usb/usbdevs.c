/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  USB Driver List				File: usbdevs.c
    *  
    *  This module contains a table of supported USB devices and
    *  the routines to look up appropriate drivers given
    *  USB product, device, and class codes.
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
#include <memory.h>
#include <stdint.h>
#include "usbhack.h"
#else
#include "lib_types.h"
#include "lib_string.h"
#include "lib_printf.h"
#endif

#include "lib_malloc.h"
#include "lib_queue.h"
#include "usbchap9.h"
#include "usbd.h"

/*  *********************************************************************
    *  The list of drivers we support.  If you add more drivers,
    *  list them here.
    ********************************************************************* */

extern usb_driver_t usbhub_driver;
extern usb_driver_t usbhid_driver;
extern usb_driver_t usbmass_driver;
extern usb_driver_t usbserial_driver;
extern usb_driver_t usbeth_driver;

usb_drvlist_t usb_drivers[] = {

    /*
     * Hub driver
     */

    {USB_DEVICE_CLASS_HUB,	VENDOR_ANY,	PRODUCT_ANY,	&usbhub_driver},

    /*
     * Keyboards and mice
     */

    {USB_DEVICE_CLASS_HUMAN_INTERFACE,	VENDOR_ANY,PRODUCT_ANY,	&usbhid_driver},

    /*
     * Mass storage devices
     */

    {USB_DEVICE_CLASS_STORAGE,	VENDOR_ANY,	PRODUCT_ANY,	&usbmass_driver},

    /*
     * Serial ports
     */

    {USB_DEVICE_CLASS_VENDOR_SPECIFIC,0x557,0x2008,&usbserial_driver},

    /*
     * Ethernet Adapters
     */

    {USB_DEVICE_CLASS_VENDOR_SPECIFIC,0x506,0x4601,&usbeth_driver},	/* 3Com */
    {USB_DEVICE_CLASS_VENDOR_SPECIFIC,0x66b,0x2202,&usbeth_driver},	/* Linksys */
    {USB_DEVICE_CLASS_VENDOR_SPECIFIC,0x66b,0x2203,&usbeth_driver},	/* Linksys */
    {USB_DEVICE_CLASS_VENDOR_SPECIFIC,0x66b,0x2204,&usbeth_driver},	/* Linksys */
    {USB_DEVICE_CLASS_VENDOR_SPECIFIC,0x66b,0x2206,&usbeth_driver},	/* Linksys */
    {USB_DEVICE_CLASS_VENDOR_SPECIFIC,0x66b,0x400b,&usbeth_driver},   	/* Linksys */
    {USB_DEVICE_CLASS_VENDOR_SPECIFIC,0x66b,0x200c,&usbeth_driver},	/* Linksys */
    {USB_DEVICE_CLASS_RESERVED,0xbda,0x8150,&usbeth_driver},	/* Realtek */
    {USB_DEVICE_CLASS_VENDOR_SPECIFIC,0x423,0x000a,&usbeth_driver},	/* CATC */
    {USB_DEVICE_CLASS_VENDOR_SPECIFIC,0x423,0x000c,&usbeth_driver},	/* Belkin */

    {0,0,0,NULL}
};


/*  *********************************************************************
    *  usb_find_driver(class,vendor,product)
    *  
    *  Find a suitable device driver to handle the specified
    *  class, vendor, or product.
    *  
    *  Input parameters: 
    *	   devdescr - device descriptor
    *
    *  Return value:
    *      pointer to device driver or NULL
    ********************************************************************* */

usb_driver_t *usb_find_driver(usbdev_t *dev)
{
    usb_device_descr_t *devdescr;
    usb_interface_descr_t *ifdescr;
    usb_drvlist_t *list;
    int dclass,vendor,product;

    devdescr = &(dev->ud_devdescr);

    dclass = devdescr->bDeviceClass;
    if (dclass == 0) {
	ifdescr = usb_find_cfg_descr(dev,USB_INTERFACE_DESCRIPTOR_TYPE,0);
	if (ifdescr) dclass = ifdescr->bInterfaceClass;
	}

    vendor = (int) GETUSBFIELD(devdescr,idVendor);
    product = (int) GETUSBFIELD(devdescr,idProduct);

    printf("USB: Locating Class %02X Vendor %04X Product %04X: ",dclass,vendor,product);

    list = usb_drivers;
    while (list->udl_disp) {
	if (((list->udl_class == dclass) || (list->udl_class == CLASS_ANY)) &&
	    ((list->udl_vendor == vendor) || (list->udl_vendor == VENDOR_ANY)) &&
	    ((list->udl_product == product) || (list->udl_product == PRODUCT_ANY))) {
	    printf("%s\n",list->udl_disp->udrv_name);
	    return list->udl_disp;
	    }
	list++;
	}

    printf("Not found.\n");

    return NULL;
}
