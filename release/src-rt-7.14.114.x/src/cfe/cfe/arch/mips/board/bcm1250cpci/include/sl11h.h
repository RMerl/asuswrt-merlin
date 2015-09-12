/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  BCM1250CPCI USB Driver					File: sl11h.h
    *  
    *  USB Include definitions
    *  
    *  Author:  Paul Burrell 
	*  Parts of code extracted from Scanlogic USB demo driver
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



unsigned char SL11Read(unsigned char address);
void SL11Write(unsigned char address, unsigned char data);
void SL11BufRead(short address,unsigned char *buffer, short byte_count);
void SL11BufWrite(short address, unsigned char *buffer, short byte_count);
int SL11HMemTest(void);
void USBReset(void);
short uFindUsbDev(unsigned short UsbProd);
short uCloseUsbDev(void);

typedef struct
{
	unsigned char 	bmRequest,bRequest;
   	unsigned short 	wValue,wIndex,wLength;
} SetupPKG, *PSetupPKG;

typedef struct
{   unsigned char 	bLength;
    unsigned char 	bDescriptorType;
    unsigned short 	bcdUSB;
    unsigned char 	bDeviceClass;
    unsigned char 	bDeviceSubClass;
    unsigned char 	bDeviceProtocol;
    unsigned char 	bMaxPacketSize0;
    unsigned short 	idVendor;
    unsigned short 	idProduct;
    unsigned short 	bcdDevice;
    unsigned char 	iManufacturer;
    unsigned char 	iProduct;
    unsigned char 	iSerialNumber;
    unsigned char 	bNumConfigurations;
} sDevDesc, *pDevDesc;

typedef struct
{	unsigned char 	bLength;
	unsigned char 	bType;
	unsigned short 	wLength;
	unsigned char 	bNumIntf;
	unsigned char 	bCV;
	unsigned char 	bIndex;
	unsigned char 	bAttr;
	unsigned char 	bMaxPower;
} sConfDesc, *pConfDesc;

typedef struct
{	unsigned char 	bLength;
	unsigned char 	bType;
	unsigned char 	iNum;
	unsigned char 	iAltString;
	unsigned char 	bEndPoints;
	unsigned char 	iClass;
	unsigned char 	iSub;
	unsigned char 	iProto;
	unsigned char 	iIndex;
} sIntfDesc, *pIntfDesc;

typedef struct
{	unsigned char 	bLength;
	unsigned char 	bType;
	unsigned char 	bEPAdd;
	unsigned char 	bAttr;
	unsigned short 	wPayLoad;
	unsigned char 	bInterval;
} sEPDesc, *pEPDesc;

#define SL11_ADDR       USBCTL_PHYS		// Address from bcm1250cpci.h
#define PC_ADDR         USBCTL_PHYS		// Address from bcm1250cpci.h
#define	A0_OFFSET		8
#define outportb        _outp
#define inportb         _inp

/*-------------------------------------------------------------------------
 * EP0 use for configuration and Vendor Specific command interface
 *-------------------------------------------------------------------------
 */
#define EP0Buf          0x40   /* SL11 memory start at 0x40 */
#define EP0Len          0x40   /* Length of config buffer EP0Buf */
#define EP1Buf          0x60
#define EP1Len          0x40

/*-------------------------------------------------------------------------
 * SL11 memory from 80h-ffh use as ping-pong buffer for endpoint1-endpoint3
 * These buffers are share for endpoint 1-endpoint 3.
 * For DMA: endpoint3 will be used
 *-------------------------------------------------------------------------
 */
#define uBufA           0x80   /* buffer A address for DATA0 */
#define uBufB           0xc0   /* buffer B address for DATA1 */
#define uXferLen        0x40   /* xfer length */
#define sMemSize        0xc0   /* Total SL11 memory size */

/*-------------------------------------------------------------------------
 * SL11 Register Control memory map
 *-------------------------------------------------------------------------
 */
#define CtrlReg         0x05
#define IntStatus       0x0d
#define USBAdd          0x07
#define IntEna          0x06
#define DATASet         0x0e
#define cSOFcnt			0x0f
#define cDATASet		0x0e

#define sDMACntLow      0x35
#define sDMACntHigh     0x36

#define IntMask         0x57   /* Reset|DMA|EP0|EP2|EP1 for IntEna */
#define HostMask        0x47   /* Host request command  for IntStatus */
#define ReadMask        0xd7   /* Read mask interrupt   for IntStatus */

#define EP0Control      0x00
#define EP0Address      0x01
#define EP0XferLen      0x02
#define EP0Status       0x03
#define EP0Counter      0x04

#define EP1AControl     0x10
#define EP1AAddress     0x11
#define EP1AXferLen     0x12
#define EP1ACounter     0x14

#define EP1BControl     0x18
#define EP1BAddress     0x19
#define EP1BXferLen     0x1a
#define EP1BCounter     0x1c

#define EP2AControl     0x20
#define EP2AAddress     0x21
#define EP2AXferLen     0x22
#define EP2ACounter     0x24

#define EP2BControl     0x28
#define EP2BAddress     0x29
#define EP2BXferLen     0x2a
#define EP2BCounter     0x2c

#define EP3AControl     0x30
#define EP3AAddress     0x31
#define EP3AXferLen     0x32
#define EP3ACounter     0x34

#define EP3BControl     0x38
#define EP3BAddress     0x39
#define EP3BXferLen     0x3a
#define EP3BCounter     0x3c

/*-------------------------------------------------------------------------
 * Standard Chapter 9 definition
 *-------------------------------------------------------------------------
 */
#define GET_STATUS      0x00
#define CLEAR_FEATURE   0x01
#define SET_FEATURE     0x03
#define SET_ADDRESS     0x05
#define GET_DESCRIPTOR  0x06
#define SET_DESCRIPTOR  0x07
#define GET_CONFIG      0x08
#define SET_CONFIG      0x09
#define GET_INTERFACE   0x0a
#define SET_INTERFACE   0x0b

#define DEVICE          0x01
#define CONFIGURATION   0x02
#define STRING          0x03
#define INTERFACE       0x04
#define ENDPOINT        0x05

/*-------------------------------------------------------------------------
 * SL11H definition
 *-------------------------------------------------------------------------
 */
#define DATA0_WR    0x07
#define DATA1_WR    0x47
#define ZDATA0_WR   0x05
#define ZDATA1_WR   0x45
#define DATA0_RD    0x03
#define DATA1_RD    0x43

#define PID_SOF     0xA5
#define PID_SETUP   0x2d
#define PID_IN      0x69
#define PID_OUT     0xe1
#define MAX_RETRY   0x3FFF
//#define TIMEOUT     6000L    /* 5 seconds */
#define TIMEOUT     100L 
