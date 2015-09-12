/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  USB debugging code			File: usbdebug.c
    *  
    *  This module contains debug code for USB.
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
#endif

#include "lib_malloc.h"
#include "lib_queue.h"
#include "usbchap9.h"
#include "usbd.h"


void usb_dbg_dumpportstatus(int port,usb_port_status_t *portstatus,int level)
{
    int idx;
    uint16_t x;

    for (idx = 0; idx < level; idx++) printf("  ");
    printf("PORT %d STATUS\n",port);

    for (idx = 0; idx < level; idx++) printf("  ");
    x = GETUSBFIELD((portstatus),wPortStatus);
    printf("wPortStatus     = %04X  ",x);
    if (x & 1) printf("DevicePresent ");
    if (x & 2) printf("Enabled ");
    if (x & 4) printf("Suspend ");
    if (x & 8) printf("OverCurrent ");
    if (x & 16) printf("InReset ");
    if (x & 256) printf("Powered ");
    if (x & 512) printf("LowSpeed ");
    printf("\n");
    for (idx = 0; idx < level; idx++) printf("  ");
    x = GETUSBFIELD((portstatus),wPortChange);
    printf("wPortChange     = %04X  ",x);
    if (x & 1) printf("ConnectChange ");
    if (x & 2) printf("EnableChange ");
    if (x & 4) printf("SuspendChange ");
    if (x & 8) printf("OverCurrentChange ");
    if (x & 16) printf("ResetChange ");
    printf("\n");
}

void usb_dbg_dumpeptdescr(usb_endpoint_descr_t * epdscr)
{
    printf("---------------------------------------------------\n");
    printf("ENDPOINT DESCRIPTOR\n");
 
    printf("bLength         = %d\n",epdscr->bLength);
    printf("bDescriptorType = %d\n",epdscr->bDescriptorType);
    printf("bEndpointAddr   = %02X\n",epdscr->bEndpointAddress);
    printf("bmAttrbutes     = %02X\n",epdscr->bmAttributes);
    printf("wMaxPacketSize  = %d\n",GETUSBFIELD(epdscr,wMaxPacketSize));
    printf("bInterval       = %d\n",epdscr->bInterval);
}

static char *getstringmaybe(usbdev_t *dev,int string)
{
    static char buf[256];

    return "";

    if (string == 0) {
	strcpy(buf,"none");
	return buf;
	}

    memset(buf,0,sizeof(buf));

    usb_get_string(dev,string,buf,sizeof(buf));

    return buf;
}

void usb_dbg_dumpdescriptors(usbdev_t *dev,uint8_t *ptr,int len)
{
    uint8_t *endptr;
    usb_config_descr_t  *cfgdscr;
    usb_interface_descr_t *ifdscr;
    usb_device_descr_t *devdscr;
    usb_endpoint_descr_t *epdscr;
    usb_hid_descr_t *hiddscr;
    usb_hub_descr_t *hubdscr;
    static char *eptattribs[4] = {"Control","Isoc","Bulk","Interrupt"};
    int idx;

    endptr = ptr + len;

    while (ptr < endptr) {

	cfgdscr = (usb_config_descr_t *) ptr;

	switch (cfgdscr->bDescriptorType) {
	    case USB_DEVICE_DESCRIPTOR_TYPE:
		devdscr = (usb_device_descr_t *) ptr;
		printf("---------------------------------------------------\n");
		printf("DEVICE DESCRIPTOR\n");
		printf("bLength         = %d\n",devdscr->bLength);
		printf("bDescriptorType = %d\n",devdscr->bDescriptorType);
		printf("bcdUSB          = %04X\n",GETUSBFIELD(devdscr,bcdUSB));
		printf("bDeviceClass    = %d\n",devdscr->bDeviceClass);
		printf("bDeviceSubClass = %d\n",devdscr->bDeviceSubClass);
		printf("bDeviceProtocol = %d\n",devdscr->bDeviceProtocol);
		printf("bMaxPktSize0    = %d\n",devdscr->bMaxPacketSize0);
		if (endptr-ptr <= 8) break;
		printf("idVendor        = %04X (%d)\n",
		       GETUSBFIELD(devdscr,idVendor),
		       GETUSBFIELD(devdscr,idVendor));
		printf("idProduct       = %04X (%d)\n",
		       GETUSBFIELD(devdscr,idProduct),
		       GETUSBFIELD(devdscr,idProduct));
		printf("bcdDevice       = %04X\n",GETUSBFIELD(devdscr,bcdDevice));
		printf("iManufacturer   = %d (%s)\n",
		       devdscr->iManufacturer,
		       getstringmaybe(dev,devdscr->iManufacturer));
		printf("iProduct        = %d (%s)\n",
		       devdscr->iProduct,
		       getstringmaybe(dev,devdscr->iProduct));
		printf("iSerialNumber   = %d (%s)\n",
		       devdscr->iSerialNumber,
		       getstringmaybe(dev,devdscr->iSerialNumber));
		printf("bNumConfigs     = %d\n",devdscr->bNumConfigurations);
		break;
	    case USB_CONFIGURATION_DESCRIPTOR_TYPE:

		cfgdscr = (usb_config_descr_t *) ptr;
		printf("---------------------------------------------------\n");
		printf("CONFIG DESCRIPTOR\n");

		printf("bLength         = %d\n",cfgdscr->bLength);
		printf("bDescriptorType = %d\n",cfgdscr->bDescriptorType);
		printf("wTotalLength    = %d\n",GETUSBFIELD(cfgdscr,wTotalLength));
		printf("bNumInterfaces  = %d\n",cfgdscr->bNumInterfaces);
		printf("bConfigValue    = %d\n",cfgdscr->bConfigurationValue);
		printf("iConfiguration  = %d (%s)\n",
		       cfgdscr->iConfiguration,
		       getstringmaybe(dev,cfgdscr->iConfiguration));
		printf("bmAttributes    = %02X\n",cfgdscr->bmAttributes);
		printf("MaxPower        = %d (%dma)\n",cfgdscr->MaxPower,cfgdscr->MaxPower*2);
		break;

	    case USB_INTERFACE_DESCRIPTOR_TYPE:
		printf("---------------------------------------------------\n");
		printf("INTERFACE DESCRIPTOR\n");

		ifdscr = (usb_interface_descr_t *) ptr;

		printf("bLength         = %d\n",ifdscr->bLength);
		printf("bDescriptorType = %d\n",ifdscr->bDescriptorType);
		printf("bInterfaceNum   = %d\n",ifdscr->bInterfaceNumber);
		printf("bAlternateSet   = %d\n",ifdscr->bAlternateSetting);
		printf("bNumEndpoints   = %d\n",ifdscr->bNumEndpoints);
		printf("bInterfaceClass = %d\n",ifdscr->bInterfaceClass);
		printf("bInterSubClass  = %d\n",ifdscr->bInterfaceSubClass);
		printf("bInterfaceProto = %d\n",ifdscr->bInterfaceProtocol);
		printf("iInterface      = %d (%s)\n",
		       ifdscr->iInterface,
		       getstringmaybe(dev,ifdscr->iInterface));
		break;

	    case USB_ENDPOINT_DESCRIPTOR_TYPE:
		printf("---------------------------------------------------\n");
		printf("ENDPOINT DESCRIPTOR\n");

		epdscr = (usb_endpoint_descr_t *) ptr;

		printf("bLength         = %d\n",epdscr->bLength);
		printf("bDescriptorType = %d\n",epdscr->bDescriptorType);
		printf("bEndpointAddr   = %02X (%d,%s)\n",
		       epdscr->bEndpointAddress,
		       epdscr->bEndpointAddress & 0x0F,
		       (epdscr->bEndpointAddress & USB_ENDPOINT_DIRECTION_IN) ? "IN" : "OUT"
		       );
		printf("bmAttrbutes     = %02X (%s)\n",
		       epdscr->bmAttributes,
		       eptattribs[epdscr->bmAttributes&3]);
		printf("wMaxPacketSize  = %d\n",GETUSBFIELD(epdscr,wMaxPacketSize));
		printf("bInterval       = %d\n",epdscr->bInterval);
		break;
		
	    case USB_HID_DESCRIPTOR_TYPE:
		printf("---------------------------------------------------\n");
		printf("HID DESCRIPTOR\n");

		hiddscr = (usb_hid_descr_t *) ptr;

		printf("bLength         = %d\n",hiddscr->bLength);
		printf("bDescriptorType = %d\n",hiddscr->bDescriptorType);
		printf("bcdHID          = %04X\n",GETUSBFIELD(hiddscr,bcdHID));
		printf("bCountryCode    = %d\n",hiddscr->bCountryCode);
		printf("bNumDescriptors = %d\n",hiddscr->bNumDescriptors);
		printf("bClassDescrType = %d\n",hiddscr->bClassDescrType);
		printf("wClassDescrLen  = %d\n",GETUSBFIELD(hiddscr,wClassDescrLength));
		break;

	    case USB_HUB_DESCRIPTOR_TYPE:
		printf("---------------------------------------------------\n");
		printf("HUB DESCRIPTOR\n");

		hubdscr = (usb_hub_descr_t *) ptr;

		printf("bLength         = %d\n",hubdscr->bDescriptorLength);
		printf("bDescriptorType = %d\n",hubdscr->bDescriptorType);
		printf("bNumberOfPorts  = %d\n",hubdscr->bNumberOfPorts);
		printf("wHubCharacters  = %04X\n",GETUSBFIELD(hubdscr,wHubCharacteristics));
		printf("bPowerOnToPwrGd = %d\n",hubdscr->bPowerOnToPowerGood);
		printf("bHubControlCurr = %d (ma)\n",hubdscr->bHubControlCurrent);
		printf("bRemPwerMask[0] = %02X\n",hubdscr->bRemoveAndPowerMask[0]);

		break;

	    default:
		printf("---------------------------------------------------\n");
		printf("UNKNOWN DESCRIPTOR\n");
		printf("bLength         = %d\n",cfgdscr->bLength);
		printf("bDescriptorType = %d\n",cfgdscr->bDescriptorType);
		printf("Data Bytes      = ");
		for (idx = 0; idx < cfgdscr->bLength; idx++) {
		    printf("%02X ",ptr[idx]);
		    }
		printf("\n");

	    }

	ptr += cfgdscr->bLength;

	}
}


void usb_dbg_dumpcfgdescr(usbdev_t *dev)
{
    uint8_t buffer[512];
    int res;
    int len;
    usb_config_descr_t  *cfgdscr;

    memset(buffer,0,sizeof(buffer));

    cfgdscr = (usb_config_descr_t *) &buffer[0];

    res = usb_get_config_descriptor(dev,cfgdscr,0,sizeof(usb_config_descr_t));
    if (res != sizeof(usb_config_descr_t)) {
	printf("[a]usb_get_config_descriptor returns %d\n",res);
	}

    len = GETUSBFIELD(cfgdscr,wTotalLength);

    res = usb_get_config_descriptor(dev,cfgdscr,0,len);
    if (res != len) {
	printf("[b]usb_get_config_descriptor returns %d\n",res);
	}

    usb_dbg_dumpdescriptors(dev,&buffer[0],res);
}
