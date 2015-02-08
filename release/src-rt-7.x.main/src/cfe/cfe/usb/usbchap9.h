/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Chapter 9 definitions			File: usbchap9.h
    *  
    *  This module contains definitions from the USB specification,
    *  chapter 9.
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



#ifndef   _USBCHAP9_H_
#define   _USBCHAP9_H_

#define MAXIMUM_USB_STRING_LENGTH 255

/*
 * values for the bits returned by the USB GET_STATUS command
 */
#define USB_GETSTATUS_SELF_POWERED                0x01
#define USB_GETSTATUS_REMOTE_WAKEUP_ENABLED       0x02


#define USB_DEVICE_DESCRIPTOR_TYPE                0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE         0x02
#define USB_STRING_DESCRIPTOR_TYPE                0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE             0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE              0x05
#define USB_POWER_DESCRIPTOR_TYPE                 0x06
#define USB_HID_DESCRIPTOR_TYPE			  0x21
#define USB_HUB_DESCRIPTOR_TYPE			  0x29

#define USB_DESCRIPTOR_TYPEINDEX(d, i) ((uint16_t)((uint16_t)(d)<<8 | (i)))

/*
 * Values for bmAttributes field of an
 * endpoint descriptor
 */

#define USB_ENDPOINT_TYPE_MASK                    0x03

#define USB_ENDPOINT_TYPE_CONTROL                 0x00
#define USB_ENDPOINT_TYPE_ISOCHRONOUS             0x01
#define USB_ENDPOINT_TYPE_BULK                    0x02
#define USB_ENDPOINT_TYPE_INTERRUPT               0x03


/*
 * definitions for bits in the bmAttributes field of a 
 * configuration descriptor.
 */
#define USB_CONFIG_POWERED_MASK                   0xc0

#define USB_CONFIG_BUS_POWERED                    0x80
#define USB_CONFIG_SELF_POWERED                   0x40
#define USB_CONFIG_REMOTE_WAKEUP                  0x20

/*
 * Endpoint direction bit, stored in address
 */

#define USB_ENDPOINT_DIRECTION_MASK               0x80
#define USB_ENDPOINT_DIRECTION_IN		  0x80	/* bit set means IN */

/*
 * test direction bit in the bEndpointAddress field of
 * an endpoint descriptor.
 */
#define USB_ENDPOINT_DIR_OUT(addr)          (!((addr) & USB_ENDPOINT_DIRECTION_MASK))
#define USB_ENDPOINT_DIR_IN(addr)           ((addr) & USB_ENDPOINT_DIRECTION_MASK)

#define USB_ENDPOINT_ADDRESS(addr)		((addr) & 0x0F)

/*
 * USB defined request codes
 * see chapter 9 of the USB 1.0 specifcation for
 * more information.
 */

/*
 * These are the correct values based on the USB 1.0
 * specification
 */

#define USB_REQUEST_GET_STATUS                    0x00
#define USB_REQUEST_CLEAR_FEATURE                 0x01

#define USB_REQUEST_SET_FEATURE                   0x03

#define USB_REQUEST_SET_ADDRESS                   0x05
#define USB_REQUEST_GET_DESCRIPTOR                0x06
#define USB_REQUEST_SET_DESCRIPTOR                0x07
#define USB_REQUEST_GET_CONFIGURATION             0x08
#define USB_REQUEST_SET_CONFIGURATION             0x09
#define USB_REQUEST_GET_INTERFACE                 0x0A
#define USB_REQUEST_SET_INTERFACE                 0x0B
#define USB_REQUEST_SYNC_FRAME                    0x0C


/*
 * defined USB device classes
 */


#define USB_DEVICE_CLASS_RESERVED           0x00
#define USB_DEVICE_CLASS_AUDIO              0x01
#define USB_DEVICE_CLASS_COMMUNICATIONS     0x02
#define USB_DEVICE_CLASS_HUMAN_INTERFACE    0x03
#define USB_DEVICE_CLASS_MONITOR            0x04
#define USB_DEVICE_CLASS_PHYSICAL_INTERFACE 0x05
#define USB_DEVICE_CLASS_POWER              0x06
#define USB_DEVICE_CLASS_PRINTER            0x07
#define USB_DEVICE_CLASS_STORAGE            0x08
#define USB_DEVICE_CLASS_HUB                0x09
#define USB_DEVICE_CLASS_VENDOR_SPECIFIC    0xFF

/*
 * USB defined Feature selectors
 */

#define USB_FEATURE_ENDPOINT_STALL          0x0000
#define USB_FEATURE_REMOTE_WAKEUP           0x0001
#define USB_FEATURE_POWER_D0                0x0002
#define USB_FEATURE_POWER_D1                0x0003
#define USB_FEATURE_POWER_D2                0x0004
#define USB_FEATURE_POWER_D3                0x0005

/*
 * USB Device descriptor.
 * To reduce problems with compilers trying to optimize
 * this structure, all the fields are bytes.
 */

#define USBWORD(x) ((x) & 0xFF),(((x) >> 8) & 0xFF)

#define USB_CONTROL_ENDPOINT_MIN_SIZE	8

typedef struct usb_device_descr_s {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bcdUSBLow,bcdUSBHigh;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint8_t idVendorLow,idVendorHigh;
    uint8_t idProductLow,idProductHigh;
    uint8_t bcdDeviceLow,bcdDeviceHigh;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} usb_device_descr_t;

typedef struct usb_endpoint_descr_s {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint8_t wMaxPacketSizeLow,wMaxPacketSizeHigh;
    uint8_t bInterval;
} usb_endpoint_descr_t;

/*
 * values for bmAttributes Field in
 * USB_CONFIGURATION_DESCRIPTOR
 */

#define CONFIG_BUS_POWERED           0x80
#define CONFIG_SELF_POWERED          0x40
#define CONFIG_REMOTE_WAKEUP         0x20

typedef struct usb_config_descr_s {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t wTotalLengthLow,wTotalLengthHigh;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t MaxPower;
} usb_config_descr_t;

#define USB_INTERFACE_CLASS_HUB		0x09

typedef struct usb_interface_descr_s {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} usb_interface_descr_t;

typedef struct usb_string_descr_s {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bString[1];
} usb_string_descr_t;

/*
 * USB power descriptor added to core specification
 */

#define USB_SUPPORT_D0_COMMAND      0x01
#define USB_SUPPORT_D1_COMMAND      0x02
#define USB_SUPPORT_D2_COMMAND      0x04
#define USB_SUPPORT_D3_COMMAND      0x08

#define USB_SUPPORT_D1_WAKEUP       0x10
#define USB_SUPPORT_D2_WAKEUP       0x20


typedef struct usb_power_descr_s {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bCapabilitiesFlags;
    uint16_t EventNotification;
    uint16_t D1LatencyTime;
    uint16_t D2LatencyTime;
    uint16_t D3LatencyTime;
    uint8_t PowerUnit;
    uint16_t D0PowerConsumption;
    uint16_t D1PowerConsumption;
    uint16_t D2PowerConsumption;
} usb_power_descr_t;


typedef struct usb_common_descr_s {
    uint8_t bLength;
    uint8_t bDescriptorType;
} usb_common_descr_t;

typedef struct usb_device_status_s {
    uint8_t wDeviceStatusLow,wDeviceStatusHigh;
} usb_device_status_t;


/*
 * Standard USB HUB definitions 
 *
 * See Chapter 11
 */

#define USB_HUB_DESCR_SIZE	8
typedef struct usb_hub_descr_s {
    uint8_t        bDescriptorLength;      /* Length of this descriptor */
    uint8_t        bDescriptorType;        /* Hub configuration type */
    uint8_t        bNumberOfPorts;         /* number of ports on this hub */
    uint8_t        wHubCharacteristicsLow; /* Hub Charateristics */
    uint8_t        wHubCharacteristicsHigh;
    uint8_t        bPowerOnToPowerGood;    /* port power on till power good in 2ms */
    uint8_t        bHubControlCurrent;     /* max current in mA */
    /* room for 255 ports power control and removable bitmask */
    uint8_t        bRemoveAndPowerMask[64];
} usb_hub_descr_t;

#define USB_HUBCHAR_PWR_GANGED	0
#define USB_HUBCHAR_PWR_IND	1
#define USB_HUBCHAR_PWR_NONE	2

typedef struct usb_hub_status_s {
    uint8_t	   wHubStatusLow,wHubStatusHigh;
    uint8_t	   wHubChangeLow,wHubChangeHigh;
} usb_hub_status_t;

#define USB_PORT_STATUS_CONNECT	0x0001
#define USB_PORT_STATUS_ENABLED 0x0002
#define USB_PORT_STATUS_SUSPEND 0x0004
#define USB_PORT_STATUS_OVERCUR 0x0008
#define USB_PORT_STATUS_RESET   0x0010
#define USB_PORT_STATUS_POWER   0x0100
#define USB_PORT_STATUS_LOWSPD	0x0200

typedef struct usb_port_status_s {
    uint8_t	   wPortStatusLow,wPortStatusHigh;
    uint8_t	   wPortChangeLow,wPortChangeHigh;
} usb_port_status_t;


#define USB_HUBREQ_GET_STATUS		0
#define USB_HUBREQ_CLEAR_FEATURE	1
#define USB_HUBREQ_GET_STATE		2
#define USB_HUBREQ_SET_FEATURE		3
#define USB_HUBREQ_GET_DESCRIPTOR	6
#define USB_HUBREQ_SET_DESCRIPTOR	7

#define USB_HUB_FEATURE_C_LOCAL_POWER	0
#define USB_HUB_FEATURE_C_OVER_CURRENT	1

#define USB_PORT_FEATURE_CONNECTION		0
#define USB_PORT_FEATURE_ENABLE			1
#define USB_PORT_FEATURE_SUSPEND		2
#define USB_PORT_FEATURE_OVER_CURRENT		3
#define USB_PORT_FEATURE_RESET			4
#define USB_PORT_FEATURE_POWER			8
#define USB_PORT_FEATURE_LOW_SPEED		9
#define USB_PORT_FEATURE_C_PORT_CONNECTION	16
#define USB_PORT_FEATURE_C_PORT_ENABLE		17
#define USB_PORT_FEATURE_C_PORT_SUSPEND		18
#define USB_PORT_FEATURE_C_PORT_OVER_CURRENT	19
#define USB_PORT_FEATURE_C_PORT_RESET		20


#define GETUSBFIELD(s,f) (((s)->f##Low) | ((s)->f##High << 8))
#define PUTUSBFIELD(s,f,v) (s)->f##Low = (v & 0xFF); \
                           (s)->f##High = ((v)>>8 & 0xFF)

typedef struct usb_device_request_s {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint8_t wValueLow,wValueHigh;
    uint8_t wIndexLow,wIndexHigh;
    uint8_t wLengthLow,wLengthHigh;
} usb_device_request_t;

/*
 * Values for the bmAttributes field of a request
 */
#define USBREQ_DIR_IN	0x80
#define USBREQ_DIR_OUT 	0x00
#define USBREQ_TYPE_STD 	0x00
#define USBREQ_TYPE_CLASS 	0x20
#define USBREQ_TYPE_VENDOR	0x40
#define USBREQ_TYPE_RSVD 	0x60
#define USBREQ_REC_DEVICE 	0x00
#define USBREQ_REC_INTERFACE 0x01
#define USBREQ_REC_ENDPOINT 0x02
#define USBREQ_REC_OTHER 	0x03

#define REQCODE(req,dir,type,rec) (((req) << 8) | (dir) | (type) | (rec))
#define REQSW(req,attr) (((req) << 8) | (attr))

/*  *********************************************************************
    *  HID stuff
    ********************************************************************* */

typedef struct usb_hid_descr_s {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bcdHIDLow,bcdHIDHigh;
    uint8_t bCountryCode;
    uint8_t bNumDescriptors;
    uint8_t bClassDescrType;
    uint8_t wClassDescrLengthLow,wClassDescrLengthHigh;
} usb_hid_descr_t;

#endif   /* _USBCHAP9_H_ */
